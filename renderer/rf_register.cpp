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
// rf_register.cpp
//

#include "rf_local.h"

cVar_t	*e_test_0;
cVar_t	*e_test_1;

cVar_t	*gl_bitdepth;
cVar_t	*gl_clear;
cVar_t	*gl_cull;
cVar_t	*gl_drawbuffer;
cVar_t	*gl_dynamic;
cVar_t	*gl_errorcheck;

cVar_t	*r_allowExtensions;
cVar_t	*r_ext_BGRA;
cVar_t	*r_ext_compiledVertexArray;
cVar_t	*r_ext_drawRangeElements;
cVar_t	*r_ext_frameBufferObject;
cVar_t	*r_ext_maxAnisotropy;
cVar_t	*r_ext_multitexture;
cVar_t	*r_ext_programs;
cVar_t	*r_ext_shaders;
cVar_t	*r_ext_stencilTwoSide;
cVar_t	*r_ext_stencilWrap;
cVar_t	*r_ext_swapInterval;
cVar_t	*r_ext_texture3D;
cVar_t	*r_ext_textureCompression;
cVar_t	*r_ext_textureCubeMap;
cVar_t	*r_ext_textureEdgeClamp;
cVar_t	*r_ext_textureEnvAdd;
cVar_t	*r_ext_textureEnvCombine;
cVar_t	*r_ext_textureEnvDot3;
cVar_t	*r_ext_textureFilterAnisotropic;
cVar_t	*r_ext_vertexBufferObject;

cVar_t	*gl_finish;
cVar_t	*gl_flashblend;
cVar_t	*gl_lightmap;
cVar_t	*gl_lockpvs;
cVar_t	*gl_maxTexSize;
cVar_t	*gl_mode;
cVar_t	*gl_modulate;

cVar_t	*gl_shadows;
cVar_t	*gl_shownormals;
cVar_t	*gl_showtris;

cVar_t	*r_caustics;
cVar_t	*r_coloredLighting;
cVar_t	*r_colorMipLevels;
cVar_t	*r_debugBatching;
cVar_t	*r_debugCulling;
cVar_t	*r_debugLighting;
cVar_t	*r_debugLightmapIndex;
cVar_t	*r_debugSorting;
cVar_t	*r_defaultFont;
cVar_t	*r_detailTextures;
cVar_t	*r_displayFreq;
cVar_t	*r_drawDecals;
cVar_t	*r_drawEntities;
cVar_t	*r_drawPolys;
cVar_t	*r_drawworld;
cVar_t	*r_facePlaneCull;
cVar_t	*r_flares;
cVar_t	*r_flareFade;
cVar_t	*r_flareSize;
cVar_t	*r_fontScale;
cVar_t	*r_fullbright;
cVar_t	*r_hwGamma;
cVar_t	*r_lerpmodels;
cVar_t	*r_lightlevel;
cVar_t	*r_lmMaxBlockSize;
cVar_t	*r_lmModulate;
cVar_t	*r_lmPacking;
cVar_t	*r_noCull;
cVar_t	*r_noRefresh;
cVar_t	*r_debugServerPhysics;
cVar_t	*r_noVis;
cVar_t	*r_offsetFactor;
cVar_t	*r_offsetUnits;
cVar_t	*r_patchDivLevel;
cVar_t	*r_roundImagesDown;
cVar_t	*r_skipBackend;
cVar_t	*r_speeds;
cVar_t	*r_swapInterval;
cVar_t	*r_textureBits;
cVar_t	*r_times;
cVar_t	*r_vertexLighting;
cVar_t	*r_zFarAbs;
cVar_t	*r_zFarMin;
cVar_t	*r_zNear;

cVar_t	*r_alphabits;
cVar_t	*r_colorbits;
cVar_t	*r_depthbits;
cVar_t	*r_multisamples;
cVar_t	*r_stencilbits;
cVar_t	*cl_stereo;
cVar_t	*gl_allow_software;
cVar_t	*gl_stencilbuffer;

cVar_t	*vid_gamma;
cVar_t	*vid_gammapics;
cVar_t	*vid_width;
cVar_t	*vid_height;

cVar_t	*intensity;

cVar_t	*gl_jpgquality;
cVar_t	*gl_nobind;
cVar_t	*gl_picmip;
cVar_t	*gl_screenshot;
cVar_t	*gl_texturemode;

static conCmd_t	*cmd_gfxInfo;
static conCmd_t	*cmd_rendererClass;
static conCmd_t	*cmd_eglRenderer;
static conCmd_t	*cmd_eglVersion;

