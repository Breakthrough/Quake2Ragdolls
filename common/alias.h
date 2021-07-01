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
// alias.h
//

/*
=============================================================================

	ALIAS COMMANDS

=============================================================================
*/

#define MAX_ALIAS_NAME		32
#define MAX_ALIAS_LOOPS		16

struct aliasCmd_t {
	aliasCmd_t			*next;
	char				name[MAX_ALIAS_NAME];
	char				*value;

	uint32				hashValue;
	aliasCmd_t			*hashNext;
};

extern int			com_aliasCount;			// for detecting runaway loops

aliasCmd_t *Alias_Exists(const char *aliasName);
void		Alias_CallBack(void (*callBack) (const char *name));

void		Alias_RemoveAlias(const char *aliasName);

void		Alias_Init();
