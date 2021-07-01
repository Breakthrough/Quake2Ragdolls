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
// gui_init.c
//

#include "gui_local.h"

static gui_t			cl_guiList[MAX_GUIS];
static guiShared_t		cl_guiSharedInfo[MAX_GUIS];
static uint32			cl_numGUI;

static uint32			cl_numGUIErrors;
static uint32			cl_numGUIWarnings;

static uint32			cl_guiRegFrames[MAX_GUIS];
static uint32			cl_guiRegTouched[MAX_GUIS];
static uint32			cl_guiRegFrameNum;

cVar_t	*gui_developer;
cVar_t	*gui_debugBounds;
cVar_t	*gui_debugScale;
cVar_t	*gui_mouseFilter;
cVar_t	*gui_mouseSensitivity;

/*
================
GUI_HashValue

String passed to this should be lowercase.
FIXME: use this! Though it may be unecessary since nested windows
will only be looked up while GUIs are being parsed
================
*/
static uint32 GUI_HashValue (const char *name)
{
	uint32	hashValue;
	int		ch, i;

	for (i=0, hashValue=0 ; *name ; i++) {
		ch = *(name++);
		hashValue = hashValue * 33 + ch;
	}

	return (hashValue + (hashValue >> 5)) & (MAX_GUI_HASH-1);
}


/*
================
GUI_FindGUI
================
*/
static gui_t *GUI_FindGUI (char *name)
{
	gui_t	*gui, *bestMatch;
	char	tempName[MAX_QPATH];
	uint32	bestNum, i;

	// Make sure it's lowercase
	Q_strncpyz (tempName, name, sizeof(tempName));
	Q_strlwr (tempName);

	bestMatch = NULL;
	bestNum = 0;

	// Look for it
	for (i=0, gui=cl_guiList ; i<cl_numGUI ; gui++, i++) {
		if (strcmp (gui->name, tempName))
			continue;

		if (!bestMatch || gui->shared->pathType >= gui->shared->pathType) {
			bestMatch = gui;
			bestNum = i;
		}
	}

	return bestMatch;
}


/*
================
GUI_FindWindow

lowerName must be lowercase!
================
*/
static gui_t *GUI_FindWindow (gui_t *gui, char *lowerName)
{
	gui_t	*child, *best;
	uint32	i;

	// See if it matches
	if (!strcmp (lowerName, gui->name))
		return gui;

	// Recurse down the children
	for (i=0, child=gui->childList ; i<gui->numChildren ; child++, i++) {
		best = GUI_FindWindow (child, lowerName);
		if (best)
			return best;
	}

	return NULL;
}


/*
===============
GUI_FindDefFloat
===============
*/
static short GUI_FindDefFloat (gui_t *gui, char *lowerName)
{
	defineFloat_t	*flt;
	short			i;

	for (i=0, flt=gui->s.defFloatList ; i<gui->s.numDefFloats ; flt++, i++) {
		if (!strcmp (lowerName, flt->name))
			break; 
	}
	if (i == gui->s.numDefFloats)
		return -1;

	return i;
}


/*
===============
GUI_FindDefVec
===============
*/
static short GUI_FindDefVec (gui_t *gui, char *lowerName)
{
	defineVec_t	*vec;
	short		i;

	for (i=0, vec=gui->s.defVecList ; i<gui->s.numDefVecs ; vec++, i++) {
		if (!strcmp (lowerName, vec->name))
			break; 
	}
	if (i == gui->s.numDefVecs)
		return -1;

	return i;
}


/*
===============
GUI_CvarValidate
===============
*/
static bool GUI_CvarValidate (const char *name)
{
	if (strchr (name, '\\'))
		return false;
	if (strchr (name, '\"'))
		return false;
	if (strchr (name, ';'))
		return false;

	return true;
}

/*
=============================================================================

	PARSE HELPERS

=============================================================================
*/

/*
==================
GUI_PrintPos
==================
*/
static void GUI_PrintPos (comPrint_t flags, parse_t *ps, char *fileName, gui_t *gui)
{
	uint32		line, col;

	// Increment tallies
	if (flags & PRNT_ERROR)
		cl_numGUIErrors++;
	else if (flags & PRNT_WARNING)
		cl_numGUIWarnings++;

	if (ps) {
		// Print the position
		PS_GetPosition (ps, &line, &col);
		if (gui)
			Com_Printf (flags, "%s(line #%i col#%i): window '%s':\n", fileName, line, col, gui->name);
		else
			Com_Printf (flags, "%s(line #%i col#%i):\n", fileName, line, col);
		return;
	}

		// Print the position
	Com_Printf (flags, "%s:\n", fileName);
}


/*
==================
GUI_DevPrintPos
==================
*/
static void GUI_DevPrintPos (comPrint_t flags, parse_t *ps, char *fileName, gui_t *gui)
{
	if (!gui_developer->intVal && !developer->intVal)
		return;

	GUI_PrintPos (flags, ps, fileName, gui);
}


/*
==================
GUI_PrintError
==================
*/
static void GUI_PrintError (char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAX_COMPRINT];

	cl_numGUIErrors++;

	// Evaluate args
	va_start (argptr, fmt);
	vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	// Print
	Com_ConPrint (PRNT_ERROR, msg);
}


/*
==================
GUI_PrintWarning
==================
*/
static void GUI_PrintWarning (char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAX_COMPRINT];

	cl_numGUIWarnings++;

	// Evaluate args
	va_start (argptr, fmt);
	vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	// Print
	Com_ConPrint (PRNT_WARNING, msg);
}


/*
==================
GUI_DevPrintf
==================
*/
static void GUI_DevPrintf (comPrint_t flags, parse_t *ps, char *fileName, gui_t *gui, char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAX_COMPRINT];

	if (!gui_developer->intVal && !developer->intVal)
		return;

	if (flags & (PRNT_ERROR|PRNT_WARNING)) {
		if (flags & PRNT_ERROR)
			cl_numGUIErrors++;
		else if (flags & PRNT_WARNING)
			cl_numGUIWarnings++;
		GUI_PrintPos (flags, ps, fileName, gui);
	}

	// Evaluate args
	va_start (argptr, fmt);
	vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	// Print
	Com_ConPrint (flags, msg);
}


/*
==================
GUI_ParseFloatRegister
==================
*/
static bool GUI_ParseFloatRegister (char *fileName, gui_t *gui, parse_t *ps, char *keyName, floatRegister_t *floatReg, bool requireValue, float defaultValue)
{
	char		source[MAX_PS_TOKCHARS];
	float		storage;
	char		windowName[MAX_GUI_NAMELEN];
	gui_t		*windowPtr;
	char		floatName[MAX_GUI_NAMELEN];
	short		floatNum;
	guiVar_t	*variable;
	char		*charToken;
	char		*p;

	if (!PS_ParseToken (ps, PSF_TO_LOWER, &charToken)) {
		if (!requireValue) {
			floatReg->sourceType = REG_SOURCE_SELF;
			floatReg->storage = defaultValue;
			return true;
		}

		GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
		GUI_PrintError ("ERROR: invalid/missing parameters for '%s'\n", keyName);
		return false;
	}

	if (charToken[0] == '$') {
		// Parse "[window::]var"
		Q_strncpyz (source, &charToken[1], sizeof(source));
		p = strstr (source, "::");
		if (p) {
			if (!*(p+2)) {
				GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
				GUI_PrintError ("ERROR: invalid argument for '%s', contains '::' with no flag name!\n", keyName);
				return false;
			}

			// "<window::>var"
			Q_strncpyz (windowName, source, sizeof(windowName));
			p = strstr (windowName, "::");
			*p = '\0';
			// "window::<var>"
			Q_strncpyz (floatName, p+2, sizeof(floatName));
		}
		else {
			// Default to this window
			Q_strncpyz (windowName, gui->name, sizeof(windowName));

			// "<var>"
			Q_strncpyz (floatName, source, sizeof(floatName));
		}

		// Check if we're looking for a guiVar
		if (!strcmp (windowName, "guivar")) {
			// Find the variable
			variable = GUIVar_Register (floatName, GVT_FLOAT);
			if (!variable) {
				GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
				GUI_PrintError ("ERROR: unable to find guivar '%s'\n", floatName);
				return false;
			}

			floatReg->sourceType = REG_SOURCE_GUIVAR;
			floatReg->guiVar = variable;
			return true;
		}
		else {
			// Find the window
			windowPtr = GUI_FindWindow (gui->owner, windowName);
			if (!windowPtr) {
				GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
				GUI_PrintError ("ERROR: unable to find window '%s'\n", windowName);
				return false;
			}

			// Find the defineFloat
			floatNum = GUI_FindDefFloat (windowPtr, floatName);
			if (floatNum == -1) {
				GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
				GUI_PrintError ("ERROR: unable to find float '%s'\n", floatName);
				return false;
			}

			floatReg->sourceType = REG_SOURCE_DEF;
			floatReg->defFloatIndex = floatNum;
			floatReg->defFloatWindow = windowPtr;
		}

		return true;
	}

	// Not a pointer, use value
	PS_UndoParse (ps);
	if (!PS_ParseDataType (ps, 0, PSDT_FLOAT, &storage, 1)) {
		GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
		GUI_PrintError ("ERROR: invalid/missing parameters for '%s'\n", keyName);
		return false;
	}

	floatReg->sourceType = REG_SOURCE_SELF;
	floatReg->storage = storage;
	return true;
}


