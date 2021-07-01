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
// sv_ents.c
//

#include "sv_local.h"

/*
=============================================================================

	ENCODE A CLIENT FRAME ONTO THE NETWORK CHANNEL

=============================================================================
*/

/*
=============
SV_EmitPacketEntities

Writes a delta update of an entityStateOld_t list to the message.
=============
*/

struct entityBits_t
{
	int				number;
	entityType_t	type;
};

static void SV_EmitPacketEntities (clientFrame_t *from, clientFrame_t *to, netMsg_t *msg)
{
	entityState_t	*oldEnt, *newEnt;
	int		oldIndex, newIndex;
	entityBits_t		oldNum, newNum;
	int		from_numEntities;
	int		bits;

	msg->WriteByte (SVC_PACKETENTITIES);

	if (!from)
		from_numEntities = 0;
	else
		from_numEntities = from->numEntities;

	newIndex = 0;
	newEnt = NULL;
	oldIndex = 0;
	oldEnt = NULL;

	for (newIndex=oldIndex=0, newEnt=oldEnt=NULL ; newIndex<to->numEntities || oldIndex<from_numEntities ; ) {
		if (newIndex >= to->numEntities)
		{
			newNum.number = 9999;
			newNum.type = 0;
		}
		else {
			newEnt = &svs.clientEntities[(to->firstEntity+newIndex)%svs.numClientEntities];
			newNum.number = newEnt->number;
			newNum.type = newEnt->type;
		}

		if (oldIndex >= from_numEntities)
		{
			oldNum.number = 9999;
			oldNum.type = 0;
		}
		else {
			oldEnt = &svs.clientEntities[(from->firstEntity+oldIndex)%svs.numClientEntities];
			oldNum.number = oldEnt->number;
			oldNum.type = oldEnt->type;
		}

		if (newNum.number == oldNum.number) {
			/*
			** delta update from old position
			** because the force parm is false, this will not result
			** in any bytes being emited if the entity has not changed at all
			** note that players are always 'newentities', this updates their oldorigin always
			** and prevents warping
			*/
			msg->WriteDeltaEntity (oldEnt, newEnt, false, newEnt->number <= maxclients->intVal);
			oldIndex++;
			newIndex++;
			continue;
		}

		if (newNum.number < oldNum.number) {
			// This is a new entity, send it from the baseline
			msg->WriteDeltaEntity (&sv.baseLines[newNum.number], newEnt, true, true);
			newIndex++;
			continue;
		}

		if (newNum.number > oldNum.number) {
			// The old entity isn't present in the new message
			bits = U_REMOVE;
			if (oldNum.number >= 256)
				bits |= U_NUMBER16 | U_MOREBITS1;

			msg->WriteByte (bits&255);
			if (bits & 0x0000ff00)
				msg->WriteByte ((bits>>8)&255);

			if (bits & U_NUMBER16)
				msg->WriteShort (oldNum.number);
			else
				msg->WriteByte (oldNum.number);

			msg->WriteByte(oldNum.type);

			oldIndex++;
			continue;
		}
	}

	msg->WriteShort (0);	// end of packetentities
}


/*
=============
SV_WritePlayerstateToClient
=============
*/
#define Vec3Clamp(v,l,h) \
	do { \
		if ((v)[0] > (h)) (v)[0] = (h); else if ((v)[0] < (l)) (v)[0] = (l); \
		if ((v)[1] > (h)) (v)[1] = (h); else if ((v)[1] < (l)) (v)[1] = (l); \
		if ((v)[2] > (h)) (v)[2] = (h); else if ((v)[2] < (l)) (v)[2] = (l); \
	} while (0)
