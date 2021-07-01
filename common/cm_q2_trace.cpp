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
// cm_q2_main.c
// Quake2 BSP map model tracing
//

#include "cm_q2_local.h"

static int				cm_q2_checkCount;
static int				cm_q2_floodValid;

static plane_t			*cm_q2_boxPlanes;
static int				cm_q2_boxHeadNode;
static cmQ2BspBrush_t	*cm_q2_boxBrush;
static cmQ2BspLeaf_t	*cm_q2_boxLeaf;

static int				cm_q2_leafCount, cm_q2_leafMaxCount;
static int				*cm_q2_leafList;
static float			*cm_q2_leafMins, *cm_q2_leafMaxs;
static int				cm_q2_leafTopNode;

// 1/32 epsilon to keep floating point happy
#define DIST_EPSILON	(0.03125f)

static cmTrace_t			cm_q2_currentTrace;
static vec3_t			cm_q2_traceStart, cm_q2_traceEnd;
static vec3_t			cm_q2_traceMins, cm_q2_traceMaxs;
static vec3_t			cm_q2_traceExtents;
static int				cm_q2_traceContents;
static bool				cm_q2_traceIsPoint;		// optimized case

/*
=============================================================================

	TRACING

=============================================================================
*/

/*
==================
CM_Q2BSP_LeafArea
==================
*/
int CM_Q2BSP_LeafArea (int leafNum)
{
	if (leafNum < 0 || leafNum >= cm_q2_numLeafs)
		Com_Error (ERR_DROP, "CM_Q2BSP_LeafArea: bad number");

	return cm_q2_leafs[leafNum].area;
}


/*
==================
CM_Q2BSP_LeafCluster
==================
*/
int CM_Q2BSP_LeafCluster (int leafNum)
{
	if (leafNum < 0 || leafNum >= cm_q2_numLeafs)
		Com_Error (ERR_DROP, "CM_Q2BSP_LeafCluster: bad number");

	return cm_q2_leafs[leafNum].cluster;
}


/*
==================
CM_Q2BSP_LeafContents
==================
*/
int CM_Q2BSP_LeafContents (int leafNum)
{
	if (leafNum < 0 || leafNum >= cm_q2_numLeafs)
		Com_Error (ERR_DROP, "CM_Q2BSP_LeafContents: bad number");

	return cm_q2_leafs[leafNum].contents;
}

// ==========================================================================

/*
===================
CM_Q2BSP_InitBoxHull

Set up the planes and nodes so that the six floats of a bounding box
can just be stored out and get a proper clipping hull structure.
===================
*/
void CM_Q2BSP_InitBoxHull ()
{
	int					i;
	int					side;
	cmQ2BspNode_t		*c;
	plane_t				*p;
	cmQ2BspBrushSide_t	*s;

	cm_q2_boxHeadNode = cm_q2_numNodes;
	cm_q2_boxPlanes = &cm_q2_planes[cm_q2_numPlanes];
	if (cm_q2_numNodes+6 > Q2BSP_MAX_NODES
	|| cm_q2_numBrushes+1 > Q2BSP_MAX_BRUSHES
	|| cm_q2_numLeafBrushes+1 > Q2BSP_MAX_LEAFBRUSHES
	|| cm_q2_numBrushSides+6 > Q2BSP_MAX_BRUSHSIDES
	|| cm_q2_numPlanes+12 > Q2BSP_MAX_PLANES)
		Com_Error (ERR_DROP, "CM_Q2BSP_InitBoxHull: Not enough room for box tree");

	cm_q2_boxBrush = &cm_q2_brushes[cm_q2_numBrushes];
	cm_q2_boxBrush->numSides = 6;
	cm_q2_boxBrush->firstBrushSide = cm_q2_numBrushSides;
	cm_q2_boxBrush->contents = CONTENTS_MONSTER;

	cm_q2_boxLeaf = &cm_q2_leafs[cm_q2_numLeafs];
	cm_q2_boxLeaf->contents = CONTENTS_MONSTER;
	cm_q2_boxLeaf->firstLeafBrush = cm_q2_numLeafBrushes;
	cm_q2_boxLeaf->numLeafBrushes = 1;

	cm_q2_leafBrushes[cm_q2_numLeafBrushes] = cm_q2_numBrushes;

	for (i=0 ; i<6 ; i++) {
		side = i&1;

		// Brush sides
		s = &cm_q2_brushSides[cm_q2_numBrushSides+i];
		s->plane = cm_q2_planes + (cm_q2_numPlanes+i*2+side);
		s->surface = &cm_q2_nullSurface;

		// Nodes
		c = &cm_q2_nodes[cm_q2_boxHeadNode+i];
		c->plane = cm_q2_planes + (cm_q2_numPlanes+i*2);
		c->children[side] = -1 - cm_q2_emptyLeaf;
		if (i != 5)
			c->children[side^1] = cm_q2_boxHeadNode+i + 1;
		else
			c->children[side^1] = -1 - cm_q2_numLeafs;

		// Planes
		p = &cm_q2_boxPlanes[i*2];
		p->type = i>>1;
		p->signBits = 0;
		Vec3Clear (p->normal);
		p->normal[i>>1] = 1;

		p = &cm_q2_boxPlanes[i*2+1];
		p->type = 3 + (i>>1);
		p->signBits = 0;
		Vec3Clear (p->normal);
		p->normal[i>>1] = -1;
	}	
}


