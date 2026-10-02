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
#include "Resource.h"
#include "Locale.h"

LPCWSTR Locale::GetString(UINT uID)
{
   std::map<UINT, String>::iterator it = m_strings.find(uID);

   if (it == m_strings.end())
   {
      LPCWSTR resource = NULL;
      int length = LoadStringW(GetResDll(), uID, (LPWSTR)&resource, 0);   //get a pointer to the read-only resource
      it = m_strings.insert(std::make_pair(uID, String(resource ? resource : L"", length))).first;
   }

   return it->second.c_str();
}

int Locale::MessageBox(HWND hWnd, UINT uIdText, UINT uIdCaption, UINT uType)
{
   return ::MessageBox(hWnd, GetString(uIdText), GetString(uIdCaption), uType);
}
