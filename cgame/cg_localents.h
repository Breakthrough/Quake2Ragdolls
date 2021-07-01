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
// cg_localents.h
// Local entities. Things like bullet casings...
//

class localEnt_t
{
public:
	localEnt_t (vec3_t origin, colorb color, vec3_t angles, vec3_t avelocity, vec3_t velocity);
	void SetModel (refModel_t* model);
	void ColorUpdate();

	int					time;

	refEntity_t			refEnt;

	vec3_t				angles;
	vec3_t				avel;

	vec3_t				velocity;
	vec3_t				mins, maxs;
	leType_t			type;

	bool				remove;

	virtual void		Think() = 0;
};

// brass state
enum
{
	BRASS_RUNNING,
	BRASS_INTERP0,
	BRASS_INTERP1,
	BRASS_INTERP2,
	BRASS_ANGLE,
	BRASS_SLEEP
};

class brassLocalEnt_t : public localEnt_t
{
public:
	byte brassState;
	vec3_t savedAngle;

	float gravity;

	brassLocalEnt_t (vec3_t origin, colorb color, vec3_t angles, vec3_t avelocity, vec3_t velocity, float gravity) :
	  localEnt_t(origin, color, angles, avelocity, velocity)
	  {
		  brassState = 0;
		  this->gravity = gravity;
	  }

	  void Think();
};