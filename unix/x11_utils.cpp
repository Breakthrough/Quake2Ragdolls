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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

//
// x11_utils.c
// This file contains some useful functions and XFree86 extensions specific code
//

#include "x11_linux.h"
#include "unix_local.h"

/*
==========================================================================

  XFree86 SPECIFIC STUFF

==========================================================================
*/

#ifdef XF86VMODE

static int xf86_vidmodes_suported = 0;
static XF86VidModeModeInfo **xf86_vidmodes;
static int xf86_vidmodes_num;
static int xf86_vidmodes_active = 0;

/*
=============
SCR_VidmodesInit
=============
*/
void SCR_VidmodesInit (void)
{
	int MajorVersion, MinorVersion;
	int i;

	// Get video mode list
	MajorVersion = MinorVersion = 0;

	if (XF86VidModeQueryVersion (x11display.dpy, &MajorVersion, &MinorVersion)) {
		Com_Printf (0, "..XFree86-VidMode Extension Version %d.%d\n", MajorVersion, MinorVersion);
		XF86VidModeGetAllModeLines (x11display.dpy, x11display.scr, &xf86_vidmodes_num, &xf86_vidmodes);
		xf86_vidmodes_suported = 1;
	}
	else {
		Com_Printf (0, "..XFree86-VidMode Extension not available\n");
		xf86_vidmodes_suported = 0;
	}
}


/*
=============
SCR_VidmodesFree
=============
*/
void SCR_VidmodesFree (void)
{
	if (xf86_vidmodes_suported)
		XFree(xf86_vidmodes);

	xf86_vidmodes_suported = 0;
}


/*
=============
SCR_VidmodesSwitch
=============
*/
void SCR_VidmodesSwitch (int mode)
{
	if (xf86_vidmodes_suported) {
		XF86VidModeSwitchToMode (x11display.dpy, x11display.scr, xf86_vidmodes[mode]);
		XF86VidModeSetViewPort (x11display.dpy, x11display.scr, 0, 0);
	}

	xf86_vidmodes_active = 1;
}


/*
=============
SCR_VidmodesSwitchBack
=============
*/
void SCR_VidmodesSwitchBack (void)
{
	if (xf86_vidmodes_suported) {
		if (xf86_vidmodes_active) 
			SCR_VidmodesSwitch (0);
	}

	xf86_vidmodes_active = 0;
}


/*
=============
SCR_VidmodesFindBest
=============
*/
void SCR_VidmodesFindBest (int *mode, int *pwidth, int *pheight)
{
	int i, best_fit, best_dist, dist, x, y;

	best_fit = -1;
	best_dist = 999999999;

	if (xf86_vidmodes_suported) {
		for (i =0; i < xf86_vidmodes_num; i++) {
			if (xf86_vidmodes[i]->hdisplay < *pwidth || xf86_vidmodes[i]->vdisplay < *pheight)
				continue;

			x = xf86_vidmodes[i]->hdisplay - *pwidth;
			y = xf86_vidmodes[i]->vdisplay - *pheight;

			if (x > y)
				dist = y;
			else
				dist = x;

			if (dist < 0) 
				dist = -dist; // Only positive number please
		
			if( dist < best_dist) {
				best_dist = dist;
				best_fit = i;
			}

			Com_Printf (0, "%ix%i -> %ix%i: %i\n", *pwidth, *pheight, xf86_vidmodes[i]->hdisplay, xf86_vidmodes[i]->vdisplay, dist);
		}

		if (best_fit >= 0) {
			Com_Printf (0, "%ix%i selected\n", xf86_vidmodes[best_fit]->hdisplay, xf86_vidmodes[best_fit]->vdisplay);

			*pwidth = xf86_vidmodes[best_fit]->hdisplay;
			*pheight = xf86_vidmodes[best_fit]->vdisplay;
		}
	}

	*mode = best_fit;
}


/*
=============
SCR_GetGammaRamp
=============
*/
qBool SCR_GetGammaRamp (uint16 *ramp)
{
	size_t stride = 256;
	return XF86VidModeGetGammaRamp (x11display.dpy, x11display.scr, stride, ramp, ramp + stride, ramp + (stride << 1) )?qTrue:qFalse;
}


/*
=============
SCR_SetGammaRamp
=============
*/
qBool SCR_SetGammaRamp (uint16 *ramp)
{
	size_t stride = 256;
	return XF86VidModeSetGammaRamp (x11display.dpy, x11display.scr, stride, ramp, ramp + stride, ramp + (stride << 1))?qTrue:qFalse;
}

#else // XF86VMODE

/*
=============
SCR_VidmodesInit
=============
*/
void SCR_VidmodesInit (void)
{
}


/*
=============
SCR_VidmodesFree
=============
*/
void SCR_VidmodesFree (void)
{
}


