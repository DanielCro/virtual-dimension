/*
 * Fast Window - A fast and convenient message dispatching window procedure.
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

#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <assert.h>

namespace Config {

class BinarySetting  { };
class DWordSetting { };
class StringSetting  { };

template <class T> class SettingType: public BinarySetting { };
#define DECLARE_SETTINGTYPE(clas, typ)    template <> class SettingType<clas>: public typ { };

template <class T> class Setting: public SettingType<T>
{
public:
   Setting(T defval, LPCWSTR name): m_default(defval), m_name(name) { }
   Setting(const T* defval, LPCWSTR name): m_default(*defval), m_name(name) { }
   T m_default;
   LPCWSTR m_name;
};

#define DECLARE_SETTING(name, type)                const Config::Setting<type> name
#define DEFINE_SETTING(clas, name, type, defval)   const Config::Setting<type> clas::name(defval, L"" #name)

//Common settings types
DECLARE_SETTINGTYPE(int, DWordSetting);
DECLARE_SETTINGTYPE(unsigned int, DWordSetting);
DECLARE_SETTINGTYPE(long, DWordSetting);
DECLARE_SETTINGTYPE(unsigned long, DWordSetting);
DECLARE_SETTINGTYPE(char, DWordSetting);
DECLARE_SETTINGTYPE(unsigned char, DWordSetting);
DECLARE_SETTINGTYPE(bool, DWordSetting);
DECLARE_SETTINGTYPE(LPCWSTR, StringSetting);

class Group
{
public:
   Group()              { m_opened = false; }
   virtual ~Group()     { }

   // Group selection
   virtual bool Open(LPCWSTR path, bool create=true) = 0;
   virtual void Close() = 0;
   bool IsOpened()      { return m_opened; }

   virtual Group * GetSubGroup(LPCWSTR path) = 0;

   //TODO: Warning: Missing GetDefaultSetting() for binary and strings !

   // Generic settings accessors, mostly for "large", fixed size, settings, saved as binary
   template<class T> inline bool LoadSetting(const Setting<T> &setting, T* data)          { return LoadSetting(setting, data, setting); }
   template<class T> inline void SaveSetting(const Setting<T> &setting, const T* data)    { SaveSetting(setting, data, setting); }

   //TODO: add a method to load a DWORD setting and know if load was OK or if default was used

   // Generic settings accessors for "small" settings (32bits or less), saved as DWORD
   template<class T> inline T LoadSetting(const Setting<T> &setting)                      { return LoadSetting(setting, setting); }
   template<class T> inline void SaveSetting(const Setting<T> &setting, T data)           { SaveSetting(setting, data, setting); }
   template<class T> static inline T GetDefaultSetting(const Setting<T> &setting)         { return GetDefaultSetting(setting, setting); }

   // String settings accessors
   inline unsigned int LoadSetting(const Setting<LPCWSTR> &setting, LPWSTR buffer, unsigned int length)  { return LoadSetting(setting, buffer, length, setting); }
   inline void SaveSetting(const Setting<LPCWSTR> &setting, LPCWSTR buffer)                              { SaveSetting(setting, buffer, setting); }

   // Load level accessors
   virtual DWORD LoadDWord(LPCWSTR entry, DWORD defVal) = 0;
   virtual void SaveDWord(LPCWSTR entry, DWORD value) = 0;
   virtual bool LoadBinary(LPCWSTR entry, LPBYTE buffer, DWORD length, const BYTE * defval) = 0;
   virtual void SaveBinary(LPCWSTR entry, const BYTE * buffer, DWORD length) = 0;
   virtual unsigned int LoadString(LPCWSTR entry, LPWSTR buffer, unsigned int length) = 0;
   virtual void SaveString(LPCWSTR entry, LPCWSTR buffer) = 0;

   virtual bool RemoveEntry(LPCWSTR entry) = 0;
   virtual bool RemoveGroup(LPCWSTR group) = 0;

   virtual BOOL EnumEntry(DWORD dwIndex, LPWSTR lpName, LPDWORD lpcName) = 0;
   virtual BOOL EnumGroup(DWORD dwIndex, LPWSTR lpName, DWORD cName) = 0;

protected:
   // Wrappers around the setting readers/writers to check data type against setting type
   template<class T> inline bool LoadSetting(const Setting<T> &setting, T* data, const BinarySetting& type);
   template<class T> inline void SaveSetting(const Setting<T> &setting, const T* data, const BinarySetting& type);

   template<class T> inline bool LoadSetting(const Setting<T> &setting, T* data, const DWordSetting& type);
   template<class T> inline T LoadSetting(const Setting<T> &setting, const DWordSetting& type);
   template<class T> inline void SaveSetting(const Setting<T> &setting, T data, const DWordSetting& type);
   template<class T> static inline T GetDefaultSetting(const Setting<T> &setting, const DWordSetting& type) { return setting.m_default; }

   unsigned int LoadSetting(const Setting<LPCWSTR> &setting, LPWSTR buffer, unsigned int length, const StringSetting& type);
   void SaveSetting(const Setting<LPCWSTR> &setting, LPCWSTR buffer, const StringSetting& /*type*/)             { SaveString(setting.m_name, buffer); }

   // Variables
   bool m_opened;
};

