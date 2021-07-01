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
// cm_q2_local.h
// Quake2 BSP map model collision header
//

#include "cm_common.h"

// ==========================================================================

struct cmQ2BspNode_t
{
	plane_t			*plane;
	int				children[2];		// negative numbers are leafs
};

struct cmQ2BspLeaf_t
{
	int				contents;
	int				cluster;
	int				area;
	uint16			firstLeafBrush;
	uint16			numLeafBrushes;
};

// used internally due to name len probs - ZOID
struct cmQ2BspSurface_t
{
	cmBspSurface_t	c;
	char			rname[32];
};

struct cmQ2BspBrushSide_t
{
	plane_t			*plane;
	cmQ2BspSurface_t *surface;
};

struct cmQ2BspBrush_t
{
	int				contents;
	int				numSides;
	int				firstBrushSide;
	int				checkCount;			// to avoid repeated testings
};

struct cmQ2BspArea_t
{
	int				numAreaPortals;
	int				firstAreaPortal;
	int				floodNum;			// if two areas have equal floodnums, they are connected
	int				floodValid;
};

// ==========================================================================

extern int					cm_q2_numNodes;
extern cmQ2BspNode_t		*cm_q2_nodes;

extern int					cm_q2_numBrushSides;
extern cmQ2BspBrushSide_t	*cm_q2_brushSides;

extern int					cm_q2_numLeafs;
extern int					cm_q2_emptyLeaf;
extern cmQ2BspLeaf_t		*cm_q2_leafs;

extern int					cm_q2_numLeafBrushes;
extern uint16				*cm_q2_leafBrushes;

extern int					cm_q2_numBrushes;
extern cmQ2BspBrush_t		*cm_q2_brushes;

extern int					cm_q2_numAreas;
extern cmQ2BspArea_t		*cm_q2_areas;

extern cmQ2BspSurface_t		cm_q2_nullSurface;

extern int					cm_q2_numPlanes;
extern plane_t				*cm_q2_planes;

extern int					cm_q2_numVisibility;
extern dQ2BspVis_t			*cm_q2_visData;

extern int					cm_q2_numAreaPortals;
extern dQ2BspAreaPortal_t	*cm_q2_areaPortals;
extern bool				*cm_q2_portalOpen;

extern int					cm_q2_numClusters;

// ==========================================================================

void		CM_Q2BSP_InitBoxHull ();
void		CM_Q2BSP_FloodAreaConnections ();
