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
// sv_init.c
//

#include "sv_local.h"

// Network transmission
netAdr_t		sv_netFrom;
netMsg_t		sv_netMessage;
byte			sv_netBuffer[MAX_SV_MSGLEN];

serverStatic_t	svs;	// persistant server info
serverState_t	sv;		// local server

float			sv_airAcceleration = 0;

/*
================
SV_CreateBaseline

Entity baselines are used to compress the update messages
to the clients -- only the fields that differ from the
baseline will be transmitted
================
*/
static void SV_CreateBaseline ()
{
	edict_t		*svent;
	int			entNum;	

	for (entNum=1 ; entNum<ge->numEdicts ; entNum++) {
		svent = EDICT_NUM(entNum);
		if (!svent->inUse)
			continue;
		if (!svent->s.modelIndex && !svent->s.sound && !svent->s.effects)
			continue;
		svent->s.number = entNum;

		// Take current state as baseline
		Vec3Copy (svent->s.origin, svent->s.oldOrigin);

		memcpy (&sv.baseLines[entNum], &svent->s, sizeof(entityState_t));
	}
}


/*
=================
SV_CheckForSavegame
=================
*/
static void SV_CheckForSavegame ()
{
	char		name[MAX_OSPATH];
	FILE		*f;
	int			i;

	if (sv_noreload->intVal)
		return;

	if (Cvar_GetFloatValue ("deathmatch"))
		return;

	Q_snprintfz (name, sizeof(name), "%s/save/current/%s.sav", FS_Gamedir(), sv.name);
	f = fopen (name, "rb");
	if (!f)
		return;		// No savegame

	fclose (f);

	SV_ClearWorld ();

	// Get configstrings and areaportals
	SV_ReadLevelFile ();

	if (!sv.loadGame) {
		// Coming back to a level after being in a different level, so run it for ten seconds

		/*
		** rlava2 was sending too many lightstyles, and overflowing the
		** reliable data. temporarily changing the server state to loading
		** prevents these from being passed down.
		*/
		EServerState	previousState;

		previousState = Com_ServerState ();
		SV_SetState (SS_LOADING);
		for (i=0 ; i<100 ; i++)
			ge->RunFrame ();

		SV_SetState (previousState);
	}
}


