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
// m_opts_controls.c
//

#include "m_local.h"

/*
=======================================================================

	CONTROLS MENU

=======================================================================
*/

static char *m_bindNames[][2] = {
	// Attack
	{"+attack",					S_COLOR_GREEN "Attack"},
	{"weapprev",				S_COLOR_GREEN "Previous Weapon"},
	{"weapnext",				S_COLOR_GREEN "Next Weapon"},

	{"use blaster",				S_COLOR_GREEN "Blaster"},
	{"use shotgun",				S_COLOR_GREEN "Shotgun"},
	{"use super shotgun",		S_COLOR_GREEN "Super Shotgun"},
	{"use machinegun",			S_COLOR_GREEN "Machinegun"},
	{"use chaingun",			S_COLOR_GREEN "Chaingun"},
	{"use grenade launcher",	S_COLOR_GREEN "Grenade Launcher"},
	{"use rocket launcher",		S_COLOR_GREEN "Rocket Launcher"},
	{"use hyperblaster",		S_COLOR_GREEN "Hyperblaster"},
	{"use railgun",				S_COLOR_GREEN "Railgun"},
	{"use bfg10k",				S_COLOR_GREEN "BFG10k"},

	// Movement
	{"+speed",					S_COLOR_GREEN "Run / Sprint"},
	{"+forward",				S_COLOR_GREEN "Walk Forward"},
	{"+back",					S_COLOR_GREEN "Backpedal"},
	{"+moveup",					S_COLOR_GREEN "Up / Jump"},
	{"+movedown",				S_COLOR_GREEN "Down / Crouch"},

	{"+left",					S_COLOR_GREEN "Turn Left"},
	{"+right",					S_COLOR_GREEN "Turn Right"},
	{"+moveleft",				S_COLOR_GREEN "Step Left"},
	{"+moveright",				S_COLOR_GREEN "Step Right"},
	{"+strafe",					S_COLOR_GREEN "Sidestep"},

	{"+lookup",					S_COLOR_GREEN "Look Up"},
	{"+lookdown",				S_COLOR_GREEN "Look Down"},
	{"centerview",				S_COLOR_GREEN "Center View"},
	{"+mlook",					S_COLOR_GREEN "Mouse Look"},
	{"+klook",					S_COLOR_GREEN "Keyboard Look"},

	// Misc
	{"messagemode",				S_COLOR_GREEN "Chat"},
	{"messagemode2",			S_COLOR_GREEN "Team Chat"},

	{"inven",					S_COLOR_GREEN "Inventory"},
	{"invuse",					S_COLOR_GREEN "Use Item"},
	{"invdrop",					S_COLOR_GREEN "Drop Item"},
	{"invprev",					S_COLOR_GREEN "Previous Item"},
	{"invnext",					S_COLOR_GREEN "Next Item"},

	{"cmd help",				S_COLOR_GREEN "Help Computer" }, 
	{ 0, 0 }
};

struct m_controlsAttackMenu_t {
	uiAction_t		header;

	uiAction_t		attack_action;
	uiAction_t		prev_weapon_action;
	uiAction_t		next_weapon_action;

	uiAction_t		weaponHeader;

	uiAction_t		weaponBlaster_action;
	uiAction_t		weaponShotgun_action;
	uiAction_t		weaponSuperShotgun_action;
	uiAction_t		weaponMachinegun_action;
	uiAction_t		weaponChaingun_action;
	uiAction_t		weaponGrenadeLauncher_action;
	uiAction_t		weaponRocketLauncher_action;
	uiAction_t		weaponHyperblaster_action;
	uiAction_t		weaponRailgun_action;
	uiAction_t		weaponBFG10k_action;
};

struct m_controlsMovementMenu_t {
	uiAction_t		header;

	uiAction_t		run_action;
	uiAction_t		walk_forward_action;
	uiAction_t		backpedal_action;
	uiAction_t		move_up_action;
	uiAction_t		move_down_action;
	uiAction_t		turn_left_action;
	uiAction_t		turn_right_action;

	uiAction_t		step_left_action;
	uiAction_t		step_right_action;
	uiAction_t		sidestep_action;

	uiAction_t		look_up_action;
	uiAction_t		look_down_action;
	uiAction_t		center_view_action;
	uiAction_t		mouse_look_action;
	uiAction_t		keyboard_look_action;
};

struct m_controlsMiscMenu_t {
	uiAction_t		header;

	uiAction_t		chat_action;
	uiAction_t		teamchat_action;

	uiAction_t		inventory_action;
	uiAction_t		inv_use_action;
	uiAction_t		inv_drop_action;
	uiAction_t		inv_prev_action;
	uiAction_t		inv_next_action;

	uiAction_t		help_computer_action;
};

enum {
	CONTROLS_ATTACK,
	CONTROLS_MOVEMENT,
	CONTROLS_MISC,

	CONTROLS_MAX
};

struct m_controlsMenu_t {
	// Menu items
	uiFrameWork_t				frameWork;

	uiImage_t					banner;

	int							currMenu;

	m_controlsAttackMenu_t		attackMenu;
	uiAction_t					attackMenu_action;
	m_controlsMovementMenu_t	movementMenu;
	uiAction_t					movementMenu_action;
	m_controlsMiscMenu_t		miscMenu;
	uiAction_t					miscMenu_action;

	uiAction_t					back_action;
};

static m_controlsMenu_t		m_controlsMenu;


static void M_UnbindCommand (char *command)
{
	int		j, l;
	char	*b;

	l = (int)strlen (command);

	for (j=0 ; j<K_MAXKEYS ; j++) {
		b = cgi.Key_GetBindingBuf ((keyNum_t)j);
		if (!b)
			continue;

		if (!Q_strnicmp (b, command, l))
			cgi.Key_SetBinding ((keyNum_t)j, "");
	}
}

static void M_FindKeysForCommand (char *command, keyNum_t *twokeys)
{
	int		count, j, l;
	char	*b;

	twokeys[0] = twokeys[1] = K_BADKEY;
	l = (int)strlen (command);
	count = 0;

	for (j=0 ; j<K_MAXKEYS ; j++) {
		b = cgi.Key_GetBindingBuf ((keyNum_t)j);
		if (!b)
			continue;

		if (!Q_stricmp (b, command)) {
			twokeys[count++] = (keyNum_t)j;
			if (count >= 2)
				break;
		}
	}
}

