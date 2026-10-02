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

#ifndef __WINDOW_H__
#define __WINDOW_H__

#include "desktop.h"
#include "TrayIconsManager.h"
#include "Transparency.h"
#include "AlwaysOnTop.h"
#include "HidingMethod.h"
#include "BalloonNotif.h"


class Window: public ToolTip::Tool, public TrayIconsManager::TrayIconHandler, public AlwaysOnTop
{
   friend class HidingMethod;
   friend class HidingMethodHide;
   friend class HidingMethodMinimize;
   friend class HidingMethodMove;

public:
   /** Constructor.
    * Builds a Window object from the handle of a window. Settings specific to this window are loaded
    * from registry, if any, and applied. Else, default settings are used.
    */
   Window(HWND hWnd);

   /** Destructor.
    * Performs cleanup: settings are saved to the registry if needed, the window is shown again
    * if it was hidden, and memory/handles are released.
    */
   virtual ~Window();

   /** Move the window to the specified desktop.
    * This function allows to specify the desktop on which the window can be seen.
    * By specifying NULL, the window will be seen on all desktops.
    *
    * @param desk Pointer to the desktop on to which the window belongs (ie, on which the
    * window is displayed), or NULL to make the window always visible (ie, on all desktops)
    */
   void MoveToDesktop(Desktop * desk);

   /** Tell if the window can be seen on the specified desktop.
    * This function returns a boolean, indicating if the window is displayed on the
    * specified desktop. By specifying the NULL desktop, the caller can easily find out
    * if the window can be seen on all desktops.
    * This does not give any information concerning the window state or whatsoever. It
    * simply tells if the windows is "present" on the specified desktop.
    *
    * @param desk Pointer to the desktop on which one wants to know if the window is
    * displayed, or NULL to find out if the window is displayed on all desktops.
    * @retval true if the window is visible on the specified desktop
    * @retval false if the window is not visible on the specified desktop
    * @see IsOnCurrentDesk, GetDesk
    */
   bool IsOnDesk(Desktop * desk) const        { return (m_desk == NULL) || (m_desk == desk); }

   /** Tell if the window is visible on the current desktop.
    * This function work exactly as IsOnDesk(), except that it checks only for the current
    * desktop.
    *
    * @retval true if the window is visible on the specified desktop
    * @retval false if the window is not visible on the specified desktop
    * @see IsOnDesk, GetDesk
    */
   bool IsOnCurrentDesk() const;

   /** Get the desktop on which the window is present.
    * If the window is present on all desktops, the return value is NULL.
    *
    * @return Pointer to the Desktop which the window belongs to, or NULL if the window is
    * visible on all desktops.
    * @see IsOnDesk, IsOnCurrentDesk
    */
   Desktop * GetDesk() const                  { return m_desk; }

   /** Builds the context menu associated with the window.
    * This function creates and initializes a popup menu that can used to perform a variety of actions
    * on the window:
    *   - all regular system menu actions (move, size, close...)
    *   - maximize height/width
    *   - kill
    *   - enable minimize to tray, transparency, present on all desktops
    *   - change the desktop
    *   - display the properties dialog
    *
    * After the menu is displayed, the action can be performed by calling the OnMenuItemSelected method
    * with both the handle of the menu and the id of the selected command. After all this is done, the
    * menu should be destroyed.
    *
    * Notice that it is the responsibility of the caller to do so: the method does not display the menu
    * nor enable the user to select anything. It simply creates the menu in memory.
    *
    * @return Handle to the newly created menu
    * @see OnMenuItemSelected
    */
   HMENU BuildMenu();
   void OnMenuItemSelected(HMENU menu, int cmdId);

   void ShowWindow();
   void HideWindow();
   bool IsHidden() const                      { return m_hidden; }
   HidingMethod * GetHidingMethod() const     { return m_hidingMethod; }

   bool IsMinimizeToTray() const              { return m_MinToTray; }
   void ToggleMinimizeToTray();
   void SetMinimizeToTray(bool totray);
   bool IsIconic() const                      { return IsHidden() ? m_iconic : (::IsIconic(m_hWnd) ? true:false); }
   bool IsInTray() const                      { return IsMinimizeToTray() && IsIconic(); }
   void Restore();

   void ToggleOnTop();

   bool IsOnAllDesktops() const               { return IsOnDesk(NULL); }
   void SetOnAllDesktops(bool onall);
   void ToggleAllDesktops();
   void Activate();
   void Minimize();
   void Maximize();
   void MaximizeHeight();
   void MaximizeWidth();
   void Kill();

   void DisplayWindowProperties();

   bool IsTransparent() const                 { return m_transp.GetTransparencyLevel() != TRANSPARENCY_DISABLED; }
   void SetTransparent(bool transp);
   void ToggleTransparent();
   unsigned char GetTransparencyLevel() const { return m_transpLevel; }
   void SetTransparencyLevel(unsigned char level);

   operator HWND()                            { return m_hWnd; }

