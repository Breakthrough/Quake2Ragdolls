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
// win_vid.c
//

#define WIN32_LEAN_AND_MEAN	1
#define _WIN32_WINNT 0x0400

#include "../client/cl_local.h"
#include "../renderer/rb_local.h"
#include "win_local.h"
#include "resource.h"

// console variables that we need to access from this module
cVar_t		*vid_xpos;			// x coordinate of window position
cVar_t		*vid_ypos;			// y coordinate of window position
cVar_t		*vid_fullscreen;

cVar_t		*win_noalttab;
cVar_t		*win_noWinKey;

static bool		vid_bAltTabDisabled;

static HHOOK	vid_kbHook;
static bool		vid_bIsActive;
static bool		vid_bQueueRestart;

/*
==============================================================================

	CONSOLE FUNCTIONS

==============================================================================
*/

/*
============
VID_Restart_f
============
*/
static void VID_Restart_f()
{
	vid_bQueueRestart = true;
}


/*
============
VID_Front_f
============
*/
static void VID_Front_f()
{
	SetWindowLong(sys_winInfo.hWnd, GWL_EXSTYLE, WS_EX_TOPMOST);
	SetForegroundWindow(sys_winInfo.hWnd);
}

/*
==============================================================================

	MESSAGE HANDLING

==============================================================================
*/

/*
=================
WIN_ToggleAltTab
=================
*/
static void WIN_ToggleAltTab(const bool bEnable)
{
	if (bEnable)
	{
		if (!vid_bAltTabDisabled)
			return;

		if (sys_winInfo.isWin9x)
		{
			BOOL	old;
			SystemParametersInfo(SPI_SCREENSAVERRUNNING, 0, &old, 0);
		}
		else
		{
			UnregisterHotKey(0, 0);
			UnregisterHotKey(0, 1);
		}

		vid_bAltTabDisabled = false;
	}
	else
	{
		if (vid_bAltTabDisabled)
			return;

		if (sys_winInfo.isWin9x)
		{
			BOOL	old;
			SystemParametersInfo(SPI_SCREENSAVERRUNNING, 1, &old, 0);
		}
		else
		{
			RegisterHotKey(0, 0, MOD_ALT, VK_TAB);
			RegisterHotKey(0, 1, MOD_ALT, VK_RETURN);
		}

		vid_bAltTabDisabled = true;
	}
}


/*
=================
WIN_ToggleWinKey
=================
*/
static LRESULT CALLBACK LowLevelKeyboardProc (int nCode, WPARAM wParam, LPARAM lParam)
{
	KBDLLHOOKSTRUCT	*p;
	bool		isDown;

	if (nCode < 0 || nCode != HC_ACTION)
		return CallNextHookEx (vid_kbHook, nCode, wParam, lParam);

	p = (KBDLLHOOKSTRUCT *)lParam;
	isDown = false;

	switch (wParam)
	{
	case WM_KEYDOWN:
		isDown = true;
	case WM_KEYUP:
		if (sys_winInfo.appActive && (p->vkCode == VK_LWIN || p->vkCode == VK_RWIN))
		{
			if (p->vkCode == VK_LWIN)
				Key_Event (K_LWINKEY, isDown, ((KBDLLHOOKSTRUCT *)lParam)->time);
			else if (p->vkCode == VK_RWIN)
				Key_Event (K_RWINKEY, isDown, ((KBDLLHOOKSTRUCT *)lParam)->time);
			return 1;
		}
		break;
	}

	return CallNextHookEx (vid_kbHook, nCode, wParam, lParam);
}
static void WIN_ToggleWinKey(const bool bEnable)
{
	if (!sys_winInfo.isWinNT)
		return;

	if (!vid_kbHook && bEnable)
	{
		vid_kbHook = SetWindowsHookEx (WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);
		if (!vid_kbHook)
		{
			Com_Printf (PRNT_ERROR, "ERROR: Unable to install low-level keyboard hook!\n");
			Cvar_VariableSetValue (win_noWinKey, 0, true);
		}
		else
		{
			Com_DevPrintf (0, "Low-level keyboard hook installed, windows key disabled\n");
		}
	}
	else if (vid_kbHook && !bEnable)
	{
		Com_DevPrintf (0, "Low-level keyboard hook disabled\n");
		UnhookWindowsHookEx (vid_kbHook);
		vid_kbHook = NULL;
	}
}


