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
#include "settings.h"

const wchar_t Settings::regKeyName[] = L"Software\\Typz Software\\Virtual Dimension\\";

static const RECT DefaultWindowPosition = {10, 10, 110, 110};
static const LOGFONT DefaultPreviewWindowFont = {-12/*height*/,0,0,0,FW_BOLD/*weight*/,FALSE/*italic*/,0,0,0,0,0,0,0,L"Segoe UI"/*fontname*/};
static const LOGFONT DefaultOSDFont = {-29/*height*/,0,0,0,FW_BOLD/*weight*/,TRUE/*italic*/,0,0,0,0,0,0,0,L"Segoe UI"/*fontname*/};
static const POINT DefaultOSDPosition = {50,50};

DEFINE_SETTING(Settings, WindowPosition, RECT, &DefaultWindowPosition);
DEFINE_SETTING(Settings, DockedBorders, int, 0);
DEFINE_SETTING(Settings, ColumnNumber, unsigned long, 2);
DEFINE_SETTING(Settings, LockPreviewWindow, bool, false);
DEFINE_SETTING(Settings, ShowWindow, bool, true);
DEFINE_SETTING(Settings, HasTrayIcon, bool, true);
DEFINE_SETTING(Settings, AlwaysOnTop, bool, false);
DEFINE_SETTING(Settings, TransparencyLevel, unsigned char, 0xff);
DEFINE_SETTING(Settings, HasCaption, bool, true);
DEFINE_SETTING(Settings, SnapSize, int, 15);
DEFINE_SETTING(Settings, AutoHideDelay, int, 0);
DEFINE_SETTING(Settings, EnableToolTips, bool, true);
DEFINE_SETTING(Settings, ConfirmKilling, bool, true);
DEFINE_SETTING(Settings, AutoSaveWindowSettings, bool, false);
DEFINE_SETTING(Settings, CloseToTray, bool, false);
DEFINE_SETTING(Settings, AutoSwitchDesktop, bool, true);
DEFINE_SETTING(Settings, AllWindowsInTaskList, bool, false);
DEFINE_SETTING(Settings, SwitchToNextDesktopHotkey, int, 0);
DEFINE_SETTING(Settings, SwitchToPreviousDesktopHotkey, int, 0);
DEFINE_SETTING(Settings, SwitchToTopDesktopHotkey, int, 0);
DEFINE_SETTING(Settings, SwitchToBottomDesktopHotkey, int, 0);
DEFINE_SETTING(Settings, SwitchToLeftDesktopHotkey, int, 0);
DEFINE_SETTING(Settings, SwitchToRightDesktopHotkey, int, 0);
DEFINE_SETTING(Settings, MoveWindowToNextDesktopHotkey, int, 0);
DEFINE_SETTING(Settings, MoveWindowToPreviousDesktopHotkey, int, 0);
DEFINE_SETTING(Settings, MoveWindowToDesktopHotkey, int, 0);
DEFINE_SETTING(Settings, MaximizeHeightHotkey, int, 0);
DEFINE_SETTING(Settings, MaximizeWidthHotkey, int, 0);
DEFINE_SETTING(Settings, AlwaysOnTopHotkey, int, 0);
DEFINE_SETTING(Settings, TransparencyHotkey, int, 0);
DEFINE_SETTING(Settings, TogglePreviewWindowHotkey, int, 0);
DEFINE_SETTING(Settings, DisplayMode, int, 0);
DEFINE_SETTING(Settings, BackgroundColor, COLORREF, RGB(0xc0,0xc0,0xc0));
DEFINE_SETTING(Settings, BackgroundPicture, LPCWSTR, L"");
DEFINE_SETTING(Settings, DesktopNameOSD, bool, false);
DEFINE_SETTING(Settings, PreviewWindowFont, LOGFONT, &DefaultPreviewWindowFont);
DEFINE_SETTING(Settings, PreviewWindowFontColor, COLORREF, RGB(0,0,0));
DEFINE_SETTING(Settings, OSDTimeout, int, 2000);
DEFINE_SETTING(Settings, OSDFont, LOGFONT, DefaultOSDFont);
DEFINE_SETTING(Settings, OSDFgColor, COLORREF, RGB(0,0,0));
DEFINE_SETTING(Settings, OSDBgColor, COLORREF, RGB(255,255,255));
DEFINE_SETTING(Settings, OSDPosition, POINT, &DefaultOSDPosition);
DEFINE_SETTING(Settings, OSDTransparencyLevel, unsigned char, 200);
DEFINE_SETTING(Settings, OSDHasBackground, bool, true);
DEFINE_SETTING(Settings, OSDIsTransparent, bool, true);
DEFINE_SETTING(Settings, WarpEnable, bool, false);
DEFINE_SETTING(Settings, WarpSensibility, LONG, 3);
DEFINE_SETTING(Settings, WarpMinDuration, DWORD, 500);
DEFINE_SETTING(Settings, WarpRewarpDelay, DWORD, 3000);
DEFINE_SETTING(Settings, WarpRequiredVKey, int, 0);
DEFINE_SETTING(Settings, WarpInvertMousePos, bool, true);
DEFINE_SETTING(Settings, DefaultHidingMethod, int, 0);

