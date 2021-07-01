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
// m_opts_sound.c
//

#include "m_local.h"

void Sound_RestartBox ()
{
	float	midrow = (cg.refConfig.vidWidth*0.5) - (18*UIFT_SIZEMED);
	float	midcol = (cg.refConfig.vidHeight*0.5) - (3*UIFT_SIZEMED);

	UI_DrawTextBox (midrow, midcol, UIFT_SCALEMED, 36, 4);
	cgi.R_DrawString (NULL, midrow + (UIFT_SIZEMED*2), midcol + UIFT_SIZEMED, UIFT_SCALEMED, UIFT_SCALEMED, 0,		"       --- PLEASE WAIT! ---       ",	Q_BColorGreen);
	cgi.R_DrawString (NULL, midrow + (UIFT_SIZEMED*2), midcol + (UIFT_SIZEMED*2), UIFT_SCALEMED, UIFT_SCALEMED, 0,	"The sound system is currently res-",	Q_BColorGreen);
	cgi.R_DrawString (NULL, midrow + (UIFT_SIZEMED*2), midcol + (UIFT_SIZEMED*3), UIFT_SCALEMED, UIFT_SCALEMED, 0,	"tarting, this could take up to a",		Q_BColorGreen);
	cgi.R_DrawString (NULL, midrow + (UIFT_SIZEMED*2), midcol + (UIFT_SIZEMED*4), UIFT_SCALEMED, UIFT_SCALEMED, 0,	"minute so, please be patient.",		Q_BColorGreen);

	cgi.R_EndFrame ();					// the text box won't show up unless we do a buffer swap
	cgi.Cbuf_AddText ("snd_restart\n");	// restart the sound system
}

/*
=======================================================================

	SOUND MENU

=======================================================================
*/

struct m_soundMenu_t {
	// Menu items
	uiFrameWork_t	frameWork;

	uiImage_t		banner;
	uiAction_t		header;

	uiList_t		sound_toggle;

	uiSlider_t		sfxvolume_slider;
	uiAction_t		sfxvolume_amount;
	uiList_t		cdtoggle_toggle;

	// openal
/*	uiAction_t		al_header;

	uiSlider_t		dopplerfactor_slider;
	uiAction_t		dopplerfactor_amount;
	uiSlider_t		dopplervelocity_slider;
	uiAction_t		dopplervelocity_amount;

	uiList_t		al_extensions_toggle;
	uiList_t		al_ext_eax_toggle;
*/
	// software
	uiAction_t		sw_header;
	uiList_t		sw_quality_list;
	uiList_t		sw_combatibility_list;

	uiAction_t		back_action;
};

static m_soundMenu_t	m_soundMenu;

static void SoundToggleFunc (void *unused)
{
	cgi.Cvar_SetValue ("s_initSound", m_soundMenu.sound_toggle.curValue, true);
	Sound_RestartBox ();
}

static void UpdateVolumeFunc (void *unused)
{
	cgi.Cvar_SetValue ("s_volume", m_soundMenu.sfxvolume_slider.curValue * 0.05, false);
	m_soundMenu.sfxvolume_amount.generic.name = cgi.Cvar_GetStringValue ("s_volume");
}

static void ALDopFactorFunc (void *unused)
{
//	cgi.Cvar_SetValue ("al_dopplerfactor", m_soundMenu.dopplerfactor_slider.curValue * 0.1, false);
//	m_soundMenu.dopplerfactor_amount.generic.name = cgi.Cvar_GetStringValue ("al_dopplerfactor");
}

static void ALDopVelocityFunc (void *unused)
{
//	cgi.Cvar_SetValue ("al_dopplervelocity", m_soundMenu.dopplervelocity_slider.curValue * 100, false);
//	m_soundMenu.dopplervelocity_amount.generic.name = cgi.Cvar_GetStringValue ("al_dopplervelocity");
}

static void ALExtensionsFunc (void *unused)
{
//	cgi.Cvar_SetValue ("al_extensions", m_soundMenu.al_extensions_toggle.curValue);
//	Sound_RestartBox ();
}

static void ALExtEAXFunc (void *unused)
{
//	cgi.Cvar_SetValue ("al_ext_eax", m_soundMenu.al_ext_eax_toggle.curValue);
//	Sound_RestartBox ();
}

static void UpdateCDToggleFunc (void *unused)
{
	cgi.Cvar_SetValue ("cd_nocd", !m_soundMenu.cdtoggle_toggle.curValue, false);
}

