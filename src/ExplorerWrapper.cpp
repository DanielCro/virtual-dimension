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
#include "ExplorerWrapper.h"

#include "TrayIconsManager.h"
#include "DesktopManager.h"

ExplorerWrapper * explorerWrapper;

ExplorerWrapper::ExplorerWrapper(FastWindow * wnd): m_tasklist(NULL)
{
   UINT uTaskbarRestart = RegisterWindowMessageW(L"TaskbarCreated");
   wnd->SetMessageHandler(uTaskbarRestart, this, &ExplorerWrapper::OnTaskbarRestart);

   //Allow the message even if explorer runs with a different integrity level than us
   ChangeWindowMessageFilterEx(*wnd, uTaskbarRestart, MSGFLT_ALLOW, NULL);

   BindTaskbar();
}

ExplorerWrapper::~ExplorerWrapper(void)
{
   ReleaseTaskbar();
}

void ExplorerWrapper::BindTaskbar()
{
   if (CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_tasklist)) == S_OK)
   {
      if (FAILED(m_tasklist->HrInit()))
         ReleaseTaskbar();
   }
   else
      m_tasklist = NULL;
}

void ExplorerWrapper::ReleaseTaskbar()
{
   if (m_tasklist)
      m_tasklist->Release();
   m_tasklist = NULL;
}

LRESULT ExplorerWrapper::OnTaskbarRestart(HWND /*hWnd*/, UINT /*msg*/, WPARAM /*wParam*/, LPARAM /*lParam*/)
{
   //Refresh tray icons
   trayManager->RefreshIcons();

   //Ensure we got the correct interface for showing/hiding icons from the taskbar
   ReleaseTaskbar();
   BindTaskbar();

   //Restore wallpaper
   deskMan->GetCurrentDesktop()->RefreshWallpaper();

   return 0;
}
