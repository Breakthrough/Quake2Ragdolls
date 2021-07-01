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
// rf_image.c
// Loads and uploads images to the video card
// The image loading functions MUST remain local to this file, since memory allocations
// and released are "batched" as to relieve possible fragmentation.
//

#include "rf_local.h"

#ifdef WIN32
extern "C" {
# include "../libjpeg/jpeglib.h"
# include "../libpng/pngpriv.h"
}
#else
# include <jpeglib.h>
# include <png.h>
#endif

#include <unordered_map>

typedef std::tr1::unordered_map<std::string, refImage_t*> TImageList;
TImageList imageList;

static byte			r_intensityTable[256];
static byte			r_gammaTable[256];
static uint32		r_paletteTable[256];

const char			*r_cubeMapSuffix[6] = { "px", "nx", "py", "ny", "pz", "nz" };
const char			*r_skyNameSuffix[6] = { "rt", "bk", "lf", "ft", "up", "dn" };

static const GLenum r_cubeTargets[] =
{
	GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB,
	GL_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB,
	GL_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB
};

static byte r_defaultImagePal[] =
{
	#include "rf_defpal.h"
};

static int	r_imageAllocTag;

/*
==============================================================================

	TEXTURE STATE

==============================================================================
*/

/*
==================
GL_TextureBits
==================
*/
void GL_TextureBits(const bool bVerbose, bool bVerboseOnly)
{
	// Print to the console if desired
	if (bVerbose)
	{
		switch(r_textureBits->intVal)
		{
		case 32:	Com_Printf(0, "Texture bits: 32\n");		break;
		case 16:	Com_Printf(0, "Texture bits: 16\n");		break;
		default:	Com_Printf(0, "Texture bits: default\n");	break;
		}

		// Only print (don't set)
		if (bVerboseOnly)
			return;
	}

	// Set
	switch(r_textureBits->intVal)
	{
	case 32:
		ri.rgbFormat = GL_RGB8;
		ri.rgbaFormat = GL_RGBA8;
		ri.greyFormat = GL_LUMINANCE8;
		break;

	case 16:
		ri.rgbFormat = GL_RGB5;
		ri.rgbaFormat = GL_RGBA4;
		ri.greyFormat = GL_LUMINANCE4;
		break;

	default:
		ri.rgbFormat = GL_RGB;
		ri.rgbaFormat = GL_RGBA;
		ri.greyFormat = GL_LUMINANCE;
		break;
	}
}


/*
===============
GL_TextureMode
===============
*/
struct glTexMode_t
{
	char	*name;

	GLint	minimize;
	GLint	maximize;
};

static const glTexMode_t modes[] =
{
	{"GL_NEAREST",					GL_NEAREST,					GL_NEAREST},
	{"GL_LINEAR",					GL_LINEAR,					GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST",	GL_NEAREST_MIPMAP_NEAREST,	GL_NEAREST},
	{"GL_NEAREST_MIPMAP_LINEAR",	GL_NEAREST_MIPMAP_LINEAR,	GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST",	GL_LINEAR_MIPMAP_NEAREST,	GL_LINEAR},		// Bilinear
	{"GL_LINEAR_MIPMAP_LINEAR",		GL_LINEAR_MIPMAP_LINEAR,	GL_LINEAR}		// Trilinear
};

#define NUM_GL_MODES (sizeof(modes) / sizeof(glTexMode_t))

void GL_TextureMode(const bool bVerbose, bool bVerboseOnly)
{
	int i;

	// Find a matching mode
	for (i=0 ; i<NUM_GL_MODES ; i++)
	{
		if (!Q_stricmp(modes[i].name, gl_texturemode->string))
		{
			ri.texMinFilter = modes[i].minimize;
			ri.texMagFilter = modes[i].maximize;
			break;
		}
	}

	if (i == NUM_GL_MODES)
	{
		// Not found
		Com_Printf(PRNT_WARNING, "bad filter name '%s' -- falling back to default\n", gl_texturemode->string);
		Cvar_VariableReset(gl_texturemode, true);
		
		ri.texMinFilter = GL_LINEAR_MIPMAP_NEAREST;
		ri.texMagFilter = GL_LINEAR;
	}

	if (bVerbose)
	{
		Com_Printf(0, "Texture mode: %s\n", modes[i].name);
		if (bVerboseOnly)
			return;
	}

	// Change all the existing mipmap texture objects
	gl_texturemode->modified = false;
	for (auto it = imageList.begin() ; it != imageList.end(); ++it)
	{
		refImage_t *image = (*it).second;
		if (!image->touchFrame)
			continue;	// Free r_imageList slot
		if (image->flags & IF_NOMIPMAP_MASK)
			continue;

		RB_BindTexture(image);
		glTexParameteri(image->target, GL_TEXTURE_MIN_FILTER, ri.texMinFilter);
		glTexParameteri(image->target, GL_TEXTURE_MAG_FILTER, ri.texMagFilter);
	}
}


/*
===============
GL_ResetAnisotropy
===============
*/
void GL_ResetAnisotropy()
{
	r_ext_maxAnisotropy->modified = false;
	if (!ri.config.ext.bTexFilterAniso)
		return;

	// Change all the existing mipmap texture objects
	if (r_ext_maxAnisotropy->intVal < 1)
		Cvar_VariableSetValue(r_ext_maxAnisotropy, 1, true);
	else if (r_ext_maxAnisotropy->intVal > ri.config.ext.maxAniso)
		Cvar_VariableSetValue(r_ext_maxAnisotropy, ri.config.ext.maxAniso, true);

	for (auto it = imageList.begin() ; it != imageList.end(); ++it)
	{
		refImage_t *image = (*it).second;
		if (!image->touchFrame)
			continue;	// Free r_imageList slot
		if (image->flags & IF_NOMIPMAP_MASK)
			continue;	// Skip non-mipmapped imagery

		RB_BindTexture(image);
		glTexParameteri(image->target, GL_TEXTURE_MAX_ANISOTROPY_EXT, r_ext_maxAnisotropy->intVal);
	}
}

/*
==============================================================================

	TEXTURE BUFFERS

	Used as temporary writing space while loading an image.
==============================================================================
*/

enum ETexBuffer
{
	// Enough loading buffers for all sides of a cubemap
	TEXBUF_LOADING_0,
	TEXBUF_LOADING_1,
	TEXBUF_LOADING_2,
	TEXBUF_LOADING_3,
	TEXBUF_LOADING_4,
	TEXBUF_LOADING_5,
	TEXBUF_PAL,
	TEXBUF_RESAMPLE,
	TEXBUF_SCRATCH,

	TEXBUF_MAX
};

static byte *r_textureBuffers[TEXBUF_MAX];
static size_t r_texBuffSize[TEXBUF_MAX];

/*
===============
_R_AllocateTexBuffer
===============
*/
#define R_AllocateTexBuffer(Buffer,Size) _R_AllocateTexBuffer((Buffer),(Size),__FILE__,__LINE__)
byte *_R_AllocateTexBuffer(const ETexBuffer Buffer, const size_t Size, const char *FileName, const int FileLine)
{
	if (r_textureBuffers[Buffer])
	{
		// If it's large enough, just use the current buffer
		if (r_texBuffSize[Buffer] >= Size)
			return r_textureBuffers[Buffer];

		// Release and prepare to re-allocation
		_Mem_Free(r_textureBuffers[Buffer], FileName, FileLine);
	}

	// Allocate
	r_texBuffSize[Buffer] = Size;
	r_textureBuffers[Buffer] = (byte*)_Mem_Alloc(Size, ri.imageSysPool, 0, FileName, FileLine);
	return r_textureBuffers[Buffer];
}


/*
===============
R_ReleaseTexBuffers
===============
*/
void R_ReleaseTexBuffers()
{
	for (int i=0 ; i<TEXBUF_MAX ; i++)
	{
		if (r_textureBuffers[i])
		{
			Mem_Free(r_textureBuffers[i]);
			r_textureBuffers[i] = NULL;;
		}
	}
}

/*
==============================================================================
 
	JPG
 
==============================================================================
*/

static void jpg_noop(j_decompress_ptr cinfo)
{
}

static void jpeg_d_error_exit(j_common_ptr cinfo)
{
	char msg[1024];

	(cinfo->err->format_message)(cinfo, msg);
	Com_Error(ERR_FATAL, "R_LoadJPG: JPEG Lib Error: '%s'", msg);
}

static boolean jpg_fill_input_buffer(j_decompress_ptr cinfo)
{
    Com_DevPrintf(PRNT_WARNING, "R_LoadJPG: Premeture end of jpeg file\n");

    return 1;
}

static void jpg_skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
    cinfo->src->next_input_byte += (size_t) num_bytes;
    cinfo->src->bytes_in_buffer -= (size_t) num_bytes;
}

static void jpeg_mem_src(j_decompress_ptr cinfo, byte *mem, int len)
{
    cinfo->src = (struct jpeg_source_mgr *)
	(*cinfo->mem->alloc_small)((j_common_ptr) cinfo,
				   JPOOL_PERMANENT,
				   sizeof(struct jpeg_source_mgr));
    cinfo->src->init_source = jpg_noop;
    cinfo->src->fill_input_buffer = jpg_fill_input_buffer;
    cinfo->src->skip_input_data = jpg_skip_input_data;
    cinfo->src->resync_to_restart = jpeg_resync_to_restart;
    cinfo->src->term_source = jpg_noop;
    cinfo->src->bytes_in_buffer = len;
    cinfo->src->next_input_byte = mem;
}

/*
=============
R_LoadJPG

ala Vic
=============
*/
static void R_LoadJPG(const char *name, byte **outData, int *outWidth, int *outHeight, const int Side = 0)
{
	if (outData)
		*outData = NULL;

	// Load the file
	byte *buffer;
	const int fileLen = FS_LoadFile(name, (void **)&buffer, false);
	if (!buffer || fileLen <= 0)
		return;

	// Parse the file
    struct jpeg_error_mgr jerr;
    struct jpeg_decompress_struct cinfo;
	cinfo.err = jpeg_std_error(&jerr);
	jerr.error_exit = jpeg_d_error_exit;

	jpeg_create_decompress(&cinfo);

	jpeg_mem_src(&cinfo, buffer, fileLen);
	jpeg_read_header(&cinfo, TRUE);

	jpeg_start_decompress(&cinfo);

	const int components = cinfo.output_components;
    if (components != 3 && components != 1)
	{
		Com_DevPrintf(PRNT_WARNING, "R_LoadJPG: Bad jpeg components '%s' (%d)\n", name, components);
		jpeg_destroy_decompress(&cinfo);
		FS_FreeFile(buffer);
		return;
	}

	if (cinfo.output_width <= 0 || cinfo.output_height <= 0)
	{
		Com_DevPrintf(PRNT_WARNING, "R_LoadJPG: Bad jpeg dimensions on '%s' (%d x %d)\n", name, cinfo.output_width, cinfo.output_height);
		jpeg_destroy_decompress(&cinfo);
		FS_FreeFile(buffer);
		return;
	}

	if (outWidth)
		*outWidth = cinfo.output_width;
	if (outHeight)
		*outHeight = cinfo.output_height;

	byte *img = R_AllocateTexBuffer((ETexBuffer)(TEXBUF_LOADING_0+Side), cinfo.output_width * cinfo.output_height * 4);
	byte *dummy = R_AllocateTexBuffer(TEXBUF_SCRATCH, cinfo.output_width * components);

	if (outData)
		*outData = img;

	while (cinfo.output_scanline < cinfo.output_height)
	{
		byte *scan = dummy;
		if (!jpeg_read_scanlines(&cinfo, &scan, 1))
		{
			Com_Printf(PRNT_WARNING, "Bad jpeg file %s\n", name);
			jpeg_destroy_decompress(&cinfo);
			FS_FreeFile(buffer);
			return;
		}

		if (components == 1)
		{
			for (uint32 i=0 ; i<cinfo.output_width ; i++, img+=4)
				img[0] = img[1] = img[2] = *scan++;
		}
		else
		{
			for (uint32 i=0 ; i<cinfo.output_width ; i++, img+=4, scan += 3)
				img[0] = scan[0], img[1] = scan[1], img[2] = scan[2];
		}
	}

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

	FS_FreeFile(buffer);
}


/*
================== 
R_WriteJPG
================== 
*/

static void R_WriteJPG(FILE *f, byte *buffer, int width, int height, int quality)
{
	// Initialise the jpeg compression object
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);
	jpeg_stdio_dest(&cinfo, f);

	// Setup jpeg parameters
	cinfo.image_width = width;
	cinfo.image_height = height;
	cinfo.in_color_space = JCS_RGB;
	cinfo.input_components = 3;
	cinfo.progressive_mode = TRUE;

	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, quality, TRUE);
	jpeg_start_compress(&cinfo, true); // start compression
	jpeg_write_marker(&cinfo, JPEG_COM, (byte*)APP_FULLNAME, (uint32)strlen(APP_FULLNAME));

	// Feed scanline data
	int w3 = cinfo.image_width * 3;
	int offset = w3 * cinfo.image_height - w3;
	while (cinfo.next_scanline < cinfo.image_height)
	{
		byte *s = &buffer[offset - (cinfo.next_scanline * w3)];
		jpeg_write_scanlines(&cinfo, &s, 1);
	}

	// Finish compression
	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);
}

