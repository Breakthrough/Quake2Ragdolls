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
// rf_model.h
// Memory representation of the different model types
//

struct mModelDataBase_t
{
};

/*
==============================================================================

	ALIAS MODELS

==============================================================================
*/

#define ALIAS_MAX_LODS		4

struct mAliasFrame_t
{
	vec3_t					mins;
	vec3_t					maxs;

	vec3_t					scale;
	vec3_t					translate;
	float					radius;
};

struct mAliasSkin_t
{
	char					name[MAX_QPATH];
	refMaterial_t			*material;
};

struct mAliasTag_t
{
	char					name[MAX_QPATH];
	quat_t					quat;
	vec3_t					origin;
};

struct mAliasVertex_t
{
	sint16					point[3];
	byte					latLong[2];
};

struct mAliasMesh_t
{
	char					name[MAX_QPATH];

	vec3_t					*mins;
	vec3_t					*maxs;
	float					*radius;

	int						numVerts;
	mAliasVertex_t			*vertexes;
	vec2_t					*coords;

	int						numTris;
	index_t					*indexes;

	int						numSkins;
	mAliasSkin_t			*skins;
};

struct mAliasLod_t
{
	float					distance;
	struct refModel_t		*model;
};

struct mAliasModel_t : public mModelDataBase_t
{
	int						numFrames;
	mAliasFrame_t			*frames;

	int						numTags;
	mAliasTag_t				*tags;

	int						numMeshes;
	mAliasMesh_t			*meshes;

	int						numAnims;
	refAliasAnimation_t		*modelAnimations;

	inline const refAliasAnimation_t *FindAnimation (const char *name)
	{
		for (int i = 0; i < numAnims; ++i)
		{
			if (modelAnimations[i].Name.CompareCaseInsensitive(name) == 0)
				return &modelAnimations[i];
		}

		return null;
	}

	int						numLODs;
	mAliasLod_t				*modelLODs;
};

/*
========================================================================

	SPRITE MODELS

========================================================================
*/

struct mSpriteFrame_t
{
							// Dimensions
	int						width;
	int						height;

	float					radius;

							// Raster coordinates inside pic
	int						originX;
	int						originY;

							// Texturing
	char					name[MAX_QPATH];	// name of pcx file
	refMaterial_t			*skin;
};

struct mSpriteModel_t : public mModelDataBase_t
{
	int						numFrames;
	mSpriteFrame_t			*frames;			// variable sized
};

/*
==============================================================================

	QUAKE2 AND QUAKE3 BSP BRUSH MODELS

==============================================================================
*/

#define SIDE_FRONT			0
#define SIDE_BACK			1
#define SIDE_ON				2

#define SURF_PLANEBACK		2
#define SURF_DRAWSKY		4
#define SURF_DRAWTURB		0x0010
#define SURF_DRAWBACKGROUND	0x0040
#define SURF_UNDERWATER		0x0080
#define SURF_LAVA			0x0100
#define SURF_SLIME			0x0200
#define SURF_WATER			0x0400

// ===========================================================================
//
// Q3BSP
//

struct mQ3BspMaterialRef_t
{
	char					name[MAX_QPATH];
	int						flags;
	int						contents;
	refMaterial_t			*mat;
};

struct mQ3BspFog_t
{
	char					name[MAX_QPATH];
	refMaterial_t			*mat;

	plane_t					*visiblePlane;

	int						numPlanes;
	plane_t					*planes;
};

struct mQ3BspGridLight_t
{
	byte					ambient[3];
	byte					diffuse[3];
	byte					direction[2];
};

struct mQ3BspLightmapRect_t
{
	int						texNum;

	float					x, y;
	float					w, h;
};

// ===========================================================================
//
// Q2BSP
//

struct mQ2BspTexInfo_t
{
	char					texName[MAX_QPATH];
	refMaterial_t			*mat;

	int						width;
	int						height;

	float					vecs[2][4];
	int						flags;
	int						numFrames;

	int						surfParams;

	struct mQ2BspTexInfo_t	*next;		// animation chain
};

struct mQ2BspPoly_t
{
	struct mQ2BspPoly_t		*next;
	refMesh_t				mesh;
};

// ===========================================================================
//
// Q2 Q3 BSP COMMON
//

