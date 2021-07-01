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
// m_vid_settings.c
//

#include "m_local.h"

/*
====================================================================

	VIDEO SETTINGS MENU

====================================================================
*/

struct m_vidSettingsMenu_t {
	// Menu items
	uiFrameWork_t		frameWork;

	uiImage_t			banner;
	uiAction_t			header;

	uiList_t			mode_list;

	uiList_t			tm_list;
	uiList_t			bitd_list;
	uiList_t			stencilbuf_list;
	uiList_t			dispfreq_list;

	uiList_t			fullscreen_toggle;
	uiSlider_t			screensize_slider;
	uiAction_t			screensize_amount;
	uiSlider_t			brightness_slider;
	uiAction_t			brightness_amount;
	uiList_t			gammapics_toggle;

	uiSlider_t			texqual_slider;
	uiAction_t			texqual_amount;

	uiList_t			finish_toggle;

	uiAction_t			apply_action;
	uiAction_t			reset_action;
	uiAction_t			cancel_action;
};

static m_vidSettingsMenu_t	m_vidSettingsMenu;

static void VIDSettingsMenu_Init ();
static void ResetDefaults (void *unused) {
	VIDSettingsMenu_Init ();
}

static void CancelChanges (void *unused) {
	M_PopMenu ();
}


/*
================
VIDSettingsMenu_ApplyValues
================
*/
static void VIDSettingsMenu_ApplyValues (void *unused)
{
	float	gamma;

	if (m_vidSettingsMenu.mode_list.curValue != 0) {
		cgi.Cvar_SetValue ("gl_mode",			m_vidSettingsMenu.mode_list.curValue-1, true);
		cgi.Cvar_SetValue ("vid_width",			0, true);
		cgi.Cvar_SetValue ("vid_height",		0, true);
	}

	if (m_vidSettingsMenu.tm_list.curValue == 1)		cgi.Cvar_Set ("gl_texturemode", "GL_LINEAR_MIPMAP_LINEAR", true);
	else if (m_vidSettingsMenu.tm_list.curValue == 0)	cgi.Cvar_Set ("gl_texturemode", "GL_LINEAR_MIPMAP_NEAREST", true);

	if (m_vidSettingsMenu.bitd_list.curValue == 0)		cgi.Cvar_Set ("gl_bitdepth", "0", true);
	else if (m_vidSettingsMenu.bitd_list.curValue == 1)	cgi.Cvar_Set ("gl_bitdepth", "16", true);
	else if (m_vidSettingsMenu.bitd_list.curValue == 2)	cgi.Cvar_Set ("gl_bitdepth", "32", true);

	cgi.Cvar_SetValue ("gl_stencilbuffer",		m_vidSettingsMenu.stencilbuf_list.curValue, true);

	if (m_vidSettingsMenu.dispfreq_list.curValue == 1)			cgi.Cvar_Set ("r_displayFreq", "60", true);
	else if (m_vidSettingsMenu.dispfreq_list.curValue == 2)		cgi.Cvar_Set ("r_displayFreq", "65", true);
	else if (m_vidSettingsMenu.dispfreq_list.curValue == 3)		cgi.Cvar_Set ("r_displayFreq", "70", true);
	else if (m_vidSettingsMenu.dispfreq_list.curValue == 4)		cgi.Cvar_Set ("r_displayFreq", "75", true);
	else if (m_vidSettingsMenu.dispfreq_list.curValue == 5)		cgi.Cvar_Set ("r_displayFreq", "80", true);
	else if (m_vidSettingsMenu.dispfreq_list.curValue == 6)		cgi.Cvar_Set ("r_displayFreq", "85", true);
	else if (m_vidSettingsMenu.dispfreq_list.curValue == 7)		cgi.Cvar_Set ("r_displayFreq", "90", true);
	else if (m_vidSettingsMenu.dispfreq_list.curValue == 8)		cgi.Cvar_Set ("r_displayFreq", "95", true);
	else if (m_vidSettingsMenu.dispfreq_list.curValue == 9)		cgi.Cvar_Set ("r_displayFreq", "100", true);
	else if (m_vidSettingsMenu.dispfreq_list.curValue == 10)	cgi.Cvar_Set ("r_displayFreq", "105", true);
	else if (m_vidSettingsMenu.dispfreq_list.curValue == 11)	cgi.Cvar_Set ("r_displayFreq", "110", true);
	else if (m_vidSettingsMenu.dispfreq_list.curValue == 12)	cgi.Cvar_Set ("r_displayFreq", "115", true);
	else if (m_vidSettingsMenu.dispfreq_list.curValue == 13)	cgi.Cvar_Set ("r_displayFreq", "120", true);
	else														cgi.Cvar_Set ("r_displayFreq", "0", true);

	cgi.Cvar_SetValue ("vid_fullscreen",	m_vidSettingsMenu.fullscreen_toggle.curValue, true);
	cgi.Cvar_SetValue ("viewsize",			m_vidSettingsMenu.screensize_slider.curValue * 10, true);

	/*
	** invert sense so greater = brighter, and scale to a range of 0.5 to 1.3
	*/
	gamma = (0.8 - ((m_vidSettingsMenu.brightness_slider.curValue * 0.1) - 0.5)) + 0.5;
	cgi.Cvar_SetValue ("vid_gamma",		gamma, true);

	cgi.Cvar_SetValue ("vid_gammapics",	m_vidSettingsMenu.gammapics_toggle.curValue, true);

	cgi.Cvar_SetValue ("gl_picmip",		3 - m_vidSettingsMenu.texqual_slider.curValue, true);
	cgi.Cvar_SetValue ("gl_finish",		m_vidSettingsMenu.finish_toggle.curValue, true);

	cgi.Cbuf_AddText ("vid_restart\n");
}


