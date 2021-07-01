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
// cm_q3_main.c
// Quake3 BSP map model loading
// FIXME TODO:
// - A lot of these allocations can be better sized (entity string for example).
//

#include "cm_q3_local.h"

static byte				*cm_q3_mapBuffer;

static int				cm_q3_numEntityChars;
static char				cm_q3_emptyEntityString[1];
static char				*cm_q3_entityString = cm_q3_emptyEntityString;

int						cm_q3_numBrushSides;
cmQ3BspBrushSide_t		*cm_q3_brushSides;

int						cm_q3_numMaterialRefs;
cmBspSurface_t			*cm_q3_surfaces;

int						cm_q3_numPlanes;
plane_t					*cm_q3_planes;

int						cm_q3_numNodes;
cmQ3BspNode_t			*cm_q3_nodes;

int						cm_q3_numLeafs = 1;	// allow leaf funcs to be called without a map
cmQ3BspLeaf_t			*cm_q3_leafs;

int						cm_q3_numLeafBrushes;
int						*cm_q3_leafBrushes;

int						cm_q3_numBrushes;
cmQ3BspBrush_t			*cm_q3_brushes;

static int				cm_q3_numVisibility;
dQ3BspVis_t				*cm_q3_visData;
dQ3BspVis_t				*cm_q3_hearData;

byte					*cm_q3_nullRow;

// each area has a list of portals that lead into other areas
// when portals are closed, other areas may not be visible or
// hearable even if the vis info says that it should be
int						cm_q3_numAreaPortals = 1;
cmQ3BspAreaPortal_t		*cm_q3_areaPortals;

int						cm_q3_numAreas = 1;
cmQ3BspArea_t			*cm_q3_areas;

cmBspSurface_t			cm_q3_nullSurface;

int						cm_q3_emptyLeaf;

int						cm_q3_numPatches;
cmQ3BspPatch_t			*cm_q3_patches;

int						cm_q3_numLeafPatches;
int						*cm_q3_leafPatches;

// These are only used during load
static int				cm_q3_numVertexes;
static vec4_t			*cm_q3_mapVerts;

static int				cm_q3_numFaces;
static cmQ3BspFace_t	*cm_q3_mapFaces;

static int				cm_q3_numLeafFaces;
static int				*cm_q3_leafFaces;

#include "LinearMath/btGeometryUtil.h"

void RecurseBuildLeaves (int node, std::vector<int> &leafs)
{
	if (node < 0)
		node = -node;

	var *nodeStr = &cm_q3_nodes[node];

	for (int i = 0; i < 2; ++i)
	{
		if (nodeStr->children[i] < 0)
			leafs.push_back(-1-nodeStr->children[i]);
		else
			RecurseBuildLeaves(nodeStr->children[i], leafs);
	}
}

void R_GetBModelVertices_q3 (int index, void (*receiveVertice) (TList<btVector3> vertices))
{
	// build a list of leafs
	std::vector<int> leafs;

	RecurseBuildLeaves(cm_mapCModels[index].headNode, leafs);

	std::vector<cmQ3BspBrush_t*> brushesAdded;
	for (size_t i = 0; i < leafs.size(); i++)
	{
		bool isValidBrush = false;

		var &leaf = cm_q3_leafs[leafs[i]];

		for (int b=0;b<leaf.numLeafBrushes;b++)
		{
			btAlignedObjectArray<btVector3> planeEquations;

			int brushid = cm_q3_leafBrushes[leaf.firstLeafBrush+b];

			var *brush = &cm_q3_brushes[brushid];

			bool has = false;
			for (size_t x = 0; x < brushesAdded.size(); ++x)
			{
				if (brushesAdded[x] == brush)
				{
					has = true;
					break;
				}
			}

			if (!has)
			{
				if (brush->contents & (CONTENTS_SOLID|CONTENTS_WINDOW))
				{
					brushesAdded.push_back(brush);

					for (int p=0;p<brush->numSides;p++)
					{
						int sideid = brush->firstBrushSide+p;
						var& brushside = cm_q3_brushSides[sideid];
						var plane = brushside.plane;
						btVector3 planeEq;
						planeEq.setValue(
							plane->normal[0],
							plane->normal[1],
							plane->normal[2]);
						planeEq[3] = -plane->dist;

						planeEquations.push_back(planeEq);
						isValidBrush=true;
					}
					if (isValidBrush)
					{

						btAlignedObjectArray<btVector3>	vertices;
						btGeometryUtil::getVerticesFromPlaneEquations(planeEquations,vertices);

						bool isEntity = false;
						btVector3 entityTarget(0.f,0.f,0.f);

						TList<btVector3> realVerts;

						for (int x = 0; x < vertices.size(); ++x)
							realVerts.Add(vertices.at(x));

						receiveVertice(realVerts);
					}
				}
			} 
		}
	}

	if (index == 0)
	{
		for (int i = 0; i < cm_q3_numPatches; ++i)
		{
			var *patch = &cm_q3_patches[i];

			for (int b = 0; b < patch->numBrushes; ++b)
			{
				var *brush = &patch->brushes[b];
				btAlignedObjectArray<btVector3> planeEquations;

				bool has = false;
				for (size_t x = 0; x < brushesAdded.size(); ++x)
				{
					if (brushesAdded[x] == brush)
					{
						has = true;
						break;
					}
				}

				if (!has)
				{
					bool isValidBrush = false;
					if (brush->contents & (CONTENTS_SOLID|CONTENTS_WINDOW))
					{
						brushesAdded.push_back(brush);

						for (int p=0;p<brush->numSides;p++)
						{
							int sideid = brush->firstBrushSide+p;
							var& brushside = cm_q3_brushSides[sideid];
							var plane = brushside.plane;
							btVector3 planeEq;
							planeEq.setValue(
								plane->normal[0],
								plane->normal[1],
								plane->normal[2]);
							planeEq[3] = -plane->dist;

							planeEquations.push_back(planeEq);
							isValidBrush=true;
						}
						if (isValidBrush)
						{

							btAlignedObjectArray<btVector3>	vertices;
							btGeometryUtil::getVerticesFromPlaneEquations(planeEquations,vertices);

							bool isEntity = false;
							btVector3 entityTarget(0.f,0.f,0.f);

							TList<btVector3> realVerts;

							for (int x = 0; x < vertices.size(); ++x)
								realVerts.Add(vertices[x]);

							receiveVertice(realVerts);
						}
					}
				} 
			}
		}
	}
}

