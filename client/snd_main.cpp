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
// snd_main.c
//

#include "snd_local.h"

bool					snd_isActive = true;
bool					snd_isInitialized;
bool					snd_isDMA;
bool					snd_isAL;

static bool				snd_queueRestart;

// Sound registration
#include <unordered_map>

typedef std::tr1::unordered_map<std::string, sfx_t*> TSFXList;
TSFXList sfxList;

uint32					snd_registrationFrame;

// Play sounds
playSound_t				snd_playSounds[MAX_PLAYSOUNDS];
playSound_t				snd_freePlays;
playSound_t				snd_pendingPlays;

cVar_t	*s_initSound;
cVar_t	*s_show;
cVar_t	*s_loadas8bit;
cVar_t	*s_volume;

cVar_t	*s_testsound;
cVar_t	*s_khz;
cVar_t	*s_mixahead;
cVar_t	*s_primary;

cVar_t	*al_allowExtensions;
cVar_t	*al_device;
cVar_t	*al_dopplerFactor;
cVar_t	*al_dopplerVelocity;
cVar_t	*al_errorCheck;
cVar_t	*al_gain;
cVar_t	*al_minDistance;
cVar_t	*al_maxDistance;
cVar_t	*al_rollOffFactor;

/*
===========
Snd_Activate
===========
*/
void Snd_Activate(const bool bActive)
{
	if (!snd_isInitialized)
		return;

	snd_isActive = bActive;

	if (snd_isAL)
		ALSnd_Activate(bActive);
}

/*
===============================================================================

	WAV LOADING AND RESAMPLING

===============================================================================
*/

static byte		*snd_dataPtr;
static byte		*snd_iffEnd;
static byte		*snd_lastIffChunk;
static byte		*snd_iffData;
static int		snd_iffChunkLength;

/*
============
_wGetLittleShort
============
*/
static sint16 _wGetLittleShort ()
{
	sint16 val = 0;

	val = *snd_dataPtr;
	val = val + (*(snd_dataPtr+1)<<8);

	snd_dataPtr += 2;

	return val;
}


/*
============
_wGetLittleLong
============
*/
static int _wGetLittleLong ()
{
	int val = 0;

	val = *snd_dataPtr;
	val = val + (*(snd_dataPtr+1)<<8);
	val = val + (*(snd_dataPtr+2)<<16);
	val = val + (*(snd_dataPtr+3)<<24);

	snd_dataPtr += 4;

	return val;
}


/*
============
_wFindNextChunk
============
*/
static void _wFindNextChunk (char *name)
{
	for ( ; ; ) {
		snd_dataPtr = snd_lastIffChunk;

		snd_dataPtr += 4;
		if (snd_dataPtr >= snd_iffEnd) {
			// Didn't find the chunk
			snd_dataPtr = NULL;
			return;
		}

		snd_iffChunkLength = _wGetLittleLong ();
		if (snd_iffChunkLength < 0) {
			snd_dataPtr = NULL;
			return;
		}

		snd_dataPtr -= 8;
		snd_lastIffChunk = snd_dataPtr + 8 + ((snd_iffChunkLength + 1) & ~1);
		if (!strncmp ((char *)snd_dataPtr, name, 4))
			return;
	}
}


/*
============
_wFindChunk
============
*/
static void _wFindChunk (char *name)
{
	snd_lastIffChunk = snd_iffData;
	_wFindNextChunk (name);
}


