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
// win_snd.c
//

#include <float.h>
#include "../client/snd_local.h"
#include "win_local.h"

HRESULT (WINAPI *pDirectSoundCreate)(GUID FAR *lpGUID, LPDIRECTSOUND FAR *lplpDS, IUnknown FAR *pUnkOuter);
#define iDirectSoundCreate(a,b,c)	pDirectSoundCreate(a,b,c)

#define SECONDARY_BUFFER_SIZE	0x10000

struct sndWin_t {
	bool				initialized;

	bool				formatSet;

	DWORD				lockSize;

	MMTIME				mmStartTime;

	LPDIRECTSOUND		pDS;
	LPDIRECTSOUNDBUFFER	pDSBuf, pDSPBuf;

	HINSTANCE			hInstDS;

	HPSTR				lpData;
	DWORD				bufferSize;

	// DMA positioning; starts at 0 for disabled
	int					sample16;
	int					sent, completed;
};

static sndWin_t		snd_win;

/*
==============
GetDirectSoundErrorString
===============
*/
static const char *GetDirectSoundErrorString (int error)
{
	switch (error) {
	case DSERR_BUFFERLOST:			return "DSERR_BUFFERLOST";
	case DSERR_INVALIDCALL:			return "DSERR_INVALIDCALLS";
	case DSERR_INVALIDPARAM:		return "DSERR_INVALIDPARAM";
	case DSERR_PRIOLEVELNEEDED:		return "DSERR_PRIOLEVELNEEDED";
	}

	return "unknown";
}

/*
==============================================================================

	INIT / SHUTDOWN
 
==============================================================================
*/

