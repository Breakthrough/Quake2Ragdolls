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
// cg_view.c
//

#include "cg_local.h"

/*
=======================================================================

	SCREEN SIZE

=======================================================================
*/

/*
==============
V_TileClear
==============
*/
static void V_TileRect (int x, int y, int w, int h)
{
	if (w == 0 || h == 0)
		return;	// Prevents div by zero (should never happen)

	if (cgMedia.tileBackMat)
		cgi.R_DrawPic (cgMedia.tileBackMat, 0,
		QuadVertices().SetVertices((float)x, (float)y, (float)w, (float)h).SetCoords(
		x/64.0f, y/64.0f, (x+w)/64.0f, (y+h)/64.0f), Q_BColorWhite);
	else
		cgi.R_DrawFill ((float)x, (float)y, (float)w, (float)h, Q_BColorBlack);
}
static void V_TileClear ()
{
	int		top, bottom, left, right;

	if (viewsize->intVal >= 100)
		return;		// Full screen rendering

	top = cg.refDef.y;
	bottom = top + cg.refDef.height-1;
	left = cg.refDef.x;
	right = left + cg.refDef.width-1;

	// Clear above view screen
	V_TileRect (0, 0, cg.refConfig.vidWidth, top);

	// Clear below view screen
	V_TileRect (0, bottom, cg.refConfig.vidWidth, cg.refConfig.vidHeight - bottom);

	// Clear left of view screen
	V_TileRect (0, top, left, bottom - top + 1);

	// Clear right of view screen
	V_TileRect (right, top, cg.refConfig.vidWidth - right, bottom - top + 1);
}


/*
=================
V_CalcVrect

Sets the coordinates of the rendered window
=================
*/
static void V_CalcVrect ()
{
	int		size;

	// Bound viewsize
	if (viewsize->intVal < 40)
		cgi.Cvar_SetValue ("viewsize", 40, true);
	if (viewsize->intVal > 100)
		cgi.Cvar_SetValue ("viewsize", 100, true);

	size = viewsize->intVal;

	cg.refDef.width = cg.refConfig.vidWidth*size/100;
	cg.refDef.width &= ~7;

	cg.refDef.height = cg.refConfig.vidHeight*size/100;
	cg.refDef.height &= ~1;

	cg.refDef.x = (cg.refConfig.vidWidth - cg.refDef.width)/2;
	cg.refDef.y = (cg.refConfig.vidHeight - cg.refDef.height)/2;

	// Clear the background
	V_TileClear ();
}


/*
=================
V_SizeUp_f
=================
*/
static void V_SizeUp_f ()
{
	cgi.Cvar_SetValue ("viewsize", viewsize->intVal + 10.0f, true);
}


/*
=================
V_SizeDown_f
=================
*/
static void V_SizeDown_f ()
{
	cgi.Cvar_SetValue ("viewsize", viewsize->intVal - 10.0f, true);
}

/*
=======================================================================

	VIEW RENDERING

=======================================================================
*/

