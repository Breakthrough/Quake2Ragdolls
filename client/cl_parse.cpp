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
// cl_parse.c
// Parses messages received from the server
//

#include "cl_local.h"

char *cl_svcStrings[] = {
	"SVC_BAD",

	"SVC_MUZZLEFLASH2",
	"SVC_TEMP_ENTITY",
	"SVC_LAYOUT",
	"SVC_INVENTORY",

	"SVC_NOP",
	"SVC_DISCONNECT",
	"SVC_RECONNECT",
	"SVC_SOUND",
	"SVC_PRINT",
	"SVC_STUFFTEXT",
	"SVC_SERVERDATA",
	"SVC_CONFIGSTRING",
	"SVC_SPAWNBASELINE",	
	"SVC_CENTERPRINT",
	"SVC_DOWNLOAD",
	"SVC_PLAYERINFO",
	"SVC_PACKETENTITIES",
	"SVC_DELTAPACKETENTITIES",
	"SVC_FRAME",

	"SVC_ZPACKET",			// new for ENHANCED_PROTOCOL_VERSION
	"SVC_ZDOWNLOAD"			// new for ENHANCED_PROTOCOL_VERSION
};

/*
=================
CL_ShowSVCString
=================
*/
static void CL_ShowSVCString (char *message)
{
	if (cl_shownet->intVal < 2)
		return;

	Com_Printf (0, "%3i:%s\n", cls.netMessage.readCount-1, message);
}


/*
=================
CL_ParseEntityBits

Returns the entity number and the header bits
=================
*/

struct entityBits_t
{
	int				number;
	entityType_t	type;
};

static entityBits_t CL_ParseEntityBits (uint32 *bits)
{
	uint32		b, total;
	entityBits_t entityBits;

	total = cls.netMessage.ReadByte ();
	if (total & U_MOREBITS1) {
		b = cls.netMessage.ReadByte ();
		total |= b<<8;
	}
	if (total & U_MOREBITS2) {
		b = cls.netMessage.ReadByte ();
		total |= b<<16;
	}
	if (total & U_MOREBITS3) {
		b = cls.netMessage.ReadByte ();
		total |= b<<24;
	}

	if (total & U_NUMBER16)
		entityBits.number = cls.netMessage.ReadShort () % MAX_REF_ENTITIES;
	else
		entityBits.number = cls.netMessage.ReadByte ();

	if (entityBits.number != 0)
		entityBits.type = cls.netMessage.ReadByte();

	*bits = total;
	return entityBits;
}


