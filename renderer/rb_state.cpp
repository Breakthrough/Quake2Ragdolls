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
// rb_state.cpp
//

#include "rb_local.h"

struct rb_glState_t
{
	// Texture state
	ETextureUnit		texUnit;
	refImage_t			*texBound[MAX_TEXUNITS];
	GLenum				texTarget[MAX_TEXUNITS];
	GLfloat				texEnvModes[MAX_TEXUNITS];
	bool				texMatIdentity[MAX_TEXUNITS];
	int					texGensEnabled[MAX_TEXUNITS];
	int					texCoordArrayMode[MAX_TEXUNITS];

	// Program state
	GLenum				boundFragProgram;
	GLenum				boundVertProgram;

	// Scene
	int					modelViewEntity;
	bool				bIn2DMode;
	bool				bFBOBound;

	// Generic state
	stateBit_t			stateBits;
	stateBit_t			stateMask;
};

static rb_glState_t		rb_glState;

/*
==============================================================================

	STATEBIT MANAGEMENT

==============================================================================
*/

/*
===============
RB_SetStateMask
===============
*/
void RB_SetStateMask(const stateBit_t newMask)
{
	rb_glState.stateMask = newMask;
}


/*
===============
RB_ResetStateMask
===============
*/
void RB_ResetStateMask()
{
	rb_glState.stateMask = ~0;
}


