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
// cm_q3_trace.c
// Quake3 BSP map model tracing
//

#include "cm_q3_local.h"

static int			cm_q3_checkCount;
static int			cm_q3_floodValid;

static plane_t		*cm_q3_boxPlanes;
static int			cm_q3_boxHeadNode;
static cmQ3BspBrush_t *cm_q3_boxBrush;
static cmQ3BspLeaf_t *cm_q3_boxLeaf;

static int			cm_q3_leafCount, cm_q3_leafMaxCount;
static int			*cm_q3_leafList;
static float		*cm_q3_leafMins, *cm_q3_leafMaxs;
static int			cm_q3_leafTopNode;

// 1/32 epsilon to keep floating point happy
#define DIST_EPSILON	(0.03125f)

static cmTrace_t		cm_q3_currentTrace;
static vec3_t		cm_q3_traceStart, cm_q3_traceStartMins, cm_q3_traceStartMaxs;
static vec3_t		cm_q3_traceEnd, cm_q3_traceEndMins, cm_q3_traceEndMaxs;
static vec3_t		cm_q3_traceMins, cm_q3_traceMaxs;
static vec3_t		cm_q3_traceAbsMins, cm_q3_traceAbsMaxs;
static vec3_t		cm_q3_traceExtents;
static int			cm_q3_traceContents;
static bool			cm_q3_traceIsPoint;		// Optimized case

/*
=============================================================================

	TRACING

=============================================================================
*/

/*
==================
CM_Q3BSP_LeafArea
==================
*/
int CM_Q3BSP_LeafArea (int leafNum)
{
	if (leafNum < 0 || leafNum >= cm_q3_numLeafs)
		Com_Error (ERR_DROP, "CM_Q3BSP_LeafArea: bad number");

	return cm_q3_leafs[leafNum].area;
}


/*
==================
CM_Q3BSP_LeafCluster
==================
*/
int CM_Q3BSP_LeafCluster (int leafNum)
{
	if (leafNum < 0 || leafNum >= cm_q3_numLeafs)
		Com_Error (ERR_DROP, "CM_Q3BSP_LeafCluster: bad number");

	return cm_q3_leafs[leafNum].cluster;
}


/*
==================
CM_Q3BSP_LeafContents
==================
*/
int CM_Q3BSP_LeafContents (int leafNum)
{
	if (leafNum < 0 || leafNum >= cm_q3_numLeafs)
		Com_Error (ERR_DROP, "CM_Q3BSP_LeafContents: bad number");

	return cm_q3_leafs[leafNum].contents;
}

// ==========================================================================

/*
===================
CM_Q3BSP_InitBoxHull
===================
*/
void CM_Q3BSP_InitBoxHull ()
{
	int				i;
	int				side;
	cmQ3BspNode_t *c;
	plane_t *p;
	cmQ3BspBrushSide_t *s;

	cm_q3_boxHeadNode = cm_q3_numNodes;
	cm_q3_boxPlanes = &cm_q3_planes[cm_q3_numPlanes];
	if (cm_q3_numNodes+6 > MAX_Q3BSP_CM_NODES
	|| cm_q3_numBrushes+1 > MAX_Q3BSP_CM_BRUSHES
	|| cm_q3_numLeafBrushes+1 > MAX_Q3BSP_CM_LEAFBRUSHES
	|| cm_q3_numBrushSides+6 > MAX_Q3BSP_CM_BRUSHSIDES
	|| cm_q3_numPlanes+12 > MAX_Q3BSP_CM_PLANES)
		Com_Error (ERR_DROP, "CM_Q3BSP_InitBoxHull: Not enough room for box tree");

	cm_q3_boxBrush = &cm_q3_brushes[cm_q3_numBrushes];
	cm_q3_boxBrush->numSides = 6;
	cm_q3_boxBrush->firstBrushSide = cm_q3_numBrushSides;
	cm_q3_boxBrush->contents = Q3CNTNTS_BODY;

	cm_q3_boxLeaf = &cm_q3_leafs[cm_q3_numLeafs];
	cm_q3_boxLeaf->contents = Q3CNTNTS_BODY;
	cm_q3_boxLeaf->firstLeafBrush = cm_q3_numLeafBrushes;
	cm_q3_boxLeaf->numLeafBrushes = 1;

	cm_q3_leafBrushes[cm_q3_numLeafBrushes] = cm_q3_numBrushes;

	for (i=0 ; i<6 ; i++) {
		side = i&1;

		// Brush sides
		s = &cm_q3_brushSides[cm_q3_numBrushSides+i];
		s->plane = cm_q3_planes + (cm_q3_numPlanes+i*2+side);
		s->surface = &cm_q3_nullSurface;

		// Nodes
		c = &cm_q3_nodes[cm_q3_boxHeadNode+i];
		c->plane = cm_q3_planes + (cm_q3_numPlanes+i*2);
		c->children[side] = -1 - cm_q3_emptyLeaf;
		if (i != 5)
			c->children[side^1] = cm_q3_boxHeadNode+i + 1;
		else
			c->children[side^1] = -1 - cm_q3_numLeafs;

		// Planes
		p = &cm_q3_boxPlanes[i*2];
		p->type = i>>1;
		p->signBits = 0;
		Vec3Clear (p->normal);
		p->normal[i>>1] = 1;

		p = &cm_q3_boxPlanes[i*2+1];
		p->type = 3 + (i>>1);
		p->signBits = 0;
		Vec3Clear (p->normal);
		p->normal[i>>1] = -1;
	}
}