/*
=============================================================================

	CONSOLE COMMANDS

=============================================================================
*/

/*
==================
R_RendererClass
==================
*/
const char *R_RendererClassName()
{
	switch (ri.renderClass)
	{
	case REND_CLASS_DEFAULT:			return "Default";
	case REND_CLASS_MCD:				return "MCD";

	case REND_CLASS_3DLABS_GLINT_MX:	return "3DLabs GLIntMX";
	case REND_CLASS_3DLABS_PERMEDIA:	return "3DLabs Permedia";
	case REND_CLASS_3DLABS_REALIZM:		return "3DLabs Realizm";
	case REND_CLASS_ATI:				return "ATi";
	case REND_CLASS_ATI_RADEON:			return "ATi Radeon";
	case REND_CLASS_INTEL:				return "Intel";
	case REND_CLASS_NVIDIA:				return "nVidia";
	case REND_CLASS_NVIDIA_GEFORCE:		return "nVidia GeForce";
	case REND_CLASS_PMX:				return "PMX";
	case REND_CLASS_POWERVR_PCX1:		return "PowerVR PCX1";
	case REND_CLASS_POWERVR_PCX2:		return "PowerVR PCX2";
	case REND_CLASS_RENDITION:			return "Rendition";
	case REND_CLASS_S3:					return "S3";
	case REND_CLASS_SGI:				return "SGI";
	case REND_CLASS_SIS:				return "SiS";
	case REND_CLASS_VOODOO:				return "Voodoo";
	}

	return "";
}


/*
==================
R_RendererClass_f
==================
*/
static void R_RendererClass_f()
{
	Com_Printf(0, "Renderer Class: %s\n", R_RendererClassName());
}


