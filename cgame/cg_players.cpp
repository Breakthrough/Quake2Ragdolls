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
// cg_players.c
//

#include "cg_local.h"

/*
================
CG_LoadClientinfo
================
*/
const char *ragdollPieces[] =
{
	"head.md2",
	"pelvis.md2",
	"chest.md2",
	"rupperarm.md2",
	"rlowerarm.md2",
	"lupperarm.md2",
	"llowerarm.md2",
	"rupperleg.md2",
	"rlowerleg.md2",
	"lupperleg.md2",
	"llowerleg.md2"
};

void CG_LoadClientinfo (clientInfo_t *ci, char *skin)
{
	char		modelName[MAX_CFGSTRLEN];
	char		skinName[MAX_CFGSTRLEN];
	char		modelFilename[MAX_QPATH];
	char		skinFilename[MAX_QPATH];
	char		*t;
	int			i;

	ci->Clear();

	Q_strncpyz (ci->cInfo, skin, sizeof(ci->cInfo));

	// Isolate the player's name
	Q_strncpyz (ci->name, skin, sizeof(ci->name));
	t = strstr (skin, "\\");
	if (t) {
		ci->name[t-skin] = 0;
		skin = t+1;
	}

	if (cl_noskins->intVal || *skin == 0) {
		// Model
		Q_snprintfz (modelFilename, sizeof(modelFilename), "players/male/tris.md2");
		ci->model = cgi.R_RegisterModel (modelFilename);

		// Weapon
		ci->weaponModels.Add(cgi.R_RegisterModel ("players/male/weapon.md2"));

		for (i = 0; i < Items::WeaponRegistryCount(); ++i)
		{
			var item = Items::WeaponFromID(i);
			ci->weaponModels.Add(cgi.R_RegisterModel(String::Format("players/male/%s.md2", item->weapmodel).CString()));
		}

		// Skin
		Q_snprintfz (skinFilename, sizeof(skinFilename), "players/male/grunt.tga");
		ci->skin = cgi.R_RegisterSkin (skinFilename);

		// Icon
		Q_snprintfz (ci->iconName, sizeof(ci->iconName), "players/male/grunt_i.tga");
		ci->icon = cgi.R_RegisterPic (ci->iconName);

		for (i = 0; i < RAG_COUNT; ++i)
		{
			String name = String::Format("players/male/ragdoll/%s", ragdollPieces[i]);
			ci->ragdollPieces[i] = cgi.R_RegisterModel(name.CString());
		}
	}
	else {
		// Isolate the model name
		Q_strncpyz (modelName, skin, sizeof(modelName));
		t = strstr (modelName, "/");
		if (!t)
			t = strstr (modelName, "\\");
		if (!t)
			t = modelName;
		*t = '\0';

		// Isolate the skin name
		if (skin[strlen(modelName)] == '/' || skin[strlen(modelName)] == '\\')
			Q_strncpyz (skinName, skin+strlen(modelName)+1, sizeof(skinName));
		else
			skinName[0] = '\0';

		// Model file
		Q_snprintfz (modelFilename, sizeof(modelFilename), "players/%s/tris.md2", modelName);
		ci->model = cgi.R_RegisterModel (modelFilename);
		if (!ci->model) {
			Q_strncpyz (modelName, "male", sizeof(modelName));
			Q_snprintfz (modelFilename, sizeof(modelFilename), "players/male/tris.md2");
			ci->model = cgi.R_RegisterModel (modelFilename);
		}

		for (i = 0; i < RAG_COUNT; ++i)
		{
			String name = String::Format("players/%s/ragdoll/%s", modelName, ragdollPieces[i]);
			ci->ragdollPieces[i] = cgi.R_RegisterModel(name.CString());

			if (ci->ragdollPieces[i] == null)
			{
				name = String::Format("players/male/ragdoll/%s", ragdollPieces[i]);
				ci->ragdollPieces[i] = cgi.R_RegisterModel(name.CString());
			}
		}

		// Skin file
		Q_snprintfz (skinFilename, sizeof(skinFilename), "players/%s/%s.tga", modelName, skinName);
		ci->skin = cgi.R_RegisterSkin (skinFilename);

		// If we don't have the skin and the model wasn't male, see if the male has it (this is for CTF's skins)
		if (!ci->skin && Q_stricmp (modelName, "male")) {
			// Change model to male
			Q_strncpyz (modelName, "male", sizeof(modelName));
			Q_snprintfz (modelFilename, sizeof(modelFilename), "players/male/tris.md2");
			ci->model = cgi.R_RegisterModel (modelFilename);

			// See if the skin exists for the male model
			Q_snprintfz (skinFilename, sizeof(skinFilename), "players/%s/%s.tga", modelName, skinName);
			ci->skin = cgi.R_RegisterSkin (skinFilename);
		}

		// If we still don't have a skin, it means that the male model didn't have it, so default to grunt
		if (!ci->skin) {
			// See if the skin exists for the male model
			Q_snprintfz (skinFilename, sizeof(skinFilename), "players/%s/grunt.tga", modelName, skinName);
			ci->skin = cgi.R_RegisterSkin (skinFilename);
		}

		// Weapon file
		/*for (i=0 ; i<cg_numWeaponModels ; i++) {
			Q_snprintfz (weaponFilename, sizeof(weaponFilename), "players/%s/%s", modelName, cg_weaponModels[i]);
			ci->weaponModels[i] = cgi.R_RegisterModel (weaponFilename);
			if (!ci->weaponModels[i] && !Q_stricmp (modelName, "cyborg")) {
				// Try male
				Q_snprintfz (weaponFilename, sizeof(weaponFilename), "players/male/%s", cg_weaponModels[i]);
				ci->weaponModels[i] = cgi.R_RegisterModel (weaponFilename);
			}

			if (!cl_vwep->intVal)
				break; // Only one when vwep is off
		}*/

		ci->weaponModels.Add(cgi.R_RegisterModel (String::Format("players/%s/weapon.md2", modelName).CString()));

		if (cl_vwep->intVal)
		{
			for (i = 0; i < Items::WeaponRegistryCount(); ++i)
			{
				var item = Items::WeaponFromID(i);
				ci->weaponModels.Add(cgi.R_RegisterModel(String::Format("players/%s/%s.md2", modelName, item->weapmodel).CString()));
			}
		}

		// Icon file
		Q_snprintfz (ci->iconName, sizeof(ci->iconName), "players/%s/%s_i.tga", modelName, skinName);
		ci->icon = cgi.R_RegisterPic (ci->iconName);
	}

	// Must have loaded all data types to be valid
	if (!ci->skin || !ci->icon || !ci->model || !ci->weaponModels.Count() || ci->weaponModels[0] == null) {
		ci->skin = NULL;
		ci->icon = NULL;
		ci->model = NULL;
		ci->weaponModels.Clear();
		return;
	}
}


