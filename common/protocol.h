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
// protocol.h
//

/*
=============================================================================

	PROTOCOL

=============================================================================
*/

#define UPDATE_BACKUP		16	// copies of entityState_t to keep buffered must be power of two
#define UPDATE_MASK			(UPDATE_BACKUP-1)

//
// client to server
//
enum {
	CLC_BAD,
	CLC_NOP,
	CLC_MOVE,				// [[userCmd_t]
	CLC_USERINFO,			// [[userinfo string]
	CLC_STRINGCMD,			// [string] message
	CLC_SETTING				// [setting][value] R1Q2 settings support.
};

enum {
	CLSET_NOGUN,
	CLSET_NOBLEND,
	CLSET_RECORDING,
	CLSET_MAX
};

// playerState_t communication
enum {
	PS_M_TYPE			= BIT(0),
	PS_M_ORIGIN			= BIT(1),
	PS_M_VELOCITY		= BIT(2),
	PS_M_TIME			= BIT(3),
	PS_M_FLAGS			= BIT(4),
	PS_M_GRAVITY		= BIT(5),
	PS_M_DELTA_ANGLES	= BIT(6),

	PS_VIEWOFFSET		= BIT(7),
	PS_VIEWANGLES		= BIT(8),
	PS_KICKANGLES		= BIT(9),
	PS_BLEND			= BIT(10),
	PS_FOV				= BIT(11),
	PS_WEAPONINDEX		= BIT(12),
	PS_WEAPONFRAME		= BIT(13),
	PS_RDFLAGS			= BIT(14)
};

// enhanced protocol
enum {
	EPS_PMOVE_VELOCITY2	= BIT(0),
	EPS_PMOVE_ORIGIN2	= BIT(1),
	EPS_VIEWANGLE2		= BIT(2),
	EPS_STATS			= BIT(3)
};


// userCmd_t communication
// ms and light always sent, the others are optional
enum {
	CM_ANGLE1			= BIT(0),
	CM_ANGLE2			= BIT(1),
	CM_ANGLE3			= BIT(2),
	CM_FORWARD			= BIT(3),
	CM_SIDE				= BIT(4),
	CM_UP				= BIT(5),
	CM_BUTTONS			= BIT(6),
	CM_IMPULSE			= BIT(7)
};


// sound communication
// a sound without an ent or pos will be a local only sound
enum {
	SND_VOLUME			= BIT(0),	// a byte
	SND_ATTENUATION		= BIT(1),	// a byte
	SND_POS				= BIT(2),	// three coordinates
	SND_ENT				= BIT(3),	// a short 0-2: channel, 3-12: entity
	SND_OFFSET			= BIT(4)	// a byte, msec offset from frame start
};

#define DEFAULT_SOUND_PACKET_VOLUME			1.0
#define DEFAULT_SOUND_PACKET_ATTENUATION	1.0

// entityState_t communication
enum {
	// try to pack the common update flags into the first byte
	U_ORIGIN1			= BIT(0),
	U_ORIGIN2			= BIT(1),
	U_ANGLE2			= BIT(2),
	U_ANGLE3			= BIT(3),
	U_ANIM				= BIT(4),		// frame is a byte
	U_EVENT1			= BIT(5),
	U_REMOVE			= BIT(6),		// REMOVE this entity, don't add it
	U_MOREBITS1			= BIT(7),		// read one additional byte

	// second byte
	U_NUMBER16			= BIT(8),		// NUMBER8 is implicit if not set
	U_ORIGIN3			= BIT(9),
	U_ANGLE1			= BIT(10),
	U_MODEL				= BIT(11),
	U_RENDERFX8			= BIT(12),		// fullbright, etc
	U_EFFECTS8			= BIT(13),		// autorotate, trails, etc
	U_QUAT				= BIT(14),
	U_MOREBITS2			= BIT(15),		// read one additional byte

	// third byte
	U_SKIN8				= BIT(16),
	U_RENDERFX16		= BIT(17),		// 8 + 16 = 32
	U_EFFECTS16			= BIT(18),		// 8 + 16 = 32
	U_MODEL2			= BIT(19),		// weapons, flags, etc
	U_MODEL3			= BIT(20),
	U_COLOR				= BIT(21),
	U_OLDORIGIN			= BIT(22),		// FIXME: get rid of this
	U_MOREBITS3			= BIT(23),		// read one additional byte

