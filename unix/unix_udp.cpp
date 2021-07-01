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
// unix_udp.c
//

#include "../common/common.h"
#include "unix_local.h"

#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <errno.h>

#ifdef NeXT
#include <libc.h>
#endif

#ifndef IPTOS_LOWDELAY
#define IPTOS_LOWDELAY 16
#endif

#define	LOOPBACK	0x7f000001

loopBack_t			net_loopBacks[NS_MAX];
int					net_ipSockets[NS_MAX];
netStats_t			net_stats;

static netAdr_t		net_localAdr;

/*
====================
NET_ErrorString
====================
*/
static char *NET_ErrorString (void)
{
	int		code;

	code = errno;
	return strerror (code);
}


/*
===================
NET_AdrToString
===================
*/
char *NET_AdrToString (netAdr_t *a)
{
	static char		str[64];

	switch (a->naType) {
	case NA_LOOPBACK:
		Q_snprintfz (str, sizeof (str), "loopback");
		break;

	case NA_IP:
		Q_snprintfz (str, sizeof (str), "%i.%i.%i.%i:%i", a->ip[0], a->ip[1], a->ip[2], a->ip[3], ntohs(a->port));
		break;

	default:
		assert (0);
		break;
	}

	return str;
}


/*
===================
NET_NetAdrToSockAdr
===================
*/
static void NET_NetAdrToSockAdr (netAdr_t *a, struct sockaddr_in *s)
{
	memset (s, 0, sizeof(*s));

	switch (a->naType) {
	case NA_BROADCAST:
		s->sin_family = AF_INET;

		s->sin_port = a->port;
		*(int *)&s->sin_addr = -1;
		break;

	case NA_IP:
		s->sin_family = AF_INET;

		*(int *)&s->sin_addr = *(int *)&a->ip;
		s->sin_port = a->port;
		break;

	default:
		assert (0);
		break;
	}
}


/*
=============
NET_StringToSockaddr

localhost
idnewt
idnewt:28000
192.246.40.70
192.246.40.70:28000
=============
*/
qBool NET_StringToSockaddr (char *s, struct sockaddr *sadr)
{
	struct hostent	*h;
	char	*colon, *p;
	char	copy[128];
	int		isIP = 0;

	memset (sadr, 0, sizeof (*sadr));

	// R1: better than just the first digit for ip validity
	p = s;
	while (*p) {
		if (*p == '.') {
			isIP++;
		}
		else if (*p == ':') {
			break;
		}
		else if (!isdigit (*p)) {
			isIP = -1;
			break;
		}
		p++;
	}
	if (isIP != -1 && isIP != 3)
		return qFalse;

	((struct sockaddr_in *)sadr)->sin_family = AF_INET;
	((struct sockaddr_in *)sadr)->sin_port = 0;

	// R1: CHECK THE GODDAMN BUFFER SIZE... sigh yet another overflow.
	Q_strncpyz (copy, s, sizeof (copy));

	// strip off a trailing :port if present
	for (colon=copy ; colon[0] ; colon++) {
		if (colon[0] == ':') {
			colon[0] = '\0';
			((struct sockaddr_in *)sadr)->sin_port = htons ((int16)atoi(colon+1));
			break;
		}
	}

	if (isIP != -1) {
		*(int *)&((struct sockaddr_in *)sadr)->sin_addr = inet_addr(copy);
	}
	else {
		if (!(h = gethostbyname(copy)))
			return qFalse;
		*(int *)&((struct sockaddr_in *)sadr)->sin_addr = *(int *)h->h_addr_list[0];
	}

	return qTrue;
}


/*
=============
NET_StringToAdr

localhost
idnewt
idnewt:28000
192.246.40.70
192.246.40.70:28000
=============
*/
qBool NET_StringToAdr (char *s, netAdr_t *a)
{
	struct sockaddr_in sadr;
	
	if (!strcmp (s, "localhost")) {
		memset (a, 0, sizeof (*a));

		a->naType = NA_LOOPBACK;
		a->ip[0] = 127;
		a->ip[3] = 1;

		return qTrue;
	}

	if (!NET_StringToSockaddr (s, &sadr))
		return qFalse;
	
	NET_SockAdrToNetAdr (&sadr, a);

	return qTrue;
}