/*
==================
GUI_ParseVectorRegister
==================
*/
static bool GUI_ParseVectorRegister (char *fileName, gui_t *gui, parse_t *ps, char *keyName, vecRegister_t *vecReg)
{
	char		source[MAX_PS_TOKCHARS];
	vec4_t		storage;
	char		windowName[MAX_GUI_NAMELEN];
	gui_t		*windowPtr;
	char		vecName[MAX_GUI_NAMELEN];
	short		vecNum;
	guiVar_t	*variable;
	char		*charToken;
	char		*p;

	if (!PS_ParseToken (ps, PSF_TO_LOWER, &charToken)) {
		GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
		GUI_PrintError ("ERROR: invalid/missing parameters for '%s'\n", keyName);
		return false;
	}

	if (charToken[0] == '$') {
		// Parse "[window::]var"
		Q_strncpyz (source, &charToken[1], sizeof(source));
		p = strstr (source, "::");
		if (p) {
			if (!*(p+2)) {
				GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
				GUI_PrintError ("ERROR: invalid argument for '%s', contains '::' with no flag name!\n", keyName);
				return false;
			}

			// "<window::>var"
			Q_strncpyz (windowName, source, sizeof(windowName));
			p = strstr (windowName, "::");
			*p = '\0';
			// "window::<var>"
			Q_strncpyz (vecName, p+2, sizeof(vecName));
		}
		else {
			// Default to this window
			Q_strncpyz (windowName, gui->name, sizeof(windowName));

			// "<var>"
			Q_strncpyz (vecName, source, sizeof(vecName));
		}

		// Check if we're looking for a guiVar
		if (!strcmp (windowName, "guivar")) {
			// Find the variable
			variable = GUIVar_Register (vecName, GVT_VEC);
			if (!variable) {
				GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
				GUI_PrintError ("ERROR: unable to find guivar '%s'\n", vecName);
				return false;
			}

			vecReg->sourceType = REG_SOURCE_GUIVAR;
			vecReg->guiVar = variable;
			return true;
		}
		else {
			// Find the window
			windowPtr = GUI_FindWindow (gui->owner, windowName);
			if (!windowPtr) {
				GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
				GUI_PrintError ("ERROR: unable to find window '%s'\n", windowName);
				return false;
			}

			// Find the defineVec
			vecNum = GUI_FindDefVec (windowPtr, vecName);
			if (vecNum == -1) {
				GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
				GUI_PrintError ("ERROR: unable to find vec '%s'\n", vecName);
				return false;
			}

			vecReg->sourceType = REG_SOURCE_DEF;
			vecReg->defVecIndex = vecNum;
			vecReg->defVecWindow = windowPtr;
		}

		return true;
	}

	// Not a pointer, use value
	PS_UndoParse (ps);
	if (!PS_ParseDataType (ps, 0, PSDT_FLOAT, &storage, 4)) {
		GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
		GUI_PrintError ("ERROR: invalid/missing parameters for '%s'\n", keyName);
		return false;
	}

	vecReg->sourceType = REG_SOURCE_SELF;
	Vec4Copy (storage, vecReg->storage);
	return true;
}

/*
=============================================================================

	KEY->FUNC PARSING

=============================================================================
*/

struct guiParseKey_t {
	const char		*keyWord;
	bool			(*func) (char *fileName, gui_t *gui, parse_t *ps, char *keyName);
};

/*
==================
GUI_CallKeyFunc
==================
*/
static bool GUI_CallKeyFunc (char *fileName, gui_t *gui, parse_t *ps, guiParseKey_t *keyList1, guiParseKey_t *keyList2, guiParseKey_t *keyList3, char *token)
{
	guiParseKey_t	*list, *key;
	char			keyName[MAX_PS_TOKCHARS];
	char			*str;

	// Copy off a lower-case copy for faster comparisons
	Q_strncpyz (keyName, token, sizeof(keyName));
	Q_strlwr (keyName);

	// Cycle through the key lists looking for a match
	for (list=keyList1 ; list ; ) {
		for (key=&list[0] ; key->keyWord ; key++) {
			// See if it matches the keyWord
			if (strcmp (key->keyWord, keyName))
				continue;

			// This is just to ignore any warnings
			if (!key->func) {
				PS_SkipLine (ps);
				return true;
			}

			// Failed to parse line
			if (!key->func (fileName, gui, ps, keyName)) {
				PS_SkipLine (ps);
				return false;
			}

			// Report any extra parameters
			if (PS_ParseToken (ps, PSF_TO_LOWER, &str)) {
				GUI_PrintPos (PRNT_WARNING, ps, fileName, gui);
				GUI_PrintWarning ("WARNING: unused trailing parameters after key '%s', \"%s\"\n", keyName, str);
				PS_SkipLine (ps);
				return true;
			}

			// Parsed fine
			return true;
		}

		// Next list
		if (list == keyList1)
			list = keyList2;
		else if (list == keyList2)
			list = keyList3;
		else
			break;
	}

	GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
	GUI_PrintError ("ERROR: unrecognized key: '%s'\n", keyName);
	return false;
}

/*
=============================================================================

	ITEMDEF PARSING

	The windowDef and itemDef's below can utilize all of these keys.
=============================================================================
*/

/*
==================
itemDef_item
==================
*/
static bool itemDef_item (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	bool	item;

	if (!PS_ParseDataType (ps, 0, PSDT_BOOLEAN, &item, 1)) {
		GUI_DevPrintPos (PRNT_WARNING, ps, fileName, gui);
		GUI_DevPrintf (PRNT_WARNING, ps, fileName, gui, "WARNING: missing '%s' paramter(s), using default\n");
		item = true;
	}

	if (item)
		gui->flags |= WFL_ITEM;
	else
		gui->flags &= ~WFL_ITEM;
	return true;
}

// ==========================================================================

static bool itemDef_rect (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	return GUI_ParseVectorRegister (fileName, gui, ps, keyName, &gui->s.vecRegisters[VR_RECT]);
}
static bool itemDef_rotate (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	return GUI_ParseFloatRegister (fileName, gui, ps, keyName, &gui->s.floatRegisters[FR_ROTATION], true, 0);
}

// ==========================================================================

/*
==================
itemDef_mat
==================
*/
static bool itemDef_mat (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	char	*str;

	if (!PS_ParseToken (ps, PSF_TO_LOWER, &str)) {
		GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
		GUI_PrintError ("ERROR: invalid/missing parameters for '%s'\n", keyName);
		return false;
	}

	Q_strncpyz (gui->matName, str, sizeof(gui->matName));
	gui->flags |= WFL_MATERIAL;
	return true;
}

static bool itemDef_fill (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	if (GUI_ParseVectorRegister (fileName, gui, ps, keyName, &gui->s.vecRegisters[VR_FILL_COLOR])) {
		gui->flags |= WFL_FILL_COLOR;
		return true;
	}
	return false;
}
static bool itemDef_matColor (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	return GUI_ParseVectorRegister (fileName, gui, ps, keyName, &gui->s.vecRegisters[VR_MAT_COLOR]);
}
static bool itemDef_matScaleX (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	return GUI_ParseFloatRegister (fileName, gui, ps, keyName, &gui->s.floatRegisters[FR_MAT_SCALE_X], true, 1);
}
static bool itemDef_matScaleY (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	return GUI_ParseFloatRegister (fileName, gui, ps, keyName, &gui->s.floatRegisters[FR_MAT_SCALE_Y], true, 1);
}

// ==========================================================================

/*
==================
itemDef_defineFloat
==================
*/
static bool itemDef_defineFloat (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	char			*name;
	float			floatToken;
	defineFloat_t	*flt;
	int				i;

	// Get the name
	if (!PS_ParseToken (ps, PSF_TO_LOWER, &name)) {
		GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
		GUI_PrintError ("ERROR: invalid/missing parameters for '%s'\n", keyName);
		return false;
	}

	// Check for duplicates
	for (i=0, flt=&gui->s.defFloatList[0] ; i<gui->s.numDefFloats ; flt++, i++) {
		if (strcmp (flt->name, name))
			continue;
		GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
		GUI_PrintError ("ERROR: %s '%s' name already in use!\n", keyName, name);
		return false;
	}

	// Store
	Q_strncpyz (gui->s.defFloatList[gui->s.numDefFloats].name, name, sizeof(gui->s.defFloatList[gui->s.numDefFloats].name));

	// Get the value
	if (!PS_ParseDataType (ps, 0, PSDT_FLOAT, &floatToken, 1)) {
		GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
		GUI_PrintError ("ERROR: invalid/missing parameters for '%s'\n", keyName);
		return false;
	}

	// Store
	gui->s.defFloatList[gui->s.numDefFloats].value = floatToken;
	gui->s.numDefFloats++;
	return true;
}


/*
==================
itemDef_defineVec
==================
*/
static bool itemDef_defineVec (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	char		*name;
	vec4_t		vecToken;
	defineVec_t	*vec;
	int			i;

	// Get the name
	if (!PS_ParseToken (ps, PSF_TO_LOWER, &name)) {
		GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
		GUI_PrintError ("ERROR: invalid/missing parameters for '%s'\n", keyName);
		return false;
	}

	// Check for duplicates
	for (i=0, vec=&gui->s.defVecList[0] ; i<gui->s.numDefVecs ; vec++, i++) {
		if (strcmp (vec->name, name))
			continue;
		GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
		GUI_PrintError ("ERROR: %s '%s' name already in use!\n", keyName, name);
		return false;
	}

	// Store
	Q_strncpyz (gui->s.defVecList[gui->s.numDefVecs].name, name, sizeof(gui->s.defVecList[gui->s.numDefVecs].name));

	// Get the value
	if (!PS_ParseDataType (ps, 0, PSDT_FLOAT, &vecToken[0], 4)) {
		GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
		GUI_PrintError ("ERROR: invalid/missing parameters for '%s'\n", keyName);
		return false;
	}

	// Store
	Vec4Copy (vecToken, gui->s.defVecList[gui->s.numDefVecs].value);
	gui->s.numDefVecs++;
	return true;
}

// ==========================================================================

static bool itemDef_modal (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	return GUI_ParseFloatRegister (fileName, gui, ps, keyName, &gui->s.floatRegisters[FR_MODAL], false, 1);
}
static bool itemDef_noEvents (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	return GUI_ParseFloatRegister (fileName, gui, ps, keyName, &gui->s.floatRegisters[FR_NO_EVENTS], false, 1);
}
static bool itemDef_noTime (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	return GUI_ParseFloatRegister (fileName, gui, ps, keyName, &gui->s.floatRegisters[FR_NO_TIME], false, 1);
}
static bool itemDef_visible (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	return GUI_ParseFloatRegister (fileName, gui, ps, keyName, &gui->s.floatRegisters[FR_VISIBLE], false, 1);
}
static bool itemDef_wantEnter (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	return GUI_ParseFloatRegister (fileName, gui, ps, keyName, &gui->s.floatRegisters[FR_WANT_ENTER], false, 1);
}

// ==========================================================================

/*
==================
event_newAction
==================
*/
struct eva_setDest_t {
	const char		*name;

	int				destNumVecs;
	uint32			destRegister;
	set_destType_t	destType;

	guiType_t		destWindowType;
};