/*
===============
RB_StateForBits
===============
*/
void RB_StateForBits(stateBit_t newBits)
{
	// Modify the incoming state
	if (rb_glState.bIn2DMode)
		newBits &= ~SB_DEPTHTEST_ON;
	if (!(newBits & SB_DEPTHTEST_ON))
		newBits &= ~(SB_DEPTHMASK_ON|SB_DEPTHFUNC_MASK);

	// Debug cvar
	if (!gl_cull->intVal)
		newBits &= ~SB_CULL_MASK;

	// Shadowmap rendering parms
	if (ri.scn.viewType == RVT_SHADOWMAP)
	{
		// Invert the winding order
		newBits ^= SB_FRONTFACE_CW;

		// Flat shade
		newBits |= SB_SHADEMODEL_FLAT;

		// Don't write to the color buffer
		newBits |= (SB_COLORMASK_RED|SB_COLORMASK_GREEN|SB_COLORMASK_BLUE|SB_COLORMASK_ALPHA);
	}

	// Apply our global state mask
	newBits &= rb_glState.stateMask;

	// Process bit differences
	stateBit_t diff = newBits ^ rb_glState.stateBits;

	if (diff)
	{
		uint32 Changes = 0;

		// Alpha testing
		if (diff & SB_ATEST_MASK)
		{
			switch(newBits & SB_ATEST_MASK)
			{
			case 0:
				glDisable(GL_ALPHA_TEST);
				break;
			case SB_ATEST_GT0:
				glEnable(GL_ALPHA_TEST);
				glAlphaFunc(GL_GREATER, 0);
				break;
			case SB_ATEST_LT128:
				glEnable(GL_ALPHA_TEST);
				glAlphaFunc(GL_LESS, 0.5f);
				break;
			case SB_ATEST_GE128:
				glEnable(GL_ALPHA_TEST);
				glAlphaFunc(GL_GEQUAL, 0.5f);
				break;
			default:
				assert(0);
				break;
			}

			Changes++;
		}

		// Blending
		if (diff & (SB_BLEND_MTEX|SB_BLENDSRC_MASK|SB_BLENDDST_MASK))
		{
			if (newBits & (SB_BLENDSRC_MASK|SB_BLENDDST_MASK))
			{
				GLenum sFactor;
				switch(newBits & SB_BLENDSRC_MASK)
				{
				case SB_BLENDSRC_ZERO:					sFactor = GL_ZERO;					break;
				case SB_BLENDSRC_ONE:					sFactor = GL_ONE;					break;
				case SB_BLENDSRC_DST_COLOR:				sFactor = GL_DST_COLOR;				break;
				case SB_BLENDSRC_ONE_MINUS_DST_COLOR:	sFactor = GL_ONE_MINUS_DST_COLOR;	break;
				case SB_BLENDSRC_SRC_ALPHA:				sFactor = GL_SRC_ALPHA;				break;
				case SB_BLENDSRC_ONE_MINUS_SRC_ALPHA:	sFactor = GL_ONE_MINUS_SRC_ALPHA;	break;
				case SB_BLENDSRC_DST_ALPHA:				sFactor = GL_DST_ALPHA;				break;
				case SB_BLENDSRC_ONE_MINUS_DST_ALPHA:	sFactor = GL_ONE_MINUS_DST_ALPHA;	break;
				case SB_BLENDSRC_SRC_ALPHA_SATURATE:	sFactor = GL_SRC_ALPHA_SATURATE;	break;
				default:								assert(0);							break;
				}

				GLenum dFactor;
				switch(newBits & SB_BLENDDST_MASK)
				{
				case SB_BLENDDST_ZERO:					dFactor = GL_ZERO;					break;
				case SB_BLENDDST_ONE:					dFactor = GL_ONE;					break;
				case SB_BLENDDST_SRC_COLOR:				dFactor = GL_SRC_COLOR;				break;
				case SB_BLENDDST_ONE_MINUS_SRC_COLOR:	dFactor = GL_ONE_MINUS_SRC_COLOR;	break;
				case SB_BLENDDST_SRC_ALPHA:				dFactor = GL_SRC_ALPHA;				break;
				case SB_BLENDDST_ONE_MINUS_SRC_ALPHA:	dFactor = GL_ONE_MINUS_SRC_ALPHA;	break;
				case SB_BLENDDST_DST_ALPHA:				dFactor = GL_DST_ALPHA;				break;
				case SB_BLENDDST_ONE_MINUS_DST_ALPHA:	dFactor = GL_ONE_MINUS_DST_ALPHA;	break;
				default:								assert(0);							break;
				}

				if (newBits & SB_BLEND_MTEX)
				{
					if (rb_glState.texEnvModes[rb_glState.texUnit] != GL_REPLACE)
						glEnable(GL_BLEND);
					else
						glDisable(GL_BLEND);
				}
				else
				{
					glEnable(GL_BLEND);
				}

				glBlendFunc(sFactor, dFactor);
			}
			else
			{
				glDisable(GL_BLEND);
			}

			Changes++;
		}

		// Culling
		if (diff & SB_CULL_MASK)
		{
			switch(newBits & SB_CULL_MASK)
			{
			case 0:
				glDisable(GL_CULL_FACE);
				break;
			case SB_CULL_FRONT:
				glCullFace(GL_FRONT);
				glEnable(GL_CULL_FACE);
				break;
			case SB_CULL_BACK:
				glCullFace(GL_BACK);
				glEnable(GL_CULL_FACE);
				break;
			default:
				assert(0);
				break;
			}

			Changes++;
		}

		// Depth masking
		if (diff & SB_DEPTHMASK_ON)
		{
			if (newBits & SB_DEPTHMASK_ON)
				glDepthMask(GL_TRUE);
			else
				glDepthMask(GL_FALSE);

			Changes++;
		}

		// Depth testing
		if (diff & SB_DEPTHTEST_ON)
		{
			if (newBits & SB_DEPTHTEST_ON)
				glEnable(GL_DEPTH_TEST);
			else
				glDisable(GL_DEPTH_TEST);

			Changes++;
		}

		// Depth function
		if (diff & SB_DEPTHFUNC_MASK)
		{
			switch(newBits & SB_DEPTHFUNC_MASK)
			{
			case 0:
				glDepthFunc(GL_LEQUAL);
				break;
			case SB_DEPTHFUNC_EQ:
				glDepthFunc(GL_EQUAL);
				break;
			case SB_DEPTHFUNC_GEQ:
				glDepthFunc(GL_GEQUAL);
				break;
			default:
				assert(0);
				break;
			}

			Changes++;
		}

		// Polygon offset
		if (diff & SB_POLYOFFSET_ON)
		{
			if (newBits & SB_POLYOFFSET_ON)
				glEnable(GL_POLYGON_OFFSET_FILL);
			else
				glDisable(GL_POLYGON_OFFSET_FILL);

			Changes++;
		}

		// Programs
		if (diff & SB_FRAGMENT_PROGRAM_ON)
		{
			assert(ri.config.ext.bPrograms);
			if (newBits & SB_FRAGMENT_PROGRAM_ON)
				glEnable(GL_FRAGMENT_PROGRAM_ARB);
			else
				glDisable(GL_FRAGMENT_PROGRAM_ARB);

			Changes++;
		}

		if (diff & SB_VERTEX_PROGRAM_ON)
		{
			assert(ri.config.ext.bPrograms);
			if (newBits & SB_VERTEX_PROGRAM_ON)
				glEnable(GL_VERTEX_PROGRAM_ARB);
			else
				glDisable(GL_VERTEX_PROGRAM_ARB);

			Changes++;
		}

		// Face winding order
		if (diff & SB_FRONTFACE_CW)
		{
			if (newBits & SB_FRONTFACE_CW)
				glFrontFace(GL_CW);
			else
				glFrontFace(GL_CCW);

			Changes++;
		}

		// Model shading
		if (diff & SB_SHADEMODEL_FLAT)
		{
			if (newBits & SB_SHADEMODEL_FLAT)
				glShadeModel(GL_FLAT);
			else
				glShadeModel(GL_SMOOTH);

			Changes++;
		}

		// Depth range hack
		if (diff & SB_DEPTHRANGE_HACK)
		{
			if (newBits & SB_DEPTHRANGE_HACK)
				glDepthRange(0, 0.3f);
			else
				glDepthRange(0, 1);

			Changes++;
		}

		// Color masking
		if (diff & SB_COLORMASK_MASK)
		{
			glColorMask(
				!(newBits & SB_COLORMASK_RED),
				!(newBits & SB_COLORMASK_GREEN),
				!(newBits & SB_COLORMASK_BLUE),
				!(newBits & SB_COLORMASK_ALPHA)
			);

			Changes++;
		}

		// Performance counter
		if (!ri.scn.bDrawingMeshOutlines)
			ri.pc.stateChanges += Changes;

		// Save for the next diff
		rb_glState.stateBits = newBits;
	}
}

