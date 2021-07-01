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
// m_opts_screen.c
//

#include "m_local.h"

/*
=======================================================================

	SCREEN MENU

=======================================================================
*/

struct crosshairInfo_t {
	int						number;
	struct refMaterial_t	*mat;
};

struct m_screenMenu_t {
	// Menu items
	uiFrameWork_t	frameWork;

	uiImage_t		banner;

	//
	// console
	//

	uiAction_t		con_header;

	uiList_t		con_clock_toggle;
	uiList_t		con_notfade_toggle;
	uiList_t		con_notlarge_toggle;
	uiList_t		con_timestamp_list;

	uiSlider_t		con_notlines_slider;
	uiAction_t		con_notlines_amount;
	uiSlider_t		con_alpha_slider;
	uiAction_t		con_alpha_amount;
	uiSlider_t		con_drop_slider;
	uiAction_t		con_drop_amount;
	uiSlider_t		con_scroll_slider;
	uiAction_t		con_scroll_amount;
	uiSlider_t		con_fontscale_slider;
	uiAction_t		con_fontscale_amount;

	//
	// crosshair
	//

	uiAction_t		ch_header;

	uiList_Better_t	ch_number_list;
	uiImage_t		ch_image;

	uiSlider_t		ch_alpha_slider;
	uiAction_t		ch_alpha_amount;
	uiSlider_t		ch_pulse_slider;
	uiAction_t		ch_pulse_amount;
	uiSlider_t		ch_scale_slider;
	uiAction_t		ch_scale_amount;

	uiSlider_t		ch_red_slider;
	uiAction_t		ch_red_amount;
	uiSlider_t		ch_green_slider;
	uiAction_t		ch_green_amount;
	uiSlider_t		ch_blue_slider;
	uiAction_t		ch_blue_amount;

	uiAction_t		back_action;

	// crosshair information
	bool			crosshairsFound;

	TList<crosshairInfo_t>	crosshairs;
	TList<String>			crosshairNames;
};

static m_screenMenu_t	m_screenMenu;

//
// console
//

static void ConsoleClockFunc (void *unused)
{
	cgi.Cvar_SetValue ("con_clock", m_screenMenu.con_clock_toggle.curValue, false);
}

static void ConsoleNotFadeFunc (void *unused)
{
	cgi.Cvar_SetValue ("con_notifyfade", m_screenMenu.con_notfade_toggle.curValue, false);
}

static void ConsoleNotLargeFunc (void *unused)
{
	cgi.Cvar_SetValue ("con_notifylarge", m_screenMenu.con_notlarge_toggle.curValue, false);
}

static void ConsoleTimeStampFunc (void *unused)
{
	cgi.Cvar_SetValue ("cl_timestamp", m_screenMenu.con_timestamp_list.curValue, false);
}

static void ConsoleNotLinesFunc (void *unused)
{
	cgi.Cvar_SetValue ("con_notifylines", m_screenMenu.con_notlines_slider.curValue, false);
	m_screenMenu.con_notlines_amount.generic.name = cgi.Cvar_GetStringValue ("con_notifylines");
}

static void ConsoleAlphaFunc (void *unused)
{
	cgi.Cvar_SetValue ("con_alpha", m_screenMenu.con_alpha_slider.curValue * 0.1, false);
	m_screenMenu.con_alpha_amount.generic.name = cgi.Cvar_GetStringValue ("con_alpha");
}

static void ConsoleDropFunc (void *unused)
{
	cgi.Cvar_SetValue ("con_drop", m_screenMenu.con_drop_slider.curValue * 0.1, false);
	m_screenMenu.con_drop_amount.generic.name = cgi.Cvar_GetStringValue ("con_drop");
}

static void ConsoleScrollFunc (void *unused)
{
	cgi.Cvar_SetValue ("con_scroll", m_screenMenu.con_scroll_slider.curValue, false);
	m_screenMenu.con_scroll_amount.generic.name = cgi.Cvar_GetStringValue ("con_scroll");
}

static void ConsoleFontScaleFunc (void *unused)
{
	cgi.Cvar_SetValue ("r_fontScale", m_screenMenu.con_fontscale_slider.curValue * 0.25, false);
	m_screenMenu.con_fontscale_amount.generic.name = cgi.Cvar_GetStringValue ("r_fontScale");
}

//
// crosshair
//

static void CrosshairFunc (void *unused)
{
	if (m_screenMenu.ch_number_list.curValue < (int)m_screenMenu.crosshairs.Count())
		cgi.Cvar_SetValue ("crosshair", m_screenMenu.crosshairs[m_screenMenu.ch_number_list.curValue].number, false);
}

static void CrosshairAlphaFunc (void *unused)
{
	cgi.Cvar_SetValue ("ch_alpha", m_screenMenu.ch_alpha_slider.curValue * 0.1, false);
	m_screenMenu.ch_alpha_amount.generic.name = cgi.Cvar_GetStringValue ("ch_alpha");
}

