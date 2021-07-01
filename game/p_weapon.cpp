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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// g_weapon.c

#include "g_local.h"
#include "m_player.h"


static bool	is_quad;
static short		is_silenced;


void weapon_grenade_fire (edict_t *ent, bool held);


static void P_ProjectSource (gclient_t *client, vec3_t point, vec3_t distance, vec3_t forward, vec3_t right, vec3_t result)
{
	vec3_t	_distance;

	Vec3Copy (distance, _distance);
	if (client->pers.hand == LEFT_HANDED)
		_distance[1] *= -1;
	else if (client->pers.hand == CENTER_HANDED)
		_distance[1] = 0;
	G_ProjectSource (point, _distance, forward, right, result);
}


/*
===============
PlayerNoise

Each player can have two noise objects associated with it:
a personal noise (jumping, pain, weapon firing), and a weapon
target noise (bullet wall impacts)

Monsters that don't directly see the player can move
to a noise in hopes of seeing the player from there.
===============
*/
void PlayerNoise(edict_t *who, vec3_t where, int type)
{
	edict_t		*noise;

	if (type == PNOISE_WEAPON)
	{
		if (who->client->silencer_shots)
		{
			who->client->silencer_shots--;
			return;
		}
	}

	if (deathmatch->floatVal)
		return;

	if (who->flags & FL_NOTARGET)
		return;


	if (!who->mynoise)
	{
		noise = G_Spawn();
		noise->classname = "player_noise";
		Vec3Set (noise->mins, -8, -8, -8);
		Vec3Set (noise->maxs, 8, 8, 8);
		noise->owner = who;
		noise->svFlags = SVF_NOCLIENT;
		who->mynoise = noise;

		noise = G_Spawn();
		noise->classname = "player_noise";
		Vec3Set (noise->mins, -8, -8, -8);
		Vec3Set (noise->maxs, 8, 8, 8);
		noise->owner = who;
		noise->svFlags = SVF_NOCLIENT;
		who->mynoise2 = noise;
	}

	if (type == PNOISE_SELF || type == PNOISE_WEAPON)
	{
		noise = who->mynoise;
		level.sound_entity = noise;
		level.sound_entity_framenum = level.framenum;
	}
	else // type == PNOISE_IMPACT
	{
		noise = who->mynoise2;
		level.sound2_entity = noise;
		level.sound2_entity_framenum = level.framenum;
	}

	Vec3Copy (where, noise->s.origin);
	Vec3Subtract (where, noise->maxs, noise->absMin);
	Vec3Add (where, noise->maxs, noise->absMax);
	noise->teleport_time = level.time;
	gi.linkentity (noise);
}


bool Pickup_Weapon (edict_t *ent, edict_t *other)
{
	int			index;
	gitem_t		*ammo;

	index = ITEM_INDEX(ent->item);

	if ( ( ((int)(dmflags->floatVal) & DF_WEAPONS_STAY) || coop->floatVal) 
		&& other->client->pers.inventory[index])
	{
		if (!(ent->spawnflags & (DROPPED_ITEM | DROPPED_PLAYER_ITEM) ) )
			return false;	// leave the weapon for others to pickup
	}

	other->client->pers.inventory[index]++;

	if (!(ent->spawnflags & DROPPED_ITEM) )
	{
		// give them some ammo with it
		ammo = FindItem (ent->item->ammo);
		if ( (int)dmflags->floatVal & DF_INFINITE_AMMO )
			Add_Ammo (other, ammo, 1000);
		else
			Add_Ammo (other, ammo, ammo->quantity);

		if (! (ent->spawnflags & DROPPED_PLAYER_ITEM) )
		{
			if (deathmatch->floatVal)
			{
				if ((int)(dmflags->floatVal) & DF_WEAPONS_STAY)
					ent->flags |= FL_RESPAWN;
				else
					SetRespawn (ent, 30);
			}
			if (coop->floatVal)
				ent->flags |= FL_RESPAWN;
		}
	}

	if (other->client->pers.weapon != ent->item && 
		(other->client->pers.inventory[index] == 1) &&
		( !deathmatch->floatVal || other->client->pers.weapon == FindItem("blaster") ) )
		other->client->newweapon = ent->item;

	return true;
}


/*
===============
ChangeWeapon

The old weapon has been dropped all the way, so make the new one
current
===============
*/
void ChangeWeapon (edict_t *ent)
{
	if (ent->client->grenade_time)
	{
		ent->client->grenade_time = level.time;
		ent->client->weapon_sound = 0;
		weapon_grenade_fire (ent, false);
		ent->client->grenade_time = 0;
	}

	ent->client->pers.lastweapon = ent->client->pers.weapon;
	ent->client->pers.weapon = ent->client->newweapon;
	ent->client->newweapon = NULL;
	ent->client->machinegun_shots = 0;

	// set visible model
	if (ent->s.modelIndex == 255)
		ent->s.skinNum = ((ent - g_edicts - 1) << 0) | (((ent->client->pers.weapon != null) ? (Items::GetWeaponID(ent->client->pers.weapon) + 1) : 0) << 8);

	if (ent->client->pers.weapon && ent->client->pers.weapon->ammo)
		ent->client->ammo_index = ITEM_INDEX(FindItem(ent->client->pers.weapon->ammo));
	else
		ent->client->ammo_index = 0;

	if (!ent->client->pers.weapon)
		return;

	ent->client->weaponstate = WEAPON_ACTIVATING;
	ent->client->weapontime = 0;
	ent->client->ps.gunAnim = 0;
}

