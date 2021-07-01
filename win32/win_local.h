/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

//
// win_local.h
// Win32-specific Quake header file
//

#ifndef WIN32
# error You should not be including this file on this platform
#endif // WIN32

#include <windows.h>

#ifndef DEDICATED_ONLY
# include <mmreg.h>
# include <mmsystem.h>
# include <winuser.h>
# include <dsound.h>
# include <ctype.h>
# include <commctrl.h>
#endif

#define WINDOW_APP_NAME		APP_FULLNAME
#define WINDOW_CLASS_NAME	APP_NAME

struct winInfo_t {
	uint32		msgTime;			// Used for key events

	bool		isWin9x;			// Running on Win9x based OS
	bool		isWinNT;			// Running on WinNT based OS (NT/2K/XP, etc)
	bool		isWow64;			// Running on a 64-bit version of Windows

	// Window information
	bool		appActive;			// True when not minimized and in the foreground
	bool		appMinimized;		// Minimized (window not active)

	bool		classRegistered;	// Only register the window class once

	HDC			hDC;				// Handle to device context
	HGLRC		hGLRC;				// Handle to GL rendering context
	HINSTANCE	hInstance;
	HWND		hWnd;				// Window handle

	bool		bppChangeAllowed;

	bool		cdsFS;
	DEVMODE		windowDM;

	int			desktopBPP;
	int			desktopWidth;
	int			desktopHeight;
	int			desktopHZ;

	HINSTANCE	oglLibrary;			// HINSTANCE for the OpenGL library
};

extern winInfo_t	sys_winInfo;

//
// win_console.c
//

void	Sys_ConsolePrint (const char *pMsg);

//
// win_input.c
//

void	IN_Activate (const bool bActive);
void	IN_MouseEvent (int state);
