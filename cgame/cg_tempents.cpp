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
// cg_tempents.c
//

#include "cg_local.h"

// explo rattles
#define MAX_TENT_EXPLORATTLES	32
float	cgExploRattles[MAX_TENT_EXPLORATTLES];

// beams
#define MAX_TENT_BEAMS			32
struct teBeam_t {
	int					entity;
	int					dest_entity;
	struct refModel_t	*model;
	int					endtime;
	vec3_t				offset;
	vec3_t				start, end;
};
teBeam_t	cgBeams[MAX_TENT_BEAMS];
teBeam_t	cgPlayerBeams[MAX_TENT_BEAMS]; // player-linked beams

// lasers
#define MAX_TENT_LASERS			32
struct teLaser_t {
	refEntity_t	ent;
	int			endtime;
};
teLaser_t	cgLasers[MAX_TENT_LASERS];

/*
=============================================================================

	EXPLOSION SCREEN RATTLES

=============================================================================
*/

/*
=================
CG_ExploRattle
=================
*/
void CG_ExploRattle (vec3_t org, float scale)
{
	int		i;
	float	dist, max;
	vec3_t	temp;

	if (!cg_explorattle->intVal)
		return;

	for (i=0 ; i<MAX_TENT_EXPLORATTLES ; i++) {
		if (cgExploRattles[i] > 0)
			continue;

		// calculate distance
		dist = Vec3DistFast (cg.refDef.viewOrigin, org) * 0.1;
		max = (20 * scale, 20, 50);

		// lessen the effect when it's behind the view
		Vec3Subtract (org, cg.refDef.viewOrigin, temp);
		VectorNormalizef (temp, temp);

		if (DotProduct (temp, cg.refDef.viewAxis[0]) < 0)
			dist *= 1.25;

		// clamp
		if ((dist > 0) && (dist < max))
			cgExploRattles[i] = max - dist;

		break;
	}
}


/*
=================
CG_AddExploRattles
=================
*/
static void CG_AddExploRattles ()
{
	int		i;
	float	scale;

	if (!cg_explorattle->intVal)
		return;

	scale = clamp (cg_explorattle_scale->floatVal, 0, 0.95);
	for (i=0 ; i<MAX_TENT_EXPLORATTLES ; i++) {
		if (cgExploRattles[i] <= 0)
			continue;

		cgExploRattles[i] *= scale * scale;

		cg.refDef.viewAngles[0] += cgExploRattles[i] * crand ();
		cg.refDef.viewAngles[1] += cgExploRattles[i] * crand ();
		cg.refDef.viewAngles[2] += cgExploRattles[i] * crand ();

		if (cgExploRattles[i] < 0.001)
			cgExploRattles[i] = -1;
	}
}


/*
=================
CG_ClearExploRattles
=================
*/
static void CG_ClearExploRattles ()
{
	int		i;

	for (i=0 ; i<MAX_TENT_EXPLORATTLES ; i++)
		cgExploRattles[i] = -1;
}
/*
=============================================================================

	BEAM MANAGEMENT

=============================================================================
*/

/*
=================
CG_ParseBeam
=================
*/
static int CG_ParseBeam (struct refModel_t *model)
{
	int			ent;
	vec3_t		start, end;
	teBeam_t	*b;
	int			i;
	
	ent = cgi.MSG_ReadShort ();
	
	cgi.MSG_ReadPos (start);
	cgi.MSG_ReadPos (end);

	// override any beam with the same entity
	for (i=0, b=cgBeams ; i<MAX_TENT_BEAMS ; i++, b++) {
		if (b->entity == ent) {
			b->entity = ent;
			b->model = model;
			b->endtime = cg.refreshTime + 200;
			Vec3Copy (start, b->start);
			Vec3Copy (end, b->end);
			Vec3Clear (b->offset);
			return ent;
		}
	}

	// find a free beam
	for (i=0, b=cgBeams ; i<MAX_TENT_BEAMS ; i++, b++) {
		if (!b->model || (b->endtime < cg.refreshTime)) {
			b->entity = ent;
			b->model = model;
			b->endtime = cg.refreshTime + 200;
			Vec3Copy (start, b->start);
			Vec3Copy (end, b->end);
			Vec3Clear (b->offset);
			return ent;
		}
	}

	Com_Printf (PRNT_WARNING, "beam list overflow!\n");
	return ent;
}