/*
=============
SCR_VidmodesSwitch
=============
*/
void SCR_VidmodesSwitch (int mode)
{
}


/*
=============
SCR_VidmodesSwitchBack
=============
*/
void SCR_VidmodesSwitchBack (void)
{
}


/*
=============
SCR_VidmodesFindBest
=============
*/
void SCR_VidmodesFindBest (int *mode, int *pwidth, int *pheight)
{
	Window	w;
	int	t;
	uint32	y;

	if (XGetGeometry (x11display.dpy, x11display.root, &w, &t, &t, (unsigned int*)pwidth, (unsigned int*)pheight, &y, &y))
		*mode = 0;
	else
		*mode = -1;
}


/*
=============
SCR_GetGammaRamp
=============
*/
qBool SCR_GetGammaRamp (uint16 *ramp)
{
	// FIXME
	return qFalse;
}


/*
=============
SCR_SetGammaRamp
=============
*/
qBool SCR_SetGammaRamp (uint16 *ramp)
{
	// FIXME
	return qFalse;
}

#endif


/*
==========================================================================

  X11 MISC STUFF

==========================================================================
*/

/*
=============
X11_SetNoResize
=============
*/
void X11_SetNoResize (Window w, int width, int height)
{
	XSizeHints *hints;

	if (!x11display.dpy)
		return;

	hints = XAllocSizeHints ();
	if (!hints)
		return;

	hints->min_width = hints->max_width = width;
	hints->min_height = hints->max_height = height;
		
	hints->flags = PMaxSize | PMinSize;

	XSetWMNormalHints (x11display.dpy, w, hints);
	XFree (hints);
}


/*
=============
X11_SetWindowInitialState
=============
*/
void X11_SetWindowInitialState (Display *display, Window w, int state)
{
	XWMHints h;
	
	h.flags = StateHint;
	h.initial_state = state;
	XSetWMHints (display, w, &h);
}


/*
=============
X11_CreateNullCursor
=============
*/
Cursor X11_CreateNullCursor (void)
{
	Pixmap cursormask; 
	XGCValues xgc;
	GC gc;
	XColor dummycolour;
	Cursor cursor;
	
	cursormask = XCreatePixmap (x11display.dpy, x11display.root, 1, 1, 1);
	xgc.function = GXclear;
	gc = XCreateGC (x11display.dpy, cursormask, GCFunction, &xgc);
	XFillRectangle (x11display.dpy, cursormask, gc, 0, 0, 1, 1);
	
	dummycolour.pixel = 0;
	dummycolour.red = 0;
	dummycolour.flags = 04;
	
	cursor = XCreatePixmapCursor (x11display.dpy, cursormask, cursormask, &dummycolour, &dummycolour, 0, 0);
	XFreePixmap (x11display.dpy, cursormask);
	XFreeGC (x11display.dpy, gc);
	
	return cursor;
}


/*
=============
X11_DiscardEvents
=============
*/
void X11_DiscardEvents (int eventType)
{
	XEvent e;

	while (XCheckTypedEvent (x11display.dpy, eventType, &e)) ;
}


/*
==========================================================================

  INPUT CONVERSION

==========================================================================
*/

static KeySym		x11_auxRemap[32];
static qBool		x11_noAltKey;
extern keyInfo_t	key_keyInfo[K_MAXKEYS]; /* this and its use should be moved to the glue file */

