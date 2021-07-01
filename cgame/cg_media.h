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
// cg_media.h
//

/*
=============================================================================

	CGAME MEDIA

=============================================================================
*/

// surface-specific step sounds
struct cgStepMedia_t {
	struct sfx_t	*standard[4];

	struct sfx_t	*concrete[4];
	struct sfx_t	*dirt[4];
	struct sfx_t	*duct[4];
	struct sfx_t	*grass[4];
	struct sfx_t	*gravel[4];
	struct sfx_t	*metal[4];
	struct sfx_t	*metalGrate[4];
	struct sfx_t	*metalLadder[4];
	struct sfx_t	*mud[4];
	struct sfx_t	*sand[4];
	struct sfx_t	*slosh[4];
	struct sfx_t	*snow[6];
	struct sfx_t	*tile[4];
	struct sfx_t	*wade[4];
	struct sfx_t	*wood[4];
	struct sfx_t	*woodPanel[4];
};

// muzzle flash sounds
struct cgMzMedia_t {
	struct sfx_t	*bfgFireSfx;
	struct sfx_t	*blasterFireSfx;
	struct sfx_t	*etfRifleFireSfx;
	struct sfx_t	*grenadeFireSfx;
	struct sfx_t	*grenadeReloadSfx;
	struct sfx_t	*hyperBlasterFireSfx;
	struct sfx_t	*ionRipperFireSfx;
	struct sfx_t	*machineGunSfx[5];
	struct sfx_t	*phalanxFireSfx;
	struct sfx_t	*railgunFireSfx;
	struct sfx_t	*railgunReloadSfx;
	struct sfx_t	*rocketFireSfx;
	struct sfx_t	*rocketReloadSfx;
	struct sfx_t	*shotgunFireSfx;
	struct sfx_t	*shotgun2FireSfx;
	struct sfx_t	*shotgunReloadSfx;
	struct sfx_t	*superShotgunFireSfx;
	struct sfx_t	*trackerFireSfx;
};

// monster muzzle flash sounds
struct cgMz2Media_t {
	struct sfx_t	*chicRocketSfx;
	struct sfx_t	*floatBlasterSfx;
	struct sfx_t	*flyerBlasterSfx;
	struct sfx_t	*gunnerGrenadeSfx;
	struct sfx_t	*gunnerMachGunSfx;
	struct sfx_t	*hoverBlasterSfx;
	struct sfx_t	*jorgMachGunSfx;
	struct sfx_t	*machGunSfx;
	struct sfx_t	*makronBlasterSfx;
	struct sfx_t	*medicBlasterSfx;
	struct sfx_t	*soldierBlasterSfx;
	struct sfx_t	*soldierMachGunSfx;
	struct sfx_t	*soldierShotgunSfx;
	struct sfx_t	*superTankRocketSfx;
	struct sfx_t	*tankBlasterSfx;
	struct sfx_t	*tankMachGunSfx[5];
	struct sfx_t	*tankRocketSfx;
};

// all sounds
struct cgMediaSounds_t {
	cgStepMedia_t		steps;
	cgMzMedia_t			mz;
	cgMz2Media_t		mz2;

	struct sfx_t		*ricochet[3];
	struct sfx_t		*spark[7];

	struct sfx_t		*disruptExplo;
	struct sfx_t		*grenadeExplo;
	struct sfx_t		*rocketExplo;
	struct sfx_t		*waterExplo;

	struct sfx_t		*gib;
	struct sfx_t		*gibSplat[3];

	struct sfx_t		*itemRespawn;
	struct sfx_t		*laserHit;
	struct sfx_t		*lightning;

	struct sfx_t		*playerFall;
	struct sfx_t		*playerFallShort;
	struct sfx_t		*playerFallFar;
	struct sfx_t		*playerPain[4][2];

	struct sfx_t		*playerTeleport;
	struct sfx_t		*bigTeleport;

	struct sfx_t		*mgShell[2];
	struct sfx_t		*sgShell[2];
};

// ==========================================================================

struct cgMedia_t {
	bool					initialized;
	bool					loadScreenPrepped;

	// fonts
	struct refFont_t		*defaultFont;

	// engine generated textures
	struct refMaterial_t	*noTexture;
	struct refMaterial_t	*whiteTexture;
	struct refMaterial_t	*blackTexture;

	// load screen images
	struct refMaterial_t	*loadSplash;
	struct refMaterial_t	*loadBarPos;
	struct refMaterial_t	*loadBarNeg;
	struct refMaterial_t	*loadNoMapShot;
	struct refMaterial_t	*loadMapShot;

