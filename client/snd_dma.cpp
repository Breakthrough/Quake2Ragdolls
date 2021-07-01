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
// snd_dma.c
// Main control for any streaming sound output device
//

#include "snd_local.h"

struct sfxSamplePair_t {
	int				left;
	int				right;
};

audioDMA_t				snd_audioDMA;

// Audio channels
static channel_t		snd_dmaOutChannels[MAX_CHANNELS];

// Raw sampling
#define MAX_RAW_SAMPLES	8192
static int				snd_dmaRawEnd;
static sfxSamplePair_t	snd_dmaRawSamples[MAX_RAW_SAMPLES];

// Buffer painting
#define SND_PBUFFER		2048
static sfxSamplePair_t	snd_dmaPaintBuffer[SND_PBUFFER];
static int				snd_dmaScaleTable[32][256];
static int				*snd_dmaMixPointer;
static int				snd_dmaLinearCount;
static sint16			*snd_dmaBufferOutput;

static int				snd_dmaSoundTime;	// sample PAIRS
int						snd_dmaPaintedTime;	// sample PAIRS

// Orientation
static vec3_t			snd_dmaOrigin;
static vec3_t			snd_dmaRightVec;

/*
================
DMASnd_ScaleTableInit
================
*/
static void DMASnd_ScaleTableInit (float volume)
{
	int		i, j;
	int		scale;

	if (volume == 0.0f) {
		memset (snd_dmaScaleTable, 0, sizeof(snd_dmaScaleTable));
		return;
	}

	for (i=0 ; i<32 ; i++) {
		scale = i * 8 * 256 * volume;
		for (j=0 ; j<256 ; j++)
			snd_dmaScaleTable[i][j] = ((signed char)j) * scale;
	}
}

/*
===============================================================================

	SPATIALIZATION

===============================================================================
*/

/*
=================
DMASnd_SpatializeOrigin

Used for spatializing channels and autosounds
=================
*/
static void DMASnd_SpatializeOrigin (vec3_t origin, float masterVol, float distMult, int &leftVol, int &rightVol)
{
	float		dot, dist;
	float		leftScale, rightScale, scale;
	vec3_t		sourceVec;

	if (Com_ClientState () != CA_ACTIVE)
	{
		leftVol = 255;
		rightVol = 255;
		return;
	}

	// Calculate stereo seperation and distance attenuation
	Vec3Subtract (origin, snd_dmaOrigin, sourceVec);

	dist = VectorNormalizef (sourceVec, sourceVec) - SOUND_FULLVOLUME;
	if (dist < 0)
		dist = 0;			// close enough to be at full volume
	dist *= distMult;		// different attenuation levels
	
	dot = DotProduct (snd_dmaRightVec, sourceVec);

	if (snd_audioDMA.channels == 1 || !distMult)
	{
		// No attenuation = no spatialization
		rightScale = 1.0f;
		leftScale = 1.0f;
	}
	else
	{
		rightScale = 0.5f * (1.0f + dot);
		leftScale = 0.5f * (1.0f - dot);
	}

	// Add in distance effect
	scale = (1.0 - dist) * rightScale;
	rightVol = Q_rint (masterVol * scale);

	scale = (1.0 - dist) * leftScale;
	leftVol = Q_rint (masterVol * scale);

	// Add in an occlusion effect
	cmTrace_t tr = CM_Trace(origin, snd_dmaOrigin, 1, CONTENTS_SOLID);
	if (tr.fraction < 1.0f)
	{
		scale = 0.5f + (0.25f * tr.fraction);
		rightVol = Q_rint(rightVol * scale);
		leftVol = Q_rint(leftVol * scale);
	}

	// Clamp outputs
	if (rightVol < 0)
		rightVol = 0;
	if (leftVol < 0)
		leftVol = 0;
}


