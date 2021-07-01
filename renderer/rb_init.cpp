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
// rb_init.cpp
//

#include "rb_local.h"

/*
============
R_GetInfoForMode
============
*/
struct vidMode_t
{
	char		*info;

	int			width;
	int			height;

	int			mode;
};

static vidMode_t r_vidModes[] =
{
	{"Mode 0: 320 x 240",			320,	240,	0 },
	{"Mode 1: 400 x 300",			400,	300,	1 },
	{"Mode 2: 512 x 384",			512,	384,	2 },
	{"Mode 3: 640 x 480",			640,	480,	3 },
	{"Mode 4: 800 x 600",			800,	600,	4 },
	{"Mode 5: 960 x 720",			960,	720,	5 },
	{"Mode 6: 1024 x 768",			1024,	768,	6 },
	{"Mode 7: 1152 x 864",			1152,	864,	7 },
	{"Mode 8: 1280 x 960",			1280,	960,	8 },
	{"Mode 9: 1600 x 1200",			1600,	1200,	9 },
	{"Mode 10: 1920 x 1440",		1920,	1440,	10},
	{"Mode 11: 2048 x 1536",		2048,	1536,	11},

	{"Mode 12: 1280 x 800 (ws)",	1280,	800,	12},
	{"Mode 13: 1440 x 900 (ws)",	1440,	900,	13}
};

#define NUM_VIDMODES (sizeof(r_vidModes) / sizeof(r_vidModes[0]))
inline bool R_GetInfoForMode(const int mode, int *width, int *height)
{
	if (mode < 0 || mode >= NUM_VIDMODES)
		return false;

	*width  = r_vidModes[mode].width;
	*height = r_vidModes[mode].height;
	return true;
}


/*
==================
R_SetMode
==================
*/
#define SAFE_MODE	3
static bool R_SetMode()
{
	Com_Printf(0, "Setting video mode\n");

	// Find the mode info
	bool bFullScreen = vid_fullscreen->intVal ? true : false;
	int width, height;
	if (vid_width->intVal > 0 && vid_height->intVal > 0)
	{
		width = vid_width->intVal;
		height = vid_height->intVal;
	}
	else if (!R_GetInfoForMode(gl_mode->intVal, &width, &height))
	{
		Com_Printf(PRNT_ERROR, "...bad mode '%i', forcing safe mode\n", gl_mode->intVal);
		Cvar_VariableSetValue(gl_mode, (float)SAFE_MODE, true);
		if (!R_GetInfoForMode(SAFE_MODE, &width, &height))
			return false;	// This should *never* happen if SAFE_MODE is a sane value
	}

	// Attempt the desired mode
	if (GLimp_AttemptMode(bFullScreen, width, height))
	{
		Cvar_VariableSetValue(vid_fullscreen, (float)ri.config.vidFullScreen, true);
		return true;
	}

	// Bad mode, fall out of fullscreen if it was attempted
	if (bFullScreen)
	{
		Com_Printf(PRNT_ERROR, "...failed to set fullscreen, attempting windowed\n");

		if (GLimp_AttemptMode(false, width, height))
		{
			Cvar_VariableSetValue(vid_fullscreen, (float)ri.config.vidFullScreen, true);
			return true;
		}
	}

	// Don't attempt the last valid safe mode if the user is already using it
	if (ri.lastValidMode != -1 && ri.lastValidMode != gl_mode->intVal)
	{
		Com_Printf(PRNT_ERROR, "...failed to set mode, attempted the last valid mode\n");
		Cvar_VariableSetValue(gl_mode, (float)ri.lastValidMode, true);

		if (GLimp_AttemptMode(false, width, height))
		{
			Cvar_VariableSetValue(vid_fullscreen, (float)ri.config.vidFullScreen, true);
			return true;
		}
	}

	// Don't attempt safe mode if the user is already using it
	if (gl_mode->intVal == SAFE_MODE)
	{
		Com_Printf(PRNT_ERROR, "...already using the safe mode, exiting\n");
		return false;
	}

	// Bad mode period, fall back to safe mode
	Com_Printf(PRNT_ERROR, "...failed to set mode, attempting safe mode '%d'\n", SAFE_MODE);
	Cvar_VariableSetValue(gl_mode, (float)SAFE_MODE, true);

	// Try setting it back to something safe
	R_GetInfoForMode(gl_mode->intVal, &width, &height);
	if (GLimp_AttemptMode(bFullScreen, width, height))
	{
		Cvar_VariableSetValue(vid_fullscreen, (float)ri.config.vidFullScreen, true);
		return true;
	}

	Com_Printf(PRNT_ERROR, "...could not revert to safe mode\n");
	return false;
}


/*
===============
ExtensionFound
===============
*/
static bool ExtensionFound(const char *extension)
{
	// Extension names should not have spaces
	byte *where = (byte *) strchr (extension, ' ');
	if (where || *extension == '\0')
		return false;

	const byte *start = ri.extensionString;
	for ( ; ; )
	{
		where = (byte *)strstr((const char *)start, extension);
		if (!where)
			break;
		byte *terminator = where + strlen(extension);
		if (where == start || (*(where-1) == ' '))
		{
			if (*terminator == ' ' || *terminator == '\0')
			{
				return true;
			}
		}
		start = terminator;
	}
	return false;
}


