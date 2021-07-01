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
// m_sp_credits.c
//

#include "m_local.h"

extern bool m_quitAfterCredits;

/*
=============================================================================

	CREDITS MENU

=============================================================================
*/

static const char *ui_eglCredits[] = {
	S_COLOR_BLUE "EGL",
	"",
	S_COLOR_BLUE "PROGRAMMING",
	"Paul \"Echon\" Jackson",
	"",
	S_COLOR_BLUE "ARTISTRY",
	"Stannum",
	"Grim",
	"FuShanks",
	"",
	S_COLOR_BLUE "CODING HOMIES",
	"Vic",
	"Tr3B",
	"Tomaz",
	"R1CH",
	"psychospaz",
	"MrG",
	"Midgar",
	"majik",
	"LordHavoc",
	"Heffo",
	"Ghostface",
	"Discoloda",
	"Ankka",
	"",
	S_COLOR_BLUE "BETA TESTERS",
	"Stasis",
	"Sgt_Pain",
	"neRd",
	"ihavenolegs",
	"DaddyDeath",
	"[PeSTiCiDE]",
	"",
	"EGL IS AN OPEN SOURCE ENGINE BASED",
	"OFF OF THE QUAKE2 SOURCE CODE. IT IS",
	"FREE AND ABIDES BY THE GPL.",
	"",
	EGL_HOMEPAGE,
	0
};

static const char *ui_idCredits[] = {
	S_COLOR_BLUE "QUAKE II BY ID SOFTWARE",
	"",
	S_COLOR_BLUE "PROGRAMMING",
	"John Carmack",
	"John Cash",
	"Brian Hook",
	"",
	S_COLOR_BLUE "ART",
	"Adrian Carmack",
	"Kevin Cloud",
	"Paul Steed",
	"",
	S_COLOR_BLUE "LEVEL DESIGN",
	"Tim Willits",
	"American McGee",
	"Christian Antkow",
	"Paul Jaquays",
	"Brandon James",
	"",
	S_COLOR_BLUE "BIZ",
	"Todd Hollenshead",
	"Barrett (Bear) Alexander",
	"Donna Jackson",
	"",
	"",
	S_COLOR_BLUE "SPECIAL THANKS",
	"Ben Donges for beta testing",
	"",
	"",
	"",
	"",
	"",
	"",
	S_COLOR_BLUE "ADDITIONAL SUPPORT",
	"",
	S_COLOR_BLUE "LINUX PORT AND CTF",
	"Dave \"Zoid\" Kirsch",
	"",
	S_COLOR_BLUE "CINEMATIC SEQUENCES",
	"Ending Cinematic by Blur Studio - ",
	"Venice, CA",
	"",
	"Environment models for Introduction",
	"Cinematic by Karl Dolgener",
	"",
	"Assistance with environment design",
	"by Cliff Iwai",
	"",
	S_COLOR_BLUE "SOUND EFFECTS AND MUSIC",
	"Sound Design by Soundelux Media Labs.",
	"Music Composed and Produced by",
	"Soundelux Media Labs.  Special thanks",
	"to Bill Brown, Tom Ozanich, Brian",
	"Celano, Jeff Eisner, and The Soundelux",
	"Players.",
	"",
	"\"Level Music\" by Sonic Mayhem",
	"www.sonicmayhem.com",
	"",
	"\"Quake II Theme Song\"",
	"(C) 1997 Rob Zombie. All Rights",
	"Reserved.",
	"",
	"Track 10 (\"Climb\") by Jer Sypult",
	"",
	"Voice of computers by",
	"Carly Staehlin-Taylor",
	"",
	S_COLOR_BLUE "THANKS TO ACTIVISION",
	S_COLOR_BLUE "IN PARTICULAR:",
	"",
	"John Tam",
	"Steve Rosenthal",
	"Marty Stratton",
	"Henk Hartong",
	"",
	"Quake II(tm) (C)1997 Id Software, Inc.",
	"All Rights Reserved.  Distributed by",
	"Activision, Inc. under license.",
	"Quake II(tm), the Id Software name,",
	"the \"Q II\"(tm) logo and id(tm)",
	"logo are trademarks of Id Software,",
	"Inc. Activision(R) is a registered",
	"trademark of Activision, Inc. All",
	"other trademarks and trade names are",
	"properties of their respective owners.",
	0
};

