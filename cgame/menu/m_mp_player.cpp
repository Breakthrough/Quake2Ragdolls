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
// m_mp_player.c
//

#include "m_local.h"

/*
=============================================================================

	PLAYER CONFIG MENU

=============================================================================
*/

#define MAX_PLAYERMODELS	512
#define MAX_PLAYERSKINS		512

struct modelInfo_t {
	int		numSkins;
	char	**skinDisplayNames;

	char	displayName[16];
	char	directory[MAX_QPATH];
};

struct m_playerConfigMenu_t {
	// Player model info
	bool				modelsFound;

	modelInfo_t			modelInfo[MAX_PLAYERMODELS];
	char				*modelNames[MAX_PLAYERMODELS];
	int					numPlayerModels;

	// Menu items
	uiFrameWork_t		frameWork;

	uiImage_t			banner;
	uiAction_t			header;

	uiField_t			nameField;
	uiList_t			modelList;
	uiList_t			skinList;
	uiList_t			handednessList;
	uiList_t			rateList;

	uiAction_t			backAction;
};

static m_playerConfigMenu_t m_playerConfigMenu;

static int rateTable[] = {
	2500,
	3200,
	5000,
	10000,
	25000,
	0
};

static void HandednessCallback (void *unused)
{
	cgi.Cvar_SetValue ("hand", m_playerConfigMenu.handednessList.curValue, false);
}

static void RateCallback (void *unused)
{
	if (m_playerConfigMenu.rateList.curValue != sizeof(rateTable) / sizeof(*rateTable) - 1)
		cgi.Cvar_SetValue ("rate", rateTable[m_playerConfigMenu.rateList.curValue], false);
}

static void ModelCallback (void *unused)
{
	if (m_playerConfigMenu.modelList.curValue >= m_playerConfigMenu.numPlayerModels)
		m_playerConfigMenu.modelList.curValue = m_playerConfigMenu.numPlayerModels-1;

	m_playerConfigMenu.skinList.itemNames = m_playerConfigMenu.modelInfo[m_playerConfigMenu.modelList.curValue].skinDisplayNames;
	m_playerConfigMenu.skinList.curValue = 0;
	UI_SetupItem (&m_playerConfigMenu.skinList);
}


