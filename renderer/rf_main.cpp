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
// r_main.c
//

#include "rf_local.h"

refInfo_t	ri;

/*
=============================================================================

	SCENE

=============================================================================
*/

/*
==================
R_DrawStats
==================
*/
extern bool CL_ConsoleOpen(); // FIXME: Maybe give the console a higher z depth?
void R_DrawStats()
{
	if (!CL_ConsoleOpen() && (r_speeds->intVal || r_times->intVal || r_debugCulling->intVal))
	{
		// If this is the first time we've drawn this, let's flush the old state counter values
		if (r_speeds->modified || r_times->modified || r_debugCulling->modified)
		{
			r_speeds->modified = false;
			r_times->modified = false;
			r_debugCulling->modified = false;
			memset (&ri.pc, 0, sizeof(refStats_t));
		}

		vec2_t CharSize;
		vec2_t Position;
		colorb BGColors[2];
		colorb TitleBGColor;
		int Color;

		R_GetFontDimensions(NULL, 0, 0, FS_SHADOW, CharSize);

		Position[0] = CharSize[0];
		Position[1] = CharSize[1] * 2;

		BGColors[0] = Q_BColorWhite;
		BGColors[0].A = 63;

		BGColors[1] = Q_BColorLtGrey;
		BGColors[1].A = 63;

		TitleBGColor = Q_BColorMdGrey;
		TitleBGColor.A = 63;

		Color = 0;

		if (r_speeds->intVal)
		{
			// Scene
			R_DrawPic(ri.media.whiteMaterial, 0, QuadVertices().SetVertices(Position[0], Position[1], CharSize[0]*64, CharSize[1]), TitleBGColor);
			R_DrawString(NULL, Position[0], Position[1], 0, 0, FS_SHADOW, "Scene:", Q_BColorGreen);

			Position[1] += CharSize[1];
			R_DrawPic(ri.media.whiteMaterial, 0, QuadVertices().SetVertices(Position[0], Position[1], CharSize[0]*64, CharSize[1]), BGColors[(Color++)&1]);
			R_DrawString(NULL, Position[0], Position[1], 0, 0, FS_SHADOW,
				Q_VarArgs("Entities: %4u   DLights: %2u", ri.scn.numEntities, ri.scn.numDLights),
				Q_BColorWhite);

			Position[1] += CharSize[1];
			R_DrawPic(ri.media.whiteMaterial, 0, QuadVertices().SetVertices(Position[0], Position[1], CharSize[0]*64, CharSize[1]), BGColors[(Color++)&1]);
			R_DrawString(NULL, Position[0], Position[1], 0, 0, FS_SHADOW,
				Q_VarArgs("AliasElems: %4u AliasPolys: %5u", ri.pc.aliasElements, ri.pc.aliasPolys),
				Q_BColorWhite);

			Position[1] += CharSize[1];
			R_DrawPic(ri.media.whiteMaterial, 0, QuadVertices().SetVertices(Position[0], Position[1], CharSize[0]*64, CharSize[1]), BGColors[(Color++)&1]);
			R_DrawString(NULL, Position[0], Position[1], 0, 0, FS_SHADOW,
				Q_VarArgs("PolyEnt: %5u   PolyEntElems: %5u PolyEntPolys: %6u", ri.scn.polyList.Count(), ri.pc.polyElements, ri.pc.polyPolys),
				Q_BColorWhite);

			// World
			if (ri.scn.worldModel != ri.scn.defaultModel  && !(ri.def.rdFlags & RDF_NOWORLDMODEL))
			{
				Position[1] += CharSize[1] * 2;
				R_DrawPic(ri.media.whiteMaterial, 0, QuadVertices().SetVertices(Position[0], Position[1], CharSize[0]*64, CharSize[1]), TitleBGColor);
				R_DrawString(NULL, Position[0], Position[1], 0, 0, FS_SHADOW, "World:", Q_BColorGreen);

				Position[1] += CharSize[1];
				R_DrawPic(ri.media.whiteMaterial, 0, QuadVertices().SetVertices(Position[0], Position[1], CharSize[0]*64, CharSize[1]), BGColors[(Color++)&1]);
				R_DrawString(NULL, Position[0], Position[1], 0, 0, FS_SHADOW,
					Q_VarArgs("WPoly: %5u  WElem: %5u      ZFar: %6.f",
						ri.pc.worldPolys, ri.pc.worldElements, ri.scn.zFar),
					Q_BColorWhite);

				Position[1] += CharSize[1];
				R_DrawPic(ri.media.whiteMaterial, 0, QuadVertices().SetVertices(Position[0], Position[1], CharSize[0]*64, CharSize[1]), BGColors[(Color++)&1]);
				R_DrawString(NULL, Position[0], Position[1], 0, 0, FS_SHADOW,
					Q_VarArgs("Decals: %5u DecalElems: %5u DecalPolys: %6u",
						ri.pc.decalsPushed, ri.pc.decalElements, ri.pc.decalPolys),
					Q_BColorWhite);
			}

			// Texture
			Position[1] += CharSize[1] * 2;
			R_DrawPic(ri.media.whiteMaterial, 0, QuadVertices().SetVertices(Position[0], Position[1], CharSize[0]*64, CharSize[1]), TitleBGColor);
			R_DrawString(NULL, Position[0], Position[1], 0, 0, FS_SHADOW, "Texture:", Q_BColorGreen);

			Position[1] += CharSize[1];
			R_DrawPic(ri.media.whiteMaterial, 0, QuadVertices().SetVertices(Position[0], Position[1], CharSize[0]*64, CharSize[1]), BGColors[(Color++)&1]);
			R_DrawString(NULL, Position[0], Position[1], 0, 0, FS_SHADOW,
				Q_VarArgs("MegaTexels: %4.2f (%3u unique textures)", ri.pc.texelsInUse/1000000.0f, ri.pc.texturesInUse),
				Q_BColorWhite);

			Position[1] += CharSize[1];
			R_DrawPic(ri.media.whiteMaterial, 0, QuadVertices().SetVertices(Position[0], Position[1], CharSize[0]*64, CharSize[1]), BGColors[(Color++)&1]);
			R_DrawString(NULL, Position[0], Position[1], 0, 0, FS_SHADOW,
				Q_VarArgs("UnitChanges: %3u TargetChanges: %3u Binds: %4u",
					ri.pc.textureUnitChanges, ri.pc.textureTargetChanges, ri.pc.textureBinds),
				Q_BColorWhite);

			Position[1] += CharSize[1];
			R_DrawPic(ri.media.whiteMaterial, 0, QuadVertices().SetVertices(Position[0], Position[1], CharSize[0]*64, CharSize[1]), BGColors[(Color++)&1]);
			R_DrawString(NULL, Position[0], Position[1], 0, 0, FS_SHADOW,
				Q_VarArgs("EnvChanges: %3u  GenChanges: %3u    MatChanges: %3u",
					ri.pc.textureEnvChanges, ri.pc.textureGenChanges, ri.pc.textureMatChanges),
				Q_BColorWhite);

			// Backend
			Position[1] += CharSize[1] * 2;
			R_DrawPic(ri.media.whiteMaterial, 0, QuadVertices().SetVertices(Position[0], Position[1], CharSize[0]*64, CharSize[1]), TitleBGColor);
			R_DrawString(NULL, Position[0], Position[1], 0, 0, FS_SHADOW, "Backend:", Q_BColorGreen);

			Position[1] += CharSize[1];
			R_DrawPic(ri.media.whiteMaterial, 0, QuadVertices().SetVertices(Position[0], Position[1], CharSize[0]*64, CharSize[1]), BGColors[(Color++)&1]);
			R_DrawString(NULL, Position[0], Position[1], 0, 0, FS_SHADOW,
				Q_VarArgs("Verts: %5u Polys: %5u", ri.pc.numVerts, ri.pc.numTris),
				Q_BColorWhite);

			Position[1] += CharSize[1];
			R_DrawPic(ri.media.whiteMaterial, 0, QuadVertices().SetVertices(Position[0], Position[1], CharSize[0]*64, CharSize[1]), BGColors[(Color++)&1]);
			R_DrawString(NULL, Position[0], Position[1], 0, 0, FS_SHADOW,
				Q_VarArgs("MeshBuffers: %4u MatPasses: %4u DrawElem: %4u", ri.pc.meshCount, ri.pc.meshPasses, ri.pc.numElements),
				Q_BColorWhite);

			Position[1] += CharSize[1];
			R_DrawPic(ri.media.whiteMaterial, 0, QuadVertices().SetVertices(Position[0], Position[1], CharSize[0]*64, CharSize[1]), BGColors[(Color++)&1]);
			R_DrawString(NULL, Position[0], Position[1], 0, 0, FS_SHADOW,
				Q_VarArgs("StateChanges: %3u", ri.pc.stateChanges),
				Q_BColorWhite);

			float batchEfficiency;
			if (ri.pc.meshBatcherPushes > 0)
			{
				batchEfficiency = 100.0f - ((float)ri.pc.meshBatcherFlushes/(float)ri.pc.meshBatcherPushes) * 100.0f;
			}
			else
			{
				batchEfficiency = 100.0f;
			}

			Position[1] += CharSize[1];
			R_DrawPic(ri.media.whiteMaterial, 0, QuadVertices().SetVertices(Position[0], Position[1], CharSize[0]*64, CharSize[1]), BGColors[(Color++)&1]);
			R_DrawString(NULL, Position[0], Position[1], 0, 0, FS_SHADOW,
				Q_VarArgs("BatchPushed: %4i BatchFlushed: %4i (%3.f%% Efficiency)",
					ri.pc.meshBatcherPushes, ri.pc.meshBatcherFlushes, batchEfficiency),
				Q_BColorWhite);

			Position[1] += CharSize[1] * 2;
		}

		// Time to process things
		if (r_times->intVal)
		{
			R_DrawPic(ri.media.whiteMaterial, 0, QuadVertices().SetVertices(Position[0], Position[1], CharSize[0]*64, CharSize[1]), TitleBGColor);
			R_DrawString(NULL, Position[0], Position[1], 0, 0, FS_SHADOW, "Backend Times:", Q_BColorGreen);

			Position[1] += CharSize[1];
			R_DrawPic(ri.media.whiteMaterial, 0, QuadVertices().SetVertices(Position[0], Position[1], CharSize[0]*64, CharSize[1]), BGColors[(Color++)&1]);
			R_DrawString(NULL, Position[0], Position[1], 0, 0, FS_SHADOW,
				Q_VarArgs("AddToList: %4.2fms    SortList: %4.2fms",
					ri.pc.timeAddToList * Sys_MSPerCycle(),
					ri.pc.timeSortList * Sys_MSPerCycle()),
				Q_BColorWhite);

			Position[1] += CharSize[1];
			R_DrawPic(ri.media.whiteMaterial, 0, QuadVertices().SetVertices(Position[0], Position[1], CharSize[0]*64, CharSize[1]), BGColors[(Color++)&1]);
			R_DrawString(NULL, Position[0], Position[1], 0, 0, FS_SHADOW,
				Q_VarArgs("PushMesh:  %4.2fms    DrawList: %4.2fms",
					ri.pc.timePushMesh * Sys_MSPerCycle(),
					ri.pc.timeDrawList * Sys_MSPerCycle()),
				Q_BColorWhite);

			if (ri.scn.worldModel != ri.scn.defaultModel && !(ri.def.rdFlags & RDF_NOWORLDMODEL))
			{
				Position[1] += CharSize[1] * 2;
				R_DrawPic(ri.media.whiteMaterial, 0, QuadVertices().SetVertices(Position[0], Position[1], CharSize[0]*64, CharSize[1]), TitleBGColor);
				R_DrawString(NULL, Position[0], Position[1], 0, 0, FS_SHADOW, "World Times:", Q_BColorGreen);						 
																																	 
				Position[1] += CharSize[1];																							 
				R_DrawPic(ri.media.whiteMaterial, 0, QuadVertices().SetVertices( Position[0], Position[1], CharSize[0]*64, CharSize[1]), BGColors[(Color++)&1]);
				R_DrawString(NULL, Position[0], Position[1], 0, 0, FS_SHADOW,
					Q_VarArgs("MarkLeaves: %4.2fms MarkLights: %4.2fms",
						ri.pc.timeMarkLeaves * Sys_MSPerCycle(),
						ri.pc.timeMarkLights * Sys_MSPerCycle()),
					Q_BColorWhite);

				Position[1] += CharSize[1];
				R_DrawPic(ri.media.whiteMaterial, 0, QuadVertices().SetVertices(Position[0], Position[1], CharSize[0]*64, CharSize[1]), BGColors[(Color++)&1]);
				R_DrawString(NULL, Position[0], Position[1], 0, 0, FS_SHADOW,
					Q_VarArgs("Recursion:  %4.2fms ShadowRecursion: %4.2fms",
					ri.pc.timeRecurseWorld * Sys_MSPerCycle(),
					ri.pc.timeShadowRecurseWorld * Sys_MSPerCycle()),
					Q_BColorWhite);
			}

			Position[1] += CharSize[1] * 2;
		}

		// Cull information
		if (r_debugCulling->intVal)
		{
			R_DrawPic(ri.media.whiteMaterial, 0, QuadVertices().SetVertices(Position[0], Position[1], CharSize[0]*64, CharSize[1]), TitleBGColor);
			R_DrawString(NULL, Position[0], Position[1], 0, 0, FS_SHADOW, "Frustum Culling (Pass/Fail):", Q_BColorGreen);

			Position[1] += CharSize[1];
			R_DrawPic(ri.media.whiteMaterial, 0, QuadVertices().SetVertices(Position[0], Position[1], CharSize[0]*64, CharSize[1]), BGColors[(Color++)&1]);
			R_DrawString(NULL, Position[0], Position[1], 0, 0, FS_SHADOW,
				Q_VarArgs("Bounds: %4u/%4u      Radial: %4u/%4u",
					ri.pc.cullBounds[CULL_PASS], ri.pc.cullBounds[CULL_FAIL],
					ri.pc.cullRadius[CULL_PASS], ri.pc.cullRadius[CULL_FAIL]),
				Q_BColorWhite);

			Position[1] += CharSize[1];
			R_DrawPic(ri.media.whiteMaterial, 0, QuadVertices().SetVertices(Position[0], Position[1], CharSize[0]*64, CharSize[1]), BGColors[(Color++)&1]);
			R_DrawString(NULL, Position[0], Position[1], 0, 0, FS_SHADOW,
				Q_VarArgs("Alias: %4u/%4u       DLights: %4u/%4u",
					ri.pc.cullAlias[CULL_PASS], ri.pc.cullAlias[CULL_FAIL],
					ri.pc.cullDLight[CULL_PASS], ri.pc.cullDLight[CULL_FAIL]),
				Q_BColorWhite);

			if (ri.scn.worldModel != ri.scn.defaultModel && !(ri.def.rdFlags & RDF_NOWORLDMODEL))
			{
				Position[1] += CharSize[1] * 2;
				R_DrawPic(ri.media.whiteMaterial, 0, QuadVertices().SetVertices(Position[0], Position[1], CharSize[0]*64, CharSize[1]), TitleBGColor);
				R_DrawString(NULL, Position[0], Position[1], 0, 0, FS_SHADOW, "World Culling:", Q_BColorGreen);

				Position[1] += CharSize[1];
				R_DrawPic(ri.media.whiteMaterial, 0, QuadVertices().SetVertices(Position[0], Position[1], CharSize[0]*64, CharSize[1]), BGColors[(Color++)&1]);
				R_DrawString(NULL, Position[0], Position[1], 0, 0, FS_SHADOW,
					Q_VarArgs("Planar:    %4u/%4u",
						ri.pc.cullPlanar[CULL_PASS], ri.pc.cullPlanar[CULL_FAIL]),
					Q_BColorWhite);

				Position[1] += CharSize[1];
				R_DrawPic(ri.media.whiteMaterial, 0, QuadVertices().SetVertices(Position[0], Position[1], CharSize[0]*64, CharSize[1]), BGColors[(Color++)&1]);
				R_DrawString(NULL, Position[0], Position[1], 0, 0, FS_SHADOW,
					Q_VarArgs("VisFrame:  %4u/%4u   SurfFrame: %4u/%4u",
						ri.pc.cullVis[CULL_PASS], ri.pc.cullVis[CULL_FAIL],
						ri.pc.cullSurf[CULL_PASS], ri.pc.cullSurf[CULL_FAIL]),
					Q_BColorWhite);
			}
		}

		memset (&ri.pc, 0, sizeof(refStats_t));
	}
}


