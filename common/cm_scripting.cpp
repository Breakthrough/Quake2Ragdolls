#include "common.h"

memPool_t *com_luaPool;

static void *LuaAlloc (void *ud, void *ptr, size_t osize, size_t nsize)
{
	(void)ud;  (void)osize;  /* not used */

	if (osize == 0 && nsize == 0)
		return null;
	else if (nsize == 0)
	{
		Mem_Free(ptr);
		return null;
	}
	else if (nsize != 0 && osize == 0)
		return Mem_PoolAlloc(nsize, com_luaPool, 0);
	else
		return Mem_ReAlloc(ptr, nsize);
}

TList<Script*> l_openStates;

int Lua_Sys_Com_Printf (lua_State *s)
{
	auto script = Lua_ScriptFromState(s);

	Com_Printf(0, "%s", script->ToString(1));

	return 0;
}

int Lua_Sys_Milliseconds (lua_State *s)
{
	auto script = Lua_ScriptFromState(s);

	script->Push(Sys_Milliseconds());

	return 1;
}

int Lua_Panic (lua_State *s)
{
	auto script = Lua_ScriptFromState(s);

	return 1;
}

void Lua_Init()
{
	com_luaPool = Mem_CreatePool("Client: Lua Pool");
}

void Lua_Shutdown()
{
	for (uint32 i = 0; i < l_openStates.Count(); ++i)
	{
		lua_close(l_openStates[i]->state);
		delete l_openStates[i];
	}

	l_openStates.Clear();

	Mem_FreePool(com_luaPool);
}

Script *Lua_ScriptFromState (lua_State *state)
{
	Script *scr = null;

	for (uint32 i = 0; i < l_openStates.Count(); ++i)
	{
		if (l_openStates[i]->state == state)
		{
			scr = l_openStates[i];
			break;
		}
	}

	return scr;
}

static const ScriptFunctionTable dblib[] = {
	{"Com_Printf", Lua_Sys_Com_Printf},
	{"Sys_Milliseconds", Lua_Sys_Milliseconds},
	{NULL, NULL}
};


LUALIB_API int luaopen_scripting (Script *L) {
	Lua_RegisterFunctions(L, dblib);
	return 1;
}

Script *Lua_CreateLuaState (const char *fileName)
{
	if (!FS_FileExists(fileName))
		return null;

	Script *comState = new Script();

	lua_State *state = lua_newstate(LuaAlloc, null);
	comState->state = state;
	lua_setallocf(state, LuaAlloc, null);

	lua_atpanic(state, Lua_Panic);

	char *buffer;
	int ret = FS_LoadFile(fileName, (void**)&buffer, false);

	if (ret == -1) // needed?
		return null;

	if (luaL_loadbuffer(state, buffer, ret, Com_SkipPath((char *)fileName)) != 0)
		Com_Printf (PRNT_ERROR, "Lua error: %s\n", lua_tostring(state, -1));
	
	Q_snprintfz(comState->name, sizeof(comState->name), "%s", Com_SkipPath((char *)fileName));
	if (lua_pcall(state, 0, 0, 0) != 0)
		Com_Printf (PRNT_ERROR, "Lua error: %s\n", lua_tostring(state, -1));

	luaopen_math(state);
	luaopen_string(state);
	luaopen_debug(state);
	luaopen_bit(state);

	luaopen_scripting(comState);

	l_openStates.Add(comState);

	return comState;
}

void RegisterFunction (Script *state, const char *functionName, lua_CFunction func)
{
	state->Push(func);
	state->SetGlobal(functionName);
}

void Lua_RegisterFunctions (Script *state, const ScriptFunctionTable *list)
{
	for (var x = &list[0]; x->Name != null; x++)
		lua_register(state->state, x->Name, x->Function);
}

void Lua_RegisterGlobals (Script *state, const ScriptGlobalTable *list)
{
	for (var x = &list[0]; x->Name != null; x++)
	{
		switch (x->Type)
		{
		case 0:
			state->Push(x->Value.String_Value);
			break;
		case 1:
			state->Push(x->Value.Int_Value);
			break;
		case 2:
			state->Push(x->Value.Float_Value);
			break;
		case 3:
			throw Exception();
			break;
		}

		state->SetGlobal(x->Name);
	}
}

void Lua_DestroyLuaState (Script *state)
{
	l_openStates.Remove(state);
	lua_close(state->state);
	delete state;
}