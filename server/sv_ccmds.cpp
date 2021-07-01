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
// sv_ccmds.c
//

#include "sv_local.h"

/*
===============================================================================

	OPERATOR CONSOLE ONLY COMMANDS

	These cmds can only be entered from stdin or by a remote operator datagram
===============================================================================
*/

/*
====================
SV_SetMaster_f

Specify a list of master servers
====================
*/
static void SV_SetMaster_f ()
{
	int		i, slot;

#ifndef DEDICATED_ONLY
	// Only dedicated servers send heartbeats
	if (!dedicated->intVal) {
		Com_Printf (PRNT_WARNING, "Only dedicated servers use masters.\n");
		return;
	}
#endif

	// Make sure the server is listed public
	Cvar_Set ("public", "1", false);

	for (i=1 ; i<MAX_MASTERS ; i++)
		memset (&sv_masterAddresses[i], 0, sizeof(sv_masterAddresses[i]));

	slot = 1;	// Slot 0 will always contain the id master
	for (i=1 ; i<Cmd_Argc () ; i++) {
		if (slot == MAX_MASTERS)
			break;

		if (!NET_StringToAdr (Cmd_Argv (i), sv_masterAddresses[i])) {
			Com_Printf (PRNT_WARNING, "Bad address: %s\n", Cmd_Argv (i));
			continue;
		}
		if (sv_masterAddresses[slot].port == 0)
			sv_masterAddresses[slot].port = BigShort (PORT_MASTER);

		Com_Printf (0, "Master server at %s\n", NET_AdrToString (sv_masterAddresses[slot]));
		Com_Printf (0, "Sending a ping.\n");

		Netchan_OutOfBandPrint (NS_SERVER, sv_masterAddresses[slot], "ping");

		slot++;
	}

	svs.lastHeartBeat = -9999999;
}


/*
==================
SV_SetPlayer

Sets sv_currentClient and sv_currentEdict to the player with idnum Cmd_Argv (1)
==================
*/
static bool SV_SetPlayer ()
{
	svClient_t	*cl;
	int			i;
	int			idnum;
	char		*s;

	if (Cmd_Argc () < 2)
		return false;

	s = Cmd_Argv (1);

	// Numeric values are just slot numbers
	if (s[0] >= '0' && s[0] <= '9') {
		idnum = atoi (Cmd_Argv (1));
		if (idnum < 0 || idnum >= maxclients->intVal) {
			Com_Printf (PRNT_WARNING, "Bad client slot: %i\n", idnum);
			return false;
		}

		sv_currentClient = &svs.clients[idnum];
		sv_currentEdict = sv_currentClient->edict;
		if (!sv_currentClient->state) {
			Com_Printf (0, "Client %i is not active\n", idnum);
			return false;
		}
		return true;
	}

	// Check for a name match
	for (i=0, cl=svs.clients ; i<maxclients->intVal ; i++, cl++) {
		if (!cl->state)
			continue;

		if (!Q_stricmp (cl->name, s)) {
			sv_currentClient = cl;
			sv_currentEdict = sv_currentClient->edict;
			return true;
		}
	}

	Com_Printf (PRNT_WARNING, "Userid %s is not on the server\n", s);
	return false;
}

/*
===============================================================================

	SAVEGAME FILES

===============================================================================
*/

/*
=====================
SV_WipeSavegame

Delete save/<XXX>/
=====================
*/
static void SV_WipeSavegame (char *saveName)
{
	char	name[MAX_OSPATH];
	char	*s;

	Com_DevPrintf (0, "SV_WipeSaveGame (%s)\n", saveName);

	Q_snprintfz (name, sizeof(name), "%s/save/%s/server.ssv", FS_Gamedir(), saveName);
	remove (name);
	Q_snprintfz (name, sizeof(name), "%s/save/%s/game.ssv", FS_Gamedir(), saveName);
	remove (name);

	Q_snprintfz (name, sizeof(name), "%s/save/%s/*.sav", FS_Gamedir(), saveName);
	s = Sys_FindFirst (name, 0, 0);
	while (s) {
		remove (s);
		s = Sys_FindNext (0, 0);
	}
	Sys_FindClose ();

	Q_snprintfz (name, sizeof(name), "%s/save/%s/*.sv2", FS_Gamedir(), saveName);
	s = Sys_FindFirst (name, 0, 0);
	while (s) {
		remove (s);
		s = Sys_FindNext (0, 0);
	}
	Sys_FindClose ();
}


