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
// rf_modelBSP.cpp
//

#include "rf_modelLocal.h"

static byte			r_q2BspNoVis[Q2BSP_MAX_VIS];
static byte			r_q3BspNoVis[Q3BSP_MAX_VIS];

// Only valid and used during map load
static uint16		*r_q2_edges;
static int			*r_q2_surfEdges;
static float		*r_q2_vertexes;

extern int			r_q2_lmBlockSize;

/*
===============================================================================

	QUAKE2 BRUSH MODELS

===============================================================================
*/

static void BoundQ2BSPPoly (int numVerts, float *verts, vec3_t mins, vec3_t maxs)
{
	int		i, j;
	float	*v;

	ClearBounds (mins, maxs);
	for (i=0, v=verts ; i<numVerts ; i++)
	{
		for (j=0 ; j<3 ; j++, v++)
		{
			if (*v < mins[j])
				mins[j] = *v;
			if (*v > maxs[j])
				maxs[j] = *v;
		}
	}
}


/*
================
R_GetImageTCSize

This is just a duplicate of R_GetImageSize modified to get the texcoord size for Q2BSP surfaces
================
*/
static void R_GetImageTCSize (refMaterial_t *mat, int *tcWidth, int *tcHeight)
{
	matPass_t	*pass;
	refImage_t	*image;
	int			i;
	int			passNum;

	if (!mat || !mat->numPasses)
	{
		if (tcWidth)
			*tcWidth = 64;
		if (tcHeight)
			*tcHeight = 64;
		return;
	}

	image = NULL;
	passNum = 0;
	for (i=0, pass=mat->passes ; i<mat->numPasses ; pass++, i++)
	{
		if (passNum++ != mat->sizeBase)
			continue;

		image = pass->animImages[0];
		break;
	}

	if (!image)
		return;

	if (tcWidth)
		*tcWidth = image->tcWidth;
	if (tcHeight)
		*tcHeight = image->tcHeight;
}


/*
================
R_SubdivideQ2BSPSurface

Breaks a polygon up along axial ^2 unit boundaries
================
*/
static bool SubdivideQ2Polygon (refModel_t *model, mBspSurface_t *surf, int numVerts, float *verts, float subdivideSize)
{
	int				i, j;
	vec3_t			mins, maxs;
	float			m;
	float			*v;
	vec3_t			front[64], back[64];
	int				f, b;
	float			dist[64];
	float			frac;
	mQ2BspPoly_t	*poly;
	vec3_t			posTotal;
	vec3_t			normalTotal;
	vec2_t			coordTotal;
	float			oneDivVerts;
	byte			*buffer;

	if (numVerts > 60)
	{
		Com_Printf (PRNT_ERROR, "SubdivideQ2Polygon: numVerts = %i\n", numVerts);
		return false;
	}

	BoundQ2BSPPoly (numVerts, verts, mins, maxs);

	for (i=0 ; i<3 ; i++)
	{
		m = (mins[i] + maxs[i]) * 0.5f;
		m = subdivideSize * (float)floor (m/subdivideSize + 0.5f);
		if (maxs[i]-m < 8)
			continue;
		if (m-mins[i] < 8)
			continue;

		// Cut it
		v = verts + i;
		for (j=0 ; j<numVerts ; j++, v+=3)
			dist[j] = *v - m;

		// Wrap cases
		dist[j] = dist[0];
		v -= i;
		Vec3Copy (verts, v);

		f = b = 0;
		v = verts;
		for (j=0 ; j<numVerts ; j++, v+=3)
		{
			if (dist[j] >= 0)
			{
				Vec3Copy (v, front[f]);
				f++;
			}
			if (dist[j] <= 0)
			{
				Vec3Copy (v, back[b]);
				b++;
			}
			if (dist[j] == 0 || dist[j+1] == 0)
				continue;
			if ((dist[j] > 0) != (dist[j+1] > 0))
			{
				// Clip point
				frac = dist[j] / (dist[j] - dist[j+1]);

				front[f][0] = back[b][0] = v[0] + frac*(v[3+0] - v[0]);
				front[f][1] = back[b][1] = v[1] + frac*(v[3+1] - v[1]);
				front[f][2] = back[b][2] = v[2] + frac*(v[3+2] - v[2]);

				f++;
				b++;
			}
		}

		return SubdivideQ2Polygon (model, surf, f, front[0], subdivideSize)
			&& SubdivideQ2Polygon (model, surf, b, back[0], subdivideSize);
	}

	// Add a point in the center to help keep warp valid
	buffer = (byte*)R_ModAlloc(model, sizeof(mQ2BspPoly_t) + ((numVerts+2) * sizeof(colorb)) + ((numVerts+2) * sizeof(vec3_t) * 2) + ((numVerts+2) * sizeof(vec2_t)));
	poly = (mQ2BspPoly_t *)buffer;

	poly->mesh.numVerts = numVerts+2;
	poly->mesh.numIndexes = numVerts*3;

	poly->mesh.indexArray = NULL;
	poly->mesh.lmCoordArray = NULL;
	poly->mesh.sVectorsArray = NULL;
	poly->mesh.tVectorsArray = NULL;

	buffer += sizeof(mQ2BspPoly_t);
	poly->mesh.colorArray = (colorb *)buffer;

	buffer += poly->mesh.numVerts * sizeof(colorb);
	poly->mesh.vertexArray = (vec3_t *)buffer;

	buffer += poly->mesh.numVerts * sizeof(vec3_t);
	poly->mesh.normalsArray = (vec3_t *)buffer;

	buffer += poly->mesh.numVerts * sizeof(vec3_t);
	poly->mesh.coordArray = (vec2_t *)buffer;

	Vec3Clear (posTotal);
	Vec3Clear (normalTotal);
	Vec2Clear (coordTotal);

	for (i=0 ; i<numVerts ; i++)
	{
		// Colors
		Vec4Set (poly->mesh.colorArray[i+1], 255, 255, 255, 255);

		// Position
		Vec3Copy (verts, poly->mesh.vertexArray[i+1]);

		// Normal
		if (!(surf->q2_flags & SURF_PLANEBACK))
			Vec3Copy (surf->q2_plane->normal, poly->mesh.normalsArray[i+1]);
		else
			Vec3Negate (surf->q2_plane->normal, poly->mesh.normalsArray[i+1]);

		// Texture coords
		poly->mesh.coordArray[i+1][0] = DotProduct (verts, surf->q2_texInfo->vecs[0]) * (1.0f/64.0f);
		poly->mesh.coordArray[i+1][1] = DotProduct (verts, surf->q2_texInfo->vecs[1]) * (1.0f/64.0f);

		// For the center point
		Vec3Add (posTotal, verts, posTotal);
		Vec3Add (normalTotal, surf->q2_plane->normal, normalTotal);
		coordTotal[0] += poly->mesh.coordArray[i+1][0];
		coordTotal[1] += poly->mesh.coordArray[i+1][1];

		verts += 3;
	}

	// Center
	oneDivVerts = (1.0f/(float)numVerts);
	Vec4Set (poly->mesh.colorArray[0], 255, 255, 255, 255);
	Vec3Scale (posTotal, oneDivVerts, poly->mesh.vertexArray[0]);
	Vec3Scale (normalTotal, oneDivVerts, poly->mesh.normalsArray[0]);
	VectorNormalizef (poly->mesh.normalsArray[0], poly->mesh.normalsArray[0]);
	Vec2Scale (coordTotal, oneDivVerts, poly->mesh.coordArray[0]);

	// Copy first vertex to last
	Vec4Set (poly->mesh.colorArray[i+1], 255, 255, 255, 255);
	Vec3Copy (poly->mesh.vertexArray[1], poly->mesh.vertexArray[i+1]);
	Vec3Copy (poly->mesh.normalsArray[1], poly->mesh.normalsArray[i+1]);
	Vec2Copy (poly->mesh.coordArray[1], poly->mesh.coordArray[i+1]);

	// Link it in
	poly->next = surf->q2_polys;
	surf->q2_polys = poly;

	return true;
}
static bool R_SubdivideQ2BSPSurface (refModel_t *model, mBspSurface_t *surf, float subdivideSize)
{
	vec3_t		verts[64];
	int			numVerts;
	index_t		index;
	float		*vec;
	int			i;

	// Convert edges back to a normal polygon
	for (i=0, numVerts=0 ; i<surf->q2_numEdges ; numVerts++, i++)
	{
		index = r_q2_surfEdges[surf->q2_firstEdge + i];
		if (index >= 0)
			vec = &r_q2_vertexes[r_q2_edges[index*2+0]*3];
		else
			vec = &r_q2_vertexes[r_q2_edges[(index*-2)+1]*3];

		Vec3Copy (vec, verts[numVerts]);
	}

	return SubdivideQ2Polygon (model, surf, numVerts, verts[0], subdivideSize);
}


/*
================
R_SubdivideQ2BSPLMSurface

Breaks a polygon up along axial ^2 unit boundaries
================
*/
static bool SubdivideQ2BSPLMSurface_r (refModel_t *model, mBspSurface_t *surf, int numVerts, float *verts, float subdivideSize)
{
	int				i, j;
	vec3_t			mins, maxs;
	float			m;
	float			*v;
	vec3_t			front[64], back[64];
	int				f, b;
	float			dist[64];
	float			frac;
	mQ2BspPoly_t	*poly;
	float			s, t;
	vec3_t			posTotal;
	vec3_t			normalTotal;
	vec2_t			coordTotal;
	vec2_t			lmCoordTotal;
	float			oneDivVerts;
	byte			*buffer;

	if (numVerts > 60)
	{
		Com_Printf (PRNT_ERROR, "SubdivideQ2BSPLMSurface_r: numVerts = %i\n", numVerts);
		return false;
	}

	BoundQ2BSPPoly (numVerts, verts, mins, maxs);

	for (i=0 ; i<3 ; i++)
	{
		m = (mins[i] + maxs[i]) * 0.5f;
		m = subdivideSize * (float)floor (m/subdivideSize + 0.5f);
		if (maxs[i] - m < 8)
			continue;
		if (m - mins[i] < 8)
			continue;

		// Cut it
		v = verts + i;
		for (j=0 ; j<numVerts ; j++, v+= 3)
			dist[j] = *v - m;

		// Wrap cases
		dist[j] = dist[0];
		v-=i;
		Vec3Copy (verts, v);

		f = b = 0;
		v = verts;
		for (j=0 ; j<numVerts ; j++, v+= 3)
		{
			if (dist[j] >= 0)
			{
				Vec3Copy (v, front[f]);
				f++;
			}
			if (dist[j] <= 0)
			{
				Vec3Copy (v, back[b]);
				b++;
			}
			if (dist[j] == 0 || dist[j+1] == 0)
				continue;
			if ((dist[j] > 0) != (dist[j+1] > 0))
			{
				// Clip point
				frac = dist[j] / (dist[j] - dist[j+1]);

				front[f][0] = back[b][0] = v[0] + frac*(v[3+0] - v[0]);
				front[f][1] = back[b][1] = v[1] + frac*(v[3+1] - v[1]);
				front[f][2] = back[b][2] = v[2] + frac*(v[3+2] - v[2]);

				f++;
				b++;
			}
		}

		return SubdivideQ2BSPLMSurface_r (model, surf, f, front[0], subdivideSize)
			&& SubdivideQ2BSPLMSurface_r (model, surf, b, back[0], subdivideSize);
	}

	// Add a point in the center to help keep warp valid
	buffer = (byte*)R_ModAlloc(model, sizeof(mQ2BspPoly_t) + ((numVerts+2) * sizeof(colorb)) + ((numVerts+2) * sizeof(vec3_t) * 2) + ((numVerts+2) * sizeof(vec2_t) * 2));
	poly = (mQ2BspPoly_t *)buffer;

	poly->mesh.numVerts = numVerts+2;
	poly->mesh.numIndexes = numVerts*3;

	poly->mesh.indexArray = NULL;
	poly->mesh.sVectorsArray = NULL;
	poly->mesh.tVectorsArray = NULL;

	buffer += sizeof(mQ2BspPoly_t);
	poly->mesh.colorArray = (colorb *)buffer;

	buffer += poly->mesh.numVerts * sizeof(colorb);
	poly->mesh.vertexArray = (vec3_t *)buffer;

	buffer += poly->mesh.numVerts * sizeof(vec3_t);
	poly->mesh.normalsArray = (vec3_t *)buffer;

	buffer += poly->mesh.numVerts * sizeof(vec3_t);
	poly->mesh.coordArray = (vec2_t *)buffer;

	buffer += poly->mesh.numVerts * sizeof(vec2_t);
	poly->mesh.lmCoordArray = (vec2_t *)buffer;

	Vec3Clear (posTotal);
	Vec3Clear (normalTotal);
	Vec2Clear (coordTotal);
	Vec2Clear (lmCoordTotal);

	for (i=0 ; i<numVerts ; i++)
	{
		// Colors
		Vec4Set (poly->mesh.colorArray[i+1], 255, 255, 255, 255);

		// Position
		Vec3Copy (verts, poly->mesh.vertexArray[i+1]);

		// Normals
		if (!(surf->q2_flags & SURF_PLANEBACK))
			Vec3Copy (surf->q2_plane->normal, poly->mesh.normalsArray[i+1]);
		else
			Vec3Negate (surf->q2_plane->normal, poly->mesh.normalsArray[i+1]);

		// Texture coordinates
		poly->mesh.coordArray[i+1][0] = (DotProduct (verts, surf->q2_texInfo->vecs[0]) + surf->q2_texInfo->vecs[0][3]) / surf->q2_texInfo->width;
		poly->mesh.coordArray[i+1][1] = (DotProduct (verts, surf->q2_texInfo->vecs[1]) + surf->q2_texInfo->vecs[1][3]) / surf->q2_texInfo->height;

		// Lightmap texture coordinates
		s = DotProduct (verts, surf->q2_texInfo->vecs[0]) + surf->q2_texInfo->vecs[0][3] - surf->q2_textureMins[0];
		poly->mesh.lmCoordArray[i+1][0] = (s + 8 + (surf->q2_lmCoords[0] * 16)) / (r_q2_lmBlockSize * 16);

		t = DotProduct (verts, surf->q2_texInfo->vecs[1]) + surf->q2_texInfo->vecs[1][3] - surf->q2_textureMins[1];
		poly->mesh.lmCoordArray[i+1][1] = (t + 8 + (surf->q2_lmCoords[1] * 16)) / (r_q2_lmBlockSize * 16);

		// For the center point
		Vec3Add (posTotal, verts, posTotal);
		Vec3Add (normalTotal, surf->q2_plane->normal, normalTotal);

		coordTotal[0] += poly->mesh.coordArray[i+1][0];
		coordTotal[1] += poly->mesh.coordArray[i+1][1];

		lmCoordTotal[0] += poly->mesh.lmCoordArray[i+1][0];
		lmCoordTotal[1] += poly->mesh.lmCoordArray[i+1][1];

		verts += 3;
	}

	// Center point
	oneDivVerts = (1.0f/(float)numVerts);
	Vec4Set (poly->mesh.colorArray[0], 255, 255, 255, 255);
	Vec3Scale (posTotal, oneDivVerts, poly->mesh.vertexArray[0]);
	Vec3Scale (normalTotal, oneDivVerts, poly->mesh.normalsArray[0]);
	VectorNormalizef (poly->mesh.normalsArray[0], poly->mesh.normalsArray[0]);
	Vec2Scale (coordTotal, oneDivVerts, poly->mesh.coordArray[0]);
	Vec2Scale (lmCoordTotal, oneDivVerts, poly->mesh.lmCoordArray[0]);

	// Copy first vertex to last
	Vec4Set (poly->mesh.colorArray[i+1], 255, 255, 255, 255);
	Vec3Copy (poly->mesh.vertexArray[1], poly->mesh.vertexArray[i+1]);
	Vec3Copy (poly->mesh.normalsArray[1], poly->mesh.normalsArray[i+1]);
	Vec2Copy (poly->mesh.coordArray[1], poly->mesh.coordArray[i+1]);
	Vec2Copy (poly->mesh.lmCoordArray[1], poly->mesh.lmCoordArray[i+1]);

	// Link it in
	poly->next = surf->q2_polys;
	surf->q2_polys = poly;

	return true;
}

