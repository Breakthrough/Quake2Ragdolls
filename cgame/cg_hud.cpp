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
// cg_hud.c
//

#include "cg_local.h"

struct hudFile
{
	String name;
	String directory;
	Script *script;
	TList<struct refMaterial_t*> materials;
};
hudFile *curHud = null;
extern cVar_t *cg_hud;

struct hudNums
{
	refMaterial_t		*material;
	int					numFields;
	int					numIndexes;
};

TList<hudNums> hudNumMats;

/*
=============================================================================

	SUPPORTING FUNCTIONS

=============================================================================
*/

#define STAT_MINUS	0	// num frame for '-' stats digit

#define ICON_WIDTH	16
#define ICON_HEIGHT	24

/*
==============
HUD_DrawString
==============
*/
static void __fastcall HUD_DrawString (char *string, float x, float y, float centerwidth, bool bold)
{
	float	margin;
	char	line[1024];
	int		width;
	vec2_t	charSize;

	margin = x;

	while (*string) {
		// Scan out one line of text from the string
		width = 0;
		while (*string && (*string != '\n'))
			line[width++] = *string++;
		line[width] = 0;

		if (centerwidth) {
			cgi.R_GetFontDimensions (NULL, cg.hudScale[0], cg.hudScale[1], bold ? FS_SECONDARY|FS_SQUARE : FS_SQUARE, charSize);
			x = margin + (centerwidth - (width * charSize[0])) * 0.5;
		}
		else
			x = margin;

		cgi.R_DrawStringLen (NULL, x, y, cg.hudScale[0], cg.hudScale[1], bold ? FS_SECONDARY|FS_SQUARE : FS_SQUARE, line, width, colorb(255, 255, 255, FloatToByte(scr_hudalpha->floatVal)));

		if (*string) {
			string++;	// skip the \n
			x = margin;
			y += charSize[1];
		}
	}
}


/*
==============
HUD_DrawField
==============
*/
static void HUD_DrawField (float x, float y, int altclr, int width, int value)
{
	/*
	char	num[16], *ptr;
	int		l, frame;

	if (width < 1)
		return;

	// Draw number string
	if (width > 5)
		width = 5;

	Q_snprintfz (num, sizeof(num), "%i", value);
	l = (int)strlen (num);
	if (l > width)
		l = width;
	x += 2 * cg.hudScale[0] + (ICON_WIDTH * cg.hudScale[1]) * (width - l);

	ptr = num;
	while (*ptr && l) {
		if (*ptr == '-')
			frame = STAT_MINUS;
		else
			frame = *ptr - '0';

		cgi.R_DrawPic (
			cgMedia.hudNumMats[altclr % 2][frame % 11], 0,
			QuadVertices().SetVertices(x, y,
			ICON_WIDTH * cg.hudScale[0],
			ICON_HEIGHT * cg.hudScale[1]),
			colorb(255, 255, 255, FloatToByte(scr_hudalpha->floatVal)));

		x += ICON_WIDTH * cg.hudScale[0];
		ptr++;
		l--;
	}
	*/
}

static void HUD_DrawField (float x, float y, hudNums &hudNumber, int width, int value, int index)
{
	char	num[16], *ptr;
	int		l, frame;
	int		w, h;

	cgi.R_GetImageSize(hudNumber.material, &w, &h);

	float	iconWidth = (float)w / ((float)hudNumber.numFields * hudNumber.numIndexes);

	if (width < 1)
		return;

	// Draw number string
	if (width > 5)
		width = 5;

	Q_snprintfz (num, sizeof(num), "%i", value);
	l = (int)strlen (num);
	if (l > width)
		l = width;
	x += 2 * cg.hudScale[0] + (iconWidth * cg.hudScale[1]) * (width - l);

	ptr = num;
	while (*ptr && l)
	{
		if (*ptr == '-')
			frame = STAT_MINUS;
		else
			frame = (*ptr - '0') + 1;

		frame += (hudNumber.numFields * (index));

		cgi.R_DrawPic (
			hudNumber.material, 0,
			QuadVertices().SetVertices(x, y,
			iconWidth * cg.hudScale[0],
			h * cg.hudScale[1])
			.SetCoords
			(
				frame * (iconWidth / w),
				0,
				(frame * (iconWidth / w)) + (iconWidth / w),
				1
			),
			colorb(255, 255, 255, FloatToByte(scr_hudalpha->floatVal)));

		x += iconWidth * cg.hudScale[0];
		ptr++;
		l--;
	}
}



