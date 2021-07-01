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
// cl_cin.c
// FIXME TODO:
// - Needs to resample the texture data if the image/cinematic size exceeds the video card maximum
//

#include "cl_local.h"

#define MAX_CIN_HBUFFER 0x20000
#define MAX_CIN_SNDBUFF 0x40000

struct huffBlock_t {
	byte				*data;
	int					count;
};

/*
=============================================================================

	RoQ DECOMPRESSION

=============================================================================
*/

static sint16	roq_sndSqrArr[256];

/*
=============
RoQ_Init

Called by the client to init, refresh also utilizes
=============
*/
void RoQ_Init ()
{
	int		i;

	for (i=0 ; i<128 ; i++) {
		roq_sndSqrArr[i] = i * i;
		roq_sndSqrArr[i+128] = i * i * -1;
	}
}


/*
=============
RoQ_ReadChunk
=============
*/
void RoQ_ReadChunk (cinematic_t *cin)
{
	roqChunk_t	*chunk = &cin->roqChunk;

	FS_Read (&chunk->id, sizeof(sint16), cin->fileNum);
	FS_Read (&chunk->size, sizeof(int), cin->fileNum);
	FS_Read (&chunk->arg, sizeof(sint16), cin->fileNum);

	chunk->id = LittleLong (chunk->id);
	chunk->size = LittleLong (chunk->size);
	chunk->arg = LittleLong (chunk->arg);
}


/*
=============
RoQ_SkipBlock
=============
*/
void RoQ_SkipBlock (cinematic_t *cin, int size)
{
	FS_Seek (cin->fileNum, size, FS_SEEK_CUR);
}


/*
=============
RoQ_SkipChunk
=============
*/
void RoQ_SkipChunk (cinematic_t *cin)
{
	FS_Seek (cin->fileNum, cin->roqChunk.size, FS_SEEK_CUR);
}


/*
=============
RoQ_ReadInfo
=============
*/
void RoQ_ReadInfo (cinematic_t *cin)
{
	sint16	t[4]; 
	int		i;

	FS_Read (t, sizeof(sint16)*4, cin->fileNum);
	for (i=0 ; i<4 ; i++)
		t[i] = LittleLong (t[i]);

	if (cin->width != t[0] || cin->height != t[1]) {
		cin->width = t[0];
		cin->height = t[1];

		if (cin->roqBuffer)
			Mem_Free (cin->roqBuffer);
		cin->roqBuffer = (byte*)Mem_PoolAlloc (cin->width*cin->height*4 * 2, cl_cinSysPool, 0);
		memset (cin->roqBuffer, 255, cin->width*cin->height*4 * 2);

		cin->frames[0] = cin->roqBuffer;
		cin->frames[1] = cin->roqBuffer + (cin->width*cin->height*4);
	}
}


/*
=============
RoQ_ReadCodeBook
=============
*/
void RoQ_ReadCodeBook (cinematic_t *cin)
{
	roqChunk_t	*chunk = &cin->roqChunk;
	uint32		nv[2];

	nv[0] = (chunk->arg >> 8) & 255;
	if (!nv[0])
		nv[0] = 256;

	nv[1] = chunk->arg & 255;
	if (!nv[1] && nv[0] * 6 < chunk->size)
		nv[1] = 256;

	FS_Read (cin->roqCells, sizeof(roqCell_t)*nv[0], cin->fileNum);
	FS_Read (cin->roqQCells, sizeof(roqQCell_t)*nv[1], cin->fileNum);
}


/*
=============
RoQ_DecodeBlock
=============
*/
static void RoQ_DecodeBlock (byte *dst0, byte *dst1, const byte *src0, const byte *src1, float u, float v)
{
	int		c[3];

	// Convert YCbCr to RGB
	Vec3Set (c, 1.402f * v, -0.34414f * u - 0.71414f * v, 1.772f * u);

	// First pixel
	dst0[0] = bound (0, c[0] + src0[0], 255);
	dst0[1] = bound (0, c[1] + src0[0], 255);
	dst0[2] = bound (0, c[2] + src0[0], 255);

	// Second pixel
	dst0[4] = bound (0, c[0] + src0[1], 255);
	dst0[5] = bound (0, c[1] + src0[1], 255);
	dst0[6] = bound (0, c[2] + src0[1], 255);

	// Third pixel
	dst1[0] = bound (0, c[0] + src1[0], 255);
	dst1[1] = bound (0, c[1] + src1[0], 255);
	dst1[2] = bound (0, c[2] + src1[0], 255);

	// Fourth pixel
	dst1[4] = bound (0, c[0] + src1[1], 255);
	dst1[5] = bound (0, c[1] + src1[1], 255);
	dst1[6] = bound (0, c[2] + src1[1], 255);
}


