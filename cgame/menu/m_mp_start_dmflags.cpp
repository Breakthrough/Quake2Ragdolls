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
// m_mp_start_dmflags.c
//

#include "m_local.h"

/*
=============================================================================

	DMFLAGS MENU

=============================================================================
*/

struct m_dMFlagsMenu_t {
	// Menu items
	uiFrameWork_t		frameWork;

	uiImage_t			banner;
	uiAction_t			header;

	uiList_t			friendlyfire_toggle;
	uiList_t			falldmg_toggle;
	uiList_t			weapons_stay_toggle;
	uiList_t			instant_powerups_toggle;
	uiList_t			powerups_toggle;
	uiList_t			health_toggle;
	uiList_t			farthest_toggle;
	uiList_t			teamplay_toggle;
	uiList_t			samelevel_toggle;
	uiList_t			force_respawn_toggle;
	uiList_t			armor_toggle;
	uiList_t			allow_exit_toggle;
	uiList_t			infinite_ammo_toggle;
	uiList_t			fixed_fov_toggle;
	uiList_t			quad_drop_toggle;

	uiAction_t			rogue_header;

	uiList_t			rogue_no_mines_toggle;
	uiList_t			rogue_no_nukes_toggle;
	uiList_t			rogue_stack_double_toggle;
	uiList_t			rogue_no_spheres_toggle;

	uiAction_t			back_action;
};

static m_dMFlagsMenu_t	m_dMFlagsMenu;
static char		mDMFlagsStatusbar[128];

static void DMFlagCallback (void *self)
{
	uiList_t	*f = (uiList_t *) self;
	int			flags, bit = 0;

	flags = cgi.Cvar_GetIntegerValue ("dmflags");

	if (f == &m_dMFlagsMenu.friendlyfire_toggle) {
		if (f->curValue)
			flags &= ~DF_NO_FRIENDLY_FIRE;
		else
			flags |= DF_NO_FRIENDLY_FIRE;
		goto setvalue;
	}
	else if (f == &m_dMFlagsMenu.falldmg_toggle) {
		if (f->curValue)
			flags &= ~DF_NO_FALLING;
		else
			flags |= DF_NO_FALLING;
		goto setvalue;
	}
	else if (f == &m_dMFlagsMenu.weapons_stay_toggle)
		bit = DF_WEAPONS_STAY;
	else if (f == &m_dMFlagsMenu.instant_powerups_toggle)
		bit = DF_INSTANT_ITEMS;
	else if (f == &m_dMFlagsMenu.allow_exit_toggle)
		bit = DF_ALLOW_EXIT;
	else if (f == &m_dMFlagsMenu.powerups_toggle) {
		if (f->curValue)
			flags &= ~DF_NO_ITEMS;
		else
			flags |= DF_NO_ITEMS;
		goto setvalue;
	}
	else if (f == &m_dMFlagsMenu.health_toggle) {
		if (f->curValue)
			flags &= ~DF_NO_HEALTH;
		else
			flags |= DF_NO_HEALTH;
		goto setvalue;
	}
	else if (f == &m_dMFlagsMenu.farthest_toggle)
		bit = DF_SPAWN_FARTHEST;
	else if (f == &m_dMFlagsMenu.teamplay_toggle) {
		if (f->curValue == 1) {
			flags |=  DF_SKINTEAMS;
			flags &= ~DF_MODELTEAMS;
		}
		else if (f->curValue == 2) {
			flags |=  DF_MODELTEAMS;
			flags &= ~DF_SKINTEAMS;
		}
		else
			flags &= ~(DF_MODELTEAMS | DF_SKINTEAMS);

		goto setvalue;
	}
	else if (f == &m_dMFlagsMenu.samelevel_toggle)
		bit = DF_SAME_LEVEL;
	else if (f == &m_dMFlagsMenu.force_respawn_toggle)
		bit = DF_FORCE_RESPAWN;
	else if (f == &m_dMFlagsMenu.armor_toggle) {
		if (f->curValue)
			flags &= ~DF_NO_ARMOR;
		else
			flags |= DF_NO_ARMOR;
		goto setvalue;
	}
	else if (f == &m_dMFlagsMenu.infinite_ammo_toggle)
		bit = DF_INFINITE_AMMO;
	else if (f == &m_dMFlagsMenu.fixed_fov_toggle)
		bit = DF_FIXED_FOV;
	else if (f == &m_dMFlagsMenu.quad_drop_toggle)
		bit = DF_QUAD_DROP;
	else if (cg.currGameMod == GAME_MOD_ROGUE) {
		if (f == &m_dMFlagsMenu.rogue_no_mines_toggle)
			bit = DF_NO_MINES;
		else if (f == &m_dMFlagsMenu.rogue_no_nukes_toggle)
			bit = DF_NO_NUKES;
		else if (f == &m_dMFlagsMenu.rogue_stack_double_toggle)
			bit = DF_NO_STACK_DOUBLE;
		else if (f == &m_dMFlagsMenu.rogue_no_spheres_toggle)
			bit = DF_NO_SPHERES;
	}

	if (f) {
		if (f->curValue == 0)
			flags &= ~bit;
		else
			flags |= bit;
	}

setvalue:
	cgi.Cvar_SetValue ("dmflags", flags, false);

	Q_snprintfz (mDMFlagsStatusbar, sizeof(mDMFlagsStatusbar), "dmflags = %d", flags);
}


