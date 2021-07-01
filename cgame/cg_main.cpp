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
// cg_main.c
//

#include "cg_local.h"

cgState_t	cg;
cgMedia_t	cgMedia;
uiMedia_t	uiMedia;

cVar_t	*cg_advInfrared;
cVar_t	*cg_brassTime;
cVar_t	*cg_decals;
cVar_t	*cg_decalBurnLife;
cVar_t	*cg_decalFadeTime;
cVar_t	*cg_decalLife;
cVar_t	*cg_decalLOD;
cVar_t	*cg_decalMax;
cVar_t	*cg_mapEffects;
cVar_t	*cl_add_particles;
cVar_t	*cg_particleCulling;
cVar_t	*cg_particleGore;
cVar_t	*cg_particleMax;
cVar_t	*cg_particleShading;
cVar_t	*cg_particleSmokeLinger;
cVar_t	*cg_railCoreRed;
cVar_t	*cg_railCoreGreen;
cVar_t	*cg_railCoreBlue;
cVar_t	*cg_railSpiral;
cVar_t	*cg_railSpiralRed;
cVar_t	*cg_railSpiralGreen;
cVar_t	*cg_railSpiralBlue;
cVar_t	*cg_thirdPerson;
cVar_t	*cg_thirdPersonAngle;
cVar_t	*cg_thirdPersonClip;
cVar_t	*cg_thirdPersonDist;
cVar_t	*cg_simpleitems;

cVar_t	*cg_explorattle;
cVar_t	*cg_explorattle_scale;
cVar_t	*cl_footsteps;
cVar_t	*cl_gun;
cVar_t	*cl_noskins;
cVar_t	*cl_predict;
cVar_t	*cl_showmiss;
cVar_t	*cl_vwep;

cVar_t	*gender_auto;
cVar_t	*gender;
cVar_t	*hand;
cVar_t	*skin;

cVar_t	*cl_testblend;
cVar_t	*cl_testentities;
cVar_t	*cl_testlights;
cVar_t	*cl_testparticles;

cVar_t	*r_hudScale;
cVar_t	*r_fontScale;

cVar_t	*crosshair;
cVar_t	*ch_alpha;
cVar_t	*ch_pulse;
cVar_t	*ch_scale;
cVar_t	*ch_red;
cVar_t	*ch_green;
cVar_t	*ch_blue;
cVar_t	*ch_xOffset;
cVar_t	*ch_yOffset;

cVar_t	*cl_showfps;
cVar_t	*cl_showping;
cVar_t	*cl_showtime;

cVar_t	*con_chatHud;
cVar_t	*con_chatHudLines;
cVar_t	*con_chatHudPosX;
cVar_t	*con_chatHudPosY;
cVar_t	*con_chatHudShadow;
cVar_t	*con_notifyfade;
cVar_t	*con_notifylarge;
cVar_t	*con_notifylines;
cVar_t	*con_notifytime;
cVar_t	*con_alpha;
cVar_t	*con_clock;
cVar_t	*con_drop;
cVar_t	*con_scroll;

cVar_t	*scr_conspeed;
cVar_t	*scr_centertime;
cVar_t	*scr_showpause;
cVar_t	*scr_hudalpha;

cVar_t	*scr_netgraph;
cVar_t	*scr_timegraph;
cVar_t	*scr_debuggraph;
cVar_t	*scr_graphheight;
cVar_t	*scr_graphscale;
cVar_t	*scr_graphshift;
cVar_t	*scr_graphalpha;

cVar_t	*viewsize;
cVar_t	*gl_polyblend;

cVar_t	*cg_paused;
cVar_t	*cg_timeDemo;

cVar_t	*cg_bobSpeed;
cVar_t	*cg_bobPitch;
cVar_t	*cg_bobYaw;
cVar_t	*cg_bobRoll;
cVar_t	*cg_hud;

// ====================================================================

/*
==================
CG_SetRefConfig
==================
*/
void CG_SetRefConfig (refConfig_t *inConfig)
{
	cg.refConfig = *inConfig;

	// Force a cg.hudScale update
	r_hudScale->modified = true;
}