/*
================
SV_CopySaveGame
================
*/
static void SV_CopySaveGame (char *src, char *dst)
{
	char	name[MAX_OSPATH], name2[MAX_OSPATH];
	int		l, len;
	char	*found;

	Com_DevPrintf (0, "SV_CopySaveGame (%s, %s)\n", src, dst);

	SV_WipeSavegame (dst);

	// Copy the savegame over
	Q_snprintfz (name, sizeof(name), "%s/save/%s/server.ssv", FS_Gamedir(), src);
	Q_snprintfz (name2, sizeof(name2), "%s/save/%s/server.ssv", FS_Gamedir(), dst);
	FS_CreatePath (name2);
	FS_CopyFile (name, name2);

	Q_snprintfz (name, sizeof(name), "%s/save/%s/game.ssv", FS_Gamedir(), src);
	Q_snprintfz (name2, sizeof(name2), "%s/save/%s/game.ssv", FS_Gamedir(), dst);
	FS_CopyFile (name, name2);

	Q_snprintfz (name, sizeof(name), "%s/save/%s/", FS_Gamedir(), src);
	len = strlen (name);
	Q_snprintfz (name, sizeof(name), "%s/save/%s/*.sav", FS_Gamedir(), src);
	found = Sys_FindFirst(name, 0, 0);
	while (found) {
		strcpy (name+len, found+len);

		Q_snprintfz (name2, sizeof(name2), "%s/save/%s/%s", FS_Gamedir(), dst, found+len);
		FS_CopyFile (name, name2);

		// Change sav to sv2
		l = strlen (name);
		strcpy (name+l-3, "sv2");
		l = strlen (name2);
		strcpy (name2+l-3, "sv2");
		FS_CopyFile (name, name2);

		found = Sys_FindNext (0, 0);
	}
	Sys_FindClose ();
}


/*
==============
SV_WriteLevelFile
==============
*/
static void SV_WriteLevelFile ()
{
	char			name[MAX_OSPATH];
	fileHandle_t	fileNum;

	Com_DevPrintf (0, "SV_WriteLevelFile ()\n");

	Q_snprintfz (name, sizeof(name), "save/current/%s.sv2", sv.name);
	FS_OpenFile (name, &fileNum, FS_MODE_WRITE_BINARY);
	if (!fileNum) {
		Com_Printf (PRNT_WARNING, "Failed to open %s\n", name);
		return;
	}
	FS_Write (sv.configStrings, sizeof(sv.configStrings), fileNum);
	CM_WritePortalState (fileNum);

	FS_CloseFile (fileNum);

	Q_snprintfz (name, sizeof(name), "%s/save/current/%s.sav", FS_Gamedir(), sv.name);
	if (ge)
		ge->WriteLevel (name);
}


/*
==============
SV_ReadLevelFile
==============
*/
void SV_ReadLevelFile ()
{
	char			name[MAX_OSPATH];
	fileHandle_t	fileNum;

	Com_DevPrintf (0, "SV_ReadLevelFile ()\n");

	Q_snprintfz (name, sizeof(name), "save/current/%s.sv2", sv.name);
	FS_OpenFile (name, &fileNum, FS_MODE_READ_BINARY);
	if (!fileNum) {
		Com_Printf (PRNT_WARNING, "Failed to open %s\n", name);
		return;
	}

	FS_Read (sv.configStrings, sizeof(sv.configStrings), fileNum);
	CM_ReadPortalState (fileNum);

	FS_CloseFile (fileNum);

	Q_snprintfz (name, sizeof(name), "%s/save/current/%s.sav", FS_Gamedir(), sv.name);
	if (ge)
		ge->ReadLevel (name);
}


