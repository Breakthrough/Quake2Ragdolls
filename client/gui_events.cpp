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
// gui_events.c
//

#include "gui_local.h"

/*
=============================================================================

	NAMED EVENTS

=============================================================================
*/

/*
==================
GUI_QueueWindowNamedEvent

Queues a named event to trigger on a specified window.
==================
*/
static void GUI_QueueWindowNamedEvent (gui_t *gui, char *name, bool warn)
{
	event_t	*event;
	int		i;

	assert (name && name[0]);
	if (!name || !name[0])
		return;

	// Only send if the window is visible
	if (!FRVALUE (gui, FR_VISIBLE)
	|| FRVALUE (gui, FR_NO_EVENTS))
		return;

	// Find it in the event list
	for (i=0, event=&gui->eventList[0] ; i<gui->numEvents ; event++, i++) {
		if (event->type != WEV_NAMED)
			continue;
		if (Q_stricmp (name, event->named))
			continue;
		break;
	}

	// Not found
	if (i == gui->numEvents) {
		if (warn)
			Com_Printf (PRNT_WARNING, "WARNING: Named event '%s' not found in '%s'\n", name, gui->name);
		return;
	}

	// No room!
	if (gui->numQueued+1 >= MAX_GUI_EVENTS)
		return;

	// Make sure it's not already queued
	for (i=0 ; i<gui->numQueued ; i++) {
		if (gui->queueList[i] == event)
			return;
	}

	// Append
	gui->queueList[gui->numQueued++] = event;
}


/*
==================
GUI_NamedGlobalEvent

Affects all opened GUIs.
==================
*/
void GUI_NamedGlobalEvent (char *name)
{
	gui_t	*gui;
	int		i;

	// Recurse through each open GUI and apply
	for (i=0, gui=cl_guiState.openLayers[0] ; i<cl_guiState.numLayers ; gui++, i++)
		GUI_NamedGUIEvent (gui, name);
}


/*
==================
GUI_NamedGUIEvent

Affects only the specified GUI.
==================
*/
void GUI_NamedGUIEvent (gui_t *gui, char *name)
{
	gui_t	*child;
	int		i;

	// Queue any matching events
	GUI_QueueWindowNamedEvent (gui, name, false);

	// Recurse down the children
	for (i=0, child=gui->childList ; i<gui->numChildren ; child++, i++)
		GUI_NamedGUIEvent (child, name);
}

/*
=============================================================================

	ACTION PROCESSING

=============================================================================
*/

/*
==================
gui_tetAction
==================
*/
static void gui_tetAction (gui_t *gui, eva_set_t *setAction)
{
	gui_t	*queueWindow;
	evType_t		queueEvent;
	int		i;

	if (!setAction->destWindowPtr)
		return;

	queueEvent = WEV_NONE;

	switch (setAction->destType) {
	case EVA_SETDEST_FLOAT|EVA_SETDEST_STORAGE:
		switch (setAction->destRegister) {
		case FR_VISIBLE:
			if (FRVALUE (setAction->destWindowPtr, setAction->destRegister) != setAction->srcStorage[0]) {
				gui->shared->cursor.mouseMoved = true;
				queueWindow = setAction->destWindowPtr;
				if (FRVALUE (setAction->destWindowPtr, setAction->destRegister))
					queueEvent = WEV_INIT;
				else
					queueEvent = WEV_SHUTDOWN;
			}
			break;
		}

		switch (setAction->srcType) {
		case EVA_SETSRC_STORAGE:
			setAction->destWindowPtr->d.floatRegisters[setAction->destRegister].storage = setAction->srcStorage[0];
			setAction->destWindowPtr->d.floatRegisters[setAction->destRegister].variable = &setAction->destWindowPtr->d.floatRegisters[setAction->destRegister].storage;
			break;
		case EVA_SETSRC_DEF:
			break;
		case EVA_SETSRC_GUIVAR:
			break;
		}
		break;

	case EVA_SETDEST_VEC|EVA_SETDEST_STORAGE:
		for (i=0 ; i<setAction->destNumVecs ; i++)
			setAction->destWindowPtr->d.vecRegisters[setAction->destRegister].storage[i] = setAction->srcStorage[i];

		setAction->destWindowPtr->d.vecRegisters[setAction->destRegister].variable = setAction->destWindowPtr->d.vecRegisters[setAction->destRegister].storage;
		break;

	case EVA_SETDEST_FLOAT|EVA_SETDEST_DEF:
		break;

	case EVA_SETDEST_VEC|EVA_SETDEST_DEF:
		break;

	default:
		assert (0);
		break;
	}

	// Trigger events
	if (queueEvent != WEV_NONE)
		GUI_QueueTrigger (queueWindow, queueEvent);
}


