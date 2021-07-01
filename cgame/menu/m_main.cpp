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
// m_main.c
//

#include "m_local.h"

/*
=======================================================================

	MAIN MENU

=======================================================================
*/

struct m_mainMenu_t {
	// Local info
	int					cursorNum;

	// Menu items
	uiFrameWork_t		frameWork;

	uiImage_t			game_menu;
	uiImage_t			multiplayer_menu;
	uiImage_t			options_menu;
	uiImage_t			video_menu;
	uiImage_t			quit_menu;

	uiImage_t			plaque_pic;
	uiImage_t			idlogo_pic;
};

static m_mainMenu_t		m_mainMenu;

static void SP_Menu (void *unused)
{
	UI_GameMenu_f ();
}

static void MP_Menu (void *unused)
{
	UI_MultiplayerMenu_f ();
}

static void OPTS_Menu (void *unused)
{
	UI_OptionsMenu_f ();
}

static void VID_Menu (void *unused)
{
	UI_VideoMenu_f ();
}

static void QUIT_Menu (void *unused)
{
	UI_QuitMenu_f ();
}

#define MAIN_ITEMS			5
static void MainCursorDrawFunc (uiFrameWork_t *menu)
{
	float	y;
	int		w, h;

	m_mainMenu.cursorNum = ((int)(cg.realTime / 100)) % MAINMENU_CURSOR_NUMFRAMES;

	cgi.R_GetImageSize (uiMedia.menus.mainGame, NULL, &h);

	y = (((m_mainMenu.frameWork.cursor%MAIN_ITEMS) * (h + 5)));

	cgi.R_GetImageSize (uiMedia.menus.mainCursors[m_mainMenu.cursorNum], &w, &h);

	cgi.R_DrawPic (uiMedia.menus.mainCursors[m_mainMenu.cursorNum], 0,
				QuadVertices().SetVertices(menu->x + LCOLUMN_OFFSET - w*UIFT_SCALE - (3 * UIFT_SCALE), menu->y + (y * UIFT_SCALE),
				w*UIFT_SCALE, h*UIFT_SCALE), Q_BColorWhite);
}

/*
=============
MainMenu_Init
=============
*/
static void MainMenu_Init ()
{
	UI_StartFramework (&m_mainMenu.frameWork, FWF_CENTERHEIGHT);
	m_mainMenu.frameWork.cursorDraw		= MainCursorDrawFunc;

	m_mainMenu.game_menu.generic.type		= UITYPE_IMAGE;
	m_mainMenu.game_menu.generic.flags		= UIF_LEFT_JUSTIFY|UIF_NOSELBAR;
	m_mainMenu.game_menu.mat				= uiMedia.menus.mainGame;
	m_mainMenu.game_menu.hoverMat			= uiMedia.menus.mainGameSel;
	m_mainMenu.game_menu.generic.callBack	= SP_Menu;

	m_mainMenu.multiplayer_menu.generic.type		= UITYPE_IMAGE;
	m_mainMenu.multiplayer_menu.generic.flags		= UIF_LEFT_JUSTIFY|UIF_NOSELBAR;
	m_mainMenu.multiplayer_menu.mat					= uiMedia.menus.mainMultiplayer;
	m_mainMenu.multiplayer_menu.hoverMat			= uiMedia.menus.mainMultiplayerSel;
	m_mainMenu.multiplayer_menu.generic.callBack	= MP_Menu;

	m_mainMenu.options_menu.generic.type		= UITYPE_IMAGE;
	m_mainMenu.options_menu.generic.flags		= UIF_LEFT_JUSTIFY|UIF_NOSELBAR;
	m_mainMenu.options_menu.mat					= uiMedia.menus.mainOptions;
	m_mainMenu.options_menu.hoverMat			= uiMedia.menus.mainOptionsSel;
	m_mainMenu.options_menu.generic.callBack	= OPTS_Menu;

	m_mainMenu.video_menu.generic.type		= UITYPE_IMAGE;
	m_mainMenu.video_menu.generic.flags		= UIF_LEFT_JUSTIFY|UIF_NOSELBAR;
	m_mainMenu.video_menu.mat				= uiMedia.menus.mainVideo;
	m_mainMenu.video_menu.hoverMat			= uiMedia.menus.mainVideoSel;
	m_mainMenu.video_menu.generic.callBack	= VID_Menu;

	m_mainMenu.quit_menu.generic.type		= UITYPE_IMAGE;
	m_mainMenu.quit_menu.generic.flags		= UIF_LEFT_JUSTIFY|UIF_NOSELBAR;
	m_mainMenu.quit_menu.mat				= uiMedia.menus.mainQuit;
	m_mainMenu.quit_menu.hoverMat			= uiMedia.menus.mainQuitSel;
	m_mainMenu.quit_menu.generic.callBack	= QUIT_Menu;

	m_mainMenu.plaque_pic.generic.type		= UITYPE_IMAGE;
	m_mainMenu.plaque_pic.generic.flags		= UIF_LEFT_JUSTIFY|UIF_NOSELECT;
	m_mainMenu.plaque_pic.mat				= uiMedia.menus.mainPlaque;
	m_mainMenu.plaque_pic.hoverMat			= NULL;

	m_mainMenu.idlogo_pic.generic.type		= UITYPE_IMAGE;
	m_mainMenu.idlogo_pic.generic.flags		= UIF_LEFT_JUSTIFY|UIF_NOSELECT;
	m_mainMenu.idlogo_pic.mat				= uiMedia.menus.mainLogo;
	m_mainMenu.idlogo_pic.hoverMat			= NULL;

	UI_AddItem (&m_mainMenu.frameWork,		&m_mainMenu.game_menu);
	UI_AddItem (&m_mainMenu.frameWork,		&m_mainMenu.multiplayer_menu);
	UI_AddItem (&m_mainMenu.frameWork,		&m_mainMenu.options_menu);
	UI_AddItem (&m_mainMenu.frameWork,		&m_mainMenu.video_menu);
	UI_AddItem (&m_mainMenu.frameWork,		&m_mainMenu.quit_menu);

	UI_AddItem (&m_mainMenu.frameWork,		&m_mainMenu.plaque_pic);
	UI_AddItem (&m_mainMenu.frameWork,		&m_mainMenu.idlogo_pic);

	UI_FinishFramework (&m_mainMenu.frameWork, true);
}