/*
==============
CG_StartSound
==============
*/
void CG_StartSound (vec3_t origin, int entNum, EEntSndChannel entChannel, int soundNum, float volume, float attenuation, float timeOffset)
{
	if (!cg.soundCfgStrings[soundNum] && cg.configStrings[CS_SOUNDS+soundNum][0])
		cg.soundCfgStrings[soundNum] = cgi.Snd_RegisterSound (cg.configStrings[CS_SOUNDS+soundNum]);

	// FIXME: move sexed sound registration here
	cgi.Snd_StartSound (origin, entNum, entChannel, cg.soundCfgStrings[soundNum], volume, attenuation, timeOffset);
}


/*
==============
CG_FixUpGender
==============
*/
void CG_FixUpGender ()
{
	char	*p, sk[80];

	if (!gender_auto->intVal)
		return;

	if (gender->modified) {
		// Was set directly, don't override the user
		gender->modified = false;
		return;
	}

	Q_strncpyz (sk, skin->string, sizeof(sk));
	if ((p = strchr (sk, '/')) != NULL)
		*p = 0;

	if (!Q_stricmp (sk, "male") || !Q_stricmp (sk, "cyborg"))
		cgi.Cvar_Set ("gender", "male", false);
	else if (!Q_stricmp (sk, "female") || !Q_stricmp (sk, "crackhor"))
		cgi.Cvar_Set ("gender", "female", false);
	else
		cgi.Cvar_Set ("gender", "none", false);
	gender->modified = false;
}
