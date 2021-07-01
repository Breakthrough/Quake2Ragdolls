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
// m_mp_join.c
//

#include "m_local.h"

/*
=============================================================================

	JOIN SERVER MENU

=============================================================================
*/

struct m_joinServerMenu_t {
	// Menu items
	uiFrameWork_t		frameWork;

	uiImage_t			banner;

	uiAction_t			addressBookAction;
	uiAction_t			bookServersAction;
	uiAction_t			localServersAction;

	uiAction_t			nameSortAction;
	uiAction_t			gameSortAction;
	uiAction_t			mapSortAction;
	uiAction_t			playerSortAction;
	uiAction_t			pingSortAction;

	uiAction_t			hostNames[MAX_LOCAL_SERVERS];
	uiAction_t			gameNames[MAX_LOCAL_SERVERS];
	uiAction_t			serverMap[MAX_LOCAL_SERVERS];
	uiAction_t			serverPlayers[MAX_LOCAL_SERVERS];
	uiAction_t			serverPing[MAX_LOCAL_SERVERS];

	uiAction_t			backAction;
	uiAction_t			refreshAction;
	uiAction_t			playAction;
};

static m_joinServerMenu_t m_joinServerMenu;

static void JoinServerMenu_Init (bool sort);

// ==========================================================================

struct serverItem_t {
	char		*mapName;
	char		*hostName, *shortName;
	char		*gameName;
	char		*netAddress;

	char		*playersStr;
	int			numPlayers;
	int			maxPlayers;

	char		*pingString;
	int			ping;

	bool		statusPacket;
};

static int			totalServers;
static serverItem_t	sortedServers[MAX_LOCAL_SERVERS];

static int		pingTime;

#define MAX_HOSTNAME_LEN	32
#define MAX_GAMENAME_LEN	10
#define MAX_MAPNAME_LEN		16
#define MAX_PING_LEN		10

enum {
	JS_SORT_HOSTNAME,
	JS_SORT_GAMENAME,
	JS_SORT_MAPNAME,
	JS_SORT_PLAYERCNT,
	JS_SORT_PINGCNT
};

enum {
	JS_PAGE_ADDRBOOK,
	JS_PAGE_LAN
};

/*
=============
UI_FreeServer
=============
*/
static void UI_FreeServer (serverItem_t *server)
{
	if (server->mapName)			CG_MemFree (server->mapName);
	if (server->hostName)			CG_MemFree (server->hostName);
	if (server->shortName)			CG_MemFree (server->shortName);
	if (server->gameName)			CG_MemFree (server->gameName);
	if (server->netAddress)			CG_MemFree (server->netAddress);
	if (server->playersStr)			CG_MemFree (server->playersStr);
	if (server->pingString)			CG_MemFree (server->pingString);
	memset (server, 0, sizeof(serverItem_t));
}


/*
=============
UI_DupeCheckServerList

Checks for duplicates and returns true if there is one...
Since status has higher priority than info, if there is already an instance and
it's not status, and the current one is status, the old one is removed.
=============
*/
static bool UI_DupeCheckServerList (char *adr, bool status)
{
	int		i;

	for (i=0 ; i<totalServers ; i++) {
		if (!sortedServers[i].netAddress && !sortedServers[i].hostName) {
			UI_FreeServer (&sortedServers[i]);
			continue;
		}

		if (sortedServers[i].netAddress && !strcmp (sortedServers[i].netAddress, adr)) {
			if (sortedServers[i].statusPacket && status)
				return true;
			else if (status) {
				UI_FreeServer (&sortedServers[i]);
				return false;
			}
		}
	}

	return false;
}


/*
=============
UI_FreeServerList
=============
*/
static void UI_FreeServerList ()
{
	int		i;

	for (i=0 ; i<totalServers ; i++)
		UI_FreeServer (&sortedServers[i]);

	totalServers = 0;
}


