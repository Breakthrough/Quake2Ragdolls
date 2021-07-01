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
// cl_input.c
// Builds an intended movement command to send to the server
//

#include "cl_local.h"

cVar_t	*cl_nodelta;

cVar_t	*cl_upspeed;
cVar_t	*cl_forwardspeed;
cVar_t	*cl_sidespeed;

cVar_t	*cl_yawspeed;
cVar_t	*cl_pitchspeed;

static cVar_t	*autosensitivity;
static cVar_t	*cl_anglespeedkey;
static cVar_t	*cl_run;
static cVar_t	*m_filter;

static uint32	in_frameTime;
static uint32	in_lastFrameTime;
static uint32	in_frameMSec;

static ivec2_t	in_mouseMove;
static ivec2_t	in_lastMouseMove;
static bool	in_mLooking;

/*
============
IN_CenterView_f
============
*/
static void IN_CenterView_f ()
{
	cl.viewAngles[PITCH] = -SHORT2ANGLE (cl.frame.playerState.pMove.deltaAngles[PITCH]);
}

/*
=============================================================================

	KEY BUTTONS

	Continuous button event tracking is complicated by the fact that two
	different input sources (say, mouse button 1 and the control key) can both
	press the same button, but the button should only be released when both of
	the pressing key have been released.

	When a key event issues a button command (+forward, +attack, etc), it
	appends its key number as a parameter to the command so it can be matched
	up with the release.

	state bit 0 is the current state of the key
	state bit 1 is edge triggered on the up to down transition
	state bit 2 is edge triggered on the down to up transition

=============================================================================
*/

struct kButton_t {
	int			down[2];		// key nums holding it down
	uint32		downTime;		// msec timestamp
	uint32		msec;			// msec down this frame
	int			state;
};

static kButton_t		btn_moveUp;
static kButton_t		btn_moveDown;
static kButton_t		btn_lookLeft;
static kButton_t		btn_lookRight;
static kButton_t		btn_moveForward;
static kButton_t		btn_moveBack;
static kButton_t		btn_lookUp;
static kButton_t		btn_lookDown;
static kButton_t		btn_moveLeft;
static kButton_t		btn_moveRight;

static kButton_t		btn_speed;
static kButton_t		btn_strafe;

static kButton_t		btn_attack;
static kButton_t		btn_use;

static kButton_t		btn_keyLook;

static int				btn_impulse;

/*
====================
CL_KeyDown
====================
*/
static void CL_KeyDown (kButton_t *b)
{
	int		k;
	char	*c;
	
	c = Cmd_Argv (1);
	if (c[0])
		k = atoi (c);
	else
		k = -1;	// Typed manually at the console for continuous down

	if (k == b->down[0] || k == b->down[1])
		return;	// Repeating key
	
	if (!b->down[0])
		b->down[0] = k;
	else if (!b->down[1])
		b->down[1] = k;
	else {
		Com_Printf (PRNT_WARNING, "Three keys down for a button!\n");
		return;
	}
	
	if (b->state & 1)
		return;	// still down

	// Save timestamp
	c = Cmd_Argv (2);
	b->downTime = atoi(c);
	if (!b->downTime)
		b->downTime = in_frameTime - ServerFrameTime;

	b->state |= 1 + 2;	// down + impulse down
}


/*
====================
CL_KeyUp
====================
*/
static void CL_KeyUp (kButton_t *b)
{
	int		k;
	char	*c;
	uint32	uptime;

	c = Cmd_Argv (1);
	if (c[0])
		k = atoi(c);
	else {
		// Typed manually at the console, assume for unsticking, so clear all
		b->down[0] = b->down[1] = 0;
		b->state = 4;	// Impulse up
		return;
	}

	if (b->down[0] == k)
		b->down[0] = 0;
	else if (b->down[1] == k)
		b->down[1] = 0;
	else
		return;		// Key up without coresponding down (menu pass through)
	if (b->down[0] || b->down[1])
		return;		// Some other key is still holding it down

	if (!(b->state & 1))
		return;		// Still up (this should not happen)

	// Save timestamp
	c = Cmd_Argv (2);
	uptime = atoi(c);
	if (uptime)
		b->msec += uptime - b->downTime;
	else
		b->msec += 10;

	b->state &= ~1;		// Now up
	b->state |= 4;		// Impulse up
}


