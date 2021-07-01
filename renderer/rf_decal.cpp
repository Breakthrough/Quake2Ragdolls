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
// rf_decal.c
// FIXME TODO:
// - Clean up CG_SpawnDecal parms
// - could be re-worked so that decals are only added to the list when their owner surface is
// - r_decal_maxFragments r_decal_maxVerts?
//

#include "rf_local.h"

#define MAX_DECAL_VERTS		512
#define MAX_DECAL_FRAGMENTS	384

struct refFragment_t
{
	int					firstVert;
	int					numVerts;

	vec3_t				normal;

	mBspSurface_t		*surf;
};

static refMesh_t		r_decalMesh;

/*
==============================================================================

REFRESH FUNCTIONS

==============================================================================
*/

/*
===============
R_AddDecalsToList
===============
*/
void R_AddDecalsToList()
{
	if (!r_drawDecals->intVal)
		return;

	refMeshBuffer *lastMB = NULL;
	mQ3BspFog_t *lastFog = NULL;
	refMaterial_t *lastMat = NULL;
	float lastMatTime = 0.0f;
	int numVerts = 0;

	// Add decal meshes to list
	for (uint32 i=0 ; i<ri.scn.decalList.Count() ; i++)
	{
		refDecal_t *d = ri.scn.decalList[i];
		if (d->poly.numVerts > RB_MAX_VERTS)
		{
			Com_Printf(PRNT_ERROR, "Bogus decal passed with %i verts, ignoring!\n", d->poly.numVerts);
			lastMB = NULL;
			continue;
		}

		// Check the surface visibility
		mQ3BspFog_t *fog = NULL;
		if (d->numSurfaces)
		{
			uint32 j;
			for (j=0 ; j<d->numSurfaces ; j++)
			{
				mBspSurface_t *surf = d->surfaces[j];
				if (surf == null || !R_CullSurface(surf))
				{
					if (!fog && surf->q3_fog)
						fog = surf->q3_fog;
					break;
				}
			}
			if (j == d->numSurfaces)
			{
				lastMB = NULL;
				continue;
			}
		}

		if (d->poly.mat == null)
		{
			lastMB = NULL;
			continue;
		}

		// Frustum cull
		if (R_CullSphere(d->poly.origin, d->poly.radius, ri.scn.clipFlags))
		{
			lastMB = NULL;
			continue;
		}

		// Add to the list
		if (!lastMB || lastMat != d->poly.mat || lastFog != fog || lastMatTime != d->poly.matTime || numVerts+d->poly.numVerts > RB_MAX_VERTS)
		{
			numVerts = 0;

			lastMat = d->poly.mat;
			lastFog = fog;

			// Encode our index in the decalList as our mesh for later
			lastMB = ri.scn.currentList->AddToList(MBT_DECAL, (void*)(i+1), d->poly.mat, d->poly.matTime, NULL, fog, 0);
		}

		lastMB->infoKey++;
		numVerts += d->poly.numVerts;

		if (!ri.scn.bDrawingMeshOutlines)
			ri.pc.decalsPushed++;
	}
}


/*
================
R_PushDecal
================
*/
void R_PushDecal(refMeshBuffer *mb, const meshFeatures_t features)
{
	int start = ((int)mb->mesh)-1;

	for (int i=0 ; i<mb->infoKey ; i++)
	{
		refDecal_t *d = ri.scn.decalList[start+i];

		r_decalMesh.numIndexes = d->numIndexes;
		r_decalMesh.indexArray = d->indexes;

		r_decalMesh.numVerts = d->poly.numVerts;
		r_decalMesh.colorArray = d->poly.colors;
		r_decalMesh.coordArray = (vec2_t *)d->poly.texCoords;
		r_decalMesh.normalsArray = (vec3_t *)d->normals;
		r_decalMesh.vertexArray = (vec3_t *)d->poly.vertices;

		RB_PushMesh(&r_decalMesh, features);
	}
}


