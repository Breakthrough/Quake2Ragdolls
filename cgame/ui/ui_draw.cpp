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
// ui_draw.c
//

#include "ui_local.h"

/*
=======================================================================

	MENU ITEM DRAWING

=======================================================================
*/

/*
=============
UI_DrawTextBox
=============
*/
void UI_DrawTextBox (float x, float y, float scale, int width, int lines)
{
	int		n;
	float	cx, cy;

	// fill in behind it
	cgi.R_DrawFill (x, y, (width + 2)*scale*8, (lines + 2)*scale*8, Q_BColorBlack);

	// draw left side
	cx = x;
	cy = y;
	cgi.R_DrawChar (NULL, cx, cy, scale, scale, FS_SHADOW, 1, Q_BColorWhite);
	for (n=0 ; n<lines ; n++) {
		cy += (8*scale);
		cgi.R_DrawChar (NULL, cx, cy, scale, scale, FS_SHADOW, 4, Q_BColorWhite);
	}
	cgi.R_DrawChar (NULL, cx, cy+(8*scale), scale, scale, FS_SHADOW, 7, Q_BColorWhite);

	// draw middle
	cx += (8*scale);
	while (width > 0) {
		cy = y;
		cgi.R_DrawChar (NULL, cx, cy, scale, scale, FS_SHADOW, 2, Q_BColorWhite);
		for (n = 0; n < lines; n++) {
			cy += (8*scale);
			cgi.R_DrawChar (NULL, cx, cy, scale, scale, FS_SHADOW, 5, Q_BColorWhite);
		}
		cgi.R_DrawChar (NULL, cx, cy+(8*scale), scale, scale, FS_SHADOW, 8, Q_BColorWhite);
		width -= 1;
		cx += (8*scale);
	}

	// draw right side
	cy = y;
	cgi.R_DrawChar (NULL, cx, cy, scale, scale, FS_SHADOW, 3, Q_BColorWhite);
	for (n=0 ; n<lines ; n++) {
		cy += (8*scale);
		cgi.R_DrawChar (NULL, cx, cy, scale, scale, FS_SHADOW, 6, Q_BColorWhite);
	}
	cgi.R_DrawChar (NULL, cx, cy+(8*scale), scale, scale, FS_SHADOW, 9, Q_BColorWhite);
}


/*
=============
UI_CenterHeight
=============
*/
static void UI_CenterHeight (uiFrameWork_t *fw)
{
	float		lowest, highest, height;
	uiCommon_t	*item;
	int			i;

	if (!fw)
		return;

	highest = lowest = height = 0;
	for (i=0 ; i<fw->numItems ; i++) {
		item = (uiCommon_t *) fw->items[i];

		if (item->y < highest)
			highest = item->y;

		if (item->type == UITYPE_IMAGE)
			height = ((uiImage_t *) fw->items[i])->height * UISCALE_TYPE (item->flags);
		else if (item->flags & UIF_MEDIUM)
			height = UIFT_SIZEINCMED;
		else if (item->flags & UIF_LARGE)
			height = UIFT_SIZEINCLG;
		else
			height = UIFT_SIZEINC;

		if (item->y + height > lowest)
			lowest = item->y + height;
	}

	fw->y += (cg.refConfig.vidHeight - (lowest - highest)) * 0.5f;
}


/*
=============
UI_DrawStatusBar
=============
*/
static void UI_DrawStatusBar (char *string)
{
	if (string) {
		int col = (cg.refConfig.vidWidth*0.5) - (((Q_ColorCharCount (string, (int)strlen (string))) * 0.5) * UIFT_SIZE);

		cgi.R_DrawFill (0, cg.refConfig.vidHeight - UIFT_SIZE - 2, cg.refConfig.vidWidth, UIFT_SIZE + 2, Q_BColorDkGrey);
		cgi.R_DrawString (NULL, col, cg.refConfig.vidHeight - UIFT_SIZE - 1, UIFT_SCALE, UIFT_SCALE, FS_SHADOW, string, Q_BColorWhite);
	}
}