static bool R_SubdivideQ2BSPLMSurface (refModel_t *model, mBspSurface_t *surf, float subdivideSize)
{
	vec3_t		verts[64];
	int			numVerts;
	int			i;
	int			index;
	float		*vec;

	// Convert edges back to a normal polygon
	for (i=0, numVerts=0 ; i<surf->q2_numEdges ; numVerts++, i++)
	{
		index = r_q2_surfEdges[surf->q2_firstEdge + i];
		if (index >= 0)
			vec = &r_q2_vertexes[r_q2_edges[index*2+0]*3];
		else
			vec = &r_q2_vertexes[r_q2_edges[(index*-2)+1]*3];

		Vec3Copy (vec, verts[numVerts]);
	}

	return SubdivideQ2BSPLMSurface_r (model, surf, numVerts, verts[0], subdivideSize);
}


/*
================
R_BuildQ2BSPSurface
================
*/
static bool R_BuildQ2BSPSurface (refModel_t *model, mBspSurface_t *surf)
{
	byte			*buffer;
	byte			*outColors;
	float			*outCoords;
	index_t			*outIndexes;
	float			*outLMCoords;
	refMesh_t		*outMesh;
	float			*outNormals;
	float			*outVerts;
	int				numVerts, numIndexes;
	float			*vec, s, t;
	int				index, i;
	mQ2BspTexInfo_t	*ti;

	// Bogus face
	if (surf->q2_numEdges < 3)
	{
		assert (0);
		surf->mesh = NULL;
		return true;	// FIXME: return false?
	}

	ti = surf->q2_texInfo;
	numVerts = surf->q2_numEdges;
	numIndexes = (numVerts - 2) * 3;

	// Allocate space
	if (ti->flags & (SURF_TEXINFO_SKY|SURF_TEXINFO_WARP))
	{
		buffer = (byte*)R_ModAlloc(model, sizeof(refMesh_t)
			+ (numVerts * sizeof(vec3_t) * 2)
			+ (numIndexes * sizeof(index_t))
			+ (numVerts * sizeof(vec2_t))
			+ (numVerts * sizeof(colorb)));
	}
	else
	{
		buffer = (byte*)R_ModAlloc(model, sizeof(refMesh_t)
			+ (numVerts * sizeof(vec3_t) * 2)
			+ (numIndexes * sizeof(index_t))
			+ (numVerts * sizeof(vec2_t) * 2)
			+ (numVerts * sizeof(colorb)));
	}

	surf->mesh = outMesh = (refMesh_t *)buffer;
	outMesh->numIndexes = numIndexes;
	outMesh->numVerts = numVerts;

	buffer += sizeof(refMesh_t);
	outVerts = (float *)buffer;

	buffer += sizeof(vec3_t) * numVerts;
	outNormals = (float *)buffer;

	buffer += sizeof(vec3_t) * numVerts;
	outIndexes = (index_t *)buffer;

	buffer += sizeof(index_t) * numIndexes;
	outCoords = (float *)buffer;

	if (ti->flags & (SURF_TEXINFO_SKY|SURF_TEXINFO_WARP))
	{
		outLMCoords = NULL;
	}
	else
	{
		buffer += sizeof(vec2_t) * numVerts;
		outLMCoords = (float *)buffer;
	}

	buffer += sizeof(vec2_t) * numVerts;
	outColors = (byte *)buffer;

	outMesh->colorArray = (colorb *)outColors;
	outMesh->coordArray = (vec2_t *)outCoords;
	outMesh->indexArray = (index_t *)outIndexes;
	outMesh->lmCoordArray = (vec2_t *)outLMCoords;
	outMesh->normalsArray = (vec3_t *)outNormals;
	outMesh->sVectorsArray = NULL;
	outMesh->tVectorsArray = NULL;
	outMesh->vertexArray = (vec3_t *)outVerts;

	// Check mesh validity
	if (RB_InvalidMesh(outMesh))
	{
		Com_Printf (PRNT_ERROR, "R_BuildQ2BSPSurface: surface mesh is invalid: %i verts, %i indexes\n", outMesh->numVerts, outMesh->numIndexes);
		return false;
	}

	// Copy vertex data
	for (i=0 ; i<numVerts ; i++)
	{
		// Color
		Vec4Set (outColors, 255, 255, 255, 255);

		// Position
		index = r_q2_surfEdges[surf->q2_firstEdge+i];
		if (index >= 0)
			vec = &r_q2_vertexes[r_q2_edges[index*2+0]*3];
		else
			vec = &r_q2_vertexes[r_q2_edges[(index*-2)+1]*3];
		Vec3Copy (vec, outVerts);

		// Normal
		if (!(surf->q2_flags & SURF_PLANEBACK))
			Vec3Copy (surf->q2_plane->normal, outNormals);
		else
			Vec3Negate (surf->q2_plane->normal, outNormals);

		// Texture coordinates
		outCoords[0] = (DotProduct (vec, ti->vecs[0]) + ti->vecs[0][3]) / ti->width;
		outCoords[1] = (DotProduct (vec, ti->vecs[1]) + ti->vecs[1][3]) / ti->height;

		outColors += 4;
		outCoords += 2;
		outNormals += 3;
		outVerts += 3;
	}

	// Lightmap coordinates (if needed)
	outVerts = (float *)outMesh->vertexArray;
	if (outLMCoords)
	{
		for (i=0 ; i<numVerts ; i++)
		{
			s = DotProduct (outVerts, ti->vecs[0]) + ti->vecs[0][3] - surf->q2_textureMins[0];
			outLMCoords[0] = (s + 8 + (surf->q2_lmCoords[0] * 16)) / (r_q2_lmBlockSize * 16);

			t = DotProduct (outVerts, ti->vecs[1]) + ti->vecs[1][3] - surf->q2_textureMins[1];
			outLMCoords[1] = (t + 8 + (surf->q2_lmCoords[1] * 16)) / (r_q2_lmBlockSize * 16);

			outVerts += 3;
			outLMCoords += 2;
		}
	}

	// Indexes
	for (i=2 ; i<numVerts ; i++)
	{
		outIndexes[0] = 0;
		outIndexes[1] = i - 1;
		outIndexes[2] = i;

		outIndexes += 3;
	}

	return true;
}


/*
================
R_ConvertQ2BSPSurface
================
*/
static bool R_ConvertQ2BSPSurface (refModel_t *model, mBspSurface_t *surf)
{
	byte			*buffer;
	mQ2BspPoly_t	*poly, *next;
	mQ2BspTexInfo_t	*ti;
	uint32			totalIndexes;
	uint32			totalVerts;
	byte			*outColors;
	float			*outCoords;
	index_t			*outIndexes;
	float			*outLMCoords;
	refMesh_t		*outMesh;
	float			*outNormals;
	float			*outVerts;
	int				i;

	ti = surf->q2_texInfo;

	// Find the total vertex count and index count
	totalIndexes = 0;
	totalVerts = 0;
	for (poly=surf->q2_polys ; poly ; poly=poly->next)
	{
		totalIndexes += (poly->mesh.numVerts - 2) * 3;
		totalVerts += poly->mesh.numVerts;
	}

	// Allocate space
	if (ti->flags & (SURF_TEXINFO_SKY|SURF_TEXINFO_WARP))
	{
		buffer = (byte*)R_ModAlloc(model, sizeof(refMesh_t)
			+ (totalVerts * sizeof(vec3_t) * 2)
			+ (totalIndexes * sizeof(index_t))
			+ (totalVerts * sizeof(vec2_t))
			+ (totalVerts * sizeof(colorb)));
	}
	else
	{
		buffer = (byte*)R_ModAlloc(model, sizeof(refMesh_t)
			+ (totalVerts * sizeof(vec3_t) * 2)
			+ (totalIndexes * sizeof(index_t))
			+ (totalVerts * sizeof(vec2_t) * 2)
			+ (totalVerts * sizeof(colorb)));
	}

	surf->mesh = outMesh = (refMesh_t *)buffer;
	outMesh->numIndexes = totalIndexes;
	outMesh->numVerts = totalVerts;

	buffer += sizeof(refMesh_t);
	outVerts = (float *)buffer;

	buffer += sizeof(vec3_t) * totalVerts;
	outNormals = (float *)buffer;

	buffer += sizeof(vec3_t) * totalVerts;
	outIndexes = (index_t *)buffer;

	buffer += sizeof(index_t) * totalIndexes;
	outCoords = (float *)buffer;

	if (ti->flags & (SURF_TEXINFO_SKY|SURF_TEXINFO_WARP))
	{
		outLMCoords = NULL;
	}
	else
	{
		buffer += sizeof(vec2_t) * totalVerts;
		outLMCoords = (float *)buffer;
	}

	buffer += sizeof(vec2_t) * totalVerts;
	outColors = (byte *)buffer;

	outMesh->colorArray = (colorb *)outColors;
	outMesh->coordArray = (vec2_t *)outCoords;
	outMesh->indexArray = (index_t *)outIndexes;
	outMesh->lmCoordArray = (vec2_t *)outLMCoords;
	outMesh->normalsArray = (vec3_t *)outNormals;
	outMesh->sVectorsArray = NULL;
	outMesh->tVectorsArray = NULL;
	outMesh->vertexArray = (vec3_t *)outVerts;

	// Check mesh validity
	if (RB_InvalidMesh(outMesh))
	{
		Com_Printf (PRNT_ERROR, "R_ConvertQ2BSPSurface: surface mesh is invalid: %i verts, %i indexes\n", outMesh->numVerts, outMesh->numIndexes);
		return false;
	}

	// Store vertex data
	totalIndexes = 0;
	totalVerts = 0;
	for (poly=surf->q2_polys ; poly ; poly=poly->next)
	{
		// Indexes
		outIndexes = outMesh->indexArray + totalIndexes;
		totalIndexes += (poly->mesh.numVerts - 2) * 3;
		for (i=2 ; i<poly->mesh.numVerts ; i++)
		{
			outIndexes[0] = totalVerts;
			outIndexes[1] = totalVerts + i - 1;
			outIndexes[2] = totalVerts + i;

			outIndexes += 3;
		}

		for (i=0 ; i<poly->mesh.numVerts ; i++)
		{
			// Vertices
			outVerts[0] = poly->mesh.vertexArray[i][0];
			outVerts[1] = poly->mesh.vertexArray[i][1];
			outVerts[2] = poly->mesh.vertexArray[i][2];

			// Normals
			outNormals[0] = poly->mesh.normalsArray[i][0];
			outNormals[1] = poly->mesh.normalsArray[i][1];
			outNormals[2] = poly->mesh.normalsArray[i][2];

			// Colors
			outColors[0] = 255;
			outColors[1] = 255;
			outColors[2] = 255;
			outColors[3] = 255;

			// Coords
			outCoords[0] = poly->mesh.coordArray[i][0];
			outCoords[1] = poly->mesh.coordArray[i][1];

			outVerts += 3;
			outNormals += 3;
			outColors += 4;
			outCoords += 2;
		}

		totalVerts += poly->mesh.numVerts;
	}

	// Lightmap coords
	if (!(ti->flags & (SURF_TEXINFO_SKY|SURF_TEXINFO_WARP)))
	{
		for (poly=surf->q2_polys ; poly ; poly=poly->next)
		{
			for (i=0 ; i<poly->mesh.numVerts ; i++)
			{
				outLMCoords[0] = poly->mesh.lmCoordArray[i][0];
				outLMCoords[1] = poly->mesh.lmCoordArray[i][1];

				outLMCoords += 2;
			}
		}
	}

	// Release the old q2_polys crap
	for (poly=surf->q2_polys ; poly ; poly=next)
	{
		next = poly->next;
		Mem_Free (poly);
	}

	return true;
}

/*
===============================================================================

	QUAKE2 BRUSH MODEL LOADING

===============================================================================
*/