/*
================
R_DecalOverflow
================
*/
bool R_DecalOverflow(refMeshBuffer *mb)
{
	int start = ((int)mb->mesh)-1;

	int totalVerts = 0;
	int totalIndexes = 0;
	for (int i=0 ; i<mb->infoKey ; i++)
	{
		refDecal_t *d = ri.scn.decalList[start+i];
		totalVerts += d->poly.numVerts;
		totalIndexes += d->numIndexes;
	}

	return RB_BackendOverflow(totalVerts, totalIndexes);
}


/*
================
R_DecalInit
================
*/
void R_DecalInit()
{
	r_decalMesh.lmCoordArray = NULL;
	r_decalMesh.sVectorsArray = NULL;
	r_decalMesh.tVectorsArray = NULL;
}

/*
==============================================================================

FRAGMENT CLIPPING

==============================================================================
*/

static uint32			r_numFragmentVerts;
static vec3_t			r_fragmentVerts[MAX_DECAL_VERTS];
static vec3_t			r_fragmentNormals[MAX_DECAL_VERTS];

static uint32			r_numClippedFragments;
static refFragment_t	r_clippedFragments[MAX_DECAL_FRAGMENTS];

static uint32			r_fragmentFrame = 0;
static plane_t			r_fragmentPlanes[6];

static vec3_t			r_decalOrigin;
static vec3_t			r_decalNormal;
static float			r_decalRadius;

/*
=================
R_WindingClipFragment

This function operates on windings (convex polygons without 
any points inside) like triangles, quads, etc. The output is 
a convex fragment (polygon, trifan) which the result of clipping 
the input winding by six fragment planes.
=================
*/
static void R_WindingClipFragment(vec3_t *wVerts, int numVerts, refFragment_t *fr)
{
	int			i, j;
	int			stage, newc, numv;
	plane_t		*plane;
	bool		front;
	float		*v, *nextv, d;
	float		dists[MAX_DECAL_VERTS+1];
	int			sides[MAX_DECAL_VERTS+1];
	vec3_t		*verts, *newverts, newv[2][MAX_DECAL_VERTS];

	numv = numVerts;
	verts = wVerts;

	for (stage=0, plane=r_fragmentPlanes ; stage<6 ; stage++, plane++) {
		for (i=0, v=verts[0], front=false ; i<numv ; i++, v+=3) {
			d = PlaneDiff (v, plane);

			if (d > LARGE_EPSILON) {
				front = true;
				sides[i] = SIDE_FRONT;
			}
			else if (d < -LARGE_EPSILON) {
				sides[i] = SIDE_BACK;
			}
			else {
				front = true;
				sides[i] = SIDE_ON;
			}
			dists[i] = d;
		}

		if (!front)
			return;

		// Clip it
		sides[i] = sides[0];
		dists[i] = dists[0];

		newc = 0;
		newverts = newv[stage & 1];

		for (i=0, v=verts[0] ; i<numv ; i++, v+=3) {
			switch (sides[i]) {
			case SIDE_FRONT:
				if (newc == MAX_DECAL_VERTS)
					return;
				Vec3Copy (v, newverts[newc]);
				newc++;
				break;

			case SIDE_BACK:
				break;

			case SIDE_ON:
				if (newc == MAX_DECAL_VERTS)
					return;
				Vec3Copy (v, newverts[newc]);
				newc++;
				break;
			}

			if (sides[i] == SIDE_ON
				|| sides[i+1] == SIDE_ON
				|| sides[i+1] == sides[i])
				continue;
			if (newc == MAX_DECAL_VERTS)
				return;

			d = dists[i] / (dists[i] - dists[i+1]);
			nextv = (i == numv - 1) ? verts[0] : v + 3;
			for (j=0 ; j<3 ; j++)
				newverts[newc][j] = v[j] + d * (nextv[j] - v[j]);

			newc++;
		}

		if (newc <= 2)
			return;

		// Continue with new verts
		numv = newc;
		verts = newverts;
	}

	// Fully clipped
	if (r_numFragmentVerts + numv > MAX_DECAL_VERTS)
		return;

	fr->numVerts = numv;
	fr->firstVert = r_numFragmentVerts;

	for (i=0, v=verts[0] ; i<numv ; i++, v+=3) {
		Vec3Copy (fr->normal, r_fragmentNormals[r_numFragmentVerts + i]);
		Vec3Copy (v, r_fragmentVerts[r_numFragmentVerts + i]);
	}
	r_numFragmentVerts += numv;
}