/*
=================
NoAmmoWeaponChange
=================
*/
void NoAmmoWeaponChange (edict_t *ent)
{
	if ( ent->client->pers.inventory[ITEM_INDEX(FindItem("slugs"))]
		&&  ent->client->pers.inventory[ITEM_INDEX(FindItem("railgun"))] )
	{
		ent->client->newweapon = FindItem ("railgun");
		return;
	}
	if ( ent->client->pers.inventory[ITEM_INDEX(FindItem("cells"))]
		&&  ent->client->pers.inventory[ITEM_INDEX(FindItem("hyperblaster"))] )
	{
		ent->client->newweapon = FindItem ("hyperblaster");
		return;
	}
	if ( ent->client->pers.inventory[ITEM_INDEX(FindItem("bullets"))]
		&&  ent->client->pers.inventory[ITEM_INDEX(FindItem("chaingun"))] )
	{
		ent->client->newweapon = FindItem ("chaingun");
		return;
	}
	if ( ent->client->pers.inventory[ITEM_INDEX(FindItem("bullets"))]
		&&  ent->client->pers.inventory[ITEM_INDEX(FindItem("machinegun"))] )
	{
		ent->client->newweapon = FindItem ("machinegun");
		return;
	}
	if ( ent->client->pers.inventory[ITEM_INDEX(FindItem("shells"))] > 1
		&&  ent->client->pers.inventory[ITEM_INDEX(FindItem("super shotgun"))] )
	{
		ent->client->newweapon = FindItem ("super shotgun");
		return;
	}
	if ( ent->client->pers.inventory[ITEM_INDEX(FindItem("shells"))]
		&&  ent->client->pers.inventory[ITEM_INDEX(FindItem("shotgun"))] )
	{
		ent->client->newweapon = FindItem ("shotgun");
		return;
	}
	ent->client->newweapon = FindItem ("blaster");
}

/*
================
Use_Weapon

Make the weapon ready if there is ammo
================
*/
void Use_Weapon (edict_t *ent, gitem_t *item)
{
	int			ammo_index;
	gitem_t		*ammo_item;

	// see if we're already using it
	if (item == ent->client->pers.weapon)
		return;

	if (item->ammo && !g_select_empty->floatVal && !(item->flags & IT_AMMO))
	{
		ammo_item = FindItem(item->ammo);
		ammo_index = ITEM_INDEX(ammo_item);

		if (!ent->client->pers.inventory[ammo_index])
		{
			gi.cprintf (ent, PRINT_HIGH, "No %s for %s.\n", ammo_item->pickup_name, item->pickup_name);
			return;
		}

		if (ent->client->pers.inventory[ammo_index] < item->quantity)
		{
			gi.cprintf (ent, PRINT_HIGH, "Not enough %s for %s.\n", ammo_item->pickup_name, item->pickup_name);
			return;
		}
	}

	// change to this weapon when down
	ent->client->newweapon = item;
}



/*
================
Drop_Weapon
================
*/
void Drop_Weapon (edict_t *ent, gitem_t *item)
{
	int		index;

	if ((int)(dmflags->floatVal) & DF_WEAPONS_STAY)
		return;

	index = ITEM_INDEX(item);
	// see if we're already using it
	if ( ((item == ent->client->pers.weapon) || (item == ent->client->newweapon))&& (ent->client->pers.inventory[index] == 1) )
	{
		gi.cprintf (ent, PRINT_HIGH, "Can't drop current weapon\n");
		return;
	}

	Drop_Item (ent, item);
	ent->client->pers.inventory[index]--;
}

void Weapon_GenericTime(edict_t *ent, int activate_time, int fire_time, int deact_time, int *fire_frames, void (*fire)(edict_t *ent))
{
	if (ent->client->weaponstate != WEAPON_READY)
		ent->client->weapontime += (int)ceilf(ServerFrameTime);

	switch (ent->client->weaponstate)
	{
	case WEAPON_ACTIVATING:
		if (ent->client->weapontime >= activate_time)
		{
			ent->client->weapontime = 0;
			ent->client->weaponstate = WEAPON_READY;
			ent->client->ps.gunAnim = -1;
		}
		break;

	case WEAPON_READY:
		if (ent->client->newweapon)
		{
			ent->client->weaponstate = WEAPON_DROPPING;
			ent->client->ps.gunAnim = 3;
			ent->client->weapontime = 0;

			AddEntityEvent(ent, EV_GESTURE, entityParms_t().PushByte(6));
			return;
		}

		if ( ((ent->client->latched_buttons|ent->client->buttons) & BUTTON_ATTACK) )
		{
			ent->client->latched_buttons &= ~BUTTON_ATTACK;
			if ((!ent->client->ammo_index) || 
				( ent->client->pers.inventory[ent->client->ammo_index] >= ent->client->pers.weapon->quantity))
			{
				ent->client->weaponfireindex = 0;
				ent->client->ps.gunAnim = 1;
				ent->client->weaponstate = WEAPON_FIRING;

				// start the animation
				/*ent->client->anim_priority = ANIM_ATTACK;
				if (ent->client->ps.pMove.pmFlags & PMF_DUCKED)
				{
					ent->s.frame = FRAME_crattak1-1;
					ent->client->anim_end = FRAME_crattak9;
				}
				else
				{
					ent->s.frame = FRAME_attack1-1;
					ent->client->anim_end = FRAME_attack8;
				}*/
			}
			else
			{
				if (level.time >= ent->pain_debounce_time)
				{
					gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/noammo.wav"), 1, ATTN_NORM, 0);
					ent->pain_debounce_time = level.time + 1;
				}
				NoAmmoWeaponChange (ent);
				break;
			}
		}

		if (ent->client->weaponstate == WEAPON_READY)
			break;

	case WEAPON_FIRING:		
		if (fire_frames[ent->client->weaponfireindex] != -1)
		{
			if (ent->client->weapontime >= fire_frames[ent->client->weaponfireindex])
			{
				if (ent->client->quad_framenum > level.framenum)
					gi.sound(ent, CHAN_ITEM, gi.soundindex("items/damage3.wav"), 1, ATTN_NORM, 0);

				fire (ent);
				ent->client->weaponfireindex++;
			}
		}

		if (ent->client->weapontime >= fire_time)
		{
			ent->client->weaponstate = WEAPON_READY;
			ent->client->weapontime = 0;

			if (ent->client->ps.gunAnim != 2)
				ent->client->ps.gunAnim = -1;
		}
		break;

	case WEAPON_DROPPING:
		if (ent->client->weapontime >= deact_time)
			ChangeWeapon (ent);
		break;
	}
}

