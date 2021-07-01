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
// sv_user.c
// Server code for moving users
//

#include "sv_local.h"

edict_t	*sv_currentEdict;

/*
=============================================================================

	USER STRINGCMD EXECUTION

	sv_currentClient and sv_currentEdict will be valid.
=============================================================================
*/

/*
==================
SV_BeginDemoServer
==================
*/
static void SV_BeginDemoserver ()
{
	char	name[MAX_OSPATH];

	Q_snprintfz (name, sizeof(name), "demos/%s", sv.name);
	FS_OpenFile (name, &sv.demoFile, FS_MODE_READ_BINARY);

	if (!sv.demoFile)
		Com_Error (ERR_DROP, "Couldn't open %s", name);
}


/*
================
SV_New_f

Sends the first message from the server to a connected client.
This will be sent on the initial connection and upon each server load.
================
*/
static void SV_New_f ()
{
	char		*gamedir;
	int			playerNum;
	edict_t		*ent;

	Com_DevPrintf (0, "New() from %s\n", sv_currentClient->name);

	if (sv_currentClient->state != SVCS_CONNECTED) {
		Com_Printf (PRNT_WARNING, "New not valid from '%s' (state %d) -- already spawned\n", sv_currentClient->name, sv_currentClient->state);
		return;
	}

	// Demo servers just dump the file message
	if (Com_ServerState () == SS_DEMO) {
		SV_BeginDemoserver ();
		return;
	}

	// Serverdata needs to go over for all types of servers to make sure the protocol is right, and to set the gamedir
	gamedir = Cvar_GetStringValue ("gamedir");

	// Send the serverdata
	sv_currentClient->netChan.message.WriteByte (SVC_SERVERDATA);
	sv_currentClient->netChan.message.WriteLong (ORIGINAL_PROTOCOL_VERSION);
	sv_currentClient->netChan.message.WriteLong (svs.spawnCount);
	sv_currentClient->netChan.message.WriteByte (sv.attractLoop);
	sv_currentClient->netChan.message.WriteString (gamedir);

	switch (Com_ServerState()) {
	case SS_CINEMATIC:
	case SS_PIC:
		playerNum = -1;
		break;

	default:
		playerNum = sv_currentClient - svs.clients;
		break;
	}
	sv_currentClient->netChan.message.WriteShort (playerNum);

	// Send full levelname
	sv_currentClient->netChan.message.WriteString (sv.configStrings[CS_NAME]);

	// Game server
	if (Com_ServerState () == SS_GAME)
	{
		// Set up the entity for the client
		ent = EDICT_NUM(playerNum+1);
		ent->s.number = playerNum+1;
		sv_currentClient->edict = ent;
		memset (&sv_currentClient->lastCmd, 0, sizeof(sv_currentClient->lastCmd));

		// Begin fetching configstrings
		sv_currentClient->netChan.message.WriteByte (SVC_STUFFTEXT);
		sv_currentClient->netChan.message.WriteString (Q_VarArgs ("cmd configstrings %i 0\n", svs.spawnCount));
	}
}


/*
==================
SV_ConfigStrings_f
==================
*/
static void SV_ConfigStrings_f ()
{
	int			start;

	Com_DevPrintf (0, "Configstrings() from %s\n", sv_currentClient->name);

	if (sv_currentClient->state != SVCS_CONNECTED) {
		Com_Printf (0, "configstrings not valid -- already spawned\n");
		return;
	}

	// Handle the case of a level changing while a client was connecting
	if (atoi(Cmd_Argv(1)) != svs.spawnCount) {
		Com_Printf (0, "SV_ConfigStrings_f from different level\n");
		SV_New_f ();
		return;
	}
	
	start = atoi (Cmd_Argv (2));

	// Security check
	if (start < 0) {
		Com_Printf (0, "ERROR: Illegal configstring from %s[%s], client dropped\n",
			sv_currentClient->name, NET_AdrToString (sv_currentClient->netChan.remoteAddress));
		SV_DropClient (sv_currentClient);
		return;
	}

	// Write a packet full of data
	while (sv_currentClient->netChan.message.curSize < MAX_SV_MSGLEN/2 && start < MAX_CFGSTRINGS) {
		if (sv.configStrings[start][0]) {
			sv_currentClient->netChan.message.WriteByte (SVC_CONFIGSTRING);
			sv_currentClient->netChan.message.WriteShort (start);
			sv_currentClient->netChan.message.WriteString (sv.configStrings[start]);
		}

		start++;
	}

	// Send next command
	if (start == MAX_CFGSTRINGS) {
		sv_currentClient->netChan.message.WriteByte (SVC_STUFFTEXT);
		sv_currentClient->netChan.message.WriteString (Q_VarArgs ("cmd baselines %i 0\n", svs.spawnCount));
	}
	else {
		sv_currentClient->netChan.message.WriteByte (SVC_STUFFTEXT);
		sv_currentClient->netChan.message.WriteString (Q_VarArgs ("cmd configstrings %i %i\n", svs.spawnCount, start));
	}
}


