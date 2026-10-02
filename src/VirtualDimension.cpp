/*
 * Virtual Dimension -  a free, fast, and feature-full virtual desktop manager
 * for the Microsoft Windows platform.
 * Copyright (C) 2003-2005 Francois Ferrand
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

// Virtual Dimension.cpp : Defines the entry point for the application.
//
#include "stdafx.h"
#include <windowsx.h>
#include "VirtualDimension.h"
#include "Settings.h"
#include "DesktopManager.h"
#include "WindowsManager.h"
#include "HotKeyManager.h"
#include "ToolTip.h"
#include "FastWindow.h"
#include "HotKeyControl.h"
#include "LinkControl.h"
#include "ExplorerWrapper.h"
#include "PlatformHelper.h"
#include "WallPaper.h"
#include "Messages.h"
#include "Locale.h"
#include "CmdLine.h"

// Global Variables:
HWND configBox = NULL;
Transparency * transp;
TrayIcon * trayIcon;
AlwaysOnTop * ontop;
ToolTip * tooltip;

VirtualDimension vdWindow;

// Forward function definition
HWND CreateConfigBox();

/** Last resort handler: if the program crashes, make sure that the windows it hid
 * are displayed again, else they would be lost for the user.
 */
static LONG WINAPI CrashHandler(EXCEPTION_POINTERS * /*exceptionInfo*/)
{
   static LONG reentrance = 0;

   if (InterlockedIncrement(&reentrance) == 1 && winMan)
      winMan->EmergencyRestore();

   return EXCEPTION_CONTINUE_SEARCH;
}

int APIENTRY wWinMain( HINSTANCE hInstance,
                       HINSTANCE /*hPrevInstance*/,
                       LPWSTR    lpCmdLine,
                       int       nCmdShow)
{
   MSG msg;
   HACCEL hAccelTable;
   HANDLE hInstanceMutex;
   INITCOMMONCONTROLSEX icc;

   icc.dwSize = sizeof(icc);
   icc.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES | ICC_LINK_CLASS;
   InitCommonControlsEx(&icc);
   if (FAILED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)))
      return -1;

   // If a previous instance is running, forward the command line to it (or
   // activate it) and terminate this one.
   hInstanceMutex = CreateMutexW(NULL, FALSE, L"Local\\VirtualDimension.SingleInstance");
   if (GetLastError() == ERROR_ALREADY_EXISTS)
   {
      HWND hwndPrev = VirtualDimension::FindWindow();

      if (lpCmdLine && *lpCmdLine)
      {
         CommandLineParser parser;
         parser.ParseCommandLine(lpCmdLine);
      }
      else if (hwndPrev != NULL)
      {
         AllowSetForegroundWindow(ASFW_ANY);
         SetForegroundWindow(hwndPrev);
      }

      CloseHandle(hInstanceMutex);
      CoUninitialize();
      return -1;
   }

   SetUnhandledExceptionFilter(CrashHandler);

   if (!vdWindow.Start(hInstance, nCmdShow))
   {
      CloseHandle(hInstanceMutex);
      CoUninitialize();
      return -1;
   }

   // Load accelerators
   hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_VIRTUALDIMENSION));

   // Main message loop:
   while (GetMessage(&msg, NULL, 0, 0) > 0)
   {
      if (IsWindow(configBox) && IsDialogMessage(configBox, &msg))
      {
         if (NULL == PropSheet_GetCurrentPageHwnd(configBox))
         {
            DestroyWindow(configBox);
            configBox = NULL;
         }
      }
      else if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
      {
         TranslateMessage(&msg);
         DispatchMessage(&msg);
      }
   }

   // Restore the wallpaper of Windows
   WallPaper::Shutdown();

   CloseHandle(hInstanceMutex);
   CoUninitialize();

   return (int) msg.wParam;
}

VirtualDimension::VirtualDimension(): m_draggedWindow(NULL), m_dragCursor(NULL), m_pSysMenu(NULL),
                                      m_hInstance(NULL), m_iconSize(16)
{
   *m_szTitle = 0;
}

