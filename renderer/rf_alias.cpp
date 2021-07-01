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
// rf_alias.c
// Alias model rendering
//

#include "rf_local.h"

/*
===============================================================================

	ALIAS PROCESSING

===============================================================================
*/

/*
=============
R_AliasModelBBox
=============
*/
static void R_AliasModelBBox(refEntity_t *ent, vec3_t outMins, vec3_t outMaxs, float &outRadius)
{
	mAliasModel_t *aliasModel = ent->model->AliasData();
	mAliasFrame_t *frame = aliasModel->frames + ent->frame;
	mAliasFrame_t *oldFrame = aliasModel->frames + ent->oldFrame;

	// Compute axially aligned mins and maxs
	if (ent->frame == ent->oldFrame)
	{
		Vec3Copy(frame->mins, outMins);
		Vec3Copy(frame->maxs, outMaxs);
		outRadius = frame->radius;
	}
	else
	{
		// Find the greatest mins/maxs between the current and last frame
		MinMins(frame->mins, oldFrame->mins, outMins);
		MaxMaxs(frame->maxs, oldFrame->maxs, outMaxs);
		outRadius = RadiusFromBounds(outMins, outMaxs);
	}

	// Scale if necessary
	if (ent->scale != 1.0f)
	{
		Vec3Scale(outMins, ent->scale, outMins);
		Vec3Scale(outMaxs, ent->scale, outMaxs);
		outRadius *= ent->scale;
	}
}


/*
=============
R_CullAliasModel
=============
*/
bool R_CullAliasModel(refEntity_t *ent, const uint32 clipFlags)
{
	// Sanity checks
	// Since this function is called before we add to the list or render, catch it here
	mAliasModel_t *aliasModel = ent->model->AliasData();
	if (ent->frame >= aliasModel->numFrames || ent->frame < 0)
	{
		Com_DevPrintf(PRNT_WARNING, "R_AddAliasModelToList: '%s' no such frame '%d'\n", ent->model->name, ent->frame);
		ent->frame = 0;
	}

	if (ent->oldFrame >= aliasModel->numFrames || ent->oldFrame < 0)
	{
		Com_DevPrintf(PRNT_WARNING, "R_AddAliasModelToList: '%s' no such oldFrame '%d'\n", ent->model->name, ent->oldFrame);
		ent->oldFrame = 0;
	}

	if (ent->flags & RF_WEAPONMODEL)
		return (ri.scn.viewType == RVT_MIRROR);
	if (ent->flags & RF_VIEWERMODEL)
		return !(ri.scn.viewType == RVT_MIRROR || ri.scn.viewType == RVT_SHADOWMAP);

	if (r_noCull->intVal)
		return false;

	// Get our bounds
	vec3_t Mins, Maxs;
	float Radius;
	R_AliasModelBBox(ent, Mins, Maxs, Radius);

	// Compute and rotate a full bounding box
	vec3_t BBox[8];
	R_TransformBoundsToBBox(ent->origin, ent->axis, Mins, Maxs, BBox);
	if (R_CullTransformedBox(BBox, clipFlags))
	{
		if (!ri.scn.bDrawingMeshOutlines)
			ri.pc.cullAlias[CULL_PASS]++;
		return true;
	}
	else
	{
		if (!ri.scn.bDrawingMeshOutlines)
			ri.pc.cullAlias[CULL_FAIL]++;
	}

	// Mirror/portal culling
	if (ri.scn.viewType == RVT_MIRROR || ri.scn.viewType == RVT_PORTAL)
	{
		if (PlaneDiff(ent->origin, &ri.scn.clipPlane) < -Radius)
		{
			if (!ri.scn.bDrawingMeshOutlines)
			{
				ri.pc.cullRadius[CULL_PASS]++;
				ri.pc.cullAlias[CULL_PASS]++;
			}
			return true;
		}

		if (!ri.scn.bDrawingMeshOutlines)
		{
			ri.pc.cullRadius[CULL_FAIL]++;
			ri.pc.cullAlias[CULL_FAIL]++;
		}
	}

	// Couldn't frustum cull
	return false;
}