/*
================
V_TestParticles
================
*/
static cgParticle_t v_testParticleList[PT_PICTOTAL];
static void V_TestParticles ()
{
	int				i;
	float			d, r, u;
	vec3_t			origin;
	float			scale;
	cgParticle_t	*p;

	cgi.R_ClearScene ();
	scale = 1;

	for (p=&v_testParticleList[0], i=0 ; i<PT_PICTOTAL ; i++, p++) {
		d = i*0.5f;
		r = 3*((i&7)-3.5f);
		u = 3*(((i>>3)&7)-3.5f);

		const int type = (cg.realTime / 1000) % PT_PICTOTAL ;//i;
		Com_Printf(0, "%i\n", type);

		// Center
		origin[0] = cg.refDef.viewOrigin[0] + cg.refDef.viewAxis[0][0]*d - cg.refDef.viewAxis[1][0]*r + cg.refDef.viewAxis[2][0]*u;
		origin[1] = cg.refDef.viewOrigin[1] + cg.refDef.viewAxis[0][1]*d - cg.refDef.viewAxis[1][1]*r + cg.refDef.viewAxis[2][1]*u;
		origin[2] = cg.refDef.viewOrigin[2] + cg.refDef.viewAxis[0][2]*d - cg.refDef.viewAxis[1][2]*r + cg.refDef.viewAxis[2][2]*u;

		// Top left
		v_testParticleList[i].outColor[0] = Q_BColorWhite;
		Vec2Set (v_testParticleList[i].outCoords[0], cgMedia.particleCoords[type][0], cgMedia.particleCoords[type][1]);
		Vec3Set (v_testParticleList[i].outVertices[0],	origin[0] + cg.refDef.viewAxis[2][0]*scale + cg.refDef.viewAxis[1][0]*scale,
														origin[1] + cg.refDef.viewAxis[2][1]*scale + cg.refDef.viewAxis[1][1]*scale,
														origin[2] + cg.refDef.viewAxis[2][2]*scale + cg.refDef.viewAxis[1][2]*scale);

		// Bottom left
		v_testParticleList[i].outColor[1] = Q_BColorWhite;
		Vec2Set (v_testParticleList[i].outCoords[1], cgMedia.particleCoords[type][0], cgMedia.particleCoords[type][3]);
		Vec3Set (v_testParticleList[i].outVertices[1],	origin[0] - cg.refDef.viewAxis[2][0]*scale + cg.refDef.viewAxis[1][0]*scale,
														origin[1] - cg.refDef.viewAxis[2][1]*scale + cg.refDef.viewAxis[1][1]*scale,
														origin[2] - cg.refDef.viewAxis[2][2]*scale + cg.refDef.viewAxis[1][2]*scale);

		// Bottom right
		v_testParticleList[i].outColor[2] = Q_BColorWhite;
		Vec2Set (v_testParticleList[i].outCoords[2], cgMedia.particleCoords[type][2], cgMedia.particleCoords[type][3]);
		Vec3Set (v_testParticleList[i].outVertices[2],	origin[0] - cg.refDef.viewAxis[2][0]*scale - cg.refDef.viewAxis[1][0]*scale,
														origin[1] - cg.refDef.viewAxis[2][1]*scale - cg.refDef.viewAxis[1][1]*scale,
														origin[2] - cg.refDef.viewAxis[2][2]*scale - cg.refDef.viewAxis[1][2]*scale);

		// Top right
		v_testParticleList[i].outColor[3] = Q_BColorWhite;
		Vec2Set (v_testParticleList[i].outCoords[3], cgMedia.particleCoords[type][2], cgMedia.particleCoords[type][1]);
		Vec3Set (v_testParticleList[i].outVertices[3],	origin[0] + cg.refDef.viewAxis[2][0]*scale - cg.refDef.viewAxis[1][0]*scale,
														origin[1] + cg.refDef.viewAxis[2][1]*scale - cg.refDef.viewAxis[1][1]*scale,
														origin[2] + cg.refDef.viewAxis[2][2]*scale - cg.refDef.viewAxis[1][2]*scale);

		// Render it
		p->outPoly.numVerts = 4;
		p->outPoly.colors = v_testParticleList[i].outColor;
		p->outPoly.texCoords = v_testParticleList[i].outCoords;
		p->outPoly.vertices = v_testParticleList[i].outVertices;
		p->outPoly.mat = cgMedia.particleTable[type];
		p->outPoly.matTime = 0;

		cgi.R_AddPoly (&p->outPoly);
	}
}


/*
================
V_TestEntities
================
*/
static void V_TestEntities ()
{
	int			i, j;
	float		f, r;
	refEntity_t	ent;

	cgi.R_ClearScene ();
	for (i=0 ; i<32 ; i++) {
		r = 64 * ((i%4) - 1.5f);
		f = 64 * (i/40.f) + 128;

		for (j=0 ; j<3 ; j++) {
			ent.origin[j] = cg.refDef.viewOrigin[j] + cg.refDef.viewAxis[0][j]*f - cg.refDef.viewAxis[1][j]*r;
			ent.oldOrigin[j] = ent.origin[j];
		}

		Matrix3_Identity (ent.axis);

		ent.model = cg.baseClientInfo.model;
		ent.skin = cg.baseClientInfo.skin;
		ent.skinNum = 0;

		ent.flags = 0;
		ent.scale = 1;
		Vec4Set (ent.color, 255, 255, 255, 255);

		ent.backLerp = 0;
		ent.frame = 0;
		ent.oldFrame = 0;

		cgi.R_AddEntity (&ent);
	}
}