/*
==============================================================================

	TEXTURE STATE

==============================================================================
*/

/*
===============
RB_BindTexture
===============
*/
void RB_BindTexture(refImage_t *image)
{
	// Performance evaluation option
	if (!image)
		image = ri.media.noTexture;

	// Determine if it's already bound
	if (rb_glState.texBound[rb_glState.texUnit] == image)
		return;
	rb_glState.texBound[rb_glState.texUnit] = image;

	// Nope, bind it
	glBindTexture(image->target, image->texNum);

	// Performance counters
	if (r_speeds->intVal && !ri.scn.bDrawingMeshOutlines)
	{
		ri.pc.textureBinds++;
		if (image->visFrame != ri.frameCount)
		{
			image->visFrame = ri.frameCount;
			ri.pc.texturesInUse++;
			ri.pc.texelsInUse += image->upWidth * image->upHeight;
		}
	}
}


/*
===============
RB_SelectTexture
===============
*/
void RB_SelectTexture(const ETextureUnit texUnit)
{
	if (texUnit == rb_glState.texUnit)
		return;
	if (texUnit > ri.config.maxTexUnits)
	{
		Com_Error(ERR_DROP, "Attempted selection of an out of bounds (%d) texture unit!", texUnit);
		return;
	}

	// Select the unit
	rb_glState.texUnit = texUnit;
	if (ri.config.ext.bARBMultitexture)
	{
		qglActiveTextureARB(texUnit + GL_TEXTURE0_ARB);
		qglClientActiveTextureARB(texUnit + GL_TEXTURE0_ARB);
	}
	else if (ri.config.ext.bSGISMultiTexture)
	{
		qglSelectTextureSGIS(texUnit + GL_TEXTURE0_SGIS);
	}
	else
	{
		return;
	}

	// Performance counter
	if (!ri.scn.bDrawingMeshOutlines)
		ri.pc.textureUnitChanges++;
}