/*
=================
CG_UpdateCvars
=================
*/
void CG_UpdateCvars(const bool bForceUpdate)
{
	// HUD scale
	if (r_hudScale->modified || bForceUpdate)
	{
		r_hudScale->modified = false;
		if (r_hudScale->floatVal <= 0)
			cgi.Cvar_VariableSetValue(r_hudScale, 1, true);

		cg.hudScale[0] = r_hudScale->floatVal;
		cg.hudScale[1] = r_hudScale->floatVal;
	}

	// cg_brassTime
	if (cg_brassTime->modified)
	{
		cg_brassTime->modified = false;
		if (cg_brassTime->floatVal < 0)
			cgi.Cvar_VariableSetValue(cg_brassTime, 0, true);
	}

	// cg_decalBurnLife
	if (cg_decalBurnLife->modified)
	{
		cg_decalBurnLife->modified = false;
		if (cg_decalBurnLife->floatVal < 0)
			cgi.Cvar_VariableSetValue(cg_decalBurnLife, 0, true);
	}

	// cg_decalFadeTime
	if (cg_decalFadeTime->modified)
	{
		cg_decalFadeTime->modified = false;
		if (cg_decalFadeTime->floatVal < 0)
			cgi.Cvar_VariableSetValue(cg_decalFadeTime, 0, true);
	}

	// cg_decalLife
	if (cg_decalLife->modified)
	{
		cg_decalLife->modified = false;
		if (cg_decalLife->floatVal < 0)
			cgi.Cvar_VariableSetValue(cg_decalLife, 0, true);
	}

	// cg_decalMax
	if (cg_decalMax->modified)
	{
		cg_decalMax->modified = false;
		if (cg_decalMax->intVal > MAX_REF_DECALS)
			cgi.Cvar_VariableSetValue(cg_decalMax, MAX_REF_DECALS, true);
		else if (cg_decalMax->intVal < 0)
			cgi.Cvar_VariableSetValue(cg_decalMax, 0, true);
	}

	// cg_particleMax
	if (cg_particleMax->modified)
	{
		cg_particleMax->modified = false;
		if (cg_particleMax->intVal > MAX_PARTICLES)
			cgi.Cvar_VariableSetValue(cg_particleMax, MAX_PARTICLES, true);
		else if (cg_particleMax->intVal < 0)
			cgi.Cvar_VariableSetValue(cg_particleMax, 0, true);
	}

	// cg_particleGore
	if (cg_particleGore->modified || bForceUpdate)
	{
		cg_particleGore->modified = false;
		if (cg_particleGore->floatVal < 0.0f)
			cgi.Cvar_VariableSetValue(cg_particleGore, 0.0f, true);
		else if (cg_particleGore->floatVal > 10.0f)
			cgi.Cvar_VariableSetValue(cg_particleGore, 10.0f, true);

		// 0.0-10.0 -> 0.0-1.0
		cg.goreScale = cg_particleGore->floatVal * 0.1f;
	}

	// cg_particleSmokeLinger
	if (cg_particleSmokeLinger->modified || bForceUpdate)
	{
		cg_particleSmokeLinger->modified = false;
		if (cg_particleSmokeLinger->floatVal < 0.0f)
			cgi.Cvar_VariableSetValue(cg_particleSmokeLinger, 0.0f, true);
		else if (cg_particleSmokeLinger->floatVal > 10.0f)
			cgi.Cvar_VariableSetValue(cg_particleSmokeLinger, 10.0f, true);

		// 0.0-10.0 -> 0.0-1.0
		cg.smokeLingerScale = cg_particleSmokeLinger->floatVal;
	}
}

/*
=======================================================================

	CONSOLE FUNCTIONS

=======================================================================
*/

/*
=================
CG_Skins_f

Load or download any custom player skins and models
=================
*/
static void CG_Skins_f ()
{
	int		i;

	for (i=0 ; i<MAX_CS_CLIENTS ; i++) {
		if (!cg.configStrings[CS_PLAYERSKINS+i][0])
			continue;

		Com_Printf (0, "client %i: %s\n", i, cg.configStrings[CS_PLAYERSKINS+i]); 
		cgi.R_UpdateScreen ();
		cgi.Sys_SendKeyEvents ();	// pump message loop
		CG_ParseClientinfo (i);
	}
}


/*
=================
CG_ThirdPerson_f
=================
*/
static void CG_ThirdPerson_f ()
{
	cgi.Cvar_SetValue ("cg_thirdPerson", !cg_thirdPerson->intVal, false);
}

/*
=======================================================================

	INIT / SHUTDOWN

=======================================================================
*/