/*
====================
VID_AppActivate
====================
*/
static void VID_AppActivate(const bool bActive, const bool bMinimized)
{
	Key_ClearStates();

	const bool bForeground = (bActive && !bMinimized);
	sys_winInfo.appMinimized = bMinimized;
	sys_winInfo.appActive = bForeground;

	// Notify sub systems
	CL_Activate(bForeground);
	IN_Activate(bForeground);
	SndImp_Activate(bForeground);

	if (win_noalttab->intVal)
		WIN_ToggleAltTab(!bForeground);
	if (win_noWinKey->intVal)
		WIN_ToggleWinKey(bForeground);

	GLimp_AppActivate(bForeground); // FIXME was bActive, should it be this instead?
}


/*
============
VID_UpdateWindowPosAndSize
============
*/
static void VID_UpdateWindowPosAndSize()
{
	if (!vid_xpos->modified && !vid_ypos->modified)
		return;

	if (!ri.config.vidFullScreen)
	{
		RECT	rect;
		int		style;
		int		w, h;

		rect.left	= 0;
		rect.top	= 0;
		rect.right	= ri.config.vidWidth;
		rect.bottom	= ri.config.vidHeight;

		style = GetWindowLong (sys_winInfo.hWnd, GWL_STYLE);
		AdjustWindowRect (&rect, style, FALSE);

		w = rect.right - rect.left;
		h = rect.bottom - rect.top;

		MoveWindow (sys_winInfo.hWnd, vid_xpos->intVal, vid_ypos->intVal, w, h, true);

		ri.def.width = w;
		ri.def.height = h;
	}

	vid_xpos->modified = false;
	vid_ypos->modified = false;
}


/*
====================
MainWndProc

Main window procedure
====================
*/
#ifndef WM_MOUSEWHEEL
# define WM_MOUSEWHEEL		(WM_MOUSELAST+1)	// message that will be supported by the OS
#endif // WM_MOUSEWHEEL

#ifndef WM_MOUSEHWHEEL
# define WM_MOUSEHWHEEL		0x020E
#endif // WM_MOUSEHWHEEL

#ifndef WM_XBUTTONDOWN
# define WM_XBUTTONDOWN		0x020B
# define WM_XBUTTONUP		0x020C
#endif // WM_XBUTTONDOWN

#ifndef MK_XBUTTON1
# define MK_XBUTTON1		0x0020
# define MK_XBUTTON2		0x0040
#endif // MK_XBUTTON1