/*
=============================================================================

	PATCH LOADING

=============================================================================
*/

/*
===============
Patch_FlatnessTest2
===============
*/
static int Patch_FlatnessTest2 (float maxflat, vec4_t point0, vec4_t point1, vec4_t point2)
{
	vec3_t	v1, v2, v3;
	vec3_t	t, n;
	float	dist, d, l;
	int		ft0, ft1;

	Vec3Subtract (point2, point0, n);
	l = VectorNormalizef (n, n);

	if (!l)
		return 0;

	Vec3Subtract (point1, point0, t);
	d = -DotProduct (t, n);
	Vec3MA (t, d, n, t);
	dist = Vec3Length (t);

	if (fabs(dist) <= maxflat)
		return 0;

	Vec3Average (point1, point0, v1);
	Vec3Average (point2, point1, v2);
	Vec3Average (v1, v2, v3);

	ft0 = Patch_FlatnessTest2 (maxflat, point0, v1, v3);
	ft1 = Patch_FlatnessTest2 (maxflat, v3, v2, point2);

	return 1 + (int)floor(max (ft0, ft1) + 0.5f);
}


/*
===============
Patch_GetFlatness2
===============
*/
static void Patch_GetFlatness2 (float maxflat, vec4_t *points, int *patch_cp, int *flat)
{
	int	i, p, u, v;

	flat[0] = flat[1] = 0;
	for (v=0 ; v<patch_cp[1]-1 ; v+=2) {
		for (u=0 ; u<patch_cp[0]-1 ; u+=2) {
			p = v * patch_cp[0] + u;

			i = Patch_FlatnessTest2 (maxflat, points[p], points[p+1], points[p+2]);
			flat[0] = max (flat[0], i);
			i = Patch_FlatnessTest2 (maxflat, points[p+patch_cp[0]], points[p+patch_cp[0]+1], points[p+patch_cp[0]+2]);
			flat[0] = max (flat[0], i);
			i = Patch_FlatnessTest2 (maxflat, points[p+2*patch_cp[0]], points[p+2*patch_cp[0]+1], points[p+2*patch_cp[0]+2]);
			flat[0] = max (flat[0], i);

			i = Patch_FlatnessTest2 (maxflat, points[p], points[p+patch_cp[0]], points[p+2*patch_cp[0]]);
			flat[1] = max (flat[1], i);
			i = Patch_FlatnessTest2 (maxflat, points[p+1], points[p+patch_cp[0]+1], points[p+2*patch_cp[0]+1]);
			flat[1] = max (flat[1], i);
			i = Patch_FlatnessTest2 (maxflat, points[p+2], points[p+patch_cp[0]+2], points[p+2*patch_cp[0]+2]);
			flat[1] = max (flat[1], i);
		}
	}
}


/*
===============
Patch_Evaluate_QuadricBezier2
===============
*/
static void Patch_Evaluate_QuadricBezier2 (float t, vec4_t point0, vec4_t point1, vec3_t point2, vec4_t out)
{
	float	qt = t * t;
	float	dt = 2.0f * t, tt;
	vec4_t	tvec4;

	tt = 1.0f - dt + qt;
	Vec4Scale (point0, tt, out);

	tt = dt - 2.0f * qt;
	Vec4Scale (point1, tt, tvec4);
	Vec4Add (out, tvec4, out);

	Vec4Scale (point2, qt, tvec4);
	Vec4Add (out, tvec4, out);
}


/*
===============
Patch_Evaluate2
===============
*/
static void Patch_Evaluate2 (vec4_t *p, int *numcp, int *tess, vec4_t *dest)
{
	int		num_patches[2], num_tess[2];
	int		index[3], dstpitch, i, u, v, x, y;
	float	s, t, step[2];
	vec4_t	*tvec, pv[3][3], v1, v2, v3;

	num_patches[0] = numcp[0] / 2;
	num_patches[1] = numcp[1] / 2;
	dstpitch = num_patches[0] * tess[0] + 1;

	step[0] = 1.0f / (float)tess[0];
	step[1] = 1.0f / (float)tess[1];

	for (v=0 ; v<num_patches[1] ; v++) {
		// Last patch has one more row 
		if (v < num_patches[1]-1) {
			num_tess[1] = tess[1];
		}
		else {
			num_tess[1] = tess[1] + 1;
		}

		for (u=0 ; u<num_patches[0] ; u++) {
			// Last patch has one more column
			if (u < num_patches[0] - 1) {
				num_tess[0] = tess[0];
			}
			else {
				num_tess[0] = tess[0] + 1;
			}

			index[0] = (v * numcp[0] + u) * 2;
			index[1] = index[0] + numcp[0];
			index[2] = index[1] + numcp[0];

			// Current 3x3 patch control points
			for (i=0 ; i<3 ; i++) {
				Vec4Copy (p[index[0]+i], pv[i][0]);
				Vec4Copy (p[index[1]+i], pv[i][1]);
				Vec4Copy (p[index[2]+i], pv[i][2]);
			}
			
			t = 0.0f;
			tvec = dest + v * tess[1] * dstpitch + u * tess[0];

			for (y=0 ; y<num_tess[1] ; y++, t+=step[1]) {
				Patch_Evaluate_QuadricBezier2 (t, pv[0][0], pv[0][1], pv[0][2], v1);
				Patch_Evaluate_QuadricBezier2 (t, pv[1][0], pv[1][1], pv[1][2], v2);
				Patch_Evaluate_QuadricBezier2 (t, pv[2][0], pv[2][1], pv[2][2], v3);

				s = 0.0f;
				for (x=0 ; x<num_tess[0] ; x++, s+=step[0])
					Patch_Evaluate_QuadricBezier2 (s, v1, v2, v3, tvec[x]);

				tvec += dstpitch;
			}
		}
	}
}


