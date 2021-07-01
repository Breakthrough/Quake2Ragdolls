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
// cg_loadscreen.c
//

#include "cg_local.h"

#define CG_HSCALE (((float)cg.refConfig.vidWidth / 640.0f))
#define CG_VSCALE (((float)cg.refConfig.vidHeight / 480.0f))

#define CGFT_HSCALE (8*CG_HSCALE)
#define CGFT_VSCALE (8*CG_VSCALE)

/*
=============================================================================

	LOADING SCREEN

=============================================================================
*/

/*
=================
CG_PrepLoadScreen
=================
*/
static void CG_PrepLoadScreen ()
{
	char	buffer[MAX_QPATH];

	if (cgMedia.loadScreenPrepped)
		return;

	if (cg.configStrings[CS_MODELS+1][0]) {
		Com_FileBase (cg.configStrings[CS_MODELS+1], buffer);
		cgMedia.loadMapShot = cgi.R_RegisterPic (Q_VarArgs ("maps/%s.tga", buffer));

		cgMedia.loadScreenPrepped = true;
	}
	else {
		cgMedia.loadMapShot = NULL;
	}
}


/*
=================
CG_DrawLoadingScreen
=================
*/
static void CG_DrawLoadingScreen ()
{
	char	buffer[MAX_QPATH];
	float	barSize;
	int		yOffset;
	char	*token;

	if (cgMedia.initialized)
		return;

	// Fill back with black
	cgi.R_DrawFill (0, 0, cg.refConfig.vidWidth, cg.refConfig.vidHeight, Q_BColorBlack);

	if (!cgMedia.loadScreenPrepped)
		CG_PrepLoadScreen ();

	if (!cg.configStrings[CS_MODELS+1][0])
		return;

	colorb brownColor(217, 179, 51, 255);

	// Backsplash
	cgi.R_DrawPic (cgMedia.loadSplash, 0, QuadVertices().SetVertices(0, 0, cg.refConfig.vidWidth, cg.refConfig.vidHeight), Q_BColorWhite);

	// Map name
	Q_strncpyz (buffer, cg.configStrings[CS_NAME], sizeof(buffer));
	for (token=strtok(buffer, "\n"), yOffset=0 ; token ; token=strtok(NULL, "\n")) {
		cgi.R_DrawString (NULL, 10*CG_HSCALE, (yOffset+170)*CG_VSCALE, CG_HSCALE*1.25f, CG_HSCALE*1.25f, FS_SHADOW, token, brownColor);
		yOffset += 10;
	}

	// Maps/map.bsp name
	brownColor.R = 152;
	brownColor.G = 125;
	brownColor.B = 36;

	Q_strncpyz (buffer, cg.configStrings[CS_MODELS+1], sizeof(buffer));
	cgi.R_DrawString (NULL, 10*CG_HSCALE, (yOffset+170)*CG_VSCALE, CG_HSCALE*1.25f, CG_HSCALE*1.25f, FS_SHADOW, buffer, brownColor);

	// Loading item
	if (cg.loadingString[0]) {
		cgi.R_DrawString (NULL, 10*CG_HSCALE, (yOffset+200)*CG_VSCALE, CG_HSCALE, CG_HSCALE, FS_SHADOW, cg.loadingString, Q_BColorLtGrey);
		cgi.R_DrawString (NULL, 10*CG_HSCALE, (yOffset+200)*CG_VSCALE+CGFT_VSCALE, CG_HSCALE, CG_HSCALE, FS_SHADOW, cg.loadFileName, Q_BColorLtGrey);
	}

	// Map image
	cgi.R_DrawFill (cg.refConfig.vidWidth - ((18+260)*CG_HSCALE), 18*CG_VSCALE, 260*CG_HSCALE, 260*CG_VSCALE, Q_BColorBlack);
	cgi.R_DrawPic (cgMedia.loadMapShot ? cgMedia.loadMapShot : cgMedia.loadNoMapShot, 0,
		QuadVertices().SetVertices(cg.refConfig.vidWidth - ((20+256)*CG_HSCALE), 20*CG_VSCALE,
		256*CG_HSCALE, 256*CG_VSCALE),
		Q_BColorWhite);

	// Loading bar
	barSize = cg.refConfig.vidWidth*(((float)cg.loadingValue) / ((float)cg.loadingMaxValues));
	cgi.R_DrawPic (cgMedia.loadBarNeg, 0, QuadVertices().SetVertices(barSize, 310*CG_VSCALE, cg.refConfig.vidWidth-(int)barSize, 32*CG_VSCALE), Q_BColorWhite);
	if (cg.loadingValue)
		cgi.R_DrawPic (cgMedia.loadBarPos, 0, QuadVertices().SetVertices(0, 310*CG_VSCALE, (int)barSize, 32*CG_VSCALE), Q_BColorWhite);

	// Percentage display
	Q_snprintfz (buffer, sizeof(buffer), "%3.1f%%", (((float)cg.loadingValue) / ((float)cg.loadingMaxValues)) * 100);
	cgi.R_DrawString (NULL, (cg.refConfig.vidWidth-(strlen(buffer)*CGFT_HSCALE*1.5))*0.5, (310+32)*CG_VSCALE-(CGFT_VSCALE*2), CG_HSCALE*1.5f, CG_HSCALE*1.5f, FS_SHADOW, buffer, Q_BColorLtGrey);
}