/*
==================
CL_ParseDelta

Can go from either a baseline or a previous packet_entity
==================
*/
static void CL_ParseDelta (entityState_t *from, entityState_t *to, entityBits_t &entBits, int bits)
{
	// Set everything to the state we are delta'ing from
	*to = *from;

	if (cls.serverProtocol != ENHANCED_PROTOCOL_VERSION)
		Vec3Copy (from->origin, to->oldOrigin);
	else if (!(bits & U_OLDORIGIN) && !(from->renderFx & RF_BEAM))
		Vec3Copy (from->origin, to->oldOrigin);

	to->number = entBits.number;
	to->type = entBits.type;
	
	if (bits & U_MODEL)
		to->modelIndex = cls.netMessage.ReadByte ();
	if (bits & U_MODEL2)
		to->modelIndex2 = cls.netMessage.ReadByte ();
	if (bits & U_MODEL3)
		to->modelIndex3 = cls.netMessage.ReadByte ();
	if (bits & U_COLOR)
	{
		var col = cls.netMessage.ReadLong ();
		to->color = col;
	}

	if (bits & U_ANIM)
		to->animation = cls.netMessage.ReadChar ();

	if ((bits & U_SKIN8) && (bits & U_SKIN16))
		to->skinNum = cls.netMessage.ReadLong (); // used for laser colors
	else if (bits & U_SKIN8)
		to->skinNum = cls.netMessage.ReadByte ();
	else if (bits & U_SKIN16)
		to->skinNum = cls.netMessage.ReadShort ();

	if ((bits & (U_EFFECTS8|U_EFFECTS16)) == (U_EFFECTS8|U_EFFECTS16))
		to->effects = cls.netMessage.ReadLong ();
	else if (bits & U_EFFECTS8)
		to->effects = cls.netMessage.ReadByte ();
	else if (bits & U_EFFECTS16)
		to->effects = cls.netMessage.ReadShort ();

	if ((bits & (U_RENDERFX8|U_RENDERFX16)) == (U_RENDERFX8|U_RENDERFX16))
		to->renderFx = cls.netMessage.ReadLong ();
	else if (bits & U_RENDERFX8)
		to->renderFx = cls.netMessage.ReadByte ();
	else if (bits & U_RENDERFX16)
		to->renderFx = cls.netMessage.ReadShort ();

	if (bits & U_ORIGIN1)
		to->origin[0] = cls.netMessage.ReadCoord ();
	if (bits & U_ORIGIN2)
		to->origin[1] = cls.netMessage.ReadCoord ();
	if (bits & U_ORIGIN3)
		to->origin[2] = cls.netMessage.ReadCoord ();
		
	if (to->type & ET_MISSILE)
	{
		if (bits & U_ANGLE1)
			to->angles[0] = cls.netMessage.ReadShort() * (1.0f/32);
		if (bits & U_ANGLE2)
			to->angles[1] = cls.netMessage.ReadShort() * (1.0f/32);
		if (bits & U_ANGLE3)
			to->angles[2] = cls.netMessage.ReadShort() * (1.0f/32);
	}
	else
	{
		if (bits & U_ANGLE1)
			to->angles[0] = cls.netMessage.ReadAngle16 ();
		if (bits & U_ANGLE2)
			to->angles[1] = cls.netMessage.ReadAngle16 ();
		if (bits & U_ANGLE3)
			to->angles[2] = cls.netMessage.ReadAngle16 ();
	}

	if (bits & U_QUAT)
	{
		int quatBits = cls.netMessage.ReadByte();

		if (quatBits)
			for (int i = 0; i < 4; ++i)
			{
				if (quatBits & BIT(i))
					to->quat[i] = cls.netMessage.ReadAngle16_Compress();
			}
	}

	if (bits & U_OLDORIGIN)
		cls.netMessage.ReadPos (to->oldOrigin);

	if (bits & U_SOUND)
		to->sound = cls.netMessage.ReadByte ();

	if ( bits & U_EVENT1 ) {
		int event;

		event = cls.netMessage.ReadByte ();

		to->events[0].ID = event;
		to->events[0].parms.Count() = cls.netMessage.ReadByte();

		if (to->events[0].parms.Count())
			cls.netMessage.ReadData(to->events[0].parms.Pointer(), to->events[0].parms.Count());

	} else {
		to->events[0].ID = 0;
		to->events[0].parms.Count() = 0;
	}

	if ( bits & U_EVENT2 ) {
		int event;

		event = cls.netMessage.ReadByte ();

		to->events[1].ID = event;
		to->events[1].parms.Count() = cls.netMessage.ReadByte();

		if (to->events[1].parms.Count())
			cls.netMessage.ReadData(to->events[1].parms.Pointer(), to->events[1].parms.Count());

	} else {
		to->events[1].ID = 0;
		to->events[1].parms.Count() = 0;
	}

	if (bits & U_SOLID)
	{
		if (cls.protocolMinorVersion >= MINOR_VERSION_R1Q2_32BIT_SOLID)
			to->solid = cls.netMessage.ReadLong ();
		else
			to->solid = cls.netMessage.ReadShort ();
	}
}

/*
=========================================================================

	FRAME PARSING

=========================================================================
*/