/*
=================
DMASnd_SpatializeChannel
=================
*/
static void DMASnd_SpatializeChannel (channel_t *ch)
{
	vec3_t		origin, velocity;

	switch (ch->psType) {
	case PSND_FIXED:
		Vec3Copy (ch->origin, origin);
		break;

	case PSND_ENTITY:
		CL_CGModule_GetEntitySoundOrigin (ch->entNum, origin, velocity);
		break;

	case PSND_LOCAL:
		// Anything coming from the view entity will always be full volume
		ch->leftVol = ch->masterVol;
		ch->rightVol = ch->masterVol;
		return;
	}

	// Spatialize fixed/entity sounds
	DMASnd_SpatializeOrigin (origin, ch->masterVol, ch->distMult, ch->leftVol, ch->rightVol);
}

/*
===============================================================================

	CHANNELS

===============================================================================
*/

/*
=================
DMASnd_PickChannel
=================
*/
static channel_t *DMASnd_PickChannel (int entNum, EEntSndChannel entChannel)
{
	int			i;
	int			firstToDie;
	int			lifeLeft;
	channel_t	*ch;

	firstToDie = -1;
	lifeLeft = 0x7fffffff;

	// Check for replacement sound, or find the best one to replace
	for (i=0, ch=snd_dmaOutChannels ; i<MAX_CHANNELS ; ch++, i++) {
		// Channel 0 never overrides
		if (entChannel && ch->entNum == entNum && ch->entChannel == entChannel) {
			// Always override sound from same entity
			firstToDie = i;
			break;
		}

		// Don't let monster sounds override player sounds
		if (ch->entNum == cl.playerNum+1 && entNum != cl.playerNum+1 && ch->sfx)
			continue;

		// Replace the oldest sound
		if (ch->endTime - snd_dmaPaintedTime < lifeLeft) {
			lifeLeft = ch->endTime - snd_dmaPaintedTime;
			firstToDie = i;
		}
   }

	if (firstToDie == -1)
		return NULL;

	ch = &snd_dmaOutChannels[firstToDie];
	memset (ch, 0, sizeof(channel_t));
	return ch;
}

/*
===============================================================================

	PLAYSOUNDS

===============================================================================
*/

/*
===============
DMASnd_IssuePlaysound

Take the next playsound and begin it on the channel. This is never
called directly by Snd_Play*, but only by the update loop.
===============
*/
static void DMASnd_IssuePlaysounds (int endTime)
{
	channel_t	*ch;
	sfxCache_t	*sc;
	playSound_t	*ps;

	for ( ; ; ) {
		ps = snd_pendingPlays.next;
		if (ps == &snd_pendingPlays)
			break;	// No more pending sounds
		if (ps->beginTime > snd_dmaPaintedTime) {
			if (ps->beginTime < endTime)
				endTime = ps->beginTime;	// Stop here
			break;
		}

		if (s_show->intVal)
			Com_Printf (0, "Issue %i\n", ps->beginTime);

		// Pick a channel to play on
		ch = DMASnd_PickChannel (ps->entNum, ps->entChannel);
		if (!ch) {
			Snd_FreePlaysound (ps);
			return;
		}

		// Spatialize
		if (ps->attenuation == ATTN_STATIC)
			ch->distMult = ps->attenuation * 0.001f;
		else
			ch->distMult = ps->attenuation * 0.0005f;

		ch->masterVol = ps->volume;
		ch->entNum = ps->entNum;
		ch->entChannel = ps->entChannel;
		ch->sfx = ps->sfx;
		Vec3Copy (ps->origin, ch->origin);
		ch->psType = ps->type;

		DMASnd_SpatializeChannel (ch);

		ch->position = 0;
		sc = Snd_LoadSound (ch->sfx);
		ch->endTime = snd_dmaPaintedTime + sc->length;

		// Free the playsound
		Snd_FreePlaysound (ps);
	}
}

/*
===============================================================================

	SOUND PLAYING

===============================================================================
*/

/*
==================
DMASnd_ClearBuffer
==================
*/
static void DMASnd_ClearBuffer ()
{
	int		clear;

	// Clear the buffers
	snd_dmaRawEnd = 0;
	if (snd_audioDMA.sampleBits == 8)
		clear = 0x80;
	else
		clear = 0;

	SndImp_BeginPainting ();
	if (snd_audioDMA.buffer)
		memset (snd_audioDMA.buffer, clear, snd_audioDMA.samples * snd_audioDMA.sampleBits/8);
	SndImp_Submit ();
}