bool VirtualDimension::Start(HINSTANCE hInstance, int nCmdShow)
{
   HWND hWnd;
   RECT pos;
   Settings settings;
   DWORD dwStyle;

   m_hInstance = hInstance;
   LoadString(m_hInstance, IDS_APP_TITLE, m_szTitle, MAX_LOADSTRING);

   InitHotkeyControl();
   InitHyperLinkControl();

   // Register the window class
   RegisterClass();

   // Bind the message handlers
   SetCommandHandler(IDM_ABOUT, this, &VirtualDimension::OnCmdAbout);
   SetSysCommandHandler(IDM_ABOUT, this, &VirtualDimension::OnCmdAbout);
   SetCommandHandler(IDM_CONFIGURE, this, &VirtualDimension::OnCmdConfigure);
   SetSysCommandHandler(IDM_CONFIGURE, this, &VirtualDimension::OnCmdConfigure);
   SetCommandHandler(IDM_EXIT, this, &VirtualDimension::OnCmdExit);
   SetSysCommandHandler(SC_CLOSE, this, &VirtualDimension::OnCmdExit);
   SetCommandHandler(IDM_LOCKPREVIEWWND, this, &VirtualDimension::OnCmdLockPreviewWindow);
   SetSysCommandHandler(IDM_LOCKPREVIEWWND, this, &VirtualDimension::OnCmdLockPreviewWindow);
   SetCommandHandler(IDM_SHOWCAPTION, this, &VirtualDimension::OnCmdShowCaption);
   SetSysCommandHandler(IDM_SHOWCAPTION, this, &VirtualDimension::OnCmdShowCaption);

   SetMessageHandler(WM_DESTROY, this, &VirtualDimension::OnDestroy);
   SetMessageHandler(WM_ENDSESSION, this, &VirtualDimension::OnEndSession);
   SetMessageHandler(WM_MOVE, this, &VirtualDimension::OnMove);
   SetMessageHandler(WM_WINDOWPOSCHANGING, this, &VirtualDimension::OnWindowPosChanging);
   SetMessageHandler(WM_DISPLAYCHANGE, this, &VirtualDimension::OnDisplayChange);
   SetMessageHandler(WM_SHOWWINDOW, this, &VirtualDimension::OnShowWindow);

   SetMessageHandler(WM_LBUTTONDOWN, this, &VirtualDimension::OnLeftButtonDown);
   SetMessageHandler(WM_LBUTTONUP, this, &VirtualDimension::OnLeftButtonUp);
   SetMessageHandler(WM_LBUTTONDBLCLK, this, &VirtualDimension::OnLeftButtonDblClk);
   SetMessageHandler(WM_RBUTTONDOWN, this, &VirtualDimension::OnRightButtonDown);

   SetMessageHandler(WM_MEASUREITEM, this, &VirtualDimension::OnMeasureItem);
   SetMessageHandler(WM_DRAWITEM, this, &VirtualDimension::OnDrawItem);

   m_autoHideTimerId = CreateTimer(this, &VirtualDimension::OnTimer);
   SetMessageHandler(WM_ACTIVATEAPP, this, &VirtualDimension::OnActivateApp);

   SetMessageHandler(WM_MOUSEHOVER, this, &VirtualDimension::OnMouseHover);
   SetMessageHandler(WM_MOUSELEAVE, this, &VirtualDimension::OnMouseLeave);
   SetMessageHandler(WM_NCHITTEST, this, &VirtualDimension::OnNCHitTest);
   SetMessageHandler(WM_ERASEBKGND, this, &VirtualDimension::OnEraseBackground);

   // compare the window's style
   m_hasCaption = settings.LoadSetting(Settings::HasCaption);
   dwStyle = WS_POPUP | WS_SYSMENU | (m_hasCaption ? WS_CAPTION : WS_DLGFRAME);

   // Reload the window's position
   settings.LoadSetting(Settings::WindowPosition, &pos);
   AdjustWindowRectEx(&pos, dwStyle, FALSE, WS_EX_TOOLWINDOW);

   // Dock the window to the screen borders
   m_dockedBorders = settings.LoadSetting(Settings::DockedBorders);
   DockWindow(pos);

   // Create the main window
   Create( WS_EX_TOOLWINDOW, VD_WINDOW_CLASS, m_szTitle, dwStyle,
           pos.left, pos.top, pos.right - pos.left, pos.bottom - pos.top,
           NULL, NULL, hInstance);
   if (!IsValid())
      return false;

   hWnd = *this;
   UpdateIconSize();

   // Load some settings
   m_snapSize = settings.LoadSetting(Settings::SnapSize);
   m_autoHideDelay = settings.LoadSetting(Settings::AutoHideDelay);
   m_shrinked = false;

   m_tracking = false;

   //Ensure the window gets docked if it is close enough to the borders
   SetWindowPos(hWnd, NULL, pos.left, pos.top, pos.right - pos.left, pos.bottom - pos.top, SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOOWNERZORDER);

   // Setup the system menu
   m_pSysMenu = GetSystemMenu(hWnd, FALSE);
   if (m_pSysMenu != NULL)
   {
      RemoveMenu(m_pSysMenu, SC_RESTORE, MF_BYCOMMAND);
      RemoveMenu(m_pSysMenu, SC_MINIMIZE, MF_BYCOMMAND);
      RemoveMenu(m_pSysMenu, SC_MAXIMIZE, MF_BYCOMMAND);
      RemoveMenu(m_pSysMenu, SC_MOVE, MF_BYCOMMAND);
      RemoveMenu(m_pSysMenu, SC_SIZE, MF_BYCOMMAND);
      RemoveMenu(m_pSysMenu, 0, MF_BYCOMMAND);

      AppendMenu(m_pSysMenu, MF_SEPARATOR, 0, NULL);
      AppendMenu(m_pSysMenu, MF_STRING, IDM_CONFIGURE, Locale::GetInstance().GetString(IDS_CONFIGURE));
      AppendMenu(m_pSysMenu, MF_STRING, IDM_LOCKPREVIEWWND, Locale::GetInstance().GetString(IDS_LOCKPREVIEWWND));
      AppendMenu(m_pSysMenu, MF_STRING, IDM_SHOWCAPTION, Locale::GetInstance().GetString(IDS_SHOWCAPTION));
      AppendMenu(m_pSysMenu, MF_STRING, IDM_ABOUT, Locale::GetInstance().GetString(IDS_ABOUT));
      CheckMenuItem(m_pSysMenu, IDM_SHOWCAPTION, m_hasCaption ? MF_CHECKED : MF_UNCHECKED );
   }

   // Lock the preview window as appropriate
   LockPreviewWindow(settings.LoadSetting(Settings::LockPreviewWindow));

   // Bind to explorer
   explorerWrapper = new ExplorerWrapper(this);

   // Initialize the tray icon manager
   trayManager = new TrayIconsManager();

   // Initialize tray icon
   trayIcon = new TrayIcon(hWnd);

   // Initialize transparency (set value two times, to make a fade-in)
   transp = new Transparency(hWnd);
   transp->SetTransparencyLevel(0);
   transp->SetTransparencyLevel(settings.LoadSetting(Settings::TransparencyLevel), true);

   // Initialize always on top state
   ontop = new AlwaysOnTop(hWnd);
   ontop->SetAlwaysOnTop(settings.LoadSetting(Settings::AlwaysOnTop));

   // Create the tooltip
   tooltip = new ToolTip(hWnd);

   // Create mouse warp
   mousewarp = new MouseWarp();

   // Create the windows manager
   winMan = new WindowsManager;

   // Create the desk manager
   settings.LoadSetting(Settings::WindowPosition, &pos);   //use client position
   deskMan = new DesktopManager(pos.right - pos.left, pos.bottom - pos.top);

   // Retrieve the initial list of windows
   winMan->PopulateInitialWindowsSet();

   //Update tray icon tooltip
   trayIcon->Update();

   //Bind some additional message handlers (which need the desktop manager)
   SetMessageHandler(WM_SIZE, this, &VirtualDimension::OnSize);
   SetMessageHandler(WM_PAINT, deskMan, &DesktopManager::OnPaint);
   SetMessageHandler(WM_DPICHANGED, this, &VirtualDimension::OnDpiChanged);
   SetMessageHandler(WM_SETTINGCHANGE, this, &VirtualDimension::OnSettingChange);

   // Show window if needed
   if ((m_isWndVisible = (settings.LoadSetting(Settings::ShowWindow) || !trayIcon->HasIcon())) == true)
   {
      ShowWindow(hWnd, nCmdShow);
      Refresh();
   }

   return true;
}

