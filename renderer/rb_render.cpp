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
// rb_render.cpp
//

#include "rb_local.h"

/*
===============================================================================

	ARRAYS

===============================================================================
*/

// Backend data
rbData_t				rb;

// Dynamic data
static matPass_t		*rb_accumPasses[MAX_TEXUNITS];
static int				rb_numPasses;
static int				rb_numOldPasses;

static bool				rb_bArraysLocked;
static bool				rb_bNormalsEnabled;
static bool				rb_bColorArrayEnabled;

static float			rb_matTime;
static stateBit_t		rb_stateBits;

// Static data
static matPass_t		rb_dLightPass;
static matPass_t		rb_shadowPass;
static matPass_t		rb_fogPass;
static matPass_t		rb_debugLightMapPass;

// Math tables
#define FTABLE_SIZE 2048
#define FTABLE_CLAMP(x) (((int)((x)*FTABLE_SIZE) & (FTABLE_SIZE-1)))
#define FTABLE_EVALUATE(table,x) (table[FTABLE_CLAMP(x)])

float					rb_sinTable[FTABLE_SIZE];
float					rb_triangleTable[FTABLE_SIZE];
float					rb_squareTable[FTABLE_SIZE];
float					rb_sawtoothTable[FTABLE_SIZE];
float					rb_inverseSawtoothTable[FTABLE_SIZE];
float					rb_noiseTable[FTABLE_SIZE];

/*
==============
RB_FastSin
==============
*/
float RB_FastSin(const float t)
{
	return FTABLE_EVALUATE(rb_sinTable, t);
}


/*
==============
RB_TableForFunc
==============
*/
inline float *RB_TableForFunc(const EMatTableFunc func)
{
	switch(func)
	{
	case MAT_FUNC_SIN:				return rb_sinTable;
	case MAT_FUNC_TRIANGLE:			return rb_triangleTable;
	case MAT_FUNC_SQUARE:			return rb_squareTable;
	case MAT_FUNC_SAWTOOTH:			return rb_sawtoothTable;
	case MAT_FUNC_INVERSESAWTOOTH:	return rb_inverseSawtoothTable;
	case MAT_FUNC_NOISE:			return rb_noiseTable;
	}

	// Assume error
	Com_Error(ERR_FATAL, "RB_TableForFunc: unknown function");
	return NULL;
}


/*
=============
RB_LockArrays
=============
*/
static void RB_LockArrays()
{
	if (!rb_bArraysLocked)
	{
		if (ri.config.ext.bCompiledVertArray)
			qglLockArraysEXT(0, rb.numVerts);

		assert(rb.inVertices);
		glVertexPointer(3, GL_FLOAT, 0, rb.inVertices);

		if (rb.curMeshFeatures & MF_ENABLENORMALS)
		{
			rb_bNormalsEnabled = true;
			glEnableClientState(GL_NORMAL_ARRAY);
			assert(rb.inNormals);
			glNormalPointer(GL_FLOAT, 16, rb.inNormals);
		}

		rb_bArraysLocked = true;
	}
}


/*
=============
RB_UnlockArrays
=============
*/
static void RB_UnlockArrays()
{
	if (rb_bArraysLocked)
	{
		if (ri.config.ext.bCompiledVertArray)
			qglUnlockArraysEXT();

		if (rb_bNormalsEnabled)
		{
			rb_bNormalsEnabled = false;
			glDisableClientState(GL_NORMAL_ARRAY);
		}

		rb_bArraysLocked = false;
	}
}


/*
=============
RB_ResetPointers
=============
*/
static void RB_ResetPointers()
{
	rb.inColors = NULL;
	rb.inCoords = NULL;
	rb.inIndices = NULL;
	rb.inLMCoords = NULL;
	rb.inNormals = NULL;
	rb.inSVectors = NULL;
	rb.inTVectors = NULL;
	rb.inVertices = NULL;

	rb.curEntity = NULL;
	rb.curDLightBits = 0;
	rb.curShadowBits = 0;
	rb.curLMTexNum = BAD_LMTEXNUM;
	rb.curMeshFeatures = 0;
	rb.curMeshType = MBT_MAX;
	rb.curPatchWidth = 0;
	rb.curPatchHeight = 0;
	rb.curMat = NULL;

	rb.curTexFog = NULL;
	rb.curColorFog = NULL;

	rb.numIndexes = 0;
	rb.numVerts = 0;

	rb_numPasses = 0;
	rb_stateBits = 0;
}

/*
===============================================================================

	PASS HANDLING

===============================================================================
*/

/*
=============
RB_SetColor
=============
*/
static inline void RB_SetColor(colorb *colorArray, const int numColors = 1)
{
	if (!colorArray)
	{
		if (rb_bColorArrayEnabled)
		{
			glDisableClientState(GL_COLOR_ARRAY);
			rb_bColorArrayEnabled = false;
		}
	}
	else if (numColors == 1)
	{
		if (rb_bColorArrayEnabled)
		{
			glDisableClientState(GL_COLOR_ARRAY);
			rb_bColorArrayEnabled = false;
		}
		glColor4ubv(colorArray[0]);
	}
	else
	{
		if (!rb_bColorArrayEnabled)
		{
			glEnableClientState(GL_COLOR_ARRAY);
			rb_bColorArrayEnabled = true;
		}
		assert(colorArray);
		glColorPointer(4, GL_UNSIGNED_BYTE, 0, colorArray);
	}
}


/*
=============
RB_SetupColorFog
=============
*/
static void RB_SetupColorFog(const matPass_t *pass, int numColors)
{
	const int blendSource = pass->stateBits & SB_BLENDSRC_MASK;
	const int blendDest = pass->stateBits & SB_BLENDDST_MASK;

	bool alphaFog;
	if ((blendSource != SB_BLENDSRC_SRC_ALPHA && blendDest != SB_BLENDDST_SRC_ALPHA)
	&& (blendSource != SB_BLENDSRC_ONE_MINUS_SRC_ALPHA && blendDest != SB_BLENDDST_ONE_MINUS_SRC_ALPHA))
		alphaFog = false;
	else
		alphaFog = true;

	plane_t *fogPlane = rb.curColorFog->visiblePlane;
	double fogMatDist = rb.curColorFog->mat->fogDist;
	double dist = PlaneDiff(ri.def.viewOrigin, fogPlane);

	vec3_t viewToFog;
	if (rb.curMat->flags & MAT_SKY)
	{
		if (dist > 0)
			Vec3Scale(fogPlane->normal, -dist, viewToFog);
		else
			Vec3Clear(viewToFog);
	}
	else
		Vec3Copy(rb.curEntity->origin, viewToFog);

	double vpnNormal[3];
	vpnNormal[0] = DotProduct(rb.curEntity->axis[0], ri.def.viewAxis[0]) * fogMatDist * rb.curEntity->scale;
	vpnNormal[1] = DotProduct(rb.curEntity->axis[1], ri.def.viewAxis[0]) * fogMatDist * rb.curEntity->scale;
	vpnNormal[2] = DotProduct(rb.curEntity->axis[2], ri.def.viewAxis[0]) * fogMatDist * rb.curEntity->scale;
	double vpnDist = ((ri.def.viewOrigin[0] - viewToFog[0]) * ri.def.viewAxis[0][0]
				+ (ri.def.viewOrigin[1] - viewToFog[1]) * ri.def.viewAxis[0][1]
				+ (ri.def.viewOrigin[2] - viewToFog[2]) * ri.def.viewAxis[0][2])
				* fogMatDist;

	byte *bArray = rb.outColorArray[0];
	if (dist < 0)
	{
		// Camera is inside the fog
		for (int i=0 ; i<numColors ; i++, bArray+=4)
		{
			float c = DotProduct(rb.inVertices[i], vpnNormal) - vpnDist;
			int amount = (1.0f - bound(0, c, 1.0f)) * 0xFFFF;

			if (alphaFog)
			{
				bArray[3] = (bArray[3] * amount) >> 16;
			}
			else
			{
				bArray[0] = (bArray[0] * amount) >> 16;
				bArray[1] = (bArray[1] * amount) >> 16;
				bArray[2] = (bArray[2] * amount) >> 16;
			}
		}
	}
	else
	{
		double fogNormal[3];
		fogNormal[0] = DotProduct(rb.curEntity->axis[0], fogPlane->normal) * rb.curEntity->scale;
		fogNormal[1] = DotProduct(rb.curEntity->axis[1], fogPlane->normal) * rb.curEntity->scale;
		fogNormal[2] = DotProduct(rb.curEntity->axis[2], fogPlane->normal) * rb.curEntity->scale;
		int fogptype = (fogNormal[0] == 1.0 ? PLANE_X : (fogNormal[1] == 1.0 ? PLANE_Y : (fogNormal[2] == 1.0 ? PLANE_Z : PLANE_NON_AXIAL)));
		if (fogptype > 2)
			Vec3Scale(fogNormal, fogMatDist, fogNormal);
		double fogDist = (fogPlane->dist - DotProduct(viewToFog, fogPlane->normal)) * fogMatDist;
		dist *= fogMatDist;

		for (int i=0 ; i<numColors ; i++, bArray+=4)
		{
			double vdist;
			if (fogptype < 3)
				vdist = rb.inVertices[i][fogptype] * fogMatDist - fogDist;
			else
				vdist = DotProduct(rb.inVertices[i], fogNormal) - fogDist;

			if (vdist < 0)
			{
				float c = (DotProduct(rb.inVertices[i], vpnNormal) - vpnDist) * vdist / (vdist - dist);
				int amount = (1.0f - bound(0, c, 1.0f)) * 0xFFFF;

				if (alphaFog)
				{
					bArray[3] = (bArray[3] * amount) >> 16;
				}
				else
				{
					bArray[0] = (bArray[0] * amount) >> 16;
					bArray[1] = (bArray[1] * amount) >> 16;
					bArray[2] = (bArray[2] * amount) >> 16;
				}
			}
		}
	}
}


