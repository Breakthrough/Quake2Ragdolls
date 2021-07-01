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
// rf_register.h
//

extern cVar_t	*e_test_0;
extern cVar_t	*e_test_1;

extern cVar_t	*intensity;

extern cVar_t	*gl_bitdepth;
extern cVar_t	*gl_clear;
extern cVar_t	*gl_cull;
extern cVar_t	*gl_drawbuffer;
extern cVar_t	*gl_dynamic;
extern cVar_t	*gl_errorcheck;

extern cVar_t	*r_allowExtensions;
extern cVar_t	*r_ext_BGRA;
extern cVar_t	*r_ext_compiledVertexArray;
extern cVar_t	*r_ext_drawRangeElements;
extern cVar_t	*r_ext_frameBufferObject;
extern cVar_t	*r_ext_maxAnisotropy;
extern cVar_t	*r_ext_multitexture;
extern cVar_t	*r_ext_programs;
extern cVar_t	*r_ext_shaders;
extern cVar_t	*r_ext_stencilTwoSide;
extern cVar_t	*r_ext_stencilWrap;
extern cVar_t	*r_ext_swapInterval;
extern cVar_t	*r_ext_texture3D;
extern cVar_t	*r_ext_textureCompression;
extern cVar_t	*r_ext_textureCubeMap;
extern cVar_t	*r_ext_textureEdgeClamp;
extern cVar_t	*r_ext_textureEnvAdd;
extern cVar_t	*r_ext_textureEnvCombine;
extern cVar_t	*r_ext_textureEnvDot3;
extern cVar_t	*r_ext_textureFilterAnisotropic;
extern cVar_t	*r_ext_vertexBufferObject;

extern cVar_t	*gl_finish;
extern cVar_t	*gl_flashblend;
extern cVar_t	*gl_jpgquality;
extern cVar_t	*gl_lightmap;
extern cVar_t	*gl_lockpvs;
extern cVar_t	*gl_maxTexSize;
extern cVar_t	*gl_mode;
extern cVar_t	*gl_modulate;

extern cVar_t	*gl_screenshot;
extern cVar_t	*gl_shadows;
extern cVar_t	*gl_shownormals;
extern cVar_t	*gl_showtris;
extern cVar_t	*gl_stencilbuffer;
extern cVar_t	*gl_texturemode;

extern cVar_t	*r_caustics;
extern cVar_t	*r_coloredLighting;
extern cVar_t	*r_colorMipLevels;
extern cVar_t	*r_debugBatching;
extern cVar_t	*r_debugCulling;
extern cVar_t	*r_debugLighting;
extern cVar_t	*r_debugLightmapIndex;
extern cVar_t	*r_debugSorting;
extern cVar_t	*r_defaultFont;
extern cVar_t	*r_detailTextures;
extern cVar_t	*r_displayFreq;
extern cVar_t	*r_drawDecals;
extern cVar_t	*r_drawEntities;
extern cVar_t	*r_drawPolys;
extern cVar_t	*r_drawworld;
extern cVar_t	*r_facePlaneCull;
extern cVar_t	*r_flares;
extern cVar_t	*r_flareFade;
extern cVar_t	*r_flareSize;
extern cVar_t	*r_fontScale;
extern cVar_t	*r_fullbright;
extern cVar_t	*r_hwGamma;
extern cVar_t	*r_lerpmodels;
extern cVar_t	*r_lightlevel;	// FIXME: This is a HACK to get the client's light level
extern cVar_t	*r_lmMaxBlockSize;
extern cVar_t	*r_lmModulate;
extern cVar_t	*r_lmPacking;
extern cVar_t	*r_noCull;
extern cVar_t	*r_noRefresh;
extern cVar_t	*r_debugServerPhysics;
extern cVar_t	*r_noVis;
extern cVar_t	*r_offsetFactor;
extern cVar_t	*r_offsetUnits;
extern cVar_t	*r_patchDivLevel;
extern cVar_t	*r_roundImagesDown;
extern cVar_t	*r_skipBackend;
extern cVar_t	*r_speeds;
extern cVar_t	*r_swapInterval;
extern cVar_t	*r_textureBits;
extern cVar_t	*r_times;
extern cVar_t	*r_vertexLighting;
extern cVar_t	*r_zFarAbs;
extern cVar_t	*r_zFarMin;
extern cVar_t	*r_zNear;

extern cVar_t	*r_alphabits;
extern cVar_t	*r_colorbits;
extern cVar_t	*r_depthbits;
extern cVar_t	*r_multisamples;
extern cVar_t	*r_stencilbits;
extern cVar_t	*cl_stereo;
extern cVar_t	*gl_allow_software;
extern cVar_t	*gl_stencilbuffer;

extern cVar_t	*vid_gammapics;
extern cVar_t	*vid_gamma;
extern cVar_t	*vid_width;
extern cVar_t	*vid_height;

extern cVar_t	*intensity;

extern cVar_t	*gl_nobind;
extern cVar_t	*gl_picmip;
extern cVar_t	*gl_screenshot;
extern cVar_t	*gl_texturemode;

//
// functions
//
const char *R_RendererClassName();

void R_RegisterConfig();
void R_UnRegisterConfig();