/*
==================
R_UpdateCvars

Updates scene based on cvar changes
==================
*/
static void R_UpdateCvars()
{
	// Draw buffer stuff
	if (gl_drawbuffer->modified)
	{
		gl_drawbuffer->modified = false;
		if (!ri.cameraSeparation || !ri.config.stereoEnabled)
		{
			if (!Q_stricmp(gl_drawbuffer->string, "GL_FRONT"))
				glDrawBuffer(GL_FRONT);
			else
				glDrawBuffer(GL_BACK);
		}
	}

	// Texturemode stuff
	if (gl_texturemode->modified)
		GL_TextureMode(false, false);

	// Update anisotropy
	if (r_ext_maxAnisotropy->modified)
		GL_ResetAnisotropy();

	// Update font
	if (r_defaultFont->modified)
		R_CheckFont();
	if (r_fontScale->modified)
	{
		r_fontScale->modified = false;
		if (r_fontScale->floatVal <= 0)
			Cvar_VariableSetValue(r_fontScale, 1, true);
	}

	// Gamma ramp
	if (ri.config.bHWGammaInUse && vid_gamma->modified)
		R_UpdateGammaRamp();

	// Clamp zFar
	if (r_zFarAbs->modified)
	{
		r_zFarAbs->modified = false;
		if (r_zFarAbs->floatVal < 0.0f)
			Cvar_VariableSetValue(r_zFarAbs, 0, true);
	}
	if (r_zFarMin->modified)
	{
		r_zFarMin->modified = false;
		if (r_zFarMin->intVal <= 0)
			Cvar_VariableSetValue(r_zFarMin, 1, true);
	}

	// Clamp zNear
	if (r_zNear->modified)
	{
		r_zNear->modified = false;
		if (r_zNear->floatVal < 0.1f)
			Cvar_VariableSetValue(r_zNear, 4, true);
	}
}