/*
============
Snd_GetWavinfo
============
*/
static wavInfo_t Snd_GetWavinfo (char *name, byte *wav, int wavLength)
{
	wavInfo_t	info;
	int			i;
	int			format;
	int			samples;

	memset (&info, 0, sizeof(info));

	if (!wav)
		return info;
		
	snd_iffData = wav;
	snd_iffEnd = wav + wavLength;

	// Find "RIFF" chunk
	_wFindChunk ("RIFF");
	if (!(snd_dataPtr && !strncmp ((char *)snd_dataPtr+8, "WAVE", 4))) {
		Com_Printf (0, "Missing RIFF/WAVE chunks\n");
		return info;
	}

	// Get "fmt " chunk
	snd_iffData = snd_dataPtr + 12;
	_wFindChunk ("fmt ");
	if (!snd_dataPtr) {
		Com_Printf (0, "Missing fmt chunk\n");
		return info;
	}

	snd_dataPtr += 8;
	format = _wGetLittleShort ();
	if (format != 1) {
		Com_Printf (0, "Microsoft PCM format only\n");
		return info;
	}

	// Channels, rate, width...
	info.channels = _wGetLittleShort ();
	info.rate = _wGetLittleLong ();
	snd_dataPtr += 4+2;
	info.width = _wGetLittleShort () / 8;

	// Get cue chunk
	_wFindChunk ("cue ");
	if (snd_dataPtr) {
		snd_dataPtr += 32;
		info.loopStart = _wGetLittleLong ();

		// If the next chunk is a LIST chunk, look for a cue length marker
		_wFindNextChunk ("LIST");
		if (snd_dataPtr) {
			if (!strncmp ((char *)snd_dataPtr+28, "mark", 4)) {
				// This is not a proper parse, but it works with cooledit
				snd_dataPtr += 24;
				i = _wGetLittleLong ();	// Samples in loop
				info.samples = info.loopStart + i;
			}
		}
	}
	else
		info.loopStart = -1;

	// Find data chunk
	_wFindChunk ("data");
	if (!snd_dataPtr) {
		Com_Printf (0, "Missing data chunk\n");
		return info;
	}

	// Check loop length
	snd_dataPtr += 4;
	samples = _wGetLittleLong () / info.width;
	if (info.samples) {
		if (samples < info.samples)
			Com_Error (ERR_DROP, "Sound %s has a bad loop length", name);
	}
	else
		info.samples = samples;

	info.dataOfs = snd_dataPtr - wav;
	return info;
}


/*
================
DMASnd_ResampleSfx
================
*/
static void DMASnd_ResampleSfx (sfx_t *sfx, int inRate, int inWidth, byte *data)
{
	int		outcount;
	int		srcsample;
	float	stepscale;
	int		i;
	int		sample, samplefrac, fracstep;
	sfxCache_t	*sc;
	
	sc = sfx->cache;
	if (!sc)
		return;

	stepscale = (float)inRate / snd_audioDMA.speed;	// This is usually 0.5, 1, or 2

	outcount = sc->length / stepscale;
	sc->length = outcount;
	if (sc->loopStart != -1)
		sc->loopStart = sc->loopStart / stepscale;

	sc->speed = snd_audioDMA.speed;
	if (s_loadas8bit->intVal)
		sc->width = 1;
	else
		sc->width = inWidth;
	sc->stereo = 0;

	// Resample / decimate to the current source rate
	if (stepscale == 1 && inWidth == 1 && sc->width == 1) {
		// Fast special case
		for (i=0 ; i<outcount ; i++)
			((signed char *)sc->data)[i] = (int)((byte)(data[i]) - 128);
	}
	else {
		// General case
		samplefrac = 0;
		fracstep = stepscale*256;
		for (i=0 ; i<outcount ; i++) {
			srcsample = samplefrac >> 8;
			samplefrac += fracstep;
			if (inWidth == 2)
				sample = LittleShort (((sint16 *)data)[srcsample]);
			else
				sample = (int)((byte)(data[srcsample]) - 128) << 8;
			if (sc->width == 2)
				((sint16 *)sc->data)[i] = sample;
			else
				((signed char *)sc->data)[i] = sample >> 8;
		}
	}
}


