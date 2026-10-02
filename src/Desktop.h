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

#ifndef __DESKTOP_H__
#define __DESKTOP_H__

#include <list>
#include "settings.h"
#include "tooltip.h"
#include "HotKeyManager.h"
#include "WallPaper.h"

using namespace std;

class Window;

#define DESKTOP_WALLPAPER_DEFAULT   L"<default>"
#define DESKTOP_WALLPAPER_NONE      L"<none>"

#define DESKTOP_NAME_LENGTH         80

class Desktop: public ToolTip::Tool, HotKeyManager::EventHandler
{
public:
   Desktop(int i);
   Desktop(Settings::Desktop * desktop);
   virtual ~Desktop(void);

   HMENU BuildMenu();
   void OnMenuItemSelected(HMENU menu, int cmdId);

   void Draw(HDC dc);
   void resize(LPRECT rect);
   void UpdateLayout();
   Window* GetWindowFromPoint(int x, int y);

   void Rename(LPCWSTR name);
   void Remove();
   void Save();

   void Activate(void);
   void Desactivate(void);
   bool IsActive() const      { return m_active; }

   int GetHotkey() const      { return m_hotkey; }
   void SetHotkey(int hotkey);

   void SetIndex(int index)   { m_index = index; }
   int GetIndex() const       { return m_index; }

   LPCWSTR GetWallpaper()     { return m_wallpaperFile; }
   void SetWallpaper(LPCWSTR fileName);
   void RefreshWallpaper()    { m_wallpaper.Refresh(); }
   static LPCWSTR FormatWallpaper(LPWSTR fileName);

   COLORREF GetBackgroundColor() const   { return m_bkColor; }
   void SetBackgroundColor(COLORREF col);

   LPCWSTR GetText()          { return m_name; }
   void GetRect(LPRECT rect)  { *rect = m_rect; }

   static bool deskOrder(Desktop * first, Desktop * second);

   bool Configure(HWND hDlg);

protected:
   void OnHotkey();

   static INT_PTR CALLBACK DeskProperties(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

   bool m_active;
   int m_index;
   wchar_t m_name[DESKTOP_NAME_LENGTH];
   int m_hotkey;
   RECT m_rect;
   WallPaper m_wallpaper;
   wchar_t m_wallpaperFile[MAX_PATH];
   COLORREF m_bkColor;

   class DesktopProperties
   {
   public:
      DesktopProperties(Desktop * desktop);
      ~DesktopProperties();

      void InitDialog(HWND hDlg);
      bool Apply(HWND hDlg);
      void OnWallpaperChanged(HWND hDlg, HWND ctrl);
      void OnBrowseWallpaper(HWND hDlg);
      void OnChooseWallpaper(HWND hDlg);
      void OnPreviewDrawItem(LPDRAWITEMSTRUCT lpDrawItem);
      void OnBgColorDrawItem(LPDRAWITEMSTRUCT lpDrawItem);
      void SelectColor(HWND hDlg);
      void ResetWallpaper(HWND hDlg);

   protected:
      Desktop * m_desk;
      HBITMAP m_picture;
      wchar_t m_wallpaper[MAX_PATH];
      COLORREF m_bgColor;
   };
};

#endif /*__DESKTOP_H__*/