/*
===================
CL_ParsePlayerstate
===================
*/
static void CL_ParsePlayerstate (const netFrame_t *oldFrame, netFrame_t *newFrame, int extraFlags)
{
	playerState_t	*state;
	int					i, statBits;
	int					flags;
	bool				enhanced;

	enhanced = (cls.serverProtocol == ENHANCED_PROTOCOL_VERSION);

	state = &newFrame->playerState;

	// clear to old value before delta parsing
	if (oldFrame)
		*state = oldFrame->playerState;
	else
		memset (state, 0, sizeof(*state));

	flags = cls.netMessage.ReadShort ();

	//
	// parse the pMoveState_t
	//
	if (flags & PS_M_TYPE)
		state->pMove.pmType = cls.netMessage.ReadByte ();

	// protocol changes
	if (flags & PS_M_ORIGIN) {
		if (!enhanced)
			extraFlags |= EPS_PMOVE_ORIGIN2;
		state->pMove.origin[0] = cls.netMessage.ReadShort ();
		state->pMove.origin[1] = cls.netMessage.ReadShort ();
	}
	if (extraFlags & EPS_PMOVE_ORIGIN2)
		state->pMove.origin[2] = cls.netMessage.ReadShort ();

	if (flags & PS_M_VELOCITY) {
		if (!enhanced)
			extraFlags |= EPS_PMOVE_VELOCITY2;
		state->pMove.velocity[0] = cls.netMessage.ReadShort ();
		state->pMove.velocity[1] = cls.netMessage.ReadShort ();
	}
	if (extraFlags & EPS_PMOVE_VELOCITY2)
		state->pMove.velocity[2] = cls.netMessage.ReadShort ();
	// protocol changes

	if (flags & PS_M_TIME)
		state->pMove.pmTime = cls.netMessage.ReadByte ();

	if (flags & PS_M_FLAGS)
		state->pMove.pmFlags = cls.netMessage.ReadByte ();

	if (flags & PS_M_GRAVITY)
		state->pMove.gravity = cls.netMessage.ReadShort ();

	if (flags & PS_M_DELTA_ANGLES)
	{
		state->pMove.deltaAngles[0] = cls.netMessage.ReadShort ();
		state->pMove.deltaAngles[1] = cls.netMessage.ReadShort ();
		state->pMove.deltaAngles[2] = cls.netMessage.ReadShort ();
	}

	//
	// parse the rest of the playerState_t
	//
	if (flags & PS_VIEWOFFSET) {
		state->viewOffset[0] = cls.netMessage.ReadChar () * 0.25f;
		state->viewOffset[1] = cls.netMessage.ReadChar () * 0.25f;
		state->viewOffset[2] = cls.netMessage.ReadChar () * 0.25f;
	}

	// protocol changes
	if (flags & PS_VIEWANGLES) {
		if (!enhanced)
			extraFlags |= EPS_VIEWANGLE2;
		state->viewAngles[0] = cls.netMessage.ReadAngle16 ();
		state->viewAngles[1] = cls.netMessage.ReadAngle16 ();
	}
	if (extraFlags & EPS_VIEWANGLE2)
		state->viewAngles[2] = cls.netMessage.ReadAngle16 ();
	// protocol changes

	if (flags & PS_KICKANGLES) {
		state->kickAngles[0] = cls.netMessage.ReadChar () * 0.25f;
		state->kickAngles[1] = cls.netMessage.ReadChar () * 0.25f;
		state->kickAngles[2] = cls.netMessage.ReadChar () * 0.25f;
	}

	// protocol changes
	if (flags & PS_WEAPONFRAME)
		state->gunAnim = cls.netMessage.ReadChar ();

	if (flags & PS_BLEND) {
		state->viewBlend[0] = cls.netMessage.ReadByte () * (1.0f/255.0f);
		state->viewBlend[1] = cls.netMessage.ReadByte () * (1.0f/255.0f);
		state->viewBlend[2] = cls.netMessage.ReadByte () * (1.0f/255.0f);
		state->viewBlend[3] = cls.netMessage.ReadByte () * (1.0f/255.0f);
	}
	else
		state->viewBlend[0] = state->viewBlend[1] = state->viewBlend[2] = state->viewBlend[3] = 0;

	if (flags & PS_FOV)
		state->fov = cls.netMessage.ReadByte ();

	if (flags & PS_RDFLAGS)
		state->rdFlags = cls.netMessage.ReadByte ();

	// parse stats
	if (!enhanced)
		extraFlags |= EPS_STATS;
	if (extraFlags & EPS_STATS)
	{
		statBits = cls.netMessage.ReadLong ();
		
		if (statBits)
		{
			for (i=0 ; i<MAX_STATS ; i++)
				if (statBits & BIT(i))
					state->stats[i] = cls.netMessage.ReadShort ();
		}
	}
}


/*
==================
CL_ParsePacketEntities

An SVC_PACKETENTITIES has just been parsed, deal with the rest of the data stream.
==================
*/
static void CL_DeltaEntity (netFrame_t *newFrame, entityBits_t &entBits, entityState_t *old, int bits)
{
	// Parses deltas from the given base, and adds the resulting entity to the current frame
	entityState_t	*state;

	state = &cl_parseEntities[cl.parseEntities & (MAX_PARSEENTITIES_MASK)];
	cl.parseEntities++;
	newFrame->numEntities++;

	CL_ParseDelta (old, state, entBits, bits);
	CL_CGModule_NewPacketEntityState (entBits.number, *state);
}