template<class T> inline bool Group::LoadSetting(const Setting<T> &setting, T* data, const BinarySetting& /*type*/)
{
   assert(sizeof(T)>sizeof(DWORD));
   return LoadBinary(setting.m_name, (LPBYTE)data, sizeof(T), (const BYTE*)&setting.m_default);
}

template<class T> inline void Group::SaveSetting(const Setting<T> &setting, const T* data, const BinarySetting& /*type*/)
{
   assert(sizeof(T)>sizeof(DWORD));
   return SaveBinary(setting.m_name, (const BYTE*)data, sizeof(T));
}

template<class T> inline T ConvertFromDWord(DWORD dw)    { return (T)dw; }
template<> inline bool ConvertFromDWord<bool>(DWORD dw)  { return dw ? true : false; }

template<class T> inline bool Group::LoadSetting(const Setting<T> &setting, T* data, const DWordSetting& type)
{
   assert(sizeof(T)<=sizeof(DWORD));
   return LoadBinary(setting.m_name, (LPBYTE)data, sizeof(T), (const BYTE*)&setting.m_default);
}

template<class T> T Group::LoadSetting(const Setting<T> &setting, const DWordSetting& /*type*/)
{
   assert(sizeof(T)<=sizeof(DWORD));
   return ConvertFromDWord<T>(LoadDWord(setting.m_name, (DWORD)setting.m_default));
}

template<class T> void Group::SaveSetting(const Setting<T> &setting, T data, const DWordSetting& /*type*/)
{
   assert(sizeof(T)<=sizeof(DWORD));
   SaveDWord(setting.m_name, (DWORD)data);
}

class RegistryGroup: public Group
{
public:
   RegistryGroup(): m_regKey(NULL)                                                  { }
   RegistryGroup(LPCWSTR path, bool create=true)                                    { Open(HKEY_CURRENT_USER, path, create); }
   RegistryGroup(RegistryGroup & parent, LPCWSTR relpath, bool create=true)         { Open(parent.m_regKey, relpath, create); }
   virtual ~RegistryGroup()                                                         { Close(); }

   virtual void Close();

   //TODO: Open(path) should parse the string to find if it begins with HKEY_CURRENT_USER/..., and open the correct key accordingly
   virtual bool Open(LPCWSTR path, bool create=true)                                   { return Open(HKEY_CURRENT_USER, path, create); }
   virtual bool Open(RegistryGroup & parent, LPCWSTR relpath, bool create=true)        { return Open(parent.m_regKey, relpath, create); }
   bool Open(HKEY parent, LPCWSTR path, bool create=true);

   virtual Group * GetSubGroup(LPCWSTR path);

   operator HKEY()                                               { return m_regKey; }

   // Load level accessors
   virtual DWORD LoadDWord(LPCWSTR entry, DWORD defVal);
   virtual void SaveDWord(LPCWSTR entry, DWORD value);
   virtual bool LoadBinary(LPCWSTR entry, LPBYTE buffer, DWORD length, const BYTE * defval);
   virtual void SaveBinary(LPCWSTR entry, const BYTE * buffer, DWORD length);
   virtual unsigned int LoadString(LPCWSTR entry, LPWSTR buffer, unsigned int length);
   virtual void SaveString(LPCWSTR entry, LPCWSTR buffer);

   virtual bool RemoveEntry(LPCWSTR entry);
   virtual bool RemoveGroup(LPCWSTR group);

   virtual BOOL EnumEntry(DWORD dwIndex, LPWSTR lpName, LPDWORD lpcName);
   virtual BOOL EnumGroup(DWORD dwIndex, LPWSTR lpName, DWORD cName);

protected:
   HKEY m_regKey;
};

};	/*namespace Config*/

#endif /*__CONFIG_H__*/