#include "btBulletDynamicsCommon.h"

class glDebugDraw :public btIDebugDraw
{
public:
	int m_debugMode;
	glDebugDraw()
		:m_debugMode(0)
	{

	}

	void    drawLine(const btVector3& from,const btVector3& to,const btVector3& color)
	{
		//      if (m_debugMode > 0)
		{
			float tmp[ 6 ] = { from.getX(), from.getY(), from.getZ(),
				to.getX(), to.getY(), to.getZ() };

			glPushMatrix();
			{         
				glColor4f(color.getX(), color.getY(), color.getZ(), 1.0f);         
				glVertexPointer( 3,
					GL_FLOAT,
					0,
					&tmp );

				glPointSize( 5.0f );
				glDrawArrays( GL_POINTS, 0, 2 );
				glDrawArrays( GL_LINES, 0, 2 );
			}
			glPopMatrix();      
		}
	}

	void    setDebugMode(int debugMode)
	{
		m_debugMode = debugMode;
	}

	int      getDebugMode() const { return m_debugMode;}

	void    draw3dText(const btVector3& location,const char* textString)
	{
		//glRasterPos3f(location.x(),  location.y(),  location.z());
		//BMF_DrawString(BMF_GetFont(BMF_kHelvetica10),textString);
	}

	void    reportErrorWarning(const char* warningString)
	{
		printf(warningString);
	}

