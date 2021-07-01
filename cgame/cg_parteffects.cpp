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
// cg_parteffects.c
//

#include "cg_local.h"

/*
===============
CG_FindExplosionDir

This is a "necessary hack" for explosion decal placement.
===============
*/
bool CG_FindExplosionDir (vec3_t origin, float radius, vec3_t endPos, vec3_t dir)
{
	static vec3_t	planes[6] = {{0,0,1}, {0,1,0}, {1,0,0}, {0,0,-1}, {0,-1,0}, {-1,0,0}};
	cmTrace_t		tr;
	vec3_t			tempDir;
	vec3_t			tempOrg;
	float			best;
	int				i;

	best = 1.0f;
	Vec3Clear (endPos);
	Vec3Clear (dir);
	for (i=0 ; i<6 ; i++) {
		Vec3MA (origin, radius * 0.9f, planes[i], tempDir);
		Vec3MA (origin, radius * 0.1f, planes[(i+3)%6], tempOrg);

		CG_PMTrace (&tr, tempOrg, NULL, NULL, tempDir, false, true);
		if (tr.allSolid || tr.fraction == 1.0f)
			continue;

		if (tr.fraction < best) {
			best = tr.fraction;
			Vec3Copy (tr.endPos, endPos);
			Vec3Copy (tr.plane.normal, dir);
		}
	}

	if (best < 1.0f) {
		// FIXME: why does this "fix" decal normals on xdmt4? Something to do with the fragment planes...
		byte dirByte = DirToByte (dir);
		ByteToDir (dirByte, dir);
		return true;
	}

	return false;
}

/*
=============================================================================

	PARTICLE EFFECTS

=============================================================================
*/

/*
===============
CG_BlasterBlueParticles
===============
*/
void CG_BlasterBlueParticles (vec3_t org, vec3_t dir)
{
	int			i, count;
	int			rnum, rnum2;
	float		d;

	// Glow mark
	rnum = (rand () % 5);
	rnum2 = (rand () % 5);
	CG_SpawnDecal (
		org[0],								org[1],								org[2],
		dir[0],								dir[1],								dir[2],
		palRed (0x70 + rnum),				palGreen (0x70 + rnum),				palBlue (0x70 + rnum),
		palRed (0x70 + rnum2),				palGreen (0x70 + rnum2),			palBlue (0x70 + rnum2),
		1,									0,
		7 + (frand() * 0.5f),
		DT_BLASTER_BLUEMARK,				DF_USE_BURNLIFE|DF_ALPHACOLOR,
		0,									false,
		0,									frand () * 360);

	// Burn mark
	rnum = (rand () % 5);
	CG_SpawnDecal (
		org[0],								org[1],								org[2],
		dir[0],								dir[1],								dir[2],
		(255-palRed(0x70+rnum))*0.5f+128,	(255-palGreen(0x70+rnum))*0.5f+128,	(255-palBlue(0x70+rnum))*0.5f+128,
		0,									0,									0,
		0.9f + (crand() * 0.1f),			0.9f + (crand() * 0.1f),
		5 + (frand() * 0.5f),
		DT_BLASTER_BURNMARK,				DF_ALPHACOLOR,
		0,									false,
		0,									frand () * 360.0f);

	// Smoke
	count = 6 + (cg.smokeLingerScale * 2.5f);
	for (i=0 ; i<count ; i++) {
		d = 3 + (frand () * 6);
		rnum = (rand () % 5);

		CG_SpawnParticle (
			org[0] + (d*dir[0]) + (crand()*2),	org[1] + (d*dir[1]) + (crand()*2),	org[2] + (d*dir[2]) + (crand()*2),
			0,									0,									0,
			0,									0,									0,
			0,									0,									5 + (frand () * 25),
			palRed (0x70 + rnum),				palGreen (0x70 + rnum),				palBlue (0x70 + rnum),
			palRed (0x70 + rnum),				palGreen (0x70 + rnum),				palBlue (0x70 + rnum),
			0.9f + (frand() * 0.1f),			-1.0f / (0.8f + (cg.smokeLingerScale * 0.1f) + (frand() * 0.1f)),
			5 + crand (),						16 + (crand () * 8),
			pRandGlowSmoke (),					PF_ALPHACOLOR,
			NULL,								NULL,								NULL,
			PART_STYLE_QUAD,
			frand () * 360);
	}

	// Dots
	count = 60;
	for (i=0 ; i<count ; i++) {
		d = 6 + (frand () * 12);
		rnum = (rand () % 5);

		CG_SpawnParticle (
			org[0] + (d*dir[0]) + (crand()*4),	org[1] + (d*dir[1]) + (crand()*4),	org[2] + (d*dir[2]) + (crand()*4),
			0,									0,									0,
			(dir[0] * 25) + (crand () * 35),	(dir[1] * 25) + (crand () * 35),	(dir[2] * 25) + (crand () * 35),
			0,									0,									-(frand () * 10),
			palRed (0x70 + rnum),				palGreen (0x70 + rnum),				palBlue (0x70 + rnum),
			palRed (0x70 + rnum),				palGreen (0x70 + rnum),				palBlue (0x70 + rnum),
			0.9f + (frand () * 0.1f),			-1.0f / (1 + frand () * 0.3f),
			11 + (frand () * -10.75f),			0.1f + (frand () * 0.5f),
			PT_BLASTER_BLUE,					PF_SCALED|PF_GRAVITY|PF_ALPHACOLOR|PF_NOCLOSECULL,
			pBounceThink,						NULL,								NULL,
			PART_STYLE_QUAD,
			0);
	}
}


/*
===============
CG_BlasterGoldParticles
===============
*/
void CG_BlasterGoldParticles (vec3_t org, vec3_t dir)
{
	int			i, count;
	int			rnum, rnum2;
	float		d;

	// Glow mark
	rnum = (rand () % 5);
	rnum2 = (rand () % 5);
	CG_SpawnDecal (
		org[0],								org[1],								org[2],
		dir[0],								dir[1],								dir[2],
		palRed (0xe0 + rnum),				palGreen (0xe0 + rnum),				palBlue (0xe0 + rnum),
		palRed (0xe0 + rnum2),				palGreen (0xe0 + rnum2),			palBlue (0xe0 + rnum2),
		1,									0,
		7 + (frand() * 0.5f),
		DT_BLASTER_REDMARK,					DF_USE_BURNLIFE|DF_ALPHACOLOR,
		0,									false,
		0,									frand () * 360);

	// Burn mark
	rnum = (rand () % 5);
	CG_SpawnDecal (
		org[0],								org[1],								org[2],
		dir[0],								dir[1],								dir[2],
		(255-palRed(0xe0+rnum))*0.5f+128,	(255-palGreen(0xe0+rnum))*0.5f+128,	(255-palBlue(0xe0+rnum))*0.5f+128,
		0,									0,									0,
		0.9f + (crand() * 0.1f),			0.9f + (crand() * 0.1f),
		5 + (frand() * 0.5f),
		DT_BLASTER_BURNMARK,				DF_ALPHACOLOR,
		0,									false,
		0,									frand () * 360);

	// Light emitting smoke
	d = 3 + (frand () * 6);
	rnum = rand () % 5;
	rnum2 = rand () % 5;

	CG_SpawnParticle (
		org[0] + (d*dir[0]) + (crand()*2),	org[1] + (d*dir[1]) + (crand()*2),	org[2] + (d*dir[2]) + (crand()*2),
		0,									0,									0,
		0,									0,									0,
		0,									0,									5 + (frand () * 25),
		palRed (0xe0 + rnum),				palGreen (0xe0 + rnum),				palBlue (0xe0 + rnum),
		palRed (0xe0 + rnum2),				palGreen (0xe0 + rnum2),			palBlue (0xe0 + rnum2),
		0.9f + (frand() * 0.1f),			-1.0f / (0.6f + (cg.smokeLingerScale * 0.1f) + (frand() * 0.1f)),
		5 + crand (),						16 + (crand () * 8),
		pRandGlowSmoke (),					PF_ALPHACOLOR,
		pBlasterGlowThink,					NULL,								NULL,
		PART_STYLE_QUAD,
		frand () * 360);

	// Smoke
	count = 5 + (cg.smokeLingerScale * 2.5f);
	for (i=0 ; i<count ; i++) {
		d = 3 + (frand () * 2);
		rnum = rand () % 5;
		rnum2 = rand () % 5;

		CG_SpawnParticle (
			org[0] + (d*dir[0]) + (crand()*2),	org[1] + (d*dir[1]) + (crand()*2),	org[2] + (d*dir[2]) + (crand()*2),
			0,									0,									0,
			0,									0,									0,
			0,									0,									5 + (frand () * 25),
			palRed (0xe0 + rnum),				palGreen (0xe0 + rnum),				palBlue (0xe0 + rnum),
			palRed (0xe0 + rnum2),				palGreen (0xe0 + rnum2),			palBlue (0xe0 + rnum2),
			0.9f + (frand() * 0.1f),			-1.0f / (0.8f + (cg.smokeLingerScale * 0.1f) + (frand() * 0.1f)),
			5 + crand (),						16 + (crand () * 8),
			pRandGlowSmoke (),					PF_ALPHACOLOR,
			NULL,								NULL,								NULL,
			PART_STYLE_QUAD,
			frand () * 360);
	}

	// Dots
	count = 25;
	for (i=0 ; i<count ; i++)
	{
		CG_SpawnParticle (
			org[0] + (d*dir[0]) + (crand()*2),	org[1] + (d*dir[1]) + (crand()*2),	org[2] + (d*dir[2]) + (crand()*2),
			0,									0,									0,
			(dir[0] * 25) + (crand () * 35),	(dir[1] * 25) + (crand () * 35),	(dir[2] * 25) + (crand () * 35),
			0,									0,									-(frand () * 10),
			palRed (0xe0 + rnum),				palGreen (0xe0 + rnum),				palBlue (0xe0 + rnum),
			palRed (0xe0 + rnum),				palGreen (0xe0 + rnum),				palBlue (0xe0 + rnum),
			0.9f + (frand () * 0.1f),			-1.0f / (1 + frand () * 0.3f),
			6 + (frand () * -10.75f),			0.1f + (frand () * 0.5f),
			PT_BLASTER_RED,						PF_SCALED|PF_GRAVITY|PF_ALPHACOLOR|PF_NOCLOSECULL,
			pBounceThink,						NULL,								NULL,
			PART_STYLE_QUAD,
			0);
	}
}


