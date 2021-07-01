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
// cl_local.h
// Primary header for client
//

#ifdef DEDICATED_ONLY
# error You should not be including this file in a dedicated server build
#endif // DEDICATED_ONLY

#include "../renderer/r_public.h"
#include "snd_public.h"
#include "../cgame/cg_shared.h"
#include "cl_keys.h"
#include "gui_public.h"

#define CL_ANTICHEAT	1

#if USE_CURL
# define CL_HTTPDL		1
#endif

extern struct memPool_t	*cl_cGameSysPool;
extern struct memPool_t	*cl_cinSysPool;
extern struct memPool_t	*cl_guiSysPool;
extern struct memPool_t	*cl_soundSysPool;

/*
=============================================================================

	CLIENT STATE

	The clientState_t structure is wiped at every server map change
=============================================================================
*/

struct clientState_t {
	int					timeOutCount;

	int					maxClients;
	int					parseEntities;				// index (not anded off) into cl_parseEntities[]

	userCmd_t			cmd;
	userCmd_t			cmds[CMD_BACKUP];			// each mesage will send several old cmds
	int					cmdTime[CMD_BACKUP];		// time sent, for calculating pings
	int					cmdNum;

	netFrame_t			frame;						// received from server
	int					surpressCount;				// number of messages rate supressed
	netFrame_t			frames[UPDATE_BACKUP];

	refDef_t			refDef;
	// the client maintains its own idea of view angles, which are
	// sent to the server each frame.  It is cleared to 0 upon entering each level.
	// the server sends a delta each frame which is added to the locally
	// tracked view angles to account for standing on rotating objects,
	// and teleport direction changes
	vec3_t				viewAngles;

	//
	// cinematic playback
	//
	cinematic_t			cin;

	//
	// demo recording
	//
	netMsg_t			demoBuffer;
	byte				demoFrame[MAX_SV_MSGLEN];

	//
	// server state information
	//
	bool				attractLoop;				// running the attract loop, any key will menu
	int					serverCount;				// server identification for prespawns
	int					enhancedServer;				// ENHANCED_SERVER_PROTOCOL
	bool				strafeHack;
	char				gameDir[MAX_QPATH];
	int					playerNum;

	char				configStrings[MAX_CFGSTRINGS][MAX_CFGSTRLEN];
	struct sfx_t		*soundCfgStrings[MAX_CS_SOUNDS];
};

extern	clientState_t	cl;

/*
=============================================================================

	MEDIA

=============================================================================
*/

struct clMedia_t {
	bool					initialized;

	// sounds
	struct sfx_t			*talkSfx;

	// images
	struct refMaterial_t	*cinMaterial;
	struct refMaterial_t	*consoleMaterial;
};

extern clMedia_t	clMedia;

void	CL_ImageMediaInit ();
void	CL_SoundMediaInit ();
void	CL_MediaInit ();

void	CL_MediaShutdown ();
void	CL_MediaRestart ();
/*
=============================================================================

	CLIENT STATIC

	Persistant through an arbitrary number of server connections
=============================================================================
*/

#define NET_RETRYDELAY	3000.0f

struct downloadStatic_t {
	FILE				*file;						// file transfer from server
	char				tempName[MAX_OSPATH];
	char				name[MAX_OSPATH];
	int					percent;

#ifdef CL_HTTPDL
	char				httpServer[512];
	char				httpReferer[32];
#endif
};

struct clientStatic_t {
	int					connectCount;
	float				connectTime;				// for connection retransmits

	float				netFrameTime;				// seconds since last net packet frame
	float				trueNetFrameTime;			// un-clamped net frametime that is passed to cgame
	float				refreshFrameTime;			// seconds since last refresh frame
	float				trueRefreshFrameTime;		// un-clamped refresh frametime that is passed to cgame

	int					realTime;					// system realtime

	// Screen rendering information
	bool				refreshPrepped;				// false if on new level or new ref dll
	bool				disableScreen;				// skip rendering until this is true

	// Audio information
	bool				soundPrepped;				// once loading is started, sounds can't play until the frame is valid

	// Connection information
	char				serverMessage[MAX_STRING_TOKENS];
	char				serverName[MAX_OSPATH];		// name of server from original connect
	char				serverNameLast[MAX_OSPATH];
	int					serverProtocol;				// in case we are doing some kind of version hack
	int					protocolMinorVersion;

