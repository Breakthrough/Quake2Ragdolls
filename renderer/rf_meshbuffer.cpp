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
// rf_meshbuffer.c
//

#include "rf_local.h"

#define QSORT_MAX_STACKDEPTH 4096

#define R_MBCopy(in,out) \
	(\
		(out).sortValue = (in).sortValue, \
		(out).matTime = (in).matTime, \
		(out).mesh = (in).mesh, \
		(out).infoKey = (in).infoKey \
	)

#define R_MBCmp(mb1,mb2) ((mb1).sortValue > (mb2).sortValue)

/*
=============
refMeshBuffer::Setup
=============
*/
void refMeshBuffer::Setup(const EMeshBufferType inMeshType, void *inMeshData,
					refMaterial_t *inMaterial, const float inMatTime,
					refEntity_t *inEntity,
					struct mQ3BspFog_t *inFog,
					const int inInfoKey)
{
	if (!inEntity)
		inEntity = ri.scn.defaultEntity;

	// Store values
	matTime = inMatTime;
	mesh = inMeshData;
	infoKey = inInfoKey;

	// Sort by mesh type
	sortValue = (uint64)(inMeshType & (MBT_MAX-1));					// [0 - 2^4]

	// Sort by entity
	sortValue |= ((uint64)(inEntity - ri.scn.entityList)+1) << 5;	// [2^5 - 2^17]

	// Sort by material
	sortValue |= ((uint64)(inMaterial - r_materialList)+1) << 18;	// [2^18 - 2^31]

	// Add meshType specific flags
	switch(inMeshType)
	{
	case MBT_Q2BSP:
		{
			mBspSurface_t *surf = (mBspSurface_t *)mesh;

			// Sort by lightmap texture
			sortValue |= (uint64)(surf->lmTexNum+1) << 32;			// [2^32 - 2^38]

			// No need to sort by dlightBits because Q2BSP uses raw lightmap updates for this

			// Sort by shadow bits
			if (surf->shadowBits && surf->shadowFrame == ri.frameCount)
				sortValue |= (uint64)1 << 40;						// [2^40]
		}
		break;

	case MBT_Q3BSP:
		{
			mBspSurface_t *surf = (mBspSurface_t *)mesh;

			// Sort by lightmap texture
			sortValue |= (uint64)(surf->lmTexNum+1) << 32;			// [2^32 - 2^38]

			// Sort by dlightbits
			if (surf->dLightBits && surf->dLightFrame == ri.frameCount)
				sortValue |= (uint64)1 << 39;						// [2^39]

			// Sort by shadow bits
			if (surf->shadowBits && surf->shadowFrame == ri.frameCount)
				sortValue |= (uint64)1 << 40;						// [2^40]
		}
		break;

	case MBT_BAD:
	case MBT_MAX:
		assert(0);
		break;
	}

	// Sort by fog
	if (inFog)														// [2^45 - 2^56]
		sortValue |= ((uint64)(inFog - ri.scn.worldModel->Q3BSPData()->fogs)+1) << 45;

	// Sort by the material's sort key
	if (inEntity->flags & RF_TRANSLUCENT && inMaterial->sortKey == MAT_SORT_ENTITY) // Kind of a hack, kinda necessary...
		sortValue |= (uint64)(inMaterial->sortKey+2) << 57;				// [2^57 - 2^62]
	else
		sortValue |= (uint64)(inMaterial->sortKey+1) << 57;				// [2^57 - 2^62]

	// Stupidity check
	assert(DecodeMeshType() == inMeshType);
	assert(DecodeEntity() == inEntity);
	assert(DecodeMaterial() == inMaterial);
	assert(DecodeFog() == inFog);
}

/*
=============
refMeshBuffer::DecodeEntity
=============
*/
refEntity_t *refMeshBuffer::DecodeEntity()
{
	return ri.scn.entityList + (((sortValue >> 5) & (MAX_REF_ENTITIES-1))) - 1;
}

