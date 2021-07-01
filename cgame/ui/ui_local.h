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
// ui_local.h
// UI attributes CGame doesn't need to see
//

#ifndef __UI_LOCAL_H__
#define __UI_LOCAL_H__

#include "../cg_local.h"

/*
=============================================================================

	SCALING

=============================================================================
*/

#define UI_HSCALE			((float)(cg.refConfig.vidWidth / 640.0))
#define UI_VSCALE			((float)(cg.refConfig.vidHeight / 480.0))

#define UI_SCALE			((float)(cg.refConfig.vidWidth / 640.0))

#define UIFT_SCALE			(UI_SCALE)
#define UIFT_SIZE			(8 * UIFT_SCALE)
#define UIFT_SIZEINC		(UIFT_SIZE)

#define UIFT_SCALEMED		(UI_SCALE*1.25)
#define UIFT_SIZEMED		(8 * UIFT_SCALEMED)
#define UIFT_SIZEINCMED		(UIFT_SIZEMED)

#define UIFT_SCALELG		(UI_SCALE*1.5)
#define UIFT_SIZELG			(8 * UIFT_SCALELG)
#define UIFT_SIZEINCLG		(UIFT_SIZELG)

#define UISCALE_TYPE(flags)		((flags&UIF_LARGE) ? UIFT_SCALELG	: (flags&UIF_MEDIUM) ? UIFT_SCALEMED	: UIFT_SCALE)
#define UISIZE_TYPE(flags)		((flags&UIF_LARGE) ? UIFT_SIZELG	: (flags&UIF_MEDIUM) ? UIFT_SIZEMED		: UIFT_SIZE)
#define UISIZEINC_TYPE(flags)	((flags&UIF_LARGE) ? UIFT_SIZEINCLG	: (flags&UIF_MEDIUM) ? UIFT_SIZEINCMED	: UIFT_SIZEINC)

/*
=============================================================================

	UI FRAMEWORK

=============================================================================
*/

#define MAX_UI_DEPTH		32
#define MAX_UI_ITEMS		128

// Framework flags
enum {
	FWF_CENTERHEIGHT		= BIT(0),
	FWF_INTERFACE			= BIT(1),
};

struct uiFrameWork_t {
	bool					locked;
	bool					initialized;
	int						flags;

	float					x, y;

	int						cursor;		// Item last selected
	int						numItems;
	void					*items[MAX_UI_ITEMS];

	char					*statusBar;

	void					(*cursorDraw) (uiFrameWork_t *m);
};

/*
=============================================================================

	UI ITEMS

=============================================================================
*/

// Item types
enum {
	UITYPE_ACTION,
	UITYPE_FIELD,
	UITYPE_IMAGE,
	UITYPE_SLIDER,
	UITYPE_SPINCONTROL,
	UITYPE_SPINCONTROL_BETTER,
};

// Item flags
enum {
	UIF_LEFT_JUSTIFY	= BIT(0),
	UIF_CENTERED		= BIT(1),

	UIF_NUMBERSONLY		= BIT(2),

	UIF_SHADOW			= BIT(3),

	UIF_MEDIUM			= BIT(4),
	UIF_LARGE			= BIT(5),
	UIF_NOTOOLTIP		= BIT(6),

	UIF_NOSELECT		= BIT(7),
	UIF_DBLCLICK		= BIT(8),
	UIF_SELONLY			= BIT(9),
	UIF_NOSELBAR		= BIT(10),
	UIF_FORCESELBAR		= BIT(11)
};

#define RCOLUMN_OFFSET		(UIFT_SIZE*2)
#define LCOLUMN_OFFSET		(-(RCOLUMN_OFFSET))

#define SLIDER_RANGE		10

struct uiCommon_t {
	int						type;
	uint32					flags;

	char					*name;

	float					x;
	float					y;

	uiFrameWork_t			*parent;
	int						cursorOffset;
	int						localData[4];

	char					*statusBar;
	float					topLeft[2];		// top left for mouse collision
	float					botRight[2];	// bottom right for mouse collision

