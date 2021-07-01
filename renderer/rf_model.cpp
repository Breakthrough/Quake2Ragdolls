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
// rf_model.c
// Model loading and caching
//

#include "rf_modelLocal.h"

static refModel_t	r_modelList[MAX_REF_MODELS];
static refModel_t	*r_modelHashTree[MAX_REF_MODEL_HASH];
static uint32		r_numModels;

extern cVar_t		*flushmap;

/*
===============================================================================

	GENERIC MODEL FUNCTIONS

===============================================================================
*/

/*
=================
R_ModelBounds
=================
*/
void R_ModelBounds(refModel_t *model, vec3_t mins, vec3_t maxs)
{
	if (model)
	{
		Vec3Copy(model->mins, mins);
		Vec3Copy(model->maxs, maxs);
	}
}


/*
===============
R_BuildTangentVectors
===============
*/
void R_BuildTangentVectors(const int numVertexes, const vec3_t *vertexArray, const vec2_t *coordArray, const int numTris, const index_t *indexes, vec3_t *sVectorsArray, vec3_t *tVectorsArray)
{
	// Assuming arrays have already been allocated
	// this also does some nice precaching
	memset(sVectorsArray, 0, numVertexes * sizeof(vec3_t));
	memset(tVectorsArray, 0, numVertexes * sizeof(vec3_t));

	for (int i=0 ; i<numTris ; i++, indexes+=3)
	{
		float *vert[3], *texCoord[3];
		for (int j=0 ; j<3 ; j++)
		{
			vert[j] = (float *)(vertexArray + indexes[j]);
			texCoord[j] = (float *)(coordArray + indexes[j]);
		}

		// Calculate two mostly perpendicular edge directions
		vec3_t stVec[3];
		Vec3Subtract(vert[0], vert[1], stVec[0]);
		Vec3Subtract(vert[2], vert[1], stVec[1]);

		// We have two edge directions, we can calculate the normal then
		vec3_t normal;
		CrossProduct(stVec[0], stVec[1], normal);
		VectorNormalizef(normal, normal);

		for (int j=0 ; j<3 ; j++)
		{
			stVec[0][j] = ((texCoord[1][1] - texCoord[0][1]) * (vert[2][j] - vert[0][j]) - (texCoord[2][1] - texCoord[0][1]) * (vert[1][j] - vert[0][j]));
			stVec[1][j] = ((texCoord[1][0] - texCoord[0][0]) * (vert[2][j] - vert[0][j]) - (texCoord[2][0] - texCoord[0][0]) * (vert[1][j] - vert[0][j]));
		}

		// Keep s\t vectors orthogonal
		for (int j=0 ; j<2 ; j++)
		{
			float dot = -DotProduct(stVec[j], normal);
			Vec3MA(stVec[j], dot, normal, stVec[j]);
			VectorNormalizef(stVec[j], stVec[j]);
		}

		// Inverse tangent vectors if needed
		CrossProduct(stVec[1], stVec[0], stVec[2]);
		if (DotProduct(stVec[2], normal) < 0)
		{
			Vec3Inverse(stVec[0]);
			Vec3Inverse(stVec[1]);
		}

		for (int j=0 ; j<3 ; j++)
		{
			Vec3Add(sVectorsArray[indexes[j]], stVec[0], sVectorsArray[indexes[j]]);
			Vec3Add(tVectorsArray[indexes[j]], stVec[1], tVectorsArray[indexes[j]]);
		}
	}

	// Normalize
	for (int i=0 ; i<numVertexes ; i++)
	{
		VectorNormalizef(sVectorsArray[i], sVectorsArray[i]);
		VectorNormalizef(tVectorsArray[i], tVectorsArray[i]);
	}
}

/*
===============================================================================

	SP2 LOADING

===============================================================================
*/