/*
=============
RoQ_ApplyVector2x2
=============
*/
static void RoQ_ApplyVector2x2 (cinematic_t *cin, int x, int y, const roqCell_t *cell)
{
	byte	*dst0, *dst1;

	dst0 = cin->frames[0] + (y * cin->width + x) * 4;
	dst1 = dst0 + cin->width * 4;

	RoQ_DecodeBlock (dst0, dst1, cell->y, cell->y+2, (float)((int)cell->u-128), (float)((int)cell->v-128));
}


/*
=============
RoQ_ApplyVector4x4
=============
*/
static void RoQ_ApplyVector4x4 (cinematic_t *cin, int x, int y, const roqCell_t *cell)
{
	byte	*dst0, *dst1;
	byte	p[4];
	float	u, v;

	u = (float)((int)cell->u - 128);
	v = (float)((int)cell->v - 128);

	p[0] = p[1] = cell->y[0];
	p[2] = p[3] = cell->y[1];
	dst0 = cin->frames[0] + (y * cin->width + x) * 4;
	dst1 = dst0 + cin->width * 4;
	RoQ_DecodeBlock (dst0, dst0+8, p, p+2, u, v);
	RoQ_DecodeBlock (dst1, dst1+8, p, p+2, u, v);

	p[0] = p[1] = cell->y[2];
	p[2] = p[3] = cell->y[3];
	dst0 += cin->width * 4 * 2;
	dst1 +=	cin->width * 4 * 2;
	RoQ_DecodeBlock (dst0, dst0+8, p, p+2, u, v);
	RoQ_DecodeBlock (dst1, dst1+8, p, p+2, u, v);
}


/*
=============
RoQ_ApplyMotion4x4
=============
*/
static void RoQ_ApplyMotion4x4 (cinematic_t *cin, int x, int y, byte mv, char meanX, char meanY)
{
	byte	*src, *dst;
	int		x0, y0;

	// Find the source coords
	x0 = x + 8 - (mv >> 4) - meanX;
	y0 = y + 8 - (mv & 0xf) - meanY;

	src = cin->frames[1] + (y0 * cin->width + x0) * 4;
	dst = cin->frames[0] + (y * cin->width + x) * 4;

	for (y=0 ; y<4 ; y++, src+=cin->width*4, dst+=cin->width*4)
		memcpy (dst, src, 4*4);	// FIXME
}


/*
=============
RoQ_ApplyMotion8x8
=============
*/
static void RoQ_ApplyMotion8x8 (cinematic_t *cin, int x, int y, byte mv, char meanX, char meanY)
{
	byte	*src, *dst;
	int		x0, y0;

	// Find the source coords
	x0 = x + 8 - (mv >> 4) - meanX;
	y0 = y + 8 - (mv & 0xf) - meanY;

	src = cin->frames[1] + (y0 * cin->width + x0) * 4;
	dst = cin->frames[0] + (y * cin->width + x) * 4;

	for (y=0 ; y<8 ; y++, src+=cin->width*4, dst+=cin->width*4)
		memcpy (dst, src, 8*4);	// FIXME
}

/*
=============================================================================

	STATIC PCX LOADING

=============================================================================
*/