static eva_setDest_t gui_tetDests[] = {
	// textDef
	{ "textcolor",			4,		VR_TEXT_COLOR,		EVA_SETDEST_VEC,	WTP_TEXT	},
	{ "texthovercolor",		4,		VR_TEXT_HOVERCOLOR,	EVA_SETDEST_VEC,	WTP_TEXT	},

	{ "textalign",			1,		FR_TEXT_ALIGN,		EVA_SETDEST_FLOAT,	WTP_TEXT	},
	{ "textscale",			1,		FR_TEXT_SCALE,		EVA_SETDEST_FLOAT,	WTP_TEXT	},
	{ "textshadow",			1,		FR_TEXT_SHADOW,		EVA_SETDEST_FLOAT,	WTP_TEXT	},

	// All windows
	{ "fillcolor",			4,		VR_FILL_COLOR,		EVA_SETDEST_VEC,	WTP_MAX		},
	{ "matcolor",			4,		VR_MAT_COLOR,		EVA_SETDEST_VEC,	WTP_MAX		},

	{ "matscalex",			1,		FR_MAT_SCALE_X,		EVA_SETDEST_FLOAT,	WTP_MAX		},
	{ "matscaley",			1,		FR_MAT_SCALE_Y,		EVA_SETDEST_FLOAT,	WTP_MAX		},

	{ "modal",				1,		FR_MODAL,			EVA_SETDEST_FLOAT,	WTP_MAX		},
	{ "noevents",			1,		FR_NO_EVENTS,		EVA_SETDEST_FLOAT,	WTP_MAX		},
	{ "notime",				1,		FR_NO_TIME,			EVA_SETDEST_FLOAT,	WTP_MAX		},
	{ "visible",			1,		FR_VISIBLE,			EVA_SETDEST_FLOAT,	WTP_MAX		},
	{ "wantenter",			1,		FR_WANT_ENTER,		EVA_SETDEST_FLOAT,	WTP_MAX		},

	{ NULL,					0,		0,					0,					WTP_MAX		},
};

static bool event_newAction (evAction_t *newAction, evaType_t type, char *fileName, gui_t *gui, parse_t *ps, char *keyName, bool gotSemicolon)
{
	eva_setDest_t	*setDest;
	char			target[MAX_PS_TOKCHARS];
	char			*charToken;
	char			*p;

	// If we got a semi-colon, certain types require args
	if (gotSemicolon) {
		switch (type) {
		case EVA_COMMAND:
		case EVA_LOCAL_SOUND:
		case EVA_RESET_TIME:
		case EVA_SET:
		case EVA_TRANSITION:
			GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
			GUI_PrintError ("ERROR: invalid/missing arguments for '%s'!\n", keyName);
			return false;
		}
	}

	// Parse arguments
	switch (type) {
	case EVA_CLOSE:
		break;

	case EVA_COMMAND:
		if (!PS_ParseToken (ps, PSF_ALLOW_NEWLINES|PSF_TO_LOWER, &charToken)) {
			GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
			GUI_PrintError ("ERROR: invalid/missing arguments for '%s'!\n", keyName);
			return false;
		}

		newAction->command = (char*)GUI_AllocTag (strlen(charToken)+2, GUITAG_SCRATCH);
		Q_snprintfz (newAction->command, strlen(charToken)+2, "%s\n", charToken);
		break;

	case EVA_IF:
		break;

	case EVA_LOCAL_SOUND:
		if (!PS_ParseToken (ps, PSF_ALLOW_NEWLINES|PSF_TO_LOWER, &charToken)) {
			GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
			GUI_PrintError ("ERROR: invalid/missing arguments for '%s'!\n", keyName);
			return false;
		}

		newAction->localSound = (eva_localSound_t*)GUI_AllocTag (sizeof(eva_localSound_t), GUITAG_SCRATCH);

		Com_NormalizePath (newAction->localSound->name, sizeof(newAction->localSound->name), charToken);

		// FIXME: Make a register?
		if (!PS_ParseDataType (ps, 0, PSDT_FLOAT, &newAction->localSound->volume, 1)) {
			GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
			GUI_PrintError ("ERROR: invalid/missing arguments for '%s'!\n", keyName);
			return false;
		}
		break;

	case EVA_NAMED_EVENT:
		// Get the "[window::]event" token in lower-case for faster lookup
		if (!PS_ParseToken (ps, PSF_ALLOW_NEWLINES|PSF_TO_LOWER, &charToken)) {
			GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
			GUI_PrintError ("ERROR: invalid/missing arguments for '%s'!\n", keyName);
			return false;
		}

		newAction->named = (eva_named_t*)GUI_AllocTag (sizeof(eva_named_t), GUITAG_SCRATCH);

		// Parse "[window::]event"
		Q_strncpyz (target, charToken, sizeof(target));
		p = strstr (target, "::");
		if (p) {
			// Make sure "window::<event>" exists
			if (!*(p+2)) {
				GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
				GUI_PrintError ("ERROR: invalid argument for '%s', contains '::' with no event name!\n", keyName);
				return false;
			}

			// "<window::>event"
			Q_strncpyz (newAction->named->destWindowName, target, sizeof(newAction->named->destWindowName));
			p = strstr (newAction->named->destWindowName, "::");
			*p = '\0';

			// "window::<event>"
			Q_strncpyz (newAction->named->eventName, p+2, sizeof(newAction->named->eventName));
		}
		else {
			// Default to this window
			Q_strncpyz (newAction->named->destWindowName, gui->name, sizeof(newAction->named->destWindowName));

			// "<event>"
			Q_strncpyz (newAction->named->eventName, target, sizeof(newAction->named->eventName));
		}
		break;

	case EVA_RESET_TIME:
		if (!PS_ParseDataType (ps, 0, PSDT_INTEGER, &newAction->resetTime, 1)) {
			GUI_PrintPos (PRNT_WARNING, ps, fileName, gui);
			GUI_PrintWarning ("WARNING: invalid/missing arguments for '%s', assuming '0'\n", keyName);
			newAction->resetTime = 0;
		}
		break;

	case EVA_SET:
		// Get the "[window::]register" token in lower-case for faster lookup
		if (!PS_ParseToken (ps, PSF_TO_LOWER, &charToken)) {
			GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
			GUI_PrintError ("ERROR: invalid/missing arguments for '%s'!\n", keyName);
			return false;
		}

		newAction->set = (eva_set_t*)GUI_AllocTag (sizeof(eva_set_t), GUITAG_SCRATCH);

		// Parse "[window::]register" destination
		Q_strncpyz (target, charToken, sizeof(target));
		p = strstr (target, "::");
		if (p) {
			// Make sure "window::<register>" exists
			if (!*(p+2)) {
				GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
				GUI_PrintError ("ERROR: invalid argument for '%s', contains '::' with no flag name!\n", keyName);
				return false;
			}

			// "<window::>register"
			Q_strncpyz (newAction->set->destWindowName, target, sizeof(newAction->set->destWindowName));
			p = strstr (newAction->set->destWindowName, "::");
			*p = '\0';

			// "window::<register>"
			Q_strncpyz (newAction->set->destVarName, p+2, sizeof(newAction->set->destVarName));
		}
		else {
			// Default to this window
			Q_strncpyz (newAction->set->destWindowName, gui->name, sizeof(newAction->set->destWindowName));

			// "<register>"
			Q_strncpyz (newAction->set->destVarName, target, sizeof(newAction->set->destVarName));
		}

		// Find the destination register type
		for (setDest=gui_tetDests ; setDest->name ; setDest++) {
			if (!strcmp (newAction->set->destVarName, setDest->name))
				break;
		}
		if (!setDest) {
			GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
			GUI_PrintError ("ERROR: invalid '%s' variable '%s'!\n", keyName, newAction->set->destVarName);
			return false;
		}

		// Store destination parms
		newAction->set->destNumVecs = setDest->destNumVecs;
		newAction->set->destRegister = setDest->destRegister;
		newAction->set->destType = setDest->destType;
		newAction->set->destWindowType = setDest->destWindowType;

		newAction->set->destType |= EVA_SETDEST_STORAGE;

		// Check if the source is a defineFloat/defineVec
		if (!PS_ParseToken (ps, PSF_TO_LOWER, &charToken)) {
			GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
			GUI_PrintError ("ERROR: invalid/missing arguments for '%s'!\n", keyName);
			return false;
		}

		if (charToken[0] == '$') {
			// Parse "[window::]var"
			Q_strncpyz (target, &charToken[1], sizeof(target));
			p = strstr (target, "::");
			if (p) {
				if (!*(p+2)) {
					GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
					GUI_PrintError ("ERROR: invalid argument for '%s', contains '::' with no flag name!\n", keyName);
					return false;
				}

				// "<window::>var"
				Q_strncpyz (newAction->set->srcWindowName, target, sizeof(newAction->set->srcWindowName));
				p = strstr (newAction->set->srcWindowName, "::");
				*p = '\0';

				// "window::<var>"
				Q_strncpyz (newAction->set->srcName, p+2, sizeof(newAction->set->srcName));

				// Check if we're looking for a guiVar
				if (!strcmp (newAction->set->srcWindowName, "guivar")) {
					newAction->set->srcType = EVA_SETSRC_GUIVAR;
					break;
				}
				else {
					// Nope, it's a defineFloat/defineVec
					newAction->set->srcType = EVA_SETSRC_DEF;
					break;
				}
			}
			else {
				// Default to this window
				Q_strncpyz (newAction->set->srcWindowName, gui->name, sizeof(newAction->set->srcWindowName));

				// "<var>"
				Q_strncpyz (newAction->set->srcName, target, sizeof(newAction->set->srcName));

				// defineFloat/defineVec
				newAction->set->srcType = EVA_SETSRC_DEF;
				break;
			}
		}

		// Not a pointer, parse the var setValue
		PS_UndoParse (ps);
		if (!PS_ParseDataType (ps, 0, PSDT_FLOAT, &newAction->set->srcStorage[0], setDest->destNumVecs)) {
			GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
			GUI_PrintError ("ERROR: invalid/missing arguments for '%s'!\n", keyName);
			return false;
		}

		// Store source parms
		newAction->set->srcType = EVA_SETSRC_STORAGE;
		break;

	case EVA_STOP_TRANSITIONS:
		break;

	case EVA_TRANSITION:
		break;
	}

	// Final ';'
	if (!gotSemicolon && (!PS_ParseToken (ps, PSF_ALLOW_NEWLINES|PSF_TO_LOWER, &charToken) || strcmp (charToken, ";"))) {
		GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
		GUI_PrintError ("ERROR: expecting ';' after %s <args>, got '%s'!\n", keyName, charToken);
		return false;
	}

	newAction->type = type;
	return true;
}