/*
==================
DMASnd_StopAllSounds
==================
*/
void DMASnd_StopAllSounds ()
{
	// Clear all the channels
	memset (snd_dmaOutChannels, 0, sizeof(snd_dmaOutChannels));

	// Clear the buffers
	DMASnd_ClearBuffer ();
}

/*
===============================================================================

	CHANNEL MIXING

===============================================================================
*/

/*
==================
DMASnd_AddLoopSounds

Entities with an ent->sound field will generated looped sounds that are automatically
started, stopped, and merged together as the entities are sent to the client
==================
*/
static void DMASnd_AddLoopSounds ()
{
	int				left, right;
	int				leftTotal, rightTotal;
	channel_t		*ch;
	sfx_t			*sfx;
	sfxCache_t		*sc;
	entityState_t	*ent;
	vec3_t			origin, velocity;
	bool			bDone[MAX_PARSE_ENTITIES];

	if (cl_paused->intVal || Com_ClientState () != CA_ACTIVE || !cls.soundPrepped)
		return;

	memset(&bDone, false, sizeof(bool)*MAX_PARSE_ENTITIES);

	// Add sounds
	for (int i=0 ; i<cl.frame.numEntities ; i++)
	{
		int entIdx = (cl.frame.parseEntities + i)&(MAX_PARSEENTITIES_MASK);
		ent = &cl_parseEntities[entIdx];
		if (!ent->sound || bDone[entIdx])
			continue;

		if (!cl.soundCfgStrings[ent->sound] && cl.configStrings[CS_SOUNDS+ent->sound][0])
			cl.soundCfgStrings[ent->sound] = Snd_RegisterSound (cl.configStrings[CS_SOUNDS+ent->sound]);

		sfx = cl.soundCfgStrings[ent->sound];
		if (!sfx)
			continue;	// Bad sound effect

		sc = sfx->cache;
		if (!sc)
			continue;

		// Get the entity sound origin
		CL_CGModule_GetEntitySoundOrigin (ent->number, origin, velocity);

		// Find the total contribution of all sounds of this type
		DMASnd_SpatializeOrigin (origin, 255.0f, SOUND_LOOPATTENUATE, leftTotal, rightTotal);
		bDone[entIdx] = true;

		int NumCombined = 1;
		for (int j=i+1 ; j<cl.frame.numEntities ; j++)
		{
			int otherIdx = (cl.frame.parseEntities + j)&(MAX_PARSEENTITIES_MASK);
			entityState_t *other = &cl_parseEntities[otherIdx];

			if (other->sound != ent->sound || bDone[otherIdx])
				continue;

			CL_CGModule_GetEntitySoundOrigin (other->number, origin, velocity);
			DMASnd_SpatializeOrigin (origin, 255.0f, SOUND_LOOPATTENUATE, left, right);
			bDone[otherIdx] = true;

			leftTotal += left;
			rightTotal += right;
			NumCombined++;
		}

		// Average out the result
		if (NumCombined > 1)
		{
			float Avg = 1.0f / (float)NumCombined;
			leftTotal *= Avg;
			rightTotal *= Avg;
		}

		if (leftTotal == 0 && rightTotal == 0)
			continue;	// Not audible

		// Allocate a channel
		ch = DMASnd_PickChannel (0, CHAN_AUTO);
		if (!ch)
			return;

		// Clamp high
		if (leftTotal > 255)
			leftTotal = 255;
		if (rightTotal > 255)
			rightTotal = 255;

		ch->leftVol = leftTotal;
		ch->rightVol = rightTotal;
		ch->autoSound = true;	// Remove next frame
		ch->sfx = sfx;
		ch->position = snd_dmaPaintedTime % sc->length;
		ch->endTime = snd_dmaPaintedTime + sc->length - ch->position;
	}
}