static void SV_WritePlayerstateToClient (clientFrame_t *from, clientFrame_t *to, netMsg_t *msg)
{
	playerState_t	*ps, *ops;
	playerState_t	dummy;
	int				i, psFlags;
	int				statBits;

	ps = &to->playerState;
	if (!from) {
		memset (&dummy, 0, sizeof(dummy));
		ops = &dummy;
	}
	else
		ops = &from->playerState;

	// Range cap
	Vec3Clamp (ps->viewOffset, -32, 31.75f);
	Vec3Clamp (ps->kickAngles, -32, 31.75f);

	// Determine what needs to be sent
	psFlags = 0;
	if (ps->pMove.pmType != ops->pMove.pmType)
		psFlags |= PS_M_TYPE;

	if (ps->pMove.origin[0] != ops->pMove.origin[0]
	|| ps->pMove.origin[1] != ops->pMove.origin[1]
	|| ps->pMove.origin[2] != ops->pMove.origin[2])
		psFlags |= PS_M_ORIGIN;

	if (ps->pMove.velocity[0] != ops->pMove.velocity[0]
	|| ps->pMove.velocity[1] != ops->pMove.velocity[1]
	|| ps->pMove.velocity[2] != ops->pMove.velocity[2])
		psFlags |= PS_M_VELOCITY;

	if (ps->pMove.pmTime != ops->pMove.pmTime)
		psFlags |= PS_M_TIME;

	if (ps->pMove.pmFlags != ops->pMove.pmFlags)
		psFlags |= PS_M_FLAGS;

	if (ps->pMove.gravity != ops->pMove.gravity)
		psFlags |= PS_M_GRAVITY;

	if (ps->pMove.deltaAngles[0] != ops->pMove.deltaAngles[0]
	|| ps->pMove.deltaAngles[1] != ops->pMove.deltaAngles[1]
	|| ps->pMove.deltaAngles[2] != ops->pMove.deltaAngles[2])
		psFlags |= PS_M_DELTA_ANGLES;


	if (ps->viewOffset[0] != ops->viewOffset[0]
	|| ps->viewOffset[1] != ops->viewOffset[1]
	|| ps->viewOffset[2] != ops->viewOffset[2])
		psFlags |= PS_VIEWOFFSET;

	if (ps->viewAngles[0] != ops->viewAngles[0]
	|| ps->viewAngles[1] != ops->viewAngles[1]
	|| ps->viewAngles[2] != ops->viewAngles[2])
		psFlags |= PS_VIEWANGLES;

	if (ps->kickAngles[0] != ops->kickAngles[0]
	|| ps->kickAngles[1] != ops->kickAngles[1]
	|| ps->kickAngles[2] != ops->kickAngles[2])
		psFlags |= PS_KICKANGLES;

	if (ps->viewBlend[0] != 0
	|| ps->viewBlend[1] != 0
	|| ps->viewBlend[2] != 0
	|| ps->viewBlend[3] != 0)
		psFlags |= PS_BLEND;

	if (ps->fov != ops->fov)
		psFlags |= PS_FOV;

	if (ps->rdFlags != ops->rdFlags)
		psFlags |= PS_RDFLAGS;

	if (ps->gunAnim != ops->gunAnim)
		psFlags |= PS_WEAPONFRAME;

	psFlags |= PS_WEAPONINDEX;

	// Write it
	msg->WriteByte (SVC_PLAYERINFO);
	msg->WriteShort (psFlags);

	// Write the pMoveState_t
	if (psFlags & PS_M_TYPE)
		msg->WriteByte (ps->pMove.pmType);

	if (psFlags & PS_M_ORIGIN)
	{
		msg->WriteShort (ps->pMove.origin[0]);
		msg->WriteShort (ps->pMove.origin[1]);
		msg->WriteShort (ps->pMove.origin[2]);
	}

	if (psFlags & PS_M_VELOCITY) {
		msg->WriteShort (ps->pMove.velocity[0]);
		msg->WriteShort (ps->pMove.velocity[1]);
		msg->WriteShort (ps->pMove.velocity[2]);
	}

	if (psFlags & PS_M_TIME)
		msg->WriteByte (ps->pMove.pmTime);

	if (psFlags & PS_M_FLAGS)
		msg->WriteByte (ps->pMove.pmFlags);

	if (psFlags & PS_M_GRAVITY)
		msg->WriteShort (ps->pMove.gravity);

	if (psFlags & PS_M_DELTA_ANGLES)
	{
		msg->WriteShort (ps->pMove.deltaAngles[0]);
		msg->WriteShort (ps->pMove.deltaAngles[1]);
		msg->WriteShort (ps->pMove.deltaAngles[2]);
	}

	// Write the rest of the playerState_t
	if (psFlags & PS_VIEWOFFSET)
	{
		msg->WriteChar (ps->viewOffset[0]*4);
		msg->WriteChar (ps->viewOffset[1]*4);
		msg->WriteChar (ps->viewOffset[2]*4);
	}

	if (psFlags & PS_VIEWANGLES)
	{
		msg->WriteAngle16 (ps->viewAngles[0]);
		msg->WriteAngle16 (ps->viewAngles[1]);
		msg->WriteAngle16 (ps->viewAngles[2]);
	}

	if (psFlags & PS_KICKANGLES)
	{
		msg->WriteChar (ps->kickAngles[0]*4);
		msg->WriteChar (ps->kickAngles[1]*4);
		msg->WriteChar (ps->kickAngles[2]*4);
	}

	if (psFlags & PS_WEAPONFRAME)
		msg->WriteChar (ps->gunAnim);

	if (psFlags & PS_BLEND)
	{
		// R1: clamp the color
		if (ps->viewBlend[1] > 1)
			ps->viewBlend[1] = 1;
		if (ps->viewBlend[2] > 1)
			ps->viewBlend[2] = 1;
		if (ps->viewBlend[3] > 1)
			ps->viewBlend[3] = 1;

		msg->WriteByte (ps->viewBlend[0]*255);
		msg->WriteByte (ps->viewBlend[1]*255);
		msg->WriteByte (ps->viewBlend[2]*255);
		msg->WriteByte (ps->viewBlend[3]*255);
	}

	if (psFlags & PS_FOV)
		msg->WriteByte (ps->fov);

	if (psFlags & PS_RDFLAGS)
		msg->WriteByte (ps->rdFlags);

	// Send stats
	statBits = 0;
	for (i=0 ; i<MAX_STATS ; i++)
		if (ps->stats[i] != ops->stats[i])
			statBits |= BIT(i);
	msg->WriteLong (statBits);
	for (i=0 ; i<MAX_STATS ; i++)
		if (statBits & BIT(i))
			msg->WriteShort (ps->stats[i]);
}


