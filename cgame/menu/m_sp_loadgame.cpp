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
// m_sp_loadgame.c
//

#include "m_local.h"

/*
=============================================================================

	LOADGAME MENU

=============================================================================
*/

struct m_loadGameMenu_t {
	// Menu items
	uiFrameWork_t		frameWork;

	uiImage_t			banner;

	uiAction_t			actions[MAX_SAVEGAMES];
	uiAction_t			back_action;
};

static m_loadGameMenu_t	m_loadGameMenu;

char		ui_saveStrings[MAX_SAVEGAMES][32];
bool		ui_saveValid[MAX_SAVEGAMES];

void Create_Savestrings ()
{
	int		i;
	FILE	*f;
	char	name[MAX_OSPATH];

	for (i=0 ; i<MAX_SAVEGAMES ; i++) {
		Q_snprintfz (name, sizeof(name), "%s/save/save%i/server.ssv", cgi.FS_Gamedir(), i);
		f = fopen (name, "rb");
		if (!f) {
			Q_strncpyz (ui_saveStrings[i], "<EMPTY>", sizeof(ui_saveStrings[i]));
			ui_saveValid[i] = false;
		}
		else {
			fread (ui_saveStrings[i], 1, sizeof(ui_saveStrings[i]), f);
			fclose (f);
			ui_saveValid[i] = true;
		}
	}
}

static void LoadGameCallback (void *self)
{
	uiAction_t *a = (uiAction_t *) self;

	if (ui_saveValid[a->generic.localData[0]])
		cgi.Cbuf_AddText (Q_VarArgs ("load save%i\n", a->generic.localData[0]));
	M_ForceMenuOff ();
}


/*
=============
LoadGameMenu_Init
=============
*/
static void LoadGameMenu_Init ()
{
	int		i;

	UI_StartFramework (&m_loadGameMenu.frameWork, FWF_CENTERHEIGHT);

	Create_Savestrings ();

	m_loadGameMenu.banner.generic.type			= UITYPE_IMAGE;
	m_loadGameMenu.banner.generic.flags			= UIF_NOSELECT|UIF_CENTERED;
	m_loadGameMenu.banner.generic.name			= NULL;
	m_loadGameMenu.banner.mat					= uiMedia.banners.loadGame;

	UI_AddItem (&m_loadGameMenu.frameWork,		&m_loadGameMenu.banner);

	for (i=0 ; i<MAX_SAVEGAMES ; i++) {
		m_loadGameMenu.actions[i].generic.type			= UITYPE_ACTION;
		m_loadGameMenu.actions[i].generic.name			= ui_saveStrings[i];
		m_loadGameMenu.actions[i].generic.flags			= UIF_LEFT_JUSTIFY;
		m_loadGameMenu.actions[i].generic.localData[0]	= i;
		m_loadGameMenu.actions[i].generic.callBack		= LoadGameCallback;

		UI_AddItem (&m_loadGameMenu.frameWork,	&m_loadGameMenu.actions[i]);
	}

	m_loadGameMenu.back_action.generic.type			= UITYPE_ACTION;
	m_loadGameMenu.back_action.generic.flags		= UIF_CENTERED|UIF_LARGE|UIF_SHADOW;
	m_loadGameMenu.back_action.generic.name			= "< Back";
	m_loadGameMenu.back_action.generic.callBack		= Menu_Pop;
	m_loadGameMenu.back_action.generic.statusBar	= "Back a menu";

	UI_AddItem (&m_loadGameMenu.frameWork,	&m_loadGameMenu.back_action);

	UI_FinishFramework (&m_loadGameMenu.frameWork, true);
}


/*
=============
LoadGameMenu_Close
=============
*/
static struct sfx_t *LoadGameMenu_Close ()
{
	return uiMedia.sounds.menuOut;
}


/*
=============
LoadGameMenu_Draw
=============
*/
static void LoadGameMenu_Draw ()
{
	float	y;
	int		i;

	// Initialize if necessary
	if (!m_loadGameMenu.frameWork.initialized)
		LoadGameMenu_Init ();

	// Dynamically position
	m_loadGameMenu.frameWork.x			= cg.refConfig.vidWidth * 0.5f;
	m_loadGameMenu.frameWork.y			= 0;

	m_loadGameMenu.banner.generic.x		= 0;
	m_loadGameMenu.banner.generic.y		= 0;

	y = m_loadGameMenu.banner.height * UI_SCALE;

	for (i=0 ; i<MAX_SAVEGAMES ; i++) {
		m_loadGameMenu.actions[i].generic.x		= -UIFT_SIZE*15;
		m_loadGameMenu.actions[i].generic.y		= y + ((i + 1) * UIFT_SIZEINC);
		if (i > 0)	// separate from autosave
			m_loadGameMenu.actions[i].generic.y	+= UIFT_SIZEINC;
	}

	m_loadGameMenu.back_action.generic.x		= 0;
	m_loadGameMenu.back_action.generic.y		= y + ((i + 1) * UIFT_SIZEINC) + (UIFT_SIZEINCLG);

	// Render
	UI_DrawInterface (&m_loadGameMenu.frameWork);
}


/*
=============
UI_LoadGameMenu_f
=============
*/
void UI_LoadGameMenu_f ()
{
	LoadGameMenu_Init ();
	M_PushMenu (&m_loadGameMenu.frameWork, LoadGameMenu_Draw, LoadGameMenu_Close, M_KeyHandler);
}
