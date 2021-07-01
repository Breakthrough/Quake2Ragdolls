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
// cg_partthink.c
//

#include "cg_local.h"

/*
===============
pTrace
===============
*/
static inline cmTrace_t pTrace(vec3_t start, vec3_t end, float size)
{
	cmTrace_t tr;
	vec3_t mins, maxs;
	Vec3Set (mins, -size, -size, -size);
	Vec3Set (maxs, size, size, size);
	CG_PMTrace(&tr, start, mins, maxs, end, false, true);
	return tr;
}


/*
===============
pCalcPartVelocity
===============
*/
static void pCalcPartVelocity(cgParticle_t *p, float scale, float *time, vec3_t velocity, float gravityScale)
{
	float time1 = *time;
	float time2 = time1*time1;

	velocity[0] = scale * (p->vel[0] * time1 + (p->accel[0]) * time2);
	velocity[1] = scale * (p->vel[1] * time1 + (p->accel[1]) * time2);
	velocity[2] = scale;

	if (p->flags & PF_GRAVITY)
		velocity[2] *= p->vel[2] * time1 + (p->accel[2]-(PART_GRAVITY * gravityScale)) * time2;
	else
		velocity[2] *= p->vel[2] * time1 + (p->accel[2]) * time2;
}


/*
===============
pClipVelocity
===============
*/
static void pClipVelocity(vec3_t in, vec3_t normal, vec3_t out)
{
	float	backoff;
	int		i;
	
	backoff = Vec3LengthFast(in) * 0.25f + DotProduct(in, normal) * 3.0f;

	for (i=0 ; i<3 ; i++)
	{
		out[i] = in[i] - (normal[i] * backoff);
		if ((out[i] > -LARGE_EPSILON) && (out[i] < LARGE_EPSILON))
			out[i] = 0;
	}
}

/*
=============================================================================

	PARTICLE INTELLIGENCE

=============================================================================
*/

/*
===============
pAirOnlyThink
===============
*/
bool pAirOnlyThink(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time)
{
	if (cgi.CM_PointContents(org, 0) & CONTENTS_MASK_WATER)
	{
		// Kill it
		p->color[3] = 0;
		return false;
	}
	else
	{
		nextThinkTime = cg.refreshTime + THINK_DELAY_EXPENSIVE;
		return true;
	}
}

/*
===============
pBlasterGlowThink
===============
*/
bool pBlasterGlowThink(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time)
{
	cgi.R_AddLight(org, 200 * (color[3] / p->color[3]), 1, 1, 0);
	// FIXME: Can't set nextTimeThink because it adds a light
	return true;
}


/*
===============
pBloodThink
===============
*/
bool pBloodThink(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time)
{
	cmTrace_t	tr;
	float		clipsize, sizescale;
	bool		isGreen;
	static int	sfxDelay = -1;
	float		alpha, alphaVel;

	isGreen = (p->flags & PF_GREENBLOOD) ? true : false;

	// make a decal
	clipsize = *size * 0.1f;
	if (clipsize<0.25)
		clipsize = 0.25f;
	tr = pTrace(lastOrg, org, clipsize);

	if (tr.fraction < 1)
	{
		// Don't spawn a decal if it's inside a solid
		if (!(p->flags & PF_NODECAL) && !tr.allSolid && !tr.startSolid)
		{
			sizescale = clamp((p->size < p->sizeVel) ? (p->sizeVel / *size) : (p->size / *size), 0.75f, 1.25f);

			alpha = clamp(color[3]*3, 0, p->color[3]);
			alphaVel = alpha - 0.1f;
			if (alphaVel < 0.0f)
				alphaVel = 0.0f;

			CG_SpawnDecal(
				org[0],							org[1],							org[2],
				tr.plane.normal[0],				tr.plane.normal[1],				tr.plane.normal[2],
				isGreen ? 30.0f : 255.0f,		isGreen ? 70.0f : 255.0f,		isGreen ? 30.0f : 255.0f,
				0,								0,								0,
				alpha,							alphaVel,
				(13 + (crand()*4)) * sizescale,
				isGreen ? dRandGrnBloodMark () : dRandBloodMark (),
				DF_ALPHACOLOR,
				0,								false,
				0,								frand () * 360.0f
			);

			if (!(p->flags & PF_NOSFX) && cg.refreshTime >= sfxDelay)
			{
				sfxDelay = cg.refreshTime + 300;
				cgi.Snd_StartSound(org, 0, CHAN_AUTO, cgMedia.sfx.gibSplat[rand()%3], 0.33f, ATTN_IDLE, 0);
			}
		}

		// Kill it
		p->color[3] = 0;
		return false;
	}

	nextThinkTime = cg.refreshTime + THINK_DELAY_EXPENSIVE;
	return true;
}


