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
// rb_public.h
// Refresh backend public header.
// Public to the rest of the renderer, not necessarily to the entire engine.
//

/*
=============================================================================

	BACKEND RENDERER

=============================================================================
*/

#define RB_MAX_VERTS		8192
#define RB_MAX_INDEXES		RB_MAX_VERTS*6
#define RB_MAX_TRIANGLES	RB_MAX_INDEXES/3

// FIXME: this could be made local to the backend
struct rbData_batch_t
{
	colorb					colors[RB_MAX_VERTS];
	vec2_t					coords[RB_MAX_VERTS];
	index_t					indices[RB_MAX_INDEXES];
	vec2_t					lmCoords[RB_MAX_VERTS];
	vec3_t					normals[RB_MAX_VERTS];
	vec3_t					sVectors[RB_MAX_VERTS];
	vec3_t					tVectors[RB_MAX_VERTS];
	vec3_t					vertices[RB_MAX_VERTS];
};

struct rbData_t
{
	// Batch buffers are used for MAT_ENTITY_MERGABLE materials and for
	rbData_batch_t			batch;

	// Output containers for rendering
	colorb					outColorArray[RB_MAX_VERTS];
	vec2_t					outCoordArray[MAX_TEXUNITS][RB_MAX_VERTS];

	// Input data pointers for rendering a mesh buffer
	colorb					*inColors;
	vec2_t					*inCoords;
	index_t					*inIndices;
	vec2_t					*inLMCoords;
	vec3_t					*inNormals;
	vec3_t					*inSVectors;
	vec3_t					*inTVectors;
	vec3_t					*inVertices;

	refEntity_t				*curEntity;
	dlightBit_t				curDLightBits;
	shadowBit_t				curShadowBits;
	int						curLMTexNum;
	meshFeatures_t			curMeshFeatures;
	EMeshBufferType			curMeshType;
	uint32					curPatchWidth;
	uint32					curPatchHeight;
	refMaterial_t			*curMat;

	struct mQ3BspFog_t		*curColorFog;
	struct mQ3BspFog_t		*curTexFog;

	// Total for the next flush
	int						numIndexes;
	int						numVerts;
};

extern rbData_t	rb;

float RB_FastSin(const float t);

void RB_StartRendering();
void RB_RenderMeshBuffer(refMeshBuffer *mb);
void RB_FinishRendering();

void RB_BeginFrame();
void RB_EndFrame();

/*
=============================================================================

	STATE COMPRESSION

=============================================================================
*/

typedef uint64 stateBit_t;

#define SB_BLENDSRC_ZERO					0x1
#define SB_BLENDSRC_ONE						0x2
#define SB_BLENDSRC_DST_COLOR				0x3
#define SB_BLENDSRC_ONE_MINUS_DST_COLOR		0x4
#define SB_BLENDSRC_SRC_ALPHA				0x5
#define SB_BLENDSRC_ONE_MINUS_SRC_ALPHA		0x6
#define SB_BLENDSRC_DST_ALPHA				0x7
#define SB_BLENDSRC_ONE_MINUS_DST_ALPHA		0x8
#define SB_BLENDSRC_SRC_ALPHA_SATURATE		0x9
#define SB_BLENDSRC_MASK					0xf

#define SB_BLENDDST_ZERO					0x10
#define SB_BLENDDST_ONE						0x20
#define SB_BLENDDST_SRC_COLOR				0x30
#define SB_BLENDDST_ONE_MINUS_SRC_COLOR		0x40
#define SB_BLENDDST_SRC_ALPHA				0x50
#define SB_BLENDDST_ONE_MINUS_SRC_ALPHA		0x60
#define SB_BLENDDST_DST_ALPHA				0x70
#define SB_BLENDDST_ONE_MINUS_DST_ALPHA		0x80
#define SB_BLENDDST_MASK					0xf0

#define SB_BLEND_MTEX						0x100

#define SB_DEPTHMASK_ON						0x200
#define SB_DEPTHTEST_ON						0x400
#define SB_POLYOFFSET_ON					0x800

#define SB_CULL_FRONT						0x1000
#define SB_CULL_BACK						0x2000
#define SB_CULL_MASK						0xf000

