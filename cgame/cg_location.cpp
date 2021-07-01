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
// cg_location.c
//

#include "cg_local.h"

struct cg_location_t {
	cg_location_t			*next;

	char					*name;
	vec3_t					location;
};

static cg_location_t		*cg_locationList;
static char					cg_locFileName[MAX_QPATH];

/*
=============================================================================

	LOADING

=============================================================================
*/

/*
====================
CG_FreeLocations

Called on map change, full shutdown, and to release a bad loc file
====================
*/
static void CG_FreeLocations ()
{
	if (cg_locationList) {
		cg_locationList->next = NULL;
		cg_locationList = NULL;
	}

	// Empty memory in pool
	CG_FreeTag (CGTAG_LOCATION);
}


/*
====================
CG_LoadLocations

Called on map change or location addition to reload the file
====================
*/
void CG_LoadLocations (char *mapName)
{
	cg_location_t	*loc;
	vec3_t			location;
	bool			finished;
	char			*buf;
	char			*token;
	int				fileLen;

	CG_FreeLocations ();

	if (!mapName || !mapName[0])
		return;

	Com_StripExtension (cg_locFileName, sizeof(cg_locFileName), mapName);
	Q_snprintfz (cg_locFileName, sizeof(cg_locFileName), "%s.loc", cg_locFileName);

	fileLen = cgi.FS_LoadFile (cg_locFileName, (void **)&buf, true);
	if (!buf || fileLen <= 0) {
		Com_DevPrintf (PRNT_WARNING, "WARNING: can't load '%s' -- %s\n", cg_locFileName, (fileLen == -1) ? "not found" : "empty file");
		return;
	}

	token = strtok (buf, "\t ");
	finished = false;
	while (token) {
		finished = false;

		// X coordinate
		location[0] = (float)atoi (token)*0.125f;

		// Y coordinate
		token = strtok (NULL, "\t ");
		if (!token)
			break;
		location[1] = (float)atoi (token)*0.125f;

		// Z coordinate
		token = strtok (NULL, "\t ");
		if (!token)
			break;
		location[2] = (float)atoi (token)*0.125f;

		// Location
		token = strtok (NULL, "\n\r");
		if (!token)
			break;

		// Allocate
		loc = (cg_location_t*)CG_AllocTag (sizeof(cg_location_t), CGTAG_LOCATION);
		loc->name = CG_TagStrDup (token, CGTAG_LOCATION);
		Vec3Copy (location, loc->location);

		// Link it in
		loc->next = cg_locationList;
		cg_locationList = loc;

		// Go to the next x coord
		token = strtok (NULL, "\n\r\t ");
		if (!token)
			finished = true;
	}

	if (!finished) {
		Com_Printf (PRNT_ERROR, "CG_LoadLocations: Bad loc file '%s'\n", cg_locFileName);
		CG_FreeLocations ();
	}

	CG_FS_FreeFile (buf);
}

/*
=============================================================================

	MESSAGE PREPROCESSING

=============================================================================
*/

/*
====================
CG_GetLocation

Grabs the location for the message preprocessing
====================
*/
static char *CG_GetLocation (vec3_t where)
{
	cg_location_t	*loc, *best;
	uint32			length, bestLength = 0xFFFFFFFF;

	best = NULL;
	for (loc=cg_locationList ; loc ; loc=loc->next) {
		length = Vec3Dist (loc->location, where);
		if (length < bestLength && loc->name && loc->name[0]) {
			best = loc;
			bestLength = length;
		}
	}

	if (best)
		return best->name;

	return NULL;
}