struct mBspSurface_t
{
	// Quake2 BSP specific
	plane_t					*q2_plane;
	int						q2_flags;

	int						q2_firstEdge;		// look up in model->surfedges[], negative numbers
	int						q2_numEdges;		// are backwards edges

	svec2_t					q2_textureMins;
	svec2_t					q2_extents;

	mQ2BspPoly_t			*q2_polys;			// multiple if subdivided
	mQ2BspTexInfo_t			*q2_texInfo;

	ivec2_t					q2_dLightCoords;	// gl lightmap coordinates for dynamic lightmaps

	uint32					q2_dLightUpdateFrame; // last frame that the dynamic light was updated
	bool					q2_bDLightDirty;	// had dynamic lights written to it last update
	ivec2_t					q2_lmCoords;		// gl lightmap coordinates
	int						q2_lmWidth;
	int						q2_lmHeight;
	byte					*q2_lmSamples;		// [numstyles*surfsize]

	int						q2_numStyles;
	byte					q2_styles[Q2BSP_MAX_LIGHTMAPS];
	float					q2_cachedLight[Q2BSP_MAX_LIGHTMAPS];	// values currently used in lightmap

	// Quake3 BSP specific
	uint32					q3_nodeFrame;		// used so we don't have to recurse EVERY frame

	int						q3_faceType;

	mQ3BspFog_t				*q3_fog;
	mQ3BspMaterialRef_t		*q3_matRef;

    vec3_t					q3_origin;

	uint32					q3_patchWidth;
	uint32					q3_patchHeight;

	// Common between Quake2 and Quake3 BSP formats
	uint32					visFrame;			// for efficient decal culling

	vec3_t					mins, maxs;

	refMesh_t				*mesh;

	uint32					fragmentFrame;

	int						lmTexNum;
	uint32					dLightFrame;
	dlightBit_t				dLightBits;

	uint32					shadowFrame;
	shadowBit_t				shadowBits;
};

struct mNodeLeafShared_t
{
	uint32					visFrame;			// node needs to be traversed if current

	plane_t					*plane;				// Q2BSP uses this in nodes, Q3BSP uses this in leafs
	int						q2_contents;		// -1, to differentiate from leafs

	// For bounding box culling
	bool					badBounds;
	vec3_t					mins;
	vec3_t					maxs;

	struct mBspNode_t		*parent;
};

struct mBspNode_t : public mNodeLeafShared_t
{
	mBspNode_t				*children[2];	

	uint16					q2_firstSurface;
	uint16					q2_numSurfaces;

	mBspSurface_t			**q2_firstVisSurface;
	mBspSurface_t			**q2_firstLitSurface;
};

struct mBspLeaf_t : public mNodeLeafShared_t
{
	// Leaf specific
	int						cluster;
	int						area;

	int						q2_numMarkSurfaces;
	mBspSurface_t			**q2_firstMarkSurface;

	mBspSurface_t			**q3_firstVisSurface;
	mBspSurface_t			**q3_firstLitSurface;

	mBspSurface_t			**firstFragmentSurface;
};

struct mBspVisBase_t
{
	int						numClusters;
};

struct mQ2BspVis_t : public mBspVisBase_t
{
	int						bitOfs[8][2];	// bitofs[numclusters][2]
};

struct mQ3BspVis_t : public mBspVisBase_t
{
	int						rowSize;
	byte					data[1];
};

struct mBspHeader_t
{
	vec3_t					mins, maxs;
	float					radius;
	vec3_t					origin;		// for sounds or lights

	int						firstFace;
	int						numFaces;

	// Quake2 BSP specific
	int						q2_headNode;
	int						q2_visLeafs;	// not including the solid leaf 0
};

/*
==============================================================================

	BASE MODEL STRUCT

==============================================================================
*/

enum EModelType
{
	MODEL_BAD,

	MODEL_INTERNAL,
	MODEL_Q2BSP,
	MODEL_Q3BSP,
	MODEL_MD2,
	MODEL_MD3,
	MODEL_SP2
};

struct mInternalModel_t : public mModelDataBase_t
{
	refMesh_t				*mesh;
};