/*
================
CG_DrawHUDModel
================
*/
void CG_DrawHUDModel (int x, int y, int w, int h, struct refModel_t *model, struct refMaterial_t *mat, float yawSpeed)
{
	vec3_t	mins, maxs;
	vec3_t	origin, angles;

	// Get model bounds
	cgi.R_ModelBounds (model, mins, maxs);

	// Try to fill the the window with the model
	origin[0] = 0.5 * (maxs[2] - mins[2]) * (1.0 / 0.179);
	origin[1] = 0.5 * (mins[1] + maxs[1]);
	origin[2] = -0.5 * (mins[2] + maxs[2]);
	Vec3Set (angles, 0, AngleModf (yawSpeed * (cg.refreshTime & 2047) * (360.0 / 2048.0)), 0);
}

/*
=============================================================================

	RENDERING

=============================================================================
*/

/*
================
HUD_ExecuteLayoutString
================
*/
static void HUD_ExecuteLayoutString (char *layout)
{
	float					x, y;
	int						value, width, index;
	char					*token;
	clientInfo_t			*ci;
	struct refMaterial_t	*mat;
	vec2_t					charSize;
	int						w, h;

	colorb hudColor(255, 255, 255, FloatToByte(scr_hudalpha->floatVal));

	if (!layout[0])
		return;

	cgi.R_GetFontDimensions (NULL, cg.hudScale[0], cg.hudScale[1], FS_SQUARE, charSize);

	x = 0;
	y = 0;
	width = 3;

	for ( ; layout ; ) {
		token = Com_Parse (&layout);
		if (!token[0])
			break;

		switch (token[0]) {
		case 'a':
			// Ammo number
			if (!strcmp (token, "anum")) {
				int		altclr;

				width = 3;
				value = cg.frame.playerState.stats[STAT_AMMO];
				if (value > 5)
					altclr = 0;	// Green
				else if (value >= 0)
					altclr = (cg.frame.serverFrame>>2) & 1;		// Flash
				else
					continue;	// Negative number = don't show

				if (cg.frame.playerState.stats[STAT_FLASHES] & 4) {
					cgi.R_GetImageSize (cgMedia.hudFieldMat, &w, &h);
					cgi.R_DrawPic (
						cgMedia.hudFieldMat, 0,
						QuadVertices().SetVertices(x, y,
						w * cg.hudScale[0],
						h * cg.hudScale[1]),
						hudColor);
				}

				HUD_DrawField (x, y, altclr, width, value);
				continue;
			}
			break;

		case 'c':
			// Draw a deathmatch client block
			if (!strcmp (token, "client")) {
				int		score, ping, time;

				token = Com_Parse (&layout);
				x = (cg.refConfig.vidWidth * 0.5f) - (160.0f * cg.hudScale[0]) + atoi (token) * cg.hudScale[0];
				token = Com_Parse (&layout);
				y = (cg.refConfig.vidHeight * 0.5f) - (120.0f * cg.hudScale[1]) + atoi (token) * cg.hudScale[1];

				token = Com_Parse (&layout);
				value = atoi (token);
				if (value >= cg.maxClients || value < 0)
					Com_Error (ERR_DROP, "Bad client block index %i (maxClients: %i)", value, cg.maxClients);
				ci = &cg.clientInfo[value];

				token = Com_Parse (&layout);
				score = atoi (token);

				token = Com_Parse (&layout);
				ping = atoi (token);

				token = Com_Parse (&layout);
				time = atoi (token);

				// Icon
				if (!ci->icon)
					ci->icon = cgi.R_RegisterPic (ci->iconName);
				if (!ci->icon)
					ci->icon = cg.baseClientInfo.icon;
				if (ci->icon) {
					cgi.R_GetImageSize (ci->icon, &w, &h);
					cgi.R_DrawPic (ci->icon, 0, QuadVertices().SetVertices(x, y, w * cg.hudScale[0], h * cg.hudScale[1]), hudColor);
				}
				else {
					w = 32;
					h = 32;
				}

				// Block
				cgi.R_DrawString (NULL, x+(charSize[0]*4), y, cg.hudScale[0], cg.hudScale[1], FS_SECONDARY|FS_SQUARE, ci->name, hudColor);
				cgi.R_DrawString (NULL, x+(charSize[0]*4), y+charSize[1], cg.hudScale[0], cg.hudScale[1], FS_SQUARE, "Score: ", hudColor);
				cgi.R_DrawString (NULL, x+(charSize[0]*4) + (charSize[0]*7), y+charSize[1], cg.hudScale[0], cg.hudScale[1], FS_SECONDARY|FS_SQUARE, Q_VarArgs ("%i", score), hudColor);
				cgi.R_DrawString (NULL, x+(charSize[0]*4), y+(charSize[1]*2), cg.hudScale[0], cg.hudScale[1], FS_SQUARE, Q_VarArgs ("Ping:  %i", ping), hudColor);
				cgi.R_DrawString (NULL, x+(charSize[0]*4), y+(charSize[1]*3), cg.hudScale[0], cg.hudScale[1], FS_SQUARE, Q_VarArgs ("Time:  %i", time), hudColor);
				continue;
			}

			// Draw a string
			if (!strcmp (token, "cstring")) {
				token = Com_Parse (&layout);
				HUD_DrawString (token, x, y, 320.0f * cg.hudScale[0], false);
				continue;
			}

			// Draw a high-bit string
			if (!strcmp (token, "cstring2")) {
				token = Com_Parse (&layout);
				HUD_DrawString (token, x, y, 320.0f * cg.hudScale[0], true);
				continue;
			}

			// Draw a ctf client block
			if (!strcmp (token, "ctf")) {
				int		score, ping;
				char	block[80];

				token = Com_Parse (&layout);
				x = (cg.refConfig.vidWidth * 0.5f);
				x -= (160.0f * cg.hudScale[0]);
				x += atof (token) * cg.hudScale[0];

				token = Com_Parse (&layout);
				y = (cg.refConfig.vidHeight * 0.5f);
				y -= (120.0f * cg.hudScale[1]);
				y += atof (token) * cg.hudScale[1];

				token = Com_Parse (&layout);
				value = atoi (token);

				if (value >= cg.maxClients || value < 0)
					Com_Error (ERR_DROP, "Bad client block index %i (maxClients: %i)", value, cg.maxClients);
				ci = &cg.clientInfo[value];

				token = Com_Parse (&layout);
				score = atoi (token);

				token = Com_Parse (&layout);
				ping = clamp (atoi (token), 0, 999);

				Q_snprintfz (block, sizeof(block), "%3d %3d %-12.12s", score, ping, ci->name);
				cgi.R_DrawString (NULL, x, y, cg.hudScale[0], cg.hudScale[1], (value == cg.playerNum) ? FS_SECONDARY|FS_SQUARE : FS_SQUARE, block, hudColor);
				continue;
			}
			break;

		case 'h':
			// Health number
			if (!strcmp (token, "hnum")) {
				int		altclr;

				width = 3;
				value = cg.frame.playerState.stats[STAT_HEALTH];
				if (value > 25)
					altclr = 0;	// Green
				else if (value > 0)
					altclr = (cg.frame.serverFrame>>2) & 1;	// Flash
				else
					altclr = 1;

				if (cg.frame.playerState.stats[STAT_FLASHES] & 1) {
					int		w, h;
					cgi.R_GetImageSize (cgMedia.hudFieldMat, &w, &h);
					cgi.R_DrawPic (
						cgMedia.hudFieldMat, 0,
						QuadVertices().SetVertices(x, y,
						w * cg.hudScale[0],
						h * cg.hudScale[1]),
						hudColor);
				}

				HUD_DrawField (x, y, altclr, width, value);
				continue;
			}
			break;

		case 'i':
			// Evaluate the expression
			if (!strcmp (token, "if")) {
				token = Com_Parse (&layout);
				value = cg.frame.playerState.stats[atoi (token)];
				if (!value) {
					// Skip to endif
					while (token && token[0] && strcmp (token, "endif"))
						token = Com_Parse (&layout);
				}

				continue;
			}
			break;

		case 'n':
			// Draw a HUD number
			if (!strcmp (token, "num")) {
				token = Com_Parse (&layout);
				width = atoi (token);

				token = Com_Parse (&layout);
				value = cg.frame.playerState.stats[atoi(token)];

				HUD_DrawField (x, y, 0, width, value);
				continue;
			}
			break;

		case 'p':
			// Draw a pic from a stat number
			if (!strcmp (token, "pic")) {
				token = Com_Parse (&layout);
				value = cg.frame.playerState.stats[atoi (token)];
				if (value >= MAX_CS_IMAGES)
					Com_Error (ERR_DROP, "Pic >= MAX_CS_IMAGES");

				if (cg.imageCfgStrings[value])
					mat = cg.imageCfgStrings[value];
				else if (cg.configStrings[CS_IMAGES+value] && cg.configStrings[CS_IMAGES+value][0])
					mat = CG_RegisterPic (cg.configStrings[CS_IMAGES+value]);
				else
					mat = cgMedia.noTexture;

				cgi.R_GetImageSize (mat, &w, &h);
				cgi.R_DrawPic (mat, 0, QuadVertices().SetVertices(x, y, w * cg.hudScale[0], h * cg.hudScale[1]), hudColor);
				continue;
			}

			// Draw a pic from a name
			if (!strcmp (token, "picn")) {
				token = Com_Parse (&layout);

				mat = CG_RegisterPic (token);
				if (mat) {
					cgi.R_GetImageSize (mat, &w, &h);
					cgi.R_DrawPic (mat, 0, QuadVertices().SetVertices(x, y, w * cg.hudScale[0], h * cg.hudScale[1]), hudColor);
				}
				continue;
			}
			break;

		case 'r':
			// Armor number
			if (!strcmp (token, "rnum")) {
				width = 3;
				value = cg.frame.playerState.stats[STAT_ARMOR];
				if (value < 1)
					continue;

				if (cg.frame.playerState.stats[STAT_FLASHES] & 2) {
					cgi.R_GetImageSize (cgMedia.hudFieldMat, &w, &h);
					cgi.R_DrawPic (
						cgMedia.hudFieldMat, 0,
						QuadVertices().SetVertices(x, y,
						w * cg.hudScale[0],
						h * cg.hudScale[1]),
						hudColor);
				}

				HUD_DrawField (x, y, 0, width, value);
				continue;
			}
			break;

		case 's':
			// Draw a string in the config strings
			if (!strcmp (token, "stat_string")) {
				token = Com_Parse (&layout);
				index = atoi (token);
				if (index < 0 || index >= MAX_STATS)
					Com_Error (ERR_DROP, "Bad stat_string stat index %i", index);

				index = cg.frame.playerState.stats[index];
				if (index < 0 || index >= MAX_CFGSTRINGS)
					Com_Error (ERR_DROP, "Bad stat_string config string index %i", index);

				cgi.R_DrawString (NULL, x, y, cg.hudScale[0], cg.hudScale[1], FS_SQUARE, cg.configStrings[index], hudColor);
				continue;
			}

			// Draw a string
			if (!strcmp (token, "string")) {
				token = Com_Parse (&layout);
				cgi.R_DrawString (NULL, x, y, cg.hudScale[0], cg.hudScale[1], FS_SQUARE, token, hudColor);
				continue;
			}

			// Draw a string in high-bit form
			if (!strcmp (token, "string2")) {
				token = Com_Parse (&layout);
				cgi.R_DrawString (NULL, x, y, cg.hudScale[0], cg.hudScale[1], FS_SECONDARY|FS_SQUARE, token, hudColor);
				continue;
			}
			break;

		case 'x':
			// X position
			if (!strcmp (token, "xl")) {
				token = Com_Parse (&layout);
				x = atoi (token) * cg.hudScale[0];
				continue;
			}

			if (!strcmp (token, "xr")) {
				token = Com_Parse (&layout);
				x = cg.refConfig.vidWidth + atoi (token) * cg.hudScale[0];
				continue;
			}

			if (!strcmp (token, "xv")) {
				token = Com_Parse (&layout);
				x = (cg.refConfig.vidWidth * 0.5f) - (160.0f * cg.hudScale[0]) + (atoi (token) * cg.hudScale[0]);
				continue;
			}
			break;

		case 'y':
			// Y position
			if (!strcmp (token, "yt")) {
				token = Com_Parse (&layout);
				y = atoi (token) * cg.hudScale[1];
				continue;
			}

			if (!strcmp (token, "yb")) {
				token = Com_Parse (&layout);
				y = cg.refConfig.vidHeight + atoi (token) * cg.hudScale[1];
				continue;
			}

			if (!strcmp (token, "yv")) {
				token = Com_Parse (&layout);
				y = (cg.refConfig.vidHeight * 0.5f) - (120.0f * cg.hudScale[1]) + (atoi (token) * cg.hudScale[1]);
				continue;
			}
			break;
		}
	}
}