/*
==============
Snd_LoadSound
==============
*/
sfxCache_t *Snd_LoadSound (sfx_t *s)
{
    char		namebuffer[MAX_QPATH];
	byte		*data;
	wavInfo_t	info;
	int			len;
	float		stepscale;
	sfxCache_t	*sc;
	int			fileLen;
	char		*name;

	if (!s)
		return NULL;

	s->touchFrame = snd_registrationFrame;
	if (s->name[0] == '*')
		return NULL;

	// See if still in memory
	sc = s->cache;
	if (sc)
		return sc;

	// Load it in
	if (s->trueName)
		name = s->trueName;
	else
		name = s->name;

	if (name[0] == '#')
		Q_strncpyz (namebuffer, &name[1], sizeof(namebuffer));
	else
		Q_snprintfz (namebuffer, sizeof(namebuffer), "sound/%s", name);

	fileLen = FS_LoadFile (namebuffer, (void **)&data, false);
	if (!data || fileLen <= 0) {
		Com_DevPrintf (0, "Snd_LoadSound: Couldn't load %s -- %s\n", namebuffer, (fileLen == -1) ? "not found" : "empty file");
		return NULL;
	}

	// Get WAV info
	info = Snd_GetWavinfo (s->name, data, fileLen);
	if (snd_isDMA) {
		if (info.channels != 1) {
			Com_Printf (0, "Snd_LoadSound: %s is a stereo sample\n", s->name);
			FS_FreeFile (data);
			return NULL;
		}
		stepscale = (float)info.rate / snd_audioDMA.speed;	
		len = (info.samples / stepscale) * info.width * info.channels;
	}
	else if (snd_isAL) {
		len = 0;
	}

	sc = s->cache = (sfxCache_t*)Mem_PoolAlloc (len + sizeof(sfxCache_t), cl_soundSysPool, 0);
	if (!sc) {
		FS_FreeFile (data);
		return NULL;
	}

	if (snd_isDMA) {
		sc->length = info.samples;
		sc->loopStart = info.loopStart;
		sc->speed = info.rate;
		sc->width = info.width;
		sc->stereo = info.channels;

		DMASnd_ResampleSfx (s, sc->speed, sc->width, data + info.dataOfs);
	}
	else if (snd_isAL) {
		ALSnd_CreateBuffer (sc, info.width, info.channels, data + info.dataOfs, info.samples * info.width * info.channels, info.rate);
	}

	FS_FreeFile (data);
	return sc;
}

/*
===============================================================================

	CONSOLE FUNCTIONS

===============================================================================
*/

/*
============
Snd_Play_f
============
*/
static void Snd_Play_f ()
{
	char	name[256];
	sfx_t	*sfx;
	int		i;

	if (!snd_isInitialized) {
		Com_Printf (PRNT_WARNING, "Sound system not started\n");
		return;
	}

	i = 1;
	while (i < Cmd_Argc ()) {
		if (!strrchr (Cmd_Argv (i), '.')) {
			Q_snprintfz (name, sizeof(name), "%s.wav", Cmd_Argv (i));
		}
		else
			Q_strncpyz (name, Cmd_Argv (i), sizeof(name));
		sfx = Snd_RegisterSound (name);
		Snd_StartSound (NULL, cl.playerNum+1, CHAN_AUTO, sfx, 1.0f, 1.0f, 0);
		i++;
	}
}


/*
================
Snd_Restart_f

For queueing a sound restart
================
*/
static void Snd_Restart_f ()
{
	snd_queueRestart = true;
}


/*
============
Snd_SoundList_f
============
*/
static void Snd_SoundList_f ()
{
	sfxCache_t	*sc;
	int			total = 0;
	int			size;

	if (!snd_isInitialized) {
		Com_Printf (PRNT_WARNING, "Sound system not started\n");
		return;
	}

	for (auto it = sfxList.begin(); it != sfxList.end(); ++it)
	{
		auto sfx = (*it).second;
		if (!sfx->touchFrame)
			continue;

		sc = sfx->cache;
		if (sc)
		{
			size = sc->length * sc->width * (sc->stereo + 1);
			total += size;
			if (sc->loopStart >= 0)
				Com_Printf (0, "L");
			else
				Com_Printf (0, " ");
			Com_Printf (0, "(%2db) %6i : %s\n", sc->width*8, size, sfx->name);
		}
		else
		{
			if (sfx->name[0] == '*')
				Com_Printf (0, "  placeholder : %s\n", sfx->name);
			else
				Com_Printf (0, "  not loaded  : %s\n", sfx->name);
		}
	}

	Com_Printf (0, "Total resident: %i\n", total);
}