/*
==============
DS_CreateBuffers
===============
*/
static bool DS_CreateBuffers (bool init)
{
	DSBUFFERDESC	dsbuf;
	DSBCAPS			dsbcaps;
	WAVEFORMATEX	pformat, format;
	DWORD			dwWrite;
	void			(*PrintFunc) (comPrint_t flags, char *fmt, ...);

	// We only want to spam the console when first initializing, not on focus change
	if (init)
		PrintFunc = Com_Printf;
	else
		PrintFunc = Com_DevPrintf;

	// Set the format
	PrintFunc (0, "Creating DirectSound buffers\n");

	memset (&format, 0, sizeof(format));
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = snd_audioDMA.channels;
	format.wBitsPerSample = snd_audioDMA.sampleBits;
	format.nSamplesPerSec = snd_audioDMA.speed;
	format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
	format.cbSize = 0;
	format.nAvgBytesPerSec = format.nSamplesPerSec*format.nBlockAlign; 

	PrintFunc (0, "...setting EXCLUSIVE coop level\n");
	if (snd_win.pDS->SetCooperativeLevel (sys_winInfo.hWnd, DSSCL_EXCLUSIVE) != DS_OK) {
		Com_Printf (PRNT_ERROR, "FAILED!\n");
		return false;
	}

	// Get access to the primary buffer, if possible, so we can set the sound hardware format
	memset (&dsbuf, 0, sizeof(dsbuf));
	dsbuf.dwSize = sizeof(DSBUFFERDESC);
	dsbuf.dwFlags = DSBCAPS_PRIMARYBUFFER;
	dsbuf.dwBufferBytes = 0;
	dsbuf.lpwfxFormat = NULL;

	memset (&dsbcaps, 0, sizeof(dsbcaps));
	dsbcaps.dwSize = sizeof(dsbcaps);
	snd_win.formatSet = true;

	// Create the primary buffer
	PrintFunc (0, "...creating the primary buffer\n");
	if (snd_win.pDS->CreateSoundBuffer (&dsbuf, &snd_win.pDSPBuf, NULL) == DS_OK) {
		pformat = format;

		PrintFunc (0, "...set primary sound format\n");
		if (snd_win.pDSPBuf->SetFormat (&pformat) != DS_OK) {
			Com_Printf (PRNT_ERROR, "FAILED!\n");
			snd_win.formatSet = false;
		}
	}
	else
		Com_Printf (PRNT_ERROR, "FAILED!\n");

	// Create the secondary buffer we'll actually work with
	if (!snd_win.formatSet || !s_primary->intVal) {
		PrintFunc (0, "...using secondary sound buffer\n");

		memset (&dsbuf, 0, sizeof(dsbuf));
		dsbuf.dwSize = sizeof(DSBUFFERDESC);
		dsbuf.dwFlags = DSBCAPS_CTRLFREQUENCY|DSBCAPS_LOCHARDWARE;
		dsbuf.dwBufferBytes = SECONDARY_BUFFER_SIZE;
		dsbuf.lpwfxFormat = &format;

		memset (&dsbcaps, 0, sizeof(dsbcaps));
		dsbcaps.dwSize = sizeof(dsbcaps);

		// Creat the sound buffer
		PrintFunc (0, "...creating secondary buffer\n");
		if (snd_win.pDS->CreateSoundBuffer (&dsbuf, &snd_win.pDSBuf, NULL) == DS_OK) {
			PrintFunc (0, "...locked hardware\n");
		}
		else {
			// Try again with software acceleration
			dsbuf.dwFlags = DSBCAPS_CTRLFREQUENCY|DSBCAPS_LOCSOFTWARE;

			if (snd_win.pDS->CreateSoundBuffer (&dsbuf, &snd_win.pDSBuf, NULL) != DS_OK) {
				Com_Printf (PRNT_ERROR, "FAILED!\n");
				return false;
			}

			PrintFunc (0, "...locked software\n");
		}

		snd_audioDMA.channels = format.nChannels;
		snd_audioDMA.sampleBits = format.wBitsPerSample;
		snd_audioDMA.speed = format.nSamplesPerSec;

		// Make sure the mixer is active
		if (snd_win.pDSBuf->Play (0, 0, DSBPLAY_LOOPING) != DS_OK) {
			Com_Printf (PRNT_ERROR, "...Looped sound play failed\n");
			return false;
		}

		// Get the buffer size
		if (snd_win.pDSBuf->GetCaps (&dsbcaps) != DS_OK) {
			Com_Printf (PRNT_ERROR, "...GetCaps FAILED!\n");
			return false;
		}
	}
	else {
		PrintFunc (0, "...using primary buffer\n");

		PrintFunc (0, "...setting WRITEPRIMARY coop level\n");
		if (snd_win.pDS->SetCooperativeLevel (sys_winInfo.hWnd, DSSCL_WRITEPRIMARY) != DS_OK) {
			Com_Printf (PRNT_ERROR, "FAILED!\n");
			return false;
		}

		// Get the buffer size
		if (snd_win.pDSPBuf->GetCaps (&dsbcaps) != DS_OK) {
			Com_Printf (PRNT_ERROR, "...GetCaps FAILED!\n");
			return false;
		}

		snd_win.pDSBuf = snd_win.pDSPBuf;

		// Make sure the mixer is active
		if (snd_win.pDSBuf->Play (0, 0, DSBPLAY_LOOPING) != DS_OK) {
			Com_Printf (PRNT_ERROR, "...Looped sound play failed\n");
			return false;
		}
	}

	PrintFunc (0,	"   %d channel(s)\n"
					"   %d bits/sample\n"
					"   %d bytes/sec\n",
					snd_audioDMA.channels, snd_audioDMA.sampleBits, snd_audioDMA.speed);

	snd_win.bufferSize = dsbcaps.dwBufferBytes;

	// We don't want anyone to access the buffer directly without locking it first
	snd_win.lpData = NULL; 

	snd_win.pDSBuf->Stop ();
	snd_win.pDSBuf->GetCurrentPosition (&snd_win.mmStartTime.u.sample, &dwWrite);
	snd_win.pDSBuf->Play (0, 0, DSBPLAY_LOOPING);

	snd_audioDMA.samples = snd_win.bufferSize / (snd_audioDMA.sampleBits / 8);
	snd_audioDMA.samplePos = 0;
	snd_audioDMA.submissionChunk = 1;
	snd_audioDMA.buffer = (byte *) snd_win.lpData;
	snd_win.sample16 = (snd_audioDMA.sampleBits/8) - 1;

	return true;
}