/*
=================
R_PlanarSurfClipFragment

NOTE: one might want to combine this function with 
R_WindingClipFragment for special cases like trifans (q1 and
q2 polys) or tristrips for ultra-fast clipping, providing there's 
enough stack space (depending on MAX_DECAL_VERTS value).
=================
*/
static void R_PlanarSurfClipFragment (mBspSurface_t *surf, mBspNode_t *node)
{
	int				i;
	refMesh_t		*mesh;
	index_t			*index;
	vec3_t			*normals, *verts, tri[3];
	refFragment_t	*fr;

	mesh = surf->mesh;

	// Clip each triangle individually
	index = mesh->indexArray;
	normals = mesh->normalsArray;
	verts = mesh->vertexArray;
	for (i=0 ; i<mesh->numIndexes ; i+=3, index+=3) {
		fr = &r_clippedFragments[r_numClippedFragments];
		fr->numVerts = 0;
		fr->surf = surf;

		Vec3Copy (normals[index[0]], fr->normal);

		Vec3Copy (verts[index[0]], tri[0]);
		Vec3Copy (verts[index[1]], tri[1]);
		Vec3Copy (verts[index[2]], tri[2]);

		R_WindingClipFragment (tri, 3, fr);
		if (fr->numVerts && (r_numFragmentVerts == MAX_DECAL_VERTS || ++r_numClippedFragments == MAX_DECAL_FRAGMENTS))
			return;
	}
}

/*
==============================================================================

QUAKE II FRAGMENT CLIPPING

==============================================================================
*/

/*
=================
R_Q2BSP_FragmentNode
=================
*/
static void R_Q2BSP_FragmentNode (mBspNode_t *node)
{
	float			dist;
	mBspLeaf_t		*leaf;
	mBspSurface_t	*surf, **mark;

mark0:
	if (r_numFragmentVerts >= MAX_DECAL_VERTS || r_numClippedFragments >= MAX_DECAL_FRAGMENTS)
		return;	// Already reached the limit somewhere else

	if (node->q2_contents != -1) {
		if (node->q2_contents == CONTENTS_SOLID)
			return;

		// Leaf
		leaf = (mBspLeaf_t *)node;
		if (!leaf->firstFragmentSurface)
			return;

		mark = leaf->firstFragmentSurface;
		do {
			if (r_numFragmentVerts >= MAX_DECAL_VERTS || r_numClippedFragments >= MAX_DECAL_FRAGMENTS)
				return;

			surf = *mark++;
			if (!surf)
				continue;

			if (surf->fragmentFrame == r_fragmentFrame)
				continue;		// Already touched
			surf->fragmentFrame = r_fragmentFrame;

			if (surf->q2_numEdges < 3)
				continue;		// Bogus face

			if (surf->q2_flags & SURF_PLANEBACK) {
				if (DotProduct(r_decalNormal, surf->q2_plane->normal) > -0.5f)
					continue;	// Greater than 60 degrees
			}
			else {
				if (DotProduct(r_decalNormal, surf->q2_plane->normal) < 0.5f)
					continue;	// Greater than 60 degrees
			}

			// Clip
			R_PlanarSurfClipFragment (surf, node);
		} while (*mark);

		return;
	}

	dist = PlaneDiff (r_decalOrigin, node->plane);
	if (dist > r_decalRadius) {
		node = node->children[0];
		goto mark0;
	}
	if (dist < -r_decalRadius) {
		node = node->children[1];
		goto mark0;
	}

	R_Q2BSP_FragmentNode (node->children[0]);
	R_Q2BSP_FragmentNode (node->children[1]);
}

