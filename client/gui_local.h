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
// gui_local.h
//

#include "cl_local.h"

#define MAX_GUIS			512				// Maximum in-memory GUIs

#define MAX_GUI_CHILDREN	32				// Maximum children per child
#define MAX_GUI_DEFINES		32				// Maximum definefloat/definevec's
#define MAX_GUI_DEPTH		32				// Maximum opened at the same time
#define MAX_GUI_HASH		(MAX_GUIS/4)	// Hash optimization
#define MAX_GUI_NAMELEN		64				// Maximum window name length

#define MAX_GUI_EVENTS		16				// Maximum events per window
#define MAX_EVENT_ARGS		1
#define MAX_EVENT_ARGLEN	64
#define MAX_EVENT_ACTIONS	16				// Maximum actions per event per window

//
// Script path location
// This is so that moddir gui's of the same name
// have precedence over basedir ones
//
typedef enum guiPathType_s {
	GUIPT_BASEDIR,
	GUIPT_MODDIR,
} guiPathType_t;

//
// Window types
//
typedef enum guiType_s {
	WTP_GUI,
	WTP_GENERIC,

	WTP_BIND,
	WTP_CHECKBOX,
	WTP_CHOICE,
	WTP_EDIT,
	WTP_LIST,
	WTP_RENDER,
	WTP_SLIDER,
	WTP_TEXT,

	WTP_MAX
} guiType_t;

//
// Window flags
// FIXME: yyuucckk
//
typedef uint32 guiFlags_t;
enum {
	WFL_CURSOR			= BIT(0),
	WFL_FILL_COLOR		= BIT(1),
	WFL_MATERIAL		= BIT(2),
	WFL_ITEM			= BIT(3),
};

/*
=============================================================================

	MEMORY MANAGEMENT

=============================================================================
*/

enum {
	GUITAG_KEEP,			// What's kept in memory at all times
	GUITAG_SCRATCH,			// Scratch tag for init. When a GUI is complete it's tag is shifted to GUITAG_KEEP
	GUITAG_VARS,			// for GUI vars
};

#define GUI_AllocTag(size,tagNum)			_Mem_Alloc((size),cl_guiSysPool,(tagNum),__FILE__,__LINE__)
#define GUI_FreeTag(tagNum)					_Mem_FreeTag(cl_guiSysPool,(tagNum),__FILE__,__LINE__)
#define gui_ttrDup(in,tagNum)				_Mem_PoolStrDup((in),cl_guiSysPool,(tagNum),__FILE__,__LINE__)
#define GUI_MemFree(ptr)					_Mem_Free((ptr),__FILE__,__LINE__)

/*
=============================================================================

	GUI VARIABLES

=============================================================================
*/

#define MAX_GUIVARS			1024
#define MAX_GV_NAMELEN		64
#define MAX_GV_STRLEN		1024	// Maximum GVT_STR length

struct guiVar_t {
	char				*name;
	guiVarType_t		type;

	bool				modified;

	char				*strVal;
	float				floatVal;
	vec4_t				vecVal;
};

void		GUIVar_Init ();
void		GUIVar_Shutdown ();

/*
=============================================================================

	GUI CURSOR

	Each GUI can script their cursor setup, and modify (with events)
	the properties.
=============================================================================
*/

struct guiCursorData_t {
	bool				visible;

	char					matName[MAX_QPATH];
	struct refMaterial_t	*matPtr;
	vec4_t					color;

	bool				locked;
	vec2_t				pos;
	vec2_t				size;
};

struct guiCursor_t {
	// Static (compiled) information
	guiCursorData_t		s;

	// Dynamic information
	guiCursorData_t		d;

	struct gui_t		*curWindow;
	bool				mouseMoved;
};

/*
=============================================================================

	GUI SHARED DATA

	This data only exists once in any given GUI, and is just pointed to by
	all of the children. It's used for things that do not need to be in
	every single window.
=============================================================================
*/

struct guiShared_t {
	guiPathType_t		pathType;
	guiCursor_t			cursor;

	bool				queueClose;

	float				xScale;
	float				yScale;
};

/*
=============================================================================

	WINDOW DEFINES

	These can be changed at any time in the script using event actions.
=============================================================================
*/

struct defineFloat_t {
	char				name[MAX_GUI_NAMELEN];
	float				value;
};

struct defineVec_t {
	char				name[MAX_GUI_NAMELEN];
	vec4_t				value;
};

/*
=============================================================================

	EVENT ACTIONS

=============================================================================
*/