/*
=================
R_LoadSP2Model
=================
*/
static bool R_LoadSP2Model(refModel_t *model)
{
	// Load the file
	byte *buffer;
	const int fileLen = FS_LoadFile(model->name, (void **)&buffer, false);
	if (!buffer || fileLen <= 0)
		return false;

	dSpriteHeader_t *inModel = (dSpriteHeader_t *)buffer;

	//
	// Sanity checks
	//
	const int version = LittleLong(inModel->version);
	if (version != SP2_VERSION)
	{
		FS_FreeFile(buffer);
		Com_Printf(PRNT_ERROR, "R_LoadSP2Model: '%s' has wrong version number (%i should be %i)\n", model->name, version, SP2_VERSION);
		return false;
	}

	const int numFrames = LittleLong(inModel->numFrames);
	if (numFrames > SP2_MAX_FRAMES)
	{
		FS_FreeFile(buffer);
		Com_Printf(PRNT_ERROR, "R_LoadSP2Model: '%s' has too many frames (%i > %i)\n", model->name, numFrames, SP2_MAX_FRAMES);
		return false;
	}

	//
	// Allocate
	//
	byte *allocBuffer = (byte*)R_ModAlloc(model, sizeof(mSpriteModel_t) + (sizeof(mSpriteFrame_t) * numFrames));

	model->type = MODEL_SP2;
	model->modelData = (mSpriteModel_t *)allocBuffer;
	mSpriteModel_t *outModel = model->SpriteData();
	outModel->numFrames = numFrames;

	//
	// Byte swap
	//
	allocBuffer += sizeof(mSpriteModel_t);
	outModel->frames = (mSpriteFrame_t *)allocBuffer;
	mSpriteFrame_t *outFrames = outModel->frames;
	dSpriteFrame_t *inFrames = inModel->frames;

	for (int i=0 ; i<outModel->numFrames ; i++, inFrames++, outFrames++)
	{
		outFrames->width	= LittleLong(inFrames->width);
		outFrames->height	= LittleLong(inFrames->height);
		outFrames->originX	= LittleLong(inFrames->originX);
		outFrames->originY	= LittleLong(inFrames->originY);

		// For culling
		outFrames->radius	= sqrtf((outFrames->width*outFrames->width) + (outFrames->height*outFrames->height));
		model->radius		= max(model->radius, outFrames->radius);

		// Register the material
		Com_NormalizePath(outFrames->name, sizeof(outFrames->name), inFrames->name);
		outFrames->skin = R_RegisterPoly(outFrames->name);
		if (!outFrames->skin)
			Com_DevPrintf(PRNT_WARNING, "R_LoadSP2Model: '%s' could not load skin '%s'\n", model->name, outFrames->name);
	}

	// Done
	return true;
}

static bool R_LoadImageModel (refModel_t *model)
{
	var image = R_RegisterPoly(model->name);

	if (image == null)
		return false;

	int width, height;
	R_GetImageSize(image, &width, &height);

	//
	// Allocate
	//
	byte *allocBuffer = (byte*)R_ModAlloc(model, sizeof(mSpriteModel_t) + (sizeof(mSpriteFrame_t) * 1));

	model->type = MODEL_SP2;
	model->modelData = (mSpriteModel_t *)allocBuffer;
	mSpriteModel_t *outModel = model->SpriteData();
	outModel->numFrames = 1;

	//
	// Byte swap
	//
	allocBuffer += sizeof(mSpriteModel_t);
	outModel->frames = (mSpriteFrame_t *)allocBuffer;
	mSpriteFrame_t *outFrames = outModel->frames;

	Q_strncpyz(outFrames->name, model->name, strlen(model->name)+1);

	outFrames->width	= LittleLong(width);
	outFrames->height	= LittleLong(height);
	outFrames->originX	= LittleLong(width / 2);
	outFrames->originY	= LittleLong(height / 2);

	// For culling
	outFrames->radius	= sqrtf((outFrames->width*outFrames->width) + (outFrames->height*outFrames->height));
	model->radius		= max(model->radius, outFrames->radius);

	// Register the material
	outFrames->skin = image;
	if (!outFrames->skin)
		Com_DevPrintf(PRNT_WARNING, "R_LoadSP2Model: '%s' could not load skin '%s'\n", model->name, outFrames->name);

	return true;
}

/*
===============================================================================

	MODEL REGISTRATION

===============================================================================
*/

/*
================
R_FreeModel
================
*/
static void R_FreeModel(refModel_t *model)
{
	assert(model);
	if (!model)
		return;

	// De-link it from the hash tree
	refModel_t	**prev = &r_modelHashTree[model->hashValue];
	for ( ; ; )
	{
		refModel_t *hashMdl = *prev;
		if (!hashMdl)
			break;

		if (hashMdl == model)
		{
			*prev = hashMdl->hashNext;
			break;
		}
		prev = &hashMdl->hashNext;
	}

	ri.reg.modelsReleased++;

	// Free it
	Mem_FreeTag(ri.modelSysPool, model->memTag);

	// Clear the spot
	model->type = MODEL_BAD;
}