/*
==============================================================================

	PCX

==============================================================================
*/

/*
=============
R_LoadPCX
=============
*/
static void R_LoadPCX(const char *name, byte **outData, byte **palette, int *outWidth, int *outHeight)
{
	if (outData)
		*outData = NULL;
	if (palette)
		*palette = NULL;

	// Load the file
	byte *buffer;
	int fileLen = FS_LoadFile(name, (void **)&buffer, false);
	if (!buffer || fileLen <= 0)
		return;

	// Parse the PCX file
	pcxHeader_t *pcx = (pcxHeader_t *)buffer;
	pcx->xMin = LittleShort(pcx->xMin);
	pcx->yMin = LittleShort(pcx->yMin);
	pcx->xMax = LittleShort(pcx->xMax);
	pcx->yMax = LittleShort(pcx->yMax);
	pcx->hRes = LittleShort(pcx->hRes);
	pcx->vRes = LittleShort(pcx->vRes);
	pcx->bytesPerLine = LittleShort(pcx->bytesPerLine);
	pcx->paletteType = LittleShort(pcx->paletteType);

	buffer = &pcx->data;

	// Sanity checks
	if (pcx->manufacturer != 0x0a || pcx->version != 5 || pcx->encoding != 1)
	{
		Com_DevPrintf(PRNT_WARNING, "R_LoadPCX: %s: Invalid PCX header\n", name);
		return;
	}

	if (pcx->bitsPerPixel != 8 || pcx->colorPlanes != 1)
	{
		Com_DevPrintf(PRNT_WARNING, "R_LoadPCX: %s: Only 8-bit PCX images are supported\n", name);
		return;
	}

	if (pcx->xMax >= 640 || pcx->xMax <= 0 || pcx->yMax >= 480 || pcx->yMax <= 0)
	{
		Com_DevPrintf(PRNT_WARNING, "R_LoadPCX: %s: Bad PCX file dimensions: %i x %i\n", name, pcx->xMax, pcx->yMax);
		return;
	}

	byte *pix = R_AllocateTexBuffer(TEXBUF_LOADING_0, (pcx->xMax+1)*(pcx->yMax+1));
	if (outData)
		*outData = pix;

	if (palette)
	{
		*palette = (byte*)Mem_PoolAlloc(768, ri.imageSysPool, r_imageAllocTag);
		memcpy(*palette, (byte *)pcx + fileLen - 768, 768);
	}

	if (outWidth)
		*outWidth = pcx->xMax+1;
	if (outHeight)
		*outHeight = pcx->yMax+1;

	for (int y=0 ; y<=pcx->yMax ; y++, pix+=pcx->xMax+1)
	{
		for (int x=0 ; x<=pcx->xMax ; )
		{
			int dataByte = *buffer++;

			int runLength;
			if ((dataByte & 0xC0) == 0xC0)
			{
				runLength = dataByte & 0x3F;
				dataByte = *buffer++;
			}
			else
				runLength = 1;

			while (runLength-- > 0)
				pix[x++] = dataByte;
		}
	}

	if (buffer-(byte *)pcx > fileLen)
	{
		Com_DevPrintf(PRNT_WARNING, "R_LoadPCX: %s: PCX file was malformed", name);

		if (outData)
			*outData = NULL;
		if (palette)
		{
			Mem_Free(palette);
			*palette = NULL;
		}
	}

	FS_FreeFile(pcx);
}

/*
==============================================================================

	PNG

==============================================================================
*/

struct pngBuf_t
{
	byte	*buffer;
	int		pos;
};

void __cdecl PngReadFunc (png_struct *Png, png_bytep buf, png_size_t size)
{
	pngBuf_t *PngFileBuffer = (pngBuf_t*)png_get_io_ptr(Png);
	memcpy (buf, PngFileBuffer->buffer + PngFileBuffer->pos, size);
	PngFileBuffer->pos += size;
}

/*
=============
R_LoadPNG
=============
*/
static void R_LoadPNG(const char *name, byte **outData, int *outWidth, int *outHeight, int *outSamples, const int Side = 0)
{
	if (outData)
		*outData = NULL;

	// Load the file
	pngBuf_t PngFileBuffer = { NULL, 0 };
	int fileLen = FS_LoadFile(name, (void **)&PngFileBuffer.buffer, false);
	if (!PngFileBuffer.buffer || fileLen <= 0)
		return;

	// Parse the PNG file
	if ((png_check_sig(PngFileBuffer.buffer, 8)) == 0)
	{
		Com_Printf(PRNT_WARNING, "R_LoadPNG: Not a PNG file: %s\n", name);
		FS_FreeFile(PngFileBuffer.buffer);
		return;
	}

	PngFileBuffer.pos = 0;

	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL,  NULL, NULL);
	if (!png_ptr)
	{
		Com_Printf (PRNT_WARNING, "R_LoadPNG: Bad PNG file: %s\n", name);
		FS_FreeFile (PngFileBuffer.buffer);
		return;
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
	{
		png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
		Com_Printf(PRNT_WARNING, "R_LoadPNG: Bad PNG file: %s\n", name);
		FS_FreeFile(PngFileBuffer.buffer);
		return;
	}

	png_infop end_info = png_create_info_struct(png_ptr);
	if (!end_info)
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
		Com_Printf(PRNT_WARNING, "R_LoadPNG: Bad PNG file: %s\n", name);
		FS_FreeFile(PngFileBuffer.buffer);
		return;
	}

	png_set_read_fn(png_ptr, (png_voidp)&PngFileBuffer, (png_rw_ptr)PngReadFunc);

	png_read_info(png_ptr, info_ptr);

	// Color
	switch (info_ptr->color_type)
	{
	case PNG_COLOR_TYPE_PALETTE:
		png_set_palette_to_rgb(png_ptr);
		png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
		break;

	case PNG_COLOR_TYPE_RGB:
		png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
		break;

	case PNG_COLOR_TYPE_GRAY:
		if (info_ptr->bit_depth < 8)
			png_set_expand_gray_1_2_4_to_8 (png_ptr);
		break;
	}	

	if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png_ptr);

	switch (info_ptr->color_type)
	{
	case PNG_COLOR_TYPE_GRAY:
	case PNG_COLOR_TYPE_GRAY_ALPHA:
		png_set_gray_to_rgb(png_ptr);
		break;
	}

	if (info_ptr->bit_depth == 16)
		png_set_strip_16(png_ptr);
	else if (info_ptr->bit_depth < 8)
        png_set_packing(png_ptr);

	double file_gamma;
	if (png_get_gAMA(png_ptr, info_ptr, &file_gamma))
		png_set_gamma(png_ptr, 2.0, file_gamma);

	png_read_update_info(png_ptr, info_ptr);

	const uint32 rowbytes = png_get_rowbytes(png_ptr, info_ptr);

	if (!info_ptr->channels)
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
		Com_Printf(PRNT_WARNING, "R_LoadPNG: Bad PNG file: %s\n", name);
		FS_FreeFile(PngFileBuffer.buffer);
	}

	png_bytep pic_ptr = (png_bytep)R_AllocateTexBuffer((ETexBuffer)(TEXBUF_LOADING_0+Side), info_ptr->height * rowbytes);
	if (outData)
		*outData = (byte*)pic_ptr;

	png_bytepp row_pointers = (png_bytepp)R_AllocateTexBuffer(TEXBUF_SCRATCH, sizeof(png_bytep) * info_ptr->height);

	for (png_uint_32 i=0 ; i<info_ptr->height ; i++)
		row_pointers[i] = pic_ptr + i*rowbytes;

	png_read_image(png_ptr, row_pointers);

	if (outWidth)
		*outWidth = info_ptr->width;
	if (outHeight)
		*outHeight = info_ptr->height;
	if (outSamples)
		*outSamples = info_ptr->channels;

	png_read_end(png_ptr, end_info);
	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);

	FS_FreeFile(PngFileBuffer.buffer);
}


/*
================== 
R_WritePNG
================== 
*/
static void R_WritePNG(FILE *f, byte *buffer, int width, int height)
{
	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr)
	{
		Com_Printf(PRNT_WARNING, "R_WritePNG: LibPNG Error!\n");
		return;
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
	{
		png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
		Com_Printf(PRNT_WARNING, "R_WritePNG: LibPNG Error!\n");
		return;
	}

	png_init_io(png_ptr, f);

	png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB,
				PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	png_set_compression_level(png_ptr, Z_DEFAULT_COMPRESSION);
	png_set_compression_mem_level(png_ptr, 9);

	png_write_info(png_ptr, info_ptr);

	png_bytepp row_pointers = (png_bytepp)Mem_PoolAlloc(height * sizeof(png_bytep), ri.imageSysPool, 0);
	for (int i=0 ; i<height ; i++)
		row_pointers[i] = buffer + (height - 1 - i) * 3 * width;

	png_write_image(png_ptr, row_pointers);
	png_write_end(png_ptr, info_ptr);

	png_destroy_write_struct(&png_ptr, &info_ptr);

	Mem_Free(row_pointers);
}

/*
==============================================================================

	TGA

==============================================================================
*/

