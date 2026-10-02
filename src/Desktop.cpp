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
#include "Desktop.h"
#include <assert.h>
#include "WindowsManager.h"
#include "DesktopManager.h"
#include "WindowsList.h"
#include "VirtualDimension.h"
#include "PlatformHelper.h"
#include "Locale.h"

Desktop::Desktop(int i)
{
   m_active = false;
   m_index = i;
   m_hotkey = 0;
   m_rect.bottom = m_rect.left = m_rect.right = m_rect.top = 0;
   wcscpy_s(m_wallpaperFile, DESKTOP_WALLPAPER_DEFAULT);
   m_bkColor = GetSysColor(COLOR_DESKTOP);

   m_wallpaper.SetImage(FormatWallpaper(m_wallpaperFile));
   m_wallpaper.SetColor(m_bkColor);

   swprintf_s(m_name, L"%s%i", Locale::GetInstance().GetString(IDS_DESKTOP_BASENAME), i);
}

Desktop::Desktop(Settings::Desktop * desktop)
{
   desktop->GetName(m_name, DESKTOP_NAME_LENGTH);
   desktop->LoadSetting(Settings::Desktop::DeskWallpaper, m_wallpaperFile, MAX_PATH);
   m_index = desktop->LoadSetting(Settings::Desktop::DeskIndex);
   m_hotkey = desktop->LoadSetting(Settings::Desktop::DeskHotkey);
   m_bkColor = desktop->LoadSetting(Settings::Desktop::BackgroundColor);
   m_rect.bottom = m_rect.left = m_rect.right = m_rect.top = 0;

   m_wallpaper.SetImage(FormatWallpaper(m_wallpaperFile));
   m_wallpaper.SetColor(m_bkColor);

   m_active = false;

   if (m_hotkey != 0)
      HotKeyManager::GetInstance()->RegisterHotkey(m_hotkey, this);
}

Desktop::~Desktop(void)
{
   WindowsManager::Iterator it;

   //Show the hidden windows, if any
   for(it = winMan->GetIterator(); it; it++)
   {
      Window * win = it;

      if (win->IsOnDesk(this))
         win->ShowWindow();
   }

   //Unregister the hotkey
   if (m_hotkey != 0)
      HotKeyManager::GetInstance()->UnregisterHotkey(this);

   //Remove the tooltip tool
   tooltip->UnsetTool(this);
}

HMENU Desktop::BuildMenu()
{
   WindowsManager::Iterator it;
   HMENU hMenu;
   MENUITEMINFO mii;
   MENUINFO mi;
   UINT index = 0;

   //Create the menu
   hMenu = CreatePopupMenu();

   //Set its style
   mi.cbSize = sizeof(MENUINFO);
   mi.fMask = MIM_STYLE;
   mi.dwStyle = MNS_CHECKORBMP;
   SetMenuInfo(hMenu, &mi);

   //Add the menu items
   mii.cbSize = sizeof(mii);
   mii.fMask = MIIM_STRING | MIIM_ID | MIIM_DATA | MIIM_BITMAP;

   for(it = winMan->GetIterator(); it; it++)
   {
      Window * win = it;

      if (!win->IsOnDesk(this))
         continue;

      //The id is the index of the window among the ones of this desktop
      mii.dwItemData = (ULONG_PTR)win->GetIcon();
      mii.dwTypeData = (LPWSTR)win->GetText();
      mii.wID = WM_USER + index++;
      mii.hbmpItem = HBMMENU_CALLBACK;

      InsertMenuItem(hMenu, (UINT)-1, TRUE, &mii);
   }

   return hMenu;
}

void Desktop::OnMenuItemSelected(HMENU /*menu*/, int cmdId)
{
   WindowsManager::Iterator it;
   int index = cmdId - WM_USER;

   for(it = winMan->GetIterator(); it; it++)
   {
      Window * win = it;

      if (!win->IsOnDesk(this))
         continue;

      if (index-- == 0)
      {
         win->Activate();
         break;
      }
   }
}

void Desktop::resize(LPRECT rect)
{
   m_rect.left = rect->left + 2;
   m_rect.top = rect->top + 2;
   m_rect.right = rect->right - 2;
   m_rect.bottom = rect->bottom - 2;

   UpdateLayout();
}

