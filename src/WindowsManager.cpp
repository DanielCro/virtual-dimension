/*
 * Virtual Dimension -  a free, fast, and feature-full virtual desktop manager
 * for the Microsoft Windows platform.
 * Copyright (C) 2003-2008 Francois Ferrand
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include "StdAfx.h"
#include <assert.h>
#include <algorithm>
#include "WindowsManager.h"
#include "VirtualDimension.h"
#include "movewindow.h"
#include "DesktopManager.h"
#include "Locale.h"
#include "BalloonNotif.h"
#include "Messages.h"
#include "PlatformHelper.h"
#include <shlwapi.h>

/** Delay between a notification about a window and the check of its state. */
#define WINDOW_CHECK_DELAY   150

WindowsManager * winMan;

/** Classes of top-level windows that belong to the shell, and which must never be managed. */
static const wchar_t * const s_shellWindowClasses[] =
{
   L"Progman",
   L"WorkerW",
   L"Shell_TrayWnd",
   L"Shell_SecondaryTrayWnd",
   L"Windows.UI.Core.CoreWindow",
   L"ApplicationManager_ImmersiveShellWindow",
   L"MultitaskingViewFrame",
   L"ForegroundStaging",
   L"XamlExplorerHostIslandWindow",
   L"TopLevelWindowForOverflowXamlIsland",
   L"NotifyIconOverflowWindow",
   L"TaskListThumbnailWnd",
   L"Shell_InputSwitchTopLevelWindow",
   L"EdgeUiInputTopWndClass",
   L"EdgeUiInputWndClass",
};

WindowsManager::WindowsManager(): m_shellhook(vdWindow), m_layoutChanged(false), m_checkTimerSet(false),
                                  m_firstFreeDelayedUpdateWndIdx(-1)
{
   Settings settings;

   m_confirmKill = settings.LoadSetting(Settings::ConfirmKilling);
   m_autoSwitch = settings.LoadSetting(Settings::AutoSwitchDesktop);
   m_allWindowsInTaskList = settings.LoadSetting(Settings::AllWindowsInTaskList);

   m_nbDisabledAnimations = 0;
   {
      ANIMATIONINFO info;
      info.cbSize = sizeof(ANIMATIONINFO);
      info.iMinAnimate = 0;

      SystemParametersInfo(SPI_GETANIMATION, sizeof(ANIMATIONINFO), &info, 0);

      m_iAnimate = info.iMinAnimate;
   }

   m_checkTimer = vdWindow.CreateTimer(this, &WindowsManager::OnCheckTimer);

   vdWindow.SetMessageHandler(ShellHook::GetMessageId(), this, &WindowsManager::OnShellHookMessage);
   vdWindow.SetMessageHandler(WM_VD_STARTONDESKTOP, this, &WindowsManager::OnStartOnDesktop);
   vdWindow.SetMessageHandler(WM_COPYDATA, this, &WindowsManager::OnCopyData);
}

WindowsManager::~WindowsManager(void)
{
   TRACE(L"~WindowsManager - started");

   Settings settings;

   RemoveWinEventHooks();

   vdWindow.UnSetMessageHandler(ShellHook::GetMessageId());
   vdWindow.DestroyTimer(m_checkTimer);

   for(Iterator it = GetIterator(); it; it++)
   {
      Window* win = it;
      if (win->IsInTray())
         trayManager->DelIcon(win);
      win->ShowWindow();
   }
   m_HWNDMap.clear();
   m_zorder.clear();
   m_windows.clear();

   settings.SaveSetting(Settings::ConfirmKilling, m_confirmKill);
   settings.SaveSetting(Settings::AutoSwitchDesktop, m_autoSwitch);
   settings.SaveSetting(Settings::AllWindowsInTaskList, m_allWindowsInTaskList);

   //Restore the animations
   {
      ANIMATIONINFO info;
      info.cbSize = sizeof(ANIMATIONINFO);
      info.iMinAnimate = m_iAnimate;

      SystemParametersInfo(SPI_SETANIMATION, sizeof(ANIMATIONINFO), &info, 0);
   }

   TRACE(L"~WindowsManager - ended");
}

