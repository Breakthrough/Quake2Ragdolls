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
// sv_gameapi.c
// Interface to the game dll
//

#include "sv_local.h"

gameExport_t	*ge;

/*
=============================================================================

	GAME IMPORT API

=============================================================================
*/

/*
================
SV_ItemIndex
================
*/
static int SV_ItemIndex (char *name, int start, int max, bool create)
{
	int		i;

	if (!name || !name[0])
		return 0;

	for (i=1 ; i<max && sv.configStrings[start+i][0] ; i++) {
		if (!strcmp (sv.configStrings[start+i], name))
			return i;
	}

	if (!create)
		return 0;

	if (i == max)
		Com_Error (ERR_DROP, "*Index: overflow");

	strncpy (sv.configStrings[start+i], name, sizeof(sv.configStrings[i]));

	if (Com_ServerState () != SS_LOADING) {
		// Send the update to everyone
		sv.multiCast.Clear();
		sv.multiCast.WriteChar (SVC_CONFIGSTRING);
		sv.multiCast.WriteShort (start+i);
		sv.multiCast.WriteString (name);
		SV_Multicast (vec3Origin, MULTICAST_ALL_R);
	}

	return i;
}

static int GI_ModelIndex (char *name) { return SV_ItemIndex (name, CS_MODELS, MAX_CS_MODELS, true); }
static int GI_SoundIndex (char *name) { return SV_ItemIndex (name, CS_SOUNDS, MAX_CS_SOUNDS, true); }
static int GI_ImageIndex (char *name) { return SV_ItemIndex (name, CS_IMAGES, MAX_CS_IMAGES, true); }


/*
=================
GI_SetModel

Also sets mins and maxs for inline bmodels
=================
*/
static void GI_SetModel (edict_t *ent, char *name)
{
	struct cmBspModel_t	*mod;
	int			i;

	if (!name)
		Com_Error (ERR_DROP, "GI_SetModel: NULL");

	i = GI_ModelIndex (name);

	ent->s.modelIndex = i;

	// If it is an inline model, get the size information for it
	if (name[0] == '*') {
		mod = CM_InlineModel (name);
		CM_InlineModelBounds (mod, ent->mins, ent->maxs);
		SV_LinkEdict (ent);
	}
}


/*
===============
GI_ConfigString
===============
*/
static void GI_ConfigString (int index, char *val)
{
	if (index < 0 || index >= MAX_CFGSTRINGS)
		Com_Error (ERR_DROP, "configstring: bad index %i\n", index);

	if (!val)
		val = "";

	// Change the string in sv
	strcpy (sv.configStrings[index], val);

	if (Com_ServerState () != SS_LOADING) {
		// Send the update to everyone
		sv.multiCast.Clear();
		sv.multiCast.WriteChar (SVC_CONFIGSTRING);
		sv.multiCast.WriteShort (index);
		sv.multiCast.WriteString (val);

		SV_Multicast (vec3Origin, MULTICAST_ALL_R);
	}
}

// ==========================================================================

static void GI_WriteChar (int c)		{ sv.multiCast.WriteChar (c); }
static void GI_WriteByte (int c)		{ sv.multiCast.WriteByte (c); }
static void GI_WriteShort (int c)		{ sv.multiCast.WriteShort (c); }
static void GI_WriteLong (int c)		{ sv.multiCast.WriteLong (c); }
static void GI_WriteFloat (float f)		{ sv.multiCast.WriteFloat (f); }
static void GI_WriteString (char *s)	{ sv.multiCast.WriteString (s); }
static void GI_WritePos (vec3_t pos)	{ sv.multiCast.WritePos (pos); }
static void GI_WriteDir (vec3_t dir)	{ sv.multiCast.WriteDir (dir); }
static void GI_WriteAngle (float f)		{ sv.multiCast.WriteAngle (f); }

// ==========================================================================

