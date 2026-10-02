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
#include "CmdLine.h"
#include "Resource.h"

using namespace std;

map<wchar_t, CommandLineOption*>& CommandLineOption::GetOptionsMap()
{
   //Function-local static: options are registered by static constructors
   static map<wchar_t, CommandLineOption*> argsmap;
   return argsmap;
}

CommandLineOption::CommandLineOption(wchar_t opcode, UINT resid, ArgType arg): m_resid(resid), m_argType(arg)
{
   CommandLineOption::RegisterOption(opcode, this);
}

CommandLineParser::CommandLineParser(): m_argState(NONE), m_curOption(NULL)
{
}

CommandLineParser::~CommandLineParser()
{
}

static bool CommandLineError(UINT uIdMessage, LPCWSTR arg)
{
   String message = Locale::GetInstance().GetString(uIdMessage);
   if (arg)
      message += arg;
   MessageBox(NULL, message.c_str(), Locale::GetInstance().GetString(IDS_CMDLINE_TITLE), MB_ICONERROR);
   return false;
}

bool CommandLineParser::ProcessArg(LPCTSTR arg)
{
   CommandLineOption * option;
   bool res = true;

   switch(m_argState)
   {
   case NONE:
      if (arg[0] != L'-' && arg[0] != L'/')
         res = CommandLineError(IDS_CMDLINE_NOTANOPTION, arg);
      else if ((m_curOption = CommandLineOption::GetOption(arg[1])) == NULL)
         res = CommandLineError(IDS_CMDLINE_INVALIDOPTION, arg);
      else if (m_curOption->GetArgType() == CommandLineOption::required_argument)
         m_argState = REQARG;
      else if (m_curOption->GetArgType() == CommandLineOption::optional_argument)
         m_argState = OPTARG;
      else
         m_curOption->ParseOption();
      break;

   case REQARG:
      m_curOption->ParseOption(arg);
      m_argState = NONE;
      break;

   case OPTARG:
      if ((arg[0] == L'-' || arg[0] == L'/') && (option = CommandLineOption::GetOption(arg[1])) != NULL)
      {
         m_curOption->ParseOption();   //no argument !
         m_curOption = option;
         if (m_curOption->GetArgType() == CommandLineOption::required_argument)
            m_argState = REQARG;
         else if (m_curOption->GetArgType() == CommandLineOption::optional_argument)
            m_argState = OPTARG;
         else
         {
            m_curOption->ParseOption();
            m_argState = NONE;
         }
      }
      else
      {
         m_curOption->ParseOption(arg);
         m_argState = NONE;
      }
      break;
   }

   return res;
}

bool CommandLineParser::EndParsing()
{
   bool res = true;

   switch(m_argState)
   {
   case NONE:
      //nothing to be done
      break;

   case REQARG:
      res = CommandLineError(IDS_CMDLINE_MISSINGARG, NULL);
      break;

   case OPTARG:
      m_curOption->ParseOption();
      break;
   }

   return res;
}

bool CommandLineParser::ParseCommandLine(LPCWSTR cmdline)
{
   int argc = 0;
   LPWSTR * argv;
   bool res = true;

   //Let Windows split the arguments (it handles quotes the standard way)
   argv = CommandLineToArgvW(cmdline, &argc);
   if (argv == NULL)
      return false;

   m_argState = NONE;
   for(int i = 0; res && i < argc; i++)
      res = ProcessArg(argv[i]);

   if (res)
      res = EndParsing();

   LocalFree(argv);

   return res;
}

CommandLineOption * CommandLineOption::GetOption(wchar_t opcode)
{
   map<wchar_t, CommandLineOption*>::iterator it = GetOptionsMap().find(opcode);
   return it == GetOptionsMap().end() ? NULL : (*it).second;
}

void CommandLineOption::RegisterOption(wchar_t opcode, CommandLineOption* option)
{
   GetOptionsMap()[opcode] = option;
}
