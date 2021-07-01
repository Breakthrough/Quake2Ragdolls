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
// m_mp_downloading.c
//

#include "m_local.h"

/*
=============================================================================

	DOWNLOAD OPTIONS MENU

=============================================================================
*/

struct m_downloadOptionsMenu_t {
	// Menu items
	uiFrameWork_t		frameWork;

	uiImage_t			banner;
	uiAction_t			header;

	uiList_t			download_toggle;
	uiList_t			download_maps_toggle;
	uiList_t			download_models_toggle;
	uiList_t			download_players_toggle;
	uiList_t			download_sounds_box;

	uiAction_t			back_action;
};

static m_downloadOptionsMenu_t	m_downloadOptionsMenu;

static void DownloadCallback (void *self)
{
	uiList_t *f = (uiList_t *) self;

	if (f == &m_downloadOptionsMenu.download_toggle)
		cgi.Cvar_SetValue ("allow_download", f->curValue, false);
	else if (f == &m_downloadOptionsMenu.download_maps_toggle)
		cgi.Cvar_SetValue ("allow_download_maps", f->curValue, false);
	else if (f == &m_downloadOptionsMenu.download_models_toggle)
		cgi.Cvar_SetValue ("allow_download_models", f->curValue, false);
	else if (f == &m_downloadOptionsMenu.download_players_toggle)
		cgi.Cvar_SetValue ("allow_download_players", f->curValue, false);
	else if (f == &m_downloadOptionsMenu.download_sounds_box)
		cgi.Cvar_SetValue ("allow_download_sounds", f->curValue, false);
}


/*
=============
DLOptionsMenu_SetValues
=============
*/
static void DLOptionsMenu_SetValues ()
{
	cgi.Cvar_SetValue ("allow_download", clamp (cgi.Cvar_GetIntegerValue ("allow_download"), 0, 1), false);
	m_downloadOptionsMenu.download_toggle.curValue	= (cgi.Cvar_GetFloatValue("allow_download") != 0);

	cgi.Cvar_SetValue ("allow_download_maps", clamp (cgi.Cvar_GetIntegerValue ("allow_download_maps"), 0, 1), false);
	m_downloadOptionsMenu.download_maps_toggle.curValue	= (cgi.Cvar_GetFloatValue("allow_download_maps") != 0);

	cgi.Cvar_SetValue ("allow_download_players", clamp (cgi.Cvar_GetIntegerValue ("allow_download_players"), 0, 1), false);
	m_downloadOptionsMenu.download_players_toggle.curValue	= (cgi.Cvar_GetFloatValue("allow_download_players") != 0);

	cgi.Cvar_SetValue ("allow_download_models", clamp (cgi.Cvar_GetIntegerValue ("allow_download_models"), 0, 1), false);
	m_downloadOptionsMenu.download_models_toggle.curValue	= (cgi.Cvar_GetFloatValue("allow_download_models") != 0);

	cgi.Cvar_SetValue ("allow_download_sounds", clamp (cgi.Cvar_GetIntegerValue ("allow_download_sounds"), 0, 1), false);
	m_downloadOptionsMenu.download_sounds_box.curValue	= (cgi.Cvar_GetFloatValue("allow_download_sounds") != 0);
}