/*
===============
CG_BlasterGreenParticles
===============
*/
void CG_BlasterGreenParticles (vec3_t org, vec3_t dir)
{
	int			i, count, rnum;
	float		d, randwhite;

	// Glow mark
	CG_SpawnDecal (
		org[0],								org[1],								org[2],
		dir[0],								dir[1],								dir[2],
		(float)(rand()%41+135),				(float)180 + (rand()%76),			(float)(rand()%41+135),
		(float)(rand()%41+135),				(float)180 + (rand()%76),			(float)(rand()%41+135),
		1,									0,
		5 + (frand() * 0.5f),
		DT_BLASTER_GREENMARK,				DF_USE_BURNLIFE|DF_ALPHACOLOR,
		0,									false,
		0,									frand () * 360);

	// Burn mark
	rnum = (rand()%5);
	CG_SpawnDecal (
		org[0],								org[1],								org[2],
		dir[0],								dir[1],								dir[2],
		(255-palRed(0xd0+rnum))*0.5f+128,	(255-palGreen(0xd0+rnum))*0.5f+128,	(255-palBlue(0xd0+rnum))*0.5f+128,
		0,									0,									0,
		0.9f + (crand() * 0.1f),			0.9f + (crand() * 0.1f),
		4 + (frand() * 0.5f),
		DT_BLASTER_BURNMARK,				DF_ALPHACOLOR,
		0,									false,
		0,									frand () * 360);

	// Smoke
	count = 6 + (cg.smokeLingerScale * 2.5f);
	for (i=0 ; i<count ; i++) {
		d = 3 + (frand () * 6);
		randwhite = (rand()&1)?150 + (rand()%26) : 0.0f;

		CG_SpawnParticle (
			org[0] + (d*dir[0]) + (crand()*2),	org[1] + (d*dir[1]) + (crand()*2),	org[2] + (d*dir[2]) + (crand()*2),
			0,									0,									0,
			0,									0,									0,
			0,									0,									5 + (frand () * 25),
			randwhite,							65 + (rand()%150) + randwhite,		(rand()%50) + randwhite,
			randwhite,							65 + (rand()%150) + randwhite,		(rand()%50) + randwhite,
			0.9f + (frand() * 0.1f),			-1.0f / (0.8f + (cg.smokeLingerScale * 0.1f) + (frand() * 0.1f)),
			5 + crand (),						16 + (crand () * 8),
			pRandGlowSmoke (),					PF_ALPHACOLOR,
			NULL,								NULL,								NULL,
			PART_STYLE_QUAD,
			frand () * 360);
	}

	// Dots
	count = 60.0f;
	for (i=0 ; i<count ; i++) {
		d = 6 + (frand () * 12);
		randwhite = (rand()&1)?150 + (rand()%26) : 0.0f;

		CG_SpawnParticle (
			org[0] + (d*dir[0]) + (crand()*4),	org[1] + (d*dir[1]) + (crand()*4),	org[2] + (d*dir[2]) + (crand()*4),
			0,									0,									0,
			(dir[0] * 25) + (crand () * 35),	(dir[1] * 25) + (crand () * 35),	(dir[2] * 25) + (crand () * 35),
			0,									0,									-(frand () * 10),
			randwhite,							65 + (rand()%150) + randwhite,		(rand()%50) + randwhite,
			randwhite,							65 + (rand()%150) + randwhite,		(rand()%50) + randwhite,
			0.9f + (frand() * 0.1f),			-0.4f / (0.3f + (frand () * 0.3f)),
			10 + (frand() * -9.75f),			0.1f + (frand() * 0.5f),
			PT_BLASTER_GREEN,					PF_SCALED|PF_GRAVITY|PF_ALPHACOLOR|PF_NOCLOSECULL,
			pBounceThink,						NULL,								NULL,
			PART_STYLE_QUAD,
			0);
	}
}


/*
===============
CG_BlasterGreyParticles
===============
*/
void CG_BlasterGreyParticles (vec3_t org, vec3_t dir)
{
	int		i, count;
	float	d;

	// Smoke
	count = 6 + (cg.smokeLingerScale * 2.5f);
	for (i=0 ; i<count ; i++) {
		d = (float)(rand()%13 + 3);

		CG_SpawnParticle (
			org[0] + d*dir[0],					org[1] + d*dir[1],					org[2] + d*dir[2],
			0,									0,									0,
			0,									0,									0,
			0,									0,									10 + (frand () * 20),
			130.0f + (rand()%6),				162.0f + (rand()%6),				178.0f + (rand()%6),
			130.0f + (rand()%6),				162.0f + (rand()%6),				178.0f + (rand()%6),
			0.9f + (frand() * 0.1f),			-1.0f / (0.8f + (cg.smokeLingerScale * 0.1f) + (frand() * 0.1f)),
			5 + crand (),						15 + (crand () * 8),
			pRandGlowSmoke (),					PF_ALPHACOLOR,
			NULL,								NULL,								NULL,
			PART_STYLE_QUAD,
			frand () * 360);
	}

	// Dots
	count = 50.0f;
	for (i=0 ; i<count ; i++) {
		d = 1.5f + (rand()%13 + 3);

		CG_SpawnParticle (
			org[0] + crand () * 4 + d*dir[0],	org[1] + crand () * 4 + d*dir[1],	org[2] + crand () * 4 + d*dir[2],
			0,									0,									0,
			dir[0] * 25 + crand () * 35,		dir[1] * 25 + crand () * 35,		dir[2] * 25 + crand () * 35,
			0,									0,									-10 + (frand () * 10),
			130.0f + (rand()%6),				162.0f + (rand()%6),				178.0f + (rand()%6),
			130.0f + (rand()%6),				162.0f + (rand()%6),				178.0f + (rand()%6),
			0.9f + (frand() * 0.1f),			-1.0f / (0.8f + (frand () * 0.3f)),
			10 + (frand() * -9.75f),			0.1f + (frand() * 0.5f),
			PT_FLARE,							PF_SCALED|PF_GRAVITY|PF_NOCLOSECULL,
			pBounceThink,						NULL,								NULL,
			PART_STYLE_QUAD,
			0);
	}
}


/*
===============
CG_BleedEffect
===============
*/
void CG_BleedEffect(vec3_t org, vec3_t dir, const int count, const bool bGreen)
{
	int			i, flags;
	float		gore, amount, fly;
	vec3_t		orgVec, dirVec;

	gore = cg.goreScale * 10.0f;
	fly = (gore * 0.2f) * 20;

	// Splurt
	amount = ((gore + 5) / 10.0f) + 0.5f;
	for (i=0 ; i<amount ; i++) {
		dirVec[0] = crand () * 3;
		dirVec[1] = crand () * 3;
		dirVec[2] = crand () * 3;

		CG_SpawnParticle (
			org[0] + (crand () * 3),			org[1] + (crand () * 3),			org[2] + (crand () * 3),
			0,									0,									0,
			dirVec[0],							dirVec[1],							dirVec[2],
			dirVec[0] * -0.25f,					dirVec[1] * -0.25f,					dirVec[2] * -0.25f,
			230 + (frand () * 5),				245 + (frand () * 10),				245 + (frand () * 10),
			0,									0,									0,
			1.0f,								-0.5f / (0.4f + (frand () * 0.3f)),
			7 + (crand () * 2),					14 + (crand () * 3),
			bGreen ? PT_BLDSPURT_GREEN : PT_BLDSPURT,
			PF_SCALED|PF_ALPHACOLOR|PF_NOCLOSECULL,
			NULL,								NULL,								NULL,
			PART_STYLE_QUAD,
			frand () * 360);
	}

	// Larger splurt
	for (i=0 ; i<amount ; i++) {
		dirVec[0] = crand () * 4;
		dirVec[1] = crand () * 4;
		dirVec[2] = crand () * 4;

		CG_SpawnParticle (
			org[0] + (crand () * 3),			org[1] + (crand () * 3),			org[2] + (crand () * 3),
			0,									0,									0,
			dirVec[0],							dirVec[1],							dirVec[2],
			dirVec[0] * -0.25f,					dirVec[1] * -0.25f,					dirVec[2] * -0.25f,
			230 + (frand () * 5),				245 + (frand () * 10),				245 + (frand () * 10),
			0,									0,									0,
			1.0f,								-0.5f / (0.4f + (frand () * 0.3f)),
			8 + (crand () * 2),					14 + (crand () * 3),
			bGreen ? PT_BLDSPURT_GREEN2 : PT_BLDSPURT2,
			PF_SCALED|PF_ALPHACOLOR|PF_NOCLOSECULL,
			NULL,								NULL,								NULL,
			PART_STYLE_QUAD,
			frand () * 360);
	}

	// Drips
	amount = (count * 0.25f) + (gore * 0.5f);
	flags = PF_ALPHACOLOR|PF_SCALED|PF_GRAVITY;
	if (bGreen)
		flags |= PF_GREENBLOOD;

	for (i=0 ; i<amount ; i++)
	{
		// A few drops follow a 'leader'
		if (!(i&7))
		{
			// Leader
			orgVec[0] = org[0] + (crand() * 2);
			orgVec[1] = org[1] + (crand() * 2);
			orgVec[2] = org[2] + (crand() * 2);

			dirVec[0] = dir[0] * (120.0f + (frand() * 30.0f));
			dirVec[1] = dir[1] * (120.0f + (frand() * 30.0f));
			dirVec[2] = dir[2] * (40.0f + (frand() * 10.0f));
			dirVec[0] += crand () * 40;
			dirVec[1] += crand () * 40;
			dirVec[2] += (crand () * 10) + (50 + fly);

			flags &= ~PF_NODECAL;
		}
		else
		{
			// Follower
			Vec3Scale(dirVec, 0.9f + (crand () * 0.2f), dirVec);

			flags |= PF_NODECAL;
		}

		// Create the drip
		CG_SpawnParticle (
			orgVec[0] + (dir[0] * frand()),		orgVec[1] + (dir[1] * frand()),		orgVec[2] + (dir[2] * frand()),
			0,									0,									0,
			dirVec[0],							dirVec[1],							dirVec[2],
			0,									0,									-200 + (crand() * fly),
			230 + (frand () * 5),				245 + (frand () * 10),				245 + (frand () * 10),
			0,									0,									0,
			1.0f,								-0.5f / (0.4f + (frand () * 0.9f)),
			0.25f + (frand () * 0.9f),			0.35f + (frand () * 0.5f),
			bGreen ? pRandGrnBloodDrip() : pRandBloodDrip (),
			flags,
			pBloodDripThink,					NULL,								NULL,
			PART_STYLE_DIRECTION,
			PMAXBLDDRIPLEN*0.5f + (frand () * PMAXBLDDRIPLEN));
	}
}


