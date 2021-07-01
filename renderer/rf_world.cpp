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
// rf_world.c
// World surface related refresh code
//

#include "rf_local.h"

/*
=============================================================================

	QUAKE II WORLD MODEL

=============================================================================
*/

/*
================
R_AddQ2Surface
================
*/
static void R_AddQ2Surface(mBspSurface_t *surf, mQ2BspTexInfo_t *texInfo, refEntity_t *entity)
{
	surf->visFrame = ri.frameCount;

	// Add to list
	if (!ri.scn.currentList->AddToList(MBT_Q2BSP, surf, texInfo->mat, entity->matTime, entity, NULL, 0))
		return;

	// Caustics
	if (r_caustics->intVal && surf->q2_flags & SURF_UNDERWATER && texInfo->mat->sortKey == MAT_SORT_OPAQUE && surf->lmTexNum != BAD_LMTEXNUM)
	{
		if (surf->q2_flags & SURF_LAVA && ri.media.worldLavaCaustics)
			ri.scn.currentList->AddToList(MBT_Q2BSP, surf, ri.media.worldLavaCaustics, entity->matTime, entity, NULL, 0);
		else if (surf->q2_flags & SURF_SLIME && ri.media.worldSlimeCaustics)
			ri.scn.currentList->AddToList(MBT_Q2BSP, surf, ri.media.worldSlimeCaustics, entity->matTime, entity, NULL, 0);
		else if (ri.media.worldWaterCaustics)
			ri.scn.currentList->AddToList(MBT_Q2BSP, surf, ri.media.worldWaterCaustics, entity->matTime, entity, NULL, 0);
	}
}


/*
================
R_Q2SurfMaterial
================
*/
static inline mQ2BspTexInfo_t *R_Q2SurfMaterial(mBspSurface_t *surf)
{
	mQ2BspTexInfo_t	*texInfo;
	int				i;

	// Doesn't animate
	if (!surf->q2_texInfo->next)
		return surf->q2_texInfo;

	// Animates
	texInfo = surf->q2_texInfo;
	for (i=((int)(ri.def.time * 2))%texInfo->numFrames ; i ; i--)
		texInfo = texInfo->next;

	return texInfo;
}


/*
================
R_CullQ2SurfacePlanar
================
*/
static bool R_CullQ2SurfacePlanar(const mBspSurface_t *surf, const refMaterial_t *mat, const float dist)
{
	// Side culling
	if (r_facePlaneCull->intVal)
	{
		if (mat->stateBits & SB_CULL_BACK)
		{
			if (surf->q2_flags & SURF_PLANEBACK)
			{
				if (dist <= SMALL_EPSILON)
				{
					if (!ri.scn.bDrawingMeshOutlines)
						ri.pc.cullPlanar[CULL_PASS]++;
					return true;	// Wrong side
				}
			}
			else
			{
				if (dist >= -SMALL_EPSILON)
				{
					if (!ri.scn.bDrawingMeshOutlines)
						ri.pc.cullPlanar[CULL_PASS]++;
					return true;	// Wrong side
				}
			}
		}
		else if (mat->stateBits & SB_CULL_FRONT)
		{
			if (surf->q2_flags & SURF_PLANEBACK)
			{
				if (dist >= -SMALL_EPSILON)
				{
					if (!ri.scn.bDrawingMeshOutlines)
						ri.pc.cullPlanar[CULL_PASS]++;
					return true;	// Wrong side
				}
			}
			else
			{
				if (dist <= SMALL_EPSILON)
				{
					if (!ri.scn.bDrawingMeshOutlines)
						ri.pc.cullPlanar[CULL_PASS]++;
					return true;	// Wrong side
				}
			}
		}
	}

	if (!ri.scn.bDrawingMeshOutlines)
		ri.pc.cullPlanar[CULL_FAIL]++;
	return false;
}


/*
================
R_CullQ2SurfaceBounds
================
*/
#define R_CullQ2SurfaceBounds(surf,clipFlags) R_CullBox((surf)->mins,(surf)->maxs,(clipFlags))