/*
=============
R_LoadTGA

Loads type 1, 2, 3, 9, 10, 11 TARGA images.
Type 32 and 33 are unsupported.
=============
*/
static void R_LoadTGA(const char *name, byte **outData, int *outWidth, int *outHeight, int *outSamples, const int Side = 0)
{
	*outData = NULL;

	// Load the file
	byte *buffer;
	int fileLen = FS_LoadFile(name, (void **)&buffer, false);
	if (!buffer || fileLen <= 0)
		return;

	// Parse the header
	byte *buf_p = buffer;
	tgaHeader_t tga;
	tga.idLength = *buf_p++;
	tga.colorMapType = *buf_p++;
	tga.imageType = *buf_p++;

	tga.colorMapIndex = buf_p[0] + buf_p[1] * 256; buf_p+=2;
	tga.colorMapLength = buf_p[0] + buf_p[1] * 256; buf_p+=2;
	tga.colorMapSize = *buf_p++;
	tga.xOrigin = LittleShort(*((sint16 *)buf_p)); buf_p+=2;
	tga.yOrigin = LittleShort(*((sint16 *)buf_p)); buf_p+=2;
	tga.width = LittleShort(*((sint16 *)buf_p)); buf_p+=2;
	tga.height = LittleShort(*((sint16 *)buf_p)); buf_p+=2;
	tga.pixelSize = *buf_p++;
	tga.attributes = *buf_p++;

	// Check header values
	if (tga.width == 0 || tga.height == 0 || tga.width > 4096 || tga.height > 4096)
	{
		Com_DevPrintf(PRNT_WARNING, "R_LoadTGA: %s: Bad TGA file (%i x %i)\n", name, tga.width, tga.height);
		FS_FreeFile(buffer);
		return;
	}

	// Skip TARGA image comment
	if (tga.idLength)
		buf_p += tga.idLength;

	bool bCompressed = false;
	byte palette[256][4];
	switch (tga.imageType)
	{
	case 9:
		bCompressed = true;
	case 1:
		// Uncompressed colormapped image
		if (tga.pixelSize != 8)
		{
			Com_DevPrintf(PRNT_WARNING, "R_LoadTGA: %s: Only 8 bit images supported for type 1 and 9\n", name);
			FS_FreeFile(buffer);
			return;
		}
		if (tga.colorMapLength != 256)
		{
			Com_DevPrintf(PRNT_WARNING, "R_LoadTGA: %s: Only 8 bit colormaps are supported for type 1 and 9\n", name);
			FS_FreeFile(buffer);
			return;
		}
		if (tga.colorMapIndex)
		{
			Com_DevPrintf(PRNT_WARNING, "R_LoadTGA: %s: colorMapIndex is not supported for type 1 and 9\n", name);
			FS_FreeFile(buffer);
			return;
		}

		switch (tga.colorMapSize)
		{
		case 32:
			for (int i=0 ; i<tga.colorMapLength ; i++)
			{
				palette[i][2] = *buf_p++;
				palette[i][1] = *buf_p++;
				palette[i][0] = *buf_p++;
				palette[i][3] = *buf_p++;
			}
			break;

		case 24:
			for (int i=0 ; i<tga.colorMapLength ; i++)
			{
				palette[i][2] = *buf_p++;
				palette[i][1] = *buf_p++;
				palette[i][0] = *buf_p++;
				palette[i][3] = 255;
			}
			break;

		default:
			Com_DevPrintf(PRNT_WARNING, "R_LoadTGA: %s: Only 24 and 32 bit colormaps are supported for type 1 and 9\n", name);
			FS_FreeFile(buffer);
			return;
		}
		break;

	case 10:
		bCompressed = true;
	case 2:
		// Uncompressed or RLE compressed RGB
		if (tga.pixelSize != 32 && tga.pixelSize != 24)
		{
			Com_DevPrintf(PRNT_WARNING, "R_LoadTGA: %s: Only 32 or 24 bit images supported for type 2 and 10\n", name);
			FS_FreeFile(buffer);
			return;
		}
		break;

	case 11:
		bCompressed = true;
	case 3:
		// Uncompressed greyscale
		if (tga.pixelSize != 8)
		{
			Com_DevPrintf(PRNT_WARNING, "R_LoadTGA: %s: Only 8 bit images supported for type 3 and 11", name);
			FS_FreeFile(buffer);
			return;
		}
		break;

	default:
		Com_DevPrintf(PRNT_WARNING, "R_LoadTGA: %s: Only type 1, 2, 3, 9, 10, and 11 TGA images are supported (%i)", name, tga.imageType);
		FS_FreeFile(buffer);
		return;
	}

	int columns = tga.width;
	if (outWidth)
		*outWidth = columns;

	int rows = tga.height;
	if (outHeight)
		*outHeight = rows;

	byte *targaRGBA = R_AllocateTexBuffer((ETexBuffer)(TEXBUF_LOADING_0+Side), columns * rows * 4);
	*outData = targaRGBA;

	// If bit 5 of attributes isn't set, the image has been stored from bottom to top
	byte *pixbuf;
	int rowInc;
	if (tga.attributes & 0x20)
	{
		pixbuf = targaRGBA;
		rowInc = 0;
	}
	else
	{
		pixbuf = targaRGBA + (rows - 1) * columns * 4;
		rowInc = -columns * 4 * 2;
	}

	int components = 3;
	for (int row=0, col=0 ; row<rows ; )
	{
		int pixelCount = 0x10000;
		int readPixelCount = 0x10000;

		if (bCompressed)
		{
			pixelCount = *buf_p++;
			if (pixelCount & 0x80)
				readPixelCount = 1; // Run-length packet
			pixelCount = 1 + (pixelCount & 0x7f);
		}

		while (pixelCount-- && row < rows)
		{
			byte red, green, blue, alpha;
			if (readPixelCount-- > 0)
			{
				switch (tga.imageType)
				{
				case 1:
				case 9:
					// Colormapped image
					blue = *buf_p++;
					red = palette[blue][0];
					green = palette[blue][1];
					blue = palette[blue][2];
					alpha = palette[blue][3];
					if (alpha != 255)
						components = 4;
					break;
				case 2:
				case 10:
					// 24 or 32 bit image
					blue = *buf_p++;
					green = *buf_p++;
					red = *buf_p++;
					if (tga.pixelSize == 32)
					{
						alpha = *buf_p++;
						if (alpha != 255)
							components = 4;
					}
					else
						alpha = 255;
					break;
				case 3:
				case 11:
					// Greyscale image
					blue = green = red = *buf_p++;
					alpha = 255;
					break;
				}
			}

			*pixbuf++ = red;
			*pixbuf++ = green;
			*pixbuf++ = blue;
			*pixbuf++ = alpha;
			if (++col == columns)
			{
				// Run spans across rows
				row++;
				col = 0;
				pixbuf += rowInc;
			}
		}
	}

	FS_FreeFile(buffer);

	if (outSamples)
		*outSamples = components;
}


/*
================== 
R_WriteTGA
================== 
*/
static void R_WriteTGA(FILE *f, byte *buffer, const int width, const int height, const bool bRGB)
{
	// Allocate an output buffer
	int size = (width * height * 3) + 18;
	byte *out = (byte*)Mem_PoolAlloc(size, ri.imageSysPool, 0);

	// Fill in header
	out[2] = 2;		// Uncompressed type
	out[12] = width & 255;
	out[13] = width >> 8;
	out[14] = height & 255;
	out[15] = height >> 8;
	out[16] = 24;	// Pixel size

	// Copy to temporary buffer
	memcpy(out + 18, buffer, width * height * 3);

	// Swap rgb to bgr
	if (bRGB)
	{
		for (int i=18 ; i<size ; i+=3)
		{
			int temp = out[i];
			out[i] = out[i+2];
			out[i+2] = temp;
		}
	}

	fwrite(out, 1, size, f);

	Mem_Free(out);
}

/*
==============================================================================

	WAL

==============================================================================
*/

/*
================
R_LoadWal
================
*/
static void R_LoadWal(const char *name, byte **outData, int *outWidth, int *outHeight)
{
	// Load the file
	byte *buffer;
	int fileLen = FS_LoadFile(name, (void **)&buffer, false);
	if (!buffer || fileLen <= 0)
		return;

	// Parse the WAL file
	walTex_t *mt = (walTex_t *)buffer;

	mt->width = LittleLong(mt->width);
	mt->height = LittleLong(mt->height);
	mt->offsets[0] = LittleLong(mt->offsets[0]);

	// Sanity check
	if (mt->width <= 0 || mt->height <= 0)
	{
		Com_DevPrintf(0, "R_LoadWal: bad WAL file 's' (%i x %i)\n", name, mt->width, mt->height);
		FS_FreeFile((void *)buffer);
		return;
	}

	// Store values
	if (outWidth)
		*outWidth = mt->width;
	if (outHeight)
		*outHeight = mt->height;

	// Copy data
	*outData = R_AllocateTexBuffer(TEXBUF_LOADING_0, mt->width*mt->height);
	byte *out = *outData;
	for (uint32 i=0 ; i<mt->width*mt->height ; i++)
		*out++ = *(buffer + mt->offsets[0] + i);

	// Done
	FS_FreeFile((void *)buffer);
}

/*
==============================================================================

	PRE-UPLOAD HANDLING

==============================================================================
*/

/*
===============
R_ColorMipLevel
===============
*/
static void R_ColorMipLevel(byte *image, const int size, const int level)
{
	if (level == 0)
		return;

	switch ((level+2) % 3)
	{
	case 0:
		for (int i=0 ; i<size ; i++, image+=4)
		{
			image[0] = 255;
			image[1] *= 0.5f;
			image[2] *= 0.5f;
		}
		break;

	case 1:
		for (int i=0 ; i<size ; i++, image+=4)
		{
			image[0] *= 0.5f;
			image[1] = 255;
			image[2] *= 0.5f;

			image += 4;
		}
		break;

	case 2:
		for (int i=0 ; i<size ; i++, image+=4)
		{
			image[0] *= 0.5f;
			image[1] *= 0.5f;
			image[2] = 255;

			image += 4;
		}
		break;
	}
}


/*
===============
R_ImageInternalFormat
===============
*/
static inline GLint R_ImageInternalFormat(const char *name, const texFlags_t flags, int *samples)
{
	if (flags & IF_NOALPHA && *samples == 4)
		*samples = 3;

	GLint Result;
	if (flags & (IT_FBO|IT_DEPTHANDFBO))
	{
		Result = GL_RGBA;
	}
	else if (flags & IT_DEPTHFBO)
	{
		Result = GL_DEPTH_COMPONENT24;
	}
	else
	{
		const bool bCompressed = (ri.config.ext.bTexCompression && !(flags & IF_NOCOMPRESS));
		if (flags & IF_GREYSCALE)
		{
			Result = bCompressed ? ri.greyFormatCompressed : ri.greyFormat;
		}
		else if (*samples == 3)
		{
			Result = bCompressed ? ri.rgbFormatCompressed : ri.rgbFormat;
		}
		else
		{
			if (*samples != 4)
			{
				Com_Printf(PRNT_WARNING, "WARNING: Invalid image sample count '%d' on '%s', assuming '4'\n", samples, name);
				*samples = 4;
			}
			Result = bCompressed ? ri.rgbaFormatCompressed : ri.rgbaFormat;
		}
	}

	return Result;
}


/*
===============
R_ImageSourceFormat
===============
*/
static inline GLint R_ImageSourceFormat(const texFlags_t flags)
{
	if (flags & IT_DEPTHFBO)
		return GL_DEPTH_COMPONENT;

	return GL_RGBA;
}

/*
===============
R_ImageUploadType
===============
*/
static inline GLenum R_ImageUploadType(const texFlags_t flags)
{
	if (ri.config.ext.bFrameBufferObject)
	{
		if (flags & (IT_FBO|IT_DEPTHANDFBO|IT_DEPTHFBO))
			return GL_FLOAT;
	}

	return GL_UNSIGNED_BYTE;
}


/*
================
R_LightScaleImage

Scale up the pixel values in a texture to increase the lighting range
================
*/
static void R_LightScaleImage(uint32 *in, const int inWidth, const int inHeight, const bool useGamma, const bool useIntensity)
{
	byte *out = (byte *)in;
	int c = inWidth * inHeight;

	if (useGamma)
	{
		if (useIntensity)
		{
			for (int i=0 ; i<c ; i++, out+=4)
			{
				out[0] = r_gammaTable[r_intensityTable[out[0]]];
				out[1] = r_gammaTable[r_intensityTable[out[1]]];
				out[2] = r_gammaTable[r_intensityTable[out[2]]];
			}
		}
		else
		{
			for (int i=0 ; i<c ; i++, out+=4)
			{
				out[0] = r_gammaTable[out[0]];
				out[1] = r_gammaTable[out[1]];
				out[2] = r_gammaTable[out[2]];
			}
		}
	}
	else if (useIntensity)
	{
		for (int i=0 ; i<c ; i++, out+=4)
		{
			out[0] = r_intensityTable[out[0]];
			out[1] = r_intensityTable[out[1]];
			out[2] = r_intensityTable[out[2]];
		}
	}
}


/*
================
R_MipmapImage

Operates in place, quartering the size of the texture
================
*/
static void R_MipmapImage(byte *in, int inWidth, int inHeight)
{
	inWidth <<= 2;
	inHeight >>= 1;

	byte *out = in;
	for (int i=0 ; i<inHeight ; i++, in+=inWidth)
	{
		for (int j=0 ; j<inWidth ; j+=8, out+=4, in+=8)
		{
			out[0] = (in[0] + in[4] + in[inWidth+0] + in[inWidth+4])>>2;
			out[1] = (in[1] + in[5] + in[inWidth+1] + in[inWidth+5])>>2;
			out[2] = (in[2] + in[6] + in[inWidth+2] + in[inWidth+6])>>2;
			out[3] = (in[3] + in[7] + in[inWidth+3] + in[inWidth+7])>>2;
		}
	}
}


