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
// rf_font.c
// FIXME TODO:
// - Hash optimize (though the table size should be pretty small, like 4).
// - Different materials for different size ranges (like smallMaterial fonts/quake3_sm.tga, mediumMaterial, largeMaterial, etc).
// - Per-character coords and width/height?
// - More font styles, italic, bold, etc...
// - matTime passed to font renders?
//	FS_ALIGN_CENTER			= BIT(0),
//	FS_ALIGN_RIGHT			= BIT(1),
//	FS_ITALIC				= BIT(2),
//

#include "rf_local.h"

#define MAX_FONTS		64
#define MAX_FONT_SIZES	8

struct fontSize_t
{
	float			minScale;
	char			matName[MAX_QPATH];
	refMaterial_t	*matPtr;
};

struct refFont_t
{
	char			name[MAX_QPATH-11];	// fonts/[name].font
	uint32			touchFrame;

	byte			numSizes;
	fontSize_t		fontSizes[MAX_FONT_SIZES];

	float			charWidth;
	float			charHeight;
};

static refFont_t	r_curFont;

static refFont_t	r_fontList[MAX_FONTS];
static uint32		r_numFonts;

static colorb		r_fontShadowClr;	// Only the alpha value ever changes

/*
=============================================================================

	FONT REGISTRATION

=============================================================================
*/

/*
===============
R_FindFont
===============
*/
static inline refFont_t *R_FindFont (char *name)
{
	refFont_t	*font;
	uint32	i;

	assert (name && name[0]);
	ri.reg.fontsSeaked++;

	for (i=0, font=&r_fontList[0] ; i<r_numFonts ; font++, i++)
	{
		if (!font->touchFrame)
			continue;	// Free slot

		// Check name
		if (!strcmp (font->name, name))
			return font;
	}

	return NULL;
}


/*
===============
R_TouchFont
===============
*/
static inline void R_TouchFont (refFont_t *font)
{
	fontSize_t	*fs;
	byte		i;

	assert (font);

	// Register the material(s)
	for (i=0, fs=font->fontSizes ; i<font->numSizes ; fs++, i++)
	{
		fs->matPtr = R_RegisterPic (fs->matName);
		if (!fs->matPtr)
			Com_Printf (PRNT_ERROR, "R_TouchFont: could not locate material '%s'!\n", fs->matName);
	}

	// Used this sequence
	font->touchFrame = ri.reg.registerFrame;
	ri.reg.fontsTouched++;
}


/*
===============
R_FreeFont
===============
*/
static inline void R_FreeFont (refFont_t *font)
{
	assert (font);
	if (!font)
		return;

	// Free it
	memset (font, 0, sizeof(refFont_t));
	ri.reg.fontsReleased++;
}


