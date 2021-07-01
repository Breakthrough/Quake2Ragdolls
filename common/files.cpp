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
// files.c
//

#include "common.h"
#include "../minizip/unzip.h"
#include "../lzma/7z.h"
#include "../lzma/7zAlloc.h"
#include "../lzma/7zCrc.h"
#include "../lzma/7zFile.h"
#include "../lzma/7zVersion.h"

#define FS_MAX_PAKS			1024
#define FS_MAX_HASHSIZE		1024
#define FS_MAX_FILEINDICES	1024

cVar_t	*fs_basedir;
cVar_t	*fs_cddir;
cVar_t	*fs_game;
cVar_t	*fs_gamedircvar;
cVar_t	*fs_defaultPaks;

/*
=============================================================================

	IN-MEMORY PAK FILES

=============================================================================
*/

struct mPackFile_t
{
	char					fileName[MAX_QPATH];

	int						filePos;
	int						fileLen;

	mPackFile_t				*hashNext;
};

enum PackType
{
	PT_PAK,
	PT_UNZ,
	PT_LZMA
};

/*abstract*/ struct mPackBase_t
{
protected:
	mPackBase_t (PackType type): 
	  type(type)
	{
	};
	  
public:
	PackType				type;
	char					name[MAX_OSPATH];

	// Information
	int						numFiles;
	mPackFile_t				*files;

	mPackFile_t				*fileHashTree[FS_MAX_HASHSIZE];

	virtual void Close() = 0;
	virtual int OpenFile(struct fsHandleIndex_t *handle, mPackFile_t *searchFile) = 0;
};

struct mPakPack_t : mPackBase_t
{
	mPakPack_t (FILE *file) :
	  mPackBase_t(PT_PAK),
	  pak(file)
	  {
	  };

	// Standard
	FILE					*pak;

	void Close()
	{
		fclose(pak);
	}

	int OpenFile(struct fsHandleIndex_t *handle, mPackFile_t *searchFile);
};

struct mUnzPack_t : mPackBase_t
{
	mUnzPack_t (unzFile file) :
	  mPackBase_t(PT_UNZ),
	  pak(file)
	  {
	  };

	// Standard
	unzFile					pak;

	void Close ()
	{
		gzclose(pak);
	}

	int OpenFile(struct fsHandleIndex_t *handle, mPackFile_t *searchFile);
};

/*
=============================================================================

	FILESYSTEM FUNCTIONALITY

=============================================================================
*/

struct fsLink_t {
	fsLink_t				*next;
	char					*from;
	int						fromLength;
	char					*to;
};

struct fsPath_t {
	char					pathName[MAX_OSPATH];
	char					gamePath[MAX_OSPATH];
	mPackBase_t				*package;

	fsPath_t				*next;
};

static char		fs_gameDir[MAX_OSPATH];

static fsLink_t	*fs_fileLinks;

static fsPath_t	*fs_searchPaths;
static fsPath_t	**fs_invSearchPaths;
static int		fs_numInvSearchPaths;
static fsPath_t	*fs_baseSearchPath;		// Without gamedirs

/*
=============================================================================

	FILE INDEXING

=============================================================================
*/

struct fsHandleIndex_t {
	char					name[MAX_QPATH];

	bool					inUse;
	EFSOpenMode				openMode;

	// One of these is always NULL
	FILE					*regFile;
	unzFile					*pkzFile;
};

static fsHandleIndex_t	fs_fileIndices[FS_MAX_FILEINDICES];

// Functions
int mPakPack_t::OpenFile(fsHandleIndex_t *handle, mPackFile_t *searchFile)
{
	if (fs_developer->intVal)
		Com_Printf(0, "FS_OpenFileRead: pack file %s : %s\n", name, handle->name);

	// Open a new file on the pakfile
	handle->regFile = fopen(name, "rb");
	if (handle->regFile)
	{
		fseek(handle->regFile, searchFile->filePos, SEEK_SET);
		return searchFile->fileLen;
	}

	return -1;
}

int mUnzPack_t::OpenFile(fsHandleIndex_t *handle, mPackFile_t *searchFile)
{
	if (fs_developer->intVal)
		Com_Printf(0, "FS_OpenFileRead: pkp file %s : %s\n", name, handle->name);

	handle->pkzFile = (unzFile*)unzOpen(name);
	if (handle->pkzFile)
	{
		if (unzSetOffset(handle->pkzFile, searchFile->filePos) == UNZ_OK)
		{
			if (unzOpenCurrentFile(handle->pkzFile) == UNZ_OK)
				return searchFile->fileLen;
		}
	}

	// Failed to locate/open
	unzClose(handle->pkzFile);

	return -1;
}

/*
=============================================================================

	ZLIB FUNCTIONS

=============================================================================
*/

/*
================
FS_ZLibDecompress
================
*/
int FS_ZLibDecompress (byte *in, int inLen, byte *out, int outLen, int wbits)
{
	z_stream	zs;
	int			result;

	memset (&zs, 0, sizeof(zs));

	zs.next_in = in;
	zs.avail_in = 0;

	zs.next_out = out;
	zs.avail_out = outLen;

	result = inflateInit2 (&zs, wbits);
	if (result != Z_OK) {
		Sys_Error ("Error on inflateInit %d\nMessage: %s\n", result, zs.msg);
		return 0;
	}

	zs.avail_in = inLen;

	result = inflate (&zs, Z_FINISH);
	if (result != Z_STREAM_END) {
		Sys_Error ("Error on inflate %d\nMessage: %s\n", result, zs.msg);
		zs.total_out = 0;
	}

	result = inflateEnd (&zs);
	if (result != Z_OK) {
		Sys_Error ("Error on inflateEnd %d\nMessage: %s\n", result, zs.msg);
		return 0;
	}

	return zs.total_out;
}


/*
================
FS_ZLibCompressChunk
================
*/
int FS_ZLibCompressChunk (byte *in, int inLen, byte *out, int outLen, int method, int wbits)
{
	z_stream	zs;
	int			result;

	zs.next_in = in;
	zs.avail_in = inLen;
	zs.total_in = 0;

	zs.next_out = out;
	zs.avail_out = outLen;
	zs.total_out = 0;

	zs.msg = NULL;
	zs.state = NULL;
	zs.zalloc = Z_NULL;
	zs.zfree = Z_NULL;
	zs.opaque = NULL;

	zs.data_type = Z_BINARY;
	zs.adler = 0;
	zs.reserved = 0;

	result = deflateInit2 (&zs, method, Z_DEFLATED, wbits, 9, Z_DEFAULT_STRATEGY);
	if (result != Z_OK)
		return 0;

	result = deflate(&zs, Z_FINISH);
	if (result != Z_STREAM_END)
		return 0;

	result = deflateEnd(&zs);
	if (result != Z_OK)
		return 0;

	return zs.total_out;
}

/*
=============================================================================

	HELPER FUNCTIONS

=============================================================================
*/

/*
================
__FileLen
================
*/
static int __FileLen(FILE *f)
{
	int pos = ftell(f);
	fseek(f, 0, SEEK_END);
	int end = ftell(f);
	fseek(f, pos, SEEK_SET);

	return end;
}


/*
================
__FileLen
================
*/
static int __FileLen(unzFile *f)
{
	int pos = gztell(f);
	gzseek(f, 0, SEEK_END);
	int end = gztell(f);
	gzseek(f, pos, SEEK_SET);

	return end;
}


/*
============
FS_CreatePath

Creates any directories needed to store the given filename
============
*/
void FS_CreatePath(char *path)
{
	for (char *ofs=path+1 ; *ofs ; ofs++)
	{
		if (*ofs == '/')
		{
			// Create the directory
			*ofs = 0;
			Sys_Mkdir (path);
			*ofs = '/';
		}
	}
}


