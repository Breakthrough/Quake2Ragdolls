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
// rf_cull.c
//

#include "rf_local.h"

/*
=============================================================================

	FRUSTUM CULLING

=============================================================================
*/

/*
===============
R_SetupFrustum
===============
*/
void R_SetupFrustum()
{
	if (r_noCull->intVal)
	{
		ri.scn.clipFlags = 0;
	}
	else
	{
		ri.scn.clipFlags = BIT(FRP_NEAR) | BIT(FRP_LEFT) | BIT(FRP_RIGHT) | BIT(FRP_DOWN) | BIT(FRP_UP);

		// Far cull in shadowmap rendering
		if (ri.scn.viewType == RVT_SHADOWMAP)
			ri.scn.clipFlags |= BIT(FRP_FAR);
	}

	// Calculate the view frustum
	vec3_t rightVec;
	Vec3Negate(ri.def.viewAxis[1], rightVec);
	Vec3Copy(ri.def.viewAxis[0], ri.scn.viewFrustum[FRP_NEAR].normal);
	RotatePointAroundVector(ri.scn.viewFrustum[FRP_LEFT].normal, ri.def.viewAxis[2], ri.def.viewAxis[0], -(90-ri.def.fovX / 2));
	RotatePointAroundVector(ri.scn.viewFrustum[FRP_RIGHT].normal, ri.def.viewAxis[2], ri.def.viewAxis[0], 90-ri.def.fovX / 2);
	RotatePointAroundVector(ri.scn.viewFrustum[FRP_DOWN].normal, rightVec, ri.def.viewAxis[0], 90-ri.def.fovY / 2);
	RotatePointAroundVector(ri.scn.viewFrustum[FRP_UP].normal, rightVec, ri.def.viewAxis[0], -(90 - ri.def.fovY / 2));

	// Set our plane values
	for (int i=0 ; i<FRP_FAR ; i++)
	{
		ri.scn.viewFrustum[i].type = PLANE_NON_AXIAL;
		ri.scn.viewFrustum[i].dist = DotProduct(ri.def.viewOrigin, ri.scn.viewFrustum[i].normal);
		ri.scn.viewFrustum[i].signBits = SignbitsForPlane(&ri.scn.viewFrustum[i]);
	}

	// Adjust the near plane
	ri.scn.viewFrustum[FRP_NEAR].dist += r_zNear->floatVal;

	// Calculate the far plane
	vec3_t farPoint;
	Vec3Negate(ri.def.viewAxis[0], ri.scn.viewFrustum[FRP_FAR].normal);
	Vec3MA(ri.def.viewOrigin, ri.scn.zFar, ri.def.viewAxis[0], farPoint);

	// Set the far plane's values
	ri.scn.viewFrustum[FRP_FAR].type = PLANE_NON_AXIAL;
	ri.scn.viewFrustum[FRP_FAR].dist = DotProduct(farPoint, ri.scn.viewFrustum[FRP_FAR].normal);
	ri.scn.viewFrustum[FRP_FAR].signBits = SignbitsForPlane(&ri.scn.viewFrustum[FRP_FAR]);
}


/*
=================
R_CullBox

Returns true if the box is completely outside the frustum
=================
*/
bool R_CullBox(const vec3_t mins, const vec3_t maxs, const uint32 clipFlags)
{
	if (r_noCull->intVal)
		return false;

	for (int i=0 ; i<FRP_MAX ; i++)
	{
		if (!(clipFlags & BIT(i)))
			continue;

		plane_t *p = &ri.scn.viewFrustum[i];
		switch(p->signBits)
		{
		case 0:
			if (p->normal[0]*maxs[0] + p->normal[1]*maxs[1] + p->normal[2]*maxs[2] < p->dist)
			{
				if (!ri.scn.bDrawingMeshOutlines)
					ri.pc.cullBounds[CULL_PASS]++;
				return true;
			}
			break;

		case 1:
			if (p->normal[0]*mins[0] + p->normal[1]*maxs[1] + p->normal[2]*maxs[2] < p->dist)
			{
				if (!ri.scn.bDrawingMeshOutlines)
					ri.pc.cullBounds[CULL_PASS]++;
				return true;
			}
			break;

		case 2:
			if (p->normal[0]*maxs[0] + p->normal[1]*mins[1] + p->normal[2]*maxs[2] < p->dist)
			{
				if (!ri.scn.bDrawingMeshOutlines)
					ri.pc.cullBounds[CULL_PASS]++;
				return true;
			}
			break;

		case 3:
			if (p->normal[0]*mins[0] + p->normal[1]*mins[1] + p->normal[2]*maxs[2] < p->dist)
			{
				if (!ri.scn.bDrawingMeshOutlines)
					ri.pc.cullBounds[CULL_PASS]++;
				return true;
			}
			break;

		case 4:
			if (p->normal[0]*maxs[0] + p->normal[1]*maxs[1] + p->normal[2]*mins[2] < p->dist)
			{
				if (!ri.scn.bDrawingMeshOutlines)
					ri.pc.cullBounds[CULL_PASS]++;
				return true;
			}
			break;

		case 5:
			if (p->normal[0]*mins[0] + p->normal[1]*maxs[1] + p->normal[2]*mins[2] < p->dist)
			{
				if (!ri.scn.bDrawingMeshOutlines)
					ri.pc.cullBounds[CULL_PASS]++;
				return true;
			}
			break;

		case 6:
			if (p->normal[0]*maxs[0] + p->normal[1]*mins[1] + p->normal[2]*mins[2] < p->dist)
			{
				if (!ri.scn.bDrawingMeshOutlines)
					ri.pc.cullBounds[CULL_PASS]++;
				return true;
			}
			break;

		case 7:
			if (p->normal[0]*mins[0] + p->normal[1]*mins[1] + p->normal[2]*mins[2] < p->dist)
			{
				if (!ri.scn.bDrawingMeshOutlines)
					ri.pc.cullBounds[CULL_PASS]++;
				return true;
			}
			break;

		default:
			assert (0);
			return false;
		}
	}

	if (!ri.scn.bDrawingMeshOutlines)
		ri.pc.cullBounds[CULL_FAIL]++;
	return false;
}


