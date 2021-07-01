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
// cl_screen.c
// Client master for refresh
//

#include "cl_local.h"

/*
=============================================================================

	FRAME HANDLING

=============================================================================
*/

/*
================
SCR_BeginLoadingPlaque
================
*/
void SCR_BeginLoadingPlaque ()
{
	// Stop audio
	cls.soundPrepped = false;
	cls.refreshPrepped = false;
	Snd_StopAllSounds ();

	// Update connection info
	CL_CGModule_UpdateConnectInfo ();

	// Update the screen
	SCR_UpdateScreen ();

	// Shutdown media
	CL_MediaShutdown ();
}


/*
================
SCR_EndLoadingPlaque
================
*/
void SCR_EndLoadingPlaque ()
{
	// Clear the notify lines
	CL_ClearNotifyLines ();

	// Load media
	CL_MediaInit ();
}


/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush text to the screen.
==================
*/
void SCR_UpdateScreen()
{
	// If the screen is disabled do nothing at all
	if (cls.disableScreen)
		return;

	// Not initialized yet
	if (!clMedia.initialized)
		return;

	// Set separation values
	int numFrames;
	float separation[2];
	if (cls.refConfig.stereoEnabled)
	{
		numFrames = 2;

		// Range check cl_camera_separation so we don't inadvertently fry someone's brain
		Cvar_VariableSetValue (cl_stereo_separation, clamp (cl_stereo_separation->floatVal, 0.0, 1.0), true);

		separation[0] = -cl_stereo_separation->floatVal * 0.5f;
		separation[1] = cl_stereo_separation->floatVal * 0.5f;
	}
	else
	{
		numFrames = 1;
		separation[0] = separation[1] = 0;
	}

	// Update connection info
	switch (Com_ClientState())
	{
	case CA_CONNECTING:
	case CA_CONNECTED:
		CL_CGModule_UpdateConnectInfo();
		break;
	}

	// Render frame(s)
	for (int i=0 ; i<numFrames ; i++)
	{
		if (cl.cin.time > 0)
		{
			// Render the cinematic
			R_BeginFrame(separation[i]);
			CIN_DrawCinematic();
			GUI_Refresh();
		}
		else
		{
			// Time demo update
			if (Com_ClientState() == CA_ACTIVE && cl_timedemo->intVal)
			{
				R_TimeDemoFrame();
			}

			// Render the scene
			R_BeginFrame(separation[i]);
			CL_CGModule_RenderView(separation[i]);
			GUI_Refresh();
		}

		CL_DrawConsole();
	}

	R_EndFrame();
}

