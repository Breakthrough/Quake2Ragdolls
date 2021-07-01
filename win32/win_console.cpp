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
// win_console.c
//

#include "../common/common.h"
#include "win_local.h"
#include "resource.h"

#include <errno.h>
#include <float.h>
#include <fcntl.h>
#include <stdio.h>
#include <direct.h>
#include <io.h>
#include <conio.h>

#define COPY_ID			1
#define QUIT_ID			2
#define CLEAR_ID		3

#define ERRORBOX_ID		10
#define ERRORTEXT_ID	11

#define EDIT_ID			100
#define INPUT_ID		101

#define BUFF_BGCOLOR	RGB (0xff, 0xff, 0xff)
#define BUFF_TXTCOLOR	RGB (0x11, 0x11, 0x11)

#define ERR_BGCOLOR		RGB (0xbb, 0xbb, 0xbb)
#define ERR_TXTCOLOR	RGB (0xff, 0x00, 0x00)
#define ERR_TXTCOLOR2	RGB (0xbb, 0x00, 0x00)

#define EDIT_BGCOLOR	RGB (0xff, 0xff, 0xff)

struct winConsole_t {
	HWND		hWnd;
	HWND		hwndBuffer;

	HWND		hwndButtonClear;
	HWND		hwndButtonCopy;
	HWND		hwndButtonQuit;

	HWND		hwndErrorBox;
	HWND		hwndErrorText;

	HBITMAP		hbmLogo;
	HBITMAP		hbmClearBitmap;

	HBRUSH		hbrEditBackground;
	HBRUSH		hbrErrorBackground;

	HFONT		hfBufferFont;

	HWND		hwndInputLine;

	char		errorString[128];

	char		consoleText[512], returnedText[512];
	int			visLevel;
	bool		quitOnClose;
	int			windowWidth, windowHeight;
	
	WNDPROC		SysInputLineWndProc;
};

static winConsole_t winConsole;

/*
==================
Sys_ConWndProc
==================
*/
static LONG WINAPI Sys_ConWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static bool	s_timePolarity;

	switch (uMsg) {
	case WM_ACTIVATE:
		if (LOWORD(wParam) != WA_INACTIVE)
			SetFocus (winConsole.hwndInputLine);
		break;

	case WM_CLOSE:
#ifndef DEDICATED_ONLY
		if (dedicated && dedicated->intVal) {
#endif
			Cbuf_AddText ("quit\n");
#ifndef DEDICATED_ONLY
		}
		else if (winConsole.quitOnClose) {
			PostQuitMessage (0);
		}
		else {
			Sys_ShowConsole (0, false);
		}
#endif
		return 0;

	case WM_CTLCOLORSTATIC:
		if ((HWND)lParam == winConsole.hwndBuffer) {
			SetBkColor ((HDC)wParam, BUFF_BGCOLOR);
			SetTextColor ((HDC)wParam, BUFF_TXTCOLOR);
			return (long)winConsole.hbrEditBackground;
		}
		else if ((HWND)lParam == winConsole.hwndErrorBox) {
			if (s_timePolarity & 1) {
				SetBkColor ((HDC) wParam, ERR_BGCOLOR);
				SetTextColor ((HDC) wParam, ERR_TXTCOLOR);
			}
			else {
				SetBkColor ((HDC)wParam, ERR_BGCOLOR);
				SetTextColor ((HDC)wParam, ERR_TXTCOLOR2);
			}
			return (long)winConsole.hbrErrorBackground;
		}
		break;

	case WM_COMMAND:
		switch (wParam) {
		case COPY_ID:
			SendMessage (winConsole.hwndBuffer, EM_SETSEL, 0, -1);
			SendMessage (winConsole.hwndBuffer, WM_COPY, 0, 0);
			break;

		case QUIT_ID:
			if (winConsole.quitOnClose) {
				PostQuitMessage (0);
			}
			else {
				Cbuf_AddText ("quit\n");
			}
			break;

		case CLEAR_ID:
			SendMessage (winConsole.hwndBuffer, EM_SETSEL, 0, -1);
			SendMessage (winConsole.hwndBuffer, EM_REPLACESEL, FALSE, (LPARAM)"");
			UpdateWindow (winConsole.hwndBuffer);
			break;
		}
		break;

	case WM_CREATE:
		winConsole.hbrEditBackground = CreateSolidBrush (EDIT_BGCOLOR);
		winConsole.hbrErrorBackground = CreateSolidBrush (ERR_BGCOLOR);
		SetTimer (hWnd, 1, 500, NULL);
		break;

	case WM_ERASEBKGND:
	    return DefWindowProc (hWnd, uMsg, wParam, lParam);

	case WM_TIMER:
		if (wParam == 1) {
			s_timePolarity = !s_timePolarity;
			if (winConsole.hwndErrorBox)
				InvalidateRect (winConsole.hwndErrorBox, NULL, FALSE);
		}
		break;
    }

    return DefWindowProc (hWnd, uMsg, wParam, lParam);
}