/*
==================
SV_WriteFrameToClient
==================
*/
void SV_WriteFrameToClient (svClient_t *client, netMsg_t *msg)
{
	clientFrame_t	*frame, *oldFrame;
	int				lastFrame;

	// This is the frame we are creating
	frame = &client->frames[sv.frameNum & UPDATE_MASK];

	if (client->lastFrame <= 0) {
		// Client is asking for a retransmit
		oldFrame = NULL;
		lastFrame = -1;
	}
	else if (sv.frameNum - client->lastFrame >= (UPDATE_BACKUP - 3)) {
		// Client hasn't gotten a good message through in a long time
		oldFrame = NULL;
		lastFrame = -1;
	}
	else {
		// We have a valid message to delta from
		oldFrame = &client->frames[client->lastFrame & UPDATE_MASK];
		lastFrame = client->lastFrame;
	}

	msg->WriteByte (SVC_FRAME);
	msg->WriteLong (sv.frameNum);
	msg->WriteLong (lastFrame);	// What we are delta'ing from
	msg->WriteByte (client->surpressCount);	// Rate dropped packets
	client->surpressCount = 0;

	// Send over the areaBits
	msg->WriteByte (frame->areaBytes);
	msg->WriteRaw (frame->areaBits, frame->areaBytes);

	// Delta encode the playerstate
	SV_WritePlayerstateToClient (oldFrame, frame, msg);

	// Delta encode the entities
	SV_EmitPacketEntities (oldFrame, frame, msg);
}


/*
=============================================================================

	BUILD A CLIENT FRAME STRUCTURE

=============================================================================
*/

