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
// cg_mapeffects.c
// FIXME TODO:
// - "nets" so that rain within a set bounds is possible
//

#include "cg_local.h"

#define MAPFX_DELIMINATORS	"\t\r\n "
#define MAPFX_MAXFX		512

typedef void (*mapEffectFunc_t)(struct mapEffect_t &effect);

struct mapEffect_t
{
	vec3_t		origin;

	vec3_t		velocity;
	vec3_t		acceleration;

	vec4_t		color;
	vec4_t		colorVel;

	float		scale;
	float		scaleVel;

	int			type;
	int			flags;

	float		orient;

	mapEffectFunc_t function;
	
	int			lastRefreshTime;
	bool		lastVisible;

	refPoly_t	outPoly;
	colorb		outColor[4];
	vec2_t		outCoords[4];
	vec3_t		outVertices[4];

	int			visFrame;
};

// <org0 org1 org2> <vel0 vel1 vel2> <accel0 accel1 accel2>
// <clr0 clr1 clr2> <clv0 clv1 clv2> <alpha alphavel>
// <scale> <scalevel>
// <type> <flags>

enum {
	MFXS_ORG0,		MFXS_ORG1,		MFXS_ORG2,
	MFXS_VEL0,		MFXS_VEL1,		MFXS_VEL2,
	MFXS_ACCEL0,	MFXS_ACCEL1,	MFXS_ACCEL2,
	MFXS_CLR0,		MFXS_CLR1,		MFXS_CLR2,
	MFXS_CLRVEL0,	MFXS_CLRVEL1,	MFXS_CLRVEL2,
	MFXS_ALPHA,		MFXS_ALPHAVEL,
	MFXS_SCALE,		MFXS_SCALEVEL,
	MFXS_TYPE,		MFXS_FLAGS,
	MFXS_DELAY,

	MFXS_MAX
};

static bool		cg_mfxInitialized;

struct pvs_t
{
	int numClusters;
	int clusterNums[32];
	int areas[2];
	int headNode;
	byte *bits;

	pvs_t ()
	{
		memset(clusterNums, 0, sizeof(clusterNums));
		areas[0] = areas[1] = 0;
		bits = null;
	}

	// builds a PVS around this origin
	void BuildPVS (vec3_t origin)
	{
		vec3_t absMin, absMax;

		Vec3Copy (origin, absMin);
		Vec3Copy (origin, absMax);
		absMin[0] -= 1;
		absMin[1] -= 1;
		absMin[2] -= 1;
		absMax[0] += 1;
		absMax[1] += 1;
		absMax[2] += 1;

		int leafs[128], clusters[128], topnode;
		// Get all leafs, including solids
		int num_leafs = cgi.CM_BoxLeafnums (absMin, absMax, leafs, 128, &topnode);

		// Set areas
		for (int i = 0; i < num_leafs; i++)
		{
			clusters[i] = cgi.CM_LeafCluster (leafs[i]);
			var area = cgi.CM_LeafArea (leafs[i]);
			if (area)
			{
				/*
				** Doors may legally straggle two areas,
				** but nothing should even need more than that
				*/
				if (areas[0] && areas[0] != area)
				{
					if (areas[1] && areas[1] != area && cgi.Com_ServerState () == SS_LOADING)
						Com_DevPrintf (0, "Object touching 3 areas at %f %f %f\n",
						absMin[0], absMin[1], absMin[2]);
					areas[1] = area;
				}
				else
					areas[0] = area;
			}
		}

		if (num_leafs >= 128)
		{
			// Assume we missed some leafs, and mark by headNode
			numClusters = -1;
			headNode = topnode;
		}
		else
		{
			numClusters = 0;
				
			for (int i = 0; i < num_leafs; i++)
			{
				if (clusters[i] == -1)
					continue;		// Not a visible leaf
				int j;
				for (j = 0; j < i; j++)
					if (clusters[j] == clusters[i])
						break;

				if (j == i)
				{
					if (numClusters == 128) {
						// Assume we missed some leafs, and mark by headNode
						numClusters = -1;
						headNode = topnode;
						break;
					}

					clusterNums[numClusters++] = clusters[i];
				}
			}
		}
	}
};