static void CL_ParsePacketEntities (const netFrame_t *oldFrame, netFrame_t *newFrame)
{
	uint32			bits;
	entityState_t	*oldState = NULL;
	int				oldIndex;
	entityBits_t	oldBits = { 99999, 0};

	newFrame->parseEntities = cl.parseEntities;
	newFrame->numEntities = 0;

	CL_CGModule_BeginFrameSequence ();

	// Delta from the entities present in oldFrame
	oldIndex = 0;
	if (oldFrame) {
		if (oldIndex < oldFrame->numEntities) {
			oldState = &cl_parseEntities[(oldFrame->parseEntities+oldIndex) & (MAX_PARSEENTITIES_MASK)];
			oldBits.number = oldState->number;
			oldBits.type = oldState->type;
		}
	}

	for ( ; ; ) {
		var newBits = CL_ParseEntityBits (&bits);

		if (newBits.number >= MAX_CS_EDICTS)
			Com_Error (ERR_DROP, "CL_ParsePacketEntities: bad number:%i", newBits.number);

		if (cls.netMessage.readCount > cls.netMessage.curSize)
			Com_Error (ERR_DROP, "CL_ParsePacketEntities: end of message");

		if (!newBits.number)
			break;

		while (oldBits.number < newBits.number) {
			// One or more entities from the old packet are unchanged
			if (cl_shownet->intVal == 3)
				Com_Printf (0, "   unchanged: %i\n", oldBits.number);
			CL_DeltaEntity (newFrame, oldBits, oldState, 0);
			
			oldIndex++;

			if (oldIndex >= oldFrame->numEntities)
			{
				oldBits.number = 99999;
				oldBits.type = 0;
			}
			else {
				oldState = &cl_parseEntities[(oldFrame->parseEntities+oldIndex) & (MAX_PARSEENTITIES_MASK)];
				oldBits.number = oldState->number;
				oldBits.type = oldState->type;
			}
		}

		if (bits & U_REMOVE) {
			// The entity present in oldFrame is not in the current frame
			if (cl_shownet->intVal == 3)
				Com_Printf (0, "   remove: %i\n", newBits.number);
			if (oldBits.number != newBits.number)
				Com_DevPrintf (PRNT_WARNING, "U_REMOVE: oldNum != newNum\n");

			oldIndex++;

			if (oldIndex >= oldFrame->numEntities)
			{
				oldBits.number = 99999;
				oldBits.type = 0;
			}
			else {
				oldState = &cl_parseEntities[(oldFrame->parseEntities+oldIndex) & (MAX_PARSEENTITIES_MASK)];
				oldBits.number = oldState->number;
				oldBits.type = oldState->type;
			}
			continue;
		}

		if (oldBits.number == newBits.number) {
			// Delta from previous state
			if (cl_shownet->intVal == 3)
				Com_Printf (0, "   delta: %i\n", newBits.number);
			CL_DeltaEntity (newFrame, newBits, oldState, bits);

			oldIndex++;

			if (oldIndex >= oldFrame->numEntities)
			{
				oldBits.number = 99999;
				oldBits.type = 0;
			}
			else {
				oldState = &cl_parseEntities[(oldFrame->parseEntities+oldIndex) & (MAX_PARSEENTITIES_MASK)];
				oldBits.number = oldState->number;
				oldBits.type = oldState->type;
			}
			continue;
		}

		if (oldBits.number > newBits.number) {
			// Delta from baseline
			if (cl_shownet->intVal == 3)
				Com_Printf (0, "   baseline: %i\n", newBits.number);
			CL_DeltaEntity (newFrame, newBits, &cl_baseLines[newBits.number], bits);
			continue;
		}

	}

	// Any remaining entities in the old frame are copied over
	while (oldBits.number != 99999) {
		// One or more entities from the old packet are unchanged
		if (cl_shownet->intVal == 3)
			Com_Printf (0, "   unchanged: %i\n", oldBits.number);
		CL_DeltaEntity (newFrame, oldBits, oldState, 0);
		
		oldIndex++;

		if (oldIndex >= oldFrame->numEntities)
		{
			oldBits.number = 99999;
			oldBits.type = 0;
		}
		else {
			oldState = &cl_parseEntities[(oldFrame->parseEntities+oldIndex) & MAX_PARSEENTITIES_MASK];
			oldBits.number = oldState->number;
			oldBits.type = oldState->type;
		}
	}

	CL_CGModule_EndFrameSequence ();
}