/*
================
R_TouchModel

Touches/loads all textures for the model type
================
*/
static void R_TouchModel(refModel_t *model)
{
	ri.reg.modelsTouched++;
	model->touchFrame = ri.reg.registerFrame;

	switch(model->type)
	{
	case MODEL_INTERNAL:
		break;

	case MODEL_MD2:
	case MODEL_MD3:
		{
			mAliasModel_t *aliasModel = model->AliasData();
			for (int i=0 ; i<aliasModel->numMeshes ; i++)
			{
				mAliasMesh_t *aliasMesh = &aliasModel->meshes[i];
				for (int j=0 ; j<aliasMesh->numSkins ; j++)
				{
					mAliasSkin_t *aliasSkin = &aliasMesh->skins[j];
					if (!aliasSkin->name[0])
					{
						aliasSkin->material = NULL;
						continue;
					}

					aliasSkin->material = R_RegisterSkin(aliasSkin->name);
				}
			}

			for (int i = 0; i < aliasModel->numLODs; ++i)
				aliasModel->modelLODs[i].model = R_RegisterModel(aliasModel->modelLODs[i].model->bareName);
		}
		break;

	case MODEL_Q2BSP:
		{
			mQ2BspModel_t *q2BspModel = model->Q2BSPData();
			for (int i=0 ; i<q2BspModel->numTexInfo ; i++)
			{
				mQ2BspTexInfo_t *ti = &q2BspModel->texInfo[i];
				if (ti->flags & SURF_TEXINFO_SKY)
				{
					ti->mat = ri.media.noMaterialSky;
					continue;
				}

				ti->mat = R_RegisterTexture(ti->texName, ti->surfParams);
				if (!ti->mat)
				{
					if (ti->surfParams & MAT_SURF_LIGHTMAP)
						ti->mat = ri.media.noMaterialLightmap;
					else
						ti->mat = ri.media.noMaterial;
				}
			}

			R_TouchLightmaps();
		}
		break;

	case MODEL_Q3BSP:
		{
			mQ3BspModel_t *q3BspModel = model->Q3BSPData();
			for (int i=0 ; i<model->BSPData()->numSurfaces ; i++)
			{
				mBspSurface_t *surf = &model->BSPData()->surfaces[i];
				mQ3BspMaterialRef_t *matRef = surf->q3_matRef;

				if (surf->q3_faceType == FACETYPE_FLARE)
				{
					matRef->mat = R_RegisterFlare(matRef->name);
					if (!matRef->mat)
						matRef->mat = ri.media.noMaterial;
				}
				else
				{
					if (surf->q3_faceType == FACETYPE_TRISURF || r_vertexLighting->intVal || surf->lmTexNum < 0)
					{
						matRef->mat = R_RegisterTextureVertex(matRef->name);
						if (!matRef->mat)
							matRef->mat = ri.media.noMaterial;
					}
					else
					{
						matRef->mat = R_RegisterTextureLM(matRef->name);
						if (!matRef->mat)
							matRef->mat = ri.media.noMaterialLightmap;
					}
				}
			}

			for (int i=0 ; i<q3BspModel->numFogs ; i++)
			{
				mQ3BspFog_t *fog = &q3BspModel->fogs[i];
				if (!fog->name[0])
				{
					fog->mat = NULL;
					continue;
				}
				fog->mat = R_RegisterTextureLM(fog->name);
			}

			R_TouchLightmaps();
		}
		break;

	case MODEL_SP2:
		{
			mSpriteModel_t *spriteModel = model->SpriteData();
			for (int i=0 ; i<spriteModel->numFrames ; i++)
			{
				mSpriteFrame_t *spriteFrame = &spriteModel->frames[i];
				if (!spriteFrame)
				{
					spriteFrame->skin = NULL;
					continue;
				}

				spriteFrame->skin = R_RegisterPoly(spriteFrame->name);
			}
		}
		break;

	default:
		assert(0);
		break;
	}
}


/*
================
R_FindModel
================
*/
static inline refModel_t *R_FindModel(const char *bareName)
{
	assert(bareName && bareName[0]);
	ri.reg.modelsSeaked++;

	const uint32 hash = Com_HashGeneric(bareName, MAX_REF_MODEL_HASH);

	// Search the currently loaded models
	for (refModel_t *model=r_modelHashTree[hash] ; model ; model=model->hashNext)
	{
		if (model->type != MODEL_BAD)
		{
			// Check name
			if (!strcmp (bareName, model->bareName))
				return model;
		}
	}

	return NULL;
}


/*
================
R_GetModelSlot
================
*/
static inline refModel_t *R_GetModelSlot()
{
	// Find a free model slot spot
	for (uint32 i=0 ; i<r_numModels ; i++)
	{
		refModel_t *model = &r_modelList[i];
		if (model->type != MODEL_BAD)
			continue;	// Used r_modelList slot

		model->memTag = i+1;
		return model;
	}

	if (r_numModels+1 >= MAX_REF_MODELS)
		Com_Error(ERR_DROP, "r_numModels >= MAX_REF_MODELS");

	refModel_t *model = &r_modelList[r_numModels++];
	model->memTag = r_numModels;
	return model;
}


/*
==================
R_LoadInlineBSPModel
==================
*/
static refModel_t *R_LoadInlineBSPModel(const char *name)
{
	ri.reg.modelsSeaked++;

	// Inline models are grabbed only from worldmodel
	uint32 i = atoi(name+1);
	if (i < 1 || i >= ri.scn.worldModel->BSPData()->numSubModels)
		Com_Error(ERR_DROP, "R_LoadInlineBSPModel: Bad inline model number '%d'", i);

	refModel_t *model = &ri.scn.worldModel->BSPData()->inlineModels[i];
	if (model)
	{
		model->touchFrame = ri.reg.registerFrame;
		return model;
	}

	return NULL;
}


/*
==================
R_LoadBSPModel
==================
*/
struct bspFormat_t
{
	const char	*headerStr;
	int			headerLen;
	int			version;
	bool		(*loader) (refModel_t *model, byte *buffer);
};

static bspFormat_t r_bspFormats[] =
{
	{ Q2BSP_HEADER,		4,	Q2BSP_VERSION,					R_LoadQ2BSPModel },	// Quake2 BSP models
	{ Q3BSP_HEADER,		4,	Q3BSP_VERSION,					R_LoadQ3BSPModel },	// Quake3 BSP models

	{ NULL,				0,	0,								NULL }
};

