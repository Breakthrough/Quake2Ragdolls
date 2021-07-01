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
// rf_material.h
//

/*
=============================================================================

	MATERIALS

=============================================================================
*/

#define MAX_MATERIALS				4096
#define MAX_MATERIAL_DEFORMVS		8
#define MAX_MATERIAL_PASSES			8
#define MAX_MATERIAL_ANIM_FRAMES	16
#define MAX_MATERIAL_TCMODS			8
#define MAX_MATERIAL_PROG_PARMS		8

// Material pass flags
typedef uint16 matPassFlags_t;
enum
{
    MAT_PASS_ANIMMAP			= BIT(0),
    MAT_PASS_BLEND				= BIT(1),
	MAT_PASS_CUBEMAP			= BIT(2),
	MAT_PASS_DEPTHWRITE			= BIT(3),
	MAT_PASS_DETAIL				= BIT(4),
    MAT_PASS_LIGHTMAP			= BIT(5),
	MAT_PASS_NOTDETAIL			= BIT(6),
	MAT_PASS_NOCOLORARRAY		= BIT(7),
	MAT_PASS_NOTEXTURING		= BIT(8),

	MAT_PASS_DLIGHT				= BIT(9),
	MAT_PASS_SHADOWMAP			= BIT(10),
	MAT_PASS_FRAGMENTPROGRAM	= BIT(11),
	MAT_PASS_VERTEXPROGRAM		= BIT(12),
	MAT_PASS_SHADER				= BIT(13),
};

#define MAT_PASS_PROGRAM_MASK (MAT_PASS_FRAGMENTPROGRAM|MAT_PASS_VERTEXPROGRAM)

// Material pass tcGen functions
enum EMatPass_TCGen
{
	TC_GEN_NONE,

	TC_GEN_BASE,
	TC_GEN_LIGHTMAP,
	TC_GEN_ENVIRONMENT,
	TC_GEN_VECTOR,
	TC_GEN_REFLECTION,
	TC_GEN_WARP,

	// The following are used internally ONLY!
	TC_GEN_DLIGHT,
	TC_GEN_SHADOWMAP,
	TC_GEN_FOG
};

// Periodic functions
enum EMatTableFunc
{
	MAT_FUNC_BAD,
    MAT_FUNC_SIN,
    MAT_FUNC_TRIANGLE,
    MAT_FUNC_SQUARE,
    MAT_FUNC_SAWTOOTH,
    MAT_FUNC_INVERSESAWTOOTH,
	MAT_FUNC_NOISE,
	MAT_FUNC_CONSTANT
};

struct materialFunc_t
{
    EMatTableFunc		type;			// MAT_FUNC enum
    float				args[4];		// offset, amplitude, phase_offset, rate
};

// Material pass tcMod functions
enum EMatPass_TCMod
{
	TC_MOD_NONE,
	TC_MOD_SCALE,
	TC_MOD_SCROLL,
	TC_MOD_ROTATE,
	TC_MOD_TRANSFORM,
	TC_MOD_TURB,
	TC_MOD_STRETCH
};

struct tcMod_t
{
	EMatPass_TCMod		type;
	float				args[6];
};

// Material pass rgbGen functions
enum EMatPass_RGBGen
{
	RGB_GEN_UNKNOWN,
	RGB_GEN_IDENTITY,
	RGB_GEN_IDENTITY_LIGHTING,
	RGB_GEN_CONST,
	RGB_GEN_COLORWAVE,
	RGB_GEN_ENTITY,
	RGB_GEN_ONE_MINUS_ENTITY,
	RGB_GEN_EXACT_VERTEX,
	RGB_GEN_VERTEX,
	RGB_GEN_ONE_MINUS_VERTEX,
	RGB_GEN_ONE_MINUS_EXACT_VERTEX,
	RGB_GEN_LIGHTING_DIFFUSE,
	RGB_GEN_FOG
};

struct rgbGen_t
{
	EMatPass_RGBGen			type;
	float					fArgs[3];
	byte					bArgs[3];
    materialFunc_t			func;
};

// Material pass alphaGen functions
enum EMatPass_AlphaGen
{
	ALPHA_GEN_UNKNOWN,
	ALPHA_GEN_IDENTITY,
	ALPHA_GEN_CONST,
	ALPHA_GEN_PORTAL,
	ALPHA_GEN_VERTEX,
	ALPHA_GEN_ONE_MINUS_VERTEX,
	ALPHA_GEN_ENTITY,
	ALPHA_GEN_SPECULAR,
	ALPHA_GEN_WAVE,
	ALPHA_GEN_DOT,
	ALPHA_GEN_ONE_MINUS_DOT,
	ALPHA_GEN_FOG
};

struct alphaGen_t
{
	EMatPass_AlphaGen		type;
	float					args[2];
    materialFunc_t			func;
};

enum EMatPass_ProgParmType
{
	PPTYPE_CONST,
	PPTYPE_VIEW_ORIGIN,
	PPTYPE_VIEW_ANGLES,
	PPTYPE_VIEW_AXIS_FWD,
	PPTYPE_VIEW_AXIS_LEFT,
	PPTYPE_VIEW_AXIS_UP,
	PPTYPE_ENTITY_ORIGIN,
	PPTYPE_ENTITY_ANGLES,
	PPTYPE_ENTITY_AXIS_FWD,
	PPTYPE_ENTITY_AXIS_LEFT,
	PPTYPE_ENTITY_AXIS_UP,
	PPTYPE_TIME,

	PPTYPE_MAX
};

struct progParm_t
{
	byte					index;
	EMatPass_ProgParmType	types[4];
	vec4_t					values;	// For PPTYPE_CONSTANTS only!
};

