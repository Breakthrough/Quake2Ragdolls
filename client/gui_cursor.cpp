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
// gui_cursor.c
//

#include "gui_local.h"

/*
=============================================================================

	BOUNDS CALCULATION

=============================================================================
*/

/*
==================
GUI_GenerateBounds
==================
*/
void GUI_GenerateBounds (gui_t *gui)
{
	gui_t	*child;
	int		i;

	// Generate bounds
	gui->mins[0] = gui->rect[0];
	gui->mins[1] = gui->rect[1];
	gui->maxs[0] = gui->mins[0] + gui->rect[2];
	gui->maxs[1] = gui->mins[1] + gui->rect[3];

	// Generate bounds for children
	for (i=0, child=gui->childList ; i<gui->numChildren ; child++, i++) {
		GUI_GenerateBounds (child);

		// Add to the parent
		AddBoundsTo2DBounds (child->mins, child->maxs, gui->mins, gui->maxs);
	}
}

/*
=============================================================================

	CURSOR COLLISION

=============================================================================
*/

/*
==================
GUI_CursorUpdate
==================
*/
static gui_t	*gui_bestWindow;
void GUI_r_CursorUpdate (gui_t *gui, evType_t forceEvent)
{
	gui_t	*child;
	int		i;

	// Check for collision
	if (forceEvent == WEV_SHUTDOWN) {
		GUI_QueueTrigger (gui, WEV_SHUTDOWN);
	}
	else if (forceEvent == WEV_MOUSE_EXIT) {
		GUI_QueueTrigger (gui, WEV_MOUSE_EXIT);
	}
	else if (!FRVALUE (gui, FR_VISIBLE)) {
		GUI_QueueTrigger (gui, WEV_SHUTDOWN);
		forceEvent = WEV_SHUTDOWN;
	}
	else if (FRVALUE (gui, FR_NO_EVENTS)
	|| gui->shared->cursor.d.pos[0] < gui->mins[0]
	|| gui->shared->cursor.d.pos[1] < gui->mins[1]
	|| gui->shared->cursor.d.pos[0] > gui->maxs[0]
	|| gui->shared->cursor.d.pos[1] > gui->maxs[1]) {
		GUI_QueueTrigger (gui, WEV_MOUSE_EXIT);
		forceEvent = WEV_MOUSE_EXIT;
	}
	else {
		// Collided with this child
		if (!FRVALUE (gui_bestWindow, FR_MODAL) || FRVALUE (gui, FR_MODAL)) {
			GUI_QueueTrigger (gui, WEV_MOUSE_ENTER);
			forceEvent = WEV_NONE;
			gui_bestWindow = gui;
		}
	}

	// Recurse down the children
	for (i=0, child=gui->childList ; i<gui->numChildren ; child++, i++)
		GUI_r_CursorUpdate (child, forceEvent);
}
void GUI_CursorUpdate (gui_t *gui)
{
	evType_t	forceEvent;
	gui_t		*child;
	int			i;

	if (!gui->shared->cursor.mouseMoved)
		return;
	gui->shared->cursor.mouseMoved = false;

	cl_guiState.inputWindow = NULL;
	gui_bestWindow = NULL;

	// Check for collision
	if (!FRVALUE (gui, FR_VISIBLE)) {
		GUI_QueueTrigger (gui, WEV_SHUTDOWN);
		forceEvent = WEV_SHUTDOWN;
	}
	else if (FRVALUE (gui, FR_NO_EVENTS)
	|| gui->shared->cursor.d.pos[0] < gui->mins[0]
	|| gui->shared->cursor.d.pos[1] < gui->mins[1]
	|| gui->shared->cursor.d.pos[0] > gui->maxs[0]
	|| gui->shared->cursor.d.pos[1] > gui->maxs[1]) {
		GUI_QueueTrigger (gui, WEV_MOUSE_EXIT);
		forceEvent = WEV_MOUSE_EXIT;
	}
	else {
		GUI_QueueTrigger (gui, WEV_MOUSE_ENTER);
		forceEvent = WEV_NONE;

		// Collided with this window
		cl_guiState.inputWindow = gui;
		gui_bestWindow = gui;
	}

	// Recurse down the children
	for (i=0, child=gui->childList ; i<gui->numChildren ; child++, i++)
		GUI_r_CursorUpdate (child, forceEvent);

	// Trigger events
	if (gui->shared->cursor.curWindow != gui_bestWindow) {
		if (gui->shared->cursor.curWindow)
			GUI_QueueTrigger (gui->shared->cursor.curWindow, WEV_SHUTDOWN);
	}

	// Store
	gui->shared->cursor.curWindow = gui_bestWindow;
}

