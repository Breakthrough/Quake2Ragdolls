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
// m_sp_savegame.c
//

#include "m_local.h"

/*
=============================================================================

	SAVEGAME MENU

=============================================================================
*/

struct m_saveGameMenu_t {
	// Menu items
	uiFrameWork_t		frameWork;

	uiImage_t			banner;

	uiAction_t			actions[MAX_SAVEGAMES];
	uiAction_t			back_action;
};

static m_saveGameMenu_t	m_saveGameMenu;

static void SaveGameCallback (void *self)
{
	uiAction_t *a = (uiAction_t *) self;

	cgi.Cbuf_AddText (Q_VarArgs ("save save%i\n", a->generic.localData[0]));
	M_ForceMenuOff ();
}


/*
=============
SaveGameMenu_Init
=============
*/
static void SaveGameMenu_Init ()
{
	int		i;

	UI_StartFramework (&m_saveGameMenu.frameWork, FWF_CENTERHEIGHT);

	Create_Savestrings ();

	m_saveGameMenu.banner.generic.type		= UITYPE_IMAGE;
	m_saveGameMenu.banner.generic.flags		= UIF_NOSELECT|UIF_CENTERED;
	m_saveGameMenu.banner.generic.name		= NULL;
	m_saveGameMenu.banner.mat				= uiMedia.banners.saveGame;

	UI_AddItem (&m_saveGameMenu.frameWork,		&m_saveGameMenu.banner);

	// Don't include the autosave slot
	for (i=0 ; i<MAX_SAVEGAMES-1 ; i++) {
		m_saveGameMenu.actions[i].generic.type			= UITYPE_ACTION;
		m_saveGameMenu.actions[i].generic.name			= ui_saveStrings[i+1];
		m_saveGameMenu.actions[i].generic.localData[0]	= i+1;
		m_saveGameMenu.actions[i].generic.flags			= UIF_LEFT_JUSTIFY;
		m_saveGameMenu.actions[i].generic.callBack		= SaveGameCallback;

		UI_AddItem (&m_saveGameMenu.frameWork,	&m_saveGameMenu.actions[i]);
	}

	m_saveGameMenu.back_action.generic.type			= UITYPE_ACTION;
	m_saveGameMenu.back_action.generic.flags		= UIF_CENTERED|UIF_LARGE|UIF_SHADOW;
	m_saveGameMenu.back_action.generic.name			= "< Back";
	m_saveGameMenu.back_action.generic.callBack		= Menu_Pop;
	m_saveGameMenu.back_action.generic.statusBar	= "Back a menu";

	UI_AddItem (&m_saveGameMenu.frameWork,	&m_saveGameMenu.back_action);

	UI_FinishFramework (&m_saveGameMenu.frameWork, true);
}


/*
=============
SaveGameMenu_Close
=============
*/
static struct sfx_t *SaveGameMenu_Close ()
{
	return uiMedia.sounds.menuOut;
}


/*
=============
SaveGameMenu_Draw
=============
*/
static void SaveGameMenu_Draw ()
{
	float	y;
	int		i;

	// Initialize if necessary
	if (!m_saveGameMenu.frameWork.initialized)
		SaveGameMenu_Init ();

	// Dynamically position
	m_saveGameMenu.frameWork.x			= cg.refConfig.vidWidth * 0.5f;
	m_saveGameMenu.frameWork.y			= 0;

	m_saveGameMenu.banner.generic.x			= 0;
	m_saveGameMenu.banner.generic.y			= 0;

	y = m_saveGameMenu.banner.height * UI_SCALE;

	// Don't include the autosave slot
	for (i=0 ; i<MAX_SAVEGAMES-1 ; i++) {
		m_saveGameMenu.actions[i].generic.x	= -UIFT_SIZE*15;
		m_saveGameMenu.actions[i].generic.y	= y + ((i + 1) * UIFT_SIZEINC);
	}

	m_saveGameMenu.back_action.generic.x	= 0;
	m_saveGameMenu.back_action.generic.y	= y + ((i + 1) * UIFT_SIZEINC) + (UIFT_SIZEINCLG);

	// Render
	UI_DrawInterface (&m_saveGameMenu.frameWork);
}


/*
=============
UI_SaveGameMenu_f
=============
*/
void UI_SaveGameMenu_f ()
{
	if (!cgi.Com_ServerState ())
		return;	// Not playing a game

	SaveGameMenu_Init ();
	M_PushMenu (&m_saveGameMenu.frameWork, SaveGameMenu_Draw, SaveGameMenu_Close, M_KeyHandler);
	Create_Savestrings ();
}