static void CrosshairPulseFunc (void *unused)
{
	cgi.Cvar_SetValue ("ch_pulse", m_screenMenu.ch_pulse_slider.curValue * 0.1, false);
	m_screenMenu.ch_pulse_amount.generic.name = cgi.Cvar_GetStringValue ("ch_pulse");
}

static void CrosshairScaleFunc (void *unused)
{
	cgi.Cvar_SetValue ("ch_scale", m_screenMenu.ch_scale_slider.curValue * 0.1, false);
	m_screenMenu.ch_scale_amount.generic.name = cgi.Cvar_GetStringValue ("ch_scale");
}

static void CrosshairRedFunc (void *unused)
{
	cgi.Cvar_SetValue ("ch_red", m_screenMenu.ch_red_slider.curValue * 0.1, false);
	m_screenMenu.ch_red_amount.generic.name = cgi.Cvar_GetStringValue ("ch_red");
}

static void CrosshairGreenFunc (void *unused)
{
	cgi.Cvar_SetValue ("ch_green", m_screenMenu.ch_green_slider.curValue * 0.1, false);
	m_screenMenu.ch_green_amount.generic.name = cgi.Cvar_GetStringValue ("ch_green");
}

static void CrosshairBlueFunc (void *unused)
{
	cgi.Cvar_SetValue ("ch_blue", m_screenMenu.ch_blue_slider.curValue * 0.1, false);
	m_screenMenu.ch_blue_amount.generic.name = cgi.Cvar_GetStringValue ("ch_blue");
}


/*
=============
ScreenMenu_SetValues
=============
*/
static void ScreenMenu_SetValues ()
{
	//
	// console
	//

	cgi.Cvar_SetValue ("con_clock",					clamp (cgi.Cvar_GetIntegerValue ("con_clock"), 0, 1), false);
	m_screenMenu.con_clock_toggle.curValue			= cgi.Cvar_GetIntegerValue ("con_clock");

	cgi.Cvar_SetValue ("con_notifyfade",			clamp (cgi.Cvar_GetIntegerValue ("con_notifyfade"), 0, 1), false);
	m_screenMenu.con_notfade_toggle.curValue		= cgi.Cvar_GetIntegerValue ("con_notifyfade");

	cgi.Cvar_SetValue ("con_notifylarge",			clamp (cgi.Cvar_GetIntegerValue ("con_notifylarge"), 0, 1), false);
	m_screenMenu.con_notlarge_toggle.curValue		= cgi.Cvar_GetIntegerValue ("con_notifylarge");

	cgi.Cvar_SetValue ("cl_timestamp",				clamp (cgi.Cvar_GetIntegerValue ("cl_timestamp"), 0, 2), false);
	m_screenMenu.con_timestamp_list.curValue		= cgi.Cvar_GetIntegerValue ("cl_timestamp");

	m_screenMenu.con_notlines_slider.curValue		= cgi.Cvar_GetFloatValue ("con_notifylines");
	m_screenMenu.con_notlines_amount.generic.name	= cgi.Cvar_GetStringValue ("con_notifylines");
	m_screenMenu.con_alpha_slider.curValue			= cgi.Cvar_GetFloatValue ("con_alpha") * 10;
	m_screenMenu.con_alpha_amount.generic.name		= cgi.Cvar_GetStringValue ("con_alpha");
	m_screenMenu.con_drop_slider.curValue			= cgi.Cvar_GetFloatValue ("con_drop") * 10;
	m_screenMenu.con_drop_amount.generic.name		= cgi.Cvar_GetStringValue ("con_drop");
	m_screenMenu.con_scroll_slider.curValue			= cgi.Cvar_GetFloatValue ("con_scroll");
	m_screenMenu.con_scroll_amount.generic.name		= cgi.Cvar_GetStringValue ("con_scroll");

	m_screenMenu.con_fontscale_slider.curValue		= cgi.Cvar_GetFloatValue ("r_fontScale") * 4;
	m_screenMenu.con_fontscale_amount.generic.name	= cgi.Cvar_GetStringValue ("r_fontScale");

	//
	// crosshair
	//

	m_screenMenu.ch_number_list.curValue		= cgi.Cvar_GetFloatValue ("crosshair");
	int i;
	for (i=0 ; i<(int)m_screenMenu.crosshairs.Count() ; i++) {
		if (m_screenMenu.crosshairs[i].number != m_screenMenu.ch_number_list.curValue)
			continue;
		break;
	}
	m_screenMenu.ch_number_list.curValue = (i == m_screenMenu.crosshairs.Count()) ? 0 : i;
	if (m_screenMenu.crosshairs.Count())
		cgi.Cvar_SetValue ("crosshair", m_screenMenu.crosshairs[m_screenMenu.ch_number_list.curValue].number, true);
	else
		cgi.Cvar_SetValue ("crosshair", 0, true);

	m_screenMenu.ch_alpha_slider.curValue		= cgi.Cvar_GetFloatValue ("ch_alpha") * 10;
	m_screenMenu.ch_alpha_amount.generic.name	= cgi.Cvar_GetStringValue ("ch_alpha");
	m_screenMenu.ch_pulse_slider.curValue		= cgi.Cvar_GetFloatValue ("ch_pulse") * 10;
	m_screenMenu.ch_pulse_amount.generic.name	= cgi.Cvar_GetStringValue ("ch_pulse");
	m_screenMenu.ch_scale_slider.curValue		= cgi.Cvar_GetFloatValue ("ch_scale")* 10;
	m_screenMenu.ch_scale_amount.generic.name	= cgi.Cvar_GetStringValue ("ch_scale");

	m_screenMenu.ch_red_slider.curValue			= cgi.Cvar_GetFloatValue ("ch_red") * 10;
	m_screenMenu.ch_red_amount.generic.name		= cgi.Cvar_GetStringValue ("ch_red");
	m_screenMenu.ch_green_slider.curValue		= cgi.Cvar_GetFloatValue ("ch_green") * 10;
	m_screenMenu.ch_green_amount.generic.name	= cgi.Cvar_GetStringValue ("ch_green");
	m_screenMenu.ch_blue_slider.curValue		= cgi.Cvar_GetFloatValue ("ch_blue") * 10;
	m_screenMenu.ch_blue_amount.generic.name	= cgi.Cvar_GetStringValue ("ch_blue");
}