/*
================
VIDSettingsMenu_SetValues
================
*/
static void VIDSettingsMenu_SetValues ()
{
	cgi.Cvar_SetValue ("gl_mode",		clamp (cgi.Cvar_GetIntegerValue ("gl_mode"), 0, 12), true);
	if (cgi.Cvar_GetIntegerValue ("vid_width") && cgi.Cvar_GetIntegerValue ("vid_height"))
		m_vidSettingsMenu.mode_list.curValue	= 0;
	else
		m_vidSettingsMenu.mode_list.curValue	= cgi.Cvar_GetIntegerValue ("gl_mode")+1;

	if (!Q_stricmp (cgi.Cvar_GetStringValue ("gl_texturemode"), "GL_LINEAR_MIPMAP_LINEAR"))			m_vidSettingsMenu.tm_list.curValue = 1;
	else if (!Q_stricmp (cgi.Cvar_GetStringValue ("gl_texturemode"), "GL_LINEAR_MIPMAP_NEAREST"))	m_vidSettingsMenu.tm_list.curValue = 0;
	else																							m_vidSettingsMenu.tm_list.curValue = 2;

	if (cgi.Cvar_GetFloatValue ("gl_bitdepth") == 0)		m_vidSettingsMenu.bitd_list.curValue = 0;
	else if (cgi.Cvar_GetFloatValue ("gl_bitdepth") == 16)	m_vidSettingsMenu.bitd_list.curValue = 1;
	else if (cgi.Cvar_GetFloatValue ("gl_bitdepth") == 32)	m_vidSettingsMenu.bitd_list.curValue = 2;

	cgi.Cvar_SetValue ("gl_stencilbuffer",		clamp (cgi.Cvar_GetIntegerValue ("gl_stencilbuffer"), 0, 1), true);
	m_vidSettingsMenu.stencilbuf_list.curValue	= cgi.Cvar_GetIntegerValue ("gl_stencilbuffer");

	if (cgi.Cvar_GetFloatValue ("r_displayFreq") == 60)			m_vidSettingsMenu.dispfreq_list.curValue = 1;
	else if (cgi.Cvar_GetFloatValue ("r_displayFreq") == 65)	m_vidSettingsMenu.dispfreq_list.curValue = 2;
	else if (cgi.Cvar_GetFloatValue ("r_displayFreq") == 70)	m_vidSettingsMenu.dispfreq_list.curValue = 3;
	else if (cgi.Cvar_GetFloatValue ("r_displayFreq") == 75)	m_vidSettingsMenu.dispfreq_list.curValue = 4;
	else if (cgi.Cvar_GetFloatValue ("r_displayFreq") == 80)	m_vidSettingsMenu.dispfreq_list.curValue = 5;
	else if (cgi.Cvar_GetFloatValue ("r_displayFreq") == 85)	m_vidSettingsMenu.dispfreq_list.curValue = 6;
	else if (cgi.Cvar_GetFloatValue ("r_displayFreq") == 90)	m_vidSettingsMenu.dispfreq_list.curValue = 7;
	else if (cgi.Cvar_GetFloatValue ("r_displayFreq") == 95)	m_vidSettingsMenu.dispfreq_list.curValue = 8;
	else if (cgi.Cvar_GetFloatValue ("r_displayFreq") == 100)	m_vidSettingsMenu.dispfreq_list.curValue = 9;
	else if (cgi.Cvar_GetFloatValue ("r_displayFreq") == 105)	m_vidSettingsMenu.dispfreq_list.curValue = 10;
	else if (cgi.Cvar_GetFloatValue ("r_displayFreq") == 110)	m_vidSettingsMenu.dispfreq_list.curValue = 11;
	else if (cgi.Cvar_GetFloatValue ("r_displayFreq") == 115)	m_vidSettingsMenu.dispfreq_list.curValue = 12;
	else if (cgi.Cvar_GetFloatValue ("r_displayFreq") == 120)	m_vidSettingsMenu.dispfreq_list.curValue = 13;
	else														m_vidSettingsMenu.dispfreq_list.curValue = 0;

	cgi.Cvar_SetValue ("vid_fullscreen",			clamp (cgi.Cvar_GetIntegerValue ("vid_fullscreen"), 0, 1), true);
	m_vidSettingsMenu.fullscreen_toggle.curValue	= cgi.Cvar_GetIntegerValue ("vid_fullscreen");

	cgi.Cvar_SetValue ("viewsize",					clamp (cgi.Cvar_GetIntegerValue ("viewsize"), 40, 100), true);
	m_vidSettingsMenu.screensize_slider.curValue	= cgi.Cvar_GetIntegerValue ("viewsize") * 0.1; // kthx view menu?

	cgi.Cvar_SetValue ("vid_gamma",					clamp (cgi.Cvar_GetFloatValue ("vid_gamma"), 0.5, 1.3), true);
	m_vidSettingsMenu.brightness_slider.curValue	= (1.3 - cgi.Cvar_GetFloatValue ("vid_gamma") + 0.5) * 10;

	cgi.Cvar_SetValue ("vid_gammapics",			clamp (cgi.Cvar_GetIntegerValue ("vid_gammapics"), 0, 1), true);
	m_vidSettingsMenu.gammapics_toggle.curValue	= cgi.Cvar_GetIntegerValue ("vid_gammapics");

	m_vidSettingsMenu.texqual_slider.curValue		= 3 - cgi.Cvar_GetIntegerValue ("gl_picmip");

	cgi.Cvar_SetValue ("gl_finish",				clamp (cgi.Cvar_GetIntegerValue ("gl_finish"), 0, 1), true);
	m_vidSettingsMenu.finish_toggle.curValue	= cgi.Cvar_GetIntegerValue ("gl_finish");
}


