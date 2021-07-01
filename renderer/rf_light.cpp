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
// rf_light.c
// Dynamic lights
// Lightmaps
// Alias model lighting
//

#include "rf_local.h"

#define Q2LMBLOCKSIZE 512

/*
=============================================================================

	QUAKE II DYNAMIC LIGHTS

=============================================================================
*/

static vec3_t	r_q2_pointColor;
static vec3_t	r_q2_lightSpot;

/*
=============
R_Q2BSP_MarkWorldLights
=============
*/
static void R_Q2BSP_r_MarkWorldLights(mBspNode_t *node, refDLight_t *lt, const dlightBit_t bit)
{
loc0:
	if (node->q2_contents != -1)
		return;
	if (node->visFrame != ri.scn.visFrameCount)
		return;

	const float dist = PlaneDiff(lt->origin, node->plane);
	if (dist > lt->intensity)
	{
		node = node->children[0];
		goto loc0;
	}
	if (dist < -lt->intensity)
	{
		node = node->children[1];
		goto loc0;
	}
	if (!BoundsIntersect(node->mins, node->maxs, lt->mins, lt->maxs))
		return;

	// Mark the polygons
	if (node->q2_firstLitSurface)
	{
		mBspSurface_t **mark = node->q2_firstLitSurface;
		do
		{
			mBspSurface_t *surf = *mark++;
			if (!BoundsIntersect(surf->mins, surf->maxs, lt->mins, lt->maxs))
				continue;

			if (surf->dLightFrame != ri.frameCount)
			{
				surf->dLightFrame = ri.frameCount;
				surf->dLightBits = 0;
			}
			surf->dLightBits |= bit;
		} while (*mark);
	}

	R_Q2BSP_r_MarkWorldLights(node->children[0], lt, bit);
	R_Q2BSP_r_MarkWorldLights(node->children[1], lt, bit);
}
void R_Q2BSP_MarkWorldLights()
{
	if (gl_flashblend->intVal)
		return;

	for (uint32 i=0 ; i<ri.scn.numDLights ; i++)
	{
		refDLight_t *lt = &ri.scn.dLightList[i];
		if (!(ri.scn.dLightCullBits & BIT(i)))
			R_Q2BSP_r_MarkWorldLights (ri.scn.worldModel->BSPData()->nodes, lt, BIT(i));
	}
}

/*
=============================================================================

	QUAKE II LIGHT SAMPLING

=============================================================================
*/

/*
===============
R_Q2BSP_RecursiveLightPoint
===============
*/
static int Q2BSP_RecursiveLightPoint(mBspNode_t *node, vec3_t start, vec3_t end)
{
	float			front, back, frac;
	int				s, t, ds, dt, r;
	int				side, map;
	plane_t			*plane;
	vec3_t			mid;
	mBspSurface_t	**mark, *surf;
	byte			*lightmap;

	// Didn't hit anything
	if (node->q2_contents != -1)
		return -1;
	
	// Calculate mid point
	plane = node->plane;
	if (plane->type < 3)
	{
		front = start[plane->type] - plane->dist;
		back = end[plane->type] - plane->dist;
	}
	else
	{
		front = DotProduct (start, plane->normal) - plane->dist;
		back = DotProduct (end, plane->normal) - plane->dist;
	}

	side = (front < 0) ? 1 : 0;
	if ((back < 0) == side)
		return Q2BSP_RecursiveLightPoint (node->children[side], start, end);
	
	frac = front / (front - back);
	mid[0] = start[0] + (end[0] - start[0]) * frac;
	mid[1] = start[1] + (end[1] - start[1]) * frac;
	mid[2] = start[2] + (end[2] - start[2]) * frac;
	
	// Go down front side
	r = Q2BSP_RecursiveLightPoint (node->children[side], start, mid);
	if (r >= 0)
		return r;	// Hit something
		
	if ((back < 0) == side)
		return -1;	// Didn't hit anything
		
	// Check for impact on this node
	Vec3Copy (mid, r_q2_lightSpot);

	if (node->q2_firstLitSurface)
	{
		mark = node->q2_firstLitSurface;
		do
		{
			surf = *mark++;

			s = Q_ftol (DotProduct (mid, surf->q2_texInfo->vecs[0]) + surf->q2_texInfo->vecs[0][3]);
			if (s < surf->q2_textureMins[0])
				continue;
			ds = s - surf->q2_textureMins[0];
			if (ds > surf->q2_extents[0])
				continue;

			t = Q_ftol (DotProduct (mid, surf->q2_texInfo->vecs[1]) + surf->q2_texInfo->vecs[1][3]);
			if (t < surf->q2_textureMins[1])
				continue;
			dt = t - surf->q2_textureMins[1];
			if (dt > surf->q2_extents[1])
				continue;

			ds >>= 4;
			dt >>= 4;

			lightmap = surf->q2_lmSamples;
			Vec3Clear (r_q2_pointColor);
			if (lightmap)
			{
				vec3_t scale;

				lightmap += 3 * (dt*surf->q2_lmWidth + ds);

				for (map=0 ; map<surf->q2_numStyles ; map++)
				{
					Vec3Scale (ri.scn.lightStyles[surf->q2_styles[map]].rgb, gl_modulate->floatVal, scale);

					r_q2_pointColor[0] += lightmap[0] * scale[0] * (1.0f/255.0f);
					r_q2_pointColor[1] += lightmap[1] * scale[1] * (1.0f/255.0f);
					r_q2_pointColor[2] += lightmap[2] * scale[2] * (1.0f/255.0f);

					lightmap += 3*surf->q2_lmWidth*surf->q2_lmWidth;
				}
			}
			
			return 1;
		} while (*mark);
	}

	// Go down back side
	return Q2BSP_RecursiveLightPoint (node->children[!side], mid, end);
}
static bool R_Q2BSP_RecursiveLightPoint(vec3_t point, vec3_t end)
{
	int	r;

	if (ri.def.rdFlags & RDF_NOWORLDMODEL || !ri.scn.worldModel->Q2BSPData()->lightData)
	{
		Vec3Set(r_q2_pointColor, 1, 1, 1);
		return false;
	}

	r = Q2BSP_RecursiveLightPoint(ri.scn.worldModel->BSPData()->nodes, point, end);
	
	if (r == -1)
	{
		Vec3Clear(r_q2_pointColor);
		return false;
	}

	if (!r_coloredLighting->intVal)
	{
		float grey = (r_q2_pointColor[0] * 0.3f) + (r_q2_pointColor[1] * 0.59f) + (r_q2_pointColor[2] * 0.11f);
		Vec3Set(r_q2_pointColor, grey, grey, grey);
	}

	return true;
}