/*
=================
GI_IsInPVS

Also checks portalareas so that doors block sight
=================
*/
static BOOL GI_IsInPVS (vec3_t p1, vec3_t p2)
{
	int		leafnum;
	int		cluster;
	int		area1, area2;
	byte	*mask;

	leafnum = CM_PointLeafnum (p1);
	cluster = CM_LeafCluster (leafnum);
	area1 = CM_LeafArea (leafnum);
	mask = CM_ClusterPVS (cluster);

	leafnum = CM_PointLeafnum (p2);
	cluster = CM_LeafCluster (leafnum);
	area2 = CM_LeafArea (leafnum);

	if (mask && (!(mask[cluster>>3] & BIT(cluster&7))))
		return false;

	if (!CM_AreasConnected (area1, area2))
		return false;		// A door blocks sight

	return true;
}


/*
=================
GI_IsInPHS

Also checks portalareas so that doors block sound
=================
*/
static BOOL GI_IsInPHS (vec3_t p1, vec3_t p2)
{
	int		leafnum;
	int		cluster;
	int		area1, area2;
	byte	*mask;

	leafnum = CM_PointLeafnum (p1);
	cluster = CM_LeafCluster (leafnum);
	area1 = CM_LeafArea (leafnum);
	mask = CM_ClusterPHS (cluster);

	leafnum = CM_PointLeafnum (p2);
	cluster = CM_LeafCluster (leafnum);
	area2 = CM_LeafArea (leafnum);

	if (mask && (!(mask[cluster>>3] & BIT(cluster&7))))
		return false;		// More than one bounce away

	if (!CM_AreasConnected (area1, area2))
		return false;		// A door blocks hearing

	return true;
}

// ==========================================================================

/*
=================
GI_StartSound
=================
*/
static void GI_StartSound (edict_t *entity, int channel, int soundNum, float volume, float attenuation, float timeOffset)
{
	if (!entity)
		return;

	SV_StartSound (NULL, entity, channel, soundNum, volume, attenuation, timeOffset);
}

// ==========================================================================

/*
===============
GI_DevPrintf

Debug print to server console
===============
*/
static void GI_DevPrintf (char *fmt, ...)
{
	char		msg[MAX_COMPRINT];
	va_list		argptr;

	// Evaluate args
	va_start (argptr, fmt);
	vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	// Print
	Com_ConPrint (0, msg);
}


/*
===============
GI_CPrintf

Print to a single client
===============
*/
static void GI_CPrintf (edict_t *ent, EGamePrintLevel level, char *fmt, ...)
{
	char		msg[1024];
	va_list		argptr;
	int			n = 0;

	if (ent) {
		n = NUM_FOR_EDICT(ent);
		if (n < 1 || n > maxclients->intVal)
			Com_Error (ERR_DROP, "cprintf to a non-client");
	}

	// Evaluate args
	va_start (argptr, fmt);
	vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	// Print
	if (ent)
		SV_ClientPrintf (svs.clients+(n-1), level, "%s", msg);
	else
		Com_Printf (0, "%s", msg);
}


/*
===============
GI_CenterPrintf

centerprint to a single client
===============
*/
static void GI_CenterPrintf (edict_t *ent, char *fmt, ...)
{
	char		msg[1024];
	va_list		argptr;
	int			n;

	n = NUM_FOR_EDICT(ent);
	if ((n < 1) || (n > maxclients->intVal))
		return;

	va_start (argptr,fmt);
	vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	sv.multiCast.WriteByte (SVC_CENTERPRINT);
	sv.multiCast.WriteString (msg);
	SV_Unicast (ent, true);
}


/*
===============
GI_Error

Abort the server with a game error
===============
*/
static void GI_Error (char *fmt, ...)
{
	char		msg[1024];
	va_list		argptr;

	va_start (argptr,fmt);
	vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	Com_Error (ERR_DROP, "Game Error: %s", msg);
}


/*
=================
GI_TagAlloc

Makes sure the game DLL does not use client, or signed tags
=================
*/
static void *GI_TagAlloc (int size, int tagNum)
{
	if (tagNum < 0)
		tagNum *= -1;

	return _Mem_Alloc (size, sv_gameSysPool, tagNum, "GAME DLL", 0);
}

/*
=================
GI_MemFree
=================
*/
static void GI_MemFree (void *ptr)
{
	_Mem_Free (ptr, "GAME DLL", -1);
}