Settings::Settings(void): RegistryGroup(regKeyName)
{
}

const wchar_t Settings::regKeyWindowsStartup[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const wchar_t Settings::regValStartWithWindows[] = L"Virtual Dimension";

bool Settings::LoadStartWithWindows()
{
   return RegGetValueW(HKEY_CURRENT_USER, regKeyWindowsStartup, regValStartWithWindows,
                       RRF_RT_REG_SZ, NULL, NULL, NULL) == ERROR_SUCCESS;
}

void Settings::SaveStartWithWindows(bool start)
{
   HKEY regKey;
   if (RegOpenKeyExW(HKEY_CURRENT_USER, regKeyWindowsStartup, 0, KEY_WRITE, &regKey) == ERROR_SUCCESS)
   {
      if (start)
      {
         wchar_t path[MAX_PATH];
         wchar_t buffer[MAX_PATH+2];
         GetModuleFileNameW(NULL, path, MAX_PATH);
         swprintf_s(buffer, L"\"%s\"", path);
         RegSetValueExW(regKey, regValStartWithWindows, 0, REG_SZ, (const BYTE*)buffer, (DWORD)((wcslen(buffer)+1)*sizeof(wchar_t)));
      }
      else
         RegDeleteValueW(regKey, regValStartWithWindows);
      RegCloseKey(regKey);
   }
}

const wchar_t Settings::regSubKeyHidingMethods[] = L"HidingMethodsTweaks";

int Settings::LoadHidingMethod(LPCWSTR program)
{
   DWORD val;
   DWORD size = sizeof(val);

   if (m_opened &&
       RegGetValueW(m_regKey, regSubKeyHidingMethods, program, RRF_RT_REG_DWORD, NULL, &val, &size) == ERROR_SUCCESS)
      return val;

   return LoadSetting(DefaultHidingMethod);
}

void Settings::SaveHidingMethod(LPCWSTR program, int method)
{
   HKEY regKey=NULL;

   if (m_opened &&
       RegCreateKeyExW(m_regKey, regSubKeyHidingMethods, 0, NULL, 0, KEY_WRITE, NULL, &regKey, NULL) == ERROR_SUCCESS)
   {
      if (method == 0)
         RegDeleteValueW(regKey, program);
      else
         RegSetValueExW(regKey, program, 0, REG_DWORD, (BYTE*)&method, sizeof(method));

      RegCloseKey(regKey);
   }
}

const wchar_t Settings::Desktop::regKeyDesktops[] = L"Desktops";

DEFINE_SETTING(Settings::Desktop, DeskIndex, int, 0);
DEFINE_SETTING(Settings::Desktop, DeskWallpaper, LPCWSTR, L"");
DEFINE_SETTING(Settings::Desktop, DeskHotkey, int, 0);
DEFINE_SETTING(Settings::Desktop, BackgroundColor, COLORREF, GetSysColor(COLOR_DESKTOP));

Settings::SubkeyList::SubkeyList(Settings * settings, LPCWSTR regKey): m_group(*settings, regKey)
{
   *m_name = 0;
}

Settings::SubkeyList::SubkeyList(Settings * settings, LPCWSTR regKey, int index): m_group(*settings, regKey)
{
   *m_name = 0;
   Open(index);
}

Settings::SubkeyList::SubkeyList(Settings * settings, LPCWSTR regKey, LPCWSTR name, bool create): m_group(*settings, regKey)
{
   *m_name = 0;
   Open(name, create);
}

bool Settings::SubkeyList::Open(LPCWSTR name, bool create)
{
   if (name == NULL)
      return false;

   lstrcpynW(m_name, name, MAX_NAME_LENGTH);
   return Config::RegistryGroup::Open(m_group, name, create);
}

bool Settings::SubkeyList::Open(int index)
{
   DWORD length;
   LSTATUS result;

   if (m_opened)
      Close();

   length = MAX_NAME_LENGTH;
   m_opened =
      (m_group.IsOpened()) &&
      (((result = RegEnumKeyExW(m_group, index, m_name, &length, NULL, NULL, NULL, NULL)) == ERROR_SUCCESS) || (result == ERROR_MORE_DATA)) &&
      (RegOpenKeyExW(m_group, m_name, 0, KEY_READ | KEY_WRITE, &m_regKey) == ERROR_SUCCESS);

   return m_opened;
}

bool Settings::SubkeyList::IsValid()
{
   return m_opened;
}

void Settings::SubkeyList::Destroy()
{
   if (m_opened)
      Close();
   else
      return;

   if (m_group.IsOpened())
      RegDeleteTreeW(m_group, m_name);
}

LPWSTR Settings::SubkeyList::GetName(LPWSTR buffer, unsigned int length)
{
   if (m_opened && (buffer != NULL) && length > 0)
      lstrcpynW(buffer, m_name, length);

   return buffer;
}

bool Settings::SubkeyList::Rename(LPCWSTR name)
{
   HKEY newKey;

   if (!m_opened || (wcsncmp(name, m_name, MAX_NAME_LENGTH) == 0))
      return m_opened;

   if ( (!m_group.IsOpened()) ||
        (RegCreateKeyExW(m_group, name, 0, NULL, REG_OPTION_NON_VOLATILE,
                         KEY_READ | KEY_WRITE, NULL, &newKey, NULL) != ERROR_SUCCESS) )
      return false;

   RegCopyTreeW(m_regKey, NULL, newKey);

   Destroy();
   m_regKey = newKey;
   m_opened = true;
   lstrcpynW(m_name, name, MAX_NAME_LENGTH);

   return true;
}

const wchar_t Settings::Window::regKeyWindows[] = L"Windows";

static const RECT DefaultWindowAutoPosition = { 0, 200, 0, 300 };

DEFINE_SETTING(Settings::Window, AlwaysOnTop, bool, false);
DEFINE_SETTING(Settings::Window, OnAllDesktops, bool, false);
DEFINE_SETTING(Settings::Window, MinimizeToTray, bool, false);
DEFINE_SETTING(Settings::Window, TransparencyLevel, unsigned char, 0xc0);
DEFINE_SETTING(Settings::Window, EnableTransparency, bool, false);
DEFINE_SETTING(Settings::Window, AutoSaveSettings, bool, false);
DEFINE_SETTING(Settings::Window, WindowPosition, RECT, DefaultWindowAutoPosition);
DEFINE_SETTING(Settings::Window, AutoSetSize, bool, false);
DEFINE_SETTING(Settings::Window, AutoSetPos, bool, false);
DEFINE_SETTING(Settings::Window, AutoSetDesk, bool, false);
DEFINE_SETTING(Settings::Window, DesktopIndex, int, -1);