/*
===============
R_RegisterFont
===============
*/
refFont_t *R_RegisterFont (char *name)
{
	char		fixedName[MAX_QPATH];
	char		*buf;
	int			fileLen;
	char		*token;
	float		floatTok;
	parse_t		*ps;
	refFont_t	*font;
	uint32		i;

	// Check the name
	if (!name || !name[0])
	{
		Com_Printf (PRNT_ERROR, "R_RegisterFont: NULL name!\n");
		return NULL;
	}
	if (strchr (name, '/') || strchr (name, '\\') || strchr (name, '.'))
	{
		Com_Printf (PRNT_ERROR, "R_RegisterFont: name can not be a path ('%s')!\n", name);
		return NULL;
	}
	Q_strncpyz (fixedName, name, sizeof(fixedName));
	Q_strlwr (fixedName);

	// See if it's already loaded
	font = R_FindFont (name);
	if (font)
	{
		R_TouchFont (font);
		return font;
	}

	// Nope, use the scratch font spot for writing
	font = &r_curFont;
	memset (&r_curFont, 0, sizeof(refFont_t));
	Q_strncpyz (font->name, name, sizeof(font->name));
	Q_strlwr (font->name);
	font->charWidth = 8.0f;
	font->charHeight = 8.0f;

	// Load the file
	Q_snprintfz (fixedName, sizeof(fixedName), "fonts/%s.font", name);
	fileLen = FS_LoadFile (fixedName, (void **)&buf, true);
	if (!buf || fileLen <= 0)
	{
		Com_Printf (PRNT_ERROR, "ERROR: can't load '%s' -- %s\n", name, (fileLen == -1) ? "not found" : "empty file");
		return NULL;
	}

	// Parse the file
	ps = PS_StartSession (buf, PSP_COMMENT_BLOCK|PSP_COMMENT_LINE|PSP_COMMENT_POUND);
	for ( ; PS_ParseToken (ps, PSF_ALLOW_NEWLINES|PSF_TO_LOWER, &token) ; )
	{
		// Parse the materials
		if (!strcmp (token, "charwidth"))
		{
			// Width
			if (!PS_ParseDataType (ps, 0, PSDT_FLOAT, &floatTok, 1))
			{
				Com_Printf (PRNT_ERROR, "ERROR: invalid/missing parameters for '%s'\n", token);
				break;
			}
			if (floatTok < 0)
			{
				Com_Printf (PRNT_ERROR, "ERROR: invalid/missing parameters for '%s'\n", token);
				break;
			}

			font->charWidth = floatTok;
			continue;
		}
		else if (!strcmp (token, "charheight"))
		{
			// Height
			if (!PS_ParseDataType (ps, 0, PSDT_FLOAT, &floatTok, 1))
			{
				Com_Printf (PRNT_ERROR, "ERROR: invalid/missing parameters for '%s'\n", token);
				break;
			}
			if (floatTok < 0)
			{
				Com_Printf (PRNT_ERROR, "ERROR: invalid/missing parameters for '%s'\n", token);
				break;
			}

			font->charHeight = floatTok;
			continue;
		}
		else if (!strcmp (token, "mat"))
		{
			// Check for too many
			if (font->numSizes+1 >= MAX_FONT_SIZES)
			{
				Com_Printf (PRNT_ERROR, "ERROR: too many size definitions!\n");
				break;
			}

			// Scale
			if (!PS_ParseDataType (ps, 0, PSDT_FLOAT, &floatTok, 1))
			{
				Com_Printf (PRNT_ERROR, "ERROR: invalid/missing parameters for '%s'\n", token);
				break;
			}
			if (floatTok < 0)
			{
				Com_Printf (PRNT_ERROR, "ERROR: invalid/missing parameters for '%s'\n", token);
				break;
			}

			// Name
			if (!PS_ParseToken (ps, PSF_TO_LOWER, &token))
			{
				Com_Printf (PRNT_ERROR, "ERROR: invalid/missing parameters for '%s'\n", "mat");
				break;
			}

			Q_strncpyz (font->fontSizes[font->numSizes].matName, token, sizeof(font->fontSizes[font->numSizes].matName));
			font->fontSizes[font->numSizes].minScale = floatTok;
			font->numSizes++;
			continue;
		}

		Com_Printf (PRNT_ERROR, "ERROR: unknown key '%s'\n", token);
	}

	// Done parsing
	PS_EndSession (ps);
	FS_FreeFile (buf);

	// Make sure there's at least one sizedef
	if (font->numSizes == 0)
	{
		Com_Printf (PRNT_ERROR, "ERROR: no valid fonts for '%s'!\n", name);
		return NULL;
	}

	// Find a free slot
	for (i=0, font=&r_fontList[0] ; i<r_numFonts ; font++, i++)
	{
		if (font->touchFrame)
			continue;	// In use
		break;
	}

	// None found, make a new slot
	if (i == r_numFonts)
	{
		if (r_numFonts+1 >= MAX_FONTS)
			Com_Error (ERR_DROP, "R_RegisterFont: MAX_FONTS");

		font = &r_fontList[r_numFonts++];
	}

	// Copy from scratch space, touch, finish
	memcpy (font, &r_curFont, sizeof(refFont_t));
	R_TouchFont (font);
	return font;
}


/*
===============
R_EndFontRegistration
===============
*/
void R_EndFontRegistration()
{
	// Free un-touched fonts
	for (uint32 i=0 ; i<r_numFonts ; i++)
	{
		refFont_t *font = &r_fontList[i];
		if (!font->touchFrame)
			continue;	// Free spot
		if (font->touchFrame == ri.reg.registerFrame)
			continue;	// Used in this sequence

		R_FreeFont(font);
	}

	Com_DevPrintf(PRNT_CONSOLE, "Completing font system registration:\n-Released: %i\n-Touched: %i\n-Seaked: %i\n", ri.reg.fontsReleased, ri.reg.fontsTouched, ri.reg.fontsSeaked);
}