/*
==================
itemDef_newEvent
==================
*/
static bool itemDef_newEvent (evType_t type, char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	event_t		*newEvent;
	char		actionName[MAX_PS_TOKCHARS];
	evaType_t	action;
	evAction_t	*actionList;
	char		*token;
	int			len;
	bool		gotSemicolon;
	int			i;

	// Allocate a spot
	if (gui->numEvents+1 >= MAX_GUI_EVENTS) {
		GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
		GUI_PrintError ("ERROR: too many events!\n");
		return false;
	}
	newEvent = &gui->eventList[gui->numEvents];
	memset (&gui->eventList[gui->numEvents], 0, sizeof(event_t));
	newEvent->type = type;

	// Parse arguments
	switch (type) {
	case WEV_NAMED:
		if (!PS_ParseToken (ps, PSF_ALLOW_NEWLINES|PSF_TO_LOWER, &token) || !strcmp (token, "{")) {
			GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
			GUI_PrintError ("ERROR: invalid/missing arguments for '%s'!\n", keyName);
			return false;
		}

		newEvent->named = gui_ttrDup (token, GUITAG_SCRATCH);
		break;

	case WEV_TIME:
		if (!PS_ParseDataType (ps, PSF_ALLOW_NEWLINES, PSDT_INTEGER, &len, 1)) {
			GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
			GUI_PrintError ("ERROR: invalid/missing arguments for '%s'!\n", keyName);
			return false;
		}
		newEvent->onTime = len;
		break;
	}

	// Make sure the event doesn't already exist
	for (i=0 ; i<gui->numEvents ; i++) {
		if (gui->eventList[i].type != type)
			continue;

		// You can have multiple numbers of these if the args don't match
		switch (gui->eventList[i].type) {
		case WEV_NAMED:
			if (!strcmp (gui->eventList[i].named, newEvent->named)) {
				GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
				GUI_PrintError ("ERROR: event '%s' already exists (with the same arguments) for this window!\n", keyName);
				return false;
			}
			break;

		case WEV_TIME:
			if (gui->eventList[i].onTime == newEvent->onTime) {
				GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
				GUI_PrintError ("ERROR: event '%s' already exists (with the same arguments) for this window!\n", keyName);
				return false;
			}
			break;

		default:
			GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
			GUI_PrintError ("ERROR: event '%s' already exists for this window!\n", keyName);
			return false;
		}
	}

	// Next is the opening brace
	if (!PS_ParseToken (ps, PSF_ALLOW_NEWLINES|PSF_TO_LOWER, &token) || strcmp (token, "{")) {
		GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
		GUI_PrintError ("ERROR: expecting '{' after %s <args>, got '%s'!\n", keyName, token);
		return false;
	}

	// Storage space for children
	newEvent->numActions = 0;
	newEvent->actionList = (evAction_t*)GUI_AllocTag (sizeof(evAction_t) * MAX_EVENT_ACTIONS, GUITAG_SCRATCH);

	// Parse the actions
	for ( ; ; ) {
		if (!PS_ParseToken (ps, PSF_ALLOW_NEWLINES|PSF_TO_LOWER, &token) || !strcmp (token, "}"))
			break;

		// Store the name and trim the ';' if found
		// FIXME: start at the end, skipping whitespace until ';' is hit.
		Q_strncpyz (actionName, token, sizeof(actionName));
		len = strlen (actionName);
		if (actionName[len-1] == ';') {
			actionName[len-1] = '\0';
			gotSemicolon = true;
		}
		else {
			gotSemicolon = false;
		}

		// Find out the type
		if (!strcmp (actionName, "close"))
			action = EVA_CLOSE;
		else if (!strcmp (actionName, "command"))
			action = EVA_COMMAND;
		else if (!strcmp (actionName, "if"))
			action = EVA_IF;
		else if (!strcmp (actionName, "localsound"))
			action = EVA_LOCAL_SOUND;
		else if (!strcmp (actionName, "namedevent"))
			action = EVA_NAMED_EVENT;
		else if (!strcmp (actionName, "resettime"))
			action = EVA_RESET_TIME;
		else if (!strcmp (actionName, "set"))
			action = EVA_SET;
		else if (!strcmp (actionName, "stoptransitions"))
			action = EVA_STOP_TRANSITIONS;
		else if (!strcmp (actionName, "transition"))
			action = EVA_TRANSITION;
		else {
			GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
			GUI_PrintError ("ERROR: unknown action '%s'!\n", actionName);
			return false;
		}

		// Parse it
		if (!event_newAction (&newEvent->actionList[newEvent->numActions], action, fileName, gui, ps, actionName, gotSemicolon))
			return false;

		// Done
		newEvent->numActions++;
	}

	// Closing brace
	if (strcmp (token, "}")) {
		GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
		GUI_PrintError ("ERROR: expecting '}' after %s <args>, got '%s'!\n", keyName, token);
		return false;
	}

	// Store events
	actionList = newEvent->actionList;
	if (newEvent->numActions) {
		newEvent->actionList = (evAction_t*)GUI_AllocTag (sizeof(evAction_t) * newEvent->numActions, GUITAG_SCRATCH);
		memcpy (newEvent->actionList, actionList, sizeof(evAction_t) * newEvent->numActions);
		Mem_Free (actionList);
	}
	else
		newEvent->actionList = NULL;

	// Done
	gui->numEvents++;
	return true;
}

static bool itemDef_onAction (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	return itemDef_newEvent (WEV_ACTION, fileName, gui, ps, keyName);
}
static bool itemDef_onEsc (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	return itemDef_newEvent (WEV_ESCAPE, fileName, gui, ps, keyName);
}
static bool itemDef_onFrame (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	return itemDef_newEvent (WEV_FRAME, fileName, gui, ps, keyName);
}
static bool itemDef_onInit (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	return itemDef_newEvent (WEV_INIT, fileName, gui, ps, keyName);
}
static bool itemDef_onMouseEnter (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	return itemDef_newEvent (WEV_MOUSE_ENTER, fileName, gui, ps, keyName);
}
static bool itemDef_onMouseExit (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	return itemDef_newEvent (WEV_MOUSE_EXIT, fileName, gui, ps, keyName);
}
static bool itemDef_onNamedEvent (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	return itemDef_newEvent (WEV_NAMED, fileName, gui, ps, keyName);
}
static bool itemDef_onShutdown (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	return itemDef_newEvent (WEV_SHUTDOWN, fileName, gui, ps, keyName);
}
static bool itemDef_onTime (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	return itemDef_newEvent (WEV_TIME, fileName, gui, ps, keyName);
}

// ==========================================================================

static guiParseKey_t	cl_itemDefKeyList[] = {
	// Flags
	{ "item",					&itemDef_item				},

	// Orientation
	{ "rect",					&itemDef_rect				},
	{ "rotate",					&itemDef_rotate				},

	// Background
	{ "fill",					&itemDef_fill				},
	{ "mat",					&itemDef_mat				},
	{ "matcolor",				&itemDef_matColor			},
	{ "matscalex",				&itemDef_matScaleX			},
	{ "matscaley",				&itemDef_matScaleY			},

	// Defines
	{ "definefloat",			&itemDef_defineFloat		},
	{ "definevec",				&itemDef_defineVec			},

	// Registers
	{ "modal",					&itemDef_modal				},
	{ "noevents",				&itemDef_noEvents			},
	{ "notime",					&itemDef_noTime				},
	{ "visible",				&itemDef_visible			},
	{ "wantenter",				&itemDef_wantEnter			},

	// Events
	{ "onaction",				&itemDef_onAction			},
	{ "onesc",					&itemDef_onEsc				},
	{ "onframe",				&itemDef_onFrame			},
	{ "oninit",					&itemDef_onInit				},
	{ "onmouseenter",			&itemDef_onMouseEnter		},
	{ "onmouseexit",			&itemDef_onMouseExit		},
	{ "onnamedevent",			&itemDef_onNamedEvent		},
	{ "ontime",					&itemDef_onTime				},

	{ NULL,						NULL						}
};

/*
=============================================================================

	BINDDEF PARSING

=============================================================================
*/

/*
==================
bindDef_bind
==================
*/
static bool bindDef_bind (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	char		*str;
	keyNum_t	keyNum;

	if (!PS_ParseToken (ps, PSF_TO_LOWER, &str)) {
		GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
		GUI_PrintError ("ERROR: invalid/missing parameters for '%s'\n", keyName);
		return false;
	}
	
	keyNum = Key_StringToKeynum (str);
	if (keyNum == -1) {
		Com_Printf (0, "\"%s\" isn't a valid key\n", str);
		return false;
	}

	gui->s.bindDef->keyNum = keyNum;
	return true;
}

// ==========================================================================

static guiParseKey_t	cl_bindDefKeyList[] = {
	{ "bind",					&bindDef_bind				},
	{ NULL,						NULL						}
};

/*
=============================================================================

	CHECKDEF PARSING

=============================================================================
*/

/*
==================
checkDef_liveUpdate
==================
*/
static bool checkDef_liveUpdate (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	bool	liveUpdate;

	if (!PS_ParseDataType (ps, 0, PSDT_BOOLEAN, &liveUpdate, 1)) {
		GUI_DevPrintPos (PRNT_WARNING, ps, fileName, gui);
		GUI_DevPrintf (PRNT_WARNING, ps, fileName, gui, "WARNING: missing '%s' paramter(s), using default\n");
		liveUpdate = true;
	}

	gui->s.checkDef->liveUpdate = liveUpdate;
	return true;
}


/*
==================
checkDef_offMat
==================
*/
static bool checkDef_offMat (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	char	*str;

	if (!PS_ParseToken (ps, PSF_TO_LOWER, &str)) {
		GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
		GUI_PrintError ("ERROR: invalid/missing parameters for '%s'\n", keyName);
		return false;
	}

	Com_NormalizePath (gui->s.checkDef->offMatName, sizeof(gui->s.checkDef->offMatName), str);
	return true;
}


/*
==================
checkDef_onMat
==================
*/
static bool checkDef_onMat (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	char	*str;

	if (!PS_ParseToken (ps, PSF_TO_LOWER, &str)) {
		GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
		GUI_PrintError ("ERROR: invalid/missing parameters for '%s'\n", keyName);
		return false;
	}

	Com_NormalizePath (gui->s.checkDef->onMatName, sizeof(gui->s.checkDef->onMatName), str);
	return true;
}


/*
==================
checkDef_values
==================
*/
static bool checkDef_values (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	char	*str;
	char	*p;

	if (!PS_ParseToken (ps, PSF_TO_LOWER, &str)) {
		GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
		GUI_PrintError ("ERROR: invalid/missing parameters for '%s'\n", keyName);
		return false;
	}

	// Only has two values
	p = strchr (str, ';');
	if (!*p) {
		GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
		GUI_PrintError ("ERROR: invalid/missing parameters for '%s'\n", keyName);
		return false;
	}
	*p = '\0';
	gui->s.checkDef->values[0] = gui_ttrDup (str, GUITAG_SCRATCH);

	// Second value
	p++;
	if (!*p) {
		GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
		GUI_PrintError ("ERROR: invalid/missing parameters for '%s'\n", keyName);
		return false;
	}
	gui->s.checkDef->values[1] = gui_ttrDup (p, GUITAG_SCRATCH);

	return true;
}