/*
================
FS_CopyFile
================
*/
void FS_CopyFile(const char *src, const char *dst)
{
	if (fs_developer->intVal)
		Com_Printf(0, "FS_CopyFile (%s, %s)\n", src, dst);

	FILE *f1 = fopen(src, "rb");
	if (!f1)
		return;
	FILE *f2 = fopen(dst, "wb");
	if (!f2)
	{
		fclose(f1);
		return;
	}

	byte buffer[4096];
	while (true)
	{
		int l = fread(buffer, 1, sizeof(buffer), f1);
		if (!l)
			break;
		fwrite(buffer, 1, l, f2);
	}

	fclose(f1);
	fclose(f2);
}

void FS_DeleteFile (const char *src)
{
	remove(src);
}

void FS_RenameFile (const char *src, const char *dst)
{
	FS_CopyFile(src, dst);
	FS_DeleteFile(src);
}

/*
=============================================================================

	FILE HANDLING

=============================================================================
*/

/*
=================
FS_GetFreeHandle
=================
*/
static fileHandle_t FS_GetFreeHandle (fsHandleIndex_t **handle)
{
	fileHandle_t		i;
	fsHandleIndex_t		*hIndex;

	for (i=0, hIndex=fs_fileIndices ; i<FS_MAX_FILEINDICES ; hIndex++, i++) {
		if (hIndex->inUse)
			continue;

		hIndex->inUse = true;
		*handle = hIndex;
		return i+1;
	}

	Com_Error (ERR_FATAL, "FS_GetFreeHandle: no free handles!");
	return 0;
}


/*
=================
FS_GetHandle
=================
*/
static fsHandleIndex_t *FS_GetHandle (fileHandle_t fileNum)
{
	fsHandleIndex_t *hIndex;

	if (fileNum < 0 || fileNum > FS_MAX_FILEINDICES)
		Com_Error (ERR_FATAL, "FS_GetHandle: invalid file number");

	hIndex = &fs_fileIndices[fileNum-1];
	if (!hIndex->inUse)
		Com_Error (ERR_FATAL, "FS_GetHandle: invalid handle index");

	return hIndex;
}


/*
============
FS_FileLength

Returns the TOTAL file size, not the position.
Make sure to move the position back after moving to the beginning of the file for the size lookup.
============
*/
int FS_FileLength(fileHandle_t fileNum)
{
	fsHandleIndex_t *handle = FS_GetHandle(fileNum);
	if (handle->regFile)
	{
		return __FileLen(handle->regFile);
	}
	else if (handle->pkzFile)
	{
		// FIXME
		return __FileLen(handle->pkzFile);
	}

	// Shouldn't happen...
	assert (0);
	return -1;
}

int FS_FileLength(const char *fileName)
{
	fileHandle_t handle;
	int len = FS_OpenFile (fileName, &handle, FS_MODE_READ_BINARY);
	FS_CloseFile(handle);
	return len;
}

/*
============
FS_Tell

Returns the current file position.
============
*/
int FS_Tell(fileHandle_t fileNum)
{
	fsHandleIndex_t *handle = FS_GetHandle(fileNum);
	if (handle->regFile)
		return ftell(handle->regFile);
	else if (handle->pkzFile)
		return unztell(handle->pkzFile);

	// Shouldn't happen...
	assert (0);
	return 0;
}


/*
=================
FS_Read

Properly handles partial reads
=================
*/

int FS_Read(void *buffer, const int len, fileHandle_t fileNum)
{
	fsHandleIndex_t *handle = FS_GetHandle(fileNum);
	if (handle->openMode != FS_MODE_READ_BINARY &&
		handle->openMode != FS_MODE_READ_WRITE_BINARY)
		Com_Error (ERR_FATAL, "FS_Read: %s: was not opened in read mode", handle->name);

	// Read in chunks for progress bar
	int remaining = len;
	byte *buf = (byte *)buffer;

	if (handle->regFile)
	{
		// File
		while (remaining)
		{
			int read = fread(buf, 1, remaining, handle->regFile);
			switch(read)
			{
			case 0:
				if (fs_developer->intVal)
					Com_Printf(0, "FS_Read: 0 bytes read from \"%s\"", handle->name);
				return len - remaining;

			case -1:
				Com_Error(ERR_FATAL, "FS_Read: -1 bytes read from \"%s\"", handle->name);
				break;
			}

			// Do some progress bar thing here
			remaining -= read;
			buf += read;
		}

		return len;
	}
	else if (handle->pkzFile)
	{
		// Zip file
		while (remaining)
		{
			int read = unzReadCurrentFile(handle->pkzFile, buf, remaining);
			switch(read)
			{
			case 0:
				if (fs_developer->intVal)
					Com_Printf(0, "FS_Read: 0 bytes read from \"%s\"", handle->name);
				return len - remaining;

			case -1:
				Com_Error(ERR_FATAL, "FS_Read: -1 bytes read from \"%s\"", handle->name);
				break;
			}

			// Do some progress bar thing here
			remaining -= read;
			buf += read;
		}

		return len;
	}

	// Shouldn't happen...
	assert(0);
	return 0;
}


/*
=================
FS_Write
=================
*/
int FS_Write(void *buffer, const int size, fileHandle_t fileNum)
{
	fsHandleIndex_t *handle = FS_GetHandle(fileNum);
	switch(handle->openMode)
	{
	case FS_MODE_WRITE_BINARY:
	case FS_MODE_APPEND_BINARY:
	case FS_MODE_WRITE_TEXT:
	case FS_MODE_APPEND_TEXT:
	case FS_MODE_READ_WRITE_BINARY:
		break;
	default:
		Com_Error(ERR_FATAL, "FS_Write: %s: was no opened in append/write mode", handle->name);
		break;
	}
	if (size < 0)
		Com_Error(ERR_FATAL, "FS_Write: size < 0");

	// Write
	int remaining = size;
	byte *buf = (byte *)buffer;

	if (handle->regFile)
	{
		// File
		while (remaining)
		{
			int write = fwrite(buf, 1, remaining, handle->regFile);

			switch(write)
			{
			case 0:
				if (fs_developer->intVal)
					Com_Printf(PRNT_ERROR, "FS_Write: 0 bytes written to %s\n", handle->name);
				return size - remaining;

			case -1:
				Com_Error(ERR_FATAL, "FS_Write: -1 bytes written to %s", handle->name);
				break;
			}

			remaining -= write;
			buf += write;
		}

		return size;
	}
	else if (handle->pkzFile)
	{
		// Zip file
		Com_Error(ERR_FATAL, "FS_Write: can't write to zip file %s", handle->name);
	}

	// Shouldn't happen...
	assert (0);
	return 0;
}


/*
=================
FS_Seek
=================
*/
void FS_Seek(fileHandle_t fileNum, const int offset, const EFSSeekOrigin seekOrigin)
{
	fsHandleIndex_t *handle = FS_GetHandle(fileNum);
	if (handle->regFile)
	{
		// Seek through a regular file
		switch(seekOrigin)
		{
		case FS_SEEK_SET:
			fseek(handle->regFile, offset, SEEK_SET);
			break;

		case FS_SEEK_CUR:
			fseek(handle->regFile, offset, SEEK_CUR);
			break;

		case FS_SEEK_END:
			fseek(handle->regFile, offset, SEEK_END);
			break;

		default:
			Com_Error(ERR_FATAL, "FS_Seek: bad origin (%i)", seekOrigin);
			break;
		}
	}
	else if (handle->pkzFile)
	{
		// Seek through a zip
		int remaining;
		switch (seekOrigin)
		{
		case FS_SEEK_SET:
			remaining = offset;
			break;

		case FS_SEEK_CUR:
			remaining = offset + unztell (handle->pkzFile);
			break;

		case FS_SEEK_END:
			{
				unz_file_info info;
				unzGetCurrentFileInfo(handle->pkzFile, &info, NULL, 0, NULL, 0, NULL, 0);

				remaining = offset + info.uncompressed_size;
			}
			break;

		default:
			Com_Error(ERR_FATAL, "FS_Seek: bad origin (%i)", seekOrigin);
		}

		// Reopen the file
		unzCloseCurrentFile(handle->pkzFile);
		unzOpenCurrentFile(handle->pkzFile);

		// Skip until the desired offset is reached
		while (remaining)
		{
			int len = remaining;

			auto dummy = new byte[len];
			int r = unzReadCurrentFile(handle->pkzFile, dummy, len);	
			delete[] dummy;

			if (r <= 0)
				break;

			remaining -= r;
		}
	}
	else
	{
		assert (0);
	}
}