/*
==============
SV_WriteServerFile
==============
*/
static void SV_WriteServerFile(bool autoSave)
{
	char			name[MAX_OSPATH];
	char			comment[32];
	time_t			aclock;
	struct tm		*newtime;
	fileHandle_t	fileNum;

	Com_DevPrintf (0, "SV_WriteServerFile (%s)\n", autoSave ? "true" : "false");

	Q_strncpyz (name, "save/current/server.ssv", sizeof(name));
	FS_OpenFile (name, &fileNum, FS_MODE_WRITE_BINARY);
	if (!fileNum) {
		Com_Printf (PRNT_WARNING, "Couldn't write %s\n", name);
		return;
	}

	// Write the comment field
	memset (comment, 0, sizeof(comment));

	if (!autoSave) {
		time (&aclock);
		newtime = localtime (&aclock);
		Q_snprintfz (comment, sizeof(comment), "%2i:%i%i %2i/%2i  ",
					newtime->tm_hour, newtime->tm_min/10, newtime->tm_min%10, newtime->tm_mon+1, newtime->tm_mday);
		strncat (comment, sv.configStrings[CS_NAME], sizeof(comment) - 1 - strlen (comment));
	}
	else {
		// Autosaved
		Q_snprintfz (comment, sizeof(comment), "ENTERING %s", sv.configStrings[CS_NAME]);
	}

	FS_Write (comment, sizeof(comment), fileNum);

	// Write the mapcmd
	FS_Write (svs.mapCmd, sizeof(svs.mapCmd), fileNum);

	// Write all CVAR_LATCH_SERVER cvars
	Cvar_WriteServerSaveVars(fileNum);

	FS_CloseFile (fileNum);

	// Write game state
	Q_snprintfz (name, sizeof(name), "%s/save/current/game.ssv", FS_Gamedir());
	if (ge)
		ge->WriteGame (name, autoSave);
}


/*
==============
SV_ReadServerFile
==============
*/
static void SV_ReadServerFile ()
{
	char			name[MAX_OSPATH], string[128];
	char			comment[32];
	char			mapcmd[MAX_TOKEN_CHARS];
	fileHandle_t	fileNum;

	Com_DevPrintf (0, "SV_ReadServerFile()\n");

	Q_strncpyz (name, "save/current/server.ssv", sizeof(name));
	FS_OpenFile (name, &fileNum, FS_MODE_READ_BINARY);
	if (!fileNum) {
		Com_Printf (PRNT_WARNING, "SV_ReadServerFile: Couldn't read %s\n", name);
		return;
	}

	// Read the comment field
	FS_Read (comment, sizeof(comment), fileNum);

	// Read the mapcmd
	FS_Read (mapcmd, sizeof(mapcmd), fileNum);

	// Read all CVAR_LATCH_SERVER cvars
	for ( ; ; ) {
		if (!FS_Read (name, sizeof(name), fileNum))
			break;

		FS_Read (string, sizeof(string), fileNum);
		Com_DevPrintf (0, "Set %s = %s\n", name, string);
		Cvar_Set (name, string, true);
	}

	FS_CloseFile (fileNum);

	// Start a new game fresh with new cvars
	SV_GameInit ();

	Q_strncpyz (svs.mapCmd, mapcmd, sizeof(svs.mapCmd));

	// Read game state
	Q_snprintfz (name, sizeof(name), "%s/save/current/game.ssv", FS_Gamedir());
	if (ge)
		ge->ReadGame (name);
}


//=========================================================

/*
==================
SV_DemoMap_f

Puts the server in demo mode on a specific map/cinematic
==================
*/
static void SV_DemoMap_f ()
{
	char	*map;
	char	expanded[MAX_QPATH];

	if (Cmd_Argc () < 2) {
		Com_Printf (0, "synax: demomap <demoname>\n");
		if (svs.initialized)
			Com_Printf (0, "current demomap: %s\n", sv.name);
		return;
	}

	// If not a pcx, demo, or cinematic, check to make sure the level exists
	map = Cmd_Argv (1);
	if (!strchr (map, '$') && !strchr (map, '+') && !strchr (map, '.') && map[0] != '*') {
		if (strchr (map, '\\') || strchr (map, '/'))
			Com_NormalizePath (expanded, sizeof(expanded), map);
		else
			Q_snprintfz (expanded, sizeof(expanded), "demos/%s", map);
		Com_DefaultExtension (expanded, ".dm2", sizeof(expanded));

		// Check if it exists
		if (FS_FileExists (expanded) == -1) {
			Com_Printf (0, "Can't find %s\n", expanded);
			return;
		}

		map = Com_SkipPath (expanded);
		SV_LoadMap (true, map, false, false);
		return;
	}

	SV_LoadMap (true, map, false, false);
}