/*
=============
UI_DrawInterface
=============
*/
#define ItemTxtFlags(i, outflags) { \
	if (i->generic.flags & UIF_SHADOW) outflags |= FS_SHADOW; \
}
static void Action_Draw (uiAction_t *a)
{
	uint32	txtflags = 0;
	float	xoffset = 0;

	if (!a) return;
	if (!a->generic.name) return;

	ItemTxtFlags (a, txtflags);

	if (a->generic.flags & UIF_LEFT_JUSTIFY)
		xoffset = 0;
	else if (a->generic.flags & UIF_CENTERED)
		xoffset = (((Q_ColorCharCount (a->generic.name, (int)strlen (a->generic.name))) * UISIZE_TYPE (a->generic.flags))*0.5);
	else
		xoffset = ((Q_ColorCharCount (a->generic.name, (int)strlen (a->generic.name))) * UISIZE_TYPE (a->generic.flags)) + RCOLUMN_OFFSET;

	cgi.R_DrawString (NULL, a->generic.x + a->generic.parent->x - xoffset, a->generic.y + a->generic.parent->y,
		UISCALE_TYPE (a->generic.flags), UISCALE_TYPE (a->generic.flags), txtflags, a->generic.name, Q_BColorWhite);

	if (a->generic.ownerDraw)
		a->generic.ownerDraw (a);
}

static void Field_Draw (uiField_t *f)
{
	int		i, curOffset;
	char	tempbuffer[128]="";
	float	x, y;
	uint32	txtflags = 0;

	if (!f) return;

	ItemTxtFlags (f, txtflags);

	x = f->generic.x + f->generic.parent->x;
	y = f->generic.y + f->generic.parent->y;

	if (f->generic.flags & UIF_CENTERED) {
		x -= ((f->visibleLength + 2) * UISIZE_TYPE (f->generic.flags)) * 0.5;

		if (f->generic.name)
			x -= (Q_ColorCharCount (f->generic.name, (int)strlen (f->generic.name)) * UISIZE_TYPE (f->generic.flags)) * 0.5;
	}

	// title
	if (f->generic.name)
		cgi.R_DrawString (NULL, x - ((Q_ColorCharCount (f->generic.name, (int)strlen (f->generic.name)) + 1) * UISIZE_TYPE (f->generic.flags)),
						y, UISCALE_TYPE (f->generic.flags), UISCALE_TYPE (f->generic.flags), txtflags, f->generic.name, Q_BColorGreen);

	Q_strncpyz (tempbuffer, f->buffer + f->visibleOffset, f->visibleLength);

	// left
	cgi.R_DrawChar (NULL, x, y - (UISIZE_TYPE (f->generic.flags)*0.5), UISCALE_TYPE (f->generic.flags), UISCALE_TYPE (f->generic.flags), txtflags, 18, Q_BColorWhite);
	cgi.R_DrawChar (NULL, x, y + (UISIZE_TYPE (f->generic.flags)*0.5), UISCALE_TYPE (f->generic.flags), UISCALE_TYPE (f->generic.flags), txtflags, 24, Q_BColorWhite);

	// right
	cgi.R_DrawChar (NULL, x + UISIZE_TYPE (f->generic.flags) + (f->visibleLength * UISIZE_TYPE (f->generic.flags)), y - (UISIZE_TYPE (f->generic.flags)*0.5), UISCALE_TYPE (f->generic.flags), UISCALE_TYPE (f->generic.flags), txtflags, 20, Q_BColorWhite);
	cgi.R_DrawChar (NULL, x + UISIZE_TYPE (f->generic.flags) + (f->visibleLength * UISIZE_TYPE (f->generic.flags)), y + (UISIZE_TYPE (f->generic.flags)*0.5), UISCALE_TYPE (f->generic.flags), UISCALE_TYPE (f->generic.flags), txtflags, 26, Q_BColorWhite);

	// middle
	for (i=0 ; i<f->visibleLength ; i++) {
		cgi.R_DrawChar (NULL, x + UISIZE_TYPE (f->generic.flags) + (i * UISIZE_TYPE (f->generic.flags)), y - (UISIZE_TYPE (f->generic.flags)*0.5), UISCALE_TYPE (f->generic.flags), UISCALE_TYPE (f->generic.flags), txtflags, 19, Q_BColorWhite);
		cgi.R_DrawChar (NULL, x + UISIZE_TYPE (f->generic.flags) + (i * UISIZE_TYPE (f->generic.flags)), y + (UISIZE_TYPE (f->generic.flags)*0.5), UISCALE_TYPE (f->generic.flags), UISCALE_TYPE (f->generic.flags), txtflags, 25, Q_BColorWhite);
	}

	// text
	curOffset = cgi.R_DrawString (NULL, x + UISIZE_TYPE (f->generic.flags), y, UISCALE_TYPE (f->generic.flags), UISCALE_TYPE (f->generic.flags), txtflags, tempbuffer, Q_BColorWhite);

	// blinking cursor
	if (UI_ItemAtCursor (f->generic.parent) == f) {
		int offset;

		if (f->visibleOffset)
			offset = f->visibleLength;
		else
			offset = curOffset;

		offset++;

		if ((((int)(cg.realTime / 250))&1))
			cgi.R_DrawChar (NULL, x + (offset * UISIZE_TYPE (f->generic.flags)),
				y, UISCALE_TYPE (f->generic.flags), UISCALE_TYPE (f->generic.flags), txtflags, 11, Q_BColorWhite);
	}
}