/*
===============
GL_InitExtensions
===============
*/
static void GL_InitExtensions()
{
	// Check for gl errors
	RB_CheckForError("GL_InitExtensions");

	/*
	** GL_ARB_multitexture
	** GL_SGIS_multitexture
	*/
	if (r_ext_multitexture->intVal)
	{
		// GL_ARB_multitexture
		if (ExtensionFound("GL_ARB_multitexture"))
		{
			qglActiveTextureARB = (PFNGLACTIVETEXTUREARBPROC)GLimp_GetProcAddress("glActiveTextureARB");
			qglClientActiveTextureARB = (PFNGLCLIENTACTIVETEXTUREARBPROC)GLimp_GetProcAddress("glClientActiveTextureARB");

			if (!qglActiveTextureARB || !qglClientActiveTextureARB)
			{
				Com_Printf(PRNT_ERROR, "...GL_ARB_multitexture not properly supported!\n");
			}
			else
			{
				Com_Printf(0, "...enabling GL_ARB_multitexture\n");
				ri.config.ext.bARBMultitexture = true;
			}
		}
		else
			Com_Printf(0, "...GL_ARB_multitexture not found\n");

		// GL_SGIS_multitexture
		if (!ri.config.ext.bARBMultitexture)
		{
			Com_Printf(0, "...attempting GL_SGIS_multitexture\n");

			if (ExtensionFound("GL_SGIS_multitexture"))
			{
				qglSelectTextureSGIS = (void(APIENTRYP)(GLenum))GLimp_GetProcAddress("glSelectTextureSGIS");

				if (!qglSelectTextureSGIS)
				{
					Com_Printf(PRNT_ERROR, "...GL_SGIS_multitexture not properly supported!\n");
				}
				else
				{
					Com_Printf(0, "...enabling GL_SGIS_multitexture\n");
					ri.config.ext.bSGISMultiTexture = true;
				}
			}
			else
				Com_Printf(0, "...GL_SGIS_multitexture not found\n");
		}
	}
	else
	{
		Com_Printf(0, "...ignoring GL_ARB/SGIS_multitexture\n");
		Com_Printf(PRNT_WARNING, "WARNING: Disabling multitexture is not recommended!\n");
	}

	// Keep texture unit counts in check
	if (ri.config.ext.bSGISMultiTexture || ri.config.ext.bARBMultitexture)
	{
		glGetIntegerv(GL_MAX_TEXTURE_UNITS, &ri.config.maxTexUnits);

		if (ri.config.maxTexUnits < 2)
		{
			Com_Printf(0, "...not using GL_ARB/SGIS_multitexture, < 2 texture units\n");

			ri.config.maxTexUnits = 1;
			ri.config.ext.bARBMultitexture	= false;
			ri.config.ext.bSGISMultiTexture	= false;
		}
		else
		{
			if (ri.config.ext.bSGISMultiTexture && ri.config.maxTexUnits > 2)
			{
				// GL_SGIS_multitexture doesn't support more than 2 units does it?
				ri.config.maxTexUnits = 2;
				Com_Printf(0, "...* GL_SGIS_multitexture clamped to 2 texture units\n");
			}
			else if (ri.config.maxTexUnits > MAX_TEXUNITS)
			{
				// Clamp at the maximum amount the engine supports
				ri.config.maxTexUnits = MAX_TEXUNITS;
				Com_Printf(0, "...* clamped to engine maximum of %i texture units\n", ri.config.maxTexUnits);
			}
			else
				Com_Printf(0, "...* using video card maximum of %i texture units\n", ri.config.maxTexUnits);
		}
	}
	else
	{
		ri.config.maxTexUnits = 1;
	}

	/*
	** GL_ARB_texture_compression
	** GL_EXT_texture_compression_s3tc
	** GL_S3_s3tc
	*/
	if (r_ext_textureCompression->intVal)
	{
		while (r_ext_textureCompression->intVal)
		{
			switch(r_ext_textureCompression->intVal)
			{
			case 1:
				if (!ExtensionFound("GL_ARB_texture_compression"))
				{
					Com_Printf(0, "...GL_ARB_texture_compression not found\n");
					Cvar_VariableSetValue(r_ext_textureCompression, 2, true);
					break;
				}

				Com_Printf(0, "...enabling GL_ARB_texture_compression\n");
				ri.config.ext.bTexCompression = true;

				ri.rgbFormatCompressed = GL_COMPRESSED_RGB_ARB;
				ri.rgbaFormatCompressed = GL_COMPRESSED_RGBA_ARB;
				ri.greyFormatCompressed = GL_COMPRESSED_LUMINANCE_ARB;
				break;

			case 2:
			case 3:
			case 4:
				if (!ExtensionFound("GL_EXT_texture_compression_s3tc"))
				{
					Com_Printf(0, "...GL_EXT_texture_compression_s3tc not found\n");
					Cvar_VariableSetValue(r_ext_textureCompression, 5, true);
					break;
				}

				Com_Printf(0, "...enabling GL_EXT_texture_compression_s3tc\n");
				ri.config.ext.bTexCompression = true;

				ri.rgbFormatCompressed = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
				ri.greyFormatCompressed = GL_LUMINANCE4; // Not supported, just use 4bit per sample luminance
				switch(r_ext_textureCompression->intVal)
				{
				case 2:
					ri.rgbaFormatCompressed = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
					Com_Printf(0, "...* using S3TC_DXT1\n");
					break;

				case 3:
					ri.rgbaFormatCompressed = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
					Com_Printf(0, "...* using S3TC_DXT3\n");
					break;

				case 4:
					ri.rgbaFormatCompressed = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
					Com_Printf(0, "...* using S3TC_DXT5\n");
					break;
				}
				break;

			case 5:
				if (!ExtensionFound("GL_S3_s3tc"))
				{
					Com_Printf(0, "...GL_S3_s3tc not found\n");
					Cvar_VariableSetValue(r_ext_textureCompression, 0, true);
					break;
				}

				Com_Printf(0, "...enabling GL_S3_s3tc\n");
				ri.config.ext.bTexCompression = true;

				ri.rgbFormatCompressed = GL_RGB_S3TC;
				ri.rgbaFormatCompressed = GL_RGBA_S3TC;
				ri.greyFormatCompressed = GL_LUMINANCE4; // Not supported, just use 4bit per sample luminance
				break;

			default:
				Cvar_VariableSetValue(r_ext_textureCompression, 0, true);
				break;
			}

			if (ri.config.ext.bTexCompression || !r_ext_textureCompression->intVal)
				break;
		}
	}
	else
	{
		Com_Printf(0, "...ignoring GL_ARB_texture_compression\n");
		Com_Printf(0, "...ignoring GL_EXT_texture_compression_s3tc\n");
		Com_Printf(0, "...ignoring GL_S3_s3tc\n");
	}

	/*
	** GL_ARB_texture_cube_map
	*/
	if (r_ext_textureCubeMap->intVal)
	{
		if (ExtensionFound("GL_ARB_texture_cube_map"))
		{
			glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB, &ri.config.ext.maxCMTexSize);

			if (ri.config.ext.maxCMTexSize <= 0)
			{
				Com_Printf(PRNT_ERROR, "GL_ARB_texture_cube_map not properly supported!\n");
				ri.config.ext.maxCMTexSize = 0;
			}
			else
			{
				ri.config.ext.maxCMTexSize = Q_NearestPow<int>(ri.config.ext.maxCMTexSize, true);

				Com_Printf(0, "...enabling GL_ARB_texture_cube_map\n");
				Com_Printf(0, "...* Max cubemap texture size: %i\n", ri.config.ext.maxCMTexSize);
				ri.config.ext.bTexCubeMap = true;
			}
		}
		else
			Com_Printf(0, "...GL_ARB_texture_cube_map not found\n");
	}
	else
		Com_Printf(0, "...ignoring GL_ARB_texture_cube_map\n");

	/*
	** GL_ARB_texture_env_add
	*/
	if (r_ext_textureEnvAdd->intVal)
	{
		if (ExtensionFound("GL_ARB_texture_env_add"))
		{
			if (ri.config.ext.bSGISMultiTexture || ri.config.ext.bARBMultitexture)
			{
				Com_Printf(0, "...enabling GL_ARB_texture_env_add\n");
				ri.config.ext.bTexEnvAdd = true;
			}
			else
				Com_Printf(0, "...ignoring GL_ARB_texture_env_add (no multitexture)\n");
		}
		else
			Com_Printf(0, "...GL_ARB_texture_env_add not found\n");
	}
	else
		Com_Printf(0, "...ignoring GL_ARB_texture_env_add\n");

	/*
	** GL_ARB_texture_env_combine
	** GL_EXT_texture_env_combine
	*/
	if (r_ext_textureEnvCombine->intVal)
	{
		if (ExtensionFound("GL_ARB_texture_env_combine") ||
			ExtensionFound("GL_EXT_texture_env_combine"))
		{
			if (ri.config.ext.bSGISMultiTexture || ri.config.ext.bARBMultitexture)
			{
				Com_Printf(0, "...enabling GL_ARB/EXT_texture_env_combine\n");
				ri.config.ext.bTexEnvCombine = true;
			}
			else
				Com_Printf(0, "...ignoring GL_ARB/EXT_texture_env_combine (no multitexture)\n");
		}
		else
			Com_Printf(0, "...GL_ARB/EXT_texture_env_combine not found\n");
	}
	else
		Com_Printf(0, "...ignoring GL_ARB/EXT_texture_env_combine\n");

	/*
	** GL_ARB_texture_env_dot3
	*/
	if (r_ext_textureEnvDot3->intVal)
	{
		if (ExtensionFound("GL_ARB_texture_env_dot3"))
		{
			if (ri.config.ext.bTexEnvCombine)
			{
				Com_Printf(0, "...enabling GL_ARB_texture_env_dot3\n");
				ri.config.ext.bTexEnvDot3 = true;
			}
			else
				Com_Printf(0, "...ignoring GL_ARB_texture_env_dot3 (no combine)\n");
		}
		else
			Com_Printf(0, "...GL_ARB_texture_env_dot3 not found\n");
	}
	else
		Com_Printf(0, "...ignoring GL_ARB_texture_env_dot3\n");

	/*
	** GL_ARB_vertex_program
	** GL_ARB_fragment_program
	*/
	if (r_ext_programs->intVal)
	{
		if (ExtensionFound("GL_ARB_vertex_program")
		&& ExtensionFound("GL_ARB_fragment_program"))
		{
			qglVertexAttrib1dARB = (PFNGLVERTEXATTRIB1DARBPROC)GLimp_GetProcAddress("glVertexAttrib1dARB");
			qglVertexAttrib1dvARB = (PFNGLVERTEXATTRIB1DVARBPROC)GLimp_GetProcAddress("glVertexAttrib1dvARB");
			qglVertexAttrib1fARB = (PFNGLVERTEXATTRIB1FARBPROC)GLimp_GetProcAddress("glVertexAttrib1fARB");
			qglVertexAttrib1fvARB = (PFNGLVERTEXATTRIB1FVARBPROC)GLimp_GetProcAddress("glVertexAttrib1fvARB");
			qglVertexAttrib1sARB = (PFNGLVERTEXATTRIB1SARBPROC)GLimp_GetProcAddress("glVertexAttrib1sARB");
			qglVertexAttrib1svARB = (PFNGLVERTEXATTRIB1SVARBPROC)GLimp_GetProcAddress("glVertexAttrib1svARB");
			qglVertexAttrib2dARB = (PFNGLVERTEXATTRIB2DARBPROC)GLimp_GetProcAddress("glVertexAttrib2dARB");
			qglVertexAttrib2dvARB = (PFNGLVERTEXATTRIB2DVARBPROC)GLimp_GetProcAddress("glVertexAttrib2dvARB");
			qglVertexAttrib2fARB = (PFNGLVERTEXATTRIB2FARBPROC)GLimp_GetProcAddress("glVertexAttrib2fARB");
			qglVertexAttrib2fvARB = (PFNGLVERTEXATTRIB2FVARBPROC)GLimp_GetProcAddress("glVertexAttrib2fvARB");
			qglVertexAttrib2sARB = (PFNGLVERTEXATTRIB2SARBPROC)GLimp_GetProcAddress("glVertexAttrib2sARB");
			qglVertexAttrib2svARB = (PFNGLVERTEXATTRIB2SVARBPROC)GLimp_GetProcAddress("glVertexAttrib2svARB");
			qglVertexAttrib3dARB = (PFNGLVERTEXATTRIB3DARBPROC)GLimp_GetProcAddress("glVertexAttrib3dARB");
			qglVertexAttrib3dvARB = (PFNGLVERTEXATTRIB3DVARBPROC)GLimp_GetProcAddress("glVertexAttrib3dvARB");
			qglVertexAttrib3fARB = (PFNGLVERTEXATTRIB3FARBPROC)GLimp_GetProcAddress("glVertexAttrib3fARB");
			qglVertexAttrib3fvARB = (PFNGLVERTEXATTRIB3FVARBPROC)GLimp_GetProcAddress("glVertexAttrib3fvARB");
			qglVertexAttrib3sARB = (PFNGLVERTEXATTRIB3SARBPROC)GLimp_GetProcAddress("glVertexAttrib3sARB");
			qglVertexAttrib3svARB = (PFNGLVERTEXATTRIB3SVARBPROC)GLimp_GetProcAddress("glVertexAttrib3svARB");
			qglVertexAttrib4NbvARB = (PFNGLVERTEXATTRIB4NBVARBPROC)GLimp_GetProcAddress("glVertexAttrib4NbvARB");
			qglVertexAttrib4NivARB = (PFNGLVERTEXATTRIB4NIVARBPROC)GLimp_GetProcAddress("glVertexAttrib4NivARB");
			qglVertexAttrib4NsvARB = (PFNGLVERTEXATTRIB4NSVARBPROC)GLimp_GetProcAddress("glVertexAttrib4NsvARB");
			qglVertexAttrib4NubARB = (PFNGLVERTEXATTRIB4NUBARBPROC)GLimp_GetProcAddress("glVertexAttrib4NubARB");
			qglVertexAttrib4NubvARB = (PFNGLVERTEXATTRIB4NUBVARBPROC)GLimp_GetProcAddress("glVertexAttrib4NubvARB");
			qglVertexAttrib4NuivARB = (PFNGLVERTEXATTRIB4NUIVARBPROC)GLimp_GetProcAddress("glVertexAttrib4NuivARB");
			qglVertexAttrib4NusvARB = (PFNGLVERTEXATTRIB4USVARBPROC)GLimp_GetProcAddress("glVertexAttrib4NusvARB");
			qglVertexAttrib4bvARB = (PFNGLVERTEXATTRIB4BVARBPROC)GLimp_GetProcAddress("glVertexAttrib4bvARB");
			qglVertexAttrib4dARB = (PFNGLVERTEXATTRIB4DARBPROC)GLimp_GetProcAddress("glVertexAttrib4dARB");
			qglVertexAttrib4dvARB = (PFNGLVERTEXATTRIB4DVARBPROC)GLimp_GetProcAddress("glVertexAttrib4dvARB");
			qglVertexAttrib4fARB = (PFNGLVERTEXATTRIB4FARBPROC)GLimp_GetProcAddress("glVertexAttrib4fARB");
			qglVertexAttrib4fvARB = (PFNGLVERTEXATTRIB4FVARBPROC)GLimp_GetProcAddress("glVertexAttrib4fvARB");
			qglVertexAttrib4ivARB = (PFNGLVERTEXATTRIB4IVARBPROC)GLimp_GetProcAddress("glVertexAttrib4ivARB");
			qglVertexAttrib4sARB = (PFNGLVERTEXATTRIB4SARBPROC)GLimp_GetProcAddress("glVertexAttrib4sARB");
			qglVertexAttrib4svARB = (PFNGLVERTEXATTRIB4SVARBPROC)GLimp_GetProcAddress("glVertexAttrib4svARB");
			qglVertexAttrib4ubvARB = (PFNGLVERTEXATTRIB4UBVARBPROC)GLimp_GetProcAddress("glVertexAttrib4ubvARB");
			qglVertexAttrib4uivARB = (PFNGLVERTEXATTRIB4UIVARBPROC)GLimp_GetProcAddress("glVertexAttrib4uivARB");
			qglVertexAttrib4usvARB = (PFNGLVERTEXATTRIB4USVARBPROC)GLimp_GetProcAddress("glVertexAttrib4usvARB");
			qglVertexAttribPointerARB = (PFNGLVERTEXATTRIBPOINTERARBPROC)GLimp_GetProcAddress("glVertexAttribPointerARB");
			qglEnableVertexAttribArrayARB = (PFNGLENABLEVERTEXATTRIBARRAYARBPROC)GLimp_GetProcAddress("glEnableVertexAttribArrayARB");
			qglDisableVertexAttribArrayARB = (PFNGLDISABLEVERTEXATTRIBARRAYARBPROC)GLimp_GetProcAddress("glDisableVertexAttribArrayARB");
			qglProgramStringARB = (PFNGLPROGRAMSTRINGARBPROC)GLimp_GetProcAddress("glProgramStringARB");
			qglBindProgramARB = (PFNGLBINDPROGRAMARBPROC)GLimp_GetProcAddress("glBindProgramARB");
			qglDeleteProgramsARB = (PFNGLDELETEPROGRAMSARBPROC)GLimp_GetProcAddress("glDeleteProgramsARB");
			qglGenProgramsARB = (PFNGLGENPROGRAMSARBPROC)GLimp_GetProcAddress("glGenProgramsARB");
			qglProgramEnvParameter4dARB = (PFNGLPROGRAMENVPARAMETER4DARBPROC)GLimp_GetProcAddress("glProgramEnvParameter4dARB");
			qglProgramEnvParameter4dvARB = (PFNGLPROGRAMENVPARAMETER4DVARBPROC)GLimp_GetProcAddress("glProgramEnvParameter4dvARB");
			qglProgramEnvParameter4fARB = (PFNGLPROGRAMENVPARAMETER4FARBPROC)GLimp_GetProcAddress("glProgramEnvParameter4fARB");
			qglProgramEnvParameter4fvARB = (PFNGLPROGRAMENVPARAMETER4FVARBPROC)GLimp_GetProcAddress("glProgramEnvParameter4fvARB");
			qglProgramLocalParameter4dARB = (PFNGLPROGRAMLOCALPARAMETER4DARBPROC)GLimp_GetProcAddress("glProgramLocalParameter4dARB");
			qglProgramLocalParameter4dvARB = (PFNGLPROGRAMLOCALPARAMETER4DVARBPROC)GLimp_GetProcAddress("glProgramLocalParameter4dvARB");
			qglProgramLocalParameter4fARB = (PFNGLPROGRAMLOCALPARAMETER4FARBPROC)GLimp_GetProcAddress("glProgramLocalParameter4fARB");
			qglProgramLocalParameter4fvARB = (PFNGLPROGRAMLOCALPARAMETER4FVARBPROC)GLimp_GetProcAddress("glProgramLocalParameter4fvARB");
			qglGetProgramEnvParameterdvARB = (PFNGLGETPROGRAMENVPARAMETERDVARBPROC)GLimp_GetProcAddress("glGetProgramEnvParameterdvARB");
			qglGetProgramEnvParameterfvARB = (PFNGLGETPROGRAMENVPARAMETERFVARBPROC)GLimp_GetProcAddress("glGetProgramEnvParameterfvARB");
			qglGetProgramLocalParameterdvARB = (PFNGLGETPROGRAMLOCALPARAMETERDVARBPROC)GLimp_GetProcAddress("glGetProgramLocalParameterdvARB");
			qglGetProgramLocalParameterfvARB = (PFNGLGETPROGRAMLOCALPARAMETERFVARBPROC)GLimp_GetProcAddress("glGetProgramLocalParameterfvARB");
			qglGetProgramivARB = (PFNGLGETPROGRAMIVARBPROC)GLimp_GetProcAddress("glGetProgramivARB");
			qglGetProgramStringARB = (PFNGLGETPROGRAMSTRINGARBPROC)GLimp_GetProcAddress("glGetProgramStringARB");
			qglGetVertexAttribdvARB = (PFNGLGETVERTEXATTRIBDVARBPROC)GLimp_GetProcAddress("glGetVertexAttribdvARB");
			qglGetVertexAttribfvARB = (PFNGLGETVERTEXATTRIBFVARBPROC)GLimp_GetProcAddress("glGetVertexAttribfvARB");
			qglGetVertexAttribivARB = (PFNGLGETVERTEXATTRIBIVARBPROC)GLimp_GetProcAddress("glGetVertexAttribivARB");
			qglGetVertexAttribPointervARB = (PFNGLGETVERTEXATTRIBPOINTERVARBPROC)GLimp_GetProcAddress("glGetVertexAttribPointervARB");
			qglIsProgramARB = (PFNGLISPROGRAMARBPROC)GLimp_GetProcAddress("glIsProgramARB");

			glGetIntegerv(GL_MAX_TEXTURE_COORDS_ARB, &ri.config.ext.maxTexCoords);
			glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS_ARB, &ri.config.ext.maxTexImageUnits);

			if(!qglVertexAttrib1dARB || !qglVertexAttrib1dvARB || !qglVertexAttrib1fARB || !qglVertexAttrib1fvARB
			|| !qglVertexAttrib1sARB || !qglVertexAttrib1svARB || !qglVertexAttrib2dARB || !qglVertexAttrib2dvARB
			|| !qglVertexAttrib2fARB || !qglVertexAttrib2fvARB || !qglVertexAttrib2sARB || !qglVertexAttrib2svARB
			|| !qglVertexAttrib3dARB || !qglVertexAttrib3dvARB || !qglVertexAttrib3fARB || !qglVertexAttrib3fvARB
			|| !qglVertexAttrib3sARB || !qglVertexAttrib3svARB || !qglVertexAttrib4NbvARB || !qglVertexAttrib4NivARB
			|| !qglVertexAttrib4NsvARB || !qglVertexAttrib4NubARB || !qglVertexAttrib4NubvARB || !qglVertexAttrib4NuivARB
			|| !qglVertexAttrib4NusvARB || !qglVertexAttrib4bvARB || !qglVertexAttrib4dARB || !qglVertexAttrib4dvARB
			|| !qglVertexAttrib4fARB || !qglVertexAttrib4fvARB || !qglVertexAttrib4ivARB || !qglVertexAttrib4sARB
			|| !qglVertexAttrib4svARB || !qglVertexAttrib4ubvARB || !qglVertexAttrib4uivARB || !qglVertexAttrib4usvARB
			|| !qglVertexAttribPointerARB || !qglEnableVertexAttribArrayARB || !qglDisableVertexAttribArrayARB || !qglProgramStringARB
			|| !qglBindProgramARB || !qglDeleteProgramsARB || !qglGenProgramsARB || !qglProgramEnvParameter4dARB
			|| !qglProgramEnvParameter4dvARB || !qglProgramEnvParameter4fARB || !qglProgramEnvParameter4fvARB || !qglProgramLocalParameter4dARB
			|| !qglProgramLocalParameter4dvARB || !qglProgramLocalParameter4fARB || !qglProgramLocalParameter4fvARB || !qglGetProgramEnvParameterdvARB
			|| !qglGetProgramEnvParameterfvARB || !qglGetProgramLocalParameterdvARB || !qglGetProgramLocalParameterfvARB || !qglGetProgramivARB
			|| !qglGetProgramStringARB || !qglGetVertexAttribdvARB || !qglGetVertexAttribfvARB || !qglGetVertexAttribivARB
			|| !qglGetVertexAttribPointervARB || !qglIsProgramARB
			|| ri.config.ext.maxTexCoords <= 0 || ri.config.ext.maxTexImageUnits <= 0)
			{
				Com_Printf(PRNT_ERROR, "GL_ARB_vertex_program and GL_ARB_fragment_program not properly supported!\n");
			}
			else
			{
				Com_Printf(0, "...enabling GL_ARB_vertex_program\n");
				Com_Printf(0, "...enabling GL_ARB_fragment_program\n");
				Com_Printf(0, "...* Max texture coordinates: %i\n", ri.config.ext.maxTexCoords);
				Com_Printf(0, "...* Max texture image units: %i\n", ri.config.ext.maxTexImageUnits);
				ri.config.ext.bPrograms = true;
			}
		}
		else
			Com_Printf(0, "...GL_ARB_vertex_program or GL_ARB_fragment_program not found\n");
	}
	else
		Com_Printf(0, "...ignoring GL_ARB_vertex_program and GL_ARB_fragment_program\n");

	/*
	** GL_ARB_shader_objects
	*/
	if (ExtensionFound("GL_ARB_shader_objects"))
	{
		qglDeleteObjectARB = (PFNGLDELETEOBJECTARBPROC)GLimp_GetProcAddress("glDeleteObjectARB");
		qglGetHandleARB = (PFNGLGETHANDLEARBPROC)GLimp_GetProcAddress("glGetHandleARB");
		qglDetachObjectARB = (PFNGLDETACHOBJECTARBPROC)GLimp_GetProcAddress("glDetachObjectARB");
		qglCreateShaderObjectARB = (PFNGLCREATESHADEROBJECTARBPROC)GLimp_GetProcAddress("glCreateShaderObjectARB");
		qglShaderSourceARB = (PFNGLSHADERSOURCEARBPROC)GLimp_GetProcAddress("glShaderSourceARB");
		qglCompileShaderARB = (PFNGLCOMPILESHADERARBPROC)GLimp_GetProcAddress("glCompileShaderARB");
		qglCreateProgramObjectARB = (PFNGLCREATEPROGRAMOBJECTARBPROC)GLimp_GetProcAddress("glCreateProgramObjectARB");
		qglAttachObjectARB = (PFNGLATTACHOBJECTARBPROC)GLimp_GetProcAddress("glAttachObjectARB");
		qglLinkProgramARB = (PFNGLLINKPROGRAMARBPROC)GLimp_GetProcAddress("glLinkProgramARB");
		qglUseProgramObjectARB = (PFNGLUSEPROGRAMOBJECTARBPROC)GLimp_GetProcAddress("glUseProgramObjectARB");
		qglValidateProgramARB = (PFNGLVALIDATEPROGRAMARBPROC)GLimp_GetProcAddress("glValidateProgramARB");
		qglUniform1fARB = (PFNGLUNIFORM1FARBPROC)GLimp_GetProcAddress("glUniform1fARB");
		qglUniform2fARB = (PFNGLUNIFORM2FARBPROC)GLimp_GetProcAddress("glUniform2fARB");
		qglUniform3fARB = (PFNGLUNIFORM3FARBPROC)GLimp_GetProcAddress("glUniform3fARB");
		qglUniform4fARB = (PFNGLUNIFORM4FARBPROC)GLimp_GetProcAddress("glUniform4fARB");
		qglUniform1iARB = (PFNGLUNIFORM1IARBPROC)GLimp_GetProcAddress("glUniform1iARB");
		qglUniform2iARB = (PFNGLUNIFORM2IARBPROC)GLimp_GetProcAddress("glUniform2iARB");
		qglUniform3iARB = (PFNGLUNIFORM3IARBPROC)GLimp_GetProcAddress("glUniform3iARB");
		qglUniform4iARB = (PFNGLUNIFORM4IARBPROC)GLimp_GetProcAddress("glUniform4iARB");
		qglUniform1fvARB = (PFNGLUNIFORM1FVARBPROC)GLimp_GetProcAddress("glUniform1fvARB");
		qglUniform2fvARB = (PFNGLUNIFORM2FVARBPROC)GLimp_GetProcAddress("glUniform2fvARB");
		qglUniform3fvARB = (PFNGLUNIFORM3FVARBPROC)GLimp_GetProcAddress("glUniform3fvARB");
		qglUniform4fvARB = (PFNGLUNIFORM4FVARBPROC)GLimp_GetProcAddress("glUniform4fvARB");
		qglUniform1ivARB = (PFNGLUNIFORM1IVARBPROC)GLimp_GetProcAddress("glUniform1ivARB");
		qglUniform2ivARB = (PFNGLUNIFORM2IVARBPROC)GLimp_GetProcAddress("glUniform2ivARB");
		qglUniform3ivARB = (PFNGLUNIFORM3IVARBPROC)GLimp_GetProcAddress("glUniform3ivARB");
		qglUniform4ivARB = (PFNGLUNIFORM4IVARBPROC)GLimp_GetProcAddress("glUniform4ivARB");
		qglUniformMatrix2fvARB = (PFNGLUNIFORMMATRIX2FVARBPROC)GLimp_GetProcAddress("glUniformMatrix2fvARB");
		qglUniformMatrix3fvARB = (PFNGLUNIFORMMATRIX3FVARBPROC)GLimp_GetProcAddress("glUniformMatrix3fvARB");
		qglUniformMatrix4fvARB = (PFNGLUNIFORMMATRIX4FVARBPROC)GLimp_GetProcAddress("glUniformMatrix4fvARB");
		qglGetObjectParameterfvARB = (PFNGLGETOBJECTPARAMETERFVARBPROC)GLimp_GetProcAddress("glGetObjectParameterfvARB");
		qglGetObjectParameterivARB = (PFNGLGETOBJECTPARAMETERIVARBPROC)GLimp_GetProcAddress("glGetObjectParameterivARB");
		qglGetInfoLogARB = (PFNGLGETINFOLOGARBPROC)GLimp_GetProcAddress("glGetInfoLogARB");
		qglGetAttachedObjectsARB = (PFNGLGETATTACHEDOBJECTSARBPROC)GLimp_GetProcAddress("glGetAttachedObjectsARB");
		qglGetUniformLocationARB = (PFNGLGETUNIFORMLOCATIONARBPROC)GLimp_GetProcAddress("glGetUniformLocationARB");
		qglGetActiveUniformARB = (PFNGLGETACTIVEUNIFORMARBPROC)GLimp_GetProcAddress("glGetActiveUniformARB");
		qglGetUniformfvARB = (PFNGLGETUNIFORMFVARBPROC)GLimp_GetProcAddress("glGetUniformfvARB");
		qglGetUniformivARB = (PFNGLGETUNIFORMIVARBPROC)GLimp_GetProcAddress("glGetUniformivARB");
		qglGetShaderSourceARB = (PFNGLGETSHADERSOURCEARBPROC)GLimp_GetProcAddress("glGetShaderSourceARB");

		if (!qglDeleteObjectARB || !qglGetHandleARB || !qglDetachObjectARB || !qglCreateShaderObjectARB
		|| !qglShaderSourceARB || !qglCompileShaderARB || !qglCreateProgramObjectARB || !qglAttachObjectARB
		|| !qglLinkProgramARB || !qglUseProgramObjectARB || !qglValidateProgramARB || !qglUniform1fARB
		|| !qglUniform2fARB || !qglUniform3fARB || !qglUniform4fARB || !qglUniform1iARB
		|| !qglUniform2iARB || !qglUniform3iARB || !qglUniform4iARB || !qglUniform1fvARB
		|| !qglUniform2fvARB || !qglUniform3fvARB || !qglUniform4fvARB || !qglUniform1ivARB
		|| !qglUniform2ivARB || !qglUniform3ivARB || !qglUniform4ivARB || !qglUniformMatrix2fvARB
		|| !qglUniformMatrix3fvARB || !qglUniformMatrix4fvARB || !qglGetObjectParameterfvARB || !qglGetObjectParameterivARB
		|| !qglGetInfoLogARB || !qglGetAttachedObjectsARB || !qglGetUniformLocationARB || !qglGetActiveUniformARB
		|| !qglGetUniformfvARB || !qglGetUniformivARB || !qglGetShaderSourceARB)
		{
			Com_Printf(PRNT_ERROR, "GL_ARB_shader_objects not properly supported!\n");
		}
		else
		{
			Com_Printf(0, "...enabling GL_ARB_shader_objects\n");
			ri.config.ext.bShaderObjects = true;
		}
	}
	else
		Com_Printf(0, "...GL_ARB_shader_objects not found\n");

	/*
	** GL_ARB_vertex_shader
	** GL_ARB_fragment_shader
	*/
	if (r_ext_shaders->intVal)
	{
		if (!ri.config.ext.bPrograms)
		{
			Com_Printf(0, "...ignoring GL_ARB_vertex_shader and GL_ARB_fragment_shader because GL_ARB_vertex/fragment_program was not found\n");
		}
		else if (!ri.config.ext.bShaderObjects)
		{
			Com_Printf(0, "...ignoring GL_ARB_vertex_shader and GL_ARB_fragment_shader because GL_ARB_shader_objects was not found\n");
		}
		else if (!ExtensionFound("GL_ARB_shading_language_100"))
		{
			Com_Printf(0, "...ignoring GL_ARB_vertex_shader and GL_ARB_fragment_shader because GL_ARB_shading_language_100 was not found\n");
		}
		else if (ExtensionFound("GL_ARB_vertex_shader")
		&& ExtensionFound("GL_ARB_fragment_shader"))
		{
			qglBindAttribLocationARB = (PFNGLBINDATTRIBLOCATIONARBPROC)GLimp_GetProcAddress("glBindAttribLocationARB");
			qglGetActiveAttribARB = (PFNGLGETACTIVEATTRIBARBPROC)GLimp_GetProcAddress("glGetActiveAttribARB");
			qglGetAttribLocationARB = (PFNGLGETATTRIBLOCATIONARBPROC)GLimp_GetProcAddress("glGetAttribLocationARB");

			glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS_ARB, &ri.config.ext.maxVertexUniforms);
			glGetIntegerv(GL_MAX_VARYING_FLOATS_ARB, &ri.config.ext.maxVaryingFloats);
			glGetIntegerv(GL_MAX_VERTEX_ATTRIBS_ARB, &ri.config.ext.maxVertexAttribs);
			glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS_ARB, &ri.config.ext.maxFragmentUniforms);

			if (!qglBindAttribLocationARB || !qglGetActiveAttribARB || !qglGetAttribLocationARB
			|| ri.config.ext.maxVertexUniforms <= 0 || ri.config.ext.maxVaryingFloats <= 0
			|| ri.config.ext.maxVertexAttribs <= 0 || ri.config.ext.maxFragmentUniforms <= 0)
			{
				Com_Printf(PRNT_ERROR, "GL_ARB_vertex_shader and GL_ARB_fragment_shader not properly supported!\n");
			}
			else
			{
				Com_Printf(0, "...enabling GL_ARB_vertex_shader\n");
				Com_Printf(0, "...enabling GL_ARB_fragment_shader\n");
				Com_Printf(0, "...* Max vertex uniform components: %i\n", ri.config.ext.maxVertexUniforms);
				Com_Printf(0, "...* Max varying floats: %i\n", ri.config.ext.maxVaryingFloats);
				Com_Printf(0, "...* Max vertex attribs: %i\n", ri.config.ext.maxVertexAttribs);
				Com_Printf(0, "...* Max fragment uniform components: %i\n", ri.config.ext.maxFragmentUniforms);
				ri.config.ext.bShaders = true;
			}
		}
		else
			Com_Printf(0, "...GL_ARB_vertex_shader or GL_ARB_fragment_shader not found\n");
	}
	else
		Com_Printf(0, "...ignoring GL_ARB_vertex_shader and GL_ARB_fragment_shader\n");

	/*
	** GL_ARB_vertex_buffer_object
	*/
	if (r_ext_vertexBufferObject->intVal)
	{
		if (ExtensionFound("GL_ARB_vertex_buffer_object"))
		{
			qglBindBufferARB = (PFNGLBINDBUFFERARBPROC)GLimp_GetProcAddress("glBindBufferARB");
			qglDeleteBuffersARB = (PFNGLDELETEBUFFERSARBPROC)GLimp_GetProcAddress("glDeleteBuffersARB");
			qglGenBuffersARB = (PFNGLGENBUFFERSARBPROC)GLimp_GetProcAddress("glGenBuffersARB");
			qglIsBufferARB = (PFNGLISBUFFERARBPROC)GLimp_GetProcAddress("glIsBufferARB");
			qglMapBufferARB = (PFNGLMAPBUFFERARBPROC)GLimp_GetProcAddress("glMapBufferARB");
			qglUnmapBufferARB = (PFNGLUNMAPBUFFERARBPROC)GLimp_GetProcAddress("glUnmapBufferARB");
			qglBufferDataARB = (PFNGLBUFFERDATAARBPROC)GLimp_GetProcAddress("glBufferDataARB");
			qglBufferSubDataARB = (PFNGLBUFFERSUBDATAARBPROC)GLimp_GetProcAddress("glBufferSubDataARB");

			if(!qglBindBufferARB || !qglDeleteBuffersARB || !qglGenBuffersARB || !qglIsBufferARB
			|| !qglMapBufferARB || !qglUnmapBufferARB || !qglBufferDataARB || !qglBufferSubDataARB)
			{
				Com_Printf(PRNT_ERROR, "GL_ARB_vertex_buffer_object not properly supported!\n");
			}
			else
			{
				Com_Printf(0, "...enabling GL_ARB_vertex_buffer_object\n");
				ri.config.ext.bVertexBufferObject = true;
			}
		}
		else
			Com_Printf(0, "...GL_ARB_vertex_buffer_object not found\n");
	}
	else
		Com_Printf(0, "...ignoring GL_ARB_vertex_buffer_object\n");

	if (r_ext_frameBufferObject->intVal)
	{
		if (ExtensionFound("GL_EXT_framebuffer_object"))
		{
			qglIsRenderbufferEXT = (PFNGLISRENDERBUFFEREXTPROC)GLimp_GetProcAddress("glIsRenderbufferEXT");
			qglBindRenderbufferEXT = (PFNGLBINDRENDERBUFFEREXTPROC)GLimp_GetProcAddress("glBindRenderbufferEXT");
			qglDeleteRenderbuffersEXT = (PFNGLDELETERENDERBUFFERSEXTPROC)GLimp_GetProcAddress("glDeleteRenderbuffersEXT");
			qglGenRenderbuffersEXT = (PFNGLGENRENDERBUFFERSEXTPROC)GLimp_GetProcAddress("glGenRenderbuffersEXT");
			qglRenderbufferStorageEXT = (PFNGLRENDERBUFFERSTORAGEEXTPROC)GLimp_GetProcAddress("glRenderbufferStorageEXT");
			qglGetRenderbufferParameterivEXT = (PFNGLGETRENDERBUFFERPARAMETERIVEXTPROC)GLimp_GetProcAddress("glGetRenderbufferParameterivEXT");
			qglIsFramebufferEXT = (PFNGLISFRAMEBUFFEREXTPROC)GLimp_GetProcAddress("glIsFramebufferEXT");
			qglBindFramebufferEXT = (PFNGLBINDFRAMEBUFFEREXTPROC)GLimp_GetProcAddress("glBindFramebufferEXT");
			qglDeleteFramebuffersEXT = (PFNGLDELETEFRAMEBUFFERSEXTPROC)GLimp_GetProcAddress("glDeleteFramebuffersEXT");
			qglGenFramebuffersEXT = (PFNGLGENFRAMEBUFFERSEXTPROC)GLimp_GetProcAddress("glGenFramebuffersEXT");
			qglCheckFramebufferStatusEXT = (PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC)GLimp_GetProcAddress("glCheckFramebufferStatusEXT");
			qglFramebufferTexture1DEXT = (PFNGLFRAMEBUFFERTEXTURE1DEXTPROC)GLimp_GetProcAddress("glFramebufferTexture1DEXT");
			qglFramebufferTexture2DEXT = (PFNGLFRAMEBUFFERTEXTURE2DEXTPROC)GLimp_GetProcAddress("glFramebufferTexture2DEXT");
			qglFramebufferTexture3DEXT = (PFNGLFRAMEBUFFERTEXTURE3DEXTPROC)GLimp_GetProcAddress("glFramebufferTexture3DEXT");
			qglFramebufferRenderbufferEXT = (PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC)GLimp_GetProcAddress("glFramebufferRenderbufferEXT");
			qglGetFramebufferAttachmentParameterivEXT = (PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVEXTPROC)GLimp_GetProcAddress("glGetFramebufferAttachmentParameterivEXT");
			qglGenerateMipmapEXT = (PFNGLGENERATEMIPMAPEXTPROC)GLimp_GetProcAddress("glGenerateMipmapEXT");

			if (!qglIsRenderbufferEXT || !qglBindRenderbufferEXT || !qglDeleteRenderbuffersEXT || !qglGenRenderbuffersEXT
			|| !qglRenderbufferStorageEXT || !qglGetRenderbufferParameterivEXT || !qglIsFramebufferEXT || !qglBindFramebufferEXT
			|| !qglDeleteFramebuffersEXT || !qglGenFramebuffersEXT || !qglCheckFramebufferStatusEXT || !qglFramebufferTexture1DEXT
			|| !qglFramebufferTexture2DEXT || !qglFramebufferTexture3DEXT || !qglFramebufferRenderbufferEXT || !qglGetFramebufferAttachmentParameterivEXT
			|| !qglGenerateMipmapEXT)
			{
				Com_Printf(PRNT_ERROR, "GL_EXT_framebuffer_object not properly supported!\n");
			}
			else
			{
				Com_Printf(0, "...enabling GL_EXT_framebuffer_object\n");
				ri.config.ext.bFrameBufferObject = true;
			}
		}
		else
			Com_Printf(0, "...GL_EXT_framebuffer_object not found\n");
	}
	else
		Com_Printf(0, "...ignoring GL_EXT_framebuffer_object\n");

	/*
	** GL_EXT_bgra
	*/
	if (r_ext_BGRA->intVal)
	{
		if (ExtensionFound("GL_EXT_bgra"))
		{
			Com_Printf(0, "...enabling GL_EXT_bgra\n");
			ri.config.ext.bBGRA = true;
		}
		else
			Com_Printf(0, "...GL_EXT_bgra not found\n");
	}
	else
		Com_Printf(0, "...ignoring GL_EXT_bgra\n");

	/*
	** GL_ARB_depth_texture
	*/
	if (ExtensionFound("GL_ARB_depth_texture"))
	{
		Com_Printf(0, "...enabling GL_ARB_depth_texture\n");
		ri.config.ext.bDepthTexture = true;
	}
	else
		Com_Printf(0, "...GL_ARB_depth_texture not found\n");

	/*
	** GL_ARB_shadow
	*/
	if (ExtensionFound("GL_ARB_shadow"))
	{
		if (!ri.config.ext.bDepthTexture)
		{
			Com_Printf(0, "...ignoring GL_ARB_shadow because GL_ARB_depth_texture was not found\n");
		}
		else
		{
			Com_Printf(0, "...enabling GL_ARB_shadow\n");
			ri.config.ext.bARBShadow = true;
		}
	}
	else
		Com_Printf(0, "...GL_ARB_shadow not found\n");

	/*
	** GL_EXT_compiled_vertex_array
	** GL_SGI_compiled_vertex_array
	*/
	if (r_ext_compiledVertexArray->intVal)
	{
		if (ExtensionFound("GL_EXT_compiled_vertex_array")
		|| ExtensionFound("GL_SGI_compiled_vertex_array"))
		{
			if (r_ext_compiledVertexArray->intVal != 2
			&& (ri.renderClass == REND_CLASS_INTEL || ri.renderClass == REND_CLASS_S3 || ri.renderClass == REND_CLASS_SIS))
			{
				Com_Printf(PRNT_WARNING, "...forcibly ignoring GL_EXT/SGI_compiled_vertex_array\n"
								"...* Your card is known for not supporting it properly\n"
								"...* If you would like it enabled, set r_ext_compiledVertexArray to 2\n");
			}
			else
			{
				qglLockArraysEXT = (PFNGLLOCKARRAYSEXTPROC)GLimp_GetProcAddress("glLockArraysEXT");
				qglUnlockArraysEXT = (PFNGLUNLOCKARRAYSEXTPROC)GLimp_GetProcAddress("glUnlockArraysEXT");

				if (!qglLockArraysEXT || !qglUnlockArraysEXT)
				{
					Com_Printf(PRNT_ERROR, "...GL_EXT/SGI_compiled_vertex_array not properly supported!\n");
				}
				else
				{
					Com_Printf(0, "...enabling GL_EXT/SGI_compiled_vertex_array\n");
					ri.config.ext.bCompiledVertArray = true;
				}
			}
		}
		else
			Com_Printf(0, "...GL_EXT/SGI_compiled_vertex_array not found\n");
	}
	else
		Com_Printf(0, "...ignoring GL_EXT/SGI_compiled_vertex_array\n");

	/*
	** GL_EXT_draw_range_elements
	*/
	if (r_ext_drawRangeElements->intVal)
	{
		if (ExtensionFound("GL_EXT_draw_range_elements"))
		{
			// These are not actual maximums, but rather recommendations for performance...
			glGetIntegerv(GL_MAX_ELEMENTS_VERTICES_EXT, &ri.config.ext.maxElementVerts);
			glGetIntegerv(GL_MAX_ELEMENTS_INDICES_EXT, &ri.config.ext.maxElementIndices);

			if (ri.config.ext.maxElementVerts > 0 && ri.config.ext.maxElementIndices > 0)
			{
				qglDrawRangeElementsEXT = (PFNGLDRAWRANGEELEMENTSEXTPROC)GLimp_GetProcAddress("glDrawRangeElementsEXT");
				if (!qglDrawRangeElementsEXT)
					qglDrawRangeElementsEXT = (PFNGLDRAWRANGEELEMENTSPROC)GLimp_GetProcAddress("glDrawRangeElements");
			}

			if (!qglDrawRangeElementsEXT)
			{
				Com_Printf(PRNT_ERROR, "...GL_EXT_draw_range_elements not properly supported!\n");
			}
			else
			{
				Com_Printf(0, "...enabling GL_EXT_draw_range_elements\n");
				Com_Printf(0, "...* Max element vertices: %i\n", ri.config.ext.maxElementVerts);
				Com_Printf(0, "...* Max element indices: %i\n", ri.config.ext.maxElementIndices);
				ri.config.ext.bDrawRangeElements = true;
			}
		}
		else
			Com_Printf(0, "...GL_EXT_draw_range_elements not found\n");
	}
	else
		Com_Printf(0, "...ignoring GL_EXT_draw_range_elements\n");

	/*
	** GL_EXT_texture3D
	*/
	if (r_ext_texture3D->intVal)
	{
		if (ExtensionFound("GL_EXT_texture3D"))
		{
			qglTexImage3D = (PFNGLTEXIMAGE3DEXTPROC)GLimp_GetProcAddress("glTexImage3D");
			qglTexSubImage3D = (PFNGLTEXSUBIMAGE3DEXTPROC)GLimp_GetProcAddress("glTexSubImage3D");
			glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &ri.config.ext.max3DTexSize);

			if (!qglTexImage3D || !qglTexSubImage3D || ri.config.ext.max3DTexSize <= 0)
			{
				Com_Printf(PRNT_ERROR, "...GL_EXT_texture3D not properly supported!\n");
			}
			else
			{
				ri.config.ext.max3DTexSize = Q_NearestPow<int>(ri.config.ext.max3DTexSize, true);
				Com_Printf(0, "...enabling GL_EXT_texture3D\n");
				Com_Printf(0, "...* Max 3D texture size: %i\n", ri.config.ext.max3DTexSize);
				ri.config.ext.bTex3D = true;
			}
		}
		else
			Com_Printf(0, "...GL_EXT_texture3D not found\n");
	}
	else
		Com_Printf(0, "...ignoring GL_EXT_texture3D\n");

	/*
	** GL_EXT_texture_edge_clamp
	*/
	if (r_ext_textureEdgeClamp->intVal)
	{
		if (ExtensionFound("GL_EXT_texture_edge_clamp"))
		{
			Com_Printf(0, "...enabling GL_EXT_texture_edge_clamp\n");
			ri.config.ext.bTexEdgeClamp = true;
		}
		else
			Com_Printf(0, "...GL_EXT_texture_edge_clamp not found\n");
	}
	else
		Com_Printf(0, "...ignoring GL_EXT_texture_edge_clamp\n");

	/*
	** GL_EXT_texture_filter_anisotropic
	*/
	if (r_ext_textureFilterAnisotropic->intVal)
	{
		if (ExtensionFound("GL_EXT_texture_filter_anisotropic"))
		{
			glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &ri.config.ext.maxAniso);
			if (ri.config.ext.maxAniso <= 0)
			{
				Com_Printf(PRNT_ERROR, "...GL_EXT_texture_filter_anisotropic not properly supported!\n");
				ri.config.ext.maxAniso = 0;
			}
			else
			{
				Com_Printf(0, "...enabling GL_EXT_texture_filter_anisotropic\n");
				Com_Printf(0, "...* Max texture anisotropy: %i\n", ri.config.ext.maxAniso);
				ri.config.ext.bTexFilterAniso = true;
			}
		}
		else
			Com_Printf(0, "...GL_EXT_texture_filter_anisotropic not found\n");
	}
	else
	{
		// Query anyway, so the video menu knows what the maximum possible value could be
		if (ExtensionFound("GL_EXT_texture_filter_anisotropic"))
			glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &ri.config.ext.maxAniso);

		Com_Printf(0, "...ignoring GL_EXT_texture_filter_anisotropic\n");
	}

	/*
	** GL_EXT_stencil_two_side
	*/
	if (r_ext_stencilTwoSide->intVal)
	{
		if (ExtensionFound("GL_EXT_stencil_two_side"))
		{
			qglActiveStencilFaceEXT = (PFNGLACTIVESTENCILFACEEXTPROC)GLimp_GetProcAddress("glActiveStencilFaceEXT");

			if (!qglActiveStencilFaceEXT)
			{
				Com_Printf(PRNT_ERROR, "...GL_EXT_stencil_two_side not properly supported!\n");
			}
			else
			{
				Com_Printf(0, "...enabling GL_EXT_stencil_two_side\n");
				ri.config.ext.bStencilTwoSide = true;
			}
		}
		else
			Com_Printf(0, "...GL_EXT_stencil_two_side not found\n");
	}
	else
		Com_Printf(0, "...ignoring GL_EXT_stencil_two_side\n");

	/*
	** GL_EXT_stencil_wrap
	*/
	if (r_ext_stencilWrap->intVal)
	{
		if (ExtensionFound("GL_EXT_stencil_wrap"))
		{
			Com_Printf(0, "...enabling GL_EXT_stencil_wrap\n");
			ri.config.ext.bStencilWrap = true;
		}
		else
			Com_Printf(0, "...GL_EXT_stencil_wrap not found\n");
	}
	else
		Com_Printf(0, "...ignoring GL_EXT_stencil_wrap\n");