/*
=============================================================================

	HANDLING

=============================================================================
*/

int Lua_XFromLeft(lua_State *state)
{
	var script = cgi.Lua_ScriptFromState(state);

	script->Push(script->ToNumber(1) * cg.hudScale[0]);

	return 1;
}

int Lua_YFromTop(lua_State *state)
{
	var script = cgi.Lua_ScriptFromState(state);

	script->Push(script->ToNumber(1) * cg.hudScale[1]);

	return 1;
}

int Lua_XFromRight(lua_State *state)
{
	var script = cgi.Lua_ScriptFromState(state);

	script->Push(cg.refConfig.vidWidth + script->ToNumber(1) * cg.hudScale[0]);

	return 1;
}

int Lua_YFromBottom(lua_State *state)
{
	var script = cgi.Lua_ScriptFromState(state);

	script->Push(cg.refConfig.vidHeight + script->ToNumber(1) * cg.hudScale[1]);

	return 1;
}

int Lua_XFromVirtual(lua_State *state)
{
	var script = cgi.Lua_ScriptFromState(state);

	script->Push((cg.refConfig.vidWidth * 0.5f) - (160.0f * cg.hudScale[0]) + (script->ToNumber(1) * cg.hudScale[0]));

	return 1;
}

int Lua_YFromVirtual(lua_State *state)
{
	var script = cgi.Lua_ScriptFromState(state);

	script->Push((cg.refConfig.vidHeight * 0.5f) - (120.0f * cg.hudScale[1]) + (script->ToNumber(1) * cg.hudScale[1]));

	return 1;
}