/*
===================
CM_Q2BSP_HeadnodeForBox

To keep everything totally uniform, bounding boxes are turned into small
BSP trees instead of being compared directly.
===================
*/
int	CM_Q2BSP_HeadnodeForBox (vec3_t mins, vec3_t maxs)
{
	cm_q2_boxPlanes[0].dist = maxs[0];
	cm_q2_boxPlanes[1].dist = -maxs[0];
	cm_q2_boxPlanes[2].dist = mins[0];
	cm_q2_boxPlanes[3].dist = -mins[0];
	cm_q2_boxPlanes[4].dist = maxs[1];
	cm_q2_boxPlanes[5].dist = -maxs[1];
	cm_q2_boxPlanes[6].dist = mins[1];
	cm_q2_boxPlanes[7].dist = -mins[1];
	cm_q2_boxPlanes[8].dist = maxs[2];
	cm_q2_boxPlanes[9].dist = -maxs[2];
	cm_q2_boxPlanes[10].dist = mins[2];
	cm_q2_boxPlanes[11].dist = -mins[2];

	return cm_q2_boxHeadNode;
}


/*
==================
CM_Q2BSP_PointLeafnum_r
==================
*/
static int CM_Q2BSP_PointLeafnum_r (vec3_t p, int num)
{
	float			d;
	cmQ2BspNode_t	*node;
	plane_t			*plane;

	while (num >= 0) {
		node = cm_q2_nodes + num;
		plane = node->plane;

		d = PlaneDiff (p, plane);
		if (d < 0)
			num = node->children[1];
		else
			num = node->children[0];
	}

	cm_numPointContents++;	// Optimize counter

	return -1 - num;
}


/*
==================
CM_Q2BSP_PointLeafnum
==================
*/
int CM_Q2BSP_PointLeafnum (vec3_t p)
{
	if (!cm_q2_numPlanes)
		return 0;	// Sound may call this without map loaded
	return CM_Q2BSP_PointLeafnum_r (p, 0);
}


/*
=============
CM_Q2BSP_BoxLeafnums

Fills in a list of all the leafs touched
=============
*/
static void CM_Q2BSP_BoxLeafnums_r (int nodeNum)
{
	plane_t			*plane;
	cmQ2BspNode_t	*node;
	int				s;

	for ( ; ; ) {
		if (nodeNum < 0) {
			if (cm_q2_leafCount >= cm_q2_leafMaxCount)
				return;

			cm_q2_leafList[cm_q2_leafCount++] = -1 - nodeNum;
			return;
		}
	
		node = &cm_q2_nodes[nodeNum];
		plane = node->plane;
		s = BOX_ON_PLANE_SIDE (cm_q2_leafMins, cm_q2_leafMaxs, plane);
		if (s == 1)
			nodeNum = node->children[0];
		else if (s == 2)
			nodeNum = node->children[1];
		else {
			// Go down both
			if (cm_q2_leafTopNode == -1)
				cm_q2_leafTopNode = nodeNum;
			CM_Q2BSP_BoxLeafnums_r (node->children[0]);
			nodeNum = node->children[1];
		}
	}
}


