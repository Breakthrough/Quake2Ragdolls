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
// net_msg.c
//

#include "common.h"

/*
==============================================================================

	SUPPORTING FUNCTIONS

==============================================================================
*/

/*
================
MSG_Init
================
*/
void netMsg_t::Init (byte *data, int length)
{
	assert (length > 0);

	allowOverflow = overFlowed = false;
	maxSize = curSize = readCount = 0;

	this->data = data;
	this->maxSize = length;
	this->bufferSize = length;
}


/*
================
MSG_Clear
================
*/
void netMsg_t::Clear ()
{
	allowOverflow = overFlowed = false;
	curSize = readCount = 0;

#ifdef _DEBUG
	memset (data, 0xcc, bufferSize);
#endif
}

/*
==============================================================================

	WRITING FUNCTIONS

==============================================================================
*/

/*
================
MSG_GetWriteSpace
================
*/
void *netMsg_t::GetWriteSpace (int length)
{
	void	*data;

	assert (length > 0);
	if (curSize + length > maxSize) {
		if (!this->data)
			Com_Error (ERR_FATAL, "MSG_GetWriteSpace: attempted to write %d bytes to an uninitialized buffer!", length);

		if (!allowOverflow) {
			if (length > maxSize)
				Com_Error (ERR_FATAL, "MSG_GetWriteSpace: %i is > full buffer size %d (%d)", length, maxSize, bufferSize);

			Com_Error (ERR_FATAL, "MSG_GetWriteSpace: overflow without allowOverflow set (%d+%d > %d)", curSize, length, maxSize);
		}

		// R1: clear the buffer BEFORE the error!! (for console buffer)
		if (curSize + length >= bufferSize) {
			Clear();
			Com_Printf (PRNT_WARNING, "MSG_GetWriteSpace: overflow\n");
		}
		else
			Com_Printf (PRNT_WARNING, "MSG_GetWriteSpace: overflowed maxSize\n");

		overFlowed = true;
	}

	data = this->data + curSize;
	curSize += length;
	return data;
}


/*
================
MSG_WriteByte
================
*/
//#define DEBUG_MAGIC
bool _tryMagic = true;

void netMsg_t::WriteByte (int c)
{
	assert (!(c < 0 || c > 255));

	byte *buf = (byte*)GetWriteSpace (1);
	buf[0] = c;

#ifdef DEBUG_MAGIC
	if (_tryMagic)
	{
		_tryMagic = false;
		WriteByte(123);
		_tryMagic = true;
	}
#endif
}


/*
================
MSG_WriteChar
================
*/
void netMsg_t::WriteChar (int c)
{
	byte	*buf;

	assert (!(c < -128 || c > 127));

	buf = (byte*)GetWriteSpace (1);
	buf[0] = c;
}