struct mapEffectGroup_t
{
	pvs_t					pvs;
	TList<mapEffect_t*>		effects;
};

static TList<mapEffect_t*> cg_mapEffectList;
static TList<mapEffectGroup_t*> cg_mapEffectGroups;

static char			cg_mfxFileName[MAX_QPATH];
static char			cg_mfxMapName[MAX_QPATH];

/*
=============================================================================

	EFFECTS

=============================================================================
*/

bool CheckMFXCulling (mapEffect_t *mfx)
{
	mfx->orient = 0;

	if (cg.refreshTime-mfx->lastRefreshTime >= THINK_DELAY_EXPENSIVE)
	{
		mfx->lastVisible = false;
		mfx->lastRefreshTime = cg.refreshTime;

		// Kill particles behind the view
		{
			//var start = cgi.Sys_Cycles();
			
			vec3_t temp;
			Vec3Subtract(mfx->origin, cg.refDef.viewOrigin, temp);
			VectorNormalizeFastf(temp);
			if (DotProduct(temp, cg.refDef.viewAxis[0]) < 0)
			{
				//viewCullMs += (cgi.Sys_Cycles() - start) * cgi.Sys_MSPerCycle();
				return true;
			}

			//viewCullMs += (cgi.Sys_Cycles() - start) * cgi.Sys_MSPerCycle();			
		}

		// Trace
		{
			//var start = cgi.Sys_Cycles();
			vec3_t mins, maxs;
			Vec3Set(maxs, 1, 1, 1);
			Vec3Set(mins, -1, -1, -1);

			bool entities = (mfx->type != 2);

			cmTrace_t tr;
			CG_PMTrace(&tr, cg.refDef.viewOrigin, mins, maxs, mfx->origin, entities, entities);
			if (tr.startSolid || tr.allSolid || tr.fraction < 1.0f)
			{
				//traceCullMs += (cgi.Sys_Cycles() - start) * cgi.Sys_MSPerCycle();
				return true;
			}

			//traceCullMs += (cgi.Sys_Cycles() - start) * cgi.Sys_MSPerCycle();
		}

		mfx->lastVisible = true;
	}

	// Calculate orientation
	float dist = Vec3DistFast(cg.refDef.viewOrigin, mfx->origin);
	mfx->orient = dist * 0.2f;

	return !mfx->lastVisible;
}

static byte		sv_fatPVS[65536/8];	// 32767 is Q2BSP_MAX_LEAFS
void SV_FatPVS (vec3_t org)
{
	int		leafs[64];
	int		i, j, count;
	int		longs;
	byte	*src;
	vec3_t	mins, maxs;

	for (i=0 ; i<3 ; i++) {
		mins[i] = org[i] - 8;
		maxs[i] = org[i] + 8;
	}

	count = cgi.CM_BoxLeafnums (mins, maxs, leafs, 64, NULL);
	if (count < 1)
		Com_Error (ERR_FATAL, "SV_FatPVS: count < 1");
	longs = (cgi.CM_NumClusters()+31)>>5;

	// convert leafs to clusters
	for (i=0 ; i<count ; i++)
		leafs[i] = cgi.CM_LeafCluster(leafs[i]);

	memcpy (sv_fatPVS, cgi.CM_ClusterPVS(leafs[0]), longs<<2);
	// or in all the other leaf bits
	for (i=1 ; i<count ; i++) {
		for (j=0 ; j<i ; j++)
			if (leafs[i] == leafs[j])
				break;
		if (j != i)
			continue;		// already have the cluster we want
		src = cgi.CM_ClusterPVS(leafs[i]);
		for (j=0 ; j<longs ; j++)
			((long *)sv_fatPVS)[j] |= ((long *)src)[j];
	}
}

int numCulled = 0;