/*
=================
CG_ParseBeam2
=================
*/
static int CG_ParseBeam2 (struct refModel_t *model)
{
	int			ent;
	vec3_t		start, end, offset;
	teBeam_t	*b;
	int			i;
	
	ent = cgi.MSG_ReadShort ();
	
	cgi.MSG_ReadPos (start);
	cgi.MSG_ReadPos (end);
	cgi.MSG_ReadPos (offset);

	// override any beam with the same entity
	for (i=0, b=cgBeams ; i<MAX_TENT_BEAMS ; i++, b++) {
		if (b->entity == ent) {
			b->entity = ent;
			b->model = model;
			b->endtime = cg.refreshTime + 200;
			Vec3Copy (start, b->start);
			Vec3Copy (end, b->end);
			Vec3Copy (offset, b->offset);
			return ent;
		}
	}

	// find a free beam
	for (i=0, b=cgBeams ; i<MAX_TENT_BEAMS ; i++, b++) {
		if (!b->model || (b->endtime < cg.refreshTime)) {
			b->entity = ent;
			b->model = model;
			b->endtime = cg.refreshTime + 200;	
			Vec3Copy (start, b->start);
			Vec3Copy (end, b->end);
			Vec3Copy (offset, b->offset);
			return ent;
		}
	}

	Com_Printf (PRNT_WARNING, "beam list overflow!\n");	
	return ent;
}


/*
=================
CG_ParsePlayerBeam

Adds to the cgPlayerBeams array instead of the cgBeams array
=================
*/
static int CG_ParsePlayerBeam (struct refModel_t *model)
{
	int			ent;
	vec3_t		start, end, offset;
	teBeam_t	*b;
	int			i;
	
	ent = cgi.MSG_ReadShort ();
	
	cgi.MSG_ReadPos (start);
	cgi.MSG_ReadPos (end);

	if (model == cgMedia.heatBeamModel)
		Vec3Set (offset, 2, 7, -3);
	else if (model == cgMedia.monsterHeatBeamModel) {
		model = cgMedia.heatBeamModel;
		Vec3Set (offset, 0, 0, 0);
	}
	else
		cgi.MSG_ReadPos (offset);

	// override any beam with the same entity
	// PMM - For player beams, we only want one per player (entity) so..
	for (i=0, b=cgPlayerBeams ; i<MAX_TENT_BEAMS ; i++, b++) {
		if (b->entity == ent) {
			b->entity = ent;
			b->model = model;
			b->endtime = cg.refreshTime + 200;
			Vec3Copy (start, b->start);
			Vec3Copy (end, b->end);
			Vec3Copy (offset, b->offset);
			return ent;
		}
	}

	// find a free beam
	for (i=0, b=cgPlayerBeams ; i<MAX_TENT_BEAMS ; i++, b++) {
		if (!b->model || (b->endtime < cg.refreshTime)) {
			b->entity = ent;
			b->model = model;
			b->endtime = cg.refreshTime + 100;		// PMM - this needs to be 100 to prevent multiple heatbeams
			Vec3Copy (start, b->start);
			Vec3Copy (end, b->end);
			Vec3Copy (offset, b->offset);
			return ent;
		}
	}

	Com_Printf (PRNT_WARNING, "beam list overflow!\n");	
	return ent;
}