/*
=============
RB_SetupColor
=============
*/
static void RB_SetupColor(const matPass_t *pass)
{
	const materialFunc_t *rgbGenFunc, *alphaGenFunc;
	int		r, g, b;
	float	*table, c, a;
	byte	*bArray, *inArray;
	int		numColors, i;
	vec3_t	t, v;

	rgbGenFunc = &pass->rgbGen.func;
	alphaGenFunc = &pass->alphaGen.func;

	// Optimal case
	if (pass->flags & MAT_PASS_NOCOLORARRAY && !rb.curColorFog)
	{
		numColors = 1;
	}
	else
	{
		numColors = rb.numVerts;
	}

	// Color generation
	bArray = rb.outColorArray[0];
	inArray = rb.inColors[0];
	switch (pass->rgbGen.type)
	{
	case RGB_GEN_UNKNOWN:
	case RGB_GEN_IDENTITY:
		for (i=0 ; i<numColors ; i++, bArray+=4)
		{
			bArray[0] = 255;
			bArray[1] = 255;
			bArray[2] = 255;
		}
		break;

	case RGB_GEN_IDENTITY_LIGHTING:
		for (i=0 ; i<numColors ; i++, bArray+=4)
		{
			bArray[0] = ri.identityLighting;
			bArray[1] = ri.identityLighting;
			bArray[2] = ri.identityLighting;
		}
		break;

	case RGB_GEN_CONST:
		r = pass->rgbGen.bArgs[0];
		g = pass->rgbGen.bArgs[1];
		b = pass->rgbGen.bArgs[2];

		for (i=0 ; i<numColors ; i++, bArray+=4)
		{
			bArray[0] = r;
			bArray[1] = g;
			bArray[2] = b;
		}
		break;

	case RGB_GEN_COLORWAVE:
		table = RB_TableForFunc (rgbGenFunc->type);
		c = rb_matTime * rgbGenFunc->args[3] + rgbGenFunc->args[2];
		c = FTABLE_EVALUATE(table, c) * rgbGenFunc->args[1] + rgbGenFunc->args[0];
		a = pass->rgbGen.fArgs[0] * c; r = FloatToByte (bound (0, a, 1));
		a = pass->rgbGen.fArgs[1] * c; g = FloatToByte (bound (0, a, 1));
		a = pass->rgbGen.fArgs[2] * c; b = FloatToByte (bound (0, a, 1));

		for (i=0 ; i<numColors ; i++, bArray+=4)
		{
			bArray[0] = r;
			bArray[1] = g;
			bArray[2] = b;
		}
		break;

	case RGB_GEN_ENTITY:
		for (i=0 ; i<numColors ; i++, bArray+=4)
			*(colorb *)bArray = rb.curEntity->color;
		break;

	case RGB_GEN_ONE_MINUS_ENTITY:
		for (i=0 ; i<numColors ; i++, bArray+=4)
		{
			bArray[0] = 255 - rb.curEntity->color[0];
			bArray[1] = 255 - rb.curEntity->color[1];
			bArray[2] = 255 - rb.curEntity->color[2];
		}
		break;

	case RGB_GEN_VERTEX:
		if (intensity->intVal > 0)
		{
			for (i=0 ; i<numColors ; i++, bArray+=4, inArray+=4)
			{
				bArray[0] = inArray[0] >> (intensity->intVal / 2);
				bArray[1] = inArray[1] >> (intensity->intVal / 2);
				bArray[2] = inArray[2] >> (intensity->intVal / 2);
			}
			break;
		}

	// FALL THROUGH
	case RGB_GEN_LIGHTING_DIFFUSE:
	case RGB_GEN_EXACT_VERTEX:
		for (i=0 ; i<numColors ; i++, bArray+=4, inArray+=4)
		{
			bArray[0] = inArray[0];
			bArray[1] = inArray[1];
			bArray[2] = inArray[2];
		}
		break;

	case RGB_GEN_ONE_MINUS_VERTEX:
		if (intensity->intVal > 0)
		{
			for (i=0 ; i<numColors ; i++, bArray+=4, inArray+=4)
			{
				bArray[0] = 255 - (inArray[0] >> (intensity->intVal / 2));
				bArray[1] = 255 - (inArray[1] >> (intensity->intVal / 2));
				bArray[2] = 255 - (inArray[2] >> (intensity->intVal / 2));
			}
			break;
		}

	// FALL THROUGH
	case RGB_GEN_ONE_MINUS_EXACT_VERTEX:
		for (i=0 ; i<numColors ; i++, bArray+=4, inArray+=4)
		{
			bArray[0] = 255 - inArray[0];
			bArray[1] = 255 - inArray[1];
			bArray[2] = 255 - inArray[2];
		}
		break;

	case RGB_GEN_FOG:
		for (i=0 ; i<numColors ; i++, bArray+=4)
		{
			bArray[0] = rb.curTexFog->mat->fogColor[0];
			bArray[1] = rb.curTexFog->mat->fogColor[1];
			bArray[2] = rb.curTexFog->mat->fogColor[2];
		}
		break;

	default:
		assert (0);
		break;
	}

	// Alpha generation
	bArray = rb.outColorArray[0];
	inArray = rb.inColors[0];
	switch (pass->alphaGen.type)
	{
	case ALPHA_GEN_UNKNOWN:
	case ALPHA_GEN_IDENTITY:
		for (i=0 ; i<numColors ; i++, bArray+=4)
		{
			bArray[3] = 255;
		}
		break;

	case ALPHA_GEN_CONST:
		b = FloatToByte (pass->alphaGen.args[0]);
		for (i=0 ; i<numColors ; i++, bArray+=4)
			bArray[3] = b;
		break;

	case ALPHA_GEN_WAVE:
		table = RB_TableForFunc (alphaGenFunc->type);
		a = alphaGenFunc->args[2] + rb_matTime * alphaGenFunc->args[3];
		a = FTABLE_EVALUATE(table, a) * alphaGenFunc->args[1] + alphaGenFunc->args[0];
		b = FloatToByte (bound (0.0f, a, 1.0f));
		for (i=0 ; i<numColors ; i++, bArray+=4)
			bArray[3] = b;
		break;

	case ALPHA_GEN_PORTAL:
		Vec3Add (rb.inVertices[0], rb.curEntity->origin, v);
		Vec3Subtract (ri.def.viewOrigin, v, t);
		a = Vec3Length (t) * pass->alphaGen.args[0];
		b = FloatToByte (clamp (a, 0.0f, 1.0f));

		for (i=0 ; i<numColors ; i++, bArray+=4)
			bArray[3] = b;
		break;

	case ALPHA_GEN_VERTEX:
		for (i=0 ; i<numColors ; i++, bArray+=4, inArray+=4)
			bArray[3] = inArray[3];
		break;

	case ALPHA_GEN_ONE_MINUS_VERTEX:
		for (i=0 ; i<numColors ; i++, bArray+=4, inArray+=4)
			bArray[3] = 255 - inArray[3];

	case ALPHA_GEN_ENTITY:
		for (i=0 ; i<numColors ; i++, bArray+=4)
			bArray[3] = rb.curEntity->color[3];
		break;

	case ALPHA_GEN_SPECULAR:
		Vec3Subtract (ri.def.viewOrigin, rb.curEntity->origin, t);
		if (!Matrix3_Compare (rb.curEntity->axis, axisIdentity))
			Matrix3_TransformVector (rb.curEntity->axis, t, v);
		else
			Vec3Copy (t, v);

		for (i=0 ; i<numColors ; i++, bArray+=4)
		{
			Vec3Subtract (v, rb.inVertices[i], t);
			a = DotProduct (t, rb.inNormals[i]) * Q_RSqrtf (DotProduct (t, t));
			a = a * a * a * a * a;
			bArray[3] = FloatToByte (bound (0.0f, a, 1.0f));
		}
		break;

	case ALPHA_GEN_DOT:
		if (!Matrix3_Compare (rb.curEntity->axis, axisIdentity))
			Matrix3_TransformVector (rb.curEntity->axis, ri.def.viewAxis[0], v);
		else
			Vec3Copy (ri.def.viewAxis[0], v);

		for (i=0 ; i<numColors ; i++, bArray+=4)
		{
			a = DotProduct (v, rb.inNormals[i]);
			if (a < 0)
				a = -a;
			bArray[3] = FloatToByte (bound (pass->alphaGen.args[0], a, pass->alphaGen.args[1]));
		}
		break;

	case ALPHA_GEN_ONE_MINUS_DOT:
		if (!Matrix3_Compare (rb.curEntity->axis, axisIdentity))
			Matrix3_TransformVector (rb.curEntity->axis, ri.def.viewAxis[0], v);
		else
			Vec3Copy (ri.def.viewAxis[0], v);

		for (i=0 ; i<numColors ; i++, bArray+=4)
		{
			a = DotProduct (v, rb.inNormals[i]);
			if (a < 0)
				a = -a;
			a = 1.0f - a;
			bArray[3] = FloatToByte (bound (pass->alphaGen.args[0], a, pass->alphaGen.args[1]));
		}
		break;

	case ALPHA_GEN_FOG:
		for (i=0 ; i<numColors ; i++, bArray+=4)
			bArray[3] = rb.curTexFog->mat->fogColor[3];
		break;

	default:
		assert (0);
		break;
	}

	// Colored fog
	if (rb.curColorFog)
		RB_SetupColorFog(pass, numColors);

	// Set color
	RB_SetColor(rb.outColorArray, numColors);
}

static const float r_warpSinTable[] =
{
	0.000000f,	0.098165f,	0.196270f,	0.294259f,	0.392069f,	0.489643f,	0.586920f,	0.683850f,
	0.780360f,	0.876405f,	0.971920f,	1.066850f,	1.161140f,	1.254725f,	1.347560f,	1.439580f,
	1.530735f,	1.620965f,	1.710220f,	1.798445f,	1.885585f,	1.971595f,	2.056410f,	2.139990f,
	2.222280f,	2.303235f,	2.382795f,	2.460925f,	2.537575f,	2.612690f,	2.686235f,	2.758160f,
	2.828425f,	2.896990f,	2.963805f,	3.028835f,	3.092040f,	3.153385f,	3.212830f,	3.270340f,
	3.325880f,	3.379415f,	3.430915f,	3.480350f,	3.527685f,	3.572895f,	3.615955f,	3.656840f,
	3.695520f,	3.731970f,	3.766175f,	3.798115f,	3.827760f,	3.855105f,	3.880125f,	3.902810f,
	3.923140f,	3.941110f,	3.956705f,	3.969920f,	3.980740f,	3.989160f,	3.995180f,	3.998795f,
	4.000000f,	3.998795f,	3.995180f,	3.989160f,	3.980740f,	3.969920f,	3.956705f,	3.941110f,
	3.923140f,	3.902810f,	3.880125f,	3.855105f,	3.827760f,	3.798115f,	3.766175f,	3.731970f,
	3.695520f,	3.656840f,	3.615955f,	3.572895f,	3.527685f,	3.480350f,	3.430915f,	3.379415f,
	3.325880f,	3.270340f,	3.212830f,	3.153385f,	3.092040f,	3.028835f,	2.963805f,	2.896990f,
	2.828425f,	2.758160f,	2.686235f,	2.612690f,	2.537575f,	2.460925f,	2.382795f,	2.303235f,
	2.222280f,	2.139990f,	2.056410f,	1.971595f,	1.885585f,	1.798445f,	1.710220f,	1.620965f,
	1.530735f,	1.439580f,	1.347560f,	1.254725f,	1.161140f,	1.066850f,	0.971920f,	0.876405f,
	0.780360f,	0.683850f,	0.586920f,	0.489643f,	0.392069f,	0.294259f,	0.196270f,	0.098165f,
	0.000000f,	-0.098165f,	-0.196270f,	-0.294259f,	-0.392069f,	-0.489643f,	-0.586920f,	-0.683850f,
	-0.780360f,	-0.876405f,	-0.971920f,	-1.066850f,	-1.161140f,	-1.254725f,	-1.347560f,	-1.439580f,
	-1.530735f,	-1.620965f,	-1.710220f,	-1.798445f,	-1.885585f,	-1.971595f,	-2.056410f,	-2.139990f,
	-2.222280f,	-2.303235f,	-2.382795f,	-2.460925f,	-2.537575f,	-2.612690f,	-2.686235f,	-2.758160f,
	-2.828425f,	-2.896990f,	-2.963805f,	-3.028835f,	-3.092040f,	-3.153385f,	-3.212830f,	-3.270340f,
	-3.325880f,	-3.379415f,	-3.430915f,	-3.480350f,	-3.527685f,	-3.572895f,	-3.615955f,	 -3.656840f,
	-3.695520f,	-3.731970f,	-3.766175f,	-3.798115f,	-3.827760f,	-3.855105f,	-3.880125f,	-3.902810f,
	-3.923140f,	-3.941110f,	-3.956705f,	-3.969920f,	-3.980740f,	-3.989160f,	-3.995180f,	-3.998795f,
	-4.000000f,	-3.998795f,	-3.995180f,	-3.989160f,	-3.980740f,	-3.969920f,	-3.956705f,	-3.941110f,
	-3.923140f,	-3.902810f,	-3.880125f,	-3.855105f,	-3.827760f,	-3.798115f,	-3.766175f,	-3.731970f,
	-3.695520f,	-3.656840f,	-3.615955f,	-3.572895f,	-3.527685f,	-3.480350f,	-3.430915f,	-3.379415f,
	-3.325880f,	-3.270340f,	-3.212830f,	-3.153385f,	-3.092040f,	-3.028835f,	-2.963805f,	-2.896990f,
	-2.828425f,	-2.758160f,	-2.686235f,	-2.612690f,	-2.537575f,	-2.460925f,	-2.382795f,	-2.303235f,
	-2.222280f,	-2.139990f,	-2.056410f,	-1.971595f,	-1.885585f,	-1.798445f,	-1.710220f,	-1.620965f,
	-1.530735f,	-1.439580f,	-1.347560f,	-1.254725f,	-1.161140f,	-1.066850f,	-0.971920f,	-0.876405f,
	-0.780360f,	-0.683850f,	-0.586920f,	-0.489643f,	-0.392069f,	 -0.294259f,-0.196270f,	-0.098165f
};