/*
================
VIDSettingsMenu_Init
================
*/
static void VIDSettingsMenu_Init ()
{
	static char *resolutions[] = {
		// 4:3
		"[CUSTOM    ]",
		"[320 240   ]",
		"[400 300   ]",
		"[512 384   ]",
		"[640 480   ]",
		"[800 600   ]",
		"[960 720   ]",
		"[1024 768  ]",
		"[1152 864  ]",
		"[1280 960  ]",
		"[1600 1200 ]",
		"[1920 1440 ]",
		"[2048 1536 ]",

		// widescreen
		"[1280 800 (ws)]",
		"[1440 900 (ws)]",
		0
	};

	static char *refs[] = {
		"[default OpenGL]",
		"[3Dfx OpenGL   ]",
		"[PowerVR OpenGL]",
		0
	};

	static char *yesno_names[] = {
		"no",
		"yes",
		0
	};

	static char *tm_names[] = {
		"bilinear (fast)",
		"trilinear (best)",
		"other",
		0
	};

	static char *bitd_names[] = {
		"default",
		"16 bit",
		"32 bit",
		"other",
		0
	};

	static char *dispfreq_names[] = {
		"desktop",
		"60",
		"65",
		"70",
		"75",
		"80",
		"85",
		"90",
		"95",
		"100",
		"105",
		"110",
		"115",
		"120",
		"other",
		0
	};

	UI_StartFramework (&m_vidSettingsMenu.frameWork, FWF_CENTERHEIGHT);

	m_vidSettingsMenu.banner.generic.type		= UITYPE_IMAGE;
	m_vidSettingsMenu.banner.generic.flags		= UIF_NOSELECT|UIF_CENTERED;
	m_vidSettingsMenu.banner.generic.name		= NULL;
	m_vidSettingsMenu.banner.mat				= uiMedia.banners.video;

	m_vidSettingsMenu.header.generic.type		= UITYPE_ACTION;
	m_vidSettingsMenu.header.generic.flags		= UIF_NOSELECT|UIF_CENTERED|UIF_MEDIUM|UIF_SHADOW;
	m_vidSettingsMenu.header.generic.name		= "Video Settings";

	m_vidSettingsMenu.mode_list.generic.type		= UITYPE_SPINCONTROL;
	m_vidSettingsMenu.mode_list.generic.name		= "Resolution";
	m_vidSettingsMenu.mode_list.itemNames			= resolutions;
	m_vidSettingsMenu.mode_list.generic.statusBar	= "Resolution Selection";

	m_vidSettingsMenu.tm_list.generic.type		= UITYPE_SPINCONTROL;
	m_vidSettingsMenu.tm_list.generic.name		= "Texture Mode";
	m_vidSettingsMenu.tm_list.itemNames			= tm_names;
	m_vidSettingsMenu.tm_list.generic.statusBar	= "Texture mode";

	m_vidSettingsMenu.bitd_list.generic.type		= UITYPE_SPINCONTROL;
	m_vidSettingsMenu.bitd_list.generic.name		= "Bit Depth";
	m_vidSettingsMenu.bitd_list.itemNames			= bitd_names;
	m_vidSettingsMenu.bitd_list.generic.statusBar	= "Display bit depth";

	m_vidSettingsMenu.stencilbuf_list.generic.type		= UITYPE_SPINCONTROL;
	m_vidSettingsMenu.stencilbuf_list.generic.name		= "Stencil Buffer";
	m_vidSettingsMenu.stencilbuf_list.itemNames			= yesno_names;
	m_vidSettingsMenu.stencilbuf_list.generic.statusBar	= "Stencil buffer";

	m_vidSettingsMenu.dispfreq_list.generic.type		= UITYPE_SPINCONTROL;
	m_vidSettingsMenu.dispfreq_list.generic.name		= "Display Freq";
	m_vidSettingsMenu.dispfreq_list.itemNames			= dispfreq_names;
	m_vidSettingsMenu.dispfreq_list.generic.statusBar	= "Display frequency";

	m_vidSettingsMenu.fullscreen_toggle.generic.type		= UITYPE_SPINCONTROL;
	m_vidSettingsMenu.fullscreen_toggle.generic.name		= "Fullscreen";
	m_vidSettingsMenu.fullscreen_toggle.itemNames			= yesno_names;
	m_vidSettingsMenu.fullscreen_toggle.generic.statusBar	= "Fullscreen or Window";

	m_vidSettingsMenu.screensize_slider.generic.type		= UITYPE_SLIDER;
	m_vidSettingsMenu.screensize_slider.generic.name		= "Screen Size";
	m_vidSettingsMenu.screensize_slider.minValue			= 4;
	m_vidSettingsMenu.screensize_slider.maxValue			= 10;
	m_vidSettingsMenu.screensize_slider.generic.statusBar	= "Screen Size";

	m_vidSettingsMenu.brightness_slider.generic.type		= UITYPE_SLIDER;
	m_vidSettingsMenu.brightness_slider.generic.name		= "Brightness";
	m_vidSettingsMenu.brightness_slider.minValue			= 5;
	m_vidSettingsMenu.brightness_slider.maxValue			= 13;
	m_vidSettingsMenu.brightness_slider.generic.statusBar	= "Brightness";

	m_vidSettingsMenu.gammapics_toggle.generic.type			= UITYPE_SPINCONTROL;
	m_vidSettingsMenu.gammapics_toggle.generic.name			= "Pic Gamma";
	m_vidSettingsMenu.gammapics_toggle.itemNames			= yesno_names;
	m_vidSettingsMenu.gammapics_toggle.generic.statusBar	= "Apply Gamma to Pics";

	m_vidSettingsMenu.texqual_slider.generic.type		= UITYPE_SLIDER;
	m_vidSettingsMenu.texqual_slider.generic.name		= "Texture Quality";
	m_vidSettingsMenu.texqual_slider.minValue			= 0;
	m_vidSettingsMenu.texqual_slider.maxValue			= 3;
	m_vidSettingsMenu.texqual_slider.generic.statusBar	= "Texture upload quality";

	m_vidSettingsMenu.finish_toggle.generic.type		= UITYPE_SPINCONTROL;
	m_vidSettingsMenu.finish_toggle.generic.name		= "Frame Sync";
	m_vidSettingsMenu.finish_toggle.itemNames			= yesno_names;
	m_vidSettingsMenu.finish_toggle.generic.statusBar	= "Framerate sync (waits on gl/buf/net commands to complete after each frame)";

	m_vidSettingsMenu.apply_action.generic.type			= UITYPE_ACTION;
	m_vidSettingsMenu.apply_action.generic.flags		= UIF_CENTERED|UIF_MEDIUM|UIF_SHADOW;
	m_vidSettingsMenu.apply_action.generic.name			= "Apply";
	m_vidSettingsMenu.apply_action.generic.callBack		= VIDSettingsMenu_ApplyValues;
	m_vidSettingsMenu.apply_action.generic.statusBar	= "Apply Changes";

	m_vidSettingsMenu.reset_action.generic.type			= UITYPE_ACTION;
	m_vidSettingsMenu.reset_action.generic.flags		= UIF_CENTERED|UIF_MEDIUM|UIF_SHADOW;
	m_vidSettingsMenu.reset_action.generic.name			= "Reset";
	m_vidSettingsMenu.reset_action.generic.callBack		= ResetDefaults;
	m_vidSettingsMenu.reset_action.generic.statusBar	= "Reset Changes";

	m_vidSettingsMenu.cancel_action.generic.type		= UITYPE_ACTION;
	m_vidSettingsMenu.cancel_action.generic.flags		= UIF_CENTERED|UIF_MEDIUM|UIF_SHADOW;
	m_vidSettingsMenu.cancel_action.generic.name		= "Cancel";
	m_vidSettingsMenu.cancel_action.generic.callBack	= CancelChanges;
	m_vidSettingsMenu.cancel_action.generic.statusBar	= "Closes the Menu";

	VIDSettingsMenu_SetValues ();

	UI_AddItem (&m_vidSettingsMenu.frameWork,		&m_vidSettingsMenu.banner);
	UI_AddItem (&m_vidSettingsMenu.frameWork,		&m_vidSettingsMenu.header);

	UI_AddItem (&m_vidSettingsMenu.frameWork,		&m_vidSettingsMenu.mode_list);

	UI_AddItem (&m_vidSettingsMenu.frameWork,		&m_vidSettingsMenu.tm_list);
	UI_AddItem (&m_vidSettingsMenu.frameWork,		&m_vidSettingsMenu.bitd_list);
	UI_AddItem (&m_vidSettingsMenu.frameWork,		&m_vidSettingsMenu.stencilbuf_list);
	UI_AddItem (&m_vidSettingsMenu.frameWork,		&m_vidSettingsMenu.dispfreq_list);

	UI_AddItem (&m_vidSettingsMenu.frameWork,		&m_vidSettingsMenu.fullscreen_toggle);
	UI_AddItem (&m_vidSettingsMenu.frameWork,		&m_vidSettingsMenu.screensize_slider);
	UI_AddItem (&m_vidSettingsMenu.frameWork,		&m_vidSettingsMenu.brightness_slider);
	UI_AddItem (&m_vidSettingsMenu.frameWork,		&m_vidSettingsMenu.gammapics_toggle);
	UI_AddItem (&m_vidSettingsMenu.frameWork,		&m_vidSettingsMenu.texqual_slider);
	UI_AddItem (&m_vidSettingsMenu.frameWork,		&m_vidSettingsMenu.finish_toggle);

	UI_AddItem (&m_vidSettingsMenu.frameWork,		&m_vidSettingsMenu.apply_action);
	UI_AddItem (&m_vidSettingsMenu.frameWork,		&m_vidSettingsMenu.reset_action);
	UI_AddItem (&m_vidSettingsMenu.frameWork,		&m_vidSettingsMenu.cancel_action);

	UI_FinishFramework (&m_vidSettingsMenu.frameWork, true);
}


