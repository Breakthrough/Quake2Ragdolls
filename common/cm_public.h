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
// cm_public.h
//

/*
=============================================================================

	CMODEL

=============================================================================
*/

struct cmBspModel_t	*CM_LoadMap (char *name, bool clientLoad, uint32 *checksum);
void		CM_UnloadMap ();
struct cmBspModel_t	*CM_InlineModel (char *name);	// *1, *2, etc
void		CM_InlineModelBounds (struct cmBspModel_t *model, vec3_t mins, vec3_t maxs);
int			CM_InlineModelHeadNode (struct cmBspModel_t *model);

// ==========================================================================

void		CM_PrintStats ();

// ==========================================================================

char		*CM_EntityString ();
char		*CM_SurfRName (int texNum);

int			CM_NumClusters ();
int			CM_NumInlineModels ();
int			CM_NumTexInfo ();

// ==========================================================================

int			CM_HeadnodeForBox (vec3_t mins, vec3_t maxs);	// creates a clipping hull for an arbitrary box

int			CM_PointContents (vec3_t p, int headNode);	// returns an ORed contents mask
int			CM_TransformedPointContents (vec3_t p, int headNode, vec3_t origin, vec3_t angles);

cmTrace_t	CM_Trace (vec3_t start, vec3_t end, float size,  int contentMask);
cmTrace_t	CM_BoxTrace (vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs,  int headNode, int brushMask);
void		CM_TransformedBoxTrace (cmTrace_t *out, vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, int headNode, int brushMask, vec3_t origin, vec3_t angles);

byte		*CM_ClusterPVS (int cluster);
byte		*CM_ClusterPHS (int cluster);

int			CM_PointLeafnum (vec3_t p);

// call with topnode set to the headnode, returns with topnode
// set to the first node that splits the box
int			CM_BoxLeafnums (vec3_t mins, vec3_t maxs, int *list, int listSize, int *topNode);

int			CM_LeafContents (int leafnum);
int			CM_LeafCluster (int leafnum);
int			CM_LeafArea (int leafnum);

void		CM_SetAreaPortalState (int portalNum, int area, int otherArea, bool open);
bool		CM_AreasConnected (int area1, int area2);

int			CM_WriteAreaBits (byte *buffer, int area);
bool		CM_HeadnodeVisible (int headNode, byte *visBits);

void		CM_WritePortalState (fileHandle_t fileNum);
void		CM_ReadPortalState (fileHandle_t fileNum);

// ==========================================================================

void		Patch_GetFlatness (float maxflat, vec3_t *points, int *patch_cp, int *flat);
void		Patch_Evaluate (float *p, int *numcp, int *tess, float *dest, int comp);