void CG_AddMapFXToList()
{
	if (!cg_mapEffects->intVal)
		return;

	numCulled = 0;
	SV_FatPVS(cg.refDef.viewOrigin);

	for (uint32 g = 0; g < cg_mapEffectGroups.Count(); ++g)
	{
		// Area
		{
			var &pvs = cg_mapEffectGroups[g]->pvs;
			if (pvs.numClusters == -1) {
				// too many leafs for individual check, go by headnode
				if (!cgi.CM_HeadnodeVisible (pvs.headNode, sv_fatPVS))
					continue;
			}
			else {
				// check individual leafs
				int i;
				for (i=0 ; i < pvs.numClusters ; i++) {
					var l = pvs.clusterNums[i];
					if (sv_fatPVS[l >> 3] & BIT(l&7))
						break;
				}
				if (i == pvs.numClusters)
				{
					numCulled++;
					continue;		// not visible
				}
			}
		}

		for (uint32 i = 0; i < cg_mapEffectGroups[g]->effects.Count(); ++i)
		{
			var *mfx = cg_mapEffectGroups[g]->effects[i];

			if (mfx->visFrame == cg.realTime)
				continue;

			mfx->visFrame = cg.realTime;

			vec3_t outOrigin;
			outOrigin[0] = mfx->origin[0];
			outOrigin[1] = mfx->origin[1];
			outOrigin[2] = mfx->origin[2];

			// sizeVel calcs
			float size = mfx->scale * 10;

			if (CheckMFXCulling(mfx))
				continue;

			float outOrient = mfx->orient;

			// Add to be rendered
			float scale;
			scale = (outOrigin[0] - cg.refDef.viewOrigin[0]) * cg.refDef.viewAxis[0][0] +
				(outOrigin[1] - cg.refDef.viewOrigin[1]) * cg.refDef.viewAxis[0][1] +
				(outOrigin[2] - cg.refDef.viewOrigin[2]) * cg.refDef.viewAxis[0][2];

			scale = (scale < 20) ? 1 : 1 + scale * 0.004f;
			scale = (scale - 1) + size;

			// Rendering
			colorb outColor (
				mfx->color[0],
				mfx->color[1],
				mfx->color[2],
				mfx->color[3] * 255);

			// Top left
			Vec2Set(mfx->outCoords[0], cgMedia.particleCoords[(mfx->type == 0) ? MFX_WHITE : MFX_CORONA][0], cgMedia.particleCoords[(mfx->type == 0) ? MFX_WHITE : MFX_CORONA][1]);
			Vec3Set(mfx->outVertices[0],	outOrigin[0] + cg.refDef.viewAxis[2][0]*scale + cg.refDef.viewAxis[1][0]*scale,
				outOrigin[1] + cg.refDef.viewAxis[2][1]*scale + cg.refDef.viewAxis[1][1]*scale,
				outOrigin[2] + cg.refDef.viewAxis[2][2]*scale + cg.refDef.viewAxis[1][2]*scale);

			// Bottom left
			Vec2Set(mfx->outCoords[1], cgMedia.particleCoords[(mfx->type == 0) ? MFX_WHITE : MFX_CORONA][0], cgMedia.particleCoords[(mfx->type == 0) ? MFX_WHITE : MFX_CORONA][3]);
			Vec3Set(mfx->outVertices[1],	outOrigin[0] - cg.refDef.viewAxis[2][0]*scale + cg.refDef.viewAxis[1][0]*scale,
				outOrigin[1] - cg.refDef.viewAxis[2][1]*scale + cg.refDef.viewAxis[1][1]*scale,
				outOrigin[2] - cg.refDef.viewAxis[2][2]*scale + cg.refDef.viewAxis[1][2]*scale);

			// Bottom right
			Vec2Set(mfx->outCoords[2], cgMedia.particleCoords[mfx->type][2], cgMedia.particleCoords[(mfx->type == 0) ? MFX_WHITE : MFX_CORONA][3]);
			Vec3Set(mfx->outVertices[2],	outOrigin[0] - cg.refDef.viewAxis[2][0]*scale - cg.refDef.viewAxis[1][0]*scale,
				outOrigin[1] - cg.refDef.viewAxis[2][1]*scale - cg.refDef.viewAxis[1][1]*scale,
				outOrigin[2] - cg.refDef.viewAxis[2][2]*scale - cg.refDef.viewAxis[1][2]*scale);

			// Top right
			Vec2Set(mfx->outCoords[3], cgMedia.particleCoords[(mfx->type == 0) ? MFX_WHITE : MFX_CORONA][2], cgMedia.particleCoords[(mfx->type == 0) ? MFX_WHITE : MFX_CORONA][1]);
			Vec3Set(mfx->outVertices[3],	outOrigin[0] + cg.refDef.viewAxis[2][0]*scale - cg.refDef.viewAxis[1][0]*scale,
				outOrigin[1] + cg.refDef.viewAxis[2][1]*scale - cg.refDef.viewAxis[1][1]*scale,
				outOrigin[2] + cg.refDef.viewAxis[2][2]*scale - cg.refDef.viewAxis[1][2]*scale);

			// Render it
			mfx->outColor[0] = outColor;
			mfx->outColor[1] = outColor;
			mfx->outColor[2] = outColor;
			mfx->outColor[3] = outColor;

			mfx->outPoly.mat = cgMedia.particleTable[(mfx->type == 0) ? MFX_WHITE : MFX_CORONA];
			Vec3Copy(outOrigin, mfx->outPoly.origin);
			mfx->outPoly.radius = scale;

			cgi.R_AddPoly(&mfx->outPoly);
		}
	}
}