/*
=============
X11_LateKey
=============
*/
int X11_LateKey (XKeyEvent *ev)
{
	char	buf[64];
	KeySym	keysym;
	int	i;

	XLookupString (ev, buf, sizeof buf, &keysym, 0);

	if (buf[1])
		buf[0] = 0;

	if ((keysym == XK_Meta_L || keysym == XK_Meta_R) && x11_noAltKey)
		return K_ALT;

	switch (keysym) {
	case XK_KP_Page_Up:	return K_KP_PGUP;
	case XK_Page_Up:	return K_PGUP;

	case XK_KP_Page_Down:	return K_KP_PGDN;
	case XK_Page_Down:	return K_PGDN;

	case XK_KP_Home:	return K_KP_HOME;
	case XK_Home:		return K_HOME;

	case XK_KP_End:		return K_KP_END;
	case XK_End:		return K_END;

	case XK_KP_Left:	return K_KP_LEFTARROW;
	case XK_Left:		return K_LEFTARROW;

	case XK_KP_Right:	return K_KP_RIGHTARROW;
	case XK_Right:		return K_RIGHTARROW;  

	case XK_KP_Down:	return K_KP_DOWNARROW;
	case XK_Down:		return K_DOWNARROW;

	case XK_KP_Up:		return K_KP_UPARROW;
	case XK_Up:		return K_UPARROW;

	case XK_Escape:		return K_ESCAPE;

	case XK_KP_Enter:	return K_KP_ENTER;
	case XK_Return:		return K_ENTER;

	case XK_Tab:		return K_TAB;

	case XK_F1:		return K_F1;
	case XK_F2:		return K_F2;
	case XK_F3:		return K_F3;
	case XK_F4:		return K_F4;
	case XK_F5:		return K_F5;
	case XK_F6:		return K_F6;
	case XK_F7:		return K_F7;
	case XK_F8:		return K_F8;
	case XK_F9:		return K_F9;
	case XK_F10:		return K_F10;
	case XK_F11:		return K_F11;
	case XK_F12:		return K_F12;

	case XK_BackSpace:	return K_BACKSPACE;

	case XK_KP_Delete:	return K_KP_DEL;
	case XK_Delete:		return K_DEL;

	case XK_Pause:		return K_PAUSE;

	case XK_Shift_L:	return K_SHIFT;
	case XK_Shift_R:	return K_SHIFT;

	case XK_Execute:	return K_CTRL;
	case XK_Control_L:	return K_CTRL;
	case XK_Control_R:	return K_CTRL;

	case XK_Alt_L:		return K_ALT;
	case XK_Alt_R:		return K_ALT;

	case XK_KP_Begin:	return K_KP_FIVE;

	case XK_Insert:		return K_INS;
	case XK_KP_Insert:	return K_KP_INS;

	case XK_KP_Multiply:	return '*';
	case XK_KP_Add:		return K_KP_PLUS;
	case XK_KP_Subtract:	return K_KP_MINUS;
	case XK_KP_Divide:	return K_KP_SLASH;

	case XK_Caps_Lock:	return K_CAPSLOCK;

	default:
		if (buf[0] >= 'A' && buf[0] <= 'Z')
			buf[0] |= 32;
		if (buf[0] > 26)
			return buf[0];

		for (i=0 ; i<sizeof(x11_auxRemap)/sizeof(x11_auxRemap[0]) && x11_auxRemap[i] ; i++) {
			if (x11_auxRemap[i] == keysym)
				return K_AUX32-i;
		}
		break;
	}

	return 0;
}


/*
=============
X11_GetAuxKeyRemapName
=============
*/
char *X11_GetAuxKeyRemapName (int index, int *kc)
{
	if (index < 0 || index > sizeof(x11_auxRemap)/sizeof(x11_auxRemap[0]))
		return NULL;
	if (!x11_auxRemap[index])
		return NULL;

	if (kc)
		*kc = K_AUX32-index;
	return XKeysymToString (x11_auxRemap[index]);
}


/*
=============
X11_ScanUnmappedKeys
=============
*/
int X11_ScanUnmappedKeys (void)
{
  int     i;
  int     kc = 0;
  int     shift = 0;
  KeySym  ks;
  XKeyEvent ev;
  XModifierKeymap *mkm;

  memset (x11_auxRemap, 0, sizeof (x11_auxRemap));

  /* find the modifier bit corresponding to shift */
  mkm = XGetModifierMapping(x11display.dpy);
  for (i = 0; i < mkm->max_keypermod*8; i++) if (mkm->modifiermap[i]) {
    ks = XKeycodeToKeysym(x11display.dpy, mkm->modifiermap[i], 0);
    if (ks == XK_Shift_L || ks == XK_Shift_R) {
      shift = 1 << (i/mkm->max_keypermod);
      break;
    }
  }
  XFreeModifiermap(mkm);

  /* determine if keyboard has both meta and shift at the same time */
  x11_noAltKey = qTrue;
  for (i = 1; i < 256; i++) {
    ks = XKeycodeToKeysym(x11display.dpy, i, 0);
    if (ks == XK_Alt_L || ks == XK_Alt_R) {
      x11_noAltKey = qFalse;
      break;
    }
  }

  memset(&ev, 0, sizeof(ev));
  ev.display = x11display.dpy;

 /* search for unmapped keys */
 for (i = 1; i < 256; i++) {
    ks = XKeycodeToKeysym(x11display.dpy, i, 0);
    if (ks) {
      int kl, nk;
      ev.keycode = i;
      ev.state = 0;
      kl = X11_LateKey (&ev);
      if (kl > 32 && kl < 128) {
        /* if it is a known charater key, determine which is it's shift character */
        ev.state = shift;
        nk =  X11_LateKey (&ev);
        if (nk > 32 && nk < 128)
           key_keyInfo[kl].shiftValue = nk;
      } else
      if (!kl && kc < sizeof(x11_auxRemap)/sizeof(x11_auxRemap[0])) {
        /* unknown key, remap it to a free aux key */
        x11_auxRemap[kc++] = ks;
      }
    }
  }
}