/*
==================
checkDef_cvar
==================
*/
static bool checkDef_cvar (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	char	*str;

	if (!PS_ParseToken (ps, PSF_TO_LOWER, &str)) {
		GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
		GUI_PrintError ("ERROR: invalid/missing parameters for '%s'\n", keyName);
		return false;
	}

	if (!GUI_CvarValidate (str)) {
		GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
		GUI_PrintError ("ERROR: invalid cvar name for '%s'\n", keyName);
		return false;
	}

	gui->s.checkDef->cvar = Cvar_Exists (str);
	if (!gui->s.checkDef->cvar) {
		GUI_DevPrintf (PRNT_WARNING, ps, fileName, gui, "WARNING: cvar '%s' does not exist, creating\n", str);
		gui->s.checkDef->cvar = Cvar_Register (str, "0", 0);
	}

	return true;
}

// ==========================================================================

static guiParseKey_t	cl_checkDefKeyList[] = {
	{ "liveupdate",				&checkDef_liveUpdate		},
	{ "offmat",					&checkDef_offMat			},
	{ "onmat",					&checkDef_onMat				},
	{ "values",					&checkDef_values			},
	{ "cvar",					&checkDef_cvar				},
	{ NULL,						&checkDef_cvar				}
};

/*
=============================================================================

	CHOICEDEF PARSING

=============================================================================
*/

// ==========================================================================

static guiParseKey_t	cl_choiceDefKeyList[] = {
	{ "liveupdate",				NULL						},
	{ "choices",				NULL						},
	{ "choicetype",				NULL						},
	{ "currentchoice",			NULL						},
	{ "values",					NULL						},
	{ "cvar",					NULL						},
	{ NULL,						NULL						}
};

/*
=============================================================================

	EDITDEF PARSING

=============================================================================
*/

// ==========================================================================

static guiParseKey_t	cl_editDefKeyList[] = {
	{ "liveupdate",				NULL						},
	{ "maxchars",				NULL						},
	{ "numeric",				NULL						},
	{ "wrap",					NULL						},
	{ "readonly",				NULL						},
	{ "source",					NULL						},
	{ "password",				NULL						},
	{ "cvar",					NULL						},
	{ NULL,						NULL						}
};

/*
=============================================================================

	LISTDEF PARSING

=============================================================================
*/

/*
==================
listDef_scrollBar
==================
*/
static bool listDef_scrollBar (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	if (!PS_ParseDataType (ps, 0, PSDT_BOOLEAN, gui->s.listDef->scrollBar, 2)) {
		GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
		GUI_PrintError ("ERROR: invalid/missing parameters for '%s'\n", keyName);
		return false;
	}

	return true;
}

// ==========================================================================

static guiParseKey_t	cl_listDefKeyList[] = {
	{ "scrollbar",				&listDef_scrollBar			},
	{ NULL,						NULL						}
};

/*
=============================================================================

	RENDERDEF PARSING

=============================================================================
*/

// ==========================================================================

static guiParseKey_t	cl_renderDefKeyList[] = {
	{ "lightcolor",				NULL						},
	{ "lightorigin",			NULL						},
	{ "model",					NULL						},
	{ "modelcolor",				NULL						},
	{ "modelrotate",			NULL						},
	{ "vieworigin",				NULL						},
	{ "viewangles",				NULL						},
	{ NULL,						NULL						}
};

/*
=============================================================================

	SLIDERDEF PARSING

=============================================================================
*/

// ==========================================================================

static guiParseKey_t	cl_sliderDefKeyList[] = {
	{ "liveupdate",				NULL						},
	{ "low",					NULL						},
	{ "high",					NULL						},
	{ "step",					NULL						},
	{ "vertical",				NULL						},
	{ "thumbmaterial",			NULL						},
	{ "cvar",					NULL						},
	{ NULL,						NULL						}
};

/*
=============================================================================

	TEXTDEF PARSING

=============================================================================
*/

/*
==================
textDef_font
==================
*/
static bool textDef_font (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	char	*str;

	if (!PS_ParseToken (ps, PSF_TO_LOWER, &str)) {
		GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
		GUI_PrintError ("ERROR: invalid/missing parameters for '%s'\n", keyName);
		return false;
	}

	Com_NormalizePath (gui->s.textDef->fontName, sizeof(gui->s.textDef->fontName), str);
	return true;
}


/*
==================
textDef_text
==================
*/
static bool textDef_text (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	char	*token;
	int		len;

	if (!PS_ParseToken (ps, PSF_CONVERT_NEWLINE, &token)) {
		GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
		GUI_PrintError ("ERROR: invalid/missing parameters for '%s'\n", keyName);
		return false;
	}

	// Check length
	len = strlen (token);
	if (len >= MAX_TEXTDEF_STRLEN-1) {
		GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
		GUI_PrintError ("ERROR: parameter too long for '%s'\n", keyName);
		return false;
	}

	gui->s.textDef->textString = gui_ttrDup (token, GUITAG_SCRATCH);
	gui->s.textDef->textStringLen = len;
	return true;
}

static bool textDef_textAlign (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	return GUI_ParseFloatRegister (fileName, gui, ps, keyName, &gui->s.floatRegisters[FR_TEXT_ALIGN], true, 1);
}
static bool textDef_textColor (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	return GUI_ParseVectorRegister (fileName, gui, ps, keyName, &gui->s.vecRegisters[VR_TEXT_COLOR]);
}
static bool textDef_textHoverColor (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	return GUI_ParseVectorRegister (fileName, gui, ps, keyName, &gui->s.vecRegisters[VR_TEXT_HOVERCOLOR]);
}
static bool textDef_textScale (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	return GUI_ParseFloatRegister (fileName, gui, ps, keyName, &gui->s.floatRegisters[FR_TEXT_SCALE], true, 1);
}
static bool textDef_textShadow (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	return GUI_ParseFloatRegister (fileName, gui, ps, keyName, &gui->s.floatRegisters[FR_TEXT_SHADOW], false, 1);
}

// ==========================================================================

static guiParseKey_t	cl_textDefKeyList[] = {
	{ "font",					&textDef_font				},
	{ "text",					&textDef_text				},
	{ "textalign",				&textDef_textAlign			},
	{ "textcolor",				&textDef_textColor			},
	{ "texthovercolor",			&textDef_textHoverColor		},
	{ "textscale",				&textDef_textScale			},
	{ "textshadow",				&textDef_textShadow			},
	{ NULL,						NULL						}
};

/*
=============================================================================

	GUIDEF PARSING

=============================================================================
*/

/*
==================
guiDef_cursorMat
==================
*/
static bool guiDef_cursorMat (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	char	*str;

	if (!PS_ParseToken (ps, PSF_TO_LOWER, &str)) {
		GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
		GUI_PrintError ("ERROR: invalid/missing parameters for '%s'\n", keyName);
		return false;
	}

	Q_strncpyz (gui->shared->cursor.s.matName, str, sizeof(gui->shared->cursor.s.matName));
	gui->shared->cursor.s.visible = true;
	gui->flags |= WFL_CURSOR;
	return true;
}


/*
==================
guiDef_cursorColor
==================
*/
static bool guiDef_cursorColor (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	vec4_t	color;

	if (!PS_ParseDataType (ps, 0, PSDT_FLOAT, &color[0], 4)) {
		GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
		GUI_PrintError ("ERROR: invalid/missing parameters for '%s'\n", keyName);
		return false;
	}

	ColorNormalizef (color, gui->shared->cursor.s.color);
	gui->shared->cursor.s.color[3] = color[3];
	return true;
}


/*
==================
guiDef_cursorHeight
==================
*/
static bool guiDef_cursorHeight (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	float	height;

	if (!PS_ParseDataType (ps, 0, PSDT_FLOAT, &height, 1)) {
		GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
		GUI_PrintError ("ERROR: invalid/missing parameters for '%s'\n", keyName);
		return false;
	}

	gui->shared->cursor.s.size[1] = height;
	return true;
}


/*
==================
guiDef_cursorWidth
==================
*/
static bool guiDef_cursorWidth (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	float	width;

	if (!PS_ParseDataType (ps, 0, PSDT_FLOAT, &width, 1)) {
		GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
		GUI_PrintError ("ERROR: invalid/missing parameters for '%s'\n", keyName);
		return false;
	}

	gui->shared->cursor.s.size[0] = width;
	return true;
}


/*
==================
guiDef_cursorPos
==================
*/
static bool guiDef_cursorPos (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	vec2_t	pos;

	if (!PS_ParseDataType (ps, 0, PSDT_FLOAT, &pos[0], 2)) {
		GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
		GUI_PrintError ("ERROR: invalid/missing parameters for '%s'\n", keyName);
		return false;
	}

	Vec2Copy (pos, gui->shared->cursor.s.pos);
	return true;
}


/*
==================
guiDef_cursorVisible
==================
*/
static bool guiDef_cursorVisible (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	bool	visible;

	if (!PS_ParseDataType (ps, 0, PSDT_BOOLEAN, &visible, 1)) {
		GUI_DevPrintPos (PRNT_WARNING, ps, fileName, gui);
		GUI_DevPrintf (PRNT_WARNING, ps, fileName, gui, "WARNING: missing '%s' paramter(s), using default\n");
		visible = true;
	}

	gui->shared->cursor.s.visible = visible;
	return true;
}


static guiParseKey_t	cl_guiDefKeyList[] = {
	{ "cursormat",				&guiDef_cursorMat			},
	{ "cursorcolor",			&guiDef_cursorColor			},
	{ "cursorheight",			&guiDef_cursorHeight		},
	{ "cursorwidth",			&guiDef_cursorWidth			},
	{ "cursorpos",				&guiDef_cursorPos			},
	{ "cursorvisible",			&guiDef_cursorVisible		},
	{ NULL,						NULL						}
};

/*
=============================================================================

	WINDOWDEF PARSING

=============================================================================
*/