/*
=================
GI_FreeTags

Makes sure the game DLL does not use client, or signed tags
=================
*/
static void GI_FreeTags (int tagNum)
{
	if (tagNum < 0)
		tagNum *= -1;

	_Mem_FreeTag (sv_gameSysPool, tagNum, "GAME DLL", 0);
}


/*
===============
GI_Pmove
===============
*/
static void GI_Pmove (pMove_t *pMove)
{
	pMoveNew_t	epm;

	memcpy (&epm, pMove, sizeof(pMove_t));
	epm.cmd = pMove->cmd;
	epm.groundEntity = pMove->groundEntity;
	epm.pointContents = pMove->pointContents;
	epm.trace = pMove->trace;
	epm.multiplier = 1;
	epm.strafeHack = 0;

#ifndef DEDICATED_ONLY
	if (!dedicated->intVal && !CL_CGModule_Pmove (&epm, sv_airAcceleration))
#endif
		SV_Pmove (&epm, sv_airAcceleration);

	pMove->groundEntity = epm.groundEntity;
	memcpy (pMove, &epm, sizeof(pMove_t));
	memcpy (pMove->touchEnts, &epm.touchEnts, sizeof(pMove->touchEnts));
}


/*
===============
GI_SetAreaPortalState
===============
*/
static void GI_SetAreaPortalState (int portalNum, BOOL open)
{
	edict_t	*ent;
	int		area1, area2;

	ent = EDICT_NUM (portalNum);
	if (ent) {
		area1 = ent->areaNum;
		area2 = ent->areaNum2;
	}
	else {
		area1 = 0;
		area2 = 0;
	}

	// Change areaportal's state
	CM_SetAreaPortalState (portalNum, area1, area2, open ? true : false);
}

/*
===============
GI_AreasConnected
===============
*/
static inline BOOL GI_AreasConnected(int area1, int area2)
{
	return CM_AreasConnected(area1, area2);
}

// ==========================================================================

/*
===============
GI_Cvar_Register
===============
*/
static cVar_t *GI_Cvar_Register(char *varName, char *defaultValue, int flags)
{
	return Cvar_Register(varName, defaultValue, flags);
}

/*
===============
GI_Cvar_Set
===============
*/
static cVar_t *GI_Cvar_Set(char *varName, char *value)
{
	return Cvar_Set(varName, value, false);
}


/*
===============
GI_Cvar_ForceSet
===============
*/
static cVar_t *GI_Cvar_ForceSet(char *varName, char *value)
{
	return Cvar_Set(varName, value, true);
}

// ==========================================================================

/*
===============
SV_GameAPI_Init

Init the game subsystem for a new map
===============
*/
#ifdef DEDICATED_ONLY
void CL_CGModule_DebugGraph (float value, int color)
{
	Com_Printf (PRNT_ERROR, "DebugGraph is not supported in a dedicated-only server!\n");
}
#else
void CL_CGModule_DebugGraph (float value, int color);
#endif

void R_GetModelVertices(const char *model, void (*receiveVertice) (vec3_t *vertice, int *indices, int num_indices));
void R_GetBModelVertices (int index, void (*receiveVertice) (TList<btVector3> vertices));
btVector3 R_GetBModelOrigin(int index);

void SV_SetPhysics(void *physics)
{
	sv.physicsWorld = physics;
}

int GI_Sys_Milliseconds ()
{
	return Sys_Milliseconds();
}

char *GI_argv()
{
	return Cmd_ArgsOffset(1);
}