/*
=============
DMFlagsMenu_SetValues
=============
*/
static void DMFlagsMenu_SetValues ()
{
	int		dmflags = cgi.Cvar_GetIntegerValue ("dmflags");

	m_dMFlagsMenu.falldmg_toggle.curValue			= !(dmflags & DF_NO_FALLING);
	m_dMFlagsMenu.weapons_stay_toggle.curValue		= !!(dmflags & DF_WEAPONS_STAY);
	m_dMFlagsMenu.instant_powerups_toggle.curValue	= !!(dmflags & DF_INSTANT_ITEMS);
	m_dMFlagsMenu.powerups_toggle.curValue			= !(dmflags & DF_NO_ITEMS);
	m_dMFlagsMenu.health_toggle.curValue				= !(dmflags & DF_NO_HEALTH);
	m_dMFlagsMenu.armor_toggle.curValue				= !(dmflags & DF_NO_ARMOR);
	m_dMFlagsMenu.farthest_toggle.curValue			= !!(dmflags & DF_SPAWN_FARTHEST);
	m_dMFlagsMenu.samelevel_toggle.curValue			= !!(dmflags & DF_SAME_LEVEL);
	m_dMFlagsMenu.force_respawn_toggle.curValue		= !!(dmflags & DF_FORCE_RESPAWN);
	m_dMFlagsMenu.allow_exit_toggle.curValue			= !!(dmflags & DF_ALLOW_EXIT);
	m_dMFlagsMenu.infinite_ammo_toggle.curValue		= !!(dmflags & DF_INFINITE_AMMO);
	m_dMFlagsMenu.fixed_fov_toggle.curValue			= !!(dmflags & DF_FIXED_FOV);
	m_dMFlagsMenu.quad_drop_toggle.curValue			= !!(dmflags & DF_QUAD_DROP);
	m_dMFlagsMenu.friendlyfire_toggle.curValue		= !(dmflags & DF_NO_FRIENDLY_FIRE);

	if (cg.currGameMod == GAME_MOD_ROGUE) {
		m_dMFlagsMenu.rogue_no_mines_toggle.curValue		= !!(dmflags & DF_NO_MINES);
		m_dMFlagsMenu.rogue_no_nukes_toggle.curValue		= !!(dmflags & DF_NO_NUKES);
		m_dMFlagsMenu.rogue_stack_double_toggle.curValue	= !!(dmflags & DF_NO_STACK_DOUBLE);
		m_dMFlagsMenu.rogue_no_spheres_toggle.curValue	= !!(dmflags & DF_NO_SPHERES);
	}
}