/*
==================
CM_Q2BSP_BoxLeafnumsHeadNode
==================
*/
static int CM_Q2BSP_BoxLeafnumsHeadNode (vec3_t mins, vec3_t maxs, int *list, int listSize, int headNode, int *topNode)
{
	cm_q2_leafList = list;
	cm_q2_leafCount = 0;
	cm_q2_leafMaxCount = listSize;
	cm_q2_leafMins = mins;
	cm_q2_leafMaxs = maxs;
	cm_q2_leafTopNode = -1;

	CM_Q2BSP_BoxLeafnums_r (headNode);

	if (topNode)
		*topNode = cm_q2_leafTopNode;

	return cm_q2_leafCount;
}


/*
==================
CM_Q2BSP_BoxLeafnums
==================
*/
int	CM_Q2BSP_BoxLeafnums (vec3_t mins, vec3_t maxs, int *list, int listSize, int *topNode)
{
	return CM_Q2BSP_BoxLeafnumsHeadNode (mins, maxs, list, listSize, cm_mapCModels[0].headNode, topNode);
}


/*
==================
CM_Q2BSP_PointContents
==================
*/
int CM_Q2BSP_PointContents (vec3_t p, int headNode)
{
	int		l;

	if (!cm_q2_numNodes)	// Map not loaded
		return 0;

	l = CM_Q2BSP_PointLeafnum_r (p, headNode);

	return cm_q2_leafs[l].contents;
}


/*
==================
CM_Q2BSP_TransformedPointContents

Handles offseting and rotation of the end points for moving and rotating entities
==================
*/
int	CM_Q2BSP_TransformedPointContents (vec3_t p, int headNode, vec3_t origin, vec3_t angles)
{
	vec3_t	dist;
	vec3_t	temp;
	vec3_t	forward, right, up;
	int		l;

	// Subtract origin offset
	Vec3Subtract (p, origin, dist);

	// Rotate start and end into the models frame of reference
	if (headNode != cm_q2_boxHeadNode && (angles[0] || angles[1] || angles[2])) {
		Angles_Vectors (angles, forward, right, up);

		Vec3Copy (dist, temp);
		dist[0] = DotProduct (temp, forward);
		dist[1] = -DotProduct (temp, right);
		dist[2] = DotProduct (temp, up);
	}

	l = CM_Q2BSP_PointLeafnum_r (dist, headNode);

	return cm_q2_leafs[l].contents;
}

/*
=============================================================================

	BOX TRACING

=============================================================================
*/

