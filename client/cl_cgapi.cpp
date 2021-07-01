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
// cl_cgapi.c
// Client-Game API
//

#include "cl_local.h"
#include "../cgame/cg_api.h"

static cgExportAPI_t *cge;

/*
=============================================================================

	CGAME EXPORT FUNCTIONS

=============================================================================
*/

/*
===============
CL_CGModule_LoadMap
===============
*/
void CL_CGModule_LoadMap()
{
	if (!cge)
		Com_Error (ERR_FATAL, "CL_CGModule_LoadMap: CGame not initialized!");

	cls.mapLoading = true;
	cls.mapLoaded = false;

	uint32 startCycles = Sys_Cycles();

	// Update connection info
	CL_CGModule_UpdateConnectInfo ();

	// Begin registration
	GUI_BeginRegistration ();
	R_BeginRegistration ();
	Snd_BeginRegistration ();

	R_GetRefConfig (&cls.refConfig);

	CL_ImageMediaInit ();
	CL_SoundMediaInit ();
	SCR_UpdateScreen ();

	// Load the map and media in CGame
	cge->LoadMap (cl.playerNum, cls.serverProtocol, cls.protocolMinorVersion, cl.attractLoop, cl.strafeHack, &cls.refConfig);

	// Touch before registration ends
	R_MediaInit ();

	// The subsystems can now free unneeded stuff
	GUI_EndRegistration ();
	R_EndRegistration ();
	Snd_EndRegistration ();

	// Clear notify lines
	CL_ClearNotifyLines ();

	// Update the screen
	SCR_UpdateScreen ();

	// Pump message loop
	Sys_SendKeyEvents ();

	// All done!
	cls.refreshPrepped = true;
	cls.mapLoading = false;
	cls.mapLoaded = true;

	Com_Printf (0, "Map loaded in: %6.2fms\n", (Sys_Cycles()-startCycles) * Sys_MSPerCycle());
}


/*
===============
CL_CGModule_UpdateConnectInfo
===============
*/
void CL_CGModule_UpdateConnectInfo ()
{
	if (!cge)
		return;

	// Update download info
	if (cls.download.file) {
		float	size = (float)ftell (cls.download.file);

		cge->UpdateConnectInfo (cls.serverName, cls.serverMessage, cls.connectCount, cls.download.name, cls.download.percent, size, cls.acLoading);
		return;
	}

	// Nothing downloading
	cge->UpdateConnectInfo (cls.serverName, cls.serverMessage, cls.connectCount, NULL, -1, -1, cls.acLoading);
}


/*
===============
CL_CGModule_BeginFrameSequence
===============
*/
void CL_CGModule_BeginFrameSequence ()
{
	if (cge)
		cge->BeginFrameSequence (cl.frame);
}


/*
===============
CL_CGModule_NewPacketEntityState
===============
*/
void CL_CGModule_NewPacketEntityState (int entNum, entityState_t &state)
{
	if (cge)
		cge->NewPacketEntityState (entNum, state);
}


/*
===============
CL_CGModule_EndFrameSequence
===============
*/
void CL_CGModule_EndFrameSequence ()
{
	if (cge)
		cge->EndFrameSequence (cl.frame.numEntities);
}


/*
===============
CL_CGModule_GetEntitySoundOrigin
===============
*/
void CL_CGModule_GetEntitySoundOrigin (int entNum, vec3_t origin, vec3_t velocity)
{
	if (cge)
		cge->GetEntitySoundOrigin (entNum, origin, velocity);
}


/*
===============
CL_CGModule_ParseConfigString
===============
*/
void CL_CGModule_ParseConfigString (int num, char *str)
{
	if (cge)
		cge->ParseConfigString (num, str);
}


/*
===============
CL_CGModule_DebugGraph
===============
*/
void CL_CGModule_DebugGraph (float value, int color)
{
	if (cge)
		cge->DebugGraph (value, color);
}


/*
===============
CL_CGModule_StartServerMessage
===============
*/
void CL_CGModule_StartServerMessage ()
{
	if (cge)
		cge->StartServerMessage ();
}


/*
===============
CL_CGModule_ParseServerMessage
===============
*/
bool CL_CGModule_ParseServerMessage (int command)
{
	if (cge)
		return cge->ParseServerMessage (command);

	return false;
}