/*
=============
CIN_LoadPCX
=============
*/
static bool CIN_LoadPCX (char *name, byte **pic, byte **palette, int *width, int *height)
{
	byte		*raw;
	pcxHeader_t	*pcx;
	int			x, y, fileLen;
	int			dataByte, runLength;
	byte		*out, *pix;

	if (pic)
		*pic = NULL;
	if (palette)
		*palette = NULL;

	// Load the file
	fileLen = FS_LoadFile (name, (void **)&raw, false);
	if (!raw || fileLen <= 0)
		return false;

	// Parse the PCX file
	pcx = (pcxHeader_t *)raw;

	pcx->xMin = LittleShort (pcx->xMin);
	pcx->yMin = LittleShort (pcx->yMin);
	pcx->xMax = LittleShort (pcx->xMax);
	pcx->yMax = LittleShort (pcx->yMax);
	pcx->hRes = LittleShort (pcx->hRes);
	pcx->vRes = LittleShort (pcx->vRes);
	pcx->bytesPerLine = LittleShort (pcx->bytesPerLine);
	pcx->paletteType = LittleShort (pcx->paletteType);

	raw = &pcx->data;

	// Sanity checks
	if (pcx->manufacturer != 0x0a || pcx->version != 5 || pcx->encoding != 1) {
		Com_DevPrintf (PRNT_WARNING, "CIN_LoadPCX: %s: Invalid PCX header\n", name);
		return false;
	}

	if (pcx->bitsPerPixel != 8 || pcx->colorPlanes != 1) {
		Com_DevPrintf (PRNT_WARNING, "CIN_LoadPCX: %s: Only 8-bit PCX images are supported\n", name);
		return false;
	}

	if (pcx->xMax >= 640 || pcx->xMax <= 0 || pcx->yMax >= 480 || pcx->yMax <= 0) {
		Com_DevPrintf (PRNT_WARNING, "CIN_LoadPCX: %s: Bad PCX file dimensions: %i x %i\n", name, pcx->xMax, pcx->yMax);
		return false;
	}

	// FIXME: Some images with weird dimensions will crash if I don't do this...
	x = max (pcx->yMax+1, pcx->xMax+1);
	pix = out = (byte*)Mem_PoolAlloc (x * x, cl_cinSysPool, 0);
	if (pic)
		*pic = out;

	if (palette) {
		*palette = (byte*)Mem_PoolAlloc (768, cl_cinSysPool, 0);
		memcpy (*palette, (byte *)pcx + fileLen - 768, 768);
	}

	if (width)
		*width = pcx->xMax+1;
	if (height)
		*height = pcx->yMax+1;

	for (y=0 ; y<=pcx->yMax ; y++, pix+=pcx->xMax+1) {
		for (x=0 ; x<=pcx->xMax ; ) {
			dataByte = *raw++;

			if ((dataByte & 0xC0) == 0xC0) {
				runLength = dataByte & 0x3F;
				dataByte = *raw++;
			}
			else
				runLength = 1;

			while (runLength-- > 0)
				pix[x++] = dataByte;
		}
	}

	if (raw - (byte *)pcx > fileLen) {
		Com_DevPrintf (PRNT_WARNING, "CIN_LoadPCX: PCX file %s was malformed", name);
		Mem_Free (out);
		out = NULL;
		pix = NULL;

		if (pic)
			*pic = NULL;
		if (palette) {
			Mem_Free (palette);
			*palette = NULL;
		}

		return false;
	}

	if (!pic)
		Mem_Free (out);
	FS_FreeFile (pcx);

	return true;
}

/*
=============================================================================

	CINEMATIC DECOMPRESSION

=============================================================================
*/

/*
==================
CIN_SmallestHuffNode
==================
*/
static int CIN_SmallestHuffNode (int numNodes)
{
	int		best, bestNode;
	int		i;

	best = 99999999;
	bestNode = -1;
	for (i=0 ; i<numNodes ; i++) {
		if (cl.cin.hUsed[i] || !cl.cin.hCount[i])
			continue;

		if (cl.cin.hCount[i] < best) {
			best = cl.cin.hCount[i];
			bestNode = i;
		}
	}

	if (bestNode == -1)
		return -1;

	cl.cin.hUsed[bestNode] = true;
	return bestNode;
}


/*
==================
CIN_TableInit

Reads the 64k counts table and initializes the node trees
==================
*/
static void CIN_TableInit ()
{
	int		*node, *nodeBase;
	byte	counts[256];
	int		numNodes;
	int		prev, j;

	cl.cin.hBuffer = (byte*)Mem_PoolAlloc (MAX_CIN_HBUFFER, cl_cinSysPool, 0);
	cl.cin.hNodes = (int*)Mem_PoolAlloc (256*256*2*4, cl_cinSysPool, 0);

	for (prev=0 ; prev<256 ; prev++) {
		memset (cl.cin.hCount, 0, sizeof(cl.cin.hCount));
		memset (cl.cin.hUsed, 0, sizeof(cl.cin.hUsed));

		// Read a row of counts
		FS_Read (counts, sizeof(counts), cl.cin.fileNum);
		for (j=0 ; j<256 ; j++)
			cl.cin.hCount[j] = counts[j];

		// Build the nodes
		numNodes = 256;
		nodeBase = cl.cin.hNodes + prev*256*2;

		while (numNodes != 511) {
			node = nodeBase + (numNodes-256)*2;

			// Pick two lowest counts
			node[0] = CIN_SmallestHuffNode (numNodes);
			if (node[0] == -1)
				break;	// no more

			node[1] = CIN_SmallestHuffNode (numNodes);
			if (node[1] == -1)
				break;

			cl.cin.hCount[numNodes] = cl.cin.hCount[node[0]] + cl.cin.hCount[node[1]];
			numNodes++;
		}

		cl.cin.hNumNodes[prev] = numNodes-1;
	}
}