/*
==============
DS_DestroyBuffers
===============
*/
static void DS_DestroyBuffers (bool shutdown)
{
	void			(*PrintFunc) (comPrint_t flags, char *fmt, ...);

	// We only want to spam the console when first initializing, not on focus change
	if (shutdown)
		PrintFunc = Com_Printf;
	else
		PrintFunc = Com_DevPrintf;

	PrintFunc (0, "Destroying DirectSound buffers\n");
	if (snd_win.pDS) {
		PrintFunc (0, "...setting NORMAL coop level\n");
		snd_win.pDS->SetCooperativeLevel (sys_winInfo.hWnd, DSSCL_NORMAL);
	}

	if (snd_win.pDSBuf) {
		PrintFunc (0, "...stopping and releasing sound buffer\n");
		snd_win.pDSBuf->Stop ();
		snd_win.pDSBuf->Release ();
	}

	// only release primary buffer if it's not also the mixing buffer we just released
	if (snd_win.pDSPBuf && snd_win.pDSBuf != snd_win.pDSPBuf) {
		PrintFunc (0, "...releasing primary buffer\n");
		snd_win.pDSPBuf->Release ();
	}
	snd_win.pDSBuf = NULL;
	snd_win.pDSPBuf = NULL;

	snd_audioDMA.buffer = NULL;
}


/*
==================
SndImp_Init

Try to find a sound device to mix for.
Returns false if nothing is found.
==================
*/
bool SndImp_Init ()
{
	DSCAPS			dscaps;
	HRESULT			hresult;

	memset ((void *)&snd_audioDMA, 0, sizeof(snd_audioDMA));

	// Init DirectSound
	Com_Printf (0, "Initializing DirectSound\n");

	snd_audioDMA.channels = 2;
	snd_audioDMA.sampleBits = 16;

	switch (s_khz->intVal) {
	case 48:	snd_audioDMA.speed = 48000;	break;
	case 44:	snd_audioDMA.speed = 44100;	break;
	case 32:	snd_audioDMA.speed = 32000;	break;
	case 22:	snd_audioDMA.speed = 22050;	break;
	case 16:	snd_audioDMA.speed = 16000;	break;
	default:	snd_audioDMA.speed = 11025;	break;
	case 8:		snd_audioDMA.speed = 8000;	break;
	}

	// Load the DirectSound library
	if (!snd_win.hInstDS) {
		Com_Printf (0, "...LoadLibrary ( \"dsound.dll\" )\n");
		snd_win.hInstDS = LoadLibrary ("dsound.dll");
		if (!snd_win.hInstDS) {
			Com_Printf (PRNT_ERROR, "...LoadLibrary FAILED!\n");
			goto error;
		}

		pDirectSoundCreate = (HRESULT(APIENTRY*)(GUID*,LPDIRECTSOUND*,IUnknown*))GetProcAddress (snd_win.hInstDS, "DirectSoundCreate");
		if (!pDirectSoundCreate) {
			Com_Printf (PRNT_ERROR, "...NULL DirectSoundCreate proc address!\n");
			goto error;
		}
	}

	// Create the DirectSound object
	Com_Printf (0, "...creating DirectSound object\n");
	for ( ; ; ) {
		hresult = iDirectSoundCreate (NULL, &snd_win.pDS, NULL);
		if (hresult == DS_OK)
			break;
		if (hresult != DSERR_ALLOCATED) {
			Com_Printf (PRNT_ERROR, "FAILED!\n");
			goto error;
		}

		if (MessageBox (NULL,
						"The sound hardware is in use by another app.\n\n"
						"Select Retry to try to start sound again or Cancel to run EGL with no sound.",
						"Sound not available",
						MB_RETRYCANCEL|MB_SETFOREGROUND|MB_ICONEXCLAMATION) != IDRETRY) {
			Com_Printf (PRNT_ERROR, "FAILED! Hardware already in use!\n");
			goto error;
		}
	}

	dscaps.dwSize = sizeof(dscaps);
	if (snd_win.pDS->GetCaps (&dscaps) != DS_OK) {
		Com_Printf (PRNT_ERROR, "...GetCaps FAILED!\n");
		goto error;
	}

	if (dscaps.dwFlags & DSCAPS_EMULDRIVER) {
		Com_Printf (PRNT_ERROR, "...No DirectSound driver found!\n");
		goto error;
	}

	// Create buffers
	if (!DS_CreateBuffers (true))
		goto error;

	// Done
	snd_win.initialized = true;
	return true;

error:
	Com_Printf (PRNT_ERROR, "...error!\n");
	SndImp_Shutdown ();
	return false;
}