/*
======================================================================

GRENADE

======================================================================
*/

#define GRENADE_TIMER		3.0
#define GRENADE_MINSPEED	400
#define GRENADE_MAXSPEED	800

void fire_physgrenade2 (edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, float timer, float damage_radius, bool held);
void weapon_grenade_fire (edict_t *ent, bool held)
{
	vec3_t	offset;
	vec3_t	forward, right;
	vec3_t	start;
	int		damage = 125;
	float	timer;
	int		speed;
	float	radius;

	radius = damage+40;
	if (is_quad)
		damage *= 4;

	Vec3Set (offset, 8, 8, ent->viewheight-8);
	Angles_Vectors (ent->client->v_angle, forward, right, NULL);
	P_ProjectSource (ent->client, ent->s.origin, offset, forward, right, start);

	timer = ent->client->grenade_time - level.time;
	speed = GRENADE_MINSPEED + (GRENADE_TIMER - timer) * ((GRENADE_MAXSPEED - GRENADE_MINSPEED) / GRENADE_TIMER);
	fire_physgrenade2 (ent, start, forward, damage, speed, timer, radius, held);

	if (! ( (int)dmflags->floatVal & DF_INFINITE_AMMO ) )
		ent->client->pers.inventory[ent->client->ammo_index]--;

	ent->client->grenade_time = level.time + 1.0;

	if(ent->deadflag || ent->s.modelIndex != 255) // VWep animations screw up corpses
	{
		return;
	}

	if (ent->health <= 0)
		return;

	AddEntityEvent(ent, EV_GESTURE, entityParms_t().PushByte(5));
}

void Weapon_Grenade (edict_t *ent)
{
	if ((ent->client->newweapon) && (ent->client->weaponstate == WEAPON_READY))
	{
		ChangeWeapon (ent);
		return;
	}

	if (ent->client->weaponstate == WEAPON_ACTIVATING)
	{
		ent->client->ps.gunAnim = 4;
		ent->client->weaponstate = WEAPON_READY;
		return;
	}

	if (ent->client->weaponstate == WEAPON_READY)
	{
		if ( ((ent->client->latched_buttons|ent->client->buttons) & BUTTON_ATTACK) )
		{
			ent->client->latched_buttons &= ~BUTTON_ATTACK;
			if (ent->client->pers.inventory[ent->client->ammo_index])
			{
				ent->client->ps.gunAnim = 0;
				ent->client->weaponstate = WEAPON_FIRING;
				ent->client->grenade_time = 0;
				ent->client->weapontime = 0;
			}
			else
			{
				if (level.time >= ent->pain_debounce_time)
				{
					gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/noammo.wav"), 1, ATTN_NORM, 0);
					ent->pain_debounce_time = level.time + 1;
				}
				NoAmmoWeaponChange (ent);
			}
			return;
		}

		return;
	}

	if (ent->client->weaponstate == WEAPON_FIRING)
	{
		int oldTime = ent->client->weapontime;
		ent->client->weapontime += (int)ceilf(ServerFrameTime);

		if (ent->client->weaponfireindex == 5 && ent->client->grenade_time == 0)
		{
			ent->client->grenade_time = -1;
			gi.sound(ent, CHAN_WEAPON, gi.soundindex("weapons/hgrena1b.wav"), 1, ATTN_NORM, 0);
		}

		else if (ent->client->weaponfireindex == 11)
		{
			ent->client->ps.gunAnim = 1;
			if (ent->client->grenade_time < 0)
			{
				ent->client->grenade_time = level.time + GRENADE_TIMER + 0.2;
				ent->client->weapon_sound = gi.soundindex("weapons/hgrenc1b.wav");
			}

			// they waited too long, detonate it in their hand
			if (!ent->client->grenade_blew_up && level.time >= ent->client->grenade_time)
			{
				ent->client->weapon_sound = 0;
				weapon_grenade_fire (ent, true);
				ent->client->grenade_blew_up = true;
			}

			if (ent->client->buttons & BUTTON_ATTACK)
				return;

			ent->client->ps.gunAnim = 2;

			if (ent->client->grenade_blew_up)
			{
				if (level.time >= ent->client->grenade_time)
				{
					ent->client->weapontime = 0;
					ent->client->weaponfireindex = 15;
					ent->client->grenade_blew_up = false;
				}
				else
				{
					return;
				}
			}
		}

		else if (ent->client->weaponfireindex == 12 && ent->client->weapon_sound != 0)
		{
			ent->client->weapon_sound = 0;
			weapon_grenade_fire (ent, false);
		}

		else if ((ent->client->weaponfireindex == 15) && (level.time < ent->client->grenade_time))
		{
			ent->client->ps.gunAnim = 3;
			return;
		}

		if (ent->client->weapontime > 100)
		{
			ent->client->weapontime = 0;
			ent->client->weaponfireindex++;
		}

		if (ent->client->weaponfireindex == 16)
		{
			ent->client->grenade_time = 0;
			ent->client->weaponstate = WEAPON_READY;
			ent->client->ps.gunAnim = 4;
			ent->client->weaponfireindex = 0;
		}
	}
}