/*
=============
PlayerConfig_ScanDirectories
=============
*/
static bool PlayerConfig_ScanDirectories ()
{
	char	**modelSkins;
	char	scratch[1024];
	char	directory[1024];
	int		numModelSkins;
	uint32	i, j, k;
	char	*p;

	m_playerConfigMenu.numPlayerModels = 0;

	// Model list
	var modelList = cgi.FS_FindFiles ("players", "players/*/tris.md*", "md*", false, true);
	if (!modelList.Count())
		return false;

	for (i=0 ; i<modelList.Count() ; i++) {
		// skip duplicates
		for (j=i+1 ; j<modelList.Count() ; j++) {
			if (modelList[i].Count() != modelList[j].Count())
				continue;
			if (!modelList[i].CompareCaseInsensitive(modelList[j], modelList[i].Count()-3))
				break;
		}

		if (j != modelList.Count())
			continue;

		// Get the directory
		Q_strncpyz (directory, modelList[i].CString(), sizeof(directory));
		p = strstr (directory, "/tris.md");
		if (p)
			*p = '\0';

		// Get the skins
		var skinList = cgi.FS_FindFiles (directory, "*.pcx", NULL, false, false);
		if (!skinList.Count())
			continue;

		// Make sure it's valid (has a "_i" icon)
		for (j=0, numModelSkins=0 ; j<skinList.Count() ; j++) {
			if (skinList[j].Contains("_i.pcx"))
				continue;

			// Replace ".pcx" with "_i.pcx"
			Q_strncpyz (scratch, skinList[j].CString(), sizeof(scratch));
			p = strstr (scratch, ".pcx");
			if (p)
				*p = '\0';
			Q_strcatz (scratch, "_i.pcx", sizeof(scratch));

			for (k=0 ; k<skinList.Count() ; k++) {
				if (!Q_stricmp (scratch, skinList[k].CString()))
					break;
			}

			if (k == skinList.Count())
				continue;

			numModelSkins++;
		}

		if (!numModelSkins)
			continue;

		// Copy valid skins to list
		modelSkins = (char**)CG_AllocTag (sizeof(char *) * (numModelSkins + 1), CGTAG_MENU);

		for (j=0, numModelSkins=0 ; j<skinList.Count() ; j++) {
			if (skinList[j].Contains("_i.pcx"))
				continue;

			// Replace ".pcx" with "_i.pcx"
			Q_strncpyz (scratch, skinList[j].CString(), sizeof(scratch));
			p = strstr (scratch, ".pcx");
			if (p)
				*p = '\0';
			Q_strcatz (scratch, "_i.pcx", sizeof(scratch));

			for (k=0 ; k<skinList.Count() ; k++) {
				if (!Q_stricmp (scratch, skinList[k].CString()))
					break;
			}

			if (k == skinList.Count())
				continue;

			// Create the skin name
			Q_strncpyz (scratch, skinList[j].CString()+8, sizeof(scratch));
			p = strchr (scratch, '/');
			if (!p)
				p = strchr (scratch, '\\');
			if (p)
				Q_strncpyz (scratch, p+1, sizeof(scratch));

			p = strstr (scratch, ".pcx");
			if (p)
				*p = '\0';

			modelSkins[numModelSkins++] = CG_TagStrDup (scratch, CGTAG_MENU);
		}

		// At this point we have a valid player model
		m_playerConfigMenu.modelInfo[m_playerConfigMenu.numPlayerModels].numSkins = numModelSkins;
		m_playerConfigMenu.modelInfo[m_playerConfigMenu.numPlayerModels].skinDisplayNames = modelSkins;

		// Find the model dir
		Q_strncpyz (scratch, directory+8, sizeof(scratch));

		Q_strncpyz (m_playerConfigMenu.modelInfo[m_playerConfigMenu.numPlayerModels].displayName, scratch, sizeof(m_playerConfigMenu.modelInfo[m_playerConfigMenu.numPlayerModels].displayName));
		Q_strncpyz (m_playerConfigMenu.modelInfo[m_playerConfigMenu.numPlayerModels].directory, scratch, sizeof(m_playerConfigMenu.modelInfo[m_playerConfigMenu.numPlayerModels].directory));

		// Next
		m_playerConfigMenu.numPlayerModels++;
	}

	return (bool)(m_playerConfigMenu.numPlayerModels > 0);
}


