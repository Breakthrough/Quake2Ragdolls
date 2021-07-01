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
// cg_keys.c
//

#include "cg_local.h"

/*
=================
CG_KeyEvent
=================
*/
void CG_KeyEvent (keyNum_t keyNum, bool isDown)
{
	if (!isDown)
		return;

	if (keyNum == K_ESCAPE) {
		switch (cgi.Key_GetDest ()) {
		case KD_MENU:
			UI_KeyDown (keyNum, isDown);
			break;

		case KD_GAME:
			if (cg.frame.playerState.stats[STAT_LAYOUTS]) {
				// Put away help computer / inventory
				cgi.Cbuf_AddText ("cmd putaway\n");
				break;
			}

			// Bring up the menu
			UI_MainMenu_f ();
			break;
		}

		return;
	}

	switch (cgi.Key_GetDest ()) {
	case KD_MENU:
		UI_KeyDown (keyNum, isDown);
		break;
	}
}