/*
===============
CM_Q3BSP_CreateBrush
===============
*/
static void CM_Q3BSP_CreateBrush (cmQ3BspBrush_t *brush, vec3_t *verts, cmBspSurface_t *surface)
{
	static plane_t	mainplane, patchplanes[20];
	int				i, j, k, sign;
	vec3_t			v1, v2;
	vec3_t			absMins, absMaxs;
	cmQ3BspBrushSide_t *side;
	plane_t			*plane;
	bool			skip[20];
	int				numPatchPlanes = 0;
	vec3_t			normal;

	// Calc absMins & absMaxs
	ClearBounds (absMins, absMaxs);
	for (i=0 ; i<3 ; i++)
		AddPointToBounds (verts[i], absMins, absMaxs);

	PlaneFromPoints (verts, &mainplane);

	// Front plane
	plane = &patchplanes[numPatchPlanes++];
	*plane = mainplane;

	// Back plane
	plane = &patchplanes[numPatchPlanes++];
	Vec3Negate (mainplane.normal, plane->normal);
	plane->dist = -mainplane.dist;

	// Axial planes
	for (i=0 ; i<3 ; i++) {
		for (sign=-1 ; sign<=1 ; sign+=2) {
			plane = &patchplanes[numPatchPlanes++];
			Vec3Clear (plane->normal);
			plane->normal[i] = sign;
			if (sign > 0)
				plane->dist = absMaxs[i];
			else
				plane->dist = -absMins[i];
		}
	}

	// Edge planes
	for (i=0 ; i<3 ; i++) {
		Vec3Copy (verts[i], v1);
		Vec3Copy (verts[(i + 1) % 3], v2);

		for (k=0 ; k<3 ; k++) {
			normal[k] = 0;
			normal[(k+1)%3] = v1[(k+2)%3] - v2[(k+2)%3];
			normal[(k+2)%3] = -(v1[(k+1)%3] - v2[(k+1)%3]);

			if (Vec3Compare (normal, vec3Origin))
				continue;

			plane = &patchplanes[numPatchPlanes++];

			VectorNormalizef ( normal, normal );
			Vec3Copy ( normal, plane->normal );
			plane->dist = DotProduct (plane->normal, v1);

			if (DotProduct(verts[(i+2)%3], normal) - plane->dist > 0) {
				// Invert
				Vec3Inverse (plane->normal);
				plane->dist = -plane->dist;
			}
		}
	}

	// Set plane->type and mark duplicate planes for removal
	for (i=0 ; i<numPatchPlanes ; i++) {
		CategorizePlane (&patchplanes[i]);
		skip[i] = false;

		for (j=i+1 ; j<numPatchPlanes ; j++) {
			if (patchplanes[j].dist == patchplanes[i].dist
			&& Vec3Compare (patchplanes[j].normal, patchplanes[i].normal)) {
				skip[i] = true;
				break;
			}
		}
	}

	brush->numSides = 0;
	brush->firstBrushSide = cm_q3_numBrushSides;

	for (k=0 ; k<2 ; k++) {
		for (i=0 ; i<numPatchPlanes ; i++) {
			if (skip[i])
				continue;

			// first, store all axially aligned planes
			// then store everything else
			// does it give a noticeable speedup?
			if (!k && patchplanes[i].type >= 3)
				continue;

			skip[i] = true;

			if (cm_q3_numPlanes == MAX_Q3BSP_CM_PLANES)
				Com_Error (ERR_DROP, "CM_Q3BSP_CreateBrush: cm_q3_numPlanes == MAX_Q3BSP_CM_PLANES");

			plane = &cm_q3_planes[cm_q3_numPlanes++];
			*plane = patchplanes[i];

			if (cm_q3_numBrushSides == MAX_Q3BSP_CM_BRUSHSIDES)
				Com_Error (ERR_DROP, "CM_Q3BSP_CreateBrush: cm_q3_numBrushSides == MAX_Q3BSP_CM_BRUSHSIDES");

			side = &cm_q3_brushSides[cm_q3_numBrushSides++];
			side->plane = plane;

			if (DotProduct(plane->normal, mainplane.normal) >= 0)
				side->surface = surface;
			else
				side->surface = NULL;	// don't clip against this side

			brush->numSides++;
		}
	}
}


/*
===============
CM_Q3BSP_CreatePatch
===============
*/
static void CM_Q3BSP_CreatePatch (cmQ3BspPatch_t *patch, int numverts, vec4_t *verts, int *patch_cp)
{
    int			step[2], size[2], flat[2], i, u, v;
	vec4_t		points[MAX_Q3BSP_CM_PATCH_VERTS];
	vec3_t		tverts[4], tverts2[4];
	cmQ3BspBrush_t	*brush;
	plane_t		mainplane;

	// Find the degree of subdivision in the u and v directions
	Patch_GetFlatness2 (CM_SUBDIVLEVEL, verts, patch_cp, flat);

	step[0] = (1 << flat[0]);
	step[1] = (1 << flat[1]);
	size[0] = (patch_cp[0] / 2) * step[0] + 1;
	size[1] = (patch_cp[1] / 2) * step[1] + 1;

	if (size[0]*size[1] > MAX_Q3BSP_CM_PATCH_VERTS) {
		Com_Error (ERR_DROP, "CM_Q3BSP_CreatePatch: patch has too many vertices");
		return;
	}

	// Fill in
	Patch_Evaluate2 (verts, patch_cp, step, points);

	patch->brushes = brush = cm_q3_brushes + cm_q3_numBrushes;
	patch->numBrushes = 0;

	ClearBounds (patch->absMins, patch->absMaxs);

	// Create a set of brushes
    for (v=0 ; v<size[1]-1 ; v++) {
		for (u=0 ; u<size[0]-1 ; u++) {
			if (cm_q3_numBrushes >= MAX_Q3BSP_CM_BRUSHES)
				Com_Error (ERR_DROP, "CM_Q3BSP_CreatePatch: too many patch brushes");

			i = v * size[0] + u;
			Vec3Copy (points[i], tverts[0]);
			Vec3Copy (points[i + size[0]], tverts[1]);
			Vec3Copy (points[i + 1], tverts[2]);
			Vec3Copy (points[i + size[0] + 1], tverts[3]);

			// Add to bounds
			AddPointToBounds (tverts[0], patch->absMins, patch->absMaxs);
			AddPointToBounds (tverts[1], patch->absMins, patch->absMaxs);
			AddPointToBounds (tverts[2], patch->absMins, patch->absMaxs);
			AddPointToBounds (tverts[3], patch->absMins, patch->absMaxs);

			PlaneFromPoints (tverts, &mainplane);

			// Create two brushes
			CM_Q3BSP_CreateBrush (brush, tverts, patch->surface);

			brush->contents = patch->surface->contents;
			brush++;
			cm_q3_numBrushes++;
			patch->numBrushes++;

			Vec3Copy (tverts[2], tverts2[0]);
			Vec3Copy (tverts[1], tverts2[1]);
			Vec3Copy (tverts[3], tverts2[2]);
			CM_Q3BSP_CreateBrush (brush, tverts2, patch->surface);

			brush->contents = patch->surface->contents;
			brush++;
			cm_q3_numBrushes++;
			patch->numBrushes++;
		}
    }
}

