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
// cmd.c
//

#include "common.h"

#define MAX_CMDFUNCS	1024
#define MAX_CMD_HASH	(MAX_CMDFUNCS/4)

class conCmdInternal_t : public conCmd_t
{
public:
	bool					bInUse;				// Spot is in use

	char					*name;				// Command name
	int						flags;				// Command flags
	void					(*function)();	// Function to execute
	const char				*description;		// Description of the command

	uint32					hashValue;
	class conCmdInternal_t	*hashNext;
};

static conCmdInternal_t	com_cmdFuncList[MAX_CMDFUNCS];
static conCmdInternal_t	*com_cmdHashTree[MAX_CMD_HASH];
static uint32			com_numCmdFuncs;

bool				com_cmdWait;

static int			com_cmdArgc;
static char			*com_cmdArgv[MAX_STRING_TOKENS];
static char			com_cmdArgs[MAX_STRING_CHARS];

// ==========================================================================

/*
============
Cmd_Exists
============
*/
conCmd_t *Cmd_Exists(const char *name)
{
	if (!name)
		return NULL;

	uint32 hash = Com_HashGeneric (name, MAX_CMD_HASH);
	for (conCmdInternal_t *cmd=com_cmdHashTree[hash] ; cmd ; cmd=cmd->hashNext)
	{
		if (!cmd->bInUse)
			continue;
		if (!Q_stricmp (name, cmd->name))
			return cmd;
	}

	return NULL;
}


/*
============
Cmd_CallBack
============
*/
void Cmd_CallBack(void(*callBack)(const char *name))
{
	for (uint32 i=0 ; i<com_numCmdFuncs ; i++)
	{
		conCmdInternal_t *cmd = &com_cmdFuncList[i];
		if (!cmd->bInUse)
			continue;

		callBack(cmd->name);
	}
}

/*
=============================================================================

	COMMAND EXECUTION

=============================================================================
*/

/*
============
Cmd_Argc
============
*/
int Cmd_Argc ()
{
	return com_cmdArgc;
}


/*
============
Cmd_Argv
============
*/
char *Cmd_Argv (int arg)
{
	if (arg >= com_cmdArgc)
		return "";
	return com_cmdArgv[arg];	
}

/*
============
Cmd_ArgsOffset

Returns a single string containing argv(start) to argv(argc()-1)
============
*/
char *Cmd_ArgsOffset (int start)
{
	static char	args[2048];
	int			i;

	if (start == 1)
		return com_cmdArgs;
	if (start >= com_cmdArgc) {
		assert (0);
		return "";
	}

	for (i=start ; i<com_cmdArgc ; i++) {
		if (strlen(args) + (strlen(com_cmdArgv[i])+2) >= sizeof(args)) {
			Com_Printf (PRNT_WARNING, "Cmd_ArgsOffset: overflow\n");
			break;
		}

		Q_strcatz (args, com_cmdArgv[i], sizeof(args));
		if (i != com_cmdArgc-1)
			Q_strcatz (args, " ", sizeof(args));
	}

	return args;
}


/*
======================
Cmd_MacroExpandString
======================
*/
char *Cmd_MacroExpandString (char *text)
{
	static	char	expanded[MAX_STRING_CHARS];
	int		i, j, count, len;
	bool	inquote;
	char	*scan;
	char	temporary[MAX_STRING_CHARS];
	char	*token, *start;

	inquote = false;
	scan = text;

	len = strlen (scan);
	if (len >= MAX_STRING_CHARS) {
		Com_Printf (PRNT_WARNING, "Line exceeded %i chars, discarded.\n", MAX_STRING_CHARS);
		return NULL;
	}

	count = 0;
	for (i=0 ; i<len ; i++) {
		if (scan[i] == '"')
			inquote ^= 1;
		if (inquote)
			continue;	// Don't expand inside quotes
		if (scan[i] != '$')
			continue;

		// Scan out the complete macro
		start = scan+i+1;
		token = Com_Parse (&start);
		if (!start)
			continue;
	
		token = Cvar_GetStringValue (token);

		j = strlen (token);
		len += j;
		if (len >= MAX_STRING_CHARS) {
			Com_Printf (PRNT_WARNING, "Expanded line exceeded %i chars, discarded.\n", MAX_STRING_CHARS);
			return NULL;
		}

		strncpy (temporary, scan, i);
		strcpy (temporary+i, token);
		strcpy (temporary+i+j, start);

		strcpy (expanded, temporary);
		scan = expanded;
		i--;

		if (++count == 100) {
			Com_Printf (PRNT_WARNING, "Macro expansion loop, discarded.\n");
			return NULL;
		}
	}

	if (inquote) {
		Com_Printf (PRNT_WARNING, "Line has unmatched quote, discarded.\n");
		return NULL;
	}

	return scan;
}