/*
===============
CL_KeyState

Returns the fraction of the frame that the key was down
===============
*/
static float CL_KeyState (kButton_t *key)
{
	int			msec;

	key->state &= 1;		// clear impulses

	msec = key->msec;
	key->msec = 0;

	if (key->state) {
		// Still down
		msec += in_frameTime - key->downTime;
		key->downTime = in_frameTime;
	}

	return clamp ((float)msec / (float)in_frameMSec, 0, 1);
}


static void IN_UpDown_f ()			{ CL_KeyDown (&btn_moveUp); }
static void IN_UpUp_f ()			{ CL_KeyUp (&btn_moveUp); }
static void IN_DownDown_f ()		{ CL_KeyDown (&btn_moveDown); }
static void IN_DownUp_f ()			{ CL_KeyUp (&btn_moveDown); }
static void IN_LeftDown_f ()		{ CL_KeyDown (&btn_lookLeft); }
static void IN_LeftUp_f ()			{ CL_KeyUp (&btn_lookLeft); }
static void IN_RightDown_f ()		{ CL_KeyDown (&btn_lookRight); }
static void IN_RightUp_f ()			{ CL_KeyUp (&btn_lookRight); }
static void IN_ForwardDown_f ()		{ CL_KeyDown (&btn_moveForward); }
static void IN_ForwardUp_f ()		{ CL_KeyUp (&btn_moveForward); }
static void IN_BackDown_f ()		{ CL_KeyDown (&btn_moveBack); }
static void IN_BackUp_f ()			{ CL_KeyUp (&btn_moveBack); }
static void IN_LookupDown_f ()		{ CL_KeyDown (&btn_lookUp); }
static void IN_LookupUp_f ()		{ CL_KeyUp (&btn_lookUp); }
static void IN_LookdownDown_f ()	{ CL_KeyDown (&btn_lookDown); }
static void IN_LookdownUp_f ()		{ CL_KeyUp (&btn_lookDown); }

// FIXME: Treat like the other keys here?
// Only like this because it was done like this in win32 input...
static void IN_MLookDown_f ()
{
	in_mLooking = true;
}
static void IN_MLookUp_f ()
{
	in_mLooking = false;
	if (!freelook->intVal && lookspring->intVal)
		IN_CenterView_f ();
}

static void IN_MoveleftDown_f ()	{ CL_KeyDown (&btn_moveLeft); }
static void IN_MoveleftUp_f ()		{ CL_KeyUp (&btn_moveLeft); }
static void IN_MoverightDown_f ()	{ CL_KeyDown (&btn_moveRight); }
static void IN_MoverightUp_f ()		{ CL_KeyUp (&btn_moveRight); }

static void IN_SpeedDown_f ()		{ CL_KeyDown (&btn_speed); }
static void IN_SpeedUp_f ()			{ CL_KeyUp (&btn_speed); }
static void IN_StrafeDown_f ()		{ CL_KeyDown (&btn_strafe); }
static void IN_StrafeUp_f ()		{ CL_KeyUp (&btn_strafe); }

static void IN_AttackDown_f ()		{ CL_KeyDown (&btn_attack); }
static void IN_AttackUp_f ()		{ CL_KeyUp (&btn_attack); }

static void IN_UseDown_f ()			{ CL_KeyDown (&btn_use); }
static void IN_UseUp_f ()			{ CL_KeyUp (&btn_use); }

static void IN_Impulse_f ()			{ btn_impulse=atoi (Cmd_Argv (1)); }

static void IN_KLookDown_f ()		{ CL_KeyDown (&btn_keyLook); }
static void IN_KLookUp_f ()			{ CL_KeyUp (&btn_keyLook); }

//==========================================================================

/*
================
CL_GetRunState

For Win32 joystick input.
================
*/
bool CL_GetRunState ()
{
	return ((btn_speed.state & 1) ^ cl_run->intVal) ? true : false;
}


/*
================
CL_GetStrafeState

For Win32 joystick input.
================
*/
bool CL_GetStrafeState ()
{
	return (btn_strafe.state & 1);
}


/*
================
CL_GetMLookState

For Win32 joystick input.
================
*/
bool CL_GetMLookState ()
{
	return in_mLooking;
}

/*
=============================================================================

	MOUSE MOVEMENT

	This takes mouse moves from the operating system and queues up the moves
	to later factor into the transmitted move command.

=============================================================================
*/

