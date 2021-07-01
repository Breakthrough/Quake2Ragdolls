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
// rb_local.h
// Local to the rendering backend.
//

#include "r_local.h"

/*
=============================================================================

	IMPLEMENTATION SPECIFIC

=============================================================================
*/

//
// imp_vid.cpp
//

extern cVar_t				*vid_fullscreen;
extern cVar_t				*vid_xpos;
extern cVar_t				*vid_ypos;

//
// glimp_imp.cpp
//

void GLimp_BeginFrame();
void GLimp_EndFrame();

void GLimp_AppActivate(const bool bActive);

void *GLimp_GetProcAddress(const char *procName);

void GLimp_Shutdown(const bool full);
bool GLimp_Init();
bool GLimp_AttemptMode(const bool fullScreen, const int width, const int height);

/*
===============================================================================

	FUNCTION PROTOTYPES

===============================================================================
*/

//
// rb_render.cpp
//

bool RB_RenderInit();
void RB_RenderShutdown();

//
// rb_state.cpp
//

void RB_SetStateMask(const stateBit_t newMask);
void RB_ResetStateMask();
void RB_StateForBits(stateBit_t newBits);

void RB_TextureEnv(GLfloat mode);
void RB_SetTextureGen(const uint32 Coord, const int Mode);
void RB_SetTexCoordArrayMode(const int NewMode);
void RB_LoadTexMatrix(mat4x4_t m);
void RB_LoadIdentityTexMatrix();

void RB_BindProgram(struct refProgram_t *program);

//
// rb_shadow.cpp
//

extern uint32 rb_entShadowBits[MAX_REF_ENTITIES];

bool RB_InitShadows();
void RB_ShutdownShadows();

void RB_BloomInit ();
void RB_BloomShutdown ();
