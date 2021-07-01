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
// cg_muzzleflash.c
//

#include "cg_local.h"

/*
==============
CG_BrassHack
==============
*/
static void CG_BrassHack (cgEntity_t *pl, leType_t type, int count)
{
	vec3_t	forward, right;
	int		i;

	Angles_Vectors (pl->current.angles, forward, right, NULL);
	Vec3Scale (forward, 14, forward);
	Vec3Scale (right, 14, right);

	for (i=0 ; i<count ; i++)
	{
		vec3_t origin, angles, vel, avel;

		Vec3Set(origin, pl->current.origin[0] + forward[0] + right[0] + crand (),
						pl->current.origin[1] + forward[1] + right[1] + crand (),
						pl->current.origin[2] + forward[2] + right[2] + crand ());

		Vec3Set(vel, (forward[0] * ((frand () * 2.5f) + 0.5f)) + (crand () * 45.0f),
						(forward[1] * ((frand () * 2.5f) + 0.5f)) + (crand () * 45.0f),
						(forward[2] * ((frand () * 2.5f) + 0.5f)) + (crand () * 45.0f) + 144);

		Vec3Set(angles, frand () * 360,				frand () * 360,				frand () * 360);

		Vec3Set(avel, crand () * 1000,			crand () * 1000,			crand () * 1000);

		var brass = new brassLocalEnt_t(origin, Q_BColorWhite, angles, avel, vel, 
						750.f + (crand () * .50));
		brass->type = type;

		switch (type) {
		case LE_MGSHELL:
			brass->SetModel(cgMedia.brassMGModel);
			brass->refEnt.flags |= RF_FULLBRIGHT;
			break;

		case LE_SGSHELL:
			brass->SetModel(cgMedia.brassSGModel);
			brass->refEnt.flags |= RF_FULLBRIGHT;
			break;
		}
	}
}

#include "../game/game.h"

// matches one from g_weapon
static void fire_lead (entityState_t *state, vec3_t start, vec3_t aimdir, int te_impact, int hspread, int vspread, MTwister &generator)
{
	cmTrace_t	tr;
	vec3_t		dir;
	vec3_t		forward, right, up;
	vec3_t		end;
	float		r;
	float		u;
	vec3_t		water_start;
	bool		water = false;
	int			content_mask = CONTENTS_MASK_SHOT | CONTENTS_MASK_WATER;

	CG_Trace(&tr, state->origin, NULL, NULL, start, true, true, state->number, CONTENTS_MASK_SHOT);
	if (!(tr.fraction < 1.0))
	{
		VecToAngles (aimdir, dir);
		Angles_Vectors (dir, forward, right, up);

		r = generator.CRandom()*hspread;
		u = generator.CRandom()*vspread;
		Vec3MA (start, 8192, forward, end);
		Vec3MA (end, r, right, end);
		Vec3MA (end, u, up, end);

		if (CG_PMPointContents(start) & CONTENTS_MASK_WATER)
		{
			water = true;
			Vec3Copy (start, water_start);
			content_mask &= ~CONTENTS_MASK_WATER;
		}

		CG_Trace (&tr, start, NULL, NULL, end, true, true, state->number, content_mask);

		// see if we hit water
		if (tr.contents & CONTENTS_MASK_WATER)
		{
			//PhysRayCast	(start, tr.endPos, kick);
			water = true;
			Vec3Copy (tr.endPos, water_start);

			if (!Vec3Compare (start, tr.endPos))
			{
				int color;
				if (tr.contents & CONTENTS_WATER)
				{
					if (strcmp(tr.surface->name, "*brwater") == 0)
						color = SPLASH_BROWN_WATER;
					else
						color = SPLASH_BLUE_WATER;
				}
				else if (tr.contents & CONTENTS_SLIME)
					color = SPLASH_SLIME;
				else if (tr.contents & CONTENTS_LAVA)
					color = SPLASH_LAVA;
				else
					color = SPLASH_UNKNOWN;

				if (color != SPLASH_UNKNOWN)
				{
					/*gi.WriteByte (SVC_TEMP_ENTITY);
					gi.WriteByte (TE_SPLASH);
					gi.WriteByte (8);
					gi.WritePosition (tr.endPos);
					gi.WriteDir (tr.plane.normal);
					gi.WriteByte (color);
					gi.multicast (tr.endPos, MULTICAST_PVS);*/
					CG_SplashEffect (tr.endPos, tr.plane.normal, color, 8);
				}

				// change bullet's course when it enters water
				Vec3Subtract (end, start, dir);
				VecToAngles (dir, dir);
				Angles_Vectors (dir, forward, right, up);
				r = generator.CRandom()*hspread*2;
				u = generator.CRandom()*vspread*2;
				Vec3MA (water_start, 8192, forward, end);
				Vec3MA (end, r, right, end);
				Vec3MA (end, u, up, end);
			}

			// re-trace ignoring water this time
			CG_Trace (&tr, water_start, NULL, NULL, end, true, true, state->number, CONTENTS_MASK_SHOT);
			//PhysRayCast	(start, tr.endPos, kick);
		}
		else
		{
			//PhysRayCast	(start, tr.endPos, kick);
		}
	}

	// send gun puff / flash
	if (!((tr.surface) && (tr.surface->flags & SURF_TEXINFO_SKY)))
	{
		if (tr.fraction < 1.0)
		{
			//if (tr.ent)
			//{
			//	T_Damage (tr.ent, self, self, aimdir, tr.endPos, tr.plane.normal, damage, kick, DAMAGE_BULLET, mod);
			//}
			//else
			if ((tr.ent != (edict_t*)1 && tr.ent->s.solid == 31) || tr.ent == (edict_t*)1)
			{
				if (strncmp (tr.surface->name, "sky", 3) != 0)
				{
					/*gi.WriteByte (SVC_TEMP_ENTITY);
					gi.WriteByte (te_impact);
					gi.WritePosition (tr.endPos);
					gi.WriteDir (tr.plane.normal);
					gi.multicast (tr.endPos, MULTICAST_PVS);*/
					CG_RicochetEffect (tr.endPos, tr.plane.normal, 20);

					uint32 cnt = rand() % ((te_impact == TE_SHOTGUN) ? 16 : 8);
				switch (cnt) {
					case 1:	cgi.Snd_StartSound (tr.endPos, 0, CHAN_AUTO, cgMedia.sfx.ricochet[0], 1, ATTN_NORM, 0);	break;
					case 2:	cgi.Snd_StartSound (tr.endPos, 0, CHAN_AUTO, cgMedia.sfx.ricochet[1], 1, ATTN_NORM, 0);	break;
					case 3:	cgi.Snd_StartSound (tr.endPos, 0, CHAN_AUTO, cgMedia.sfx.ricochet[2], 1, ATTN_NORM, 0);	break;
					default:
						break;
					}
				}
			}
		}
	}

	// if went through water, determine where the end and make a bubble trail
	/*
	if (water)
	{
		vec3_t	pos;

		Vec3Subtract (tr.endPos, water_start, dir);
		VectorNormalizef (dir, dir);
		Vec3MA (tr.endPos, -2, dir, pos);
		if (CG_PMPointContents (pos) & CONTENTS_MASK_WATER)
			Vec3Copy (pos, tr.endPos);
		else
			CG_Trace (&tr, pos, NULL, NULL, water_start, true, true, tr.ent->s.number, CONTENTS_MASK_WATER);

		Vec3Add (water_start, tr.endPos, pos);
		Vec3Scale (pos, 0.5, pos);

		gi.WriteByte (SVC_TEMP_ENTITY);
		gi.WriteByte (TE_BUBBLETRAIL);
		gi.WritePosition (water_start);
		gi.WritePosition (tr.endPos);
		gi.multicast (pos, MULTICAST_PVS);
	}
	*/
}