static int r_numBSPFormats = (sizeof(r_bspFormats) / sizeof(r_bspFormats[0])) - 1;

static refModel_t *R_LoadBSPModel(const char *name)
{
	// Normalize, strip, lower
	char bareName[MAX_QPATH];
	Com_NormalizePath(bareName, sizeof(bareName), name);
	Com_StripExtension(bareName, sizeof(bareName), bareName);
	Q_strlwr(bareName);

	// Use if already loaded
	refModel_t *model = R_FindModel(bareName);
	if (model)
	{
		R_TouchModel(model);
		return model;
	}

	// Not found -- allocate a spot
	model = R_GetModelSlot();

	// Load the file
	byte *buffer;
	const int fileLen = FS_LoadFile(name, (void **)&buffer, false);
	if (!buffer || fileLen <= 0)
		Com_Error(ERR_DROP, "R_LoadBSPModel: %s not found", name);

	// Find the format
	bspFormat_t *descr;
	int i;
	for (i=0 ; i<r_numBSPFormats ; i++)
	{
		descr = &r_bspFormats[i];
		if (strncmp((const char *)buffer, descr->headerStr, descr->headerLen))
			continue;
		if (((int *)buffer)[1] != descr->version)
			continue;
		break;
	}
	if (i == r_numBSPFormats)
	{
		FS_FreeFile(buffer);
		Com_Error(ERR_DROP, "R_LoadBSPModel: unknown fileId for %s", model->name);
	}

	// Clear
	model->radius = 0;
	ClearBounds(model->mins, model->maxs);

	// Load
	Q_strncpyz(model->bareName, bareName, sizeof(model->bareName));
	Q_strncpyz(model->name, name, sizeof(model->name));
	if (!descr->loader(model, buffer))
	{
		Mem_FreeTag(ri.modelSysPool, model->memTag);
		model->type = MODEL_BAD;
		FS_FreeFile(buffer);
		Com_Error(ERR_DROP, "R_LoadBSPModel: failed to load map!", model->name);
	}

	// Store values
	model->hashValue = Com_HashGeneric(bareName, MAX_REF_MODEL_HASH);
	model->touchFrame = ri.reg.registerFrame;

	// Link into hash tree
	model->hashNext = r_modelHashTree[model->hashValue];
	r_modelHashTree[model->hashValue] = model;

	FS_FreeFile(buffer);
	return model;
}


/*
================
R_RegisterMap

Specifies the model that will be used as the world
================
*/
void R_RegisterMap(const char *mapName)
{
	// Check the name
	if (!mapName)
		Com_Error(ERR_DROP, "R_RegisterMap: NULL name");
	if (!mapName[0])
		Com_Error(ERR_DROP, "R_RegisterMap: empty name");

	// Explicitly free the old map if different...
	if ((ri.scn.worldModel->type != MODEL_BAD && ri.scn.worldModel->type != MODEL_INTERNAL)
	&& (strcmp(ri.scn.worldModel->name, mapName) || flushmap->intVal))
	{
		R_FreeModel(ri.scn.worldModel);
	}

	// Load the model
	ri.scn.worldModel = R_LoadBSPModel(mapName);
	ri.scn.worldEntity->model = ri.scn.worldModel;

	// Force updates (markleaves, light marking, etc)
	ri.scn.viewCluster = -1;
	ri.scn.visFrameCount++;
	ri.frameCount++;
}


/*
================
R_EndModelRegistration
================
*/
void R_EndModelRegistration()
{
	for (uint32 i=0 ; i<r_numModels ; i++)
	{
		refModel_t *model = &r_modelList[i];
		if (model->type != MODEL_BAD)
		{
			if (model->type == MODEL_INTERNAL)
			{
				model->touchFrame = ri.reg.registerFrame;
			}

			if (model->touchFrame == ri.reg.registerFrame)
			{
				// Used this sequence
				R_TouchModel(model);
			}
			else
			{
				// Unused
				R_FreeModel(model);
			}
		}
	}

	Com_DevPrintf(PRNT_CONSOLE, "Completing model system registration:\n-Released: %i\n-Touched: %i\n-Seaked: %i\n", ri.reg.modelsReleased, ri.reg.modelsTouched, ri.reg.modelsSeaked);
}

refAliasAnimation_t blankAnimation;

refAliasAnimation_t *R_GetModelAnimation (refModel_t *model, int index)
{
	var ad = model->AliasData();

	if (index < 0 || index >= ad->numAnims)
		return &blankAnimation;

	return &ad->modelAnimations[index];
}