	void    drawContactPoint(const btVector3& pointOnB,const btVector3& normalOnB,btScalar distance,int lifeTime,const btVector3& color)
	{
		{
			//btVector3 to=pointOnB+normalOnB*distance;
			//const btVector3&from = pointOnB;
			//glColor4f(color.getX(), color.getY(), color.getZ(), 1.0f);   

			//GLDebugDrawer::drawLine(from, to, color);

			//glRasterPos3f(from.x(),  from.y(),  from.z());
			//char buf[12];
			//sprintf(buf," %d",lifeTime);
			//BMF_DrawString(BMF_GetFont(BMF_kHelvetica10),buf);
		}
	}

};

glDebugDraw drawer;

/*
================
R_RenderRefDef

Adds scene items to the desired list
================
*/
void RB_StateForBits(stateBit_t newBits);
static void R_RenderRefDef(refDef_t *rd)
{
	ri.def = *rd;

	// Clear mesh list
	ri.scn.currentList = &ri.scn.meshLists[ri.scn.viewType];
	ri.scn.currentList->Clear();

	// Add items to the list
	{
		qStatCycle_Scope Stat(r_times, ri.pc.timeAddToList);

		// Clear scene visibility dimensions
		ClearBounds(ri.scn.visMins, ri.scn.visMaxs);

		R_UpdateSkyMesh();

		// Setup view matrixes and calculate zFar
		// zFar is set using visMins/Maxs, which is set when the world is being added to the list
		RB_SetupViewMatrixes();

		// Now that zFar is set, setup the frustum
		R_SetupFrustum();

		// Add world meshes, while building occluders

		R_AddWorldToList();
		R_AddBModelsToList();

		// Cache culling results for multiple renders
		R_CullEntityList();
		RB_CullShadowMaps();
		R_CullDynamicLightList();

		// Add scene items
		if (ri.scn.viewType != RVT_SHADOWMAP)
		{
			RB_RenderShadowMaps();
			R_AddDecalsToList();
			R_AddPolysToList();
		}
		R_AddEntitiesToList();
	}

	// Sort the list
	{
		qStatCycle_Scope Stat(r_times, ri.pc.timeSortList);
		ri.scn.currentList->SortList();
	}

	// Setup state for rendering
	RB_SetupGL3D();

	if (!(rd->rdFlags & RDF_NOCLEAR))
		RB_ClearBuffers(~0);

	// Render
	{
		qStatCycle_Scope Stat(r_times, ri.pc.timeDrawList);
		glPushMatrix();
		ri.scn.currentList->DrawList();
		glPopMatrix();
	}

	ri.scn.currentList->DrawOutlines();

	var world = (btDiscreteDynamicsWorld*)SV_GetPhysicsWorld();
	if (world && r_debugServerPhysics->intVal)
	{
		glLineWidth(2);
		glColor4ub(255, 255, 255, 255);
		glBindTexture(GL_TEXTURE_2D, 0);
		drawer.setDebugMode(btIDebugDraw::DBG_DrawWireframe | btIDebugDraw::DBG_DrawConstraints | btIDebugDraw::DBG_DrawConstraintLimits);
		world->setDebugDrawer(&drawer);
		world->debugDrawWorld();
		glLineWidth(1);
	}

	RB_DrawDLights();

	RB_EndGL3D();
}