/*
==============
FS_OpenFileAppend
==============
*/
static int FS_OpenFileAppend(fsHandleIndex_t *handle, bool binary)
{
	char path[MAX_OSPATH];
	Q_snprintfz(path, sizeof(path), "%s/%s", fs_gameDir, handle->name);

	// Create the path if it doesn't exist
	FS_CreatePath(path);

	// Open
	handle->regFile = fopen(path, (binary) ? "ab" : "at");

	// Return length
	if (handle->regFile)
	{
		if (fs_developer->intVal)
			Com_Printf(0, "FS_OpenFileAppend: \"%s\"", path);
		return __FileLen(handle->regFile);
	}

	// Failed to open
	if (fs_developer->intVal)
		Com_Printf(0, "FS_OpenFileAppend: couldn't open \"%s\"", path);
	return -1;
}


/*
==============
FS_OpenFileWrite
==============
*/
static int FS_OpenFileWrite(fsHandleIndex_t *handle, bool binary)
{
	char path[MAX_OSPATH];
	Q_snprintfz(path, sizeof(path), "%s/%s", fs_gameDir, handle->name);

	// Create the path if it doesn't exist
	FS_CreatePath(path);

	// Open
	handle->regFile = fopen (path, (binary) ? "wb" : "wt");

	// Return length
	if (handle->regFile)
	{
		if (fs_developer->intVal)
			Com_Printf(0, "FS_OpenFileWrite: \"%s\"", path);
		return 0;
	}

	// Failed to open
	if (fs_developer->intVal)
		Com_Printf(0, "FS_OpenFileWrite: couldn't open \"%s\"", path);
	return -1;
}


/*
==============
FS_OpenFileReadWrite
==============
*/
static int FS_OpenFileReadWrite(fsHandleIndex_t *handle, bool binary)
{
	char path[MAX_OSPATH];
	Q_snprintfz(path, sizeof(path), "%s/%s", fs_gameDir, handle->name);

	// Create the path if it doesn't exist
	FS_CreatePath(path);

	// Open
	handle->regFile = fopen (path, (binary) ? "r+b" : "r+t");

	// Return length
	if (handle->regFile)
	{
		if (fs_developer->intVal)
			Com_Printf(0, "FS_OpenFileWrite: \"%s\"", path);
		return __FileLen(handle->regFile);
	}

	// Failed to open
	if (fs_developer->intVal)
		Com_Printf(0, "FS_OpenFileWrite: couldn't open \"%s\"", path);
	return -1;
}


/*
===========
FS_OpenFileRead

Finds the file in the search path.
returns filesize and an open FILE *
Used for streaming data out of either a pak file or
a seperate file.
===========
*/
bool fs_fileFromPak = false;
static int FS_OpenFileRead(fsHandleIndex_t *handle)
{
	fs_fileFromPak = false;
	// Check for links first
	for (fsLink_t *link=fs_fileLinks ; link ; link=link->next)
	{
		if (!strncmp (handle->name, link->from, link->fromLength))
		{
			char netPath[MAX_OSPATH];
			Q_snprintfz(netPath, sizeof(netPath), "%s%s", link->to, handle->name+link->fromLength);

			// Open
			handle->regFile = fopen(netPath, "rb");

			// Return length
			if (fs_developer->intVal)
				Com_Printf(0, "FS_OpenFileRead: link file: %s\n", netPath);
			if (handle->regFile)
				return __FileLen(handle->regFile);

			// Failed to load
			return -1;
		}
	}

	// Calculate hash value
	const uint32 hashValue = Com_HashFileName(handle->name, FS_MAX_HASHSIZE);

	// Search through the path, one element at a time
	for (fsPath_t *searchPath=fs_searchPaths ; searchPath ; searchPath=searchPath->next)
	{
		// Is the element a pak file?
		if (searchPath->package)
		{
			// Look through all the pak file elements
			mPackBase_t *package = searchPath->package;

			for (mPackFile_t *searchFile=package->fileHashTree[hashValue] ; searchFile ; searchFile=searchFile->hashNext)
			{
				if (Q_stricmp(searchFile->fileName, handle->name))
					continue;

				// Found it!
				fs_fileFromPak = true;

				int len = package->OpenFile(handle, searchFile);


				if (len <= 0)
					Com_Error(ERR_FATAL, "FS_OpenFileRead: couldn't reopen \"%s\"", handle->name);
			
				return len;
			}
		}
		else
		{
			// Check a file in the directory tree
			char netPath[MAX_OSPATH];
			Q_snprintfz(netPath, sizeof(netPath), "%s/%s", searchPath->pathName, handle->name);

			handle->regFile = fopen(netPath, "rb");
			if (handle->regFile)
			{
				// Found it!
				if (fs_developer->intVal)
					Com_Printf(0, "FS_OpenFileRead: %s\n", netPath);
				return __FileLen(handle->regFile);
			}
		}
	}

	if (fs_developer->intVal)
		Com_Printf(0, "FS_OpenFileRead: can't find %s\n", handle->name);

	return -1;
}


/*
===========
FS_OpenFile
===========
*/
int FS_OpenFile(const char *fileName, fileHandle_t *fileNum, const EFSOpenMode openMode)
{
	fsHandleIndex_t	*handle;
	int				fileSize;

	*fileNum = FS_GetFreeHandle(&handle);

	Com_NormalizePath(handle->name, sizeof(handle->name), fileName);
	handle->openMode = openMode;

	// Open under the desired mode
	switch(openMode)
	{
	case FS_MODE_APPEND_BINARY:
		fileSize = FS_OpenFileAppend(handle, true);
		break;
	case FS_MODE_APPEND_TEXT:
		fileSize = FS_OpenFileAppend(handle, false);
		break;

	case FS_MODE_READ_BINARY:
		fileSize = FS_OpenFileRead(handle);
		break;

	case FS_MODE_WRITE_BINARY:
		fileSize = FS_OpenFileWrite(handle, true);
		break;
	case FS_MODE_WRITE_TEXT:
		fileSize = FS_OpenFileWrite(handle, false);
		break;

	case FS_MODE_READ_WRITE_BINARY:
		fileSize = FS_OpenFileReadWrite(handle, true);
		break;

	default:
		Com_Error(ERR_FATAL, "FS_OpenFile: %s: invalid mode '%i'", handle->name, openMode);
		break;
	}

	// Failed
	if (fileSize == -1)
	{
		memset(handle, 0, sizeof(fsHandleIndex_t));
		*fileNum = 0;
	}

	return fileSize;
}


/*
==============
FS_CloseFile
==============
*/
void FS_CloseFile(fileHandle_t fileNum)
{
	// Get local handle
	fsHandleIndex_t *handle = FS_GetHandle(fileNum);
	if (!handle->inUse)
		return;

	// Close file/zip
	if (handle->regFile)
	{
		fclose(handle->regFile);
		handle->regFile = NULL;
	}
	else if (handle->pkzFile)
	{
		unzCloseCurrentFile(handle->pkzFile);
		unzClose(handle->pkzFile);
		handle->pkzFile = NULL;
	}
	else
	{
		assert(0);
	}

	// Clear handle
	handle->inUse = false;
	handle->name[0] = '\0';
}

// ==========================================================================