/*
===============
CL_CGModule_EndServerMessage
===============
*/
void CL_CGModule_EndServerMessage ()
{
	if (cge)
		cge->EndServerMessage (cls.realTime);
}


/*
===============
CL_CGModule_Pmove
===============
*/
bool CL_CGModule_Pmove (pMoveNew_t *pMove, float airAcceleration)
{
	if (cge) {
		cge->Pmove (pMove, airAcceleration);
		return true;
	}

	return false;
}


/*
===============
CL_CGModule_RegisterSounds
===============
*/
void CL_CGModule_RegisterSounds ()
{
	if (cge)
		cge->RegisterSounds ();
}


/*
===============
CL_CGModule_RenderView
===============
*/
void CL_CGModule_RenderView (float stereoSeparation)
{
	if (cge)
		cge->RenderView (cls.realTime, cls.trueNetFrameTime, cls.trueRefreshFrameTime, stereoSeparation, cls.refreshPrepped);
}


/*
===============
CL_CGModule_SetRefConfig
===============
*/
void CL_CGModule_SetRefConfig ()
{
	if (cge)
		cge->SetRefConfig (&cls.refConfig);
}


/*
===============
CL_CGModule_MainMenu
===============
*/
void CL_CGModule_MainMenu ()
{
	if (cge)
		cge->MainMenu ();
}


/*
===============
CL_CGModule_ForceMenuOff
===============
*/
void CL_CGModule_ForceMenuOff ()
{
	if (cge)
		cge->ForceMenuOff ();
}


/*
===============
CL_CGModule_MoveMouse
===============
*/
void CL_CGModule_MoveMouse (float mx, float my)
{
	if (cge)
		cge->MoveMouse (mx, my);
}


/*
===============
CL_CGModule_KeyEvent
===============
*/
void CL_CGModule_KeyEvent (keyNum_t keyNum, bool isDown)
{
	if (cge)
		cge->KeyEvent (keyNum, isDown);
}


/*
===============
CL_CGModule_ParseServerInfo
===============
*/
bool CL_CGModule_ParseServerInfo (char *adr, char *info)
{
	if (cge)
		return cge->ParseServerInfo (adr, info);
	return false;
}


/*
===============
CL_CGModule_ParseServerStatus
===============
*/
bool CL_CGModule_ParseServerStatus (char *adr, char *info)
{
	if (cge)
		return cge->ParseServerStatus (adr, info);
	return false;
}


/*
===============
CL_CGModule_StartSound
===============
*/
void CL_CGModule_StartSound (vec3_t origin, int entNum, EEntSndChannel entChannel, int soundNum, float volume, float attenuation, float timeOffset)
{
	if (cge)
		cge->StartSound (origin, entNum, entChannel, soundNum, volume, attenuation, timeOffset);
}

/*
=============================================================================

	CGAME IMPORT API

=============================================================================
*/

/*
==================
CGI_Cmd_AddCommand
==================
*/
static conCmd_t *CGI_Cmd_AddCommand(const char *name, const int flags, void (*function) (), const char *description)
{
	return Cmd_AddCommand(name, CMD_CGAME|flags, function, description);
}


/*
==================
CGI_Com_Error
==================
*/
static void CGI_Com_Error (EComErrorType code, char *text)
{
	Com_Error (code, "%s", text);
}


/*
==================
CGI_Com_Printf
==================
*/
static void CGI_Com_Printf (comPrint_t flags, char *text)
{
	Com_ConPrint (flags, text);
}


/*
==================
CGI_Com_DevPrintf
==================
*/
static void CGI_Com_DevPrintf (comPrint_t flags, char *text)
{
	if (!cg_developer->intVal && !developer->intVal)
		return;

	Com_ConPrint (flags, text);
}


/*
===============
CGI_NET_GetCurrentUserCmdNum
===============
*/
static int CGI_NET_GetCurrentUserCmdNum ()
{
	return cl.cmdNum;
}


/*
===============
CGI_NET_GetPacketDropCount
===============
*/
static int CGI_NET_GetPacketDropCount ()
{
	return cls.netChan.dropped;
}


/*
===============
CGI_NET_GetRateDropCount
===============
*/
static int CGI_NET_GetRateDropCount ()
{
	return cl.surpressCount;
}