/*
==================
MSG_WriteDeltaEntity

Writes part of a packetentities message.
Can delta from either a baseline or a previous packet_entity
==================
*/
void netMsg_t::WriteDeltaEntity (entityState_t *from, entityState_t *to, bool force, bool newEntity)
{
	int		bits;

	if (!to) {
		bits = U_REMOVE;
		if (from->number >= 256)
			bits |= U_NUMBER16 | U_MOREBITS1;

		WriteByte (bits&255);
		if (bits & 0x0000ff00)
			WriteByte ((bits>>8)&255);

		if (bits & U_NUMBER16)
			WriteShort (from->number);
		else
			WriteByte (from->number);
		WriteByte(from->type);
		return;
	}

	if (!to->number)
		Com_Error (ERR_FATAL, "MSG_WriteDeltaEntity: Unset entity number");
	if (to->number >= MAX_CS_EDICTS)
		Com_Error (ERR_FATAL, "MSG_WriteDeltaEntity: Entity number >= MAX_CS_EDICTS");

	// Send an update
	bits = 0;
	if (to->number >= 256)
		bits |= U_NUMBER16;		// number8 is implicit otherwise

	if (to->origin[0] != from->origin[0])		bits |= U_ORIGIN1;
	if (to->origin[1] != from->origin[1])		bits |= U_ORIGIN2;
	if (to->origin[2] != from->origin[2])		bits |= U_ORIGIN3;

	if (to->angles[0] != from->angles[0])		bits |= U_ANGLE1;		
	if (to->angles[1] != from->angles[1])		bits |= U_ANGLE2;
	if (to->angles[2] != from->angles[2])		bits |= U_ANGLE3;

	if (to->quat[0] != from->quat[0] ||
		to->quat[1] != from->quat[1] ||
		to->quat[2] != from->quat[2] ||
		to->quat[3] != from->quat[3])
		bits |= U_QUAT;
		
	if (to->skinNum != from->skinNum) {
		if ((uint32)to->skinNum < 256)			bits |= U_SKIN8;
		else if ((uint32)to->skinNum < 0x10000)	bits |= U_SKIN16;
		else									bits |= (U_SKIN8|U_SKIN16);
	}
		
	if ((to->type & ET_ANIMATION) && (to->animation != from->animation ||
		to->animation != to->oldAnimation))
		bits |= U_ANIM;

	if (to->effects != from->effects) {
		if (to->effects < 256)			bits |= U_EFFECTS8;
		else if (to->effects < 0x8000)	bits |= U_EFFECTS16;
		else							bits |= U_EFFECTS8|U_EFFECTS16;
	}
	
	if (to->renderFx != from->renderFx) {
		if (to->renderFx < 256)			bits |= U_RENDERFX8;
		else if (to->renderFx < 0x8000)	bits |= U_RENDERFX16;
		else							bits |= U_RENDERFX8|U_RENDERFX16;
	}
	
	if (to->solid != from->solid)
		bits |= U_SOLID;

	// Event is not delta compressed, just 0 compressed
	if (to->events[0].ID)
		bits |= U_EVENT1;
	if (to->events[1].ID)
		bits |= U_EVENT2;
	
	if (to->modelIndex != from->modelIndex)		bits |= U_MODEL;
	if (to->modelIndex2 != from->modelIndex2)	bits |= U_MODEL2;
	if (to->modelIndex3 != from->modelIndex3)	bits |= U_MODEL3;
	if (to->color != from->color)
		bits |= U_COLOR;

	if (to->sound != from->sound)
		bits |= U_SOUND;

	if (((to->type & ET_EVENT) && !Vec3Compare(to->oldOrigin, vec3Origin)) || (newEntity || to->renderFx & RF_FRAMELERP || to->renderFx & RF_BEAM))
		bits |= U_OLDORIGIN;

	//
	// Write the message
	//
	if (!bits && !force)
		return;		// Nothing to send!

	//----------

	if (bits & 0xff000000)		bits |= U_MOREBITS3 | U_MOREBITS2 | U_MOREBITS1;
	else if (bits & 0x00ff0000)	bits |= U_MOREBITS2 | U_MOREBITS1;
	else if (bits & 0x0000ff00)	bits |= U_MOREBITS1;

	WriteByte (bits&255);
	if (bits & 0xff000000) {
		WriteByte ((bits>>8)&255);
		WriteByte ((bits>>16)&255);
		WriteByte ((bits>>24)&255);
	}
	else if (bits & 0x00ff0000) {
		WriteByte ((bits>>8)&255);
		WriteByte ((bits>>16)&255);
	}
	else if (bits & 0x0000ff00) {
		WriteByte ((bits>>8)&255);
	}

	//----------

	if (bits & U_NUMBER16)	WriteShort (to->number);
	else					WriteByte (to->number);

	// fixme: always send? type shouldn't change
	WriteByte(to->type);

	if (bits & U_MODEL)		WriteByte (to->modelIndex);
	if (bits & U_MODEL2)	WriteByte (to->modelIndex2);
	if (bits & U_MODEL3)	WriteByte (to->modelIndex3);
	if (bits & U_COLOR)
		WriteLong (*(int*)&to->color.R);

	if (bits & U_ANIM)	WriteChar (to->animation);

	// Used for laser colors
	if ((bits & U_SKIN8) && (bits & U_SKIN16))	WriteLong (to->skinNum);
	else if (bits & U_SKIN8)					WriteByte (to->skinNum);
	else if (bits & U_SKIN16)					WriteShort (to->skinNum);

	if ((bits & (U_EFFECTS8|U_EFFECTS16)) == (U_EFFECTS8|U_EFFECTS16))		WriteLong (to->effects);
	else if (bits & U_EFFECTS8)		WriteByte (to->effects);
	else if (bits & U_EFFECTS16)	WriteShort (to->effects);

	if ((bits & (U_RENDERFX8|U_RENDERFX16)) == (U_RENDERFX8|U_RENDERFX16))	WriteLong (to->renderFx);
	else if (bits & U_RENDERFX8)	WriteByte (to->renderFx);
	else if (bits & U_RENDERFX16)	WriteShort (to->renderFx);

	if (bits & U_ORIGIN1)	WriteCoord (to->origin[0]);		
	if (bits & U_ORIGIN2)	WriteCoord (to->origin[1]);
	if (bits & U_ORIGIN3)	WriteCoord (to->origin[2]);

	if (to->type & ET_MISSILE)
	{
		if (bits & U_ANGLE1)	WriteShort (to->angles[0] / (1.0f/32));
		if (bits & U_ANGLE2)	WriteShort (to->angles[1] / (1.0f/32));
		if (bits & U_ANGLE3)	WriteShort (to->angles[2] / (1.0f/32));
	}
	else
	{
		if (bits & U_ANGLE1)	WriteAngle16 (to->angles[0]);
		if (bits & U_ANGLE2)	WriteAngle16 (to->angles[1]);
		if (bits & U_ANGLE3)	WriteAngle16 (to->angles[2]);
	}

	if (bits & U_QUAT)
	{
		byte quadBits = 0;

		for (int i = 0; i < 4; ++i)
		{
			if (fabs(to->quat[i]) +	fabs(from->quat[i]) > 0.005)
				quadBits |= BIT(i);
		}

		WriteByte(quadBits);

		for (int i = 0; i < 4; ++i)
		{
			if (quadBits & BIT(i))
				WriteAngle16_Compress(to->quat[i]);
		}
	}

	if (bits & U_OLDORIGIN) {
		WriteCoord (to->oldOrigin[0]);
		WriteCoord (to->oldOrigin[1]);
		WriteCoord (to->oldOrigin[2]);
	}

	if (bits & U_SOUND)	WriteByte (to->sound);

	if ( bits & U_EVENT1 )
	{
		WriteByte (to->events[0].ID );
		WriteByte (to->events[0].parms.Count());

		if (to->events[0].parms.Count())
			WriteRaw(to->events[0].parms.Pointer(), to->events[0].parms.Count());
	}

	if ( bits & U_EVENT2 )
	{
		WriteByte (to->events[1].ID );
		WriteByte (to->events[1].parms.Count());

		if (to->events[1].parms.Count())
			WriteRaw(to->events[1].parms.Pointer(), to->events[1].parms.Count());
	}

	if (bits & U_SOLID)	WriteShort (to->solid);
}