/*
==================
SV_GameMap_f

Saves the state of the map just being exited and goes to a new map.

If the initial character of the map string is '*', the next map is
in a new unit, so the current savegame directory is cleared of
map files.

Example:

*inter.cin+jail

Clears the archived maps, plays the inter.cin cinematic, then
goes to map jail.bsp.
==================
*/
static void SV_GameMap_f ()
{
	char		*map;
	int			i;
	svClient_t	*cl;
	bool		*savedInuse;
	char		expanded[MAX_QPATH];

	if (Cmd_Argc () != 2) {
		Com_Printf (0, "sytnax: gamemap <map>\n");
		return;
	}

	Com_DevPrintf (0, "SV_GameMap(%s)\n", Cmd_Argv (1));

	FS_CreatePath (Q_VarArgs ("%s/save/current/", FS_Gamedir()));

	// Check for clearing the current savegame
	map = Cmd_Argv (1);
	if (map[0] == '*') {
		// Wipe all the *.sav files
		SV_WipeSavegame ("current");
	}
	else {
		// Save the map just exited
		if (!strchr (map, '$') && !strchr (map, '+') && !strchr (map, '.')) {
			if (strchr (map, '\\') || strchr (map, '/'))
				Com_NormalizePath (expanded, sizeof(expanded), map);
			else
				Q_snprintfz (expanded, sizeof(expanded), "maps/%s.bsp", map);

			// Check it exists
			if (FS_FileExists (expanded) == -1) {
				Com_Printf (0, "Can't find map '%s'\n", expanded);
				return;
			}

			map = Com_SkipPath (expanded);
			Com_StripExtension (expanded, sizeof(expanded), map);
			map = expanded;
		}

		if (Com_ServerState () == SS_GAME) {
			/*
			** Clear all the client inUse flags before saving so that
			** when the level is re-entered, the clients will spawn
			** at spawn points instead of occupying body shells
			*/
			savedInuse = (bool*)Mem_PoolAlloc (maxclients->intVal * sizeof(bool), sv_genericPool, 0);
			for (i=0, cl=svs.clients ; i<maxclients->intVal ; i++, cl++) {
				savedInuse[i] = cl->edict->inUse ? true : false;
				cl->edict->inUse = FALSE;
			}

			SV_WriteLevelFile ();

			// We must restore these for clients to transfer over correctly
			for (i=0, cl=svs.clients ; i<maxclients->intVal ; i++, cl++)
				cl->edict->inUse = savedInuse[i];
			Mem_Free (savedInuse);
		}
	}

	// Start up the next map
	SV_LoadMap (false, map, false, !Q_stricmp (Cmd_Argv (0), "devmap"));

	// Archive server state
	Q_strncpyz (svs.mapCmd, Cmd_Argv (1), sizeof(svs.mapCmd));

#ifndef DEDICATED_ONLY
	// Copy off the level to the autosave slot
	if (!dedicated->intVal) {
		SV_WriteServerFile (true);
		SV_CopySaveGame ("current", "save0");
	}
#endif
}


/*
==================
SV_Map_f

Goes directly to a given map without any savegame archiving.
For development work
==================
*/
static void SV_Map_f ()
{
	char	*map;
	char	expanded[MAX_QPATH];

	if (Cmd_Argc () < 2) {
		Com_Printf (0, "synax: map <mapname>\n");
		if (svs.initialized)
			Com_Printf (0, "current map: %s\n", sv.name);
		return;
	}

	// If not a pcx, demo, or cinematic, check to make sure the level exists
	map = Cmd_Argv (1);
	if (!strchr (map, '$') && !strchr (map, '+') && !strchr (map, '.') && map[0] != '*') {
		if (strchr (map, '\\') || strchr (map, '/'))
			Com_NormalizePath (expanded, sizeof(expanded), map);
		else
			Q_snprintfz (expanded, sizeof(expanded), "maps/%s.bsp", map);

		// Check if it exists
        if (FS_FileExists (expanded) == -1) {
			Com_Printf (0, "Can't find map '%s'\n", expanded);
			return;
		}
	}

	SV_SetState (SS_DEAD);	// Don't save current level when changing
	SV_WipeSavegame ("current");
	SV_GameMap_f ();
}

/*
=============================================================================

  SAVEGAMES

=============================================================================
*/

