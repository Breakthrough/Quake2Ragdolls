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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

//
// unix_main.c
//

#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <errno.h>
#include <dlfcn.h>
#include <dirent.h>

#include "../common/common.h"
#include "unix_local.h"

#if defined(__FreeBSD__)
#include <machine/param.h>
#endif

uid_t	saved_euid;

#ifndef DEDICATED_ONLY
void X11_Shutdown (void);
#endif

/* Like glob_match, but match PATTERN against any final segment of TEXT.  */
static int glob_match_after_star (char *pattern, char *text)
{
	register char *p = pattern, *t = text;
	register char c, c1;

	while ((c = *p++) == '?' || c == '*')
		if (c == '?' && *t++ == '\0')
			return 0;

	if (c == '\0')
		return 1;

	if (c == '\\')
		c1 = *p;
	else
		c1 = c;

	while (1) {
		if ((c == '[' || *t == c1) && glob_match(p - 1, t))
			return 1;
		if (*t++ == '\0')
			return 0;
	}
}

#if 0 /* Not used */
/* Return nonzero if PATTERN has any special globbing chars in it.  */
static int glob_pattern_p (char *pattern)
{
	register char *p = pattern;
	register char c;
	int open = 0;

	while ((c = *p++) != '\0')
		switch (c) {
		case '?':
		case '*':
			return 1;

		case '[':		/* Only accept an open brace if there is a close */
			open++;		/* brace to match it.  Bracket expressions must be */
			continue;	/* complete, according to Posix.2 */
		case ']':
			if (open)
				return 1;
			continue;

		case '\\':
			if (*p++ == '\0')
				return 0;
		}

	return 0;
}
#endif

/* Match the pattern PATTERN against the string TEXT;
   return 1 if it matches, 0 otherwise.

   A match means the entire string TEXT is used up in matching.

   In the pattern string, `*' matches any sequence of characters,
   `?' matches any character, [SET] matches any character in the specified set,
   [!SET] matches any character not in the specified set.

   A set is composed of characters or ranges; a range looks like
   character hyphen character (as in 0-9 or A-Z).
   [0-9a-zA-Z_] is the set of characters allowed in C identifiers.
   Any other character in the pattern must be matched exactly.

   To suppress the special syntactic significance of any of `[]*?!-\',
   and match the character exactly, precede it with a `\'.
*/

int glob_match (char *pattern, char *text)
{
	register char *p = pattern, *t = text;
	register char c;

	while ((c = *p++) != '\0')
		switch (c) {
		case '?':
			if (*t == '\0')
				return 0;
			else
				++t;
			break;

		case '\\':
			if (*p++ != *t++)
				return 0;
			break;

		case '*':
			return glob_match_after_star(p, t);

		case '[':
			{
				register char c1 = *t++;
				int invert;

				if (!c1)
					return (0);

				invert = ((*p == '!') || (*p == '^'));
				if (invert)
					p++;

				c = *p++;
				while (1) {
					register char cstart = c, cend = c;

					if (c == '\\') {
						cstart = *p++;
						cend = cstart;
					}
					if (c == '\0')
						return 0;

					c = *p++;
					if (c == '-' && *p != ']') {
						cend = *p++;
						if (cend == '\\')
							cend = *p++;
						if (cend == '\0')
							return 0;
						c = *p++;
					}
					if (c1 >= cstart && c1 <= cend)
						goto match;
					if (c == ']')
						break;
				}
				if (!invert)
					return 0;
				break;

			  match:
				/* Skip the rest of the [...] construct that already matched.  */
				while (c != ']') {
					if (c == '\0')
						return 0;
					c = *p++;
					if (c == '\0')
						return 0;
					else if (c == '\\')
						++p;
				}
				if (invert)
					return 0;
				break;
			}

		default:
			if (c != *t++)
				return 0;
		}

	return *t == '\0';
}


// ========================================


/*
================
Sys_Print
================
*/

void Sys_Print (const char *message)
{
  Conbuf_AppendText (message);
}


/*
================
Sys_Init
================
*/
void Sys_Init (void)
{
}


/*
================
Sys_Quit
================
*/
void Sys_Quit (qBool error)
{
#ifndef DEDICATED_ONLY
	if (!dedicated->intVal)
		CL_ClientShutdown (error);

  X11_Shutdown ();
#endif
  Com_Shutdown ();
  fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);
  exit(0);
}


/*
================
Sys_Error
================
*/
void Sys_Error (char *error, ...)
{
    va_list     argptr;
    char        string[1024];

	// change stdin to non blocking
    fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);