/*
================
V_TestLights
================
*/
static void V_TestLights ()
{
	int			i;
	float		f, r;
	vec3_t		origin;
	float		red, green, blue;

	for (i=0 ; i<32 ; i++) {
		r = 64 * ((i%4) - 1.5f);
		f = 64 * (i/40.f) + 128;

		origin[0] = cg.refDef.viewOrigin[0] + cg.refDef.viewAxis[0][0]*f - cg.refDef.viewAxis[1][0]*r;
		origin[1] = cg.refDef.viewOrigin[1] + cg.refDef.viewAxis[0][1]*f - cg.refDef.viewAxis[1][1]*r;
		origin[2] = cg.refDef.viewOrigin[2] + cg.refDef.viewAxis[0][2]*f - cg.refDef.viewAxis[1][2]*r;
			
		red = (float)(((i%6)+1) & 1);
		green = (float)((((i%6)+1) & 2)>>1);
		blue = (float)((((i%6)+1) & 4)>>2);

		cgi.R_AddLight (origin, 200, red, green, blue);
	}
}

// ====================================================================

/*
===============
V_CalcThirdPersonView
===============
*/
static void ClipCam(vec3_t start, vec3_t end, vec3_t newPos)
{
	cmTrace_t	tr;
	vec3_t		mins, maxs;

	Vec3Set (mins, -5, -5, -5);
	Vec3Set (maxs, 5, 5, 5);

	CG_PMTrace (&tr, start, mins, maxs, end, true, true);

	newPos[0] = tr.endPos[0];
	newPos[1] = tr.endPos[1];
	newPos[2] = tr.endPos[2];
}
static void V_CalcThirdPersonView()
{
	vec3_t	end, camPosition;
	vec3_t	dir, newAngles;
	float	upDist, backDist, angle;

	// Set the camera angle
	if (cg_thirdPersonAngle->modified) {
		cg_thirdPersonAngle->modified = false;

		if (cg_thirdPersonAngle->floatVal < 0.0f)
			cgi.Cvar_SetValue ("cg_thirdPersonAngle", 0.0f, true);
	}

	// Set the camera distance
	if (cg_thirdPersonDist->modified) {
		cg_thirdPersonDist->modified = false;

		if (cg_thirdPersonDist->floatVal < 1.0f)
			cgi.Cvar_SetValue ("cg_thirdPersonDist", 1.0f, true);
	}

	// Trig stuff
	angle = M_PI * (cg_thirdPersonAngle->floatVal / 180.0f);
	Q_SinCosf(angle, &upDist, &backDist);
	upDist *= cg_thirdPersonDist->floatVal;
	backDist *= cg_thirdPersonDist->floatVal;

	// Move up
	Vec3MA (cg.refDef.viewOrigin, -backDist, cg.refDef.viewAxis[0], end);
	Vec3MA (end, upDist, cg.refDef.viewAxis[2], end);

	// Clip
	ClipCam (cg.refDef.viewOrigin, end, camPosition);

	// Adjust player transparency
	cg.cameraTrans = Vec3DistFast(cg.refDef.viewOrigin, camPosition);
	if (cg.cameraTrans < cg_thirdPersonDist->floatVal)
	{
		cg.cameraTrans = (cg.cameraTrans / cg_thirdPersonDist->floatVal) * 255;

		if (cg.cameraTrans == 0)
			return;
		else if (cg.cameraTrans > 245)
			cg.cameraTrans = 245;
	}
	else
	{
		cg.cameraTrans = 255;
	}

	// Clip and adjust aim
	// FIXME: Interpolate this? It's too snappy. Maybe adjust the camera better, so that it adjusts position as well as angle, so the change isn't so dramatic.
	if (cg_thirdPersonClip->intVal) {
		Vec3MA (cg.refDef.viewOrigin, 8192, cg.refDef.viewAxis[0], dir);
		ClipCam (cg.refDef.viewOrigin, dir, newAngles);

		Vec3Subtract (newAngles, camPosition, dir);
		VectorNormalizef (dir, dir);
		VecToAngles (dir, newAngles);

		// Apply
		Vec3Copy (newAngles, cg.refDef.viewAngles);
		Angles_Matrix3 (cg.refDef.viewAngles, cg.refDef.viewAxis);
	}

	Vec3Copy (camPosition, cg.refDef.viewOrigin);
}