/*
===================
CM_Q3BSP_HeadnodeForBox
===================
*/
int	CM_Q3BSP_HeadnodeForBox (vec3_t mins, vec3_t maxs)
{
	cm_q3_boxPlanes[0].dist = maxs[0];
	cm_q3_boxPlanes[1].dist = -maxs[0];
	cm_q3_boxPlanes[2].dist = mins[0];
	cm_q3_boxPlanes[3].dist = -mins[0];
	cm_q3_boxPlanes[4].dist = maxs[1];
	cm_q3_boxPlanes[5].dist = -maxs[1];
	cm_q3_boxPlanes[6].dist = mins[1];
	cm_q3_boxPlanes[7].dist = -mins[1];
	cm_q3_boxPlanes[8].dist = maxs[2];
	cm_q3_boxPlanes[9].dist = -maxs[2];
	cm_q3_boxPlanes[10].dist = mins[2];
	cm_q3_boxPlanes[11].dist = -mins[2];

	return cm_q3_boxHeadNode;
}


/*
==================
CM_Q3BSP_PointLeafnum
==================
*/
int CM_PointLeafnum_r (vec3_t p, int num)
{
	float		d;
	cmQ3BspNode_t *node;
	plane_t	*plane;

	while (num >= 0) {
		node = cm_q3_nodes + num;
		plane = node->plane;
		d = PlaneDiff (p, plane);

		if (d < 0)
			num = node->children[1];
		else
			num = node->children[0];
	}

	// Optimize counter
	cm_numPointContents++;
	return -1 - num;
}
int CM_Q3BSP_PointLeafnum (vec3_t p)
{
	if (!cm_q3_numPlanes)
		return 0;		// Sound may call this without map loaded
	return CM_PointLeafnum_r (p, 0);
}


/*
==================
CM_Q3BSP_BoxLeafnums
==================
*/
static void CM_Q3BSP_BoxLeafnums_r (int nodeNum)
{
	cmQ3BspNode_t *node;
	int		s;

	for ( ; ; ) {
		if (nodeNum < 0) {
			if (cm_q3_leafCount >= cm_q3_leafMaxCount)
				return;

			cm_q3_leafList[cm_q3_leafCount++] = -1 - nodeNum;
			return;
		}
	
		node = &cm_q3_nodes[nodeNum];
		s = BoxOnPlaneSide (cm_q3_leafMins, cm_q3_leafMaxs, node->plane);

		if (s == 1) {
			nodeNum = node->children[0];
		}
		else if (s == 2) {
			nodeNum = node->children[1];
		}
		else {
			// Go down both
			if (cm_q3_leafTopNode == -1)
				cm_q3_leafTopNode = nodeNum;
			CM_Q3BSP_BoxLeafnums_r (node->children[0]);
			nodeNum = node->children[1];
		}
	}
}
static int CM_Q3BSP_BoxLeafnums_headnode (vec3_t mins, vec3_t maxs, int *list, int listSize, int headNode, int *topNode)
{
	cm_q3_leafList = list;
	cm_q3_leafCount = 0;
	cm_q3_leafMaxCount = listSize;
	cm_q3_leafMins = mins;
	cm_q3_leafMaxs = maxs;
	cm_q3_leafTopNode = -1;

	CM_Q3BSP_BoxLeafnums_r (headNode);

	if (topNode)
		*topNode = cm_q3_leafTopNode;

	return cm_q3_leafCount;
}
int	CM_Q3BSP_BoxLeafnums (vec3_t mins, vec3_t maxs, int *list, int listSize, int *topNode)
{
	return CM_Q3BSP_BoxLeafnums_headnode (mins, maxs, list, listSize, cm_mapCModels[0].headNode, topNode);
}


/*
==================
CM_Q3BSP_PointContents
==================
*/
int CM_Q3BSP_PointContents (vec3_t p, int headNode)
{
	int				i, j, contents;
	cmQ3BspLeaf_t	*leaf;
	cmQ3BspBrush_t	*brush;
	cmQ3BspBrushSide_t *brushSide;

	if (!cm_q3_numNodes)
		return 0;	// Map not loaded

	i = CM_PointLeafnum_r (p, headNode);
	leaf = &cm_q3_leafs[i];

	if (leaf->contents & Q3CNTNTS_NODROP)
		contents = Q3CNTNTS_NODROP;
	else
		contents = 0;

	for (i=0 ; i<leaf->numLeafBrushes ; i++) {
		brush = &cm_q3_brushes[cm_q3_leafBrushes[leaf->firstLeafBrush + i]];

		// Check if brush actually adds something to contents
		if ((contents & brush->contents) == brush->contents)
			continue;
		
		brushSide = &cm_q3_brushSides[brush->firstBrushSide];
		for (j=0 ; j<brush->numSides ; j++, brushSide++) {
			if (PlaneDiff (p, brushSide->plane) > 0)
				break;
		}

		if (j == brush->numSides) 
			contents |= brush->contents;
	}

	return contents;
}