/*
===============
R_MarkQ2Leaves

Mark the leaves and nodes that are in the PVS for the current cluster
===============
*/
static void R_MarkQ2Leaves()
{
	static int oldViewCluster2;
	static int viewCluster2;

	if (ri.scn.viewType == RVT_SHADOWMAP)
		return;

	// If we're forcing an update, forcibly update the "other" view cluster
	if (ri.scn.viewCluster == -1)
	{
		ri.scn.oldViewCluster = -1;
		viewCluster2 = -1;
		oldViewCluster2 = -1;
	}

	// Current viewcluster
	mBspLeaf_t *leaf = R_PointInBSPLeaf(ri.def.viewOrigin, ri.scn.worldModel);
	ri.scn.viewCluster = viewCluster2 = leaf->cluster;

	// Check above and below so crossing solid water doesn't draw wrong
	vec3_t temp;
	temp[0] = ri.def.viewOrigin[0];
	temp[1] = ri.def.viewOrigin[1];
	if (!leaf->q2_contents)
	{
		// Look down a bit
		temp[2] = ri.def.viewOrigin[2] - 16;
		leaf = R_PointInBSPLeaf(temp, ri.scn.worldModel);
		if (!(leaf->q2_contents & CONTENTS_SOLID))
			viewCluster2 = leaf->cluster;
	}
	else
	{
		// Look up a bit
		temp[2] = ri.def.viewOrigin[2] + 16;
		leaf = R_PointInBSPLeaf(temp, ri.scn.worldModel);
		if (!(leaf->q2_contents & CONTENTS_SOLID))
			viewCluster2 = leaf->cluster;
	}

	if (ri.scn.oldViewCluster == ri.scn.viewCluster && oldViewCluster2 == viewCluster2 && (ri.def.rdFlags & RDF_OLDAREABITS) && !r_noVis->intVal && ri.scn.viewCluster != -1)
		return;

	// Development aid to let you run around and see exactly where the pvs ends
	if (gl_lockpvs->intVal)
		return;

	ri.scn.visFrameCount++;
	ri.scn.oldViewCluster = ri.scn.viewCluster;
	oldViewCluster2 = viewCluster2;

	if (r_noVis->intVal || ri.scn.viewCluster == -1 || !ri.scn.worldModel->Q2BSPData()->vis)
	{
		// Mark everything
		for (int i=0 ; i<ri.scn.worldModel->BSPData()->numLeafs ; i++)
			ri.scn.worldModel->BSPData()->leafs[i].visFrame = ri.scn.visFrameCount;
		for (int i=0 ; i<ri.scn.worldModel->BSPData()->numNodes ; i++)
			ri.scn.worldModel->BSPData()->nodes[i].visFrame = ri.scn.visFrameCount;
		return;
	}

	byte *vis = R_BSPClusterPVS(ri.scn.viewCluster, ri.scn.worldModel);

	// May have to combine two clusters because of solid water boundaries
	if (viewCluster2 != ri.scn.viewCluster)
	{
		byte fatVis[Q2BSP_MAX_VIS];
		memcpy(fatVis, vis, (ri.scn.worldModel->BSPData()->numLeafs+7)/8);
		vis = R_BSPClusterPVS(viewCluster2, ri.scn.worldModel);
		int c = (ri.scn.worldModel->BSPData()->numLeafs+31)/32;
		for (int i=0 ; i<c ; i++)
			((int*)fatVis)[i] |= ((int*)vis)[i];
		vis = fatVis;
	}

	for (int i=0 ; i<ri.scn.worldModel->BSPData()->numLeafs ; i++)
	{
		leaf = &ri.scn.worldModel->BSPData()->leafs[i];
		if (leaf->cluster != -1 && vis[leaf->cluster>>3] & BIT(leaf->cluster&7))
		{
			mBspNode_t *node = (mBspNode_t *)leaf;
			do
			{
				if (node->visFrame == ri.scn.visFrameCount)
					break;

				node->visFrame = ri.scn.visFrameCount;
				node = node->parent;
			} while (node);
		}
	}
}