#ifndef DEDICATED_ONLY
	CL_ClientShutdown (qTrue);
#endif
	Com_Shutdown ();
    
    va_start (argptr, error);
	vsnprintf (string, sizeof (string), error, argptr);
    va_end (argptr);
	fprintf (stderr, "Error: %s\n", string);

	exit (1);
}


/*
================
Sys_Warn
================
*/
void Sys_Warn (char *warning, ...)
{
    va_list     argptr;
    char        string[1024];
    
    va_start (argptr, warning);
	vsnprintf (string, sizeof (string), warning, argptr);
    va_end (argptr);
	fprintf (stderr, "Warning: %s", string);
} 


/*
============
Sys_FileTime

returns -1 if not present
============
*/
int Sys_FileTime (char *path)
{
	struct	stat	buf;
	
	if (stat (path,&buf) == -1)
		return -1;
	
	return buf.st_mtime;
}


/*
================
Sys_Milliseconds
================
*/
int Sys_Milliseconds (void)
{
	struct timeval	tp;
	struct timezone	tzp;
	static int		secbase;

	gettimeofday (&tp, &tzp);

	if (!secbase) {
		secbase = tp.tv_sec;
		return tp.tv_usec/1000;
	}
	
	return (tp.tv_sec - secbase)*1000 + tp.tv_usec/1000;
}


/*
================
Sys_UMilliseconds
================
*/
uint32 Sys_UMilliseconds (void)
{
	struct timeval	tp;
	struct timezone	tzp;
	static int		secbase;

	gettimeofday (&tp, &tzp);

	if (!secbase) {
		secbase = tp.tv_sec;
		return tp.tv_usec/1000;
	}
	
	return (tp.tv_sec - secbase)*1000 + tp.tv_usec/1000;
}


/*
================
Sys_AppActivate
================
*/
void Sys_AppActivate (void)
{
}


/*
================
Sys_Mkdir
================
*/
void Sys_Mkdir (char *path)
{
    mkdir (path, 0777);
}


/*
================
Sys_SendKeyEvents
================
*/
void Sys_SendKeyEvents (void)
{
#ifndef DEDICATED_ONLY
	// Update the client
	if (!dedicated->intVal)
		CL_UpdateFrameTime (Sys_Milliseconds ());
#endif
}


/*
================
Sys_GetClipboardData
================
*/
char *Sys_GetClipboardData (void)
{
	return NULL;
}


// =====================================================================

static	char	findbase[MAX_OSPATH];
static	char	findpath[MAX_OSPATH];
static	char	findpattern[MAX_OSPATH];
static	DIR		*fdir;

static qBool CompareAttributes(char *path, char *name, unsigned musthave, unsigned canthave) {
	struct stat st;
	char fn[MAX_OSPATH];

	// '.' and '..' never match
	if ((strcmp (name, ".") == 0) || (strcmp (name, "..") == 0))
		return qFalse;

	return qTrue;

	if (stat(fn, &st) == -1)
		return qFalse; // shouldn't happen

	if ((st.st_mode & S_IFDIR) && (canthave & SFF_SUBDIR))
		return qFalse;

	if ((musthave & SFF_SUBDIR) && !(st.st_mode & S_IFDIR))
		return qFalse;

	return qTrue;
}


/*
==================
Sys_FindClose
==================
*/
void Sys_FindClose (void) {
	if (fdir != NULL)
		closedir (fdir);

	fdir = NULL;
}


/*
==================
Sys_FindFirst
==================
*/
char *Sys_FindFirst (char *path, unsigned musthave, unsigned canhave) {
	struct dirent *d;
	char *p;

	if (fdir)
		Sys_Error ("Sys_BeginFind without close");

	strcpy (findbase, path);

	if ((p = strrchr (findbase, '/')) != NULL) {
		*p = 0;
		strcpy (findpattern, p + 1);
	} else
		strcpy (findpattern, "*");

	if (strcmp (findpattern, "*.*") == 0)
		strcpy (findpattern, "*");
	
	if ((fdir = opendir (findbase)) == NULL)
		return NULL;
	while ((d = readdir (fdir)) != NULL) {
		if (!*findpattern || glob_match (findpattern, d->d_name)) {
			if (CompareAttributes (findbase, d->d_name, musthave, canhave)) {
				sprintf (findpath, "%s/%s", findbase, d->d_name);
				return findpath;
			}
		}
	}
	return NULL;
}


