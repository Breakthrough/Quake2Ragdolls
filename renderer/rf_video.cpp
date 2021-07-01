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
// rf_video.c
// Cinematic (Quake II CIN and Quake III RoQ) playback and handling
//

#include "rf_local.h"

/*
==================
R_StopCinematic
==================
*/
void R_StopCinematic (cinematic_t *cin)
{
}


/*
==================
R_PlayCinematic
==================
*/
void R_PlayCinematic (cinematic_t *cin)
{
//	int			fileLen;
	roqChunk_t	*chunk = &cin->roqChunk;

	cin->width = cin->height = 0;
//	fileLen = FS_OpenFile (cin->name, cin->fileNum, FS_MODE_READ_BINARY);
	if (!cin->fileNum) {
		cin->time = 0;
		return;
	}

	// Read the header
//	RoQ_ReadChunk (cin);
	if (chunk->id != RoQ_HEADER1 || chunk->id != RoQ_HEADER2 || chunk->id != RoQ_HEADER3) {
		R_StopCinematic (cin);
		cin->time = 0;
		return;
	}
}
