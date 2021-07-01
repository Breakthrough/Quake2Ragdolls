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
// win_sock.c
//

#define WIN32_LEAN_AND_MEAN 1
#include "winsock.h"
#include "../common/common.h"

loopBack_t			net_loopBacks[NS_MAX];
int					net_ipSockets[NS_MAX];
netStats_t			net_stats;

static WSADATA		net_wsaData;

/*static*/ netAdr_t netAdr_t::FromSockAdr(sockaddr_in *s)
{
	netAdr_t newAdr;
	newAdr.naType = NA_IP;
	*(int *)&newAdr.ip = s->sin_addr.s_addr;
	newAdr.port = s->sin_port;
	return newAdr;
}

/*
====================
NET_ErrorString
====================
*/
static char *NET_ErrorString (int code)
{
	switch (code) {
	case WSAEINTR:						return "WSAEINTR";				// A blocking operation was interrupted by a call to WSACancelBlockingCall.
	case WSAEBADF:						return "WSAEBADF";				// The file handle supplied is not valid.
	case WSAEACCES:						return "WSAEACCES";				// An attempt was made to access a socket in a way forbidden by its access permissions.
	case WSAEFAULT:						return "WSAEFAULT";				// The system detected an invalid pointer address in attempting to use a pointer argument in a call.
	case WSAEINVAL:						return "WSAEINVAL";				// An invalid argument was supplied.
	case WSAEMFILE:						return "WSAEMFILE";				// Too many open sockets.
	case WSAEWOULDBLOCK:				return "WSAEWOULDBLOCK";		// A non-blocking socket operation could not be completed immediately.
	case WSAEINPROGRESS:				return "WSAEINPROGRESS";		// A blocking operation is currently executing.
	case WSAEALREADY:					return "WSAEALREADY";			// An operation was attempted on a non-blocking socket that already had an operation in progress.
	case WSAENOTSOCK:					return "WSAENOTSOCK";			// An operation was attempted on something that is not a socket.
	case WSAEDESTADDRREQ:				return "WSAEDESTADDRREQ";		// A required address was omitted from an operation on a socket.
	case WSAEMSGSIZE:					return "WSAEMSGSIZE";			// A message sent on a datagram socket was larger than the internal message buffer or some other network limit, or the buffer used to receive a datagram into was smaller than the datagram itself.
	case WSAEPROTOTYPE:					return "WSAEPROTOTYPE";			// A protocol was specified in the socket function call that does not support the semantics of the socket type requested.
	case WSAENOPROTOOPT:				return "WSAENOPROTOOPT";		// An unknown, invalid, or unsupported option or level was specified in a getsockopt or setsockopt call.
	case WSAEPROTONOSUPPORT:			return "WSAEPROTONOSUPPORT";	// The requested protocol has not been configured into the system, or no implementation for it exists.
	case WSAESOCKTNOSUPPORT:			return "WSAESOCKTNOSUPPORT";	// The support for the specified socket type does not exist in this address family.
	case WSAEOPNOTSUPP:					return "WSAEOPNOTSUPP";			// The attempted operation is not supported for the type of object referenced.
	case WSAEPFNOSUPPORT:				return "WSAEPFNOSUPPORT";		// The protocol family has not been configured into the system or no implementation for it exists.
	case WSAEAFNOSUPPORT:				return "WSAEAFNOSUPPORT";		// An address incompatible with the requested protocol was used.
	case WSAEADDRINUSE:					return "WSAEADDRINUSE";			// Only one usage of each socket address (protocol/network address/port) is normally permitted.
	case WSAEADDRNOTAVAIL:				return "WSAEADDRNOTAVAIL";		// The requested address is not valid in its context.
	case WSAENETDOWN:					return "WSAENETDOWN";			// A socket operation encountered a dead network.
	case WSAENETUNREACH:				return "WSAENETUNREACH";		// A socket operation was attempted to an unreachable network.
	case WSAENETRESET:					return "WSAENETRESET";			// The connection has been broken due to keep-alive activity detecting a failure while the operation was in progress.
	case WSAECONNABORTED:				return "WSAECONNABORTED";		// An established connection was aborted by the software in your host machine.
	case WSAECONNRESET:					return "WSAECONNRESET";			// An existing connection was forcibly closed by the remote host.
	case WSAENOBUFS:					return "WSAENOBUFS";			// An operation on a socket could not be performed because the system lacked sufficient buffer space or because a queue was full.
	case WSAEISCONN:					return "WSAEISCONN";			// A connect request was made on an already connected socket.
	case WSAENOTCONN:					return "WSAENOTCONN";			// A request to send or receive data was disallowed because the socket is not connected and (when sending on a datagram socket using a sendto call) no address was supplied.
	case WSAESHUTDOWN:					return "WSAESHUTDOWN";			// A request to send or receive data was disallowed because the socket had already been shut down in that direction with a previous shutdown call.
	case WSAETIMEDOUT:					return "WSAETIMEDOUT";			// A connection attempt failed because the connected party did not properly respond after a period of time, or established connection failed because connected host has failed to respond.
	case WSAECONNREFUSED:				return "WSAECONNREFUSED";		// No connection could be made because the target machine actively refused it.
	case WSAELOOP:						return "WSAELOOP";				// Cannot translate name.
	case WSAENAMETOOLONG:				return "WSAENAMETOOLONG";		// Name component or name was too long.
	case WSAEHOSTDOWN:					return "WSAEHOSTDOWN";			// A socket operation failed because the destination host was down.
	case WSAEHOSTUNREACH:				return "WSAEHOSTUNREACH";		// A socket operation was attempted to an unreachable host.
	case WSAENOTEMPTY:					return "WSAENOTEMPTY";			// Cannot remove a directory that is not empty.
	case WSAEPROCLIM:					return "WSAEPROCLIM";			// A Windows Sockets implementation may have a limit on the number of applications that may use it simultaneously.
	case WSAEUSERS:						return "WSAEUSERS";				// Ran out of quota.
	case WSAEDQUOT:						return "WSAEDQUOT";				// Ran out of disk quota.
	case WSAESTALE:						return "WSAESTALE";				// File handle reference is no longer available.
	case WSAEREMOTE:					return "WSAEREMOTE";			// Item is not available locally.
	case WSASYSNOTREADY:				return "WSASYSNOTREADY";		// WSAStartup cannot function at this time because the underlying system it uses to provide network services is currently unavailable.
	case WSAVERNOTSUPPORTED:			return "WSAVERNOTSUPPORTED";	// The Windows Sockets version requested is not supported.
	case WSANOTINITIALISED:				return "WSANOTINITIALISED";		// Either the application has not called WSAStartup, or WSAStartup failed.
	case WSAEDISCON:					return "WSAEDISCON";			// Returned by WSARecv or WSARecvFrom to indicate the remote party has initiated a graceful shutdown sequence.
	default:							return Q_VarArgs ("UNDEFINED ERROR %d", code);
	}
}