void R_LoadAnimationFile (refModel_t *model, const String &fileName)
{
	char *buffer;

	FS_LoadFile(fileName.CString(), (void**)&buffer, true);

	var p = PS_StartSession(buffer, PSP_COMMENT_LINE | PSP_COMMENT_BLOCK);
	TList<String> nextAnimations;
	TList<refAliasAnimation_t> modelAnimations;

	while (true)
	{
		char *token;

		if (!PS_ParseToken(p, PSF_ALLOW_NEWLINES | PSF_TO_LOWER, &token))
			break;

		refAliasAnimation_t animation;
		animation.Name = token;

		if (!PS_ParseToken(p, PSF_ALLOW_NEWLINES | PSF_TO_LOWER, &token))
			break;

		if (strcmp(token, "{") != 0)
			break;

		if (!PS_ParseDataType(p, PSF_ALLOW_NEWLINES | PSF_TO_LOWER, PSDT_INTEGER, &animation.Start, 1) ||
			!PS_ParseDataType(p, PSF_ALLOW_NEWLINES | PSF_TO_LOWER, PSDT_INTEGER, &animation.End, 1) ||
			!PS_ParseDataType(p, PSF_ALLOW_NEWLINES | PSF_TO_LOWER, PSDT_UINTEGER, &animation.FPS, 1))
			break;

		if (!PS_ParseToken(p, PSF_ALLOW_NEWLINES | PSF_TO_LOWER, &token))
			break;

		animation.TypeData = null;

		if (strcmp(token, "loop") == 0)
		{
			animation.Type = AA_LOOP;

			refAnimLoopData_t *loopData = new refAnimLoopData_t();
			loopData->Start = animation.Start;
			animation.TypeData = loopData;

			if (!PS_ParseToken(p, PSF_ALLOW_NEWLINES | PSF_TO_LOWER, &token))
				break;

			if (strcmp(token, "}"))
				loopData->Start = atoi(token);
		}
		else
		{
			animation.Type = AA_NORMAL;

			if (!PS_ParseToken(p, PSF_ALLOW_NEWLINES | PSF_TO_LOWER, &token))
				break;
		}

		animation.NextAnimation = -1;
		modelAnimations.Add(animation);

		if (strcmp(token, "}") == 0)
			nextAnimations.Add("-1");
		else
		{
			nextAnimations.Add(token);

			if (!PS_ParseToken(p, PSF_ALLOW_NEWLINES | PSF_TO_LOWER, &token))
				break;

			if (strcmp(token, "}") != 0)
				break;
		}
	}

	for (uint32 i = 0; i < modelAnimations.Count(); ++i)
	{
		bool isDigit = false;

		for (uint32 x = 0; x < nextAnimations[i].Count(); ++x)
		{
			if ((nextAnimations[i][x] >= '0' && nextAnimations[i][x] <= '9') || nextAnimations[i][x] == '-')
			{
				isDigit = true;
				break;
			}
		}

		if (isDigit)
			modelAnimations[i].NextAnimation = atoi(nextAnimations[i].CString());
		else
		{
			int index = -1;

			for (uint32 x = 0; x < modelAnimations.Count(); ++x)
			{
				if (modelAnimations[x].Name.Compare(nextAnimations[i]) == 0)
				{
					index = x;
					break;
				}
			}

			modelAnimations[i].NextAnimation = index;
		}
	}

	var aliasData = model->AliasData();
	aliasData->numAnims = modelAnimations.Count();
	aliasData->modelAnimations = QNew(ri.modelSysPool, 0) refAliasAnimation_t[modelAnimations.Count()];

	for (uint32 i = 0; i < modelAnimations.Count(); ++i)
		aliasData->modelAnimations[i] = modelAnimations[i];
}

void R_LoadLODs (refModel_t *model, const String &fileName)
{
	char *buffer;

	FS_LoadFile(fileName.CString(), (void**)&buffer, true);

	if (buffer == null)
		return;

	var p = PS_StartSession(buffer, PSP_COMMENT_LINE | PSP_COMMENT_BLOCK);
	TList<mAliasLod_t> lods;

	while (true)
	{
		char *token;

		if (!PS_ParseToken(p, PSF_ALLOW_NEWLINES | PSF_TO_LOWER, &token))
			break;

		mAliasLod_t lod;

		String name = token;
		String path = Path::GetFilePath(fileName) + '/' + name;

		lod.model = R_RegisterModel(path.CString());

		if (!PS_ParseDataType(p, PSF_ALLOW_NEWLINES | PSF_TO_LOWER, PSDT_FLOAT, &lod.distance, 1))
			break;

		lods.Add(lod);
	}

	var *aliasData = model->AliasData();
	aliasData->numLODs = lods.Count();

	aliasData->modelLODs = QNew(ri.modelSysPool, 0) mAliasLod_t[lods.Count()];
	for (uint32 i = 0; i < lods.Count(); ++i)
		aliasData->modelLODs[i] = lods[i];
}

