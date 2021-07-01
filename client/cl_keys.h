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
// cl_keys.h
//

#define MAXCMDLINE	256

typedef enum chatDest_s {
	CHATDEST_BAD,

	CHATDEST_PUB,
	CHATDEST_TEAM,
	CHATDEST_CUSTOM,

	CHATDEST_MAX
} chatDest_t;

// FIXME: this is only here for ../unix/x11_utils.c
struct keyInfo_t {
        char    *bind;                  // action the key is bound to
        bool   down;                   // key up events are sent even if in console mode
        int             repeated;               // if > 1, it is autorepeating
        bool   console;                // if true, can't be typed while in the console
        int             shiftValue;             // key to map to if shift held down when this key is pressed
};

extern bool		key_insertOn;

extern int			key_anyKeyDown;

extern char			key_consoleBuffer[32][MAXCMDLINE];
extern int			key_consoleCursorPos;
extern int			key_consoleEditLine;

extern chatDest_t	key_chatDest;
extern char			key_chatBuffer[32][MAXCMDLINE];
extern int			key_chatCursorPos;
extern int			key_chatEditLine;

extern char			key_customCmd[32];
extern char			key_customPrompt[32];

EKeyDest	Key_GetDest();
void		Key_SetDest(const EKeyDest keyDest);

keyNum_t	Key_StringToKeynum (char *str);
char		*Key_KeynumToString (keyNum_t keyNum);

bool		Key_InsertOn ();
bool		Key_CapslockOn ();
bool		Key_ShiftDown ();
void		Key_Event (int keyNum, bool isDown, uint32 time);
void		Key_ClearStates ();
void		Key_ClearTyping ();

void		Key_SetBinding (keyNum_t keyNum, char *binding);
void		Key_WriteBindings (FILE *f);

void		Key_Init ();
