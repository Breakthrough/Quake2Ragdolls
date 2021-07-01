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
Foundation, Inc., 59 Temple Place - Suite 330, v
*/

//
// cl_console.c
//

#include "cl_local.h"

#define CON_TEXTSIZE	65536
#define CON_MAXTIMES	128

#define CON_NOTIFYTIMES	( clamp (con_notifylines->intVal, 1, CON_MAXTIMES) )

struct console_t {
	bool		initialized;

	char		text[CON_TEXTSIZE];		// console text
	float		times[CON_MAXTIMES];	// cls.realTime time the line was generated
										// for transparent notify lines

	int			orMask;

	bool		carriageReturn;			// last newline was '\r'
	int			xOffset;

	int			lastColor;				// color before last newline
	int			lastStyle;				// style before last newline
	int			currentLine;			// line where next message will be printed
	int			display;				// bottom of console displays this line

	float		visLines;
	int			totalLines;				// total lines in console scrollback
	int			lineWidth;				// characters across screen

	float		currentDrop;			// aproaches con_DropHeight at scr_conspeed->floatVal
	float		dropHeight;				// 0.0 to 1.0 lines of console to display
};

static console_t	cl_console;
static console_t	cl_chatConsole;

/*
================
CL_ClearNotifyLines
================
*/
void CL_ClearNotifyLines ()
{
	memset (&cl_console.times, 0, sizeof(cl_console.times));
	memset (&cl_chatConsole.times, 0, sizeof(cl_chatConsole.times));
}


/*
================
CL_ConsoleClose
================
*/
void CL_ConsoleClose ()
{
	cl_console.currentDrop = 0;
	cl_chatConsole.currentDrop = 0;

	Key_ClearTyping ();
	CL_ClearNotifyLines ();
	Key_ClearStates ();
}


/*
================
CL_MoveConsoleDisplay
================
*/
void CL_MoveConsoleDisplay (int value)
{
	cl_console.display += value;
	if (cl_console.display > cl_console.currentLine)
		cl_console.display = cl_console.currentLine;
}


/*
================
CL_SetConsoleDisplay
================
*/
void CL_SetConsoleDisplay (bool top)
{
	cl_console.display = cl_console.currentLine;
	if (top)
		cl_console.display -= cl_console.totalLines + 10;
}


/*
================
CL_ConsoleCheckResize

If the line width has changed, reformat the buffer.
================
*/
static void CL_ResizeConsole (console_t *console)
{
	int		width, oldWidth;
	int		numLines, numChars;
	int		oldTotalLines;
	int		i, j;
	char	tempbuf[CON_TEXTSIZE];
	vec2_t	charSize;

	if (cls.refConfig.vidWidth < 1) {
		// Video hasn't been initialized yet
		console->lineWidth = (640 / 8) - 2;
		console->totalLines = CON_TEXTSIZE / console->lineWidth;
		memset (console->text, ' ', CON_TEXTSIZE);
	}
	else {
		R_GetFontDimensions (NULL, 0, 0, 0, charSize);

		width = (cls.refConfig.vidWidth / charSize[0]) - 2;
		if (width == console->lineWidth)
			return;

		oldWidth = console->lineWidth;
		console->lineWidth = width;

		oldTotalLines = console->totalLines;
		console->totalLines = CON_TEXTSIZE / console->lineWidth;

		numLines = oldTotalLines;
		if (console->totalLines < numLines)
			numLines = console->totalLines;

		numChars = oldWidth;
		if (console->lineWidth < numChars)
			numChars = console->lineWidth;

		memcpy (tempbuf, console->text, CON_TEXTSIZE);
		memset (console->text, ' ', CON_TEXTSIZE);

		for (i=0 ; i<numLines ; i++) {
			for (j=0 ; j<numChars ; j++) {
				console->text[(console->totalLines - 1 - i) * console->lineWidth + j] =
					tempbuf[((console->currentLine - i + oldTotalLines) % oldTotalLines) * oldWidth + j];
			}
		}

		CL_ClearNotifyLines ();
	}

	console->currentLine = max (1, console->totalLines - 1);
	console->display = console->currentLine;
}