/*
=============================================================================

	PARSING

=============================================================================
*/

/*
==================
CG_MapFXClear
==================
*/
void CG_MapFXClear ()
{
	cg_mapEffectList.Clear();
	cg_mapEffectGroups.Clear();
	cg_mfxInitialized = false;
}


/*
==================
CG_MapFXLoad
==================
*/
void CG_MapFXLoad (char *mapName)
{
	char			*token, *buf;
	uint32			fileLen, numFx;
	mapEffect_t		*mfx;
	int				stageNum;

	if (cg_mfxInitialized)
		return;
	if (!mapName[0])
		return;
	if (strlen(mapName) < 9)
		return;	// Name is too short

	// Skip maps/ and remove .bsp
	mapName = Com_SkipPath (mapName);
	Com_StripExtension (cg_mfxMapName, sizeof(cg_mfxMapName), mapName);

	// Load the mfx file
	Q_snprintfz (cg_mfxFileName, sizeof(cg_mfxFileName), "mfx/%s.mfx", cg_mfxMapName);
	fileLen = cgi.FS_LoadFile (cg_mfxFileName, (void **)&buf, true);
	if (!buf || fileLen <= 0) {	
		Com_DevPrintf (PRNT_WARNING, "WARNING: can't load '%s' -- %s\n", cg_mfxFileName, (fileLen == -1) ? "not found" : "empty file");
		return;
	}

	Com_DevPrintf (0, "...loading '%s'\n", cg_mfxFileName);

	stageNum = 0;
	numFx = 0;
	mfx = NULL;
	token = strtok (buf, MAPFX_DELIMINATORS);

	// FIXME: re-write parser to not use strtok
	while (token)
	{
		if (stageNum == 0)
		{
			cg_mapEffectList.Add(new mapEffect_t());
			mfx = cg_mapEffectList[cg_mapEffectList.Count()-1];
			mfx->lastRefreshTime = 0;

			mfx->outPoly.numVerts = 4;
			mfx->outPoly.colors = mfx->outColor;
			mfx->outPoly.texCoords = mfx->outCoords;
			mfx->outPoly.vertices = mfx->outVertices;
			mfx->outPoly.matTime = 0;
		}

		switch (stageNum++)
		{
		case MFXS_ORG0:		mfx->origin[0] = (float)atoi (token) * 0.125f;			break;
		case MFXS_ORG1:		mfx->origin[1] = (float)atoi (token) * 0.125f;			break;
		case MFXS_ORG2:		mfx->origin[2] = (float)atoi (token) * 0.125f;			break;
		case MFXS_VEL0:		mfx->velocity[0] = (float)atoi (token) * 0.125f;		break;
		case MFXS_VEL1:		mfx->velocity[1] = (float)atoi (token) * 0.125f;		break;
		case MFXS_VEL2:		mfx->velocity[2] = (float)atoi (token) * 0.125f;		break;
		case MFXS_ACCEL0:	mfx->acceleration[0] = (float)atoi (token) * 0.125f;	break;
		case MFXS_ACCEL1:	mfx->acceleration[1] = (float)atoi (token) * 0.125f;	break;
		case MFXS_ACCEL2:	mfx->acceleration[2] = (float)atoi (token) * 0.125f;	break;
		case MFXS_CLR0:		mfx->color[0] = (float)atof (token);					break;
		case MFXS_CLR1:		mfx->color[1] = (float)atof (token);					break;
		case MFXS_CLR2:		mfx->color[2] = (float)atof (token);					break;
		case MFXS_CLRVEL0:	mfx->colorVel[0] = (float)atof (token);					break;
		case MFXS_CLRVEL1:	mfx->colorVel[1] = (float)atof (token);					break;
		case MFXS_CLRVEL2:	mfx->colorVel[2] = (float)atof (token);					break;
		case MFXS_ALPHA:	mfx->color[3] = (float)atof (token);					break;
		case MFXS_ALPHAVEL:	mfx->colorVel[3] = (float)atof (token);					break;
		case MFXS_SCALE:	mfx->scale = (float)atof (token);						break;
		case MFXS_SCALEVEL:	mfx->scaleVel = (float)atof (token);					break;
		case MFXS_TYPE:		mfx->type = atoi (token);								break;
		case MFXS_FLAGS:	mfx->flags = atoi (token);								break;
		}

		if (stageNum == MFXS_MAX)
		{
			stageNum = 0;

			pvs_t mfxPvs;
			mfxPvs.BuildPVS(mfx->origin);

			for (int c = 0; c < mfxPvs.numClusters; ++c)
			{
				if (mfxPvs.clusterNums[c] == -1)
					continue; // non-visible

				int numGrabbed = 0;

				// see if a group exists with this area
				for (uint32 i = 0; i < cg_mapEffectGroups.Count(); ++i)
				{
					for (int z = 0; z < cg_mapEffectGroups[i]->pvs.numClusters; ++z)
					{
						if (mfxPvs.clusterNums[c] == cg_mapEffectGroups[i]->pvs.clusterNums[z])
						{
							cg_mapEffectGroups[i]->effects.Add(mfx);
							numGrabbed++;
						}
					}
				}

				if (!numGrabbed)
				{
					// none found
					var newGroup = new mapEffectGroup_t();
					cg_mapEffectGroups.Add(newGroup);
					newGroup->pvs = mfxPvs;
					newGroup->effects.Add(mfx);
				}
			}
		}

		token = strtok (NULL, MAPFX_DELIMINATORS);
	}

	if (stageNum != 0) {
		Com_Printf (PRNT_ERROR, "CG_MapFXLoad: Bad file '%s'\n", cg_mfxFileName);
		CG_MapFXClear ();
	}
	else {
		cg_mfxInitialized = true;
	}

	CG_FS_FreeFile (buf);
}