/*
================
MSG_WriteDeltaUsercmd
================
*/
void netMsg_t::WriteDeltaUsercmd (userCmd_t *from, userCmd_t *cmd, int protocolMinorVersion)
{
	int bits;
	int buttons;

	// Send the movement message
	bits = 0;
	buttons = 0;

	if (cmd->angles[0] != from->angles[0])		bits |= CM_ANGLE1;
	if (cmd->angles[1] != from->angles[1])		bits |= CM_ANGLE2;
	if (cmd->angles[2] != from->angles[2])		bits |= CM_ANGLE3;
	if (cmd->forwardMove != from->forwardMove)	bits |= CM_FORWARD;
	if (cmd->sideMove != from->sideMove)		bits |= CM_SIDE;
	if (cmd->upMove != from->upMove)			bits |= CM_UP;
	if (cmd->buttons != from->buttons)
	{
		buttons = cmd->buttons;
		bits |= CM_BUTTONS;
	}
	if (cmd->impulse != from->impulse)			bits |= CM_IMPULSE;

	WriteByte (bits);

	//waste not what precious bytes we have...
	if (protocolMinorVersion >= MINOR_VERSION_R1Q2_UCMD_UPDATES)
	{
		if (bits & CM_BUTTONS)
		{
			if ((bits & CM_FORWARD) && (cmd->forwardMove % 5) == 0)
				buttons |= BUTTON_UCMD_DBLFORWARD;
			if ((bits & CM_SIDE) && (cmd->sideMove % 5) == 0)
				buttons |= BUTTON_UCMD_DBLSIDE;
			if ((bits & CM_UP) && (cmd->upMove % 5) == 0)
				buttons |= BUTTON_UCMD_DBLUP;

			if ((bits & CM_ANGLE1) && (cmd->angles[0] % 64) == 0 && (abs(cmd->angles[0] / 64)) < 128)
				buttons |= BUTTON_UCMD_DBL_ANGLE1;
			if ((bits & CM_ANGLE2) && (cmd->angles[1] % 256) == 0)
				buttons |= BUTTON_UCMD_DBL_ANGLE2;

			WriteByte (buttons);
		}
	}

	if (bits & CM_ANGLE1)
	{
		if (buttons & BUTTON_UCMD_DBL_ANGLE1)
			WriteChar (cmd->angles[0] / 64);
		else
			WriteShort (cmd->angles[0]);
	}
	if (bits & CM_ANGLE2)
	{
		if (buttons & BUTTON_UCMD_DBL_ANGLE2)
			WriteChar (cmd->angles[1] / 256);
		else
			WriteShort (cmd->angles[1]);
	}
	if (bits & CM_ANGLE3)	WriteShort (cmd->angles[2]);
	
	if (bits & CM_FORWARD)
	{
		if (buttons & BUTTON_UCMD_DBLFORWARD)
			WriteChar (cmd->forwardMove / 5);
		else
			WriteShort (cmd->forwardMove);
	}
	if (bits & CM_SIDE)
	{
		if (buttons & BUTTON_UCMD_DBLSIDE)
			WriteChar (cmd->sideMove / 5);
		else
			WriteShort (cmd->sideMove);
	}
	if (bits & CM_UP)
	{
		if (buttons & BUTTON_UCMD_DBLUP)
			WriteChar (cmd->upMove / 5);
		else
			WriteShort (cmd->upMove);
	}

	if (protocolMinorVersion < MINOR_VERSION_R1Q2_UCMD_UPDATES)
	{
		if (bits & CM_BUTTONS)
			WriteByte (cmd->buttons);
	}

	if (bits & CM_IMPULSE)	WriteByte (cmd->impulse);

	WriteByte (cmd->msec);
	WriteByte (cmd->lightLevel);
}


