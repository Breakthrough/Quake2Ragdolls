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
// cg_localents.c
// Local entities. Things like bullet casings...
//

#include "cg_local.h"
#include <list>

typedef TLinkedList<localEnt_t*> TLocalEnts;
TLocalEnts localEnts;

/*
=============================================================================

	LOCAL ENTITY THINKING

=============================================================================
*/

/*
===============
LE_ClipVelocity
===============
*/
#define	STOP_EPSILON	0.1f
static int LE_ClipVelocity (vec3_t in, vec3_t normal, vec3_t out, float overBounce)
{
	float		backOff, change;
	int			blocked, i;

	if (normal[2] >= 0)
		blocked = 1;	// Floor
	else
		blocked = 0;
	if (!normal[2])
		blocked |= 2;	// Step

	backOff = DotProduct(in, normal) * overBounce;
	for (i=0 ; i<3 ; i++) {
		change = normal[i] * backOff;
		out[i] = in[i] - change;
		if (out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON)
			out[i] = 0;
	}

	return blocked;
}

/*
===============
LE_BrassThink
===============
*/
void brassLocalEnt_t::Think ()
{
	cmTrace_t	tr;
	vec3_t		normal, perpNormal;
	vec3_t		perpPos;
	vec3_t		angles;

	// Check if time is up
	if (cg.refreshTime >= time+(cg_brassTime->floatVal*1000)) {
		remove = true;
		return;
	}
	
	// Check if we stopped moving
	switch (brassState)
	{
	case BRASS_RUNNING:
		// Run a frame
		// Copy origin to old
		Vec3Copy (refEnt.origin, refEnt.oldOrigin);

		// Move angles
		Vec3MA (this->angles, cg.refreshFrameTime, avel, angles);
		Angles_Matrix3 (angles, refEnt.axis);

		// Scale velocity and add gravity
		refEnt.origin[0] += velocity[0] * cg.refreshFrameTime;
		refEnt.origin[1] += velocity[1] * cg.refreshFrameTime;
		refEnt.origin[2] += velocity[2] * cg.refreshFrameTime;
		velocity[2] -= gravity * cg.refreshFrameTime;

		// Check for collision
		CG_PMTrace (&tr, refEnt.oldOrigin, mins, maxs, refEnt.origin, false, false);
		if (tr.allSolid || tr.startSolid) {
			remove = true;
			return;
		}

		if (tr.fraction != 1.0f) {
			Vec3Copy (tr.endPos, refEnt.origin);

			// Check if it's time to stop
			LE_ClipVelocity (velocity, tr.plane.normal, velocity, 1.0f + (frand () * 0.5f));
			if (tr.plane.normal[2] > 0.7f) {
				// Play crash sound
				if (type == LE_SGSHELL)
					cgi.Snd_StartSound (tr.endPos, 0, CHAN_AUTO, cgMedia.sfx.sgShell[(rand()&1)], 1, ATTN_STATIC, 0);
				else
					cgi.Snd_StartSound (tr.endPos, 0, CHAN_AUTO, cgMedia.sfx.mgShell[(rand()&1)], 1, ATTN_STATIC, 0);

				// Store current angles
				Vec3Copy (angles, savedAngle);

				// Orient flat and rotate randomly on the plane
				VectorNormalizef (tr.plane.normal, normal);
				PerpendicularVector (normal, perpNormal);
				ProjectPointOnPlane (perpPos, tr.endPos, perpNormal);
				RotatePointAroundVector (angles, perpNormal, perpPos, frand()*360);

				// Found our new home
				brassState++;
			}
		}
		break;

	case BRASS_INTERP0:
	case BRASS_INTERP1:
	case BRASS_INTERP2:
		// Interpolate between last and final
		this->angles[0] = LerpAngle (this->angles[0], savedAngle[0], 0.25f * brassState);
		this->angles[1] = LerpAngle (this->angles[1], savedAngle[1], 0.25f * brassState);
		this->angles[2] = LerpAngle (this->angles[2], savedAngle[2], 0.25f * brassState);
		Angles_Matrix3 (this->angles, refEnt.axis);
		brassState++;
		break;

	case BRASS_ANGLE:
		// Set final
		Angles_Matrix3 (this->angles, refEnt.axis);
		brassState++;
		break;

	case BRASS_SLEEP:
		// Rest
		break;

	default:
		assert (0);
		break;
	}

	// Update color
	ColorUpdate ();
}

/*
=============================================================================

	LOCAL ENTITY MANAGEMENT

=============================================================================
*/

/*
===============
CG_ClearLocalEnts
===============
*/
void CG_ClearLocalEnts ()
{
	for (var node = localEnts.Head(); node != null; node = node->Next)
		delete node->Value;

	localEnts.Clear();
}


/*
===============
CG_AddLocalEnts
===============
*/
void CG_AddLocalEnts ()
{
	TLocalEnts::Node *node = localEnts.Head();
	TLocalEnts::Node *next;

	for (; node != null; node = next)
	{
		next = node->Next;

		var le = node->Value;

		// Run physics and other per-frame things
		le->Think();

		// Remove if desired
		if (le->remove)
		{
			delete le;
			localEnts.RemoveNode(node);
		}
		else
			// Add to refresh
			cgi.R_AddEntity (&le->refEnt);
		
		node = next;
	}
}

localEnt_t::localEnt_t (vec3_t origin, colorb color, vec3_t angles, vec3_t avelocity, vec3_t velocity)
{
	memset(this, 0, sizeof(*this));

	time = cg.refreshTime;
	Vec3Copy (origin, refEnt.origin);
	Vec3Copy (origin, refEnt.oldOrigin);
	Vec4Copy (color, refEnt.color);

	Vec3Copy (angles, this->angles);
	Vec3Copy (avelocity, this->avel);
	Angles_Matrix3 (angles, refEnt.axis);

	Vec3Copy (velocity, this->velocity);

	// Set up the refEnt
	refEnt.flags = RF_NOSHADOW;

	refEnt.scale = 1.0f;

	localEnts.AddToBack(this);
}

void localEnt_t::ColorUpdate()
{
	vec3_t	color;

	cgi.R_LightPoint (refEnt.origin, color);
	ColorNormalizeb (color, refEnt.color);
}

void localEnt_t::SetModel (refModel_t* model)
{
	refEnt.model = model;
	cgi.R_ModelBounds (refEnt.model, mins, maxs);
}