/*
================
R_RenderScene
================
*/
void R_RenderScene(refDef_t *rd)
{
	if (r_noRefresh->intVal)
		return;

	if (ri.scn.worldModel == ri.scn.defaultModel && !(rd->rdFlags & RDF_NOWORLDMODEL))
		Com_Error(ERR_DROP, "R_RenderScene: NULL worldmodel");

	if (gl_finish->intVal)
		glFinish();

	// Clear shadows
	RB_ClearShadowMaps();

	// Categorize entities
	R_CategorizeEntityList();

	// Render the world scene
	ri.scn.viewType = RVT_NORMAL;
	R_RenderRefDef(rd);

	R_BloomBlend(rd);

	// Calculate light level
	R_SetLightLevel();

	// Prepare for any 2D rendering
	RB_SetupGL2D();
}


/*
==================
R_BeginFrame
==================
*/
void R_BeginFrame(const float cameraSeparation)
{
	ri.cameraSeparation = cameraSeparation;

	// Apply cvar settings
	R_UpdateCvars();

	// Update the backend
	RB_BeginFrame();
}


/*
==================
R_EndFrame
==================
*/
void R_EndFrame()
{
	// Update the backend
	RB_EndFrame();

	// Next frame
	ri.frameCount++;
}

// ==========================================================