/*
==============
CG_ParseMuzzleFlash
==============
*/
void CG_ParseMuzzleFlash (entityState_t *ent, int eventID)
{
	cgDLight_t	*dl;
	cgEntity_t	*pl;
	vec3_t		fv, rv;
	int			silenced;
	float		volume;
	short flashNum = ent->events[eventID].parms.ToByte(0);

	silenced = flashNum & MZ_SILENCED;
	flashNum &= ~MZ_SILENCED;

	pl = &cg_entityList[ent->number];

	dl = CG_AllocDLight (ent->number);
	Vec3Copy (pl->current.origin, dl->origin);
	Angles_Vectors (pl->current.angles, fv, rv, NULL);
	Vec3MA (dl->origin, 18, fv, dl->origin);
	Vec3MA (dl->origin, 16, rv, dl->origin);

	pl->muzzleOn = true;
	pl->muzzVWeap = false;

	if (silenced) {
		pl->muzzSilenced = true;
		volume = 0.2f;
		dl->radius = 100 + (frand () * 32);
	}
	else {
		pl->muzzSilenced = false;
		volume = 1;
		dl->radius = 200 + (frand () * 32);
	}

	dl->minlight = 32;
	dl->die = cg.refreshTime;

	// Sup hack
	switch (flashNum) {
	case MZ_SHOTGUN:
		CG_BrassHack (pl, LE_SGSHELL, 1);
		break;

	case MZ_SSHOTGUN:
		CG_BrassHack (pl, LE_SGSHELL, 2);
		break;

	case MZ_MACHINEGUN:
	case MZ_CHAINGUN1:
	case MZ_CHAINGUN2:
	case MZ_CHAINGUN3:
		CG_BrassHack (pl, LE_MGSHELL, 1);
		break;
	}

	switch (flashNum)
	{
	// MZ_BLASTER
	case MZ_BLASTER:
		Vec3Set (dl->color, 1, 1, 0);
		cgi.Snd_StartSound (NULL, ent->number, CHAN_WEAPON, cgMedia.sfx.mz.blasterFireSfx, volume, ATTN_NORM, 0);
		break;

	// MZ_BLUEHYPERBLASTER
	case MZ_BLUEHYPERBLASTER:
		Vec3Set (dl->color, 0, 0, 1);
		cgi.Snd_StartSound (NULL, ent->number, CHAN_WEAPON, cgMedia.sfx.mz.hyperBlasterFireSfx, volume, ATTN_NORM, 0);
		break;

	// MZ_HYPERBLASTER
	case MZ_HYPERBLASTER:
		Vec3Set (dl->color, 1, 1, 0);
		cgi.Snd_StartSound (NULL, ent->number, CHAN_WEAPON, cgMedia.sfx.mz.hyperBlasterFireSfx, volume, ATTN_NORM, 0);
		break;

	// MZ_MACHINEGUN
	case MZ_MACHINEGUN:
		Vec3Set (dl->color, 1, 1, 0);
		cgi.Snd_StartSound (NULL, ent->number, CHAN_WEAPON, cgMedia.sfx.mz.machineGunSfx[(rand () % 5)], volume, ATTN_NORM, 0);

		{
			vec3_t pos, angles;
			int viewHeight;
			byte handVal;
			int evPos = 1;

			/*if (ent->number-1 == cg.playerNum)
			{
				Vec3Copy (cg.predicted.origin, pos);
				Vec3Copy (cg.predicted.angles, angles);
				viewHeight = cg.predicted.viewHeight;
				handVal = hand->intVal;

				evPos += sizeof(vec3_t) + 2;
			}
			else*/
			{
				Vec3Copy(ent->origin, pos);
				ent->events[eventID].parms.ToVector(evPos, angles);
				evPos += sizeof(vec3_t);
				viewHeight = ent->events[eventID].parms.ToChar(evPos);
				evPos++;
				handVal = ent->events[eventID].parms.ToByte(evPos);
				evPos++;
			}

			ushort seed = (ushort)ent->events[eventID].parms.ToShort(evPos);

			MTwister generator (seed);

			vec3_t offset, forward, right;
			Angles_Vectors (angles, forward, right, NULL);
			Vec3Set (offset, 0, 8, viewHeight-8);

			vec3_t start;
			CG_ProjectSource (handVal, pos, offset, forward, right, start);

			fire_lead(ent, start, forward, TE_GUNSHOT, DEFAULT_BULLET_HSPREAD, DEFAULT_BULLET_VSPREAD, generator);
		}

		break;

	// MZ_SHOTGUN
	case MZ_SHOTGUN:
		Vec3Set (dl->color, 1, 1, 0);
		cgi.Snd_StartSound (NULL, ent->number, CHAN_WEAPON, cgMedia.sfx.mz.shotgunFireSfx, volume, ATTN_NORM, 0);
		cgi.Snd_StartSound (NULL, ent->number, CHAN_AUTO, cgMedia.sfx.mz.shotgunReloadSfx, volume, ATTN_NORM, 0.1f);

		{
			vec3_t pos, angles;
			int viewHeight;
			byte handVal;
			int evPos = 1;

			/*if (ent->number-1 == cg.playerNum)
			{
				Vec3Copy (cg.predicted.origin, pos);
				Vec3Copy (cg.predicted.angles, angles);
				viewHeight = cg.predicted.viewHeight;
				handVal = hand->intVal;

				evPos += sizeof(vec3_t) + 2;
			}
			else*/
			{
				Vec3Copy(ent->origin, pos);
				ent->events[eventID].parms.ToVector(evPos, angles);
				evPos += sizeof(vec3_t);
				viewHeight = ent->events[eventID].parms.ToChar(evPos);
				evPos++;
				handVal = ent->events[eventID].parms.ToByte(evPos);
				evPos++;
			}

			ushort seed = (ushort)ent->events[eventID].parms.ToShort(evPos);

			MTwister generator (seed);

			vec3_t offset, forward, right;
			Angles_Vectors (angles, forward, right, NULL);
			Vec3Set (offset, 0, 8, viewHeight-8);

			vec3_t start;
			CG_ProjectSource (handVal, pos, offset, forward, right, start);

			for (int i = 0; i < DEFAULT_SHOTGUN_COUNT; ++i)
				fire_lead(ent, start, forward, TE_SHOTGUN, 500, 500, generator);
		}

		break;

	// MZ_SSHOTGUN
	case MZ_SSHOTGUN:
		Vec3Set (dl->color, 1, 1, 0);
		cgi.Snd_StartSound (NULL, ent->number, CHAN_WEAPON, cgMedia.sfx.mz.superShotgunFireSfx, volume, ATTN_NORM, 0);

		{
			vec3_t pos, angles;
			int viewHeight;
			byte handVal;
			int evPos = 1;

			/*if (ent->number-1 == cg.playerNum)
			{
				Vec3Copy (cg.predicted.origin, pos);
				Vec3Copy (cg.predicted.angles, angles);
				viewHeight = cg.predicted.viewHeight;
				handVal = hand->intVal;

				evPos += sizeof(vec3_t) + 2;
			}
			else*/
			{
				Vec3Copy(ent->origin, pos);
				ent->events[eventID].parms.ToVector(evPos, angles);
				evPos += sizeof(vec3_t);
				viewHeight = ent->events[eventID].parms.ToChar(evPos);
				evPos++;
				handVal = ent->events[eventID].parms.ToByte(evPos);
				evPos++;
			}

			ushort seed = (ushort)ent->events[eventID].parms.ToShort(evPos);

			MTwister generator (seed);

			vec3_t offset, forward, right;
			Angles_Vectors (angles, forward, right, NULL);
			Vec3Set (offset, 0, 8, viewHeight-8);

			vec3_t start;
			CG_ProjectSource (handVal, pos, offset, forward, right, start);

			vec3_t v;

			v[PITCH] = angles[PITCH];
			v[YAW]   = angles[YAW] - 5;
			v[ROLL]  = angles[ROLL];
			Angles_Vectors (v, forward, NULL, NULL);
			for (int i = 0; i < DEFAULT_SSHOTGUN_COUNT / 2; ++i)
				fire_lead(ent, start, forward, TE_SHOTGUN, DEFAULT_SHOTGUN_HSPREAD, DEFAULT_SHOTGUN_VSPREAD, generator);
			v[YAW]   = angles[YAW] + 5;
			Angles_Vectors (v, forward, NULL, NULL);
			for (int i = 0; i < DEFAULT_SSHOTGUN_COUNT / 2; ++i)
				fire_lead(ent, start, forward, TE_SHOTGUN, DEFAULT_SHOTGUN_HSPREAD, DEFAULT_SHOTGUN_VSPREAD, generator);
		}

		break;

	case MZ_CHAINGUN1:
	case MZ_CHAINGUN2:
	case MZ_CHAINGUN3:
		{
			int shots = flashNum - 2;
			Vec3Set(dl->color, 1, 1.0f / shots, 0);
			dl->radius = 200 + (25 * (shots - 1));
			
			if (shots >= 2)
				dl->die = cg.refreshTime + 0.1f;

			for (int i = 0; i < shots; ++i)
				cgi.Snd_StartSound (NULL, ent->number, CHAN_WEAPON, cgMedia.sfx.mz.machineGunSfx[(rand () % 5)], volume, ATTN_NORM, 
				(i == 0) ? 0 :
				(i == 1) ? 0.033f :
				0.066f);

			{
				vec3_t pos, angles;
				int viewHeight;
				byte handVal;
				int evPos = 1;

				/*if (ent->number-1 == cg.playerNum)
				{
					Vec3Copy (cg.predicted.origin, pos);
					Vec3Copy (cg.predicted.angles, angles);
					viewHeight = cg.predicted.viewHeight;
					handVal = hand->intVal;

					evPos += sizeof(vec3_t) + 2;
				}
				else*/
				{
					Vec3Copy(ent->origin, pos);
					ent->events[eventID].parms.ToVector(evPos, angles);
					evPos += sizeof(vec3_t);
					viewHeight = ent->events[eventID].parms.ToChar(evPos);
					evPos++;
					handVal = ent->events[eventID].parms.ToByte(evPos);
					evPos++;
				}

				ushort seed = (ushort)ent->events[eventID].parms.ToShort(evPos);

				MTwister generator (seed);

				vec3_t forward, right, up, offset, start;
				for (int i=0; i<shots ; i++)
				{
					// get start / end positions
					Angles_Vectors (angles, forward, right, up);
					float r = 7 + generator.CRandom()*4;
					float u = generator.CRandom()*4;
					Vec3Set (offset, 0, r, u + viewHeight-8);
					CG_ProjectSource (handVal, pos, offset, forward, right, start);

					fire_lead(ent, start, forward, TE_SHOTGUN, DEFAULT_BULLET_HSPREAD, DEFAULT_BULLET_VSPREAD, twister);
				}
			}
		}
		break;

	// MZ_RAILGUN
	case MZ_RAILGUN:
		Vec3Set (dl->color, 0.5, 0.5, 1);
		cgi.Snd_StartSound (NULL, ent->number, CHAN_WEAPON, cgMedia.sfx.mz.railgunFireSfx, volume, ATTN_NORM, 0);

		{
			vec3_t pos, angles;
			int viewHeight;
			byte handVal;
			int evPos = 1;

			/*if (ent->number-1 == cg.playerNum)
			{
				Vec3Copy (cg.predicted.origin, pos);
				Vec3Copy (cg.predicted.angles, angles);
				viewHeight = cg.predicted.viewHeight;
				handVal = hand->intVal;
			}
			else*/
			{
				Vec3Copy(ent->origin, pos);
				ent->events[eventID].parms.ToVector(evPos, angles);
				evPos += sizeof(vec3_t);
				viewHeight = ent->events[eventID].parms.ToChar(evPos);
				evPos++;
				handVal = ent->events[eventID].parms.ToByte(evPos);
				evPos++;
			}

			vec3_t offset, forward, right;
			Angles_Vectors (angles, forward, right, NULL);
			Vec3Set (offset, 0, 7, viewHeight-8);
			
			vec3_t start;
			CG_ProjectSource (handVal, pos, offset, forward, right, start);
			vec3_t end;

			Vec3MA (start, 8192, forward, end);

			cmTrace_t tr;
			CG_PMTrace(&tr, start, vec3Origin, vec3Origin, end, false, true);

			if (ent->number-1 == cg.playerNum)
			{
				Angles_Vectors (cg.predicted.angles, forward, right, NULL);
				CG_ProjectSource (handVal, cg.predicted.origin, offset, forward, right, start);
			}

			CG_RailTrail(start, tr.endPos);
		}
		break;

	// MZ_ROCKET
	case MZ_ROCKET:
		CG_RocketFireParticles (pl->current.origin, pl->current.angles);
		Vec3Set (dl->color, 1, 0.5f, 0.2f);
		cgi.Snd_StartSound (NULL, ent->number, CHAN_WEAPON, cgMedia.sfx.mz.rocketFireSfx, volume, ATTN_NORM, 0);
		cgi.Snd_StartSound (NULL, ent->number, CHAN_AUTO, cgMedia.sfx.mz.rocketReloadSfx, volume, ATTN_NORM, 0.1f);
		break;

	// MZ_GRENADE
	case MZ_GRENADE:
		Vec3Set (dl->color, 1, 0.5f, 0);
		cgi.Snd_StartSound (NULL, ent->number, CHAN_WEAPON, cgMedia.sfx.mz.grenadeFireSfx, volume, ATTN_NORM, 0);
		cgi.Snd_StartSound (NULL, ent->number, CHAN_AUTO, cgMedia.sfx.mz.grenadeReloadSfx, volume, ATTN_NORM, 0.1f);
		break;

	// MZ_BFG
	case MZ_BFG:
		Vec3Set (dl->color, 0, 1, 0);
		cgi.Snd_StartSound (NULL, ent->number, CHAN_WEAPON, cgMedia.sfx.mz.bfgFireSfx, volume, ATTN_NORM, 0);
		break;

	// MZ_LOGIN
	case MZ_LOGIN:
		Vec3Set (dl->color, 0, 1, 0);
		dl->die = cg.refreshTime + 1.0;
		cgi.Snd_StartSound (NULL, ent->number, CHAN_WEAPON, cgMedia.sfx.mz.grenadeFireSfx, 1, ATTN_NORM, 0);
		CG_LogoutEffect (pl->current.origin, flashNum);
		break;

	// MZ_LOGOUT
	case MZ_LOGOUT:
		Vec3Set (dl->color, 1, 0, 0);
		dl->die = cg.refreshTime + 1.0;
		cgi.Snd_StartSound (NULL, ent->number, CHAN_WEAPON, cgMedia.sfx.mz.grenadeFireSfx, 1, ATTN_NORM, 0);
		CG_LogoutEffect (pl->current.origin, flashNum);
		break;

	// MZ_RESPAWN
	case MZ_RESPAWN:
		Vec3Set (dl->color, 1, 1, 0);
		dl->die = cg.refreshTime + 1.0;
		cgi.Snd_StartSound (NULL, ent->number, CHAN_WEAPON, cgMedia.sfx.mz.grenadeFireSfx, 1, ATTN_NORM, 0);
		CG_LogoutEffect (pl->current.origin, flashNum);
		break;

	// RAFAEL
	// MZ_PHALANX
	case MZ_PHALANX:
		Vec3Set (dl->color, 1, 0.5, 0.5);
		cgi.Snd_StartSound (NULL, ent->number, CHAN_WEAPON, cgMedia.sfx.mz.phalanxFireSfx, volume, ATTN_NORM, 0);
		break;
	// RAFAEL

	// MZ_IONRIPPER
	case MZ_IONRIPPER:
		Vec3Set (dl->color, 1, 0.5, 0.5);
		cgi.Snd_StartSound (NULL, ent->number, CHAN_WEAPON, cgMedia.sfx.mz.ionRipperFireSfx, volume, ATTN_NORM, 0);
		break;

	// PGM
	// MZ_ETF_RIFLE
	case MZ_ETF_RIFLE:
		Vec3Set (dl->color, 0.9f, 0.7f, 0);
		cgi.Snd_StartSound (NULL, ent->number, CHAN_WEAPON, cgMedia.sfx.mz.etfRifleFireSfx, volume, ATTN_NORM, 0);
		break;

	// MZ_SHOTGUN2
	case MZ_SHOTGUN2:
		Vec3Set (dl->color, 1, 1, 0);
		cgi.Snd_StartSound (NULL, ent->number, CHAN_WEAPON, cgMedia.sfx.mz.shotgun2FireSfx, volume, ATTN_NORM, 0);
		break;

	// MZ_HEATBEAM
	case MZ_HEATBEAM:
		Vec3Set (dl->color, 1, 1, 0);
		dl->die = cg.refreshTime + 100;
		break;

	// MZ_BLASTER2
	case MZ_BLASTER2:
		Vec3Set (dl->color, 0, 1, 0);
		cgi.Snd_StartSound (NULL, ent->number, CHAN_WEAPON, cgMedia.sfx.mz.blasterFireSfx, volume, ATTN_NORM, 0);
		break;

	// MZ_TRACKER
	case MZ_TRACKER:
		Vec3Set (dl->color, -1, -1, -1);
		cgi.Snd_StartSound (NULL, ent->number, CHAN_WEAPON, cgMedia.sfx.mz.trackerFireSfx, volume, ATTN_NORM, 0);
		break;

	// MZ_NUKE1
	case MZ_NUKE1:
		Vec3Set (dl->color, 1, 0, 0);
		dl->die = cg.refreshTime + 100;
		break;

	// MZ_NUKE2
	case MZ_NUKE2:
		Vec3Set (dl->color, 1, 1, 0);
		dl->die = cg.refreshTime + 100;
		break;

	// MZ_NUKE4
	case MZ_NUKE4:
		Vec3Set (dl->color, 0, 0, 1);
		dl->die = cg.refreshTime + 100;
		break;

	// MZ_NUKE8
	case MZ_NUKE8:
		Vec3Set (dl->color, 0, 1, 1);
		dl->die = cg.refreshTime + 100;
		break;
	// PGM
	}

	switch (flashNum)
	{
	case MZ_BLASTER:
	case MZ_SHOTGUN:
	case MZ_SSHOTGUN:
	case MZ_MACHINEGUN:
	case MZ_CHAINGUN1:
	case MZ_CHAINGUN2:
	case MZ_CHAINGUN3:
	case MZ_GRENADE:
	case MZ_ROCKET:
	case MZ_HYPERBLASTER:
	case MZ_RAILGUN:
	case MZ_BFG:
		cg_entityList[ent->number].animation.forcedAnimation = (ent->animation & 2) ? 16 : 2;
		cg_entityList[ent->number].animation.startForcedAnimation = true;
		break;
	}
}