typedef enum evaType_s {
	EVA_NONE,

	EVA_CLOSE,
	EVA_COMMAND,
	EVA_IF,
	EVA_LOCAL_SOUND,
	EVA_NAMED_EVENT,
	EVA_RESET_TIME,
	EVA_SET,
	EVA_STOP_TRANSITIONS,
	EVA_TRANSITION,

	EVA_MAX
} evaType_t;

//
// EVA_LOCAL_SOUND
//

struct eva_localSound_t {
	char				name[MAX_QPATH];
	struct sfx_t		*sound;
	float				volume;
};

//
// EVA_NAMED_EVENT
//

struct eva_named_t {
	char				destWindowName[MAX_GUI_NAMELEN];
	struct gui_t		*destWindowPtr;

	char				eventName[MAX_GUI_NAMELEN];
};

//
// EVA_SET
//
typedef int set_destType_t;
enum {
	EVA_SETDEST_FLOAT	= BIT(0),
	EVA_SETDEST_VEC		= BIT(1),

	// Or'd with what's above
	EVA_SETDEST_STORAGE	= BIT(2),
	EVA_SETDEST_DEF		= BIT(3)
};

typedef enum set_srcType_s {
	EVA_SETSRC_STORAGE,
	EVA_SETSRC_DEF,
	EVA_SETSRC_GUIVAR,
} set_srcType_t;

struct eva_set_t {
	// Destination
	char				destWindowName[MAX_GUI_NAMELEN];
	struct gui_t		*destWindowPtr;
	guiType_t			destWindowType;

	char				destVarName[MAX_GUI_NAMELEN];

	set_destType_t		destType;

	// EVA_SETDEST_DEF
	byte				destDefIndex;

	// EVA_SETDEST_STORAGE
	uint32				destRegister;
	byte				destNumVecs;

	// Source
	set_srcType_t		srcType;

	// EVA_SETSRC_STORAGE
	vec4_t				srcStorage;

	// EVA_SETSRC_DEF
	char				srcWindowName[MAX_GUI_NAMELEN];
	struct gui_t		*srcWindowPtr;

	char				srcName[MAX_GUI_NAMELEN];
	defineFloat_t		*srcDefFloat;
	defineVec_t			*srcDefVec;

	// EVA_SETSRC_GUIVAR
	guiVar_t			*srcGUIVar;
};

//
// Action handling
//
struct evAction_t {
	evaType_t			type;

	// EVA_CLOSE
	// EVA_COMMAND
	char				*command;

	// EVA_IF
	// EVA_LOCAL_SOUND
	eva_localSound_t	*localSound;

	// EVA_NAMED_EVENT
	eva_named_t			*named;

	// EVA_RESET_TIME
	int					resetTime;

	// EVA_SET
	eva_set_t			*set;

	// EVA_STOP_TRANSITIONS
	// EVA_TRANSITION
};

/*
=============================================================================

	WINDOW EVENTS

=============================================================================
*/

enum evType_t {
	WEV_NONE,

	WEV_ACTION,
	WEV_ESCAPE,
	WEV_FRAME,
	WEV_INIT,
	WEV_MOUSE_ENTER,
	WEV_MOUSE_EXIT,
	WEV_NAMED,
	WEV_SHUTDOWN,
	WEV_TIME,

	WEV_MAX
};

struct event_t {
	evType_t			type;

	byte				numActions;
	evAction_t			*actionList;

	// WEV_NAMED
	char				*named;

	// WEV_TIME
	uint32				onTime;
};

/*
=============================================================================

	WINDOW REGISTERS

	These can be changed at any time in the script using event actions.
=============================================================================
*/

enum regSource_t {
	REG_SOURCE_SELF,
	REG_SOURCE_DEF,
	REG_SOURCE_GUIVAR,

	REG_SOURCE_MAX
};

//
// Vector registers
//
#define VRVALUE(gui,reg) ((gui)->d.vecRegisters[(reg)].variable)
enum {
	// textDef windows
	VR_TEXT_COLOR,
	VR_TEXT_HOVERCOLOR,

	// ALL windows
	VR_FILL_COLOR,
	VR_MAT_COLOR,
	VR_RECT,

	VR_MAX,
};

struct vecRegister_t {
	regSource_t			sourceType;
	float				*variable;

	// REG_SOURCE_SELF
	vec4_t				storage;

	// REG_SOURCE_DEF
	struct gui_t		*defVecWindow;
	int					defVecIndex;

	// REG_SOURCE_GUIVAR
	guiVar_t			*guiVar;
};

//
// Float registers
//
#define FRVALUE(gui,reg) (*((gui)->d.floatRegisters[(reg)].variable))

enum {
	// textDef windows
	FR_TEXT_ALIGN,
	FR_TEXT_SCALE,
	FR_TEXT_SHADOW,

