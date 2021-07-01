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
// sv_main.c.
// Server main program
//

#include "sv_local.h"

bool netAdr_t::IsLocalAddress()
{
	return ((naType == NA_LOOPBACK) ? true : false);
}

netAdr_t	sv_masterAddresses[MAX_MASTERS];	// address of group servers

svClient_t	*sv_currentClient;			// current client

cVar_t	*sv_paused;
cVar_t	*sv_timedemo;

cVar_t	*sv_enforcetime;

cVar_t	*timeout;				// seconds without any message
cVar_t	*zombietime;			// seconds to sink messages after disconnect

cVar_t	*rcon_password;			// password for remote server commands

cVar_t	*allow_download;
cVar_t	*allow_download_players;
cVar_t	*allow_download_models;
cVar_t	*allow_download_sounds;
cVar_t	*allow_download_maps;

cVar_t	*sv_airaccelerate;

cVar_t	*sv_noreload;			// don't reload level state when reentering

cVar_t	*maxclients;
cVar_t	*sv_showclamp;

cVar_t	*hostname;
cVar_t	*public_server;			// should heartbeats be sent

cVar_t	*sv_reconnect_limit;	// minimum seconds between connect messages

struct memPool_t	*sv_gameSysPool;
struct memPool_t	*sv_genericPool;

/*
=====================
SV_SetState
=====================
*/
void SV_SetState (EServerState state)
{
	Com_SetServerState (state);
}


/*
=====================
SV_DropClient

Called when the player is totally leaving the server, either willingly
or unwillingly.  This is NOT called if the entire server is quiting
or crashing.
=====================
*/
void SV_DropClient (svClient_t *drop)
{
	// Add the disconnect
	drop->netChan.message.WriteByte (SVC_DISCONNECT);

	if (drop->state == SVCS_SPAWNED) {
		// Call the prog function for removing a client
		// This will remove the body, among other things
		ge->ClientDisconnect (drop->edict);
	}

	if (drop->download) {
		FS_FreeFile (drop->download);
		drop->download = NULL;
	}

	drop->state = SVCS_FREE;		// become free in a few seconds
	drop->name[0] = 0;
}


/*
=================
SV_UserinfoChanged

Pull specific info from a newly changed userinfo string
into a more C freindly form.
=================
*/
void SV_UserinfoChanged (svClient_t *cl)
{
	char	*val;
	int		i;

	// Call prog code to allow overrides
	ge->ClientUserinfoChanged (cl->edict, cl->userInfo);
	
	// Name for C code
	strncpy (cl->name, Info_ValueForKey(cl->userInfo, "name"), sizeof(cl->name)-1);
	// Mask off high bit
	for (i=0 ; i<sizeof(cl->name) ; i++)
		cl->name[i] &= 127;

	// Rate command
	val = Info_ValueForKey (cl->userInfo, "rate");
	if (strlen (val)) {
		i = atoi(val);
		cl->rate = clamp (i, 100, 15000);
	}
	else
		cl->rate = 5000;

	// MSG command
	val = Info_ValueForKey (cl->userInfo, "msg");
	if (strlen (val))
		cl->messageLevel = atoi (val);
}


/*
===================
SV_UpdateTitle
===================
*/
void SV_UpdateTitle ()
{
	hostname->modified = false;
#ifdef WIN32	// FIXME
	if (Com_ServerState() == SS_GAME) {
		Sys_SetConsoleTitle (Q_VarArgs ("EGL Server: %s (port %i)", Cvar_GetStringValue ("hostname"), Cvar_GetIntegerValue ("port")));
		return;
	}

	Sys_SetConsoleTitle (NULL);
#endif
}

/*
=============================================================================

	Com_Printf REDIRECTION

=============================================================================
*/

#define SV_OUTPUTBUF_LENGTH	(MAX_SV_MSGLEN - 16)
enum {
	RD_NONE,
	RD_CLIENT,
	RD_PACKET
};

static char	sv_outputBuffer[SV_OUTPUTBUF_LENGTH];