/*
=================
CG_AddBeams
=================
*/
static void CG_AddBeams ()
{
	int			i, j;
	float		d, yaw, pitch, forward, len, steps;
	float		model_length;
	teBeam_t	*b;
	vec3_t		dist, org;
	refEntity_t	ent;
	vec3_t		angles;
	
	// update beams
	for (i=0, b=cgBeams ; i<MAX_TENT_BEAMS ; i++, b++) {
		if (!b->model || (b->endtime < cg.refreshTime))
			continue;

		// if coming from the player, update the start position
		// entity 0 is the world
		if (b->entity == cg.playerNum+1) {
			Vec3Copy (cg.refDef.viewOrigin, b->start);
			b->start[2] -= 22;	// adjust for view height
		}
		Vec3Add (b->start, b->offset, org);

		// calculate pitch and yaw
		Vec3Subtract (b->end, org, dist);

		if (dist[1] == 0 && dist[0] == 0) {
			yaw = 0;
			if (dist[2] > 0)
				pitch = 90;
			else
				pitch = 270;
		}
		else {
			// PMM - fixed to correct for pitch of 0
			if (dist[0])
				yaw = (atan2f(dist[1], dist[0]) * (180.0f / M_PI));
			else if (dist[1] > 0)
				yaw = 90;
			else
				yaw = 270;
			if (yaw < 0)
				yaw += 360;
	
			forward = sqrtf(dist[0]*dist[0] + dist[1]*dist[1]);
			pitch = (atan2f(dist[2], forward) * -(180.0f / M_PI));
			if (pitch < 0)
				pitch += 360.0;
		}

		// add new entities for the beams
		d = VectorNormalizef (dist, dist);

		memset (&ent, 0, sizeof(ent));
		if (b->model == cgMedia.lightningModel) {
			model_length = 35.0;
			d-= 20.0;  // correction so it doesn't end in middle of tesla
		}
		else {
			model_length = 30.0;
		}
		steps = ceil(d/model_length);
		len = (d-model_length)/(steps-1);

		// PMM - special case for lightning model .. if the real length is shorter than the model,
		// flip it around & draw it from the end to the start.  This prevents the model from going
		// through the tesla mine (instead it goes through the target)
		if (b->model == cgMedia.lightningModel && d <= model_length) {
			Vec3Copy (b->end, ent.origin);
			// offset to push beam outside of tesla model (negative because dist is from end to start

			ent.model = b->model;
			ent.flags = RF_FULLBRIGHT;
			ent.scale = 1;
			Vec3Set (angles, pitch, yaw, frand () * 360);

			if (angles[0] || angles[1] || angles[2])
				Angles_Matrix3 (angles, ent.axis);
			else
				Matrix3_Identity (ent.axis);
			Vec4Set (ent.color, 255, 255, 255, 255);
			cgi.R_AddEntity (&ent);
			return;
		}

		while (d > 0) {
			Vec3Copy (org, ent.origin);
			ent.model = b->model;
			if (b->model == cgMedia.lightningModel) {
				ent.flags = RF_FULLBRIGHT;
				Vec3Set (angles, -pitch, yaw + 180.0, frand () * 360);
			}
			else
				Vec3Set (angles, pitch, yaw, frand () * 360);

			ent.scale = 1;
			if (angles[0] || angles[1] || angles[2])
				Angles_Matrix3 (angles, ent.axis);
			else
				Matrix3_Identity (ent.axis);
			Vec4Set (ent.color, 255, 255, 255, 255);
			cgi.R_AddEntity (&ent);

			for (j=0 ; j<3 ; j++)
				org[j] += dist[j]*len;
			d -= model_length;
		}
	}
}