/*
=============
R_AddAliasModelToList
=============
*/
void R_AddAliasModelToList(refEntity_t *ent)
{
	// Get our radius so fog can be applied
	vec3_t Mins, Maxs;
	float Radius;
	R_AliasModelBBox(ent, Mins, Maxs, Radius);

	// Fog
	mQ3BspFog_t *Fog = R_FogForSphere(ent->origin, Radius);

	// Add to list
	mAliasModel_t *model = ent->model->AliasData();
	for (int meshNum=0 ; meshNum<model->numMeshes ; meshNum++)
	{
		mAliasMesh_t *aliasMesh = &model->meshes[meshNum];

		// Find the skin for this mesh
		refMaterial_t *mat;
		if (ent->skin)
		{
			// Custom player skin
			mat = ent->skin;
		}
		else
		{
			if (ent->skinNum >= MD2_MAX_SKINS)
			{
				// Server's only send MD2 skinNums anyways
				mat = aliasMesh->skins[0].material;
			}
			else if (ent->skinNum >= 0 && ent->skinNum < aliasMesh->numSkins)
			{
				mat = aliasMesh->skins[ent->skinNum].material;
				if (!mat)
					mat = aliasMesh->skins[0].material;
			}
			else
			{
				mat = aliasMesh->skins[0].material;

				if (aliasMesh->skins[0].material == null)
					mat = null;
			}
		}

		if (!mat)
		{
			Com_DevPrintf(PRNT_WARNING, "R_AddAliasModelToList: '%s' has a NULL material\n", ent->model->name);
			mat = ri.media.noMaterial;
			//return;
		}

		ri.scn.currentList->AddToList(MBT_ALIAS, aliasMesh, mat, ent->matTime, ent, Fog, 0);
	}
}


