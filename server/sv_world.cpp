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
// sv_world.c
// World query functions
//

#include "sv_local.h"

// FIXME: this use of "area" is different from the bsp file use
// FIXME: remove this mess!
inline edict_t *EDICT_FROM_AREA(link_t *l)
{
	return ((edict_t *)((byte *)l - (int)&(((edict_t *)0)->area)));
}

struct areaNode_t {
	int			axis;		// -1 = leaf node
	float		dist;
	areaNode_t	*children[2];
	link_t		trigger_edicts;
	link_t		solid_edicts;
};

#define AREA_DEPTH	4
#define AREA_NODES	32

static areaNode_t	sv_areaNodes[AREA_NODES];
static int			sv_numAreaNodes;

static float	*sv_areaMins, *sv_areaMaxs;
static int		sv_areaType;

// ClearLink is used for new headNodes
static void ClearLink (link_t *l)
{
	l->prev = l->next = l;
}
static void RemoveLink (link_t *l)
{
	l->next->prev = l->prev;
	l->prev->next = l->next;
}
static void InsertLinkBefore (link_t *l, link_t *before)
{
	l->next = before;
	l->prev = before->prev;
	l->prev->next = l;
	l->next->prev = l;
}

// ============================================================================

struct moveClip_t {
	vec3_t		boxMins, boxMaxs;	// enclose the test object along entire move
	float		*mins, *maxs;		// size of the moving object
	vec3_t		mins2, maxs2;		// size when clipping against mosnters
	float		*start, *end;
	cmTrace_t	trace;
	edict_t		*passEdict;
	int			contentMask;
};

/*
===============================================================================

	ENTITY AREA CHECKING

===============================================================================
*/

/*
===============
SV_CreateAreaNode

Builds a uniformly subdivided tree for the given world size
===============
*/
static areaNode_t *SV_CreateAreaNode (int depth, vec3_t mins, vec3_t maxs)
{
	areaNode_t	*anode;
	vec3_t		size;
	vec3_t		mins1, maxs1, mins2, maxs2;

	anode = &sv_areaNodes[sv_numAreaNodes];
	sv_numAreaNodes++;

	ClearLink (&anode->trigger_edicts);
	ClearLink (&anode->solid_edicts);
	
	if (depth == AREA_DEPTH) {
		anode->axis = -1;
		anode->children[0] = anode->children[1] = NULL;
		return anode;
	}
	
	Vec3Subtract (maxs, mins, size);
	if (size[0] > size[1])
		anode->axis = 0;
	else
		anode->axis = 1;
	
	anode->dist = 0.5 * (maxs[anode->axis] + mins[anode->axis]);
	Vec3Copy (mins, mins1);	
	Vec3Copy (mins, mins2);	
	Vec3Copy (maxs, maxs1);	
	Vec3Copy (maxs, maxs2);	
	
	maxs1[anode->axis] = mins2[anode->axis] = anode->dist;
	
	anode->children[0] = SV_CreateAreaNode (depth+1, mins2, maxs2);
	anode->children[1] = SV_CreateAreaNode (depth+1, mins1, maxs1);

	return anode;
}


/*
===============
SV_ClearWorld
===============
*/
void SV_ClearWorld ()
{
	vec3_t	mins, maxs;

	memset (sv_areaNodes, 0, sizeof(sv_areaNodes));
	sv_numAreaNodes = 0;

	CM_InlineModelBounds (sv.models[1], mins, maxs);
	SV_CreateAreaNode (0, mins, maxs);
}


/*
===============
SV_UnlinkEdict
===============
*/
void SV_UnlinkEdict (edict_t *ent)
{
	if (!ent->area.prev)
		return;		// not linked in anywhere

	RemoveLink (&ent->area);
	ent->area.prev = ent->area.next = NULL;
}


