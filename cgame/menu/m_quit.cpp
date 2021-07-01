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
// m_quit.c
//

#include "m_local.h"

/*
=======================================================================

	QUIT MENU

=======================================================================
*/

struct m_quitMenu_t {
	// Menu items
	uiFrameWork_t		frameWork;

	uiImage_t			banner;

	uiAction_t			yes_action;
	uiAction_t			no_action;
};

static m_quitMenu_t		m_quitMenu;

bool m_quitAfterCredits;
static void QuitCredits (void *unused)
{
	m_quitAfterCredits = true;
	UI_CreditsMenu_f ();
}

/*
=============
QuitMenu_Init
=============
*/
static void QuitMenu_Init ()
{
	UI_StartFramework (&m_quitMenu.frameWork, FWF_CENTERHEIGHT);

	m_quitMenu.banner.generic.type			= UITYPE_IMAGE;
	m_quitMenu.banner.generic.flags			= UIF_NOSELECT|UIF_CENTERED;
	m_quitMenu.banner.generic.name			= NULL;
	m_quitMenu.banner.mat					= uiMedia.banners.quit;

	m_quitMenu.yes_action.generic.type		= UITYPE_ACTION;
	m_quitMenu.yes_action.generic.flags		= UIF_CENTERED|UIF_LARGE|UIF_SHADOW;
	m_quitMenu.yes_action.generic.name		= "Yes";
	m_quitMenu.yes_action.generic.callBack	= QuitCredits;
	m_quitMenu.yes_action.generic.statusBar	= "Sell out";

	m_quitMenu.no_action.generic.type		= UITYPE_ACTION;
	m_quitMenu.no_action.generic.flags		= UIF_CENTERED|UIF_LARGE|UIF_SHADOW;
	m_quitMenu.no_action.generic.name		= "No";
	m_quitMenu.no_action.generic.callBack	= Menu_Pop;
	m_quitMenu.no_action.generic.statusBar	= "No";

	UI_AddItem (&m_quitMenu.frameWork,		&m_quitMenu.banner);

	UI_AddItem (&m_quitMenu.frameWork,		&m_quitMenu.yes_action);
	UI_AddItem (&m_quitMenu.frameWork,		&m_quitMenu.no_action);

	UI_FinishFramework (&m_quitMenu.frameWork, true);
}


/*
=============
QuitMenu_Close
=============
*/
static struct sfx_t *QuitMenu_Close ()
{
	return uiMedia.sounds.menuOut;
}


/*
=============
QuitMenu_Draw
=============
*/
static void QuitMenu_Draw ()
{
	float	y;

	// Initialize if necessary
	if (!m_quitMenu.frameWork.initialized)
		QuitMenu_Init ();

	// Dynamically position
	m_quitMenu.frameWork.x				= cg.refConfig.vidWidth * 0.5f;
	m_quitMenu.frameWork.y				= 0;

	m_quitMenu.banner.generic.x			= 0;
	m_quitMenu.banner.generic.y			= 0;

	y = m_quitMenu.banner.height * UI_SCALE;

	m_quitMenu.yes_action.generic.x		= 0;
	m_quitMenu.yes_action.generic.y		= y += UIFT_SIZEINC;
	m_quitMenu.no_action.generic.x		= 0;
	m_quitMenu.no_action.generic.y		= y += UIFT_SIZEINCLG;

	// Render
	UI_DrawInterface (&m_quitMenu.frameWork);
}


/*
=============
QuitMenu_Key
=============
*/
static struct sfx_t *QuitMenu_Key (uiFrameWork_t *fw, keyNum_t keyNum)
{
	switch (keyNum) {
	case 'n':
	case 'N':
		M_PopMenu ();
		return NULL;

	case 'Y':
	case 'y':
		QuitCredits (NULL);
		return NULL;
	}

	return M_KeyHandler (fw, keyNum);
}


/*
=============
UI_QuitMenu_f
=============
*/
void UI_QuitMenu_f ()
{
	QuitMenu_Init ();
	M_PushMenu (&m_quitMenu.frameWork, QuitMenu_Draw, QuitMenu_Close, QuitMenu_Key);
}