#define SB_ATEST_GT0						0x10000
#define SB_ATEST_LT128						0x20000
#define SB_ATEST_GE128						0x40000
#define SB_ATEST_MASK						0xf0000

#define SB_DEPTHFUNC_EQ						0x100000
#define SB_DEPTHFUNC_GEQ					0x200000
#define SB_DEPTHFUNC_MASK					0xf00000

#define SB_FRAGMENT_PROGRAM_ON				0x1000000
#define SB_VERTEX_PROGRAM_ON				0x2000000
#define SB_FRONTFACE_CW						0x4000000
#define SB_SHADEMODEL_FLAT					0x8000000
#define SB_DEPTHRANGE_HACK					0x10000000

#define SB_COLORMASK_RED					0x100000000
#define SB_COLORMASK_GREEN					0x200000000
#define SB_COLORMASK_BLUE					0x400000000
#define SB_COLORMASK_ALPHA					0x800000000
#define SB_COLORMASK_MASK					0xf00000000

#define SB_DEFAULT_2D						(0)
#define SB_DEFAULT_3D						(SB_DEPTHTEST_ON|SB_DEPTHMASK_ON)

void RB_TextureTarget(const GLenum target);
void RB_SelectTexture(const ETextureUnit texUnit);
void RB_TextureEnv(GLfloat mode);
void RB_BindTexture(refImage_t *image);
void RB_BeginFBOUpdate(refImage_t *FBOImage);
void RB_EndFBOUpdate();

void RB_SetupProjectionMatrix(refDef_t *rd, mat4x4_t m);
void RB_SetupModelviewMatrix(refDef_t *rd, mat4x4_t m);
void RB_SetupViewMatrixes();
void RB_LoadModelIdentity();
void RB_RotateForEntity(refEntity_t *ent);

bool RB_In2DMode();
void RB_SetupGL2D();
void RB_SetupGL3D();
void RB_EndGL3D();

void RB_ClearBuffers(const GLbitfield bitMask);

void RB_CheckForError(const char *Where);
void RB_SetDefaultState();

/*
=============================================================================

	FUNCTION PROTOTYPES

=============================================================================
*/

//
// rb_batch.cpp
//

inline bool RB_BackendOverflow(const int numVerts, const int numIndexes)
{
	return (rb.numVerts + numVerts > RB_MAX_VERTS || rb.numIndexes + numIndexes > RB_MAX_INDEXES);
}

inline bool RB_BackendOverflow(const refMesh_t *Mesh)
{
	return (rb.numVerts + Mesh->numVerts > RB_MAX_VERTS || rb.numIndexes + Mesh->numIndexes > RB_MAX_INDEXES);
}

inline bool RB_BackendOverflow2(const refMesh_t *Mesh1, const refMesh_t *Mesh2)
{
	return (rb.numVerts + Mesh1->numVerts + Mesh2->numVerts > RB_MAX_VERTS || rb.numIndexes + Mesh1->numIndexes + Mesh2->numIndexes > RB_MAX_INDEXES);
}

inline bool RB_InvalidMesh(const int numVerts, const int numIndexes)
{
	return (numVerts <= 0 || numVerts > RB_MAX_VERTS || numIndexes <= 0 || numIndexes > RB_MAX_INDEXES);
}

inline bool RB_InvalidMesh(const refMesh_t *Mesh)
{
	return (Mesh->numVerts <= 0 || Mesh->numVerts > RB_MAX_VERTS || Mesh->numIndexes <= 0 || Mesh->numIndexes > RB_MAX_INDEXES);
}

void RB_PushMesh(refMesh_t *mesh, const meshFeatures_t meshFeatures);

//
// rb_init.cpp
//

bool RB_BackendInit();
bool RB_PostFrontEndInit();
void RB_BackendShutdown(const bool bFull);

//
// rb_light.cpp
//

void RB_DrawDLights();

//
// rb_shadow.cpp
//

#define MAX_SHADOW_GROUPS 32

void RB_ClearShadowMaps();
void RB_AddShadowCaster(refEntity_t *ent);
void RB_CullShadowMaps();
void RB_RenderShadowMaps();