/*
===============
R_Q2BSP_LightForEntity
===============
*/
static void R_Q2BSP_LightForEntity(refEntity_t *ent, vec3_t *vertexArray, const int numVerts, vec3_t *normalArray, byte *bArray)
{
	static vec3_t tempColorsArray[RB_MAX_VERTS];

	// Fullbright entity
	if (r_fullbright->intVal || ent->flags & RF_FULLBRIGHT || ri.def.rdFlags & RDF_NOWORLDMODEL)
	{
		for (int i=0 ; i<numVerts ; i++, bArray+=4)
			*(colorb *)bArray = ent->color;
		return;
	}

	//
	// Get the lighting from below
	//
	vec3_t end;
	vec3_t ambientLight;
	vec3_t directedLight;
	Vec3Set(end, ent->origin[0], ent->origin[1], ent->origin[2] - 2048);
	if (!R_Q2BSP_RecursiveLightPoint(ent->origin, end))
	{
		end[2] = ent->origin[2] + 16;
		if (!(ent->flags & RF_WEAPONMODEL) && !R_Q2BSP_RecursiveLightPoint(ent->origin, end))
		{
			// Not found!
			Vec3Copy(r_q2_pointColor, ambientLight);
			Vec3Copy(r_q2_pointColor, directedLight);
		}
		else
		{
			// Found!
			Vec3Copy(r_q2_pointColor, directedLight);
			Vec3Scale(r_q2_pointColor, 0.6f, ambientLight);
		}
	}
	else
	{
		// Found!
		Vec3Copy(r_q2_pointColor, directedLight);
		Vec3Scale(r_q2_pointColor, 0.6f, ambientLight);
	}

	// Save off light value for server to look at (BIG HACK!)
	if (ent->flags & RF_WEAPONMODEL)
	{
		// Pick the greatest component, which should be
		// the same as the mono value returned by software
		Cvar_VariableSetValue(r_lightlevel, 150 * Max(r_q2_pointColor[0], Max(r_q2_pointColor[1], r_q2_pointColor[2])), true);
	}

	//
	// Flag effects
	//
	if (ent->flags & RF_MINLIGHT)
	{
		int i;
		for (i=0 ; i<3 ; i++)
			if (ambientLight[i] > 0.1f)
				break;

		if (i == 3)
		{
			ambientLight[0] += 0.1f;
			ambientLight[1] += 0.1f;
			ambientLight[2] += 0.1f;
		}
	}

	if (ent->flags & RF_GLOW)
	{
		// Bonus items will pulse with time
		float scale = 0.1f * RB_FastSin(ri.def.time);
		for (int i=0 ; i<3 ; i++)
		{
			float min = ambientLight[i] * 0.8f;
			ambientLight[i] += scale;
			if (ambientLight[i] < min)
				ambientLight[i] = min;
		}
	}

	//
	// Add ambient lights
	//
	vec3_t dir, direction;
	Vec3Set(dir, -1, 0, 1);
	Matrix3_TransformVector(ent->axis, dir, direction);

	for (int i=0 ; i<numVerts; i++)
	{
		float dot = DotProduct(normalArray[i], direction);
		if (dot <= 0)
			Vec3Copy(ambientLight, tempColorsArray[i]);
		else
			Vec3MA(ambientLight, dot, directedLight, tempColorsArray[i]);
	}

	//
	// Add dynamic lights
	//
	if (gl_dynamic->intVal && ri.scn.numDLights)
	{
		for (uint32 num=0 ; num<ri.scn.numDLights ; num++)
		{
			if (ri.scn.dLightCullBits & BIT(num))
				continue;

			refDLight_t *lt = &ri.scn.dLightList[num];
			if (!BoundsAndSphereIntersect(lt->mins, lt->maxs, ent->origin, ent->model->radius * ent->scale))
				continue;

			// Translate
			Vec3Subtract(lt->origin, ent->origin, dir);
			float dist = Vec3LengthFast(dir);

			if (!dist || dist > lt->intensity + ent->model->radius * ent->scale)
				continue;

			// Rotate
			vec3_t dlOrigin;
			Matrix3_TransformVector(ent->axis, dir, dlOrigin);

			// Calculate intensity
			float intensity = lt->intensity - dist;
			if (intensity <= 0)
				continue;
			float intensity8 = lt->intensity * 8;

			// Ambience
			float ambience = (1.0f - (dist / lt->intensity)) * 0.5f;
			if (ambience > 0.5f)
				ambience = 0.5f;

			for (int i=0 ; i<numVerts ; i++)
			{
				Vec3Subtract(dlOrigin, vertexArray[i], dir);
				float add = DotProduct(normalArray[i], dir);

				// Add some ambience
				Vec3MA(tempColorsArray[i], ambience, lt->color, tempColorsArray[i]);

				// Shade the verts
				if (add > 0)
				{
					float dot = DotProduct(dir, dir);
					add *= (intensity8 / dot) * Q_RSqrtf(dot);
					if (add > 255.0f)
						add = 255.0f / add;

					Vec3MA(tempColorsArray[i], add, lt->color, tempColorsArray[i]);
				}
			}
		}
	}

	//
	// Clamp
	//
	float *cArray = tempColorsArray[0];
	for (int i=0 ; i<numVerts ; i++, bArray+=4, cArray+=3)
	{
		int r = Q_ftol(cArray[0] * ent->color[0]);
		int g = Q_ftol(cArray[1] * ent->color[1]);
		int b = Q_ftol(cArray[2] * ent->color[2]);

		bArray[0] = clamp (r, 0, 255);
		bArray[1] = clamp (g, 0, 255);
		bArray[2] = clamp (b, 0, 255);
	}
}


/*
===============
R_Q2BSP_LightPoint
===============
*/
static void R_Q2BSP_LightPoint(vec3_t point, vec3_t light)
{
	vec3_t		end;
	vec3_t		dist;
	float		add;

	Vec3Set (end, point[0], point[1], point[2] - 2048);
	if (!R_Q2BSP_RecursiveLightPoint (point, end))
	{
		end[2] = point[2] + 16;
		R_Q2BSP_RecursiveLightPoint (point, end);
	}
	Vec3Copy (r_q2_pointColor, light);

	//
	// Add dynamic lights
	//
	for (uint32 num=0 ; num<ri.scn.numDLights ; num++)
	{
		refDLight_t *lt = &ri.scn.dLightList[num];

		Vec3Subtract(point, lt->origin, dist);
		add = (lt->intensity - Vec3Length (dist)) * (1.0f/256.0f);

		if (add > 0)
			Vec3MA(light, add, lt->color, light);
	}
}