/*
=============
RB_ModifyTextureCoords
=============
*/
static void RB_ApplyTCMods(matPass_t *pass, mat4x4_t result)
{
	mat4x4_t	m1, m2;
	float		t1, t2, sint, cost;
	float		*table;
	tcMod_t		*tcMod;
	int			i;

	for (i=0, tcMod=pass->tcMods ; i<pass->numTCMods ; tcMod++, i++)
	{
		switch (tcMod->type)
		{
		case TC_MOD_ROTATE:
			cost = tcMod->args[0] * rb_matTime;
			sint = RB_FastSin (cost);
			cost = RB_FastSin (cost + 0.25);
			m2[0] =  cost, m2[1] = sint, m2[12] =  0.5f * (sint - cost + 1);
			m2[4] = -sint, m2[5] = cost, m2[13] = -0.5f * (sint + cost - 1);
			Matrix4_Copy2D (result, m1);
			Matrix4_Multiply2D (m2, m1, result);
			break;

		case TC_MOD_SCALE:
			Matrix4_Scale2D (result, tcMod->args[0], tcMod->args[1]);
			break;

		case TC_MOD_TURB:
			t1 = (1.0f / 4.0f);
			t2 = tcMod->args[2] + rb_matTime * tcMod->args[3];
			Matrix4_Scale2D (result, 1 + (tcMod->args[1] * RB_FastSin (t2) + tcMod->args[0]) * t1, 1 + (tcMod->args[1] * RB_FastSin (t2 + 0.25f) + tcMod->args[0]) * t1);
			break;

		case TC_MOD_STRETCH:
			table = RB_TableForFunc ((EMatTableFunc)(int)tcMod->args[0]);
			t2 = tcMod->args[3] + rb_matTime * tcMod->args[4];
			t1 = FTABLE_EVALUATE (table, t2) * tcMod->args[2] + tcMod->args[1];
			t1 = t1 ? 1.0f / t1 : 1.0f;
			t2 = 0.5f - 0.5f * t1;
			Matrix4_Stretch2D (result, t1, t2);
			break;

		case TC_MOD_SCROLL:
			t1 = tcMod->args[0] * rb_matTime; t1 = t1 - floor (t1);
			t2 = tcMod->args[1] * rb_matTime; t2 = t2 - floor (t2);
			Matrix4_Translate2D (result, t1, t2);
			break;

		case TC_MOD_TRANSFORM:
			m2[0] = tcMod->args[0], m2[1] = tcMod->args[2], m2[12] = tcMod->args[4],
			m2[5] = tcMod->args[1], m2[4] = tcMod->args[3], m2[13] = tcMod->args[5]; 
			Matrix4_Copy2D (result, m1);
			Matrix4_Multiply2D (m2, m1, result);
			break;

		default:
			assert (0);
			break;
		}
	}
}
static bool RB_VertexTCBase(matPass_t *pass, ETextureUnit texUnit, mat4x4_t matrix)
{
	Matrix4_Identity(matrix);

	switch(pass->tcGen)
	{
	case TC_GEN_VECTOR:
	case TC_GEN_REFLECTION:
		break;

	default:
		RB_SetTextureGen(GL_S, 0);
		RB_SetTextureGen(GL_T, 0);
		RB_SetTextureGen(GL_R, 0);
		RB_SetTextureGen(GL_Q, 0);
		break;
	}

	// tcGen
	switch (pass->tcGen)
	{
	case TC_GEN_NONE:
		return true;

	case TC_GEN_BASE:
		// If there's no array, just point to something so it doesn't crash
		if (rb.inCoords)
			glTexCoordPointer(2, GL_FLOAT, 0, rb.inCoords);
		else
			glTexCoordPointer(2, GL_FLOAT, 0, rb.outCoordArray[texUnit][0]);
		return true;

	case TC_GEN_LIGHTMAP:
		// If there's no array, just point to something so it doesn't crash
		if (rb.inLMCoords)
			glTexCoordPointer(2, GL_FLOAT, 0, rb.inLMCoords);
		else
			glTexCoordPointer(2, GL_FLOAT, 0, rb.outCoordArray[texUnit][0]);
		return true;

	case TC_GEN_ENVIRONMENT:
		{
			if (RB_In2DMode())
				return true;

			matrix[0] = matrix[12] = -0.5f;
			matrix[5] = matrix[13] = 0.5f;

			vec3_t transform;
			vec3_t projection;

			Vec3Subtract(ri.def.viewOrigin, rb.curEntity->origin, projection);
			Matrix3_TransformVector(rb.curEntity->axis, projection, transform);

			for (int i=0 ; i<rb.numVerts ; i++)
			{
				Vec3Subtract(transform, rb.inVertices[i], projection);
				VectorNormalizeFastf(projection);

				float depth = DotProduct(rb.inNormals[i], projection);
				depth += depth;

				rb.outCoordArray[texUnit][i][0] = 0.5 + (rb.inNormals[i][1] * depth - projection[1]) * 0.5;
				rb.outCoordArray[texUnit][i][1] = 0.5 - (rb.inNormals[i][2] * depth - projection[2]) * 0.5;
			}

			glTexCoordPointer(2, GL_FLOAT, 0, rb.outCoordArray[texUnit][0]);
		}
		return false;

	case TC_GEN_VECTOR:
		{
			GLfloat genVector[2][4];

			for (int i=0 ; i<3 ; i++)
			{
				genVector[0][i] = pass->tcGenVec[0][i];
				genVector[1][i] = pass->tcGenVec[1][i];
			}
			genVector[0][3] = genVector[1][3] = 0;

			matrix[12] = pass->tcGenVec[0][3];
			matrix[13] = pass->tcGenVec[1][3];

			RB_SetTexCoordArrayMode(0);
			RB_SetTextureGen(GL_S, GL_OBJECT_LINEAR);
			RB_SetTextureGen(GL_T, GL_OBJECT_LINEAR);
			RB_SetTextureGen(GL_R, 0);
			RB_SetTextureGen(GL_Q, 0);
			glTexGenfv(GL_S, GL_OBJECT_PLANE, genVector[0]);
			glTexGenfv(GL_T, GL_OBJECT_PLANE, genVector[1]);
			return false;
		}

	case TC_GEN_REFLECTION:
		RB_SetTextureGen(GL_S, GL_REFLECTION_MAP_ARB);
		RB_SetTextureGen(GL_T, GL_REFLECTION_MAP_ARB);
		RB_SetTextureGen(GL_R, GL_REFLECTION_MAP_ARB);
		RB_SetTextureGen(GL_Q, 0);
		return true;

	case TC_GEN_WARP:
		for (int i=0 ; i<rb.numVerts ; i++)
		{
			rb.outCoordArray[texUnit][i][0] = rb.inCoords[i][0] + (r_warpSinTable[Q_ftol (((rb.inCoords[i][1]*8.0f + rb_matTime) * (256.0f / (M_PI * 2.0f)))) & 255] * (1.0/64));
			rb.outCoordArray[texUnit][i][1] = rb.inCoords[i][1] + (r_warpSinTable[Q_ftol (((rb.inCoords[i][0]*8.0f + rb_matTime) * (256.0f / (M_PI * 2.0f)))) & 255] * (1.0/64));
		}

		glTexCoordPointer(2, GL_FLOAT, 0, rb.outCoordArray[texUnit][0]);
		return true;

	case TC_GEN_DLIGHT:
		return true;

	case TC_GEN_SHADOWMAP:
		return true;

	case TC_GEN_FOG:
		{
			plane_t *fogPlane = rb.curTexFog->visiblePlane;
			refMaterial_t *fogMat = rb.curTexFog->mat;

			matrix[0] = matrix[5] = fogMat->fogDist;
			matrix[13] = 1.5f/(float)FOGTEX_HEIGHT;

			// Distance to fog
			double dist = PlaneDiff(ri.def.viewOrigin, fogPlane);

			vec3_t viewToFog;
			if (rb.curMat->flags & MAT_SKY)
			{
				if (dist > 0)
					Vec3MA (ri.def.viewOrigin, -dist, fogPlane->normal, viewToFog);
				else
					Vec3Copy (ri.def.viewOrigin, viewToFog);
			}
			else
			{
				Vec3Copy (rb.curEntity->origin, viewToFog);
			}

			// Fog settings
			double fogNormal[3];
			fogNormal[0] = DotProduct (rb.curEntity->axis[0], fogPlane->normal) * rb.curEntity->scale;
			fogNormal[1] = DotProduct (rb.curEntity->axis[1], fogPlane->normal) * rb.curEntity->scale;
			fogNormal[2] = DotProduct (rb.curEntity->axis[2], fogPlane->normal) * rb.curEntity->scale;
			int fogPtype = (fogNormal[0] == 1.0 ? PLANE_X : (fogNormal[1] == 1.0 ? PLANE_Y : (fogNormal[2] == 1.0 ? PLANE_Z : PLANE_NON_AXIAL)));
			double fogDist = (fogPlane->dist - DotProduct (viewToFog, fogPlane->normal));

			// Forward view normal/distance
			double vpnNormal[3];
			vpnNormal[0] = DotProduct (rb.curEntity->axis[0], ri.def.viewAxis[0]) * rb.curEntity->scale;
			vpnNormal[1] = DotProduct (rb.curEntity->axis[1], ri.def.viewAxis[0]) * rb.curEntity->scale;
			vpnNormal[2] = DotProduct (rb.curEntity->axis[2], ri.def.viewAxis[0]) * rb.curEntity->scale;
			double vpnDist = ((ri.def.viewOrigin[0] - viewToFog[0]) * ri.def.viewAxis[0][0]
						+ (ri.def.viewOrigin[1] - viewToFog[1]) * ri.def.viewAxis[0][1]
						+ (ri.def.viewOrigin[2] - viewToFog[2]) * ri.def.viewAxis[0][2]);

			float *outCoords = rb.outCoordArray[texUnit][0];
			if (dist < 0)
			{
				// Camera is inside the fog brush
				if (fogPtype < 3)
				{
					for (int i=0 ; i<rb.numVerts ; i++, outCoords+=2)
					{
						outCoords[0] = DotProduct (rb.inVertices[i], vpnNormal) - vpnDist;
						outCoords[1] = -(rb.inVertices[i][fogPtype] - fogDist);
					}
				}
				else
				{
					for (int i=0 ; i<rb.numVerts ; i++, outCoords+=2)
					{
						outCoords[0] = DotProduct (rb.inVertices[i], vpnNormal) - vpnDist;
						outCoords[1] = -(DotProduct (rb.inVertices[i], fogNormal) - fogDist);
					}
				}
			}
			else
			{
				if (fogPtype < 3)
				{
					for (int i=0 ; i<rb.numVerts ; i++, outCoords+=2)
					{
						double vdist = rb.inVertices[i][fogPtype] - fogDist;
						outCoords[0] = ((vdist < 0) ? (DotProduct (rb.inVertices[i], vpnNormal) - vpnDist) * vdist / (vdist - dist) : 0.0f);
						outCoords[1] = -vdist;
					}
				}
				else
				{
					for (int i=0 ; i<rb.numVerts ; i++, outCoords+=2)
					{
						double vdist = DotProduct (rb.inVertices[i], fogNormal) - fogDist;
						outCoords[0] = ((vdist < 0) ? (DotProduct (rb.inVertices[i], vpnNormal) - vpnDist) * vdist / (vdist - dist) : 0.0f);
						outCoords[1] = -vdist;
					}
				}
			}

			glTexCoordPointer(2, GL_FLOAT, 0, rb.outCoordArray[texUnit][0]);
			return false;
		}
	}

	// Should never reach here...
	assert(0);
	return true;
}
static void RB_ModifyTextureCoords(matPass_t *pass, ETextureUnit texUnit)
{
	// Texture coordinate base
	mat4x4_t result;
	bool bIdentityMatrix = RB_VertexTCBase(pass, texUnit, result);

	// Texture coordinate modifications
	glMatrixMode(GL_TEXTURE);

	if (pass->numTCMods)
	{
		bIdentityMatrix = false;
		RB_ApplyTCMods(pass, result);
	}

	if (pass->tcGen == TC_GEN_REFLECTION)
	{
		mat4x4_t m1, m2;

		Matrix4_Transpose(ri.scn.modelViewMatrix, m1);
		Matrix4_Copy(result, m2);
		Matrix4_Multiply(m2, m1, result);
		RB_LoadTexMatrix(result);
		return;
	}

	// Load identity
	if (bIdentityMatrix)
		RB_LoadIdentityTexMatrix();
	else
		RB_LoadTexMatrix(result);
}