/*
==================
Sys_InputLineWndProc
==================
*/
static LONG WINAPI Sys_InputLineWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	char inputBuffer[1024];

	switch (uMsg) {
	case WM_KILLFOCUS:
		if ((HWND)wParam == winConsole.hWnd
		|| (HWND)wParam == winConsole.hwndErrorBox) {
			SetFocus (hWnd);
			return 0;
		}
		break;

	case WM_CHAR:
		if (wParam == 13) {
			GetWindowText (winConsole.hwndInputLine, inputBuffer, sizeof(inputBuffer));
			strncat (winConsole.consoleText, inputBuffer, sizeof(winConsole.consoleText)-strlen(winConsole.consoleText)-5);
			strcat (winConsole.consoleText, "\n");
			SetWindowText (winConsole.hwndInputLine, "");

			Sys_Print (Q_VarArgs ("]%s\n", inputBuffer));

			return 0;
		}
	}

	return CallWindowProc (winConsole.SysInputLineWndProc, hWnd, uMsg, wParam, lParam);
}


/*
==================
Sys_CreateConsole
==================
*/
void Sys_CreateConsole ()
{
	HDC			hDC;
	WNDCLASS	wc;
	RECT		rect;
	const char *DEDCLASS = "EGL WinConsole";
	int			nHeight;
	int			swidth, sheight;
	int			DEDSTYLE = WS_POPUPWINDOW | WS_CAPTION | WS_MINIMIZEBOX;

	memset (&wc, 0, sizeof(wc));

	wc.style         = 0;
	wc.lpfnWndProc   = (WNDPROC)Sys_ConWndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = sys_winInfo.hInstance;
	wc.hIcon         = LoadIcon (sys_winInfo.hInstance, MAKEINTRESOURCE (IDI_ICON1));
	wc.hCursor       = LoadCursor (NULL,IDC_ARROW);
	wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
	wc.lpszMenuName  = 0;
	wc.lpszClassName = DEDCLASS;

	if (!RegisterClass (&wc))
		return;

	rect.left = 0;
	rect.right = 540;
	rect.top = 0;
	rect.bottom = 450;
	AdjustWindowRect (&rect, DEDSTYLE, FALSE);

	hDC = GetDC (GetDesktopWindow ());
	swidth = GetDeviceCaps (hDC, HORZRES);
	sheight = GetDeviceCaps (hDC, VERTRES);
	ReleaseDC (GetDesktopWindow (), hDC);

	winConsole.windowWidth = rect.right - rect.left + 1;
	winConsole.windowHeight = rect.bottom - rect.top + 1;

	winConsole.hWnd = CreateWindowEx (0,
							   DEDCLASS,
							   "EGL Console",
							   DEDSTYLE,
							   (swidth - 600) / 2, (sheight - 450) / 2 , rect.right - rect.left + 1, rect.bottom - rect.top + 1,
							   NULL,
							   NULL,
							   sys_winInfo.hInstance,
							   NULL);

	if (winConsole.hWnd == NULL)
		return;

	//
	// create fonts
	//
	hDC = GetDC (winConsole.hWnd);
	nHeight = -MulDiv (8, GetDeviceCaps (hDC, LOGPIXELSY), 72);

	winConsole.hfBufferFont = CreateFont (nHeight,
									  0,
									  0,
									  0,
									  FW_LIGHT,
									  0,
									  0,
									  0,
									  DEFAULT_CHARSET,
									  OUT_DEFAULT_PRECIS,
									  CLIP_DEFAULT_PRECIS,
									  DEFAULT_QUALITY,
									  FF_MODERN | FIXED_PITCH,
									  "Courier New");

	ReleaseDC (winConsole.hWnd, hDC);

	//
	// create the input line
	//
	winConsole.hwndInputLine = CreateWindow ("edit", NULL, WS_CHILD | WS_VISIBLE | WS_BORDER | 
												ES_LEFT | ES_AUTOHSCROLL,
												6, 400, 528, 20,
												winConsole.hWnd, 
												(HMENU)INPUT_ID,	// child window ID
												sys_winInfo.hInstance, NULL);

	//
	// create the buttons
	//
	winConsole.hwndButtonCopy = CreateWindow ("button", NULL, BS_PUSHBUTTON | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
												5, 425, 72, 24,
												winConsole.hWnd, 
												(HMENU)COPY_ID,	// child window ID
												sys_winInfo.hInstance, NULL);
	SendMessage (winConsole.hwndButtonCopy, WM_SETTEXT, 0, (LPARAM)"Copy");

	winConsole.hwndButtonClear = CreateWindow ("button", NULL, BS_PUSHBUTTON | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
												82, 425, 72, 24,
												winConsole.hWnd, 
												(HMENU)CLEAR_ID,	// child window ID
												sys_winInfo.hInstance, NULL);
	SendMessage (winConsole.hwndButtonClear, WM_SETTEXT, 0, (LPARAM)"Clear");

	winConsole.hwndButtonQuit = CreateWindow ("button", NULL, BS_PUSHBUTTON | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
												462, 425, 72, 24,
												winConsole.hWnd, 
												(HMENU)QUIT_ID,	// child window ID
												sys_winInfo.hInstance, NULL );
	SendMessage (winConsole.hwndButtonQuit, WM_SETTEXT, 0, (LPARAM)"Quit");


	//
	// create the scrollbuffer
	//
	winConsole.hwndBuffer = CreateWindow ("edit", NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER | 
												ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
												6, 40, 526, 354,
												winConsole.hWnd, 
												(HMENU) EDIT_ID,	// child window ID
												sys_winInfo.hInstance, NULL);
	SendMessage (winConsole.hwndBuffer, WM_SETFONT, (WPARAM) winConsole.hfBufferFont, 0);

	winConsole.SysInputLineWndProc = (WNDPROC)SetWindowLong (winConsole.hwndInputLine, GWL_WNDPROC, (long)Sys_InputLineWndProc);
	SendMessage (winConsole.hwndInputLine, WM_SETFONT, (WPARAM)winConsole.hfBufferFont, 0);

	ShowWindow (winConsole.hWnd, SW_SHOWDEFAULT);
	UpdateWindow (winConsole.hWnd);
	SetForegroundWindow (winConsole.hWnd);
	SetFocus (winConsole.hwndInputLine);

	winConsole.visLevel = 1;
}