// ==========================================================================

/*
=================
CM_Q3BSP_CreatePatchesForLeafs
=================
*/
static void CM_Q3BSP_CreatePatchesForLeafs ()
{
	int				i, j, k;
	cmQ3BspLeaf_t	*leaf;
	cmQ3BspFace_t	*face;
	cmBspSurface_t	*surf;
	cmQ3BspPatch_t	*patch;
	int				checkout[MAX_Q3BSP_CM_FACES];

	memset (checkout, -1, sizeof(int)*MAX_Q3BSP_CM_FACES);

	for (i=0, leaf=cm_q3_leafs ; i<cm_q3_numLeafs ; i++, leaf++) {
		leaf->numLeafPatches = 0;
		leaf->firstLeafPatch = cm_q3_numLeafPatches;

		if (leaf->cluster == -1)
			continue;

		for (j=0 ; j<leaf->numLeafFaces ; j++) {
			k = leaf->firstLeafFace + j;
			if (k >= cm_q3_numLeafFaces)
				break;

			k = cm_q3_leafFaces[k];
			face = &cm_q3_mapFaces[k];

			if (face->faceType != FACETYPE_PATCH || face->numVerts <= 0)
				continue;
			if (face->patch_cp[0] <= 0 || face->patch_cp[1] <= 0)
				continue;
			if (face->matNum < 0 || face->matNum >= cm_q3_numMaterialRefs)
				continue;

			surf = &cm_q3_surfaces[face->matNum];
			if (!surf->contents || surf->flags & SHREF_NONSOLID)
				continue;

			if (cm_q3_numLeafPatches >= MAX_Q3BSP_CM_LEAFFACES)
				Com_Error (ERR_DROP, "CM_Q3BSP_CreatePatchesForLeafs: map has too many faces");

			// The patch was already built
			if (checkout[k] != -1) {
				cm_q3_leafPatches[cm_q3_numLeafPatches] = checkout[k];
				patch = &cm_q3_patches[checkout[k]];
			}
			else {
				if (cm_q3_numPatches >= MAX_Q3BSP_CM_PATCHES)
					Com_Error (ERR_DROP, "CM_Q3BSP_CreatePatchesForLeafs: map has too many patches");

				patch = &cm_q3_patches[cm_q3_numPatches];
				patch->surface = surf;
				cm_q3_leafPatches[cm_q3_numLeafPatches] = cm_q3_numPatches;
				checkout[k] = cm_q3_numPatches++;

				CM_Q3BSP_CreatePatch (patch, face->numVerts, cm_q3_mapVerts + face->firstVert, face->patch_cp);
			}

			leaf->contents |= patch->surface->contents;
			leaf->numLeafPatches++;

			cm_q3_numLeafPatches++;
		}
	}
}

/*
=============================================================================

	QUAKE3 BSP LOADING

=============================================================================
*/

/*
=================
CM_Q3BSP_LoadVertexes
=================
*/
static void CM_Q3BSP_LoadVertexes (dQ3BspLump_t *l)
{
	dQ3BspVertex_t	*in;
	vec4_t			*out;
	int				i;

	// Check lump size
	in = (dQ3BspVertex_t *)((byte*)(cm_q3_mapBuffer + l->fileOfs));
	if (l->fileLen % sizeof(*in))
		Com_Error (ERR_DROP, "CM_Q3BSP_LoadVertexes: funny lump size");

	// Check data size
	cm_q3_numVertexes = l->fileLen / sizeof(*in);
	if (cm_q3_numVertexes > MAX_Q3BSP_CM_VERTEXES)
		Com_Error (ERR_DROP, "CM_Q3BSP_LoadVertexes: Map has too many vertexes");
	cm_q3_mapVerts = out = (vec4_t*)Mem_Alloc (cm_q3_numVertexes * sizeof(*out));

	// Byte swap
	for (i=0 ; i<cm_q3_numVertexes ; i++, in++) {
		out[i][0] = LittleFloat (in->point[0]);
		out[i][1] = LittleFloat (in->point[1]);
		out[i][2] = LittleFloat (in->point[2]);
	}
}


/*
=================
CM_Q3BSP_LoadFaces
=================
*/
static void CM_Q3BSP_LoadFaces (dQ3BspLump_t *l)
{
	dQ3BspFace_t	*in;
	cmQ3BspFace_t	*out;
	int				i;

	// Check lump size
	in = (dQ3BspFace_t *)((byte*)(cm_q3_mapBuffer + l->fileOfs));
	if (l->fileLen % sizeof(*in))
		Com_Error (ERR_DROP, "CM_Q3BSP_LoadFaces: funny lump size");

	// Check data size
	cm_q3_numFaces = l->fileLen / sizeof(*in);
	if (cm_q3_numFaces > MAX_Q3BSP_CM_FACES)
		Com_Error (ERR_DROP, "CM_Q3BSP_LoadFaces: Map has too many faces");
	cm_q3_mapFaces = out = (cmQ3BspFace_t*)Mem_Alloc (cm_q3_numFaces * sizeof(*out));

	// Byte swap
	for (i=0 ; i<cm_q3_numFaces ; i++, in++, out++) {
		out->faceType = LittleLong (in->faceType);
		out->matNum = LittleLong (in->matNum);

		out->numVerts = LittleLong (in->numVerts);
		out->firstVert = LittleLong (in->firstVert);

		out->patch_cp[0] = LittleLong (in->patch_cp[0]);
		out->patch_cp[1] = LittleLong (in->patch_cp[1]);
	}
}


/*
=================
CM_Q3BSP_LoadLeafFaces
=================
*/
static void CM_Q3BSP_LoadLeafFaces (dQ3BspLump_t *l)
{
	int		i, j;
	int		*in;
	int		*out;

	// Check lump size
	in = (int *)((byte*)(cm_q3_mapBuffer + l->fileOfs));
	if (l->fileLen % sizeof(*in))
		Com_Error (ERR_DROP, "CM_Q3BSP_LoadLeafFaces: funny lump size");

	// Check data size
	cm_q3_numLeafFaces = l->fileLen / sizeof(*in);
	if (cm_q3_numLeafFaces > MAX_Q3BSP_CM_LEAFFACES) 
		Com_Error (ERR_DROP, "CM_Q3BSP_LoadLeafFaces: Map has too many leaffaces"); 
	cm_q3_leafFaces = out = (int*)Mem_Alloc (cm_q3_numLeafFaces*sizeof(*out));

	// Byte swap
	for (i=0 ; i<cm_q3_numLeafFaces ; i++) {
		j = LittleLong (in[i]);
		if (j < 0 ||  j >= cm_q3_numFaces)
			Com_Error (ERR_DROP, "CMod_LoadLeafFaces: bad surface number");

		out[i] = j;
	}
}