/*
=============
RB_DeformVertices
=============
*/
static void RB_DeformVertices()
{
	for (int deformNum=0 ; deformNum<rb.curMat->numDeforms ; deformNum++)
	{
		vertDeform_t *vertDeform = &rb.curMat->deforms[deformNum];
		switch (vertDeform->type)
		{
		case DEFORMV_WAVE:
			{
				float *table = RB_TableForFunc(vertDeform->func.type);
				float now = vertDeform->func.args[2] + vertDeform->func.args[3] * rb_matTime;

				for (int j=0 ; j<rb.numVerts ; j++)
				{
					float deflect = (rb.inVertices[j][0] + rb.inVertices[j][1] + rb.inVertices[j][2]) * vertDeform->args[0] + now;
					deflect = FTABLE_EVALUATE (table, deflect) * vertDeform->func.args[1] + vertDeform->func.args[0];

					// Deflect vertex along its normal by wave amount
					Vec3MA (rb.inVertices[j], deflect, rb.inNormals[j], rb.inVertices[j]);
				}
			}
			break;

		case DEFORMV_NORMAL:
			{
				vec2_t args;
				args[0] = vertDeform->args[1] * rb_matTime;

				for (int j=0 ; j<rb.numVerts ; j++)
				{
					args[1] = rb.inNormals[j][2] * args[0];

					rb.inNormals[j][0] *= vertDeform->args[0] * RB_FastSin(args[1]);
					rb.inNormals[j][1] *= vertDeform->args[0] * RB_FastSin(args[1] + 0.25);

					VectorNormalizeFastf(rb.inNormals[j]);
				}
			}
			break;

		case DEFORMV_BULGE:
			{
				vec3_t args;
				args[0] = vertDeform->args[0] / (float)rb.curPatchHeight;
				args[1] = vertDeform->args[1];
				args[2] = rb_matTime / (vertDeform->args[2] * rb.curPatchWidth);

				for (uint32 l=0, p=0 ; l<rb.curPatchHeight ; l++)
				{
					float deflect = RB_FastSin ((float)l * args[0] + args[2]) * args[1];
					for (uint32 m=0 ; m<rb.curPatchWidth ; m++, p++)
						Vec3MA (rb.inVertices[p], deflect, rb.inNormals[p], rb.inVertices[p]);
				}
			}
			break;

		case DEFORMV_MOVE:
			{
				float *table = RB_TableForFunc (vertDeform->func.type);
				float deflect = vertDeform->func.args[2] + rb_matTime * vertDeform->func.args[3];
				deflect = FTABLE_EVALUATE(table, deflect) * vertDeform->func.args[1] + vertDeform->func.args[0];

				for (int j=0 ; j<rb.numVerts ; j++)
					Vec3MA (rb.inVertices[j], deflect, vertDeform->args, rb.inVertices[j]);
			}
			break;

		case DEFORMV_AUTOSPRITE:
			{
				mat3x3_t m0, m1, m2, result;
				vec3_t tv, rot_centre;
				float *quadIn[4], *quadOut[4];

				if (rb.numIndexes % 6)
					break;

				if (rb.curEntity == ri.scn.worldEntity)
					Matrix4_Matrix3 (ri.scn.worldViewMatrix, m1);
				else
					Matrix4_Matrix3 (ri.scn.modelViewMatrix, m1);

				Matrix3_Transpose (m1, m2);

				for (int k=0 ; k<rb.numIndexes ; k+=6)
				{
					quadIn[0] = (float *)(rb.inVertices + rb.inIndices[k+0]);
					quadIn[1] = (float *)(rb.inVertices + rb.inIndices[k+1]);
					quadIn[2] = (float *)(rb.inVertices + rb.inIndices[k+2]);

					quadOut[0] = (float *)(rb.inVertices + rb.inIndices[k+0]);
					quadOut[1] = (float *)(rb.inVertices + rb.inIndices[k+1]);
					quadOut[2] = (float *)(rb.inVertices + rb.inIndices[k+2]);

					for (int j=2 ; j>=0 ; j--)
					{
						quadIn[3] = (float *)(rb.inVertices + rb.inIndices[k+3+j]);
						if (!Vec3Compare (quadIn[3], quadIn[0])
						&& !Vec3Compare (quadIn[3], quadIn[1])
						&& !Vec3Compare (quadIn[3], quadIn[2]))
						{
							quadOut[3] = (float *)(rb.inVertices + rb.inIndices[k+3+j]);
							break;
						}
					}

					Matrix3_FromPoints (quadIn[0], quadIn[1], quadIn[2], m0);

					// Swap m0[0] an m0[1] - FIXME?
					Vec3Copy (m0[1], rot_centre);
					Vec3Copy (m0[0], m0[1]);
					Vec3Copy (rot_centre, m0[0]);

					Matrix3_Multiply (m2, m0, result);

					rot_centre[0] = (quadIn[0][0] + quadIn[1][0] + quadIn[2][0] + quadIn[3][0]) * 0.25f;
					rot_centre[1] = (quadIn[0][1] + quadIn[1][1] + quadIn[2][1] + quadIn[3][1]) * 0.25f;
					rot_centre[2] = (quadIn[0][2] + quadIn[1][2] + quadIn[2][2] + quadIn[3][2]) * 0.25f;

					for (int j=0 ; j<4 ; j++)
					{
						Vec3Subtract (quadIn[j], rot_centre, tv);
						Matrix3_TransformVector (result, tv, quadOut[j]);
						Vec3Add (rot_centre, quadOut[j], quadOut[j]);
					}
				}
			}
			break;

		case DEFORMV_AUTOSPRITE2:
			{
				vec3_t tv, rot_centre;
				float *quadIn[4], *quadOut[4];

				if (rb.numIndexes % 6)
					break;

				for (int k=0 ; k<rb.numIndexes ; k+=6)
				{
					int			long_axis, short_axis;
					vec3_t		axis, tmp, len;
					mat3x3_t	m0, m1, m2, result;

					quadIn[0] = (float *)(rb.inVertices + rb.inIndices[k+0]);
					quadIn[1] = (float *)(rb.inVertices + rb.inIndices[k+1]);
					quadIn[2] = (float *)(rb.inVertices + rb.inIndices[k+2]);

					quadOut[0] = (float *)(rb.inVertices + rb.inIndices[k+0]);
					quadOut[1] = (float *)(rb.inVertices + rb.inIndices[k+1]);
					quadOut[2] = (float *)(rb.inVertices + rb.inIndices[k+2]);

					for (int j=2 ; j>=0 ; j--)
					{
						quadIn[3] = (float *)(rb.inVertices + rb.inIndices[k+3+j]);

						if (!Vec3Compare (quadIn[3], quadIn[0])
						&& !Vec3Compare (quadIn[3], quadIn[1])
						&& !Vec3Compare (quadIn[3], quadIn[2]))
						{
							quadOut[3] = (float *)(rb.inVertices + rb.inIndices[k+3+j]);
							break;
						}
					}

					// Build a matrix were the longest axis of the billboard is the Y-Axis
					Vec3Subtract (quadIn[1], quadIn[0], m0[0]);
					Vec3Subtract (quadIn[2], quadIn[0], m0[1]);
					Vec3Subtract (quadIn[2], quadIn[1], m0[2]);
					len[0] = DotProduct (m0[0], m0[0]);
					len[1] = DotProduct (m0[1], m0[1]);
					len[2] = DotProduct (m0[2], m0[2]);

					if (len[2] > len[1] && len[2] > len[0])
					{
						if (len[1] > len[0])
						{
							long_axis = 1;
							short_axis = 0;
						}
						else
						{
							long_axis = 0;
							short_axis = 1;
						}
					}
					else if (len[1] > len[2] && len[1] > len[0])
					{
						if (len[2] > len[0])
						{
							long_axis = 2;
							short_axis = 0;
						}
						else
						{
							long_axis = 0;
							short_axis = 2;
						}
					}
					else if (len[0] > len[1] && len[0] > len[2])
					{
						if (len[2] > len[1])
						{
							long_axis = 2;
							short_axis = 1;
						}
						else
						{
							long_axis = 1;
							short_axis = 2;
						}
					}
					else
					{
						// FIXME
						long_axis = 0;
						short_axis = 0;
					}

					if (!len[long_axis])
						break;
					len[long_axis] = Q_RSqrtf (len[long_axis]);
					Vec3Scale (m0[long_axis], len[long_axis], axis);

					if (DotProduct (m0[long_axis], m0[short_axis]))
					{
						Vec3Copy (axis, m0[1]);
						if (axis[0] || axis[1])
							MakeNormalVectorsf (m0[1], m0[0], m0[2]);
						else
							MakeNormalVectorsf (m0[1], m0[2], m0[0]);
					}
					else
					{
						if (!len[short_axis])
							break;
						len[short_axis] = Q_RSqrtf (len[short_axis]);
						Vec3Scale (m0[short_axis], len[short_axis], m0[0]);
						Vec3Copy (axis, m0[1]);
						CrossProduct (m0[0], m0[1], m0[2]);
					}

					rot_centre[0] = (quadIn[0][0] + quadIn[1][0] + quadIn[2][0] + quadIn[3][0]) * 0.25f;
					rot_centre[1] = (quadIn[0][1] + quadIn[1][1] + quadIn[2][1] + quadIn[3][1]) * 0.25f;
					rot_centre[2] = (quadIn[0][2] + quadIn[1][2] + quadIn[2][2] + quadIn[3][2]) * 0.25f;

					if (rb.curEntity == ri.scn.worldEntity)
					{
						Vec3Copy (rot_centre, tv);
						Vec3Subtract (ri.def.viewOrigin, tv, tv);
					}
					else
					{
						Vec3Add (rb.curEntity->origin, rot_centre, tv);
						Vec3Subtract (ri.def.viewOrigin, tv, tmp);
						Matrix3_TransformVector (rb.curEntity->axis, tmp, tv);
					}

					// Filter any longest-axis-parts off the camera-direction
					float deflect = -DotProduct (tv, axis);

					Vec3MA (tv, deflect, axis, m1[2]);
					VectorNormalizeFastf (m1[2]);
					Vec3Copy (axis, m1[1]);
					CrossProduct (m1[1], m1[2], m1[0]);

					Matrix3_Transpose (m1, m2);
					Matrix3_Multiply (m2, m0, result);

					for (int j=0 ; j<4 ; j++)
					{
						Vec3Subtract (quadIn[j], rot_centre, tv);
						Matrix3_TransformVector (result, tv, quadOut[j]);
						Vec3Add (rot_centre, quadOut[j], quadOut[j]);
					}
				}
			}
			break;

		case DEFORMV_PROJECTION_SHADOW:
			{
				// FIXME: better deform
				float LowZ;

				LowZ = 999999;
				for (int j=0 ; j<rb.numVerts ; j++)
				{
					if (rb.inVertices[j][2] < LowZ)
						LowZ = rb.inVertices[j][2];
				}

				for (int j=0 ; j<rb.numVerts ; j++)
				{
					rb.inVertices[j][2] = LowZ;
				}
			}
			break;

		case DEFORMV_AUTOPARTICLE:
			{
				mat3x3_t m0, m1, m2, result;
				float scale;
				vec3_t tv, rot_centre;
				float *quadIn[4], *quadOut[4];

				if (rb.numIndexes % 6)
					break;

				if (rb.curEntity == ri.scn.worldEntity)
					Matrix4_Matrix3 (ri.scn.worldViewMatrix, m1);
				else
					Matrix4_Matrix3 (ri.scn.modelViewMatrix, m1);

				Matrix3_Transpose (m1, m2);

				for (int k=0 ; k<rb.numIndexes ; k+=6)
				{
					quadIn[0] = (float *)(rb.inVertices + rb.inIndices[k+0]);
					quadIn[1] = (float *)(rb.inVertices + rb.inIndices[k+1]);
					quadIn[2] = (float *)(rb.inVertices + rb.inIndices[k+2]);

					quadOut[0] = (float *)(rb.inVertices + rb.inIndices[k+0]);
					quadOut[1] = (float *)(rb.inVertices + rb.inIndices[k+1]);
					quadOut[2] = (float *)(rb.inVertices + rb.inIndices[k+2]);

					for (int j=2 ; j>=0 ; j--)
					{
						quadIn[3] = (float *)(rb.inVertices + rb.inIndices[k+3+j]);

						if (!Vec3Compare (quadIn[3], quadIn[0])
						&& !Vec3Compare (quadIn[3], quadIn[1])
						&& !Vec3Compare (quadIn[3], quadIn[2]))
						{
							quadOut[3] = (float *)(rb.inVertices + rb.inIndices[k+3+j]);
							break;
						}
					}

					Matrix3_FromPoints (quadIn[0], quadIn[1], quadIn[2], m0);
					Matrix3_Multiply (m2, m0, result);

					// Hack a scale up to keep particles from disappearing
					scale = (quadIn[0][0] - ri.def.viewOrigin[0]) * ri.def.viewAxis[0][0] +
							(quadIn[0][1] - ri.def.viewOrigin[1]) * ri.def.viewAxis[0][1] +
							(quadIn[0][2] - ri.def.viewOrigin[2]) * ri.def.viewAxis[0][2];
					if (scale < 20)
						scale = 1.5;
					else
						scale = 1.5 + scale * 0.006f;

					rot_centre[0] = (quadIn[0][0] + quadIn[1][0] + quadIn[2][0] + quadIn[3][0]) * 0.25f;
					rot_centre[1] = (quadIn[0][1] + quadIn[1][1] + quadIn[2][1] + quadIn[3][1]) * 0.25f;
					rot_centre[2] = (quadIn[0][2] + quadIn[1][2] + quadIn[2][2] + quadIn[3][2]) * 0.25f;

					for (int j=0 ; j<4 ; j++)
					{
						Vec3Subtract (quadIn[j], rot_centre, tv);
						Matrix3_TransformVector (result, tv, quadOut[j]);
						Vec3MA (rot_centre, scale, quadOut[j], quadOut[j]);
					}
				}
			}
			break;

		case DEFORMV_NONE:
		default:
			assert (0);
			break;
		}
	}
}

