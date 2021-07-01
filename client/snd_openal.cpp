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
// snd_openal.c
//

#include "snd_local.h"

#include "../include/openal/al.h"
#include "../include/openal/alc.h"

#if defined(WIN32)

# define WIN32_LEAN_AND_MEAN 1
# include <windows.h>

#elif defined(__unix__)

# include <dlfcn.h>
# include <unistd.h>
# include <sys/types.h>

#endif

static ALCdevice			*al_hDevice;
static ALCcontext			*al_hALC;

audioAL_t					snd_audioAL;
static channel_t			snd_alOutChannels[MAX_CHANNELS];

static vec3_t				snd_alOrigin;

/*
===========
ALSnd_CheckForError

Return true if there was an error.
===========
*/
static inline const char *GetALErrorString(ALenum error)
{
	switch (error)
	{
	case AL_NO_ERROR:			return "AL_NO_ERROR";
	case AL_INVALID_NAME:		return "AL_INVALID_NAME";
	case AL_INVALID_ENUM:		return "AL_INVALID_ENUM";
	case AL_INVALID_VALUE:		return "AL_INVALID_VALUE";
	case AL_INVALID_OPERATION:	return "AL_INVALID_OPERATION";
	case AL_OUT_OF_MEMORY:		return "AL_OUT_OF_MEMORY";
	}

	return "unknown";
}
bool ALSnd_CheckForError(char *where)
{
	ALenum	error;

	error = alGetError();
	if (error != AL_NO_ERROR)
	{
		Com_Printf(PRNT_ERROR, "AL_ERROR: alGetError(): '%s' (0x%x)", GetALErrorString(error), error);
		if (where)
			Com_Printf(0, " %s\n", where);
		else
			Com_Printf(0, "\n");
		return false;
	}

	return true;
}


/*
==================
ALSnd_Activate
==================
*/
void ALSnd_Activate(const bool bActive)
{
	alListenerf(AL_GAIN, bActive ? s_volume->floatVal * al_gain->floatVal : 0.0f);
}

/*
===============================================================================

	BUFFER MANAGEMENT

===============================================================================
*/

/*
==================
ALSnd_BufferFormat
==================
*/
static int ALSnd_BufferFormat(int width, int channels)
{
	switch(width)
	{
	case 1:
		if (channels == 1)
			return AL_FORMAT_MONO8;
		else if (channels == 2)
			return AL_FORMAT_STEREO8;
		break;

	case 2:
		if (channels == 1)
			return AL_FORMAT_MONO16;
		else if (channels == 2)
			return AL_FORMAT_STEREO16;
		break;
	}

	return AL_FORMAT_MONO16;
}


/*
==================
ALSnd_CreateBuffer
==================
*/
void ALSnd_CreateBuffer(sfxCache_t *sc, int width, int channels, byte *data, int size, int frequency)
{
	if (!sc)
		return;

	// Find the format
	sc->alFormat = ALSnd_BufferFormat(width, channels);

	// Upload
	alGenBuffers(1, &sc->alBufferNum);
	alBufferData(sc->alBufferNum, sc->alFormat, data, size, frequency);

	// Check
	if (!alIsBuffer(sc->alBufferNum))
		Com_Error(ERR_DROP, "ALSnd_CreateBuffer: created buffer is not valid!");
}


/*
==================
ALSnd_DeleteBuffer
==================
*/
void ALSnd_DeleteBuffer(sfxCache_t *sc)
{
	if (sc && sc->alBufferNum)
		alDeleteBuffers(1, &sc->alBufferNum);
}

/*
===============================================================================

	SOUND PLAYING

===============================================================================
*/

/*
==================
ALSnd_StartChannel
==================
*/
static void ALSnd_StartChannel(channel_t *ch, sfx_t *sfx)
{
	ch->sfx = sfx;

	alSourcei(ch->alSourceNum, AL_BUFFER, sfx->cache->alBufferNum);
	alSourcei(ch->alSourceNum, AL_LOOPING, ch->alLooping);
	switch(ch->psType)
	{
	case PSND_ENTITY:
	case PSND_FIXED:
		alSourcei(ch->alSourceNum, AL_SOURCE_RELATIVE, AL_FALSE);
		break;

	case PSND_LOCAL:
		alSourcei(ch->alSourceNum, AL_SOURCE_RELATIVE, AL_TRUE);
		alSourcefv(ch->alSourceNum, AL_POSITION, vec3Origin);
		alSourcefv(ch->alSourceNum, AL_VELOCITY, vec3Origin);
		alSourcef(ch->alSourceNum, AL_REFERENCE_DISTANCE, 0);
		alSourcef(ch->alSourceNum, AL_MAX_DISTANCE, 0);
		alSourcef(ch->alSourceNum, AL_ROLLOFF_FACTOR, 0);
		break;
	}

	alSourcePlay(ch->alSourceNum);
}