/*
=============
MainMenu_Close
=============
*/
static struct sfx_t *MainMenu_Close ()
{
	return uiMedia.sounds.menuOut;
}


/*
=============
MainMenu_Draw
=============
*/
static void MainMenu_Draw ()
{
	int		y = 0;
	int		height;
	int		curWidth;
	int		plaqueWidth;
	int		logoWidth;

	// Initialize if necessary
	if (!m_mainMenu.frameWork.initialized)
		MainMenu_Init ();

	// Dynamically position
	m_mainMenu.frameWork.x				= cg.refConfig.vidWidth * 0.5 - (40 * UIFT_SCALE);
	m_mainMenu.frameWork.y				= 0;

	m_mainMenu.game_menu.generic.x			= 0;
	m_mainMenu.game_menu.generic.y			= y;
	height = m_mainMenu.game_menu.height;
	y += (height + 5) * UIFT_SCALE;

	m_mainMenu.multiplayer_menu.generic.x	= 0;
	m_mainMenu.multiplayer_menu.generic.y	= y;
	height = m_mainMenu.multiplayer_menu.height;
	y += (height + 5) * UIFT_SCALE;

	m_mainMenu.options_menu.generic.x		= 0;
	m_mainMenu.options_menu.generic.y		= y;
	height = m_mainMenu.options_menu.height;
	y += (height + 5) * UIFT_SCALE;

	m_mainMenu.video_menu.generic.x			= 0;
	m_mainMenu.video_menu.generic.y			= y;
	height = m_mainMenu.video_menu.height;
	y += (height + 5) * UIFT_SCALE;

	m_mainMenu.quit_menu.generic.x			= 0;
	m_mainMenu.quit_menu.generic.y			= y;

	cgi.R_GetImageSize (uiMedia.menus.mainCursors[m_mainMenu.cursorNum], &curWidth, NULL);
	cgi.R_GetImageSize (uiMedia.menus.mainPlaque, &plaqueWidth, NULL);
	cgi.R_GetImageSize (uiMedia.menus.mainLogo, &logoWidth, NULL);

	m_mainMenu.plaque_pic.generic.x			= - ((curWidth + plaqueWidth + 6) * UIFT_SCALE);
	m_mainMenu.plaque_pic.generic.y			= y = - (15 * UIFT_SCALE);
	cgi.R_GetImageSize (uiMedia.menus.mainPlaque, NULL, &height);
	y += height * UIFT_SCALE;

	m_mainMenu.idlogo_pic.generic.x			= - ((curWidth + logoWidth + 6) * UIFT_SCALE);
	m_mainMenu.idlogo_pic.generic.y			= y + (5 * UIFT_SCALE);

	// Render
	UI_DrawInterface (&m_mainMenu.frameWork);
}


/*
=============
UI_MainMenu_f
=============
*/
void UI_MainMenu_f ()
{
	if (uiState.activeUI == &m_mainMenu.frameWork)
		return;

	MainMenu_Init ();
	M_PushMenu (&m_mainMenu.frameWork, MainMenu_Draw, MainMenu_Close, M_KeyHandler);
}
