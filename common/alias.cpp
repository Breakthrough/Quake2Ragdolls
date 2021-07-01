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
// alias.c
//

#include "common.h"

#define MAX_ALIAS_HASH	32

aliasCmd_t	*com_aliasList;
aliasCmd_t	*com_aliasHashTree[MAX_ALIAS_HASH];

int			com_aliasCount;

/*
=============================================================================

	ALIASES

=============================================================================
*/

/*
============
Alias_Exists
============
*/
aliasCmd_t *Alias_Exists(const char *aliasName)
{
	aliasCmd_t	*alias;
	uint32		hash;

	if (!aliasName)
		return NULL;

	hash = Com_HashGeneric (aliasName, MAX_ALIAS_HASH);
	for (alias=com_aliasHashTree[hash] ; alias ; alias=alias->hashNext) {
		if (!Q_stricmp (aliasName, alias->name))
			return alias;
	}

	return NULL;
}


/*
============
Alias_CallBack
============
*/
void Alias_CallBack (void (*callBack) (const char *name))
{
	aliasCmd_t	*alias;

	for (alias=com_aliasList ; alias ; alias=alias->next)
		callBack (alias->name);
}


/*
============
Alias_AddAlias
============
*/
static void Alias_AddAlias (const char *aliasName)
{
	aliasCmd_t	*alias;
	char		cmd[2048];
	int			i, c;

	// Check if it's already a command
	if (Cmd_Exists (aliasName)) {
		Com_Printf (PRNT_WARNING, "Alias_AddAlias: \"%s\" already defined as a command\n", aliasName);
		return;
	}

	// Check if it's already a cvar
	if (Cvar_Exists (aliasName)) {
		Com_Printf (PRNT_WARNING, "Alias_AddAlias: \"%s\" already defined as a cvar\n", aliasName);
		return;
	}

	// Check name length
	if (strlen(aliasName)+1 >= MAX_ALIAS_NAME) {
		Com_Printf (PRNT_WARNING, "Alias_AddAlias: name \"%s\" too long\n", aliasName);
		return;
	}

	// See if it's actually getting a value
	if (Cmd_Argc () < 3) {
		Com_Printf (PRNT_WARNING, "Alias_AddAlias: no commands for \"%s\" alias\n", aliasName);
		return;
	}

	// If the alias already exists, reuse it
	alias = Alias_Exists (aliasName);
	if (alias) {
		Mem_Free (alias->value);
	}
	else {
		alias = (aliasCmd_t*)Mem_PoolAlloc (sizeof(aliasCmd_t), com_aliasSysPool, 0);
		alias->hashValue = Com_HashGeneric (aliasName, MAX_ALIAS_HASH);

		// Link it in
		alias->next = com_aliasList;
		com_aliasList = alias;

		// Link it into the hash tree
		alias->hashNext = com_aliasHashTree[alias->hashValue];
		com_aliasHashTree[alias->hashValue] = alias;
	}
	Q_strncpyz (alias->name, aliasName, sizeof(alias->name));

	// Copy the rest of the command line
	cmd[0] = '\0';		// Start out with a null string
	c = Cmd_Argc ();
	for (i=2 ; i<c ; i++) {
		Q_strcatz (cmd, Cmd_Argv (i), sizeof(cmd));
		if (i != c-1)
			Q_strcatz (cmd, " ", sizeof(cmd));
	}
	Q_strcatz (cmd, "\n", sizeof(cmd));

	alias->value = Mem_PoolStrDup (cmd, com_aliasSysPool, 0);
}


/*
============
Alias_RemoveAlias
============
*/
void Alias_RemoveAlias(const char *aliasName)
{
	aliasCmd_t	*alias, **prev;

	if (!aliasName || !aliasName[0]) {
		Com_Printf (PRNT_WARNING, "Alias_RemoveAlias: NULL aliasName\n");
		return;
	}

	// de-link it from alias list
	prev = &com_aliasList;
	for ( ; ; ) {
		alias = *prev;
		if (!alias) {
			Com_Printf (PRNT_WARNING, "Alias_RemoveAlias: %s not added\n", aliasName);
			return;
		}

		if (!Q_stricmp (aliasName, alias->name)) {
			*prev = alias->next;
			break;
		}
		prev = &alias->next;
	}

	// de-link it from hash list
	prev = &com_aliasHashTree[alias->hashValue];
	for ( ; ; ) {
		alias = *prev;
		if (!alias) {
			Com_Printf (PRNT_WARNING, "Alias_RemoveAlias: %s not added to hash list\n", aliasName);
			return;
		}

		if (!Q_stricmp (aliasName, alias->name)) {
			*prev = alias->hashNext;
			if (alias->value)
				Mem_Free (alias->value);
			Mem_Free (alias);
			return;
		}
		prev = &alias->hashNext;
	}
}