/*
============
Cmd_TokenizeString

Parses the given string into command line tokens.
$Cvars will be expanded unless they are in a quoted token
============
*/
void Cmd_TokenizeString (char *text, bool macroExpand)
{
	char	*token;
	int		i;

	// Clear the args from the last string
	for (i=0 ; i<com_cmdArgc ; i++)
		Mem_Free (com_cmdArgv[i]);

	com_cmdArgc = 0;
	com_cmdArgs[0] = 0;
	
	// Macro expand the text
	if (macroExpand)
		text = Cmd_MacroExpandString (text);

	if (!text)
		return;

	for ( ; ; ) {
		// Skip whitespace up to an \n
		while (*text && *text <= ' ' && *text != '\n')
			text++;
		
		if (*text == '\n') {
			// A newline seperates commands in the buffer
			text++;
			break;
		}

		if (!*text)
			return;

		// Set com_cmdArgs to everything after the first arg
		if (com_cmdArgc == 1) {
			int		l;

			Q_strcatz (com_cmdArgs, text, sizeof(com_cmdArgs));

			// Strip off any trailing whitespace
			l = strlen (com_cmdArgs) - 1;
			for ( ; l>=0 ; l--)
				if (com_cmdArgs[l] <= ' ')
					com_cmdArgs[l] = 0;
				else
					break;
		}

		token = Com_Parse (&text);
		if (!text)
			return;

		if (com_cmdArgc < MAX_STRING_TOKENS)
			com_cmdArgv[com_cmdArgc++] = Mem_PoolStrDup (token, com_cmdSysPool, 0);
	}
}


/*
============
Cmd_ExecuteString

A complete command line has been parsed, so try to execute it
FIXME: lookuponadd the token to speed search?
============
*/
void Cmd_ExecuteString (char *text)
{	
	conCmdInternal_t	*cmd;
	aliasCmd_t			*alias;

	Cmd_TokenizeString (text, true);
			
	// Execute the command line
	if (!Cmd_Argc ())
		return;		// no tokens

	// Check functions
	cmd = (conCmdInternal_t*)Cmd_Exists(com_cmdArgv[0]);
	if (cmd) {
		if (!cmd->function) {
			// Forward to server command
			//Cmd_ExecuteString (Q_VarArgs ("cmd %s", text));
#ifndef DEDICATED_ONLY
			CL_ForwardCmdToServer ();
#endif
		}
		else {
			cmd->function ();
		}
		return;
	}

	// Check alias
	alias = Alias_Exists (com_cmdArgv[0]);
	if (alias) {
		if (++com_aliasCount == MAX_ALIAS_LOOPS) {
			Com_Printf (PRNT_WARNING, "Cmd_ExecuteString: MAX_ALIAS_LOOPS hit\n");
			return;
		}

		Cbuf_InsertText (Q_VarArgs ("%s\n", alias->value));
		return;
	}
	
	// Check cvars
	if (Cvar_Command ())
		return;

#ifndef DEDICATED_ONLY
	// Send it as a server command if we are connected
	if (!dedicated->intVal && CL_ForwardCmdToServer ())
		return;
#endif

	// Command unknown
	Com_Printf (0, "Unknown command \"%s" S_STYLE_RETURN "\"\n", Cmd_Argv (0));
}


