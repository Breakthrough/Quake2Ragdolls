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
// cm_common.c
// Wraps functionality between the two formats, leaving the other systems
// perfectly ignorant to the actual format loaded
//

#include "cm_common.h"

enum {
	BSP_TYPE_Q2,
	BSP_TYPE_Q3,
};

static int				cm_bspType;

static char				cm_mapName[MAX_QPATH];
static uint32			cm_mapChecksum;

int						cm_numCModels;
cmBspModel_t			cm_mapCModels[MAX_CM_CMODELS];

int						cm_numTraces;
int						cm_numBrushTraces;
int						cm_numPointContents;

cVar_t					*flushmap;
cVar_t					*cm_noAreas;
cVar_t					*cm_noCurves;
cVar_t					*cm_showTrace;

/*
=============================================================================

	BSP LOADING

=============================================================================
*/

/*
==================
CM_PrepMap
==================
*/
static void CM_PrepMap ()
{
	if (cm_bspType == BSP_TYPE_Q3)
		CM_Q3BSP_PrepMap ();
	else
		CM_Q2BSP_PrepMap ();
}


/*
==================
CM_LoadMap

Loads in the map and all submodels
==================
*/
struct bspFormat_t {
	byte			type;
	const char		*headerStr;
	byte			headerLen;
	byte			version;
	cmBspModel_t	*(*loader) (uint32 *buffer);
};

static bspFormat_t bspFormats[] = {
	{ BSP_TYPE_Q2,	Q2BSP_HEADER,		4,	Q2BSP_VERSION,			CM_Q2BSP_LoadMap },	// Quake2 BSP models
	{ BSP_TYPE_Q3,	Q3BSP_HEADER,		4,	Q3BSP_VERSION,			CM_Q3BSP_LoadMap },	// Quake3 BSP models

	{ 0,			NULL,				0,	0,						NULL }
};

static int numBSPFormats = (sizeof(bspFormats) / sizeof(bspFormats[0])) - 1;
cmBspModel_t *CM_LoadMap(char *name, bool clientLoad, uint32 *checksum)
{
	cmBspModel_t *model;
	uint32		*buffer;
	int			fileLen, i;
	bspFormat_t	*descr;
	char		fixedName[MAX_QPATH];

	flushmap		= Cvar_Register ("flushmap",		"0",		0);
	cm_noAreas		= Cvar_Register ("cm_noAreas",		"0",		CVAR_CHEAT);
	cm_noCurves		= Cvar_Register ("cm_noCurves",		"0",		CVAR_CHEAT);
	cm_showTrace	= Cvar_Register ("cm_showTrace",	"0",		0);

	Com_NormalizePath (fixedName, sizeof(fixedName), name);
	if (fixedName[0])	// Demos will pass a NULL name, don't need to append an extension to that...
		Com_DefaultExtension (fixedName, ".bsp", sizeof(fixedName));
	if (!strcmp(cm_mapName, fixedName) && (clientLoad || !flushmap->intVal))
	{
		*checksum = cm_mapChecksum;
		if (!clientLoad)
			CM_PrepMap();

		// Check integrity and return
		Mem_CheckPoolIntegrity(com_cmodelSysPool);

		return &cm_mapCModels[0];		// still have the right version
	}

	// Free old stuff
	CM_UnloadMap();
	if (!fixedName[0])
	{
		*checksum = cm_mapChecksum;
		return &cm_mapCModels[0];		// cinematic servers won't have anything at all
	}

	// Load the file
	fileLen = FS_LoadFile(fixedName, (void **)&buffer, false);
	if (!buffer || fileLen <= 0)
		Com_Error (ERR_DROP, "CM_LoadMap: Couldn't %s %s", (fileLen == -1) ? "find" : "load", fixedName);

	// Calculate checksum
	cm_mapChecksum = LittleLong(Com_BlockChecksum (buffer, fileLen));
	*checksum = cm_mapChecksum;

	// Load the model
	descr = bspFormats;
	for (i=0 ; i<numBSPFormats ; i++, descr++)
	{
		if (strncmp ((const char *)buffer, descr->headerStr, descr->headerLen))
			continue;
		if (((int *)buffer)[1] != descr->version)
			continue;
		break;
	}
	if (i == numBSPFormats)
		Com_Error (ERR_DROP, "CM_LoadMap: unknown fileId for %s", fixedName);

	model = descr->loader(buffer);
	if (!model)
	{
		FS_FreeFile(buffer);
		return NULL;
	}

	cm_bspType = descr->type;
	Q_strncpyz(cm_mapName, fixedName, sizeof(cm_mapName));

	// Free the buffer
	FS_FreeFile(buffer);

	// Check integrity and return
	Mem_CheckPoolIntegrity(com_cmodelSysPool);
	return model;
}