/*
==============
SV_Loadgame_f
==============
*/
static void SV_Loadgame_f ()
{
	char			name[MAX_OSPATH];
	fileHandle_t	fileNum;
	char			*dir;

	if (Cmd_Argc () != 2) {
		Com_Printf (0, "syntax: loadgame <directory>\n");
		return;
	}

	Com_Printf (0, "Loading game...\n");

	dir = Cmd_Argv (1);
	if (strstr (dir, "..") || strstr (dir, "/") || strstr (dir, "\\"))
		Com_Printf (PRNT_WARNING, "Bad savedir.\n");

	// Make sure the server.ssv file exists
	Q_snprintfz (name, sizeof(name), "save/%s/server.ssv", Cmd_Argv (1));
	FS_OpenFile (name, &fileNum, FS_MODE_READ_BINARY);
	if (!fileNum) {
		Com_Printf (0, "No such savegame: %s\n", name);
		return;
	}
	FS_CloseFile (fileNum);

	SV_CopySaveGame (Cmd_Argv (1), "current");

	SV_ReadServerFile ();

	// Go to the map
	SV_SetState (SS_DEAD);	// Don't save current level when changing
	SV_LoadMap (false, svs.mapCmd, true, false);
}


/*
==============
SV_Savegame_f
==============
*/
static void SV_Savegame_f ()
{
	char	*dir;

	if (Com_ServerState () != SS_GAME) {
		Com_Printf (0, "You must be in a game to save.\n");
		return;
	}

	if (Cmd_Argc () != 2) {
		Com_Printf (0, "syntax: savegame <directory>\n");
		return;
	}

	if (Cvar_GetFloatValue("deathmatch")) {
		Com_Printf (0, "Can't savegame in a deathmatch\n");
		return;
	}

	if (!strcmp (Cmd_Argv (1), "current")) {
		Com_Printf (0, "Can't save to 'current'\n");
		return;
	}

	if (maxclients->intVal == 1 && svs.clients[0].edict->client->playerState.stats[STAT_HEALTH] <= 0) {
		Com_Printf (0, "\nCan't savegame while dead!\n");
		return;
	}

	dir = Cmd_Argv (1);
	if (strstr (dir, "..") || strstr (dir, "/") || strstr (dir, "\\"))
		Com_Printf (PRNT_WARNING, "Bad savedir.\n");

	Com_Printf (0, "Saving game...\n");

	/*
	** Archive current level, including all client edicts. when the level is reloaded,
	** they will be shells awaiting a connecting client
	*/
	SV_WriteLevelFile ();

	// Save server state
	SV_WriteServerFile (false);

	// Copy it off
	SV_CopySaveGame ("current", dir);

	Com_Printf (0, "Done.\n");
}

// ==========================================================================

/*
==================
SV_Kick_f

Kick a user off of the server
==================
*/
static void SV_Kick_f ()
{
	if (!svs.initialized) {
		Com_Printf (0, "No server running.\n");
		return;
	}

	if (Cmd_Argc () != 2) {
		Com_Printf (0, "syntax: kick <userid>\n");
		return;
	}

	if (!SV_SetPlayer ())
		return;

	// R1: ignore kick message on connecting players (and those with no name)
	if (sv_currentClient->state == SVCS_SPAWNED && *sv_currentClient->name)
		SV_BroadcastPrintf (PRINT_HIGH, "%s was kicked\n", sv_currentClient->name);

	// Print directly, because the dropped client won't get the SV_BroadcastPrintf message
	SV_ClientPrintf (sv_currentClient, PRINT_HIGH, "You were kicked from the game\n");
	SV_DropClient (sv_currentClient);
	sv_currentClient->lastMessage = svs.realTime;	// min case there is a funny zombie
}