/*
=============
DMFlagsMenu_Init
=============
*/
static void DMFlagsMenu_Init ()
{
	static char *yesno_names[] = {
		"no",
		"yes",
		0
	};
	static char *teamplay_names[] = {
		"disabled",
		"by skin",
		"by model",
		0
	};

	UI_StartFramework (&m_dMFlagsMenu.frameWork, FWF_CENTERHEIGHT);

	m_dMFlagsMenu.banner.generic.type		= UITYPE_IMAGE;
	m_dMFlagsMenu.banner.generic.flags		= UIF_NOSELECT|UIF_CENTERED;
	m_dMFlagsMenu.banner.generic.name		= NULL;
	m_dMFlagsMenu.banner.mat				= uiMedia.banners.startServer;

	m_dMFlagsMenu.header.generic.type		= UITYPE_ACTION;
	m_dMFlagsMenu.header.generic.flags		= UIF_NOSELECT|UIF_CENTERED|UIF_MEDIUM|UIF_SHADOW;
	m_dMFlagsMenu.header.generic.name		= "Deathmatch Flags";

	m_dMFlagsMenu.falldmg_toggle.generic.type		= UITYPE_SPINCONTROL;
	m_dMFlagsMenu.falldmg_toggle.generic.name		= "Falling damage";
	m_dMFlagsMenu.falldmg_toggle.generic.callBack	= DMFlagCallback;
	m_dMFlagsMenu.falldmg_toggle.itemNames			= yesno_names;

	m_dMFlagsMenu.weapons_stay_toggle.generic.type		= UITYPE_SPINCONTROL;
	m_dMFlagsMenu.weapons_stay_toggle.generic.name		= "Weapons stay";
	m_dMFlagsMenu.weapons_stay_toggle.generic.callBack	= DMFlagCallback;
	m_dMFlagsMenu.weapons_stay_toggle.itemNames			= yesno_names;

	m_dMFlagsMenu.instant_powerups_toggle.generic.type		= UITYPE_SPINCONTROL;
	m_dMFlagsMenu.instant_powerups_toggle.generic.name		= "Instant powerups";
	m_dMFlagsMenu.instant_powerups_toggle.generic.callBack	= DMFlagCallback;
	m_dMFlagsMenu.instant_powerups_toggle.itemNames			= yesno_names;

	m_dMFlagsMenu.powerups_toggle.generic.type		= UITYPE_SPINCONTROL;
	m_dMFlagsMenu.powerups_toggle.generic.name		= "Allow powerups";
	m_dMFlagsMenu.powerups_toggle.generic.callBack	= DMFlagCallback;
	m_dMFlagsMenu.powerups_toggle.itemNames			= yesno_names;

	m_dMFlagsMenu.health_toggle.generic.type		= UITYPE_SPINCONTROL;
	m_dMFlagsMenu.health_toggle.generic.callBack	= DMFlagCallback;
	m_dMFlagsMenu.health_toggle.generic.name		= "Allow health";
	m_dMFlagsMenu.health_toggle.itemNames			= yesno_names;

	m_dMFlagsMenu.armor_toggle.generic.type		= UITYPE_SPINCONTROL;
	m_dMFlagsMenu.armor_toggle.generic.name		= "Allow armor";
	m_dMFlagsMenu.armor_toggle.generic.callBack	= DMFlagCallback;
	m_dMFlagsMenu.armor_toggle.itemNames		= yesno_names;

	m_dMFlagsMenu.farthest_toggle.generic.type		= UITYPE_SPINCONTROL;
	m_dMFlagsMenu.farthest_toggle.generic.name		= "Spawn farthest";
	m_dMFlagsMenu.farthest_toggle.generic.callBack	= DMFlagCallback;
	m_dMFlagsMenu.farthest_toggle.itemNames			= yesno_names;

	m_dMFlagsMenu.samelevel_toggle.generic.type		= UITYPE_SPINCONTROL;
	m_dMFlagsMenu.samelevel_toggle.generic.name		= "Same map";
	m_dMFlagsMenu.samelevel_toggle.generic.callBack	= DMFlagCallback;
	m_dMFlagsMenu.samelevel_toggle.itemNames		= yesno_names;

	m_dMFlagsMenu.force_respawn_toggle.generic.type		= UITYPE_SPINCONTROL;
	m_dMFlagsMenu.force_respawn_toggle.generic.name		= "Force respawn";
	m_dMFlagsMenu.force_respawn_toggle.generic.callBack	= DMFlagCallback;
	m_dMFlagsMenu.force_respawn_toggle.itemNames		= yesno_names;

	m_dMFlagsMenu.teamplay_toggle.generic.type		= UITYPE_SPINCONTROL;
	m_dMFlagsMenu.teamplay_toggle.generic.name		= "Teamplay";
	m_dMFlagsMenu.teamplay_toggle.generic.callBack	= DMFlagCallback;
	m_dMFlagsMenu.teamplay_toggle.itemNames			= teamplay_names;

	m_dMFlagsMenu.allow_exit_toggle.generic.type		= UITYPE_SPINCONTROL;
	m_dMFlagsMenu.allow_exit_toggle.generic.name		= "Allow exit";
	m_dMFlagsMenu.allow_exit_toggle.generic.callBack	= DMFlagCallback;
	m_dMFlagsMenu.allow_exit_toggle.itemNames			= yesno_names;

	m_dMFlagsMenu.infinite_ammo_toggle.generic.type		= UITYPE_SPINCONTROL;
	m_dMFlagsMenu.infinite_ammo_toggle.generic.name		= "Infinite ammo";
	m_dMFlagsMenu.infinite_ammo_toggle.generic.callBack	= DMFlagCallback;
	m_dMFlagsMenu.infinite_ammo_toggle.itemNames		= yesno_names;

	m_dMFlagsMenu.fixed_fov_toggle.generic.type		= UITYPE_SPINCONTROL;
	m_dMFlagsMenu.fixed_fov_toggle.generic.name		= "Fixed FOV";
	m_dMFlagsMenu.fixed_fov_toggle.generic.callBack	= DMFlagCallback;
	m_dMFlagsMenu.fixed_fov_toggle.itemNames		= yesno_names;

	m_dMFlagsMenu.quad_drop_toggle.generic.type		= UITYPE_SPINCONTROL;
	m_dMFlagsMenu.quad_drop_toggle.generic.name		= "Quad drop";
	m_dMFlagsMenu.quad_drop_toggle.generic.callBack	= DMFlagCallback;
	m_dMFlagsMenu.quad_drop_toggle.itemNames		= yesno_names;

	m_dMFlagsMenu.friendlyfire_toggle.generic.type		= UITYPE_SPINCONTROL;
	m_dMFlagsMenu.friendlyfire_toggle.generic.name		= "Friendly fire";
	m_dMFlagsMenu.friendlyfire_toggle.generic.callBack	= DMFlagCallback;
	m_dMFlagsMenu.friendlyfire_toggle.itemNames			= yesno_names;

	if (cg.currGameMod == GAME_MOD_ROGUE) {
		m_dMFlagsMenu.rogue_header.generic.type		= UITYPE_ACTION;
		m_dMFlagsMenu.rogue_header.generic.flags	= UIF_NOSELECT|UIF_CENTERED|UIF_MEDIUM;
		m_dMFlagsMenu.rogue_header.generic.name		= "Rogue Flags";

		m_dMFlagsMenu.rogue_no_mines_toggle.generic.type		= UITYPE_SPINCONTROL;
		m_dMFlagsMenu.rogue_no_mines_toggle.generic.name		= "Remove mines";
		m_dMFlagsMenu.rogue_no_mines_toggle.generic.callBack	= DMFlagCallback;
		m_dMFlagsMenu.rogue_no_mines_toggle.itemNames			= yesno_names;

		m_dMFlagsMenu.rogue_no_nukes_toggle.generic.type		= UITYPE_SPINCONTROL;
		m_dMFlagsMenu.rogue_no_nukes_toggle.generic.name		= "Remove nukes";
		m_dMFlagsMenu.rogue_no_nukes_toggle.generic.callBack	= DMFlagCallback;
		m_dMFlagsMenu.rogue_no_nukes_toggle.itemNames			= yesno_names;

		m_dMFlagsMenu.rogue_stack_double_toggle.generic.type		= UITYPE_SPINCONTROL;
		m_dMFlagsMenu.rogue_stack_double_toggle.generic.name		= "2x/4x stacking off";
		m_dMFlagsMenu.rogue_stack_double_toggle.generic.callBack	= DMFlagCallback;
		m_dMFlagsMenu.rogue_stack_double_toggle.itemNames			= yesno_names;

		m_dMFlagsMenu.rogue_no_spheres_toggle.generic.type		= UITYPE_SPINCONTROL;
		m_dMFlagsMenu.rogue_no_spheres_toggle.generic.name		= "Remove spheres";
		m_dMFlagsMenu.rogue_no_spheres_toggle.generic.callBack	= DMFlagCallback;
		m_dMFlagsMenu.rogue_no_spheres_toggle.itemNames			= yesno_names;

	}

	m_dMFlagsMenu.back_action.generic.type			= UITYPE_ACTION;
	m_dMFlagsMenu.back_action.generic.flags			= UIF_CENTERED|UIF_LARGE|UIF_SHADOW;
	m_dMFlagsMenu.back_action.generic.name			= "< Back";
	m_dMFlagsMenu.back_action.generic.callBack		= Menu_Pop;
	m_dMFlagsMenu.back_action.generic.statusBar		= "Back a menu";

	DMFlagsMenu_SetValues ();

	UI_AddItem (&m_dMFlagsMenu.frameWork,		&m_dMFlagsMenu.banner);
	UI_AddItem (&m_dMFlagsMenu.frameWork,		&m_dMFlagsMenu.header);

	UI_AddItem (&m_dMFlagsMenu.frameWork,		&m_dMFlagsMenu.falldmg_toggle);
	UI_AddItem (&m_dMFlagsMenu.frameWork,		&m_dMFlagsMenu.weapons_stay_toggle);
	UI_AddItem (&m_dMFlagsMenu.frameWork,		&m_dMFlagsMenu.instant_powerups_toggle);
	UI_AddItem (&m_dMFlagsMenu.frameWork,		&m_dMFlagsMenu.powerups_toggle);
	UI_AddItem (&m_dMFlagsMenu.frameWork,		&m_dMFlagsMenu.health_toggle);
	UI_AddItem (&m_dMFlagsMenu.frameWork,		&m_dMFlagsMenu.armor_toggle);
	UI_AddItem (&m_dMFlagsMenu.frameWork,		&m_dMFlagsMenu.farthest_toggle);
	UI_AddItem (&m_dMFlagsMenu.frameWork,		&m_dMFlagsMenu.samelevel_toggle);
	UI_AddItem (&m_dMFlagsMenu.frameWork,		&m_dMFlagsMenu.force_respawn_toggle);
	UI_AddItem (&m_dMFlagsMenu.frameWork,		&m_dMFlagsMenu.teamplay_toggle);
	UI_AddItem (&m_dMFlagsMenu.frameWork,		&m_dMFlagsMenu.allow_exit_toggle);
	UI_AddItem (&m_dMFlagsMenu.frameWork,		&m_dMFlagsMenu.infinite_ammo_toggle);
	UI_AddItem (&m_dMFlagsMenu.frameWork,		&m_dMFlagsMenu.fixed_fov_toggle);
	UI_AddItem (&m_dMFlagsMenu.frameWork,		&m_dMFlagsMenu.quad_drop_toggle);
	UI_AddItem (&m_dMFlagsMenu.frameWork,		&m_dMFlagsMenu.friendlyfire_toggle);

	if (cg.currGameMod == GAME_MOD_ROGUE) {
		UI_AddItem (&m_dMFlagsMenu.frameWork,	&m_dMFlagsMenu.rogue_header);
		UI_AddItem (&m_dMFlagsMenu.frameWork,	&m_dMFlagsMenu.rogue_no_mines_toggle);
		UI_AddItem (&m_dMFlagsMenu.frameWork,	&m_dMFlagsMenu.rogue_no_nukes_toggle);
		UI_AddItem (&m_dMFlagsMenu.frameWork,	&m_dMFlagsMenu.rogue_stack_double_toggle);
		UI_AddItem (&m_dMFlagsMenu.frameWork,	&m_dMFlagsMenu.rogue_no_spheres_toggle);
	}

	UI_AddItem (&m_dMFlagsMenu.frameWork,	&m_dMFlagsMenu.back_action);

	// Set the original dmflags statusbar
	DMFlagCallback (0);

	m_dMFlagsMenu.frameWork.statusBar = mDMFlagsStatusbar;
	UI_FinishFramework (&m_dMFlagsMenu.frameWork, true);
}