/*
====================
R_Q2BSP_SetLightLevel

Save off light value for server to look at (BIG HACK!)
====================
*/
static void R_Q2BSP_SetLightLevel()
{
	vec3_t shadelight;
	R_Q2BSP_LightPoint(ri.def.viewOrigin, shadelight);

	// Pick the greatest component, which should be
	// the same as the mono value returned by software
	Cvar_VariableSetValue(r_lightlevel, 150 * Max(shadelight[0], Max(shadelight[1], shadelight[2])), true);
}

/*
=============================================================================

	QUAKE II LIGHTMAP

=============================================================================
*/

static byte		*r_q2_lmBuffer;
static bool		r_q2_lmBufferDirty;
static int		r_q2_lmNumUploaded;
static int		*r_q2_lmAllocated;
int				r_q2_lmBlockSize;

/*
===============
R_Q2BSP_AddDynamicLights
===============
*/
static void R_Q2BSP_AddDynamicLights(refEntity_t *ent, mBspSurface_t *surf, float *blockLights)
{
	for (uint32 num=0 ; num<ri.scn.numDLights ; num++)
	{
		if (ri.scn.dLightCullBits & BIT(num))
			continue;

		refDLight_t *lt = &ri.scn.dLightList[num];
		if (!(surf->dLightBits & BIT(num)))
			continue;	// Not lit by this light

		vec3_t origin;
		if (!Matrix3_Compare(ent->axis, axisIdentity))
		{
			vec3_t tmp;
			Vec3Subtract(lt->origin, ent->origin, tmp);
			Matrix3_TransformVector(ent->axis, tmp, origin);
		}
		else
		{
			Vec3Subtract(lt->origin, ent->origin, origin);
		}

		float fDist = PlaneDiff(origin, surf->q2_plane);
		const float fRad = lt->intensity - fabsf(fDist); // fRad is now the highest intensity on the plane
		if (fRad < 0)
			continue;

		vec3_t impact;
		impact[0] = origin[0] - (surf->q2_plane->normal[0] * fDist);
		impact[1] = origin[1] - (surf->q2_plane->normal[1] * fDist);
		impact[2] = origin[2] - (surf->q2_plane->normal[2] * fDist);

		float sl = DotProduct(impact, surf->q2_texInfo->vecs[0]) + surf->q2_texInfo->vecs[0][3] - surf->q2_textureMins[0];
		float st = DotProduct(impact, surf->q2_texInfo->vecs[1]) + surf->q2_texInfo->vecs[1][3] - surf->q2_textureMins[1];

		float *bl = blockLights;
		float ftacc = 0.0f;
		for (int t=0 ; t<surf->q2_lmHeight ; t++)
		{
			float td = st - ftacc;
			if (td < 0)
				td = -td;

			float fsacc = 0.0f;
			for (int s=0 ; s<surf->q2_lmWidth ; s++)
			{
				float sd = sl - fsacc;
				if (sd < 0)
					sd = -sd;

				float fDist2;
				if (sd > td)
				{
					fDist = sd + (td * 0.5f);
					fDist2 = sd + (td * 2.0f);
				}
				else
				{
					fDist = td + (sd * 0.5f);
					fDist2 = td + (sd * 2.0f);
				}

				if (fDist < fRad)
				{
					float scale = fRad - fDist;

					bl[0] += lt->color[0] * scale;
					bl[1] += lt->color[1] * scale;
					bl[2] += lt->color[2] * scale;

					// Amplify the center a little
					if (fDist2 < fRad)
					{
						scale = fRad - fDist2;
						bl[0] += lt->color[0] * scale * 0.5f;
						bl[1] += lt->color[1] * scale * 0.5f;
						bl[2] += lt->color[2] * scale * 0.5f;
					}
				}

				fsacc += 16;
				bl += 3;
			}

			ftacc += 16;
		}
	}
}


/*
===============
R_Q2BSP_BuildLightMap

Combine and scale multiple lightmaps into the floating format in blocklights
===============
*/
static void R_Q2BSP_BuildLightMap(refEntity_t *ent, mBspSurface_t *surf, byte *dest, int stride)
{
	if (surf->q2_texInfo->flags & (SURF_TEXINFO_SKY|SURF_TEXINFO_WARP))
		Com_Error(ERR_DROP, "LM_BuildLightMap called for non-lit surface");

	float blockLights[34*34*3];
	const int size = surf->q2_lmWidth*surf->q2_lmHeight;
	if (size > sizeof(blockLights) >> 4)
		Com_Error(ERR_DROP, "Bad blockLights size");

	// Set to full bright if no light data
	if (!surf->q2_lmSamples || r_fullbright->intVal)
	{
		for (int i=0 ; i<size*3 ; i++)
			blockLights[i] = 255.0f;
	}
	else
	{
		byte *lightMap = surf->q2_lmSamples;

		// Add all the lightmaps
		float *bl = blockLights;
		vec3_t scale;
		Vec3Scale(ri.scn.lightStyles[surf->q2_styles[0]].rgb, gl_modulate->floatVal, scale);
		for (int i=0 ; i<size ; i++, bl+=3)
		{
			bl[0] = lightMap[i*3+0] * scale[0];
			bl[1] = lightMap[i*3+1] * scale[1];
			bl[2] = lightMap[i*3+2] * scale[2];
		}

		// Skip to next lightmap
		lightMap += size*3;

		if (surf->q2_numStyles > 1)
		{
			for (int map=1 ; map<surf->q2_numStyles ; map++)
			{
				bl = blockLights;

				Vec3Scale(ri.scn.lightStyles[surf->q2_styles[map]].rgb, gl_modulate->floatVal, scale);
				for (int i=0 ; i<size ; i++, bl+=3)
				{
					bl[0] += lightMap[i*3+0] * scale[0];
					bl[1] += lightMap[i*3+1] * scale[1];
					bl[2] += lightMap[i*3+2] * scale[2];
				}

				// Skip to next lightmap
				lightMap += size*3;
			}
		}

		// Add all the dynamic lights
		if (surf->dLightFrame == ri.frameCount)
			R_Q2BSP_AddDynamicLights(ent, surf, blockLights);
	}

	// Put into texture format
	stride -= (surf->q2_lmWidth << 2);
	float *bl = blockLights;

	for (int i=0 ; i<surf->q2_lmHeight ; i++)
	{
		for (int j=0 ; j<surf->q2_lmWidth ; j++)
		{
			// Temp storage
			float r = bl[0];
			float g = bl[1];
			float b = bl[2];

			// Catch negative lights
			if (r < 0)
				r = 0;
			if (g < 0)
				g = 0;
			if (b < 0)
				b = 0;

			// Determine the brightest of the three color components
			// Normalize the color components to the highest channel
			float max = Max(r, Max(g, b));
			if (max > 255.0f)
			{
				max = 255.0f / max;
				r *= max;
				g *= max;
				b *= max;
			}

			if (!r_coloredLighting->intVal)
			{
				byte grey = (byte)((r * 0.3f) + (g * 0.59f) + (b * 0.11f));
				dest[0] = grey;
				dest[1] = grey;
				dest[2] = grey;
			}
			else
			{
				dest[0] = (byte)r;
				dest[1] = (byte)g;
				dest[2] = (byte)b;
			}
			dest[3] = 255;

			bl += 3;
			dest += 4;
		}

		dest += stride;
	}
}