/*
============
FS_LoadFile

Filename are reletive to the egl search path.
A NULL buffer will just return the file length without loading.
-1 is returned if it wasn't found, 0 is returned if it's a blank file. In both cases a buffer is set to NULL.
============
*/
int FS_LoadFile(const char *path, void **buffer, const bool terminate)
{
	// Look for it in the filesystem or pack files
	fileHandle_t fileNum;
	int fileLen = FS_OpenFile(path, &fileNum, FS_MODE_READ_BINARY);
	if (!fileNum || fileLen <= 0)
	{
		if (buffer)
			*buffer = NULL;
		if (fileNum)
			FS_CloseFile (fileNum);
		if (fileLen >= 0)
			return 0;
		return -1;
	}

	// Just needed to get the length
	if (!buffer)
	{
		FS_CloseFile(fileNum);
		return fileLen;
	}

	// Allocate a local buffer
	uint32 termLen;
	if (terminate)
	{
		termLen = 2;
	}
	else
	{
		termLen = 0;
	}
	byte *buf = (byte*)Mem_PoolAlloc(fileLen+termLen, com_fileSysPool, 0);
	*buffer = buf;

	// Copy the file data to a local buffer
	FS_Read(buf, fileLen, fileNum);
	FS_CloseFile(fileNum);

	// Terminate if desired
	if (termLen)
		strncpy((char *)buf+fileLen, "\n\0", termLen);
	return fileLen+termLen;
}


/*
=============
_FS_FreeFile
=============
*/
void _FS_FreeFile(void *buffer, const char *fileName, const int fileLine)
{
	if (buffer)
		_Mem_Free(buffer, fileName, fileLine);
}

// ==========================================================================

/*
============
FS_FileExists

Filename are reletive to the egl search path.
Just like calling FS_LoadFile with a NULL buffer.
============
*/
int FS_FileExists(const char *path)
{
	// Look for it in the filesystem or pack files
	fileHandle_t fileNum;
	int fileLen = FS_OpenFile(path, &fileNum, FS_MODE_READ_BINARY);
	if (!fileNum || fileLen <= 0)
		return -1;

	// Just needed to get the length
	FS_CloseFile(fileNum);
	return fileLen;
}

/*
=============================================================================

	PACKAGE LOADING

=============================================================================
*/

#include <vector>
class LnkWindowsShortcut
{
public:
	String FileName;

	struct LnkShellLinkHeader
	{
		struct CLSID
		{
			uint32		P1;
			uint16		P2;
			uint16		P3;
			uint16		P4;

			uint32		P5a;
			uint16		P5b;
		};

		typedef uint32 LinkFlagsFlags;
		enum
		{
			LF_HasLinkTargetIDList				= BIT(0),
			LF_HasLinkInfo						= BIT(1),
			LF_HasName							= BIT(2),
			LF_HasRelativePath					= BIT(3),
			LF_HasWorkingDir					= BIT(4),
			LF_HasArguments						= BIT(5),
			LF_HasIconLocation					= BIT(6),
			LF_IsUnicode						= BIT(7),
			LF_ForceNoLinkInfo					= BIT(8),
			LF_HasExpString						= BIT(9),
			LF_RunInSeparateProcess				= BIT(10),
			LF_Unused1							= BIT(11),
			LF_HasDarwinID						= BIT(12),
			LF_RunAsUser						= BIT(13),
			LF_HasExpIcon						= BIT(14),
			LF_NoPidlAlias						= BIT(15),
			LF_Unused2							= BIT(16),
			LF_RunWithShimLayer					= BIT(17),
			LF_ForceNoLinkTrack					= BIT(18),
			LF_EnableTargetMetadata				= BIT(19),
			LF_DisableLinkPathTracking			= BIT(20),
			LF_DisableKnownFolderTracking		= BIT(21),
			LF_DisableKnownAliasFolder			= BIT(22),
			LF_AllowLinkToLink					= BIT(23),
			LF_UnaliasOnSave					= BIT(24),
			LF_PreferEnvironmentPath			= BIT(25),
			LF_KeepLocalIDListForUNCTarget		= BIT(26),
		};

		typedef uint32 FileAttributesFlags;
		enum
		{
			FA_FileAttributeReadOnly			= BIT(0),
			FA_FileAttributeHidden				= BIT(1),
			FA_FileAttributeSystem				= BIT(2),
			FA_Reserved1						= BIT(3),
			FA_FileAttributeDirectory			= BIT(4),
			FA_FileAttributeArchive				= BIT(5),
			FA_Reserved2						= BIT(6),
			FA_FileAttributeNormal				= BIT(7),
			FA_FileAttributeTemporary			= BIT(8),
			FA_FileAttributeSparseFile			= BIT(9),
			FA_FileAttributeReparsePoint		= BIT(10),
			FA_FileAttributeCompressed			= BIT(11),
			FA_FileAttributeOffline				= BIT(12),
			FA_FileAttributeNotContentIndexed	= BIT(13),
			FA_FileAttributeEncrypted			= BIT(14)
		};

		uint32					HeaderSize;
		union
		{
			CLSID				LinkCLSID;
			byte				LinkCLSIDBytes[16];
		};
		LinkFlagsFlags			LinkFlags;
		FileAttributesFlags		FileAttributes;
		uint64					CreationTime;
		uint64					AccessTime;
		uint64					WriteTime;
		uint32					FileSize;
		sint32					IconIndex;
		uint32					ShowCommand;
		union
		{
			uint16				HotKey;
			byte				HotKeyBytes[2];
		};
		uint16					Reserved1;
		uint32					Reserved2;
		uint32					Reserved3;

		void Read (LnkWindowsShortcut *shortcut)
		{
			long start = shortcut->Length();

			HeaderSize = shortcut->Read<uint32>();
			shortcut->ReadBytes(LinkCLSIDBytes, 16);
			LinkFlags = shortcut->Read<uint32>();
			FileAttributes = shortcut->Read<uint32>();
			CreationTime = shortcut->Read<uint64>();
			AccessTime = shortcut->Read<uint64>();
			WriteTime = shortcut->Read<uint64>();
			FileSize = shortcut->Read<uint32>();
			IconIndex = shortcut->Read<sint32>();
			ShowCommand = shortcut->Read<uint32>();
			shortcut->ReadBytes (HotKeyBytes, 2);
			Reserved1 = shortcut->Read<uint16>();
			Reserved2 = shortcut->Read<uint32>();
			Reserved3 = shortcut->Read<uint32>();

			long count = shortcut->Length() - start;

			if (count != HeaderSize)
				throw Exception();
		}
	};

	struct LnkLinkTargetIDList
	{
		struct LnkIDList
		{
			struct LnkItemID
			{
				uint16			ItemIDSize;
				byte			*Data;

				LnkItemID() :
				  Data(null)
				{
				}
			};

			TList<LnkItemID>		ItemIDs;

			~LnkIDList ()
			{
				for (uint32 i = 0; i < ItemIDs.Count(); ++i)
					delete ItemIDs[i].Data;
			}
		};

		uint16						IDListSize;
		LnkIDList					IDList;

		void Read (LnkWindowsShortcut *shortcut)
		{
			long start = shortcut->Length();

			IDListSize = shortcut->Read<uint16>();

			do
			{
				uint16 size = shortcut->Read<uint16>();

				if (size == 0)
					break;

				LnkIDList::LnkItemID item;
				item.ItemIDSize = size;
				item.Data = new byte[size - sizeof(uint16)];
				shortcut->ReadBytes(item.Data, size - sizeof(uint16));
				
				IDList.ItemIDs.Add(item);
			} while ((shortcut->Length() - start) < IDListSize);

			if ((shortcut->Length() - start) != IDListSize)
				throw Exception();

			if (shortcut->Read<uint16>() != 0)
				throw Exception();
		}
	};

	struct LnkLinkInfo
	{
		uint32					LinkInfoSize;
		uint32					LinkInfoHeaderSize;

