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
// cvar.c
//

#include "common.h"

#define MAX_CVARS 1024
#define MAX_CVAR_HASH		(MAX_CVARS/4)

struct cVarInternal_t : public cVar_t
{
	char					*defaultString;
	struct cVarInternal_t	*hashNext;
};

static cVarInternal_t	com_cvarList[MAX_CVARS];
static int				com_numCvars;
static cVarInternal_t	*com_cvarHashTree[MAX_CVAR_HASH];

bool					com_userInfoModified;

/*
===============================================================================

	MISCELLANEOUS

===============================================================================
*/

/*
============
Cvar_BitInfo

Returns an info string containing all the 'bit' cvars
============
*/
char *Cvar_BitInfo(const int bit)
{
	static char	info[MAX_INFO_STRING];

	memset (&info, 0, sizeof(info));
	for (int i=0 ; i<com_numCvars ; i++)
	{
		cVar_t *cvar = &com_cvarList[i];
		if (cvar->flags & bit)
		{
			Info_SetValueForKey (info, cvar->name, cvar->string);
		}
	}

	return info;
}


/*
============
Cvar_InfoValidate
============
*/
static bool Cvar_InfoValidate(const char *s)
{
	if (strchr (s, '\\') || strchr (s, '\"') || strchr (s, ';'))
		return false;

	return true;
}


/*
============
Cvar_WriteVariables

Appends "set variable value" for all archive flagged variables
============
*/
void Cvar_WriteVariables(FILE *f)
{
	char buffer[1024];
	char *value;

	for (int i=0 ; i<com_numCvars ; i++)
	{
		cVar_t *cvar = &com_cvarList[i];
		if (cvar->flags & CVAR_ARCHIVE)
		{
			if ((cvar->flags & (CVAR_LATCH_SERVER|CVAR_LATCH_VIDEO|CVAR_LATCH_AUDIO)) && cvar->latchedString)
				value = cvar->latchedString;
			else
				value = cvar->string;

			Q_snprintfz(buffer, sizeof(buffer), "seta %s \"%s\"\n", cvar->name, cvar->string);

			fprintf(f, "%s", buffer);
		}
	}
}

/*
============
Cvar_WriteServerSaveVars

Used by the server when writing a save file to store off all relevant cVars.
============
*/
void Cvar_WriteServerSaveVars(const fileHandle_t fileNum)
{
	char name[MAX_OSPATH], string[128];

	// Write all CVAR_LATCH_SERVER cvars
	for (int i=0 ; i<com_numCvars ; i++)
	{
		cVar_t *cvar = &com_cvarList[i];
		if (!(cvar->flags & CVAR_LATCH_SERVER))
			continue;

		if (strlen(cvar->name) >= sizeof(name)-1 || strlen(cvar->string) >= sizeof(string)-1)
		{
			Com_Printf(PRNT_WARNING, "Cvar too long: %s = %s\n", cvar->name, cvar->string);
			continue;
		}

		// Write to a temporary buffer to make sure they're terminated and do not exceed the maximum size
		Q_strncpyz(name, cvar->name, sizeof(name));
		Q_strncpyz(string, cvar->string, sizeof(string));

		FS_Write(name, sizeof(name), fileNum);
		FS_Write(string, sizeof(string), fileNum);
	}
}

/*
===============================================================================

	CVAR RETRIEVAL

===============================================================================
*/

/*
============
Cvar_Exists
============
*/
cVar_t *Cvar_Exists(const char *varName)
{
	if (!varName || !varName[0])
		return NULL;

	uint32 hash = Com_HashGeneric(varName, MAX_CVAR_HASH);
	for (cVarInternal_t *cvar=com_cvarHashTree[hash] ; cvar ; cvar=cvar->hashNext)
	{
		if (!Q_stricmp (varName, cvar->name))
			return cvar;
	}

	return NULL;
}


/*
============
Cvar_CallBack
============
*/
void Cvar_CallBack(void (*callBack)(const char *name))
{
	for (int i=0 ; i<com_numCvars ; i++)
	{
		cVar_t *cvar = &com_cvarList[i];
		callBack(cvar->name);
	}
}


