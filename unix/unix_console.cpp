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
// unix_console.c
//

#include "../common/common.h"
#include "unix_local.h"
#include <unistd.h>
#include <fcntl.h>

qBool	stdin_active = qTrue;

static cVar_t *con_convertcolors = NULL;
static char    ttyc;

/*
==================
Sys_InitConsole
==================
*/
void Sys_InitConsole (void)
{
  fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY); /* make stdin nonblocking */
  fcntl (1, F_SETFL, fcntl (1, F_GETFL, 0) | FNDELAY); /* make stdout nonblocking */

  ttyc = isatty(1); /* determine if stdout is a tty for color conversion */

  if (!con_convertcolors)
    con_convertcolors = Cvar_Register("con_convertcolors", "1", CVAR_ARCHIVE);
    /* 0 = don't process colors, 1 = convert only if on a tty, 2 = always convert */
}


/*
==================
Sys_ShowConsole
==================
*/
void Sys_ShowConsole (int visLevel, qBool quitOnClose)
{
}


/*
==================
Sys_ConsoleInput
==================
*/

char *Sys_ConsoleInput (void)
{
  static char text[4096];
  static char tret[4096];
  static int  start = 0;
  static char err = 0;
  static int  len = 0;
  fd_set  fdset;
  struct timeval timeout;

  if (!stdin_active)
    return NULL;

  if (len <= 0) {
    FD_ZERO (&fdset);
    FD_SET (0, &fdset); // stdin
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    if (select (1, &fdset, NULL, NULL, &timeout) == -1 || !FD_ISSET(0, &fdset))
      return NULL;

    len = read (0, text+start, sizeof (text)-start);

    if (len < 1) {
      if (len == 0) // eof
        stdin_active = qFalse;
      len = 0;
      return NULL;
    }
  }

  while (len--) {
    if (text[start] == 10 || text[start] == 13) {
      text[start] = 0;
      strcpy(tret, text);
      memmove(text, text+start+1, len);
      start = 0;
      if (err) {
        err = 0;
        return NULL;
      }
      return tret[0]?tret:NULL;
    }
    if (++start == sizeof(text)) {
      err = 1;
      start = 0;
    }
  }

  return NULL;
}

/*
================
Sys_ConsoleOutput
================
*/
void Sys_ConsoleOutput (const char *string)
{
  static qBool colorleft = qFalse;
  qBool        convert = con_convertcolors && (con_convertcolors->intVal == 1 && ttyc || con_convertcolors->intVal > 1);
  char buf[65];
  char c = 0;
  char e;

  buf[65] = 0;
  while (*string) {
    if (convert && *string == '^') {
      if (c > 56) {
        buf[c] = 0;
        c = 0;
        printf("%s", buf);
      }
      buf[c++] = 27;
      buf[c++] = '[';
      c++;
      buf[c++] = ';';
      buf[c++] = '3';
      c++;
      buf[c++] = 'm';
      e = *++string;
      if (e >= '0' && e < '8') {
        buf[c-5] = '1';
        if (e == '5') e = '6';
        else
        if (e == '6') e = '5';
        buf[c-2] = e;
        colorleft = qTrue;
      } else
      if (e == '8') {
        buf[c-5] = '0';
        buf[c-2] = '7';
        colorleft = qTrue;
      } else
      if (e == 'r' || e == 'R') {
        buf[c-5] = '0';
        c -= 3;
        buf[c++] = 'm';
        colorleft = qFalse;
     } else
       c -= 7; /* ignore all other */
    } else {
      if (*string == 10 && colorleft) {
        colorleft = qFalse;
        if (c > 60) {
          buf[c] = 0;
          c = 0;
          printf("%s", buf);
        }
        buf[c++] = 27;
        buf[c++] = '[';
        buf[c++] = '0';
        buf[c++] = 'm';
      }
      buf[c++] = *string & 127;
    }
    string++;
    if (c == 65) {
      c = 0;
      printf("%s", buf);
    }
  }
  if (c) {
    buf[c] = 0;
    printf("%s", buf);
  }
  fflush(stdout);
}


/*
==================
Conbuf_AppendText
==================
*/
void Conbuf_AppendText (const char *pMsg)
{
  Sys_ConsoleOutput (pMsg);
}


/*
==================
Sys_SetErrorText
==================
*/
void Sys_SetErrorText (const char *buf)
{
  Conbuf_AppendText ("********************\n"
                     "SYS ERROR: ");
  Conbuf_AppendText (buf);
  Conbuf_AppendText ("\n"
                     "********************\n");
  fprintf(stderr, "********************\nSYS ERROR: %s\n********************\n", buf);
}