void WindowsManager::PopulateInitialWindowsSet()
{
   std::vector<HWND> windows;
   Desktop * desk;

   //EnumWindows lists the windows from top to bottom of the z-order: add them in
   //reverse order, so that the top-most window is the last one of the z-order list.
   EnumWindows(ListWindowsProc, (LPARAM)&windows);
   for(std::vector<HWND>::reverse_iterator it = windows.rbegin(); it != windows.rend(); it++)
      AddWindow(*it);

   desk = deskMan->GetCurrentDesktop();
   if (desk)
      desk->UpdateLayout();

   //From now on, track the changes
   InstallWinEventHooks();
}

BOOL CALLBACK WindowsManager::ListWindowsProc( HWND hWnd, LPARAM lParam )
{
   std::vector<HWND> * windows = (std::vector<HWND> *)lParam;

   //Windows tagged by a previous instance (which crashed) may be hidden: take them anyway, to restore them
   if (IsCandidateWindow(hWnd) || Window::HasTag(hWnd))
      windows->push_back(hWnd);

   return TRUE;
}

bool WindowsManager::IsCandidateWindow(HWND hWnd)
{
   DWORD pId = 0;
   LONG_PTR style, exStyle;
   wchar_t className[64];
   bool appWindow;

   if (!IsWindow(hWnd) || !IsWindowVisible(hWnd))
      return false;

   //Ignore our own windows
   GetWindowThreadProcessId(hWnd, &pId);
   if (pId == GetCurrentProcessId())
      return false;

   style = GetWindowLongPtr(hWnd, GWL_STYLE);
   exStyle = GetWindowLongPtr(hWnd, GWL_EXSTYLE);
   if (style & WS_CHILD)
      return false;

   //Same rules as the taskbar: tool windows, owned windows and windows which cannot be
   //activated are ignored, unless they explicitly ask to appear in the taskbar.
   appWindow = (exStyle & WS_EX_APPWINDOW) != 0;
   if (!appWindow &&
       ((exStyle & (WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE)) || ::GetWindow(hWnd, GW_OWNER) != NULL))
      return false;

   //Ignore the windows which are not actually displayed: closed/suspended Store
   //applications, windows on another virtual desktop of Windows, shell flyouts...
   if (PlatformHelper::IsWindowCloaked(hWnd))
      return false;

   //Ignore the shell
   if (hWnd == GetShellWindow())
      return false;
   if (GetClassNameW(hWnd, className, sizeof(className)/sizeof(*className)))
   {
      for(const wchar_t * cls: s_shellWindowClasses)
         if (wcscmp(className, cls) == 0)
            return false;
   }

   //Ignore invisible (empty) windows
   if (!::IsIconic(hWnd))
   {
      RECT rect;
      if (!GetWindowRect(hWnd, &rect) || IsRectEmpty(&rect))
         return false;
   }

   //Windows of elevated programs cannot be changed (UIPI), unless we are elevated as well
   if (!PlatformHelper::CanManageWindow(hWnd))
      return false;

   return true;
}

Window * WindowsManager::AddWindow(HWND hWnd)
{
   Window * window = GetWindow(hWnd);
   WindowsList::Node * node;

   if (window)
      return window;

#ifdef DEBUG
   {
      wchar_t className[64], title[64];
      GetClassNameW(hWnd, className, 64);
      GetWindowTextW(hWnd, title, 64);
      TRACE(L"Managing window %p [%s] \"%s\"", hWnd, className, title);
   }
#endif

   //Update the list
   node = new WindowsList::Node(hWnd);
   m_windows.push_back(node);
   m_HWNDMap[hWnd] = node;

   window = *node;
   m_zorder.push_back(window);

   //The window may belong to a program started on some desktop from the command line
   ApplyStartRequests(window);

   //Add the tooltip (let the desktop do it)
   if (window->IsOnDesk(NULL))  //on all desktops
      deskMan->UpdateLayout();
   else
      window->GetDesk()->UpdateLayout();

   vdWindow.Refresh();

   return window;
}