void CL_ConsoleCheckResize ()
{
	CL_ResizeConsole (&cl_console);
	CL_ResizeConsole (&cl_chatConsole);
}


/*
===============
CL_ConsoleLinefeed
===============
*/
static void CL_ConsoleLinefeed (console_t *console, bool skipNotify)
{
	// Line feed
	if (console->display == console->currentLine)
		console->display++;

	console->currentLine++;
	memset (&console->text[(console->currentLine%console->totalLines)*console->lineWidth], ' ', console->lineWidth);

	// Set the time for notify lines
	console->times[console->currentLine % CON_MAXTIMES] = (skipNotify) ? 0 : cls.realTime;
}


/*
================
CL_ConsolePrintf

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the text will appear at the top of the game window
================
*/
static void CL_PrintToConsole (console_t *console, comPrint_t flags, const char *txt)
{
	int			y, l;
	int			orMask;

	if (!console->initialized)
		return;

	if (txt[0] == 1 || txt[0] == 2) {
		orMask = 128;
		txt++;
	}
	else
		orMask = 0;

	while (*txt) {
		// Count word length
		for (l=0 ; l<console->lineWidth ; l++) {
			if (txt[l] <= ' ')
				break;
		}

		// Word wrap
		if (l != console->lineWidth && console->xOffset + l > console->lineWidth)
			console->xOffset = 0;

		if (console->carriageReturn) {
			console->currentLine = max (1, console->currentLine - 1);
			console->carriageReturn = false;
		}

		if (!console->xOffset) {
			// Feed a line
			CL_ConsoleLinefeed (console, (flags & PRNT_CONSOLE) ? true : false);

			y = console->currentLine % console->totalLines;
			if (console->lastColor != -1 && y*console->lineWidth < CON_TEXTSIZE-2) {
				console->text[y*console->lineWidth] = COLOR_ESCAPE;
				console->text[y*console->lineWidth+1] = '0' + console->lastColor;
				console->xOffset += 2;
			}

			if (console->lastStyle != -1 && y*console->lineWidth+console->xOffset < CON_TEXTSIZE-2) {
				console->text[y*console->lineWidth+console->xOffset] = COLOR_ESCAPE;
				console->text[y*console->lineWidth+console->xOffset+1] = console->lastStyle;
				console->xOffset += 2;
			}
		}

		switch (*txt) {
		case '\n':
			console->lastColor = -1;
			console->lastStyle = -1;
			console->xOffset = 0;
			break;

		case '\r':
			console->lastColor = -1;
			console->lastStyle = -1;
			console->xOffset = 0;
			console->carriageReturn = true;
			break;

		default:
			// Display character and advance
			y = console->currentLine % console->totalLines;
			console->text[y*console->lineWidth+console->xOffset] = *txt | orMask | console->orMask;
			if (++console->xOffset >= console->lineWidth)
				console->xOffset = 0;

			// Get old color/style codes
			if (Q_IsColorString (txt)) {
				switch ((*(txt+1)) & 127) {
				case 's':
				case 'S':
					if (console->lastStyle == 'S')
						console->lastStyle = -1;
					else
						console->lastStyle = 'S';
					break;

				case 'r':
				case 'R':
					console->lastStyle = -1;
					console->lastColor = -1;
					break;

				case COLOR_BLACK:
				case COLOR_RED:
				case COLOR_GREEN:
				case COLOR_YELLOW:
				case COLOR_BLUE:
				case COLOR_CYAN:
				case COLOR_MAGENTA:
				case COLOR_WHITE:
				case COLOR_GREY:
					console->lastColor = Q_StrColorIndex (*(txt+1));
					break;

				default:
					console->lastColor = -1;
					break;
				}
			}
			break;
		}

		txt++;
	}
}

