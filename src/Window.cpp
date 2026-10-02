/*
 * Virtual Dimension -  a free, fast, and feature-full virtual desktop manager
 * for the Microsoft Windows platform.
 * Copyright (C) 2003-2008 Francois Ferrand
 * Copyright (C) 2026 Daniel Filkovic (64-bit Windows 10/11 port)
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
#include "Window.h"
#include "VirtualDimension.h"
#include "movewindow.h"
#include "PlatformHelper.h"
#include "ExplorerWrapper.h"
#include "DesktopManager.h"
#include "WindowsManager.h"
#include "Messages.h"
#include "Locale.h"

/** Time during which a window is considered as being changed after a show/hide
 * operation: the notifications received during that time are caused by Virtual
 * Dimension itself.
 */
#define PENDING_OPERATION_DELAY  1000

/** Delay after which the icon of a window is retrieved again. */
#define ICON_REFRESH_DELAY       5000

HidingMethodHide       Window::s_hider_method;
HidingMethodMinimize   Window::s_minimizer_method;
HidingMethodMove       Window::s_mover_method;

HidingMethod* Window::s_hiding_methods[] =
{
   &s_hider_method,
   &s_minimizer_method,
   &s_mover_method
};

const wchar_t Window::s_VDPropertyTag[] = L"ViRtUaL DiMeNsIoN rocks !";


Window::Window(HWND hWnd): AlwaysOnTop(hWnd), m_hWnd(hWnd), m_hOwnedWnd(GetOwnedWindow(hWnd)),
                           m_MinToTray(false), m_iconic(false), m_transp(m_hOwnedWnd), m_transpLevel(128),
                           m_autoSaveSettings(false), m_autosize(false), m_autopos(false), m_autodesk(false),
                           m_hIcon(NULL), m_hOwnIcon(NULL), m_iconTime(0), m_BallonMsg(NULL), m_dwProcessId(0),
                           m_switching(false), m_moving(false), m_hidden(false),
                           m_responsive(true), m_lastOperation(0), m_hiddenIconic(false)
{
   Settings s;
   Settings::Window settings(&s);
   unsigned int method;
   bool recovering = HasTag(hWnd);

   *m_name = 0;
   m_hiddenPos.x = m_hiddenPos.y = 0;
   GetWindowThreadProcessId(m_hWnd, &m_dwProcessId);

   //Try to see if there are some special settings for this window
   GetClassNameW(m_hWnd, m_className, sizeof(m_className)/sizeof(*m_className));
   OpenSettings(settings, false);

   //Setup the hiding method to use (try to restore from tag, if present)
   if (!recovering)
   {
      wchar_t filename[MAX_PATH];
      PlatformHelper::GetWindowFileName(m_hWnd, filename, MAX_PATH);
      method = s.LoadHidingMethod(filename);
   }
   else
      method = GetTag(hWnd);
   if (method >= sizeof(s_hiding_methods)/sizeof(*s_hiding_methods))
      method = 0;
   m_hidingMethod = s_hiding_methods[method];

   //If the window was managed by a previous instance (which may have crashed), make sure it is visible
   if (recovering)
   {
      m_responsive = PlatformHelper::IsWindowResponsive(m_hWnd);
      m_hidingMethod->Recover(this);
   }

   //Tag the window, to remember the window was managed by VD (in case of crash). It also tracks the hidding method used
   SetTag(hWnd, method);

   //Load settings for this window
   m_desk = settings.LoadSetting(Settings::Window::OnAllDesktops) ? NULL : deskMan->GetCurrentDesktop();

   SetMinimizeToTray(settings.LoadSetting(Settings::Window::MinimizeToTray));

   //Only touch the always-on-top/transparency state if the user configured it for this window:
   //some applications legitimately make their windows topmost.
   if (settings.IsValid())
   {
      SetAlwaysOnTop(settings.LoadSetting(Settings::Window::AlwaysOnTop));

      SetTransparencyLevel(settings.LoadSetting(Settings::Window::TransparencyLevel));
      SetTransparent(settings.LoadSetting(Settings::Window::EnableTransparency));
   }

   m_autoSaveSettings = settings.LoadSetting(Settings::Window::AutoSaveSettings);
   m_autosize = settings.LoadSetting(Settings::Window::AutoSetSize);
   m_autopos = settings.LoadSetting(Settings::Window::AutoSetPos);
   m_autodesk = settings.LoadSetting(Settings::Window::AutoSetDesk);

   //Auto-switch desktop
   if (m_autodesk && !IsOnAllDesktops())
   {
      Desktop * desk = deskMan->GetDesktop(settings.LoadSetting(Settings::Window::DesktopIndex));
      if (desk)
         //Move the window to its associated desk
         MoveToDesktop(desk);
      else if (!m_autoSaveSettings)
         //Disable auto-move window to desktop (keep enabled if auto-saving settings)
         settings.SaveSetting(Settings::Window::AutoSetDesk, m_autodesk = false);
   }

   //Delay-update
   winMan->ScheduleDelayedUpdate(this);
}

