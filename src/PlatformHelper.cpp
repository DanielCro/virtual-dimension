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
#include "PlatformHelper.h"
#include <wincodec.h>
#include <propkey.h>
#include <propsys.h>
#include <shlwapi.h>
#include <stdio.h>
#include <share.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

#ifdef DEBUG
void TraceMessage(LPCWSTR format, ...)
{
   wchar_t buffer[512];
   va_list args;

   va_start(args, format);
   _vsnwprintf_s(buffer, _TRUNCATE, format, args);
   va_end(args);

   OutputDebugStringW(L"VD: ");
   OutputDebugStringW(buffer);
   OutputDebugStringW(L"\n");

   //Also keep a log file, which is handy to understand what happened
   static FILE * log = NULL;
   if (log == NULL)
   {
      wchar_t path[MAX_PATH];
      GetTempPathW(MAX_PATH, path);
      wcscat_s(path, L"VirtualDimension-debug.log");
      log = _wfsopen(path, L"w, ccs=UTF-8", _SH_DENYWR);
   }
   if (log)
   {
      SYSTEMTIME time;
      GetLocalTime(&time);
      fwprintf(log, L"%02d:%02d:%02d.%03d %s\n", time.wHour, time.wMinute, time.wSecond, time.wMilliseconds, buffer);
      fflush(log);
   }
}
#endif

DWORD PlatformHelper::GetWindowFileName(HWND hWnd, LPWSTR lpFileName, int nBufLen)
{
   DWORD pId = 0;
   DWORD size = nBufLen;
   HANDLE hProcess;

   if (nBufLen <= 0)
      return 0;
   *lpFileName = 0;

   GetWindowThreadProcessId(hWnd, &pId);
   hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pId);
   if (!hProcess)
      return 0;

   if (!QueryFullProcessImageNameW(hProcess, 0, lpFileName, &size))
   {
      *lpFileName = 0;
      size = 0;
   }
   CloseHandle(hProcess);

   return size;
}