/*
================
R_RegisterModel

Load/re-register a model
================
*/
static bool checkLODs = true;
refModel_t *R_RegisterModel(const char *name)
{
	refModel_t	*model;
	char		bareName[MAX_QPATH];
	int			len;

	// Check the name
	if (!name || !name[0])
		return NULL;

	// If this is a BModel, skip what's below
	if (name[0] == '*')
	{
		model = R_LoadInlineBSPModel(name);
		return model;
	}

	// Check the length
	len = strlen (name);
	if (len+1 >= MAX_QPATH)
	{
		Com_Printf(PRNT_ERROR, "R_RegisterModel: Model name too long! %s\n", name);
		return NULL;
	}

	// Normalize, strip, lower
	Com_NormalizePath(bareName, sizeof(bareName), name);
	Com_StripExtension(bareName, sizeof(bareName), bareName);
	Q_strlwr(bareName);

	// Use if already loaded
	model = R_FindModel(bareName);
	if (model)
	{
		R_TouchModel (model);
		return model;
	}

	// Not found -- allocate a spot and fill base values
	model = R_GetModelSlot();
	model->radius = 0;
	ClearBounds(model->mins, model->maxs);

	// SP2 model hack
	if (!Q_stricmp(name+len-4, ".sp2"))
	{
		Q_snprintfz(model->name, sizeof(model->name), "%s.sp2", bareName);
		if (R_LoadSP2Model(model))
			goto loadModel;
	}
	else if (!Q_stricmp(name+len-4, ".pcx") ||
		!Q_stricmp(name+len-4, ".tga") ||
		!Q_stricmp(name+len-4, ".png") ||
		!Q_stricmp(name+len-4, ".jpg") ||
		!strstr(name, "."))
	{
		Q_snprintfz(model->name, sizeof(model->name), "%s", bareName);
		if (R_LoadImageModel(model))
			goto loadModel;

		Q_snprintfz(model->name, sizeof(model->name), "pics/%s", bareName);
		if (R_LoadImageModel(model))
			goto loadModel;
	}

	// MD3
	Q_snprintfz(model->name, sizeof(model->name), "%s.md3", bareName);
	if (R_LoadMD3Model(model))
		goto loadModel;

	// MD2
	Q_snprintfz(model->name, sizeof(model->name), "%s.md2", bareName);
	if (R_LoadMD2Model(model))
		goto loadModel;

	// MD2
	Q_snprintfz(model->name, sizeof(model->name), "%s.md2e", bareName);
	if (R_LoadMD2EModel(model))
		goto loadModel;


	// Nothing found!
	model->type = MODEL_BAD;
	return NULL;

	// Found and loaded it, finish the model struct
loadModel:
	Q_strncpyz(model->bareName, bareName, sizeof(model->bareName));
	model->hashValue = Com_HashGeneric(bareName, MAX_REF_MODEL_HASH);
	model->touchFrame = ri.reg.registerFrame;

	// Link into hash tree
	model->hashNext = r_modelHashTree[model->hashValue];
	r_modelHashTree[model->hashValue] = model;

	String str = Path::StripExtension(model->name) + ".anm";

	model->AliasData()->numAnims = 0;

	if (FS_FileExists(str.CString()) != -1)
		R_LoadAnimationFile(model, str);

	if ((model->type == MODEL_MD2 || model->type == MODEL_MD3) && checkLODs)
	{
		checkLODs = false;

		str = Path::StripExtension(model->name) + ".lod";

		R_LoadLODs(model, str);

		checkLODs = true;
	}

	return model;
}

/*
===============================================================================

	CONSOLE COMMANDS

===============================================================================
*/

/*
================
R_ModelList_f
================
*/
static void R_ModelList_f()
{
	Com_Printf(0, "Loaded models:\n");

	uint32 total = 0;
	uint32 totalBytes = 0;
	for (uint32 i=0 ; i<r_numModels ; i++)
	{
		refModel_t *mod = &r_modelList[i];
		if (mod->type == MODEL_BAD)
			continue; // Free r_modelList slot

		switch (mod->type)
		{
		case MODEL_INTERNAL:	Com_Printf(0, "INTRN");			break;
		case MODEL_MD2:			Com_Printf(0, "MD2  ");			break;
		case MODEL_MD3:			Com_Printf(0, "MD3  ");			break;
		case MODEL_Q2BSP:		Com_Printf(0, "Q2BSP");			break;
		case MODEL_Q3BSP:		Com_Printf(0, "Q3BSP");			break;
		case MODEL_SP2:			Com_Printf(0, "SP2  ");			break;
		default:				Com_Printf(PRNT_ERROR, "BAD");	break;
		}

		uint32 MemSize = Mem_TagSize(ri.modelSysPool, mod->memTag);
		Com_Printf(0, " %9uB (%6.3fMB) %s\n", MemSize, MemSize/1048576.0f, mod->name);

		totalBytes += MemSize;
		total++;
	}

	Com_Printf(0, "%i model(s) loaded, %u bytes (%6.3fMB) total\n", total, totalBytes, totalBytes/1048576.0f);
}

/*
===============================================================================

	INTERNAL MODELS

===============================================================================
*/