static byte		sv_fatPVS[65536/8];	// 32767 is Q2BSP_MAX_LEAFS

/*
============
SV_FatPVS

The client will interpolate the view position,
so we can't use a single PVS point
===========
*/
static void SV_FatPVS (vec3_t org)
{
	int		leafs[64];
	int		i, j, count;
	int		longs;
	byte	*src;
	vec3_t	mins, maxs;

	for (i=0 ; i<3 ; i++) {
		mins[i] = org[i] - 8;
		maxs[i] = org[i] + 8;
	}

	count = CM_BoxLeafnums (mins, maxs, leafs, 64, NULL);
	if (count < 1)
		Com_Error (ERR_FATAL, "SV_FatPVS: count < 1");
	longs = (CM_NumClusters()+31)>>5;

	// convert leafs to clusters
	for (i=0 ; i<count ; i++)
		leafs[i] = CM_LeafCluster(leafs[i]);

	memcpy (sv_fatPVS, CM_ClusterPVS(leafs[0]), longs<<2);
	// or in all the other leaf bits
	for (i=1 ; i<count ; i++) {
		for (j=0 ; j<i ; j++)
			if (leafs[i] == leafs[j])
				break;
		if (j != i)
			continue;		// already have the cluster we want
		src = CM_ClusterPVS(leafs[i]);
		for (j=0 ; j<longs ; j++)
			((long *)sv_fatPVS)[j] |= ((long *)src)[j];
	}
}

edict_t *GetEntity (int num)
{
	return EDICT_NUM(num);
}

/*
=============
SV_BuildClientFrame

Decides which entities are going to be visible to the client, and
copies off the playerstat and areaBits.
=============
*/
void SV_BuildClientFrame (svClient_t *client)
{
	int			e, i;
	vec3_t		org;
	edict_t		*ent;
	edict_t		*clent;
	clientFrame_t	*frame;
	entityState_t	*state;
	int			l;
	int			clientarea, clientcluster;
	int			leafnum;
	int			c_fullsend;
	byte		*clientphs;
	byte		*bitvector;

	clent = client->edict;
	if (!clent->client)
		return;		// not in game yet

	// This is the frame we are creating
	frame = &client->frames[sv.frameNum & UPDATE_MASK];

	frame->sentTime = svs.realTime; // save it for ping calc later

	// Find the client's PVS
	org[0] = clent->client->playerState.pMove.origin[0]*(1.0f/8.0f) + clent->client->playerState.viewOffset[0];
	org[1] = clent->client->playerState.pMove.origin[1]*(1.0f/8.0f) + clent->client->playerState.viewOffset[1];
	org[2] = clent->client->playerState.pMove.origin[2]*(1.0f/8.0f) + clent->client->playerState.viewOffset[2];

	leafnum = CM_PointLeafnum (org);
	clientarea = CM_LeafArea (leafnum);
	clientcluster = CM_LeafCluster (leafnum);

	// calculate the visible areas
	frame->areaBytes = CM_WriteAreaBits (frame->areaBits, clientarea);

	// grab the current playerState_t
	frame->playerState = clent->client->playerState;

	SV_FatPVS (org);
	clientphs = CM_ClusterPHS (clientcluster);

	// build up the list of visible entities
	frame->numEntities = 0;
	frame->firstEntity = svs.nextClientEntities;

	c_fullsend = 0;

	for (e=1 ; e<ge->numEdicts ; e++) {
		ent = EDICT_NUM(e);

		// ignore ents without visible models
		if ((ent->svFlags & SVF_NOCLIENT)
			// as long as they have no events!
			&& !ent->s.events[0].ID && !ent->s.events[1].ID)
			continue;

		// ignore ents without visible models unless they have an effect
		if (!ent->s.modelIndex && !ent->s.effects && !ent->s.sound &&
			!ent->s.events[0].ID && !ent->s.events[1].ID)
			continue;

		// ignore events if they have none
		if ((ent->svFlags & SVF_EVENT) && !ent->s.events[0].ID && !ent->s.events[1].ID)
			continue;

		// ignore if not touching a PV leaf
		if (ent != clent) {
			// check area
			if (!CM_AreasConnected (clientarea, ent->areaNum)) {
				/*
				** doors can legally straddle two areas, so
				** we may need to check another one
				*/
				if (!ent->areaNum2 || !CM_AreasConnected (clientarea, ent->areaNum2))
					continue;		// blocked by a door
			}

			// beams just check one point for PHS
			if (ent->s.renderFx & RF_BEAM) {
				l = ent->clusterNums[0];
				if (!(clientphs[l >> 3] & BIT(l&7)))
					continue;
			}
			else {
				// FIXME: if an ent has a model and a sound, but isn't
				// in the PVS, only the PHS, clear the model
				if (ent->s.sound)
					bitvector = sv_fatPVS;	//clientphs;
				else
					bitvector = sv_fatPVS;

				if (ent->numClusters == -1) {
					// too many leafs for individual check, go by headnode
					if (!CM_HeadnodeVisible (ent->headNode, bitvector))
						continue;
					c_fullsend++;
				}
				else {
					// check individual leafs
					for (i=0 ; i < ent->numClusters ; i++) {
						l = ent->clusterNums[i];
						if (bitvector[l >> 3] & BIT(l&7))
							break;
					}
					if (i == ent->numClusters)
						continue;		// not visible
				}

				if (!ent->s.modelIndex && !(ent->svFlags & SVF_EVENT)) {
					// don't send sounds if they will be attenuated away
					vec3_t	delta;
					float	len;

					Vec3Subtract (org, ent->s.origin, delta);
					len = Vec3Length (delta);
					if (len > 400)
						continue;
				}
			}
		}

		// add it to the circular clientEntities array
		state = &svs.clientEntities[svs.nextClientEntities%svs.numClientEntities];
		if (ent->s.number != e) {
			Com_DevPrintf (0, "FIXING ENT->S.NUMBER!!!\n");
			ent->s.number = e;
		}
		//memcpy (&state, &ent->s, sizeof(entityStateOld_t));
		*state = ent->s;

		// don't mark players missiles as solid
		if (ent->owner == client->edict)
			state->solid = SOLID_NOT;

		svs.nextClientEntities++;
		frame->numEntities++;
	}
}