void Desktop::UpdateLayout()
{
   tooltip->SetTool(this);

   WindowsManager::Iterator it;
   int iconSize = vdWindow.GetIconSize();
   int x, y;

   x = m_rect.left;
   y = m_rect.top;
   for(it = winMan->GetIterator(); it; it++)
   {
      Window * win = it;
      RECT rect;

      if (!win->IsOnDesk(this))
         continue;

      rect.left = x;
      rect.top = y;
      rect.right = x + iconSize;
      rect.bottom = y + iconSize;

      tooltip->SetTool(win, &rect);

      x += iconSize;
      if (x > m_rect.right-iconSize)
      {
         x = m_rect.left;
         y += iconSize;
      }
   }

   vdWindow.Refresh();
}

void Desktop::Draw(HDC hDc)
{
   RECT rect = m_rect;

   //Print desktop name in the middle
   SetBkMode(hDc, TRANSPARENT);
   DrawTextW(hDc, m_name, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

   //Draw a frame around the desktop
   FrameRect(hDc, &m_rect, (HBRUSH)GetStockObject(BLACK_BRUSH));

   //Draw icons for each window, clipped to the desktop (when there are more windows than room)
   int savedDC = SaveDC(hDc);
   IntersectClipRect(hDc, m_rect.left, m_rect.top, m_rect.right, m_rect.bottom);

   WindowsManager::Iterator it;
   list<Window*> obsoleteWindowsList;
   int iconSize = vdWindow.GetIconSize();
   int x, y;

   x = m_rect.left;
   y = m_rect.top;
   for(it = winMan->GetIterator(); it; it++)
   {
      Window * win = it;
      HICON hIcon;

      //Mark obsolete windows for removal
      if (!win->CheckExists())
      {
         obsoleteWindowsList.push_front(win);
         continue;
      }

      //Skip windows not present on this desk
      if (!win->IsOnDesk(this))
         continue;

      //Draw the window's icon
      hIcon = win->GetIcon();
      DrawIconEx(hDc, x, y, hIcon, iconSize, iconSize, 0, NULL, DI_NORMAL);

      x += iconSize;
      if (x > m_rect.right-iconSize)
      {
         x = m_rect.left;
         y += iconSize;
      }
   }

   RestoreDC(hDc, savedDC);

   //Remove windows flagged as obsolete
   for(list<Window*>::iterator i = obsoleteWindowsList.begin();
       i != obsoleteWindowsList.end();
       i++)
      winMan->RemoveWindow(*i);
   obsoleteWindowsList.clear();
}

/** Get a pointer to the window represented at some position.
 * This function returns a pointer to the window object that is represented at the specified
 * position, if any. If there is no such window, it returns NULL.
 * In addition, one should make sure the point is in the desktop's rectangle.
 *
 * @param X Horizontal position of the cursor
 * @param Y Vertical position of the cursor
 */
Window* Desktop::GetWindowFromPoint(int X, int Y)
{
   WindowsManager::Iterator it;
   int iconSize = vdWindow.GetIconSize();
   int columns;
   int index;

   if (X < m_rect.left || Y < m_rect.top || X >= m_rect.right || Y >= m_rect.bottom)
      return NULL;

   //Same layout as in Draw()
   columns = std::max(1, (int)(m_rect.right - m_rect.left) / iconSize);
   if ((X - m_rect.left) / iconSize >= columns)
      return NULL;
   index = ((X - m_rect.left) / iconSize) + columns * ((Y - m_rect.top) / iconSize);

   for(it = winMan->GetIterator(); it; it++)
   {
      Window * win = it;

      if (!win->IsOnDesk(this))
         continue;

      if (index == 0)
         return win;
      index --;
   }

   return NULL;
}

void Desktop::Rename(LPCWSTR name)
{
   Settings settings;
   Settings::Desktop desktop(&settings, m_name);

   /* Remove the desktop from registry */
   desktop.Destroy();

   /* copy the new name */
   lstrcpynW(m_name, name, DESKTOP_NAME_LENGTH);
}

void Desktop::Remove()
{
   Settings settings;
   Settings::Desktop desktop(&settings, m_name);

   /* Remove the desktop from registry */
   desktop.Destroy();

   /* Move all windows present only on this desktop to the current desk */
   Desktop * curDesk;
   WindowsManager::Iterator it;

   curDesk = deskMan->GetCurrentDesktop();
   for(it = winMan->GetIterator(); it; it++)
   {
      Window * win = it;

      if ( (win->IsOnDesk(this)) &&
          !(win->IsOnDesk(NULL)) )
         win->MoveToDesktop(curDesk);
   }
}

void Desktop::Save()
{
   Settings settings;
   Settings::Desktop desktop(&settings, m_name);

   desktop.SaveSetting(Settings::Desktop::DeskWallpaper, m_wallpaperFile);
   desktop.SaveSetting(Settings::Desktop::DeskIndex, m_index);
   desktop.SaveSetting(Settings::Desktop::DeskHotkey, m_hotkey);
   desktop.SaveSetting(Settings::Desktop::BackgroundColor, m_bkColor);
}

void Desktop::Activate(void)
{
   WindowsManager::Iterator it;
   Window * topWindow;

   m_active = true;
   TRACE(L"Activating desktop %s", m_name);

   /* Set the wallpaper */
   m_wallpaper.Activate();

   //Activate our own window while the windows are shown/hidden, so that the system does
   //not activate (one after the other) the windows which are being hidden.
   SetForegroundWindow(vdWindow);

   winMan->DisableAnimations();

   //First show the windows of this desktop, then hide the other ones: the windows being
   //hidden cover the ones being shown, which reduces flickering.
   for(it = winMan->GetIterator(); it; it++)
   {
      Window * win = it;

      //Ignore obsolete windows
      if (!win->CheckExists())
         continue;

      if (win->IsMoving())
      {
         //The window is being dragged: it follows the user on the new desktop
         win->MoveToDesktop(this);
      }
      else if (win->IsOnDesk(this))
      {
         win->UnFlashWindow();
         win->SetSwitching(true);
         if (win->IsInTray())
            trayManager->AddIcon(win);
         else
            win->ShowWindow();
         win->SetSwitching(false);
      }
   }

   for(it = winMan->GetIterator(); it; it++)
   {
      Window * win = it;

      if (!win->CheckExists() || win->IsOnDesk(this))
         continue;

      win->SetSwitching(true);
      if (win->IsInTray())
         trayManager->DelIcon(win);
      else
         win->HideWindow();
      win->SetSwitching(false);
   }

   winMan->EnableAnimations();

   //Give the focus to the window which was active when this desktop was left
   topWindow = winMan->GetTopWindow(this);
   if (topWindow)
      SetForegroundWindow(topWindow->GetOwnedWindow());
   else if (!IsWindowVisible(vdWindow))
      SetForegroundWindow(GetShellWindow());
}

void Desktop::Desactivate(void)
{
   m_active = false;
}

void Desktop::SetHotkey(int hotkey)
{
   if (m_hotkey != 0)
      HotKeyManager::GetInstance()->UnregisterHotkey(this);

   m_hotkey = hotkey;

   if (m_hotkey != 0)
      HotKeyManager::GetInstance()->RegisterHotkey(m_hotkey, this);
}

void Desktop::OnHotkey()
{
   deskMan->SwitchToDesktop(this);
}

/** Get the actual wallpaper to use for some wallpaper setting.
 * @return "" for the default wallpaper, NULL for no wallpaper, or the path of the image.
 */
LPCWSTR Desktop::FormatWallpaper(LPWSTR fileName)
{
   LPCWSTR res;

   if (*fileName == 0)
   {
      wcscpy_s(fileName, MAX_PATH, DESKTOP_WALLPAPER_DEFAULT);
      res = L"";
   }
   else if (_wcsicmp(fileName, DESKTOP_WALLPAPER_NONE) == 0)
      res = NULL;
   else if (_wcsicmp(fileName, DESKTOP_WALLPAPER_DEFAULT) == 0)
      res = L"";
   else
      res = fileName;

   return res;
}

void Desktop::SetWallpaper(LPCWSTR fileName)
{
   lstrcpynW(m_wallpaperFile, fileName, MAX_PATH);
   m_wallpaper.SetImage(FormatWallpaper(m_wallpaperFile));
}

void Desktop::SetBackgroundColor(COLORREF col)
{
   if (col == m_bkColor)
      return;

   m_bkColor = col;

   m_wallpaper.SetColor(m_bkColor);
}

bool Desktop::deskOrder(Desktop * first, Desktop * second)
{
   return first->m_index < second->m_index;
}