/*
=================
SV_FlushRedirect
=================
*/
static void SV_FlushRedirect (int target, char *outBuffer)
{
	switch (target) {
	case RD_PACKET:
		Netchan_OutOfBandPrint (NS_SERVER, sv_netFrom, "print\n%s", outBuffer);
		break;

	case RD_CLIENT:
		sv_currentClient->netChan.message.WriteByte (SVC_PRINT);
		sv_currentClient->netChan.message.WriteByte (PRINT_HIGH);
		sv_currentClient->netChan.message.WriteString (outBuffer);
		break;
	}
}

/*
==============================================================================

	CONNECTIONLESS COMMANDS

==============================================================================
*/

/*
===============
SV_StatusString

Builds the string that is sent as heartbeats and status replies
===============
*/
static char *SV_StatusString ()
{
	char		player[1024];
	static char	status[MAX_SV_MSGLEN - 16];
	int			i;
	svClient_t	*cl;
	int			statusLength;
	int			playerLength;

	Q_snprintfz (status, sizeof(status), "%s\n", Cvar_BitInfo(CVAR_SERVERINFO));
	statusLength = strlen (status);

	for (i=0 ; i<maxclients->intVal ; i++) {
		if (svs.clients[i].state < SVCS_CONNECTED)
			continue;

		cl = &svs.clients[i];
		Q_snprintfz (player, sizeof(player), "%i %i \"%s\"\n", 
			cl->edict->client->playerState.stats[STAT_FRAGS], cl->ping, cl->name);

		playerLength = strlen (player);
		if (statusLength+playerLength >= sizeof(status))
			break;		// Can't hold any more

		strcpy (status+statusLength, player);
		statusLength += playerLength;
	}

	return status;
}


/*
================
SVC_Status_f

Responds with all the info that qplug or qspy can see
================
*/
static void SVC_Status_f ()
{
	Netchan_OutOfBandPrint (NS_SERVER, sv_netFrom, "print\n%s", SV_StatusString());
}


/*
================
SVC_Ack_f
================
*/
static void SVC_Ack_f ()
{
	int		i;

	// R1: could be used to flood server console - only show acks from masters.
	for (i=0 ; i<MAX_MASTERS ; i++) {
		if (!sv_masterAddresses[i].port)
			continue;
		if (!sv_masterAddresses[i].CompareBaseAdr(sv_netFrom))
			continue;

		Com_Printf (0, "Ping acknowledge from %s\n", NET_AdrToString (sv_netFrom));
	}
}


/*
================
SVC_Info_f

Responds with short info for broadcast scans
The second parameter should be the current protocol version number.
================
*/
static void SVC_Info_f ()
{
	char	string[64];
	int		i, count;
	int		version;

	if (maxclients->intVal == 1)
		return;		// Ignore in single player

	version = atoi (Cmd_Argv (1));
	if (version != ORIGINAL_PROTOCOL_VERSION)
		return;

	count = 0;
	for (i=0 ; i<maxclients->intVal ; i++) {
		if (svs.clients[i].state >= SVCS_CONNECTED)
			count++;
	}

	Q_snprintfz (string, sizeof(string), "%16s %8s %2i/%2i\n", hostname->string, sv.name, count, maxclients->intVal);

	Netchan_OutOfBandPrint (NS_SERVER, sv_netFrom, "info\n%s", string);
}


/*
================
SVC_Ping_f

Just responds with an acknowledgement
================
*/
static void SVC_Ping_f ()
{
	Netchan_OutOfBandPrint (NS_SERVER, sv_netFrom, "ack");
}