/*
=============
VIDSettingsMenu_Close
=============
*/
static struct sfx_t *VIDSettingsMenu_Close ()
{
	return uiMedia.sounds.menuOut;
}


/*
================
VIDSettingsMenu_Draw
================
*/
static void VIDSettingsMenu_Draw ()
{
	float	y;

	// Initialize if necessary
	if (!m_vidSettingsMenu.frameWork.initialized)
		VIDSettingsMenu_Init ();

	// Dynamically position
	m_vidSettingsMenu.frameWork.x			= cg.refConfig.vidWidth * 0.5f;
	m_vidSettingsMenu.frameWork.y			= 0;

	m_vidSettingsMenu.banner.generic.x			= 0;
	m_vidSettingsMenu.banner.generic.y			= 0;

	y = m_vidSettingsMenu.banner.height * UI_SCALE;

	m_vidSettingsMenu.header.generic.x				= 0;
	m_vidSettingsMenu.header.generic.y				= y += UIFT_SIZEINC;
	m_vidSettingsMenu.mode_list.generic.x			= 0;
	m_vidSettingsMenu.mode_list.generic.y			= y += UIFT_SIZEINC;
	m_vidSettingsMenu.tm_list.generic.x				= 0;
	m_vidSettingsMenu.tm_list.generic.y				= y += UIFT_SIZEINC*2;
	m_vidSettingsMenu.bitd_list.generic.x			= 0;
	m_vidSettingsMenu.bitd_list.generic.y			= y += UIFT_SIZEINC;
	m_vidSettingsMenu.stencilbuf_list.generic.x		= 0;
	m_vidSettingsMenu.stencilbuf_list.generic.y		= y += UIFT_SIZEINC;
	m_vidSettingsMenu.dispfreq_list.generic.x		= 0;
	m_vidSettingsMenu.dispfreq_list.generic.y		= y += UIFT_SIZEINC;
	m_vidSettingsMenu.fullscreen_toggle.generic.x	= 0;
	m_vidSettingsMenu.fullscreen_toggle.generic.y	= y += (UIFT_SIZEINC * 2);
	m_vidSettingsMenu.screensize_slider.generic.x	= 0;
	m_vidSettingsMenu.screensize_slider.generic.y	= y += UIFT_SIZEINC;
	m_vidSettingsMenu.brightness_slider.generic.x	= 0;
	m_vidSettingsMenu.brightness_slider.generic.y	= y += UIFT_SIZEINC;
	m_vidSettingsMenu.gammapics_toggle.generic.x	= 0;
	m_vidSettingsMenu.gammapics_toggle.generic.y	= y += UIFT_SIZEINC;
	m_vidSettingsMenu.texqual_slider.generic.x		= 0;
	m_vidSettingsMenu.texqual_slider.generic.y		= y += (UIFT_SIZEINC*2);
	m_vidSettingsMenu.finish_toggle.generic.x		= 0;
	m_vidSettingsMenu.finish_toggle.generic.y		= y += (UIFT_SIZEINC*2);
	m_vidSettingsMenu.apply_action.generic.x		= 0;
	m_vidSettingsMenu.apply_action.generic.y		= y += UIFT_SIZEINCMED*2;
	m_vidSettingsMenu.reset_action.generic.x		= 0;
	m_vidSettingsMenu.reset_action.generic.y		= y += UIFT_SIZEINCMED;
	m_vidSettingsMenu.cancel_action.generic.x		= 0;
	m_vidSettingsMenu.cancel_action.generic.y		= y += UIFT_SIZEINCMED;

	// Render
	UI_DrawInterface (&m_vidSettingsMenu.frameWork);
}


/*
=============
UI_VIDSettingsMenu_f
=============
*/
void UI_VIDSettingsMenu_f ()
{
	VIDSettingsMenu_Init ();
	M_PushMenu (&m_vidSettingsMenu.frameWork, VIDSettingsMenu_Draw, VIDSettingsMenu_Close, M_KeyHandler);
}
