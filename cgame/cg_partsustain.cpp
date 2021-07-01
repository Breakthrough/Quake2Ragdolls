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
// cg_partsustain.c
//

#include "cg_local.h"

#define MAX_SUSTAINS	32
cgSustainPfx_t	cg_pfxSustains[MAX_SUSTAINS];

/*
=============================================================================

	SUSTAIN HANDLING

=============================================================================
*/

/*
=================
CG_ClearSustains
=================
*/
void CG_ClearSustains ()
{
	memset (cg_pfxSustains, 0, sizeof(cg_pfxSustains));
}


/*
=================
CG_AddSustains
=================
*/
void CG_AddSustains ()
{
	cgSustainPfx_t		*s;
	int				i;

	for (i=0, s=cg_pfxSustains ; i<MAX_SUSTAINS ; i++, s++) {
		if (s->id) {
			if ((s->endtime >= cg.refreshTime) && (cg.refreshTime >= s->nextthink))
				s->think (s);
			else if (s->endtime < cg.refreshTime)
				s->id = 0;
		}
	}
}


/*
=================
CG_FindSustainSlot
=================
*/
cgSustainPfx_t *CG_FindSustainSlot ()
{
	int			i;
	cgSustainPfx_t	*s;

	for (i=0, s=cg_pfxSustains ; i<MAX_SUSTAINS ; i++, s++) {
		if (s->id == 0) {
			return s;
			break;
		}
	}

	return NULL;
}

/*
=============================================================================

	SUSTAINED EFFECTS

=============================================================================
*/

/*
===============
CG_Nukeblast
===============
*/
static void CG_Nukeblast (cgSustainPfx_t *self)
{
	vec3_t			dir, porg;
	int				i;
	float			ratio;
	int		clrtable[4] = {110, 112, 114, 116};
	int		rnum, rnum2;

	ratio = 1.0f - (((float)self->endtime - (float)cg.refreshTime)/1000.0f);
	for (i=0 ; i<600 ; i++) {
		Vec3Set (dir, crand (), crand (), crand ());
		VectorNormalizeFastf (dir);
		Vec3MA (self->org, (200.0f * ratio), dir, porg);

		rnum = (rand () % 4);
		rnum2 = (rand () % 4);
		CG_SpawnParticle (
			porg[0],						porg[1],						porg[2],
			0,								0,								0,
			0,								0,								0,
			0,								0,								0,
			palRed (clrtable[rnum]),		palGreen (clrtable[rnum]),		palBlue (clrtable[rnum]),
			palRed (clrtable[rnum2]),		palGreen (clrtable[rnum2]),		palBlue (clrtable[rnum2]),
			1.0,							PART_INSTANT,
			1.0f,							1.0f,
			PT_GENERIC,						PF_SCALED,
			NULL,							NULL,							NULL,
			PART_STYLE_QUAD,
			0);
	}
}

/*
===============
CG_ParticleSteamEffect

Puffs with velocity along direction, with some randomness thrown in
===============
*/
void __fastcall CG_ParticleSteamEffect (vec3_t org, vec3_t dir, int color, int count, int magnitude)
{
	int			rnum, rnum2;
	float		i, d;
	vec3_t		r, u, pvel;

	MakeNormalVectorsf (dir, r, u);

	for (i=0 ; i<count ; i++) {
		Vec3Scale (dir, magnitude, pvel);
		d = crand () * magnitude/3;
		Vec3MA (pvel, d, r, pvel);
		d = crand () * magnitude/3;
		Vec3MA (pvel, d, u, pvel);

		rnum = (rand () % 5);
		rnum2 = (rand () % 5);
		CG_SpawnParticle (
			org[0] + magnitude*0.1f*crand(),org[1] + magnitude*0.1f*crand(),org[2] + magnitude*0.1f*crand(),
			0,								0,								0,
			pvel[0],						pvel[1],						pvel[2],
			0,								0,								-40/2,
			palRed (color + rnum),			palGreen (color + rnum),		palBlue (color + rnum),
			palRed (color + rnum2),			palGreen (color + rnum2),		palBlue (color + rnum2),
			0.9f + (crand () * 0.1f),		-1.0f / (0.5f + (frand () * 0.3f)),
			3 + (frand () * 3),				8 + (frand () * 4),
			pRandSmoke (),					0,
			pLight70Think,					NULL,							NULL,
			PART_STYLE_QUAD,
			0);
	}
}


/*
===============
CG_ParticleSteamEffect2
===============
*/
void CG_ParticleSteamEffect2 (cgSustainPfx_t *self)
{
	int			i, rnum, rnum2;
	float		d;
	vec3_t		r, u, dir, pvel;

	Vec3Copy (self->dir, dir);
	MakeNormalVectorsf (dir, r, u);

	for (i=0 ; i<self->count ; i++) {
		Vec3Scale (dir, self->magnitude, pvel);
		d = crand () * self->magnitude/3;
		Vec3MA (pvel, d, r, pvel);
		d = crand () * self->magnitude/3;
		Vec3MA (pvel, d, u, pvel);

		rnum = (rand () % 5);
		rnum2 = (rand () % 5);
		CG_SpawnParticle (
			self->org[0] + self->magnitude * 0.1f * crand (),
			self->org[1] + self->magnitude * 0.1f * crand (),
			self->org[2] + self->magnitude * 0.1f * crand (),
			0,								0,								0,
			pvel[0],						pvel[1],						pvel[2],
			0,								0,								-20,
			palRed (self->color + rnum),	palGreen (self->color + rnum),	palBlue (self->color + rnum),
			palRed (self->color + rnum2),	palGreen (self->color + rnum2),	palBlue (self->color + rnum2),
			0.9f + (crand () * 0.1f),		-1.0f / (0.5f + (frand () * 0.3f)),
			3 + (frand () * 3),				8 + (frand () * 4),
			pRandSmoke (),					0,
			pLight70Think,					NULL,							NULL,
			PART_STYLE_QUAD,
			0);
	}

	self->nextthink += self->thinkinterval;
}


