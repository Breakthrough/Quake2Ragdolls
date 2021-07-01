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
// rf_program.c
// Vertex and fragment program handling
//

#include "rf_local.h"

#define MAX_PROGRAMS		256
#define MAX_PROGRAM_HASH	(MAX_PROGRAMS/4)

static refProgram_t	r_programList[MAX_PROGRAMS];
static refProgram_t	*r_programHashTree[MAX_PROGRAM_HASH];
static uint32		r_numPrograms;

static uint32		r_numProgramErrors;
static uint32		r_numProgramWarnings;

/*
==================
Program_Printf
==================
*/
static void Program_Printf(const comPrint_t flags, const char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAX_COMPRINT];

	if (flags & PRNT_ERROR)
		r_numProgramErrors++;
	else if (flags & PRNT_WARNING)
		r_numProgramWarnings++;

	// Evaluate args
	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	// Print
	Com_ConPrint(flags, msg);
}


/*
==================
Program_DevPrintf
==================
*/
static void Program_DevPrintf(const comPrint_t flags, const char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAX_COMPRINT];

	if (!developer->intVal)
		return;

	if (flags & PRNT_ERROR)
		r_numProgramErrors++;
	else if (flags & PRNT_WARNING)
		r_numProgramWarnings++;

	// Evaluate args
	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	// Print
	Com_ConPrint(flags, msg);
}

/*
==============================================================================

	PROGRAM UPLOADING

==============================================================================
*/

/*
===============
R_UploadProgram
===============
*/
static bool R_UploadProgram(char *name, GLenum target, const char *buffer, GLsizei bufferLen, GLuint *progNum, GLint *upInstructions, GLint *upNative)
{
	const byte	*errorString;
	int			errorPos;

	// Generate a progNum
	qglGenProgramsARB(1, progNum);
	qglBindProgramARB(target, *progNum);
	if (!*progNum)
	{
		Program_Printf(PRNT_ERROR, "R_UploadProgram: could not allocate a progNum!\n");
		return false;
	}

	// Upload
	qglProgramStringARB(target, GL_PROGRAM_FORMAT_ASCII_ARB, bufferLen, buffer);

	// Check for errors
	glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &errorPos);
	if (errorPos != -1)
	{
		// Error thrown
		errorString = glGetString(GL_PROGRAM_ERROR_STRING_ARB);
		switch(target)
		{
		case GL_VERTEX_PROGRAM_ARB:
			if (errorPos == bufferLen)
			{
				Program_Printf(PRNT_ERROR, "R_UploadProgram: '%s' vertex program error at EOF\n", name);
			}
			else
			{
				Program_Printf(PRNT_ERROR, "R_UploadProgram: '%s' vertex program error\n", name);
				Program_Printf(PRNT_ERROR, "GL_PROGRAM_ERROR_POSITION: %i\n", errorPos);
			}
			break;

		case GL_FRAGMENT_PROGRAM_ARB:
			if (errorPos == bufferLen)
			{
				Program_Printf(PRNT_ERROR, "R_UploadProgram: '%s' fragment program error at EOF\n", name);
			}
			else
			{
				Program_Printf(PRNT_ERROR, "R_UploadProgram: '%s' fragment program error\n", name);
				Program_Printf(PRNT_ERROR, "GL_PROGRAM_ERROR_POSITION: %i\n", errorPos);
			}
			break;
		}
		Program_Printf(PRNT_ERROR, "GL_PROGRAM_ERROR_STRING: %s\n", errorString);

		qglDeleteProgramsARB(1, progNum);
		return false;
	}

	qglGetProgramivARB(target, GL_PROGRAM_INSTRUCTIONS_ARB, upInstructions);
	qglGetProgramivARB(target, GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB, upNative);
	return true;
}

