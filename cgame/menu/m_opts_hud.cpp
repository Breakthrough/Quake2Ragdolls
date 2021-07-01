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
// m_opts_hud.c
//

#include "m_local.h"

/*
=======================================================================

	HUD MENU

=======================================================================
*/

struct m_hudMenu_t {
	// Menu items
	uiFrameWork_t	frameWork;

	uiImage_t		banner;
	uiAction_t		header;

	uiList_t		showfps_toggle;
	uiList_t		showping_toggle;
	uiList_t		showtime_toggle;

	uiSlider_t		hudscale_slider;
	uiAction_t		hudscale_amount;
	uiSlider_t		hudalpha_slider;
	uiAction_t		hudalpha_amount;

	uiList_t		netgraph_toggle;
	uiSlider_t		netgraph_alpha_slider;
	uiAction_t		netgraph_alpha_amount;

	uiAction_t		back_action;
};

static m_hudMenu_t	m_hudMenu;

static void ShowFPSFunc (void *unused)
{
	cgi.Cvar_SetValue ("cl_showfps", m_hudMenu.showfps_toggle.curValue, false);
}

static void ShowPINGFunc (void *unused)
{
	cgi.Cvar_SetValue ("cl_showping", m_hudMenu.showping_toggle.curValue, false);
}

static void ShowTIMEFunc (void *unused)
{
	cgi.Cvar_SetValue ("cl_showtime", m_hudMenu.showtime_toggle.curValue, false);
}

static void HudScaleFunc (void *unused)
{
	cgi.Cvar_SetValue ("r_hudScale", m_hudMenu.hudscale_slider.curValue * 0.25, false);
	m_hudMenu.hudscale_amount.generic.name = cgi.Cvar_GetStringValue ("r_hudScale");
}

static void HudAlphaFunc (void *unused)
{
	cgi.Cvar_SetValue ("scr_hudalpha", m_hudMenu.hudalpha_slider.curValue * 0.1, false);
	m_hudMenu.hudalpha_amount.generic.name = cgi.Cvar_GetStringValue ("scr_hudalpha");
}

static void NetgraphToggleFunc (void *unused)
{
	cgi.Cvar_SetValue ("netgraph", m_hudMenu.netgraph_toggle.curValue, false);
}

static void NetgraphAlphaFunc (void *unused)
{
	cgi.Cvar_SetValue ("scr_graphalpha", m_hudMenu.netgraph_alpha_slider.curValue * 0.1, false);
	m_hudMenu.netgraph_alpha_amount.generic.name = cgi.Cvar_GetStringValue ("scr_graphalpha");
}


/*
=============
HUDMenu_SetValues
=============
*/
static void HUDMenu_SetValues ()
{
	cgi.Cvar_SetValue ("cl_showfps",			clamp (cgi.Cvar_GetIntegerValue ("cl_showfps"), 0, 1), false);
	m_hudMenu.showfps_toggle.curValue			= cgi.Cvar_GetIntegerValue ("cl_showfps");

	cgi.Cvar_SetValue ("cl_showping",			clamp (cgi.Cvar_GetIntegerValue ("cl_showping"), 0, 1), false);
	m_hudMenu.showping_toggle.curValue			= cgi.Cvar_GetIntegerValue ("cl_showping");

	cgi.Cvar_SetValue ("cl_showtime",			clamp (cgi.Cvar_GetIntegerValue ("cl_showtime"), 0, 1), false);
	m_hudMenu.showtime_toggle.curValue			= cgi.Cvar_GetIntegerValue ("cl_showtime");

	m_hudMenu.hudscale_slider.curValue			= cgi.Cvar_GetFloatValue ("r_hudScale") * 4;
	m_hudMenu.hudscale_amount.generic.name		= cgi.Cvar_GetStringValue ("r_hudScale");

	m_hudMenu.hudalpha_slider.curValue			= cgi.Cvar_GetFloatValue ("scr_hudalpha") * 10;
	m_hudMenu.hudalpha_amount.generic.name		= cgi.Cvar_GetStringValue ("scr_hudalpha");

	cgi.Cvar_SetValue ("netgraph",				clamp (cgi.Cvar_GetIntegerValue ("netgraph"), 0, 1), false);
	m_hudMenu.netgraph_toggle.curValue			= cgi.Cvar_GetIntegerValue ("netgraph");

	m_hudMenu.netgraph_alpha_slider.curValue		= cgi.Cvar_GetFloatValue ("scr_graphalpha") * 10;
	m_hudMenu.netgraph_alpha_amount.generic.name	= cgi.Cvar_GetStringValue ("scr_graphalpha");
}