/*
============
Cvar_Register

If the variable already exists, the value will not be set
The flags will be or'ed in if the variable exists.
============
*/
cVar_t *Cvar_Register(const char *varName, const char *defaultValue, const int flags)
{
	if (!varName)
		return NULL;

	// Overwrite commands
	if (Cmd_Exists(varName))
	{
		Com_Printf(PRNT_WARNING, "Cvar_Register: command with same name '%s' already exists\n", varName);
		return NULL;
	}

	// Overwrite aliases
	if (Alias_Exists(varName))
	{
		Com_Printf(PRNT_WARNING, "Cvar_Register: overwriting alias '%s' with a cvar\n", varName);
		Alias_RemoveAlias(varName);
	}

	// Verify user/serverinfo cvar name
	if (flags & (CVAR_USERINFO|CVAR_SERVERINFO))
	{
		if (!Cvar_InfoValidate(varName))
		{
			Com_Printf (PRNT_WARNING, "Cvar_Register: '%s' invalid info cvar name\n", varName);
			return NULL;
		}
	}

	// If it already exists, replace the value and return it
	cVarInternal_t *cvar = (cVarInternal_t*)Cvar_Exists(varName);
	if (cvar)
	{
		Mem_Free (cvar->defaultString);
		cvar->defaultString = Mem_PoolStrDup (defaultValue, com_cvarSysPool, 0);
		cvar->flags |= flags;

		return cvar;
	}

	if (!defaultValue)
		return NULL;

	// Verify user/serverinfo cvar value
	if (flags & (CVAR_USERINFO|CVAR_SERVERINFO))
	{
		if (!Cvar_InfoValidate (defaultValue))
		{
			Com_Printf (PRNT_WARNING, "Cvar_Register: '%s' invalid info cvar value\n", varName);
			return NULL;
		}
	}

	// Allocate
	if (com_numCvars >= MAX_CVARS)
		Com_Error (ERR_FATAL, "Cvar_Register: MAX_CVARS");
	cvar = &com_cvarList[com_numCvars++];
	uint32 hashValue = Com_HashGeneric (varName, MAX_CVAR_HASH);

	// Fill it in
	cvar->name = Mem_PoolStrDup (varName, com_cvarSysPool, 0);
	cvar->string = Mem_PoolStrDup (defaultValue, com_cvarSysPool, 0);
	cvar->defaultString = Mem_PoolStrDup (defaultValue, com_cvarSysPool, 0);
	cvar->floatVal = atof (cvar->string);
	cvar->intVal = atoi (cvar->string);

	cvar->flags = flags;
	cvar->modified = true;

	// Link it into the hash list
	cvar->hashNext = com_cvarHashTree[hashValue];
	com_cvarHashTree[hashValue] = cvar;

	return cvar;
}


/*
============
Cvar_FixCheatVars
============
*/
void Cvar_FixCheatVars()
{
	if (!(Com_ServerState () && !Cvar_GetIntegerValue ("cheats"))
	&& !(Com_ClientState () >= CA_CONNECTED && !Com_ServerState ()))
		return;

	for (int i=0 ; i<com_numCvars ; i++)
	{
		cVarInternal_t *cvar = &com_cvarList[i];
		if (!(cvar->flags & CVAR_CHEAT))
			continue;

		Mem_Free (cvar->string);
		cvar->string = Mem_PoolStrDup (cvar->defaultString, com_cvarSysPool, 0);
		cvar->floatVal = atof (cvar->string);
		cvar->intVal = atoi (cvar->string);
	}
}


/*
============
Cvar_GetLatchedVars

Any variables with latched values will now be updated
============
*/
void Cvar_GetLatchedVars(const int flags)
{
	for (int i=0 ; i<com_numCvars ; i++)
	{
		cVarInternal_t *cvar = &com_cvarList[i];
		if (!(cvar->flags & flags))
			continue;
		if (!cvar->latchedString)
			continue;

		Mem_Free (cvar->string);
		cvar->string = cvar->latchedString;
		cvar->latchedString = NULL;
		cvar->floatVal = atof (cvar->string);
		cvar->intVal = atoi (cvar->string);

		if (cvar->flags & CVAR_RESET_GAMEDIR)
			FS_SetGamedir (cvar->string, false);
	}
}


/*
============
Cvar_GetIntegerValue
============
*/
int Cvar_GetIntegerValue(const char *varName)
{	
	cVar_t *cvar = Cvar_Exists(varName);
	if (!cvar)
		return 0;

	return cvar->intVal;
}


/*
============
Cvar_GetStringValue
============
*/
char *Cvar_GetStringValue(const char *varName)
{	
	cVar_t *cvar = Cvar_Exists(varName);
	if (!cvar)
		return "";

	return cvar->string;
}


