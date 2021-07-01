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
// cg_decals.c
//

#include "cg_local.h"

static TLinkedList<cgDecal_t*> decalList;

/*
=============================================================================

	DECAL MANAGEMENT

=============================================================================
*/

static inline void CG_FreeDecal (cgDecal_t *d);
static cgDecal_t *CG_ReplaceDecal (vec3_t origin)
{
	TList<cgDecal_t*> closestDecals;

	for (auto it = decalList.Head(); it != null; it = it->Next)
	{
		var node = it->Value;

		var dist = Vec3DistSquared(origin, node->origin);

		if (dist < 100)
			closestDecals.Add(node);

		if (closestDecals.Count() == 5)
			break;
	}

	if (closestDecals.Count() >= 5)
		CG_FreeDecal (closestDecals[0]);

	return null;
}

/*
===============
CG_AllocDecal
===============
*/
static cgDecal_t *CG_AllocDecal (vec3_t origin)
{
	cgDecal_t *decal = CG_ReplaceDecal(origin);

	if (decal == null)
	{
		// Can we allocate a new one?
		if (decalList.Count() >= (uint32)cg_decalMax->intVal)
			// remove head if not
			cgi.R_FreeDecal(&decalList.PopFront()->refDecal);

		decal = new cgDecal_t;
	}
	
	memset(decal, 0, sizeof(*decal));
	
	decal->node = decalList.AddToBack(decal);

	return decal;
}


/*
===============
CG_FreeDecal
===============
*/
static inline void CG_FreeDecal (cgDecal_t *d)
{
	decalList.RemoveNode(d->node);

	// Free in renderer
	cgi.R_FreeDecal (&d->refDecal);

	delete d;
}


/*
===============
CG_SpawnDecal
===============
*/
cgDecal_t *CG_SpawnDecal(float org0,				float org1,					float org2,
						float dir0,					float dir1,					float dir2,
						float red,					float green,				float blue,
						float redVel,				float greenVel,				float blueVel,
						float alpha,				float alphaVel,
						float size,
						const EDecalType type,		const uint32 flags,
						void (*think)(struct cgDecal_t *d, vec4_t color, int *type, uint32 *flags),
						const bool thinkNext,
						const float lifeTime,		float angle)
{
	vec3_t		origin, dir;
	cgDecal_t	*d;

	// Decal toggling
	if (!cg_decals->intVal)
		return NULL;

	if (flags & DF_USE_BURNLIFE)
	{
		if (!cg_decalBurnLife->floatVal)
			return NULL;
	}
	else if (!cg_decalLife->floatVal)
		return NULL;

	// Copy values
	Vec3Set(dir, dir0, dir1, dir2);
	Vec3Set(origin, org0, org1, org2);

	// Create the decal
	d = CG_AllocDecal(origin);
	if (!cgi.R_CreateDecal(&d->refDecal, cgMedia.decalTable[type%DT_PICTOTAL], cgMedia.decalCoords[type%DT_PICTOTAL], origin, dir, angle, size))
	{
		CG_FreeDecal(d);
		return NULL;
	}

	// Store values
	Vec3Copy (origin, d->origin);
	d->time = (float)cg.refreshTime;
	d->lifeTime = lifeTime;

	Vec4Set(d->color, red, green, blue, alpha);
	Vec4Set(d->colorVel, redVel, greenVel, blueVel, alphaVel);

	d->size = size;

	d->flags = flags;

	d->think = think;
	d->thinkNext = thinkNext;
	return d;
}


/*
===============
CG_ClearDecals
===============
*/
void CG_ClearDecals ()
{
	for (var node = decalList.Head(); node != null; node = node->Next)
		delete node->Value;

	decalList.Clear();
}