/*
================
R_CalcQ2BSPSurfaceExtents

Fills in surf->mins, surf->maxs, surf->textureMins and surf->extents
================
*/
static void R_CalcQ2BSPSurfaceExtents (refModel_t *model, mBspSurface_t *surf)
{
	float			val;
	int				i, j, index;
	float			*vec;
	mQ2BspTexInfo_t	*tex;
	vec2_t			mins, maxs;
	ivec2_t			bmins, bmaxs;

	Clear2DBounds (mins, maxs);
	ClearBounds (surf->mins, surf->maxs);
	tex = surf->q2_texInfo;
	for (i=0 ; i<surf->q2_numEdges ; i++)
	{
		index = r_q2_surfEdges[surf->q2_firstEdge+i];
		if (index >= 0)
			vec = &r_q2_vertexes[r_q2_edges[index*2+0]*3];
		else
			vec = &r_q2_vertexes[r_q2_edges[(index*-2)+1]*3];

		AddPointToBounds (vec, surf->mins, surf->maxs);

		for (j=0 ; j<2 ; j++)
		{
			val = vec[0]*tex->vecs[j][0] + vec[1]*tex->vecs[j][1] + vec[2]*tex->vecs[j][2] + tex->vecs[j][3];
			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i=0 ; i<2 ; i++)
	{	
		bmins[i] = (int)floor (mins[i]/16);
		bmaxs[i] = (int)ceil (maxs[i]/16);

		surf->q2_textureMins[i] = bmins[i] * 16;
		surf->q2_extents[i] = (bmaxs[i] - bmins[i]) * 16;
	}
}


/*
=================
R_SetParentQ2BSPNode
=================
*/
static void R_SetParentQ2BSPNode (mBspNode_t *node, mBspNode_t *parent)
{
	node->parent = parent;
	if (node->q2_contents != -1)
		return;

	R_SetParentQ2BSPNode (node->children[0], node);
	R_SetParentQ2BSPNode (node->children[1], node);
}


/*
=================
R_Q2BSP_SurfPotentiallyVisible
=================
*/
static inline bool R_Q2BSP_SurfPotentiallyVisible (mBspSurface_t *surf)
{
	if (!surf->mesh || RB_InvalidMesh(surf->mesh))
		return false;
	if (!surf->q2_texInfo->mat->numPasses)
		return false;

	return true;
}


/*
=================
R_Q2BSP_SurfPotentiallyLit
=================
*/
static inline bool R_Q2BSP_SurfPotentiallyLit (mBspSurface_t *surf)
{
	if (surf->q2_texInfo->flags & (SURF_TEXINFO_SKY|SURF_TEXINFO_WARP)) 
		return false;
	if (!surf->q2_lmSamples)
		return false;

	return true;
}


/*
=================
R_Q2BSP_SurfPotentiallyFragmented
=================
*/
static inline bool R_Q2BSP_SurfPotentiallyFragmented (mBspSurface_t *surf)
{
	if (surf->q2_texInfo->flags & SURF_TEXINFO_NODRAW)
		return false;
	if (surf->q2_texInfo->mat->flags & MAT_NOMARK)
		return false;

	return true;
}

// ============================================================================

/*
=================
R_LoadQ2BSPVertexes
=================
*/
static bool R_LoadQ2BSPVertexes (refModel_t *model, byte *byteBase, const dQ2BspLump_t *lump)
{
	dQ2BspVertex_t	*in;
	float			*out;
	int				i;

	in = (dQ2BspVertex_t *)((byte*)(byteBase + lump->fileOfs));
	if (lump->fileLen % sizeof(*in))
	{
		Com_Printf (PRNT_ERROR, "R_LoadQ2BSPVertexes: funny lump size in %s\n", model->name);
		return false;
	}

	int numVertexes = lump->fileLen / sizeof(*in);
	r_q2_vertexes = out = (float*)R_ModAlloc(model, sizeof(vec3_t) * numVertexes);

	//
	// Byte swap
	//
	for (i=0 ; i<numVertexes ; i++, in++, out+=3)
	{
		out[0] = LittleFloat (in->point[0]);
		out[1] = LittleFloat (in->point[1]);
		out[2] = LittleFloat (in->point[2]);
	}

	return true;
}


/*
=================
R_LoadQ2BSPEdges
=================
*/
static bool R_LoadQ2BSPEdges (refModel_t *model, byte *byteBase, const dQ2BspLump_t *lump)
{
	dQ2BspEdge_t	*in;
	uint16			*out;
	int				i;

	in = (dQ2BspEdge_t *)((byte*)(byteBase + lump->fileOfs));
	if (lump->fileLen % sizeof(*in))
	{
		Com_Printf (PRNT_ERROR, "R_LoadQ2BSPEdges: funny lump size in %s\n", model->name);
		return false;
	}

	int numEdges = lump->fileLen / sizeof(*in);
	r_q2_edges = out = (uint16*)R_ModAlloc(model, sizeof(uint16) * 2 * (numEdges + 1));

	//
	// Byte swap
	//
	for (i=0 ; i<numEdges ; i++, in++, out+=2)
	{
		out[0] = (uint16) LittleShort (in->v[0]);
		out[1] = (uint16) LittleShort (in->v[1]);
	}

	return true;
}


/*
=================
R_LoadQ2BSPSurfEdges
=================
*/
static bool R_LoadQ2BSPSurfEdges(refModel_t *model, byte *byteBase, const dQ2BspLump_t *lump)
{	
	int		*in;
	int		*out;
	int		i;

	in = (int *)((byte*)(byteBase + lump->fileOfs));
	if (lump->fileLen % sizeof(*in))
	{
		Com_Printf (PRNT_ERROR, "R_LoadQ2BSPSurfEdges: funny lump size in %s\n", model->name);
		return false;
	}

	int numSurfEdges = lump->fileLen / sizeof(*in);
	if (numSurfEdges < 1 || numSurfEdges >= Q2BSP_MAX_SURFEDGES)
	{
		Com_Printf (PRNT_ERROR, "R_LoadQ2BSPSurfEdges: invalid surfEdges count in %s: %i (min: 1; max: %d)\n", model->name, numSurfEdges, Q2BSP_MAX_SURFEDGES);
		return false;
	}

	r_q2_surfEdges = out = (int*)R_ModAlloc(model, sizeof(*out) * numSurfEdges);

	//
	// Byte swap
	//
	for (i=0 ; i<numSurfEdges ; i++)
		out[i] = LittleLong (in[i]);

	return true;
}


/*
=================
R_LoadQ2BSPLighting
=================
*/
static bool R_LoadQ2BSPLighting(refModel_t *model, mQ2BspModel_t *q2BspModel, byte *byteBase, const dQ2BspLump_t *lump)
{
	if (!lump->fileLen)
	{
		q2BspModel->lightData = NULL;
		return true;
	}

	q2BspModel->lightData = (byte*)R_ModAlloc(model, lump->fileLen);	
	memcpy(q2BspModel->lightData, byteBase + lump->fileOfs, lump->fileLen);

	return true;
}


/*
=================
R_LoadQ2BSPPlanes
=================
*/
static bool R_LoadQ2BSPPlanes(refModel_t *model, byte *byteBase, const dQ2BspLump_t *lump)
{
	dQ2BspPlane_t *in = (dQ2BspPlane_t *)((byte*)(byteBase + lump->fileOfs));
	if (lump->fileLen % sizeof(*in))
	{
		Com_Printf (PRNT_ERROR, "R_LoadQ2BSPPlanes: funny lump size in %s\n", model->name);
		return false;
	}

	model->BSPData()->numPlanes = lump->fileLen / sizeof(*in);
	model->BSPData()->planes = (plane_t*)R_ModAlloc(model, sizeof(plane_t) * model->BSPData()->numPlanes * 2);

	//
	// Byte swap
	//
	for (int i=0 ; i<model->BSPData()->numPlanes ; i++, in++)
	{
		plane_t *out = &model->BSPData()->planes[i];
		out->normal[0] = LittleFloat(in->normal[0]);
		out->normal[1] = LittleFloat(in->normal[1]);
		out->normal[2] = LittleFloat(in->normal[2]);
		out->dist = LittleFloat(in->dist);
		CategorizePlane(out);
	}

	return true;
}


/*
=================
R_LoadQ2BSPTexInfo
=================
*/
static bool R_LoadQ2BSPTexInfo(refModel_t *model, mQ2BspModel_t *q2BspModel, byte *byteBase, const dQ2BspLump_t *lump)
{
	dQ2BspTexInfo_t *in = (dQ2BspTexInfo_t *)((byte*)(byteBase + lump->fileOfs));
	if (lump->fileLen % sizeof(*in))
	{
		Com_Printf(PRNT_ERROR, "R_LoadQ2BSPTexInfo: funny lump size in %s\n", model->name);
		return false;
	}

	q2BspModel->numTexInfo = lump->fileLen / sizeof(*in);
	q2BspModel->texInfo = (mQ2BspTexInfo_t*)R_ModAlloc(model, sizeof(mQ2BspTexInfo_t) * q2BspModel->numTexInfo);

	//
	// Byte swap
	//
	for (int i=0 ; i<q2BspModel->numTexInfo ; i++, in++)
	{
		mQ2BspTexInfo_t *out = &q2BspModel->texInfo[i];
		out->vecs[0][0] = LittleFloat(in->vecs[0][0]);
		out->vecs[0][1] = LittleFloat(in->vecs[0][1]);
		out->vecs[0][2] = LittleFloat(in->vecs[0][2]);
		out->vecs[0][3] = LittleFloat(in->vecs[0][3]);
		out->vecs[1][0] = LittleFloat(in->vecs[1][0]);
		out->vecs[1][1] = LittleFloat(in->vecs[1][1]);
		out->vecs[1][2] = LittleFloat(in->vecs[1][2]);
		out->vecs[1][3] = LittleFloat(in->vecs[1][3]);

		out->flags = LittleLong(in->flags);
		int next = LittleLong(in->nextTexInfo);
		out->next = (next > 0) ? q2BspModel->texInfo + next : NULL;

		//
		// Find surfParams
		//
		out->surfParams = 0;
		if (out->flags & SURF_TEXINFO_TRANS33)
			out->surfParams |= MAT_SURF_TRANS33;
		if (out->flags & SURF_TEXINFO_TRANS66)
			out->surfParams |= MAT_SURF_TRANS66;
		if (out->flags & SURF_TEXINFO_WARP)
			out->surfParams |= MAT_SURF_WARP;
		if (out->flags & SURF_TEXINFO_FLOWING)
			out->surfParams |= MAT_SURF_FLOWING;
		if (!(out->flags & SURF_TEXINFO_WARP))
			out->surfParams |= MAT_SURF_LIGHTMAP;

		//
		// Register textures and materials
		//
		if (out->flags & SURF_TEXINFO_SKY)
		{
			out->mat = ri.media.noMaterialSky;
		}
		else
		{
			Com_NormalizePath(out->texName, sizeof(out->texName), Q_VarArgs("textures/%s.wal", in->texture));
			out->mat = R_RegisterTexture(out->texName, out->surfParams);
			if (!out->mat)
			{
				Com_Printf(PRNT_WARNING, "Couldn't load %s\n", out->texName);

				if (out->surfParams & MAT_SURF_LIGHTMAP)
					out->mat = ri.media.noMaterialLightmap;
				else
					out->mat = ri.media.noMaterial;
			}
		}

		R_GetImageTCSize(out->mat, &out->width, &out->height);
	}

	//
	// Count animation frames
	//
	for (int i=0 ; i<q2BspModel->numTexInfo ; i++)
	{
		mQ2BspTexInfo_t *out = &q2BspModel->texInfo[i];
		out->numFrames = 1;
		for (mQ2BspTexInfo_t *step=out->next ; step && step!=out ; step=step->next)
			out->numFrames++;
	}

	return true;
}


/*
=================
R_LoadQ2BSPFaces
=================
*/
static bool R_LoadQ2BSPFaces (refModel_t *model, mQ2BspModel_t *q2BspModel, byte *byteBase, const dQ2BspLump_t *lump)
{
	dQ2BspSurface_t	*in;
	mBspSurface_t	*out;
	int				i, j;

	in = (dQ2BspSurface_t *)((byte*)(byteBase + lump->fileOfs));
	if (lump->fileLen % sizeof(*in))
	{
		Com_Printf (PRNT_ERROR, "R_LoadQ2BSPFaces: funny lump size in %s\n", model->name);
		return false;
	}

	model->BSPData()->numSurfaces = lump->fileLen / sizeof(*in);
	model->BSPData()->surfaces = out = (mBspSurface_t*)R_ModAlloc(model, sizeof(*out) * model->BSPData()->numSurfaces);

	//
	// Byte swap
	//
	for (i=0 ; i<model->BSPData()->numSurfaces ; i++, in++, out++)
	{
		out->q2_firstEdge = LittleLong (in->firstEdge);
		out->q2_numEdges = LittleShort (in->numEdges);		
		out->q2_flags = 0;
		out->q2_polys = NULL;
		out->lmTexNum = BAD_LMTEXNUM;

		out->q2_plane = model->BSPData()->planes + LittleShort (in->planeNum);
		if (LittleShort (in->side))
			out->q2_flags |= SURF_PLANEBACK;

		j = LittleShort (in->texInfo);
		if (j < 0 || j >= q2BspModel->numTexInfo)
		{
			Com_Printf (PRNT_ERROR, "R_LoadQ2BSPFaces: bad texInfo number: %i\n", j);
			return false;
		}
		out->q2_texInfo = q2BspModel->texInfo + j;

		//
		// Calculate surface bounds and extents
		//
		R_CalcQ2BSPSurfaceExtents (model, out);

		//
		// Lighting info
		//
		out->q2_lmWidth = (out->q2_extents[0]>>4) + 1;
		out->q2_lmHeight = (out->q2_extents[1]>>4) + 1;

		j = LittleLong (in->lightOfs);
		out->q2_lmSamples = (j == -1) ? NULL : q2BspModel->lightData + j;

		for (out->q2_numStyles=0 ; out->q2_numStyles<Q2BSP_MAX_LIGHTMAPS && in->styles[out->q2_numStyles]!=255 ; out->q2_numStyles++)
			out->q2_styles[out->q2_numStyles] = in->styles[out->q2_numStyles];

		//
		// Create lightmaps
		//
		if (out->q2_texInfo->flags & SURF_TEXINFO_WARP)
		{
			out->q2_extents[0] = out->q2_extents[1] = 16384;
			out->q2_textureMins[0] = out->q2_textureMins[1] = -8192;
		}
	}

	return true;
}


/*
=================
R_LoadQ2BSPMarkSurfaces
=================
*/
static bool R_LoadQ2BSPMarkSurfaces (refModel_t *model, mQ2BspModel_t *q2BspModel, byte *byteBase, const dQ2BspLump_t *lump)
{
	mBspSurface_t	**out;
	int				i;
	sint16			*in;
	sint16			index;

	in = (sint16 *)((byte*)(byteBase + lump->fileOfs));
	if (lump->fileLen % sizeof(*in))
	{
		Com_Printf (PRNT_ERROR, "R_LoadQ2BSPMarkSurfaces: funny lump size in %s\n", model->name);
		return false;
	}

	q2BspModel->numMarkSurfaces = lump->fileLen / sizeof(*in);
	q2BspModel->markSurfaces = out = (mBspSurface_t**)R_ModAlloc(model, sizeof(*out) * q2BspModel->numMarkSurfaces);

	//
	// Byte swap
	//
	for (i=0 ; i<q2BspModel->numMarkSurfaces ; i++)
	{
		index = LittleShort (in[i]);
		if (index < 0 || index >= model->BSPData()->numSurfaces)
		{
			Com_Printf (PRNT_ERROR, "R_LoadQ2BSPMarkSurfaces: bad surface number: %i\n", index);
			return false;
		}
		out[i] = model->BSPData()->surfaces + index;
	}

	return true;
}


/*
=================
R_LoadQ2BSPVisibility
=================
*/
static bool R_LoadQ2BSPVisibility (refModel_t *model, mQ2BspModel_t *q2BspModel, byte *byteBase, const dQ2BspLump_t *lump)
{
	int i;

	if (!lump->fileLen)
	{
		q2BspModel->vis = NULL;
		return true;
	}

	q2BspModel->vis = (mQ2BspVis_t*)R_ModAlloc(model, lump->fileLen);	
	memcpy (q2BspModel->vis, byteBase + lump->fileOfs, lump->fileLen);

	q2BspModel->vis->numClusters = LittleLong(q2BspModel->vis->numClusters);

	//
	// Byte swap
	//
	for (i=0 ; i<q2BspModel->vis->numClusters ; i++)
	{
		q2BspModel->vis->bitOfs[i][0] = LittleLong (q2BspModel->vis->bitOfs[i][0]);
		q2BspModel->vis->bitOfs[i][1] = LittleLong (q2BspModel->vis->bitOfs[i][1]);
	}

	return true;
}


/*
=================
R_LoadQ2BSPLeafs
=================
*/
static bool R_LoadQ2BSPLeafs (refModel_t *model, mQ2BspModel_t *q2BspModel, byte *byteBase, const dQ2BspLump_t *lump)
{
	dQ2BspLeaf_t	*in;
	mBspLeaf_t		*out;
	int				i, j;
	bool			badBounds;

	in = (dQ2BspLeaf_t *)((byte*)(byteBase + lump->fileOfs));
	if (lump->fileLen % sizeof(*in))
	{
		Com_Printf (PRNT_ERROR, "R_LoadQ2BSPLeafs: funny lump size in %s\n", model->name);
		return false;
	}

	model->BSPData()->numLeafs = lump->fileLen / sizeof(*in);
	model->BSPData()->leafs = out = (mBspLeaf_t*)R_ModAlloc(model, sizeof(*out) * model->BSPData()->numLeafs);

	//
	// Byte swap
	//
	for (i=0 ; i<model->BSPData()->numLeafs ; i++, in++, out++)
	{
		badBounds = false;
		for (j=0 ; j<3 ; j++)
		{
			out->mins[j] = LittleShort (in->mins[j]);
			out->maxs[j] = LittleShort (in->maxs[j]);
			if (out->mins[j] > out->maxs[j])
				badBounds = true;
		}

		if (i && (badBounds || Vec3Compare (out->mins, out->maxs)))
		{
			Com_DevPrintf (PRNT_WARNING, "WARNING: bad leaf %i bounds:\n", i);
			Com_DevPrintf (PRNT_WARNING, "mins: %i %i %i\n", Q_rint (out->mins[0]), Q_rint (out->mins[1]), Q_rint (out->mins[2]));
			Com_DevPrintf (PRNT_WARNING, "maxs: %i %i %i\n", Q_rint (out->maxs[0]), Q_rint (out->maxs[1]), Q_rint (out->maxs[2]));
			Com_DevPrintf (PRNT_WARNING, "cluster: %i\n", out->cluster);
			Com_DevPrintf (PRNT_WARNING, "area: %i\n", out->area);
			out->badBounds = true;
		}
		else
		{
			out->badBounds = false;
		}

		out->q2_contents = LittleLong (in->contents);

		out->cluster = LittleShort (in->cluster);
		out->area = LittleShort (in->area);

		out->q2_firstMarkSurface = q2BspModel->markSurfaces + LittleShort (in->firstLeafFace);
		out->q2_numMarkSurfaces = LittleShort (in->numLeafFaces);
	}

	return true;
}


/*
=================
R_LoadQ2BSPNodes
=================
*/
static bool R_LoadQ2BSPNodes (refModel_t *model, byte *byteBase, const dQ2BspLump_t *lump)
{
	int				i, p, j;
	dQ2BspNode_t	*in;
	mBspNode_t		*out;
	bool			badBounds;

	in = (dQ2BspNode_t *)((byte*)(byteBase + lump->fileOfs));
	if (lump->fileLen % sizeof(*in))
	{
		Com_Printf (PRNT_ERROR, "R_LoadQ2BSPNodes: funny lump size in %s\n", model->name);
		return false;
	}

	model->BSPData()->numNodes = lump->fileLen / sizeof(*in);
	model->BSPData()->nodes = out = (mBspNode_t*)R_ModAlloc(model, sizeof(*out) * model->BSPData()->numNodes);

	//
	// Byte swap
	//
	for (i=0 ; i<model->BSPData()->numNodes ; i++, in++, out++)
	{
		badBounds = false;
		for (j=0 ; j<3 ; j++)
		{
			out->mins[j] = LittleShort (in->mins[j]);
			out->maxs[j] = LittleShort (in->maxs[j]);

			if (out->mins[j] > out->maxs[j])
				badBounds = true;
		}

		if (badBounds || Vec3Compare (out->mins, out->maxs))
		{
			Com_DevPrintf (PRNT_WARNING, "WARNING: bad node %i bounds:\n", i);
			Com_DevPrintf (PRNT_WARNING, "mins: %i %i %i\n", Q_rint (out->mins[0]), Q_rint (out->mins[1]), Q_rint (out->mins[2]));
			Com_DevPrintf (PRNT_WARNING, "maxs: %i %i %i\n", Q_rint (out->maxs[0]), Q_rint (out->maxs[1]), Q_rint (out->maxs[2]));
			out->badBounds = true;
		}
		else
		{
			out->badBounds = false;
		}

		out->q2_contents = -1;	// Differentiate from leafs

		out->plane = model->BSPData()->planes + LittleLong (in->planeNum);
		out->q2_firstSurface = LittleShort (in->firstFace);
		out->q2_numSurfaces = LittleShort (in->numFaces);

		p = LittleLong (in->children[0]);
		out->children[0] = (p >= 0) ? model->BSPData()->nodes + p : (mBspNode_t *)(model->BSPData()->leafs + (-1 - p));

		p = LittleLong (in->children[1]);
		out->children[1] = (p >= 0) ? model->BSPData()->nodes + p : (mBspNode_t *)(model->BSPData()->leafs + (-1 - p));
	}

	//
	// Set the nodes and leafs
	//
	R_SetParentQ2BSPNode(model->BSPData()->nodes, NULL);

	return true;
}


/*
=================
R_LoadQ2BSPSubModels
=================
*/
static bool R_LoadQ2BSPSubModels (refModel_t *model, byte *byteBase, const dQ2BspLump_t *lump)
{
	dQ2BspModel_t	*in;
	mBspHeader_t	*out;
	bool			badBounds;
	uint32			i;
	int				j;

	in = (dQ2BspModel_t *)((byte*)(byteBase + lump->fileOfs));
	if (lump->fileLen % sizeof(*in))
	{
		Com_Printf (PRNT_ERROR, "R_LoadQ2BSPSubModels: funny lump size in %s\n", model->name);
		return false;
	}

	model->BSPData()->numSubModels = lump->fileLen / sizeof(*in);
	if (model->BSPData()->numSubModels >= MAX_REF_MODELS)
	{
		Com_Printf (PRNT_ERROR, "R_LoadQ2BSPSubModels: too many submodels %i >= %i\n", model->BSPData()->numSubModels, MAX_REF_MODELS);
		return false;
	}

	model->BSPData()->subModels = out = (mBspHeader_t*)R_ModAlloc(model, sizeof(*out) * model->BSPData()->numSubModels);
	model->BSPData()->inlineModels = (refModel_t*)R_ModAlloc(model, sizeof(refModel_t) * model->BSPData()->numSubModels);

	//
	// Byte swap
	//
	for (i=0 ; i<model->BSPData()->numSubModels ; i++, in++, out++)
	{
		// Pad the mins / maxs by a pixel
		badBounds = false;
		for (j=0 ; j<3 ; j++)
		{
			out->mins[j] = LittleFloat (in->mins[j]) - 1;
			out->maxs[j] = LittleFloat (in->maxs[j]) + 1;
			if (out->mins[j] > out->maxs[j])
				badBounds = true;
		}

		if (badBounds || Vec3Compare (out->mins, out->maxs))
		{
			Com_DevPrintf (PRNT_WARNING, "WARNING: bad submodel %i bounds:\n", i);
			Com_DevPrintf (PRNT_WARNING, "mins: %i %i %i\n", Q_rint (out->mins[0]), Q_rint (out->mins[1]), Q_rint (out->mins[2]));
			Com_DevPrintf (PRNT_WARNING, "maxs: %i %i %i\n", Q_rint (out->maxs[0]), Q_rint (out->maxs[1]), Q_rint (out->maxs[2]));
			out->radius = 0;
		}
		else
		{
			out->radius = RadiusFromBounds (out->mins, out->maxs);
		}

		out->origin[0]	= LittleFloat (in->origin[0]);
		out->origin[1]	= LittleFloat (in->origin[1]);
		out->origin[2]	= LittleFloat (in->origin[2]);

		out->q2_headNode = LittleLong (in->headNode);
		out->firstFace = LittleLong (in->firstFace);
		out->numFaces = LittleLong (in->numFaces);
	}

	return true;
}


/*
=================
R_SetupQ2BSPSubModels
=================
*/
static bool R_SetupQ2BSPSubModels(refModel_t *model, mQ2BspModel_t *q2BspModel)
{
	int totalLitRemoved = 0;
	int totalVisRemoved = 0;

	for (uint32 i=0 ; i<model->BSPData()->numSubModels ; i++)
	{
		mBspHeader_t *sm = &model->BSPData()->subModels[i];
		refModel_t *out = &model->BSPData()->inlineModels[i];

		out->memTag = model->memTag;
		out->type = model->type;
		Vec3Copy(model->mins, out->mins);
		Vec3Copy(model->maxs, out->maxs);
		out->radius = model->radius;

		out->modelData = (mBspModelBase_t*)R_ModAlloc(model, sizeof(mQ2BspModel_t));
		memcpy(out->BSPData(), model->BSPData(), sizeof(mQ2BspModel_t));

		out->Q2BSPData()->firstNode = sm->q2_headNode;

		if (out->Q2BSPData()->firstNode >= model->BSPData()->numNodes)
		{
			Com_Printf (PRNT_ERROR, "R_LoadQ2BSPModel: Inline model number '%i' has a bad firstNode (%d >= %d)\n", i, out->Q2BSPData()->firstNode, model->BSPData()->numNodes);
			return false;
		}

		// Find total visible/lit surfaces
		int numLitSurfs = 0;
		int numVisSurfs = 0;
		for (int surfNum=0 ; surfNum<sm->numFaces ; surfNum++)
		{
			mBspSurface_t *surf = out->BSPData()->surfaces + sm->firstFace + surfNum;
			if (R_Q2BSP_SurfPotentiallyVisible(surf))
			{
				numVisSurfs++;
				if (R_Q2BSP_SurfPotentiallyLit(surf))
					numLitSurfs++;
			}
		}

		if (!numVisSurfs)
		{
			out->BSPData()->firstModelVisSurface = NULL;
			out->BSPData()->firstModelLitSurface = NULL;
		}
		else
		{
			// Allocate space
			size_t size = numVisSurfs + 1;
			if (numLitSurfs)
				size += numLitSurfs + 1;
			size *= sizeof(mBspSurface_t *);
			byte *buffer = (byte*)R_ModAlloc(model, size);

			out->BSPData()->firstModelVisSurface = (mBspSurface_t **)buffer;
			buffer += sizeof(mBspSurface_t *) * (numVisSurfs + 1);

			if (numLitSurfs)
				out->BSPData()->firstModelLitSurface = (mBspSurface_t **)buffer;
			else
				out->BSPData()->firstModelLitSurface = NULL;

			// Copy
			numLitSurfs = 0;
			numVisSurfs = 0;
			for (int surfNum=0 ; surfNum<sm->numFaces ; surfNum++)
			{
				mBspSurface_t *surf = model->BSPData()->surfaces + sm->firstFace + surfNum;
				if (R_Q2BSP_SurfPotentiallyVisible(surf))
				{
					out->BSPData()->firstModelVisSurface[numVisSurfs++] = surf;
					if (R_Q2BSP_SurfPotentiallyLit(surf))
						out->BSPData()->firstModelLitSurface[numLitSurfs++] = surf;
				}
			}
		}

		totalLitRemoved += sm->numFaces - numLitSurfs;
		totalVisRemoved += sm->numFaces - numVisSurfs;

		// Copy properties
		Vec3Copy(sm->maxs, out->maxs);
		Vec3Copy(sm->mins, out->mins);
		out->radius = sm->radius;

		if (i == 0)
		{
			Vec3Copy(out->mins, model->mins);
			Vec3Copy(out->maxs, model->maxs);
			model->radius = out->radius;

			memcpy(model->BSPData(), out->BSPData(), sizeof(mQ2BspModel_t));
		}

		out->BSPData()->numLeafs = sm->q2_visLeafs;
	}

	Com_DevPrintf(0, "R_SetupQ2BSPSubModels: %i non-visible %i non-lit surfaces skipped.\n", totalVisRemoved, totalVisRemoved+totalLitRemoved);
	return true;
}


/*
=================
R_FinishQ2BSPModel
=================
*/
static bool R_FinishQ2BSPModel(refModel_t *model)
{
	//
	// Create surface meshes
	//
	for (int i=0 ; i<model->BSPData()->numSurfaces ; i++)
	{
		mBspSurface_t *surf = &model->BSPData()->surfaces[i];

		if (surf->q2_texInfo->mat->flags & MAT_SUBDIVIDE)
		{
			if (surf->q2_texInfo->flags & (SURF_TEXINFO_SKY|SURF_TEXINFO_WARP))
			{
				if (!R_SubdivideQ2BSPSurface(model, surf, surf->q2_texInfo->mat->subdivide)
				|| !R_ConvertQ2BSPSurface(model, surf))
					return false;
			}
			else
			{
				if (!R_SubdivideQ2BSPLMSurface(model, surf, surf->q2_texInfo->mat->subdivide)
				|| !R_ConvertQ2BSPSurface(model, surf))
					return false;
			}
		}
		else if (!R_BuildQ2BSPSurface(model, surf))
		{
			return false;
		}
	}

	//
	// Generate leaf surface lists
	//
	int totalFragRemoved = 0;
	for (int i=0 ; i<model->BSPData()->numLeafs ; i++)
	{
		mBspLeaf_t *leaf = &model->BSPData()->leafs[i];
		if (!leaf->q2_numMarkSurfaces)
			continue;

		// Mark poly flags
		if (leaf->q2_contents & (CONTENTS_WATER|CONTENTS_SLIME|CONTENTS_LAVA))
		{
			for (int j=0 ; j<leaf->q2_numMarkSurfaces ; j++)
			{
				leaf->q2_firstMarkSurface[j]->q2_flags |= SURF_UNDERWATER;

				if (leaf->q2_contents & CONTENTS_LAVA)
					leaf->q2_firstMarkSurface[j]->q2_flags |= SURF_LAVA;
				if (leaf->q2_contents & CONTENTS_SLIME)
					leaf->q2_firstMarkSurface[j]->q2_flags |= SURF_SLIME;
				if (leaf->q2_contents & CONTENTS_WATER)
					leaf->q2_firstMarkSurface[j]->q2_flags |= SURF_WATER;
			}
		}

		// Calculate the total amount of fragmentable surfaces
		int numFragSurfs = 0;
		for (int j=0 ; j<leaf->q2_numMarkSurfaces ; j++)
		{
			mBspSurface_t *surf = leaf->q2_firstMarkSurface[j];

			if (surf && R_Q2BSP_SurfPotentiallyVisible(surf))
			{
				if (R_Q2BSP_SurfPotentiallyFragmented(surf))
					numFragSurfs++;
			}
		}

		if (!numFragSurfs)
		{
			leaf->firstFragmentSurface = NULL;
			continue;
		}

		leaf->firstFragmentSurface = (mBspSurface_t**)R_ModAlloc(model, sizeof(mBspSurface_t *) * (numFragSurfs + 1));

		// Store fragmentable surfaces
		numFragSurfs = 0;
		for (int j=0 ; j<leaf->q2_numMarkSurfaces ; j++)
		{
			mBspSurface_t *surf = leaf->q2_firstMarkSurface[j];

			if (surf && R_Q2BSP_SurfPotentiallyVisible(surf))
			{
				if (R_Q2BSP_SurfPotentiallyFragmented(surf))
					leaf->firstFragmentSurface[numFragSurfs++] = surf;
			}
		}

		totalFragRemoved += leaf->q2_numMarkSurfaces - numFragSurfs;
	}

	Com_DevPrintf(0, "R_FinishQ2BSPModel: %i non-fragmentable surfaces skipped.\n", totalFragRemoved);

	//
	// Generate node surface lists
	//
	R_Q2BSP_BeginBuildingLightmaps();

	int totalLitRemoved = 0;
	int totalVisRemoved = 0;
	for (int i=0 ; i<model->BSPData()->numNodes ; i++)
	{
		mBspNode_t *node = &model->BSPData()->nodes[i];

		// Generate surface lists
		if (!node->q2_numSurfaces)
		{
			node->q2_firstVisSurface = NULL;
			node->q2_firstLitSurface = NULL;
			continue;
		}

		// Find total visible/lit surfaces
		int numLitSurfs = 0;
		int numVisSurfs = 0;
		for (int j=0 ; j<node->q2_numSurfaces ; j++)
		{
			mBspSurface_t *surf = model->BSPData()->surfaces + node->q2_firstSurface + j;
			if (R_Q2BSP_SurfPotentiallyVisible(surf))
			{
				numVisSurfs++;
				if (R_Q2BSP_SurfPotentiallyLit(surf))
				{
					numLitSurfs++;

					// Build a lightmap
					R_Q2BSP_CreateSurfaceLightmap(surf);
				}
			}
		}

		if (!numVisSurfs)
		{
			node->q2_firstVisSurface = NULL;
			node->q2_firstLitSurface = NULL;
		}
		else
		{
			// Allocate space
			size_t size = numVisSurfs + 1;
			if (numLitSurfs)
				size += numLitSurfs + 1;
			size *= sizeof(mBspSurface_t *);
			byte *buffer = (byte*)R_ModAlloc(model, size);

			node->q2_firstVisSurface = (mBspSurface_t **)buffer;
			buffer += sizeof(mBspSurface_t *) * (numVisSurfs + 1);

			if (numLitSurfs)
				node->q2_firstLitSurface = (mBspSurface_t **)buffer;
			else
				node->q2_firstLitSurface = NULL;

			// Copy
			numLitSurfs = 0;
			numVisSurfs = 0;
			for (int j=0 ; j<node->q2_numSurfaces ; j++)
			{
				mBspSurface_t *surf = model->BSPData()->surfaces + node->q2_firstSurface + j;
				if (R_Q2BSP_SurfPotentiallyVisible(surf))
				{
					node->q2_firstVisSurface[numVisSurfs++] = surf;
					if (R_Q2BSP_SurfPotentiallyLit(surf))
						node->q2_firstLitSurface[numLitSurfs++] = surf;
				}
			}
		}

		totalLitRemoved += node->q2_numSurfaces - numLitSurfs;
		totalVisRemoved += node->q2_numSurfaces - numVisSurfs;
	}

	R_Q2BSP_EndBuildingLightmaps();

	Com_DevPrintf(0, "R_FinishQ2BSPModel: %i non-visible %i non-lit surfaces skipped.\n", totalVisRemoved, totalVisRemoved+totalLitRemoved);

	//
	// Update surface lightmap coordinates
	//
	for (int i=0 ; i<model->BSPData()->numSurfaces ; i++)
	{
		mQ2BspTexInfo_t		*ti;
		float				*outLMCoords, *outVerts;
		float				s, t;

		mBspSurface_t *surf = &model->BSPData()->surfaces[i];
		if (surf->q2_texInfo->flags & (SURF_TEXINFO_SKY|SURF_TEXINFO_WARP))
			continue;
		if (!surf->mesh)
			continue;

		ti = surf->q2_texInfo;
		outVerts = (float *)surf->mesh->vertexArray;
		outLMCoords = (float *)surf->mesh->lmCoordArray;

		for (int j=0 ; j<surf->mesh->numVerts ; j++)
		{
			s = DotProduct (outVerts, ti->vecs[0]) + ti->vecs[0][3] - surf->q2_textureMins[0];
			outLMCoords[0] = (s + 8 + (surf->q2_lmCoords[0] * 16)) / (r_q2_lmBlockSize * 16);

			t = DotProduct (outVerts, ti->vecs[1]) + ti->vecs[1][3] - surf->q2_textureMins[1];
			outLMCoords[1] = (t + 8 + (surf->q2_lmCoords[1] * 16)) / (r_q2_lmBlockSize * 16);

			outVerts += 3;
			outLMCoords += 2;
		}
	}

	//
	// Do this for each submodel too
	//

	//
	// Batch surfaces with identical planes, materials, and lightmap texture
	//

	//
	// Re-make markSurface indexes
	//

	//
	// Generate node/leaf special-case surface lists
	//

	return true;
}


/*
=================
R_LoadQ2BSPModel
=================
*/
bool R_LoadQ2BSPModel(refModel_t *model, byte *buffer)
{
	dQ2BspHeader_t	*header;
	byte			*modBase;
	int				version;
	uint32			i;

	//
	// Load the world model
	//
	model->modelData = (mBspModelBase_t*)R_ModAlloc(model, sizeof(mQ2BspModel_t));
	model->type = MODEL_Q2BSP;

	header = (dQ2BspHeader_t *)buffer;
	version = LittleLong (header->version);
	if (version != Q2BSP_VERSION)
	{
		Com_Printf (PRNT_ERROR, "R_LoadQ2BSPModel: %s has wrong version number (%i should be %i)\n", model->name, version, Q2BSP_VERSION);
		return false;
	}

	//
	// Swap all the lumps
	//
	modBase = (byte *)header;
	for (i=0 ; i<sizeof(dQ2BspHeader_t)/4 ; i++)
		((int *)header)[i] = LittleLong (((int *)header)[i]);

	//
	// Load into heap
	//
	if (!R_LoadQ2BSPVertexes	(model, modBase, &header->lumps[Q2BSP_LUMP_VERTEXES])
	|| !R_LoadQ2BSPEdges		(model, modBase, &header->lumps[Q2BSP_LUMP_EDGES])
	|| !R_LoadQ2BSPSurfEdges	(model, modBase, &header->lumps[Q2BSP_LUMP_SURFEDGES])
	|| !R_LoadQ2BSPLighting		(model, model->Q2BSPData(), modBase, &header->lumps[Q2BSP_LUMP_LIGHTING])
	|| !R_LoadQ2BSPPlanes		(model, modBase, &header->lumps[Q2BSP_LUMP_PLANES])
	|| !R_LoadQ2BSPTexInfo		(model, model->Q2BSPData(), modBase, &header->lumps[Q2BSP_LUMP_TEXINFO])
	|| !R_LoadQ2BSPFaces		(model, model->Q2BSPData(), modBase, &header->lumps[Q2BSP_LUMP_FACES])
	|| !R_LoadQ2BSPMarkSurfaces	(model, model->Q2BSPData(), modBase, &header->lumps[Q2BSP_LUMP_LEAFFACES])
	|| !R_LoadQ2BSPVisibility	(model, model->Q2BSPData(), modBase, &header->lumps[Q2BSP_LUMP_VISIBILITY])
	|| !R_LoadQ2BSPLeafs		(model, model->Q2BSPData(), modBase, &header->lumps[Q2BSP_LUMP_LEAFS])
	|| !R_LoadQ2BSPNodes		(model, modBase, &header->lumps[Q2BSP_LUMP_NODES])
	|| !R_LoadQ2BSPSubModels	(model, modBase, &header->lumps[Q2BSP_LUMP_MODELS]))
		return false;

	//
	// Finishing optimizations
	//
	if (!R_FinishQ2BSPModel(model))
		return false;

	//
	// Setup submodels
	//
	if (!R_SetupQ2BSPSubModels(model, model->Q2BSPData()))
		return false;

	//
	// Remove temporary objects
	//
	Mem_Free(r_q2_surfEdges);
	Mem_Free(r_q2_edges);
	Mem_Free(r_q2_vertexes);

	return true;
}

/*
===============================================================================

	QUAKE3 BRUSH MODELS

===============================================================================
*/

/*
=================
R_FogForSphere
=================
*/
mQ3BspFog_t *R_FogForSphere(const vec3_t center, const float radius)
{
	if (ri.scn.worldModel->type == MODEL_Q3BSP && ri.scn.viewType != RVT_SHADOWMAP)
	{
		mQ3BspModel_t *q3BspModel = ri.scn.worldModel->Q3BSPData();
		if (q3BspModel->numFogs)
		{
			for (int i=0 ; i<q3BspModel->numFogs ; i++)
			{
				mQ3BspFog_t *fog = &q3BspModel->fogs[i];
				if (!fog->mat || !fog->name[0])
					continue;

				bool bFound = true;
				for (int j=0 ; j<fog->numPlanes ; j++)
				{
					plane_t *plane = &fog->planes[j];

					// If completely in front of face, no intersection
					if (PlaneDiff(center, plane) > radius)
					{
						bFound = false;
						break;
					}
				}

				if (bFound)
					return fog;
			}
		}
	}

	return NULL;
}


/*
=================
R_SetParentQ3BSPNode
=================
*/
static void R_SetParentQ3BSPNode(mBspNode_t *node, mBspNode_t *parent)
{
	node->parent = parent;
	if (!node->plane)
		return;

	R_SetParentQ3BSPNode(node->children[0], node);
	R_SetParentQ3BSPNode(node->children[1], node);
}


/*
=================
R_Q3BSP_SurfPotentiallyFragmented

Only true if R_Q3BSP_SurfPotentiallyVisible is true
=================
*/
static inline bool R_Q3BSP_SurfPotentiallyFragmented(mBspSurface_t *surf)
{
	if (surf->q3_matRef->flags & (SHREF_NOMARKS|SHREF_NOIMPACT))
		return false;

	switch (surf->q3_faceType)
	{
	case FACETYPE_PLANAR:
		if (surf->q3_matRef->contents & CONTENTS_SOLID)
			return true;
		break;

	case FACETYPE_PATCH:
		return true;
	}

	return false;
}


/*
=============
R_Q3BSP_SurfPotentiallyLit

Only true if R_Q3BSP_SurfPotentiallyVisible is true
=============
*/
static inline bool R_Q3BSP_SurfPotentiallyLit(mBspSurface_t *surf)
{
	if (surf->q3_matRef->flags & (SHREF_SKY|SHREF_NODLIGHT))
		return false;

	if (surf->q3_faceType == FACETYPE_FLARE)
		return false;
	if (surf->q3_matRef->mat
	&& surf->q3_matRef->mat->flags & (MAT_FLARE|MAT_SKY))
		return false;

	return true;
}


/*
=================
R_Q3BSP_SurfPotentiallyVisible
=================
*/
static inline bool R_Q3BSP_SurfPotentiallyVisible(mBspSurface_t *surf)
{
	if (surf->q3_matRef->flags & SHREF_NODRAW)
		return false;

	if (!surf->mesh || RB_InvalidMesh(surf->mesh))
		return false;
	if (!surf->q3_matRef->mat)
		return false;
	if (!surf->q3_matRef->mat->numPasses)
		return false;

	return true;
}

/*
===============================================================================

	QUAKE3 BRUSH MODEL LOADING

===============================================================================
*/

/*
=================
R_LoadQ3BSPLighting
=================
*/
static bool R_LoadQ3BSPLighting(refModel_t *model, byte *byteBase, const dQ3BspLump_t *lightLump, const dQ3BspLump_t *gridLump)
{
	dQ3BspGridLight_t 	*inGrid;

	mQ3BspModel_t *q3BspModel = model->Q3BSPData();

	// Load lighting
	if (lightLump->fileLen && !r_vertexLighting->intVal)
	{
		if (lightLump->fileLen % Q3LIGHTMAP_SIZE)
		{
			Com_Printf (PRNT_ERROR, "R_LoadQ3BSPLighting: funny lighting lump size in %s\n", model->name);
			return false;
		}

		q3BspModel->numLightmaps = lightLump->fileLen / Q3LIGHTMAP_SIZE;
		q3BspModel->lightmapRects = (mQ3BspLightmapRect_t*)R_ModAlloc(model, q3BspModel->numLightmaps * sizeof(*q3BspModel->lightmapRects));
	}

	// Load the light grid
	if (gridLump->fileLen % sizeof(*inGrid))
	{
		Com_Printf (PRNT_ERROR, "R_LoadQ3BSPLighting: funny lightgrid lump size in %s\n", model->name);
		return false;
	}

	inGrid = (dQ3BspGridLight_t *)((byte*)(byteBase + gridLump->fileOfs));
	q3BspModel->numLightGridElems = gridLump->fileLen / sizeof(*inGrid);
	q3BspModel->lightGrid = (mQ3BspGridLight_t*)R_ModAlloc(model, q3BspModel->numLightGridElems * sizeof(*q3BspModel->lightGrid));

	memcpy (q3BspModel->lightGrid, inGrid, q3BspModel->numLightGridElems * sizeof(*q3BspModel->lightGrid));

	return true;
}


/*
=================
R_LoadQ3BSPVisibility
=================
*/
static bool R_LoadQ3BSPVisibility (refModel_t *model, byte *byteBase, const dQ3BspLump_t *lump)
{
	mQ3BspModel_t *q3BspModel = model->Q3BSPData();

	if (!lump->fileLen)
	{
		q3BspModel->vis = NULL;
		return true;
	}

	q3BspModel->vis = (mQ3BspVis_t*)R_ModAlloc(model, lump->fileLen);
	memcpy (q3BspModel->vis, byteBase + lump->fileOfs, lump->fileLen);

	q3BspModel->vis->numClusters = LittleLong (q3BspModel->vis->numClusters);
	q3BspModel->vis->rowSize = LittleLong (q3BspModel->vis->rowSize);

	return true;
}


/*
=================
R_LoadQ3BSPVertexes
=================
*/
static bool R_LoadQ3BSPVertexes(refModel_t *model, byte *byteBase, const dQ3BspLump_t *lump)
{
	dQ3BspVertex_t *in = (dQ3BspVertex_t *)((byte*)(byteBase + lump->fileOfs));
	if (lump->fileLen % sizeof(*in))
	{
		Com_Printf (PRNT_ERROR, "R_LoadQ3BSPVertexes: funny lump size in %s\n", model->name);
		return false;
	}
	const int numVertexes = lump->fileLen / sizeof(*in);

	byte *buffer = (byte*)R_ModAlloc(model, (numVertexes * sizeof(vec3_t) * 2)
		+ (numVertexes * sizeof(vec2_t) * 2)
		+ (numVertexes * sizeof(colorb)));

	mQ3BspModel_t *q3BspModel = model->Q3BSPData();

	q3BspModel->numVertexes = numVertexes;
	q3BspModel->vertexArray = (vec3_t *)buffer; buffer += numVertexes * sizeof(vec3_t);
	q3BspModel->normalsArray = (vec3_t *)buffer; buffer += numVertexes * sizeof(vec3_t);
	q3BspModel->coordArray = (vec2_t *)buffer; buffer += numVertexes * sizeof(vec2_t);
	q3BspModel->lmCoordArray = (vec2_t *)buffer; buffer += numVertexes * sizeof(vec2_t);	
	q3BspModel->colorArray = (colorb *)buffer;

	float *outVerts = q3BspModel->vertexArray[0];
	float *outNormals = q3BspModel->normalsArray[0];
	float *outCoords = q3BspModel->coordArray[0];
	float *outLMCoords = q3BspModel->lmCoordArray[0];
	byte *outColors = q3BspModel->colorArray[0];

	float modulateDiv;
	if (r_lmModulate->intVal > 0)
		modulateDiv = (float)BIT(r_lmModulate->intVal) / 255.0f;
	else
		modulateDiv = 1.0f / 255.0f;

	for (int i=0 ; i<numVertexes ; i++, in++)
	{
		outVerts[0] = LittleFloat(in->point[0]);
		outVerts[1] = LittleFloat(in->point[1]);
		outVerts[2] = LittleFloat(in->point[2]);
		outNormals[0] = LittleFloat(in->normal[0]);
		outNormals[1] = LittleFloat(in->normal[1]);
		outNormals[2] = LittleFloat(in->normal[2]);
		outCoords[0] = LittleFloat(in->texCoords[0]);
		outCoords[1] = LittleFloat(in->texCoords[1]);
		outLMCoords[0] = LittleFloat(in->lmCoords[0]);
		outLMCoords[1] = LittleFloat(in->lmCoords[1]);

		if (r_fullbright->intVal)
		{
			outColors[0] = 255;
			outColors[1] = 255;
			outColors[2] = 255;
			outColors[3] = in->color[3];
		}
		else
		{
			vec3_t color, fcolor;
			color[0] = ((float)in->color[0] * modulateDiv);
			color[1] = ((float)in->color[1] * modulateDiv);
			color[2] = ((float)in->color[2] * modulateDiv);
			if (!r_coloredLighting->intVal)
			{
				float grey = (color[0] * 0.3f) + (color[1] * 0.59f) + (color[2] * 0.11f);
				Vec3Set(color, grey, grey, grey);
			}
			ColorNormalizef(color, fcolor);

			outColors[0] = (byte)(fcolor[0] * 255);
			outColors[1] = (byte)(fcolor[1] * 255);
			outColors[2] = (byte)(fcolor[2] * 255);
			outColors[3] = in->color[3];
		}

		outVerts += 3;
		outNormals += 3;
		outCoords += 2;
		outLMCoords += 2;
		outColors += 4;
	}

	return true;
}


/*
=================
R_LoadQ3BSPSubmodels
=================
*/
static bool R_LoadQ3BSPSubmodels(refModel_t *model, byte *byteBase, const dQ3BspLump_t *lump)
{
	dQ3BspModel_t *in = (dQ3BspModel_t *)((byte*)(byteBase + lump->fileOfs));
	if (lump->fileLen % sizeof(*in))
	{
		Com_Printf(PRNT_ERROR, "R_LoadQ3BSPSubmodels: funny lump size in %s\n", model->name);
		return false;
	}

	model->BSPData()->numSubModels = lump->fileLen / sizeof(*in);
	if (model->BSPData()->numSubModels >= MAX_REF_MODELS)
	{
		Com_Printf(PRNT_ERROR, "R_LoadQ3BSPSubmodels: too many submodels %i >= %i\n", model->BSPData()->numSubModels, MAX_REF_MODELS);
		return false;
	}

	model->BSPData()->subModels = (mBspHeader_t*)R_ModAlloc(model, model->BSPData()->numSubModels * sizeof(mBspHeader_t));
	model->BSPData()->inlineModels = (refModel_t*)R_ModAlloc(model, sizeof(refModel_t) * model->BSPData()->numSubModels);

	for (uint32 i=0 ; i<model->BSPData()->numSubModels ; i++, in++)
	{
		mBspHeader_t *out = &model->BSPData()->subModels[i];

		// Spread the mins / maxs by a pixel
		out->mins[0] = LittleFloat(in->mins[0]) - 1;
		out->mins[1] = LittleFloat(in->mins[1]) - 1;
		out->mins[2] = LittleFloat(in->mins[2]) - 1;
		out->maxs[0] = LittleFloat(in->maxs[0]) + 1;
		out->maxs[1] = LittleFloat(in->maxs[1]) + 1;
		out->maxs[2] = LittleFloat(in->maxs[2]) + 1;

		out->radius = RadiusFromBounds(out->mins, out->maxs);
		out->firstFace = LittleLong(in->firstFace);
		out->numFaces = LittleLong(in->numFaces);
	}

	return true;
}


/*
=================
R_LoadQ3BSPMaterialRefs
=================
*/
static bool R_LoadQ3BSPMaterialRefs(refModel_t *model, byte *byteBase, const dQ3BspLump_t *lump)
{
	dQ3BspMaterialRef_t *in = (dQ3BspMaterialRef_t *)((byte*)(byteBase + lump->fileOfs));
	if (lump->fileLen % sizeof(*in))
	{
		Com_Printf (PRNT_ERROR, "R_LoadQ3BSPMaterialRefs: funny lump size in %s\n", model->name);	
		return false;
	}

	mQ3BspModel_t *q3BspModel = model->Q3BSPData();

	q3BspModel->numMatRefs = lump->fileLen / sizeof(*in);
	q3BspModel->matRefs = (mQ3BspMaterialRef_t*)R_ModAlloc(model, q3BspModel->numMatRefs * sizeof(mQ3BspMaterialRef_t));

	for (int i=0 ; i<q3BspModel->numMatRefs ; i++, in++)
	{
		mQ3BspMaterialRef_t *out = &q3BspModel->matRefs[i];
		Com_NormalizePath(out->name, sizeof(out->name), in->name);
		out->flags = LittleLong (in->flags);
		out->contents = LittleLong (in->contents);
		out->mat = NULL;
	}

	return true;
}


/*
=================
R_CreateQ3BSPMeshForSurface
=================
*/
#define COLOR_RGB(r,g,b)	(((r) << 0)|((g) << 8)|((b) << 16))
#define COLOR_RGBA(r,g,b,a) (((r) << 0)|((g) << 8)|((b) << 16)|((a) << 24))
static refMesh_t *R_CreateQ3BSPMeshForSurface (refModel_t *model, dQ3BspFace_t *in, mBspSurface_t *out)
{
	static vec3_t	tempNormalsArray[RB_MAX_VERTS];
	refMesh_t		*mesh;

	mQ3BspModel_t *q3BspModel = model->Q3BSPData();

	switch (out->q3_faceType)
	{
	case FACETYPE_FLARE:
		{
			mesh = (refMesh_t *)R_ModAlloc(model, sizeof(refMesh_t) + sizeof(vec3_t));
			mesh->vertexArray = (vec3_t *)((byte *)mesh + sizeof(refMesh_t));
			mesh->numVerts = 1;
			mesh->indexArray = (index_t *)1;
			mesh->numIndexes = 1;
			Vec3Copy(out->q3_origin, mesh->vertexArray[0]);

			int r = LittleFloat(in->mins[0]) * 255.0f;
			clamp (r, 0, 255);

			int g = LittleFloat(in->mins[1]) * 255.0f;
			clamp (g, 0, 255);

			int b = LittleFloat(in->mins[2]) * 255.0f;
			clamp (b, 0, 255);

			out->dLightBits =(dlightBit_t)COLOR_RGB(r, g, b);
		}
		return mesh;

	case FACETYPE_PATCH:
		{
			int				i, u, v, p;
			int				patch_cp[2], step[2], size[2], flat[2];
			float			subdivLevel, f;
			int				numVerts, firstVert;
			static vec4_t	colors[RB_MAX_VERTS];
			static vec4_t	colors2[RB_MAX_VERTS];
			index_t			*indexes;
			byte			*buffer;

			patch_cp[0] = LittleLong (in->patch_cp[0]);
			patch_cp[1] = LittleLong (in->patch_cp[1]);

			if (!patch_cp[0] || !patch_cp[1])
				break;

			subdivLevel = bound (1, r_patchDivLevel->intVal, 32);

			numVerts = LittleLong (in->numVerts);
			firstVert = LittleLong (in->firstVert);
			for (i=0 ; i<numVerts ; i++)
				Vec4Scale (q3BspModel->colorArray[firstVert + i], (1.0 / 255.0), colors[i]);

			// Find the degree of subdivision in the u and v directions
			Patch_GetFlatness (subdivLevel, &q3BspModel->vertexArray[firstVert], patch_cp, flat);

			// Allocate space for mesh
			step[0] = (1 << flat[0]);
			step[1] = (1 << flat[1]);
			size[0] = (patch_cp[0] >> 1) * step[0] + 1;
			size[1] = (patch_cp[1] >> 1) * step[1] + 1;
			numVerts = size[0] * size[1];

			if (numVerts > RB_MAX_VERTS)
				break;

			out->q3_patchWidth = size[0];
			out->q3_patchHeight = size[1];

			buffer = (byte*)R_ModAlloc(model, sizeof(refMesh_t)
				+ (numVerts * sizeof(vec2_t) * 2)
				+ (numVerts * sizeof(vec3_t) * 2)
				+ (numVerts * sizeof(colorb))
				+ (numVerts * sizeof(index_t) * ((size[0]-1) * (size[1]-1) * 6)));

			mesh = (refMesh_t *)buffer; buffer += sizeof(refMesh_t);
			mesh->numVerts = numVerts;
			mesh->vertexArray = (vec3_t *)buffer; buffer += numVerts * sizeof(vec3_t);
			mesh->normalsArray = (vec3_t *)buffer; buffer += numVerts * sizeof(vec3_t);
			mesh->coordArray = (vec2_t *)buffer; buffer += numVerts * sizeof(vec2_t);
			mesh->lmCoordArray = (vec2_t *)buffer; buffer += numVerts * sizeof(vec2_t);
			mesh->colorArray = (colorb *)buffer; buffer += numVerts * sizeof(colorb);

			Patch_Evaluate (q3BspModel->vertexArray[firstVert], patch_cp, step, mesh->vertexArray[0], 3);
			Patch_Evaluate (q3BspModel->normalsArray[firstVert], patch_cp, step, tempNormalsArray[0], 3);
			Patch_Evaluate (colors[0], patch_cp, step, colors2[0], 4);
			Patch_Evaluate (q3BspModel->coordArray[firstVert], patch_cp, step, mesh->coordArray[0], 2);
			Patch_Evaluate (q3BspModel->lmCoordArray[firstVert], patch_cp, step, mesh->lmCoordArray[0], 2);

			for (i=0 ; i<numVerts ; i++)
			{
				VectorNormalizef (tempNormalsArray[i], mesh->normalsArray[i]);

				f = max (max (colors2[i][0], colors2[i][1]), colors2[i][2]);
				if (f > 1.0f)
				{
					f = 255.0f / f;
					mesh->colorArray[i][0] = colors2[i][0] * f;
					mesh->colorArray[i][1] = colors2[i][1] * f;
					mesh->colorArray[i][2] = colors2[i][2] * f;
				}
				else
				{
					mesh->colorArray[i][0] = colors2[i][0] * 255;
					mesh->colorArray[i][1] = colors2[i][1] * 255;
					mesh->colorArray[i][2] = colors2[i][2] * 255;
				}
			}

			// Compute new indexes
			mesh->numIndexes = (size[0] - 1) * (size[1] - 1) * 6;
			indexes = mesh->indexArray = (index_t *)buffer;
			for (v=0, i=0 ; v<size[1]-1 ; v++)
			{
				for (u=0 ; u<size[0]-1 ; u++)
				{
					indexes[0] = p = v * size[0] + u;
					indexes[1] = p + size[0];
					indexes[2] = p + 1;
					indexes[3] = p + 1;
					indexes[4] = p + size[0];
					indexes[5] = p + size[0] + 1;
					indexes += 6;
				}
			}
		}
		return mesh;

	case FACETYPE_PLANAR:
	case FACETYPE_TRISURF:
		{
			const int firstVert = LittleLong(in->firstVert);

			mesh = (refMesh_t *)R_ModAlloc(model, sizeof(refMesh_t));
			mesh->numVerts = LittleLong (in->numVerts);
			mesh->vertexArray = q3BspModel->vertexArray + firstVert;
			mesh->normalsArray = q3BspModel->normalsArray + firstVert;
			mesh->coordArray = q3BspModel->coordArray + firstVert;
			mesh->lmCoordArray = q3BspModel->lmCoordArray + firstVert;
			mesh->colorArray = q3BspModel->colorArray + firstVert;
			mesh->indexArray = q3BspModel->surfIndexes + LittleLong(in->firstIndex);
			mesh->numIndexes = LittleLong(in->numIndexes);
		}
		return mesh;
	}

	return NULL;
}

/*
=================
R_LoadQ3BSPFaces
=================
*/
static void R_FixAutosprites(mBspSurface_t *surf)
{
	vec2_t			*stArray;
	index_t			*quad;
	refMesh_t		*mesh;
	refMaterial_t	*mat;
	int				i, j;

	if ((surf->q3_faceType != FACETYPE_PLANAR && surf->q3_faceType != FACETYPE_TRISURF) || !surf->q3_matRef)
		return;

	mesh = surf->mesh;
	if (!mesh || !mesh->numIndexes || mesh->numIndexes % 6)
		return;

	mat = surf->q3_matRef->mat;
	if (!mat || !mat->numDeforms || !(mat->flags & MAT_AUTOSPRITE))
		return;

	for (i=0 ; i<mat->numDeforms ; i++)
		if (mat->deforms[i].type == DEFORMV_AUTOSPRITE)
			break;

	if (i == mat->numDeforms)
		return;

	stArray = mesh->coordArray;
	for (i=0, quad=mesh->indexArray ; i<mesh->numIndexes ; i+=6, quad+=6)
	{
		for (j=0 ; j<6 ; j++)
		{
			if (stArray[quad[j]][0] < -0.1f || stArray[quad[j]][0] > 1.1f || stArray[quad[j]][1] < -0.1f || stArray[quad[j]][1] > 1.1f)
			{
				stArray[quad[0]][0] = 0;
				stArray[quad[0]][1] = 1;
				stArray[quad[1]][0] = 0;
				stArray[quad[1]][1] = 0;
				stArray[quad[2]][0] = 1;
				stArray[quad[2]][1] = 1;
				stArray[quad[5]][0] = 1;
				stArray[quad[5]][1] = 0;
				break;
			}
		}
	}
}
static bool R_LoadQ3BSPFaces(refModel_t *model, byte *byteBase, const dQ3BspLump_t *lump)
{
	int					j;
	dQ3BspFace_t		*in;
	mBspSurface_t 		*out;
	refMesh_t			*mesh;
	mQ3BspFog_t			*fog;
	mQ3BspMaterialRef_t	*matRef;
	int					matNum, fogNum, surfNum;
	float				*vert;

	in = (dQ3BspFace_t *)((byte*)(byteBase + lump->fileOfs));
	if (lump->fileLen % sizeof(*in))
	{
		Com_Printf (PRNT_ERROR, "R_LoadQ3BSPFaces: funny lump size in %s\n", model->name);
		return false;
	}

	mQ3BspModel_t *q3BspModel = model->Q3BSPData();

	model->BSPData()->numSurfaces = lump->fileLen / sizeof(*in);
	model->BSPData()->surfaces = out = (mBspSurface_t*)R_ModAlloc(model, model->BSPData()->numSurfaces * sizeof(*out));

	// Fill it in
	for (surfNum=0 ; surfNum<model->BSPData()->numSurfaces ; surfNum++, in++, out++)
	{
		out->q3_origin[0] = LittleFloat(in->origin[0]);
		out->q3_origin[1] = LittleFloat(in->origin[1]);
		out->q3_origin[2] = LittleFloat(in->origin[2]);

		out->q3_faceType = LittleLong(in->faceType);

		// Lighting info
		if (r_vertexLighting->intVal)
		{
			out->lmTexNum = BAD_LMTEXNUM;
		}
		else
		{
			out->lmTexNum = LittleLong(in->lmTexNum);
			if (out->lmTexNum >= q3BspModel->numLightmaps)
			{
				Com_DevPrintf(PRNT_ERROR, "WARNING: bad lightmap number: %i\n", out->lmTexNum);
				out->lmTexNum = BAD_LMTEXNUM;
			}
		}

		// matRef
		matNum = LittleLong(in->matNum);
		if (matNum < 0 || matNum >= q3BspModel->numMatRefs)
		{
			Com_Printf(PRNT_ERROR, "R_LoadQ3BSPFaces: bad material number\n");
			return false;
		}

		matRef = q3BspModel->matRefs + matNum;
		out->q3_matRef = matRef;

		if (!matRef->mat)
		{
			if (out->q3_faceType == FACETYPE_FLARE)
			{
				matRef->mat = R_RegisterFlare(matRef->name);
				if (!matRef->mat)
				{
					Com_Printf(PRNT_WARNING, "Couldn't load (flare): '%s'\n", matRef->name);
					matRef->mat = ri.media.noMaterial;
				}
			}
			else
			{
				if (out->q3_faceType == FACETYPE_TRISURF || r_vertexLighting->intVal || out->lmTexNum < 0)
				{
					if (out->q3_faceType != FACETYPE_TRISURF && !r_vertexLighting->intVal && out->lmTexNum < 0)
						Com_DevPrintf(PRNT_WARNING, "WARNING: surface '%s' has a lightmap but no lightmap stage!\n", matRef->name);

					matRef->mat = R_RegisterTextureVertex(matRef->name);
					if (!matRef->mat)
					{
						Com_Printf(PRNT_WARNING, "Couldn't load (vertex): '%s'\n", matRef->name);
						matRef->mat = ri.media.noMaterial;
					}
				}
				else
				{
					matRef->mat = R_RegisterTextureLM(matRef->name);
					if (!matRef->mat)
					{
						Com_Printf(PRNT_WARNING, "Couldn't load (lm): '%s'\n", matRef->name);
						matRef->mat = ri.media.noMaterialLightmap;
					}
				}
			}
		}

		// Fog
		fogNum = LittleLong(in->fogNum);
		if (fogNum != -1 && fogNum < q3BspModel->numFogs)
		{
			fog = q3BspModel->fogs + fogNum;
			if (fog->numPlanes && fog->mat && fog->name[0])
				out->q3_fog = fog;
		}

		// Mesh
		mesh = out->mesh = R_CreateQ3BSPMeshForSurface(model, in, out);
		if (!mesh)
			continue;

		// Bounds
		ClearBounds (out->mins, out->maxs);
		for (j=0, vert=mesh->vertexArray[0] ; j<mesh->numVerts ; j++, vert+=3)
			AddPointToBounds(vert, out->mins, out->maxs);

		if (out->q3_faceType == FACETYPE_PLANAR)
		{
			out->q3_origin[0] = LittleFloat(in->normal[0]);
			out->q3_origin[1] = LittleFloat(in->normal[1]);
			out->q3_origin[2] = LittleFloat(in->normal[2]);
		}

		// Fix autosprites
		R_FixAutosprites(out);
	}

	return true;
}


/*
=================
R_LoadQ3BSPNodes
=================
*/
static bool R_LoadQ3BSPNodes(refModel_t *model, byte *byteBase, const dQ3BspLump_t *lump)
{
	int				i, j, p;
	dQ3BspNode_t	*in;
	mBspNode_t 		*out;

	in = (dQ3BspNode_t *)((byte*)(byteBase + lump->fileOfs));
	if (lump->fileLen % sizeof(*in))
	{
		Com_Printf(PRNT_ERROR, "R_LoadQ3BSPNodes: funny lump size in %s\n", model->name);
		return false;
	}

	model->BSPData()->numNodes = lump->fileLen / sizeof(*in);
	model->BSPData()->nodes = out = (mBspNode_t*)R_ModAlloc(model, model->BSPData()->numNodes * sizeof(*out));

	for (i=0 ; i<model->BSPData()->numNodes ; i++, in++, out++)
	{
		out->plane = model->BSPData()->planes + LittleLong(in->planeNum);

		for (j=0 ; j<2 ; j++)
		{
			p = LittleLong (in->children[j]);
			if (p >= 0)
				out->children[j] = model->BSPData()->nodes + p;
			else
				out->children[j] = (mBspNode_t *)(model->BSPData()->leafs + (-1 - p));
		}

		bool badBounds = false;
		for (j=0 ; j<3 ; j++)
		{
			out->mins[j] = LittleFloat(in->mins[j]);
			out->maxs[j] = LittleFloat(in->maxs[j]);
			if (out->mins[j] > out->maxs[j])
				badBounds = true;
		}

		if (!badBounds)
			badBounds = Vec3Compare(out->mins, out->maxs);

		if (badBounds)
		{
			Com_DevPrintf(PRNT_WARNING, "WARNING: bad node %i bounds:\n", i);
			Com_DevPrintf(PRNT_WARNING, "mins: %i %i %i\n", Q_rint(out->mins[0]), Q_rint(out->mins[1]), Q_rint(out->mins[2]));
			Com_DevPrintf(PRNT_WARNING, "maxs: %i %i %i\n", Q_rint(out->maxs[0]), Q_rint(out->maxs[1]), Q_rint(out->maxs[2]));
			out->badBounds = true;
		}
	}

	return true;
}


/*
=================
R_LoadQ3BSPFogs
=================
*/
static bool R_LoadQ3BSPFogs(refModel_t *model, byte *byteBase, const dQ3BspLump_t *lump, const dQ3BspLump_t *brLump, const dQ3BspLump_t *brSidesLump)
{
	int					i, j, p;
	dQ3BspFog_t 		*in;
	mQ3BspFog_t			*out;
	dQ3BspBrush_t 		*inBrushes, *brush;
	dQ3BspBrushSide_t	*inBrushSides, *brushSide;

	inBrushes = (dQ3BspBrush_t *)((byte*)(byteBase + brLump->fileOfs));
	if (brLump->fileLen % sizeof(*inBrushes))
	{
		Com_Printf (PRNT_ERROR, "R_LoadQ3BSPFogs: funny lump size in %s\n", model->name);
		return false;
	}

	inBrushSides = (dQ3BspBrushSide_t *)((byte*)(byteBase + brSidesLump->fileOfs));
	if (brSidesLump->fileLen % sizeof(*inBrushSides))
	{
		Com_Printf (PRNT_ERROR, "R_LoadQ3BSPFogs: funny lump size in %s\n", model->name);
		return false;
	}

	in = (dQ3BspFog_t *)((byte*)(byteBase + lump->fileOfs));
	if (lump->fileLen % sizeof(*in))
	{
		Com_Printf (PRNT_ERROR, "R_LoadQ3BSPFogs: funny lump size in %s\n", model->name);
		return false;
	}

	mQ3BspModel_t *q3BspModel = model->Q3BSPData();
	q3BspModel->numFogs = lump->fileLen / sizeof(*in);
	if (!q3BspModel->numFogs)
		return true;

	if (q3BspModel->numFogs > Q3BSP_MAX_FOGS)
	{
		Com_Printf(PRNT_ERROR, "R_LoadQ3BSPFogs: exceeded maximum fog count! (%i > %i)\n", q3BspModel->numFogs, Q3BSP_MAX_FOGS);
		return false;
	}

	q3BspModel->fogs = out = (mQ3BspFog_t*)R_ModAlloc(model, q3BspModel->numFogs * sizeof(*out));

	for (i=0 ; i<q3BspModel->numFogs ; i++, in++, out++)
	{
		Com_NormalizePath(out->name, sizeof(out->name), in->matName);
		out->mat = R_RegisterTextureLM(out->name);

		p = LittleLong(in->brushNum);
		if (p == -1)
			continue;	 // Global fog
		brush = inBrushes + p;

		p = LittleLong(brush->firstSide);
		if (p == -1)
		{
			out->name[0] = '\0';
			out->mat = NULL;
			continue;
		}
		brushSide = inBrushSides + p;

		p = LittleLong(in->visibleSide);
		if (p == -1)
		{
			out->name[0] = '\0';
			out->mat = NULL;
			continue;
		}

		out->numPlanes = LittleLong(brush->numSides);
		out->planes = (plane_t*)R_ModAlloc(model, out->numPlanes * sizeof(plane_t));

		out->visiblePlane = model->BSPData()->planes + LittleLong(brushSide[p].planeNum);
		for (j=0 ; j<out->numPlanes; j++)
			out->planes[j] = *(model->BSPData()->planes + LittleLong(brushSide[j].planeNum));
	}

	return true;
}


/*
=================
R_LoadQ3BSPLeafs
=================
*/
static bool R_LoadQ3BSPLeafs(refModel_t *model, byte *byteBase, const dQ3BspLump_t *lump, const dQ3BspLump_t *msLump)
{
	int *inMarkSurfaces = (int *)((byte*)(byteBase + msLump->fileOfs));
	if (msLump->fileLen % sizeof(*inMarkSurfaces))
	{
		Com_Printf(PRNT_ERROR, "R_LoadQ3BSPLeafs: funny lump size in %s\n", model->name);
		return false;
	}
	const int countMarkSurfaces = msLump->fileLen / sizeof(*inMarkSurfaces);

	dQ3BspLeaf_t *in = (dQ3BspLeaf_t *)((byte*)(byteBase + lump->fileOfs));
	if (lump->fileLen % sizeof(*in))
	{
		Com_Printf(PRNT_ERROR, "R_LoadQ3BSPLeafs: funny lump size in %s\n", model->name);
		return false;
	}

	mQ3BspModel_t *q3BspModel = model->Q3BSPData();

	model->BSPData()->numLeafs = lump->fileLen / sizeof(*in);
	mBspLeaf_t *out = (mBspLeaf_t*)R_ModAlloc(model, model->BSPData()->numLeafs * sizeof(*out));
	model->BSPData()->leafs = out;

	int totalLitRemoved = 0;
	int totalVisRemoved = 0;
	int totalFragRemoved = 0;

	for (int i=0 ; i<model->BSPData()->numLeafs ; i++, in++, out++)
	{
		bool bBadBounds = false;
		for (int j=0 ; j<3 ; j++)
		{
			out->mins[j] = (float)LittleLong(in->mins[j]);
			out->maxs[j] = (float)LittleLong(in->maxs[j]);
			if (out->mins[j] > out->maxs[j])
				bBadBounds = true;
		}
		out->cluster = LittleLong(in->cluster);

		if (i && (bBadBounds || Vec3Compare(out->mins, out->maxs)))
		{
			Com_DevPrintf(PRNT_WARNING, "WARNING: bad leaf %i bounds:\n", i);
			Com_DevPrintf(PRNT_WARNING, "mins: %i %i %i\n", Q_rint(out->mins[0]), Q_rint(out->mins[1]), Q_rint(out->mins[2]));
			Com_DevPrintf(PRNT_WARNING, "maxs: %i %i %i\n", Q_rint(out->maxs[0]), Q_rint(out->maxs[1]), Q_rint(out->maxs[2]));
			Com_DevPrintf(PRNT_WARNING, "cluster: %i\n", out->cluster);
			Com_DevPrintf(PRNT_WARNING, "surfaces: %i\n", LittleLong(in->numLeafFaces));
			Com_DevPrintf(PRNT_WARNING, "brushes: %i\n", LittleLong(in->numLeafBrushes));
			out->badBounds = true;
			out->cluster = -1;
		}

		if (q3BspModel->vis && out->cluster >= q3BspModel->vis->numClusters)
		{
			Com_Printf(PRNT_ERROR, "R_LoadQ3BSPLeafs: leaf cluster > numclusters\n");
			return false;
		}

		out->plane = NULL;
		out->area = LittleLong(in->area) + 1;

		const int numMarkSurfaces = LittleLong(in->numLeafFaces);
		if (!numMarkSurfaces)
			continue;

		const int firstMarkSurface = LittleLong(in->firstLeafFace);
		if (firstMarkSurface < 0 || numMarkSurfaces + firstMarkSurface > countMarkSurfaces)
		{
			Com_Printf(PRNT_ERROR, "R_LoadQ3BSPLeafs: bad marksurfaces in leaf %i\n", i);
			return false;
		}

		// Count how many surfaces we're going to have in our lists
		int numVisSurfs = 0;
		int numLitSurfs = 0;
		int numFragSurfs = 0;
		for (int j=0 ; j<numMarkSurfaces ; j++)
		{
			int k = LittleLong(inMarkSurfaces[firstMarkSurface + j]);
			if (k < 0 || k >= model->BSPData()->numSurfaces)
			{
				Com_Printf(PRNT_ERROR, "R_LoadQ3BSPLeafs: bad surface number: %i\n", k);
				return false;
			}

			mBspSurface_t *surf = model->BSPData()->surfaces + k;
			if (R_Q3BSP_SurfPotentiallyVisible(surf))
			{
				numVisSurfs++;

				if (R_Q3BSP_SurfPotentiallyLit(surf))
					numLitSurfs++;

				if (R_Q3BSP_SurfPotentiallyFragmented(surf))
					numFragSurfs++;
			}
		}

		if (!numVisSurfs)
		{
			out->q3_firstVisSurface = NULL;
			out->q3_firstLitSurface = NULL;
			out->firstFragmentSurface = NULL;
		}
		else
		{
			// Allocate a buffer
			size_t size = numVisSurfs + 1;
			if (numLitSurfs)
				size += numLitSurfs + 1;
			if (numFragSurfs)
				size += numFragSurfs + 1;
			size *= sizeof(mBspSurface_t *);

			byte *buffer = (byte *)R_ModAlloc(model, size);
			out->q3_firstVisSurface = (mBspSurface_t **)buffer;
			buffer += (numVisSurfs + 1) * sizeof(mBspSurface_t *);
			if (numLitSurfs)
			{
				out->q3_firstLitSurface = (mBspSurface_t **)buffer;
				buffer += (numLitSurfs + 1) * sizeof(mBspSurface_t *);
			}
			if (numFragSurfs)
			{
				out->firstFragmentSurface = (mBspSurface_t **)buffer;
				buffer += (numFragSurfs + 1) * sizeof(mBspSurface_t *);
			}

			// Store surface list
			numVisSurfs = numLitSurfs = numFragSurfs = 0;
			for (int j=0 ; j<numMarkSurfaces ; j++)
			{
				int k = LittleLong(inMarkSurfaces[firstMarkSurface + j]);
				mBspSurface_t *surf = model->BSPData()->surfaces + k;

				if (R_Q3BSP_SurfPotentiallyVisible(surf))
				{
					out->q3_firstVisSurface[numVisSurfs++] = surf;

					if (R_Q3BSP_SurfPotentiallyLit(surf))
						out->q3_firstLitSurface[numLitSurfs++] = surf;

					if (R_Q3BSP_SurfPotentiallyFragmented(surf))
						out->firstFragmentSurface[numFragSurfs++] = surf;
				}
			}
		}

		totalLitRemoved += numMarkSurfaces - numLitSurfs;
		totalVisRemoved += numMarkSurfaces - numVisSurfs;
		totalFragRemoved += numMarkSurfaces - numFragSurfs;
	}

	Com_DevPrintf(0, "R_LoadQ3BSPLeafs: %i non-visible %i non-lit %i non-fragmentable surfaces skipped.\n", totalVisRemoved, totalVisRemoved+totalLitRemoved, totalVisRemoved+totalFragRemoved);

	return true;
}


/*
=================
R_LoadQ3BSPEntities
=================
*/
static bool R_LoadQ3BSPEntities(refModel_t *model, byte *byteBase, const dQ3BspLump_t *lump)
{
	char *data = (char *)byteBase + lump->fileOfs;
	if (!data || !data[0])
		return true;

	mQ3BspModel_t *q3BspModel = model->Q3BSPData();

	parse_t *ps = PS_StartSession(data, PSP_COMMENT_BLOCK|PSP_COMMENT_LINE);
	char *token;
	for ( ; PS_ParseToken(ps, PSF_ALLOW_NEWLINES, &token) && token[0] == '{' ; )
	{
		bool isWorldSpawn = false;
		for ( ; ; )
		{
			if (!PS_ParseToken(ps, PSF_ALLOW_NEWLINES, &token))
				break;	// Error
			if (token[0] == '}')
				break;	// End of entity

			char key[MAX_KEY];
			Q_strncpyz(key, token, sizeof(key));
			while (key[strlen(key)-1] == ' ')	// remove trailing spaces
				key[strlen(key)-1] = '\0';

			if (!PS_ParseToken(ps, PSF_ALLOW_NEWLINES, &token))
				break;	// Error

			char value[MAX_VALUE];
			Q_strncpyz(value, token, sizeof(value));

			// Now that we have the key pair worked out...
			if (!strcmp(key, "classname"))
			{
				if (!strcmp(value, "worldspawn"))
					isWorldSpawn = true;
			}
			else if (isWorldSpawn && !strcmp(key, "gridsize"))
			{
				float gridsizef[3];
				Vec3Clear(gridsizef);
				sscanf(value, "%f %f %f", &gridsizef[0], &gridsizef[1], &gridsizef[2]);

				if (!gridsizef[0] || !gridsizef[1] || !gridsizef[2])
				{
					int gridsizei[3];
					sscanf(value, "%i %i %i", &gridsizei[0], &gridsizei[1], &gridsizei[2]);
					Vec3Copy(gridsizei, gridsizef);
				}

				Vec3Copy(gridsizef, q3BspModel->gridSize);
			}
		}
	}
	PS_EndSession(ps);
	return true;
}


/*
=================
R_LoadQ3BSPIndexes
=================
*/
static bool R_LoadQ3BSPIndexes (refModel_t *model, byte *byteBase, const dQ3BspLump_t *lump)
{
	int		i, *in, *out;
	
	in = (int *)((byte*)(byteBase + lump->fileOfs));
	if (lump->fileLen % sizeof(*in))
	{
		Com_Printf (PRNT_ERROR, "R_LoadQ3BSPIndexes: funny lump size in %s\n", model->name);
		return false;
	}

	mQ3BspModel_t *q3BspModel = model->Q3BSPData();

	q3BspModel->numSurfIndexes = lump->fileLen / sizeof(*in);
	q3BspModel->surfIndexes = out = (int*)R_ModAlloc(model, q3BspModel->numSurfIndexes * sizeof(*out));

	for (i=0 ; i<q3BspModel->numSurfIndexes ; i++)
		out[i] = LittleLong (in[i]);

	return true;
}


/*
=================
R_LoadQ3BSPPlanes
=================
*/
static bool R_LoadQ3BSPPlanes (refModel_t *model, byte *byteBase, const dQ3BspLump_t *lump)
{
	int				i, j;
	plane_t			*out;
	dQ3BspPlane_t 	*in;
	
	in = (dQ3BspPlane_t *)((byte*)(byteBase + lump->fileOfs));
	if (lump->fileLen % sizeof(*in))
	{
		Com_Printf (PRNT_ERROR, "R_LoadQ3BSPPlanes: funny lump size in %s\n", model->name);
		return false;
	}

	model->BSPData()->numPlanes = lump->fileLen / sizeof(*in);
	model->BSPData()->planes = out = (plane_t*)R_ModAlloc(model, model->BSPData()->numPlanes * sizeof(*out));

	for (i=0 ; i<model->BSPData()->numPlanes ; i++, in++, out++)
	{
		out->type = PLANE_NON_AXIAL;
		out->signBits = 0;

		for (j=0 ; j<3 ; j++)
		{
			out->normal[j] = LittleFloat (in->normal[j]);
			if (out->normal[j] < 0)
				out->signBits |= BIT(j);
			if (out->normal[j] == 1.0f)
				out->type = j;
		}
		out->dist = LittleFloat (in->dist);
	}

	return true;
}


/*
=================
R_SetupQ3BSPSubModels
=================
*/
static void R_SetupQ3BSPSubModels(refModel_t *model)
{
	int totalLitRemoved = 0;
	int totalVisRemoved = 0;

	for (uint32 i=0 ; i<model->BSPData()->numSubModels ; i++)
	{
		mBspHeader_t *sm = &model->BSPData()->subModels[i];
		refModel_t *out = &model->BSPData()->inlineModels[i];

		out->memTag = model->memTag;
		out->type = model->type;
		Vec3Copy(model->mins, out->mins);
		Vec3Copy(model->maxs, out->maxs);
		out->radius = model->radius;

		out->modelData = (mBspModelBase_t*)R_ModAlloc(model, sizeof(mQ3BspModel_t));
		memcpy(out->BSPData(), model->BSPData(), sizeof(mQ3BspModel_t));

		// Find total visible/lit surfaces
		int numLitSurfs = 0;
		int numVisSurfs = 0;
		for (int surfNum=0 ; surfNum<sm->numFaces ; surfNum++)
		{
			mBspSurface_t *surf = out->BSPData()->surfaces + sm->firstFace + surfNum;
			if (R_Q3BSP_SurfPotentiallyVisible(surf))
			{
				numVisSurfs++;
				if (R_Q3BSP_SurfPotentiallyLit(surf))
					numLitSurfs++;
			}
		}

		if (!numVisSurfs)
		{
			out->BSPData()->firstModelVisSurface = NULL;
			out->BSPData()->firstModelLitSurface = NULL;
		}
		else
		{
			// Allocate space
			size_t size = numVisSurfs + 1;
			if (numLitSurfs)
				size += numLitSurfs + 1;
			size *= sizeof(mBspSurface_t *);
			byte *buffer = (byte*)R_ModAlloc(model, size);

			out->BSPData()->firstModelVisSurface = (mBspSurface_t **)buffer;
			buffer += sizeof(mBspSurface_t *) * (numVisSurfs + 1);

			if (numLitSurfs)
				out->BSPData()->firstModelLitSurface = (mBspSurface_t **)buffer;
			else
				out->BSPData()->firstModelLitSurface = NULL;

			// Copy
			numLitSurfs = 0;
			numVisSurfs = 0;
			for (int surfNum=0 ; surfNum<sm->numFaces ; surfNum++)
			{
				mBspSurface_t *surf = model->BSPData()->surfaces + sm->firstFace + surfNum;
				if (R_Q3BSP_SurfPotentiallyVisible(surf))
				{
					out->BSPData()->firstModelVisSurface[numVisSurfs++] = surf;
					if (R_Q3BSP_SurfPotentiallyLit(surf))
						out->BSPData()->firstModelLitSurface[numLitSurfs++] = surf;
				}
			}
		}

		totalLitRemoved += sm->numFaces - numLitSurfs;
		totalVisRemoved += sm->numFaces - numVisSurfs;

		// Copy properties
		Vec3Copy (sm->maxs, out->maxs);
		Vec3Copy (sm->mins, out->mins);
		out->radius = sm->radius;

		if (i == 0)
		{
			Vec3Copy(out->mins, model->mins);
			Vec3Copy(out->maxs, model->maxs);
			model->radius = out->radius;

			memcpy(model->BSPData(), out->BSPData(), sizeof(mQ3BspModel_t));
		}
	}

	Com_DevPrintf(0, "R_SetupQ3BSPSubModels: %i non-visible %i non-lit surfaces skipped.\n", totalVisRemoved, totalVisRemoved+totalLitRemoved);
}


/*
=================
R_SetupQ3LightGrid
=================
*/
static void R_SetupQ3LightGrid(refModel_t *model, mQ3BspModel_t *q3BspModel)
{
	if (q3BspModel->gridSize[0] < 1 || q3BspModel->gridSize[1] < 1 || q3BspModel->gridSize[2] < 1)
		Vec3Set(q3BspModel->gridSize, 64, 64, 128);

	for (int i=0 ; i<3 ; i++)
	{
		q3BspModel->gridMins[i] = q3BspModel->gridSize[i] * ceil ((model->mins[i] + 1) / q3BspModel->gridSize[i]);
		float Max = q3BspModel->gridSize[i] * floor ((model->maxs[i] - 1) / q3BspModel->gridSize[i]);
		q3BspModel->gridBounds[i] = (Max - q3BspModel->gridMins[i])/q3BspModel->gridSize[i] + 1;
	}
	q3BspModel->gridBounds[3] = q3BspModel->gridBounds[1] * q3BspModel->gridBounds[0];
}


/*
=================
R_FinishQ3BSPModel
=================
*/
static void R_FinishQ3BSPModel(refModel_t *model, byte *byteBase, const dQ3BspLump_t *lightmaps)
{
	refMesh_t				*mesh;
	mBspSurface_t 			*surf;
	mQ3BspLightmapRect_t	*lmRect;
	float					*lmArray;
	int						i, j;

	mQ3BspModel_t *q3BspModel = model->Q3BSPData();

	R_Q3BSP_BuildLightmaps (q3BspModel->numLightmaps, Q3LIGHTMAP_WIDTH, Q3LIGHTMAP_WIDTH, byteBase + lightmaps->fileOfs, q3BspModel->lightmapRects);

	// Generate visibility planes for fogs that dont have them
	for (i=0 ; i<q3BspModel->numFogs ; i++)
	{
		mQ3BspFog_t *Fog = &q3BspModel->fogs[i];
		if (!Fog->mat)
			continue;
		if (Fog->visiblePlane)
			continue;

		Fog->visiblePlane = (plane_t*)R_ModAlloc(model, sizeof(plane_t));
		Vec3Set(Fog->visiblePlane->normal, 0, 0, 1);
		Fog->visiblePlane->type = PLANE_Z;
		Fog->visiblePlane->dist = model->maxs[0] + 1;
	}

	// Now walk list of surface and apply lightmap info
	for (i=0, surf=model->BSPData()->surfaces ; i<model->BSPData()->numSurfaces ; i++, surf++)
	{
		if (surf->lmTexNum < 0 || surf->q3_faceType == FACETYPE_FLARE || !(mesh = surf->mesh))
		{
			surf->lmTexNum = BAD_LMTEXNUM;
		}
		else
		{
			lmRect = &q3BspModel->lightmapRects[surf->lmTexNum];
			surf->lmTexNum = lmRect->texNum;

			if (r_lmPacking->intVal)
			{
				// Scale/shift lightmap coords
				lmArray = mesh->lmCoordArray[0];
				for (j=0 ; j<mesh->numVerts ; j++, lmArray+=2)
				{
					lmArray[0] = (double)(lmArray[0]) * lmRect->w + lmRect->x;
					lmArray[1] = (double)(lmArray[1]) * lmRect->h + lmRect->y;
				}
			}
		}
	}

	if (q3BspModel->numLightmaps)
		Mem_Free(q3BspModel->lightmapRects);

	R_SetParentQ3BSPNode(model->BSPData()->nodes, NULL);
}


/*
=================
R_LoadQ3BSPModel
=================
*/
bool R_LoadQ3BSPModel(refModel_t *model, byte *buffer)
{
	//
	// Load the world model
	//
	model->modelData = (mBspModelBase_t*)R_ModAlloc(model, sizeof(mQ3BspModel_t));
	model->type = MODEL_Q3BSP;

	mQ3BspModel_t *q3BspModel = model->Q3BSPData();

	dQ3BspHeader_t *header = (dQ3BspHeader_t *)buffer;
	int version = LittleLong (header->version);
	if (version != Q3BSP_VERSION)
	{
		Com_Printf (PRNT_ERROR, "R_LoadQ3BSPModel: %s has wrong version number (%i should be %i)\n", model->name, version, Q3BSP_VERSION);
		return false;
	}

	//
	// Swap all the lumps
	//
	byte *modBase = (byte *)header;
	for (uint32 i=0 ; i<sizeof(dQ3BspHeader_t)/4 ; i++)
		((int *)header)[i] = LittleLong (((int *)header)[i]);

	//
	// Load into heap
	//
	if (!R_LoadQ3BSPEntities	(model, modBase, &header->lumps[Q3BSP_LUMP_ENTITIES])
	|| !R_LoadQ3BSPVertexes		(model, modBase, &header->lumps[Q3BSP_LUMP_VERTEXES])
	|| !R_LoadQ3BSPIndexes		(model, modBase, &header->lumps[Q3BSP_LUMP_INDEXES])
	|| !R_LoadQ3BSPLighting		(model, modBase, &header->lumps[Q3BSP_LUMP_LIGHTING], &header->lumps[Q3BSP_LUMP_LIGHTGRID])
	|| !R_LoadQ3BSPVisibility	(model, modBase, &header->lumps[Q3BSP_LUMP_VISIBILITY])
	|| !R_LoadQ3BSPMaterialRefs	(model, modBase, &header->lumps[Q3BSP_LUMP_MATERIALREFS])
	|| !R_LoadQ3BSPPlanes		(model, modBase, &header->lumps[Q3BSP_LUMP_PLANES])
	|| !R_LoadQ3BSPFogs			(model, modBase, &header->lumps[Q3BSP_LUMP_FOGS], &header->lumps[Q3BSP_LUMP_BRUSHES], &header->lumps[Q3BSP_LUMP_BRUSHSIDES])
	|| !R_LoadQ3BSPFaces		(model, modBase, &header->lumps[Q3BSP_LUMP_FACES])
	|| !R_LoadQ3BSPLeafs		(model, modBase, &header->lumps[Q3BSP_LUMP_LEAFS], &header->lumps[Q3BSP_LUMP_LEAFFACES])
	|| !R_LoadQ3BSPNodes		(model, modBase, &header->lumps[Q3BSP_LUMP_NODES])
	|| !R_LoadQ3BSPSubmodels	(model, modBase, &header->lumps[Q3BSP_LUMP_MODELS]))
		return false;

	// Finishing touches
	R_FinishQ3BSPModel(model, modBase, &header->lumps[Q3BSP_LUMP_LIGHTING]);

	// Set up the submodels
	R_SetupQ3BSPSubModels(model);

	// Set up lightgrid
	R_SetupQ3LightGrid(model, q3BspModel);

	return true;
}

/*
===============================================================================

	BRUSH MODEL COMMON

===============================================================================
*/

/*
===============
R_PointInBSPLeaf
===============
*/
mBspLeaf_t *R_PointInBSPLeaf(const vec3_t point, refModel_t *model)
{
	if (!model || !model->BSPData() || !model->BSPData()->nodes)
		Com_Error (ERR_DROP, "R_PointInBSPLeaf: bad model");

	mBspNode_t *node = model->BSPData()->nodes;
	if (model->type == MODEL_Q2BSP)
	{
		for ( ; ; )
		{
			if (node->q2_contents != -1)
				return (mBspLeaf_t *)node;

			const float diff = PlaneDiff(point, node->plane);
			node = node->children[!(diff > 0)];
		}
	}
	else if (model->type == MODEL_Q3BSP)
	{
		do
		{
			const float diff = PlaneDiff(point, node->plane);
			node = node->children[diff < 0];
		} while (node->plane);

		return (mBspLeaf_t *)node;
	}

	assert(0);
	return NULL;
}


/*
=================
R_BSPClusterPVS
=================
*/
byte *R_BSPClusterPVS(const int cluster, refModel_t *model)
{
	if (model->type == MODEL_Q2BSP)
	{
		static byte	decompressed[Q2BSP_MAX_VIS];

		if (cluster == -1 || !model->Q2BSPData()->vis)
			return r_q2BspNoVis;

		int row = (model->Q2BSPData()->vis->numClusters+7)>>3;
		byte *in = (byte *)model->Q2BSPData()->vis + model->Q2BSPData()->vis->bitOfs[cluster][Q2BSP_VIS_PVS];
		byte *out = decompressed;

		if (!in)
		{
			// no vis info, so make all visible
			while (row)
			{
				*out++ = 0xff;
				row--;
			}
			return decompressed;
		}

		do
		{
			if (*in)
			{
				*out++ = *in++;
				continue;
			}

			int c = in[1];
			in += 2;

			while (c)
			{
				*out++ = 0;
				c--;
			}
		} while (out - decompressed < row);

		return decompressed;
	}
	else if (model->type == MODEL_Q3BSP)
	{
		mQ3BspModel_t *q3BspModel = model->Q3BSPData();

		if (cluster == -1 || !q3BspModel->vis)
			return r_q3BspNoVis;

		return ((byte *)q3BspModel->vis->data + cluster*q3BspModel->vis->rowSize);
	}

	assert(0);
	return NULL;
}


/*
=================
R_ModelBSPInit
=================
*/
void R_ModelBSPInit()
{
	memset(r_q2BspNoVis, 0xff, sizeof(r_q2BspNoVis));
	memset(r_q3BspNoVis, 0xff, sizeof(r_q3BspNoVis));
}
