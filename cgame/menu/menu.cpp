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
// menu.c
//

#include "m_local.h"

menuState_t	menuState;

cVar_t	*ui_jsMenuPage;
cVar_t	*ui_jsSortItem;
cVar_t	*ui_jsSortMethod;

static conCmd_t	*cmd_menuMain;

static conCmd_t	*cmd_menuGame;
static conCmd_t	*cmd_menuLoadGame;
static conCmd_t	*cmd_menuSaveGame;
static conCmd_t	*cmd_menuCredits;

static conCmd_t	*cmd_menuMultiplayer;
static conCmd_t	*cmd_menuDLOptions;
static conCmd_t	*cmd_menuJoinServer;
static conCmd_t	*cmd_menuAddressBook;
static conCmd_t	*cmd_menuPlayerConfig;
static conCmd_t	*cmd_menuStartServer;
static conCmd_t	*cmd_menuDMFlags;

static conCmd_t	*cmd_menuOptions;
static conCmd_t	*cmd_menuControls;
static conCmd_t	*cmd_menuEffects;
static conCmd_t	*cmd_menuHUD;
static conCmd_t	*cmd_menuInput;
static conCmd_t	*cmd_menuMisc;
static conCmd_t	*cmd_menuScreen;
static conCmd_t	*cmd_menuSound;

static conCmd_t	*cmd_menuVideo;
static conCmd_t	*cmd_menuGLExts;
static conCmd_t	*cmd_menuVidSettings;

static conCmd_t	*cmd_menuQuit;

static conCmd_t	*cmd_startSStatus;

/*
=============================================================================

	MENU INITIALIZATION/SHUTDOWN

=============================================================================
*/

/*
=================
M_Init
=================
*/
void JoinMenu_StartSStatus (); // FIXME
void M_Init ()
{
	int		i;

   	// Register cvars
	for (i=0 ; i<MAX_ADDRBOOK_SAVES ; i++)
		cgi.Cvar_Register (Q_VarArgs ("adr%i", i),				"",			CVAR_ARCHIVE);

	ui_jsMenuPage		= cgi.Cvar_Register ("ui_jsMenuPage",	"0",		CVAR_ARCHIVE);
	ui_jsSortItem		= cgi.Cvar_Register ("ui_jsSortItem",	"0",		CVAR_ARCHIVE);
	ui_jsSortMethod		= cgi.Cvar_Register ("ui_jsSortMethod",	"0",		CVAR_ARCHIVE);

	// Add commands
	cmd_menuMain		= cgi.Cmd_AddCommand ("menu_main",			0, UI_MainMenu_f,				"Opens the main menu");

	cmd_menuGame		= cgi.Cmd_AddCommand ("menu_game",			0, UI_GameMenu_f,				"Opens the single player menu");
	cmd_menuLoadGame	= cgi.Cmd_AddCommand ("menu_loadgame",		0, UI_LoadGameMenu_f,			"Opens the load game menu");
	cmd_menuSaveGame	= cgi.Cmd_AddCommand ("menu_savegame",		0, UI_SaveGameMenu_f,			"Opens the save game menu");
	cmd_menuCredits		= cgi.Cmd_AddCommand ("menu_credits",		0, UI_CreditsMenu_f,			"Opens the credits menu");

	cmd_menuMultiplayer	= cgi.Cmd_AddCommand ("menu_multiplayer",	0, UI_MultiplayerMenu_f,		"Opens the multiplayer menu");
	cmd_menuDLOptions	= cgi.Cmd_AddCommand ("menu_dloptions",		0, UI_DLOptionsMenu_f,			"Opens the download options menu");
	cmd_menuJoinServer	= cgi.Cmd_AddCommand ("menu_joinserver",	0, UI_JoinServerMenu_f,			"Opens the join server menu");
	cmd_menuAddressBook	= cgi.Cmd_AddCommand ("menu_addressbook",	0, UI_AddressBookMenu_f,		"Opens the address book menu");
	cmd_menuPlayerConfig= cgi.Cmd_AddCommand ("menu_playerconfig",	0, UI_PlayerConfigMenu_f,		"Opens the player configuration menu");
	cmd_menuStartServer	= cgi.Cmd_AddCommand ("menu_startserver",	0, UI_StartServerMenu_f,		"Opens the start server menu");
	cmd_menuDMFlags		= cgi.Cmd_AddCommand ("menu_dmflags",		0, UI_DMFlagsMenu_f,			"Opens the deathmatch flags menu");

	cmd_menuOptions		= cgi.Cmd_AddCommand ("menu_options",		0, UI_OptionsMenu_f,			"Opens the options menu");
	cmd_menuControls	= cgi.Cmd_AddCommand ("menu_controls",		0, UI_ControlsMenu_f,			"Opens the controls menu");
	cmd_menuEffects		= cgi.Cmd_AddCommand ("menu_effects",		0, UI_EffectsMenu_f,			"Opens the effects menu");
	cmd_menuHUD			= cgi.Cmd_AddCommand ("menu_hud",			0, UI_HUDMenu_f,				"Opens the hud menu");
	cmd_menuInput		= cgi.Cmd_AddCommand ("menu_input",			0, UI_InputMenu_f,				"Opens the input menu");
	cmd_menuMisc		= cgi.Cmd_AddCommand ("menu_misc",			0, UI_MiscMenu_f,				"Opens the misc menu");
	cmd_menuScreen		= cgi.Cmd_AddCommand ("menu_screen",		0, UI_ScreenMenu_f,				"Opens the screen menu");
	cmd_menuSound		= cgi.Cmd_AddCommand ("menu_sound",			0, UI_SoundMenu_f,				"Opens the sound menu");

	cmd_menuVideo		= cgi.Cmd_AddCommand ("menu_video",			0, UI_VideoMenu_f,				"Opens the video menu");
	cmd_menuGLExts		= cgi.Cmd_AddCommand ("menu_glexts",		0, UI_GLExtsMenu_f,				"Opens the opengl extensions menu");
	cmd_menuVidSettings	= cgi.Cmd_AddCommand ("menu_vidsettings",	0, UI_VIDSettingsMenu_f,		"Opens the video settings menu");

	cmd_menuQuit		= cgi.Cmd_AddCommand ("menu_quit",			0, UI_QuitMenu_f,				"Opens the quit menu");

	cmd_startSStatus	= cgi.Cmd_AddCommand ("ui_startSStatus",	0, JoinMenu_StartSStatus,		"");
}