/*
===============================================================================

	MESH RENDERING

===============================================================================
*/

/*
=============
RB_DrawElements
=============
*/
static void RB_DrawElements()
{
	if (!rb.numVerts || !rb.numIndexes)
		return;

	// Flush
	if (ri.config.ext.bDrawRangeElements)
		qglDrawRangeElementsEXT(GL_TRIANGLES, 0, rb.numVerts, rb.numIndexes, GL_UNSIGNED_INT, rb.inIndices);
	else
		glDrawElements(GL_TRIANGLES, rb.numIndexes, GL_UNSIGNED_INT, rb.inIndices);

	// Increment performance counters
	if (ri.scn.bDrawingMeshOutlines || !r_speeds->intVal)
		return;

	ri.pc.numVerts += rb.numVerts;
	ri.pc.numTris += rb.numIndexes/3;
	ri.pc.numElements++;

	switch (rb.curMeshType)
	{
	case MBT_Q2BSP:
	case MBT_Q3BSP:
		ri.pc.worldElements++;
		break;

	case MBT_ALIAS:
		ri.pc.aliasElements++;
		ri.pc.aliasPolys += rb.numIndexes/3;
		break;

	case MBT_DECAL:
		ri.pc.decalElements++;
		ri.pc.decalPolys += rb.numIndexes/3;
		break;

	case MBT_POLY:
		ri.pc.polyElements++;
		ri.pc.polyPolys += rb.numIndexes/3;
		break;
	}
}


/*
=============
RB_CleanUpTextureUnits

This cleans up the texture units not planned to be used in the next render.
=============
*/
static void RB_CleanUpTextureUnits()
{
	glMatrixMode(GL_TEXTURE);

	for (int i=rb_numOldPasses ; i>rb_numPasses ; i--)
	{
		RB_SelectTexture((ETextureUnit)(i-1));
		RB_TextureTarget(0);
		RB_SetTextureGen(GL_S, 0);
		RB_SetTextureGen(GL_T, 0);
		RB_SetTextureGen(GL_R, 0);
		RB_SetTextureGen(GL_Q, 0);
		RB_SetTexCoordArrayMode(0);
		RB_LoadIdentityTexMatrix();
	}

	glMatrixMode(GL_MODELVIEW);

	rb_numOldPasses = rb_numPasses;
}


/*
=============
RB_BindMaterialPass
=============
*/
static void RB_BindMaterialPass(matPass_t *pass, refImage_t *image, const ETextureUnit texUnit)
{
	RB_SelectTexture(texUnit);

	if (pass->flags & MAT_PASS_NOTEXTURING)
	{
		// Special internal case
		RB_TextureTarget(0);
		RB_SetTexCoordArrayMode(0);
	}
	else
	{
		if (!image)
		{
			// Find the texture
			if (pass->flags & MAT_PASS_LIGHTMAP)
			{
				if (rb.curLMTexNum == BAD_LMTEXNUM)
				{
					image = ri.media.whiteTexture;
				}
				else
				{
					if (rb.curLMTexNum == r_debugLightmapIndex->intVal)
						image = ri.media.blackTexture;
					else
						image = ri.media.lmTextures[rb.curLMTexNum];
				}
			}
			else if (pass->flags & MAT_PASS_ANIMMAP)
			{
				image = pass->animImages[(int)(pass->animFPS * rb_matTime) % pass->animNumImages];
			}
			else
			{
				image = pass->animImages[0];
			}

			if (!image)
				image = ri.media.noTexture;
		}

		if (gl_nobind->intVal)
			image = ri.media.noTexture;

		// Bind the texture
		RB_BindTexture(image);
		RB_TextureTarget(image->target);
		RB_SetTexCoordArrayMode( (image->flags & IT_CUBEMAP) ? GL_TEXTURE_CUBE_MAP_ARB : GL_TEXTURE_COORD_ARRAY );
	}

	// Modify the texture coordinates
	RB_ModifyTextureCoords(pass, texUnit);
}