void WindowsManager::RemoveWindow(Window * win)
{
   HWNDMapIterator it = m_HWNDMap.find(*win);
   Desktop * desk;

   if (it == m_HWNDMap.end())
      return;

   TRACE(L"Releasing window %p", (HWND)*win);

   //Update the list
   WindowsList::Iterator nIt(&m_windows, (*it).second);
   m_zorder.remove(win);
   m_HWNDMap.erase(it);
   m_pendingChecks.erase(*win);

   //Remove tooltip(s)
   tooltip->UnsetTool(win);
   desk = win->IsOnDesk(NULL) ? NULL : win->GetDesk();

   //Delete the object
   if (win->IsInTray())
      trayManager->DelIcon(win);
   nIt.Erase();

   //Update layout
   if (desk)
      desk->UpdateLayout();
   else
      deskMan->UpdateLayout();

   //Refresh display
   vdWindow.Refresh();
}

void WindowsManager::MoveWindow(HWND hWnd, Desktop* desk)
{
   Window * win = GetWindow(hWnd);
   if (win)
      win->MoveToDesktop(desk);
}

Window* WindowsManager::GetWindow(HWND hWnd)
{
   HWNDMapIterator it = m_HWNDMap.find(hWnd);

   if (it == m_HWNDMap.end())
      return NULL;
   else
      return *((*it).second);
}

Window * WindowsManager::FindWindowOrOwner(HWND hWnd)
{
   Window * win = GetWindow(hWnd);

   //Dialogs and other owned windows: look for the managed owner
   while(!win && hWnd)
   {
      hWnd = ::GetWindow(hWnd, GW_OWNER);
      if (hWnd)
         win = GetWindow(hWnd);
   }

   return win;
}

// Window events
//**************************************************************************

