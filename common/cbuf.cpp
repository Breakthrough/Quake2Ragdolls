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
// cbuf.c
//

#include "common.h"

static netMsg_t	com_cbufText;
static byte		com_cbufTextBuf[COMMAND_BUFFER_SIZE];
static byte		com_cbufDeferTextBuf[COMMAND_BUFFER_SIZE];

/*
=============================================================================

	COMMAND BUFFER

=============================================================================
*/

/*
============
Cbuf_AddText

Adds command text at the end of the buffer
============
*/
void Cbuf_AddText (char *text)
{
	int		l;

	if (!text || !text[0])
		return;

	// Check for room
	l = (int)strlen (text);
	if (com_cbufText.curSize+l >= com_cbufText.maxSize) {
		Com_Printf (PRNT_WARNING, "Cbuf_AddText: overflow\n");
		return;
	}

	// Write
	com_cbufText.WriteRaw (text, l);
}


/*
============
Cbuf_InsertText

Adds command text immediately after the current command
Adds a \n to the text
FIXME: actually change the command buffer to do less copying
============
*/
void Cbuf_InsertText (char *text)
{
	char	temp[COMMAND_BUFFER_SIZE];
	int		tempLen;

	// Copy off any commands still remaining in the exec buffer
	tempLen = com_cbufText.curSize;
	if (tempLen) {
		memcpy (temp, com_cbufText.data, tempLen);
		com_cbufText.Clear();
	}

	// Add the entire text of the file
	Cbuf_AddText (text);

	// Add the copied off data
	if (tempLen)
		com_cbufText.WriteRaw (temp, tempLen);
}


/*
============
Cbuf_CopyToDefer
============
*/
void Cbuf_CopyToDefer ()
{
	memcpy (com_cbufDeferTextBuf, com_cbufTextBuf, com_cbufText.curSize);
	com_cbufDeferTextBuf[com_cbufText.curSize] = 0;
	com_cbufText.curSize = 0;
}


/*
============
Cbuf_InsertFromDefer
============
*/
void Cbuf_InsertFromDefer ()
{
	Cbuf_InsertText ((char *)com_cbufDeferTextBuf);
	com_cbufDeferTextBuf[0] = 0;
}


/*
============
Cbuf_Execute
============
*/
void Cbuf_Execute ()
{
	char		*text;
	char		line[1024];
	int			i, quotes;

	com_aliasCount = 0;		// Don't allow infinite alias loops

	while (com_cbufText.curSize)
	{
		// Find a \n or ; line break
		text = (char *)com_cbufText.data;

		quotes = 0;
		for (i=0 ; i<com_cbufText.curSize ; i++)
		{
			if (text[i] == '"')
				quotes++;
			if (!(quotes&1) && text[i] == ';')
				break;	// Don't break if inside a quoted string
			if (text[i] == '\n')
				break;
		}

		if (i >= sizeof(line)-1)
		{
			Com_DevPrintf (PRNT_WARNING, "Cbuf_Execute: overflow of %d truncated\n", i);
			memcpy (line, text, sizeof(line)-1);
			line[sizeof(line)-1] = '\0';
		}
		else
		{
			memcpy (line, text, i);
			line[i] = 0;
		}

		/*
		** Delete the text from the command buffer and move remaining commands down
		** this is necessary because commands (exec, alias) can insert data at the
		** beginning of the text buffer
		*/
		if (i == com_cbufText.curSize)
		{
			com_cbufText.curSize = 0;
		}
		else
		{
			i++;
			com_cbufText.curSize -= i;

			if (com_cbufText.curSize)
				memmove (text, text+i, com_cbufText.curSize);
		}

		// Execute the command line
		Cmd_ExecuteString (line);

		if (com_cmdWait)
		{
			// Skip out while text still remains in buffer, leaving it for next frame
#ifndef DEDICATED_ONLY
			if (!dedicated->intVal)
				CL_ForcePacket ();
#endif
			com_cmdWait = false;
			break;
		}
	}
}

/*
=============================================================================

	INITIALIZATION

=============================================================================
*/

/*
============
Cbuf_Init
============
*/
void Cbuf_Init ()
{
	com_cbufText.Init(com_cbufTextBuf, sizeof(com_cbufTextBuf));
}