int Lua_DrawPic (lua_State *state)
{
	var script = cgi.Lua_ScriptFromState(state);
	colorb hudColor(255, 255, 255, FloatToByte(scr_hudalpha->floatVal));

	var value = cg.frame.playerState.stats[script->ToInteger(3)];
	if (value >= MAX_CS_IMAGES)
		Com_Error (ERR_DROP, "Pic >= MAX_CS_IMAGES");

	struct refMaterial_t *mat;
	if (cg.imageCfgStrings[value])
		mat = cg.imageCfgStrings[value];
	else if (cg.configStrings[CS_IMAGES+value] && cg.configStrings[CS_IMAGES+value][0])
		mat = CG_RegisterPic (cg.configStrings[CS_IMAGES+value]);
	else
		mat = cgMedia.noTexture;

	int w, h;
	cgi.R_GetImageSize (mat, &w, &h);
	cgi.R_DrawPic (mat, 0, QuadVertices().SetVertices(script->ToInteger(1), script->ToInteger(2), w * cg.hudScale[0], h * cg.hudScale[1]), hudColor);

	return 0;
}

int Lua_DrawMaterial (lua_State *state)
{
	var script = cgi.Lua_ScriptFromState(state);
	colorb hudColor(255, 255, 255, FloatToByte(scr_hudalpha->floatVal));

	var mat = (refMaterial_t*)script->ToUserData(3);

	if (mat == null)
		mat = cgMedia.noTexture;

	int w, h;
	cgi.R_GetImageSize (mat, &w, &h);
	cgi.R_DrawPic (mat, 0, QuadVertices().SetVertices(script->ToInteger(1), script->ToInteger(2), w * cg.hudScale[0], h * cg.hudScale[1]), hudColor);

	return 0;
}