/*
===============
RB_TextureEnv
===============
*/
void RB_TextureEnv(GLfloat mode)
{
	if (mode == GL_ADD && !ri.config.ext.bTexEnvAdd)
		mode = GL_MODULATE;

	if (mode != rb_glState.texEnvModes[rb_glState.texUnit])
	{
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, mode);
		rb_glState.texEnvModes[rb_glState.texUnit] = mode;

		// Performance counter
		if (!ri.scn.bDrawingMeshOutlines)
			ri.pc.textureEnvChanges++;
	}
}


/*
===============
RB_TextureTarget

Supplements glEnable/glDisable on GL_TEXTURE_1D/2D/3D/CUBE_MAP_ARB.
===============
*/
void RB_TextureTarget(const GLenum target)
{
	if (target != rb_glState.texTarget[rb_glState.texUnit])
	{
		if (rb_glState.texTarget[rb_glState.texUnit])
		{
			glDisable(rb_glState.texTarget[rb_glState.texUnit]);

			// Performance counter
			if (!ri.scn.bDrawingMeshOutlines)
				ri.pc.textureTargetChanges++;
		}

		rb_glState.texTarget[rb_glState.texUnit] = target;

		if (target)
		{
			glEnable(target);

			// Performance counter
			if (!ri.scn.bDrawingMeshOutlines)
				ri.pc.textureTargetChanges++;
		}
	}
}


/*
===============
RB_SetTextureGen
===============
*/
void RB_SetTextureGen(const uint32 Coord, const int Mode)
{
	uint32 Bit, Gen;
	switch(Coord)
	{
	case GL_S:
		Bit = 1;
		Gen = GL_TEXTURE_GEN_S;
		break;
	case GL_T:
		Bit = 2;
		Gen = GL_TEXTURE_GEN_T;
		break;
	case GL_R:
		Bit = 4;
		Gen = GL_TEXTURE_GEN_R;
		break;
	case GL_Q:
		Bit = 8;
		Gen = GL_TEXTURE_GEN_Q;
		break;
	default:
		assert(0);
		return;
	}

	if (Mode)
	{
		if (!(rb_glState.texGensEnabled[rb_glState.texUnit] & Bit))
		{
			glEnable(Gen);
			rb_glState.texGensEnabled[rb_glState.texUnit] |= Bit;

			// Performance counter
			if (!ri.scn.bDrawingMeshOutlines)
				ri.pc.textureGenChanges++;
		}
		glTexGeni(Coord, GL_TEXTURE_GEN_MODE, Mode);
	}
	else
	{
		if (rb_glState.texGensEnabled[rb_glState.texUnit] & Bit)
		{
			glDisable(Gen);
			rb_glState.texGensEnabled[rb_glState.texUnit] &= ~Bit;

			// Performance counter
			if (!ri.scn.bDrawingMeshOutlines)
				ri.pc.textureGenChanges++;
		}
	}
}


/*
===============
RB_SetTexCoordArrayMode
===============
*/
void RB_SetTexCoordArrayMode(const int NewMode)
{
	int NewBit;
	if (NewMode == GL_TEXTURE_COORD_ARRAY)
		NewBit = 1;
	else if (NewMode == GL_TEXTURE_CUBE_MAP_ARB)
		NewBit = 2;
	else
		NewBit = 0;

	const int CurrentBit = rb_glState.texCoordArrayMode[rb_glState.texUnit];
	if (CurrentBit != NewBit)
	{
		if (CurrentBit == 1)
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		else if (CurrentBit == 2)
			glDisable(GL_TEXTURE_CUBE_MAP_ARB);

		if (NewBit == 1)
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		else if (NewBit == 2)
			glEnable(GL_TEXTURE_CUBE_MAP_ARB);

		rb_glState.texCoordArrayMode[rb_glState.texUnit] = NewBit;
	}
}


