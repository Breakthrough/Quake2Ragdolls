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
// m_opts_effects.c
//

#include "m_local.h"

/*
=======================================================================

	EFFECTS MENU

=======================================================================
*/

struct m_effectsMenu_t {
	// Menu items
	uiFrameWork_t	frameWork;

	uiImage_t		banner;

	//
	// particles
	//

	uiAction_t		part_header;

	uiList_t		part_gore_list;
	uiSlider_t		part_smokelinger_slider;
	uiAction_t		part_smokelinger_amount;

	uiList_t		part_railtrail_list;

	uiSlider_t		part_railred_slider;
	uiAction_t		part_railred_amount;
	uiSlider_t		part_railgreen_slider;
	uiAction_t		part_railgreen_amount;
	uiSlider_t		part_railblue_slider;
	uiAction_t		part_railblue_amount;

	uiSlider_t		part_spiralred_slider;
	uiAction_t		part_spiralred_amount;
	uiSlider_t		part_spiralgreen_slider;
	uiAction_t		part_spiralgreen_amount;
	uiSlider_t		part_spiralblue_slider;
	uiAction_t		part_spiralblue_amount;

	uiList_t		part_cull_toggle;
	uiList_t		part_lighting_toggle;

	uiList_t		explorattle_toggle;
	uiSlider_t		explorattle_scale_slider;
	uiAction_t		explorattle_scale_amount;

	uiList_t		mapeffects_toggle;

	//
	// decals
	//

	uiAction_t		dec_header;

	uiList_t		dec_toggle;
	uiList_t		dec_lod_toggle;
	uiSlider_t		dec_life_slider;
	uiAction_t		dec_life_amount;
	uiSlider_t		dec_burnlife_slider;
	uiAction_t		dec_burnlife_amount;
	uiSlider_t		dec_max_slider;
	uiAction_t		dec_max_amount;

	//
	// materials
	//

	uiAction_t		mat_header;

	uiList_t		mat_caustics_toggle;
	uiList_t		mat_detail_toggle;

	//
	// misc
	//

	uiAction_t		back_action;
};

static m_effectsMenu_t	m_effectsMenu;

//
// particles
//

static void GoreTypeFunc (void *unused)
{
	cgi.Cvar_SetValue ("cg_particleGore", m_effectsMenu.part_gore_list.curValue, false);
}

static void SmokeLingerFunc (void *unused)
{
	cgi.Cvar_SetValue ("cg_particleSmokeLinger", m_effectsMenu.part_smokelinger_slider.curValue, false);
	m_effectsMenu.part_smokelinger_amount.generic.name = cgi.Cvar_GetStringValue ("cg_particleSmokeLinger");
}

static void RailTrailFunc (void *unused)
{
	cgi.Cvar_SetValue ("cg_railSpiral", m_effectsMenu.part_railtrail_list.curValue, false);
}

static void RailRedColorFunc (void *unused)
{
	cgi.Cvar_SetValue ("cg_railCoreRed", m_effectsMenu.part_railred_slider.curValue * 0.1, false);
	m_effectsMenu.part_railred_amount.generic.name = cgi.Cvar_GetStringValue ("cg_railCoreRed");
}

static void RailGreenColorFunc (void *unused)
{
	cgi.Cvar_SetValue ("cg_railCoreGreen", m_effectsMenu.part_railgreen_slider.curValue * 0.1, false);
	m_effectsMenu.part_railgreen_amount.generic.name = cgi.Cvar_GetStringValue ("cg_railCoreGreen");
}

static void RailBlueColorFunc (void *unused)
{
	cgi.Cvar_SetValue ("cg_railCoreBlue", m_effectsMenu.part_railblue_slider.curValue * 0.1, false);
	m_effectsMenu.part_railblue_amount.generic.name = cgi.Cvar_GetStringValue ("cg_railCoreBlue");
}

static void SpiralRedColorFunc (void *unused)
{
	cgi.Cvar_SetValue ("cg_railSpiralRed", m_effectsMenu.part_spiralred_slider.curValue * 0.1, false);
	m_effectsMenu.part_spiralred_amount.generic.name = cgi.Cvar_GetStringValue ("cg_railSpiralRed");
}

static void SpiralGreenColorFunc (void *unused)
{
	cgi.Cvar_SetValue ("cg_railSpiralGreen", m_effectsMenu.part_spiralgreen_slider.curValue * 0.1, false);
	m_effectsMenu.part_spiralgreen_amount.generic.name = cgi.Cvar_GetStringValue ("cg_railSpiralGreen");
}

static void SpiralBlueColorFunc (void *unused)
{
	cgi.Cvar_SetValue ("cg_railSpiralBlue", m_effectsMenu.part_spiralblue_slider.curValue * 0.1, false);
	m_effectsMenu.part_spiralblue_amount.generic.name = cgi.Cvar_GetStringValue ("cg_railSpiralBlue");
}

static void PartCullFunc (void *unused)
{
	cgi.Cvar_SetValue ("cg_particleCulling", m_effectsMenu.part_cull_toggle.curValue, false);
}

static void PartLightFunc (void *unused)
{
	cgi.Cvar_SetValue ("cg_particleShading", m_effectsMenu.part_lighting_toggle.curValue, false);
}