/*
===============
CG_Widowbeamout
===============
*/
void CG_Widowbeamout (cgSustainPfx_t *self)
{
	vec3_t			dir, porg;
	int				i, rnum, rnum2;
	static int		clrtable[4] = {2*8, 13*8, 21*8, 18*8};
	float			ratio;

	ratio = 1.0f - (((float)self->endtime - (float)cg.refreshTime)/ 2100.0f);

	for (i=0 ; i<300 ; i++) {
		Vec3Set (dir, crand (), crand (), crand ());
		VectorNormalizeFastf (dir);
	
		Vec3MA (self->org, (45.0f * ratio), dir, porg);

		rnum = (rand () % 4);
		rnum2 = (rand () % 4);
		CG_SpawnParticle (
			porg[0],						porg[1],						porg[2],
			0,								0,								0,
			0,								0,								0,
			0,								0,								0,
			palRed (clrtable[rnum]),		palGreen (clrtable[rnum]),		palBlue (clrtable[rnum]),
			palRed (clrtable[rnum2]),		palGreen (clrtable[rnum2]),		palBlue (clrtable[rnum2]),
			1.0,							PART_INSTANT,
			1.0f,							1.0f,
			PT_GENERIC,						PF_SCALED,
			NULL,							NULL,							NULL,
			PART_STYLE_QUAD,
			0);
	}
}

/*
=============================================================================

	SUSTAIN PARSING

=============================================================================
*/

/*
=================
CG_ParseNuke
=================
*/
void CG_ParseNuke ()
{
	cgSustainPfx_t	*s;

	s = CG_FindSustainSlot (); // find a free sustain

	if (s) {
		// Found one
		s->id = 21000;
		cgi.MSG_ReadPos (s->org);
		s->endtime = cg.refreshTime + 1000;
		s->think = CG_Nukeblast;
		s->thinkinterval = 1;
		s->nextthink = cg.refreshTime;
	}
	else {
		// No free sustains
		vec3_t	pos;
		cgi.MSG_ReadPos (pos);
	}
}


/*
=================
CG_ParseSteam
=================
*/
void CG_ParseSteam ()
{
	int		id, r, cnt;
	int		color, magnitude;
	cgSustainPfx_t	*s;

	id = cgi.MSG_ReadShort ();		// an id of -1 is an instant effect
	if (id != -1) {
		// Sustained
		s = CG_FindSustainSlot (); // find a free sustain

		if (s) {
			s->id = id;
			s->count = cgi.MSG_ReadByte ();
			cgi.MSG_ReadPos (s->org);
			cgi.MSG_ReadDir (s->dir);
			r = cgi.MSG_ReadByte ();
			s->color = r & 0xff;
			s->magnitude = cgi.MSG_ReadShort ();
			s->endtime = cg.refreshTime + cgi.MSG_ReadLong ();
			s->think = CG_ParticleSteamEffect2;
			s->thinkinterval = 100;
			s->nextthink = cg.refreshTime;
		}
		else {
			vec3_t	pos, dir;
			cnt = cgi.MSG_ReadByte ();
			cgi.MSG_ReadPos (pos);
			cgi.MSG_ReadDir (dir);
			r = cgi.MSG_ReadByte ();
			magnitude = cgi.MSG_ReadShort ();
			magnitude = cgi.MSG_ReadLong (); // really interval
		}
	}
	else {
		// Instant
		vec3_t	pos, dir;
		cnt = cgi.MSG_ReadByte ();
		cgi.MSG_ReadPos (pos);
		cgi.MSG_ReadDir (dir);
		r = cgi.MSG_ReadByte ();
		magnitude = cgi.MSG_ReadShort ();
		color = r & 0xff;
		CG_ParticleSteamEffect (pos, dir, color, cnt, magnitude);
	}
}


/*
=================
CG_ParseWidow
=================
*/
void CG_ParseWidow ()
{
	int			id;
	cgSustainPfx_t	*s;

	id = cgi.MSG_ReadShort ();

	s = CG_FindSustainSlot (); // find a free sustain

	if (s) {
		// Found one
		s->id = id;
		cgi.MSG_ReadPos (s->org);
		s->endtime = cg.refreshTime + 2100;
		s->think = CG_Widowbeamout;
		s->thinkinterval = 1;
		s->nextthink = cg.refreshTime;
	}
	else {
		// No free sustains
		vec3_t	pos;
		cgi.MSG_ReadPos (pos);
	}
}