/*
=================
CM_Q3BSP_LoadSubmodels
=================
*/
static void CM_Q3BSP_LoadSubmodels (dQ3BspLump_t *l)
{
	dQ3BspModel_t	*in;
	cmBspModel_t	*out;
	cmQ3BspLeaf_t	*bleaf;
	int				*leafbrush;
	int				i, j;

	// Check lump size
	in = (dQ3BspModel_t *)((byte*)(cm_q3_mapBuffer + l->fileOfs));
	if (l->fileLen % sizeof(*in))
		Com_Error (ERR_DROP, "CM_Q3BSP_LoadSubmodels: funny lump size");

	// Check data size
	cm_numCModels = l->fileLen / sizeof(*in);
	if (cm_numCModels < 1)
		Com_Error (ERR_DROP, "CM_Q3BSP_LoadSubmodels: Map with no models");
	else if (cm_numCModels > MAX_Q3BSP_CM_MODELS)
		Com_Error (ERR_DROP, "CM_Q3BSP_LoadSubmodels: Map has too many models");

	// Byte swap
	for (i=0 ; i<cm_numCModels ; i++, in++, out++) {
		out = &cm_mapCModels[i];

		if (!i) {
			out->headNode = 0;
		}
		else {
			out->headNode = -1 - cm_q3_numLeafs;

			bleaf = &cm_q3_leafs[cm_q3_numLeafs++];
			bleaf->numLeafBrushes = LittleLong (in->numBrushes);
			bleaf->firstLeafBrush = cm_q3_numLeafBrushes;
			bleaf->contents = 0;

			leafbrush = &cm_q3_leafBrushes[cm_q3_numLeafBrushes];
			for (j=0 ; j<bleaf->numLeafBrushes ; j++, leafbrush++) {
				*leafbrush = LittleLong (in->firstBrush) + j;
				bleaf->contents |= cm_q3_brushes[*leafbrush].contents;
			}

			cm_q3_numLeafBrushes += bleaf->numLeafBrushes;
		}

		// Spread the mins / maxs by a pixel
		for (j=0 ; j<3 ; j++) {
			out->mins[j] = LittleFloat (in->mins[j]) - 1;
			out->maxs[j] = LittleFloat (in->maxs[j]) + 1;
		}
	}
}


/*
=================
CM_Q3BSP_LoadSurfaces
=================
*/
static void CM_Q3BSP_LoadSurfaces (dQ3BspLump_t *l)
{
	dQ3BspMaterialRef_t	*in;
	cmBspSurface_t		*out;
	int					i;

	// Sanity check lump size
	in = (dQ3BspMaterialRef_t *)((byte*)(cm_q3_mapBuffer + l->fileOfs));
	if (l->fileLen % sizeof(*in))
		Com_Error (ERR_DROP, "CM_Q3BSP_LoadSurfaces: funny lump size");

	// Find the size and allocate
	cm_q3_numMaterialRefs = l->fileLen / sizeof(*in);
	if (cm_q3_numMaterialRefs < 1)
		Com_Error (ERR_DROP, "CM_Q3BSP_LoadSurfaces: Map with no materials");
	else if (cm_q3_numMaterialRefs > MAX_Q3BSP_CM_MATERIALS)
		Com_Error (ERR_DROP, "CM_Q3BSP_LoadSurfaces: Map has too many materials");
	cm_q3_surfaces = (cmBspSurface_t*)Mem_PoolAlloc (sizeof(cmBspSurface_t) * cm_q3_numMaterialRefs, com_cmodelSysPool, 0);

	// Byte swap
	out = cm_q3_surfaces;
	for (i=0 ; i<cm_q3_numMaterialRefs ; i++, in++, out++) {
		out->flags = LittleLong (in->flags);
		out->contents = LittleLong (in->contents);
	}
}


/*
=================
CM_Q3BSP_LoadNodes
=================
*/
static void CM_Q3BSP_LoadNodes (dQ3BspLump_t *l)
{
	dQ3BspNode_t	*in;
	int				child;
	cmQ3BspNode_t	*out;
	int				i, j;

	// Sanity check lump size
	in = (dQ3BspNode_t *)((byte*)(cm_q3_mapBuffer + l->fileOfs));
	if (l->fileLen % sizeof(*in))
		Com_Error (ERR_DROP, "CM_Q3BSP_LoadNodes: funny lump size");

	// Find the size and allocate (with extra for box hull)
	cm_q3_numNodes = l->fileLen / sizeof(*in);
	if (cm_q3_numNodes < 1)
		Com_Error (ERR_DROP, "CM_Q3BSP_LoadNodes: Map has no nodes");
	else if (cm_q3_numNodes > MAX_Q3BSP_CM_NODES)
		Com_Error (ERR_DROP, "CM_Q3BSP_LoadNodes: Map has too many nodes");
	cm_q3_nodes = (cmQ3BspNode_t*)Mem_PoolAlloc (sizeof(cmQ3BspNode_t) * (cm_q3_numNodes+6), com_cmodelSysPool, 0);

	// Byte swap
	out = cm_q3_nodes;
	for (i=0 ; i<cm_q3_numNodes ; i++, out++, in++) {
		out->plane = cm_q3_planes + LittleLong (in->planeNum);
		for (j=0 ; j<2 ; j++) {
			child = LittleLong (in->children[j]);
			out->children[j] = child;
		}
	}
}


/*
=================
CM_Q3BSP_LoadBrushes
=================
*/
static void CM_Q3BSP_LoadBrushes (dQ3BspLump_t *l)
{
	dQ3BspBrush_t	*in;
	cmQ3BspBrush_t	*out;
	int				i, matRef;

	// Check lump size
	in = (dQ3BspBrush_t *)((byte*)(cm_q3_mapBuffer + l->fileOfs));
	if (l->fileLen % sizeof(*in))
		Com_Error (ERR_DROP, "CM_Q3BSP_LoadBrushes: funny lump size");

	// Check data size
	cm_q3_numBrushes = l->fileLen / sizeof(*in);
	if (cm_q3_numBrushes > MAX_Q3BSP_CM_BRUSHES)
		Com_Error (ERR_DROP, "CM_Q3BSP_LoadBrushes: Map has too many brushes");

	// Byte swap
	out = cm_q3_brushes;
	for (i=0 ; i<cm_q3_numBrushes ; i++, out++, in++) {
		matRef = LittleLong (in->matNum);
		out->contents = cm_q3_surfaces[matRef].contents;
		out->firstBrushSide = LittleLong (in->firstSide);
		out->numSides = LittleLong (in->numSides);
	}
}