/*
=============
UI_ParseServerInfo

Not used by default anymore, but still works
Kind of dumb, since long host names fuck up parsing beyond all control
Don't use it.
=============
*/
bool UI_ParseServerInfo (char *adr, char *info)
{
	char			*token, name[128];
	serverItem_t	*server;

	if (!cg.menuOpen || !m_joinServerMenu.frameWork.initialized)
		return false;
	if (!info || !info[0])
		return false;
	if (!adr || !adr[0])
		return false;

	// kill the retarded '_'
	info[strlen(info)-1] = '\0';

	if (totalServers >= MAX_LOCAL_SERVERS)
		return true;
	if (UI_DupeCheckServerList (adr, false))
		return true;

	server = &sortedServers[totalServers];
	UI_FreeServer (server);
	totalServers++;

	// add net address
	server->netAddress = CG_TagStrDup (adr, CGTAG_MENU);

	// start at end of string
	token = info + strlen (info);

	// find max players
	while (*token != '/')
		token--;

	if (token < info) {
		// not found
		token = info + strlen (info);
		server->playersStr = CG_TagStrDup ("?/?", CGTAG_MENU);
		server->mapName = CG_TagStrDup ("?", CGTAG_MENU);
		server->maxPlayers = -1;
		server->numPlayers = -1;
	}
	else {
		// found
		server->maxPlayers = atoi (token+1);

		// find current number of players
		*token = 0;
		token--;
		while (token > info && *token >= '0' && *token <= '9')
			token--;
		server->numPlayers = atoi (token+1);

		// set the player string
		server->playersStr = CG_TagStrDup (Q_VarArgs ("%i/%i", server->numPlayers, server->maxPlayers), CGTAG_MENU);

		// find map name
		while ((token > info) && (*token == ' ')) // clear end whitespace
			token--;
		*(token+1) = 0;

		// go to the beginning of the single word
		while ((token > info) && (*token != ' '))
			token--;
		server->mapName = CG_TagStrDup (token+1, CGTAG_MENU);
	}

	// host name is what's left over
	*token = 0;
	if (strlen (info) > MAX_HOSTNAME_LEN-1) {
		token = info + MAX_HOSTNAME_LEN-4;
		while ((token > info) && (*token == ' '))
			token--;
		*token++ = '.';
		*token++ = '.';
		*token++ = '.';
	}
	else
		token = info + strlen (info);
	*token = 0;
	Com_StripPadding (info, name);
	server->hostName = CG_TagStrDup (name, CGTAG_MENU);
	server->shortName = CG_TagStrDup (name, CGTAG_MENU);

	// add the ping
	server->ping = cgi.Sys_Milliseconds () - pingTime;
	server->pingString = CG_TagStrDup (Q_VarArgs ("%ims", server->ping), CGTAG_MENU);

	server->statusPacket = false;

	// print information
	Com_Printf (0, "%s %s ", server->hostName, server->mapName);
	Com_Printf (0, "%i/%i %ims\n", server->numPlayers, server->maxPlayers, server->ping);

	// refresh menu
	// do after printing so that sorting doesn't throw the pointers off
	JoinServerMenu_Init (true);

	return true;
}


/*
=============
UI_ParseServerStatus

Parses a status packet from a server
FIXME: check against a list of sent status requests so it's not attempting to parse things it shouldn't
=============
*/
#define TOKDELIMS	"\\"
bool UI_ParseServerStatus (char *adr, char *info)
{
	serverItem_t	*server;
	char			*token;
	char			shortName[MAX_HOSTNAME_LEN];

	if (!cg.menuOpen || !m_joinServerMenu.frameWork.initialized)
		return false;
	if (!info || !info[0])
		return false;
	if (!adr || !adr[0])
		return false;
	if (!strchr (info, '\\'))
		return false;

	if (totalServers >= MAX_LOCAL_SERVERS)
		return true;
	if (UI_DupeCheckServerList (adr, true))
		return true;

	server = &sortedServers[totalServers];
	UI_FreeServer (server);
	totalServers++;

	// Add net address
	server->netAddress = CG_TagStrDup (adr, CGTAG_MENU);
	server->mapName = CG_TagStrDup (Info_ValueForKey (info, "mapname"), CGTAG_MENU);
	server->maxPlayers = atoi (Info_ValueForKey (info, "maxclients"));
	server->gameName = CG_TagStrDup (Info_ValueForKey (info, "gamename"), CGTAG_MENU);
	server->hostName = CG_TagStrDup (Info_ValueForKey (info, "hostname"), CGTAG_MENU);
	if (server->hostName) {
		Q_strncpyz (shortName, server->hostName, sizeof(shortName));
		server->shortName = CG_TagStrDup (shortName, CGTAG_MENU);
	}

	// Check the player count
	server->numPlayers = atoi (Info_ValueForKey (info, "curplayers"));
	if (server->numPlayers <= 0) {
		server->numPlayers = 0;

		token = strtok (info, "\n");
		if (token) {
			token = strtok (NULL, "\n");

			while (token) {
				server->numPlayers++;
				token = strtok (NULL, "\n");
			}
		}
	}

	// Check if it's valid
	if (!server->mapName[0] && !server->maxPlayers && !server->gameName[0] && !server->hostName[0]) {
		UI_FreeServer (server);
		return false;
	}

	server->playersStr = CG_TagStrDup (Q_VarArgs ("%i/%i", server->numPlayers, server->maxPlayers), CGTAG_MENU);

	// Add the ping
	server->ping = cgi.Sys_Milliseconds () - pingTime;
	server->pingString = CG_TagStrDup (Q_VarArgs ("%ims", server->ping), CGTAG_MENU);

	server->statusPacket = true;

	// Print information
	Com_Printf (0, "%s %s ", server->hostName, server->mapName);
	Com_Printf (0, "%i/%i %ims\n", server->numPlayers, server->maxPlayers, server->ping);

	// Refresh menu
	// Do after printing so that sorting doesn't throw the pointers off
	JoinServerMenu_Init (true);

	return true;
}

