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
// gui_main.c
//

#include "gui_local.h"

guiState_t	cl_guiState;

/*
=============================================================================

	GUI STATE

=============================================================================
*/

/*
==================
GUI_ResetGUIState
==================
*/
void GUI_ResetGUIState (gui_t *gui)
{
	gui_t	*child;
	int		i;

	memcpy (gui->d.defFloatList, gui->s.defFloatList, sizeof(defineFloat_t) * gui->s.numDefFloats);
	memcpy (gui->d.defVecList, gui->s.defVecList, sizeof(defineVec_t) * gui->s.numDefVecs);

	memcpy (gui->d.floatRegisters, gui->s.floatRegisters, sizeof(floatRegister_t) * FR_MAX);
	memcpy (gui->d.vecRegisters, gui->s.vecRegisters, sizeof(vecRegister_t) * VR_MAX);

	switch (gui->type) {
	case WTP_GUI:
		memcpy (&gui->shared->cursor.d, &gui->shared->cursor.s, sizeof(guiCursorData_t));
		gui->shared->queueClose = false;
		break;

	case WTP_GENERIC:
		break;

	case WTP_BIND:		memcpy (gui->d.bindDef, gui->s.bindDef, sizeof(bindDef_t));		break;
	case WTP_CHECKBOX:	memcpy (gui->d.checkDef, gui->s.checkDef, sizeof(checkDef_t));		break;
	case WTP_CHOICE:	memcpy (gui->d.choiceDef, gui->s.choiceDef, sizeof(choiceDef_t));	break;
	case WTP_EDIT:		memcpy (gui->d.editDef, gui->s.editDef, sizeof(editDef_t));		break;
	case WTP_LIST:		memcpy (gui->d.listDef, gui->s.listDef, sizeof(listDef_t));		break;
	case WTP_RENDER:	memcpy (gui->d.renderDef, gui->s.renderDef, sizeof(renderDef_t));	break;
	case WTP_SLIDER:	memcpy (gui->d.sliderDef, gui->s.sliderDef, sizeof(sliderDef_t));	break;
	case WTP_TEXT:		memcpy (gui->d.textDef, gui->s.textDef, sizeof(textDef_t));		break;
	}

	if (FRVALUE (gui, FR_VISIBLE)) {
		gui->openTime = Sys_UMilliseconds ();
		gui->lastTime = gui->openTime;
		GUI_QueueTrigger (gui, WEV_INIT);
	}
	else {
		gui->inited = false;
		gui->mouseEntered = false;
		gui->mouseExited = true;
	}
	gui->time = 0;

	// Recurse down the children
	for (i=0, child=gui->childList ; i<gui->numChildren ; child++, i++)
		GUI_ResetGUIState (child);
}

/*
=============================================================================

	OPENED GUIS

=============================================================================
*/

/*
==================
GUI_OpenGUI
==================
*/
void GUI_OpenGUI (gui_t *gui)
{
	int		i;

	if (!gui)
		return;

	// Check if it's open
	for (i=0 ; i<cl_guiState.numLayers ; i++) {
		if (gui == cl_guiState.openLayers[i])
			break;
	}
	if (i != cl_guiState.numLayers) {
		Com_Error (ERR_DROP, "GUI_OpenGUI: GUI '%s' already opened!", gui->name);
		return;
	}

	// Check for room
	if (cl_guiState.numLayers+1 >= MAX_GUI_DEPTH) {
		Com_Error (ERR_DROP, "GUI_OpenGUI: Too many GUIs open to open '%s'!", gui->name);
		return;
	}

	// Reset state
	GUI_ResetGUIState (gui);

	// Add it to the list
	cl_guiState.openLayers[cl_guiState.numLayers++] = gui;

	// FIXME
	Key_SetDest (KD_MENU);
}


