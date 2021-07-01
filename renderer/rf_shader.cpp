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
// rf_shader.c
//

#include "rf_local.h"

#define MAX_SHADERS		512
#define MAX_SHADER_HASH	(MAX_SHADERS/4)

struct refShaderInternal_t : public refShader_t
{
	bool						baseDir;

	uint32						vertexShader;
	uint32						fragmentShader;
};

#include <unordered_map>

typedef std::tr1::unordered_map<std::string, refShaderInternal_t*> TShaderList;
TShaderList shaderList;


/*
==============================================================================

	REGISTRATION

==============================================================================
*/

/*
===============
R_RegisterShader
===============
*/
refShader_t *R_RegisterShader(const char *name)
{
	// Check the name
	if (!name || !name[0])
		return NULL;

	// Check for extension
	if (!ri.config.ext.bShaders)
	{
		Com_Error(ERR_DROP, "R_RegisterShader: attempted to register shader when extension is not enabled or available");
		return NULL;
	}

	// Fix the name
	char fixedName[MAX_QPATH];
	Com_NormalizePath(fixedName, sizeof(fixedName), name);
	Q_strlwr(fixedName);

	// Calculate hash
	const uint32 hash = Com_HashFileName(fixedName, MAX_SHADER_HASH);

	// Search
	var it = shaderList.find(fixedName);

	if (it == shaderList.end())
		return null;

	return (*it).second;
}


/*
===============
R_FreeShader
===============
*/
static void R_FreeShader(refShaderInternal_t *Shader)
{
	if (Shader->vertexShader)
	{
		qglDetachObjectARB(Shader->objectHandle, Shader->vertexShader);
		qglDeleteObjectARB(Shader->vertexShader);
		Shader->vertexShader = 0;
	}

	if (Shader->fragmentShader)
	{
		qglDetachObjectARB(Shader->objectHandle, Shader->fragmentShader);
		qglDeleteObjectARB(Shader->fragmentShader);
		Shader->fragmentShader = 0;
	}

	if (Shader->objectHandle)
	{
		qglDeleteObjectARB(Shader->objectHandle);
		Shader->objectHandle = 0;
	}

	Shader->name[0] = '\0';
	delete Shader;
}

/*
==============================================================================

	SHADER UPLOADING

==============================================================================
*/

/*
===============
R_CompileShader
===============
*/
static int R_CompileShader(const uint32 baseHandle, const char *fixedName, const char *TypeName, const uint32 ShaderType, const char **strings, const int numStrings)
{
	// Get an object handle
	uint32 shaderHandle = qglCreateShaderObjectARB(ShaderType);
	if (!shaderHandle)
		return 0;

	// Compile the shader
	// if lengths is NULL, then each string is assumed to be null-terminated
	qglShaderSourceARB(shaderHandle, numStrings, strings, NULL);
	qglCompileShaderARB(shaderHandle);

	// Check compile status
	int Status;
	qglGetObjectParameterivARB(shaderHandle, GL_OBJECT_COMPILE_STATUS_ARB, &Status);
	if (!Status)
	{
		char Output[4096];

		qglGetInfoLogARB(shaderHandle, sizeof(Output), NULL, Output);
		Output[sizeof(Output)-1] = '\0';

		if (Output[0])
			Com_Printf(PRNT_WARNING, "Failed to compile %s shader object for shader '%s':\n%s\n", TypeName, fixedName, Output);

		qglDeleteObjectARB(shaderHandle);
		return 0;
	}

	// Attach
	qglAttachObjectARB(baseHandle, shaderHandle);
	return shaderHandle;
}