static conCmd_t	*cmd_skins;
static conCmd_t	*cmd_thirdPerson;
static conCmd_t *cmd_say;
static conCmd_t *cmd_say_team;
static conCmd_t *cmd_wave;
static conCmd_t *cmd_inven;
static conCmd_t *cmd_kill;
static conCmd_t *cmd_use;
static conCmd_t *cmd_drop;
static conCmd_t *cmd_info;
static conCmd_t *cmd_prog;
static conCmd_t *cmd_give;
static conCmd_t *cmd_god;
static conCmd_t *cmd_notarget;
static conCmd_t *cmd_noclip;
static conCmd_t *cmd_invuse;
static conCmd_t *cmd_invprev;
static conCmd_t *cmd_invnext;
static conCmd_t *cmd_invdrop;
static conCmd_t *cmd_weapnext;
static conCmd_t *cmd_weapprev;

/*
=================
CG_CopyConfigStrings

This is necessary to make certain configstrings are consistant between
CGame and the client. Sometimes while connecting a configstring is sent
just before the CGame module is loaded.
=================
*/
void CG_CopyConfigStrings ()
{
	int		num;

	for (num=0 ; num<MAX_CFGSTRINGS ; num++) {
		cgi.GetConfigString (num, cg.configStrings[num], MAX_CFGSTRLEN);

		CG_ParseConfigString (num, cg.configStrings[num]);
	}
}