/*
=============
RB_AddProgramParms
=============
*/
static void RB_AddProgramParms(const GLenum type, matPass_t *pass, progParm_t *source, const int numParms)
{
	for (int i=0 ; i<numParms ; i++)
	{
		switch (source[i].types[0])
		{
		case PPTYPE_VIEW_ORIGIN:
			qglProgramLocalParameter4fARB(type, source[i].index, ri.def.viewOrigin[0], ri.def.viewOrigin[1], ri.def.viewOrigin[2], 0);
			continue;
		case PPTYPE_VIEW_ANGLES:
			qglProgramLocalParameter4fARB(type, source[i].index, ri.def.viewAngles[0], ri.def.viewAngles[1], ri.def.viewAngles[2], 0);
			continue;
		case PPTYPE_VIEW_AXIS_FWD:
			qglProgramLocalParameter4fARB(type, source[i].index, ri.def.viewAxis[0][0], ri.def.viewAxis[0][1], ri.def.viewAxis[0][2], 0);
			continue;
		case PPTYPE_VIEW_AXIS_LEFT:
			qglProgramLocalParameter4fARB(type, source[i].index, ri.def.viewAxis[1][0], ri.def.viewAxis[1][1], ri.def.viewAxis[1][2], 0);
			continue;
		case PPTYPE_VIEW_AXIS_UP:
			qglProgramLocalParameter4fARB(type, source[i].index, ri.def.viewAxis[2][0], ri.def.viewAxis[2][1], ri.def.viewAxis[2][2], 0);
			continue;
		case PPTYPE_ENTITY_ORIGIN:
			qglProgramLocalParameter4fARB(type, source[i].index, rb.curEntity->origin[0], rb.curEntity->origin[1], rb.curEntity->origin[2], 0);
			continue;
		case PPTYPE_ENTITY_ANGLES:
			{
				vec3_t temp;
				Matrix3_Angles(rb.curEntity->axis, temp);
				qglProgramLocalParameter4fARB(type, source[i].index, temp[0], temp[1], temp[2], 0);
			}
			continue;
		case PPTYPE_ENTITY_AXIS_FWD:
			qglProgramLocalParameter4fARB(type, source[i].index, rb.curEntity->axis[0][0], rb.curEntity->axis[0][1], rb.curEntity->axis[0][2], 0);
			continue;
		case PPTYPE_ENTITY_AXIS_LEFT:
			qglProgramLocalParameter4fARB(type, source[i].index, rb.curEntity->axis[1][0], rb.curEntity->axis[1][1], rb.curEntity->axis[1][2], 0);
			continue;
		case PPTYPE_ENTITY_AXIS_UP:
			qglProgramLocalParameter4fARB(type, source[i].index, rb.curEntity->axis[2][0], rb.curEntity->axis[2][1], rb.curEntity->axis[2][2], 0);
			continue;
		}

		vec4_t outValues;
		Vec4Clear(outValues);
		for (int j=0 ; j<4 ; j++)
		{
			switch (source[i].types[j])
			{
			case PPTYPE_CONST:
				outValues[j] = source[i].values[j];
				break;
			case PPTYPE_TIME:
				outValues[j] = rb_matTime;
				break;
			default:
				assert(0);
				break;
			}
		}

		qglProgramLocalParameter4fvARB(type, source[i].index, outValues);
	}
}


/*
=============
RB_SetupPassState
=============
*/
static void RB_SetupPassState(matPass_t *pass, const stateBit_t addSB = 0)
{
	stateBit_t passBits = rb_stateBits|pass->stateBits|addSB;

	// Force blending for RF_TRANSLUCENT
	if (rb.curEntity->flags & RF_TRANSLUCENT)
	{
		if (!(pass->flags & MAT_PASS_BLEND))
		{
			passBits &= ~(SB_BLENDSRC_MASK|SB_BLENDDST_MASK);
			passBits |= SB_BLENDSRC_SRC_ALPHA|SB_BLENDDST_ONE_MINUS_SRC_ALPHA;
		}
	}
	// Depth masking
	else if (pass->flags & MAT_PASS_DEPTHWRITE)
		passBits |= SB_DEPTHMASK_ON;

	// Use flat shading on solidly colored objects
	if (pass->flags & MAT_PASS_NOCOLORARRAY && !rb.curColorFog)
		passBits |= SB_SHADEMODEL_FLAT;

	// Commit
	RB_StateForBits(passBits);
}


/*
=================
RB_RenderDLightAttenuation

Special case for rendering Q3BSP dynamic light attenuation.
=================
*/
static void RB_RenderDLightAttenuation()
{
	GLfloat s[4], t[4], r[4];

	// Set state
	matPass_t *pass = rb_accumPasses[0];
	RB_BindMaterialPass(pass, ri.media.dLightTexture, TEXUNIT0);
	RB_SetupPassState(pass);
	RB_TextureEnv(GL_MODULATE);

	// Texture state
	if (ri.config.ext.bTex3D)
	{
		RB_SetTexCoordArrayMode(0);

		RB_SetTextureGen(GL_S, GL_OBJECT_LINEAR);
		s[1] = s[2] = 0;

		RB_SetTextureGen(GL_T, GL_OBJECT_LINEAR);
		t[0] = t[2] = 0;

		RB_SetTextureGen(GL_R, GL_OBJECT_LINEAR);
		r[0] = r[1] = 0;

		RB_SetTextureGen(GL_Q, 0);

		RB_SetColor(NULL);
	}
	else
	{
		RB_SetTextureGen(GL_S, GL_OBJECT_LINEAR);
		s[1] = s[2] = 0;

		RB_SetTextureGen(GL_T, GL_OBJECT_LINEAR);
		t[0] = t[2] = 0;

		RB_SetTextureGen(GL_R, 0);
		RB_SetTextureGen(GL_Q, 0);

		RB_SetColor(NULL);
	}

	for (uint32 num=0 ; num<ri.scn.numDLights ; num++)
	{
		if (!(rb.curDLightBits & BIT(num)))
			continue; // Not lit by this light

		if (ri.scn.dLightCullBits & BIT(num))
			continue;

		refDLight_t *light = &ri.scn.dLightList[num];

		// Transform
		vec3_t lightOrigin;
		Vec3Subtract(light->origin, rb.curEntity->origin, lightOrigin);
		if (!Matrix3_Compare (rb.curEntity->axis, axisIdentity))
		{
			vec3_t tempVec;
			Vec3Copy(lightOrigin, tempVec);
			Matrix3_TransformVector(rb.curEntity->axis, tempVec, lightOrigin);
		}

		float scale = 1.0f / light->intensity;

		// Calculate coordinates
		s[0] = scale;
		s[3] = (-lightOrigin[0] * scale) + 0.5f;
		glTexGenfv(GL_S, GL_OBJECT_PLANE, s);

		t[1] = scale;
		t[3] = (-lightOrigin[1] * scale) + 0.5f;
		glTexGenfv(GL_T, GL_OBJECT_PLANE, t);

		// Color
		glColor3fv(light->color);

		if (ri.config.ext.bTex3D)
		{
			// Depth coordinate
			r[2] = scale;
			r[3] = (-lightOrigin[2] * scale) + 0.5f;
			glTexGenfv(GL_R, GL_OBJECT_PLANE, r);
		}

		// Render
		RB_DrawElements();
	}
}


/*
=================
RB_RenderShadowAttenuation

Special case for rendering shadow map attenuation.
=================
*/
static void RB_RenderShadowAttenuation()
{
}


/*
=============
RB_RenderProgrammed
=============
*/
static void RB_RenderProgrammed()
{
	matPass_t *pass = rb_accumPasses[0];
	stateBit_t addBits = 0;

	RB_BindMaterialPass(pass, NULL, TEXUNIT0);
	RB_SetupColor(pass);

	if (pass->flags & MAT_PASS_VERTEXPROGRAM)
	{
		addBits |= SB_VERTEX_PROGRAM_ON;
		RB_BindProgram(pass->vertProgPtr);
		RB_AddProgramParms(GL_VERTEX_PROGRAM_ARB, pass, pass->vertParms, pass->numVertParms);
	}

	if (pass->flags & MAT_PASS_FRAGMENTPROGRAM)
	{
		addBits |= SB_FRAGMENT_PROGRAM_ON;
		RB_BindProgram(pass->fragProgPtr);
		RB_AddProgramParms(GL_FRAGMENT_PROGRAM_ARB, pass, pass->fragParms, pass->numFragParms);
	}

	RB_SetupPassState(pass, addBits);
	RB_TextureEnv((pass->blendMode == GL_REPLACE) ? GL_REPLACE : GL_MODULATE);

	RB_DrawElements();
}


/*
=============
RB_RenderShadered
=============
*/
static void RB_RenderShadered()
{
	matPass_t *pass = rb_accumPasses[0];

	RB_BindMaterialPass(pass, NULL, TEXUNIT0);
	RB_SetupColor(pass);
	RB_SetupPassState(pass);
	RB_TextureEnv((pass->blendMode == GL_REPLACE) ? GL_REPLACE : GL_MODULATE);

	qglUseProgramObjectARB(pass->shaderPtr->objectHandle);

	RB_DrawElements();

	qglUseProgramObjectARB(0);
}


/*
=============
RB_RenderGeneric
=============
*/
static void RB_RenderGeneric()
{
	matPass_t *pass = rb_accumPasses[0];

	RB_BindMaterialPass(pass, NULL, TEXUNIT0);
	RB_SetupColor(pass);
	RB_SetupPassState(pass);
	RB_TextureEnv((pass->blendMode == GL_REPLACE) ? GL_REPLACE : GL_MODULATE);

	RB_DrawElements();
}


/*
=============
RB_RenderCombine
=============
*/
static void RB_RenderCombine()
{
	matPass_t *pass = rb_accumPasses[0];

	RB_BindMaterialPass(pass, NULL, TEXUNIT0);
	RB_SetupColor(pass);
	RB_SetupPassState(pass, SB_BLEND_MTEX);
	RB_TextureEnv(GL_MODULATE);

	for (int i=1 ; i<rb_numPasses ; i++)
	{
		pass = rb_accumPasses[i];
		RB_BindMaterialPass(pass, NULL, (ETextureUnit)i);

		switch (pass->blendMode)
		{
		case GL_REPLACE:
		case GL_MODULATE:
			RB_TextureEnv(GL_MODULATE);
			break;

		case GL_ADD:
			// These modes are best set with TexEnv, Combine4 would need much more setup
			RB_TextureEnv(GL_ADD);
			break;

		case GL_DECAL:
			// Mimics Alpha-Blending in upper texture stage, but instead of multiplying the alpha-channel, they´re added
			// this way it can be possible to use GL_DECAL in both texture-units, while still looking good
			// normal mutlitexturing would multiply the alpha-channel which looks ugly
			RB_TextureEnv(GL_COMBINE_ARB);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_INTERPOLATE_ARB);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_ADD);

			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_ARB, GL_SRC_ALPHA);

			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_PREVIOUS_ARB);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, GL_SRC_ALPHA);

			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB_ARB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB_ARB, GL_SRC_ALPHA);
			break;

		default:
			assert(0);
			break;
		}
	}

	RB_DrawElements();
}


/*
=============
RB_RenderMTex
=============
*/
static void RB_RenderMTex()
{
	matPass_t *pass = rb_accumPasses[0];

	RB_BindMaterialPass(pass, NULL, TEXUNIT0);
	RB_SetupColor(pass);
	RB_SetupPassState(pass, SB_BLEND_MTEX);
	RB_TextureEnv(GL_MODULATE);

	for (int i=1 ; i<rb_numPasses ; i++)
	{
		pass = rb_accumPasses[i];
		RB_BindMaterialPass(pass, NULL, (ETextureUnit)i);
		RB_TextureEnv(pass->blendMode);
	}

	RB_DrawElements();
}