/*
============
Cmd_AddCommand
============
*/
conCmd_t *Cmd_AddCommand(const char *name, const int flags, void (*function) (), const char *description)
{
	// Check if it's already a cmd
	if (Cmd_Exists(name))
	{
		Com_Printf(PRNT_WARNING, "Cmd_AddCommand: \"%s\" already defined as a command\n", name);
		return NULL;
	}
	
	// Check if it's already a cvar
	if (Cvar_Exists(name))
	{
		Com_Printf(PRNT_WARNING, "Cmd_AddCommand: \"%s\" already defined as a cvar\n", name);
		return NULL;
	}

	// Overwrite aliases
	if (Alias_Exists(name))
	{
		Com_Printf(PRNT_WARNING, "Cmd_AddCommand: overwriting alias \"%s\" with a command\n", name);
		Alias_RemoveAlias(name);
	}

	// Find a free spot
	uint32 i;
	conCmdInternal_t *cmd;

	for (i=0, cmd=com_cmdFuncList ; i<com_numCmdFuncs ; cmd++, i++)
	{
		if (!cmd->bInUse)
			break;
	}
	if (i == com_numCmdFuncs)
	{
		// Allocate a spot
		if (com_numCmdFuncs >= MAX_CMDFUNCS)
			Com_Error (ERR_FATAL, "Cmd_AddCommand: MAX_CMDFUNCS");
		cmd = &com_cmdFuncList[com_numCmdFuncs++];
	}

	// Fill it in
	cmd->hashValue = Com_HashGeneric(name, MAX_CMD_HASH);
	cmd->name = Mem_PoolStrDup(name, com_cmdSysPool, 0);
	cmd->flags = flags;
	cmd->function = function;
	cmd->description = description;
	cmd->bInUse = true;

	// Link it into the hash tree
	cmd->hashNext = com_cmdHashTree[cmd->hashValue];
	com_cmdHashTree[cmd->hashValue] = cmd;

	return cmd;
}


/*
============
Cmd_RemoveCommand
============
*/
void Cmd_RemoveCommand(conCmd_t *Command)
{
	if (!Command)
	{
		Com_Printf(PRNT_ERROR, "Cmd_RemoveCommand: called without a command pointer to remove!\n");
		return;
	}

	// Find the spot
	conCmdInternal_t *cmd = (conCmdInternal_t *)Command;
	if (!cmd->bInUse)
	{
		Com_Printf (PRNT_WARNING, "Cmd_RemoveCommand: %s: not in command list\n", cmd->name);
		return;
	}

	// De-link it from hash list
	conCmdInternal_t **prev;
	prev = &com_cmdHashTree[cmd->hashValue];
	for ( ; ; )
	{
		cmd = *prev;
		if (!cmd)
		{
			Com_Printf (PRNT_WARNING, "Cmd_RemoveCommand: %s: not added to hash list\n", cmd->name);
			return;
		}

		if (cmd == Command)
		{
			*prev = cmd->hashNext;
			Mem_Free (cmd->name);
			memset (cmd, 0, sizeof(conCmdInternal_t));
			return;
		}
		prev = &cmd->hashNext;
	}
}


/*
============
Cmd_RemoveCGameCmds
============
*/
uint32 Cmd_RemoveByFlag(const int flags)
{
	uint32 Result = 0;

	for (uint32 i=0 ; i<com_numCmdFuncs ; i++)
	{
		conCmdInternal_t *cmd = &com_cmdFuncList[i];
		if (!cmd->bInUse || !(cmd->flags & flags))
			continue;

		Cmd_RemoveCommand(cmd);
		i--;
		Result++;
	}

	return Result;
}

/*
==============================================================================

	CONSOLE COMMANDS

==============================================================================
*/

/*
===============
Cmd_Exec_f
===============
*/
static void Cmd_Exec_f()
{
	char	*buf;
	int		fileLen;

	if (Cmd_Argc () != 2) {
		Com_Printf (0, "syntax: exec <filename> : execute a script file\n");
		return;
	}

	// Sanity check (don't load, just get size -- in case some jerkoff decides to exec pak0.pak)
	fileLen = FS_LoadFile (Cmd_Argv (1), NULL, true);
	if (fileLen >= COMMAND_BUFFER_SIZE) {
		Com_Printf (PRNT_ERROR, "Cmd_Exec_f: %s exceeds maximum config file length\n", Cmd_Argv (1));
		return;
	}

	// Load the file
	fileLen = FS_LoadFile (Cmd_Argv (1), (void **)&buf, true);
	if (!buf || fileLen <= 0) {
		Com_Printf (PRNT_WARNING, "Cmd_Exec_f: couldn't exec %s -- %s\n", Cmd_Argv (1), (fileLen == -1) ? "not found" : "empty file");
		return;
	}
	Com_Printf (0, "executing %s\n", Cmd_Argv (1));

	// Execute
	Cbuf_InsertText (buf);

	// Done
	FS_FreeFile (buf);
}