/*
=============
ScreenMenu_Init
=============
*/
static int stricmpSort (const String &a, const String &b)
{
/*	char	ch1[MAX_QPATH];
	char	ch2[MAX_QPATH];
	int		chNum1;
	int		chNum2;
	char	*p;

	// crop off after "ch"
	Q_strncpyz (ch1, a.CString(), sizeof(ch1));
	p = strstr (ch1, "ch");
	if (p) {
		p += 2;
		*p = '\0';
	}

	// crop off after "ch"
	Q_strncpyz (ch2, b.CString(), sizeof(ch2));
	p = strstr (ch2, "ch");
	if (p) {
		p += 2;
		*p = '\0';
	}

	// these should be IDENTICAL
	if (Q_stricmp (ch1, ch2))
		return -1;

	// skip "pics/ch"
	Q_strncpyz (ch1, a.CString()+strlen(ch1), sizeof(ch1));
	Q_strncpyz (ch2, b.CString()+strlen(ch2), sizeof(ch2));

	// crop off extensions
	p = strchr (ch1, '.');
	if (p)
		*p = '\0';
	p = strchr (ch2, '.');
	if (p)
		*p = '\0';

	// get and compare integer values
	chNum1 = atoi (ch1);
	chNum2 = atoi (ch2);

	return chNum1 - chNum2;*/
	int ch1 = a.IndexOf("ch");
	String st1 = a.Substring(0, ch1 + 2);

	int ch2 = b.IndexOf("ch");
	String st2 = b.Substring(0, ch2 + 2);

	if (st1 != st2)
		return -1;

	int chNum1 = atoi(a.Substring(ch1 + 2).CString());
	int chNum2 = atoi(b.Substring(ch2 + 2).CString());

	return chNum1 - chNum2;
}