/*
===============================================================================

	PASS ACCUMULATION

===============================================================================
*/

/*
=============
RB_RenderAccumulatedPasses
=============
*/
static void RB_RenderAccumulatedPasses()
{
	// Clean up texture units not used in this flush
	RB_CleanUpTextureUnits();

	if (rb_numPasses == 1)
	{
		// Handle some special cases
		matPass_t *pass = rb_accumPasses[0];
		if (pass->flags & MAT_PASS_DLIGHT)
		{
			assert(rb.curDLightBits);
			RB_RenderDLightAttenuation();
		}
		else if (pass->flags & MAT_PASS_SHADOWMAP)
		{
			assert(rb.curShadowBits);
			RB_RenderShadowAttenuation();
		}
		else if (pass->flags & MAT_PASS_PROGRAM_MASK)
		{
			RB_RenderProgrammed();
		}
		else if (pass->flags & MAT_PASS_SHADER)
		{
			RB_RenderShadered();
		}
		else
		{
			RB_RenderGeneric();
		}
	}
	else if (ri.config.ext.bTexEnvCombine)
	{
		RB_RenderCombine();
	}
	else
	{
		RB_RenderMTex();
	}
	rb_numPasses = 0;
}


/*
=============
RB_AccumulatePass
=============
*/
static void RB_AccumulatePass(matPass_t *pass)
{
	// Only accumulate depth in the shadow scenes
	if (ri.scn.viewType == RVT_SHADOWMAP && !(rb.curMat->flags & MAT_DEPTHWRITE))
		return;

	if (!ri.scn.bDrawingMeshOutlines)
		ri.pc.meshPasses++;

	bool bRenderNow = (pass->flags & (MAT_PASS_DLIGHT|MAT_PASS_SHADOWMAP|MAT_PASS_PROGRAM_MASK|MAT_PASS_SHADER)) ? true : false;
	bool bAccum = (rb_numPasses+1 < ri.config.maxTexUnits) && !bRenderNow;
	if (bAccum)
	{
		// First pass always accumulates
		if (!rb_numPasses)
		{
			rb_accumPasses[rb_numPasses++] = pass;
			return;
		}

		// Compare against previous pass properties
		matPass_t *prevPass = rb_accumPasses[rb_numPasses-1];
		if (prevPass->flags & (MAT_PASS_DLIGHT|MAT_PASS_SHADOWMAP|MAT_PASS_PROGRAM_MASK|MAT_PASS_SHADER)
		|| pass->stateBits & SB_ATEST_MASK
		|| pass->rgbGen.type != RGB_GEN_IDENTITY
		|| pass->alphaGen.type != ALPHA_GEN_IDENTITY
		|| (prevPass->stateBits ^ pass->stateBits) & SB_DEPTHFUNC_MASK // Depth func must match
		|| (prevPass->stateBits ^ pass->stateBits) & SB_COLORMASK_MASK // Color mask must match
		|| ((prevPass->stateBits & SB_ATEST_MASK) && !(pass->stateBits & SB_DEPTHFUNC_EQ)))
		{
			bAccum = false;
		}
		else
		{
			// Check the blend modes
			switch (prevPass->blendMode)
			{
			case GL_REPLACE:
				if (ri.config.ext.bTexEnvCombine)
					bAccum = (pass->blendMode == GL_ADD) ? ri.config.ext.bTexEnvAdd : true;
				else
					bAccum = (pass->blendMode == GL_ADD) ? ri.config.ext.bTexEnvAdd : (pass->blendMode != GL_DECAL);
				break;

			case GL_ADD:
				bAccum = (pass->blendMode == GL_ADD) && ri.config.ext.bTexEnvAdd;
				break;

			case GL_MODULATE:
				bAccum = (pass->blendMode == GL_MODULATE || pass->blendMode == GL_REPLACE);
				break;

			default:
				bAccum = false;
				break;
			}
		}
	}

	// Flush if we can't accumulate
	if (!bAccum)
	{
		if (rb_numPasses)
			RB_RenderAccumulatedPasses();
	}

	rb_accumPasses[rb_numPasses++] = pass;
	if (bRenderNow)
		RB_RenderAccumulatedPasses();
}

/*
===============================================================================

	MESH RENDERING SUPPORTING FUNCTIONS

===============================================================================
*/

/*
=============
RB_SetupMaterialState
=============
*/
static void RB_SetupMaterialState(refMaterial_t *mat)
{
	rb_stateBits = mat->stateBits;

	// Culling
	if (rb.curMeshFeatures & MF_NOCULL)
		rb_stateBits &= ~SB_CULL_MASK;

	// Flip it for lefty
	if (rb.curEntity->flags & RF_CULLHACK)
		rb_stateBits |= SB_FRONTFACE_CW;

	// The following doesn't apply to debug outlines
	if (ri.scn.bDrawingMeshOutlines)
	{
		rb_stateBits &= ~SB_POLYOFFSET_ON;
	}
	else
	{
		// Shadowmap settings
		if (ri.scn.viewType == RVT_SHADOWMAP)
			rb_stateBits |= SB_POLYOFFSET_ON;

		// Depth range
		if (rb.curEntity->flags & RF_DEPTHHACK)
			rb_stateBits |= SB_DEPTHRANGE_HACK;

		// Depth testing
		if (!(mat->flags & MAT_FLARE))
			rb_stateBits |= SB_DEPTHTEST_ON;
	}
}


/*
=============
RB_SetOutlineColor
=============
*/
static inline void RB_SetOutlineColor()
{
	switch(rb.curMeshType)
	{
	case MBT_2D:
	case MBT_ALIAS:
	case MBT_INTERNAL:
	case MBT_SP2:
	case MBT_SKY:
		glColor4ubv(Q_BColorRed);
		break;

	case MBT_Q2BSP:
	case MBT_Q3BSP:
	case MBT_Q3BSP_FLARE:
		glColor4ubv(Q_BColorWhite);
		break;

	case MBT_DECAL:
		glColor4ubv(Q_BColorYellow);
		break;

	case MBT_POLY:
		glColor4ubv(Q_BColorGreen);
		break;

	default:
		assert(0);
		break;
	}
}


/*
=============
RB_ShowTriangles
=============
*/
static inline void RB_ShowTriangles()
{
	// Set color
	switch(gl_showtris->intVal)
	{
	case 1:
		glColor4ub(255, 255, 255, 255);
		break;

	case 2:
		RB_SetOutlineColor();
		break;
	}

	// Draw
	for (int i=0 ; i<rb.numIndexes ; i+=3)
	{
		glBegin(GL_LINE_STRIP);
		glArrayElement(rb.inIndices[i]);
		glArrayElement(rb.inIndices[i+1]);
		glArrayElement(rb.inIndices[i+2]);
		glArrayElement(rb.inIndices[i]);
		glEnd();
	}
}


/*
=============
RB_ShowNormals
=============
*/
static inline void RB_ShowNormals()
{
	if (!rb.inNormals)
		return;

	// Set color
	switch (gl_shownormals->intVal)
	{
	case 1:
		glColor4ub(255, 255, 255, 255);
		break;

	case 2:
		RB_SetOutlineColor();
		break;
	}

	// Draw
	glBegin(GL_LINES);
	for (int i=0 ; i<rb.numVerts ; i++)
	{
		glVertex3f (rb.inVertices[i][0],
					rb.inVertices[i][1],
					rb.inVertices[i][2]);
		glVertex3f (rb.inVertices[i][0] + rb.inNormals[i][0]*2,
					rb.inVertices[i][1] + rb.inNormals[i][1]*2,
					rb.inVertices[i][2] + rb.inNormals[i][2]*2);
	}
	glEnd();
}

/*
===============================================================================

	MESH BUFFER RENDERING

===============================================================================
*/