/*
==============
SndImp_Shutdown

Reset the sound device for exiting
===============
*/
void SndImp_Shutdown ()
{
	// Destroy DS buffers
	if (snd_win.pDS)
		DS_DestroyBuffers (true);

	// Release DS
	if (snd_win.pDS) {
		Com_Printf (0, "Releasing DirectSound object\n");
		snd_win.pDS->Release ();
	}

	// Free DS
	if (snd_win.hInstDS) {
		Com_Printf (0, "Freeing DSOUND.DLL\n");
		FreeLibrary (snd_win.hInstDS);
		snd_win.hInstDS = NULL;
	}

	// Clear old values
	snd_win.initialized = false;
	snd_win.lpData = NULL;
	snd_win.pDS = NULL;
	snd_win.pDSBuf = NULL;
	snd_win.pDSPBuf = NULL;
}

/*
==============================================================================

	MISC
 
==============================================================================
*/

/*
==============
SndImp_GetDMAPos

Return the current sample position (in mono samples read) inside the recirculating
dma buffer, so the mixing code will know how many sample are required to fill it up.
===============
*/
int SndImp_GetDMAPos ()
{
	int		s;
	MMTIME	mmtime;
	DWORD	dwWrite;

	if (!snd_win.initialized)
		return 0;

	mmtime.wType = TIME_SAMPLES;
	snd_win.pDSBuf->GetCurrentPosition (&mmtime.u.sample, &dwWrite);
	s = ((mmtime.u.sample - snd_win.mmStartTime.u.sample) >> snd_win.sample16) & (snd_audioDMA.samples-1);

	return s;
}


/*
==============
SndImp_BeginPainting

Makes sure snd_audioDMA.buffer is valid
===============
*/
void SndImp_BeginPainting ()
{
	int		reps;
	DWORD	dwSize2;
	DWORD	*pbuf, *pbuf2;
	HRESULT	hresult;
	DWORD	dwStatus;

	if (!snd_win.pDSBuf)
		return;

	// If the buffer was lost or stopped, restore it and/or restart it
	if (snd_win.pDSBuf->GetStatus (&dwStatus) != DS_OK)
		Com_Printf (PRNT_ERROR, "Couldn't get sound buffer status\n");
	
	if (dwStatus & DSBSTATUS_BUFFERLOST)
		snd_win.pDSBuf->Restore ();
	
	if (!(dwStatus & DSBSTATUS_PLAYING))
		snd_win.pDSBuf->Play (0, 0, DSBPLAY_LOOPING);

	// Lock the DirectSound buffer
	reps = 0;
	snd_audioDMA.buffer = NULL;
	for ( ; ; ) {
		hresult = snd_win.pDSBuf->Lock (0, snd_win.bufferSize, (LPVOID*)&pbuf, &snd_win.lockSize, (LPVOID*)&pbuf2, &dwSize2, 0);
		if (hresult == DS_OK)
			break;
		if (hresult != DSERR_BUFFERLOST) {
			Com_Printf (PRNT_ERROR, "SndImp_BeginPainting: Lock failed with error '%s'\n", GetDirectSoundErrorString (hresult));
			Snd_Shutdown ();
			return;
		}
		else {
			snd_win.pDSBuf->Restore ();
		}

		if (++reps > 2)
			return;
	}

	snd_audioDMA.buffer = (byte *)pbuf;
}


/*
==============
SndImp_Submit

Send sound to device if buffer isn't really the dma buffer
Also unlocks the DirectSound buffer
===============
*/
void SndImp_Submit ()
{
	// Unlock the DirectSound buffer
	if (snd_win.pDSBuf && snd_audioDMA.buffer)
		snd_win.pDSBuf->Unlock (snd_audioDMA.buffer, snd_win.lockSize, NULL, 0);
}


/*
===========
SndImp_Activate

Called when the main window gains or loses focus. The window have been
destroyed and recreated between a deactivate and an activate.
===========
*/
void SndImp_Activate(const bool bActive)
{
	Snd_Activate(bActive);
	if (!snd_win.pDS || !sys_winInfo.hWnd || !snd_win.initialized)
		return;

	if (bActive)
		DS_CreateBuffers(false);
	else
		DS_DestroyBuffers(false);
}
