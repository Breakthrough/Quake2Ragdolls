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
// r_public.h
// Client-refresh global header
//

#include "../common/common.h"
#include "../cgame/cg_shared.h"

#ifndef __REFRESH_H__
#define __REFRESH_H__

/*
=============================================================================

	CINEMATIC PLAYBACK

=============================================================================
*/

#define RoQ_HEADER1			4228
#define RoQ_HEADER2			-1
#define RoQ_HEADER3			30

#define RoQ_FRAMERATE		30

#define RoQ_INFO			0x1001
#define RoQ_QUAD_CODEBOOK	0x1002
#define RoQ_QUAD_VQ			0x1011
#define RoQ_SOUND_MONO		0x1020
#define RoQ_SOUND_STEREO	0x1021

#define RoQ_ID_MOT			0x00
#define RoQ_ID_FCC			0x01
#define RoQ_ID_SLD			0x02
#define RoQ_ID_CCC			0x03

struct roqChunk_t
{
	uint16					id;
	uint32					size;
	uint16					arg;
};

struct roqCell_t
{
	byte					y[4], u, v;
};

struct roqQCell_t
{
	byte					idx[4];
};

struct cinematic_t
{
	int						time;
	int						frameNum;

	fileHandle_t			fileNum;		// File handle to the open cinematic

	byte					*frames[2];

	int						width;			// Width of the cinematic
	int						height;			// Height of the cinematic
	uint32					*vidBuffer;		// Written to for rendering each frame

							// Audio settings
	bool					sndRestart;

	byte					*sndBuffer;
	int						sndRate;
	int						sndWidth;
	int						sndChannels;

	struct channel_t		*sndRawChannel;
	bool					sndAL;

							// RoQ information
	roqChunk_t				roqChunk;
	roqCell_t				roqCells[256];
	roqQCell_t				roqQCells[256];

	byte					*roqBuffer;

							// Cinematic information
	uint32					hPalette[256];

	byte					*hBuffer;		// Buffer for decompression
	int						*hNodes;
	int						hNumNodes[256];

	int						hUsed[512];
	int						hCount[512];
};

/*
=============================================================================

	FUNCTION PROTOTYPES

=============================================================================
*/

//
// rf_2d.cpp
//

void RF_Flush2D();
void R_DrawPic(struct refMaterial_t *mat, const float matTime,
	const QuadVertices &vertices,
	const colorb &color);
void R_DrawFill(const float x, const float y, const int w, const int h, const colorb &color);

//
// rf_decal.cpp
//

bool R_CreateDecal(refDecal_t *d, struct refMaterial_t *material, vec4_t subUVs, vec3_t origin, vec3_t direction, float angle, float size);
bool R_FreeDecal(refDecal_t *d);

//
// rf_font.cpp
//

struct refFont_t *R_RegisterFont(char *name);
void R_GetFontDimensions(struct refFont_t *font, float xScale, float yScale, uint32 flags, vec2_t dest);

void R_DrawChar(struct refFont_t *font, float x, float y, float xScale, float yScale, uint32 flags, int num, const colorb &color);
int R_DrawString(struct refFont_t *font, float x, float y, float xScale, float yScale, uint32 flags, char *string, const colorb &color);
int R_DrawStringLen(struct refFont_t *font, float x, float y, float xScale, float yScale, uint32 flags, char *string, int len, const colorb &color);

//
// rf_image.cpp
//

bool R_UpdateTexture(const char *Name, byte *Data, int xOffset, int yOffset, int Width, int Height);
void R_GetImageSize(struct refMaterial_t *mat, int *width, int *height);

//
// rf_init.cpp
//

void R_MediaInit();

bool R_InitRefresh();
void R_ShutdownRefresh(const bool full);

//
// rf_light.cpp
//

void R_LightPoint(vec3_t point, vec3_t light);

//
// rf_main.cpp
//

void R_RenderScene(refDef_t *rd);
void R_BeginFrame(const float cameraSeparation);
void R_EndFrame();

void R_BeginTimeDemo();
void R_TimeDemoFrame();
void R_EndTimeDemo();

void R_ClearScene();

void R_AddDecal(refDecal_t *decal, const colorb &color, float materialTime);
void R_AddEntity(refEntity_t *ent);
void R_AddPoly(refPoly_t *poly);
void R_AddLight(vec3_t org, float intensity, float r, float g, float b);
void R_AddLightStyle(int style, float r, float g, float b);

void R_GetRefConfig(refConfig_t *outConfig);

void R_TransformVectorToScreen(refDef_t *rd, vec3_t in, vec2_t out);

//
// rf_model.cpp
//

void R_RegisterMap(const char *mapName);
struct refModel_t *R_RegisterModel(const char *name);
void R_ModelBounds(struct refModel_t *model, vec3_t mins, vec3_t maxs);
refAliasAnimation_t *R_GetModelAnimation (struct refModel_t *model, int index);

//
// rf_material.cpp
//

struct refMaterial_t *R_RegisterPic(const char *name);
struct refMaterial_t *R_RegisterPoly(const char *name);
struct refMaterial_t *R_RegisterSkin(const char *name);

//
// rf_register.cpp
//

void R_BeginRegistration();
void R_EndRegistration();

//
// rf_sky.cpp
//

void R_SetSky(char *name, float rotate, vec3_t axis);

#endif // __REFRESH_H__