#ifdef WIN32
	/*
	** WGL_3DFX_gamma_control
	*/
	if (ExtensionFound("WGL_3DFX_gamma_control"))
	{
		qwglGetDeviceGammaRamp3DFX = (BOOL(APIENTRYP)(HDC,WORD*))GLimp_GetProcAddress("wglGetDeviceGammaRamp3DFX");
		qwglSetDeviceGammaRamp3DFX = (BOOL(APIENTRYP)(HDC,WORD*))GLimp_GetProcAddress("wglSetDeviceGammaRamp3DFX");

		if (!qwglGetDeviceGammaRamp3DFX || !qwglSetDeviceGammaRamp3DFX)
		{
			Com_Printf(PRNT_ERROR, "...WGL_3DFX_gamma_control not properly supported!\n");
		}
	}

	/*
	** WGL_EXT_swap_control
	*/
	if (r_ext_swapInterval->intVal)
	{
		if (ExtensionFound("WGL_EXT_swap_control"))
		{
			if (!ri.config.ext.bWinSwapInterval)
			{
				qwglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)GLimp_GetProcAddress("wglSwapIntervalEXT");

				if (!qwglSwapIntervalEXT)
				{
					Com_Printf(PRNT_ERROR, "...WGL_EXT_swap_control not properly supported!\n");
				}
				else
				{
					Com_Printf(0, "...enabling WGL_EXT_swap_control\n");
					ri.config.ext.bWinSwapInterval = true;
				}
			}
		}
		else
			Com_Printf(0, "...WGL_EXT_swap_control not found\n");
	}
	else
		Com_Printf(0, "...ignoring WGL_EXT_swap_control\n");