/*
=============================================================================

	CONSOLE COMMANDS

=============================================================================
*/

/*
====================
CG_MFX_AddOrigin_f
====================
*/
static void CG_MFX_AddOrigin_f ()
{
	FILE	*f;
	char	path[MAX_QPATH];

	if (!cg.mapLoaded) {
		Com_Printf (0, "CG_MFX_AddOrigin_f: No map loaded!\n");
		return;
	}
	if (!cg_mfxInitialized)
		CG_MapFXLoad (cg.configStrings[CS_MODELS+1]);

	// Open file
	Q_snprintfz (path, sizeof(path), "%s/mfx/%s.mfx", cgi.FS_Gamedir (), cg_mfxMapName);
	f = fopen (path, "at");
	if (!f) {
		Com_Printf (PRNT_ERROR, "ERROR: CG_AddMFX Couldn't write %s\n", path);
		return;
	}

	// Print to file
	fprintf (f, "%i %i %i\t\t0 0 0\t\t0 0 0\t\t255 255 255\t255 255 255\t0.6 -10000\t2 2\t0\t0\t0\n",
		(int)(cg.refDef.viewOrigin[0]*8),
		(int)(cg.refDef.viewOrigin[1]*8),
		(int)(cg.refDef.viewOrigin[2]*8));

	fclose (f);

	// Echo
	Com_Printf (0, "Saved (x%i y%i z%i) to '%s', reloading file to display...\n",
		(int)cg.refDef.viewOrigin[0],
		(int)cg.refDef.viewOrigin[1],
		(int)cg.refDef.viewOrigin[2],
		path);

	// Reload
	CG_MapFXClear ();
	CG_MapFXLoad (cg.configStrings[CS_MODELS+1]);
}