/*
================
CL_MoveMouse

Queue cursor movement.
================
*/
void CL_MoveMouse (int xMove, int yMove)
{
	// Update GUI
	if (Key_GetDest () == KD_MENU) {
		CL_CGModule_MoveMouse (xMove, yMove);
		GUI_MoveMouse (xMove, yMove);

		Vec2Clear (in_mouseMove);
		Vec2Clear (in_lastMouseMove);
		return;
	}

	// Queue movement
	in_mouseMove[0] += xMove;
	in_mouseMove[1] += yMove;
}

/*
=============================================================================

	MOVE USER COMMAND

=============================================================================
*/

/*
================
CL_BaseMove

Send the intended movement message to the server
================
*/
static void CL_BaseMove (userCmd_t *cmd)
{
	float	turnSpeed;
	float	moveSpeed;

	// Adjust for turning speed
	if (btn_speed.state & 1)
		turnSpeed = cls.netFrameTime * cl_anglespeedkey->floatVal;
	else
		turnSpeed = cls.netFrameTime;

	// Adjust for running speed
	if (CL_GetRunState ())
		moveSpeed = 2;
	else
		moveSpeed = 1;

	// Handle left/right on keyboard
	cl.cmdNum = cls.netChan.outgoingSequence & CMD_MASK;
	cmd = &cl.cmds[cl.cmdNum];
	cl.cmdTime[cl.cmdNum] = cls.realTime;	// for netgraph ping calculation

	if (CL_GetStrafeState ()) {
		// Keyboard strafe
		cmd->sideMove += cl_sidespeed->floatVal * CL_KeyState (&btn_lookRight);
		cmd->sideMove -= cl_sidespeed->floatVal * CL_KeyState (&btn_lookLeft);
	}
	else {
		// Keyboard turn
		cl.viewAngles[YAW] -= turnSpeed * cl_yawspeed->floatVal * CL_KeyState (&btn_lookRight);
		cl.viewAngles[YAW] += turnSpeed * cl_yawspeed->floatVal * CL_KeyState (&btn_lookLeft);
	}

	if (btn_keyLook.state & 1) {
		// Keyboard look
		cl.viewAngles[PITCH] -= turnSpeed * cl_pitchspeed->floatVal * CL_KeyState (&btn_moveForward);
		cl.viewAngles[PITCH] += turnSpeed * cl_pitchspeed->floatVal * CL_KeyState (&btn_moveBack);
	}
	else {
		// Keyboard move front/back
		cmd->forwardMove += cl_forwardspeed->floatVal * CL_KeyState (&btn_moveForward);
		cmd->forwardMove -= cl_forwardspeed->floatVal * CL_KeyState (&btn_moveBack);
	}

	// Keyboard look up/down
	cl.viewAngles[PITCH] -= turnSpeed * cl_pitchspeed->floatVal * CL_KeyState (&btn_lookUp);
	cl.viewAngles[PITCH] += turnSpeed * cl_pitchspeed->floatVal * CL_KeyState (&btn_lookDown);

	// Keyboard strafe left/right
	cmd->sideMove += cl_sidespeed->floatVal * CL_KeyState (&btn_moveRight);
	cmd->sideMove -= cl_sidespeed->floatVal * CL_KeyState (&btn_moveLeft);

	// Keyboard jump/crouch
	cmd->upMove += cl_upspeed->floatVal * CL_KeyState (&btn_moveUp);
	cmd->upMove -= cl_upspeed->floatVal * CL_KeyState (&btn_moveDown);

	// Cap to max allowed ranges
	if (cmd->forwardMove > cl_forwardspeed->floatVal * moveSpeed)
		cmd->forwardMove = cl_forwardspeed->floatVal * moveSpeed;
	else if (cmd->forwardMove < -cl_forwardspeed->floatVal * moveSpeed)
		cmd->forwardMove = -cl_forwardspeed->floatVal * moveSpeed;

	if (cmd->sideMove > cl_sidespeed->floatVal * moveSpeed)
		cmd->sideMove = cl_sidespeed->floatVal * moveSpeed;
	else if (cmd->sideMove < -cl_sidespeed->floatVal * moveSpeed)
		cmd->sideMove = -cl_sidespeed->floatVal * moveSpeed;

	if (cmd->upMove > cl_upspeed->floatVal * moveSpeed)
		cmd->upMove = cl_upspeed->floatVal * moveSpeed;
	else if (cmd->upMove < -cl_upspeed->floatVal * moveSpeed)
		cmd->upMove = -cl_upspeed->floatVal * moveSpeed;
}