/*
============
Cvar_GetFloatValue
============
*/
float Cvar_GetFloatValue(const char *varName)
{	
	cVar_t *cvar = Cvar_Exists(varName);
	if (!cvar)
		return 0;

	return cvar->floatVal;
}

/*
===============================================================================

	CVAR SETTING

===============================================================================
*/

/*
============
Cvar_SetVariableValue
============
*/
static cVar_t *Cvar_SetVariableValue(cVar_t *cvar, const char *value, const int flags, const bool bForce)
{
	if (!cvar)
		return NULL;

	cvar->flags |= flags;

	// Validate the userinfo string
	if (cvar->flags & (CVAR_USERINFO|CVAR_SERVERINFO))
	{
		if (!Cvar_InfoValidate(value))
		{
			Com_Printf(PRNT_WARNING, "Cvar_SetVariableValue: '%s' invalid info cvar value\n", cvar->name);
			return cvar;
		}
	}

	if (bForce)
	{
		if (cvar->latchedString)
		{
			Mem_Free(cvar->latchedString);
			cvar->latchedString = NULL;
		}
	}
	else
	{
		// Don't touch if read only
		if (cvar->flags & CVAR_READONLY)
		{
			Com_Printf(0, "Cvar_SetVariableValue: '%s' is write protected.\n", cvar->name);
			return cvar;
		}

		// Check cheat vars
		if (cvar->flags & CVAR_CHEAT)
		{
			if ((Com_ServerState() && !Cvar_GetIntegerValue("cheats"))
			|| (Com_ClientState() >= CA_CONNECTING && !Com_ServerState()))
			{
				// Some modifications use this to check a variable value
				// We still want them to get the info, just don't change the variable
				if (cvar->flags & CVAR_USERINFO)
					com_userInfoModified = true;	// Transmit at next opportunity

				if (!strcmp(value, cvar->string))
					return cvar;
				Com_Printf(0, "Cvar_SetVariableValue: '%s' is cheat protected.\n", cvar->name);
				return cvar;
			}
		}

		if (cvar->flags & (CVAR_LATCH_SERVER|CVAR_LATCH_VIDEO|CVAR_LATCH_AUDIO))
		{
			// Not changed
			if (cvar->latchedString)
			{
				if (!strcmp(value, cvar->latchedString))
					return cvar;
				Mem_Free(cvar->latchedString);
			}
			else if (!strcmp(value, cvar->string))
				return cvar;

			// Change the latched value
			if (cvar->flags & CVAR_LATCH_SERVER)
			{
				if (Com_ServerState())
				{
					Com_Printf(0, "Cvar_SetVariableValue: '%s' will be changed to '%s' for next game.\n", cvar->name, value);
					cvar->latchedString = Mem_PoolStrDup(value, com_cvarSysPool, 0);
					return cvar;
				}
			}
			else if (cvar->flags & CVAR_LATCH_VIDEO)
			{
				Com_Printf(0, "Cvar_SetVariableValue: '%s' will be changed to '%s' after a vid_restart.\n", cvar->name, value);
				cvar->latchedString = Mem_PoolStrDup(value, com_cvarSysPool, 0);
				return cvar;
			}
			else if (cvar->flags & CVAR_LATCH_AUDIO)
			{
				Com_Printf(0, "Cvar_SetVariableValue: '%s' will be changed to '%s' after a snd_restart.\n", cvar->name, value);
				cvar->latchedString = Mem_PoolStrDup(value, com_cvarSysPool, 0);
				return cvar;
			}
		}
	}

	// Not changed
	if (!strcmp(value, cvar->string))
		return cvar;

	// Free the old value string
	Mem_Free(cvar->string);

	// Fill in the new value
	cvar->string = Mem_PoolStrDup(value, com_cvarSysPool, 0);
	cvar->floatVal = atof(cvar->string);
	cvar->intVal = atoi(cvar->string);

	// It has changed
	cvar->modified = true;
	if (cvar->flags & CVAR_USERINFO)
		com_userInfoModified = true;	// Transmit at next opportunity

	// Update the game directory
	if (cvar->flags & CVAR_RESET_GAMEDIR)
		FS_SetGamedir(cvar->string, false);

	return cvar;
}