static void Image_Draw (uiImage_t *i)
{
	struct	refMaterial_t *mat;
	float	x, y;
	int		width, height;

	if (!i)
		return;

	if ((uiState.cursorItem && uiState.cursorItem == i) && i->hoverMat)
		mat = i->hoverMat;
	else
		mat = i->mat;
	if (!mat)
		mat = cgMedia.whiteTexture;

	if (i->width || i->height) {
		width = i->width;
		height = i->height;
	}
	else {
		cgi.R_GetImageSize (mat, &width, &height);
		i->width = width;
		i->height = height;
	}

	width *= UISCALE_TYPE (i->generic.flags);
	height *= UISCALE_TYPE (i->generic.flags);

	x = i->generic.x + i->generic.parent->x;
	if (i->generic.flags & UIF_CENTERED)
		x -= width * 0.5f;
	else if (i->generic.flags & UIF_LEFT_JUSTIFY)
		x += LCOLUMN_OFFSET;

	y = i->generic.y + i->generic.parent->y;

	cgi.R_DrawPic (mat, 0, QuadVertices().SetVertices(x, y,
				width, height),
				Q_BColorWhite);
}

static void Slider_Draw (uiSlider_t *s)
{
	uint32	txtflags = 0;
	int		i;

	if (!s) return;

	ItemTxtFlags (s, txtflags);

	// name
	if (s->generic.name) {
		cgi.R_DrawString (NULL, s->generic.x + s->generic.parent->x + LCOLUMN_OFFSET - ((Q_ColorCharCount (s->generic.name, (int)strlen (s->generic.name)) - 1)*UISIZE_TYPE (s->generic.flags)),
			s->generic.y + s->generic.parent->y, UISCALE_TYPE (s->generic.flags), UISCALE_TYPE (s->generic.flags), txtflags, s->generic.name, Q_BColorGreen);
	}

	s->range = clamp (((s->curValue - s->minValue) / (float) (s->maxValue - s->minValue)), 0, 1);

	// left
	cgi.R_DrawChar (NULL, s->generic.x + s->generic.parent->x + RCOLUMN_OFFSET, s->generic.y + s->generic.parent->y, UISCALE_TYPE (s->generic.flags), UISCALE_TYPE (s->generic.flags), txtflags, 128, Q_BColorWhite);

	// center
	for (i=0 ; i<SLIDER_RANGE ; i++)
		cgi.R_DrawChar (NULL, RCOLUMN_OFFSET + s->generic.x + i*UISIZE_TYPE (s->generic.flags) + s->generic.parent->x + UISIZE_TYPE (s->generic.flags), s->generic.y + s->generic.parent->y, UISCALE_TYPE (s->generic.flags), UISCALE_TYPE (s->generic.flags), txtflags, 129, Q_BColorWhite);

	// right
	cgi.R_DrawChar (NULL, RCOLUMN_OFFSET + s->generic.x + i*UISIZE_TYPE (s->generic.flags) + s->generic.parent->x + UISIZE_TYPE (s->generic.flags), s->generic.y + s->generic.parent->y, UISCALE_TYPE (s->generic.flags), UISCALE_TYPE (s->generic.flags), txtflags, 130, Q_BColorWhite);

	// slider bar
	cgi.R_DrawChar (NULL, (UISIZE_TYPE (s->generic.flags) + RCOLUMN_OFFSET + s->generic.parent->x + s->generic.x + (SLIDER_RANGE-1)*UISIZE_TYPE (s->generic.flags) * s->range), s->generic.y + s->generic.parent->y, UISCALE_TYPE (s->generic.flags), UISCALE_TYPE (s->generic.flags), txtflags, 131, Q_BColorWhite);
}