int Lua_GetStat (lua_State *state)
{
	var script = cgi.Lua_ScriptFromState(state);

	script->Push(cg.frame.playerState.stats[script->ToInteger(1)]);

	return 1;
}

int Lua_DrawField (lua_State *state)
{
	var script = cgi.Lua_ScriptFromState(state);

	HUD_DrawField(script->ToNumber(1), script->ToNumber(2), hudNumMats[script->ToInteger(3)], script->ToInteger(4), script->ToInteger(5), script->ToInteger(6));

	return 0;
}

int Lua_GetScale (lua_State *state)
{
	var script = cgi.Lua_ScriptFromState(state);

	script->Push(cg.hudScale[0]);
	script->Push(cg.hudScale[1]);

	return 2;
}

int Lua_GetStatPic (lua_State *state)
{
	var script = cgi.Lua_ScriptFromState(state);
	refMaterial_t *mat;
	int value = script->ToInteger(1);

	if (cg.imageCfgStrings[value])
		mat = cg.imageCfgStrings[value];
	else if (cg.configStrings[CS_IMAGES+value] && cg.configStrings[CS_IMAGES+value][0])
		mat = CG_RegisterPic (cg.configStrings[CS_IMAGES+value]);
	else
		mat = cgMedia.noTexture;

	script->Push(mat);

	return 1;
}

int Lua_StatString (lua_State *state)
{
	var script = cgi.Lua_ScriptFromState(state);

	int x = script->ToNumber(1), y = script->ToNumber(2), stat = script->ToNumber(3);

	if (stat < 0 || stat >= MAX_STATS)
		Com_Error (ERR_DROP, "Bad stat_string stat index %i", stat);

	stat = cg.frame.playerState.stats[stat];
	if (stat < 0 || stat >= MAX_CFGSTRINGS)
		Com_Error (ERR_DROP, "Bad stat_string config string index %i", stat);

	cgi.R_DrawString (NULL, x, y, cg.hudScale[0], cg.hudScale[1], FS_SQUARE, cg.configStrings[stat], colorb(script->ToNumber(4), script->ToNumber(5), script->ToNumber(6), script->ToNumber(7)));
	return 0;
}