		typedef uint32 LinkInfoFlagsFlags;
		enum
		{
			LIF_VolumeIDAndLocalBasePath				= BIT(0),
			LIF_CommonNetworkRelativeLinkAndPathSuffix	= BIT(1),
		};

		LinkInfoFlagsFlags		LinkInfoFlags;
		uint32					VolumeIDOffset;
		uint32					LocalBasePathOffset;
		uint32					CommonNetworkRelativeLinkOffset;
		uint32					CommonPathSuffixOffset;

		// Optional
		uint32					LocalBasePathOffsetUnicode;
		uint32					CommonPathSuffixOffsetUnicode;

		struct LnkVolumeID
		{
			uint32				VolumeIDSize;
			uint32				DriveType;
			uint32				DriveSerialNumber;
			uint32				VolumeLabelOffset;
			uint32				VolumeLabelOffsetUnicode;
			byte				*Data;

			LnkVolumeID() :
			  Data(null)
			  {
			  }

			~LnkVolumeID()
			{
				delete Data;
			}

			void Read (LnkWindowsShortcut *shortcut)
			{
				long start = shortcut->Length();

				VolumeIDSize = shortcut->Read<uint32>();
				DriveType = shortcut->Read<uint32>();
				DriveSerialNumber = shortcut->Read<uint32>();
				VolumeLabelOffset = shortcut->Read<uint32>();

				if (VolumeLabelOffset == 0x00000014)
				{
					VolumeLabelOffsetUnicode = shortcut->Read<uint32>();
					shortcut->SeekFromPos(start, VolumeLabelOffsetUnicode);
					shortcut->ReadStringLength((char**)&Data, VolumeIDSize - VolumeLabelOffsetUnicode);
				}
				else
				{
					shortcut->SeekFromPos(start, VolumeLabelOffset);
					shortcut->ReadStringLength((char**)&Data, VolumeIDSize - VolumeLabelOffset);
				}

				long count = (shortcut->Length() - start);

				if (count != VolumeIDSize)
					throw Exception();
			}
		};
		
		LnkVolumeID				VolumeID;
		char					*LocalBasePath;
		//network
		char					*CommonPathSuffix;

		char					*LocalBasePathUnicode;
		char					*CommonPathSuffixUnicode;

		LnkLinkInfo () :
		  LocalBasePath(null),
		  CommonPathSuffix(null),
		  LocalBasePathUnicode(null),
		  CommonPathSuffixUnicode(null)
		  {
		  }

		~LnkLinkInfo ()
		{
			if (LocalBasePath)
				delete LocalBasePath;
			if (CommonPathSuffix)
				delete CommonPathSuffix;
			if (LocalBasePathUnicode)
				delete LocalBasePathUnicode;
			if (CommonPathSuffixUnicode)
				delete CommonPathSuffixUnicode;
		}

		void Read (LnkWindowsShortcut *shortcut)
		{
			long start = shortcut->Length();

			LinkInfoSize = shortcut->Read<uint32>();
			LinkInfoHeaderSize = shortcut->Read<uint32>();
			LinkInfoFlags = shortcut->Read<uint32>();
			VolumeIDOffset = shortcut->Read<uint32>();
			LocalBasePathOffset = shortcut->Read<uint32>();
			CommonNetworkRelativeLinkOffset = shortcut->Read<uint32>();
			CommonPathSuffixOffset = shortcut->Read<uint32>();

			if (LinkInfoHeaderSize >= 0x00000024)
			{
				LocalBasePathOffsetUnicode = shortcut->Read<uint32>();
				CommonPathSuffixOffsetUnicode = shortcut->Read<uint32>();
			}

			LocalBasePath = null;

			if (LinkInfoFlags & LIF_VolumeIDAndLocalBasePath)
			{
				VolumeID.Read(shortcut);
				shortcut->ReadNullTerminatedString(&LocalBasePath);
			}

			if (LinkInfoFlags & LIF_CommonNetworkRelativeLinkAndPathSuffix)
			{
				// skip? plz?
			}

			LocalBasePathUnicode = null;
			CommonPathSuffixUnicode = null;

			shortcut->ReadNullTerminatedString(&CommonPathSuffix);

			if ((LinkInfoFlags & LIF_VolumeIDAndLocalBasePath) &&
				LinkInfoHeaderSize >= 0x00000024)
				shortcut->ReadNullTerminatedString(&LocalBasePathUnicode);

			if (LinkInfoHeaderSize >= 0x00000024)
				shortcut->ReadNullTerminatedString(&CommonPathSuffixUnicode);

			long count = (shortcut->Length() - start);

			if (count != LinkInfoSize)
				throw Exception();
		}
	};

	struct LnkStringData
	{
		struct LnkString
		{
			uint16			Count;
			wchar_t			*WData;

			void Read (LnkWindowsShortcut *shortcut)
			{
				Count = shortcut->Read<uint16>();
				shortcut->ReadStringUnicodeLength(&WData, Count);
			}

			LnkString () :
			  WData(null)
			{
			}

			~LnkString ()
			{
				if (WData)
					delete WData;
			}
		};

		LnkString			NameString;
		LnkString			RelativePath;
		LnkString			WorkingDir;
		LnkString			CommandLineArguments;
		LnkString			IconLocation;

		void Read (LnkWindowsShortcut *shortcut)
		{
			if (shortcut->Header.LinkFlags & LnkShellLinkHeader::LF_HasName)
				NameString.Read(shortcut);
			
			if (shortcut->Header.LinkFlags & LnkShellLinkHeader::LF_HasRelativePath)
				RelativePath.Read(shortcut);

			if (shortcut->Header.LinkFlags & LnkShellLinkHeader::LF_HasWorkingDir)
				WorkingDir.Read(shortcut);

			if (shortcut->Header.LinkFlags & LnkShellLinkHeader::LF_HasArguments)
				CommandLineArguments.Read(shortcut);

			if (shortcut->Header.LinkFlags & LnkShellLinkHeader::LF_HasIconLocation)
				IconLocation.Read(shortcut);
		}
	};

	struct LnkExtraData
	{
	};

	LnkShellLinkHeader		Header;
	LnkLinkTargetIDList		IDList;
	LnkLinkInfo				LinkInfo;
	LnkStringData			StringData;
	TList<LnkExtraData>		ExtraData;
	FILE					*FilePointer;

	LnkWindowsShortcut (String fileName) :
	  FileName(fileName)
	{
		FilePointer = fopen(FileName.CString(), "rb");
	}

	~LnkWindowsShortcut ()
	{
		if (FilePointer != null)
		{
			fclose(FilePointer);
			FilePointer = null;
		}
	}

	long Length ()
	{
		return ftell(FilePointer);
	}

	void SeekFromBeginning (int ofs)
	{
		fseek(FilePointer, ofs, SEEK_SET);
	}

	void SeekFromCurrent (int ofs)
	{
		fseek(FilePointer, ofs, SEEK_CUR);
	}

	void SeekFromEnd (int ofs)
	{
		fseek(FilePointer, ofs, SEEK_END);
	}

	void SeekFromPos (int pos, int ofs)
	{
		SeekFromBeginning(pos);
		SeekFromCurrent(ofs);
	}

	bool Valid ()
	{
		return FilePointer != null;
	}

	template<typename T>
	T Read ()
	{
		T v;
		fread(&v, 1, sizeof(T), FilePointer);
		return v;
	}

	void ReadBytes (byte *buffer, int count)
	{
		fread(buffer, 1, count, FilePointer);
	}

	void ReadShorts (short *buffer, int count)
	{
		fread(buffer, sizeof(short), count, FilePointer);
	}

	void ReadNullTerminatedString (char **buffer)
	{
		String str;
		char c;

		while ((c = Read<char>()) != 0)
			str += c;

		char *buf = new char[str.Count()+1];
		Q_strncpyz(buf, str.CString(), str.Count()+1);
		*buffer = buf;
	}