/*
================
SV_Status_f
================
*/
static void SV_Status_f ()
{
	int			i, j, l;
	svClient_t	*cl;
	char		*s;
	int			ping;

	if (!svs.clients) {
		Com_Printf (0, "No server running.\n");
		return;
	}

	Com_Printf (0, "map              : %s\n", sv.name);

	Com_Printf (0, "num score ping name            lastmsg address               qPort  ver\n");
	Com_Printf (0, "--- ----- ---- --------------- ------- --------------------- ------ ---\n");
	for (i=0, cl=svs.clients ; i<maxclients->intVal ; i++, cl++) {
		if (!cl->state)
			continue;
		Com_Printf (0, "%3i ", i);
		Com_Printf (0, "%5i ", cl->edict->client->playerState.stats[STAT_FRAGS]);

		if (cl->state == SVCS_CONNECTED)
			Com_Printf (0, "CNCT ");
		else if (cl->state == SVCS_FREE)
			Com_Printf (0, "ZMBI ");
		else {
			ping = cl->ping < 9999 ? cl->ping : 9999;
			Com_Printf (0, "%4i ", ping);
		}

		Com_Printf (0, "%s", cl->name);
		l = 16 - strlen (cl->name);
		for (j=0 ; j<l ; j++)
			Com_Printf (0, " ");

		Com_Printf (0, "%7i ", svs.realTime - cl->lastMessage);

		s = NET_AdrToString (cl->netChan.remoteAddress);
		Com_Printf (0, "%s", s);
		l = 22 - strlen (s);
		for (j=0 ; j<l ; j++)
			Com_Printf (0, " ");
		
		Com_Printf (0, "%5i", cl->netChan.qPort);

		Com_Printf (0, "%2i", cl->protocol);

		Com_Printf (0, "\n");
	}
	Com_Printf (0, "\n");
}


/*
==================
SV_ConSay_f
==================
*/
static void SV_ConSay_f ()
{
	svClient_t *client;
	int			j;
	char		*p, text[1024];

	if (Cmd_Argc () < 2)
		return;

	Q_strncpyz (text, "console: ", sizeof(text));
	p = Cmd_ArgsOffset (1);

	if (*p == '"') {
		p++;
		p[strlen (p) - 1] = 0;
	}

	Q_strcatz (text, p, sizeof(text));
	Com_Printf (0, "%s\n", text);

	if (!svs.clients)
		return;

	for (j=0, client=svs.clients; j<maxclients->intVal ; j++, client++) {
		if (client->state != SVCS_SPAWNED)
			continue;
		SV_ClientPrintf (client, PRINT_CHAT, "%s\n", text);
	}
}


/*
==================
SV_Heartbeat_f
==================
*/
static void SV_Heartbeat_f ()
{
	svs.lastHeartBeat = -9999999;
}


/*
===========
SV_Serverinfo_f

Examine or change the serverinfo string
===========
*/
static void SV_Serverinfo_f ()
{
	Com_Printf (0, "Server info settings:\n");
	Info_Print (Cvar_BitInfo (CVAR_SERVERINFO));
}


/*
===========
SV_DumpUser_f

Examine all a users info strings
===========
*/
static void SV_DumpUser_f ()
{
	if (!svs.initialized) {
		Com_Printf (0, "No server running.\n");
		return;
	}

	if (Cmd_Argc () != 2) {
		Com_Printf (0, "syntax: dumpuser <userid>\n");
		return;
	}

	if (!SV_SetPlayer ())
		return;

	Com_Printf (0, "userinfo\n");
	Com_Printf (0, "--------\n");
	Info_Print (sv_currentClient->userInfo);
}