/*
=============
PlayerConfigMenu_Init
=============
*/
static int pmicmpfnc (const void *_a, const void *_b)
{
	const modelInfo_t *a = (const modelInfo_t *) _a;
	const modelInfo_t *b = (const modelInfo_t *) _b;

	/*
	** sort by male, female, then alphabetical
	*/
	if (strcmp (a->directory, "male") == 0)
		return -1;
	else if (strcmp (b->directory, "male") == 0)
		return 1;

	if (strcmp (a->directory, "female") == 0)
		return -1;
	else if (strcmp (b->directory, "female") == 0)
		return 1;

	return strcmp (a->directory, b->directory);
}
static void PlayerConfigMenu_Init ()
{
	char	currentDirectory[1024];
	int		currentDirectoryIndex;
	char	currentSkin[1024];
	int		currentSkinIndex;
	int		i, j;

	static char *rateNames[] = {
		"28.8 Modem",
		"33.6 Modem",
		"Single ISDN",
		"Dual ISDN/Cable",
		"T1/LAN",
		"User defined",
		0
	};

	static char *handedness[] = {
		"right",
		"left",
		"center",
		0
	};

	currentDirectoryIndex = 0;
	currentSkinIndex = 0;

	// clamp "hand"
	if (cgi.Cvar_GetIntegerValue ("hand") < 0 || cgi.Cvar_GetIntegerValue ("hand") > 2)
		cgi.Cvar_SetValue ("hand", 0, false);

	// find current directory and skin
	Q_strncpyz (currentDirectory, cgi.Cvar_GetStringValue ("skin"), sizeof(currentDirectory));
	if (strchr (currentDirectory, '/')) {
		Q_strncpyz (currentSkin, strchr (currentDirectory, '/') + 1, sizeof(currentSkin));
		*strchr (currentDirectory, '/' ) = 0;
	}
	else if (strchr (currentDirectory, '\\')) {
		Q_strncpyz (currentSkin, strchr (currentDirectory, '\\') + 1, sizeof(currentSkin));
		*strchr (currentDirectory, '\\') = 0;
	}
	else {
		Q_strncpyz (currentDirectory, "male", sizeof(currentDirectory));
		Q_strncpyz (currentSkin, "grunt", sizeof(currentSkin));
	}

	// sort
	qsort (m_playerConfigMenu.modelInfo, m_playerConfigMenu.numPlayerModels, sizeof(m_playerConfigMenu.modelInfo[0]), pmicmpfnc);

	// generate model and skin list
	memset (m_playerConfigMenu.modelNames, 0, sizeof(m_playerConfigMenu.modelNames));
	for (i=0 ; i<m_playerConfigMenu.numPlayerModels ; i++) {
		m_playerConfigMenu.modelNames[i] = m_playerConfigMenu.modelInfo[i].displayName;
		if (!Q_stricmp (m_playerConfigMenu.modelInfo[i].directory, currentDirectory)) {
			currentDirectoryIndex = i;

			for (j=0 ; j<m_playerConfigMenu.modelInfo[i].numSkins ; j++) {
				if (!Q_stricmp (m_playerConfigMenu.modelInfo[i].skinDisplayNames[j], currentSkin)) {
					currentSkinIndex = j;
					break;
				}
			}
		}
	}

	UI_StartFramework (&m_playerConfigMenu.frameWork, FWF_CENTERHEIGHT);

	m_playerConfigMenu.banner.generic.type		= UITYPE_IMAGE;
	m_playerConfigMenu.banner.generic.flags		= UIF_NOSELECT|UIF_CENTERED;
	m_playerConfigMenu.banner.generic.name		= NULL;
	m_playerConfigMenu.banner.mat				= uiMedia.banners.multiplayer;

	m_playerConfigMenu.header.generic.type		= UITYPE_ACTION;
	m_playerConfigMenu.header.generic.flags		= UIF_NOSELECT|UIF_CENTERED|UIF_MEDIUM|UIF_SHADOW;
	m_playerConfigMenu.header.generic.name		= "Player Configuration";

	m_playerConfigMenu.nameField.generic.type		= UITYPE_FIELD;
	m_playerConfigMenu.nameField.generic.name		= "Name";
	m_playerConfigMenu.nameField.generic.callBack	= 0;
	m_playerConfigMenu.nameField.length				= 20;
	m_playerConfigMenu.nameField.visibleLength		= 20;
	Q_strncpyz (m_playerConfigMenu.nameField.buffer, cgi.Cvar_GetStringValue ("name"), sizeof(m_playerConfigMenu.nameField.buffer));
	m_playerConfigMenu.nameField.cursor				= (int)strlen (cgi.Cvar_GetStringValue ("name"));

	if (m_playerConfigMenu.modelsFound) {
		m_playerConfigMenu.modelList.generic.type		= UITYPE_SPINCONTROL;
		m_playerConfigMenu.modelList.generic.name		= "Model";
		m_playerConfigMenu.modelList.generic.callBack	= ModelCallback;
		m_playerConfigMenu.modelList.curValue			= currentDirectoryIndex;
		m_playerConfigMenu.modelList.itemNames			= m_playerConfigMenu.modelNames;

		m_playerConfigMenu.skinList.generic.type		= UITYPE_SPINCONTROL;
		m_playerConfigMenu.skinList.generic.name		= "Skin";
		m_playerConfigMenu.skinList.generic.callBack	= 0;
		m_playerConfigMenu.skinList.curValue			= currentSkinIndex;
		m_playerConfigMenu.skinList.itemNames			= m_playerConfigMenu.modelInfo[currentDirectoryIndex].skinDisplayNames;
	}

	m_playerConfigMenu.handednessList.generic.type		= UITYPE_SPINCONTROL;
	m_playerConfigMenu.handednessList.generic.name		= "Hand";
	m_playerConfigMenu.handednessList.generic.callBack	= HandednessCallback;
	m_playerConfigMenu.handednessList.curValue			= cgi.Cvar_GetIntegerValue ("hand");
	m_playerConfigMenu.handednessList.itemNames			= handedness;

	for (i=0 ; i < (sizeof(rateTable) / sizeof(*rateTable) - 1) ; i++)
		if (cgi.Cvar_GetFloatValue ("rate") == rateTable[i])
			break;

	m_playerConfigMenu.rateList.generic.type		= UITYPE_SPINCONTROL;
	m_playerConfigMenu.rateList.generic.name		= "Speed";
	m_playerConfigMenu.rateList.generic.callBack	= RateCallback;
	m_playerConfigMenu.rateList.curValue			= i;
	m_playerConfigMenu.rateList.itemNames			= rateNames;

	m_playerConfigMenu.backAction.generic.type		= UITYPE_ACTION;
	m_playerConfigMenu.backAction.generic.flags		= UIF_SHADOW;
	m_playerConfigMenu.backAction.generic.name		= S_COLOR_GREEN"< back";
	m_playerConfigMenu.backAction.generic.callBack	= Menu_Pop;
	m_playerConfigMenu.backAction.generic.statusBar	= "Back a menu";

	UI_AddItem (&m_playerConfigMenu.frameWork,		&m_playerConfigMenu.banner);
	UI_AddItem (&m_playerConfigMenu.frameWork,		&m_playerConfigMenu.header);

	UI_AddItem (&m_playerConfigMenu.frameWork,		&m_playerConfigMenu.nameField);
	if (m_playerConfigMenu.modelsFound) {
		UI_AddItem (&m_playerConfigMenu.frameWork,		&m_playerConfigMenu.modelList);
		if (m_playerConfigMenu.skinList.itemNames)
			UI_AddItem (&m_playerConfigMenu.frameWork,	&m_playerConfigMenu.skinList);
	}

	UI_AddItem (&m_playerConfigMenu.frameWork,		&m_playerConfigMenu.handednessList);
	UI_AddItem (&m_playerConfigMenu.frameWork,		&m_playerConfigMenu.rateList);

	UI_AddItem (&m_playerConfigMenu.frameWork,		&m_playerConfigMenu.backAction);

	UI_FinishFramework (&m_playerConfigMenu.frameWork, true);
}