/*
==================
ALSnd_StopSound
==================
*/
static void ALSnd_StopSound(channel_t *ch)
{
	ch->sfx = NULL;

	alSourceStop(ch->alSourceNum);
	alSourcei(ch->alSourceNum, AL_BUFFER, 0);
}


/*
==================
ALSnd_StopAllSounds
==================
*/
void ALSnd_StopAllSounds()
{
	int		i;

	// Stop all sources
	for (i=0 ; i<snd_audioAL.numChannels ; i++)
	{
		if (!snd_alOutChannels[i].sfx)
			continue;

		ALSnd_StopSound(&snd_alOutChannels[i]);
	}

	// Stop raw streaming
	ALSnd_RawShutdown();

	// Reset frame count
	snd_audioAL.frameCount = 0;
}

/*
===============================================================================

	SPATIALIZATION

===============================================================================
*/

/*
==================
ALSnd_SpatializeChannel

Updates volume, distance, rolloff, origin, and velocity for a channel
If it's a local sound only the volume is updated
==================
*/
static void ALSnd_SpatializeChannel(channel_t *ch)
{
	float Volume = ch->alVolume;

	// Local sounds aren't positioned
	if (ch->psType != PSND_LOCAL)
	{
		vec3_t	position, velocity;

		// Get the sound's position
		if (ch->psType == PSND_FIXED)
		{
			// Fixed origin
			Vec3Copy(ch->origin, position);
			Vec3Clear(velocity);
		}
		else
		{
			// Entity origin
			if (ch->alLooping)
				CL_CGModule_GetEntitySoundOrigin(ch->alLoopEntNum, position, velocity);
			else
				CL_CGModule_GetEntitySoundOrigin(ch->entNum, position, velocity);
		}

		// Distance, rolloff
		if (ch->distMult)
		{
			// Add in an occlusion effect
			cmTrace_t tr = CM_Trace(position, snd_alOrigin, 1, CONTENTS_SOLID);
			if (tr.fraction < 1.0f)
			{
				Volume *= 0.5f + (0.25f * tr.fraction);
			}

			alSourcef(ch->alSourceNum, AL_REFERENCE_DISTANCE, al_minDistance->floatVal * ch->distMult);
		}
		else
		{
			alSourcef(ch->alSourceNum, AL_REFERENCE_DISTANCE, al_maxDistance->floatVal);
		}
		alSourcef(ch->alSourceNum, AL_MAX_DISTANCE, al_maxDistance->floatVal);
		alSourcef(ch->alSourceNum, AL_ROLLOFF_FACTOR, al_rollOffFactor->floatVal);

		alSource3f(ch->alSourceNum, AL_POSITION, position[1], position[2], -position[0]);
		alSource3f(ch->alSourceNum, AL_VELOCITY, velocity[1], velocity[2], -velocity[0]);
	}

	alSourcef(ch->alSourceNum, AL_PITCH, Cvar_GetFloatValue("timescale"));
	alSourcef(ch->alSourceNum, AL_GAIN, Volume);
}

/*
===============================================================================

	CHANNELS

===============================================================================
*/