static const char *ui_rogueCredits[] = {
	S_COLOR_BLUE "QUAKE II MISSION PACK 2: GROUND ZERO",
	S_COLOR_BLUE "BY",
	S_COLOR_BLUE "ROGUE ENTERTAINMENT, INC.",
	"",
	S_COLOR_BLUE "PRODUCED BY",
	"Jim Molinets",
	"",
	S_COLOR_BLUE "PROGRAMMING",
	"Peter Mack",
	"Patrick Magruder",
	"",
	S_COLOR_BLUE "LEVEL DESIGN",
	"Jim Molinets",
	"Cameron Lamprecht",
	"Berenger Fish",
	"Robert Selitto",
	"Steve Tietze",
	"Steve Thoms",
	"",
	S_COLOR_BLUE "ART DIRECTION",
	"Rich Fleider",
	"",
	S_COLOR_BLUE "ART",
	"Rich Fleider",
	"Steve Maines",
	"Won Choi",
	"",
	S_COLOR_BLUE "ANIMATION SEQUENCES",
	"Creat Studios",
	"Steve Maines",
	"",
	S_COLOR_BLUE "ADDITIONAL LEVEL DESIGN",
	"Rich Fleider",
	"Steve Maines",
	"Peter Mack",
	"",
	S_COLOR_BLUE "SOUND",
	"James Grunke",
	"",
	S_COLOR_BLUE "GROUND ZERO THEME",
	S_COLOR_BLUE "AND",
	S_COLOR_BLUE "MUSIC BY",
	"Sonic Mayhem",
	"",
	S_COLOR_BLUE "VWEP MODELS",
	"Brent \"Hentai\" Dill",
	"",
	"",
	"",
	S_COLOR_BLUE "SPECIAL THANKS",
	S_COLOR_BLUE "TO",
	S_COLOR_BLUE "OUR FRIENDS AT ID SOFTWARE",
	"",
	"John Carmack",
	"John Cash",
	"Brian Hook",
	"Adrian Carmack",
	"Kevin Cloud",
	"Paul Steed",
	"Tim Willits",
	"Christian Antkow",
	"Paul Jaquays",
	"Brandon James",
	"Todd Hollenshead",
	"Barrett (Bear) Alexander",
	"Katherine Anna Kang",
	"Donna Jackson",
	"Dave \"Zoid\" Kirsch",
	"",
	"",
	"",
	S_COLOR_BLUE "THANKS TO ACTIVISION",
	S_COLOR_BLUE "IN PARTICULAR:",
	"",
	"Marty Stratton",
	"Henk Hartong",
	"Mitch Lasky",
	"Steve Rosenthal",
	"Steve Elwell",
	"",
	S_COLOR_BLUE "AND THE GAME TESTERS",
	"",
	"The Ranger Clan",
	"Dave \"Zoid\" Kirsch",
	"Nihilistic Software",
	"Robert Duffy",
	"",
	"And Countless Others",
	"",
	"",
	"",
	"Quake II Mission Pack 2: Ground Zero",
	"(tm) (C)1998 Id Software, Inc. All",
	"Rights Reserved. Developed by Rogue",
	"Entertainment, Inc. for Id Software,",
	"Inc. Distributed by Activision Inc.",
	"under license. Quake(R) is a",
	"registered trademark of Id Software,",
	"Inc. Quake II Mission Pack 2: Ground",
	"Zero(tm), Quake II(tm), the Id",
	"Software name, the \"Q II\"(tm) logo",
	"and id(tm) logo are trademarks of Id",
	"Software, Inc. Activision(R) is a",
	"registered trademark of Activision,",
	"Inc. Rogue(R) is a registered",
	"trademark of Rogue Entertainment,",
	"Inc. All other trademarks and trade",
	"names are properties of their",
	"respective owners.",
	0
};

