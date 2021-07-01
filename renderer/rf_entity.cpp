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
// rf_entity.cpp
//

#include "rf_local.h"

/*
=============================================================================

	ENTITY MANAGEMENT

=============================================================================
*/

static byte r_entityCullBits[MAX_REF_ENTITIES/8];

static TList<refEntity_t*> r_bmodelEntities;

/*
=============
R_CategorizeEntityList
=============
*/
void R_CategorizeEntityList()
{
	r_bmodelEntities.Clear();

	if (!r_drawEntities->intVal)
		return;

	for (uint32 i=0 ; i<ri.scn.numEntities ; i++)
	{
		refEntity_t *ent = &ri.scn.entityList[ENTLIST_OFFSET+i];
		if (!ent->model)
			ent->model = ri.scn.defaultModel;

		switch (ent->model->type)
		{
		case MODEL_MD2:
		case MODEL_MD3:
			if (!(ent->flags & RF_NOSHADOW))
				RB_AddShadowCaster(ent);
			break;

		case MODEL_Q2BSP:
		case MODEL_Q3BSP:
			r_bmodelEntities.Add(ent);
			break;
		}
	}
}


/*
=============
R_AddBModelsToList
=============
*/
void R_AddBModelsToList()
{
	if (!r_drawEntities->intVal)
		return;

	for (uint32 i=0 ; i<r_bmodelEntities.Count() ; i++)
	{
		refEntity_t *ent = r_bmodelEntities[i];
		if (!R_CullBrushModel(ent, ri.scn.clipFlags))
		{
			switch (ent->model->type)
			{
			case MODEL_Q2BSP:
				R_AddQ2BrushModel(ent);
				break;

			case MODEL_Q3BSP:
				R_AddQ3BrushModel(ent);
				break;

			default:
				assert(0);
				break;
			}
		}
	}
}


/*
=============
R_CullEntityList
=============
*/
void R_CullEntityList()
{
	if (!r_drawEntities->intVal)
		return;

	memset(r_entityCullBits, 0, sizeof(r_entityCullBits));
	for (uint32 i=0 ; i<ri.scn.numEntities ; i++)
	{
		refEntity_t *ent = &ri.scn.entityList[ENTLIST_OFFSET+i];

		bool bCulled = false;
		switch (ent->model->type)
		{
		case MODEL_INTERNAL:
			break;

		case MODEL_MD2:
		case MODEL_MD3:
			bCulled = R_CullAliasModel(ent, ri.scn.clipFlags);
			break;
			
		case MODEL_Q2BSP:
		case MODEL_Q3BSP:
			break;

		case MODEL_SP2:
			break;

		case MODEL_BAD:
		default:
			bCulled = true;
			assert(0);
			break;
		}

		if (bCulled)
			r_entityCullBits[i>>3] |= BIT(i&7);
	}
}


/*
=============
R_AddEntitiesToList
=============
*/
void R_AddEntitiesToList()
{
	if (!r_drawEntities->intVal)
		return;

	// Add all entities to the list
	for (uint32 i=0 ; i<ri.scn.numEntities ; i++)
	{
		refEntity_t *ent = &ri.scn.entityList[ENTLIST_OFFSET+i];
		if (!(r_entityCullBits[i>>3] & BIT(i&7)))
		{
			switch (ent->model->type)
			{
			case MODEL_INTERNAL:
				R_AddInternalModelToList(ent);
				break;

			case MODEL_MD2:
			case MODEL_MD3:
				R_AddAliasModelToList(ent);
				break;

			case MODEL_Q2BSP:
			case MODEL_Q3BSP:
				break;

			case MODEL_SP2:
				R_AddSP2ModelToList(ent);
				break;
			}
		}
	}
}


/*
=============
R_EntityInit
=============
*/
void R_EntityInit()
{
	// Reserve a spot for the default entity
	ri.scn.defaultEntity = &ri.scn.entityList[0];

	memset(ri.scn.defaultEntity, 0, sizeof(refEntity_t));
	ri.scn.defaultEntity->model = ri.scn.defaultModel;
	ri.scn.defaultEntity->scale = 1.0f;
	Matrix3_Identity(ri.scn.defaultEntity->axis);
	Vec4Set(ri.scn.defaultEntity->color, 255, 255, 255, 255);

	// And for the world entity
	ri.scn.worldEntity = &ri.scn.entityList[1];
	memcpy(ri.scn.worldEntity, ri.scn.defaultEntity, sizeof(refEntity_t));
}