   HICON GetIcon(void);
   void InvalidateIcon()                      { m_iconTime = 0; }
   LPCWSTR GetText()
   {
      GetWindowTextW(m_hWnd, m_name, sizeof(m_name)/sizeof(*m_name));
      return m_name;
   }
   void GetRect(LPRECT /*rect*/)  { return; }

   inline HWND GetOwnedWindow() const         { return m_hOwnedWnd; }
   inline static HWND GetOwnedWindow(HWND hWnd);

   /** Tell if the window is being shown/hidden by Virtual Dimension.
    * Changes are performed asynchronously for the windows which do not respond, and
    * the corresponding notifications are received later, so the window state may not
    * be up to date for some time.
    */
   inline bool IsSwitching() const            { return m_switching || HasPendingOperation(); }
   inline void SetSwitching(bool on)          { m_switching = on; }
   bool HasPendingOperation() const;

   inline bool IsMoving() const               { return m_moving; }
   inline void SetMoving(bool moving)         { m_moving = moving; }

   inline bool CheckExists() const            { return IsWindow(m_hWnd) != 0; }

   void OnDelayUpdate();

   /** The window has been minimized by the user (or the application). */
   void OnMinimized();
   /** The window, hidden by Virtual Dimension, has been displayed by the application. */
   void OnShownExternally();

   static void SetTag(HWND hWnd, int val)     { SetPropW(hWnd, s_VDPropertyTag, (HANDLE)(INT_PTR)(val+1)); }
   static void RemTag(HWND hWnd)              { RemovePropW(hWnd, s_VDPropertyTag); }
   static bool HasTag(HWND hWnd)              { return GetPropW(hWnd, s_VDPropertyTag) != NULL; }
   static int GetTag(HWND hWnd)               { return (int)(INT_PTR)GetPropW(hWnd, s_VDPropertyTag) - 1; }

   void FlashWindow(void);
   void UnFlashWindow(void);
   bool IsWindowFlashing(void)                { return m_BallonMsg ? true : false; }

   /** Helpers for the hiding methods: change the window without blocking on unresponsive applications. */
   bool IsResponsive() const                  { return m_responsive; }
   void SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int x, int y, int cx, int cy, UINT flags);
   void ShowWindowCmd(HWND hWnd, int cmd);
   void HideOwnedPopups();
   void ShowOwnedPopups();

protected:
   LRESULT OnTrayIconMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
   void OnContextMenu();
   void InsertMenuItem(HMENU menu, bool checked, HANDLE bmp, UINT id, UINT uIdStr);
   HANDLE LoadBmpRes(int id);
   void BeginOperation();

   enum AutoSettingsModes {
      ASS_DISABLED,
      ASS_AUTOSAVE,
      ASS_SAVED
   };

   void OpenSettings(Settings::Window &settings, bool create=false);
   void EraseSettings();
   void SaveSettings();

   void OnInitSettingsDlg(HWND hDlg);
   void OnApplySettingsBtn(HWND hDlg);
   static INT_PTR CALLBACK SettingsProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

   void OnInitAutoSettingsDlg(HWND hDlg);
   void OnApplyAutoSettingsBtn(HWND hDlg);
   void OnUpdateAutoSettingsUI(HWND hDlg, AutoSettingsModes mode);
   static INT_PTR CALLBACK AutoSettingsProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

   static void OnFlashBallonClick(BalloonNotification::Message msg, LPARAM data);

   HWND m_hWnd;
   HWND m_hOwnedWnd;
   Desktop * m_desk;
   bool m_MinToTray;
   bool m_iconic;
   wchar_t m_name[256];

   Transparency m_transp;
   unsigned char m_transpLevel;

   bool m_autoSaveSettings;
   bool m_autosize;
   bool m_autopos;
   bool m_autodesk;

   wchar_t m_className[256];
   HICON m_hIcon;          ///< Icon of the window (belongs to the process of the window, or is m_hOwnIcon)
   HICON m_hOwnIcon;       ///< Icon created by Virtual Dimension, which must be destroyed
   ULONGLONG m_iconTime;   ///< Time when the icon was retrieved

   BalloonNotification::Message m_BallonMsg;

   DWORD m_dwProcessId;

   bool m_switching;
   bool m_moving;

   bool m_hidden;
   HidingMethod * m_hidingMethod;

   // State used by the hiding methods
   bool m_responsive;                     ///< Does the window respond to messages (evaluated for each show/hide)
   ULONGLONG m_lastOperation;             ///< Time of the last show/hide operation
   bool m_hiddenIconic;                   ///< Was the window minimized when it was hidden ?
   POINT m_hiddenPos;                     ///< Position of the window before it was moved away
   std::vector<HWND> m_hiddenPopups;      ///< Owned windows hidden along with this window

   static HidingMethodHide       s_hider_method;
   static HidingMethodMinimize   s_minimizer_method;
   static HidingMethodMove       s_mover_method;

   static HidingMethod* s_hiding_methods[];

   static const wchar_t s_VDPropertyTag[];
};

HWND Window::GetOwnedWindow(HWND hWnd)
{
   HWND owned = GetWindow(hWnd, GW_ENABLEDPOPUP);
   return owned ? owned : hWnd;
}

#endif /*__WINDOW_H__*/
