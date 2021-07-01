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
// cg_effects.h
//

/*
=============================================================================

	LIGHTING

=============================================================================
*/

struct cgDLight_t {
	vec3_t		origin;
	vec3_t		color;

	int			key;				// so entities can reuse same entry

	float		radius;
	float		die;				// stop lighting after this time
	float		decay;				// drop this each second
	float		minlight;			// don't add when contributing less
};

//
// cg_light.c
//

void	CG_ClearLightStyles ();
cgDLight_t *CG_AllocDLight (int key);
void	CG_RunLightStyles ();
void	CG_SetLightstyle (int num);
void	CG_AddLightStyles ();

void	CG_ClearDLights ();
void	CG_RunDLights ();
void	CG_AddDLights ();

void	CG_Flashlight (int ent, vec3_t pos);
void	__fastcall CG_ColorFlash (vec3_t pos, int ent, float intensity, float r, float g, float b);
void	CG_WeldingSparkFlash (vec3_t pos);

/*
=============================================================================

	EFFECTS

=============================================================================
*/

#define THINK_DELAY_DEFAULT		16.5f // 60 FPS
#define THINK_DELAY_EXPENSIVE	33.0f // 30 FPS

enum EParticleType
{
	PT_BFG_DOT,

	PT_BLASTER_BLUE,
	PT_BLASTER_GREEN,
	PT_BLASTER_RED,

	PT_IONTAIL,
	PT_IONTIP,
	PT_ITEMRESPAWN,
	PT_ENGYREPAIR_DOT,
	PT_PHALANXTIP,

	PT_GENERIC,
	PT_GENERIC_GLOW,

	PT_SMOKE,
	PT_SMOKE2,

	PT_SMOKEGLOW,
	PT_SMOKEGLOW2,

	PT_BLUEFIRE,
	PT_FIRE1,
	PT_FIRE2,
	PT_FIRE3,
	PT_FIRE4,
	PT_EMBERS1,
	PT_EMBERS2,
	PT_EMBERS3,

	PT_BLOODTRAIL,
	PT_BLOODTRAIL2,
	PT_BLOODTRAIL3,
	PT_BLOODTRAIL4,
	PT_BLOODTRAIL5,
	PT_BLOODTRAIL6,
	PT_BLOODTRAIL7,
	PT_BLOODTRAIL8,

	PT_GRNBLOODTRAIL,
	PT_GRNBLOODTRAIL2,
	PT_GRNBLOODTRAIL3,
	PT_GRNBLOODTRAIL4,
	PT_GRNBLOODTRAIL5,
	PT_GRNBLOODTRAIL6,
	PT_GRNBLOODTRAIL7,
	PT_GRNBLOODTRAIL8,

	PT_BLDDRIP01,
	PT_BLDDRIP02,
	PT_BLDDRIP01_GRN,
	PT_BLDDRIP02_GRN,
	PT_BLDSPURT,
	PT_BLDSPURT2,
	PT_BLDSPURT_GREEN,
	PT_BLDSPURT_GREEN2,

	PT_BEAM,

	PT_EXPLOFLASH,
	PT_EXPLOWAVE,

	PT_FLARE,
	PT_FLAREGLOW,

	PT_FLY,

	PT_RAIL_CORE,
	PT_RAIL_WAVE,
	PT_RAIL_SPIRAL,

	PT_SPARK,

	PT_WATERBUBBLE,
	PT_WATERDROPLET,
	PT_WATERIMPACT,
	PT_WATERMIST,
	PT_WATERMIST_GLOW,
	PT_WATERPLUME,
	PT_WATERPLUME_GLOW,
	PT_WATERRING,
	PT_WATERRIPPLE,

	// Animated
	PT_EXPLO1,
	PT_EXPLO2,
	PT_EXPLO3,
	PT_EXPLO4,
	PT_EXPLO5,
	PT_EXPLO6,
	PT_EXPLO7,

	PT_EXPLOEMBERS1,
	PT_EXPLOEMBERS2,

	// Map effects
	MFX_WHITE,
	MFX_CORONA,

	PT_PICTOTAL
};

enum EDecalType
{
	DT_BFG_BURNMARK,
	DT_BFG_GLOWMARK,

	DT_BLASTER_BLUEMARK,
	DT_BLASTER_BURNMARK,
	DT_BLASTER_GREENMARK,
	DT_BLASTER_REDMARK,

	DT_DRONE_SPIT_GLOW,