/*
================
CL_ParseFrame
================
*/
static void CL_ParseFrame (int extraBits)
{
	int			cmd, len;
	uint32		serverFrame;
	int			extraFlags;
	netFrame_t	*oldFrame;

	memset (&cl.frame, 0, sizeof(cl.frame));

	// Protocol updates
	serverFrame = cls.netMessage.ReadLong ();
	if (cls.serverProtocol != ENHANCED_PROTOCOL_VERSION) {
		cl.frame.serverFrame = serverFrame;
		cl.frame.deltaFrame = cls.netMessage.ReadLong ();
	}
	else {
		uint32	offset;

		offset = serverFrame & 0xF8000000;
		offset >>= 27;

		serverFrame &= 0x07FFFFFF;
		cl.frame.serverFrame = serverFrame;

		if (offset == 31)
			cl.frame.deltaFrame = -1;
		else
			cl.frame.deltaFrame = serverFrame - offset;
	}

	if (Com_ClientState() != CA_ACTIVE)
		cl.frame.serverTime = 0;
	else
		cl.frame.serverTime = cl.frame.serverFrame * ServerFrameTime;

	// HACK UGLY SHIT
	// moving the extrabits from cmd over so that the 4 that come from extraflags (surpressCount) don't conflict
	extraFlags = extraBits >> 1;

	// BIG HACK to let old demos continue to work
	if (cls.serverProtocol != 26) {
		byte		data;

		data = cls.netMessage.ReadByte ();

		//r1: HACK to get extra 4 bits of otherwise unused data
		if (cls.serverProtocol == ENHANCED_PROTOCOL_VERSION) {
			cl.surpressCount = (data & 0x0F);
			extraFlags |= (data & 0xF0) >> 4;
		}
		else {
			cl.surpressCount = data;
		}
	}

	if (cl_shownet->intVal == 3)
		Com_Printf (0, "   frame:%i  delta:%i\n", cl.frame.serverFrame, cl.frame.deltaFrame);

	// If the frame is delta compressed from data that we no longer have available, we must
	// suck up the rest of the frame, but not use it, then ask for a non-compressed message
	if (cl.frame.deltaFrame <= 0) {
		cl.frame.valid = true;		// uncompressed frame
		cls.demoWaiting = false;	// we can start recording now
		oldFrame = NULL;
	}
	else {
		oldFrame = &cl.frames[cl.frame.deltaFrame & UPDATE_MASK];
		if (!oldFrame->valid) {
			// Should never happen
			Com_Printf (PRNT_ERROR, "Delta from invalid frame (not supposed to happen!).\n");
		}
		if (oldFrame->serverFrame != cl.frame.deltaFrame) {
			// The frame that the server did the delta from
			// is too old, so we can't reconstruct it properly
			Com_DevPrintf (PRNT_WARNING, "Delta frame too old.\n");
		}
		else if (cl.parseEntities-oldFrame->parseEntities > MAX_PARSE_ENTITIES-128) {
			Com_DevPrintf (PRNT_WARNING, "Delta parseEntities too old.\n");
		}
		else
			cl.frame.valid = true;	// Valid delta parse
	}

	// Read areaBits
	len = cls.netMessage.ReadByte ();
	cls.netMessage.ReadData (&cl.frame.areaBits, len);

	// Read playerinfo
	if (cls.serverProtocol != ENHANCED_PROTOCOL_VERSION) {
		cmd = cls.netMessage.ReadByte ();
		CL_ShowSVCString (cl_svcStrings[cmd]);
		if (cmd != SVC_PLAYERINFO)
			Com_Error (ERR_DROP, "CL_ParseFrame: not playerinfo");
	}
	CL_ParsePlayerstate (oldFrame, &cl.frame, extraFlags);

	// Read packet entities
	if (cls.serverProtocol != ENHANCED_PROTOCOL_VERSION) {
		cmd = cls.netMessage.ReadByte ();
		CL_ShowSVCString (cl_svcStrings[cmd]);
		if (cmd != SVC_PACKETENTITIES)
			Com_Error (ERR_DROP, "CL_ParseFrame: not packetentities");
	}
	CL_ParsePacketEntities (oldFrame, &cl.frame);

	// Translate for demos
	if (cls.demoRecording && cls.serverProtocol != ORIGINAL_PROTOCOL_VERSION) {
		netMsg_t	fakeMsg;
		byte		fakeFrame[1300];

		// Start
		fakeMsg.Init(fakeFrame, sizeof(fakeFrame));
		fakeMsg.allowOverflow = true;

		// SVC_FRAME header
		fakeMsg.WriteByte (SVC_FRAME);
		fakeMsg.WriteLong (cl.frame.serverFrame);
		fakeMsg.WriteLong (cl.frame.deltaFrame);
		fakeMsg.WriteByte (cl.surpressCount);

		// areaBits
		fakeMsg.WriteByte (len);
		fakeMsg.WriteRaw (&cl.frame.areaBits, len);

		// Delta playerState
		CL_WriteDemoPlayerstate (oldFrame, &cl.frame, &fakeMsg);

		// Delta packet entities
		CL_WriteDemoPacketEntities (oldFrame, &cl.frame, &fakeMsg);

		if (!fakeMsg.overFlowed) {
			if (fakeMsg.curSize+cl.demoBuffer.curSize > cl.demoBuffer.maxSize)
				Com_DevPrintf (0, "Discarded a demo frame of %d bytes.\n", fakeMsg.curSize);
			else
				cl.demoBuffer.WriteRaw (fakeFrame, fakeMsg.curSize);
		}
	}

	// Save the frame off in the backup array for later delta comparisons
	cl.frames[cl.frame.serverFrame & UPDATE_MASK] = cl.frame;
	if (cl.frame.valid) {
		// Getting a valid frame message ends the connection process
		if (Com_ClientState () != CA_ACTIVE)
			CL_SetState (CA_ACTIVE);
		cls.soundPrepped = true;	// can start mixing ambient sounds
	}
}

/*
=====================================================================

	SERVER CONNECTING MESSAGES

=====================================================================
*/