/*
=============
HUDMenu_Init
=============
*/
static void HUDMenu_Init ()
{
	static char *onoff_names[] = {
		"off",
		"on",
		0
	};

	UI_StartFramework (&m_hudMenu.frameWork, FWF_CENTERHEIGHT);

	m_hudMenu.banner.generic.type		= UITYPE_IMAGE;
	m_hudMenu.banner.generic.flags		= UIF_NOSELECT|UIF_CENTERED;
	m_hudMenu.banner.generic.name		= NULL;
	m_hudMenu.banner.mat				= uiMedia.banners.options;

	m_hudMenu.header.generic.type			= UITYPE_ACTION;
	m_hudMenu.header.generic.flags			= UIF_NOSELECT|UIF_CENTERED|UIF_MEDIUM|UIF_SHADOW;
	m_hudMenu.header.generic.name			= "HUD Settings";

	m_hudMenu.showfps_toggle.generic.type		= UITYPE_SPINCONTROL;
	m_hudMenu.showfps_toggle.generic.name		= "Framerate display";
	m_hudMenu.showfps_toggle.generic.callBack	= ShowFPSFunc;
	m_hudMenu.showfps_toggle.itemNames			= onoff_names;
	m_hudMenu.showfps_toggle.generic.statusBar	= "Framerate Display";

	m_hudMenu.showping_toggle.generic.type			= UITYPE_SPINCONTROL;
	m_hudMenu.showping_toggle.generic.name			= "Ping display";
	m_hudMenu.showping_toggle.generic.callBack		= ShowPINGFunc;
	m_hudMenu.showping_toggle.itemNames				= onoff_names;
	m_hudMenu.showping_toggle.generic.statusBar		= "Ping Display";

	m_hudMenu.showtime_toggle.generic.type			= UITYPE_SPINCONTROL;
	m_hudMenu.showtime_toggle.generic.name			= "Map timer";
	m_hudMenu.showtime_toggle.generic.callBack		= ShowTIMEFunc;
	m_hudMenu.showtime_toggle.itemNames				= onoff_names;
	m_hudMenu.showtime_toggle.generic.statusBar		= "Map Timer";

	m_hudMenu.hudscale_slider.generic.type			= UITYPE_SLIDER;
	m_hudMenu.hudscale_slider.generic.name			= "HUD scale";
	m_hudMenu.hudscale_slider.generic.callBack		= HudScaleFunc;
	m_hudMenu.hudscale_slider.minValue				= 1;
	m_hudMenu.hudscale_slider.maxValue				= 12;
	m_hudMenu.hudscale_slider.generic.statusBar		= "HUD font size scaling";
	m_hudMenu.hudscale_amount.generic.type			= UITYPE_ACTION;
	m_hudMenu.hudscale_amount.generic.flags			= UIF_LEFT_JUSTIFY|UIF_NOSELECT;

	m_hudMenu.hudalpha_slider.generic.type			= UITYPE_SLIDER;
	m_hudMenu.hudalpha_slider.generic.name			= "HUD alpha";
	m_hudMenu.hudalpha_slider.generic.callBack		= HudAlphaFunc;
	m_hudMenu.hudalpha_slider.minValue				= 0;
	m_hudMenu.hudalpha_slider.maxValue				= 10;
	m_hudMenu.hudalpha_slider.generic.statusBar		= "HUD alpha transparency";
	m_hudMenu.hudalpha_amount.generic.type			= UITYPE_ACTION;
	m_hudMenu.hudalpha_amount.generic.flags			= UIF_LEFT_JUSTIFY|UIF_NOSELECT;

	m_hudMenu.netgraph_toggle.generic.type			= UITYPE_SPINCONTROL;
	m_hudMenu.netgraph_toggle.generic.name			= "Netgraph";
	m_hudMenu.netgraph_toggle.generic.callBack		= NetgraphToggleFunc;
	m_hudMenu.netgraph_toggle.itemNames				= onoff_names;
	m_hudMenu.netgraph_toggle.generic.statusBar		= "Toggles showing the lag netgraph";

	m_hudMenu.netgraph_alpha_slider.generic.type		= UITYPE_SLIDER;
	m_hudMenu.netgraph_alpha_slider.generic.name		= "Netgraph alpha";
	m_hudMenu.netgraph_alpha_slider.generic.callBack	= NetgraphAlphaFunc;
	m_hudMenu.netgraph_alpha_slider.minValue			= 0;
	m_hudMenu.netgraph_alpha_slider.maxValue			= 10;
	m_hudMenu.netgraph_alpha_slider.generic.statusBar	= "Netgraph alpha transparency";
	m_hudMenu.netgraph_alpha_amount.generic.type		= UITYPE_ACTION;
	m_hudMenu.netgraph_alpha_amount.generic.flags		= UIF_LEFT_JUSTIFY|UIF_NOSELECT;

	m_hudMenu.back_action.generic.type			= UITYPE_ACTION;
	m_hudMenu.back_action.generic.flags			= UIF_CENTERED|UIF_LARGE|UIF_SHADOW;
	m_hudMenu.back_action.generic.name			= "< Back";
	m_hudMenu.back_action.generic.callBack		= Menu_Pop;
	m_hudMenu.back_action.generic.statusBar		= "Back a menu";

	HUDMenu_SetValues ();

	UI_AddItem (&m_hudMenu.frameWork,			&m_hudMenu.banner);
	UI_AddItem (&m_hudMenu.frameWork,			&m_hudMenu.header);

	UI_AddItem (&m_hudMenu.frameWork,			&m_hudMenu.showfps_toggle);
	UI_AddItem (&m_hudMenu.frameWork,			&m_hudMenu.showping_toggle);
	UI_AddItem (&m_hudMenu.frameWork,			&m_hudMenu.showtime_toggle);

	UI_AddItem (&m_hudMenu.frameWork,			&m_hudMenu.hudscale_slider);
	UI_AddItem (&m_hudMenu.frameWork,			&m_hudMenu.hudscale_amount);
	UI_AddItem (&m_hudMenu.frameWork,			&m_hudMenu.hudalpha_slider);
	UI_AddItem (&m_hudMenu.frameWork,			&m_hudMenu.hudalpha_amount);

	UI_AddItem (&m_hudMenu.frameWork,			&m_hudMenu.netgraph_toggle);
	UI_AddItem (&m_hudMenu.frameWork,			&m_hudMenu.netgraph_alpha_slider);
	UI_AddItem (&m_hudMenu.frameWork,			&m_hudMenu.netgraph_alpha_amount);

	UI_AddItem (&m_hudMenu.frameWork,			&m_hudMenu.back_action);

	UI_FinishFramework (&m_hudMenu.frameWork, true);
}