static void SpinControl_Draw (uiList_t *s)
{
	uint32	txtflags = 0;
	char	buffer[100];

	if (!s)
		return;

	ItemTxtFlags (s, txtflags);

	if (s->generic.name) {
		cgi.R_DrawString (NULL, s->generic.x + s->generic.parent->x + LCOLUMN_OFFSET - ((Q_ColorCharCount (s->generic.name, (int)strlen (s->generic.name)) - 1)*UISIZE_TYPE (s->generic.flags)),
					s->generic.y + s->generic.parent->y, UISCALE_TYPE (s->generic.flags), UISCALE_TYPE (s->generic.flags), txtflags, s->generic.name, Q_BColorGreen);
	}

	if (!s->itemNames || !s->itemNames[s->curValue])
		return;

	if (!strchr (s->itemNames[s->curValue], '\n')) {
		cgi.R_DrawString (NULL, RCOLUMN_OFFSET + s->generic.x + s->generic.parent->x,
					s->generic.y + s->generic.parent->y, UISCALE_TYPE (s->generic.flags), UISCALE_TYPE (s->generic.flags), txtflags, s->itemNames[s->curValue], Q_BColorWhite);
	}
	else {
		Q_strncpyz (buffer, s->itemNames[s->curValue], sizeof(buffer));
		*strchr (buffer, '\n') = 0;
		cgi.R_DrawString (NULL, RCOLUMN_OFFSET + s->generic.x + s->generic.parent->x,
					s->generic.y + s->generic.parent->y, UISCALE_TYPE (s->generic.flags), UISCALE_TYPE (s->generic.flags), txtflags, buffer, Q_BColorWhite);

		Q_strncpyz (buffer, strchr (s->itemNames[s->curValue], '\n') + 1, sizeof(buffer));
		cgi.R_DrawString (NULL, RCOLUMN_OFFSET + s->generic.x + s->generic.parent->x,
					s->generic.y + s->generic.parent->y + UISIZEINC_TYPE(s->generic.flags), UISCALE_TYPE (s->generic.flags), UISCALE_TYPE (s->generic.flags), txtflags, buffer, Q_BColorWhite);
	}
}