/*
=============
refMeshBuffer::DecodeFog
=============
*/
struct mQ3BspFog_t *refMeshBuffer::DecodeFog()
{
	if (ri.scn.worldModel->type == MODEL_Q3BSP)
	{
		uint64 Index = (uint64)((sortValue >> 45) & (Q3BSP_MAX_FOGS-1));
		if (Index != 0)
			return ri.scn.worldModel->Q3BSPData()->fogs + Index - 1;
	}

	return NULL;
}

/*
=============
refMeshBuffer::DecodeMaterial
=============
*/
refMaterial_t *refMeshBuffer::DecodeMaterial()
{
	return r_materialList + (((sortValue >> 18) & (MAX_MATERIALS-1))) - 1;
}


/*
=============
refMeshList::AddToList
=============
*/
refMeshBuffer *refMeshList::AddToList(EMeshBufferType meshType, void *mesh, refMaterial_t *mat, float matTime, refEntity_t *ent, struct mQ3BspFog_t *fog, const int infoKey)
{
	assert(meshType >= 0 && meshType < MBT_MAX);

	// Check if it qualifies to be added to the list
	if (!mat)
		return NULL;
	else if (!mat->numPasses)
		return NULL;
	if (!mesh)
		return NULL;

	// Choose the buffer to append to
	refMeshBuffer *Result;
	switch(mat->sortKey)
	{
	case MAT_SORT_SKY:
	case MAT_SORT_OPAQUE:
		if (meshBufferOpaque.Count() >= MAX_MESH_BUFFER)
			return NULL;

		meshBufferOpaque.Add(refMeshBuffer());
		Result = &meshBufferOpaque[meshBufferOpaque.Count() - 1];
		break;

	case MAT_SORT_POSTPROCESS:
		if (meshBufferPostProcess.Count() >= MAX_POSTPROC_BUFFER)
			return NULL;

		meshBufferOpaque.Add(refMeshBuffer());
		Result = &meshBufferPostProcess[meshBufferPostProcess.Count() - 1];
		break;

	case MAT_SORT_PORTAL:
		if (ri.scn.viewType == RVT_MIRROR || ri.scn.viewType == RVT_PORTAL)
			return NULL;
		// Fall through

	default:
		if (meshBufferAdditive.Count() >= MAX_ADDITIVE_BUFFER)
			return NULL;

		meshBufferAdditive.Add(refMeshBuffer());
		Result = &meshBufferAdditive[meshBufferAdditive.Count() - 1];
		break;
	}

	// Fill it in
	Result->Setup(meshType, mesh, mat, matTime, ent, fog, infoKey);
	return Result;
}


/*
================
R_QSortMeshBuffers

Quick sort
================
*/
static void R_QSortMeshBuffers(TList<refMeshBuffer> &meshes, int Li, int Ri)
{
	int li, ri, stackdepth = 0, total = Ri + 1;
	refMeshBuffer median, tempbuf;
	int lstack[QSORT_MAX_STACKDEPTH], rstack[QSORT_MAX_STACKDEPTH];

mark0:
	if (Ri-Li > 8)
	{
		li = Li;
		ri = Ri;

		R_MBCopy(meshes[(Li+Ri) >> 1], median);

		if (R_MBCmp(meshes[Li], median))
		{
			if (R_MBCmp(meshes[Ri], meshes[Li]))
				R_MBCopy(meshes[Li], median);
		}
		else if (R_MBCmp(median, meshes[Ri]))
		{
			R_MBCopy(meshes[Ri], median);
		}

		do
		{
			while (R_MBCmp(median, meshes[li]))
				li++;
			while (R_MBCmp(meshes[ri], median))
				ri--;

			if (li <= ri)
			{
				R_MBCopy(meshes[ri], tempbuf);
				R_MBCopy(meshes[li], meshes[ri]);
				R_MBCopy(tempbuf, meshes[li]);

				li++;
				ri--;
			}
		}
		while(li < ri);

		if (Li < ri && stackdepth < QSORT_MAX_STACKDEPTH)
		{
			lstack[stackdepth] = li;
			rstack[stackdepth] = Ri;
			stackdepth++;
			li = Li;
			Ri = ri;
			goto mark0;
		}

		if (li < Ri)
		{
			Li = li;
			goto mark0;
		}
	}
	if (stackdepth)
	{
		--stackdepth;
		Ri = ri = rstack[stackdepth];
		Li = li = lstack[stackdepth];
		goto mark0;
	}

	for (li=1 ; li<total ; li++)
	{
		R_MBCopy(meshes[li], tempbuf);
		ri = li - 1;

		while (ri >= 0 && R_MBCmp(meshes[ri], tempbuf))
		{
			R_MBCopy(meshes[ri], meshes[ri+1]);
			ri--;
		}
		if (li != ri+1)
			R_MBCopy(tempbuf, meshes[ri+1]);
	}
}