/*
==============================================================================

QUAKE III FRAGMENT CLIPPING

==============================================================================
*/

/*
=================
R_Q3BSP_PatchSurfClipFragment
=================
*/
static void R_Q3BSP_PatchSurfClipFragment (mBspSurface_t *surf, mBspNode_t *node)
{
	int				i;
	refMesh_t		*mesh;
	index_t			*index;
	vec3_t			*normals, *verts, tri[3];
	vec3_t			dir1, dir2, snorm;
	refFragment_t	*fr;

	mesh = surf->mesh;

	// Clip each triangle individually
	index = mesh->indexArray;
	normals = mesh->normalsArray;
	verts = mesh->vertexArray;
	for (i=0 ; i<mesh->numIndexes ; i+=3, index+=3) {
		fr = &r_clippedFragments[r_numClippedFragments];
		fr->numVerts = 0;
		fr->surf = surf;

		Vec3Copy (normals[index[0]], fr->normal);

		Vec3Copy (verts[index[0]], tri[0]);
		Vec3Copy (verts[index[1]], tri[1]);
		Vec3Copy (verts[index[2]], tri[2]);

		// Calculate two mostly perpendicular edge directions
		Vec3Subtract (tri[0], tri[1], dir1);
		Vec3Subtract (tri[2], tri[1], dir2);

		// We have two edge directions, we can calculate a third vector from
		// them, which is the direction of the triangle normal
		CrossProduct (dir1, dir2, snorm);

		// We multiply 0.5 by length of snorm to avoid normalizing
		if (DotProduct(r_decalNormal, snorm) < 0.5f * Vec3Length(snorm))
			continue;	// Greater than 60 degrees

		R_WindingClipFragment (tri, 3, fr);
		if (fr->numVerts && (r_numFragmentVerts == MAX_DECAL_VERTS || ++r_numClippedFragments == MAX_DECAL_FRAGMENTS))
			return;
	}
}


/*
=================
R_Q3BSP_FragmentNode
=================
*/
static void R_Q3BSP_FragmentNode ()
{
	int				stackdepth = 0;
	float			dist;
	mBspNode_t		*node, *localStack[2048];
	mBspLeaf_t		*leaf;
	mBspSurface_t	*surf, **mark;

	node = ri.scn.worldModel->BSPData()->nodes;
	for (stackdepth=0 ; ; ) {
		if (node->plane == NULL) {
			leaf = (mBspLeaf_t *)node;
			if (!leaf->firstFragmentSurface)
				goto nextNodeOnStack;

			mark = leaf->firstFragmentSurface;
			do {
				if (r_numFragmentVerts == MAX_DECAL_VERTS || r_numClippedFragments == MAX_DECAL_FRAGMENTS)
					return;		// Already reached the limit

				surf = *mark++;
				if (surf->fragmentFrame == r_fragmentFrame)
					continue;
				surf->fragmentFrame = r_fragmentFrame;

				if (surf->q3_faceType == FACETYPE_PLANAR) {
					if (DotProduct(r_decalNormal, surf->q3_origin) < 0.5f)
						continue;		// Greater than 60 degrees
					R_PlanarSurfClipFragment (surf, node);
				}
				else {
					R_Q3BSP_PatchSurfClipFragment (surf, node);
				}
			} while (*mark);

			if (r_numFragmentVerts == MAX_DECAL_VERTS || r_numClippedFragments == MAX_DECAL_FRAGMENTS)
				return;		// Already reached the limit

nextNodeOnStack:
			if (!stackdepth)
				break;
			node = localStack[--stackdepth];
			continue;
		}

		dist = PlaneDiff (r_decalOrigin, node->plane);
		if (dist > r_decalRadius) {
			node = node->children[0];
			continue;
		}

		if (dist >= -r_decalRadius && (stackdepth < sizeof(localStack) / sizeof(mBspNode_t *)))
			localStack[stackdepth++] = node->children[0];
		node = node->children[1];
	}
}

