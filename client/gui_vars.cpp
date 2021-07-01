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
// gui_vars.c
// FIXME TODO:
// - Hash optimize!
//

#include "gui_local.h"

static guiVar_t		gui_varList[MAX_GUIVARS];
static uint32		gui_numVars;

/*
=============================================================================

	HELPER FUNCTIONS

=============================================================================
*/

/*
==================
GUIVar_Find
==================
*/
static guiVar_t *GUIVar_Find (char *name, guiVarType_t type)
{
	guiVar_t	*variable;
	uint32		i;

	for (i=0, variable=&gui_varList[0] ; i<gui_numVars ; variable++, i++) {
		if (strcmp (variable->name, name))
			continue;
		if (variable->type != type)
			continue;

		return variable;
	}

	return NULL;
}

/*
=============================================================================

	VARIABLE REGISTRATION

=============================================================================
*/

/*
==================
GUIVar_Register
==================
*/
guiVar_t *GUIVar_Register (char *name, guiVarType_t type)
{
	guiVar_t	*variable;
	char		fixedName[MAX_GV_NAMELEN];

	// Check name
	if (!name || !name[0]) {
		Com_Printf (PRNT_ERROR, "GUIVar_Register: NULL name!\n");
		assert (0);
		return NULL;
	}
	if (strlen(name)+1 >= MAX_GV_NAMELEN) {
		Com_Printf (PRNT_ERROR, "GUIVar_Register: name length too long!\n");
		return NULL;
	}

	// Check type
	switch (type) {
	case GVT_FLOAT:
	case GVT_STR:
	case GVT_STR_PTR:
	case GVT_VEC:
		break;

	default:
		assert (0);
		Com_Error (ERR_DROP, "GUIVar_Register: invalid type '%i'", type);
		break;
	}

	// See if it's already loaded
	Q_strncpyz (fixedName, name, sizeof(fixedName));
	variable = GUIVar_Find (fixedName, type);
	if (variable)
		return variable;

	// Create it
	if (gui_numVars+1 >= MAX_GUIVARS)
		Com_Error (ERR_DROP, "GUIVar_Register: MAX_GUIVARS");

	variable = &gui_varList[gui_numVars++];
	variable->name = gui_ttrDup (fixedName, GUITAG_VARS);
	variable->type = type;
	variable->modified = true;
	return variable;
}

/*
=============================================================================

	VALUE GETTING

=============================================================================
*/

/*
==================
GUIVar_GetFloatValue
==================
*/
bool GUIVar_GetFloatValue (guiVar_t *variable, float *dest)
{
	// Stupidity check
	if (!variable) {
		assert (0);
		Com_Printf (PRNT_ERROR, "GUIVar_GetFloatValue: NULL variable!\n");
		return false;
	}
	if (!dest) {
		assert (0);
		Com_Printf (PRNT_ERROR, "GUIVar_GetFloatValue: NULL dest!\n");
		return false;
	}
	if (variable->type != GVT_FLOAT) {
		Com_Printf (PRNT_ERROR, "GUIVar_GetFloatValue: variable '%s' is not type float!\n", variable->name);
		return false;
	}

	// Get the value
	*dest = variable->floatVal;
	return true;
}


/*
==================
GUIVar_GetStrValue
==================
*/
bool GUIVar_GetStrValue (guiVar_t *variable, char *dest, size_t size)
{
	// Stupidity check
	if (!variable) {
		assert (0);
		Com_Printf (PRNT_ERROR, "GUIVar_GetStrValue: NULL variable!\n");
		return false;
	}
	if (!dest) {
		assert (0);
		Com_Printf (PRNT_ERROR, "GUIVar_GetStrValue: NULL target!\n");
		return false;
	}
	if (!size) {
		assert (0);
		Com_Printf (PRNT_ERROR, "GUIVar_GetStrValue: invalid size!\n");
		return false;
	}
	if (variable->type != GVT_STR && variable->type != GVT_STR_PTR) {
		Com_Printf (PRNT_ERROR, "GUIVar_GetStrValue: variable '%s' is not type string!\n", variable->name);
		return false;
	}

	// Get the value
	if (!variable->strVal) {
		dest[0] = '\0';
	}
	else {
		if (strlen(variable->strVal)+1 > size)
			Com_Printf (PRNT_WARNING, "GUIVar_GetStrValue: dest size is shorter than value length, dest will be truncated!\n");

		Q_strncpyz (dest, variable->strVal, size);
	}
	return true;
}


/*
==================
GUIVar_GetVecValue
==================
*/
bool GUIVar_GetVecValue (guiVar_t *variable, vec4_t dest)
{
	// Stupidity check
	if (!variable) {
		assert (0);
		Com_Printf (PRNT_ERROR, "GUIVar_GetVecValue: NULL variable!\n");
		return false;
	}
	if (!dest) {
		assert (0);
		Com_Printf (PRNT_ERROR, "GUIVar_GetVecValue: NULL target!\n");
		return false;
	}
	if (variable->type != GVT_VEC) {
		Com_Printf (PRNT_ERROR, "GUIVar_GetVecValue: variable '%s' is not type vec!\n", variable->name);
		return false;
	}

	// Get the value
	dest[0] = variable->vecVal[0];
	dest[1] = variable->vecVal[1];
	dest[2] = variable->vecVal[2];
	dest[3] = variable->vecVal[3];
	return true;
}

