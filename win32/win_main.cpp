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
// win_main.c
//

#include "../common/common.h"
#include "win_local.h"
#include "resource.h"
#include <errno.h>
#include <float.h>
#include <fcntl.h>
#include <stdio.h>
#include <direct.h>
#include <io.h>
#include <conio.h>
#include <process.h>
#include <dbghelp.h>

#if USE_CURL
#define CURL_STATICLIB
#include "../include/curl/curl.h"
#endif

#include "../minizip/unzip.h"

#define MINIMUM_WIN_MEMORY	0x0a00000
#define MAXIMUM_WIN_MEMORY	0x1000000

winInfo_t	sys_winInfo;

#define MAX_NUM_ARGVS	128
static int	sys_argCnt = 0;
static char	*sys_argVars[MAX_NUM_ARGVS];

/*
==============================================================================

	SYSTEM IO

==============================================================================
*/

/*
================
Sys_Init
================
*/
typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
void Sys_Init ()
{
	LPFN_ISWOW64PROCESS	fnIsWow64Process;
	OSVERSIONINFO		vInfo;
	BOOL				isWow64 = false;

	// Check operating system info
	vInfo.dwOSVersionInfoSize = sizeof(vInfo);

	if (!GetVersionEx (&vInfo))
		Sys_Error ("Couldn't get OS info");

	if (vInfo.dwMajorVersion < 4)
		Sys_Error ("EGL requires Windows version 4 or greater");
	if (vInfo.dwPlatformId == VER_PLATFORM_WIN32s)
		Sys_Error ("EGL doesn't run on Win32s");

	sys_winInfo.isWin9x = (vInfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS);
	sys_winInfo.isWinNT = (vInfo.dwMajorVersion >= 5);

	// Check if it's 64-bit
	sys_winInfo.isWow64 = false;
	fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(GetModuleHandle("KERNEL32"),"IsWow64Process");
	if (fnIsWow64Process)
	{
		if (fnIsWow64Process (GetCurrentProcess(), &isWow64))
			sys_winInfo.isWow64 = isWow64 ? true : false;
	}
}


/*
================
Sys_Quit
================
*/
void Sys_Quit (bool error)
{
	Sys_DestroyConsole ();

#ifndef DEDICATED_ONLY
	if (!dedicated->intVal)
		CL_ClientShutdown (error);
#endif
	Com_Shutdown ();

	timeEndPeriod (1);

	ExitProcess (0);
}


/*
================
Sys_Error
================
*/
void Sys_Error (char *error, ...)
{
	va_list		argptr;
	char		text[1024];
	MSG			msg;

	va_start (argptr, error);
	vsnprintf (text, sizeof(text), error, argptr);
	va_end (argptr);

	Sys_SetConsoleTitle ("EGL ERROR!");
	Sys_SetErrorText (text);
	Sys_ShowConsole (1, true);

#ifndef DEDICATED_ONLY
	if (!dedicated->intVal)
		CL_ClientShutdown (true);
#endif
	Com_Shutdown ();

	timeEndPeriod (1);

	// Wait for the user to quit
	for ( ; ; )
	{
		if (!GetMessage (&msg, NULL, 0, 0))
			Com_Quit (true);

		TranslateMessage (&msg);
      	DispatchMessage (&msg);
	}

	Sys_DestroyConsole ();

	ExitProcess (1);
}


/*
================
Sys_Print
================
*/
void Sys_Print (const char *message)
{
	Sys_ConsolePrint (message);
}

// ===========================================================================

/*
================
Sys_ScanForCD
================
*/
static char *Sys_ScanForCD ()
{
	static char		cddir[MAX_OSPATH];
	static bool	done = false;
	char			drive[4];
	FILE			*f;
	char			test[MAX_QPATH];

	if (done)	// Don't re-check
		return cddir;

	drive[0] = 'c';
	drive[1] = ':';
	drive[2] = '\\';
	drive[3] = 0;

	done = true;

	// Scan the drives
	for (drive[0]='c' ; drive[0]<='z' ; drive[0]++) {
		// Where activision put the stuff
		Q_snprintfz (cddir, sizeof(cddir), "%sinstall\\data", drive);
		Q_snprintfz (test, sizeof(test), "%sinstall\\data\\quake2.exe", drive);
		f = fopen (test, "r");
		if (f) {
			fclose (f);
			if (GetDriveType (drive) == DRIVE_CDROM)
				return cddir;
		}
	}

	cddir[0] = '\0';
	
	return NULL;
}

// ===========================================================================

static int sys_timeBase;
static double sys_msPerCycle;

/*
================
Sys_Cycles
================
*/
uint32 Sys_Cycles()
{
	LARGE_INTEGER PC;

	QueryPerformanceCounter(&PC);

	return PC.QuadPart;
}


/*
================
Sys_MSPerCycle
================
*/
double Sys_MSPerCycle()
{
	return sys_msPerCycle;
}


/*
================
Sys_Milliseconds
================
*/
int Sys_Milliseconds()
{
	return timeGetTime() - sys_timeBase;
}


/*
================
Sys_UMilliseconds
================
*/
uint32 Sys_UMilliseconds()
{
	return timeGetTime() - sys_timeBase;
}


/*
=================
Sys_AppActivate
=================
*/
void Sys_AppActivate ()
{
#ifndef DEDICATED_ONLY
	if (dedicated->intVal)
		return;

	SetForegroundWindow (sys_winInfo.hWnd);
	ShowWindow (sys_winInfo.hWnd, SW_SHOW);
#endif
}


/*
================
Sys_Mkdir
================
*/
void Sys_Mkdir (char *path)
{
	_mkdir (path);
}