	netAdr_t			netFrom;
	netMsg_t			netMessage;
	byte				netBuffer[MAX_CL_MSGLEN];

	netChan_t			netChan;
	int					quakePort;					// a 16 bit value that allows quake servers
													// to work around address translating routers
	int					challenge;					// from the server to use for connecting
	bool				forcePacket;				// forces a packet to be sent the next frame

	downloadStatic_t	download;

	//
	// demo recording info must be here, so it isn't cleared on level change
	//
	fileHandle_t		demoFile;
	bool				demoRecording;
	bool				demoWaiting;				// don't record until a non-delta message is received

	//
	// cgame information
	//
	bool				mapLoading;
	bool				mapLoaded;
	bool				acLoading;

	//
	// video settings
	//
	refConfig_t			refConfig;
};

extern clientStatic_t	cls;

/*
=============================================================================

	CVARS

=============================================================================
*/

extern cVar_t	*allow_download;
extern cVar_t	*allow_download_players;
extern cVar_t	*allow_download_models;
extern cVar_t	*allow_download_sounds;
extern cVar_t	*allow_download_maps;

extern cVar_t	*cl_downloadToBase;
extern cVar_t	*cl_upspeed;
extern cVar_t	*cl_forwardspeed;
extern cVar_t	*cl_sidespeed;
extern cVar_t	*cl_yawspeed;
extern cVar_t	*cl_pitchspeed;
extern cVar_t	*cl_shownet;
extern cVar_t	*cl_stereo_separation;
extern cVar_t	*cl_lightlevel;
extern cVar_t	*cl_paused;
extern cVar_t	*cl_timedemo;

extern cVar_t	*freelook;
extern cVar_t	*lookspring;
extern cVar_t	*lookstrafe;
extern cVar_t	*sensitivity;
extern cVar_t	*s_khz;

extern cVar_t	*m_pitch;
extern cVar_t	*m_yaw;
extern cVar_t	*m_forward;
extern cVar_t	*m_side;

extern cVar_t	*cl_timestamp;
extern cVar_t	*con_chatHud;
extern cVar_t	*con_chatHudLines;
extern cVar_t	*con_chatHudPosX;
extern cVar_t	*con_chatHudPosY;
extern cVar_t	*con_chatHudShadow;
extern cVar_t	*con_notifyfade;
extern cVar_t	*con_notifylarge;
extern cVar_t	*con_notifylines;
extern cVar_t	*con_notifytime;
extern cVar_t	*con_alpha;
extern cVar_t	*con_clock;
extern cVar_t	*con_drop;
extern cVar_t	*con_scroll;
extern cVar_t	*m_accel;
extern cVar_t	*r_fontScale;

extern cVar_t	*scr_conspeed;

/*
=============================================================================

	MISCELLANEOUS

=============================================================================
*/

//
// cl_acapi.c
//

#ifdef CL_ANTICHEAT
bool		CL_ACAPI_Init ();
#endif // CL_ANTICHEAT

//
// cl_cgapi.c
//

void		CL_CGModule_LoadMap ();

void		CL_CGModule_UpdateConnectInfo ();

void		CL_CGModule_BeginFrameSequence ();
void		CL_CGModule_NewPacketEntityState (int entNum, entityState_t &state);
void		CL_CGModule_EndFrameSequence ();

void		CL_CGModule_GetEntitySoundOrigin (int entNum, vec3_t origin, vec3_t velocity);

void		CL_CGModule_ParseConfigString (int num, char *str);

void		CL_CGModule_DebugGraph (float value, int color);

void		CL_CGModule_StartServerMessage ();
bool		CL_CGModule_ParseServerMessage (int command);
void		CL_CGModule_EndServerMessage ();

bool		CL_CGModule_Pmove (pMoveNew_t *pMove, float airAcceleration);

void		CL_CGModule_RegisterSounds ();
void		CL_CGModule_RenderView (float stereoSeparation);

void		CL_CGameAPI_Init ();
void		CL_CGameAPI_Shutdown ();

void		CL_CGModule_SetRefConfig ();