/*
================
CL_MouseMove

Add mouse X/Y movement to cmd
================
*/
static void CL_MouseMove (userCmd_t *cmd)
{
	ivec2_t			move;

	// Movement filtering
	if (m_filter->intVal) {
		move[0] = (in_mouseMove[0] + in_lastMouseMove[0]) * 0.5f;
		move[1] = (in_mouseMove[1] + in_lastMouseMove[1]) * 0.5f;
	}
	else {
		move[0] = in_mouseMove[0];
		move[1] = in_mouseMove[1];
	}
	in_lastMouseMove[0] = in_mouseMove[0];
	in_lastMouseMove[1] = in_mouseMove[1];

	// Zooming in preserves sensitivity
	if (autosensitivity->intVal) {
		move[0] *= sensitivity->floatVal * (cl.refDef.fovX/90.0f);
		move[1] *= sensitivity->floatVal * (cl.refDef.fovY/90.0f);
	}
	else {
		move[0] *= sensitivity->floatVal;
		move[1] *= sensitivity->floatVal;
	}

	// Side/yaw movement
	if (CL_GetStrafeState () || (lookstrafe->intVal && in_mLooking))
		cmd->sideMove += m_side->floatVal * move[0];
	else
		cl.viewAngles[YAW] -= m_yaw->floatVal * move[0];

	// Forward/pitch movement
	if (!CL_GetStrafeState () && (freelook->intVal || in_mLooking))
		cl.viewAngles[PITCH] += m_pitch->floatVal * move[1];
	else
		cmd->forwardMove -= m_forward->floatVal * move[1];

	// Clear
	Vec2Clear (in_mouseMove);
}


/*
==============
CL_ClampPitch
==============
*/
static void CL_ClampPitch ()
{
	float	pitch;

	pitch = SHORT2ANGLE (cl.frame.playerState.pMove.deltaAngles[PITCH]);
	if (pitch > 180)
		pitch -= 360;

	if (cl.viewAngles[PITCH] + pitch < -360)
		cl.viewAngles[PITCH] += 360; // wrapped
	if (cl.viewAngles[PITCH] + pitch > 360)
		cl.viewAngles[PITCH] -= 360; // wrapped

	if (cl.viewAngles[PITCH] + pitch > 89)
		cl.viewAngles[PITCH] = 89 - pitch;
	if (cl.viewAngles[PITCH] + pitch < -89)
		cl.viewAngles[PITCH] = -89 - pitch;
}


/*
=================
CL_RefreshCmd
=================
*/
void CL_RefreshCmd ()
{
	int			ms;
	userCmd_t	*cmd = &cl.cmds[cls.netChan.outgoingSequence & CMD_MASK];

	// Get delta for this sample.
	in_frameMSec = in_frameTime - in_lastFrameTime;
	if (in_frameMSec < 1)
		return;
	else if (in_frameMSec > 20)
		in_frameMSec = 20; // FIXME: clamp to the net framerate?

	// Get basic movement from keyboard
	CL_BaseMove (cmd);

	// Allow mice or other external controllers to add to the move
	IN_Move (cmd);

	// Add mouse movement
	CL_MouseMove (cmd);

	// Update cmd viewangles for CL_PredictMove
	CL_ClampPitch ();

	// Transmit data
	cmd->angles[0] = ANGLE2SHORT(cl.viewAngles[0]);
	cmd->angles[1] = ANGLE2SHORT(cl.viewAngles[1]);
	cmd->angles[2] = ANGLE2SHORT(cl.viewAngles[2]);

	// Update cmd->msec for CL_PredictMove
	ms = (int)(cls.netFrameTime * 1000);
	if (ms > 250)
		ms = 100;
	cmd->msec = ms;

	// Update counter
	in_lastFrameTime = in_frameTime;

	// Send packet immediately on important events
	if (btn_attack.state & 2 || btn_use.state & 2)
		cls.forcePacket = true;
}


