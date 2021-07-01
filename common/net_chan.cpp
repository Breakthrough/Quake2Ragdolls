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
// net_chan.c
//

#include "common.h"

/*
packet header
-------------
31	sequence
1	does this message contain a reliable payload
31	acknowledge sequence
1	acknowledge receipt of even/odd message
16	qport

The remote connection never knows if it missed a reliable message, the
local side detects that it has been dropped by seeing a sequence acknowledge
higher thatn the last reliable sequence, but without the correct evon/odd
bit for the reliable set.

If the sender notices that a reliable message has been dropped, it will be
retransmitted.  It will not be retransmitted again until a message after
the retransmit has been acknowledged and the reliable still failed to get there.

if the sequence number is -1, the packet should be handled without a netcon

The reliable message can be added to at any time by doing
MSG_Write* (<data>).

If the message buffer is overflowed, either by a single message, or by
multiple frames worth piling up while the last reliable transmit goes
unacknowledged, the netchan signals a fatal error.

Reliable messages are always placed first in a packet, then the unreliable
message is included if there is sufficient room.

To the receiver, there is no distinction between the reliable and unreliable
parts of the message, they are just processed out as a single larger message.

Illogical packet sequence numbers cause the packet to be dropped, but do
not kill the connection.  This, combined with the tight window of valid
reliable acknowledgement numbers provides protection against malicious
address spoofing.


The qport field is a workaround for bad address translating routers that
sometimes remap the client's source port on a packet during gameplay.

If the base part of the net address matches and the qport matches, then the
channel matches even if the IP port differs.  The IP port should be updated
to the new value before sending out any replies.


If there is no information that needs to be transfered on a given frame,
such as during the connection stage while waiting for the client to load,
then a packet only needs to be delivered if there is something in the
unacknowledged reliable
*/

cVar_t	*showpackets;
cVar_t	*showdrop;
cVar_t	*qport;
cVar_t	*net_maxMsgLen;

/*
===============
Netchan_Init
===============
*/
void Netchan_Init ()
{
	int		port;

	// Pick a port value that should be nice and random
	port = Sys_Milliseconds () & 0xffff;

	showpackets		= Cvar_Register ("showpackets",		"0",									0);
	showdrop		= Cvar_Register ("showdrop",		"0",									0);
	qport			= Cvar_Register ("qport",			Q_VarArgs ("%i", port),					0);
	net_maxMsgLen	= Cvar_Register ("net_maxMsgLen",	Q_VarArgs ("%i", MAX_SV_USABLEMSG),		0);
}


/*
===============
Netchan_OutOfBand

Sends an out-of-band datagram
================
*/
void Netchan_OutOfBand (netSrc_t netSocket, netAdr_t &adr, int length, byte *data)
{
	netMsg_t	send;
	byte		sendBuf[MAX_CL_MSGLEN];

	// Write the packet header
	send.Init(sendBuf, sizeof(sendBuf));

	send.WriteLong (-1);	// -1 sequence means out of band
	send.WriteRaw (data, length);

	// Send the datagram
	NET_SendPacket (netSocket, send.curSize, send.data, adr);
}


/*
===============
Netchan_OutOfBandPrint

Sends a text message in an out-of-band datagram
================
*/
void Netchan_OutOfBandPrint (netSrc_t netSocket, netAdr_t &adr, char *format, ...)
{
	va_list		argptr;
	static char	string[MAX_CL_MSGLEN - 4];

	va_start (argptr, format);
	if (vsnprintf (string, sizeof(string)-1, format, argptr) < 0)
		Com_Printf (PRNT_WARNING, "WARNING: Netchan_OutOfBandPrint: message overflow.\n");
	va_end (argptr);

	Netchan_OutOfBand (netSocket, adr, (int)strlen(string), (byte *)string);
}