/*
================
R_ResampleImage
================
*/
static void R_ResampleImage(uint32 *inData, const int inWidth, const int inHeight, uint32 *outData, const int outWidth, const int outHeight)
{
	if (inWidth == outWidth && inHeight == outHeight)
	{
		memcpy(outData, inData, inWidth*inHeight*sizeof(uint32));
		return;
	}

	uint32 *resampleBuffer = (uint32*)R_AllocateTexBuffer(TEXBUF_SCRATCH, outWidth*outHeight*4);
	uint32 *p1 = resampleBuffer;
	uint32 *p2 = resampleBuffer + outWidth;

	// Resample
	const uint32 fracstep = inWidth * 0x10000 / outWidth;
	uint32 frac = fracstep >> 2;
	for (int i=0 ; i<outWidth ; i++)
	{
		p1[i] = 4 * (frac >> 16);
		frac += fracstep;
	}

	frac = 3 * (fracstep >> 2);
	for (int i=0 ; i<outWidth ; i++)
	{
		p2[i] = 4 * (frac >> 16);
		frac += fracstep;
	}

	for (int i=0 ; i<outHeight ; i++, outData+=outWidth)
	{
		uint32 *inrow = inData + inWidth * (int)((i + 0.25f) * inHeight / outHeight);
		uint32 *inrow2 = inData + inWidth * (int)((i + 0.75f) * inHeight / outHeight);
		frac = fracstep >> 1;

		for (int j=0 ; j<outWidth ; j++)
		{
			byte *pix1 = (byte *)inrow + p1[j];
			byte *pix2 = (byte *)inrow + p2[j];
			byte *pix3 = (byte *)inrow2 + p1[j];
			byte *pix4 = (byte *)inrow2 + p2[j];

			((byte *)(outData + j))[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0]) >> 2;
			((byte *)(outData + j))[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1]) >> 2;
			((byte *)(outData + j))[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2]) >> 2;
			((byte *)(outData + j))[3] = (pix1[3] + pix2[3] + pix3[3] + pix4[3]) >> 2;
		}
	}

	ri.reg.imagesResampled++;
}

/*
==============================================================================

	IMAGE UPLOADING

==============================================================================
*/

/*
===============
R_UploadCMImage
===============
*/
static void R_UploadCMImage(const char *name, byte **data, const int width, const int height,
							const texFlags_t flags, int samples,
							int *upWidth, int *upHeight, int *upInteralFormat)
{
	assert(name && name[0]);
	assert(data);
	assert(width > 0 && height > 0);

	// Find next highest power of two
	const bool bRoundDown = r_roundImagesDown->intVal ? true : false;
	GLsizei scaledWidth = Q_NearestPow<GLsizei>(width, bRoundDown);
	GLsizei scaledHeight = Q_NearestPow<GLsizei>(height, bRoundDown);

	// Mipmap
	const bool bMipMap = (flags & IF_NOMIPMAP_MASK) ? false : true;

	// Let people sample down the world textures for speed
	if (bMipMap && !(flags & IF_NOPICMIP))
	{
		if (gl_picmip->intVal > 0)
		{
			scaledWidth >>= gl_picmip->intVal;
			scaledHeight >>= gl_picmip->intVal;
		}
	}

	// Clamp dimensions
	scaledWidth = clamp(scaledWidth, 1, ri.config.ext.maxCMTexSize);
	scaledHeight = clamp(scaledHeight, 1, ri.config.ext.maxCMTexSize);

	// Get the image format
	const GLint internalFormat = R_ImageInternalFormat(name, flags, &samples);
	const GLint sourceFormat = R_ImageSourceFormat(flags);
	const GLenum uploadType = R_ImageUploadType(flags);

	// Set base upload values
	if (upWidth)
		*upWidth = scaledWidth;
	if (upHeight)
		*upHeight = scaledHeight;
	if (upInteralFormat)
		*upInteralFormat = internalFormat;

	// Texture params
	if (bMipMap)
	{
		if (ri.config.ext.bTexFilterAniso)
			glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MAX_ANISOTROPY_EXT, r_ext_maxAnisotropy->intVal);

		glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MIN_FILTER, ri.texMinFilter);
		glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MAG_FILTER, ri.texMagFilter);
	}
	else
	{
		if (ri.config.ext.bTexFilterAniso)
			glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1);

		if (flags & IF_NOMIPMAP_LINEAR)
		{
			glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}
		else
		{
			glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		}
	}

	// Cubemaps use edge clamping
	if (ri.config.ext.bTexEdgeClamp)
	{
		glTexParameterf(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameterf(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameterf(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	}
	else
	{
		glTexParameterf(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameterf(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glTexParameterf(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_R, GL_CLAMP);
	}

	// Allocate a buffer
	uint32 *scaledData = (uint32*)R_AllocateTexBuffer(TEXBUF_RESAMPLE, scaledWidth*scaledHeight*4);

	// Upload
	for (int i=0 ; i<6 ; i++)
	{
		// Resample
		R_ResampleImage((uint32 *)(data[i]), width, height, scaledData, scaledWidth, scaledHeight);

		// Scan and replace channels if desired
		if (flags & (IF_NORGB|IF_NOALPHA))
		{
			byte	*scan;

			if (flags & IF_NORGB)
			{
				scan = (byte *)scaledData;
				for (int c=scaledWidth*scaledHeight ; c>0 ; c--, scan+=4)
					scan[0] = scan[1] = scan[2] = 255;
			}
			else
			{
				scan = (byte *)scaledData + 3;
				for (int c=scaledWidth*scaledHeight ; c>0 ; c--, scan+=4)
					*scan = 255;
			}
		}

		// Apply image gamma/intensity
		R_LightScaleImage(scaledData, scaledWidth, scaledHeight, (!(flags & IF_NOGAMMA)) && !ri.config.bHWGammaInUse, bMipMap && !(flags & IF_NOINTENS));

		// Upload the base image
		glTexImage2D(r_cubeTargets[i], 0, internalFormat, scaledWidth, scaledHeight, 0, sourceFormat, uploadType, scaledData);

		// Upload mipmap levels
		if (bMipMap)
		{
			int mipLevel = 0;
			int mipWidth = scaledWidth;
			int mipHeight = scaledHeight;
			while (mipWidth > 1 || mipHeight > 1)
			{
				R_MipmapImage((byte *)scaledData, mipWidth, mipHeight);

				mipWidth >>= 1;
				if (mipWidth < 1)
					mipWidth = 1;

				mipHeight >>= 1;
				if (mipHeight < 1)
					mipHeight = 1;

				if (r_colorMipLevels->intVal)
					R_ColorMipLevel((byte *)scaledData, mipWidth * mipHeight, mipLevel);

				mipLevel++;

				glTexImage2D(r_cubeTargets[i], mipLevel, internalFormat, mipWidth, mipHeight, 0, sourceFormat, uploadType, scaledData);
			}
		}
	}
}


/*
===============
R_Upload2DImage
===============
*/
static void R_Upload2DImage(const char *name, byte *data, const int width, const int height,
							const texFlags_t flags, int samples,
							int *upWidth, int *upHeight, int *upInteralFormat)
{
	assert(name && name[0]);
	assert(width > 0 && height > 0);

	// Find next highest power of two
	const bool bRoundDown = r_roundImagesDown->intVal ? true : false;
	GLsizei scaledWidth = Q_NearestPow<GLsizei>(width, bRoundDown);
	GLsizei scaledHeight = Q_NearestPow<GLsizei>(height, bRoundDown);

	// Mipmap
	bool bMipMap = (flags & IF_NOMIPMAP_MASK) ? false : true;

	// Let people sample down the world textures for speed
	if (bMipMap && !(flags & IF_NOPICMIP))
	{
		if (gl_picmip->intVal > 0)
		{
			scaledWidth >>= gl_picmip->intVal;
			scaledHeight >>= gl_picmip->intVal;
		}
	}

	// Clamp dimensions
	scaledWidth = clamp(scaledWidth, 1, ri.config.maxTexSize);
	scaledHeight = clamp(scaledHeight, 1, ri.config.maxTexSize);

	// Get the image format
	const GLint internalFormat = R_ImageInternalFormat(name, flags, &samples);
	const GLint sourceFormat = R_ImageSourceFormat(flags);
	const GLenum uploadType = R_ImageUploadType(flags);

	// Set base upload values
	if (upWidth)
		*upWidth = scaledWidth;
	if (upHeight)
		*upHeight = scaledHeight;
	if (upInteralFormat)
		*upInteralFormat = internalFormat;

	// Texture params
	if (bMipMap)
	{
		if (ri.config.ext.bTexFilterAniso)
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, r_ext_maxAnisotropy->intVal);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, ri.texMinFilter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, ri.texMagFilter);
	}
	else
	{
		if (ri.config.ext.bTexFilterAniso)
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1);

		if (flags & IF_NOMIPMAP_LINEAR)
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}
		else
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		}
	}

	// Texture edge clamping
	if (flags & IF_CLAMP_S)
	{
		if (ri.config.ext.bTexEdgeClamp)
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		else
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	}
	else
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	}

	if (flags & IF_CLAMP_T)
	{
		if (ri.config.ext.bTexEdgeClamp)
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		else
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	}
	else
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	}

	if (!data)
	{
		assert(!bMipMap);

		// No data passed, so we're simply initializing the texture
		glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, scaledWidth, scaledHeight, 0, sourceFormat, uploadType, NULL);
	}
	else
	{
		// Allocate a buffer
		uint32 *scaledData = (uint32*)R_AllocateTexBuffer(TEXBUF_RESAMPLE, scaledWidth*scaledHeight*4);

		// Resample
		R_ResampleImage((uint32 *)data, width, height, scaledData, scaledWidth, scaledHeight);

		// Scan and replace channels if desired
		if (flags & (IF_NORGB|IF_NOALPHA))
		{
			if (flags & IF_NORGB)
			{
				byte *scan = (byte *)scaledData;
				for (int c=scaledWidth*scaledHeight ; c>0 ; c--, scan+=4)
					scan[0] = scan[1] = scan[2] = 255;
			}
			else
			{
				byte *scan = (byte *)scaledData + 3;
				for (int c=scaledWidth*scaledHeight ; c>0 ; c--, scan+=4)
					*scan = 255;
			}
		}

		// Apply image gamma/intensity
		R_LightScaleImage(scaledData, scaledWidth, scaledHeight, (!(flags & IF_NOGAMMA)) && !ri.config.bHWGammaInUse, bMipMap && !(flags & IF_NOINTENS));

		// Upload the base image
		glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, scaledWidth, scaledHeight, 0, sourceFormat, uploadType, scaledData);

		// Upload mipmap levels
		if (bMipMap)
		{
			int mipLevel = 0;
			int mipWidth = scaledWidth;
			int mipHeight = scaledHeight;

			while (mipWidth > 1 || mipHeight > 1)
			{
				R_MipmapImage((byte *)scaledData, mipWidth, mipHeight);

				mipWidth >>= 1;
				if (mipWidth < 1)
					mipWidth = 1;

				mipHeight >>= 1;
				if (mipHeight < 1)
					mipHeight = 1;

				if (r_colorMipLevels->intVal)
					R_ColorMipLevel((byte *)scaledData, mipWidth * mipHeight, mipLevel);

				mipLevel++;

				glTexImage2D(GL_TEXTURE_2D, mipLevel, internalFormat, mipWidth, mipHeight, 0, sourceFormat, uploadType, scaledData);
			}
		}
	}
}