/*
===============
SV_LinkEdict
===============
*/
#define MAX_TOTAL_ENT_LEAFS		128
void SV_LinkEdict (edict_t *ent)
{
	areaNode_t	*node;
	int			leafs[MAX_TOTAL_ENT_LEAFS];
	int			clusters[MAX_TOTAL_ENT_LEAFS];
	int			num_leafs;
	int			i, j, k;
	int			area;
	int			topnode;

	if (ent->area.prev)
		SV_UnlinkEdict (ent);	// unlink from old position

	if (ent == ge->edicts)
		return;		// don't add the world

	if (!ent->inUse)
		return;

	// Set the size
	Vec3Subtract (ent->maxs, ent->mins, ent->size);
	
	// Encode the size into the entity_state for client prediction
	if (ent->solid == SOLID_BBOX && !(ent->svFlags & SVF_DEADMONSTER)) {
		// Assume that x/y are equal and symetric
		i = ent->maxs[0]/8;
		if (i<1)
			i = 1;
		if (i>31)
			i = 31;

		// z is not symetric
		j = (-ent->mins[2])/8;
		if (j<1)
			j = 1;
		if (j>31)
			j = 31;

		// and z maxs can be negative...
		k = (ent->maxs[2]+32)/8;
		if (k<1)
			k = 1;
		if (k>63)
			k = 63;

		ent->s.solid = (k<<10) | (j<<5) | i;
	}
	else if (ent->solid == SOLID_BSP) {
		ent->s.solid = 31;		// A solid_bbox will never create this value
	}
	else
		ent->s.solid = 0;

	// Set the abs box
	if (ent->solid == SOLID_BSP && (ent->s.angles[0] || ent->s.angles[1] || ent->s.angles[2])) {
		// Expand for rotation
		float		rMax, v;
		int			j;

		rMax = 0;
		for (j=0 ; j<3 ; j++) {
			v = fabs (ent->mins[j]);
			if (v > rMax)
				rMax = v;
			v = fabs (ent->maxs[j]);
			if (v > rMax)
				rMax = v;
		}

		ent->absMin[0] = ent->s.origin[0] - rMax;
		ent->absMin[1] = ent->s.origin[1] - rMax;
		ent->absMin[2] = ent->s.origin[2] - rMax;

		ent->absMax[0] = ent->s.origin[0] + rMax;
		ent->absMax[1] = ent->s.origin[1] + rMax;
		ent->absMax[2] = ent->s.origin[2] + rMax;
	}
	else {
		// Normal
		Vec3Add (ent->s.origin, ent->mins, ent->absMin);	
		Vec3Add (ent->s.origin, ent->maxs, ent->absMax);
	}

	/*
	** Because movement is clipped an epsilon away from an actual edge,
	** we must fully check even when bounding boxes don't quite touch
	*/
	ent->absMin[0] -= 1;
	ent->absMin[1] -= 1;
	ent->absMin[2] -= 1;
	ent->absMax[0] += 1;
	ent->absMax[1] += 1;
	ent->absMax[2] += 1;

	// Link to PVS leafs
	ent->numClusters = 0;
	ent->areaNum = 0;
	ent->areaNum2 = 0;

	// Get all leafs, including solids
	num_leafs = CM_BoxLeafnums (ent->absMin, ent->absMax, leafs, MAX_TOTAL_ENT_LEAFS, &topnode);

	// Set areas
	for (i=0 ; i<num_leafs ; i++) {
		clusters[i] = CM_LeafCluster (leafs[i]);
		area = CM_LeafArea (leafs[i]);
		if (area) {
			/*
			** Doors may legally straggle two areas,
			** but nothing should evern need more than that
			*/
			if (ent->areaNum && ent->areaNum != area) {
				if (ent->areaNum2 && ent->areaNum2 != area && Com_ServerState () == SS_LOADING)
					Com_DevPrintf (0, "Object touching 3 areas at %f %f %f\n",
					ent->absMin[0], ent->absMin[1], ent->absMin[2]);
				ent->areaNum2 = area;
			}
			else
				ent->areaNum = area;
		}
	}

	if (num_leafs >= MAX_TOTAL_ENT_LEAFS) {
		// Assume we missed some leafs, and mark by headNode
		ent->numClusters = -1;
		ent->headNode = topnode;
	}
	else {
		ent->numClusters = 0;
		for (i=0 ; i<num_leafs ; i++) {
			if (clusters[i] == -1)
				continue;		// Not a visible leaf
			for (j=0 ; j<i ; j++)
				if (clusters[j] == clusters[i])
					break;
			if (j == i) {
				if (ent->numClusters == MAX_ENT_CLUSTERS) {
					// Assume we missed some leafs, and mark by headNode
					ent->numClusters = -1;
					ent->headNode = topnode;
					break;
				}

				ent->clusterNums[ent->numClusters++] = clusters[i];
			}
		}
	}

	// If first time, make sure oldOrigin is valid
	if (!ent->linkCount)
		Vec3Copy (ent->s.origin, ent->s.oldOrigin);

	ent->linkCount++;

	if (ent->solid == SOLID_NOT)
		return;

	// Find the first node that the ent's box crosses
	node = sv_areaNodes;
	for ( ; ; ) {
		if (node->axis == -1)
			break;
		if (ent->absMin[node->axis] > node->dist)
			node = node->children[0];
		else if (ent->absMax[node->axis] < node->dist)
			node = node->children[1];
		else
			break;	// Crosses the node
	}
	
	// Link it in	
	if (ent->solid == SOLID_TRIGGER)
		InsertLinkBefore (&ent->area, &node->trigger_edicts);
	else
		InsertLinkBefore (&ent->area, &node->solid_edicts);
}