/*
==================
R_GfxInfo_f
==================
*/
static void R_GfxInfo_f()
{
	Com_Printf(0, "----------------------------------------\n");

	Com_Printf(0, APP_FULLNAME ":\n" "GL_PFD: c(%d-bits) a(%d-bits) z(%d-bit) s(%d-bit)\n",
				ri.cColorBits, ri.cAlphaBits, ri.cDepthBits, ri.cStencilBits);

	Com_Printf(0, "Renderer Class: %s\n", R_RendererClassName());

	Com_Printf(0, "----------------------------------------\n");

	Com_Printf(0, "GL_VENDOR: %s\n",		ri.vendorString);
	Com_Printf(0, "GL_RENDERER: %s\n",		ri.rendererString);
	Com_Printf(0, "GL_VERSION: %s\n",		ri.versionString);
	Com_Printf(0, "GL_EXTENSIONS: %s\n",	ri.extensionString);

	Com_Printf(0, "----------------------------------------\n");

	Com_Printf(0, "Extensions:\n");
	Com_Printf(0, "...ARB Multitexture: %s\n",				ri.config.ext.bARBMultitexture ? "On" : "Off");

	Com_Printf(0, "...BGRA: %s\n",							ri.config.ext.bBGRA ? "On" : "Off");
	Com_Printf(0, "...Compiled Vertex Array: %s\n",			ri.config.ext.bCompiledVertArray ? "On" : "Off");

	Com_Printf(0, "...Draw Range Elements: %s\n",			ri.config.ext.bDrawRangeElements ? "On" : "Off");
	if (ri.config.ext.bDrawRangeElements)
	{
		Com_Printf(0, "...* Max element vertices: %i\n",	ri.config.ext.maxElementVerts);
		Com_Printf(0, "...* Max element indices: %i\n",		ri.config.ext.maxElementIndices);
	}

	Com_Printf(0, "...Frame Buffer Objects: %s\n",			ri.config.ext.bFrameBufferObject ? "On" : "Off");

	Com_Printf(0, "...SGIS Multitexture: %s\n",				ri.config.ext.bSGISMultiTexture ? "On" : ri.config.ext.bARBMultitexture ? "Deprecated for ARB Multitexture" : "Off");
	Com_Printf(0, "...Stencil Two Side: %s\n",				ri.config.ext.bStencilTwoSide ? "On" : "Off");
	Com_Printf(0, "...Stencil Wrap: %s\n",					ri.config.ext.bStencilWrap ? "On" : "Off");

	Com_Printf(0, "...Texture Cube Map: %s\n",				ri.config.ext.bTexCubeMap ? "On" : "Off");
	if (ri.config.ext.bTexCubeMap)
		Com_Printf(0, "...* Max cubemap texture size: %i\n",ri.config.ext.maxCMTexSize);

	Com_Printf(0, "...Texture Compression: %s\n",			ri.config.ext.bTexCompression ? "On" : "Off");

	Com_Printf(0, "...Texture 3D: %s\n",					ri.config.ext.bTex3D ? "On" : "Off");
	if (ri.config.ext.bTex3D)
		Com_Printf(0, "...* Max texture3D size: %i\n", ri.config.ext.max3DTexSize);

	Com_Printf(0, "...Texture Edge Clamp: %s\n",			ri.config.ext.bTexEdgeClamp ? "On" : "Off");
	Com_Printf(0, "...Texture Env Add: %s\n",				ri.config.ext.bTexEnvAdd ? "On" : "Off");
	Com_Printf(0, "...Texture Env Combine: %s\n",			ri.config.ext.bTexEnvCombine ? "On" : "Off");
	Com_Printf(0, "...Texture Env DOT3: %s\n",				ri.config.ext.bTexEnvDot3 ? "On" : "Off");

	Com_Printf(0, "...Texture Filter Anisotropic: %s\n",	ri.config.ext.bTexFilterAniso ? "On" : "Off");
	if (ri.config.ext.bTexFilterAniso)
		Com_Printf(0, "...* Max texture anisotropy: %i\n",	ri.config.ext.maxAniso);

	Com_Printf(0, "...Vertex Buffer Objects: %s\n",			ri.config.ext.bVertexBufferObject ? "On" : "Off");

	Com_Printf(0, "...Vertex and fragment programs/shaders: %s/%s\n",
		ri.config.ext.bPrograms ? "On" : "Off",
		ri.config.ext.bShaders ? "On" : "Off");
	if (ri.config.ext.bPrograms)
	{
		Com_Printf(0, "...* Max texture coordinates: %i\n",	ri.config.ext.maxTexCoords);
		Com_Printf(0, "...* Max texture image units: %i\n",	ri.config.ext.maxTexImageUnits);
	}
	Com_Printf(0, "...Shader objects: %s\n",				ri.config.ext.bShaderObjects ? "On" : "Off");
	if (ri.config.ext.bShaders)
	{
		Com_Printf(0, "...* Max vertex uniform components: %i\n", ri.config.ext.maxVertexUniforms);
		Com_Printf(0, "...* Max varying floats: %i\n", ri.config.ext.maxVaryingFloats);
		Com_Printf(0, "...* Max vertex attribs: %i\n", ri.config.ext.maxVertexAttribs);
		Com_Printf(0, "...* Max fragment uniform components: %i\n", ri.config.ext.maxFragmentUniforms);
	}
	Com_Printf(0, "...Depth textures: %s\n",				ri.config.ext.bDepthTexture ? "On" : "Off");
	Com_Printf(0, "...ARB Shadow: %s\n",					ri.config.ext.bARBShadow ? "On" : "Off");

	Com_Printf(0, "----------------------------------------\n");

	GL_TextureMode(true, true);
	GL_TextureBits(true, true);

	Com_Printf(0, "----------------------------------------\n");

	Com_Printf(0, "Max texture size: %i\n", ri.config.maxTexSize);
	Com_Printf(0, "Max texture units: %i\n", ri.config.maxTexUnits);

	Com_Printf(0, "----------------------------------------\n");
}


/*
==================
R_RendererMsg_f
==================
*/
static void R_RendererMsg_f()
{
	Cbuf_AddText(Q_VarArgs ("say [" APP_FULLNAME "]: [%s: %s v%s] GL_PFD[c%d/a%d/z%d/s%d] RES[%dx%dx%d]\n",
		ri.vendorString, ri.rendererString, ri.versionString,
		ri.cColorBits, ri.cAlphaBits, ri.cDepthBits, ri.cStencilBits,
		ri.config.vidWidth, ri.config.vidHeight, ri.config.vidBitDepth));
}


/*
==================
R_VersionMsg_f
==================
*/
static void R_VersionMsg_f()
{
	Cbuf_AddText(Q_VarArgs ("say [" APP_FULLNAME " (%s-%s) by Echon] [" EGL_HOMEPAGE "]\n",
		BUILDSTRING, CPUSTRING));
}

/*
=============================================================================

	CONFIG REGISTRATION

	Registers the renderer's cvars/commands and gets the latched ones during a vid_restart
=============================================================================
*/