/*
==================
CM_UnloadMap
==================
*/
void CM_UnloadMap()
{
	Mem_FreePool(com_cmodelSysPool);

	if (cm_bspType == BSP_TYPE_Q3)
		CM_Q3BSP_UnloadMap();
	else
		CM_Q2BSP_UnloadMap();

	cm_mapName[0] = 0;
	cm_mapChecksum = 0;

	cm_numCModels = 0;

	cm_numTraces = 0;
	cm_numBrushTraces = 0;
	cm_numPointContents = 0;
}


/*
==================
CM_InlineModel
==================
*/
cmBspModel_t *CM_InlineModel (char *name)
{
	int		num;

	if (!name || name[0] != '*')
		Com_Error (ERR_DROP, "CM_InlineModel: bad name '%s'", name);
	num = atoi (name+1);
	if (num < 1 || num >= cm_numCModels)
		Com_Error (ERR_DROP, "CM_InlineModel: bad number %i (%i)", num, cm_numCModels);

	return &cm_mapCModels[num];
}


/*
==================
CM_InlineModelBounds
==================
*/
void CM_InlineModelBounds (cmBspModel_t *model, vec3_t mins, vec3_t maxs)
{
	if (model) {
		Vec3Copy (model->mins, mins);
		Vec3Copy (model->maxs, maxs);
	}
}


/*
==================
CM_InlineModelBounds
==================
*/
int CM_InlineModelHeadNode (cmBspModel_t *model)
{
	if (model)
		return model->headNode;

	return 0;
}

/*
=============================================================================

	BSP INFORMATION

=============================================================================
*/

/*
==================
CM_EntityString
==================
*/
char *CM_EntityString ()
{
	if (cm_bspType == BSP_TYPE_Q3)
		return CM_Q3BSP_EntityString ();
	return CM_Q2BSP_EntityString ();
}


/*
==================
CM_SurfRName
==================
*/
char *CM_SurfRName (int texNum)
{
	if (cm_bspType == BSP_TYPE_Q3)
		return CM_Q3BSP_SurfRName (texNum);
	return CM_Q2BSP_SurfRName (texNum);
}


/*
==================
CM_NumClusters
==================
*/
int CM_NumClusters ()
{
	if (cm_bspType == BSP_TYPE_Q3)
		return CM_Q3BSP_NumClusters ();
	return CM_Q2BSP_NumClusters ();
}


/*
==================
CM_NumInlineModels
==================
*/
int CM_NumInlineModels ()
{
	return cm_numCModels;
}


/*
==================
CM_NumTexInfo
==================
*/
int CM_NumTexInfo ()
{
	if (cm_bspType == BSP_TYPE_Q3)
		return CM_Q3BSP_NumTexInfo ();
	return CM_Q2BSP_NumTexInfo ();
}

/*
=============================================================================

	TRACING

=============================================================================
*/

int CM_LeafArea (int leafNum)
{
	if (cm_bspType == BSP_TYPE_Q3)
		return CM_Q3BSP_LeafArea (leafNum);
	return CM_Q2BSP_LeafArea (leafNum);
}

int CM_LeafCluster (int leafNum)
{
	if (cm_bspType == BSP_TYPE_Q3)
		return CM_Q3BSP_LeafCluster (leafNum);
	return CM_Q2BSP_LeafCluster (leafNum);
}

int CM_LeafContents (int leafNum)
{
	if (cm_bspType == BSP_TYPE_Q3)
		return CM_Q3BSP_LeafContents (leafNum);
	return CM_Q2BSP_LeafContents (leafNum);
}

// ==========================================================================

int	CM_HeadnodeForBox (vec3_t mins, vec3_t maxs)
{
	if (cm_bspType == BSP_TYPE_Q3)
		return CM_Q3BSP_HeadnodeForBox (mins, maxs);
	return CM_Q2BSP_HeadnodeForBox (mins, maxs);
}

int CM_PointLeafnum (vec3_t p)
{
	if (cm_bspType == BSP_TYPE_Q3)
		return CM_Q3BSP_PointLeafnum (p);
	return CM_Q2BSP_PointLeafnum (p);
}

int	CM_BoxLeafnums (vec3_t mins, vec3_t maxs, int *list, int listSize, int *topNode)
{
	if (cm_bspType == BSP_TYPE_Q3)
		return CM_Q3BSP_BoxLeafnums (mins, maxs, list, listSize, topNode);
	return CM_Q2BSP_BoxLeafnums (mins, maxs, list, listSize, topNode);
}

int CM_PointContents (vec3_t p, int headNode)
{
	if (cm_bspType == BSP_TYPE_Q3)
		return CM_Q3BSP_PointContents (p, headNode);
	return CM_Q2BSP_PointContents (p, headNode);
}