static void ScreenMenu_Init ()
{
	static char *onoff_names[] = {
		"off",
		"on",
		0
	};

	static char *timestamp_names[] = {
		"off",
		"messages only",
		"all messages",
		0
	};

	int		i, j;

	// get crosshair list
	var crosshairList = cgi.FS_FindFiles ("pics", "pics/ch*.*", NULL, false, false);
	if (!crosshairList.Count())
		m_screenMenu.crosshairsFound = false;
	else
		m_screenMenu.crosshairsFound = true;

	if (m_screenMenu.crosshairsFound) {
		// sort
		crosshairList.Sort(stricmpSort);

		for (i=0 ; i<(int)crosshairList.Count() ; i++) {
			// skip duplicates
			for (j=i+1 ; j<(int)crosshairList.Count() ; j++) {
				if (crosshairList[i].Count() != crosshairList[j].Count())
					continue;
				if (!crosshairList[i].CompareCaseInsensitive(crosshairList[j], crosshairList[i].Count()-3))
					break;
			}
			if (j != (int)crosshairList.Count())
				continue;

			// skip "pics/ch"
			String p = Path::GetFileNameWithoutExtension(crosshairList[i]);
			if (!p.StartsWith("ch"))
				continue;

			// find the 'num'
			int f = atoi (p.Substring(2).CString());
			if (!f)
				continue;

			// add to list
			m_screenMenu.crosshairs.Add(crosshairInfo_t());
			m_screenMenu.crosshairs[m_screenMenu.crosshairs.Count()-1].number = f;
			m_screenMenu.crosshairs[m_screenMenu.crosshairs.Count()-1].mat = cgi.R_RegisterPic (crosshairList[i].CString());
			m_screenMenu.crosshairNames.Add(crosshairList[i]);
		}

		if (!m_screenMenu.crosshairs.Count())
			m_screenMenu.crosshairsFound = false;
	}

	// create menu
	UI_StartFramework (&m_screenMenu.frameWork, FWF_CENTERHEIGHT);

	m_screenMenu.banner.generic.type		= UITYPE_IMAGE;
	m_screenMenu.banner.generic.flags		= UIF_NOSELECT|UIF_CENTERED;
	m_screenMenu.banner.generic.name		= NULL;
	m_screenMenu.banner.mat					= uiMedia.banners.options;

	//
	// console
	//

	m_screenMenu.con_header.generic.type		= UITYPE_ACTION;
	m_screenMenu.con_header.generic.flags		= UIF_NOSELECT|UIF_CENTERED|UIF_MEDIUM|UIF_SHADOW;
	m_screenMenu.con_header.generic.name		= "Console";

	m_screenMenu.con_clock_toggle.generic.type		= UITYPE_SPINCONTROL;
	m_screenMenu.con_clock_toggle.generic.name		= "Console clock";
	m_screenMenu.con_clock_toggle.generic.callBack	= ConsoleClockFunc;
	m_screenMenu.con_clock_toggle.itemNames			= onoff_names;
	m_screenMenu.con_clock_toggle.generic.statusBar	= "Console Clock";

	m_screenMenu.con_notfade_toggle.generic.type		= UITYPE_SPINCONTROL;
	m_screenMenu.con_notfade_toggle.generic.name		= "Notify fading";
	m_screenMenu.con_notfade_toggle.generic.callBack	= ConsoleNotFadeFunc;
	m_screenMenu.con_notfade_toggle.itemNames			= onoff_names;
	m_screenMenu.con_notfade_toggle.generic.statusBar	= "Notifyline Fading";

	m_screenMenu.con_notlarge_toggle.generic.type		= UITYPE_SPINCONTROL;
	m_screenMenu.con_notlarge_toggle.generic.name		= "Large notify";
	m_screenMenu.con_notlarge_toggle.generic.callBack	= ConsoleNotLargeFunc;
	m_screenMenu.con_notlarge_toggle.itemNames			= onoff_names;
	m_screenMenu.con_notlarge_toggle.generic.statusBar	= "Larger notify lines";

	m_screenMenu.con_timestamp_list.generic.type		= UITYPE_SPINCONTROL;
	m_screenMenu.con_timestamp_list.generic.name		= "Timestamping";
	m_screenMenu.con_timestamp_list.generic.callBack	= ConsoleTimeStampFunc;
	m_screenMenu.con_timestamp_list.itemNames			= timestamp_names;
	m_screenMenu.con_timestamp_list.generic.statusBar	= "Message Timestamping";

	m_screenMenu.con_notlines_slider.generic.type		= UITYPE_SLIDER;
	m_screenMenu.con_notlines_slider.generic.name		= "Notify lines";
	m_screenMenu.con_notlines_slider.generic.callBack	= ConsoleNotLinesFunc;
	m_screenMenu.con_notlines_slider.minValue			= 0;
	m_screenMenu.con_notlines_slider.maxValue			= 10;
	m_screenMenu.con_notlines_slider.generic.statusBar	= "Maximum Notifylines";
	m_screenMenu.con_notlines_amount.generic.type		= UITYPE_ACTION;
	m_screenMenu.con_notlines_amount.generic.flags		= UIF_LEFT_JUSTIFY|UIF_NOSELECT;

	m_screenMenu.con_alpha_slider.generic.type		= UITYPE_SLIDER;
	m_screenMenu.con_alpha_slider.generic.name		= "Console alpha";
	m_screenMenu.con_alpha_slider.generic.callBack	= ConsoleAlphaFunc;
	m_screenMenu.con_alpha_slider.minValue			= 0;
	m_screenMenu.con_alpha_slider.maxValue			= 10;
	m_screenMenu.con_alpha_slider.generic.statusBar	= "Console Alpha";
	m_screenMenu.con_alpha_amount.generic.type		= UITYPE_ACTION;
	m_screenMenu.con_alpha_amount.generic.flags		= UIF_LEFT_JUSTIFY|UIF_NOSELECT;

	m_screenMenu.con_drop_slider.generic.type		= UITYPE_SLIDER;
	m_screenMenu.con_drop_slider.generic.name		= "Drop height";
	m_screenMenu.con_drop_slider.generic.callBack	= ConsoleDropFunc;
	m_screenMenu.con_drop_slider.minValue			= 0;
	m_screenMenu.con_drop_slider.maxValue			= 10;
	m_screenMenu.con_drop_slider.generic.statusBar	= "Console Drop Height";
	m_screenMenu.con_drop_amount.generic.type		= UITYPE_ACTION;
	m_screenMenu.con_drop_amount.generic.flags		= UIF_LEFT_JUSTIFY|UIF_NOSELECT;

	m_screenMenu.con_scroll_slider.generic.type		= UITYPE_SLIDER;
	m_screenMenu.con_scroll_slider.generic.name		= "Scroll lines";
	m_screenMenu.con_scroll_slider.generic.callBack	= ConsoleScrollFunc;
	m_screenMenu.con_scroll_slider.minValue			= 0;
	m_screenMenu.con_scroll_slider.maxValue			= 10;
	m_screenMenu.con_scroll_slider.generic.statusBar= "Scroll console lines with PGUP/DN or MWHEELUP/DN";
	m_screenMenu.con_scroll_amount.generic.type		= UITYPE_ACTION;
	m_screenMenu.con_scroll_amount.generic.flags	= UIF_LEFT_JUSTIFY|UIF_NOSELECT;

	m_screenMenu.con_fontscale_slider.generic.type		= UITYPE_SLIDER;
	m_screenMenu.con_fontscale_slider.generic.name		= "Font scale";
	m_screenMenu.con_fontscale_slider.generic.callBack	= ConsoleFontScaleFunc;
	m_screenMenu.con_fontscale_slider.minValue			= 1;
	m_screenMenu.con_fontscale_slider.maxValue			= 12;
	m_screenMenu.con_fontscale_slider.generic.statusBar	= "Font size scaling";
	m_screenMenu.con_fontscale_amount.generic.type		= UITYPE_ACTION;
	m_screenMenu.con_fontscale_amount.generic.flags		= UIF_LEFT_JUSTIFY|UIF_NOSELECT;

	//
	// crosshair
	//

	m_screenMenu.ch_header.generic.type		= UITYPE_ACTION;
	m_screenMenu.ch_header.generic.flags	= UIF_NOSELECT|UIF_CENTERED|UIF_MEDIUM|UIF_SHADOW;
	m_screenMenu.ch_header.generic.name		= "Crosshair";

	if (m_screenMenu.crosshairsFound) {
		m_screenMenu.ch_number_list.generic.type		= UITYPE_SPINCONTROL_BETTER;
		m_screenMenu.ch_number_list.generic.name		= "Crosshair";
		m_screenMenu.ch_number_list.generic.callBack	= CrosshairFunc;
		m_screenMenu.ch_number_list.itemNames			= m_screenMenu.crosshairNames;
		m_screenMenu.ch_number_list.generic.statusBar	= "Crosshair Number";

		m_screenMenu.ch_image.generic.type				= UITYPE_IMAGE;
		m_screenMenu.ch_image.generic.flags				= UIF_NOSELECT|UIF_NOSELBAR;
		m_screenMenu.ch_image.mat						= NULL;
	}

	m_screenMenu.ch_alpha_slider.generic.type		= UITYPE_SLIDER;
	m_screenMenu.ch_alpha_slider.generic.name		= "Alpha";
	m_screenMenu.ch_alpha_slider.generic.callBack	= CrosshairAlphaFunc;
	m_screenMenu.ch_alpha_slider.minValue			= 0;
	m_screenMenu.ch_alpha_slider.maxValue			= 10;
	m_screenMenu.ch_alpha_slider.generic.statusBar	= "Crosshair Alpha";
	m_screenMenu.ch_alpha_amount.generic.type		= UITYPE_ACTION;
	m_screenMenu.ch_alpha_amount.generic.flags		= UIF_LEFT_JUSTIFY|UIF_NOSELECT;

	m_screenMenu.ch_pulse_slider.generic.type		= UITYPE_SLIDER;
	m_screenMenu.ch_pulse_slider.generic.name		= "Pulse";
	m_screenMenu.ch_pulse_slider.generic.callBack	= CrosshairPulseFunc;
	m_screenMenu.ch_pulse_slider.minValue			= 0;
	m_screenMenu.ch_pulse_slider.maxValue			= 20;
	m_screenMenu.ch_pulse_slider.generic.statusBar	= "Crosshair Alpha Pulsing";
	m_screenMenu.ch_pulse_amount.generic.type		= UITYPE_ACTION;
	m_screenMenu.ch_pulse_amount.generic.flags		= UIF_LEFT_JUSTIFY|UIF_NOSELECT;

	m_screenMenu.ch_scale_slider.generic.type		= UITYPE_SLIDER;
	m_screenMenu.ch_scale_slider.generic.name		= "Scale";
	m_screenMenu.ch_scale_slider.generic.callBack	= CrosshairScaleFunc;
	m_screenMenu.ch_scale_slider.minValue			= -10;
	m_screenMenu.ch_scale_slider.maxValue			= 20;
	m_screenMenu.ch_scale_slider.generic.statusBar	= "Crosshair Scale";
	m_screenMenu.ch_scale_amount.generic.type		= UITYPE_ACTION;
	m_screenMenu.ch_scale_amount.generic.flags		= UIF_LEFT_JUSTIFY|UIF_NOSELECT;

	m_screenMenu.ch_red_slider.generic.type			= UITYPE_SLIDER;
	m_screenMenu.ch_red_slider.generic.name			= "Red color";
	m_screenMenu.ch_red_slider.generic.callBack		= CrosshairRedFunc;
	m_screenMenu.ch_red_slider.minValue				= 0;
	m_screenMenu.ch_red_slider.maxValue				= 10;
	m_screenMenu.ch_red_slider.generic.statusBar	= "Crosshair Red Amount";
	m_screenMenu.ch_red_amount.generic.type			= UITYPE_ACTION;
	m_screenMenu.ch_red_amount.generic.flags		= UIF_LEFT_JUSTIFY|UIF_NOSELECT;

	m_screenMenu.ch_green_slider.generic.type		= UITYPE_SLIDER;
	m_screenMenu.ch_green_slider.generic.name		= "Green color";
	m_screenMenu.ch_green_slider.generic.callBack	= CrosshairGreenFunc;
	m_screenMenu.ch_green_slider.minValue			= 0;
	m_screenMenu.ch_green_slider.maxValue			= 10;
	m_screenMenu.ch_green_slider.generic.statusBar	= "Crosshair Green Amount";
	m_screenMenu.ch_green_amount.generic.type		= UITYPE_ACTION;
	m_screenMenu.ch_green_amount.generic.flags		= UIF_LEFT_JUSTIFY|UIF_NOSELECT;

	m_screenMenu.ch_blue_slider.generic.type		= UITYPE_SLIDER;
	m_screenMenu.ch_blue_slider.generic.name		= "Blue color";
	m_screenMenu.ch_blue_slider.generic.callBack	= CrosshairBlueFunc;
	m_screenMenu.ch_blue_slider.minValue			= 0;
	m_screenMenu.ch_blue_slider.maxValue			= 10;
	m_screenMenu.ch_blue_slider.generic.statusBar	= "Crosshair Blue Amount";
	m_screenMenu.ch_blue_amount.generic.type		= UITYPE_ACTION;
	m_screenMenu.ch_blue_amount.generic.flags		= UIF_LEFT_JUSTIFY|UIF_NOSELECT;

	m_screenMenu.back_action.generic.type		= UITYPE_ACTION;
	m_screenMenu.back_action.generic.flags		= UIF_CENTERED|UIF_LARGE|UIF_SHADOW;
	m_screenMenu.back_action.generic.name		= "< Back";
	m_screenMenu.back_action.generic.callBack	= Menu_Pop;
	m_screenMenu.back_action.generic.statusBar	= "Back a menu";

	ScreenMenu_SetValues ();

	UI_AddItem (&m_screenMenu.frameWork,		&m_screenMenu.banner);

	//
	// console
	//

	UI_AddItem (&m_screenMenu.frameWork,		&m_screenMenu.con_header);

	UI_AddItem (&m_screenMenu.frameWork,		&m_screenMenu.con_clock_toggle);
	UI_AddItem (&m_screenMenu.frameWork,		&m_screenMenu.con_notfade_toggle);
	UI_AddItem (&m_screenMenu.frameWork,		&m_screenMenu.con_notlarge_toggle);
	UI_AddItem (&m_screenMenu.frameWork,		&m_screenMenu.con_timestamp_list);

	UI_AddItem (&m_screenMenu.frameWork,		&m_screenMenu.con_notlines_slider);
	UI_AddItem (&m_screenMenu.frameWork,		&m_screenMenu.con_notlines_amount);
	UI_AddItem (&m_screenMenu.frameWork,		&m_screenMenu.con_alpha_slider);
	UI_AddItem (&m_screenMenu.frameWork,		&m_screenMenu.con_alpha_amount);
	UI_AddItem (&m_screenMenu.frameWork,		&m_screenMenu.con_drop_slider);
	UI_AddItem (&m_screenMenu.frameWork,		&m_screenMenu.con_drop_amount);
	UI_AddItem (&m_screenMenu.frameWork,		&m_screenMenu.con_scroll_slider);
	UI_AddItem (&m_screenMenu.frameWork,		&m_screenMenu.con_scroll_amount);
	UI_AddItem (&m_screenMenu.frameWork,		&m_screenMenu.con_fontscale_slider);
	UI_AddItem (&m_screenMenu.frameWork,		&m_screenMenu.con_fontscale_amount);

	//
	// crosshair
	//

	UI_AddItem (&m_screenMenu.frameWork,		&m_screenMenu.ch_header);

	if (m_screenMenu.crosshairsFound) {
		UI_AddItem (&m_screenMenu.frameWork,	&m_screenMenu.ch_number_list);
		UI_AddItem (&m_screenMenu.frameWork,	&m_screenMenu.ch_image);
	}

	UI_AddItem (&m_screenMenu.frameWork,		&m_screenMenu.ch_alpha_slider);
	UI_AddItem (&m_screenMenu.frameWork,		&m_screenMenu.ch_alpha_amount);
	UI_AddItem (&m_screenMenu.frameWork,		&m_screenMenu.ch_pulse_slider);
	UI_AddItem (&m_screenMenu.frameWork,		&m_screenMenu.ch_pulse_amount);
	UI_AddItem (&m_screenMenu.frameWork,		&m_screenMenu.ch_scale_slider);
	UI_AddItem (&m_screenMenu.frameWork,		&m_screenMenu.ch_scale_amount);

	UI_AddItem (&m_screenMenu.frameWork,		&m_screenMenu.ch_red_slider);
	UI_AddItem (&m_screenMenu.frameWork,		&m_screenMenu.ch_red_amount);
	UI_AddItem (&m_screenMenu.frameWork,		&m_screenMenu.ch_green_slider);
	UI_AddItem (&m_screenMenu.frameWork,		&m_screenMenu.ch_green_amount);
	UI_AddItem (&m_screenMenu.frameWork,		&m_screenMenu.ch_blue_slider);
	UI_AddItem (&m_screenMenu.frameWork,		&m_screenMenu.ch_blue_amount);

	UI_AddItem (&m_screenMenu.frameWork,		&m_screenMenu.back_action);

	UI_FinishFramework (&m_screenMenu.frameWork, true);
}