static void ExploRattleFunc (void *unused)
{
	cgi.Cvar_SetValue ("cg_explorattle", m_effectsMenu.explorattle_toggle.curValue, false);
}

static void ExploRattleScaleFunc (void *unused)
{
	cgi.Cvar_SetValue ("cg_explorattle_scale", m_effectsMenu.explorattle_scale_slider.curValue * 0.1, false);
	m_effectsMenu.explorattle_scale_amount.generic.name = cgi.Cvar_GetStringValue ("cg_explorattle_scale");
}

static void MapEffectsToggleFunc (void *unused)
{
	cgi.Cvar_SetValue ("cg_mapeffects", m_effectsMenu.mapeffects_toggle.curValue, false);
}

//
// decals
//

static void DecalToggleFunc (void *unused)
{
	cgi.Cvar_SetValue ("cg_decals", m_effectsMenu.dec_toggle.curValue, false);
}

static void DecalLODFunc (void *unused)
{
	cgi.Cvar_SetValue ("cg_decalLOD", m_effectsMenu.dec_lod_toggle.curValue, false);
}

static void DecalLifeFunc (void *unused)
{
	cgi.Cvar_SetValue ("cg_decalLife", m_effectsMenu.dec_life_slider.curValue * 100, false);
	m_effectsMenu.dec_life_amount.generic.name = cgi.Cvar_GetStringValue ("cg_decalLife");
}
static void DecalBurnLifeFunc (void *unused)
{
	cgi.Cvar_SetValue ("cg_decalBurnLife", m_effectsMenu.dec_burnlife_slider.curValue * 10, false);
	m_effectsMenu.dec_burnlife_amount.generic.name = cgi.Cvar_GetStringValue ("cg_decalBurnLife");
}

static void DecalMaxFunc (void *unused)
{
	cgi.Cvar_SetValue ("cg_decalMax", m_effectsMenu.dec_max_slider.curValue * 1000, false);
	m_effectsMenu.dec_max_amount.generic.name = cgi.Cvar_GetStringValue ("cg_decalMax");
}

//
// mats
//

static void CausticsFunc (void *unused)
{
	cgi.Cvar_SetValue ("r_caustics", m_effectsMenu.mat_caustics_toggle.curValue, false);
}
static void MaterialDetailFunc (void *unused)
{
	cgi.Cvar_SetValue ("r_detailTextures", m_effectsMenu.mat_detail_toggle.curValue, false);
}