//=============================================================================

/*
===================
NET_IsLocalAddress
===================
*/
qBool NET_IsLocalAddress (netAdr_t adr)
{
	return NET_CompareAdr (adr, net_localAdr);
}

/*
=============================================================================

LOOPBACK BUFFERS FOR LOCAL PLAYER

=============================================================================
*/

#ifndef DEDICATED_ONLY
/*
===================
NET_GetLoopPacket
===================
*/
static qBool NET_GetLoopPacket (netSrc_t sock, netAdr_t *fromAddr, netMsg_t *message)
{
	int		i;
	loopBack_t	*loop;

	loop = &net_loopBacks[sock];

	if (loop->send - loop->get > MAX_LOOPBACK)
		loop->get = loop->send - MAX_LOOPBACK;

	if (loop->get >= loop->send)
		return qFalse;

	i = loop->get & MAX_LOOPBACKMASK;
	loop->get++;

	memcpy (message->data, loop->msgs[i].data, loop->msgs[i].datalen);
	message->curSize = loop->msgs[i].datalen;
	*fromAddr = net_localAdr;
	return qTrue;

}


/*
===================
NET_SendLoopPacket
===================
*/
static void NET_SendLoopPacket (netSrc_t sock, int length, void *data)
{
	int		i;
	loopBack_t	*loop;

	loop = &net_loopBacks[sock^1];

	i = loop->send & MAX_LOOPBACKMASK;
	loop->send++;

	memcpy (loop->msgs[i].data, data, length);
	loop->msgs[i].datalen = length;
}
#endif // DEDICATED_ONLY

/*
=============================================================================

	GET / SEND

=============================================================================
*/

/*
===================
NET_GetPacket
===================
*/
qBool NET_GetPacket (netSrc_t sock, netAdr_t *fromAddr, netMsg_t *message)
{
	int 	ret;
	struct sockaddr_in	from;
	socklen_t		fromlen;
	int		netSocket;
	int		err;

#ifndef DEDICATED_ONLY
	if (NET_GetLoopPacket (sock, fromAddr, message))
		return qTrue;
#endif

	netSocket = net_ipSockets[sock];
	if (!netSocket)
		return qFalse;

	fromlen = sizeof(from);
	ret = recvfrom (netSocket, message->data, message->maxSize, 0, (struct sockaddr *)&from, &fromlen);

	NET_SockAdrToNetAdr (&from, fromAddr);

	if (ret == -1) {
		err = errno;

		if (err == EWOULDBLOCK || err == ECONNREFUSED)
			return qFalse;
		Com_Printf (0, "NET_GetPacket: %s from %s\n", NET_ErrorString (),
					NET_AdrToString (*fromAddr));
		return 0;
	}

	if (ret == message->maxSize) {
		Com_Printf (0, "Oversize packet from %s\n", NET_AdrToString (*fromAddr));
		return qFalse;
	}

	net_stats.sizeIn += ret;
	net_stats.packetsIn++;

	message->curSize = ret;
	return qTrue;
}


/*
===================
NET_SendPacket
===================
*/
int NET_SendPacket (netSrc_t sock, int length, void *data, netAdr_t *to)
{
	int		ret;
	struct sockaddr_in	addr;
	int		netSocket;

	switch (to->naType) {
#ifndef DEDICATED_ONLY
	case NA_LOOPBACK:
		NET_SendLoopPacket (sock, length, data);
		return 0;
#endif

	case NA_BROADCAST:
		netSocket = net_ipSockets[sock];
		if (!netSocket)
			return 0;
		break;

	case NA_IP:
		netSocket = net_ipSockets[sock];
		if (!netSocket)
			return 0;
		break;

	default:
		Com_Error (ERR_FATAL, "NET_SendPacket: bad address type: %d", to->naType);
		break;
	}

	NET_NetAdrToSockAdr (to, &addr);

	ret = sendto (netSocket, data, length, 0, (struct sockaddr *)&addr, sizeof(addr));
	if (ret == -1) {
		Com_Printf (0, "NET_SendPacket ERROR: %s to %s\n", NET_ErrorString (), NET_AdrToString (to));
		return 0;
		// FIXME: return -1 for certain errors like in net_wins.c
	}

	net_stats.sizeOut += ret;
	net_stats.packetsOut++;

	return 1;
}