void CL_ConsolePrintf (comPrint_t flags, const char *txt)
{
	// Error/warning color coding
	if (flags & PRNT_ERROR)
		CL_PrintToConsole (&cl_console, flags, S_COLOR_RED);
	else if (flags & PRNT_WARNING)
		CL_PrintToConsole (&cl_console, flags, S_COLOR_YELLOW);

	// High-bit character list
	if (flags & PRNT_CHATHUD) {
		cl_console.orMask = 128;
		cl_chatConsole.orMask = 128;

		// Regular console
		if (con_chatHud->intVal == 2)
			flags |= PRNT_CONSOLE;
	}

	// Regular console
	CL_PrintToConsole (&cl_console, flags, txt);

	// Chat console
	if (flags & PRNT_CHATHUD) {
		CL_PrintToConsole (&cl_chatConsole, flags, txt);

		cl_console.orMask = 0;
		cl_chatConsole.orMask = 0;
	}
}

/*
==============================================================================

	CONSOLE COMMANDS

==============================================================================
*/

/*
================
CL_ClearConsoleText_f
================
*/
static void CL_ClearConsoleText_f ()
{
	// Reset line locations and clear the buffers
	cl_console.currentLine = max (1, cl_console.totalLines - 1);
	cl_console.display = cl_console.currentLine;
	memset (cl_console.text, ' ', CON_TEXTSIZE);

	cl_chatConsole.currentLine = max (1, cl_chatConsole.totalLines - 1);
	cl_chatConsole.display = cl_chatConsole.currentLine;
	memset (cl_chatConsole.text, ' ', CON_TEXTSIZE);
}


/*
================
CL_ConsoleDump_f

Save the console contents out to a file
================
*/
static void CL_ConsoleDump_f ()
{
	int		l, x;
	char	*line;
	FILE	*f;
	char	buffer[1024];
	char	name[MAX_OSPATH];

	if (Cmd_Argc () != 2) {
		Com_Printf (0, "usage: condump <filename>\n");
		return;
	}

	if (strchr (Cmd_Argv (1), '.'))
		Q_snprintfz (name, sizeof(name), "%s/condumps/%s", FS_Gamedir(), Cmd_Argv(1));
	else
		Q_snprintfz (name, sizeof(name), "%s/condumps/%s.txt", FS_Gamedir(), Cmd_Argv (1));

	Com_Printf (0, "Dumped console text to %s.\n", name);
	FS_CreatePath (name);
	f = fopen (name, "w");
	if (!f) {
		Com_Printf (PRNT_ERROR, "ERROR: CL_ConsoleDump_f: couldn't open, make sure the name you attempted is valid (%s).\n", name);
		return;
	}

	// Skip empty lines
	for (l=cl_console.currentLine-cl_console.totalLines+1 ; l<=cl_console.currentLine ; l++) {
		line = cl_console.text + (l%cl_console.totalLines)*cl_console.lineWidth;
		for (x=0 ; x<cl_console.lineWidth ; x++)
			if (line[x] != ' ')
				break;
		if (x != cl_console.lineWidth)
			break;
	}

	// Write the remaining lines
	buffer[cl_console.lineWidth] = 0;
	for ( ; l<=cl_console.currentLine ; l++) {
		line = cl_console.text + (l % cl_console.totalLines) * cl_console.lineWidth;
		strncpy (buffer, line, cl_console.lineWidth);
		for (x=cl_console.lineWidth-1 ; x>=0 ; x--) {
			if (buffer[x] == ' ')
				buffer[x] = '\0';
			else
				break;
		}

		// Handle high-bit text
		for (x=0 ; buffer[x] ; x++)
			buffer[x] &= 127;

		fprintf (f, "%s\n", buffer);
	}

	fclose (f);
}


/*
================
CL_ConsoleOpen
================
*/
bool CL_ConsoleOpen()
{
	return (cl_console.currentDrop > 0);
}


/*
================
CL_ToggleConsole_f
================
*/
void CL_ToggleConsole_f()
{
	static EKeyDest oldKD;

	// Kill the loading plaque
	SCR_EndLoadingPlaque();

	Key_ClearTyping();
	CL_ClearNotifyLines();
	Key_ClearStates();

	if (Key_GetDest() == KD_CONSOLE)
	{
		if (oldKD == KD_MESSAGE)
			Key_SetDest(KD_GAME);
		else
			Key_SetDest(oldKD);
	}
	else
	{
		oldKD = Key_GetDest();
		Key_SetDest(KD_CONSOLE);
	}
}