VirtualDimension::~VirtualDimension()
{
}

void VirtualDimension::UpdateIconSize()
{
   m_iconSize = PlatformHelper::ScaleForWindow(m_hWnd, 16);
}

void VirtualDimension::LockPreviewWindow(bool lock)
{
   LONG_PTR style;

   m_lockPreviewWindow = lock;

   CheckMenuItem(m_pSysMenu, IDM_LOCKPREVIEWWND, m_lockPreviewWindow ? MF_CHECKED : MF_UNCHECKED );

   style = GetWindowLongPtr(m_hWnd, GWL_STYLE);
   if (m_lockPreviewWindow)
   {
      style &= ~WS_THICKFRAME;

      RemoveMenu(m_pSysMenu, SC_MOVE, MF_BYCOMMAND);
      RemoveMenu(m_pSysMenu, SC_SIZE, MF_BYCOMMAND);
   }
   else
   {
      style |= WS_THICKFRAME;

      InsertMenu(m_pSysMenu, 0, MF_BYPOSITION, SC_SIZE, Locale::GetInstance().GetString(IDS_MENU_SIZE)); // "&Size"
      InsertMenu(m_pSysMenu, 0, MF_BYPOSITION, SC_MOVE, Locale::GetInstance().GetString(IDS_MENU_MOVE)); // "&Move"
   }
   ApplyFrameStyle(style);
}

/** Change the frame of the window (caption, sizing border), keeping the client area
 * (ie, the preview) at the same place and with the same size.
 */