/*
==================
CM_Q3BSP_TransformedPointContents
==================
*/
int	CM_Q3BSP_TransformedPointContents (vec3_t p, int headNode, vec3_t origin, vec3_t angles)
{
	vec3_t		p_l;
	vec3_t		temp;
	vec3_t		forward, right, up;

	if (!cm_q3_numNodes)
		return 0;	// Map not loaded

	// Subtract origin offset
	Vec3Subtract (p, origin, p_l);

	// Rotate start and end into the models frame of reference
	if (headNode != cm_q3_boxHeadNode && (angles[0] || angles[1] || angles[2])) {
		Angles_Vectors (angles, forward, right, up);

		Vec3Copy (p_l, temp);
		p_l[0] = DotProduct (temp, forward);
		p_l[1] = -DotProduct (temp, right);
		p_l[2] = DotProduct (temp, up);
	}

	return CM_PointContents (p_l, headNode);
}

/*
=============================================================================

	BOX TRACING

=============================================================================
*/

/*
================
CM_Q3BSP_ClipBoxToBrush
================
*/
static void CM_Q3BSP_ClipBoxToBrush (cmQ3BspBrush_t *brush)
{
	int				i;
	plane_t			*p, *clipPlane;
	float			enterFrac, leaveFrac;
	float			d1, d2;
	bool			getOut, startOut;
	float			f;
	cmQ3BspBrushSide_t *side, *leadSide;

	enterFrac = -1;
	leaveFrac = 1;
	clipPlane = NULL;

	if (!brush->numSides)
		return;

	cm_numBrushTraces++;

	getOut = false;
	startOut = false;
	leadSide = NULL;

	for (i=0, side=&cm_q3_brushSides[brush->firstBrushSide] ; i<brush->numSides ; side++, i++) {
		p = side->plane;

		// Push the plane out apropriately for mins/maxs
		if (p->type < 3) {
			d1 = cm_q3_traceStartMins[p->type] - p->dist;
			d2 = cm_q3_traceEndMins[p->type] - p->dist;
		}
		else {
			switch (p->signBits) {
			case 0:
				d1 = p->normal[0]*cm_q3_traceStartMins[0] + p->normal[1]*cm_q3_traceStartMins[1] + p->normal[2]*cm_q3_traceStartMins[2] - p->dist;
				d2 = p->normal[0]*cm_q3_traceEndMins[0] + p->normal[1]*cm_q3_traceEndMins[1] + p->normal[2]*cm_q3_traceEndMins[2] - p->dist;
				break;
			case 1:
				d1 = p->normal[0]*cm_q3_traceStartMaxs[0] + p->normal[1]*cm_q3_traceStartMins[1] + p->normal[2]*cm_q3_traceStartMins[2] - p->dist;
				d2 = p->normal[0]*cm_q3_traceEndMaxs[0] + p->normal[1]*cm_q3_traceEndMins[1] + p->normal[2]*cm_q3_traceEndMins[2] - p->dist;
				break;
			case 2:
				d1 = p->normal[0]*cm_q3_traceStartMins[0] + p->normal[1]*cm_q3_traceStartMaxs[1] + p->normal[2]*cm_q3_traceStartMins[2] - p->dist;
				d2 = p->normal[0]*cm_q3_traceEndMins[0] + p->normal[1]*cm_q3_traceEndMaxs[1] + p->normal[2]*cm_q3_traceEndMins[2] - p->dist;
				break;
			case 3:
				d1 = p->normal[0]*cm_q3_traceStartMaxs[0] + p->normal[1]*cm_q3_traceStartMaxs[1] + p->normal[2]*cm_q3_traceStartMins[2] - p->dist;
				d2 = p->normal[0]*cm_q3_traceEndMaxs[0] + p->normal[1]*cm_q3_traceEndMaxs[1] + p->normal[2]*cm_q3_traceEndMins[2] - p->dist;
				break;
			case 4:
				d1 = p->normal[0]*cm_q3_traceStartMins[0] + p->normal[1]*cm_q3_traceStartMins[1] + p->normal[2]*cm_q3_traceStartMaxs[2] - p->dist;
				d2 = p->normal[0]*cm_q3_traceEndMins[0] + p->normal[1]*cm_q3_traceEndMins[1] + p->normal[2]*cm_q3_traceEndMaxs[2] - p->dist;
				break;
			case 5:
				d1 = p->normal[0]*cm_q3_traceStartMaxs[0] + p->normal[1]*cm_q3_traceStartMins[1] + p->normal[2]*cm_q3_traceStartMaxs[2] - p->dist;
				d2 = p->normal[0]*cm_q3_traceEndMaxs[0] + p->normal[1]*cm_q3_traceEndMins[1] + p->normal[2]*cm_q3_traceEndMaxs[2] - p->dist;
				break;
			case 6:
				d1 = p->normal[0]*cm_q3_traceStartMins[0] + p->normal[1]*cm_q3_traceStartMaxs[1] + p->normal[2]*cm_q3_traceStartMaxs[2] - p->dist;
				d2 = p->normal[0]*cm_q3_traceEndMins[0] + p->normal[1]*cm_q3_traceEndMaxs[1] + p->normal[2]*cm_q3_traceEndMaxs[2] - p->dist;
				break;
			case 7:
				d1 = p->normal[0]*cm_q3_traceStartMaxs[0] + p->normal[1]*cm_q3_traceStartMaxs[1] + p->normal[2]*cm_q3_traceStartMaxs[2] - p->dist;
				d2 = p->normal[0]*cm_q3_traceEndMaxs[0] + p->normal[1]*cm_q3_traceEndMaxs[1] + p->normal[2]*cm_q3_traceEndMaxs[2] - p->dist;
				break;
			default:
				d1 = d2 = 0;	// Shut up compiler
				assert (0);
				break;
			}
		}

		if (d2 > 0)
			getOut = true;	// Endpoint is not in solid
		if (d1 > 0)
			startOut = true;

		// If completely in front of face, no intersection
		if (d1 > 0 && d2 >= d1)
			return;
		if (d1 <= 0 && d2 <= 0)
			continue;

		// Crosses face
		f = d1 - d2;
		if (f > 0) {
			// Enter
			f = (d1 - DIST_EPSILON) / f;
			if (f > enterFrac) {
				enterFrac = f;
				clipPlane = p;
				leadSide = side;
			}
		}
		else {
			// Leave
			f = (d1 + DIST_EPSILON) / f;
			if (f < leaveFrac)
				leaveFrac = f;
		}
	}

	if (!startOut) {
		// Original point was inside brush
		cm_q3_currentTrace.startSolid = true;
		if (!getOut)
			cm_q3_currentTrace.allSolid = true;
		return;
	}

	if (enterFrac-(1.0f/1024.0f) <= leaveFrac) {
		if (enterFrac > -1 && enterFrac < cm_q3_currentTrace.fraction) {
			if (enterFrac < 0)
				enterFrac = 0;
			cm_q3_currentTrace.fraction = enterFrac;
			cm_q3_currentTrace.plane = *clipPlane;
			cm_q3_currentTrace.surface = leadSide->surface;
			cm_q3_currentTrace.contents = brush->contents;
		}
	}
}


