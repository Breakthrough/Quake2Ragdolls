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
// unix_glimp.c
// This file contains GL context related code and some glue
//

#include <termios.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <dlfcn.h>

#include "../renderer/r_local.h"
#include "../client/cl_local.h"
#include "unix_glimp.h"
#include "unix_local.h"

static qBool    vid_queueRestart;
static qBool    vid_isActive;

glxState_t glxState = {.OpenGLLib = NULL};

/*
=============================================================================

	FRAME SETUP

=============================================================================
*/

/*
=================
GLimp_BeginFrame
=================
*/
void GLimp_BeginFrame (void)
{
}


/*
=================
GLimp_EndFrame

Responsible for doing a swapbuffers and possibly for other stuff as yet to be determined.
Probably better not to make this a GLimp function and instead do a call to GLimp_SwapBuffers.

Only error check if active, and don't swap if not active an you're in fullscreen
=================
*/
void GLimp_EndFrame (void)
{
	X11_SwapBuffers ();
}

/*
=============================================================================

	DLL GLUE

=============================================================================
*/

/*
============
VID_Restart_f

Console command to re-start the video mode and refresh DLL. We do this
simply by setting the modified flag for the vid_ref variable, which will
cause the entire video mode and refresh DLL to be reset on the next frame.
============
*/
void VID_Restart_f (void)
{
	vid_queueRestart = qTrue;
}


/*
============
ListRemaps_f

Console command to list all keys mapped to AUX%d
============
*/
static void ListRemaps_f (void)
{
	int	i, a;
	char	*k;

	Com_Printf (0, "Remapped keys:\n");
	for (i=0 ; ; i++) {
		k = X11_GetAuxKeyRemapName (i, &a);
		if (!k)
			break;

		Com_Printf (0, "AUX%-2d = %s\n", a-K_AUX1+1, k);
	}
}


/*
============
VID_CheckChanges

This function gets called once just before drawing each frame, and it's sole purpose in life
is to check to see if any of the video mode parameters have changed, and if they have to 
update the rendering DLL and/or video mode to match.
============
*/
void VID_CheckChanges (refConfig_t *outConfig)
{
	int errNum;

	while (vid_queueRestart) {
		qBool cgWasActive = cls.mapLoaded;

		CL_MediaShutdown ();

		// Refresh has changed
		vid_queueRestart = qFalse;
		cls.refreshPrepped = qFalse;
		cls.disableScreen = qTrue;

		// Kill if already active
		if (vid_isActive) {
			R_Shutdown (qFalse);
			vid_isActive = qFalse;
		}

		// Initialize renderer
		errNum = R_Init ();

		// Refresh init failed!
		if (errNum != R_INIT_SUCCESS) {
			R_Shutdown (qTrue);
			vid_isActive = qFalse;

			switch (errNum) {
			case R_INIT_QGL_FAIL:
				Com_Error (ERR_FATAL, "Couldn't initialize OpenGL!\n" "QGL library failure!");
				break;

			case R_INIT_OS_FAIL:
				Com_Error (ERR_FATAL, "Couldn't initialize OpenGL!\n" "Incorrect operating system!");
				break;

			case R_INIT_MODE_FAIL:
				Com_Error (ERR_FATAL, "Couldn't initialize OpenGL!\n" "Couldn't set video mode!");
				break;
			}
		}

		R_GetRefConfig (outConfig);

		Snd_Init ();
		CL_MediaInit ();

		cls.disableScreen = qFalse;

		CL_ConsoleClose ();

		// This is to stop cgame from initializing on first load
		// and so it will load after a vid_restart while connected somewhere
		if (cgWasActive) {
			CL_CGModule_LoadMap ();
			Key_SetDest (KD_GAME);
		}
		else {
			CL_CGModule_MainMenu ();
		}

		vid_isActive = qTrue;
	}
}