static void KeyCursorDrawFunc (uiFrameWork_t *menu)
{
	uiCommon_t	*item = NULL;
	int				cursor;

	cursor = (uiState.cursorLock) ? '=' : ((int)(cg.realTime/250)&1) + 12;

	item = (uiCommon_t*)UI_ItemAtCursor (menu);
	if (item->flags & UIF_LEFT_JUSTIFY)
		cgi.R_DrawChar (NULL, menu->x + item->x + LCOLUMN_OFFSET + item->cursorOffset, menu->y + item->y, UISCALE_TYPE (item->flags), UISCALE_TYPE (item->flags), 0, cursor, Q_BColorWhite);
	else if (item->flags & UIF_CENTERED)
		cgi.R_DrawChar (NULL, menu->x + item->x - (((Q_ColorCharCount (item->name, (int)strlen (item->name)) + 4) * UISIZE_TYPE (item->flags))*0.5) + item->cursorOffset, menu->y + item->y, UISCALE_TYPE (item->flags), UISCALE_TYPE (item->flags), 0, cursor, Q_BColorWhite);
	else
		cgi.R_DrawChar (NULL, menu->x + item->x + item->cursorOffset, menu->y + item->y, UISCALE_TYPE (item->flags), UISCALE_TYPE (item->flags), 0, cursor, Q_BColorWhite);
}

static void DrawKeyBindingFunc (void *self)
{
	keyNum_t	keyNums[2];
	uiAction_t	*a = (uiAction_t *) self;

	M_FindKeysForCommand (m_bindNames[a->generic.localData[0]][0], keyNums);
		
	if (keyNums[0] == -1) {
		cgi.R_DrawString (NULL, a->generic.x + a->generic.parent->x + (UISIZE_TYPE (a->generic.flags)*2), a->generic.y + a->generic.parent->y,
			UISCALE_TYPE (a->generic.flags), UISCALE_TYPE (a->generic.flags), 0, "???", Q_BColorWhite);

		a->generic.botRight[0] = a->generic.x + a->generic.parent->x + (UISIZE_TYPE (a->generic.flags)*5);
		a->generic.botRight[1] = a->generic.y + a->generic.parent->y + UISIZE_TYPE (a->generic.flags);
	}
	else {
		int		x;
		char	*name;

		name = cgi.Key_KeynumToString (keyNums[0]);

		cgi.R_DrawString (NULL, a->generic.x + a->generic.parent->x + (UISIZE_TYPE (a->generic.flags)*2), a->generic.y + a->generic.parent->y,
			UISCALE_TYPE (a->generic.flags), UISCALE_TYPE (a->generic.flags), 0, name, Q_BColorWhite);

		x = Q_ColorCharCount (name, (int)strlen (name)) * UISIZE_TYPE (a->generic.flags);

		a->generic.botRight[0] = a->generic.x + a->generic.parent->x + (UISIZE_TYPE (a->generic.flags)*2) + x;
		a->generic.botRight[1] = a->generic.y + a->generic.parent->y + UISIZE_TYPE (a->generic.flags);

		if (keyNums[1] != -1) {
			cgi.R_DrawString (NULL, a->generic.x + a->generic.parent->x + (UISIZE_TYPE (a->generic.flags)*3) + x, a->generic.y + a->generic.parent->y,
				UISCALE_TYPE (a->generic.flags), UISCALE_TYPE (a->generic.flags), 0, "or", Q_BColorWhite);
			cgi.R_DrawString (NULL, a->generic.x + a->generic.parent->x + (UISIZE_TYPE (a->generic.flags)*6) + x, a->generic.y + a->generic.parent->y,
				UISCALE_TYPE (a->generic.flags), UISCALE_TYPE (a->generic.flags), 0, cgi.Key_KeynumToString (keyNums[1]), Q_BColorWhite);

			a->generic.botRight[0] = a->generic.x + a->generic.parent->x + ((int)strlen (cgi.Key_KeynumToString (keyNums[1])) * UISIZE_TYPE (a->generic.flags)) + (UISIZE_TYPE (a->generic.flags)*6) + x;
			a->generic.botRight[1] = a->generic.y + a->generic.parent->y + UISIZE_TYPE (a->generic.flags);
		}
	}
}

static void KeyBindingFunc (void *self)
{
	keyNum_t	keyNums[2];
	uiAction_t	*a = (uiAction_t *) self;

	M_FindKeysForCommand (m_bindNames[a->generic.localData[0]][0], keyNums);

	if (keyNums[1] != -1)
		M_UnbindCommand (m_bindNames[a->generic.localData[0]][0]);

	uiState.cursorLock = true;
	cgi.Snd_StartLocalSound (uiMedia.sounds.menuIn, 1);

	m_controlsMenu.frameWork.statusBar = "Press a button for this action";
}


/*
=============
ControlsMenu_AddItems
=============
*/
static void ControlsMenu_AddItems ()
{
	// Clear items
	UI_StartFramework (&m_controlsMenu.frameWork, FWF_CENTERHEIGHT);

	// Add them back
	UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.banner);

	UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.attackMenu_action);
	UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.movementMenu_action);
	UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.miscMenu_action);

	switch (m_controlsMenu.currMenu) {
	case CONTROLS_ATTACK:
		// Attack menu
		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.attackMenu.header);

		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.attackMenu.attack_action);
		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.attackMenu.prev_weapon_action);
		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.attackMenu.next_weapon_action);

		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.attackMenu.weaponHeader);

		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.attackMenu.weaponBlaster_action);
		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.attackMenu.weaponShotgun_action);
		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.attackMenu.weaponSuperShotgun_action);
		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.attackMenu.weaponMachinegun_action);
		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.attackMenu.weaponChaingun_action);
		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.attackMenu.weaponGrenadeLauncher_action);
		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.attackMenu.weaponRocketLauncher_action);
		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.attackMenu.weaponHyperblaster_action);
		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.attackMenu.weaponRailgun_action);
		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.attackMenu.weaponBFG10k_action);
		break;

	case CONTROLS_MOVEMENT:
		// Movement menu
		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.movementMenu.header);

		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.movementMenu.run_action);
		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.movementMenu.walk_forward_action);
		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.movementMenu.backpedal_action);
		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.movementMenu.move_up_action);
		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.movementMenu.move_down_action);
		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.movementMenu.turn_left_action);
		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.movementMenu.turn_right_action);

		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.movementMenu.step_left_action);
		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.movementMenu.step_right_action);
		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.movementMenu.sidestep_action);

		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.movementMenu.look_up_action);
		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.movementMenu.look_down_action);
		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.movementMenu.center_view_action);
		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.movementMenu.mouse_look_action);
		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.movementMenu.keyboard_look_action);
		break;

	case CONTROLS_MISC:
		// Misc menu
		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.miscMenu.header);

		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.miscMenu.chat_action);
		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.miscMenu.teamchat_action);

		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.miscMenu.inventory_action);
		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.miscMenu.inv_use_action);
		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.miscMenu.inv_drop_action);
		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.miscMenu.inv_prev_action);
		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.miscMenu.inv_next_action);

		UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.miscMenu.help_computer_action);
		break;
	}

	UI_AddItem (&m_controlsMenu.frameWork,			&m_controlsMenu.back_action);

	m_controlsMenu.frameWork.statusBar = "[ENTER] to change, [BACKSPACE] to clear";
	UI_FinishFramework (&m_controlsMenu.frameWork, true);
}