/*
==================
CL_ParseServerData
==================
*/
static bool CL_ParseServerData ()
{
	extern	cVar_t *fs_game;
	int		i, newVersion;
	char	*str;

	Com_DevPrintf (0, "Serverdata packet received.\n");

	// Wipe the clientState_t struct
	CL_ClearState ();
	CL_SetState (CA_CONNECTED);

	// Parse protocol version number
	i = cls.netMessage.ReadLong ();
	cls.serverProtocol = i;

	cl.serverCount = cls.netMessage.ReadLong () ? true : false;
	cl.attractLoop = cls.netMessage.ReadByte () ? true : false;

	// BIG HACK to let demos from release work with the 3.0x patch!!!
	if (i != ORIGINAL_PROTOCOL_VERSION && i != ENHANCED_PROTOCOL_VERSION && i != 26 && !cl.attractLoop)
		Com_Error (ERR_DROP, "Server is using unknown protocol %i.", i);

	// Game directory
	str = cls.netMessage.ReadString ();
	Q_strncpyz (cl.gameDir, str, sizeof(cl.gameDir));

	// Set gamedir
	if ((*str && (!fs_game->string || !*fs_game->string || strcmp (fs_game->string, str))) || (!*str && (fs_game->string || *fs_game->string)))
		Cvar_Set ("game", str, false);

	// Parse player entity number
	cl.playerNum = cls.netMessage.ReadShort ();

	// Get the full level name
	str = cls.netMessage.ReadString ();

	// Check the protocol version
	if (cls.serverProtocol == ENHANCED_PROTOCOL_VERSION)
	{
		cl.enhancedServer = cls.netMessage.ReadByte ();
		newVersion = cls.netMessage.ReadShort ();
		if (newVersion != ENHANCED_COMPATIBILITY_NUMBER)
		{
			if (cl.attractLoop)
			{
				if (newVersion < ENHANCED_COMPATIBILITY_NUMBER)
					Com_Printf(0, "This demo was recorded with an earlier version of the R1Q2 protocol. It may not play back properly.\n");
				else
					Com_Printf(0, "This demo was recorded with a later version of the R1Q2 protocol. It may not play back properly. Please update your R1Q2 client.\n");
			}
			else
			{
				if (newVersion > ENHANCED_COMPATIBILITY_NUMBER)
					Com_Printf(0, "Server reports a higher R1Q2 protocol number than your client supports. Some features will be unavailable until you update your EGL client.\n");
				else
					Com_Printf(0, "Server reports a lower R1Q2 protocol number. The server admin needs to update their server!\n");
			}

			// Cap if server is above us just to be safe
			if (newVersion > ENHANCED_COMPATIBILITY_NUMBER)
				newVersion = ENHANCED_COMPATIBILITY_NUMBER;
		}

		if (newVersion >= MINOR_VERSION_R1Q2_BASE)
		{
			/*cl.advancedDeltas = */cls.netMessage.ReadByte ();
			cl.strafeHack = cls.netMessage.ReadByte () ? true : false;
		}
		else
		{
			cl.strafeHack = false;
		}

		cls.protocolMinorVersion = newVersion;
	}
	else
	{
		cl.enhancedServer = 0;
		cls.protocolMinorVersion = 0;
		cl.strafeHack = false;
	}

	Com_DevPrintf (0, "Server data: protocol=%d(%d), serverCount=%d, attractLoop=%d, playerNum=%d, game=%s, map=%s, enhanced=%d\n",
		cls.serverProtocol, cls.protocolMinorVersion, cl.serverCount, cl.attractLoop, cl.playerNum, cl.gameDir, str, cl.enhancedServer);

	CL_MediaRestart ();

	if (cl.playerNum == -1) {
		// Playing a cinematic or showing a pic, not a level
		CIN_PlayCinematic (str);
	}
	else {
		// Separate the printfs so the server message can have a color
		Com_Printf (0, "\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");
		Com_Printf (0, "%c%s\n", 2, str);

		// Need to prep refresh at next opportunity
		cls.refreshPrepped = false;
	}

	return true;
}


/*
==================
CL_ParseBaseline
==================
*/
static void CL_ParseBaseline ()
{
	entityState_t	*es;
	entityState_t	nullState;
	uint32			bits;

	var newBits = CL_ParseEntityBits (&bits);
	es = &cl_baseLines[newBits.number];
	CL_ParseDelta (&nullState, es, newBits, bits);
}


/*
================
CL_ParseConfigString
================
*/
static void CL_ParseConfigString ()
{
	int		num;
	char	*str;

	num = cls.netMessage.ReadShort ();
	if (num < 0 || num >= MAX_CFGSTRINGS)
		Com_Error (ERR_DROP, "CL_ParseConfigString: bad num");
	str = cls.netMessage.ReadString ();

	strcpy (cl.configStrings[num], str);

	// We do need to know some of these here...
	switch (num)
	{
	case CS_MAXCLIENTS:
		cl.maxClients = atoi (cl.configStrings[CS_MAXCLIENTS]);
		break;

	default:
		if (num >= CS_SOUNDS && num < CS_SOUNDS+MAX_CS_SOUNDS)
			cl.soundCfgStrings[num-CS_SOUNDS] = Snd_RegisterSound (cl.configStrings[num]);
		break;
	}

	// Let CGame know
	CL_CGModule_ParseConfigString (num, str);
}

/*
=====================================================================

	SERVER MESSAGES

=====================================================================
*/

