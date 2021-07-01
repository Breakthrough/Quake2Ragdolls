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
// cg_particles.c
//

#include "cg_local.h"

static TLinkedList<cgParticle_t*>	cg_particles;

/*
=============================================================================

	PARTICLE MANAGEMENT

=============================================================================
*/

/*
===============
CG_AllocParticle
===============
*/
static cgParticle_t *CG_AllocParticle()
{
	cgParticle_t *p;

	if (cg_particles.Count() >= (uint32)cg_particleMax->intVal)
		delete cg_particles.PopBack();

	p = new cgParticle_t();
	p->node = cg_particles.AddToFront(p);

	// Store static poly info
	p->outPoly.numVerts = 4;
	p->outPoly.colors = p->outColor;
	p->outPoly.texCoords = p->outCoords;
	p->outPoly.vertices = p->outVertices;
	p->outPoly.matTime = 0;

	return p;
}


/*
===============
CG_FreeParticle
===============
*/
static inline void CG_FreeParticle(cgParticle_t *p)
{
	cg_particles.RemoveNode(p->node);
	delete p;
}

/*
===============
CG_SpawnParticle

FIXME: JESUS H FUNCTION PARAMATERS
===============
*/
void CG_SpawnParticle  (float org0,						float org1,					float org2,
						float angle0,					float angle1,				float angle2,
						float vel0,						float vel1,					float vel2,
						float accel0,					float accel1,				float accel2,
						float red,						float green,				float blue,
						float redVel,					float greenVel,				float blueVel,
						float alpha,					float alphaVel,
						float size,						float sizeVel,
						const EParticleType type,		const uint32 flags,
						bool (*preThink)(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time),
						bool (*think)(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time),
						bool (*postThink)(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time),
						const EParticleStyle style,
						const float orient)
{
	cgParticle_t *p = CG_AllocParticle();
	p->time = (float)cg.refreshTime;
	p->type = type;

	Vec3Set(p->org, org0, org1, org2);

	Vec3Set(p->angle, angle0, angle1, angle2);
	Vec3Set(p->vel, vel0, vel1, vel2);
	Vec3Set(p->accel, accel0, accel1, accel2);

	Vec4Set(p->color, red, green, blue, alpha);
	Vec4Set(p->colorVel, redVel, greenVel, blueVel, alphaVel);

	p->mat = cgMedia.particleTable[p->type];
	p->style = style;
	p->flags = flags;

	p->size = size;
	p->sizeVel = sizeVel;
	p->orient = orient;

	p->preThink = preThink;
	p->bPreThinkNext = (preThink != NULL);
	if (p->bPreThinkNext)
	{
		p->lastPreThinkTime = p->time;
		p->nextPreThinkTime = p->time;
		Vec3Copy(p->org, p->lastPreThinkOrigin);
	}

	p->think = think;
	p->bThinkNext = (think != NULL);
	if (p->bThinkNext)
	{
		p->lastThinkTime = p->time;
		p->nextThinkTime = p->time;
		Vec3Copy(p->org, p->lastThinkOrigin);
	}

	p->postThink = postThink;
	p->bPostThinkNext = (postThink != NULL);
	if (p->bPostThinkNext)
	{
		p->lastPostThinkTime = p->time;
		p->nextPostThinkTime = p->time;
		Vec3Copy(p->org, p->lastPostThinkOrigin);
	}

	p->nextLightingTime = p->time;
}


/*
===============
CG_ClearParticles
===============
*/
void CG_ClearParticles()
{
	for (var node = cg_particles.Head(); node != null; node = node->Next)
		delete node->Value;

	cg_particles.Clear();
}