/*
==================
CIN_DecompressFrame
==================
*/
static huffBlock_t CIN_DecompressFrame (huffBlock_t in)
{
	byte		*input, *out_p;
	huffBlock_t	out;
	int			nodenum, count;
	int			inbyte;
	int			*hnodes, *hnodesbase;
	int			i;

	// Get decompressed count
	count = in.data[0] + (in.data[1]<<8) + (in.data[2]<<16) + (in.data[3]<<24);
	input = in.data + 4;
	out_p = out.data = (byte*)Mem_PoolAlloc (count, cl_cinSysPool, 0);

	// Read bits
	hnodesbase = cl.cin.hNodes - 256*2;	// nodes 0-255 aren't stored

	hnodes = hnodesbase;
	nodenum = cl.cin.hNumNodes[0];
	while (count) {
		inbyte = *input++;

		for (i=0 ; i<8 ; i++) {
			if (nodenum < 256) {
				hnodes = hnodesbase + (nodenum<<9);
				*out_p++ = nodenum;
				if (!--count)
					break;
				nodenum = cl.cin.hNumNodes[nodenum];
			}
			nodenum = hnodes[nodenum*2 + (inbyte&1)];
			inbyte >>=1;
		}
	}

	if (input-in.data != in.count && input-in.data != in.count+1)
		Com_Printf (PRNT_WARNING, "Decompression overread by %i", (input - in.data) - in.count);

	out.count = out_p - out.data;

	return out;
}


/*
==================
CIN_ResamplePalette
==================
*/
static void CIN_ResamplePalette (byte *in, uint32 *out)
{
	int		i;
	byte	*bOut;

	bOut = (byte *)out;
	for (i=0 ; i<256 ; i++) {
		bOut[i*4+0] = in[i*3+0];
		bOut[i*4+1] = in[i*3+1];
		bOut[i*4+2] = in[i*3+2];
		bOut[i*4+3] = 255;
	}
}


/*
==================
CIN_ReadNextFrame
==================
*/
static byte *CIN_ReadNextFrame ()
{
	int			command;
	int			size;
	huffBlock_t	in, out;
	int			start, end, samples;
	byte		palette[768];

	// Read the next frame
	FS_Read (&command, sizeof(command), cl.cin.fileNum);
	command = LittleLong (command);
	switch (command) {
	case 2:
		// Last frame marker
		return NULL;

	case 1:
		// Read palette
		FS_Read (palette, sizeof(palette), cl.cin.fileNum);
		CIN_ResamplePalette (palette, cl.cin.hPalette);
		break;
	}

	// Decompress the next frame
	FS_Read (&size, 4, cl.cin.fileNum);
	size = LittleLong (size);
	if (size > MAX_CIN_HBUFFER || size < 1)
		Com_Error (ERR_DROP, "Bad compressed frame size");
	FS_Read (cl.cin.hBuffer, size, cl.cin.fileNum);

	// Read sound
	start = cl.cin.frameNum * cl.cin.sndRate / 14;
	end = (cl.cin.frameNum+1) * cl.cin.sndRate / 14;
	samples = end - start;

	// FIXME: HACK: disgusting, but necessary for sync with OpenAL
	if (!cl.cin.frameNum && cl.cin.sndAL) {
		samples += 4096;
		if (cl.cin.sndWidth == 2)
			memset (cl.cin.sndBuffer, 0x00, 4096*cl.cin.sndWidth*cl.cin.sndChannels);
		else
			memset (cl.cin.sndBuffer, 0x80, 4096*cl.cin.sndWidth*cl.cin.sndChannels);
		FS_Read (cl.cin.sndBuffer+(4096*cl.cin.sndWidth*cl.cin.sndChannels), (samples-4096)*cl.cin.sndWidth*cl.cin.sndChannels, cl.cin.fileNum);
	}
	else
		FS_Read (cl.cin.sndBuffer, samples*cl.cin.sndWidth*cl.cin.sndChannels, cl.cin.fileNum);

	Snd_RawSamples (cl.cin.sndRawChannel, samples, cl.cin.sndRate, cl.cin.sndWidth, cl.cin.sndChannels, cl.cin.sndBuffer);

	in.data = cl.cin.hBuffer;
	in.count = size;

	out = CIN_DecompressFrame (in);

	cl.cin.frameNum++;
	return out.data;
}