/*
===============
RB_LoadTexMatrix
===============
*/
void RB_LoadTexMatrix(mat4x4_t m)
{
	glLoadMatrixf(m);
	rb_glState.texMatIdentity[rb_glState.texUnit] = false;

	if (!ri.scn.bDrawingMeshOutlines)
		ri.pc.textureMatChanges++;
}


/*
===============
RB_LoadIdentityTexMatrix
===============
*/
void RB_LoadIdentityTexMatrix()
{
	if (!rb_glState.texMatIdentity[rb_glState.texUnit])
	{
		glLoadIdentity();
		rb_glState.texMatIdentity[rb_glState.texUnit] = true;

		if (!ri.scn.bDrawingMeshOutlines)
			ri.pc.textureMatChanges++;
	}
}


/*
===============
RB_BeginFBOUpdate
===============
*/
void RB_BeginFBOUpdate(refImage_t *FBOImage)
{
	assert(FBOImage->flags & (IT_FBO|IT_DEPTHANDFBO|IT_DEPTHFBO));
	glPushAttrib(GL_VIEWPORT_BIT);
	qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, FBOImage->fboNum);
	glViewport(0, 0, FBOImage->upWidth, FBOImage->upHeight);
	glScissor(0, 0, FBOImage->upWidth, FBOImage->upHeight);
}


/*
===============
RB_EndFBOUpdate
===============
*/
void RB_EndFBOUpdate()
{
	glPopAttrib();
	qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
}

/*
==============================================================================

	PROGRAM STATE

==============================================================================
*/

/*
===============
RB_BindProgram
===============
*/
void RB_BindProgram(refProgram_t *program)
{
	switch (program->target)
	{
	case GL_FRAGMENT_PROGRAM_ARB:
		if (rb_glState.boundFragProgram == program->progNum)
			return;
		rb_glState.boundFragProgram = program->progNum;

		qglBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, program->progNum);
		break;

	case GL_VERTEX_PROGRAM_ARB:
		if (rb_glState.boundVertProgram == program->progNum)
			return;
		rb_glState.boundVertProgram = program->progNum;

		qglBindProgramARB(GL_VERTEX_PROGRAM_ARB, program->progNum);
		break;

#ifdef _DEBUG
	default:
		assert(0);
		break;
#endif // _DEBUG
	}
}

/*
==============================================================================

	ORIENTATION MATRIX MANAGEMENT

==============================================================================
*/

/*
=============
R_CalcZFar
=============
*/
static inline float R_CalcZFar()
{
	float farClipDist = 0;
	if (!(ri.def.rdFlags & RDF_NOWORLDMODEL))
	{
		for (int i=0 ; i<8 ; i++)
		{
			vec3_t tmp;
			tmp[0] = ((i & 1) ? ri.scn.visMins[0] : ri.scn.visMaxs[0]);
			tmp[1] = ((i & 2) ? ri.scn.visMins[1] : ri.scn.visMaxs[1]);
			tmp[2] = ((i & 4) ? ri.scn.visMins[2] : ri.scn.visMaxs[2]);

			float dist = Vec3DistSquared(tmp, ri.def.viewOrigin);
			farClipDist = max(farClipDist, dist);
		}

		farClipDist = sqrtf(farClipDist);
	}
	else
	{
		farClipDist = 2048;
	}

	return r_zFarMin->intVal + farClipDist;
}

/*
=============
RB_SetupProjectionMatrix
=============
*/
void RB_SetupProjectionMatrix(refDef_t *rd, mat4x4_t m)
{
	// Near/far clip
	float zNear = r_zNear->intVal;
	if (r_zFarAbs->floatVal)
	{
		ri.scn.zFar = r_zFarAbs->floatVal;
	}
	else
	{
		ri.scn.zFar = R_CalcZFar();
	}

	// Calculate aspect
	float yMax = zNear * (float)tan ((rd->fovY * M_PI) / 360.0f);
	float yMin = -yMax;

	const float vAspect = (float)rd->width/(float)rd->height;
	float xMin = yMin * vAspect;
	float xMax = yMax * vAspect;

	// Adjust for stereo camera separation
	xMin += -(2 * ri.cameraSeparation) / zNear;
	xMax += -(2 * ri.cameraSeparation) / zNear;

	// Apply to matrix
	m[0] = (2.0f * zNear) / (xMax - xMin);
	m[1] = 0.0f;
	m[2] = 0.0f;
	m[3] = 0.0f;
	m[4] = 0.0f;
	m[5] = (2.0f * zNear) / (yMax - yMin);
	m[6] = 0.0f;
	m[7] = 0.0f;
	m[8] = (xMax + xMin) / (xMax - xMin);
	m[9] = (yMax + yMin) / (yMax - yMin);
	m[10] = -(ri.scn.zFar + zNear) / (ri.scn.zFar - zNear);
	m[11] = -1.0f;
	m[12] = 0.0f;
	m[13] = 0.0f;
	m[14] = -(2.0f * ri.scn.zFar * zNear) / (ri.scn.zFar - zNear);
	m[15] = 0.0f;
}