/*
=============================================================================

	SOCKETS

=============================================================================
*/

/*
====================
NET_GetIPSocket
====================
*/
static int NET_GetIPSocket (char *netInterface, int port)
{
	int newsocket;
	struct sockaddr_in address;
	qBool _qTrue = qTrue;
	int	i = 1;

	if ((newsocket = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		Com_Printf (0, "ERROR: UDP_OpenSocket: socket: %s", NET_ErrorString ());
		return 0;
	}

	// Make it non-blocking
	if (ioctl (newsocket, FIONBIO, &_qTrue) == -1) {
		Com_Printf (0, "ERROR: UDP_OpenSocket: ioctl FIONBIO:%s\n", NET_ErrorString ());
		return 0;
	}

	// Make it broadcast capable
	if (setsockopt(newsocket, SOL_SOCKET, SO_BROADCAST, (char *)&i, sizeof(i)) == -1) {
		Com_Printf (0, "ERROR: UDP_OpenSocket: setsockopt SO_BROADCAST:%s\n", NET_ErrorString ());
		return 0;
	}

	// R1: set 'interactive' ToS
	i = IPTOS_LOWDELAY;
	if (setsockopt(newsocket, IPPROTO_IP, IP_TOS, (char *)&i, sizeof(i)) == -1) {
		Com_Printf (0, "WARNING: UDP_OpenSocket: setsockopt IP_TOS: %s\n", NET_ErrorString ());
	}
	
	if (!netInterface || !netInterface[0] || !Q_stricmp(netInterface, "localhost"))
		address.sin_addr.s_addr = INADDR_ANY;
	else
		NET_StringToSockaddr (netInterface, (struct sockaddr *)&address);

	address.sin_port = (port == PORT_ANY)?0:htons((short)port);
	address.sin_family = AF_INET;

	if (bind (newsocket, (void *)&address, sizeof(address)) == -1) {
		Com_Printf (0, "ERROR: UDP_OpenSocket: bind: %s\n", NET_ErrorString ());
		close (newsocket);
		return 0;
	}

	return newsocket;
}


/*
====================
NET_Config

A single player game will only use the loopback code
====================
*/
netConfig_t NET_Config (netConfig_t openFlags)
{
	static netConfig_t	oldFlags;
	int					oldest;
	cVar_t				*ip;
	int					port;

	if (oldFlags == openFlags)
		return oldFlags;

	memset (&net_stats, 0, sizeof (net_stats));

	if (openFlags == NET_NONE) {
		oldest = oldFlags;
		oldFlags = NET_NONE;

		// shut down any existing sockets
		if (net_ipSockets[NS_CLIENT]) {
			close (net_ipSockets[NS_CLIENT]);
			net_ipSockets[NS_CLIENT] = 0;
		}

		if (net_ipSockets[NS_SERVER]) {
			close (net_ipSockets[NS_SERVER]);
			net_ipSockets[NS_SERVER] = 0;
		}
	}
	else {
		oldest = oldFlags;
		oldFlags |= openFlags;

		ip = Cvar_Register ("ip", "localhost", CVAR_READONLY);

		// Open sockets
		net_stats.initTime = time (0);
		net_stats.initialized = qTrue;

		if (openFlags & NET_SERVER) {
			if (!net_ipSockets[NS_SERVER]) {
				port = Cvar_Register ("ip_hostport", "0", CVAR_READONLY)->intVal;
				if (!port) {
					port = Cvar_Register ("hostport", "0", CVAR_READONLY)->intVal;
					if (!port)
						port = Cvar_Register ("port", Q_VarArgs ("%i", PORT_SERVER), CVAR_READONLY)->intVal;
				}

				net_ipSockets[NS_SERVER] = NET_GetIPSocket (ip->string, port);
				if (!net_ipSockets[NS_SERVER] && dedicated->intVal)
					Com_Error (ERR_FATAL, "Couldn't allocate dedicated server IP port");
			}
		}

#ifndef DEDICATED_ONLY
		// Dedicated servers don't need client ports
		if (!dedicated->intVal && openFlags & NET_CLIENT) {
			if (!net_ipSockets[NS_CLIENT]) {
 				int		newport = frand() * 64000 + 1024;

				port = Cvar_Register ("ip_clientport", Q_VarArgs ("%i", newport), CVAR_READONLY)->intVal;
				if (!port) {
					port = Cvar_Register ("clientport", Q_VarArgs ("%i", newport), CVAR_READONLY)->intVal;
					if (!port) {
						port = PORT_ANY;
 						Cvar_Set ("clientport", Q_VarArgs ("%d", newport), qFalse);
					}
				}

				net_ipSockets[NS_CLIENT] = NET_GetIPSocket (ip->string, port);
				if (!net_ipSockets[NS_CLIENT])
					net_ipSockets[NS_CLIENT] = NET_GetIPSocket (ip->string, PORT_ANY);
			}
		}
#endif
	}

	return oldest;	
}


/*
====================
NET_Server_Sleep

Sleeps for msec or until net socket is ready
====================
*/
void NET_Server_Sleep (int msec)
{
	struct timeval timeout;
	fd_set	fdset;
	extern cVar_t *dedicated;
	extern qBool stdin_active;

	if (!net_ipSockets[NS_SERVER] || (dedicated && !dedicated->intVal))
		return; // we're not a server, just run full speed

	FD_ZERO(&fdset);
	if (stdin_active)
		FD_SET(0, &fdset); // stdin is processed too
	FD_SET(net_ipSockets[NS_SERVER], &fdset); // network socket
	timeout.tv_sec = msec/1000;
	timeout.tv_usec = (msec%1000)*1000;
	select(net_ipSockets[NS_SERVER]+1, &fdset, NULL, NULL, &timeout);
}


#ifndef DEDICATED_ONLY
/*
====================
NET_Client_Sleep
====================
*/
int NET_Client_Sleep (int msec)
{
    struct timeval	timeout;
	fd_set			fdset;
	SOCKET			i;

	FD_ZERO(&fdset);
	i = 0;

	if (net_ipSockets[NS_CLIENT]) {
		FD_SET(net_ipSockets[NS_CLIENT], &fdset); // network socket
		i = net_ipSockets[NS_CLIENT];
	}

	timeout.tv_sec = msec/1000;
	timeout.tv_usec = (msec%1000)*1000;
	return select ((int)(i+1), &fdset, NULL, NULL, &timeout);
}
#endif

/*
=============================================================================

	CONSOLE COMMANDS

=============================================================================
*/

/*
====================
NET_Stats_f
====================
*/
static void NET_Stats_f (void)
{
	uint32	now = time(0);
	uint32	diff = now - net_stats.initTime;

	if (!net_stats.initialized) {
		Com_Printf (0, "Network sockets not up!\n");
		return;
	}

	Com_Printf (0, "Network up for %i seconds.\n"
		"%i bytes in %i packets received (av: %i kbps)\n"
		"%i bytes in %i packets sent (av: %i kbps)\n",
		
		diff,
		net_stats.sizeIn, net_stats.packetsIn, (int)(((net_stats.sizeIn * 8) / 1024) / diff),
		net_stats.sizeOut, net_stats.packetsOut, (int)((net_stats.sizeOut * 8) / 1024) / diff);
}

/*
=============================================================================

	INIT / SHUTDOWN

=============================================================================
*/

static void *cmd_net_stats;

/*
====================
NET_Init
====================
*/
void NET_Init (void)
{
	// Clear stats
	memset (&net_stats, 0, sizeof (net_stats));

	// Add commands
	cmd_net_stats = Cmd_AddCommand ("net_stats", NET_Stats_f, "Prints out connection information");
}


/*
====================
NET_Shutdown
====================
*/
void NET_Shutdown (void)
{
	Cmd_RemoveCommand ("net_stats", cmd_net_stats);

	// Clear stats
	memset (&net_stats, 0, sizeof (net_stats));

	// Close sockets
	NET_Config (NET_NONE);
}