// ==========================================================================

static void JoinServerFunc (void *used)
{
	char	buffer[128];
	int		index;

	if (uiState.selectedItem)
		index = (uiAction_t *)uiState.selectedItem - m_joinServerMenu.hostNames;
	else
		index = (uiAction_t *)used - m_joinServerMenu.hostNames;

	if (index >= totalServers)
		return;
	if (!sortedServers[index].netAddress)
		return;

	Q_snprintfz (buffer, sizeof(buffer), "connect %s\n", sortedServers[index].netAddress);
	cgi.Cbuf_AddText (buffer);

	M_ForceMenuOff ();
}

static void ADDRBOOK_MenuFunc (void *unused)
{
	UI_AddressBookMenu_f ();
}

static void JS_Menu_PopFunc (void *unused)
{
	UI_FreeServerList ();
	M_PopMenu ();
}

void JoinMenu_StartSStatus ()
{
	pingTime = cgi.Sys_Milliseconds ();
}

static void SearchLocalGamesFunc (void *item)
{
	float	midrow = (cg.refConfig.vidWidth*0.5) - (18*UIFT_SIZEMED);
	float	midcol = (cg.refConfig.vidHeight*0.5) - (3*UIFT_SIZEMED);
	int		i;
	char	*adrString;
	char	name[32];

	UI_FreeServerList ();
	JoinServerMenu_Init (true);

	UI_DrawTextBox (midrow, midcol, UIFT_SCALEMED, 36, 4);
	cgi.R_DrawString (NULL, midrow + (UIFT_SIZEMED*2), midcol + UIFT_SIZEMED, UIFT_SCALEMED, UIFT_SCALEMED, 0,		"       --- PLEASE WAIT! ---       ",	Q_BColorGreen);
	cgi.R_DrawString (NULL, midrow + (UIFT_SIZEMED*2), midcol + (UIFT_SIZEMED*2), UIFT_SCALEMED, UIFT_SCALEMED, 0,	"Searching for local servers, this",	Q_BColorGreen);
	cgi.R_DrawString (NULL, midrow + (UIFT_SIZEMED*2), midcol + (UIFT_SIZEMED*3), UIFT_SCALEMED, UIFT_SCALEMED, 0,	"could take up to a minute, please",	Q_BColorGreen);
	cgi.R_DrawString (NULL, midrow + (UIFT_SIZEMED*2), midcol + (UIFT_SIZEMED*4), UIFT_SCALEMED, UIFT_SCALEMED, 0,	"please be patient.",					Q_BColorGreen);

	cgi.R_EndFrame ();		// the text box won't show up unless we do a buffer swap

	if (item == &m_joinServerMenu.bookServersAction)
		cgi.Cvar_VariableSetValue (ui_jsMenuPage, JS_PAGE_ADDRBOOK, true);
	else if (item == &m_joinServerMenu.localServersAction)
		cgi.Cvar_VariableSetValue (ui_jsMenuPage, JS_PAGE_LAN, true);

	switch (ui_jsMenuPage->intVal) {
	case JS_PAGE_LAN:
		m_joinServerMenu.localServersAction.generic.flags |= UIF_FORCESELBAR;
		m_joinServerMenu.bookServersAction.generic.flags &= ~UIF_FORCESELBAR;

//		cgi.Cbuf_AddText ("pinglocal\n");	// send out info packet request
		cgi.Cbuf_AddText ("statuslocal\n");	// send out status packet request
		break;
	case JS_PAGE_ADDRBOOK:
		m_joinServerMenu.localServersAction.generic.flags &= ~UIF_FORCESELBAR;
		m_joinServerMenu.bookServersAction.generic.flags |= UIF_FORCESELBAR;

		cgi.Cbuf_AddText ("ui_startSStatus\n");
		for (i=0 ; i<MAX_ADDRBOOK_SAVES ; i++) {
			Q_snprintfz (name, sizeof(name), "adr%i", i);
			adrString = cgi.Cvar_GetStringValue (name);

			if (adrString)
				cgi.Cbuf_AddText (Q_VarArgs ("sstatus %s\n", adrString));
		}
	}

	JoinServerMenu_Init (true);
}

// ==========================================================================