/*
=================
Snd_SoundInfo_f
=================
*/
static void Snd_SoundInfo_f ()
{
	if (snd_isDMA) {
		Com_Printf (0, "DMA Sound info:\n");
		Com_Printf (0, "%5d stereo\n", snd_audioDMA.channels - 1);
		Com_Printf (0, "%5d samples\n", snd_audioDMA.samples);
		Com_Printf (0, "%5d samplepos\n", snd_audioDMA.samplePos);
		Com_Printf (0, "%5d samplebits\n", snd_audioDMA.sampleBits);
		Com_Printf (0, "%5d submission_chunk\n", snd_audioDMA.submissionChunk);
		Com_Printf (0, "%5d speed\n", snd_audioDMA.speed);
		Com_Printf (0, "0x%x dma buffer\n", snd_audioDMA.buffer);
	}
	else if (snd_isAL) {
		Com_Printf (0, "OpenAL Sound info:\n");
		Com_Printf (0, "AL_VENDOR: %s\n", snd_audioAL.vendorString);
		Com_Printf (0, "AL_RENDERER: %s\n", snd_audioAL.rendererString);
		Com_Printf (0, "AL_VERSION: %s\n", snd_audioAL.versionString);
		Com_Printf (0, "AL_EXTENSIONS: %s\n", snd_audioAL.extensionString);
		Com_Printf (0, "ALC_DEVICE_SPECIFIER: %s\n", snd_audioAL.deviceName);
		Com_Printf (0, "%i sources\n", snd_audioAL.numChannels);
	}
	else {
		Com_Printf (PRNT_WARNING, "Sound system not started\n");
	}
}

/*
==============================================================================

	SOUND REGISTRATION
 
==============================================================================
*/

/*
==================
Snd_FreeSound
==================
*/
static void Snd_FreeSound (sfx_t *sfx)
{
	if (!sfx)
		return;

	// FIXME: trueName is a goddamned hack
	if (sfx->trueName)
		Mem_Free (sfx->trueName);

	if (sfx->cache) {
		// Delete the OpenAL buffer
		if (snd_isAL)
			ALSnd_DeleteBuffer (sfx->cache);

		// It is possible to have a leftover from a server that didn't finish loading
		Mem_Free (sfx->cache);
	}
}

/*
==================
Snd_FindName
==================
*/
static sfx_t *Snd_FindName (char *name, bool create)
{
	char	outName[MAX_QPATH];

	if (!name) {
		Com_Printf (PRNT_ERROR, "Snd_FindName: NULL name\n");
		return NULL;
	}
	if (!name[0]) {
		Com_Printf (PRNT_ERROR, "Snd_FindName: empty name\n");
		return NULL;
	}

	if (strlen(name)+1 >= MAX_QPATH)
		Com_Error (ERR_FATAL, "Snd_FindName: Sound name too long: %s", name);

	// Copy, then normalize the path, but don't strip the extension
	Com_NormalizePath (outName, sizeof(outName), name);

	// See if already loaded
	auto ind = sfxList.find(name);

	if (ind != sfxList.end())
		return (*ind).second;

	if (!create)
		return NULL;

	// Find a free sfx
	auto sfx = new sfx_t;
	sfxList.insert(TSFXList::value_type(name, sfx));

	// Fill
	memset (sfx, 0, sizeof(sfx_t));
	Q_strncpyz (sfx->name, outName, sizeof(sfx->name));

	// Bump
	sfx->touchFrame = snd_registrationFrame;

	return sfx;
}


/*
==================
Snd_AliasName
==================
*/
static sfx_t *Snd_AliasName (char *aliasName, char *trueName)
{
	int		len;

	len = strlen (trueName);
	if (len+1 >= MAX_QPATH)
		Com_Error (ERR_FATAL, "Snd_AliasName: Sound name too long: %s", trueName);

	// Find a free sfx
	auto ind = sfxList.find(aliasName);

	if (ind != sfxList.end())
		return (*ind).second;

	auto sfx = new sfx_t;
	sfxList.insert(TSFXList::value_type(aliasName, sfx));

	memset (sfx, 0, sizeof(sfx_t));

	// Fill
	Q_strncpyz (sfx->name, aliasName, sizeof(sfx->name));
	sfx->trueName = Mem_PoolStrDup (trueName, cl_soundSysPool, 0);

	// Bump
	sfx->touchFrame = snd_registrationFrame;

	return sfx;
}


