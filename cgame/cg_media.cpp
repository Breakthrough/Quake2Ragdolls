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
// cg_media.c
//

#include "cg_local.h"

vec3_t	cg_randVels[NUMVERTEXNORMALS];

/*
=============================================================================

	MEDIA INITIALIZATION

=============================================================================
*/

/*
================
CG_InitBaseMedia
================
*/
void CG_InitBaseMedia ()
{
	int		i;

	// CGame media
	cgMedia.noTexture				= cgi.R_RegisterPic ("***r_noTexture***");
	cgMedia.whiteTexture			= cgi.R_RegisterPic ("***r_whiteTexture***");
	cgMedia.blackTexture			= cgi.R_RegisterPic ("***r_blackTexture***");

	cgMedia.tileBackMat				= cgi.R_RegisterPic ("pics/backtile.tga");

	cgMedia.alienInfraredVision		= cgi.R_RegisterPic ("alienInfraredVision");
	cgMedia.infraredGoggles			= cgi.R_RegisterPic ("infraredGoggles");

	cgMedia.loadSplash				= cgi.R_RegisterPic ("egl/ui/loadscreen/loadsplash.tga");
	cgMedia.loadBarPos				= cgi.R_RegisterPic ("egl/ui/loadscreen/barpos.tga");
	cgMedia.loadBarNeg				= cgi.R_RegisterPic ("egl/ui/loadscreen/barneg.tga");
	cgMedia.loadNoMapShot			= cgi.R_RegisterPic ("egl/ui/loadscreen/unknownmap.tga");

	cgMedia.defaultFont				= cgi.R_RegisterFont ("default");

	// Menu image media
	uiMedia.bgBig					= cgi.R_RegisterPic ("egl/ui/bg_big.tga");

	uiMedia.cursorMat				= cgi.R_RegisterPic ("egl/ui/cursor.tga");
	uiMedia.cursorHoverMat			= cgi.R_RegisterPic ("egl/ui/cursorhover.tga");

	cgi.R_GetImageSize (uiMedia.cursorMat, &uiState.cursorW, &uiState.cursorH);

	// Banners
	uiMedia.banners.addressBook		= cgi.R_RegisterPic ("pics/m_banner_addressbook.tga");
	uiMedia.banners.multiplayer		= cgi.R_RegisterPic ("pics/m_banner_multiplayer.tga");
	uiMedia.banners.startServer		= cgi.R_RegisterPic ("pics/m_banner_start_server.tga");
	uiMedia.banners.joinServer		= cgi.R_RegisterPic ("pics/m_banner_join_server.tga");
	uiMedia.banners.options			= cgi.R_RegisterPic ("pics/m_banner_options.tga");
	uiMedia.banners.game			= cgi.R_RegisterPic ("pics/m_banner_game.tga");
	uiMedia.banners.loadGame		= cgi.R_RegisterPic ("pics/m_banner_load_game.tga");
	uiMedia.banners.saveGame		= cgi.R_RegisterPic ("pics/m_banner_save_game.tga");
	uiMedia.banners.video			= cgi.R_RegisterPic ("pics/m_banner_video.tga");
	uiMedia.banners.quit			= cgi.R_RegisterPic ("pics/m_main_quit.tga");

	// Main menu cursors
	for (i=0 ; i<MAINMENU_CURSOR_NUMFRAMES ; i++)
		uiMedia.menus.mainCursors[i]	= cgi.R_RegisterPic (Q_VarArgs ("pics/m_cursor%d.tga", i));

	// Main menu
	uiMedia.menus.mainPlaque			= cgi.R_RegisterPic ("pics/m_main_plaque.tga");
	uiMedia.menus.mainLogo				= cgi.R_RegisterPic ("pics/m_main_logo.tga");

	uiMedia.menus.mainGame				= cgi.R_RegisterPic ("pics/m_main_game.tga");
	uiMedia.menus.mainMultiplayer		= cgi.R_RegisterPic ("pics/m_main_multiplayer.tga");
	uiMedia.menus.mainOptions			= cgi.R_RegisterPic ("pics/m_main_options.tga");
	uiMedia.menus.mainVideo				= cgi.R_RegisterPic ("pics/m_main_video.tga");
	uiMedia.menus.mainQuit				= cgi.R_RegisterPic ("pics/m_main_quit.tga");

	uiMedia.menus.mainGameSel			= cgi.R_RegisterPic ("pics/m_main_game_sel.tga");
	uiMedia.menus.mainMultiplayerSel	= cgi.R_RegisterPic ("pics/m_main_multiplayer_sel.tga");
	uiMedia.menus.mainOptionsSel		= cgi.R_RegisterPic ("pics/m_main_options_sel.tga");
	uiMedia.menus.mainVideoSel			= cgi.R_RegisterPic ("pics/m_main_video_sel.tga");
	uiMedia.menus.mainQuitSel			= cgi.R_RegisterPic ("pics/m_main_quit_sel.tga");
}