bool PlatformHelper::IsWindowCloaked(HWND hWnd)
{
   DWORD cloaked = 0;
   return SUCCEEDED(DwmGetWindowAttribute(hWnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked != 0;
}

static bool GetProcessIntegrityLevel(HANDLE hProcess, DWORD * level)
{
   HANDLE hToken;
   BYTE buffer[sizeof(TOKEN_MANDATORY_LABEL) + SECURITY_MAX_SID_SIZE];
   DWORD length;
   bool res = false;

   if (!OpenProcessToken(hProcess, TOKEN_QUERY, &hToken))
      return false;

   if (GetTokenInformation(hToken, TokenIntegrityLevel, buffer, sizeof(buffer), &length))
   {
      PSID sid = ((TOKEN_MANDATORY_LABEL*)buffer)->Label.Sid;
      *level = *GetSidSubAuthority(sid, *GetSidSubAuthorityCount(sid) - 1);
      res = true;
   }
   CloseHandle(hToken);

   return res;
}

bool PlatformHelper::CanManageWindow(HWND hWnd)
{
   static DWORD ourLevel = MAXDWORD;
   DWORD pId = 0;
   DWORD level;
   HANDLE hProcess;
   bool res;

   if (ourLevel == MAXDWORD && !GetProcessIntegrityLevel(GetCurrentProcess(), &ourLevel))
      ourLevel = SECURITY_MANDATORY_MEDIUM_RID;

   GetWindowThreadProcessId(hWnd, &pId);
   hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pId);
   if (!hProcess)
      return false;

   //If the token cannot be queried, the process is most likely elevated (or protected)
   res = GetProcessIntegrityLevel(hProcess, &level) && level <= ourLevel;
   CloseHandle(hProcess);

   return res;
}

bool PlatformHelper::IsWindowResponsive(HWND hWnd, UINT timeout)
{
   DWORD_PTR result;
   return SendMessageTimeoutW(hWnd, WM_NULL, 0, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK, timeout, &result) != 0;
}

/** Convert a 32bpp premultiplied-alpha bitmap to an icon. */
static HICON BitmapToIcon(HBITMAP hBitmap)
{
   ICONINFO ii;
   SIZE size = PlatformHelper::GetBitmapSize(hBitmap);
   HICON hIcon;

   ii.fIcon = TRUE;
   ii.xHotspot = ii.yHotspot = 0;
   ii.hbmColor = hBitmap;
   ii.hbmMask = CreateBitmap(size.cx, size.cy, 1, 1, NULL);   //ignored: the color bitmap has an alpha channel
   hIcon = CreateIconIndirect(&ii);
   DeleteObject(ii.hbmMask);

   return hIcon;
}

HICON PlatformHelper::GetAppIconForWindow(HWND hWnd, int size)
{
   ComPtr<IPropertyStore> store;
   PROPVARIANT value;
   HICON hIcon = NULL;

   if (FAILED(SHGetPropertyStoreForWindow(hWnd, IID_PPV_ARGS(&store))))
      return NULL;

   PropVariantInit(&value);
   if (SUCCEEDED(store->GetValue(PKEY_AppUserModel_ID, &value)) && value.vt == VT_LPWSTR && value.pwszVal && *value.pwszVal)
   {
      ComPtr<IShellItemImageFactory> factory;
      if (SUCCEEDED(SHCreateItemInKnownFolder(FOLDERID_AppsFolder, KF_FLAG_DONT_VERIFY, value.pwszVal, IID_PPV_ARGS(&factory))))
      {
         HBITMAP hBitmap;
         SIZE sz = { size, size };
         if (SUCCEEDED(factory->GetImage(sz, SIIGBF_ICONONLY | SIIGBF_BIGGERSIZEOK, &hBitmap)))
         {
            hIcon = BitmapToIcon(hBitmap);
            DeleteObject(hBitmap);
         }
      }
   }
   PropVariantClear(&value);

   return hIcon;
}

static ComPtr<IWICImagingFactory> GetImagingFactory()
{
   ComPtr<IWICImagingFactory> factory;
   CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
   return factory;
}

static HBITMAP DecodeImage(IWICImagingFactory * factory, IWICBitmapDecoder * decoder, int width, int height)
{
   ComPtr<IWICBitmapFrameDecode> frame;
   ComPtr<IWICFormatConverter> converter;
   ComPtr<IWICBitmapSource> source;
   UINT cx, cy;

   if (FAILED(decoder->GetFrame(0, &frame)) ||
       FAILED(factory->CreateFormatConverter(&converter)) ||
       FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone,
                                    NULL, 0.0, WICBitmapPaletteTypeCustom)))
      return NULL;
   source = converter;

   if (width > 0 && height > 0)
   {
      ComPtr<IWICBitmapScaler> scaler;
      if (FAILED(factory->CreateBitmapScaler(&scaler)) ||
          FAILED(scaler->Initialize(source.Get(), width, height, WICBitmapInterpolationModeFant)))
         return NULL;
      source = scaler;
   }

   if (FAILED(source->GetSize(&cx, &cy)) || cx == 0 || cy == 0)
      return NULL;

   BITMAPINFO bmi = {};
   bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
   bmi.bmiHeader.biWidth = cx;
   bmi.bmiHeader.biHeight = -(LONG)cy;   //top-down
   bmi.bmiHeader.biPlanes = 1;
   bmi.bmiHeader.biBitCount = 32;
   bmi.bmiHeader.biCompression = BI_RGB;

   void * bits;
   HBITMAP hBitmap = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
   if (!hBitmap)
      return NULL;

   if (FAILED(source->CopyPixels(NULL, cx*4, cx*cy*4, (BYTE*)bits)))
   {
      DeleteObject(hBitmap);
      return NULL;
   }

   return hBitmap;
}

HBITMAP PlatformHelper::LoadImageFile(LPCWSTR fileName, int width, int height)
{
   ComPtr<IWICImagingFactory> factory = GetImagingFactory();
   ComPtr<IWICBitmapDecoder> decoder;

   if (!factory || !fileName || !*fileName ||
       FAILED(factory->CreateDecoderFromFilename(fileName, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder)))
      return NULL;

   return DecodeImage(factory.Get(), decoder.Get(), width, height);
}