/*
================
MSG_WriteDir
================
*/
void netMsg_t::WriteDir (vec3_t dir)
{
	byte	best = DirToByte (dir);
	WriteByte (best);
}

/*
================
MSG_WriteFloat
================
*/
void netMsg_t::WriteFloat (float f)
{
	union
	{
		float	f;
		int		l;
	} dat;

	dat.f = f;
	dat.l = LittleLong (dat.l);

	WriteRaw (&dat.l, 4);

#ifdef DEBUG_MAGIC
	_tryMagic = false;
	WriteByte(154);
	_tryMagic = true;
#endif
}


/*
================
MSG_WriteInt3
================
*/
void netMsg_t::WriteInt3 (int c)
{
	byte	*buf = (byte*)GetWriteSpace (3);
	buf[0] = c&0xff;
	buf[1] = (c>>8)&0xff;
	buf[2] = (c>>16)&0xff;
}


/*
================
MSG_WriteLong
================
*/
void netMsg_t::WriteLong (int c)
{
	byte	*buf = (byte*)GetWriteSpace (4);
	buf[0] = c&0xff;
	buf[1] = (c>>8)&0xff;
	buf[2] = (c>>16)&0xff;
	buf[3] = c>>24;

#ifdef DEBUG_MAGIC
	_tryMagic = false;
	WriteByte(122);
	_tryMagic = true;
#endif

}