void CG_LoadingValues (int values)
{
	if (cgMedia.initialized)
		return;
	if (!cg.mapLoading)
		return;

	cg.loadingMaxValues = values;
	cg.loadingValue = 0;
	cgi.R_UpdateScreen ();
	cgi.Sys_SendKeyEvents ();	// pump message loop
}

void CG_IncLoadingVal ()
{
	if (cgMedia.initialized)
		return;
	if (!cg.mapLoading)
		return;

	cg.loadingValue++;
	cgi.R_UpdateScreen ();
	cgi.Sys_SendKeyEvents ();	// pump message loop
}


/*
=================
CG_LoadingString
=================
*/
void CG_LoadingString (char *str)
{
	if (cgMedia.initialized)
		return;
	if (!cg.mapLoading)
		return;

	if (str) {
		Q_strncpyz (cg.loadingString, str, sizeof(cg.loadingString));
		cgi.R_UpdateScreen ();
		cgi.Sys_SendKeyEvents ();	// pump message loop
	}
	else
		cg.loadFileName[0] = '\0';
}


/*
=================
CG_LoadingFilename
=================
*/
void CG_LoadingFilename (char *str)
{
	if (cgMedia.initialized)
		return;
	if (!cg.mapLoading)
		return;

	if (str) {
		Q_strncpyz (cg.loadFileName, str, sizeof(cg.loadFileName));
		cgi.R_UpdateScreen ();
		cgi.Sys_SendKeyEvents ();	// pump message loop
	}
	else
		cg.loadFileName[0] = '\0';
}


/*
==============
CG_UpdateConnectInfo

-1 is passed to dlPercent and bytesDownloaded if there is no file downloading
==============
*/
void CG_UpdateConnectInfo (char *serverName, char *serverMessage, int connectCount, char *dlFileName, int dlPercent, float bytesDownloaded, bool acLoading)
{
	cg.serverName = serverName;
	cg.serverMessage = serverMessage;
	cg.connectCount = connectCount;
	cg.download.fileName = dlFileName;
	cg.download.percent = dlPercent;
	cg.download.bytesDown = bytesDownloaded;

	cg.download.downloading = (bool)(dlFileName && dlFileName[0] && dlPercent >= 0 && bytesDownloaded >= 0);
	cg.localServer = (bool)(cgi.Com_ServerState () && (cg.maxClients <= 1 || !Q_stricmp (cg.serverName, "localhost")));

	cg.acLoading = acLoading;
}


