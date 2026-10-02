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
#include "BackgroundDisplayMode.h"
#include "Settings.h"
#include <Commdlg.h>
#include "PlatformHelper.h"
#include "VirtualDimension.h"
#include "Locale.h"

void BackgroundDisplayMode::BeginPainting(HDC /*hdc*/)
{}

void BackgroundDisplayMode::PaintDesktop(HDC /*hdc*/, LPRECT /*rect*/, bool /*active*/)
{}

void BackgroundDisplayMode::EndPainting(HDC /*hdc*/)
{}

void BackgroundDisplayMode::ReSize(int /*width*/, int /*height*/)
{}

bool BackgroundDisplayMode::ChooseOptions(HWND /*hWnd*/)
{ return false; }



PictureBackgroundDisplayMode::PictureBackgroundDisplayMode(): m_selDeskBkPicture(NULL), m_deskBkPicture(NULL), m_picDC(NULL), m_height(0), m_width(0)
{
   Settings settings;

   settings.LoadSetting(Settings::BackgroundPicture, m_bkgrndPictureFile, MAX_PATH);
}

PictureBackgroundDisplayMode::~PictureBackgroundDisplayMode()
{
   Settings settings;

   if (m_deskBkPicture)
      DeleteObject(m_deskBkPicture);
   if (m_selDeskBkPicture)
      DeleteObject(m_selDeskBkPicture);

   settings.SaveSetting(Settings::BackgroundPicture, m_bkgrndPictureFile);
}

void PictureBackgroundDisplayMode::BeginPainting(HDC hdc)
{
   m_picDC = CreateCompatibleDC(hdc);
}

void PictureBackgroundDisplayMode::PaintDesktop(HDC hdc, LPRECT rect, bool active)
{
   HBITMAP picture = active ? m_selDeskBkPicture : m_deskBkPicture;

   if (picture == NULL)
   {
      FillRect(hdc, rect, GetSysColorBrush(active ? COLOR_WINDOW : COLOR_BTNFACE));
      return;
   }

   SelectObject(m_picDC, picture);

   BitBlt(hdc, rect->left, rect->top, rect->right-rect->left, rect->bottom-rect->top,
          m_picDC, 0, 0, SRCCOPY);
}

void PictureBackgroundDisplayMode::EndPainting(HDC /*hdc*/)
{
   DeleteDC(m_picDC);
}

void PictureBackgroundDisplayMode::ReSize(int width, int height)
{
   if ((m_width == width) && (m_height == height))
      return;

   m_width = width;
   m_height = height;

   UpdatePictureObjects();
}

bool PictureBackgroundDisplayMode::ChooseOptions(HWND hWnd)
{
   OPENFILENAME ofn;
   BOOL res;
   String filter;

   ZeroMemory(&ofn, sizeof(OPENFILENAME));
   ofn.lStructSize = sizeof(OPENFILENAME);
   ofn.hwndOwner = hWnd;
   ofn.lpstrFile = m_bkgrndPictureFile;
   ofn.nMaxFile = MAX_PATH;
   filter = Locale::GetInstance().GetString(IDS_PICTUREFILTER);
   std::replace(filter.begin(), filter.end(), L'|', L'\0');
   ofn.lpstrFilter = filter.c_str();
   ofn.nFilterIndex = 1;
   ofn.lpstrFileTitle = NULL;
   ofn.nMaxFileTitle = 0;
   ofn.lpstrInitialDir = NULL;
	locGetString(ofn.lpstrTitle, IDS_SELECT_BACKGROUND);
   ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER /*| OFN_ENABLESIZING*/;

   res = GetOpenFileName(&ofn);
   if (res)
      UpdatePictureObjects();

   return res ? true : false;
}

void PictureBackgroundDisplayMode::UpdatePictureObjects()
{
   if (m_deskBkPicture)
      DeleteObject(m_deskBkPicture);
   if (m_selDeskBkPicture)
      DeleteObject(m_selDeskBkPicture);
   m_deskBkPicture = m_selDeskBkPicture = NULL;

   if (m_width <= 0 || m_height <= 0)
      return;

   //Deselected picture
   m_deskBkPicture = PlatformHelper::LoadImageFile(m_bkgrndPictureFile, m_width, m_height);
   if (!m_deskBkPicture)
      return;

   //Selected picture: the same, lighter
   HDC winDC = GetWindowDC(vdWindow);
   HDC memDC = CreateCompatibleDC(winDC);
   HDC picDC = CreateCompatibleDC(winDC);
   RECT rect;

   m_selDeskBkPicture = CreateCompatibleBitmap(winDC, m_width, m_height);
   HGDIOBJ oldMemBmp = SelectObject(memDC, m_selDeskBkPicture);
   HGDIOBJ oldPicBmp = SelectObject(picDC, m_deskBkPicture);

   rect.left = rect.top = 0;
   rect.right = m_width;
   rect.bottom = m_height;
   FillRect(memDC, &rect, (HBRUSH)GetStockObject(WHITE_BRUSH));
   PlatformHelper::AlphaBlend(memDC, 0, 0, picDC, 0, 0, m_width, m_height, 128);

   SelectObject(picDC, oldPicBmp);
   SelectObject(memDC, oldMemBmp);
   DeleteDC(picDC);
   DeleteDC(memDC);
   ReleaseDC(vdWindow, winDC);
}

PlainColorBackgroundDisplayMode::PlainColorBackgroundDisplayMode()
{
   Settings settings;

   m_bkgrndColor = settings.LoadSetting(Settings::BackgroundColor);
   UpdateBrushObjects();
}

PlainColorBackgroundDisplayMode::~PlainColorBackgroundDisplayMode()
{
   Settings settings;

   DeleteObject(m_selDeskBkBrush);
   DeleteObject(m_deskBkBrush);

   settings.SaveSetting(Settings::BackgroundColor, m_bkgrndColor);
}

void PlainColorBackgroundDisplayMode::PaintDesktop(HDC hdc, LPRECT rect, bool active)
{
   if (active)
      FillRect(hdc, rect, m_selDeskBkBrush);
   else
      FillRect(hdc, rect, m_deskBkBrush);
}

bool PlainColorBackgroundDisplayMode::ChooseOptions(HWND hWnd)
{
   CHOOSECOLOR cc;
   BOOL res;
   static COLORREF acrCustClr[16];

   ZeroMemory(&cc, sizeof(CHOOSECOLOR));
   cc.lStructSize = sizeof(CHOOSECOLOR);
   cc.hwndOwner = hWnd;
   cc.rgbResult = m_bkgrndColor;
   cc.lpCustColors = acrCustClr;
   cc.Flags = CC_RGBINIT | CC_ANYCOLOR | CC_FULLOPEN;

   res = ChooseColor(&cc);
   if (res)
   {
      m_bkgrndColor = cc.rgbResult;
      DeleteObject(m_selDeskBkBrush);
      DeleteObject(m_deskBkBrush);
      UpdateBrushObjects();
   }

   return res ? true : false;
}

void PlainColorBackgroundDisplayMode::UpdateBrushObjects()
{
   COLORREF selected = RGB((GetRValue(m_bkgrndColor) * 255) / 192,
                           (GetGValue(m_bkgrndColor) * 255) / 192,
                           (GetBValue(m_bkgrndColor) * 255) / 192);
   m_selDeskBkBrush = CreateSolidBrush(selected);
   m_deskBkBrush = CreateSolidBrush(m_bkgrndColor);
}