/*
================
DMASnd_PaintChannelFrom8
================
*/
static void DMASnd_PaintChannelFrom8 (channel_t *ch, sfxCache_t *sc, int count, int offset)
{
	int		data;
	int		*lScale, *rScale;
	byte	*sfx;
	int		i;
	sfxSamplePair_t	*samp;

	// Clamp
	if (ch->leftVol > 255)
		ch->leftVol = 255;
	if (ch->rightVol > 255)
		ch->rightVol = 255;

	// Left/right scale
	lScale = snd_dmaScaleTable[ch->leftVol >> 3];
	rScale = snd_dmaScaleTable[ch->rightVol >> 3];
	sfx = (byte *)sc->data + ch->position;

	samp = &snd_dmaPaintBuffer[offset];
	for (i=0 ; i<count ; i++, samp++) {
		data = sfx[i];
		samp->left += lScale[data];
		samp->right += rScale[data];
	}
	
	ch->position += count;
}


/*
================
DMASnd_PaintChannelFrom16
================
*/
static void DMASnd_PaintChannelFrom16 (channel_t *ch, sfxCache_t *sc, int count, int offset, float volume)
{
	int				data, i;
	int				left, right;
	int				leftVol, rightVol;
	signed short	*sfx;
	sfxSamplePair_t	*samp;

	leftVol = ch->leftVol * volume * 256;
	rightVol = ch->rightVol * volume * 256;
	sfx = (signed short *)sc->data + ch->position;

	samp = &snd_dmaPaintBuffer[offset];
	for (i=0 ; i<count ; i++, samp++) {
		data = sfx[i];
		left = (data * leftVol)>>8;
		right = (data * rightVol)>>8;
		samp->left += left;
		samp->right += right;
	}

	ch->position += count;
}


/*
===================
DMASnd_TransferPaintBuffer
===================
*/
static void DMASnd_TransferPaintBuffer (int endTime)
{
	int		count;
	int		val;
	int		i;
	uint32	*pbuf;

	pbuf = (uint32 *)snd_audioDMA.buffer;

	if (s_testsound->intVal) {
		// Write a fixed sine wave
		count = (endTime - snd_dmaPaintedTime);
		for (i=0 ; i<count ; i++)
			snd_dmaPaintBuffer[i].left = snd_dmaPaintBuffer[i].right = sinf((snd_dmaPaintedTime+i)*0.1)*20000*256;
	}

	if (snd_audioDMA.sampleBits == 16 && snd_audioDMA.channels == 2) {
		int		pos;
		int		paintedTime;
		
		// Optimized case
		snd_dmaMixPointer = (int *) snd_dmaPaintBuffer;
		paintedTime = snd_dmaPaintedTime;

		while (paintedTime < endTime) {
			// Handle recirculating buffer issues
			pos = paintedTime & ((snd_audioDMA.samples>>1)-1);

			snd_dmaBufferOutput = (sint16 *) pbuf + (pos<<1);
			snd_dmaLinearCount = (snd_audioDMA.samples>>1) - pos;
			if (paintedTime + snd_dmaLinearCount > endTime)
				snd_dmaLinearCount = endTime - paintedTime;

			snd_dmaLinearCount <<= 1;

			// Write a linear blast of samples
			for (i=0 ; i<snd_dmaLinearCount ; i+=2) {
				val = snd_dmaMixPointer[i]>>8;
				if (val > 0x7fff)
					snd_dmaBufferOutput[i] = 0x7fff;
				else if (val < (sint16)0x8000)
					snd_dmaBufferOutput[i] = (sint16)0x8000;
				else
					snd_dmaBufferOutput[i] = val;

				val = snd_dmaMixPointer[i+1]>>8;
				if (val > 0x7fff)
					snd_dmaBufferOutput[i+1] = 0x7fff;
				else if (val < (sint16)0x8000)
					snd_dmaBufferOutput[i+1] = (sint16)0x8000;
				else
					snd_dmaBufferOutput[i+1] = val;
			}

			snd_dmaMixPointer += snd_dmaLinearCount;
			paintedTime += (snd_dmaLinearCount>>1);
		}
	}
	else {
		int		outMask;
		int		outIndex;
		int		step;
		int		*p;

		// General case
		p = (int *) snd_dmaPaintBuffer;
		count = (endTime - snd_dmaPaintedTime) * snd_audioDMA.channels;
		outMask = snd_audioDMA.samples - 1; 
		outIndex = snd_dmaPaintedTime * snd_audioDMA.channels & outMask;
		step = 3 - snd_audioDMA.channels;

		if (snd_audioDMA.sampleBits == 16) {
			sint16 *out = (sint16 *) pbuf;
			while (count--) {
				val = *p >> 8;
				p += step;

				if (val > 0x7fff)
					val = 0x7fff;
				else if (val < (sint16)0x8000)
					val = (sint16)0x8000;
				out[outIndex] = val;
				outIndex = (outIndex + 1) & outMask;
			}
		}
		else if (snd_audioDMA.sampleBits == 8) {
			byte	*out = (byte *) pbuf;
			while (count--) {
				val = *p >> 8;
				p += step;

				if (val > 0x7fff)
					val = 0x7fff;
				else if (val < (sint16)0x8000)
					val = (sint16)0x8000;
				out[outIndex] = (val>>8) + 128;
				outIndex = (outIndex + 1) & outMask;
			}
		}
	}
}