void VirtualDimension::ApplyFrameStyle(LONG_PTR style)
{
   RECT rect;

   //While shrinked, the window has no frame at all: the style is applied by UnShrink()
   if (m_shrinked || !IsValid())
      return;

   //Client area, in screen coordinates
   GetClientRect(m_hWnd, &rect);
   MapWindowPoints(m_hWnd, NULL, (LPPOINT)&rect, 2);

   SetWindowLongPtr(m_hWnd, GWL_STYLE, style);

   //Window rectangle needed for that client area with the new frame
   AdjustWindowRectExForDpi(&rect, (DWORD)style, FALSE, (DWORD)GetWindowLongPtr(m_hWnd, GWL_EXSTYLE),
                            GetDpiForWindow(m_hWnd));

   //A window docked to the screen borders stays docked (else the caption may go off-screen)
   DockWindow(rect);

   //Let the system recompute the frame (SWP_FRAMECHANGED), then repaint everything
   SetWindowPos(m_hWnd, NULL, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
   RedrawWindow(m_hWnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

void VirtualDimension::ShowCaption(bool caption)
{
   LONG_PTR style;

   m_hasCaption = caption;

   CheckMenuItem(m_pSysMenu, IDM_SHOWCAPTION, m_hasCaption ? MF_CHECKED : MF_UNCHECKED );

   style = GetWindowLongPtr(m_hWnd, GWL_STYLE);
   if (m_hasCaption)
   {
      style &= ~WS_DLGFRAME;
      style |= WS_CAPTION;
   }
   else
   {
      style &= ~WS_CAPTION;
      style |= WS_DLGFRAME;
   }
   ApplyFrameStyle(style);
}

ATOM VirtualDimension::RegisterClass()
{
   WNDCLASSEX wcex;

   wcex.cbSize = sizeof(WNDCLASSEX);

   wcex.style        = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
   wcex.cbClsExtra   = 0;
   wcex.cbWndExtra   = 0;
   wcex.hInstance    = m_hInstance;
   wcex.hIcon        = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_VIRTUALDIMENSION));
   wcex.hCursor      = LoadCursor(NULL, IDC_ARROW);
   wcex.hbrBackground   = (HBRUSH)GetStockObject(HOLLOW_BRUSH);
   wcex.lpszMenuName = 0;
   wcex.lpszClassName   = VD_WINDOW_CLASS;
   wcex.hIconSm      = NULL;

   return FastWindow::RegisterClassEx(&wcex);
}

LRESULT VirtualDimension::OnCmdAbout(HWND hWnd, UINT /*message*/, WPARAM /*wParam*/, LPARAM /*lParam*/)
{
   DialogBox(vdWindow, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);

   return 0;
}

LRESULT VirtualDimension::OnCmdLockPreviewWindow(HWND /*hWnd*/, UINT /*message*/, WPARAM /*wParam*/, LPARAM /*lParam*/)
{
   LockPreviewWindow(!IsPreviewWindowLocked());

   return 0;
}

LRESULT VirtualDimension::OnCmdShowCaption(HWND /*hWnd*/, UINT /*message*/, WPARAM /*wParam*/, LPARAM /*lParam*/)
{
   ShowCaption(!HasCaption());

   return 0;
}

LRESULT VirtualDimension::OnCmdConfigure(HWND /*hWnd*/, UINT /*message*/, WPARAM /*wParam*/, LPARAM /*lParam*/)
{
   if (!configBox)
      configBox = CreateConfigBox();
   else
      SetForegroundWindow(configBox);

   return 0;
}

LRESULT VirtualDimension::OnCmdExit(HWND hWnd, UINT /*message*/, WPARAM /*wParam*/, LPARAM /*lParam*/)
{
   DestroyWindow(hWnd);
   return 0;
}

LRESULT VirtualDimension::OnLeftButtonDown(HWND hWnd, UINT /*message*/, WPARAM /*wParam*/, LPARAM lParam)
{
   POINT pt;
   BOOL screenPos = FALSE;

   pt.x = GET_X_LPARAM(lParam);
   pt.y = GET_Y_LPARAM(lParam);

   if (m_shrinked)
   {
      if (!IsPreviewWindowLocked() &&         //for performance reasons only
          (ClientToScreen(hWnd, &pt)) &&
          (DragDetect(hWnd, pt)))
      {
         //trick windows into thinking we are dragging the title bar, to let the user move the window
         m_draggedWindow = NULL;
         m_dragCursor = NULL;
         ReleaseCapture();
         ::SendMessage(hWnd,WM_NCLBUTTONDOWN,HTCAPTION,MAKELPARAM(pt.x, pt.y));
      }
      else
         UnShrink();
   }
   else
   {
      //Stop the hide timer, to ensure the window does not get hidden
      KillTimer(m_autoHideTimerId);

      //Find the item under the mouse, and check if it's being dragged
      Desktop * desk = deskMan->GetDesktopFromPoint(pt.x, pt.y);
      if ( (desk) &&
           ((m_draggedWindow = desk->GetWindowFromPoint(pt.x, pt.y)) != NULL) &&
           (!m_draggedWindow->IsOnDesk(NULL)) &&
           ((screenPos = ClientToScreen(hWnd, &pt)) != FALSE) &&
           (DragDetect(hWnd, pt)) )
      {
         ICONINFO icon;

         //Dragging a window's icon
         SetCapture(hWnd);

         if (GetIconInfo(m_draggedWindow->GetIcon(), &icon))
         {
            icon.fIcon = FALSE;
            m_dragCursor = (HCURSOR)CreateIconIndirect(&icon);
            if (icon.hbmColor)
               DeleteObject(icon.hbmColor);
            if (icon.hbmMask)
               DeleteObject(icon.hbmMask);
            SetCursor(m_dragCursor);
         }
      }
      else if (!IsPreviewWindowLocked() &&         //for performance reasons only
               (screenPos || ClientToScreen(hWnd, &pt)) &&
               (DragDetect(hWnd, pt)))
      {
         //trick windows into thinking we are dragging the title bar, to let the user move the window
         m_draggedWindow = NULL;
         m_dragCursor = NULL;
         ReleaseCapture();
         ::SendMessage(hWnd,WM_NCLBUTTONDOWN,HTCAPTION,MAKELPARAM(pt.x, pt.y));
      }
      else
      {
         //switch to the desktop that was clicked
         m_draggedWindow = NULL;
         deskMan->SwitchToDesktop(desk);
      }
   }

   return 0;
}

LRESULT VirtualDimension::OnLeftButtonUp(HWND /*hWnd*/, UINT /*message*/, WPARAM /*wParam*/, LPARAM lParam)
{
   POINT pt;
   Window * draggedWindow = m_draggedWindow;

   //If not dragging a window, nothing to do
   if (draggedWindow == NULL)
      return 0;
   m_draggedWindow = NULL;

   //Release capture
   ReleaseCapture();

   //Free the cursor
   if (m_dragCursor)
      DestroyCursor(m_dragCursor);
   m_dragCursor = NULL;

   pt.x = GET_X_LPARAM(lParam);
   pt.y = GET_Y_LPARAM(lParam);

   //Find out the target desktop
   Desktop * desk = deskMan->GetDesktopFromPoint(pt.x, pt.y);
   if (desk == NULL || draggedWindow->IsOnDesk(desk))
      return 0;   //window already on the target desk

   //Move the window to this desktop
   draggedWindow->MoveToDesktop(desk);

   //Refresh the window
   Refresh();

   return 0;
}

LRESULT VirtualDimension::OnLeftButtonDblClk(HWND /*hWnd*/, UINT /*message*/, WPARAM /*wParam*/, LPARAM lParam)
{
   POINT pt;
   Window * window;
   Desktop * desk;

   if (m_shrinked)
      return 0;

   pt.x = GET_X_LPARAM(lParam);
   pt.y = GET_Y_LPARAM(lParam);

   desk = deskMan->GetDesktopFromPoint(pt.x, pt.y);
   if ( (desk) &&
        ((window = desk->GetWindowFromPoint(pt.x, pt.y)) != NULL) )
      window->Activate();
   return 0;
}

LRESULT VirtualDimension::OnRightButtonDown(HWND hWnd, UINT /*message*/, WPARAM wParam, LPARAM lParam)
{
   HMENU hMenu = NULL, hBaseMenu;
   POINT pt;
   int res;

   pt.x = GET_X_LPARAM(lParam);
   pt.y = GET_Y_LPARAM(lParam);

   //Stop the hide timer, to ensure the window does not get hidden
   KillTimer(m_autoHideTimerId);

   //Get the context menu
   Desktop * desk = deskMan->GetDesktopFromPoint(pt.x, pt.y);
   Window * window = NULL;
   if ((!m_shrinked) &&
       ((wParam & MK_CONTROL) == 0) &&
       (desk != NULL))
   {
      window = desk->GetWindowFromPoint(pt.x, pt.y);
      if (window)
         hMenu = window->BuildMenu();
      else
         hMenu = desk->BuildMenu();
   }

   //If no window on desktop, or no menu for the window, display system menu
   if (hMenu == NULL || GetMenuItemCount(hMenu) == 0)
   {
      hBaseMenu = hMenu; //destroy the newly created menu, even if we don't use it
      hMenu = m_pSysMenu;
   }
   else
      hBaseMenu = hMenu;

   assert(hMenu != NULL);

   //And show the menu
   ClientToScreen(hWnd, &pt);
   res = TrackPopupMenu(hMenu, TPM_RETURNCMD|TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);

   //Process the resulting message
   if (res == 0)
      ;  //menu cancelled
   else if (hMenu == m_pSysMenu)
      PostMessage(hWnd, WM_SYSCOMMAND, res, 0);
   else if (res >= WM_USER)
   {
      if (window != NULL)
         window->OnMenuItemSelected(hMenu, res);
      else
         desk->OnMenuItemSelected(hMenu, res);
   }
   else
      PostMessage(hWnd, WM_COMMAND, res, 0);

   if (hBaseMenu)
      DestroyMenu(hBaseMenu);

   return 0;
}

LRESULT VirtualDimension::OnDestroy(HWND /*hWnd*/, UINT /*message*/, WPARAM /*wParam*/, LPARAM /*lParam*/)
{
   RECT pos;
   Settings settings;

   // Before exiting, save the window position
   pos.left = m_location.x;
   pos.top = m_location.y;
   pos.right = m_location.x + deskMan->GetWindowWidth();
   pos.bottom = m_location.y + deskMan->GetWindowHeight();
   settings.SaveSetting(Settings::WindowPosition, &pos);
   settings.SaveSetting(Settings::DockedBorders, m_dockedBorders);

   //Save the snap size
   settings.SaveSetting(Settings::SnapSize, m_snapSize);

   //Save the auto-hide delay
   settings.SaveSetting(Settings::AutoHideDelay, m_autoHideDelay);

   //Save the visibility state of the window before it is hidden
   settings.SaveSetting(Settings::ShowWindow, m_isWndVisible);

   //Save the locking state of the window
   settings.SaveSetting(Settings::LockPreviewWindow, IsPreviewWindowLocked());

   //Save the visibility state of the title bar
   settings.SaveSetting(Settings::HasCaption, HasCaption());

   // Close the configuration dialog
   if (IsWindow(configBox))
      DestroyWindow(configBox);
   configBox = NULL;

   // Remove the tray icon
   delete trayIcon;
   trayIcon = NULL;

   // Cleanup transparency
   settings.SaveSetting(Settings::TransparencyLevel, transp->GetTransparencyLevel());
   delete transp;
   transp = NULL;

   // Cleanup always on top state
   settings.SaveSetting(Settings::AlwaysOnTop, ontop->IsAlwaysOnTop());
   delete ontop;
   ontop = NULL;

   // Destroy the mouse warp
   delete mousewarp;
   mousewarp = NULL;

   // Destroy the desktop manager (shows all the windows)
   delete deskMan;

   // Destroy the windows manager
   delete winMan;
   winMan = NULL;
   deskMan = NULL;

   // Destroy the tooltip
   delete tooltip;
   tooltip = NULL;

   // Destroy the tray icons manager
   delete trayManager;
   trayManager = NULL;

   delete explorerWrapper;
   explorerWrapper = NULL;

   PostQuitMessage(0);

   return 0;
}

LRESULT VirtualDimension::OnMeasureItem(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
   LPMEASUREITEMSTRUCT lpmis = (LPMEASUREITEMSTRUCT)lParam;

   if (wParam != 0)
      return DefWindowProc(hWnd, message, wParam, lParam);

   lpmis->itemHeight = GetSystemMetrics(SM_CYSMICON);
   lpmis->itemWidth = GetSystemMetrics(SM_CXSMICON);

   return TRUE;
}

LRESULT VirtualDimension::OnDrawItem(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
   LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lParam;

   if (wParam != 0)
      return DefWindowProc(hWnd, message, wParam, lParam);

   DrawIconEx(lpdis->hDC, lpdis->rcItem.left, lpdis->rcItem.top, (HICON)lpdis->itemData,
              GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0, NULL, DI_NORMAL);

   return TRUE;
}

LRESULT VirtualDimension::OnEndSession(HWND /*hWnd*/, UINT /*message*/, WPARAM wParam, LPARAM /*lParam*/)
{
   if (wParam)
      //The session is ending -> destroy the window
      DestroyWindow(m_hWnd);

   return 0;
}

LRESULT VirtualDimension::OnMove(HWND /*hWnd*/, UINT /*message*/, WPARAM /*wParam*/, LPARAM lParam)
{
   if (!m_shrinked)
   {
      m_location.x = GET_X_LPARAM(lParam);
      m_location.y = GET_Y_LPARAM(lParam);
   }

   return 0;
}

LRESULT VirtualDimension::OnWindowPosChanging(HWND /*hWnd*/, UINT /*message*/, WPARAM /*wParam*/, LPARAM lParam)
{
   RECT deskRect;
   RECT newRect;
   WINDOWPOS * lpwndpos = (WINDOWPOS*)lParam;

   // No action if the window is not moved or sized
   if ((lpwndpos->flags & SWP_NOMOVE) && (lpwndpos->flags & SWP_NOSIZE))
      return TRUE;

   // Get work area dimensions (of the monitor where the window goes)
   if ((lpwndpos->flags & SWP_NOMOVE) || (lpwndpos->flags & SWP_NOSIZE))
   {
      GetWindowRect(m_hWnd, &newRect);
      if (!(lpwndpos->flags & SWP_NOMOVE))
         OffsetRect(&newRect, lpwndpos->x - newRect.left, lpwndpos->y - newRect.top);
      else
      {
         lpwndpos->x = newRect.left;
         lpwndpos->y = newRect.top;
      }
      if (lpwndpos->flags & SWP_NOSIZE)
      {
         lpwndpos->cx = newRect.right - newRect.left;
         lpwndpos->cy = newRect.bottom - newRect.top;
      }
   }
   SetRect(&newRect, lpwndpos->x, lpwndpos->y, lpwndpos->x + lpwndpos->cx, lpwndpos->y + lpwndpos->cy);
   deskRect = PlatformHelper::GetWorkArea(newRect);

   if (!m_shrinked)
   {
      // Snap to screen border
      m_dockedBorders = 0;
      if( (lpwndpos->x >= -m_snapSize + deskRect.left) &&
          (lpwndpos->x <= deskRect.left + m_snapSize) )
      {
         //Left border
         lpwndpos->x = deskRect.left;
         m_dockedBorders |= DOCK_LEFT;
      }
      if( (lpwndpos->y >= -m_snapSize + deskRect.top) &&
          (lpwndpos->y <= deskRect.top + m_snapSize) )
      {
         // Top border
         lpwndpos->y = deskRect.top;
         m_dockedBorders |= DOCK_TOP;
      }
      if( (lpwndpos->x + lpwndpos->cx <= deskRect.right + m_snapSize) &&
          (lpwndpos->x + lpwndpos->cx >= deskRect.right - m_snapSize) )
      {
         // Right border
         lpwndpos->x = deskRect.right - lpwndpos->cx;
         m_dockedBorders |= DOCK_RIGHT;
      }
      if( (lpwndpos->y + lpwndpos->cy <= deskRect.bottom + m_snapSize) &&
          (lpwndpos->y + lpwndpos->cy >= deskRect.bottom - m_snapSize) )
      {
         // Bottom border
         lpwndpos->y = deskRect.bottom - lpwndpos->cy;
         m_dockedBorders |= DOCK_BOTTOM;
      }
   }
   else
   {
      //Constrain to borders
      if (lpwndpos->x < deskRect.left)
         lpwndpos->x = deskRect.left;
      if (lpwndpos->x+lpwndpos->cx > deskRect.right)
         lpwndpos->x = deskRect.right - lpwndpos->cx;
      if (lpwndpos->y < deskRect.top)
         lpwndpos->y = deskRect.top;
      if (lpwndpos->y+lpwndpos->cy > deskRect.bottom)
         lpwndpos->y = deskRect.bottom - lpwndpos->cy;

      int xdist = std::min(lpwndpos->x-deskRect.left, deskRect.right-lpwndpos->x-lpwndpos->cx) >> 4;
      int ydist = std::min(lpwndpos->y-deskRect.top, deskRect.bottom-lpwndpos->y-lpwndpos->cy) >> 4;

      m_dockedBorders = 0;
      if (xdist <= ydist)
      {
         //Dock to left/right
         if (2*lpwndpos->x+lpwndpos->cx > deskRect.right+deskRect.left)
         {
            //dock to right
            lpwndpos->x = deskRect.right - lpwndpos->cx;
            m_dockedBorders |= DOCK_RIGHT;
         }
         else
         {
            //dock to left
            lpwndpos->x = deskRect.left;
            m_dockedBorders |= DOCK_LEFT;
         }
      }
      if (xdist >= ydist)
      {
         //Dock to top/bottom
         if (2*lpwndpos->y+lpwndpos->cy > deskRect.bottom+deskRect.top)
         {
            //dock to bottom
            lpwndpos->y = deskRect.bottom - lpwndpos->cy;
            m_dockedBorders |= DOCK_BOTTOM;
         }
         else
         {
            //dock to top
            lpwndpos->y = deskRect.top;
            m_dockedBorders |= DOCK_TOP;
         }
      }
   }

   return TRUE;
}

/** Update the rectangle, to dock the window.
 * @return true if the docking caused the rect to change, else false.
 */
bool VirtualDimension::DockWindow(RECT & pos)
{
   RECT deskRect = PlatformHelper::GetWorkArea(pos);
   bool res = false;

   if (m_dockedBorders & DOCK_LEFT)
   {
      pos.right -= pos.left - deskRect.left;
      pos.left = deskRect.left;
      res = true;
   }
   if (m_dockedBorders & DOCK_RIGHT)
   {
      if (!(m_dockedBorders & DOCK_LEFT))
         pos.left -= pos.right - deskRect.right;
      pos.right = deskRect.right;
      res = true;
   }
   if (m_dockedBorders & DOCK_TOP)
   {
      pos.bottom -= pos.top - deskRect.top;
      pos.top = deskRect.top;
      res = true;
   }
   if (m_dockedBorders & DOCK_BOTTOM)
   {
      if (!(m_dockedBorders & DOCK_TOP))
         pos.top -= pos.bottom - deskRect.bottom;
      pos.bottom = deskRect.bottom;
      res = true;
   }
   return res;
}

LRESULT VirtualDimension::OnDisplayChange(HWND /*hWnd*/, UINT /*message*/, WPARAM /*wParam*/, LPARAM /*lParam*/)
{
   RECT  pos;

   if (mousewarp)
      mousewarp->RefreshDesktopSize();

   GetWindowRect(m_hWnd, &pos);
   if (DockWindow(pos))
   {
      if (!m_shrinked && m_autoHideDelay > 0)
         SetTimer(m_autoHideTimerId, m_autoHideDelay); //reset the timer, to avoid hiding the window during the resolution change, as it does not look very nice
      MoveWindow(m_hWnd, pos.left, pos.top, pos.right-pos.left, pos.bottom-pos.top, TRUE);
   }
   return 0;
}

LRESULT VirtualDimension::OnDpiChanged(HWND /*hWnd*/, UINT /*message*/, WPARAM /*wParam*/, LPARAM lParam)
{
   RECT * suggested = (RECT *)lParam;

   UpdateIconSize();
   deskMan->UpdatePreviewWindowFont();

   SetWindowPos(m_hWnd, NULL, suggested->left, suggested->top,
                suggested->right - suggested->left, suggested->bottom - suggested->top,
                SWP_NOZORDER | SWP_NOACTIVATE);

   deskMan->UpdateLayout();
   return 0;
}

LRESULT VirtualDimension::OnSettingChange(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
   winMan->OnSettingsChange(hWnd, message, wParam, lParam);
   deskMan->OnSettingsChange(hWnd, message, wParam, lParam);

   if (wParam == SPI_SETWORKAREA)
      OnDisplayChange(hWnd, message, 0, 0);

   return 0;
}

LRESULT VirtualDimension::OnShowWindow(HWND /*hWnd*/, UINT /*message*/, WPARAM wParam, LPARAM /*lParam*/)
{
   m_isWndVisible = (wParam != FALSE);
   return 0;
}

LRESULT VirtualDimension::OnTimer(HWND /*hWnd*/, UINT /*message*/, WPARAM /*wParam*/, LPARAM /*lParam*/)
{
   POINT pt;

   //Do not shrink and let the timer run if the mouse is over the window -> it will be hidden later, when
   //mouse is not on window anymore.
   GetCursorPos(&pt);
   if (!IsPointInWindow(pt) && GetWindowThreadProcessId(GetForegroundWindow(),NULL) != GetCurrentThreadId())
   {
      KillTimer(m_autoHideTimerId); //already auto-hidden -> do not need to
      Shrink();
   }

   return 0;
}

LRESULT VirtualDimension::OnActivateApp(HWND /*hWnd*/, UINT /*message*/, WPARAM wParam, LPARAM lParam)
{
   if (wParam == TRUE)
      KillTimer(m_autoHideTimerId);                   //Kill auto-hide timer if activated
   else if (m_autoHideDelay > 0 && ((DWORD)lParam != GetCurrentThreadId()))
      SetTimer(m_autoHideTimerId, m_autoHideDelay);   //Re-start auto-hide timer if de-activated
   return 0;
}

LRESULT VirtualDimension::OnPaint(HWND hWnd, UINT /*message*/, WPARAM /*wParam*/, LPARAM /*lParam*/)
{
   //This method is used to paint the shrinked window
   PAINTSTRUCT ps;
   RECT rect;
   HDC hdc;

   GetClientRect(hWnd, &rect);
   hdc = BeginPaint(hWnd, &ps);

   Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);

   EndPaint(hWnd, &ps);
   return 0;
}

