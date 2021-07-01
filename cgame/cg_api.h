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
// cg_api.h
//

#ifndef __CGAMEAPI_H__
#define __CGAMEAPI_H__

#define CGAME_APIVERSION	034		// Just the engine version number

struct cgExportAPI_t
{
	uint32		apiVersion;

	void		(*Init) ();
	void		(*Shutdown) ();

	void		(*UpdateConnectInfo) (char *serverName, char *serverMessage, int connectCount, char *dlFileName, int dlPercent, float bytesDownloaded, bool acLoading);
	void		(*LoadMap) (int playerNum, int serverProtocol, int protocolMinorVersion, bool attractLoop, bool strafeHack, refConfig_t *inConfig);

	void		(*DebugGraph) (float value, int color);

	void		(*BeginFrameSequence) (netFrame_t frame);
	void		(*EndFrameSequence) (int numEntities);
	void		(*NewPacketEntityState) (int entNum, entityState_t &state);
	// the sound code makes callbacks to the client for entitiy position
	// information, so entities can be dynamically re-spatialized
	void		(*GetEntitySoundOrigin) (int entNum, vec3_t origin, vec3_t velocity);

	void		(*ParseConfigString) (int num, char *str);

	void		(*StartServerMessage) ();
	bool		(*ParseServerMessage) (int command);
	void		(*EndServerMessage) (int realTime);

	void		(*StartSound) (vec3_t origin, int entNum, EEntSndChannel entChannel, int soundNum, float volume, float attenuation, float timeOffset);

	void		(*Pmove) (pMoveNew_t *pmove, float airAcceleration);

	void		(*RegisterSounds) ();

	void		(*RenderView) (int realTime, float netFrameTime, float refreshFrameTime, float stereoSeparation, bool refreshPrepped);

	void		(*SetRefConfig) (refConfig_t *inConfig);

	void		(*MainMenu) ();
	void		(*ForceMenuOff) ();

	void		(*MoveMouse) (float x, float y);
	void		(*KeyEvent) (keyNum_t keyNum, bool isDown);

	bool		(*ParseServerInfo) (char *adr, char *info);
	bool		(*ParseServerStatus) (char *adr, char *info);
};

struct cgImportAPI_t
{
	void		(*Cbuf_AddText) (char *text);
	void		(*Cbuf_Execute) ();
	void		(*Cbuf_ExecuteString) (char *text);
	void		(*Cbuf_InsertText) (char *text);

	bool		(*CL_ForwardCmdToServer) ();
	void		(*CL_ResetServerCount) ();

	cmTrace_t	(*CM_BoxTrace) (vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, int headNode, int brushMask);
	int			(*CM_HeadnodeForBox) (vec3_t mins, vec3_t maxs);
	struct cmBspModel_t	*(*CM_InlineModel) (char *name);
	void		(*CM_InlineModelBounds) (struct cmBspModel_t *model, vec3_t mins, vec3_t maxs);
	int			(*CM_InlineModelHeadNode) (struct cmBspModel_t *model);
	int			(*CM_PointContents) (vec3_t point, int headNode);
	cmTrace_t	(*CM_Trace) (vec3_t start, vec3_t end, float size, int contentMask);
	void		(*CM_TransformedBoxTrace) (cmTrace_t *out, vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, int headNode, int brushMask, vec3_t origin, vec3_t angles);
	int			(*CM_TransformedPointContents) (vec3_t point, int headNode, vec3_t origin, vec3_t angles);
	int			(*CM_PointLeafnum) (vec3_t p);
	int			(*CM_LeafCluster) (int leafnum);
	int			(*CM_LeafArea) (int leafnum);
	byte		*(*CM_ClusterPVS) (int cluster);
	int			(*CM_BoxLeafnums) (vec3_t mins, vec3_t maxs, int *list, int listSize, int *topNode);
	int			(*CM_NumClusters) ();
	bool		(*CM_HeadnodeVisible) (int nodeNum, byte *visBits);

	conCmd_t	*(*Cmd_AddCommand)(const char *name, const int flags, void (*function) (), const char *description);
	void		(*Cmd_RemoveCommand)(conCmd_t *command);

	void		(*Cmd_TokenizeString) (char *text, bool macroExpand);
	int			(*Cmd_Argc) ();
	char		*(*Cmd_ArgsOffset) (int start);
	char		*(*Cmd_Argv) (int arg);

	void		(*Com_Error) (EComErrorType code, char *text);
	void		(*Com_Printf) (comPrint_t flags, char *text);
	void		(*Com_DevPrintf) (comPrint_t flags, char *text);
	EClientState	(*Com_ClientState) ();
	EServerState	(*Com_ServerState) ();

