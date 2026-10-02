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

#ifndef __HIDINGMETHOD_H__
#define __HIDINGMETHOD_H__

class Window;

/** Strategy used to make a window disappear when its desktop is not displayed.
 * Hide: the window is hidden (like if it was closed). Fastest and cleanest.
 * Minimize: the window is minimized and removed from the taskbar. For applications
 *   which misbehave when hidden.
 * Move: the window is moved out of the screen. For applications which misbehave
 *   when hidden or minimized.
 *
 * The data needed to restore a window is also stored as window properties, so that
 * the windows can be recovered if Virtual Dimension is restarted after a crash.
 */
class HidingMethod
{
public:
   virtual ~HidingMethod()                         { }

   /** Called when a window which was managed by a previous instance of Virtual
    * Dimension (and which may still be hidden) is managed again.
    */
   virtual void Recover(Window * wnd) = 0;
   virtual void Show(Window * wnd) = 0;
   virtual void Hide(Window * wnd) = 0;

   /** Tell if a window hidden with this method is still visible for the system. */
   virtual bool KeepsWindowVisible() const         { return true; }

protected:
   static void SaveExStyle(HWND hWnd, LONG_PTR exStyle);
   static bool LoadExStyle(HWND hWnd, LONG_PTR * exStyle);
   static void ClearExStyle(HWND hWnd);

   /** Remove the window from the taskbar and from the alt-tab list. */
   static void RemoveFromTaskList(Window * wnd);
   /** Undo RemoveFromTaskList(). */
   static void RestoreToTaskList(Window * wnd);
};

class HidingMethodHide: public HidingMethod
{
public:
   virtual void Recover(Window * wnd);
   virtual void Show(Window * wnd);
   virtual void Hide(Window * wnd);
   virtual bool KeepsWindowVisible() const         { return false; }
};

class HidingMethodMinimize: public HidingMethod
{
public:
   virtual void Recover(Window * wnd);
   virtual void Show(Window * wnd);
   virtual void Hide(Window * wnd);
};

class HidingMethodMove: public HidingMethod
{
public:
   virtual void Recover(Window * wnd);
   virtual void Show(Window * wnd);
   virtual void Hide(Window * wnd);
};

#endif /*__HIDINGMETHOD_H__*/