LRESULT VirtualDimension::OnSize(HWND /*hWnd*/, UINT /*message*/, WPARAM wParam, LPARAM lParam)
{
   //Follow every size change (including while the border is being dragged)
   if ((!m_shrinked) && (wParam != SIZE_MINIMIZED))
   {
      deskMan->ReSize(LOWORD(lParam), HIWORD(lParam));
      Refresh();
   }

   return 0;
}

LRESULT VirtualDimension::OnEraseBackground(HWND /*hWnd*/, UINT /*message*/, WPARAM /*wParam*/, LPARAM /*lParam*/)
{
   //The whole client area is painted by WM_PAINT (double buffered): erasing it first would only flicker
   return TRUE;
}

LRESULT VirtualDimension::OnMouseHover(HWND /*hWnd*/, UINT /*message*/, WPARAM /*wParam*/, LPARAM /*lParam*/)
{
   //Un-shrink the window
   if (m_shrinked)
      UnShrink();

   m_tracking = false;
   return 0;
}

LRESULT VirtualDimension::OnMouseLeave(HWND /*hWnd*/, UINT /*message*/, WPARAM /*wParam*/, LPARAM /*lParam*/)
{
   //Set timer to auto-hide
   if (!m_shrinked && m_autoHideDelay > 0)
      SetTimer(m_autoHideTimerId, m_autoHideDelay);

   m_tracking = false;
   return 0;
}