/*
=============
RB_RenderMeshBuffer

This is the entry point for rendering just about everything
=============
*/
void RB_RenderMeshBuffer(refMeshBuffer *mb)
{
	if (r_skipBackend->intVal)
		return;

	if (!rb.numVerts || !rb.numIndexes)
		return;

	// Collect mesh buffer values
	rb.curMeshType = mb->DecodeMeshType();
	rb.curMat = mb->DecodeMaterial();
	rb.curEntity = mb->DecodeEntity();

	rb.curShadowBits = 0;
	rb.curDLightBits = 0;

	switch(rb.curMeshType)
	{
	case MBT_ALIAS:
		{
			
			rb.curShadowBits = rb_entShadowBits[rb.curEntity - ri.scn.entityList];
		}
		break;

	case MBT_Q2BSP:
		{
			mBspSurface_t *surf = (mBspSurface_t *)mb->mesh;

			rb.curLMTexNum = surf->lmTexNum;

			if (surf->shadowBits && surf->shadowFrame == ri.frameCount)
			{
				rb.curShadowBits = surf->shadowBits;
			}
		}
		break;

	case MBT_Q3BSP:
		{
			mBspSurface_t *surf = (mBspSurface_t *)mb->mesh;

			rb.curLMTexNum = surf->lmTexNum;
			rb.curPatchWidth = surf->q3_patchWidth;
			rb.curPatchHeight = surf->q3_patchHeight;

			if (surf->dLightBits && surf->dLightFrame == ri.frameCount
			&& ri.scn.numDLights
			&& !(rb.curMat->flags & MAT_FLARE))
			{
				rb.curDLightBits = surf->dLightBits;
			}
		}
		break;

	case MBT_BAD:
	case MBT_MAX:
		assert(0);
		break;

	default:
		break;
	}

	const bool bAddDLights = (rb.curDLightBits != 0);
	const bool bAddShadows = (rb.curShadowBits != 0);

	// Set time
	if (RB_In2DMode())
		rb_matTime = Sys_Milliseconds() * 0.001f;
	else
		rb_matTime = ri.def.time;
	rb_matTime -= mb->matTime;
	if (rb_matTime < 0)
		rb_matTime = 0;

	// State
	RB_SetupMaterialState(rb.curMat);

	// Setup vertices
	if (rb.curMat->numDeforms)
		RB_DeformVertices();

	// Unlock a previous render if necessary
	if (!(rb.curMeshFeatures & MF_KEEPLOCK))
		RB_UnlockArrays();

	// Render outlines if desired
	if (ri.scn.bDrawingMeshOutlines)
	{
		RB_LockArrays();

		if (gl_showtris->intVal)
			RB_ShowTriangles();
		if (gl_shownormals->intVal)
			RB_ShowNormals();

		RB_ResetPointers();
		return;
	}
	else if (!(rb.curMeshFeatures & MF_NONBATCHED))
	{
		ri.pc.meshBatcherFlushes++;
	}

	// Set fog materials
	mQ3BspFog_t *Fog = mb->DecodeFog();
	if (Fog && Fog->mat)
	{
		if (( (rb.curMat->sortKey <= MAT_SORT_SEETHROUGH || (rb.curMat->sortKey == MAT_SORT_ENTITY && !(rb.curEntity->flags & RF_TRANSLUCENT)))
		&& rb.curMat->flags & (MAT_DEPTHWRITE|MAT_SKY)) || rb.curMat->fogDist)
			rb.curTexFog = Fog;
		else
			rb.curColorFog = Fog;
	}

	RB_LockArrays();

	if (gl_lightmap->intVal && rb.curMat->flags & MAT_LIGHTMAPPED && rb.curLMTexNum != BAD_LMTEXNUM)
	{
		// Accumulate a lightmap pass for debugging purposes
		RB_AccumulatePass(&rb_debugLightMapPass);
		if (bAddDLights)
			RB_AccumulatePass(&rb_dLightPass);
		if (bAddShadows)
			RB_AccumulatePass(&rb_shadowPass);
	}
	else
	{
		// Accumulate passes and render
		bool bDLightsDrawn = false;
		bool bShadowsDrawn = false;
		for (int i=0 ; i<rb.curMat->numPasses ; i++)
		{
			matPass_t *pass = &rb.curMat->passes[i];

			// Detail textures
			if (r_detailTextures->intVal)
			{
				if (pass->flags & MAT_PASS_NOTDETAIL)
					continue;
			}
			else if (pass->flags & MAT_PASS_DETAIL)
				continue;

			// Must have some textures
			if (!pass->animNumImages)
				continue;

			// Accumulate
			RB_AccumulatePass(pass);

			// Forcibly accumulate a dynamic lighting and shadowing attenuation passes
			// We do this right after the lightmap intentionally, to get proper blending
			if (pass->flags & MAT_PASS_LIGHTMAP)
			{
				if (bAddDLights)
				{
					RB_AccumulatePass(&rb_dLightPass);
					bDLightsDrawn = true;
				}
				if (bAddShadows)
				{
					RB_AccumulatePass(&rb_shadowPass);
					bShadowsDrawn = true;
				}
			}
		}

		// Accumulate attenuation for dynamic lighting and shadowing if they weren't already
		if (bAddDLights && !bDLightsDrawn)
			RB_AccumulatePass(&rb_dLightPass);
		if (bAddShadows && !bShadowsDrawn)
			RB_AccumulatePass(&rb_shadowPass);

		// Accumulate fog
		if (rb.curTexFog && rb.curTexFog->mat)
		{
			rb_fogPass.animImages[0] = ri.media.fogTexture;
			rb_fogPass.stateBits &= ~SB_DEPTHFUNC_MASK;
			if (rb.curMat->numPasses && rb.curMat->fogDist && rb.curMat->flags & MAT_SKY)
				rb_fogPass.stateBits |= SB_DEPTHFUNC_EQ;
			RB_AccumulatePass(&rb_fogPass);
		}
	}

	// Make sure we've flushed
	if (rb_numPasses)
		RB_RenderAccumulatedPasses();

	// Reset the texture matrices
	glMatrixMode(GL_MODELVIEW);

	if (!ri.scn.bDrawingMeshOutlines)
	{
		// Reset backend information
		RB_ResetPointers();

		// Update performance counters
		ri.pc.meshCount++;
	}
}


/*
=============
RB_StartRendering
=============
*/
void RB_StartRendering()
{
	if (ri.scn.bDrawingMeshOutlines)
	{
		RB_StateForBits(SB_BLENDSRC_SRC_ALPHA|SB_BLENDDST_ONE_MINUS_SRC_ALPHA);
		RB_TextureTarget(0);
		RB_SetColor(NULL);
	}
}


/*
=============
RB_FinishRendering

This is called after rendering the mesh list and individual items that pass through the
rendering backend. It resets states so that anything rendered through another system
doesn't catch a leaked state.
=============
*/
void RB_FinishRendering()
{
	RB_UnlockArrays();

	glMatrixMode(GL_TEXTURE);

	for (int i=ri.config.maxTexUnits-1 ; i>0 ; i--)
	{
		RB_SelectTexture((ETextureUnit)i);
		RB_TextureTarget(0);
		RB_SetTextureGen(GL_S, 0);
		RB_SetTextureGen(GL_T, 0);
		RB_SetTextureGen(GL_R, 0);
		RB_SetTextureGen(GL_Q, 0);
		RB_SetTexCoordArrayMode(0);
		RB_LoadIdentityTexMatrix();
	}

	RB_SelectTexture(TEXUNIT0);
	RB_TextureTarget(GL_TEXTURE_2D);
	RB_SetTextureGen(GL_S, 0);
	RB_SetTextureGen(GL_T, 0);
	RB_SetTextureGen(GL_R, 0);
	RB_SetTextureGen(GL_Q, 0);
	RB_SetTexCoordArrayMode(0);
	RB_LoadIdentityTexMatrix();

	glMatrixMode(GL_MODELVIEW);

	RB_SetColor(NULL);
	RB_StateForBits(SB_DEFAULT_3D);

	RB_ResetPointers();

	rb_numOldPasses = 0;
}

/*
===============================================================================

	FRAME HANDLING

===============================================================================
*/

/*
=============
RB_BeginFrame

Does any pre-frame backend actions
=============
*/
void RB_BeginFrame()
{
	static uint32 prevUpdate;
	static uint32 interval = 300;

	// Update the noise table
	if (prevUpdate > Sys_UMilliseconds()%interval)
	{
		int j = rand()*(FTABLE_SIZE/4);
		int k = rand()*(FTABLE_SIZE/2);

		for (int i=0 ; i<FTABLE_SIZE ; i++)
		{
			if (i >= j && i < j+k)
			{
				float t = (double)((i-j)) / (double)(k);
				rb_noiseTable[i] = RB_FastSin(t + 0.25f);
			}
			else
			{
				rb_noiseTable[i] = 1;
			}
		}

		interval = 300 + rand() % 300;
	}
	prevUpdate = Sys_UMilliseconds() % interval;

	// Setup the frame for rendering
	GLimp_BeginFrame();

	// Go into 2D mode
	RB_SetupGL2D();
}


/*
=============
RB_EndFrame
=============
*/
void RB_EndFrame()
{
	// Unlock if necessary
	RB_UnlockArrays();

	// Go into 2D mode
	RB_SetupGL2D();

	// Rendering speeds
	R_DrawStats();

	// Flush 2D
	RF_Flush2D();

	// Swap buffers
	GLimp_EndFrame();
}

/*
===============================================================================

	INIT / SHUTDOWN

===============================================================================
*/

void CHECK_FRAMEBUFFER_STATUS()
{                                                           
	GLenum status;                                            
	status = qglCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
	switch(status) {                                          
	case GL_FRAMEBUFFER_COMPLETE_EXT:                       
		break;                                                
	case GL_FRAMEBUFFER_UNSUPPORTED_EXT:                    
		/* choose different formats */                        
		break;                                                
	default:                                                
		/* programming error; will fail on all hardware */    
		assert(0);                                            
	}
}

int err ()
{
	return glGetError();
}

/*
=============
RB_RenderInit
=============
*/
bool RB_RenderInit()
{
	// Set defaults
	rb_bArraysLocked = false;
	rb_bNormalsEnabled = false;
	rb_bColorArrayEnabled = false;

	RB_ResetPointers();

	// Build lookup tables
	for (int i=0 ; i<FTABLE_SIZE ; i++)
	{
		double t = (double)i / (double)FTABLE_SIZE;

		rb_sinTable[i] = sinf(t * (M_PI * 2.0f));
		rb_triangleTable[i] = (t < 0.25) ? t * 4.0f : (t < 0.75) ? 2 - 4.0f * t : (t - 0.75f) * 4.0f - 1.0f;
		rb_squareTable[i] = (t < 0.5) ? 1.0f : -1.0f;
		rb_sawtoothTable[i] = t;
		rb_inverseSawtoothTable[i] = 1.0 - t;
	}

	// Quake3 BSP specific dynamic light pass
	memset(&rb_dLightPass, 0, sizeof(matPass_t));
	rb_dLightPass.flags = MAT_PASS_DLIGHT|MAT_PASS_BLEND|MAT_PASS_NOCOLORARRAY;
	rb_dLightPass.tcGen = TC_GEN_DLIGHT;
	rb_dLightPass.blendMode = GL_REPLACE;
	rb_dLightPass.stateBits = SB_DEPTHFUNC_EQ|SB_BLENDSRC_DST_COLOR|SB_BLENDDST_ONE;

	// Shadowing pass
	memset(&rb_shadowPass, 0, sizeof(matPass_t));
	rb_shadowPass.flags = MAT_PASS_SHADOWMAP|MAT_PASS_BLEND|MAT_PASS_NOCOLORARRAY;
	rb_shadowPass.tcGen = TC_GEN_SHADOWMAP;
	rb_shadowPass.blendMode = GL_MODULATE;
	rb_shadowPass.stateBits = SB_DEPTHFUNC_EQ|SB_BLENDSRC_ZERO|SB_BLENDDST_SRC_COLOR;

	// Create the fog pass
	memset(&rb_fogPass, 0, sizeof(matPass_t));
	rb_fogPass.flags = MAT_PASS_BLEND|MAT_PASS_NOCOLORARRAY;
	rb_fogPass.tcGen = TC_GEN_FOG;
	rb_fogPass.blendMode = GL_DECAL;
	rb_fogPass.rgbGen.type = RGB_GEN_FOG;
	rb_fogPass.alphaGen.type = ALPHA_GEN_FOG;
	rb_fogPass.stateBits = SB_BLENDSRC_SRC_ALPHA|SB_BLENDDST_ONE_MINUS_SRC_ALPHA;

	// Togglable solid lightmap overlay
	memset(&rb_debugLightMapPass, 0, sizeof(matPass_t));
	rb_debugLightMapPass.flags = MAT_PASS_LIGHTMAP|MAT_PASS_NOCOLORARRAY|MAT_PASS_DEPTHWRITE;
	rb_debugLightMapPass.tcGen = TC_GEN_LIGHTMAP;
	rb_debugLightMapPass.blendMode = GL_REPLACE;
	rb_debugLightMapPass.rgbGen.type = RGB_GEN_IDENTITY;
	rb_debugLightMapPass.alphaGen.type = ALPHA_GEN_IDENTITY;
	rb_debugLightMapPass.stateBits = 0;

	if (ri.config.ext.bFrameBufferObject)
	{
		const bool bRoundDown = r_roundImagesDown->intVal ? true : false;
		GLsizei scaledWidth = Q_NearestPow<GLsizei>(ri.config.vidWidth, bRoundDown);
		GLsizei scaledHeight = Q_NearestPow<GLsizei>(ri.config.vidHeight, bRoundDown);

		ri.screenFBO = R_CreateImage("***screenspace***", null, null, scaledWidth, scaledHeight, 0, IF_NOFLUSH|IT_DEPTHANDFBO, 1);

		RB_BloomInit();
	}

	RB_CheckForError("RB_RenderInit");
	return true;
}


/*
=============
RB_RenderShutdown
=============
*/
void RB_RenderShutdown()
{
	RB_BloomShutdown();
}