/*
===============
R_CheckFont
===============
*/
void R_CheckFont ()
{
	// Load console characters
	r_defaultFont->modified = false;

	// Load the font
	ri.media.defaultFont = R_RegisterFont (r_defaultFont->string);
	if (!ri.media.defaultFont && strcmp (r_defaultFont->string, "default"))
		ri.media.defaultFont = R_RegisterFont ("default");

	if (!ri.media.defaultFont)
		Com_Error (ERR_FATAL, "R_CheckFont: unable to load default font!");
}

/*
=============================================================================

	FONT RENDERING

=============================================================================
*/

/*
================
R_MaterialForFont
================
*/
// FIXME: let format decide based on x/y scale
static refMaterial_t *R_MaterialForFont (refFont_t *font, float xScale, float yScale)
{
	fontSize_t	*fs;
	byte		i;
	refMaterial_t	*bestMat;
	float		bestScale;

	assert (font);

	// Find the most appropriate scale
	bestMat = NULL;
	bestScale = -1;
	for (i=0, fs=font->fontSizes ; i<font->numSizes ; fs++, i++)
	{
		if (bestMat == NULL || (xScale >= fs->minScale && xScale > bestScale))
		{
			bestScale = fs->minScale;
			bestMat = fs->matPtr;
		}
	}

	return bestMat;
}


/*
================
R_GetFontDimensions
================
*/
// FIXME: (refFont_t *font, vec2_t scale, uint32 flags, vec2_t target)
// FIXME: adjust for shadowing flag?
void R_GetFontDimensions (refFont_t *font, float xScale, float yScale, uint32 flags, vec2_t dest)
{
	if (!font)
		font = ri.media.defaultFont;
	if (!xScale && !yScale)
	{
		xScale = r_fontScale->floatVal;
		yScale = r_fontScale->floatVal;
	}

	if (dest)
	{
		if (flags & FS_SQUARE)
		{
			if (font->charWidth >= font->charHeight)
			{
				dest[0] = font->charWidth * xScale;
				dest[1] = font->charWidth * yScale;
			}
			else
			{
				dest[0] = font->charHeight * xScale;
				dest[1] = font->charHeight * yScale;
			}
		}
		else
		{
			dest[0] = font->charWidth * xScale;
			dest[1] = font->charHeight * yScale;
		}
	}
}


/*
================
R_DrawString
================
*/
int R_DrawString (refFont_t *font, float x, float y, float xScale, float yScale, uint32 flags, char *string, const colorb &color)
{
	int				num, i;
	float			frow, fcol;
	float			startX;
	colorb			strColor;
	bool			isShadowed, isItalic;
	bool			skipNext = false;
	bool			inColorCode = false;
	refMaterial_t	*mat;
	vec2_t			ftSize;

	if (!string)
		return 0;

	// Find the font
	if (!xScale && !yScale)
	{
		xScale = r_fontScale->floatVal;
		yScale = r_fontScale->floatVal;
	}
	if (!font)
		font = ri.media.defaultFont;
	mat = R_MaterialForFont (font, xScale, yScale);

	strColor = color;
	isShadowed = (flags & FS_SHADOW) ? true : false;
	isItalic = (flags & FS_ITALIC) ? true : false;
	r_fontShadowClr.A = strColor.A;

	R_GetFontDimensions (font, xScale, yScale, flags, ftSize);

	startX = x;
	for (i=0 ; *string ; )
	{
		num = *string;
		if (flags & FS_SECONDARY && num < 128)
			num |= 128;

		if (skipNext)
		{
			skipNext = false;
		}
		else if ((num & 127) == COLOR_ESCAPE && *(string+1))
		{
			switch (string[1] & 127)
			{
			case COLOR_ESCAPE:
				string++;
				skipNext = true;
				continue;

			case 'i':
			case 'I':
				isItalic = !isItalic || (flags & FS_ITALIC);
				string += 2;
				continue;

			case 'r':
			case 'R':
				isShadowed = (flags & FS_SHADOW) ? true : false;
				inColorCode = false;
				strColor = Q_BColorWhite;
				string += 2;
				continue;

			case 's':
			case 'S':
				isShadowed = !isShadowed || (flags & FS_SHADOW);
				string += 2;
				continue;

			case COLOR_BLACK:
			case COLOR_RED:
			case COLOR_GREEN:
			case COLOR_YELLOW:
			case COLOR_BLUE:
			case COLOR_CYAN:
			case COLOR_MAGENTA:
			case COLOR_WHITE:
			case COLOR_GREY:
				strColor = Q_BStrColorTable[Q_StrColorIndex(string[1])];
				inColorCode = true;
				string += 2;
				continue;
			}
		}
		else if ((num & 127) == '\n')
		{
			x = startX;
			y += ftSize[1];
			string++;
			continue;
		}

		if (inColorCode)
			num &= 127;
		else
			num &= 255;

		// Skip spaces
		if ((num&127) != 32)
		{
			QuadVertices verts;

			frow = (num>>4) * (1.0f/16.0f);
			fcol = (num&15) * (1.0f/16.0f);

			verts.SetVertices(x, y, ftSize[0], ftSize[1]).SetCoords(fcol, frow, fcol+(1.0f/16.0f), frow+(1.0f/16.0f));

			if (isItalic)
			{
				verts.verts[0][0] += ftSize[0] / 2;
				verts.verts[1][0] += ftSize[0] / 2;
			}

			if (isShadowed)
			{
				QuadVertices shadow (verts);

				for (int z = 0; z < 3; ++z)
				{
					shadow.verts[z][0] += 2;
					shadow.verts[z][1] += 2;
				}

				R_DrawPic (mat, 0, shadow, r_fontShadowClr);
			}

			R_DrawPic (mat, 0, verts, strColor);
		}

		x += ftSize[0];
		string++;
		i++;
	}

	return i;
}


