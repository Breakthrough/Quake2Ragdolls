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
// ui_items.c
//

#include "ui_local.h"

/*
=============================================================================

	UI ITEM MANAGEMENT

=============================================================================
*/

/*
=============
UI_SetupItem
=============
*/
void UI_SetupItem (void *item)
{
	uiCommon_t	*citem;
	uiList_t	*list;
	int			i;

	citem = (uiCommon_t *)item;
	switch (citem->type) {
	case UITYPE_SPINCONTROL:
		list = (uiList_t*)item;

		for (i=0 ; list->itemNames[i] ; i++) ;
		list->numItemNames = i;
		break;
	}
}


/*
=============
UI_AddItem
=============
*/
void UI_AddItem (uiFrameWork_t *fw, void *item)
{
	int			i;

	if (!item)
		return;
	if (fw->numItems >= MAX_UI_ITEMS-1)
		Com_Error (ERR_FATAL, "UI_AddItem: MAX_UI_ITEMS hit");
	if (fw->locked)
		Com_Error (ERR_FATAL, "UI_AddItem: Attempted to add item when framework is locked");

	// Check to see if it already exists
	for (i=0 ; i<fw->numItems ; i++) {
		if (fw->items[i] == item)
			Com_Error (ERR_FATAL, "UI_AddItem: Attempted to add item that is already in list");
	}

	// Add to list and set parent
	fw->items[fw->numItems] = item;
	((uiCommon_t *) fw->items[fw->numItems])->parent = fw;

	// Do necessary work
	UI_SetupItem (fw->items[fw->numItems]);

	fw->numItems++;
}


/*
=============
UI_RemoveItem
=============
*/
void UI_RemoveItem (uiFrameWork_t *fw, void *item)
{
	int		i;
	bool	found;

	if (!item)
		return;
	if (fw->locked)
		Com_Error (ERR_FATAL, "UI_RemoveItem: Attempted to remove item when framework is locked");

	// Pull the list backwards starting at the item
	found = false;
	for (i=0 ; i<fw->numItems ; i++) {
		if (found) {
			fw->items[i-1] = fw->items[i];
		}
		else if (fw->items[i] == item)
			found = true;
	}

	// Remove the last entry, since it's now a duplicate
	if (found) {
		fw->items[fw->numItems-1] = NULL;
		fw->numItems--;
	}
}


/*
=============
UI_AdjustCursor

This function takes the given framework, the direction, and attempts to adjust the framework's
cursor so that it's at the next available slot.
=============
*/
void UI_AdjustCursor (uiFrameWork_t *fw, int dir)
{
	uiCommon_t *curItem = NULL;

	if (!fw || !fw->numItems)
		return;

	// Move in the specified direction until a valid item is hit
	while (dir) {
		curItem = (uiCommon_t*)UI_ItemAtCursor (fw);
		if (curItem) {
			uiState.cursorItem = curItem;
			break;
		}

		fw->cursor += dir;
		if (fw->cursor >= fw->numItems)
			fw->cursor = 0;
		else if (fw->cursor < 0)
			fw->cursor = fw->numItems - 1;
	}
}


/*
=============
UI_ItemAtCursor
=============
*/
void *UI_ItemAtCursor (uiFrameWork_t *fw)
{
	if (!fw || !fw->numItems)
		return NULL;

	if (fw->cursor >= fw->numItems)
		fw->cursor = 0;
	else if (fw->cursor < 0)
		fw->cursor = fw->numItems - 1;

	if (((uiCommon_t *)fw->items[fw->cursor])->flags & UIF_NOSELECT)
		return NULL;

	return fw->items[fw->cursor];
}


/*
=============
UI_SelectItem
=============
*/
void UI_SelectItem (uiCommon_t *item)
{
	if (!item || item->flags & UIF_NOSELECT || !item->callBack)
		return;

	switch (item->type) {
	case UITYPE_ACTION:
	case UITYPE_IMAGE:
	case UITYPE_FIELD:
		item->callBack (item);
		break;
	}
}


/*
=============
UI_SlideItem
=============
*/
static void Slider_DoSlide (uiSlider_t *s, int dir)
{
	s->curValue += dir;

	if (s->curValue > s->maxValue)
		s->curValue = s->maxValue;
	else if (s->curValue < s->minValue)
		s->curValue = s->minValue;

	if (s->generic.callBack)
		s->generic.callBack (s);
}

static void SpinControl_DoSlide (uiList_t *s, int dir)
{
	if (!s->itemNames || !s->numItemNames)
		return;

	// Move
	s->curValue += dir;
	if (s->curValue < 0)
		s->curValue = s->numItemNames-1;
	else if (s->curValue >= s->numItemNames)
		s->curValue = 0;

	// Callback
	if (s->generic.callBack)
		s->generic.callBack (s);
}

static void SpinControl_Better_DoSlide (uiList_Better_t *s, int dir)
{
	if (!s->itemNames.Count())
		return;

	// Move
	s->curValue += dir;
	if (s->curValue < 0)
		s->curValue = s->itemNames.Count()-1;
	else if (s->curValue >= (int)s->itemNames.Count())
		s->curValue = 0;

	// Callback
	if (s->generic.callBack)
		s->generic.callBack (s);
}

bool UI_SlideItem (uiCommon_t *item, int dir)
{
	if (!item || item->flags & UIF_NOSELECT)
		return false;

	switch (item->type) {
	case UITYPE_SLIDER:
		Slider_DoSlide ((uiSlider_t *) item, dir);
		return true;

	case UITYPE_SPINCONTROL:
		SpinControl_DoSlide ((uiList_t *) item, dir);
		return true;

	case UITYPE_SPINCONTROL_BETTER:
		SpinControl_Better_DoSlide ((uiList_Better_t *) item, dir);
		return true;
	}

	return false;
}