/*
================
DMASnd_PaintChannels
================
*/
static void DMASnd_PaintChannels (int endTime, float volume)
{
	channel_t	*ch;
	sfxCache_t	*sc;
	int			lTime, count;
	int			newEnd, i;

	while (snd_dmaPaintedTime < endTime) {
		// If snd_dmaPaintBuffer is smaller than DMA buffer
		newEnd = endTime;
		if (endTime - snd_dmaPaintedTime > SND_PBUFFER)
			newEnd = snd_dmaPaintedTime + SND_PBUFFER;

		// Start any playsounds
		DMASnd_IssuePlaysounds (newEnd);

		// Clear the paint buffer
		if (snd_dmaRawEnd < snd_dmaPaintedTime) {
			memset (snd_dmaPaintBuffer, 0, (newEnd - snd_dmaPaintedTime) * sizeof(sfxSamplePair_t));
		}
		else {
			// Copy from the streaming sound source
			int		stop, s;

			stop = (newEnd < snd_dmaRawEnd) ? newEnd : snd_dmaRawEnd;

			for (i=snd_dmaPaintedTime ; i<stop ; i++) {
				s = i & (MAX_RAW_SAMPLES-1);
				snd_dmaPaintBuffer[i-snd_dmaPaintedTime].left = snd_dmaRawSamples[s].left * volume;
				snd_dmaPaintBuffer[i-snd_dmaPaintedTime].right = snd_dmaRawSamples[s].right * volume;
			}

			for ( ; i<newEnd ; i++) {
				snd_dmaPaintBuffer[i-snd_dmaPaintedTime].left = 0;
				snd_dmaPaintBuffer[i-snd_dmaPaintedTime].right = 0;
			}
		}

		// Paint in the channels
		for (i=0, ch=snd_dmaOutChannels ; i<MAX_CHANNELS ; ch++, i++) {
			lTime = snd_dmaPaintedTime;
		
			while (lTime < newEnd) {
				if (!ch->sfx || (!ch->leftVol && !ch->rightVol))
					break;

				// Max painting is to the end of the buffer
				count = newEnd - lTime;

				// Might be stopped by running out of data
				if (ch->endTime - lTime < count)
					count = ch->endTime - lTime;
		
				sc = Snd_LoadSound (ch->sfx);
				if (!sc)
					break;

				if (count > 0 && ch->sfx) {	
					if (sc->width == 1)
						DMASnd_PaintChannelFrom8 (ch, sc, count, lTime - snd_dmaPaintedTime);
					else
						DMASnd_PaintChannelFrom16 (ch, sc, count, lTime - snd_dmaPaintedTime, volume);
	
					lTime += count;
				}

				// If at end of loop, restart
				if (lTime >= ch->endTime) {
					if (ch->autoSound) {
						// Autolooping sounds always go back to start
						ch->position = 0;
						ch->endTime = lTime + sc->length;
					}
					else if (sc->loopStart >= 0) {
						ch->position = sc->loopStart;
						ch->endTime = lTime + sc->length - ch->position;
					}
					else {
						// Channel just stopped
						ch->sfx = NULL;
					}
				}
			}
															  
		}

		// Transfer out according to DMA format
		DMASnd_TransferPaintBuffer (newEnd);
		snd_dmaPaintedTime = newEnd;
	}
}


