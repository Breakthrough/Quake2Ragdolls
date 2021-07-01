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
// r_local.h
// Refresh only header file
//

#ifdef DEDICATED_ONLY
# error You should not be including this file in a dedicated server build
#endif // DEDICATED_ONLY

#include "r_public.h"

#ifdef WIN32
# include <windows.h>
#endif

#include <GL/gl.h>
#include "glext.h"

#ifdef WIN32
# include "../win32/wglext.h"
#endif

#include <math.h>

#include "rb_qgl.h"
#include "rf_image.h"
#include "rf_program.h"
#include "rf_shader.h"
#include "rf_public.h"
#include "rb_public.h"
#include "rf_material.h"
#include "rf_model.h"

/*
=============================================================================

	REFRESH INFO

=============================================================================
*/

enum ERendererClass
{
	REND_CLASS_DEFAULT,
	REND_CLASS_MCD,

	REND_CLASS_3DLABS_GLINT_MX,
	REND_CLASS_3DLABS_PERMEDIA,
	REND_CLASS_3DLABS_REALIZM,
	REND_CLASS_ATI,
	REND_CLASS_ATI_RADEON,
	REND_CLASS_INTEL,
	REND_CLASS_NVIDIA,
	REND_CLASS_NVIDIA_GEFORCE,
	REND_CLASS_PMX,
	REND_CLASS_POWERVR_PCX1,
	REND_CLASS_POWERVR_PCX2,
	REND_CLASS_RENDITION,
	REND_CLASS_S3,
	REND_CLASS_SGI,
	REND_CLASS_SIS,
	REND_CLASS_VOODOO
};

struct refMedia_t
{
	struct refFont_t		*defaultFont;

	// Internal textures
	refImage_t				*noTexture;			// use for bad textures
	refImage_t				*whiteTexture;		// used in mats/fallback
	refImage_t				*blackTexture;		// used in mats/fallback
	refImage_t				*cinTexture;		// allocates memory on load as to not every cin frame
	refImage_t				*dLightTexture;		// dynamic light texture for q3 bsp
	refImage_t				*fogTexture;		// fog texture for q3 bsp

	refImage_t				*lmTextures[MAX_LIGHTMAP_IMAGES];
	refImage_t				*shadowTextures[MAX_SHADOW_GROUPS];

	// Internal materials
	refMaterial_t			*cinMaterial;
	refMaterial_t			*noMaterial;
	refMaterial_t			*noMaterialAlias;
	refMaterial_t			*noMaterialLightmap;
	refMaterial_t			*noMaterialSky;
	refMaterial_t			*whiteMaterial;
	refMaterial_t			*blackMaterial;

							// This is hacky but necessary, fuck you Quake2.
	refMaterial_t			*worldLavaCaustics;
	refMaterial_t			*worldSlimeCaustics;
	refMaterial_t			*worldWaterCaustics;
};

enum ERefViewType
{
	RVT_NORMAL,
	RVT_SHADOWMAP,
	RVT_PORTAL,
	RVT_MIRROR,

	RVT_MAX
};

enum
{
	FRP_NEAR,
	FRP_LEFT,
	FRP_RIGHT,
	FRP_DOWN,
	FRP_UP,
	FRP_FAR,

	FRP_MAX
};

// FIXME: some of this can be moved to ri...
struct refScene_t
{
	// View
	uint32					clipFlags;
	plane_t					viewFrustum[FRP_MAX];

	vec3_t					visMins;
	vec3_t					visMaxs;

	ERefViewType			viewType;
	refMeshList				*currentList;
	refMeshList				meshLists[RVT_MAX];
	bool					bDrawingMeshOutlines;

	// Matrixes
	mat4x4_t				objectMatrix;
	mat4x4_t				projectionMatrix;
	mat4x4_t				worldViewMatrix;

	mat4x4_t				modelViewMatrix; // worldViewMatrix * objectMatrix
	mat4x4_t				worldViewProjectionMatrix; // worldViewMatrix * projectionMatrix

	vec3_t					portalOrigin;
	plane_t					clipPlane;

	float					zFar;

	// Default internal types
	refEntity_t				*defaultEntity;
	refModel_t				*defaultModel;

	// World model
	refModel_t				*worldModel;
	refEntity_t				*worldEntity;

	uint32					visFrameCount;	// Bumped when going to a new PVS
	int						viewCluster;
	int						oldViewCluster;

	// Items
	TList<refDecal_t*>		decalList;

	uint32					numEntities;
	refEntity_t				entityList[MAX_REF_ENTITIES];

	TList<refPoly_t*>		polyList;

	uint32					numDLights;
	refDLight_t				dLightList[MAX_REF_DLIGHTS];
	uint32					dLightCullBits;

	refLightStyle_t			lightStyles[MAX_CS_LIGHTSTYLES];
};

/*
=============================================================================

	PERFORMANCE COUNTERS

=============================================================================
*/

enum
{
	CULL_FAIL,
	CULL_PASS
};

struct refStats_t
{
	// Totals
	uint32					numTris;
	uint32					numVerts;
	uint32					numElements;

	uint32					decalsPushed;
	uint32					decalElements;
	uint32					decalPolys;

