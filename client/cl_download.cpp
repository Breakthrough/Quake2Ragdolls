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
// cl_download.c
//

#include "cl_local.h"

static int	cl_downloadCheck; // for autodownload of precache items
static int	cl_downloadSpawnCount;
static int	cl_downloadTexNum;

static byte	*cl_downloadModel; // used for skin checking in alias models
static int	cl_downloadModelSkin;

#define PLAYER_MULT 5

// ENV_CNT is map load, ENV_CNT+1 is first env map
#define ENV_CNT (CS_PLAYERSKINS + MAX_CS_CLIENTS * PLAYER_MULT)
#define TEXTURE_CNT (ENV_CNT+13)

/*
=====================================================================

	DOWNLOAD PARSING AND HANDLING

=====================================================================
*/

/*
===============
CL_DownloadFileName
===============
*/
static void CL_DownloadFileName (char *dest, int destLen, char *fileName)
{
	if (cl_downloadToBase->intVal) {
		Q_snprintfz (dest, destLen, BASE_MODDIRNAME "/%s", fileName);
		return;
	}

	Q_snprintfz (dest, destLen, "%s/%s", FS_Gamedir(), fileName);
}


/*
===============
CL_CheckOrDownloadFile

Returns true if the file exists, otherwise it attempts to start a download from the server.
===============
*/
bool CL_CheckOrDownloadFile (char *fileName)
{
	FILE	*fp;
	char	tempName[MAX_OSPATH];

	// Don't download if there's ".." in the path
#if 0 // FIXME: looks like iD used this on some of it's models! (see boss2.bsp)
	if (strstr (fileName, "..")) {
		Com_Printf (PRNT_WARNING, "Refusing to check a path with '..' (%s)\n", fileName);
		return true;
	}
#endif
	if (strchr (fileName, ' ')) {
		Com_Printf (PRNT_WARNING, "Refusing to check a path containing spaces (%s)\n", fileName);
		return true;
	}
	if (strchr (fileName, ':')) {
		Com_Printf (PRNT_WARNING, "Refusing to check a path containing a colon (%s)\n", fileName);
		return true;
	}
	if (fileName[0] == '/') {
		Com_Printf (PRNT_WARNING, "Refusing to check a path starting with '/' (%s)\n", fileName);
		return true;
	}

	// No need to redownload a file that already exists
	if (!Cvar_Register("cl_download_simulation", "0", 0)->intVal && FS_FileExists (fileName) != -1)
		return true;

#ifdef CL_HTTPDL
	// Check with the download server
	if (CL_HTTPDL_QueueDownload (fileName))
		return true;
#endif

	// Don't attempt to download another file with UDP
	if (cls.download.file) {
		Com_Printf (PRNT_WARNING, "Refusing to download while a file is already downloading (%s)\n", fileName);
		return true;
	}

	// Copy a normalized version of the filename
	Com_NormalizePath (cls.download.name, sizeof(cls.download.name), fileName);

	// Verify the final path is legal
	if (cls.download.name[0] == '/') {
		Com_Printf (PRNT_WARNING, "Refusing to download a path starting with '/' (%s)\n", cls.download.name);
		return true;
	}
	if (cls.download.name[strlen(cls.download.name)-1] == '/') {
		Com_Printf (PRNT_WARNING, "Refusing to download a path ending with '/' (%s)\n", cls.download.name);
		return true;
	}

	// Download to a temp filename and rename when done (so if it's interrupted a runt wont be left)
	Com_StripExtension (cls.download.tempName, sizeof(cls.download.tempName), cls.download.name);
	Q_strcatz (cls.download.tempName, ".tmp", sizeof(cls.download.tempName));

	// Resume if there's already a temp file
	CL_DownloadFileName (tempName, sizeof(tempName), cls.download.tempName);
	fp = fopen (tempName, "r+b");
	if (fp) {
		// It exists
		int		len;

		fseek (fp, 0, SEEK_END);
		len = ftell (fp);

		cls.download.file = fp;

		// Give the server an offset to start the download
		Com_Printf (0, "Resuming %s\n", cls.download.name);

		cls.netChan.message.WriteByte (CLC_STRINGCMD);
		if (cls.serverProtocol == ENHANCED_PROTOCOL_VERSION)
			cls.netChan.message.WriteString (Q_VarArgs ("download \"%s\" %i udp-zlib", cls.download.name, len));
		else
			cls.netChan.message.WriteString (Q_VarArgs ("download \"%s\" %i", cls.download.name, len));
	}
	else {
		Com_Printf (0, "Downloading %s\n", cls.download.name);

		cls.netChan.message.WriteByte (CLC_STRINGCMD);
		if (cls.serverProtocol == ENHANCED_PROTOCOL_VERSION)
			cls.netChan.message.WriteString (Q_VarArgs ("download \"%s\" 0 udp-zlib", cls.download.name));
		else
			cls.netChan.message.WriteString (Q_VarArgs ("download \"%s\"", cls.download.name));
	}

	cls.forcePacket = true;
	return false;
}


