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
// rf_2d.c
//

#include "rf_local.h"

/*
===============================================================================

	2D HANDLING

===============================================================================
*/

static refMesh_t		r_2DMesh;
static refMeshBuffer	r_2DMBuffer;

static vec3_t		r_2DVertices[4];
static vec3_t		r_2DNormals[4] = { {0,1,0}, {0,1,0}, {0,1,0}, {0,1,0} };
static vec2_t		r_2DTexCoords[4];
static colorb		r_2DColors[4];

/*
=============
R_DrawPic
=============
*/
void R_DrawPic(refMaterial_t *mat, const float matTime,
			   const QuadVertices &vertices,
			   const colorb &color)
{
	if (!mat)
		return;

	if (!RB_In2DMode())
	{
		Com_Printf(PRNT_ERROR, "R_DrawPic called while not in 2D rendering mode! Ignored!\n");
		return;
	}

	for (int i = 0; i < 4; ++i)
	{
		Vec2Copy(vertices.verts[i], r_2DVertices[i]);
		Vec2Copy(vertices.stCoords[i], r_2DTexCoords[i]);
		r_2DColors[i] = color;
	}

	if (r_2DMBuffer.mesh)
	{
		if (RB_BackendOverflow(&r_2DMesh)
		|| r_2DMBuffer.mesh != mat
		|| r_2DMBuffer.matTime != matTime)
		{
			RB_RenderMeshBuffer(&r_2DMBuffer);

			r_2DMBuffer.Setup(MBT_2D, mat, mat, matTime, NULL, NULL, 0);
		}
	}
	else
	{
		r_2DMBuffer.Setup(MBT_2D, mat, mat, matTime, NULL, NULL, 0);
	}

	meshFeatures_t features = MF_TRIFAN|mat->features;
	RB_PushMesh(&r_2DMesh, features);
	if (features & MF_NONBATCHED)
		RB_RenderMeshBuffer(&r_2DMBuffer);
}

/*
=============
R_DrawFill
=============
*/
void R_DrawFill(const float x, const float y, const int w, const int h, const colorb &color)
{
	R_DrawPic(ri.media.whiteMaterial, 0, QuadVertices().SetVertices(x, y, w, h), color);
}

/*
===============================================================================

	INIT / SHUTDOWN

===============================================================================
*/

/*
=============
RF_2DInit
=============
*/
void RF_2DInit()
{
	r_2DVertices[0][2] = 1;
	r_2DVertices[1][2] = 1;
	r_2DVertices[2][2] = 1;
	r_2DVertices[3][2] = 1;

	r_2DMesh.numIndexes = 0;
	r_2DMesh.numVerts = 4;

	r_2DMesh.colorArray = r_2DColors;
	r_2DMesh.coordArray = r_2DTexCoords;
	r_2DMesh.indexArray = NULL;
	r_2DMesh.lmCoordArray = NULL;
	r_2DMesh.normalsArray = r_2DNormals;
	r_2DMesh.sVectorsArray = NULL;
	r_2DMesh.tVectorsArray = NULL;
	r_2DMesh.vertexArray = r_2DVertices;

	r_2DMBuffer.sortValue = MBT_2D & (MBT_MAX-1);
	r_2DMBuffer.mesh = NULL;
}

/*
=============
RF_Flush2D
=============
*/
void RF_Flush2D()
{
	if (r_2DMBuffer.mesh)
	{
		RB_RenderMeshBuffer(&r_2DMBuffer);
		r_2DMBuffer.mesh = NULL;
	}
}
