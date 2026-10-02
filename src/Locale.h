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

#ifndef __LOCALE_H__
#define __LOCALE_H__

#include <string>
#include <map>
#include <type_traits>

typedef std::wstring String;

/** Access to the user interface resources (strings, menus, dialogs).
 * All resources are embedded in the executable (English only).
 */
class Locale
{
public:
   static Locale& GetInstance()        { static Locale instance; return instance; }

   operator HINSTANCE()                { return GetResDll(); }
   HINSTANCE GetResDll()               { return GetModuleHandle(NULL); }

   HMENU LoadMenu(UINT uID)            { return ::LoadMenu(GetResDll(), MAKEINTRESOURCE(uID)); }

   /** Get a resource string.
    * The returned pointer stays valid for the whole life of the program.
    */
   LPCWSTR GetString(UINT uID);

   int MessageBox(HWND hWnd, UINT uIdText, UINT uIdCaption, UINT uType);

protected:
   std::map<UINT, String> m_strings;
};

#define locMessageBox(hwnd, uIdText, uIdCaption, uType)              \
   Locale::GetInstance().MessageBox(hwnd, uIdText, uIdCaption, uType)

/** Retrieve a resource string into a (possibly non-const) string pointer.
 * The string must not be modified.
 */
#define locGetString(str, uId)                                       \
   (str = (std::remove_reference_t<decltype(str)>)Locale::GetInstance().GetString(uId))

#endif /*__LOCALE_H__*/