static const char *ui_xatrixCredits[] = {
	S_COLOR_BLUE "QUAKE II MISSION PACK: THE RECKONING",
	S_COLOR_BLUE "BY",
	S_COLOR_BLUE "XATRIX ENTERTAINMENT, INC.",
	"",
	S_COLOR_BLUE "DESIGN AND DIRECTION",
	"Drew Markham",
	"",
	S_COLOR_BLUE "PRODUCED BY",
	"Greg Goodrich",
	"",
	S_COLOR_BLUE "PROGRAMMING",
	"Rafael Paiz",
	"",
	S_COLOR_BLUE "LEVEL DESIGN / ADDITIONAL GAME DESIGN",
	"Alex Mayberry",
	"",
	S_COLOR_BLUE "LEVEL DESIGN",
	"Mal Blackwell",
	"Dan Koppel",
	"",
	S_COLOR_BLUE "ART DIRECTION",
	"Michael \"Maxx\" Kaufman",
	"",
	S_COLOR_BLUE "COMPUTER GRAPHICS SUPERVISOR AND",
	S_COLOR_BLUE "CHARACTER ANIMATION DIRECTION",
	"Barry Dempsey",
	"",
	S_COLOR_BLUE "SENIOR ANIMATOR AND MODELER",
	"Jason Hoover",
	"",
	S_COLOR_BLUE "CHARACTER ANIMATION AND",
	S_COLOR_BLUE "MOTION CAPTURE SPECIALIST",
	"Amit Doron",
	"",
	S_COLOR_BLUE "ART",
	"Claire Praderie-Markham",
	"Viktor Antonov",
	"Corky Lehmkuhl",
	"",
	S_COLOR_BLUE "INTRODUCTION ANIMATION",
	"Dominique Drozdz",
	"",
	S_COLOR_BLUE "ADDITIONAL LEVEL DESIGN",
	"Aaron Barber",
	"Rhett Baldwin",
	"",
	S_COLOR_BLUE "3D CHARACTER ANIMATION TOOLS",
	"Gerry Tyra, SA Technology",
	"",
	S_COLOR_BLUE "ADDITIONAL EDITOR TOOL PROGRAMMING",
	"Robert Duffy",
	"",
	S_COLOR_BLUE "ADDITIONAL PROGRAMMING",
	"Ryan Feltrin",
	"",
	S_COLOR_BLUE "PRODUCTION COORDINATOR",
	"Victoria Sylvester",
	"",
	S_COLOR_BLUE "SOUND DESIGN",
	"Gary Bradfield",
	"",
	S_COLOR_BLUE "MUSIC BY",
	"Sonic Mayhem",
	"",
	"",
	"",
	S_COLOR_BLUE "SPECIAL THANKS",
	S_COLOR_BLUE "TO",
	S_COLOR_BLUE "OUR FRIENDS AT ID SOFTWARE",
	"",
	"John Carmack",
	"John Cash",
	"Brian Hook",
	"Adrian Carmack",
	"Kevin Cloud",
	"Paul Steed",
	"Tim Willits",
	"Christian Antkow",
	"Paul Jaquays",
	"Brandon James",
	"Todd Hollenshead",
	"Barrett (Bear) Alexander",
	"Dave \"Zoid\" Kirsch",
	"Donna Jackson",
	"",
	"",
	"",
	S_COLOR_BLUE "THANKS TO ACTIVISION",
	S_COLOR_BLUE "IN PARTICULAR:",
	"",
	"Marty Stratton",
	"Henk \"The Original Ripper\" Hartong",
	"Kevin Kraff",
	"Jamey Gottlieb",
	"Chris Hepburn",
	"",
	S_COLOR_BLUE "AND THE GAME TESTERS",
	"",
	"Tim Vanlaw",
	"Doug Jacobs",
	"Steven Rosenthal",
	"David Baker",
	"Chris Campbell",
	"Aaron Casillas",
	"Steve Elwell",
	"Derek Johnstone",
	"Igor Krinitskiy",
	"Samantha Lee",
	"Michael Spann",
	"Chris Toft",
	"Juan Valdes",
	"",
	S_COLOR_BLUE "THANKS TO INTERGRAPH COMPUTER SYTEMS",
	S_COLOR_BLUE "IN PARTICULAR:",
	"",
	"Michael T. Nicolaou",
	"",
	"",
	"Quake II Mission Pack: The Reckoning",
	"(tm) (C)1998 Id Software, Inc. All",
	"Rights Reserved. Developed by Xatrix",
	"Entertainment, Inc. for Id Software,",
	"Inc. Distributed by Activision Inc.",
	"under license. Quake(R) is a",
	"registered trademark of Id Software,",
	"Inc. Quake II Mission Pack: The",
	"Reckoning(tm), Quake II(tm), the Id",
	"Software name, the \"Q II\"(tm) logo",
	"and id(tm) logo are trademarks of Id",
	"Software, Inc. Activision(R) is a",
	"registered trademark of Activision,",
	"Inc. Xatrix(R) is a registered",
	"trademark of Xatrix Entertainment,",
	"Inc. All other trademarks and trade",
	"names are properties of their",
	"respective owners.",
	0
};

struct m_creditsMenu_t {
	// Local info
	int					startTime;
	char				*fileBuffer;
	char				*creditsIndex[256];
	const char			**creditsBuffer;

	// Menu items
	uiFrameWork_t		frameWork;

	uiAction_t			eglCredits_action;
	uiAction_t			quake2Credits_action;
	uiAction_t			rogueCredits_action;
	uiAction_t			xatrixCredits_action;