/*
================
SV_SpawnServer

Change the server to a new map, taking all connected
clients along with it.
================
*/
static void SV_SpawnServer (char *server, char *spawnPoint, EServerState serverState, bool attractLoop, bool loadGame, bool devMap)
{
	int		i;
	uint32	checkSum;

	Cvar_SetValue ("paused", 0, false);
	if (devMap)
		Cvar_SetValue ("cheats", 1, true);

	Cvar_FixCheatVars ();

	Com_Printf (0, "\n--------- Server Initialization --------\n");

	Com_DevPrintf (0, "SpawnServer: %s\n", server);
	if (sv.demoFile) {
		FS_CloseFile (sv.demoFile);
		sv.demoFile = 0;
	}

	svs.spawnCount++;	// Any partially connected client will be restarted

	// Wipe the entire per-level structure
	sv.Clear();
	SV_SetState (SS_DEAD);
	svs.realTime = 0;
	sv.loadGame = loadGame;
	sv.attractLoop = attractLoop;

	// Save name for levels that don't set message
	strcpy (sv.configStrings[CS_NAME], server);
	if (Cvar_GetFloatValue ("deathmatch")) {
		Q_snprintfz (sv.configStrings[CS_AIRACCEL], sizeof(sv.configStrings[CS_AIRACCEL]), "%g", sv_airaccelerate->floatVal);
		sv_airAcceleration = sv_airaccelerate->floatVal;
	}
	else {
		strcpy (sv.configStrings[CS_AIRACCEL], "0");
		sv_airAcceleration = 0;
	}

	sv.multiCast.Init(sv.multiCastBuf, sizeof(sv.multiCastBuf));

	// Leave slots at start for clients only
	for (i=0 ; i<maxclients->intVal ; i++) {
		// Needs to reconnect
		if (svs.clients[i].state == SVCS_SPAWNED)
			svs.clients[i].state = SVCS_CONNECTED;
		svs.clients[i].lastFrame = -1;
	}

	sv.time = 1000;
	
	Q_strncpyz (sv.name, server, sizeof(sv.name));
	strcpy (sv.configStrings[CS_NAME], server);

	if (serverState == SS_GAME) {
		Q_snprintfz (sv.configStrings[CS_MODELS+1], sizeof(sv.configStrings[CS_MODELS+1]), "maps/%s.bsp", server);
		sv.models[1] = CM_LoadMap (sv.configStrings[CS_MODELS+1], false, &checkSum);
	}
	else {
		sv.models[1] = CM_LoadMap ("", false, &checkSum);	// no real map
	}
	Q_snprintfz (sv.configStrings[CS_MAPCHECKSUM], sizeof(sv.configStrings[CS_MAPCHECKSUM]), "%i", checkSum);

	// Clear physics interaction links
	SV_ClearWorld ();
	
	for (i=1 ; i<CM_NumInlineModels () ; i++) {
		Q_snprintfz (sv.configStrings[CS_MODELS+1+i], sizeof(sv.configStrings[CS_MODELS+1+i]), "*%i", i);
		sv.models[i+1] = CM_InlineModel (sv.configStrings[CS_MODELS+1+i]);
	}

	// Spawn the rest of the entities on the map

	// Precache and static commands can be issued during map initialization
	SV_SetState (SS_LOADING);

	// Load and spawn all other entities
	ge->SpawnEntities (sv.name, CM_EntityString(), spawnPoint);

	// Run two frames to allow everything to settle
	ge->RunFrame ();
	ge->RunFrame ();

	// All precaches are complete
	SV_SetState (serverState);
	
	// Create a baseline for more efficient communications
	SV_CreateBaseline ();

	// Check for a savegame
	SV_CheckForSavegame ();

	// Set serverinfo variable
	Cvar_Set ("mapname", sv.name, true);

	// Update dedicated window title
	SV_UpdateTitle ();

	Com_Printf (0, "----------------------------------------\n");
}


/*
==============
SV_GameInit

A brand new game has been started
==============
*/
void SV_GameInit ()
{
	int		i;
	edict_t	*ent;

	if (svs.initialized) {
		// Cause any connected clients to reconnect
		SV_ServerShutdown ("Server restarted\n", true, false);
	}
	else {
#ifndef DEDICATED_ONLY
		// Make sure the client is down
		if (!dedicated->intVal) {
			CL_Disconnect (false);
			SCR_BeginLoadingPlaque ();
		}
#endif

		// Get any latched variable changes (maxclients, etc)
		Cvar_GetLatchedVars (CVAR_LATCH_SERVER);
	}

	svs.initialized = true;

	if (Cvar_GetFloatValue ("coop") && Cvar_GetFloatValue ("deathmatch")) {
		Com_Printf (0, "Deathmatch and Coop both set, disabling Coop\n");
		Cvar_SetValue ("coop", 0, true);
	}

	/*
	** Dedicated servers are can't be single player and are usually DM
	** so unless they explicity set coop, force it to deathmatch
	*/
#ifndef DEDICATED_ONLY
	if (dedicated->intVal) {
#endif
		if (!Cvar_GetFloatValue ("coop"))
			Cvar_SetValue ("deathmatch", 1, true);
#ifndef DEDICATED_ONLY
	}
#endif

	// Init clients
	if (Cvar_GetFloatValue ("deathmatch")) {
		if (maxclients->intVal <= 1)
			Cvar_SetValue ("maxclients", 8, true);
		else if (maxclients->intVal > MAX_CS_CLIENTS)
			Cvar_SetValue ("maxclients", MAX_CS_CLIENTS, true);
	}
	else if (Cvar_GetFloatValue ("coop")) {
		if (maxclients->intVal <= 1 || maxclients->intVal > 4)
			Cvar_SetValue ("maxclients", 4, true);
	}
	else {
		// Non-deathmatch, non-coop is one player
		Cvar_SetValue ("maxclients", 1, true);
	}

	svs.spawnCount = rand ();
	svs.clients = (svClient_t*)Mem_PoolAlloc (sizeof(svClient_t)*maxclients->intVal, sv_genericPool, 0);
	svs.numClientEntities = maxclients->intVal*UPDATE_BACKUP*64;
	svs.clientEntities = (entityState_t*)Mem_PoolAlloc (sizeof(entityState_t)*svs.numClientEntities, sv_genericPool, 0);

	for (i = 0; i < svs.numClientEntities; ++i)
		svs.clientEntities[i].Clear();

	// Init network stuff
	if (maxclients->intVal > 1)
		NET_Config (NET_SERVER);
	else
		Cvar_SetValue ("cheats", 1, true);

	// Heartbeats will always be sent to the id master
	svs.lastHeartBeat = -9999999;		// send immediately
	NET_StringToAdr ("192.246.40.37:27900", sv_masterAddresses[0]);

	// Init game
	SV_GameAPI_Init ();
	for (i=0 ; i<maxclients->intVal ; i++) {
		ent = EDICT_NUM(i+1);
		ent->s.number = i+1;
		svs.clients[i].edict = ent;
		memset (&svs.clients[i].lastCmd, 0, sizeof(svs.clients[i].lastCmd));
	}
}


