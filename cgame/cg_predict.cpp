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
// cg_predict.c
//

#include "cg_local.h"

static TList<entityState_t*>	cg_solidList;

/*
===================
CG_CheckPredictionError
===================
*/
void CG_CheckPredictionError ()
{
	int		frame;
	int		delta[3];
	int		len;
	int		incAck;

	cgi.NET_GetSequenceState (NULL, &incAck);

	// Calculate the last userCmd_t we sent that the server has processed
	frame = incAck & CMD_MASK;

	// Compare what the server returned with what we had predicted it to be
	Vec3Subtract (cg.frame.playerState.pMove.origin, cg.predicted.origins[frame], delta);

	// Save the prediction error for interpolation
	len = abs (delta[0]) + abs (delta[1]) + abs (delta[2]);
	if (len > 640) {
		// 80 world units, a teleport or something
		Vec3Clear (cg.predicted.error);
	}
	else {
		if (cl_showmiss->intVal && (delta[0] || delta[1] || delta[2]))
			Com_Printf (PRNT_WARNING, "CG_CheckPredictionError: prediction miss on frame %i: %i\n",
				cg.frame.serverFrame, delta[0]+delta[1]+delta[2]);

		Vec3Copy (cg.frame.playerState.pMove.origin, cg.predicted.origins[frame]);
		Vec3Scale (delta, (1.0f/8.0f), cg.predicted.error);
	}
}


/*
====================
CG_BuildSolidList
====================
*/
void CG_BuildSolidList ()
{
	entityState_t	*ent;
	int				num, i;

	cg_solidList.Clear();
	for (i=0 ; i<cg.frame.numEntities ; i++) {
		num = (cg.frame.parseEntities + i) & (MAX_PARSEENTITIES_MASK);
		ent = &cg_parseEntities[num];

		if (ent->solid)
			cg_solidList.Add(ent);
	}
}


/*
====================
CG_ClipMoveToEntities
====================
*/
static void CG_ClipMoveToEntities (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int ignoreNum, bool entities, bool bModels, cmTrace_t *out)
{
	int				x, zd, zu;
	cmTrace_t		trace;
	int				headnode;
	float			*angles;
	entityState_t	*ent;
	struct cmBspModel_t *cmodel;
	vec3_t			bmins, bmaxs;

	for (uint32 i=0 ; i<cg_solidList.Count() ; i++) {
		ent = cg_solidList[i];
		if (ent->number == ignoreNum)
			continue;

		if (ent->solid == 31) {
			// Special value for bmodel
			if (!bModels)
				continue;

			cmodel = cg.modelCfgClip[ent->modelIndex];
			if (!cmodel)
				continue;
			headnode = cgi.CM_InlineModelHeadNode (cmodel);
			angles = ent->angles;
		}
		else {
			// Encoded bbox
			if (!entities)
				continue;

			if (cg.protocolMinorVersion >= MINOR_VERSION_R1Q2_32BIT_SOLID)
			{
				x = (ent->solid & 255);
				zd = ((ent->solid>>8) & 255);
				zu = ((ent->solid>>16) & 65535) - 32768;
			}
			else
			{
				x = 8 * (ent->solid & 31);
				zd = 8 * ((ent->solid >> 5) & 31);
				zu = 8 * ((ent->solid >> 10) & 63) - 32;
			}

			bmins[0] = bmins[1] = -x;
			bmaxs[0] = bmaxs[1] = x;
			bmins[2] = -zd;
			bmaxs[2] = zu;

			headnode = cgi.CM_HeadnodeForBox (bmins, bmaxs);
			angles = vec3Origin;	// Boxes don't rotate
		}

		if (out->allSolid)
			return;

		cgi.CM_TransformedBoxTrace (&trace, start, end, mins, maxs, headnode, CONTENTS_MASK_PLAYERSOLID, ent->origin, angles);
		if (trace.allSolid || trace.startSolid || trace.fraction < out->fraction) {
			trace.ent = (struct edict_t *)ent;
			if (out->startSolid) {
				*out = trace;
				out->startSolid = true;
			}
			else
				*out = trace;
		}
		else if (trace.startSolid)
			out->startSolid = true;
	}
}


/*
================
CG_PMTrace
================
*/
void CG_Trace (cmTrace_t *out, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, bool entities, bool bModels, int ignore, int contentMask)
{
	if (!out)
		return;

	if (!mins)
		mins = vec3Origin;

	if (!maxs)
		maxs = vec3Origin;

	memset (out, 0, sizeof(*out));

	// Check against world
	*out = cgi.CM_BoxTrace (start, end, mins, maxs, 0, contentMask);
	if (out->fraction < 1.0)
		out->ent = (struct edict_t *)1;

	// Check all other solid models
	if (entities || bModels)
		CG_ClipMoveToEntities (start, mins, maxs, end, cg.playerNum+1, entities, bModels, out);
}

void CG_PMTrace (cmTrace_t *out, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, bool entities, bool bModels)
{
	CG_Trace (out, start, mins, maxs, end, entities, bModels, cg.playerNum+1, CONTENTS_MASK_SOLID|CONTENTS_MONSTER);
}


/*
================
CG_PMLTrace

Local version
================
*/
static cmTrace_t CG_PMLTrace (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end)
{
	cmTrace_t tr;

	// Check against world
	tr = cgi.CM_BoxTrace (start, end, mins, maxs, 0, CONTENTS_MASK_PLAYERSOLID);
	if (tr.fraction < 1.0)
		tr.ent = (struct edict_t *)1;

	// Check all other solid models
	CG_ClipMoveToEntities (start, mins, maxs, end, cg.playerNum+1, true, true, &tr);

	return tr;
}