/*
=============================================================================

	PUBLIC FUNCTIONALITY

=============================================================================
*/

/*
==================
CIN_RunCinematic
==================
*/
void CIN_RunCinematic ()
{
	int		frame;

	if (cl.cin.time <= 0) {
		CIN_StopCinematic ();
		return;
	}

	if (cl.cin.frameNum == -1)
		return;		// Static image

	frame = (cls.realTime - cl.cin.time)*14.0f/1000;
	if (frame <= cl.cin.frameNum)
		return;

	if (frame > cl.cin.frameNum+1) {
		Com_Printf (PRNT_WARNING, "Dropped frame: %i > %i\n", frame, cl.cin.frameNum+1);
		cl.cin.time = cls.realTime - cl.cin.frameNum*1000/14;
	}

	if (cl.cin.frames[0])
		Mem_Free (cl.cin.frames[0]);

	cl.cin.frames[0] = cl.cin.frames[1];
	cl.cin.frames[1] = CIN_ReadNextFrame ();

	if (!cl.cin.frames[1]) {
		CIN_StopCinematic ();
		CIN_FinishCinematic ();

		SCR_BeginLoadingPlaque ();
		return;
	}
}


/*
==================
CIN_DrawCinematic
==================
*/
void CIN_DrawCinematic ()
{
	uint32		*dest;
	int			i, j, outRows, row;
	int			frac, fracStep;
	float		hScale;
	byte		*source;

	// Fill the background with black
	R_DrawFill(0, 0, cls.refConfig.vidWidth, cls.refConfig.vidHeight, Q_BColorBlack);

	// No cinematic image to render!
	if (!cl.cin.frames[0] || !cl.cin.vidBuffer)
		return;

	// Resample
	// FIXME: only do this if 1) needed and 2) this frame isn't the same as the last one (high framerate situation)
	memset (cl.cin.vidBuffer, 0, sizeof(cl.cin.vidBuffer));
	if (cl.cin.height <= 256) {
		hScale = 1;
		outRows = cl.cin.height;
	}
	else {
		hScale = cl.cin.height/256.0f;
		outRows = 256;
	}

	for (i=0 ; i<outRows ; i++) {
		row = (int)(i*hScale);
		if (row > cl.cin.height)
			break;

		source = cl.cin.frames[0] + cl.cin.width*row;
		dest = &cl.cin.vidBuffer[i*256];
		fracStep = cl.cin.width*0x10000/256;
		frac = fracStep >> 1;

		for (j=0 ; j<256 ; j++) {
			dest[j] = cl.cin.hPalette[source[frac>>16]];
			frac += fracStep;
		}
	}

	// Update the texture
	if (R_UpdateTexture ("***r_cinTexture***", (byte *)cl.cin.vidBuffer, 0, 0, 256, 256))
		R_DrawPic (clMedia.cinMaterial, 0, QuadVertices().SetVertices(0, 0, cls.refConfig.vidWidth, cls.refConfig.vidHeight).SetCoords(0, 0, 1, cl.cin.height*hScale/256), Q_BColorWhite);
}


