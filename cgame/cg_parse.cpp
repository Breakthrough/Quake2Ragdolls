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
// cg_parse.c
//

#include "cg_local.h"

/*
================
CG_ParseClientinfo

Load the skin, icon, and model for a client
================
*/
void CG_ParseClientinfo (int player)
{
	CG_LoadClientinfo (&cg.clientInfo[player], cg.configStrings[player+CS_PLAYERSKINS]);
}


/*
=================
CG_ParseConfigString
=================
*/
void CG_ParseConfigString (int num, char *str)
{
	char	oldCfgStr[MAX_CFGSTRLEN];

	if (num < 0 || num >= MAX_CFGSTRINGS)
		Com_Error (ERR_DROP, "CG_ParseConfigString: bad num");

	strncpy (oldCfgStr, cg.configStrings[num], sizeof(oldCfgStr));
	oldCfgStr[sizeof(oldCfgStr)-1] = '\0';

	strcpy (cg.configStrings[num], str);

	// Do something apropriate
	if (num >= CS_LIGHTS && num < CS_LIGHTS+MAX_CS_LIGHTSTYLES) {
		// Lightstyle
		CG_SetLightstyle (num-CS_LIGHTS);
	}
	else if (num >= CS_MODELS && num < CS_MODELS+MAX_CS_MODELS) {
		// Model
		if (cg.mapLoading || cg.mapLoaded) {
			cg.modelCfgDraw[num-CS_MODELS] = cgi.R_RegisterModel (cg.configStrings[num]);
			if (cg.configStrings[num][0] == '*')
				cg.modelCfgClip[num-CS_MODELS] = cgi.CM_InlineModel (cg.configStrings[num]);
			else
				cg.modelCfgClip[num-CS_MODELS] = NULL;
		}
		else {
			cg.modelCfgClip[num-CS_MODELS] = NULL;
			cg.modelCfgDraw[num-CS_MODELS] = NULL;
		}
	}
	else if (num >= CS_SOUNDS && num < CS_SOUNDS+MAX_CS_SOUNDS) {
		// Sound
		if (cg.configStrings[num][0])
			cg.soundCfgStrings[num-CS_SOUNDS] = cgi.Snd_RegisterSound (cg.configStrings[num]);
	}
	else if (num >= CS_IMAGES && num < CS_IMAGES+MAX_CS_IMAGES) {
		// Image
		cg.imageCfgStrings[num-CS_IMAGES] = CG_RegisterPic (cg.configStrings[num]);
	}
	else if (num == CS_MAXCLIENTS) {
		// Max client count
		if (!cg.attractLoop)
			cg.maxClients = atoi (cg.configStrings[CS_MAXCLIENTS]);
	}
	else if (num >= CS_PLAYERSKINS && num < CS_PLAYERSKINS+MAX_CS_CLIENTS) {
		// Skin
		if (strcmp (oldCfgStr, str))
			CG_ParseClientinfo (num-CS_PLAYERSKINS);
	}
}

/*
==============================================================

	SERVER MESSAGE PARSING

==============================================================
*/

static bool	in_parseSequence = false;

/*
==============
CG_StartServerMessage

Called by the client BEFORE all server messages have been parsed
==============
*/
void CG_StartServerMessage ()
{
	in_parseSequence = true;
}


/*
==============
CG_EndServerMessage

Called by the client AFTER all server messages have been parsed
==============
*/
void CG_EndServerMessage (int realTime)
{
	in_parseSequence = false;

	cg.realTime = realTime;

	CG_AddNetgraph ();
	SCR_UpdatePING ();
}


/*
==============
CG_ParseServerMessage

Parses command operations known to the game dll
Returns true if the message was parsed
==============
*/
bool CG_ParseServerMessage (int command)
{
	switch (command) {
	case SVC_CENTERPRINT:
		SCR_ParseCenterPrint ();
		return true;

	case SVC_INVENTORY:
		Inv_ParseInventory ();
		return true;

	case SVC_LAYOUT:
		HUD_CopyLayout ();
		return true;

	case SVC_MUZZLEFLASH2:
		CG_ParseMuzzleFlash2 ();
		return true;

	case SVC_TEMP_ENTITY:
		CG_ParseTempEnt ();
		return true;

	default:
		return false;
	}
}