/*
==================
SV_Baselines_f
==================
*/
static void SV_Baselines_f ()
{
	int					start;
	entityState_t	nullstate;
	entityState_t	*base;

	Com_DevPrintf (0, "Baselines() from %s\n", sv_currentClient->name);

	if (sv_currentClient->state != SVCS_CONNECTED) {
		Com_Printf (0, "baselines not valid -- already spawned\n");
		return;
	}
	
	// Handle the case of a level changing while a client was connecting
	if (atoi(Cmd_Argv (1)) != svs.spawnCount) {
		Com_Printf (0, "SV_Baselines_f from different level\n");
		SV_New_f ();
		return;
	}
	
	start = atoi(Cmd_Argv (2));
	if (start < 0)
		start = 0;

	// Write a packet full of data
	while (sv_currentClient->netChan.message.curSize < MAX_SV_MSGLEN/2 && start < MAX_CS_EDICTS) {
		base = &sv.baseLines[start];
		if (base->modelIndex || base->sound || base->effects) {
			sv_currentClient->netChan.message.WriteByte (SVC_SPAWNBASELINE);
			sv_currentClient->netChan.message.WriteDeltaEntity (&nullstate, base, true, true);
		}

		start++;
	}

	// Send next command
	if (start == MAX_CS_EDICTS) {
		sv_currentClient->netChan.message.WriteByte (SVC_STUFFTEXT);
		sv_currentClient->netChan.message.WriteString (Q_VarArgs ("precache %i\n", svs.spawnCount));
	}
	else {
		sv_currentClient->netChan.message.WriteByte (SVC_STUFFTEXT);
		sv_currentClient->netChan.message.WriteString (Q_VarArgs ("cmd baselines %i %i\n", svs.spawnCount, start));
	}
}


/*
==================
SV_Begin_f
==================
*/
static void SV_Begin_f ()
{
	if (sv_currentClient->state == SVCS_SPAWNED)
		return;

	Com_DevPrintf (0, "Begin() from %s\n", sv_currentClient->name);

	// Handle the case of a level changing while a client was connecting
	if (atoi (Cmd_Argv (1)) != svs.spawnCount) {
		Com_Printf (0, "SV_Begin_f from different level\n");
		SV_New_f ();
		return;
	}

	sv_currentClient->state = SVCS_SPAWNED;
	
	// Call the game begin function
	if (ge)
		ge->ClientBegin (sv_currentEdict);

	Cbuf_InsertFromDefer ();
}

// ==========================================================================

/*
==================
SV_NextDownload_f
==================
*/
static void SV_NextDownload_f ()
{
	int		r;
	int		percent;
	int		size;

	if (!sv_currentClient->download)
		return;

	r = sv_currentClient->downloadSize - sv_currentClient->downloadCount;
	if (r > 1356)
		r = 1356;

	sv_currentClient->netChan.message.WriteByte (SVC_DOWNLOAD);
	sv_currentClient->netChan.message.WriteShort (r);

	sv_currentClient->downloadCount += r;
	size = sv_currentClient->downloadSize;
	if (!size)
		size = 1;
	percent = sv_currentClient->downloadCount*100/size;
	sv_currentClient->netChan.message.WriteByte (percent);
	sv_currentClient->netChan.message.WriteRaw (sv_currentClient->download + sv_currentClient->downloadCount - r, r);

	if (sv_currentClient->downloadCount != sv_currentClient->downloadSize)
		return;

	FS_FreeFile (sv_currentClient->download);
	sv_currentClient->download = NULL;
}