/*
=================
ALSnd_PickChannel
=================
*/
static channel_t *ALSnd_PickChannel(int entNum, EEntSndChannel entChannel)
{
	int			i;
	int			firstToDie;
	int			oldest;
	channel_t	*ch;
	uint32		sourceNum;

	firstToDie = -1;
	oldest = cls.realTime;

	// Check for replacement sound, or find the best one to replace;
	for (i=0, ch=snd_alOutChannels ; i<snd_audioAL.numChannels ; ch++, i++)
	{
		// Never take over raw stream channels
		if (ch->alRawStream)
			continue;

		// Free channel
		if (!ch->sfx)
		{
			firstToDie = i;
			break;
		}

		// Channel 0 never overrides
		if (entChannel && ch->entNum == entNum && ch->entChannel == entChannel)
		{
			// Always override sound from same entity
			firstToDie = i;
			break;
		}

		// Don't let monster sounds override player sounds
		if (ch->entNum == cl.playerNum+1 && entNum != cl.playerNum+1 && ch->sfx)
			continue;

		// Replace the oldest sound
		if (ch->alStartTime < oldest)
		{
			oldest = ch->alStartTime;
			firstToDie = i;
		}
   }

	if (firstToDie == -1)
		return NULL;

	ch = &snd_alOutChannels[firstToDie];
	sourceNum = ch->alSourceNum;
	memset(ch, 0, sizeof(channel_t));

	ch->entNum = entNum;
	ch->entChannel = entChannel;
	ch->alStartTime = cls.realTime;
	ch->alSourceNum = sourceNum;

	// Stop the source
	ALSnd_StopSound(ch);

	return ch;
}

/*
===============================================================================

	PLAYSOUNDS

===============================================================================
*/

/*
===============
ALSnd_IssuePlaysounds
===============
*/
static void ALSnd_IssuePlaysounds()
{
	playSound_t	*ps;
	channel_t	*ch;

	for ( ; ; )
	{
		ps = snd_pendingPlays.next;
		if (ps == &snd_pendingPlays)
			break;	// No more pending sounds
		if (ps->beginTime > cls.realTime)
			break;	// No more pending sounds this frame

		// Pick a channel to play on
		ch = ALSnd_PickChannel(ps->entNum, ps->entChannel);
		if (!ch)
		{
			Snd_FreePlaysound(ps);
			return;
		}

		if (s_show->intVal)
			Com_Printf(0, "Issue %i\n", ps->beginTime);

		// Spatialize
		ch->alLooping = false;
		ch->alRawStream = false;
		ch->alVolume = ps->volume * (1.0f/255.0f);
		ch->entNum = ps->entNum;
		ch->entChannel = ps->entChannel;
		ch->sfx = ps->sfx;
		Vec3Copy(ps->origin, ch->origin);

		// Convert to a local sound if it's not supposed to attenuation
		if (ps->attenuation == ATTN_NONE)
			ch->psType = PSND_LOCAL;
		else
			ch->psType = ps->type;

		if (ps->attenuation != ATTN_NONE)
			ch->distMult = 1.0 / ps->attenuation;
		else
			ch->distMult = 0.0f;

		ALSnd_SpatializeChannel(ch);
		ALSnd_StartChannel(ch, ps->sfx);

		// Free the playsound
		Snd_FreePlaysound(ps);
	}
}

/*
===============================================================================

	RAW SAMPLING

	Cinematic streaming and voice over network

===============================================================================
*/

/*
===========
ALSnd_RawStart
===========
*/
channel_t *ALSnd_RawStart()
{
	channel_t	*ch;

	ch = ALSnd_PickChannel(0, CHAN_AUTO);
	if (!ch)
		Com_Error(ERR_FATAL, "ALSnd_RawStart: failed to allocate a source!");

	// Fill source values
	ch->psType = PSND_LOCAL;
	ch->alLooping = false;
	ch->alRawStream = true;
	ch->alVolume = 1;
	ch->sfx = NULL;

	// Spatialize
	alSourcef(ch->alSourceNum, AL_PITCH, 1);
	alSourcef(ch->alSourceNum, AL_GAIN, 1);
	alSourcei(ch->alSourceNum, AL_BUFFER, 0);
	alSourcei(ch->alSourceNum, AL_LOOPING, AL_FALSE);

	// Local sound
	alSourcei(ch->alSourceNum, AL_SOURCE_RELATIVE, AL_TRUE);
	alSourcefv(ch->alSourceNum, AL_POSITION, vec3Origin);
	alSourcefv(ch->alSourceNum, AL_VELOCITY, vec3Origin);
	alSourcef(ch->alSourceNum, AL_ROLLOFF_FACTOR, 0);

	return ch;
}


/*
============
ALSnd_RawSamples

Cinematic streaming and voice over network
============
*/
void ALSnd_RawSamples(channel_t *rawChannel, int samples, int rate, int width, int channels, byte *data)
{
	ALuint	buffer;
	ALuint	format;

	if (!rawChannel || !rawChannel->alRawStream)
		return;

	// Find the format
	format = ALSnd_BufferFormat(width, channels);

	// Generate a buffer
	alGenBuffers(1, &buffer);
	alBufferData(buffer, format, data, (samples * width * channels), rate);

	// Place in queue
	alSourceQueueBuffers(rawChannel->alSourceNum, 1, &buffer);
}