/*
===============
V_CalcViewValues

Sets cg.refDef view values
===============
*/
static void V_CalcViewValues ()
{
	playerState_t	*ps, *ops;
	int					i;

	// Set cg.lerpFrac
	if (cg_timeDemo->intVal)
		cg.lerpFrac = 1.0f;
	else
		cg.lerpFrac = 1.0f - (cg.frame.serverTime - cg.refreshTime)*ServerFrameTimeInv;

	// Find the previous frame to interpolate from
	ps = &cg.frame.playerState;
	if (cg.oldFrame.serverFrame != cg.frame.serverFrame-1 || !cg.oldFrame.valid)
		ops = &cg.frame.playerState;	// previous frame was dropped or invalid
	else
		ops = &cg.oldFrame.playerState;

	// See if the player entity was teleported this frame
	if (fabsf (ops->pMove.origin[0] - ps->pMove.origin[0]) > 256*8
	|| abs (ops->pMove.origin[1] - ps->pMove.origin[1]) > 256*8
	|| abs (ops->pMove.origin[2] - ps->pMove.origin[2]) > 256*8)
		ops = ps;		// don't interpolate

	// Calculate the origin
	if (cl_predict->intVal && !(cg.frame.playerState.pMove.pmFlags & PMF_NO_PREDICTION)) {
		// Use predicted values
		uint32		delta;
		float		backLerp;

		backLerp = 1.0f - cg.lerpFrac;
		for (i=0 ; i<3 ; i++) {
			cg.refDef.viewOrigin[i] = cg.predicted.origin[i] + ops->viewOffset[i] 
								+ cg.lerpFrac*(ps->viewOffset[i] - ops->viewOffset[i])
								- backLerp*cg.predicted.error[i];
		}

		// Smooth out stair climbing
		delta = cg.refreshTime - cg.predicted.stepTime;
		if (delta < 150)
			cg.refDef.viewOrigin[2] -= cg.predicted.step * (150 - delta) / 150;
	}
	else {
		// Just use interpolated values
		for (i=0 ; i<3 ; i++) {
			cg.refDef.viewOrigin[i] = ops->pMove.origin[i]*(1.0f/8.0f) + ops->viewOffset[i] 
								+ cg.lerpFrac*(ps->pMove.origin[i]*(1.0f/8.0f) + ps->viewOffset[i] 
								- (ops->pMove.origin[i]*(1.0f/8.0f) + ops->viewOffset[i]));
		}
	}

	// If not running a demo or on a locked frame, add the local angle movement
	if (cg.frame.playerState.pMove.pmType < PMT_DEAD && !cg.attractLoop) {
		// Use predicted values
		Vec3Copy (cg.predicted.angles, cg.refDef.viewAngles);
	}
	else {
		// Just use interpolated values
		if (cg.frame.playerState.pMove.pmType >= PMT_DEAD && ops->pMove.pmType < PMT_DEAD) {
			cg.refDef.viewAngles[0] = LerpAngle (cg.predicted.angles[0], ps->viewAngles[0], cg.lerpFrac);
			cg.refDef.viewAngles[1] = LerpAngle (cg.predicted.angles[1], ps->viewAngles[1], cg.lerpFrac);
			cg.refDef.viewAngles[2] = LerpAngle (cg.predicted.angles[2], ps->viewAngles[2], cg.lerpFrac);
		}
		else {
			cg.refDef.viewAngles[0] = LerpAngle (ops->viewAngles[0], ps->viewAngles[0], cg.lerpFrac);
			cg.refDef.viewAngles[1] = LerpAngle (ops->viewAngles[1], ps->viewAngles[1], cg.lerpFrac);
			cg.refDef.viewAngles[2] = LerpAngle (ops->viewAngles[2], ps->viewAngles[2], cg.lerpFrac);
		}
	}

	// Add kick angles
	cg.refDef.viewAngles[0] += LerpAngle (ops->kickAngles[0], ps->kickAngles[0], cg.lerpFrac);
	cg.refDef.viewAngles[1] += LerpAngle (ops->kickAngles[1], ps->kickAngles[1], cg.lerpFrac);
	cg.refDef.viewAngles[2] += LerpAngle (ops->kickAngles[2], ps->kickAngles[2], cg.lerpFrac);

	// Calculate direction vectors
	Angles_Matrix3 (cg.refDef.viewAngles, cg.refDef.viewAxis);

	// Interpolate field of view
	cg.refDef.fovX = ops->fov + cg.lerpFrac * (ps->fov - ops->fov);

	// Don't interpolate blend color
	//Vec4Copy (ps->viewBlend, cg.viewBlend);

	// Offset if in third person
	if (cg.thirdPerson)
		V_CalcThirdPersonView ();
}