bool GUI_NewWindowDef (char *fileName, gui_t *gui, parse_t *ps, char *keyName);
static guiParseKey_t	cl_windowDefKeyList[] = {
	// Window types
	{ "windowdef",				&GUI_NewWindowDef			},
	{ "binddef",				&GUI_NewWindowDef			},
	{ "checkdef",				&GUI_NewWindowDef			},
	{ "choicedef",				&GUI_NewWindowDef			},
	{ "editdef",				&GUI_NewWindowDef			},
	{ "listdef",				&GUI_NewWindowDef			},
	{ "renderdef",				&GUI_NewWindowDef			},
	{ "sliderdef",				&GUI_NewWindowDef			},
	{ "textdef",				&GUI_NewWindowDef			},
	{ NULL,						NULL						}
};

static const char		*cl_windowDefTypes[] = {
	"guidef",		// WTP_GUI,
	"windowdef",	// WTP_GENERIC,

	"binddef",		// WTP_BIND,
	"checkdef",		// WTP_CHECKBOX,
	"choicedef",	// WTP_CHOICE,
	"editdef",		// WTP_EDIT,
	"listdef",		// WTP_LIST,
	"renderdef",	// WTP_RENDER,
	"sliderdef",	// WTP_SLIDER,
	"textdef",		// WTP_TEXT

	NULL,
};

/*
==================
GUI_NewWindowDef
==================
*/
bool GUI_NewWindowDef (char *fileName, gui_t *gui, parse_t *ps, char *keyName)
{
	char			windowName[MAX_GUI_NAMELEN];
	gui_t			*newGUI, *owner;
	char			*token;
	int				type;
	gui_t			*childList;
	event_t			*eventList;
	defineFloat_t	*defFloatList;
	defineVec_t		*defVecList;

	// First is the name
	if (!PS_ParseToken (ps, PSF_TO_LOWER, &token)) {
		GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
		GUI_PrintError ("ERROR: expecting <name> after %s, got '%s'!\n", keyName, token);
		return false;
	}
	if (strlen(token)+1 >= MAX_GUI_NAMELEN) {
		GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
		GUI_PrintError ("ERROR: %s '%s' name too long!\n", keyName, token);
		return false;
	}
	Q_strncpyz (windowName, token, sizeof(windowName));

	// Check for duplicates
	if (gui && GUI_FindWindow (gui->owner, windowName)) {
		GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
		GUI_PrintError ("ERROR: %s '%s' name already in use!\n", keyName, token);
		return false;
	}

	// Make sure the name is allowed
	if (!strcmp (windowName, "guivar")) {
		GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
		GUI_PrintError ("ERROR: %s '%s' name not allowed!\n", keyName, token);
		return false;
	}
	if (strchr (windowName, '$') || strstr (windowName, "::")) {
		GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
		GUI_PrintError ("ERROR: %s '%s' name must not contain '$' or '::'!\n", keyName, token);
		return false;
	}

	// Next is the opening brace
	if (!PS_ParseToken (ps, PSF_ALLOW_NEWLINES|PSF_TO_LOWER, &token) || strcmp (token, "{")) {
		GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
		GUI_PrintError ("ERROR: expecting '{' after %s <name>, got '%s'!\n", keyName, token);
		return false;
	}

	// Find out the type
	// This should always be valid since we only reach this point through keyFunc's
	for (type=0 ; type<WTP_MAX; type++) {
		if (!strcmp (cl_windowDefTypes[type], keyName))
			break;
	}

	// Allocate a space
	if (gui) {
		// Add to the parent
		if (gui->numChildren+1 >= MAX_GUI_CHILDREN) {
			GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
			GUI_PrintError ("ERROR: too many children!\n");
			return false;
		}
		newGUI = &gui->childList[gui->numChildren];
		newGUI->parent = gui;
		newGUI->shared = gui->shared;
		Q_strncpyz (newGUI->name, windowName, sizeof(newGUI->name));

		// Find the owner
		for (owner=gui ; owner ; owner=owner->parent)
			newGUI->owner = owner;

		// Increment parent children
		gui->numChildren++;
	}
	else {
		// This is a parent
		if (cl_numGUI+1 >= MAX_GUIS) {
			GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
			GUI_PrintError ("ERROR: too many GUIs!\n");
			return false;
		}
		newGUI = &cl_guiList[cl_numGUI];
		memset (&cl_guiList[cl_numGUI], 0, sizeof(gui_t));
		Q_strncpyz (newGUI->name, windowName, sizeof(newGUI->name));

		newGUI->shared = &cl_guiSharedInfo[cl_numGUI];
		memset (&cl_guiSharedInfo[cl_numGUI], 0, sizeof(guiShared_t));
		Vec4Set (newGUI->shared->cursor.s.color, 1, 1, 1, 1);
		newGUI->shared->cursor.s.visible = true;

		newGUI->owner = newGUI;

		cl_numGUI++;
	}

	// Allocate space and set default values
	newGUI->type = (guiType_t)type;
	Vec4Set (newGUI->s.vecRegisters[VR_MAT_COLOR].storage, 1, 1, 1, 1);
	newGUI->s.floatRegisters[FR_MAT_SCALE_X].storage = 1;
	newGUI->s.floatRegisters[FR_MAT_SCALE_Y].storage = 1;
	newGUI->s.floatRegisters[FR_VISIBLE].storage = 1;

	switch (type) {
	case WTP_GUI:
		break;

	case WTP_GENERIC:
		break;

	case WTP_BIND:
		newGUI->flags |= WFL_ITEM;
		newGUI->s.bindDef = (bindDef_t*)GUI_AllocTag (sizeof(bindDef_t) * 2, GUITAG_SCRATCH);
		newGUI->d.bindDef = newGUI->s.bindDef + 1;
		break;

	case WTP_CHECKBOX:
		newGUI->flags |= WFL_ITEM;
		newGUI->s.checkDef = (checkDef_t*)GUI_AllocTag (sizeof(checkDef_t) * 2, GUITAG_SCRATCH);
		newGUI->d.checkDef = newGUI->s.checkDef + 1;

		Q_strncpyz (newGUI->s.checkDef->offMatName, "guis/assets/textures/items/check_off.tga", sizeof(newGUI->s.checkDef->offMatName));
		Q_strncpyz (newGUI->s.checkDef->onMatName, "guis/assets/textures/items/check_on.tga", sizeof(newGUI->s.checkDef->onMatName));
		break;

	case WTP_CHOICE:
		newGUI->flags |= WFL_ITEM;
		newGUI->s.choiceDef = (choiceDef_t*)GUI_AllocTag (sizeof(choiceDef_t) * 2, GUITAG_SCRATCH);
		newGUI->d.choiceDef = newGUI->s.choiceDef + 1;
		break;

	case WTP_EDIT:
		newGUI->flags |= WFL_ITEM;
		newGUI->s.editDef = (editDef_t*)GUI_AllocTag (sizeof(editDef_t) * 2, GUITAG_SCRATCH);
		newGUI->d.editDef = newGUI->s.editDef + 1;
		break;

	case WTP_LIST:
		newGUI->flags |= WFL_ITEM;
		newGUI->s.listDef = (listDef_t*)GUI_AllocTag (sizeof(listDef_t) * 2, GUITAG_SCRATCH);
		newGUI->d.listDef = newGUI->s.listDef + 1;

		newGUI->s.listDef->scrollBar[0] = true;
		newGUI->s.listDef->scrollBar[1] = true;
		break;

	case WTP_RENDER:
		newGUI->flags |= WFL_ITEM;
		newGUI->s.renderDef = (renderDef_t*)GUI_AllocTag (sizeof(renderDef_t) * 2, GUITAG_SCRATCH);
		newGUI->d.renderDef = newGUI->s.renderDef + 1;
		break;

	case WTP_SLIDER:
		newGUI->flags |= WFL_ITEM;
		newGUI->s.sliderDef = (sliderDef_t*)GUI_AllocTag (sizeof(sliderDef_t) * 2, GUITAG_SCRATCH);
		newGUI->d.sliderDef = newGUI->s.sliderDef + 1;
		break;

	case WTP_TEXT:
		newGUI->flags |= WFL_ITEM;
		newGUI->s.textDef = (textDef_t*)GUI_AllocTag (sizeof(textDef_t) * 2, GUITAG_SCRATCH);
		newGUI->d.textDef = newGUI->s.textDef + 1;

		Q_strncpyz (newGUI->s.textDef->fontName, "default", sizeof(newGUI->s.textDef->fontName));
		Vec4Set (newGUI->s.vecRegisters[VR_TEXT_COLOR].storage, 1, 1, 1, 1);
		newGUI->s.floatRegisters[FR_TEXT_SCALE].storage = 1;
		break;
	}

	// Storage space for children
	newGUI->numChildren = 0;
	newGUI->childList = (gui_t*)GUI_AllocTag (sizeof(gui_t) * MAX_GUI_CHILDREN, GUITAG_SCRATCH);
	newGUI->numEvents = 0;
	newGUI->eventList = (event_t*)GUI_AllocTag (sizeof(event_t) * MAX_GUI_EVENTS, GUITAG_SCRATCH);
	newGUI->s.numDefFloats = 0;
	newGUI->s.defFloatList = (defineFloat_t*)GUI_AllocTag (sizeof(defineFloat_t) * MAX_GUI_DEFINES, GUITAG_SCRATCH);
	newGUI->s.numDefVecs = 0;
	newGUI->s.defVecList = (defineVec_t*)GUI_AllocTag (sizeof(defineVec_t) * MAX_GUI_DEFINES, GUITAG_SCRATCH);

	// Parse the keys
	for ( ; ; ) {
		if (!PS_ParseToken (ps, PSF_ALLOW_NEWLINES|PSF_TO_LOWER, &token) || !strcmp (token, "}"))
			break;

		switch (type) {
		case WTP_GUI:
			if (!GUI_CallKeyFunc (fileName, newGUI, ps, cl_guiDefKeyList, cl_itemDefKeyList, cl_windowDefKeyList, token))
				return false;
			break;
		case WTP_GENERIC:
			if (!GUI_CallKeyFunc (fileName, newGUI, ps, cl_itemDefKeyList, cl_windowDefKeyList, NULL, token))
				return false;
			break;
		case WTP_BIND:
			if (!GUI_CallKeyFunc (fileName, newGUI, ps, cl_itemDefKeyList, cl_bindDefKeyList, NULL, token))
				return false;
			break;
		case WTP_CHECKBOX:
			if (!GUI_CallKeyFunc (fileName, newGUI, ps, cl_itemDefKeyList, cl_checkDefKeyList, NULL, token))
				return false;
			break;
		case WTP_CHOICE:
			if (!GUI_CallKeyFunc (fileName, newGUI, ps, cl_itemDefKeyList, cl_choiceDefKeyList, NULL, token))
				return false;
			break;
		case WTP_EDIT:
			if (!GUI_CallKeyFunc (fileName, newGUI, ps, cl_itemDefKeyList, cl_editDefKeyList, NULL, token))
				return false;
			break;
		case WTP_LIST:
			if (!GUI_CallKeyFunc (fileName, newGUI, ps, cl_itemDefKeyList, cl_listDefKeyList, NULL, token))
				return false;
			break;
		case WTP_RENDER:
			if (!GUI_CallKeyFunc (fileName, newGUI, ps, cl_itemDefKeyList, cl_renderDefKeyList, NULL, token))
				return false;
			break;
		case WTP_SLIDER:
			if (!GUI_CallKeyFunc (fileName, newGUI, ps, cl_itemDefKeyList, cl_sliderDefKeyList, NULL, token))
				return false;
			break;
		case WTP_TEXT:
			if (!GUI_CallKeyFunc (fileName, newGUI, ps, cl_itemDefKeyList, cl_textDefKeyList, NULL, token))
				return false;
			break;
		}
	}

	// Final '}' closing brace
	if (strcmp (token, "}")) {
		GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
		GUI_PrintError ("ERROR: expecting '}' after %s, got '%s'!\n", keyName, token);
		return false;
	}

	// Check for required values
	switch (type) {
	case WTP_CHECKBOX:
		if (!newGUI->s.checkDef->cvar) {
			GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
			GUI_PrintError ("ERROR: missing required 'cvar' value!\n");
			return false;
		}
		if (!newGUI->s.checkDef->values[0] || !newGUI->s.checkDef->values[1]) {
			GUI_PrintPos (PRNT_ERROR, ps, fileName, gui);
			GUI_PrintError ("ERROR: missing required 'values' value!\n");
			return false;
		}
		break;
	}

	// Store children
	childList = newGUI->childList;
	if (newGUI->numChildren) {
		newGUI->childList = (gui_t*)GUI_AllocTag (sizeof(gui_t) * newGUI->numChildren, GUITAG_SCRATCH);
		memcpy (newGUI->childList, childList, sizeof(gui_t) * newGUI->numChildren);
	}
	else
		newGUI->childList = NULL;
	Mem_Free (childList);

	// Store events
	eventList = newGUI->eventList;
	if (newGUI->numEvents) {
		newGUI->eventList = (event_t*)GUI_AllocTag (sizeof(event_t) * newGUI->numEvents, GUITAG_SCRATCH);
		memcpy (newGUI->eventList, eventList, sizeof(event_t) * newGUI->numEvents);
	}
	else
		newGUI->eventList = NULL;
	Mem_Free (eventList);

	// Store floats
	defFloatList = newGUI->s.defFloatList;
	if (newGUI->s.numDefFloats) {
		newGUI->s.defFloatList = (defineFloat_t*)GUI_AllocTag (sizeof(defineFloat_t) * newGUI->s.numDefFloats, GUITAG_SCRATCH);
		memcpy (newGUI->s.defFloatList, defFloatList, sizeof(defineFloat_t) * newGUI->s.numDefFloats);

		newGUI->d.defFloatList = (defineFloat_t*)GUI_AllocTag (sizeof(defineFloat_t) * newGUI->s.numDefFloats, GUITAG_SCRATCH);
		memcpy (newGUI->d.defFloatList, defFloatList, sizeof(defineFloat_t) * newGUI->d.numDefFloats);
	}
	else {
		newGUI->s.defFloatList = NULL;
		newGUI->d.defFloatList = NULL;
	}
	Mem_Free (defFloatList);

	// Store vecs
	defVecList = newGUI->s.defVecList;
	if (newGUI->s.numDefVecs) {
		newGUI->s.defVecList = (defineVec_t*)GUI_AllocTag (sizeof(defineVec_t) * newGUI->s.numDefVecs, GUITAG_SCRATCH);
		memcpy (newGUI->s.defVecList, defVecList, sizeof(defineVec_t) * newGUI->s.numDefVecs);

		newGUI->d.defVecList = (defineVec_t*)GUI_AllocTag (sizeof(defineVec_t) * newGUI->s.numDefVecs, GUITAG_SCRATCH);
		memcpy (newGUI->d.defVecList, defVecList, sizeof(defineVec_t) * newGUI->d.numDefVecs);
	}
	else {
		newGUI->s.defVecList = NULL;
		newGUI->d.defVecList = NULL;
	}
	Mem_Free (defVecList);

	// Done
	return true;
}