/*
================
R_ISortMeshes

Insertion sort
================
*/
template <typename T>
static void R_ISortMeshBuffers(TList<T> &meshes, int numMeshes)
{
	for (int i=1 ; i<numMeshes ; i++)
	{
		refMeshBuffer tempbuf;
		R_MBCopy(meshes[i], tempbuf);

		int j = i - 1;
		while (j >= 0 && R_MBCmp(meshes[j], tempbuf))
		{
			R_MBCopy(meshes[j], meshes[j+1]);
			j--;
		}
		if (i != j+1)
			R_MBCopy(tempbuf, meshes[j+1]);
	}
}


/*
=============
refMeshList::SortList
=============
*/
void refMeshList::SortList()
{
	if (r_debugSorting->intVal)
		return;

	// Sort meshes
	if (meshBufferOpaque.Count() != 0)
		R_QSortMeshBuffers(meshBufferOpaque, 0, meshBufferOpaque.Count() - 1);

	// Sort additive meshes
	if (meshBufferAdditive.Count() != 0)
		R_ISortMeshBuffers(meshBufferAdditive, meshBufferAdditive.Count());

	// Sort post-process meshes
	if (meshBufferPostProcess.Count() != 0)
		R_ISortMeshBuffers(meshBufferPostProcess, meshBufferPostProcess.Count());
}