/*
================
Sys_SendKeyEvents

Send Key_Event calls
================
*/
void Sys_SendKeyEvents()
{
	MSG		msg;

	// Dispatch window messages
	while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
	{
		if (!GetMessage(&msg, NULL, 0, 0))
			Com_Quit(true);

		sys_winInfo.msgTime = msg.time;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

#ifndef DEDICATED_ONLY
	// Update the client
	if (!dedicated->intVal)
		CL_UpdateFrameTime(timeGetTime());
#endif
}


/*
================
Sys_GetClipboardData
================
*/
char *Sys_GetClipboardData ()
{
	HANDLE	hClipboardData;
	char	*data = NULL;
	char	*clipText;

	if (OpenClipboard (NULL)) {
		hClipboardData = GetClipboardData (CF_TEXT);
		if (hClipboardData) {
			clipText = (char*)GlobalLock (hClipboardData);
			if (clipText) {
				data = (char*)Mem_Alloc (GlobalSize (hClipboardData) + 1);
				strcpy (data, clipText);
				GlobalUnlock (hClipboardData);
			}
		}

		CloseClipboard ();
	}

	return data;
}

// ===========================================================================

char		sys_findBase[MAX_OSPATH];
char		sys_findPath[MAX_OSPATH];
HANDLE		sys_findHandle = INVALID_HANDLE_VALUE;

static bool Sys_CompareFileAttributes (int found, uint32 mustHave, uint32 cantHave)
{
	// read only
	if (found & FILE_ATTRIBUTE_READONLY) {
		if (cantHave & SFF_RDONLY)
			return false;
	}
	else if (mustHave & SFF_RDONLY)
		return false;

	// hidden
	if (found & FILE_ATTRIBUTE_HIDDEN) {
		if (cantHave & SFF_HIDDEN)
			return false;
	}
	else if (mustHave & SFF_HIDDEN)
		return false;

	// system
	if (found & FILE_ATTRIBUTE_SYSTEM) {
		if (cantHave & SFF_SYSTEM)
			return false;
	}
	else if (mustHave & SFF_SYSTEM)
		return false;

	// subdir
	if (found & FILE_ATTRIBUTE_DIRECTORY) {
		if (cantHave & SFF_SUBDIR)
			return false;
	}
	else if (mustHave & SFF_SUBDIR)
		return false;

	// arch
	if (found & FILE_ATTRIBUTE_ARCHIVE) {
		if (cantHave & SFF_ARCH)
			return false;
	}
	else if (mustHave & SFF_ARCH)
		return false;

	return true;
}

/*
================
Sys_FindClose
================
*/
void Sys_FindClose ()
{
	if (sys_findHandle != INVALID_HANDLE_VALUE)
		FindClose (sys_findHandle);

	sys_findHandle = INVALID_HANDLE_VALUE;
}


/*
================
Sys_FindFirst
================
*/
char *Sys_FindFirst (char *path, uint32 mustHave, uint32 cantHave)
{
	WIN32_FIND_DATA	findinfo;

	if (sys_findHandle != INVALID_HANDLE_VALUE)
		Sys_Error ("Sys_BeginFind without close");

	Com_FilePath (path, sys_findBase, sizeof(sys_findBase));
	sys_findHandle = FindFirstFile (path, &findinfo);

	if (sys_findHandle == INVALID_HANDLE_VALUE)
		return NULL;

	if (!Sys_CompareFileAttributes (findinfo.dwFileAttributes, mustHave, cantHave))
		return NULL;

	Q_snprintfz (sys_findPath, sizeof(sys_findPath), "%s/%s", sys_findBase, findinfo.cFileName);
	return sys_findPath;
}


/*
================
Sys_FindNext
================
*/
char *Sys_FindNext (uint32 mustHave, uint32 cantHave)
{
	WIN32_FIND_DATA	findinfo;

	if (sys_findHandle == INVALID_HANDLE_VALUE)
		return NULL;

	if (!FindNextFile (sys_findHandle, &findinfo))
		return NULL;

	if (!Sys_CompareFileAttributes (findinfo.dwFileAttributes, mustHave, cantHave))
		return NULL;

	Q_snprintfz (sys_findPath, sizeof(sys_findPath), "%s/%s", sys_findBase, findinfo.cFileName);
	return sys_findPath;
}


/*
================
Sys_FindFiles
================
*/
TList<String> Sys_FindFiles (char *path, char *pattern, int fileCount, bool recurse, bool addFiles, bool addDirs)
{
	WIN32_FIND_DATA	findInfo;
	HANDLE			findHandle;
	BOOL			findRes = TRUE;
	char			findPath[MAX_OSPATH], searchPath[MAX_OSPATH];
	TList<String>	files;

	Q_snprintfz (searchPath, sizeof(searchPath), "%s/*", path);

	findHandle = FindFirstFile (searchPath, &findInfo);
	if (findHandle == INVALID_HANDLE_VALUE)
		return fileCount;

	while (findRes == TRUE) {
		// Check for invalid file name
		if (findInfo.cFileName[strlen(findInfo.cFileName)-1] == '.') {
			findRes = FindNextFile (findHandle, &findInfo);
			continue;
		}

		Q_snprintfz (findPath, sizeof(findPath), "%s/%s", path, findInfo.cFileName);

		if (findInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			// Add a directory
			if (addDirs)
				files.Add(String(findPath));

			// Recurse down the next directory
			if (recurse)
				files.AddRange(Sys_FindFiles (findPath, pattern, fileCount, recurse, addFiles, addDirs));
		}
		else {
			// Match pattern
			if (!Q_WildcardMatch (pattern, findPath, 1)) {
				findRes = FindNextFile (findHandle, &findInfo);
				continue;
			}

			// Add a file
			if (addFiles)
				files.Add(String(findPath));
		}

		findRes = FindNextFile (findHandle, &findInfo);
	}

	FindClose (findHandle);

	return files;
}

/*
==============================================================================

	LIBRARY MANAGEMENT

==============================================================================
*/

#if defined(_M_IX86)
# define LIBARCH		"x86"
# ifdef _DEBUG
#  define LIBDEBUGDIR	"debug"
# else
#  define LIBDEBUGDIR	"release"
# endif

#elif defined(_M_ALPHA)
# define LIBARCH		"axp"
# ifdef _DEBUG
#  define LIBDEBUGDIR	"debugaxp"
# else
#  define LIBDEBUGDIR	"releaseaxp"
# endif
#endif

struct libList_t {
	const char		*title;
	HINSTANCE		hInst;
	const char		*fileName;
	LPCSTR			apiFuncName;
};

static libList_t sys_libList[LIB_MAX] = {
	{ "LIB_CGAME",	NULL,	"pglcgame" LIBARCH ".dll",	"GetCGameAPI"	},
	{ "LIB_GAME",	NULL,	"pglgame" LIBARCH ".dll",		"GetGameAPI"	},
};

/*
=================
Sys_UnloadLibrary
=================
*/
void Sys_UnloadLibrary (ELibType libType)
{
	HINSTANCE	*lib;

	// Sanity check
	if (libType < 0 || libType > LIB_MAX)
		Com_Error (ERR_FATAL, "Sys_UnloadLibrary: Bad libType (%i)", libType);

	// Find the library
	lib = &sys_libList[libType].hInst;

	// Free it
	Com_DevPrintf (0, "FreeLibrary (%s)\n", sys_libList[libType].title);
	if (!FreeLibrary (*lib))
		Com_Error (ERR_FATAL, "FreeLibrary (%s) failed", sys_libList[libType].title);

	*lib = NULL;
}


/*
=================
Sys_LoadLibrary

Loads a module
=================
*/
void *Sys_LoadLibrary (ELibType libType, void *parms)
{
	HINSTANCE	*lib;
	const char	*libName;
	void		*(*APIfunc) (void *);
	char		name[MAX_OSPATH];
	char		cwd[MAX_OSPATH];
	char		*path;

	// Sanity check
	if (libType < 0 || libType > LIB_MAX)
		Com_Error (ERR_FATAL, "Sys_UnloadLibrary: Bad libType (%i)", libType);

	// Find the library
	lib = &sys_libList[libType].hInst;
	libName = sys_libList[libType].fileName;

	// Make sure it's not already loaded first
	if (*lib)
		Com_Error (ERR_FATAL, "Sys_LoadLibrary (%s) without Sys_UnloadLibrary", sys_libList[libType].title);

	// Check the current debug directory first for development purposes
	_getcwd (cwd, sizeof(cwd));
	Q_snprintfz (name, sizeof(name), "%s/%s/%s", cwd, LIBDEBUGDIR, libName);
	*lib = LoadLibrary (name);
	if (*lib) {
		Com_DevPrintf (0, "Sys_LoadLibrary (%s)\n", name);
	}
	else {
#ifdef _DEBUG
		// Check the current directory for other development purposes
		Q_snprintfz (name, sizeof(name), "%s/%s", cwd, libName);
		*lib = LoadLibrary (name);
		if (*lib) {
			Com_DevPrintf (0, "Sys_LoadLibrary (%s)\n", libName);
		}
		else {
#endif
			// Now run through the search paths
			path = NULL;
			for ( ; ; ) {
				path = FS_NextPath (path);
				if (!path)
					return NULL;	// Couldn't find one anywhere

				Q_snprintfz (name, sizeof(name), "%s/%s", path, libName);
				*lib = LoadLibrary (name);
				if (*lib) {
					Com_DevPrintf (0, "Sys_LoadLibrary (%s)\n",name);
					break;
				}
			}
#ifdef _DEBUG
		}
#endif
	}

	// Find the API function
	APIfunc = (void *(*)(void*))GetProcAddress (*lib, sys_libList[libType].apiFuncName);
	if (!APIfunc) {
		Sys_UnloadLibrary (libType);
		return NULL;
	}

	return APIfunc (parms);
}

/*
==============================================================================

	EXCEPTION HANDLER

==============================================================================
*/

typedef BOOL (WINAPI *ENUMERATELOADEDMODULES64) (HANDLE hProcess, PENUMLOADED_MODULES_CALLBACK64 EnumLoadedModulesCallback, PVOID UserContext);
typedef DWORD (WINAPI *SYMSETOPTIONS) (DWORD SymOptions);
typedef BOOL (WINAPI *SYMINITIALIZE) (HANDLE hProcess, PSTR UserSearchPath, BOOL fInvadeProcess);
typedef BOOL (WINAPI *SYMCLEANUP) (HANDLE hProcess);
typedef BOOL (WINAPI *STACKWALK64) (
								DWORD MachineType,
								HANDLE hProcess,
								HANDLE hThread,
								LPSTACKFRAME64 StackFrame,
								PVOID ContextRecord,
								PREAD_PROCESS_MEMORY_ROUTINE64 ReadMemoryRoutine,
								PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine,
								PGET_MODULE_BASE_ROUTINE64 GetModuleBaseRoutine,
								PTRANSLATE_ADDRESS_ROUTINE64 TranslateAddress
								);

typedef PVOID	(WINAPI *SYMFUNCTIONTABLEACCESS64) (HANDLE hProcess, DWORD64 AddrBase);
typedef DWORD64 (WINAPI *SYMGETMODULEBASE64) (HANDLE hProcess, DWORD64 dwAddr);
typedef BOOL	(WINAPI *SYMFROMADDR) (HANDLE hProcess, DWORD64 Address, PDWORD64 Displacement, PSYMBOL_INFO Symbol);
typedef BOOL	(WINAPI *SYMGETMODULEINFO64) (HANDLE hProcess, DWORD64 dwAddr, PIMAGEHLP_MODULE64 ModuleInfo);

typedef DWORD64 (WINAPI *SYMLOADMODULE64) (HANDLE hProcess, HANDLE hFile, PSTR ImageName, PSTR ModuleName, DWORD64 BaseOfDll, DWORD SizeOfDll);

typedef BOOL (WINAPI *MINIDUMPWRITEDUMP) (
	HANDLE hProcess,
	DWORD ProcessId,
	HANDLE hFile,
	MINIDUMP_TYPE DumpType,
	PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
	PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
	PMINIDUMP_CALLBACK_INFORMATION CallbackParam
	);

typedef HINSTANCE (WINAPI *SHELLEXECUTEA) (HWND hwnd, LPCTSTR lpOperation, LPCTSTR lpFile, LPCTSTR lpParameters, LPCTSTR lpDirectory, INT nShowCmd);

typedef BOOL (WINAPI *VERQUERYVALUE) (const LPVOID pBlock, LPTSTR lpSubBlock, PUINT lplpBuffer, PUINT puLen);
typedef DWORD (WINAPI *GETFILEVERSIONINFOSIZE) (LPTSTR lptstrFilename, LPDWORD lpdwHandle);
typedef BOOL (WINAPI *GETFILEVERSIONINFO) (LPTSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData);

static SYMGETMODULEINFO64		fnSymGetModuleInfo64;
static SYMLOADMODULE64			fnSymLoadModule64;

static VERQUERYVALUE			fnVerQueryValue;
static GETFILEVERSIONINFOSIZE	fnGetFileVersionInfoSize;
static GETFILEVERSIONINFO		fnGetFileVersionInfo;

static CHAR						szModuleName[MAX_PATH];

/*
==================
EGLExceptionHandler
==================
*/
static int Sys_FileLength (const char *path)
{
	WIN32_FILE_ATTRIBUTE_DATA	fileData;

	if (GetFileAttributesEx (path, GetFileExInfoStandard, &fileData))
		return fileData.nFileSizeLow;

	return -1;
}

#if USE_CURL
static int EGLUploadProgress (void *clientp, double downTotal, double downNow, double upTotal, double upNow)
{
	char	progressBuff[512];
	DWORD	len;
	double	speed;
	float	percent;
	MSG		msg;

	if (upTotal <= 0)
		return 0;

	curl_easy_getinfo ((CURL *)clientp, CURLINFO_SPEED_UPLOAD, &speed);

	if (upNow == upTotal)
		percent = 100.0f;
	else {
		percent = ((upNow / upTotal) * 100.0f);
		if (percent == 100.0f)
			percent = 99.99f;
	}

	len = snprintf (progressBuff, sizeof(progressBuff)-1, "[%6.2f%%] %g / %g bytes, %g bytes/sec.\n", percent, upNow, upTotal, speed);
	Sys_ConsolePrint (progressBuff);

	snprintf (progressBuff, sizeof(progressBuff)-1, "EGL Crash Dump Uploader [%6.2f%%]", percent);
	Sys_SetConsoleTitle (progressBuff);

	// Dispatch window messages
	while (PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE)) {
		GetMessage (&msg, NULL, 0, 0);
		TranslateMessage (&msg);
		DispatchMessage (&msg);
	}

	return 0;
}

