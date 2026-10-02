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

#include "stdafx.h"
#include "MouseWarp.h"
#include "VirtualDimension.h"
#include "DesktopManager.h"
#include "Settings.h"
#include "PlatformHelper.h"
#include "Locale.h"

/** Polling interval for mouse position check. */
#define MOUSE_WARP_DELAY_CHECK   50

MouseWarp * mousewarp;

MouseWarp::MouseWarp(): m_warpLocation(WARP_NONE), m_checkLocation(WARP_NONE), m_duration(0)
{
   Settings settings;

   //Load settings
   m_enableWarp = settings.LoadSetting(Settings::WarpEnable);
   m_sensibility = settings.LoadSetting(Settings::WarpSensibility);
   m_minDuration = settings.LoadSetting(Settings::WarpMinDuration);
   m_reWarpDelay = settings.LoadSetting(Settings::WarpRewarpDelay);
   m_warpVKey = settings.LoadSetting(Settings::WarpRequiredVKey);
   m_invertMousePos = settings.LoadSetting(Settings::WarpInvertMousePos);

   //Compute size of center rect
   RefreshDesktopSize();

   //Create the timers
   m_warpTimerId = vdWindow.CreateTimer(this, &MouseWarp::OnWarpTimer);
   m_checkTimerId = vdWindow.CreateTimer(this, &MouseWarp::OnCheckTimer);
   if (m_enableWarp)
      vdWindow.SetTimer(m_checkTimerId, MOUSE_WARP_DELAY_CHECK);
}

MouseWarp::~MouseWarp(void)
{
   Settings settings;

   vdWindow.DestroyTimer(m_checkTimerId);
   vdWindow.DestroyTimer(m_warpTimerId);

   //Save settings
   settings.SaveSetting(Settings::WarpEnable, m_enableWarp);
   settings.SaveSetting(Settings::WarpSensibility, m_sensibility);
   settings.SaveSetting(Settings::WarpMinDuration, m_minDuration);
   settings.SaveSetting(Settings::WarpRewarpDelay, m_reWarpDelay);
   settings.SaveSetting(Settings::WarpRequiredVKey, m_warpVKey);
   settings.SaveSetting(Settings::WarpInvertMousePos, m_invertMousePos);
}

/** Mouse position check.
 * Checks the mouse position periodically (when warp is enabled), to detect if it
 * is near the screen border.
 */
LRESULT MouseWarp::OnCheckTimer(HWND /*hWnd*/, UINT /*message*/, WPARAM /*wParam*/, LPARAM /*lParam*/)
{
   POINT pt;
   WarpLocation newWarpLoc;

   //Compute new warp location
   GetCursorPos(&pt);
   if (vdWindow.IsPointInWindow(pt) ||
       (m_warpVKey != 0 && (GetAsyncKeyState(m_warpVKey)&0x8000) == 0))
      newWarpLoc = WARP_NONE;   //ignore the mouse if it is over the preview window OR if the enabling key is not pressed
   else if (pt.x < m_centerRect.left)
      newWarpLoc = WARP_LEFT;
   else if (pt.x > m_centerRect.right)
      newWarpLoc = WARP_RIGHT;
   else if (pt.y < m_centerRect.top)
      newWarpLoc = WARP_TOP;
   else if (pt.y > m_centerRect.bottom)
      newWarpLoc = WARP_BOTTOM;
   else
      newWarpLoc = WARP_NONE;

   //Notify if warp location has changed, or if it has not changed for some time
   if (newWarpLoc != m_checkLocation || (m_reWarpDelay != 0 && m_duration > m_reWarpDelay))
   {
      m_duration = 0;
      m_checkLocation = newWarpLoc;
      OnMouseWarp(m_checkLocation);

      if (m_invertMousePos && m_checkLocation != WARP_NONE)
      {
         switch(m_checkLocation)
         {
         case WARP_NONE:      break;   //nothing to do
         case WARP_LEFT:      pt.x = m_centerRect.right + m_centerRect.left - pt.x;   m_checkLocation = WARP_RIGHT;   break;
         case WARP_RIGHT:     pt.x = m_centerRect.left + m_centerRect.right - pt.x;   m_checkLocation = WARP_LEFT;    break;
         case WARP_TOP:       pt.y = m_centerRect.top + m_centerRect.bottom - pt.y;   m_checkLocation = WARP_BOTTOM;  break;
         case WARP_BOTTOM:    pt.y = m_centerRect.top + m_centerRect.bottom - pt.y;   m_checkLocation = WARP_TOP;     break;
         }
         SetCursorPos(pt.x, pt.y);
      }
   }
   else if (m_checkLocation != WARP_NONE && m_reWarpDelay != 0)   //do not generate multiple WARP_NONE events
      m_duration += MOUSE_WARP_DELAY_CHECK;

   return 0;
}