/*
================
CL_ToggleChat_f
================
*/
static void CL_ToggleChat_f ()
{
	Key_ClearTyping ();

	if (Key_GetDest () == KD_CONSOLE) {
		if (Com_ClientState () == CA_ACTIVE) {
			CL_CGModule_ForceMenuOff ();
			Key_SetDest (KD_GAME);
		}
	}
	else
		Key_SetDest (KD_CONSOLE);

	CL_ClearNotifyLines ();
}


/*
================
CL_MessageMode_f

FIXME: Allow for a 'default' value. eg 'messagemode blah' will open the message prompt starting with "blah".
================
*/
static void CL_MessageMode_f ()
{
	if (Com_ClientState () != CA_ACTIVE)
		return;

	key_chatDest = CHATDEST_PUB;
	Key_SetDest (KD_MESSAGE);
}


/*
================
CL_MessageMode2_f

FIXME: See CL_MessageMode_f's FIXME.
================
*/
static void CL_MessageMode2_f ()
{
	if (Com_ClientState () != CA_ACTIVE)
		return;

	key_chatDest = CHATDEST_TEAM;
	Key_SetDest (KD_MESSAGE);
}


/*
================
CL_MessageModeX_f

FIXME: See CL_MessageMode_f's FIXME.
================
*/
static void CL_MessageModeX_f ()
{
	Com_Printf (0, "%s\n", Cmd_Argv(0));
	if (Com_ClientState () != CA_ACTIVE)
		return;
	if (Cmd_Argc () < 2) {
		Com_Printf (0, "Usage: messagemodex prompt\n");
		return;
	}

	key_chatDest = CHATDEST_CUSTOM;
	Key_SetDest (KD_MESSAGE);

	Q_strncpyz (key_customCmd, Cmd_Argv(1), sizeof(key_customCmd));
	Q_snprintfz (key_customPrompt, sizeof(key_customPrompt), "%s:", Cmd_Argv(1));
}

/*
==============================================================================

	INIT / SHUTDOWN

==============================================================================
*/

/*
================
CL_ConsoleInit
================
*/
void CL_ConsoleInit ()
{
	if (dedicated->intVal)
		return;

	Cmd_AddCommand ("toggleconsole",	0, CL_ToggleConsole_f,		"Toggles displaying the console");
	Cmd_AddCommand ("togglechat",		0, CL_ToggleChat_f,			"");
	Cmd_AddCommand ("messagemode",		0, CL_MessageMode_f,		"Public message");
	Cmd_AddCommand ("messagemode2",		0, CL_MessageMode2_f,		"Team/group message");
	Cmd_AddCommand ("messagemodex",		0, CL_MessageModeX_f,		"Custom prompt");
	Cmd_AddCommand ("clear",			0, CL_ClearConsoleText_f,	"Clears the console buffer");
	Cmd_AddCommand ("condump",			0, CL_ConsoleDump_f,		"Dumps the content of the console to file");

	// Setup the console
	memset (&cl_console, 0, sizeof(console_t));
	cl_console.lineWidth = -1;
	cl_console.totalLines = -1;
	cl_console.lastColor = -1;
	cl_console.lastStyle = -1;

	// Setup the chat console
	memset (&cl_chatConsole, 0, sizeof(console_t));
	cl_chatConsole.lineWidth = -1;
	cl_chatConsole.totalLines = -1;
	cl_chatConsole.lastColor = -1;
	cl_chatConsole.lastStyle = -1;

	// Done
	CL_ConsoleCheckResize ();

	cl_console.initialized = true;
	cl_chatConsole.initialized = true;
}

/*
==============================================================================

	DRAWING

==============================================================================
*/