/*
==================
Sys_DestroyConsole
==================
*/
void Sys_DestroyConsole ()
{
	if (!winConsole.hWnd)
		return;

	ShowWindow (winConsole.hWnd, SW_HIDE);
	CloseWindow (winConsole.hWnd);
	DestroyWindow (winConsole.hWnd);
	winConsole.hWnd = 0;
}


/*
==================
Sys_ShowConsole
==================
*/
void Sys_ShowConsole (int visLevel, bool quitOnClose)
{
	winConsole.quitOnClose = quitOnClose;

	if (visLevel == winConsole.visLevel)
		return;
	winConsole.visLevel = visLevel;

	if (!winConsole.hWnd)
		return;

	switch (visLevel) {
	case 0:
		ShowWindow (winConsole.hWnd, SW_HIDE);
		break;

	case 1:
		ShowWindow (winConsole.hWnd, SW_SHOWNORMAL);
		SendMessage (winConsole.hwndBuffer, EM_LINESCROLL, 0, 0xffff);
		break;

	case 2:
		ShowWindow (winConsole.hWnd, SW_MINIMIZE);
		break;

	default:
		Sys_Error ("Invalid visLevel %d sent to Sys_ShowConsole\n", visLevel);
		break;
	}
}


/*
==================
Sys_ConsoleInput
==================
*/
char *Sys_ConsoleInput ()
{
	if (!winConsole.consoleText[0])
		return NULL;

	strcpy (winConsole.returnedText, winConsole.consoleText);
	winConsole.consoleText[0] = 0;

	return winConsole.returnedText;
}


