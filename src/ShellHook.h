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

#ifndef __SHELLHOOK_H__
#define __SHELLHOOK_H__

/** Registration of a window to receive shell hook notifications.
 * The notifications are sent to the window as the registered "SHELLHOOK" message.
 */
class ShellHook
{
public:
   ShellHook(HWND hWnd);
   ~ShellHook(void);

   static UINT GetMessageId()   { return RegisterWindowMessageW(L"SHELLHOOK"); }

   enum ShellNotifications {
      WINDOWCREATED = HSHELL_WINDOWCREATED,
      WINDOWDESTROYED = HSHELL_WINDOWDESTROYED,
      ACTIVATESHELLWINDOW = HSHELL_ACTIVATESHELLWINDOW,
      WINDOWACTIVATED = HSHELL_WINDOWACTIVATED,
      GETMINRECT = HSHELL_GETMINRECT,
      REDRAW = HSHELL_REDRAW,
      TASKMAN = HSHELL_TASKMAN,
      LANGUAGE = HSHELL_LANGUAGE,
      SYSMENU = HSHELL_SYSMENU,
      ENDTASK = HSHELL_ENDTASK,
      ACCESSIBILITYSTATE = HSHELL_ACCESSIBILITYSTATE,
      APPCOMMAND = HSHELL_APPCOMMAND,
      WINDOWREPLACED = HSHELL_WINDOWREPLACED,
      WINDOWREPLACING = HSHELL_WINDOWREPLACING,
      MONITORCHANGED = HSHELL_MONITORCHANGED,
      FLASH = HSHELL_FLASH,
      RUDEAPPACTIVATED = HSHELL_RUDEAPPACTIVATED
   };

protected:
   HWND m_hWnd;
   bool m_registered;
};

#endif /*__SHELLHOOK_H__*/