/*
===============
R_ParseShaderFile
===============
*/
static void R_ParseShaderFile(const char *fixedName, const bool baseDir)
{
	if (!ri.config.ext.bShaders)
	{
		Com_Printf(PRNT_ERROR, "Attempted to parse '%s' while shaders are disabled!\n", fixedName);
		return;
	}

	Com_Printf(0, "...loading %s\n", fixedName);

	var shader = new refShaderInternal_t;

	// Load the file
	char *FileBuffer;
	int FileLen = FS_LoadFile(fixedName, (void**)&FileBuffer, true);
	if (!FileBuffer || FileLen <= 0)
	{
		Com_Printf(PRNT_ERROR, "...ERROR: couldn't load '%s' -- %s\n", fixedName, (FileLen == -1) ? "not found" : "empty");
		return;
	}

	// We only store file names
	const char *newName = strrchr(fixedName, '/') + 1;
	if (newName == '\0')
		return;

	// Allocate a shader object
	shader->objectHandle = qglCreateProgramObjectARB();
	if (!shader->objectHandle)
	{
		FS_FreeFile(FileBuffer);
		R_FreeShader(shader);
		return;
	}

	// Generate shader data
	const char *vertStrings[2];
	vertStrings[0] = "#define VERTEX_SHADER\n";
	vertStrings[1] = FileBuffer;

	// Compile the shaders
	shader->vertexShader = R_CompileShader(shader->objectHandle, newName, "vertex", GL_VERTEX_SHADER_ARB, vertStrings, 2);
	if (!shader->vertexShader)
	{
		FS_FreeFile(FileBuffer);
		R_FreeShader(shader);
		return;
	}

	// Generate shader data
	const char *fragStrings[2];
	fragStrings[0] = "#define FRAGMENT_SHADER\n";
	fragStrings[1] = FileBuffer;

	// Compile the shaders
	shader->fragmentShader = R_CompileShader(shader->objectHandle, newName, "fragment", GL_FRAGMENT_SHADER_ARB, fragStrings, 2);
	if (!shader->fragmentShader)
	{
		FS_FreeFile(FileBuffer);
		R_FreeShader(shader);
		return;
	}

	// Don't need this anymore
	FS_FreeFile(FileBuffer);

	// Link
	qglLinkProgramARB(shader->objectHandle);

	// Check link status
	int Linked;
	qglGetObjectParameterivARB(shader->objectHandle, GL_OBJECT_LINK_STATUS_ARB, &Linked);
	if (!Linked)
	{
		char Output[4096];

		qglGetInfoLogARB(shader->objectHandle, sizeof(Output), NULL, Output);
		Output[sizeof(Output)-1] = '\0';

		if (Output[0])
			Com_Printf(PRNT_WARNING, "Failed to compile link object for shader '%s':\n%s\n", newName, Output);

		R_FreeShader(shader);
		return;
	}

	qglUseProgramObjectARB(shader->objectHandle);

	int TexUnit0 = qglGetUniformLocationARB(shader->objectHandle, "TexUnit0");
	if (TexUnit0 >= 0)
		qglUniform1iARB(TexUnit0, 0);

	qglUseProgramObjectARB(0);

	// Store values
	Q_strncpyz(shader->name, newName, sizeof(shader->name));
	Q_strlwr(shader->name);
	shader->baseDir = baseDir;

	shaderList.insert(TShaderList::value_type(fixedName, shader));
}

/*
==============================================================================

	CONSOLE FUNCTIONS

==============================================================================
*/

/*
===============
R_ShaderList_f
===============
*/
static void R_ShaderList_f()
{
	for(var it = shaderList.begin(); it != shaderList.end(); ++it)
	{
		refShaderInternal_t *shader = (*it).second;
		Com_Printf(0, "%2i(%2i %2i) %s\n", shader->objectHandle, shader->vertexShader, shader->fragmentShader, shader->name);
	}
}

/*
==============================================================================

	INIT / SHUTDOWN

==============================================================================
*/

static conCmd_t *cmd_shaderList;

/*
===============
R_ShaderInit
===============
*/
void R_ShaderInit()
{
	uint32 startCycles = Sys_Cycles();
	Com_Printf(0, "\n--------- Shader Initialization --------\n");

	// Commands
	cmd_shaderList = Cmd_AddCommand("shaderlist", 0, R_ShaderList_f, "Prints out a list of currently loaded vertex and fragment shaders");

	if (ri.config.ext.bShaders)
	{
		// Load *.glsl shaders
		Com_Printf(0, "Looking for *.glsl shaders...\n");

		var fileList = FS_FindFiles("shaders", "*shaders/*.glsl", "glsl", true, true);
		for (uint32 i=0 ; i<fileList.Count() ; i++)
		{
			// Fix the path
			char fixedName[MAX_QPATH];
			Com_NormalizePath(fixedName, sizeof(fixedName), fileList[i].CString());

			// Check the path
			char *name = strstr(fixedName, "/shaders/") + 1;
			if (!name)
				continue;

			// Base dir shader?
			bool baseDir = (fileList[i].Contains(BASE_MODDIRNAME "/")) ? true : false;

			R_ParseShaderFile(name, baseDir);
		}
	}
	else
	{
		Com_Printf(0, "Shader extension disabled, not loading shaders.\n");
	}

	// Check for gl errors
	RB_CheckForError("R_ShaderInit");

	// Check memory integrity
	Mem_CheckPoolIntegrity(ri.shaderSysPool);

	Com_Printf(0, "%u shaders loaded in %6.2fms\n", shaderList.size(), (Sys_Cycles()-startCycles) * Sys_MSPerCycle());
	Com_Printf(0, "----------------------------------------\n");
}


/*
===============
R_ShaderShutdown
===============
*/
void R_ShaderShutdown()
{
	Com_Printf(0, "Shader system shutdown:\n");

	Cmd_RemoveCommand(cmd_shaderList);

	for (var it = shaderList.begin(); it != shaderList.end(); ++it)
		R_FreeShader((*it).second);

	shaderList.clear();

	uint32 size = Mem_FreePool(ri.shaderSysPool);
	Com_Printf(0, "...releasing %u bytes...\n", size);
}