LRESULT CALLBACK MainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static UINT MSH_MOUSEWHEEL;
	int		state;
	int		width;
	int		height;

	if (uMsg == MSH_MOUSEWHEEL)
	{
		// for Win95
		if (((int) wParam) > 0)
		{
			Key_Event (K_MWHEELUP, true, sys_winInfo.msgTime);
			Key_Event (K_MWHEELUP, false, sys_winInfo.msgTime);
		}
		else
		{
			Key_Event (K_MWHEELDOWN, true, sys_winInfo.msgTime);
			Key_Event (K_MWHEELDOWN, false, sys_winInfo.msgTime);
		}
		goto end;
	}

	switch(uMsg)
	{
	case WM_MOUSEWHEEL:
		// this chunk of code theoretically only works under NT4 and Win98
		// since this message doesn't exist under Win95
		if ((sint16)HIWORD (wParam) > 0)
		{
			Key_Event (K_MWHEELUP, true, sys_winInfo.msgTime);
			Key_Event (K_MWHEELUP, false, sys_winInfo.msgTime);
		}
		else
		{
			Key_Event (K_MWHEELDOWN, true, sys_winInfo.msgTime);
			Key_Event (K_MWHEELDOWN, false, sys_winInfo.msgTime);
		}
		goto end;

	case WM_MOUSEHWHEEL:
		if ((sint16)HIWORD (wParam) > 0)
		{
			Key_Event (K_MWHEELRIGHT, true, sys_winInfo.msgTime);
			Key_Event (K_MWHEELRIGHT, false, sys_winInfo.msgTime);
		}
		else
		{
			Key_Event (K_MWHEELLEFT, true, sys_winInfo.msgTime);
			Key_Event (K_MWHEELLEFT, false, sys_winInfo.msgTime);
		}
		Com_Printf (0, "WM_MOUSE H WHEEL\n");
		goto end;

	case WM_HOTKEY:
		return 0;

	case WM_CREATE:
		MSH_MOUSEWHEEL = RegisterWindowMessage("MSWHEEL_ROLLMSG");
		goto end;

	case WM_PAINT:
		// force entire screen to update next frame
		// FIXME: this necessary?
		// SCR_UpdateScreen ();
		goto end;

	case WM_DESTROY:
		// let sound and input know about this?
		sys_winInfo.hWnd = NULL;
		goto end;

	case WM_ACTIVATE:
		// KJB: Watch this for problems in fullscreen modes with Alt-tabbing.
		VID_AppActivate ((LOWORD(wParam) != WA_INACTIVE) ? true : false, HIWORD (wParam) ? true : false);
		goto end;

	case WM_MOVE:
		if (!ri.config.vidFullScreen)
		{
			int		xPos, yPos;
			RECT	r;
			int		style;

			xPos = (sint16) LOWORD (lParam);		// horizontal position
			yPos = (sint16) HIWORD (lParam);		// vertical position

			r.left		= 0;
			r.top		= 0;
			r.right		= 1;
			r.bottom	= 1;

			style = GetWindowLong (hWnd, GWL_STYLE);
			AdjustWindowRect (&r, style, FALSE);

			Cvar_VariableSetValue (vid_xpos, xPos + r.left, true);
			Cvar_VariableSetValue (vid_ypos, yPos + r.top, true);
			vid_xpos->modified = false;
			vid_ypos->modified = false;

			if (sys_winInfo.appActive)
				IN_Activate (true);
		}
		goto end;

	// this is complicated because Win32 seems to pack multiple mouse events into
	// one update sometimes, so we always check all states and look for events
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MOUSEMOVE:
	case WM_XBUTTONDOWN:
	case WM_XBUTTONUP:
		state = 0;

		if (wParam & MK_LBUTTON)	state |= 1;
		if (wParam & MK_RBUTTON)	state |= 2;
		if (wParam & MK_MBUTTON)	state |= 4;
		if (wParam & MK_XBUTTON1)	state |= 8;
		if (wParam & MK_XBUTTON2)	state |= 16;

		IN_MouseEvent (state);

		goto end;

	case WM_SYSCOMMAND:
		switch (wParam)
		{
		case SC_MONITORPOWER:
		case SC_SCREENSAVE:
		case SC_KEYMENU:
			return 0;

		case SC_CLOSE:
			Cbuf_AddText ("quit\n");
			return 0;
		}
		goto end;

	case WM_SYSKEYDOWN:
		if (wParam == 13)
		{
			Cvar_VariableSetValue (vid_fullscreen, !vid_fullscreen->intVal, true);
			VID_Restart_f ();
			return 0;
		}
	// fall through
	case WM_KEYDOWN:
		Key_Event (In_MapKey (wParam, lParam), true, sys_winInfo.msgTime);
		break;

	case WM_SYSKEYUP:
	case WM_KEYUP:
		Key_Event (In_MapKey (wParam, lParam), false, sys_winInfo.msgTime);
		break;

	case WM_CLOSE:
		Cbuf_AddText ("quit\n");
		return 0;

	case WM_SIZE:
		width = LOWORD (lParam);
		height = HIWORD (lParam);

		if (width > 0 && height > 0)
		{
			// Force a window size update, so that the window dimensions cannot be float.
			ri.config.vidWidth  = width;
			ri.config.vidHeight = height;

			vid_xpos->modified = true;
			vid_ypos->modified = true;

			VID_UpdateWindowPosAndSize ();

			// Update the subsystems
			CL_SetRefConfig ();
		}
		goto end;
	}