/*
==================
CG_RegisterMain
==================
*/
static void CG_RegisterMain ()
{
	cg_advInfrared			= cgi.Cvar_Register ("cg_advInfrared",			"1",			CVAR_ARCHIVE);
	cg_brassTime			= cgi.Cvar_Register ("cg_brassTime",			"10",			CVAR_ARCHIVE);
	cg_decals				= cgi.Cvar_Register ("cg_decals",				"1",			CVAR_ARCHIVE);
	cg_decalBurnLife		= cgi.Cvar_Register ("cg_decalBurnLife",		"10",			CVAR_ARCHIVE);
	cg_decalFadeTime		= cgi.Cvar_Register ("cg_decalFadeTime",		"1",			CVAR_ARCHIVE);
	cg_decalLife			= cgi.Cvar_Register ("cg_decalLife",			"15",			CVAR_ARCHIVE);
	cg_decalLOD				= cgi.Cvar_Register ("cg_decalLOD",				"1",			CVAR_ARCHIVE);
	cg_decalMax				= cgi.Cvar_Register ("cg_decalMax",				"4096",			CVAR_ARCHIVE);
	cg_mapEffects			= cgi.Cvar_Register ("cg_mapEffects",			"1",			CVAR_ARCHIVE);
	cl_add_particles		= cgi.Cvar_Register ("cl_particles",			"1",			0);
	cg_particleCulling		= cgi.Cvar_Register ("cg_particleCulling",		"1",			CVAR_ARCHIVE);
	cg_particleGore			= cgi.Cvar_Register ("cg_particleGore",			"3",			CVAR_ARCHIVE);
	cg_particleMax			= cgi.Cvar_Register ("cg_particleMax",			"8192",			CVAR_ARCHIVE);
	cg_particleShading		= cgi.Cvar_Register ("cg_particleShading",		"1",			CVAR_ARCHIVE);
	cg_particleSmokeLinger	= cgi.Cvar_Register ("cg_particleSmokeLinger",	"0.6",			CVAR_ARCHIVE);
	cg_railCoreRed			= cgi.Cvar_Register ("cg_railCoreRed",			"0.75",			CVAR_ARCHIVE);
	cg_railCoreGreen		= cgi.Cvar_Register ("cg_railCoreGreen",		"1",			CVAR_ARCHIVE);
	cg_railCoreBlue			= cgi.Cvar_Register ("cg_railCoreBlue",			"1",			CVAR_ARCHIVE);
	cg_railSpiral			= cgi.Cvar_Register ("cg_railSpiral",			"1",			CVAR_ARCHIVE);
	cg_railSpiralRed		= cgi.Cvar_Register ("cg_railSpiralRed",		"0",			CVAR_ARCHIVE);
	cg_railSpiralGreen		= cgi.Cvar_Register ("cg_railSpiralGreen",		"0.5",			CVAR_ARCHIVE);
	cg_railSpiralBlue		= cgi.Cvar_Register ("cg_railSpiralBlue",		"0.9",			CVAR_ARCHIVE);

	cg_thirdPerson		= cgi.Cvar_Register ("cg_thirdPerson",			"0",			CVAR_ARCHIVE);
	cg_thirdPersonAngle		= cgi.Cvar_Register ("cg_thirdPersonAngle",		"30",			CVAR_ARCHIVE);
	cg_thirdPersonClip		= cgi.Cvar_Register ("cg_thirdPersonClip",		"1",			CVAR_ARCHIVE);
	cg_thirdPersonDist		= cgi.Cvar_Register ("cg_thirdPersonDist",		"50",			CVAR_ARCHIVE);
	cg_simpleitems			= cgi.Cvar_Register ("cg_simpleitems",			"0",			CVAR_ARCHIVE);

	cg_explorattle			= cgi.Cvar_Register ("cg_explorattle",			"1",			CVAR_ARCHIVE);
	cg_explorattle_scale	= cgi.Cvar_Register ("cg_explorattle_scale",	"0.3",			CVAR_ARCHIVE);
	cl_footsteps			= cgi.Cvar_Register ("cl_footsteps",			"1",			0);
	cl_gun					= cgi.Cvar_Register ("cl_gun",					"1",			0);
	cl_noskins				= cgi.Cvar_Register ("cl_noskins",				"0",			CVAR_CHEAT);
	cl_predict				= cgi.Cvar_Register ("cl_predict",				"1",			0);
	cl_showmiss				= cgi.Cvar_Register ("cl_showmiss",				"0",			0);
	cl_vwep					= cgi.Cvar_Register ("cl_vwep",					"1",			CVAR_ARCHIVE);

	gender_auto				= cgi.Cvar_Register ("gender_auto",				"1",			CVAR_ARCHIVE);
	gender					= cgi.Cvar_Register ("gender",					"male",			CVAR_USERINFO|CVAR_ARCHIVE);
	hand					= cgi.Cvar_Register ("hand",					"0",			CVAR_USERINFO|CVAR_ARCHIVE);
	skin					= cgi.Cvar_Register ("skin",					"male/grunt",	CVAR_USERINFO|CVAR_ARCHIVE);

	cl_testblend			= cgi.Cvar_Register ("cl_testblend",			"0",			CVAR_CHEAT);
	cl_testentities			= cgi.Cvar_Register ("cl_testentities",			"0",			CVAR_CHEAT);
	cl_testlights			= cgi.Cvar_Register ("cl_testlights",			"0",			CVAR_CHEAT);
	cl_testparticles		= cgi.Cvar_Register ("cl_testparticles",		"0",			CVAR_CHEAT);

	r_hudScale				= cgi.Cvar_Register ("r_hudScale",				"1",			CVAR_ARCHIVE);
	r_fontScale				= cgi.Cvar_Register ("r_fontScale",				"1",			CVAR_ARCHIVE);

	if (cg.currGameMod == GAME_MOD_DDAY)
		crosshair			= cgi.Cvar_Register ("crosshair",				"0",			CVAR_ARCHIVE|CVAR_CHEAT);
	else
		crosshair			= cgi.Cvar_Register ("crosshair",				"0",			CVAR_ARCHIVE);
	ch_alpha				= cgi.Cvar_Register ("ch_alpha",				"1",			CVAR_ARCHIVE);
	ch_pulse				= cgi.Cvar_Register ("ch_pulse",				"1",			CVAR_ARCHIVE);
	ch_scale				= cgi.Cvar_Register ("ch_scale",				"1",			CVAR_ARCHIVE);
	ch_red					= cgi.Cvar_Register ("ch_red",					"1",			CVAR_ARCHIVE);
	ch_green				= cgi.Cvar_Register ("ch_green",				"1",			CVAR_ARCHIVE);
	ch_blue					= cgi.Cvar_Register ("ch_blue",					"1",			CVAR_ARCHIVE);
	ch_xOffset				= cgi.Cvar_Register ("ch_xOffset",				"0",			CVAR_ARCHIVE);
	ch_yOffset				= cgi.Cvar_Register ("ch_yOffset",				"0",			CVAR_ARCHIVE);

	cl_showfps				= cgi.Cvar_Register ("cl_showfps",				"1",			CVAR_ARCHIVE);
	cl_showping				= cgi.Cvar_Register ("cl_showping",				"1",			CVAR_ARCHIVE);
	cl_showtime				= cgi.Cvar_Register ("cl_showtime",				"1",			CVAR_ARCHIVE);

	con_chatHud				= cgi.Cvar_Register ("con_chatHud",				"0",			CVAR_ARCHIVE);
	con_chatHudLines		= cgi.Cvar_Register ("con_chatHudLines",		"4",			CVAR_ARCHIVE);
	con_chatHudPosX			= cgi.Cvar_Register ("con_chatHudPosX",			"8",			CVAR_ARCHIVE);
	con_chatHudPosY			= cgi.Cvar_Register ("con_chatHudPosY",			"-14",			CVAR_ARCHIVE);
	con_chatHudShadow		= cgi.Cvar_Register ("con_chatHudShadow",		"0",			CVAR_ARCHIVE);
	con_notifyfade			= cgi.Cvar_Register ("con_notifyfade",			"1",			CVAR_ARCHIVE);
	con_notifylarge			= cgi.Cvar_Register ("con_notifylarge",			"0",			CVAR_ARCHIVE);
	con_notifylines			= cgi.Cvar_Register ("con_notifylines",			"4",			CVAR_ARCHIVE);
	con_notifytime			= cgi.Cvar_Register ("con_notifytime",			"3",			CVAR_ARCHIVE);
	con_alpha				= cgi.Cvar_Register ("con_alpha",				"1",			CVAR_ARCHIVE);
	con_clock				= cgi.Cvar_Register ("con_clock",				"1",			CVAR_ARCHIVE);
	con_drop				= cgi.Cvar_Register ("con_drop",				"0.5",			CVAR_ARCHIVE);
	con_scroll				= cgi.Cvar_Register ("con_scroll",				"2",			CVAR_ARCHIVE);

	scr_conspeed			= cgi.Cvar_Register ("scr_conspeed",			"3",			0);
	scr_centertime			= cgi.Cvar_Register ("scr_centertime",			"2.5",			0);
	scr_showpause			= cgi.Cvar_Register ("scr_showpause",			"1",			0);
	scr_hudalpha			= cgi.Cvar_Register ("scr_hudalpha",			"1",			CVAR_ARCHIVE);

	scr_netgraph			= cgi.Cvar_Register ("netgraph",				"0",			0);
	scr_timegraph			= cgi.Cvar_Register ("timegraph",				"0",			0);
	scr_debuggraph			= cgi.Cvar_Register ("debuggraph",				"0",			0);
	scr_graphheight			= cgi.Cvar_Register ("graphheight",				"32",			0);
	scr_graphscale			= cgi.Cvar_Register ("graphscale",				"1",			0);
	scr_graphshift			= cgi.Cvar_Register ("graphshift",				"0",			0);
	scr_graphalpha			= cgi.Cvar_Register ("scr_graphalpha",			"0.6",			CVAR_ARCHIVE);

	viewsize				= cgi.Cvar_Register ("viewsize",				"100",			CVAR_ARCHIVE);

	gl_polyblend			= cgi.Cvar_Register ("gl_polyblend",			"1",			0);

	cg_paused				= cgi.Cvar_Exists("paused");
	cg_timeDemo				= cgi.Cvar_Exists("timedemo");

	cg_bobSpeed				= cgi.Cvar_Register ("cg_bobSpeed", "2.5", CVAR_ARCHIVE);
	cg_bobPitch				= cgi.Cvar_Register ("cg_bobPitch", "6", CVAR_ARCHIVE);
	cg_bobYaw				= cgi.Cvar_Register ("cg_bobYaw", "3", CVAR_ARCHIVE);
	cg_bobRoll				= cgi.Cvar_Register ("cg_bobRoll", "6", CVAR_ARCHIVE);

	gender->modified = false; // clear this so we know when user sets it manually

	cmd_skins		= cgi.Cmd_AddCommand ("skins",			0, CG_Skins_f,			"Lists skins of players connected");
	cmd_thirdPerson	= cgi.Cmd_AddCommand ("thirdPerson",	0, CG_ThirdPerson_f,	"Toggles the third person camera");

	// Userinfo cvars
	cgi.Cvar_Register ("fov",			"90",			CVAR_USERINFO|CVAR_ARCHIVE);
	cgi.Cvar_Register ("password",		"",				CVAR_USERINFO);
	cgi.Cvar_Register ("spectator",		"0",			CVAR_USERINFO);
	cgi.Cvar_Register ("msg",			"0",			CVAR_USERINFO|CVAR_ARCHIVE);
	cgi.Cvar_Register ("name",			"unnamed",		CVAR_USERINFO|CVAR_ARCHIVE);
	cgi.Cvar_Register ("rate",			"8000",			CVAR_USERINFO|CVAR_ARCHIVE);
	cgi.Cvar_Register ("gender",		"male",			CVAR_USERINFO|CVAR_ARCHIVE);
	cgi.Cvar_Register ("hand",			"0",			CVAR_USERINFO|CVAR_ARCHIVE);
	cgi.Cvar_Register ("skin",			"",				CVAR_USERINFO|CVAR_ARCHIVE);

	cg_hud					= cgi.Cvar_Register ("cg_hud", "default", CVAR_ARCHIVE);

	// Register our commands
	cmd_say			= cgi.Cmd_AddCommand ("say",			0, CG_Say_Preprocessor,	"");
	cmd_say_team	= cgi.Cmd_AddCommand ("say_team",		0, CG_Say_Preprocessor,	"");

	/*
	** Forward to server commands...
	** The only thing this does is allow command completion to work -- all unknown
	** commands are automatically forwarded to the server
	*/
	cmd_wave		= cgi.Cmd_AddCommand ("wave",			0, NULL,					"");
	cmd_inven		= cgi.Cmd_AddCommand ("inven",			0, NULL,					"");
	cmd_kill		= cgi.Cmd_AddCommand ("kill",			0, NULL,					"");
	cmd_use			= cgi.Cmd_AddCommand ("use",			0, NULL,					"");
	cmd_drop		= cgi.Cmd_AddCommand ("drop",			0, NULL,					"");
	cmd_info		= cgi.Cmd_AddCommand ("info",			0, NULL,					"");
	cmd_prog		= cgi.Cmd_AddCommand ("prog",			0, NULL,					"");
	cmd_give		= cgi.Cmd_AddCommand ("give",			0, NULL,					"");
	cmd_god			= cgi.Cmd_AddCommand ("god",			0, NULL,					"");
	cmd_notarget	= cgi.Cmd_AddCommand ("notarget",		0, NULL,					"");
	cmd_noclip		= cgi.Cmd_AddCommand ("noclip",			0, NULL,					"");
	cmd_invuse		= cgi.Cmd_AddCommand ("invuse",			0, NULL,					"");
	cmd_invprev		= cgi.Cmd_AddCommand ("invprev",		0, NULL,					"");
	cmd_invnext		= cgi.Cmd_AddCommand ("invnext",		0, NULL,					"");
	cmd_invdrop		= cgi.Cmd_AddCommand ("invdrop",		0, NULL,					"");
	cmd_weapnext	= cgi.Cmd_AddCommand ("weapnext",		0, NULL,					"");
	cmd_weapprev	= cgi.Cmd_AddCommand ("weapprev",		0, NULL,					"");
}


