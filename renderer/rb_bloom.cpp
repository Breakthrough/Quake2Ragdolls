/*
Copyright (C) Forest Hale
Copyright (C) 2006-2007 German Garcia

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

// rb_bloom.cpp: 2D lighting post process effect

#include "rb_local.h"

/*
==============================================================================

LIGHT BLOOMS

==============================================================================
*/

cVar_t	 *r_bloom;
cVar_t	 *r_bloom_alpha;
cVar_t	 *r_bloom_diamond_size;
cVar_t	 *r_bloom_intensity;
cVar_t	 *r_bloom_darken;
cVar_t	 *r_bloom_sample_size;
cVar_t	 *r_bloom_fast_sample;
cVar_t	 *r_bloom_spread_scale;
cVar_t	 *r_bloom_radial;
cVar_t	 *r_bloom_radial_distance;
cVar_t	 *r_bloom_radial_alpha;
cVar_t	 *r_bloom_radial_distance_increase;
cVar_t	 *r_bloom_radial_alpha_loss;

static struct
{
	bool initialized;
	bool enabled;

	int bloomDiamondSize;
	float *bloomDiamond;

	int bloomSampleSize;
	vec2_t screenTexCoords;
	vec2_t sampleTexCoords;

	vec2_t downSampleSize;
	vec2_t smallSampleSize;

	refImage_t *bloom_downsampleFBO2x;
	refImage_t *bloom_downsampleFBO1x;
	refImage_t *bloom_effectTexture;
	refImage_t *bloom_radialTexture;
}
rb_bloom = { false };

void GenerateDiamond (int size, float *diamond)
{
	int center = size / 2;

	vec2_t centerVec = {center, center};
	float high = -99999;

	float *curD = diamond;
	for (int y = 0; y < size; ++y)
	{
		for (int x = 0; x < size; ++x)
		{
			vec2_t curVec = {x, y};
			float dist = Vec2Dist(curVec, centerVec);

			if (dist > high)
				high = dist;

			*curD = dist;
			++curD;
		}
	}

	var divHigh = (high / 0.9f);

	curD = diamond;
	for (int y = 0; y < size; ++y)
	{
		for (int x = 0; x < size; ++x)
		{
			*curD = (high - *curD) / divHigh;
			curD++;
		}
	}
}