	DT_ENGYREPAIR_BURNMARK,
	DT_ENGYREPAIR_GLOWMARK,

	DT_BLOOD01,
	DT_BLOOD02,
	DT_BLOOD03,
	DT_BLOOD04,
	DT_BLOOD05,
	DT_BLOOD06,
	DT_BLOOD07,
	DT_BLOOD08,
	DT_BLOOD09,
	DT_BLOOD10,
	DT_BLOOD11,
	DT_BLOOD12,
	DT_BLOOD13,
	DT_BLOOD14,
	DT_BLOOD15,
	DT_BLOOD16,

	DT_BLOOD01_GRN,
	DT_BLOOD02_GRN,
	DT_BLOOD03_GRN,
	DT_BLOOD04_GRN,
	DT_BLOOD05_GRN,
	DT_BLOOD06_GRN,
	DT_BLOOD07_GRN,
	DT_BLOOD08_GRN,
	DT_BLOOD09_GRN,
	DT_BLOOD10_GRN,
	DT_BLOOD11_GRN,
	DT_BLOOD12_GRN,
	DT_BLOOD13_GRN,
	DT_BLOOD14_GRN,
	DT_BLOOD15_GRN,
	DT_BLOOD16_GRN,

	DT_BULLET,

	DT_EXPLOMARK,
	DT_EXPLOMARK2,
	DT_EXPLOMARK3,

	DT_RAIL_BURNMARK,
	DT_RAIL_GLOWMARK,
	DT_RAIL_WHITE,

	DT_SLASH,
	DT_SLASH2,
	DT_SLASH3,

	DT_PICTOTAL
};

extern vec3_t	cg_randVels[NUMVERTEXNORMALS];

/*
=============================================================================

	SCRIPTED MAP EFFECTS

=============================================================================
*/

void	CG_AddMapFXToList ();

void	CG_MapFXLoad (char *mapName);
void	CG_MapFXClear ();

void	CG_MapFXInit ();
void	CG_MapFXShutdown ();

/*
=============================================================================

	DECAL SYSTEM

=============================================================================
*/

struct cgDecal_t
{
	TLinkedList<cgDecal_t*>::Node	*node;

	refDecal_t				refDecal;

	float					time;

	vec4_t					color;
	vec4_t					colorVel;
	vec3_t					origin;

	float					size;

	float					lifeTime;

	uint32					flags;

	void					(*think)(struct cgDecal_t *d, vec4_t color, int *type, uint32 *flags);
	bool					thinkNext;
};

enum {
	DF_USE_BURNLIFE	= BIT(0),
	DF_FIXED_LIFE	= BIT(1),
	DF_ALPHACOLOR	= BIT(2),
};

cgDecal_t *CG_SpawnDecal(float org0,				float org1,					float org2,
						float dir0,					float dir1,					float dir2,
						float red,					float green,				float blue,
						float redVel,				float greenVel,				float blueVel,
						float alpha,				float alphaVel,
						float size,
						const EDecalType type,		const uint32 flags,
						void (*think)(struct cgDecal_t *d, vec4_t color, int *type, uint32 *flags),
						const bool thinkNext,
						const float lifeTime,		float angle);

// constants
#define DECAL_INSTANT	-10000.0f

// random texturing
inline EDecalType dRandBloodMark() { return (EDecalType)(DT_BLOOD01 + (rand()&15)); }
inline EDecalType dRandGrnBloodMark() { return (EDecalType)(DT_BLOOD01_GRN + (rand()&15)); }
inline EDecalType dRandExploMark() { return (EDecalType)(DT_EXPLOMARK + (rand()%3)); }
inline EDecalType dRandSlashMark() { return (EDecalType)(DT_SLASH + (rand()%3)); }

// management
void	CG_ClearDecals ();
void	CG_AddDecals ();

/*
=============================================================================

	PARTICLE SYSTEM

=============================================================================
*/

enum {
	PF_SCALED		= BIT(0),

	PF_GRAVITY		= BIT(1),
	PF_NOCLOSECULL	= BIT(2),
	PF_NODECAL		= BIT(3),
	PF_NOSFX		= BIT(4),
	PF_ALPHACOLOR	= BIT(5),

	// Special
	PF_GREENBLOOD	= BIT(6),
};

enum EParticleStyle
{
	PART_STYLE_QUAD,

	PART_STYLE_ANGLED,
	PART_STYLE_BEAM,
	PART_STYLE_DIRECTION
};

struct cgParticle_t
{
	TLinkedList<cgParticle_t*>::Node *node;