/*
=============
RB_SetupModelviewMatrix
=============
*/
void RB_SetupModelviewMatrix (refDef_t *rd, mat4x4_t m)
{
#if 0
	Matrix4_Identity (m);
	Matrix4_Rotate (m, -90, 1, 0, 0);
	Matrix4_Rotate (m,  90, 0, 0, 1);
#else
	Vec4Set(&m[0], 0, 0, -1, 0);
	Vec4Set(&m[4], -1, 0, 0, 0);
	Vec4Set(&m[8], 0, 1, 0, 0);
	Vec4Set(&m[12], 0, 0, 0, 1);
#endif

	Matrix4_Rotate(m, -rd->viewAngles[2], 1, 0, 0);
	Matrix4_Rotate(m, -rd->viewAngles[0], 0, 1, 0);
	Matrix4_Rotate(m, -rd->viewAngles[1], 0, 0, 1);
	Matrix4_Translate(m, -rd->viewOrigin[0], -rd->viewOrigin[1], -rd->viewOrigin[2]);
}


/*
=============
RB_SetupViewMatrixes
=============
*/
void RB_SetupViewMatrixes()
{
	// Set up the world view matrix
	RB_SetupModelviewMatrix(&ri.def, ri.scn.worldViewMatrix);

	// Set up projection matrix
	RB_SetupProjectionMatrix(&ri.def, ri.scn.projectionMatrix);
	if (ri.scn.viewType == RVT_MIRROR)
		ri.scn.projectionMatrix[0] = -ri.scn.projectionMatrix[0];

	// Cache a multiply
	Matrix4_Multiply(ri.scn.projectionMatrix, ri.scn.worldViewMatrix, ri.scn.worldViewProjectionMatrix);
}


/*
=================
RB_LoadModelIdentity

ala Vic
=================
*/
void RB_LoadModelIdentity()
{
	if (rb_glState.modelViewEntity != -1)
	{
		rb_glState.modelViewEntity = -1;
		Matrix4_Identity(ri.scn.objectMatrix);
		Matrix4_Copy(ri.scn.worldViewMatrix, ri.scn.modelViewMatrix);
		glLoadMatrixf(ri.scn.worldViewMatrix);
	}
}


/*
=================
RB_RotateForEntity

ala Vic
=================
*/
void RB_RotateForEntity(refEntity_t *ent)
{
	if (!ent || ent == ri.scn.worldEntity || ent == ri.scn.defaultEntity)
	{
		// Special case, since these entities never rotate
		RB_LoadModelIdentity();
	}
	else if (rb_glState.modelViewEntity != ent-ri.scn.entityList)
	{
		rb_glState.modelViewEntity = ent-ri.scn.entityList;

		Matrix3_Matrix4(ent->axis, ent->origin, ri.scn.objectMatrix);
		if (ent->model && ent->model->BSPData() && ent->scale != 1.0f)
			Matrix4_Scale(ri.scn.objectMatrix, ent->scale, ent->scale, ent->scale);
		Matrix4_MultiplyFast(ri.scn.worldViewMatrix, ri.scn.objectMatrix, ri.scn.modelViewMatrix);

		glLoadMatrixf(ri.scn.modelViewMatrix);
	}
}