/*
===============
pBloodDripThink
===============
*/
bool pBloodDripThink(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time)
{
	float	length;

	pCalcPartVelocity(p, 0.4f, time, angle, *orient);

	length = VectorNormalizeFastf(angle);
	if (length > *orient)
		length = *orient;
	Vec3Scale(angle, -length, angle);

	return pBloodThink(p, deltaTime, nextThinkTime, org, lastOrg, angle, color, size, orient, time);
}


/*
===============
pBounceThink
===============
*/
#define pBounceMaxVelocity 100
bool pBounceThink(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time)
{
	float		clipsize, length;
	cmTrace_t	tr;
	vec3_t		velocity;

	clipsize = *size*0.5f;
	if (clipsize<0.25) clipsize = 0.25;
	tr = pTrace(lastOrg, org, clipsize);

	// Don't fall through
	if (tr.startSolid || tr.allSolid)
	{
		Vec3Copy(tr.endPos, p->org);
		Vec3Copy(p->org, lastOrg);
		if (p->flags & PF_GRAVITY)
			p->flags &= ~PF_GRAVITY;
		Vec3Clear(p->vel);
		Vec3Clear(p->accel);
		return false;
	}

	if (tr.fraction < 1)
	{
		pCalcPartVelocity(p, 0.9f, time, velocity, 1);
		pClipVelocity(velocity, tr.plane.normal, p->vel);

		p->color[3]	= color[3];
		p->size		= *size;
		p->time		= (float)cg.refreshTime;

		Vec3Clear(p->accel);
		Vec3Copy(tr.endPos, p->org);
		Vec3Copy(p->org, org);
		Vec3Copy(p->org, lastOrg);

		if (tr.plane.normal[2] > 0.6f && Vec3LengthFast(p->vel) < 2)
		{
			if (p->flags & PF_GRAVITY)
				p->flags &= ~PF_GRAVITY;
			Vec3Clear(p->vel);
			// more realism; if they're moving they "cool down" faster than when settled
			if (p->colorVel[3] != PART_INSTANT)
				p->colorVel[3] *= 0.5;

			return false;
		}
	}

	length = VectorNormalizeFastf(p->vel);
	if (length > pBounceMaxVelocity)
		Vec3Scale(p->vel, pBounceMaxVelocity, p->vel);
	else
		Vec3Scale(p->vel, length, p->vel);

	nextThinkTime = cg.refreshTime + THINK_DELAY_EXPENSIVE;
	return true;
}


/*
===============
pDropletThink
===============
*/
bool pDropletThink(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time)
{
	if ((int)p->org[2] & 1)
		p->orient -= deltaTime*0.05f * (color[3]*color[3]);
	else
		p->orient += deltaTime*0.05f * (color[3]*color[3]);
	*orient = p->orient;

	nextThinkTime = cg.refreshTime + THINK_DELAY_DEFAULT;
	return true;
}


/*
===============
pExploAnimThink
===============
*/
bool pExploAnimThink(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time)
{
	cgi.R_AddLight(org, 400 * ((color[3] / p->color[3]) + (frand() * 0.01f)), 1, 0.8f, 0.6f);

	if (color[3] > p->color[3]*0.95f)
		p->mat = cgMedia.particleTable[PT_EXPLO1];
	else if (color[3] > p->color[3]*0.9f)
		p->mat = cgMedia.particleTable[PT_EXPLO2];
	else if (color[3] > p->color[3]*0.8f)
		p->mat = cgMedia.particleTable[PT_EXPLO3];
	else if (color[3] > p->color[3]*0.65f)
		p->mat = cgMedia.particleTable[PT_EXPLO4];
	else if (color[3] > p->color[3]*0.3f)
		p->mat = cgMedia.particleTable[PT_EXPLO5];
	else if (color[3] > p->color[3]*0.15f)
		p->mat = cgMedia.particleTable[PT_EXPLO6];
	else
		p->mat = cgMedia.particleTable[PT_EXPLO7];

	// FIXME: Can't set nextTimeThink because it adds a light
	return true;
}


/*
===============
pFireThink
===============
*/
bool pFireThink(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time)
{
	*orient = (frand() * 360) * (color[3] * color[3]);

	nextThinkTime = cg.refreshTime + THINK_DELAY_DEFAULT;
	return true;
}