/*
===========
ALSnd_RawStop
===========
*/
void ALSnd_RawStop(channel_t *rawChannel)
{
	if (!rawChannel || !rawChannel->alRawStream)
		return;

	alSourceStop(rawChannel->alSourceNum);
	rawChannel->alRawPlaying = false;
	rawChannel->alRawStream = false;
}


/*
===========
ALSnd_RawShutdown
===========
*/
void ALSnd_RawShutdown()
{
	channel_t	*ch;
	int			i;

	// Stop all raw streaming channels
	for (i=0, ch=snd_alOutChannels ; i<snd_audioAL.numChannels ; ch++, i++)
	{
		if (!ch->alRawStream)
			continue;

		ALSnd_RawStop(ch);
	}
}


/*
===========
ALSnd_RawUpdate
===========
*/
static void ALSnd_RawUpdate(channel_t *rawChannel)
{
	int		processed;
	ALuint	buffer;
	ALint	state;

	if (!rawChannel || !rawChannel->alRawStream)
		return;

	// Delete processed buffers
	alGetSourcei(rawChannel->alSourceNum, AL_BUFFERS_PROCESSED, &processed);
	while (processed--)
	{
		alSourceUnqueueBuffers(rawChannel->alSourceNum, 1, &buffer);
		alDeleteBuffers(1, &buffer);
	}

	// Start the queued buffers
	alGetSourcei(rawChannel->alSourceNum, AL_BUFFERS_QUEUED, &processed);
	alGetSourcei(rawChannel->alSourceNum, AL_SOURCE_STATE, &state);
	if (state == AL_STOPPED)
	{
		if (processed)
		{
			alSourcePlay(rawChannel->alSourceNum);
			rawChannel->alRawPlaying = true;
		}
		else if (!rawChannel->alRawPlaying)
			ALSnd_RawStop(rawChannel);
	}
}

/*
===============================================================================

	CHANNEL MIXING

===============================================================================
*/

/*
===========
ALSnd_AddLoopSounds
===========
*/
static void ALSnd_AddLoopSounds()
{
	if (cl_paused->intVal || Com_ClientState() != CA_ACTIVE || !cls.soundPrepped)
		return;

	// Add looping entity sounds
	for (int i=0 ; i<cl.frame.numEntities ; i++)
	{
		entityState_t *ent = &cl_parseEntities[((cl.frame.parseEntities + i) & MAX_PARSEENTITIES_MASK)];
		if (!ent->sound)
			continue;

		if (!cl.soundCfgStrings[ent->sound] && cl.configStrings[CS_SOUNDS+ent->sound][0])
			cl.soundCfgStrings[ent->sound] = Snd_RegisterSound(cl.configStrings[CS_SOUNDS+ent->sound]);

		sfx_t *sfx = cl.soundCfgStrings[ent->sound];
		if (!sfx || !sfx->cache)
			continue;	// Bad sound effect

		// Update if already active
		int j;
		int byteOffset = 0;
		for (j=0 ; j<snd_audioAL.numChannels ; j++)
		{
			channel_t *ch = &snd_alOutChannels[j];
			if (ch->sfx != sfx)
				continue;

			alGetSourcei(ch->alSourceNum, AL_BYTE_OFFSET, &byteOffset);

			if (ch->alRawStream)
				continue;
			if (!ch->alLooping)
				continue;
			if (ch->alLoopEntNum != ent->number)
				continue;
			if (ch->alLoopFrame + 1 != snd_audioAL.frameCount)
				continue;

			ch->alLoopFrame = snd_audioAL.frameCount;
			break;
		}

		// Already active, and simply updated
		if (j != snd_audioAL.numChannels)
			continue;

		// Pick a channel to start the effect
		channel_t *ch = ALSnd_PickChannel(0, CHAN_AUTO);
		if (!ch)
			return;

		ch->alLooping = true;
		ch->alLoopEntNum = ent->number;
		ch->alLoopFrame = snd_audioAL.frameCount;
		ch->alRawStream = false;
		ch->alVolume = 1;
		ch->psType = PSND_ENTITY;
		ch->distMult = 1.0f / ATTN_STATIC;

		ALSnd_SpatializeChannel(ch);
		ALSnd_StartChannel(ch, sfx);
		alSourcei(ch->alSourceNum, AL_BYTE_OFFSET, byteOffset);
	}
}