/*
================
R_RecursiveQ2WorldNode
================
*/
static void R_RecursiveQ2WorldNode(mBspNode_t *node, uint32 clipFlags)
{
	if (node->q2_contents == CONTENTS_SOLID)
		return;		// Solid
	if (R_CullNode(node))
		return;		// Node not visible this frame

	// Cull
	if (clipFlags && !node->badBounds)
	{
		for (int num=0 ; num<FRP_MAX ; num++)
		{
			if (clipFlags & BIT(num))
			{
				plane_t *p = &ri.scn.viewFrustum[num];
				int clipped = BoxOnPlaneSide(node->mins, node->maxs, p);
				switch(clipped)
				{
				case 1:
					clipFlags &= ~BIT(num);
					break;

				case 2:
					if (!ri.scn.bDrawingMeshOutlines)
						ri.pc.cullBounds[CULL_PASS]++;
					return;
				}
			}
		}

		if (!ri.scn.bDrawingMeshOutlines)
			ri.pc.cullBounds[CULL_FAIL]++;
	}

	// If this is a leaf node
	if (node->q2_contents != -1)
	{
		mBspLeaf_t *Leaf = (mBspLeaf_t*)node;

		// Check for door connected areas
		if (ri.def.areaBits)
		{
			if (!(ri.def.areaBits[Leaf->area>>3] & BIT(Leaf->area&7)))
				return; // Not visible
		}

		// Mark surfaces visible
		mBspSurface_t **mark = Leaf->q2_firstMarkSurface;
		int Count = Leaf->q2_numMarkSurfaces;

		if (Count)
		{
			do
			{
				mBspSurface_t *surf = *mark++;
				surf->visFrame = ri.frameCount;
			} while (--Count);
		}

		return;
	}

	// Node is just a decision point, so go down the apropriate sides
	// Find which side of the node we are on
	const float dist = PlaneDiff(ri.def.viewOrigin, node->plane);
	const int side = (dist >= 0) ? 0 : 1;

	// Recurse down the children, back side first
	R_RecursiveQ2WorldNode(node->children[side], clipFlags);

	if (node->q2_firstVisSurface && *node->q2_firstVisSurface)
	{
		// Add to rendering bounds
		if (!node->badBounds)
		{
			AddPointToBounds(node->mins, ri.scn.visMins, ri.scn.visMaxs);
			AddPointToBounds(node->maxs, ri.scn.visMins, ri.scn.visMaxs);
		}

		// Draw stuff
		mBspSurface_t **mark = node->q2_firstVisSurface;
		do
		{
			mBspSurface_t *surf = *mark++;

			// Check to make sure it was marked by a leaf (above)
			if (R_CullSurface(surf))
				continue;

			// Get the material
			mQ2BspTexInfo_t *texInfo = R_Q2SurfMaterial(surf);

			// Cull
			if (R_CullQ2SurfacePlanar(surf, texInfo->mat, dist))
				continue;
			if (R_CullQ2SurfaceBounds(surf, clipFlags))
				continue;

			// Sky surface
			if (texInfo->flags & SURF_TEXINFO_SKY)
			{
				R_ClipSkySurface(surf);
				continue;
			}

			// World surface
			R_AddQ2Surface(surf, texInfo, ri.scn.worldEntity);
		} while (*mark);
	}

	// Recurse down the front side
	R_RecursiveQ2WorldNode(node->children[side^1], clipFlags);
}

/*
=============================================================================

	QUAKE II BRUSH MODELS

=============================================================================
*/

/*
=================
R_AddQ2BrushModel
=================
*/
void R_AddQ2BrushModel(refEntity_t *ent)
{
	// No surfaces
	assert(ent->model->BSPData()->firstModelVisSurface);

	vec3_t origin, mins, maxs;
	Vec3Subtract(ri.def.viewOrigin, ent->origin, origin);
	if (!Matrix3_Compare(ent->axis, axisIdentity))
	{
		mins[0] = ent->origin[0] - ent->model->radius * ent->scale;
		mins[1] = ent->origin[1] - ent->model->radius * ent->scale;
		mins[2] = ent->origin[2] - ent->model->radius * ent->scale;

		maxs[0] = ent->origin[0] + ent->model->radius * ent->scale;
		maxs[1] = ent->origin[1] + ent->model->radius * ent->scale;
		maxs[2] = ent->origin[2] + ent->model->radius * ent->scale;

		vec3_t temp;
		Vec3Copy(origin, temp);
		Matrix3_TransformVector(ent->axis, temp, origin);
	}
	else
	{
		// Calculate bounds
		Vec3MA(ent->origin, ent->scale, ent->model->mins, mins);
		Vec3MA(ent->origin, ent->scale, ent->model->maxs, maxs);
	}

	// Calculate dynamic lighting for bmodel
	R_MarkBModelLights(ent, mins, maxs);

	// Draw the surfaces
	mBspSurface_t **mark = ent->model->BSPData()->firstModelVisSurface;
	do
	{
		mBspSurface_t *surf = *mark++;

		// Get the material
		mQ2BspTexInfo_t *texInfo = R_Q2SurfMaterial(surf);

		// These aren't drawn here, ever.
		if (texInfo->flags & SURF_TEXINFO_SKY)
			continue;

		// Find which side of the node we are on
		const float dist = PlaneDiff(origin, surf->q2_plane);

		// Cull
		if (R_CullQ2SurfacePlanar(surf, texInfo->mat, dist))
			continue;

		// World surface
		R_AddQ2Surface(surf, texInfo, ent);
	} while (*mark);
}