/*
=============
Sort_SetHighlight
=============
*/
static void Sort_SetHighlight ()
{
	m_joinServerMenu.nameSortAction.generic.flags &= ~UIF_FORCESELBAR;
	m_joinServerMenu.gameSortAction.generic.flags &= ~UIF_FORCESELBAR;
	m_joinServerMenu.mapSortAction.generic.flags &= ~UIF_FORCESELBAR;
	m_joinServerMenu.playerSortAction.generic.flags &= ~UIF_FORCESELBAR;
	m_joinServerMenu.pingSortAction.generic.flags &= ~UIF_FORCESELBAR;

	switch (ui_jsSortItem->intVal) {
	case JS_SORT_HOSTNAME:	m_joinServerMenu.nameSortAction.generic.flags |= UIF_FORCESELBAR;	break;
	case JS_SORT_GAMENAME:	m_joinServerMenu.gameSortAction.generic.flags |= UIF_FORCESELBAR;	break;
	case JS_SORT_MAPNAME:	m_joinServerMenu.mapSortAction.generic.flags |= UIF_FORCESELBAR;	break;
	case JS_SORT_PLAYERCNT:	m_joinServerMenu.playerSortAction.generic.flags |= UIF_FORCESELBAR;	break;
	case JS_SORT_PINGCNT:	m_joinServerMenu.pingSortAction.generic.flags |= UIF_FORCESELBAR;	break;
	default:				Com_Printf (PRNT_ERROR, "Invalid ui_jsSortItem value\n");		break;
	}
}


/*
=============
Sort_HostNameFunc
=============
*/
static int hostNameSortCmp (const void *_a, const void *_b)
{
	const serverItem_t *a = (const serverItem_t *) _a;
	const serverItem_t *b = (const serverItem_t *) _b;

	if (a && a->hostName && b && b->hostName)
		return strcmp (a->hostName, b->hostName);

	return 0;
}
static int hostNameInvSortCmp (const void *_a, const void *_b)
{
	const serverItem_t *a = (const serverItem_t *) _a;
	const serverItem_t *b = (const serverItem_t *) _b;

	if (a && a->hostName && b && b->hostName)
		return -strcmp (a->hostName, b->hostName);

	return 0;
}
static void Sort_HostNameFunc (void *item)
{
	cgi.Cvar_VariableSetValue (ui_jsSortItem, 0, true);

	// sort
	if (item)
		cgi.Cvar_VariableSetValue (ui_jsSortMethod, ui_jsSortMethod->intVal ? 0 : 1, true);
	switch (ui_jsSortMethod->intVal) {
		case 0:		qsort (sortedServers, totalServers, sizeof(serverItem_t), hostNameSortCmp);		break;
		default:	qsort (sortedServers, totalServers, sizeof(serverItem_t), hostNameInvSortCmp);		break;
	}

	if (item)
		JoinServerMenu_Init (false);
}


/*
=============
Sort_GameNameFunc
=============
*/
static int gameNameSortCmp (const void *_a, const void *_b)
{
	const serverItem_t *a = (const serverItem_t *) _a;
	const serverItem_t *b = (const serverItem_t *) _b;

	if (a && a->gameName && b && b->gameName)
		return strcmp (a->gameName, b->gameName);

	return 0;
}
static int gameNameInvSortCmp (const void *_a, const void *_b)
{
	const serverItem_t *a = (const serverItem_t *) _a;
	const serverItem_t *b = (const serverItem_t *) _b;

	if (a && a->gameName && b && b->gameName)
		return -strcmp (a->gameName, b->gameName);

	return 0;
}
static void Sort_GameNameFunc (void *item)
{
	cgi.Cvar_VariableSetValue (ui_jsSortItem, 1, true);

	// sort
	if (item)
		cgi.Cvar_VariableSetValue (ui_jsSortMethod, ui_jsSortMethod->intVal ? 0 : 1, true);
	switch (ui_jsSortMethod->intVal) {
		case 0:		qsort (sortedServers, totalServers, sizeof(serverItem_t), gameNameSortCmp);		break;
		default:	qsort (sortedServers, totalServers, sizeof(serverItem_t), gameNameInvSortCmp);		break;
	}

	if (item)
		JoinServerMenu_Init (false);
}


/*
=============
Sort_MapNameFunc
=============
*/
static int mapNameSortCmp (const void *_a, const void *_b)
{
	const serverItem_t *a = (const serverItem_t *) _a;
	const serverItem_t *b = (const serverItem_t *) _b;

	if (a && a->mapName && b && b->mapName)
		return strcmp (a->mapName, b->mapName);

	return 1;
}
static int mapNameInvSortCmp (const void *_a, const void *_b)
{
	const serverItem_t *a = (const serverItem_t *) _a;
	const serverItem_t *b = (const serverItem_t *) _b;

	if (a && a->mapName && b && b->mapName)
		return -strcmp (a->mapName, b->mapName);

	return 1;
}
static void Sort_MapNameFunc (void *item)
{
	cgi.Cvar_VariableSetValue (ui_jsSortItem, 2, true);

	// sort
	if (item)
		cgi.Cvar_VariableSetValue (ui_jsSortMethod, ui_jsSortMethod->intVal ? 0 : 1, true);
	switch (ui_jsSortMethod->intVal) {
		case 0:		qsort (sortedServers, totalServers, sizeof(serverItem_t), mapNameSortCmp);	break;
		default:	qsort (sortedServers, totalServers, sizeof(serverItem_t), mapNameInvSortCmp);	break;
	}

	if (item)
		JoinServerMenu_Init (false);
}


