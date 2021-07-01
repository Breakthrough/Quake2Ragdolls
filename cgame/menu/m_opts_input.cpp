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
// m_opts_input.c
//

#include "m_local.h"

/*
=======================================================================

	INPUT MENU

=======================================================================
*/

struct m_inputMenu_t {
	// Menu items
	uiFrameWork_t	frameWork;

	uiImage_t		banner;
	uiAction_t		header;

	uiList_t		always_run_toggle;
	uiList_t		joystick_toggle;

	uiSlider_t		ui_sensitivity_slider;
	uiSlider_t		ui_sensitivity_amount;
	uiSlider_t		sensitivity_slider;
	uiSlider_t		sensitivity_amount;
	uiList_t		maccel_list;
	uiList_t		autosensitivity_toggle;
	uiList_t		invert_mouse_toggle;
	uiList_t		lookspring_toggle;
	uiList_t		lookstrafe_toggle;
	uiList_t		freelook_toggle;

	uiAction_t		back_action;
};

static m_inputMenu_t	m_inputMenu;

static void AlwaysRunFunc (void *unused)
{
	cgi.Cvar_SetValue ("cl_run", m_inputMenu.always_run_toggle.curValue, false);
}

static void JoystickFunc (void *unused)
{
	cgi.Cvar_SetValue ("in_joystick", m_inputMenu.joystick_toggle.curValue, false);
}

static void UISensFunc (void *unused)
{
	cgi.Cvar_SetValue ("ui_sensitivity", m_inputMenu.ui_sensitivity_slider.curValue / 2.0F, false);
	m_inputMenu.ui_sensitivity_amount.generic.name = cgi.Cvar_GetStringValue ("ui_sensitivity");
}

static void SensitivityFunc (void *unused)
{
	cgi.Cvar_SetValue ("sensitivity", m_inputMenu.sensitivity_slider.curValue / 2.0F, false);
	m_inputMenu.sensitivity_amount.generic.name = cgi.Cvar_GetStringValue ("sensitivity");
}

static void MouseAccelFunc (void *unused)
{
	cgi.Cvar_SetValue ("m_accel", m_inputMenu.maccel_list.curValue, false);
}

static void InvertMouseFunc (void *unused)
{
	cgi.Cvar_SetValue ("m_pitch", (cgi.Cvar_GetFloatValue ("m_pitch")) * -1, false);
	m_inputMenu.invert_mouse_toggle.curValue		= (!!(cgi.Cvar_GetFloatValue ("m_pitch") < 0));
}

static void AutoSensFunc (void *unused)
{
	cgi.Cvar_SetValue ("autosensitivity", m_inputMenu.autosensitivity_toggle.curValue, false);
}

static void LookspringFunc (void *unused)
{
	cgi.Cvar_SetValue ("lookspring", m_inputMenu.lookspring_toggle.curValue, false);
}

static void LookstrafeFunc (void *unused)
{
	cgi.Cvar_SetValue ("lookstrafe", m_inputMenu.lookstrafe_toggle.curValue, false);
}

static void FreeLookFunc (void *unused)
{
	cgi.Cvar_SetValue ("freelook", m_inputMenu.freelook_toggle.curValue, false);
}