/*
===============
R_Q2BSP_SetLMCacheState
===============
*/
static void R_Q2BSP_SetLMCacheState (mBspSurface_t *surf)
{
	for (int map=0 ; map<surf->q2_numStyles ; map++)
		surf->q2_cachedLight[map] = ri.scn.lightStyles[surf->q2_styles[map]].white;
}


/*
=======================
R_Q2BSP_UpdateLightmap
=======================
*/
void R_Q2BSP_UpdateLightmap(refEntity_t *ent, mBspSurface_t *surf)
{
	// Don't update twice a frame
	if (surf->q2_dLightUpdateFrame == ri.frameCount)
		return;
	surf->q2_dLightUpdateFrame = ri.frameCount;

	// Is this surface allowed to have a lightmap?
	if (surf->q2_texInfo->flags & (SURF_TEXINFO_SKY|SURF_TEXINFO_WARP))
		return;
	if (surf->lmTexNum == BAD_LMTEXNUM)
		return;

	bool bDynamic = false;
	bool bUpdateCache = false;

	// Dynamic this frame or dynamic previously
	if (gl_dynamic->intVal)
	{
		if (surf->dLightFrame == ri.frameCount)
		{
			surf->q2_bDLightDirty = true;
			bDynamic = true;
		}
		else
		{
			for (int map=0 ; map<surf->q2_numStyles ; map++)
			{
				if (ri.scn.lightStyles[surf->q2_styles[map]].white != surf->q2_cachedLight[map])
				{
					bUpdateCache = (surf->q2_styles[map] >= 32 || surf->q2_styles[map] == 0);
					surf->q2_bDLightDirty = true;
					bDynamic = true;
					break;
				}
			}
		}
	}

	// If it's dirty, give it another update so that we can "clean off" old dynamic lights
	if (!bDynamic && surf->q2_bDLightDirty)
	{
		surf->q2_bDLightDirty = false;
		bDynamic = true;
	}

	if (bDynamic)
	{
		static uint32 scratch[Q2LMBLOCKSIZE*Q2LMBLOCKSIZE];

		// Update texture
		R_Q2BSP_BuildLightMap(ent, surf, (byte *)scratch, surf->q2_lmWidth*4);
		if (bUpdateCache)
		{
			R_Q2BSP_SetLMCacheState(surf);
		}

		RB_BindTexture(ri.media.lmTextures[surf->lmTexNum]);

		glTexSubImage2D(GL_TEXTURE_2D, 0,
						surf->q2_lmCoords[0], surf->q2_lmCoords[1],
						surf->q2_lmWidth, surf->q2_lmHeight,
						GL_RGBA,
						GL_UNSIGNED_BYTE,
						scratch);
	}

	RB_CheckForError("R_Q2BSP_UpdateLightmap");
}


/*
================
R_Q2BSP_UploadLMBlock
================
*/
static void R_Q2BSP_UploadLMBlock()
{
	if (r_q2_lmNumUploaded+1 >= MAX_LIGHTMAP_IMAGES)
		Com_Error (ERR_DROP, "R_Q2BSP_UploadLMBlock: - MAX_LIGHTMAP_IMAGES exceeded\n");

	if (!r_q2_lmBufferDirty)
	{
		Com_Printf(PRNT_WARNING, "R_Q2BSP_UploadLMBlock() called when buffer not dirty, ignoring!\n");
		return;
	}

	ri.media.lmTextures[r_q2_lmNumUploaded++] = R_Create2DImage(Q_VarArgs ("*lm%i", r_q2_lmNumUploaded), (byte **)(&r_q2_lmBuffer),
		r_q2_lmBlockSize, r_q2_lmBlockSize, IF_NOPICMIP|IF_NOMIPMAP_LINEAR|IF_NOGAMMA|IF_NOINTENS|IF_NOCOMPRESS|IT_LIGHTMAP, 3);

	r_q2_lmBufferDirty = false;
}


/*
================
R_Q2BSP_AllocLMBlock

Returns a texture number and the position inside it
================
*/
static bool R_Q2BSP_AllocLMBlock(const int Width, const int Height, int *x, int *y)
{
	int		i, j;
	int		best, best2;

	best = r_q2_lmBlockSize;
	for (i=0 ; i<r_q2_lmBlockSize-Width ; i++)
	{
		best2 = 0;

		for (j=0 ; j<Width ; j++)
		{
			if (r_q2_lmAllocated[i+j] >= best)
				break;

			if (r_q2_lmAllocated[i+j] > best2)
				best2 = r_q2_lmAllocated[i+j];
		}

		if (j == Width)
		{
			// This is a valid spot
			*x = i;
			*y = best = best2;
		}
	}

	if (best+Height > r_q2_lmBlockSize)
		return false;

	for (i=0 ; i<Width ; i++)
		r_q2_lmAllocated[*x + i] = best + Height;

	return true;
}