/*
================
CG_CalcViewBob

Uses the ground/swimming values set in CG_CalcViewOnGround for 
making more accurate decissions on bobbing state
================
*/
void CG_CalcViewBob()
{
	float bobMove, bobTime;

	if( cg.thirdPerson )
		return;

	//
	// calculate speed and cycle to be used for
	// all cyclic walking effects
	//
	cg.xyspeed = sqrtf( cg.predicted.velocity[0]*cg.predicted.velocity[0] + cg.predicted.velocity[1]*cg.predicted.velocity[1] );

	bobMove = 0;
	if( cg.xyspeed < 5 )
		cg.oldBobTime = 0;			// start at beginning of cycle again
	//else if( vweap.isSwim )
	//	bobMove = cg.refreshFrameTime * cg_bobSpeed->value * 0.3;
	else if( cg.frame.playerState.pMove.pmFlags & PMF_DUCKED )
		bobMove = cg.refreshFrameTime * cg_bobSpeed->floatVal * 0.6;
	else if( cg.isOnGround )
		bobMove = cg.refreshFrameTime * cg_bobSpeed->floatVal;

	bobTime = cg.oldBobTime += bobMove;

	cg.bobCycle = (int)bobTime;
	cg.bobFracSin = fabsf( sinf( bobTime*M_PI ) );
}

/*
==================
V_RenderView
==================
*/
#define FRAMETIME_MAX 0.5
void V_RenderView (int realTime, float netFrameTime, float refreshFrameTime, float stereoSeparation, bool refreshPrepped)
{
	// Check cvar sanity
	CG_UpdateCvars ();

	// Calculate screen dimensions and clear the background
	V_CalcVrect ();

	// Set time
	cg.realTime = realTime;

	cg.netFrameTime = netFrameTime;
	cg.netTime += netFrameTime * 1000.0f;
	cg.refreshFrameTime = refreshFrameTime;
	cg.refreshTime += refreshFrameTime * 1000.0f;

	// Clamp time
	cg.netTime = clamp(cg.netTime, cg.frame.serverTime - ServerFrameTime, cg.frame.serverTime);
	if (cg.netFrameTime > FRAMETIME_MAX)
		cg.netFrameTime = FRAMETIME_MAX;

	cg.refreshTime = clamp(cg.refreshTime, cg.frame.serverTime - ServerFrameTime, cg.frame.serverTime);
	if (cg.refreshFrameTime > FRAMETIME_MAX)
		cg.refreshFrameTime = FRAMETIME_MAX;

	// Only update time if we're not rendering the scene
	if (!cg.forceRefDef && (cg.mapLoading || !cg.frame.valid || cgi.Com_ClientState () != CA_ACTIVE || !refreshPrepped))
	{
		// Render the menu
		switch (cgi.Com_ClientState ())
		{
		case CA_DISCONNECTED:
			if (cgi.Key_GetDest () == KD_GAME)
				CG_DrawConnectScreen ();
			UI_Refresh (true);
			break;

		case CA_CONNECTING:
		case CA_CONNECTED:
		case CA_ACTIVE:
			CG_DrawConnectScreen ();
			UI_Refresh (false);
			break;
		}

		return;
	}

	// Predict all unacknowledged movements
	CG_PredictMovement ();

	// Watch for gender bending if desired
	CG_FixUpGender ();

	// Run light styles
	CG_RunLightStyles ();

	// An invalid frame will just use the exact previous refdef
	// We can't use the old frame if the video mode has changed, though
	if (cg.frame.valid && (cg.forceRefDef || !cg_paused->intVal))
	{
		cg.forceRefDef = false;

		cgi.R_ClearScene ();

		// Calculate the view values
		V_CalcViewValues ();

		// Add in entities and effects
		CG_AddEntities ();

		// View bob for next frame
		CG_CalcViewBob ();

		// Testing testing...
		if (cl_testblend->intVal) {
			cg.viewBlend[0] = 1.0f;
			cg.viewBlend[1] = 0.5f;
			cg.viewBlend[2] = 0.25f;
			cg.viewBlend[3] = 0.5f;
		}
		if (cl_testentities->intVal)
			V_TestEntities ();
		if (cl_testlights->intVal)
			V_TestLights ();
		if (cl_testparticles->intVal)
			V_TestParticles ();

		// Offset the viewOrigin if we're using stereo separation
		if (stereoSeparation)
			Vec3MA (cg.refDef.viewOrigin, -stereoSeparation, cg.refDef.viewAxis[1], cg.refDef.viewOrigin);

		// Never let it sit exactly on a node line, because a water plane
		// can dissapear when viewed with the eye exactly on it. the server
		// protocol only specifies to 1/8 pixel, so add 1/16 in each axis
		cg.refDef.viewOrigin[0] += (1.0f/16.0f);
		cg.refDef.viewOrigin[1] += (1.0f/16.0f);
		cg.refDef.viewOrigin[2] += (1.0f/16.0f);

		cg.refDef.velocity[0]	= cg.predicted.velocity[0];
		cg.refDef.velocity[1]	= cg.predicted.velocity[1];
		cg.refDef.velocity[2]	= cg.predicted.velocity[2];

		cg.refDef.fovY			= Q_CalcFovY (cg.refDef.fovX, (float)cg.refDef.width, (float)cg.refDef.height);

		cg.refDef.time			= cg.refreshTime * 0.001f;

		cg.refDef.areaBits = cg.frame.areaBits;
		cg.refDef.rdFlags = cg.frame.playerState.rdFlags | RDF_FBO;

		if (cg.bOldAreaBits)
			cg.refDef.rdFlags |= RDF_OLDAREABITS;
		else
			cg.bOldAreaBits = true;
	}

	// Render the frame
	cgi.R_RenderScene (&cg.refDef);

	// Update orientation for sound subsystem
	cgi.Snd_Update (&cg.refDef);

	// Run subsystems
	CG_RunDLights ();

	// Render screen stuff
	SCR_Draw ();

	// Increment frame counter
	cg.frameCount++;
}