LRESULT VirtualDimension::OnNCHitTest(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
   //Stop auto-hide timer (re-entry in the window)
   KillTimer(m_autoHideTimerId);

   //Track mouse hover/leave, if not already doing so
   if (!m_tracking)
   {
      //Setup mouse tracking
      TRACKMOUSEEVENT tme;

      tme.cbSize = sizeof(TRACKMOUSEEVENT);
      tme.dwFlags = TME_HOVER | TME_LEAVE;
      tme.dwHoverTime = 1000;
      tme.hwndTrack = m_hWnd;
      m_tracking = TrackMouseEvent(&tme) ? true : false;
   }

   return DefWindowProc(hWnd, message, wParam, lParam);
}

#define SHRUNK_THICKNESS 10

void VirtualDimension::Shrink(void)
{
   RECT pos, deskRect;
   LONG_PTR style;
   int thickness;

   if (m_shrinked || !m_dockedBorders)
      return;

   m_shrinked = true;

   //Compute the position where to display the handle
   deskRect = PlatformHelper::GetWorkArea(m_hWnd);
   GetWindowRect(m_hWnd, &pos);
   thickness = PlatformHelper::ScaleForWindow(m_hWnd, SHRUNK_THICKNESS);

   switch(m_dockedBorders & (DOCK_LEFT|DOCK_RIGHT))
   {
   case DOCK_LEFT:
      pos.right = deskRect.left + thickness;
      pos.left = pos.right - thickness;
      break;

   case DOCK_RIGHT:
      pos.left = deskRect.right - thickness;
      pos.right = pos.left + thickness;
      break;

   case DOCK_LEFT|DOCK_RIGHT:
      pos.left = deskRect.left;
      pos.right = deskRect.right;
      break;

   default:
      break;
   }

   switch(m_dockedBorders & (DOCK_TOP|DOCK_BOTTOM))
   {
   case DOCK_TOP:
      pos.bottom = deskRect.top + thickness;
      pos.top = pos.bottom - thickness;
      break;

   case DOCK_BOTTOM:
      pos.top = deskRect.bottom - thickness;
      pos.bottom = pos.top + thickness;
      break;

   case DOCK_TOP|DOCK_BOTTOM:
      pos.top = deskRect.top;
      pos.bottom = deskRect.bottom;
      break;

   default:
      break;
   }

   //Change the method to use for painting the window
   SetMessageHandler(WM_PAINT, this, &VirtualDimension::OnPaint);

   //Change the style of the window
   style = GetWindowLongPtr(m_hWnd, GWL_STYLE);
   style &= ~WS_CAPTION;
   style &= ~WS_DLGFRAME;
   style &= ~WS_BORDER;
   style &= ~WS_THICKFRAME;
   SetWindowLongPtr(m_hWnd, GWL_STYLE, style);

   RemoveMenu(m_pSysMenu, SC_MOVE, MF_BYCOMMAND);
   RemoveMenu(m_pSysMenu, SC_SIZE, MF_BYCOMMAND);

   //Apply the changes
   SetWindowPos(m_hWnd, NULL, pos.left, pos.top, pos.right-pos.left, pos.bottom-pos.top, SWP_NOZORDER | SWP_FRAMECHANGED);
   RedrawWindow(m_hWnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);

   //Disable tooltips
   tooltip->ShowTooltips(false);

   //Refresh the display
   Refresh();
}

