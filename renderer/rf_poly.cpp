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
// rf_poly.cpp
//

#include "rf_local.h"

/*
==============================================================================

	POLYGON FRONTEND

	Generic meshes can be passed from CGAME to be handled for rendering here.
	Typically used for particle effect rendering.
==============================================================================
*/

static refMesh_t		r_polyMesh;

/*
================
R_AddPolysToList
================
*/
void R_AddPolysToList()
{
	if (!r_drawPolys->intVal)
		return;

	refMeshBuffer *lastMB = NULL;
	mQ3BspFog_t *lastFog = NULL;
	refMaterial_t *lastMat = NULL;
	float lastMatTime = 0.0f;
	int numVerts = 0;

	// Add poly meshes to list
	for (uint32 i=0 ; i<ri.scn.polyList.Count() ; i++)
	{
		refPoly_t *p = ri.scn.polyList[i];
		if (p->numVerts > RB_MAX_VERTS)
		{
			Com_Printf(PRNT_ERROR, "Bogus poly passed with %i verts, ignoring!\n", p->numVerts);
			lastMB = NULL;
			continue;
		}

		// Find fog
		mQ3BspFog_t *fog = R_FogForSphere(p->origin, p->radius);

		// Add to the list
		if (!lastMB || lastMat != p->mat || lastFog != fog || lastMatTime != p->matTime || numVerts+p->numVerts > RB_MAX_VERTS)
		{
			numVerts = 0;

			lastMat = p->mat;
			lastFog = fog;

			// Encode our index in the polyList as our mesh for later
			lastMB = ri.scn.currentList->AddToList(MBT_POLY, (void*)(i+1), p->mat, p->matTime, NULL, fog, 0);
		}

		lastMB->infoKey++;
		numVerts += p->numVerts;
	}
}


/*
================
R_PushPoly
================
*/
void R_PushPoly(refMeshBuffer *mb, const meshFeatures_t features)
{
	int start = ((int)mb->mesh)-1;

	for (int i=0 ; i<mb->infoKey ; i++)
	{
		refPoly_t *p = ri.scn.polyList[start+i];

		if (p->mat->flags & MAT_FLARE)
		{
			vec3_t screenVec;
			R_TransformToScreen_Vec3(p->origin, screenVec);

			// Check if it's offscreen
			if (screenVec[0] < ri.def.x || screenVec[0] > ri.def.x + ri.def.width)
				continue;
			if (screenVec[1] < ri.def.y || screenVec[1] > ri.def.y + ri.def.height)
				continue;

			float depth;
			glReadPixels((int)(screenVec[0]), (int)(screenVec[1]), 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
			if (depth+1e-4 < screenVec[2])
				continue;
		}

		r_polyMesh.numVerts = p->numVerts;

		r_polyMesh.colorArray = p->colors;
		r_polyMesh.coordArray = p->texCoords;
		r_polyMesh.vertexArray = p->vertices;

		RB_PushMesh(&r_polyMesh, MF_TRIFAN|features);
	}
}


/*
================
R_PolyOverflow
================
*/
bool R_PolyOverflow(refMeshBuffer *mb)
{
	int start = ((int)mb->mesh)-1;

	int totalVerts = 0;
	for (int i=0 ; i<mb->infoKey ; i++)
	{
		refPoly_t *p = ri.scn.polyList[start+i];
		totalVerts += p->numVerts;
	}

	return RB_BackendOverflow(totalVerts, (totalVerts-2)*3);
}


/*
================
R_PolyInit
================
*/
void R_PolyInit()
{
	r_polyMesh.numIndexes = 0;
	r_polyMesh.indexArray = NULL;
	r_polyMesh.lmCoordArray = NULL;
	r_polyMesh.normalsArray = NULL;
	r_polyMesh.sVectorsArray = NULL;
	r_polyMesh.tVectorsArray = NULL;
}