/*
=============
Sort_PlayerCntFunc
=============
*/
static int playerSortCmp (const void *_a, const void *_b)
{
	const serverItem_t *a = (const serverItem_t *) _a;
	const serverItem_t *b = (const serverItem_t *) _b;

	if (a && b) {
		if (a->numPlayers > b->numPlayers)
			return 1;
		else
			return -1;
	}

	return 1;
}
static int playerInvSortCmp (const void *_a, const void *_b)
{
	const serverItem_t *a = (const serverItem_t *) _a;
	const serverItem_t *b = (const serverItem_t *) _b;

	if (a && b) {
		if (a->numPlayers > b->numPlayers)
			return -1;
		else
			return 1;
	}

	return -1;
}
static void Sort_PlayerCntFunc (void *item)
{
	cgi.Cvar_VariableSetValue (ui_jsSortItem, 3, true);

	// sort
	if (item)
		cgi.Cvar_VariableSetValue (ui_jsSortMethod, ui_jsSortMethod->intVal ? 0 : 1, true);
	switch (ui_jsSortMethod->intVal) {
		case 0:		qsort (sortedServers, totalServers, sizeof(serverItem_t), playerSortCmp);	break;
		default:	qsort (sortedServers, totalServers, sizeof(serverItem_t), playerInvSortCmp);	break;
	}

	if (item)
		JoinServerMenu_Init (false);
}


/*
=============
Sort_PingFunc
=============
*/
static int pingSortCmp (const void *_a, const void *_b)
{
	const serverItem_t *a = (const serverItem_t *) _a;
	const serverItem_t *b = (const serverItem_t *) _b;

	if (a && b) {
		if (a->ping > b->ping)
			return 1;
		else
			return -1;
	}

	return 1;
}
static int pingInvSortCmp (const void *_a, const void *_b)
{
	const serverItem_t *a = (const serverItem_t *) _a;
	const serverItem_t *b = (const serverItem_t *) _b;

	if (a && b) {
		if (a->ping > b->ping)
			return -1;
		else
			return 1;
	}

	return -1;
}
static void Sort_PingFunc (void *item)
{
	cgi.Cvar_VariableSetValue (ui_jsSortItem, 4, true);

	// sort
	if (item)
		cgi.Cvar_VariableSetValue (ui_jsSortMethod, ui_jsSortMethod->intVal ? 0 : 1, true);
	switch (ui_jsSortMethod->intVal) {
		case 0:		qsort (sortedServers, totalServers, sizeof(serverItem_t), pingSortCmp);	break;
		default:	qsort (sortedServers, totalServers, sizeof(serverItem_t), pingInvSortCmp);	break;
	}

	if (item)
		JoinServerMenu_Init (false);
}

// ==========================================================================