	void (*callBack)		(void *self);

	void (*statusBarFunc)	(void *self);
	void (*ownerDraw)		(void *self);
	void (*cursorDraw)		(void *self);
};

struct uiAction_t {
	uiCommon_t				generic;
};

struct uiField_t {
	uiCommon_t				generic;

	char					buffer[80];
	int						cursor;

	int						length;
	int						visibleLength;
	int						visibleOffset;
};

struct uiList_t {
	uiCommon_t				generic;

	int						curValue;

	char					**itemNames;
	int						numItemNames;
};

struct uiList_Better_t {
	uiCommon_t				generic;

	int						curValue;

	TList<String>			itemNames;
};

struct uiImage_t {
	uiCommon_t				generic;

	struct refMaterial_t	*mat;
	struct refMaterial_t	*hoverMat;

	int						width;
	int						height;
};

struct uiSlider_t {
	uiCommon_t				generic;

	float					minValue;
	float					maxValue;
	float					curValue;

	float					range;
};

/*
=============================================================================

	UI STATE

=============================================================================
*/

struct uiState_t {
	// Cursor information
	float			cursorX;
	float			cursorY;

	int				cursorW;
	int				cursorH;

	bool			newCursorItem;				// Item changed, trigger sounds
	bool			cursorOverItem;				// Mouse over an item?
	bool			cursorLock;					// Locks mouse cursor movement
	void			*cursorItem;				// Item mouse is over or keyboard moved to
	void			*mouseItem;					// Item mouse is over
	void			*selectedItem;				// For SELONLY items

	// Active interface information
	uiFrameWork_t	*activeUI;
	void			(*drawFunc) ();
	struct sfx_t	*(*closeFunc) ();
	struct sfx_t	*(*keyFunc) (uiFrameWork_t *fw, keyNum_t keyNum);
};

extern uiState_t	uiState;

/*
=============================================================================

	CURSOR

=============================================================================
*/

void	UI_DrawMouseCursor ();
void	UI_UpdateMousePos ();
void	UI_MoveMouse (float x, float y);
void	UI_SetupBounds (uiFrameWork_t *menu);

void	UI_CursorInit ();
void	UI_CursorShutdown ();

/*
=============================================================================

	CVARS

=============================================================================
*/

extern cVar_t	*ui_filtermouse;
extern cVar_t	*ui_sensitivity;

/*
=============================================================================

	SUPPORTING FUNCTIONS

=============================================================================
*/

//
// ui_backend.c
//

void		UI_InitMedia ();
void		UI_InitSoundMedia ();

void		UI_Init ();
void		UI_Shutdown ();

void		UI_PushInterface (uiFrameWork_t *fw, void (*drawFunc) (), struct sfx_t *(*closeFunc) (), struct sfx_t *(*keyFunc) (uiFrameWork_t *fw, keyNum_t keyNum));
void		UI_PopInterface ();
void		UI_ForceAllOff ();

void		UI_StartFramework (uiFrameWork_t *fw, int flags);
void		UI_FinishFramework (uiFrameWork_t *fw, bool lock);

//
// ui_draw.c
//

void		UI_DrawTextBox (float x, float y, float scale, int width, int lines);
void		UI_DrawInterface (uiFrameWork_t *fw);
void		UI_Refresh (bool fullScreen);

//
// ui_items.c
//

void		UI_SetupItem (void *item);
void		UI_AddItem (uiFrameWork_t *fw, void *item);
void		UI_RemoveItem (uiFrameWork_t *fw, void *item);

void		UI_AdjustCursor (uiFrameWork_t *fw, int dir);
void		*UI_ItemAtCursor (uiFrameWork_t *fw);
void		UI_SelectItem (uiCommon_t *item);
bool		UI_SlideItem (uiCommon_t *item, int dir);

//
// ui_keys.c
//

void		UI_KeyDown (keyNum_t keyNum, bool isDown);
struct sfx_t *UI_DefaultKeyFunc (uiFrameWork_t *fw, keyNum_t keyNum);

#endif // __UI_LOCAL_H__