/*
==================
R_Q2BSP_BeginBuildingLightmaps
==================
*/
void R_Q2BSP_BeginBuildingLightmaps()
{
	// Should be no lightmaps at this point
	r_q2_lmNumUploaded = 0;

	// Find the maximum size
	int size;
	for (size=1 ; size<Q2LMBLOCKSIZE && size<ri.config.maxTexSize ; size<<=1);
	r_q2_lmBlockSize = size;

	// Allocate buffers and clear values
	r_q2_lmAllocated = (int*)Mem_PoolAlloc(sizeof(int) * r_q2_lmBlockSize, ri.lightSysPool, 0);
	r_q2_lmBuffer = (byte*)Mem_PoolAlloc(r_q2_lmBlockSize*r_q2_lmBlockSize*4, ri.lightSysPool, 0);
	memset (r_q2_lmBuffer, 255, r_q2_lmBlockSize*r_q2_lmBlockSize*4);

	// Setup the base light styles
	for (int i=0 ; i<MAX_CS_LIGHTSTYLES ; i++)
	{
		Vec3Set (ri.scn.lightStyles[i].rgb, 1, 1, 1);
		ri.scn.lightStyles[i].white = 3;
	}
}


/*
========================
R_Q2BSP_CreateSurfaceLightmap
========================
*/
void R_Q2BSP_CreateSurfaceLightmap(mBspSurface_t *surf)
{
	byte *base;

	if (surf->lmTexNum != BAD_LMTEXNUM)
		return;

	if (!R_Q2BSP_AllocLMBlock(surf->q2_lmWidth, surf->q2_lmHeight, &surf->q2_lmCoords[0], &surf->q2_lmCoords[1]))
	{
		R_Q2BSP_UploadLMBlock();
		memset (r_q2_lmAllocated, 0, sizeof(int) * r_q2_lmBlockSize);

		if (!R_Q2BSP_AllocLMBlock (surf->q2_lmWidth, surf->q2_lmHeight, &surf->q2_lmCoords[0], &surf->q2_lmCoords[1]))
			Com_Error (ERR_FATAL, "Consecutive calls to R_Q2BSP_AllocLMBlock (%d, %d) failed\n", surf->q2_lmWidth, surf->q2_lmHeight);
	}

	surf->lmTexNum = r_q2_lmNumUploaded;

	base = r_q2_lmBuffer + ((surf->q2_lmCoords[1] * r_q2_lmBlockSize + surf->q2_lmCoords[0]) * 4);

	R_Q2BSP_SetLMCacheState(surf);
	R_Q2BSP_BuildLightMap(ri.scn.defaultEntity, surf, base, r_q2_lmBlockSize*4);

	r_q2_lmBufferDirty = true;
}


/*
=======================
R_Q2BSP_EndBuildingLightmaps
=======================
*/
void R_Q2BSP_EndBuildingLightmaps()
{
	// Upload the final block
	if (r_q2_lmBufferDirty)
		R_Q2BSP_UploadLMBlock();

	// Release allocated memory
	Mem_Free(r_q2_lmAllocated);
	Mem_Free(r_q2_lmBuffer);
}


/*
=======================
R_Q2BSP_TouchLightmaps
=======================
*/
static void R_Q2BSP_TouchLightmaps()
{
	for (int i=0 ; i<r_q2_lmNumUploaded ; i++)
		R_TouchImage(ri.media.lmTextures[i]);
}

/*
=============================================================================

	QUAKE III DYNAMIC LIGHTS

=============================================================================
*/

#define Q3_DLIGHT_SCALE 0.5f

/*
=================
R_Q3BSP_MarkLitSurfaces
=================
*/
static void R_Q3BSP_MarkLitSurfaces(refDLight_t *lt, const float lightIntensity, const dlightBit_t bit, mBspNode_t *node)
{
	for ( ; ; )
	{
		if (node->visFrame != ri.scn.visFrameCount)
			return;
		if (node->plane == NULL)
			break;

		const float dist = PlaneDiff(lt->origin, node->plane);
		if (dist > lightIntensity)
		{
			node = node->children[0];
			continue;
		}

		if (dist >= -lightIntensity)
			R_Q3BSP_MarkLitSurfaces(lt, lightIntensity, bit, node->children[0]);
		node = node->children[1];
	}

	mBspLeaf_t *leaf = (mBspLeaf_t *)node;

	// Check for door connected areas
	if (ri.def.areaBits)
	{
		if (!(ri.def.areaBits[leaf->area>>3] & BIT(leaf->area&7)))
			return;		// Not visible
	}
	if (!leaf->q3_firstLitSurface)
		return;
	if (!BoundsIntersect(leaf->mins, leaf->maxs, lt->mins, lt->maxs))
		return;

	mBspSurface_t **mark = leaf->q3_firstLitSurface;
	do
	{
		mBspSurface_t *surf = *mark++;
		if (!BoundsIntersect(surf->mins, surf->maxs, lt->mins, lt->maxs))
			continue;

		if (surf->dLightFrame != ri.frameCount)
		{
			surf->dLightBits = 0;
			surf->dLightFrame = ri.frameCount;
		}
		surf->dLightBits |= bit;
	} while (*mark);
}


/*
=================
R_Q3BSP_MarkWorldLights
=================
*/
void R_Q3BSP_MarkWorldLights()
{
	if (gl_flashblend->intVal || !gl_dynamic->intVal || !ri.scn.numDLights || r_vertexLighting->intVal || r_fullbright->intVal)
		return;

	for (uint32 i=0 ; i<ri.scn.numDLights ; i++)
	{
		refDLight_t *lt = &ri.scn.dLightList[i];
		if (!(ri.scn.dLightCullBits & BIT(i)))
			R_Q3BSP_MarkLitSurfaces(lt, lt->intensity*Q3_DLIGHT_SCALE, BIT(i), ri.scn.worldModel->BSPData()->nodes);
	}
}

/*
=============================================================================

	QUAKE III LIGHT SAMPLING

=============================================================================
*/

