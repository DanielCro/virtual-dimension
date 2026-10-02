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

#ifndef __HOTKEYCONFIG_H__
#define __HOTKEYCONFIG_H__

#include <list>
#include "PlatformHelper.h"
#include "HotkeyManager.h"
#include "Config.h"
#include "Settings.h"

using namespace std;

class ConfigurableHotkey: public HotKeyManager::EventHandler
{
   friend class ShortcutsConfigurationDlg;

public:
   ConfigurableHotkey();
   virtual ~ConfigurableHotkey();

   int GetHotkey() const;
   bool SetHotkey(int hotkey);
	virtual void SaveHotkey()	{}

   virtual LPCWSTR GetName() const = 0;

protected:
   int m_hotkey;

private:
   int m_tempHotkey;
   void Commit()  { if (SetHotkey(m_tempHotkey)) SaveHotkey(); }
};

template <const Config::Setting<int>& setting> class PersistentHotkey: ConfigurableHotkey
{
public:
	PersistentHotkey()
	{
		Settings s;
		SetHotkey(s.LoadSetting(setting));
	}
	virtual void SaveHotkey()
	{
		Settings s;
		s.SaveSetting(setting, GetHotkey());
	}
};

class ShortcutsConfigurationDlg
{
public:
   static DLGPROC GetWindowProc()                              { return DlgProc; }

   static void RegisterHotkey(ConfigurableHotkey * hotkey)     { m_hotkeys.push_back(hotkey); }
   static void UnRegisterHotkey(ConfigurableHotkey * hotkey)   { m_hotkeys.remove(hotkey); }

   ShortcutsConfigurationDlg(HWND hwnd);
   ~ShortcutsConfigurationDlg();

protected:
   void InsertItem(ConfigurableHotkey* hotkey);
   ConfigurableHotkey* GetItemHotkey(int index);
   void SetItemShortcut(int index, int shortcut);
   int GetItemShortcut(int index);
   void BeginEdit(int item);
   void EndEdit();

   LRESULT OnApply();
   void OnClick(LPNMITEMACTIVATE lpnmitem);
   void OnRightClick(LPNMITEMACTIVATE lpnmitem);
   void OnSetFocus();

   bool IsEditing() const                                      { return IsWindowVisible(m_editCtrl) ? true : false; }

   HWND m_hDlg;
   HWND m_editCtrl;
   HWND m_listViewWnd;
   int m_editedItemIndex;

   static list<ConfigurableHotkey*> m_hotkeys;
   static INT_PTR CALLBACK DlgProc(HWND hDlg, UINT message, WPARAM /*wParam*/, LPARAM lParam);
};

#endif /*__HOTKEYCONFIG_H__*/
