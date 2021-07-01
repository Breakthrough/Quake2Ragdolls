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
// cg_light.c
//

#include "cg_local.h"

struct cgLightStyle_t {
	float		map[MAX_CFGSTRLEN];

	int			length;
	float		value[3];
};

static cgLightStyle_t	cg_lightStyles[MAX_CS_LIGHTSTYLES];
static int				cg_lSLastOfs;

static cgDLight_t		cg_dLightList[MAX_REF_DLIGHTS];

/*
=============================================================================

	DLIGHT STYLE MANAGEMENT

=============================================================================
*/

/*
================
CG_ClearLightStyles
================
*/
void CG_ClearLightStyles ()
{
	memset (cg_lightStyles, 0, sizeof(cg_lightStyles));
	cg_lSLastOfs = -1;
}


/*
================
CG_RunLightStyles
================
*/
void CG_RunLightStyles ()
{
	int				ofs;
	int				i;
	cgLightStyle_t	*ls;
	float			backLerp, frac;
	float			map;

	ofs = cg.refreshTime / 100;
	if (ofs == cg_lSLastOfs)
		return;

	frac = ofs - cg_lSLastOfs;
	backLerp = 1.0f - frac;

	for (i=0, ls=cg_lightStyles ; i<MAX_CS_LIGHTSTYLES ; i++, ls++) {
		if (!ls->length) {
			ls->value[0] = ls->value[1] = ls->value[2] = 1.0;
			continue;
		}

		if (ls->length == 1) {
			ls->value[0] = ls->value[1] = ls->value[2] = ls->map[0];
		}
		else {
			map = (ls->map[cg_lSLastOfs%ls->length] * backLerp) + (ls->map[ofs%ls->length] * frac);
			ls->value[0] = ls->value[1] = ls->value[2] = map;
		}
	}

	cg_lSLastOfs = ofs;
}


/*
================
CG_SetLightstyle
================
*/
void CG_SetLightstyle (int num)
{
	char	*s;
	int		len, i;

	s = cg.configStrings[num+CS_LIGHTS];

	len = (int)strlen (s);
	if (len >= MAX_CFGSTRLEN)
		Com_Error (ERR_DROP, "CG_SetLightstyle: svc_lightstyle length=%i", len);

	cg_lightStyles[num].length = len;
	for (i=0 ; i<len ; i++)
		cg_lightStyles[num].map[i] = (float)(s[i]-'a')/(float)('m'-'a');
}


/*
================
CG_AddLightStyles
================
*/
void CG_AddLightStyles ()
{
	int				i;
	cgLightStyle_t	*ls;

	for (i=0, ls=cg_lightStyles ; i<MAX_CS_LIGHTSTYLES ; i++, ls++)
		cgi.R_AddLightStyle (i, ls->value[0], ls->value[1], ls->value[2]);
}

/*
=============================================================================

	DLIGHT MANAGEMENT

=============================================================================
*/

/*
================
CG_ClearDLights
================
*/
void CG_ClearDLights ()
{
	memset (cg_dLightList, 0, sizeof(cg_dLightList));
}


/*
===============
CG_AllocDLight
===============
*/
cgDLight_t *CG_AllocDLight (int key)
{
	int			i;
	cgDLight_t	*dl;

	// First look for an exact key match
	if (key) {
		dl = cg_dLightList;
		for (i=0 ; i<MAX_REF_DLIGHTS ; i++, dl++) {
			if (dl->key == key) {
				memset (dl, 0, sizeof(cgDLight_t));
				dl->key = key;
				return dl;
			}
		}
	}

	// Then look for anything else
	dl = cg_dLightList;
	for (i=0 ; i<MAX_REF_DLIGHTS ; i++, dl++) {
		if (dl->die < cg.refreshTime) {
			memset (dl, 0, sizeof(cgDLight_t));
			dl->key = key;
			return dl;
		}
	}

	dl = &cg_dLightList[0];
	memset (dl, 0, sizeof(cgDLight_t));
	dl->key = key;
	return dl;
}


/*
===============
CG_RunDLights
===============
*/
void CG_RunDLights ()
{
	int			i;
	cgDLight_t	*dl;

	dl = cg_dLightList;
	for (i=0 ; i<MAX_REF_DLIGHTS ; i++, dl++) {
		if (!dl->radius)
			continue;
		
		if (dl->die < cg.refreshTime) {
			dl->radius = 0;
			continue;
		}
		dl->radius -= cg.refreshFrameTime*dl->decay;
		if (dl->radius < 0)
			dl->radius = 0;
	}
}


/*
===============
CG_AddDLights
===============
*/
void CG_AddDLights ()
{
	int			i;
	cgDLight_t	*dl;

	for (dl=cg_dLightList, i=0 ; i<MAX_REF_DLIGHTS ; i++, dl++) {
		if (!dl->radius)
			continue;

		cgi.R_AddLight (dl->origin, dl->radius, dl->color[0], dl->color[1], dl->color[2]);
	}
}

/*
=============================================================================

	LIGHT EFFECTS

=============================================================================
*/

/*
===============
CG_Flashlight
===============
*/
void CG_Flashlight (int ent, vec3_t pos)
{
	cgDLight_t	*dl;

	dl = CG_AllocDLight (ent);
	Vec3Copy (pos, dl->origin);
	dl->radius = 400;
	dl->minlight = 250;
	dl->die = cg.refreshTime + 100.0f;
	dl->color[0] = 1;
	dl->color[1] = 1;
	dl->color[2] = 1;
}


/*
===============
CG_ColorFlash

flash of light
===============
*/
void __fastcall CG_ColorFlash (vec3_t pos, int ent, float intensity, float r, float g, float b)
{
	cgDLight_t	*dl;

	dl = CG_AllocDLight (ent);
	Vec3Copy (pos, dl->origin);
	dl->radius = intensity;
	dl->minlight = 250;
	dl->die = (float)cg.refreshTime + 100.0f;
	dl->color[0] = r;
	dl->color[1] = g;
	dl->color[2] = b;
}


/*
===============
CG_WeldingSparkFlash
===============
*/
void CG_WeldingSparkFlash (vec3_t pos)
{
	cgDLight_t	*dl;

	dl = CG_AllocDLight ((int)((pos[0]+pos[1]+pos[3]) / 3.0));

	Vec3Copy (pos, dl->origin);
	Vec3Set (dl->color, 1, 1, 0.3f);
	dl->decay = 10;
	dl->die = (float)cg.refreshTime + 100.0f;
	dl->minlight = 100;
	dl->radius = 175;
}
