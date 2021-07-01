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
// m_opts.c
//

#include "m_local.h"

/*
=======================================================================

	OPTIONS MENU

=======================================================================
*/

struct m_optionsMenu_t {
	// Menu items
	uiFrameWork_t	frameWork;

	uiImage_t		banner;

	uiAction_t		console_menu;
	uiAction_t		controls_menu;
	uiAction_t		crosshair_menu;
	uiAction_t		effects_menu;
	uiAction_t		hud_menu;
	uiAction_t		input_menu;
	uiAction_t		misc_menu;
	uiAction_t		screen_menu;
	uiAction_t		sound_menu;
	uiAction_t		view_menu;

	uiAction_t		back_action;
};

static m_optionsMenu_t	m_optionsMenu;

static void Controls_Menu (void *unused)
{
	UI_ControlsMenu_f ();
}

static void Effects_Menu (void *unused)
{
	UI_EffectsMenu_f ();
}

static void HUD_Menu (void *unused)
{
	UI_HUDMenu_f ();
}

static void Input_Menu (void *unused)
{
	UI_InputMenu_f ();
}

static void Misc_Menu (void *unused)
{
	UI_MiscMenu_f ();
}

static void Screen_Menu (void *unused)
{
	UI_ScreenMenu_f ();
}

static void Sound_Menu (void *unused)
{
	UI_SoundMenu_f ();
}


/*
=============
OptionsMenu_Init
=============
*/
static void OptionsMenu_Init ()
{
	UI_StartFramework (&m_optionsMenu.frameWork, FWF_CENTERHEIGHT);

	m_optionsMenu.banner.generic.type				= UITYPE_IMAGE;
	m_optionsMenu.banner.generic.flags				= UIF_NOSELECT|UIF_CENTERED;
	m_optionsMenu.banner.generic.name				= NULL;
	m_optionsMenu.banner.mat						= uiMedia.banners.options;

	m_optionsMenu.controls_menu.generic.type		= UITYPE_ACTION;
	m_optionsMenu.controls_menu.generic.flags		= UIF_CENTERED|UIF_LARGE|UIF_SHADOW;
	m_optionsMenu.controls_menu.generic.name		= "Controls";
	m_optionsMenu.controls_menu.generic.callBack	= Controls_Menu;
	m_optionsMenu.controls_menu.generic.statusBar	= "Opens the Controls menu";

	m_optionsMenu.effects_menu.generic.type			= UITYPE_ACTION;
	m_optionsMenu.effects_menu.generic.flags		= UIF_CENTERED|UIF_LARGE|UIF_SHADOW;
	m_optionsMenu.effects_menu.generic.name			= "Effects";
	m_optionsMenu.effects_menu.generic.callBack		= Effects_Menu;
	m_optionsMenu.effects_menu.generic.statusBar	= "Opens the Effects Settings menu";

	m_optionsMenu.hud_menu.generic.type				= UITYPE_ACTION;
	m_optionsMenu.hud_menu.generic.flags			= UIF_CENTERED|UIF_LARGE|UIF_SHADOW;
	m_optionsMenu.hud_menu.generic.name				= "HUD";
	m_optionsMenu.hud_menu.generic.callBack			= HUD_Menu;
	m_optionsMenu.hud_menu.generic.statusBar		= "Opens the HUD Settings menu";

	m_optionsMenu.input_menu.generic.type			= UITYPE_ACTION;
	m_optionsMenu.input_menu.generic.flags			= UIF_CENTERED|UIF_LARGE|UIF_SHADOW;
	m_optionsMenu.input_menu.generic.name			= "Input";
	m_optionsMenu.input_menu.generic.callBack		= Input_Menu;
	m_optionsMenu.input_menu.generic.statusBar		= "Opens the Input Settings menu";

	m_optionsMenu.misc_menu.generic.type			= UITYPE_ACTION;
	m_optionsMenu.misc_menu.generic.flags			= UIF_CENTERED|UIF_LARGE|UIF_SHADOW;
	m_optionsMenu.misc_menu.generic.name			= "Misc";
	m_optionsMenu.misc_menu.generic.callBack		= Misc_Menu;
	m_optionsMenu.misc_menu.generic.statusBar		= "Opens the Misc Settings menu";

	m_optionsMenu.screen_menu.generic.type			= UITYPE_ACTION;
	m_optionsMenu.screen_menu.generic.flags			= UIF_CENTERED|UIF_LARGE|UIF_SHADOW;
	m_optionsMenu.screen_menu.generic.name			= "Screen";
	m_optionsMenu.screen_menu.generic.callBack		= Screen_Menu;
	m_optionsMenu.screen_menu.generic.statusBar		= "Opens the Screen Settings menu";

	m_optionsMenu.sound_menu.generic.type			= UITYPE_ACTION;
	m_optionsMenu.sound_menu.generic.flags			= UIF_CENTERED|UIF_LARGE|UIF_SHADOW;
	m_optionsMenu.sound_menu.generic.name			= "Sound";
	m_optionsMenu.sound_menu.generic.callBack		= Sound_Menu;
	m_optionsMenu.sound_menu.generic.statusBar		= "Opens the Sound Settings menu";

	m_optionsMenu.back_action.generic.type			= UITYPE_ACTION;
	m_optionsMenu.back_action.generic.flags			= UIF_CENTERED|UIF_LARGE|UIF_SHADOW;
	m_optionsMenu.back_action.generic.name			= "< Back";
	m_optionsMenu.back_action.generic.callBack		= Menu_Pop;
	m_optionsMenu.back_action.generic.statusBar		= "Back a menu";

	UI_AddItem (&m_optionsMenu.frameWork,		&m_optionsMenu.banner);

	UI_AddItem (&m_optionsMenu.frameWork,		&m_optionsMenu.controls_menu);
	UI_AddItem (&m_optionsMenu.frameWork,		&m_optionsMenu.effects_menu);
	UI_AddItem (&m_optionsMenu.frameWork,		&m_optionsMenu.hud_menu);
	UI_AddItem (&m_optionsMenu.frameWork,		&m_optionsMenu.input_menu);
	UI_AddItem (&m_optionsMenu.frameWork,		&m_optionsMenu.misc_menu);
	UI_AddItem (&m_optionsMenu.frameWork,		&m_optionsMenu.screen_menu);
	UI_AddItem (&m_optionsMenu.frameWork,		&m_optionsMenu.sound_menu);

	UI_AddItem (&m_optionsMenu.frameWork,		&m_optionsMenu.back_action);

	UI_FinishFramework (&m_optionsMenu.frameWork, true);
}