/*
=============
EffectsMenu_SetValues
=============
*/
static void EffectsMenu_SetValues ()
{
	//
	// particles
	//

	cgi.Cvar_SetValue ("cg_particleGore",	clamp (cgi.Cvar_GetIntegerValue ("cg_particleGore"), 0, 10), false);
	m_effectsMenu.part_gore_list.curValue	= cgi.Cvar_GetIntegerValue ("cg_particleGore");

	m_effectsMenu.part_smokelinger_slider.curValue	= cgi.Cvar_GetFloatValue ("cg_particleSmokeLinger");
	m_effectsMenu.part_smokelinger_amount.generic.name	= cgi.Cvar_GetStringValue ("cg_particleSmokeLinger");

	cgi.Cvar_SetValue ("cg_railSpiral",				clamp (cgi.Cvar_GetIntegerValue ("cg_railSpiral"), 0, 1), false);
	m_effectsMenu.part_railtrail_list.curValue		= cgi.Cvar_GetIntegerValue ("cg_railSpiral");

	m_effectsMenu.part_railred_slider.curValue			= cgi.Cvar_GetFloatValue ("cg_railCoreRed") * 10;
	m_effectsMenu.part_railred_amount.generic.name		= cgi.Cvar_GetStringValue ("cg_railCoreRed");
	m_effectsMenu.part_railgreen_slider.curValue		= cgi.Cvar_GetFloatValue ("cg_railCoreGreen") * 10;
	m_effectsMenu.part_railgreen_amount.generic.name	= cgi.Cvar_GetStringValue ("cg_railCoreGreen");
	m_effectsMenu.part_railblue_slider.curValue		= cgi.Cvar_GetFloatValue ("cg_railCoreBlue") * 10;
	m_effectsMenu.part_railblue_amount.generic.name	= cgi.Cvar_GetStringValue ("cg_railCoreBlue");

	m_effectsMenu.part_spiralred_slider.curValue		= cgi.Cvar_GetFloatValue ("cg_railSpiralRed") * 10;
	m_effectsMenu.part_spiralred_amount.generic.name	= cgi.Cvar_GetStringValue ("cg_railSpiralRed");
	m_effectsMenu.part_spiralgreen_slider.curValue		= cgi.Cvar_GetFloatValue ("cg_railSpiralGreen") * 10;
	m_effectsMenu.part_spiralgreen_amount.generic.name	= cgi.Cvar_GetStringValue ("cg_railSpiralGreen");
	m_effectsMenu.part_spiralblue_slider.curValue		= cgi.Cvar_GetFloatValue ("cg_railSpiralBlue") * 10;
	m_effectsMenu.part_spiralblue_amount.generic.name	= cgi.Cvar_GetStringValue ("cg_railSpiralBlue");

	cgi.Cvar_SetValue ("cg_particleCulling",	clamp (cgi.Cvar_GetIntegerValue ("cg_particleCulling"), 0, 1), false);
	m_effectsMenu.part_cull_toggle.curValue		= cgi.Cvar_GetIntegerValue ("cg_particleCulling");

	cgi.Cvar_SetValue ("cg_particleShading",	clamp (cgi.Cvar_GetIntegerValue ("cg_particleShading"), 0, 1), false);
	m_effectsMenu.part_lighting_toggle.curValue	= cgi.Cvar_GetIntegerValue ("cg_particleShading");

	cgi.Cvar_SetValue ("cg_explorattle",				clamp (cgi.Cvar_GetIntegerValue ("cg_explorattle"), 0, 1), false);
	m_effectsMenu.explorattle_toggle.curValue			= cgi.Cvar_GetIntegerValue ("cg_explorattle");
	m_effectsMenu.explorattle_scale_slider.curValue		= cgi.Cvar_GetIntegerValue ("cg_explorattle_scale") * 10;
	m_effectsMenu.explorattle_scale_amount.generic.name	= cgi.Cvar_GetStringValue ("cg_explorattle_scale");

	cgi.Cvar_SetValue ("cg_mapeffects",			clamp (cgi.Cvar_GetIntegerValue ("cg_mapeffects"), 0, 1), false);
	m_effectsMenu.mapeffects_toggle.curValue	= cgi.Cvar_GetIntegerValue ("cg_mapeffects");
	

	//
	// decals
	//

	cgi.Cvar_SetValue ("cg_decalLOD",		clamp (cgi.Cvar_GetIntegerValue ("cg_decalLOD"), 0, 1), false);
	m_effectsMenu.dec_lod_toggle.curValue	= cgi.Cvar_GetIntegerValue ("cg_decalLOD");

	cgi.Cvar_SetValue ("cg_decals",			clamp (cgi.Cvar_GetIntegerValue ("cg_decals"), 0, 1), false);
	m_effectsMenu.dec_toggle.curValue		= cgi.Cvar_GetIntegerValue ("cg_decals");

	m_effectsMenu.dec_life_slider.curValue		= cgi.Cvar_GetFloatValue ("cg_decalLife") * 0.01;
	m_effectsMenu.dec_life_amount.generic.name	= cgi.Cvar_GetStringValue ("cg_decalLife");

	m_effectsMenu.dec_burnlife_slider.curValue		= cgi.Cvar_GetFloatValue ("cg_decalBurnLife") * 0.1;
	m_effectsMenu.dec_burnlife_amount.generic.name	= cgi.Cvar_GetStringValue ("cg_decalBurnLife");

	m_effectsMenu.dec_max_slider.curValue		= cgi.Cvar_GetFloatValue ("cg_decalMax") * 0.001;
	m_effectsMenu.dec_max_amount.generic.name	= cgi.Cvar_GetStringValue ("cg_decalMax");

	//
	// materials
	//

	cgi.Cvar_SetValue ("r_caustics",				clamp (cgi.Cvar_GetIntegerValue ("r_caustics"), 0, 1), false);
	m_effectsMenu.mat_caustics_toggle.curValue	= cgi.Cvar_GetIntegerValue ("r_caustics");

	cgi.Cvar_SetValue ("r_detailTextures",			clamp (cgi.Cvar_GetIntegerValue ("r_detailTextures"), 0, 1), false);
	m_effectsMenu.mat_detail_toggle.curValue	= cgi.Cvar_GetIntegerValue ("r_detailTextures");
}