/*
=============================================================================

	QUAKE III WORLD MODEL

=============================================================================
*/

/*
================
R_AddQ3Surface
================
*/
static void R_AddQ3Surface(mBspSurface_t *surf, refEntity_t *ent, EMeshBufferType meshType)
{
	// Add to list
	if (!ri.scn.currentList->AddToList(meshType, surf, surf->q3_matRef->mat, ent->matTime, ent, surf->q3_fog, 0))
		return;

	// Surface is used this frame
	surf->visFrame = ri.frameCount;
}


/*
================
R_CullQ3FlareSurface
================
*/
static bool R_CullQ3FlareSurface(mBspSurface_t *surf, refEntity_t *ent, const uint32 clipFlags)
{
	if (r_noCull->intVal)
		return false;

	// Check if flares/culling are disabled
	if (!r_flares->intVal || !r_flareFade->floatVal)
		return true;

	// Find the origin
	vec3_t origin;
	if (ent == ri.scn.worldEntity)
	{
		Vec3Copy(surf->q3_origin, origin);
	}
	else
	{
		Matrix3_TransformVector(ent->axis, surf->q3_origin, origin);
		Vec3Add(origin, ent->origin, origin);
	}

	// Check if it's behind the camera
	if ((origin[0]-ri.def.viewOrigin[0])*ri.def.viewAxis[0][0]
	+ (origin[1]-ri.def.viewOrigin[1])*ri.def.viewAxis[0][1]
	+ (origin[2]-ri.def.viewOrigin[2])*ri.def.viewAxis[0][2] < 0)
	{
		if (!ri.scn.bDrawingMeshOutlines)
			ri.pc.cullRadius[CULL_PASS]++;
		return true;
	}
	if (!ri.scn.bDrawingMeshOutlines)
		ri.pc.cullRadius[CULL_FAIL]++;

	// Radius cull
	if (clipFlags && R_CullSphere(origin, 1, clipFlags))
		return true;

	// Visible
	return false;
}


/*
================
R_CullQ3SurfacePlanar
================
*/
static bool R_CullQ3SurfacePlanar(mBspSurface_t *surf, refMaterial_t *mat, vec3_t modelOrigin)
{
	// Check if culling is disabled
	if (r_noCull->intVal
	|| !r_facePlaneCull->intVal
	|| Vec3Compare (surf->q3_origin, vec3Origin)
	|| !(mat->stateBits & SB_CULL_MASK))
		return false;

	// Plane culling
	// FIXME: Technically q3_origin is the normal. Use plane categorization to optimize this.
	float dot;
	if (surf->q3_origin[0] == 1.0f)
		dot = modelOrigin[0] - surf->mesh->vertexArray[0][0];
	else if (surf->q3_origin[1] == 1.0f)
		dot = modelOrigin[1] - surf->mesh->vertexArray[0][1];
	else if (surf->q3_origin[2] == 1.0f)
		dot = modelOrigin[2] - surf->mesh->vertexArray[0][2];
	else
		dot = (modelOrigin[0] - surf->mesh->vertexArray[0][0]) * surf->q3_origin[0]
			+ (modelOrigin[1] - surf->mesh->vertexArray[0][1]) * surf->q3_origin[1]
			+ (modelOrigin[2] - surf->mesh->vertexArray[0][2]) * surf->q3_origin[2];

	if (mat->stateBits & SB_CULL_FRONT || ri.scn.viewType == RVT_MIRROR)
	{
		if (dot <= SMALL_EPSILON)
		{
			if (!ri.scn.bDrawingMeshOutlines)
				ri.pc.cullPlanar[CULL_PASS]++;
			return true;
		}
	}
	else
	{
		if (dot >= -SMALL_EPSILON)
		{
			if (!ri.scn.bDrawingMeshOutlines)
				ri.pc.cullPlanar[CULL_PASS]++;
			return true;
		}
	}

	if (!ri.scn.bDrawingMeshOutlines)
		ri.pc.cullPlanar[CULL_FAIL]++;
	return false;
}