/*
=================
CG_AddPlayerBeams
=================
*/
static void CG_AddPlayerBeams ()
{
	teBeam_t	*b;
	vec3_t		dist, org, angles;
	int			i, j, framenum;
	float		d, yaw, pitch, forward, len, steps;
	refEntity_t			ent;
	float				model_length, hand_multiplier;
	netFrame_t			*oldframe;
	playerState_t	*ps, *ops;

	//PMM
	if (hand) {
		if (hand->intVal == 2)
			hand_multiplier = 0;
		else if (hand->intVal == 1)
			hand_multiplier = -1;
		else
			hand_multiplier = 1;
	}
	else
		hand_multiplier = 1;
	//PMM

	// update beams
	for (i=0, b=cgPlayerBeams ; i<MAX_TENT_BEAMS ; i++, b++) {
		vec3_t		fwd, right, up;
		if (!b->model || (b->endtime < cg.refreshTime))
			continue;

		if (cgMedia.heatBeamModel && b->model == cgMedia.heatBeamModel) {
			// if coming from the player, update the start position
			// entity 0 is the world
			if (b->entity == cg.playerNum+1) {	
				// set up gun position
				// code straight out of CG_AddViewWeapon
				ps = &cg.frame.playerState;
				oldframe = &cg.oldFrame;
				if (oldframe->serverFrame != cg.frame.serverFrame-1 || !oldframe->valid)
					oldframe = &cg.frame;		// previous frame was dropped or involid
				ops = &oldframe->playerState;
				for (j=0 ; j<3 ; j++) {
					b->start[j] = cg.refDef.viewOrigin[j];
				}
				Vec3MA (b->start,	(hand_multiplier * b->offset[0] * -1),	cg.refDef.viewAxis[1],	org);
				Vec3MA (org,		b->offset[1],							cg.refDef.viewAxis[0],	org);
				Vec3MA (org,		b->offset[2],							cg.refDef.viewAxis[2],	org);

				if (hand->intVal == 2)
					Vec3MA (org, -1, cg.refDef.viewAxis[2], org);

				// FIXME - take these out when final
				Vec3Negate (cg.refDef.viewAxis[1], right);
				Vec3Copy (cg.refDef.viewAxis[0], fwd);
				Vec3Copy (cg.refDef.viewAxis[2], up);

			}
			else
				Vec3Copy (b->start, org);
		}
		else {
			// if coming from the player, update the start position
			// entity 0 is the world
			if (b->entity == cg.playerNum+1) {
				Vec3Copy (cg.refDef.viewOrigin, b->start);
				b->start[2] -= 22;	// adjust for view height
			}
			Vec3Add (b->start, b->offset, org);
		}

		// calculate pitch and yaw
		Vec3Subtract (b->end, org, dist);

		//PMM
		if (cgMedia.heatBeamModel && b->model == cgMedia.heatBeamModel && b->entity == cg.playerNum+1) {
			float	len;

			len = Vec3Length (dist);
			Vec3Scale (fwd, len, dist);
			Vec3MA (dist, (hand_multiplier * b->offset[0]), right, dist);
			Vec3MA (dist, b->offset[1], fwd, dist);
			Vec3MA (dist, b->offset[2], up, dist);

			if (hand && (hand->intVal == 2))
				Vec3MA (org, -1, cg.refDef.viewAxis[2], org);
		}
		//PMM

		if ((dist[1] == 0) && (dist[0] == 0)) {
			yaw = 0;
			if (dist[2] > 0)
				pitch = 90;
			else
				pitch = 270;
		}
		else {
			// PMM - fixed to correct for pitch of 0
			if (dist[0])
				yaw = (atan2f(dist[1], dist[0]) * (180.0f / M_PI));
			else if (dist[1] > 0)
				yaw = 90;
			else
				yaw = 270;
			if (yaw < 0)
				yaw += 360;
	
			forward = sqrtf(dist[0]*dist[0] + dist[1]*dist[1]);
			pitch = (atan2f(dist[2], forward) * -(180.0f / M_PI));
			if (pitch < 0)
				pitch += 360.0;
		}

		framenum = 1;
		if (cgMedia.heatBeamModel && b->model == cgMedia.heatBeamModel) {
			if (b->entity != cg.playerNum+1) {
				framenum = 2;

				Vec3Set (angles, -pitch, yaw + 180.0, frand () * 360);
				Angles_Vectors (angles, fwd, right, up);
					
				// if it's a non-origin offset, it's a player, so use the hardcoded player offset
				if (!Vec3Compare (b->offset, vec3Origin)) {
					Vec3MA (org, -(b->offset[0])+1, right, org);
					Vec3MA (org, -(b->offset[1]), fwd, org);
					Vec3MA (org, -(b->offset[2])-10, up, org);
				}
				else {
					// if it's a monster, do the particle effect
					CG_MonsterPlasma_Shell (b->start);
				}
			}
			else {
				framenum = 1;
			}
		}

		// if it's the heatBeamModel, draw the particle effect
		if (cgMedia.heatBeamModel && b->model == cgMedia.heatBeamModel && b->entity == cg.playerNum+1)
			CG_Heatbeam (org, dist);

		// add new entities for the beams
		d = VectorNormalizef (dist, dist);

		memset (&ent, 0, sizeof(ent));
		if (b->model == cgMedia.heatBeamModel)
			model_length = 32.0;
		else if (b->model == cgMedia.lightningModel) {
			model_length = 35.0;
			d-= 20.0;  // correction so it doesn't end in middle of tesla
		}
		else
			model_length = 30.0;

		steps = ceil(d/model_length);
		len = (d-model_length)/(steps-1);

		// PMM - special case for lightning model .. if the real length is shorter than the model,
		// flip it around & draw it from the end to the start.  This prevents the model from going
		// through the tesla mine (instead it goes through the target)
		if (b->model == cgMedia.lightningModel && d <= model_length) {
			Vec3Copy (b->end, ent.origin);
			// offset to push beam outside of tesla model (negative because dist is from end to start
			// for this beam)
			ent.model = b->model;
			ent.flags = RF_FULLBRIGHT;
			ent.scale = 1;
			Vec3Set (angles, pitch, yaw, frand () * 360);
			if (angles[0] || angles[1] || angles[2])
				Angles_Matrix3 (angles, ent.axis);
			else
				Matrix3_Identity (ent.axis);
			Vec4Set (ent.color, 255, 255, 255, 255);
			cgi.R_AddEntity (&ent);			
			return;
		}

		while (d > 0) {
			Vec3Copy (org, ent.origin);
			ent.model = b->model;
			if (cgMedia.heatBeamModel && b->model == cgMedia.heatBeamModel) {
				ent.flags = RF_FULLBRIGHT;
				Vec3Set (angles, -pitch, yaw + 180.0, frand () * 360);
				ent.frame = framenum;
			}
			else if (b->model == cgMedia.lightningModel) {
				ent.flags = RF_FULLBRIGHT;
				Vec3Set (angles, -pitch, yaw + 180.0, frand () * 360);
			}
			else
				Vec3Set (angles, pitch, yaw, frand () * 360);

			ent.scale = 1;
			if (angles[0] || angles[1] || angles[2])
				Angles_Matrix3 (angles, ent.axis);
			else
				Matrix3_Identity (ent.axis);
			Vec4Set (ent.color, 255, 255, 255, 255);
			cgi.R_AddEntity (&ent);

			for (j=0 ; j<3 ; j++)
				org[j] += dist[j]*len;
			d -= model_length;
		}
	}
}