/*
======================================================================

GRENADE LAUNCHER

======================================================================
*/

void fire_physgrenade (edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, float timer, float damage_radius);;
void weapon_grenadelauncher_fire (edict_t *ent)
{
	vec3_t	offset;
	vec3_t	forward, right;
	vec3_t	start;
	int		damage = 120;
	float	radius;

	radius = damage+40;
	if (is_quad)
		damage *= 4;

	Vec3Set (offset, 8, 8, ent->viewheight-8);
	Angles_Vectors (ent->client->v_angle, forward, right, NULL);
	P_ProjectSource (ent->client, ent->s.origin, offset, forward, right, start);

	Vec3Scale (forward, -2, ent->client->kick_origin);
	ent->client->kick_angles[0] = -1;

	fire_physgrenade (ent, start, forward, damage, 600, 2.5, radius);

	AddEntityEvent(ent, EV_MUZZLEFLASH, entityParms_t().PushByte(MZ_GRENADE | is_silenced));

	PlayerNoise(ent, start, PNOISE_WEAPON);

	if (! ( (int)dmflags->floatVal & DF_INFINITE_AMMO ) )
		ent->client->pers.inventory[ent->client->ammo_index]--;
}

void Weapon_GrenadeLauncher (edict_t *ent)
{
	//static int	pause_frames[]	= {34, 51, 59, 0};
	//static int	fire_frames[]	= {6, 0};

	//Weapon_Generic (ent, 5, 16, 59, 64, pause_frames, fire_frames, weapon_grenadelauncher_fire);
	static int	fire_frames[]	= {0, -1};
	Weapon_GenericTime(ent, 600, 1100, 500, fire_frames, weapon_grenadelauncher_fire);
}

/*
======================================================================

ROCKET

======================================================================
*/

void Weapon_RocketLauncher_Fire (edict_t *ent)
{
	vec3_t	offset, start;
	vec3_t	forward, right;
	int		damage;
	float	damage_radius;
	int		radius_damage;

	damage = 100 + (int)(random() * 20.0);
	radius_damage = 120;
	damage_radius = 120;
	if (is_quad)
	{
		damage *= 4;
		radius_damage *= 4;
	}

	Angles_Vectors (ent->client->v_angle, forward, right, NULL);

	Vec3Scale (forward, -2, ent->client->kick_origin);
	ent->client->kick_angles[0] = -1;

	Vec3Set (offset, 8, 8, ent->viewheight-8);
	P_ProjectSource (ent->client, ent->s.origin, offset, forward, right, start);
	fire_rocket (ent, start, forward, damage, 650, damage_radius, radius_damage);

	// send muzzle flash
	AddEntityEvent(ent, EV_MUZZLEFLASH, entityParms_t().PushByte(MZ_ROCKET | is_silenced));

	PlayerNoise(ent, start, PNOISE_WEAPON);

	if (! ( (int)dmflags->floatVal & DF_INFINITE_AMMO ) )
		ent->client->pers.inventory[ent->client->ammo_index]--;
}

void Weapon_RocketLauncher (edict_t *ent)
{
	//static int	pause_frames[]	= {25, 33, 42, 50, 0};
	//static int	fire_frames[]	= {5, 0};

	//Weapon_Generic (ent, 4, 12, 50, 54, pause_frames, fire_frames, Weapon_RocketLauncher_Fire);
	static int	fire_frames[]	= {0, -1};
	Weapon_GenericTime(ent, 500, 900, 400, fire_frames, Weapon_RocketLauncher_Fire);
}


/*
======================================================================

BLASTER / HYPERBLASTER

======================================================================
*/

void Blaster_Fire (edict_t *ent, vec3_t g_offset, int damage, bool hyper, int effect)
{
	vec3_t	forward, right;
	vec3_t	start;
	vec3_t	offset;

	if (is_quad)
		damage *= 4;
	Angles_Vectors (ent->client->v_angle, forward, right, NULL);
	Vec3Set (offset, 24, 8, ent->viewheight-8);
	Vec3Add (offset, g_offset, offset);
	P_ProjectSource (ent->client, ent->s.origin, offset, forward, right, start);

	Vec3Scale (forward, -0.5f, ent->client->kick_origin);
	ent->client->kick_angles[0] = -0.25f;

	fire_blaster (ent, start, forward, damage, 1000, effect, hyper);

	// send muzzle flash
	AddEntityEvent(ent, EV_MUZZLEFLASH, entityParms_t().PushByte(((hyper) ? MZ_HYPERBLASTER : MZ_BLASTER) | is_silenced));

	PlayerNoise(ent, start, PNOISE_WEAPON);
}


void Weapon_Blaster_Fire (edict_t *ent)
{
	int		damage;

	if (deathmatch->floatVal)
		damage = 15;
	else
		damage = 10;
	Blaster_Fire (ent, vec3Origin, damage, false, EF_BLASTER);
}

void Weapon_Blaster (edict_t *ent)
{
	//static int	pause_frames[]	= {19, 32, 0};
	//static int	fire_frames[]	= {5, 0};

	//Weapon_Generic (ent, 4, 8, 52, 55, pause_frames, fire_frames, Weapon_Blaster_Fire);
	static int fire_frames[] = {0, -1};
	Weapon_GenericTime(ent, 500, 500, 300, fire_frames, Weapon_Blaster_Fire);
}