/*
==============================================================================

	GENERIC STATE MANAGEMENT

==============================================================================
*/

/*
==================
RB_In2DMode
==================
*/
bool RB_In2DMode()
{
	return rb_glState.bIn2DMode;
}


/*
==================
RB_SetupGL2D
==================
*/
void RB_SetupGL2D()
{
	if (!rb_glState.bIn2DMode)
	{
		// Set the default state
		rb_glState.bIn2DMode = true;
		RB_StateForBits(SB_DEFAULT_2D);

		// Set 2D virtual screen size
		glViewport(0, 0, ri.config.vidWidth, ri.config.vidHeight);
		glScissor(0, 0, ri.config.vidWidth, ri.config.vidHeight);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, ri.config.vidWidth, ri.config.vidHeight, 0, -99999, 99999);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
	}
}

/*
=============
RB_SetupGL3D
=============
*/
void RB_SetupGL3D()
{
	// Flush any remaining batched 2D elements
	if (rb_glState.bIn2DMode)
		RF_Flush2D();

	// Set the default state
	rb_glState.bIn2DMode = false;
	RB_StateForBits(SB_DEFAULT_3D);

	// FBO
	if ((ri.def.rdFlags & RDF_FBO) && ri.config.ext.bFrameBufferObject)
	{
		rb_glState.bFBOBound = true;
		RB_BeginFBOUpdate(ri.screenFBO);
	}

	// Set up viewport
	glViewport(ri.def.x, ri.config.vidHeight - ri.def.height - ri.def.y, ri.def.width, ri.def.height);
	glScissor(ri.def.x, ri.config.vidHeight - ri.def.height - ri.def.y, ri.def.width, ri.def.height);

	// Set up projection and modelview matrixes
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(ri.scn.projectionMatrix);
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(ri.scn.worldViewMatrix);

	// Handle portal/mirror rendering
	switch(ri.scn.viewType)
	{
	case RVT_MIRROR:
	case RVT_PORTAL:
		{
			// Setup our clip plane
			GLdouble clip[4];

			clip[0] = ri.scn.clipPlane.normal[0];
			clip[1] = ri.scn.clipPlane.normal[1];
			clip[2] = ri.scn.clipPlane.normal[2];
			clip[3] = -ri.scn.clipPlane.dist;

			glClipPlane(GL_CLIP_PLANE0, clip);
			glEnable(GL_CLIP_PLANE0);
		}
		break;
	}
}


/*
=============
RB_EndGL3D
=============
*/
void RB_EndGL3D()
{
	if (ri.config.ext.bFrameBufferObject && rb_glState.bFBOBound)
	{
		RB_EndFBOUpdate();
		rb_glState.bFBOBound = false;
	}

	// Set the default state
	RB_StateForBits(SB_DEFAULT_3D);

	switch(ri.scn.viewType)
	{
	case RVT_MIRROR:
	case RVT_PORTAL:
		// Disable our clip plane
		glDisable(GL_CLIP_PLANE0);
		break;
	}
}


/*
================
RB_ClearBuffers
================
*/
void RB_ClearBuffers(const GLbitfield bitMask)
{
	GLbitfield clearBits = GL_DEPTH_BUFFER_BIT;

	if (gl_clear->intVal && ri.scn.viewType != RVT_MIRROR && ri.scn.viewType != RVT_PORTAL)
	{
		clearBits |= GL_COLOR_BUFFER_BIT;
	}

	clearBits &= bitMask;

	if (clearBits & GL_STENCIL_BUFFER_BIT)
		glClearStencil(128);
	if (clearBits & GL_COLOR_BUFFER_BIT)
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(clearBits);
}


