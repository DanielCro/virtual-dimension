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
#include "Config.h"

using namespace Config;

unsigned int Group::LoadSetting(const Setting<LPCWSTR> &setting, LPWSTR buffer, unsigned int length, const StringSetting& /*type*/)
{
   unsigned int size;

   //Get the size (in characters, including the terminating null character)
   size = LoadString(setting.m_name, NULL, 0);
   if (!size)
      size = (unsigned int)wcslen(setting.m_default) + 1;

   if (buffer && length > 0 && LoadString(setting.m_name, buffer, length) == 0)
   {
      lstrcpynW(buffer, setting.m_default, length);
      size = (unsigned int)wcslen(buffer) + 1;
   }

   return size;
}

bool RegistryGroup::Open(HKEY parent, LPCWSTR path, bool create)
{
   if (m_opened)
      Close();

   if (create)
      m_opened = RegCreateKeyExW(parent, path,
                  0, NULL, REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE, NULL,
                  &m_regKey, NULL) == ERROR_SUCCESS;
   else
      m_opened = RegOpenKeyExW(parent, path, 0, KEY_READ | KEY_WRITE,
                  &m_regKey) == ERROR_SUCCESS;

   return m_opened;
}

void RegistryGroup::Close()
{
   if (m_opened)
      RegCloseKey(m_regKey);
   m_opened = false;
}

Group * RegistryGroup::GetSubGroup(LPCWSTR path)
{
   RegistryGroup * m_subgroup = new RegistryGroup();

   if (!m_opened || !m_subgroup->Open(m_regKey, path))
   {
      delete m_subgroup;
      m_subgroup = NULL;
   }

   return m_subgroup;
}

DWORD RegistryGroup::LoadDWord(LPCWSTR entry, DWORD defVal)
{
   DWORD size = sizeof(DWORD);
   DWORD val;

   if ( (!m_opened) ||
        (RegGetValueW(m_regKey, NULL, entry, RRF_RT_REG_DWORD, NULL, &val, &size) != ERROR_SUCCESS) )
   {
      // Cannot load the value from registry --> set default value
      val = defVal;
   }

   return val;
}

void RegistryGroup::SaveDWord(LPCWSTR entry, DWORD value)
{
   if (m_opened)
      RegSetValueExW(m_regKey, entry, 0, REG_DWORD, (LPBYTE)&value, sizeof(value));
}

bool RegistryGroup::LoadBinary(LPCWSTR entry, LPBYTE buffer, DWORD length, const BYTE * defval)
{
   DWORD size;
   DWORD type;
   bool res;

   res = (m_opened) &&
         (RegQueryValueExW(m_regKey, entry, NULL, &type, NULL, &size) == ERROR_SUCCESS) &&
         (size == length) &&
         (type == REG_BINARY) &&
         (RegQueryValueExW(m_regKey, entry, NULL, NULL, buffer, &size) == ERROR_SUCCESS);

   if (!res)
   {
      // Cannot load the value from registry --> set default value
      memcpy(buffer, defval, length);
   }

   return res;
}

void RegistryGroup::SaveBinary(LPCWSTR entry, const BYTE * buffer, DWORD length)
{
   if (m_opened)
      RegSetValueExW(m_regKey, entry, 0, REG_BINARY, buffer, length);
}

unsigned int RegistryGroup::LoadString(LPCWSTR entry, LPWSTR buffer, unsigned int length)
{
   DWORD size = 0;

   if (!m_opened)
      return 0;

   if (buffer == NULL)
   {
      //Only return the size of the string, in characters
      if (RegGetValueW(m_regKey, NULL, entry, RRF_RT_REG_SZ, NULL, NULL, &size) != ERROR_SUCCESS)
         return 0;
      return size / sizeof(wchar_t);
   }

   size = length * sizeof(wchar_t);
   if (RegGetValueW(m_regKey, NULL, entry, RRF_RT_REG_SZ, NULL, buffer, &size) != ERROR_SUCCESS)
      return 0;

   return size / sizeof(wchar_t);
}

void RegistryGroup::SaveString(LPCWSTR entry, LPCWSTR buffer)
{
   DWORD len;

   len = (DWORD)((wcslen(buffer)+1) * sizeof(wchar_t));
   if (m_opened)
      RegSetValueExW(m_regKey, entry, 0, REG_SZ, (const BYTE*)buffer, len);
}

bool RegistryGroup::RemoveEntry(LPCWSTR entry)
{
   return m_opened && RegDeleteValueW(m_regKey, entry) == ERROR_SUCCESS;
}

bool RegistryGroup::RemoveGroup(LPCWSTR group)
{
   return m_opened && RegDeleteTreeW(m_regKey, group) == ERROR_SUCCESS;
}

BOOL RegistryGroup::EnumEntry(DWORD dwIndex, LPWSTR lpName, LPDWORD lpcName)
{
   return m_opened &&
      RegEnumValueW(m_regKey, dwIndex, lpName, lpcName, NULL, NULL, NULL, NULL) != ERROR_NO_MORE_ITEMS;
}

BOOL RegistryGroup::EnumGroup(DWORD dwIndex, LPWSTR lpName, DWORD cName)
{
   return m_opened &&
      RegEnumKeyW(m_regKey, dwIndex, lpName, cName)==ERROR_SUCCESS;
}