int Lua_DrawString (lua_State *state)
{
	var script = cgi.Lua_ScriptFromState(state);

	cgi.R_DrawString(null, script->ToNumber(1), script->ToNumber(2), script->ToNumber(3), script->ToNumber(4), script->ToInteger(5), (char*)script->ToString(6), colorb(script->ToNumber(7), script->ToNumber(8), script->ToNumber(9), script->ToNumber(10)));

	return 0;
}

int Lua_GetConfigString (lua_State *state)
{
	var script = cgi.Lua_ScriptFromState(state);
	int index = script->ToInteger(1);

	script->Push(cg.configStrings[index]);

	return 1;
}

int Lua_GetItemName (lua_State *state)
{
	var script = cgi.Lua_ScriptFromState(state);
	int index = script->ToInteger(1);

	script->Push(itemlist[index].pickup_name);

	return 1;
}

int Lua_GetItemIcon (lua_State *state)
{
	var script = cgi.Lua_ScriptFromState(state);

	script->Push(cgMedia.iconRegistry[script->ToInteger(1)]);

	return 1;
}

int Lua_CG_ServerFrame (lua_State *state)
{
	var script = cgi.Lua_ScriptFromState(state);

	script->Push(cg.frame.serverFrame);

	return 1;
}

int Lua_Cvar_Register (lua_State *state)
{
	var script = cgi.Lua_ScriptFromState(state);

	const char *name = script->ToString(1);
	String val;

	if (script->IsNumber(2))
		val = String::Format("%f", script->ToNumber(2));
	else
		val = String::Format("%s", script->ToString(2));

	const int flags = script->ToInteger(3);

	cVar_t *cv = cgi.Cvar_Register(name, val.CString(), flags);

	script->Push((void*)cv);

	return 1;
}

int Lua_Cvar_GetNumberValue (lua_State *state)
{
	var script = cgi.Lua_ScriptFromState(state);

	cVar_t *cvar = (cVar_t*)script->ToUserData(1);
	script->Push(cvar->floatVal);

	return 1;
}

int Lua_Cvar_GetStringValue (lua_State *state)
{
	var script = cgi.Lua_ScriptFromState(state);

	cVar_t *cvar = (cVar_t*)script->ToUserData(1);
	script->Push(cvar->string);

	return 1;
}

int Lua_R_RegisterPic (lua_State *state)
{
	var script = cgi.Lua_ScriptFromState(state);
	String name (script->ToString(1));
	
	var mat = cgi.R_RegisterPic(name.CString());

	if (mat == null)
	{
		mat = cgi.R_RegisterPic((curHud->directory + "/" + name).CString());
		
		if (mat == null)
			throw Exception();
	}

	script->Push(mat);

	return 1;
}

int Lua_R_DrawPic (lua_State *state)
{
	var script = cgi.Lua_ScriptFromState(state);

	cgi.R_DrawPic((refMaterial_t*)script->ToUserData(1), 0, QuadVertices().SetVertices(script->ToNumber(2), script->ToNumber(3), script->ToInteger(4), script->ToNumber(5)),
		colorb(script->ToNumber(6), script->ToNumber(7), script->ToNumber(8), script->ToNumber(9)));

	return 0;
}

int Lua_R_GetImageSize (lua_State *state)
{
	var script = cgi.Lua_ScriptFromState(state);

	refMaterial_t *mat = (refMaterial_t *)script->ToUserData(1);
	int w, h;
	cgi.R_GetImageSize(mat, &w, &h);

	script->Push(w);
	script->Push(h);

	return 2;
}

int Lua_Cvar_Get (lua_State *state)
{
	var script = cgi.Lua_ScriptFromState(state);
	var cv = script->ToString(1);

	script->Push(cgi.Cvar_Exists(cv));

	return 1;
}

int Lua_R_GetFrameTime (lua_State *state)
{
	var script = cgi.Lua_ScriptFromState(state);

	script->Push(cg.refreshFrameTime);

	return 1;
}

int Lua_R_GetScreenSize (lua_State *state)
{
	var script = cgi.Lua_ScriptFromState(state);

	script->Push(cg.refConfig.vidWidth);
	script->Push(cg.refConfig.vidHeight);

	return 2;
}

int Lua_RegisterNumbers (lua_State *state)
{
	var script = cgi.Lua_ScriptFromState(state);

	int index = hudNumMats.Count();

	hudNums nums;

	String name (script->ToString(1));
	nums.material = cgi.R_RegisterPic(name.CString());

	if (nums.material == null)
	{
		nums.material = cgi.R_RegisterPic((curHud->directory + "/" + name).CString());
		
		if (nums.material == null)
			throw Exception();
	}

	nums.numFields = script->ToInteger(2);
	nums.numIndexes = script->ToInteger(3);

	script->Push(index);

	hudNumMats.Add(nums);

	return 1;
}