/*
=============
HUDMenu_Close
=============
*/
static struct sfx_t *HUDMenu_Close ()
{
	return uiMedia.sounds.menuOut;
}


/*
=============
HUDMenu_Draw
=============
*/
static void HUDMenu_Draw ()
{
	float	y;

	// Initialize if necessary
	if (!m_hudMenu.frameWork.initialized)
		HUDMenu_Init ();

	// Dynamically position
	m_hudMenu.frameWork.x			= cg.refConfig.vidWidth * 0.5;
	m_hudMenu.frameWork.y			= 0;

	m_hudMenu.banner.generic.x			= 0;
	m_hudMenu.banner.generic.y			= 0;

	y = m_hudMenu.banner.height * UI_SCALE;

	m_hudMenu.header.generic.x					= 0;
	m_hudMenu.header.generic.y					= y += UIFT_SIZEINC;
	m_hudMenu.showfps_toggle.generic.x			= 0;
	m_hudMenu.showfps_toggle.generic.y			= y += UIFT_SIZEINC + UIFT_SIZEINCMED;
	m_hudMenu.showping_toggle.generic.x			= 0;
	m_hudMenu.showping_toggle.generic.y			= y += UIFT_SIZEINC;
	m_hudMenu.showtime_toggle.generic.x			= 0;
	m_hudMenu.showtime_toggle.generic.y			= y += UIFT_SIZEINC;
	m_hudMenu.hudscale_slider.generic.x			= 0;
	m_hudMenu.hudscale_slider.generic.y			= y += (UIFT_SIZEINC*2);
	m_hudMenu.hudscale_amount.generic.x			= (UIFT_SIZE * (SLIDER_RANGE + 5));
	m_hudMenu.hudscale_amount.generic.y			= y;
	m_hudMenu.hudalpha_slider.generic.x			= 0;
	m_hudMenu.hudalpha_slider.generic.y			= y += UIFT_SIZEINC;
	m_hudMenu.hudalpha_amount.generic.x			= (UIFT_SIZE * (SLIDER_RANGE + 5));
	m_hudMenu.hudalpha_amount.generic.y			= y;
	m_hudMenu.netgraph_toggle.generic.x			= 0;
	m_hudMenu.netgraph_toggle.generic.y			= y += (UIFT_SIZEINC*2);
	m_hudMenu.netgraph_alpha_slider.generic.x	= 0;
	m_hudMenu.netgraph_alpha_slider.generic.y	= y += UIFT_SIZEINC;
	m_hudMenu.netgraph_alpha_amount.generic.x	= (UIFT_SIZE * (SLIDER_RANGE + 5));
	m_hudMenu.netgraph_alpha_amount.generic.y	= y;
	m_hudMenu.back_action.generic.x				= 0;
	m_hudMenu.back_action.generic.y				= y += UIFT_SIZEINC + UIFT_SIZEINCLG;

	// Render
	UI_DrawInterface (&m_hudMenu.frameWork);
}


/*
=============
UI_HUDMenu_f
=============
*/
void UI_HUDMenu_f ()
{
	HUDMenu_Init ();
	M_PushMenu (&m_hudMenu.frameWork, HUDMenu_Draw, HUDMenu_Close, M_KeyHandler);
}