/*
=================
CL_FinalizeCmd
=================
*/
static void CL_FinalizeCmd ()
{
	userCmd_t *cmd = &cl.cmds[cls.netChan.outgoingSequence & CMD_MASK];

	// Set any button hits that occured since last frame
	if (btn_attack.state & 3)
		cmd->buttons |= BUTTON_ATTACK;
	btn_attack.state &= ~2;

	if (btn_use.state & 3)
		cmd->buttons |= BUTTON_USE;
	btn_use.state &= ~2;

	if (key_anyKeyDown && Key_GetDest () == KD_GAME)
		cmd->buttons |= BUTTON_ANY;

	// ...
	cmd->impulse = btn_impulse;
	btn_impulse = 0;

	// Set the ambient light level at the player's current position
	cmd->lightLevel = (byte)cl_lightlevel->floatVal;
}


/*
=================
CL_SendCmd
=================
*/
void CL_SendCmd ()
{
	byte		data[128];
	netMsg_t	buf;
	userCmd_t	*cmd, *oldCmd;
	userCmd_t	nullCmd;
	int			checkSumIndex;

	switch (Com_ClientState ()) {
	case CA_CONNECTED:
		if (cls.netChan.gotReliable || cls.netChan.message.curSize || Sys_Milliseconds()-cls.netChan.lastSent > ServerFrameTime)
			Netchan_Transmit (cls.netChan, 0, NULL);
		return;

	case CA_DISCONNECTED:
	case CA_CONNECTING:
		// Wait until active
		return;
	}

	cl.cmdNum = cls.netChan.outgoingSequence & CMD_MASK;
	cmd = &cl.cmds[cl.cmdNum];
	cl.cmdTime[cl.cmdNum] = cls.realTime;	// for ping calculation

	CL_FinalizeCmd ();

	cl.cmd = *cmd;

	// Send a userinfo update if needed
	if (com_userInfoModified) {
		com_userInfoModified = false;
		cls.netChan.message.WriteByte (CLC_USERINFO);
		cls.netChan.message.WriteString (Cvar_BitInfo (CVAR_USERINFO));
	}

	buf.Init(data, sizeof(data));

	if (cmd->buttons && cl.cin.time > 0 && !cl.attractLoop && cls.realTime-cl.cin.time > 1000) {
		// Skip the rest of the cinematic
		CIN_FinishCinematic ();
	}

	// Begin a client move command
	buf.WriteByte (CLC_MOVE);

	// Save the position for a checksum byte
	if (cls.serverProtocol != ENHANCED_PROTOCOL_VERSION) {
		checkSumIndex = buf.curSize;
		buf.WriteByte (0);
	}
	else
		checkSumIndex = 0;

	/*
	** Let the server know what the last frame we got was,
	** so the next message can be delta compressed
	*/
	if (cl_nodelta->intVal || !cl.frame.valid || cls.demoWaiting)
		buf.WriteLong (-1);	// no compression
	else
		buf.WriteLong (cl.frame.serverFrame);

	/*
	** Send this and the previous cmds in the message,
	** so if the last packet was dropped, it can be recovered
	*/
	cmd = &cl.cmds[(cls.netChan.outgoingSequence-2)&CMD_MASK];
	memset (&nullCmd, 0, sizeof(nullCmd));
	buf.WriteDeltaUsercmd (&nullCmd, cmd, cls.protocolMinorVersion);
	oldCmd = cmd;

	cmd = &cl.cmds[(cls.netChan.outgoingSequence-1)&CMD_MASK];
	buf.WriteDeltaUsercmd (oldCmd, cmd, cls.protocolMinorVersion);
	oldCmd = cmd;

	cmd = &cl.cmds[(cls.netChan.outgoingSequence)&CMD_MASK];
	buf.WriteDeltaUsercmd (oldCmd, cmd, cls.protocolMinorVersion);

	// Calculate a checksum over the move commands
	if (cls.serverProtocol != ENHANCED_PROTOCOL_VERSION) {
		buf.data[checkSumIndex] = Com_BlockSequenceCRCByte (
			buf.data + checkSumIndex + 1, buf.curSize - checkSumIndex - 1,
			cls.netChan.outgoingSequence);
	}

	// Deliver the message
	Netchan_Transmit (cls.netChan, buf.curSize, buf.data);

	// Init the current cmd buffer and clear it
	cmd = &cl.cmds[cls.netChan.outgoingSequence&CMD_MASK];
	memset (cmd, 0, sizeof(userCmd_t));
}