/*
=============
R_CullQ3SurfaceBounds
=============
*/
#define R_CullQ3SurfaceBounds(surf,clipFlags) R_CullBox((surf)->mins,(surf)->maxs,(clipFlags))


/*
=============
R_RecursiveQ3WorldNode
=============
*/
static void R_RecursiveQ3WorldNode(mBspNode_t *node, uint32 clipFlags)
{
	for ( ; ; )
	{
		if (R_CullNode(node))
			return;

		if (clipFlags && !node->badBounds)
		{
			for (int num=0 ; num<FRP_MAX ; num++)
			{
				if (clipFlags & BIT(num))
				{
					plane_t *p = &ri.scn.viewFrustum[num];
					int clipped = BoxOnPlaneSide(node->mins, node->maxs, p);
					switch(clipped)
					{
					case 1:
						clipFlags &= ~BIT(num);
						break;

					case 2:
						if (!ri.scn.bDrawingMeshOutlines)
							ri.pc.cullBounds[CULL_PASS]++;
						return;
					}
				}
			}

			if (!ri.scn.bDrawingMeshOutlines)
				ri.pc.cullBounds[CULL_FAIL]++;
		}

		if (!node->plane)
			break;

		R_RecursiveQ3WorldNode(node->children[0], clipFlags);
		node = node->children[1];
	}

	// If a leaf node, draw stuff
	mBspLeaf_t *leaf = (mBspLeaf_t *)node;
	if (leaf->q3_firstVisSurface && *leaf->q3_firstVisSurface)
	{
		// Check for door connected areas
		if (ri.def.areaBits)
		{
			if (!(ri.def.areaBits[leaf->area>>3] & BIT(leaf->area&7)))
				return;		// Not visible
		}

		// Add to rendering bounds
		if (!leaf->badBounds)
		{
			AddPointToBounds(leaf->mins, ri.scn.visMins, ri.scn.visMaxs);
			AddPointToBounds(leaf->maxs, ri.scn.visMins, ri.scn.visMaxs);
		}

		mBspSurface_t **mark = leaf->q3_firstVisSurface;
		do
		{
			mBspSurface_t *surf = *mark++;

			// See if it's been touched, if not, touch it
			if (surf->q3_nodeFrame == ri.frameCount)
				continue;	// Already touched this frame
			surf->q3_nodeFrame = ri.frameCount;

			// Sky surface
			if (surf->q3_matRef->mat->flags & MAT_SKY)
			{
				if (R_CullQ3SurfacePlanar(surf, surf->q3_matRef->mat, ri.def.viewOrigin))
					continue;
				if (R_CullQ3SurfaceBounds(surf, clipFlags))
					continue;

				R_ClipSkySurface(surf);
				continue;
			}

			switch(surf->q3_faceType)
			{
			case FACETYPE_FLARE:
				if (R_CullQ3FlareSurface(surf, ri.scn.worldEntity, clipFlags))
					continue;

				R_AddQ3Surface(surf, ri.scn.worldEntity, MBT_Q3BSP_FLARE);
				break;

			case FACETYPE_PLANAR:
				if (R_CullQ3SurfacePlanar(surf, surf->q3_matRef->mat, ri.def.viewOrigin))
					continue;
				// FALL THROUGH
			default:
				if (R_CullQ3SurfaceBounds(surf, clipFlags))
					continue;

				R_AddQ3Surface(surf, ri.scn.worldEntity, MBT_Q3BSP);
				break;
			}
		} while (*mark);
	}
}