#elif defined __unix__
	qglXChooseVisual = GLimp_GetProcAddress("glXChooseVisual");
	qglXCreateContext = GLimp_GetProcAddress("glXCreateContext");
	qglXDestroyContext = GLimp_GetProcAddress("glXDestroyContext");
	qglXMakeCurrent = GLimp_GetProcAddress("glXMakeCurrent");
	qglXCopyContext = GLimp_GetProcAddress("glXCopyContext");
	qglXSwapBuffers = GLimp_GetProcAddress("glXSwapBuffers");
#endif
}


/*
=============
RB_BackendInit
=============
*/
bool RB_BackendInit()
{
	Com_Printf (0, "\n=========== Refresh Backend ============\n");

	// Set extension/max defaults
	ri.config.ext.bARBMultitexture = false;
	ri.config.ext.bSGISMultiTexture = false;

	ri.config.ext.bBGRA = false;
	ri.config.ext.bCompiledVertArray = false;

	ri.config.ext.bDrawRangeElements = false;
	ri.config.ext.maxElementVerts = 0;
	ri.config.ext.maxElementIndices = 0;

	ri.config.ext.bFrameBufferObject = false;

	ri.config.ext.bPrograms = false;
	ri.config.ext.maxTexCoords = 0;
	ri.config.ext.maxTexImageUnits = 0;

	ri.config.ext.bShaderObjects = false;

	ri.config.ext.bShaders = false;
	ri.config.ext.maxVertexUniforms = 0;
	ri.config.ext.maxVaryingFloats = 0;
	ri.config.ext.maxVertexAttribs = 0;
	ri.config.ext.maxFragmentUniforms = 0;

	ri.config.ext.bDepthTexture = false;
	ri.config.ext.bARBShadow = false;

	ri.config.ext.bStencilTwoSide = false;
	ri.config.ext.bStencilWrap = false;

	ri.config.ext.bTex3D = false;
	ri.config.ext.max3DTexSize = 0;

	ri.config.ext.bTexCompression = false;

	ri.config.ext.bTexCubeMap = false;
	ri.config.ext.maxCMTexSize = 0;

	ri.config.ext.bTexEdgeClamp = false;
	ri.config.ext.bTexEnvAdd = false;
	ri.config.ext.bTexEnvCombine = false;
	ri.config.ext.bTexEnvDot3 = false;

	ri.config.ext.bTexFilterAniso = false;
	ri.config.ext.maxAniso = 0;

	ri.config.ext.bVertexBufferObject = false;
	ri.config.ext.bWinSwapInterval = false;

	ri.config.maxTexSize = 256;
	ri.config.maxTexUnits = 1;

	// Reset static refresh info
	ri.lastValidMode = -1;
	ri.bAllowStencil = false;
	ri.cColorBits = 0;
	ri.cAlphaBits = 0;
	ri.cDepthBits = 0;
	ri.cStencilBits = 0;

	// Initialize OS-specific parts of OpenGL
	if (!GLimp_Init())
	{
		Com_Printf(PRNT_ERROR, "...unable to init gl implementation\n");
		return false;
	}

	// Create the window and set up the context
	if (!R_SetMode())
	{
		Com_Printf(PRNT_ERROR, "...could not set video mode\n");
		return false;
	}

	// Vendor string
	ri.vendorString = glGetString(GL_VENDOR);
	Com_Printf(0, "GL_VENDOR: %s\n", ri.vendorString);

	char *vendorBuffer = Mem_PoolStrDup((char *)ri.vendorString, ri.genericPool, 0);
	Q_strlwr(vendorBuffer);

	// Renderer string
	ri.rendererString = glGetString(GL_RENDERER);
	Com_Printf(0, "GL_RENDERER: %s\n", ri.rendererString);

	char *rendererBuffer = Mem_PoolStrDup((char *)ri.rendererString, ri.genericPool, 0);
	Q_strlwr(rendererBuffer);

	// Version string
	ri.versionString = glGetString(GL_VERSION);
	Com_Printf(0, "GL_VERSION: %s\n", ri.versionString);

	// Extension string
	ri.extensionString = glGetString(GL_EXTENSIONS);

	// Decide on a renderer class
	if (strstr(rendererBuffer, "glint"))			ri.renderClass = REND_CLASS_3DLABS_GLINT_MX;
	else if (strstr(rendererBuffer, "permedia"))	ri.renderClass = REND_CLASS_3DLABS_PERMEDIA;
	else if (strstr(rendererBuffer, "glzicd"))		ri.renderClass = REND_CLASS_3DLABS_REALIZM;
	else if (strstr(vendorBuffer, "ati "))
	{
		if (strstr(vendorBuffer, "radeon"))
			ri.renderClass = REND_CLASS_ATI_RADEON;
		else
			ri.renderClass = REND_CLASS_ATI;
	}
	else if (strstr(vendorBuffer, "intel"))			ri.renderClass = REND_CLASS_INTEL;
	else if (strstr(vendorBuffer, "nvidia"))
	{
		if (strstr(rendererBuffer, "geforce"))
			ri.renderClass = REND_CLASS_NVIDIA_GEFORCE;
		else
			ri.renderClass = REND_CLASS_NVIDIA;
	}
	else if (strstr(rendererBuffer, "pmx"))			ri.renderClass = REND_CLASS_PMX;
	else if (strstr(rendererBuffer, "pcx1"))		ri.renderClass = REND_CLASS_POWERVR_PCX1;
	else if (strstr(rendererBuffer, "pcx2"))		ri.renderClass = REND_CLASS_POWERVR_PCX2;
	else if (strstr(rendererBuffer, "verite"))		ri.renderClass = REND_CLASS_RENDITION;
	else if (strstr(vendorBuffer, "s3"))			ri.renderClass = REND_CLASS_S3;
	else if (strstr(rendererBuffer, "prosavage"))	ri.renderClass = REND_CLASS_S3;
	else if (strstr(rendererBuffer, "twister"))		ri.renderClass = REND_CLASS_S3;
	else if (strstr(vendorBuffer, "sgi"))			ri.renderClass = REND_CLASS_SGI;
	else if (strstr(vendorBuffer, "sis"))			ri.renderClass = REND_CLASS_SIS;
	else if (strstr(rendererBuffer, "voodoo"))		ri.renderClass = REND_CLASS_VOODOO;
	else
	{
		if (strstr(rendererBuffer, "gdi generic"))
		{
			ri.renderClass = REND_CLASS_MCD;

			// MCD has buffering issues
			Cvar_VariableSetValue(gl_finish, 1, true);
		}
		else
			ri.renderClass = REND_CLASS_DEFAULT;
	}

	Mem_Free(rendererBuffer);
	Mem_Free(vendorBuffer);

	// Print the renderer class
	Com_Printf(0, "Renderer Class: %s\n", R_RendererClassName());

#ifdef GL_FORCEFINISH
	Cvar_VariableSetValue(gl_finish, 1, true);
#endif

	// Check stencil buffer availability and usability
	Com_Printf(0, "...stencil buffer ");
	if (gl_stencilbuffer->intVal && ri.cStencilBits > 0)
	{
		if (ri.renderClass == REND_CLASS_VOODOO)
		{
			Com_Printf(0, "ignored\n");
		}
		else
		{
			Com_Printf(0, "available\n");
			ri.bAllowStencil = true;
		}
	}
	else
	{
		Com_Printf(0, "disabled\n");
	}

	// Grab opengl extensions
	if (r_allowExtensions->intVal)
		GL_InitExtensions();
	else
		Com_Printf(0, "...ignoring OpenGL extensions\n");

	// Retreive generic information
	if (gl_maxTexSize->intVal >= 256)
	{
		ri.config.maxTexSize = Q_NearestPow<int>(gl_maxTexSize->intVal, true);
		Com_Printf(0, "Using forced maximum texture size of: %ix%i\n", ri.config.maxTexSize, ri.config.maxTexSize);
	}
	else
	{
		glGetIntegerv(GL_MAX_TEXTURE_SIZE, &ri.config.maxTexSize);
		ri.config.maxTexSize = Q_NearestPow<int>(ri.config.maxTexSize, true);
		if (ri.config.maxTexSize < 256)
		{
			Com_Printf(0, "Maximum texture size forced up to 256x256 from %i\n", ri.config.maxTexSize);
			ri.config.maxTexSize = 256;
		}
		else
		{
			Com_Printf(0, "Using video card maximum texture size of %ix%i\n", ri.config.maxTexSize, ri.config.maxTexSize);
		}
	}

	// Set the default state
	RB_SetDefaultState();

	// Subsystem initialization
	if (!RB_RenderInit())
		return false;

	RB_CheckForError("RB_BackendInit");
	Com_Printf(0, "========================================\n");
	return true;
}


/*
=============
RB_PostFrontEndInit
=============
*/
bool RB_PostFrontEndInit()
{
	if (!RB_InitShadows())
		return false;

	return true;
}


/*
=============
RB_BackendShutdown
=============
*/
void RB_BackendShutdown(const bool bFull)
{
	Com_Printf(0, "\n------- Refresh Backend Shutdown -------\n");

	// Shutdown subsystems
	RB_ShutdownShadows();
	RB_RenderShutdown();

	// Shutdown OS specific OpenGL stuff like contexts, etc
	GLimp_Shutdown(bFull);

	Com_Printf(0, "----------------------------------------\n");
}
