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
// cg_weapon.c
// View weapon stuff
//

#include "cg_local.h"

vec3_t gunAngles;

/*
=======================================================================

	VIEW WEAPON

=======================================================================
*/

struct weapModel_t
{
	refModel_t *gunModel;
	int gunIndex, ammoIndex;

	int fallEff_Time, fallEff_rebTime;
	float fallKick;

	float prevFrameTime;
	float nextFrameTime;

	int	oldFrame;
	int curFrame;

	float backLerp;
	int curAnim;

	void SetGun (int index)
	{
		if (index == 0)
		{
			gunIndex = 0;
			gunModel = null;
			ammoIndex = 0;
			return;
		}

		int item = ITEM_INDEX(Items::WeaponFromID(index-1));
		gunIndex = index;
		gunModel = cgMedia.viewModelRegistry[item];

		if (itemlist[item].ammo != null)
			ammoIndex = ITEM_INDEX(FindItem(itemlist[item].ammo));
		else
			ammoIndex = 0;
	}

	void Update (refEntity_t &gun, playerState_t *ps, playerState_t *ops)
	{
		if (curAnim == -1 ||
			(ps->gunAnim != -1 && (ps->gunAnim != ops->gunAnim)))
		{
			ops->gunAnim = ps->gunAnim;

			var nextAnim = cgi.R_GetModelAnimation(gun.model, ps->gunAnim);

			curAnim = ps->gunAnim;
			oldFrame = curFrame;
			curFrame = nextAnim->Start;
			prevFrameTime = cg.refreshTime;
			nextFrameTime = cg.refreshTime + (1000.0f / nextAnim->FPS);
		}

		if (curFrame == 0)
			oldFrame = 0; // no interp

		var anim = cgi.R_GetModelAnimation(gun.model, curAnim);

		if (oldFrame == -1)
			oldFrame = curFrame = anim->Start;

		if( cg.refreshTime > nextFrameTime )
		{
			backLerp = 1.0f;

			oldFrame = curFrame;
			curFrame++;

			if (curFrame > anim->End)
			{
				if (anim->NextAnimation != -1)
				{
					curAnim = anim->NextAnimation;
					anim = cgi.R_GetModelAnimation(gun.model, curAnim);
				}

				curFrame = anim->Start;
			}

			prevFrameTime = cg.refreshTime;
			nextFrameTime = cg.refreshTime + (1000.0f / anim->FPS);
		}
		else
		{
			backLerp = 1.0f - ( ( cg.refreshTime - prevFrameTime ) / ( nextFrameTime - prevFrameTime ) );
			if( backLerp > 1 )
				backLerp = 1.0f;
			else if( backLerp < 0 )
				backLerp = 0;
		}
	}
} weap;

int CG_GetAmmoIndex ()
{
	return weap.ammoIndex;
}

/*
==============
CG_CalcGunOffset
==============
*/
void CG_CalcGunOffset( playerState_t *ps, playerState_t *ops, vec3_t angles )
{
	int		i;
	float	delta;

	// gun angles from bobbing
	if( cg.bobCycle & 1 ) {
		angles[ROLL] -= cg.xyspeed * cg.bobFracSin * 0.002 * cg_bobRoll->floatVal;
		angles[YAW] -= cg.xyspeed * cg.bobFracSin * 0.002 * cg_bobYaw->floatVal;
	} else {
		angles[ROLL] += cg.xyspeed * cg.bobFracSin * 0.002 * cg_bobRoll->floatVal;
		angles[YAW] += cg.xyspeed * cg.bobFracSin * 0.002 * cg_bobYaw->floatVal;
	}
	angles[PITCH] += cg.xyspeed * cg.bobFracSin * 0.002 * cg_bobPitch->floatVal;

	// gun angles from delta movement
	for( i = 0; i < 3; i++ ) {
		delta = ( ops->viewAngles[i] - ps->viewAngles[i] ) * cg.lerpFrac;
		if( delta > 180 )
			delta -= 360;
		if( delta < -180 )
			delta += 360;
		clamp( delta, -45, 45 );

		if( i == YAW )
			angles[ROLL] += 0.001 * delta;
		angles[i] += 0.002 * delta;
	}
}

/*
==============
CG_vWeapStartFallKickEff
==============
*/
void CG_vWeapStartFallKickEff( int parms )
{
	int	bouncetime;
	bouncetime = ((parms + 1)*15)+35;
	weap.fallEff_Time = cg.refreshTime + 2*bouncetime;
	weap.fallEff_rebTime = cg.refreshTime + bouncetime;
}

