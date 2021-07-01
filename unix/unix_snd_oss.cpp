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
// unix_snd_oss.c
//

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <linux/soundcard.h>
#include <stdio.h>

#include "../client/snd_local.h"
#include "unix_local.h"

static cVar_t	*s_oss_device;

static int		oss_audioFD = -1;
static char		*oss_curDevice;
static qBool	oss_initialized;
static int		oss_tryRates[] = {  48000, 44100, 22050, 11025, 8000 };

/*
==============
OSS_Init
===============
*/
qBool OSS_Init (void)
{
	int				rc;
    int				fmt;
	int				tmp;
    int				i;
    char			*s;
	struct audio_buf_info info;
	int				caps;
	extern uid_t	saved_euid;

	// Register variables
	if (!s_oss_device) {
		s_oss_device	= Cvar_Register ("s_oss_device",	"default",				CVAR_ARCHIVE);
	}

	if (oss_initialized)
		return qTrue;	// FIXME: can't snd_restart right now...

	// Find the target device
	if (oss_curDevice)
		Mem_Free (oss_curDevice);
	if (!Q_stricmp (s_oss_device->string, "default"))
		oss_curDevice = Mem_PoolStrDup ("/dev/dsp", cl_soundSysPool, 0);
	else
		oss_curDevice = Mem_PoolStrDup (s_oss_device->string, cl_soundSysPool, 0);

	// Open /dev/dsp, confirm capability to mmap, and get size of dma buffer
	if (oss_audioFD == -1) {
		seteuid (saved_euid);

		oss_audioFD = open (oss_curDevice, O_RDWR);

		seteuid (getuid ());

		if (oss_audioFD < 0) {
			perror (oss_curDevice);
			Com_Printf (PRNT_ERROR, "Could not open %s\n", oss_curDevice);
			return qFalse;
		}
	}

    rc = ioctl (oss_audioFD, SNDCTL_DSP_RESET, 0);
    if (rc < 0) {
		perror (oss_curDevice);
		Com_Printf (PRNT_ERROR, "Could not reset %s\n", oss_curDevice);
		close (oss_audioFD);
		return qFalse;
	}

	if (ioctl (oss_audioFD, SNDCTL_DSP_GETCAPS, &caps) == -1) {
		perror (oss_curDevice);
        Com_Printf (PRNT_ERROR, "Sound driver too old\n");
		close (oss_audioFD);
		return qFalse;
	}

	if (!(caps & DSP_CAP_TRIGGER) || !(caps & DSP_CAP_MMAP)) {
		Com_Printf (PRNT_ERROR, "Sorry but your soundcard can't do this\n");
		close (oss_audioFD);
		return qFalse;
	}

	// Set sample bits & speed
    snd_audioDMA.sampleBits = s_bits->intVal;
	if (snd_audioDMA.sampleBits != 16 && snd_audioDMA.sampleBits != 8) {
        ioctl (oss_audioFD, SNDCTL_DSP_GETFMTS, &fmt);
        if (fmt & AFMT_S16_LE)
			snd_audioDMA.sampleBits = 16;
        else if (fmt & AFMT_U8)
			snd_audioDMA.sampleBits = 8;
    }

    snd_audioDMA.speed = s_speed->intVal;
	switch (s_khz->intVal) {
	case 48:	snd_audioDMA.speed = 48000;	break;
	case 44:	snd_audioDMA.speed = 44100;	break;
	case 22:	snd_audioDMA.speed = 22050;	break;
	default:	snd_audioDMA.speed = 11025;	break;
	}
    if (!snd_audioDMA.speed) {
        for (i=0 ; i<sizeof(oss_tryRates)/4 ; i++)
            if (!ioctl (oss_audioFD, SNDCTL_DSP_SPEED, &oss_tryRates[i])) break;
        snd_audioDMA.speed = oss_tryRates[i];
    }

	snd_audioDMA.channels = s_channels->intVal;
	if (snd_audioDMA.channels < 1 || snd_audioDMA.channels > 2)
		snd_audioDMA.channels = 2;

	tmp = 0;
	if (snd_audioDMA.channels == 2)
		tmp = 1;
    rc = ioctl (oss_audioFD, SNDCTL_DSP_STEREO, &tmp);
    if (rc < 0) {
		perror (oss_curDevice);
        Com_Printf (PRNT_ERROR, "Could not set %s to stereo=%d", oss_curDevice, snd_audioDMA.channels);
		close (oss_audioFD);
        return qFalse;
    }
	if (tmp)
		snd_audioDMA.channels = 2;
	else
		snd_audioDMA.channels = 1;

    if (snd_audioDMA.sampleBits == 16) {
        rc = AFMT_S16_LE;
        rc = ioctl (oss_audioFD, SNDCTL_DSP_SETFMT, &rc);
        if (rc < 0) {
			perror (oss_curDevice);
			Com_Printf (PRNT_ERROR, "Could not support 16-bit data.  Try 8-bit.\n");
			close (oss_audioFD);
			return qFalse;
		}
    }
	else if (snd_audioDMA.sampleBits == 8) {
        rc = AFMT_U8;
        rc = ioctl (oss_audioFD, SNDCTL_DSP_SETFMT, &rc);
        if (rc < 0) {
			perror (oss_curDevice);
			Com_Printf (PRNT_ERROR, "Could not support 8-bit data.\n");
			close (oss_audioFD);
			return qFalse;
		}
    }
	else {
		perror (oss_curDevice);
		Com_Printf (PRNT_ERROR, "%d-bit sound not supported.", snd_audioDMA.sampleBits);
		close (oss_audioFD);
		return qFalse;
	}

    rc = ioctl (oss_audioFD, SNDCTL_DSP_SPEED, &snd_audioDMA.speed);
    if (rc < 0) {
		perror (oss_curDevice);
        Com_Printf (PRNT_ERROR, "Could not set %s speed to %d", oss_curDevice, snd_audioDMA.speed);
		close (oss_audioFD);
        return qFalse;
    }

    if (ioctl (oss_audioFD, SNDCTL_DSP_GETOSPACE, &info) == -1) {   
        perror ("GETOSPACE");
		Com_Printf (PRNT_ERROR, "Um, can't do GETOSPACE?\n");
		close (oss_audioFD);
		return qFalse;
    }

	snd_audioDMA.samples = info.fragstotal * info.fragsize / (snd_audioDMA.sampleBits/8);
	snd_audioDMA.submissionChunk = 1;

	// Memory map the dma buffer
	if (!snd_audioDMA.buffer)
		snd_audioDMA.buffer = (unsigned char *) mmap(NULL, info.fragstotal
			* info.fragsize, PROT_WRITE, MAP_FILE|MAP_SHARED, oss_audioFD, 0);

	if (!snd_audioDMA.buffer) {
		perror (oss_curDevice);
		Com_Printf (PRNT_ERROR, "Could not mmap %s\n", oss_curDevice);
		close (oss_audioFD);
		return qFalse;
	}

	// Toggle the trigger & start her up
    tmp = 0;
    rc  = ioctl (oss_audioFD, SNDCTL_DSP_SETTRIGGER, &tmp);
	if (rc < 0) {
		perror (oss_curDevice);
		Com_Printf (PRNT_ERROR, "Could not toggle.\n");
		close (oss_audioFD);
		return qFalse;
	}

    tmp = PCM_ENABLE_OUTPUT;
    rc = ioctl (oss_audioFD, SNDCTL_DSP_SETTRIGGER, &tmp);
	if (rc < 0) {
		perror (oss_curDevice);
		Com_Printf (PRNT_ERROR, "Could not toggle.\n");
		close (oss_audioFD);
		return qFalse;
	}

	snd_audioDMA.samplePos = 0;

	Com_Printf (0,"Stereo: %d\n", snd_audioDMA.channels - 1);
	Com_Printf (0,"Samples: %d\n", snd_audioDMA.samples);
	Com_Printf (0,"Samplepos: %d\n", snd_audioDMA.samplePos);
	Com_Printf (0,"Samplebits: %d\n", snd_audioDMA.sampleBits);
	Com_Printf (0,"Submission_chunk: %d\n", snd_audioDMA.submissionChunk);
	Com_Printf (0,"Speed: %d\n", snd_audioDMA.speed);

	oss_initialized = qTrue;
	Snd_Activate (qTrue);
	return qTrue;
}


/*
==============
OSS_Shutdown
===============
*/
void OSS_Shutdown (void)
{
/*
	FIXME: some problem when really shutting down here

	Snd_Activate (qFalse);
	if (oss_audioFD != -1) {
		close (oss_audioFD);
		oss_audioFD = -1;
	}
*/
}


/*
==============
OSS_GetDMAPos
===============
*/
int OSS_GetDMAPos (void)
{
	struct count_info count;

	if (!oss_initialized)
		return 0;

	if (ioctl (oss_audioFD, SNDCTL_DSP_GETOPTR, &count) == -1) {
		perror (oss_curDevice);
		Com_Printf (PRNT_ERROR, "SndImp_GetDMAPos: Uh, sound dead.\n");
		close (oss_audioFD);
		oss_initialized = qFalse;
		return 0;
	}

	snd_audioDMA.samplePos = count.ptr / (snd_audioDMA.sampleBits / 8);

	return snd_audioDMA.samplePos;
}

/*
==============
OSS_GetDMAPos
===============
*/
void OSS_BeginPainting (void)
{
}


/*
==============
OSS_GetDMAPos
===============
*/
void OSS_Submit (void)
{
}