/*
==================
CG_RemoveCmds
==================
*/
static void CG_RemoveCmds ()
{
	cgi.Cmd_RemoveCommand(cmd_skins);
	cgi.Cmd_RemoveCommand(cmd_thirdPerson);

	cgi.Cmd_RemoveCommand(cmd_say);
	cgi.Cmd_RemoveCommand(cmd_say_team);
	cgi.Cmd_RemoveCommand(cmd_wave);
	cgi.Cmd_RemoveCommand(cmd_inven);
	cgi.Cmd_RemoveCommand(cmd_kill);
	cgi.Cmd_RemoveCommand(cmd_use);
	cgi.Cmd_RemoveCommand(cmd_drop);
	cgi.Cmd_RemoveCommand(cmd_info);
	cgi.Cmd_RemoveCommand(cmd_prog);
	cgi.Cmd_RemoveCommand(cmd_give);
	cgi.Cmd_RemoveCommand(cmd_god);
	cgi.Cmd_RemoveCommand(cmd_notarget);
	cgi.Cmd_RemoveCommand(cmd_noclip);
	cgi.Cmd_RemoveCommand(cmd_invuse);
	cgi.Cmd_RemoveCommand(cmd_invprev);
	cgi.Cmd_RemoveCommand(cmd_invnext);
	cgi.Cmd_RemoveCommand(cmd_invdrop);
	cgi.Cmd_RemoveCommand(cmd_weapnext);
	cgi.Cmd_RemoveCommand(cmd_weapprev);
}