/*
==============
CG_DrawConnectScreen
==============
*/
#define UISCALE		(cg.refConfig.vidWidth / 640.0)
#define FTSCALE		(8*UISCALE)
void CG_DrawConnectScreen ()
{
	char	buffer[64];

	// Clear the screen between map changes
	if (cg.mapLoaded) {
		cgi.R_DrawFill (0, 0, cg.refConfig.vidWidth, cg.refConfig.vidHeight, Q_BColorBlack);
		return;
	}

	// Only draw the background if the loading screen isn't up
	if (!cg.mapLoading)
		cgi.R_DrawFill (0, 0, cg.refConfig.vidWidth, cg.refConfig.vidHeight, Q_BColorBlack);

	if (!cg.localServer) {
		Q_snprintfz (buffer, sizeof(buffer), "Connecting to: %s", cg.serverName);
		cgi.R_DrawString (NULL, 10*UISCALE, 158*UISCALE, UISCALE, UISCALE, FS_SHADOW, buffer, Q_BColorWhite);
	}

	if (cg.download.downloading) {
		char	dlBar[28]; // left[percent/4]right|terminate
		int		spot;

		Q_snprintfz (buffer, sizeof(buffer), "Downloading %s...", cg.download.fileName);
		cgi.R_DrawString (NULL, 10*UISCALE, 158*UISCALE+(FTSCALE+2), UISCALE, UISCALE, FS_SHADOW, buffer, Q_BColorWhite);

		memset (&dlBar, '\x81', sizeof(dlBar));
		dlBar[0] = '\x80';		// left
		dlBar[26] = '\x82';		// right
		dlBar[27] = '\0';		// terminate

		// Percent location
		spot = (cg.download.percent * 0.25f) + 1;
		if (spot > 25)
			spot = 25;
		dlBar[spot] = '\x83';

		Q_snprintfz (buffer, sizeof(buffer), "%s %i%%", dlBar, cg.download.percent);
		cgi.R_DrawString (NULL, 10*UISCALE+(FTSCALE*12), 158*UISCALE+((FTSCALE+2)*2), UISCALE, UISCALE, FS_SHADOW, buffer, Q_BColorWhite);

		Q_snprintfz (buffer, sizeof(buffer), "Downloaded: %.0fKBytes", cg.download.bytesDown*0.0009765625f);
		cgi.R_DrawString (NULL, 10*UISCALE+(FTSCALE*12), 158*UISCALE+((FTSCALE+2)*3), UISCALE, UISCALE, FS_SHADOW, buffer, Q_BColorWhite);
	}
	else if (cg.mapLoading) {
		// Stuff that shows while the loading screen shows
		CG_DrawLoadingScreen ();
	}
	else if (cg.acLoading) {
		// Show if anticheat is loading
		cgi.R_DrawString (NULL, 10*UISCALE, 158*UISCALE+FTSCALE+2, UISCALE, UISCALE, 0, "Anticheat loading...", Q_BColorWhite);
	}
	else if (cg.localServer) {
		// Local server
		Q_strncpyz (buffer, "Loading...", sizeof(buffer));
		cgi.R_DrawString (NULL, 10*UISCALE, 158*UISCALE+FTSCALE+2, UISCALE, UISCALE, 0, buffer, Q_BColorWhite);
	}
	else {
		// Show connection status
		Q_snprintfz (buffer, sizeof(buffer), "Awaiting connection... %i", cg.connectCount);
		cgi.R_DrawString (NULL, 10*UISCALE, 158*UISCALE+FTSCALE+2, UISCALE, UISCALE, 0, buffer, Q_BColorWhite);

		// Show any server messages
		if (cg.serverMessage && cg.serverMessage[0]) {
			cgi.R_DrawString (NULL, 10*UISCALE, 174*UISCALE+FTSCALE+2, UISCALE, UISCALE, 0, "Server message:", Q_BColorWhite);
			cgi.R_DrawString (NULL, 10*UISCALE, 184*UISCALE+FTSCALE+2, UISCALE, UISCALE, 0, cg.serverMessage, Q_BColorWhite);
		}
	}
}
