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
// rf_public.h
// Public to the entire renderer, not necessarily to the entire engine.
//

#include "rf_register.h"

/*
=============================================================================

	MESH BUFFERING

=============================================================================
*/

typedef uint32 dlightBit_t;
typedef uint32 shadowBit_t;

#define MAX_MESH_BUFFER			16384
#define MAX_ADDITIVE_BUFFER		16384
#define MAX_POSTPROC_BUFFER		64

typedef uint16 meshFeatures_t;
enum
{
	MF_NORMALS				= BIT(0),
	MF_STCOORDS				= BIT(1),
	MF_LMCOORDS				= BIT(2),
	MF_COLORS				= BIT(3),
	MF_NOCULL				= BIT(4),
	MF_DEFORMVS				= BIT(5),
	MF_STVECTORS			= BIT(6),

	MF_TRIFAN				= BIT(7),
	MF_ENABLENORMALS		= BIT(8),
	MF_NONBATCHED			= BIT(9),
	MF_KEEPLOCK				= BIT(10),
};

enum EMeshBufferType
{
	MBT_BAD,

	MBT_2D,
	MBT_ALIAS,
	MBT_DECAL,
	MBT_INTERNAL,
	MBT_POLY,
	MBT_Q2BSP,
	MBT_Q3BSP,
	MBT_Q3BSP_FLARE,
	MBT_SKY,
	MBT_SP2,

	MBT_MAX					= 16
};

struct refMesh_t
{
	int						numIndexes;
	int						numVerts;

	colorb					*colorArray;
	vec2_t					*coordArray;
	vec2_t					*lmCoordArray;
	index_t					*indexArray;
	vec3_t					*normalsArray;
	vec3_t					*sVectorsArray;
	vec3_t					*tVectorsArray;
	vec3_t					*vertexArray;
};

class refMeshBuffer
{
public:
	uint64					sortValue;
	float					matTime;
	void					*mesh;
	int						infoKey;

	void Setup(const EMeshBufferType inMeshType, void *inMeshData, refMaterial_t *inMaterial, const float inMatTime, refEntity_t *inEntity, struct mQ3BspFog_t *inFog, const int inInfoKey);

	refEntity_t *DecodeEntity();
	struct mQ3BspFog_t *DecodeFog();
	refMaterial_t *DecodeMaterial();

	inline EMeshBufferType DecodeMeshType() { return (EMeshBufferType)(sortValue & (MBT_MAX-1)); }
};

class refMeshList
{
public:
	bool					bSkyDrawn;

	TList<refMeshBuffer>	meshBufferOpaque;
	TList<refMeshBuffer>	meshBufferAdditive;
	TList<refMeshBuffer>	meshBufferPostProcess;

	inline void Clear()
	{
		meshBufferOpaque.Clear();
		meshBufferAdditive.Clear();
		meshBufferPostProcess.Clear();
	}

	refMeshBuffer *AddToList(const EMeshBufferType meshType, void *mesh, refMaterial_t *mat, const float matTime, refEntity_t *ent, struct mQ3BspFog_t *fog, const int infoKey);
	void SortList();
	void DrawList();
	void DrawOutlines();

private:
	void BatchMeshBuffer(refMeshBuffer *mb, refMeshBuffer *nextMB);
};

/*
=============================================================================

	FUNCTION PROTOTYPES

=============================================================================
*/

//
// rf_light.cpp
//

void R_LightForEntity(refEntity_t *ent, vec3_t *vertexArray, const int numVerts, vec3_t *normalArray, byte *colorArray);

//
// rf_main.cpp
//

void R_DrawStats();
