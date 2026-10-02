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
#include "HidingMethod.h"
#include "ExplorerWrapper.h"
#include "WindowsManager.h"
#include "PlatformHelper.h"

// Window properties used to recover the windows after a crash
static const wchar_t PROP_EXSTYLE[] = L"VirtualDimension.ExStyle";
static const wchar_t PROP_MOVED[] = L"VirtualDimension.Moved";
static const wchar_t PROP_POSX[] = L"VirtualDimension.PosX";
static const wchar_t PROP_POSY[] = L"VirtualDimension.PosY";

static const LONG_PTR EXSTYLE_SAVED_FLAG = (LONG_PTR)1 << 40;   //so that a null style can be stored

void HidingMethod::SaveExStyle(HWND hWnd, LONG_PTR exStyle)
{
   SetPropW(hWnd, PROP_EXSTYLE, (HANDLE)((exStyle & 0xFFFFFFFF) | EXSTYLE_SAVED_FLAG));
}

bool HidingMethod::LoadExStyle(HWND hWnd, LONG_PTR * exStyle)
{
   LONG_PTR data = (LONG_PTR)GetPropW(hWnd, PROP_EXSTYLE);
   if (!(data & EXSTYLE_SAVED_FLAG))
      return false;
   *exStyle = data & 0xFFFFFFFF;
   return true;
}

void HidingMethod::ClearExStyle(HWND hWnd)
{
   RemovePropW(hWnd, PROP_EXSTYLE);
}

void HidingMethod::RemoveFromTaskList(Window * wnd)
{
   HWND hWnd = *wnd;
   LONG_PTR exStyle = GetWindowLongPtr(hWnd, GWL_EXSTYLE);

   //Remove the taskbar button
   explorerWrapper->HideWindowInTaskbar(hWnd);

   //Changing the style of the window so that it does not appear in the alt-tab list
   //requires the application to process messages: skip it if it does not respond.
   if (!winMan->IsShowAllWindowsInTaskList() && wnd->IsResponsive())
   {
      SaveExStyle(hWnd, exStyle);
      SetWindowLongPtr(hWnd, GWL_EXSTYLE, (exStyle & ~WS_EX_APPWINDOW) | WS_EX_TOOLWINDOW);
      wnd->SetWindowPos(hWnd, NULL, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
   }
}

void HidingMethod::RestoreToTaskList(Window * wnd)
{
   HWND hWnd = *wnd;
   LONG_PTR exStyle;

   //Restore the window's style
   if (LoadExStyle(hWnd, &exStyle))
   {
      if (exStyle != GetWindowLongPtr(hWnd, GWL_EXSTYLE))
      {
         SetWindowLongPtr(hWnd, GWL_EXSTYLE, exStyle);
         wnd->SetWindowPos(hWnd, NULL, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
      }
      ClearExStyle(hWnd);
   }

   //Show the taskbar button
   explorerWrapper->ShowWindowInTaskbar(hWnd);
}


void HidingMethodHide::Recover(Window * wnd)
{
   //The window may have been hidden by a previous instance of Virtual Dimension
   if (!::IsWindowVisible(*wnd))
   {
      wnd->m_hiddenIconic = ::IsIconic(*wnd) ? true : false;
      Show(wnd);
   }
}

void HidingMethodHide::Show(Window * wnd)
{
   wnd->SetWindowPos(*wnd, NULL, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
   if (!wnd->m_hiddenIconic)
      wnd->ShowOwnedPopups();
}

void HidingMethodHide::Hide(Window * wnd)
{
   wnd->m_hiddenIconic = ::IsIconic(*wnd) ? true : false;

   //Hide the window first, else hiding the owned windows may activate it
   wnd->SetWindowPos(*wnd, NULL, 0, 0, 0, 0, SWP_HIDEWINDOW | SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
   if (!wnd->m_hiddenIconic)
      wnd->HideOwnedPopups();
}


void HidingMethodMinimize::Recover(Window * wnd)
{
   LONG_PTR exStyle;

   //The window may have been hidden by a previous instance of Virtual Dimension:
   //put it back in the task list, but leave it minimized.
   if (LoadExStyle(*wnd, &exStyle))
   {
      wnd->m_hiddenIconic = true;
      RestoreToTaskList(wnd);
   }
}

void HidingMethodMinimize::Show(Window * wnd)
{
   RestoreToTaskList(wnd);

   //Restore the application if needed
   if (!wnd->m_hiddenIconic)
   {
      wnd->ShowWindowCmd(*wnd, SW_SHOWNOACTIVATE);
      wnd->SetWindowPos(*wnd, winMan->GetPrevWindow(wnd), 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
   }
}

void HidingMethodMinimize::Hide(Window * wnd)
{
   //Minimize the application
   wnd->m_hiddenIconic = ::IsIconic(*wnd) ? true : false;
   if (!wnd->m_hiddenIconic)
      wnd->ShowWindowCmd(*wnd, SW_SHOWMINNOACTIVE);

   RemoveFromTaskList(wnd);
}


void HidingMethodMove::Recover(Window * wnd)
{
   HWND hWnd = *wnd;

   //The window may have been moved away by a previous instance of Virtual Dimension
   if (GetPropW(hWnd, PROP_MOVED))
   {
      wnd->m_hiddenIconic = ::IsIconic(hWnd) ? true : false;
      wnd->m_hiddenPos.x = (LONG)(INT_PTR)GetPropW(hWnd, PROP_POSX);
      wnd->m_hiddenPos.y = (LONG)(INT_PTR)GetPropW(hWnd, PROP_POSY);
      Show(wnd);
   }
}

void HidingMethodMove::Show(Window * wnd)
{
   HWND hWnd = *wnd;

   RestoreToTaskList(wnd);

   //Bring back to visible area
   if (!wnd->m_hiddenIconic)
      wnd->SetWindowPos(hWnd, NULL, wnd->m_hiddenPos.x, wnd->m_hiddenPos.y, 0, 0,
                        SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);

   RemovePropW(hWnd, PROP_MOVED);
   RemovePropW(hWnd, PROP_POSX);
   RemovePropW(hWnd, PROP_POSY);
}

void HidingMethodMove::Hide(Window * wnd)
{
   HWND hWnd = *wnd;
   RECT rect;

   wnd->m_hiddenIconic = ::IsIconic(hWnd) ? true : false;

   //Move the window off the visible area (to the right of all monitors)
   if (!wnd->m_hiddenIconic && GetWindowRect(hWnd, &rect))
   {
      RECT screen = PlatformHelper::GetVirtualScreen();

      wnd->m_hiddenPos.x = rect.left;
      wnd->m_hiddenPos.y = rect.top;
      SetPropW(hWnd, PROP_MOVED, (HANDLE)1);
      SetPropW(hWnd, PROP_POSX, (HANDLE)(INT_PTR)rect.left);
      SetPropW(hWnd, PROP_POSY, (HANDLE)(INT_PTR)rect.top);

      wnd->SetWindowPos(hWnd, NULL, screen.right + 100, rect.top, 0, 0,
                        SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
   }

   //This removes window from taskbar and alt+tab list
   RemoveFromTaskList(wnd);
}