/*
=============================================================================

	VALUE SETTING

=============================================================================
*/

/*
==================
GUIVar_SetFloatValue
==================
*/
void GUIVar_SetFloatValue (guiVar_t *variable, float value)
{
	// Stupidity check
	if (!variable) {
		assert (0);
		Com_Printf (PRNT_ERROR, "GUIVar_SetFloatValue: NULL variable!\n");
		return;
	}
	if (variable->type != GVT_FLOAT) {
		Com_Printf (PRNT_ERROR, "GUIVar_SetFloatValue: variable '%s' is not type float!\n", variable->name);
		return;
	}

	// Set the value
	variable->floatVal = value;
	variable->modified = true;
}


/*
==================
GUIVar_SetStrValue
==================
*/
void GUIVar_SetStrValue (guiVar_t *variable, char *value)
{
	// Stupidity check
	if (!variable) {
		assert (0);
		Com_Printf (PRNT_ERROR, "GUIVar_SetStrValue: NULL variable!\n");
		return;
	}
	if (variable->type != GVT_STR && variable->type != GVT_STR_PTR) {
		Com_Printf (PRNT_ERROR, "GUIVar_SetStrValue: variable '%s' is not type string!\n", variable->name);
		return;
	}
	if (strlen(value)+1 >= MAX_GV_STRLEN) {
		Com_Printf (PRNT_ERROR, "GUIVar_SetStrValue: value exceeds maximum value!\n", variable->name);
		return;
	}

	// Set the value
	if (variable->type == GVT_STR_PTR) {
		variable->strVal = value;
	}
	else {
		if (variable->strVal)
			GUI_MemFree (variable->strVal);
		variable->strVal = gui_ttrDup (value, GUITAG_VARS);
	}
	variable->modified = true;
}


/*
==================
GUIVar_SetVecValue
==================
*/
void GUIVar_SetVecValue (guiVar_t *variable, vec4_t value)
{
	// Stupidity check
	if (!variable) {
		assert (0);
		Com_Printf (PRNT_ERROR, "GUIVar_SetVecValue: NULL variable!\n");
		return;
	}
	if (variable->type != GVT_VEC) {
		Com_Printf (PRNT_ERROR, "GUIVar_SetVecValue: variable '%s' is not type vec!\n", variable->name);
		return;
	}

	// Set the value
	Vec4Copy (value, variable->vecVal);
	variable->modified = true;
}

/*
=============================================================================

	INIT / SHUTDOWN

=============================================================================
*/

static conCmd_t	*cmd_gui_varList;

/*
==================
GUIVar_Init
==================
*/
static void GUI_VarList_f ()
{
	guiVar_t	*variable;
	uint32		i;

	Com_Printf (0, "type  name\n");
	Com_Printf (0, "----- --------------------------------\n");
	for (i=0, variable=&gui_varList[0] ; i<gui_numVars ; variable++, i++) {
		switch (variable->type) {
		case GVT_FLOAT:	Com_Printf (0, "float ");	break;
		case GVT_STR:	Com_Printf (0, "str   ");	break;
		case GVT_VEC:	Com_Printf (0, "vec   ");	break;
		}

		Com_Printf (0, "%s\n", variable->name);
	}
	Com_Printf (0, "----- --------------------------------\n");
	Com_Printf (0, "%i vars total.\n", i);
}


/*
==================
GUIVar_Init
==================
*/
void GUIVar_Init ()
{
	// Register commands
	cmd_gui_varList = Cmd_AddCommand("gui_varList",	0, GUI_VarList_f,		"lists GUI variables");
}


/*
==================
GUIVar_Shutdown
==================
*/
void GUIVar_Shutdown ()
{
	guiVar_t	*variable;
	uint32		i;

	// Remove commands
	Cmd_RemoveCommand(cmd_gui_varList);

	// Free memory
	for (i=0, variable=&gui_varList[0] ; i<gui_numVars ; variable++, i++) {
		GUI_MemFree (variable->name);
		switch (variable->type) {
		case GVT_FLOAT:
			break;
		case GVT_STR:
			if (variable->strVal)
				GUI_MemFree (variable->strVal);
			break;
		case GVT_STR_PTR:
			break;
		case GVT_VEC:
			break;
		default:
			assert (0);
			break;
		}
	}

	// Anything missed will be caught with this.
	// (though nothing should be missed)
	GUI_FreeTag (GUITAG_VARS);

	// Clear slots
	gui_numVars = 0;
	memset (&gui_varList[0], 0, sizeof(guiVar_t) * MAX_GUIVARS);
}