static VOID EGLUploadCrashDump (LPCSTR crashDump, LPCSTR crashText)
{
	struct curl_httppost* post = NULL;
	struct curl_httppost* last = NULL;
	CURL	*curl;
	DWORD	lenDmp;
	DWORD	lenTxt;

	__try {
		lenDmp = Sys_FileLength (crashDump);
		lenTxt = Sys_FileLength (crashText);

		if (lenTxt == -1)
			return;

		Sys_SetConsoleTitle ("EGL Crash Dump Uploader");
		Sys_ConsolePrint ("Connecting...\n");

		curl = curl_easy_init ();

		// Add simple file section
		if (lenDmp > 0)
			curl_formadd (&post, &last, CURLFORM_PTRNAME, "minidump", CURLFORM_FILE, crashDump, CURLFORM_END);
		curl_formadd (&post, &last, CURLFORM_PTRNAME, "report", CURLFORM_FILE, crashText, CURLFORM_END);

		// Set the form info
		curl_easy_setopt (curl, CURLOPT_HTTPPOST, post);

//		if (cl_http_proxy)
//			curl_easy_setopt (curl, CURLOPT_PROXY, cl_http_proxy->string);

		//curl_easy_setopt (curl, CURLOPT_UPLOAD, 1);
		curl_easy_setopt (curl, CURLOPT_PROGRESSFUNCTION, EGLUploadProgress);
		curl_easy_setopt (curl, CURLOPT_PROGRESSDATA, curl);
		curl_easy_setopt (curl, CURLOPT_NOPROGRESS, 0);

		curl_easy_setopt (curl, CURLOPT_FOLLOWLOCATION, 1);
		curl_easy_setopt (curl, CURLOPT_USERAGENT, APP_FULLNAME);
		curl_easy_setopt (curl, CURLOPT_URL, "http://egl.quakedev.com/reports/receiveCrashDump.php");

		if (curl_easy_perform (curl) != CURLE_OK) {
			MessageBox (NULL, "An error occured while trying to upload the crash dump. Please post it manually on the EGL forums.", "Upload Error", MB_ICONEXCLAMATION | MB_OK);
		}
		else {
			long	response;

			if (curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &response) == CURLE_OK) {
				switch (response) {
				case 200:
					MessageBox (NULL, "Upload completed. Thanks for submitting your crash report!\n\n"
										"If you would like feedback on the cause of this crash, post on the EGL forums describing what you were\n"
										"doing at the time the exception occured. If possible, please also attach the EGLCrashLog.txt file.",
										"Upload Complete", MB_ICONINFORMATION | MB_OK);
					break;
				default:
					MessageBox (NULL, "Upload failed, HTTP error.", "Upload Failed", MB_ICONEXCLAMATION | MB_OK);
					break;
				}
			}
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		MessageBox (NULL, "An exception occured while trying to upload the crash dump. Please post it manually on the EGL forums.", "Upload Error", MB_ICONEXCLAMATION | MB_OK);
	}
}
#endif