/*
===============
R_Upload3DImage

FIXME:
- Support mipmapping, light scaling, and size scaling
- r_colorMipLevels
===============
*/
static void R_Upload3DImage(const char *name, byte **data, const int width, const int height, const int depth,
							const texFlags_t flags, int samples,
							int *upWidth, int *upHeight, int *upDepth, int *upInteralFormat)
{
	assert(name && name[0]);
	assert(data);
	assert(width > 0 && height > 0 && depth > 0);

	// Find next highest power of two
	const bool bRoundDown = r_roundImagesDown->intVal ? true : false;
	GLsizei scaledWidth = Q_NearestPow<GLsizei>(width, bRoundDown);
	GLsizei scaledHeight = Q_NearestPow<GLsizei>(height, bRoundDown);
	GLsizei scaledDepth = Q_NearestPow<GLsizei>(depth, bRoundDown);

	// Mipmap
	const bool bMipMap = (flags & IF_NOMIPMAP_MASK) ? false : true;

	// Mipmapping not supported
	if (bMipMap)
		Com_Error (ERR_DROP, "R_Upload3DImage: mipmapping not yet supported");
	if (width != scaledWidth || height != scaledHeight || depth != scaledDepth)
		Com_Error (ERR_DROP, "R_Upload3DImage: scaling not supported, use power of two dimensions and depth");
	if (scaledWidth > ri.config.ext.max3DTexSize || scaledHeight > ri.config.ext.max3DTexSize || scaledDepth > ri.config.ext.max3DTexSize)
		Com_Error (ERR_DROP, "R_Upload3DImage: dimensions too large, scaling not yet supported");

	// Get the image format
	const GLint internalFormat = R_ImageInternalFormat(name, flags, &samples);
	const GLint sourceFormat = R_ImageSourceFormat(flags);
	const GLenum uploadType = R_ImageUploadType(flags);

	// Set base upload values
	if (upWidth)
		*upWidth = scaledWidth;
	if (upHeight)
		*upHeight = scaledHeight;
	if (upDepth)
		*upDepth = scaledDepth;
	if (upInteralFormat)
		*upInteralFormat = internalFormat;

	// Texture params
	if (bMipMap)
	{
		if (ri.config.ext.bTexFilterAniso)
			glTexParameteri (GL_TEXTURE_3D, GL_TEXTURE_MAX_ANISOTROPY_EXT, r_ext_maxAnisotropy->intVal);

		glTexParameteri (GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, ri.texMinFilter);
		glTexParameteri (GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, ri.texMagFilter);
	}
	else
	{
		if (ri.config.ext.bTexFilterAniso)
			glTexParameteri (GL_TEXTURE_3D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1);

		if (flags & IF_NOMIPMAP_LINEAR)
		{
			glTexParameteri (GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri (GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}
		else
		{
			glTexParameteri (GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri (GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		}
	}

	// Texture edge clamping
	if (flags & IF_CLAMP_S)
	{
		if (ri.config.ext.bTexEdgeClamp)
			glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		else
			glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	}
	else
	{
		glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	}

	if (flags & IF_CLAMP_T)
	{
		if (ri.config.ext.bTexEdgeClamp)
			glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		else
			glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	}
	else
	{
		glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	}

	if (flags & IF_CLAMP_R)
	{
		if (ri.config.ext.bTexEdgeClamp)
			glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		else
			glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP);
	}
	else
	{
		glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
	}

	// Scan and replace channels if desired
	if (flags & (IF_NORGB|IF_NOALPHA))
	{
		byte	*scan;

		if (flags & IF_NORGB)
		{
			for (int i=0 ; i<depth ; i++)
			{
				scan = (byte *)data[i];
				for (int c=scaledWidth*scaledHeight ; c>0 ; c--, scan+=4)
					scan[0] = scan[1] = scan[2] = 255;
			}
		}
		else
		{
			for (int i=0 ; i<depth ; i++)
			{
				scan = (byte *)data[i] + 3;
				for (int c=scaledWidth*scaledHeight ; c>0 ; c--, scan+=4)
					*scan = 255;
			}
		}
	}

	// Upload
	qglTexImage3D(GL_TEXTURE_3D, 0, internalFormat, scaledWidth, scaledHeight, scaledDepth, 0, sourceFormat, uploadType, data[0]);
}

/*
==============================================================================

	WAL/PCX PALETTE

==============================================================================
*/

/*
===============
R_PalToRGBA

Converts a paletted image to standard RGB[A] before passing off for upload.
Also finds out if it's RGB or RGBA.
===============
*/
struct floodFill_t
{
	int		x, y;
};

// must be a power of 2
#define FLOODFILL_FIFO_SIZE 0x1000
#define FLOODFILL_FIFO_MASK (FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP(off, dx, dy) \
{ \
	if (pos[off] == fillColor) \
	{ \
		pos[off] = 255; \
		fifo[inpt].x = x + (dx), fifo[inpt].y = y + (dy); \
		inpt = (inpt + 1) & FLOODFILL_FIFO_MASK; \
	} \
	else if (pos[off] != 255) \
	{ \
		fdc = pos[off]; \
	} \
}

static void R_FloodFillSkin(byte *skin, const int skinWidth, int const skinHeight)
{
	// Assume this is the pixel to fill
	byte fillColor = *skin;
	int filledColor = -1;

	if (filledColor == -1)
	{
		filledColor = 0;
		// Attempt to find opaque black
		for (int i=0 ; i<256 ; ++i)
		{
			if (r_paletteTable[i] == (255 << 0))
			{
				// Alpha 1.0
				filledColor = i;
				break;
			}
		}
	}

	// Can't fill to filled color or to transparent color (used as visited marker)
	if (fillColor == filledColor || fillColor == 255)
		return;

	int inpt = 0;
	floodFill_t fifo[FLOODFILL_FIFO_SIZE];
	fifo[inpt].x = 0, fifo[inpt].y = 0;
	inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;

	int outpt = 0;
	while (outpt != inpt)
	{
		int x = fifo[outpt].x, y = fifo[outpt].y;
		int fdc = filledColor;
		byte *pos = &skin[x + skinWidth * y];

		outpt = (outpt + 1) & FLOODFILL_FIFO_MASK;

		if (x > 0)
			FLOODFILL_STEP(-1, -1, 0);

		if (x < skinWidth-1)
			FLOODFILL_STEP(1, 1, 0);

		if (y > 0)
			FLOODFILL_STEP(-skinWidth, 0, -1);

		if (y < skinHeight-1)
			FLOODFILL_STEP(skinWidth, 0, 1);
		skin[x + skinWidth * y] = fdc;
	}
}
static void R_PalToRGBA(const char *name, byte *data, refImage_t *image, const bool bIsPCX)
{
	int size = image->width * image->height;
	uint32 *trans = (uint32*)R_AllocateTexBuffer(TEXBUF_PAL, size*4);

	// Map the palette to standard RGB
	int samples = 3;
	for (int i=0 ; i<size ; i++)
	{
		byte pxl = data[i];
		trans[i] = r_paletteTable[pxl];

		if (pxl == 0xff)
		{
			samples = 4;

			// Transparent, so scan around for another color to avoid alpha fringes
			if (i > image->width && data[i-image->width] != 255)
				pxl = data[i-image->width];
			else if (i < size-image->width && data[i+image->width] != 255)
				pxl = data[i+image->width];
			else if (i > 0 && data[i-1] != 255)
				pxl = data[i-1];
			else if (i < size-1 && data[i+1] != 255)
				pxl = data[i+1];
			else
				pxl = 0;

			// Copy rgb components
			((byte *)&trans[i])[0] = ((byte *)&r_paletteTable[pxl])[0];
			((byte *)&trans[i])[1] = ((byte *)&r_paletteTable[pxl])[1];
			((byte *)&trans[i])[2] = ((byte *)&r_paletteTable[pxl])[2];
		}
	}

	// Upload
	if (bIsPCX)
		R_FloodFillSkin((byte*)trans, image->width, image->height);

	R_Upload2DImage(name, (byte*)trans, image->width, image->height, image->flags, samples, &image->upWidth, &image->upHeight, &image->upFormat);
}

/*
==============================================================================

	IMAGE LOADING

==============================================================================
*/

/*
================
R_BareImageName
================
*/
static const char *R_BareImageName(const char *name)
{
	static char	bareName[2][MAX_QPATH];
	static int	bareIndex;

	bareIndex ^= 1;

	// Fix/strip barename
	Com_NormalizePath(bareName[bareIndex], sizeof(bareName[bareIndex]), name);
	Com_StripExtension(bareName[bareIndex], sizeof(bareName[bareIndex]), bareName[bareIndex]);
	Q_strlwr(bareName[bareIndex]);

	return bareName[bareIndex];
}


/*
================
R_FindImage
================
*/
static refImage_t *R_FindImage(const char *bareName, const texFlags_t flags)
{
	// Calculate hash
	ri.reg.imagesSeaked++;

	refImage_t *image = (imageList.find(bareName)) == imageList.end() ? null : imageList[bareName];

	if (image == null)
		return null;

	// Look for it
	if (flags == 0)
	{
		if (!image->touchFrame)
			return null;	// Free r_imageList slot

		// Check name
		if (!strcmp(bareName, image->bareName))
			return image;
	}
	else
	{
		if (!image->touchFrame)
			return null;	// Free r_imageList slot
		if (image->flags != flags)
			return null;

		// Check name
		if (!strcmp(bareName, image->bareName))
			return image;
	}

	return NULL;
}

int _lastTexNum = 0;

/*
================
R_CreateImage

This is also used as an entry point for the generated ri.media.noTexture
================
*/
refImage_t *R_CreateImage(const char *name, const char *bareName,
						byte **pic,
						const int width, const int height, const int depth,
						const texFlags_t flags,
						const int samples,
						const bool bUpload8, const bool bIsPCX)
{
	refImage_t *image = new refImage_t;

	image->texNum = _lastTexNum + 1;
	_lastTexNum++;

	// See if this texture is allowed
	if (flags & IT_3D && depth > 1 && !ri.config.ext.bTex3D)
		Com_Error(ERR_DROP, "R_CreateImage: '%s' is 3D and 3D textures are disabled", name);
	if (flags & (IT_FBO|IT_DEPTHANDFBO|IT_DEPTHFBO) && !ri.config.ext.bFrameBufferObject) // FIXME
		Com_Error(ERR_DROP, "R_CreateImage: '%s' is a FBO and FBOs are disabled", name);

	// Set the name
	Q_strncpyz(image->name, name, sizeof(image->name));
	if (!bareName)
		bareName = R_BareImageName(name);
	Q_strncpyz(image->bareName, bareName, sizeof(image->bareName));

	// Set width, height, and depth
	image->width = image->tcWidth = width;
	image->height = image->tcHeight = height;
	image->depth = depth;

	// Texture scaling, hacky special case!
	if (!bUpload8 && !(flags & IF_NOMIPMAP_MASK))
	{
		char newName[MAX_QPATH];
		Q_snprintfz(newName, sizeof(newName), "%s.wal", image->bareName);

		walTex_t *mt;
		int fileLen = FS_LoadFile(newName, (void **)&mt, false);
		if (mt && fileLen > 0)
		{
			image->tcWidth = LittleLong(mt->width);
			image->tcHeight = LittleLong(mt->height);

			FS_FreeFile(mt);
		}
	}

	// Set base values
	image->flags = flags;
	image->touchFrame = ri.reg.registerFrame;

	if (image->flags & (IT_FBO|IT_DEPTHANDFBO|IT_DEPTHFBO))
	{
		assert(!(image->flags & (IT_CUBEMAP|IT_3D)));
		assert(!bUpload8 && !bIsPCX);

		image->flags |= IF_NOCOMPRESS|IF_NOGAMMA|IF_NOINTENS|IF_NOMIPMAP_LINEAR;
		image->target = GL_TEXTURE_2D;
	}
	else if (image->flags & IT_CUBEMAP)
	{
		assert(!(image->flags & IT_3D));

		image->target = GL_TEXTURE_CUBE_MAP_ARB;
	}
	else if (image->flags & IT_3D && image->depth > 1)
	{
		image->target = GL_TEXTURE_3D;
	}
	else
	{
		image->target = GL_TEXTURE_2D;
	}

	// Upload
	RB_BindTexture(image);
	switch (image->target)
	{
	case GL_TEXTURE_2D:
		if (bUpload8)
			R_PalToRGBA(name, *pic, image, bIsPCX);
		else
			R_Upload2DImage(name, pic ? *pic : NULL, image->width, image->height, image->flags, samples, &image->upWidth, &image->upHeight, &image->upFormat);
		break;

	case GL_TEXTURE_3D:
		R_Upload3DImage(name, pic, image->width, image->height, image->depth, image->flags, samples, &image->upWidth, &image->upHeight, &image->upDepth, &image->upFormat);
		break;

	case GL_TEXTURE_CUBE_MAP_ARB:
		R_UploadCMImage(name, pic, image->width, image->height, image->flags, samples, &image->upWidth, &image->upHeight, &image->upFormat);
		break;
	}

	// Set it up if it's an FBO
	if (ri.config.ext.bFrameBufferObject && image->flags & (IT_FBO|IT_DEPTHANDFBO|IT_DEPTHFBO))
	{
		// Create the object
		qglGenFramebuffersEXT(1, &image->fboNum);
		qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, image->fboNum);

		// Setup
		if (image->flags & IT_FBO)
		{
			assert(!(image->flags & (IT_DEPTHFBO|IT_DEPTHANDFBO)));

			qglFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, image->texNum, 0);
		}
		else if (image->flags & IT_DEPTHFBO)
		{
			assert(!(image->flags & (IT_FBO|IT_DEPTHANDFBO)));

			qglFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, image->texNum, 0);
			glDrawBuffer(GL_NONE);
			glReadBuffer(GL_NONE);
		}
		else if (image->flags & IT_DEPTHANDFBO)
		{
			assert(!(image->flags & (IT_FBO|IT_DEPTHFBO)));

			// Create a render buffer for depth
			qglGenRenderbuffersEXT(1, &image->depthFBONum);
			qglBindRenderbufferEXT(GL_RENDERBUFFER_EXT, image->depthFBONum);
			qglRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24, image->upWidth, image->upHeight);

			// Attach it to our frame buffer
			qglFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, image->texNum, 0);
			qglFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, image->depthFBONum);
		}

		// Check for successful completion
		uint32 errNum = qglCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
		if (errNum != GL_FRAMEBUFFER_COMPLETE_EXT)
		{
			Com_Printf(PRNT_ERROR, "R_CreateImage: failed create FBO '%s', code: 0x%x\n", name, errNum);
			RB_CheckForError("R_CreateImage");

			// Unbind
			qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
			return NULL;
		}

		// Unbind
		qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	}

	// Link it in
	imageList.insert(TImageList::value_type(bareName, image));

	return image;
}