/*
=================
SVC_GetChallenge_f

Returns a challenge number that can be used in a subsequent client_connect command.
We do this to prevent denial of service attacks that flood the server with invalid connection IPs.
With a challenge, they must give a valid IP address.
=================
*/
static void SVC_GetChallenge_f ()
{
	int		i;
	int		oldest;
	uint32	oldestTime;

	oldest = 0;
	oldestTime = 0xffffffff;	// UINT_MAX?

	// See if we already have a challenge for this ip
	for (i=0 ; i<MAX_CHALLENGES ; i++) {
		if (sv_netFrom.CompareBaseAdr(svs.challenges[i].adr))
			break;
		if (svs.challenges[i].time < oldestTime) {
			oldestTime = svs.challenges[i].time;
			oldest = i;
		}
	}

	if (i == MAX_CHALLENGES) {
		// Overwrite the oldest
		svs.challenges[oldest].challenge = rand() & 0x7fff;
		svs.challenges[oldest].adr = sv_netFrom;
		svs.challenges[oldest].time = Sys_UMilliseconds ();
		i = oldest;
	}

	// Send it back
	Netchan_OutOfBandPrint (NS_SERVER, sv_netFrom, "challenge %i", svs.challenges[i].challenge);
}


/*
==================
SVC_DirectConnect_f

A connection request that did not come from the master
==================
*/
static void SVC_DirectConnect_f ()
{
	char		userInfo[MAX_INFO_STRING];
	netAdr_t	adr;
	int			i;
	svClient_t	*cl, *newcl;
	svClient_t	temp;
	edict_t		*ent;
	int			edictNum;
	int			version;
	int			qPort;
	int			challenge;

	adr = sv_netFrom;

	Com_DevPrintf (0, "SVC_DirectConnect_f ()\n");

	// Check protocol version
	version = atoi (Cmd_Argv (1));
	if (version != ORIGINAL_PROTOCOL_VERSION) {
		Netchan_OutOfBandPrint (NS_SERVER, adr, "print\nServer is not using same protocol (%i != %i).\n",
								version, ORIGINAL_PROTOCOL_VERSION);
		Com_DevPrintf (0, "    rejected connect from version %i\n", version);
		return;
	}

	qPort = atoi (Cmd_Argv (2));
	challenge = atoi (Cmd_Argv (3));
	Q_strncpyz (userInfo, Cmd_Argv (4), sizeof(userInfo));

	// Force the IP key/value pair so the game can filter based on ip
	Info_SetValueForKey (userInfo, "ip", NET_AdrToString (sv_netFrom));

	// Attractloop servers are ONLY for local clients
	if (sv.attractLoop) {
		if (!adr.IsLocalAddress()) {
			Com_Printf (PRNT_WARNING, "Remote connect in attract loop. Ignored.\n");
			Netchan_OutOfBandPrint (NS_SERVER, adr, "print\nConnection refused.\n");
			return;
		}
	}

	// See if the challenge is valid
	if (!adr.IsLocalAddress()) {
		for (i=0 ; i<MAX_CHALLENGES ; i++) {
			if (sv_netFrom.CompareBaseAdr(svs.challenges[i].adr)) {
				if (challenge == svs.challenges[i].challenge)
					break;		// good
				Netchan_OutOfBandPrint (NS_SERVER, adr, "print\nBad challenge.\n");
				return;
			}
		}

		if (i == MAX_CHALLENGES) {
			Netchan_OutOfBandPrint (NS_SERVER, adr, "print\nNo challenge for address.\n");
			return;
		}
	}

	newcl = &temp;
	memset (newcl, 0, sizeof(svClient_t));

	// If there is already a slot for this ip, reuse it
	for (i=0, cl=svs.clients ; i<maxclients->intVal ; i++, cl++) {
		if (cl->state == SVCS_FREE)
			continue;

		if (adr.CompareBaseAdr(cl->netChan.remoteAddress)
		&& (cl->netChan.qPort == qPort || adr.port == cl->netChan.remoteAddress.port)) {
			if (!adr.IsLocalAddress() && svs.realTime-cl->lastConnect < sv_reconnect_limit->intVal*1000) {
				Com_DevPrintf (0, "%s:reconnect rejected : too soon\n", NET_AdrToString (adr));
				return;
			}

			Com_Printf (0, "%s:reconnect\n", NET_AdrToString (adr));

			newcl = cl;
			goto gotNewClient;
		}
	}

	// Find a client slot
	newcl = NULL;
	for (i=0, cl=svs.clients ; i<maxclients->intVal ; i++, cl++) {
		if (cl->state == SVCS_FREE) {
			newcl = cl;
			break;
		}
	}

	if (!newcl) {
		Netchan_OutOfBandPrint (NS_SERVER, adr, "print\nServer is full.\n");
		Com_DevPrintf (0, "Rejected a connection.\n");
		return;
	}

gotNewClient:
	/*
	** Build a new connection and accept the new client
	** This is the only place a svClient_t is ever initialized
	*/
	*newcl = temp;
	sv_currentClient = newcl;
	edictNum = (newcl-svs.clients)+1;
	ent = EDICT_NUM(edictNum);
	newcl->edict = ent;
	newcl->challenge = challenge; // Save challenge for checksumming

	// Get the game a chance to reject this connection or modify the userInfo
	if (!ge->ClientConnect (ent, userInfo)) {
		Com_DevPrintf (0, "Game rejected a connection.\n");

		if (*Info_ValueForKey (userInfo, "rejmsg")) {
			Netchan_OutOfBandPrint (NS_SERVER, adr, "print\n%s\nConnection refused.\n", Info_ValueForKey (userInfo, "rejmsg"));
			return;
		}

		Netchan_OutOfBandPrint (NS_SERVER, adr, "print\nConnection refused.\n");
		return;
	}

	// Parse some info from the info strings
	Q_strncpyz (newcl->userInfo, userInfo, sizeof(newcl->userInfo));
	SV_UserinfoChanged (newcl);

	// Send the connect packet to the client
	Netchan_OutOfBandPrint (NS_SERVER, adr, "client_connect");

	Netchan_Setup (NS_SERVER, newcl->netChan, adr, version, qPort, 0);

	newcl->protocol = version;
	newcl->state = SVCS_CONNECTED;

	newcl->datagram.Init(newcl->datagramBuff, sizeof(newcl->datagramBuff));
	newcl->datagram.allowOverflow = true;

	newcl->lastMessage = svs.realTime - ((timeout->floatVal - 5) * 1000);	// don't timeout
	newcl->lastConnect = svs.realTime;
}