/*
===============
R_Q3BSP_LightForEntity
===============
*/
static void R_Q3BSP_LightForEntity(refEntity_t *ent, vec3_t *vertexArray, const int numVerts, vec3_t *normalArray, byte *bArray)
{
	static vec3_t	tempColorsArray[RB_MAX_VERTS];
	vec3_t			vf, vf2;
	float			*cArray;
	float			t[8], direction_uv[2], dot;
	int				r, g, b, vi[3], i, j, index[4];
	vec3_t			dlorigin, ambient, diffuse, dir, direction;
	float			*gridSize, *gridMins;
	int				*gridBounds;
	mQ3BspModel_t	*q3BspModel;

	// Fullbright entity
	if (r_fullbright->intVal || ent->flags & RF_FULLBRIGHT)
	{
		for (i=0 ; i<numVerts ; i++, bArray+=4)
			*(colorb *)bArray = ent->color;
		return;
	}

	// Probably a weird material, see mpteam4 for example
	if (!ent->model || ent->model->type == MODEL_Q3BSP)
	{
		memset (bArray, 0, sizeof(colorb)*numVerts);
		return;
	}

	Vec3Set (ambient, 0, 0, 0);
	Vec3Set (diffuse, 0, 0, 0);
	Vec3Set (direction, 1, 1, 1);

	q3BspModel = ri.scn.worldModel->Q3BSPData();

	gridSize = q3BspModel->gridSize;
	gridMins = q3BspModel->gridMins;
	gridBounds = q3BspModel->gridBounds;

	if (!q3BspModel->lightGrid || !q3BspModel->numLightGridElems)
		goto dynamic;

	for (i=0 ; i<3 ; i++)
	{
		vf[i] = (ent->origin[i] - q3BspModel->gridMins[i]) / q3BspModel->gridSize[i];
		vi[i] = (int)vf[i];
		vf[i] = vf[i] - floor(vf[i]);
		vf2[i] = 1.0f - vf[i];
	}

	index[0] = vi[2]*q3BspModel->gridBounds[3] + vi[1]*q3BspModel->gridBounds[0] + vi[0];
	index[1] = index[0] + q3BspModel->gridBounds[0];
	index[2] = index[0] + q3BspModel->gridBounds[3];
	index[3] = index[2] + q3BspModel->gridBounds[0];
	for (i=0 ; i<4 ; i++)
	{
		if (index[i] < 0 || index[i] >= q3BspModel->numLightGridElems-1)
			goto dynamic;
	}

	t[0] = vf2[0] * vf2[1] * vf2[2];
	t[1] = vf[0] * vf2[1] * vf2[2];
	t[2] = vf2[0] * vf[1] * vf2[2];
	t[3] = vf[0] * vf[1] * vf2[2];
	t[4] = vf2[0] * vf2[1] * vf[2];
	t[5] = vf[0] * vf2[1] * vf[2];
	t[6] = vf2[0] * vf[1] * vf[2];
	t[7] = vf[0] * vf[1] * vf[2];

	for (j=0 ; j<3 ; j++)
	{
		ambient[j] = 0;
		diffuse[j] = 0;

		for (i=0 ; i<4 ; i++)
		{
			ambient[j] += t[i*2] * q3BspModel->lightGrid[index[i]].ambient[j];
			ambient[j] += t[i*2+1] * q3BspModel->lightGrid[index[i]+1].ambient[j];

			diffuse[j] += t[i*2] * q3BspModel->lightGrid[index[i]].diffuse[j];
			diffuse[j] += t[i*2+1] * q3BspModel->lightGrid[index[i]+1].diffuse[j];
		}
	}

	for (j=0 ; j<2 ; j++)
	{
		direction_uv[j] = 0;

		for (i=0 ; i<4 ; i++)
		{
			direction_uv[j] += t[i*2] * q3BspModel->lightGrid[index[i]].direction[j];
			direction_uv[j] += t[i*2+1] * q3BspModel->lightGrid[index[i]+1].direction[j];
		}

		direction_uv[j] = AngleModf (direction_uv[j]);
	}

	dot = bound(0.0f, /*r_ambientscale->floatVal*/ 1.0f, 1.0f) * ri.pow2MapOvrbr;
	Vec3Scale (ambient, dot, ambient);

	dot = bound(0.0f, /*r_directedscale->floatVal*/ 1.0f, 1.0f) * ri.pow2MapOvrbr;
	Vec3Scale (diffuse, dot, diffuse);

	if (ent->flags & RF_MINLIGHT)
	{
		for (i=0 ; i<3 ; i++)
			if (ambient[i] > 0.1)
				break;

		if (i == 3)
		{
			ambient[0] = 0.1f;
			ambient[1] = 0.1f;
			ambient[2] = 0.1f;
		}
	}

	if (ent->flags & RF_GLOW)
	{
		float	scale;
		float	min;

		// Bonus items will pulse with time
		scale = 0.1f * RB_FastSin(ri.def.time);
		for (i=0 ; i<3 ; i++)
		{
			min = ambient[i] * 0.8f;
			ambient[i] += scale;
			if (ambient[i] < min)
				ambient[i] = min;
		}
	}

	dot = direction_uv[0] * (1.0 / 255.0);
	t[0] = RB_FastSin (dot + 0.25f);
	t[1] = RB_FastSin (dot);

	dot = direction_uv[1] * (1.0 / 255.0);
	t[2] = RB_FastSin (dot + 0.25f);
	t[3] = RB_FastSin (dot);

	Vec3Set (dir, t[2] * t[1], t[3] * t[1], t[0]);

	// Rotate direction
	Matrix3_TransformVector (ent->axis, dir, direction);

	cArray = tempColorsArray[0];
	for (i=0 ; i<numVerts ; i++, cArray+=3)
	{
		dot = DotProduct (normalArray[i], direction);
		
		if (dot <= 0)
			Vec3Copy (ambient, cArray);
		else
			Vec3MA (ambient, dot, diffuse, cArray);
	}

dynamic:
	//
	// Add dynamic lights
	//
	if (gl_dynamic->intVal && ri.scn.numDLights)
	{
		refDLight_t	*dl;
		float		dist, add, intensity8;
		uint32		num;

		for (num=0, dl=ri.scn.dLightList ; num<ri.scn.numDLights ; dl++, num++)
		{
			if (ri.scn.dLightCullBits & BIT(num))
				continue;
			if (!BoundsAndSphereIntersect (dl->mins, dl->maxs, ent->origin, ent->model->radius * ent->scale))
				continue;

			// Translate
			Vec3Subtract ( dl->origin, ent->origin, dir );
			dist = Vec3Length ( dir );

			if (dist > dl->intensity + ent->model->radius * ent->scale)
				continue;

			// Rotate
			Matrix3_TransformVector ( ent->axis, dir, dlorigin );

			intensity8 = dl->intensity * 8;

			cArray = tempColorsArray[0];
			for (i=0 ; i<numVerts ; i++, cArray+=3)
			{
				Vec3Subtract (dlorigin, vertexArray[i], dir);
				add = DotProduct (normalArray[i], dir);

				if (add > 0)
				{
					dot = DotProduct (dir, dir);
					add *= (intensity8 / dot) * Q_RSqrtf (dot);
					Vec3MA (cArray, add, dl->color, cArray);
				}
			}
		}
	}

	cArray = tempColorsArray[0];
	for (i=0 ; i<numVerts ; i++, bArray+=4, cArray+=3)
	{
		r = Q_ftol (cArray[0] * ent->color[0]);
		g = Q_ftol (cArray[1] * ent->color[1]);
		b = Q_ftol (cArray[2] * ent->color[2]);

		bArray[0] = clamp (r, 0, 255);
		bArray[1] = clamp (g, 0, 255);
		bArray[2] = clamp (b, 0, 255);
	}
}