/*
============
Cvar_FindAndSet
============
*/
static cVar_t *Cvar_FindAndSet(const char *varName, const char *newValue, const int flags, const bool bForce)
{
	cVar_t *cvar = Cvar_Exists(varName);
	if (!cvar)
	{
		// Create it
		return Cvar_Register(varName, newValue, flags);
	}

	return Cvar_SetVariableValue(cvar, newValue, flags, bForce);
}

// ============================================================================

/*
============
Cvar_VariableSet
============
*/
cVar_t *Cvar_VariableSet (cVar_t *cvar, const char *newValue, const bool bForce)
{
	if (!cvar)
		return NULL;

	return Cvar_SetVariableValue (cvar, newValue, 0, bForce);
}


/*
============
Cvar_VariableSetValue
============
*/
cVar_t *Cvar_VariableSetValue(cVar_t *cvar, const float newValue, const bool bForce)
{
	if (!cvar)
		return NULL;

	if (newValue == (int)newValue)
		return Cvar_SetVariableValue(cvar, Q_VarArgs("%i", (int)newValue), 0, bForce);
	else
		return Cvar_SetVariableValue(cvar, Q_VarArgs("%f", newValue), 0, bForce);
}


/*
============
Cvar_VariableReset
============
*/
cVar_t *Cvar_VariableReset(cVar_t *cvar, const bool bForce)
{
	if (!cvar)
		return NULL;

	return Cvar_SetVariableValue(cvar, ((cVarInternal_t*)cvar)->defaultString, 0, bForce);
}

// ============================================================================

/*
============
Cvar_Set
============
*/
cVar_t *Cvar_Set(const char *varName, const char *newValue, const bool bForce)
{
	return Cvar_FindAndSet(varName, newValue, 0, bForce);
}


/*
============
Cvar_SetValue
============
*/
cVar_t *Cvar_SetValue(const char *varName, const float newValue, const bool bForce)
{
	if (newValue == (int)newValue)
		return Cvar_FindAndSet(varName, Q_VarArgs("%i", (int)newValue), 0, bForce);
	else
		return Cvar_FindAndSet(varName, Q_VarArgs("%f", newValue), 0, bForce);
}


/*
============
Cvar_Reset
============
*/
cVar_t *Cvar_Reset(const char *varName, const bool bForce)
{
	// Make sure it exists
	cVarInternal_t *cvar = (cVarInternal_t*)Cvar_Exists(varName);
	if (!cvar)
	{
		Com_Printf (0, "Cvar_Reset: '%s' is not a registered cvar\n", varName);
		return NULL;
	}

	// Reset
	return Cvar_SetVariableValue(cvar, cvar->defaultString, 0, bForce);
}

// ============================================================================

/*
============
Cvar_Command

Handles variable inspection and changing from the console
============
*/
bool Cvar_Command()
{
	// Check variables
	cVarInternal_t *cvar = (cVarInternal_t*)Cvar_Exists (Cmd_Argv (0));
	if (!cvar)
		return false;

	// Perform a variable print or set
	if (Cmd_Argc () == 1)
	{
		Com_Printf (0, "\"%s\" is: \"%s" S_STYLE_RETURN "\"", cvar->name, cvar->string);
		if (!(cvar->flags & CVAR_READONLY))
			Com_Printf (0, " default: \"%s" S_STYLE_RETURN "\"", cvar->defaultString);
		Com_Printf (0, "\n");
		return true;
	}

	Cvar_SetVariableValue (cvar, Cmd_Argv (1), 0, false);
	return true;
}

/*
===============================================================================

	CONSOLE FUNCTIONS

===============================================================================
*/

/*
============
Cvar_Set_f

Allows setting and defining of arbitrary cvars from console
============
*/
static void Cvar_Set_f()
{
	int		c;
	int		flags;

	c = Cmd_Argc ();
	if (c != 3 && c != 4)
	{
		Com_Printf (0, "usage: set <variable> <value> [u / s]\n");
		return;
	}

	if (c == 4)
	{
		if (!Q_stricmp (Cmd_Argv (3), "u"))
			flags = CVAR_USERINFO;
		else if (!Q_stricmp (Cmd_Argv (3), "s"))
			flags = CVAR_SERVERINFO;
		else
		{
			Com_Printf (0, "flags can only be 'u' or 's'\n");
			return;
		}
	}
	else
	{
		flags = 0;
	}

	Cvar_FindAndSet (Cmd_Argv (1), Cmd_Argv (2), flags, false);
}