/*
===============
R_RegisterCubeMap

Finds or loads the given cubemap image
Static because R_RegisterImage uses it if it's passed the IT_CUBEMAP flag
===============
*/
static refImage_t *R_RegisterCubeMap(const char *name, texFlags_t flags)
{
	// Sanity check
	if (flags & (IT_FBO|IT_DEPTHANDFBO|IT_DEPTHFBO))
		Com_Error(ERR_FATAL, "R_RegisterCubeMap: called with an FBO, are you insane?");

	// Make sure we have this
	flags |= (IT_CUBEMAP|IF_CLAMP_ALL);

	// Generate the bare name
	const char *bareName = R_BareImageName(name);

	// See if it's already loaded
	refImage_t *image = R_FindImage(bareName, flags);
	if (image)
	{
		R_TouchImage(image);
		return image;
	}

	// Not found -- load the pic from disk
	int firstSize, firstSamples;
	int side;
	byte *picSides[6];
	char storeName[MAX_QPATH];
	for (side=0 ; side<6 ; side++)
	{
		picSides[side] = NULL;

		char loadName[MAX_QPATH];
		Q_snprintfz(loadName, sizeof(loadName), "%s_%s.png", bareName, r_cubeMapSuffix[side]);
		const size_t len = strlen(loadName);

		int width, height, samples;

		// PNG
		R_LoadPNG(loadName, &picSides[side], &width, &height, &samples, side);
		if (!picSides[side])
		{
			// TGA
			loadName[len-3] = 't'; loadName[len-2] = 'g'; loadName[len-1] = 'a';
			R_LoadTGA(loadName, &picSides[side], &width, &height, &samples, side);
			if (!picSides[side])
			{
				// JPG
				samples = 3;
				loadName[len-3] = 'j'; loadName[len-2] = 'p'; loadName[len-1] = 'g';
				R_LoadJPG(loadName, &picSides[side], &width, &height, side);

				// Not found
				if (!picSides[side])
				{
					Com_Printf(PRNT_WARNING, "R_RegisterCubeMap: Unable to find all of the sides, aborting!\n");
					break;
				}
			}
		}

		// Must be square
		if (width != height)
		{
			Com_Printf(PRNT_WARNING, "R_RegisterCubeMap: %s is not square, aborting!\n", loadName);
			break;
		}

		// Must match previous
		if (!side)
		{
			Q_snprintfz(storeName, sizeof(storeName), "%s.%c%c%c", bareName, loadName[len-3], loadName[len-2], loadName[len-1]);
			firstSize = width;
			firstSamples = samples;
		}
		else
		{
			if (firstSize != width)
			{
				Com_Printf(PRNT_WARNING, "R_RegisterCubeMap: Size mismatch with previous on %s, aborting!\n", loadName);
				break;
			}

			if (firstSamples != samples)
			{
				Com_Printf(PRNT_WARNING, "R_RegisterCubeMap: Sample mismatch with previous on %s, aborting!\n", loadName);
				break;
			}
		}
	}

	// Load the cubemap
	if (side == 6)
	{
		image = R_CreateImage(storeName, bareName, picSides, firstSize, firstSize, 1, flags, firstSamples);
	}

	return image;
}


/*
===============
R_RegisterImage

Finds or loads the given image
===============
*/
refImage_t *R_RegisterImage(const char *name, texFlags_t flags)
{
	// Check the name
	if (!name)
		return NULL;

	// Check the length
	const size_t nameLen = strlen(name);
	if (nameLen < 2)
	{
		Com_Printf(PRNT_ERROR, "R_RegisterImage: Image name too short! %s\n", name);
		return NULL;
	}
	if (nameLen+1 >= MAX_QPATH)
	{
		Com_Printf(PRNT_ERROR, "R_RegisterImage: Image name too long! %s\n, name");
		return NULL;
	}

	// Cubemap stuff
	if (flags & IT_CUBEMAP)
	{
		if (ri.config.ext.bTexCubeMap)
			return R_RegisterCubeMap(name, flags);
		flags &= ~IT_CUBEMAP;
	}

	// Generate the bare name
	const char *bareName = R_BareImageName(name);

	// See if it's already loaded
	refImage_t *image = R_FindImage(bareName, flags);
	if (image)
	{
		R_TouchImage(image);
		return image;
	}

	// Not found -- load the pic from disk
	char loadName[MAX_QPATH];
	Q_snprintfz(loadName, sizeof(loadName), "%s.png", bareName);
	const size_t len = strlen(loadName);

	byte *pic;
	int width, height, samples;

	// PNG
	R_LoadPNG(loadName, &pic, &width, &height, &samples);
	if (!pic)
	{
		// TGA
		loadName[len-3] = 't'; loadName[len-2] = 'g'; loadName[len-1] = 'a';
		R_LoadTGA(loadName, &pic, &width, &height, &samples);
		if (!pic)
		{
			// JPG
			samples = 3;
			loadName[len-3] = 'j'; loadName[len-2] = 'p'; loadName[len-1] = 'g';
			R_LoadJPG(loadName, &pic, &width, &height);
			if (!pic)
			{
				// WAL
				if (!(strcmp (name+len-4, ".wal")))
				{
					loadName[len-3] = 'w'; loadName[len-2] = 'a'; loadName[len-1] = 'l';
					R_LoadWal(loadName, &pic, &width, &height);
					if (pic)
					{
						image = R_CreateImage(loadName, bareName, &pic, width, height, 1, flags, samples, true);
						return image;
					}
					return NULL;
				}

				// PCX
				loadName[len-3] = 'p'; loadName[len-2] = 'c'; loadName[len-1] = 'x';
				R_LoadPCX(loadName, &pic, NULL, &width, &height);
				if (pic)
				{
					image = R_CreateImage(loadName, bareName, &pic, width, height, 1, flags, samples, true, true);
					return image;
				}
				return NULL;
			}
		}
	}

	// Found it, upload it
	return R_CreateImage(loadName, bareName, &pic, width, height, 1, flags, samples);
}


/*
================
R_FreeImage
================
*/
TImageList::iterator R_FreeImage(TImageList::iterator &it, refImage_t *image)
{
	if (!image)
		throw Exception();

	ri.reg.imagesReleased++;

	// Free it
	if (image->texNum)
		glDeleteTextures(1, &image->texNum);
	else
		Com_DevPrintf(PRNT_WARNING, "R_FreeImage: attempted to release invalid texNum on image '%s'!\n", image->name);

	// Release any FBO data
	if (ri.config.ext.bFrameBufferObject)
	{
		if (image->flags & (IT_FBO|IT_DEPTHFBO))
		{
			qglDeleteFramebuffersEXT(1, &image->fboNum);
		}
		else if (image->flags & IT_DEPTHANDFBO)
		{
			qglDeleteFramebuffersEXT(1, &image->fboNum);
			qglDeleteFramebuffersEXT(1, &image->depthFBONum);
		}
	}

	image->touchFrame = 0;
	delete image;
	return imageList.erase(it);
}


/*
================
R_EndImageRegistration

Any image that was not touched on this registration sequence will be released
================
*/
void R_EndImageRegistration()
{
	// Free un-touched images
	for (auto it = imageList.begin(); it != imageList.end(); )
	{
		refImage_t *image = (*it).second;
		if (!image->touchFrame || // Free r_imageList slot
			image->touchFrame == ri.reg.registerFrame) // Used this sequence
		{
			++it;
			continue;			
		}

		if (image->flags & IF_NOFLUSH)
		{
			R_TouchImage (image);
			++it;
			continue;	// Don't free
		}
	
		it = R_FreeImage(it, image);
	}

	Com_DevPrintf(PRNT_CONSOLE, "Completing image system registration:\n-Released: %i\n-Resampled: %i\n-Touched: %i\n-Seaked: %i\n", ri.reg.imagesReleased, ri.reg.imagesResampled, ri.reg.imagesSeaked, ri.reg.imagesTouched);
}


/*
===============
R_UpdateTexture
===============
*/
bool R_UpdateTexture(const char *Name, byte *Data, int xOffset, int yOffset, int Width, int Height)
{
	// Check name
	if (!Name || !Name[0])
		Com_Error(ERR_DROP, "R_UpdateTexture: NULL texture name");

	// Generate the bare name
	const char *bareName = R_BareImageName(Name);

	// Find the image
	refImage_t *image = R_FindImage(bareName, 0);
	if (!image)
	{
		Com_DevPrintf(PRNT_WARNING, "R_UpdateTexture: %s: could not find!\n", Name);
		return false;
	}

	// Can't be compressed
	if (!(image->flags & IF_NOCOMPRESS))
	{
		Com_DevPrintf(PRNT_WARNING, "R_UpdateTexture: %s: can not update potentially compressed images!\n", Name);
		return false;
	}

	// Can't be picmipped
	if (!(image->flags & IF_NOPICMIP))
	{
		Com_DevPrintf(PRNT_WARNING, "R_UpdateTexture: %s: can not update potentionally picmipped images!\n", Name);
		return false;
	}

	// Can't be mipmapped
	if (!(image->flags & IF_NOMIPMAP_MASK))
	{
		Com_DevPrintf(PRNT_WARNING, "R_UpdateTexture: %s: can not update mipmapped images!\n", Name);
		return false;
	}

	// Must be 2D
	if (image->target != GL_TEXTURE_2D)
	{
		Com_DevPrintf(PRNT_WARNING, "R_UpdateTexture: %s: must be a 2D image!\n", Name);
		return false;
	}

	// Check the size
	if (Width > ri.config.maxTexSize || Height > ri.config.maxTexSize)
	{
		Com_DevPrintf(PRNT_WARNING, "R_UpdateTexture: %s: size exceeds card maximum!\n", Name);
		return false;
	}

	// Update
	RB_BindTexture(image);
	glTexSubImage2D(GL_TEXTURE_2D, 0, xOffset, yOffset, Width, Height, R_ImageSourceFormat(image->flags), R_ImageUploadType(image->flags), Data);
	return true;
}


/*
===============
R_InitScreenTexture
===============
*/
void R_InitScreenTexture(refImage_t **Image, const char *Name, const int ID, const int ScreenWidth, const int ScreenHeight, const texFlags_t TexFlags, const int Samples)
{
	assert(TexFlags & IF_NOMIPMAP_MASK);
	assert(TexFlags & IF_NOCOMPRESS);

	const int limit = min(ri.config.maxTexSize, min(ScreenWidth, ScreenHeight));
	const int size = Q_NearestPow<int>(limit, true);

	// If it doesn't exist, create
	if (!(*Image))
	{
		*Image = R_Create2DImage(Q_VarArgs("%s%i", Name, ID), NULL, size, size, TexFlags, Samples);
	}
	// Update
	else if ((*Image)->width != size || (*Image)->height != size)
	{
		RB_BindTexture(*Image);
		(*Image)->width = size;
		(*Image)->height = size;
		R_Upload2DImage(Name, NULL, (*Image)->width, (*Image)->height, TexFlags, Samples, &(*Image)->upWidth, &(*Image)->upHeight, &(*Image)->upFormat);
	}
}


/*
===============
R_InitShadowMapTexture
===============
*/
void R_InitShadowMapTexture(refImage_t **Image, const int ID, const int ScreenWidth, const int ScreenHeight)
{
	R_InitScreenTexture(
		Image, "*sm", ID, ScreenWidth, ScreenHeight,
		IT_DEPTHFBO|IF_NOPICMIP|IF_NOMIPMAP_LINEAR|IF_NOINTENS|IF_NOGAMMA|IF_NOCOMPRESS|IF_CLAMP_ALL, 1
	);
}