/*
================
CG_PMPointContents
================
*/
int CG_PMPointContents (vec3_t point)
{
	entityState_t	*ent;
	int				i, num;
	struct cmBspModel_t *cmodel;
	int				contents;

	contents = cgi.CM_PointContents (point, 0);

	for (i=0 ; i<cg.frame.numEntities ; i++) {
		num = (cg.frame.parseEntities + i)&(MAX_PARSEENTITIES_MASK);
		ent = &cg_parseEntities[num];
		if (ent->solid != 31) // Special value for bmodel
			continue;

		cmodel = cg.modelCfgClip[ent->modelIndex];
		if (!cmodel)
			continue;

		contents |= cgi.CM_TransformedPointContents (point, cgi.CM_InlineModelHeadNode (cmodel), ent->origin, ent->angles);
	}

	return contents;
}


/*
=================
CG_PredictMovement

Sets cg.predicted.origin and cg.predicted.angles
=================
*/
void CG_PredictMovement ()
{
	int			ack, current;
	int			frame;
	int			step;
	float		oldStep;
	pMoveNew_t	pm;

	if (cg_paused->intVal)
		return;

	if (!cl_predict->intVal || cg.frame.playerState.pMove.pmFlags & PMF_NO_PREDICTION) {
		userCmd_t	cmd;

		current = cgi.NET_GetCurrentUserCmdNum ();
		cgi.NET_GetUserCmd (current, &cmd);

		Vec3Scale (cg.frame.playerState.pMove.velocity, (1.0f/8.0f), cg.predicted.velocity);
		Vec3Scale (cg.frame.playerState.pMove.origin, (1.0f/8.0f), cg.predicted.origin);

		cg.predicted.angles[0] = SHORT2ANGLE(cmd.angles[0]) + SHORT2ANGLE(cg.frame.playerState.pMove.deltaAngles[0]);
		cg.predicted.angles[1] = SHORT2ANGLE(cmd.angles[1]) + SHORT2ANGLE(cg.frame.playerState.pMove.deltaAngles[1]);
		cg.predicted.angles[2] = SHORT2ANGLE(cmd.angles[2]) + SHORT2ANGLE(cg.frame.playerState.pMove.deltaAngles[2]);

		return;
	}

	cgi.NET_GetSequenceState (&current, &ack);

	// If we are too far out of date, just freeze
	if (current - ack >= CMD_BACKUP) {
		if (cl_showmiss->intVal)
			Com_Printf (PRNT_WARNING, "CG_PredictMovement: exceeded CMD_BACKUP\n");
		return;	
	}

	// Copy current state to pmove
	memset (&pm, 0, sizeof(pm));
	pm.trace = CG_PMLTrace;
	pm.pointContents = CG_PMPointContents;
	pm.state = cg.frame.playerState.pMove;

	if (cg.attractLoop)
		pm.state.pmType = PMT_FREEZE;		// Demo playback

	if (pm.state.pmType == PMT_SPECTATOR)
		pm.multiplier = 2;
	else
		pm.multiplier = 1;

	pm.strafeHack = cg.strafeHack;

	// Run frames
	frame = 0;
	while (++ack <= current) {		// Changed '<' to '<=' cause current is our pending cmd
		frame = ack & CMD_MASK;
		cgi.NET_GetUserCmd (frame, &pm.cmd);

		if (pm.cmd.msec <= 0)
			continue;	// Ignore 'null' usercmd entries.

		// Playerstate transmitted mins/maxs
		Vec3Set (pm.mins, -16, -16, -24);
		Vec3Set (pm.maxs,  16,  16,  32);

		Pmove (&pm, atof (cg.configStrings[CS_AIRACCEL]));

		// Save for debug checking
		Vec3Copy (pm.state.origin, cg.predicted.origins[frame]);
	}

	// Calculate the step adjustment
	step = pm.state.origin[2] - (int)(cg.predicted.origin[2] * 8);
	if (pm.step && step > 0 && step < 320 && pm.state.pmFlags & PMF_ON_GROUND) {
		if (cg.refreshTime - cg.predicted.stepTime < 150)
			oldStep = cg.predicted.step * (150 - (cg.refreshTime - cg.predicted.stepTime)) * (1.0f / 150.0f);
		else
			oldStep = 0;

		cg.predicted.step = oldStep + step * (1.0f/8.0f);
		cg.predicted.stepTime = cg.refreshTime - cg.refreshFrameTime * 500;
	}

	Vec3Scale (pm.state.velocity, (1.0f/8.0f), cg.predicted.velocity);
	Vec3Scale (pm.state.origin, (1.0f/8.0f), cg.predicted.origin);
	cg.predicted.viewHeight = pm.viewHeight;
	Vec3Copy (pm.viewAngles, cg.predicted.angles);
}

void G_ProjectSource (vec3_t point, vec3_t distance, vec3_t forward, vec3_t right, vec3_t result)
{
	result[0] = point[0] + forward[0] * distance[0] + right[0] * distance[1];
	result[1] = point[1] + forward[1] * distance[0] + right[1] * distance[1];
	result[2] = point[2] + forward[2] * distance[0] + right[2] * distance[1] + distance[2];
}

void CG_ProjectSource (byte hand, vec3_t point, vec3_t distance, vec3_t forward, vec3_t right, vec3_t result)
{
	vec3_t	_distance;

	Vec3Copy (distance, _distance);

	if (hand == 1)
		_distance[1] *= -1;
	else if (hand == 2)
		_distance[1] = 0;
	G_ProjectSource (point, _distance, forward, right, result);
}