	EParticleType			type;
	float					time;

	vec3_t					org;

	vec3_t					angle;
	vec3_t					vel;
	vec3_t					accel;

	vec4_t					color;
	vec4_t					colorVel;

	float					size;
	float					sizeVel;

	struct refMaterial_t	*mat;
	EParticleStyle			style;
	uint32					flags;

	float					orient;

	bool					(*preThink)(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time);
	bool					bPreThinkNext;
	float					lastPreThinkTime;
	float					nextPreThinkTime;
	vec3_t					lastPreThinkOrigin;

	bool					(*think)(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time);
	bool					bThinkNext;
	float					lastThinkTime;
	float					nextThinkTime;
	vec3_t					lastThinkOrigin;

	bool					(*postThink)(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time);
	bool					bPostThinkNext;
	float					lastPostThinkTime;
	float					nextPostThinkTime;
	vec3_t					lastPostThinkOrigin;

	// For the lighting think functions
	vec3_t					lighting;
	float					nextLightingTime;

	// Passed to refresh
	refPoly_t				outPoly;
	colorb					outColor[4];
	vec2_t					outCoords[4];
	vec3_t					outVertices[4];
};

void	CG_SpawnParticle (float org0,					float org1,					float org2,
						float angle0,					float angle1,				float angle2,
						float vel0,						float vel1,					float vel2,
						float accel0,					float accel1,				float accel2,
						float red,						float green,				float blue,
						float redVel,					float greenVel,				float blueVel,
						float alpha,					float alphaVel,
						float size,						float sizeVel,
						const EParticleType type,		const uint32 flags,
						bool (*preThink)(cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time),
						bool (*think)(cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time),
						bool (*postThink)(cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time),
						const EParticleStyle style,
						const float orient);

// constants
#define PMAXBLDDRIPLEN	3.25f
#define PMAXSPLASHLEN	2.0f

#define PART_GRAVITY	110
#define PART_INSTANT	-1000.0f

#define BEAMLENGTH		16

// random texturing
inline EParticleType pRandBloodDrip() { return (EParticleType)(PT_BLDDRIP01 + (rand()&1)); }
inline EParticleType pRandGrnBloodDrip() { return (EParticleType)(PT_BLDDRIP01_GRN + (rand()&1)); }
inline EParticleType pRandBloodTrail() { return (EParticleType)(PT_BLOODTRAIL + (rand()&7)); }
inline EParticleType pRandGrnBloodTrail() { return (EParticleType)(PT_GRNBLOODTRAIL + (rand()&7)); }
inline EParticleType pRandSmoke() { return (EParticleType)(PT_SMOKE + (rand()&1)); }
inline EParticleType pRandGlowSmoke() { return (EParticleType)(PT_SMOKEGLOW + (rand()&1)); }
inline EParticleType pRandEmbers() { return (EParticleType)(PT_EMBERS1 + (rand()%3)); }
inline EParticleType pRandFire() { return (EParticleType)(PT_FIRE1 + (rand()&3)); }

// management
void	CG_ClearParticles ();
void	CG_AddParticles ();

//
// GENERIC EFFECTS
//

bool	CG_FindExplosionDir (vec3_t origin, float radius, vec3_t endPos, vec3_t dir);

void	CG_BlasterBlueParticles (vec3_t org, vec3_t dir);
void	CG_BlasterGoldParticles (vec3_t org, vec3_t dir);
void	CG_BlasterGreenParticles (vec3_t org, vec3_t dir);
void	CG_BlasterGreyParticles (vec3_t org, vec3_t dir);
void	CG_BleedEffect (vec3_t org, vec3_t dir, const int count, const bool bGreen = false);
void	CG_BubbleEffect (vec3_t origin);
void	CG_ExplosionBFGEffect (vec3_t org);
void	CG_FlareEffect (vec3_t origin, EParticleType type, float orient, float size, float sizevel, int color, int colorvel, float alpha, float alphavel);
void	CG_ItemRespawnEffect (vec3_t org);
void	CG_LogoutEffect (vec3_t org, int type);

void	__fastcall CG_ParticleEffect (vec3_t org, vec3_t dir, int color, int count);
void	__fastcall CG_ParticleEffect2 (vec3_t org, vec3_t dir, int color, int count);
void	__fastcall CG_ParticleEffect3 (vec3_t org, vec3_t dir, int color, int count);
void	__fastcall CG_ParticleSmokeEffect (vec3_t org, vec3_t dir, int color, int count, int magnitude);
void	__fastcall CG_RicochetEffect (vec3_t org, vec3_t dir, int count);