/*
===============
R_GetImageSize
===============
*/
void R_GetImageSize(refMaterial_t *mat, int *width, int *height)
{
	matPass_t	*pass;
	refImage_t	*image;
	int			i;

	if (!mat || !mat->numPasses)
	{
		if (width)
			*width = 0;
		if (height)
			*height = 0;
		return;
	}

	image = NULL;
	for (i=0, pass=mat->passes ; i<mat->numPasses ; pass++, i++)
	{
		if (i != mat->sizeBase)
			continue;

		image = pass->animImages[0];
		break;
	}

	if (!image)
		return;

	if (width)
		*width = image->width;
	if (height)
		*height = image->height;
}

/*
==============================================================================

	CONSOLE FUNCTIONS

==============================================================================
*/

/*
===============
R_ImageList_f
===============
*/
static void R_ImageList_f ()
{
	uint32		tempWidth, tempHeight;
	uint32		i = 0, totalImages = 0, totalMips = 0;
	uint32		mipTexels = 0, texels = 0;

	Com_Printf (0, "------------------------------------------------------\n");
	Com_Printf (0, "Tex# Ta Format       LGIFC Width Height Name\n");
	Com_Printf (0, "---- -- ------------ ----- ----- ------ --------------\n");

	for (auto it = imageList.begin(); it != imageList.end(); i++, ++it)
	{
		auto image = (*it).second;
		if (!image->touchFrame)
			continue;	// Free r_imageList slot

		// Texnum
		Com_Printf(0, "%4d ", image->texNum);

		// Target
		switch(image->target)
		{
		case GL_TEXTURE_CUBE_MAP_ARB:			Com_Printf(0, "CM ");			break;
		case GL_TEXTURE_1D:						Com_Printf(0, "1D ");			break;
		case GL_TEXTURE_2D:						Com_Printf(0, "2D ");			break;
		case GL_TEXTURE_3D:						Com_Printf(0, "3D ");			break;
		}

		// Format
		switch(image->upFormat)
		{
		case GL_RGBA8:							Com_Printf(0, "RGBA8     ");	break;
		case GL_RGBA4:							Com_Printf(0, "RGBA4     ");	break;
		case GL_RGBA:							Com_Printf(0, "RGBA      ");	break;
		case GL_RGB8:							Com_Printf(0, "RGB8      ");	break;
		case GL_RGB5:							Com_Printf(0, "RGB5      ");	break;
		case GL_RGB:							Com_Printf(0, "RGB       ");	break;

		case GL_DSDT8_NV:						Com_Printf(0, "DSDT8     ");	break;

		case GL_COMPRESSED_RGB_ARB:				Com_Printf(0, "RGB   ARB  ");	break;
		case GL_COMPRESSED_RGBA_ARB:			Com_Printf(0, "RGBA  ARB  ");	break;

		case GL_RGB_S3TC:						Com_Printf(0, "RGB   S3   ");	break;
		case GL_RGB4_S3TC:						Com_Printf(0, "RGB4  S3   ");	break;
		case GL_RGBA_S3TC:						Com_Printf(0, "RGBA  S3   ");	break;
		case GL_RGBA4_S3TC:						Com_Printf(0, "RGBA4 S3   ");	break;

		case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:	Com_Printf(0, "RGB   DXT1 ");	break;
		case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:	Com_Printf(0, "RGBA  DXT1 ");	break;
		case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:	Com_Printf(0, "RGBA  DXT3 ");	break;
		case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:	Com_Printf(0, "RGBA  DXT5 ");	break;

		default:								Com_Printf(0, "??????     ");	break;
		}

		// Flags
		Com_Printf(0, "%s", (image->flags & IT_LIGHTMAP)	? "L" : "-");
		Com_Printf(0, "%s", (image->flags & IF_NOGAMMA)		? "-" : "G");
		Com_Printf(0, "%s", (image->flags & IF_NOINTENS)	? "-" : "I");
		Com_Printf(0, "%s", (image->flags & IF_NOFLUSH)		? "F" : "-");
		Com_Printf(0, "%s", (image->flags & IF_CLAMP_S)		? "Cs" : "--");
		Com_Printf(0, "%s", (image->flags & IF_CLAMP_T)		? "Ct" : "--");
		Com_Printf(0, "%s", (image->flags & IF_CLAMP_R)		? "Cr" : "--");

		// Width/height name
		Com_Printf(0, " %5i  %5i %s\n", image->upWidth, image->upHeight, image->name);

		// Increment counters
		totalImages++;
		texels += image->upWidth * image->upHeight;
		if (!(image->flags & IF_NOMIPMAP_MASK))
		{
			tempWidth=image->upWidth, tempHeight=image->upHeight;
			while (tempWidth > 1 || tempHeight > 1)
			{
				tempWidth >>= 1;
				if (tempWidth < 1)
					tempWidth = 1;

				tempHeight >>= 1;
				if (tempHeight < 1)
					tempHeight = 1;

				mipTexels += tempWidth * tempHeight;
				totalMips++;
			}
		}
	}

	Com_Printf(0, "------------------------------------------------------\n");
	Com_Printf(0, "Total images: %d (with mips: %d)\n", totalImages, totalImages+totalMips);
	Com_Printf(0, "Texel count: %d (w/o mips: %d)\n", texels+mipTexels, texels);
	Com_Printf(0, "------------------------------------------------------\n");
}


/*
================== 
R_ScreenShot_f
================== 
*/
enum {
	SSHOTTYPE_JPG,
	SSHOTTYPE_PNG,
	SSHOTTYPE_TGA,
};
static void R_ScreenShot_f ()
{
	char	checkName[MAX_OSPATH];
	int		type, shotNum, quality;
	char	*ext;
	byte	*buffer;
	FILE	*f;

	// Find out what format to save in
	if (Cmd_Argc () > 1)
		ext = Cmd_Argv (1);
	else
		ext = gl_screenshot->string;

	if (!Q_stricmp (ext, "png"))
		type = SSHOTTYPE_PNG;
	else if (!Q_stricmp (ext, "jpg"))
		type = SSHOTTYPE_JPG;
	else
		type = SSHOTTYPE_TGA;

	// Set necessary values
	switch (type) {
	case SSHOTTYPE_TGA:
		Com_Printf (0, "Taking TGA screenshot...\n");
		quality = 100;
		ext = "tga";
		break;

	case SSHOTTYPE_PNG:
		Com_Printf (0, "Taking PNG screenshot...\n");
		quality = 100;
		ext = "png";
		break;

	case SSHOTTYPE_JPG:
		if (Cmd_Argc () == 3)
			quality = atoi (Cmd_Argv (2));
		else
			quality = gl_jpgquality->intVal;
		if (quality > 100 || quality <= 0)
			quality = 100;

		Com_Printf (0, "Taking JPG screenshot (at %i%% quality)...\n", quality);
		ext = "jpg";
		break;
	}

	// Create the scrnshots directory if it doesn't exist
	Q_snprintfz (checkName, sizeof(checkName), "%s/scrnshot", FS_Gamedir ());
	Sys_Mkdir (checkName);

	// Find a file name to save it to
	for (shotNum=0 ; shotNum<1000 ; shotNum++) {
		Q_snprintfz (checkName, sizeof(checkName), "%s/scrnshot/egl%.3d.%s", FS_Gamedir (), shotNum, ext);
		f = fopen (checkName, "rb");
		if (!f)
			break;
		fclose (f);
	}

	// Open it
	f = fopen (checkName, "wb");
	if (shotNum == 1000 || !f) {
		Com_Printf (PRNT_WARNING, "R_ScreenShot_f: Couldn't create a file\n"); 
		fclose (f);
		return;
	}

	// Allocate room for a copy of the framebuffer
	buffer = (byte*)Mem_PoolAlloc (ri.config.vidWidth * ri.config.vidHeight * 3, ri.imageSysPool, 0);

	// Read the framebuffer into our storage
	if (ri.config.ext.bBGRA && type == SSHOTTYPE_TGA) {
		glReadPixels (0, 0, ri.config.vidWidth, ri.config.vidHeight, GL_BGR_EXT, GL_UNSIGNED_BYTE, buffer);

		// Apply hardware gamma
		if (ri.config.bHWGammaInUse)
		{
			int		i, size;

			size = ri.config.vidWidth * ri.config.vidHeight * 3;
			for (i=0 ; i<size ; i+=3) {
				buffer[i+2] = ri.gammaRamp[buffer[i+2]] >> 8;
				buffer[i+1] = ri.gammaRamp[buffer[i+1] + 256] >> 8;
				buffer[i+0] = ri.gammaRamp[buffer[i+0] + 512] >> 8;
			}
		}
	}
	else
	{
		glReadPixels (0, 0, ri.config.vidWidth, ri.config.vidHeight, GL_RGB, GL_UNSIGNED_BYTE, buffer);

		// Apply hardware gamma
		if (ri.config.bHWGammaInUse)
		{
			int		i, size;

			size = ri.config.vidWidth * ri.config.vidHeight * 3;

			for (i=0 ; i<size ; i+=3) {
				buffer[i+0] = ri.gammaRamp[buffer[i+0]] >> 8;
				buffer[i+1] = ri.gammaRamp[buffer[i+1] + 256] >> 8;
				buffer[i+2] = ri.gammaRamp[buffer[i+2] + 512] >> 8;
			}
		}
	}

	// Write
	switch (type) {
	case SSHOTTYPE_TGA:
		R_WriteTGA (f, buffer, ri.config.vidWidth, ri.config.vidHeight, !ri.config.ext.bBGRA);
		break;

	case SSHOTTYPE_PNG:
		R_WritePNG (f, buffer, ri.config.vidWidth, ri.config.vidHeight);
		break;

	case SSHOTTYPE_JPG:
		R_WriteJPG (f, buffer, ri.config.vidWidth, ri.config.vidHeight, quality);
		break;
	}

	// Finish
	fclose (f);
	Mem_Free (buffer);

	Com_Printf (0, "Wrote egl%.3d.%s\n", shotNum, ext);
}

/*
==============================================================================

	INIT / SHUTDOWN

==============================================================================
*/

static conCmd_t	*cmd_imageList;
static conCmd_t	*cmd_screenShot;

/*
==================
R_ResetGammaRamp
==================
*/
void R_ResetGammaRamp()
{
	if (!ri.config.bHWGammaInUse)
		return;

	for (int i=0 ; i<256 ; i++)
	{
		byte gam = (byte)(clamp (255 * powf(( (i & 255) + 0.5f)*0.0039138943248532289628180039138943, 1.0f)+ 0.5f, 0, 255));
		ri.gammaRamp[i] = ri.gammaRamp[i + 256] = ri.gammaRamp[i + 512] = gam * 255;
	}

	GLimp_SetGammaRamp (ri.gammaRamp);
}

/*
==================
R_UpdateGammaRamp
==================
*/
void R_UpdateGammaRamp()
{
	vid_gamma->modified = false;
	if (!ri.config.bHWGammaInUse)
		return;

	for (int i=0 ; i<256 ; i++)
	{
		byte gam = (byte)(clamp (255 * powf(( (i & 255) + 0.5f)*0.0039138943248532289628180039138943, vid_gamma->floatVal) + 0.5f, 0, 255));
		ri.gammaRamp[i] = ri.gammaRamp[i + 256] = ri.gammaRamp[i + 512] = gam * 255;
	}

	GLimp_SetGammaRamp(ri.gammaRamp);
}


/*
==================
R_InitSpecialTextures
==================
*/
#define INTTEXSIZE	32
#define INTTEXBYTES	4