/*
================
CL_DrawInput

The input line scrolls horizontally if typing goes beyond the right edge
================
*/
static void CL_DrawInput ()
{
	char			*text;
	int				lastColor, lastStyle;
	int				colorCursorPos;
	int				byteOfs;
	int				byteLen;
	vec2_t			charSize;

	if (Key_GetDest () == KD_MENU)
		return;

	// Don't draw anything (always draw if not active)
	if (Key_GetDest () != KD_CONSOLE && Com_ClientState () == CA_ACTIVE)
		return;

	R_GetFontDimensions (NULL, 0, 0, 0, charSize);

	text = key_consoleBuffer[key_consoleEditLine];

	// Convert byte offset to visible character count
	colorCursorPos = Q_ColorCharCount (text, key_consoleCursorPos);

	// Prestep if horizontally scrolling
	if (colorCursorPos >= cl_console.lineWidth + 1) {
		byteOfs = Q_ColorCharOffset (text, colorCursorPos - cl_console.lineWidth);
		lastColor = Q_ColorStrLastColor (text, byteOfs);
		lastStyle = Q_ColorStrLastStyle (text, byteOfs);
		text += byteOfs;
		colorCursorPos = cl_console.lineWidth;
	}
	else {
		lastColor = Q_StrColorIndex (COLOR_WHITE);
		lastStyle = 0;
	}

	byteLen = Q_ColorCharOffset (text, cl_console.lineWidth);

	// Draw it
	R_DrawStringLen (NULL, 8, cl_console.visLines - (charSize[1] * 2), 0, 0, lastStyle, text, byteLen, Q_BStrColorTable[lastColor]);

	// Add the cursor
	if ((Sys_UMilliseconds()>>8)&1)
		R_DrawChar (NULL, 8 + ((colorCursorPos - (key_insertOn ? 0.3 : 0)) * charSize[0]), cl_console.visLines - (charSize[1] * 2),
					0, 0, 0, key_insertOn ? '|' : 11, Q_BColorWhite);
}


/*
================
CL_DrawNotify

Draws the last few lines of output transparently over the game top
================
*/
static void CL_DrawNotify ()
{
	int		v, i, time;
	int		notifyLines;
	int		totalLines;
	char	*str;
	float	newScale;
	vec2_t	charSize;

	colorb notifyColor = Q_BColorWhite;

	// Make larger if desired
	R_GetFontDimensions (NULL, 0, 0, 0, charSize);
	if (con_notifylarge->intVal) {
		newScale = r_fontScale->floatVal * 1.25f;

		charSize[0] *= 1.25f;
		charSize[1] *= 1.25f;
	}
	else {
		newScale = r_fontScale->floatVal;
	}

	// Render notify lines
	if (con_notifylines->intVal) {
		for (totalLines=0, i=cl_console.currentLine ; i>cl_console.currentLine-CON_MAXTIMES+1 ; i--) {
			if (totalLines >= CON_NOTIFYTIMES)
				break;

			time = cl_console.times[i % CON_MAXTIMES];
			if (time == 0)
				continue;
			time = cls.realTime - time;
			if (time > con_notifytime->floatVal * 1000)
				continue;
			totalLines++;
		}

		for (v=charSize[1]*(totalLines-1), notifyLines=0, i=cl_console.currentLine ; i>cl_console.currentLine-CON_MAXTIMES+1 ; i--) {
			if (notifyLines >= CON_NOTIFYTIMES)
				break;

			time = cl_console.times[i % CON_MAXTIMES];
			if (time == 0)
				continue;
			time = cls.realTime - time;
			if (time > con_notifytime->floatVal * 1000)
				continue;
			notifyLines++;

			if (con_notifyfade->intVal)
				notifyColor.A = FloatToByte(0.5f + 0.5f * (con_notifytime->floatVal - (time * 0.001)) / con_notifytime->floatVal);

			R_DrawStringLen (NULL, 8, v, newScale, newScale, 0, cl_console.text + (i % cl_console.totalLines)*cl_console.lineWidth, cl_console.lineWidth, notifyColor);

			v -= charSize[1];
		}
		v = charSize[1] * totalLines;
	}
	else {
		v = 0;
	}

	//
	// Print messagemode input
	//
	if (Key_GetDest () == KD_MESSAGE) {
		int				skip;
		int				lastColor, lastStyle;
		int				colorCursorPos;
		int				byteOfs;

		switch (key_chatDest) {
		case CHATDEST_PUB:
			skip = R_DrawString (NULL, 4, v, newScale, newScale, 0, "say:", Q_BColorWhite) + 1;
			break;
		case CHATDEST_TEAM:
			skip = R_DrawString (NULL, 4, v, newScale, newScale, 0, "say_team:", Q_BColorWhite) + 1;
			break;
		case CHATDEST_CUSTOM:
			skip = R_DrawString (NULL, 4, v, newScale, newScale, 0, key_customPrompt, Q_BColorWhite) + 1;
			break;
		default:
			assert (0);
			break;
		}

		str = key_chatBuffer[key_chatEditLine];

		// Convert byte offset to visible character count
		colorCursorPos = Q_ColorCharCount (str, key_chatCursorPos) + skip + 1;

		// Prestep if horizontally scrolling
		if (colorCursorPos >= (int)(cls.refConfig.vidWidth/charSize[0])) {
			byteOfs = Q_ColorCharOffset (str, colorCursorPos - (int)(cls.refConfig.vidWidth/charSize[0]));

			lastColor = Q_ColorStrLastColor (str, byteOfs);
			lastStyle = Q_ColorStrLastStyle (str, byteOfs);

			str += byteOfs;
		}
		else {
			lastColor = Q_StrColorIndex (COLOR_WHITE);
			lastStyle = 0;
		}

		R_DrawString (NULL, skip * charSize[0], v, newScale, newScale, lastStyle, str, Q_BStrColorTable[lastColor]);

		// Add cursor
		if ((Sys_UMilliseconds()>>8)&1) {
			int charCount = Q_ColorCharCount (key_chatBuffer[key_chatEditLine], key_chatCursorPos) + skip;
			if (charCount > (int)(cls.refConfig.vidWidth/charSize[0]) - 1)
				charCount = (int)(cls.refConfig.vidWidth/charSize[0]) - 1;

			R_DrawChar (NULL, ((charCount - (key_insertOn ? 0.3 : 0)) * charSize[0]), v, newScale, newScale, 0,
						key_insertOn ? '|' : 11, Q_BColorWhite);
		}
	}
}