/*
===========
ALSnd_Update
===========
*/
void ALSnd_Update(refDef_t *rd)
{
	channel_t	*ch;
	int			state;
	int			total;
	int			i;
	ALfloat		origin[3];
	ALfloat		velocity[3];
	ALfloat		orient[6];

	snd_audioAL.frameCount++;

	// Update our position, velocity, and orientation
	if (rd)
	{
		snd_alOrigin[0] = (ALfloat)rd->viewOrigin[0];
		snd_alOrigin[1] = (ALfloat)rd->viewOrigin[1];
		snd_alOrigin[2] = (ALfloat)rd->viewOrigin[2];

		velocity[0] = (ALfloat)rd->velocity[1];
		velocity[1] = (ALfloat)rd->velocity[2];
		velocity[2] = (ALfloat)rd->velocity[0] * -1;

		orient[0] = (ALfloat)rd->viewAxis[0][1];
		orient[1] = (ALfloat)rd->viewAxis[0][2] * -1;
		orient[2] = (ALfloat)rd->viewAxis[0][0] * -1;
		orient[3] = (ALfloat)rd->viewAxis[2][1];
		orient[4] = (ALfloat)rd->viewAxis[2][2] * -1;
		orient[5] = (ALfloat)rd->viewAxis[2][0] * -1;
	}
	else
	{
		snd_alOrigin[0] = 0.0f;
		snd_alOrigin[1] = 0.0f;
		snd_alOrigin[2] = 0.0f;

		velocity[0] = 0.0f;
		velocity[1] = 0.0f;
		velocity[2] = 0.0f;

		orient[0] = axisIdentity[0][1];
		orient[1] = axisIdentity[0][2] * -1;
		orient[2] = axisIdentity[0][0] * -1;
		orient[3] = axisIdentity[2][1];
		orient[4] = axisIdentity[2][2] * -1;
		orient[5] = axisIdentity[2][0] * -1;
	}

	origin[0] = snd_alOrigin[1];
	origin[1] = snd_alOrigin[2];
	origin[2] = snd_alOrigin[0] * -1;

	alListenerfv(AL_POSITION, origin);
	alListenerfv(AL_VELOCITY, velocity);
	alListenerfv(AL_ORIENTATION, orient);

	// Update doppler
	if (al_dopplerFactor->modified)
	{
		al_dopplerFactor->modified = false;
		alDopplerFactor(al_dopplerFactor->floatVal);
	}
	if (al_dopplerVelocity->modified)
	{
		al_dopplerVelocity->modified = false;
		alDopplerVelocity(al_dopplerVelocity->floatVal);
	}

	// Update listener volume
	alListenerf(AL_GAIN, snd_isActive ? s_volume->floatVal * al_gain->floatVal : 0);

	// Distance model
	alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);

	// Add loop sounds
	ALSnd_AddLoopSounds();

	// Add play sounds
	ALSnd_IssuePlaysounds();

	// Update channel spatialization
	total = 0;
	for (i=0, ch=snd_alOutChannels ; i<snd_audioAL.numChannels ; ch++, i++)
	{
		// Update streaming channels
		if (ch->alRawStream)
		{
			ALSnd_RawUpdate(ch);
		}
		else if (!ch->sfx)
			continue;

		// Stop inactive channels
		if (ch->alLooping)
		{
			if (ch->alLoopFrame != snd_audioAL.frameCount)
			{
				ALSnd_StopSound(ch);
				continue;
			}
			else if (!snd_isActive)
			{
				ch->alLoopFrame = snd_audioAL.frameCount - 1;
				ALSnd_StopSound(ch);
				continue;
			}
		}
		else if (!ch->alRawStream)
		{
			alGetSourcei(ch->alSourceNum, AL_SOURCE_STATE, &state);
			if (state == AL_STOPPED)
			{
				ALSnd_StopSound(ch);
				continue;
			}
		}

		// Debug output
		if (s_show->intVal && ch->alVolume)
		{
			Com_Printf(0, "%3i %s\n", i+1, ch->sfx->name);
			total++;
		}

		// Spatialize
		if (!ch->alRawStream)
			ALSnd_SpatializeChannel(ch);
	}

	// Debug output
	if (s_show->intVal)
		Com_Printf(0, "----(%i)----\n", total);

	// Check for errors
	if (al_errorCheck->intVal)
		ALSnd_CheckForError("ALSnd_Update");
}