	// fourth byte
	U_SKIN16			= BIT(24),
	U_SOUND				= BIT(25),
	U_SOLID				= BIT(26),
	U_EVENT2			= BIT(27)
};

/*
=============================================================================

	MESSAGE FUNCTIONS

=============================================================================
*/

struct netMsg_t 
{
	bool		allowOverflow;	// if false, do a Com_Error
	bool		overFlowed;		// set to true if the buffer size failed
	byte		*data;
	int			maxSize;
	int			curSize;
	int			readCount;
	int			bufferSize;

	void Clear ();
	void Init(byte *inData, int length);

	void	BeginReading ();
	int		ReadByte ();
	int		ReadChar ();
	void	ReadData (void *buffer, int size);
	void	ReadDeltaUsercmd (struct userCmd_t *from, struct userCmd_t *cmd);
	void	ReadDir (vec3_t vector);
	float	ReadFloat ();
	int		ReadInt3 ();
	int		ReadLong ();
	short	ReadShort ();
	char	*ReadString ();
	char	*ReadStringLine ();

	inline float ReadAngle()
	{
		return BYTE2ANGLE (ReadChar ());
	}

	inline float ReadAngle16()
	{
		return SHORT2ANGLE (ReadShort ());
	}

	inline float ReadCoord()
	{
		return ReadShort() * (1.0f/8.0f);
	}

	inline void ReadPos(vec3_t pos)
	{
		for (int i = 0; i < 3; ++i)
			pos[i] = ReadCoord();
	}

	inline float ReadAngle16_Compress()
	{
		return SHORT2ANGLE_COMPRESS(ReadShort());
	}

	// writing
	void	WriteByte			(int c);
	void	WriteChar			(int c);
	void	WriteDeltaUsercmd	(struct userCmd_t *from, struct userCmd_t *cmd, int protocolMinorVersion);
	void	WriteDeltaEntity	(entityState_t *from, entityState_t *to, bool force, bool newEntity);
	void	WriteDir			(vec3_t vector);
	void	WriteFloat			(float f);
	void	WriteInt3			(int c);
	void	WriteLong			(int c);
	void	WriteRaw			(const void *data, int length);
	void	WriteShort			(int c);
	void	WriteString			(const char *s);
	void	WriteStringCat		(const char *data);

	inline void WriteAngle(float f)
	{
		WriteByte(ANGLE2BYTE(f));
	}

	inline void WriteAngle16(float f)
	{
		WriteShort(ANGLE2SHORT(f));
	}

	inline void WriteCoord(float f)
	{
		WriteShort((int)(f*8));
	}

	inline void WritePos(vec3_t pos)
	{
		for (int i = 0; i < 3; ++i)
			WriteCoord(pos[i]);
	}

	inline void WriteAngle16_Compress(float f)
	{
		WriteShort(ANGLE2SHORT_COMPRESS(f));
	}

	void CompressFrom(byte *buffer, int len, netMsg_t msg);

private:
	void *GetWriteSpace (int length);
};

/*
=============================================================================

	NETCHAN

=============================================================================
*/

// Default ports
#define PORT_ANY		-1
#define PORT_MASTER		27900
#define PORT_SERVER		27910

// Maximum message lengths
#define PACKET_HEADER		10			// two ints and a short
	
#define MAX_SV_MSGLEN		(1400 * 2)
#define MAX_SV_USABLEMSG	(MAX_SV_MSGLEN - PACKET_HEADER)

#define MAX_CL_MSGLEN		(4096 * 2)
#define MAX_CL_USABLEMSG	(MAX_CL_MSGLEN - PACKET_HEADER)

#define MAX_LOOPBACK		4
#define MAX_LOOPBACKMASK	(MAX_LOOPBACK-1)

typedef enum netAdrType_s {
	NA_LOOPBACK,
	NA_BROADCAST,
	NA_IP,

	NA_MAX
} netAdrType_t;