/*
======================
SV_LoadMap

  the full syntax is:

  map [*]<map>$<startspot>+<nextserver>

command from the console or progs.
Map can also be a.cin, .pcx, or .dm2 file
Nextserver is used to allow a cinematic to play, then proceed to
another level:

	map tram.cin+jail_e3
======================
*/
void SV_LoadMap (bool attractLoop, char *levelString, bool loadGame, bool devMap)
{
	char	level[MAX_QPATH];
	char	*ch;
	int		len;
	char	spawnPoint[MAX_QPATH];

	sv.loadGame = loadGame;
	sv.attractLoop = attractLoop;

	if (Com_ServerState() == SS_DEAD && !sv.loadGame)
		SV_GameInit ();	// The game is just starting

	Q_strncpyz (level, levelString, sizeof(level));

	// If there is a + in the map, set nextserver to the remainder
	ch = strchr (level, '+');
	if (ch) {
		*ch = '\0';
		Cvar_Set ("nextserver", Q_VarArgs ("gamemap \"%s\"", ch+1), false);
	}
	else
		Cvar_Set ("nextserver", "", false);

	// ZOID special hack for end game screen in coop mode
	if (Cvar_GetFloatValue ("coop") && !Q_stricmp (level, "victory.pcx"))
		Cvar_Set ("nextserver", "gamemap \"*base1\"", false);

	// If there is a $, use the remainder as a spawnPoint
	ch = strchr (level, '$');
	if (ch) {
		*ch = '\0';
		Q_strncpyz (spawnPoint, ch+1, sizeof(spawnPoint));
	}
	else {
		spawnPoint[0] = 0;
	}

	// Skip the end-of-unit flag if necessary
	if (level[0] == '*')
		Q_strncpyz (level, level+1, sizeof(level));

	len = strlen (level);
#ifndef DEDICATED_ONLY
	if (!dedicated->intVal)
		SCR_BeginLoadingPlaque ();			// For local system
#endif
	if (len > 4 && !strcmp (level+len-4, ".cin")) {
		SV_BroadcastCommand ("changing\n");
		SV_SendClientMessages ();
		SV_SpawnServer (level, spawnPoint, SS_CINEMATIC, attractLoop, loadGame, devMap);
	}
	else if (len > 4 && !strcmp (level+len-4, ".dm2")) {
		sv.attractLoop = attractLoop = true;

		SV_BroadcastCommand ("changing\n");
		SV_SendClientMessages ();
		SV_SpawnServer (level, spawnPoint, SS_DEMO, attractLoop, loadGame, devMap);
	}
	else if (len > 4 && !strcmp (level+len-4, ".pcx")) {
		SV_BroadcastCommand ("changing\n");
		SV_SendClientMessages ();
		SV_SpawnServer (level, spawnPoint, SS_PIC, attractLoop, loadGame, devMap);
	}
	else {
		SV_BroadcastCommand ("changing\n");
		SV_SendClientMessages ();
		SV_SpawnServer (level, spawnPoint, SS_GAME, attractLoop, loadGame, devMap);
		Cbuf_CopyToDefer ();
	}

	SV_BroadcastCommand ("reconnect\n");
}