/*
=============
EffectsMenu_Init
=============
*/
static void EffectsMenu_Init ()
{
	static char *goreamount_names[] = {
		"low",
		"normal",
		"x 2",
		"x 3",
		"x 4",
		"x 5",
		"x 6",
		"x 7",
		"x 8",
		"x 9",
		"x 10 !",
		0
	};

	static char *nicenorm_names[] = {
		"normal",
		"nicer",
		0
	};

	static char *onoff_names[] = {
		"off",
		"on",
		0
	};

	static char *railtrail_names[] = {
		"beam",
		"beam and spiral",
		0
	};

	UI_StartFramework (&m_effectsMenu.frameWork, FWF_CENTERHEIGHT);

	m_effectsMenu.banner.generic.type	= UITYPE_IMAGE;
	m_effectsMenu.banner.generic.flags	= UIF_NOSELECT|UIF_CENTERED;
	m_effectsMenu.banner.generic.name	= NULL;
	m_effectsMenu.banner.mat			= uiMedia.banners.options;

	//
	// particles
	//

	m_effectsMenu.part_header.generic.type		= UITYPE_ACTION;
	m_effectsMenu.part_header.generic.flags		= UIF_NOSELECT|UIF_CENTERED|UIF_MEDIUM|UIF_SHADOW;
	m_effectsMenu.part_header.generic.name		= "Particle Settings";

	m_effectsMenu.part_gore_list.generic.type		= UITYPE_SPINCONTROL;
	m_effectsMenu.part_gore_list.generic.name		= "Gore severity";
	m_effectsMenu.part_gore_list.generic.callBack	= GoreTypeFunc;
	m_effectsMenu.part_gore_list.itemNames			= goreamount_names;
	m_effectsMenu.part_gore_list.generic.statusBar	= "Select your gore";

	m_effectsMenu.part_smokelinger_slider.generic.type			= UITYPE_SLIDER;
	m_effectsMenu.part_smokelinger_slider.generic.name			= "Smoke lingering";
	m_effectsMenu.part_smokelinger_slider.generic.callBack		= SmokeLingerFunc;
	m_effectsMenu.part_smokelinger_slider.minValue				= 0;
	m_effectsMenu.part_smokelinger_slider.maxValue				= 10;
	m_effectsMenu.part_smokelinger_slider.generic.statusBar		= "Customize smoke linger duration";
	m_effectsMenu.part_smokelinger_amount.generic.type			= UITYPE_ACTION;
	m_effectsMenu.part_smokelinger_amount.generic.flags			= UIF_LEFT_JUSTIFY|UIF_NOSELECT;

	m_effectsMenu.part_railtrail_list.generic.type			= UITYPE_SPINCONTROL;
	m_effectsMenu.part_railtrail_list.generic.name			= "Rail-Trail style";
	m_effectsMenu.part_railtrail_list.generic.callBack		= RailTrailFunc;
	m_effectsMenu.part_railtrail_list.itemNames				= railtrail_names;
	m_effectsMenu.part_railtrail_list.generic.statusBar		= "Select your style";

	m_effectsMenu.part_railred_slider.generic.type			= UITYPE_SLIDER;
	m_effectsMenu.part_railred_slider.generic.name			= "Rail center red";
	m_effectsMenu.part_railred_slider.generic.callBack		= RailRedColorFunc;
	m_effectsMenu.part_railred_slider.minValue				= 0;
	m_effectsMenu.part_railred_slider.maxValue				= 10;
	m_effectsMenu.part_railred_slider.generic.statusBar		= "Rail core red color";
	m_effectsMenu.part_railred_amount.generic.type			= UITYPE_ACTION;
	m_effectsMenu.part_railred_amount.generic.flags			= UIF_LEFT_JUSTIFY|UIF_NOSELECT;

	m_effectsMenu.part_railgreen_slider.generic.type		= UITYPE_SLIDER;
	m_effectsMenu.part_railgreen_slider.generic.name		= "Rail center green";
	m_effectsMenu.part_railgreen_slider.generic.callBack	= RailGreenColorFunc;
	m_effectsMenu.part_railgreen_slider.minValue			= 0;
	m_effectsMenu.part_railgreen_slider.maxValue			= 10;
	m_effectsMenu.part_railgreen_slider.generic.statusBar	= "Rail core green color";
	m_effectsMenu.part_railgreen_amount.generic.type		= UITYPE_ACTION;
	m_effectsMenu.part_railgreen_amount.generic.flags		= UIF_LEFT_JUSTIFY|UIF_NOSELECT;

	m_effectsMenu.part_railblue_slider.generic.type			= UITYPE_SLIDER;
	m_effectsMenu.part_railblue_slider.generic.name			= "Rail center blue";
	m_effectsMenu.part_railblue_slider.generic.callBack		= RailBlueColorFunc;
	m_effectsMenu.part_railblue_slider.minValue				= 0;
	m_effectsMenu.part_railblue_slider.maxValue				= 10;
	m_effectsMenu.part_railblue_slider.generic.statusBar	= "Rail core blue color";
	m_effectsMenu.part_railblue_amount.generic.type			= UITYPE_ACTION;
	m_effectsMenu.part_railblue_amount.generic.flags		= UIF_LEFT_JUSTIFY|UIF_NOSELECT;

	m_effectsMenu.part_spiralred_slider.generic.type		= UITYPE_SLIDER;
	m_effectsMenu.part_spiralred_slider.generic.name		= "Rail spiral red";
	m_effectsMenu.part_spiralred_slider.generic.callBack	= SpiralRedColorFunc;
	m_effectsMenu.part_spiralred_slider.minValue			= 0;
	m_effectsMenu.part_spiralred_slider.maxValue			= 10;
	m_effectsMenu.part_spiralred_slider.generic.statusBar	= "Rail spiral red color";
	m_effectsMenu.part_spiralred_amount.generic.type		= UITYPE_ACTION;
	m_effectsMenu.part_spiralred_amount.generic.flags		= UIF_LEFT_JUSTIFY|UIF_NOSELECT;

	m_effectsMenu.part_spiralgreen_slider.generic.type			= UITYPE_SLIDER;
	m_effectsMenu.part_spiralgreen_slider.generic.name			= "Rail spiral green";
	m_effectsMenu.part_spiralgreen_slider.generic.callBack		= SpiralGreenColorFunc;
	m_effectsMenu.part_spiralgreen_slider.minValue				= 0;
	m_effectsMenu.part_spiralgreen_slider.maxValue				= 10;
	m_effectsMenu.part_spiralgreen_slider.generic.statusBar		= "Rail spiral green color";
	m_effectsMenu.part_spiralgreen_amount.generic.type			= UITYPE_ACTION;
	m_effectsMenu.part_spiralgreen_amount.generic.flags			= UIF_LEFT_JUSTIFY|UIF_NOSELECT;

	m_effectsMenu.part_spiralblue_slider.generic.type		= UITYPE_SLIDER;
	m_effectsMenu.part_spiralblue_slider.generic.name		= "Rail spiral blue";
	m_effectsMenu.part_spiralblue_slider.generic.callBack	= SpiralBlueColorFunc;
	m_effectsMenu.part_spiralblue_slider.minValue			= 0;
	m_effectsMenu.part_spiralblue_slider.maxValue			= 10;
	m_effectsMenu.part_spiralblue_slider.generic.statusBar	= "Rail spiral blue color";
	m_effectsMenu.part_spiralblue_amount.generic.type		= UITYPE_ACTION;
	m_effectsMenu.part_spiralblue_amount.generic.flags		= UIF_LEFT_JUSTIFY|UIF_NOSELECT;

	m_effectsMenu.part_cull_toggle.generic.type			= UITYPE_SPINCONTROL;
	m_effectsMenu.part_cull_toggle.generic.name			= "Particle culling";
	m_effectsMenu.part_cull_toggle.generic.callBack		= PartCullFunc;
	m_effectsMenu.part_cull_toggle.itemNames			= onoff_names;
	m_effectsMenu.part_cull_toggle.generic.statusBar	= "Particle culling (on = faster)";

	m_effectsMenu.part_lighting_toggle.generic.type			= UITYPE_SPINCONTROL;
	m_effectsMenu.part_lighting_toggle.generic.name			= "Particle lighting";
	m_effectsMenu.part_lighting_toggle.generic.callBack		= PartLightFunc;
	m_effectsMenu.part_lighting_toggle.itemNames			= onoff_names;
	m_effectsMenu.part_lighting_toggle.generic.statusBar	= "Enables particle lighting";

	m_effectsMenu.explorattle_toggle.generic.type		= UITYPE_SPINCONTROL;
	m_effectsMenu.explorattle_toggle.generic.name		= "Explosion rattling";
	m_effectsMenu.explorattle_toggle.generic.callBack	= ExploRattleFunc;
	m_effectsMenu.explorattle_toggle.itemNames			= onoff_names;
	m_effectsMenu.explorattle_toggle.generic.statusBar	= "Toggles screen rattling when near explosions";

	m_effectsMenu.explorattle_scale_slider.generic.type			= UITYPE_SLIDER;
	m_effectsMenu.explorattle_scale_slider.generic.name			= "Rattle severity";
	m_effectsMenu.explorattle_scale_slider.generic.callBack		= ExploRattleScaleFunc;
	m_effectsMenu.explorattle_scale_slider.minValue				= 1;
	m_effectsMenu.explorattle_scale_slider.maxValue				= 9;
	m_effectsMenu.explorattle_scale_slider.generic.statusBar	= "Severity of explosion rattles";
	m_effectsMenu.explorattle_scale_amount.generic.type			= UITYPE_ACTION;
	m_effectsMenu.explorattle_scale_amount.generic.flags		= UIF_LEFT_JUSTIFY|UIF_NOSELECT;

	m_effectsMenu.mapeffects_toggle.generic.type		= UITYPE_SPINCONTROL;
	m_effectsMenu.mapeffects_toggle.generic.name		= "Map effects";
	m_effectsMenu.mapeffects_toggle.generic.callBack	= MapEffectsToggleFunc;
	m_effectsMenu.mapeffects_toggle.itemNames			= onoff_names;
	m_effectsMenu.mapeffects_toggle.generic.statusBar	= "Toggles scripted map particle effects";
	
	//
	// decals
	//

	m_effectsMenu.dec_header.generic.type		= UITYPE_ACTION;
	m_effectsMenu.dec_header.generic.flags		= UIF_NOSELECT|UIF_CENTERED|UIF_MEDIUM|UIF_SHADOW;
	m_effectsMenu.dec_header.generic.name		= "Decal Settings";

	m_effectsMenu.dec_toggle.generic.type		= UITYPE_SPINCONTROL;
	m_effectsMenu.dec_toggle.generic.name		= "Decals";
	m_effectsMenu.dec_toggle.generic.callBack	= DecalToggleFunc;
	m_effectsMenu.dec_toggle.itemNames			= onoff_names;
	m_effectsMenu.dec_toggle.generic.statusBar	= "Toggle decals on or off";

	m_effectsMenu.dec_lod_toggle.generic.type		= UITYPE_SPINCONTROL;
	m_effectsMenu.dec_lod_toggle.generic.name		= "Decal LOD";
	m_effectsMenu.dec_lod_toggle.generic.callBack	= DecalLODFunc;
	m_effectsMenu.dec_lod_toggle.itemNames			= onoff_names;
	m_effectsMenu.dec_lod_toggle.generic.statusBar	= "Toggle decal lod (skips small far away decals)";

	m_effectsMenu.dec_life_slider.generic.type			= UITYPE_SLIDER;
	m_effectsMenu.dec_life_slider.generic.name			= "Decal life";
	m_effectsMenu.dec_life_slider.generic.callBack		= DecalLifeFunc;
	m_effectsMenu.dec_life_slider.minValue				= 0;
	m_effectsMenu.dec_life_slider.maxValue				= 10;
	m_effectsMenu.dec_life_slider.generic.statusBar		= "Decal life time";
	m_effectsMenu.dec_life_amount.generic.type			= UITYPE_ACTION;
	m_effectsMenu.dec_life_amount.generic.flags			= UIF_LEFT_JUSTIFY|UIF_NOSELECT;

	m_effectsMenu.dec_burnlife_slider.generic.type			= UITYPE_SLIDER;
	m_effectsMenu.dec_burnlife_slider.generic.name			= "Burn life";
	m_effectsMenu.dec_burnlife_slider.generic.callBack		= DecalBurnLifeFunc;
	m_effectsMenu.dec_burnlife_slider.minValue				= 0;
	m_effectsMenu.dec_burnlife_slider.maxValue				= 20;
	m_effectsMenu.dec_burnlife_slider.generic.statusBar	= "Decal burn life time";
	m_effectsMenu.dec_burnlife_amount.generic.type			= UITYPE_ACTION;
	m_effectsMenu.dec_burnlife_amount.generic.flags		= UIF_LEFT_JUSTIFY|UIF_NOSELECT;

	m_effectsMenu.dec_max_slider.generic.type		= UITYPE_SLIDER;
	m_effectsMenu.dec_max_slider.generic.name		= "Max decals";
	m_effectsMenu.dec_max_slider.generic.callBack	= DecalMaxFunc;
	m_effectsMenu.dec_max_slider.minValue			= 1;
	m_effectsMenu.dec_max_slider.maxValue			= MAX_REF_DECALS / 1000;
	m_effectsMenu.dec_max_slider.generic.statusBar	= "Maximum decals allowed";
	m_effectsMenu.dec_max_amount.generic.type		= UITYPE_ACTION;
	m_effectsMenu.dec_max_amount.generic.flags		= UIF_LEFT_JUSTIFY|UIF_NOSELECT;

	//
	// materials
	//

	m_effectsMenu.mat_header.generic.type			= UITYPE_ACTION;
	m_effectsMenu.mat_header.generic.flags			= UIF_NOSELECT|UIF_CENTERED|UIF_MEDIUM|UIF_SHADOW;
	m_effectsMenu.mat_header.generic.name			= "Material Settings";

	m_effectsMenu.mat_caustics_toggle.generic.type		= UITYPE_SPINCONTROL;
	m_effectsMenu.mat_caustics_toggle.generic.name		= "Caustics";
	m_effectsMenu.mat_caustics_toggle.generic.callBack	= CausticsFunc;
	m_effectsMenu.mat_caustics_toggle.itemNames			= onoff_names;
	m_effectsMenu.mat_caustics_toggle.generic.statusBar	= "Material-based underwater/lava/slime surface caustics";

	m_effectsMenu.mat_detail_toggle.generic.type			= UITYPE_SPINCONTROL;
	m_effectsMenu.mat_detail_toggle.generic.name			= "Detail Textures";
	m_effectsMenu.mat_detail_toggle.generic.callBack		= MaterialDetailFunc;
	m_effectsMenu.mat_detail_toggle.itemNames				= onoff_names;
	m_effectsMenu.mat_detail_toggle.generic.statusBar		= "Toggle material passes marked as 'detail'";

	//
	// misc
	//

	m_effectsMenu.back_action.generic.type		= UITYPE_ACTION;
	m_effectsMenu.back_action.generic.flags		= UIF_CENTERED|UIF_LARGE|UIF_SHADOW;
	m_effectsMenu.back_action.generic.name		= "< Back";
	m_effectsMenu.back_action.generic.callBack	= Menu_Pop;
	m_effectsMenu.back_action.generic.statusBar	= "Back a menu";

	EffectsMenu_SetValues ();


	UI_AddItem (&m_effectsMenu.frameWork,		&m_effectsMenu.banner);

	//
	// particles
	//

	UI_AddItem (&m_effectsMenu.frameWork,		&m_effectsMenu.part_header);

	UI_AddItem (&m_effectsMenu.frameWork,		&m_effectsMenu.part_gore_list);
	UI_AddItem (&m_effectsMenu.frameWork,		&m_effectsMenu.part_smokelinger_slider);
	UI_AddItem (&m_effectsMenu.frameWork,		&m_effectsMenu.part_smokelinger_amount);

	UI_AddItem (&m_effectsMenu.frameWork,		&m_effectsMenu.part_railtrail_list);

	UI_AddItem (&m_effectsMenu.frameWork,		&m_effectsMenu.part_railred_slider);
	UI_AddItem (&m_effectsMenu.frameWork,		&m_effectsMenu.part_railred_amount);
	UI_AddItem (&m_effectsMenu.frameWork,		&m_effectsMenu.part_railgreen_slider);
	UI_AddItem (&m_effectsMenu.frameWork,		&m_effectsMenu.part_railgreen_amount);
	UI_AddItem (&m_effectsMenu.frameWork,		&m_effectsMenu.part_railblue_slider);
	UI_AddItem (&m_effectsMenu.frameWork,		&m_effectsMenu.part_railblue_amount);

	UI_AddItem (&m_effectsMenu.frameWork,		&m_effectsMenu.part_spiralred_slider);
	UI_AddItem (&m_effectsMenu.frameWork,		&m_effectsMenu.part_spiralred_amount);
	UI_AddItem (&m_effectsMenu.frameWork,		&m_effectsMenu.part_spiralgreen_slider);
	UI_AddItem (&m_effectsMenu.frameWork,		&m_effectsMenu.part_spiralgreen_amount);
	UI_AddItem (&m_effectsMenu.frameWork,		&m_effectsMenu.part_spiralblue_slider);
	UI_AddItem (&m_effectsMenu.frameWork,		&m_effectsMenu.part_spiralblue_amount);

	UI_AddItem (&m_effectsMenu.frameWork,		&m_effectsMenu.part_cull_toggle);
	UI_AddItem (&m_effectsMenu.frameWork,		&m_effectsMenu.part_lighting_toggle);

	UI_AddItem (&m_effectsMenu.frameWork,		&m_effectsMenu.explorattle_toggle);
	UI_AddItem (&m_effectsMenu.frameWork,		&m_effectsMenu.explorattle_scale_slider);
	UI_AddItem (&m_effectsMenu.frameWork,		&m_effectsMenu.explorattle_scale_amount);

	UI_AddItem (&m_effectsMenu.frameWork,		&m_effectsMenu.mapeffects_toggle);
	//
	// decals
	//

	UI_AddItem (&m_effectsMenu.frameWork,		&m_effectsMenu.dec_header);

	UI_AddItem (&m_effectsMenu.frameWork,		&m_effectsMenu.dec_toggle);
	UI_AddItem (&m_effectsMenu.frameWork,		&m_effectsMenu.dec_lod_toggle);
	UI_AddItem (&m_effectsMenu.frameWork,		&m_effectsMenu.dec_life_slider);
	UI_AddItem (&m_effectsMenu.frameWork,		&m_effectsMenu.dec_life_amount);
	UI_AddItem (&m_effectsMenu.frameWork,		&m_effectsMenu.dec_burnlife_slider);
	UI_AddItem (&m_effectsMenu.frameWork,		&m_effectsMenu.dec_burnlife_amount);
	UI_AddItem (&m_effectsMenu.frameWork,		&m_effectsMenu.dec_max_slider);
	UI_AddItem (&m_effectsMenu.frameWork,		&m_effectsMenu.dec_max_amount);

	//
	// materials
	//

	UI_AddItem (&m_effectsMenu.frameWork,		&m_effectsMenu.mat_header);

	UI_AddItem (&m_effectsMenu.frameWork,		&m_effectsMenu.mat_caustics_toggle);
	UI_AddItem (&m_effectsMenu.frameWork,		&m_effectsMenu.mat_detail_toggle);

	//
	// misc
	//

	UI_AddItem (&m_effectsMenu.frameWork,		&m_effectsMenu.back_action);

	UI_FinishFramework (&m_effectsMenu.frameWork, true);
}