static BOOL CALLBACK EnumerateLoadedModulesProcSymInfo (PSTR ModuleName, DWORD64 ModuleBase, ULONG ModuleSize, PVOID UserContext)
{
	IMAGEHLP_MODULE64	symInfo = {0};
	FILE				*fhReport = (FILE *)UserContext;
	PCHAR				symType;

	symInfo.SizeOfStruct = sizeof(symInfo);

	if (fnSymGetModuleInfo64 (GetCurrentProcess(), ModuleBase, &symInfo)) {
		switch (symInfo.SymType) {
		case SymCoff:		symType = "COFF";		break;
		case SymCv:			symType = "CV";			break;
		case SymExport:		symType = "Export";		break;
		case SymPdb:		symType = "PDB";		break;
		case SymNone:		symType = "No";			break;
		default:			symType = "Unknown";	break;
		}

		fprintf (fhReport, "%s, %s symbols loaded.\r\n", symInfo.LoadedImageName, symType);
	}
	else {
		int i = GetLastError ();
		fprintf (fhReport, "%s, couldn't check symbols (error %d, DBGHELP.DLL too old?)\r\n", ModuleName, i);
	}
	
	return TRUE;
}

static BOOL CALLBACK EnumerateLoadedModulesProcDump (PSTR ModuleName, DWORD64 ModuleBase, ULONG ModuleSize, PVOID UserContext)
{
	VS_FIXEDFILEINFO *fileVersion;
	BYTE	*verInfo;
	DWORD	len;
	UINT	dummy;
	FILE	*fhReport = (FILE *)UserContext;
	CHAR	verString[32];
	CHAR	lowered[MAX_PATH];

	Q_strncpyz (lowered, ModuleName, sizeof(lowered)-1);
	Q_strlwr (lowered);

	if (fnGetFileVersionInfo && fnVerQueryValue && fnGetFileVersionInfoSize) {
		if (len = (fnGetFileVersionInfoSize ((LPTSTR)ModuleName, (LPDWORD)&dummy))) {
			verInfo = (byte*)LocalAlloc (LPTR, len);
			if (fnGetFileVersionInfo (ModuleName, dummy, len, verInfo)) {
				if (fnVerQueryValue ((void*)verInfo, "\\", (PUINT)&fileVersion, &dummy)) {
					sprintf (verString, "%d.%d.%d.%d", HIWORD(fileVersion->dwFileVersionMS), LOWORD(fileVersion->dwFileVersionMS), HIWORD(fileVersion->dwFileVersionLS), LOWORD(fileVersion->dwFileVersionLS));
				}
				else {
					strcpy (verString, "unknown");
				}
			}
			else {
				strcpy (verString, "unknown");
			}

			LocalFree (verInfo);
		}
		else {
			strcpy (verString, "unknown");
		}	
	}
	else {
		strcpy (verString, "unknown");
	}	

	fprintf (fhReport, "[0x%I64p - 0x%I64p] %s (%lu bytes, version %s)\r\n", ModuleBase, ModuleBase + (DWORD64)ModuleSize, ModuleName, ModuleSize, verString);
	return TRUE;
}