/*
=============
refMeshList::BatchMeshBuffer
=============
*/
void refMeshList::BatchMeshBuffer(refMeshBuffer *mb, refMeshBuffer *nextMB)
{
	refMaterial_t *material = mb->DecodeMaterial();

	// Check if it's a sky surface
	if (material->flags & MAT_SKY)
	{
		if (!bSkyDrawn)
		{
			R_DrawSky(mb);
			bSkyDrawn = true;
		}
		return;
	}

	// Decode some values we'll need
	EMeshBufferType meshType = mb->DecodeMeshType();
	refEntity_t *ent = mb->DecodeEntity();

	// Render it!

	// Note: comparing mesh buffer sortValue variables is the same as comparing:
	// - Mesh type
	// - Entity
	// - Material
	// - Lightmap Texnum
	// - Fog
	// - Material sort key
	/// See refMeshBuffer::Setup

	meshFeatures_t features = material->features;
	switch (meshType)
	{
	case MBT_ALIAS:
		{
			// Set features
			features |= MF_NONBATCHED;
			if (ri.scn.viewType == RVT_SHADOWMAP)
			{
				features &= ~(MF_COLORS|MF_STVECTORS|MF_ENABLENORMALS);
				if (!(material->features & MF_DEFORMVS))
					features &= ~MF_NORMALS;
			}
			else
			{
				if (features & MF_STVECTORS || gl_shownormals->intVal)
					features |= MF_NORMALS;
			}

			RB_RotateForEntity(ent);
			R_DrawAliasModel(mb, features);
		}
		break;

	case MBT_DECAL:
		{
			// FIXME: adjust for bmodel decals here

			// Set features
			if (gl_shownormals->intVal)
				features |= MF_NORMALS;
			if (r_debugBatching->intVal)
				features |= MF_NONBATCHED;

			// Push the mesh
			R_PushDecal(mb, features);

			if (features & MF_NONBATCHED
			|| !nextMB
			|| nextMB->sortValue != mb->sortValue
			|| nextMB->matTime != mb->matTime
			|| R_DecalOverflow(nextMB))
			{
				RB_LoadModelIdentity();
				RB_RenderMeshBuffer(mb);
			}
		}
		break;

	case MBT_INTERNAL:
		{
			// Set features
			features |= MF_NONBATCHED;
			if (gl_shownormals->intVal)
				features |= MF_NORMALS;

			RB_RotateForEntity(ent);
			R_DrawInternalModel(mb, features);
		}
		break;

	case MBT_POLY:
		{
			// Set features
			if (r_debugBatching->intVal)
				features |= MF_NONBATCHED;

			// Push the mesh
			R_PushPoly(mb, features);

			if (features & MF_NONBATCHED
			|| !nextMB
			|| nextMB->sortValue != mb->sortValue
			|| nextMB->matTime != mb->matTime
			|| R_PolyOverflow(nextMB))
			{
				RB_LoadModelIdentity();
				RB_RenderMeshBuffer(mb);
			}
		}
		break;

	case MBT_Q2BSP:
		{
			if (!ri.scn.bDrawingMeshOutlines)
				ri.pc.worldPolys++;

			// Find the surface
			mBspSurface_t *surf = (mBspSurface_t *)mb->mesh;
			mBspSurface_t *nextSurf = (nextMB && nextMB->DecodeMeshType() == MBT_Q2BSP) ? (mBspSurface_t *)nextMB->mesh : NULL;

			// Set features
			if (gl_shownormals->intVal)
				features |= MF_NORMALS;
			if (r_debugBatching->intVal)
				features |= MF_NONBATCHED;

			bool bCantMerge = false;
			if (features & MF_NONBATCHED
			|| !nextSurf
			|| nextMB->sortValue != mb->sortValue
			|| nextMB->matTime != mb->matTime
			|| nextSurf->shadowBits != surf->shadowBits
			|| RB_BackendOverflow2(surf->mesh, nextSurf->mesh))
			{
				bCantMerge = true;

				// Save batching the mesh (which copies to temporary buffers) if we're not already batching
				if (!rb.numVerts)
					features |= MF_NONBATCHED;
			}

			// Push the mesh
			RB_PushMesh(surf->mesh, features);

			// Update lightmap
			R_Q2BSP_UpdateLightmap(ent, surf);

			// Render if necessary
			if (bCantMerge)
			{
				RB_RotateForEntity(ent);
				RB_RenderMeshBuffer(mb);
			}
		}
		break;

	case MBT_Q3BSP:
		{
			if (!ri.scn.bDrawingMeshOutlines)
				ri.pc.worldPolys++;

			// Find the surface
			mBspSurface_t *surf = (mBspSurface_t *)mb->mesh;
			mBspSurface_t *nextSurf = (nextMB && nextMB->DecodeMeshType() == MBT_Q3BSP) ? (mBspSurface_t *)nextMB->mesh : NULL;

			// Set features
			if (gl_shownormals->intVal)
				features |= MF_NORMALS;
			if (r_debugBatching->intVal)
				features |= MF_NONBATCHED;

			bool bCantMerge = false;
			if (features & MF_NONBATCHED
			|| !nextSurf
			|| nextMB->sortValue != mb->sortValue
			|| nextMB->matTime != mb->matTime
			|| nextSurf->dLightBits != surf->dLightBits
			|| nextSurf->shadowBits != surf->shadowBits
			|| RB_BackendOverflow2(surf->mesh, nextSurf->mesh))
			{
				bCantMerge = true;

				// Save batching the mesh (which copies to temporary buffers) if we're not already batching
				if (!rb.numVerts)
					features |= MF_NONBATCHED;
			}

			// Push the mesh
			RB_PushMesh(surf->mesh, features);

			// Render if necessary
			if (bCantMerge)
			{
				RB_RotateForEntity(ent);
				RB_RenderMeshBuffer(mb);
			}
		}
		break;

	case MBT_Q3BSP_FLARE:
		{
			if (!ri.scn.bDrawingMeshOutlines)
				ri.pc.worldPolys++;

			mBspSurface_t *surf = (mBspSurface_t *)mb->mesh;
			mBspSurface_t *nextSurf = (nextMB && nextMB->DecodeMeshType() == MBT_Q3BSP_FLARE) ? (mBspSurface_t *)nextMB->mesh : NULL;

			// Set features
			if (gl_shownormals->intVal)
				features |= MF_NORMALS;
			if (r_debugBatching->intVal)
				features |= MF_NONBATCHED;

			bool bCantMerge = false;
			if (features & MF_NONBATCHED
			|| !nextSurf
			|| nextMB->sortValue != mb->sortValue
			|| nextMB->matTime != mb->matTime
			|| nextSurf->dLightBits != surf->dLightBits
			|| nextSurf->shadowBits != surf->shadowBits
			|| R_FlareOverflow())
			{
				bCantMerge = true;

				// Save batching the mesh (which copies to temporary buffers) if we're not already batching
				if (!rb.numVerts)
					features |= MF_NONBATCHED;
			}

			// Push the mesh
			R_PushFlare(mb, features);

			// Render if necessary
			if (bCantMerge)
			{
				RB_LoadModelIdentity();
				RB_RenderMeshBuffer(mb);
			}
		}
		break;

	case MBT_SP2:
		{
			// Set features
			features |= MF_NONBATCHED;
			if (gl_shownormals->intVal)
				features |= MF_NORMALS;

			// Push and render the mesh
			RB_LoadModelIdentity();
			R_DrawSP2Model(mb, features);
		}
		break;

	case MBT_BAD:
	case MBT_MAX:
		assert(0);
		break;
	}
}