/*
==================
CG_LoadMap
==================
*/
void CG_LoadMap (int playerNum, int serverProtocol, int protocolMinorVersion, bool attractLoop, bool strafeHack, refConfig_t *inConfig)
{
	// Default values
	cg.frameCount = 1;
	cg.playerNum = playerNum;
	cg.serverProtocol = serverProtocol;
	cg.protocolMinorVersion = protocolMinorVersion;
	cg.attractLoop = attractLoop;	// true if demo playback
	cg.strafeHack = strafeHack; // god damnit

	// Video settings
	cg.refConfig = *inConfig;

	// Copy config strings
	CG_CopyConfigStrings ();

	// Init media
	CG_InitBaseMedia ();

	cg.mapLoading = true;
	cg.mapLoaded = false;

	CG_MapInit ();

	cg.mapLoading = false;
	cg.mapLoaded = true;
}


/*
==================
CG_Init
==================
*/
void CG_Init()
{
	// Clear everything
	cg.Clear();
	memset (&cgMedia, 0, sizeof(cgMedia_t));
	memset (&uiMedia, 0, sizeof(uiMedia));

	CG_ClearEntities ();

	// This nastiness is done because I don't
	// feel like compiling several cgame libraries...
	const char *gameDir = cgi.FS_Gamedir ();
	if (strstr (gameDir, "dday"))
		cg.currGameMod = GAME_MOD_DDAY;
	else if (strstr (gameDir, "giex"))
		cg.currGameMod = GAME_MOD_GIEX;
	else if (strstr (gameDir, "lox"))
		cg.currGameMod = GAME_MOD_LOX;
	else if (strstr (gameDir, "rogue"))
		cg.currGameMod = GAME_MOD_ROGUE;
	else if (strstr (gameDir, "xatrix"))
		cg.currGameMod = GAME_MOD_XATRIX;
	else
		cg.currGameMod = GAME_MOD_DEFAULT;

	// Update refConfig
	cgi.R_GetRefConfig (&cg.refConfig);

	// Copy config strings
	CG_CopyConfigStrings ();

	// Initialize early needed media
	CG_InitBaseMedia ();

	// Register cvars and commands
	V_Register ();
	CG_WeapRegister ();
	CG_RegisterMain ();

	// Check cvar sanity
	CG_UpdateCvars(true);

	// Location system init
	CG_LocationInit ();

	// Map FX init
	CG_MapFXInit ();

	// Load the UI
	UI_Init ();
}