void SV_GameAPI_Init ()
{
	gameImport_t	gi;

	// Shutdown if already active
	SV_GameAPI_Shutdown ();

	Com_DevPrintf (0, "Game Initialization start\n");

	// Initialize pointers
	gi.multicast			= SV_Multicast;
	gi.unicast				= SV_Unicast;
	gi.bprintf				= SV_BroadcastPrintf;
	gi.dprintf				= GI_DevPrintf;
	gi.cprintf				= GI_CPrintf;
	gi.centerprintf			= GI_CenterPrintf;
	gi.error				= GI_Error;

	gi.linkentity			= SV_LinkEdict;
	gi.unlinkentity			= SV_UnlinkEdict;
	gi.BoxEdicts			= SV_AreaEdicts;
	gi.trace				= SV_Trace;
	gi.pointcontents		= SV_PointContents;
	gi.setmodel				= GI_SetModel;
	gi.inPVS				= GI_IsInPVS;
	gi.inPHS				= GI_IsInPHS;
	gi.Pmove				= GI_Pmove;

	gi.modelindex			= GI_ModelIndex;
	gi.soundindex			= GI_SoundIndex;
	gi.imageindex			= GI_ImageIndex;

	gi.R_GetBModelVertices	= R_GetBModelVertices;
	gi.R_GetModelVertices	= R_GetModelVertices;
	gi.SV_SetPhysics		= SV_SetPhysics;
	gi.R_GetBModelOrigin	= R_GetBModelOrigin;
	gi.Sys_Milliseconds		= GI_Sys_Milliseconds;

	gi.configstring			= GI_ConfigString;
	gi.sound				= GI_StartSound;
	gi.positioned_sound		= SV_StartSound;

	gi.WriteChar			= GI_WriteChar;
	gi.WriteByte			= GI_WriteByte;
	gi.WriteShort			= GI_WriteShort;
	gi.WriteLong			= GI_WriteLong;
	gi.WriteFloat			= GI_WriteFloat;
	gi.WriteString			= GI_WriteString;
	gi.WritePosition		= GI_WritePos;
	gi.WriteDir				= GI_WriteDir;
	gi.WriteAngle			= GI_WriteAngle;

	gi.TagMalloc			= GI_TagAlloc;
	gi.TagFree				= GI_MemFree;
	gi.FreeTags				= GI_FreeTags;

	gi.cvar					= GI_Cvar_Register;
	gi.cvar_set				= GI_Cvar_Set;
	gi.cvar_forceset		= GI_Cvar_ForceSet;

	gi.argc					= Cmd_Argc;
	gi.argv					= Cmd_Argv;
	gi.args					= GI_argv;
	gi.AddCommandString		= Cbuf_AddText;

	gi.DebugGraph			= CL_CGModule_DebugGraph;
	gi.SetAreaPortalState	= GI_SetAreaPortalState;
	gi.AreasConnected		= GI_AreasConnected;

	// Get the game api
	Com_DevPrintf (0, "LoadLibrary()\n");
	ge = (gameExport_t *) Sys_LoadLibrary (LIB_GAME, &gi);
	if (!ge)
		Com_Error (ERR_DROP, "GameAPI_Init: Find/load of Game library failed!");

	// Check the api version
	if (ge->apiVersion != GAME_APIVERSION) {
		Com_Error (ERR_DROP, "GameAPI_Init: incompatible apiVersion (%i != %i)", ge->apiVersion, GAME_APIVERSION);

		Sys_UnloadLibrary (LIB_GAME);
		ge = NULL;
	}

	// Check for exports
	if (!ge->Init || !ge->Shutdown || !ge->SpawnEntities || !ge->WriteGame
	|| !ge->ReadGame || !ge->WriteLevel || !ge->ReadLevel || !ge->ClientConnect
	|| !ge->ClientBegin || !ge->ClientUserinfoChanged || !ge->ClientDisconnect || !ge->ClientCommand
	|| !ge->ClientThink || !ge->RunFrame || !ge->ServerCommand)
		Com_Error (ERR_FATAL, "GameAPI_Init: Failed to find all Game exports!");

	// Call to init
	ge->Init ();
}


/*
===============
SV_GameAPI_Shutdown

Called when either the entire server is being killed, or
it is changing to a different game directory.
===============
*/
void SV_GameAPI_Shutdown ()
{
	uint32	size;

	if (!ge)
		return;

	// Tell the module to shutdown locally and unload dll
	ge->Shutdown ();
	Sys_UnloadLibrary (LIB_GAME);
	ge = NULL;

	// Notify of memory leaks
	size = Mem_PoolSize (sv_gameSysPool);
	if (size > 0)
		Com_Printf (PRNT_WARNING, "WARNING: Game memory leak (%u bytes)\n", size);
}