/*
===============
SVC_RemoteCommand_f

A client issued an rcon command.
Shift down the remaining args
Redirect all printfs
===============
*/
static void SVC_RemoteCommand_f ()
{
	char	remaining[1024];
	int		i;

	// Check rcon validity
	if (!strlen (rcon_password->string) || strcmp (Cmd_Argv(1), rcon_password->string)) {
		Com_Printf (PRNT_ERROR, "Bad rcon from %s:\n%s\n", NET_AdrToString (sv_netFrom), sv_netMessage.data+4);

		Com_BeginRedirect (RD_PACKET, sv_outputBuffer, SV_OUTPUTBUF_LENGTH, SV_FlushRedirect);
		Com_Printf (0, "Bad rcon_password.\n");
		Com_EndRedirect ();

		return;
	}

	Com_Printf (0, "Rcon from %s:\n%s\n", NET_AdrToString (sv_netFrom), sv_netMessage.data+4);

	Com_BeginRedirect (RD_PACKET, sv_outputBuffer, SV_OUTPUTBUF_LENGTH, SV_FlushRedirect);

	// Echo the command
	remaining[0] = 0;
	for (i=2 ; i<Cmd_Argc () ; i++) {
		Q_strcatz (remaining, Cmd_Argv (i), sizeof(remaining));
		Q_strcatz (remaining, " ", sizeof(remaining));
	}
	Cmd_ExecuteString (remaining);

	Com_EndRedirect ();
}