/*
=============
R_DrawAliasModel
=============
*/
void R_DrawAliasModel(refMeshBuffer *mb, const meshFeatures_t features)
{
	refEntity_t *ent = mb->DecodeEntity();

	refMaterial_t *mat = mb->DecodeMaterial();
	mAliasModel_t *aliasModel = ent->model->AliasData();
	mAliasMesh_t *aliasMesh = (mAliasMesh_t *)mb->mesh;
	mAliasFrame_t *frame = aliasModel->frames + ent->frame;
	mAliasFrame_t *oldFrame = aliasModel->frames + ent->oldFrame;

	if (!(ent->flags & RF_FORCENOLOD) &&
		aliasModel->numLODs != 0)
	{
		float dist = Vec3DistSquared(ri.def.viewOrigin, ent->origin);

		for (int i = aliasModel->numLODs - 1; i >= 0; --i)
		{
			if (dist > aliasModel->modelLODs[i].distance)
			{
				int meshIndex = aliasMesh - ent->model->AliasData()->meshes;
				aliasMesh = &aliasModel->modelLODs[i].model->AliasData()->meshes[meshIndex];
				break;
			}
		}
	}

	// Interpolation calculations
	const float backLerp = (!r_lerpmodels->intVal || mat->flags & MAT_NOLERP) ? 0.0f : ent->backLerp;
	const float frontLerp = 1.0f - backLerp;

	vec3_t move, delta;
	Vec3Subtract(ent->oldOrigin, ent->origin, delta);
	Matrix3_TransformVector(ent->axis, delta, move);
	Vec3Add(move, oldFrame->translate, move);

	move[0] = frame->translate[0] + (move[0] - frame->translate[0]) * backLerp;
	move[1] = frame->translate[1] + (move[1] - frame->translate[1]) * backLerp;
	move[2] = frame->translate[2] + (move[2] - frame->translate[2]) * backLerp;

	// Optimal route
	if (ent->frame == ent->oldFrame)
	{
		vec3_t scale;
		scale[0] = frame->scale[0] * ent->scale;
		scale[1] = frame->scale[1] * ent->scale;
		scale[2] = frame->scale[2] * ent->scale;

		// Store normals and vertices
		if (features & MF_NORMALS)
		{
			mAliasVertex_t *verts = aliasMesh->vertexes + (ent->frame * aliasMesh->numVerts);
			for (int i=0 ; i<aliasMesh->numVerts ; i++, verts++)
			{
				rb.batch.vertices[i][0] = move[0] + verts->point[0]*scale[0];
				rb.batch.vertices[i][1] = move[1] + verts->point[1]*scale[1];
				rb.batch.vertices[i][2] = move[2] + verts->point[2]*scale[2];

				LatLongToNorm(verts->latLong, rb.batch.normals[i]);
			}
		}
		else
		{
			mAliasVertex_t *verts = aliasMesh->vertexes + (ent->frame * aliasMesh->numVerts);
			for (int i=0 ; i<aliasMesh->numVerts ; i++, verts++)
			{
				rb.batch.vertices[i][0] = move[0] + verts->point[0]*scale[0];
				rb.batch.vertices[i][1] = move[1] + verts->point[1]*scale[1];
				rb.batch.vertices[i][2] = move[2] + verts->point[2]*scale[2];
			}
		}
	}
	else
	{
		mAliasVertex_t *verts = aliasMesh->vertexes + (ent->frame * aliasMesh->numVerts);
		mAliasVertex_t *oldVerts = aliasMesh->vertexes + (ent->oldFrame * aliasMesh->numVerts);

		vec3_t scale;
		scale[0] = (frontLerp * frame->scale[0]) * ent->scale;
		scale[1] = (frontLerp * frame->scale[1]) * ent->scale;
		scale[2] = (frontLerp * frame->scale[2]) * ent->scale;

		vec3_t oldScale;
		oldScale[0] = (backLerp * oldFrame->scale[0]) * ent->scale;
		oldScale[1] = (backLerp * oldFrame->scale[1]) * ent->scale;
		oldScale[2] = (backLerp * oldFrame->scale[2]) * ent->scale;

		// Interpolate normals and vertices
		if (features & MF_NORMALS)
		{
			for (int i=0 ; i<aliasMesh->numVerts ; i++, verts++, oldVerts++)
			{
				rb.batch.vertices[i][0] = move[0] + verts->point[0]*scale[0] + oldVerts->point[0]*oldScale[0];
				rb.batch.vertices[i][1] = move[1] + verts->point[1]*scale[1] + oldVerts->point[1]*oldScale[1];
				rb.batch.vertices[i][2] = move[2] + verts->point[2]*scale[2] + oldVerts->point[2]*oldScale[2];

				vec3_t normal, oldNormal;
				LatLongToNorm(verts->latLong, normal);
				LatLongToNorm(oldVerts->latLong, oldNormal);

				rb.batch.normals[i][0] = normal[0] + (oldNormal[0] - normal[0]) * backLerp;
				rb.batch.normals[i][1] = normal[1] + (oldNormal[1] - normal[1]) * backLerp;
				rb.batch.normals[i][2] = normal[2] + (oldNormal[2] - normal[2]) * backLerp;

				VectorNormalizeFastf(rb.batch.normals[i]);
			}
		}
		else
		{
			for (int i=0 ; i<aliasMesh->numVerts ; i++, verts++, oldVerts++)
			{
				rb.batch.vertices[i][0] = move[0] + verts->point[0]*scale[0] + oldVerts->point[0]*oldScale[0];
				rb.batch.vertices[i][1] = move[1] + verts->point[1]*scale[1] + oldVerts->point[1]*oldScale[1];
				rb.batch.vertices[i][2] = move[2] + verts->point[2]*scale[2] + oldVerts->point[2]*oldScale[2];
			}
		}
	}

	// Fill out mesh properties
	refMesh_t outMesh;

	outMesh.numIndexes = aliasMesh->numTris * 3;
	outMesh.numVerts = aliasMesh->numVerts;

	outMesh.colorArray = rb.batch.colors;
	outMesh.coordArray = aliasMesh->coords;
	outMesh.indexArray = aliasMesh->indexes;
	outMesh.lmCoordArray = NULL;
	outMesh.normalsArray = rb.batch.normals;
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

	// Push the mesh
	RB_PushMesh(&outMesh, features);
	RB_RenderMeshBuffer(mb);
}