void Weapon_HyperBlaster_Fire (edict_t *ent)
{
	float	rotation;
	vec3_t	offset;
	int		effect;
	int		damage;

	ent->client->weapon_sound = gi.soundindex("weapons/hyprbl1a.wav");

	if (!(ent->client->buttons & BUTTON_ATTACK))
	{
		//ent->client->ps.gunAnim++;
		if (ent->client->weaponfireindex == 5)
			ent->client->ps.gunAnim = 4;
	}
	else
	{
		if (! ent->client->pers.inventory[ent->client->ammo_index] )
		{
			if (level.time >= ent->pain_debounce_time)
			{
				gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/noammo.wav"), 1, ATTN_NORM, 0);
				ent->pain_debounce_time = level.time + 1;
			}
			NoAmmoWeaponChange (ent);

			if (ent->client->weaponfireindex == 5)
				ent->client->ps.gunAnim = 4;
		}
		else
		{
			rotation = (ent->client->weaponfireindex) * 2*M_PI/6;
			offset[0] = -4 * sinf(rotation);
			offset[1] = 0;
			offset[2] = 4 * cosf(rotation);

			//if ((ent->client->ps.gunAnim == 6) || (ent->client->ps.gunAnim == 9))
			if ((ent->client->weaponfireindex == 0) || (ent->client->weaponfireindex == 2))
				effect = EF_HYPERBLASTER;
			else
				effect = 0;
			if (deathmatch->floatVal)
				damage = 15;
			else
				damage = 20;
			Blaster_Fire (ent, offset, damage, true, effect);
			if (! ( (int)dmflags->floatVal & DF_INFINITE_AMMO ) )
				ent->client->pers.inventory[ent->client->ammo_index]--;

			ent->client->anim_priority = ANIM_ATTACK;
			if (ent->client->ps.pMove.pmFlags & PMF_DUCKED)
			{
				ent->s.frame = FRAME_crattak1 - 1;
				ent->client->anim_end = FRAME_crattak9;
			}
			else
			{
				ent->s.frame = FRAME_attack1 - 1;
				ent->client->anim_end = FRAME_attack8;
			}
		}

		//ent->client->ps.gunAnim++;
		//if (ent->client->ps.gunAnim == 12 && ent->client->pers.inventory[ent->client->ammo_index])
		if (ent->client->weaponfireindex == 5 && ent->client->pers.inventory[ent->client->ammo_index])
		{
			ent->client->weaponfireindex = 0;
			ent->client->weapontime = 0;
		}
	}

//	if (ent->client->ps.gunAnim == 12)
	if (ent->client->weaponfireindex == 5)
	{
		gi.sound(ent, CHAN_AUTO, gi.soundindex("weapons/hyprbd1a.wav"), 1, ATTN_NORM, 0);
		ent->client->weapon_sound = 0;
	}

}

void Weapon_HyperBlaster (edict_t *ent)
{
	//static int	pause_frames[]	= {0};
	//static int	fire_frames[]	= {6, 7, 8, 9, 10, 11, 0};

	//Weapon_Generic (ent, 5, 20, 49, 53, pause_frames, fire_frames, Weapon_HyperBlaster_Fire);
	int fire_frames[] = {0, 100, 200, 300, 400, 500, -1};
	Weapon_GenericTime (ent, 600, 1600, 500, fire_frames, Weapon_HyperBlaster_Fire);
}

/*
======================================================================

MACHINEGUN / CHAINGUN

======================================================================
*/

void Machinegun_Fire (edict_t *ent)
{
	int	i;
	vec3_t		start;
	vec3_t		forward, right;
	vec3_t		angles;
	int			damage = 8;
	int			kick = 2;
	vec3_t		offset;

	if (!(ent->client->buttons & BUTTON_ATTACK))
	{
		ent->client->machinegun_shots = 0;
		ent->client->weapontime = 200;
		ent->client->ps.gunAnim = 2;
		return;
	}

	if (ent->client->weapontime >= 100)
	{
		ent->client->weapontime = 0;
		ent->client->weaponfireindex--;
	}

	if (ent->client->pers.inventory[ent->client->ammo_index] < 1)
	{
		ent->client->ps.gunAnim = 2;
		if (level.time >= ent->pain_debounce_time)
		{
			gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/noammo.wav"), 1, ATTN_NORM, 0);
			ent->pain_debounce_time = level.time + 1;
		}
		NoAmmoWeaponChange (ent);
		return;
	}

	if (is_quad)
	{
		damage *= 4;
		kick *= 4;
	}

	for (i=1 ; i<3 ; i++)
	{
		ent->client->kick_origin[i] = crandom() * 0.35;
		ent->client->kick_angles[i] = crandom() * 0.7;
	}
	ent->client->kick_origin[0] = crandom() * 0.35;
	ent->client->kick_angles[0] = ent->client->machinegun_shots * -1.5;

	// raise the gun as it is firing
	if (!deathmatch->floatVal)
	{
		ent->client->machinegun_shots++;
		if (ent->client->machinegun_shots > 9)
			ent->client->machinegun_shots = 9;
	}

	// get start / end positions
	Vec3Add (ent->client->v_angle, ent->client->kick_angles, angles);
	Angles_Vectors (angles, forward, right, NULL);
	Vec3Set (offset, 0, 8, ent->viewheight-8);
	P_ProjectSource (ent->client, ent->s.origin, offset, forward, right, start);

	ushort seed = 1+(rand() % USHRT_MAX-1);
	MTwister randGen (seed);

	fire_bullet (ent, start, forward, damage, kick, DEFAULT_BULLET_HSPREAD, DEFAULT_BULLET_VSPREAD, MOD_MACHINEGUN, randGen);

	AddEntityEvent(ent, EV_MUZZLEFLASH, entityParms_t().PushByte(MZ_MACHINEGUN | is_silenced).PushVector(angles).PushChar(ent->viewheight).PushByte(ent->client->pers.hand).PushShort(seed));

	PlayerNoise(ent, start, PNOISE_WEAPON);

	if (! ( (int)dmflags->floatVal & DF_INFINITE_AMMO ) )
		ent->client->pers.inventory[ent->client->ammo_index]--;

	ent->client->anim_priority = ANIM_ATTACK;
	if (ent->client->ps.pMove.pmFlags & PMF_DUCKED)
	{
		ent->s.frame = FRAME_crattak1 - (int) (random()+0.25);
		ent->client->anim_end = FRAME_crattak9;
	}
	else
	{
		ent->s.frame = FRAME_attack1 - (int) (random()+0.25);
		ent->client->anim_end = FRAME_attack8;
	}
}