/*
=============
R_MarkQ3Leaves
=============
*/
static void R_MarkQ3Leaves()
{
	if (ri.scn.viewType == RVT_SHADOWMAP)
		return;

	// Current viewcluster
	if (ri.scn.viewType != RVT_MIRROR)
	{
		if (ri.scn.viewType == RVT_PORTAL)
		{
			ri.scn.oldViewCluster = -1;
			mBspLeaf_t *leaf = R_PointInBSPLeaf(ri.scn.portalOrigin, ri.scn.worldModel);
			ri.scn.viewCluster = leaf->cluster;
		}
		else
		{
			ri.scn.oldViewCluster = ri.scn.viewCluster;
			mBspLeaf_t *leaf = R_PointInBSPLeaf(ri.def.viewOrigin, ri.scn.worldModel);
			ri.scn.viewCluster = leaf->cluster;
		}
	}

	if (ri.scn.oldViewCluster == ri.scn.viewCluster && (ri.def.rdFlags & RDF_OLDAREABITS) && !r_noVis->intVal && ri.scn.viewCluster != -1)
		return;

	// Development aid to let you run around and see exactly where the pvs ends
	if (gl_lockpvs->intVal)
		return;

	ri.scn.visFrameCount++;
	ri.scn.oldViewCluster = ri.scn.viewCluster;

	if (r_noVis->intVal || ri.scn.viewCluster == -1 || !ri.scn.worldModel->Q3BSPData()->vis)
	{
		// Mark everything
		for (int i=0 ; i<ri.scn.worldModel->BSPData()->numLeafs ; i++)
			ri.scn.worldModel->BSPData()->leafs[i].visFrame = ri.scn.visFrameCount;
		for (int i=0 ; i<ri.scn.worldModel->BSPData()->numNodes ; i++)
			ri.scn.worldModel->BSPData()->nodes[i].visFrame = ri.scn.visFrameCount;
		return;
	}

	byte *vis = R_BSPClusterPVS(ri.scn.viewCluster, ri.scn.worldModel);
	for (int i=0 ; i<ri.scn.worldModel->BSPData()->numLeafs ; i++)
	{
		mBspLeaf_t *leaf = &ri.scn.worldModel->BSPData()->leafs[i];
		if (leaf->cluster != -1 && vis[leaf->cluster>>3] & BIT(leaf->cluster&7))
		{
			mBspNode_t *node = (mBspNode_t *)leaf;
			do
			{
				if (node->visFrame == ri.scn.visFrameCount)
					break;
				node->visFrame = ri.scn.visFrameCount;
				node = node->parent;
			} while (node);
		}
	}
}

/*
=============================================================================

	QUAKE III BRUSH MODELS

=============================================================================
*/

/*
=================
R_AddQ3BrushModel
=================
*/
void R_AddQ3BrushModel(refEntity_t *ent)
{
	// No surfaces
	assert(ent->model->BSPData()->firstModelVisSurface);

	// Cull
	vec3_t mins, maxs;
	vec3_t origin;
	Vec3Subtract(ri.def.viewOrigin, ent->origin, origin);
	if (!Matrix3_Compare(ent->axis, axisIdentity))
	{
		mins[0] = ent->origin[0] - ent->model->radius * ent->scale;
		mins[1] = ent->origin[1] - ent->model->radius * ent->scale;
		mins[2] = ent->origin[2] - ent->model->radius * ent->scale;

		maxs[0] = ent->origin[0] + ent->model->radius * ent->scale;
		maxs[1] = ent->origin[1] + ent->model->radius * ent->scale;
		maxs[2] = ent->origin[2] + ent->model->radius * ent->scale;

		vec3_t temp;
		Vec3Copy(origin, temp);
		Matrix3_TransformVector(ent->axis, temp, origin);
	}
	else
	{
		// Calculate bounds
		Vec3MA(ent->origin, ent->scale, ent->model->mins, mins);
		Vec3MA(ent->origin, ent->scale, ent->model->maxs, maxs);
	}

	// Mark lights
	R_MarkBModelLights(ent, mins, maxs);

	// Draw the surfaces
	mBspSurface_t **mark = ent->model->BSPData()->firstModelVisSurface;
	do
	{
		mBspSurface_t *surf = *mark++;

		// These aren't drawn here, ever.
		if (surf->q3_matRef->flags & SHREF_SKY)
			continue;

		// Cull
		switch(surf->q3_faceType)
		{
		case FACETYPE_FLARE:
			if (R_CullQ3FlareSurface(surf, ent, ri.scn.clipFlags))
				continue;

			R_AddQ3Surface(surf, ent, MBT_Q3BSP_FLARE);
			break;

		case FACETYPE_PLANAR:
			if (R_CullQ3SurfacePlanar(surf, surf->q3_matRef->mat, origin))
				continue;
			// FALL THROUGH
		default:
			R_AddQ3Surface(surf, ent, MBT_Q3BSP);
			break;
		}
	} while (*mark);
}