LRESULT MouseWarp::OnWarpTimer(HWND /*hWnd*/, UINT /*message*/, WPARAM /*wParam*/, LPARAM /*lParam*/)
{
   Desktop * desk;

   vdWindow.KillTimer(m_warpTimerId);

   switch(m_warpLocation)
   {
   case WARP_LEFT:   desk = deskMan->GetDeskOnLeft(deskMan->GetCurrentDesktop()); break;
   case WARP_RIGHT:  desk = deskMan->GetDeskOnRight(deskMan->GetCurrentDesktop()); break;
   case WARP_TOP:    desk = deskMan->GetDeskAbove(deskMan->GetCurrentDesktop()); break;
   case WARP_BOTTOM: desk = deskMan->GetDeskBelow(deskMan->GetCurrentDesktop()); break;
   default:          desk = NULL; break;
   }

   if (desk)
      deskMan->SwitchToDesktop(desk);

   return TRUE;
}

/** The mouse reached (or left) a border of the screen. */
void MouseWarp::OnMouseWarp(WarpLocation location)
{
   m_warpLocation = location;

   if (m_warpLocation == WARP_NONE)
      vdWindow.KillTimer(m_warpTimerId);
   else
      vdWindow.SetTimer(m_warpTimerId, std::max(m_minDuration, (DWORD)USER_TIMER_MINIMUM));
}

void MouseWarp::EnableWarp(bool enable)
{
   //Check if already in the correct state
   if (enable == m_enableWarp)
      return;

   m_enableWarp = enable;

   if (m_enableWarp)
   {
      m_checkLocation = WARP_NONE;
      m_duration = 0;
      vdWindow.SetTimer(m_checkTimerId, MOUSE_WARP_DELAY_CHECK);
   }
   else
   {
      vdWindow.KillTimer(m_checkTimerId);
      vdWindow.KillTimer(m_warpTimerId);
   }
}

void MouseWarp::SetSensibility(LONG sensibility)
{
   if (sensibility != m_sensibility)
   {
      //Update sensibility
      m_sensibility = sensibility;

      //Refresh center rect
      RefreshDesktopSize();
   }
}

void MouseWarp::SetMinDuration(DWORD minDuration)
{
   m_minDuration = minDuration;
}

void MouseWarp::SetRewarpDelay(DWORD rewarpDelay)
{
   m_reWarpDelay = rewarpDelay;
}

void MouseWarp::InvertMousePos(bool invert)
{
   m_invertMousePos = invert;
}

void MouseWarp::SetWarpKey(int vkey)
{
   m_warpVKey = vkey;
}

void MouseWarp::RefreshDesktopSize()
{
   //The borders are the ones of the whole (multi-monitor) screen
   m_centerRect = PlatformHelper::GetVirtualScreen();
   m_centerRect.left += m_sensibility;
   m_centerRect.right -= m_sensibility + 1;
   m_centerRect.top += m_sensibility;
   m_centerRect.bottom -= m_sensibility + 1;
}