	void ReadStringLength (char **buffer, int len)
	{
		char *buf = new char[len+1];
		buf[len] = 0;

		ReadBytes((byte*)buf, len);

		*buffer = buf;
	}

	void ReadStringUnicodeLength (wchar_t **buffer, int len)
	{
		wchar_t *buf = new wchar_t[len+1];

		ReadShorts((short*)buf, len);

		buf[len] = 0;

		*buffer = buf;
	}

	void Read ()
	{
		Header.Read(this);

		if (Header.LinkFlags & Header.LF_HasLinkTargetIDList)
			IDList.Read(this);

		if (Header.LinkFlags & Header.LF_HasLinkInfo)
			LinkInfo.Read(this);

		StringData.Read(this);
	}
};

/*
=================
FS_LoadPAK

Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
=================
*/
mPakPack_t *FS_LoadPAK(const char *fileName, bool complain)
{
	dPackHeader_t	header;
	mPackFile_t		*outPackFile;
	mPakPack_t		*outPack;
	FILE			*handle;
	dPackFile_t		fileInfo;
	int				i, numFiles;
	uint32			hashValue;
	String			name = fileName;

	// Open
	handle = fopen(fileName, "rb");

	if (!handle)
	{
		LnkWindowsShortcut lnk (name.Concatenate(".lnk"));

		if (lnk.Valid())
		{
			lnk.Read();

			if (lnk.LinkInfo.LocalBasePath != null)
				handle = fopen(lnk.LinkInfo.LocalBasePath, "rb");

			name = lnk.LinkInfo.LocalBasePath;
		}

		if (!handle)
		{
			if (complain)
				Com_Printf(PRNT_ERROR, "FS_LoadPAK: couldn't open \"%s\"\n", name.CString());
			return NULL;
		}
	}

	// Read header
	fread (&header, 1, sizeof(header), handle);
	if (LittleLong(header.ident) != PAK_HEADER)
	{
		fclose(handle);
		Com_Error(ERR_FATAL, "FS_LoadPAK: \"%s\" is not a packfile", name.CString());
	}

	header.dirOfs = LittleLong(header.dirOfs);
	header.dirLen = LittleLong(header.dirLen);

	// Sanity checks
	numFiles = header.dirLen / sizeof(dPackFile_t);
	if (numFiles > PAK_MAX_FILES)
	{
		fclose(handle);
		Com_Error(ERR_FATAL, "FS_LoadPAK: \"%s\" has too many files (%i > %i)", name.CString(), numFiles, PAK_MAX_FILES);
	}
	if (numFiles <= 0)
	{
		fclose(handle);
		Com_Error(ERR_FATAL, "FS_LoadPAK: \"%s\" is empty", name.CString());
	}

	// Read past header
	fseek(handle, header.dirOfs, SEEK_SET);

	// Create pak
	outPack = QNew(com_fileSysPool, 0) mPakPack_t(handle);
	outPackFile = (mPackFile_t*)Mem_PoolAlloc(sizeof(mPackFile_t) * numFiles, com_fileSysPool, 0);

	Com_NormalizePath(outPack->name, sizeof(outPack->name), name.CString());
	outPack->numFiles = numFiles;
	outPack->files = outPackFile;

	// Parse the directory
	for (i=0 ; i<numFiles ; i++)
	{
		fread(&fileInfo, sizeof(fileInfo), 1, handle);
		Com_NormalizePath(outPackFile->fileName, sizeof(outPackFile->fileName), fileInfo.name);
		outPackFile->filePos = LittleLong(fileInfo.filePos);
		outPackFile->fileLen = LittleLong(fileInfo.fileLen);

		// Link it into the hash tree
		hashValue = Com_HashFileName(outPackFile->fileName, FS_MAX_HASHSIZE);

		outPackFile->hashNext = outPack->fileHashTree[hashValue];	
		outPack->fileHashTree[hashValue] = outPackFile;

		// Next
		outPackFile++;
	}

	Com_Printf(0, "FS_LoadPAK: loaded \"%s\"\n", name.CString());
	return outPack;
}


/*
=================
FS_LoadPKZ
=================
*/
mUnzPack_t *FS_LoadPKZ (const char *fileName, bool complain)
{
	mPackFile_t		*outPkzFile;
	mUnzPack_t		*outPkz;
	unzFile			*handle;
	unz_global_info	global;
	unz_file_info	info;
	char			name[MAX_QPATH];
	int				status;
	int				numEntries;
	int				numFiles;
	uint32			hashValue;
	char			c;

	// Open
	handle = (unzFile*)unzOpen(fileName);
	if (!handle)
	{
		if (complain)
			Com_Printf(PRNT_ERROR, "FS_LoadPKZ: couldn't open \"%s\"\n", fileName);
		return NULL;
	}

	// Get global info
	if (unzGetGlobalInfo(handle, &global) != UNZ_OK)
	{
		unzClose(handle);
		Com_Error(ERR_FATAL, "FS_LoadPKZ: \"%s\" is not a packfile", fileName);
	}

	// Number of entries (including folders)
	numEntries = global.number_entry;

	// Find the total file count
	status = unzGoToFirstFile(handle);
	for (numFiles=0 ; status==UNZ_OK ; )
	{
		unzGetCurrentFileInfo(handle, &info, name, MAX_QPATH, NULL, 0, NULL, 0);

		c = name[strlen(name)-1];
		if (c != '/' && c != '\\')
			numFiles++;

		status = unzGoToNextFile(handle);
	}

	if (numFiles > PKZ_MAX_FILES)
	{
		unzClose(handle);
		Com_Error(ERR_FATAL, "FS_LoadPKZ: \"%s\" has too many files (%i > %i)", fileName, numFiles, PKZ_MAX_FILES);
	}
	if (numFiles <= 0)
	{
		unzClose(handle);
		Com_Error(ERR_FATAL, "FS_LoadPKZ: \"%s\" is empty", fileName);
	}

	// Create pak
	outPkz = QNew(com_fileSysPool, 0) mUnzPack_t(handle);
	outPkzFile = (mPackFile_t*)Mem_PoolAlloc (sizeof(mPackFile_t) * numFiles, com_fileSysPool, 0);

	Com_NormalizePath(outPkz->name, sizeof(outPkz->name), fileName);
	outPkz->numFiles = numFiles;
	outPkz->files = outPkzFile;

	status = unzGoToFirstFile(handle);
	while (status == UNZ_OK)
	{
		unzGetCurrentFileInfo(handle, &info, name, MAX_QPATH, NULL, 0, NULL, 0);

		c = name[strlen(name)-1];
		if (c != '/' && c != '\\')
		{
			Com_NormalizePath(outPkzFile->fileName, sizeof(outPkzFile->fileName), name);
			outPkzFile->filePos = unzGetOffset (handle);
			outPkzFile->fileLen = info.uncompressed_size;

			// Link it into the hash tree
			hashValue = Com_HashFileName(outPkzFile->fileName, FS_MAX_HASHSIZE);

			outPkzFile->hashNext = outPkz->fileHashTree[hashValue];
			outPkz->fileHashTree[hashValue] = outPkzFile;

			// Next
			outPkzFile++;
		}

		status = unzGoToNextFile(handle);
	}

	Com_Printf(0, "FS_LoadPKZ: loaded \"%s\"\n", fileName);
	return outPkz;
}

