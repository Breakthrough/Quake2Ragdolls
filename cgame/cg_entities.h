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
// cg_entities.h
//

/*
=============================================================================

	ENTITY

=============================================================================
*/

enum groundState_t
{
	GS_UNKNOWN, // force a check
	GS_ONGROUND,
	GS_INAIR
};

struct cgAnimation_t
{
	float prevFrameTime;
	float nextFrameTime;

	int	oldFrame;
	int curFrame;

	float backLerp;
	int curAnim;
	int forcedAnimation;
	bool startForcedAnimation;
	groundState_t groundState;

	void Clear ();
	void Update (refEntity_t &entity, entityState_t *ps, entityState_t *ops);
	void UpdatePlayer (refEntity_t &entity, entityState_t *ps, entityState_t *ops);
};

struct cgEntity_t {
	entityState_t	baseLine;		// delta from this if not from a previous frame
	entityState_t	current;
	entityState_t	prev;			// will always be valid, but might just be a copy of current

	int				serverFrame;		// if not current, this ent isn't in the frame

	vec3_t			lerpOrigin;		// for trails (variable hz)

	int				flyStopTime;

	bool			muzzleOn;
	bool			muzzSilenced;
	bool			muzzVWeap;

	cgAnimation_t	animation;
	bool			newEntity;
	vec3_t			oldOrigin, effectOrigin;

	void Clear ()
	{
		baseLine.Clear();
		current.Clear();
		prev.Clear();
		
		serverFrame = 0;
		newEntity = true;
		Vec3Clear(lerpOrigin);
		Vec3Clear(oldOrigin);
		Vec3Clear(effectOrigin);
		flyStopTime = 0;

		muzzleOn = muzzSilenced = muzzVWeap = false;
		animation.Clear();
	}
};

extern cgEntity_t		cg_entityList[MAX_CS_EDICTS];

// the cg_parseEntities must be large enough to hold UPDATE_BACKUP frames of
// entities, so that when a delta compressed message arives from the server
// it can be un-deltad from the original
extern entityState_t	cg_parseEntities[MAX_PARSE_ENTITIES];

//
// cg_entities.c
//

void		CG_BeginFrameSequence (netFrame_t frame);
void		CG_NewPacketEntityState (int entNum, entityState_t &state);
void		CG_EndFrameSequence (int numEntities);

void		CG_AddEntities ();
void		CG_ClearEntities ();

void		CG_GetEntitySoundOrigin (int entNum, vec3_t origin, vec3_t velocity);

//
// cg_localents.c
//

typedef enum leType_s {
	LE_MGSHELL,
	LE_SGSHELL
} leType_t;

void		CG_ClearLocalEnts ();
void		CG_AddLocalEnts ();

/*

bool		CG_SpawnLocalEnt (float org0,					float org1,						float org2,
							float vel0,						float vel1,						float vel2,
							float angle0,					float angle1,					float angle2,
							float avel0,					float avel1,					float avel2,
							float parm0,					float parm1,					float parm2,
							leType_t type);

							*/

#include "cg_localents.h"

//
// cg_tempents.c
//

void		CG_ExploRattle (vec3_t org, float scale);

void		CG_AddTempEnts ();
void		CG_ClearTempEnts ();
void		CG_ParseTempEnt ();

//
// cg_weapon.c
//

void		CG_AddViewWeapon ();

void		CG_WeapRegister ();
void		CG_WeapUnregister ();