/*
=================
CM_Q3BSP_LoadLeafs
=================
*/
static void CM_Q3BSP_LoadLeafs (dQ3BspLump_t *l)
{
	int				i, j;
	cmQ3BspLeaf_t	*out;
	dQ3BspLeaf_t 	*in;
	cmQ3BspBrush_t	*brush;

	// Check lump size
	in = (dQ3BspLeaf_t *)((byte*)(cm_q3_mapBuffer + l->fileOfs));
	if (l->fileLen % sizeof(*in))
		Com_Error (ERR_DROP, "CM_Q3BSP_LoadLeafs: funny lump size");

	// Check data size
	cm_q3_numLeafs = l->fileLen / sizeof(*in);
	if (cm_q3_numLeafs < 1)
		Com_Error (ERR_DROP, "CM_Q3BSP_LoadLeafs: Map with no leafs");
	else if (cm_q3_numLeafs > MAX_Q3BSP_CM_LEAFS)
		Com_Error (ERR_DROP, "CM_Q3BSP_LoadLeafs: Map has too many leafs");

	// Byte swap
	out = cm_q3_leafs;
	cm_q3_emptyLeaf = -1;

	for (i=0 ; i<cm_q3_numLeafs ; i++, in++, out++) {
		out->cluster = LittleLong (in->cluster);
		out->area = LittleLong (in->area) + 1;
		out->firstLeafFace = LittleLong (in->firstLeafFace);
		out->numLeafFaces = LittleLong (in->numLeafFaces);
		out->contents = 0;
		out->firstLeafBrush = LittleLong (in->firstLeafBrush);
		out->numLeafBrushes = LittleLong (in->numLeafBrushes);

		for (j=0 ; j<out->numLeafBrushes ; j++) {
			brush = &cm_q3_brushes[cm_q3_leafBrushes[out->firstLeafBrush+j]];
			out->contents |= brush->contents;
		}

		if (out->area >= cm_q3_numAreas)
			cm_q3_numAreas = out->area + 1;

		if (!out->contents)
			cm_q3_emptyLeaf = i;
	}

	// If map doesn't have an empty leaf - force one
	if (cm_q3_emptyLeaf == -1) {
		if (cm_q3_numLeafs >= MAX_Q3BSP_CM_LEAFS-1)
			Com_Error (ERR_DROP, "CM_Q3BSP_LoadLeafs: Map does not have an empty leaf");

		out->cluster = -1;
		out->area = -1;
		out->numLeafBrushes = 0;
		out->contents = 0;
		out->firstLeafBrush = 0;

		Com_DevPrintf (0, "CM_Q3BSP_LoadLeafs: Forcing an empty leaf: %i\n", cm_q3_numLeafs);
		cm_q3_emptyLeaf = cm_q3_numLeafs++;
	}
}


/*
=================
CM_Q3BSP_LoadPlanes
=================
*/
static void CM_Q3BSP_LoadPlanes (dQ3BspLump_t *l)
{
	int				i;
	plane_t			*out;
	dQ3BspPlane_t 	*in;

	// Sanity check lump size
	in = (dQ3BspPlane_t *)((byte*)(cm_q3_mapBuffer + l->fileOfs));
	if (l->fileLen % sizeof(*in))
		Com_Error (ERR_DROP, "CM_Q3BSP_LoadPlanes: funny lump size");

	// Find the size and allocate (with extra for box hull)
	cm_q3_numPlanes = l->fileLen / sizeof(*in);
	if (cm_q3_numPlanes < 1)
		Com_Error (ERR_DROP, "CM_Q3BSP_LoadPlanes: Map with no planes");
	else if (cm_q3_numPlanes > MAX_Q3BSP_CM_PLANES)
		Com_Error (ERR_DROP, "CM_Q3BSP_LoadPlanes: Map has too many planes");
	cm_q3_planes = (plane_t*)Mem_PoolAlloc (sizeof(plane_t) * (MAX_Q3BSP_CM_PLANES+12), com_cmodelSysPool, 0);	// FIXME

	// Byte swap
	out = cm_q3_planes;
	for (i=0 ; i<cm_q3_numPlanes ; i++, in++, out++) {
		out->normal[0] = LittleFloat (in->normal[0]);
		out->normal[1] = LittleFloat (in->normal[1]);
		out->normal[2] = LittleFloat (in->normal[2]);

		out->dist = LittleFloat (in->dist);

		CategorizePlane (out);
	}
}


/*
=================
CM_Q3BSP_LoadLeafBrushes
=================
*/
static void CM_Q3BSP_LoadLeafBrushes (dQ3BspLump_t *l)
{
	int			i;
	int			*out;
	int		 	*in;

	// Check lump size
	in = (int *)((byte*)(cm_q3_mapBuffer + l->fileOfs));
	if (l->fileLen % sizeof(*in))
		Com_Error (ERR_DROP, "CM_Q3BSP_LoadLeafBrushes: funny lump size");

	// Check data size
	cm_q3_numLeafBrushes = l->fileLen / sizeof(*in);
	if (cm_q3_numLeafBrushes < 1)
		Com_Error (ERR_DROP, "CM_Q3BSP_LoadLeafBrushes: Map with no planes");
	else if (cm_q3_numLeafBrushes > MAX_Q3BSP_CM_LEAFBRUSHES)
		Com_Error (ERR_DROP, "CM_Q3BSP_LoadLeafBrushes: Map has too many leafbrushes");

	// Byte swap
	out = cm_q3_leafBrushes;
	for ( i=0 ; i<cm_q3_numLeafBrushes ; i++, in++, out++)
		*out = LittleLong (*in);
}