/*
==================
R_RegisterConfig
==================
*/
void R_RegisterConfig()
{
	Cvar_GetLatchedVars(CVAR_LATCH_VIDEO);

	e_test_0			= Cvar_Register("e_test_0",				"0",			0);
	e_test_1			= Cvar_Register("e_test_1",				"0",			0);

	gl_bitdepth			= Cvar_Register("gl_bitdepth",			"0",			CVAR_LATCH_VIDEO);
	gl_clear			= Cvar_Register("gl_clear",				"0",			0);
	gl_cull				= Cvar_Register("gl_cull",				"1",			0);
	gl_drawbuffer		= Cvar_Register("gl_drawbuffer",		"GL_BACK",		0);
	gl_dynamic			= Cvar_Register("gl_dynamic",			"1",			0);
	gl_errorcheck		= Cvar_Register("gl_errorcheck",		"1",			CVAR_ARCHIVE);

	r_allowExtensions				= Cvar_Register("r_allowExtensions",				"1",		CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	r_ext_BGRA						= Cvar_Register("r_ext_BGRA",						"1",		CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	r_ext_compiledVertexArray		= Cvar_Register("r_ext_compiledVertexArray",		"1",		CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	r_ext_drawRangeElements			= Cvar_Register("r_ext_drawRangeElements",			"1",		CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	r_ext_frameBufferObject			= Cvar_Register("r_ext_frameBufferObject",			"1",		CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	r_ext_maxAnisotropy				= Cvar_Register("r_ext_maxAnisotropy",				"2",		CVAR_ARCHIVE);
	r_ext_multitexture				= Cvar_Register("r_ext_multitexture",				"1",		CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	r_ext_programs					= Cvar_Register("r_ext_programs",					"1",		CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	r_ext_shaders					= Cvar_Register("r_ext_shaders",					"1",		CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	r_ext_stencilTwoSide			= Cvar_Register("r_ext_stencilTwoSide",				"1",		CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	r_ext_stencilWrap				= Cvar_Register("r_ext_stencilWrap",				"1",		CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	r_ext_swapInterval				= Cvar_Register("r_ext_swapInterval",				"1",		CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	r_ext_texture3D					= Cvar_Register("r_ext_texture3D",					"1",		CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	r_ext_textureCompression		= Cvar_Register("r_ext_textureCompression",			"0",		CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	r_ext_textureCubeMap			= Cvar_Register("r_ext_textureCubeMap",				"1",		CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	r_ext_textureEdgeClamp			= Cvar_Register("r_ext_textureEdgeClamp",			"1",		CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	r_ext_textureEnvAdd				= Cvar_Register("r_ext_textureEnvAdd",				"1",		CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	r_ext_textureEnvCombine			= Cvar_Register("r_ext_textureEnvCombine",			"1",		CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	r_ext_textureEnvDot3			= Cvar_Register("r_ext_textureEnvDot3",				"1",		CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	r_ext_textureFilterAnisotropic	= Cvar_Register("r_ext_textureFilterAnisotropic",	"0",		CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	r_ext_vertexBufferObject		= Cvar_Register("r_ext_vertexBufferObject",			"0",		CVAR_ARCHIVE|CVAR_LATCH_VIDEO);

	gl_finish			= Cvar_Register("gl_finish",			"0",			CVAR_ARCHIVE);
	gl_flashblend		= Cvar_Register("gl_flashblend",		"0",			CVAR_ARCHIVE);
	gl_lightmap			= Cvar_Register("gl_lightmap",			"0",			CVAR_CHEAT);
	gl_lockpvs			= Cvar_Register("gl_lockpvs",			"0",			CVAR_CHEAT);
	gl_maxTexSize		= Cvar_Register("gl_maxTexSize",		"0",			CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	gl_mode				= Cvar_Register("gl_mode",				"3",			CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	gl_modulate			= Cvar_Register("gl_modulate",			"1",			CVAR_ARCHIVE);

	gl_shadows			= Cvar_Register("gl_shadows",			"0",			CVAR_ARCHIVE);
	gl_shownormals		= Cvar_Register("gl_shownormals",		"0",			CVAR_CHEAT);
	gl_showtris			= Cvar_Register("gl_showtris",			"0",			CVAR_CHEAT);

	r_caustics			= Cvar_Register("r_caustics",			"1",			CVAR_ARCHIVE);
	r_coloredLighting	= Cvar_Register("r_coloredLighting",	"1",			CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	r_colorMipLevels	= Cvar_Register("r_colorMipLevels",		"0",			CVAR_CHEAT|CVAR_LATCH_VIDEO);
	r_debugBatching		= Cvar_Register("r_debugBatching",		"0",			0);
	r_debugCulling		= Cvar_Register("r_debugCulling",		"0",			0);
	r_debugLighting		= Cvar_Register("r_debugLighting",		"0",			CVAR_CHEAT);
	r_debugLightmapIndex= Cvar_Register("r_debugLightmapIndex","-1",			CVAR_CHEAT);
	r_debugSorting		= Cvar_Register("r_debugSorting",		"0",			CVAR_CHEAT);
	r_defaultFont		= Cvar_Register("r_defaultFont",		"default",		CVAR_ARCHIVE);
	r_detailTextures	= Cvar_Register("r_detailTextures",		"1",			CVAR_ARCHIVE);
	r_displayFreq		= Cvar_Register("r_displayfreq",		"0",			CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	r_drawDecals		= Cvar_Register("r_drawDecals",			"1",			CVAR_CHEAT);
	r_drawEntities		= Cvar_Register("r_drawEntities",		"1",			CVAR_CHEAT);
	r_drawPolys			= Cvar_Register("r_drawPolys",			"1",			CVAR_CHEAT);
	r_drawworld			= Cvar_Register("r_drawworld",			"1",			CVAR_CHEAT);
	r_facePlaneCull		= Cvar_Register("r_facePlaneCull",		"1",			0);
	r_flares			= Cvar_Register("r_flares",				"1",			CVAR_ARCHIVE);
	r_flareFade			= Cvar_Register("r_flareFade",			"7",			CVAR_ARCHIVE);
	r_flareSize			= Cvar_Register("r_flareSize",			"40",			CVAR_ARCHIVE);
	r_fontScale			= Cvar_Register("r_fontScale",			"1",			CVAR_ARCHIVE);
	r_fullbright		= Cvar_Register("r_fullbright",			"0",			CVAR_CHEAT);
	r_hwGamma			= Cvar_Register("r_hwGamma",			"0",			CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	r_lerpmodels		= Cvar_Register("r_lerpmodels",			"1",			0);
	r_lightlevel		= Cvar_Register("r_lightlevel",			"0",			0);
	r_lmMaxBlockSize	= Cvar_Register("r_lmMaxBlockSize",		"4096",			CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	r_lmModulate		= Cvar_Register("r_lmModulate",			"2",			CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	r_lmPacking			= Cvar_Register("r_lmPacking",			"1",			CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	r_noCull			= Cvar_Register("r_noCull",				"0",			0);
	r_noRefresh			= Cvar_Register("r_noRefresh",			"0",			0);
	r_debugServerPhysics= Cvar_Register("r_debugPhysics",		"0",			0);
	r_noVis				= Cvar_Register("r_noVis",				"0",			0);
	r_offsetFactor		= Cvar_Register("r_offsetFactor",		"-1",			CVAR_CHEAT|CVAR_LATCH_VIDEO);
	r_offsetUnits		= Cvar_Register("r_offsetUnits",		"-2",			CVAR_CHEAT|CVAR_LATCH_VIDEO);
	r_patchDivLevel		= Cvar_Register("r_patchDivLevel",		"4",			CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	r_roundImagesDown	= Cvar_Register("r_roundImagesDown",	"0",			CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	r_skipBackend		= Cvar_Register("r_skipBackend",		"0",			CVAR_CHEAT);
	r_speeds			= Cvar_Register("r_speeds",				"0",			0);
	r_swapInterval		= Cvar_Register("r_swapInterval",		"0",			CVAR_ARCHIVE);
	r_textureBits		= Cvar_Register("r_textureBits",		"default",		CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	r_times				= Cvar_Register("r_times",				"0",			0);
	r_vertexLighting	= Cvar_Register("r_vertexLighting",		"0",			CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	r_zFarAbs			= Cvar_Register("r_zFarAbs",			"0",			CVAR_CHEAT);
	r_zFarMin			= Cvar_Register("r_zFarMin",			"256",			CVAR_CHEAT);
	r_zNear				= Cvar_Register("r_zNear",				"4",			CVAR_CHEAT);

	r_alphabits			= Cvar_Register("r_alphabits",			"0",			CVAR_LATCH_VIDEO);
	r_colorbits			= Cvar_Register("r_colorbits",			"0",			CVAR_LATCH_VIDEO);
	r_depthbits			= Cvar_Register("r_depthbits",			"0",			CVAR_LATCH_VIDEO);
	r_stencilbits		= Cvar_Register("r_stencilbits",		"8",			CVAR_LATCH_VIDEO);
	r_multisamples		= Cvar_Register("r_multisamples",		"0",			CVAR_LATCH_VIDEO);
	cl_stereo			= Cvar_Register("cl_stereo",			"0",			CVAR_LATCH_VIDEO);
	gl_allow_software	= Cvar_Register("gl_allow_software",	"0",			CVAR_LATCH_VIDEO);
	gl_stencilbuffer	= Cvar_Register("gl_stencilbuffer",		"1",			CVAR_ARCHIVE|CVAR_LATCH_VIDEO);

	vid_gamma			= Cvar_Register("vid_gamma",			"1.0",						CVAR_ARCHIVE);
	vid_gammapics		= Cvar_Register("vid_gammapics",		"0",						CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	vid_width			= Cvar_Register("vid_width",			"0",						CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	vid_height			= Cvar_Register("vid_height",			"0",						CVAR_ARCHIVE|CVAR_LATCH_VIDEO);

	intensity			= Cvar_Register("intensity",			"2",						CVAR_ARCHIVE);

	gl_jpgquality		= Cvar_Register("gl_jpgquality",		"85",						CVAR_ARCHIVE);
	gl_nobind			= Cvar_Register("gl_nobind",			"0",						CVAR_CHEAT);
	gl_picmip			= Cvar_Register("gl_picmip",			"0",						CVAR_LATCH_VIDEO);
	gl_screenshot		= Cvar_Register("gl_screenshot",		"tga",						CVAR_ARCHIVE);

	gl_texturemode		= Cvar_Register("gl_texturemode",		"GL_LINEAR_MIPMAP_NEAREST",	CVAR_ARCHIVE);

	// Force these to update next endframe
	r_swapInterval->modified = true;
	gl_drawbuffer->modified = true;
	gl_texturemode->modified = true;
	r_ext_maxAnisotropy->modified = true;
	r_defaultFont->modified = true;
	vid_gamma->modified = true;

	// Add the various commands
	cmd_gfxInfo			= Cmd_AddCommand("gfxinfo",			0, R_GfxInfo_f,				"Prints out renderer information");
	cmd_rendererClass	= Cmd_AddCommand("rendererclass",	0, R_RendererClass_f,		"Prints out the renderer class");
	cmd_eglRenderer		= Cmd_AddCommand("egl_renderer",	0, R_RendererMsg_f,			"Spams to the server your renderer information");
	cmd_eglVersion		= Cmd_AddCommand("egl_version",		0, R_VersionMsg_f,			"Spams to the server your client version");
}

/*
==================
R_UnRegisterConfig
==================
*/
void R_UnRegisterConfig()
{
	Cmd_RemoveCommand(cmd_gfxInfo);
	Cmd_RemoveCommand(cmd_rendererClass);
	Cmd_RemoveCommand(cmd_eglRenderer);
	Cmd_RemoveCommand(cmd_eglVersion);
}

/*
=============================================================================

	MAP REGISTRATION SEQUENCE

=============================================================================
*/

/*
==================
R_BeginRegistration

Starts refresh registration before map load
==================
*/
void R_BeginRegistration()
{
	// Clear the scene so that old scene object pointers are cleared
	R_ClearScene();

	// Clear old registration values
	ri.reg.fontsReleased = 0;
	ri.reg.fontsSeaked = 0;
	ri.reg.fontsTouched = 0;
	ri.reg.imagesReleased = 0;
	ri.reg.imagesResampled = 0;
	ri.reg.imagesSeaked = 0;
	ri.reg.imagesTouched = 0;
	ri.reg.modelsReleased = 0;
	ri.reg.modelsSeaked = 0;
	ri.reg.modelsTouched = 0;
	ri.reg.matsReleased = 0;
	ri.reg.matsSeaked = 0;
	ri.reg.matsTouched = 0;

	// Begin sub-system registration
	ri.reg.bInSequence = true;
	ri.reg.registerFrame++;
}


/*
==================
R_EndRegistration

Called at the end of all registration by the client
==================
*/
void R_EndRegistration()
{
	Com_DevPrintf(PRNT_CONSOLE, "Registration sequence completed...\n");

	R_EndFontRegistration(); // Register first so mats are touched
	R_EndModelRegistration(); // Register first so mats are touched
	R_EndMaterialRegistration(); // Register first so programs and images are touched
	R_EndImageRegistration();

	ri.reg.bInSequence = false;
}