/*
===============
R_LoadProgram
===============
*/
static refProgram_t *R_LoadProgram(const char *name, const bool baseDir, GLenum target, const char *buffer, GLsizei bufferLen)
{
	char			*newName;
	char			fixedName[MAX_QPATH];
	refProgram_t	*prog;
	uint32			i;
	GLuint			progNum;
	GLint			upInstructions, upNative;

	// Normalize the name
	Com_NormalizePath(fixedName, sizeof(fixedName), name);
	Q_strlwr(fixedName);
	newName = strrchr(fixedName, '/') + 1;
	if (newName == '\0')
		return NULL;

	// Upload the program
	if (!R_UploadProgram(newName, target, buffer, bufferLen, &progNum, &upInstructions, &upNative))
		return NULL;

	// Find a free r_programList spot
	for (i=0, prog=r_programList ; i<r_numPrograms ; i++, prog++)
	{
		if (!prog->progNum)
			break;
	}

	// None found, create a new spot
	if (i == r_numPrograms)
	{
		if (r_numPrograms+1 >= MAX_PROGRAMS)
			Com_Error(ERR_DROP, "R_LoadProgram: r_numPrograms >= MAX_PROGRAMS");

		prog = &r_programList[r_numPrograms++];
	}

	// Fill out properties
	Q_strncpyz(prog->name, newName, sizeof(prog->name));
	Q_strlwr(prog->name);
	prog->hashValue = Com_HashGenericFast(prog->name, MAX_PROGRAM_HASH);
	prog->baseDir = baseDir;
	prog->target = target;
	prog->upInstructions = upInstructions;
	prog->upNative = upNative;
	prog->progNum = progNum;

	// Link it in
	prog->hashNext = r_programHashTree[prog->hashValue];
	r_programHashTree[prog->hashValue] = prog;
	return prog;
}

/*
==============================================================================

	REGISTRATION

==============================================================================
*/

/*
===============
R_RegisterProgram
===============
*/
refProgram_t *R_RegisterProgram(const char *name, const bool bFragProg)
{
	char			fixedName[MAX_QPATH];
	refProgram_t	*prog, *best;
	GLenum			target;
	uint32			hash;

	// Check the name
	if (!name || !name[0])
		return NULL;

	// Check for extension
	if (!ri.config.ext.bPrograms)
	{
		Com_Error(ERR_DROP, "R_RegisterProgram: attempted to register program when extension is not enabled or available");
		return NULL;
	}

	// Set the target
	if (bFragProg)
		target = GL_FRAGMENT_PROGRAM_ARB;
	else
		target = GL_VERTEX_PROGRAM_ARB;

	// Fix the name
	Com_NormalizePath(fixedName, sizeof(fixedName), name);
	Q_strlwr(fixedName);

	// Calculate hash
	hash = Com_HashGenericFast(fixedName, MAX_PROGRAM_HASH);

	// Search
	best = NULL;
	for (prog=r_programHashTree[hash] ; prog ; prog=prog->hashNext)
	{
		if (prog->target != target)
			continue;
		if (strcmp(fixedName, prog->name))
			continue;

		if (!best || prog->baseDir >= best->baseDir)
			best = prog;
	}

	return best;
}