// ===========================================================================

/*
=================
R_GetClippedFragments
=================
*/
static uint32 R_GetClippedFragments (vec3_t origin, float radius, vec3_t axis[3])
{
	int		i;
	float	d;

	if (ri.def.rdFlags & RDF_NOWORLDMODEL)
		return 0;
	if (!ri.scn.worldModel->BSPData()->nodes)
		return 0;

	r_fragmentFrame++;

	// Store data
	Vec3Copy (origin, r_decalOrigin);
	Vec3Copy (axis[0], r_decalNormal);
	r_decalRadius = radius;

	// Initialize fragments
	r_numFragmentVerts = 0;
	r_numClippedFragments = 0;

	// Calculate clipping planes
	for (i=0 ; i<3; i++) {
		d = DotProduct (origin, axis[i]);

		Vec3Copy (axis[i], r_fragmentPlanes[i*2].normal);
		r_fragmentPlanes[i*2].dist = d - radius;
		r_fragmentPlanes[i*2].type = PlaneTypeForNormal (r_fragmentPlanes[i*2].normal);

		Vec3Negate (axis[i], r_fragmentPlanes[i*2+1].normal);
		r_fragmentPlanes[i*2+1].dist = -d - radius;
		r_fragmentPlanes[i*2+1].type = PlaneTypeForNormal (r_fragmentPlanes[i*2+1].normal);
	}

	if (ri.scn.worldModel->type == MODEL_Q3BSP)
		R_Q3BSP_FragmentNode ();
	else
		R_Q2BSP_FragmentNode (ri.scn.worldModel->BSPData()->nodes);

	return r_numClippedFragments;
}

/*
==============================================================================

EXPORT FUNCTIONS

==============================================================================
*/