static BOOL CALLBACK EnumerateLoadedModulesProcInfo (PSTR ModuleName, DWORD64 ModuleBase, ULONG ModuleSize, PVOID UserContext)
{
	DWORD	addr = (DWORD)UserContext;
	if (addr > ModuleBase && addr < ModuleBase+ModuleSize) {
		strncpy (szModuleName, ModuleName, sizeof(szModuleName)-1);
		return FALSE;
	}
	return TRUE;
}

static DWORD EGLExceptionHandler (DWORD exceptionCode, LPEXCEPTION_POINTERS exceptionInfo)
{
	HANDLE						hProcess;
	HMODULE						hDbgHelp, hVersion;
	ENUMERATELOADEDMODULES64	fnEnumerateLoadedModules64;
	SYMSETOPTIONS				fnSymSetOptions;
	SYMINITIALIZE				fnSymInitialize;
	STACKWALK64					fnStackWalk64;
	SYMFUNCTIONTABLEACCESS64	fnSymFunctionTableAccess64;
	SYMGETMODULEBASE64			fnSymGetModuleBase64;
	SYMFROMADDR					fnSymFromAddr;
	SYMCLEANUP					fnSymCleanup;
	MINIDUMPWRITEDUMP			fnMiniDumpWriteDump;
	CHAR						searchPath[MAX_PATH], *p, *upMessage;
	CHAR						reportPath[MAX_PATH];
	CHAR						dumpPath[MAX_PATH];
	MINIDUMP_EXCEPTION_INFORMATION miniInfo;
	SYSTEMTIME					timeInfo;
	FILE						*fhReport;
	DWORD64						fnOffset;
	DWORD64						InstructionPtr;
	DWORD						ret, i;
	STACKFRAME64				frame = {0};
	CONTEXT						context = *exceptionInfo->ContextRecord;
	SYMBOL_INFO					*symInfo;
	OSVERSIONINFOEX				osInfo;
	bool						minidumped;
#if USE_CURL
	bool						upload;
#endif

	// Show the mouse cursor
	// Remove the client window
#ifndef DEDICATED_ONLY
	ShowCursor (TRUE);
	if (sys_winInfo.hWnd)
		DestroyWindow (sys_winInfo.hWnd);
#endif

	// Show the console window
	Sys_SetConsoleTitle ("EGL ERROR!");
	Sys_SetErrorText ("Unhandled exception error!");
	Sys_ShowConsole (1, true);

	// Continue searching?
#ifdef _DEBUG
	ret = MessageBox (NULL, "EXCEPTION_CONTINUE_SEARCH?", "Unhandled Exception", MB_ICONERROR|MB_YESNO);
	if (ret == IDYES)
		return EXCEPTION_CONTINUE_SEARCH;
#endif

	// Load needed libraries and get the location of the needed functions
	hDbgHelp = LoadLibrary ("DBGHELP");
	if (!hDbgHelp) {
		MessageBox (NULL, APP_FULLNAME " has encountered an unhandled exception and must be terminated. No crash report could be generated since " APP_FULLNAME " failed to load DBGHELP.DLL. Please obtain DBGHELP.DLL and place it in your EGL directory to enable crash dump generation.", "Unhandled Exception", MB_OK | MB_ICONEXCLAMATION);
		return EXCEPTION_CONTINUE_SEARCH;
	}

	hVersion = LoadLibrary ("VERSION");
	if (hVersion) {
		fnVerQueryValue = (VERQUERYVALUE)GetProcAddress (hVersion, "VerQueryValueA");
		fnGetFileVersionInfo = (GETFILEVERSIONINFO)GetProcAddress (hVersion, "GetFileVersionInfoA");
		fnGetFileVersionInfoSize = (GETFILEVERSIONINFOSIZE)GetProcAddress (hVersion, "GetFileVersionInfoSizeA");
	}

	fnEnumerateLoadedModules64 = (ENUMERATELOADEDMODULES64)GetProcAddress (hDbgHelp, "EnumerateLoadedModules64");
	fnSymSetOptions = (SYMSETOPTIONS)GetProcAddress (hDbgHelp, "SymSetOptions");
	fnSymInitialize = (SYMINITIALIZE)GetProcAddress (hDbgHelp, "SymInitialize");
	fnSymFunctionTableAccess64 = (SYMFUNCTIONTABLEACCESS64)GetProcAddress (hDbgHelp, "SymFunctionTableAccess64");
	fnSymGetModuleBase64 = (SYMGETMODULEBASE64)GetProcAddress (hDbgHelp, "SymGetModuleBase64");
	fnStackWalk64 = (STACKWALK64)GetProcAddress (hDbgHelp, "StackWalk64");
	fnSymFromAddr = (SYMFROMADDR)GetProcAddress (hDbgHelp, "SymFromAddr");
	fnSymCleanup = (SYMCLEANUP)GetProcAddress (hDbgHelp, "SymCleanup");
	fnSymGetModuleInfo64 = (SYMGETMODULEINFO64)GetProcAddress (hDbgHelp, "SymGetModuleInfo64");
	//fnSymLoadModule64 = (SYMLOADMODULE64)GetProcAddress (hDbgHelp, "SymLoadModule64");
	fnMiniDumpWriteDump = (MINIDUMPWRITEDUMP)GetProcAddress (hDbgHelp, "MiniDumpWriteDump");

	if (!fnEnumerateLoadedModules64 || !fnSymSetOptions || !fnSymInitialize || !fnSymFunctionTableAccess64
	|| !fnSymGetModuleBase64 || !fnStackWalk64 || !fnSymFromAddr || !fnSymCleanup || !fnSymGetModuleInfo64) {// || !fnSymLoadModule64) {
		FreeLibrary (hDbgHelp);
		if (hVersion)
			FreeLibrary (hVersion);
		MessageBox (NULL, APP_FULLNAME " has encountered an unhandled exception and must be terminated. No crash report could be generated since " APP_FULLNAME " failed to load DBGHELP.DLL. Please obtain DBGHELP.DLL and place it in your EGL directory to enable crash dump generation.", "Unhandled Exception", MB_OK | MB_ICONEXCLAMATION);
		return EXCEPTION_CONTINUE_SEARCH;
	}

	// Let the user know
	ret = MessageBox (NULL, APP_FULLNAME " has encountered an unhandled exception and must be terminated. Would you like to generate a crash report?", "Unhandled Exception", MB_ICONEXCLAMATION | MB_YESNO);
	if (ret == IDNO) {
		FreeLibrary (hDbgHelp);
		if (hVersion)
			FreeLibrary (hVersion);
		return EXCEPTION_CONTINUE_SEARCH;
	}
	Sys_ConsolePrint ("\nGenerating crash report, please wait...\n");

	// Get the current process
	hProcess = GetCurrentProcess();

	fnSymSetOptions (SYMOPT_UNDNAME | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_LOAD_ANYTHING);

	// Used to determine the directory for dump placement
	GetModuleFileName (NULL, searchPath, sizeof(searchPath));
	p = strrchr (searchPath, '\\');
	if (p)
		*p = '\0';

	// Get the system time
	GetSystemTime (&timeInfo);

	// Find the next filename to use for this dump
	for (i=1 ; ; i++) {
		Q_snprintfz (reportPath, sizeof(reportPath), "%s\\EGLCrashLog%.4d-%.2d-%.2d%_%d.txt", searchPath, timeInfo.wYear, timeInfo.wMonth, timeInfo.wDay, i);
		if (Sys_FileLength (reportPath) == -1)
			break;
	}

	// Open the report dump file
	fhReport = fopen (reportPath, "wb");
	if (!fhReport) {
		FreeLibrary (hDbgHelp);
		if (hVersion)
			FreeLibrary (hVersion);
		return EXCEPTION_CONTINUE_SEARCH;
	}

	// Initialize symbols
	fnSymInitialize (hProcess, searchPath, TRUE);
#ifdef _M_AMD64
	InstructionPtr = context.Rip;
	frame.AddrPC.Offset = InstructionPtr;
	frame.AddrFrame.Offset = context.Rbp;
	frame.AddrPC.Offset = context.Rsp;
#else
	InstructionPtr = context.Eip;
	frame.AddrPC.Offset = InstructionPtr;
	frame.AddrFrame.Offset = context.Ebp;
	frame.AddrStack.Offset = context.Esp;
#endif

	frame.AddrFrame.Mode = AddrModeFlat;
	frame.AddrPC.Mode = AddrModeFlat;
	frame.AddrStack.Mode = AddrModeFlat;

	symInfo = (SYMBOL_INFO*)LocalAlloc (LPTR, sizeof(*symInfo) + 128);
	symInfo->SizeOfStruct = sizeof(SYMBOL_INFO);
	symInfo->MaxNameLen = 128;
	fnOffset = 0;

	// Get OS info
	memset (&osInfo, 0, sizeof(osInfo));
	osInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	if (!GetVersionEx ((OSVERSIONINFO *)&osInfo)) {
		osInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
		GetVersionEx ((OSVERSIONINFO *)&osInfo);
	}

	// Find out which module threw the exception
	strcpy (szModuleName, "<unknown>");
	fnEnumerateLoadedModules64 (hProcess, (PENUMLOADED_MODULES_CALLBACK64)EnumerateLoadedModulesProcInfo, (VOID *)InstructionPtr);
	Q_strlwr (szModuleName);

	if (strstr (szModuleName, "egl.exe") || strstr (szModuleName, "eglded.exe") || strstr (szModuleName, "eglcgamex86")) {
		upMessage =
			"The crash appears to be inside EGL, the EGL dedicated server, or EGL CGame.\r\n"
			"It would be greatly appreciated if you submitted the report.\r\n";
#if USE_CURL
		upload = true;
#endif
	}
	else if (strstr (szModuleName, "pglgamex86")) {
		upMessage =
			"It is very likely that the Game DLL you are using is at fault.\r\n"
			"Please send this crash report to the author(s) of the mod you are using\r\n";
#if USE_CURL
		upload = false;
#endif
	}
	else if (strstr (szModuleName, "anticheat.dll")) {
		upMessage =
			"The crash appears to be inside the anticheat library.\r\n"
			"Please send this crash report to R1CH (www.r1ch.net forums).\r\n";
#if USE_CURL
		upload = false;
#endif
	}
	else {
		upMessage =
			"Unable to detect where the exception occured!\r\n";
#if USE_CURL
		upload = true;
#endif
	}

	// Write out the report to file
	fprintf (fhReport,
		APP_FULLNAME " encountered an unhandled exception and was terminated. If you are\r\n"
		"able to reproduce this crash, please submit the crash report when prompted, or post\r\n"
		"the file on the EGL Bug Reports forum. Goto " EGL_HOMEPAGE " and click on\r\n"
		"Forums then on Bug Reports.\r\n"
		"\r\n"
		"*** PLEASE MAKE SURE THAT YOU ARE USING THE LATEST VERSION OF EGL BEFORE SUBMITTING ***\r\n");

	fprintf (fhReport, "\r\n");

	// Windows information
	fprintf (fhReport, "Windows information:\r\n");
	fprintf (fhReport, "--------------------------------------------------\r\n");
	fprintf (fhReport, "Version:      %d.%d\r\n", osInfo.dwMajorVersion, osInfo.dwMinorVersion);
	fprintf (fhReport, "Build:        %d\r\n", osInfo.dwBuildNumber);
	fprintf (fhReport, "Service Pack: %s\r\n", osInfo.szCSDVersion ? osInfo.szCSDVersion : "none");

	fprintf (fhReport, "\r\n");

	// Exception information
	fprintf (fhReport, "Exception information:\r\n");
	fprintf (fhReport, "--------------------------------------------------\r\n");
	fprintf (fhReport, "Code:    %x\r\n", exceptionCode);
	fprintf (fhReport, "Address: %I64p\r\n", InstructionPtr);
	fprintf (fhReport, "Module:  %s\r\n", szModuleName);

	fprintf (fhReport, "\r\n");

	// Symbol information
	fprintf (fhReport, "Symbol information:\r\n");
	fprintf (fhReport, "--------------------------------------------------\r\n");
	fnEnumerateLoadedModules64 (hProcess, (PENUMLOADED_MODULES_CALLBACK64)EnumerateLoadedModulesProcSymInfo, (VOID *)fhReport);

	fprintf (fhReport, "\r\n");

	// Loaded modules
	fprintf (fhReport, "Loaded modules:\r\n");
	fprintf (fhReport, "--------------------------------------------------\r\n");
	fnEnumerateLoadedModules64 (hProcess, (PENUMLOADED_MODULES_CALLBACK64)EnumerateLoadedModulesProcDump, (VOID *)fhReport);

	fprintf (fhReport, "\r\n");

	// Stack trace
	fprintf (fhReport, "Stack trace:\r\n");
	fprintf (fhReport, "--------------------------------------------------\r\n");
	fprintf (fhReport, "Stack    EIP      Arg0     Arg1     Arg2     Arg3     Address\r\n");
	while (fnStackWalk64 (IMAGE_FILE_MACHINE_I386, hProcess, GetCurrentThread(), &frame, &context, NULL, (PFUNCTION_TABLE_ACCESS_ROUTINE64)fnSymFunctionTableAccess64, (PGET_MODULE_BASE_ROUTINE64)fnSymGetModuleBase64, NULL)) {
		strcpy (szModuleName, "<unknown>");
		fnEnumerateLoadedModules64 (hProcess, (PENUMLOADED_MODULES_CALLBACK64)EnumerateLoadedModulesProcInfo, (VOID *)(DWORD)frame.AddrPC.Offset);

		p = strrchr (szModuleName, '\\');
		if (p) {
			p++;
		}
		else {
			p = szModuleName;
		}

		if (fnSymFromAddr (hProcess, frame.AddrPC.Offset, &fnOffset, symInfo) && !(symInfo->Flags & SYMFLAG_EXPORT)) {
			fprintf (fhReport, "%I64p %I64p %p %p %p %p %s!%s+0x%I64x\r\n", frame.AddrStack.Offset, frame.AddrPC.Offset, (DWORD)frame.Params[0], (DWORD)frame.Params[1], (DWORD)frame.Params[2], (DWORD)frame.Params[3], p, symInfo->Name, fnOffset, symInfo->Tag);
		}
		else {
			fprintf (fhReport, "%I64p %I64p %p %p %p %p %s!0x%I64p\r\n", frame.AddrStack.Offset, frame.AddrPC.Offset, (DWORD)frame.Params[0], (DWORD)frame.Params[1], (DWORD)frame.Params[2], (DWORD)frame.Params[3], p, frame.AddrPC.Offset);
		}
	}

	fprintf (fhReport, "\r\n");

	// Write a minidump
	minidumped = false;
	if (fnMiniDumpWriteDump) {
		HANDLE	hFile;

		GetTempPath (sizeof(dumpPath)-16, dumpPath);
		strcat (dumpPath, "EGLCrashDump.dmp");

		hFile = CreateFile (dumpPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile != INVALID_HANDLE_VALUE) {
			miniInfo.ClientPointers = TRUE;
			miniInfo.ExceptionPointers = exceptionInfo;
			miniInfo.ThreadId = GetCurrentThreadId ();
			if (fnMiniDumpWriteDump (hProcess, GetCurrentProcessId(), hFile, (MINIDUMP_TYPE)(MiniDumpNormal|MiniDumpWithIndirectlyReferencedMemory|MiniDumpWithDataSegs), &miniInfo, NULL, NULL)) {
				FILE	*fh;
				CHAR	zPath[MAX_PATH];

				CloseHandle (hFile);

				fh = fopen (dumpPath, "rb");
				if (fh) {
					
					BYTE	buff[0xFFFF];
					size_t	len;
					gzFile	gz;

					snprintf (zPath, sizeof(zPath)-1, "%s\\PGLCrashLog%.4d-%.2d-%.2d_%d.dmp.gz", searchPath, timeInfo.wYear, timeInfo.wMonth, timeInfo.wDay, i);
					gz = gzopen (zPath, "wb");
					if (gz) {
						while ((len = fread (buff, 1, sizeof(buff), fh)) > 0)
							gzwrite (gz, buff, (unsigned int)len);
						gzclose (gz);
						fclose (fh);
					}
				}
		
				DeleteFile (dumpPath);
				strcpy (dumpPath, zPath);
				fprintf (fhReport, "A minidump was saved to %s.\r\nPlease include this file when posting a crash report.\r\n", dumpPath);
				minidumped = true;
			}
			else {
				CloseHandle (hFile);
				DeleteFile (dumpPath);
			}
		}
	}
	else {
		fprintf (fhReport, "A minidump could not be created. Minidumps are only available on Windows XP or later.\r\n");
	}

	// Done writing reports
	fclose (fhReport);
	LocalFree (symInfo);
	fnSymCleanup (hProcess);

	// Let the client know
	Sys_ConsolePrint (Q_VarArgs ("Report written to: %s\nMini-dump written to %s\nPlease include both files if you submit manually!\n", reportPath, dumpPath));

#if USE_CURL
	if (upload) {
		ret = MessageBox (NULL, "Would you like to automatically upload this crash report for analysis?\nPlease do not submit multiple reports of the same crash as this will only delay processing.", "Unhandled Exception", MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2);
		if (ret == IDYES)
			EGLUploadCrashDump (dumpPath, reportPath);
		else
			MessageBox (NULL, "You have chosen to manually upload the crash report.\nPlease include BOTH the crash log and mini-dump when you post it on the EGL forums!\n", "Submit manually", MB_ICONEXCLAMATION|MB_OK);
	}
	else {
		MessageBox (NULL, upMessage, "Unhandled Exception", MB_OK|MB_ICONEXCLAMATION);
	}
#endif

	// Done
	FreeLibrary (hDbgHelp);
	if (hVersion)
		FreeLibrary (hVersion);

	return EXCEPTION_EXECUTE_HANDLER;
}