/*
===============
CG_vWeapSetPosition
===============
*/
void CG_vWeapSetPosition( refEntity_t *gun, playerState_t *ops )
{
	vec3_t			gun_angles;
	vec3_t			forward, right, up;
	float			gunx, guny, gunz;

	// set up gun position
	Vec3Copy( cg.refDef.viewOrigin, gun->origin );
	Vec3Copy( cg.refDef.viewAngles, gun_angles );

	// offset by client cvars
	gunx = 0;//cg_gunx->value;
	guny = 0;//cg_guny->value;
	gunz = 0;//cg_gunz->value;

	// add fallkick effect
	if( weap.fallEff_Time > cg.refreshTime )
		weap.fallKick += ( weap.fallEff_rebTime - cg.refreshTime ) * 0.001f;
	else
		weap.fallKick = 0;

	guny -= weap.fallKick;

	// move the gun
	Angles_Vectors( gun_angles, forward, right, up );
	Vec3MA( gun->origin, gunx, right, gun->origin );
	Vec3MA( gun->origin, gunz, forward, gun->origin );
	Vec3MA( gun->origin, guny, up, gun->origin );

	// add bobbing
	CG_CalcGunOffset( &cg.frame.playerState, ops, gun_angles );

	Vec3Copy( gun->origin, gun->oldOrigin );	// don't lerp
	//VectorCopy( gun->origin, gun->lightingOrigin );
	Angles_Matrix3( gun_angles, gun->axis );

	if (hand->intVal == 1)
	{
		gun->flags |= RF_CULLHACK;
		Vec3Negate (gun->axis[1], gun->axis[1]);
	}
}

/*
==============
CG_AddViewWeapon
==============
*/
void CG_AddViewWeapon ()
{
	refEntity_t			gun;
	entityState_t		*state;
	playerState_t	*ps, *ops;
//	vec3_t				angles;
//	int					pnum;

	// Dead
	if (cg.frame.playerState.stats[STAT_HEALTH] <= 0)
		return;

	// Don't draw the weapon in third person
	if (cg.thirdPerson)
		return;

	// Allow the gun to be completely removed
	if (!cl_gun->intVal || hand->intVal == 2)
		return;

	// Find the previous frame to interpolate from
	state = &cg_entityList[cg.playerNum + 1].current;
	ps = &cg.frame.playerState;
	if (cg.oldFrame.serverFrame != cg.frame.serverFrame-1 || !cg.oldFrame.valid)
		ops = &cg.frame.playerState;	// Previous frame was dropped or invalid
	else
		ops = &cg.oldFrame.playerState;

	memset (&gun, 0, sizeof(gun));

	if (weap.gunIndex != ((state->skinNum >> 8) & 255))
		weap.SetGun((state->skinNum >> 8) & 255);

	gun.model = weap.gunModel;

	if (!gun.model)
		return;

	// Set up gun position
	CG_vWeapSetPosition(&gun, ops);

	for( int i = 0; i < 2; i++ ) {
		switch( state->events[i].ID) {
			case EV_FALL:
				CG_vWeapStartFallKickEff( state->events[i].parms.ToByte(0) );
				break;
		}
	}

	Vec4Set (gun.color, 255, 255, 255, 255);

	gun.flags |= RF_MINLIGHT|RF_DEPTHHACK|RF_WEAPONMODEL;

	gun.flags |= state->renderFx;
	if (state->effects & EF_PENT)
		gun.flags |= RF_SHELL_RED;
	if (state->effects & EF_QUAD)
		gun.flags |= RF_SHELL_BLUE;
	if (state->effects & EF_DOUBLE)
		gun.flags |= RF_SHELL_DOUBLE;
	if (state->effects & EF_HALF_DAMAGE)
		gun.flags |= RF_SHELL_HALF_DAM;

	if (state->renderFx & RF_TRANSLUCENT)
		gun.color[3] = 179;

	if (state->effects & EF_BFG) {
		gun.flags |= RF_TRANSLUCENT;
		gun.color[3] = 77;
	}

	if (state->effects & EF_PLASMA) {
		gun.flags |= RF_TRANSLUCENT;
		gun.color[3] = 153;
	}

	if (state->effects & EF_SPHERETRANS) {
		gun.flags |= RF_TRANSLUCENT;

		if (state->effects & EF_TRACKERTRAIL)
			gun.color[3] = 155;
		else
			gun.color[3] = 77;
	}

	weap.Update(gun, ps, ops);
		
	gun.scale = 1.0f;
	gun.backLerp = weap.backLerp;
	gun.oldFrame = weap.oldFrame;
	gun.frame = weap.curFrame;

	cgi.R_AddEntity (&gun);

	gun.skinNum = 0;
	gun.flags |= RF_DEPTHHACK;
	if (gun.flags & RF_SHELLMASK)
	{
		if (gun.flags & RF_SHELL_DOUBLE)
			gun.skin = cgMedia.modelShellDouble;

		if (gun.flags & RF_SHELL_HALF_DAM)
			gun.skin = cgMedia.modelShellHalfDam;

		if ((gun.flags & RF_SHELL_RED) && (gun.flags & RF_SHELL_GREEN) && (gun.flags & RF_SHELL_BLUE))
			gun.skin = cgMedia.modelShellGod;
		else
		{
			if (gun.flags & RF_SHELL_RED)
				gun.skin = cgMedia.modelShellRed;
			if (gun.flags & RF_SHELL_GREEN)
				gun.skin = cgMedia.modelShellGreen;
			if (gun.flags & RF_SHELL_BLUE)
				gun.skin = cgMedia.modelShellBlue;
		}

		cgi.R_AddEntity (&gun);
	}
}

/*
=======================================================================

	INIT / SHUTDOWN

=======================================================================
*/

/*
=============
CG_WeapRegister
=============
*/
void CG_WeapRegister ()
{
	Vec3Clear(gunAngles);
	weap.gunIndex = weap.ammoIndex = 0;
	weap.gunModel = null;
}


/*
=============
CG_WeapUnregister
=============
*/
void CG_WeapUnregister ()
{
}
