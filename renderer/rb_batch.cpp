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
// rb_batch.cpp
//

#include "rb_local.h"

/*
=============
RB_PushTriangleIndexes
=============
*/
static inline void RB_PushTriangleIndexes(index_t *indexes, int count, const meshFeatures_t meshFeatures)
{
	if (meshFeatures & MF_NONBATCHED)
	{
		rb.numIndexes = count;
		rb.inIndices = indexes;
	}
	else
	{
		index_t *currentIndex = rb.batch.indices + rb.numIndexes;
		rb.numIndexes += count;
		rb.inIndices = rb.batch.indices;

		// The following code assumes that R_PushIndexes is fed with triangles
		for ( ; count>0 ; count-=3, indexes+=3, currentIndex+=3)
		{
			currentIndex[0] = rb.numVerts + indexes[0];
			currentIndex[1] = rb.numVerts + indexes[1];
			currentIndex[2] = rb.numVerts + indexes[2];
		}
	}
}


/*
=============
R_PushTrifanIndexes
=============
*/
static inline void R_PushTrifanIndexes(const int numVerts)
{
	index_t *currentIndex = rb.batch.indices + rb.numIndexes;
	rb.numIndexes += (numVerts - 2) * 3;
	rb.inIndices = rb.batch.indices;

	for (int count=2 ; count<numVerts ; count++, currentIndex+=3)
	{
		currentIndex[0] = rb.numVerts;
		currentIndex[1] = rb.numVerts + count - 1;
		currentIndex[2] = rb.numVerts + count;
	}
}

/*
=============
RB_PushMesh
=============
*/
void RB_PushMesh(refMesh_t *mesh, const meshFeatures_t meshFeatures)
{
	qStatCycle_Scope Stat(r_times, ri.pc.timePushMesh);

	// Must have verts
	assert(mesh->numVerts);

	// Check for incompatibilities
	assert(!rb.curMeshFeatures || !((rb.curMeshFeatures ^ meshFeatures) & (MF_NOCULL|MF_DEFORMVS|MF_NONBATCHED)));

	rb.curMeshFeatures |= meshFeatures;

	// Push indexes
	if (meshFeatures & MF_TRIFAN)
		R_PushTrifanIndexes(mesh->numVerts);
	else
		RB_PushTriangleIndexes(mesh->indexArray, mesh->numIndexes, meshFeatures);

	// Push the rest
	if (meshFeatures & MF_NONBATCHED)
	{
		rb.numVerts = 0;

		// Vertexes and normals
		if (meshFeatures & MF_DEFORMVS)
		{
			if (mesh->vertexArray != rb.batch.vertices)
			{
				memcpy(rb.batch.vertices+rb.numVerts, mesh->vertexArray, sizeof(vec3_t) * mesh->numVerts);
			}
			rb.inVertices = rb.batch.vertices;

			if (meshFeatures & MF_NORMALS && mesh->normalsArray)
			{
				if (mesh->normalsArray != rb.batch.normals)
				{
					memcpy(rb.batch.normals+rb.numVerts, mesh->normalsArray, sizeof(vec3_t) * mesh->numVerts);
				}
				rb.inNormals = rb.batch.normals;
			}
		}
		else
		{
			rb.inVertices = mesh->vertexArray;

			if (meshFeatures & MF_NORMALS && mesh->normalsArray)
				rb.inNormals = mesh->normalsArray;
		}

		// Texture coordinates
		if (meshFeatures & MF_STCOORDS && mesh->coordArray)
			rb.inCoords = mesh->coordArray;

		// Lightmap texture coordinates
		if (meshFeatures & MF_LMCOORDS && mesh->lmCoordArray)
			rb.inLMCoords = mesh->lmCoordArray;

		// STVectors
		if (meshFeatures & MF_STVECTORS)
		{
			if (mesh->sVectorsArray)
				rb.inSVectors = mesh->sVectorsArray;
			if (mesh->tVectorsArray)
				rb.inTVectors = mesh->tVectorsArray;
		}

		// Colors
		if (meshFeatures & MF_COLORS && mesh->colorArray)
			rb.inColors = mesh->colorArray;
	}
	else
	{
		if (!ri.scn.bDrawingMeshOutlines)
			ri.pc.meshBatcherPushes++;

		// Vertexes
		memcpy(rb.batch.vertices+rb.numVerts, mesh->vertexArray, sizeof(vec3_t) * mesh->numVerts);
		rb.inVertices = rb.batch.vertices;

		// Colors
		if (meshFeatures & MF_COLORS && mesh->colorArray)
		{
			memcpy(rb.batch.colors+rb.numVerts, mesh->colorArray, sizeof(colorb) * mesh->numVerts);
			rb.inColors = rb.batch.colors;
		}

		// Normals
		if (meshFeatures & MF_NORMALS && mesh->normalsArray)
		{
			memcpy(rb.batch.normals+rb.numVerts, mesh->normalsArray, sizeof(vec3_t) * mesh->numVerts);
			rb.inNormals = rb.batch.normals;
		}

		// Texture coordinates
		if (meshFeatures & MF_STCOORDS && mesh->coordArray)
		{
			memcpy(rb.batch.coords+rb.numVerts, mesh->coordArray, sizeof(vec2_t) * mesh->numVerts);
			rb.inCoords = rb.batch.coords;
		}

		// Lightmap texture coordinates
		if (meshFeatures & MF_LMCOORDS && mesh->lmCoordArray)
		{
			memcpy(rb.batch.lmCoords+rb.numVerts, mesh->lmCoordArray, sizeof(vec2_t) * mesh->numVerts);
			rb.inLMCoords = rb.batch.lmCoords;
		}

		// STVectors
		if (meshFeatures & MF_STVECTORS)
		{
			if (mesh->sVectorsArray)
			{
				memcpy(rb.batch.sVectors+rb.numVerts, mesh->sVectorsArray, sizeof(vec3_t) * mesh->numVerts);
				rb.inSVectors = rb.batch.sVectors;
			}

			if (mesh->tVectorsArray)
			{
				memcpy(rb.batch.tVectors+rb.numVerts, mesh->tVectorsArray, sizeof(vec3_t) * mesh->numVerts);
				rb.inTVectors = rb.batch.tVectors;
			}
		}
	}

	rb.numVerts += mesh->numVerts;
}
