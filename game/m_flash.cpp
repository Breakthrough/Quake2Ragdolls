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

// m_flash.c

#include "../shared/shared.h"

// this file is included in both the game dll and egl, the game needs it to source shot
// locations, the client needs it to position muzzle flashes
vec3_t dumb_and_hacky_monster_MuzzFlashOffset [] = {
	0.0f,			0.0f,			0.0f,			// not used							0

	20.7f,			-18.5f,			28.7f,			// MZ2_TANK_BLASTER_1				1
	16.6f,			-21.5f,			30.1f,			// MZ2_TANK_BLASTER_2				2
	11.8f,			-23.9f,			32.1f,			// MZ2_TANK_BLASTER_3				3

	22.9f,			-0.7f,			25.3f,			// MZ2_TANK_MACHINEGUN_1			4
	22.2f,			6.2f,			22.3f,			// MZ2_TANK_MACHINEGUN_2			5
	19.4f,			13.1f,			18.6f,			// MZ2_TANK_MACHINEGUN_3			6
	19.4f,			18.8f,			18.6f,			// MZ2_TANK_MACHINEGUN_4			7
	17.9f,			25.0f,			18.6f,			// MZ2_TANK_MACHINEGUN_5			8
	14.1f,			30.5f,			20.6f,			// MZ2_TANK_MACHINEGUN_6			9
	9.3f,			35.3f,			22.1f,			// MZ2_TANK_MACHINEGUN_7			10
	4.7f,			38.4f,			22.1f,			// MZ2_TANK_MACHINEGUN_8			11
	-1.1f,			40.4f,			24.1f,			// MZ2_TANK_MACHINEGUN_9			12
	-6.5f,			41.2f,			24.1f,			// MZ2_TANK_MACHINEGUN_10			13
	3.2f,			40.1f,			24.7f,			// MZ2_TANK_MACHINEGUN_11			14
	11.7f,			36.7f,			26.0f,			// MZ2_TANK_MACHINEGUN_12			15
	18.9f,			31.3f,			26.0f,			// MZ2_TANK_MACHINEGUN_13			16
	24.4f,			24.4f,			26.4f,			// MZ2_TANK_MACHINEGUN_14			17
	27.1f,			17.1f,			27.2f,			// MZ2_TANK_MACHINEGUN_15			18
	28.5f,			9.1f,			28.0f,			// MZ2_TANK_MACHINEGUN_16			19
	27.1f,			2.2f,			28.0f,			// MZ2_TANK_MACHINEGUN_17			20
	24.9f,			-2.8f,			28.0f,			// MZ2_TANK_MACHINEGUN_18			21
	21.6f,			-7.0f,			26.4f,			// MZ2_TANK_MACHINEGUN_19			22

	6.2f,			29.1f,			49.1f,			// MZ2_TANK_ROCKET_1				23
	6.9f,			23.8f,			49.1f,			// MZ2_TANK_ROCKET_2				24
	8.3f,			17.8f,			49.5f,			// MZ2_TANK_ROCKET_3				25

	26.6f,			7.1f,			13.1f,			// MZ2_INFANTRY_MACHINEGUN_1		26
	18.2f,			7.5f,			15.4f,			// MZ2_INFANTRY_MACHINEGUN_2		27
	17.2f,			10.3f,			17.9f,			// MZ2_INFANTRY_MACHINEGUN_3		28
	17.0f,			12.8f,			20.1f,			// MZ2_INFANTRY_MACHINEGUN_4		29
	15.1f,			14.1f,			21.8f,			// MZ2_INFANTRY_MACHINEGUN_5		30
	11.8f,			17.2f,			23.1f,			// MZ2_INFANTRY_MACHINEGUN_6		31
	11.4f,			20.2f,			21.0f,			// MZ2_INFANTRY_MACHINEGUN_7		32
	9.0f,			23.0f,			18.9f,			// MZ2_INFANTRY_MACHINEGUN_8		33
	13.9f,			18.6f,			17.7f,			// MZ2_INFANTRY_MACHINEGUN_9		34
	15.4f,			15.6f,			15.8f,			// MZ2_INFANTRY_MACHINEGUN_10		35
	10.2f,			15.2f,			25.1f,			// MZ2_INFANTRY_MACHINEGUN_11		36
	-1.9f,			15.1f,			28.2f,			// MZ2_INFANTRY_MACHINEGUN_12		37
	-12.4f,			13.0f,			20.2f,			// MZ2_INFANTRY_MACHINEGUN_13		38

	10.6f * 1.2f,	7.7f * 1.2f,	7.8f * 1.2f,		// MZ2_SOLDIER_BLASTER_1			39
	21.1f * 1.2f,	3.6f * 1.2f,	19.0f * 1.2f,		// MZ2_SOLDIER_BLASTER_2			40
	10.6f * 1.2f,	7.7f * 1.2f,	7.8f * 1.2f,		// MZ2_SOLDIER_SHOTGUN_1			41
	21.1f * 1.2f,	3.6f * 1.2f,	19.0f * 1.2f,		// MZ2_SOLDIER_SHOTGUN_2			42
	10.6f * 1.2f,	7.7f * 1.2f,	7.8f * 1.2f,		// MZ2_SOLDIER_MACHINEGUN_1			43
	21.1f * 1.2f,	3.6f * 1.2f,	19.0f * 1.2f,		// MZ2_SOLDIER_MACHINEGUN_2			44

	30.1f * 1.15f,	3.9f * 1.15f,	19.6f * 1.15f,	// MZ2_GUNNER_MACHINEGUN_1			45
	29.1f * 1.15f,	2.5f * 1.15f,	20.7f * 1.15f,	// MZ2_GUNNER_MACHINEGUN_2			46
	28.2f * 1.15f,	2.5f * 1.15f,	22.2f * 1.15f,	// MZ2_GUNNER_MACHINEGUN_3			47
	28.2f * 1.15f,	3.6f * 1.15f,	22.0f * 1.15f,	// MZ2_GUNNER_MACHINEGUN_4			48
	26.9f * 1.15f,	2.0f * 1.15f,	23.4f * 1.15f,	// MZ2_GUNNER_MACHINEGUN_5			49
	26.5f * 1.15f,	0.6f * 1.15f,	20.8f * 1.15f,	// MZ2_GUNNER_MACHINEGUN_6			50
	26.9f * 1.15f,	0.5f * 1.15f,	21.5f * 1.15f,	// MZ2_GUNNER_MACHINEGUN_7			51
	29.0f * 1.15f,	2.4f * 1.15f,	19.5f * 1.15f,	// MZ2_GUNNER_MACHINEGUN_8			52
	4.6f * 1.15f,	-16.8f * 1.15f,	7.3f * 1.15f,		// MZ2_GUNNER_GRENADE_1				53
	4.6f * 1.15f,	-16.8f * 1.15f,	7.3f * 1.15f,		// MZ2_GUNNER_GRENADE_2				54
	4.6f * 1.15f,	-16.8f * 1.15f,	7.3f * 1.15f,		// MZ2_GUNNER_GRENADE_3				55
	4.6f * 1.15f,	-16.8f * 1.15f,	7.3f * 1.15f,		// MZ2_GUNNER_GRENADE_4				56

	24.8f,			-9.0f,			39.0f,			// MZ2_CHICK_ROCKET_1				57

	12.1f,			13.4f,			-14.5f,			// MZ2_FLYER_BLASTER_1				58
	12.1f,			-7.4f,			-14.5f,			// MZ2_FLYER_BLASTER_2				59

	12.1f,			5.4f,			16.5f,			// MZ2_MEDIC_BLASTER_1				60

	30.0f,			18.0f,			28.0f,			// MZ2_GLADIATOR_RAILGUN_1			61

	32.5f,			-0.8f,			10.0f,			// MZ2_HOVER_BLASTER_1				62

	18.4f,			7.4f,			9.6f,			// MZ2_ACTOR_MACHINEGUN_1			63

	30.0f,			30.0f,			88.5f,			// MZ2_SUPERTANK_MACHINEGUN_1		64
	30.0f,			30.0f,			88.5f,			// MZ2_SUPERTANK_MACHINEGUN_2		65
	30.0f,			30.0f,			88.5f,			// MZ2_SUPERTANK_MACHINEGUN_3		66
	30.0f,			30.0f,			88.5f,			// MZ2_SUPERTANK_MACHINEGUN_4		67
	30.0f,			30.0f,			88.5f,			// MZ2_SUPERTANK_MACHINEGUN_5		68
	30.0f,			30.0f,			88.5f,			// MZ2_SUPERTANK_MACHINEGUN_6		69

	16.0f,			-22.5f,			91.2f,			// MZ2_SUPERTANK_ROCKET_1			70
	16.0f,			-33.4f,			86.7f,			// MZ2_SUPERTANK_ROCKET_2			71
	16.0f,			-42.8f,			83.3f,			// MZ2_SUPERTANK_ROCKET_3			72

	// --- Start Xian Stuff ---
	32.0f,			-40.0f,			70.0f,				// MZ2_BOSS2_MACHINEGUN_L1			73
	32.0f,			-40.0f,			70.0f,				// MZ2_BOSS2_MACHINEGUN_L2			74
	32.0f,			-40.0f,			70.0f,				// MZ2_BOSS2_MACHINEGUN_L3			75
	32.0f,			-40.0f,			70.0f,				// MZ2_BOSS2_MACHINEGUN_L4			76
	32.0f,			-40.0f,			70.0f,				// MZ2_BOSS2_MACHINEGUN_L5			77
	// --- End Xian Stuff ---

	22.0f,			16.0f,			10.0f,			// MZ2_BOSS2_ROCKET_1				78
	22.0f,			8.0f,			10.0f,			// MZ2_BOSS2_ROCKET_2				79
	22.0f,			-8.0f,			10.0f,			// MZ2_BOSS2_ROCKET_3				80
	22.0f,			-16.0f,			10.0f,			// MZ2_BOSS2_ROCKET_4				81

	32.5f,			-0.8f,			10.0f,				// MZ2_FLOAT_BLASTER_1				82

	20.8f * 1.2f,	10.1f * 1.2f,	-2.7f * 1.2f,		// MZ2_SOLDIER_BLASTER_3			83
	20.8f * 1.2f,	10.1f * 1.2f,	-2.7f * 1.2f,		// MZ2_SOLDIER_SHOTGUN_3			84
	20.8f * 1.2f,	10.1f * 1.2f,	-2.7f * 1.2f,		// MZ2_SOLDIER_MACHINEGUN_3			85
	7.6f * 1.2f,	9.3f * 1.2f,	0.8f * 1.2f,		// MZ2_SOLDIER_BLASTER_4			86
	7.6f * 1.2f,	9.3f * 1.2f,	0.8f * 1.2f,		// MZ2_SOLDIER_SHOTGUN_4			87
	7.6f * 1.2f,	9.3f * 1.2f,	0.8f * 1.2f,		// MZ2_SOLDIER_MACHINEGUN_4			88
	30.5f * 1.2f,	9.9f * 1.2f,	-18.7f * 1.2f,	// MZ2_SOLDIER_BLASTER_5			89
	30.5f * 1.2f,	9.9f * 1.2f,	-18.7f * 1.2f,	// MZ2_SOLDIER_SHOTGUN_5			90
	30.5f * 1.2f,	9.9f * 1.2f,	-18.7f * 1.2f,	// MZ2_SOLDIER_MACHINEGUN_5			91
	27.6f * 1.2f,	3.4f * 1.2f,	-10.4f * 1.2f,	// MZ2_SOLDIER_BLASTER_6			92
	27.6f * 1.2f,	3.4f * 1.2f,	-10.4f * 1.2f,	// MZ2_SOLDIER_SHOTGUN_6			93
	27.6f * 1.2f,	3.4f * 1.2f,	-10.4f * 1.2f,	// MZ2_SOLDIER_MACHINEGUN_6			94
	28.9f * 1.2f,	4.6f * 1.2f,	-8.1f * 1.2f,		// MZ2_SOLDIER_BLASTER_7			95
	28.9f * 1.2f,	4.6f * 1.2f,	-8.1f * 1.2f,		// MZ2_SOLDIER_SHOTGUN_7			96
	28.9f * 1.2f,	4.6f * 1.2f,	-8.1f * 1.2f,		// MZ2_SOLDIER_MACHINEGUN_7			97
	31.5f * 1.2f,	9.6f * 1.2f,	10.1f * 1.2f,		// MZ2_SOLDIER_BLASTER_8			98
	34.5f * 1.2f,	9.6f * 1.2f,	6.1f * 1.2f,		// MZ2_SOLDIER_SHOTGUN_8			99
	34.5f * 1.2f,	9.6f * 1.2f,	6.1f * 1.2f,		// MZ2_SOLDIER_MACHINEGUN_8			100

	// --- Xian shit below ---
	17.0f,			-19.5f,			62.9f,			// MZ2_MAKRON_BFG					101
	-3.6f,			-24.1f,			59.5f,			// MZ2_MAKRON_BLASTER_1				102
	-1.6f,			-19.3f,			59.5f,			// MZ2_MAKRON_BLASTER_2				103
	-0.1f,			-14.4f,			59.5f,			// MZ2_MAKRON_BLASTER_3				104
	2.0f,			-7.6f,			59.5f,			// MZ2_MAKRON_BLASTER_4				105
	3.4f,			1.3f,			59.5f,			// MZ2_MAKRON_BLASTER_5				106
	3.7f,			11.1f,			59.5f,			// MZ2_MAKRON_BLASTER_6				107
	-0.3f,			22.3f,			59.5f,			// MZ2_MAKRON_BLASTER_7				108
	-6.0f,			33.0f,			59.5f,			// MZ2_MAKRON_BLASTER_8				109
	-9.3f,			36.4f,			59.5f,			// MZ2_MAKRON_BLASTER_9				110
	-7.0f,			35.0f,			59.5f,			// MZ2_MAKRON_BLASTER_10			111
	-2.1f,			29.0f,			59.5f,			// MZ2_MAKRON_BLASTER_11			112
	3.9f,			17.3f,			59.5f,			// MZ2_MAKRON_BLASTER_12			113
	6.1f,			5.8f,			59.5f,			// MZ2_MAKRON_BLASTER_13			114
	5.9f,			-4.4f,			59.5f,			// MZ2_MAKRON_BLASTER_14			115
	4.2f,			-14.1f,			59.5f,			// MZ2_MAKRON_BLASTER_15			116
	2.4f,			-18.8f,			59.5f,			// MZ2_MAKRON_BLASTER_16			117
	-1.8f,			-25.5f,			59.5f,			// MZ2_MAKRON_BLASTER_17			118
	-17.3f,			7.8f,			72.4f,			// MZ2_MAKRON_RAILGUN_1				119

	78.5f,			-47.1f,			96.0f,				// MZ2_JORG_MACHINEGUN_L1			120
	78.5f,			-47.1f,			96.0f,				// MZ2_JORG_MACHINEGUN_L2			121
	78.5f,			-47.1f,			96.0f,				// MZ2_JORG_MACHINEGUN_L3			122
	78.5f,			-47.1f,			96.0f,				// MZ2_JORG_MACHINEGUN_L4			123
	78.5f,			-47.1f,			96.0f,				// MZ2_JORG_MACHINEGUN_L5			124
	78.5f,			-47.1f,			96.0f,				// MZ2_JORG_MACHINEGUN_L6			125
	78.5f,			46.7f,			96.0f,				// MZ2_JORG_MACHINEGUN_R1			126
	78.5f,			46.7f,			96.0f,				// MZ2_JORG_MACHINEGUN_R2			127
	78.5f,			46.7f,			96.0f,				// MZ2_JORG_MACHINEGUN_R3			128
	78.5f,			46.7f,			96.0f,				// MZ2_JORG_MACHINEGUN_R4			129
	78.5f,			46.7f,			96.0f,				// MZ2_JORG_MACHINEGUN_R5			130
	78.5f,			46.7f,			96.0f,				// MZ2_JORG_MACHINEGUN_R6			131
	6.3f,			-9.0f,			111.2f,			// MZ2_JORG_BFG_1					132

	32.0f,			40.0f,			70.0f,				// MZ2_BOSS2_MACHINEGUN_R1			73
	32.0f,			40.0f,			70.0f,				// MZ2_BOSS2_MACHINEGUN_R2			74
	32.0f,			40.0f,			70.0f,				// MZ2_BOSS2_MACHINEGUN_R3			75
	32.0f,			40.0f,			70.0f,				// MZ2_BOSS2_MACHINEGUN_R4			76
	32.0f,			40.0f,			70.0f,				// MZ2_BOSS2_MACHINEGUN_R5			77
	// --- End Xian Shit ---

	// ROGUE
	// - note that the above really ends at 137
	56.0f,				-32.0f,			32.0f,				// MZ2_CARRIER_MACHINEGUN_L1		138
	56.0f,				32.0f,			32.0f,				// MZ2_CARRIER_MACHINEGUN_R1		139
	42.0f,				24.0f,			50.0f,				// MZ2_CARRIER_GRENADE				140
	16.0f,				0.0f,			0.0f,				// MZ2_TURRET_MACHINEGUN			141
	16.0f,				0.0f,			0.0f,				// MZ2_TURRET_ROCKET				142
	16.0f,				0.0f,			0.0f,				// MZ2_TURRET_BLASTER				143
	24.0f,				0.0f,			6.0f,				// MZ2_STALKER_BLASTER				144
	32.5f,				-0.8f,			10.0f,			// MZ2_DAEDALUS_BLASTER				145
	12.1f,				5.4f,			16.5f,			// MZ2_MEDIC_BLASTER_2				146
	32.0f,				0.0f,			6.0f,				// MZ2_CARRIER_RAILGUN				147
	57.72f,				14.50f,			88.81f,			// MZ2_WIDOW_DISRUPTOR				148
	56.0f,				32.0f,			32.0f,				// MZ2_WIDOW_BLASTER				149
	62.0f,				-20.0f,			84.0f,				// MZ2_WIDOW_RAIL					150
	32.0f,				0.0f,			6.0f,				// MZ2_WIDOW_PLASMABEAM				151	PMM - not used!
	61.0f,				-32.0f,			12.0f,				// MZ2_CARRIER_MACHINEGUN_L2		152
	61.0f,				32.0f,			12.0f,				// MZ2_CARRIER_MACHINEGUN_R2		153
	17.0f,				-62.0f,			91.0f,				// MZ2_WIDOW_RAIL_LEFT				154
	68.0f,				12.0f,			86.0f,				// MZ2_WIDOW_RAIL_RIGHT				155
	47.5f,				56.0f,			89.0f,				// MZ2_WIDOW_BLASTER_SWEEP1			156	PMM - the sweeps need to be in sequential order
	54.0f,				52.0f,			91.0f,				// MZ2_WIDOW_BLASTER_SWEEP2			157
	58.0f,				40.0f,			91.0f,				// MZ2_WIDOW_BLASTER_SWEEP3			158
	68.0f,				30.0f,			88.0f,				// MZ2_WIDOW_BLASTER_SWEEP4			159
	74.0f,				20.0f,			88.0f,				// MZ2_WIDOW_BLASTER_SWEEP5			160
	73.0f,				11.0f,			87.0f,				// MZ2_WIDOW_BLASTER_SWEEP6			161
	73.0f,				3.0f,			87.0f,				// MZ2_WIDOW_BLASTER_SWEEP7			162
	70.0f,				-12.0f,			87.0f,				// MZ2_WIDOW_BLASTER_SWEEP8			163
	67.0f,				-20.0f,			90.0f,				// MZ2_WIDOW_BLASTER_SWEEP9			164

	-20.0f,				76.0f,			90.0f,				// MZ2_WIDOW_BLASTER_100			165
	-8.0f,				74.0f,			90.0f,				// MZ2_WIDOW_BLASTER_90				166
	0.0f,				72.0f,		90.0f,				// MZ2_WIDOW_BLASTER_80				167
	10.0f,				71.0f,			89.0f,				// MZ2_WIDOW_BLASTER_70				168		d06
	23.0f,				70.0f,			87.0f,				// MZ2_WIDOW_BLASTER_60				169		d07
	32.0f,				64.0f,			85.0f,				// MZ2_WIDOW_BLASTER_50				170		d08
	40.0f,				58.0f,			84.0f,				// MZ2_WIDOW_BLASTER_40				171
	48.0f,				50.0f,			83.0f,				// MZ2_WIDOW_BLASTER_30				172		d10
	54.0f,				42.0f,			82.0f,				// MZ2_WIDOW_BLASTER_20				173
	56.0f,				34.0f,			82.0f,				// MZ2_WIDOW_BLASTER_10				174		d12
	58.0f,				26.0f,			82.0f,				// MZ2_WIDOW_BLASTER_0				175
	60.0f,				16.0f,			82.0f,				// MZ2_WIDOW_BLASTER_10L			176		d14
	59.0f,				6.0f,			81.0f,				// MZ2_WIDOW_BLASTER_20L			177
	58.0f,				-2.0f,			80.0f,				// MZ2_WIDOW_BLASTER_30L			178		d16
	57.0f,				-10.0f,			79.0f,				// MZ2_WIDOW_BLASTER_40L			179
	54.0f,				-18.0f,			78.0f,				// MZ2_WIDOW_BLASTER_50L			180		d18
	42.0f,				-32.0f,			80.0f,				// MZ2_WIDOW_BLASTER_60L			181
	36.0f,				-40.0f,			78.0f,				// MZ2_WIDOW_BLASTER_70L			182		d20

	68.4f,				10.88f,			82.08f,			// MZ2_WIDOW_RUN_1				183
	68.51f,				8.64f,			85.14f,			// MZ2_WIDOW_RUN_2				184
	68.66f,				6.38f,			88.78f,			// MZ2_WIDOW_RUN_3				185
	68.73f,				5.1f,			84.47f,			// MZ2_WIDOW_RUN_4				186
	68.82f,				4.79f,			80.52f,			// MZ2_WIDOW_RUN_5				187
	68.77f,				6.11f,			85.37f,			// MZ2_WIDOW_RUN_6				188
	68.67f,				7.99f,			90.24f,			// MZ2_WIDOW_RUN_7				189
	68.55f,				9.54f,			87.36f,			// MZ2_WIDOW_RUN_8				190

	0.0f,				0.0f,			-5.0f,				// MZ2_CARRIER_ROCKET_1			191
	0.0f,				0.0f,			-5.0f,				// MZ2_CARRIER_ROCKET_2			192
	0.0f,				0.0f,			-5.0f,				// MZ2_CARRIER_ROCKET_3			193
	0.0f,				0.0f,			-5.0f,				// MZ2_CARRIER_ROCKET_4			194
	69.00f,				-17.63f,		93.77f,			// MZ2_WIDOW2_BEAMER_1			195
	69.00f,				-17.08f,		89.82f,			// MZ2_WIDOW2_BEAMER_2			196
	69.00f,				-18.40f,		90.70f,			// MZ2_WIDOW2_BEAMER_3			197
	69.00f,				-18.34f,		94.32f,			// MZ2_WIDOW2_BEAMER_4			198
	69.00f,				-18.30f,		97.98f,			// MZ2_WIDOW2_BEAMER_5			199
	45.04f,				-59.02f,		92.24f,			// MZ2_WIDOW2_BEAM_SWEEP_1		200
	50.68f,				-54.70f,		91.96f,			// MZ2_WIDOW2_BEAM_SWEEP_2		201
	56.57f,				-47.72f,		91.65f,			// MZ2_WIDOW2_BEAM_SWEEP_3		202
	61.75f,				-38.75f,		91.38f,			// MZ2_WIDOW2_BEAM_SWEEP_4		203
	65.55f,				-28.76f,		91.24f,			// MZ2_WIDOW2_BEAM_SWEEP_5		204
	67.79f,				-18.90f,		91.22f,			// MZ2_WIDOW2_BEAM_SWEEP_6		205
	68.60f,				-9.52f,			91.23f,			// MZ2_WIDOW2_BEAM_SWEEP_7		206
	68.08f,				0.18f,			91.32f,			// MZ2_WIDOW2_BEAM_SWEEP_8		207
	66.14f,				9.79f,			91.44f,			// MZ2_WIDOW2_BEAM_SWEEP_9		208
	62.77f,				18.91f,			91.65f,			// MZ2_WIDOW2_BEAM_SWEEP_10		209
	58.29f,				27.11f,			92.00f,			// MZ2_WIDOW2_BEAM_SWEEP_11		210

	0.0f,				0.0f,			0.0f				// end of table
};


