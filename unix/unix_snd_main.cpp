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
// unix_snd_main.c
//

#include "../client/snd_local.h"
#include "unix_local.h"

cVar_t	*s_bits;
cVar_t	*s_speed;
cVar_t	*s_channels;
cVar_t	*s_system;

typedef enum sndSystem_s {
	SNDSYS_NONE,

	SNDSYS_ALSA,
	SNDSYS_OSS,
	SNDSYS_SDL,
} sndSystem_t;

static const char	*s_sysStrings[] = {
	"None",

	"ALSA",
	"OSS",
	"SDL",

	NULL
};

#define S_DEFAULTSYS	SNDSYS_SDL

static sndSystem_t		s_curSystem;

/*
=======================================================================

	UNIX SOUND BACKEND

    This attempts access to ALSA, OSS, and SDL audio systems.
=======================================================================
*/

/*
==============
SndImp_Init
===============
*/
qBool SndImp_Init (void)
{
	sndSystem_t		oldSystem;
	qBool			success;

	// Register variables
	if (!s_bits) {
		s_bits			= Cvar_Register ("s_bits",			"16",					CVAR_ARCHIVE);
		s_speed			= Cvar_Register ("s_speed",			"0",					CVAR_ARCHIVE);
		s_channels		= Cvar_Register ("s_channels",		"2",					CVAR_ARCHIVE);
		s_system		= Cvar_Register ("s_system",		s_sysStrings[S_DEFAULTSYS],		CVAR_ARCHIVE);
	}

	// Find the target system
	oldSystem = s_curSystem;
	if (!Q_stricmp (s_system->string, "alsa"))
		s_curSystem = SNDSYS_ALSA;
	else if (!Q_stricmp (s_system->string, "oss"))
		s_curSystem = SNDSYS_OSS;
	else if (!Q_stricmp (s_system->string, "sdl"))
		s_curSystem = SNDSYS_SDL;
	else {
		Com_Printf (PRNT_ERROR, "SndImp_Init: Invalid s_system selection, defaulting to '%s'\n", s_sysStrings[S_DEFAULTSYS]);
		s_curSystem = S_DEFAULTSYS;
	}

mark0:
	// Initialize the target system
	success = qFalse;
	switch (s_curSystem) {
	case SNDSYS_ALSA:
		// FIXME
		break;

	case SNDSYS_OSS:
		success = OSS_Init ();
		break;

	case SNDSYS_SDL:
		// FIXME
		break;
	}

	// If failed, fall-back
	if (!success && s_curSystem > SNDSYS_ALSA) {
		Com_Printf (PRNT_WARNING, "%s failed, attempting next system\n", s_sysStrings[s_curSystem]);
		s_curSystem--;
		goto mark0;
	}

	// Done
	return success;
}


/*
==============
SndImp_Shutdown
===============
*/
void SndImp_Shutdown (void)
{
	switch (s_curSystem) {
	case SNDSYS_ALSA:
		// FIXME
		break;

	case SNDSYS_OSS:
		OSS_Shutdown ();
		break;

	case SNDSYS_SDL:
		// FIXME
		break;
	}

	// Should never reach here
	return qFalse;
}


/*
==============
SndImp_GetDMAPos
===============
*/
int SndImp_GetDMAPos (void)
{
	switch (s_curSystem) {
	case SNDSYS_ALSA:
		// FIXME
		return qFalse;

	case SNDSYS_OSS:
		return OSS_GetDMAPos ();

	case SNDSYS_SDL:
		// FIXME
		return qFalse;
	}

	// Should never reach here
	return 0;
}


/*
==============
SndImp_BeginPainting
===============
*/
void SndImp_BeginPainting (void)
{
	switch (s_curSystem) {
	case SNDSYS_ALSA:
		// FIXME
		break;

	case SNDSYS_OSS:
		OSS_BeginPainting ();
		break;

	case SNDSYS_SDL:
		// FIXME
		break;
	}
}


/*
==============
SndImp_Submit

Send sound to device if buffer isn't really the DMA buffer
===============
*/
void SndImp_Submit (void)
{
	switch (s_curSystem) {
	case SNDSYS_ALSA:
		// FIXME
		break;

	case SNDSYS_OSS:
		OSS_Submit ();
		break;

	case SNDSYS_SDL:
		// FIXME
		break;
	}
}