int CG_GetAmmoIndex ();

int Lua_CG_GetAmmoIndex (lua_State *state)
{
	var script = cgi.Lua_ScriptFromState(state);
	script->Push(CG_GetAmmoIndex());
	return 1;	
}

ScriptFunctionTable cgFunctions[] =
{
	{ "Cvar_Get", Lua_Cvar_Get },
	{ "Cvar_Register", Lua_Cvar_Register },
	{ "Cvar_GetNumberValue", Lua_Cvar_GetNumberValue },
	{ "Cvar_GetStringValue", Lua_Cvar_GetStringValue },

	{ "R_RegisterPic", Lua_R_RegisterPic },
	{ "R_DrawPic", Lua_R_DrawPic },
	{ "R_GetImageSize", Lua_R_GetImageSize },

	{ "CG_ServerFrame", Lua_CG_ServerFrame },
	{ "R_GetFrameTime", Lua_R_GetFrameTime },
	{ "R_GetScreenSize", Lua_R_GetScreenSize },
	{ "CG_GetAmmoIndex", Lua_CG_GetAmmoIndex },
	{ null, null }
};

ScriptFunctionTable hudFunctions[] =
{
	{ "Hud_RegisterNumbers", Lua_RegisterNumbers },
	{ "Hud_GetItemName", Lua_GetItemName },
	{ "Hud_GetItemIcon", Lua_GetItemIcon },
	{ "Hud_GetConfigString", Lua_GetConfigString },
	{ "Hud_DrawString", Lua_DrawString },
	{ "Hud_StatString", Lua_StatString },
	{ "Hud_GetStatPic", Lua_GetStatPic },
	{ "Hud_GetScale", Lua_GetScale },
	{ "Hud_XFromLeft", Lua_XFromLeft },
	{ "Hud_YFromTop", Lua_YFromTop },
	{ "Hud_XFromRight", Lua_XFromRight },
	{ "Hud_YFromBottom", Lua_YFromBottom },
	{ "Hud_XFromVirtual", Lua_XFromVirtual },
	{ "Hud_YFromVirtual", Lua_YFromVirtual },
	{ "Hud_DrawPic", Lua_DrawPic },
	{ "Hud_DrawMaterial", Lua_DrawMaterial },
	{ "Hud_GetStat", Lua_GetStat },
	{ "Hud_DrawField", Lua_DrawField },
	{ null, null }
};

ScriptGlobalTable cvarGlobals[] =
{
	ScriptGlobalTable ("CVAR_ARCHIVE", CVAR_ARCHIVE),
	ScriptGlobalTable ("CVAR_USERINFO", CVAR_USERINFO),
	ScriptGlobalTable ("CVAR_SERVERINFO", CVAR_SERVERINFO),
	ScriptGlobalTable ("CVAR_READONLY", CVAR_READONLY),
	ScriptGlobalTable ("CVAR_LATCH_SERVER", CVAR_LATCH_SERVER),
	ScriptGlobalTable ("CVAR_LATCH_VIDEO", CVAR_LATCH_VIDEO),
	ScriptGlobalTable ("CVAR_LATCH_AUDIO", CVAR_LATCH_AUDIO),
	ScriptGlobalTable ("CVAR_RESET_GAMEDIR", CVAR_RESET_GAMEDIR),
	ScriptGlobalTable ("CVAR_CHEAT", CVAR_CHEAT),
	ScriptGlobalTable ()
};

ScriptGlobalTable statGlobals[] =
{
	ScriptGlobalTable ("STAT_HEALTH", STAT_HEALTH),
	ScriptGlobalTable ("STAT_AMMO", STAT_AMMO),
	ScriptGlobalTable ("STAT_ARMOR_ICON", STAT_ARMOR_ICON),
	ScriptGlobalTable ("STAT_ARMOR", STAT_ARMOR),
	ScriptGlobalTable ("STAT_SELECTED_ICON", STAT_SELECTED_ICON),
	ScriptGlobalTable ("STAT_PICKUP", STAT_PICKUP),
	ScriptGlobalTable ("STAT_TIMER_ICON", STAT_TIMER_ICON),
	ScriptGlobalTable ("STAT_TIMER", STAT_TIMER),
	ScriptGlobalTable ("STAT_HELPICON", STAT_HELPICON),
	ScriptGlobalTable ("STAT_SELECTED_ITEM", STAT_SELECTED_ITEM),
	ScriptGlobalTable ("STAT_LAYOUTS", STAT_LAYOUTS),
	ScriptGlobalTable ("STAT_FRAGS", STAT_FRAGS),
	ScriptGlobalTable ("STAT_FLASHES", STAT_FLASHES),
	ScriptGlobalTable ("STAT_CHASE", STAT_CHASE),
	ScriptGlobalTable ("STAT_SPECTATOR", STAT_SPECTATOR),
	ScriptGlobalTable()
};