/*
==================
SV_BeginDownload_f
==================
*/
static void SV_BeginDownload_f ()
{
	extern cVar_t	*allow_download;
	extern cVar_t	*allow_download_players;
	extern cVar_t	*allow_download_models;
	extern cVar_t	*allow_download_sounds;
	extern cVar_t	*allow_download_maps;
	extern bool	fs_fileFromPak; // ZOID did file come from pak?

	char	*name;
	int		offset = 0;

	name = Cmd_Argv (1);

	if (Cmd_Argc () > 2)
		offset = atoi (Cmd_Argv (2)); // downloaded offset

	// Hacked by zoid to allow more conrol over download
	// no longer able to download from root (win32 issue)
	if (strstr (name, "..") || strstr (name, "\\") || !allow_download->intVal		// first off, no .. or global allow check
	|| strstr (name, "..")		// don't allow anything with .. path
	|| (*name == '.')			// leading dot is no good
	|| (*name == '/')			// leading slash bad as well, must be in subdir
	|| (strncmp (name, "players/", 8) == 0 && !allow_download_players->intVal)		// next up, skin check
	|| (strncmp (name, "models/", 7) == 0 && !allow_download_models->intVal)		// now models
	|| (strncmp (name, "sound/", 6) == 0 && !allow_download_sounds->intVal)			// now sounds
	|| (strncmp (name, "maps/", 5) == 0 && !allow_download_maps->intVal)			// now maps (note special case for maps, must not be in pak)
	|| !strstr (name, "/")) {		// MUST be in a subdirectory
		sv_currentClient->netChan.message.WriteByte (SVC_DOWNLOAD);
		sv_currentClient->netChan.message.WriteShort (-1);
		sv_currentClient->netChan.message.WriteByte (0);
		return;
	}

	if (sv_currentClient->download)
		FS_FreeFile (sv_currentClient->download);

	sv_currentClient->downloadSize = FS_LoadFile (name, (void **)&sv_currentClient->download, false);
	if (sv_currentClient->downloadSize == 0)
		sv_currentClient->downloadSize = -1;	// Don't send an empty file
	sv_currentClient->downloadCount = offset;

	if (offset > sv_currentClient->downloadSize)
		sv_currentClient->downloadCount = sv_currentClient->downloadSize;

	if (!sv_currentClient->download
		// Special check for maps, if it came from a pak file, don't allow download  ZOID
		|| (!strncmp (name, "maps/", 5) && fs_fileFromPak)) {
		Com_DevPrintf (0, "Couldn't download %s to %s\n", name, sv_currentClient->name);
		if (sv_currentClient->download) {
			FS_FreeFile (sv_currentClient->download);
			sv_currentClient->download = NULL;
		}

		sv_currentClient->netChan.message.WriteByte (SVC_DOWNLOAD);
		sv_currentClient->netChan.message.WriteShort (-1);
		sv_currentClient->netChan.message.WriteByte (0);
		return;
	}

	SV_NextDownload_f ();
	Com_DevPrintf (0, "Downloading %s to %s\n", name, sv_currentClient->name);
}

//============================================================================

/*
=================
SV_Disconnect_f

The client is going to disconnect, so remove the connection immediately
=================
*/
static void SV_Disconnect_f ()
{
	SV_DropClient (sv_currentClient);	
}


/*
==================
SV_ShowServerinfo_f

Dumps the serverinfo info string
==================
*/
static void SV_ShowServerinfo_f ()
{
	Info_Print (Cvar_BitInfo (CVAR_SERVERINFO));
}

void SV_Nextserver ()
{
	char	*v;

	//ZOID, SS_PIC can be nextserver'd in coop mode
	if (Com_ServerState () == SS_GAME || (Com_ServerState () == SS_PIC && !Cvar_GetFloatValue ("coop")))
		return;		// can't nextserver while playing a normal game

	svs.spawnCount++;	// make sure another doesn't sneak in
	v = Cvar_GetStringValue ("nextserver");
	if (!v[0]) {
		Cbuf_AddText ("killserver\n");
	}
	else {
		Cbuf_AddText (v);
		Cbuf_AddText ("\n");
	}

	Cvar_Set ("nextserver","", false);
}