/*
================
CM_Q3BSP_ClipBoxes
================
*/
static void CM_Q3BSP_ClipBoxes (int leafNum)
{
	int			i, j;
	int			brushNum, patchNum;
	cmQ3BspLeaf_t *leaf;
	cmQ3BspBrush_t *brush;
	cmQ3BspPatch_t *patch;

	leaf = &cm_q3_leafs[leafNum];
	if (!(leaf->contents & cm_q3_traceContents))
		return;

	// Trace line against all brushes in the leaf
	for (i=0 ; i<leaf->numLeafBrushes ; i++) {
		brushNum = cm_q3_leafBrushes[leaf->firstLeafBrush+i];
		brush = &cm_q3_brushes[brushNum];

		if (brush->checkCount == cm_q3_checkCount)
			continue;	// Already checked this brush in another leaf
		brush->checkCount = cm_q3_checkCount;
		if (!(brush->contents & cm_q3_traceContents))
			continue;

		CM_Q3BSP_ClipBoxToBrush (brush);
		if (!cm_q3_currentTrace.fraction)
			return;
	}

	if (cm_noCurves->intVal)
		return;

	// Trace line against all patches in the leaf
	for (i=0 ; i<leaf->numLeafPatches ; i++) {
		patchNum = cm_q3_leafPatches[leaf->firstLeafPatch+i];
		patch = &cm_q3_patches[patchNum];

		if (patch->checkCount == cm_q3_checkCount)
			continue;	// Already checked this patch in another leaf
		patch->checkCount = cm_q3_checkCount;
		if (!(patch->surface->contents & cm_q3_traceContents))
			continue;
		if (!BoundsIntersect(patch->absMins, patch->absMaxs, cm_q3_traceAbsMins, cm_q3_traceAbsMaxs))
			continue;

		for (j=0 ; j<patch->numBrushes ; j++) {
			CM_Q3BSP_ClipBoxToBrush (&patch->brushes[j]);
			if (!cm_q3_currentTrace.fraction)
				return;
		}
	}
}


