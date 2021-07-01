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
// x11_linux.h
//

#ifndef __unix__
#  error You should not be including this file on this platform
#endif // __unix__

#ifndef __X11_LINUX_H__
#define __X11_LINUX_H__

#include <dlfcn.h> // ELF dl loader
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>

#ifdef XF86DGA
#include <X11/extensions/xf86dga.h>
#endif

#ifdef XF86VMODE
#include <X11/extensions/xf86vmode.h>
#endif

#include <GL/glx.h>

#include "../renderer/r_local.h"
#include "../client/cl_local.h"

typedef struct x11display_s {
	Display			*dpy;
	int				scr;
	Window			root;
	Window			win;      /* managed window */
	Window			gl_win;   /* render window */
	Window			fs_win;   /* full screen unmanaged window */
	Window			grab_win; /* window that will grab the mouse events (either win or fs_win depending on fullscreen)*/
	Colormap		cmap;
	GLXContext		ctx;
	XVisualInfo		*visinfo;
	Atom			wmDeleteWindow;
	uint32			win_width, win_height;
	qBool			focused;
	int				fullscreenmode; /* > 0 when fullscreen */
} x11display_t;

extern x11display_t  x11display;

void		SCR_VidmodesInit (void);
void		SCR_VidmodesFree (void);

void		SCR_VidmodesSwitch (int mode);
void		SCR_VidmodesSwitchBack (void);
void		SCR_VidmodesFindBest (int *mode, int *pwidth, int *pheight);

qBool		SCR_GetGammaRamp (uint16 *ramp);
qBool		SCR_SetGammaRamp (uint16 *ramp);

void		X11_SetKMGrab (qBool kg, qBool mg);
qBool		X11_CreateGLContext (void);
int			X11_GetGLAttribute (int attr);

void		X11_DiscardEvents (int eventType);
Cursor		X11_CreateNullCursor (void);
void		X11_SetNoResize (Window w, int width, int height);

void		X11_SetWindowInitialState (Display *display, Window w, int state);

int			X11_LateKey (XKeyEvent *ev);

int			X11_ScanUnmappedKeys (void);
char		*X11_GetAuxKeyRemapName (int index, int* kc);

#endif  // __X11_LINUX_H__