/*
==================
SV_Nextserver_f

A cinematic has completed or been aborted by a client, so move
to the next server,
==================
*/
static void SV_Nextserver_f ()
{
	if (atoi(Cmd_Argv (1)) != svs.spawnCount) {
		Com_DevPrintf (0, "Nextserver() from wrong level, from %s\n", sv_currentClient->name);
		return;		// leftover from last server
	}

	Com_DevPrintf (0, "Nextserver() from %s\n", sv_currentClient->name);

	SV_Nextserver ();
}


/*
==================
SV_ExecuteUserCommand
==================
*/
struct sv_userCommand_t {
	char	*name;
	void	(*func) ();
};

static sv_userCommand_t sv_userCommands[] = {
	// auto issued
	{"new",				SV_New_f},
	{"configstrings",	SV_ConfigStrings_f},
	{"baselines",		SV_Baselines_f},
	{"begin",			SV_Begin_f},

	{"nextserver",		SV_Nextserver_f},

	{"disconnect",		SV_Disconnect_f},

	// issued by hand at client consoles
	{"info",			SV_ShowServerinfo_f},

	{"download",		SV_BeginDownload_f},
	{"nextdl",			SV_NextDownload_f},

	// null terminate
	{NULL,				NULL}
};
static void SV_ExecuteUserCommand (char *s)
{
	sv_userCommand_t	*u;
	char				*testString;

	// catch attempted server exploits
	testString = Cmd_MacroExpandString (s);
	if (!testString)
		return;

	if (strcmp (testString, s)) {
		if (sv_currentClient->state == SVCS_SPAWNED && *sv_currentClient->name)
			SV_BroadcastPrintf (PRINT_HIGH, "%s was dropped: attempted server exploit\n", sv_currentClient->name);
		SV_DropClient (sv_currentClient);
		return;
	}

	Cmd_TokenizeString (s, false);
	sv_currentEdict = sv_currentClient->edict;

	for (u=sv_userCommands ; u->name ; u++) {
		if (!strcmp (Cmd_Argv(0), u->name)) {
			u->func ();
			return;
		}
	}

	if (Com_ServerState () == SS_GAME && ge)
		ge->ClientCommand (sv_currentEdict);
}

/*
===========================================================================

	USER CMD EXECUTION

===========================================================================
*/

/*
===================
SV_ClientThink
===================
*/
static void SV_ClientThink (svClient_t *cl, userCmd_t *cmd)
{
	cl->commandMsec -= cmd->msec;

	if (cl->commandMsec < 0 && sv_enforcetime->intVal) {
		Com_DevPrintf (PRNT_WARNING, "commandMsec underflow from %s\n", cl->name);
		return;
	}

	if (ge)
		ge->ClientThink (cl->edict, cmd);
}