/*
=======================================================================

	CONSOLE FUNCTIONS

=======================================================================
*/

/*
=============
V_Viewpos_f
=============
*/
static void V_Viewpos_f ()
{
	Com_Printf (0, "(x%i y%i z%i) : yaw%i\n",
				(int)cg.refDef.viewOrigin[0],
				(int)cg.refDef.viewOrigin[1],
				(int)cg.refDef.viewOrigin[2], 
				(int)cg.refDef.viewAngles[YAW]);
}


/*
================
V_Benchmark_f
================
*/
static void V_Benchmark_f ()
{
	int			i, j, times;
	int			start;
	float		time, timeinc;
	float		result;
	refDef_t	refDef;

	memset (&refDef, 0, sizeof(refDef_t));

	refDef.width = cg.refConfig.vidWidth;
	refDef.height = cg.refConfig.vidHeight;
	refDef.fovX = 90;
	refDef.fovY = Q_CalcFovY (refDef.fovX, refDef.width, refDef.height);
	Vec3MA (cg.frame.playerState.viewOffset, (1.0f/8.0f), cg.frame.playerState.pMove.origin, refDef.viewOrigin);

	if (cgi.Cmd_Argc () >= 2)
		times = atoi (cgi.Cmd_Argv (1));
	else
		times = 10;

	for (j=0, result=0, timeinc=0 ; j<times ; j++) {
		start = cgi.Sys_Milliseconds ();

		if (cgi.Cmd_Argc () >= 3) {
			// Run without page flipping
			cgi.R_BeginFrame (0);
			for (i=0 ; i<128 ; i++) {
				refDef.viewAngles[1] = i/128.0*360.0;
				Angles_Matrix3 (refDef.viewAngles, refDef.viewAxis);

				cgi.R_RenderScene (&refDef);
			}
			cgi.R_EndFrame ();
		}
		else {
			for (i=0 ; i<128 ; i++) {
				refDef.viewAngles[1] = i/128.0*360.0;
				Angles_Matrix3 (refDef.viewAngles, refDef.viewAxis);

				cgi.R_BeginFrame (0);
				cgi.R_RenderScene (&refDef);
				cgi.R_EndFrame ();
			}
		}

		time = (cgi.Sys_Milliseconds () - start) / 1000.0;
		timeinc += time;
		result += 128.0 / time;
	}

	Com_Printf (0, "%f secs, %f fps total\n", timeinc, result/times);
}