/*
===============
Cmd_Echo_f

Just prints the rest of the line to the console
===============
*/
static void Cmd_Echo_f ()
{
	int		i;
	
	for (i=1 ; i<Cmd_Argc () ; i++) {
		Com_Printf (0, "%s ", Cmd_Argv (i));
	}

	Com_Printf (0, "\n");
}


/*
============
Cmd_List_f
============
*/
static int alphaSortCmp (const void *_a, const void *_b)
{
	const conCmdInternal_t *a = (const conCmdInternal_t *) _a;
	const conCmdInternal_t *b = (const conCmdInternal_t *) _b;

	return Q_stricmp (a->name, b->name);
}
static void Cmd_List_f () {
	conCmdInternal_t	*cmd, *sortedList;
	int			i, j, total;
	int			matching, longest;
	char		*wildCard;
	uint32		c;

	if ((Cmd_Argc () != 1) && (Cmd_Argc () != 2)) {
		Com_Printf (0, "usage: cmdlist [wildcard]\n");
		return;
	}

	if (Cmd_Argc () == 2)
		wildCard = Cmd_Argv (1);
	else
		wildCard = "*";

	// Create a list and get longest cmd length
	for (matching=0, total=0, c=0, cmd=com_cmdFuncList ; c<com_numCmdFuncs ; cmd++, c++, total++) {
		if (!Q_WildcardMatch (wildCard, cmd->name, 1))
			continue;

		matching++;
	}

	if (!matching) {
		Com_Printf (0, "%i commands total, %i matching\n", total, matching);
		return;
	}

	sortedList = (conCmdInternal_t*)Mem_PoolAlloc (matching * sizeof(conCmdInternal_t), com_cmdSysPool, 0);
	for (matching=0, longest=0, c=0, cmd=com_cmdFuncList ; c<com_numCmdFuncs ; cmd++, c++) {
		if (!Q_WildcardMatch (wildCard, cmd->name, 1))
			continue;

		if ((int)strlen (cmd->name) > longest)
			longest = strlen (cmd->name);

		sortedList[matching++] = *cmd;
	}

	// Sort it
	qsort (sortedList, matching, sizeof(sortedList[0]), alphaSortCmp);

	// Print it
	longest++;
	for (j=0 ; j<matching ;  j++) {
		cmd = &sortedList[j];

		Com_Printf (0, "%s ", cmd->name);
		for (i=0 ; i<longest-(int)strlen(cmd->name) ; i++)
			Com_Printf (0, " ");
		Com_Printf (0, "%s\n", cmd->description);
	}

	if (matching)
		Mem_Free (sortedList);
	Com_Printf (0, "%i commands total, %i matching\n", total, matching);
}


/*
============
Cmd_Wait_f

Causes execution of the remainder of the command buffer to be delayed until next frame.
This allows commands like: bind g "impulse 5 ; +attack ; wait ; -attack ; impulse 2"
============
*/
static void Cmd_Wait_f ()
{
	com_cmdWait = true;
}

/*
=============================================================================

	INITIALIZATION

=============================================================================
*/

/*
============
Cmd_Init
============
*/
void Cmd_Init ()
{
	memset (com_cmdFuncList, 0, sizeof(conCmdInternal_t) * MAX_CMDFUNCS);
	memset (com_cmdHashTree, 0, sizeof(com_cmdHashTree));

	Cmd_AddCommand("cmdlist",		0, Cmd_List_f,		"Prints out a list of commands");
	Cmd_AddCommand("echo",			0, Cmd_Echo_f,		"Echos text to the console");
	Cmd_AddCommand("exec",			0, Cmd_Exec_f,		"Execute a cfg file");
	Cmd_AddCommand("wait",			0, Cmd_Wait_f,		"Forces remainder of command buffer to be delayed until next frame");
}