/*
==================
SV_RecordDemoMessage

Save everything in the world out without deltas.
Used for recording footage for merged or assembled demos
==================
*/
void SV_RecordDemoMessage ()
{
	int				e;
	edict_t			*ent;
	entityState_t	nostate;
	netMsg_t		buf;
	byte			buf_data[32768];
	int				len;

	if (!svs.demoFile)
		return;

	buf.Init(buf_data, sizeof(buf_data));

	// write a frame message that doesn't contain a playerState_t
	buf.WriteByte (SVC_FRAME);
	buf.WriteLong (sv.frameNum);

	buf.WriteByte (SVC_PACKETENTITIES);

	e = 1;
	ent = EDICT_NUM(e);
	while (e < ge->numEdicts) {
		// ignore ents without visible models unless they have an effect
		if (ent->inUse && ent->s.number
		&& (ent->s.modelIndex || ent->s.effects || ent->s.sound || ent->s.events[0].ID || ent->s.events[1].ID)
		&& !(ent->svFlags & SVF_NOCLIENT))
			buf.WriteDeltaEntity (&nostate, &ent->s, false, true);

		e++;
		ent = EDICT_NUM(e);
	}

	buf.WriteShort (0);		// end of packetentities

	// now add the accumulated multicast information
	buf.WriteRaw (svs.demoMultiCast.data, svs.demoMultiCast.curSize);
	svs.demoMultiCast.Clear();

	// now write the entire message to the file, prefixed by the length
	len = LittleLong (buf.curSize);

	FS_Write (&len, sizeof(len), svs.demoFile);
	FS_Write (buf.data, buf.curSize, svs.demoFile);
}