/*
================
V_TimeRefresh_f
================
*/
static void V_TimeRefresh_f ()
{
	int			i, start;
	float		time;
	refDef_t	refDef;

	start = cgi.Sys_Milliseconds ();

	memset (&refDef, 0, sizeof(refDef_t));

	refDef.width = cg.refConfig.vidWidth;
	refDef.height = cg.refConfig.vidHeight;
	refDef.fovX = 90;
	refDef.fovY = Q_CalcFovY (refDef.fovX, refDef.width, refDef.height);
	Vec3MA (cg.frame.playerState.viewOffset, (1.0f/8.0f), cg.frame.playerState.pMove.origin, refDef.viewOrigin);

	if (cgi.Cmd_Argc () == 2) {
		// Run without page flipping
		cgi.R_BeginFrame (0);
		for (i=0 ; i<128 ; i++) {
			refDef.viewAngles[1] = i/128.0*360.0;
			Angles_Matrix3 (refDef.viewAngles, refDef.viewAxis);

			cgi.R_RenderScene (&refDef);
		}
		cgi.R_EndFrame ();
	}
	else {
		for (i=0 ; i<128 ; i++) {
			refDef.viewAngles[1] = i/128.0*360.0;
			Angles_Matrix3 (refDef.viewAngles, refDef.viewAxis);

			cgi.R_BeginFrame (0);
			cgi.R_RenderScene (&refDef);
			cgi.R_EndFrame ();
		}
	}

	time = (cgi.Sys_Milliseconds () - start) / 1000.0;
	Com_Printf (0, "%f seconds (%f fps)\n", time, 128/time);
}

/*
=======================================================================

	INIT / SHUTDOWN

=======================================================================
*/

static conCmd_t	*cmd_sizeUp;
static conCmd_t	*cmd_sizeDown;
static conCmd_t	*cmd_viewPos;
static conCmd_t	*cmd_benchMark;
static conCmd_t	*cmd_timeRefresh;

/*
=============
V_Register
=============
*/
void V_Register ()
{
	cmd_sizeUp		= cgi.Cmd_AddCommand("sizeup",			0, V_SizeUp_f,			"Increases viewport size");
	cmd_sizeDown	= cgi.Cmd_AddCommand("sizedown",		0, V_SizeDown_f,		"Decreases viewport size");

	cmd_viewPos		= cgi.Cmd_AddCommand("viewpos",			0, V_Viewpos_f,		"Prints view position and yaw");

	cmd_benchMark	= cgi.Cmd_AddCommand("benchmark",		0, V_Benchmark_f,		"Multiple speed tests for one scene");
	cmd_timeRefresh	= cgi.Cmd_AddCommand("timerefresh",		0, V_TimeRefresh_f,	"Prints framerate of current scene");
}


/*
=============
V_Unregister
=============
*/
void V_Unregister ()
{
	cgi.Cmd_RemoveCommand(cmd_sizeUp);
	cgi.Cmd_RemoveCommand(cmd_sizeDown);

	cgi.Cmd_RemoveCommand(cmd_viewPos);

	cgi.Cmd_RemoveCommand(cmd_benchMark);
	cgi.Cmd_RemoveCommand(cmd_timeRefresh);
}

// Viewblend
void CG_VB_AddBlend (const colorf &color)
{
	if (color.A <= 0)
		return;
	var a2 = cg.viewBlend[3] + (1-cg.viewBlend[3])*color.A;	// new total alpha
	var a3 = cg.viewBlend[3]/a2;		// fraction of color from old

	cg.viewBlend[0] = cg.viewBlend[0]*a3 + color.R*(1-a3);
	cg.viewBlend[1] = cg.viewBlend[1]*a3 + color.G*(1-a3);
	cg.viewBlend[2] = cg.viewBlend[2]*a3 + color.B*(1-a3);
	cg.viewBlend[3] = a2;
}

static colorf lavaContents (1.0f, 0.3f, 0.0f, 0.6f);
static colorf slimeContents (0.0f, 0.1f, 0.05f, 0.6f);
static colorf waterContents (0.5f, 0.3f, 0.2f, 0.4f);

