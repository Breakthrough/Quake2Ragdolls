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
// rf_modelLocal.h
// Private to the rf_model*.cpp files
//

#include "rf_local.h"

#define MAX_REF_MODELS		1024
#define MAX_REF_MODEL_HASH	(MAX_REF_MODELS/4)

#define R_ModAlloc(model,size) _Mem_Alloc((size),ri.modelSysPool,(model)->memTag,__FILE__,__LINE__)

//
// rf_modelAlias.cpp
//

bool R_LoadMD2Model(refModel_t *model);
bool R_LoadMD3Model(refModel_t *model);
bool R_LoadMD2EModel(refModel_t *model);

//
// rf_modelBSP.cpp
//

bool R_LoadQ2BSPModel(refModel_t *model, byte *buffer);
bool R_LoadQ3BSPModel(refModel_t *model, byte *buffer);

void R_ModelBSPInit();