/*
============
DMASnd_RawSamples

Cinematic streaming and voice over network
============
*/
void DMASnd_RawSamples (int samples, int rate, int width, int channels, byte *data)
{
	int		i;
	int		src, dst;
	float	scale;

	if (snd_dmaRawEnd < snd_dmaPaintedTime)
		snd_dmaRawEnd = snd_dmaPaintedTime;
	scale = (float)rate / snd_audioDMA.speed;

	switch (channels) {
	case 1:
		switch (width) {
		case 1:
			for (i=0 ; ; i++) {
				src = i*scale;
				if (src >= samples)
					break;
				dst = snd_dmaRawEnd & (MAX_RAW_SAMPLES-1);
				snd_dmaRawEnd++;
				snd_dmaRawSamples[dst].left = (((byte *)data)[src]-128) << 16;
				snd_dmaRawSamples[dst].right = (((byte *)data)[src]-128) << 16;
			}
			break;

		case 2:
			for (i=0 ; ; i++) {
				src = i*scale;
				if (src >= samples)
					break;
				dst = snd_dmaRawEnd & (MAX_RAW_SAMPLES-1);
				snd_dmaRawEnd++;
				snd_dmaRawSamples[dst].left = LittleShort(((sint16 *)data)[src]) << 8;
				snd_dmaRawSamples[dst].right = LittleShort(((sint16 *)data)[src]) << 8;
			}
			break;
		}
		break;

	case 2:
		switch (width) {
		case 1:
			for (i=0 ; ; i++) {
				src = i*scale;
				if (src >= samples)
					break;
				dst = snd_dmaRawEnd & (MAX_RAW_SAMPLES-1);
				snd_dmaRawEnd++;
				snd_dmaRawSamples[dst].left = ((char *)data)[src*2] << 16;
				snd_dmaRawSamples[dst].right = ((char *)data)[src*2+1] << 16;
			}
			break;

		case 2:
			if (scale == 1.0) {
				for (i=0 ; i<samples ; i++) {
					dst = snd_dmaRawEnd & (MAX_RAW_SAMPLES-1);
					snd_dmaRawEnd++;
					snd_dmaRawSamples[dst].left = LittleShort(((sint16 *)data)[i*2]) << 8;
					snd_dmaRawSamples[dst].right = LittleShort(((sint16 *)data)[i*2+1]) << 8;
				}
			}
			else {
				for (i=0 ; ; i++) {
					src = i*scale;
					if (src >= samples)
						break;

					dst = snd_dmaRawEnd & (MAX_RAW_SAMPLES-1);
					snd_dmaRawEnd++;
					snd_dmaRawSamples[dst].left = LittleShort(((sint16 *)data)[src*2]) << 8;
					snd_dmaRawSamples[dst].right = LittleShort(((sint16 *)data)[src*2+1]) << 8;
				}
			}
			break;
		}
		break;
	}
}