/*
=================
CG_ParseLightning
=================
*/
static int CG_ParseLightning (struct refModel_t *model)
{
	int			srcEnt, destEnt;
	vec3_t		start, end;
	teBeam_t	*b;
	int			i;
	
	srcEnt = cgi.MSG_ReadShort ();
	destEnt = cgi.MSG_ReadShort ();

	cgi.MSG_ReadPos (start);
	cgi.MSG_ReadPos (end);

	// override any beam with the same source AND destination entities
	for (i=0, b=cgBeams ; i<MAX_TENT_BEAMS ; i++, b++) {
		if (b->entity == srcEnt && b->dest_entity == destEnt) {
			b->entity = srcEnt;
			b->dest_entity = destEnt;
			b->model = model;
			b->endtime = cg.refreshTime + 200;
			Vec3Copy (start, b->start);
			Vec3Copy (end, b->end);
			Vec3Clear (b->offset);
			return srcEnt;
		}
	}

	// find a free beam
	for (i=0, b=cgBeams ; i<MAX_TENT_BEAMS ; i++, b++) {
		if (!b->model || (b->endtime < cg.refreshTime)) {
			b->entity = srcEnt;
			b->dest_entity = destEnt;
			b->model = model;
			b->endtime = cg.refreshTime + 200;
			Vec3Copy (start, b->start);
			Vec3Copy (end, b->end);
			Vec3Clear (b->offset);
			return srcEnt;
		}
	}

	Com_Printf (PRNT_WARNING, "beam list overflow!\n");	
	return srcEnt;
}

/*
=============================================================================

	LASER MANAGEMENT

=============================================================================
*/

/*
=================
CG_AddLasers
=================
*/
static void CG_AddLasers ()
{
	teLaser_t	*l;
	int			i, j, clr;
	vec3_t		length;

	for (i=0, l=cgLasers ; i<MAX_TENT_LASERS ; i++, l++) {
		if (l->endtime >= cg.refreshTime) {
			Vec3Subtract(l->ent.oldOrigin, l->ent.origin, length);

			clr = ((l->ent.skinNum >> ((rand () & 3)*8)) & 0xff);

			for (j=0 ; j<3 ; j++)
				CG_BeamTrail (l->ent.origin, l->ent.oldOrigin,
					clr, l->ent.frame, 0.33 + ((rand () & 1) * 0.1), -2);

			// outer
			CG_SpawnParticle (
				l->ent.origin[0],				l->ent.origin[1],				l->ent.origin[2],
				length[0],						length[1],						length[2],
				0,								0,								0,
				0,								0,								0,
				palRed (clr),					palGreen (clr),					palBlue (clr),
				palRed (clr),					palGreen (clr),					palBlue (clr),
				0.30f,							PART_INSTANT,
				l->ent.frame + ((l->ent.frame * 0.1f) * (rand () & 1)),
				l->ent.frame + ((l->ent.frame * 0.1f) * (rand () & 1)),
				PT_BEAM,						0,
				NULL,							NULL,							NULL,
				PART_STYLE_BEAM,
				0);
		}
	}
}


/*
=================
CG_ParseLaser
=================
*/
static void CG_ParseLaser (int colors)
{
	vec3_t		start, end;
	teLaser_t	*l;
	int			i, j, clr;
	vec3_t		length;

	cgi.MSG_ReadPos (start);
	cgi.MSG_ReadPos (end);

	for (i=0, l=cgLasers ; i<MAX_TENT_LASERS ; i++, l++) {
		if (l->endtime < cg.refreshTime) {
			Vec3Subtract(end, start, length);

			clr = ((colors >> ((rand () & 3)*8)) & 0xff);

			for (j=0 ; j<3 ; j++)
				CG_BeamTrail (start, end, clr, 2 + (rand () & 1), 0.30f, -2);

			// outer
			CG_SpawnParticle (
				start[0],							start[1],							start[2],
				length[0],							length[1],							length[2],
				0,									0,									0,
				0,									0,									0,
				palRed (clr),						palGreen (clr),						palBlue (clr),
				palRed (clr),						palGreen (clr),						palBlue (clr),
				0.30f,								-2.1f,
				4 + (0.4f * (rand () & 1)),			4 + (0.4f * (rand () & 1)),
				PT_BEAM,							0,
				NULL,								NULL,								NULL,
				PART_STYLE_BEAM,
				0);
			return;
		}
	}
}