end:
	return DefWindowProc (hWnd, uMsg, wParam, lParam);
}

/*
==============================================================================

	WINDOW SETUP

==============================================================================
*/

/*
============
VID_CheckChanges

This function gets called once just before drawing each frame, and it's sole purpose in life
is to check to see if any of the video mode parameters have changed, and if they have to 
update the video modes to match.
============
*/
void VID_CheckChanges(refConfig_t *outConfig)
{
	static HWND oldhWnd = 0;

	if (win_noalttab->modified)
	{
		win_noalttab->modified = false;
		WIN_ToggleAltTab(!win_noalttab->intVal);
	}

	if (win_noWinKey->modified)
	{
		win_noWinKey->modified = false;
		WIN_ToggleWinKey((win_noWinKey->intVal != 0));
	}

	if (vid_bQueueRestart)
	{
		bool mapWasLoaded = cls.mapLoaded;

		CL_MediaShutdown();

		// Refresh has changed
		vid_bQueueRestart = false;
		cls.refreshPrepped = false;
		cls.disableScreen = true;

		// Kill if already active
		if (vid_bIsActive)
		{
			R_ShutdownRefresh(false);
			vid_bIsActive = false;
		}

		// Initialize renderer
		if (!R_InitRefresh())
		{
			R_ShutdownRefresh(true);
			vid_bIsActive = false;
		}

		CL_SetRefConfig();
		R_GetRefConfig(outConfig);

		Snd_Init();
		CL_MediaInit();

		// R1: Restart our input devices as the window handle most likely changed
		if (oldhWnd && sys_winInfo.hWnd != oldhWnd)
			IN_Restart_f ();
		oldhWnd = sys_winInfo.hWnd;

		cls.disableScreen = false;

		CL_ConsoleClose();

		// This is to stop cgame from initializing on first load
		// and so it will load after a vid_restart while connected somewhere
		if (mapWasLoaded)
		{
			CL_CGModule_LoadMap();
			Key_SetDest (KD_GAME);
		}
		else if (Com_ClientState() < CA_CONNECTED)
		{
			CL_CGModule_MainMenu();
		}

		vid_bIsActive = true;
	}

	// Update our window position
	VID_UpdateWindowPosAndSize();
}

/*
==============================================================================

	INIT / SHUTDOWN

==============================================================================
*/

static conCmd_t *cmd_vidRestart;
static conCmd_t *cmd_vidFront;

/*
============
VID_Init
============
*/
void VID_Init(refConfig_t *outConfig)
{
	// Create the video variables so we know how to start the graphics drivers
	vid_xpos		= Cvar_Register("vid_xpos",			"3",	CVAR_ARCHIVE);
	vid_ypos		= Cvar_Register("vid_ypos",			"22",	CVAR_ARCHIVE);
	vid_fullscreen	= Cvar_Register("vid_fullscreen",	"0",	CVAR_ARCHIVE|CVAR_LATCH_VIDEO);

	win_noalttab	= Cvar_Register("win_noalttab",		"0",	CVAR_ARCHIVE);
	win_noWinKey	= Cvar_Register("win_noWinKey",		"0",	CVAR_ARCHIVE);

	// Add some console commands that we want to handle
	cmd_vidRestart	= Cmd_AddCommand("vid_restart",		0, VID_Restart_f,		"Restarts refresh and media");
	cmd_vidFront	= Cmd_AddCommand("vid_front",		0, VID_Front_f,			"");

	// Disable the 3Dfx splash screen
	_putenv("FX_GLIDE_NO_SPLASH=0");

	// Start the graphics mode
	vid_bQueueRestart = true;
	vid_bIsActive = false;
	VID_CheckChanges(outConfig);
}


/*
============
VID_Shutdown
============
*/
void VID_Shutdown()
{
	Cmd_RemoveCommand(cmd_vidRestart);
	Cmd_RemoveCommand(cmd_vidFront);

	if (vid_bIsActive)
	{
		R_ShutdownRefresh(true);
		vid_bIsActive = false;
	}

	if (vid_kbHook)
	{
		UnhookWindowsHookEx(vid_kbHook);
		vid_kbHook = NULL;
	}
}