/*
================
CG_MapMediaInit
================
*/
static void CG_MapMediaInit ()
{
	float		rotate;
	vec3_t		axis;
	int			i, j;

	if (!cg.configStrings[CS_MODELS+1][0])
		return;

	// Register models, pics, and skins
	CG_LoadingString ("Loading map and textures...");
	cgi.R_RegisterMap (cg.configStrings[CS_MODELS+1]);

	CG_IncLoadingVal();

	// Register map effects
	CG_LoadingString ("Loading map fx...");
	CG_MapFXLoad (cg.configStrings[CS_MODELS+1]);

	// Register locations
	CG_LoadingString ("Locations...");
	CG_LoadLocations (cg.configStrings[CS_MODELS+1]);

	CG_IncLoadingVal();

	// Load models
	CG_LoadingString ("Loading models...");

	for (i=1 ; i<MAX_CS_MODELS && cg.configStrings[CS_MODELS+i][0] ; i++) ;

	for (i=1 ; i<MAX_CS_MODELS && cg.configStrings[CS_MODELS+i][0] ; i++) {
		switch (cg.configStrings[CS_MODELS+i][0]) {
		case '*':
			CG_LoadingFilename (Q_VarArgs ("Inline: #%s", cg.configStrings[CS_MODELS+i]+1));
			cg.modelCfgDraw[i] = cgi.R_RegisterModel (cg.configStrings[CS_MODELS+i]);
			cg.modelCfgClip[i] = cgi.CM_InlineModel (cg.configStrings[CS_MODELS+i]);
			break;

		default:
			CG_LoadingFilename (cg.configStrings[CS_MODELS+i]);
			cg.modelCfgDraw[i] = cgi.R_RegisterModel (cg.configStrings[CS_MODELS+i]);
			cg.modelCfgClip[i] = NULL;
			break;
		}

		CG_IncLoadingVal();
		CG_LoadingFilename (0);
	}

	// Images
	CG_LoadingString ("Loading images...");
	for (i=1 ; i<MAX_CS_IMAGES && cg.configStrings[CS_IMAGES+i][0] ; i++) ;

	for (i=1 ; i<MAX_CS_IMAGES && cg.configStrings[CS_IMAGES+i][0] ; i++) {
		CG_LoadingFilename (cg.configStrings[CS_IMAGES+i]);
		cg.imageCfgStrings[i] = CG_RegisterPic (cg.configStrings[CS_IMAGES+i]);
		CG_IncLoadingVal();
	}
	CG_LoadingFilename (0);

	// Clients
	CG_LoadingString ("Loading clientinfo...");
	for (i=0, j=0 ; i<MAX_CS_CLIENTS ; i++)
	{
		if (cg.configStrings[CS_PLAYERSKINS+i][0])
			j++;
	}

	for (i=0 ; i<j ; i++)
	{
		if (cg.configStrings[CS_PLAYERSKINS+i][0])
		{
			CG_LoadingFilename(Q_VarArgs ("Client #%i", i));
			CG_ParseClientinfo(i);
			CG_IncLoadingVal();
		}
	}

	CG_LoadingFilename ("Base client info");
	CG_LoadClientinfo (&cg.baseClientInfo, "unnamed\\male/grunt");
	CG_LoadingFilename (0);

	CG_IncLoadingVal();

	// Set sky textures and speed
	CG_LoadingString ("Loading sky env...");
	rotate = (float)atof (cg.configStrings[CS_SKYROTATE]);
	sscanf (cg.configStrings[CS_SKYAXIS], "%f %f %f", &axis[0], &axis[1], &axis[2]);
	cgi.R_SetSky (cg.configStrings[CS_SKY], rotate, axis);
}


/*
================
CG_ModelMediaInit
================
*/
static void CG_ModelMediaInit ()
{
	CG_LoadingString ("Loading model media...");
	CG_LoadingFilename ("Segment models");

	cgMedia.parasiteSegmentModel	= cgi.R_RegisterModel ("models/monsters/parasite/segment/tris.md2");
	cgMedia.grappleCableModel		= cgi.R_RegisterModel ("models/ctf/segment/tris.md2");
	cgMedia.powerScreenModel		= cgi.R_RegisterModel ("models/items/armor/effect/tris.md2");

	CG_IncLoadingVal();
	CG_LoadingFilename ("Local models");

	cgMedia.brassMGModel			= cgi.R_RegisterModel ("egl/models/brass/mg/tris.md2e");
	cgMedia.brassSGModel			= cgi.R_RegisterModel ("egl/models/brass/sg/tris.md2e");

	CG_IncLoadingVal();
	CG_LoadingFilename ("Beam models");

	cgMedia.lightningModel			= cgi.R_RegisterModel ("models/proj/lightning/tris.md2");
	cgMedia.heatBeamModel			= cgi.R_RegisterModel ("models/proj/beam/tris.md2");
	cgMedia.monsterHeatBeamModel	= cgi.R_RegisterModel ("models/proj/widowbeam/tris.md2");

	CG_IncLoadingVal();
	CG_LoadingFilename ("Disguise models");

	cgMedia.maleDisguiseModel		= cgi.R_RegisterModel ("players/male/tris.md2");
	cgMedia.femaleDisguiseModel		= cgi.R_RegisterModel ("players/female/tris.md2");
	cgMedia.cyborgDisguiseModel		= cgi.R_RegisterModel ("players/cyborg/tris.md2");

	CG_LoadingFilename (0);
}


/*
================
CG_CrosshairMaterialInit
================
*/
void CG_CrosshairMaterialInit ()
{
	crosshair->modified = false;
	if (crosshair->intVal) {
		crosshair->intVal = (crosshair->intVal < 0) ? 0 : crosshair->intVal;

		cgMedia.crosshairMat = cgi.R_RegisterPic (Q_VarArgs ("pics/ch%d.tga", crosshair->intVal));
	}
}


