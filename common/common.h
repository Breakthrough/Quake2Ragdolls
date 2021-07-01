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
// common.h
// Definitions common between client and server, but not game.dll
//

#ifndef __COMMON_H__
#define __COMMON_H__

#include "../shared/shared.h"
#include "../cgame/cg_shared.h"
#include "files.h"
#include "protocol.h"
#include "cm_public.h"
#include "alias.h"
#include "cmd.h"
#include "cvar.h"
#include "memory.h"
#include "parse.h"

#define APP_NAME			"PGL"
#define APP_VER				"0.3.3"
#define APP_FULLNAME		APP_NAME " v" APP_VER

#define BASE_MODDIRNAME		"basepgl"

#define USE_CURL	0

extern struct memPool_t *com_aliasSysPool;
extern struct memPool_t *com_cmdSysPool;
extern struct memPool_t	*com_cmodelSysPool;
extern struct memPool_t *com_cvarSysPool;
extern struct memPool_t *com_fileSysPool;
extern struct memPool_t *com_genericPool;

// ==========================================================================

extern cVar_t	*developer;
extern cVar_t	*cg_developer;
extern cVar_t	*fs_developer;

extern cVar_t	*dedicated;

// hash optimizing
uint32		Com_HashFileName(const char *fileName, const int hashSize);
uint32		Com_HashGeneric(const char *name, const int hashSize);
uint32		Com_HashGenericFast(const char *name, const int hashSize);

// client/server interactions
void		Com_BeginRedirect (int target, char *buffer, int bufferSize, void (*flush)(int target, char *buffer));
void		Com_EndRedirect ();
void		Com_ConPrint (comPrint_t flags, char *string); // Does not evaluate args
void		Com_Quit (bool error);

// client/server states
EClientState	Com_ClientState ();
void			Com_SetClientState (EClientState state);

EServerState	Com_ServerState ();		// this should have just been a cvar...
void			Com_SetServerState (EServerState state);

// initialization and processing
void		Com_Init (int argc, char **argv);
void		__fastcall Com_Frame (int msec);
void		Com_Shutdown ();

// crc and checksum
byte		Com_BlockSequenceCRCByte (byte *base, int length, int sequence);
uint32		Com_BlockChecksum (void *buffer, int length);

void		Lua_Init();
void		Lua_Shutdown();
Script		*Lua_CreateLuaState (const char *fileName);
Script		*Lua_ScriptFromState (lua_State *state);
void		Lua_RegisterFunctions (Script *state, const ScriptFunctionTable *list);
void		Lua_RegisterGlobals (Script *state, const ScriptGlobalTable *list);
void		Lua_DestroyLuaState (Script *state);

/*
==============================================================================

	NON-PORTABLE SYSTEM SERVICES

==============================================================================
*/

enum ELibType
{
	LIB_CGAME,
	LIB_GAME,

	LIB_MAX
};

uint32		Sys_Cycles();
double		Sys_MSPerCycle();

int			Sys_Milliseconds();
uint32		Sys_UMilliseconds();

void		Sys_Init ();
void		Sys_AppActivate ();

void		Sys_UnloadLibrary (ELibType libType);
void		*Sys_LoadLibrary (ELibType libType, void *parms);

void		Sys_SendKeyEvents ();
void		Sys_Error (char *error, ...);
void		Sys_Print (const char *message);
void		Sys_Quit (bool error);
char		*Sys_GetClipboardData ();

void		Sys_Mkdir (char *path);

// pass in an attribute mask of things you wish to REJECT
char		*Sys_FindFirst (char *path, uint32 mustHave, uint32 cantHave);
char		*Sys_FindNext (uint32 mustHave, uint32 cantHave);
void		Sys_FindClose ();

TList<String> Sys_FindFiles (char *path, char *pattern, int fileCount, bool recurse, bool addFiles, bool addDirs);

// ==========================================================================

char		*Sys_ConsoleInput ();
void		Sys_ShowConsole (int visLevel, bool quitOnClose);
void		Sys_DestroyConsole ();
void		Sys_CreateConsole ();
void		Sys_SetConsoleTitle (const char *buf);
void		Sys_SetErrorText (const char *buf);

/*
=============================================================================

	CLIENT / SERVER SYSTEMS

=============================================================================
*/

//
// cl_cgapi.c
//

bool		CL_CGModule_Pmove (pMoveNew_t *pMove, float airAcceleration);

//
// cl_console.c
//

void		CL_ConsoleInit ();
void		CL_ConsolePrintf (comPrint_t flags, const char *text);

//
// cl_input.c
//

void		CL_UpdateFrameTime (uint32 time);

//
// cl_keys.c
//

void		Key_WriteBindings (FILE *f);

char		*Key_GetBindingBuf (keyNum_t keyNum);
bool		Key_IsDown (keyNum_t keyNum);

void		Key_Init ();

//
// cl_main.c
//

void		CL_Activate (const bool bActive);

bool		CL_ForwardCmdToServer ();
void		CL_ForcePacket ();

void		CL_Disconnect (bool openMenu);

void		__fastcall CL_Frame (int msec);

void		CL_ClientInit ();
void		CL_ClientShutdown (bool error);

//
// cl_screen.c
//

void		SCR_BeginLoadingPlaque ();
void		SCR_EndLoadingPlaque ();

//
// sv_main.c
//

void		SV_ServerInit ();
void		SV_ServerShutdown (char *finalmsg, bool reconnect, bool crashing);
void		SV_Frame (int msec);

void		*SV_GetPhysicsWorld();

#endif // __COMMON_H__