/*
================
CM_Q2BSP_ClipBoxToBrush
================
*/
static void CM_Q2BSP_ClipBoxToBrush (cmQ2BspBrush_t *brush)
{
	int					i, j;
	plane_t				*p, *clipPlane;
	float				dist, dot1, dot2, f;
	float				enterFrac, leaveFrac;
	vec3_t				ofs;
	bool				getOut, startOut;
	cmQ2BspBrushSide_t	*side, *leadSide;

	enterFrac = -1;
	leaveFrac = 1;
	clipPlane = NULL;

	if (!brush->numSides)
		return;

	cm_numBrushTraces++;

	getOut = false;
	startOut = false;
	leadSide = NULL;

	for (i=0, side=&cm_q2_brushSides[brush->firstBrushSide] ; i<brush->numSides ; side++, i++) 	{
		p = side->plane;

		// FIXME: special case for axial
		if (!cm_q2_traceIsPoint) {
			// general box case
			// push the plane out apropriately for mins/maxs
			// FIXME: use signBits into 8 way lookup for each mins/maxs
			for (j=0 ; j<3 ; j++) {
				if (p->normal[j] < 0)
					ofs[j] = cm_q2_traceMaxs[j];
				else
					ofs[j] = cm_q2_traceMins[j];
			}
			dist = DotProduct (ofs, p->normal);
			dist = p->dist - dist;
		}
		else {
			// Special point case
			dist = p->dist;
		}

		dot1 = DotProduct (cm_q2_traceStart, p->normal) - dist;
		dot2 = DotProduct (cm_q2_traceEnd, p->normal) - dist;

		if (dot2 > 0)
			getOut = true;	// Endpoint is not in solid
		if (dot1 > 0)
			startOut = true;

		// If completely in front of face, no intersection
		if (dot1 > 0 && dot2 >= dot1)
			return;

		if (dot1 <= 0 && dot2 <= 0)
			continue;

		// Crosses face
		f = dot1 - dot2;
		if (f > 0) {
			// Enter
			f = (dot1 - DIST_EPSILON) / f;
			if (f > enterFrac) {
				enterFrac = f;
				clipPlane = p;
				leadSide = side;
			}
		}
		else {
			// Leave
			f = (dot1 + DIST_EPSILON) / f;
			if (f < leaveFrac)
				leaveFrac = f;
		}
	}

	if (!startOut) {
		// Original point was inside brush
		cm_q2_currentTrace.startSolid = true;
		if (!getOut)
			cm_q2_currentTrace.allSolid = true;
		return;
	}

	if (enterFrac < leaveFrac && enterFrac > -1 && enterFrac < cm_q2_currentTrace.fraction) {
		if (enterFrac < 0)
			enterFrac = 0;

		cm_q2_currentTrace.fraction = enterFrac;
		cm_q2_currentTrace.plane = *clipPlane;
		cm_q2_currentTrace.surface = &(leadSide->surface->c);
		cm_q2_currentTrace.contents = brush->contents;
	}
}


/*
================
CM_Q2BSP_ClipBoxes
================
*/
static void CM_Q2BSP_ClipBoxes (int leafNum)
{
	cmQ2BspLeaf_t	*leaf;
	cmQ2BspBrush_t	*brush;
	int				brushNum;
	int				k;

	leaf = &cm_q2_leafs[leafNum];
	if (!(leaf->contents & cm_q2_traceContents))
		return;

	// Trace line against all brushes in the leaf
	for (k=0 ; k<leaf->numLeafBrushes ; k++) {
		brushNum = cm_q2_leafBrushes[leaf->firstLeafBrush+k];
		brush = &cm_q2_brushes[brushNum];

		if (brush->checkCount == cm_q2_checkCount)
			continue;	// Already checked this brush in another leaf
		brush->checkCount = cm_q2_checkCount;
		if (!(brush->contents & cm_q2_traceContents))
			continue;

		CM_Q2BSP_ClipBoxToBrush (brush);
		if (!cm_q2_currentTrace.fraction)
			return;
	}
}


/*
================
CM_Q2BSP_TestBoxInBrush
================
*/
static void CM_Q2BSP_TestBoxInBrush (cmQ2BspBrush_t *brush)
{
	int					i, j;
	vec3_t				ofs;
	plane_t				*p;
	cmQ2BspBrushSide_t	*side;
	float				dist, dot;

	if (!brush->numSides)
		return;

	for (i=0, side=&cm_q2_brushSides[brush->firstBrushSide] ; i<brush->numSides ; side++, i++) {
		p = side->plane;

		// FIXME: special case for axial
		// general box case
		// push the plane out apropriately for mins/maxs
		// FIXME: use signBits into 8 way lookup for each mins/maxs
		for (j=0 ; j<3 ; j++) {
			if (p->normal[j] < 0)
				ofs[j] = cm_q2_traceMaxs[j];
			else
				ofs[j] = cm_q2_traceMins[j];
		}

		dist = p->dist - DotProduct (ofs, p->normal);
		dot = DotProduct (cm_q2_traceStart, p->normal) - dist;

		// If completely in front of face, no intersection
		if (dot > 0)
			return;
	}

	// Inside this brush
	cm_q2_currentTrace.startSolid = cm_q2_currentTrace.allSolid = true;
	cm_q2_currentTrace.fraction = 0;
	cm_q2_currentTrace.contents = brush->contents;
}


