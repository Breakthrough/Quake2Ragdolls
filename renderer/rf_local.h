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
// rf_local.h
// Local to the rendering frontend.
//

#include "r_local.h"

#define ENTLIST_OFFSET	2

/*
===============================================================================

	FUNCTION PROTOTYPES

===============================================================================
*/

//
// rf_2d.cpp
//
void RF_2DInit();

//
// rf_cull.cpp
//

void R_SetupFrustum();

bool R_CullBox(const vec3_t mins, const vec3_t maxs, const uint32 clipFlags);
bool R_CullSphere(const vec3_t origin, const float radius, const uint32 clipFlags);
bool R_CullNode(mBspNode_t *node);
bool R_CullSurface(struct mBspSurface_t *surf);

void R_TransformBoundsToBBox(const vec3_t Origin, const mat3x3_t Axis, const vec3_t Mins, const vec3_t Maxs, vec3_t OutBBox[8]);
bool R_CullTransformedBox(const vec3_t BBox[8], const uint32 clipFlags);

static inline bool R_CullTransformedBounds(const vec3_t Origin, const mat3x3_t Axis, const vec3_t Mins, const vec3_t Maxs, const uint32 clipFlags)
{
	vec3_t BBox[8];
	R_TransformBoundsToBBox(Origin, Axis, Mins, Maxs, BBox);
	return R_CullTransformedBox(BBox, clipFlags);
}

//
// rf_decal.cpp
//

void R_AddDecalsToList();
void R_PushDecal(refMeshBuffer *mb, const meshFeatures_t features);
bool R_DecalOverflow(refMeshBuffer *mb);
void R_DecalInit();

//
// rf_entity.cpp
//

void R_CategorizeEntityList();
void R_CullEntityList();

void R_AddBModelsToList();
void R_AddEntitiesToList();

void R_EntityInit();

//
// rf_font.cpp
//

void R_EndFontRegistration();

void R_CheckFont();

void R_FontInit();
void R_FontShutdown();

//
// rf_light.cpp
//

void R_Q2BSP_MarkWorldLights();
void R_Q2BSP_UpdateLightmap(refEntity_t *ent, mBspSurface_t *surf);
void R_Q2BSP_BeginBuildingLightmaps();
void R_Q2BSP_CreateSurfaceLightmap(mBspSurface_t *surf);
void R_Q2BSP_EndBuildingLightmaps();

void R_Q3BSP_MarkWorldLights();

void R_Q3BSP_BuildLightmaps(int numLightmaps, int w, int h, const byte *data, struct mQ3BspLightmapRect_t *rects);

void R_MarkBModelLights(refEntity_t *ent, vec3_t mins, vec3_t maxs);
void R_CullDynamicLightList();
void R_LightBounds(const vec3_t origin, float intensity, vec3_t mins, vec3_t maxs);
void R_SetLightLevel();

void R_TouchLightmaps();

//
// rf_main.cpp
//

void R_TransformToScreen_Vec3(vec3_t in, vec3_t out);

//
// rf_poly.cpp
//
void R_AddPolysToList();
void R_PushPoly(refMeshBuffer *mb, const meshFeatures_t features);
bool R_PolyOverflow(refMeshBuffer *mb);
void R_PolyInit();

//
// rf_sky.cpp
//

void R_ClipSkySurface(mBspSurface_t *surf);
void R_AddSkyToList();
void R_UpdateSkyMesh();

void R_DrawSky(refMeshBuffer *mb);

void R_SetSky(char *name, float rotate, vec3_t axis);

void R_SkyInit();
void R_SkyShutdown();

//
// rf_world.cpp
//

void R_AddQ2BrushModel(refEntity_t *ent);
void R_AddQ3BrushModel(refEntity_t *ent);
bool R_CullBrushModel(refEntity_t *ent, const uint32 clipFlags);
void R_AddWorldToList();

void R_WorldInit();
void R_WorldShutdown();

//
// rf_bloom.cpp
//
void		R_InitBloomTextures( void );
void		R_BloomBlend( const refDef_t *fd );