void VirtualDimension::UnShrink(void)
{
   RECT pos;
   LONG_PTR style;

   if (!m_shrinked)
      return;

   //Change the method to use for painting the window
   SetMessageHandler(WM_PAINT, deskMan, &DesktopManager::OnPaint);

   //Restore the window's style
   style = GetWindowLongPtr(m_hWnd, GWL_STYLE);
   style |= (m_hasCaption ? WS_CAPTION : WS_DLGFRAME);
   style |= (m_lockPreviewWindow ? 0 : WS_THICKFRAME);
   SetWindowLongPtr(m_hWnd, GWL_STYLE, style);

   if (!m_lockPreviewWindow)
   {
      InsertMenu(m_pSysMenu, 0, MF_BYPOSITION, SC_SIZE, Locale::GetInstance().GetString(IDS_MENU_SIZE));
      InsertMenu(m_pSysMenu, 0, MF_BYPOSITION, SC_MOVE, Locale::GetInstance().GetString(IDS_MENU_MOVE));
   }

   //Restore the windows position
   pos.left = m_location.x;
   pos.right = pos.left + deskMan->GetWindowWidth();
   pos.top = m_location.y;
   pos.bottom = pos.top + deskMan->GetWindowHeight();
   AdjustWindowRectExForDpi(&pos, (DWORD)GetWindowLongPtr(m_hWnd, GWL_STYLE), FALSE,
                            (DWORD)GetWindowLongPtr(m_hWnd, GWL_EXSTYLE), GetDpiForWindow(m_hWnd));

   //Apply the changes
   SetWindowPos(m_hWnd, NULL, pos.left, pos.top, pos.right-pos.left, pos.bottom-pos.top, SWP_DRAWFRAME | SWP_NOZORDER | SWP_FRAMECHANGED);
   RedrawWindow(m_hWnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);

   //Enable tooltips
   tooltip->ShowTooltips(true);

   //Refresh the display
   Refresh();

   m_shrinked = false;
}