	uiAction_t			back_action;
};

static m_creditsMenu_t		m_creditsMenu;

static void EGLCredits (void *unused)
{
	m_creditsMenu.creditsBuffer = ui_eglCredits;
	m_creditsMenu.startTime = cg.realTime;
}

static void Quake2Credits (void *unused)
{
	m_creditsMenu.creditsBuffer = ui_idCredits;
	m_creditsMenu.startTime = cg.realTime;
}

static void RogueCredits (void *unused)
{
	m_creditsMenu.creditsBuffer = ui_rogueCredits;
	m_creditsMenu.startTime = cg.realTime;
}

static void XatrixCredits (void *unused)
{
	m_creditsMenu.creditsBuffer = ui_xatrixCredits;
	m_creditsMenu.startTime = cg.realTime;
}


/*
=============
CreditsMenu_Init
=============
*/
static void CreditsMenu_Init ()
{
	int		n, count;
	char	*p;

	if (m_quitAfterCredits) {
		m_creditsMenu.creditsBuffer = ui_eglCredits;
		m_creditsMenu.startTime = cg.realTime;

		UI_StartFramework (&m_creditsMenu.frameWork, FWF_CENTERHEIGHT);
		UI_FinishFramework (&m_creditsMenu.frameWork, false);
		return;
	}

	m_creditsMenu.fileBuffer = NULL;
	count = cgi.FS_LoadFile ("credits", (void **)&m_creditsMenu.fileBuffer, false);

	// kthx problem with using credits selection and having a file? test me!
	if (m_creditsMenu.fileBuffer && count > 0) {
		p = m_creditsMenu.fileBuffer;
		for (n=0 ; n<255 ; n++) {
			m_creditsMenu.creditsIndex[n] = p;
			while ((*p != '\r') && (*p != '\n')) {
				p++;
				if (--count == 0)
					break;
			}

			if (*p == '\r') {
				*p++ = 0;
				if (--count == 0)
					break;
			}

			*p++ = 0;
			if (--count == 0)
				break;
		}
		m_creditsMenu.creditsIndex[++n] = 0;
		m_creditsMenu.creditsBuffer = (const char **)&m_creditsMenu.creditsIndex;
	}
	else {
		switch (cg.currGameMod) {
		case GAME_MOD_XATRIX:
			m_creditsMenu.creditsBuffer = ui_xatrixCredits;
			break;

		case GAME_MOD_ROGUE:
			m_creditsMenu.creditsBuffer = ui_rogueCredits;
			break;

		default:
			m_creditsMenu.creditsBuffer = ui_eglCredits;
			break;
		}
	}

	m_creditsMenu.startTime = cg.realTime;

	UI_StartFramework (&m_creditsMenu.frameWork, 0);

	m_creditsMenu.eglCredits_action.generic.type		= UITYPE_ACTION;
	m_creditsMenu.eglCredits_action.generic.flags		= UIF_LEFT_JUSTIFY|UIF_SHADOW;
	m_creditsMenu.eglCredits_action.generic.name		= "EGL Credits";
	m_creditsMenu.eglCredits_action.generic.callBack	= EGLCredits;

	m_creditsMenu.quake2Credits_action.generic.type		= UITYPE_ACTION;
	m_creditsMenu.quake2Credits_action.generic.flags	= UIF_LEFT_JUSTIFY|UIF_SHADOW;
	m_creditsMenu.quake2Credits_action.generic.name		= "Quake2 Credits";
	m_creditsMenu.quake2Credits_action.generic.callBack	= Quake2Credits;

	m_creditsMenu.rogueCredits_action.generic.type		= UITYPE_ACTION;
	m_creditsMenu.rogueCredits_action.generic.flags		= UIF_LEFT_JUSTIFY|UIF_SHADOW;
	m_creditsMenu.rogueCredits_action.generic.name		= "Rogue Credits";
	m_creditsMenu.rogueCredits_action.generic.callBack	= RogueCredits;

	m_creditsMenu.xatrixCredits_action.generic.type		= UITYPE_ACTION;
	m_creditsMenu.xatrixCredits_action.generic.flags	= UIF_LEFT_JUSTIFY|UIF_SHADOW;
	m_creditsMenu.xatrixCredits_action.generic.name		= "Xatrix Credits";
	m_creditsMenu.xatrixCredits_action.generic.callBack	= XatrixCredits;

	m_creditsMenu.back_action.generic.type			= UITYPE_ACTION;
	m_creditsMenu.back_action.generic.flags			= UIF_LEFT_JUSTIFY|UIF_SHADOW;
	m_creditsMenu.back_action.generic.name			= "Back";
	m_creditsMenu.back_action.generic.callBack		= Menu_Pop;
	m_creditsMenu.back_action.generic.statusBar		= "Back a menu";

	UI_AddItem (&m_creditsMenu.frameWork,			&m_creditsMenu.eglCredits_action);
	UI_AddItem (&m_creditsMenu.frameWork,			&m_creditsMenu.quake2Credits_action);
	UI_AddItem (&m_creditsMenu.frameWork,			&m_creditsMenu.rogueCredits_action);
	UI_AddItem (&m_creditsMenu.frameWork,			&m_creditsMenu.xatrixCredits_action);

	UI_AddItem (&m_creditsMenu.frameWork,			&m_creditsMenu.back_action);

	UI_FinishFramework (&m_creditsMenu.frameWork, true);
}