/*
===============
CGI_NET_GetSequenceState
===============
*/
static void CGI_NET_GetSequenceState (int *outgoingSequence, int *incomingAcknowledged)
{
	if (outgoingSequence)
		*outgoingSequence = cls.netChan.outgoingSequence;
	if (incomingAcknowledged)
		*incomingAcknowledged = cls.netChan.incomingAcknowledged;
}


/*
===============
CGI_NET_GetUserCmd
===============
*/
static void CGI_NET_GetUserCmd (int frame, userCmd_t *cmd)
{
	if (cmd)
		*cmd = cl.cmds[frame & CMD_MASK];
}


/*
===============
CGI_NET_GetUserCmdTime
===============
*/
static int CGI_NET_GetUserCmdTime (int frame)
{
	return cl.cmdTime[frame & CMD_MASK];
}

// ==========================================================================

/*
===============
CGI_GetConfigString
===============
*/
static void CGI_GetConfigString (int i, char *str, int size)
{
	if (i < 0 || i >= MAX_CFGSTRINGS) {
		Com_Printf (PRNT_ERROR, "CGI_GetConfigString: i > MAX_CFGSTRINGS");
		return;
	}
	if (!str || size <= 0) {
		Com_Printf (PRNT_ERROR, "CGI_GetConfigString: NULL string");
		return;
	}

	strncpy (str, cl.configStrings[i], size);
}

// ==========================================================================

static int CGI_MSG_ReadChar ()			{ return cls.netMessage.ReadChar (); }
static int CGI_MSG_ReadByte ()			{ return cls.netMessage.ReadByte (); }
static int CGI_MSG_ReadShort ()			{ return cls.netMessage.ReadShort (); }
static int CGI_MSG_ReadLong ()			{ return cls.netMessage.ReadLong (); }
static float CGI_MSG_ReadFloat ()		{ return cls.netMessage.ReadFloat (); }
static void CGI_MSG_ReadDir (vec3_t dir)	{ cls.netMessage.ReadDir (dir); }
static void CGI_MSG_ReadPos (vec3_t pos)	{ cls.netMessage.ReadPos (pos); }
static char *CGI_MSG_ReadString ()		{ return cls.netMessage.ReadString (); }

// ==========================================================================

/*
===============
CGI_R_RenderScene
===============
*/
static void CGI_R_RenderScene (refDef_t *rd)
{
	cl.refDef = *rd;
	R_RenderScene (rd);
}

// ==========================================================================

/*
===============
CGI_Alloc
===============
*/
static void *CGI_Alloc (size_t size, const int tagNum, const char *fileName, const int fileLine)
{
	return _Mem_Alloc (size, cl_cGameSysPool, tagNum, fileName, fileLine);
}


/*
===============
CGI_ChangeTag
===============
*/
uint32 CGI_ChangeTag (const int tagFrom, const int tagTo)
{
	return _Mem_ChangeTag (cl_cGameSysPool, tagFrom, tagTo);
}


/*
===============
CGI_Free
===============
*/
static uint32 CGI_Free (const void *ptr, const char *fileName, const int fileLine)
{
	return _Mem_Free (ptr, fileName, fileLine);
}


/*
===============
CGI_FreeTag
===============
*/
static uint32 CGI_FreeTag (const int tagNum, const char *fileName, const int fileLine)
{
	return _Mem_FreeTag (cl_cGameSysPool, tagNum, fileName, fileLine);
}


/*
===============
CGI_StrDup
===============
*/
static char *CGI_StrDup (const char *in, const int tagNum, const char *fileName, const int fileLine)
{
	return _Mem_PoolStrDup (in, cl_cGameSysPool, tagNum, fileName, fileLine);
}


/*
===============
CGI_TagSize
===============
*/
static uint32 CGI_TagSize (const int tagNum)
{
	return _Mem_TagSize (cl_cGameSysPool, tagNum);
}

// ==========================================================================