/*
=============================================================================

	TENT MANAGEMENT

=============================================================================
*/

/*
=================
CG_AddTempEnts
=================
*/
void CG_AddTempEnts ()
{
	CG_AddBeams ();
	CG_AddPlayerBeams ();	// PMM - draw plasma beams
	CG_AddLasers ();

	CG_AddExploRattles ();
}


/*
=================
CG_ClearTempEnts
=================
*/
void CG_ClearTempEnts ()
{
	memset (cgBeams, 0, sizeof(cgBeams));
	memset (cgLasers, 0, sizeof(cgLasers));
	memset (cgPlayerBeams, 0, sizeof(cgPlayerBeams));

	CG_ClearExploRattles ();
}


/*
=================
CG_ParseTempEnt
=================
*/
void CG_ParseTempEnt ()
{
	int		type, cnt, color, r, ent, magnitude;
	vec3_t	pos, pos2, dir;

	type = cgi.MSG_ReadByte ();

	switch (type) {
	case TE_BLOOD:			// bullet hitting flesh
		cgi.MSG_ReadPos (pos);
		cgi.MSG_ReadDir (dir);
		CG_BleedEffect (pos, dir, 10);
		break;

	case TE_GUNSHOT:			// bullet hitting wall
	case TE_SPARKS:
	case TE_BULLET_SPARKS:
		cgi.MSG_ReadPos (pos);
		cgi.MSG_ReadDir (dir);

		if (type == TE_GUNSHOT)
			CG_RicochetEffect (pos, dir, 20);
		else
			CG_ParticleEffect (pos, dir, 0xe0, 9);

		if (type != TE_SPARKS) {
			// impact sound
			cnt = rand()&15;
			switch (cnt) {
			case 1:	cgi.Snd_StartSound (pos, 0, CHAN_AUTO, cgMedia.sfx.ricochet[0], 1, ATTN_NORM, 0);	break;
			case 2:	cgi.Snd_StartSound (pos, 0, CHAN_AUTO, cgMedia.sfx.ricochet[1], 1, ATTN_NORM, 0);	break;
			case 3:	cgi.Snd_StartSound (pos, 0, CHAN_AUTO, cgMedia.sfx.ricochet[2], 1, ATTN_NORM, 0);	break;
			default:
				break;
			}
		}

		break;
		
	case TE_SCREEN_SPARKS:
	case TE_SHIELD_SPARKS:
		cgi.MSG_ReadPos (pos);
		cgi.MSG_ReadDir (dir);
		if (type == TE_SCREEN_SPARKS)
			CG_ParticleEffect (pos, dir, 0xd0, 40);
		else
			CG_ParticleEffect (pos, dir, 0xb0, 40);

		cgi.Snd_StartSound (pos, 0, CHAN_AUTO, cgMedia.sfx.laserHit, 1, ATTN_NORM, 0);
		break;
		
	case TE_SHOTGUN:	// bullet hitting wall
		cgi.MSG_ReadPos (pos);
		cgi.MSG_ReadDir (dir);
		CG_RicochetEffect (pos, dir, 20);
		break;

	case TE_SPLASH:		// bullet hitting water
		cnt = cgi.MSG_ReadByte ();
		cgi.MSG_ReadPos (pos);
		cgi.MSG_ReadDir (dir);
		r = cgi.MSG_ReadByte ();
		if (r > 6)
			color = 0;
		else
			color = r;

		CG_SplashEffect (pos, dir, color, cnt);

		if (r == SPLASH_SPARKS) {
			r = (rand()%3);
			switch (r) {
			case 0:		cgi.Snd_StartSound (pos, 0, CHAN_AUTO, cgMedia.sfx.spark[4], 1, ATTN_STATIC, 0);	break;
			case 1:		cgi.Snd_StartSound (pos, 0, CHAN_AUTO, cgMedia.sfx.spark[5], 1, ATTN_STATIC, 0);	break;
			default:	cgi.Snd_StartSound (pos, 0, CHAN_AUTO, cgMedia.sfx.spark[6], 1, ATTN_STATIC, 0);	break;
			}
		}
		break;

	case TE_LASER_SPARKS:
		cnt = cgi.MSG_ReadByte ();
		cgi.MSG_ReadPos (pos);
		cgi.MSG_ReadDir (dir);
		color = cgi.MSG_ReadByte ();
		CG_ParticleEffect2 (pos, dir, color, cnt);
		break;

	case TE_BLUEHYPERBLASTER:
		cgi.MSG_ReadPos (pos);
		cgi.MSG_ReadPos (dir);
		CG_BlasterBlueParticles (pos, dir);
		break;

	case TE_BLASTER:	// blaster hitting wall
		cgi.MSG_ReadPos (pos);
		cgi.MSG_ReadDir (dir);
		CG_BlasterGoldParticles (pos, dir);
		cgi.Snd_StartSound (pos,  0, CHAN_AUTO, cgMedia.sfx.laserHit, 1, ATTN_NORM, 0);
		break;
		
	case TE_RAILTRAIL:	// railgun effect
		cgi.MSG_ReadPos (pos);
		cgi.MSG_ReadPos (pos2);

		CG_RailTrail (pos, pos2);
		cgi.Snd_StartSound (pos, 0, CHAN_AUTO, cgMedia.sfx.mz.railgunFireSfx, 1, ATTN_NORM, 0);
		break;

	case TE_PLASMA_EXPLOSION:
		cgi.MSG_ReadPos (pos);
		CG_ExplosionParticles (pos, 1, false, false);
		cgi.Snd_StartSound (pos, 0, CHAN_AUTO, cgMedia.sfx.rocketExplo, 1, ATTN_NORM, 0);
		break;
	
	case TE_BFG_EXPLOSION:
		cgi.MSG_ReadPos (pos);
		CG_ExplosionBFGEffect (pos);
		break;

	case TE_BFG_BIGEXPLOSION:
		cgi.MSG_ReadPos (pos);
		CG_ExplosionBFGParticles (pos);
		break;

	case TE_BFG_LASER:
		CG_ParseLaser (0xd0d1d2d3);
		break;

	case TE_BUBBLETRAIL:
		cgi.MSG_ReadPos (pos);
		cgi.MSG_ReadPos (pos2);
		CG_BubbleTrail (pos, pos2);
		break;

	case TE_PARASITE_ATTACK:
	case TE_MEDIC_CABLE_ATTACK:
		ent = CG_ParseBeam (cgMedia.parasiteSegmentModel);
		break;

	case TE_BOSSTPORT:	// boss teleporting to station
		cgi.MSG_ReadPos (pos);
		CG_BigTeleportParticles (pos);
		cgi.Snd_StartSound (pos, 0, CHAN_AUTO, cgMedia.sfx.bigTeleport, 1, ATTN_NONE, 0);
		break;

	case TE_GRAPPLE_CABLE:
		ent = CG_ParseBeam2 (cgMedia.grappleCableModel);
		break;

	case TE_WELDING_SPARKS:
		cnt = cgi.MSG_ReadByte ();
		cgi.MSG_ReadPos (pos);
		cgi.MSG_ReadDir (dir);
		color = cgi.MSG_ReadByte ();
		CG_ParticleEffect2 (pos, dir, color, cnt);

		CG_WeldingSparkFlash (pos);
		break;

	case TE_GREENBLOOD:
		cgi.MSG_ReadPos (pos);
		cgi.MSG_ReadDir (dir);
		CG_BleedEffect (pos, dir, 10, true);
		break;

	case TE_TUNNEL_SPARKS:
		cnt = cgi.MSG_ReadByte ();
		cgi.MSG_ReadPos (pos);
		cgi.MSG_ReadDir (dir);
		color = cgi.MSG_ReadByte ();
		CG_ParticleEffect3 (pos, dir, color, cnt);
		break;

	case TE_BLASTER2:	// green blaster hitting wall
	case TE_FLECHETTE:	// flechette
		cgi.MSG_ReadPos (pos);
		cgi.MSG_ReadDir (dir);
		
		if (type == TE_BLASTER2)
			CG_BlasterGreenParticles (pos, dir);
		else
			CG_BlasterGreyParticles (pos, dir);

		cgi.Snd_StartSound (pos,  0, CHAN_AUTO, cgMedia.sfx.laserHit, 1, ATTN_NORM, 0);
		break;

	case TE_LIGHTNING:
		ent = CG_ParseLightning (cgMedia.lightningModel);
		cgi.Snd_StartSound (NULL, ent, CHAN_WEAPON, cgMedia.sfx.lightning, 1, ATTN_NORM, 0);
		break;

	case TE_PLAIN_EXPLOSION:
		cgi.MSG_ReadPos (pos);

		if (type == TE_ROCKET_EXPLOSION_WATER) {
			cgi.Snd_StartSound (pos, 0, CHAN_AUTO, cgMedia.sfx.waterExplo, 1, ATTN_NORM, 0);
			CG_ExplosionParticles (pos, 1, false, true);
		}
		else {
			cgi.Snd_StartSound (pos, 0, CHAN_AUTO, cgMedia.sfx.rocketExplo, 1, ATTN_NORM, 0);
			CG_ExplosionParticles (pos, 1, false, false);
		}
		break;

	case TE_FLASHLIGHT:
		cgi.MSG_ReadPos (pos);
		ent = cgi.MSG_ReadShort ();
		CG_Flashlight (ent, pos);
		break;

	case TE_FORCEWALL:
		cgi.MSG_ReadPos (pos);
		cgi.MSG_ReadPos (pos2);
		color = cgi.MSG_ReadByte ();
		CG_ForceWall (pos, pos2, color);
		break;

	case TE_HEATBEAM:
		ent = CG_ParsePlayerBeam (cgMedia.heatBeamModel);
		break;

	case TE_MONSTER_HEATBEAM:
		ent = CG_ParsePlayerBeam (cgMedia.monsterHeatBeamModel);
		break;

	case TE_HEATBEAM_SPARKS:
		cnt = 50;
		cgi.MSG_ReadPos (pos);
		cgi.MSG_ReadDir (dir);
		r = 8;
		magnitude = 60;
		color = r & 0xff;
		CG_ParticleSteamEffect (pos, dir, color, cnt, magnitude);
		cgi.Snd_StartSound (pos,  0, CHAN_AUTO, cgMedia.sfx.laserHit, 1, ATTN_NORM, 0);
		break;
	
	case TE_HEATBEAM_STEAM:
		cgi.MSG_ReadPos (pos);
		cgi.MSG_ReadDir (dir);
		CG_ParticleSteamEffect (pos, dir, 0xe0, 20, 60);
		cgi.Snd_StartSound (pos,  0, CHAN_AUTO, cgMedia.sfx.laserHit, 1, ATTN_NORM, 0);
		break;

	case TE_STEAM:
		CG_ParseSteam ();
		break;

	case TE_BUBBLETRAIL2:
		cgi.MSG_ReadPos (pos);
		cgi.MSG_ReadPos (pos2);

		CG_BubbleTrail2 (pos, pos2, 8);

		cgi.Snd_StartSound (pos,  0, CHAN_AUTO, cgMedia.sfx.laserHit, 1, ATTN_NORM, 0);
		break;

	case TE_MOREBLOOD:
		cgi.MSG_ReadPos (pos);
		cgi.MSG_ReadDir (dir);
		CG_BleedEffect (pos, dir, 50);
		break;

	case TE_CHAINFIST_SMOKE:
		Vec3Set (dir, 0, 0, 1);

		cgi.MSG_ReadPos (pos);

		CG_ParticleSmokeEffect (pos, dir, 0, 20, 20);
		break;

	case TE_ELECTRIC_SPARKS:
		cgi.MSG_ReadPos (pos);
		cgi.MSG_ReadDir (dir);

		CG_ParticleEffect (pos, dir, 0x75, 40);

		cgi.Snd_StartSound (pos, 0, CHAN_AUTO, cgMedia.sfx.laserHit, 1, ATTN_NORM, 0);
		break;

	case TE_TRACKER_EXPLOSION:
		cgi.MSG_ReadPos (pos);

		CG_ColorFlash (pos, 0, 150, -1, -1, -1);
		CG_ExplosionColorParticles (pos);

		cgi.Snd_StartSound (pos, 0, CHAN_AUTO, cgMedia.sfx.disruptExplo, 1, ATTN_NORM, 0);
		break;

	case TE_TELEPORT_EFFECT:
	case TE_DBALL_GOAL:
		cgi.MSG_ReadPos (pos);
		CG_TeleportParticles (pos);
		break;

	case TE_WIDOWBEAMOUT:
		CG_ParseWidow ();
		break;

	case TE_NUKEBLAST:
		CG_ParseNuke ();
		break;

	case TE_WIDOWSPLASH:
		cgi.MSG_ReadPos (pos);
		CG_WidowSplash (pos);
		break;

	default:
		Com_Error (ERR_DROP, "CG_ParseTempEnt: bad type");
	}
}