static void ControlsMenu_AttackMenu (void *self)
{
	m_controlsMenu.currMenu = CONTROLS_ATTACK;
	ControlsMenu_AddItems ();
	cgi.Snd_StartLocalSound (uiMedia.sounds.menuIn, 1);
}

static void ControlsMenu_MovementMenu (void *self)
{
	m_controlsMenu.currMenu = CONTROLS_MOVEMENT;
	ControlsMenu_AddItems ();
	cgi.Snd_StartLocalSound (uiMedia.sounds.menuIn, 1);
}

static void ControlsMenu_MiscMenu (void *self)
{
	m_controlsMenu.currMenu = CONTROLS_MISC;
	ControlsMenu_AddItems ();
	cgi.Snd_StartLocalSound (uiMedia.sounds.menuIn, 1);
}


/*
=============
ControlsMenu_Init
=============
*/
static void ControlsMenu_Init ()
{
	int		i;

	m_controlsMenu.frameWork.cursorDraw		= KeyCursorDrawFunc;

	m_controlsMenu.banner.generic.type		= UITYPE_IMAGE;
	m_controlsMenu.banner.generic.flags		= UIF_NOSELECT|UIF_CENTERED;
	m_controlsMenu.banner.generic.name		= NULL;
	m_controlsMenu.banner.mat				= uiMedia.banners.options;

	//
	// Menu selection
	//
	m_controlsMenu.attackMenu_action.generic.type			= UITYPE_ACTION;
	m_controlsMenu.attackMenu_action.generic.flags			= UIF_CENTERED|UIF_MEDIUM|UIF_SHADOW;
	m_controlsMenu.attackMenu_action.generic.name			= S_COLOR_YELLOW "Attack";
	m_controlsMenu.attackMenu_action.generic.callBack		= ControlsMenu_AttackMenu;

	m_controlsMenu.movementMenu_action.generic.type			= UITYPE_ACTION;
	m_controlsMenu.movementMenu_action.generic.flags		= UIF_CENTERED|UIF_MEDIUM|UIF_SHADOW;
	m_controlsMenu.movementMenu_action.generic.name			= S_COLOR_YELLOW "Movement";
	m_controlsMenu.movementMenu_action.generic.callBack		= ControlsMenu_MovementMenu;

	m_controlsMenu.miscMenu_action.generic.type				= UITYPE_ACTION;
	m_controlsMenu.miscMenu_action.generic.flags			= UIF_CENTERED|UIF_MEDIUM|UIF_SHADOW;
	m_controlsMenu.miscMenu_action.generic.name				= S_COLOR_YELLOW "Misc";
	m_controlsMenu.miscMenu_action.generic.callBack			= ControlsMenu_MiscMenu;

	//
	// Attack menu
	//
	m_controlsMenu.attackMenu.header.generic.type		= UITYPE_ACTION;
	m_controlsMenu.attackMenu.header.generic.flags		= UIF_NOSELECT|UIF_CENTERED|UIF_MEDIUM|UIF_SHADOW;
	m_controlsMenu.attackMenu.header.generic.name		= "Attack Controls";

	m_controlsMenu.attackMenu.attack_action.generic.type			= UITYPE_ACTION;
	m_controlsMenu.attackMenu.attack_action.generic.flags			= 0;
	m_controlsMenu.attackMenu.attack_action.generic.ownerDraw		= DrawKeyBindingFunc;
	m_controlsMenu.attackMenu.attack_action.generic.localData[0]	= i = 0;
	m_controlsMenu.attackMenu.attack_action.generic.name			= m_bindNames[m_controlsMenu.attackMenu.attack_action.generic.localData[0]][1];
	m_controlsMenu.attackMenu.attack_action.generic.callBack		= KeyBindingFunc;

	m_controlsMenu.attackMenu.prev_weapon_action.generic.type			= UITYPE_ACTION;
	m_controlsMenu.attackMenu.prev_weapon_action.generic.flags			= 0;
	m_controlsMenu.attackMenu.prev_weapon_action.generic.ownerDraw		= DrawKeyBindingFunc;
	m_controlsMenu.attackMenu.prev_weapon_action.generic.localData[0]	= ++i;
	m_controlsMenu.attackMenu.prev_weapon_action.generic.name			= m_bindNames[m_controlsMenu.attackMenu.prev_weapon_action.generic.localData[0]][1];
	m_controlsMenu.attackMenu.prev_weapon_action.generic.callBack		= KeyBindingFunc;

	m_controlsMenu.attackMenu.next_weapon_action.generic.type			= UITYPE_ACTION;
	m_controlsMenu.attackMenu.next_weapon_action.generic.flags			= 0;
	m_controlsMenu.attackMenu.next_weapon_action.generic.ownerDraw		= DrawKeyBindingFunc;
	m_controlsMenu.attackMenu.next_weapon_action.generic.localData[0]	= ++i;
	m_controlsMenu.attackMenu.next_weapon_action.generic.name			= m_bindNames[m_controlsMenu.attackMenu.next_weapon_action.generic.localData[0]][1];
	m_controlsMenu.attackMenu.next_weapon_action.generic.callBack		= KeyBindingFunc;

	m_controlsMenu.attackMenu.weaponHeader.generic.type		= UITYPE_ACTION;
	m_controlsMenu.attackMenu.weaponHeader.generic.flags	= UIF_NOSELECT|UIF_CENTERED|UIF_MEDIUM|UIF_SHADOW;
	m_controlsMenu.attackMenu.weaponHeader.generic.name		= "Weapons";

	m_controlsMenu.attackMenu.weaponBlaster_action.generic.type				= UITYPE_ACTION;
	m_controlsMenu.attackMenu.weaponBlaster_action.generic.flags			= 0;
	m_controlsMenu.attackMenu.weaponBlaster_action.generic.ownerDraw		= DrawKeyBindingFunc;
	m_controlsMenu.attackMenu.weaponBlaster_action.generic.localData[0]		= ++i;
	m_controlsMenu.attackMenu.weaponBlaster_action.generic.name				= m_bindNames[m_controlsMenu.attackMenu.weaponBlaster_action.generic.localData[0]][1];
	m_controlsMenu.attackMenu.weaponBlaster_action.generic.callBack			= KeyBindingFunc;

	m_controlsMenu.attackMenu.weaponShotgun_action.generic.type				= UITYPE_ACTION;
	m_controlsMenu.attackMenu.weaponShotgun_action.generic.flags			= 0;
	m_controlsMenu.attackMenu.weaponShotgun_action.generic.ownerDraw		= DrawKeyBindingFunc;
	m_controlsMenu.attackMenu.weaponShotgun_action.generic.localData[0]		= ++i;
	m_controlsMenu.attackMenu.weaponShotgun_action.generic.name				= m_bindNames[m_controlsMenu.attackMenu.weaponShotgun_action.generic.localData[0]][1];
	m_controlsMenu.attackMenu.weaponShotgun_action.generic.callBack			= KeyBindingFunc;

	m_controlsMenu.attackMenu.weaponSuperShotgun_action.generic.type		= UITYPE_ACTION;
	m_controlsMenu.attackMenu.weaponSuperShotgun_action.generic.flags		= 0;
	m_controlsMenu.attackMenu.weaponSuperShotgun_action.generic.ownerDraw	= DrawKeyBindingFunc;
	m_controlsMenu.attackMenu.weaponSuperShotgun_action.generic.localData[0]= ++i;
	m_controlsMenu.attackMenu.weaponSuperShotgun_action.generic.name		= m_bindNames[m_controlsMenu.attackMenu.weaponSuperShotgun_action.generic.localData[0]][1];
	m_controlsMenu.attackMenu.weaponSuperShotgun_action.generic.callBack	= KeyBindingFunc;

	m_controlsMenu.attackMenu.weaponMachinegun_action.generic.type			= UITYPE_ACTION;
	m_controlsMenu.attackMenu.weaponMachinegun_action.generic.flags			= 0;
	m_controlsMenu.attackMenu.weaponMachinegun_action.generic.ownerDraw		= DrawKeyBindingFunc;
	m_controlsMenu.attackMenu.weaponMachinegun_action.generic.localData[0]	= ++i;
	m_controlsMenu.attackMenu.weaponMachinegun_action.generic.name			= m_bindNames[m_controlsMenu.attackMenu.weaponMachinegun_action.generic.localData[0]][1];
	m_controlsMenu.attackMenu.weaponMachinegun_action.generic.callBack		= KeyBindingFunc;

	m_controlsMenu.attackMenu.weaponChaingun_action.generic.type			= UITYPE_ACTION;
	m_controlsMenu.attackMenu.weaponChaingun_action.generic.flags			= 0;
	m_controlsMenu.attackMenu.weaponChaingun_action.generic.ownerDraw		= DrawKeyBindingFunc;
	m_controlsMenu.attackMenu.weaponChaingun_action.generic.localData[0]	= ++i;
	m_controlsMenu.attackMenu.weaponChaingun_action.generic.name			= m_bindNames[m_controlsMenu.attackMenu.weaponChaingun_action.generic.localData[0]][1];
	m_controlsMenu.attackMenu.weaponChaingun_action.generic.callBack		= KeyBindingFunc;

	m_controlsMenu.attackMenu.weaponGrenadeLauncher_action.generic.type			= UITYPE_ACTION;
	m_controlsMenu.attackMenu.weaponGrenadeLauncher_action.generic.flags		= 0;
	m_controlsMenu.attackMenu.weaponGrenadeLauncher_action.generic.ownerDraw	= DrawKeyBindingFunc;
	m_controlsMenu.attackMenu.weaponGrenadeLauncher_action.generic.localData[0]	= ++i;
	m_controlsMenu.attackMenu.weaponGrenadeLauncher_action.generic.name			= m_bindNames[m_controlsMenu.attackMenu.weaponGrenadeLauncher_action.generic.localData[0]][1];
	m_controlsMenu.attackMenu.weaponGrenadeLauncher_action.generic.callBack		= KeyBindingFunc;

	m_controlsMenu.attackMenu.weaponRocketLauncher_action.generic.type			= UITYPE_ACTION;
	m_controlsMenu.attackMenu.weaponRocketLauncher_action.generic.flags			= 0;
	m_controlsMenu.attackMenu.weaponRocketLauncher_action.generic.ownerDraw		= DrawKeyBindingFunc;
	m_controlsMenu.attackMenu.weaponRocketLauncher_action.generic.localData[0]	= ++i;
	m_controlsMenu.attackMenu.weaponRocketLauncher_action.generic.name			= m_bindNames[m_controlsMenu.attackMenu.weaponRocketLauncher_action.generic.localData[0]][1];
	m_controlsMenu.attackMenu.weaponRocketLauncher_action.generic.callBack		= KeyBindingFunc;

	m_controlsMenu.attackMenu.weaponHyperblaster_action.generic.type			= UITYPE_ACTION;
	m_controlsMenu.attackMenu.weaponHyperblaster_action.generic.flags			= 0;
	m_controlsMenu.attackMenu.weaponHyperblaster_action.generic.ownerDraw		= DrawKeyBindingFunc;
	m_controlsMenu.attackMenu.weaponHyperblaster_action.generic.localData[0]	= ++i;
	m_controlsMenu.attackMenu.weaponHyperblaster_action.generic.name			= m_bindNames[m_controlsMenu.attackMenu.weaponHyperblaster_action.generic.localData[0]][1];
	m_controlsMenu.attackMenu.weaponHyperblaster_action.generic.callBack		= KeyBindingFunc;

	m_controlsMenu.attackMenu.weaponRailgun_action.generic.type				= UITYPE_ACTION;
	m_controlsMenu.attackMenu.weaponRailgun_action.generic.flags			= 0;
	m_controlsMenu.attackMenu.weaponRailgun_action.generic.ownerDraw		= DrawKeyBindingFunc;
	m_controlsMenu.attackMenu.weaponRailgun_action.generic.localData[0]		= ++i;
	m_controlsMenu.attackMenu.weaponRailgun_action.generic.name				= m_bindNames[m_controlsMenu.attackMenu.weaponRailgun_action.generic.localData[0]][1];
	m_controlsMenu.attackMenu.weaponRailgun_action.generic.callBack			= KeyBindingFunc;

	m_controlsMenu.attackMenu.weaponBFG10k_action.generic.type				= UITYPE_ACTION;
	m_controlsMenu.attackMenu.weaponBFG10k_action.generic.flags				= 0;
	m_controlsMenu.attackMenu.weaponBFG10k_action.generic.ownerDraw			= DrawKeyBindingFunc;
	m_controlsMenu.attackMenu.weaponBFG10k_action.generic.localData[0]		= ++i;
	m_controlsMenu.attackMenu.weaponBFG10k_action.generic.name				= m_bindNames[m_controlsMenu.attackMenu.weaponBFG10k_action.generic.localData[0]][1];
	m_controlsMenu.attackMenu.weaponBFG10k_action.generic.callBack			= KeyBindingFunc;

	//
	// Movement menu
	//
	m_controlsMenu.movementMenu.header.generic.type			= UITYPE_ACTION;
	m_controlsMenu.movementMenu.header.generic.flags		= UIF_NOSELECT|UIF_CENTERED|UIF_MEDIUM|UIF_SHADOW;
	m_controlsMenu.movementMenu.header.generic.name			= "Movement Controls";

	m_controlsMenu.movementMenu.run_action.generic.type			= UITYPE_ACTION;
	m_controlsMenu.movementMenu.run_action.generic.flags		= 0;
	m_controlsMenu.movementMenu.run_action.generic.ownerDraw	= DrawKeyBindingFunc;
	m_controlsMenu.movementMenu.run_action.generic.localData[0]	= ++i;
	m_controlsMenu.movementMenu.run_action.generic.name			= m_bindNames[m_controlsMenu.movementMenu.run_action.generic.localData[0]][1];
	m_controlsMenu.movementMenu.run_action.generic.callBack		= KeyBindingFunc;

	m_controlsMenu.movementMenu.walk_forward_action.generic.type			= UITYPE_ACTION;
	m_controlsMenu.movementMenu.walk_forward_action.generic.flags			= 0;
	m_controlsMenu.movementMenu.walk_forward_action.generic.ownerDraw		= DrawKeyBindingFunc;
	m_controlsMenu.movementMenu.walk_forward_action.generic.localData[0]	= ++i;
	m_controlsMenu.movementMenu.walk_forward_action.generic.name			= m_bindNames[m_controlsMenu.movementMenu.walk_forward_action.generic.localData[0]][1];
	m_controlsMenu.movementMenu.walk_forward_action.generic.callBack		= KeyBindingFunc;

	m_controlsMenu.movementMenu.backpedal_action.generic.type			= UITYPE_ACTION;
	m_controlsMenu.movementMenu.backpedal_action.generic.flags			= 0;
	m_controlsMenu.movementMenu.backpedal_action.generic.ownerDraw		= DrawKeyBindingFunc;
	m_controlsMenu.movementMenu.backpedal_action.generic.localData[0]	= ++i;
	m_controlsMenu.movementMenu.backpedal_action.generic.name			= m_bindNames[m_controlsMenu.movementMenu.backpedal_action.generic.localData[0]][1];
	m_controlsMenu.movementMenu.backpedal_action.generic.callBack		= KeyBindingFunc;

	m_controlsMenu.movementMenu.move_up_action.generic.type			= UITYPE_ACTION;
	m_controlsMenu.movementMenu.move_up_action.generic.flags		= 0;
	m_controlsMenu.movementMenu.move_up_action.generic.ownerDraw	= DrawKeyBindingFunc;
	m_controlsMenu.movementMenu.move_up_action.generic.localData[0]	= ++i;
	m_controlsMenu.movementMenu.move_up_action.generic.name			= m_bindNames[m_controlsMenu.movementMenu.move_up_action.generic.localData[0]][1];
	m_controlsMenu.movementMenu.move_up_action.generic.callBack		= KeyBindingFunc;

	m_controlsMenu.movementMenu.move_down_action.generic.type			= UITYPE_ACTION;
	m_controlsMenu.movementMenu.move_down_action.generic.flags			= 0;
	m_controlsMenu.movementMenu.move_down_action.generic.ownerDraw		= DrawKeyBindingFunc;
	m_controlsMenu.movementMenu.move_down_action.generic.localData[0]	= ++i;
	m_controlsMenu.movementMenu.move_down_action.generic.name			= m_bindNames[m_controlsMenu.movementMenu.move_down_action.generic.localData[0]][1];
	m_controlsMenu.movementMenu.move_down_action.generic.callBack		= KeyBindingFunc;

	m_controlsMenu.movementMenu.turn_left_action.generic.type			= UITYPE_ACTION;
	m_controlsMenu.movementMenu.turn_left_action.generic.flags			= 0;
	m_controlsMenu.movementMenu.turn_left_action.generic.ownerDraw		= DrawKeyBindingFunc;
	m_controlsMenu.movementMenu.turn_left_action.generic.localData[0]	= ++i;
	m_controlsMenu.movementMenu.turn_left_action.generic.name			= m_bindNames[m_controlsMenu.movementMenu.turn_left_action.generic.localData[0]][1];
	m_controlsMenu.movementMenu.turn_left_action.generic.callBack		= KeyBindingFunc;

	m_controlsMenu.movementMenu.turn_right_action.generic.type			= UITYPE_ACTION;
	m_controlsMenu.movementMenu.turn_right_action.generic.flags			= 0;
	m_controlsMenu.movementMenu.turn_right_action.generic.ownerDraw		= DrawKeyBindingFunc;
	m_controlsMenu.movementMenu.turn_right_action.generic.localData[0]	= ++i;
	m_controlsMenu.movementMenu.turn_right_action.generic.name			= m_bindNames[m_controlsMenu.movementMenu.turn_right_action.generic.localData[0]][1];
	m_controlsMenu.movementMenu.turn_right_action.generic.callBack		= KeyBindingFunc;

	m_controlsMenu.movementMenu.step_left_action.generic.type			= UITYPE_ACTION;
	m_controlsMenu.movementMenu.step_left_action.generic.flags			= 0;
	m_controlsMenu.movementMenu.step_left_action.generic.ownerDraw		= DrawKeyBindingFunc;
	m_controlsMenu.movementMenu.step_left_action.generic.localData[0]	= ++i;
	m_controlsMenu.movementMenu.step_left_action.generic.name			= m_bindNames[m_controlsMenu.movementMenu.step_left_action.generic.localData[0]][1];
	m_controlsMenu.movementMenu.step_left_action.generic.callBack		= KeyBindingFunc;

	m_controlsMenu.movementMenu.step_right_action.generic.type			= UITYPE_ACTION;
	m_controlsMenu.movementMenu.step_right_action.generic.flags			= 0;
	m_controlsMenu.movementMenu.step_right_action.generic.ownerDraw		= DrawKeyBindingFunc;
	m_controlsMenu.movementMenu.step_right_action.generic.localData[0]	= ++i;
	m_controlsMenu.movementMenu.step_right_action.generic.name			= m_bindNames[m_controlsMenu.movementMenu.step_right_action.generic.localData[0]][1];
	m_controlsMenu.movementMenu.step_right_action.generic.callBack		= KeyBindingFunc;

	m_controlsMenu.movementMenu.sidestep_action.generic.type			= UITYPE_ACTION;
	m_controlsMenu.movementMenu.sidestep_action.generic.flags			= 0;
	m_controlsMenu.movementMenu.sidestep_action.generic.ownerDraw		= DrawKeyBindingFunc;
	m_controlsMenu.movementMenu.sidestep_action.generic.localData[0]	= ++i;
	m_controlsMenu.movementMenu.sidestep_action.generic.name			= m_bindNames[m_controlsMenu.movementMenu.sidestep_action.generic.localData[0]][1];
	m_controlsMenu.movementMenu.sidestep_action.generic.callBack		= KeyBindingFunc;

	m_controlsMenu.movementMenu.look_up_action.generic.type			= UITYPE_ACTION;
	m_controlsMenu.movementMenu.look_up_action.generic.flags		= 0;
	m_controlsMenu.movementMenu.look_up_action.generic.ownerDraw	= DrawKeyBindingFunc;
	m_controlsMenu.movementMenu.look_up_action.generic.localData[0]	= ++i;
	m_controlsMenu.movementMenu.look_up_action.generic.name			= m_bindNames[m_controlsMenu.movementMenu.look_up_action.generic.localData[0]][1];
	m_controlsMenu.movementMenu.look_up_action.generic.callBack		= KeyBindingFunc;

	m_controlsMenu.movementMenu.look_down_action.generic.type			= UITYPE_ACTION;
	m_controlsMenu.movementMenu.look_down_action.generic.flags			= 0;
	m_controlsMenu.movementMenu.look_down_action.generic.ownerDraw		= DrawKeyBindingFunc;
	m_controlsMenu.movementMenu.look_down_action.generic.localData[0]	= ++i;
	m_controlsMenu.movementMenu.look_down_action.generic.name			= m_bindNames[m_controlsMenu.movementMenu.look_down_action.generic.localData[0]][1];
	m_controlsMenu.movementMenu.look_down_action.generic.callBack		= KeyBindingFunc;

	m_controlsMenu.movementMenu.center_view_action.generic.type			= UITYPE_ACTION;
	m_controlsMenu.movementMenu.center_view_action.generic.flags		= 0;
	m_controlsMenu.movementMenu.center_view_action.generic.ownerDraw	= DrawKeyBindingFunc;
	m_controlsMenu.movementMenu.center_view_action.generic.localData[0]	= ++i;
	m_controlsMenu.movementMenu.center_view_action.generic.name			= m_bindNames[m_controlsMenu.movementMenu.center_view_action.generic.localData[0]][1];
	m_controlsMenu.movementMenu.center_view_action.generic.callBack		= KeyBindingFunc;

	m_controlsMenu.movementMenu.mouse_look_action.generic.type			= UITYPE_ACTION;
	m_controlsMenu.movementMenu.mouse_look_action.generic.flags			= 0;
	m_controlsMenu.movementMenu.mouse_look_action.generic.ownerDraw		= DrawKeyBindingFunc;
	m_controlsMenu.movementMenu.mouse_look_action.generic.localData[0]	= ++i;
	m_controlsMenu.movementMenu.mouse_look_action.generic.name			= m_bindNames[m_controlsMenu.movementMenu.mouse_look_action.generic.localData[0]][1];
	m_controlsMenu.movementMenu.mouse_look_action.generic.callBack		= KeyBindingFunc;

	m_controlsMenu.movementMenu.keyboard_look_action.generic.type			= UITYPE_ACTION;
	m_controlsMenu.movementMenu.keyboard_look_action.generic.flags			= 0;
	m_controlsMenu.movementMenu.keyboard_look_action.generic.ownerDraw		= DrawKeyBindingFunc;
	m_controlsMenu.movementMenu.keyboard_look_action.generic.localData[0]	= ++i;
	m_controlsMenu.movementMenu.keyboard_look_action.generic.name			= m_bindNames[m_controlsMenu.movementMenu.keyboard_look_action.generic.localData[0]][1];
	m_controlsMenu.movementMenu.keyboard_look_action.generic.callBack		= KeyBindingFunc;

	//
	// Misc menu
	//
	m_controlsMenu.miscMenu.header.generic.type			= UITYPE_ACTION;
	m_controlsMenu.miscMenu.header.generic.flags		= UIF_NOSELECT|UIF_CENTERED|UIF_MEDIUM|UIF_SHADOW;
	m_controlsMenu.miscMenu.header.generic.name			= "Misc. Controls";

	m_controlsMenu.miscMenu.chat_action.generic.type			= UITYPE_ACTION;
	m_controlsMenu.miscMenu.chat_action.generic.flags			= 0;
	m_controlsMenu.miscMenu.chat_action.generic.ownerDraw		= DrawKeyBindingFunc;
	m_controlsMenu.miscMenu.chat_action.generic.localData[0]	= ++i;
	m_controlsMenu.miscMenu.chat_action.generic.name			= m_bindNames[m_controlsMenu.miscMenu.chat_action.generic.localData[0]][1];
	m_controlsMenu.miscMenu.chat_action.generic.callBack		= KeyBindingFunc;

	m_controlsMenu.miscMenu.teamchat_action.generic.type			= UITYPE_ACTION;
	m_controlsMenu.miscMenu.teamchat_action.generic.flags			= 0;
	m_controlsMenu.miscMenu.teamchat_action.generic.ownerDraw		= DrawKeyBindingFunc;
	m_controlsMenu.miscMenu.teamchat_action.generic.localData[0]	= ++i;
	m_controlsMenu.miscMenu.teamchat_action.generic.name			= m_bindNames[m_controlsMenu.miscMenu.teamchat_action.generic.localData[0]][1];
	m_controlsMenu.miscMenu.teamchat_action.generic.callBack		= KeyBindingFunc;

	m_controlsMenu.miscMenu.inventory_action.generic.type			= UITYPE_ACTION;
	m_controlsMenu.miscMenu.inventory_action.generic.flags			= 0;
	m_controlsMenu.miscMenu.inventory_action.generic.ownerDraw		= DrawKeyBindingFunc;
	m_controlsMenu.miscMenu.inventory_action.generic.localData[0]	= ++i;
	m_controlsMenu.miscMenu.inventory_action.generic.name			= m_bindNames[m_controlsMenu.miscMenu.inventory_action.generic.localData[0]][1];
	m_controlsMenu.miscMenu.inventory_action.generic.callBack		= KeyBindingFunc;

	m_controlsMenu.miscMenu.inv_use_action.generic.type			= UITYPE_ACTION;
	m_controlsMenu.miscMenu.inv_use_action.generic.flags		= 0;
	m_controlsMenu.miscMenu.inv_use_action.generic.ownerDraw	= DrawKeyBindingFunc;
	m_controlsMenu.miscMenu.inv_use_action.generic.localData[0]	= ++i;
	m_controlsMenu.miscMenu.inv_use_action.generic.name			= m_bindNames[m_controlsMenu.miscMenu.inv_use_action.generic.localData[0]][1];
	m_controlsMenu.miscMenu.inv_use_action.generic.callBack		= KeyBindingFunc;

	m_controlsMenu.miscMenu.inv_drop_action.generic.type			= UITYPE_ACTION;
	m_controlsMenu.miscMenu.inv_drop_action.generic.flags			= 0;
	m_controlsMenu.miscMenu.inv_drop_action.generic.ownerDraw		= DrawKeyBindingFunc;
	m_controlsMenu.miscMenu.inv_drop_action.generic.localData[0]	= ++i;
	m_controlsMenu.miscMenu.inv_drop_action.generic.name			= m_bindNames[m_controlsMenu.miscMenu.inv_drop_action.generic.localData[0]][1];
	m_controlsMenu.miscMenu.inv_drop_action.generic.callBack		= KeyBindingFunc;

	m_controlsMenu.miscMenu.inv_prev_action.generic.type			= UITYPE_ACTION;
	m_controlsMenu.miscMenu.inv_prev_action.generic.flags			= 0;
	m_controlsMenu.miscMenu.inv_prev_action.generic.ownerDraw		= DrawKeyBindingFunc;
	m_controlsMenu.miscMenu.inv_prev_action.generic.localData[0]	= ++i;
	m_controlsMenu.miscMenu.inv_prev_action.generic.name			= m_bindNames[m_controlsMenu.miscMenu.inv_prev_action.generic.localData[0]][1];
	m_controlsMenu.miscMenu.inv_prev_action.generic.callBack		= KeyBindingFunc;

	m_controlsMenu.miscMenu.inv_next_action.generic.type			= UITYPE_ACTION;
	m_controlsMenu.miscMenu.inv_next_action.generic.flags			= 0;
	m_controlsMenu.miscMenu.inv_next_action.generic.ownerDraw		= DrawKeyBindingFunc;
	m_controlsMenu.miscMenu.inv_next_action.generic.localData[0]	= ++i;
	m_controlsMenu.miscMenu.inv_next_action.generic.name			= m_bindNames[m_controlsMenu.miscMenu.inv_next_action.generic.localData[0]][1];
	m_controlsMenu.miscMenu.inv_next_action.generic.callBack		= KeyBindingFunc;

	m_controlsMenu.miscMenu.help_computer_action.generic.type			= UITYPE_ACTION;
	m_controlsMenu.miscMenu.help_computer_action.generic.flags			= 0;
	m_controlsMenu.miscMenu.help_computer_action.generic.ownerDraw		= DrawKeyBindingFunc;
	m_controlsMenu.miscMenu.help_computer_action.generic.localData[0]	= ++i;
	m_controlsMenu.miscMenu.help_computer_action.generic.name			= m_bindNames[m_controlsMenu.miscMenu.help_computer_action.generic.localData[0]][1];
	m_controlsMenu.miscMenu.help_computer_action.generic.callBack		= KeyBindingFunc;

	m_controlsMenu.back_action.generic.type			= UITYPE_ACTION;
	m_controlsMenu.back_action.generic.flags		= UIF_CENTERED|UIF_LARGE|UIF_SHADOW;
	m_controlsMenu.back_action.generic.name			= "< Back";
	m_controlsMenu.back_action.generic.callBack		= Menu_Pop;
	m_controlsMenu.back_action.generic.statusBar	= "Back a menu";

	ControlsMenu_AddItems ();
}


