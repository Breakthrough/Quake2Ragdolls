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
// cvar.h
//

/*
=============================================================================

	CVARS

	Console Variables
=============================================================================
*/

extern bool		com_userInfoModified;

void Cvar_WriteVariables(FILE *f);
void Cvar_WriteServerSaveVars(const fileHandle_t fileNum);

char *Cvar_BitInfo(const int bit);

cVar_t *Cvar_Exists(const char *varName);
void Cvar_CallBack(void (*callBack) (const char *name));
cVar_t *Cvar_Register(const char *varName, const char *defaultValue, const int flags);

void Cvar_FixCheatVars();
void Cvar_GetLatchedVars(const int flags);

int Cvar_GetIntegerValue(const char *varName);
char *Cvar_GetStringValue(const char *varName);
float Cvar_GetFloatValue(const char *varName);

cVar_t *Cvar_VariableSet(cVar_t *cvar, const char *newValue, const bool bForce = false);
cVar_t *Cvar_VariableSetValue(cVar_t *cvar, const float newValue, const bool bForce = false);
cVar_t *Cvar_VariableReset(cVar_t *cvar, const bool bForce = false);

cVar_t *Cvar_Set(const char *varName, const char *newValue, const bool bForce = false);
cVar_t *Cvar_SetValue(const char *varName, const float newValue, const bool bForce = false);
cVar_t *Cvar_Reset(const char *varName, const bool bForce = false);

bool Cvar_Command();

void Cvar_Init();