/*
============
CL_UpdateFrameTime
============
*/
void CL_UpdateFrameTime (uint32 time)
{
	in_frameTime = time;
}


/*
============
CL_InputInit
============
*/
void CL_InputInit ()
{
	// Cvars
	autosensitivity		= Cvar_Register ("autosensitivity",		"0",		CVAR_ARCHIVE);

	cl_nodelta			= Cvar_Register ("cl_nodelta",			"0",		0);

	cl_upspeed			= Cvar_Register ("cl_upspeed",			"200",		0);
	cl_forwardspeed		= Cvar_Register ("cl_forwardspeed",		"200",		0);
	cl_sidespeed		= Cvar_Register ("cl_sidespeed",		"200",		0);
	cl_yawspeed			= Cvar_Register ("cl_yawspeed",			"140",		0);
	cl_pitchspeed		= Cvar_Register ("cl_pitchspeed",		"150",		0);

	cl_run				= Cvar_Register ("cl_run",				"0",		CVAR_ARCHIVE);

	cl_anglespeedkey	= Cvar_Register ("cl_anglespeedkey",	"1.5",		0);

	m_filter			= Cvar_Register ("m_filter",			"0",		0);

	// Commands
	Cmd_AddCommand ("centerview",	0, IN_CenterView_f,	"Centers the view");

	Cmd_AddCommand ("+moveup",		0, IN_UpDown_f,			"");
	Cmd_AddCommand ("-moveup",		0, IN_UpUp_f,			"");
	Cmd_AddCommand ("+movedown",	0, IN_DownDown_f,		"");
	Cmd_AddCommand ("-movedown",	0, IN_DownUp_f,			"");
	Cmd_AddCommand ("+left",		0, IN_LeftDown_f,		"");
	Cmd_AddCommand ("-left",		0, IN_LeftUp_f,			"");
	Cmd_AddCommand ("+right",		0, IN_RightDown_f,		"");
	Cmd_AddCommand ("-right",		0, IN_RightUp_f,		"");
	Cmd_AddCommand ("+forward",		0, IN_ForwardDown_f,	"");
	Cmd_AddCommand ("-forward",		0, IN_ForwardUp_f,		"");
	Cmd_AddCommand ("+back",		0, IN_BackDown_f,		"");
	Cmd_AddCommand ("-back",		0, IN_BackUp_f,			"");
	Cmd_AddCommand ("+lookup",		0, IN_LookupDown_f,		"");
	Cmd_AddCommand ("-lookup",		0, IN_LookupUp_f,		"");
	Cmd_AddCommand ("+lookdown",	0, IN_LookdownDown_f,	"");
	Cmd_AddCommand ("-lookdown",	0, IN_LookdownUp_f,		"");
	Cmd_AddCommand ("+strafe",		0, IN_StrafeDown_f,		"");
	Cmd_AddCommand ("-strafe",		0, IN_StrafeUp_f,		"");
	Cmd_AddCommand ("+mlook",		0, IN_MLookDown_f,		"");
	Cmd_AddCommand ("-mlook",		0, IN_MLookUp_f,		"");
	Cmd_AddCommand ("+moveleft",	0, IN_MoveleftDown_f,	"");
	Cmd_AddCommand ("-moveleft",	0, IN_MoveleftUp_f,		"");
	Cmd_AddCommand ("+moveright",	0, IN_MoverightDown_f,	"");
	Cmd_AddCommand ("-moveright",	0, IN_MoverightUp_f,	"");
	Cmd_AddCommand ("+speed",		0, IN_SpeedDown_f,		"");
	Cmd_AddCommand ("-speed",		0, IN_SpeedUp_f,		"");
	Cmd_AddCommand ("+attack",		0, IN_AttackDown_f,		"");
	Cmd_AddCommand ("-attack",		0, IN_AttackUp_f,		"");
	Cmd_AddCommand ("+use",			0, IN_UseDown_f,		"");
	Cmd_AddCommand ("-use",			0, IN_UseUp_f,			"");

	Cmd_AddCommand ("impulse",		0, IN_Impulse_f,		"");

	Cmd_AddCommand ("+klook",		0, IN_KLookDown_f,		"");
	Cmd_AddCommand ("-klook",		0, IN_KLookUp_f,		"");
}