void RB_BloomInit ()
{
	r_bloom					= Cvar_Register( "r_bloom",					"0",	CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	r_bloom_alpha			= Cvar_Register( "r_bloom_alpha",			"0.2",	CVAR_ARCHIVE );
	r_bloom_diamond_size	= Cvar_Register( "r_bloom_diamond_size",	"9",	CVAR_ARCHIVE );
	r_bloom_intensity		= Cvar_Register( "r_bloom_intensity",		"0.5",	CVAR_ARCHIVE );
	r_bloom_darken			= Cvar_Register( "r_bloom_darken",			"7",	CVAR_ARCHIVE );
	r_bloom_sample_size		= Cvar_Register( "r_bloom_sample_size",		"512",	CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	r_bloom_fast_sample		= Cvar_Register( "r_bloom_fast_sample",		"0",	CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	r_bloom_spread_scale	= Cvar_Register( "r_bloom_spread_scale",	"1",	CVAR_ARCHIVE);

	r_bloom_radial			= Cvar_Register( "r_bloom_radial",	"0",	CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	r_bloom_radial_distance	= Cvar_Register( "r_bloom_radial_distance",	"12",	CVAR_ARCHIVE);
	r_bloom_radial_alpha	= Cvar_Register( "r_bloom_radial_alpha",	"0.4",	CVAR_ARCHIVE);
	r_bloom_radial_distance_increase = Cvar_Register ("r_bloom_radial_distance_increase", "4", CVAR_ARCHIVE);
	r_bloom_radial_alpha_loss = Cvar_Register ("r_bloom_radial_alpha_loss", "0.07", CVAR_ARCHIVE);

	if (rb_bloom.initialized)
		throw Exception(); // shouldn't happen

	rb_bloom.enabled = r_bloom->intVal == 1;

	if (!rb_bloom.enabled)
		return; // nope

	if (!ri.config.ext.bFrameBufferObject)
	{
		Com_Printf(PRNT_WARNING, "WARNING: Bloom enabled, but framebuffer objects are not supported!\n");
		return;
	}

	// constant variables
	rb_bloom.bloomSampleSize = Q_NearestPow(Min<>(r_bloom_sample_size->intVal, Min<>(ri.config.vidWidth, ri.config.vidHeight)), true);

	if (rb_bloom.bloomSampleSize < 32)
		rb_bloom.bloomSampleSize = 32;

	Vec2Set(rb_bloom.screenTexCoords,	(float)ri.config.vidWidth / (float)ri.screenFBO->width,
										(float)ri.config.vidHeight / (float)ri.screenFBO->height);

	if( ri.config.vidHeight > ri.config.vidWidth )
	{
		Vec2Set(rb_bloom.sampleTexCoords,	(float)ri.config.vidWidth / (float)ri.config.vidHeight,
											1.0f);
	}
	else
	{
		Vec2Set(rb_bloom.sampleTexCoords,	1.0f,
											(float)ri.config.vidHeight / (float)ri.config.vidWidth);
	}

	Vec2Set(rb_bloom.smallSampleSize,	rb_bloom.bloomSampleSize * rb_bloom.sampleTexCoords[0],
										rb_bloom.bloomSampleSize * rb_bloom.sampleTexCoords[1]);

	if (!r_bloom_fast_sample->intVal && (ri.config.vidWidth > (rb_bloom.bloomSampleSize * 2)))
	{
		var downsampleSize = (int)(rb_bloom.bloomSampleSize * 2);

		Vec2Set(rb_bloom.downSampleSize,	downsampleSize * rb_bloom.sampleTexCoords[0],
											downsampleSize * rb_bloom.sampleTexCoords[1]);

		rb_bloom.bloom_downsampleFBO2x = R_Create2DImage("***bloomdownsamplefbo2x***", null, rb_bloom.downSampleSize[0], rb_bloom.downSampleSize[1], IT_FBO|IF_NOFLUSH|IF_NOPICMIP|IF_NOCOMPRESS|IF_NOMIPMAP_LINEAR|IF_CLAMP_ALL, 1);
	}
	else
		rb_bloom.bloom_downsampleFBO2x = null;

	var bloomFlags = IT_FBO|IF_NOFLUSH|IF_NOPICMIP|IF_NOCOMPRESS|IF_NOMIPMAP_LINEAR|IF_CLAMP_ALL;
	rb_bloom.bloom_downsampleFBO1x = R_Create2DImage("***bloomdownsamplefbo1x***", null, rb_bloom.smallSampleSize[0], rb_bloom.smallSampleSize[1], bloomFlags, 1);
	rb_bloom.bloom_effectTexture = R_Create2DImage("***bloomeffecttexture***", null, rb_bloom.smallSampleSize[0], rb_bloom.smallSampleSize[1], bloomFlags, 1);
	
	if (r_bloom_radial->intVal)
		rb_bloom.bloom_radialTexture = R_Create2DImage("***bloomradialtexture***", null, rb_bloom.downSampleSize[0], rb_bloom.downSampleSize[1], bloomFlags, 1);

	rb_bloom.bloomDiamondSize = r_bloom_diamond_size->intVal;

	if (!(rb_bloom.bloomDiamondSize & 1))
		rb_bloom.bloomDiamondSize++;

	rb_bloom.bloomDiamond = new float[rb_bloom.bloomDiamondSize * rb_bloom.bloomDiamondSize];
	GenerateDiamond(rb_bloom.bloomDiamondSize, rb_bloom.bloomDiamond);
	rb_bloom.initialized = true;
}

void RB_BloomShutdown ()
{
	if (rb_bloom.initialized)
		delete rb_bloom.bloomDiamond;

	rb_bloom.initialized = false;
}

/*
=================
R_Bloom_SamplePass
=================
*/
static inline void R_Bloom_SamplePass( float xpos, float ypos )
{
	glBegin( GL_QUADS );
	glTexCoord2f( 0, 1 );
	glVertex2f( xpos, ypos );
	glTexCoord2f( 0, 0 );
	glVertex2f( xpos, ypos+ri.config.vidHeight );
	glTexCoord2f( 1, 0 );
	glVertex2f( xpos+ri.config.vidWidth, ypos+ri.config.vidHeight );
	glTexCoord2f( 1, 1 );
	glVertex2f( xpos+ri.config.vidWidth, ypos );
	glEnd();
}

/*
=================
R_Bloom_Quad
=================
*/
static inline void R_Bloom_Quad( int x, int y, int w, int h, float texwidth, float texheight )
{
	glBegin( GL_QUADS );
	glTexCoord2f( 0, texheight );
	glVertex2f( x, ri.config.vidHeight-h-y );
	glTexCoord2f( 0, 0 );
	glVertex2f( x, ri.config.vidHeight-y );
	glTexCoord2f( texwidth, 0 );
	glVertex2f( x+w, ri.config.vidHeight-y );
	glTexCoord2f( texwidth, texheight );
	glVertex2f( x+w, ri.config.vidHeight-h );
	glEnd();
}

static void Bloom_GenerateDownsample2x ()
{
	// stepped downsample
	RB_BeginFBOUpdate(rb_bloom.bloom_downsampleFBO2x);

	RB_BindTexture(ri.screenFBO);
	R_Bloom_Quad(0, 0, ri.config.vidWidth, ri.config.vidHeight, rb_bloom.screenTexCoords[0], rb_bloom.screenTexCoords[1]);

	RB_EndFBOUpdate();
}

static void Bloom_GenerateDownsample1x ()
{
	RB_BeginFBOUpdate(rb_bloom.bloom_downsampleFBO1x);

	RB_BindTexture(ri.screenFBO);
	glColor4f( 0.5f, 0.5f, 0.5f, 1.0f );
	R_Bloom_Quad(0, 0, ri.config.vidWidth, ri.config.vidHeight, rb_bloom.screenTexCoords[0], rb_bloom.screenTexCoords[1]);

	// now blend the big screen texture into the bloom generation space (hoping it adds some blur)

	RB_BindTexture((rb_bloom.bloom_downsampleFBO2x == null) ? ri.screenFBO : rb_bloom.bloom_downsampleFBO2x);
	RB_StateForBits( SB_BLENDSRC_ONE|SB_BLENDDST_ONE);

	if (rb_bloom.bloom_downsampleFBO2x)
		R_Bloom_Quad( 0, 0, ri.config.vidWidth, ri.config.vidHeight, 1, 1);
	else
		R_Bloom_Quad(0, 0, ri.config.vidWidth, ri.config.vidHeight, rb_bloom.screenTexCoords[0], rb_bloom.screenTexCoords[1]);
	glColor4f( 1.0f, 1.0f, 1.0f, 1.0f );

	RB_EndFBOUpdate();
}

void R_TransformToScreen_Vec3(vec3_t in, vec3_t out);
static void Bloom_GenerateBloom()
{
	RB_BeginFBOUpdate(rb_bloom.bloom_effectTexture);

	RB_BindTexture(rb_bloom.bloom_downsampleFBO1x);

	// start modifying the small scene corner
	glColor4f( 1.0f, 1.0f, 1.0f, 1.0f );

	R_Bloom_Quad(0, 0, ri.config.vidWidth, ri.config.vidHeight, 1, 1);

	// darkening passes
	if( r_bloom_darken->intVal )
	{
		RB_TextureEnv( GL_MODULATE );
		RB_StateForBits( SB_BLENDSRC_DST_COLOR|SB_BLENDDST_ZERO);

		for( int i = 0; i < r_bloom_darken->intVal; i++ )
			R_Bloom_SamplePass( 0, 0 );
	}

	RB_EndFBOUpdate();

	var scale = r_bloom_intensity->floatVal;
	var k = rb_bloom.bloomDiamondSize / 2;
	var diamond = rb_bloom.bloomDiamond;

	RB_BeginFBOUpdate(rb_bloom.bloom_downsampleFBO1x);

	glClear(GL_COLOR_BUFFER_BIT);

	RB_BindTexture(rb_bloom.bloom_effectTexture);

	// bluring passes
	RB_StateForBits( 0 );
	R_Bloom_SamplePass(0, 0);

	// bluring passes
	RB_StateForBits( SB_BLENDSRC_ONE|SB_BLENDDST_ONE_MINUS_SRC_COLOR );

	for( int i = 0; i < rb_bloom.bloomDiamondSize; i++ )
	{
		for( int j = 0; j < rb_bloom.bloomDiamondSize; j++, diamond++ )
		{
			var intensity =  *diamond * scale;
			if( intensity < 0.01f )
				continue;

			glColor4f( intensity, intensity, intensity, 1.0 );
			R_Bloom_SamplePass( (i - k) * r_bloom_spread_scale->floatVal, (j - k) * r_bloom_spread_scale->floatVal);
		}
	}

	RB_EndFBOUpdate();

	if (r_bloom_radial->intVal)
	{
		RB_BeginFBOUpdate(rb_bloom.bloom_radialTexture);

		RB_BindTexture(rb_bloom.bloom_effectTexture);
		RB_StateForBits(SB_BLENDSRC_SRC_ALPHA|SB_BLENDDST_ONE);

		R_Bloom_Quad(0, 0, ri.config.vidWidth, ri.config.vidHeight, 1, 1);

		glClear(GL_COLOR_BUFFER_BIT);

		float alpha = r_bloom_radial_alpha->floatVal;
		int inc = r_bloom_radial_distance_increase->intVal;

		for (int x = 0; x < r_bloom_radial_distance->intVal; ++x)
		{
			glColor4f( 1, 1, 1, alpha );

			R_Bloom_Quad(-inc, -inc, ri.config.vidWidth + (inc * 2), ri.config.vidHeight + (inc * 2), 1, 1);

			inc += r_bloom_radial_distance_increase->intVal;
			alpha -= r_bloom_radial_alpha_loss->floatVal;
		}

		RB_EndFBOUpdate();
	}
}

/*
=================
R_BloomBlend
=================
*/
void R_BloomBlend( const refDef_t *fd )
{
	if (!(fd->rdFlags & RDF_FBO) || !ri.config.ext.bFrameBufferObject)
		return;

	// set up full screen workspace
	glScissor( 0, 0, ri.config.vidWidth, ri.config.vidHeight );
	glViewport( 0, 0, ri.config.vidWidth, ri.config.vidHeight );
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();
	glOrtho( 0, ri.config.vidWidth, ri.config.vidHeight, 0, -10, 100 );
	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity();

	RB_StateForBits(0);

	RB_BindTexture(ri.screenFBO);
	glColor4f( 1, 1, 1, 1 );
	glBegin(GL_QUADS);
	int topBorder = ri.screenFBO->height - ri.config.vidHeight;
	glTexCoord2f(0, 0); glVertex2f(0, ri.screenFBO->height - topBorder);
	glTexCoord2f(1, 0); glVertex2f(ri.screenFBO->width, ri.screenFBO->height - topBorder);
	glTexCoord2f(1, 1); glVertex2f(ri.screenFBO->width, -topBorder);
	glTexCoord2f(0, 1); glVertex2f(0, -topBorder);
	glEnd();

	if (!rb_bloom.enabled)
		return;

	if (rb_bloom.bloom_downsampleFBO2x)
		Bloom_GenerateDownsample2x();
	Bloom_GenerateDownsample1x();
	Bloom_GenerateBloom();

	glScissor(ri.def.x, ri.config.vidHeight - ri.def.height - ri.def.y, ri.def.width, ri.def.height);

	glColor4f( 1, 1, 1, 1 );

	RB_TextureEnv( GL_MODULATE );
	RB_StateForBits(SB_BLENDSRC_ONE|SB_BLENDDST_ONE);
	glColor4f( r_bloom_alpha->floatVal, r_bloom_alpha->floatVal, r_bloom_alpha->floatVal, 1.0f );

	RB_BindTexture(rb_bloom.bloom_downsampleFBO1x);
	R_Bloom_Quad( 0, 0, ri.config.vidWidth, ri.config.vidHeight, 1.0f, 1.0f );

	if (r_bloom_radial->intVal)
	{
		RB_BindTexture(rb_bloom.bloom_radialTexture);
		R_Bloom_Quad( 0, 0, ri.config.vidWidth, ri.config.vidHeight, 1.0f, 1.0f );
	}

	glViewport(ri.def.x, ri.config.vidHeight - ri.def.height - ri.def.y, ri.def.width, ri.def.height);

	RB_CheckForError("RB_BloomBlend");
}