/*
============
DMASnd_Update

Called once each time through the main loop
============
*/
void DMASnd_Update (refDef_t *rd)
{
	int			total, i;
	uint32		endTime, samples;
	channel_t	*ch;
	int			samplePos;
	static int	oldSamplePos;
	static int	buffers;
	int			fullSamples;
	float		tempVol;

	if (rd)
	{
		Vec3Copy (rd->viewOrigin, snd_dmaOrigin);
		Vec3Negate (rd->viewAxis[1], snd_dmaRightVec);
	}
	else
	{
		Vec3Clear (snd_dmaOrigin);
		Vec3Clear (snd_dmaRightVec);
	}

	// Don't play sounds while the screen is disabled
	if (cls.disableScreen || !snd_isActive || s_volume->floatVal == 0.0f)
	{
		DMASnd_ClearBuffer ();
		tempVol = 0.0f;
	}
	else
	{
		tempVol = s_volume->floatVal;

		// Rebuild scale tables if volume is modified
		if (s_volume->modified)
		{
			s_volume->modified = false;
			DMASnd_ScaleTableInit (s_volume->floatVal);
		}
	}

	// Update spatialization for dynamic sounds
	for (i=0, ch=snd_dmaOutChannels ; i<MAX_CHANNELS ; ch++, i++)
	{
		if (!ch->sfx)
			continue;

		if (ch->autoSound)
		{
			// Autosounds are regenerated fresh each frame
			memset (ch, 0, sizeof(channel_t));
			continue;
		}

		// Respatialize channel
		DMASnd_SpatializeChannel (ch);
		if (!ch->leftVol && !ch->rightVol)
		{
			memset (ch, 0, sizeof(channel_t));
			continue;
		}
	}

	// Add loopsounds
	DMASnd_AddLoopSounds ();

	// Debugging output
	if (s_show->intVal)
	{
		total = 0;
		for (i=0, ch=snd_dmaOutChannels ; i<MAX_CHANNELS ; ch++, i++)
		{
			if (ch->sfx && (ch->leftVol || ch->rightVol))
			{
				Com_Printf (0, "%3i %3i %s\n", ch->leftVol, ch->rightVol, ch->sfx->name);
				total++;
			}
		}

		Com_Printf (0, "----(%i)---- painted: %i\n", total, snd_dmaPaintedTime);
	}

	// Mix some sound
	SndImp_BeginPainting ();
	if (snd_audioDMA.buffer)
	{
		// Update DMA time
		fullSamples = snd_audioDMA.samples / snd_audioDMA.channels;

		/*
		** It is possible to miscount buffers if it has wrapped twice between
		** calls to Snd_Update. Oh well
		*/
		samplePos = SndImp_GetDMAPos ();
		if (samplePos < oldSamplePos)
		{
			buffers++;	// Buffer wrapped
			
			if (snd_dmaPaintedTime > 0x40000000)
			{
				// Time to chop things off to avoid 32 bit limits
				buffers = 0;
				snd_dmaPaintedTime = fullSamples;
				DMASnd_StopAllSounds ();
			}
		}

		oldSamplePos = samplePos;
		snd_dmaSoundTime = buffers*fullSamples + samplePos/snd_audioDMA.channels;

		// Check to make sure that we haven't overshot
		if (snd_dmaPaintedTime < snd_dmaSoundTime)
		{
			Com_DevPrintf (PRNT_WARNING, "Snd_Update: overflow\n");
			snd_dmaPaintedTime = snd_dmaSoundTime;
		}

		// Mix ahead of current position
		endTime = snd_dmaSoundTime + s_mixahead->floatVal * snd_audioDMA.speed;

		// Mix to an even submission block size
		endTime = (endTime + snd_audioDMA.submissionChunk-1) & ~(snd_audioDMA.submissionChunk-1);
		samples = snd_audioDMA.samples >> (snd_audioDMA.channels-1);
		if (endTime - snd_dmaSoundTime > samples)
			endTime = snd_dmaSoundTime + samples;

		DMASnd_PaintChannels (endTime, tempVol);
		SndImp_Submit ();
	}
}

/*
==============================================================================

	INIT / SHUTDOWN
 
==============================================================================
*/

/*
================
DMASnd_Init
================
*/
bool DMASnd_Init ()
{
	if (!SndImp_Init ())
		return false;

	s_volume->modified = false;
	DMASnd_ScaleTableInit (s_volume->floatVal);

	snd_dmaSoundTime = 0;
	snd_dmaPaintedTime = 0;

	return true;
}


/*
================
DMASnd_Shutdown
================
*/
void DMASnd_Shutdown ()
{
	SndImp_Shutdown ();

	snd_dmaSoundTime = 0;
	snd_dmaPaintedTime = 0;
}