/*
=============
PlayerConfigMenu_Close
=============
*/
struct sfx_t *PlayerConfigMenu_Close ()
{
	int		i, j;

	// Set name
	cgi.Cvar_Set ("name", m_playerConfigMenu.nameField.buffer, false);

	if (m_playerConfigMenu.modelsFound) {
		// Set skin
		cgi.Cvar_Set ("skin", Q_VarArgs ("%s/%s", 
			m_playerConfigMenu.modelInfo[m_playerConfigMenu.modelList.curValue].directory, 
			m_playerConfigMenu.modelInfo[m_playerConfigMenu.modelList.curValue].skinDisplayNames[m_playerConfigMenu.skinList.curValue]), false);
	}

	// Free model lists
	for (i=0 ; i<m_playerConfigMenu.numPlayerModels ; i++) {
		// Free skins
		for (j=0 ; j<m_playerConfigMenu.modelInfo[i].numSkins ; j++) {
			if (m_playerConfigMenu.modelInfo[i].skinDisplayNames[j])
				CG_MemFree (m_playerConfigMenu.modelInfo[i].skinDisplayNames[j]);
			m_playerConfigMenu.modelInfo[i].skinDisplayNames[j] = NULL;
		}

		// Free display names
		CG_MemFree (m_playerConfigMenu.modelInfo[i].skinDisplayNames);
		m_playerConfigMenu.modelInfo[i].skinDisplayNames = 0;
		m_playerConfigMenu.modelInfo[i].numSkins = 0;
	}

	m_playerConfigMenu.modelsFound = false;
	memset (&m_playerConfigMenu.modelInfo, 0, sizeof(m_playerConfigMenu.modelInfo));
	memset (&m_playerConfigMenu.modelNames, 0, sizeof(m_playerConfigMenu.modelNames));
	m_playerConfigMenu.numPlayerModels = 0;

	return uiMedia.sounds.menuOut;
}