/*
===============
R_AddInternalModelToList
===============
*/
void R_AddInternalModelToList(refEntity_t *ent)
{
	if (ri.scn.viewType == RVT_MIRROR)
	{
		if (ent->flags & RF_WEAPONMODEL) 
			return;
	}
	else
	{
		if (ent->flags & RF_VIEWERMODEL) 
			return;
		if (ent->flags & RF_WEAPONMODEL) 
			return;
	}

	refModel_t *model = ent->model;

	vec3_t outMins, outMaxs;
	Vec3Add(ent->origin, model->mins, outMins);
	Vec3Add(ent->origin, model->maxs, outMaxs);
	float outRadius = model->radius;
	if (ent->scale != 1.0f)
	{
		Vec3Scale(outMins, ent->scale, outMins);
		Vec3Scale(outMaxs, ent->scale, outMaxs);
		outRadius *= ent->scale;
	}

	if (R_CullBox(outMins, outMaxs, ri.scn.clipFlags))
		return;

	mQ3BspFog_t *Fog = R_FogForSphere(ent->origin, outRadius);
	ri.scn.currentList->AddToList(MBT_INTERNAL, model, ri.media.noMaterialAlias, 0.0f, ent, Fog, 0);
}

/*
===============
R_DrawInternalModel
===============
*/
void R_DrawInternalModel(refMeshBuffer *mb, const meshFeatures_t features)
{
	refEntity_t *ent = mb->DecodeEntity();
	refMaterial_t *mat = mb->DecodeMaterial();
	mInternalModel_t *internalModel = ((refModel_t*)mb->mesh)->InternalData();
	refMesh_t *mesh = internalModel->mesh;

	// Interpolation calculations
	const float backLerp = (!r_lerpmodels->intVal || mat->flags & MAT_NOLERP) ? 0.0f : ent->backLerp;

	vec3_t move, delta;
	Vec3Subtract(ent->oldOrigin, ent->origin, delta);
	Matrix3_TransformVector(ent->axis, delta, move);
	Vec3Scale(move, backLerp, move);

	// Adjust mesh for entity scale
	if (ent->scale != 1.0f)
	{
		for (int i=0 ; i<mesh->numVerts ; i++)
		{
			rb.batch.vertices[i][0] = move[0] + mesh->vertexArray[i][0]*ent->scale;
			rb.batch.vertices[i][1] = move[1] + mesh->vertexArray[i][1]*ent->scale;
			rb.batch.vertices[i][2] = move[2] + mesh->vertexArray[i][2]*ent->scale;
		}
	}
	else
	{
		for (int i=0 ; i<mesh->numVerts ; i++)
		{
			rb.batch.vertices[i][0] = move[0] + mesh->vertexArray[i][0];
			rb.batch.vertices[i][1] = move[1] + mesh->vertexArray[i][1];
			rb.batch.vertices[i][2] = move[2] + mesh->vertexArray[i][2];
		}
	}

	refMesh_t outMesh;

	outMesh.numIndexes = mesh->numIndexes * 3;
	outMesh.numVerts = mesh->numVerts;

	outMesh.colorArray = rb.batch.colors;
	outMesh.coordArray = mesh->coordArray;
	outMesh.indexArray = mesh->indexArray;
	outMesh.lmCoordArray = NULL;
	outMesh.normalsArray = mesh->normalsArray;
	outMesh.vertexArray = rb.batch.vertices;

	// Calculate lighting if colors are needed
	if (features & MF_NORMALS && features & MF_COLORS)
		R_LightForEntity(ent, outMesh.vertexArray, outMesh.numVerts, outMesh.normalsArray, outMesh.colorArray[0]);

	// Build stVectors
	if (features & MF_STVECTORS)
	{
		R_BuildTangentVectors(outMesh.numVerts, outMesh.vertexArray, outMesh.coordArray, outMesh.numIndexes/3, outMesh.indexArray, rb.batch.sVectors, rb.batch.tVectors);
		outMesh.sVectorsArray = rb.batch.sVectors;
		outMesh.tVectorsArray = rb.batch.tVectors;
	}
	else
	{
		outMesh.sVectorsArray = NULL;
		outMesh.tVectorsArray = NULL;
	}


	RB_PushMesh(&outMesh, features);
	RB_RenderMeshBuffer(mb);
}

/*
===============================================================================

	INIT / SHUTDOWN

===============================================================================
*/

static conCmd_t	*cmd_modelList;