/*
==================
CL_DrawChatHud
==================
*/
static void CL_DrawChatHud ()
{
	int		totalLines, v, i;
	char	*text;
	vec2_t	charSize;

	if (Com_ClientState () != CA_ACTIVE)
		return;
	if (Key_GetDest () == KD_MENU)
		return;
	if (cls.mapLoading)
		return;

	R_GetFontDimensions (NULL, 0, 0, 0, charSize);

	if (con_chatHudPosY->floatVal < 0)
		v = cls.refConfig.vidHeight - (charSize[1] * -con_chatHudPosY->floatVal);
	else
		v = cls.refConfig.vidHeight + (charSize[1] * con_chatHudPosY->floatVal);

	totalLines = 0;
	for (i=cl_chatConsole.currentLine ; i>cl_chatConsole.currentLine-CON_MAXTIMES+1 ; i--) {
		if (totalLines == con_chatHudLines->intVal)
			break;

		text = cl_chatConsole.text + (i % cl_chatConsole.totalLines)*cl_chatConsole.lineWidth;
		R_DrawStringLen (NULL, con_chatHudPosX->floatVal, v, 0, 0, con_chatHudShadow->intVal ? FS_SHADOW : 0, text, cl_chatConsole.lineWidth, Q_BColorWhite);

		totalLines++;
		v -= charSize[1];
	}
}


/*
==================
CL_RunConsole

Scroll it up or down
==================
*/
static void CL_RunConsole ()
{
	CL_ConsoleCheckResize ();

	// Decide on the height of the console
	if (Key_GetDest () == KD_CONSOLE) {
		cl_console.dropHeight = con_drop->floatVal;
		if (Com_ClientState () == CA_CONNECTING || Com_ClientState () == CA_CONNECTED) {
			cl_console.currentDrop = cl_console.dropHeight;
			return;
		}
	}
	else {
		// None visible
		cl_console.dropHeight = 0;
		if (Com_ClientState () == CA_CONNECTING || Com_ClientState () == CA_CONNECTED) {
			cl_console.currentDrop = cl_console.dropHeight;
			return;
		}
	}
	
	if (cl_console.dropHeight < cl_console.currentDrop) {
		cl_console.currentDrop -= scr_conspeed->floatVal*cls.refreshFrameTime;
		if (cl_console.dropHeight > cl_console.currentDrop)
			cl_console.currentDrop = cl_console.dropHeight;
	}
	else if (cl_console.dropHeight > cl_console.currentDrop) {
		cl_console.currentDrop += scr_conspeed->floatVal*cls.refreshFrameTime;
		if (cl_console.dropHeight < cl_console.currentDrop)
			cl_console.currentDrop = cl_console.dropHeight;
	}
}