/*
=============
ControlsMenu_Close
=============
*/
static struct sfx_t *ControlsMenu_Close ()
{
	return uiMedia.sounds.menuOut;
}


/*
=============
ControlsMenu_Draw
=============
*/
static void ControlsMenu_Draw ()
{
	float	y;

	// Initialize if necessary
	if (!m_controlsMenu.frameWork.initialized)
		ControlsMenu_Init ();

	// Dynamically position
	m_controlsMenu.frameWork.x				= cg.refConfig.vidWidth * 0.5f;
	m_controlsMenu.frameWork.y				= 0;

	m_controlsMenu.banner.generic.x			= 0;
	m_controlsMenu.banner.generic.y			= 0;

	y = m_controlsMenu.banner.height * UI_SCALE;

	m_controlsMenu.attackMenu_action.generic.x							= 0;
	m_controlsMenu.attackMenu_action.generic.y							= y += UIFT_SIZEINC;
	m_controlsMenu.movementMenu_action.generic.x						= 0;
	m_controlsMenu.movementMenu_action.generic.y						= y += UIFT_SIZEINC;
	m_controlsMenu.miscMenu_action.generic.x							= 0;
	m_controlsMenu.miscMenu_action.generic.y							= y += UIFT_SIZEINC;

	y += UIFT_SIZEINC;

	switch (m_controlsMenu.currMenu) {
	case CONTROLS_ATTACK:
		// Attack
		m_controlsMenu.attackMenu.header.generic.x							= 0;
		m_controlsMenu.attackMenu.header.generic.y							= y += UIFT_SIZEINC;

		m_controlsMenu.attackMenu.attack_action.generic.x					= 0;
		m_controlsMenu.attackMenu.attack_action.generic.y					= y += UIFT_SIZEINC + UIFT_SIZEINCMED;
		m_controlsMenu.attackMenu.prev_weapon_action.generic.x				= 0;
		m_controlsMenu.attackMenu.prev_weapon_action.generic.y				= y += UIFT_SIZEINC;
		m_controlsMenu.attackMenu.next_weapon_action.generic.x				= 0;
		m_controlsMenu.attackMenu.next_weapon_action.generic.y				= y += UIFT_SIZEINC;

		m_controlsMenu.attackMenu.weaponHeader.generic.x					= 0;
		m_controlsMenu.attackMenu.weaponHeader.generic.y					= y += UIFT_SIZEINC + UIFT_SIZEINCMED;

		m_controlsMenu.attackMenu.weaponBlaster_action.generic.x			= 0;
		m_controlsMenu.attackMenu.weaponBlaster_action.generic.y			= y += UIFT_SIZEINC + UIFT_SIZEINCMED;
		m_controlsMenu.attackMenu.weaponShotgun_action.generic.x			= 0;
		m_controlsMenu.attackMenu.weaponShotgun_action.generic.y			= y += UIFT_SIZEINC;
		m_controlsMenu.attackMenu.weaponSuperShotgun_action.generic.x		= 0;
		m_controlsMenu.attackMenu.weaponSuperShotgun_action.generic.y		= y += UIFT_SIZEINC;
		m_controlsMenu.attackMenu.weaponMachinegun_action.generic.x			= 0;
		m_controlsMenu.attackMenu.weaponMachinegun_action.generic.y			= y += UIFT_SIZEINC;
		m_controlsMenu.attackMenu.weaponChaingun_action.generic.x			= 0;
		m_controlsMenu.attackMenu.weaponChaingun_action.generic.y			= y += UIFT_SIZEINC;
		m_controlsMenu.attackMenu.weaponGrenadeLauncher_action.generic.x	= 0;
		m_controlsMenu.attackMenu.weaponGrenadeLauncher_action.generic.y	= y += UIFT_SIZEINC;
		m_controlsMenu.attackMenu.weaponRocketLauncher_action.generic.x		= 0;
		m_controlsMenu.attackMenu.weaponRocketLauncher_action.generic.y		= y += UIFT_SIZEINC;
		m_controlsMenu.attackMenu.weaponHyperblaster_action.generic.x		= 0;
		m_controlsMenu.attackMenu.weaponHyperblaster_action.generic.y		= y += UIFT_SIZEINC;
		m_controlsMenu.attackMenu.weaponRailgun_action.generic.x			= 0;
		m_controlsMenu.attackMenu.weaponRailgun_action.generic.y			= y += UIFT_SIZEINC;
		m_controlsMenu.attackMenu.weaponBFG10k_action.generic.x				= 0;
		m_controlsMenu.attackMenu.weaponBFG10k_action.generic.y				= y += UIFT_SIZEINC;
		break;

	case CONTROLS_MOVEMENT:
		// Movement menu
		m_controlsMenu.movementMenu.header.generic.x				= 0;
		m_controlsMenu.movementMenu.header.generic.y				= y += UIFT_SIZEINC;

		m_controlsMenu.movementMenu.run_action.generic.x			= 0;
		m_controlsMenu.movementMenu.run_action.generic.y			= y += UIFT_SIZEINC + UIFT_SIZEINCMED;
		m_controlsMenu.movementMenu.walk_forward_action.generic.x	= 0;
		m_controlsMenu.movementMenu.walk_forward_action.generic.y	= y += UIFT_SIZEINC;
		m_controlsMenu.movementMenu.backpedal_action.generic.x		= 0;
		m_controlsMenu.movementMenu.backpedal_action.generic.y		= y += UIFT_SIZEINC;
		m_controlsMenu.movementMenu.move_up_action.generic.x		= 0;
		m_controlsMenu.movementMenu.move_up_action.generic.y		= y += UIFT_SIZEINC;
		m_controlsMenu.movementMenu.move_down_action.generic.x		= 0;
		m_controlsMenu.movementMenu.move_down_action.generic.y		= y += UIFT_SIZEINC;
		m_controlsMenu.movementMenu.turn_left_action.generic.x		= 0;
		m_controlsMenu.movementMenu.turn_left_action.generic.y		= y += UIFT_SIZEINC;
		m_controlsMenu.movementMenu.turn_right_action.generic.x		= 0;
		m_controlsMenu.movementMenu.turn_right_action.generic.y		= y += UIFT_SIZEINC;
		m_controlsMenu.movementMenu.step_left_action.generic.x		= 0;
		m_controlsMenu.movementMenu.step_left_action.generic.y		= y += (UIFT_SIZEINC*2);
		m_controlsMenu.movementMenu.step_right_action.generic.x		= 0;
		m_controlsMenu.movementMenu.step_right_action.generic.y		= y += UIFT_SIZEINC;
		m_controlsMenu.movementMenu.sidestep_action.generic.x		= 0;
		m_controlsMenu.movementMenu.sidestep_action.generic.y		= y += UIFT_SIZEINC;
		m_controlsMenu.movementMenu.look_up_action.generic.x		= 0;
		m_controlsMenu.movementMenu.look_up_action.generic.y		= y += (UIFT_SIZEINC*2);
		m_controlsMenu.movementMenu.look_down_action.generic.x		= 0;
		m_controlsMenu.movementMenu.look_down_action.generic.y		= y += UIFT_SIZEINC;
		m_controlsMenu.movementMenu.center_view_action.generic.x	= 0;
		m_controlsMenu.movementMenu.center_view_action.generic.y	= y += UIFT_SIZEINC;
		m_controlsMenu.movementMenu.mouse_look_action.generic.x		= 0;
		m_controlsMenu.movementMenu.mouse_look_action.generic.y		= y += UIFT_SIZEINC;
		m_controlsMenu.movementMenu.keyboard_look_action.generic.x	= 0;
		m_controlsMenu.movementMenu.keyboard_look_action.generic.y	= y += UIFT_SIZEINC;
		break;

	case CONTROLS_MISC:
		// Misc menu
		m_controlsMenu.miscMenu.header.generic.x				= 0;
		m_controlsMenu.miscMenu.header.generic.y				= y += UIFT_SIZEINC;

		m_controlsMenu.miscMenu.chat_action.generic.x			= 0;
		m_controlsMenu.miscMenu.chat_action.generic.y			= y += UIFT_SIZEINC + UIFT_SIZEINCMED;
		m_controlsMenu.miscMenu.teamchat_action.generic.x		= 0;
		m_controlsMenu.miscMenu.teamchat_action.generic.y		= y += UIFT_SIZEINC;

		m_controlsMenu.miscMenu.inventory_action.generic.x		= 0;
		m_controlsMenu.miscMenu.inventory_action.generic.y		= y += UIFT_SIZEINC + UIFT_SIZEINCMED;
		m_controlsMenu.miscMenu.inv_use_action.generic.x		= 0;
		m_controlsMenu.miscMenu.inv_use_action.generic.y		= y += UIFT_SIZEINC;
		m_controlsMenu.miscMenu.inv_drop_action.generic.x		= 0;
		m_controlsMenu.miscMenu.inv_drop_action.generic.y		= y += UIFT_SIZEINC;
		m_controlsMenu.miscMenu.inv_prev_action.generic.x		= 0;
		m_controlsMenu.miscMenu.inv_prev_action.generic.y		= y += UIFT_SIZEINC;
		m_controlsMenu.miscMenu.inv_next_action.generic.x		= 0;
		m_controlsMenu.miscMenu.inv_next_action.generic.y		= y += UIFT_SIZEINC;

		m_controlsMenu.miscMenu.help_computer_action.generic.x	= 0;
		m_controlsMenu.miscMenu.help_computer_action.generic.y	= y += (UIFT_SIZEINC*2);
		break;
	}

	m_controlsMenu.back_action.generic.x			= 0;
	m_controlsMenu.back_action.generic.y			= y += UIFT_SIZEINC + UIFT_SIZEINCLG;

	// Render
	UI_DrawInterface (&m_controlsMenu.frameWork);
}