static void Cvar_SetA_f()
{
	int		c;
	int		flags;

	c = Cmd_Argc ();
	if (c != 3 && c != 4)
	{
		Com_Printf (0, "usage: set <variable> <value> [u / s]\n");
		return;
	}

	if (c == 4)
	{
		if (!Q_stricmp (Cmd_Argv (3), "u"))
		{
			flags = CVAR_USERINFO;
		}
		else if (!Q_stricmp (Cmd_Argv (3), "s"))
		{
			flags = CVAR_SERVERINFO;
		}
		else
		{
			Com_Printf (0, "flags can only be 'u' or 's'\n");
			return;
		}
	}
	else
		flags = 0;

	Cvar_FindAndSet (Cmd_Argv (1), Cmd_Argv (2), CVAR_ARCHIVE|flags, false);
}

static void Cvar_SetS_f()
{
	if (Cmd_Argc () != 3)
	{
		Com_Printf (0, "usage: sets <variable> <value>\n");
		return;
	}

	Cvar_FindAndSet (Cmd_Argv (1), Cmd_Argv (2), CVAR_SERVERINFO, false);
}

static void Cvar_SetU_f()
{
	if (Cmd_Argc () != 3)
	{
		Com_Printf (0, "usage: setu <variable> <value>\n");
		return;
	}

	Cvar_FindAndSet (Cmd_Argv (1), Cmd_Argv (2), CVAR_USERINFO, false);
}


/*
================
Cvar_Reset_f
================
*/
static void Cvar_Reset_f()
{
	cVar_t	*cvar;

	// Check args
	if (Cmd_Argc () != 2)
	{
		Com_Printf (0, "usage: reset <variable>\n");
		return;
	}

	// See if it exists
	cvar = Cvar_Exists (Cmd_Argv (1));
	if (!cvar)
	{
		Com_Printf (0, "'%s' is not a registered cvar\n", Cmd_Argv (1));
		return;
	}

	Cvar_VariableReset (cvar, false);
}


/*
================
Cvar_Toggle_f
================
*/
static void Cvar_Toggle_f()
{
	cVar_t	*cvar;
	char	*opt1 = "1", *opt2 = "0";
	int		c;

	// Check args
    c = Cmd_Argc ();
	if (c < 2)
	{
		Com_Printf (0, "usage: toggle <cvar> [option1] [option2]\n");
		return;
	}
	if (c > 2)
		opt1 = Cmd_Argv (2);
	if (c > 3)
		opt2 = Cmd_Argv (3);

	// See if it exists
	cvar = Cvar_Exists (Cmd_Argv (1));
	if (!cvar)
	{
		Com_Printf (0, "'%s' is not a registered cvar\n", Cmd_Argv (1));
		return;
	}

	if (Q_stricmp (cvar->string, opt1))
		Cvar_VariableSet (cvar, opt1, false);
	else
		Cvar_VariableSet (cvar, opt2, false);  
}


/*
================
Cvar_IncVar_f
================
*/
static void Cvar_IncVar_f()
{
	cVar_t	*cvar;
	float	inc;
	int		c;

	// Check args
	c = Cmd_Argc ();
	if (c < 2)
	{
		Com_Printf (0, "usage: inc <cvar> [amount]\n");
		return;
	}
	if (c > 2)
		inc = atof (Cmd_Argv (2));
	else
		inc = 1;

	// See if it exists
	cvar = Cvar_Exists (Cmd_Argv (1));
	if (!cvar)
	{
		Com_Printf (0, "'%s' is not a registered cvar\n", Cmd_Argv (1));
		return;
	}

	Cvar_VariableSetValue (cvar, cvar->floatVal + inc, false);  
}


/*
================
Cvar_DecVar_f
================
*/
static void Cvar_DecVar_f()
{
	cVar_t	*cvar;
	float	dec;
	int		c;

	// Check args
	c = Cmd_Argc ();
	if (c < 2)
	{
		Com_Printf (0, "usage: dec <cvar> [amount]\n");
		return;
	}
	if (c > 2)
		dec = atof (Cmd_Argv (2));
	else
		dec = 1;

	// See if it exists
	cvar = Cvar_Exists (Cmd_Argv (1));
	if (!cvar)
	{
		Com_Printf (0, "'%s' is not a registered cvar\n", Cmd_Argv (1));
		return;
	}

	Cvar_VariableSetValue (cvar, cvar->floatVal - dec, false);  
}