/*
=============================================================================

	CURSOR MOVEMENT

=============================================================================
*/

/*
==================
GUI_MoveMouse
==================
*/
void GUI_MoveMouse (int xMove, int yMove)
{
	guiCursor_t	*cursor;
	gui_t		*gui;
	int			i;

	// Let open GUIs know the cursor moved
	if (xMove || yMove) {
		for (i=0 ; i<cl_guiState.numLayers ; i++)
			cl_guiState.openLayers[i]->shared->cursor.mouseMoved = true;
	}

	// Move for the input window
	gui = cl_guiState.inputWindow;
	if (!gui)
		return;

	cursor = &gui->shared->cursor;
	if (cursor->d.locked)
		return;

	// Scale
	xMove *= gui->shared->xScale;
	yMove *= gui->shared->yScale;

	// Move
	if (gui_mouseFilter->intVal) {
		cursor->d.pos[0] = (cursor->d.pos[0] * 2 + (xMove * gui_mouseSensitivity->floatVal)) * 0.5f;
		cursor->d.pos[1] = (cursor->d.pos[1] * 2 + (yMove * gui_mouseSensitivity->floatVal)) * 0.5f;
	}
	else {
		cursor->d.pos[0] += xMove * gui_mouseSensitivity->floatVal;
		cursor->d.pos[1] += yMove * gui_mouseSensitivity->floatVal;
	}

	// Clamp
	cursor->d.pos[0] = clamp (cursor->d.pos[0], gui->owner->mins[0], gui->owner->maxs[0]);
	cursor->d.pos[1] = clamp (cursor->d.pos[1], gui->owner->mins[1], gui->owner->maxs[1]);
}