/*
==================
Sys_FindNext
==================
*/
char *Sys_FindNext (unsigned musthave, unsigned canhave) {
	struct dirent *d;

	if (fdir == NULL)
		return NULL;

	d = readdir(fdir);
	while (d) {
		if (!*findpattern || glob_match (findpattern, d->d_name)) {
			if (CompareAttributes (findbase, d->d_name, musthave, canhave)) {
				sprintf (findpath, "%s/%s", findbase, d->d_name);
				return findpath;
			}
		}

		d = readdir(fdir);
	}
	return NULL;
}


/*
================
Sys_FindFiles
================
*/
typedef struct Sys_FindFiles_s {
  char		path[MAX_OSPATH];
  int		pathlen;
  char		*pattern;
  int		addmask;
  char		**list;
  int		max;
} Sys_FindFiles_t;

static void Sys_FindFiles_R (Sys_FindFiles_t *f)
{
	DIR				*dir;
	struct dirent	*ent;
	struct stat		st;
	int				ll = f->pathlen;
	int				dl;

	if (!(dir = opendir (f->path)))
		return;

	while ((ent = readdir(dir)) && f->max) {
		if (ent->d_name[0] != '.' && ll+(dl=strlen(ent->d_name)+1) < sizeof(f->path)) {
			strcat (f->path, ent->d_name);
			if (!stat (f->path, &st)) {
				if ((f->addmask & 1 && S_ISREG(st.st_mode) || f->addmask & 2 && S_ISDIR(st.st_mode)) && Q_WildcardMatch (f->pattern, f->path, 1)) {
					*(f->list++) = Mem_StrDup(f->path);
					f->max--;
				}

				if (f->addmask & 4 && S_ISDIR(st.st_mode)) {
					strcat(f->path, "/");
					f->pathlen += dl;
					Sys_FindFiles_R (f);
					f->pathlen -= dl;
				}
			}
			f->path[ll] = 0;
		}
	}

	closedir (dir);
}

int Sys_FindFiles (char *path, char *pattern, char **fileList, int maxFiles, int fileCount, qBool recurse, qBool addFiles, qBool addDirs)
{
	Sys_FindFiles_t	f;

	fileList += fileCount;

	if (!(f.pathlen = strlen(path)))
		return 0;
	if (f.pathlen > sizeof(f.path)-2)
		return 0;

	strcpy (f.path, path);
	if (f.path[f.pathlen-1] != '/') {
		f.path[f.pathlen++] = '/';
		f.path[f.pathlen] = 0;
	}

	f.pattern = pattern;
	f.addmask = (addFiles?1:0) | (addDirs?2:0) | (recurse?4:0);
	f.list = fileList;
	f.max = maxFiles;

	Sys_FindFiles_R (&f);

	return maxFiles-f.max;
}

/*
========================================================================

	LIBRARY MANAGEMENT

========================================================================
*/

#if defined __i386__
# define LIBARCH "i386"
#elif defined __alpha__
# define LIBARCH "axp"
#elif defined __powerpc__
# define LIBARCH "axp"
#elif defined __sparc__
# define LIBARCH "sparc"
#else
# define LIBARCH
#endif

typedef struct libList_s {
	const char		*title;
	void			*lib;
	const char		*fileName;
	const char		*apiFuncName;
} libList_t;

static libList_t sys_libList[LIB_MAX] = {
	{ "LIB_CGAME",	NULL,	"eglcgame" LIBARCH ".so",	"GetCGameAPI"	},
	{ "LIB_GAME",	NULL,	"game" LIBARCH ".so",		"GetGameAPI"	},
};

/*
=================
Sys_UnloadLibrary
=================
*/
void Sys_UnloadLibrary (libType_t libType)
{
	void	**lib;

	// Sanity check
	if (libType < 0 || libType > LIB_MAX)
		Com_Error (ERR_FATAL, "Sys_UnloadLibrary: Bad libType (%i)", libType);

	// Find the library
	lib = &sys_libList[libType].lib;

	// Free it
	Com_DevPrintf (0, "dlclose (%s)\n", sys_libList[libType].title);
	if (dlclose (*lib))
		Com_Error (ERR_FATAL, "dlclose (%s) failed", sys_libList[libType].title);

	*lib = NULL;
}