/*
===============
CG_BubbleEffect
===============
*/
void CG_BubbleEffect (vec3_t origin)
{
	float	rnum, rnum2;

	// why bother spawn a particle that's just going to die anyways?
	if (!(cgi.CM_PointContents (origin, 0) & CONTENTS_MASK_WATER))
		return;

	rnum = 230 + (frand () * 25);
	rnum2 = 230 + (frand () * 25);
	CG_SpawnParticle (
		origin[0] + crand (),			origin[1] + crand (),			origin[2] + crand (),
		0,								0,								0,
		crand () * 4,					crand () * 4,					10 + (crand () * 4),
		0,								0,								0,
		rnum,							rnum,							rnum,
		rnum2,							rnum2,							rnum2,
		0.9f + (crand () * 0.1f),		-1.0f / (3 + (frand () * 0.2f)),
		0.1f + frand (),				0.1f + frand (),
		PT_WATERBUBBLE,					PF_NOCLOSECULL,
		pWaterOnlyThink,				pLight70Think,					NULL,
		PART_STYLE_QUAD,
		0);
}


/*
===============
CG_ExplosionBFGEffect

Normally drawn at the feet of opponents when a bfg explodes
===============
*/
void CG_ExplosionBFGEffect (vec3_t org)
{
	int			i;
	float		randwhite;
	float		rnum, rnum2;

	// Smoke
	for (i=0 ; i<8 ; i++) {
		rnum = 70 + (frand () * 40);
		rnum2 = 80 + (frand () * 40);
		CG_SpawnParticle (
			org[0] + (crand () * 4),		org[1] + (crand () * 4),		org[2] + (crand () * 4),
			0,								0,								0,
			crand () * 2,					crand () * 2,					crand () * 2,
			0,								0,								5 + (frand () * 6),
			rnum,							80 + rnum,						rnum,
			rnum2,							100+ rnum2,						rnum2,
			0.75f + (crand () * 0.1f),		-1.0f / (0.25f + cg.smokeLingerScale + (crand () * 0.1f)),
			35 + (crand () * 15),			140 + (crand () * 30),
			pRandGlowSmoke (),				0,
			pSmokeThink,					NULL,							NULL,
			PART_STYLE_QUAD,
			frand () * 361);
	}

	// Dots
	for (i=0 ; i<256 ; i++) {
		randwhite = (rand()&1) ? 150 + (rand()%26) : 0.0f;
		CG_SpawnParticle (
			org[0] + (crand () * 20),		org[1] + (crand () * 20),			org[2] + (crand () * 20),
			0,								0,									0,
			crand () * 50,					crand () * 50,						crand () * 50,
			0,								0,									-40,
			randwhite,						75 + (rand()%150) + randwhite,		(rand()%50) + randwhite,
			randwhite,						75 + (rand()%150) + randwhite,		(rand()%50) + randwhite,
			1.0f,							-0.8f / (0.8f + (frand () * 0.3f)),
			11 + (crand () * 10.5f),		0.6f + (crand () * 0.5f),
			PT_BFG_DOT,						PF_SCALED|PF_GRAVITY|PF_ALPHACOLOR|PF_NOCLOSECULL,
			pBounceThink,					NULL,								NULL,
			PART_STYLE_QUAD,
			0);
	}
}


/*
===============
CG_FlareEffect
===============
*/
void CG_FlareEffect (vec3_t origin, EParticleType type, float orient, float size, float sizevel, int color, int colorvel, float alpha, float alphavel)
{
	CG_SpawnParticle (
		origin[0],						origin[1],						origin[2],
		0,								0,								0,
		0,								0,								0,
		0,								0,								0,
		palRed (color),					palGreen (color),				palBlue (color),
		palRed (colorvel),				palGreen (colorvel),			palBlue (colorvel),
		alpha,							alphavel,
		size,							sizevel,
		type,							PF_SCALED|PF_ALPHACOLOR|PF_NOCLOSECULL,
		pFlareThink,					NULL,							NULL,
		PART_STYLE_QUAD,
		orient);
}


/*
===============
CG_ItemRespawnEffect
===============
*/
void CG_ItemRespawnEffect (vec3_t org)
{
	int			i;

	for (i=0 ; i<64 ; i++) {
		CG_SpawnParticle (
			org[0] + (crand () * 9),		org[1] + (crand () * 9),		org[2] + (crand () * 9),
			0,								0,								0,
			crand () * 10,					crand () * 10,					crand () * 10,
			crand () * 10,					crand () * 10,					20 + (crand () * 10),
			135 + (frand () * 40),			180 + (frand () * 75),			135 + (frand () * 40),
			135 + (frand () * 40),			180 + (frand () * 75),			135 + (frand () * 40),
			1.0f,							-1.0f / (1.0f + (frand () * 0.3f)),
			4,								2,
			PT_ITEMRESPAWN,					PF_SCALED|PF_ALPHACOLOR|PF_NOCLOSECULL,
			0,								NULL,							NULL,
			PART_STYLE_QUAD,
			0);
	}
}


/*
===============
CG_LogoutEffect
===============
*/
void CG_LogoutEffect (vec3_t org, int type)
{
	int			i, rnum, rnum2;
	float		count = 300;

	switch (type) {
	case MZ_LOGIN:
		// green
		for (i=0 ; i<count ; i++) {
			rnum = (rand() % 5);
			rnum2 = (rand() % 5);
			CG_SpawnParticle (
				org[0] - 16 + (frand () * 32),		org[1] - 16 + (frand () * 32),		org[2] - 24 + (frand () * 56),
				0,									0,									0,
				crand () * 20,						crand () * 20,						crand () * 20,
				0,									0,									-40,
				palRed (0xd0 + rnum),				palGreen (0xd0 + rnum),				palBlue (0xd0 + rnum),
				palRed (0xd0 + rnum2),				palGreen (0xd0 + rnum2),			palBlue (0xd0 + rnum2),
				1.0f,								-1.0f / (1.0f + (frand () * 0.3f)),
				3,									1,
				PT_GENERIC_GLOW,					PF_SCALED|PF_ALPHACOLOR|PF_NOCLOSECULL,
				0,									NULL,								NULL,
				PART_STYLE_QUAD,
				0);
		}
		break;

	case MZ_LOGOUT:
		// red
		for (i=0 ; i<count ; i++) {
			rnum = (rand() % 5);
			rnum2 = (rand() % 5);
			CG_SpawnParticle (
				org[0] - 16 + (frand () * 32),		org[1] - 16 + (frand () * 32),		org[2] - 24 + (frand () * 56),
				0,									0,									0,
				crand () * 20,						crand () * 20,						crand () * 20,
				0,									0,									-40,
				palRed (0x40 + rnum),				palGreen (0x40 + rnum),				palBlue (0x40 + rnum),
				palRed (0x40 + rnum2),				palGreen (0x40 + rnum2),			palBlue (0x40 + rnum2),
				1.0f,								-1.0f / (1.0f + (frand () * 0.3f)),
				3,									1,
				PT_GENERIC_GLOW,					PF_SCALED|PF_ALPHACOLOR|PF_NOCLOSECULL,
				0,									NULL,								NULL,
				PART_STYLE_QUAD,
				0);
		}
		break;

	default:
		// golden
		for (i=0 ; i<count ; i++) {
			rnum = (rand() % 5);
			rnum2 = (rand() % 5);
			CG_SpawnParticle (
				org[0] - 16 + (frand () * 32),		org[1] - 16 + (frand () * 32),		org[2] - 24 + (frand () * 56),
				0,									0,									0,
				crand () * 20,						crand () * 20,						crand () * 20,
				0,									0,									-40,
				palRed (0xe0 + rnum),				palGreen (0xe0 + rnum),				palBlue (0xe0 + rnum),
				palRed (0xe0 + rnum2),				palGreen (0xe0 + rnum2),			palBlue (0xe0 + rnum2),
				1.0f,								-1.0f / (1.0f + (frand () * 0.3f)),
				3,									1,
				PT_GENERIC_GLOW,					PF_SCALED|PF_ALPHACOLOR|PF_NOCLOSECULL,
				0,									NULL,								NULL,
				PART_STYLE_QUAD,
				0);
		}
		break;
	}
}


/*
===============
CG_ParticleEffect

Wall impact puffs
===============
*/
void __fastcall CG_ParticleEffect (vec3_t org, vec3_t dir, int color, int count)
{
	int			i, rnum, rnum2;
	float		d;

	switch (color) {
	// blood
	case 0xe8:
		CG_BleedEffect (org, dir, count);
		return;

	// bullet mark
	case 0x0:
		CG_RicochetEffect (org, dir, count);
		return;

	// default
	default:
		for (i=0 ; i<count ; i++) {
			d = (float)(rand()%31);
			rnum = (rand()%5);
			rnum2 = (rand()%5);

			CG_SpawnParticle (
				org[0] + ((rand()%7)-4) + d*dir[0],	org[1] + ((rand()%7)-4) + d*dir[1],	org[2] + ((rand()%7)-4) + d*dir[2],
				0,									0,									0,
				crand () * 20,						crand () * 20,						crand () * 20,
				0,									0,									-40,
				palRed (color + rnum),				palGreen (color + rnum),			palBlue (color + rnum),
				palRed (color + rnum2),				palGreen (color + rnum2),			palBlue (color + rnum2),
				1,									-1.0f / (0.5f + (frand () * 0.3f)),
				1,									1,
				PT_GENERIC,							PF_SCALED,
				0,									NULL,								NULL,
				PART_STYLE_QUAD,
				0);
		}
		return;
	}
}