/*
==============
Netchan_Setup

called to open a channel to a remote system
==============
*/
void Netchan_Setup (netSrc_t sock, netChan_t &chan, netAdr_t &adr, int protocol, int qPort, uint32 msgLen)
{
	memset (&chan, 0, sizeof(chan));

	chan.sock = sock;
	chan.remoteAddress = adr;

	if (protocol == ENHANCED_PROTOCOL_VERSION) {
		if (msgLen) {
			if (msgLen > MAX_CL_USABLEMSG)
				Com_Error (ERR_DROP, "Netchan_Setup: msgLen > MAX_CL_USABLEMSG (%i > %i)", msgLen, MAX_CL_USABLEMSG);
			chan.message.Init(chan.messageBuff, msgLen);
		}
		else {
			chan.message.Init(chan.messageBuff, MAX_SV_USABLEMSG);
			msgLen = MAX_SV_USABLEMSG;
		}

		qPort &= 0xff;
	}
	else {
		chan.message.Init(chan.messageBuff, MAX_SV_USABLEMSG);
		msgLen = MAX_SV_USABLEMSG;
	}

	chan.qPort = qPort;
	chan.protocol = protocol;
	chan.lastReceived = Sys_Milliseconds ();
	chan.incomingSequence = 0;
	chan.outgoingSequence = 1;
	chan.message.allowOverflow = true;

	Com_DevPrintf (0, "Netchan_Setup: sock=%d, protocol=%d, qport=%d, msgLen=%u\n", sock, protocol, qPort, msgLen);
}


/*
===============
Netchan_NeedReliable
================
*/
static inline bool Netchan_NeedReliable (netChan_t &chan)
{
	// If the remote side dropped the last reliable message, resend it
	if (chan.incomingAcknowledged > chan.lastReliableSequence
	&& chan.incomingReliableAcknowledged != chan.reliableSequence)
		return true;

	// If the reliable transmit buffer is empty, copy the current message out
	if (!chan.reliableLength && chan.message.curSize)
		return true;

	return false;
}


/*
===============
Netchan_Transmit

tries to send an unreliable message to a connection, and handles the
transmition / retransmition of the reliable messages.

A 0 length will still generate a packet and deal with the reliable messages.
================
*/
int Netchan_Transmit (netChan_t &chan, int length, byte *data)
{
	netMsg_t	send;
	byte		sendBuf[MAX_CL_MSGLEN];
	bool		sendReliable;
	uint32		w1, w2;

	// Check for message overflow
	if (chan.message.overFlowed || chan.message.curSize >= MAX_CL_MSGLEN) {
		chan.fatalError = true;
		Com_Printf (PRNT_WARNING, "%s: Outgoing message overflow\n", NET_AdrToString (chan.remoteAddress));
		return -2;
	}

	sendReliable = Netchan_NeedReliable (chan);

	if (!chan.reliableLength && chan.message.curSize) {
		memcpy (chan.reliableBuff, chan.messageBuff, chan.message.curSize);
		chan.reliableLength = chan.message.curSize;
		chan.message.curSize = 0;
		chan.reliableSequence ^= 1;
	}

	// Write the packet header
	if (chan.protocol == ENHANCED_PROTOCOL_VERSION)
		send.Init(sendBuf, MAX_CL_MSGLEN);
	else
		send.Init(sendBuf, MAX_SV_MSGLEN);

	w1 = (chan.outgoingSequence & ~BIT(31)) | (sendReliable<<31);
	w2 = (chan.incomingSequence & ~BIT(31)) | (chan.incomingReliableSequence<<31);

	chan.outgoingSequence++;
	chan.lastSent = Sys_Milliseconds ();

	send.WriteLong (w1);
	send.WriteLong (w2);

	// Send the qport if we are a client
	if (chan.sock == NS_CLIENT) {
		if (chan.protocol != ENHANCED_PROTOCOL_VERSION)
			send.WriteShort (chan.qPort);
		else if (chan.qPort)
			send.WriteByte (chan.qPort & 0xff);
	}

	// Copy the reliable message to the packet first
	if (sendReliable) {
		if (chan.reliableLength)
			send.WriteRaw (chan.reliableBuff, chan.reliableLength);
		chan.lastReliableSequence = chan.outgoingSequence;
	}

	// Add the unreliable part if space is available
	if (send.maxSize - send.curSize >= length) {
		if (length)
			send.WriteRaw (data, length);
	}
	else
		Com_Printf (PRNT_WARNING, "Netchan_Transmit: dumped unreliable\n");

	// Send the datagram
	if (NET_SendPacket (chan.sock, send.curSize, send.data, chan.remoteAddress) == -1)
		return -1;

	if (showpackets->intVal) {
		if (sendReliable)
			Com_Printf (0, "send %4i : s=%i reliable=%i ack=%i rack=%i\n",
				send.curSize,
				chan.outgoingSequence - 1,
				chan.reliableSequence,
				chan.incomingSequence,
				chan.incomingReliableSequence);
		else
			Com_Printf (0, "send %4i : s=%i ack=%i rack=%i\n",
				send.curSize,
				chan.outgoingSequence - 1,
				chan.incomingSequence,
				chan.incomingReliableSequence);
	}

	return 0;
}