/*
================
CM_Q2BSP_TestBoxes
================
*/
static void CM_Q2BSP_TestBoxes (int leafNum)
{
	cmQ2BspLeaf_t	*leaf;
	cmQ2BspBrush_t	*brush;
	int				brushNum;
	int				k;

	leaf = &cm_q2_leafs[leafNum];
	if (!(leaf->contents & cm_q2_traceContents))
		return;

	// Trace line against all brushes in the leaf
	for (k=0 ; k<leaf->numLeafBrushes ; k++) {
		brushNum = cm_q2_leafBrushes[leaf->firstLeafBrush+k];
		brush = &cm_q2_brushes[brushNum];

		if (brush->checkCount == cm_q2_checkCount)
			continue;	// Already checked this brush in another leaf
		brush->checkCount = cm_q2_checkCount;
		if (!(brush->contents & cm_q2_traceContents))
			continue;

		CM_Q2BSP_TestBoxInBrush (brush);
		if (!cm_q2_currentTrace.fraction)
			return;
	}
}


/*
==================
CM_Q2BSP_RecursiveHullCheck
==================
*/
static void CM_Q2BSP_RecursiveHullCheck (int num, float p1f, float p2f, vec3_t p1, vec3_t p2)
{
	cmQ2BspNode_t	*node;
	plane_t			*plane;
	float			t1, t2, offset;
	float			frac, frac2;
	float			idist;
	int				i, side;
	vec3_t			mid;
	float			midf;

	if (cm_q2_currentTrace.fraction <= p1f)
		return;		// already hit something nearer

	// if < 0, we are in a leaf node
	if (num < 0) {
		CM_Q2BSP_ClipBoxes (-1-num);
		return;
	}

	/*
	** find the point distances to the seperating plane
	** and the offset for the size of the box
	*/
	node = cm_q2_nodes + num;
	plane = node->plane;

	if (plane->type < 3) {
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
		offset = cm_q2_traceExtents[plane->type];
	}
	else {
		t1 = DotProduct (plane->normal, p1) - plane->dist;
		t2 = DotProduct (plane->normal, p2) - plane->dist;
		if (cm_q2_traceIsPoint)
			offset = 0;
		else
			offset = fabs (cm_q2_traceExtents[0]*plane->normal[0])
				+ fabs (cm_q2_traceExtents[1]*plane->normal[1])
				+ fabs (cm_q2_traceExtents[2]*plane->normal[2]);
	}

	// see which sides we need to consider
	if (t1 >= offset && t2 >= offset) {
		CM_Q2BSP_RecursiveHullCheck (node->children[0], p1f, p2f, p1, p2);
		return;
	}
	if (t1 < -offset && t2 < -offset) {
		CM_Q2BSP_RecursiveHullCheck (node->children[1], p1f, p2f, p1, p2);
		return;
	}

	// put the crosspoint DIST_EPSILON pixels on the near side
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

	// move up to the node
	frac = clamp (frac, 0, 1);
	midf = p1f + (p2f - p1f ) * frac;
	for (i=0 ; i<3 ; i++)
		mid[i] = p1[i] + frac * (p2[i] - p1[i]);

	CM_Q2BSP_RecursiveHullCheck (node->children[side], p1f, midf, p1, mid);

	// go past the node
	frac2 = clamp (frac2, 0, 1);
	midf = p1f + (p2f - p1f) * frac2;
	for (i=0 ; i<3 ; i++)
		mid[i] = p1[i] + frac2 * (p2[i] - p1[i]);

	CM_Q2BSP_RecursiveHullCheck (node->children[side^1], midf, p2f, mid, p2);
}