INT_PTR CALLBACK MouseWarp::PropertiesDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
   MouseWarp * self;
   HWND hWnd;

   switch(message)
   {
   case WM_INITDIALOG:
      SetWindowLongPtr(hDlg, DWLP_USER, lParam);
      self = (MouseWarp*)lParam;

      //Warp config
      CheckDlgButton(hDlg, IDC_MOUSEWARP_CHECK, self->IsWarpEnabled()?BST_CHECKED:BST_UNCHECKED);
      SetDlgItemInt(hDlg, IDC_WARPSENSIBILITY_EDIT, self->m_sensibility, FALSE);
      SetDlgItemInt(hDlg, IDC_MINDURATION_EDIT, self->m_minDuration, FALSE);
      SetDlgItemInt(hDlg, IDC_REWARPDELAY_EDIT, self->m_reWarpDelay, FALSE);
      CheckDlgButton(hDlg, IDC_SWAPMOUSE_CHECK, self->m_invertMousePos?BST_CHECKED:BST_UNCHECKED);

      //Warp key modes
      switch(self->m_warpVKey)
      {
      case 0: CheckDlgButton(hDlg, IDC_WARPKEY_NONE_RADIO, BST_CHECKED); break;
      case VK_MENU: CheckDlgButton(hDlg, IDC_WARPKEY_ALT_RADIO, BST_CHECKED); break;
      case VK_SHIFT: CheckDlgButton(hDlg, IDC_WARPKEY_SHIFT_RADIO, BST_CHECKED); break;
      case VK_CONTROL: CheckDlgButton(hDlg, IDC_WARPKEY_CTRL_RADIO, BST_CHECKED); break;
      case VK_LWIN: CheckDlgButton(hDlg, IDC_WARPKEY_WIN_RADIO, BST_CHECKED); break;
      default: CheckDlgButton(hDlg, IDC_WARPKEY_OTHER_RADIO, BST_CHECKED); break;
      }

      //Custom warp key
      hWnd = GetDlgItem(hDlg, IDC_CUSTOMKEY_EDIT);
      SendMessage(hWnd, HKM_SETRULES, HKCOMB_A|HKCOMB_C|HKCOMB_CA|HKCOMB_S|HKCOMB_SA|HKCOMB_SC|HKCOMB_SCA, 0);
      if (IsDlgButtonChecked(hDlg, IDC_WARPKEY_OTHER_RADIO))
      {
         EnableWindow(hWnd, TRUE);
         SendMessage(hWnd, HKM_SETHOTKEY, self->m_warpVKey, 0);
      }
      else
         EnableWindow(hWnd, FALSE);

      return TRUE;
      break;

   case WM_COMMAND:
      switch(LOWORD(wParam))
      {
      case IDOK:
         self = (MouseWarp*)GetWindowLongPtr(hDlg, DWLP_USER);

         self->SetSensibility(GetDlgItemInt(hDlg, IDC_WARPSENSIBILITY_EDIT, NULL, FALSE));
         self->SetMinDuration(GetDlgItemInt(hDlg, IDC_MINDURATION_EDIT, NULL, FALSE));
         self->SetRewarpDelay(GetDlgItemInt(hDlg, IDC_REWARPDELAY_EDIT, NULL, FALSE));
         self->InvertMousePos(IsDlgButtonChecked(hDlg, IDC_SWAPMOUSE_CHECK) ? true : false);
         self->EnableWarp(IsDlgButtonChecked(hDlg, IDC_MOUSEWARP_CHECK) ? true : false);

         if (IsDlgButtonChecked(hDlg, IDC_WARPKEY_ALT_RADIO))
            self->SetWarpKey(VK_MENU);
         else if (IsDlgButtonChecked(hDlg, IDC_WARPKEY_SHIFT_RADIO))
            self->SetWarpKey(VK_SHIFT);
         else if (IsDlgButtonChecked(hDlg, IDC_WARPKEY_CTRL_RADIO))
            self->SetWarpKey(VK_CONTROL);
         else if (IsDlgButtonChecked(hDlg, IDC_WARPKEY_WIN_RADIO))
            self->SetWarpKey(VK_LWIN);
         else if (IsDlgButtonChecked(hDlg, IDC_WARPKEY_OTHER_RADIO))
            self->SetWarpKey(LOBYTE(SendMessage(GetDlgItem(hDlg, IDC_CUSTOMKEY_EDIT), HKM_GETHOTKEY, 0, 0)));
         else
            self->SetWarpKey(0);

         EndDialog(hDlg, IDOK);
         break;

      case IDCANCEL:
         EndDialog(hDlg, IDCANCEL);
         break;

      case IDC_WARPKEY_NONE_RADIO:
      case IDC_WARPKEY_ALT_RADIO:
      case IDC_WARPKEY_SHIFT_RADIO:
      case IDC_WARPKEY_CTRL_RADIO:
      case IDC_WARPKEY_WIN_RADIO:
         EnableWindow(GetDlgItem(hDlg, IDC_CUSTOMKEY_EDIT), FALSE);
         break;

      case IDC_WARPKEY_OTHER_RADIO:
         EnableWindow(GetDlgItem(hDlg, IDC_CUSTOMKEY_EDIT), TRUE);
         break;
      }
      break;
   }

   return FALSE;
}

void MouseWarp::Configure(HWND hParentWnd)
{
   DialogBoxParam(Locale::GetInstance(), MAKEINTRESOURCE(IDD_MOUSEWARP_SETTINGS), hParentWnd, &PropertiesDlgProc, (LPARAM)this);
}