/*
================
FS_AddGameDirectory

Sets fs_gameDir, adds the directory to the head of the path, and loads *.pak/*.pkz
================
*/
static void FS_AddGameDirectory (char *dir, char *gamePath)
{
	fsPath_t	*search;

	if (fs_developer->intVal)
		Com_Printf (0, "FS_AddGameDirectory: adding \"%s\"\n", dir);

	// Set as game directory
	Q_strncpyz(fs_gameDir, dir, sizeof(fs_gameDir));

	// Add the directory to the search path
	search = (fsPath_t*)Mem_PoolAlloc (sizeof(fsPath_t), com_fileSysPool, 0);
	Q_strncpyz(search->pathName, dir, sizeof(search->pathName));
	Q_strncpyz(search->gamePath, gamePath, sizeof(search->gamePath));
	search->next = fs_searchPaths;
	fs_searchPaths = search;

	var packFiles = Sys_FindFiles (dir, "*/*.pkp", 0, false, true, false);
	packFiles.AddRange(Sys_FindFiles (dir, "*/*.pkp.lnk", 0, false, true, false));
	packFiles.AddRange(Sys_FindFiles (dir, "*/*.pak", 0, false, true, false));
	packFiles.AddRange(Sys_FindFiles (dir, "*/*.pak.lnk", 0, false, true, false));

	for (uint32 i = 0; i < packFiles.Count(); i++)
	{
		if (packFiles[i].EndsWith(".lnk"))
			packFiles[i].Remove(packFiles[i].Count() - 4);

		mPackBase_t *pack;

		if (packFiles[i].EndsWith("pkp"))
			pack = FS_LoadPKZ (packFiles[i].CString(), true);
		else if (packFiles[i].EndsWith("pak"))
			pack = FS_LoadPAK (packFiles[i].CString(), true);

		if (!pack)
			continue;

		search = (fsPath_t*)Mem_PoolAlloc (sizeof(fsPath_t), com_fileSysPool, 0);
		Q_strncpyz (search->pathName, dir, sizeof(search->pathName));
		Q_strncpyz (search->gamePath, gamePath, sizeof(search->gamePath));
		search->package = pack;
		search->next = fs_searchPaths;
		fs_searchPaths = search;
	}
}

/*
=============================================================================

	GAME PATH

=============================================================================
*/

/*
============
FS_Gamedir
============
*/
const char *FS_Gamedir()
{
	if (*fs_gameDir)
		return fs_gameDir;
	else
		return BASE_MODDIRNAME;
}


/*
================
FS_SetGamedir

Sets the gamedir and path to a different directory.
================
*/
void FS_SetGamedir(char *dir, bool firstTime)
{
	fsPath_t	*next;
	mPackBase_t	*package;
	int			i;

	uint32 startCycles = Sys_Cycles();

	// Make sure it's not a path
	if (strstr (dir, "..") || strchr (dir, '/') || strchr (dir, '\\') || strchr (dir, ':'))
	{
		Com_Printf(PRNT_WARNING, "FS_SetGamedir: Gamedir should be a single directory name, not a path\n");
		return;
	}

	// Free old inverted paths
	if (fs_invSearchPaths)
	{
		Mem_Free(fs_invSearchPaths);
		fs_invSearchPaths = NULL;
	}

	// Free up any current game dir info
	for ( ; fs_searchPaths != fs_baseSearchPath ; fs_searchPaths=next)
	{
		next = fs_searchPaths->next;

		if (fs_searchPaths->package)
		{
			package = fs_searchPaths->package;
			package->Close();

			Mem_Free(package->files);
			Mem_Free(package);
		}

		Mem_Free(fs_searchPaths);
	}

	// Load packages
	Com_Printf (0, "\n------------- Changing Game ------------\n");

	Q_snprintfz(fs_gameDir, sizeof(fs_gameDir), "%s/%s", fs_basedir->string, dir);

	if (!strcmp(dir, BASE_MODDIRNAME) || *dir == 0)
	{
		Cvar_VariableSet(fs_gamedircvar, "", true);
		Cvar_VariableSet(fs_game, "", true);
	}
	else
	{
		Cvar_VariableSet(fs_gamedircvar, dir, true);
		if (fs_cddir->string[0])
			FS_AddGameDirectory(Q_VarArgs ("%s/%s", fs_cddir->string, dir), dir);

		FS_AddGameDirectory(Q_VarArgs ("%s/%s", fs_basedir->string, dir), dir);
	}

	// Store a copy of the search paths inverted for FS_FindFiles
	for (fs_numInvSearchPaths=0, next=fs_searchPaths ; next ; next=next->next, fs_numInvSearchPaths++);
	fs_invSearchPaths = (fsPath_t**)Mem_PoolAlloc(sizeof(fsPath_t) * fs_numInvSearchPaths, com_fileSysPool, 0);
	for (i=fs_numInvSearchPaths-1, next=fs_searchPaths ; i>=0 ; next=next->next, i--)
		fs_invSearchPaths[i] = next;

	if (!firstTime)
	{
		Com_Printf(0, "----------------------------------------\n");
		Com_Printf(0, "init time: %6.2fms\n", (Sys_Cycles()-startCycles) * Sys_MSPerCycle());
		Com_Printf(0, "----------------------------------------\n");

#ifndef DEDICATED_ONLY
		// Forced to reload to flush old data
		if (!dedicated->intVal)
		{
			Cbuf_AddText("exec default.cfg\n");
			Cbuf_AddText("exec config.cfg\n");
			Cbuf_AddText("exec pglcfg.cfg\n");
			FS_ExecAutoexec();
			Cbuf_Execute();
			Cbuf_AddText("vid_restart\nsnd_restart\n");
			Cbuf_Execute();
		}
#endif
	}
}


/*
=============
FS_ExecAutoexec
=============
*/
void FS_ExecAutoexec()
{
	char	*dir;
	char	name[MAX_QPATH];

	dir = Cvar_GetStringValue("gamedir");
	if (*dir)
		Q_snprintfz(name, sizeof(name), "%s/%s/autoexec.cfg", fs_basedir->string, dir);
	else
		Q_snprintfz(name, sizeof(name), "%s/%s/autoexec.cfg", fs_basedir->string, BASE_MODDIRNAME);

	if (Sys_FindFirst(name, 0, (SFF_SUBDIR|SFF_HIDDEN|SFF_SYSTEM)))
		Cbuf_AddText("exec autoexec.cfg\n");

	Sys_FindClose();
}

/*
=============================================================================

	FILE SEARCHING

=============================================================================
*/

/*
================
FS_FindFiles
================
*/
TList<String> FS_FindFiles(const char *path, const char *filter, const char *extension, const bool addGameDir, const bool recurse)
{
	TList<String> fileList;

	// Search through the path, one element at a time
	int fileCount = 0;
	for (int pathNum=0 ; pathNum<fs_numInvSearchPaths ; pathNum++)
	{
		fsPath_t *search = fs_invSearchPaths[pathNum];

		if (search->package)
		{
			// Pack file
			mPackBase_t *pack = search->package;
			for (int fileNum=0 ; fileNum<pack->numFiles ; fileNum++)
			{
				mPackFile_t *packFile = &pack->files[fileNum];

				// Match path
				if (!recurse)
				{
					char dir[MAX_OSPATH];
					Com_FilePath(packFile->fileName, dir, sizeof(dir));
					if (Q_stricmp(path, dir))
						continue;
				}
				// Check path
				else if (!strstr(packFile->fileName, path))
					continue;

				// Match extension
				if (extension)
				{
					char ext[MAX_QEXT];
					Com_FileExtension(packFile->fileName, ext, sizeof(ext));

					// Filter or compare
					if (strchr(extension, '*'))
					{
						if (!Q_WildcardMatch(extension, ext, 1))
							continue;
					}
					else
					{
						if (Q_stricmp(extension, ext))
							continue;
					}
				}

				// Match filter
				if (filter)
				{
					if (!Q_WildcardMatch(filter, packFile->fileName, 1))
						continue;
				}

				// Found something
				String name (packFile->fileName);

				// Ignore duplicates
				bool bFound = false;
				for (int i=0 ; i<fileCount ; i++)
				{
					if (!fileList[i].CompareCaseInsensitive(name))
					{
						bFound = true;
						break;
					}
				}

				if (!bFound)
				{
					if (addGameDir)
						fileList.Add(String::Format("%s/%s", search->gamePath, name));
					else
						fileList.Add(name);
				}
			}
		}
		else
		{
			// Directory tree
			char dir[MAX_OSPATH];
			Q_snprintfz(dir, sizeof(dir), "%s/%s", search->pathName, path);
		
			TList<String> dirFiles;
			if (extension)
			{
				char ext[MAX_QEXT];
				Q_snprintfz(ext, sizeof(ext), "*.%s", extension);
				dirFiles = Sys_FindFiles(dir, ext, 0, recurse, true, false);
			}
			else
				dirFiles = Sys_FindFiles(dir, "*", 0, recurse, true, true);

			for (uint32 fileNum=0 ; fileNum<dirFiles.Count() ; fileNum++)
			{
				// Match filter
				if (filter)
				{
					if (!Q_WildcardMatch(filter, dirFiles[fileNum].CString()+strlen(search->pathName)+1, 1))
						continue;
				}

				// Found something
				String name = dirFiles[fileNum].Substring(strlen(search->pathName) + 1);

				// Ignore duplicates
				bool bFound = false;
				for (int i=0 ; i<fileCount ; i++)
				{
					if (!fileList[i].CompareCaseInsensitive(name))
						break;
				}

				if (!bFound)
				{
					if (addGameDir)
						fileList.Add(String::Format("%s/%s", search->gamePath, name));
					else
						fileList.Add(name);
				}
			}
		}
	}

	return fileList;
}