/*
==================
CL_ParseStartSoundPacket
==================
*/
static void CL_ParseStartSoundPacket ()
{
	vec3_t			pos_vec;
	int				entNum, soundNum, flags;
	float			*pos, volume, attenuation, timeOffset;
	short			entChannel;

	flags = cls.netMessage.ReadByte ();
	soundNum = cls.netMessage.ReadByte ();

	// Volume
	if (flags & SND_VOLUME)
		volume = cls.netMessage.ReadByte () * (1.0f/255.0f);
	else
		volume = DEFAULT_SOUND_PACKET_VOLUME;

	// Attenuation
	if (flags & SND_ATTENUATION)
		attenuation = cls.netMessage.ReadByte () * (1.0f/64.0f);
	else
		attenuation = DEFAULT_SOUND_PACKET_ATTENUATION;

	// Offset
	if (flags & SND_OFFSET)
		timeOffset = cls.netMessage.ReadByte () * (1.0f/1000.0f);
	else
		timeOffset = 0;

	// Entity sound
	if (flags & SND_ENT) {
		// Entity reletive
		entChannel = cls.netMessage.ReadShort (); 
		entNum = entChannel >> 3;
		if (entNum > MAX_CS_EDICTS)
			Com_Error (ERR_DROP, "CL_ParseStartSoundPacket: entNum = %i", entNum);

		entChannel &= 7;
	}
	else {
		entNum = 0;
		entChannel = CHAN_AUTO;
	}

	// Specified position
	if (flags & SND_POS) {
		// Positioned in space
		cls.netMessage.ReadPos (pos_vec);
 
		pos = pos_vec;
	}
	else {
		// Use entity number
		pos = NULL;
	}

	// Play the sound
	if (!cl.soundCfgStrings[soundNum] && cl.configStrings[CS_SOUNDS+soundNum][0])
		cl.soundCfgStrings[soundNum] = Snd_RegisterSound (cl.configStrings[CS_SOUNDS+soundNum]);

	CL_CGModule_StartSound (pos, entNum, (EEntSndChannel)entChannel, soundNum, volume, attenuation, timeOffset);
}


/*
=====================
CL_ParseZPacket
=====================
*/
void CL_ParseZPacket ()
{
	byte		*buff_in;
	byte		*buff_out;
	netMsg_t	sb, old;
	sint16		compressedLen;
	sint16		uncompressedLen;

	compressedLen = cls.netMessage.ReadShort ();
	uncompressedLen = cls.netMessage.ReadShort ();

	//if (cls.serverProtocol != ENHANCED_PROTOCOL_VERSION)
	//	Com_Error (ERR_DROP, "CL_ParseZPacket: SVC_ZPACKET -requires- ENHANCED_PROTOCOL_VERSION");

	if (uncompressedLen <= 0)
		Com_Error (ERR_DROP, "CL_ParseZPacket: uncompressedLen <= 0");
	if (compressedLen <= 0)
		Com_Error (ERR_DROP, "CL_ParseZPacket: compressedLen <= 0");

	buff_in = (byte*)Mem_Alloc (compressedLen);
	buff_out = (byte*)Mem_Alloc (uncompressedLen);

	cls.netMessage.ReadData (buff_in, compressedLen);

	sb.Init(buff_out, uncompressedLen);
	sb.curSize = FS_ZLibDecompress (buff_in, compressedLen, buff_out, uncompressedLen, -15);

	old = cls.netMessage;
	cls.netMessage = sb;

	CL_ParseServerMessage ();

	cls.netMessage = old;

	Mem_Free (buff_in);
	Mem_Free (buff_out);

	Com_DevPrintf (0, "Got a ZPacket, %d->%d\n", uncompressedLen + 4, compressedLen);
}