/*
=============
PlayerConfigMenu_Draw
=============
*/
void PlayerConfigMenu_Draw ()
{
	refEntity_t				entity[2];
	refDef_t				refDef;
	vec3_t					angles;
	struct refMaterial_t	*icon;
	float					y, xoffset;

	// Initialize if necessary
	if (!m_playerConfigMenu.frameWork.initialized)
		PlayerConfigMenu_Init ();

	// Dynamically position
	xoffset = -(UIFT_SIZE * 9);

	m_playerConfigMenu.frameWork.x				= cg.refConfig.vidWidth * 0.5f;
	m_playerConfigMenu.frameWork.y				= m_playerConfigMenu.modelsFound ? -((4 * 6) * UIFT_SIZE) * 0.5f : 0;

	m_playerConfigMenu.banner.generic.x			= 0;
	m_playerConfigMenu.banner.generic.y			= 0;

	y = m_playerConfigMenu.banner.height * UI_SCALE;

	m_playerConfigMenu.header.generic.x			= 0;
	m_playerConfigMenu.header.generic.y			= y += UIFT_SIZEINC;

	m_playerConfigMenu.nameField.generic.x		= xoffset;
	m_playerConfigMenu.nameField.generic.y		= y += UIFT_SIZEINC + UIFT_SIZEINCMED;

	if (m_playerConfigMenu.modelsFound) {
		m_playerConfigMenu.modelList.generic.x	= xoffset;
		m_playerConfigMenu.modelList.generic.y	= y += (UIFT_SIZEINC*2);
		m_playerConfigMenu.skinList.generic.x	= xoffset;
		m_playerConfigMenu.skinList.generic.y	= y += UIFT_SIZEINC;
	}
	else {
		y += UIFT_SIZEINC;
	}

	m_playerConfigMenu.handednessList.generic.x	= xoffset;
	m_playerConfigMenu.handednessList.generic.y	= y += UIFT_SIZEINC;
	m_playerConfigMenu.rateList.generic.x		= xoffset;
	m_playerConfigMenu.rateList.generic.y		= y += UIFT_SIZEINC;
	m_playerConfigMenu.backAction.generic.x		= xoffset + UIFT_SIZE;
	m_playerConfigMenu.backAction.generic.y		= y += (UIFT_SIZEINC*2);

	// Render
	UI_DrawInterface (&m_playerConfigMenu.frameWork);

	if (!m_playerConfigMenu.modelsFound) {
		char	str[64];

		Q_strncpyz (str, S_COLOR_RED S_STYLE_SHADOW "No player models found!", sizeof(str));
		cgi.R_DrawString (NULL,
			(cg.refConfig.vidWidth-((strlen(str)-4)*UIFT_SIZELG)) * 0.5f,
			m_playerConfigMenu.frameWork.y + (y + UIFT_SIZEINC),
			UIFT_SCALELG, UIFT_SCALELG, 0, str, Q_BColorWhite);
		return;
	}

	// Setup refDef
	memset (&refDef, 0, sizeof(refDef));

	refDef.x = (cg.refConfig.vidWidth * 0.5f) - (UIFT_SIZE * 10);
	refDef.y = m_playerConfigMenu.frameWork.y + (y + UIFT_SIZEINC);
	refDef.width = (UIFT_SIZE * 20);
	refDef.height = ((4 * 6) * UIFT_SIZE);
	refDef.fovX = refDef.fovY = 20.0f / UIFT_SCALE;
	refDef.time = cg.realTime * 0.001f;
	refDef.areaBits = NULL;
	refDef.rdFlags = RDF_NOWORLDMODEL;
	Matrix3_Identity(refDef.viewAxis);

	float yaw = (int)(refDef.time * 100) % 360;

	memset (entity, 0, sizeof(refEntity_t) * 2);

	// Player
	entity[0].model = cgi.R_RegisterModel (Q_VarArgs ("players/%s/tris.md2",
		m_playerConfigMenu.modelInfo[m_playerConfigMenu.modelList.curValue].directory));
	if (entity[0].model) {
		entity[0].skin = cgi.R_RegisterSkin (Q_VarArgs ("players/%s/%s.pcx",
			m_playerConfigMenu.modelInfo[m_playerConfigMenu.modelList.curValue].directory,
			m_playerConfigMenu.modelInfo[m_playerConfigMenu.modelList.curValue].skinDisplayNames[m_playerConfigMenu.skinList.curValue]));

		if (entity[0].skin) {
			entity[0].origin[0] = entity[0].oldOrigin[0] = refDef.x;
			entity[0].origin[1] = entity[0].oldOrigin[1] = 0;
			entity[0].origin[2] = entity[0].oldOrigin[2] = 0;

			entity[0].color[0] = 255;
			entity[0].color[1] = 255;
			entity[0].color[2] = 255;
			entity[0].color[3] = 255;

			entity[0].flags = RF_DEPTHHACK|RF_NOSHADOW;
			entity[0].frame = 0;
			entity[0].oldFrame = 0;
			entity[0].backLerp = 0.0f;
			entity[0].scale = 1;

			angles[0] = 0;
			angles[1] = yaw;
			angles[2] = 0;

			Angles_Matrix3 (angles, entity[0].axis);
		}
	}

	// Weapon
	entity[1].model = cgi.R_RegisterModel (Q_VarArgs ("players/%s/weapon.md2",
		m_playerConfigMenu.modelInfo[m_playerConfigMenu.modelList.curValue].directory));

	if (0) { //entity[1].model) {
		entity[1].origin[0] = entity[1].oldOrigin[0] = refDef.x;
		entity[1].origin[1] = entity[1].oldOrigin[1] = 0;
		entity[1].origin[2] = entity[1].oldOrigin[2] = 0;

		entity[1].color[0] = 255;
		entity[1].color[1] = 255;
		entity[1].color[2] = 255;
		entity[1].color[3] = 255;

		entity[1].flags = RF_DEPTHHACK|RF_NOSHADOW;
		entity[1].frame = 0;
		entity[1].oldFrame = 0;
		entity[1].backLerp = 0.0f;
		entity[1].scale = 1;

		angles[0] = 0;
		angles[1] = yaw;
		angles[2] = 0;

		Angles_Matrix3 (angles, entity[1].axis);
	}

	// Clear the scene and add the entities
	cgi.R_ClearScene ();
	if (entity[0].skin) {
		cgi.R_AddEntity (&entity[0]);
		if (entity[1].model)
			cgi.R_AddEntity (&entity[1]);
	}

	// Render the scene
	cgi.R_RenderScene (&refDef);

	// Pic selection
	icon = cgi.R_RegisterPic (Q_VarArgs ("players/%s/%s_i.pcx", 
			m_playerConfigMenu.modelInfo[m_playerConfigMenu.modelList.curValue].directory,
			m_playerConfigMenu.modelInfo[m_playerConfigMenu.modelList.curValue].skinDisplayNames[m_playerConfigMenu.skinList.curValue]));
	if (icon) {
		int		width;
		int		height;

		cgi.R_GetImageSize (icon, &width, &height);
		cgi.R_DrawPic (icon, 0, QuadVertices().SetVertices(refDef.x - (width * UIFT_SCALE), refDef.y,
					width*UIFT_SCALE, height*UIFT_SCALE), Q_BColorWhite);
	}
}