/*
===============
R_ParseProgramFile
===============
*/
static void R_ParseProgramFile(const char *fixedName, const bool baseDir, bool vp, bool fp)
{
	char	*fileBuffer;
	int		fileLen;
	char	*vpBuf, *fpBuf;
	char	*start, *end;
	char	*token;
	parse_t	*ps;

	if (!ri.config.ext.bPrograms)
	{
		Program_Printf(PRNT_ERROR, "Attempted to parse '%s' while programs are disabled!\n", fixedName);
		return;
	}

	// Load the file
	Program_Printf(0, "...loading '%s'\n", fixedName);
	fileLen = FS_LoadFile(fixedName, (void **)&fileBuffer, true);
	if (!fileBuffer || fileLen <= 0)
	{
		Program_DevPrintf(PRNT_ERROR, "...ERROR: couldn't load '%s' -- %s\n", fixedName, (fileLen == -1) ? "not found" : "empty");
		return;
	}

	// Copy the buffer, and make certain it's newline and null terminated
	if (vp)
	{
		vpBuf = (char *)Mem_PoolAlloc(fileLen, ri.programSysPool, 0);
		memcpy(vpBuf, fileBuffer, fileLen);
	}
	if (fp)
	{
		fpBuf = (char *)Mem_PoolAlloc(fileLen, ri.programSysPool, 0);
		memcpy(fpBuf, fileBuffer, fileLen);
	}

	// Don't need this anymore
	FS_FreeFile(fileBuffer);

	if (vp)
	{
		fileBuffer = vpBuf;
		ps = PS_StartSession(vpBuf, PSP_COMMENT_BLOCK|PSP_COMMENT_LINE|PSP_COMMENT_POUND);

		start = end = NULL;

		// Parse
		for ( ; ; )
		{
			if (!PS_ParseToken(ps, PSF_ALLOW_NEWLINES, &token))
			{
				Program_Printf(PRNT_ERROR, "Missing '!!ARBvp1.0' header\n");
				break;
			}

			// Header
			if (!Q_stricmp(token, "!!ARBvp1.0"))
			{
				start = ps->dataPtr - 10;

				// Find the footer
				for ( ; ; )
				{
					if (!PS_ParseToken(ps, PSF_ALLOW_NEWLINES, &token))
					{
						Program_Printf(PRNT_ERROR, "Missing 'END' footer!\n");
						break;
					}

					if (!Q_stricmp(token, "END"))
					{
						end = ps->dataPtr;
						break;
					}
				}

				if (end)
					break;
			}
		}

		if (start && end)
			R_LoadProgram(fixedName, baseDir, GL_VERTEX_PROGRAM_ARB, start, end-start);
	
		// Done
		PS_EndSession(ps);
		Mem_Free(fileBuffer);
	}
	if (fp)
	{
		fileBuffer = fpBuf;
		ps = PS_StartSession(fpBuf, PSP_COMMENT_BLOCK|PSP_COMMENT_LINE|PSP_COMMENT_POUND);

		start = end = NULL;

		// Parse
		for ( ; ; )
		{
			if (!PS_ParseToken(ps, PSF_ALLOW_NEWLINES, &token))
			{
				Program_Printf(PRNT_ERROR, "Missing '!!ARBfp1.0' header\n");
				break;
			}

			// Header
			if (!Q_stricmp(token, "!!ARBfp1.0"))
			{
				start = ps->dataPtr - 10;

				// Find the footer
				for ( ; ; )
				{
					if (!PS_ParseToken(ps, PSF_ALLOW_NEWLINES, &token))
					{
						Program_Printf(PRNT_ERROR, "Missing 'END' footer!\n");
						break;
					}

					if (!Q_stricmp(token, "END"))
					{
						end = ps->dataPtr;
						break;
					}
				}

				if (end)
					break;
			}
		}

		if (start && end)
			R_LoadProgram(fixedName, baseDir, GL_FRAGMENT_PROGRAM_ARB, start, end-start);

		// Done
		PS_EndSession(ps);
		Mem_Free(fileBuffer);
	}
}

/*
==============================================================================

	CONSOLE FUNCTIONS

==============================================================================
*/

/*
===============
R_ProgramList_f
===============
*/
static void R_ProgramList_f()
{
	Com_Printf(0, "------------------------------------------------------\n");
	for (uint32 i=0 ; i<r_numPrograms ; i++)
	{
		refProgram_t *prog = &r_programList[i];
		Com_Printf(0, "%3i ", prog->progNum);
		Com_Printf(0, "%5i ", prog->upInstructions);
		Com_Printf(0, "%s ", prog->upNative ? "n" : "-");

		switch(prog->target)
		{
		case GL_FRAGMENT_PROGRAM_ARB:	Com_Printf(0, "FP ");	break;
		case GL_VERTEX_PROGRAM_ARB:		Com_Printf(0, "VP ");	break;
		}

		Com_Printf(0, "%s\n", prog->name);
	}
	Com_Printf(0, "Total programs: %i\n", r_numPrograms);
	Com_Printf(0, "------------------------------------------------------\n");
}

/*
==============================================================================

	INIT / SHUTDOWN

==============================================================================
*/

static conCmd_t	*cmd_programList;