/*
================
R_DrawStringLen
================
*/
int R_DrawStringLen (refFont_t *font, float x, float y, float xScale, float yScale, uint32 flags, char *string, int len, const colorb &color)
{
	char	swap;
	int		length;

	if (len < 0)
		return R_DrawString (font, x, y, xScale, yScale, flags, string, color);

	swap = string[len];
	string[len] = 0;
	length = R_DrawString (font, x, y, xScale, yScale, flags, string, color);
	string[len] = swap;

	return length;
}


/*
================
R_DrawChar
================
*/
void R_DrawChar (refFont_t *font, float x, float y, float xScale, float yScale, uint32 flags, int num, const colorb &color)
{
	float			frow, fcol;
	refMaterial_t	*mat;
	vec2_t			ftSize;

	if (!xScale && !yScale)
	{
		xScale = r_fontScale->floatVal;
		yScale = r_fontScale->floatVal;
	}
	if (!font)
		font = ri.media.defaultFont;
	mat = R_MaterialForFont (font, xScale, yScale);

	R_GetFontDimensions (font, xScale, yScale, flags, ftSize);

	if (flags & FS_SECONDARY && num < 128)
		num |= 128;
	num &= 255;

	if ((num&127) == 32)
		return;	// Space

	frow = (num>>4) * (1.0f/16.0f);
	fcol = (num&15) * (1.0f/16.0f);

	if (flags & FS_SHADOW)
	{
		r_fontShadowClr[3] = color.A;
		R_DrawPic (mat, 0, QuadVertices().SetVertices(x+2, y+2, ftSize[0], ftSize[1]).SetCoords(fcol, frow, fcol+(1.0f/16.0f), frow+(1.0f/16.0f)), r_fontShadowClr);
	}

	R_DrawPic (mat, 0, QuadVertices().SetVertices(x, y, ftSize[0], ftSize[1]).SetCoords(fcol, frow, fcol+(1.0f/16.0f), frow+(1.0f/16.0f)), color);
}

/*
=============================================================================

	INITIALIZATION

=============================================================================
*/

/*
================
R_FontInit
================
*/
void R_FontInit ()
{
	// Set values
	r_fontShadowClr.R = 0;
	r_fontShadowClr.G = 0;
	r_fontShadowClr.B = 0;
	r_fontShadowClr.A = 255;

	// FIXME: load all font files into memory here, and only register the images when needed?

	Mem_CheckPoolIntegrity (ri.fontSysPool);
}


/*
================
R_FontShutdown
================
*/
void R_FontShutdown ()
{
	refFont_t *font;
	uint32	size, i;

	Com_Printf (0, "Font system shutdown:\n");
	// Release fonts
	for (i=0, font=&r_fontList[0] ; i<r_numFonts ; font++, i++)
		R_FreeFont (font);

	r_numFonts = 0;
	memset (&r_fontList[0], 0, sizeof(refFont_t) * MAX_FONTS);

	// Empty the memory pool
	size = Mem_FreePool (ri.fontSysPool);
	Com_Printf (0, "...releasing %u bytes...\n", size);
}