/*
==================
GUI_CloseGUI

FIXME: send off WEV_SHUTDOWN messages.
==================
*/
void GUI_CloseGUI (gui_t *gui)
{
	bool	found;
	int		i;

	if (!gui)
		return;

	// Clear this in case the mouse moves before we find our new inputWindow
	cl_guiState.inputWindow = NULL;

	// Reset the state
	GUI_ResetGUIState (gui);

	// Remove it from the list
	found = false;
	for (i=0 ; i<cl_guiState.numLayers ; i++) {
		if (found)
			cl_guiState.openLayers[i-1] = cl_guiState.openLayers[i];
		else if (gui == cl_guiState.openLayers[i])
			found = true;
	}

	// Check if it's open
	if (!found) {
		Com_Error (ERR_DROP, "GUI_CloseGUI: GUI '%s' not opened!", gui->name);
		return;
	}

	// Trim off the extra at the end
	cl_guiState.openLayers[--cl_guiState.numLayers] = NULL;

	// FIXME
	if (!cl_guiState.numLayers)
		Key_SetDest (KD_GAME);
}


/*
==================
GUI_CloseAllGUIs
==================
*/
void GUI_CloseAllGUIs ()
{
	gui_t	*gui;
	int		i;

	for (i=0, gui=cl_guiState.openLayers[0] ; i<cl_guiState.numLayers ; gui++, i++)
		GUI_CloseGUI (gui);
}

/*
=============================================================================

	GUI SCALING

=============================================================================
*/

/*
==================
gui_tcaleGUI
==================
*/
static void GUI_r_CalcRect (gui_t *window, gui_t *parent)
{
	gui_t	*child;
	int		i;

	// Scale this window
	if (parent) {
		window->rect[0] = parent->rect[0] + VRVALUE (window, VR_RECT)[0];
		window->rect[1] = parent->rect[1] + VRVALUE (window, VR_RECT)[1];
	}
	else {
		window->rect[0] = VRVALUE (window, VR_RECT)[0];
		window->rect[1] = VRVALUE (window, VR_RECT)[1];
	}
	window->rect[2] = VRVALUE (window, VR_RECT)[2];
	window->rect[3] = VRVALUE (window, VR_RECT)[3];

	// Recurse down the children
	for (i=0, child=window->childList ; i<window->numChildren ; child++, i++) {
		if (!FRVALUE (window, FR_VISIBLE))
			continue;

		GUI_r_CalcRect (child, window);
	}
}
static void GUI_r_ScaleGUI (gui_t *window)
{
	gui_t	*child;
	int		i;

	// Scale
	window->rect[0] *= window->shared->xScale;
	window->rect[1] *= window->shared->yScale;
	window->rect[2] *= window->shared->xScale;
	window->rect[3] *= window->shared->yScale;

	// Recurse down the children
	for (i=0, child=window->childList ; i<window->numChildren ; child++, i++) {
		if (!FRVALUE (window, FR_VISIBLE))
			continue;

		GUI_r_ScaleGUI (child);
	}
}
static void gui_tcaleGUI (gui_t *gui)
{
	if (!FRVALUE (gui, FR_VISIBLE))
		return;

	// Calculate aspect
	if (gui_debugScale->intVal) {
		gui->shared->xScale = 1;
		gui->shared->yScale = 1;
	}
	else {
		gui->shared->xScale = cls.refConfig.vidWidth / VRVALUE (gui, VR_RECT)[2];
		gui->shared->yScale = cls.refConfig.vidHeight / VRVALUE (gui, VR_RECT)[3];
	}

	// Scale the children
	GUI_r_CalcRect (gui, NULL);
	GUI_r_ScaleGUI (gui);
}

/*
=============================================================================

	GUI RENDERING

=============================================================================
*/

/*
==================
GUI_DrawBindDef
==================
*/
static void GUI_DrawBindDef (gui_t *gui)
{
	assert (gui->d.bindDef);
}


