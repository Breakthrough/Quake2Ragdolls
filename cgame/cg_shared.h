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
// cg_shared.h
//

#ifndef __CG_SHARED_H__
#define __CG_SHARED_H__

#define CMD_BACKUP			64					// allow a lot of command backups for very fast systems
#define CMD_MASK			(CMD_BACKUP-1)

#define MAX_REF_DECALS		20000
#define MAX_REF_DLIGHTS		32
#define MAX_REF_ENTITIES	2048 // NOTE: Affects refMeshBuffer->sortValue
#define MAX_REF_POLYS		8192

#define MAX_LENTS			(MAX_REF_ENTITIES/2)	// leave breathing room for normal entities
#define MAX_PARTICLES		8192

/*
=============================================================================

	KEYBOARD

=============================================================================
*/

typedef int keyNum_t;
enum
{
	K_BADKEY		=	-1,

	K_TAB			=	9,
	K_ENTER			=	13,
	K_ESCAPE		=	27,
	K_SPACE			=	32,

	// normal keys should be passed as lowercased ascii
	K_BACKSPACE		=	127,
	K_UPARROW,
	K_DOWNARROW,
	K_LEFTARROW,
	K_RIGHTARROW,

	K_ALT,
	K_CTRL,

	K_SHIFT,
	K_LSHIFT,
	K_RSHIFT,

	K_WINKEY,
	K_LWINKEY,
	K_RWINKEY,

	K_CAPSLOCK,

	K_F1,
	K_F2,
	K_F3,
	K_F4,
	K_F5,
	K_F6,
	K_F7,
	K_F8,
	K_F9,
	K_F10,
	K_F11,
	K_F12,

	K_INS,
	K_DEL,
	K_PGDN,
	K_PGUP,
	K_HOME,
	K_END,

	K_KP_HOME		=	160,
	K_KP_UPARROW,
	K_KP_PGUP,
	K_KP_LEFTARROW,
	K_KP_FIVE,
	K_KP_RIGHTARROW,
	K_KP_END,
	K_KP_DOWNARROW,
	K_KP_PGDN,
	K_KP_ENTER,
	K_KP_INS,
	K_KP_DEL,
	K_KP_SLASH,
	K_KP_MINUS,
	K_KP_PLUS,

	// mouse buttons generate virtual keys
	K_MOUSE1	=		200,
	K_MOUSE2,
	K_MOUSE3,
	K_MOUSE4,
	K_MOUSE5,

	// joystick buttons
	K_JOY1,
	K_JOY2,
	K_JOY3,
	K_JOY4,

	/*
	** aux keys are for multi-buttoned joysticks to generate so that
	** they can use the normal binding process
	*/
	K_AUX1,
	K_AUX2,
	K_AUX3,
	K_AUX4,
	K_AUX5,
	K_AUX6,
	K_AUX7,
	K_AUX8,
	K_AUX9,
	K_AUX10,
	K_AUX11,
	K_AUX12,
	K_AUX13,
	K_AUX14,
	K_AUX15,
	K_AUX16,
	K_AUX17,
	K_AUX18,
	K_AUX19,
	K_AUX20,
	K_AUX21,
	K_AUX22,
	K_AUX23,
	K_AUX24,
	K_AUX25,
	K_AUX26,
	K_AUX27,
	K_AUX28,
	K_AUX29,
	K_AUX30,
	K_AUX31,
	K_AUX32,

	K_MWHEELDOWN,
	K_MWHEELUP,
	K_MWHEELLEFT,
	K_MWHEELRIGHT,

	K_PAUSE			=	255,

	K_MAXKEYS
};

enum EKeyDest
{
	KD_MINDEST		=	0,

	KD_GAME			=	0,
	KD_CONSOLE		=	1,
	KD_MESSAGE		=	2,
	KD_MENU			=	3,

	KD_MAXDEST		=	3
};

/*
=============================================================================

	CONSOLE COMMANDS

=============================================================================
*/

class conCmd_t
{
};

/*
=============================================================================

	ENTITY

=============================================================================
*/

#define MAX_PARSE_ENTITIES			1024
#define MAX_PARSEENTITIES_MASK		(MAX_PARSE_ENTITIES-1)

// ==========================================================================

