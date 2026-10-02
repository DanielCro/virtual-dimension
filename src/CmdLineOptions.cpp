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

#include "stdafx.h"
#include "shellapi.h"
#include "CmdLine.h"
#include "VirtualDimension.h"
#include "Messages.h"
#include <shlwapi.h>


class CommandLineStartApp : public CommandLineOption {
public:
   CommandLineStartApp(wchar_t opcode, UINT resid): CommandLineOption(opcode, resid, required_argument)   {}
   virtual void ParseOption(LPCTSTR arg);
};

class CommandLineSwitchDesktop : public CommandLineOption {
public:
   CommandLineSwitchDesktop(wchar_t opcode, UINT resid): CommandLineOption(opcode, resid, required_argument)   {}
   virtual void ParseOption(LPCTSTR arg);
};

//CommandLineInt g_cmdLineTransp('t', 0, 0, 192, CommandLineOption::optional_argument);	//start the application with transparency enabled
//CommandLineFlag g_cmdLineMinToTray('m', 0);												//start the application with MinToTray flag
CommandLineInt g_cmdLineDesktop(L'd', 0, -1, -1, CommandLineOption::optional_argument);	//Desktop on which to start the application. default is current desktop
CommandLineStartApp g_cmdLineStartApp(L'x', 0);											//Start an application
CommandLineSwitchDesktop g_cmdLineSwitchDesk(L's', 0);									//Switch current desktop

void CommandLineStartApp::ParseOption(LPCTSTR arg)
{
	//start the specified application !!!
	SHELLEXECUTEINFO info = {};
	info.cbSize = sizeof(info);
	info.fMask = SEE_MASK_NOCLOSEPROCESS|SEE_MASK_FLAG_DDEWAIT;
	info.lpFile = arg;
	info.lpDirectory = NULL;
	info.lpParameters = NULL;
	info.lpVerb = NULL;
	info.nShow = SW_SHOW;
	if (ShellExecuteEx(&info) && info.hProcess)
	{
		HWND hWnd = VirtualDimension::FindWindow();
		if (hWnd && g_cmdLineDesktop != -1)
		{
			//Ask the running instance to move the windows of the program to the desktop
			StartOnDesktopRequest request = {};
			wchar_t path[MAX_PATH];
			DWORD size = MAX_PATH;
			COPYDATASTRUCT data;
			DWORD_PTR result;

			request.processId = GetProcessId(info.hProcess);
			request.desktop = g_cmdLineDesktop;
			if (QueryFullProcessImageNameW(info.hProcess, 0, path, &size))
				lstrcpynW(request.program, PathFindFileNameW(path), MAX_PATH);
			else
				lstrcpynW(request.program, PathFindFileNameW(arg), MAX_PATH);

			data.dwData = VD_COPYDATA_STARTONDESKTOP;
			data.cbData = sizeof(request);
			data.lpData = &request;
			SendMessageTimeout(hWnd, WM_COPYDATA, 0, (LPARAM)&data, SMTO_ABORTIFHUNG, 5000, &result);
		}
		CloseHandle(info.hProcess);
	}
}

void CommandLineSwitchDesktop::ParseOption(LPCTSTR arg)
{
	//switch to the specified desktop
	int desk = wcstol(arg, NULL, 0);
	HWND hWnd = VirtualDimension::FindWindow();
	if (hWnd)
	PostMessage(hWnd, WM_VD_SWITCHDESKTOP, 0, desk);
}

