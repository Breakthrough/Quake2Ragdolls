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
// rf_image.h
//

/*
=============================================================================

	IMAGING

=============================================================================
*/

enum ETextureUnit
{
	TEXUNIT0,
	TEXUNIT1,
	TEXUNIT2,
	TEXUNIT3,
	TEXUNIT4,
	TEXUNIT5,
	TEXUNIT6,
	TEXUNIT7,

	MAX_TEXUNITS
};

typedef uint32 texFlags_t;
enum
{
	IT_3D					= BIT(0),		// 3d texture
	IT_CUBEMAP				= BIT(1),		// it's a cubemap env base image
	IT_LIGHTMAP				= BIT(2),		// lightmap texture
	IT_FBO					= BIT(3),		// frame buffer object texture
	IT_DEPTHANDFBO			= BIT(4),		// frame buffer object texture with a depth attachment texture
	IT_DEPTHFBO				= BIT(5),		// frame buffer object depth texture

	IF_CLAMP_S				= BIT(6),		// texcoords edge clamped
	IF_CLAMP_T				= BIT(7),		// texcoords edge clamped
	IF_CLAMP_R				= BIT(8),		// texcoords edge clamped (3D)
	IF_NOCOMPRESS			= BIT(9),		// no texture compression
	IF_NOGAMMA				= BIT(10),		// not affected by vid_gama
	IF_NOINTENS				= BIT(11),		// not affected by intensity
	IF_NOMIPMAP_LINEAR		= BIT(12),		// not mipmapped, linear filtering
	IF_NOMIPMAP_NEAREST		= BIT(13),		// not mipmapped, nearest filtering
	IF_NOPICMIP				= BIT(14),		// not affected by gl_picmip
	IF_NOFLUSH				= BIT(15),		// do not flush at the end of registration (internal only)
	IF_NOALPHA				= BIT(16),		// force alpha to 255
	IF_NORGB				= BIT(17),		// force rgb to 255 255 255
	IF_GREYSCALE			= BIT(18),		// uploaded as greyscale
};
#define IF_CLAMP_ALL (IF_CLAMP_S|IF_CLAMP_T|IF_CLAMP_R)
#define IF_NOMIPMAP_MASK (IF_NOMIPMAP_LINEAR|IF_NOMIPMAP_NEAREST)

struct refImage_t
{
	char					name[MAX_QPATH];				// game path, including extension
	char					bareName[MAX_QPATH];			// filename only, as called when searching

	int						width,		upWidth;			// source image
	int						height,		upHeight;			// after power of two and picmip
	int						depth,		upDepth;			// for 3d textures
	texFlags_t				flags;

	int						tcWidth,	tcHeight;			// special case for high-res texture scaling

	int						target;							// destination for binding
	int						upFormat;						// uploaded texture color component format

	uint32					touchFrame;						// free if this doesn't match the current frame
	uint32					fboNum, depthFBONum;			// used if it's an FBO
	uint32					texNum;							// gl texture binding, r_numImages + 1, can't be 0
	uint32					visFrame;						// frame this texture was last used in
};

#define MAX_IMAGES			1024			// maximum local images
#define MAX_LIGHTMAP_IMAGES	32				// maximum local lightmap textures
#define BAD_LMTEXNUM		-1

#define FOGTEX_WIDTH		256
#define FOGTEX_HEIGHT		32

extern const char			*r_skyNameSuffix[6];
extern const char			*r_cubeMapSuffix[6];

//
// rf_image.c
//

void GL_TextureBits(const bool bVerbose = true, bool bVerboseOnly = false);
void GL_TextureMode(const bool bVerbose = true, bool bVerboseOnly = false);

void GL_ResetAnisotropy();

#define R_Create2DImage(name,pic,width,height,flags,samples)		R_CreateImage((name),NULL,(pic),(width),(height),1,(flags),(samples),false,false)
#define R_Create3DImage(name,pic,width,height,depth,flags,samples)	R_CreateImage((name),NULL,(pic),(width),(height),(depth),(flags),(samples),false,false)

refImage_t *R_CreateImage(const char *name, const char *bareName,
						byte **pic,
						const int width, const int height, const int depth,
						const texFlags_t flags,
						const int samples,
						const bool bUpload8 = false, const bool bIsPCX = false);

#define R_TouchImage(img) ((img)->touchFrame = ri.reg.registerFrame, ri.reg.imagesTouched++)

refImage_t *R_RegisterImage(const char *name, texFlags_t flags);

void R_InitScreenTexture(refImage_t **Image, const char *Name, const int ID, const int ScreenWidth, const int ScreenHeight, const texFlags_t TexFlags, const int Samples);
void R_InitShadowMapTexture(refImage_t **Image, const int ID, const int ScreenWidth, const int ScreenHeight);

void R_EndImageRegistration();

void R_ResetGammaRamp();
void R_UpdateGammaRamp();

void R_ImageInit();
void R_ImageShutdown();