// Common between Quake2 and Quake3 BSP formats
struct mBspModelBase_t : public mModelDataBase_t
{
	uint32					numSubModels;
	mBspHeader_t			*subModels;
	struct refModel_t		*inlineModels;

	mBspSurface_t			**firstModelVisSurface;
	mBspSurface_t			**firstModelLitSurface;

	int						numLeafs;		// number of visible leafs, not counting 0
	mBspLeaf_t				*leafs;

	int						numNodes;
	mBspNode_t				*nodes;

	int						numPlanes;
	plane_t					*planes;

	int						numSurfaces;
	mBspSurface_t			*surfaces;
};

struct mQ2BspModel_t : public mBspModelBase_t
{
	int						firstNode;

	int						numTexInfo;
	mQ2BspTexInfo_t			*texInfo;

	int						numMarkSurfaces;
	mBspSurface_t			**markSurfaces;

	mQ2BspVis_t				*vis;

	byte					*lightData;
};

struct mQ3BspModel_t : public mBspModelBase_t
{
	vec3_t					gridSize;
	vec3_t					gridMins;
	int						gridBounds[4];

	int						numVertexes;
	vec3_t					*vertexArray;
	vec3_t					*normalsArray;		// normals
	vec2_t					*coordArray;		// texture coords		
	vec2_t					*lmCoordArray;		// lightmap texture coords
	colorb					*colorArray;		// colors used for vertex lighting

	int						numSurfIndexes;
	int						*surfIndexes;

	int						numMatRefs;
	mQ3BspMaterialRef_t		*matRefs;

	int						numLightGridElems;
	mQ3BspGridLight_t		*lightGrid;

	int						numFogs;
	mQ3BspFog_t				*fogs;

	int						numLightmaps;
	mQ3BspLightmapRect_t	*lightmapRects;

	mQ3BspVis_t				*vis;
};

struct refModel_t
{
	char					name[MAX_QPATH];
	char					bareName[MAX_QPATH];

	uint32					touchFrame;

	uint32					memTag;		// memory tag

	uint32					hashValue;
	refModel_t				*hashNext;

	EModelType				type;

	//
	// volume occupied by the model graphics
	//		
	vec3_t					mins;
	vec3_t					maxs;
	float					radius;

	//
	// model rendering information
	//
	mModelDataBase_t		*modelData;

	inline mInternalModel_t *InternalData() { return (mInternalModel_t*)modelData; }
	inline mBspModelBase_t *BSPData() { return (mBspModelBase_t*)modelData; }
	inline mQ2BspModel_t *Q2BSPData() { return (mQ2BspModel_t*)modelData; }
	inline mQ3BspModel_t *Q3BSPData() { return (mQ3BspModel_t*)modelData; }
	inline mAliasModel_t *AliasData() { return (mAliasModel_t*)modelData; }
	inline mSpriteModel_t *SpriteData() { return (mSpriteModel_t*)modelData; }
};

//
// rf_alias.c
//

bool R_CullAliasModel(refEntity_t *ent, const uint32 clipFlags);
void R_AddAliasModelToList(refEntity_t *ent);
void R_DrawAliasModel(refMeshBuffer *mb, const meshFeatures_t features);

//
// rf_model.c
//

void R_BuildTangentVectors(const int numVertexes, const vec3_t *vertexArray, const vec2_t *coordArray, const int numTris, const index_t *indexes, vec3_t *sVectorsArray, vec3_t *tVectorsArray);

void R_EndModelRegistration();

void R_AddInternalModelToList(refEntity_t *ent);
void R_DrawInternalModel(refMeshBuffer *mb, const meshFeatures_t features);

void R_ModelInit();
void R_ModelShutdown();

//
// rf_modelBSP.cpp
//

mBspLeaf_t *R_PointInBSPLeaf(const vec3_t point, refModel_t *model);
byte *R_BSPClusterPVS(const int cluster, refModel_t *model);

mQ3BspFog_t *R_FogForSphere(const vec3_t center, const float radius);

//
// rf_sprite.c
//

void R_AddSP2ModelToList(refEntity_t *ent);
void R_DrawSP2Model(refMeshBuffer *mb, const meshFeatures_t features);

bool R_FlareOverflow();
void R_PushFlare(refMeshBuffer *mb, const meshFeatures_t features);