/*
==================
CIN_PlayCinematic
==================
*/
void CIN_PlayCinematic (char *name)
{
	byte	*palette;
	char	bareName[MAX_OSPATH];
	char	loadName[MAX_OSPATH];
	int		old_khz;

	// Check the name
	if (!name || !name[0])
		Com_Error (ERR_DROP, "CIN_PlayCinematic: NULL name!\n");
	if (strlen(name) >= MAX_OSPATH)
		Com_Error (ERR_DROP, "CIN_PlayCinematic: name length exceeds MAX_OSPATH!\n");

	// Normalize, strip
	Com_NormalizePath (bareName, sizeof(bareName), name);
	Com_StripExtension (bareName, sizeof(bareName), bareName);

	cl.cin.frameNum = 0;

	// Static pcx image
	Q_snprintfz (loadName, sizeof(loadName), "pics/%s.pcx", bareName);
	if (FS_FileExists (loadName) != -1) {
		cl.cin.frameNum = -1;
		cl.cin.time = 1;

		if (!CIN_LoadPCX (loadName, &cl.cin.frames[0], &palette, &cl.cin.width, &cl.cin.height) || !cl.cin.frames[0]) {
			cl.cin.time = 0;
			return;
		}

		cl.cin.vidBuffer = (uint32*)Mem_PoolAlloc (256*256*sizeof(uint32), cl_cinSysPool, 0);

		SCR_EndLoadingPlaque ();
		CL_SetState (CA_ACTIVE);

		CIN_ResamplePalette (palette, cl.cin.hPalette);
		Mem_Free (palette);
		return;
	}

	// Load the video
	Q_snprintfz (loadName, sizeof(loadName), "video/%s.cin", bareName);
	FS_OpenFile (loadName, &cl.cin.fileNum, FS_MODE_READ_BINARY);
	if (!cl.cin.fileNum) {
		Com_Printf (PRNT_WARNING, "CIN_PlayCinematic: %s: not found!\n", loadName);
		CIN_FinishCinematic ();
		cl.cin.time = 0;
		return;
	}

	// Set the prep state
	SCR_EndLoadingPlaque ();
	CL_SetState (CA_ACTIVE);

	// Read and byte swap values
	FS_Read (&cl.cin.width, 4, cl.cin.fileNum);
	FS_Read (&cl.cin.height, 4, cl.cin.fileNum);
	cl.cin.width = LittleLong (cl.cin.width);
	cl.cin.height = LittleLong (cl.cin.height);

	FS_Read (&cl.cin.sndRate, 4, cl.cin.fileNum);
	FS_Read (&cl.cin.sndWidth, 4, cl.cin.fileNum);
	FS_Read (&cl.cin.sndChannels, 4, cl.cin.fileNum);
	cl.cin.sndRate = LittleLong (cl.cin.sndRate);
	cl.cin.sndWidth = LittleLong (cl.cin.sndWidth);
	cl.cin.sndChannels = LittleLong (cl.cin.sndChannels);

	// Setup the streaming channel
	cl.cin.sndBuffer = (byte*)Mem_PoolAlloc (MAX_CIN_SNDBUFF, cl_cinSysPool, 0);
	cl.cin.sndRawChannel = Snd_RawStart ();
	cl.cin.sndAL = (cl.cin.sndRawChannel) ? true : false;
	cl.cin.vidBuffer = (uint32*)Mem_PoolAlloc (256*256*sizeof(uint32), cl_cinSysPool, 0);

	// Setup the huff table
	CIN_TableInit ();

	// Switch up to 22 khz sound if necessary
	if (!cl.cin.sndAL) {
		old_khz = Cvar_GetIntegerValue ("s_khz");
		if (old_khz != cl.cin.sndRate/1000) {
			cl.cin.sndRestart = true;
			Cvar_VariableSetValue (s_khz, cl.cin.sndRate/1000, true);
			Cbuf_AddText ("snd_restart\n");
			Cvar_VariableSetValue (s_khz, old_khz, true);
		}
	}

	// Start
	cl.cin.frameNum = 0;
	cl.cin.frames[0] = CIN_ReadNextFrame ();
	cl.cin.time = Sys_Milliseconds ();
}


/*
==================
CIN_StopCinematic
==================
*/
void CIN_StopCinematic ()
{
	// Stop streaming
	Snd_RawStop (cl.cin.sndRawChannel);

	// Release memory
	Mem_FreePool (cl_cinSysPool);

	// Close the file
	if (cl.cin.fileNum)
		FS_CloseFile (cl.cin.fileNum);

	// Switch sound back if necessary
	if (cl.cin.sndRestart)
		Cbuf_AddText ("snd_restart\n");

	memset (&cl.cin, 0, sizeof(cinematic_t));
}


/*
====================
CIN_FinishCinematic

Called when either the cinematic completes, or it is aborted
====================
*/
void CIN_FinishCinematic ()
{
	// Tell the server to advance to the next map / cinematic
	cls.netChan.message.WriteByte (CLC_STRINGCMD);
	cls.netChan.message.WriteStringCat (Q_VarArgs ("nextserver %i\n", cl.serverCount));

	CL_SetState (CA_CONNECTED);
}