/*
=============
InputMenu_SetValues
=============
*/
static void InputMenu_SetValues ()
{
	cgi.Cvar_SetValue ("cl_run",				clamp (cgi.Cvar_GetIntegerValue ("cl_run"), 0, 1), false);
	m_inputMenu.always_run_toggle.curValue		= cgi.Cvar_GetIntegerValue ("cl_run");

	cgi.Cvar_SetValue ("in_joystick",			clamp (cgi.Cvar_GetIntegerValue ("in_joystick"), 0, 1), false);
	m_inputMenu.joystick_toggle.curValue		= cgi.Cvar_GetIntegerValue ("in_joystick");

	m_inputMenu.ui_sensitivity_slider.curValue		= (cgi.Cvar_GetFloatValue ("ui_sensitivity")) * 2;
	m_inputMenu.ui_sensitivity_amount.generic.name = cgi.Cvar_GetStringValue ("ui_sensitivity");
	m_inputMenu.sensitivity_slider.curValue		= (cgi.Cvar_GetFloatValue ("sensitivity")) * 2;
	m_inputMenu.sensitivity_amount.generic.name	= cgi.Cvar_GetStringValue ("sensitivity");

	cgi.Cvar_SetValue ("m_accel",					clamp (cgi.Cvar_GetIntegerValue ("m_accel"), 0, 2), false);
	m_inputMenu.maccel_list.curValue				= cgi.Cvar_GetIntegerValue ("m_accel");

	m_inputMenu.invert_mouse_toggle.curValue		= (!!(cgi.Cvar_GetFloatValue ("m_pitch") < 0));

	cgi.Cvar_SetValue ("autosensitivity",			clamp (cgi.Cvar_GetIntegerValue ("autosensitivity"), 0, 1), false);
	m_inputMenu.autosensitivity_toggle.curValue	= cgi.Cvar_GetIntegerValue ("autosensitivity");

	cgi.Cvar_SetValue ("lookspring",			clamp (cgi.Cvar_GetIntegerValue ("lookspring"), 0, 1), false);
	m_inputMenu.lookspring_toggle.curValue		= cgi.Cvar_GetIntegerValue ("lookspring");

	cgi.Cvar_SetValue ("lookstrafe",			clamp (cgi.Cvar_GetIntegerValue ("lookstrafe"), 0, 1), false);
	m_inputMenu.lookstrafe_toggle.curValue		= cgi.Cvar_GetIntegerValue ("lookstrafe");

	cgi.Cvar_SetValue ("freelook",				clamp (cgi.Cvar_GetIntegerValue ("freelook"), 0, 1), false);
	m_inputMenu.freelook_toggle.curValue		= cgi.Cvar_GetIntegerValue ("freelook");
}