/*
==================
GUI_DrawCheckDef
==================
*/
static void GUI_DrawCheckDef (gui_t *gui)
{
	assert (gui->d.checkDef);
	assert (gui->d.checkDef->cvar);
	assert (gui->d.checkDef->offMatPtr);
	assert (gui->d.checkDef->onMatPtr);
	assert (gui->d.checkDef->values[0]);
	assert (gui->d.checkDef->values[1]);

	// Check if it's on
	if (!Q_stricmp (gui->d.checkDef->cvar->string, gui->d.checkDef->values[1])) {
		R_DrawPic (gui->d.checkDef->onMatPtr, 0,
			QuadVertices().SetVertices(gui->rect[0], gui->rect[1], gui->rect[2], gui->rect[3]).SetCoords(
			0, 0,
			FRVALUE (gui, FR_MAT_SCALE_X),
			FRVALUE (gui, FR_MAT_SCALE_Y)),
			colorb(VRVALUE (gui, VR_MAT_COLOR)));
	}
	else {
		R_DrawPic (gui->d.checkDef->offMatPtr, 0,
			QuadVertices().SetVertices(gui->rect[0], gui->rect[1], gui->rect[2], gui->rect[3]).SetCoords(
			0, 0,
			FRVALUE (gui, FR_MAT_SCALE_X),
			FRVALUE (gui, FR_MAT_SCALE_Y)),
			colorb(VRVALUE (gui, VR_MAT_COLOR)));
	}
}


/*
==================
GUI_DrawChoiceDef
==================
*/
static void GUI_DrawChoiceDef (gui_t *gui)
{
	assert (gui->d.choiceDef);
}


/*
==================
GUI_DrawEditDef
==================
*/
static void GUI_DrawEditDef (gui_t *gui)
{
	assert (gui->d.editDef);
}


/*
==================
GUI_DrawListDef
==================
*/
static void GUI_DrawListDef (gui_t *gui)
{
	assert (gui->d.listDef);
}


/*
==================
GUI_DrawRenderDef
==================
*/
static void GUI_DrawRenderDef (gui_t *gui)
{
	assert (gui->d.renderDef);
}


/*
==================
GUI_DrawSliderDef
==================
*/
static void GUI_DrawSliderDef (gui_t *gui)
{
	assert (gui->d.sliderDef);
}


/*
==================
GUI_DrawTextDef
==================
*/
static void GUI_DrawTextDef (gui_t *gui)
{
	vec2_t	charSize;
	float	x, y;
	float	*color;
	vec2_t	scale;
	uint32	textFlags;

	assert (gui->d.textDef);

	// Scale
	scale[0] = FRVALUE (gui, FR_TEXT_SCALE) * gui->shared->xScale;
	scale[1] = FRVALUE (gui, FR_TEXT_SCALE) * gui->shared->yScale;

	// Text flags
	textFlags = FRVALUE (gui, FR_TEXT_SHADOW) ? FS_SHADOW : 0;

	// Position
	// FIXME TODO: FS_ALIGN_*
	switch ((int)FRVALUE (gui, FR_TEXT_ALIGN)) {
	default:
	case 0:
		// Left
		x = gui->rect[0];
		y = gui->rect[1];
		break;

	case 1:
		// Center
		R_GetFontDimensions (gui->d.textDef->fontPtr, scale[0], scale[1], textFlags, charSize);
		x = gui->rect[0] + (gui->rect[2] * 0.5f) - (gui->d.textDef->textStringLen * charSize[0] * 0.5f);
		y = gui->rect[1];
		break;

	case 2:
		// Right
		R_GetFontDimensions (gui->d.textDef->fontPtr, scale[0], scale[1], textFlags, charSize);
		x = gui->rect[0] + (gui->rect[2] - (gui->d.textDef->textStringLen * charSize[0]));
		y = gui->rect[1];
		break;
	}

	// Choose the color
	if (gui->shared->cursor.curWindow == gui)
		color = VRVALUE (gui, VR_TEXT_HOVERCOLOR);
	else
		color = VRVALUE (gui, VR_TEXT_COLOR);

	// Draw it
	R_DrawString (gui->d.textDef->fontPtr, x, y, scale[0], scale[1], textFlags, gui->d.textDef->textString, colorb(color));
}