/*
=====================
Snd_BeginRegistration
=====================
*/
void Snd_BeginRegistration ()
{
	snd_registrationFrame++;
}


/*
==================
Snd_RegisterSound
==================
*/
sfx_t *Snd_RegisterSound (char *name)
{
	sfx_t	*sfx;

	if (!snd_isInitialized)
		return NULL;

	sfx = Snd_FindName (name, true);
	Snd_LoadSound (sfx);

	return sfx;
}


/*
===============
Snd_RegisterSexedSound
===============
*/
static struct sfx_t *Snd_RegisterSexedSound (char *base, int entNum)
{
	char			model[MAX_QPATH];
	char			sexedFilename[MAX_QPATH];
	char			maleFilename[MAX_QPATH];
	int				n;
	struct sfx_t	*sfx;
	char			*p;
	fileHandle_t	fileNum;

	// Determine what model the client is using
	model[0] = 0;
	n = CS_PLAYERSKINS + entNum - 1;
	if (cl.configStrings[n][0]) {
		p = strchr (cl.configStrings[n], '\\');
		if (p) {
			p += 1;
			Q_strncpyz (model, p, sizeof(model));
			p = strchr (model, '/');
			if (p)
				*p = 0;
		}
	}

	// If we can't figure it out, they're male
	if (!model[0])
		Q_strncpyz (model, "male", sizeof(model));

	// See if we already know of the model specific sound
	Q_snprintfz (sexedFilename, sizeof(sexedFilename), "#players/%s/%s", model, base+1);
	sfx = Snd_FindName (sexedFilename, false);

	if (!sfx) {
		// No, so see if it exists
		FS_OpenFile (&sexedFilename[1], &fileNum, FS_MODE_READ_BINARY);
		if (fileNum) {
			// Yes, close the file and register it
			FS_CloseFile (fileNum);
			sfx = Snd_RegisterSound (sexedFilename);
		}
		else {
			// No, revert to the male sound
			Q_snprintfz (maleFilename, sizeof(maleFilename), "player/male/%s", base+1);
			sfx = Snd_AliasName (sexedFilename, maleFilename);
		}
	}

	return sfx;
}


/*
=====================
Snd_EndRegistration
=====================
*/
void Snd_EndRegistration ()
{
	int		released = 0;

	// Free untouched sounds and make sure it is paged in
	for (auto it = sfxList.begin(); it != sfxList.end(); ++it)
	{
		if (it->second->touchFrame == snd_registrationFrame)
			continue;

		Snd_FreeSound (it->second);
		delete it->second;
		it = sfxList.erase(it);
		released++;
	}
	Com_DevPrintf (PRNT_CONSOLE, "sounds released: %i\n", released);
}

/*
==============================================================================

	INIT / SHUTDOWN
 
==============================================================================
*/

static conCmd_t	*cmd_snd_restart;

static conCmd_t	*cmd_play;
static conCmd_t	*cmd_stopSound;
static conCmd_t	*cmd_soundList;
static conCmd_t	*cmd_soundInfo;


/*
================
Snd_Restart

Restart the sound subsystem so it can pick up new parameters and flush all sounds
================
*/
void Snd_Restart ()
{
	Snd_Shutdown ();
	Snd_Init ();

	Snd_BeginRegistration ();
	CL_SoundMediaInit ();
	Snd_EndRegistration ();
}


/*
================
Snd_CheckChanges

Forces a sound restart now
================
*/
void Snd_CheckChanges ()
{
	if (snd_queueRestart) {
		snd_queueRestart = false;
		Snd_Restart ();
	}
}