HBITMAP PlatformHelper::LoadImageResource(LPCWSTR name, LPCWSTR type, int width, int height)
{
   ComPtr<IWICImagingFactory> factory = GetImagingFactory();
   ComPtr<IWICBitmapDecoder> decoder;
   ComPtr<IStream> stream;
   HRSRC hrsrc;
   HGLOBAL hres;

   if (!factory ||
       (hrsrc = FindResourceW(NULL, name, type)) == NULL ||
       (hres = LoadResource(NULL, hrsrc)) == NULL)
      return NULL;

   stream.Attach(SHCreateMemStream((const BYTE*)LockResource(hres), SizeofResource(NULL, hrsrc)));
   if (!stream ||
       FAILED(factory->CreateDecoderFromStream(stream.Get(), NULL, WICDecodeMetadataCacheOnDemand, &decoder)))
      return NULL;

   return DecodeImage(factory.Get(), decoder.Get(), width, height);
}

SIZE PlatformHelper::GetBitmapSize(HBITMAP hBitmap)
{
   BITMAP bm;
   SIZE size = { 0, 0 };

   if (hBitmap && GetObject(hBitmap, sizeof(bm), &bm))
   {
      size.cx = bm.bmWidth;
      size.cy = abs(bm.bmHeight);
   }

   return size;
}

void PlatformHelper::DrawBitmap(HDC hdc, HBITMAP hBitmap, const RECT& rect, bool stretch)
{
   SIZE size = GetBitmapSize(hBitmap);
   LONG width = rect.right - rect.left;
   LONG height = rect.bottom - rect.top;
   LONG x, y, cx, cy;

   if (size.cx == 0 || size.cy == 0 || width <= 0 || height <= 0)
      return;

   if (stretch)
   {
      x = rect.left;
      y = rect.top;
      cx = width;
      cy = height;
   }
   else
   {
      //Scale down (never up) while keeping the aspect ratio, and center
      cx = size.cx;
      cy = size.cy;
      if (cx > width)
      {
         cy = MulDiv(cy, width, cx);
         cx = width;
      }
      if (cy > height)
      {
         cx = MulDiv(cx, height, cy);
         cy = height;
      }
      x = rect.left + (width - cx) / 2;
      y = rect.top + (height - cy) / 2;
   }

   HDC memDC = CreateCompatibleDC(hdc);
   HGDIOBJ oldBmp = SelectObject(memDC, hBitmap);
   BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
   ::AlphaBlend(hdc, x, y, cx, cy, memDC, 0, 0, size.cx, size.cy, bf);
   SelectObject(memDC, oldBmp);
   DeleteDC(memDC);
}

void PlatformHelper::AlphaBlend(HDC hdcDest, int nXOriginDest, int nYOriginDest,
                                HDC hdcSrc, int nXOriginSrc, int nYOriginSrc,
                                int nWidth, int nHeight, BYTE sourceAlpha)
{
   BLENDFUNCTION bf;
   bf.AlphaFormat = 0;
   bf.BlendFlags = 0;
   bf.BlendOp = AC_SRC_OVER;
   bf.SourceConstantAlpha = sourceAlpha;

   ::AlphaBlend(hdcDest, nXOriginDest, nYOriginDest, nWidth, nHeight, hdcSrc,
                nXOriginSrc, nYOriginSrc, nWidth, nHeight, bf);
}

int PlatformHelper::ScaleForWindow(HWND hWnd, int value)
{
   UINT dpi = hWnd ? GetDpiForWindow(hWnd) : 0;
   if (dpi == 0)
      dpi = GetDpiForSystem();
   return MulDiv(value, dpi, USER_DEFAULT_SCREEN_DPI);
}

static RECT GetMonitorWorkArea(HMONITOR hMonitor)
{
   MONITORINFO mi;
   mi.cbSize = sizeof(mi);
   if (!GetMonitorInfo(hMonitor, &mi))
      SystemParametersInfo(SPI_GETWORKAREA, 0, &mi.rcWork, 0);
   return mi.rcWork;
}

RECT PlatformHelper::GetWorkArea(HWND hWnd)
{
   return GetMonitorWorkArea(MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST));
}

RECT PlatformHelper::GetWorkArea(const RECT& rect)
{
   return GetMonitorWorkArea(MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST));
}

RECT PlatformHelper::GetVirtualScreen()
{
   RECT rect;
   rect.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
   rect.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
   rect.right = rect.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
   rect.bottom = rect.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
   return rect;
}