/*
=============
OptionsMenu_Close
=============
*/
static struct sfx_t *OptionsMenu_Close ()
{
	return uiMedia.sounds.menuOut;
}


/*
=============
OptionsMenu_Draw
=============
*/
static void OptionsMenu_Draw ()
{
	float	y;

	// Initialize if necessary
	if (!m_optionsMenu.frameWork.initialized)
		OptionsMenu_Init ();

	// Dynamically position
	m_optionsMenu.frameWork.x			= cg.refConfig.vidWidth * 0.5f;
	m_optionsMenu.frameWork.y			= 0;

	m_optionsMenu.banner.generic.x			= 0;
	m_optionsMenu.banner.generic.y			= 0;

	y = m_optionsMenu.banner.height * UI_SCALE;

	m_optionsMenu.controls_menu.generic.x		= 0;
	m_optionsMenu.controls_menu.generic.y		= y += UIFT_SIZEINC;
	m_optionsMenu.effects_menu.generic.x		= 0;
	m_optionsMenu.effects_menu.generic.y		= y += UIFT_SIZEINCLG;
	m_optionsMenu.hud_menu.generic.x			= 0;
	m_optionsMenu.hud_menu.generic.y			= y += UIFT_SIZEINCLG;
	m_optionsMenu.input_menu.generic.x			= 0;
	m_optionsMenu.input_menu.generic.y			= y += UIFT_SIZEINCLG;
	m_optionsMenu.misc_menu.generic.x			= 0;
	m_optionsMenu.misc_menu.generic.y			= y += UIFT_SIZEINCLG;
	m_optionsMenu.screen_menu.generic.x			= 0;
	m_optionsMenu.screen_menu.generic.y			= y += UIFT_SIZEINCLG;
	m_optionsMenu.sound_menu.generic.x			= 0;
	m_optionsMenu.sound_menu.generic.y			= y += UIFT_SIZEINCLG;
	m_optionsMenu.back_action.generic.x			= 0;
	m_optionsMenu.back_action.generic.y			= y += (UIFT_SIZEINCLG*2);

	// Render
	UI_DrawInterface (&m_optionsMenu.frameWork);
}


/*
=============
UI_OptionsMenu_f
=============
*/
void UI_OptionsMenu_f ()
{
	OptionsMenu_Init ();
	M_PushMenu (&m_optionsMenu.frameWork, OptionsMenu_Draw, OptionsMenu_Close, M_KeyHandler);
}