/*
==================
GUI_RunEvent

Processes event actions.
==================
*/
static void GUI_RunEvent (gui_t *gui, event_t *event)
{
	evAction_t	*action;
	int			i;

	for (i=0, action=event->actionList ; i<event->numActions ; action++, i++) {
		switch (action->type) {
		case EVA_CLOSE:
			gui->shared->queueClose = true;
			break;

		case EVA_COMMAND:
			assert (action->command && action->command[0]);
			Cbuf_AddText (action->command);
			break;

		case EVA_IF:
			break;

		case EVA_LOCAL_SOUND:
			Snd_StartLocalSound (action->localSound->sound, action->localSound->volume);
			break;

		case EVA_NAMED_EVENT:
			GUI_QueueWindowNamedEvent (action->named->destWindowPtr, action->named->eventName, true);
			break;

		case EVA_RESET_TIME:
			gui->openTime = Sys_UMilliseconds () - action->resetTime;
			gui->time = gui->lastTime = action->resetTime;
			break;

		case EVA_SET:
			gui_tetAction (gui, action->set);
			break;

		case EVA_STOP_TRANSITIONS:
			break;
		case EVA_TRANSITION:
			break;
		}
	}
}

/*
=============================================================================

	DEFAULT EVENTS

=============================================================================
*/

/*
==================
GUI_CheckDefDefault
==================
*/
static void GUI_CheckDefDefault (gui_t *gui, evType_t type)
{
	switch (type) {
	case WEV_ACTION:
		if (gui->d.checkDef->liveUpdate) {
			if (!Q_stricmp (gui->d.checkDef->cvar->string, gui->d.checkDef->values[0]))
				Cvar_VariableSet (gui->d.checkDef->cvar, gui->d.checkDef->values[1], false);
			else
				Cvar_VariableSet (gui->d.checkDef->cvar, gui->d.checkDef->values[0], false);
		}
		else {
			assert (0);
		}
		break;
	}
}

/*
=============================================================================

	EVENT QUEUE

	Events can be saved here as "triggers", so that on the next render call
	they will trigger the appropriate event.
=============================================================================
*/

/*
==================
GUI_TriggerEvents

Triggers any queued events, along with the per-frame and time-based events.
==================
*/
void GUI_TriggerEvents (gui_t *gui)
{
	gui_t	*child;
	event_t	*event;
	int		i;

	if (!FRVALUE (gui, FR_VISIBLE)
	|| FRVALUE (gui, FR_NO_EVENTS)) {
		// Clear default events
		gui->numDefaultQueued = 0;

		// Clear queued triggers
		gui->numQueued = 0;
		return;
	}

	// Update time
	gui->time = Sys_UMilliseconds () - gui->openTime;

	// FIXME: something needs to be done about uint32 wrapping!!
	/*if (gui->lastTime > gui->time) {
		gui->lastTime = gui->time;
	}*/

	// Process events
	for (i=0, event=gui->eventList ; i<gui->numEvents ; event++, i++) {
		// This is triggered every frame
		if (event->type == WEV_FRAME) {
			GUI_RunEvent (gui, event);
			continue;
		}

		// Time-based triggers
		if (event->type == WEV_TIME) {
			if (event->onTime >= gui->lastTime && event->onTime < gui->time)
				GUI_RunEvent (gui, event);
			continue;
		}
		break;
	}

	// Fire queued triggers
	for (i=0 ; i<gui->numQueued ; i++)
		GUI_RunEvent (gui, gui->queueList[i]);
	gui->numQueued = 0;

	// Fire default events
	for (i=0 ; i<gui->numDefaultQueued ; i++) {
		switch (gui->type) {
		case WTP_CHECKBOX:
			GUI_CheckDefDefault (gui, gui->defaultQueueList[i]);
			break;
		}
	}
	gui->numDefaultQueued = 0;

	// Store the current time for the next trip
	gui->lastTime = gui->time;

	// Recurse down the children
	for (i=0, child=gui->childList ; i<gui->numChildren ; child++, i++)
		GUI_TriggerEvents (child);
}