/*
===============
CG_ParticleEffect2
===============
*/
void __fastcall CG_ParticleEffect2 (vec3_t org, vec3_t dir, int color, int count)
{
	int			i, rnum, rnum2;
	float		d;

	for (i=0 ; i<count ; i++) {
		d = frand () * 7;
		rnum = (rand () % 5);
		rnum2 = (rand () % 5);

		CG_SpawnParticle (
			org[0] + (crand () * 4) + d*dir[0],	org[1] + (crand () * 4) + d*dir[1],	org[2] + (crand () * 4) + d*dir[2],
			0,									0,									0,
			crand () * 20,						crand () * 20,						crand () * 20,
			0,									0,									-40,
			palRed (color + rnum),				palGreen (color + rnum),			palBlue (color + rnum),
			palRed (color + rnum2),				palGreen (color + rnum2),			palBlue (color + rnum2),
			1,									-1.0f / (0.5f + (frand () * 0.3f)),
			1,									1,
			PT_GENERIC,							PF_SCALED,
			0,									NULL,								NULL,
			PART_STYLE_QUAD,
			0);
	}
}


/*
===============
CG_ParticleEffect3
===============
*/
void __fastcall CG_ParticleEffect3 (vec3_t org, vec3_t dir, int color, int count)
{
	int			i, rnum, rnum2;
	float		d;

	for (i=0 ; i<count ; i++) {
		d = frand () * 7;
		rnum = (rand () % 5);
		rnum2 = (rand () % 5);

		CG_SpawnParticle (
			org[0] + (crand () * 4) + d*dir[0],	org[1] + (crand () * 4) + d*dir[1],	org[2] + (crand () * 4) + d*dir[2],
			0,									0,									0,
			crand () * 20,						crand () * 20,						crand () * 20,
			0,									0,									40,
			palRed (color + rnum),				palGreen (color + rnum),			palBlue (color + rnum),
			palRed (color + rnum2),				palGreen (color + rnum2),			palBlue (color + rnum2),
			1.0f,								-1.0f / (0.5f + (frand () * 0.3f)),
			1,									1,
			PT_GENERIC,							PF_SCALED,
			0,									NULL,								NULL,
			PART_STYLE_QUAD,
			0);
	}
}


/*
===============
CG_ParticleSmokeEffect

like the steam effect, but unaffected by gravity
===============
*/
void __fastcall CG_ParticleSmokeEffect (vec3_t org, vec3_t dir, int color, int count, int magnitude)
{
	int			i, rnum, rnum2;
	float		d;
	vec3_t		r, u, pvel;

	MakeNormalVectorsf (dir, r, u);

	for (i=0 ; i<count ; i++) {
		Vec3Scale (dir, magnitude, pvel);
		d = crand() * magnitude / 3;
		Vec3MA (pvel, d, r, pvel);
		d = crand() * magnitude/ 3;
		Vec3MA (pvel, d, u, pvel);

		rnum = (rand () % 5);
		rnum2 = (rand () % 5);
		CG_SpawnParticle (
			org[0] + magnitude*0.1f*crand(),org[1] + magnitude*0.1f*crand(),org[2] + magnitude*0.1f*crand(),
			0,								0,								0,
			pvel[0],						pvel[1],						pvel[2],
			0,								0,								0,
			palRed (color + rnum),			palGreen (color + rnum),		palBlue (color + rnum),
			palRed (color + rnum2),			palGreen (color + rnum2),		palBlue (color + rnum2),
			0.9f + (crand () * 0.1f),		-1.0f / (0.5f + (cg.smokeLingerScale * 5.0f) + (frand () * 0.3f)),
			5 + (frand () * 4),				10 + (frand () * 4),
			pRandSmoke (),					0,
			pSmokeThink,					pLight70Think,					NULL,
			PART_STYLE_QUAD,
			frand () * 360);
	}
}


/*
===============
CG_RicochetEffect
===============
*/
void __fastcall CG_RicochetEffect (vec3_t org, vec3_t dir, int count)
{
	int		i, rnum, rnum2;
	float	d;

	// Bullet mark
	CG_SpawnDecal (
		org[0],								org[1],								org[2],
		dir[0],								dir[1],								dir[2],
		255,								255,								255,
		0,									0,									0,
		0.9f + (crand () * 0.1f),			0.8f,
		4 + crand (),
		DT_BULLET,							DF_ALPHACOLOR,
		0,									false,
		0,									frand () * 360);

	// Dots
	for (i=0 ; i<count ; i++) {
		d = (float)(rand()%17);
		rnum = (rand()%3) + 2;
		rnum2 = (rand()%5);

		CG_SpawnParticle (
			org[0] + ((rand()%7)-3) + d*dir[0],	org[1] + ((rand()%7)-3) + d*dir[1],	org[2] + ((rand()%7)-3) + d*dir[2],
			0,									0,									0,
			dir[0] * crand () * 3,				dir[1] * crand () * 3,				dir[2] * crand () * 3,
			(dir[0] * (crand() * 8)) + ((rand()%7)-3),
			(dir[1] * (crand() * 8)) + ((rand()%7)-3),
			(dir[2] * (crand() * 8)) + ((rand()%7)-3) - (40 * frand() * 1.5f),
			palRed (rnum),						palGreen (rnum),					palBlue (rnum),
			palRed (rnum2),						palGreen (rnum2),					palBlue (rnum2),
			1.0f,								-1.0f / (0.5f + (frand() * 0.2f)),
			0.5f,								0.6f,
			PT_GENERIC,							PF_SCALED|PF_NOCLOSECULL,
			0,									NULL,								NULL,
			PART_STYLE_QUAD,
			0);
	}

	CG_SparkEffect (org, dir, 10, 10, count/2, 0.9f, 1.5f);
}


/*
===============
CG_RocketFireParticles
===============
*/
void CG_RocketFireParticles (vec3_t org, vec3_t dir)
{
	float	rnum, rnum2;

	// FIXME: need to use dir to tell smoke where to fly to
	rnum = 60 + (frand () * 50);
	rnum2 = 70 + (frand () * 50);

	CG_SpawnParticle (
		org[0] + (crand () * 4),		org[1] + (crand () * 4),		org[2] + (crand () * 4) + 16,
		0,								0,								0,
		crand () * 2,					crand () * 2,					crand () * 2,
		crand () * 2,					crand () * 2,					crand () + (frand () * 4),
		rnum,							rnum,							rnum,
		rnum2,							rnum2,							rnum2,
		0.4f,							-1.0f / (1.75f + (crand () * 0.25f)),
		65 + (crand () * 10),			450 + (crand () * 10),
		pRandSmoke (),					0,
		pSmokeThink,					pLight70Think,					NULL,
		PART_STYLE_QUAD,
		frand () * 360);
}


/*
===============
CG_SparkEffect
===============
*/
void __fastcall CG_SparkEffect (vec3_t org, vec3_t dir, int color, int colorvel, int count, float smokeScale, float lifeScale)
{
	int			i, j;
	float		d, d2;
	float		rnum, rnum2;

	// Smoke
	j = 3 + (rand () & 1);
	for (i=1 ; i<j ; i++) {
		rnum = 60 + (frand () * 50);
		rnum2 = 70 + (frand () * 50);
		CG_SpawnParticle (
			org[0] + (i*dir[0]*2.5f) + crand (),org[1] + (i*dir[1]*2.5f) + crand (),org[2] + (i*dir[2]*2.5f) + crand (),
			0,									0,									0,
			0,									0,									0,
			0,									0,									i*3.5f,
			rnum,								rnum,								rnum,
			rnum2,								rnum2,								rnum2,
			0.9f + (crand () * 0.1f),			-1.0f / (1.5f + (cg.smokeLingerScale * 0.5f) + (crand() * 0.2f)),
			(4 + (frand () * 3)) * smokeScale,	(12 + (crand () * 3)) * smokeScale,
			pRandSmoke (),						PF_NOCLOSECULL,
			pFastSmokeThink,					pLight70Think,						NULL,
			PART_STYLE_QUAD,
			frand () * 360);
	}

	// Burst smoke
	j = 5 + (rand () % 3);
	for (i=1 ; i<j ; i++) {
		rnum = 60 + (frand () * 50);
		rnum2 = 70 + (frand () * 50);
		CG_SpawnParticle (
			org[0]+(i*dir[0]*3.25f)+crand()*2,	org[1]+(i*dir[1]*3.25f)+crand()*2,	org[2]+(i*dir[2]*3.25f)+crand()*2,
			0,									0,									0,
			0,									0,									0,
			0,									0,									5,
			rnum,								rnum,								rnum,
			rnum2,								rnum2,								rnum2,
			0.9f + (crand () * 0.1f),			-1.0f / (1.25f + (cg.smokeLingerScale * 0.5f) + (crand() * 0.2f)),
			(4 + (frand () * 3)) * smokeScale,	(12 + (crand () * 3)) * smokeScale,
			pRandSmoke (),						PF_NOCLOSECULL,
			pFastSmokeThink,					pLight70Think,						NULL,
			PART_STYLE_QUAD,
			frand () * 360);
	}

	// Sparks
	for (i=0 ; i<count ; i++) {
		d = 140 + (crand () * 40) * lifeScale;
		d2 = 1 + crand ();
		rnum = (float)(rand () % 5);
		rnum2 = (float)(rand () % 5);

		CG_SpawnParticle (
			org[0] + (dir[0] * d2) + crand (),	org[1] + (dir[1] * d2) + crand (),	org[2] + (dir[2] * d2) + crand (),
			0,									0,									0,
			(dir[0] * d) + (crand () * 24),		(dir[1] * d) + (crand () * 24),		(dir[2] * d) + (crand () * 24),
			0,									0,									0,
			palRed ((int)(color + rnum)),		palGreen ((int)(color + rnum)),		palBlue ((int)(color + rnum)),
			palRed ((int)(colorvel + rnum2)),	palGreen ((int)(colorvel + rnum2)),	palBlue ((int)(colorvel + rnum2)),
			1,									-1.0f / (0.175f + (frand() * 0.05f)),
			0.3f,								0.3f + (frand () * 0.6f),
			PT_SPARK,							PF_ALPHACOLOR,
			pRicochetSparkThink,				NULL,								NULL,
			PART_STYLE_DIRECTION,
			16 + (crand () * 4));
	}
}