/*
=============
refMeshList::DrawList
=============
*/
void refMeshList::DrawList()
{
	// Start
	RB_StartRendering();

	// Only draw the sky once per list
	bSkyDrawn = false;

	// Draw meshes
	if (meshBufferOpaque.Count() != 0)
	{
		refMeshBuffer *mb = &meshBufferOpaque[0];
		for (uint32 i=0 ; i<meshBufferOpaque.Count()-1 ; i++, mb++)
			BatchMeshBuffer(mb, mb+1);
		BatchMeshBuffer(mb, NULL);
	}

	// Draw additive meshes
	if (meshBufferAdditive.Count() != 0)
	{
		// Render meshes
		refMeshBuffer *mb = &meshBufferAdditive[0];
		for (uint32 i=0 ; i<meshBufferAdditive.Count()-1 ; i++, mb++)
			BatchMeshBuffer(mb, mb+1);
		BatchMeshBuffer(mb, NULL);
	}

	// Draw post process meshes
	if (meshBufferPostProcess.Count() != 0)
	{
		refMeshBuffer *mb = &meshBufferPostProcess[0];
		for (uint32 i=0 ; i<meshBufferPostProcess.Count()-1 ; i++, mb++)
			BatchMeshBuffer(mb, mb+1);
		BatchMeshBuffer(mb, NULL);
	}

	// Clear state
	RB_FinishRendering();
}


/*
=============
refMeshList::DrawOutlines
=============
*/
void refMeshList::DrawOutlines()
{
	if (!gl_showtris->intVal && !gl_shownormals->intVal)
		return;

	ri.scn.bDrawingMeshOutlines = true;
	DrawList();
	ri.scn.bDrawingMeshOutlines = false;
}