/*
================
CM_Q3BSP_TestBoxInBrush
================
*/
static void CM_Q3BSP_TestBoxInBrush (cmQ3BspBrush_t *brush)
{
	int				i;
	plane_t			*p;
	cmQ3BspBrushSide_t *side;

	if (!brush->numSides)
		return;

	for (i=0, side=&cm_q3_brushSides[brush->firstBrushSide] ; i<brush->numSides ; side++, i++) {
		p = side->plane;

		// Push the plane out apropriately for mins/maxs
		// if completely in front of face, no intersection
		if (p->type < 3) {
			if (cm_q3_traceStartMins[p->type] > p->dist)
				return;
		}
		else {
			switch (p->signBits) {
			case 0:
				if (p->normal[0]*cm_q3_traceStartMins[0] + p->normal[1]*cm_q3_traceStartMins[1] + p->normal[2]*cm_q3_traceStartMins[2] > p->dist)
					return;
				break;
			case 1:
				if (p->normal[0]*cm_q3_traceStartMaxs[0] + p->normal[1]*cm_q3_traceStartMins[1] + p->normal[2]*cm_q3_traceStartMins[2] > p->dist)
					return;
				break;
			case 2:
				if (p->normal[0]*cm_q3_traceStartMins[0] + p->normal[1]*cm_q3_traceStartMaxs[1] + p->normal[2]*cm_q3_traceStartMins[2] > p->dist)
					return;
				break;
			case 3:
				if (p->normal[0]*cm_q3_traceStartMaxs[0] + p->normal[1]*cm_q3_traceStartMaxs[1] + p->normal[2]*cm_q3_traceStartMins[2] > p->dist)
					return;
				break;
			case 4:
				if (p->normal[0]*cm_q3_traceStartMins[0] + p->normal[1]*cm_q3_traceStartMins[1] + p->normal[2]*cm_q3_traceStartMaxs[2] > p->dist)
					return;
				break;
			case 5:
				if (p->normal[0]*cm_q3_traceStartMaxs[0] + p->normal[1]*cm_q3_traceStartMins[1] + p->normal[2]*cm_q3_traceStartMaxs[2] > p->dist)
					return;
				break;
			case 6:
				if (p->normal[0]*cm_q3_traceStartMins[0] + p->normal[1]*cm_q3_traceStartMaxs[1] + p->normal[2]*cm_q3_traceStartMaxs[2] > p->dist)
					return;
				break;
			case 7:
				if (p->normal[0]*cm_q3_traceStartMaxs[0] + p->normal[1]*cm_q3_traceStartMaxs[1] + p->normal[2]*cm_q3_traceStartMaxs[2] > p->dist)
					return;
				break;
			default:
				assert (0);
				return;
			}
		}
	}

	// Inside this brush
	cm_q3_currentTrace.startSolid = cm_q3_currentTrace.allSolid = true;
	cm_q3_currentTrace.fraction = 0;
	cm_q3_currentTrace.contents = brush->contents;
}


/*
================
CM_Q3BSP_TestBoxInLeaf
================
*/
static void CM_Q3BSP_TestBoxInLeaf (int leafNum)
{
	int			i, j;
	int			brushNum, patchNum;
	cmQ3BspLeaf_t *leaf;
	cmQ3BspBrush_t *brush;
	cmQ3BspPatch_t *patch;

	leaf = &cm_q3_leafs[leafNum];
	if (!(leaf->contents & cm_q3_traceContents))
		return;

	// Trace line against all brushes in the leaf
	for (i=0 ; i<leaf->numLeafBrushes ; i++) {
		brushNum = cm_q3_leafBrushes[leaf->firstLeafBrush+i];
		brush = &cm_q3_brushes[brushNum];

		if (brush->checkCount == cm_q3_checkCount)
			continue;	// Already checked this brush in another leaf
		brush->checkCount = cm_q3_checkCount;
		if (!(brush->contents & cm_q3_traceContents))
			continue;

		CM_Q3BSP_TestBoxInBrush (brush);
		if (!cm_q3_currentTrace.fraction)
			return;
	}

	if (cm_noCurves->intVal)
		return;

	// Trace line against all patches in the leaf
	for (i=0 ; i<leaf->numLeafPatches ; i++) {
		patchNum = cm_q3_leafPatches[leaf->firstLeafPatch+i];
		patch = &cm_q3_patches[patchNum];

		if (patch->checkCount == cm_q3_checkCount)
			continue;	// Already checked this patch in another leaf
		patch->checkCount = cm_q3_checkCount;
		if (!(patch->surface->contents & cm_q3_traceContents))
			continue;
		if (!BoundsIntersect(patch->absMins, patch->absMaxs, cm_q3_traceAbsMins, cm_q3_traceAbsMaxs))
			continue;

		for (j=0 ; j<patch->numBrushes; j++) {
			CM_Q3BSP_TestBoxInBrush (&patch->brushes[j]);
			if (!cm_q3_currentTrace.fraction)
				return;
		}
	}
}