/*
===============
R_CreateDecal
===============
*/
bool R_CreateDecal(refDecal_t *d, struct refMaterial_t *material, vec4_t subUVs, vec3_t origin, vec3_t direction, float angle, float size)
{
	vec3_t			*clipNormals, *clipVerts;
	refFragment_t	*fr, *clipFragments;
	vec3_t			axis[3];
	uint32			numFragments, i, k;
	byte			*buffer;
	uint32			totalIndexes;
	index_t			*outIndexes;
	uint32			totalVerts;
	float			*outNormals;
	float			*outVerts;
	float			*outCoords;
	uint32			totalSurfaces;
	mBspSurface_t	**outSurfs;
	vec3_t			mins, maxs;
	vec3_t			temp;
	int				j;

	if (!d)
		return false;

	// See if there's room and it's valid
	if (!size || Vec3Compare (direction, vec3Origin)) {
		Com_DevPrintf (PRNT_WARNING, "WARNING: attempted to create a decal with an invalid %s\n", !size ? "size" : "direction");
		return false;
	}

	// Negativity check
	if (size < 0)
		size *= -1;

	// Calculate orientation matrix
	VectorNormalizef (direction, axis[0]);
	PerpendicularVector (axis[0], axis[1]);
	RotatePointAroundVector (axis[2], axis[0], axis[1], angle);
	CrossProduct (axis[0], axis[2], axis[1]);

	// Clip it
	clipNormals = r_fragmentNormals;
	clipVerts = r_fragmentVerts;
	clipFragments = r_clippedFragments;
	numFragments = R_GetClippedFragments (origin, size, axis);
	if (!numFragments)
		return false;	// No valid fragments

	// Find the total allocation size
	totalIndexes = 0;
	totalVerts = 0;
	totalSurfaces = 0;
	for (i=0, fr=clipFragments ; i<numFragments ; fr++, i++) {
		totalIndexes += (fr->numVerts - 2) * 3;
		totalVerts += fr->numVerts;

		// NULL out duplicate surfaces to save redundant cull attempts
		if (fr->surf) {
			for (k=i+1 ; k<numFragments ; k++) {
				if (clipFragments[k].surf == fr->surf)
					fr->surf = NULL;
			}

			if (fr->surf)
				totalSurfaces++;
		}
	}
	assert (totalIndexes && totalVerts);

	// Store values
	Vec3Copy (origin, d->poly.origin);
	d->poly.mat = material;
	d->numIndexes = totalIndexes;
	d->poly.numVerts = totalVerts;
	d->numSurfaces = totalSurfaces;

	// Allocate space
	buffer = (byte*)Mem_PoolAlloc ((d->poly.numVerts * sizeof(vec3_t) * 2)
		+ (d->numIndexes * sizeof(index_t))
		+ (d->poly.numVerts * sizeof(vec2_t))
		+ (d->poly.numVerts * sizeof(colorb))
		+ (d->numSurfaces * sizeof(struct mBspSurface_t *)), ri.decalSysPool, 0);
	outVerts = (float *)buffer;
	d->poly.vertices = (vec3_t *)buffer;

	buffer += d->poly.numVerts * sizeof(vec3_t);
	outNormals = (float *)buffer;
	d->normals = (vec3_t *)buffer;

	buffer += d->poly.numVerts * sizeof(vec3_t);
	outIndexes = d->indexes = (int *)buffer;

	buffer += d->numIndexes * sizeof(index_t);
	outCoords = (float *)buffer;
	d->poly.texCoords = (vec2_t *)buffer;

	buffer += d->poly.numVerts * sizeof(vec2_t);
	d->poly.colors = (colorb *)buffer;

	buffer += d->poly.numVerts * sizeof(colorb);
	d->surfaces = outSurfs = (struct mBspSurface_t **)buffer;

	// Store vertex data
	ClearBounds (mins, maxs);
	totalVerts = 0;
	totalIndexes = 0;

	size = 0.5f / size;
	Vec3Scale (axis[1], size, axis[1]);
	Vec3Scale (axis[2], size, axis[2]);
	for (i=0, fr=clipFragments ; i<numFragments ; fr++, i++) {
		if (fr->surf)
			*outSurfs++ = fr->surf;

		// Indexes
		outIndexes = d->indexes + totalIndexes;
		totalIndexes += (fr->numVerts - 2) * 3;
		for (j=2 ; j<fr->numVerts ; j++) {
			outIndexes[0] = totalVerts;
			outIndexes[1] = totalVerts + j - 1;
			outIndexes[2] = totalVerts + j;

			outIndexes += 3;
		}

		for (j=0 ; j<fr->numVerts ; j++) {
			// Vertices
			outVerts[0] = clipVerts[fr->firstVert+j][0];
			outVerts[1] = clipVerts[fr->firstVert+j][1];
			outVerts[2] = clipVerts[fr->firstVert+j][2];

			// Normals
			outNormals[0] = clipNormals[fr->firstVert+j][0];
			outNormals[1] = clipNormals[fr->firstVert+j][1];
			outNormals[2] = clipNormals[fr->firstVert+j][2];

			// Bounds
			AddPointToBounds (outVerts, mins, maxs);

			// Coords
			Vec3Subtract (outVerts, origin, temp);
			outCoords[0] = DotProduct (temp, axis[1]) + 0.5f;
			outCoords[1] = DotProduct (temp, axis[2]) + 0.5f;

			// Sub coords
			outCoords[0] = subUVs[0] + (outCoords[0] * (subUVs[2]-subUVs[0]));
			outCoords[1] = subUVs[1] + (outCoords[1] * (subUVs[3]-subUVs[1]));

			outVerts += 3;
			outNormals += 3;
			outCoords += 2;
		}

		totalVerts += fr->numVerts;
	}

	// Calculate radius
	d->poly.radius = RadiusFromBounds (mins, maxs);
	assert (d->poly.radius);
	return true;
}


/*
===============
R_FreeDecal

Releases decal memory for index and vertex data.
===============
*/
bool R_FreeDecal (refDecal_t *d)
{
	if (!d || !d->poly.vertices)
		return false;

	Mem_Free (d->poly.vertices);
	d->poly.vertices = NULL;
	return true;
}