/*
===============
CG_SplashEffect
===============
*/
void __fastcall CG_SplashParticles (vec3_t org, vec3_t dir, int color, int count, bool glow)
{
	int		i, rnum, rnum2;
	vec3_t	angle, dirVec;
	float	d;

	// Ripple
	rnum = (rand () % 5);
	rnum2 = (rand () % 5);

	VecToAngleRolled (dir, frand () * 360, angle);
	CG_SpawnParticle (
		org[0] + dir[0],					org[1] + dir[0],					org[2] + dir[0],
		angle[0],							angle[1],							angle[2],
		0,									0,									0,
		0,									0,									0,
		palRed (color + rnum) + 64,			palGreen (color + rnum) + 64,		palBlue (color + rnum) + 64,
		palRed (color + rnum2) + 64,		palGreen (color + rnum2) + 64,		palBlue (color + rnum2) + 64,
		0.5f + (crand () * 0.1f),			-1.0f / (1.9f + (frand () * 0.1f)),
		7 + (crand () * 2),					24 + (crand () * 3),
		PT_WATERRIPPLE,						PF_SCALED,
		NULL,								NULL,								NULL,
		PART_STYLE_ANGLED,
		0);

	// Ring
	rnum = (rand () % 5);
	rnum2 = (rand () % 5);

	VecToAngleRolled (dir, frand () * 360, angle);
	CG_SpawnParticle (
		org[0] + dir[0],					org[1] + dir[0],					org[2] + dir[0],
		angle[0],							angle[1],							angle[2],
		0,									0,									0,
		0,									0,									0,
		palRed (color + rnum) + 64,			palGreen (color + rnum) + 64,		palBlue (color + rnum) + 64,
		palRed (color + rnum2) + 64,		palGreen (color + rnum2) + 64,		palBlue (color + rnum2) + 64,
		0.9f + (crand () * 0.1f),			-1.0f / (0.5f + (frand () * 0.1f)),
		2 + crand (),						15 + (crand () * 2),
		PT_WATERRING,						PF_SCALED,
		NULL,								NULL,								NULL,
		PART_STYLE_ANGLED,
		0);

	// Impact
	rnum = (rand () % 5);
	rnum2 = (rand () % 5);

	VecToAngleRolled (dir, frand () * 360, angle);
	CG_SpawnParticle (
		org[0] + dir[0],					org[1] + dir[0],					org[2] + dir[0],
		angle[0],							angle[1],							angle[2],
		0,									0,									0,
		0,									0,									0,
		palRed (color + rnum) + 64,			palGreen (color + rnum) + 64,		palBlue (color + rnum) + 64,
		palRed (color + rnum2) + 64,		palGreen (color + rnum2) + 64,		palBlue (color + rnum2) + 64,
		0.5f + (crand () * 0.1f),			-1.0f / (0.2f + (frand () * 0.1f)),
		5 + (crand () * 2),					25 + (crand () * 3),
		PT_WATERIMPACT,						PF_SCALED,
		NULL,								NULL,								NULL,
		PART_STYLE_ANGLED,
		0);

	// Mist
	for (i=0 ; i<2 ; i++) {
		d = 1 + (frand () * 3);
		rnum = (rand () % 5);
		rnum2 = (rand () % 5);

		CG_SpawnParticle (
			org[0] + (dir[0] * d),				org[1] + (dir[1] * d),				org[2] + (dir[2] * d),
			0,									0,									0,
			d * dir[0],							d * dir[1],							d * dir[2],
			0,									0,									100,
			palRed (color + rnum) + 64,			palGreen (color + rnum) + 64,		palBlue (color + rnum) + 64,
			palRed (color + rnum2) + 64,		palGreen (color + rnum2) + 64,		palBlue (color + rnum2) + 64,
			0.6f + (crand () * 0.1f),			-1.0f / (0.4f + (frand () * 0.1f)),
			5 + (crand () * 5),					35 + (crand () * 5),
			glow ? PT_WATERMIST_GLOW : PT_WATERMIST,
			PF_SCALED|PF_GRAVITY|PF_NOCLOSECULL,
			0,									NULL,								NULL,
			PART_STYLE_QUAD,
			frand () * 360);
	}

	// Point-away plume
	d = 10 + crand ();
	rnum = (rand () % 5);
	rnum2 = (rand () % 5);
	CG_SpawnParticle (
		org[0] + (dir[0] * 7.5f),			org[1] + (dir[1] * 7.5f),			org[2] + (dir[2] * 7.5f),
		dir[0],								dir[1],								dir[2],
		0,									0,									0,
		0,									0,									0,
		palRed (color + rnum) + 64,			palGreen (color + rnum) + 64,		palBlue (color + rnum) + 64,
		palRed (color + rnum2) + 64,		palGreen (color + rnum2) + 64,		palBlue (color + rnum2) + 64,
		1.0f,								-1.0f / (0.2f + (frand() * 0.2f)),
		9,									9,
		glow ? PT_WATERPLUME_GLOW : PT_WATERPLUME,		0,
		NULL,								NULL,								NULL,
		PART_STYLE_DIRECTION,
		0);

	// Flying droplets
	for (i=0 ; i<count*2 ; i++) {
		d = 34 + (frand () * 14);
		rnum = (rand () % 5);
		rnum2 = (rand () % 5);

		dirVec[0] = dir[0] + crand ();
		dirVec[1] = dir[1] + crand ();
		dirVec[2] = dir[2] + crand ();

		CG_SpawnParticle (
			org[0] + dir[0],					org[1] + dir[1],					org[2] + dir[2],
			0,									0,									0,
			dirVec[0] * (frand () * 74),		dirVec[1] * (frand () * 74),		dirVec[2] * (frand () * 74),
			0,									0,									50,
			palRed (color + rnum) + 64,			palGreen (color + rnum) + 64,		palBlue (color + rnum) + 64,
			palRed (color + rnum2) + 64,		palGreen (color + rnum2) + 64,		palBlue (color + rnum2) + 64,
			0.7f + (frand () * 0.3f),			-1.0f / (0.5f + (frand () * 0.3f)),
			1.5f + crand (),					0.15 + (crand () * 0.125f),
			PT_WATERDROPLET,					PF_GRAVITY|PF_NOCLOSECULL,
			pAirOnlyThink,						pDropletThink,						NULL,
			PART_STYLE_QUAD,
			frand () * 360);
	}
}

byte		clrtbl[] = {0x00, 0xe0, 0xb0, 0x50, 0xd0, 0xe0, 0xe8};
void __fastcall CG_SplashEffect (vec3_t org, vec3_t dir, int color, int count)
{
	// this function merely decides what route to take
	switch (color) {
	case SPLASH_UNKNOWN:
		CG_RicochetEffect (org, dir, count);
		break;
	case SPLASH_SPARKS:
		CG_SparkEffect (org, dir, 12, 12, count, 1, 4);
		break;
	case SPLASH_BLUE_WATER:
		CG_SplashParticles (org, dir, 0x09, count, false);
		break;
	case SPLASH_BROWN_WATER:
		CG_ParticleEffect (org, dir, clrtbl[color], count);
		break;
	case SPLASH_SLIME:
		CG_SplashParticles (org, dir, clrtbl[color], count, true);
		break;
	case SPLASH_LAVA:
		CG_SplashParticles (org, dir, clrtbl[color], count, true);
		break;
	case SPLASH_BLOOD:
		CG_BleedEffect (org, dir, count);
		break;
	}
}

/*
=============================================================================

	MISCELLANEOUS PARTICLE EFFECTS

=============================================================================
*/

/*
===============
CG_BigTeleportParticles
===============
*/
void CG_BigTeleportParticles (vec3_t org)
{
	int i;
	float angle, dist;
	float sv, cv;

	for (i=0 ; i<4096 ; i++)
	{
		angle = (M_PI * 2.0f) * (rand () & 1023) / 1023.0f;
		dist = (float)(rand () & 31);

		Q_SinCosf(angle, &sv, &cv);

		CG_SpawnParticle (
			org[0] + cv * dist,				org[1] + sv * dist,				org[2] + 8 + (frand () * 90),
			0,								0,								0,
			cv * (70+(rand()&63)),			sv * (70+(rand()&63)),			-100.0f + (rand () & 31),
			-cv * 100,						-sv * 100,						160.0f,
			255,							255,							255,
			230,							230,							230,
			1.0f,							-0.3f / (0.2f + (frand () * 0.3f)),
			10,								3,
			PT_FLAREGLOW,					PF_SCALED|PF_NOCLOSECULL,
			0,								NULL,							NULL,
			PART_STYLE_QUAD,
			0);
	}
}