int	CM_TransformedPointContents (vec3_t p, int headNode, vec3_t origin, vec3_t angles)
{
	if (cm_bspType == BSP_TYPE_Q3)
		return CM_Q3BSP_TransformedPointContents (p, headNode, origin, angles);
	return CM_Q2BSP_TransformedPointContents (p, headNode, origin, angles);
}

/*
=============================================================================

	BOX TRACING

=============================================================================
*/

cmTrace_t CM_Trace (vec3_t start, vec3_t end, float size, int contentMask)
{
	if (cm_bspType == BSP_TYPE_Q3)
		return CM_Q3BSP_Trace (start, end, size, contentMask);
	return CM_Q2BSP_Trace (start, end, size, contentMask);
}

cmTrace_t CM_BoxTrace (vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, int headNode, int brushMask)
{
	if (cm_bspType == BSP_TYPE_Q3)
		return CM_Q3BSP_BoxTrace (start, end, mins, maxs, headNode, brushMask);
	return CM_Q2BSP_BoxTrace (start, end, mins, maxs, headNode, brushMask);
}

void CM_TransformedBoxTrace (cmTrace_t *out, vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, int headNode, int brushMask, vec3_t origin, vec3_t angles)
{
	if (!out)
		return;

	if (cm_bspType == BSP_TYPE_Q3) {
		CM_Q3BSP_TransformedBoxTrace (out, start, end, mins, maxs, headNode, brushMask, origin, angles);
		return;
	}
	CM_Q2BSP_TransformedBoxTrace (out, start, end, mins, maxs, headNode, brushMask, origin, angles);
}

/*
=============================================================================

	PVS / PHS

=============================================================================
*/

byte *CM_ClusterPVS (int cluster)
{
	if (cm_bspType == BSP_TYPE_Q3)
		return CM_Q3BSP_ClusterPVS (cluster);
	return CM_Q2BSP_ClusterPVS (cluster);
}

byte *CM_ClusterPHS (int cluster)
{
	if (cm_bspType == BSP_TYPE_Q3)
		return CM_Q3BSP_ClusterPHS (cluster);
	return CM_Q2BSP_ClusterPHS (cluster);
}

/*
=============================================================================

	AREAPORTALS

=============================================================================
*/

void CM_SetAreaPortalState (int portalNum, int area, int otherArea, bool open)
{
	if (cm_bspType == BSP_TYPE_Q3) {
		if (area && otherArea) // Must be touching two areas
			CM_Q3BSP_SetAreaPortalState (portalNum, area, otherArea, open);
		return;
	}
	CM_Q2BSP_SetAreaPortalState (portalNum, open);
}

bool CM_AreasConnected (int area1, int area2)
{
	if (cm_bspType == BSP_TYPE_Q3)
		return CM_Q3BSP_AreasConnected (area1, area2);
	return CM_Q2BSP_AreasConnected (area1, area2);
}

int CM_WriteAreaBits (byte *buffer, int area)
{
	if (cm_bspType == BSP_TYPE_Q3)
		return CM_Q3BSP_WriteAreaBits (buffer, area);
	return CM_Q2BSP_WriteAreaBits (buffer, area);
}

void CM_WritePortalState (fileHandle_t fileNum)
{
	if (cm_bspType == BSP_TYPE_Q3) {
		CM_Q3BSP_WritePortalState (fileNum);
		return;
	}
	CM_Q2BSP_WritePortalState (fileNum);
}

void CM_ReadPortalState (fileHandle_t fileNum)
{
	if (cm_bspType == BSP_TYPE_Q3) {
		CM_Q3BSP_ReadPortalState (fileNum);
		return;
	}
	CM_Q2BSP_ReadPortalState (fileNum);
}

bool CM_HeadnodeVisible (int nodeNum, byte *visBits)
{
	if (cm_bspType == BSP_TYPE_Q3)
		return CM_Q3BSP_HeadnodeVisible (nodeNum, visBits);
	return CM_Q2BSP_HeadnodeVisible (nodeNum, visBits);
}

/*
=============================================================================

	MISCELLANEOUS

=============================================================================
*/

/*
==================
CM_PrintStats
==================
*/
void CM_PrintStats ()
{
	static int	highTrace = 0;
	static int	highBTrace = 0;
	static int	highPC = 0;

	if (cm_showTrace && cm_showTrace->intVal)
		Com_Printf (0, "%4i/%4i tr %4i/%4i brtr %4i/%4i pt\n",
			cm_numTraces, highTrace,
			cm_numBrushTraces, highBTrace,
			cm_numPointContents, highPC);

	if (cm_numTraces > highTrace)
		highTrace = cm_numTraces;
	if (cm_numBrushTraces > highBTrace)
		highBTrace = cm_numBrushTraces;
	if (cm_numPointContents > highPC)
		highPC = cm_numPointContents;

	// Reset
	cm_numTraces = 0;
	cm_numBrushTraces = 0;
	cm_numPointContents = 0;
}