/*
=================
SV_ConnectionlessPacket

A connectionless packet has four leading 0xff
characters to distinguish it from a game channel.
Clients that are in the game can still send
connectionless packets.
=================
*/
static void SV_ConnectionlessPacket ()
{
	char	*s;
	char	*c;

	// Should NEVER EVER happen
	if (sv_netMessage.curSize > 1024) {
		Com_Printf (PRNT_WARNING, "Illegitimate packet size (%i) received, ignored.\n", sv_netMessage.curSize);
		return;
	}

	sv_netMessage.BeginReading ();
	assert (sv_netMessage.ReadLong () == -1);		// skip the -1 marker

	s = sv_netMessage.ReadStringLine ();

	Cmd_TokenizeString (s, false);

	c = Cmd_Argv (0);
	Com_DevPrintf (0, "Packet %s : %s\n", NET_AdrToString (sv_netFrom), c);

	if (!strcmp (c, "ping"))
		SVC_Ping_f ();
	else if (!strcmp (c, "ack"))
		SVC_Ack_f ();
	else if (!strcmp (c, "status"))
		SVC_Status_f ();
	else if (!strcmp (c, "info"))
		SVC_Info_f ();
	else if (!strcmp (c, "getchallenge"))
		SVC_GetChallenge_f ();
	else if (!strcmp (c, "connect"))
		SVC_DirectConnect_f ();
	else if (!strcmp (c, "rcon"))
		SVC_RemoteCommand_f ();
	else
		Com_Printf (PRNT_WARNING, "SV_ConnectionlessPacket: Bad connectionless packet from %s:\n%s\n", NET_AdrToString (sv_netFrom), s);
}

/*
==============================================================================

	MASTER SERVERS

==============================================================================
*/

/*
================
SV_MasterHeartbeat

Send a message to the master every few minutes to
let it know we are alive, and log information
================
*/
#define HEARTBEAT_SECONDS	300
static void SV_MasterHeartbeat ()
{
	char		*string;
	int			i;

#ifndef DEDICATED_ONLY
	if (!dedicated->intVal)
		return;		// Only dedicated servers send heartbeats
#endif

	if (!public_server->intVal)
		return;		// A private dedicated game

	// Check for time wraparound
	if (svs.lastHeartBeat > svs.realTime)
		svs.lastHeartBeat = svs.realTime;

	if (svs.realTime-svs.lastHeartBeat < HEARTBEAT_SECONDS*1000)
		return;		// Not time to send yet

	svs.lastHeartBeat = svs.realTime;

	// Send the same string that we would give for a status OOB command
	string = SV_StatusString ();

	// Send to group master
	for (i=0 ; i<MAX_MASTERS ; i++) {
		if (!sv_masterAddresses[i].port)
			continue;

		Com_Printf (0, "Sending heartbeat to %s\n", NET_AdrToString (sv_masterAddresses[i]));
		Netchan_OutOfBandPrint (NS_SERVER, sv_masterAddresses[i], "heartbeat\n%s", string);
	}
}


/*
=================
SV_MasterShutdown

Informs all masters that this server is going down
=================
*/
static void SV_MasterShutdown ()
{
	int		i;

#ifndef DEDICATED_ONLY
	if (!dedicated->intVal)
		return;		// Only dedicated servers send heartbeats
#endif

	if (!public_server->intVal)
		return;		// A private dedicated game

	// Send to group master
	for (i=0 ; i<MAX_MASTERS ; i++) {
		if (!sv_masterAddresses[i].port)
			continue;

		if (i > 0)
			Com_Printf (0, "Sending heartbeat to %s\n", NET_AdrToString (sv_masterAddresses[i]));
		Netchan_OutOfBandPrint (NS_SERVER, sv_masterAddresses[i], "shutdown");
	}
}

/*
==============================================================================

	SERVER FRAME LOOP

==============================================================================
*/

/*
===================
SV_CalcPings

Updates the cl->ping variables
===================
*/
static void SV_CalcPings ()
{
	int			i, j;
	svClient_t	*cl;
	int			total, count;

	for (i=0 ; i<maxclients->intVal ; i++) {
		cl = &svs.clients[i];
		if (!cl->edict || !cl->edict->client || cl->state != SVCS_SPAWNED)
			continue;

		total = 0;
		count = 0;
		for (j=0 ; j<LATENCY_COUNTS ; j++) {
			if (cl->frameLatency[j] > 0) {
				count++;
				total += cl->frameLatency[j];
			}
		}

		if (!count)
			cl->ping = 0;
		else
			cl->ping = total / count;

		// Let the game dll know about the ping
		cl->edict->client->ping = cl->ping;
	}
}


