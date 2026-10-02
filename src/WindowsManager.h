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

#ifndef __WINDOWSMANAGER_H__
#define __WINDOWSMANAGER_H__

#include <map>
#include <vector>
#include <list>
#include <set>
#include "Desktop.h"
#include "Window.h"
#include "ShellHook.h"
#include "WindowsList.h"
#include "HotkeyConfig.h"
#include "BalloonNotif.h"

#define FIRST_WINDOW_MANAGER_TIMER     100
#define DELAYED_UPDATE_DELAY           1000 /*1sec*/

/** Keeps track of the top-level windows which are managed by Virtual Dimension.
 *
 * The list of windows is maintained from the notifications of the system (WinEvents
 * and shell hook). As the same changes trigger several notifications, and as the
 * notifications caused by Virtual Dimension itself (when hiding/showing windows)
 * must be told apart from the ones caused by the applications, the notifications
 * only schedule a check of the window, which is performed a bit later. The check
 * compares the actual state of the window with the state expected by Virtual
 * Dimension.
 */
class WindowsManager
{
public:
   WindowsManager();
   ~WindowsManager(void);

   void PopulateInitialWindowsSet();

   void MoveWindow(HWND hWnd, Desktop* desk);
   Window* GetWindow(HWND hWnd);

   bool ConfirmKillWindow();
   bool IsConfirmKill() const         { return m_confirmKill; }
   void SetConfirmKill(bool confirm)  { m_confirmKill = confirm; }

   typedef WindowsList::Iterator Iterator;
   Iterator GetIterator()                   { return m_windows.begin(); }
   Iterator FirstWindow()                   { return m_windows.first(); }
   Iterator LastWindow()                    { return m_windows.last(); }

   Window * GetForegroundWindow();

   /** Get the most recently activated window of a desktop, if any. */
   Window * GetTopWindow(Desktop * desk);

   bool IsAutoSwitchDesktop() const         { return m_autoSwitch; }
   void SetAutoSwitchDesktop(bool autoSw)   { m_autoSwitch = autoSw; }

   bool IsShowAllWindowsInTaskList() const  { return m_allWindowsInTaskList; }
   void ShowAllWindowsInTaskList(bool all)  { m_allWindowsInTaskList = all; }

   HWND GetPrevWindow(Window * wnd);

   void EnableAnimations();
   void DisableAnimations();

   void RemoveWindow(Window * win);

   void ScheduleDelayedUpdate(Window * win);
   void CancelDelayedUpdate(Window * win);

   /** Show all the windows hidden by Virtual Dimension.
    * Used as a last resort, when the program crashes.
    */
   void EmergencyRestore();

   /** Tell if a window should be managed by Virtual Dimension. */
   static bool IsCandidateWindow(HWND hWnd);