/*
================
CL_DrawConsole
================
*/
void CL_DrawConsole ()
{
	float		frac = 0.0f;
	int			i, row;
	float		x, y, lines;
	char		version[32];
	vec2_t		charSize;

	R_GetFontDimensions (NULL, 0, 0, 0, charSize);

	// Advance for next frame
	CL_RunConsole ();

	// Draw the chat hud
	if (con_chatHud->intVal && con_chatHudLines->intVal > 0)
		CL_DrawChatHud ();

	if (cl_console.currentDrop)
		frac = cl_console.currentDrop;
	else if (Com_ClientState () == CA_ACTIVE && (Key_GetDest () == KD_GAME || Key_GetDest () == KD_MESSAGE) && !cls.mapLoading && !cl.cin.time) {
		CL_DrawNotify ();	// Only draw notify in game
		return;
	}

	// Check if it's even visible
	cl_console.visLines = cls.refConfig.vidHeight * clamp (frac, 0, 1);
	if (cl_console.visLines <= 0)
		return;

	// Draw the background
	R_DrawPic (clMedia.consoleMaterial, 0, QuadVertices().SetVertices(0, -(cls.refConfig.vidHeight - cls.refConfig.vidHeight*frac), cls.refConfig.vidWidth, cls.refConfig.vidHeight),
		colorb(255, 255, 255, FloatToByte(con_alpha->floatVal)));

	// Version
	Q_strncpyz (version, APP_FULLNAME, sizeof(version));
	R_DrawString (NULL, cls.refConfig.vidWidth - (strlen(version) * charSize[0]) - 2,
				cl_console.visLines - charSize[1],
				0, 0, FS_SHADOW, version, Q_BColorCyan);

	// Time if desired
	if (con_clock->intVal) {
		char		clockStr[16];
		time_t		ctime;
		struct tm	*ltime;

		ctime = time (NULL);
		ltime = localtime (&ctime);
		strftime (clockStr, sizeof(clockStr), "%I:%M %p", ltime);

		// Kill the initial zero
		if (clockStr[0] == '0') {
			for (i=0 ; i<(int)(strlen (clockStr) + 1) ; i++)
				clockStr[i] = clockStr[i+1];
			clockStr[i+1] = '\0';
		}

		R_DrawString (NULL, cls.refConfig.vidWidth - (strlen (clockStr) * charSize[0]) - 2,
					cl_console.visLines - (charSize[1] * 2),
					0, 0, FS_SHADOW, clockStr, Q_BColorCyan);
	}

	// Draw the console text
	y = cl_console.visLines - (charSize[1] * 3);	// start point (from the bottom)
	lines = (y / charSize[1]);						// lines of text to draw

	// Draw arrows to show the buffer is backscrolled
	if (cl_console.display != cl_console.currentLine) {
		for (x=0 ; x<(cl_console.lineWidth+1)*charSize[0] ; x+=charSize[0]*2)
			R_DrawChar (NULL, x + (charSize[0] * 0.5), y, 0, 0, FS_SHADOW, '^', Q_BColorRed);

		y -= charSize[1];
		lines--;
	}

	// Draw the print, from the bottom up
	for (i=0, row=cl_console.display ; i<lines ; i++, y-=charSize[1], row--) {
		if (row < 0)
			break;
		if (cl_console.currentLine - row >= cl_console.totalLines)
			break;	// past scrollback wrap point
		if (y < -charSize[1])
			break;

		R_DrawStringLen (NULL, 8, y, 0, 0, 0, cl_console.text + (row % cl_console.totalLines) * cl_console.lineWidth, cl_console.lineWidth, Q_BColorWhite);
	}

	// Draw the input prompt, user text, and cursor if desired
	CL_DrawInput ();
}