/*
=============================================================================

	FUNCTION WRAPPING

=============================================================================
*/

/*
=================
R_CullBrushModel
=================
*/
bool R_CullBrushModel(refEntity_t *ent, const uint32 clipFlags)
{
	if (!ent->model->BSPData()->firstModelVisSurface)
		return true;
	if (r_noCull->intVal)
		return false;

	// Frustum cull
	if (!Matrix3_Compare(ent->axis, axisIdentity))
	{
		if (R_CullSphere(ent->origin, ent->model->radius * ent->scale, clipFlags))
			return true;
	//	if (R_CullTransformedBounds(ent->origin, ent->axis, ent->model->mins, ent->model->maxs, clipFlags))
	//		return true;
	}
	else
	{
		// Calculate bounds
		vec3_t mins;
		Vec3MA(ent->origin, ent->scale, ent->model->mins, mins);

		vec3_t maxs;
		Vec3MA(ent->origin, ent->scale, ent->model->maxs, maxs);

		if (R_CullBox(mins, maxs, clipFlags))
			return true;
	}

	return false;
}


/*
=============
R_AddWorldToList
=============
*/
void R_AddWorldToList()
{
	// Prepare the sky
	R_AddSkyToList();

	if (ri.def.rdFlags & RDF_NOWORLDMODEL)
		return;

	if (ri.scn.viewType == RVT_SHADOWMAP)
	{
		qStatCycle_Scope Stat(r_times, ri.pc.timeShadowRecurseWorld);
	}
	else
	{
		// Add to rendering bounds
		AddPointToBounds(ri.scn.worldModel->mins, ri.scn.visMins, ri.scn.visMaxs);
		AddPointToBounds(ri.scn.worldModel->maxs, ri.scn.visMins, ri.scn.visMaxs);

		if (ri.scn.worldModel->type == MODEL_Q3BSP)
		{
			// Mark leaves
			{
				qStatCycle_Scope Stat(r_times, ri.pc.timeMarkLeaves);
				R_MarkQ3Leaves();
			}

			if (r_drawworld->intVal)
			{
				// Mark lights
				{
					qStatCycle_Scope Stat(r_times, ri.pc.timeMarkLights);
					R_Q3BSP_MarkWorldLights();
				}

				// Recurse the world
				{
					qStatCycle_Scope Stat(r_times, ri.pc.timeRecurseWorld);
					R_RecursiveQ3WorldNode(ri.scn.worldModel->BSPData()->nodes, ri.scn.clipFlags);
				}
			}
		}
		else
		{
			assert(ri.scn.worldModel->type == MODEL_Q2BSP);

			// Mark leaves
			{
				qStatCycle_Scope Stat(r_times, ri.pc.timeMarkLeaves);
				R_MarkQ2Leaves();
			}

			if (r_drawworld->intVal)
			{
				// Mark lights
				{
					qStatCycle_Scope Stat(r_times, ri.pc.timeMarkLights);
					R_Q2BSP_MarkWorldLights();
				}

				// Recurse the world
				{
					qStatCycle_Scope Stat(r_times, ri.pc.timeRecurseWorld);
					R_RecursiveQ2WorldNode(ri.scn.worldModel->BSPData()->nodes, ri.scn.clipFlags);
				}
			}
		}
	}
}

/*
=============================================================================

	INIT / SHUTDOWN

=============================================================================
*/

/*
==================
R_WorldInit
==================
*/
void R_WorldInit()
{
	R_SkyInit();
}


/*
==================
R_WorldShutdown
==================
*/
void R_WorldShutdown()
{
	R_SkyShutdown();
}