/*
=============================================================================

	QUAKE III LIGHTMAP

=============================================================================
*/

static byte		*r_q3_lmBuffer;
static int		r_q3_lmBufferSize;
static int		r_q3_lmNumUploaded;
static int		r_q3_lmMaxBlockSize;

/*
=======================
R_Q3BSP_BuildLightmap
=======================
*/
static void R_Q3BSP_BuildLightmap(const int Width, int Height, const byte *data, byte *dest, int blockWidth)
{
	if (!data || r_fullbright->intVal)
	{
		for (int y=0 ; y<Height ; y++)
			memset(dest + y * blockWidth, 255, Width * 4);
		return;
	}

	for (int y=0 ; y<Height ; y++)
	{
		byte *rgba = dest+y*blockWidth;
		for (int x=0 ; x<Width ; x++, rgba+=4)
		{
			vec3_t scaled;
			scaled[0] = data[(y*Width+x) * Q3LIGHTMAP_BYTES+0] * ri.pow2MapOvrbr;
			scaled[1] = data[(y*Width+x) * Q3LIGHTMAP_BYTES+1] * ri.pow2MapOvrbr;
			scaled[2] = data[(y*Width+x) * Q3LIGHTMAP_BYTES+2] * ri.pow2MapOvrbr;

			if (!r_coloredLighting->intVal)
			{
				float grey = (scaled[0] * 0.3f) + (scaled[1] * 0.59f) + (scaled[2] * 0.11f);
				Vec3Set(scaled, grey, grey, grey);
			}

			ColorNormalizeb(scaled, rgba);
		}
	}
}


/*
=======================
R_Q3BSP_PackLightmaps
=======================
*/
static int R_Q3BSP_PackLightmaps (int num, int w, int h, int size, const byte *data, mQ3BspLightmapRect_t *rects)
{
	int i, x, y, root;
	byte *block;
	refImage_t *image;
	int	rectX, rectY, rectSize;
	int maxX, maxY, max, xStride;
	double tw, th, tx, ty;

	maxX = r_q3_lmMaxBlockSize / w;
	maxY = r_q3_lmMaxBlockSize / h;
	max = maxY;
	if (maxY > maxX)
		max = maxX;

	if (r_q3_lmNumUploaded >= MAX_LIGHTMAP_IMAGES-1)
		Com_Error (ERR_DROP, "R_Q3BSP_PackLightmaps: - MAX_LIGHTMAP_IMAGES exceeded\n");

	Com_DevPrintf (0, "Packing %i lightmap(s) -> ", num);

	if (!max || num == 1 || !r_lmPacking->intVal) {
		// Process as it is
		R_Q3BSP_BuildLightmap (w, h, data, r_q3_lmBuffer, w * 4);

		image = R_Create2DImage (Q_VarArgs ("*lm%i", r_q3_lmNumUploaded), (byte **)(&r_q3_lmBuffer),
			w, h, IF_CLAMP_ALL|IF_NOPICMIP|IF_NOMIPMAP_LINEAR|IF_NOGAMMA|IF_NOINTENS|IF_NOCOMPRESS|IT_LIGHTMAP, Q3LIGHTMAP_BYTES);

		ri.media.lmTextures[r_q3_lmNumUploaded] = image;
		rects[0].texNum = r_q3_lmNumUploaded;

		rects[0].w = 1; rects[0].x = 0;
		rects[0].h = 1; rects[0].y = 0;

		Com_DevPrintf (0, "%ix%i\n", 1, 1);

		r_q3_lmNumUploaded++;
		return 1;
	}

	// Find the nearest square block size
	root = (int)sqrtf(num);
	if (root > max)
		root = max;

	// Keep row size a power of two
	for (i=1 ; i<root ; i <<= 1);
	if (i > root)
		i >>= 1;
	root = i;

	num -= root * root;
	rectX = rectY = root;

	if (maxY > maxX) {
		for ( ; num>=root && rectY<maxY ; rectY++, num-=root);

		// Sample down if not a power of two
		for (y=1 ; y<rectY ; y <<= 1);
		if (y > rectY)
			y >>= 1;
		rectY = y;
	}
	else {
		for ( ; num>=root && rectX<maxX ; rectX++, num-=root);

		// Sample down if not a power of two
		for (x=1 ; x<rectX ; x<<=1);
		if (x > rectX)
			x >>= 1;
		rectX = x;
	}

	tw = 1.0 / (double)rectX;
	th = 1.0 / (double)rectY;

	xStride = w * 4;
	rectSize = (rectX * w) * (rectY * h) * 4;
	if (rectSize > r_q3_lmBufferSize) {
		if (r_q3_lmBuffer)
			Mem_Free (r_q3_lmBuffer);
		r_q3_lmBuffer = (byte*)Mem_PoolAlloc (rectSize, ri.lightSysPool, 0);
		memset (r_q3_lmBuffer, 255, rectSize);
		r_q3_lmBufferSize = rectSize;
	}

	block = r_q3_lmBuffer;
	for (y=0, ty=0.0f, num=0 ; y<rectY ; y++, ty+=th, block+=rectX*xStride*h) {
		for (x=0, tx=0.0f ; x<rectX ; x++, tx+=tw, num++) {
			R_Q3BSP_BuildLightmap (w, h, data + num * size, block + x * xStride, rectX * xStride);

			rects[num].w = tw; rects[num].x = tx;
			rects[num].h = th; rects[num].y = ty;
		}
	}

	image = R_Create2DImage (Q_VarArgs ("*lm%i", r_q3_lmNumUploaded), (byte **)(&r_q3_lmBuffer),
		rectX * w, rectY * h, IF_CLAMP_ALL|IF_NOPICMIP|IF_NOMIPMAP_LINEAR|IF_NOGAMMA|IF_NOINTENS|IF_NOCOMPRESS|IT_LIGHTMAP, Q3LIGHTMAP_BYTES);

	ri.media.lmTextures[r_q3_lmNumUploaded] = image;
	for (i=0 ; i<num ; i++)
		rects[i].texNum = r_q3_lmNumUploaded;

	Com_DevPrintf (0, "%ix%i\n", rectX, rectY);

	r_q3_lmNumUploaded++;
	return num;
}