/*
==================
GUI_AdjustCursor
==================
*/
static bool	gui_bestFurthest;
static float	gui_bestKeyDist;
static gui_t	*gui_bestKeyWindow;
void GUI_r_AdjustCursor (gui_t *gui, guiCursor_t *cursor, vec4_t bounds)
{
	gui_t	*child;
	float	dist;
	vec2_t	center;
	int		i;

	// Must be visible and accept events
	if (!FRVALUE (gui, FR_VISIBLE)
	|| FRVALUE (gui, FR_NO_EVENTS)) {
		return;
	}

	// Check for collision with movement bounds
	if (bounds[0] > gui->maxs[0]
	|| bounds[1] > gui->maxs[1]
	|| bounds[2] < gui->mins[0]
	|| bounds[3] < gui->mins[1])
		return;

	// Can't be the same window and must have a selectable item flag
	if (gui != cursor->curWindow && gui->flags & WFL_ITEM) {
		// Find the center point of this window
		center[0] = gui->mins[0] + ((gui->maxs[0] - gui->mins[0]) * 0.5f);
		center[1] = gui->mins[1] + ((gui->maxs[1] - gui->mins[1]) * 0.5f);

		// Calculate distance
		dist = Vec2Dist (cursor->d.pos, center);

		// Check if it's the closest
		if (gui_bestFurthest) {
			if (dist > gui_bestKeyDist) {
				gui_bestKeyDist = dist;
				gui_bestKeyWindow = gui;
			}
		}
        else if (dist < gui_bestKeyDist) {
			gui_bestKeyDist = dist;
			gui_bestKeyWindow = gui;
		}
	}

	// Recurse down the children
	for (i=0, child=gui->childList ; i<gui->numChildren ; child++, i++)
		GUI_r_AdjustCursor (child, cursor, bounds);
}
void GUI_AdjustCursor (keyNum_t keyNum)
{
	gui_t		*gui, *curItem;
	guiCursor_t	*cursor;
	vec2_t		end;
	vec4_t		bounds;
	float		xMove, yMove;

	// Move for the input window
	gui = cl_guiState.inputWindow;
	if (!gui)
		return;

	cursor = &gui->shared->cursor;
	if (!cursor || cursor->d.locked)
		return;

	curItem = cursor->curWindow;
	if (!(curItem->flags & WFL_ITEM))
		curItem = NULL;

	// Generate bounds
	switch (keyNum) {
	case K_UPARROW:
	case K_KP_UPARROW:
		bounds[0] = gui->owner->mins[0];
		bounds[1] = gui->owner->mins[1];
		bounds[2] = gui->owner->maxs[0];
		bounds[3] = (curItem) ? curItem->mins[1] + 1 : cursor->d.pos[1] + 1;
		break;

	case K_DOWNARROW:
	case K_KP_DOWNARROW:
		bounds[0] = gui->owner->mins[0];
		bounds[1] = (curItem) ? curItem->maxs[1] - 1 : cursor->d.pos[1] - 1;
		bounds[2] = gui->owner->maxs[0];
		bounds[3] = gui->owner->maxs[1];
		break;

	case K_LEFTARROW:
	case K_KP_LEFTARROW:
		bounds[0] = gui->owner->mins[0];
		bounds[1] = gui->owner->mins[1];
		bounds[2] = (curItem) ? curItem->mins[0] - 1 : cursor->d.pos[0] - 1;
		bounds[3] = gui->owner->maxs[1];
		break;

	case K_RIGHTARROW:
	case K_KP_RIGHTARROW:
		bounds[0] = (curItem) ? curItem->maxs[0] + 1 : cursor->d.pos[0] + 1;
		bounds[1] = gui->owner->mins[1];
		bounds[2] = gui->owner->maxs[0];
		bounds[3] = gui->owner->maxs[1];
		break;

	default:
		assert (0);
		break;
	}

	// Search for the nearest item in that direction
	if (!gui_bestFurthest)
		gui_bestKeyDist = 999999;
	gui_bestKeyWindow = NULL;
	GUI_r_AdjustCursor (gui->owner, cursor, bounds);

	// If nothing is found, try the complete opposite direction
	if (!gui_bestKeyWindow) {
		if (!gui_bestFurthest) {
			gui_bestFurthest = true;
			gui_bestKeyDist = -999999;
			switch (keyNum) {
			case K_UPARROW:
			case K_KP_UPARROW:
				GUI_AdjustCursor (K_DOWNARROW);
				break;

			case K_DOWNARROW:
			case K_KP_DOWNARROW:
				GUI_AdjustCursor (K_UPARROW);
				break;

			case K_LEFTARROW:
			case K_KP_LEFTARROW:
				GUI_AdjustCursor (K_RIGHTARROW);
				break;

			case K_RIGHTARROW:
			case K_KP_RIGHTARROW:
				GUI_AdjustCursor (K_LEFTARROW);
				break;
			}
			gui_bestFurthest = false;
		}
		return;
	}

	// Calculate the final end point (center of the best item)
	end[0] = gui_bestKeyWindow->mins[0] + ((gui_bestKeyWindow->maxs[0] - gui_bestKeyWindow->mins[0]) * 0.5f);
	end[1] = gui_bestKeyWindow->mins[1] + ((gui_bestKeyWindow->maxs[1] - gui_bestKeyWindow->mins[1]) * 0.5f);

	// Find the distance moved
	if (cursor->d.pos[0] >= end[0])
		xMove = -(cursor->d.pos[0] - end[0]);
	else
		xMove = end[0] - cursor->d.pos[0];
	if (cursor->d.pos[1] >= end[1])
		yMove = -(cursor->d.pos[1] - end[1]);
	else
		yMove = end[1] - cursor->d.pos[1];

	// Remove scale, GUI_MouseMove scales
	if (xMove)
		xMove /= gui->shared->xScale;
	if (yMove)
		yMove /= gui->shared->yScale;

	// Move the cursor to that item
	GUI_MoveMouse (xMove, yMove);
}
