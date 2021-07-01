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
// gui_keys.c
// FIXME TODO:
// - Check for arrow keys/keypad arrows
//

#include "gui_local.h"

/*
=================
GUI_KeyUp
=================
*/
void GUI_KeyUp (keyNum_t keyNum)
{
	gui_t		*gui;

	if (!cl_guiState.inputWindow)
		return;

	gui = cl_guiState.inputWindow->shared->cursor.curWindow;
	if (!gui)
		return;
}


/*
=================
GUI_KeyDown
=================
*/
static void GUI_r_WantEnter (gui_t *gui)
{
	gui_t	*child;
	uint32	i;

	if (FRVALUE (gui, FR_WANT_ENTER))
		GUI_QueueTrigger (gui, WEV_ACTION);

	for (i=0, child=gui->childList ; i<gui->numChildren ; child++, i++)
		GUI_r_WantEnter (child);
}
void GUI_KeyDown (keyNum_t keyNum)
{
	gui_t	*gui;

	if (!cl_guiState.inputWindow)
		return;

	gui = cl_guiState.inputWindow->shared->cursor.curWindow;
	if (!gui)
		return;

	switch (keyNum) {
	case K_ESCAPE:
		GUI_QueueTrigger (gui, WEV_ESCAPE);
		break;

	case K_ENTER:
		GUI_r_WantEnter (gui->owner);
	case K_MOUSE1:
		GUI_QueueTrigger (gui, WEV_ACTION);
		break;

	case K_UPARROW:
	case K_KP_UPARROW:
	case K_DOWNARROW:
	case K_KP_DOWNARROW:
	case K_LEFTARROW:
	case K_KP_LEFTARROW:
	case K_RIGHTARROW:
	case K_KP_RIGHTARROW:
		GUI_AdjustCursor (keyNum);
		break;
	}
}