void CG_VB_DrawViewBlends ()
{
	cg.viewBlend = colorf(0, 0, 0, 0);

	// add for contents
	var contents = CG_PMPointContents(cg.refDef.viewOrigin);
	//if (contents & (CONTENTS_LAVA|CONTENTS_SLIME|CONTENTS_WATER) )
	//	ent->client->ps.rdFlags |= RDF_UNDERWATER;
	//else
	//	ent->client->ps.rdFlags &= ~RDF_UNDERWATER;

	if (contents & (CONTENTS_SOLID|CONTENTS_LAVA))
		CG_VB_AddBlend (lavaContents);
	else if (contents & CONTENTS_SLIME)
		CG_VB_AddBlend (slimeContents);
	else if (contents & CONTENTS_WATER)
		CG_VB_AddBlend (waterContents);

	// add for powerups
	/*if (ent->client->quad_framenum > level.framenum)
	{
		remaining = ent->client->quad_framenum - level.framenum;
		if (remaining == 30)	// beginning to fade
			gi.sound(ent, CHAN_ITEM, gi.soundindex("items/damage2.wav"), 1, ATTN_NORM, 0);
		if (remaining > 30 || (remaining & 4) )
			SV_AddBlend (0, 0, 1, 0.08f, ent->client->ps.viewBlend);
	}
	else if (ent->client->invincible_framenum > level.framenum)
	{
		remaining = ent->client->invincible_framenum - level.framenum;
		if (remaining == 30)	// beginning to fade
			gi.sound(ent, CHAN_ITEM, gi.soundindex("items/protect2.wav"), 1, ATTN_NORM, 0);
		if (remaining > 30 || (remaining & 4) )
			SV_AddBlend (1, 1, 0, 0.08f, ent->client->ps.viewBlend);
	}
	else if (ent->client->enviro_framenum > level.framenum)
	{
		remaining = ent->client->enviro_framenum - level.framenum;
		if (remaining == 30)	// beginning to fade
			gi.sound(ent, CHAN_ITEM, gi.soundindex("items/airout.wav"), 1, ATTN_NORM, 0);
		if (remaining > 30 || (remaining & 4) )
			SV_AddBlend (0, 1, 0, 0.08f, ent->client->ps.viewBlend);
	}
	else if (ent->client->breather_framenum > level.framenum)
	{
		remaining = ent->client->breather_framenum - level.framenum;
		if (remaining == 30)	// beginning to fade
			gi.sound(ent, CHAN_ITEM, gi.soundindex("items/airout.wav"), 1, ATTN_NORM, 0);
		if (remaining > 30 || (remaining & 4) )
			SV_AddBlend (0.4f, 1, 0.4f, 0.04f, ent->client->ps.viewBlend);
	}*/

	if (cg.frame.playerState.viewBlend[3] != 0)
	{
		cg.damageBlend[0] = cg.frame.playerState.viewBlend[0];
		cg.damageBlend[1] = cg.frame.playerState.viewBlend[1];
		cg.damageBlend[2] = cg.frame.playerState.viewBlend[2];
		cg.damageAlpha += cg.frame.playerState.viewBlend[3];

		if (cg.damageAlpha > 0.6f)
			cg.damageAlpha = 0.6f;

		Vec4Set(cg.frame.playerState.viewBlend, 0, 0, 0, 0);
	}

	if (cg.damageAlpha > 0)
		CG_VB_AddBlend (colorf(cg.damageBlend[0], cg.damageBlend[1], cg.damageBlend[2], cg.damageAlpha));

	if (cg.bonusAlpha > 0)
		CG_VB_AddBlend (colorf(0.85f, 0.7f, 0.3f, cg.bonusAlpha));

	// drop the damage value
	cg.damageAlpha -= cg.refreshFrameTime;
	if (cg.damageAlpha < 0)
		cg.damageAlpha = 0;

	// drop the bonus value
	cg.bonusAlpha -= cg.refreshFrameTime;
	if (cg.bonusAlpha < 0)
		cg.bonusAlpha = 0;

	if (gl_polyblend->intVal && cg.viewBlend[3] > 0.01f)
		cgi.R_DrawFill (0, 0, cg.refDef.width, cg.refDef.height, colorb(cg.viewBlend));
}