static void UpdateSoundQualityFunc (void *unused)
{
	static char quality_ints[] = {
		8,
		11,
		16,
		22,
		32,
		44,
		48,
		0
	};

	cgi.Cvar_SetValue ("s_khz", quality_ints[m_soundMenu.sw_quality_list.curValue], true);

	if (cgi.Cvar_GetIntegerValue ("s_khz") < 16) { cgi.Cvar_SetValue ("s_loadas8bit", 1, true); }
	else { cgi.Cvar_SetValue ("s_loadas8bit", 0, true); }

	cgi.Cvar_SetValue ("s_primary", m_soundMenu.sw_combatibility_list.curValue, true);

	Sound_RestartBox ();
}


/*
=============
Sound_SetValues
=============
*/
static void SoundMenu_SetValues ()
{
//	cgi.Cvar_SetValue ("s_initSound",	clamp (cgi.Cvar_GetIntegerValue ("s_initSound"), 0, 2), true);
	cgi.Cvar_SetValue ("s_initSound",	clamp (cgi.Cvar_GetIntegerValue ("s_initSound"), 0, 1), true);
	m_soundMenu.sound_toggle.curValue		= cgi.Cvar_GetIntegerValue ("s_initSound");

	m_soundMenu.sfxvolume_slider.curValue		= cgi.Cvar_GetFloatValue ("s_volume") * 20;
	m_soundMenu.sfxvolume_amount.generic.name	= cgi.Cvar_GetStringValue ("s_volume");

	cgi.Cvar_SetValue ("cd_nocd",			clamp (cgi.Cvar_GetIntegerValue ("cd_nocd"), 0, 1), false);
	m_soundMenu.cdtoggle_toggle.curValue	= !cgi.Cvar_GetIntegerValue ("cd_nocd");

/*	m_soundMenu.dopplerfactor_slider.curValue		= cgi.Cvar_GetFloatValue ("al_dopplerfactor") * 10;
	m_soundMenu.dopplerfactor_amount.generic.name	= cgi.Cvar_GetStringValue ("al_dopplerfactor");

	m_soundMenu.dopplervelocity_slider.curValue		= cgi.Cvar_GetFloatValue ("al_dopplervelocity") * 0.01;
	m_soundMenu.dopplervelocity_amount.generic.name	= cgi.Cvar_GetStringValue ("al_dopplervelocity");

	cgi.Cvar_SetValue ("al_extensions",	clamp (cgi.Cvar_GetIntegerValue ("al_extensions"), 0, 1));
	m_soundMenu.al_extensions_toggle.curValue	= cgi.Cvar_GetIntegerValue ("al_extensions");

	cgi.Cvar_SetValue ("al_ext_eax",	clamp (cgi.Cvar_GetIntegerValue ("al_ext_eax"), 0, 1));
	m_soundMenu.al_ext_eax_toggle.curValue	= cgi.Cvar_GetIntegerValue ("al_ext_eax");
*/
	if (cgi.Cvar_GetIntegerValue ("s_khz") == 11) { m_soundMenu.sw_quality_list.curValue = 1; }
	else if (cgi.Cvar_GetIntegerValue ("s_khz") == 16) { m_soundMenu.sw_quality_list.curValue = 2; }
	else if (cgi.Cvar_GetIntegerValue ("s_khz") == 22) { m_soundMenu.sw_quality_list.curValue = 3; }
	else if (cgi.Cvar_GetIntegerValue ("s_khz") == 32) { m_soundMenu.sw_quality_list.curValue = 4; }
	else if (cgi.Cvar_GetIntegerValue ("s_khz") == 44) { m_soundMenu.sw_quality_list.curValue = 5; }
	else if (cgi.Cvar_GetIntegerValue ("s_khz") == 48) { m_soundMenu.sw_quality_list.curValue = 6; }
	else { m_soundMenu.sw_quality_list.curValue = 0; }

	m_soundMenu.sw_combatibility_list.curValue	= cgi.Cvar_GetIntegerValue  ("s_primary");
}