/*
=====================
CL_ParseServerMessage
=====================
*/
void CL_ParseServerMessage ()
{
	int			cmd, i;
	char		*s, timestamp[10];
	time_t		ctime;
	struct tm	*ltime;
	static int	queryLastTime = 0;
	static int	lastCmd = -2;
	int			extraBits;
	int			oldReadCount;

	// If recording demos, copy the message out
	if (cl_shownet->intVal == 1)
		Com_Printf (0, "%i ", cls.netMessage.curSize);
	else if (cl_shownet->intVal >= 2)
		Com_Printf (0, "------------------\n");

	CL_CGModule_StartServerMessage ();

	// Parse the message
	for ( ; ; ) {
		if (cls.netMessage.readCount > cls.netMessage.curSize) {
			Com_Error (ERR_DROP, "CL_ParseServerMessage: Bad server message");
			break;
		}

		oldReadCount = cls.netMessage.readCount;
		cmd = cls.netMessage.ReadByte ();
		if (cmd == -1) {
			CL_ShowSVCString ("END OF MESSAGE");
			break;
		}

		//r1: more hacky bit stealing in the name of bandwidth
		extraBits = cmd & 0xE0;
		cmd &= 0x1F;
		CL_ShowSVCString (cl_svcStrings[cmd]);

		//
		// These are private to the client and server
		//
		switch (cmd) {
		case SVC_NOP:
			break;

		case SVC_DISCONNECT:
			cls.connectCount = 0;
			CL_WriteDemoMessageChunk (cls.netMessage.data + oldReadCount, cls.netMessage.readCount - oldReadCount, false);
			Com_Error (ERR_DISCONNECT, "Server disconnected\n");
			break;

		case SVC_RECONNECT:
			Com_Printf (0, "Server disconnected, reconnecting\n");
			if (cls.download.file) {
				// ZOID, close download
				fclose (cls.download.file);
				cls.download.file = NULL;
			}
			CL_SetState (CA_CONNECTING);
			cls.connectTime = -99999;	// CL_CheckForResend () will fire immediately
			cls.connectCount = 0;
			break;

		case SVC_SOUND:
			CL_ParseStartSoundPacket ();
			CL_WriteDemoMessageChunk (cls.netMessage.data + oldReadCount, cls.netMessage.readCount - oldReadCount, false);
			break;

		case SVC_PRINT:
			i = cls.netMessage.ReadByte ();
			s = cls.netMessage.ReadString ();
			CL_WriteDemoMessageChunk (cls.netMessage.data + oldReadCount, cls.netMessage.readCount - oldReadCount, false);

			if (cl_timestamp->intVal) {
				ctime = time (NULL);
				ltime = localtime (&ctime);
				strftime (timestamp, sizeof(timestamp), "%I:%M %p", ltime);
			}

			switch (i) {
			case PRINT_CHAT:
				Snd_StartLocalSound (clMedia.talkSfx, 1);

				if (cl_timestamp->intVal)
					Com_Printf (PRNT_CHATHUD, "[%s] %s", timestamp, s);
				else
					Com_Printf (PRNT_CHATHUD, "%s", s);

				if (!queryLastTime || (queryLastTime < cls.realTime-(300*1000))) {
					// lets wait 300 seconds
					if (strstr (s, "!version")) {
						queryLastTime = cls.realTime;
						Cbuf_AddText ("egl_version\n");
					}
					else if (strstr (s, "!renderer")) {
						queryLastTime = cls.realTime;
						Cbuf_AddText ("egl_renderer\n");
					}
				}
				break;

			default:
				if (cl_timestamp->intVal > 1)
					Com_Printf (0, "[%s] %s", timestamp, s);
				else
					Com_Printf (0, "%s", s);
				break;
			}
			break;

		case SVC_STUFFTEXT:
			s = cls.netMessage.ReadString ();
			Com_DevPrintf (0, "stufftext: %s\n", s);
			if (!cl.attractLoop || !strcmp (s, "precache\n") || !strcmp (s, "reconnect\n"))
				Cbuf_AddText (s);
			else
				Com_DevPrintf (PRNT_WARNING, "WARNING: Demo tried to execute command '%s', ignored.\n", s);
			break;

		case SVC_SERVERDATA:
			Cbuf_Execute ();	// Make sure any stuffed commands are done
			if (!CL_ParseServerData ()) {
				CL_CGModule_EndServerMessage ();
				return;
			}
			CL_WriteDemoMessageChunk (cls.netMessage.data + oldReadCount, cls.netMessage.readCount - oldReadCount, false);
			break;

		case SVC_CONFIGSTRING:
			CL_ParseConfigString ();
			CL_WriteDemoMessageChunk (cls.netMessage.data + oldReadCount, cls.netMessage.readCount - oldReadCount, false);
			break;

		case SVC_SPAWNBASELINE:
			CL_ParseBaseline ();
			CL_WriteDemoMessageChunk (cls.netMessage.data + oldReadCount, cls.netMessage.readCount - oldReadCount, false);
			break;

		case SVC_DOWNLOAD:
			CL_ParseDownload (false);
			break;

		case SVC_PLAYERINFO:
		case SVC_PACKETENTITIES:
		case SVC_DELTAPACKETENTITIES:
			assert (0);
			Com_Error (ERR_DROP, "CL_ParseServerMessage: Out of place frame data");
			break;

		case SVC_FRAME:
			CL_ParseFrame (extraBits);
			break;

		case SVC_ZPACKET:
			CL_ParseZPacket ();
			break;

		case SVC_ZDOWNLOAD:
			CL_ParseDownload (true);
			break;

		default:
			CL_WriteDemoMessageChunk (cls.netMessage.data + oldReadCount, cls.netMessage.readCount - oldReadCount, false);
			if (CL_CGModule_ParseServerMessage (cmd))
				break;

			// Unknown to the client and CGame failed to parse it so...
			assert (0);
			Com_Error (ERR_DROP, "CL_ParseServerMessage: Illegible server message\n"
				"Message #%i: %s\n"
				"Last #%i: %s\n"
				"Try 'set cl_protocol 34' before connecting, a protocol change could be causing problems",
				cmd, cl_svcStrings[cmd],
				lastCmd, (lastCmd == -2) ? "None" : cl_svcStrings[lastCmd]);
			break;
		}

		lastCmd = cmd;
	}

	// Flush this frame
	CL_WriteDemoMessageChunk (NULL, 0, true);

	// Let CGame know
	CL_CGModule_EndServerMessage ();
}