/*
================
Snd_Init

If already initialized the sound system will restart
================
*/
void Snd_Init()
{
	uint32 startCycles = Sys_Cycles();

	if (snd_isInitialized)
		Snd_Shutdown();

	Com_Printf(0, "\n--------- Sound Initialization ---------\n");

	Cvar_GetLatchedVars(CVAR_LATCH_AUDIO);

	snd_isInitialized = false;
	snd_isDMA = false;
	snd_isAL = false;
	snd_queueRestart = false;
	snd_registrationFrame = 1;

	s_initSound			= Cvar_Register("s_initSound",			"1",			CVAR_ARCHIVE|CVAR_LATCH_AUDIO);
	s_volume			= Cvar_Register("s_volume",				"0.7",			CVAR_ARCHIVE);
	s_loadas8bit		= Cvar_Register("s_loadas8bit",			"0",			CVAR_ARCHIVE|CVAR_LATCH_AUDIO);

	s_khz				= Cvar_Register("s_khz",				"11",			CVAR_ARCHIVE|CVAR_LATCH_AUDIO);
	s_mixahead			= Cvar_Register("s_mixahead",			"0.2",			CVAR_ARCHIVE);
	s_show				= Cvar_Register("s_show",				"0",			CVAR_CHEAT);
	s_testsound			= Cvar_Register("s_testsound",			"0",			0);
	s_primary			= Cvar_Register("s_primary",			"0",			CVAR_ARCHIVE|CVAR_LATCH_AUDIO);	// win32 specific

	al_allowExtensions	= Cvar_Register("al_allowExtensions",	"1",			CVAR_ARCHIVE|CVAR_LATCH_AUDIO);
	al_device			= Cvar_Register("al_device",			"",				CVAR_ARCHIVE);
	al_dopplerFactor	= Cvar_Register("al_dopplerFactor",		"1",			CVAR_ARCHIVE);
	al_dopplerVelocity	= Cvar_Register("al_dopplerVelocity",	"16384",		CVAR_ARCHIVE);
	al_errorCheck		= Cvar_Register("al_errorCheck",		"1",			CVAR_ARCHIVE);
	al_gain				= Cvar_Register("al_gain",				"5",			CVAR_ARCHIVE);
	al_minDistance		= Cvar_Register("al_minDistance",		"128",			CVAR_ARCHIVE);
	al_maxDistance		= Cvar_Register("al_maxDistance",		"8192",			CVAR_ARCHIVE);
	al_rollOffFactor	= Cvar_Register("al_rollOffFactor",		"0.8",			CVAR_ARCHIVE);

	cmd_snd_restart = Cmd_AddCommand("snd_restart",		0, Snd_Restart_f,		"Restarts the audio subsystem");
	cmd_play		= Cmd_AddCommand("play",			0, Snd_Play_f,			"Plays a sound");
	cmd_stopSound	= Cmd_AddCommand("stopsound",		0, Snd_StopAllSounds,	"Stops all currently playing sounds");
	cmd_soundList	= Cmd_AddCommand("soundlist",		0, Snd_SoundList_f,		"Prints out a list of loaded sound files");
	cmd_soundInfo	= Cmd_AddCommand("soundinfo",		0, Snd_SoundInfo_f,		"Prints out information on sound subsystem");

	if (!s_initSound->intVal)
	{
		Com_Printf(0, "...not initializing\n");
		Com_Printf(0, "----------------------------------------\n");
		return;
	}

	// Initialize sound
	if (s_initSound->intVal == 2)
	{
		if (ALSnd_Init())
		{
			snd_isAL = true;
		}
		else
		{
			Com_Printf(PRNT_ERROR, "Snd_Init: Failed to initialize OpenAL!\n");
		}
	}
	if (!snd_isAL)
	{
		if (!DMASnd_Init())
			return;
		snd_isDMA = true;
	}

	snd_isInitialized = true;

	Snd_StopAllSounds();

	Com_Printf(0, "----------------------------------------\n");

	// Check memory integrity
	Mem_CheckPoolIntegrity(cl_soundSysPool);

	Com_Printf(0, "init time: %6.2fms\n", (Sys_Cycles()-startCycles) * Sys_MSPerCycle());
	Com_Printf(0, "----------------------------------------\n");
}