	// ALL windows
	FR_MAT_SCALE_X,
	FR_MAT_SCALE_Y,

	FR_MODAL,
	FR_NO_EVENTS,
	FR_NO_TIME,
	FR_ROTATION,		// FIXME: Todo
	FR_VISIBLE,
	FR_WANT_ENTER,

	FR_MAX
};

struct floatRegister_t {
	regSource_t			sourceType;			// storage, defFloat, or guiVar
	float				*variable;

	// REG_SOURCE_SELF
	float				storage;

	// REG_SOURCE_DEF
	struct gui_t		*defFloatWindow;
	int					defFloatIndex;

	// REG_SOURCE_GUIVAR
	guiVar_t			*guiVar;
};

/*
=============================================================================

	WINDOW STRUCTURES

=============================================================================
*/

//
// bindDef
//
struct bindDef_t {
	keyNum_t			keyNum;
};

//
// checkDef
//
struct checkDef_t {
	bool				liveUpdate;

	char					offMatName[MAX_QPATH];
	struct refMaterial_t	*offMatPtr;

	char					onMatName[MAX_QPATH];
	struct refMaterial_t	*onMatPtr;

	char				*values[2];

	cVar_t				*cvar;
};

//
// choiceDef
//
struct choiceDef_t {
	int					todo;
};

//
// editDef
//
struct editDef_t {
	int					todo;
};

//
// listDef
//
struct listDef_t {
	bool				scrollBar[2];
};

//
// renderDef
//
struct renderDef_t {
	int					todo;
};

//
// sliderDef
//
struct sliderDef_t {
	int					todo;
};

//
// textDef
//
#define MAX_TEXTDEF_STRLEN		1024

struct textDef_t {
	char				fontName[MAX_QPATH];
	struct refFont_t	*fontPtr;

	char				*textString;
	size_t				textStringLen;
};

//
// Generic window data
//
struct guiData_t {
	byte				numDefFloats;
	defineFloat_t		*defFloatList;
	byte				numDefVecs;
	defineVec_t			*defVecList;

	floatRegister_t		floatRegisters[FR_MAX];
	vecRegister_t		vecRegisters[VR_MAX];

	bindDef_t			*bindDef;
	checkDef_t			*checkDef;
	choiceDef_t			*choiceDef;
	editDef_t			*editDef;
	listDef_t			*listDef;
	renderDef_t			*renderDef;
	sliderDef_t			*sliderDef;
	textDef_t			*textDef;
};

struct gui_t {
	// Static (compiled) information
	char				name[MAX_GUI_NAMELEN];
	guiFlags_t			flags;
	guiType_t			type;

	guiShared_t			*shared;
	guiData_t			s;

	char					matName[MAX_QPATH];
	struct refMaterial_t	*mat;

	// Events
	byte				numEvents;
	event_t				*eventList;

	// Children
	gui_t				*owner;							// guiDef
	gui_t				*parent;						// Next window up

	byte				numChildren;
	gui_t				*childList;

	// Dynamic information
	guiData_t			d;

	vec4_t				rect;
	vec3_t				mins, maxs;

	uint32				lastTime;
	uint32				openTime;
	uint32				time;

	bool				mouseEntered;
	bool				mouseExited;

	bool				inited;

	// Event queue
	byte				numQueued;
	event_t				*queueList[MAX_GUI_EVENTS];

	byte				numDefaultQueued;
	evType_t			defaultQueueList[MAX_GUI_EVENTS];
};

/*
=============================================================================

	LOCAL GUI STATE

=============================================================================
*/

struct guiState_t {
	uint32				frameCount;

	gui_t				*inputWindow;

	byte				numLayers;
	gui_t				*openLayers[MAX_GUI_DEPTH];
};

extern guiState_t	cl_guiState;

/*
=============================================================================

	CVARS

=============================================================================
*/

extern cVar_t	*gui_developer;
extern cVar_t	*gui_debugBounds;
extern cVar_t	*gui_debugScale;
extern cVar_t	*gui_mouseFilter;
extern cVar_t	*gui_mouseSensitivity;

/*
=============================================================================

	FUNCTION PROTOTYPES

=============================================================================
*/

//
// gui_cursor.c
//
void		GUI_GenerateBounds (gui_t *gui);
void		GUI_CursorUpdate (gui_t *gui);
void		GUI_AdjustCursor (keyNum_t keyNum);

//
// gui_events.c
//
void		GUI_TriggerEvents (gui_t *gui);
void		GUI_QueueTrigger (gui_t *gui, evType_t trigger);

//
// gui_init.c
//

void		gui_thutdown ();

//
// gui_main.c
//
void		GUI_ResetGUIState (gui_t *gui);