/*
===============
CG_BlasterTip
===============
*/
void CG_BlasterTip (vec3_t start, vec3_t end)
{
	int		rnum, rnum2;
	vec3_t	move, vec;
	float	len, dec;
	int		i;

	// Bubbles
	CG_BubbleEffect (start);

	// Smoke
	dec = 1 + cg.smokeLingerScale;
	for (i=0 ; i<dec ; i++) {
		rnum = rand () % 5;
		rnum2 = rand () % 5;

		CG_SpawnParticle (
			start[0] + crand(),					start[1] + crand(),					start[2] + crand(),
			0,									0,									0,
			0,									0,									0,
			0,									0,									5 + (frand () * 25),
			palRed (0xe0 + rnum),				palGreen (0xe0 + rnum),				palBlue (0xe0 + rnum),
			palRed (0xe0 + rnum2),				palGreen (0xe0 + rnum2),			palBlue (0xe0 + rnum2),
			0.9f + (frand() * 0.1f),			-1.0f / (0.25f + (cg.smokeLingerScale * 0.1f) + (frand() * 0.1f)),
			2 + crand (),						12 + (crand () * 2),
			pRandGlowSmoke (),					PF_ALPHACOLOR,
			NULL,								NULL,								NULL,
			PART_STYLE_QUAD,
			frand () * 360);
	}

	// Dot trail
	Vec3Copy (start, move);
	Vec3Subtract (start, end, vec);
	len = VectorNormalizeFastf (vec);

	dec = 2.5f;
	Vec3Scale (vec, dec, vec);

	for (; len>0 ; Vec3Add (move, vec, move)) {
		len -= dec;

		rnum = rand () % 5;
		rnum2 = rand () % 5;

		CG_SpawnParticle (
			move[0] + crand (),					move[1] + crand (),					move[2] + crand (),
			0,									0,									0,
			crand () * 2,						crand () * 2,						crand () * 2,
			crand () * 2,						crand () * 2,						crand () * 2,
			palRed (0xe0 + rnum),				palGreen (0xe0 + rnum),				palBlue (0xe0 + rnum),
			palRed (0xe0 + rnum2),				palGreen (0xe0 + rnum2),			palBlue (0xe0 + rnum2),
			1.0f,								-15,
			3 + frand (),						1.5f + frand (),
			PT_BLASTER_RED,						PF_NOCLOSECULL,
			0,									NULL,								NULL,
			PART_STYLE_QUAD,
			0);
	}
}


/*
===============
CG_ExplosionParticles
===============
*/
void CG_ExplosionParticles (vec3_t org, float scale, bool exploOnly, bool inWater)
{
	int			i, j;
	float		rnum, rnum2;
	float		waterOffset;
	vec3_t		normal, angle, endPos;
	float		distScale;
	bool		decal;

	CG_ExploRattle (org, scale);

	if (inWater) {
		waterOffset = 155;

		// Bubbles
		for (i=0 ; i<50*scale ; i++) {
			rnum = 230 + (frand () * 25);
			rnum2 = 230 + (frand () * 25);
			CG_SpawnParticle (
				org[0] + crand (),				org[1] + crand (),				org[2] + crand (),
				0,								0,								0,
				crand () * (164 * scale),		crand () * (164 * scale),		10 + (crand () * (164 * scale)),
				0,								0,								0,
				rnum,							rnum,							rnum,
				rnum2,							rnum2,							rnum2,
				0.9f + (crand () * 0.1f),		-1.0f / (1 + (frand () * 0.2f)),
				0.1f + frand (),				0.1f + frand (),
				PT_WATERBUBBLE,					0,
				pWaterOnlyThink,				pLight70Think,					NULL,
				PART_STYLE_QUAD,
				0);
		}
	}
	else
		waterOffset = 0;

	if (!exploOnly) {
		// Smoke
		j = scale + 3;
		for (i=0 ; i<j ; i++) {
			rnum = 70 + (frand () * 40);
			rnum2 = 80 + (frand () * 40);
			CG_SpawnParticle (
				org[0] + (crand () * 4),		org[1] + (crand () * 4),		org[2] + (crand () * 4),
				0,								0,								0,
				crand () * 2,					crand () * 2,					crand () * 2,
				0,								0,								3 + (frand () * 4),
				20 + rnum,						10 + rnum,						rnum,
				10 + rnum2,						5 + rnum2,						rnum2,
				0.9f + (crand () * 0.1f),		-1.0f / (2.0f + (cg.smokeLingerScale * 3.0f) + (crand () * 0.2f)),
				(40 + (crand () * 5)) * scale,	(100 + (crand () * 10)) * scale,
				pRandSmoke (),					0,
				pSmokeThink,					pLight70Think,					NULL,
				PART_STYLE_QUAD,
				frand () * 361);
		}
	}

	// Sparks
	for (i=0 ; i<20 ; i++) {
		CG_SpawnParticle (
			org[0] + (crand () * 10 * scale),	org[1] + (crand () * 10 * scale),	org[2] + (crand () * 10 * scale),
			0,									0,									0,
			crand () * (140 * scale),			crand () * (140 * scale),			crand () * (140 * scale),
			0,									0,									0,
			235 + (frand () * 20),				225 + (frand () * 20),				205,
			235 + (frand () * 20),				225 + (frand () * 20),				205,
			0.9f,								-1.5f / (0.6f + (crand () * 0.15f)),
			0.3f,								0.3f + (frand () * 0.6f),
			PT_SPARK,							0,
			pSparkGrowThink,					NULL,								NULL,
			PART_STYLE_DIRECTION,
			(16 + (crand () * 4)) * scale);
	}

	// Explosion anim
	CG_SpawnParticle (
		org[0],								org[1],								org[2],
		0,									0,									0,
		0,									0,									0,
		0,									0,									0,
		255 - waterOffset,					255 - waterOffset,					255,
		255 - waterOffset,					255 - waterOffset,					255,
		1.0f,								-2.5f + (crand () * 0.1f) + (scale * 0.33f),
		(40 + (crand () * 5)) * scale,		(120 + (crand () * 10)) * scale,
		PT_EXPLO1,							PF_NOCLOSECULL,
		pExploAnimThink,					NULL,								NULL,
		PART_STYLE_QUAD,
		crand () * 12);

	// Explosion embers
	for (i=0 ; i<2 ; i++) {
		CG_SpawnParticle (
			org[0] + (crand () * 3),			org[1] + (crand () * 3),			org[2] + (crand () * 3),
			0,									0,									0,
			0,									0,									0,
			0,									0,									0,
			255 - waterOffset,					255 - waterOffset,					255,
			255 - waterOffset,					255 - waterOffset,					255,
			1.0f,								-2.25f + (crand () * 0.1f) + (scale * 0.33f),
			(2 + crand ()) * scale,				(140 + (crand () * 10)) * scale,
			i?PT_EXPLOEMBERS1:PT_EXPLOEMBERS2,	PF_NOCLOSECULL,
			0,									NULL,								NULL,
			PART_STYLE_QUAD,
			crand () * 360);
	}

	if (exploOnly)
		return;

	// Explosion flash
	CG_SpawnParticle (
		org[0],								org[1],								org[2],
		0,									0,									0,
		crand () * 20,						crand () * 20,						crand () * 20,
		0,									0,									0,
		255 - waterOffset,					255 - waterOffset,					255,
		255 - waterOffset,					255 - waterOffset,					255,
		1.0f,								-4.5f + (crand () * 0.1f) + (scale * 0.33f),
		(2 + crand ()) * scale,				(125 + (crand () * 10)) * scale,
		PT_EXPLOFLASH,						PF_NOCLOSECULL,
		0,									NULL,								NULL,
		PART_STYLE_QUAD,
		crand () * 360);

	// Explosion mark
	decal = CG_FindExplosionDir (org, 30 * scale, endPos, normal);
	if (!decal)
		Vec3Set (normal, 0, 0, 1);

	// Directional smoke
	j = scale + 3;
	for (i=1 ; i<j ; i++) {
		// Color randomization
		rnum = 70 + (frand () * 40);
		rnum2 = 80 + (frand () * 40);

		distScale = (float)i/(float)j;

		// Spawn particles
		CG_SpawnParticle (
			org[0] + (crand()*4) + (normal[0]*8*distScale),
			org[1] + (crand()*4) + (normal[1]*8*distScale),
			org[2] + (crand()*4) + (normal[2]*8*distScale),
			0,								0,								0,
			normal[0] * 90 * distScale,		normal[1] * 90 * distScale,		normal[2] * 90 * distScale,
			normal[0] * -32 * distScale,	normal[1] * -32 * distScale,	(normal[2] * -32 * distScale) + 5 + (frand () * 6),
			20 + rnum,						10 + rnum,						rnum,
			10 + rnum2,						5 + rnum2,						rnum2,
			0.85f + (crand () * 0.1f),		-1.0f / (1.5f + cg.smokeLingerScale + (crand () * 0.2f)),
			(30 + (crand () * 5)) * scale,	(120 + (crand () * 10)) * scale,
			pRandSmoke (),					0,
			pSmokeThink,					pLight70Think,					NULL,
			PART_STYLE_QUAD,
			frand () * 361);
	}

	// Directional sparks
	for (i=0 ; i<25 ; i++) {
		CG_SpawnParticle (
			org[0] + (crand () * 10 * scale),	org[1] + (crand () * 10 * scale),	org[2] + (crand () * 10 * scale),
			0,									0,									0,
			(normal[0] + (crand () * 0.4f)) * 175,
			(normal[1] + (crand () * 0.4f)) * 175,
			(normal[2] + (crand () * 0.4f)) * 175,
			0,									0,									0,
			235 + (frand () * 20),				225 + (frand () * 20),				205,
			235 + (frand () * 20),				225 + (frand () * 20),				205,
			0.9f,								-1.5f / (0.5f + (crand () * 0.15f)),
			0.3f,								0.3f + (frand () * 0.6f),
			PT_SPARK,							0,
			pSparkGrowThink,					NULL,								NULL,
			PART_STYLE_DIRECTION,
			(16 + (crand () * 4)) * scale);
	}

	if (!decal)
		return;

	// Burn mark
	CG_SpawnDecal (
		endPos[0],							endPos[1],							endPos[2],
		normal[0],							normal[1],							normal[2],
		255,								255,								255,
		0,									0,									0,
		0.9f + (crand () * 0.1f),			0.8f,
		(35 + (frand () * 5)) * scale,
		dRandExploMark (),					DF_ALPHACOLOR,
		0,									false,
		0,									frand () * 360);

	// Only do these for small effects
	if (scale <= 2.0f) {
		VecToAngleRolled (normal, 180, angle);
		rnum = Vec3DistFast (org, endPos);
		if (rnum < 50.0f) {
			// Wave
			CG_SpawnParticle (
				endPos[0] + normal[0],				endPos[1] + normal[0],				endPos[2] + normal[0],
				angle[0],							angle[1],							angle[2],
				0,									0,									0,
				0,									0,									0,
				255 - waterOffset,					155 - waterOffset,					waterOffset,
				255 - waterOffset,					155 - waterOffset,					waterOffset,
				0.7f + (crand () * 0.1f),			-1.0f / (0.1f + (frand () * 0.05f)),
				5 + (crand () * 2),					(60 + (crand () * 5)) * scale,
				PT_EXPLOWAVE,						PF_SCALED,
				NULL,								NULL,								NULL,
				PART_STYLE_ANGLED,
				0);
		}

		// Smoke
		for (j=0 ; j<1.25f + (cg.smokeLingerScale*0.5f) ; j++) {
			rnum = 60 + (frand () * 50);
			rnum2 = 70 + (frand () * 50);
			CG_SpawnParticle (
				endPos[0] + normal[0],				endPos[1] + normal[0],				endPos[2] + normal[0],
				angle[0],							angle[1],							angle[2],
				0,									0,									0,
				0,									0,									0,
				rnum,								rnum,								rnum,
				rnum2,								rnum2,								rnum2,
				0.6f + (crand () * 0.1f),			-1.0f / (1.65f + (cg.smokeLingerScale * 5.0f) + (crand () * 0.2f)),
				(60 + (crand () * 5)) * scale,		(80 + (crand () * 10)) * scale,
				pRandSmoke (),						0,
				pSmokeThink,						pLight70Think,						NULL,
				PART_STYLE_ANGLED,
				frand () * 361);
		}
	}
}