/*
================
SV_AreaEdicts
================
*/
static void SV_AreaEdicts_r (areaNode_t *node, TList<edict_t*> &list)
{
	link_t		*l, *next, *start;
	edict_t		*check;

	// Touch linked edicts
	if (sv_areaType == AREA_SOLID)
		start = &node->solid_edicts;
	else
		start = &node->trigger_edicts;

	for (l=start->next ; l!=start ; l=next) {
		next = l->next;
		check = EDICT_FROM_AREA(l);

		if (check->solid == SOLID_NOT)
			continue;		// Deactivated
		if (check->absMin[0] > sv_areaMaxs[0]
		|| check->absMin[1] > sv_areaMaxs[1]
		|| check->absMin[2] > sv_areaMaxs[2]
		|| check->absMax[0] < sv_areaMins[0]
		|| check->absMax[1] < sv_areaMins[1]
		|| check->absMax[2] < sv_areaMins[2])
			continue;		// Not touching

		/*
		if (sv_areaCount == sv_areaMaxCount) {
			Com_Printf (0, "SV_AreaEdicts: MAXCOUNT\n");
			return;
		}
		*/

		list.Add(check);
	}
	
	if (node->axis == -1)
		return;		// Terminal node

	// Recurse down both sides
	if (sv_areaMaxs[node->axis] > node->dist)
		SV_AreaEdicts_r (node->children[0], list);
	if (sv_areaMins[node->axis] < node->dist)
		SV_AreaEdicts_r (node->children[1], list);
}

TList<edict_t*> SV_AreaEdicts (vec3_t mins, vec3_t maxs, int areaType)
{
	sv_areaMins = mins;
	sv_areaMaxs = maxs;
	sv_areaType = areaType;
	TList<edict_t*> list;

	SV_AreaEdicts_r (sv_areaNodes, list);

	return list;
}

/*
===============================================================================

	POINT CONTENTS

===============================================================================
*/

/*
================
SV_HullForEntity

Returns a headNode that can be used for testing or clipping an
object of mins/maxs size.
Offset is filled in to contain the adjustment that must be added to the
testing object's origin to get a point to use with the returned hull.
================
*/
static int SV_HullForEntity (edict_t *ent)
{
	struct cmBspModel_t	*model;

	// decide which clipping hull to use, based on the size
	if (ent->solid == SOLID_BSP) {
		// explicit hulls in the BSP model
		model = sv.models[ent->s.modelIndex];
		if (!model)
			Com_Error (ERR_FATAL, "MOVETYPE_PUSH with a non bsp model");

		return CM_InlineModelHeadNode (model);
	}

	// create a temp hull from bounding box sizes
	return CM_HeadnodeForBox (ent->mins, ent->maxs);
}


/*
=============
SV_PointContents
=============
*/
int SV_PointContents (vec3_t p)
{
	edict_t		*hit;
	int			contents;
	int			headNode;
	float		*angles;

	// Get base contents from world
	contents = CM_PointContents (p, CM_InlineModelHeadNode (sv.models[1]));

	// Or in contents from all the other entities
	var touch = SV_AreaEdicts (p, p, AREA_SOLID);

	for (uint32 i=0 ; i<touch.Count() ; i++) {
		hit = touch[i];

		// Might intersect, so do an exact clip
		headNode = SV_HullForEntity (hit);
		if (hit->solid != SOLID_BSP)
			angles = vec3Origin;	// Boxes don't rotate
		else
			angles = hit->s.angles;

		contents |= CM_TransformedPointContents (p, headNode, hit->s.origin, hit->s.angles);
	}

	return contents;
}