/*
===================
SV_GiveMsec

Every few frames, gives all clients an allotment of milliseconds
for their command moves.  If they exceed it, assume cheating.
===================
*/
static void SV_GiveMsec ()
{
	int			i;
	svClient_t	*cl;

	if (sv.frameNum & 15)
		return;

	for (i=0 ; i<maxclients->intVal ; i++) {
		cl = &svs.clients[i];
		if (cl->state == SVCS_FREE)
			continue;
		
		cl->commandMsec = 1800;		// 1600 + some slop
	}
}


/*
=================
SV_ReadPackets
=================
*/
static void SV_ReadPackets ()
{
	int			i;
	svClient_t	*cl;
	int			qPort;

	while (NET_GetPacket (NS_SERVER, sv_netFrom, sv_netMessage)) {
		// Check for connectionless packet (0xffffffff) first
		if (*(int *)sv_netMessage.data == -1) {
			SV_ConnectionlessPacket ();
			continue;
		}

		/*
		** Read the qPort out of the message so we can fix up
		** stupid address translating routers
		*/
		sv_netMessage.BeginReading ();
		sv_netMessage.ReadLong ();		// Sequence number
		sv_netMessage.ReadLong ();		// Sequence number
		qPort = sv_netMessage.ReadShort () & 0xffff;

		// Check for packets from connected clients
		for (i=0, cl=svs.clients ; i<maxclients->intVal ; i++, cl++) {
			if (cl->state == SVCS_FREE)
				continue;
			if (!sv_netFrom.CompareBaseAdr(cl->netChan.remoteAddress))
				continue;
			if (cl->netChan.qPort != qPort)
				continue;
			if (cl->netChan.remoteAddress.port != sv_netFrom.port) {
				Com_Printf (0, "SV_ReadPackets: fixing up a translated port\n");
				cl->netChan.remoteAddress.port = sv_netFrom.port;
			}

			if (Netchan_Process (cl->netChan, sv_netMessage)) {
				// This is a valid, sequenced packet, so process it
				if (cl->state != SVCS_FREE) {
					cl->lastMessage = svs.realTime;	// Don't timeout
					SV_ExecuteClientMessage (cl);
				}
			}
			break;
		}
	}
}


/*
==================
SV_CheckTimeouts

If a packet has not been received from a client for timeout->floatVal
seconds, drop the conneciton.  Server frames are used instead of
realtime to avoid dropping the local client while debugging.

When a client is normally dropped, the svClient_t goes into a zombie state
for a few seconds to make sure any final reliable message gets resent
if necessary
==================
*/
static void SV_CheckTimeouts ()
{
	int			i;
	svClient_t	*cl;
	int			droppoint;
	int			zombiepoint;

	droppoint = svs.realTime - 1000*timeout->floatVal;
	zombiepoint = svs.realTime - 1000*zombietime->floatVal;

	for (i=0, cl=svs.clients ; i<maxclients->intVal ; i++, cl++) {
		// Message times may be wrong across a changelevel
		if (cl->lastMessage > svs.realTime)
			cl->lastMessage = svs.realTime;

		if (cl->state == SVCS_FREE && cl->lastMessage < zombiepoint) {
			cl->state = SVCS_FREE;	// Can now be reused
			continue;
		}

		if (cl->state >= SVCS_CONNECTED && cl->lastMessage < droppoint) {
			SV_BroadcastPrintf (PRINT_HIGH, "%s timed out\n", cl->name);
			SV_DropClient (cl); 
			cl->state = SVCS_FREE;	// Don't bother with zombie state
		}
	}
}


/*
================
SV_PrepWorldFrame

This has to be done before the world logic, because
player processing happens outside RunWorldFrame
================
*/
static void SV_PrepWorldFrame ()
{
	edict_t	*ent;
	int		i;

	for (i=0 ; i<ge->numEdicts ; i++, ent++) {
		ent = EDICT_NUM(i);
		// Events only last for a single message
		memset (&ent->s.events, 0, sizeof(ent->s.events));
	}
}