/*
===============
CL_CGameAPI_Init
===============
*/
void CL_CGameAPI_Init()
{
	cgImportAPI_t cgi;

	// Shutdown if already active
	CL_CGameAPI_Shutdown ();

	CGI_Com_DevPrintf (0, "\n--------- CGame Initialization ---------\n");

	// Initialize pointers
	cgi.Cbuf_AddText				= Cbuf_AddText;
	cgi.Cbuf_Execute				= Cbuf_Execute;
	cgi.Cbuf_ExecuteString			= Cmd_ExecuteString;
	cgi.Cbuf_InsertText				= Cbuf_InsertText;

	cgi.CL_ForwardCmdToServer		= CL_ForwardCmdToServer;
	cgi.CL_ResetServerCount			= CL_ResetServerCount;

	cgi.CM_BoxTrace					= CM_BoxTrace;
	cgi.CM_HeadnodeForBox			= CM_HeadnodeForBox;
	cgi.CM_InlineModel				= CM_InlineModel;
	cgi.CM_InlineModelBounds		= CM_InlineModelBounds;
	cgi.CM_InlineModelHeadNode		= CM_InlineModelHeadNode;
	cgi.CM_PointContents			= CM_PointContents;
	cgi.CM_Trace					= CM_Trace;
	cgi.CM_TransformedBoxTrace		= CM_TransformedBoxTrace;
	cgi.CM_TransformedPointContents = CM_TransformedPointContents;
	cgi.CM_PointLeafnum				= CM_PointLeafnum;
	cgi.CM_LeafCluster				= CM_LeafCluster;
	cgi.CM_LeafArea					= CM_LeafArea;
	cgi.CM_ClusterPVS				= CM_ClusterPVS;
	cgi.CM_BoxLeafnums				= CM_BoxLeafnums;
	cgi.CM_NumClusters				= CM_NumClusters;
	cgi.CM_HeadnodeVisible			= CM_HeadnodeVisible;

	cgi.Cmd_AddCommand				= CGI_Cmd_AddCommand;
	cgi.Cmd_RemoveCommand			= Cmd_RemoveCommand;

	cgi.Cmd_TokenizeString			= Cmd_TokenizeString;
	cgi.Cmd_Argc					= Cmd_Argc;
	cgi.Cmd_ArgsOffset				= Cmd_ArgsOffset;
	cgi.Cmd_Argv					= Cmd_Argv;

	cgi.Com_Error					= CGI_Com_Error;
	cgi.Com_Printf					= CGI_Com_Printf;
	cgi.Com_DevPrintf				= CGI_Com_DevPrintf;
	cgi.Com_ClientState				= Com_ClientState;
	cgi.Com_ServerState				= Com_ServerState;

	cgi.Cvar_Register				= Cvar_Register;
	cgi.Cvar_Exists					= Cvar_Exists;

	cgi.Cvar_GetIntegerValue		= Cvar_GetIntegerValue;
	cgi.Cvar_GetStringValue			= Cvar_GetStringValue;
	cgi.Cvar_GetFloatValue			= Cvar_GetFloatValue;

	cgi.Cvar_VariableSet			= Cvar_VariableSet;
	cgi.Cvar_VariableSetValue		= Cvar_VariableSetValue;
	cgi.Cvar_VariableReset			= Cvar_VariableReset;

	cgi.Cvar_Set					= Cvar_Set;
	cgi.Cvar_SetValue				= Cvar_SetValue;
	cgi.Cvar_Reset					= Cvar_Reset;

	cgi.FS_CreatePath				= FS_CreatePath;
	cgi.FS_Gamedir					= FS_Gamedir;
	cgi.FS_FileExists				= FS_FileExists;
	cgi.FS_FindFiles				= FS_FindFiles;
	cgi.FS_LoadFile					= FS_LoadFile;
	cgi.FS_FreeFile					= _FS_FreeFile;
	cgi.FS_NextPath					= FS_NextPath;
	cgi.FS_Read						= FS_Read;
	cgi.FS_Write					= FS_Write;
	cgi.FS_Seek						= FS_Seek;
	cgi.FS_OpenFile					= FS_OpenFile;
	cgi.FS_CloseFile				= FS_CloseFile;

	cgi.GetConfigString				= CGI_GetConfigString;

	cgi.GUI_RegisterGUI				= GUI_RegisterGUI;
	cgi.GUI_OpenGUI					= GUI_OpenGUI;
	cgi.GUI_CloseGUI				= GUI_CloseGUI;
	cgi.GUI_CloseAllGUIs			= GUI_CloseAllGUIs;
	cgi.GUI_NamedGlobalEvent		= GUI_NamedGlobalEvent;
	cgi.GUI_NamedGUIEvent			= GUI_NamedGUIEvent;

	cgi.GUIVar_Register				= GUIVar_Register;
	cgi.GUIVar_GetFloatValue		= GUIVar_GetFloatValue;
	cgi.GUIVar_GetStrValue			= GUIVar_GetStrValue;
	cgi.GUIVar_GetVecValue			= GUIVar_GetVecValue;
	cgi.GUIVar_SetFloatValue		= GUIVar_SetFloatValue;
	cgi.GUIVar_SetStrValue			= GUIVar_SetStrValue;
	cgi.GUIVar_SetVecValue			= GUIVar_SetVecValue;

	cgi.Key_ClearStates				= Key_ClearStates;
	cgi.Key_GetDest					= Key_GetDest;
	cgi.Key_GetBindingBuf			= Key_GetBindingBuf;
	cgi.Key_IsDown					= Key_IsDown;
	cgi.Key_KeynumToString			= Key_KeynumToString;
	cgi.Key_SetBinding				= Key_SetBinding;
	cgi.Key_SetDest					= Key_SetDest;
	cgi.Key_InsertOn				= Key_InsertOn;
	cgi.Key_CapslockOn				= Key_CapslockOn;
	cgi.Key_ShiftDown				= Key_ShiftDown;

	cgi.Mem_Alloc					= CGI_Alloc;
	cgi.Mem_Free					= CGI_Free;
	cgi.Mem_FreeTag					= CGI_FreeTag;
	cgi.Mem_StrDup					= CGI_StrDup;
	cgi.Mem_TagSize					= CGI_TagSize;
	cgi.Mem_ChangeTag				= CGI_ChangeTag;

	cgi.MSG_ReadChar				= CGI_MSG_ReadChar;
	cgi.MSG_ReadByte				= CGI_MSG_ReadByte;
	cgi.MSG_ReadShort				= CGI_MSG_ReadShort;
	cgi.MSG_ReadLong				= CGI_MSG_ReadLong;
	cgi.MSG_ReadFloat				= CGI_MSG_ReadFloat;
	cgi.MSG_ReadDir					= CGI_MSG_ReadDir;
	cgi.MSG_ReadPos					= CGI_MSG_ReadPos;
	cgi.MSG_ReadString				= CGI_MSG_ReadString;

	cgi.NET_GetCurrentUserCmdNum	= CGI_NET_GetCurrentUserCmdNum;
	cgi.NET_GetPacketDropCount		= CGI_NET_GetPacketDropCount;
	cgi.NET_GetRateDropCount		= CGI_NET_GetRateDropCount;
	cgi.NET_GetSequenceState		= CGI_NET_GetSequenceState;
	cgi.NET_GetUserCmd				= CGI_NET_GetUserCmd;
	cgi.NET_GetUserCmdTime			= CGI_NET_GetUserCmdTime;

	cgi.R_AddDecal					= R_AddDecal;
	cgi.R_AddEntity					= R_AddEntity;
	cgi.R_AddPoly					= R_AddPoly;
	cgi.R_AddLight					= R_AddLight;
	cgi.R_AddLightStyle				= R_AddLightStyle;

	cgi.R_ClearScene				= R_ClearScene;

	cgi.R_RegisterFont				= R_RegisterFont;
	cgi.R_GetFontDimensions			= R_GetFontDimensions;
	cgi.R_DrawChar					= R_DrawChar;
	cgi.R_DrawString				= R_DrawString;
	cgi.R_DrawStringLen				= R_DrawStringLen;

	cgi.R_DrawPic					= R_DrawPic;
	cgi.R_DrawFill					= R_DrawFill;

	cgi.R_GetRefConfig				= R_GetRefConfig;
	cgi.R_GetImageSize				= R_GetImageSize;

	cgi.R_CreateDecal				= R_CreateDecal;
	cgi.R_FreeDecal					= R_FreeDecal;

	cgi.R_RegisterMap				= R_RegisterMap;
	cgi.R_RegisterModel				= R_RegisterModel;
	cgi.R_ModelBounds				= R_ModelBounds;
	cgi.R_GetModelAnimation			= R_GetModelAnimation;

	cgi.R_UpdateScreen				= SCR_UpdateScreen;
	cgi.R_RenderScene				= CGI_R_RenderScene;
	cgi.R_BeginFrame				= R_BeginFrame;
	cgi.R_EndFrame					= R_EndFrame;

	cgi.R_LightPoint				= R_LightPoint;
	cgi.R_TransformVectorToScreen	= R_TransformVectorToScreen;
	cgi.R_SetSky					= R_SetSky;

	cgi.R_RegisterPic				= R_RegisterPic;
	cgi.R_RegisterPoly				= R_RegisterPoly;
	cgi.R_RegisterSkin				= R_RegisterSkin;

	cgi.Snd_RegisterSound			= Snd_RegisterSound;
	cgi.Snd_StartLocalSound			= Snd_StartLocalSound;
	cgi.Snd_StartSound				= Snd_StartSound;
	cgi.Snd_Update					= Snd_Update;

	cgi.Sys_FindClose				= Sys_FindClose;
	cgi.Sys_FindFirst				= Sys_FindFirst;
	cgi.Sys_GetClipboardData		= Sys_GetClipboardData;
	cgi.Sys_Milliseconds			= Sys_Milliseconds;
	cgi.Sys_SendKeyEvents			= Sys_SendKeyEvents;
	cgi.Sys_Cycles					= Sys_Cycles;
	cgi.Sys_MSPerCycle				= Sys_MSPerCycle;

	cgi.Lua_CreateLuaState			= Lua_CreateLuaState;
	cgi.Lua_ScriptFromState			= Lua_ScriptFromState;
	cgi.Lua_RegisterFunctions		= Lua_RegisterFunctions;
	cgi.Lua_RegisterGlobals			= Lua_RegisterGlobals;
	cgi.Lua_DestroyLuaState			= Lua_DestroyLuaState;

	// Get the cgame api
	CGI_Com_DevPrintf (0, "LoadLibrary()\n");
	cge = (cgExportAPI_t *) Sys_LoadLibrary (LIB_CGAME, &cgi);
	if (!cge)
		Com_Error (ERR_FATAL, "CL_CGameAPI_Init: Find/load of CGame library failed!");

	// Check the api version
	if (cge->apiVersion != CGAME_APIVERSION) {
		uint32	temp;

		temp = cge->apiVersion;
		cge = NULL;
		Sys_UnloadLibrary (LIB_CGAME);
		Com_Error (ERR_FATAL, "CL_CGameAPI_Init: incompatible apiVersion (%i != %i), your installation may be bad!", temp, CGAME_APIVERSION);
	}

	// Check for exports
	if (!cge->Init || !cge->Shutdown
	|| !cge->UpdateConnectInfo || !cge->LoadMap || !cge->DebugGraph || !cge->BeginFrameSequence
	|| !cge->EndFrameSequence || !cge->NewPacketEntityState || !cge->GetEntitySoundOrigin || !cge->ParseConfigString
	|| !cge->StartServerMessage || !cge->ParseServerMessage || !cge->EndServerMessage || !cge->StartSound
	|| !cge->Pmove || !cge->RegisterSounds || !cge->RenderView || !cge->SetRefConfig
	|| !cge->MainMenu || !cge->ForceMenuOff || !cge->MoveMouse || !cge->KeyEvent
	|| !cge->ParseServerInfo || !cge->ParseServerStatus)
		Com_Error (ERR_FATAL, "CL_CGameAPI_Init: Failed to find all CGame exports!");

	// Call to init
	CGI_Com_DevPrintf (0, "cgame->Init()\n");
	cge->Init ();

	CGI_Com_DevPrintf (0, "----------------------------------------\n");
}


/*
===============
CL_CGameAPI_Shutdown
===============
*/
void CL_CGameAPI_Shutdown ()
{
	if (!cge)
		return;

	CGI_Com_DevPrintf (0, "\n------------ CGame Shutdown ------------\n");

	// Tell the module to shutdown locally and unload dll
	CGI_Com_DevPrintf (0, "cgame->Shutdown()\n");
	cge->Shutdown ();

	CGI_Com_DevPrintf (0, "UnloadLibrary()\n");
	Sys_UnloadLibrary (LIB_CGAME);
	cls.mapLoaded = false;
	cge = NULL;

	// Notify of memory leaks
	uint32 size = Mem_PoolSize(cl_cGameSysPool);
	if (size > 0)
		Com_Printf(PRNT_WARNING, "WARNING: CGame memory leak (%u bytes)\n", size);

	// Remove CGame-created console command functions
	uint32 Num = Cmd_RemoveByFlag(CMD_CGAME);
	if (Num != 0)
		Com_Printf(PRNT_WARNING, "%i commands were not properly removed during CGame shutdown, forcing removal.\n", Num);

	CGI_Com_DevPrintf (0, "----------------------------------------\n");
}