/*
================
CG_PicMediaInit
================
*/
static void CG_PicMediaInit ()
{
	CG_LoadingString ("Loading image media...");
	CG_LoadingFilename ("Crosshair");

	CG_CrosshairMaterialInit ();

	CG_IncLoadingVal();
	CG_LoadingFilename ("HUDs");

	cgMedia.hudFieldMat		= cgi.R_RegisterPic ("pics/field_3.tga");
	cgMedia.hudInventoryMat	= cgi.R_RegisterPic ("pics/inventory.tga");
	cgMedia.hudNetMat		= cgi.R_RegisterPic ("pics/net.tga");
	cgMedia.hudPausedMat	= cgi.R_RegisterPic ("pics/pause.tga");

	// HUDs
	HUD_LoadHuds();

	CG_IncLoadingVal();
	CG_LoadingFilename ("Disguise skins");

	cgMedia.maleDisguiseSkin	= cgi.R_RegisterSkin ("players/male/disguise.tga");
	cgMedia.femaleDisguiseSkin	= cgi.R_RegisterSkin ("players/female/disguise.tga");
	cgMedia.cyborgDisguiseSkin	= cgi.R_RegisterSkin ("players/cyborg/disguise.tga");

	CG_LoadingFilename ("Shell skins");
	cgMedia.modelShellGod		= cgi.R_RegisterSkin ("shell_god");
	cgMedia.modelShellHalfDam	= cgi.R_RegisterSkin ("shell_halfdam");
	cgMedia.modelShellDouble	= cgi.R_RegisterSkin ("shell_double");
	cgMedia.modelShellRed		= cgi.R_RegisterSkin ("shell_red");
	cgMedia.modelShellGreen		= cgi.R_RegisterSkin ("shell_green");
	cgMedia.modelShellBlue		= cgi.R_RegisterSkin ("shell_blue");

	CG_LoadingFilename (0);
}