Window::~Window(void)
{
   winMan->CancelDelayedUpdate(this);

   UnFlashWindow();

   if (m_autoSaveSettings)
      SaveSettings();

   //Never leave a window hidden behind us
   if (m_hidden && CheckExists())
      ShowWindow();

   if (m_hOwnIcon)
      DestroyIcon(m_hOwnIcon);

   //Tag is not needed anymore
   RemTag(m_hWnd);
}

/** Delay update callback.
 * This method is called as the final step in the delayed-update process. This process is used to
 * fix some issues when VD is called too early after a window has been created. Indeed, sometime
 * the owned window is not ready, thus many features would not work (always on top, transparency...).
 * This function is thus called some time after the constructor (provided a call to
 * WindowManager::ScheduleDelayedUpdate() is made). It checks if the owned window has changed,
 * updates various parameters consequently, and in any case performs auto-size/position.
 */
void Window::OnDelayUpdate()
{
   Settings s;
   Settings::Window settings(&s);
   RECT rect;
   HWND hOwnedWnd = GetOwnedWindow(m_hWnd);

   if (hOwnedWnd != m_hOwnedWnd)
   {
      m_hOwnedWnd = hOwnedWnd;
      m_transp.SetWindow(m_hOwnedWnd);
   }

   //Auto-size/position
   OpenSettings(settings, false);
   if ( (m_autosize || m_autopos) && settings.LoadSetting(Settings::Window::WindowPosition, &rect) )
      ::SetWindowPos(m_hOwnedWnd, 0,
                     rect.left, rect.top, rect.right-rect.left, rect.bottom-rect.top,
                     SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS |
                     (m_autopos?0:SWP_NOMOVE) | (m_autosize?0:SWP_NOSIZE));
}

void Window::MoveToDesktop(Desktop * desk)
{
   Desktop * oldDesk;

   if (desk == m_desk)
      return;

   oldDesk = m_desk;
   m_desk = desk;

   if (deskMan->GetCurrentDesktop() == desk)
      UnFlashWindow();

   if (IsOnDesk(deskMan->GetCurrentDesktop()))
   {
      if (IsInTray())
         trayManager->AddIcon(this);
      else
         ShowWindow();
   }
   else
   {
      if (IsInTray())
         trayManager->DelIcon(this);
      else
         HideWindow();
   }

   if (IsOnDesk(NULL))  //on all desktops
      deskMan->UpdateLayout();
   else
   {
      if (oldDesk != NULL)
      {
         oldDesk->UpdateLayout();
         m_desk->UpdateLayout();
      }
      else
         deskMan->UpdateLayout();
   }
}

bool Window::IsOnCurrentDesk() const
{
   return IsOnDesk(deskMan->GetCurrentDesktop());
}

void Window::ShowWindow()
{
   if (m_hidden)
   {
      BeginOperation();
      m_hidingMethod->Show(this);
      m_hidden = false;
   }
}

void Window::HideWindow()
{
   if (!m_hidden)
   {
      BeginOperation();
      m_hidingMethod->Hide(this);
      m_hidden = true;
   }
}

void Window::BeginOperation()
{
   //Hung applications are changed asynchronously, so that we do not hang as well
   m_responsive = PlatformHelper::IsWindowResponsive(m_hWnd);
   m_lastOperation = GetTickCount64();
}

bool Window::HasPendingOperation() const
{
   if (m_lastOperation == 0)
      return false;

   //Requests to an unresponsive window stay pending as long as it does not respond
   return (GetTickCount64() - m_lastOperation < PENDING_OPERATION_DELAY) ||
          (!m_responsive && IsHungAppWindow(m_hWnd));
}

