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

#ifndef __EXPLORERWRAPPER_H__
#define __EXPLORERWRAPPER_H__

#include <shobjidl.h>
#include "FastWindow.h"

/** Interface with the Windows shell (explorer).
 * Takes care of re-binding to the shell when explorer is restarted.
 */
class ExplorerWrapper
{
public:
   ExplorerWrapper(FastWindow * wnd);
   ~ExplorerWrapper(void);

   /** Add a button for the window in the taskbar. */
   void ShowWindowInTaskbar(HWND hWnd)
   {
      if (m_tasklist)
         m_tasklist->AddTab(hWnd);
   }

   /** Remove the button of the window from the taskbar. */
   void HideWindowInTaskbar(HWND hWnd)
   {
      if (m_tasklist)
         m_tasklist->DeleteTab(hWnd);
   }

protected:
   LRESULT OnTaskbarRestart(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
   void BindTaskbar();
   void ReleaseTaskbar();

   /** Pointer to the COM taskbar interface.
    * This interface is used to add/remove the icons from the taskbar, when showing/hiding
    */
   ITaskbarList* m_tasklist;
};

extern ExplorerWrapper * explorerWrapper;

#endif /*__EXPLORERWRAPPER_H__*/