bool VirtualDimension::IsPointInWindow(POINT pt)
{
   RECT rect;
   GetWindowRect(vdWindow, &rect);
   return PtInRect(&rect, pt) ? true : false;
}

/** Get a string from the version information of the executable. */
static String GetVersionString(LPCWSTR name)
{
   wchar_t path[MAX_PATH];
   DWORD dwHandle;
   DWORD size;
   String res;

   GetModuleFileNameW(NULL, path, MAX_PATH);
   size = GetFileVersionInfoSizeW(path, &dwHandle);
   if (size)
   {
      std::vector<BYTE> data(size);
      LPWSTR value;
      UINT length;
      wchar_t query[128];

      swprintf_s(query, L"\\StringFileInfo\\040904b0\\%s", name);
      if (GetFileVersionInfoW(path, 0, size, data.data()) &&
          VerQueryValueW(data.data(), query, (LPVOID*)&value, &length) && length > 0)
         res.assign(value, wcsnlen(value, length));
   }

   return res;
}

// Message handler for about box.
INT_PTR CALLBACK VirtualDimension::About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
   static HBITMAP picture;

   switch (message)
   {
   case WM_INITDIALOG:
      {
         SetFocus(GetDlgItem(hDlg, IDOK));
         picture = PlatformHelper::LoadImageResource(MAKEINTRESOURCE(IDR_LOGO), MAKEINTRESOURCE(300));

         String text = GetVersionString(L"ProductName") + L" v" + GetVersionString(L"ProductVersion");
         SetDlgItemText(hDlg, IDC_PRODUCT, text.c_str());
         SetDlgItemText(hDlg, IDC_COPYRIGHT, GetVersionString(L"LegalCopyright").c_str());
      }
      return FALSE;

   case WM_COMMAND:
      switch(LOWORD(wParam))
      {
      case IDOK:
      case IDCANCEL:
         EndDialog(hDlg, LOWORD(wParam));
         if (picture)
         {
            DeleteObject(picture);
            picture = NULL;
         }
         return TRUE;

      case IDC_HOMEPAGE_LINK:
         if (HIWORD(wParam) == STN_CLICKED)
         {
            ShellExecute(hDlg, L"open", L"http://virt-dimension.sourceforge.net",
                         NULL, NULL, SW_SHOWNORMAL);
         }
         break;

      case IDC_GPL_LINK:
         if (HIWORD(wParam) == STN_CLICKED)
         {
            ShellExecute(hDlg, L"open", L"https://www.gnu.org/licenses/old-licenses/gpl-2.0.html",
                         NULL, NULL, SW_SHOWNORMAL);
         }
         break;
      }
      break;

   case WM_DRAWITEM:
      {
         LPDRAWITEMSTRUCT lpDrawItem = (LPDRAWITEMSTRUCT)lParam;
         FillRect(lpDrawItem->hDC, &lpDrawItem->rcItem, GetSysColorBrush(COLOR_BTNFACE));
         if (picture)
            PlatformHelper::DrawBitmap(lpDrawItem->hDC, picture, lpDrawItem->rcItem, false);
      }
      return TRUE;
   }
   return FALSE;
}