	// screen materials
	struct refMaterial_t	*alienInfraredVision;
	struct refMaterial_t	*infraredGoggles;

	// sounds
	cgMediaSounds_t			sfx;

	// models
	struct refModel_t		*parasiteSegmentModel;
	struct refModel_t		*grappleCableModel;
	struct refModel_t		*powerScreenModel;

	struct refModel_t		*brassMGModel;
	struct refModel_t		*brassSGModel;

	struct refModel_t		*lightningModel;
	struct refModel_t		*heatBeamModel;
	struct refModel_t		*monsterHeatBeamModel;

	struct refModel_t		*maleDisguiseModel;
	struct refModel_t		*femaleDisguiseModel;
	struct refModel_t		*cyborgDisguiseModel;

	// skins
	struct refMaterial_t	*maleDisguiseSkin;
	struct refMaterial_t	*femaleDisguiseSkin;
	struct refMaterial_t	*cyborgDisguiseSkin;

	struct refMaterial_t	*modelShellGod;
	struct refMaterial_t	*modelShellHalfDam;
	struct refMaterial_t	*modelShellDouble;
	struct refMaterial_t	*modelShellRed;
	struct refMaterial_t	*modelShellGreen;
	struct refMaterial_t	*modelShellBlue;

	// images
	struct refMaterial_t	*crosshairMat;

	struct refMaterial_t	*tileBackMat;

	struct refMaterial_t	*hudFieldMat;
	struct refMaterial_t	*hudInventoryMat;
	struct refMaterial_t	*hudNetMat;
	struct refMaterial_t	*hudPausedMat;

	// particle/decal media
	struct refMaterial_t	*decalTable[DT_PICTOTAL];
	vec4_t					decalCoords[DT_PICTOTAL];

	struct refMaterial_t	*particleTable[PT_PICTOTAL];
	vec4_t					particleCoords[PT_PICTOTAL];

	// registry
	refModel_t				**worldModelRegistry;
	refModel_t				**viewModelRegistry;
	sfx_t					**pickupSoundRegistry;
	refMaterial_t			**iconRegistry;
};

extern cgMedia_t	cgMedia;

/*
=============================================================================

	UI MEDIA

=============================================================================
*/

// sounds
struct uiSoundMedia_t {
	struct sfx_t		*menuIn;
	struct sfx_t		*menuMove;
	struct sfx_t		*menuOut;
};

// menu banners
struct uiBannerMedia_t {
	struct refMaterial_t	*addressBook;
	struct refMaterial_t	*multiplayer;
	struct refMaterial_t	*startServer;
	struct refMaterial_t	*joinServer;
	struct refMaterial_t	*options;
	struct refMaterial_t	*game;
	struct refMaterial_t	*loadGame;
	struct refMaterial_t	*saveGame;
	struct refMaterial_t	*video;
	struct refMaterial_t	*quit;
};

// menu media
#define MAINMENU_CURSOR_NUMFRAMES	15
struct uiMenuMedia_t {
	struct refMaterial_t	*mainCursors[MAINMENU_CURSOR_NUMFRAMES];
	struct refMaterial_t	*mainPlaque;
	struct refMaterial_t	*mainLogo;

	struct refMaterial_t	*mainGame;
	struct refMaterial_t	*mainMultiplayer;
	struct refMaterial_t	*mainOptions;
	struct refMaterial_t	*mainVideo;
	struct refMaterial_t	*mainQuit;

	struct refMaterial_t	*mainGameSel;
	struct refMaterial_t	*mainMultiplayerSel;
	struct refMaterial_t	*mainOptionsSel;
	struct refMaterial_t	*mainVideoSel;
	struct refMaterial_t	*mainQuitSel;
};

// ==========================================================================

struct uiMedia_t {
	// sounds
	uiSoundMedia_t			sounds;

	// background images
	struct refMaterial_t	*bgBig;

	// cursor images
	struct refMaterial_t	*cursorMat;
	struct refMaterial_t	*cursorHoverMat;

	// menu items
	uiBannerMedia_t			banners;
	uiMenuMedia_t			menus;
};

extern uiMedia_t	uiMedia;

// ==========================================================================

//
// cg_media.c
//

void	CG_InitBaseMedia ();
void	CG_MapInit ();
void	CG_ShutdownMap ();

void	CG_SoundMediaInit ();
void	CG_CrosshairMaterialInit ();