/*
===============
pFlareThink
===============
*/
bool pFlareThink(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time)
{
	float	dist;

	dist = Vec3DistFast(cg.refDef.viewOrigin, org);
	*orient = dist * 0.2f;

	if (p->flags & PF_SCALED)
		*size = clamp(*size * (dist * 0.001f), *size, *size*10);

	nextThinkTime = cg.refreshTime + THINK_DELAY_DEFAULT;
	return true;
}


/*
===============
pLight70Think
===============
*/
bool pLight70Think(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time)
{
	if (cg_particleShading->intVal)
	{
		// Update lighting
		if (cg.refreshTime >= p->nextLightingTime)
		{
			cgi.R_LightPoint(p->org, p->lighting);

			switch(cg_particleShading->intVal)
			{
			case 1: p->nextLightingTime = cg.refreshTime + 33.0f; // 30 FPS
			case 2: p->nextLightingTime = cg.refreshTime + 16.5f; // 60 FPS
			// Otherwise always update
			}
		}

		// Apply lighting
		float lightest = 0;
		for (int j=0 ; j<3 ; j++)
		{
			color[j] = ((0.7f*p->lighting[j]) + 0.3f) * p->color[j];
			if (color[j] > lightest)
				lightest = color[j];
		}

		// Normalize
		if (lightest > 255.0)
		{
			color[0] *= 255.0f / lightest;
			color[1] *= 255.0f / lightest;
			color[2] *= 255.0f / lightest;
		}

		// FIXME: Always think so that the cached lighting values are applied
		nextThinkTime = cg.refreshTime;// + THINK_DELAY_EXPENSIVE;
	}

	return true;
}


/*
===============
pRailSpiralThink
===============
*/
bool pRailSpiralThink(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time)
{
	p->orient += deltaTime * 0.2f * color[3];
	*orient = p->orient;

	nextThinkTime = cg.refreshTime + THINK_DELAY_DEFAULT;
	return true;
}


/*
===============
pRicochetSparkThink
===============
*/
bool pRicochetSparkThink(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time)
{
	float	length;

	pCalcPartVelocity(p, 6, time, angle, *orient);

	length = VectorNormalizeFastf(angle);
	if (length > *orient)
		length = *orient;
	Vec3Scale(angle, -length, angle);

	nextThinkTime = cg.refreshTime + THINK_DELAY_DEFAULT;
	return true;
}


/*
===============
pFastSmokeThink
===============
*/
bool pFastSmokeThink(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time)
{
	if ((int)p->org[2] & 1)
		p->orient -= deltaTime*0.1f * (1.0f - (color[3]*color[3]));
	else
		p->orient += deltaTime*0.1f * (1.0f - (color[3]*color[3]));
	*orient = p->orient;

	nextThinkTime = cg.refreshTime + THINK_DELAY_DEFAULT;
	return true;
}


/*
===============
pSmokeThink
===============
*/
bool pSmokeThink(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time)
{
	if ((int)p->org[2] & 1)
		p->orient -= deltaTime*0.05f * (1.0f - (color[3]*color[3]));
	else
		p->orient += deltaTime*0.05f * (1.0f - (color[3]*color[3]));
	*orient = p->orient;

	nextThinkTime = cg.refreshTime + THINK_DELAY_DEFAULT;
	return true;
}


/*
===============
pSparkGrowThink
===============
*/
bool pSparkGrowThink(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time)
{
	float length;

	pCalcPartVelocity(p, 6, time, angle, *orient);

	length = VectorNormalizeFastf(angle);
	if (length > *orient)
		length = *orient;
	Vec3Scale(angle, -length, angle);

	nextThinkTime = cg.refreshTime + THINK_DELAY_DEFAULT;
	return true;
}


/*
===============
pSplashThink
===============
*/
bool pSplashThink(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time)
{
	float	length;

	pCalcPartVelocity(p, 0.7f, time, angle, *orient);

	length = VectorNormalizeFastf(angle);
	if (length > *orient)
		length = *orient;
	Vec3Scale(angle, -length, angle);

	nextThinkTime = cg.refreshTime + THINK_DELAY_DEFAULT;
	return true;
}


/*
===============
pWaterOnlyThink
===============
*/
bool pWaterOnlyThink(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time)
{
	// FIXME: Yay hack (I guess the bubble trail is used for other things in these mods)
	if (cg.currGameMod != GAME_MOD_LOX && cg.currGameMod != GAME_MOD_GIEX
	&& !(cgi.CM_PointContents(org, 0) & CONTENTS_MASK_WATER))
	{
		// Kill it
		p->color[3] = 0;
		return false;
	}
	else
	{
		nextThinkTime = cg.refreshTime + THINK_DELAY_EXPENSIVE;
		return true;
	}
}