// ==========================================================================

/*
====================
CM_Q2BSP_Trace
====================
*/
cmTrace_t CM_Q2BSP_Trace (vec3_t start, vec3_t end, float size, int contentMask)
{
	vec3_t maxs, mins;

	Vec3Set (maxs, size, size, size);
	Vec3Set (mins, -size, -size, -size);

	return CM_Q2BSP_BoxTrace (start, end, mins, maxs, 0, contentMask);
}


/*
==================
CM_Q2BSP_BoxTrace
==================
*/
cmTrace_t CM_Q2BSP_BoxTrace (vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, int headNode, int brushMask)
{
	cm_q2_checkCount++;	// For multi-check avoidance
	cm_numTraces++;		// For statistics, may be zeroed

	// Fill in a default trace
	cm_q2_currentTrace.allSolid = false;
	cm_q2_currentTrace.contents = 0;
	Vec3Clear (cm_q2_currentTrace.endPos);
	cm_q2_currentTrace.ent = NULL;
	cm_q2_currentTrace.fraction = 1;
	cm_q2_currentTrace.plane.dist = 0;
	Vec3Clear (cm_q2_currentTrace.plane.normal);
	cm_q2_currentTrace.plane.signBits = 0;
	cm_q2_currentTrace.plane.type = 0;
	cm_q2_currentTrace.startSolid = false;
	cm_q2_currentTrace.surface = &(cm_q2_nullSurface.c);

	if (!cm_q2_numNodes)	// Map not loaded
		return cm_q2_currentTrace;

	cm_q2_traceContents = brushMask;
	Vec3Copy (start, cm_q2_traceStart);
	Vec3Copy (end, cm_q2_traceEnd);
	Vec3Copy (mins, cm_q2_traceMins);
	Vec3Copy (maxs, cm_q2_traceMaxs);

	// Check for position test special case
	if (Vec3Compare (start, end)) {
		int		leafs[1024];
		int		i, numLeafs;
		vec3_t	c1, c2;
		int		topNode;

		Vec3Add (start, mins, c1);
		Vec3Add (start, maxs, c2);
		for (i=0 ; i<3 ; i++) {
			c1[i] -= 1;
			c2[i] += 1;
		}

		numLeafs = CM_Q2BSP_BoxLeafnumsHeadNode (c1, c2, leafs, 1024, headNode, &topNode);
		for (i=0 ; i<numLeafs ; i++) {
			CM_Q2BSP_TestBoxes (leafs[i]);
			if (cm_q2_currentTrace.allSolid)
				break;
		}
		Vec3Copy (start, cm_q2_currentTrace.endPos);
		return cm_q2_currentTrace;
	}

	// Check for point special case
	if (Vec3Compare (mins, vec3Origin) && Vec3Compare (maxs, vec3Origin)) {
		cm_q2_traceIsPoint = true;
		Vec3Clear (cm_q2_traceExtents);
	}
	else {
		cm_q2_traceIsPoint = false;
		cm_q2_traceExtents[0] = -mins[0] > maxs[0] ? -mins[0] : maxs[0];
		cm_q2_traceExtents[1] = -mins[1] > maxs[1] ? -mins[1] : maxs[1];
		cm_q2_traceExtents[2] = -mins[2] > maxs[2] ? -mins[2] : maxs[2];
	}

	// General sweeping through world
	CM_Q2BSP_RecursiveHullCheck (headNode, 0, 1, start, end);

	if (cm_q2_currentTrace.fraction == 1) {
		Vec3Copy (end, cm_q2_currentTrace.endPos);
	}
	else {
		cm_q2_currentTrace.endPos[0] = start[0] + cm_q2_currentTrace.fraction * (end[0] - start[0]);
		cm_q2_currentTrace.endPos[1] = start[1] + cm_q2_currentTrace.fraction * (end[1] - start[1]);
		cm_q2_currentTrace.endPos[2] = start[2] + cm_q2_currentTrace.fraction * (end[2] - start[2]);
	}

	return cm_q2_currentTrace;
}