void Weapon_Machinegun (edict_t *ent)
{
	//static int	pause_frames[]	= {23, 45, 0};
	//static int	fire_frames[]	= {4, 5, 0};

	//Weapon_Generic (ent, 3, 5, 45, 49, pause_frames, fire_frames, Machinegun_Fire);
	static int	fire_frames[]	= {0, 100, 0};
	Weapon_GenericTime(ent, 400, 200, 400, fire_frames, Machinegun_Fire);
}

void Chaingun_Fire (edict_t *ent)
{
	int			i;
	int			shots;
	vec3_t		start;
	vec3_t		forward, right, up;
	float		r, u;
	vec3_t		offset;
	int			damage;
	int			kick = 2;

	if (deathmatch->floatVal)
		damage = 6;
	else
		damage = 8;

	//if (ent->client->ps.gunAnim == 5)
	if (ent->client->weaponfireindex == 0)
		gi.sound(ent, CHAN_AUTO, gi.soundindex("weapons/chngnu1a.wav"), 1, ATTN_IDLE, 0);

	//if ((ent->client->ps.gunAnim == 14) && !(ent->client->buttons & BUTTON_ATTACK))
	if ((ent->client->weaponfireindex == 9))
	{
		if (!(ent->client->buttons & BUTTON_ATTACK))
		{
			//ent->client->ps.gunAnim = 32;
			ent->client->weaponfireindex = 27;
			ent->client->weapontime = 2800;
			ent->client->weapon_sound = 0;
			ent->client->ps.gunAnim = 2;
			return;
		}
		ent->client->ps.gunAnim = 4;
	}
	//else if ((ent->client->ps.gunAnim == 21) && (ent->client->buttons & BUTTON_ATTACK)
	else if ((ent->client->weaponfireindex == 16))
	{
		if ((ent->client->buttons & BUTTON_ATTACK) && ent->client->pers.inventory[ent->client->ammo_index])
		{
			//ent->client->ps.gunAnim = 15;
			ent->client->weaponfireindex = 10;
			ent->client->weapontime = 1000;
		}
		else
			ent->client->ps.gunAnim = 5;
	}
	//else
	//{
		//ent->client->ps.gunAnim++;
	//}

	//if (ent->client->ps.gunAnim == 22)
	if (ent->client->weaponfireindex == 17)
	{
		ent->client->weapon_sound = 0;
		gi.sound(ent, CHAN_AUTO, gi.soundindex("weapons/chngnd1a.wav"), 1, ATTN_IDLE, 0);
	}
	else
	{
		ent->client->weapon_sound = gi.soundindex("weapons/chngnl1a.wav");
	}

	ent->client->anim_priority = ANIM_ATTACK;
	if (ent->client->ps.pMove.pmFlags & PMF_DUCKED)
	{
		ent->s.frame = FRAME_crattak1 - (level.framenum & 1);
		ent->client->anim_end = FRAME_crattak9;
	}
	else
	{
		ent->s.frame = FRAME_attack1 - (level.framenum & 1);
		ent->client->anim_end = FRAME_attack8;
	}

	//if (ent->client->ps.gunAnim <= 9)
	if (ent->client->weaponfireindex <= 4)
		shots = 1;
	//else if (ent->client->ps.gunAnim <= 14)
	else if (ent->client->weaponfireindex <= 9)
	{
		if (ent->client->buttons & BUTTON_ATTACK)
			shots = 2;
		else
			shots = 1;
	}
	else
		shots = 3;

	if (ent->client->pers.inventory[ent->client->ammo_index] < shots)
		shots = ent->client->pers.inventory[ent->client->ammo_index];

	if (!shots)
	{
		if (level.time >= ent->pain_debounce_time)
		{
			gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/noammo.wav"), 1, ATTN_NORM, 0);
			ent->pain_debounce_time = level.time + 1;
		}
		NoAmmoWeaponChange (ent);
		return;
	}

	if (is_quad)
	{
		damage *= 4;
		kick *= 4;
	}

	for (i=0 ; i<3 ; i++)
	{
		ent->client->kick_origin[i] = crandom() * 0.35;
		ent->client->kick_angles[i] = crandom() * 0.7;
	}

	ushort seed = 1+(rand() % USHRT_MAX-1);
	MTwister randGen (seed);

	for (i=0 ; i<shots ; i++)
	{
		// get start / end positions
		Angles_Vectors (ent->client->v_angle, forward, right, up);
		r = 7 + randGen.CRandom()*4;
		u = randGen.CRandom()*4;
		Vec3Set (offset, 0, r, u + ent->viewheight-8);
		P_ProjectSource (ent->client, ent->s.origin, offset, forward, right, start);

		fire_bullet (ent, start, forward, damage, kick, DEFAULT_BULLET_HSPREAD, DEFAULT_BULLET_VSPREAD, MOD_CHAINGUN, twister);
	}

	// send muzzle flash
	AddEntityEvent(ent, EV_MUZZLEFLASH, entityParms_t().PushByte((MZ_CHAINGUN1 + shots - 1) | is_silenced).PushVector(ent->client->v_angle).PushChar(ent->viewheight).PushByte(ent->client->pers.hand).PushShort(seed));

	PlayerNoise(ent, start, PNOISE_WEAPON);

	if (! ( (int)dmflags->floatVal & DF_INFINITE_AMMO ) )
		ent->client->pers.inventory[ent->client->ammo_index] -= shots;
}