bool r_bInTimeDemo;

static uint32 r_timeDemoFrame;
static double r_timeDemoMS;
static uint32 r_timeDemoLastCycles;

/*
====================
R_BeginTimeDemo
====================
*/
void R_BeginTimeDemo()
{
	// Make sure these are off so that they don't flush performance counters
	Cvar_VariableSetValue(r_speeds, 0, true);
	Cvar_VariableSetValue(r_times, 0, true);
	Cvar_VariableSetValue(r_debugCulling, 0, true);

	// Reset our storage variables
	r_bInTimeDemo = true;
	r_timeDemoFrame = 0;
	r_timeDemoMS = 0.0;
	r_timeDemoLastCycles = Sys_Cycles();
	memset(&ri.pc, 0, sizeof(refStats_t));
}

/*
====================
R_TimeDemoFrame
====================
*/
void R_TimeDemoFrame()
{
	r_timeDemoFrame++;

	r_timeDemoMS += (Sys_Cycles() - r_timeDemoLastCycles) * Sys_MSPerCycle();
	r_timeDemoLastCycles = Sys_Cycles();
}

/*
====================
R_EndTimeDemo
====================
*/
void R_EndTimeDemo()
{
	assert(r_bInTimeDemo);
	r_bInTimeDemo = false;

	r_timeDemoMS += (Sys_Cycles() - r_timeDemoLastCycles) * Sys_MSPerCycle();

	const double InvDuration = (r_timeDemoMS != 0.0) ? 1.0 / r_timeDemoMS : 0.0f;
	const float InvFrameCount = (r_timeDemoFrame > 0) ? 1.0f / (float)r_timeDemoFrame : 0.0f;

	Com_Printf(0, "Refresh timedemo dump:\n");
	Com_Printf(0, "...number of frames: %i\n", r_timeDemoFrame);
	Com_Printf(0, "...duration: %7.2fms (%3.2fs)\n", r_timeDemoMS, r_timeDemoMS / 1000.0f);
	Com_Printf(0, "...fps: %3.1f\n", r_timeDemoFrame*1000.0f*InvDuration);

	Com_Printf(0, "Mesh buffering times (total/average):\n");
	Com_Printf(0, "...Add:  %7.2fms/%3.2fms\n", ri.pc.timeAddToList * Sys_MSPerCycle(), ri.pc.timeAddToList * Sys_MSPerCycle() * InvFrameCount);
	Com_Printf(0, "...Sort: %7.2fms/%3.2fms\n", ri.pc.timeSortList * Sys_MSPerCycle(), ri.pc.timeSortList * Sys_MSPerCycle() * InvFrameCount);
	Com_Printf(0, "...Push: %7.2fms/%3.2fms\n", ri.pc.timePushMesh * Sys_MSPerCycle(), ri.pc.timePushMesh * Sys_MSPerCycle() * InvFrameCount);
	Com_Printf(0, "...Draw: %7.2fms/%3.2fms\n", ri.pc.timeDrawList * Sys_MSPerCycle(), ri.pc.timeDrawList * Sys_MSPerCycle() * InvFrameCount);

	Com_Printf(0, "World rendering times (total/average):\n");
	Com_Printf(0, "...MarkLeaves:      %7.2fms/%3.2fms\n", ri.pc.timeMarkLeaves * Sys_MSPerCycle(), ri.pc.timeMarkLeaves * Sys_MSPerCycle() * InvFrameCount);
	Com_Printf(0, "...MarkLights:      %7.2fms/%3.2fms\n", ri.pc.timeMarkLights * Sys_MSPerCycle(), ri.pc.timeMarkLights * Sys_MSPerCycle() * InvFrameCount);
	Com_Printf(0, "...Recursion:       %7.2fms/%3.2fms\n", ri.pc.timeRecurseWorld * Sys_MSPerCycle(), ri.pc.timeRecurseWorld * Sys_MSPerCycle() * InvFrameCount);
	Com_Printf(0, "...ShadowRecursion: %7.2fms/%3.2fms\n", ri.pc.timeShadowRecurseWorld * Sys_MSPerCycle(), ri.pc.timeShadowRecurseWorld * Sys_MSPerCycle() * InvFrameCount);
}