/*
===============
CG_ExplosionBFGParticles
===============
*/
void CG_ExplosionBFGParticles (vec3_t org)
{
	int			i;
	cgDecal_t	*d;
	float		mult;
	float		rnum, rnum2;
	vec3_t		endPos, dir;

	// Smoke
	for (i=0 ; i<8 ; i++) {
		rnum = 70 + (frand () * 40);
		rnum2 = 80 + (frand () * 40);
		CG_SpawnParticle (
			org[0] + (crand () * 4),		org[1] + (crand () * 4),		org[2] + (crand () * 4),
			0,								0,								0,
			crand () * 2,					crand () * 2,					crand () * 2,
			0,								0,								5 + (frand () * 6),
			rnum,							80 + rnum,						rnum,
			rnum2,							100+ rnum2,						rnum2,
			0.75f + (crand () * 0.1f),		-1.0f / (0.25f + cg.smokeLingerScale + (crand () * 0.1f)),
			35 + (crand () * 15),			140 + (crand () * 30),
			pRandGlowSmoke (),				0,
			pSmokeThink,					NULL,							NULL,
			PART_STYLE_QUAD,
			frand () * 361);
	}

	// Dots
	mult = 2.5f;
	for (i=0 ; i<196 ; i++) {
		rnum = (rand () % 2) ? 150 + (frand () * 25) : 0;
		CG_SpawnParticle (
			org[0] + (crand () * 16),		org[1] + (crand () * 16),			org[2] + (crand () * 16),
			0,								0,									0,
			(crand () * 192) * mult,		(crand () * 192) * mult,			(crand () * 192) * mult,
			0,								0,									-40,
			rnum,							rnum + 75 + (frand () * 150),		rnum + (frand () * 50),
			rnum,							rnum + 75 + (frand () * 150),		rnum + (frand () * 50),
			1.0f,							-0.8f / (0.8f + (frand () * 0.3f)),
			11 + (crand () * 10.5f),		0.1f + (frand () * 0.5f),
			PT_BFG_DOT,						PF_SCALED|PF_GRAVITY|PF_NOCLOSECULL,
			pBounceThink,					NULL,								NULL,
			PART_STYLE_QUAD,
			0);
	}

	// Decal
	if (!CG_FindExplosionDir (org, 64, endPos, dir))
		return;

	// Burn mark
	d = CG_SpawnDecal (
		endPos[0],							endPos[1],							endPos[2],
		dir[0],								dir[1],								dir[2],
		255,								255,								255,
		0,									0,									0,
		0.9f + (crand () * 0.1f),			0.8f,
		40 + (crand () * 3) - 8,
		DT_BFG_BURNMARK,					DF_ALPHACOLOR,
		NULL,								false,
		0,									frand () * 360);

	// Glow mark
	d = CG_SpawnDecal (
		endPos[0],							endPos[1],							endPos[2],
		dir[0],								dir[1],								dir[2],
		255,								255,								255,
		0,									0,									0,
		1.0f,								0,
		40 + (crand () * 3) - 8,
		DT_BFG_GLOWMARK,					DF_USE_BURNLIFE|DF_ALPHACOLOR,
		NULL,								false,
		0,									frand () * 360);
}


/*
===============
CG_ExplosionColorParticles
===============
*/
void CG_ExplosionColorParticles (vec3_t org)
{
	int			i;

	for (i=0 ; i<128 ; i++) {
		CG_SpawnParticle (
			org[0] + (crand () * 16),		org[1] + (crand () * 16),			org[2] + (crand () * 16),
			0,								0,									0,
			crand () * 128,					crand () * 128,						crand () * 128,
			0,								0,									-40,
			0 + crand (),					0 + crand (),						0 + crand (),
			0 + crand (),					0 + crand (),						0 + crand (),
			1.0f,							-0.4f / (0.6f + (frand () * 0.2f)),
			1.0f,							1.0f,
			PT_GENERIC,						PF_SCALED|PF_NOCLOSECULL,
			0,								NULL,								NULL,
			PART_STYLE_QUAD,
			0);
	}
}


/*
===============
CG_FlyEffect
===============
*/
void CG_FlyParticles (vec3_t origin, int count)
{
	int			i;
	float		angle;
	float		sr, sp, sy, cr, cp, cy;
	vec3_t		forward;
	float		dist = 64;
	float		ltime;

	if (count > NUMVERTEXNORMALS)
		count = NUMVERTEXNORMALS;

	ltime = (float)cg.refreshTime / 1000.0f;
	for (i=0 ; i<count ; i+=2) {
		angle = ltime * cg_randVels[i][0];
		Q_SinCosf(angle, &sy, &cy);
		angle = ltime * cg_randVels[i][1];
		Q_SinCosf(angle, &sp, &cp);
		angle = ltime * cg_randVels[i][2];
		Q_SinCosf(angle, &sr, &cr);
	
		forward[0] = cp*cy;
		forward[1] = cp*sy;
		forward[2] = -sp;

		dist = sinf(ltime + i) * 64;

		CG_SpawnParticle (
			origin[0] + (m_byteDirs[i][0] * dist) + (forward[0] * BEAMLENGTH),
			origin[1] + (m_byteDirs[i][1] * dist) + (forward[1] * BEAMLENGTH),
			origin[2] + (m_byteDirs[i][2] * dist) + (forward[2] * BEAMLENGTH),
			0,								0,								0,
			0,								0,								0,
			0,								0,								0,
			0,								0,								0,
			0,								0,								0,
			1,								-100,
			1.5f,							1.5f,
			PT_FLY,							PF_NOCLOSECULL,
			0,								NULL,							NULL,
			PART_STYLE_QUAD,
			0);
	}
}

void CG_FlyEffect (cgEntity_t *ent, vec3_t origin)
{
	int		n;
	float	count;
	int		starttime;

	if (ent->flyStopTime < cg.refreshTime) {
		starttime = cg.refreshTime;
		ent->flyStopTime = cg.refreshTime + 60000;
	}
	else
		starttime = ent->flyStopTime - 60000;

	n = cg.refreshTime - starttime;
	if (n < 20000)
		count = n * 162 / 20000.0f;
	else {
		n = ent->flyStopTime - cg.refreshTime;
		if (n < 20000)
			count = n * 162 / 20000.0f;
		else
			count = 162;
	}

	CG_FlyParticles (origin, count*2);
}


/*
===============
CG_ForceWall
===============
*/
void CG_ForceWall (vec3_t start, vec3_t end, int color)
{
	vec3_t		move, vec;
	float		len, dec;

	Vec3Copy (start, move);
	Vec3Subtract (end, start, vec);
	len = VectorNormalizeFastf (vec);

	dec = 4;
	Vec3Scale (vec, dec, vec);

	for (; len>0 ; Vec3Add (move, vec, move)) {
		len -= dec;
		
		if (frand () > 0.3) {
			CG_SpawnParticle (
				move[0] + (crand () * 3),		move[1] + (crand () * 3),		move[2] + (crand () * 3),
				0,								0,								0,
				0,								0,								-40 - (crand () * 10),
				0,								0,								0,
				palRed (color),					palGreen (color),				palBlue (color),
				palRed (color),					palGreen (color),				palBlue (color),
				1.0f,							-1.0f / (3.0f + (frand () * 0.5f)),
				1.0f,							1.0f,
				PT_GENERIC,						PF_SCALED,
				0,								NULL,							NULL,
				PART_STYLE_QUAD,
				0);
		}
	}
}


/*
===============
CG_MonsterPlasma_Shell
===============
*/
void CG_MonsterPlasma_Shell (vec3_t origin)
{
	vec3_t			dir, porg;
	int				i, rnum, rnum2;

	for (i=0 ; i<40 ; i++) {
		Vec3Set (dir, crand (), crand (), crand ());
		VectorNormalizeFastf (dir);
		Vec3MA (origin, 10, dir, porg);

		rnum = (rand () % 5);
		rnum2 = (rand () % 5);
		CG_SpawnParticle (
			porg[0],						porg[1],						porg[2],
			0,								0,								0,
			0,								0,								0,
			0,								0,								0,
			palRed (0xe0 + rnum),			palGreen (0xe0 + rnum),			palBlue (0xe0 + rnum),
			palRed (0xe0 + rnum2),			palGreen (0xe0 + rnum2),		palBlue (0xe0 + rnum2),
			1.0,							PART_INSTANT,
			1.0,							1.0,
			PT_GENERIC,						PF_SCALED,
			0,								NULL,							NULL,
			PART_STYLE_QUAD,
			0);
	}
}