/*
=======================
R_Q3BSP_BuildLightmaps
=======================
*/
void R_Q3BSP_BuildLightmaps(int numLightmaps, int w, int h, const byte *data, mQ3BspLightmapRect_t *rects)
{
	int		i;
	int		size;

	// Pack lightmaps
	for (size=1 ; size<r_lmMaxBlockSize->intVal && size<ri.config.maxTexSize ; size<<=1);

	r_q3_lmNumUploaded = 0;
	r_q3_lmMaxBlockSize = size;
	size = w * h * Q3LIGHTMAP_BYTES;
	r_q3_lmBufferSize = w * h * 4;
	r_q3_lmBuffer = (byte*)Mem_PoolAlloc(r_q3_lmBufferSize, ri.lightSysPool, 0);

	for (i=0 ; i<numLightmaps ; )
		i += R_Q3BSP_PackLightmaps(numLightmaps - i, w, h, size, data + i * size, &rects[i]);

	if (r_q3_lmBuffer)
		Mem_Free(r_q3_lmBuffer);

	Com_DevPrintf (0, "Packed %i lightmaps into %i texture(s)\n", numLightmaps, r_q3_lmNumUploaded);
}


/*
=======================
R_Q3BSP_TouchLightmaps
=======================	
*/
static void R_Q3BSP_TouchLightmaps()
{
	for (int i=0 ; i<r_q3_lmNumUploaded ; i++)
		R_TouchImage(ri.media.lmTextures[i]);
}

/*
=============================================================================

	COMMON LIGHTING

=============================================================================
*/

/*
=============
R_CullDynamicLightList
=============
*/
void R_CullDynamicLightList()
{
	ri.scn.dLightCullBits = 0;
	for (uint32 num=0 ; num<ri.scn.numDLights ; num++)
	{
		refDLight_t *dl = &ri.scn.dLightList[num];
		if (R_CullBox(dl->mins, dl->maxs, ri.scn.clipFlags))
		{
			ri.scn.dLightCullBits |= BIT(num);
			if (!ri.scn.bDrawingMeshOutlines)
				ri.pc.cullDLight[CULL_PASS]++;
		}
		else
		{
			if (!ri.scn.bDrawingMeshOutlines)
				ri.pc.cullDLight[CULL_FAIL]++;
		}
	}
}


/*
=================
R_MarkBModelLights
=================
*/
void R_MarkBModelLights(refEntity_t *ent, vec3_t mins, vec3_t maxs)
{
	if (!gl_dynamic->intVal || !ri.scn.numDLights || r_fullbright->intVal)
		return;

	qStatCycle_Scope Stat(r_times, ri.pc.timeMarkLights);

	refModel_t *model = ent->model;
	for (uint32 i=0 ; i<ri.scn.numDLights ; i++)
	{
		if (ri.scn.dLightCullBits & BIT(i))
			continue;
		if (!model->BSPData()->firstModelLitSurface)
			continue;

		refDLight_t *lt = &ri.scn.dLightList[i];
		if (!BoundsIntersect(mins, maxs, lt->mins, lt->maxs))
			continue;

		mBspSurface_t **mark = model->BSPData()->firstModelLitSurface;
		do
		{
			mBspSurface_t *surf = *mark++;
			if (surf->dLightFrame != ri.frameCount)
			{
				surf->dLightBits = 0;
				surf->dLightFrame = ri.frameCount;
			}
			surf->dLightBits |= BIT(i);
		} while (*mark);
	}
}


/*
=============
R_LightBounds
=============
*/
void R_LightBounds(const vec3_t origin, float intensity, vec3_t mins, vec3_t maxs)
{
	if (ri.scn.worldModel->type == MODEL_Q2BSP)
	{
		Vec3Set(mins, origin[0] - (intensity * 2), origin[1] - (intensity * 2), origin[2] - (intensity * 2));
		Vec3Set(maxs, origin[0] + (intensity * 2), origin[1] + (intensity * 2), origin[2] + (intensity * 2));
		return;
	}

	Vec3Set(mins, origin[0] - intensity, origin[1] - intensity, origin[2] - intensity);
	Vec3Set(maxs, origin[0] + intensity, origin[1] + intensity, origin[2] + intensity);
}

/*
=============================================================================

	FUNCTION WRAPPING

=============================================================================
*/

/*
=============
R_LightPoint
=============
*/
void R_LightPoint(vec3_t point, vec3_t light)
{
	if (ri.def.rdFlags & RDF_NOWORLDMODEL)
		return;

	if (ri.scn.worldModel->type == MODEL_Q2BSP)
	{
		R_Q2BSP_LightPoint(point, light);
		return;
	}

	// FIXME
	Vec3Set(light, 1, 1, 1);
}


/*
=============
R_SetLightLevel
=============
*/
void R_SetLightLevel()
{
	if (ri.def.rdFlags & RDF_NOWORLDMODEL)
		return;

	if (ri.scn.worldModel->type == MODEL_Q2BSP)
	{
		R_Q2BSP_SetLightLevel();
		return;
	}
}


/*
=======================
R_TouchLightmaps
=======================
*/
void R_TouchLightmaps()
{
	if (ri.def.rdFlags & RDF_NOWORLDMODEL)
		return;

	if (ri.scn.worldModel->type == MODEL_Q2BSP)
	{
		R_Q2BSP_TouchLightmaps();
		return;
	}

	R_Q3BSP_TouchLightmaps();
}


/*
=============
R_LightForEntity
=============
*/
void R_LightForEntity(refEntity_t *ent, vec3_t *vertexArray, const int numVerts, vec3_t *normalArray, byte *colorArray)
{
	if (ri.def.rdFlags & RDF_NOWORLDMODEL || ri.scn.worldModel->type == MODEL_Q2BSP)
	{
		R_Q2BSP_LightForEntity(ent, vertexArray, numVerts, normalArray, colorArray);
		return;
	}

	R_Q3BSP_LightForEntity(ent, vertexArray, numVerts, normalArray, colorArray);
}