/*
==================
CG_Shutdown
==================
*/
void CG_Shutdown ()
{
	// Shutdown map fx
	CG_MapFXShutdown ();

	// Clean out entities
	CG_ClearEntities ();

	// Remove commands
	CG_RemoveCmds ();
	V_Unregister ();
	CG_WeapUnregister ();

	// Shutdown media
	CG_ShutdownMap ();

	// Shutdown the UI
	UI_Shutdown ();

	// Shutdown locations
	CG_LocationShutdown ();

	HUD_CloseHuds();

	// The above function calls should release all of this anyways, but this is to be certain
	CG_FreeTag (CGTAG_ANY);
	CG_FreeTag (CGTAG_MAPFX);
	CG_FreeTag (CGTAG_MENU);
}

//======================================================================

#ifndef CGAME_HARD_LINKED
/*
==================
Com_Printf
==================
*/
void Com_Printf (comPrint_t flags, char *fmt, ...)
{
	va_list		argptr;
	char		text[MAX_COMPRINT];

	// Evaluate args
	va_start (argptr, fmt);
	vsnprintf (text, sizeof(text), fmt, argptr);
	va_end (argptr);

	// Print
	cgi.Com_Printf (flags, text);
}


/*
==================
Com_DevPrintf
==================
*/
void Com_DevPrintf (comPrint_t flags, char *fmt, ...)
{
	va_list		argptr;
	char		text[MAX_COMPRINT];

	// Evaluate args
	va_start (argptr, fmt);
	vsnprintf (text, sizeof(text), fmt, argptr);
	va_end (argptr);

	// Print
	cgi.Com_DevPrintf (flags, text);
}


/*
==================
Com_Error
==================
*/
void Com_Error (EComErrorType code, char *fmt, ...)
{
	va_list		argptr;
	char		text[MAX_COMPRINT];

	// Evaluate args
	va_start (argptr, fmt);
	vsnprintf (text, sizeof(text), fmt, argptr);
	va_end (argptr);

	// Print
	cgi.Com_Error (code, text);
}
#endif // CGAME_HARD_LINKED