/*
===============
R_InitSpecialModels
===============
*/
static void R_InitSpecialModels()
{
	// Generate a defaultModel
	// Fill in the default information
	ri.scn.defaultModel = R_GetModelSlot();
	ri.scn.defaultModel->type = MODEL_INTERNAL;
	Q_strncpyz(ri.scn.defaultModel->name, "***r_defaultModel***", sizeof(ri.scn.defaultModel->name));
	Q_strncpyz(ri.scn.defaultModel->bareName, "***r_defaultModel***", sizeof(ri.scn.defaultModel->bareName));

	// Generate mesh data
	ri.scn.defaultModel->modelData = (mInternalModel_t*)R_ModAlloc(ri.scn.defaultModel, sizeof(mInternalModel_t));
	refMesh_t *outMesh = (refMesh_t*)R_ModAlloc(ri.scn.defaultModel, sizeof(refMesh_t));
	ri.scn.defaultModel->InternalData()->mesh = outMesh;

	outMesh->numVerts = 6;
	outMesh->numIndexes = 24;
	outMesh->indexArray = (index_t*)R_ModAlloc(ri.scn.defaultModel, sizeof(index_t) * outMesh->numIndexes);
	outMesh->vertexArray = (vec3_t*)R_ModAlloc(ri.scn.defaultModel, sizeof(vec3_t) * outMesh->numVerts);
	outMesh->coordArray = (vec2_t*)R_ModAlloc(ri.scn.defaultModel, sizeof(vec2_t) * outMesh->numVerts);
	outMesh->normalsArray = (vec3_t*)R_ModAlloc(ri.scn.defaultModel, sizeof(vec3_t) * outMesh->numVerts);

	// Create verts
	float *outVerts = (float*)outMesh->vertexArray;

	Vec3Set(outVerts, 0, 0, 16); outVerts += 3;
	Vec3Set(outVerts, 0, 0, -16); outVerts += 3;

	for (int i=0 ; i<4 ; i++)
	{
		float s, c;

		Q_SinCosf((i&3) * (M_PI / 2.0f), &s, &c);
		Vec3Set(outVerts, 16 * c, 16 * s, 0);
		outVerts += 3;
	}

	// Create indexes
	for (int i=0, Side=0 ; i<outMesh->numIndexes ; Side++, i+=3)
	{
		if (Side > 3)
		{
			// Top
			outMesh->indexArray[i+0] = 2 + (Side&3);
			outMesh->indexArray[i+1] = 0;
			outMesh->indexArray[i+2] = 2 + ((Side+1)&3);
		}
		else
		{
			// Bottom
			outMesh->indexArray[i+0] = 1;
			outMesh->indexArray[i+1] = 2 + (Side&3);
			outMesh->indexArray[i+2] = 2 + ((Side+1)&3);
		}
	}

	// Generate normals
	for (int i=0 ; i<outMesh->numIndexes ; i+=3)
	{
		vec3_t v1, v2;
		Vec3Subtract(outMesh->vertexArray[outMesh->indexArray[i+1]], outMesh->vertexArray[outMesh->indexArray[i+0]], v1);
		Vec3Subtract(outMesh->vertexArray[outMesh->indexArray[i+2]], outMesh->vertexArray[outMesh->indexArray[i+0]], v2);

		vec3_t norm;
		CrossProduct(v1, v2, norm);
		VectorNormalizef(norm, norm);

		Vec3Copy(norm, outMesh->normalsArray[outMesh->indexArray[i+0]]);
		Vec3Copy(norm, outMesh->normalsArray[outMesh->indexArray[i+1]]);
		Vec3Copy(norm, outMesh->normalsArray[outMesh->indexArray[i+2]]);
	}

	// Create bounds and radius
	ClearBounds(ri.scn.defaultModel->mins, ri.scn.defaultModel->maxs);
	for (int i=0 ; i<outMesh->numVerts ; i++)
	{
		AddPointToBounds(outMesh->vertexArray[i], ri.scn.defaultModel->mins, ri.scn.defaultModel->maxs);
	}
	ri.scn.defaultModel->radius = RadiusFromBounds(ri.scn.defaultModel->mins, ri.scn.defaultModel->maxs);

	// Unused
	outMesh->colorArray = NULL;
	outMesh->lmCoordArray = NULL;
	outMesh->sVectorsArray = NULL;
	outMesh->tVectorsArray = NULL;

	// Link it into the list
	ri.scn.defaultModel->hashValue = Com_HashGeneric(ri.scn.defaultModel->bareName, MAX_REF_MODEL_HASH);
	ri.scn.defaultModel->hashNext = r_modelHashTree[ri.scn.defaultModel->hashValue];
	r_modelHashTree[ri.scn.defaultModel->hashValue] = ri.scn.defaultModel;

	// Assign worldModel so it's never NULL (so we don't have to NULL check all over the place)
	ri.scn.worldModel = ri.scn.defaultModel;
}

/*
===============
R_ModelInit
===============
*/
void R_ModelInit()
{
	// Register commands/cvars
	flushmap	= Cvar_Register("flushmap",		"0",		0);

	cmd_modelList = Cmd_AddCommand("modellist",	0, R_ModelList_f,		"Prints to the console a list of loaded models and their sizes");

	R_ModelBSPInit();

	memset(r_modelList, 0, sizeof(refModel_t) * MAX_REF_MODELS);
	memset(r_modelHashTree, 0, sizeof(refModel_t *) * MAX_REF_MODEL_HASH);

	// Create our internal models
	R_InitSpecialModels();

	// Check allocation integrity
	Mem_CheckPoolIntegrity(ri.modelSysPool);
}


/*
==================
R_ModelShutdown
==================
*/
void R_ModelShutdown ()
{
	Com_Printf(0, "Model system shutdown:\n");

	// Remove commands
	Cmd_RemoveCommand(cmd_modelList);

	// Free known loaded models
	for (uint32 i=0 ; i<r_numModels ; i++)
		R_FreeModel(&r_modelList[i]);

	// Release pool memory
	uint32 size = Mem_FreePool(ri.modelSysPool);
	Com_Printf(0, "...releasing %u bytes...\n", size);
}