/*
=============
EffectsMenu_Close
=============
*/
static struct sfx_t *EffectsMenu_Close ()
{
	return uiMedia.sounds.menuOut;
}


/*
=============
EffectsMenu_Draw
=============
*/
static void EffectsMenu_Draw ()
{
	float	y;

	// Initialize if necessary
	if (!m_effectsMenu.frameWork.initialized)
		EffectsMenu_Init ();

	// Dynamically position
	m_effectsMenu.frameWork.x			= cg.refConfig.vidWidth * 0.5f;
	m_effectsMenu.frameWork.y			= 0;

	m_effectsMenu.banner.generic.x		= 0;
	m_effectsMenu.banner.generic.y		= 0;

	y = m_effectsMenu.banner.height * UI_SCALE;

	//
	// particles
	//
	m_effectsMenu.part_header.generic.x					= 0;
	m_effectsMenu.part_header.generic.y					= y += UIFT_SIZEINC;
	m_effectsMenu.part_gore_list.generic.x				= 0;
	m_effectsMenu.part_gore_list.generic.y				= y += UIFT_SIZEINC + UIFT_SIZEINCMED;
	m_effectsMenu.part_smokelinger_slider.generic.x		= 0;
	m_effectsMenu.part_smokelinger_slider.generic.y		= y += UIFT_SIZEINC;
	m_effectsMenu.part_smokelinger_amount.generic.x		= (UIFT_SIZE * (SLIDER_RANGE + 5));
	m_effectsMenu.part_smokelinger_amount.generic.y		= y;
	m_effectsMenu.part_railtrail_list.generic.x			= 0;
	m_effectsMenu.part_railtrail_list.generic.y			= y += (UIFT_SIZEINC*2);
	m_effectsMenu.part_railred_slider.generic.x			= 0;
	m_effectsMenu.part_railred_slider.generic.y			= y += (UIFT_SIZEINC*2);
	m_effectsMenu.part_railred_amount.generic.x			= (UIFT_SIZE * (SLIDER_RANGE + 5));
	m_effectsMenu.part_railred_amount.generic.y			= y;
	m_effectsMenu.part_railgreen_slider.generic.x		= 0;
	m_effectsMenu.part_railgreen_slider.generic.y		= y += UIFT_SIZEINC;
	m_effectsMenu.part_railgreen_amount.generic.x		= (UIFT_SIZE * (SLIDER_RANGE + 5));
	m_effectsMenu.part_railgreen_amount.generic.y		= y;
	m_effectsMenu.part_railblue_slider.generic.x		= 0;
	m_effectsMenu.part_railblue_slider.generic.y		= y += UIFT_SIZEINC;
	m_effectsMenu.part_railblue_amount.generic.x		= (UIFT_SIZE * (SLIDER_RANGE + 5));
	m_effectsMenu.part_railblue_amount.generic.y		= y;
	m_effectsMenu.part_spiralred_slider.generic.x		= 0;
	m_effectsMenu.part_spiralred_slider.generic.y		= y += (UIFT_SIZEINC*2);
	m_effectsMenu.part_spiralred_amount.generic.x		= (UIFT_SIZE * (SLIDER_RANGE + 5));
	m_effectsMenu.part_spiralred_amount.generic.y		= y;
	m_effectsMenu.part_spiralgreen_slider.generic.x		= 0;
	m_effectsMenu.part_spiralgreen_slider.generic.y		= y += UIFT_SIZEINC;
	m_effectsMenu.part_spiralgreen_amount.generic.x		= (UIFT_SIZE * (SLIDER_RANGE + 5));
	m_effectsMenu.part_spiralgreen_amount.generic.y		= y;
	m_effectsMenu.part_spiralblue_slider.generic.x		= 0;
	m_effectsMenu.part_spiralblue_slider.generic.y		= y += UIFT_SIZEINC;
	m_effectsMenu.part_spiralblue_amount.generic.x		= (UIFT_SIZE * (SLIDER_RANGE + 5));
	m_effectsMenu.part_spiralblue_amount.generic.y		= y;
	m_effectsMenu.part_cull_toggle.generic.x			= 0;
	m_effectsMenu.part_cull_toggle.generic.y			= y += (UIFT_SIZEINC*2);
	m_effectsMenu.part_lighting_toggle.generic.x		= 0;
	m_effectsMenu.part_lighting_toggle.generic.y		= y += UIFT_SIZEINC;
	m_effectsMenu.explorattle_toggle.generic.x			= 0;
	m_effectsMenu.explorattle_toggle.generic.y			= y += (UIFT_SIZEINC*2);
	m_effectsMenu.explorattle_scale_slider.generic.x	= 0;
	m_effectsMenu.explorattle_scale_slider.generic.y	= y += UIFT_SIZEINC;
	m_effectsMenu.explorattle_scale_amount.generic.x	= (UIFT_SIZE * (SLIDER_RANGE + 5));
	m_effectsMenu.explorattle_scale_amount.generic.y	= y;
	m_effectsMenu.mapeffects_toggle.generic.x			= 0;
	m_effectsMenu.mapeffects_toggle.generic.y			= y += UIFT_SIZEINC*2;

	//
	// decals
	//
	m_effectsMenu.dec_header.generic.x				= 0;
	m_effectsMenu.dec_header.generic.y				= y += UIFT_SIZEINC*2;
	m_effectsMenu.dec_toggle.generic.x				= 0;
	m_effectsMenu.dec_toggle.generic.y				= y += UIFT_SIZEINC + UIFT_SIZEINCMED;
	m_effectsMenu.dec_lod_toggle.generic.x			= 0;
	m_effectsMenu.dec_lod_toggle.generic.y			= y += UIFT_SIZEINC;
	m_effectsMenu.dec_life_slider.generic.x			= 0;
	m_effectsMenu.dec_life_slider.generic.y			= y += UIFT_SIZEINC;
	m_effectsMenu.dec_life_amount.generic.x			= (UIFT_SIZE * (SLIDER_RANGE + 5));
	m_effectsMenu.dec_life_amount.generic.y			= y;
	m_effectsMenu.dec_burnlife_slider.generic.x		= 0;
	m_effectsMenu.dec_burnlife_slider.generic.y		= y += UIFT_SIZEINC;
	m_effectsMenu.dec_burnlife_amount.generic.x		= (UIFT_SIZE * (SLIDER_RANGE + 5));
	m_effectsMenu.dec_burnlife_amount.generic.y		= y;
	m_effectsMenu.dec_max_slider.generic.x			= 0;
	m_effectsMenu.dec_max_slider.generic.y			= y += UIFT_SIZEINC;
	m_effectsMenu.dec_max_amount.generic.x			= (UIFT_SIZE * (SLIDER_RANGE + 5));
	m_effectsMenu.dec_max_amount.generic.y			= y;

	//
	// materials
	//
	m_effectsMenu.mat_header.generic.x			= 0;
	m_effectsMenu.mat_header.generic.y			= y += UIFT_SIZEINC*2;
	m_effectsMenu.mat_caustics_toggle.generic.x	= 0;
	m_effectsMenu.mat_caustics_toggle.generic.y	= y += UIFT_SIZEINC;
	m_effectsMenu.mat_detail_toggle.generic.x	= 0;
	m_effectsMenu.mat_detail_toggle.generic.y	= y += UIFT_SIZEINC;

	//
	// misc
	//
	m_effectsMenu.back_action.generic.x			= 0;
	m_effectsMenu.back_action.generic.y			= y += UIFT_SIZEINC + UIFT_SIZEINCLG;

	// Render
	UI_DrawInterface (&m_effectsMenu.frameWork);
}


/*
=============
UI_EffectsMenu_f
=============
*/

void UI_EffectsMenu_f ()
{
	EffectsMenu_Init ();
	M_PushMenu (&m_effectsMenu.frameWork, EffectsMenu_Draw, EffectsMenu_Close, M_KeyHandler);
}