/*
=================
CM_Q3BSP_LoadBrushSides
=================
*/
static void CM_Q3BSP_LoadBrushSides (dQ3BspLump_t *l)
{
	int					i, j;
	cmQ3BspBrushSide_t	*out;
	dQ3BspBrushSide_t 	*in;

	// Check lump size
	in = (dQ3BspBrushSide_t *)((byte*)(cm_q3_mapBuffer + l->fileOfs));
	if (l->fileLen % sizeof(*in))
		Com_Error (ERR_DROP, "CM_Q3BSP_LoadBrushSides: funny lump size");

	// Check data size
	cm_q3_numBrushSides = l->fileLen / sizeof(*in);
	if (cm_q3_numBrushSides > MAX_Q3BSP_CM_BRUSHSIDES)
		Com_Error (ERR_DROP, "CM_Q3BSP_LoadBrushSides: Map has too many brushSides");

	// Byte swap
	out = cm_q3_brushSides;
	for (i=0 ; i<cm_q3_numBrushSides ; i++, in++, out++) {
		out->plane = cm_q3_planes + LittleLong (in->planeNum);
		j = LittleLong (in->matNum);
		if (j >= cm_q3_numMaterialRefs)
			Com_Error (ERR_DROP, "CM_Q3BSP_LoadBrushSides: Bad brushside texinfo");
		out->surface = &cm_q3_surfaces[j];
	}
}


/*
=================
CM_Q3BSP_LoadVisibility
=================
*/
static void CM_Q3BSP_LoadVisibility (dQ3BspLump_t *l)
{
	// Check data size
	cm_q3_numVisibility = l->fileLen;
	if (l->fileLen > MAX_Q3BSP_CM_VISIBILITY)
		Com_Error (ERR_DROP, "CM_Q3BSP_LoadVisibility: Map has too large visibility lump");

	// Byte swap
	memcpy (cm_q3_visData, cm_q3_mapBuffer + l->fileOfs, l->fileLen);

	cm_q3_visData->numClusters = LittleLong (cm_q3_visData->numClusters);
	cm_q3_visData->rowSize = LittleLong (cm_q3_visData->rowSize);
}


/*
=================
CM_Q3BSP_LoadEntityString
=================
*/
static void CM_Q3BSP_LoadEntityString (dQ3BspLump_t *l)
{
	// Find the size and allocate (with extra for NULL termination)
	cm_q3_numEntityChars = l->fileLen;
	if (l->fileLen > MAX_Q3BSP_CM_ENTSTRING)
		Com_Error (ERR_DROP, "CM_Q3BSP_LoadEntityString: Map has too large entity lump");
	cm_q3_entityString = (char*)Mem_PoolAlloc (sizeof(char) * (cm_q3_numEntityChars+1), com_cmodelSysPool, 0);

	// Copy data
	memcpy (cm_q3_entityString, cm_q3_mapBuffer + l->fileOfs, l->fileLen);
}


/*
=================
CM_Q3BSP_CalcPHS
=================
*/
static void CM_Q3BSP_CalcPHS ()
{
	int		rowbytes, rowwords;
	int		i, j, k, l, index;
	int		bitbyte;
	uint32	*dest, *src;
	byte	*scan;
	int		count, vcount;
	int		numClusters;

	Com_DevPrintf (0, "CM_Q3BSP_CalcPHS: Building PHS...\n");

	rowwords = cm_q3_visData->rowSize / sizeof(long);
	rowbytes = cm_q3_visData->rowSize;

	memset (cm_q3_hearData, 0, MAX_Q3BSP_CM_VISIBILITY);

	cm_q3_hearData->rowSize = cm_q3_visData->rowSize;
	cm_q3_hearData->numClusters = numClusters = cm_q3_visData->numClusters;

	vcount = 0;
	for (i=0 ; i<numClusters ; i++) {
		scan = CM_Q3BSP_ClusterPVS (i);
		for (j=0 ; j<numClusters ; j++) {
			if (scan[j>>3] & (BIT(j&7)))
				vcount++;
		}
	}

	count = 0;
	scan = (byte *)cm_q3_visData->data;
	dest = (uint32 *)cm_q3_hearData->data;

	for (i=0 ; i<numClusters ; i++, dest += rowwords, scan += rowbytes) {
		memcpy (dest, scan, rowbytes);
		for (j=0 ; j<rowbytes ; j++) {
			bitbyte = scan[j];
			if (!bitbyte)
				continue;
			for (k=0 ; k<8 ; k++) {
				if (!(bitbyte & BIT(k)))
					continue;

				// OR this pvs row into the phs
				index = (j<<3) + k;
				if (index >= numClusters)
					Com_Error (ERR_DROP, "CM_Q3BSP_CalcPHS: Bad bit in PVS");	// pad bits should be 0
				src = (unsigned *)((byte*)cm_q3_visData->data) + index*rowwords;
				for (l=0 ; l<rowwords ; l++)
					dest[l] |= src[l];
			}
		}
		for (j=0 ; j<numClusters ; j++)
			if (((byte *)dest)[j>>3] & BIT(j&7))
				count++;
	}

	Com_DevPrintf (0, "CM_Q3BSP_CalcPHS: Average clusters visible / hearable / total: %i / %i / %i\n"
		, vcount ? vcount/numClusters : 0,
		count ? count/numClusters : 0, numClusters);
}

// ==========================================================================