	uint32					polyElements;
	uint32					polyPolys;

	uint32					meshCount;
	uint32					meshPasses;

	uint32					stateChanges;

	// Alias Models
	uint32					aliasElements;
	uint32					aliasPolys;

	uint32					cullAlias[2];

	// Batching
	uint32					meshBatcherPushes;
	uint32					meshBatcherFlushes;

	// Culling
	uint32					cullBounds[2];
	uint32					cullRadius[2];

	// Image
	uint32					textureBinds;
	uint32					textureEnvChanges;
	uint32					textureUnitChanges;
	uint32					textureMatChanges;
	uint32					textureTargetChanges;
	uint32					textureGenChanges;

	uint32					texelsInUse;
	uint32					texturesInUse;

	// World model
	uint32					worldElements;
	uint32					worldPolys;

	uint32					cullPlanar[2];
	uint32					cullSurf[2];
	uint32					cullVis[2];

	uint32					cullDLight[2];

	// Time to process
	uint32					timeAddToList;
	uint32					timeSortList;
	uint32					timePushMesh;
	uint32					timeDrawList;

	uint32					timeMarkLeaves;
	uint32					timeMarkLights;
	uint32					timeRecurseWorld;
	uint32					timeShadowRecurseWorld;
};

/*
=============================================================================

	REGISTRATION SEQUENCE

=============================================================================
*/

struct refRegInfo_t
{
	bool					bInSequence;			// True when in a registration sequence
	uint32					registerFrame;		// Used to determine what's kept and what's not

	// Fonts
	uint32					fontsReleased;
	uint32					fontsSeaked;
	uint32					fontsTouched;

	// Images
	uint32					imagesReleased;
	uint32					imagesResampled;
	uint32					imagesSeaked;
	uint32					imagesTouched;

	// Models
	uint32					modelsReleased;
	uint32					modelsSeaked;
	uint32					modelsTouched;

	// Materials
	uint32					matsReleased;
	uint32					matsSeaked;
	uint32					matsTouched;
};

/*
=============================================================================

	CORE REFRESH INFO

=============================================================================
*/

struct refInfo_t
{
							// Refresh information
	ERendererClass			renderClass;

	const byte				*rendererString;
	const byte				*vendorString;
	const byte				*versionString;
	const byte				*extensionString;

	int						lastValidMode;

							// Frame information
	float					cameraSeparation;
	uint32					frameCount;

							// Hardware gamma
	bool					bRampDownloaded;
	uint16					originalRamp[768];
	uint16					gammaRamp[768];

							// PFD Stuff
	bool					bAllowStencil;

	byte					cAlphaBits;
	byte					cColorBits;
	byte					cDepthBits;
	byte					cStencilBits;
	int						cMultiSamples;

							// Texture
	float					inverseIntensity;
	byte					identityLighting; // inverseIntensity in byte form.

	GLint					texMinFilter;
	GLint					texMagFilter;

	int						rgbFormat;
	int						rgbaFormat;
	int						greyFormat;

	int						rgbFormatCompressed;
	int						rgbaFormatCompressed;
	int						greyFormatCompressed;

	float					pow2MapOvrbr;

							// Memory management
	struct memPool_t		*decalSysPool;
	struct memPool_t		*fontSysPool;
	struct memPool_t		*genericPool;
	struct memPool_t		*imageSysPool;
	struct memPool_t		*lightSysPool;
	struct memPool_t		*matSysPool;
	struct memPool_t		*modelSysPool;
	struct memPool_t		*programSysPool;
	struct memPool_t		*shaderSysPool;

							// Misc
	refConfig_t				config;			// Information output to the client/cgame

	refScene_t				scn;			// Local scene information
	refDef_t				def;			// Current refDef being rendered in the scene

	refMedia_t				media;			// Local media
	refRegInfo_t			reg;			// Registration counters
	refStats_t				pc;				// Performance counters

	// FBOs
	refImage_t				*screenFBO;
};

extern refInfo_t			ri;

/*
=============================================================================

	SUPPORTING FUNCTIONS

=============================================================================
*/

extern bool r_bInTimeDemo;

class qStatCycle_Scope
{
protected:
	uint32 StartCycles;

	cVar_t *EnablingVar;
	uint32 &Output;

public:
	qStatCycle_Scope(cVar_t *Enabler, uint32 &Target)
		: EnablingVar(Enabler)
		, Output(Target)
	{
		if (Enabled())
			Start();
	}
	~qStatCycle_Scope()
	{
		if (Enabled())
			Stop();
	}

protected:
	inline void Start()
	{
		StartCycles = Sys_Cycles();
	}
	inline void Stop()
	{
		uint32 EndCycles = Sys_Cycles();
		Output += (EndCycles - StartCycles);
	}

	inline bool Enabled()
	{
		if (!ri.scn.bDrawingMeshOutlines && (r_bInTimeDemo || EnablingVar->intVal))
			return true;

		return false;
	}
};

/*
=============================================================================

	IMPLEMENTATION SPECIFIC

=============================================================================
*/

//
// glimp_imp.cpp
//

bool GLimp_GetGammaRamp(uint16 *ramp);
void GLimp_SetGammaRamp(uint16 *ramp);