/*
==============
SV_ServerRecord_f

Begins server demo recording.  Every entity and every message will be
recorded, but no playerinfo will be stored.  Primarily for demo merging.
==============
*/
static void SV_ServerRecord_f ()
{
	char		name[MAX_OSPATH];
	byte		buf_data[32768];
	netMsg_t	buf;
	int			len;
	int			i;

	if (Cmd_Argc () != 2) {
		Com_Printf (0, "serverrecord <demoname>\n");
		return;
	}

	if (svs.demoFile) {
		Com_Printf (0, "Already recording.\n");
		return;
	}

	if (Com_ServerState () != SS_GAME) {
		Com_Printf (0, "You must be in a level to record.\n");
		return;
	}

	// Open the demo file
	Q_snprintfz (name, sizeof(name), "demos/%s.dm2", Cmd_Argv (1));

	Com_Printf (0, "recording to %s.\n", name);
	FS_CreatePath (name);
	FS_OpenFile (name, &svs.demoFile, FS_MODE_WRITE_BINARY);
	if (!svs.demoFile) {
		Com_Printf (PRNT_ERROR, "ERROR: couldn't open.\n");
		return;
	}

	// Setup a buffer to catch all multicasts
	svs.demoMultiCast.Init(svs.demoMultiCastBuf, sizeof(svs.demoMultiCastBuf));

	// Write a single giant fake message with all the startup info
	buf.Init(buf_data, sizeof(buf_data));

	/*
	** Serverdata needs to go over for all types of servers
	** to make sure the protocol is right, and to set the gamedir
	*/

	// Send the serverdata
	buf.WriteByte (SVC_SERVERDATA);
	buf.WriteLong (ORIGINAL_PROTOCOL_VERSION);
	buf.WriteLong (svs.spawnCount);

	// 2 means server demo
	buf.WriteByte (2);	// demos are always attract loops
	buf.WriteString (Cvar_GetStringValue ("gamedir"));
	buf.WriteShort (-1);

	// Send full levelname
	buf.WriteString (sv.configStrings[CS_NAME]);

	for (i=0 ; i<MAX_CFGSTRINGS ; i++) {
		if (sv.configStrings[i][0]) {
			buf.WriteByte (SVC_CONFIGSTRING);
			buf.WriteShort (i);
			buf.WriteString (sv.configStrings[i]);
			if (buf.curSize + 67 >= buf.maxSize) {
				Com_Printf (PRNT_ERROR, "Not enough buffer space available.\n");
				FS_CloseFile (svs.demoFile);
				svs.demoFile = 0;
				return;
			}
		}
	}

	// Write it to the demo file
	Com_DevPrintf (0, "signon message length: %i\n", buf.curSize);
	len = LittleLong (buf.curSize);
	FS_Write (&len, sizeof(len), svs.demoFile);
	FS_Write (buf.data, buf.curSize, svs.demoFile);

	// The rest of the demo file will be individual frames
}


/*
==============
SV_ServerStop_f

Ends server demo recording
==============
*/
static void SV_ServerStop_f ()
{
	if (!svs.demoFile) {
		Com_Printf (0, "Not doing a serverrecord.\n");
		return;
	}
	FS_CloseFile (svs.demoFile);
	svs.demoFile = 0;
	Com_Printf (0, "Recording completed.\n");
}


/*
===============
SV_KillServer_f

Kick everyone off, possibly in preparation for a new game
===============
*/
static void SV_KillServer_f ()
{
	if (!svs.initialized)
		return;

	if (Cmd_Argc () == 1)
		SV_ServerShutdown ("Server was killed.\n", false, false);
	else
		SV_ServerShutdown ("Server is restarting...\n", true, false);

	NET_Config (NET_NONE);	// Close network sockets
}


/*
===============
SV_ServerCommand_f

Let the game dll handle a command
===============
*/
static void SV_ServerCommand_f ()
{
	if (!ge) {
		Com_Printf (0, "No game loaded.\n");
		return;
	}

	if (ge)
		ge->ServerCommand ();
}


/*
==================
SV_OperatorCommandInit
==================
*/
void SV_OperatorCommandInit ()
{
	Cmd_AddCommand ("heartbeat",	0, SV_Heartbeat_f,		"");
	Cmd_AddCommand ("kick",			0, SV_Kick_f,			"");
	Cmd_AddCommand ("status",		0, SV_Status_f,			"");
	Cmd_AddCommand ("serverinfo",	0, SV_Serverinfo_f,		"");
	Cmd_AddCommand ("dumpuser",		0, SV_DumpUser_f,		"");

	Cmd_AddCommand ("map",			0, SV_Map_f,			"Loads a map");
	Cmd_AddCommand ("devmap",		0, SV_Map_f,			"Opens a map with cheats enabled");
	Cmd_AddCommand ("demomap",		0, SV_DemoMap_f,		"Loads a demo");
	Cmd_AddCommand ("gamemap",		0, SV_GameMap_f,		"Loads a map without clearing game state");
	Cmd_AddCommand ("setmaster",	0, SV_SetMaster_f,		"");

#ifndef DEDICATED_ONLY
	if (dedicated->intVal)
#endif
		Cmd_AddCommand ("say",		0, SV_ConSay_f,			"");

	Cmd_AddCommand ("serverrecord",	0, SV_ServerRecord_f,	"");
	Cmd_AddCommand ("serverstop",	0, SV_ServerStop_f,		"");

	Cmd_AddCommand ("save",			0, SV_Savegame_f,		"");
	Cmd_AddCommand ("load",			0, SV_Loadgame_f,		"");

	Cmd_AddCommand ("killserver",	0, SV_KillServer_f,		"");

	Cmd_AddCommand ("sv",			0, SV_ServerCommand_f,	"");
}