/*
===============
CG_AddParticles
===============
*/
void CG_AddParticles()
{
	CG_AddMapFXToList();
	CG_AddSustains();

	if (!cl_add_particles->intVal)
		return;

	int partNum = 0;
	TLinkedList<cgParticle_t*>::Node *next = null;

	for (var cur = cg_particles.Head(); cur != null; cur = next)
	{
		next = cur->Next;
		var p = cur->Value;

		// Check if we hit our limit
		if (++partNum > cg_particleMax->intVal)
			break;

		float time;
		vec4_t color;
		if (p->colorVel[3] > PART_INSTANT)
		{
			time = (cg.refreshTime - p->time)*0.001f;
			color[3] = p->color[3] + time*p->colorVel[3];
		}
		else
		{
			time = 1;
			color[3] = p->color[3];
		}

		// Faded out
		if (color[3] <= TINY_NUMBER)
		{
			CG_FreeParticle(p);
			continue;
		}

		if (color[3] > 1.0)
			color[3] = 1.0f;

		// Origin
		float timeSquared = time*time;

		vec3_t outOrigin;
		outOrigin[0] = p->org[0] + p->vel[0]*time + p->accel[0]*timeSquared;
		outOrigin[1] = p->org[1] + p->vel[1]*time + p->accel[1]*timeSquared;
		outOrigin[2] = p->org[2] + p->vel[2]*time + p->accel[2]*timeSquared;

		if (p->flags & PF_GRAVITY)
			outOrigin[2] -= (timeSquared * PART_GRAVITY);

		// sizeVel calcs
		float size;
		if (p->colorVel[3] > PART_INSTANT && p->size != p->sizeVel)
		{
			if (p->size > p->sizeVel) // shrink
				size = p->size - ((p->size - p->sizeVel) * (p->color[3] - color[3]));
			else // grow
				size = p->size + ((p->sizeVel - p->size) * (p->color[3] - color[3]));
		}
		else
		{
			size = p->size;
		}

		// Skip it if it's too small
		while (size > TINY_NUMBER)
		{
			// colorVel calcs
			Vec3Copy(p->color, color);
			if (p->colorVel[3] > PART_INSTANT)
			{
				for (int i=0 ; i<3 ; i++)
				{
					if (p->color[i] != p->colorVel[i])
					{
						if (p->color[i] > p->colorVel[i])
							color[i] = p->color[i] - ((p->color[i] - p->colorVel[i]) * (p->color[3] - color[3]));
						else
							color[i] = p->color[i] + ((p->colorVel[i] - p->color[i]) * (p->color[3] - color[3]));
					}

					color[i] = clamp(color[i], 0, 255);
				}
			}

			// Think functions
			bool bHadAThought = false;
			float outOrient = p->orient;

			if (p->bPreThinkNext && cg.refreshTime >= p->nextPreThinkTime)
			{
				p->bPreThinkNext = p->preThink(p, cg.refreshTime-p->lastPreThinkTime, p->nextPreThinkTime, outOrigin, p->lastPreThinkOrigin, p->angle, color, &size, &outOrient, &time);
				p->lastPreThinkTime = cg.refreshTime;
				Vec3Copy(outOrigin, p->lastPreThinkOrigin);

				bHadAThought = true;
			}

			if (p->bThinkNext && cg.refreshTime >= p->nextThinkTime)
			{
				p->bThinkNext = p->think(p, cg.refreshTime-p->lastThinkTime, p->nextThinkTime, outOrigin, p->lastThinkOrigin, p->angle, color, &size, &outOrient, &time);
				p->lastThinkTime = cg.refreshTime;
				Vec3Copy(outOrigin, p->lastThinkOrigin);

				bHadAThought = true;
			}

			if (p->bPostThinkNext && cg.refreshTime >= p->nextPostThinkTime)
			{
				p->bPostThinkNext = p->postThink(p, cg.refreshTime-p->lastPostThinkTime, p->nextPostThinkTime, outOrigin, p->lastPostThinkOrigin, p->angle, color, &size, &outOrient, &time);
				p->lastPostThinkTime = cg.refreshTime;
				Vec3Copy(outOrigin, p->lastPostThinkOrigin);

				bHadAThought = true;
			}

			// Check alpha and size after the think function runs
			if (bHadAThought)
			{
				if (color[3] <= TINY_NUMBER)
					break;
				if (size <= TINY_NUMBER)
					break;
			}

			bool culled = false;

			// Culling
			switch (p->style)
			{
			case PART_STYLE_ANGLED:
			case PART_STYLE_BEAM:
			case PART_STYLE_DIRECTION:
				break;

			default:
				if (cg_particleCulling->intVal)
				{
					// Kill particles behind the view
					vec3_t temp;
					Vec3Subtract(outOrigin, cg.refDef.viewOrigin, temp);
					VectorNormalizeFastf(temp);
					if (DotProduct(temp, cg.refDef.viewAxis[0]) < 0)
						culled = true;

					// Lessen fillrate consumption
					if (!(p->flags & PF_NOCLOSECULL))
					{
						float dist = Vec3DistSquared(cg.refDef.viewOrigin, outOrigin);
						if (dist <= 5*5)
							culled = true;
					}
				}
				break;
			}

			if (culled)
				break;

			// Alpha*color
			if (p->flags & PF_ALPHACOLOR)
				Vec3Scale(color, color[3], color);

			// Add to be rendered
			float scale;
			if (p->flags & PF_SCALED)
			{
				scale = (outOrigin[0] - cg.refDef.viewOrigin[0]) * cg.refDef.viewAxis[0][0] +
						(outOrigin[1] - cg.refDef.viewOrigin[1]) * cg.refDef.viewAxis[0][1] +
						(outOrigin[2] - cg.refDef.viewOrigin[2]) * cg.refDef.viewAxis[0][2];

				scale = (scale < 20) ? 1 : 1 + scale * 0.004f;
				scale = (scale - 1) + size;
			}
			else
			{
				scale = size;
			}

			// Rendering
			colorb outColor (
				color[0],
				color[1],
				color[2],
				color[3] * 255);

			switch(p->style)
			{
			case PART_STYLE_ANGLED:
				{
					vec3_t a_upVec, a_rtVec;

					Angles_Vectors(p->angle, NULL, a_rtVec, a_upVec); 

					if (outOrient)
					{
						float c, s;

						Q_SinCosf(DEG2RAD(outOrient), &c, &s);
						c *= scale;
						s *= scale;

						// Top left
						Vec2Set(p->outCoords[0], cgMedia.particleCoords[p->type][0], cgMedia.particleCoords[p->type][1]);
						Vec3Set(p->outVertices[0],	outOrigin[0] + a_upVec[0]*s - a_rtVec[0]*c,
													outOrigin[1] + a_upVec[1]*s - a_rtVec[1]*c,
													outOrigin[2] + a_upVec[2]*s - a_rtVec[2]*c);

						// Bottom left
						Vec2Set(p->outCoords[1], cgMedia.particleCoords[p->type][0], cgMedia.particleCoords[p->type][3]);
						Vec3Set(p->outVertices[1],	outOrigin[0] - a_upVec[0]*c - a_rtVec[0]*s,
													outOrigin[1] - a_upVec[1]*c - a_rtVec[1]*s,
													outOrigin[2] - a_upVec[2]*c - a_rtVec[2]*s);

						// Bottom right
						Vec2Set(p->outCoords[2], cgMedia.particleCoords[p->type][2], cgMedia.particleCoords[p->type][3]);
						Vec3Set(p->outVertices[2],	outOrigin[0] - a_upVec[0]*s + a_rtVec[0]*c,
													outOrigin[1] - a_upVec[1]*s + a_rtVec[1]*c,
													outOrigin[2] - a_upVec[2]*s + a_rtVec[2]*c);

						// Top right
						Vec2Set(p->outCoords[3], cgMedia.particleCoords[p->type][2], cgMedia.particleCoords[p->type][1]);
						Vec3Set(p->outVertices[3],	outOrigin[0] + a_upVec[0]*c + a_rtVec[0]*s,
													outOrigin[1] + a_upVec[1]*c + a_rtVec[1]*s,
													outOrigin[2] + a_upVec[2]*c + a_rtVec[2]*s);
					}
					else
					{
						// Top left
						Vec2Set(p->outCoords[0], cgMedia.particleCoords[p->type][0], cgMedia.particleCoords[p->type][1]);
						Vec3Set(p->outVertices[0],	outOrigin[0] + a_upVec[0]*scale - a_rtVec[0]*scale,
													outOrigin[1] + a_upVec[1]*scale - a_rtVec[1]*scale,
													outOrigin[2] + a_upVec[2]*scale - a_rtVec[2]*scale);

						// Bottom left
						Vec2Set(p->outCoords[1], cgMedia.particleCoords[p->type][0], cgMedia.particleCoords[p->type][3]);
						Vec3Set(p->outVertices[1],	outOrigin[0] - a_upVec[0]*scale - a_rtVec[0]*scale,
													outOrigin[1] - a_upVec[1]*scale - a_rtVec[1]*scale,
													outOrigin[2] - a_upVec[2]*scale - a_rtVec[2]*scale);

						// Bottom right
						Vec2Set(p->outCoords[2], cgMedia.particleCoords[p->type][2], cgMedia.particleCoords[p->type][3]);
						Vec3Set(p->outVertices[2],	outOrigin[0] - a_upVec[0]*scale + a_rtVec[0]*scale,
													outOrigin[1] - a_upVec[1]*scale + a_rtVec[1]*scale,
													outOrigin[2] - a_upVec[2]*scale + a_rtVec[2]*scale);

						// Top right
						Vec2Set(p->outCoords[3], cgMedia.particleCoords[p->type][2], cgMedia.particleCoords[p->type][1]);
						Vec3Set(p->outVertices[3],	outOrigin[0] + a_upVec[0]*scale + a_rtVec[0]*scale,
													outOrigin[1] + a_upVec[1]*scale + a_rtVec[1]*scale,
													outOrigin[2] + a_upVec[2]*scale + a_rtVec[2]*scale);
					}

					// Render it
					p->outColor[0] = outColor;
					p->outColor[1] = outColor;
					p->outColor[2] = outColor;
					p->outColor[3] = outColor;

					p->outPoly.mat = p->mat;
					Vec3Copy(outOrigin, p->outPoly.origin);
					p->outPoly.radius = scale;

					cgi.R_AddPoly(&p->outPoly);
				}
				break;

			case PART_STYLE_BEAM:
				{
					vec3_t point, width;

					Vec3Subtract(outOrigin, cg.refDef.viewOrigin, point);
					CrossProduct(point, p->angle, width);
					VectorNormalizeFastf(width);
					Vec3Scale(width, scale, width);

					vec3_t delta;
					Vec3Add(outOrigin, p->angle, delta);
					float dist = Vec3DistFast(outOrigin, delta) / 64.0f; // FIXME: tile based off of material's height (see: sizeBase)

					Vec2Set(p->outCoords[0], 0, 0);
					Vec3Set(p->outVertices[0],	outOrigin[0] - width[0],
												outOrigin[1] - width[1],
												outOrigin[2] - width[2]);

					Vec2Set(p->outCoords[1], 1, 0);
					Vec3Set(p->outVertices[1],	outOrigin[0] + width[0],
												outOrigin[1] + width[1],
												outOrigin[2] + width[2]);

					Vec3Add(point, p->angle, point);
					CrossProduct(point, p->angle, width);
					VectorNormalizeFastf(width);
					Vec3Scale(width, scale, width);

					Vec2Set(p->outCoords[2], 1, dist);
					Vec3Set(p->outVertices[2],	delta[0] + width[0],
												delta[1] + width[1],
												delta[2] + width[2]);

					Vec2Set(p->outCoords[3], 0, dist);
					Vec3Set(p->outVertices[3],	delta[0] - width[0],
												delta[1] - width[1],
												delta[2] - width[2]);

					// Render it
					p->outColor[0] = outColor;
					p->outColor[1] = outColor;
					p->outColor[2] = outColor;
					p->outColor[3] = outColor;

					p->outPoly.mat = p->mat;
					Vec3Copy(outOrigin, p->outPoly.origin);
					p->outPoly.radius = Vec3DistFast(outOrigin, delta);

					cgi.R_AddPoly(&p->outPoly);
				}
				break;

			case PART_STYLE_DIRECTION:
				{
					vec3_t delta, vdelta;

					Vec3Add(p->angle, outOrigin, vdelta);

					vec3_t move;
					Vec3Subtract(outOrigin, vdelta, move);
					VectorNormalizeFastf(move);

					vec3_t a_upVec, a_rtVec;
					Vec3Copy(move, a_upVec);
					Vec3Subtract(cg.refDef.viewOrigin, vdelta, delta);
					CrossProduct(a_upVec, delta, a_rtVec);

					VectorNormalizeFastf(a_rtVec);

					Vec3Scale(a_rtVec, 0.75f, a_rtVec);
					Vec3Scale(a_upVec, 0.75f * Vec3LengthFast(p->angle), a_upVec);

					// Top left
					Vec2Set(p->outCoords[0], cgMedia.particleCoords[p->type][0], cgMedia.particleCoords[p->type][1]);
					Vec3Set(p->outVertices[0], outOrigin[0] + a_upVec[0]*scale - a_rtVec[0]*scale,
												outOrigin[1] + a_upVec[1]*scale - a_rtVec[1]*scale,
												outOrigin[2] + a_upVec[2]*scale - a_rtVec[2]*scale);

					// Bottom left
					Vec2Set(p->outCoords[1], cgMedia.particleCoords[p->type][0], cgMedia.particleCoords[p->type][3]);
					Vec3Set(p->outVertices[1], outOrigin[0] - a_upVec[0]*scale - a_rtVec[0]*scale,
												outOrigin[1] - a_upVec[1]*scale - a_rtVec[1]*scale,
												outOrigin[2] - a_upVec[2]*scale - a_rtVec[2]*scale);

					// Bottom right
					Vec2Set(p->outCoords[2], cgMedia.particleCoords[p->type][2], cgMedia.particleCoords[p->type][3]);
					Vec3Set(p->outVertices[2], outOrigin[0] - a_upVec[0]*scale + a_rtVec[0]*scale,
												outOrigin[1] - a_upVec[1]*scale + a_rtVec[1]*scale,
												outOrigin[2] - a_upVec[2]*scale + a_rtVec[2]*scale);

					// Top right
					Vec2Set(p->outCoords[3], cgMedia.particleCoords[p->type][2], cgMedia.particleCoords[p->type][1]);
					Vec3Set(p->outVertices[3], outOrigin[0] + a_upVec[0]*scale + a_rtVec[0]*scale,
												outOrigin[1] + a_upVec[1]*scale + a_rtVec[1]*scale,
												outOrigin[2] + a_upVec[2]*scale + a_rtVec[2]*scale);

					// Render it
					p->outColor[0] = outColor;
					p->outColor[1] = outColor;
					p->outColor[2] = outColor;
					p->outColor[3] = outColor;

					p->outPoly.mat = p->mat;
					Vec3Copy(outOrigin, p->outPoly.origin);
					p->outPoly.radius = scale;

					cgi.R_AddPoly(&p->outPoly);
				}
				break;

			case PART_STYLE_QUAD:
				if (outOrient)
				{
					float c, s;

					Q_SinCosf(DEG2RAD(outOrient), &c, &s);
					c *= scale;
					s *= scale;

					// Top left
					Vec2Set(p->outCoords[0], cgMedia.particleCoords[p->type][0], cgMedia.particleCoords[p->type][1]);
					Vec3Set(p->outVertices[0],	outOrigin[0] + cg.refDef.viewAxis[1][0]*c + cg.refDef.viewAxis[2][0]*s,
												outOrigin[1] + cg.refDef.viewAxis[1][1]*c + cg.refDef.viewAxis[2][1]*s,
												outOrigin[2] + cg.refDef.viewAxis[1][2]*c + cg.refDef.viewAxis[2][2]*s);

					// Bottom left
					Vec2Set(p->outCoords[1], cgMedia.particleCoords[p->type][0], cgMedia.particleCoords[p->type][3]);
					Vec3Set(p->outVertices[1],	outOrigin[0] - cg.refDef.viewAxis[1][0]*s + cg.refDef.viewAxis[2][0]*c,
												outOrigin[1] - cg.refDef.viewAxis[1][1]*s + cg.refDef.viewAxis[2][1]*c,
												outOrigin[2] - cg.refDef.viewAxis[1][2]*s + cg.refDef.viewAxis[2][2]*c);

					// Bottom right
					Vec2Set(p->outCoords[2], cgMedia.particleCoords[p->type][2], cgMedia.particleCoords[p->type][3]);
					Vec3Set(p->outVertices[2],	outOrigin[0] - cg.refDef.viewAxis[1][0]*c - cg.refDef.viewAxis[2][0]*s,
												outOrigin[1] - cg.refDef.viewAxis[1][1]*c - cg.refDef.viewAxis[2][1]*s,
												outOrigin[2] - cg.refDef.viewAxis[1][2]*c - cg.refDef.viewAxis[2][2]*s);

					// Top right
					Vec2Set(p->outCoords[3], cgMedia.particleCoords[p->type][2], cgMedia.particleCoords[p->type][1]);
					Vec3Set(p->outVertices[3],	outOrigin[0] + cg.refDef.viewAxis[1][0]*s - cg.refDef.viewAxis[2][0]*c,
												outOrigin[1] + cg.refDef.viewAxis[1][1]*s - cg.refDef.viewAxis[2][1]*c,
												outOrigin[2] + cg.refDef.viewAxis[1][2]*s - cg.refDef.viewAxis[2][2]*c);
				}
				else
				{
					// Top left
					Vec2Set(p->outCoords[0], cgMedia.particleCoords[p->type][0], cgMedia.particleCoords[p->type][1]);
					Vec3Set(p->outVertices[0],	outOrigin[0] + cg.refDef.viewAxis[2][0]*scale + cg.refDef.viewAxis[1][0]*scale,
												outOrigin[1] + cg.refDef.viewAxis[2][1]*scale + cg.refDef.viewAxis[1][1]*scale,
												outOrigin[2] + cg.refDef.viewAxis[2][2]*scale + cg.refDef.viewAxis[1][2]*scale);

					// Bottom left
					Vec2Set(p->outCoords[1], cgMedia.particleCoords[p->type][0], cgMedia.particleCoords[p->type][3]);
					Vec3Set(p->outVertices[1],	outOrigin[0] - cg.refDef.viewAxis[2][0]*scale + cg.refDef.viewAxis[1][0]*scale,
												outOrigin[1] - cg.refDef.viewAxis[2][1]*scale + cg.refDef.viewAxis[1][1]*scale,
												outOrigin[2] - cg.refDef.viewAxis[2][2]*scale + cg.refDef.viewAxis[1][2]*scale);

					// Bottom right
					Vec2Set(p->outCoords[2], cgMedia.particleCoords[p->type][2], cgMedia.particleCoords[p->type][3]);
					Vec3Set(p->outVertices[2],	outOrigin[0] - cg.refDef.viewAxis[2][0]*scale - cg.refDef.viewAxis[1][0]*scale,
												outOrigin[1] - cg.refDef.viewAxis[2][1]*scale - cg.refDef.viewAxis[1][1]*scale,
												outOrigin[2] - cg.refDef.viewAxis[2][2]*scale - cg.refDef.viewAxis[1][2]*scale);

					// Top right
					Vec2Set(p->outCoords[3], cgMedia.particleCoords[p->type][2], cgMedia.particleCoords[p->type][1]);
					Vec3Set(p->outVertices[3],	outOrigin[0] + cg.refDef.viewAxis[2][0]*scale - cg.refDef.viewAxis[1][0]*scale,
												outOrigin[1] + cg.refDef.viewAxis[2][1]*scale - cg.refDef.viewAxis[1][1]*scale,
												outOrigin[2] + cg.refDef.viewAxis[2][2]*scale - cg.refDef.viewAxis[1][2]*scale);
				}

				// Render it
				p->outColor[0] = outColor;
				p->outColor[1] = outColor;
				p->outColor[2] = outColor;
				p->outColor[3] = outColor;

				p->outPoly.mat = p->mat;
				Vec3Copy(outOrigin, p->outPoly.origin);
				p->outPoly.radius = scale;

				cgi.R_AddPoly(&p->outPoly);
				break;

			default:
				assert(0);
				break;
			}

			break;
		}

		// Kill if instant
		if (p->colorVel[3] <= PART_INSTANT)
		{
			p->color[3] = 0;
			p->colorVel[3] = 0;
		}
	}
}