/*
===============
CG_PhalanxTip
===============
*/
void CG_PhalanxTip (vec3_t start, vec3_t end)
{
	int		i, j, k;
	int		rnum, rnum2;
	vec3_t	move, vec, dir;
	float	len, dec, vel;

	// Bubbles
	CG_BubbleEffect (start);

	// Smoke
	dec = 1 + (cg.smokeLingerScale * 2.0f);
	for (i=0 ; i<dec ; i++) {
		rnum = (rand () % 5);
		rnum2 = (rand () % 5);

		CG_SpawnParticle (
			start[0] + (crand()*2),				start[1] + (crand()*2),				start[2] + (crand()*2),
			0,									0,									0,
			0,									0,									0,
			0,									0,									5 + (frand () * 25),
			palRed (0xe0 + rnum),				palGreen (0xe0 + rnum),				palBlue (0xe0 + rnum),
			palRed (0xe0 + rnum2),				palGreen (0xe0 + rnum2),			palBlue (0xe0 + rnum2),
			0.9f + (frand() * 0.1f),			-1.0f / (0.25f + (cg.smokeLingerScale * 0.1f) + (frand() * 0.1f)),
			5 + crand (),						16 + (crand () * 8),
			pRandGlowSmoke (),					PF_ALPHACOLOR,
			NULL,								NULL,								NULL,
			PART_STYLE_QUAD,
			frand () * 360);
	}

	// Trail
	Vec3Copy (start, move);
	Vec3Subtract (start, end, vec);
	len = VectorNormalizeFastf (vec);

	dec = 2.5f;
	Vec3Scale (vec, dec, vec);

	for (; len>0 ; Vec3Add (move, vec, move)) {
		len -= dec;

		rnum = (rand () % 5);
		rnum2 = (rand () % 5);

		CG_SpawnParticle (
			move[0] + crand (),					move[1] + crand (),					move[2] + crand (),
			0,									0,									0,
			crand () * 2,						crand () * 2,						crand () * 2,
			crand () * 2,						crand () * 2,						crand () * 2,
			palRed (0xe0 + rnum),				palGreen (0xe0 + rnum),				palBlue (0xe0 + rnum),
			palRed (0xe0 + rnum2),				palGreen (0xe0 + rnum2),			palBlue (0xe0 + rnum2),
			1.0,								-15,
			5 + (frand () * 4),					3 + (frand () * 2.5f),
			PT_PHALANXTIP,						0,
			0,									NULL,								NULL,
			PART_STYLE_QUAD,
			0);
	}

	for (i=-2 ; i<=2 ; i+=4) {
		for (j=-2 ; j<=2 ; j+=4) {
			for (k=-2 ; k<=4 ; k+=4) {
				Vec3Set (dir, (float)(j * 4), (float)(i * 4), (float)(k * 4));	
				VectorNormalizeFastf (dir);
				vel = 10.0f + rand () % 11;

				rnum = (rand () % 5);
				rnum2 = (rand () % 5);
				CG_SpawnParticle (
					start[0] + i + ((rand () % 6) * crand ()),
					start[1] + j + ((rand () % 6) * crand ()),
					start[2] + k + ((rand () % 6) * crand ()),
					0,									0,									0,
					dir[0] * vel,						dir[1] * vel,						dir[2] * vel,
					0,									0,									-40,
					palRed (0xe0 + rnum),				palGreen (0xe0 + rnum),				palBlue (0xe0 + rnum),
					palRed (0xe0 + rnum2),				palGreen (0xe0 + rnum2),			palBlue (0xe0 + rnum2),
					0.9f,								-3.5f,
					2 + (frand () * 0.5f),				0.5f + (frand () * 0.5f),
					PT_GENERIC,							PF_NOCLOSECULL,
					0,									NULL,								NULL,
					PART_STYLE_QUAD,
					0);
			}
		}
	}
}


/*
===============
CG_TeleportParticles
===============
*/
void CG_TeleportParticles (vec3_t org)
{
	int		i;

	for (i=0 ; i<300 ; i++) {
		CG_SpawnParticle (
			org[0] + (crand () * 32),		org[1] + (crand () * 32),		org[2] + (frand () * 85 - 25),
			0,								0,								0,
			crand () * 50,					crand () * 50,					crand () * 50,
			crand () * 50,					crand () * 50,					50 + (crand () * 20),
			220,							190,							150,
			255,							255,							230,
			0.9f + (frand () * 0.25f),		-0.3f / (0.1f + (frand () * 0.1f)),
			10 + (frand () * 0.25f),		0.5f + (frand () * 0.25f),
			PT_FLAREGLOW,					PF_SCALED|PF_GRAVITY|PF_NOCLOSECULL,
			pBounceThink,					NULL,							NULL,
			PART_STYLE_QUAD,
			0);
	}
}


/*
===============
CG_TeleporterParticles
===============
*/
void CG_TeleporterParticles (entityState_t *ent)
{
	for (int i=0 ; i<(int)(rand()&1) ; i++)
	{
		CG_SpawnParticle(
			ent->origin[0] + (crand () * 16),	ent->origin[1] + (crand () * 16),	ent->origin[2] + (crand () * 8) - 3,
			0,									0,									0,
			crand () * 15,						crand () * 15,						80 + (frand () * 5),
			0,									0,									-40,
			210 + (crand () *  5),				180 + (crand () *  5),				120 + (crand () *  5),
			255 + (crand () *  5),				210 + (crand () *  5),				140 + (crand () *  5),
			1.0f,								-0.6f + (crand () * 0.1f),
			2 + (crand () * 0.5f),				frand () * 0.5f,
			PT_GENERIC_GLOW,					PF_SCALED|PF_NOCLOSECULL,
			0,									NULL,								NULL,
			PART_STYLE_QUAD,
			0
		);
	}
}


/*
===============
CG_TrackerShell
===============
*/
void CG_TrackerShell (vec3_t origin)
{
	vec3_t			dir, porg;
	int				i;

	for (i=0 ; i<300 ; i++) {
		Vec3Set (dir, crand (), crand (), crand ());
		VectorNormalizeFastf (dir);
		Vec3MA (origin, 40, dir, porg);

		CG_SpawnParticle (
			porg[0],						porg[1],						porg[2],
			0,								0,								0,
			0,								0,								0,
			0,								0,								0,
			0,								0,								0,
			0,								0,								0,
			1.0,							PART_INSTANT,
			1.0f,							1.0f,
			PT_GENERIC,						PF_SCALED,
			0,								NULL,							NULL,
			PART_STYLE_QUAD,
			0);
	}
}


/*
===============
CG_TrapParticles
===============
*/
void CG_TrapParticles (refEntity_t *ent)
{
	vec3_t		move, vec, start, end, dir, org;
	int			i, j, k, rnum, rnum2;
	float		len, vel, dec;

	ent->origin[2] -= 16;
	Vec3Copy (ent->origin, start);
	Vec3Copy (ent->origin, end);
	end[2] += 10;

	Vec3Copy (start, move);
	Vec3Subtract (end, start, vec);
	len = VectorNormalizeFastf (vec);

	dec = 5;
	Vec3Scale (vec, dec, vec);

	for (; len>0 ; Vec3Add (move, vec, move)) {
		len -= dec;

		rnum = (rand () % 5);
		rnum2 = (rand () % 5);
		CG_SpawnParticle (
			move[0] + (crand () * 2),		move[1] + (crand () * 1.5f),	move[2] + (crand () * 1.5f),
			0,								0,								0,
			crand () * 20,					crand () * 20,					crand () * 20,
			0,								0,								40,
			palRed (0xe0 + rnum),			palGreen (0xe0 + rnum),			palBlue (0xe0 + rnum),
			palRed (0xe0 + rnum2),			palGreen (0xe0 + rnum2),		palBlue (0xe0 + rnum2),
			1.0f,							-1.0f / (0.45f + (frand () * 0.2f)),
			5.0f,							1.0f,
			PT_GENERIC,						PF_SCALED,
			0,								NULL,							NULL,
			PART_STYLE_QUAD,
			0);
	}

	ent->origin[2]+=14;
	Vec3Copy (ent->origin, org);

	for (i=-2 ; i<=2 ; i+=4)
		for (j=-2 ; j<=2 ; j+=4)
			for (k=-2 ; k<=4 ; k+=4) {
				dir[0] = (float)(j * 8);
				dir[1] = (float)(i * 8);
				dir[2] = (float)(k * 8);
	
				VectorNormalizeFastf (dir);
				vel = 50 + (float)(rand () & 63);

				rnum = (rand () % 5);
				rnum2 = (rand () % 5);
				CG_SpawnParticle (
					org[0] + i + ((rand () & 23) * crand ()),
					org[1] + j + ((rand () & 23) * crand ()),
					org[2] + k + ((rand () & 23) * crand ()),
					0,								0,								0,
					dir[0] * vel,					dir[1] * vel,					dir[2] * vel,
					0,								0,								-40,
					palRed (0xe0 + rnum),			palGreen (0xe0 + rnum),			palBlue (0xe0 + rnum),
					palRed (0xe0 + rnum2),			palGreen (0xe0 + rnum2),		palBlue (0xe0 + rnum2),
					1.0f,							-1.0f / (0.3f + (frand () * 0.15f)),
					2.0f,							1.0f,
					PT_GENERIC,						PF_SCALED,
					0,								NULL,							NULL,
					PART_STYLE_QUAD,
					0);
			}
}


/*
===============
CG_WidowSplash
===============
*/
void CG_WidowSplash (vec3_t org)
{
	int			clrtable[4] = {2*8, 13*8, 21*8, 18*8};
	int			i, rnum, rnum2;
	vec3_t		dir, porg, pvel;

	for (i=0 ; i<256 ; i++) {
		Vec3Set (dir, crand (), crand (), crand ());
		VectorNormalizeFastf (dir);
		Vec3MA (org, 45.0f, dir, porg);
		Vec3MA (vec3Origin, 40.0f, dir, pvel);

		rnum = (rand () % 4);
		rnum2 = (rand () % 4);
		CG_SpawnParticle (
			porg[0],						porg[1],						porg[2],
			0,								0,								0,
			pvel[0],						pvel[1],						pvel[2],
			0,								0,								0,
			palRed (clrtable[rnum]),		palGreen (clrtable[rnum]),		palBlue (clrtable[rnum]),
			palRed (clrtable[rnum2]),		palGreen (clrtable[rnum2]),		palBlue (clrtable[rnum2]),
			1.0f,							-0.8f / (0.5f + (frand () * 0.3f)),
			1.0f,							1.0f,
			PT_GENERIC,						PF_SCALED,
			0,								NULL,							NULL,
			PART_STYLE_QUAD,
			0);
	}
}