/*
=============================================================================

	GUI SCRIPT PARSING

=============================================================================
*/

/*
==================
GUI_ParseScript
==================
*/
static void GUI_ParseScript (char *fixedName, guiPathType_t pathType)
{
	char	keyName[MAX_PS_TOKCHARS];
	char	*buf;
	char	*token;
	int		fileLen;
	parse_t	*ps;

	if (!fixedName)
		return;

	// Load the file
	fileLen = FS_LoadFile (fixedName, (void **)&buf, true); 
	if (!buf || fileLen <= 0) {
		GUI_PrintWarning ("WARNING: can't load '%s' -- %s\n", fixedName, (fileLen == -1) ? "not found" : "empty file");
		return;
	}

	Com_Printf (0, "...loading '%s'\n", fixedName);

	// Start parsing
	ps = PS_StartSession (buf, PSP_COMMENT_BLOCK|PSP_COMMENT_LINE);

	// Parse the script
	for ( ; ; ) {
		if (!PS_ParseToken (ps, PSF_ALLOW_NEWLINES|PSF_TO_LOWER, &token))
			break;

		// Search for a guiDef
		if (strcmp (token, "guidef")) {
			GUI_PrintPos (PRNT_ERROR, ps, fixedName, NULL);
			GUI_PrintError ("ERROR: expecting 'guiDef' got '%s'!\n", token);
			GUI_PrintError ("ERROR: removing GUI and halting on file due to errors!\n");
			break;
		}

		// Found one, create it
		Q_strncpyz (keyName, token, sizeof(keyName));
		if (GUI_NewWindowDef (fixedName, NULL, ps, keyName)) {
			// It parsed ok, store the pathType
			cl_guiSharedInfo[cl_numGUI-1].pathType = pathType;
			Mem_ChangeTag (cl_guiSysPool, GUITAG_SCRATCH, GUITAG_KEEP);
		}
		else {
			// Failed to parse, halt!
			GUI_PrintError ("ERROR: removing GUI and halting on file due to errors!\n");
			memset (&cl_guiList[--cl_numGUI], 0, sizeof(gui_t));
			break;
		}
	}

	// Done
	PS_AddErrorCount (ps, &cl_numGUIErrors, &cl_numGUIWarnings);
	PS_EndSession (ps);
	FS_FreeFile (buf);
	GUI_FreeTag (GUITAG_SCRATCH);
}

/*
=============================================================================

	REGISTRATION

=============================================================================
*/

/*
==================
GUI_TouchGUI
==================
*/
static void GUI_CantFindMedia (gui_t *gui, char *item, char *name)
{
	if (gui && gui->name[0]) {
		GUI_PrintWarning ("WARNING: GUI '%s': can't find %s '%s'\n", gui->name, item, name);
		return;
	}

	GUI_PrintWarning ("WARNING: can't find %s '%s'\n", item, name);
}
static void GUI_RegisterFloats (floatRegister_t *floats, int maxFloats)
{
	int		i;

	for (i=0 ; i<maxFloats ; i++) {
		switch (floats[i].sourceType) {
		case REG_SOURCE_SELF:
			floats[i].variable = &floats[i].storage;
			break;
		case REG_SOURCE_DEF:
			floats[i].variable = &floats[i].defFloatWindow->d.defFloatList[floats[i].defFloatIndex].value;
			break;
		case REG_SOURCE_GUIVAR:
			floats[i].variable = &floats[i].guiVar->floatVal;
			break;
		default:
			assert (0);
			break;
		}
	}
}
static void GUI_RegisterVecs (vecRegister_t *vecs, int maxVecs)
{
	int		i;

	for (i=0 ; i<maxVecs ; i++) {
		switch (vecs[i].sourceType) {
		case REG_SOURCE_SELF:
			vecs[i].variable = &vecs[i].storage[0];
			break;
		case REG_SOURCE_DEF:
			vecs[i].variable = &vecs[i].defVecWindow->d.defVecList[vecs[i].defVecIndex].value[0];
			break;
		case REG_SOURCE_GUIVAR:
			vecs[i].variable = &vecs[i].guiVar->vecVal[0];
			break;
		default:
			assert (0);
			break;
		}
	}
}
static void GUI_r_TouchGUI (gui_t *gui)
{
	gui_t		*child;
	event_t		*event;
	evAction_t	*action;
	int			i, j;
	short		index;

	// Load generic media
	if (gui->flags & WFL_MATERIAL) {
		gui->mat = R_RegisterPic (gui->matName);
		if (!gui->mat)
			GUI_CantFindMedia (gui, "Material", gui->matName);
	}

	GUI_RegisterFloats (gui->s.floatRegisters, FR_MAX);
	GUI_RegisterVecs (gui->s.vecRegisters, VR_MAX);

	// Load type-specific media
	switch (gui->type) {
	case WTP_GUI:
		break;

	case WTP_GENERIC:
		break;

	case WTP_BIND:
		break;

	case WTP_CHECKBOX:
		// Find on/off materials
		gui->s.checkDef->offMatPtr = R_RegisterPic (gui->s.checkDef->offMatName);
		if (!gui->s.checkDef->offMatPtr) {
			GUI_CantFindMedia (gui, "offMat", gui->s.checkDef->offMatName);
			break;
		}
		gui->s.checkDef->onMatPtr = R_RegisterPic (gui->s.checkDef->onMatName);
		if (!gui->s.checkDef->onMatPtr) {
			GUI_CantFindMedia (gui, "onMat", gui->s.checkDef->onMatName);
			break;
		}
		break;

	case WTP_CHOICE:
		break;

	case WTP_EDIT:
		break;

	case WTP_LIST:
		break;

	case WTP_RENDER:
		break;

	case WTP_SLIDER:
		break;

	case WTP_TEXT:
		// Find the font
		gui->s.textDef->fontPtr = R_RegisterFont (gui->s.textDef->fontName);
		if (!gui->s.textDef->fontPtr)
			GUI_CantFindMedia (gui, "Font", gui->s.textDef->fontName);
		break;
	}

	// Load event-action specific media
	for (i=0, event=gui->eventList ; i<gui->numEvents ; event++, i++) {
		for (j=0, action=event->actionList ; j<event->numActions ; action++, j++) {
			switch (action->type) {
			case EVA_CLOSE:
				break;

			case EVA_COMMAND:
				break;

			case EVA_IF:
				break;

			case EVA_LOCAL_SOUND:
				// Find the sound
				action->localSound->sound = Snd_RegisterSound (action->localSound->name);
				if (!action->localSound->sound)
					GUI_CantFindMedia (gui, "Sound", action->localSound->name);
				break;

			case EVA_NAMED_EVENT:
				// Find the window
				action->named->destWindowPtr = GUI_FindWindow (gui->owner, action->named->destWindowName);
				if (!action->named->destWindowPtr) {
					GUI_CantFindMedia (gui, "Window", action->named->destWindowName);
					break;
				}
				break;

			case EVA_RESET_TIME:
				break;

			case EVA_SET:
				// Find the destination window
				action->set->destWindowPtr = GUI_FindWindow (gui->owner, action->set->destWindowName);
				if (!action->set->destWindowPtr) {
					GUI_CantFindMedia (gui, "Window", action->set->destWindowName);
					break;
				}

				// Check if it's allowed
				if (action->set->destWindowType != WTP_MAX && action->set->destWindowPtr->type != action->set->destWindowType)  {
					GUI_PrintError ("ERROR: Invalid set action destination '%s::%s'!\n", action->set->destWindowPtr->name, action->set->destVarName);
					action->set->destWindowPtr = NULL;
					break;
				}

				switch (action->set->srcType) {
				case EVA_SETSRC_DEF:
					// Find the source window
					action->set->srcWindowPtr = GUI_FindWindow (gui->owner, action->set->srcWindowName);
					if (!action->set->srcWindowPtr) {
						GUI_CantFindMedia (gui, "Window", action->set->srcWindowName);
						break;
					}

					// Find the source defineFloat/defineVec
					if (action->set->destType & EVA_SETDEST_FLOAT)
						index = GUI_FindDefFloat (action->set->srcWindowPtr, action->set->srcName);
					else if (action->set->destType & EVA_SETDEST_VEC)
						index = GUI_FindDefVec (action->set->srcWindowPtr, action->set->srcName);
					if (index == -1) {
						GUI_CantFindMedia (gui, "Define", action->set->srcName);
						break;
					}
					break;

				case EVA_SETSRC_GUIVAR:
					// Register the GUIVar
					if (action->set->destType & EVA_SETDEST_FLOAT)
						action->set->srcGUIVar = GUIVar_Register (action->set->srcName, GVT_FLOAT);
					else if (action->set->destType & EVA_SETDEST_VEC)
						action->set->srcGUIVar = GUIVar_Register (action->set->srcName, GVT_VEC);
					break;
				}
				break;

			case EVA_STOP_TRANSITIONS:
				break;

			case EVA_TRANSITION:
				break;
			}
		}
	}

	// Recurse down the children
	for (i=0, child=gui->childList ; i<gui->numChildren ; child++, i++)
		GUI_r_TouchGUI (child);
}
static void GUI_TouchGUI (gui_t *gui)
{
	// Register cursor media
	if (gui->flags & WFL_CURSOR) {
		gui->shared->cursor.s.matPtr = R_RegisterPic (gui->shared->cursor.s.matName);
		if (!gui->shared->cursor.s.matPtr)
			GUI_CantFindMedia (gui, "cursorMat", gui->shared->cursor.s.matName);
	}

	// Register base media
	GUI_r_TouchGUI (gui);
}