/*
=============
SoundMenu_Init
=============
*/
static void SoundMenu_Init ()
{
	static char *cd_music_items[] = {
		"disabled",
		"enabled",
		0
	};

	static char *compatibility_items[] = {
		"max compatibility",
		"max performance",
		0
	};

	static char *soundinit_items[] = {
		"off",
		"on",
//		"OpenAL [ EXPERIMENTAL ]",
		0
	};

	static char *quality_items[] = {
		"8KHz",
		"11KHz",
		"16KHz",
		"22KHz",
		"32KHz",
		"44KHz",
		"48KHz",
		0
	};

	UI_StartFramework (&m_soundMenu.frameWork, FWF_CENTERHEIGHT);

	m_soundMenu.banner.generic.type		= UITYPE_IMAGE;
	m_soundMenu.banner.generic.flags	= UIF_NOSELECT|UIF_CENTERED;
	m_soundMenu.banner.generic.name		= NULL;
	m_soundMenu.banner.mat				= uiMedia.banners.options;

	m_soundMenu.header.generic.type		= UITYPE_ACTION;
	m_soundMenu.header.generic.flags	= UIF_NOSELECT|UIF_CENTERED|UIF_MEDIUM|UIF_SHADOW;
	m_soundMenu.header.generic.name		= "All Sound Settings";

	m_soundMenu.sound_toggle.generic.type		= UITYPE_SPINCONTROL;
	m_soundMenu.sound_toggle.generic.name		= "Sound";
	m_soundMenu.sound_toggle.generic.callBack	= SoundToggleFunc;
	m_soundMenu.sound_toggle.itemNames			= soundinit_items;
	m_soundMenu.sound_toggle.generic.statusBar	= "Toggle Sound";

	m_soundMenu.sfxvolume_slider.generic.type		= UITYPE_SLIDER;
	m_soundMenu.sfxvolume_slider.generic.name		= "Volume";
	m_soundMenu.sfxvolume_slider.generic.callBack	= UpdateVolumeFunc;
	m_soundMenu.sfxvolume_slider.minValue			= 0;
	m_soundMenu.sfxvolume_slider.maxValue			= 20;
	m_soundMenu.sfxvolume_slider.generic.statusBar	= "Volume Control";
	m_soundMenu.sfxvolume_amount.generic.type		= UITYPE_ACTION;
	m_soundMenu.sfxvolume_amount.generic.flags		= UIF_LEFT_JUSTIFY|UIF_NOSELECT;

	m_soundMenu.cdtoggle_toggle.generic.type		= UITYPE_SPINCONTROL;
	m_soundMenu.cdtoggle_toggle.generic.name		= "CD music";
	m_soundMenu.cdtoggle_toggle.generic.callBack	= UpdateCDToggleFunc;
	m_soundMenu.cdtoggle_toggle.itemNames			= cd_music_items;
	m_soundMenu.cdtoggle_toggle.generic.statusBar	= "Toggle CD Play";

	// openal
/*	m_soundMenu.al_header.generic.type		= UITYPE_ACTION;
	m_soundMenu.al_header.generic.flags		= UIF_NOSELECT|UIF_CENTERED|UIF_MEDIUM|UIF_SHADOW;
	m_soundMenu.al_header.generic.name		= "OpenAL Sound Settings";

	m_soundMenu.dopplerfactor_slider.generic.type		= UITYPE_SLIDER;
	m_soundMenu.dopplerfactor_slider.generic.name		= "Doppler Factor";
	m_soundMenu.dopplerfactor_slider.generic.callBack	= ALDopFactorFunc;
	m_soundMenu.dopplerfactor_slider.minValue			= 0;
	m_soundMenu.dopplerfactor_slider.maxValue			= 10;
	m_soundMenu.dopplerfactor_slider.generic.statusBar	= "OpenAL Doppler Factor";
	m_soundMenu.dopplerfactor_amount.generic.type		= UITYPE_ACTION;
	m_soundMenu.dopplerfactor_amount.generic.flags		= UIF_LEFT_JUSTIFY|UIF_NOSELECT;

	m_soundMenu.dopplervelocity_slider.generic.type		= UITYPE_SLIDER;
	m_soundMenu.dopplervelocity_slider.generic.name		= "Doppler Velocity";
	m_soundMenu.dopplervelocity_slider.generic.callBack	= ALDopVelocityFunc;
	m_soundMenu.dopplervelocity_slider.minValue			= 1;
	m_soundMenu.dopplervelocity_slider.maxValue			= 10;
	m_soundMenu.dopplervelocity_slider.generic.statusBar= "OpenAL Doppler Velocity";
	m_soundMenu.dopplervelocity_amount.generic.type		= UITYPE_ACTION;
	m_soundMenu.dopplervelocity_amount.generic.flags	= UIF_LEFT_JUSTIFY|UIF_NOSELECT;

	m_soundMenu.al_extensions_toggle.generic.type		= UITYPE_SPINCONTROL;
	m_soundMenu.al_extensions_toggle.generic.name		= "OpenAL Extensions";
	m_soundMenu.al_extensions_toggle.generic.callBack	= ALExtensionsFunc;
	m_soundMenu.al_extensions_toggle.itemNames			= soundinit_items;
	m_soundMenu.al_extensions_toggle.generic.statusBar	= "Toggle OpenAL Extensions";

	m_soundMenu.al_ext_eax_toggle.generic.type			= UITYPE_SPINCONTROL;
	m_soundMenu.al_ext_eax_toggle.generic.name			= "EAX 2.0 extension";
	m_soundMenu.al_ext_eax_toggle.generic.callBack		= ALExtEAXFunc;
	m_soundMenu.al_ext_eax_toggle.itemNames				= soundinit_items;
	m_soundMenu.al_ext_eax_toggle.generic.statusBar		= "Toggle the OpenAL extension EAX 2.0";		
*/
	// software
	m_soundMenu.sw_header.generic.type		= UITYPE_ACTION;
	m_soundMenu.sw_header.generic.flags		= UIF_NOSELECT|UIF_CENTERED|UIF_MEDIUM|UIF_SHADOW;
	m_soundMenu.sw_header.generic.name		= "Software Sound Settings";

	m_soundMenu.sw_quality_list.generic.type		= UITYPE_SPINCONTROL;
	m_soundMenu.sw_quality_list.generic.name		= "Sound quality";
	m_soundMenu.sw_quality_list.generic.callBack	= UpdateSoundQualityFunc;
	m_soundMenu.sw_quality_list.itemNames			= quality_items;
	m_soundMenu.sw_quality_list.generic.statusBar	= "Sound Quality";

	m_soundMenu.sw_combatibility_list.generic.type		= UITYPE_SPINCONTROL;
	m_soundMenu.sw_combatibility_list.generic.name		= "Compatibility";
	m_soundMenu.sw_combatibility_list.generic.callBack	= UpdateSoundQualityFunc;
	m_soundMenu.sw_combatibility_list.itemNames			= compatibility_items;
	m_soundMenu.sw_combatibility_list.generic.statusBar	= "Primary/Secondary sound buffer";

	m_soundMenu.back_action.generic.type		= UITYPE_ACTION;
	m_soundMenu.back_action.generic.flags		= UIF_CENTERED|UIF_LARGE|UIF_SHADOW;
	m_soundMenu.back_action.generic.name		= "< Back";
	m_soundMenu.back_action.generic.callBack	= Menu_Pop;
	m_soundMenu.back_action.generic.statusBar	= "Back a menu";

	SoundMenu_SetValues ();

	UI_AddItem (&m_soundMenu.frameWork,		&m_soundMenu.banner);
	UI_AddItem (&m_soundMenu.frameWork,		&m_soundMenu.header);

	UI_AddItem (&m_soundMenu.frameWork,		&m_soundMenu.sound_toggle);
	
	UI_AddItem (&m_soundMenu.frameWork,		&m_soundMenu.sfxvolume_slider);
	UI_AddItem (&m_soundMenu.frameWork,		&m_soundMenu.sfxvolume_amount);
	UI_AddItem (&m_soundMenu.frameWork,		&m_soundMenu.cdtoggle_toggle);

/*	UI_AddItem (&m_soundMenu.frameWork,		&m_soundMenu.al_header);
	UI_AddItem (&m_soundMenu.frameWork,		&m_soundMenu.dopplerfactor_slider);
	UI_AddItem (&m_soundMenu.frameWork,		&m_soundMenu.dopplerfactor_amount);
	UI_AddItem (&m_soundMenu.frameWork,		&m_soundMenu.dopplervelocity_slider);
	UI_AddItem (&m_soundMenu.frameWork,		&m_soundMenu.dopplervelocity_amount);
	UI_AddItem (&m_soundMenu.frameWork,		&m_soundMenu.al_extensions_toggle);
	UI_AddItem (&m_soundMenu.frameWork,		&m_soundMenu.al_ext_eax_toggle);
*/
	UI_AddItem (&m_soundMenu.frameWork,		&m_soundMenu.sw_header);
	UI_AddItem (&m_soundMenu.frameWork,		&m_soundMenu.sw_quality_list);
	UI_AddItem (&m_soundMenu.frameWork,		&m_soundMenu.sw_combatibility_list);

	UI_AddItem (&m_soundMenu.frameWork,		&m_soundMenu.back_action);

	UI_FinishFramework (&m_soundMenu.frameWork, true);
}