/*
==================
CM_Q3BSP_RecursiveHullCheck
==================
*/
static void CM_Q3BSP_RecursiveHullCheck (int num, float p1f, float p2f, vec3_t p1, vec3_t p2)
{
	cmQ3BspNode_t *node;
	plane_t		*plane;
	float		t1, t2, offset;
	float		frac, frac2;
	float		idist;
	int			i;
	vec3_t		mid;
	int			side;
	float		midf;

	if (cm_q3_currentTrace.fraction <= p1f)
		return;		// Already hit something nearer

	// If < 0, we are in a leaf node
	if (num < 0) {
		CM_Q3BSP_ClipBoxes (-1-num);
		return;
	}

	//
	// Find the point distances to the seperating plane
	// and the offset for the size of the box
	//
	node = cm_q3_nodes + num;
	plane = node->plane;

	if (plane->type < 3) {
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
		offset = cm_q3_traceExtents[plane->type];
	}
	else {
		t1 = DotProduct (plane->normal, p1) - plane->dist;
		t2 = DotProduct (plane->normal, p2) - plane->dist;
		if (cm_q3_traceIsPoint)
			offset = 0;
		else
			offset = fabs(cm_q3_traceExtents[0]*plane->normal[0])
				+ fabs(cm_q3_traceExtents[1]*plane->normal[1])
				+ fabs(cm_q3_traceExtents[2]*plane->normal[2]);
	}


	// See which sides we need to consider
	if (t1 >= offset && t2 >= offset) {
		CM_Q3BSP_RecursiveHullCheck (node->children[0], p1f, p2f, p1, p2);
		return;
	}
	if (t1 < -offset && t2 < -offset) {
		CM_Q3BSP_RecursiveHullCheck (node->children[1], p1f, p2f, p1, p2);
		return;
	}

	// Put the crosspoint DIST_EPSILON pixels on the near side
	if (t1 < t2) {
		idist = 1.0/(t1-t2);
		side = 1;
		frac2 = (t1 + offset + DIST_EPSILON)*idist;
		frac = (t1 - offset + DIST_EPSILON)*idist;
	}
	else if (t1 > t2) {
		idist = 1.0/(t1-t2);
		side = 0;
		frac2 = (t1 - offset - DIST_EPSILON)*idist;
		frac = (t1 + offset + DIST_EPSILON)*idist;
	}
	else {
		side = 0;
		frac = 1;
		frac2 = 0;
	}

	// Move up to the node
	if (frac < 0)
		frac = 0;
	if (frac > 1)
		frac = 1;

	midf = p1f + (p2f - p1f)*frac;
	for (i=0 ; i<3 ; i++)
		mid[i] = p1[i] + frac*(p2[i] - p1[i]);

	CM_Q3BSP_RecursiveHullCheck (node->children[side], p1f, midf, p1, mid);

	// Go past the node
	if (frac2 < 0)
		frac2 = 0;
	if (frac2 > 1)
		frac2 = 1;

	midf = p1f + (p2f - p1f)*frac2;
	for (i=0 ; i<3 ; i++)
		mid[i] = p1[i] + frac2*(p2[i] - p1[i]);

	CM_Q3BSP_RecursiveHullCheck (node->children[side^1], midf, p2f, mid, p2);
}

// ==========================================================================

/*
====================
CM_Q3BSP_Trace
====================
*/
cmTrace_t CM_Q3BSP_Trace (vec3_t start, vec3_t end, float size, int contentMask)
{
	vec3_t maxs, mins;

	Vec3Set (maxs, size, size, size);
	Vec3Set (mins, -size, -size, -size);

	return CM_Q3BSP_BoxTrace (start, end, mins, maxs, 0, contentMask);
}


/*
==================
CM_Q3BSP_BoxTrace
==================
*/
cmTrace_t CM_Q3BSP_BoxTrace (vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, int headNode, int brushMask)
{
	cm_q3_checkCount++;		// For multi-check avoidance
	cm_numTraces++;			// For statistics, may be zeroed

	// Fill in a default trace
	cm_q3_currentTrace.allSolid = false;
	cm_q3_currentTrace.contents = 0;
	Vec3Clear (cm_q3_currentTrace.endPos);
	cm_q3_currentTrace.ent = NULL;
	cm_q3_currentTrace.fraction = 1;
	cm_q3_currentTrace.plane.dist = 0;
	Vec3Clear (cm_q3_currentTrace.plane.normal);
	cm_q3_currentTrace.plane.signBits = 0;
	cm_q3_currentTrace.plane.type = 0;
	cm_q3_currentTrace.startSolid = false;
	cm_q3_currentTrace.surface = &cm_q3_nullSurface;

	if (!cm_q3_numNodes)	// map not loaded
		return cm_q3_currentTrace;

	cm_q3_traceContents = brushMask;
	Vec3Copy (start, cm_q3_traceStart);
	Vec3Copy (end, cm_q3_traceEnd);
	Vec3Copy (mins, cm_q3_traceMins);
	Vec3Copy (maxs, cm_q3_traceMaxs);

	// Build a bounding box of the entire move
	ClearBounds (cm_q3_traceAbsMins, cm_q3_traceAbsMaxs);

	Vec3Add (start, cm_q3_traceMins, cm_q3_traceStartMins);
	AddPointToBounds (cm_q3_traceStartMins, cm_q3_traceAbsMins, cm_q3_traceAbsMaxs);
	Vec3Add (start, cm_q3_traceMaxs, cm_q3_traceStartMaxs);
	AddPointToBounds (cm_q3_traceStartMaxs, cm_q3_traceAbsMins, cm_q3_traceAbsMaxs);
	Vec3Add (end, cm_q3_traceMins, cm_q3_traceEndMins);
	AddPointToBounds (cm_q3_traceEndMins, cm_q3_traceAbsMins, cm_q3_traceAbsMaxs);
	Vec3Add (end, cm_q3_traceMaxs, cm_q3_traceEndMaxs);
	AddPointToBounds (cm_q3_traceEndMaxs, cm_q3_traceAbsMins, cm_q3_traceAbsMaxs);

	// Check for position test special case
	if (start[0] == end[0] && start[1] == end[1] && start[2] == end[2]) {
		int		leafs[1024];
		int		i, numLeafs;
		vec3_t	c1, c2;
		int		topnode;

		Vec3Add (start, mins, c1);
		Vec3Add (start, maxs, c2);
		for (i=0 ; i<3 ; i++) {
			c1[i] -= 1;
			c2[i] += 1;
		}

		numLeafs = CM_Q3BSP_BoxLeafnums_headnode (c1, c2, leafs, 1024, headNode, &topnode);
		for (i=0 ; i<numLeafs ; i++) {
			CM_Q3BSP_TestBoxInLeaf (leafs[i]);
			if (cm_q3_currentTrace.allSolid)
				break;
		}
		Vec3Copy (start, cm_q3_currentTrace.endPos);
		return cm_q3_currentTrace;
	}

	// Check for point special case
	if (mins[0] == 0 && mins[1] == 0 && mins[2] == 0 && maxs[0] == 0 && maxs[1] == 0 && maxs[2] == 0) {
		cm_q3_traceIsPoint = true;
		Vec3Clear (cm_q3_traceExtents);
	}
	else {
		cm_q3_traceIsPoint = false;
		cm_q3_traceExtents[0] = -mins[0] > maxs[0] ? -mins[0] : maxs[0];
		cm_q3_traceExtents[1] = -mins[1] > maxs[1] ? -mins[1] : maxs[1];
		cm_q3_traceExtents[2] = -mins[2] > maxs[2] ? -mins[2] : maxs[2];
	}

	// General sweeping through world
	CM_Q3BSP_RecursiveHullCheck (headNode, 0, 1, start, end);

	if (cm_q3_currentTrace.fraction == 1) {
		Vec3Copy (end, cm_q3_currentTrace.endPos);
	}
	else {
		cm_q3_currentTrace.endPos[0] = start[0] + cm_q3_currentTrace.fraction * (end[0] - start[0]);
		cm_q3_currentTrace.endPos[1] = start[1] + cm_q3_currentTrace.fraction * (end[1] - start[1]);
		cm_q3_currentTrace.endPos[2] = start[2] + cm_q3_currentTrace.fraction * (end[2] - start[2]);
	}
	return cm_q3_currentTrace;
}