/*
==============================================================================

	CONSOLE COMMANDS

==============================================================================
*/

/*
===============
Cmd_Alias_f

Creates a new command that executes a command string (possibly ; seperated)
===============
*/
static void Cmd_Alias_f ()
{
	if (Cmd_Argc () < 2) {
		Com_Printf (0, "syntax: alias <name> <value>; 'aliaslist' to see a list\n");
		return;
	}

	Alias_AddAlias (Cmd_Argv (1));
}


/*
===============
Cmd_Unalias_f

Kills an alias
===============
*/
static void Cmd_Unalias_f ()
{
	if (Cmd_Argc () != 2) {
		Com_Printf (0, "syntax: unalias <name>; 'aliaslist' to see a list\n");
		return;
	}

	Alias_RemoveAlias (Cmd_Argv (1));
}


/*
============
Cmd_AliasList_f
============
*/
static int alphaSortCmp (const void *_a, const void *_b) {
	const aliasCmd_t *a = (const aliasCmd_t *) _a;
	const aliasCmd_t *b = (const aliasCmd_t *) _b;

	return Q_stricmp (a->name, b->name);
}
static void Cmd_AliasList_f ()
{
	aliasCmd_t	*alias, *sortedList;
	int			i, j;
	int			matching;
	int			longest;
	int			total;
	char		*wildCard;

	if ((Cmd_Argc () != 1) && (Cmd_Argc () != 2)) {
		Com_Printf (0, "usage: aliaslist [wildcard]\n");
		return;
	}

	if (Cmd_Argc () == 2)
		wildCard = Cmd_Argv (1);
	else
		wildCard = "*";

	// create a list
	for (matching=0, total=0, alias=com_aliasList ; alias ; alias=alias->next, total++) {
		if (!Q_WildcardMatch (wildCard, alias->name, 1))
			continue;
		matching++;
	}

	if (!matching) {
		Com_Printf (0, "%i aliases total, %i matching\n", total, matching);
		return;
	}

	sortedList = (aliasCmd_t*)Mem_PoolAlloc (matching * sizeof(aliasCmd_t), com_aliasSysPool, 0);
	for (matching=0, longest=0, alias=com_aliasList ; alias ; alias=alias->next) {
		if (!Q_WildcardMatch (wildCard, alias->name, 1))
			continue;

		if ((int)strlen (alias->name) > longest)
			longest = strlen (alias->name);

		sortedList[matching++] = *alias;
	}

	// sort it
	qsort (sortedList, matching, sizeof(sortedList[0]), alphaSortCmp);

	// print it
	longest++;
	for (i=0 ; i<matching ;  i++) {
		alias = &sortedList[i];

		Com_Printf (0, alias->name);
		for (j=0 ; j<longest-(int)strlen(alias->name) ; j++)
			Com_Printf (0, " ");
		Com_Printf (0, "\"%s\"\n", alias->value);
	}

	if (matching)
		Mem_Free (sortedList);
	Com_Printf (0, "%i aliases total, %i matching\n", total, matching);
}

/*
=============================================================================

	INITIALIZATION

=============================================================================
*/

/*
============
Alias_Init
============
*/
void Alias_Init ()
{
	memset (com_aliasHashTree, 0, sizeof(com_aliasHashTree));

	Cmd_AddCommand ("alias",		0, Cmd_Alias_f,			"Adds an alias command");
	Cmd_AddCommand ("unalias",		0, Cmd_Unalias_f,		"Removes an alias command");
	Cmd_AddCommand ("aliaslist",	0, Cmd_AliasList_f,		"Prints out a list of alias commands");
}