struct netFrame_t
{
	bool					valid;			// cleared if delta parsing was invalid
	int						serverFrame;
	int						serverTime;		// server time the message is valid for (in msec)
	int						deltaFrame;
	byte					areaBits[MAX_AREA_BITS];		// portalarea visibility bits
	playerState_t		playerState;
	int						numEntities;
	int						parseEntities;	// non-masked index into cg_parseEntities array
};

// ==========================================================================

struct refEntity_t
{
	struct refModel_t		*model;			// Opaque type outside refresh

	struct refMaterial_t	*skin;			// NULL for inline skin
	int						skinNum;
	float					matTime;

	mat3x3_t				axis;

	vec3_t					origin;
	vec3_t					oldOrigin;

	int						frame;
	int						oldFrame;

	float					scale;

	float					backLerp;		// 0.0 = current, 1.0 = old

	colorb					color;

	int						flags;
};

/*
=============================================================================

	DLIGHTS

=============================================================================
*/

struct refDLight_t
{
	vec3_t					origin;

	vec3_t					color;
	float					intensity;

	vec3_t					mins;
	vec3_t					maxs;
};

struct refLightStyle_t
{
	float					rgb[3];			// 0.0 - 2.0
	float					white;			// highest of rgb
};

/*
=============================================================================

	EFFECTS

=============================================================================
*/

struct refPoly_t
{
	int						numVerts;

	vec3_t					origin;
	float					radius;

	vec3_t					*vertices;
	vec2_t					*texCoords;
	colorb					*colors;

	struct refMaterial_t	*mat;
	float					matTime;
};

struct refDecal_t
{
	// Rendering data
	uint32					numIndexes;
	int						*indexes;

	vec3_t					*normals;

	refPoly_t				poly;			// This data should *not* be touched by CGame, R_Create/Free/AddDecal handle this data.

	// For culling
	uint32					numSurfaces;
	struct mBspSurface_t	**surfaces;
};

/*
=============================================================================

	GUI INFORMATION

=============================================================================
*/

typedef enum guiVarType_s {
	GVT_FLOAT,
	GVT_STR,
	GVT_STR_PTR,
	GVT_VEC
} guiVarType_t;

/*
=============================================================================

	REFRESH DEFINITION

=============================================================================
*/

struct refExtensionConfig_t
{
	bool					bARBMultitexture;
	bool					bSGISMultiTexture;

	bool					bBGRA;
	bool					bCompiledVertArray;

	bool					bDrawRangeElements;
	int						maxElementVerts;
	int						maxElementIndices;

	bool					bFrameBufferObject;

	bool					bPrograms;
	int						maxTexCoords;
	int						maxTexImageUnits;

	bool					bShaderObjects;

	bool					bShaders;
	int						maxVertexUniforms;
	int						maxVaryingFloats;
	int						maxVertexAttribs;
	int						maxFragmentUniforms;

	bool					bDepthTexture;
	bool					bARBShadow;

	bool					bStencilTwoSide;
	bool					bStencilWrap;

	bool					bTex3D;
	int						max3DTexSize;

	bool					bTexCompression;

	bool					bTexCubeMap;
	int						maxCMTexSize;

	bool					bTexEdgeClamp;
	bool					bTexEnvAdd;
	bool					bTexEnvCombine;
	bool					bTexEnvDot3;

	bool					bTexFilterAniso;
	int						maxAniso;

	bool					bVertexBufferObject;
	bool					bWinSwapInterval;
};

struct refConfig_t
{
	// Gamma ramp
	bool					bHWGammaAvail;
	bool					bHWGammaInUse;

	// Extensions
	refExtensionConfig_t	ext;

	// GL Queries
	int						maxTexSize;
	int						maxTexUnits;

	// Video
	int						vidWidth;
	int						vidHeight;
	bool					vidFullScreen;
	int						vidFrequency;
	int						vidBitDepth;
	bool					stereoEnabled;
};

// ==========================================================================

struct refDef_t
{
	float					time;				// time is used to auto animate

	int						x, y;
	int						width, height;

	float					fovX, fovY;

	vec3_t					viewOrigin;
	vec3_t					velocity;			// primarily used for the audio system

	vec3_t					viewAngles;
	mat3x3_t				viewAxis;			// Forward, left, up

	int						rdFlags;			// RDF_NOWORLDMODEL, etc

	byte					*areaBits;			// if not NULL, only areas with set bits will be drawn
};

#endif // __CG_SHARED_H__
