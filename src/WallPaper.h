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

#ifndef __WALLPAPER_H__
#define __WALLPAPER_H__

#include <string>

/** Wallpaper of a desktop.
 * The wallpaper is changed asynchronously (it can take some time), by a worker
 * thread. The wallpaper of Windows is only changed if some desktop uses a
 * specific wallpaper; the original wallpaper is restored when switching to a
 * desktop using the default wallpaper, and when the program ends.
 */
class WallPaper
{
public:
   WallPaper();
   ~WallPaper(void);

   /** Display this wallpaper. */
   void Activate();
   /** Display this wallpaper again, if it is the active one (eg, after explorer restarted). */
   void Refresh();

   /** Set the wallpaper.
    * Path of the image to use, or an empty string for default wallpaper (the one of Windows),
    * or NULL to disable the wallpaper.
    */
   void SetImage(LPCWSTR fileName);
   void SetColor(COLORREF bkColor);

   /** Get the wallpaper of Windows (the one used for the default wallpaper). */
   static LPCWSTR GetDefaultWallpaper();
   /** The wallpaper of Windows may have been changed. */
   static void RefreshDefaultWallpaper();

   /** Restore the wallpaper of Windows, and stop the worker thread. */
   static void Shutdown();

protected:
   enum Mode { WP_DEFAULT, WP_NONE, WP_IMAGE };

   Mode m_mode;
   std::wstring m_fileName;
   COLORREF m_bkColor;

   void Apply();

   static WallPaper * m_activeWallPaper;
   static std::wstring m_defaultWallpaper;
};

#endif /*__WALLPAPER_H__*/