/*
=============
JoinServerMenu_Init
=============
*/
static void JoinServerMenu_Init (bool sort)
{
	int		i;

	if (sort) {
		switch (ui_jsSortItem->intVal) {
		case JS_SORT_HOSTNAME:	Sort_HostNameFunc (NULL);	break;
		case JS_SORT_GAMENAME:	Sort_GameNameFunc (NULL);	break;
		case JS_SORT_MAPNAME:	Sort_MapNameFunc (NULL);	break;
		case JS_SORT_PLAYERCNT:	Sort_PlayerCntFunc (NULL);	break;
		case JS_SORT_PINGCNT:	Sort_PingFunc (NULL);		break;
		default:
			Com_Printf (PRNT_ERROR, "Invalid ui_jsSortItem value\n");
			break;
		}
	}

	UI_StartFramework (&m_joinServerMenu.frameWork, FWF_CENTERHEIGHT);

	m_joinServerMenu.banner.generic.type		= UITYPE_IMAGE;
	m_joinServerMenu.banner.generic.flags		= UIF_NOSELECT|UIF_CENTERED;
	m_joinServerMenu.banner.generic.name		= NULL;
	m_joinServerMenu.banner.mat					= uiMedia.banners.joinServer;

	m_joinServerMenu.addressBookAction.generic.type			= UITYPE_ACTION;
	m_joinServerMenu.addressBookAction.generic.name			= "Edit Addresses";
	m_joinServerMenu.addressBookAction.generic.flags		= UIF_LEFT_JUSTIFY|UIF_SHADOW;
	m_joinServerMenu.addressBookAction.generic.callBack		= ADDRBOOK_MenuFunc;
	m_joinServerMenu.addressBookAction.generic.statusBar	= "Edit address book entries";

	m_joinServerMenu.bookServersAction.generic.type			= UITYPE_ACTION;
	m_joinServerMenu.bookServersAction.generic.name			= S_COLOR_YELLOW"Address Book";
	m_joinServerMenu.bookServersAction.generic.flags		|= UIF_LEFT_JUSTIFY|UIF_SHADOW;
	m_joinServerMenu.bookServersAction.generic.callBack		= SearchLocalGamesFunc;
	m_joinServerMenu.bookServersAction.generic.cursorDraw	= Cursor_NullFunc;

	m_joinServerMenu.localServersAction.generic.type		= UITYPE_ACTION;
	m_joinServerMenu.localServersAction.generic.name		= S_COLOR_YELLOW"LAN";
	m_joinServerMenu.localServersAction.generic.flags		|= UIF_LEFT_JUSTIFY|UIF_SHADOW;
	m_joinServerMenu.localServersAction.generic.callBack	= SearchLocalGamesFunc;
	m_joinServerMenu.localServersAction.generic.cursorDraw	= Cursor_NullFunc;

	UI_AddItem (&m_joinServerMenu.frameWork,			&m_joinServerMenu.banner);
	UI_AddItem (&m_joinServerMenu.frameWork,			&m_joinServerMenu.addressBookAction);
	UI_AddItem (&m_joinServerMenu.frameWork,			&m_joinServerMenu.bookServersAction);
	UI_AddItem (&m_joinServerMenu.frameWork,			&m_joinServerMenu.localServersAction);

	m_joinServerMenu.nameSortAction.generic.type		= UITYPE_ACTION;
	m_joinServerMenu.nameSortAction.generic.name		= S_COLOR_GREEN"Server name";
	m_joinServerMenu.nameSortAction.generic.flags		= UIF_LEFT_JUSTIFY|UIF_SHADOW;
	m_joinServerMenu.nameSortAction.generic.callBack	= Sort_HostNameFunc;
	m_joinServerMenu.nameSortAction.generic.statusBar	= "Sort by server name";
	m_joinServerMenu.nameSortAction.generic.cursorDraw	= Cursor_NullFunc;

	m_joinServerMenu.gameSortAction.generic.type		= UITYPE_ACTION;
	m_joinServerMenu.gameSortAction.generic.name		= S_COLOR_GREEN"Game";
	m_joinServerMenu.gameSortAction.generic.flags		= UIF_LEFT_JUSTIFY|UIF_SHADOW;
	m_joinServerMenu.gameSortAction.generic.callBack	= Sort_GameNameFunc;
	m_joinServerMenu.gameSortAction.generic.statusBar	= "Sort by game name";
	m_joinServerMenu.gameSortAction.generic.cursorDraw	= Cursor_NullFunc;

	m_joinServerMenu.mapSortAction.generic.type			= UITYPE_ACTION;
	m_joinServerMenu.mapSortAction.generic.name			= S_COLOR_GREEN"Map";
	m_joinServerMenu.mapSortAction.generic.flags		= UIF_LEFT_JUSTIFY|UIF_SHADOW;
	m_joinServerMenu.mapSortAction.generic.callBack		= Sort_MapNameFunc;
	m_joinServerMenu.mapSortAction.generic.statusBar	= "Sort by mapname";
	m_joinServerMenu.mapSortAction.generic.cursorDraw	= Cursor_NullFunc;

	m_joinServerMenu.playerSortAction.generic.type		= UITYPE_ACTION;
	m_joinServerMenu.playerSortAction.generic.name		= S_COLOR_GREEN"Players";
	m_joinServerMenu.playerSortAction.generic.flags		= UIF_LEFT_JUSTIFY|UIF_SHADOW;
	m_joinServerMenu.playerSortAction.generic.callBack	= Sort_PlayerCntFunc;
	m_joinServerMenu.playerSortAction.generic.statusBar	= "Sort by player count";
	m_joinServerMenu.playerSortAction.generic.cursorDraw= Cursor_NullFunc;

	m_joinServerMenu.pingSortAction.generic.type		= UITYPE_ACTION;
	m_joinServerMenu.pingSortAction.generic.name		= S_COLOR_GREEN"Ping";
	m_joinServerMenu.pingSortAction.generic.flags		= UIF_LEFT_JUSTIFY|UIF_SHADOW;
	m_joinServerMenu.pingSortAction.generic.callBack	= Sort_PingFunc;
	m_joinServerMenu.pingSortAction.generic.statusBar	= "Sort by player count";
	m_joinServerMenu.pingSortAction.generic.cursorDraw	= Cursor_NullFunc;

	Sort_SetHighlight ();

	UI_AddItem (&m_joinServerMenu.frameWork,			&m_joinServerMenu.nameSortAction);
	UI_AddItem (&m_joinServerMenu.frameWork,			&m_joinServerMenu.gameSortAction);
	UI_AddItem (&m_joinServerMenu.frameWork,			&m_joinServerMenu.mapSortAction);
	UI_AddItem (&m_joinServerMenu.frameWork,			&m_joinServerMenu.playerSortAction);
	UI_AddItem (&m_joinServerMenu.frameWork,			&m_joinServerMenu.pingSortAction);

	for (i=0 ; i<totalServers ; i++) {
		if (!sortedServers[i].netAddress)
			continue;

		// server name
		m_joinServerMenu.hostNames[i].generic.type			= UITYPE_ACTION;
		m_joinServerMenu.hostNames[i].generic.name			= sortedServers[i].shortName;
		m_joinServerMenu.hostNames[i].generic.flags			= UIF_LEFT_JUSTIFY|UIF_DBLCLICK|UIF_SELONLY;
		m_joinServerMenu.hostNames[i].generic.callBack		= JoinServerFunc;
		m_joinServerMenu.hostNames[i].generic.statusBar		= sortedServers[i].hostName;

		// game name
		m_joinServerMenu.gameNames[i].generic.type			= UITYPE_ACTION;
		m_joinServerMenu.gameNames[i].generic.name			= sortedServers[i].gameName;
		m_joinServerMenu.gameNames[i].generic.flags			= UIF_NOSELECT|UIF_LEFT_JUSTIFY;

		// map name
		m_joinServerMenu.serverMap[i].generic.type			= UITYPE_ACTION;
		m_joinServerMenu.serverMap[i].generic.name			= sortedServers[i].mapName;
		m_joinServerMenu.serverMap[i].generic.flags			= UIF_NOSELECT|UIF_LEFT_JUSTIFY;

		// players connected
		m_joinServerMenu.serverPlayers[i].generic.type		= UITYPE_ACTION;
		m_joinServerMenu.serverPlayers[i].generic.name		= sortedServers[i].playersStr;
		m_joinServerMenu.serverPlayers[i].generic.flags		= UIF_NOSELECT|UIF_LEFT_JUSTIFY;

		// ping
		m_joinServerMenu.serverPing[i].generic.type			= UITYPE_ACTION;
		m_joinServerMenu.serverPing[i].generic.name			= sortedServers[i].pingString;
		m_joinServerMenu.serverPing[i].generic.flags			= UIF_NOSELECT|UIF_LEFT_JUSTIFY;

		UI_AddItem (&m_joinServerMenu.frameWork,		&m_joinServerMenu.hostNames[i]);
		UI_AddItem (&m_joinServerMenu.frameWork,		&m_joinServerMenu.gameNames[i]);
		UI_AddItem (&m_joinServerMenu.frameWork,		&m_joinServerMenu.serverMap[i]);
		UI_AddItem (&m_joinServerMenu.frameWork,		&m_joinServerMenu.serverPlayers[i]);
		UI_AddItem (&m_joinServerMenu.frameWork,		&m_joinServerMenu.serverPing[i]);
	}

	m_joinServerMenu.backAction.generic.type		= UITYPE_ACTION;
	m_joinServerMenu.backAction.generic.flags		= UIF_CENTERED|UIF_LARGE|UIF_SHADOW;
	m_joinServerMenu.backAction.generic.name		= "< Back";
	m_joinServerMenu.backAction.generic.callBack	= JS_Menu_PopFunc;
	m_joinServerMenu.backAction.generic.statusBar	= "Back a menu";
	m_joinServerMenu.backAction.generic.cursorDraw	= Cursor_NullFunc;

	m_joinServerMenu.refreshAction.generic.type		= UITYPE_ACTION;
	m_joinServerMenu.refreshAction.generic.name		= "[Update]";
	m_joinServerMenu.refreshAction.generic.flags	= UIF_CENTERED|UIF_LARGE|UIF_SHADOW;
	m_joinServerMenu.refreshAction.generic.callBack	= SearchLocalGamesFunc;
	m_joinServerMenu.refreshAction.generic.statusBar= "Refresh the active server list";
	m_joinServerMenu.refreshAction.generic.cursorDraw= Cursor_NullFunc;

	m_joinServerMenu.playAction.generic.type		= UITYPE_ACTION;
	m_joinServerMenu.playAction.generic.flags		= UIF_CENTERED|UIF_LARGE|UIF_SHADOW;
	m_joinServerMenu.playAction.generic.name		= "Play >";
	m_joinServerMenu.playAction.generic.callBack	= JoinServerFunc;
	m_joinServerMenu.playAction.generic.statusBar	= "Join Game";
	m_joinServerMenu.playAction.generic.cursorDraw	= Cursor_NullFunc;

	UI_AddItem (&m_joinServerMenu.frameWork,			&m_joinServerMenu.backAction);
	UI_AddItem (&m_joinServerMenu.frameWork,			&m_joinServerMenu.refreshAction);
	UI_AddItem (&m_joinServerMenu.frameWork,			&m_joinServerMenu.playAction);

	UI_FinishFramework (&m_joinServerMenu.frameWork, true);
}