void WindowsManager::InstallWinEventHooks()
{
   static const DWORD ranges[][2] =
   {
      { EVENT_SYSTEM_FOREGROUND,      EVENT_SYSTEM_FOREGROUND },
      { EVENT_SYSTEM_MOVESIZESTART,   EVENT_SYSTEM_MOVESIZEEND },
      { EVENT_SYSTEM_MINIMIZESTART,   EVENT_SYSTEM_MINIMIZEEND },
      { EVENT_OBJECT_DESTROY,         EVENT_OBJECT_HIDE },
      { EVENT_OBJECT_NAMECHANGE,      EVENT_OBJECT_NAMECHANGE },
      { EVENT_OBJECT_CLOAKED,         EVENT_OBJECT_UNCLOAKED },
   };

   if (!m_winEventHooks.empty())
      return;

   //Out-of-context hooks: no code is injected in the other processes, and the events
   //are delivered asynchronously, through the message loop of this thread.
   for(const DWORD * range: ranges)
   {
      HWINEVENTHOOK hook = SetWinEventHook(range[0], range[1], NULL, WinEventProc, 0, 0,
                                           WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
      if (hook)
         m_winEventHooks.push_back(hook);
   }
}

void WindowsManager::RemoveWinEventHooks()
{
   for(HWINEVENTHOOK hook: m_winEventHooks)
      UnhookWinEvent(hook);
   m_winEventHooks.clear();
}

void CALLBACK WindowsManager::WinEventProc(HWINEVENTHOOK /*hWinEventHook*/, DWORD event, HWND hWnd,
                                           LONG idObject, LONG idChild, DWORD /*idEventThread*/, DWORD /*dwmsEventTime*/)
{
   //Only interested in the windows themselves (not in their content)
   if (hWnd == NULL || idObject != OBJID_WINDOW || idChild != CHILDID_SELF || winMan == NULL)
      return;

   winMan->OnWinEvent(event, hWnd);
}

void WindowsManager::OnWinEvent(DWORD event, HWND hWnd)
{
   Window * win;

   switch(event)
   {
   case EVENT_SYSTEM_FOREGROUND:
      OnWindowActivated(hWnd);
      break;

   case EVENT_SYSTEM_MOVESIZESTART:
   case EVENT_SYSTEM_MOVESIZEEND:
      win = GetWindow(hWnd);
      if (win)
         win->SetMoving(event == EVENT_SYSTEM_MOVESIZESTART);
      break;

   case EVENT_SYSTEM_MINIMIZESTART:
      win = GetWindow(hWnd);
      if (win && !win->IsSwitching())
         win->OnMinimized();
      break;

   case EVENT_SYSTEM_MINIMIZEEND:
      vdWindow.Refresh();
      break;

   case EVENT_OBJECT_NAMECHANGE:
      if (GetWindow(hWnd))
         ScheduleLayoutUpdate();
      break;

   case EVENT_OBJECT_DESTROY:
      //The window is already gone: only managed windows are interesting
      if (GetWindow(hWnd))
         ScheduleCheck(hWnd);
      break;

   case EVENT_OBJECT_SHOW:
   case EVENT_OBJECT_UNCLOAKED:
      //Only top-level windows can be managed
      if (GetWindow(hWnd) || GetAncestor(hWnd, GA_PARENT) == GetDesktopWindow())
         ScheduleCheck(hWnd);
      break;

   case EVENT_OBJECT_HIDE:
   case EVENT_OBJECT_CLOAKED:
      if (GetWindow(hWnd))
         ScheduleCheck(hWnd);
      break;
   }
}

void WindowsManager::ScheduleCheck(HWND hWnd)
{
   m_pendingChecks.insert(hWnd);
   StartCheckTimer();
}

void WindowsManager::ScheduleLayoutUpdate()
{
   m_layoutChanged = true;
   StartCheckTimer();
}

void WindowsManager::StartCheckTimer()
{
   if (!m_checkTimerSet)
   {
      vdWindow.SetTimer(m_checkTimer, WINDOW_CHECK_DELAY);
      m_checkTimerSet = true;
   }
}

LRESULT WindowsManager::OnCheckTimer(HWND /*hWnd*/, UINT /*message*/, WPARAM /*wParam*/, LPARAM /*lParam*/)
{
   std::set<HWND> windows;

   vdWindow.KillTimer(m_checkTimer);
   m_checkTimerSet = false;

   //Checking a window may schedule new checks
   windows.swap(m_pendingChecks);
   for(HWND hWnd: windows)
      CheckWindow(hWnd);

   if (m_layoutChanged)
   {
      m_layoutChanged = false;
      deskMan->UpdateLayout();
   }

   return 0;
}

void WindowsManager::CheckWindow(HWND hWnd)
{
   Window * win = GetWindow(hWnd);

   if (!IsWindow(hWnd))
   {
      //The window has been destroyed
      if (win)
         RemoveWindow(win);
      return;
   }

   if (!win)
   {
      //New window ?
      if (IsCandidateWindow(hWnd))
         AddWindow(hWnd);
      return;
   }

   //Virtual Dimension is changing the window: its state is not stable yet
   if (win->IsSwitching())
   {
      ScheduleCheck(hWnd);
      return;
   }

   if (win->IsHidden())
   {
      //A window hidden by Virtual Dimension has been displayed by the application
      if (!win->GetHidingMethod()->KeepsWindowVisible() &&
          IsWindowVisible(hWnd) && !PlatformHelper::IsWindowCloaked(hWnd))
      {
         TRACE(L"Window %p shown by its application", hWnd);
         win->OnShownExternally();
         vdWindow.Refresh();
      }
   }
   else if (!IsCandidateWindow(hWnd))
   {
      //The window has been hidden, closed or cloaked by its application (this is how
      //Store applications are "closed", for instance): do not manage it anymore.
      TRACE(L"Window %p hidden/cloaked by its application", hWnd);
      RemoveWindow(win);
   }
}

LRESULT WindowsManager::OnShellHookMessage(HWND /*hWnd*/, UINT /*message*/, WPARAM wParam, LPARAM lParam)
{
   switch(wParam)
   {
      case ShellHook::WINDOWCREATED:
      case ShellHook::WINDOWDESTROYED:
      case ShellHook::WINDOWREPLACED:
         ScheduleCheck((HWND)lParam);
         break;

      case ShellHook::REDRAW:
         OnRedraw((HWND)lParam);
         break;

      case ShellHook::FLASH:
         OnWindowFlash((HWND)lParam);
         break;
   }

   return 0;
}

void WindowsManager::OnWindowActivated(HWND hWnd)
{
   Window * win;

   win = FindWindowOrOwner(hWnd);
   if (win == NULL)
   {
      //Maybe a new window: the list will be updated if needed
      ScheduleCheck(hWnd);
      return;
   }

   //Ignore iconic windows
   if (IsIconic(*win))
      return;

   m_zorder.remove(win);
   m_zorder.push_back(win);

   if (win->IsSwitching())
      return;  //Ignore switching windows (the activation is a side effect of Virtual Dimension's changes)

   //Try to see if some window that should not be on this desktop has
   //been activated. If so, move it to the current desktop
   if (!win->IsOnCurrentDesk())
   {
      if (m_autoSwitch)
         //Auto switch desktop
         deskMan->SwitchToDesktop(win->GetDesk());
      else
         //Auto move window
         win->MoveToDesktop(deskMan->GetCurrentDesktop());
   }
}

void WindowsManager::OnRedraw(HWND hWnd)
{
   Window * win = GetWindow(hWnd);

   //The title or the icon of the window has changed
   if (win)
   {
      win->InvalidateIcon();
      vdWindow.Refresh();
   }
}

void WindowsManager::OnWindowFlash(HWND hWnd)
{
   Window * win = GetWindow(hWnd);
   if (win)
      win->FlashWindow();
}

bool WindowsManager::ConfirmKillWindow()
{
   return (!m_confirmKill) ||
          (locMessageBox(vdWindow, IDS_CONFIRMKILL, IDS_KILLWARNING, MB_OKCANCEL|MB_ICONWARNING) == IDOK);
}

LRESULT WindowsManager::OnSettingsChange(HWND /*hWnd*/, UINT /*message*/, WPARAM wParam, LPARAM /*lParam*/)
{
   if (wParam == SPI_SETANIMATION)
   {
      ANIMATIONINFO info;
      info.cbSize = sizeof(ANIMATIONINFO);
      info.iMinAnimate = 0;

      SystemParametersInfo(SPI_GETANIMATION, sizeof(ANIMATIONINFO), &info, 0);

      //Ignore the changes made by ourselves
      if (m_nbDisabledAnimations == 0)
         m_iAnimate = info.iMinAnimate;
   }

   return 0;
}

void WindowsManager::DisableAnimations()
{
   //Increment number of time animation has been disabled
   if ((InterlockedIncrement(&m_nbDisabledAnimations) == 1) &&
       (m_iAnimate != 0))
   {
      //If this is the first time, disable animations
      ANIMATIONINFO info;
      info.cbSize = sizeof(ANIMATIONINFO);
      info.iMinAnimate = 0;

      SystemParametersInfo(SPI_SETANIMATION, sizeof(ANIMATIONINFO), &info, 0);
   }
}

void WindowsManager::EnableAnimations()
{
   //Decrement number of time animation has been disabled
   if ((InterlockedDecrement(&m_nbDisabledAnimations) == 0) &&
       (m_iAnimate != 0))
   {
      //If this is the last time (ie, nobody else wants the animations to be disabled), reenable them
      ANIMATIONINFO info;
      info.cbSize = sizeof(ANIMATIONINFO);
      info.iMinAnimate = m_iAnimate;

      SystemParametersInfo(SPI_SETANIMATION, sizeof(ANIMATIONINFO), &info, 0);
   }
}

void WindowsManager::EmergencyRestore()
{
   for(Iterator it = GetIterator(); it; it++)
   {
      Window * win = it;
      if (win->IsHidden())
         win->ShowWindow();
   }

   if (m_iAnimate != 0)
   {
      ANIMATIONINFO info;
      info.cbSize = sizeof(ANIMATIONINFO);
      info.iMinAnimate = m_iAnimate;
      SystemParametersInfo(SPI_SETANIMATION, sizeof(ANIMATIONINFO), &info, 0);
   }
}

Window * WindowsManager::GetForegroundWindow()
{
   return FindWindowOrOwner(::GetForegroundWindow());
}

Window * WindowsManager::GetTopWindow(Desktop * desk)
{
   for(list<Window*>::reverse_iterator it = m_zorder.rbegin(); it != m_zorder.rend(); it++)
   {
      Window * win = *it;
      if (win->IsOnDesk(desk) && !win->IsIconic() && win->CheckExists())
         return win;
   }
   return NULL;
}

HWND WindowsManager::GetPrevWindow(Window * wnd)
{
   ZOrderIterator it = find(m_zorder.begin(), m_zorder.end(), wnd);

   if (it != m_zorder.end())
   {
      it++;

      while(it != m_zorder.end() && !(*it)->IsOnAllDesktops() && (*it)->GetDesk()!=wnd->GetDesk())
         it++;
   }

   if (it != m_zorder.end())
      return *(*it);
   else
      return ::GetWindow(*wnd, GW_HWNDPREV);
}

LRESULT WindowsManager::OnStartOnDesktop(HWND /*hWnd*/, UINT /*message*/, WPARAM wParam, LPARAM lParam)
{
   AddStartRequest((DWORD)wParam, (int)lParam, NULL);
   return 0;
}

LRESULT WindowsManager::OnCopyData(HWND /*hWnd*/, UINT /*message*/, WPARAM /*wParam*/, LPARAM lParam)
{
   COPYDATASTRUCT * data = (COPYDATASTRUCT *)lParam;

   if (data->dwData == VD_COPYDATA_STARTONDESKTOP && data->cbData == sizeof(StartOnDesktopRequest))
   {
      StartOnDesktopRequest request = *(StartOnDesktopRequest *)data->lpData;
      request.program[MAX_PATH-1] = 0;
      AddStartRequest(request.processId, request.desktop, request.program);
      return TRUE;
   }

   return FALSE;
}

/** How long the windows of a program started on some desktop are waited for. */
#define START_REQUEST_DELAY   15000

void WindowsManager::AddStartRequest(DWORD processId, int desktop, LPCWSTR program)
{
   Desktop * desk = deskMan->GetDesktop(desktop);
   StartRequest request;

   if (!desk)
      return;

   //The windows which already exist
   for(Iterator it = GetIterator(); it; it++)
   {
      DWORD process;
      Window * win = it;
      GetWindowThreadProcessId(*win, &process);
      if (process == processId)
         win->MoveToDesktop(desk);
   }

   //The windows which will be created soon
   request.processId = processId;
   request.desktop = desktop;
   request.program = program ? program : L"";
   request.deadline = GetTickCount64() + START_REQUEST_DELAY;
   m_startRequests.push_back(request);
}

void WindowsManager::ApplyStartRequests(Window * win)
{
   ULONGLONG now = GetTickCount64();
   DWORD processId = 0;
   wchar_t path[MAX_PATH];
   LPCWSTR program = NULL;

   //Forget about the old requests
   m_startRequests.erase(std::remove_if(m_startRequests.begin(), m_startRequests.end(),
                                        [now](const StartRequest& r) { return r.deadline < now; }),
                         m_startRequests.end());
   if (m_startRequests.empty())
      return;

   GetWindowThreadProcessId(*win, &processId);
   if (PlatformHelper::GetWindowFileName(*win, path, MAX_PATH))
      program = PathFindFileNameW(path);

   //Match the process, or the program (which may run in another process than the one which was started)
   for(const StartRequest& request: m_startRequests)
   {
      if (request.processId == processId ||
          (program && !request.program.empty() && _wcsicmp(request.program.c_str(), program) == 0))
      {
         Desktop * desk = deskMan->GetDesktop(request.desktop);
         TRACE(L"Window %p started on desktop %d", (HWND)*win, request.desktop);
         if (desk)
            win->MoveToDesktop(desk);
         break;
      }
   }
}

// Delayed window update
//**************************************************************************

void WindowsManager::ScheduleDelayedUpdate(Window * win)
{
   int idx = AddDelayedUpdateWnd(win);
   ::SetTimer(vdWindow, FIRST_WINDOW_MANAGER_TIMER+idx, DELAYED_UPDATE_DELAY, OnDelayedUpdateTimer);
}

void WindowsManager::CancelDelayedUpdate(Window * win)
{
   DelayedUdateWndIterator it = find(m_delayedUpdateWndTab.begin(), m_delayedUpdateWndTab.end(), win);

   if (it != m_delayedUpdateWndTab.end())
   {
      int idx = (int)distance(m_delayedUpdateWndTab.begin(), it);
      RemoveDelayedUpdateWnd(idx);
   }
}

void CALLBACK WindowsManager::OnDelayedUpdateTimer(HWND /*hwnd*/, UINT /*uMsg*/, UINT_PTR idEvent, DWORD /*dwTime*/)
{
   unsigned int idx = (unsigned int)(idEvent - FIRST_WINDOW_MANAGER_TIMER);

   if (idx >= winMan->m_delayedUpdateWndTab.size() || winMan->m_delayedUpdateWndTab[idx] == NULL)
   {
      ::KillTimer(vdWindow, idEvent);
      return;
   }

   TRACE(L"DelayedUpdateTimer");
   Window * win = winMan->m_delayedUpdateWndTab[idx];
   winMan->RemoveDelayedUpdateWnd(idx);
   win->OnDelayUpdate();
}

unsigned int WindowsManager::AddDelayedUpdateWnd(Window * wnd)
{
   unsigned int idx;

   //If we reached the end of the allocated space, but there are holes, try to fill them
   //(circular buffer style)
   if (m_firstFreeDelayedUpdateWndIdx != -1)
   {
      idx = m_firstFreeDelayedUpdateWndIdx;
      m_firstFreeDelayedUpdateWndIdx = m_delayedUpdateNextTab[idx];
      m_delayedUpdateWndTab[idx] = wnd;
   }
   else
   {
      idx = (unsigned int)m_delayedUpdateWndTab.size();
      m_delayedUpdateWndTab.push_back(wnd);
      m_delayedUpdateNextTab.push_back(-1);
   }

   return idx;
}

void WindowsManager::RemoveDelayedUpdateWnd(unsigned int idx)
{
   assert(idx < m_delayedUpdateWndTab.size());
   assert(m_delayedUpdateWndTab[idx] != NULL);

   //Stop the timer
   ::KillTimer(vdWindow, FIRST_WINDOW_MANAGER_TIMER+idx);

   //Remove the entry
   m_delayedUpdateWndTab[idx] = NULL;

   //Track the first entry
   m_delayedUpdateNextTab[idx] = m_firstFreeDelayedUpdateWndIdx;
   m_firstFreeDelayedUpdateWndIdx = idx;
}

void WindowsManager::MoveWindowToNextDesktopEventHandler::OnHotkey()
{
   Window * window = winMan->GetForegroundWindow();
   Desktop * desk;
   if ((window != NULL) &&
       (window->GetDesk() == deskMan->GetCurrentDesktop()) &&
       ((desk = deskMan->GetNextDesk(window->GetDesk())) != NULL))
      window->MoveToDesktop(desk);
}

void WindowsManager::MoveWindowToPrevDesktopEventHandler::OnHotkey()
{
   Window * window = winMan->GetForegroundWindow();
   Desktop * desk;
   if ((window != NULL) &&
       (window->GetDesk() == deskMan->GetCurrentDesktop()) &&
       ((desk = deskMan->GetPrevDesk(window->GetDesk())) != NULL))
      window->MoveToDesktop(desk);
}

void WindowsManager::MoveWindowToDesktopEventHandler::OnHotkey()
{
   Window * window = winMan->GetForegroundWindow();
   if ((window != NULL) && (window->IsOnCurrentDesk()))
   {
      SetForegroundWindow(vdWindow);
      SelectDesktopForWindow(window);
      if (window->IsOnCurrentDesk())
         SetForegroundWindow(window->GetOwnedWindow());
   }
}

void WindowsManager::MaximizeHeightEventHandler::OnHotkey()
{
   Window * window = winMan->GetForegroundWindow();
   if ((window != NULL) && (window->IsOnCurrentDesk()))
      window->MaximizeHeight();
}

void WindowsManager::MaximizeWidthEventHandler::OnHotkey()
{
   Window * window = winMan->GetForegroundWindow();
   if ((window != NULL) && (window->IsOnCurrentDesk()))
      window->MaximizeWidth();
}

void WindowsManager::ToggleAlwaysOnTopEventHandler::OnHotkey()
{
   Window * window = winMan->GetForegroundWindow();
   if ((window != NULL) && (window->IsOnCurrentDesk()))
      window->ToggleOnTop();
}

void WindowsManager::ToggleTransparencyEventHandler::OnHotkey()
{
   Window * window = winMan->GetForegroundWindow();
   if ((window != NULL) && (window->IsOnCurrentDesk()))
      window->ToggleTransparent();
}