void Weapon_Chaingun (edict_t *ent)
{
	//static int	pause_frames[]	= {38, 43, 51, 61, 0};
	//static int	fire_frames[]	= {5, 6, 7, 8, 9, 10, 11, 12, 13, 14 [10], 15, 16, 17, 18, 19, 20, 21, 0};

	//Weapon_Generic (ent, 4, 31, 61, 64, pause_frames, fire_frames, Chaingun_Fire);
	static int fire_frames[] = {0, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100, 1200, 1300, 1400, 1500, 1600, 1700, -1};
	Weapon_GenericTime (ent, 500, 2800, 400, fire_frames, Chaingun_Fire);
}


/*
======================================================================

SHOTGUN / SUPERSHOTGUN

======================================================================
*/

void weapon_shotgun_fire (edict_t *ent)
{
	vec3_t		start;
	vec3_t		forward, right;
	vec3_t		offset;
	int			damage = 4;
	int			kick = 8;

	Angles_Vectors (ent->client->v_angle, forward, right, NULL);

	Vec3Scale (forward, -2, ent->client->kick_origin);
	ent->client->kick_angles[0] = -2;

	Vec3Set (offset, 0, 8,  ent->viewheight-8);
	P_ProjectSource (ent->client, ent->s.origin, offset, forward, right, start);

	if (is_quad)
	{
		damage *= 4;
		kick *= 4;
	}

	ushort seed = 1+(rand() % USHRT_MAX-1);
	MTwister randGen (seed);

	if (deathmatch->floatVal)
		fire_shotgun (ent, start, forward, damage, kick, 500, 500, DEFAULT_DEATHMATCH_SHOTGUN_COUNT, MOD_SHOTGUN, twister);
	else
		fire_shotgun (ent, start, forward, damage, kick, 500, 500, DEFAULT_SHOTGUN_COUNT, MOD_SHOTGUN, twister);

	// send muzzle flash
	AddEntityEvent(ent, EV_MUZZLEFLASH, entityParms_t().PushByte(MZ_SHOTGUN | is_silenced).PushVector(ent->client->v_angle).PushChar(ent->viewheight).PushByte(ent->client->pers.hand).PushShort(seed));

	PlayerNoise(ent, start, PNOISE_WEAPON);

	if (! ( (int)dmflags->floatVal & DF_INFINITE_AMMO ) )
		ent->client->pers.inventory[ent->client->ammo_index]--;
}

void Weapon_Shotgun (edict_t *ent)
{
	//static int	pause_frames[]	= {22, 28, 34, 0};
	static int	fire_frames[]	= {0, -1};

	//Weapon_Generic (ent, 7, 18, 36, 39, pause_frames, fire_frames, weapon_shotgun_fire);
	Weapon_GenericTime(ent, 900, 1200, 300, fire_frames, weapon_shotgun_fire);
}


void weapon_supershotgun_fire (edict_t *ent)
{
	vec3_t		start;
	vec3_t		forward, right;
	vec3_t		offset;
	vec3_t		v;
	int			damage = 6;
	int			kick = 12;

	Angles_Vectors (ent->client->v_angle, forward, right, NULL);

	Vec3Scale (forward, -2, ent->client->kick_origin);
	ent->client->kick_angles[0] = -2;

	Vec3Set (offset, 0, 8,  ent->viewheight-8);
	P_ProjectSource (ent->client, ent->s.origin, offset, forward, right, start);

	if (is_quad)
	{
		damage *= 4;
		kick *= 4;
	}

	ushort seed = 1+(rand() % USHRT_MAX-1);
	MTwister randGen (seed);

	v[PITCH] = ent->client->v_angle[PITCH];
	v[YAW]   = ent->client->v_angle[YAW] - 5;
	v[ROLL]  = ent->client->v_angle[ROLL];
	Angles_Vectors (v, forward, NULL, NULL);
	fire_shotgun (ent, start, forward, damage, kick, DEFAULT_SHOTGUN_HSPREAD, DEFAULT_SHOTGUN_VSPREAD, DEFAULT_SSHOTGUN_COUNT/2, MOD_SSHOTGUN, randGen);
	v[YAW]   = ent->client->v_angle[YAW] + 5;
	Angles_Vectors (v, forward, NULL, NULL);
	fire_shotgun (ent, start, forward, damage, kick, DEFAULT_SHOTGUN_HSPREAD, DEFAULT_SHOTGUN_VSPREAD, DEFAULT_SSHOTGUN_COUNT/2, MOD_SSHOTGUN, randGen);

	// send muzzle flash
	AddEntityEvent(ent, EV_MUZZLEFLASH, entityParms_t().PushByte(MZ_SSHOTGUN | is_silenced).PushVector(ent->client->v_angle).PushChar(ent->viewheight).PushByte(ent->client->pers.hand).PushShort(seed));

	PlayerNoise(ent, start, PNOISE_WEAPON);

	if (! ( (int)dmflags->floatVal & DF_INFINITE_AMMO ) )
		ent->client->pers.inventory[ent->client->ammo_index] -= 2;
}