/*
=============
JoinServerMenu_Close
=============
*/
static struct sfx_t *JoinServerMenu_Close ()
{
	UI_FreeServerList ();

	return uiMedia.sounds.menuOut;
}


/*
=============
JoinServerMenu_Draw
=============
*/
static void JoinServerMenu_Draw ()
{
	float	xOffset = -(UIFT_SIZE*36);
	float	oldOffset = xOffset;
	float	y, oldy;
	int		i;

	// Initialize if necessary
	if (!m_joinServerMenu.frameWork.initialized)
		SearchLocalGamesFunc (NULL);

	// Dynamically position
	m_joinServerMenu.frameWork.x			= cg.refConfig.vidWidth * 0.5f;
	m_joinServerMenu.frameWork.y			= 0;

	m_joinServerMenu.banner.generic.x		= 0;
	m_joinServerMenu.banner.generic.y		= 0;

	y = m_joinServerMenu.banner.height * UI_SCALE;

	m_joinServerMenu.addressBookAction.generic.x	= xOffset += UIFT_SIZE*3;
	m_joinServerMenu.addressBookAction.generic.y	= y + UIFT_SIZEINC*2;
	m_joinServerMenu.bookServersAction.generic.x	= xOffset;
	m_joinServerMenu.bookServersAction.generic.y	= y += UIFT_SIZEINC;
	m_joinServerMenu.localServersAction.generic.x	= xOffset += UIFT_SIZE*16;
	m_joinServerMenu.localServersAction.generic.y	= y;

	xOffset = oldOffset;

	m_joinServerMenu.nameSortAction.generic.x		= xOffset;
	m_joinServerMenu.nameSortAction.generic.y		= oldy = y += (UIFT_SIZEINC*3);
	m_joinServerMenu.gameSortAction.generic.x		= xOffset += UIFT_SIZE*MAX_HOSTNAME_LEN;
	m_joinServerMenu.gameSortAction.generic.y		= oldy;
	m_joinServerMenu.mapSortAction.generic.x		= xOffset += UIFT_SIZE*MAX_GAMENAME_LEN;
	m_joinServerMenu.mapSortAction.generic.y		= oldy;
	m_joinServerMenu.playerSortAction.generic.x		= xOffset += UIFT_SIZE*MAX_MAPNAME_LEN;
	m_joinServerMenu.playerSortAction.generic.y		= oldy;
	m_joinServerMenu.pingSortAction.generic.x		= xOffset += UIFT_SIZE*MAX_PING_LEN;
	m_joinServerMenu.pingSortAction.generic.y		= oldy;

	for (i=0 ; i<totalServers ; i++) {
		if (!sortedServers[i].netAddress)
			continue;

		xOffset = oldOffset;

		m_joinServerMenu.hostNames[i].generic.x		= xOffset;
		m_joinServerMenu.hostNames[i].generic.y		= y += UIFT_SIZEINC;
		m_joinServerMenu.gameNames[i].generic.x		= xOffset += UIFT_SIZE*MAX_HOSTNAME_LEN;
		m_joinServerMenu.gameNames[i].generic.y		= y;
		m_joinServerMenu.serverMap[i].generic.x		= xOffset += UIFT_SIZE*MAX_GAMENAME_LEN;
		m_joinServerMenu.serverMap[i].generic.y		= y;
		m_joinServerMenu.serverPlayers[i].generic.x	= xOffset += UIFT_SIZE*MAX_MAPNAME_LEN;
		m_joinServerMenu.serverPlayers[i].generic.y	= y;
		m_joinServerMenu.serverPing[i].generic.x	= xOffset += UIFT_SIZE*MAX_PING_LEN;
		m_joinServerMenu.serverPing[i].generic.y	= y;
	}

	m_joinServerMenu.backAction.generic.x			= (-10 * UIFT_SIZELG);
	m_joinServerMenu.backAction.generic.y			= y = oldy + (UIFT_SIZEINC*(MAX_LOCAL_SERVERS+1)) + (UIFT_SIZEINCLG*2);
	m_joinServerMenu.refreshAction.generic.x		= 0;
	m_joinServerMenu.refreshAction.generic.y		= y;
	m_joinServerMenu.playAction.generic.x			= (10 * UIFT_SIZELG);
	m_joinServerMenu.playAction.generic.y			= y;

	// Render
	UI_DrawInterface (&m_joinServerMenu.frameWork);
}


/*
=============
UI_JoinServerMenu_f
=============
*/
void UI_JoinServerMenu_f ()
{
	SearchLocalGamesFunc (NULL);
	M_PushMenu (&m_joinServerMenu.frameWork, JoinServerMenu_Draw, JoinServerMenu_Close, M_KeyHandler);
}