typedef enum netSrc_s {
	NS_CLIENT,
	NS_SERVER,

	NS_MAX
} netSrc_t;

typedef uint32 netConfig_t;
typedef enum {
	NET_NONE,

	NET_CLIENT		= BIT(0),
	NET_SERVER		= BIT(1),
};

struct netAdr_t {
	netAdrType_t	naType;

	byte			ip[4];

	uint16			port;


	// Compares with the port
	bool CompareAdr(const netAdr_t &b)
	{
		return 	(naType != b.naType) ? false :
			(naType == NA_LOOPBACK) ? true :
			(naType == NA_IP) ?
			((ip[0] == b.ip[0]) &&
			(ip[1] == b.ip[1]) &&
			(ip[2] == b.ip[2]) &&
			(ip[3] == b.ip[3]) &&
			(port == b.port)) ? true : false
			: false;
	}

	// Compares without the port
	bool CompareBaseAdr(const netAdr_t &b)
	{
		return (naType != b.naType) ? false :
			(naType == NA_LOOPBACK) ? true :
			(naType == NA_IP) ?
			((ip[0] == b.ip[0]) &&
			(ip[1] == b.ip[1]) &&
			(ip[2] == b.ip[2]) &&
			(ip[3] == b.ip[3])) ? true : false
			: false;
	}

	// Converts from sockaddr_in to netAdr_t
	static netAdr_t FromSockAdr(struct sockaddr_in *s);

	// Checks if an address is a loopback address
	bool		IsLocalAddress ();
};

struct loopMsg_t {
	byte			data[MAX_CL_MSGLEN];
	int				dataLen;
};

struct loopBack_t {
	loopMsg_t		msgs[MAX_LOOPBACK];
	int				get;
	int				send;
};

struct netStats_t {
	bool			initialized;
	uint32			initTime;

	uint32			sizeIn;
	uint32			sizeOut;

	uint32			packetsIn;
	uint32			packetsOut;
};

extern loopBack_t	net_loopBacks[NS_MAX];
extern int			net_ipSockets[NS_MAX];
extern netStats_t	net_stats;

void		NET_Init ();
void		NET_Shutdown ();

netConfig_t NET_Config (netConfig_t openFlags);

bool		NET_GetPacket (netSrc_t sock, netAdr_t &fromAddr, netMsg_t &message);
int			NET_SendPacket (netSrc_t sock, int length, void *data, netAdr_t &to);

char		*NET_AdrToString (netAdr_t &a);
bool		NET_StringToAdr (char *s, netAdr_t &a);

void		NET_Server_Sleep (int msec);
int			NET_Client_Sleep (int msec);

struct netChan_t {
	bool		fatalError;

	netSrc_t	sock;

	int			dropped;						// between last packet and previous

	int			lastReceived;					// for timeouts
	int			lastSent;						// for retransmits

	netAdr_t	remoteAddress;
	uint16		qPort;							// qport value to write when transmitting
	uint16		protocol;

	// Sequencing variables
	int			incomingSequence;
	int			incomingAcknowledged;
	int			incomingReliableAcknowledged;	// single bit
	int			incomingReliableSequence;		// single bit, maintained local
	bool		gotReliable;

	int			outgoingSequence;
	int			reliableSequence;				// single bit
	int			lastReliableSequence;			// sequence number of last send

	// Reliable staging and holding areas
	netMsg_t	message;		// writing buffer to send to server
	byte		messageBuff[MAX_CL_USABLEMSG];	// leave space for header

	// Message is copied to this buffer when it is first transfered
	int			reliableLength;
	byte		reliableBuff[MAX_CL_USABLEMSG];	// unacked reliable message
};

void		Netchan_Init ();
void		Netchan_Setup (netSrc_t sock, netChan_t &chan, netAdr_t &adr, int protocol, int qPort, uint32 msgLen);

int			Netchan_Transmit (netChan_t &chan, int length, byte *data);
void		Netchan_OutOfBand (netSrc_t netSocket, netAdr_t &adr, int length, byte *data);
void		Netchan_OutOfBandPrint (netSrc_t netSocket, netAdr_t &adr, char *format, ...);
bool		Netchan_Process (netChan_t &chan, netMsg_t &msg);