/*
=============
UI_PlayerConfigMenu_f
=============
*/
void UI_PlayerConfigMenu_f ()
{
	float	midrow = (cg.refConfig.vidWidth*0.5) - (18*UIFT_SIZEMED);
	float	midcol = (cg.refConfig.vidHeight*0.5) - (3*UIFT_SIZEMED);

	// show box
	UI_DrawTextBox (midrow, midcol, UIFT_SCALEMED, 36, 4);
	cgi.R_DrawString (NULL, midrow + (UIFT_SIZEMED*2), midcol + UIFT_SIZEMED, UIFT_SCALEMED, UIFT_SCALEMED, 0,		"       --- PLEASE WAIT! ---       ",	Q_BColorGreen);
	cgi.R_DrawString (NULL, midrow + (UIFT_SIZEMED*2), midcol + (UIFT_SIZEMED*2), UIFT_SCALEMED, UIFT_SCALEMED, 0,	"Player models, skins and icons are",	Q_BColorGreen);
	cgi.R_DrawString (NULL, midrow + (UIFT_SIZEMED*2), midcol + (UIFT_SIZEMED*3), UIFT_SCALEMED, UIFT_SCALEMED, 0,	"being listed, and this might take ",	Q_BColorGreen);
	cgi.R_DrawString (NULL, midrow + (UIFT_SIZEMED*2), midcol + (UIFT_SIZEMED*4), UIFT_SCALEMED, UIFT_SCALEMED, 0,	"a minute so, please be patient.   ",	Q_BColorGreen);

	cgi.R_EndFrame ();	// the text box won't show up unless we do a buffer swap

	// load models
	m_playerConfigMenu.modelsFound = PlayerConfig_ScanDirectories ();
	PlayerConfigMenu_Init ();

	M_PushMenu (&m_playerConfigMenu.frameWork, PlayerConfigMenu_Draw, PlayerConfigMenu_Close, M_KeyHandler);
}