/*
==================
Sys_ConsolePrint
==================
*/
void Sys_ConsolePrint (const char *pMsg)
{
#define CONSOLE_BUFFER_SIZE		16384

	char buffer[CONSOLE_BUFFER_SIZE*2];
	char *b = buffer;
	const char *msg;
	int bufLen;
	int i = 0;
	static unsigned long s_totalChars;

	//
	// if the message is REALLY long, use just the last portion of it
	//
	if (strlen(pMsg) > CONSOLE_BUFFER_SIZE-1) {
		msg = pMsg + strlen (pMsg) - CONSOLE_BUFFER_SIZE + 1;
	}
	else {
		msg = pMsg;
	}

	//
	// copy into an intermediate buffer
	//
	while (msg[i] && (b-buffer < sizeof(buffer)-1)) {
		if (msg[i] == '\n' && msg[i+1] == '\r') {
			b[0] = '\r';
			b[1] = '\n';
			b += 2;
			i++;
		}
		else if (msg[i] == '\r') {
			b[0] = '\r';
			b[1] = '\n';
			b += 2;
		}
		else if (msg[i] == '\n') {
			b[0] = '\r';
			b[1] = '\n';
			b += 2;
		}
		else if (Q_IsColorString (&msg[i])) {
			i++;
		}
		else {
			*b= msg[i];
			b++;
		}
		i++;
	}
	*b = 0;
	bufLen = b - buffer;

	s_totalChars += bufLen;

	//
	// replace selection instead of appending if we're overflowing
	//
	if (s_totalChars > 0x7fff) {
		SendMessage (winConsole.hwndBuffer, EM_SETSEL, 0, -1);
		s_totalChars = bufLen;
	}

	//
	// put this text into the windows console
	//
	SendMessage (winConsole.hwndBuffer, EM_LINESCROLL, 0, 0xffff);
	SendMessage (winConsole.hwndBuffer, EM_SCROLLCARET, 0, 0);
	SendMessage (winConsole.hwndBuffer, EM_REPLACESEL, 0, (LPARAM)buffer);
}


/*
==================
Sys_SetConsoleTitle
==================
*/
void Sys_SetConsoleTitle (const char *title)
{
	if (!winConsole.hWnd)
		return;

	if (!title || !title[0]) {
		SetWindowText (winConsole.hWnd, "EGL Console");
		return;
	}

	SetWindowText (winConsole.hWnd, title);
}


/*
==================
Sys_SetErrorText
==================
*/
void Sys_SetErrorText (const char *buf)
{
	Q_strncpyz (winConsole.errorString, buf, sizeof(winConsole.errorString));

	Sys_ConsolePrint ("********************\n");
	Sys_ConsolePrint ("SYS ERROR: ");
	Sys_ConsolePrint (buf);
	Sys_ConsolePrint ("\n");
	Sys_ConsolePrint ("********************\n");

	if (!winConsole.hwndErrorBox) {
		winConsole.hwndErrorBox = CreateWindow ("static", NULL, WS_CHILD | WS_VISIBLE | SS_SUNKEN,
													6, 5, 526, 30,
													winConsole.hWnd, 
													(HMENU)ERRORBOX_ID,	// child window ID
													sys_winInfo.hInstance, NULL);

		SendMessage (winConsole.hwndErrorBox, WM_SETFONT, (WPARAM)winConsole.hfBufferFont, 0);
		SetWindowText (winConsole.hwndErrorBox, winConsole.errorString);

		DestroyWindow (winConsole.hwndInputLine);
		winConsole.hwndInputLine = NULL;
	}
}
