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
// m_vid.c
//

#include "m_local.h"

/*
=======================================================================

	VIDEO MENU

=======================================================================
*/

struct m_videoMenu_t {
	// Menu items
	uiFrameWork_t		frameWork;

	uiImage_t			banner;

	uiAction_t			vidsettings_menu;
	uiAction_t			glexts_menu;

	uiAction_t			back_action;
};

static m_videoMenu_t	m_videoMenu;

static void GLExts_Menu (void *unused)
{
	UI_GLExtsMenu_f ();
}

static void VIDSettings_Menu (void *unused)
{
	UI_VIDSettingsMenu_f ();
}


/*
=============
VideoMenu_Init
=============
*/
static void VideoMenu_Init ()
{
	UI_StartFramework (&m_videoMenu.frameWork, FWF_CENTERHEIGHT);

	m_videoMenu.banner.generic.type			= UITYPE_IMAGE;
	m_videoMenu.banner.generic.flags		= UIF_NOSELECT|UIF_CENTERED;
	m_videoMenu.banner.generic.name			= NULL;
	m_videoMenu.banner.mat					= uiMedia.banners.video;

	m_videoMenu.vidsettings_menu.generic.type		= UITYPE_ACTION;
	m_videoMenu.vidsettings_menu.generic.flags		= UIF_CENTERED|UIF_LARGE|UIF_SHADOW;
	m_videoMenu.vidsettings_menu.generic.name		= "Video Settings";
	m_videoMenu.vidsettings_menu.generic.callBack	= VIDSettings_Menu;
	m_videoMenu.vidsettings_menu.generic.statusBar	= "Opens the Video Settings menu";

	m_videoMenu.glexts_menu.generic.type		= UITYPE_ACTION;
	m_videoMenu.glexts_menu.generic.flags		= UIF_CENTERED|UIF_LARGE|UIF_SHADOW;
	m_videoMenu.glexts_menu.generic.name		= "OpenGL Extensions";
	m_videoMenu.glexts_menu.generic.callBack	= GLExts_Menu;
	m_videoMenu.glexts_menu.generic.statusBar	= "Opens the GL Extensions menu";

	m_videoMenu.back_action.generic.type		= UITYPE_ACTION;
	m_videoMenu.back_action.generic.flags		= UIF_CENTERED|UIF_LARGE|UIF_SHADOW;
	m_videoMenu.back_action.generic.name		= "< Back";
	m_videoMenu.back_action.generic.callBack	= Menu_Pop;
	m_videoMenu.back_action.generic.statusBar	= "Back a menu";

	UI_AddItem (&m_videoMenu.frameWork,		&m_videoMenu.banner);

	UI_AddItem (&m_videoMenu.frameWork,		&m_videoMenu.vidsettings_menu);
	UI_AddItem (&m_videoMenu.frameWork,		&m_videoMenu.glexts_menu);

	UI_AddItem (&m_videoMenu.frameWork,		&m_videoMenu.back_action);

	UI_FinishFramework (&m_videoMenu.frameWork, true);
}


/*
=============
VideoMenu_Close
=============
*/
static struct sfx_t *VideoMenu_Close ()
{
	return uiMedia.sounds.menuOut;
}


/*
=============
VideoMenu_Draw
=============
*/
static void VideoMenu_Draw ()
{
	float	y;

	// Initialize if necessary
	if (!m_videoMenu.frameWork.initialized)
		VideoMenu_Init ();

	// Dynamically position
	m_videoMenu.frameWork.x		= cg.refConfig.vidWidth * 0.5f;
	m_videoMenu.frameWork.y		= 0;

	y = m_videoMenu.banner.height * UI_SCALE;

	m_videoMenu.banner.generic.x			= 0;
	m_videoMenu.banner.generic.y			= 0;
	m_videoMenu.vidsettings_menu.generic.x		= 0;
	m_videoMenu.vidsettings_menu.generic.y		= y += UIFT_SIZEINC;
	m_videoMenu.glexts_menu.generic.x			= 0;
	m_videoMenu.glexts_menu.generic.y			= y += UIFT_SIZEINCLG;
	m_videoMenu.back_action.generic.x			= 0;
	m_videoMenu.back_action.generic.y			= y += (UIFT_SIZEINCLG*2);

	// Render
	UI_DrawInterface (&m_videoMenu.frameWork);
}


/*
=============
UI_VideoMenu_f
=============
*/
void UI_VideoMenu_f ()
{
	VideoMenu_Init ();
	M_PushMenu (&m_videoMenu.frameWork, VideoMenu_Draw, VideoMenu_Close, M_KeyHandler);
}