/*
==================
GUI_QueueTrigger
==================
*/
void GUI_QueueTrigger (gui_t *gui, evType_t type)
{
	gui_t	*child;
	event_t	*event;
	int		i;

	if (!gui)
		return;

	// Mouse enter/exit is sent every frame, regardless of the state actually changing
	// or not. This is purely to make the exit event as reliable as possible.
	switch (type) {
	case WEV_ACTION:
		break;

	case WEV_ESCAPE:
		break;

	case WEV_INIT:
		if (gui->inited)
			return;
		gui->inited = true;
		break;

	case WEV_SHUTDOWN:
		if (!gui->inited)
			return;
		gui->inited = false;

		GUI_QueueTrigger (gui, WEV_MOUSE_EXIT);
		for (i=0, child=gui->childList ; i<gui->numChildren ; child++, i++)
			GUI_QueueTrigger (child, WEV_SHUTDOWN);
		break;

	case WEV_MOUSE_ENTER:
		if (gui->mouseEntered && !gui->mouseExited)
			return;
		gui->mouseEntered = true;
		gui->mouseExited = false;
		break;

	case WEV_MOUSE_EXIT:
		if (!gui->mouseEntered && gui->mouseExited)
			return;
		gui->mouseEntered = false;
		gui->mouseExited = true;
		break;

	case WEV_NAMED:
		// These are queued up elsewhere!
		assert (0);
		return;

	case WEV_FRAME:
	case WEV_TIME:
		// These should *NEVER* be queued!!
		// They're forcibly ran in GUI_TriggerEvents at the beginning of every frame anyways!
		assert (0);
		return;

	default:
		// Should never happen!
		assert (0);
		return;
	}

	// Add to default action list (if not found already)
	for (i=0 ; i<gui->numDefaultQueued ; i++) {
		if (gui->defaultQueueList[i] == type)
			break;
	}
	if (i == gui->numDefaultQueued) {
		if (gui->numDefaultQueued < MAX_GUI_EVENTS)
			gui->defaultQueueList[gui->numDefaultQueued++] = type;
	}

	// Find it in the event list
	for (i=0, event=&gui->eventList[0] ; i<gui->numEvents ; event++, i++) {
		if (event->type == type)
			break;
	}
	if (i == gui->numEvents)
		return;	// Not found

	// If already in the list, ignore
	for (i=0 ; i<gui->numQueued ; i++) {
		if (gui->queueList[i] == event)
			return;
	}

	// Append
	if (gui->numQueued < MAX_GUI_EVENTS)
		gui->queueList[gui->numQueued++] = event;

	// Update
	gui->shared->cursor.mouseMoved = true;

	// Post-process sub-triggers
	switch (type) {
	case WEV_INIT:
		for (i=0, child=gui->childList ; i<gui->numChildren ; child++, i++)
			GUI_QueueTrigger (child, WEV_INIT);
		GUI_QueueTrigger (gui, WEV_MOUSE_ENTER);
		break;
	}
}