	cVar_t		*(*Cvar_Register) (const char *name, const char *defaultValue, const int flags);
	cVar_t		*(*Cvar_Exists) (const char *varName);

	int			(*Cvar_GetIntegerValue) (const char *varName);
	char		*(*Cvar_GetStringValue) (const char *varName);
	float		(*Cvar_GetFloatValue) (const char *varName);

	cVar_t		*(*Cvar_VariableSet) (cVar_t *cvar, const char *newValue, const bool bForce);
	cVar_t		*(*Cvar_VariableSetValue) (cVar_t *cvar, const float newValue, const bool bForce);
	cVar_t		*(*Cvar_VariableReset) (cVar_t *cvar, const bool bForce);

	cVar_t		*(*Cvar_Set) (const char *name, const char *newValue, const bool bForce);
	cVar_t		*(*Cvar_SetValue) (const char *name, const float newValue, const bool bForce);
	cVar_t		*(*Cvar_Reset) (const char *varName, const bool bForce);

	void		(*FS_CreatePath) (char *path);
	const char	*(*FS_Gamedir) ();
	int			(*FS_FileExists) (const char *path);
	TList<String>(*FS_FindFiles) (const char *path, const char *filter, const char *extension, const bool addGameDir, const bool recurse);
	int			(*FS_LoadFile) (const char *path, void **buffer, const bool terminate);
	void		(*FS_FreeFile) (void *buffer, const char *fileName, const int fileLine);
	char		*(*FS_NextPath) (char *prevPath);
	int			(*FS_Read) (void *buffer, const int len, fileHandle_t fileNum);
	int			(*FS_Write) (void *buffer, const int size, fileHandle_t fileNum);
	void		(*FS_Seek) (fileHandle_t fileNum, const int offset, const EFSSeekOrigin seekOrigin);
	int			(*FS_OpenFile) (const char *fileName, fileHandle_t *fileNum, const EFSOpenMode openMode);
	void		(*FS_CloseFile) (fileHandle_t fileNum);

	void		(*GetConfigString) (int i, char *str, int size);

	struct gui_t *(*GUI_RegisterGUI) (char *name);
	void		(*GUI_OpenGUI) (struct gui_t *gui);
	void		(*GUI_CloseGUI) (struct gui_t *gui);
	void		(*GUI_CloseAllGUIs) ();
	void		(*GUI_NamedGlobalEvent) (char *name);
	void		(*GUI_NamedGUIEvent) (struct gui_t *gui, char *name);

	struct guiVar_t	*(*GUIVar_Register) (char *name, guiVarType_t type);
	bool		(*GUIVar_GetFloatValue) (struct guiVar_t *variable, float *dest);
	bool		(*GUIVar_GetStrValue) (struct guiVar_t *variable, char *dest, size_t size);
	bool		(*GUIVar_GetVecValue) (struct guiVar_t *variable, vec4_t dest);
	void		(*GUIVar_SetFloatValue) (struct guiVar_t *variable, float value);
	void		(*GUIVar_SetStrValue) (struct guiVar_t *variable, char *value);
	void		(*GUIVar_SetVecValue) (struct guiVar_t *variable, vec4_t value);

	void		(*Key_ClearStates) ();
	char		*(*Key_GetBindingBuf) (keyNum_t keyNum);
	EKeyDest	(*Key_GetDest)();
	bool		(*Key_IsDown)(keyNum_t keyNum);
	char		*(*Key_KeynumToString) (keyNum_t keyNum);
	void		(*Key_SetBinding) (keyNum_t keyNum, char *binding);
	void		(*Key_SetDest) (const EKeyDest keyDest);
	bool		(*Key_InsertOn) ();
	bool		(*Key_CapslockOn) ();
	bool		(*Key_ShiftDown) ();

	void		*(*Mem_Alloc) (size_t size, const int tagNum, const char *fileName, const int fileLine);
	uint32		(*Mem_Free) (const void *ptr, const char *fileName, const int fileLine);
	uint32		(*Mem_FreeTag) (const int tagNum, const char *fileName, const int fileLine);
	char		*(*Mem_StrDup) (const char *in, const int tagNum, const char *fileName, const int fileLine);
	uint32		(*Mem_TagSize) (const int tagNum);
	uint32		(*Mem_ChangeTag) (const int tagFrom, const int tagTo);

	int			(*MSG_ReadChar) ();
	int			(*MSG_ReadByte) ();
	int			(*MSG_ReadShort) ();
	int			(*MSG_ReadLong) ();
	float		(*MSG_ReadFloat) ();
	void		(*MSG_ReadDir) (vec3_t dir);
	void		(*MSG_ReadPos) (vec3_t pos);
	char		*(*MSG_ReadString) ();