/*
===============
R_ProgramInit
===============
*/
void R_ProgramInit()
{
	char	fixedName[MAX_QPATH];
	char	*name;

	uint32 startCycles = Sys_Cycles();
	Com_Printf(0, "\n-------- Program Initialization --------\n");

	// Commands
	cmd_programList = Cmd_AddCommand("programlist", 0, R_ProgramList_f, "Prints out a list of currently loaded vertex and fragment programs");

	r_numProgramErrors = 0;
	r_numProgramWarnings = 0;

	if (ri.config.ext.bPrograms)
	{
		// Load *.vfp programs
		Com_Printf(0, "Looking for *.vfp programs...\n");
		var fileList = FS_FindFiles("programs", "*programs/*.vfp", "vfp", true, true);
		for (uint32 i=0 ; i<fileList.Count() ; i++)
		{
			// Fix the path
			Com_NormalizePath(fixedName, sizeof(fixedName), fileList[i].CString());

			// Check the path
			name = strstr(fixedName, "/programs/");
			if (!name)
				continue;	// This shouldn't happen...
			name++;	// Skip the initial '/'

			// Base dir program?
			bool baseDir = (fileList[i].Contains(BASE_MODDIRNAME "/")) ? true : false;

			R_ParseProgramFile(name, baseDir, true, true);
		}

		// Load *.vp programs
		Com_Printf(0, "Looking for *.vp programs...\n");
		fileList = FS_FindFiles("programs", "*programs/*.vp", "vp", true, true);
		for (uint32 i=0 ; i<fileList.Count() ; i++)
		{
			// Fix the path
			Com_NormalizePath(fixedName, sizeof(fixedName), fileList[i].CString());

			// Check the path
			name = strstr(fixedName, "/programs/");
			if (!name)
				continue;	// This shouldn't happen...
			name++;	// Skip the initial '/'

			// Base dir program?
			bool baseDir = (fileList[i].Contains(BASE_MODDIRNAME "/")) ? true : false;

			R_ParseProgramFile(name, baseDir, true, false);
		}

		// Load *.fp programs
		Com_Printf(0, "Looking for *.fp programs...\n");
		fileList = FS_FindFiles("programs", "*programs/*.fp", "fp", true, true);
		for (uint32 i=0 ; i<fileList.Count() ; i++)
		{
			// Fix the path
			Com_NormalizePath(fixedName, sizeof(fixedName), fileList[i].CString());

			// Check the path
			name = strstr(fixedName, "/programs/");
			if (!name)
				continue;	// This shouldn't happen...
			name++;	// Skip the initial '/'

			// Base dir program?
			bool baseDir = (fileList[i].Contains(BASE_MODDIRNAME "/")) ? true : false;

			R_ParseProgramFile(name, baseDir, false, true);
		}

		Com_Printf(0, "----------------------------------------\n");
	}
	else
	{
		Com_Printf(0, "Program extension disabled, not loading programs.\n");
	}

	// Check for gl errors
	RB_CheckForError("R_ProgramInit");

	// Check memory integrity
	Mem_CheckPoolIntegrity(ri.programSysPool);

	Com_Printf(0, "PROGRAMS - %i error(s), %i warning(s)\n", r_numProgramErrors, r_numProgramWarnings);
	Com_Printf(0, "%u programs loaded in %6.2fms\n", r_numPrograms, (Sys_Cycles()-startCycles) * Sys_MSPerCycle());
	Com_Printf(0, "----------------------------------------\n");
}


/*
===============
R_ProgramShutdown
===============
*/
void R_ProgramShutdown()
{
	Com_Printf(0, "Program system shutdown:\n");

	// Remove commands
	Cmd_RemoveCommand(cmd_programList);

	// Shut the programs down
	if (ri.config.ext.bPrograms)
	{
		qglBindProgramARB(GL_VERTEX_PROGRAM_ARB, 0);
		qglBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, 0);
	}

	for (uint32 i=0 ; i<r_numPrograms ; i++)
	{
		refProgram_t *prog = &r_programList[i];
		if (!prog->progNum)
			continue;	// Free r_programList slot

		// Free it
		qglDeleteProgramsARB(1, &prog->progNum);
	}

	r_numPrograms = 0;
	memset(r_programList, 0, sizeof(r_programList));
	memset(r_programHashTree, 0, sizeof(r_programHashTree));

	uint32 size = Mem_FreePool(ri.programSysPool);
	Com_Printf(0, "...releasing %u bytes...\n", size);
}