/*
=================
R_CullSphere

Returns true if the sphere is completely outside the frustum
=================
*/
bool R_CullSphere(const vec3_t origin, const float radius, const uint32 clipFlags)
{
	if (r_noCull->intVal)
		return false;

	for (int i=0 ; i<FRP_MAX ; i++)
	{
		if (!(clipFlags & BIT(i)))
			continue;

		plane_t *p = &ri.scn.viewFrustum[i];
		if (DotProduct(origin, p->normal)-p->dist <= -radius)
		{
			if (!ri.scn.bDrawingMeshOutlines)
				ri.pc.cullRadius[CULL_PASS]++;
			return true;
		}
	}

	if (!ri.scn.bDrawingMeshOutlines)
		ri.pc.cullRadius[CULL_FAIL]++;
	return false;
}

/*
=============================================================================

	MAP VISIBILITY CULLING

=============================================================================
*/

/*
===============
R_CullNode

Returns true if this node hasn't been touched this frame
===============
*/
bool R_CullNode(mBspNode_t *node)
{
	if (r_noCull->intVal)
		return false;

	if (!node || node->visFrame == ri.scn.visFrameCount)
	{
		if (!ri.scn.bDrawingMeshOutlines)
			ri.pc.cullVis[CULL_FAIL]++;
		return false;
	}

	if (!ri.scn.bDrawingMeshOutlines)
		ri.pc.cullVis[CULL_PASS]++;
	return true;
}


/*
===============
R_CullSurface

Returns true if this surface hasn't been touched this frame
===============
*/
bool R_CullSurface(mBspSurface_t *surf)
{
	if (r_noCull->intVal)
		return false;

	if (!surf || surf->visFrame == ri.frameCount)
	{
		if (!ri.scn.bDrawingMeshOutlines)
			ri.pc.cullSurf[CULL_FAIL]++;
		return false;
	}

	if (!ri.scn.bDrawingMeshOutlines)
		ri.pc.cullSurf[CULL_PASS]++;
	return true;
}

/*
=============================================================================

	TRANSFORMED BOUNDS CULLING

=============================================================================
*/

/*
=============
R_TransformBoundsToBBox
=============
*/
void R_TransformBoundsToBBox(const vec3_t Origin, const mat3x3_t Axis, const vec3_t Mins, const vec3_t Maxs, vec3_t OutBBox[8])
{
	// Compute and rotate a full bounding box
	for (int i=0 ; i<8 ; i++)
	{
		vec3_t corner;
		corner[0] = ( ( i & 1 ) ? Mins[0] : Maxs[0] );
		corner[1] = ( ( i & 2 ) ? Mins[1] : Maxs[1] );
		corner[2] = ( ( i & 4 ) ? Mins[2] : Maxs[2] );

		Matrix3_TransformVector(Axis, corner, OutBBox[i]);
		OutBBox[i][0] += Origin[0];
		OutBBox[i][1] = -OutBBox[i][1] + Origin[1];
		OutBBox[i][2] += Origin[2];
	}
}


/*
=============
R_CullTransformedBox
=============
*/
bool R_CullTransformedBox(const vec3_t BBox[8], const uint32 clipFlags)
{
	if (r_noCull->intVal)
		return false;

	int aggregateMask = ~0;
	for (int i=0 ; i<8 ; i++)
	{
		int mask = 0;
		for (int num=0 ; num<FRP_MAX ; num++)
		{
			if (clipFlags & BIT(num))
			{
				plane_t *p = &ri.scn.viewFrustum[num];
				if (DotProduct(p->normal, BBox[i]) < p->dist)
					mask |= BIT(num);
			}
		}

		aggregateMask &= mask;
	}

	if (aggregateMask != 0)
	{
		if (!ri.scn.bDrawingMeshOutlines)
			ri.pc.cullBounds[CULL_PASS]++;
		return true;
	}

	if (!ri.scn.bDrawingMeshOutlines)
		ri.pc.cullBounds[CULL_FAIL]++;
	return false;
}