/*
=================
M_Shutdown
=================
*/
void M_Shutdown ()
{
	// Don't play the menu exit sound
	menuState.playExitSound = false;

	// Get rid of the menu
	M_ForceMenuOff ();

	// Remove commands
	cgi.Cmd_RemoveCommand(cmd_menuMain);

	cgi.Cmd_RemoveCommand(cmd_menuGame);
	cgi.Cmd_RemoveCommand(cmd_menuLoadGame);
	cgi.Cmd_RemoveCommand(cmd_menuSaveGame);
	cgi.Cmd_RemoveCommand(cmd_menuCredits);

	cgi.Cmd_RemoveCommand(cmd_menuMultiplayer);
	cgi.Cmd_RemoveCommand(cmd_menuDLOptions);
	cgi.Cmd_RemoveCommand(cmd_menuJoinServer);
	cgi.Cmd_RemoveCommand(cmd_menuAddressBook);
	cgi.Cmd_RemoveCommand(cmd_menuPlayerConfig);
	cgi.Cmd_RemoveCommand(cmd_menuStartServer);
	cgi.Cmd_RemoveCommand(cmd_menuDMFlags);

	cgi.Cmd_RemoveCommand(cmd_menuOptions);
	cgi.Cmd_RemoveCommand(cmd_menuControls);
	cgi.Cmd_RemoveCommand(cmd_menuEffects);
	cgi.Cmd_RemoveCommand(cmd_menuHUD);
	cgi.Cmd_RemoveCommand(cmd_menuInput);
	cgi.Cmd_RemoveCommand(cmd_menuMisc);
	cgi.Cmd_RemoveCommand(cmd_menuScreen);
	cgi.Cmd_RemoveCommand(cmd_menuSound);

	cgi.Cmd_RemoveCommand(cmd_menuVideo);
	cgi.Cmd_RemoveCommand(cmd_menuGLExts);
	cgi.Cmd_RemoveCommand(cmd_menuVidSettings);

	cgi.Cmd_RemoveCommand(cmd_menuQuit);

	cgi.Cmd_RemoveCommand(cmd_startSStatus);
}

/*
=============================================================================

	PUBLIC FUNCTIONS

=============================================================================
*/

/*
=================
M_Refresh
=================
*/
void M_Refresh ()
{
	// Delay playing the enter sound until after the menu has
	// been drawn, to avoid delay while caching images
	if (menuState.playEnterSound) {
		cgi.Snd_StartLocalSound (uiMedia.sounds.menuIn, 1);
		menuState.playEnterSound = false;
	}
	else if (uiState.newCursorItem) {
		// Play menu open sound
		cgi.Snd_StartLocalSound (uiMedia.sounds.menuMove, 1);
		uiState.newCursorItem = false;
	}
}


/*
=================
M_ForceMenuOff
=================
*/
void M_ForceMenuOff ()
{
	cg.menuOpen = false;

	// Unpause
	cgi.Cvar_Set ("paused", "0", false);

	// Play exit sound
	if (menuState.playExitSound) {
		cgi.Snd_StartLocalSound (uiMedia.sounds.menuOut, 1);
		menuState.playExitSound = false;
	}

	// Update mouse position
	UI_UpdateMousePos ();

	// Kill the interfaces
	UI_ForceAllOff ();
}


/*
=================
M_PopMenu
=================
*/
void M_PopMenu ()
{
	UI_PopInterface ();
}

/*
=============================================================================

	LOCAL FUNCTIONS

=============================================================================
*/

/*
=================
M_PushMenu
=================
*/
void M_PushMenu (uiFrameWork_t *frameWork, void (*drawFunc) (), struct sfx_t *(*closeFunc) (), struct sfx_t *(*keyFunc) (uiFrameWork_t *fw, keyNum_t keyNum))
{
	// Pause single-player games
	if (cgi.Cvar_GetFloatValue ("maxclients") == 1 && cgi.Com_ServerState ())
		cgi.Cvar_Set ("paused", "1", false);

	menuState.playEnterSound = true;
	menuState.playExitSound = true;

	UI_PushInterface (frameWork, drawFunc, closeFunc, keyFunc);

	cg.menuOpen = true;
}


/*
=================
M_KeyHandler
=================
*/
struct sfx_t *M_KeyHandler (uiFrameWork_t *fw, keyNum_t keyNum)
{
	if (keyNum == K_ESCAPE) {
		M_PopMenu ();
		return NULL;
	}

	return UI_DefaultKeyFunc (fw, keyNum);
}
