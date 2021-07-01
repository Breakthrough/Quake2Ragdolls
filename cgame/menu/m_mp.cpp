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
// m_mp.c
//

#include "m_local.h"

/*
=============================================================================

	MULTIPLAYER MENU

=============================================================================
*/

struct m_multiPlayerMenu_t {
	// Menu items
	uiFrameWork_t		frameWork;

	uiImage_t			banner;

	uiAction_t			joinservers_menu;
	uiAction_t			startserver_menu;
	uiAction_t			plyrcfg_menu;
	uiAction_t			dlopts_menu;

	uiAction_t			back_action;
};

static m_multiPlayerMenu_t	m_multiPlayerMenu;

static void JoinServer_Menu (void *unused)
{
	UI_JoinServerMenu_f ();
}

static void StartServer_Menu (void *unused)
{
	UI_StartServerMenu_f ();
}

static void PlayerSetup_Menu (void *unused)
{
	UI_PlayerConfigMenu_f ();
}

static void DLOpts_Menu (void *unused)
{
	UI_DLOptionsMenu_f ();
}


/*
=============
MultiplayerMenu_Init
=============
*/
static void MultiplayerMenu_Init ()
{
	UI_StartFramework (&m_multiPlayerMenu.frameWork, FWF_CENTERHEIGHT);

	m_multiPlayerMenu.banner.generic.type		= UITYPE_IMAGE;
	m_multiPlayerMenu.banner.generic.flags		= UIF_NOSELECT|UIF_CENTERED;
	m_multiPlayerMenu.banner.generic.name		= NULL;
	m_multiPlayerMenu.banner.mat				= uiMedia.banners.multiplayer;

	m_multiPlayerMenu.joinservers_menu.generic.type		= UITYPE_ACTION;
	m_multiPlayerMenu.joinservers_menu.generic.flags	= UIF_CENTERED|UIF_LARGE|UIF_SHADOW;
	m_multiPlayerMenu.joinservers_menu.generic.name		= "Join Network Server";
	m_multiPlayerMenu.joinservers_menu.generic.callBack	= JoinServer_Menu;

	m_multiPlayerMenu.startserver_menu.generic.type		= UITYPE_ACTION;
	m_multiPlayerMenu.startserver_menu.generic.flags	= UIF_CENTERED|UIF_LARGE|UIF_SHADOW;
	m_multiPlayerMenu.startserver_menu.generic.name		= "Start Network Server";
	m_multiPlayerMenu.startserver_menu.generic.callBack	= StartServer_Menu;

	m_multiPlayerMenu.plyrcfg_menu.generic.type		= UITYPE_ACTION;
	m_multiPlayerMenu.plyrcfg_menu.generic.flags	= UIF_CENTERED|UIF_LARGE|UIF_SHADOW;
	m_multiPlayerMenu.plyrcfg_menu.generic.name		= "Player Configuration";
	m_multiPlayerMenu.plyrcfg_menu.generic.callBack	= PlayerSetup_Menu;

	m_multiPlayerMenu.dlopts_menu.generic.type		= UITYPE_ACTION;
	m_multiPlayerMenu.dlopts_menu.generic.flags		= UIF_CENTERED|UIF_LARGE|UIF_SHADOW;
	m_multiPlayerMenu.dlopts_menu.generic.name		= "Download Options";
	m_multiPlayerMenu.dlopts_menu.generic.callBack	= DLOpts_Menu;

	m_multiPlayerMenu.back_action.generic.type		= UITYPE_ACTION;
	m_multiPlayerMenu.back_action.generic.flags		= UIF_CENTERED|UIF_LARGE|UIF_SHADOW;
	m_multiPlayerMenu.back_action.generic.name		= "< Back";
	m_multiPlayerMenu.back_action.generic.callBack	= Menu_Pop;
	m_multiPlayerMenu.back_action.generic.statusBar	= "Back a menu";

	UI_AddItem (&m_multiPlayerMenu.frameWork,		&m_multiPlayerMenu.banner);

	UI_AddItem (&m_multiPlayerMenu.frameWork,		&m_multiPlayerMenu.joinservers_menu);
	UI_AddItem (&m_multiPlayerMenu.frameWork,		&m_multiPlayerMenu.startserver_menu);
	UI_AddItem (&m_multiPlayerMenu.frameWork,		&m_multiPlayerMenu.plyrcfg_menu);
	UI_AddItem (&m_multiPlayerMenu.frameWork,		&m_multiPlayerMenu.dlopts_menu);

	UI_AddItem (&m_multiPlayerMenu.frameWork,		&m_multiPlayerMenu.back_action);

	UI_FinishFramework (&m_multiPlayerMenu.frameWork, true);
}


/*
=============
MultiplayerMenu_Close
=============
*/
static struct sfx_t *MultiplayerMenu_Close ()
{
	return uiMedia.sounds.menuOut;
}


/*
=============
MultiplayerMenu_Draw
=============
*/
static void MultiplayerMenu_Draw ()
{
	float	y;

	// Initialize if necessary
	if (!m_multiPlayerMenu.frameWork.initialized)
		MultiplayerMenu_Init ();

	// Dynamically position
	m_multiPlayerMenu.frameWork.x			= cg.refConfig.vidWidth * 0.5f;
	m_multiPlayerMenu.frameWork.y			= 0;

	m_multiPlayerMenu.banner.generic.x		= 0;
	m_multiPlayerMenu.banner.generic.y		= 0;

	y = m_multiPlayerMenu.banner.height * UI_SCALE;

	m_multiPlayerMenu.joinservers_menu.generic.x	= 0;
	m_multiPlayerMenu.joinservers_menu.generic.y	= y += UIFT_SIZEINC;
	m_multiPlayerMenu.startserver_menu.generic.x	= 0;
	m_multiPlayerMenu.startserver_menu.generic.y	= y += UIFT_SIZEINCLG;
	m_multiPlayerMenu.plyrcfg_menu.generic.x		= 0;
	m_multiPlayerMenu.plyrcfg_menu.generic.y		= y += UIFT_SIZEINCLG;
	m_multiPlayerMenu.dlopts_menu.generic.x			= 0;
	m_multiPlayerMenu.dlopts_menu.generic.y			= y += UIFT_SIZEINCLG;
	m_multiPlayerMenu.back_action.generic.x			= 0;
	m_multiPlayerMenu.back_action.generic.y			= y += (UIFT_SIZEINCLG*2);

	// Render
	UI_DrawInterface (&m_multiPlayerMenu.frameWork);
}


/*
=============
UI_MultiplayerMenu_f
=============
*/
void UI_MultiplayerMenu_f ()
{
	MultiplayerMenu_Init ();
	M_PushMenu (&m_multiPlayerMenu.frameWork, MultiplayerMenu_Draw, MultiplayerMenu_Close, M_KeyHandler);
}