static void SpinControl_Better_Draw (uiList_Better_t *s)
{
	uint32	txtflags = 0;

	if (!s)
		return;

	ItemTxtFlags (s, txtflags);

	if (s->generic.name) {
		cgi.R_DrawString (NULL, s->generic.x + s->generic.parent->x + LCOLUMN_OFFSET - ((Q_ColorCharCount (s->generic.name, (int)strlen (s->generic.name)) - 1)*UISIZE_TYPE (s->generic.flags)),
					s->generic.y + s->generic.parent->y, UISCALE_TYPE (s->generic.flags), UISCALE_TYPE (s->generic.flags), txtflags, s->generic.name, Q_BColorGreen);
	}

	if (!s->itemNames.Count() || s->itemNames[s->curValue].IsNullOrEmpty())
		return;

	if (s->itemNames[s->curValue].IndexOf('\n') == -1) {
		cgi.R_DrawString (NULL, RCOLUMN_OFFSET + s->generic.x + s->generic.parent->x,
					s->generic.y + s->generic.parent->y, UISCALE_TYPE (s->generic.flags), UISCALE_TYPE (s->generic.flags), txtflags, (char*)s->itemNames[s->curValue].CString(), Q_BColorWhite);
	}
	else
	{
		var spl = s->itemNames[s->curValue].Split('\n', false);

		cgi.R_DrawString (NULL, RCOLUMN_OFFSET + s->generic.x + s->generic.parent->x,
					s->generic.y + s->generic.parent->y, UISCALE_TYPE (s->generic.flags), UISCALE_TYPE (s->generic.flags), txtflags, (char*)spl[0].CString(), Q_BColorWhite);

		cgi.R_DrawString (NULL, RCOLUMN_OFFSET + s->generic.x + s->generic.parent->x,
					s->generic.y + s->generic.parent->y + UISIZEINC_TYPE(s->generic.flags), UISCALE_TYPE (s->generic.flags), UISCALE_TYPE (s->generic.flags), txtflags, (char*)spl[1].CString(), Q_BColorWhite);
	}
}

static void Selection_Draw (uiCommon_t *i)
{
	if (!(i->flags & UIF_NOSELBAR))
	{
		if (i->flags & UIF_FORCESELBAR || ((uiState.cursorItem && uiState.cursorItem == i) || (uiState.selectedItem && uiState.selectedItem == i)))
			cgi.R_DrawFill (i->topLeft[0], i->topLeft[1], i->botRight[0] - i->topLeft[0], i->botRight[1] - i->topLeft[1], colorb(Q_BColorDkGrey.R, Q_BColorDkGrey.G, Q_BColorDkGrey.B, 85));
	}
}