/*
============
VID_Init
============
*/
void VID_Init (refConfig_t *outConfig)
{
	vid_xpos = Cvar_Register ("vid_xpos", "3", CVAR_ARCHIVE);
	vid_ypos = Cvar_Register ("vid_ypos", "22", CVAR_ARCHIVE);
	vid_fullscreen = Cvar_Register ("vid_fullscreen", "0", CVAR_ARCHIVE);

	// Add some console commands that we want to handle
	Cmd_AddCommand ("vid_restart", VID_Restart_f, "Restarts refresh and media");
	Cmd_AddCommand ("listremaps", ListRemaps_f, "Lists what keys are remapped to AUX* bindings");

	// Start the graphics mode and load refresh DLL
	vid_isActive = qFalse;
	vid_fullscreen->modified = qTrue;
	vid_queueRestart = qTrue;

	VID_CheckChanges (outConfig);
}


/*
============
VID_Shutdown
============
*/
void VID_Shutdown (void)
{
	if(vid_isActive) {
		R_Shutdown (qTrue);
		vid_isActive = qFalse;
	}

	Cmd_RemoveCommand ("vid_restart", NULL);
	Cmd_RemoveCommand ("listremaps", NULL);
}

/*
=============================================================================

	INIT / SHUTDOWN

=============================================================================
*/

/*
=================
GLimp_Shutdown
=================
*/
void GLimp_Shutdown (qBool full)
{
	X11_DestroyGLContext ();
}


/*
=================
GLimp_Init
=================
*/
qBool GLimp_Init (void)
{
	GLimp_Shutdown (qFalse);

	if (X11_CreateGLContext ()) {
		if (X11_GetGLAttribute (GLX_RGBA)) {
			ri.cColorBits = X11_GetGLAttribute (GLX_RED_SIZE) + X11_GetGLAttribute (GLX_GREEN_SIZE) + X11_GetGLAttribute (GLX_BLUE_SIZE);
			ri.cAlphaBits = X11_GetGLAttribute (GLX_ALPHA_SIZE);
		}
		else {
			// Indexed colors
			ri.cColorBits = ri.cAlphaBits = 0;
		}

		ri.cDepthBits = X11_GetGLAttribute (GLX_DEPTH_SIZE);
		ri.cStencilBits = X11_GetGLAttribute (GLX_STENCIL_SIZE);

		// Grab mouse input
		X11_SetKMGrab (qFalse, qTrue);
		return qTrue;
	}

	return qFalse;
}


/*
=================
GLimp_AttemptMode

Returns qTrue when the a mode change was successful
=================
*/
qBool GLimp_AttemptMode (qBool fullScreen, int width, int height)
{
	ri.config.vidFullScreen = fullScreen;
	ri.config.vidWidth = width;
	ri.config.vidHeight = height;
	// FIXME: These are windows specific really, they can be moved to glwState or something...
	ri.config.vidBitDepth = 0;
	ri.config.vidFrequency = 0;

	Com_Printf (0, "Mode: %d x %d %s\n", width, height, fullScreen ? "(fullscreen)" : "(windowed)");
	
	// Attempt fullscreen if desired
	if (fullScreen) {
		if (X11_SetVideoMode (width, height, qTrue))
			return qTrue;

		Com_Printf (PRNT_ERROR, "...fullscreen mode failed\n");
		return qFalse;
	}

	// Otherwise, attempt windowed mode
	Com_Printf (0, "...attempting windowed mode\n");
	if (X11_SetVideoMode (width, height, qFalse))
		return qTrue;

	Com_Printf (PRNT_ERROR, "...windowed mode failed\n");
	return qFalse;
}



/*
=================
GLimp_GetGammaRamp
=================
*/
qBool GLimp_GetGammaRamp (uint16 *ramp)
{
	return SCR_GetGammaRamp (ramp);
}


/*
=================
GLimp_SetGammaRamp
=================
*/
void GLimp_SetGammaRamp (uint16 *ramp)
{
	SCR_SetGammaRamp (ramp);
}