void Window::SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int x, int y, int cx, int cy, UINT flags)
{
   if (!m_responsive)
      flags |= SWP_ASYNCWINDOWPOS;
   ::SetWindowPos(hWnd, hWndInsertAfter, x, y, cx, cy, flags);
}

void Window::ShowWindowCmd(HWND hWnd, int cmd)
{
   if (m_responsive)
      ::ShowWindow(hWnd, cmd);
   else
      ::ShowWindowAsync(hWnd, cmd);
}

struct OwnedPopupsEnumInfo
{
   HWND hOwner;
   std::vector<HWND> * popups;
};

static BOOL CALLBACK ListOwnedPopupsProc(HWND hWnd, LPARAM lParam)
{
   OwnedPopupsEnumInfo * info = (OwnedPopupsEnumInfo *)lParam;

   if ( (hWnd != info->hOwner) &&
        (IsWindowVisible(hWnd)) &&
        (GetAncestor(hWnd, GA_ROOTOWNER) == info->hOwner) )
      info->popups->push_back(hWnd);

   return TRUE;
}

void Window::HideOwnedPopups()
{
   OwnedPopupsEnumInfo info = { m_hWnd, &m_hiddenPopups };

   m_hiddenPopups.clear();
   EnumWindows(ListOwnedPopupsProc, (LPARAM)&info);

   for(HWND hWnd: m_hiddenPopups)
      SetWindowPos(hWnd, NULL, 0, 0, 0, 0, SWP_HIDEWINDOW | SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void Window::ShowOwnedPopups()
{
   for(HWND hWnd: m_hiddenPopups)
   {
      if (IsWindow(hWnd) && GetAncestor(hWnd, GA_ROOTOWNER) == m_hWnd)
         SetWindowPos(hWnd, NULL, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
   }
   m_hiddenPopups.clear();
}

void Window::OnMinimized()
{
   //Emulate "minimize to tray": move the minimized window from the taskbar to the tray
   if (!IsMinimizeToTray() || m_hidden || !IsOnCurrentDesk())
      return;

   m_iconic = true;
   trayManager->AddIcon(this);
   HideWindow();
}

void Window::OnShownExternally()
{
   Desktop * oldDesk = m_desk;

   if (!m_hidden)
      return;

   //The application displayed the window again (eg, it was restored from its own
   //notification icon): consider that the window now belongs to the current desktop.
   if (IsInTray())
      trayManager->DelIcon(this);
   m_iconic = false;
   m_hidden = false;
   m_hiddenPopups.clear();

   if (m_desk != NULL)
   {
      m_desk = deskMan->GetCurrentDesktop();
      if (oldDesk != NULL && oldDesk != m_desk)
         oldDesk->UpdateLayout();
      m_desk->UpdateLayout();
   }
}

HICON Window::GetIcon(void)
{
   ULONGLONG now = GetTickCount64();
   int size = vdWindow.GetIconSize();
   bool isStoreApp;
   HICON hIcon = NULL;
   DWORD_PTR res;

   if (m_hIcon && (now - m_iconTime < ICON_REFRESH_DELAY))
      return m_hIcon;
   m_iconTime = now;

   //Store applications are hosted in ApplicationFrameWindow, which has no icon of its own
   isStoreApp = (wcscmp(m_className, L"ApplicationFrameWindow") == 0);

   if (!isStoreApp)
   {
      //Get the icon from the window
      if (SendMessageTimeoutW(m_hWnd, WM_GETICON, size > 16 ? ICON_BIG : ICON_SMALL2, 0, SMTO_ABORTIFHUNG, 50, &res) && res)
         hIcon = (HICON)res;
      else if (SendMessageTimeoutW(m_hWnd, WM_GETICON, ICON_SMALL, 0, SMTO_ABORTIFHUNG, 50, &res) && res)
         hIcon = (HICON)res;
      else if (SendMessageTimeoutW(m_hWnd, WM_GETICON, ICON_BIG, 0, SMTO_ABORTIFHUNG, 50, &res) && res)
         hIcon = (HICON)res;

      //Get class icon
      if (!hIcon)
         hIcon = (HICON)GetClassLongPtrW(m_hWnd, GCLP_HICONSM);
      if (!hIcon)
         hIcon = (HICON)GetClassLongPtrW(m_hWnd, GCLP_HICON);
   }

   if (hIcon)
      return m_hIcon = hIcon;

   //No icon from the window: build one (only once)
   if (!m_hOwnIcon)
   {
      m_hOwnIcon = PlatformHelper::GetAppIconForWindow(m_hWnd, size);

      if (!m_hOwnIcon)
      {
         wchar_t fileName[MAX_PATH];
         if (PlatformHelper::GetWindowFileName(m_hWnd, fileName, MAX_PATH))
            ExtractIconExW(fileName, 0, NULL, &m_hOwnIcon, 1);
      }
   }

   if (m_hOwnIcon)
      return m_hIcon = m_hOwnIcon;

   //Use generic default icon
   return m_hIcon = (HICON)LoadImage(vdWindow, MAKEINTRESOURCE(IDI_DEFAPP_SMALL), IMAGE_ICON, 16, 16, LR_SHARED);
}

void Window::InsertMenuItem(HMENU menu, bool checked, HANDLE bmp, UINT id, UINT uIdStr)
{
   MENUITEMINFO mii;

   mii.cbSize = sizeof(MENUITEMINFO);
   mii.fMask = MIIM_DATA | MIIM_BITMAP | MIIM_ID | MIIM_STRING | MIIM_STATE;
   mii.hbmpItem = (INT_PTR)bmp <= 11 ? (HBITMAP)bmp : HBMMENU_CALLBACK;   //HBMMENU_xxx values are 1 to 11
   mii.dwItemData = (ULONG_PTR)bmp;
   mii.wID = id;
   locGetString(mii.dwTypeData, uIdStr);
   mii.fState = checked ? MFS_CHECKED : MFS_UNCHECKED;
   ::InsertMenuItem(menu, (UINT)-1, TRUE, &mii);
}

HANDLE Window::LoadBmpRes(int id)
{
   return LoadImage(vdWindow, MAKEINTRESOURCE(id), IMAGE_ICON, 16, 16, LR_SHARED);
}

HMENU Window::BuildMenu()
{
   HMENU hMenu;
   MENUINFO mi;

   //Create the menu
   hMenu = CreatePopupMenu();

   //Set its style
   mi.cbSize = sizeof(MENUINFO);
   mi.fMask = MIM_STYLE;
   mi.dwStyle = MNS_CHECKORBMP;
   SetMenuInfo(hMenu, &mi);

   //Now add the items
   InsertMenuItem(hMenu, IsAlwaysOnTop(), NULL, VDM_TOGGLEONTOP, IDS_MENU_ALWAYSONTOP);
   InsertMenuItem(hMenu, IsMinimizeToTray(), NULL, VDM_TOGGLEMINIMIZETOTRAY, IDS_MENU_MINTOTRAY);
   InsertMenuItem(hMenu, IsTransparent(), NULL, VDM_TOGGLETRANSPARENCY, IDS_MENU_TRANSPARENT);
   AppendMenu(hMenu, MF_SEPARATOR, 0, 0);

   InsertMenuItem(hMenu, IsOnAllDesktops(), NULL, VDM_TOGGLEALLDESKTOPS, IDS_MENU_ONALLDESKTOPS);
   InsertMenuItem(hMenu, FALSE, NULL, VDM_MOVEWINDOW, IDS_MENU_CHANGEDESKTOP);
   AppendMenu(hMenu, MF_SEPARATOR, 0, 0);

   InsertMenuItem(hMenu, false, HBMMENU_POPUP_RESTORE, VDM_ACTIVATEWINDOW, IDS_MENU_ACTIVATEWND);
   if (IsIconic() || IsZoomed(m_hWnd))
      InsertMenuItem(hMenu, false, HBMMENU_POPUP_RESTORE, VDM_RESTORE, IDS_MENU_RESTOREWND);
   if (!IsIconic())
      InsertMenuItem(hMenu, false, HBMMENU_POPUP_MINIMIZE, VDM_MINIMIZE, IDS_MENU_MINIMIZEWND);
   if (!IsZoomed(m_hWnd))
   {
      InsertMenuItem(hMenu, false, HBMMENU_POPUP_MAXIMIZE, VDM_MAXIMIZE, IDS_MENU_MAXIMIZEWND);
      InsertMenuItem(hMenu, false, LoadBmpRes(IDI_MAXIMIZE_VERT), VDM_MAXIMIZEHEIGHT, IDS_MENU_MAXHEIGHTWND);
      InsertMenuItem(hMenu, false, LoadBmpRes(IDI_MAXIMIZE_HORIZ), VDM_MAXIMIZEWIDTH, IDS_MENU_MAXWIDTHWND);
   }
   InsertMenuItem(hMenu, false, HBMMENU_POPUP_CLOSE, VDM_CLOSE, IDS_MENU_CLOSEWND);
   InsertMenuItem(hMenu, false, LoadBmpRes(IDI_KILL), VDM_KILL, IDS_MENU_KILLWND);
   AppendMenu(hMenu, MF_SEPARATOR, 0, 0);

   InsertMenuItem(hMenu, false, NULL, VDM_PROPERTIES, IDS_MENU_WNDPROPERTIES);

   return hMenu;
}

void Window::OnMenuItemSelected(HMENU /*menu*/, int cmdId)
{
   switch(cmdId)
   {
   case VDM_ACTIVATEWINDOW:
      Activate();
      break;

   case VDM_TOGGLEONTOP:
      ToggleOnTop();
      break;

   case VDM_TOGGLEMINIMIZETOTRAY:
      ToggleMinimizeToTray();
      break;

   case VDM_TOGGLETRANSPARENCY:
      ToggleTransparent();
      break;

   case VDM_TOGGLEALLDESKTOPS:
      ToggleAllDesktops();
      break;

   case VDM_MOVEWINDOW:
      SelectDesktopForWindow(this);
      break;

   case VDM_RESTORE:
      Restore();
      break;

   case VDM_MINIMIZE:
      Minimize();
      break;

   case VDM_MAXIMIZE:
      Maximize();
      break;

   case VDM_MAXIMIZEHEIGHT:
      MaximizeHeight();
      break;

   case VDM_MAXIMIZEWIDTH:
      MaximizeWidth();
      break;

   case VDM_CLOSE:
      PostMessage(m_hWnd, WM_SYSCOMMAND, SC_CLOSE, 0);
      break;

   case VDM_KILL:
      Kill();
      break;

   case VDM_PROPERTIES:
      DisplayWindowProperties();
      break;
   }
}

void Window::SetMinimizeToTray(bool totray)
{
   m_MinToTray = totray;

   if (IsOnCurrentDesk())
   {
      if (m_MinToTray && ::IsIconic(m_hWnd) && !m_hidden)
      {
         // Move minimized icon from taskbar to tray
         m_iconic = true;
         trayManager->AddIcon(this);
         HideWindow();
      }
      else if (!m_MinToTray && m_hidden && m_iconic)
      {
         // Move minimized icon from tray to taskbar
         trayManager->DelIcon(this);
         ShowWindow();
      }
   }
}

void Window::ToggleMinimizeToTray()
{
   SetMinimizeToTray(!IsMinimizeToTray());
}

void Window::ToggleOnTop()
{
   SetAlwaysOnTop(!IsAlwaysOnTop());
}

void Window::SetOnAllDesktops(bool onall)
{
   if (onall)
      MoveToDesktop(NULL);
   else if (IsOnAllDesktops())
      MoveToDesktop(deskMan->GetCurrentDesktop());
}

void Window::ToggleAllDesktops()
{
   SetOnAllDesktops(!IsOnAllDesktops());
}

void Window::SetTransparent(bool transp)
{
   m_transp.SetTransparencyLevel(transp ? m_transpLevel : (unsigned char)TRANSPARENCY_DISABLED);
}

void Window::ToggleTransparent()
{
   if (GetTransparencyLevel() == TRANSPARENCY_DISABLED && !IsTransparent())
      SetTransparencyLevel(Settings::GetDefaultSetting(Settings::Window::TransparencyLevel));
   SetTransparent(!IsTransparent());
}

void Window::SetTransparencyLevel(unsigned char level)
{
   //Update the variable
   m_transpLevel = level;

   //Refresh the display
   SetTransparent(IsTransparent());
}

void Window::Activate()
{
   if (IsIconic())
      Restore();

   if (!IsOnCurrentDesk())
      deskMan->SwitchToDesktop(m_desk);

   m_hOwnedWnd = GetOwnedWindow(m_hWnd);
   SetForegroundWindow(m_hOwnedWnd);
}

void Window::Restore()
{
   if (IsIconic())
   {
      m_iconic = false;

      if (IsOnCurrentDesk())
      {
         if (IsMinimizeToTray() && m_hidden)
         {
            trayManager->DelIcon(this);
            ShowWindow();
         }
         if (::IsIconic(m_hWnd))
            ::ShowWindowAsync(m_hWnd, SW_RESTORE);
      }
   }
   else if (IsZoomed(m_hWnd))
      ::ShowWindowAsync(m_hWnd, SW_RESTORE);
}

void Window::Minimize()
{
   if (IsOnCurrentDesk())
   {
      if (IsMinimizeToTray())
      {
         m_iconic = true;
         trayManager->AddIcon(this);
         HideWindow();
      }
      else
         ::ShowWindowAsync(m_hWnd, SW_MINIMIZE);
   }
   m_iconic = true;
}

void Window::Maximize()
{
   ::ShowWindowAsync(m_hWnd, SW_MAXIMIZE);
}

void Window::MaximizeHeight()
{
   RECT rect;
   HWND hWnd = GetOwnedWindow();

   GetWindowRect(hWnd, &rect);
   RECT screen = PlatformHelper::GetWorkArea(hWnd);
   ::SetWindowPos(hWnd, NULL, rect.left, screen.top,
                  rect.right-rect.left, screen.bottom-screen.top,
                  SWP_NOZORDER | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
}

void Window::MaximizeWidth()
{
   RECT rect;
   HWND hWnd = GetOwnedWindow();

   GetWindowRect(hWnd, &rect);
   RECT screen = PlatformHelper::GetWorkArea(hWnd);
   ::SetWindowPos(hWnd, NULL, screen.left, rect.top,
                  screen.right-screen.left, rect.bottom - rect.top,
                  SWP_NOZORDER | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
}

void Window::Kill()
{
   HANDLE hProcess;
   DWORD pId;

   GetWindowThreadProcessId(m_hWnd, &pId);
   hProcess = OpenProcess( PROCESS_TERMINATE, 0, pId );
   if (hProcess == NULL)
      return;

   if (winMan->ConfirmKillWindow())
      TerminateProcess( hProcess, 9);
   CloseHandle (hProcess);
}

LRESULT Window::OnTrayIconMessage(HWND /*hWnd*/, UINT /*message*/, WPARAM /*wParam*/, LPARAM lParam)
{
   switch(LOWORD(lParam))
   {
   case WM_RBUTTONUP:
   case WM_CONTEXTMENU:
      OnContextMenu();
      break;

   case WM_LBUTTONUP:
      OnMenuItemSelected(NULL, VDM_ACTIVATEWINDOW);
      break;
   }

   return 0;
}

void Window::OnContextMenu()
{
   HMENU hMenu;
   int res;
   POINT pt;

   hMenu = BuildMenu();

   GetCursorPos(&pt);
   SetForegroundWindow(vdWindow);
   res = TrackPopupMenu(hMenu, TPM_RETURNCMD|TPM_RIGHTBUTTON, pt.x, pt.y, 0, vdWindow, NULL);

   if (res >= WM_USER)
      OnMenuItemSelected(hMenu, res);
   else if (res)
      PostMessage(vdWindow, WM_COMMAND, res, 0);

   DestroyMenu(hMenu);
}

void Window::FlashWindow(void)
{
   if (!IsOnCurrentDesk() && !IsWindowFlashing())
   {
      m_BallonMsg = msgManager.Add(Locale::GetInstance().GetString(IDS_FLASH_MESSAGE),
                                   GetText(), (INT_PTR)GetIcon(), &OnFlashBallonClick, (LPARAM)m_hWnd);
   }
}

void Window::UnFlashWindow(void)
{
   if (IsWindowFlashing())
   {
      msgManager.Remove(m_BallonMsg);
      m_BallonMsg = NULL;
   }
}

void Window::OnFlashBallonClick(BalloonNotification::Message /*msg*/, LPARAM data)
{
   Window * wnd = winMan->GetWindow((HWND)data);
   if (wnd)
   {
      wnd->m_BallonMsg = NULL;   //the balloon destroys itself
      wnd->Activate();
   }
}
