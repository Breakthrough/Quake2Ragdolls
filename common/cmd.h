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
// cmd.h
//

/*
=============================================================================

	CONSOLE COMMANDS

=============================================================================
*/

extern bool		com_cmdWait;

conCmd_t	*Cmd_Exists(const char *name);
void		Cmd_CallBack(void (*callBack) (const char *name));

conCmd_t	*Cmd_AddCommand(const char *name, const int flags, void (*function) (), const char *description);
void		Cmd_RemoveCommand(conCmd_t *command);
uint32		Cmd_RemoveByFlag(const int flags);

int			Cmd_Argc();
char		*Cmd_Argv(int arg);
char		*Cmd_ArgsOffset(int start);

char		*Cmd_MacroExpandString(char *text);
void		Cmd_TokenizeString(char *text, bool macroExpand);
void		Cmd_ExecuteString(char *text);

void		Cmd_Init();

/*
=============================================================================

	COMMAND BUFFER

=============================================================================
*/

#define	COMMAND_BUFFER_SIZE	0x10000

void	Cbuf_AddText(char *text);

void	Cbuf_InsertText(char *text);
void	Cbuf_Execute();

void	Cbuf_CopyToDefer();
void	Cbuf_InsertFromDefer();

void	Cbuf_Init();