// ==========================================================

/*
====================
R_ClearScene
====================
*/
void R_ClearScene()
{
	ri.scn.decalList.Clear();
	ri.scn.numDLights = 0;
	ri.scn.numEntities = 0;
	ri.scn.polyList.Clear();
}


/*
=====================
R_AddDecal
=====================
*/
void R_AddDecal(refDecal_t *decal, const colorb &color, const float materialTime)
{
	if (!decal || ri.scn.decalList.Count()+1 >= MAX_REF_DECALS)
		return;

	// Update
	for (int i=0 ; i<decal->poly.numVerts ; i++)
		decal->poly.colors[i] = color;
	decal->poly.matTime = materialTime;

	// Store
	ri.scn.decalList.Add(decal);
}


/*
=====================
R_AddEntity
=====================
*/
void R_AddEntity(refEntity_t *ent)
{
	if (ri.scn.numEntities+ENTLIST_OFFSET+1 >= MAX_REF_ENTITIES)
		return;
	if (ent->color[3] <= 0)
		return;

	ri.scn.entityList[ri.scn.numEntities+ENTLIST_OFFSET] = *ent;
	if (ent->color[3] < 255)
		ri.scn.entityList[ri.scn.numEntities+ENTLIST_OFFSET].flags |= RF_TRANSLUCENT;

	ri.scn.numEntities++;
}