/*
=====================
CL_ParseDownload

A download message has been received from the server
=====================
*/
double lastReceiveTime = 0;
int sizeSince = 0;
void CL_ParseDownload (bool compressed)
{
	int		size, percent;
	char	name[MAX_OSPATH];

	// Read the data
	size = cls.netMessage.ReadShort ();
	percent = cls.netMessage.ReadByte ();
	if (size < 0) {
		if (size == -1)
			Com_Printf (PRNT_WARNING, "Server does not have this file.\n");
		else
			Com_Printf (PRNT_ERROR, "Bad download data from server.\n");

		// Nuke the temp file name
		cls.download.tempName[0] = '\0';
		cls.download.name[0] = '\0';

		if (cls.download.file) {
			// If here, we tried to resume a file but the server said no
			fclose (cls.download.file);
			cls.download.file = NULL;
		}
		CL_RequestNextDownload ();
		return;
	}

	// Open the file if not opened yet
	if (!cls.download.file) {
		if (!cls.download.tempName[0]) {
			Com_Printf (PRNT_WARNING, "Received download packet without requesting it first, ignoring.\n");
			return;
		}

		CL_DownloadFileName (name, sizeof(name), cls.download.tempName);

		FS_CreatePath (name);

		cls.download.file = fopen (name, "wb");
		if (!cls.download.file) {
			cls.netMessage.readCount += size;
			Com_Printf (PRNT_WARNING, "Failed to open %s\n", cls.download.tempName);
			CL_RequestNextDownload ();
			return;
		}
	}

	// Insert block
 	if (compressed) {
 		uint16	uncompressedLen;
 		byte	*uncompressed;

 		uncompressedLen = cls.netMessage.ReadShort ();
 		if (!uncompressedLen)
			Com_Error (ERR_DROP, "CL_ParseDownload: uncompressedLen == 0");

		uncompressed = new byte[uncompressedLen];
 		FS_ZLibDecompress (cls.netMessage.data + cls.netMessage.readCount, size, uncompressed, uncompressedLen, -15);
 		if (!Cvar_Register("cl_download_simulation", "0", 0))
			fwrite (uncompressed, 1, uncompressedLen, cls.download.file);
		delete[] uncompressed;
 		Com_DevPrintf (0, "SVC_ZDOWNLOAD(%s): %d -> %d\n", cls.download.name, size, uncompressedLen);
 	}
 	else {
		if (!Cvar_Register("cl_download_simulation", "0", 0))
			fwrite (cls.netMessage.data + cls.netMessage.readCount, 1, size, cls.download.file);
	}

	cls.netMessage.readCount += size;

	if (Cvar_Register("cl_download_simulation", "0", 0))
	{
		Com_Printf (0, "Received %u packet: %i%%", size, percent);
		if ((Sys_UMilliseconds() - lastReceiveTime) > 1000)
		{
			Com_Printf (0, " | measured %ib/s\n", sizeSince);
			lastReceiveTime = Sys_UMilliseconds();
			sizeSince = 0;
		}
		else
		{
			Com_Printf(0, "\n");
			sizeSince += size;
		}
	}

	if (percent != 100) {
		// Request next block
		cls.download.percent = percent;

		cls.netChan.message.WriteByte (CLC_STRINGCMD);
		cls.netChan.message.WriteStringCat ("nextdl");
		cls.forcePacket = true;
	}
	else {
		char	oldName[MAX_OSPATH];
		char	newName[MAX_OSPATH];
		int		rn;

		fclose (cls.download.file);

		// Rename the temp file to it's final name
		if (!Cvar_Register("cl_download_simulation", "0", 0))
		{
			CL_DownloadFileName (oldName, sizeof(oldName), cls.download.tempName);
			CL_DownloadFileName (newName, sizeof(newName), cls.download.name);
			rn = rename (oldName, newName);
			if (rn)
				Com_Printf (PRNT_ERROR, "Failed to rename!\n");
			else
				Com_Printf (0, "Download of %s completed\n", newName);
		}
		else
		{
			CL_DownloadFileName (oldName, sizeof(oldName), cls.download.tempName);
			remove (oldName);
			Com_Printf (0, "Download of %s completed\n", oldName);
		}

		cls.download.file = NULL;
		cls.download.percent = 0;
		cls.download.name[0] = '\0';
		cls.download.tempName[0] = '\0';

		// Get another file if needed
		CL_RequestNextDownload ();
	}
}