/*
=================
Netchan_Process

called when the current msg is from remoteAddress
modifies msg so that it points to the packet payload
=================
*/
bool Netchan_Process (netChan_t &chan, netMsg_t &msg)
{
	uint32		sequence, sequenceAck;
	uint32		reliableAck, reliableMessage;

	msg.BeginReading ();

	// Get sequence numbers	
	sequence = msg.ReadLong ();
	sequenceAck = msg.ReadLong ();

	// Read the qport if we are a server
	if (chan.sock == NS_SERVER) {
		assert (chan.protocol == ORIGINAL_PROTOCOL_VERSION);
		if (chan.qPort)
			msg.ReadShort ();
	}

	reliableMessage = sequence >> 31;
	reliableAck = sequenceAck >> 31;
	chan.gotReliable = reliableMessage ? true : false;

	sequence &= ~BIT(31);
	sequenceAck &= ~BIT(31);

	if (showpackets->intVal) {
		if (reliableMessage)
			Com_Printf (0, "recv %4i : s=%i reliable=%i ack=%i rack=%i\n",
				msg.curSize,
				sequence,
				chan.incomingReliableSequence ^ 1,
				sequenceAck,
				reliableAck);
		else
			Com_Printf (0, "recv %4i : s=%i ack=%i rack=%i\n",
				msg.curSize,
				sequence,
				sequenceAck,
				reliableAck);
	}

	// Discard stale or duplicated packets
	if ((int)sequence <= chan.incomingSequence) {
		if (showdrop->intVal)
			Com_Printf (0, "%s:Out of order packet %i at %i\n",
				NET_AdrToString (chan.remoteAddress),
				sequence,
				chan.incomingSequence);
		return false;
	}

	// Dropped packets don't keep the message from being used
	chan.dropped = sequence - (chan.incomingSequence+1);
	if (chan.dropped > 0) {
		if (showdrop->intVal)
			Com_Printf (0, "%s:Dropped %i packets at %i\n",
				NET_AdrToString (chan.remoteAddress),
				chan.dropped,
				sequence);
	}

	// If the current outgoing reliable message has been acknowledged,
	// clear the buffer to make way for the next
	if (reliableAck == chan.reliableSequence)
		chan.reliableLength = 0;	// it has been received
	
	// If this message contains a reliable message, bump incoming_ReliableSequence
	chan.incomingSequence = sequence;
	chan.incomingAcknowledged = sequenceAck;
	chan.incomingReliableAcknowledged = reliableAck;
	if (reliableMessage)
		chan.incomingReliableSequence ^= 1;

	// The message can now be read from the current message pointer
	chan.lastReceived = Sys_Milliseconds ();

	return true;
}