/*
=============
DLOptionsMenu_Init
=============
*/
static void DLOptionsMenu_Init ()
{
	static char *yes_no_names[] = {
		"no",
		"yes",
		0
	};

	UI_StartFramework (&m_downloadOptionsMenu.frameWork, FWF_CENTERHEIGHT);

	m_downloadOptionsMenu.banner.generic.type		= UITYPE_IMAGE;
	m_downloadOptionsMenu.banner.generic.flags		= UIF_NOSELECT|UIF_CENTERED;
	m_downloadOptionsMenu.banner.generic.name		= NULL;
	m_downloadOptionsMenu.banner.mat				= uiMedia.banners.multiplayer;

	m_downloadOptionsMenu.header.generic.type		= UITYPE_ACTION;
	m_downloadOptionsMenu.header.generic.flags		= UIF_NOSELECT|UIF_CENTERED|UIF_MEDIUM|UIF_SHADOW;
	m_downloadOptionsMenu.header.generic.name		= "Download Options";

	m_downloadOptionsMenu.download_toggle.generic.type		= UITYPE_SPINCONTROL;
	m_downloadOptionsMenu.download_toggle.generic.name		= "Allow downloading";
	m_downloadOptionsMenu.download_toggle.generic.callBack	= DownloadCallback;
	m_downloadOptionsMenu.download_toggle.generic.statusBar	= "Completely toggling downloading";
	m_downloadOptionsMenu.download_toggle.itemNames			= yes_no_names;

	m_downloadOptionsMenu.download_maps_toggle.generic.type			= UITYPE_SPINCONTROL;
	m_downloadOptionsMenu.download_maps_toggle.generic.name			= "Download maps";
	m_downloadOptionsMenu.download_maps_toggle.generic.callBack		= DownloadCallback;
	m_downloadOptionsMenu.download_maps_toggle.generic.statusBar	= "Toggling downloading maps";
	m_downloadOptionsMenu.download_maps_toggle.itemNames			= yes_no_names;

	m_downloadOptionsMenu.download_players_toggle.generic.type		= UITYPE_SPINCONTROL;
	m_downloadOptionsMenu.download_players_toggle.generic.name		= "Download models/skins";
	m_downloadOptionsMenu.download_players_toggle.generic.callBack	= DownloadCallback;
	m_downloadOptionsMenu.download_players_toggle.generic.statusBar	= "Toggling downloading player models/skins";
	m_downloadOptionsMenu.download_players_toggle.itemNames			= yes_no_names;

	m_downloadOptionsMenu.download_models_toggle.generic.type		= UITYPE_SPINCONTROL;
	m_downloadOptionsMenu.download_models_toggle.generic.name		= "Download models";
	m_downloadOptionsMenu.download_models_toggle.generic.callBack	= DownloadCallback;
	m_downloadOptionsMenu.download_models_toggle.generic.statusBar	= "Toggling downloading models";
	m_downloadOptionsMenu.download_models_toggle.itemNames			= yes_no_names;

	m_downloadOptionsMenu.download_sounds_box.generic.type		= UITYPE_SPINCONTROL;
	m_downloadOptionsMenu.download_sounds_box.generic.name		= "Download sounds";
	m_downloadOptionsMenu.download_sounds_box.generic.callBack	= DownloadCallback;
	m_downloadOptionsMenu.download_sounds_box.generic.statusBar	= "Toggling downloading sounds";
	m_downloadOptionsMenu.download_sounds_box.itemNames			= yes_no_names;

	m_downloadOptionsMenu.back_action.generic.type			= UITYPE_ACTION;
	m_downloadOptionsMenu.back_action.generic.flags			= UIF_CENTERED|UIF_LARGE|UIF_SHADOW;
	m_downloadOptionsMenu.back_action.generic.name			= "< Back";
	m_downloadOptionsMenu.back_action.generic.callBack		= Menu_Pop;
	m_downloadOptionsMenu.back_action.generic.statusBar		= "Back a menu";

	DLOptionsMenu_SetValues ();

	UI_AddItem (&m_downloadOptionsMenu.frameWork,				&m_downloadOptionsMenu.banner);
	UI_AddItem (&m_downloadOptionsMenu.frameWork,				&m_downloadOptionsMenu.header);

	UI_AddItem (&m_downloadOptionsMenu.frameWork,				&m_downloadOptionsMenu.download_toggle);
	UI_AddItem (&m_downloadOptionsMenu.frameWork,				&m_downloadOptionsMenu.download_maps_toggle);
	UI_AddItem (&m_downloadOptionsMenu.frameWork,				&m_downloadOptionsMenu.download_players_toggle);
	UI_AddItem (&m_downloadOptionsMenu.frameWork,				&m_downloadOptionsMenu.download_models_toggle);
	UI_AddItem (&m_downloadOptionsMenu.frameWork,				&m_downloadOptionsMenu.download_sounds_box);

	UI_AddItem (&m_downloadOptionsMenu.frameWork,				&m_downloadOptionsMenu.back_action);

	UI_FinishFramework (&m_downloadOptionsMenu.frameWork, true);
}


/*
=============
DLOptionsMenu_Close
=============
*/
static struct sfx_t *DLOptionsMenu_Close ()
{
	return uiMedia.sounds.menuOut;
}


/*
=============
DLOptionsMenu_Draw
=============
*/
static void DLOptionsMenu_Draw ()
{
	float	y;

	// Initialize if necessary
	if (!m_downloadOptionsMenu.frameWork.initialized)
		DLOptionsMenu_Init ();

	// Dynamically position
	m_downloadOptionsMenu.frameWork.x						= cg.refConfig.vidWidth * 0.50f;
	m_downloadOptionsMenu.frameWork.y						= 0;

	m_downloadOptionsMenu.banner.generic.x					= 0;
	m_downloadOptionsMenu.banner.generic.y					= 0;

	y = m_downloadOptionsMenu.banner.height * UI_SCALE;

	m_downloadOptionsMenu.header.generic.x					= 0;
	m_downloadOptionsMenu.header.generic.y					= y += UIFT_SIZEINC;
	m_downloadOptionsMenu.download_toggle.generic.x			= 0;
	m_downloadOptionsMenu.download_toggle.generic.y			= y += UIFT_SIZEINC + UIFT_SIZEINCMED;
	m_downloadOptionsMenu.download_maps_toggle.generic.x	= 0;
	m_downloadOptionsMenu.download_maps_toggle.generic.y	= y += (UIFT_SIZEINC * 2);
	m_downloadOptionsMenu.download_players_toggle.generic.x	= 0;
	m_downloadOptionsMenu.download_players_toggle.generic.y	= y += UIFT_SIZEINC;
	m_downloadOptionsMenu.download_models_toggle.generic.x	= 0;
	m_downloadOptionsMenu.download_models_toggle.generic.y	= y += UIFT_SIZEINC;
	m_downloadOptionsMenu.download_sounds_box.generic.x		= 0;
	m_downloadOptionsMenu.download_sounds_box.generic.y		= y += UIFT_SIZEINC;

	m_downloadOptionsMenu.back_action.generic.x				= 0;
	m_downloadOptionsMenu.back_action.generic.y				= y += UIFT_SIZEINC + UIFT_SIZEINCLG;

	// Render
	UI_DrawInterface (&m_downloadOptionsMenu.frameWork);
}


/*
=============
UI_DLOptionsMenu_f
=============
*/
void UI_DLOptionsMenu_f ()
{
	DLOptionsMenu_Init ();
	M_PushMenu (&m_downloadOptionsMenu.frameWork, DLOptionsMenu_Draw, DLOptionsMenu_Close, M_KeyHandler);
}