/*
=================
CM_Q3BSP_LoadMap
=================
*/
cmBspModel_t *CM_Q3BSP_LoadMap (uint32 *buffer)
{
	dQ3BspHeader_t	header;
	int				i;

	//
	// Allocate space
	//
	cm_q3_areaPortals = (cmQ3BspAreaPortal_t*)Mem_PoolAlloc (sizeof(cmQ3BspAreaPortal_t) * MAX_Q3BSP_CM_AREAPORTALS, com_cmodelSysPool, 0);
	cm_q3_areas = (cmQ3BspArea_t*)Mem_PoolAlloc (sizeof(cmQ3BspArea_t) * MAX_Q3BSP_CM_AREAS, com_cmodelSysPool, 0);
	cm_q3_brushes = (cmQ3BspBrush_t*)Mem_PoolAlloc (sizeof(cmQ3BspBrush_t) * (MAX_Q3BSP_CM_BRUSHES+1), com_cmodelSysPool, 0); // extra for box hull
	cm_q3_brushSides = (cmQ3BspBrushSide_t*)Mem_PoolAlloc (sizeof(cmQ3BspBrushSide_t) * (MAX_Q3BSP_CM_BRUSHSIDES+6), com_cmodelSysPool, 0); // extra for box hull
	cm_q3_hearData = (dQ3BspVis_t*)Mem_PoolAlloc (sizeof(byte) * MAX_Q3BSP_CM_VISIBILITY, com_cmodelSysPool, 0);
	cm_q3_leafBrushes = (int*)Mem_PoolAlloc (sizeof(int) * (MAX_Q3BSP_CM_LEAFBRUSHES+1), com_cmodelSysPool, 0); // extra for box hull
	cm_q3_leafPatches = (int*)Mem_PoolAlloc (sizeof(int) * MAX_Q3BSP_CM_LEAFFACES, com_cmodelSysPool, 0);
	cm_q3_leafs = (cmQ3BspLeaf_t*)Mem_PoolAlloc (sizeof(cmQ3BspLeaf_t) * MAX_Q3BSP_CM_LEAFS, com_cmodelSysPool, 0);
	cm_q3_nullRow = (byte*)Mem_PoolAlloc (sizeof(byte) * (MAX_Q3BSP_CM_LEAFS / 8), com_cmodelSysPool, 0);
	cm_q3_patches = (cmQ3BspPatch_t*)Mem_PoolAlloc (sizeof(cmQ3BspPatch_t) * MAX_Q3BSP_CM_PATCHES, com_cmodelSysPool, 0);
	cm_q3_visData = (dQ3BspVis_t*)Mem_PoolAlloc (sizeof(byte) * MAX_Q3BSP_CM_VISIBILITY, com_cmodelSysPool, 0);

	// Default values
	memset (cm_q3_nullRow, 255, MAX_Q3BSP_CM_LEAFS / 8);

	//
	// Byte swap lumps
	//
	header = *(dQ3BspHeader_t *)buffer;
	for (i=0 ; i<sizeof(dQ3BspHeader_t)/4 ; i++)
		((int *)&header)[i] = LittleLong (((int *)&header)[i]);
	cm_q3_mapBuffer = (byte *)buffer;

	//
	// Load into heap
	//
	CM_Q3BSP_LoadSurfaces		(&header.lumps[Q3BSP_LUMP_MATERIALREFS]);
	CM_Q3BSP_LoadPlanes			(&header.lumps[Q3BSP_LUMP_PLANES]);
	CM_Q3BSP_LoadLeafBrushes	(&header.lumps[Q3BSP_LUMP_LEAFBRUSHES]);
	CM_Q3BSP_LoadBrushes		(&header.lumps[Q3BSP_LUMP_BRUSHES]);
	CM_Q3BSP_LoadBrushSides		(&header.lumps[Q3BSP_LUMP_BRUSHSIDES]);
	CM_Q3BSP_LoadVertexes		(&header.lumps[Q3BSP_LUMP_VERTEXES]);
	CM_Q3BSP_LoadFaces			(&header.lumps[Q3BSP_LUMP_FACES]);
	CM_Q3BSP_LoadLeafFaces		(&header.lumps[Q3BSP_LUMP_LEAFFACES]);
	CM_Q3BSP_LoadLeafs			(&header.lumps[Q3BSP_LUMP_LEAFS]);
	CM_Q3BSP_LoadNodes			(&header.lumps[Q3BSP_LUMP_NODES]);
	CM_Q3BSP_LoadSubmodels		(&header.lumps[Q3BSP_LUMP_MODELS]);
	CM_Q3BSP_LoadVisibility		(&header.lumps[Q3BSP_LUMP_VISIBILITY]);
	CM_Q3BSP_LoadEntityString	(&header.lumps[Q3BSP_LUMP_ENTITIES]);

	CM_Q3BSP_CreatePatchesForLeafs ();

	CM_Q3BSP_InitBoxHull ();
	CM_Q3BSP_PrepMap ();

	CM_Q3BSP_CalcPHS ();

	if (cm_q3_mapVerts)
		Mem_Free (cm_q3_mapVerts);
	if (cm_q3_mapFaces)
		Mem_Free (cm_q3_mapFaces);
	if (cm_q3_leafFaces)
		Mem_Free (cm_q3_leafFaces);

	return &cm_mapCModels[0];
}


/*
==================
CM_Q3BSP_PrepMap
==================
*/
void CM_Q3BSP_PrepMap ()
{
	CM_Q3BSP_FloodAreaConnections ();
}


/*
==================
CM_Q3BSP_UnloadMap
==================
*/
void CM_Q3BSP_UnloadMap ()
{
	cm_q3_areaPortals = NULL;
	cm_q3_areas = NULL;
	cm_q3_brushes = NULL;
	cm_q3_brushSides = NULL;
	cm_q3_entityString = cm_q3_emptyEntityString;
	cm_q3_hearData = NULL;
	cm_q3_leafBrushes = NULL;
	cm_q3_leafPatches = NULL;
	cm_q3_leafs = NULL;
	cm_q3_nodes = NULL;
	cm_q3_nullRow = NULL;
	cm_q3_patches = NULL;
	cm_q3_planes = NULL;
	cm_q3_surfaces = NULL;
	cm_q3_visData = NULL;

	cm_q3_numEntityChars = 0;
	cm_q3_numBrushSides = 0;
	cm_q3_numMaterialRefs = 0;
	cm_q3_numPlanes = 0;
	cm_q3_numNodes = 0;
	cm_q3_numLeafs = 1;
	cm_q3_numLeafBrushes = 0;
	cm_q3_numBrushes = 0;
	cm_q3_numVisibility = 0;
	cm_q3_numAreaPortals = 1;
	cm_q3_numAreas = 1;
	cm_q3_numPatches = 0;
	cm_q3_numLeafPatches = 0;
	cm_q3_numVertexes = 0;
	cm_q3_numFaces = 0;
	cm_q3_numLeafFaces = 0;
}

/*
=============================================================================

	QUAKE3 BSP INFORMATION

=============================================================================
*/

/*
==================
CM_Q3BSP_EntityString
==================
*/
char *CM_Q3BSP_EntityString ()
{
	return cm_q3_entityString;
}


/*
==================
CM_Q3BSP_SurfRName
==================
*/
char *CM_Q3BSP_SurfRName (int texNum)
{
	// FIXME
	return NULL;
}


/*
==================
CM_Q3BSP_NumClusters
==================
*/
int CM_Q3BSP_NumClusters ()
{
	return cm_q3_visData ? cm_q3_visData->numClusters : 1;
}

/*
==================
CM_Q3BSP_NumTexInfo
==================
*/
int CM_Q3BSP_NumTexInfo ()
{
	// FIXME
	return 0;
}