/*
==============================================================================

	INIT / SHUTDOWN
 
==============================================================================
*/

/*
===========
ALSnd_Init
===========
*/
bool ALSnd_Init()
{
	char	*device;
	int		i;

	Com_Printf(0, "Initializing OpenAL\n");

	// Open the AL device
	device = al_device->string[0] ? al_device->string : NULL;
	al_hDevice = alcOpenDevice(device);
	Com_DevPrintf(0, "...opening device\n");
	if (!al_hDevice)
	{
		Com_Printf(PRNT_ERROR, "failed to open the device!\n");
		return false;
	}

	// Create the context and make it current
	al_hALC = alcCreateContext(al_hDevice, NULL);
	Com_DevPrintf(0, "...creating context\n");
	if (!al_hALC)
	{
		Com_Printf(PRNT_ERROR, "failed to create the context!\n");
		ALSnd_Shutdown();
		return false;
	}

	Com_DevPrintf(0, "...making context current\n");
	if (!alcMakeContextCurrent(al_hALC))
	{
		Com_Printf(PRNT_ERROR, "failed to make context current!\n");
		ALSnd_Shutdown();
		return false;
	}

	// Generate sources
	Com_DevPrintf(0, "...generating sources\n");
	snd_audioAL.numChannels = 0;
	for (i=0 ; i<MAX_CHANNELS ; i++)
	{
		alGenSources(1, &snd_alOutChannels[i].alSourceNum);
		if (alGetError() != AL_NO_ERROR)
			break;
		snd_audioAL.numChannels++;
	}
	if (!snd_audioAL.numChannels)
	{
		Com_Printf(PRNT_ERROR, "failed to generate sources!\n");
		ALSnd_Shutdown();
		return false;
	}
	Com_Printf(0, "...generated %i sources\n", snd_audioAL.numChannels);

	// Doppler
	Com_DevPrintf(0, "...setting doppler\n");
	alDopplerFactor(al_dopplerFactor->floatVal);
	alDopplerVelocity(al_dopplerVelocity->floatVal);

	al_dopplerFactor->modified = false;
	al_dopplerVelocity->modified = false;

	// Query some info
	snd_audioAL.extensionString = alGetString(AL_EXTENSIONS);
	snd_audioAL.rendererString = alGetString(AL_RENDERER);
	snd_audioAL.vendorString = alGetString(AL_VENDOR);
	snd_audioAL.versionString = alGetString(AL_VERSION);
	snd_audioAL.deviceName = alcGetString(al_hDevice, ALC_DEVICE_SPECIFIER);

	Com_DevPrintf(0, "Initialization successful\n");
	Com_Printf(0, "AL_VENDOR: %s\n", snd_audioAL.vendorString);
	Com_Printf(0, "AL_RENDERER: %s\n", snd_audioAL.rendererString);
	Com_Printf(0, "AL_VERSION: %s\n", snd_audioAL.versionString);
	Com_Printf(0, "AL_EXTENSIONS: %s\n", snd_audioAL.extensionString);
	Com_Printf(0, "ALC_DEVICE_SPECIFIER: %s\n", snd_audioAL.deviceName);
	return true;
}


/*
===========
ALSnd_Shutdown
===========
*/
void ALSnd_Shutdown()
{
	int		i;

	Com_Printf(0, "Shutting down OpenAL\n");

	// Make sure RAW is shutdown
	ALSnd_RawShutdown();

	// Free sources
	Com_Printf(0, "...releasing sources\n");
	for (i=0 ; i<snd_audioAL.numChannels ; i++)
	{
		alSourceStop(snd_alOutChannels[i].alSourceNum);
		alDeleteSources(1, &snd_alOutChannels[i].alSourceNum);
	}
	snd_audioAL.numChannels = 0;

	// Release the context
	if (al_hALC)
	{
		Com_Printf(0, "...releasing the context\n");
		alcMakeContextCurrent(NULL);

		Com_Printf(0, "...destroying the context\n");
		alcDestroyContext(al_hALC);

		al_hALC = NULL;
	}

	// Close the device
	if (al_hDevice)
	{
		Com_Printf(0, "...closing the device\n");
		alcCloseDevice(al_hDevice);

		al_hDevice = NULL;
	}
}