/*
==================
CM_Q3BSP_TransformedBoxTrace
==================
*/
#ifdef WIN32
#pragma optimize( "", off )
#endif
void CM_Q3BSP_TransformedBoxTrace (cmTrace_t *out, vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, int headNode, int brushMask, vec3_t origin, vec3_t angles)
{
	vec3_t		start_l, end_l;
	vec3_t		a;
	vec3_t		forward, right, up;
	vec3_t		temp;
	bool		rotated;

	// Subtract origin offset
	Vec3Subtract (start, origin, start_l);
	Vec3Subtract (end, origin, end_l);

	// Rotate start and end into the models frame of reference
	if (headNode != cm_q3_boxHeadNode && (angles[0] || angles[1] || angles[2]))
		rotated = true;
	else
		rotated = false;

	if (rotated) {
		Angles_Vectors (angles, forward, right, up);

		Vec3Copy (start_l, temp);
		start_l[0] = DotProduct (temp, forward);
		start_l[1] = -DotProduct (temp, right);
		start_l[2] = DotProduct (temp, up);

		Vec3Copy (end_l, temp);
		end_l[0] = DotProduct (temp, forward);
		end_l[1] = -DotProduct (temp, right);
		end_l[2] = DotProduct (temp, up);
	}

	// Sweep the box through the model
	*out = CM_Q3BSP_BoxTrace (start_l, end_l, mins, maxs, headNode, brushMask);

	if (rotated && out->fraction != 1.0) {
		// FIXME: figure out how to do this with existing angles
		Vec3Negate (angles, a);
		Angles_Vectors (a, forward, right, up);

		Vec3Copy (out->plane.normal, temp);
		out->plane.normal[0] = DotProduct (temp, forward);
		out->plane.normal[1] = -DotProduct (temp, right);
		out->plane.normal[2] = DotProduct (temp, up);
	}

	out->endPos[0] = start[0] + out->fraction * (end[0] - start[0]);
	out->endPos[1] = start[1] + out->fraction * (end[1] - start[1]);
	out->endPos[2] = start[2] + out->fraction * (end[2] - start[2]);
}
#ifdef WIN32
#pragma optimize( "", on )
#endif

/*
=============================================================================

	PVS / PHS

=============================================================================
*/

/*
===================
CM_Q3BSP_ClusterPVS
===================
*/
byte *CM_Q3BSP_ClusterPVS (int cluster)
{
	if (cluster != -1 && cm_q3_visData && cm_q3_visData->numClusters)
		return (byte *)cm_q3_visData->data + cluster * cm_q3_visData->rowSize;

	return cm_q3_nullRow;
}


/*
===================
CM_Q3BSP_ClusterPHS
===================
*/
byte *CM_Q3BSP_ClusterPHS (int cluster)
{
	if (cluster != -1 && cm_q3_hearData && cm_q3_hearData->numClusters)
		return (byte *)cm_q3_hearData->data + cluster * cm_q3_hearData->rowSize;

	return cm_q3_nullRow;
}

/*
=============================================================================

	AREAPORTALS

=============================================================================
*/