	int			(*NET_GetCurrentUserCmdNum) ();
	int			(*NET_GetPacketDropCount) ();
	int			(*NET_GetRateDropCount) ();
	void		(*NET_GetSequenceState) (int *outgoingSequence, int *incomingAcknowledged);
	void		(*NET_GetUserCmd) (int frame, userCmd_t *cmd);
	int			(*NET_GetUserCmdTime) (int frame);

	void		(*R_AddDecal) (refDecal_t *decal, const colorb &color, const float materialTime);
	void		(*R_AddEntity) (refEntity_t *ent);
	void		(*R_AddPoly) (refPoly_t *poly);
	void		(*R_AddLight) (vec3_t org, float intensity, float r, float g, float b);
	void		(*R_AddLightStyle) (int style, float r, float g, float b);

	void		(*R_ClearScene) ();

	struct refFont_t *(*R_RegisterFont) (char *name);
	void		(*R_GetFontDimensions) (struct refFont_t *font, float xScale, float yScale, uint32 flags, vec2_t dest);
	void		(*R_DrawChar) (struct refFont_t *font, float x, float y, float xScale, float yScale, uint32 flags, int num, const colorb &color);
	int			(*R_DrawString) (struct refFont_t *font, float x, float y, float xScale, float yScale, uint32 flags, char *string, const colorb &color);
	int			(*R_DrawStringLen) (struct refFont_t *font, float x, float y, float xScale, float yScale, uint32 flags, char *string, int len, const colorb &color);

	void		(*R_DrawPic) (struct refMaterial_t *mat, const float matTime, const QuadVertices &vertices, const colorb &color);
	void		(*R_DrawFill) (const float x, const float y, const int w, const int h, const colorb &color);

	void		(*R_GetRefConfig) (refConfig_t *outConfig);
	void		(*R_GetImageSize) (struct refMaterial_t *mat, int *width, int *height);

	bool		(*R_CreateDecal) (refDecal_t *d, struct refMaterial_t *material, vec4_t subUVs, vec3_t origin, vec3_t direction, float angle, float size);
	bool		(*R_FreeDecal) (refDecal_t *d);

	void		(*R_RegisterMap) (const char *mapName);
	struct refModel_t *(*R_RegisterModel) (const char *name);
	void		(*R_ModelBounds) (struct refModel_t *model, vec3_t mins, vec3_t maxs);
	refAliasAnimation_t	*(*R_GetModelAnimation) (struct refModel_t *model, int index);

	void		(*R_UpdateScreen) ();
	void		(*R_RenderScene) (refDef_t *rd);
	void		(*R_BeginFrame) (const float cameraSeparation);
	void		(*R_EndFrame) ();

	struct refMaterial_t *(*R_RegisterPic) (const char *name);
	struct refMaterial_t *(*R_RegisterPoly) (const char *name);
	struct refMaterial_t *(*R_RegisterSkin) (const char *name);

	void		(*R_LightPoint) (vec3_t point, vec3_t light);
	void		(*R_TransformVectorToScreen) (refDef_t *rd, vec3_t in, vec2_t out);
	void		(*R_SetSky) (char *name, float rotate, vec3_t axis);

	struct sfx_t *(*Snd_RegisterSound) (char *sample);
	void		(*Snd_StartSound) (vec3_t origin, int entNum, EEntSndChannel entChannel, struct sfx_t *sfx, float volume, float attenuation, float timeOffset);
	void		(*Snd_StartLocalSound) (struct sfx_t *sfx, float volume);
	void		(*Snd_Update) (refDef_t *rd);

	void		(*Sys_FindClose) ();
	char		*(*Sys_FindFirst) (char *path, uint32 mustHave, uint32 cantHave);
	char		*(*Sys_GetClipboardData) ();
	int			(*Sys_Milliseconds) ();
	void		(*Sys_SendKeyEvents) ();
	uint32		(*Sys_Cycles) ();
	double		(*Sys_MSPerCycle) ();

	Script		*(*Lua_CreateLuaState) (const char *fileName);
	Script		*(*Lua_ScriptFromState) (lua_State *state);
	void		(*Lua_RegisterFunctions) (Script *state, const ScriptFunctionTable *list);
	void		(*Lua_RegisterGlobals) (Script *state, const ScriptGlobalTable *list);
	void		(*Lua_DestroyLuaState) (Script *state);
};

typedef cgExportAPI_t (*GetCGameAPI_t) (cgImportAPI_t);

#endif // __CGAMEAPI_H__