/*
==============
CL_ResetDownload

Can only be called by CL_Precache_f!
==============
*/
void CL_ResetDownload ()
{
	cl_downloadCheck = CS_MODELS;
	cl_downloadSpawnCount = atoi (Cmd_Argv (1));
	cl_downloadModel = 0;
	cl_downloadModelSkin = 0;
}


/*
==============
CL_RequestNextDownload
==============
*/
void CL_RequestNextDownload ()
{
	static const char	*skySuffix[6] = { "rt", "bk", "lf", "ft", "up", "dn" };
	uint32			mapCheckSum;		// for detecting cheater maps
	char			fileName[MAX_OSPATH];
	dMd2Header_t	*md2Header;
	int				fileLen;

	if (Com_ClientState () != CA_CONNECTED)
		return;

	//
	// If downloading is off by option, skip to loading the map
	//
	if (!allow_download->intVal && cl_downloadCheck < ENV_CNT)
		cl_downloadCheck = ENV_CNT;

	if (cl_downloadCheck == CS_MODELS) {
		// Confirm map
		cl_downloadCheck = CS_MODELS+2; // 0 isn't used
		if (allow_download_maps->intVal) {
			if (!CL_CheckOrDownloadFile (cl.configStrings[CS_MODELS+1]))
				return; // started a download
		}
	}

	//
	// Models
	//
	if (cl_downloadCheck >= CS_MODELS && cl_downloadCheck < CS_MODELS+MAX_CS_MODELS) {
		if (allow_download_models->intVal) {
			while (cl_downloadCheck < CS_MODELS+MAX_CS_MODELS && cl.configStrings[cl_downloadCheck][0]) {
				if (cl.configStrings[cl_downloadCheck][0] == '*' || cl.configStrings[cl_downloadCheck][0] == '#') {
					cl_downloadCheck++;
					continue;
				}
				if (cl_downloadModelSkin == 0) {
					if (!CL_CheckOrDownloadFile (cl.configStrings[cl_downloadCheck])) {
						cl_downloadModelSkin = 1;
						return; // Started a download
					}
					cl_downloadModelSkin = 1;
				}

				// Checking for skins in the model
				if (!cl_downloadModel) {
					fileLen = FS_LoadFile (cl.configStrings[cl_downloadCheck], (void **)&cl_downloadModel, false);
					if (!cl_downloadModel || fileLen <= 0) {
						cl_downloadModelSkin = 0;
						cl_downloadCheck++;
						cl_downloadModel = NULL;
						continue; // Couldn't load it
					}
					// Hacks! yay!
					if (LittleLong (*(uint32 *)cl_downloadModel) != MD2_HEADER) {
						cl_downloadModelSkin = 0;
						cl_downloadCheck++;

						FS_FreeFile (cl_downloadModel);
						cl_downloadModel = NULL;
						continue; // Not an alias model
					}
					md2Header = (dMd2Header_t *)cl_downloadModel;
					if (LittleLong (md2Header->version) != MD2_MODEL_VERSION) {
						cl_downloadCheck++;
						cl_downloadModelSkin = 0;

						FS_FreeFile (cl_downloadModel);
						cl_downloadModel = NULL;
						continue; // Couldn't load it
					}
				}

				md2Header = (dMd2Header_t *)cl_downloadModel;
				while (cl_downloadModelSkin - 1 < LittleLong (md2Header->numSkins)) {
					if (!CL_CheckOrDownloadFile ((char *)cl_downloadModel +
						LittleLong (md2Header->ofsSkins) +
						(cl_downloadModelSkin - 1)*MD2_MAX_SKINNAME)) {
						cl_downloadModelSkin++;

						FS_FreeFile (cl_downloadModel);
						cl_downloadModel = NULL;
						return; // Started a download
					}
					cl_downloadModelSkin++;
				}

				if (cl_downloadModel) { 
					FS_FreeFile (cl_downloadModel);
					cl_downloadModel = NULL;
				}

				cl_downloadModelSkin = 0;
				cl_downloadCheck++;
			}
		}

		cl_downloadCheck = CS_SOUNDS;
	}

	//
	// Sound
	//
	if (cl_downloadCheck >= CS_SOUNDS && cl_downloadCheck < CS_SOUNDS+MAX_CS_SOUNDS) { 
		if (allow_download_sounds->intVal) {
			if (cl_downloadCheck == CS_SOUNDS)
				cl_downloadCheck++; // zero is blank

			while (cl_downloadCheck < CS_SOUNDS+MAX_CS_SOUNDS && cl.configStrings[cl_downloadCheck][0]) {
				if (cl.configStrings[cl_downloadCheck][0] == '*') {
					cl_downloadCheck++;
					continue;
				}

				Q_snprintfz (fileName, sizeof(fileName), "sound/%s", cl.configStrings[cl_downloadCheck++]);
				if (!CL_CheckOrDownloadFile (fileName))
					return; // started a download
			}
		}

		cl_downloadCheck = CS_IMAGES;
	}

	//
	// Images
	//
	if (cl_downloadCheck >= CS_IMAGES && cl_downloadCheck < CS_IMAGES+MAX_CS_IMAGES) {
		if (cl_downloadCheck == CS_IMAGES)
			cl_downloadCheck++; // zero is blank

		while (cl_downloadCheck < CS_IMAGES+MAX_CS_IMAGES && cl.configStrings[cl_downloadCheck][0]) {
			Q_snprintfz (fileName, sizeof(fileName), "pics/%s.pcx", cl.configStrings[cl_downloadCheck++]);
			if (!CL_CheckOrDownloadFile (fileName))
				return; // started a download
		}

		cl_downloadCheck = CS_PLAYERSKINS;
	}

	//
	// Skins are special, since a player has three things to download:
	// - model
	// - weapon model
	// - weapon skin
	// - skin
	// - icon
	// So cl_downloadCheck is now * 5
	//
	if (cl_downloadCheck >= CS_PLAYERSKINS && cl_downloadCheck < CS_PLAYERSKINS+MAX_CS_CLIENTS*PLAYER_MULT) {
		if (allow_download_players->intVal) {
			while (cl_downloadCheck < CS_PLAYERSKINS + MAX_CS_CLIENTS * PLAYER_MULT) {
				int		i, n;
				char	model[MAX_QPATH];
				char	skin[MAX_QPATH];
				char	*p;

				i = (cl_downloadCheck - CS_PLAYERSKINS)/PLAYER_MULT;
				n = (cl_downloadCheck - CS_PLAYERSKINS)%PLAYER_MULT;

				if (!cl.configStrings[CS_PLAYERSKINS+i][0]) {
					cl_downloadCheck = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
					continue;
				}

				if ((p = strchr(cl.configStrings[CS_PLAYERSKINS+i], '\\')) != NULL)
					p++;
				else
					p = cl.configStrings[CS_PLAYERSKINS+i];

				Q_strncpyz (model, p, sizeof(model));
				p = strchr (model, '/');
				if (!p)
					p = strchr (model, '\\');
				if (p) {
					*p++ = 0;
					Q_strncpyz (skin, p, sizeof(skin));
				}
				else
					*skin = 0;

				switch (n) {
				case 0:
					// Model
					Q_snprintfz (fileName, sizeof(fileName), "players/%s/tris.md2", model);
					if (!CL_CheckOrDownloadFile (fileName)) {
						cl_downloadCheck = CS_PLAYERSKINS + i * PLAYER_MULT + 1;
						return; // started a download
					}
					n++;
					// FALL THROUGH

				case 1:
					// Weapon model
					Q_snprintfz (fileName, sizeof(fileName), "players/%s/weapon.md2", model);
					if (!CL_CheckOrDownloadFile (fileName)) {
						cl_downloadCheck = CS_PLAYERSKINS + i * PLAYER_MULT + 2;
						return; // started a download
					}
					n++;
					// FALL THROUGH

				case 2:
					// Weapon skin
					Q_snprintfz (fileName, sizeof(fileName), "players/%s/weapon.pcx", model);
					if (!CL_CheckOrDownloadFile (fileName)) {
						cl_downloadCheck = CS_PLAYERSKINS + i * PLAYER_MULT + 3;
						return; // started a download
					}
					n++;
					// FALL THROUGH

				case 3:
					// Skin
					Q_snprintfz (fileName, sizeof(fileName), "players/%s/%s.pcx", model, skin);
					if (!CL_CheckOrDownloadFile (fileName)) {
						cl_downloadCheck = CS_PLAYERSKINS + i * PLAYER_MULT + 4;
						return; // started a download
					}
					n++;
					// FALL THROUGH

				case 4:
					// Skin_i
					Q_snprintfz (fileName, sizeof(fileName), "players/%s/%s_i.pcx", model, skin);
					if (!CL_CheckOrDownloadFile (fileName)) {
						cl_downloadCheck = CS_PLAYERSKINS + i * PLAYER_MULT + 5;
						return; // started a download
					}

					// Move on to next model
					cl_downloadCheck = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
				}
			}
		}

		// Precache phase completed
		cl_downloadCheck = ENV_CNT;
	}

	//
	// Map
	//
	if (cl_downloadCheck == ENV_CNT) {
		cl_downloadCheck++;

		CM_LoadMap (cl.configStrings[CS_MODELS+1], true, &mapCheckSum);
		if (mapCheckSum != atoi(cl.configStrings[CS_MAPCHECKSUM])) {
			Com_Error (ERR_DROP, "Local map version differs from server: %i != '%s'", mapCheckSum, cl.configStrings[CS_MAPCHECKSUM]);
			return;
		}
	}

	//
	// Sky box env
	//
	if (cl_downloadCheck > ENV_CNT && cl_downloadCheck < TEXTURE_CNT) {
		if (allow_download->intVal && allow_download_maps->intVal) {
			while (cl_downloadCheck < TEXTURE_CNT) {
				int n = cl_downloadCheck++ - ENV_CNT - 1;

				if (n & 1)
					Q_snprintfz (fileName, sizeof(fileName), "env/%s%s.pcx", cl.configStrings[CS_SKY], skySuffix[n/2]);
				else
					Q_snprintfz (fileName, sizeof(fileName), "env/%s%s.tga", cl.configStrings[CS_SKY], skySuffix[n/2]);

				if (!CL_CheckOrDownloadFile (fileName))
					return; // Started a download
			}
		}

		cl_downloadCheck = TEXTURE_CNT;
	}

	if (cl_downloadCheck == TEXTURE_CNT) {
		cl_downloadCheck++;
		cl_downloadTexNum = 0;
	}

	//
	// Confirm existance of textures, download any that don't exist
	//
	if (cl_downloadCheck == TEXTURE_CNT+1) {
		int			numTexInfo;

		numTexInfo = CM_NumTexInfo ();
		if (allow_download->intVal && allow_download_maps->intVal) {
			while (cl_downloadTexNum < numTexInfo) {
				Q_snprintfz (fileName, sizeof(fileName), "textures/%s.wal", CM_SurfRName (cl_downloadTexNum++));
				if (!CL_CheckOrDownloadFile (fileName))
					return; // Started a download
			}
		}

		cl_downloadCheck = TEXTURE_CNT+999;
	}

	// All done!
	CL_CGModule_LoadMap ();

	cls.netChan.message.WriteByte (CLC_STRINGCMD);
	cls.netChan.message.WriteString (Q_VarArgs ("begin %i\n", cl_downloadSpawnCount));
	cls.forcePacket = true;
}

/*
=======================================================================

	HTTP DOWNLOADING

=======================================================================
*/

#ifdef CL_HTTPDL

/*
==============
CL_HTTPDL_Init
==============
*/
void CL_HTTPDL_Init ()
{
}


/*
==============
CL_HTTPDL_SetServer
==============
*/
void CL_HTTPDL_SetServer (char *url)
{
}


/*
==============
CL_HTTPDL_CancelDownloads
==============
*/
void CL_HTTPDL_CancelDownloads (bool permKill)
{
}


/*
==============
CL_HTTPDL_QueueDownload
==============
*/
bool CL_HTTPDL_QueueDownload (char *file)
{
	return false;
}


/*
==============
CL_HTTPDL_PendingDownloads
==============
*/
bool CL_HTTPDL_PendingDownloads ()
{
	return false;
}


/*
==============
CL_HTTPDL_Cleanup
==============
*/
void CL_HTTPDL_Cleanup (bool shutdown)
{
}


/*
==============
CL_HTTPDL_RunDownloads
==============
*/
void CL_HTTPDL_RunDownloads ()
{
}

#endif