/*
================
CG_FXMediaInit
================
*/
static void CG_FXMediaInit ()
{
	struct refMaterial_t *tempMat, *greenMat;
	float yOffset;

	// Particles / Decals
	CG_LoadingString ("Loading effect media...");
	CG_LoadingFilename ("Particles");

	for (int i=0 ; i<(NUMVERTEXNORMALS*3) ; i++)
		cg_randVels[0][i] = (frand () * 255) * 0.01f;

	// Set default subUV coords
	for (int i=0 ; i<PT_PICTOTAL ; i++)
	{
		Vec4Set(cgMedia.particleCoords[i], 0, 0, 1, 1);
	}

	cgMedia.particleTable[PT_BFG_DOT]		= cgi.R_RegisterPoly ("egl/parts/bfg_dot.tga");

	cgMedia.particleTable[PT_BLASTER_BLUE]	= cgi.R_RegisterPoly ("egl/parts/blaster_blue.tga");
	cgMedia.particleTable[PT_BLASTER_GREEN]	= cgi.R_RegisterPoly ("egl/parts/blaster_green.tga");
	cgMedia.particleTable[PT_BLASTER_RED]	= cgi.R_RegisterPoly ("egl/parts/blaster_red.tga");

	cgMedia.particleTable[PT_IONTAIL]		= cgi.R_RegisterPoly ("egl/parts/iontail.tga");
	cgMedia.particleTable[PT_IONTIP]		= cgi.R_RegisterPoly ("egl/parts/iontip.tga");
	cgMedia.particleTable[PT_ITEMRESPAWN]	= cgi.R_RegisterPoly ("egl/parts/respawn_dots.tga");
	cgMedia.particleTable[PT_ENGYREPAIR_DOT]= cgi.R_RegisterPoly ("egl/parts/engy_repair_dot.tga");
	cgMedia.particleTable[PT_PHALANXTIP]	= cgi.R_RegisterPoly ("egl/parts/phalanxtip.tga");

	cgMedia.particleTable[PT_GENERIC]		= cgi.R_RegisterPoly ("egl/parts/generic.tga");
	cgMedia.particleTable[PT_GENERIC_GLOW]	= cgi.R_RegisterPoly ("egl/parts/generic_glow.tga");

	cgMedia.particleTable[PT_SMOKE]			= cgi.R_RegisterPoly ("egl/parts/smoke1.tga");
	cgMedia.particleTable[PT_SMOKE2]		= cgi.R_RegisterPoly ("egl/parts/smoke2.tga");

	cgMedia.particleTable[PT_SMOKEGLOW]		= cgi.R_RegisterPoly ("egl/parts/smoke_glow.tga");
	cgMedia.particleTable[PT_SMOKEGLOW2]	= cgi.R_RegisterPoly ("egl/parts/smoke_glow2.tga");

	cgMedia.particleTable[PT_BLUEFIRE]		= cgi.R_RegisterPoly ("egl/parts/bluefire.tga");

	tempMat = cgi.R_RegisterPoly("egl/parts/firetable.tga");
	yOffset = 0;
	for (int i=0 ; i<4 ; i++)
	{
		cgMedia.particleTable[PT_FIRE1+i] = tempMat;

		if (i == 2)
			yOffset += 0.5f;

		cgMedia.particleCoords[PT_FIRE1+i][0] = (i&1) * 0.5f;
		cgMedia.particleCoords[PT_FIRE1+i][1] = yOffset;
		cgMedia.particleCoords[PT_FIRE1+i][2] = cgMedia.particleCoords[PT_FIRE1+i][0] + 0.5f;
		cgMedia.particleCoords[PT_FIRE1+i][3] = cgMedia.particleCoords[PT_FIRE1+i][1] + 0.5f;
	}

	cgMedia.particleTable[PT_EMBERS1]		= cgi.R_RegisterPoly ("egl/parts/embers1.tga");
	cgMedia.particleTable[PT_EMBERS2]		= cgi.R_RegisterPoly ("egl/parts/embers2.tga");
	cgMedia.particleTable[PT_EMBERS3]		= cgi.R_RegisterPoly ("egl/parts/embers3.tga");

	tempMat = cgi.R_RegisterPoly("egl/parts/bloodparticles.tga");
	greenMat = cgi.R_RegisterPoly("egl/parts/bloodparticles_green.tga");

	yOffset = 0;
	for (int i=0 ; i<8 ; i++)
	{
		cgMedia.particleTable[PT_BLOODTRAIL+i] = tempMat;
		cgMedia.particleTable[PT_GRNBLOODTRAIL+i] = greenMat;

		if (i && !(i&1))
			yOffset += 0.25f;

		cgMedia.particleCoords[PT_BLOODTRAIL+i][0] = (i&1) * 0.25f;
		cgMedia.particleCoords[PT_BLOODTRAIL+i][1] = yOffset;
		cgMedia.particleCoords[PT_BLOODTRAIL+i][2] = cgMedia.particleCoords[PT_BLOODTRAIL+i][0] + 0.25f;
		cgMedia.particleCoords[PT_BLOODTRAIL+i][3] = cgMedia.particleCoords[PT_BLOODTRAIL+i][1] + 0.25f;

		Vec4Copy(cgMedia.particleCoords[PT_BLOODTRAIL+i], cgMedia.particleCoords[PT_GRNBLOODTRAIL+i]);
	}

	for (int i=0 ; i<2 ; i++)
	{
		cgMedia.particleTable[PT_BLDDRIP01+i] = tempMat;
		cgMedia.particleTable[PT_BLDDRIP01_GRN+i] = greenMat;

		cgMedia.particleCoords[PT_BLDDRIP01+i][0] = 0.5f;
		cgMedia.particleCoords[PT_BLDDRIP01+i][1] = (i*0.5f);
		cgMedia.particleCoords[PT_BLDDRIP01+i][2] = cgMedia.particleCoords[PT_BLDDRIP01+i][0] + 0.25f;
		cgMedia.particleCoords[PT_BLDDRIP01+i][3] = cgMedia.particleCoords[PT_BLDDRIP01+i][1] + 0.5f;

		Vec4Copy(cgMedia.particleCoords[PT_BLDDRIP01+i], cgMedia.particleCoords[PT_BLDDRIP01_GRN+i]);
	}

	yOffset = 0;
	for (int i=0 ; i<2 ; i++)
	{
		cgMedia.particleTable[PT_BLDSPURT+i] = tempMat;
		cgMedia.particleTable[PT_BLDSPURT_GREEN+i] = greenMat;

		cgMedia.particleCoords[PT_BLDSPURT+i][0] = 0.75f;
		cgMedia.particleCoords[PT_BLDSPURT+i][1] = yOffset;
		cgMedia.particleCoords[PT_BLDSPURT+i][2] = cgMedia.particleCoords[PT_BLDSPURT+i][0] + 0.25f;
		cgMedia.particleCoords[PT_BLDSPURT+i][3] = cgMedia.particleCoords[PT_BLDSPURT+i][1] + 0.25f;

		Vec4Copy(cgMedia.particleCoords[PT_BLDSPURT+i], cgMedia.particleCoords[PT_BLDSPURT_GREEN+i]);

		yOffset += 0.25f;
	}

	CG_IncLoadingVal();

	cgMedia.particleTable[PT_BEAM]			= cgi.R_RegisterPoly ("egl/parts/beam.tga");

	cgMedia.particleTable[PT_EXPLOFLASH]	= cgi.R_RegisterPoly ("egl/parts/exploflash.tga");
	cgMedia.particleTable[PT_EXPLOWAVE]		= cgi.R_RegisterPoly ("egl/parts/explowave.tga");

	cgMedia.particleTable[PT_FLARE]			= cgi.R_RegisterPoly ("egl/parts/flare.tga");
	cgMedia.particleTable[PT_FLAREGLOW]		= cgi.R_RegisterPoly ("egl/parts/flare_glow.tga");

	cgMedia.particleTable[PT_FLY]			= cgi.R_RegisterPoly ("egl/parts/fly.tga");

	cgMedia.particleTable[PT_RAIL_CORE]		= cgi.R_RegisterPoly ("egl/parts/rail_core.tga");
	cgMedia.particleTable[PT_RAIL_WAVE]		= cgi.R_RegisterPoly ("egl/parts/rail_wave.tga");
	cgMedia.particleTable[PT_RAIL_SPIRAL]	= cgi.R_RegisterPoly ("egl/parts/rail_spiral.tga");

	cgMedia.particleTable[PT_SPARK]			= cgi.R_RegisterPoly ("egl/parts/spark.tga");

	cgMedia.particleTable[PT_WATERBUBBLE]		= cgi.R_RegisterPoly ("egl/parts/water_bubble.tga");
	cgMedia.particleTable[PT_WATERDROPLET]		= cgi.R_RegisterPoly ("egl/parts/water_droplet.tga");
	cgMedia.particleTable[PT_WATERIMPACT]		= cgi.R_RegisterPoly ("egl/parts/water_impact.tga");
	cgMedia.particleTable[PT_WATERMIST]			= cgi.R_RegisterPoly ("egl/parts/water_mist.tga");
	cgMedia.particleTable[PT_WATERMIST_GLOW]	= cgi.R_RegisterPoly ("egl/parts/water_mist_glow.tga");
	cgMedia.particleTable[PT_WATERPLUME]		= cgi.R_RegisterPoly ("egl/parts/water_plume.tga");
	cgMedia.particleTable[PT_WATERPLUME_GLOW]	= cgi.R_RegisterPoly ("egl/parts/water_plume_glow.tga");
	cgMedia.particleTable[PT_WATERRING]			= cgi.R_RegisterPoly ("egl/parts/water_ring.tga");
	cgMedia.particleTable[PT_WATERRIPPLE]		= cgi.R_RegisterPoly ("egl/parts/water_ripple.tga");

	// Animated explosions
	CG_IncLoadingVal();
	CG_LoadingFilename ("Explosions");

	cgMedia.particleTable[PT_EXPLO1]		= cgi.R_RegisterPoly ("egl/parts/explo1.tga");
	cgMedia.particleTable[PT_EXPLO2]		= cgi.R_RegisterPoly ("egl/parts/explo2.tga");
	cgMedia.particleTable[PT_EXPLO3]		= cgi.R_RegisterPoly ("egl/parts/explo3.tga");
	cgMedia.particleTable[PT_EXPLO4]		= cgi.R_RegisterPoly ("egl/parts/explo4.tga");
	cgMedia.particleTable[PT_EXPLO5]		= cgi.R_RegisterPoly ("egl/parts/explo5.tga");
	cgMedia.particleTable[PT_EXPLO6]		= cgi.R_RegisterPoly ("egl/parts/explo6.tga");
	cgMedia.particleTable[PT_EXPLO7]		= cgi.R_RegisterPoly ("egl/parts/explo7.tga");

	cgMedia.particleTable[PT_EXPLOEMBERS1]	= cgi.R_RegisterPoly ("egl/parts/exploembers.tga");
	cgMedia.particleTable[PT_EXPLOEMBERS2]	= cgi.R_RegisterPoly ("egl/parts/exploembers2.tga");

	// mapfx media
	CG_LoadingFilename ("MapFX Media");

	cgMedia.particleTable[MFX_CORONA]		= cgi.R_RegisterPoly ("egl/mfx/corona.tga");
	cgMedia.particleTable[MFX_WHITE]		= cgi.R_RegisterPoly ("egl/mfx/white.tga");

	// Decal specific
	CG_IncLoadingVal();
	CG_LoadingFilename ("Decals");

	// Set default subUV coords
	for (int i=0 ; i<DT_PICTOTAL ; i++)
	{
		Vec4Set(cgMedia.decalCoords[i], 0, 0, 1, 1);
	}

	cgMedia.decalTable[DT_BFG_BURNMARK]			= cgi.R_RegisterPoly ("egl/decals/bfg_burnmark.tga");
	cgMedia.decalTable[DT_BFG_GLOWMARK]			= cgi.R_RegisterPoly ("egl/decals/bfg_glowmark.tga");

	cgMedia.decalTable[DT_BLASTER_BLUEMARK]		= cgi.R_RegisterPoly ("egl/decals/blaster_bluemark.tga");
	cgMedia.decalTable[DT_BLASTER_BURNMARK]		= cgi.R_RegisterPoly ("egl/decals/blaster_burnmark.tga");
	cgMedia.decalTable[DT_BLASTER_GREENMARK]	= cgi.R_RegisterPoly ("egl/decals/blaster_greenmark.tga");
	cgMedia.decalTable[DT_BLASTER_REDMARK]		= cgi.R_RegisterPoly ("egl/decals/blaster_redmark.tga");

	cgMedia.decalTable[DT_DRONE_SPIT_GLOW]		= cgi.R_RegisterPoly ("egl/decals/drone_spit_glow.tga");

	cgMedia.decalTable[DT_ENGYREPAIR_BURNMARK]	= cgi.R_RegisterPoly ("egl/decals/engy_repair_burnmark.tga");
	cgMedia.decalTable[DT_ENGYREPAIR_GLOWMARK]	= cgi.R_RegisterPoly ("egl/decals/engy_repair_glowmark.tga");

	tempMat = cgi.R_RegisterPoly("egl/decals/bloodtable.tga");
	greenMat = cgi.R_RegisterPoly("egl/decals/bloodtable_green.tga");
	yOffset = 0;
	for (int i=0 ; i<16 ; i++)
	{
		cgMedia.decalTable[DT_BLOOD01+i] = tempMat;
		cgMedia.decalTable[DT_BLOOD01_GRN+i] = greenMat;

		if (i && !(i&3))
			yOffset += 0.25f;

		cgMedia.decalCoords[DT_BLOOD01+i][0] = (i&3) * 0.25f;
		cgMedia.decalCoords[DT_BLOOD01+i][1] = yOffset;
		cgMedia.decalCoords[DT_BLOOD01+i][2] = cgMedia.decalCoords[DT_BLOOD01+i][0] + 0.25f;
		cgMedia.decalCoords[DT_BLOOD01+i][3] = cgMedia.decalCoords[DT_BLOOD01+i][1] + 0.25f;

		Vec4Copy(cgMedia.decalCoords[DT_BLOOD01+i], cgMedia.decalCoords[DT_BLOOD01_GRN+i]);
	}

	cgMedia.decalTable[DT_BULLET]				= cgi.R_RegisterPoly ("egl/decals/bullet.tga");

	cgMedia.decalTable[DT_EXPLOMARK]			= cgi.R_RegisterPoly ("egl/decals/explomark.tga");
	cgMedia.decalTable[DT_EXPLOMARK2]			= cgi.R_RegisterPoly ("egl/decals/explomark2.tga");
	cgMedia.decalTable[DT_EXPLOMARK3]			= cgi.R_RegisterPoly ("egl/decals/explomark3.tga");

	cgMedia.decalTable[DT_RAIL_BURNMARK]		= cgi.R_RegisterPoly ("egl/decals/rail_burnmark.tga");
	cgMedia.decalTable[DT_RAIL_GLOWMARK]		= cgi.R_RegisterPoly ("egl/decals/rail_glowmark.tga");
	cgMedia.decalTable[DT_RAIL_WHITE]			= cgi.R_RegisterPoly ("egl/decals/rail_white.tga");

	cgMedia.decalTable[DT_SLASH]				= cgi.R_RegisterPoly ("egl/decals/slash.tga");
	cgMedia.decalTable[DT_SLASH2]				= cgi.R_RegisterPoly ("egl/decals/slash2.tga");
	cgMedia.decalTable[DT_SLASH3]				= cgi.R_RegisterPoly ("egl/decals/slash3.tga");

	// clear filename
	CG_LoadingFilename (0);
}