static void R_InitSpecialTextures()
{
	byte *data;

	/*
	** ri.media.noTexture
	*/
	data = (byte*)Mem_PoolAlloc(INTTEXSIZE * INTTEXSIZE * 4 * INTTEXBYTES, ri.imageSysPool, 0);
	for (int x=0 ; x<INTTEXSIZE*2 ; x++)
	{
		for (int y=0 ; y<INTTEXSIZE*2 ; y++)
		{
			byte color;
			if ((x == 0 || x == INTTEXSIZE*2-1) || (y == 0 || y == INTTEXSIZE*2-1))
			{
				// Out-most border
				color = 43;
			}
			else
			{
				// Alternate innards (checkerboard)
				if ((x >= INTTEXSIZE) ^ (y >= INTTEXSIZE))
				{
					color = 170;
				}
				else
				{
					color = 85;
				}

				// Brighten inner-most borders of the checkerboard
				if(x == 1 || x == INTTEXSIZE || x == INTTEXSIZE-1 || x == INTTEXSIZE*2-2
				|| y == 1 || y == INTTEXSIZE || y == INTTEXSIZE-1 || y == INTTEXSIZE*2-2)
					color += 43;
			}

			data[(y*INTTEXSIZE*2 + x)*INTTEXBYTES+0] = color;
			data[(y*INTTEXSIZE*2 + x)*INTTEXBYTES+1] = color;
			data[(y*INTTEXSIZE*2 + x)*INTTEXBYTES+2] = color;
			data[(y*INTTEXSIZE*2 + x)*INTTEXBYTES+3] = 255;
		}
	}

	memset(&ri.media.noTexture, 0, sizeof(ri.media.noTexture));
	ri.media.noTexture = R_Create2DImage("***r_noTexture***", &data, INTTEXSIZE*2, INTTEXSIZE*2,
		IF_NOFLUSH|IF_NOPICMIP|IF_NOGAMMA|IF_NOINTENS|IF_NOCOMPRESS|IF_GREYSCALE, 3);
	Mem_Free(data);

	data = (byte*)Mem_PoolAlloc(INTTEXSIZE * INTTEXSIZE * INTTEXBYTES, ri.imageSysPool, 0);

	/*
	** ri.media.whiteTexture
	*/
	memset(data, 255, INTTEXSIZE * INTTEXSIZE * INTTEXBYTES);
	memset(&ri.media.whiteTexture, 0, sizeof(ri.media.whiteTexture));
	ri.media.whiteTexture = R_Create2DImage("***r_whiteTexture***", &data, INTTEXSIZE, INTTEXSIZE,
		IF_NOFLUSH|IF_NOPICMIP|IF_NOGAMMA|IF_NOINTENS|IF_NOCOMPRESS|IF_GREYSCALE, 3);

	/*
	** ri.media.blackTexture
	*/
	memset(data, 0, INTTEXSIZE * INTTEXSIZE * INTTEXBYTES);
	memset(&ri.media.blackTexture, 0, sizeof(ri.media.blackTexture));
	ri.media.blackTexture = R_Create2DImage("***r_blackTexture***", &data, INTTEXSIZE, INTTEXSIZE,
		IF_NOFLUSH|IF_NOPICMIP|IF_NOGAMMA|IF_NOINTENS|IF_NOCOMPRESS|IF_GREYSCALE, 3);

	Mem_Free(data);

	/*
	** ri.media.cinTexture
	** Reserve a texNum for cinematics
	*/
	data = (byte*)Mem_PoolAlloc(256 * 256 * 4, ri.imageSysPool, 0);
	memset(data, 0, 256 * 256 * 4);
	memset(&ri.media.cinTexture, 0, sizeof(ri.media.cinTexture));
	ri.media.cinTexture = R_Create2DImage ("***r_cinTexture***", &data, 256, 256,
		IF_NOFLUSH|IF_NOPICMIP|IF_NOMIPMAP_LINEAR|IF_CLAMP_ALL|IF_NOINTENS|IF_NOGAMMA|IF_NOCOMPRESS, 3);

	/*
	** ri.media.dLightTexture
	*/
	int size;
	if (ri.config.ext.bTex3D)
	{
		size = 32;
		data = (byte*)Mem_PoolAlloc(size * size * size * 4, ri.imageSysPool, 0);
	}
	else
	{
		size = 64;
		data = (byte*)Mem_PoolAlloc(size * size * 4, ri.imageSysPool, 0);
	}

	for (int x=0 ; x<size ; x++)
	{
		for (int y=0 ; y<size ; y++)
		{
			for (int z=0 ; z<size ; z++)
			{
				vec3_t v;

				v[0] = ((x + 0.5f) * (2.0f / (float)size) - 1.0f) * (1.0f / 0.9375f);
				v[1] = ((y + 0.5f) * (2.0f / (float)size) - 1.0f) * (1.0f / 0.9375f);
				if (ri.config.ext.bTex3D)
					v[2] = ((z + 0.5f) * (2.0f / (float)size) - 1.0f) * (1.0f / 0.9375f);
				else
					v[2] = 0;

				float intensity = 1.0f - Vec3Length(v);
				if (intensity > 0)
					intensity *= 256.0f;
				byte d = clamp(intensity, 0, 255);

				data[((z*size+y)*size + x) * 4 + 0] = d;
				data[((z*size+y)*size + x) * 4 + 1] = d;
				data[((z*size+y)*size + x) * 4 + 2] = d;
				data[((z*size+y)*size + x) * 4 + 3] = 255;

				if (!ri.config.ext.bTex3D)
					break;
			}
		}
	}

	memset(&ri.media.dLightTexture, 0, sizeof(ri.media.dLightTexture));
	if (ri.config.ext.bTex3D)
		ri.media.dLightTexture = R_Create3DImage("***r_dLightTexture***", &data, size, size, size,
			IF_NOFLUSH|IF_NOPICMIP|IF_NOMIPMAP_LINEAR|IF_NOINTENS|IF_NOGAMMA|IF_CLAMP_ALL|IF_GREYSCALE|IT_3D, 4);
	else
		ri.media.dLightTexture = R_Create2DImage("***r_dLightTexture***", &data, size, size,
			IF_NOFLUSH|IF_NOPICMIP|IF_NOMIPMAP_LINEAR|IF_NOINTENS|IF_NOGAMMA|IF_CLAMP_ALL|IF_GREYSCALE, 4);
	Mem_Free(data);

	/*
	** ri.media.fogTexture
	*/
	double tw = 1.0f / ((float)FOGTEX_WIDTH - 1.0f);
	double th = 1.0f / ((float)FOGTEX_HEIGHT - 1.0f);

	data = (byte*)Mem_PoolAlloc(FOGTEX_WIDTH * FOGTEX_HEIGHT * 4, ri.imageSysPool, 0);
	memset (data, 255, FOGTEX_WIDTH*FOGTEX_HEIGHT*4);

	float ty = 0.0f;
	for (int y=0 ; y<FOGTEX_HEIGHT ; y++, ty+=th)
	{
		float tx = 0.0f;
		for (int x=0 ; x<FOGTEX_WIDTH ; x++, tx+=tw)
		{
			double t = sqrt(tx) * 255.0;
			data[(x+y*FOGTEX_WIDTH)*4+3] = (byte)(min (t, 255.0));
		}

		data[y*4+3] = 0;
	}
	memset(&ri.media.fogTexture, 0, sizeof(ri.media.fogTexture));
	ri.media.fogTexture = R_Create2DImage("***r_fogTexture***", &data, FOGTEX_WIDTH, FOGTEX_HEIGHT,
		IF_NOFLUSH|IF_NOPICMIP|IF_NOMIPMAP_LINEAR|IF_NOINTENS|IF_NOGAMMA|IF_CLAMP_ALL, 4);
	Mem_Free(data);

	RB_CheckForError("R_InitSpecialTextures");
}


/*
===============
R_ImageInit
===============
*/
void R_ImageInit()
{
	int		i, j;
	float	gam;
	int		red, green, blue;
	uint32	v;
	byte	*pal;

	uint32 startCycles = Sys_Cycles();
	Com_Printf(0, "\n--------- Image Initialization ---------\n");

	// Registration
	cmd_imageList	= Cmd_AddCommand("imagelist",	0, R_ImageList_f,			"Prints out a list of the currently loaded textures");
	cmd_screenShot	= Cmd_AddCommand("screenshot",	0, R_ScreenShot_f,			"Takes a screenshot");

	// Set the initial state
	GL_TextureMode(true, false);
	GL_TextureBits(true, false);

	// Get the palette
	Com_DevPrintf(0, "Loading pallete table\n");
	R_LoadPCX("pics/colormap.pcx", NULL, &pal, NULL, NULL);
	if (!pal)
	{
		Com_Printf(0, "...not found, using internal default\n");
		pal = r_defaultImagePal;
	}

	for (i=0 ; i<256 ; i++)
	{
		red = pal[i*3+0];
		green = pal[i*3+1];
		blue = pal[i*3+2];
		
		v = (255<<24) + (red<<0) + (green<<8) + (blue<<16);
		r_paletteTable[i] = LittleLong (v);
	}

	r_paletteTable[255] &= LittleLong (0xffffff);	// 255 is transparent

	if (pal != r_defaultImagePal)
		Mem_Free(pal);

	// Set up the gamma and intensity ramps
	Com_DevPrintf(0, "Creating software gamma and intensity ramps\n");
	if (intensity->floatVal < 1)
		Cvar_VariableSetValue(intensity, 1, true);
	ri.inverseIntensity = 1.0f / intensity->floatVal;
	ri.identityLighting = FloatToByte(ri.inverseIntensity);

	// Hack! because voodoo's are nasty bright
	if (ri.renderClass == REND_CLASS_VOODOO)
		gam = 1.0f;
	else
		gam = vid_gamma->floatVal;

	// Gamma
	if (gam == 1)
	{
		for (i=0 ; i<256 ; i++)
			r_gammaTable[i] = i;
	}
	else
	{
		for (i=0 ; i<256 ; i++)
		{
			j = (byte)(255 * pow ((i + 0.5f)*0.0039138943248532289628180039138943f, gam) + 0.5f);
			if (j < 0)
				j = 0;
			else if (j > 255)
				j = 255;

			r_gammaTable[i] = j;
		}
	}

	// Intensity (eww)
	for (i=0 ; i<256 ; i++)
	{
		j = i * intensity->intVal;
		if (j > 255)
			j = 255;
		r_intensityTable[i] = j;
	}

	// Get gamma ramp
	Com_DevPrintf(0, "Downloading desktop gamma ramp\n");
	ri.bRampDownloaded = GLimp_GetGammaRamp(ri.originalRamp);
	if (ri.bRampDownloaded)
	{
		Com_DevPrintf(0, "...GLimp_GetGammaRamp succeeded\n");
		ri.config.bHWGammaAvail = true;
	}
	else
	{
		Com_Printf(PRNT_ERROR, "...GLimp_GetGammaRamp failed!\n");
		ri.config.bHWGammaAvail = false;
	}

	// Use hardware gamma?
	ri.config.bHWGammaInUse = (ri.bRampDownloaded && r_hwGamma->intVal);
	if (ri.config.bHWGammaInUse)
	{
		Com_Printf(0, "...using hardware gamma\n");
		R_UpdateGammaRamp();
	}
	else
		Com_Printf(0, "...using software gamma\n");

	// Load up special textures
	Com_DevPrintf(0, "Generating internal textures\n");
	R_InitSpecialTextures();

	Com_Printf(0, "----------------------------------------\n");

	// Check memory integrity
	Mem_CheckPoolIntegrity(ri.imageSysPool);

	Com_Printf(0, "init time: %6.2fms\n", (Sys_Cycles()-startCycles) * Sys_MSPerCycle());
	Com_Printf(0, "----------------------------------------\n");
}


/*
===============
R_ImageShutdown
===============
*/
void R_ImageShutdown()
{
	_lastTexNum = 0;
	Com_Printf(0, "Image system shutdown:\n");

	// Replace gamma ramp
	if (ri.config.bHWGammaInUse)
		GLimp_SetGammaRamp(ri.originalRamp);

	// Unregister commands
	Cmd_RemoveCommand(cmd_imageList);
	Cmd_RemoveCommand(cmd_screenShot);

	// Free loaded textures
	for (auto it = imageList.begin(); it != imageList.end(); ++it)
	{
		refImage_t *image = (*it).second;
		if (!image->touchFrame || !image->texNum)
			continue;	// Free r_imageList slot

		// Free it
		glDeleteTextures(1, &image->texNum);

		// Release any FBO data
		if (ri.config.ext.bFrameBufferObject)
		{
			if (image->flags & (IT_FBO|IT_DEPTHFBO))
			{
				qglDeleteFramebuffersEXT(1, &image->fboNum);
			}
			else if (image->flags & IT_DEPTHANDFBO)
			{
				qglDeleteFramebuffersEXT(1, &image->fboNum);
				qglDeleteFramebuffersEXT(1, &image->depthFBONum);
			}
		}

		delete image;
	}

	// Clear everything
	imageList.clear();

	memset(ri.media.lmTextures, 0, sizeof(refImage_t*) * MAX_LIGHTMAP_IMAGES);
	memset(ri.media.shadowTextures, 0, sizeof(refImage_t*) * MAX_SHADOW_GROUPS);

	// Free memory
	R_ReleaseTexBuffers();

	uint32 size = Mem_FreePool(ri.imageSysPool);
	Com_Printf (0, "...releasing %u bytes...\n", size);
}