void UI_DrawInterface (uiFrameWork_t *fw)
{
	int			i;
	uiCommon_t	*item;
	uiCommon_t	*selitem;

	// Center the height
	if (fw->flags & FWF_CENTERHEIGHT)
		UI_CenterHeight (fw);

	// Setup collision bounds
	UI_SetupBounds (fw);

	// Make sure we're on a valid item
	UI_AdjustCursor (fw, 1);

	// Draw the items
	for (i=0 ; i<fw->numItems ; i++) {
		Selection_Draw ((uiCommon_t *) fw->items[i]);

		switch (((uiCommon_t *) fw->items[i])->type) {
		case UITYPE_ACTION:
			Action_Draw ((uiAction_t *) fw->items[i]);
			break;

		case UITYPE_FIELD:
			Field_Draw ((uiField_t *) fw->items[i]);
			break;

		case UITYPE_IMAGE:
			Image_Draw ((uiImage_t *) fw->items[i]);
			break;

		case UITYPE_SLIDER:
			Slider_Draw ((uiSlider_t *) fw->items[i]);
			break;

		case UITYPE_SPINCONTROL:
			SpinControl_Draw ((uiList_t *) fw->items[i]);
			break;

		case UITYPE_SPINCONTROL_BETTER:
			SpinControl_Better_Draw ((uiList_Better_t *) fw->items[i]);
			break;
		}
	}

	//
	// Blinking cursor
	//

	item = (uiCommon_t*)UI_ItemAtCursor (fw);
	if (item && item->cursorDraw)
		item->cursorDraw (item);
	else if (fw->cursorDraw)
		fw->cursorDraw (fw);
	else if (item && (item->type != UITYPE_FIELD) && (item->type != UITYPE_IMAGE) && (!(item->flags & UIF_NOSELECT))) {
		if (item->flags & UIF_LEFT_JUSTIFY)
			cgi.R_DrawChar (NULL, fw->x + item->x + LCOLUMN_OFFSET + item->cursorOffset, fw->y + item->y, UISCALE_TYPE (item->flags), UISCALE_TYPE (item->flags), 0, 12 + ((int)(cg.realTime/250) & 1), Q_BColorWhite);
		else if (item->flags & UIF_CENTERED)
			cgi.R_DrawChar (NULL, fw->x + item->x - (((Q_ColorCharCount (item->name, (int)strlen (item->name)) + 4) * UISIZE_TYPE (item->flags))*0.5) + item->cursorOffset, fw->y + item->y, UISCALE_TYPE (item->flags), UISCALE_TYPE (item->flags), 0, 12 + ((int)(cg.realTime/250) & 1), Q_BColorWhite);
		else
			cgi.R_DrawChar (NULL, fw->x + item->x + item->cursorOffset, fw->y + item->y, UISCALE_TYPE (item->flags), UISCALE_TYPE (item->flags), 0, 12 + ((int)(cg.realTime/250) & 1), Q_BColorWhite);
	}

	selitem = (uiCommon_t *) uiState.selectedItem;
	if (selitem && (selitem->type != UITYPE_FIELD) && (selitem->type != UITYPE_IMAGE) && (!(selitem->flags & UIF_NOSELECT))) {
		if (selitem->flags & UIF_LEFT_JUSTIFY)
			cgi.R_DrawChar (NULL, fw->x + selitem->x + LCOLUMN_OFFSET + selitem->cursorOffset, fw->y + selitem->y, UISCALE_TYPE (selitem->flags), UISCALE_TYPE (selitem->flags), 0, 12 + ((int)(cg.realTime/250) & 1), Q_BColorWhite);
		else if (selitem->flags & UIF_CENTERED)
			cgi.R_DrawChar (NULL, fw->x + selitem->x - (((Q_ColorCharCount (selitem->name, (int)strlen (selitem->name)) + 4) * UISIZE_TYPE (selitem->flags))*0.5) + selitem->cursorOffset, fw->y + selitem->y, UISCALE_TYPE (selitem->flags), UISCALE_TYPE (selitem->flags), 0, 12 + ((int)(cg.realTime/250) & 1), Q_BColorWhite);
		else
			cgi.R_DrawChar (NULL, fw->x + selitem->x + selitem->cursorOffset, fw->y + selitem->y, UISCALE_TYPE (selitem->flags), UISCALE_TYPE (selitem->flags), 0, 12 + ((int)(cg.realTime/250) & 1), Q_BColorWhite);
	}

	//
	// Statusbar
	//
	if (item) {
		if (item->statusBarFunc)
			item->statusBarFunc ((void *) item);
		else if (item->statusBar)
			UI_DrawStatusBar (item->statusBar);
		else
			UI_DrawStatusBar (fw->statusBar);
	}
	else
		UI_DrawStatusBar (fw->statusBar);
}


/*
=================
UI_Refresh
=================
*/
void UI_Refresh (bool fullScreen)
{
	if (!cg.menuOpen || !uiState.drawFunc)
		return;

	// Draw the backdrop
	if (fullScreen)
	{
		cgi.R_DrawPic (uiMedia.bgBig, 0, QuadVertices().SetVertices(0, 0, cg.refConfig.vidWidth, cg.refConfig.vidHeight), Q_BColorWhite);
	}
	else
	{
		cgi.R_DrawFill (0, 0, cg.refConfig.vidWidth, cg.refConfig.vidHeight, colorb(Q_BColorBlack.R, Q_BColorBlack.G, Q_BColorBlack.B, 204));
	}

	// Draw the menu
	uiState.drawFunc ();

	// Draw the cursor
	UI_DrawMouseCursor ();

	// Let the menu do it's updates
	M_Refresh ();
}
