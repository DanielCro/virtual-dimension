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

#ifndef __MESSAGES_H__
#define __MESSAGES_H__

/** Command identifiers of the window context menu.
 * All of them are >= WM_USER, which is how the menu handlers tell them apart
 * from the main window commands.
 */
enum MenuItems
{
   VDM_TOGGLEONTOP = WM_USER+1,
   VDM_TOGGLEMINIMIZETOTRAY,
   VDM_TOGGLETRANSPARENCY,

   VDM_TOGGLEALLDESKTOPS,
   VDM_MOVEWINDOW,

   VDM_ACTIVATEWINDOW,
   VDM_RESTORE,
   VDM_MINIMIZE,
   VDM_MAXIMIZE,
   VDM_MAXIMIZEHEIGHT,
   VDM_MAXIMIZEWIDTH,
   VDM_CLOSE,
   VDM_KILL,

   VDM_PROPERTIES,
};

/** Private messages of the main Virtual Dimension window.
 * Some of them are also posted by a second instance of the program, to forward
 * its command line to the running instance.
 */
enum VirtualDimensionMessages
{
   WM_VD_MOUSEWARP = WM_APP + 103,

   WM_VD_STARTONDESKTOP = WM_APP + 105,   /* an application should start on the specified desktop. wParam = processId lParam = deskopIdx */
   WM_VD_SWITCHDESKTOP,                   /* switch to some desktop. lParam = desktopIdx */
};

/** WM_COPYDATA request, sent by a second instance of the program: the windows of
 * a program which has just been started should be displayed on some desktop.
 */
#define VD_COPYDATA_STARTONDESKTOP  0x56440001

struct StartOnDesktopRequest
{
   DWORD processId;           ///< Process which has been started
   int desktop;               ///< Index of the desktop
   wchar_t program[MAX_PATH]; ///< File name of the program (packaged applications run in another process)
};

#endif /*__MESSAGES_H__*/