/*
=============
DMFlagsMenu_Close
=============
*/
static struct sfx_t *DMFlagsMenu_Close ()
{
	return uiMedia.sounds.menuOut;
}


/*
=============
DMFlagsMenu_Draw
=============
*/
static void DMFlagsMenu_Draw ()
{
	float	y;

	// Initialize if necessary
	if (!m_dMFlagsMenu.frameWork.initialized)
		DMFlagsMenu_Init ();

	// Dynamically position
	m_dMFlagsMenu.frameWork.x			= cg.refConfig.vidWidth * 0.50f;
	m_dMFlagsMenu.frameWork.y			= 0;

	m_dMFlagsMenu.banner.generic.x		= 0;
	m_dMFlagsMenu.banner.generic.y		= 0;

	y = m_dMFlagsMenu.banner.height * UI_SCALE;

	m_dMFlagsMenu.header.generic.x					= 0;
	m_dMFlagsMenu.header.generic.y					= y += UIFT_SIZEINC;
	m_dMFlagsMenu.falldmg_toggle.generic.x			= 0;
	m_dMFlagsMenu.falldmg_toggle.generic.y			= y += UIFT_SIZEINC + UIFT_SIZEINCMED;
	m_dMFlagsMenu.weapons_stay_toggle.generic.x		= 0;
	m_dMFlagsMenu.weapons_stay_toggle.generic.y		= y += UIFT_SIZEINC;
	m_dMFlagsMenu.instant_powerups_toggle.generic.x	= 0;
	m_dMFlagsMenu.instant_powerups_toggle.generic.y	= y += UIFT_SIZEINC;
	m_dMFlagsMenu.powerups_toggle.generic.x			= 0;
	m_dMFlagsMenu.powerups_toggle.generic.y			= y += UIFT_SIZEINC;
	m_dMFlagsMenu.health_toggle.generic.x			= 0;
	m_dMFlagsMenu.health_toggle.generic.y			= y += UIFT_SIZEINC;
	m_dMFlagsMenu.armor_toggle.generic.x			= 0;
	m_dMFlagsMenu.armor_toggle.generic.y			= y += UIFT_SIZEINC;
	m_dMFlagsMenu.farthest_toggle.generic.x			= 0;
	m_dMFlagsMenu.farthest_toggle.generic.y			= y += UIFT_SIZEINC;
	m_dMFlagsMenu.samelevel_toggle.generic.x		= 0;
	m_dMFlagsMenu.samelevel_toggle.generic.y		= y += UIFT_SIZEINC;
	m_dMFlagsMenu.force_respawn_toggle.generic.x	= 0;
	m_dMFlagsMenu.force_respawn_toggle.generic.y	= y += UIFT_SIZEINC;
	m_dMFlagsMenu.teamplay_toggle.generic.x			= 0;
	m_dMFlagsMenu.teamplay_toggle.generic.y			= y += UIFT_SIZEINC;
	m_dMFlagsMenu.allow_exit_toggle.generic.x		= 0;
	m_dMFlagsMenu.allow_exit_toggle.generic.y		= y += UIFT_SIZEINC;
	m_dMFlagsMenu.infinite_ammo_toggle.generic.x	= 0;
	m_dMFlagsMenu.infinite_ammo_toggle.generic.y	= y += UIFT_SIZEINC;
	m_dMFlagsMenu.fixed_fov_toggle.generic.x		= 0;
	m_dMFlagsMenu.fixed_fov_toggle.generic.y		= y += UIFT_SIZEINC;
	m_dMFlagsMenu.quad_drop_toggle.generic.x		= 0;
	m_dMFlagsMenu.quad_drop_toggle.generic.y		= y += UIFT_SIZEINC;
	m_dMFlagsMenu.friendlyfire_toggle.generic.x		= 0;
	m_dMFlagsMenu.friendlyfire_toggle.generic.y		= y += UIFT_SIZEINC;

	if (cg.currGameMod == GAME_MOD_ROGUE) {
		m_dMFlagsMenu.rogue_header.generic.x				= 0;
		m_dMFlagsMenu.rogue_header.generic.y				= y += UIFT_SIZEINC;
		m_dMFlagsMenu.rogue_no_mines_toggle.generic.x		= 0;
		m_dMFlagsMenu.rogue_no_mines_toggle.generic.y		= y += UIFT_SIZEINC;
		m_dMFlagsMenu.rogue_no_nukes_toggle.generic.x		= 0;
		m_dMFlagsMenu.rogue_no_nukes_toggle.generic.y		= y += UIFT_SIZEINC;
		m_dMFlagsMenu.rogue_stack_double_toggle.generic.x	= 0;
		m_dMFlagsMenu.rogue_stack_double_toggle.generic.y	= y += UIFT_SIZEINC;
		m_dMFlagsMenu.rogue_no_spheres_toggle.generic.x		= 0;
		m_dMFlagsMenu.rogue_no_spheres_toggle.generic.y		= y += UIFT_SIZEINC;
	}

	m_dMFlagsMenu.back_action.generic.x				= 0;
	m_dMFlagsMenu.back_action.generic.y				= y += UIFT_SIZEINC + UIFT_SIZEINCLG;

	// Render
	UI_DrawInterface (&m_dMFlagsMenu.frameWork);
}


/*
=============
UI_DMFlagsMenu_f
=============
*/
void UI_DMFlagsMenu_f ()
{
	DMFlagsMenu_Init();
	M_PushMenu (&m_dMFlagsMenu.frameWork, DMFlagsMenu_Draw, DMFlagsMenu_Close, M_KeyHandler);
}