/*
===================
SV_ExecuteClientMessage

The current sv_netMessage is parsed for the given client
===================
*/
#define MAX_UINFOUPDATES	8
#define MAX_STRINGCMDS		8
void SV_ExecuteClientMessage (svClient_t *cl)
{
	userCmd_t	nullcmd;
	userCmd_t	oldest, oldcmd, newcmd;
	int			netDrop;
	int			uinfoChgCount;
	int			stringCmdCount;
	int			checksum, calculatedChecksum;
	int			checksumIndex;
	bool		moveIssued;
	int			lastFrame;
	int			c;
	char		*s;

	sv_currentClient = cl;
	sv_currentEdict = sv_currentClient->edict;

	// Only allow one move command
	moveIssued = false;

	// Throttle the amount of string commands and userinfo changes in a packet
	stringCmdCount = 0;
	uinfoChgCount = 0;

	for ( ; ; ) {
		if (sv_netMessage.readCount > sv_netMessage.curSize) {
			Com_Printf (0, "SV_ReadClientMessage: badread (%i > %i)\n", sv_netMessage.readCount, sv_netMessage.curSize);
			SV_DropClient (cl);
			return;
		}

		c = sv_netMessage.ReadByte ();
		if (c == -1)
			break;
				
		switch (c) {
		default:
			Com_Printf (PRNT_ERROR, "SV_ReadClientMessage: unknown command char %i\n", c);
			SV_DropClient (cl);
			return;
						
		case CLC_NOP:
			break;

		case CLC_USERINFO:
			if (++uinfoChgCount < MAX_UINFOUPDATES) {
				Q_strncpyz (cl->userInfo, sv_netMessage.ReadString (), sizeof(cl->userInfo));
				SV_UserinfoChanged (cl);
			}
			else {
				Com_Printf (PRNT_WARNING, "WARNING: Too many user info updates (%i) in a single packet from %s\n", uinfoChgCount, cl->name);
				sv_netMessage.ReadString ();
			}

			// Check for out-of-order packets
			if (moveIssued)
				Com_Printf (PRNT_WARNING, "WARNING: Out of order userinfo from %s\n", cl->name);

			if (cl->state == SVCS_FREE)
				return;		// Kicked
			break;

		case CLC_MOVE:
			if (moveIssued) {
				Com_Printf (PRNT_WARNING, "WARNING: User attempted CLC_MOVE when move was already issued: %s\n", cl->name);
				SV_DropClient (cl);
				return;		// Someone is trying to cheat...
			}

			moveIssued = true;
			checksumIndex = sv_netMessage.readCount;
			checksum = sv_netMessage.ReadByte ();
			lastFrame = sv_netMessage.ReadLong ();
			if (lastFrame != cl->lastFrame) {
				cl->lastFrame = lastFrame;
				if (cl->lastFrame > 0) {
					cl->frameLatency[cl->lastFrame&(LATENCY_COUNTS-1)] = 
						svs.realTime - cl->frames[cl->lastFrame & UPDATE_MASK].sentTime;
				}
			}

			memset (&nullcmd, 0, sizeof(nullcmd));
			sv_netMessage.ReadDeltaUsercmd (&nullcmd, &oldest);
			sv_netMessage.ReadDeltaUsercmd (&oldest, &oldcmd);
			sv_netMessage.ReadDeltaUsercmd (&oldcmd, &newcmd);

			if (cl->state != SVCS_SPAWNED) {
				cl->lastFrame = -1;
				break;
			}

			// If the checksum fails, ignore the rest of the packet
			calculatedChecksum = Com_BlockSequenceCRCByte (
				sv_netMessage.data + checksumIndex + 1,
				sv_netMessage.readCount - checksumIndex - 1,
				cl->netChan.incomingSequence);

			if (calculatedChecksum != checksum) {
				Com_DevPrintf (0, "Failed command checksum for %s (%d != %d)/%d\n", 
					cl->name, calculatedChecksum, checksum, 
					cl->netChan.incomingSequence);
				return;
			}

			if (!sv_paused->intVal) {
				netDrop = cl->netChan.dropped;
				if (netDrop < 20) {
					while (netDrop > 2) {
						SV_ClientThink (cl, &cl->lastCmd);

						netDrop--;
					}
					if (netDrop > 1)
						SV_ClientThink (cl, &oldest);

					if (netDrop > 0)
						SV_ClientThink (cl, &oldcmd);

				}
				SV_ClientThink (cl, &newcmd);
			}

			cl->lastCmd = newcmd;
			break;

		case CLC_STRINGCMD:
			c = sv_netMessage.readCount;
			s = sv_netMessage.ReadString ();

			// r1:Another security check, client caps at 256+1, but a hacked client could
			//    send huge strings, if they are then used in a mod which sends a %s cprintf
			//    to the exe, this could result in a buffer overflow for example.
			c = sv_netMessage.readCount - c;
			if (c > 256) {
				Com_Printf (PRNT_WARNING, "WARNING: %i byte excessive stringcmd discarded from %s: '%.32s...'\n", c, cl->name, s);
				break;
			}

			// Check for out-of-order packets
			if (moveIssued)
				Com_Printf (PRNT_WARNING, "WARNING: Out of order string command '%.32s...' from %s\n", s, cl->name);

			// Malicious users may try using too many string commands
			if (++stringCmdCount < MAX_STRINGCMDS)
				SV_ExecuteUserCommand (s);

			if (cl->state == SVCS_FREE)
				return;		// Disconnect command
			break;
		}
	}
}