/*
=============
SoundMenu_Close
=============
*/
static struct sfx_t *SoundMenu_Close ()
{
	return uiMedia.sounds.menuOut;
}


/*
=============
SoundMenu_Draw
=============
*/
static void SoundMenu_Draw ()
{
	float	y;

	// Initialize if necessary
	if (!m_soundMenu.frameWork.initialized)
		SoundMenu_Init ();

	// Dynamically position
	m_soundMenu.frameWork.x		= cg.refConfig.vidWidth * 0.5f;
	m_soundMenu.frameWork.y		= 0;

	m_soundMenu.banner.generic.x		= 0;
	m_soundMenu.banner.generic.y		= 0;

	y = m_soundMenu.banner.height * UI_SCALE;

	m_soundMenu.header.generic.x					= 0;
	m_soundMenu.header.generic.y					= y += UIFT_SIZEINC;
	m_soundMenu.sound_toggle.generic.x				= 0;
	m_soundMenu.sound_toggle.generic.y				= y += UIFT_SIZEINC + UIFT_SIZEINCMED;
	m_soundMenu.sfxvolume_slider.generic.x			= 0;
	m_soundMenu.sfxvolume_slider.generic.y			= y += UIFT_SIZEINC;
	m_soundMenu.sfxvolume_amount.generic.x			= (UIFT_SIZE * (SLIDER_RANGE + 5));
	m_soundMenu.sfxvolume_amount.generic.y			= y;
	m_soundMenu.cdtoggle_toggle.generic.x			= 0;
	m_soundMenu.cdtoggle_toggle.generic.y			= y += UIFT_SIZEINC;
/*	m_soundMenu.al_header.generic.x					= 0;
	m_soundMenu.al_header.generic.y					= y += UIFT_SIZEINC + UIFT_SIZEINCMED;
	m_soundMenu.dopplerfactor_slider.generic.x		= 0;
	m_soundMenu.dopplerfactor_slider.generic.y		= y += UIFT_SIZEINC + UIFT_SIZEINCMED;
	m_soundMenu.dopplerfactor_amount.generic.x		= (UIFT_SIZE * (SLIDER_RANGE + 5));
	m_soundMenu.dopplerfactor_amount.generic.y		= y;
	m_soundMenu.dopplervelocity_slider.generic.x	= 0;
	m_soundMenu.dopplervelocity_slider.generic.y	= y += UIFT_SIZEINC;
	m_soundMenu.dopplervelocity_amount.generic.x	= (UIFT_SIZE * (SLIDER_RANGE + 5));
	m_soundMenu.dopplervelocity_amount.generic.y	= y;
	m_soundMenu.al_extensions_toggle.generic.x		= 0;
	m_soundMenu.al_extensions_toggle.generic.y		= y += UIFT_SIZEINC*2;
	m_soundMenu.al_ext_eax_toggle.generic.x			= 0;
	m_soundMenu.al_ext_eax_toggle.generic.y			= y += UIFT_SIZEINC;*/
	m_soundMenu.sw_header.generic.x					= 0;
	m_soundMenu.sw_header.generic.y					= y += UIFT_SIZEINC + UIFT_SIZEINCMED;
	m_soundMenu.sw_quality_list.generic.x			= 0;
	m_soundMenu.sw_quality_list.generic.y			= y += UIFT_SIZEINC + UIFT_SIZEINCMED;
	m_soundMenu.sw_combatibility_list.generic.x		= 0;
	m_soundMenu.sw_combatibility_list.generic.y		= y += UIFT_SIZEINC;
	m_soundMenu.back_action.generic.x				= 0;
	m_soundMenu.back_action.generic.y				= y += UIFT_SIZEINC + UIFT_SIZEINCLG;

	// Render
	UI_DrawInterface (&m_soundMenu.frameWork);
}


/*
=============
UI_SoundMenu_f
=============
*/
void UI_SoundMenu_f ()
{
	SoundMenu_Init ();
	M_PushMenu (&m_soundMenu.frameWork, SoundMenu_Draw, SoundMenu_Close, M_KeyHandler);
}