/*
==============================================================================

	MAIN WINDOW LOOP

==============================================================================
*/

/*
==================
WinMain
==================
*/
static void ParseCommandLine(LPSTR lpCmdLine)
{
	sys_argCnt = 1;
	sys_argVars[0] = "exe";

	while (*lpCmdLine && sys_argCnt < MAX_NUM_ARGVS)
	{
		while (*lpCmdLine && *lpCmdLine <= 32 || *lpCmdLine > 126)
			lpCmdLine++;

		if (*lpCmdLine)
		{
			sys_argVars[sys_argCnt] = lpCmdLine;
			sys_argCnt++;

			while (*lpCmdLine && *lpCmdLine > 32 && *lpCmdLine <= 126)
				lpCmdLine++;

			if (*lpCmdLine)
			{
				*lpCmdLine = 0;
				lpCmdLine++;
			}
			
		}
	}

}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	// Previous instances do not exist in Win32
	if (hPrevInstance)
		return 0;

	// Make sure the timer is high precision, otherwise NT gets 18ms resolution
	timeBeginPeriod(1);
	sys_timeBase = timeGetTime() & 0xffff0000;

	// This is later multiplied times a cycle duration to get the CPU time in MS
	{
		LARGE_INTEGER PF;
		QueryPerformanceFrequency(&PF);
		sys_msPerCycle = (1.0 / (double)PF.QuadPart) * 1000.0;
	}

	sys_winInfo.hInstance = hInstance;

	// Create the console
	Sys_CreateConsole();

	// Parse the command line
	ParseCommandLine(lpCmdLine);

	// No abort/retry/fail errors
	SetErrorMode(SEM_FAILCRITICALERRORS);

	// If we find the CD, add a +set cddirxxx command line
	if (sys_argCnt < MAX_NUM_ARGVS-3)
	{
		int i;
		for (i=0 ; i<sys_argCnt ; i++)
			if (!strcmp (sys_argVars[i], "cddir"))
				break;

		// Don't override a cddir on the command line
		if (i == sys_argCnt)
		{
			char	*cddir = Sys_ScanForCD();
			if (cddir)
			{
				sys_argVars[sys_argCnt++] = "+set";
				sys_argVars[sys_argCnt++] = "cddir";
				sys_argVars[sys_argCnt++] = cddir;
			}
		}
	}

	__try
	{
		// Common intialization
		Com_Init (sys_argCnt, sys_argVars);

#ifndef DEDICATED_ONLY
		// Show the console
		if (!dedicated->intVal)
			Sys_ShowConsole (0, false);
#endif

		// Pump message loop
		Sys_SendKeyEvents();

		// Initial loop
		int oldTime = Sys_Milliseconds();
		int newTime, timeDelta;
		for ( ; ; )
		{
			// If at a full screen console, don't update unless needed
#ifndef DEDICATED_ONLY
			if (sys_winInfo.appMinimized || dedicated->intVal)
				Sleep(5);
#else
			if (sys_winInfo.appMinimized)
				Sleep(5);
#endif

			for ( ; ; )
			{
				newTime = Sys_Milliseconds();
				timeDelta = newTime - oldTime;
				if (!timeDelta)
				{
					Sleep(0);
				}
				else
				{
					break;
				}
			}

			Com_Frame(timeDelta);

			oldTime = newTime;
		}
	}
	__except (EGLExceptionHandler(GetExceptionCode(), GetExceptionInformation()))
	{
		return 1;
	}

	// Never gets here
	return 0;
}