   LRESULT OnSettingsChange(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

protected:
   map<HWND, WindowsList::Node*> m_HWNDMap;
   WindowsList m_windows;
   list<Window*> m_zorder;

   typedef map<HWND, WindowsList::Node*>::iterator HWNDMapIterator;
   typedef list<Window*>::iterator ZOrderIterator;

   ShellHook m_shellhook;

   bool m_confirmKill;
   bool m_autoSwitch;
   bool m_allWindowsInTaskList;
   int m_iAnimate;
   LONG m_nbDisabledAnimations;

   Window * AddWindow(HWND hWnd);
   Window * FindWindowOrOwner(HWND hWnd);

   LRESULT OnStartOnDesktop(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
   LRESULT OnCopyData(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

   // Programs started from the command line, which windows must go to some desktop
   //**************************************************************************
   struct StartRequest
   {
      DWORD processId;
      int desktop;
      std::wstring program;
      ULONGLONG deadline;
   };
   std::vector<StartRequest> m_startRequests;

   void AddStartRequest(DWORD processId, int desktop, LPCWSTR program);
   void ApplyStartRequests(Window * win);
   LRESULT OnShellHookMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

   void OnWindowActivated(HWND hWnd);     //activation changed to another window
   void OnRedraw(HWND hWnd);              //window's title/icon changed
   void OnWindowFlash(HWND hWnd);         //window is flashing

   // Window events
   //**************************************************************************
   std::vector<HWINEVENTHOOK> m_winEventHooks;
   std::set<HWND> m_pendingChecks;        //windows which state should be checked
   bool m_layoutChanged;                  //preview window layout must be refreshed
   UINT_PTR m_checkTimer;
   bool m_checkTimerSet;

   void InstallWinEventHooks();
   void RemoveWinEventHooks();
   static void CALLBACK WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hWnd,
                                     LONG idObject, LONG idChild, DWORD idEventThread, DWORD dwmsEventTime);
   void OnWinEvent(DWORD event, HWND hWnd);

   void ScheduleCheck(HWND hWnd);
   void ScheduleLayoutUpdate();
   void StartCheckTimer();
   LRESULT OnCheckTimer(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
   void CheckWindow(HWND hWnd);

   // Delayed window update
   //**************************************************************************
   typedef vector<Window *>::iterator DelayedUdateWndIterator;
   vector<Window *> m_delayedUpdateWndTab; //List of the windows which have requested delayed update
   vector<int> m_delayedUpdateNextTab;
   int m_firstFreeDelayedUpdateWndIdx;

   unsigned int AddDelayedUpdateWnd(Window * wnd);
   void RemoveDelayedUpdateWnd(unsigned int idx);
   static void CALLBACK OnDelayedUpdateTimer(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);

   // Various hotkeys
   //**************************************************************************
   class MoveWindowToNextDesktopEventHandler: public PersistentHotkey<Settings::MoveWindowToNextDesktopHotkey>
   {
   public:
      virtual void OnHotkey();
      virtual LPCWSTR GetName() const  { return L"Move window to next desk"; }
   };

   class MoveWindowToPrevDesktopEventHandler: public PersistentHotkey<Settings::MoveWindowToPreviousDesktopHotkey>
   {
   public:
      virtual void OnHotkey();
      virtual LPCWSTR GetName() const  { return L"Move window to previous desk"; }
   };

   class MoveWindowToDesktopEventHandler: public PersistentHotkey<Settings::MoveWindowToDesktopHotkey>
   {
   public:
      virtual void OnHotkey();
      virtual LPCWSTR GetName() const  { return L"Move window to some desk"; }
   };

   class MaximizeHeightEventHandler: public PersistentHotkey<Settings::MaximizeHeightHotkey>
   {
   public:
      virtual void OnHotkey();
      virtual LPCWSTR GetName() const  { return L"Maximize height"; }
   };

   class MaximizeWidthEventHandler: public PersistentHotkey<Settings::MaximizeWidthHotkey>
   {
   public:
      virtual void OnHotkey();
      virtual LPCWSTR GetName() const  { return L"Maximize width"; }
   };

   class ToggleAlwaysOnTopEventHandler: public PersistentHotkey<Settings::AlwaysOnTopHotkey>
   {
   public:
      virtual void OnHotkey();
      virtual LPCWSTR GetName() const  { return L"Toggle always on top"; }
   };

   class ToggleTransparencyEventHandler: public PersistentHotkey<Settings::TransparencyHotkey>
   {
   public:
      virtual void OnHotkey();
      virtual LPCWSTR GetName() const  { return L"Toggle transparency"; }
   };

   MoveWindowToNextDesktopEventHandler m_moveToNextDeskEH;
   MoveWindowToPrevDesktopEventHandler m_moveToPrevDeskEH;
   MoveWindowToDesktopEventHandler m_moveToDesktopEH;
   MaximizeHeightEventHandler m_maximizeHeightEH;
   MaximizeWidthEventHandler m_maximizeWidthEH;
   ToggleAlwaysOnTopEventHandler m_toggleAlwaysOnTopEH;
   ToggleTransparencyEventHandler m_toggleTransparencyEH;

   static BOOL CALLBACK ListWindowsProc( HWND hWnd, LPARAM lParam );
};

extern WindowsManager * winMan;

#endif /*__WINDOWSMANAGER_H__*/