/*
=================
Sys_LoadLibrary
=================
*/
void *Sys_LoadLibrary (libType_t libType, void *parms)
{
	char		name[MAX_OSPATH];
	char		cwd[MAX_OSPATH];
	char		*path;
	void		*(*APIfunc) (void *);
	void		**lib;
	const char	*libName;
	char		*error;

#if defined __i386__
#ifdef NDEBUG
	const char *debugdir = "releasei386";
#else
	const char *debugdir = "debugi386";
#endif
#elif defined __alpha__
#ifdef NDEBUG
	const char *debugdir = "releaseaxp";
#else
	const char *debugdir = "debugaxp";
#endif

#elif defined __powerpc__
#ifdef NDEBUG
	const char *debugdir = "releaseppc";
#else
	const char *debugdir = "debugppc";
#endif
#elif defined __sparc__
#ifdef NDEBUG
	const char *debugdir = "releasepsparc";
#else
	const char *debugdir = "debugpsparc";
#endif
#else
#ifdef NDEBUG
	const char *debugdir = "release";
#else
	const char *debugdir = "debug";
#endif
#endif

	// Sanity check
	if (libType < 0 || libType > LIB_MAX)
		Com_Error (ERR_FATAL, "Sys_UnloadLibrary: Bad libType (%i)", libType);

	// Find the library
	lib = &sys_libList[libType].lib;
	libName = sys_libList[libType].fileName;

	// Make sure it's not already loaded first
	if (*lib)
		Com_Error (ERR_FATAL, "Sys_LoadLibrary (%s) without Sys_UnloadLibrary", sys_libList[libType].title);

	// Check the current debug directory first for development purposes
	getcwd (cwd, sizeof(cwd));
	Q_snprintfz (name, sizeof(name), "%s/%s/%s", cwd, debugdir, libName);
	*lib = dlopen (name,  RTLD_NOW);

	if (*lib) {
		Com_DevPrintf (0, "dlopen (%s)\n", name);
	}
	else {
		// Now run through the search paths
		path = NULL;

		for ( ; ; ) {
			path = FS_NextPath (path);
			if (!path) 
				return NULL; // couldn't find one anywhere

			Q_snprintfz (name, sizeof(name), "%s/%s", path, libName);
			dlerror ();
			*lib = dlopen (name, RTLD_NOW);

			if (*lib) {
				Com_DevPrintf (0, "dlopen (%s)\n", name);
				break;
			}
#if 0	// FIXME: has issues
			if (!*lib && error = dlerror())
				Com_Printf (PRNT_ERROR, "dlopen (%s): %s\n", name, error);
#endif
		}
	}

	// Find the API function
	APIfunc = (void *)dlsym (*lib, sys_libList[libType].apiFuncName);
	if (!APIfunc) {
		Com_Error (ERR_FATAL, "dlopen (%s): does not provide %s \n", name, sys_libList[libType].apiFuncName);
		Sys_UnloadLibrary (libType);
		return NULL;
	}

	return APIfunc (parms);
}

/*
========================================================================

	MAIN LOOP

========================================================================
*/


static int trapped_signals[] = {
  SIGHUP,
  SIGINT,
  SIGQUIT,
  SIGILL,
  SIGTRAP,
  SIGIOT,
  SIGBUS,
  SIGFPE,
  SIGSEGV,
  SIGTERM,
  0
};

static char *trapped_signames[] = {
  "SIGHUP",
  "SIGINT",
  "SIGQUIT",
  "SIGILL",
  "SIGTRAP",
  "SIGIOT",
  "SIGBUS",
  "SIGFPE",
  "SIGSEGV",
  "SIGTERM"
};


static void signal_handler (int sig)
{
	int i;

	// Avoid possible infinite loops if signal is triggered again here
	signal (sig, SIG_DFL);

	for (i=0 ; trapped_signals[i] ; i++) {
		if (trapped_signals[i] == sig) {
			printf ("Received signal %s, exiting...\n", trapped_signames[i]);
			sig = 0;
			break;
		}
	}

	// This shouldn't happen
	if (sig)
		printf ("Received signal %d, exiting...\n", sig);

#ifndef DEDICATED_ONLY
	GLimp_Shutdown ();
	X11_Shutdown ();
#endif

	exit (sig);
}

static void InitSig (void) 
{
	int i;
	for (i=0 ; trapped_signals[i] ; i++)
		signal (trapped_signals[i], signal_handler);
}


/*
==================
main
==================
*/
int main (int argc, char **argv)
{
	int		time, oldTime, newTime;

	// Go back to real user for config loads
	saved_euid = geteuid ();
	seteuid (getuid ());

	Com_Init (argc, argv);

	Sys_InitConsole ();

	InitSig ();

	oldTime = Sys_Milliseconds ();
	for ( ; ; ) {
		// Find time spent rendering last frame
		do {
			newTime = Sys_Milliseconds ();
			time = newTime - oldTime;
		} while (time < 1);

		Com_Frame (time);
		oldTime = newTime;
	}
}