void	CG_RocketFireParticles (vec3_t org, vec3_t dir);

void	__fastcall CG_SparkEffect (vec3_t org, vec3_t dir, int color, int colorvel, int count, float smokeScale, float lifeScale);
void	__fastcall CG_SplashParticles (vec3_t org, vec3_t dir, int color, int count, bool glow);
void	__fastcall CG_SplashEffect (vec3_t org, vec3_t dir, int color, int count);

void	CG_BigTeleportParticles (vec3_t org);
void	CG_BlasterTip (vec3_t start, vec3_t end);
void	CG_ExplosionParticles (vec3_t org, float scale, bool exploonly, bool inwater);
void	CG_ExplosionBFGParticles (vec3_t org);
void	CG_ExplosionColorParticles (vec3_t org);
void	CG_FlyEffect (cgEntity_t *ent, vec3_t origin);
void	CG_ForceWall (vec3_t start, vec3_t end, int color);
void	CG_MonsterPlasma_Shell (vec3_t origin);
void	CG_PhalanxTip (vec3_t start, vec3_t end);
void	CG_TeleportParticles (vec3_t org);
void	CG_TeleporterParticles (entityState_t *ent);
void	CG_TrackerShell (vec3_t origin);
void	CG_TrapParticles (refEntity_t *ent);
void	CG_WidowSplash (vec3_t org);

//
// SUSTAINED EFFECTS
//

void	__fastcall CG_ParticleSteamEffect (vec3_t org, vec3_t dir, int color, int count, int magnitude);

void	CG_ParseNuke ();
void	CG_ParseSteam ();
void	CG_ParseWidow ();

void	CG_ClearSustains ();
void	CG_AddSustains ();

//
// TRAIL EFFECTS
//

void	__fastcall CG_BeamTrail (vec3_t start, vec3_t end, int color, float size, float alpha, float alphaVel);
void	CG_BfgTrail (refEntity_t *ent);
void	CG_BlasterGoldTrail (vec3_t oldLerped, vec3_t newLerped);
void	CG_BlasterGreenTrail (vec3_t start, vec3_t end);
void	CG_BubbleTrail (vec3_t start, vec3_t end);
void	CG_BubbleTrail2 (vec3_t start, vec3_t end, int dist);
void	CG_DebugTrail (vec3_t start, vec3_t end, colorb color);
void	CG_FlagTrail (vec3_t start, vec3_t end, int flags);
void	CG_GibTrail (vec3_t start, vec3_t end, int flags);
void	CG_GrenadeTrail (vec3_t start, vec3_t end);
void	CG_Heatbeam (vec3_t start, vec3_t forward);
void	CG_IonripperTrail (vec3_t start, vec3_t end);
void	CG_QuadTrail (vec3_t start, vec3_t end);
void	CG_RailTrail (vec3_t start, vec3_t end);
void	CG_RocketTrail (refEntity_t &entity, vec3_t start, vec3_t end);
void	CG_TagTrail (vec3_t start, vec3_t end);
void	CG_TrackerTrail (vec3_t start, vec3_t end);

//
// PARTICLE THINK FUNCTIONS
//

bool pAirOnlyThink(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time);
bool pBlasterGlowThink(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time);
bool pBloodDripThink(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time);
bool pBloodThink(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time);
bool pBounceThink(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time);
bool pDropletThink(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time);
bool pExploAnimThink(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time);
bool pFireThink(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time);
bool pFlareThink(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time);
bool pLight70Think(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time);
bool pRailSpiralThink(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time);
bool pRicochetSparkThink(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time);
bool pFastSmokeThink(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time);
bool pSmokeThink(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time);
bool pSparkGrowThink(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time);
bool pSplashThink(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time);
bool pWaterOnlyThink(struct cgParticle_t *p, const float deltaTime, float &nextThinkTime, vec3_t org, vec3_t lastOrg, vec3_t angle, vec4_t color, float *size, float *orient, float *time);

/*
=============================================================================

	SUSTAINED PARTICLE EFFECTS

=============================================================================
*/

struct cgSustainPfx_t {
	vec3_t		org;
	vec3_t		dir;

	int			id;
	int			type;

	int			endtime;
	int			nextthink;
	int			thinkinterval;

	int			color;
	int			count;
	int			magnitude;

	void		(*think)(cgSustainPfx_t *self);
};