/*
====================
CG_Say_Preprocessor
====================
*/
void CG_Say_Preprocessor ()
{
	char		*locName, *p;
	char		*sayText;
	int			locLen, cmdLen, c;
	cmTrace_t	tr;
	vec3_t		end;

	sayText = p = cgi.Cmd_ArgsOffset (1);
	while (*sayText && *(sayText+1)) {
		if (sayText[0] == '@') {
			c = Q_tolower (sayText[1]);

			locName = NULL;
			switch (c) {
			case 't':
				if (cg_locationList) {
					// Trace and find the end-point for the location
					end[0] = cg.refDef.viewOrigin[0] + cg.refDef.viewAxis[0][0] * 65536 - cg.refDef.viewAxis[1][0];
					end[1] = cg.refDef.viewOrigin[1] + cg.refDef.viewAxis[0][1] * 65536 - cg.refDef.viewAxis[1][1];
					end[2] = cg.refDef.viewOrigin[2] + cg.refDef.viewAxis[0][2] * 65536 - cg.refDef.viewAxis[1][2];
					CG_PMTrace (&tr, cg.refDef.viewOrigin, NULL, NULL, end, false, true);
					if (tr.fraction < 1) {
						locName = CG_GetLocation (tr.endPos);
						if (locName)
							break;
					}
				}
				else {
					locName = "there";
					break;
				}

			// FALL THROUGH
			case 'l':
				// Local location
				if (cg_locationList) {
					locName = CG_GetLocation (cg.refDef.viewOrigin);
				}
				else {
					locName = "here";
				}
				break;
			}

			// Insert if found
			if (locName) {
				// Lengths
				cmdLen = strlen (cgi.Cmd_ArgsOffset (1));
				locLen = strlen (locName);

				// Check if it will fit
				if (cmdLen + locLen >= MAX_STRING_CHARS) {
					Com_DevPrintf (0, "CG_Say_Preprocessor: location expansion aborted, not enough space\n");
					break;
				}

				// Insert
				memmove (sayText+locLen, sayText+2, cmdLen-(sayText-p)-1);
				memcpy (sayText, locName, locLen);
				sayText += locLen-1;
			}
		}
		sayText++;
	}

	if (cgi.CL_ForwardCmdToServer ())
		return;

	// Command unknown (shouldn't honestly happen but oh well)
	Com_Printf (0, "Unknown command \"%s" S_STYLE_RETURN "\"\n", cgi.Cmd_Argv (0));
}

/*
=============================================================================

	CONSOLE COMMANDS

=============================================================================
*/

/*
====================
CG_AddLoc_f
====================
*/
static void CG_AddLoc_f ()
{
	FILE	*f;
	char	path[MAX_QPATH];

	if (cgi.Cmd_Argc () < 2) {
		Com_Printf (0, "syntax: addloc <message>\n");
		return;
	}

	if (!cg_locFileName[0]) {
		Com_Printf (0, "CG_AddLoc_f: No map loaded!\n");
		return;
	}

	Q_snprintfz (path, sizeof(path), "%s/%s", cgi.FS_Gamedir (), cg_locFileName);

	f = fopen (path, "ab");
	if (!f) {
		Com_Printf (PRNT_ERROR, "ERROR: Couldn't write %s\n", path);
		return;
	}

	fprintf (f, "%i %i %i %s\n",
		(int)(cg.refDef.viewOrigin[0]*8),
		(int)(cg.refDef.viewOrigin[1]*8),
		(int)(cg.refDef.viewOrigin[2]*8),
		cgi.Cmd_ArgsOffset (1));

	fclose (f);

	// Tell them
	Com_Printf (0, "Saved location (x%i y%i z%i): \"%s\"\n",
		(int)cg.refDef.viewOrigin[0],
		(int)cg.refDef.viewOrigin[1],
		(int)cg.refDef.viewOrigin[2],
		cgi.Cmd_ArgsOffset (1));

	CG_LoadLocations (cg_locFileName);
}

/*
=============================================================================

	INIT / SHUTDOWN

=============================================================================
*/

static conCmd_t	*cmd_addLoc;

/*
====================
CG_LocationInit
====================
*/
void CG_LocationInit()
{
	// Clear locations
	CG_FreeLocations();

	// Add console commands
	cmd_addLoc	= cgi.Cmd_AddCommand("addloc",		0, CG_AddLoc_f,		"");

	// Clear the loc filename
	cg_locFileName[0] = '\0';
}


/*
====================
CG_LocationShutdown
====================
*/
void CG_LocationShutdown()
{
	// Free locations
	CG_FreeLocations();

	// Remove console commands
	cgi.Cmd_RemoveCommand(cmd_addLoc);
}