/*
===============
CG_AddDecals
===============
*/
void CG_AddDecals ()
{
	float		lifeTime, finalTime;
	float		fade;
	int			i, type;
	uint32		flags;
	vec4_t		color;
	vec3_t		temp;
	colorb		outColor;
	int			num;

	if (!cg_decals->intVal)
		return;

	// Add to list
	num = 0;
	TLinkedList<cgDecal_t*>::Node *next = null;
	for (var d = decalList.Head(); d != null; d = next)
	{
		next = d->Next;
		num++;

		var decal = d->Value;

		if (decal->colorVel[3] > DECAL_INSTANT) {
			// Determine how long this decal shall live for
			if (decal->flags & DF_FIXED_LIFE)
				lifeTime = decal->lifeTime;
			else if (decal->flags & DF_USE_BURNLIFE)
				lifeTime = decal->lifeTime + cg_decalBurnLife->floatVal;
			else
				lifeTime = decal->lifeTime + cg_decalLife->floatVal;

			// Start fading
			finalTime = decal->time + (lifeTime * 1000);
			if ((float)cg.refreshTime > finalTime)  {
				// Finished the life, fade for cg_decalFadeTime
				if (cg_decalFadeTime->floatVal) {
					lifeTime = cg_decalFadeTime->floatVal;

					// final alpha * ((fade time - time since death) / fade time)
					color[3] = decal->colorVel[3] * ((lifeTime - (((float)cg.refreshTime - finalTime) * 0.001f)) / lifeTime);
				}
				else
					color[3] = 0.0f;
			}
			else {
				// Not done living, fade between start/final alpha
				fade = (lifeTime - (((float)cg.refreshTime - decal->time) * 0.001f)) / lifeTime;
				color[3] = (fade * decal->color[3]) + ((1.0f - fade) * decal->colorVel[3]);
			}
		}
		else {
			color[3] = decal->color[3];
		}

		// Faded out
		if (color[3] <= 0.0001f || num > cg_decalMax->intVal) {
			CG_FreeDecal (decal);
			continue;
		}

		if (color[3] > 1.0f)
			color[3] = 1.0f;

		// Small decal lod
		if (cg_decalLOD->intVal && decal->size < 12) {
			Vec3Subtract (cg.refDef.viewOrigin, decal->refDecal.poly.origin, temp);
			if (DotProduct(temp, temp)/15000 > 100*decal->size)
				goto nextDecal;
		}

		// ColorVel calcs
		if (decal->color[3] > DECAL_INSTANT) {
			for (i=0 ; i<3 ; i++) {
				if (decal->color[i] != decal->colorVel[i]) {
					if (decal->color[i] > decal->colorVel[i])
						color[i] = decal->color[i] - ((decal->color[i] - decal->colorVel[i]) * (decal->color[3] - color[3]));
					else
						color[i] = decal->color[i] + ((decal->colorVel[i] - decal->color[i]) * (decal->color[3] - color[3]));
				}
				else {
					color[i] = decal->color[i];
				}

				color[i] = clamp (color[i], 0, 255);
			}
		}
		else {
			Vec3Copy (decal->color, color);
		}

		// Adjust ramp to desired initial and final alpha settings
		color[3] = (color[3] * decal->color[3]) + ((1 - color[3]) * decal->colorVel[3]);

		if (decal->flags & DF_ALPHACOLOR)
			Vec3Scale (color, color[3], color);

		// Think func
		flags = decal->flags;
		if (decal->think && decal->thinkNext) {
			decal->thinkNext = false;
			decal->think (decal, color, &type, &flags);
		}

		if (color[3] <= 0.0f)
			goto nextDecal;

		// Render it
		outColor[0] = color[0];
		outColor[1] = color[1];
		outColor[2] = color[2];
		outColor[3] = color[3] * 255;

		cgi.R_AddDecal(&decal->refDecal, outColor, 0);

nextDecal:
		// Kill if instant
		if (decal->colorVel[3] <= DECAL_INSTANT) {
			decal->color[3] = 0.0;
			decal->colorVel[3] = 0.0;
		}
	}
}