/*
==================
GUI_RegisterGUI
==================
*/
gui_t *GUI_RegisterGUI (char *name)
{
	gui_t	*gui;

	// Find it
	gui = GUI_FindGUI (name);
	if (!gui)
		return NULL;

	// Touch it
	GUI_TouchGUI (gui);
	cl_guiRegTouched[gui-&cl_guiList[0]] = cl_guiRegFrameNum;

	// Reset state
	GUI_ResetGUIState (gui);
	return gui;
}


/*
==================
GUI_RegisterSounds

On snd_restart this will be called.
==================
*/
void GUI_RegisterSounds ()
{
	// FIXME: k just register sounds again
	GUI_EndRegistration ();
}


/*
==================
GUI_BeginRegistration
==================
*/
void GUI_BeginRegistration ()
{
	cl_guiRegFrameNum++;
}


/*
==================
GUI_EndRegistration
==================
*/
void GUI_EndRegistration ()
{
	gui_t	*gui;
	uint32	i;

	// Register media specific to GUI windows
	for (i=0, gui=cl_guiList ; i<cl_numGUI ; gui++, i++) {
		if (cl_guiRegTouched[i] != cl_guiRegFrameNum
		&& cl_guiRegFrames[i] != cl_guiRegFrameNum)
			continue;	// Don't touch if already touched and if it wasn't scheduled for a touch

		cl_guiRegTouched[i] = cl_guiRegFrameNum;
		GUI_TouchGUI (gui);
	}
}

/*
=============================================================================

	INIT / SHUTDOWN

=============================================================================
*/

static conCmd_t *cmd_gui_list;
static conCmd_t *cmd_gui_namedEvent;
static conCmd_t	*cmd_gui_restart;
static conCmd_t *cmd_gui_test;

/*
==================
GUI_List_f
==================
*/
static void GUI_r_List_f (gui_t *window, uint32 depth)
{
	gui_t	*child;
	uint32	i;

	for (i=0 ; i<depth ; i++)
		Com_Printf (0, "   ");
	Com_Printf (0, "%s (%i children)\n", window->name, window->numChildren);

	// Recurse down the children
	for (i=0, child=window->childList ; i<window->numChildren ; child++, i++)
		GUI_r_List_f (child, depth+1);
}
static void GUI_List_f ()
{
	gui_t	*gui;
	uint32	i;

	// List all GUIs and their child windows
	for (i=0, gui=cl_guiList ; i<cl_numGUI ; gui++, i++) {
		Com_Printf (0, "#%2i GUI %s (%i children)\n", i+1, gui->name, gui->numChildren);
		GUI_r_List_f (gui, 0);
	}
}


/*
==================
GUI_NamedEvent_f
==================
*/
static void GUI_NamedEvent_f ()
{
	if (Cmd_Argc () < 2) {
		Com_Printf (0, "Usage: gui_namedEvent <event name>\n");
		return;
	}

	GUI_NamedGlobalEvent (Cmd_Argv (1));
}


/*
==================
GUI_Restart_f
==================
*/
static void GUI_Restart_f ()
{
	gui_thutdown ();
	GUI_Init ();
}


/*
==================
GUI_Test_f
==================
*/
static void GUI_Test_f ()
{
	gui_t	*gui;

	if (Cmd_Argc () < 2) {
		Com_Printf (0, "Usage: gui_test <GUI name>\n");
		return;
	}

	// Find it and touch it
	gui = GUI_RegisterGUI (Cmd_Argv (1));
	if (!gui) {
		Com_Printf (0, "Not found...\n");
		return;
	}

	// Open it
	GUI_OpenGUI (gui);
}


/*
==================
GUI_Init
==================
*/
void GUI_Init ()
{
	char			fixedName[MAX_QPATH];
	uint32			i;
	guiPathType_t	pathType;
	char			*name;

	uint32 startCycles = Sys_Cycles();
	Com_Printf (0, "\n---------- GUI Initialization ----------\n");

	// Clear lists
	cl_numGUI = 0;
	memset (&cl_guiList[0], 0, sizeof(gui_t) * MAX_GUIS);
	memset (&cl_guiSharedInfo[0], 0, sizeof(guiShared_t) * MAX_GUIS);

	// Console commands
	cmd_gui_list		= Cmd_AddCommand("gui_list",		0, GUI_List_f,			"List GUIs in memory");
	cmd_gui_namedEvent	= Cmd_AddCommand("gui_namedEvent",	0, GUI_NamedEvent_f,	"Triggers a named event on all open GUIs (for debugging)");
	cmd_gui_restart		= Cmd_AddCommand("gui_restart",		0, GUI_Restart_f,		"Restart the GUI system");
	cmd_gui_test		= Cmd_AddCommand("gui_test",		0, GUI_Test_f,			"Test the specified GUI");

	// Cvars
	gui_developer			= Cvar_Register ("gui_developer",				"0",		0);
	gui_debugBounds			= Cvar_Register ("gui_debugBounds",				"0",		0);
	gui_debugScale			= Cvar_Register ("gui_debugScale",				"0",		0);
	gui_mouseFilter			= Cvar_Register ("gui_mouseFilter",				"1",		CVAR_ARCHIVE);
	gui_mouseSensitivity	= Cvar_Register ("gui_mouseSensitivity",		"2",		CVAR_ARCHIVE);

	// Load scripts
	cl_numGUIErrors = 0;
	cl_numGUIWarnings = 0;
	var guiList = FS_FindFiles ("guis", "*guis/*.gui", "gui", true, false);
	for (i=0 ; i<guiList.Count() ; i++) {
		// Fix the path
		Com_NormalizePath (fixedName, sizeof(fixedName), guiList[i].CString());

		// Skip the path
		name = strstr (fixedName, "/guis/");
		if (!name)
			continue;
		name++;	// Skip the initial '/'

		// Base dir GUI?
		if (guiList[i].Contains(BASE_MODDIRNAME "/"))
			pathType = GUIPT_BASEDIR;
		else
			pathType = GUIPT_MODDIR;

		// Parse it
		GUI_ParseScript (name, pathType);
	}

	// Free auxiliary (scratch space) tags
	GUI_FreeTag (GUITAG_SCRATCH);

	// Var init
	GUIVar_Init ();

	Com_Printf (0, "----------------------------------------\n");

	// Check memory integrity
	Mem_CheckPoolIntegrity (cl_guiSysPool);

	Com_Printf (0, "GUI - %i error(s), %i warning(s)\n", cl_numGUIErrors, cl_numGUIWarnings);
	Com_Printf (0, "%u GUIs loaded in %6.2fms\n", cl_numGUI, (Sys_Cycles()-startCycles) * Sys_MSPerCycle());
	Com_Printf (0, "----------------------------------------\n");
}


/*
==================
gui_thutdown

Only called by gui_restart (for development purposes generally).
==================
*/
void gui_thutdown ()
{
	// Close all open GUIs
	GUI_CloseAllGUIs ();

	// Shutdown GUI vars
	GUIVar_Shutdown ();

	// Remove commands
	Cmd_RemoveCommand(cmd_gui_list);
	Cmd_RemoveCommand(cmd_gui_namedEvent);
	Cmd_RemoveCommand(cmd_gui_restart);
	Cmd_RemoveCommand(cmd_gui_test);

	// Release all used memory
	Mem_FreePool (cl_guiSysPool);
}