/*
==================
CM_Q2BSP_TransformedBoxTrace

Handles offseting and rotation of the end points for moving and rotating entities
==================
*/
#ifdef WIN32
#pragma optimize ("", off)
#endif
void CM_Q2BSP_TransformedBoxTrace (cmTrace_t *out, vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, int headNode, int brushMask, vec3_t origin, vec3_t angles)
{
	vec3_t		start_l, end_l;
	vec3_t		forward, right, up;
	vec3_t		temp, a;
	bool		rotated;

	// Subtract origin offset
	Vec3Subtract (start, origin, start_l);
	Vec3Subtract (end, origin, end_l);

	// Rotate start and end into the models frame of reference
	if (headNode != cm_q2_boxHeadNode && (angles[0] || angles[1] || angles[2]))
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
	*out = CM_Q2BSP_BoxTrace (start_l, end_l, mins, maxs, headNode, brushMask);

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
#pragma optimize ("", on)
#endif

/*
=============================================================================

	PVS / PHS

=============================================================================
*/

/*
===================
CM_Q2BSP_DecompressVis
===================
*/
static void CM_Q2BSP_DecompressVis (byte *in, byte *out)
{
	int		c;
	byte	*outPtr;
	int		row;

	row = (cm_q2_numClusters + 7) >> 3;	
	outPtr = out;

	if (!in || !cm_q2_numVisibility) {
		// No vis info, so make all visible
		while (row) {
			*outPtr++ = 0xff;
			row--;
		}
		return;		
	}

	do {
		if (*in) {
			*outPtr++ = *in++;
			continue;
		}
	
		c = in[1];
		in += 2;
		if ((outPtr - out) + c > row) {
			c = row - (outPtr - out);
			Com_DevPrintf (PRNT_WARNING, "Warning: Vis decompression overrun\n");
		}

		while (c) {
			*outPtr++ = 0;
			c--;
		}
	} while (outPtr - out < row);
}


/*
===================
CM_Q2BSP_ClusterPVS
===================
*/
byte *CM_Q2BSP_ClusterPVS (int cluster)
{
	static byte		pvsRow[Q2BSP_MAX_VIS];

	if (cluster == -1 || !cm_q2_visData)
		memset (pvsRow, 0, (cm_q2_numClusters + 7) >> 3);
	else
		CM_Q2BSP_DecompressVis ((byte *)cm_q2_visData + cm_q2_visData->bitOfs[cluster][Q2BSP_VIS_PVS], pvsRow);
	return pvsRow;
}


/*
===================
CM_Q2BSP_ClusterPHS
===================
*/
byte *CM_Q2BSP_ClusterPHS (int cluster)
{
	static byte		phsRow[Q2BSP_MAX_VIS];

	if (cluster == -1 || !cm_q2_visData)
		memset (phsRow, 0, (cm_q2_numClusters + 7) >> 3);
	else
		CM_Q2BSP_DecompressVis ((byte *)cm_q2_visData + cm_q2_visData->bitOfs[cluster][Q2BSP_VIS_PHS], phsRow);
	return phsRow;
}

/*
=============================================================================

	AREAPORTALS

=============================================================================
*/

/*
====================
CM_Q2BSP_FloodAreaConnections
====================
*/
static void FloodArea_r (cmQ2BspArea_t *area, int floodNum)
{
	dQ2BspAreaPortal_t	*p;
	int					i;

	if (area->floodValid == cm_q2_floodValid) {
		if (area->floodNum == floodNum)
			return;
		Com_Error (ERR_DROP, "FloodArea_r: reflooded");
	}

	area->floodNum = floodNum;
	area->floodValid = cm_q2_floodValid;
	p = &cm_q2_areaPortals[area->firstAreaPortal];
	for (i=0 ; i<area->numAreaPortals ; i++, p++) {
		if (cm_q2_portalOpen[p->portalNum])
			FloodArea_r (&cm_q2_areas[p->otherArea], floodNum);
	}
}
void CM_Q2BSP_FloodAreaConnections ()
{
	cmQ2BspArea_t	*area;
	int				floodNum;
	int				i;

	// All current floods are now invalid
	cm_q2_floodValid++;
	floodNum = 0;

	// Area 0 is not used
	for (i=1 ; i<cm_q2_numAreas ; i++) {
		area = &cm_q2_areas[i];
		if (area->floodValid == cm_q2_floodValid)
			continue;		// already flooded into
		floodNum++;
		FloodArea_r (area, floodNum);
	}
}


/*
====================
CM_Q2BSP_SetAreaPortalState
====================
*/
void CM_Q2BSP_SetAreaPortalState (int portalNum, bool open)
{
	if (portalNum > cm_q2_numAreaPortals)
		Com_Error (ERR_DROP, "CM_SetAreaPortalState: areaportal > numAreaPortals");

	cm_q2_portalOpen[portalNum] = open;
	CM_Q2BSP_FloodAreaConnections ();
}


/*
====================
CM_Q2BSP_AreasConnected
====================
*/
bool CM_Q2BSP_AreasConnected (int area1, int area2)
{
	if (cm_noAreas->intVal)
		return true;

	if (area1 > cm_q2_numAreas || area2 > cm_q2_numAreas)
		Com_Error (ERR_DROP, "CM_AreasConnected: area > cm_q2_numAreas");

	if (cm_q2_areas[area1].floodNum == cm_q2_areas[area2].floodNum)
		return true;
	return false;
}


/*
=================
CM_Q2BSP_WriteAreaBits

Writes a length byte followed by a bit vector of all the areas
that area in the same flood as the area parameter

This is used by the client refreshes to cull visibility
=================
*/
int CM_Q2BSP_WriteAreaBits (byte *buffer, int area)
{
	int		i;
	int		floodNum;
	int		bytes;

	bytes = (cm_q2_numAreas+7)>>3;

	if (cm_noAreas->intVal) {
		// For debugging, send everything
		memset (buffer, 255, bytes);
	}
	else {
		memset (buffer, 0, bytes);

		floodNum = cm_q2_areas[area].floodNum;
		for (i=0 ; i<cm_q2_numAreas ; i++) {
			if (cm_q2_areas[i].floodNum == floodNum || !area)
				buffer[i>>3] |= BIT(i&7);
		}
	}

	return bytes;
}


/*
===================
CM_Q2BSP_WritePortalState

Writes the portal state to a savegame file
===================
*/
void CM_Q2BSP_WritePortalState (fileHandle_t fileNum)
{
	FS_Write (cm_q2_portalOpen, sizeof(bool) * Q2BSP_MAX_AREAPORTALS, fileNum);
}


/*
===================
CM_Q2BSP_ReadPortalState

Reads the portal state from a savegame file and recalculates the area connections
===================
*/
void CM_Q2BSP_ReadPortalState (fileHandle_t fileNum)
{
	FS_Read (cm_q2_portalOpen, sizeof(bool) * Q2BSP_MAX_AREAPORTALS, fileNum);
	CM_Q2BSP_FloodAreaConnections ();
}


/*
=============
CM_Q2BSP_HeadnodeVisible

Returns true if any leaf under headnode has a cluster that is potentially visible
=============
*/
bool CM_Q2BSP_HeadnodeVisible (int nodeNum, byte *visBits)
{
	cmQ2BspNode_t	*node;
	int				leafNum;
	int				cluster;

	if (nodeNum < 0) {
		leafNum = -1-nodeNum;
		cluster = cm_q2_leafs[leafNum].cluster;
		if (cluster == -1)
			return false;
		if (visBits[cluster>>3] & (BIT(cluster&7)))
			return true;
		return false;
	}

	node = &cm_q2_nodes[nodeNum];
	if (CM_Q2BSP_HeadnodeVisible (node->children[0], visBits))
		return true;
	return CM_Q2BSP_HeadnodeVisible (node->children[1], visBits);
}