/*
================
Snd_Shutdown
================
*/
void Snd_Shutdown ()
{
	uint32	size;

	Cmd_RemoveCommand(cmd_snd_restart);
	Cmd_RemoveCommand(cmd_play);
	Cmd_RemoveCommand(cmd_stopSound);
	Cmd_RemoveCommand(cmd_soundList);
	Cmd_RemoveCommand(cmd_soundInfo);

	if (!snd_isInitialized)
		return;
	snd_isInitialized = false;

	Com_Printf (0, "\n------------ Sound Shutdown ------------\n");

	// Free all sounds
	Snd_FreeSounds ();

	// Free all memory
	size = Mem_PoolSize (cl_soundSysPool);
	Com_Printf (0, "...releasing %u bytes\n", size);
	Mem_FreePool (cl_soundSysPool);

	// Shutdown the subsystem
	if (snd_isDMA) {
		DMASnd_Shutdown ();
		snd_isDMA = false;
	}
	else if (snd_isAL) {
		ALSnd_Shutdown ();
		snd_isAL = false;
	}

	Com_Printf (0, "----------------------------------------\n");
}


/*
================
Snd_FreeSounds
================
*/
void Snd_FreeSounds ()
{
	for (auto it = sfxList.begin(); it != sfxList.end(); ++it)
	{
		Snd_FreeSound (it->second);
		delete it->second;
	}

	sfxList.clear();
}


/*
===============================================================================

	PLAYSOUNDS

===============================================================================
*/

/*
=================
Snd_AllocPlaysound
=================
*/
static playSound_t *Snd_AllocPlaysound ()
{
	playSound_t	*ps;

	ps = snd_freePlays.next;
	if (ps == &snd_freePlays)
		return NULL;	// No free playsounds

	// Unlink from freelist
	ps->prev->next = ps->next;
	ps->next->prev = ps->prev;
	
	return ps;
}


/*
=================
Snd_FreePlaysound
=================
*/
void Snd_FreePlaysound (playSound_t *ps)
{
	// Unlink from channel
	ps->prev->next = ps->next;
	ps->next->prev = ps->prev;

	// Add to free list
	ps->next = snd_freePlays.next;
	snd_freePlays.next->prev = ps;
	ps->prev = &snd_freePlays;
	snd_freePlays.next = ps;
}

/*
===============================================================================

	SOUND PLAYING

===============================================================================
*/

/*
====================
Snd_StartSound

Validates the parms and ques the sound up
If origin is NULL, the sound will be dynamically sourced from the entity
entChannel 0 will never override a playing sound
====================
*/
void Snd_StartSound (vec3_t origin, int entNum, EEntSndChannel entChannel, struct sfx_t *sfx, float volume, float attenuation, float timeOffset)
{
	sfxCache_t	*sc;
	playSound_t	*ps, *sort;
	int			start;
	static int	beginOfs = 0;

	if (cls.disableScreen || !snd_isActive || !snd_isInitialized)
		return;
	if (!sfx)
		return;

	if (sfx->name[0] == '*')
		sfx = Snd_RegisterSexedSound (sfx->name, entNum);

	// Make sure the sound is loaded
	sc = Snd_LoadSound (sfx);
	if (!sc)
		return;		// Couldn't load the sound's data

	// Make the playSound_t
	ps = Snd_AllocPlaysound ();
	if (!ps)
		return;

	// Sanity check
	if (entNum < 0 || entNum >= MAX_CS_EDICTS) {
		Com_Printf (PRNT_ERROR, "Snd_StartSound: invalid entNum (%i), forcing 0", entNum);
		entNum = 0;
	}
	if (entChannel < 0) {
		Com_Printf (PRNT_ERROR, "Snd_StartSound: invalid entChannel (%i), forcing 0", entChannel);
		entChannel = CHAN_AUTO;
	}
	if (attenuation < ATTN_NONE || attenuation > ATTN_STATIC) {
		Com_Printf (PRNT_ERROR, "Snd_StartSound: invalid attenuation (%i), forcing normal (%i)", attenuation, ATTN_NORM);
		attenuation = ATTN_NORM;
	}

	// Set the origin type
	if (origin) {
		Vec3Copy (origin, ps->origin);
		ps->type = PSND_FIXED;
	}
	else if (entNum == cl.playerNum+1)
		ps->type = PSND_LOCAL;
	else
		ps->type = PSND_ENTITY;

	// Volume
	ps->volume = Q_rint (volume * 255);
	ps->volume = clamp (ps->volume, 0, 255);

	// Other fields
	ps->entNum = entNum;
	ps->entChannel = entChannel;
	ps->attenuation = attenuation;
	ps->sfx = sfx;

	// Drift beginOfs
	if (snd_isDMA) {
		start = cl.frame.serverTime * 0.001f * snd_audioDMA.speed + beginOfs;
		if (start < snd_dmaPaintedTime) {
			start = snd_dmaPaintedTime;
			beginOfs = start - (cl.frame.serverTime * 0.001f * snd_audioDMA.speed);
		}
		else if (start > snd_dmaPaintedTime + 0.3f * snd_audioDMA.speed) {
			start = snd_dmaPaintedTime + 0.1f * snd_audioDMA.speed;
			beginOfs = start - (cl.frame.serverTime * 0.001f * snd_audioDMA.speed);
		}
		else
			beginOfs -= 10;

		if (!timeOffset)
			ps->beginTime = snd_dmaPaintedTime;
		else
			ps->beginTime = start + timeOffset * snd_audioDMA.speed;
	}
	else if (snd_isAL) {
		// FIXME: 3000 seems like an odd number, but it comes closest to matching DMA delays...
		ps->beginTime = cls.realTime + (timeOffset * 3000);
	}

	// Sort into the pending sound list
	for (sort=snd_pendingPlays.next ; sort != &snd_pendingPlays && sort->beginTime < ps->beginTime ; sort=sort->next) ;

	ps->next = sort;
	ps->prev = sort->prev;

	ps->next->prev = ps;
	ps->prev->next = ps;
}