//
// Material passes
//
struct matPass_t
{
	int							animFPS;

								// For material registration
	byte						animNumImages;
	refImage_t					*animImages[MAX_MATERIAL_ANIM_FRAMES];

								// For material creation (static after material is finished)
	byte						animNumNames;
	char						*animNames[MAX_MATERIAL_ANIM_FRAMES];
	texFlags_t					animTexFlags[MAX_MATERIAL_ANIM_FRAMES];
	texFlags_t					passTexFlags;

	refProgram_t				*vertProgPtr;
	char						vertProgName[MAX_QPATH];
	byte						numVertParms;
	progParm_t					vertParms[MAX_MATERIAL_PROG_PARMS];

	refProgram_t				*fragProgPtr;
	char						fragProgName[MAX_QPATH];
	byte						numFragParms;
	progParm_t					fragParms[MAX_MATERIAL_PROG_PARMS];

	refShader_t					*shaderPtr;
	char						shaderName[MAX_QPATH];

	matPassFlags_t				flags;

	alphaGen_t					alphaGen;
	rgbGen_t					rgbGen;

	EMatPass_TCGen				tcGen;
	vec4_t						tcGenVec[2];

	byte						numTCMods;
	tcMod_t						*tcMods;

	uint32						blendMode;

	stateBit_t					stateBits;
};

// Material path types
enum EMatPathType
{
	MAT_PATHTYPE_INTERNAL,
	MAT_PATHTYPE_BASEDIR,
	MAT_PATHTYPE_MODDIR
};

// Material registration types
enum EMatRegType
{
	MAT_RT_ALIAS,
	MAT_RT_BSP,
	MAT_RT_BSP_FLARE,
	MAT_RT_BSP_LM,
	MAT_RT_BSP_VERTEX,
	MAT_RT_PIC,
	MAT_RT_POLY,
	MAT_RT_SKYBOX
};

// Material flags
typedef uint16 matBaseFlags_t;
enum
{
	MAT_AUTOSPRITE				= BIT(0),
	MAT_DEPTHWRITE				= BIT(1),
	MAT_ENTITY_MERGABLE			= BIT(3),
	MAT_FLARE					= BIT(4),
	MAT_LIGHTMAPPED				= BIT(5),
	MAT_NOFLUSH					= BIT(6),
	MAT_NOLERP					= BIT(7),
	MAT_NOMARK					= BIT(8),
	MAT_NOSHADOW				= BIT(9),
	MAT_POLYGONOFFSET			= BIT(10),
	MAT_SKY						= BIT(11),
	MAT_SUBDIVIDE				= BIT(12),
};

// Material sortKeys
enum EMatSortKey
{
	MAT_SORT_NONE,
	MAT_SORT_PORTAL,
	MAT_SORT_SKY,
	MAT_SORT_OPAQUE,
	MAT_SORT_DECAL,
	MAT_SORT_SEETHROUGH,
	MAT_SORT_BANNER,
	MAT_SORT_UNDERWATER,
	MAT_SORT_ENTITY,
	MAT_SORT_ENTITY2,
	MAT_SORT_PARTICLE,
	MAT_SORT_WATER,
	MAT_SORT_ADDITIVE,
	MAT_SORT_NEAREST,
	MAT_SORT_POSTPROCESS,

	MAT_SORT_MAX
};

// Material surfParam flags
typedef uint16 matSurfParams_t;
enum
{
	MAT_SURF_TRANS33			= BIT(0),
	MAT_SURF_TRANS66			= BIT(1),
	MAT_SURF_WARP				= BIT(2),
	MAT_SURF_FLOWING			= BIT(3),
	MAT_SURF_LIGHTMAP			= BIT(4)
};

// Material vertice deformation functions
enum EMatDeformvType
{
	DEFORMV_NONE,
	DEFORMV_WAVE,
	DEFORMV_NORMAL,
	DEFORMV_BULGE,
	DEFORMV_MOVE,
	DEFORMV_AUTOSPRITE,
	DEFORMV_AUTOSPRITE2,
	DEFORMV_PROJECTION_SHADOW,
	DEFORMV_AUTOPARTICLE
};

struct vertDeform_t
{
	EMatDeformvType		type;
	float				args[4];
	materialFunc_t		func;
};

//
// Base material structure
//
struct refMaterial_t
{
	char						name[MAX_QPATH];		// material name
	matBaseFlags_t				flags;
	EMatPathType				pathType;				// gameDir > baseDir > internal

	matPassFlags_t				addPassFlags;			// add these to all passes before completion
	texFlags_t					addTexFlags;			// add these to all passes before registration

	int							sizeBase;				// used for texcoord generation and image size lookup function
	uint32						touchFrame;				// touch if this matches the current ri.reg.registerFrame
	matSurfParams_t				surfParams;
	meshFeatures_t				features;
	EMatSortKey					sortKey;

	int							numPasses;
	matPass_t					*passes;

	int							numDeforms;
	vertDeform_t				*deforms;

	colorb						fogColor;
	double						fogDist;

	stateBit_t					stateBits;
	sint16						subdivide;

	uint32						hashValue;
	struct refMaterial_t		*hashNext;

	int							index;
};

extern refMaterial_t	r_materialList[MAX_MATERIALS];

//
// rf_material.c
//

void R_EndMaterialRegistration();

refMaterial_t *R_RegisterFlare(const char *name);
refMaterial_t *R_RegisterSky(const char *name);
refMaterial_t *R_RegisterTexture(const char *name, const matSurfParams_t surfParams);
refMaterial_t *R_RegisterTextureLM(const char *name);
refMaterial_t *R_RegisterTextureVertex(const char *name);

void R_MaterialInit();
void R_MaterialShutdown();
