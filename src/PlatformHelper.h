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

#ifndef __PLATFORMHELPER_H__
#define __PLATFORMHELPER_H__

#ifdef DEBUG
void TraceMessage(LPCWSTR format, ...);
#define TRACE(...) TraceMessage(__VA_ARGS__)
#else
#define TRACE(...)
#endif

/** Assorted helpers wrapping the platform specific (and often not so trivial)
 * parts of the Windows API.
 */
class PlatformHelper
{
public:
   /** Get the full path of the executable which owns a window. */
   static DWORD GetWindowFileName(HWND hWnd, LPWSTR lpFileName, int iBufLen);

   /** Tell if a window is cloaked by DWM.
    * Cloaked windows are "visible" for the window manager, but not displayed on
    * screen: suspended/closed Store applications, windows on another (native)
    * virtual desktop, shell flyouts... They must be ignored.
    */
   static bool IsWindowCloaked(HWND hWnd);

   /** Tell if this process may manipulate the windows of the process owning hWnd.
    * User Interface Privilege Isolation prevents a process from changing the
    * windows of processes running with a higher integrity level (eg, programs
    * run as administrator).
    */
   static bool CanManageWindow(HWND hWnd);

   /** Tell if a window answers messages in a timely manner. */
   static bool IsWindowResponsive(HWND hWnd, UINT timeout = 200);

   /** Get the icon of a packaged (Store) application window, from its
    * AppUserModelID. Returns NULL if the window does not belong to such an
    * application. The caller must destroy the returned icon.
    */
   static HICON GetAppIconForWindow(HWND hWnd, int size);

   /** Load an image file (any format supported by WIC: bmp, jpg, png, gif, tiff...)
    * as a 32bpp premultiplied-alpha DIB section. If width and height are not 0,
    * the image is resized. Returns NULL on failure. The caller must delete the bitmap.
    */
   static HBITMAP LoadImageFile(LPCWSTR fileName, int width = 0, int height = 0);
   static HBITMAP LoadImageResource(LPCWSTR name, LPCWSTR type, int width = 0, int height = 0);
   static SIZE GetBitmapSize(HBITMAP hBitmap);

   /** Draw a bitmap loaded with LoadImageFile()/LoadImageResource().
    * If stretch is false, the image is scaled down (keeping its aspect ratio)
    * to fit in the rectangle, and centered.
    */
   static void DrawBitmap(HDC hdc, HBITMAP hBitmap, const RECT& rect, bool stretch = true);

   static void AlphaBlend(HDC hdcDest, int nXOriginDest, int nYOriginDest,
                          HDC hdcSrc, int nXOriginSrc, int nYOriginSrc,
                          int nWidth, int nHeight, BYTE sourceAlpha);

   /** Scale a value expressed in 96 DPI pixels to the DPI of a window. */
   static int ScaleForWindow(HWND hWnd, int value);

   /** Get the work area of the monitor which (mostly) displays the window. */
   static RECT GetWorkArea(HWND hWnd);
   static RECT GetWorkArea(const RECT& rect);

   /** Get the rectangle covering all monitors. */
   static RECT GetVirtualScreen();
};

#endif /*__PLATFORMHELPER_H__*/
