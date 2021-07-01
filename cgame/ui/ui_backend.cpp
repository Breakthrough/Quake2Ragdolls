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
// ui_backend.c
//

#include "ui_local.h"

uiState_t	uiState;

struct uiLayer_t {
	uiFrameWork_t	*frameWork;
	void			(*drawFunc) ();
	struct sfx_t	*(*closeFunc) ();
	struct sfx_t	*(*keyFunc) (uiFrameWork_t *fw, keyNum_t keyNum);
};

uiLayer_t	ui_layers[MAX_UI_DEPTH];
static int	ui_layerDepth;

cVar_t	*ui_filtermouse;
cVar_t	*ui_sensitivity;

/*
=======================================================================

	UI INIT/SHUTDOWN

=======================================================================
*/

/*
=================
UI_Init
=================
*/
void UI_Init ()
{
	// Register cvars
	ui_filtermouse		= cgi.Cvar_Register ("ui_filtermouse",	"1",		CVAR_ARCHIVE);
	ui_sensitivity		= cgi.Cvar_Register ("ui_sensitivity",	"2",		CVAR_ARCHIVE);

	// Cursor init
	UI_CursorInit ();

	// Menu init
	M_Init ();
}


/*
=================
UI_Shutdown
=================
*/
void UI_Shutdown ()
{
	// Cursor shutdown
	UI_CursorShutdown ();

	// Shut the menu down
	M_Shutdown ();
}

/*
=======================================================================

	INTERFACE HANDLING

=======================================================================
*/

/*
=============
UI_PushInterface
=============
*/
void UI_PushInterface (uiFrameWork_t *frameWork, void (*drawFunc) (), struct sfx_t *(*closeFunc) (), struct sfx_t *(*keyFunc) (uiFrameWork_t *fw, keyNum_t keyNum))
{
	int		i;

	// If this interface is already open, jump to it's level to avoid stacking
	for (i=0 ; i<ui_layerDepth ; i++) {
		if (ui_layers[i].frameWork != frameWork)
			continue;
		if (ui_layers[i].closeFunc != closeFunc)
			continue;
		if (ui_layers[i].drawFunc != drawFunc)
			continue;
		if (ui_layers[i].keyFunc != keyFunc)
			continue;

		ui_layerDepth = i;
	}

	// Create a new layer
	if (i == ui_layerDepth) {
		if (ui_layerDepth >= MAX_UI_DEPTH)
			Com_Error (ERR_FATAL, "UI_PushInterface: MAX_UI_DEPTH");

		ui_layers[ui_layerDepth].frameWork = uiState.activeUI;
		ui_layers[ui_layerDepth].drawFunc = uiState.drawFunc;
		ui_layers[ui_layerDepth].closeFunc = uiState.closeFunc;
		ui_layers[ui_layerDepth].keyFunc = uiState.keyFunc;

		ui_layerDepth++;
	}

	uiState.activeUI = frameWork;
	uiState.drawFunc = drawFunc;
	uiState.closeFunc = closeFunc;
	uiState.keyFunc = keyFunc;

	uiState.cursorLock = false;
	uiState.cursorItem = NULL;
	uiState.mouseItem = NULL;
	uiState.selectedItem = NULL;

	cgi.Key_SetDest (KD_MENU);
	UI_UpdateMousePos ();
}


/*
=============
UI_PopInterface
=============
*/
void UI_PopInterface ()
{
	struct sfx_t	*outSound;

	if (ui_layerDepth == 1) {
		// Start the demo loop again
		if (cgi.Com_ClientState () < CA_CONNECTING)
			return;

		if (uiState.activeUI->flags & FWF_INTERFACE)
			UI_ForceAllOff ();
		else
			M_ForceMenuOff ();
		return;
	}

	// Call close function
	if (uiState.activeUI && uiState.closeFunc) {
		outSound = uiState.closeFunc ();
		uiState.activeUI->initialized = false;
		uiState.activeUI->numItems = 0;
	}
	else
		outSound = uiMedia.sounds.menuOut;

	if (outSound)
		cgi.Snd_StartLocalSound (outSound, 1);

	// Fall back a layer
	if (ui_layerDepth < 1)
		Com_Error (ERR_FATAL, "UI_PopInterface: ui_layerDepth < 1");
	ui_layerDepth--;

	uiState.activeUI = ui_layers[ui_layerDepth].frameWork;
	uiState.drawFunc = ui_layers[ui_layerDepth].drawFunc;
	uiState.closeFunc = ui_layers[ui_layerDepth].closeFunc;
	uiState.keyFunc = ui_layers[ui_layerDepth].keyFunc;

	uiState.cursorItem = NULL;
	uiState.mouseItem = NULL;
	uiState.selectedItem = NULL;

	if (!ui_layerDepth) {
		if (uiState.activeUI->flags & FWF_INTERFACE)
			UI_ForceAllOff ();
		else
			M_ForceMenuOff ();
	}

	// Update mouse position
	UI_UpdateMousePos ();
}


/*
=============
UI_ForceAllOff
=============
*/
void UI_ForceAllOff ()
{
	int		i;

	// Call all close functions
	if (uiState.activeUI && uiState.closeFunc) {
		uiState.closeFunc ();
		uiState.activeUI->initialized = false;
		uiState.activeUI->numItems = 0;
	}
	for (i=ui_layerDepth-1 ; i>=0 ; i--) {
		if (!ui_layers[i].closeFunc)
			continue;
		if (ui_layers[i].closeFunc == uiState.closeFunc)
			continue;

		ui_layers[i].closeFunc ();
		ui_layers[i].frameWork->initialized = false;
		ui_layers[i].frameWork->numItems = 0;
	}
	ui_layerDepth = 0;

	// Clear values
	uiState.activeUI = NULL;
	uiState.drawFunc = NULL;
	uiState.closeFunc = NULL;
	uiState.keyFunc = NULL;

	uiState.cursorItem = NULL;
	uiState.mouseItem = NULL;
	uiState.selectedItem = NULL;

	// Back to game and clear key states
	cgi.Key_SetDest (KD_GAME);
	cgi.Key_ClearStates ();

	// Update mouse position
	UI_UpdateMousePos ();
}


/*
=============
UI_StartFramework

Clears out items and prepares a framework for adding items
=============
*/
void UI_StartFramework (uiFrameWork_t *fw, int flags)
{
	fw->locked = false;
	fw->initialized = false;
	fw->flags = flags;

	// No items yet
	fw->numItems = 0;
}


/*
=============
UI_FinishFramework

All items have been added to the framework by the time this function is called
=============
*/
void UI_FinishFramework (uiFrameWork_t *fw, bool lock)
{
	uiImage_t	*image;
	int			i;

	if (!fw->numItems && lock)
		Com_Error (ERR_FATAL, "UI_FinishFramework: Framework has no items");

	fw->locked = lock;
	fw->initialized = true;

	// Do anything item-specific work now
	for (i=0 ; i<fw->numItems ; i++) {
		switch (((uiCommon_t *) fw->items[i])->type) {
		case UITYPE_IMAGE:
			image = (uiImage_t*)fw->items[i];

			cgi.R_GetImageSize (image->mat, &image->width, &image->height);
			break;
		}
	}
}