/*
==================
GUI_DrawWindows
==================
*/
static void GUI_DrawWindows (gui_t *gui)
{
	gui_t	*child;
	int		i;

	if (!FRVALUE (gui, FR_VISIBLE))
		return;

	// Fill
	if (gui->flags & WFL_FILL_COLOR)
		R_DrawFill(gui->rect[0], gui->rect[1], gui->rect[2], gui->rect[3], colorb(VRVALUE (gui, VR_FILL_COLOR)));

	// Material
	if (gui->flags & WFL_MATERIAL && gui->mat)
		R_DrawPic (gui->mat, 0,
			QuadVertices().SetVertices(gui->rect[0], gui->rect[1], gui->rect[2], gui->rect[3]).SetCoords(
			0, 0,
			FRVALUE (gui, FR_MAT_SCALE_X),
			FRVALUE (gui, FR_MAT_SCALE_Y)),
			colorb(VRVALUE (gui, VR_MAT_COLOR)));

	// Item-specific rendering
	switch (gui->type) {
	case WTP_GUI:									break;
	case WTP_GENERIC:								break;
	case WTP_BIND:		GUI_DrawBindDef (gui);		break;
	case WTP_CHECKBOX:	GUI_DrawCheckDef (gui);		break;
	case WTP_CHOICE:	GUI_DrawChoiceDef (gui);	break;
	case WTP_EDIT:		GUI_DrawEditDef (gui);		break;
	case WTP_LIST:		GUI_DrawListDef (gui);		break;
	case WTP_RENDER:	GUI_DrawRenderDef (gui);	break;
	case WTP_SLIDER:	GUI_DrawSliderDef (gui);	break;
	case WTP_TEXT:		GUI_DrawTextDef (gui);		break;
	default:			assert (0);					break;
	}

	// Bounds debug
	// FIXME: remove this stupid image
	if (gui_debugBounds->intVal == 1 && gui->shared->cursor.curWindow == gui
	|| gui_debugBounds->intVal == 2)
		R_DrawPic (R_RegisterPic ("guis/debug/bounds.tga"), 0,
			QuadVertices().SetVertices(gui->rect[0], gui->rect[1], gui->rect[2], gui->rect[3]),
			Q_BColorYellow);

	// Recurse down the children
	for (i=0, child=gui->childList ; i<gui->numChildren ; child++, i++)
		GUI_DrawWindows (child);
}


/*
==================
GUI_DrawCursor
==================
*/
static void GUI_DrawCursor (gui_t *gui)
{
	guiCursor_t *cursor;

	cursor = &gui->shared->cursor;
	if (!cursor || !cursor->d.matPtr || !cursor->d.visible)
		return;

	R_DrawPic (cursor->d.matPtr, 0,
		QuadVertices().SetVertices(cursor->d.pos[0], cursor->d.pos[1],
		cursor->d.size[0] * gui->shared->xScale,
		cursor->d.size[1] * gui->shared->yScale),
		colorb(cursor->d.color));
}


/*
==================
GUI_Refresh
==================
*/
void GUI_Refresh ()
{
	gui_t	*gui;
	int		i;

	cl_guiState.frameCount++;

	for (i=0, gui=cl_guiState.openLayers[0] ; i<cl_guiState.numLayers ; gui++, i++) {
		// Fire event triggers queued last frame
		GUI_TriggerEvents (gui);

		// Calculate scale
		gui_tcaleGUI (gui);

		// Generate bounds for collision
		GUI_GenerateBounds (gui);

		// Calculate bounds and check collision
		GUI_CursorUpdate (gui);

		// Render all windows
		GUI_DrawWindows (gui);

		// Render the cursor
		GUI_DrawCursor (gui);

		// Close if queued
		// FIXME: gross
		if (gui->shared->queueClose)
			GUI_CloseGUI (gui);
	}
}