/*
=================
SV_RunGameFrame
=================
*/
static void SV_RunGameFrame ()
{
	/*
	** We always need to bump framenum, even if we don't run the world, otherwise the delta
	** compression can get confused when a client has the "current" frame
	*/
	sv.frameNum++;
	sv.time = sv.frameNum*ServerFrameTime;

	// Don't run if paused
	if (!sv_paused->intVal || maxclients->intVal > 1) {
		ge->RunFrame ();

		// Never get more than one tic behind
		if (sv.time < (uint32)svs.realTime) {
			if (sv_showclamp->intVal)
				Com_Printf (0, "sv highclamp\n");
			svs.realTime = sv.time;
		}
	}
}

void *SV_GetPhysicsWorld()
{
	return sv.physicsWorld;
}

/*
==================
SV_Frame
==================
*/

void SV_Frame (int msec)
{
	// If server is not active, do nothing
	if (!svs.initialized)
		return;

	svs.realTime += msec;

	// Keep the random time dependent
	rand ();

	// Update console title
	if (hostname->modified)
		SV_UpdateTitle ();

	// Check timeouts
	SV_CheckTimeouts ();

	// Get packets from clients
	SV_ReadPackets ();

	// Move autonomous things around if enough time has passed
	if (!sv_timedemo->intVal && (uint32)svs.realTime < sv.time) {
		// Never let the time get too far off
		if (sv.time - svs.realTime > ServerFrameTime) {
			if (sv_showclamp->intVal)
				Com_Printf (0, "sv lowclamp\n");
			svs.realTime = sv.time - ServerFrameTime;
		}
		NET_Server_Sleep (sv.time - svs.realTime);
		return;
	}

	// Update ping based on the last known frame from all clients
	SV_CalcPings ();

	// Give the clients some timeslices
	SV_GiveMsec ();

	// Let everything in the world think and move
	SV_RunGameFrame ();

	// Send messages back to the clients that had packets read this frame
	SV_SendClientMessages ();

	// Save the entire world state if recording a serverdemo
	SV_RecordDemoMessage ();

	// Send a heartbeat to the master if needed
	SV_MasterHeartbeat ();

	// Clear teleport flags, etc for next frame
	SV_PrepWorldFrame ();

}

//============================================================================

/*
===============
SV_ServerInit

Only called at egl.exe startup, not for each game
===============
*/
void SV_ServerInit ()
{
	sv_gameSysPool = Mem_CreatePool ("Server: Game system");
	sv_genericPool = Mem_CreatePool ("Server: Generic");

	SV_OperatorCommandInit	();

	Cvar_Register ("skill",			"1",											0);
	Cvar_Register ("deathmatch",	"0",											CVAR_SERVERINFO|CVAR_LATCH_SERVER);
	Cvar_Register ("coop",			"0",											CVAR_SERVERINFO|CVAR_LATCH_SERVER);
	Cvar_Register ("dmflags",		Q_VarArgs ("%i", DF_INSTANT_ITEMS),				CVAR_SERVERINFO);
	Cvar_Register ("fraglimit",		"0",											CVAR_SERVERINFO);
	Cvar_Register ("timelimit",		"0",											CVAR_SERVERINFO);
	Cvar_Register ("cheats",		"0",											CVAR_SERVERINFO|CVAR_LATCH_SERVER);
	Cvar_Register ("protocol",		Q_VarArgs ("%i", ENHANCED_PROTOCOL_VERSION),	CVAR_SERVERINFO|CVAR_READONLY);
	Cvar_Register ("mapname",		"",												CVAR_SERVERINFO|CVAR_READONLY);

	rcon_password			= Cvar_Register ("rcon_password",			"",			0);

	maxclients				= Cvar_Register ("maxclients",				"1",		CVAR_SERVERINFO|CVAR_LATCH_SERVER);
	hostname				= Cvar_Register ("hostname",				"noname",	CVAR_SERVERINFO|CVAR_ARCHIVE);
	timeout					= Cvar_Register ("timeout",					"125",		0);
	zombietime				= Cvar_Register ("zombietime",				"2",		0);

	sv_showclamp			= Cvar_Register ("showclamp",				"0",		0);
	sv_paused				= Cvar_Register ("paused",					"0",		CVAR_CHEAT);
	sv_timedemo				= Cvar_Register ("timedemo",				"0",		CVAR_CHEAT);

	sv_enforcetime			= Cvar_Register ("sv_enforcetime",			"0",		0);
	sv_reconnect_limit		= Cvar_Register ("sv_reconnect_limit",		"3",		CVAR_ARCHIVE);
	sv_noreload				= Cvar_Register ("sv_noreload",				"0",		0);
	sv_airaccelerate		= Cvar_Register ("sv_airaccelerate",		"0",		CVAR_LATCH_SERVER);

	allow_download			= Cvar_Register ("allow_download",			"1",		CVAR_ARCHIVE);
	allow_download_players	= Cvar_Register ("allow_download_players",	"0",		CVAR_ARCHIVE);
	allow_download_models	= Cvar_Register ("allow_download_models",	"1",		CVAR_ARCHIVE);
	allow_download_sounds	= Cvar_Register ("allow_download_sounds",	"1",		CVAR_ARCHIVE);
	allow_download_maps		= Cvar_Register ("allow_download_maps",		"1",		CVAR_ARCHIVE);

	public_server			= Cvar_Register ("public",					"0",		0);

	sv_netMessage.Init(sv_netBuffer, sizeof(sv_netBuffer));
}