/*
=============
InputMenu_Init
=============
*/
static void InputMenu_Init ()
{
	static char *yesno_names[] = {
		"no",
		"yes",
		0
	};

	static char *maccel_items[] = {
		"no accel",
		"normal",
		"os values",
		0
	};

	static char *onoff_names[] = {
		"off",
		"on",
		0
	};

	UI_StartFramework (&m_inputMenu.frameWork, FWF_CENTERHEIGHT);

	m_inputMenu.banner.generic.type		= UITYPE_IMAGE;
	m_inputMenu.banner.generic.flags	= UIF_NOSELECT|UIF_CENTERED;
	m_inputMenu.banner.generic.name		= NULL;
	m_inputMenu.banner.mat				= uiMedia.banners.options;

	m_inputMenu.header.generic.type		= UITYPE_ACTION;
	m_inputMenu.header.generic.flags	= UIF_NOSELECT|UIF_CENTERED|UIF_MEDIUM|UIF_SHADOW;
	m_inputMenu.header.generic.name		= "Input Settings";

	m_inputMenu.always_run_toggle.generic.type		= UITYPE_SPINCONTROL;
	m_inputMenu.always_run_toggle.generic.name		= "Always run";
	m_inputMenu.always_run_toggle.generic.callBack	= AlwaysRunFunc;
	m_inputMenu.always_run_toggle.itemNames			= yesno_names;
	m_inputMenu.always_run_toggle.generic.statusBar	= "Always Run";

	m_inputMenu.joystick_toggle.generic.type		= UITYPE_SPINCONTROL;
	m_inputMenu.joystick_toggle.generic.name		= "Use joystick";
	m_inputMenu.joystick_toggle.generic.callBack	= JoystickFunc;
	m_inputMenu.joystick_toggle.itemNames			= yesno_names;
	m_inputMenu.joystick_toggle.generic.statusBar	= "Use Joystick";

	m_inputMenu.ui_sensitivity_slider.generic.type		= UITYPE_SLIDER;
	m_inputMenu.ui_sensitivity_slider.generic.name		= "UI speed";
	m_inputMenu.ui_sensitivity_slider.generic.callBack	= UISensFunc;
	m_inputMenu.ui_sensitivity_slider.minValue			= 1;
	m_inputMenu.ui_sensitivity_slider.maxValue			= 10;
	m_inputMenu.ui_sensitivity_slider.generic.statusBar	= "Menu mouse cursor sensitivity";
	m_inputMenu.ui_sensitivity_amount.generic.type		= UITYPE_ACTION;
	m_inputMenu.ui_sensitivity_amount.generic.flags		= UIF_LEFT_JUSTIFY|UIF_NOSELECT;

	m_inputMenu.sensitivity_slider.generic.type			= UITYPE_SLIDER;
	m_inputMenu.sensitivity_slider.generic.name			= "Mouse speed";
	m_inputMenu.sensitivity_slider.generic.callBack		= SensitivityFunc;
	m_inputMenu.sensitivity_slider.minValue				= 2;
	m_inputMenu.sensitivity_slider.maxValue				= 52;
	m_inputMenu.sensitivity_slider.generic.statusBar	= "Mouse Sensitivity";
	m_inputMenu.sensitivity_amount.generic.type			= UITYPE_ACTION;
	m_inputMenu.sensitivity_amount.generic.flags		= UIF_LEFT_JUSTIFY|UIF_NOSELECT;

	m_inputMenu.maccel_list.generic.type		= UITYPE_SPINCONTROL;
	m_inputMenu.maccel_list.generic.name		= "Mouse accel";
	m_inputMenu.maccel_list.generic.callBack	= MouseAccelFunc;
	m_inputMenu.maccel_list.itemNames			= maccel_items;
	m_inputMenu.maccel_list.generic.statusBar	= "Mouse Acceleration options";

	m_inputMenu.autosensitivity_toggle.generic.type			= UITYPE_SPINCONTROL;
	m_inputMenu.autosensitivity_toggle.generic.name			= "Auto sensitivity";
	m_inputMenu.autosensitivity_toggle.generic.callBack		= AutoSensFunc;
	m_inputMenu.autosensitivity_toggle.itemNames			= yesno_names;
	m_inputMenu.autosensitivity_toggle.generic.statusBar	= "FOV auto-affects mouse sensitivity";

	m_inputMenu.invert_mouse_toggle.generic.type		= UITYPE_SPINCONTROL;
	m_inputMenu.invert_mouse_toggle.generic.name		= "Invert mouse";
	m_inputMenu.invert_mouse_toggle.generic.callBack	= InvertMouseFunc;
	m_inputMenu.invert_mouse_toggle.itemNames			= yesno_names;
	m_inputMenu.invert_mouse_toggle.generic.statusBar	= "Invert Mouse";

	m_inputMenu.lookspring_toggle.generic.type		= UITYPE_SPINCONTROL;
	m_inputMenu.lookspring_toggle.generic.name		= "Lookspring";
	m_inputMenu.lookspring_toggle.generic.callBack	= LookspringFunc;
	m_inputMenu.lookspring_toggle.itemNames			= onoff_names;
	m_inputMenu.lookspring_toggle.generic.statusBar	= "Lookspring";

	m_inputMenu.lookstrafe_toggle.generic.type		= UITYPE_SPINCONTROL;
	m_inputMenu.lookstrafe_toggle.generic.name		= "Lookstrafe";
	m_inputMenu.lookstrafe_toggle.generic.callBack	= LookstrafeFunc;
	m_inputMenu.lookstrafe_toggle.itemNames			= onoff_names;
	m_inputMenu.lookstrafe_toggle.generic.statusBar	= "Lookstrafe";

	m_inputMenu.freelook_toggle.generic.type		= UITYPE_SPINCONTROL;
	m_inputMenu.freelook_toggle.generic.name		= "Free look";
	m_inputMenu.freelook_toggle.generic.callBack	= FreeLookFunc;
	m_inputMenu.freelook_toggle.itemNames			= onoff_names;
	m_inputMenu.freelook_toggle.generic.statusBar	= "Free Look";

	m_inputMenu.back_action.generic.type		= UITYPE_ACTION;
	m_inputMenu.back_action.generic.flags		= UIF_CENTERED|UIF_LARGE|UIF_SHADOW;
	m_inputMenu.back_action.generic.name		= "< Back";
	m_inputMenu.back_action.generic.callBack	= Menu_Pop;
	m_inputMenu.back_action.generic.statusBar	= "Back a menu";

	InputMenu_SetValues ();

	UI_AddItem (&m_inputMenu.frameWork,		&m_inputMenu.banner);
	UI_AddItem (&m_inputMenu.frameWork,		&m_inputMenu.header);

	UI_AddItem (&m_inputMenu.frameWork,		&m_inputMenu.always_run_toggle);
	UI_AddItem (&m_inputMenu.frameWork,		&m_inputMenu.joystick_toggle);

	UI_AddItem (&m_inputMenu.frameWork,		&m_inputMenu.ui_sensitivity_slider);
	UI_AddItem (&m_inputMenu.frameWork,		&m_inputMenu.ui_sensitivity_amount);
	UI_AddItem (&m_inputMenu.frameWork,		&m_inputMenu.sensitivity_slider);
	UI_AddItem (&m_inputMenu.frameWork,		&m_inputMenu.sensitivity_amount);
	UI_AddItem (&m_inputMenu.frameWork,		&m_inputMenu.maccel_list);
	UI_AddItem (&m_inputMenu.frameWork,		&m_inputMenu.autosensitivity_toggle);
	UI_AddItem (&m_inputMenu.frameWork,		&m_inputMenu.invert_mouse_toggle);

	UI_AddItem (&m_inputMenu.frameWork,		&m_inputMenu.lookspring_toggle);
	UI_AddItem (&m_inputMenu.frameWork,		&m_inputMenu.lookstrafe_toggle);
	UI_AddItem (&m_inputMenu.frameWork,		&m_inputMenu.freelook_toggle);

	UI_AddItem (&m_inputMenu.frameWork,		&m_inputMenu.back_action);

	UI_FinishFramework (&m_inputMenu.frameWork, true);
}