/*
==================
RB_CheckForError
==================
*/
static inline const char *GetGLErrorString(const GLenum error)
{
	switch(error)
	{
	case GL_INVALID_ENUM:		return "INVALID ENUM";
	case GL_INVALID_OPERATION:	return "INVALID OPERATION";
	case GL_INVALID_VALUE:		return "INVALID VALUE";
	case GL_NO_ERROR:			return "NO ERROR";
	case GL_OUT_OF_MEMORY:		return "OUT OF MEMORY";
	case GL_STACK_OVERFLOW:		return "STACK OVERFLOW";
	case GL_STACK_UNDERFLOW:	return "STACK UNDERFLOW";
	}

	return "unknown";
}
void RB_CheckForError(const char *Where)
{
	const GLenum errNum = glGetError();
	if (errNum != GL_NO_ERROR)
	{
		if (Where)
			Com_Printf(PRNT_ERROR, "%s: GL_ERROR '%s' (0x%x)\n", Where, GetGLErrorString(errNum), errNum);
		else
			Com_Printf(PRNT_ERROR, "GL_ERROR '%s' (0x%x)\n", GetGLErrorString(errNum), errNum);
	}
}


/*
==================
RB_SetDefaultState

Sets our default OpenGL state
==================
*/
void RB_SetDefaultState()
{
	rb_glState.stateBits = 0;
	rb_glState.stateMask = ~0;
	rb_glState.bIn2DMode = false;
	rb_glState.bFBOBound = false;

	glFinish();

	glColor4f(1, 1, 1, 1);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	glEnable(GL_SCISSOR_TEST);
	glEnableClientState(GL_VERTEX_ARRAY);

	// Texture-unit specific
	for (int i=MAX_TEXUNITS-1 ; i>=0 ; i--)
	{
		rb_glState.texBound[i] = NULL;
		rb_glState.texEnvModes[i] = 0;
		rb_glState.texMatIdentity[i] = true;
		if (i >= ri.config.maxTexUnits)
			continue;

		// Texture
		RB_SelectTexture((ETextureUnit)i);

		glDisable(GL_TEXTURE_1D);
		glBindTexture(GL_TEXTURE_1D, 0);

		if (ri.config.ext.bTex3D)
		{
			glDisable(GL_TEXTURE_3D);
			glBindTexture(GL_TEXTURE_3D, 0);
		}
		if (ri.config.ext.bTexCubeMap)
		{
			glDisable(GL_TEXTURE_CUBE_MAP_ARB);
			glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, 0);
		}
		rb_glState.texCoordArrayMode[i] = 0;

		if (i == 0)
		{
			glEnable(GL_TEXTURE_2D);
			rb_glState.texTarget[i] = GL_TEXTURE_2D;
		}
		else
		{
			glDisable(GL_TEXTURE_2D);
			rb_glState.texTarget[i] = 0;
		}
		glBindTexture(GL_TEXTURE_2D, 0);

		// Texgen
		glDisable(GL_TEXTURE_GEN_S);
		glDisable(GL_TEXTURE_GEN_T);
		glDisable(GL_TEXTURE_GEN_R);
		glDisable(GL_TEXTURE_GEN_Q);
		rb_glState.texGensEnabled[i] = 0;
		rb_glState.texGensEnabled[i] = 0;
		rb_glState.texGensEnabled[i] = 0;
		rb_glState.texGensEnabled[i] = 0;
	}

	// Vertex and fragment programs
	if (ri.config.ext.bPrograms)
	{
		glDisable(GL_VERTEX_PROGRAM_ARB);
		glDisable(GL_FRAGMENT_PROGRAM_ARB);
	}

	// Frame buffer objects
	if (ri.config.ext.bFrameBufferObject)
	{
		qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	}

	// Stencil testing
	if (ri.bAllowStencil)
	{
		glDisable(GL_STENCIL_TEST);
		glStencilMask((GLuint)~0);
		glStencilFunc(GL_EQUAL, 128, 0xFF);
		glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
	}

	// Polygon modes
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glDisable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(r_offsetFactor->intVal, r_offsetUnits->intVal);

	// Depth testing
	glDisable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glDepthRange(0, 1);

	// Face culling
	glDisable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	rb_glState.stateBits |= SB_CULL_FRONT;

	// Alpha testing
	glDisable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0);

	// Blending
	glDisable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Model shading
	glShadeModel(GL_SMOOTH);

	// Face mode
	glFrontFace(GL_CCW);

	// Check for errors
	RB_CheckForError("RB_SetDefaultState");
}