/*
==============
CG_ParseMuzzleFlash2
==============
*/
void CG_ParseMuzzleFlash2 ()
{
	cgDLight_t	*dl;
	vec3_t		origin, forward, right;
	int			entNum, flashNum;

	entNum = cgi.MSG_ReadShort ();
	if (entNum < 1 || entNum >= MAX_CS_EDICTS)
		Com_Error (ERR_DROP, "CG_ParseMuzzleFlash2: bad entity");
	flashNum = cgi.MSG_ReadByte ();

	// locate the origin
	Angles_Vectors (cg_entityList[entNum].current.angles, forward, right, NULL);
	origin[0] = cg_entityList[entNum].current.origin[0] + forward[0] * dumb_and_hacky_monster_MuzzFlashOffset[flashNum][0] + right[0] * dumb_and_hacky_monster_MuzzFlashOffset[flashNum][1];
	origin[1] = cg_entityList[entNum].current.origin[1] + forward[1] * dumb_and_hacky_monster_MuzzFlashOffset[flashNum][0] + right[1] * dumb_and_hacky_monster_MuzzFlashOffset[flashNum][1];
	origin[2] = cg_entityList[entNum].current.origin[2] + forward[2] * dumb_and_hacky_monster_MuzzFlashOffset[flashNum][0] + right[2] * dumb_and_hacky_monster_MuzzFlashOffset[flashNum][1] + dumb_and_hacky_monster_MuzzFlashOffset[flashNum][2];

	dl = CG_AllocDLight (entNum);
	Vec3Copy (origin,  dl->origin);
	dl->radius = 200 + (rand()&31);
	dl->minlight = 32;
	dl->die = cg.refreshTime;

	switch (flashNum) {
	case MZ2_INFANTRY_MACHINEGUN_1:
	case MZ2_INFANTRY_MACHINEGUN_2:
	case MZ2_INFANTRY_MACHINEGUN_3:
	case MZ2_INFANTRY_MACHINEGUN_4:
	case MZ2_INFANTRY_MACHINEGUN_5:
	case MZ2_INFANTRY_MACHINEGUN_6:
	case MZ2_INFANTRY_MACHINEGUN_7:
	case MZ2_INFANTRY_MACHINEGUN_8:
	case MZ2_INFANTRY_MACHINEGUN_9:
	case MZ2_INFANTRY_MACHINEGUN_10:
	case MZ2_INFANTRY_MACHINEGUN_11:
	case MZ2_INFANTRY_MACHINEGUN_12:
	case MZ2_INFANTRY_MACHINEGUN_13:
		Vec3Set (dl->color, 1, 1, 0);
		CG_ParticleEffect (origin, vec3Origin, 0, 40);
		cgi.Snd_StartSound (NULL, entNum, CHAN_WEAPON, cgMedia.sfx.mz2.machGunSfx, 1, ATTN_NORM, 0);
		break;

	case MZ2_SOLDIER_MACHINEGUN_1:
	case MZ2_SOLDIER_MACHINEGUN_2:
	case MZ2_SOLDIER_MACHINEGUN_3:
	case MZ2_SOLDIER_MACHINEGUN_4:
	case MZ2_SOLDIER_MACHINEGUN_5:
	case MZ2_SOLDIER_MACHINEGUN_6:
	case MZ2_SOLDIER_MACHINEGUN_7:
	case MZ2_SOLDIER_MACHINEGUN_8:
		Vec3Set (dl->color, 1, 1, 0);
		CG_ParticleEffect (origin, vec3Origin, 0, 40);
		cgi.Snd_StartSound (NULL, entNum, CHAN_WEAPON, cgMedia.sfx.mz2.soldierMachGunSfx, 1, ATTN_NORM, 0);
		break;

	case MZ2_GUNNER_MACHINEGUN_1:
	case MZ2_GUNNER_MACHINEGUN_2:
	case MZ2_GUNNER_MACHINEGUN_3:
	case MZ2_GUNNER_MACHINEGUN_4:
	case MZ2_GUNNER_MACHINEGUN_5:
	case MZ2_GUNNER_MACHINEGUN_6:
	case MZ2_GUNNER_MACHINEGUN_7:
	case MZ2_GUNNER_MACHINEGUN_8:
		Vec3Set (dl->color, 1, 1, 0);
		CG_ParticleEffect (origin, vec3Origin, 0, 40);
		cgi.Snd_StartSound (NULL, entNum, CHAN_WEAPON, cgMedia.sfx.mz2.gunnerMachGunSfx, 1, ATTN_NORM, 0);
		break;

	case MZ2_ACTOR_MACHINEGUN_1:
	case MZ2_SUPERTANK_MACHINEGUN_1:
	case MZ2_SUPERTANK_MACHINEGUN_2:
	case MZ2_SUPERTANK_MACHINEGUN_3:
	case MZ2_SUPERTANK_MACHINEGUN_4:
	case MZ2_SUPERTANK_MACHINEGUN_5:
	case MZ2_SUPERTANK_MACHINEGUN_6:
	case MZ2_TURRET_MACHINEGUN:			// PGM
		Vec3Set (dl->color, 1, 1, 0);
		CG_ParticleEffect (origin, vec3Origin, 0, 40);
		cgi.Snd_StartSound (NULL, entNum, CHAN_WEAPON, cgMedia.sfx.mz2.machGunSfx, 1, ATTN_NORM, 0);
		break;

	case MZ2_BOSS2_MACHINEGUN_L1:
	case MZ2_BOSS2_MACHINEGUN_L2:
	case MZ2_BOSS2_MACHINEGUN_L3:
	case MZ2_BOSS2_MACHINEGUN_L4:
	case MZ2_BOSS2_MACHINEGUN_L5:
	case MZ2_CARRIER_MACHINEGUN_L1:		// PMM
	case MZ2_CARRIER_MACHINEGUN_L2:		// PMM
		Vec3Set (dl->color, 1, 1, 0);
		CG_ParticleEffect (origin, vec3Origin, 0, 40);
		cgi.Snd_StartSound (NULL, entNum, CHAN_WEAPON, cgMedia.sfx.mz2.machGunSfx, 1, ATTN_NONE, 0);
		break;

	case MZ2_SOLDIER_BLASTER_1:
	case MZ2_SOLDIER_BLASTER_2:
	case MZ2_SOLDIER_BLASTER_3:
	case MZ2_SOLDIER_BLASTER_4:
	case MZ2_SOLDIER_BLASTER_5:
	case MZ2_SOLDIER_BLASTER_6:
	case MZ2_SOLDIER_BLASTER_7:
	case MZ2_SOLDIER_BLASTER_8:
	case MZ2_TURRET_BLASTER:			// PGM
		Vec3Set (dl->color, 1, 1, 0);
		cgi.Snd_StartSound (NULL, entNum, CHAN_WEAPON, cgMedia.sfx.mz2.soldierBlasterSfx, 1, ATTN_NORM, 0);
		break;

	case MZ2_FLYER_BLASTER_1:
	case MZ2_FLYER_BLASTER_2:
		Vec3Set (dl->color, 1, 1, 0);
		cgi.Snd_StartSound (NULL, entNum, CHAN_WEAPON, cgMedia.sfx.mz2.flyerBlasterSfx, 1, ATTN_NORM, 0);
		break;

	case MZ2_MEDIC_BLASTER_1:
		Vec3Set (dl->color, 1, 1, 0);
		cgi.Snd_StartSound (NULL, entNum, CHAN_WEAPON, cgMedia.sfx.mz2.medicBlasterSfx, 1, ATTN_NORM, 0);
		break;

	case MZ2_HOVER_BLASTER_1:
		Vec3Set (dl->color, 1, 1, 0);
		cgi.Snd_StartSound (NULL, entNum, CHAN_WEAPON, cgMedia.sfx.mz2.hoverBlasterSfx, 1, ATTN_NORM, 0);
		break;

	case MZ2_FLOAT_BLASTER_1:
		Vec3Set (dl->color, 1, 1, 0);
		cgi.Snd_StartSound (NULL, entNum, CHAN_WEAPON, cgMedia.sfx.mz2.floatBlasterSfx, 1, ATTN_NORM, 0);
		break;

	case MZ2_SOLDIER_SHOTGUN_1:
	case MZ2_SOLDIER_SHOTGUN_2:
	case MZ2_SOLDIER_SHOTGUN_3:
	case MZ2_SOLDIER_SHOTGUN_4:
	case MZ2_SOLDIER_SHOTGUN_5:
	case MZ2_SOLDIER_SHOTGUN_6:
	case MZ2_SOLDIER_SHOTGUN_7:
	case MZ2_SOLDIER_SHOTGUN_8:
		Vec3Set (dl->color, 1, 1, 0);
		cgi.Snd_StartSound (NULL, entNum, CHAN_WEAPON, cgMedia.sfx.mz2.soldierShotgunSfx, 1, ATTN_NORM, 0);
		break;

	case MZ2_TANK_BLASTER_1:
	case MZ2_TANK_BLASTER_2:
	case MZ2_TANK_BLASTER_3:
		Vec3Set (dl->color, 1, 1, 0);
		cgi.Snd_StartSound (NULL, entNum, CHAN_WEAPON, cgMedia.sfx.mz2.tankBlasterSfx, 1, ATTN_NORM, 0);
		break;

	case MZ2_TANK_MACHINEGUN_1:
	case MZ2_TANK_MACHINEGUN_2:
	case MZ2_TANK_MACHINEGUN_3:
	case MZ2_TANK_MACHINEGUN_4:
	case MZ2_TANK_MACHINEGUN_5:
	case MZ2_TANK_MACHINEGUN_6:
	case MZ2_TANK_MACHINEGUN_7:
	case MZ2_TANK_MACHINEGUN_8:
	case MZ2_TANK_MACHINEGUN_9:
	case MZ2_TANK_MACHINEGUN_10:
	case MZ2_TANK_MACHINEGUN_11:
	case MZ2_TANK_MACHINEGUN_12:
	case MZ2_TANK_MACHINEGUN_13:
	case MZ2_TANK_MACHINEGUN_14:
	case MZ2_TANK_MACHINEGUN_15:
	case MZ2_TANK_MACHINEGUN_16:
	case MZ2_TANK_MACHINEGUN_17:
	case MZ2_TANK_MACHINEGUN_18:
	case MZ2_TANK_MACHINEGUN_19:
		Vec3Set (dl->color, 1, 1, 0);
		CG_ParticleEffect (origin, vec3Origin, 0, 40);
		cgi.Snd_StartSound (NULL, entNum, CHAN_WEAPON, cgMedia.sfx.mz2.tankMachGunSfx[(rand () % 5)], 1, ATTN_NORM, 0);
		break;

	case MZ2_CHICK_ROCKET_1:
	case MZ2_TURRET_ROCKET:			// PGM
		Vec3Set (dl->color, 1, 0.5f, 0.2f);
		cgi.Snd_StartSound (NULL, entNum, CHAN_WEAPON, cgMedia.sfx.mz2.chicRocketSfx, 1, ATTN_NORM, 0);
		break;

	case MZ2_TANK_ROCKET_1:
	case MZ2_TANK_ROCKET_2:
	case MZ2_TANK_ROCKET_3:
		Vec3Set (dl->color, 1, 0.5f, 0.2f);
		cgi.Snd_StartSound (NULL, entNum, CHAN_WEAPON, cgMedia.sfx.mz2.tankRocketSfx, 1, ATTN_NORM, 0);
		break;

	case MZ2_SUPERTANK_ROCKET_1:
	case MZ2_SUPERTANK_ROCKET_2:
	case MZ2_SUPERTANK_ROCKET_3:
	case MZ2_BOSS2_ROCKET_1:
	case MZ2_BOSS2_ROCKET_2:
	case MZ2_BOSS2_ROCKET_3:
	case MZ2_BOSS2_ROCKET_4:
	case MZ2_CARRIER_ROCKET_1:
//	case MZ2_CARRIER_ROCKET_2:
//	case MZ2_CARRIER_ROCKET_3:
//	case MZ2_CARRIER_ROCKET_4:
		Vec3Set (dl->color, 1, 0.5f, 0.2f);
		cgi.Snd_StartSound (NULL, entNum, CHAN_WEAPON, cgMedia.sfx.mz2.superTankRocketSfx, 1, ATTN_NORM, 0);
		break;

	case MZ2_GUNNER_GRENADE_1:
	case MZ2_GUNNER_GRENADE_2:
	case MZ2_GUNNER_GRENADE_3:
	case MZ2_GUNNER_GRENADE_4:
		Vec3Set (dl->color, 1, 0.5f, 0);
		cgi.Snd_StartSound (NULL, entNum, CHAN_WEAPON, cgMedia.sfx.mz2.gunnerGrenadeSfx, 1, ATTN_NORM, 0);
		break;

	case MZ2_GLADIATOR_RAILGUN_1:
	case MZ2_CARRIER_RAILGUN:	// PMM
	case MZ2_WIDOW_RAIL:		// PMM
		Vec3Set (dl->color, 0.5f, 0.5f, 1);
		break;

	// --- Xian's shit starts ---
	case MZ2_MAKRON_BFG:
		Vec3Set (dl->color, 0.5f, 1, 0.5f);
		break;

	case MZ2_MAKRON_BLASTER_1:
	case MZ2_MAKRON_BLASTER_2:
	case MZ2_MAKRON_BLASTER_3:
	case MZ2_MAKRON_BLASTER_4:
	case MZ2_MAKRON_BLASTER_5:
	case MZ2_MAKRON_BLASTER_6:
	case MZ2_MAKRON_BLASTER_7:
	case MZ2_MAKRON_BLASTER_8:
	case MZ2_MAKRON_BLASTER_9:
	case MZ2_MAKRON_BLASTER_10:
	case MZ2_MAKRON_BLASTER_11:
	case MZ2_MAKRON_BLASTER_12:
	case MZ2_MAKRON_BLASTER_13:
	case MZ2_MAKRON_BLASTER_14:
	case MZ2_MAKRON_BLASTER_15:
	case MZ2_MAKRON_BLASTER_16:
	case MZ2_MAKRON_BLASTER_17:
		Vec3Set (dl->color, 1, 1, 0);
		cgi.Snd_StartSound (NULL, entNum, CHAN_WEAPON, cgMedia.sfx.mz2.makronBlasterSfx, 1, ATTN_NORM, 0);
		break;
	
	case MZ2_JORG_MACHINEGUN_L1:
	case MZ2_JORG_MACHINEGUN_L2:
	case MZ2_JORG_MACHINEGUN_L3:
	case MZ2_JORG_MACHINEGUN_L4:
	case MZ2_JORG_MACHINEGUN_L5:
	case MZ2_JORG_MACHINEGUN_L6:
		Vec3Set (dl->color, 1, 1, 0);
		CG_ParticleEffect (origin, vec3Origin, 0, 40);
		cgi.Snd_StartSound (NULL, entNum, CHAN_WEAPON, cgMedia.sfx.mz2.jorgMachGunSfx, 1, ATTN_NORM, 0);
		break;

	case MZ2_JORG_MACHINEGUN_R1:
	case MZ2_JORG_MACHINEGUN_R2:
	case MZ2_JORG_MACHINEGUN_R3:
	case MZ2_JORG_MACHINEGUN_R4:
	case MZ2_JORG_MACHINEGUN_R5:
	case MZ2_JORG_MACHINEGUN_R6:
		Vec3Set (dl->color, 1, 1, 0);
		CG_ParticleEffect (origin, vec3Origin, 0, 40);
		break;

	case MZ2_JORG_BFG_1:
		dl->color[0] = 0.5;dl->color[1] = 1 ;dl->color[2] = 0.5;
		break;

	case MZ2_BOSS2_MACHINEGUN_R1:
	case MZ2_BOSS2_MACHINEGUN_R2:
	case MZ2_BOSS2_MACHINEGUN_R3:
	case MZ2_BOSS2_MACHINEGUN_R4:
	case MZ2_BOSS2_MACHINEGUN_R5:
	case MZ2_CARRIER_MACHINEGUN_R1:			// PMM
	case MZ2_CARRIER_MACHINEGUN_R2:			// PMM
		Vec3Set (dl->color, 1, 1, 0);
		CG_ParticleEffect (origin, vec3Origin, 0, 40);
		break;

	// ROGUE
	case MZ2_STALKER_BLASTER:
	case MZ2_DAEDALUS_BLASTER:
	case MZ2_MEDIC_BLASTER_2:
	case MZ2_WIDOW_BLASTER:
	case MZ2_WIDOW_BLASTER_SWEEP1:
	case MZ2_WIDOW_BLASTER_SWEEP2:
	case MZ2_WIDOW_BLASTER_SWEEP3:
	case MZ2_WIDOW_BLASTER_SWEEP4:
	case MZ2_WIDOW_BLASTER_SWEEP5:
	case MZ2_WIDOW_BLASTER_SWEEP6:
	case MZ2_WIDOW_BLASTER_SWEEP7:
	case MZ2_WIDOW_BLASTER_SWEEP8:
	case MZ2_WIDOW_BLASTER_SWEEP9:
	case MZ2_WIDOW_BLASTER_100:
	case MZ2_WIDOW_BLASTER_90:
	case MZ2_WIDOW_BLASTER_80:
	case MZ2_WIDOW_BLASTER_70:
	case MZ2_WIDOW_BLASTER_60:
	case MZ2_WIDOW_BLASTER_50:
	case MZ2_WIDOW_BLASTER_40:
	case MZ2_WIDOW_BLASTER_30:
	case MZ2_WIDOW_BLASTER_20:
	case MZ2_WIDOW_BLASTER_10:
	case MZ2_WIDOW_BLASTER_0:
	case MZ2_WIDOW_BLASTER_10L:
	case MZ2_WIDOW_BLASTER_20L:
	case MZ2_WIDOW_BLASTER_30L:
	case MZ2_WIDOW_BLASTER_40L:
	case MZ2_WIDOW_BLASTER_50L:
	case MZ2_WIDOW_BLASTER_60L:
	case MZ2_WIDOW_BLASTER_70L:
	case MZ2_WIDOW_RUN_1:
	case MZ2_WIDOW_RUN_2:
	case MZ2_WIDOW_RUN_3:
	case MZ2_WIDOW_RUN_4:
	case MZ2_WIDOW_RUN_5:
	case MZ2_WIDOW_RUN_6:
	case MZ2_WIDOW_RUN_7:
	case MZ2_WIDOW_RUN_8:
		Vec3Set (dl->color, 0, 1, 0);
		cgi.Snd_StartSound (NULL, entNum, CHAN_WEAPON, cgMedia.sfx.mz2.tankBlasterSfx, 1, ATTN_NORM, 0);
		break;

	case MZ2_WIDOW_DISRUPTOR:
		Vec3Set (dl->color, -1, -1, -1);
		cgi.Snd_StartSound (NULL, entNum, CHAN_WEAPON, cgMedia.sfx.mz.trackerFireSfx, 1, ATTN_NORM, 0);
		break;

	case MZ2_WIDOW_PLASMABEAM:
	case MZ2_WIDOW2_BEAMER_1:
	case MZ2_WIDOW2_BEAMER_2:
	case MZ2_WIDOW2_BEAMER_3:
	case MZ2_WIDOW2_BEAMER_4:
	case MZ2_WIDOW2_BEAMER_5:
	case MZ2_WIDOW2_BEAM_SWEEP_1:
	case MZ2_WIDOW2_BEAM_SWEEP_2:
	case MZ2_WIDOW2_BEAM_SWEEP_3:
	case MZ2_WIDOW2_BEAM_SWEEP_4:
	case MZ2_WIDOW2_BEAM_SWEEP_5:
	case MZ2_WIDOW2_BEAM_SWEEP_6:
	case MZ2_WIDOW2_BEAM_SWEEP_7:
	case MZ2_WIDOW2_BEAM_SWEEP_8:
	case MZ2_WIDOW2_BEAM_SWEEP_9:
	case MZ2_WIDOW2_BEAM_SWEEP_10:
	case MZ2_WIDOW2_BEAM_SWEEP_11:
		dl->radius = 300 + (rand()&100);
		Vec3Set (dl->color, 1, 1, 0);
		dl->die = cg.refreshTime + 200;
		break;
	// ROGUE

	// --- Xian's shit ends ---
	}
}