/*
==================
SV_FinalMessage

Used by SV_ServerShutdown to send a final message to all connected clients before
the server goes down. The messages are sent immediately, not just stuck on
the outgoing message list, because the server is going to totally exit after
returning from this function.
==================
*/
static void SV_FinalMessage (char *message, bool reconnect)
{
	int			i;
	svClient_t	*cl;

	sv_netMessage.Clear();
	sv_netMessage.WriteByte (SVC_PRINT);
	sv_netMessage.WriteByte (PRINT_HIGH);
	sv_netMessage.WriteString (message);

	if (reconnect)
		sv_netMessage.WriteByte (SVC_RECONNECT);
	else
		sv_netMessage.WriteByte (SVC_DISCONNECT);

	// Send it twice
	// stagger the packets to crutch operating system limited buffers
	// FIXME: can this be done in one loop?
	for (i=0, cl=svs.clients ; i<maxclients->intVal ; i++, cl++) {
		if (cl->state >= SVCS_CONNECTED)
			Netchan_Transmit (cl->netChan, sv_netMessage.curSize, sv_netMessage.data);
	}

	for (i=0, cl=svs.clients ; i<maxclients->intVal ; i++, cl++) {
		if (cl->state >= SVCS_CONNECTED)
			Netchan_Transmit (cl->netChan, sv_netMessage.curSize, sv_netMessage.data);
	}
}


/*
================
SV_ServerShutdown

Called when each game quits, before Sys_Quit or Sys_Error
================
*/
void SV_ServerShutdown (char *finalMessage, bool reconnect, bool crashing)
{
	if (svs.clients)
		SV_FinalMessage (finalMessage, reconnect);

	SV_MasterShutdown ();

	if (!crashing) {
		SV_GameAPI_Shutdown ();

		// Get latched vars
		Cvar_GetLatchedVars (CVAR_LATCH_SERVER);
	}

	// Free current level
	if (sv.demoFile) {
		FS_CloseFile (sv.demoFile);
		sv.demoFile = 0;
	}
	sv.Clear();
	Com_SetServerState (SS_DEAD);

	// Free server static data
	if (svs.clients)
		Mem_Free (svs.clients);
	if (svs.clientEntities)
		Mem_Free (svs.clientEntities);
	if (svs.demoFile)
		FS_CloseFile (svs.demoFile);
	svs.Clear();

	// If the server is crashing there's no sense in releasing this memory
	// (it may even be the cause! *gasp*)
	if (!crashing)
		CM_UnloadMap ();

	sv_netMessage.Clear();
}