void Weapon_SuperShotgun (edict_t *ent)
{
	//static int	pause_frames[]	= {29, 42, 57, 0};
	static int	fire_frames[]	= {0, -1};

	//Weapon_Generic (ent, 6, 17, 57, 61, pause_frames, fire_frames, weapon_supershotgun_fire);
	Weapon_GenericTime(ent, 700, 1200, 400, fire_frames, weapon_supershotgun_fire);
}



/*
======================================================================

RAILGUN

======================================================================
*/

void weapon_railgun_fire (edict_t *ent)
{
	vec3_t		start;
	vec3_t		forward, right;
	vec3_t		offset;
	int			damage;
	int			kick;

#ifdef INSTARAIL
	damage = 9999;
	kick = 400;
#else
	if (deathmatch->floatVal)
	{	// normal damage is too extreme in dm
		damage = 100;
		kick = 200;
	}
	else
	{
		damage = 150;
		kick = 250;
	}

	if (is_quad)
	{
		damage *= 4;
		kick *= 4;
	}
#endif

	Angles_Vectors (ent->client->v_angle, forward, right, NULL);

	Vec3Scale (forward, -3, ent->client->kick_origin);
	ent->client->kick_angles[0] = -3;

	Vec3Set (offset, 0, 7, ent->viewheight-8);
	P_ProjectSource (ent->client, ent->s.origin, offset, forward, right, start);
	fire_rail (ent, start, forward, damage, kick);

	// send muzzle flash
	AddEntityEvent(ent, EV_MUZZLEFLASH, entityParms_t().PushByte(MZ_RAILGUN | is_silenced).PushVector(ent->client->v_angle).PushChar(ent->viewheight).PushByte(ent->client->pers.hand));

	PlayerNoise(ent, start, PNOISE_WEAPON);

	if (! ( (int)dmflags->floatVal & DF_INFINITE_AMMO ) )
		ent->client->pers.inventory[ent->client->ammo_index]--;
}


void Weapon_Railgun (edict_t *ent)
{
	//static int	pause_frames[]	= {56, 0};
	//static int	fire_frames[]	= {4, 0};

	//Weapon_Generic (ent, 3, 18, 56, 61, pause_frames, fire_frames, weapon_railgun_fire);
	static int	fire_frames[]	= {0, -1};
	Weapon_GenericTime(ent, 400, 1600, 500, fire_frames, weapon_railgun_fire);
}


/*
======================================================================

BFG10K

======================================================================
*/

void weapon_bfg_fire (edict_t *ent)
{
	vec3_t	offset, start;
	vec3_t	forward, right;
	int		damage;
	float	damage_radius = 1000;

	if (deathmatch->floatVal)
		damage = 200;
	else
		damage = 500;

	if (ent->client->weaponfireindex == 0)
	{
		// send muzzle flash
		AddEntityEvent(ent, EV_MUZZLEFLASH, entityParms_t().PushByte(MZ_BFG | is_silenced));

		PlayerNoise(ent, start, PNOISE_WEAPON);
		return;
	}

	// cells can go down during windup (from power armor hits), so
	// check again and abort firing if we don't have enough now
	if (ent->client->pers.inventory[ent->client->ammo_index] < 50)
		return;

	if (is_quad)
		damage *= 4;

	Angles_Vectors (ent->client->v_angle, forward, right, NULL);

	Vec3Scale (forward, -2, ent->client->kick_origin);

	// make a big pitch kick with an inverse fall
	ent->client->v_dmg_pitch = -40;
	ent->client->v_dmg_roll = crandom()*8;
	ent->client->v_dmg_time = level.time + DAMAGE_TIME;

	Vec3Set (offset, 8, 8, ent->viewheight-8);
	P_ProjectSource (ent->client, ent->s.origin, offset, forward, right, start);
	fire_bfg (ent, start, forward, damage, 400, damage_radius);

	PlayerNoise(ent, start, PNOISE_WEAPON);

	if (! ( (int)dmflags->floatVal & DF_INFINITE_AMMO ) )
		ent->client->pers.inventory[ent->client->ammo_index] -= 50;
}

void Weapon_BFG (edict_t *ent)
{
	//static int	pause_frames[]	= {39, 45, 50, 55, 0};
	//static int	fire_frames[]	= {9, 17, 0};

	//Weapon_Generic (ent, 8, 32, 55, 58, pause_frames, fire_frames, weapon_bfg_fire);
	static int	fire_frames[]	= {0, 800, -1};
	Weapon_GenericTime(ent, 900, 2500, 400, fire_frames, weapon_bfg_fire);
}


//======================================================================


/*
=================
Think_Weapon

Called by ClientBeginServerFrame and ClientThink
=================
*/

struct weaponThinks_t
{
	void (*weaponFunc) (edict_t *entity);
}
weaponThinks[] = 
{
	Weapon_Blaster,
	Weapon_Shotgun,
	Weapon_SuperShotgun,
	Weapon_Machinegun,
	Weapon_Chaingun,
	Weapon_Grenade,
	Weapon_GrenadeLauncher,
	Weapon_RocketLauncher,
	Weapon_HyperBlaster,
	Weapon_Railgun,
	Weapon_BFG
};

void Think_Weapon (edict_t *ent)
{
	// if just died, put the weapon away
	if (ent->health < 1)
	{
		ent->client->newweapon = NULL;
		ChangeWeapon (ent);
	}

	// call active weapon think routine
	if (ent->client->pers.weapon)
	{
		is_quad = (bool)(ent->client->quad_framenum > level.framenum);
		if (ent->client->silencer_shots)
			is_silenced = MZ_SILENCED;
		else
			is_silenced = 0;

		weaponThinks[ITEM_INDEX(ent->client->pers.weapon)-Items::Blaster].weaponFunc(ent);
	}
}
