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
// rb_shadow.cpp
//

#include "rb_local.h"

struct refShadowGroup_t
{
	shadowBit_t			bit;
	refImage_t			*depthTexture;

	vec3_t				origin;
};

refShadowGroup_t rb_shadowGroups[MAX_SHADOW_GROUPS];
byte rb_shadowCullBits[MAX_SHADOW_GROUPS/8];
uint32 rb_numShadowGroups;

uint32 rb_entShadowBits[MAX_REF_ENTITIES];

/*
=============
RB_SoftShadowsEnabled
=============
*/
static inline bool RB_SoftShadowsEnabled()
{
	return (
		!(ri.def.rdFlags & RDF_NOWORLDMODEL)
		&& ri.config.ext.bShaders
		&& ri.config.ext.bDepthTexture
		&& ri.config.ext.bARBShadow
		&& ri.config.ext.bFrameBufferObject
	);
}


/*
=============
RB_ClearShadowMaps
=============
*/
void RB_ClearShadowMaps()
{
	rb_numShadowGroups = 0;

	if (!RB_SoftShadowsEnabled())
		return;

	memset(rb_shadowGroups, 0, sizeof(rb_shadowGroups));
	memset(rb_entShadowBits, 0, sizeof(rb_entShadowBits));
}


/*
=============
RB_AddShadowCaster
=============
*/
void RB_AddShadowCaster(refEntity_t *ent)
{
	if (!RB_SoftShadowsEnabled())
		return;
}


/*
=============
RB_CullShadowMaps
=============
*/
void RB_CullShadowMaps()
{
	if (!RB_SoftShadowsEnabled())
		return;

	memset(rb_shadowCullBits, 0, sizeof(rb_shadowCullBits));

	for (uint32 num=0 ; num<rb_numShadowGroups ; num++)
	{
		refShadowGroup_t *group = &rb_shadowGroups[num];
	}
}


/*
=============
RB_RenderShadowMaps
=============
*/
void RB_RenderShadowMaps()
{
	if (!RB_SoftShadowsEnabled())
		return;
}


/*
=============
RB_InitShadows
=============
*/
bool RB_InitShadows()
{
	memset(rb_shadowGroups, 0, sizeof(rb_shadowGroups));
	rb_numShadowGroups = 0;

	memset(rb_entShadowBits, 0, sizeof(rb_entShadowBits));

	for (uint32 num=0 ; num<MAX_SHADOW_GROUPS ; num++)
	{
		refShadowGroup_t *group = &rb_shadowGroups[num];
		group->bit = BIT(num);
	}

	return true;
}


/*
=============
RB_ShutdownShadows
=============
*/
void RB_ShutdownShadows()
{
}