/*
================
FS_NextPath

Allows enumerating all of the directories in the search path
================
*/
char *FS_NextPath (char *prevPath)
{
	fsPath_t	*s;
	char		*prev;

	if (!prevPath)
		return fs_gameDir;

	prev = fs_gameDir;
	for (s=fs_searchPaths ; s ; s=s->next) {
		if (s->package)
			continue;
		if (prevPath == prev)
			return s->pathName;
		prev = s->pathName;
	}

	return NULL;
}

/*
=============================================================================

	CONSOLE FUNCTIONS

=============================================================================
*/

/*
================
FS_Link_f

Creates a fsLink_t
================
*/
static void FS_Link_f ()
{
	fsLink_t	*l, **prev;

	if (Cmd_Argc () != 3) {
		Com_Printf (0, "USAGE: link <from> <to>\n");
		return;
	}

	// See if the link already exists
	prev = &fs_fileLinks;
	for (l=fs_fileLinks ; l ; l=l->next) {
		if (!strcmp (l->from, Cmd_Argv (1))) {
			Mem_Free (l->to);
			if (!strlen (Cmd_Argv (2))) {
				// Delete it
				*prev = l->next;
				Mem_Free (l->from);
				Mem_Free (l);
				return;
			}
			l->to = Mem_PoolStrDup (Cmd_Argv (2), com_fileSysPool, 0);
			return;
		}
		prev = &l->next;
	}

	// Create a new link
	l = (fsLink_t*)Mem_PoolAlloc (sizeof(*l), com_fileSysPool, 0);
	l->from = Mem_PoolStrDup (Cmd_Argv (1), com_fileSysPool, 0);
	l->fromLength = strlen (l->from);
	l->to = Mem_PoolStrDup (Cmd_Argv (2), com_fileSysPool, 0);
	l->next = fs_fileLinks;
	fs_fileLinks = l;
}


/*
============
FS_ListHandles_f
============
*/
static void FS_ListHandles_f ()
{
	fsHandleIndex_t	*index;
	int				i;

	Com_Printf (0, " #  mode name\n");
	Com_Printf (0, "--- ---- ----------------\n");
	for (i=0, index=&fs_fileIndices[0] ; i<FS_MAX_FILEINDICES ; index++, i++) {
		if (!index->inUse)
			continue;

		Com_Printf (0, "%3i ", i+1);
		switch (index->openMode) {
		case FS_MODE_READ_BINARY:	Com_Printf (0, "RB ");	break;
		case FS_MODE_WRITE_BINARY:	Com_Printf (0, "WB ");	break;
		case FS_MODE_APPEND_BINARY:	Com_Printf (0, "AB ");	break;
		case FS_MODE_WRITE_TEXT:	Com_Printf (0, "WT ");	break;
		case FS_MODE_APPEND_TEXT:	Com_Printf (0, "AT ");	break;
		case FS_MODE_READ_WRITE_BINARY: Com_Printf (0, "+B"); break;
		}
		Com_Printf (0, "%s\n", index->name);
	}
}


/*
============
FS_Path_f
============
*/
static void FS_Path_f ()
{
	fsPath_t	*s;
	fsLink_t	*l;

	Com_Printf (0, "Current search path:\n");
	for (s=fs_searchPaths ; s ; s=s->next) {
		if (s == fs_baseSearchPath)
			Com_Printf (0, "----------\n");

		if (s->package)
			Com_Printf (0, "%s (%i files)\n", s->package->name, s->package->numFiles);
		else
			Com_Printf (0, "%s\n", s->pathName);
	}

	Com_Printf (0, "\nLinks:\n");
	for (l=fs_fileLinks ; l ; l=l->next)
		Com_Printf (0, "%s : %s\n", l->from, l->to);
}

/*
=============================================================================

	INIT / SHUTDOWN

=============================================================================
*/

/*
================
FS_Init
================
*/
void FS_Init()
{
	fsPath_t	*next;
	int			i;

	uint32 startCycles = Sys_Cycles();
	Com_Printf (0, "\n------- Filesystem Initialization ------\n");

	// Register commands/cvars
	Cmd_AddCommand("link",			0, FS_Link_f,			"");
	Cmd_AddCommand("listHandles",	0, FS_ListHandles_f,	"Lists active files");
	Cmd_AddCommand("path",			0, FS_Path_f,			"");

	fs_basedir		= Cvar_Register("basedir",			".",	CVAR_READONLY);
	fs_cddir		= Cvar_Register("cddir",			"",		CVAR_READONLY);
	fs_game			= Cvar_Register("game",				"",		CVAR_LATCH_SERVER|CVAR_SERVERINFO|CVAR_RESET_GAMEDIR);
	fs_gamedircvar	= Cvar_Register("gamedir",			"",		CVAR_SERVERINFO|CVAR_READONLY);
	fs_defaultPaks	= Cvar_Register("fs_defaultPaks",	"1",	CVAR_ARCHIVE);

	// Load pak files
	if (fs_cddir->string[0])
		FS_AddGameDirectory(Q_VarArgs("%s/"BASE_MODDIRNAME, fs_cddir->string), BASE_MODDIRNAME);

	FS_AddGameDirectory(Q_VarArgs("%s/baseq2", fs_basedir->string), "baseq2");
	FS_AddGameDirectory(Q_VarArgs("%s/"BASE_MODDIRNAME, fs_basedir->string), BASE_MODDIRNAME);

	// Any set gamedirs will be freed up to here
	fs_baseSearchPath = fs_searchPaths;

	// Load the game directory
	if (fs_game->string[0])
	{
		FS_SetGamedir(fs_game->string, true);
	}
	else
	{
		// Store a copy of the search paths inverted for FS_FindFiles
		for (fs_numInvSearchPaths=0, next=fs_searchPaths ; next ; next=next->next, fs_numInvSearchPaths++);
		fs_invSearchPaths = (fsPath_t**)Mem_PoolAlloc (sizeof(fsPath_t) * fs_numInvSearchPaths, com_fileSysPool, 0);
		for (i=fs_numInvSearchPaths-1, next=fs_searchPaths ; i>=0 ; next=next->next, i--)
			fs_invSearchPaths[i] = next;
	}

	Com_Printf (0, "----------------------------------------\n");

	// Check memory integrity
	Mem_CheckPoolIntegrity (com_fileSysPool);

	Com_Printf (0, "init time: %6.2fms\n", (Sys_Cycles()-startCycles) * Sys_MSPerCycle());
	Com_Printf (0, "----------------------------------------\n");
}
