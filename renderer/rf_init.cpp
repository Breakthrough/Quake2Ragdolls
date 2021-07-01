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
// rf_init.c
//

#include "rf_local.h"

/*
=============================================================================

	INIT / SHUTDOWN

=============================================================================
*/

/*
==================
R_MediaInit
==================
*/
void R_MediaInit()
{
	// Chars image/materials
	R_CheckFont();

	// World Caustic materials
	ri.media.worldLavaCaustics = R_RegisterTexture("egl/lavacaustics", -1);
	ri.media.worldSlimeCaustics = R_RegisterTexture("egl/slimecaustics", -1);
	ri.media.worldWaterCaustics = R_RegisterTexture("egl/watercaustics", -1);
}


/*
===============
R_InitRefresh
===============
*/
bool R_InitRefresh()
{
	uint32 startCycles = Sys_Cycles();
	Com_Printf (0, "\n======== Refresh Initialization ========\n");

	ri.frameCount = 1;
	ri.reg.registerFrame = 1;

	// Register renderer cvars
	R_RegisterConfig();

	// Create memory pools
	ri.decalSysPool = Mem_CreatePool("Refresh: Decal system");
	ri.fontSysPool = Mem_CreatePool("Refresh: Font system");
	ri.genericPool = Mem_CreatePool("Refresh: Generic");
	ri.imageSysPool = Mem_CreatePool("Refresh: Image system");
	ri.lightSysPool = Mem_CreatePool("Refresh: Light system");
	ri.matSysPool = Mem_CreatePool("Refresh: Material system");
	ri.modelSysPool = Mem_CreatePool("Refresh: Model system");
	ri.programSysPool = Mem_CreatePool("Refresh: Program system");
	ri.shaderSysPool = Mem_CreatePool("Refresh: Shader system");

	// Fire up the backend
	if (!RB_BackendInit())
	{
		Com_Printf(PRNT_ERROR, "R_Init: Failed to initialize the rendering backend!\n");
		return false;
	}

	Com_Printf (0, "\n=========== Refresh Frontend ===========\n");

	// Map overbrights
	ri.pow2MapOvrbr = r_lmModulate->intVal;
	if (ri.pow2MapOvrbr > 0)
		ri.pow2MapOvrbr = pow(2, ri.pow2MapOvrbr) / 255.0f;
	else
		ri.pow2MapOvrbr = 1.0f / 255.0f;

	// Sub-system init
	R_ImageInit();
	R_ProgramInit();
	R_ShaderInit();
	R_MaterialInit();
	R_FontInit();
	R_MediaInit();
	R_ModelInit();
	R_EntityInit();
	R_WorldInit();
	R_PolyInit();
	R_DecalInit();
	RF_2DInit();

	// Let the frontend do any initialization that relied on frontend systems
	if (!RB_PostFrontEndInit())
		return false;

	Com_Printf(0, "========================================\n");

	// Check for gl errors
	RB_CheckForError("R_Init");
	Com_Printf(0, "\n===== Refresh Initialized %6.2fms =====\n", (Sys_Cycles()-startCycles) * Sys_MSPerCycle());
	return true;
}


/*
===============
R_ShutdownRefresh
===============
*/
void R_ShutdownRefresh(const bool bFull)
{
	Com_Printf(0, "\n------- Refresh Frontend Shutdown ------\n");

	// Remove commands
	R_UnRegisterConfig();

	// Shutdown subsystems
	R_FontShutdown();
	R_MaterialShutdown();
	R_ShaderShutdown();
	R_ProgramShutdown();
	R_ImageShutdown();
	R_ModelShutdown();
	R_WorldShutdown();

	Com_Printf(0, "----------------------------------------\n");

	RB_BackendShutdown(bFull);
}