/*
=============
ScreenMenu_Close
=============
*/
static struct sfx_t *ScreenMenu_Close ()
{
	return uiMedia.sounds.menuOut;
}


/*
=============
ScreenMenu_Draw
=============
*/
static void ScreenMenu_Draw ()
{
	struct refMaterial_t	*chMat;
	int						width, height;
	float					y;

	// Initialize if necessary
	if (!m_screenMenu.frameWork.initialized)
		ScreenMenu_Init ();

	// Dynamically position
	m_screenMenu.frameWork.x				= cg.refConfig.vidWidth * 0.5f;
	m_screenMenu.frameWork.y				= 0;

	m_screenMenu.banner.generic.x			= 0;
	m_screenMenu.banner.generic.y			= 0;

	y = m_screenMenu.banner.height * UI_SCALE;

	m_screenMenu.con_header.generic.x			= 0;
	m_screenMenu.con_header.generic.y			= y += UIFT_SIZEINC;
	m_screenMenu.con_clock_toggle.generic.x		= 0;
	m_screenMenu.con_clock_toggle.generic.y		= y += UIFT_SIZEINCMED + UIFT_SIZEINC;
	m_screenMenu.con_notfade_toggle.generic.x	= 0;
	m_screenMenu.con_notfade_toggle.generic.y	= y += UIFT_SIZEINC;
	m_screenMenu.con_notlarge_toggle.generic.x	= 0;
	m_screenMenu.con_notlarge_toggle.generic.y	= y += UIFT_SIZEINC;
	m_screenMenu.con_timestamp_list.generic.x	= 0;
	m_screenMenu.con_timestamp_list.generic.y	= y += UIFT_SIZEINC;
	m_screenMenu.con_notlines_slider.generic.x	= 0;
	m_screenMenu.con_notlines_slider.generic.y	= y += (UIFT_SIZEINC*2);
	m_screenMenu.con_notlines_amount.generic.x	= (UIFT_SIZE * (SLIDER_RANGE + 5));
	m_screenMenu.con_notlines_amount.generic.y	= y;
	m_screenMenu.con_alpha_slider.generic.x		= 0;
	m_screenMenu.con_alpha_slider.generic.y		= y += UIFT_SIZEINC;
	m_screenMenu.con_alpha_amount.generic.x		= (UIFT_SIZE * (SLIDER_RANGE + 5));
	m_screenMenu.con_alpha_amount.generic.y		= y;
	m_screenMenu.con_drop_slider.generic.x		= 0;
	m_screenMenu.con_drop_slider.generic.y		= y += UIFT_SIZEINC;
	m_screenMenu.con_drop_amount.generic.x		= (UIFT_SIZE * (SLIDER_RANGE + 5));
	m_screenMenu.con_drop_amount.generic.y		= y;
	m_screenMenu.con_scroll_slider.generic.x	= 0;
	m_screenMenu.con_scroll_slider.generic.y	= y += UIFT_SIZEINC;
	m_screenMenu.con_scroll_amount.generic.x	= (UIFT_SIZE * (SLIDER_RANGE + 5));
	m_screenMenu.con_scroll_amount.generic.y	= y;
	m_screenMenu.con_fontscale_slider.generic.x	= 0;
	m_screenMenu.con_fontscale_slider.generic.y	= y += UIFT_SIZEINC;
	m_screenMenu.con_fontscale_amount.generic.x	= (UIFT_SIZE * (SLIDER_RANGE + 5));
	m_screenMenu.con_fontscale_amount.generic.y	= y;

	m_screenMenu.ch_header.generic.x			= 0;
	m_screenMenu.ch_header.generic.y			= y += UIFT_SIZEINC*2;

	if (m_screenMenu.crosshairsFound && m_screenMenu.ch_number_list.curValue < (int)m_screenMenu.crosshairs.Count()) {
		chMat = m_screenMenu.crosshairs[m_screenMenu.ch_number_list.curValue].mat;
		cgi.R_GetImageSize (chMat, &width, &height);
	}
	else {
		chMat = cgMedia.whiteTexture;
		width = 32;
		height = 32;
	}

	if (m_screenMenu.crosshairsFound) {
		m_screenMenu.ch_number_list.generic.x		= 0;
		m_screenMenu.ch_number_list.generic.y		= y += UIFT_SIZEINCMED + UIFT_SIZEINC;
		m_screenMenu.ch_image.generic.x				= width * -0.5;
		m_screenMenu.ch_image.generic.y				= y += UIFT_SIZEINC*2;
		m_screenMenu.ch_image.width					= width;
		m_screenMenu.ch_image.height				= height;
		m_screenMenu.ch_image.mat					= chMat;
	}
	else
		y += UIFT_SIZEINC;

	m_screenMenu.ch_alpha_slider.generic.x		= 0;
	m_screenMenu.ch_alpha_slider.generic.y		= y += m_screenMenu.ch_image.height * UI_SCALE + UIFT_SIZEINC;
	m_screenMenu.ch_alpha_amount.generic.x		= (UIFT_SIZE * (SLIDER_RANGE + 5));
	m_screenMenu.ch_alpha_amount.generic.y		= y;
	m_screenMenu.ch_pulse_slider.generic.x		= 0;
	m_screenMenu.ch_pulse_slider.generic.y		= y += UIFT_SIZEINC;
	m_screenMenu.ch_pulse_amount.generic.x		= (UIFT_SIZE * (SLIDER_RANGE + 5));
	m_screenMenu.ch_pulse_amount.generic.y		= y;
	m_screenMenu.ch_scale_slider.generic.x		= 0;
	m_screenMenu.ch_scale_slider.generic.y		= y += UIFT_SIZEINC;
	m_screenMenu.ch_scale_amount.generic.x		= (UIFT_SIZE * (SLIDER_RANGE + 5));
	m_screenMenu.ch_scale_amount.generic.y		= y;
	m_screenMenu.ch_red_slider.generic.x		= 0;
	m_screenMenu.ch_red_slider.generic.y		= y += (UIFT_SIZEINC*2);
	m_screenMenu.ch_red_amount.generic.x		= (UIFT_SIZE * (SLIDER_RANGE + 5));
	m_screenMenu.ch_red_amount.generic.y		= y;
	m_screenMenu.ch_green_slider.generic.x		= 0;
	m_screenMenu.ch_green_slider.generic.y		= y += UIFT_SIZEINC;
	m_screenMenu.ch_green_amount.generic.x		= (UIFT_SIZE * (SLIDER_RANGE + 5));
	m_screenMenu.ch_green_amount.generic.y		= y;
	m_screenMenu.ch_blue_slider.generic.x		= 0;
	m_screenMenu.ch_blue_slider.generic.y		= y += UIFT_SIZEINC;
	m_screenMenu.ch_blue_amount.generic.x		= (UIFT_SIZE * (SLIDER_RANGE + 5));
	m_screenMenu.ch_blue_amount.generic.y		= y;
	m_screenMenu.back_action.generic.x			= 0;
	m_screenMenu.back_action.generic.y			= y += UIFT_SIZEINC + UIFT_SIZEINCLG;

	// Render
	UI_DrawInterface (&m_screenMenu.frameWork);
}


/*
=============
UI_ScreenMenu_f
=============
*/
void UI_ScreenMenu_f ()
{
	ScreenMenu_Init ();
	M_PushMenu (&m_screenMenu.frameWork, ScreenMenu_Draw, ScreenMenu_Close, M_KeyHandler);
}
