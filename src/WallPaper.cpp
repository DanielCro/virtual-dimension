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
#include "WallPaper.h"
#include "BackgroundColor.h"
#include "PlatformHelper.h"
#include <shobjidl.h>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

/** Worker thread applying the wallpapers.
 * Only the last request matters: intermediate requests are dropped.
 */
class WallPaperApplier
{
public:
   struct Request
   {
      enum { SET_DEFAULT, SET_NONE, SET_IMAGE, STOP } action;
      std::wstring fileName;
      COLORREF color;
   };

   static WallPaperApplier& GetInstance()    { static WallPaperApplier instance; return instance; }

   void Post(const Request& request)
   {
      {
         std::lock_guard<std::mutex> lock(m_mutex);
         if (m_stopped)
            return;
         if (!m_thread.joinable())
            m_thread = std::thread(&WallPaperApplier::ThreadProc, this);
         m_request = request;
         m_pending = true;
      }
      m_cond.notify_one();
   }

   void Stop()
   {
      Request request;
      request.action = Request::STOP;
      request.color = CLR_INVALID;
      Post(request);

      if (m_thread.joinable())
         m_thread.join();
   }

protected:
   WallPaperApplier(): m_pending(false), m_stopped(false), m_changed(false) { }
   ~WallPaperApplier()
   {
      if (m_thread.joinable())
         m_thread.detach();
   }

   void ThreadProc()
   {
      ComPtr<IDesktopWallpaper> wallpaper;

      CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
      CoCreateInstance(CLSID_DesktopWallpaper, NULL, CLSCTX_ALL, IID_PPV_ARGS(&wallpaper));

      for(;;)
      {
         Request request;
         {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cond.wait(lock, [this]{ return m_pending; });
            request = m_request;
            m_pending = false;
            if (request.action == Request::STOP)
               m_stopped = true;
         }

         if (wallpaper)
            Apply(wallpaper.Get(), request);

         if (request.color != CLR_INVALID)
            BackgroundColor::GetInstance().SetColor(request.color);

         if (request.action == Request::STOP)
            break;
      }

      wallpaper.Reset();
      CoUninitialize();
   }

   void Apply(IDesktopWallpaper * wallpaper, const Request& request)
   {
      switch(request.action)
      {
      case Request::SET_DEFAULT:
      case Request::STOP:
         //Restore the wallpapers of Windows, if we changed them
         if (m_changed)
         {
            for(std::map<std::wstring, std::wstring>::iterator it = m_original.begin(); it != m_original.end(); it++)
               wallpaper->SetWallpaper(it->first.c_str(), it->second.c_str());
            m_changed = false;
         }
         break;

      case Request::SET_NONE:
      case Request::SET_IMAGE:
         //Remember the wallpapers of Windows before changing them
         if (!m_changed)
            SaveOriginalWallpapers(wallpaper);
         m_changed = true;
         wallpaper->SetWallpaper(NULL, request.action == Request::SET_IMAGE ? request.fileName.c_str() : L"");
         break;
      }
   }

   void SaveOriginalWallpapers(IDesktopWallpaper * wallpaper)
   {
      UINT count = 0;

      m_original.clear();
      if (FAILED(wallpaper->GetMonitorDevicePathCount(&count)))
         return;

      for(UINT i = 0; i < count; i++)
      {
         LPWSTR monitorId = NULL;
         LPWSTR path = NULL;

         if (SUCCEEDED(wallpaper->GetMonitorDevicePathAt(i, &monitorId)) &&
             SUCCEEDED(wallpaper->GetWallpaper(monitorId, &path)))
            m_original[monitorId] = path ? path : L"";

         CoTaskMemFree(path);
         CoTaskMemFree(monitorId);
      }
   }

   std::thread m_thread;
   std::mutex m_mutex;
   std::condition_variable m_cond;
   Request m_request;
   bool m_pending;
   bool m_stopped;

   //Only used by the worker thread
   bool m_changed;
   std::map<std::wstring, std::wstring> m_original;   //monitor id -> wallpaper
};


WallPaper * WallPaper::m_activeWallPaper = NULL;
std::wstring WallPaper::m_defaultWallpaper;

WallPaper::WallPaper(): m_mode(WP_DEFAULT), m_bkColor(GetSysColor(COLOR_DESKTOP))
{
   if (m_defaultWallpaper.empty())
      RefreshDefaultWallpaper();
}

WallPaper::~WallPaper(void)
{
   if (m_activeWallPaper == this)
      m_activeWallPaper = NULL;
}

LPCWSTR WallPaper::GetDefaultWallpaper()
{
   return m_defaultWallpaper.c_str();
}

void WallPaper::RefreshDefaultWallpaper()
{
   //Only possible while the wallpaper of Windows is displayed
   if (m_activeWallPaper && m_activeWallPaper->m_mode != WP_DEFAULT)
      return;

   wchar_t path[MAX_PATH] = L"";
   SystemParametersInfoW(SPI_GETDESKWALLPAPER, MAX_PATH, path, 0);
   m_defaultWallpaper = path;
}

void WallPaper::Shutdown()
{
   m_activeWallPaper = NULL;
   WallPaperApplier::GetInstance().Stop();
}

void WallPaper::Activate()
{
   if (m_activeWallPaper == this)
      return;

   m_activeWallPaper = this;
   Apply();
}

void WallPaper::Refresh()
{
   if (m_activeWallPaper == this)
      Apply();
}

void WallPaper::SetImage(LPCWSTR fileName)
{
   if (fileName == NULL)
      m_mode = WP_NONE;
   else if (*fileName == 0)
      m_mode = WP_DEFAULT;
   else
   {
      m_mode = WP_IMAGE;
      m_fileName = fileName;
   }

   if (m_activeWallPaper == this)
      Apply();
}

void WallPaper::SetColor(COLORREF bkColor)
{
   if (bkColor == m_bkColor)
      return;

   m_bkColor = bkColor;

   if (m_activeWallPaper == this)
      Apply();
}

void WallPaper::Apply()
{
   WallPaperApplier::Request request;

   switch(m_mode)
   {
   case WP_DEFAULT:  request.action = WallPaperApplier::Request::SET_DEFAULT;  break;
   case WP_NONE:     request.action = WallPaperApplier::Request::SET_NONE;     break;
   case WP_IMAGE:    request.action = WallPaperApplier::Request::SET_IMAGE;    break;
   }
   request.fileName = m_fileName;
   request.color = m_bkColor;

   WallPaperApplier::GetInstance().Post(request);
}