void		CL_CGModule_MainMenu ();
void		CL_CGModule_ForceMenuOff ();
void		CL_CGModule_MoveMouse (float mx, float my);
void		CL_CGModule_KeyEvent (keyNum_t keyNum, bool isDown);
bool		CL_CGModule_ParseServerInfo (char *adr, char *info);
bool		CL_CGModule_ParseServerStatus (char *adr, char *info);
void		CL_CGModule_StartSound (vec3_t origin, int entNum, EEntSndChannel entChannel, int soundNum, float volume, float attenuation, float timeOffset);
void		CL_CGModule_RegisterSounds ();

//
// cl_cin.c
//

void		RoQ_Init ();

void		CIN_PlayCinematic (char *name);
void		CIN_DrawCinematic ();
void		CIN_RunCinematic ();
void		CIN_StopCinematic ();
void		CIN_FinishCinematic ();

//
// cl_demo.c
//

void		CL_WriteDemoPlayerstate (netFrame_t *from, netFrame_t *to, netMsg_t *msg);
void		CL_WriteDemoPacketEntities (const netFrame_t *from, netFrame_t *to, netMsg_t *msg);
void		CL_WriteDemoMessageChunk (byte *buffer, int length, bool forceFlush);
void		CL_WriteDemoMessageFull ();

bool		CL_StartDemoRecording (char *name);
void		CL_StopDemoRecording ();

//
// cl_console.c
//

void		CL_ClearNotifyLines (void);
void		CL_ConsoleClose (void);
void		CL_MoveConsoleDisplay (int value);
void		CL_SetConsoleDisplay (bool top);

void		CL_ToggleConsole_f (void);

void		CL_ConsoleCheckResize (void);

void		CL_ConsoleInit (void);

void		CL_DrawConsole (void);

//
// cl_download.c
//

bool		CL_CheckOrDownloadFile (char *fileName);

void		CL_ParseDownload (bool compressed);

void		CL_ResetDownload ();
void		CL_RequestNextDownload ();

#ifdef CL_HTTPDL
void		CL_HTTPDL_Init ();
void		CL_HTTPDL_SetServer (char *url);
void		CL_HTTPDL_CancelDownloads (bool permKill);
bool		CL_HTTPDL_QueueDownload (char *file);
bool		CL_HTTPDL_PendingDownloads ();
void		CL_HTTPDL_Cleanup (bool shutdown);
void		CL_HTTPDL_RunDownloads ();
#endif

//
// cl_input.c
//

bool		CL_GetRunState ();
bool		CL_GetStrafeState ();
bool		CL_GetMLookState ();
void		CL_MoveMouse (int xMove, int yMove);

void		CL_RefreshCmd ();
void		CL_SendCmd ();
void		CL_SendMove (userCmd_t *cmd);

void		CL_InputInit ();

//
// cl_main.c
//

// delta from this if not from a previous frame
extern entityState_t	cl_baseLines[MAX_CS_EDICTS];

// the cl_parseEntities must be large enough to hold UPDATE_BACKUP frames of
// entities, so that when a delta compressed message arives from the server
// it can be un-deltad from the original
extern entityState_t	cl_parseEntities[MAX_PARSE_ENTITIES];

bool		CL_ForwardCmdToServer ();

void		CL_ResetServerCount ();

void		CL_SetRefConfig ();
void		CL_SetState (EClientState state);
void		CL_ClearState ();

void		CL_Disconnect (bool openMenu);

void		__fastcall CL_Frame (int msec);

void		CL_ClientInit ();
void		CL_ClientShutdown (bool error);

//
// cl_parse.c
//

extern char *cl_svcStrings[256];

void		CL_ParseServerMessage ();

//
// cl_screen.c
//

void		SCR_BeginLoadingPlaque ();
void		SCR_EndLoadingPlaque ();

void		SCR_UpdateScreen ();

/*
=============================================================================

	IMPLEMENTATION SPECIFIC

=============================================================================
*/

//
// in_imp.c
//

int			In_MapKey (int wParam, int lParam);
bool		In_GetKeyState (keyNum_t keyNum);

void		IN_Restart_f ();
void		IN_Init ();
void		IN_Shutdown ();

void		IN_Commands ();	// oportunity for devices to stick commands on the script buffer
void		IN_Frame ();
void		IN_Move (userCmd_t *cmd);	// add additional movement on top of the keyboard move cmd

void		IN_Activate (const bool bActive);

//
// vid_imp.c
//

void VID_CheckChanges(refConfig_t *outConfig);
void VID_Init(refConfig_t *outConfig);
void VID_Shutdown();