/*
====================
CG_MFX_AddTrace_f
====================
*/
static void CG_MFX_AddTrace_f ()
{
	static vec3_t	mins = {-1, -1, -1}, maxs = {1, 1, 1};
	FILE		*f;
	char		path[MAX_QPATH];
	cmTrace_t	tr;
	vec3_t		forward;

	if (!cg.mapLoaded) {
		Com_Printf (0, "CG_MFX_AddTrace_f: No map loaded!\n");
		return;
	}
	if (!cg_mfxInitialized)
		CG_MapFXLoad (cg.configStrings[CS_MODELS+1]);

	// Open file
	Q_snprintfz (path, sizeof(path), "%s/mfx/%s.mfx", cgi.FS_Gamedir (), cg_mfxMapName);
	f = fopen (path, "at");
	if (!f) {
		Com_Printf (PRNT_ERROR, "ERROR: CG_AddMFXTr Couldn't write %s\n", path);
		return;
	}

	// Project forward
	Angles_Vectors (cg.refDef.viewAngles, forward, NULL, NULL);
	Vec3Scale (forward, 2048, forward);
	Vec3Add (forward, cg.refDef.viewOrigin, forward);
	CG_PMTrace (&tr, cg.refDef.viewOrigin, mins, maxs, forward, false, true);
	if (tr.startSolid || tr.allSolid) {
		Com_Printf (PRNT_ERROR, "ERROR: outside world!\n");
		fclose (f);
		return;
	}
	if (tr.fraction == 1.0f) {
		Com_Printf (PRNT_ERROR, "ERROR: didn't hit anything!\n");
		fclose (f);
		return;
	}

	// Print to file
	fprintf (f, "%i %i %i\t\t0 0 0\t\t0 0 0\t\t255 255 255\t255 255 255\t0.6 -10000\t2 2\t0\t0\t0\n",
		(int)((tr.endPos[0] + tr.plane.normal[0])*8),
		(int)((tr.endPos[1] + tr.plane.normal[1])*8),
		(int)((tr.endPos[2] + tr.plane.normal[2])*8));

	fclose (f);

	// Echo
	Com_Printf (0, "Saved (x%i y%i z%i) to '%s', reloading file to display...\n",
		(int)(tr.endPos[0] + tr.plane.normal[0]),
		(int)(tr.endPos[1] + tr.plane.normal[1]),
		(int)(tr.endPos[2] + tr.plane.normal[2]),
		path);

	// Reload
	CG_MapFXClear ();
	CG_MapFXLoad (cg.configStrings[CS_MODELS+1]);
}


/*
====================
CG_MFX_Restart_f
====================
*/
static void CG_MFX_Restart_f ()
{
	if (!cg.mapLoaded) {
		Com_Printf (0, "CG_MFX_Restart_f: No map loaded!\n");
		return;
	}

	Com_Printf (0, "Reloading mapfx...\n");

	CG_MapFXClear ();
	CG_MapFXLoad (cg.configStrings[CS_MODELS+1]);
}

/*
=============================================================================

	INIT / SHUTDOWN

=============================================================================
*/

static conCmd_t	*cmd_addMFX;
static conCmd_t *cmd_addMFXTrace;
static conCmd_t *cmd_mfx_restart;

/*
==================
CG_MapFXInit
==================
*/
void CG_MapFXInit()
{
	// All the console command
	cmd_addMFX		= cgi.Cmd_AddCommand("mfx_addorg",		0, CG_MFX_AddOrigin_f,	"Appends a generic effect to this map's mfx file at your location");
	cmd_addMFXTrace	= cgi.Cmd_AddCommand("mfx_addtr",		0, CG_MFX_AddTrace_f,	"Appends a generic effect to this map's mfx file at the target surface");
	cmd_mfx_restart	= cgi.Cmd_AddCommand("mfx_restart",		0, CG_MFX_Restart_f,	"Reloads this maps' MapFX file");
}


/*
==================
CG_MapFXShutdown
==================
*/
void CG_MapFXShutdown()
{
	CG_MapFXClear();

	cgi.Cmd_RemoveCommand(cmd_addMFX);
	cgi.Cmd_RemoveCommand(cmd_addMFXTrace);
	cgi.Cmd_RemoveCommand(cmd_mfx_restart);
}