/*
============
Cvar_List_f
============
*/
static int alphaSortCmp (const void *_a, const void *_b)
{
	const cVar_t *a = (const cVar_t *) _a;
	const cVar_t *b = (const cVar_t *) _b;

	return Q_stricmp (a->name, b->name);
}
static void Cvar_List_f()
{
	cVarInternal_t	*cvar, *sortedList;
	int		total, matching, i;
	char	*wildCard;
	int		c;

	c = Cmd_Argc ();
	if (c != 1 && c != 2)
	{
		Com_Printf (0, "usage: cvarlist [wildcard]\n");
		return;
	}

	if (c == 2)
		wildCard = Cmd_Argv (1);
	else
		wildCard = "*";

	// create a list
	for (matching=0, total=0, i=0, cvar=com_cvarList ; i<com_numCvars ; cvar++, total++, i++)
	{
		if (!Q_WildcardMatch (wildCard, cvar->name, 1))
			continue;

		matching++;
	}

	if (!matching)
	{
		Com_Printf (0, "%i cvars total, %i matching\n", total, matching);
		return;
	}

	sortedList = (cVarInternal_t*)Mem_PoolAlloc (matching * sizeof(cVarInternal_t), com_cvarSysPool, 0);
	for (matching=0, i=0, cvar=com_cvarList ; i<com_numCvars ; cvar++, i++)
	{
		if (!Q_WildcardMatch (wildCard, cvar->name, 1))
			continue;

		sortedList[matching++] = *cvar;
	}

	// sort it
	qsort (sortedList, matching, sizeof(sortedList[0]), alphaSortCmp);

	// print it
	for (i=0 ; i<matching ;  i++)
	{
		cvar = &sortedList[i];

		Com_Printf (0, "%s", (cvar->flags & CVAR_ARCHIVE) ?		"*" : " ");
		Com_Printf (0, "%s", (cvar->flags & CVAR_USERINFO) ?		"U" : " ");
		Com_Printf (0, "%s", (cvar->flags & CVAR_SERVERINFO) ?	"S" : " ");
		Com_Printf (0, "%s", (cvar->flags & CVAR_READONLY) ?		"-" : " ");
		Com_Printf (0, "%s", (cvar->flags & CVAR_LATCH_SERVER) ?	"LS" : "  ");
		Com_Printf (0, "%s", (cvar->flags & CVAR_LATCH_VIDEO) ?	"LV" : "  ");
		Com_Printf (0, "%s", (cvar->flags & CVAR_LATCH_AUDIO) ?	"LA" : "  ");

		Com_Printf (0, " %s is: \"%s" S_STYLE_RETURN "\"", cvar->name, cvar->string);
		if (!(cvar->flags & CVAR_READONLY))
			Com_Printf (0, " default: \"%s" S_STYLE_RETURN "\"", cvar->defaultString);
		Com_Printf (0, "\n");
	}

	if (matching)
		Mem_Free (sortedList);
	Com_Printf (0, "%i cvars total, %i matching\n", total, matching);
}

/*
===============================================================================

	INITIALIZATION

===============================================================================
*/

/*
============
Cvar_Init
============
*/
void Cvar_Init()
{
	com_numCvars = 0;
	memset (com_cvarHashTree, 0, sizeof(com_cvarHashTree));

	Cmd_AddCommand ("set",		0, Cvar_Set_f,			"Sets a cvar with a value");
	Cmd_AddCommand ("seta",		0, Cvar_SetA_f,			"Sets a cvar with a value and adds to be archived");
	Cmd_AddCommand ("sets",		0, Cvar_SetS_f,			"Sets a cvar with a value and makes it serverinfo");
	Cmd_AddCommand ("setu",		0, Cvar_SetU_f,			"Sets a cvar with a value and makes it userinfo");
	Cmd_AddCommand ("reset",	0, Cvar_Reset_f,		"Resets a cvar to it's default value");

	Cmd_AddCommand ("toggle",	0, Cvar_Toggle_f,		"Toggles a cvar between two values");
	Cmd_AddCommand ("inc",		0, Cvar_IncVar_f,		"Increments a cvar's value by one");
	Cmd_AddCommand ("dec",		0, Cvar_DecVar_f,		"Decrements a cvar's value by one");

	Cmd_AddCommand ("cvarlist",	0, Cvar_List_f,			"Prints out a list of the current cvars");
}