/*
=============
InputMenu_Close
=============
*/
static struct sfx_t *InputMenu_Close ()
{
	return uiMedia.sounds.menuOut;
}


/*
=============
InputMenu_Draw
=============
*/
static void InputMenu_Draw ()
{
	float	y;

	// Initialize if necessary
	if (!m_inputMenu.frameWork.initialized)
		InputMenu_Init ();

	// Dynamically position
	m_inputMenu.frameWork.x		= cg.refConfig.vidWidth * 0.5f;
	m_inputMenu.frameWork.y		= 0;

	m_inputMenu.banner.generic.x			= 0;
	m_inputMenu.banner.generic.y			= 0;

	y = m_inputMenu.banner.height * UI_SCALE;

	m_inputMenu.header.generic.x					= 0;
	m_inputMenu.header.generic.y					= y += UIFT_SIZEINC;
	m_inputMenu.always_run_toggle.generic.x			= 0;
	m_inputMenu.always_run_toggle.generic.y			= y += UIFT_SIZEINC + UIFT_SIZEINCMED;
	m_inputMenu.joystick_toggle.generic.x			= 0;
	m_inputMenu.joystick_toggle.generic.y			= y += UIFT_SIZEINC;
	m_inputMenu.ui_sensitivity_slider.generic.x		= 0;
	m_inputMenu.ui_sensitivity_slider.generic.y		= y += (UIFT_SIZEINC*2);
	m_inputMenu.ui_sensitivity_amount.generic.x		= (UIFT_SIZE * (SLIDER_RANGE + 5));
	m_inputMenu.ui_sensitivity_amount.generic.y		= y;
	m_inputMenu.sensitivity_slider.generic.x		= 0;
	m_inputMenu.sensitivity_slider.generic.y		= y += UIFT_SIZEINC;
	m_inputMenu.sensitivity_amount.generic.x		= (UIFT_SIZE * (SLIDER_RANGE + 5));
	m_inputMenu.sensitivity_amount.generic.y		= y;
	m_inputMenu.maccel_list.generic.x				= 0;
	m_inputMenu.maccel_list.generic.y				= y += UIFT_SIZEINC;
	m_inputMenu.autosensitivity_toggle.generic.x	= 0;
	m_inputMenu.autosensitivity_toggle.generic.y	= y += UIFT_SIZEINC;
	m_inputMenu.invert_mouse_toggle.generic.x		= 0;
	m_inputMenu.invert_mouse_toggle.generic.y		= y += UIFT_SIZEINC;
	m_inputMenu.lookspring_toggle.generic.x			= 0;
	m_inputMenu.lookspring_toggle.generic.y			= y += (UIFT_SIZEINC*2);
	m_inputMenu.lookstrafe_toggle.generic.x			= 0;
	m_inputMenu.lookstrafe_toggle.generic.y			= y += UIFT_SIZEINC;
	m_inputMenu.freelook_toggle.generic.x			= 0;
	m_inputMenu.freelook_toggle.generic.y			= y += UIFT_SIZEINC;
	m_inputMenu.back_action.generic.x				= 0;
	m_inputMenu.back_action.generic.y				= y += UIFT_SIZEINC + UIFT_SIZEINCLG;

	// Render
	UI_DrawInterface (&m_inputMenu.frameWork);
}


/*
=============
UI_InputMenu_f
=============
*/
void UI_InputMenu_f ()
{
	InputMenu_Init ();
	M_PushMenu (&m_inputMenu.frameWork, InputMenu_Draw, InputMenu_Close, M_KeyHandler);
}