#include "../minizip/unzip.h"

void netMsg_t::CompressFrom(byte *buffer, int len, netMsg_t msg) 
{
	Init(buffer, len);

	byte *tempBuffer = new byte[len];

	int compLen = FS_ZLibCompressChunk(msg.data, msg.curSize, tempBuffer, len, Z_DEFAULT_COMPRESSION, -15);

	WriteByte(SVC_ZPACKET);
	WriteShort(compLen);
	WriteShort(msg.curSize);
	WriteRaw(tempBuffer, compLen);

	delete[] tempBuffer;
}

/*
================
MSG_WriteRaw
================
*/
void netMsg_t::WriteRaw(const void *data, int length)
{
	assert(length > 0);
	memcpy(GetWriteSpace (length), data, length);		
}


/*
================
MSG_WriteShort
================
*/
void netMsg_t::WriteShort (int c)
{
	byte	*buf;

	buf = (byte*)GetWriteSpace (2);
	buf[0] = c&0xff;
	buf[1] = c>>8;

#ifdef DEBUG_MAGIC
	_tryMagic = false;
	WriteByte(74);
	_tryMagic = true;
#endif
}


/*
================
MSG_WriteString
================
*/
void netMsg_t::WriteString(const char *s)
{
	if (!s)
		WriteRaw("", 1);
	else
		WriteRaw(s, (int)strlen(s)+1);
}


/*
================
MSG_WriteStringCat
================
*/
void netMsg_t::WriteStringCat(const char *data)
{	
	int len = strlen(data)+1;
	assert(len > 1);

	if (curSize)
	{
		if (this->data[curSize-1])
			memcpy((byte *)GetWriteSpace(len), data, len); // no trailing 0
		else
			memcpy((byte *)GetWriteSpace(len-1)-1, data, len); // write over trailing 0
	}
	else
	{
		memcpy((byte *)GetWriteSpace (len), data, len);
	}
}

/*
==============================================================================

	READING FUNCTIONS

==============================================================================
*/

/*
================
MSG_BeginReading
================
*/
void netMsg_t::BeginReading ()
{
	readCount = 0;
}


/*
================
MSG_ReadByte
================
*/
bool _tryReadByte = true;

int netMsg_t::ReadByte ()
{
	int c;

	if (readCount+1 > curSize)
		c = -1;
	else
		c = (byte)data[readCount];
	readCount++;

#ifdef DEBUG_MAGIC
	if (_tryReadByte)
	{
		_tryReadByte = false;
		if (ReadByte() != 123)
			assert(0);
		_tryReadByte = true;
	}
#endif

	return c;
}


/*
================
MSG_ReadChar

returns -1 if no more characters are available
================
*/
int netMsg_t::ReadChar ()
{
	int c;

	if (readCount+1 > curSize)
		c = -1;
	else
		c = (signed char)data[readCount];
	readCount++;

	return c;
}


/*
================
MSG_ReadData
================
*/
void netMsg_t::ReadData (void *data, int len)
{
	int		i;

	for (i=0 ; i<len ; i++)
		((byte *)data)[i] = ReadByte ();
}


