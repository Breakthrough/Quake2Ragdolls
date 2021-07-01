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
// cm_q3_local.h
// Quake3 BSP map model collision header
//

#include "cm_common.h"

// ==========================================================================

#define MAX_Q3BSP_CM_AREAPORTALS	(MAX_CS_EDICTS)
#define MAX_Q3BSP_CM_AREAS			(Q3BSP_MAX_AREAS)
#define MAX_Q3BSP_CM_BRUSHSIDES		(Q3BSP_MAX_BRUSHSIDES << 1)
#define MAX_Q3BSP_CM_MATERIALS		(Q3BSP_MAX_MATERIALS)
#define MAX_Q3BSP_CM_PLANES			(Q3BSP_MAX_PLANES << 2)
#define MAX_Q3BSP_CM_NODES			(Q3BSP_MAX_NODES)
#define MAX_Q3BSP_CM_LEAFS			(Q3BSP_MAX_LEAFS)
#define MAX_Q3BSP_CM_LEAFBRUSHES	(Q3BSP_MAX_LEAFBRUSHES)
#define MAX_Q3BSP_CM_MODELS			(Q3BSP_MAX_MODELS)
#define MAX_Q3BSP_CM_BRUSHES		(Q3BSP_MAX_BRUSHES << 1)
#define MAX_Q3BSP_CM_VISIBILITY		(Q3BSP_MAX_VISIBILITY)
#define MAX_Q3BSP_CM_FACES			(Q3BSP_MAX_FACES)
#define MAX_Q3BSP_CM_LEAFFACES		(Q3BSP_MAX_LEAFFACES)
#define MAX_Q3BSP_CM_VERTEXES		(Q3BSP_MAX_VERTEXES)
#define MAX_Q3BSP_CM_PATCHES		(0x10000)
#define MAX_Q3BSP_CM_PATCH_VERTS	(4096)
#define MAX_Q3BSP_CM_ENTSTRING		(Q3BSP_MAX_ENTSTRING)

#define CM_SUBDIVLEVEL				(15)

struct cmQ3BspFace_t
{
	int					faceType;

	int					numVerts;
	int					firstVert;

	int					matNum;
	int					patch_cp[2];
};

struct cmQ3BspNode_t
{
	plane_t				*plane;
	int					children[2];	// negative numbers are leafs
};

struct cmQ3BspBrushSide_t
{
	plane_t				*plane;
	cmBspSurface_t		*surface;
};

struct cmQ3BspLeaf_t
{
	int					contents;
	int					cluster;
	int					area;

	int					firstLeafFace;
	int					numLeafFaces;

	int					firstLeafBrush;
	int					numLeafBrushes;

	int					firstLeafPatch;
	int					numLeafPatches;
};

struct cmQ3BspBrush_t
{
	int					contents;
	int					numSides;
	int					firstBrushSide;
	int					checkCount;		// to avoid repeated testings
};

struct cmQ3BspPatch_t
{
	vec3_t				absMins, absMaxs;

	int					numBrushes;
	cmQ3BspBrush_t		*brushes;

	cmBspSurface_t		*surface;
	int					checkCount;		// to avoid repeated testings
};

struct cmQ3BspAreaPortal_t
{
	bool				open;
	int					area;
	int					otherArea;
};

struct cmQ3BspArea_t
{
	int					numAreaPortals;
	int					areaPortals[MAX_Q3BSP_CM_AREAS];
	int					floodNum;		// if two areas have equal floodnums, they are connected
	int					floodValid;
};

// ==========================================================================

extern int					cm_q3_numBrushSides;
extern cmQ3BspBrushSide_t	*cm_q3_brushSides;

extern int					cm_q3_numMaterialRefs;
extern cmBspSurface_t		*cm_q3_surfaces;

extern int					cm_q3_numPlanes;
extern plane_t				*cm_q3_planes;

extern int					cm_q3_numNodes;
extern cmQ3BspNode_t		*cm_q3_nodes;

extern int					cm_q3_numLeafs;						// allow leaf funcs to be called without a map
extern cmQ3BspLeaf_t		*cm_q3_leafs;

extern int					cm_q3_numLeafBrushes;
extern int					*cm_q3_leafBrushes;

extern int					cm_q3_numBrushes;
extern cmQ3BspBrush_t		*cm_q3_brushes;

extern dQ3BspVis_t			*cm_q3_visData;
extern dQ3BspVis_t			*cm_q3_hearData;

extern byte					*cm_q3_nullRow;

extern int					cm_q3_numAreaPortals;
extern cmQ3BspAreaPortal_t	*cm_q3_areaPortals;

extern int					cm_q3_numAreas;
extern cmQ3BspArea_t		*cm_q3_areas;

extern cmBspSurface_t		cm_q3_nullSurface;

extern int					cm_q3_emptyLeaf;

extern int					cm_q3_numPatches;
extern cmQ3BspPatch_t		*cm_q3_patches;

extern int					cm_q3_numLeafPatches;
extern int					*cm_q3_leafPatches;

// ==========================================================================

void		CM_Q3BSP_InitBoxHull ();
void		CM_Q3BSP_FloodAreaConnections ();