/*
================
CG_SoundMediaInit

Called on CGame init and on snd_restart
================
*/
void CG_SoundMediaInit ()
{
	char	name[MAX_QPATH];
	int		i;

	CG_LoadingString ("Loading sound media...");

	// UI sounds
	uiMedia.sounds.menuIn			= cgi.Snd_RegisterSound ("misc/menu1.wav");
	uiMedia.sounds.menuMove			= cgi.Snd_RegisterSound ("misc/menu2.wav");
	uiMedia.sounds.menuOut			= cgi.Snd_RegisterSound ("misc/menu3.wav");

	// Generic sounds
	cgMedia.sfx.disruptExplo		= cgi.Snd_RegisterSound ("weapons/disrupthit.wav");
	cgMedia.sfx.grenadeExplo		= cgi.Snd_RegisterSound ("weapons/grenlx1a.wav");
	cgMedia.sfx.rocketExplo			= cgi.Snd_RegisterSound ("weapons/rocklx1a.wav");
	cgMedia.sfx.waterExplo			= cgi.Snd_RegisterSound ("weapons/xpld_wat.wav");

	cgMedia.sfx.gib					= cgi.Snd_RegisterSound ("misc/udeath.wav");
	cgMedia.sfx.gibSplat[0]			= cgi.Snd_RegisterSound ("egl/gibimp1.wav");
	cgMedia.sfx.gibSplat[1]			= cgi.Snd_RegisterSound ("egl/gibimp2.wav");
	cgMedia.sfx.gibSplat[2]			= cgi.Snd_RegisterSound ("egl/gibimp3.wav");

	CG_IncLoadingVal();

	cgMedia.sfx.itemRespawn			= cgi.Snd_RegisterSound ("items/respawn1.wav");
	cgMedia.sfx.laserHit			= cgi.Snd_RegisterSound ("weapons/lashit.wav");
	cgMedia.sfx.lightning			= cgi.Snd_RegisterSound ("weapons/tesla.wav");

	cgMedia.sfx.playerFall			= cgi.Snd_RegisterSound ("*fall2.wav");
	cgMedia.sfx.playerFallShort		= cgi.Snd_RegisterSound ("player/land1.wav");
	cgMedia.sfx.playerFallFar		= cgi.Snd_RegisterSound ("*fall1.wav");

	for (i = 0; i < 4; ++i)
	{
		for (int z = 0; z < 2; ++z)
			cgMedia.sfx.playerPain[i][z] = cgi.Snd_RegisterSound((char*)String::Format("*pain%i_%i.wav", (i + 1) * 25, z + 1).CString());
	}

	cgMedia.sfx.playerTeleport		= cgi.Snd_RegisterSound ("misc/tele1.wav");
	cgMedia.sfx.bigTeleport			= cgi.Snd_RegisterSound ("misc/bigtele.wav");

	for (i=0 ; i<7 ; i++) {
		CG_IncLoadingVal();

		Q_snprintfz (name, sizeof(name), "world/spark%i.wav", i+1);
		cgMedia.sfx.spark[i]				= cgi.Snd_RegisterSound (name);

		if (i > 5)
			continue;

		Q_snprintfz (name, sizeof(name), "egl/steps/snow%i.wav", i+1);
		cgMedia.sfx.steps.snow[i]			= cgi.Snd_RegisterSound (name);

		if (i > 3)
			continue;

		Q_snprintfz (name, sizeof(name), "player/step%i.wav", i+1);
		cgMedia.sfx.steps.standard[i]		= cgi.Snd_RegisterSound (name);


		Q_snprintfz (name, sizeof(name), "egl/steps/concrete%i.wav", i+1);
		cgMedia.sfx.steps.concrete[i]		= cgi.Snd_RegisterSound (name);

		Q_snprintfz (name, sizeof(name), "egl/steps/dirt%i.wav", i+1);
		cgMedia.sfx.steps.dirt[i]			= cgi.Snd_RegisterSound (name);

		Q_snprintfz (name, sizeof(name), "egl/steps/duct%i.wav", i+1);
		cgMedia.sfx.steps.duct[i]			= cgi.Snd_RegisterSound (name);

		Q_snprintfz (name, sizeof(name), "egl/steps/grass%i.wav", i+1);
		cgMedia.sfx.steps.grass[i]			= cgi.Snd_RegisterSound (name);

		Q_snprintfz (name, sizeof(name), "egl/steps/gravel%i.wav", i+1);
		cgMedia.sfx.steps.gravel[i]			= cgi.Snd_RegisterSound (name);

		Q_snprintfz (name, sizeof(name), "egl/steps/metal%i.wav", i+1);
		cgMedia.sfx.steps.metal[i]			= cgi.Snd_RegisterSound (name);

		Q_snprintfz (name, sizeof(name), "egl/steps/metalgrate%i.wav", i+1);
		cgMedia.sfx.steps.metalGrate[i]		= cgi.Snd_RegisterSound (name);

		Q_snprintfz (name, sizeof(name), "egl/steps/metalladder%i.wav", i+1);
		cgMedia.sfx.steps.metalLadder[i]	= cgi.Snd_RegisterSound (name);

		Q_snprintfz (name, sizeof(name), "egl/steps/mud%i.wav", i+1);
		cgMedia.sfx.steps.mud[i]			= cgi.Snd_RegisterSound (name);

		Q_snprintfz (name, sizeof(name), "egl/steps/sand%i.wav", i+1);
		cgMedia.sfx.steps.sand[i]			= cgi.Snd_RegisterSound (name);

		Q_snprintfz (name, sizeof(name), "egl/steps/slosh%i.wav", i+1);
		cgMedia.sfx.steps.slosh[i]			= cgi.Snd_RegisterSound (name);

		Q_snprintfz (name, sizeof(name), "egl/steps/tile%i.wav", i+1);
		cgMedia.sfx.steps.tile[i]			= cgi.Snd_RegisterSound (name);

		Q_snprintfz (name, sizeof(name), "egl/steps/wade%i.wav", i+1);
		cgMedia.sfx.steps.wade[i]			= cgi.Snd_RegisterSound (name);

		Q_snprintfz (name, sizeof(name), "egl/steps/wood%i.wav", i+1);
		cgMedia.sfx.steps.wood[i]			= cgi.Snd_RegisterSound (name);

		Q_snprintfz (name, sizeof(name), "egl/steps/woodpanel%i.wav", i+1);
		cgMedia.sfx.steps.woodPanel[i]		= cgi.Snd_RegisterSound (name);

		if (i > 2)
			continue;

		Q_snprintfz (name, sizeof(name), "world/ric%i.wav", i+1);
		cgMedia.sfx.ricochet[i]				= cgi.Snd_RegisterSound (name);
	}

	CG_IncLoadingVal();
	CG_LoadingFilename ("Muzzle flashes");

	// Muzzleflash sounds
	cgMedia.sfx.mz.bfgFireSfx			= cgi.Snd_RegisterSound ("weapons/bfg__f1y.wav");
	cgMedia.sfx.mz.blasterFireSfx		= cgi.Snd_RegisterSound ("weapons/blastf1a.wav");
	cgMedia.sfx.mz.etfRifleFireSfx		= cgi.Snd_RegisterSound ("weapons/nail1.wav");
	cgMedia.sfx.mz.grenadeFireSfx		= cgi.Snd_RegisterSound ("weapons/grenlf1a.wav");
	cgMedia.sfx.mz.grenadeReloadSfx		= cgi.Snd_RegisterSound ("weapons/grenlr1b.wav");
	cgMedia.sfx.mz.hyperBlasterFireSfx	= cgi.Snd_RegisterSound ("weapons/hyprbf1a.wav");
	cgMedia.sfx.mz.ionRipperFireSfx		= cgi.Snd_RegisterSound ("weapons/rippfire.wav");

	for (i=0 ; i<5 ; i++) {
		Q_snprintfz (name, sizeof(name), "weapons/machgf%ib.wav", i+1);
		cgMedia.sfx.mz.machineGunSfx[i]	= cgi.Snd_RegisterSound (name);
	}

	cgMedia.sfx.mz.phalanxFireSfx		= cgi.Snd_RegisterSound ("weapons/plasshot.wav");
	cgMedia.sfx.mz.railgunFireSfx		= cgi.Snd_RegisterSound ("weapons/railgf1a.wav");
	cgMedia.sfx.mz.railgunReloadSfx		= cgi.Snd_RegisterSound ("weapons/railgr1a.wav");
	cgMedia.sfx.mz.rocketFireSfx		= cgi.Snd_RegisterSound ("weapons/rocklf1a.wav");
	cgMedia.sfx.mz.rocketReloadSfx		= cgi.Snd_RegisterSound ("weapons/rocklr1b.wav");
	cgMedia.sfx.mz.shotgunFireSfx		= cgi.Snd_RegisterSound ("weapons/shotgf1b.wav");
	cgMedia.sfx.mz.shotgun2FireSfx		= cgi.Snd_RegisterSound ("weapons/shotg2.wav");
	cgMedia.sfx.mz.shotgunReloadSfx		= cgi.Snd_RegisterSound ("weapons/shotgr1b.wav");
	cgMedia.sfx.mz.superShotgunFireSfx	= cgi.Snd_RegisterSound ("weapons/sshotf1b.wav");
	cgMedia.sfx.mz.trackerFireSfx		= cgi.Snd_RegisterSound ("weapons/disint2.wav");

	CG_IncLoadingVal();

	// Monster muzzleflash sounds
	cgMedia.sfx.mz2.chicRocketSfx		= cgi.Snd_RegisterSound ("chick/chkatck2.wav");
	cgMedia.sfx.mz2.floatBlasterSfx		= cgi.Snd_RegisterSound ("floater/fltatck1.wav");
	cgMedia.sfx.mz2.flyerBlasterSfx		= cgi.Snd_RegisterSound ("flyer/flyatck3.wav");
	cgMedia.sfx.mz2.gunnerGrenadeSfx	= cgi.Snd_RegisterSound ("gunner/gunatck3.wav");
	cgMedia.sfx.mz2.gunnerMachGunSfx	= cgi.Snd_RegisterSound ("gunner/gunatck2.wav");
	cgMedia.sfx.mz2.hoverBlasterSfx		= cgi.Snd_RegisterSound ("hover/hovatck1.wav");
	cgMedia.sfx.mz2.jorgMachGunSfx		= cgi.Snd_RegisterSound ("boss3/xfire.wav");
	cgMedia.sfx.mz2.machGunSfx			= cgi.Snd_RegisterSound ("infantry/infatck1.wav");
	cgMedia.sfx.mz2.makronBlasterSfx	= cgi.Snd_RegisterSound ("makron/blaster.wav");
	cgMedia.sfx.mz2.medicBlasterSfx		= cgi.Snd_RegisterSound ("medic/medatck1.wav");
	cgMedia.sfx.mz2.soldierBlasterSfx	= cgi.Snd_RegisterSound ("soldier/solatck2.wav");
	cgMedia.sfx.mz2.soldierMachGunSfx	= cgi.Snd_RegisterSound ("soldier/solatck3.wav");
	cgMedia.sfx.mz2.soldierShotgunSfx	= cgi.Snd_RegisterSound ("soldier/solatck1.wav");
	cgMedia.sfx.mz2.superTankRocketSfx	= cgi.Snd_RegisterSound ("tank/rocket.wav");
	cgMedia.sfx.mz2.tankBlasterSfx		= cgi.Snd_RegisterSound ("tank/tnkatck3.wav");

	for (i=0 ; i<5 ; i++) {
		Q_snprintfz (name, sizeof(name), "tank/tnkatk2%c.wav", 'a' + i);
		cgMedia.sfx.mz2.tankMachGunSfx[i] = cgi.Snd_RegisterSound (name);
	}

	cgMedia.sfx.mz2.tankRocketSfx		= cgi.Snd_RegisterSound ("tank/tnkatck1.wav");

	// Brass sounds
	cgMedia.sfx.mgShell[0]				= cgi.Snd_RegisterSound ("#egl/sounds/brass/mg_shell1.wav");
	cgMedia.sfx.mgShell[1]				= cgi.Snd_RegisterSound ("#egl/sounds/brass/mg_shell2.wav");
	cgMedia.sfx.sgShell[0]				= cgi.Snd_RegisterSound ("#egl/sounds/brass/sg_shell1.wav");
	cgMedia.sfx.sgShell[1]				= cgi.Snd_RegisterSound ("#egl/sounds/brass/sg_shell2.wav");

	// Configstring-based sounds
	for (i=1 ; i<MAX_CS_SOUNDS ; i++) {
		if (!cg.configStrings[CS_SOUNDS+i][0]) {
			cg.soundCfgStrings[i] = NULL;
			break;
		}

		cg.soundCfgStrings[i] = cgi.Snd_RegisterSound (cg.configStrings[CS_SOUNDS+i]);
		cgi.Sys_SendKeyEvents ();	// pump message loop
	}

	if (cgi.Com_ClientState() >= CA_CONNECTING)
		CG_ItemMediaInit(ITEMBIT_MASK);

	// Clear filename
	CG_LoadingFilename (0);
}

/*
=============================================================================

	MEDIA INITIALIZATION

=============================================================================
*/

/*
================
CG_MapInit

Called before all the cgame is initialized
================
*/
void CG_MapInit ()
{
	float		percent;

	if (cgMedia.initialized)
		return;

	CG_LoadingValues (227);
	CG_LoadingString (0);
	CG_LoadingFilename (0);

	cgi.R_UpdateScreen ();

	percent = 0;

	// Map media
	CG_MapMediaInit ();

	// Model media
	CG_ModelMediaInit ();

	// Pic media
	CG_PicMediaInit ();

	// Effect media
	CG_FXMediaInit ();

	// Sound media
	CG_SoundMediaInit ();

	CG_LoadingValues (0);
	CG_LoadingString (0);
	CG_LoadingFilename (0);

	cg.frame.valid = false;	// Probably out of date

	cgMedia.initialized = true;
}


/*
================
CG_ShutdownMap
================
*/
void CG_ShutdownMap ()
{
	if (!cgMedia.initialized)
		return;

	cgMedia.initialized = false;
}