/*
================
MSG_ReadDeltaUsercmd
================
*/
void netMsg_t::ReadDeltaUsercmd (userCmd_t *from, userCmd_t *move)
{
	int bits;

	memcpy (move, from, sizeof(*move));

	bits = ReadByte ();
		
	// Read current angles
	if (bits & CM_ANGLE1)
		move->angles[0] = ReadShort ();
	if (bits & CM_ANGLE2) 
		move->angles[1] = ReadShort ();
	if (bits & CM_ANGLE3) 
		move->angles[2] = ReadShort ();
		
	// Read movement
	if (bits & CM_FORWARD)
		move->forwardMove = ReadShort ();
	if (bits & CM_SIDE)
		move->sideMove = ReadShort ();
	if (bits & CM_UP)
		move->upMove = ReadShort ();
	
	// Read buttons
	if (bits & CM_BUTTONS)
		move->buttons = ReadByte ();

	if (bits & CM_IMPULSE)
		move->impulse = ReadByte ();

	// Read time to run command
	move->msec = ReadByte ();

	// Read the light level
	move->lightLevel = ReadByte ();
}


/*
================
MSG_ReadDir
================
*/
void netMsg_t::ReadDir (vec3_t dir)
{
	byte	b = ReadByte ();
	ByteToDir (b, dir);
}


/*
================
MSG_ReadFloat
================
*/
float netMsg_t::ReadFloat ()
{
#ifdef DEBUG_MAGIC
	_tryReadByte = false;
	int magic = ReadByte();
	_tryReadByte = true;
	if (magic != 154)
		assert(0);
#endif

	union
	{
		byte	b[4];
		float	f;
		int		l;
	} dat;

	if (readCount+4 > curSize)
		dat.f = -1;
	else
	{
		dat.b[0] = data[readCount];
		dat.b[1] = data[readCount+1];
		dat.b[2] = data[readCount+2];
		dat.b[3] = data[readCount+3];
	}
	
	readCount += 4;

	dat.l = LittleLong (dat.l);

	return dat.f;	
}


/*
================
MSG_ReadInt3
================
*/
int netMsg_t::ReadInt3 ()
{
	int	c;

	if (readCount+3 > curSize)
		c = -1;
	else
	{
		c = data[readCount]
		| (data[readCount+1]<<8)
		| (data[readCount+2]<<16)
		| ((data[readCount+2] & 0x80) ? ~0xFFFFFF : 0);
	}
	readCount += 3;

	return c;
}


/*
================
MSG_ReadLong
================
*/
int netMsg_t::ReadLong ()
{
	int	c;

	if (readCount+4 > curSize)
		c = -1;
	else
	{
		c = data[readCount]
		+ (data[readCount+1]<<8)
		+ (data[readCount+2]<<16)
		+ (data[readCount+3]<<24);
	}
	readCount += 4;

#ifdef DEBUG_MAGIC
	_tryReadByte = false;
	int magic = ReadByte();
	_tryReadByte = true;
	if (magic != 122)
		assert(0);
#endif


	return c;
}


/*
================
MSG_ReadShort
================
*/
short netMsg_t::ReadShort ()
{
	short c;

	if (readCount+2 > curSize)
		c = -1;
	else
	{
		c = (sint16)(data[readCount]
		+ (data[readCount+1]<<8));
	}
	readCount += 2;

#ifdef DEBUG_MAGIC
	_tryReadByte = false;
	int magic = ReadByte();
	_tryReadByte = true;
	if (magic != 74)
		assert(0);
#endif

	return c;
}


/*
================
MSG_ReadString
================
*/
char *netMsg_t::ReadString ()
{
	static char	string[2048];
	int			l, c;

	l = 0;
	do {
		c = ReadByte ();
		if (c == -1 || c == 0)
			break;

		string[l] = c;
		l++;
	} while (l < sizeof(string)-1);

	string[l] = 0;

	return string;
}


/*
================
MSG_ReadStringLine
================
*/
char *netMsg_t::ReadStringLine ()
{
	static char	string[2048];
	int			l, c;
		
	l = 0;
	do {
		c = ReadByte ();
		if (c == -1 || c == 0 || c == '\n')
			break;

		string[l] = c;
		l++;
	} while (l < sizeof(string)-1);

	string[l] = 0;

	return string;
}