/*
=====================
R_AddPoly
=====================
*/
void R_AddPoly(refPoly_t *poly)
{
	if (ri.scn.polyList.Count()+1 >= MAX_REF_POLYS)
		return;

	// Material
	if (!poly->mat)
		poly->mat = ri.media.noMaterial;

	// Store
	ri.scn.polyList.Add(poly);
}


/*
=====================
R_AddLight
=====================
*/
void R_AddLight(vec3_t origin, float intensity, float r, float g, float b)
{
	if (ri.scn.numDLights+1 >= MAX_REF_DLIGHTS)
		return;

	if (!intensity)
		return;

	refDLight_t *dl = &ri.scn.dLightList[ri.scn.numDLights++];

	Vec3Copy(origin, dl->origin);
	if (!r_coloredLighting->intVal)
	{
		float grey = (r * 0.3f) + (g * 0.59f) + (b * 0.11f);
		Vec3Set(dl->color, grey, grey, grey);
	}
	else
	{
		Vec3Set(dl->color, r, g, b);
	}
	dl->intensity = intensity;

	R_LightBounds(origin, intensity, dl->mins, dl->maxs);
}


/*
=====================
R_AddLightStyle
=====================
*/
void R_AddLightStyle(int style, float r, float g, float b)
{
	if (style < 0 || style > MAX_CS_LIGHTSTYLES)
	{
		Com_Error(ERR_DROP, "Bad light style %i", style);
		return;
	}

	refLightStyle_t *ls = &ri.scn.lightStyles[style];

	ls->white = r+g+b;
	Vec3Set(ls->rgb, r, g, b);
}

/*
=============================================================================

	MISC

=============================================================================
*/

/*
=============
R_GetRefConfig
=============
*/
void R_GetRefConfig(refConfig_t *outConfig)
{
	*outConfig = ri.config;
}


/*
=============
R_TransformToScreen_Vec3
=============
*/
void R_TransformToScreen_Vec3(vec3_t in, vec3_t out)
{
	vec4_t temp, temp2;
	temp[0] = in[0];
	temp[1] = in[1];
	temp[2] = in[2];
	temp[3] = 1.0f;
	Matrix4_Multiply_Vec4(ri.scn.worldViewProjectionMatrix, temp, temp2);

	if (!temp[3])
		return;

	out[0] = (temp2[0] / temp2[3] + 1.0f) * 0.5f * ri.def.width;
	out[1] = (temp2[1] / temp2[3] + 1.0f) * 0.5f * ri.def.height;
	out[2] = (temp2[2] / temp2[3] + 1.0f) * 0.5f;
}


/*
=============
R_TransformVectorToScreen
=============
*/
void R_TransformVectorToScreen(refDef_t *rd, vec3_t in, vec2_t out)
{
	if (!rd || !in || !out)
		return;

	vec4_t temp;
	temp[0] = in[0];
	temp[1] = in[1];
	temp[2] = in[2];
	temp[3] = 1.0f;

	mat4x4_t p;
	RB_SetupProjectionMatrix(rd, p);

	mat4x4_t m;
	RB_SetupModelviewMatrix(rd, m);

	vec4_t temp2;
	Matrix4_Multiply_Vec4(m, temp, temp2);
	Matrix4_Multiply_Vec4(p, temp2, temp);

	if (!temp[3])
		return;
	out[0] = rd->x + (temp[0] / temp[3] + 1.0f) * rd->width * 0.5f;
	out[1] = rd->y + (temp[1] / temp[3] + 1.0f) * rd->height * 0.5f;
}