/*
=================
CM_Q3BSP_AddAreaPortal
=================
*/
static bool CM_Q3BSP_AddAreaPortal (int portalNum, int area, int otherArea)
{
	cmQ3BspArea_t *a;
	cmQ3BspAreaPortal_t *ap;

	if (portalNum >= MAX_Q3BSP_CM_AREAPORTALS)
		return false;
	if (!area || area > cm_q3_numAreas || !otherArea || otherArea > cm_q3_numAreas)
		return false;

	ap = &cm_q3_areaPortals[portalNum];
	ap->area = area;
	ap->otherArea = otherArea;

	a = &cm_q3_areas[area];
	a->areaPortals[a->numAreaPortals++] = portalNum;

	a = &cm_q3_areas[otherArea];
	a->areaPortals[a->numAreaPortals++] = portalNum;

	cm_q3_numAreaPortals++;
	return true;
}


/*
=================
CM_Q3BSP_FloodArea_r
=================
*/
static void CM_Q3BSP_FloodArea_r (int areaNum, int floodNum)
{
	int i;
	cmQ3BspArea_t *area;
	cmQ3BspAreaPortal_t *p;

	area = &cm_q3_areas[areaNum];
	if (area->floodValid == cm_q3_floodValid) {
		if (area->floodNum == floodNum)
			return;
		Com_Error (ERR_DROP, "CM_Q3BSP_FloodArea_r: reflooded");
	}

	area->floodNum = floodNum;
	area->floodValid = cm_q3_floodValid;
	for (i=0 ; i<area->numAreaPortals ; i++) {
		p = &cm_q3_areaPortals[area->areaPortals[i]];
		if (!p->open)
			continue;

		if (p->area == areaNum)
			CM_Q3BSP_FloodArea_r (p->otherArea, floodNum);
		else if (p->otherArea == areaNum)
			CM_Q3BSP_FloodArea_r (p->area, floodNum);
	}
}


/*
====================
CM_Q3BSP_FloodAreaConnections
====================
*/
void CM_Q3BSP_FloodAreaConnections ()
{
	int		i;
	int		floodNum;

	// All current floods are now invalid
	cm_q3_floodValid++;
	floodNum = 0;

	// Area 0 is not used
	for (i=1 ; i<cm_q3_numAreas ; i++) {
		if (cm_q3_areas[i].floodValid == cm_q3_floodValid)
			continue;		// already flooded into
		floodNum++;
		CM_Q3BSP_FloodArea_r (i, floodNum);
	}
}


/*
====================
CM_Q3BSP_SetAreaPortalState
====================
*/
void CM_Q3BSP_SetAreaPortalState (int portalNum, int area, int otherArea, bool open)
{
	if (portalNum >= MAX_Q3BSP_CM_AREAPORTALS)
		Com_Error (ERR_DROP, "portalNum >= MAX_Q3BSP_CM_AREAPORTALS");

	if (!cm_q3_areaPortals[portalNum].area) {
		// Add new areaportal if it doesn't exist
		if (!CM_Q3BSP_AddAreaPortal (portalNum, area, otherArea))
			return;
	}

	cm_q3_areaPortals[portalNum].open = open;
	CM_Q3BSP_FloodAreaConnections ();
}


/*
====================
CM_Q3BSP_AreasConnected
====================
*/
bool CM_Q3BSP_AreasConnected (int area1, int area2)
{
	if (cm_noAreas->intVal)
		return true;
	if (area1 > cm_q3_numAreas || area2 > cm_q3_numAreas)
		Com_Error (ERR_DROP, "CM_AreasConnected: area > cm_q3_numAreas");

	if (cm_q3_areas[area1].floodNum == cm_q3_areas[area2].floodNum)
		return true;
	return false;
}


/*
=================
CM_Q3BSP_WriteAreaBits
=================
*/
int CM_Q3BSP_WriteAreaBits (byte *buffer, int area)
{
	int		i;
	int		bytes;

	bytes = (cm_q3_numAreas+7)>>3;

	if (cm_noAreas->intVal) {
		// For debugging, send everything
		memset (buffer, 255, bytes);
	}
	else {
		memset (buffer, 0, bytes);

		for (i=1 ; i<cm_q3_numAreas ; i++) {
			if (!area || CM_AreasConnected ( i, area ) || i == area)
				buffer[i>>3] |= BIT(i&7);
		}
	}

	return bytes;
}


/*
===================
CM_Q3BSP_WritePortalState
===================
*/
void CM_Q3BSP_WritePortalState (fileHandle_t fileNum)
{
}


/*
===================
CM_Q3BSP_ReadPortalState
===================
*/
void CM_Q3BSP_ReadPortalState (fileHandle_t fileNum)
{
}


/*
=============
CM_Q3BSP_HeadnodeVisible
=============
*/
bool CM_Q3BSP_HeadnodeVisible (int nodeNum, byte *visBits)
{
	int		leafNum;
	int		cluster;
	cmQ3BspNode_t *node;

	if (nodeNum < 0) {
		leafNum = -1-nodeNum;
		cluster = cm_q3_leafs[leafNum].cluster;
		if (cluster == -1)
			return false;
		if (visBits[cluster>>3] & BIT(cluster&7))
			return true;
		return false;
	}

	node = &cm_q3_nodes[nodeNum];
	if (CM_HeadnodeVisible(node->children[0], visBits))
		return true;
	return CM_HeadnodeVisible(node->children[1], visBits);
}