/*
=============
CreditsMenu_Close
=============
*/
static struct sfx_t *CreditsMenu_Close ()
{
	if (m_creditsMenu.fileBuffer)
		CG_FS_FreeFile (m_creditsMenu.fileBuffer);

	return uiMedia.sounds.menuOut;
}


/*
=============
CreditsMenu_Draw
=============
*/
static void CreditsMenu_Draw ()
{
	int		i;
	float	y = 0;

	// Initialize if necessary
	if (!m_creditsMenu.frameWork.initialized)
		CreditsMenu_Init ();

	if (!m_quitAfterCredits) {
		m_creditsMenu.frameWork.x			= UIFT_SIZEINC*2;
		m_creditsMenu.frameWork.y			= 0;

		// Dynamically position
		m_creditsMenu.eglCredits_action.generic.x		= 0;
		m_creditsMenu.eglCredits_action.generic.y		= y += UIFT_SIZEINC;
		m_creditsMenu.quake2Credits_action.generic.x	= 0;
		m_creditsMenu.quake2Credits_action.generic.y	= y += UIFT_SIZEINC;
		m_creditsMenu.rogueCredits_action.generic.x		= 0;
		m_creditsMenu.rogueCredits_action.generic.y		= y += UIFT_SIZEINC;
		m_creditsMenu.xatrixCredits_action.generic.x	= 0;
		m_creditsMenu.xatrixCredits_action.generic.y	= y += UIFT_SIZEINC;
		m_creditsMenu.back_action.generic.x				= 0;
		m_creditsMenu.back_action.generic.y				= y += UIFT_SIZEINC*2;
	}

	// Draw the credits
	for (i=0, y=cg.refConfig.vidHeight - ((cg.realTime - m_creditsMenu.startTime) / 40.0F) ; m_creditsMenu.creditsBuffer[i] && (y < cg.refConfig.vidHeight) ; y += (UIFT_SIZEINCMED), i++) {
		if (y <= -UIFT_SIZEMED)
			continue;

		cgi.R_DrawString (NULL, (cg.refConfig.vidWidth - (Q_ColorCharCount (m_creditsMenu.creditsBuffer[i], (int)strlen (m_creditsMenu.creditsBuffer[i])) * UIFT_SIZEMED)) * 0.5,
			y, UIFT_SCALEMED, UIFT_SCALEMED, FS_SHADOW, (char *)m_creditsMenu.creditsBuffer[i], Q_BColorWhite);
	}

	// Restart credits
	if (y < 0) {
		if (m_quitAfterCredits)
			cgi.Cbuf_AddText ("quit\n");
		m_creditsMenu.startTime = cg.realTime;
	}

	// Render
	if (!m_quitAfterCredits)
		UI_DrawInterface (&m_creditsMenu.frameWork);
}


/*
=============
CreditsMenu_Key
=============
*/
static struct sfx_t *CreditsMenu_Key (uiFrameWork_t *fw, keyNum_t keyNum)
{
	if (m_quitAfterCredits) {
		cgi.Cbuf_AddText ("quit\n");
		return NULL;
	}

	switch (keyNum) {
	case K_ESCAPE:
		if (m_creditsMenu.fileBuffer)
			CG_FS_FreeFile (m_creditsMenu.fileBuffer);

		M_PopMenu ();
		return NULL;
	}

	return M_KeyHandler (fw, keyNum);
}


/*
=============
UI_CreditsMenu_f
=============
*/
void UI_CreditsMenu_f ()
{
	CreditsMenu_Init ();
	M_PushMenu (&m_creditsMenu.frameWork, CreditsMenu_Draw, CreditsMenu_Close, CreditsMenu_Key);
}