/*
=============
ControlsMenu_Key
=============
*/
static struct sfx_t *ControlsMenu_Key (uiFrameWork_t *fw, keyNum_t keyNum)
{
	char	cmd[1024];

	uiAction_t *item = (uiAction_t *) UI_ItemAtCursor (fw);

	if (uiState.cursorLock) {
		switch (keyNum) {
		case K_ESCAPE:
		case '`':
		case '~':
			M_UnbindCommand (m_bindNames[item->generic.localData[0]][0]);
			break;

		default:
			Q_snprintfz (cmd, sizeof(cmd), "bind \"%s\" \"%s\"\n", cgi.Key_KeynumToString(keyNum), m_bindNames[item->generic.localData[0]][0]);
			cgi.Cbuf_InsertText (cmd);
		}
		
		m_controlsMenu.frameWork.statusBar = "[ENTER] to change, [BACKSPACE] to clear";
		uiState.cursorLock = false;
		return uiMedia.sounds.menuOut;
	}

	return UI_DefaultKeyFunc (fw, keyNum);
}


/*
=============
UI_ControlsMenu_f
=============
*/
void UI_ControlsMenu_f ()
{
	ControlsMenu_Init ();

	m_controlsMenu.frameWork.statusBar = "[ENTER] to change, [BACKSPACE] to clear";
	M_PushMenu (&m_controlsMenu.frameWork, ControlsMenu_Draw, ControlsMenu_Close, ControlsMenu_Key);
}