/*
===================
NET_AdrToString
===================
*/
char *NET_AdrToString (netAdr_t &a)
{
	static char		str[22];

	switch (a.naType)
	{
	case NA_LOOPBACK:
		Q_snprintfz (str, sizeof(str), "loopback");
		break;

	case NA_IP:
		Q_snprintfz (str, sizeof(str), "%i.%i.%i.%i:%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3], ntohs(a.port));
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
static void NET_NetAdrToSockAdr (netAdr_t &a, struct sockaddr *s)
{
	memset (s, 0, sizeof(s));

	((struct sockaddr_in *)s)->sin_family = AF_INET;
	((struct sockaddr_in *)s)->sin_port = a.port;

	switch (a.naType) {
	case NA_BROADCAST:
		((struct sockaddr_in *)s)->sin_addr.s_addr = INADDR_BROADCAST;
		break;

	case NA_IP:
		((struct sockaddr_in *)s)->sin_addr.s_addr = *(int *)&a.ip;
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
bool NET_StringToSockaddr (char *s, struct sockaddr *sadr)
{
	struct hostent	*h;
	char	*colon, *p;
	char	copy[128];
	int		isIP = 0;

	memset (sadr, 0, sizeof(*sadr));

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
		return false;

	((struct sockaddr_in *)sadr)->sin_family = AF_INET;
	((struct sockaddr_in *)sadr)->sin_port = 0;

	// R1: CHECK THE GODDAMN BUFFER SIZE... sigh yet another overflow.
	Q_strncpyz (copy, s, sizeof(copy));

	// strip off a trailing :port if present
	for (colon=copy ; colon[0] ; colon++) {
		if (colon[0] == ':') {
			colon[0] = '\0';
			((struct sockaddr_in *)sadr)->sin_port = htons ((sint16)atoi(colon+1));
			break;
		}
	}

	if (isIP != -1) {
		*(int *)&((struct sockaddr_in *)sadr)->sin_addr = inet_addr(copy);
	}
	else {
		if (!(h = gethostbyname(copy)))
			return false;
		*(int *)&((struct sockaddr_in *)sadr)->sin_addr = *(int *)h->h_addr_list[0];
	}

	return true;
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
bool NET_StringToAdr (char *s, netAdr_t &a)
{
	struct sockaddr sadr;
	
	if (!strcmp (s, "localhost")) {
		memset (&a, 0, sizeof(a));

		a.naType = NA_LOOPBACK;
		a.ip[0] = 127;
		a.ip[3] = 1;

		return true;
	}

	if (!NET_StringToSockaddr (s, &sadr))
		return false;
	
	a = netAdr_t::FromSockAdr ((sockaddr_in*)&sadr);
	return true;
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
static bool NET_GetLoopPacket (netSrc_t sock, netAdr_t &fromAddr, netMsg_t &message)
{
	int			i;
	loopBack_t	*loop;

	loop = &net_loopBacks[sock];

	if (loop->send - loop->get > MAX_LOOPBACK)
		loop->get = loop->send - MAX_LOOPBACK;

	if (loop->get >= loop->send)
		return false;

	i = loop->get & MAX_LOOPBACKMASK;
	loop->get++;

	memcpy (message.data, loop->msgs[i].data, loop->msgs[i].dataLen);
	message.curSize = loop->msgs[i].dataLen;

	memset (&fromAddr, 0, sizeof(fromAddr));
	fromAddr.naType = NA_LOOPBACK;
	fromAddr.ip[0] = 127;
	fromAddr.ip[3] = 1;

	return true;
}


/*
===================
NET_SendLoopPacket
===================
*/
static void NET_SendLoopPacket (netSrc_t sock, int length, void *data)
{
	int			i;
	loopBack_t	*loop;

	loop = &net_loopBacks[sock^1];

	i = loop->send & MAX_LOOPBACKMASK;
	loop->send++;

	memcpy (loop->msgs[i].data, data, length);
	loop->msgs[i].dataLen = length;
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
bool NET_GetPacket (netSrc_t sock, netAdr_t &fromAddr, netMsg_t &message)
{
	struct	sockaddr fromSockAddr;
	int		ret, fromLen, netSocket, error;

#ifndef DEDICATED_ONLY
	if (NET_GetLoopPacket (sock, fromAddr, message))
		return true;
#endif

	netSocket = net_ipSockets[sock];
	if (!netSocket)
		return false;

	fromLen = sizeof(fromSockAddr);
	ret = recvfrom (netSocket, (char *)message.data, message.maxSize, 0, (struct sockaddr *)&fromSockAddr, &fromLen);

	fromAddr = netAdr_t::FromSockAdr ((sockaddr_in*)&fromSockAddr);

	if (ret == -1)
	{
		error = WSAGetLastError();

		switch (error) {
		// wouldblock is silent
		case WSAEWOULDBLOCK:
			return false;

		// large packet
		case WSAEMSGSIZE:
			Com_Printf (PRNT_WARNING, "WARNING: NET_GetPacket: Oversize packet from %s\n", NET_AdrToString (fromAddr));
			return false;
		}

#ifndef DEDICATED_ONLY
		if (dedicated->intVal || error == WSAECONNRESET)
#endif
			Com_Printf (PRNT_WARNING, "NET_GetPacket: %s from %s\n", NET_ErrorString (error), NET_AdrToString (fromAddr));
#ifndef DEDICATED_ONLY
		else
			Com_Printf (PRNT_ERROR, "NET_GetPacket: %s from %s\n", NET_ErrorString (error), NET_AdrToString (fromAddr));
#endif

		return false;
	}

	net_stats.sizeIn += ret;
	net_stats.packetsIn++;

	if (ret == message.maxSize) {
		Com_Printf (PRNT_WARNING, "NET_GetPacket: Oversize packet from %s\n", NET_AdrToString (fromAddr));
		return false;
	}

	message.curSize = ret;
	return true;
}


/*
===================
NET_SendPacket
===================
*/
int NET_SendPacket (netSrc_t sock, int length, void *data, netAdr_t &to)
{
	int				ret;
	struct sockaddr	addr;
	int				netSocket = 0;

	switch (to.naType) {
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
		assert (0);
		Com_Error (ERR_FATAL, "NET_SendPacket: bad address naType");
		break;
	}

	NET_NetAdrToSockAdr (to, &addr);

	ret = sendto (netSocket, (const char*)data, length, 0, &addr, sizeof(addr));
	if (ret == -1) {
		int error = WSAGetLastError ();

		switch (error) {
		// WSAEWOULDBLOCK is silent
		case WSAEWOULDBLOCK:
		case WSAEINTR:
			return 0;

		// Some PPP links dont allow broadcasts
		case WSAEADDRNOTAVAIL:
			if (to.naType == NA_BROADCAST)
				return 0;
			break;

		// Dont warn about connection reset by peer because
		// Windows like spitting this error out for no reason
		case WSAECONNRESET:
#ifndef DEDICATED_ONLY
			if (dedicated->intVal)
#endif
				return -1;
			break;
		}

		Com_Printf (PRNT_ERROR, "NET_SendPacket: %s to %s\n", NET_ErrorString (error), NET_AdrToString (to));
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
	SOCKET	newSocket;
	struct	sockaddr_in		address;
	u_long	hasArgs = 1;
	int		i = 1;

	// Create a socket
	if ((newSocket = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET) {
		int		error = WSAGetLastError ();

		if (error != WSAEAFNOSUPPORT)
			Com_Printf (PRNT_WARNING, "WARNING: NET_GetIPSocket: socket: %s", NET_ErrorString (error));

		return 0;
	}

	// Make it non-blocking
	if (ioctlsocket (newSocket, FIONBIO, &hasArgs) == -1) {
		Com_Printf (PRNT_WARNING, "WARNING: NET_GetIPSocket: ioctl FIONBIO: %s\n", NET_ErrorString (WSAGetLastError ()));
		return 0;
	}

	// Make it broadcast capable
	if (setsockopt (newSocket, SOL_SOCKET, SO_BROADCAST, (char *)&i, sizeof(i)) == -1) {
		Com_Printf (PRNT_WARNING, "WARNING: NET_GetIPSocket: setsockopt SO_BROADCAST: %s\n", NET_ErrorString (WSAGetLastError ()));
		return 0;
	}

	// R1: set 'interactive' ToS
	i = 0x10;
	if (setsockopt (newSocket, IPPROTO_IP, IP_TOS, (char *)&i, sizeof(i)) == -1)
		Com_Printf (0, "WARNING: UDP_OpenSocket: setsockopt IP_TOS: %s\n", NET_ErrorString (WSAGetLastError ()));

	if (!netInterface || !netInterface[0] || !Q_stricmp (netInterface, "localhost"))
		address.sin_addr.s_addr = INADDR_ANY;
	else
		NET_StringToSockaddr (netInterface, (struct sockaddr *)&address);

	address.sin_port = (port == PORT_ANY) ? 0 : htons((sint16)port);
	address.sin_family = AF_INET;

	if (bind (newSocket, (const sockaddr*)((void *)&address), sizeof(address)) == -1) {
		Com_Printf (PRNT_WARNING, "WARNING: NET_GetIPSocket: bind: %s\n", NET_ErrorString (WSAGetLastError ()));
		closesocket (newSocket);
		return 0;
	}

	return (int)newSocket;
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
	netConfig_t			oldest;
	cVar_t				*ip;
	int					port;

	if (oldFlags == openFlags)
		return oldFlags;

	memset (&net_stats, 0, sizeof(net_stats));

	if (openFlags == NET_NONE) {
		oldest = oldFlags;
		oldFlags = NET_NONE;

		// Shut down any existing sockets
		if (net_ipSockets[NS_CLIENT]) {
			closesocket (net_ipSockets[NS_CLIENT]);
			net_ipSockets[NS_CLIENT] = 0;
		}

		if (net_ipSockets[NS_SERVER]) {
			closesocket (net_ipSockets[NS_SERVER]);
			net_ipSockets[NS_SERVER] = 0;
		}
	}
	else {
		oldest = oldFlags;
		oldFlags |= openFlags;

		ip = Cvar_Register ("ip", "localhost", CVAR_READONLY);

		// Open sockets
		net_stats.initTime = time (0);
		net_stats.initialized = true;

		if (openFlags & NET_SERVER)
		{
			if (!net_ipSockets[NS_SERVER])
			{
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
 						Cvar_Set ("clientport", Q_VarArgs ("%d", newport), false);
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
	int		socket;

#ifndef DEDICATED_ONLY
	if (!dedicated->intVal)
		return; // we're not a server, just run full speed
#endif

	FD_ZERO (&fdset);
	if (net_ipSockets[NS_SERVER]) {
		FD_SET (net_ipSockets[NS_SERVER], &fdset); // network socket
		socket = net_ipSockets[NS_SERVER];
	}
	else
		socket = 0;

	timeout.tv_sec = msec / 1000;
	timeout.tv_usec = (msec % 1000) * 1000;
	select (socket+1, &fdset, NULL, NULL, &timeout);
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
NET_Restart_f
====================
*/
static void NET_Restart_f ()
{
	netConfig_t		old;

	old = NET_Config (NET_NONE);

	NET_Config (old);
}


/*
====================
NET_ShowIP_f
====================
*/
static void NET_ShowIP_f ()
{
	char			s[512];
	int				i;
	struct hostent	*h;
	struct in_addr	in;

	gethostname (s, sizeof(s));
	if (!(h = gethostbyname (s))) {
		Com_Printf (PRNT_ERROR, "Can't get host\n");
		return;
	}

	Com_Printf (0, "Hostname: %s\n", h->h_name);

	for (i=0 ; h->h_addr_list[i] ; i++) {
		in.s_addr = *(int *)h->h_addr_list[i];

		Com_Printf (0, "IP Address: %s\n", inet_ntoa (in));
	}
}


/*
====================
NET_Stats_f
====================
*/
static void NET_Stats_f ()
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

static conCmd_t	*cmd_showIP;
static conCmd_t	*cmd_net_restart;
static conCmd_t	*cmd_net_stats;

/*
====================
NET_Init
====================
*/
void NET_Init ()
{
	WORD	wVerRequested; 
	int		wStartupError;

	// Clear stats
	memset (&net_stats, 0, sizeof(net_stats));

	// Startup winsock
	wVerRequested = MAKEWORD (1, 1); 
	wStartupError = WSAStartup (wVerRequested, &net_wsaData);
	if (wStartupError)
		Com_Error (ERR_FATAL, "Winsock initialization failed.");

	Com_Printf (0, "\nWinsock initialized\n");

	NET_ShowIP_f ();

	// Add commands
	cmd_showIP		= Cmd_AddCommand("showip",			0, NET_ShowIP_f,		"Prints your IP to the console");
	cmd_net_restart	= Cmd_AddCommand("net_restart",	0, NET_Restart_f,		"Restarts net subsystem");
	cmd_net_stats	= Cmd_AddCommand("net_stats",		0, NET_Stats_f,		"Prints out connection information");
}


/*
====================
NET_Shutdown
====================
*/
void NET_Shutdown ()
{
	// Remove commands
	Cmd_RemoveCommand(cmd_showIP);
	Cmd_RemoveCommand(cmd_net_restart);
	Cmd_RemoveCommand(cmd_net_stats);

	// Clear stats
	memset (&net_stats, 0, sizeof(net_stats));

	// Close sockets
	NET_Config (NET_NONE);

	WSACleanup ();
}