/*
==================
Snd_StartLocalSound
==================
*/
void Snd_StartLocalSound (struct sfx_t *sfx, float volume)
{
	Snd_StartSound (NULL, cl.playerNum+1, CHAN_AUTO, sfx, volume, ATTN_NONE, 0);
}


/*
==================
Snd_StopAllSounds
==================
*/
void Snd_StopAllSounds ()
{
	int		i;

	if (!snd_isInitialized)
		return;

	// Clear all the playsounds
	memset (snd_playSounds, 0, sizeof(snd_playSounds));
	snd_freePlays.next = snd_freePlays.prev = &snd_freePlays;
	snd_pendingPlays.next = snd_pendingPlays.prev = &snd_pendingPlays;

	for (i=0 ; i<MAX_PLAYSOUNDS ; i++) {
		snd_playSounds[i].prev = &snd_freePlays;
		snd_playSounds[i].next = snd_freePlays.next;
		snd_playSounds[i].prev->next = &snd_playSounds[i];
		snd_playSounds[i].next->prev = &snd_playSounds[i];
	}

	// Hand off to the implementation
	if (snd_isDMA) {
		DMASnd_StopAllSounds ();
	}
	else if (snd_isAL) {
		ALSnd_StopAllSounds ();
	}
}

/*
===============================================================================

	RAW SAMPLING

	Cinematic streaming and voice over network

===============================================================================
*/

/*
============
Snd_RawStart
============
*/
channel_t *Snd_RawStart ()
{
	if (!snd_isInitialized || !snd_isAL)
		return NULL;

	return ALSnd_RawStart ();
}


/*
============
Snd_RawSamples
============
*/
void Snd_RawSamples (channel_t *rawChannel, int samples, int rate, int width, int channels, byte *data)
{
	if (!snd_isInitialized)
		return;

	if (snd_isDMA)
		DMASnd_RawSamples (samples, rate, width, channels, data);
	else if (snd_isAL)
		ALSnd_RawSamples (rawChannel, samples, rate, width, channels, data);
}


/*
============
Snd_RawStop
============
*/
void Snd_RawStop (channel_t *rawChannel)
{
	if (!snd_isInitialized || !snd_isAL)
		return;

	ALSnd_RawStop (rawChannel);
}

/*
===============================================================================

	CHANNEL MIXING

===============================================================================
*/


/*
============
Snd_Update

Called once each time through the main loop
============
*/
void Snd_Update (refDef_t *rd)
{
	if (!snd_isInitialized)
		return;

	if (snd_isDMA)
		DMASnd_Update (rd);
	else if (snd_isAL)
		ALSnd_Update (rd);
}