ScriptGlobalTable fontGlobals[] =
{
	ScriptGlobalTable ("FS_ALIGN_CENTER", FS_ALIGN_CENTER),
	ScriptGlobalTable ("FS_ALIGN_RIGHT", FS_ALIGN_RIGHT),
	ScriptGlobalTable ("FS_ITALIC", FS_ITALIC),
	ScriptGlobalTable ("FS_SECONDARY", FS_SECONDARY),
	ScriptGlobalTable ("FS_SHADOW", FS_SHADOW),
	ScriptGlobalTable ("FS_SQUARE", FS_SQUARE),
	ScriptGlobalTable()
};

TList<hudFile> huds;

void HUD_Reload ()
{
	HUD_CloseHuds();
	HUD_LoadHuds();
}
	
conCmd_t *reloadCmd;

void HUD_LoadHuds ()
{
	var startCycles = cgi.Sys_Cycles();
	Com_Printf (0, "\n--------- HUD Initialization ---------\n");
	reloadCmd = cgi.Cmd_AddCommand("hud_reload", 0, HUD_Reload, "Reload all the HUDs");
	var files = cgi.FS_FindFiles("huds", "*", "hud", false, true);

	Com_Printf (0, "Found %i HUDs\n", files.Count());

	for (uint32 i = 0; i < files.Count(); ++i)
	{
		hudFile file;

		Com_Printf (0, "Loading %s... ", files[i].CString());
		file.directory = Path::GetFilePath(files[i]);
		file.name = file.directory.Substring(file.directory.IndexOf('/') + 1);
		file.script = cgi.Lua_CreateLuaState(files[i].CString());
		cgi.Lua_RegisterFunctions(file.script, hudFunctions);
		cgi.Lua_RegisterFunctions(file.script, cgFunctions);

		cgi.Lua_RegisterGlobals(file.script, cvarGlobals);
		cgi.Lua_RegisterGlobals(file.script, statGlobals);
		cgi.Lua_RegisterGlobals(file.script, fontGlobals);

		curHud = &file;
		file.script->GetGlobal("InitHud");
		file.script->Call(0, 0);

		huds.Add(file);
		curHud = null;

		Com_Printf (0, "completed.\n");
	}

	Com_Printf (0, "\n--------------------------------------\n");
	Com_Printf (0, "Loaded %i HUDs in %6.2fms.\n", huds.Count(), (cgi.Sys_Cycles()-startCycles) * cgi.Sys_MSPerCycle());
	Com_Printf (0, "\n--------------------------------------\n");

	Com_Printf (0, "\n");
}

void HUD_CloseHuds ()
{
	if (reloadCmd || huds.Count())
	{
		hudNumMats.Clear();

		Com_Printf (0, "Releasing HUDs...\n");
		cgi.Cmd_RemoveCommand(reloadCmd);

		for (uint32 i = 0; i < huds.Count(); ++i)
			cgi.Lua_DestroyLuaState(huds[i].script);

		huds.Clear();
		Com_Printf(0, "Done.\n");
	}
}

/*
================
HUD_CopyLayout
================
*/
void HUD_CopyLayout ()
{
	Q_strncpyz (cg.layout, cgi.MSG_ReadString (), sizeof(cg.layout));
}


/*
================
HUD_DrawStatusBar

The status bar is a small layout program that is based on the stats array
================
*/

hudFile *FindHUD (const String &str)
{
	for (uint32 i = 0; i < huds.Count(); ++i)
	{
		if (huds[i].name.Compare(str) == 0)
		{
			return &huds[i];
			break;
		}
	}

	return null;
}

void HUD_DrawStatusBar ()
{
	if (cg_hud->modified || curHud == null)
	{
		curHud = FindHUD(cg_hud->string);

		if (curHud == null)
		{
			Com_Printf(PRNT_ERROR, "Unable to find HUD \"%s\", using default.\n", cg_hud->string);
			curHud = FindHUD("default");
		}

		cg_hud->modified = false;
	}

	if (curHud != null)
	{
		curHud->script->GetGlobal("ExecuteLayout");
		curHud->script->Call(0, 0);
	}
}

/*
================
HUD_DrawLayout
================
*/
void HUD_DrawLayout ()
{
	if (cg.frame.playerState.stats[STAT_LAYOUTS] & 1)
		HUD_ExecuteLayoutString (cg.layout);
}