/*
===============================================================================

	TRACING

===============================================================================
*/

/*
====================
SV_ClipMoveToEntities
====================
*/
static void SV_ClipMoveToEntities (moveClip_t *clip)
{
	edict_t		*touch;
	cmTrace_t	trace;
	int			headNode;
	float		*angles;

	var touchlist = SV_AreaEdicts (clip->boxMins, clip->boxMaxs, AREA_SOLID);

	/*
	** be careful, it is possible to have an entity in this
	** list removed before we get to it (killtriggered)
	*/
	for (uint32 i=0 ; i<touchlist.Count() ; i++) {
		touch = touchlist[i];
		if (touch->solid == SOLID_NOT)
			continue;
		if (touch == clip->passEdict)
			continue;
		if (clip->trace.allSolid)
			return;
		if (clip->passEdict) {
			if (touch->owner == clip->passEdict)
				continue;	// Don't clip against own missiles
			if (clip->passEdict->owner == touch)
				continue;	// Don't clip against owner
		}

		if (!(clip->contentMask & CONTENTS_DEADMONSTER) && (touch->svFlags & SVF_DEADMONSTER))
			continue;

		// Might intersect, so do an exact clip
		headNode = SV_HullForEntity (touch);
		if (touch->solid != SOLID_BSP)
			angles = vec3Origin;	// Boxes don't rotate
		else
			angles = touch->s.angles;

		if (touch->svFlags & SVF_MONSTER)
			CM_TransformedBoxTrace (&trace, clip->start, clip->end,
				clip->mins2, clip->maxs2, headNode, clip->contentMask,
				touch->s.origin, angles);
		else
			CM_TransformedBoxTrace (&trace, clip->start, clip->end,
				clip->mins, clip->maxs, headNode,  clip->contentMask,
				touch->s.origin, angles);

		if (trace.allSolid || trace.startSolid || (trace.fraction < clip->trace.fraction)) {
			trace.ent = touch;
			if (clip->trace.startSolid) {
				clip->trace = trace;
				clip->trace.startSolid = true;
			}
			else
				clip->trace = trace;
		}
		else if (trace.startSolid)
			clip->trace.startSolid = true;
	}
}


/*
==================
SV_TraceBounds
==================
*/
static void SV_TraceBounds (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, vec3_t boxMins, vec3_t boxMaxs)
{
	int		i;
	
	for (i=0 ; i<3 ; i++) {
		if (end[i] > start[i]) {
			boxMins[i] = start[i] + mins[i] - 1;
			boxMaxs[i] = end[i] + maxs[i] + 1;
		}
		else {
			boxMins[i] = end[i] + mins[i] - 1;
			boxMaxs[i] = start[i] + maxs[i] + 1;
		}
	}
}


/*
==================
SV_Trace

Moves the given mins/maxs volume through the world from start to end.

passEdict and edicts owned by passEdict are explicitly not checked.
==================
*/
cmTrace_t SV_Trace (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, edict_t *passEdict, int contentMask)
{
	moveClip_t	clip;

	if (!mins)
		mins = vec3Origin;
	if (!maxs)
		maxs = vec3Origin;

	memset (&clip, 0, sizeof(moveClip_t));

	// Clip to world
	clip.trace = CM_BoxTrace (start, end, mins, maxs, 0, contentMask);
	clip.trace.ent = ge->edicts;
	if (clip.trace.fraction == 0)
		return clip.trace;		// Blocked by the world

	clip.contentMask = contentMask;
	clip.start = start;
	clip.end = end;
	clip.mins = mins;
	clip.maxs = maxs;
	clip.passEdict = passEdict;

	Vec3Copy (mins, clip.mins2);
	Vec3Copy (maxs, clip.maxs2);
	
	// Create the bounding box of the entire move
	SV_TraceBounds (start, clip.mins2, clip.maxs2, end, clip.boxMins, clip.boxMaxs);

	// Clip to other solid entities
	SV_ClipMoveToEntities (&clip);

	return clip.trace;
}
