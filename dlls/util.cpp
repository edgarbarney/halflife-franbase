/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
/*

===== util.cpp ========================================================

  Utility code.  Really not optional after all.

*/

#include <algorithm>

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "saverestore.h"
#include <time.h>
#include "shake.h"
#include "decals.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"
#include "UserMessages.h"
#include "movewith.h"
#include "locus.h"

float UTIL_WeaponTimeBase()
{
#if defined(CLIENT_WEAPONS)
	return 0.0;
#else
	return gpGlobals->time;
#endif
}

CBaseEntity* UTIL_FindEntityForward(CBaseEntity* pMe)
{
	TraceResult tr;

	UTIL_MakeVectors(pMe->pev->v_angle);
	UTIL_TraceLine(pMe->pev->origin + pMe->pev->view_ofs, pMe->pev->origin + pMe->pev->view_ofs + gpGlobals->v_forward * 8192, dont_ignore_monsters, pMe->edict(), &tr);
	if (tr.flFraction != 1.0 && !FNullEnt(tr.pHit))
	{
		CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
		return pHit;
	}
	return nullptr;
}

static unsigned int glSeed = 0;

unsigned int seed_table[256] =
	{
		28985, 27138, 26457, 9451, 17764, 10909, 28790, 8716, 6361, 4853, 17798, 21977, 19643, 20662, 10834, 20103,
		27067, 28634, 18623, 25849, 8576, 26234, 23887, 18228, 32587, 4836, 3306, 1811, 3035, 24559, 18399, 315,
		26766, 907, 24102, 12370, 9674, 2972, 10472, 16492, 22683, 11529, 27968, 30406, 13213, 2319, 23620, 16823,
		10013, 23772, 21567, 1251, 19579, 20313, 18241, 30130, 8402, 20807, 27354, 7169, 21211, 17293, 5410, 19223,
		10255, 22480, 27388, 9946, 15628, 24389, 17308, 2370, 9530, 31683, 25927, 23567, 11694, 26397, 32602, 15031,
		18255, 17582, 1422, 28835, 23607, 12597, 20602, 10138, 5212, 1252, 10074, 23166, 19823, 31667, 5902, 24630,
		18948, 14330, 14950, 8939, 23540, 21311, 22428, 22391, 3583, 29004, 30498, 18714, 4278, 2437, 22430, 3439,
		28313, 23161, 25396, 13471, 19324, 15287, 2563, 18901, 13103, 16867, 9714, 14322, 15197, 26889, 19372, 26241,
		31925, 14640, 11497, 8941, 10056, 6451, 28656, 10737, 13874, 17356, 8281, 25937, 1661, 4850, 7448, 12744,
		21826, 5477, 10167, 16705, 26897, 8839, 30947, 27978, 27283, 24685, 32298, 3525, 12398, 28726, 9475, 10208,
		617, 13467, 22287, 2376, 6097, 26312, 2974, 9114, 21787, 28010, 4725, 15387, 3274, 10762, 31695, 17320,
		18324, 12441, 16801, 27376, 22464, 7500, 5666, 18144, 15314, 31914, 31627, 6495, 5226, 31203, 2331, 4668,
		12650, 18275, 351, 7268, 31319, 30119, 7600, 2905, 13826, 11343, 13053, 15583, 30055, 31093, 5067, 761,
		9685, 11070, 21369, 27155, 3663, 26542, 20169, 12161, 15411, 30401, 7580, 31784, 8985, 29367, 20989, 14203,
		29694, 21167, 10337, 1706, 28578, 887, 3373, 19477, 14382, 675, 7033, 15111, 26138, 12252, 30996, 21409,
		25678, 18555, 13256, 23316, 22407, 16727, 991, 9236, 5373, 29402, 6117, 15241, 27715, 19291, 19888, 19847};

unsigned int U_Random()
{
	glSeed *= 69069;
	glSeed += seed_table[glSeed & 0xff];

	return (++glSeed & 0x0fffffff);
}

void U_Srand(unsigned int seed)
{
	glSeed = seed_table[seed & 0xff];
}

/*
=====================
UTIL_SharedRandomLong
=====================
*/
int UTIL_SharedRandomLong(unsigned int seed, int low, int high)
{
	unsigned int range;

	U_Srand((int)seed + low + high);

	range = high - low + 1;
	if (0 == (range - 1))
	{
		return low;
	}
	else
	{
		int offset;
		int rnum;

		rnum = U_Random();

		offset = rnum % range;

		return (low + offset);
	}
}

/*
=====================
UTIL_SharedRandomFloat
=====================
*/
float UTIL_SharedRandomFloat(unsigned int seed, float low, float high)
{
	//
	unsigned int range;

	U_Srand((int)seed + *(int*)&low + *(int*)&high);

	U_Random();
	U_Random();

	range = high - low;
	if (0 == range)
	{
		return low;
	}
	else
	{
		int tensixrand;
		float offset;

		tensixrand = U_Random() & 65535;

		offset = (float)tensixrand / 65536.0;

		return (low + offset * range);
	}
}

void UTIL_ParametricRocket(entvars_t* pev, Vector vecOrigin, Vector vecAngles, edict_t* owner)
{
	pev->startpos = vecOrigin;
	// Trace out line to end pos
	TraceResult tr;
	UTIL_MakeVectors(vecAngles);
	UTIL_TraceLine(pev->startpos, pev->startpos + gpGlobals->v_forward * 8192, ignore_monsters, owner, &tr);
	pev->endpos = tr.vecEndPos;

	// Now compute how long it will take based on current velocity
	Vector vecTravel = pev->endpos - pev->startpos;
	float travelTime = 0.0;
	if (pev->velocity.Length() > 0)
	{
		travelTime = vecTravel.Length() / pev->velocity.Length();
	}
	pev->starttime = gpGlobals->time;
	pev->impacttime = gpGlobals->time + travelTime;
}

// Normal overrides
void UTIL_SetGroupTrace(int groupmask, int op)
{
	g_groupmask = groupmask;
	g_groupop = op;

	ENGINE_SETGROUPMASK(g_groupmask, g_groupop);
}

void UTIL_UnsetGroupTrace()
{
	g_groupmask = 0;
	g_groupop = 0;

	ENGINE_SETGROUPMASK(0, 0);
}

// Smart version, it'll clean itself up when it pops off stack
UTIL_GroupTrace::UTIL_GroupTrace(int groupmask, int op)
{
	m_oldgroupmask = g_groupmask;
	m_oldgroupop = g_groupop;

	g_groupmask = groupmask;
	g_groupop = op;

	ENGINE_SETGROUPMASK(g_groupmask, g_groupop);
}

UTIL_GroupTrace::~UTIL_GroupTrace()
{
	g_groupmask = m_oldgroupmask;
	g_groupop = m_oldgroupop;

	ENGINE_SETGROUPMASK(g_groupmask, g_groupop);
}

TYPEDESCRIPTION gEntvarsDescription[] =
	{
		DEFINE_ENTITY_FIELD(classname, FIELD_STRING),
		DEFINE_ENTITY_GLOBAL_FIELD(globalname, FIELD_STRING),

		DEFINE_ENTITY_FIELD(origin, FIELD_POSITION_VECTOR),
		DEFINE_ENTITY_FIELD(oldorigin, FIELD_POSITION_VECTOR),
		DEFINE_ENTITY_FIELD(velocity, FIELD_VECTOR),
		DEFINE_ENTITY_FIELD(basevelocity, FIELD_VECTOR),
		DEFINE_ENTITY_FIELD(movedir, FIELD_VECTOR),

		DEFINE_ENTITY_FIELD(angles, FIELD_VECTOR),
		DEFINE_ENTITY_FIELD(avelocity, FIELD_VECTOR),
		DEFINE_ENTITY_FIELD(punchangle, FIELD_VECTOR),
		DEFINE_ENTITY_FIELD(v_angle, FIELD_VECTOR),
		DEFINE_ENTITY_FIELD(fixangle, FIELD_FLOAT),
		DEFINE_ENTITY_FIELD(idealpitch, FIELD_FLOAT),
		DEFINE_ENTITY_FIELD(pitch_speed, FIELD_FLOAT),
		DEFINE_ENTITY_FIELD(ideal_yaw, FIELD_FLOAT),
		DEFINE_ENTITY_FIELD(yaw_speed, FIELD_FLOAT),

		DEFINE_ENTITY_FIELD(modelindex, FIELD_INTEGER),
		DEFINE_ENTITY_GLOBAL_FIELD(model, FIELD_MODELNAME),

		DEFINE_ENTITY_FIELD(viewmodel, FIELD_MODELNAME),
		DEFINE_ENTITY_FIELD(weaponmodel, FIELD_MODELNAME),

		DEFINE_ENTITY_FIELD(absmin, FIELD_POSITION_VECTOR),
		DEFINE_ENTITY_FIELD(absmax, FIELD_POSITION_VECTOR),
		DEFINE_ENTITY_GLOBAL_FIELD(mins, FIELD_VECTOR),
		DEFINE_ENTITY_GLOBAL_FIELD(maxs, FIELD_VECTOR),
		DEFINE_ENTITY_GLOBAL_FIELD(size, FIELD_VECTOR),

		DEFINE_ENTITY_FIELD(ltime, FIELD_TIME),
		DEFINE_ENTITY_FIELD(nextthink, FIELD_TIME),

		DEFINE_ENTITY_FIELD(solid, FIELD_INTEGER),
		DEFINE_ENTITY_FIELD(movetype, FIELD_INTEGER),

		DEFINE_ENTITY_FIELD(skin, FIELD_INTEGER),
		DEFINE_ENTITY_FIELD(body, FIELD_INTEGER),
		DEFINE_ENTITY_FIELD(effects, FIELD_INTEGER),

		DEFINE_ENTITY_FIELD(gravity, FIELD_FLOAT),
		DEFINE_ENTITY_FIELD(friction, FIELD_FLOAT),
		DEFINE_ENTITY_FIELD(light_level, FIELD_FLOAT),

		DEFINE_ENTITY_FIELD(frame, FIELD_FLOAT),
		DEFINE_ENTITY_FIELD(scale, FIELD_FLOAT),
		DEFINE_ENTITY_FIELD(sequence, FIELD_INTEGER),
		DEFINE_ENTITY_FIELD(animtime, FIELD_TIME),
		DEFINE_ENTITY_FIELD(framerate, FIELD_FLOAT),
		DEFINE_ENTITY_ARRAY(controller, FIELD_CHARACTER, NUM_ENT_CONTROLLERS),
		DEFINE_ENTITY_ARRAY(blending, FIELD_CHARACTER, NUM_ENT_BLENDERS),

		DEFINE_ENTITY_FIELD(rendermode, FIELD_INTEGER),
		DEFINE_ENTITY_FIELD(renderamt, FIELD_FLOAT),
		DEFINE_ENTITY_FIELD(rendercolor, FIELD_VECTOR),
		DEFINE_ENTITY_FIELD(renderfx, FIELD_INTEGER),

		DEFINE_ENTITY_FIELD(health, FIELD_FLOAT),
		DEFINE_ENTITY_FIELD(frags, FIELD_FLOAT),
		DEFINE_ENTITY_FIELD(weapons, FIELD_INTEGER),
		DEFINE_ENTITY_FIELD(takedamage, FIELD_FLOAT),

		DEFINE_ENTITY_FIELD(deadflag, FIELD_FLOAT),
		DEFINE_ENTITY_FIELD(view_ofs, FIELD_VECTOR),
		DEFINE_ENTITY_FIELD(button, FIELD_INTEGER),
		DEFINE_ENTITY_FIELD(impulse, FIELD_INTEGER),

		DEFINE_ENTITY_FIELD(chain, FIELD_EDICT),
		DEFINE_ENTITY_FIELD(dmg_inflictor, FIELD_EDICT),
		DEFINE_ENTITY_FIELD(enemy, FIELD_EDICT),
		DEFINE_ENTITY_FIELD(aiment, FIELD_EDICT),
		DEFINE_ENTITY_FIELD(owner, FIELD_EDICT),
		DEFINE_ENTITY_FIELD(groundentity, FIELD_EDICT),

		DEFINE_ENTITY_FIELD(spawnflags, FIELD_INTEGER),
		DEFINE_ENTITY_FIELD(flags, FIELD_FLOAT),

		DEFINE_ENTITY_FIELD(colormap, FIELD_INTEGER),
		DEFINE_ENTITY_FIELD(team, FIELD_INTEGER),

		DEFINE_ENTITY_FIELD(max_health, FIELD_FLOAT),
		DEFINE_ENTITY_FIELD(teleport_time, FIELD_TIME),
		DEFINE_ENTITY_FIELD(armortype, FIELD_FLOAT),
		DEFINE_ENTITY_FIELD(armorvalue, FIELD_FLOAT),
		DEFINE_ENTITY_FIELD(waterlevel, FIELD_INTEGER),
		DEFINE_ENTITY_FIELD(watertype, FIELD_INTEGER),

		// Having these fields be local to the individual levels makes it easier to test those levels individually.
		DEFINE_ENTITY_GLOBAL_FIELD(target, FIELD_STRING),
		DEFINE_ENTITY_GLOBAL_FIELD(targetname, FIELD_STRING),
		DEFINE_ENTITY_FIELD(netname, FIELD_STRING),
		DEFINE_ENTITY_FIELD(message, FIELD_STRING),

		DEFINE_ENTITY_FIELD(dmg_take, FIELD_FLOAT),
		DEFINE_ENTITY_FIELD(dmg_save, FIELD_FLOAT),
		DEFINE_ENTITY_FIELD(dmg, FIELD_FLOAT),
		DEFINE_ENTITY_FIELD(dmgtime, FIELD_TIME),

		DEFINE_ENTITY_FIELD(noise, FIELD_SOUNDNAME),
		DEFINE_ENTITY_FIELD(noise1, FIELD_SOUNDNAME),
		DEFINE_ENTITY_FIELD(noise2, FIELD_SOUNDNAME),
		DEFINE_ENTITY_FIELD(noise3, FIELD_SOUNDNAME),
		DEFINE_ENTITY_FIELD(speed, FIELD_FLOAT),
		DEFINE_ENTITY_FIELD(air_finished, FIELD_TIME),
		DEFINE_ENTITY_FIELD(pain_finished, FIELD_TIME),
		DEFINE_ENTITY_FIELD(radsuit_finished, FIELD_TIME),
};

#define ENTVARS_COUNT (sizeof(gEntvarsDescription) / sizeof(gEntvarsDescription[0]))

edict_t* UTIL_GetEntityList()
{
	return g_engfuncs.pfnPEntityOfEntOffset(0);
}

CBaseEntity* UTIL_GetLocalPlayer()
{
	return UTIL_PlayerByIndex(1);
}

#ifdef DEBUG
edict_t* DBG_EntOfVars(const entvars_t* pev)
{
	if (pev->pContainingEntity != nullptr)
		return pev->pContainingEntity;
	ALERT(at_debug, "entvars_t pContainingEntity is NULL, calling into engine");
	edict_t* pent = (*g_engfuncs.pfnFindEntityByVars)((entvars_t*)pev);
	if (pent == nullptr)
		ALERT(at_debug, "DAMN!  Even the engine couldn't FindEntityByVars!");
	((entvars_t*)pev)->pContainingEntity = pent;
	return pent;
}
#endif //DEBUG


#ifdef DEBUG
void DBG_AssertFunction(
	bool fExpr,
	const char* szExpr,
	const char* szFile,
	int szLine,
	const char* szMessage)
{
	if (fExpr)
		return;
	char szOut[512];
	if (szMessage != nullptr)
		sprintf(szOut, "ASSERT FAILED:\n %s \n(%s@%d)\n%s", szExpr, szFile, szLine, szMessage);
	else
		sprintf(szOut, "ASSERT FAILED:\n %s \n(%s@%d)", szExpr, szFile, szLine);
	ALERT(at_debug, szOut);
}
#endif // DEBUG

// ripped this out of the engine
float UTIL_AngleMod(float a)
{
	if (a < 0)
	{
		a = a + 360 * ((int)(a / 360) + 1);
	}
	else if (a >= 360)
	{
		a = a - 360 * ((int)(a / 360));
	}
	// a = (360.0/65536) * ((int)(a*(65536/360.0)) & 65535);
	return a;
}

float UTIL_AngleDiff(float destAngle, float srcAngle)
{
	float delta;

	delta = destAngle - srcAngle;
	if (destAngle > srcAngle)
	{
		if (delta >= 180)
			delta -= 360;
	}
	else
	{
		if (delta <= -180)
			delta += 360;
	}
	return delta;
}

Vector UTIL_VecToAngles(const Vector& vec)
{
	float rgflVecOut[3];
	VEC_TO_ANGLES(vec, rgflVecOut);
	return Vector(rgflVecOut);
}

//LRC - pass in a normalised axis vector and a number of degrees, and this returns the corresponding
// angles value for an entity.
Vector UTIL_AxisRotationToAngles(const Vector& vec, float angle)
{
	Vector vecTemp = UTIL_AxisRotationToVec(vec, angle);
	float rgflVecOut[3];
	//ugh, mathsy.
	rgflVecOut[0] = asin(vecTemp.z) * (-180.0 / M_PI);
	rgflVecOut[1] = acos(vecTemp.x) * (180.0 / M_PI);
	if (vecTemp.y < 0)
		rgflVecOut[1] = -rgflVecOut[1];
	rgflVecOut[2] = 0; //for now
	return Vector(rgflVecOut);
}

//LRC - as above, but returns the position of point 1 0 0 under the given rotation
Vector UTIL_AxisRotationToVec(const Vector& vec, float angle)
{
	float rgflVecOut[3];
	float flRads = angle * (M_PI / 180.0);
	float c = cos(flRads);
	float s = sin(flRads);
	float v = vec.x * (1 - c);
	//ugh, more maths. Thank goodness for internet geometry sites...
	rgflVecOut[0] = vec.x * v + c;
	rgflVecOut[1] = vec.y * v + vec.z * s;
	rgflVecOut[2] = vec.z * v - vec.y * s;
	return Vector(rgflVecOut);
}

//	float UTIL_MoveToOrigin( edict_t *pent, const Vector vecGoal, float flDist, int iMoveType )
void UTIL_MoveToOrigin(edict_t* pent, const Vector& vecGoal, float flDist, int iMoveType)
{
	float rgfl[3];
	vecGoal.CopyToArray(rgfl);
	//		return MOVE_TO_ORIGIN ( pent, rgfl, flDist, iMoveType );
	MOVE_TO_ORIGIN(pent, rgfl, flDist, iMoveType);
}


int UTIL_EntitiesInBox(CBaseEntity** pList, int listMax, const Vector& mins, const Vector& maxs, int flagMask)
{
	edict_t* pEdict = UTIL_GetEntityList();
	CBaseEntity* pEntity;
	int count;

	count = 0;

	if (pEdict == nullptr)
		return count;

	// Ignore world.
	++pEdict;

	for (int i = 1; i < gpGlobals->maxEntities; i++, pEdict++)
	{
		if (0 != pEdict->free) // Not in use
			continue;

		if (0 != flagMask && (pEdict->v.flags & flagMask) == 0) // Does it meet the criteria?
			continue;

		if (mins.x > pEdict->v.absmax.x ||
			mins.y > pEdict->v.absmax.y ||
			mins.z > pEdict->v.absmax.z ||
			maxs.x < pEdict->v.absmin.x ||
			maxs.y < pEdict->v.absmin.y ||
			maxs.z < pEdict->v.absmin.z)
			continue;

		pEntity = CBaseEntity::Instance(pEdict);
		if (pEntity == nullptr)
			continue;

		pList[count] = pEntity;
		count++;

		if (count >= listMax)
			return count;
	}

	return count;
}


int UTIL_MonstersInSphere(CBaseEntity** pList, int listMax, const Vector& center, float radius)
{
	edict_t* pEdict = UTIL_GetEntityList();
	CBaseEntity* pEntity;
	int count;
	float distance, delta;

	count = 0;
	float radiusSquared = radius * radius;

	if (pEdict == nullptr)
		return count;

	// Ignore world.
	++pEdict;

	for (int i = 1; i < gpGlobals->maxEntities; i++, pEdict++)
	{
		if (0 != pEdict->free) // Not in use
			continue;

		if ((pEdict->v.flags & (FL_CLIENT | FL_MONSTER)) == 0) // Not a client/monster ?
			continue;

		// Use origin for X & Y since they are centered for all monsters
		// Now X
		delta = center.x - pEdict->v.origin.x; //(pEdict->v.absmin.x + pEdict->v.absmax.x)*0.5;
		delta *= delta;

		if (delta > radiusSquared)
			continue;
		distance = delta;

		// Now Y
		delta = center.y - pEdict->v.origin.y; //(pEdict->v.absmin.y + pEdict->v.absmax.y)*0.5;
		delta *= delta;

		distance += delta;
		if (distance > radiusSquared)
			continue;

		// Now Z
		delta = center.z - (pEdict->v.absmin.z + pEdict->v.absmax.z) * 0.5;
		delta *= delta;

		distance += delta;
		if (distance > radiusSquared)
			continue;

		pEntity = CBaseEntity::Instance(pEdict);
		if (pEntity == nullptr)
			continue;

		pList[count] = pEntity;
		count++;

		if (count >= listMax)
			return count;
	}


	return count;
}


CBaseEntity* UTIL_FindEntityInSphere(CBaseEntity* pStartEntity, const Vector& vecCenter, float flRadius)
{
	edict_t* pentEntity;

	if (pStartEntity != nullptr)
		pentEntity = pStartEntity->edict();
	else
		pentEntity = nullptr;

	pentEntity = FIND_ENTITY_IN_SPHERE(pentEntity, vecCenter, flRadius);

	if (!FNullEnt(pentEntity))
		return CBaseEntity::Instance(pentEntity);
	return nullptr;
}


CBaseEntity* UTIL_FindEntityByString(CBaseEntity* pStartEntity, const char* szKeyword, const char* szValue)
{
	edict_t* pentEntity;
	CBaseEntity* pEntity;

	if (pStartEntity != nullptr)
		pentEntity = pStartEntity->edict();
	else
		pentEntity = nullptr;

	for (;;)
	{
		// Don't change this to use UTIL_FindEntityByString!
		pentEntity = FIND_ENTITY_BY_STRING(pentEntity, szKeyword, szValue);

		// if pentEntity (the edict) is null, we're at the end of the entities. Give up.
		if (FNullEnt(pentEntity))
		{
			return nullptr;
		}
		else
		{
			// ...but if only pEntity (the classptr) is null, we've just got one dud, so we try again.
			pEntity = CBaseEntity::Instance(pentEntity);
			if (pEntity != nullptr)
				return pEntity;
		}
	}
}

CBaseEntity* UTIL_FindEntityByClassname(CBaseEntity* pStartEntity, const char* szName)
{
	return UTIL_FindEntityByString(pStartEntity, "classname", szName);
}

#define MAX_ALIASNAME_LEN 80

//LRC - things get messed up if aliases change in the middle of an entity traversal.
// so instead, they record what changes should be made, and wait until this function gets
// called.
void UTIL_FlushAliases()
{
	//	ALERT(at_console, "Flushing alias list\n");
	if (g_pWorld == nullptr)
	{
		ALERT(at_debug, "FlushAliases has no AliasList!\n");
		return;
	}

	while (g_pWorld->m_pFirstAlias != nullptr)
	{
		if ((g_pWorld->m_pFirstAlias->m_iLFlags & LF_ALIASLIST) != 0)
		{
			//			ALERT(at_console, "call FlushChanges for %s \"%s\"\n", STRING(g_pWorld->m_pFirstAlias->pev->classname), STRING(g_pWorld->m_pFirstAlias->pev->targetname));
			g_pWorld->m_pFirstAlias->FlushChanges();
			g_pWorld->m_pFirstAlias->m_iLFlags &= ~LF_ALIASLIST;
		}
		g_pWorld->m_pFirstAlias = g_pWorld->m_pFirstAlias->m_pNextAlias;
	}
}

void UTIL_AddToAliasList(CBaseMutableAlias* pAlias)
{
	if (g_pWorld == nullptr)
	{
		ALERT(at_debug, "AddToAliasList has no AliasList!\n");
		return;
	}

	pAlias->m_iLFlags |= LF_ALIASLIST;

	//	ALERT(at_console, "Adding %s \"%s\" to alias list\n", STRING(pAlias->pev->classname), STRING(pAlias->pev->targetname));
	if (g_pWorld->m_pFirstAlias == nullptr)
	{
		g_pWorld->m_pFirstAlias = pAlias;
		pAlias->m_pNextAlias = nullptr;
	}
	else if (g_pWorld->m_pFirstAlias == pAlias)
	{
		// already in the list
		return;
	}
	else
	{
		CBaseMutableAlias* pCurrent = g_pWorld->m_pFirstAlias;
		while (pCurrent->m_pNextAlias != nullptr)
		{
			if (pCurrent->m_pNextAlias == pAlias)
			{
				// already in the list
				return;
			}
			pCurrent = pCurrent->m_pNextAlias;
		}
		pCurrent->m_pNextAlias = pAlias;
		pAlias->m_pNextAlias = nullptr;
	}
}

// for every alias which has the given name, find the earliest entity which any of them refers to
// and which is later than pStartEntity.
CBaseEntity* UTIL_FollowAliasReference(CBaseEntity* pStartEntity, const char* szValue)
{
	CBaseEntity* pEntity;
	CBaseEntity* pBestEntity = nullptr; // the entity we're currently planning to return.
	int iBestOffset = -1;			 // the offset of that entity.
	CBaseEntity* pTempEntity;
	int iTempOffset;

	pEntity = UTIL_FindEntityByTargetname(nullptr, szValue);

	while (pEntity != nullptr)
	{
		//LRC 1.8 - FollowAlias is now in CBaseEntity, no need to cast
		pTempEntity = pEntity->FollowAlias(pStartEntity);
		if (pTempEntity != nullptr)
		{
			// We've found an entity; only use it if its offset is lower than the offset we've currently got.
			iTempOffset = OFFSET(pTempEntity->pev);
			if (iBestOffset == -1 || iTempOffset < iBestOffset)
			{
				iBestOffset = iTempOffset;
				pBestEntity = pTempEntity;
			}
		}
		pEntity = UTIL_FindEntityByTargetname(pEntity, szValue);
	}

	return pBestEntity;
}

// for every info_group which has the given groupname, find the earliest entity which is referred to by its member
// with the given membername and which is later than pStartEntity.
CBaseEntity* UTIL_FollowGroupReference(CBaseEntity* pStartEntity, char* szGroupName, char* szMemberName)
{
	CBaseEntity* pEntity;
	CBaseEntity* pBestEntity = nullptr; // the entity we're currently planning to return.
	int iBestOffset = -1;			 // the offset of that entity.
	CBaseEntity* pTempEntity;
	int iTempOffset;
	char szBuf[MAX_ALIASNAME_LEN];
	char* szThisMember = szMemberName;
	char* szTail = nullptr;
	int iszMemberValue;
	int i;

	// find the first '.' in the membername and if there is one, split the string at that point.
	for (i = 0; szMemberName[i] != 0; i++)
	{
		if (szMemberName[i] == '.')
		{
			// recursive member-reference
			// FIXME: we should probably check that i < MAX_ALIASNAME_LEN.
			strncpy(szBuf, szMemberName, i);
			szBuf[i] = 0;
			szTail = &(szMemberName[i + 1]);
			szThisMember = szBuf;
			break;
		}
	}

	pEntity = UTIL_FindEntityByTargetname(nullptr, szGroupName);
	while (pEntity != nullptr)
	{
		if (FStrEq(STRING(pEntity->pev->classname), "info_group"))
		{
			iszMemberValue = ((CInfoGroup*)pEntity)->GetMember(szThisMember);
			//			ALERT(at_console,"survived getMember\n");
			//			return NULL;
			if (!FStringNull(iszMemberValue))
			{
				if (szTail != nullptr) // do we have more references to follow?
					pTempEntity = UTIL_FollowGroupReference(pStartEntity, (char*)STRING(iszMemberValue), szTail);
				else
					pTempEntity = UTIL_FindEntityByTargetname(pStartEntity, STRING(iszMemberValue));

				if (pTempEntity != nullptr)
				{
					iTempOffset = OFFSET(pTempEntity->pev);
					if (iBestOffset == -1 || iTempOffset < iBestOffset)
					{
						iBestOffset = iTempOffset;
						pBestEntity = pTempEntity;
					}
				}
			}
		}
		pEntity = UTIL_FindEntityByTargetname(pEntity, szGroupName);
	}

	if (pBestEntity != nullptr)
	{
		//		ALERT(at_console,"\"%s\".\"%s\" returns %s\n",szGroupName,szMemberName,STRING(pBestEntity->pev->targetname));
		return pBestEntity;
	}
	return nullptr;
}

// Returns the first entity which szName refers to and which is after pStartEntity.
CBaseEntity* UTIL_FollowReference(CBaseEntity* pStartEntity, const char* szName)
{
	char szRoot[MAX_ALIASNAME_LEN + 1]; // allow room for null-terminator
	char* szMember;
	int i;
	CBaseEntity* pResult;

	if ((szName == nullptr) || szName[0] == 0)
		return nullptr;

	// reference through an info_group?
	for (i = 0; szName[i] != 0; i++)
	{
		if (szName[i] == '.')
		{
			// yes, it looks like a reference through an info_group...
			// FIXME: we should probably check that i < MAX_ALIASNAME_LEN.
			strncpy(szRoot, szName, i);
			szRoot[i] = 0;
			szMember = (char*)&szName[i + 1];
			//ALERT(at_console,"Following reference- group %s with member %s\n",szRoot,szMember);
			pResult = UTIL_FollowGroupReference(pStartEntity, szRoot, szMember);
			//			if (pResult)
			//				ALERT(at_console,"\"%s\".\"%s\" = %s\n",szRoot,szMember,STRING(pResult->pev->targetname));
			return pResult;
		}
	}
	// reference through an info_alias?
	if (szName[0] == '*')
	{
		if (FStrEq(szName, "*player"))
		{
			CBaseEntity* pPlayer = UTIL_FindEntityByClassname(nullptr, "player");
			if ((pPlayer != nullptr) && (pStartEntity == nullptr || pPlayer->eoffset() > pStartEntity->eoffset()))
				return pPlayer;
			else
				return nullptr;
		}
		//ALERT(at_console,"Following alias %s\n",szName+1);
		pResult = UTIL_FollowAliasReference(pStartEntity, szName + 1);
		//		if (pResult)
		//			ALERT(at_console,"alias \"%s\" = %s\n",szName+1,STRING(pResult->pev->targetname));
		return pResult;
	}
	// not a reference
	//	ALERT(at_console,"%s is not a reference\n",szName);
	return nullptr;
}

CBaseEntity* UTIL_FindEntityByTargetname(CBaseEntity* pStartEntity, const char* szName)
{
	CBaseEntity* pFound = UTIL_FollowReference(pStartEntity, szName);
	if (pFound != nullptr)
		return pFound;
	else
		return UTIL_FindEntityByString(pStartEntity, "targetname", szName);
}

CBaseEntity* UTIL_FindEntityByTargetname(CBaseEntity* pStartEntity, const char* szName, CBaseEntity* pActivator)
{
	if (FStrEq(szName, "*locus"))
	{
		if ((pActivator != nullptr) && (pStartEntity == nullptr || pActivator->eoffset() > pStartEntity->eoffset()))
			return pActivator;
		else
			return nullptr;
	}
	else
		return UTIL_FindEntityByTargetname(pStartEntity, szName);
}

CBaseEntity* UTIL_FindEntityByTarget(CBaseEntity* pStartEntity, const char* szName)
{
	return UTIL_FindEntityByString(pStartEntity, "target", szName);
}

CBaseEntity* UTIL_FindEntityGeneric(const char* szWhatever, Vector& vecSrc, float flRadius)
{
	CBaseEntity* pEntity = nullptr;

	pEntity = UTIL_FindEntityByTargetname(nullptr, szWhatever);
	if (pEntity != nullptr)
		return pEntity;

	CBaseEntity* pSearch = nullptr;
	float flMaxDist2 = flRadius * flRadius;
	while ((pSearch = UTIL_FindEntityByClassname(pSearch, szWhatever)) != nullptr)
	{
		float flDist2 = (pSearch->pev->origin - vecSrc).Length();
		flDist2 = flDist2 * flDist2;
		if (flMaxDist2 > flDist2)
		{
			pEntity = pSearch;
			flMaxDist2 = flDist2;
		}
	}
	return pEntity;
}


// returns a CBaseEntity pointer to a player by index.  Only returns if the player is spawned and connected
// otherwise returns NULL
// Index is 1 based
CBaseEntity* UTIL_PlayerByIndex(int playerIndex)
{
	CBaseEntity* pPlayer = nullptr;

	if (playerIndex > 0 && playerIndex <= gpGlobals->maxClients)
	{
		edict_t* pPlayerEdict = INDEXENT(playerIndex);
		if ((pPlayerEdict != nullptr) && 0 == pPlayerEdict->free)
		{
			pPlayer = CBaseEntity::Instance(pPlayerEdict);
		}
	}

	return pPlayer;
}


void UTIL_MakeVectors(const Vector& vecAngles)
{
	MAKE_VECTORS(vecAngles);
}


void UTIL_MakeAimVectors(const Vector& vecAngles)
{
	float rgflVec[3];
	vecAngles.CopyToArray(rgflVec);
	rgflVec[0] = -rgflVec[0];
	MAKE_VECTORS(rgflVec);
}


#define SWAP(a, b, temp) ((temp) = (a), (a) = (b), (b) = (temp))

void UTIL_MakeInvVectors(const Vector& vec, globalvars_t* pgv)
{
	MAKE_VECTORS(vec);

	float tmp;
	pgv->v_right = pgv->v_right * -1;

	SWAP(pgv->v_forward.y, pgv->v_right.x, tmp);
	SWAP(pgv->v_forward.z, pgv->v_up.x, tmp);
	SWAP(pgv->v_right.z, pgv->v_up.y, tmp);
}


void UTIL_EmitAmbientSound(edict_t* entity, const Vector& vecOrigin, const char* samp, float vol, float attenuation, int fFlags, int pitch)
{
	float rgfl[3];
	vecOrigin.CopyToArray(rgfl);

	if ((samp != nullptr) && *samp == '!')
	{
		char name[32];
		if (SENTENCEG_Lookup(samp, name) >= 0)
			EMIT_AMBIENT_SOUND(entity, rgfl, name, vol, attenuation, fFlags, pitch);
	}
	else
		EMIT_AMBIENT_SOUND(entity, rgfl, samp, vol, attenuation, fFlags, pitch);
}

static unsigned short FixedUnsigned16(float value, float scale)
{
	int output;

	output = value * scale;
	if (output < 0)
		output = 0;
	if (output > 0xFFFF)
		output = 0xFFFF;

	return (unsigned short)output;
}

static short FixedSigned16(float value, float scale)
{
	int output;

	output = value * scale;

	if (output > 32767)
		output = 32767;

	if (output < -32768)
		output = -32768;

	return (short)output;
}

// Shake the screen of all clients within radius
// radius == 0, shake all clients
// UNDONE: Allow caller to shake clients not ONGROUND?
// UNDONE: Fix falloff model (disabled)?
// UNDONE: Affect user controls?
//LRC UNDONE: Work during trigger_camera?
void UTIL_ScreenShake(const Vector& center, float amplitude, float frequency, float duration, float radius)
{
	int i;
	float localAmplitude;
	ScreenShake shake;

	shake.duration = FixedUnsigned16(duration, 1 << 12);  // 4.12 fixed
	shake.frequency = FixedUnsigned16(frequency, 1 << 8); // 8.8 fixed

	for (i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBaseEntity* pPlayer = UTIL_PlayerByIndex(i);

		if ((pPlayer == nullptr) || (pPlayer->pev->flags & FL_ONGROUND) == 0) // Don't shake if not onground
			continue;

		localAmplitude = 0;

		if (radius <= 0)
			localAmplitude = amplitude;
		else
		{
			Vector delta = center - pPlayer->pev->origin;
			float distance = delta.Length();

			// Had to get rid of this falloff - it didn't work well
			if (distance < radius)
				localAmplitude = amplitude; //radius - distance;
		}
		if (0 != localAmplitude)
		{
			shake.amplitude = FixedUnsigned16(localAmplitude, 1 << 12); // 4.12 fixed

			MESSAGE_BEGIN(MSG_ONE, gmsgShake, nullptr, pPlayer->edict()); // use the magic #1 for "one client"

			WRITE_SHORT(shake.amplitude); // shake amount
			WRITE_SHORT(shake.duration);  // shake lasts this long
			WRITE_SHORT(shake.frequency); // shake noise frequency

			MESSAGE_END();
		}
	}
}



void UTIL_ScreenShakeAll(const Vector& center, float amplitude, float frequency, float duration)
{
	UTIL_ScreenShake(center, amplitude, frequency, duration, 0);
}


void UTIL_ScreenFadeBuild(ScreenFade& fade, const Vector& color, float fadeTime, float fadeHold, int alpha, int flags)
{
	fade.duration = FixedUnsigned16(fadeTime, 1 << 12); // 4.12 fixed
	fade.holdTime = FixedUnsigned16(fadeHold, 1 << 12); // 4.12 fixed
	fade.r = (int)color.x;
	fade.g = (int)color.y;
	fade.b = (int)color.z;
	fade.a = alpha;
	fade.fadeFlags = flags;
}


void UTIL_ScreenFadeWrite(const ScreenFade& fade, CBaseEntity* pEntity)
{
	if ((pEntity == nullptr) || !pEntity->IsNetClient())
		return;

	MESSAGE_BEGIN(MSG_ONE, gmsgFade, nullptr, pEntity->edict()); // use the magic #1 for "one client"

	WRITE_SHORT(fade.duration);	 // fade lasts this long
	WRITE_SHORT(fade.holdTime);	 // fade lasts this long
	WRITE_SHORT(fade.fadeFlags); // fade type (in / out)
	WRITE_BYTE(fade.r);			 // fade red
	WRITE_BYTE(fade.g);			 // fade green
	WRITE_BYTE(fade.b);			 // fade blue
	WRITE_BYTE(fade.a);			 // fade blue

	MESSAGE_END();
}


void UTIL_ScreenFadeAll(const Vector& color, float fadeTime, float fadeHold, int alpha, int flags)
{
	int i;
	ScreenFade fade;

	UTIL_ScreenFadeBuild(fade, color, fadeTime, fadeHold, alpha, flags);

	for (i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBaseEntity* pPlayer = UTIL_PlayerByIndex(i);
		UTIL_ScreenFadeWrite(fade, pPlayer);
	}
}


void UTIL_ScreenFade(CBaseEntity* pEntity, const Vector& color, float fadeTime, float fadeHold, int alpha, int flags)
{
	ScreenFade fade;

	UTIL_ScreenFadeBuild(fade, color, fadeTime, fadeHold, alpha, flags);
	UTIL_ScreenFadeWrite(fade, pEntity);
}


void UTIL_HudMessage(CBaseEntity* pEntity, const hudtextparms_t& textparms, const char* pMessage)
{
	if ((pEntity == nullptr) || !pEntity->IsNetClient())
		return;

	MESSAGE_BEGIN(MSG_ONE, SVC_TEMPENTITY, nullptr, pEntity->edict());
	WRITE_BYTE(TE_TEXTMESSAGE);
	WRITE_BYTE(textparms.channel & 0xFF);

	WRITE_SHORT(FixedSigned16(textparms.x, 1 << 13));
	WRITE_SHORT(FixedSigned16(textparms.y, 1 << 13));
	WRITE_BYTE(textparms.effect);

	WRITE_BYTE(textparms.r1);
	WRITE_BYTE(textparms.g1);
	WRITE_BYTE(textparms.b1);
	WRITE_BYTE(textparms.a1);

	WRITE_BYTE(textparms.r2);
	WRITE_BYTE(textparms.g2);
	WRITE_BYTE(textparms.b2);
	WRITE_BYTE(textparms.a2);

	WRITE_SHORT(FixedUnsigned16(textparms.fadeinTime, 1 << 8));
	WRITE_SHORT(FixedUnsigned16(textparms.fadeoutTime, 1 << 8));
	WRITE_SHORT(FixedUnsigned16(textparms.holdTime, 1 << 8));

	if (textparms.effect == 2)
		WRITE_SHORT(FixedUnsigned16(textparms.fxTime, 1 << 8));

	if (strlen(pMessage) < 512)
	{
		WRITE_STRING(pMessage);
	}
	else
	{
		char tmp[512];
		strncpy(tmp, pMessage, 511);
		tmp[511] = 0;
		WRITE_STRING(tmp);
	}
	MESSAGE_END();
}

void UTIL_HudMessageAll(const hudtextparms_t& textparms, const char* pMessage)
{
	int i;

	for (i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBaseEntity* pPlayer = UTIL_PlayerByIndex(i);
		if (pPlayer != nullptr)
			UTIL_HudMessage(pPlayer, textparms, pMessage);
	}
}

void UTIL_ClientPrintAll(int msg_dest, const char* msg_name, const char* param1, const char* param2, const char* param3, const char* param4)
{
	MESSAGE_BEGIN(MSG_ALL, gmsgTextMsg);
	WRITE_BYTE(msg_dest);
	WRITE_STRING(msg_name);

	if (param1 != nullptr)
		WRITE_STRING(param1);
	if (param2 != nullptr)
		WRITE_STRING(param2);
	if (param3 != nullptr)
		WRITE_STRING(param3);
	if (param4 != nullptr)
		WRITE_STRING(param4);

	MESSAGE_END();
}

void ClientPrint(entvars_t* client, int msg_dest, const char* msg_name, const char* param1, const char* param2, const char* param3, const char* param4)
{
	MESSAGE_BEGIN(MSG_ONE, gmsgTextMsg, nullptr, client);
	WRITE_BYTE(msg_dest);
	WRITE_STRING(msg_name);

	if (param1 != nullptr)
		WRITE_STRING(param1);
	if (param2 != nullptr)
		WRITE_STRING(param2);
	if (param3 != nullptr)
		WRITE_STRING(param3);
	if (param4 != nullptr)
		WRITE_STRING(param4);

	MESSAGE_END();
}

void UTIL_SayText(const char* pText, CBaseEntity* pEntity)
{
	if (!pEntity->IsNetClient())
		return;

	MESSAGE_BEGIN(MSG_ONE, gmsgSayText, nullptr, pEntity->edict());
	WRITE_BYTE(pEntity->entindex());
	WRITE_STRING(pText);
	MESSAGE_END();
}

void UTIL_SayTextAll(const char* pText, CBaseEntity* pEntity)
{
	MESSAGE_BEGIN(MSG_ALL, gmsgSayText, nullptr);
	WRITE_BYTE(pEntity->entindex());
	WRITE_STRING(pText);
	MESSAGE_END();
}


char* UTIL_dtos1(int d)
{
	static char buf[8];
	sprintf(buf, "%d", d);
	return buf;
}

char* UTIL_dtos2(int d)
{
	static char buf[8];
	sprintf(buf, "%d", d);
	return buf;
}

char* UTIL_dtos3(int d)
{
	static char buf[8];
	sprintf(buf, "%d", d);
	return buf;
}

char* UTIL_dtos4(int d)
{
	static char buf[8];
	sprintf(buf, "%d", d);
	return buf;
}

void UTIL_ShowMessage(const char* pString, CBaseEntity* pEntity)
{
	if ((pEntity == nullptr) || !pEntity->IsNetClient())
		return;

	MESSAGE_BEGIN(MSG_ONE, gmsgHudText, nullptr, pEntity->edict());
	WRITE_STRING(pString);
	MESSAGE_END();
}


void UTIL_ShowMessageAll(const char* pString)
{
	int i;

	// loop through all players

	for (i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBaseEntity* pPlayer = UTIL_PlayerByIndex(i);
		if (pPlayer != nullptr)
			UTIL_ShowMessage(pString, pPlayer);
	}
}

// Overloaded to add IGNORE_GLASS
void UTIL_TraceLine(const Vector& vecStart, const Vector& vecEnd, IGNORE_MONSTERS igmon, IGNORE_GLASS ignoreGlass, edict_t* pentIgnore, TraceResult* ptr)
{
	//TODO: define constants
	TRACE_LINE(vecStart, vecEnd, (igmon == ignore_monsters ? 1 : 0) | (ignore_glass == ignoreGlass ? 0x100 : 0), pentIgnore, ptr);
}


void UTIL_TraceLine(const Vector& vecStart, const Vector& vecEnd, IGNORE_MONSTERS igmon, edict_t* pentIgnore, TraceResult* ptr)
{
	TRACE_LINE(vecStart, vecEnd, (igmon == ignore_monsters ? 1 : 0), pentIgnore, ptr);
}


void UTIL_TraceHull(const Vector& vecStart, const Vector& vecEnd, IGNORE_MONSTERS igmon, int hullNumber, edict_t* pentIgnore, TraceResult* ptr)
{
	TRACE_HULL(vecStart, vecEnd, (igmon == ignore_monsters ? 1 : 0), hullNumber, pentIgnore, ptr);
}

void UTIL_TraceModel(const Vector& vecStart, const Vector& vecEnd, int hullNumber, edict_t* pentModel, TraceResult* ptr)
{
	g_engfuncs.pfnTraceModel(vecStart, vecEnd, hullNumber, pentModel, ptr);
}


TraceResult UTIL_GetGlobalTrace()
{
	TraceResult tr;

	tr.fAllSolid = gpGlobals->trace_allsolid;
	tr.fStartSolid = gpGlobals->trace_startsolid;
	tr.fInOpen = gpGlobals->trace_inopen;
	tr.fInWater = gpGlobals->trace_inwater;
	tr.flFraction = gpGlobals->trace_fraction;
	tr.flPlaneDist = gpGlobals->trace_plane_dist;
	tr.pHit = gpGlobals->trace_ent;
	tr.vecEndPos = gpGlobals->trace_endpos;
	tr.vecPlaneNormal = gpGlobals->trace_plane_normal;
	tr.iHitgroup = gpGlobals->trace_hitgroup;
	return tr;
}


void UTIL_SetSize(entvars_t* pev, const Vector& vecMin, const Vector& vecMax)
{
	SET_SIZE(ENT(pev), vecMin, vecMax);
}


float UTIL_VecToYaw(const Vector& vec)
{
	return VEC_TO_YAW(vec);
}

void UTIL_SetEdictOrigin(edict_t* pEdict, const Vector& vecOrigin)
{
	SET_ORIGIN(pEdict, vecOrigin);
}

// 'links' the entity into the world
void UTIL_SetOrigin(CBaseEntity* pEntity, const Vector& vecOrigin)
{
	SET_ORIGIN(ENT(pEntity->pev), vecOrigin);
}

void UTIL_ParticleEffect(const Vector& vecOrigin, const Vector& vecDirection, unsigned int ulColor, unsigned int ulCount)
{
	PARTICLE_EFFECT(vecOrigin, vecDirection, (float)ulColor, (float)ulCount);
}

float UTIL_Approach(float target, float value, float speed)
{
	float delta = target - value;

	if (delta > speed)
		value += speed;
	else if (delta < -speed)
		value -= speed;
	else
		value = target;

	return value;
}


float UTIL_ApproachAngle(float target, float value, float speed)
{
	target = UTIL_AngleMod(target);
	value = UTIL_AngleMod(target);

	float delta = target - value;

	// Speed is assumed to be positive
	if (speed < 0)
		speed = -speed;

	if (delta < -180)
		delta += 360;
	else if (delta > 180)
		delta -= 360;

	if (delta > speed)
		value += speed;
	else if (delta < -speed)
		value -= speed;
	else
		value = target;

	return value;
}


float UTIL_AngleDistance(float next, float cur)
{
	float delta = next - cur;

	// LRC- correct for deltas > 360
	while (delta < -180)
		delta += 360;
	while (delta > 180)
		delta -= 360;

	return delta;
}


float UTIL_SplineFraction(float value, float scale)
{
	value = scale * value;
	float valueSquared = value * value;

	// Nice little ease-in, ease-out spline-like curve
	return 3 * valueSquared - 2 * valueSquared * value;
}


char* UTIL_VarArgs(const char* format, ...)
{
	va_list argptr;
	static char string[1024];

	va_start(argptr, format);
	vsprintf(string, format, argptr);
	va_end(argptr);

	return string;
}

Vector UTIL_GetAimVector(edict_t* pent, float flSpeed)
{
	Vector tmp;
	GET_AIM_VECTOR(pent, flSpeed, tmp);
	return tmp;
}

bool UTIL_IsMasterTriggered(string_t iszMaster, CBaseEntity* pActivator)
{
	int i, j, found = 0;
	const char* szMaster;
	char szBuf[80];
	CBaseEntity* pMaster;
	int reverse = 0;


	if (!FStringNull(iszMaster))
	{
		//		ALERT(at_console, "IsMasterTriggered(%s, %s \"%s\")\n", STRING(iszMaster), STRING(pActivator->pev->classname), STRING(pActivator->pev->targetname));
		szMaster = STRING(iszMaster);
		if (szMaster[0] == '~') //inverse master
		{
			reverse = 1;
			szMaster++;
		}

		pMaster = UTIL_FindEntityByTargetname(nullptr, szMaster);
		if (pMaster == nullptr)
		{
			for (i = 0; szMaster[i] != 0; i++)
			{
				if (szMaster[i] == '(')
				{
					for (j = i + 1; szMaster[j] != 0; j++)
					{
						if (szMaster[j] == ')')
						{
							strncpy(szBuf, szMaster + i + 1, (j - i) - 1);
							szBuf[(j - i) - 1] = 0;
							pActivator = UTIL_FindEntityByTargetname(nullptr, szBuf);
							found = 1;
							break;
						}
					}
					if (found == 0) // no ) found
					{
						ALERT(at_error, "Missing ')' in master \"%s\"\n", szMaster);
						return false;
					}
					break;
				}
			}
			if (found == 0) // no ( found
			{
				ALERT(at_debug, "Master \"%s\" not found!\n", szMaster);
				return true;
			}

			strncpy(szBuf, szMaster, i);
			szBuf[i] = 0;
			pMaster = UTIL_FindEntityByTargetname(nullptr, szBuf);
		}

		if (pMaster != nullptr)
		{
			if (reverse != 0)
				return (pMaster->GetState(pActivator) != STATE_ON);
			else
				return (pMaster->GetState(pActivator) == STATE_ON);
		}
	}

	// if the entity has no master (or the master is missing), just say yes.
	return true;
}

bool UTIL_ShouldShowBlood(int color)
{
	if (color != DONT_BLEED)
	{
		if (color == BLOOD_COLOR_RED)
		{
			if (CVAR_GET_FLOAT("violence_hblood") != 0)
				return true;
		}
		else
		{
			if (CVAR_GET_FLOAT("violence_ablood") != 0)
				return true;
		}
	}
	return false;
}

int UTIL_PointContents(const Vector& vec)
{
	return POINT_CONTENTS(vec);
}

void UTIL_BloodStream(const Vector& origin, const Vector& direction, int color, int amount)
{
	if (!UTIL_ShouldShowBlood(color))
		return;

	if (g_Language == LANGUAGE_GERMAN && color == BLOOD_COLOR_RED)
		color = 0;

	// RENDERERS START
	if (CVAR_GET_FLOAT("te_particles") >= 1)
	{
		if (color == BLOOD_COLOR_RED)
			UTIL_Particle("blood_effects_cluster.txt", origin, direction, 1);
		else
			UTIL_Particle("blood_effects_cluster_alien.txt", origin, direction, 1);
	}
	else
	{
		MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, origin);
		WRITE_BYTE(TE_BLOODSTREAM);
		WRITE_COORD(origin.x);
		WRITE_COORD(origin.y);
		WRITE_COORD(origin.z);
		WRITE_COORD(direction.x);
		WRITE_COORD(direction.y);
		WRITE_COORD(direction.z);
		WRITE_BYTE(color);
		WRITE_BYTE(V_min(amount, 255));
		MESSAGE_END();
		// RENDERERS END
	}
}

void UTIL_BloodDrips(const Vector& origin, const Vector& direction, int color, int amount)
{
	if (!UTIL_ShouldShowBlood(color))
		return;

	if (color == DONT_BLEED || amount == 0)
		return;

	if (g_Language == LANGUAGE_GERMAN && color == BLOOD_COLOR_RED)
		color = 0;

	if (g_pGameRules->IsMultiplayer())
	{
		// scale up blood effect in multiplayer for better visibility
		amount *= 2;
	}

	if (amount > 255)
		amount = 255;

//RENDERERS START
	if (CVAR_GET_FLOAT("te_particles") >= 1)
	{
		if (color == BLOOD_COLOR_RED)
			UTIL_Particle("blood_effects_cluster.txt", origin, direction, 1);
		else
			UTIL_Particle("blood_effects_cluster_alien.txt", origin, direction, 1);
	}
	else
	{
		MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, origin);
		WRITE_BYTE(TE_BLOODSTREAM);
		WRITE_COORD(origin.x);
		WRITE_COORD(origin.y);
		WRITE_COORD(origin.z);
		WRITE_COORD(direction.x);
		WRITE_COORD(direction.y);
		WRITE_COORD(direction.z);
		WRITE_BYTE(color);
		WRITE_BYTE(std::min(amount, 255));
		MESSAGE_END();
	}
//RENDERERS END
}	

Vector UTIL_RandomBloodVector()
{
	Vector direction;

	direction.x = RANDOM_FLOAT(-1, 1);
	direction.y = RANDOM_FLOAT(-1, 1);
	direction.z = RANDOM_FLOAT(0, 1);

	return direction;
}


void UTIL_BloodDecalTrace(TraceResult* pTrace, int bloodColor)
{
//RENDERERS START
	if (UTIL_ShouldShowBlood(bloodColor))
	{
		if (VARS(pTrace->pHit)->rendermode != kRenderTransAlpha)
		{
			if (bloodColor == BLOOD_COLOR_RED)
			{
				UTIL_CustomDecal(pTrace, "redblood");
			}
			else
			{
				UTIL_CustomDecal(pTrace, "yellowblood");
			}
		}
	}
//RENDERERS END
}


void UTIL_DecalTrace(TraceResult* pTrace, int decalNumber)
{
	short entityIndex;
	int index;
	int message;

	if (decalNumber < 0)
		return;

	index = gDecals[decalNumber].index;

	if (index < 0)
		return;

	if (pTrace->flFraction == 1.0)
		return;

	// Only decal BSP models
	if (pTrace->pHit != nullptr)
	{
		CBaseEntity* pEntity = CBaseEntity::Instance(pTrace->pHit);
		if ((pEntity != nullptr) && !pEntity->IsBSPModel())
			return;
		entityIndex = ENTINDEX(pTrace->pHit);
	}
	else
		entityIndex = 0;

	message = TE_DECAL;
	if (entityIndex != 0)
	{
		if (index > 255)
		{
			message = TE_DECALHIGH;
			index -= 256;
		}
	}
	else
	{
		message = TE_WORLDDECAL;
		if (index > 255)
		{
			message = TE_WORLDDECALHIGH;
			index -= 256;
		}
	}

	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(message);
	WRITE_COORD(pTrace->vecEndPos.x);
	WRITE_COORD(pTrace->vecEndPos.y);
	WRITE_COORD(pTrace->vecEndPos.z);
	WRITE_BYTE(index);
	if (0 != entityIndex)
		WRITE_SHORT(entityIndex);
	MESSAGE_END();
}

/*
==============
UTIL_PlayerDecalTrace

A player is trying to apply his custom decal for the spray can.
Tell connected clients to display it, or use the default spray can decal
if the custom can't be loaded.
==============
*/
void UTIL_PlayerDecalTrace(TraceResult* pTrace, int playernum, int decalNumber, bool bIsCustom)
{
	int index;

	if (!bIsCustom)
	{
		if (decalNumber < 0)
			return;

		index = gDecals[decalNumber].index;
		if (index < 0)
			return;
	}
	else
		index = decalNumber;

	if (pTrace->flFraction == 1.0)
		return;

	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_PLAYERDECAL);
	WRITE_BYTE(playernum);
	WRITE_COORD(pTrace->vecEndPos.x);
	WRITE_COORD(pTrace->vecEndPos.y);
	WRITE_COORD(pTrace->vecEndPos.z);
	WRITE_SHORT((short)ENTINDEX(pTrace->pHit));
	WRITE_BYTE(index);
	MESSAGE_END();
}

void UTIL_GunshotDecalTrace(TraceResult* pTrace, int decalNumber)
{
	if (decalNumber < 0)
		return;

	int index = gDecals[decalNumber].index;
	if (index < 0)
		return;

	if (pTrace->flFraction == 1.0)
		return;

	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, pTrace->vecEndPos);
	WRITE_BYTE(TE_GUNSHOTDECAL);
	WRITE_COORD(pTrace->vecEndPos.x);
	WRITE_COORD(pTrace->vecEndPos.y);
	WRITE_COORD(pTrace->vecEndPos.z);
	WRITE_SHORT((short)ENTINDEX(pTrace->pHit));
	WRITE_BYTE(index);
	MESSAGE_END();
}


void UTIL_Sparks(const Vector& position)
{
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, position);
	WRITE_BYTE(TE_SPARKS);
	WRITE_COORD(position.x);
	WRITE_COORD(position.y);
	WRITE_COORD(position.z);
	MESSAGE_END();
}


void UTIL_Ricochet(const Vector& position, float scale)
{
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, position);
	WRITE_BYTE(TE_ARMOR_RICOCHET);
	WRITE_COORD(position.x);
	WRITE_COORD(position.y);
	WRITE_COORD(position.z);
	WRITE_BYTE((int)(scale * 10));
	MESSAGE_END();
}


bool UTIL_TeamsMatch(const char* pTeamName1, const char* pTeamName2)
{
	// Everyone matches unless it's teamplay
	if (!g_pGameRules->IsTeamplay())
		return true;

	// Both on a team?
	if (*pTeamName1 != 0 && *pTeamName2 != 0)
	{
		if (!stricmp(pTeamName1, pTeamName2)) // Same Team?
			return true;
	}

	return false;
}

//LRC - moved here from barney.cpp
bool UTIL_IsFacing(entvars_t* pevTest, const Vector& reference)
{
	Vector vecDir = (reference - pevTest->origin);
	vecDir.z = 0;
	vecDir = vecDir.Normalize();
	Vector forward, angle;
	angle = pevTest->v_angle;
	angle.x = 0;
	UTIL_MakeVectorsPrivate(angle, forward, nullptr, nullptr);
	// He's facing me, he meant it
	if (DotProduct(forward, vecDir) > 0.96) // +/- 15 degrees or so
	{
		return true;
	}
	return false;
}

void UTIL_StringToVector(float* pVector, const char* pString)
{
	char *pstr, *pfront, tempString[128];
	int j;

	strncpy(tempString, pString, sizeof(tempString));
	tempString[sizeof(tempString) - 1] = '\0';
	pstr = pfront = tempString;

	for (j = 0; j < 3; j++) // lifted from pr_edict.c
	{
		pVector[j] = atof(pfront);

		while ('\0' != *pstr && *pstr != ' ')
			pstr++;
		if ('\0' == *pstr)
			break;
		pstr++;
		pfront = pstr;
	}
	if (j < 2)
	{
		/*
		ALERT( at_error, "Bad field in entity!! %s:%s == \"%s\"\n",
			pkvd->szClassName, pkvd->szKeyName, pkvd->szValue );
		*/
		for (j = j + 1; j < 3; j++)
			pVector[j] = 0;
	}
}


//LRC - randomized vectors of the form "0 0 0 .. 1 0 0"
void UTIL_StringToRandomVector(float* pVector, const char* pString)
{
	char *pstr, *pfront, tempString[128];
	int j;
	float pAltVec[3];

	strcpy(tempString, pString);
	pstr = pfront = tempString;

	for (j = 0; j < 3; j++) // lifted from pr_edict.c
	{
		pVector[j] = atof(pfront);

		while ((*pstr != 0) && *pstr != ' ')
			pstr++;
		if (*pstr == 0)
			break;
		pstr++;
		pfront = pstr;
	}
	if (j < 2)
	{
		/*
		ALERT( at_error, "Bad field in entity!! %s:%s == \"%s\"\n",
			pkvd->szClassName, pkvd->szKeyName, pkvd->szValue );
		*/
		for (j = j + 1; j < 3; j++)
			pVector[j] = 0;
	}
	else if (*pstr == '.')
	{
		pstr++;
		if (*pstr != '.')
			return;
		pstr++;
		if (*pstr != ' ')
			return;

		UTIL_StringToVector(pAltVec, pstr);

		pVector[0] = RANDOM_FLOAT(pVector[0], pAltVec[0]);
		pVector[1] = RANDOM_FLOAT(pVector[1], pAltVec[1]);
		pVector[2] = RANDOM_FLOAT(pVector[2], pAltVec[2]);
	}
}


void UTIL_StringToIntArray(int* pVector, int count, const char* pString)
{
	char *pstr, *pfront, tempString[128];
	int j;

	strncpy(tempString, pString, sizeof(tempString));
	tempString[sizeof(tempString) - 1] = '\0';
	pstr = pfront = tempString;

	for (j = 0; j < count; j++) // lifted from pr_edict.c
	{
		pVector[j] = atoi(pfront);

		while ('\0' != *pstr && *pstr != ' ')
			pstr++;
		if ('\0' == *pstr)
			break;
		pstr++;
		pfront = pstr;
	}

	for (j++; j < count; j++)
	{
		pVector[j] = 0;
	}
}

Vector UTIL_ClampVectorToBox(const Vector& input, const Vector& clampSize)
{
	Vector sourceVector = input;

	if (sourceVector.x > clampSize.x)
		sourceVector.x -= clampSize.x;
	else if (sourceVector.x < -clampSize.x)
		sourceVector.x += clampSize.x;
	else
		sourceVector.x = 0;

	if (sourceVector.y > clampSize.y)
		sourceVector.y -= clampSize.y;
	else if (sourceVector.y < -clampSize.y)
		sourceVector.y += clampSize.y;
	else
		sourceVector.y = 0;

	if (sourceVector.z > clampSize.z)
		sourceVector.z -= clampSize.z;
	else if (sourceVector.z < -clampSize.z)
		sourceVector.z += clampSize.z;
	else
		sourceVector.z = 0;

	return sourceVector.Normalize();
}


float UTIL_WaterLevel(const Vector& position, float minz, float maxz)
{
	Vector midUp = position;
	midUp.z = minz;

	if (UTIL_PointContents(midUp) != CONTENTS_WATER)
		return minz;

	midUp.z = maxz;
	if (UTIL_PointContents(midUp) == CONTENTS_WATER)
		return maxz;

	float diff = maxz - minz;
	while (diff > 1.0)
	{
		midUp.z = minz + diff / 2.0;
		if (UTIL_PointContents(midUp) == CONTENTS_WATER)
		{
			minz = midUp.z;
		}
		else
		{
			maxz = midUp.z;
		}
		diff = maxz - minz;
	}

	return midUp.z;
}

void UTIL_Bubbles(Vector mins, Vector maxs, int count)
{
	Vector mid = (mins + maxs) * 0.5;

	float flHeight = UTIL_WaterLevel(mid, mid.z, mid.z + 1024);
	flHeight = flHeight - mins.z;

	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, mid);
	WRITE_BYTE(TE_BUBBLES);
	WRITE_COORD(mins.x); // mins
	WRITE_COORD(mins.y);
	WRITE_COORD(mins.z);
	WRITE_COORD(maxs.x); // maxz
	WRITE_COORD(maxs.y);
	WRITE_COORD(maxs.z);
	WRITE_COORD(flHeight); // height
	WRITE_SHORT(g_sModelIndexBubbles);
	WRITE_BYTE(count); // count
	WRITE_COORD(8);	   // speed
	MESSAGE_END();
}

void UTIL_BubbleTrail(Vector from, Vector to, int count)
{
	float flHeight = UTIL_WaterLevel(from, from.z, from.z + 256);
	flHeight = flHeight - from.z;

	if (flHeight < 8)
	{
		flHeight = UTIL_WaterLevel(to, to.z, to.z + 256);
		flHeight = flHeight - to.z;
		if (flHeight < 8)
			return;

		// UNDONE: do a ploink sound
		flHeight = flHeight + to.z - from.z;
	}

	if (count > 255)
		count = 255;

	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_BUBBLETRAIL);
	WRITE_COORD(from.x); // mins
	WRITE_COORD(from.y);
	WRITE_COORD(from.z);
	WRITE_COORD(to.x); // maxz
	WRITE_COORD(to.y);
	WRITE_COORD(to.z);
	WRITE_COORD(flHeight); // height
	WRITE_SHORT(g_sModelIndexBubbles);
	WRITE_BYTE(count); // count
	WRITE_COORD(8);	   // speed
	MESSAGE_END();
}


void UTIL_Remove(CBaseEntity* pEntity)
{
	if (pEntity == nullptr)
		return;

//RENDERERS START
	if (gmsgFreeEnt != 0)
	{
		MESSAGE_BEGIN(MSG_ALL, gmsgFreeEnt);
		WRITE_SHORT(pEntity->entindex());
		MESSAGE_END();
	}
//RENDERERS END

	pEntity->UpdateOnRemove();
	pEntity->pev->flags |= FL_KILLME;
	pEntity->pev->targetname = 0;
}


bool UTIL_IsValidEntity(edict_t* pent)
{
	if ((pent == nullptr) || 0 != pent->free || (pent->v.flags & FL_KILLME) != 0)
		return false;
	return true;
}

void UTIL_PrecacheOther(const char* szClassname)
{
	edict_t* pent;

	pent = CREATE_NAMED_ENTITY(MAKE_STRING(szClassname));
	if (FNullEnt(pent))
	{
		ALERT(at_debug, "NULL Ent in UTIL_PrecacheOther\n");
		return;
	}

	CBaseEntity* pEntity = CBaseEntity::Instance(VARS(pent));
	if (pEntity != nullptr)
		pEntity->Precache();
	REMOVE_ENTITY(pent);
}

//=========================================================
// UTIL_LogPrintf - Prints a logged message to console.
// Preceded by LOG: ( timestamp ) < message >
//=========================================================
void UTIL_LogPrintf(const char* fmt, ...)
{
	va_list argptr;
	static char string[1024];

	va_start(argptr, fmt);
	vsprintf(string, fmt, argptr);
	va_end(argptr);

	// Print to server console
	ALERT(at_logged, "%s", string);
}

//=========================================================
// UTIL_DotPoints - returns the dot product of a line from
// src to check and vecdir.
//=========================================================
float UTIL_DotPoints(const Vector& vecSrc, const Vector& vecCheck, const Vector& vecDir)
{
	Vector2D vec2LOS;

	vec2LOS = (vecCheck - vecSrc).Make2D();
	vec2LOS = vec2LOS.Normalize();

	return DotProduct(vec2LOS, (vecDir.Make2D()));
}


//=========================================================
// UTIL_StripToken - for redundant keynames
//=========================================================
void UTIL_StripToken(const char* pKey, char* pDest, int nLen)
{
	int i = 0;

	while (i < nLen - 1 && '\0' != pKey[i] && pKey[i] != '#')
	{
		pDest[i] = pKey[i];
		i++;
	}
	pDest[i] = 0;
}


const char* GetStringForUseType(USE_TYPE useType)
{
	switch (useType)
	{
	case USE_ON:
		return "USE_ON";
	case USE_OFF:
		return "USE_OFF";
	case USE_TOGGLE:
		return "USE_TOGGLE";
	case USE_KILL:
		return "USE_KILL";
	case USE_NOT:
		return "USE_NOT";
	default:
		return "USE_UNKNOWN!?";
	}
}

const char* GetStringForState(STATE state)
{
	switch (state)
	{
	case STATE_ON:
		return "ON";
	case STATE_OFF:
		return "OFF";
	case STATE_TURN_ON:
		return "TURN ON";
	case STATE_TURN_OFF:
		return "TURN OFF";
	case STATE_IN_USE:
		return "IN USE";
	default:
		return "STATE_UNKNOWN!?";
	}
}


// --------------------------------------------------------------
//
// CSave
//
// --------------------------------------------------------------
static int gSizes[FIELD_TYPECOUNT] =
	{
		sizeof(float),	   // FIELD_FLOAT
		sizeof(int),	   // FIELD_STRING
		sizeof(int),	   // FIELD_ENTITY
		sizeof(int),	   // FIELD_CLASSPTR
		sizeof(EHANDLE),   // FIELD_EHANDLE
		sizeof(int),	   // FIELD_entvars_t
		sizeof(int),	   // FIELD_EDICT
		sizeof(float) * 3, // FIELD_VECTOR
		sizeof(float) * 3, // FIELD_POSITION_VECTOR
		sizeof(int*),	   // FIELD_POINTER
		sizeof(int),	   // FIELD_INTEGER
#ifdef GNUC
		sizeof(int*) * 2, // FIELD_FUNCTION
#else
		sizeof(int*), // FIELD_FUNCTION
#endif
		sizeof(byte),		   // FIELD_BOOLEAN
		sizeof(short),		   // FIELD_SHORT
		sizeof(char),		   // FIELD_CHARACTER
		sizeof(float),		   // FIELD_TIME
		sizeof(int),		   // FIELD_MODELNAME
		sizeof(int),		   // FIELD_SOUNDNAME
		sizeof(std::uint64_t), //FIELD_INT64
};


// Base class includes common SAVERESTOREDATA pointer, and manages the entity table
CSaveRestoreBuffer::CSaveRestoreBuffer(SAVERESTOREDATA& data)
	: m_data(data)
{
}


CSaveRestoreBuffer::~CSaveRestoreBuffer() = default;

int CSaveRestoreBuffer::EntityIndex(CBaseEntity* pEntity)
{
	if (pEntity == nullptr)
		return -1;
	return EntityIndex(pEntity->pev);
}


int CSaveRestoreBuffer::EntityIndex(entvars_t* pevLookup)
{
	if (pevLookup == nullptr)
		return -1;
	return EntityIndex(ENT(pevLookup));
}

int CSaveRestoreBuffer::EntityIndex(EOFFSET eoLookup)
{
	return EntityIndex(ENT(eoLookup));
}


int CSaveRestoreBuffer::EntityIndex(edict_t* pentLookup)
{
	if (pentLookup == nullptr)
		return -1;

	int i;
	ENTITYTABLE* pTable;

	for (i = 0; i < m_data.tableCount; i++)
	{
		pTable = m_data.pTable + i;
		if (pTable->pent == pentLookup)
			return i;
	}
	return -1;
}


edict_t* CSaveRestoreBuffer::EntityFromIndex(int entityIndex)
{
	if (entityIndex < 0)
		return nullptr;

	int i;
	ENTITYTABLE* pTable;

	for (i = 0; i < m_data.tableCount; i++)
	{
		pTable = m_data.pTable + i;
		if (pTable->id == entityIndex)
			return pTable->pent;
	}
	return nullptr;
}


int CSaveRestoreBuffer::EntityFlagsSet(int entityIndex, int flags)
{
	if (entityIndex < 0)
		return 0;
	if (entityIndex > m_data.tableCount)
		return 0;

	m_data.pTable[entityIndex].flags |= flags;

	return m_data.pTable[entityIndex].flags;
}


void CSaveRestoreBuffer::BufferRewind(int size)
{
	if (m_data.size < size)
		size = m_data.size;

	m_data.pCurrentData -= size;
	m_data.size -= size;
}

#ifndef WIN32
extern "C" {
unsigned _rotr(unsigned val, int shift)
{
	unsigned lobit;	 /* non-zero means lo bit set */
	unsigned num = val; /* number to rotate */

	shift &= 0x1f; /* modulo 32 -- this will also make
										   negative shifts work */

	while (shift--)
	{
		lobit = num & 1; /* get high bit */
		num >>= 1;		 /* shift right one bit */
		if (lobit)
			num |= 0x80000000; /* set hi bit if lo bit was set */
	}

	return num;
}
}
#endif

unsigned int CSaveRestoreBuffer::HashString(const char* pszToken)
{
	unsigned int hash = 0;

	while ('\0' != *pszToken)
		hash = _rotr(hash, 4) ^ *pszToken++;

	return hash;
}

unsigned short CSaveRestoreBuffer::TokenHash(const char* pszToken)
{
#if _DEBUG
	static int tokensparsed = 0;
	tokensparsed++;
#endif
	if (0 == m_data.tokenCount || nullptr == m_data.pTokens)
	{
		//if we're here it means trigger_changelevel is trying to actually save something when it's not supposed to.
		ALERT(at_error, "No token table array in TokenHash()!\n");
		return 0;
	}

	const unsigned short hash = (unsigned short)(HashString(pszToken) % (unsigned)m_data.tokenCount);

	for (int i = 0; i < m_data.tokenCount; i++)
	{
#if _DEBUG
		static bool beentheredonethat = false;
		if (i > 50 && !beentheredonethat)
		{
			beentheredonethat = true;
			ALERT(at_error, "CSaveRestoreBuffer::TokenHash() is getting too full!\n");
		}
#endif

		int index = hash + i;
		if (index >= m_data.tokenCount)
			index -= m_data.tokenCount;

		if ((m_data.pTokens[index] == nullptr) || strcmp(pszToken, m_data.pTokens[index]) == 0)
		{
			m_data.pTokens[index] = (char*)pszToken;
			return index;
		}
	}

	// Token hash table full!!!
	// [Consider doing overflow table(s) after the main table & limiting linear hash table search]
	ALERT(at_error, "CSaveRestoreBuffer::TokenHash() is COMPLETELY FULL!\n");
	return 0;
}

void CSave::WriteData(const char* pname, int size, const char* pdata)
{
	BufferField(pname, size, pdata);
}


void CSave::WriteShort(const char* pname, const short* data, int count)
{
	BufferField(pname, sizeof(short) * count, (const char*)data);
}


void CSave::WriteInt(const char* pname, const int* data, int count)
{
	BufferField(pname, sizeof(int) * count, (const char*)data);
}


void CSave::WriteFloat(const char* pname, const float* data, int count)
{
	BufferField(pname, sizeof(float) * count, (const char*)data);
}


void CSave::WriteTime(const char* pname, const float* data, int count)
{
	int i;
	Vector tmp, input;

	BufferHeader(pname, sizeof(float) * count);
	for (i = 0; i < count; i++)
	{
		float tmp = data[0];

		// Always encode time as a delta from the current time so it can be re-based if loaded in a new level
		// Times of 0 are never written to the file, so they will be restored as 0, not a relative time
		tmp -= m_data.time;

		BufferData((const char*)&tmp, sizeof(float));
		data++;
	}
}


void CSave::WriteString(const char* pname, const char* pdata)
{
#ifdef TOKENIZE
	short token = (short)TokenHash(pdata);
	WriteShort(pname, &token, 1);
#else
	BufferField(pname, strlen(pdata) + 1, pdata);
#endif
}


void CSave::WriteString(const char* pname, const int* stringId, int count)
{
	int i, size;

#ifdef TOKENIZE
	short token = (short)TokenHash(STRING(*stringId));
	WriteShort(pname, &token, 1);
#else
#if 0
	if ( count != 1 )
		ALERT( at_error, "No string arrays!\n" );
	WriteString( pname, (char *)STRING(*stringId) );
#endif

	size = 0;
	for (i = 0; i < count; i++)
		size += strlen(STRING(stringId[i])) + 1;

	BufferHeader(pname, size);
	for (i = 0; i < count; i++)
	{
		const char* pString = STRING(stringId[i]);
		BufferData(pString, strlen(pString) + 1);
	}
#endif
}


void CSave::WriteVector(const char* pname, const Vector& value)
{
	WriteVector(pname, &value.x, 1);
}


void CSave::WriteVector(const char* pname, const float* value, int count)
{
	BufferHeader(pname, sizeof(float) * 3 * count);
	BufferData((const char*)value, sizeof(float) * 3 * count);
}



void CSave::WritePositionVector(const char* pname, const Vector& value)
{
	if (0 != m_data.fUseLandmark)
	{
		Vector tmp = value - m_data.vecLandmarkOffset;
		WriteVector(pname, tmp);
	}

	WriteVector(pname, value);
}


void CSave::WritePositionVector(const char* pname, const float* value, int count)
{
	int i;
	Vector tmp, input;

	BufferHeader(pname, sizeof(float) * 3 * count);
	for (i = 0; i < count; i++)
	{
		Vector tmp(value[0], value[1], value[2]);

		if (0 != m_data.fUseLandmark)
			tmp = tmp - m_data.vecLandmarkOffset;

		BufferData((const char*)&tmp.x, sizeof(float) * 3);
		value += 3;
	}
}


void CSave::WriteFunction(const char* cname, const char* pname, void** data, int count)
{
	const char* functionName;

	functionName = NAME_FOR_FUNCTION((uint32)*data);
	if (functionName != nullptr)
		BufferField(pname, strlen(functionName) + 1, functionName);
	else
		ALERT(at_error, "Member \"%s\" of \"%s\" contains an invalid function pointer %p!\n", pname, cname, *data);
}


void EntvarsKeyvalue(entvars_t* pev, KeyValueData* pkvd)
{
	int i;
	TYPEDESCRIPTION* pField;

	for (i = 0; i < ENTVARS_COUNT; i++)
	{
		pField = &gEntvarsDescription[i];

		if (!stricmp(pField->fieldName, pkvd->szKeyName))
		{
			switch (pField->fieldType)
			{
			case FIELD_MODELNAME:
			case FIELD_SOUNDNAME:
			case FIELD_STRING:
				(*(int*)((char*)pev + pField->fieldOffset)) = ALLOC_STRING(pkvd->szValue);
				break;

			case FIELD_TIME:
			case FIELD_FLOAT:
				(*(float*)((char*)pev + pField->fieldOffset)) = atof(pkvd->szValue);
				break;

			case FIELD_INTEGER:
				(*(int*)((char*)pev + pField->fieldOffset)) = atoi(pkvd->szValue);
				break;

			case FIELD_POSITION_VECTOR:
			case FIELD_VECTOR:
				UTIL_StringToVector((float*)((char*)pev + pField->fieldOffset), pkvd->szValue);
				break;

			default:
			case FIELD_EVARS:
			case FIELD_CLASSPTR:
			case FIELD_EDICT:
			case FIELD_ENTITY:
			case FIELD_POINTER:
				ALERT(at_error, "Bad field in entity!!\n");
				break;
			}
			pkvd->fHandled = 1;
			return;
		}
	}
}



bool CSave::WriteEntVars(const char* pname, entvars_t* pev)
{
	if (pev->targetname != 0u)
		return WriteFields(STRING(pev->targetname), pname, pev, gEntvarsDescription, ENTVARS_COUNT);
	else
		return WriteFields(STRING(pev->classname), pname, pev, gEntvarsDescription, ENTVARS_COUNT);
}



bool CSave::WriteFields(const char* cname, const char* pname, void* pBaseData, TYPEDESCRIPTION* pFields, int fieldCount)
{
	int i, j, actualCount, emptyCount;
	TYPEDESCRIPTION* pTest;
	int entityArray[MAX_ENTITYARRAY];
	byte boolArray[MAX_ENTITYARRAY];

	// Precalculate the number of empty fields
	emptyCount = 0;
	for (i = 0; i < fieldCount; i++)
	{
		void* pOutputData;
		pOutputData = ((char*)pBaseData + pFields[i].fieldOffset);
		if (DataEmpty((const char*)pOutputData, pFields[i].fieldSize * gSizes[pFields[i].fieldType]))
			emptyCount++;
	}

	// Empty fields will not be written, write out the actual number of fields to be written
	actualCount = fieldCount - emptyCount;
	WriteInt(pname, &actualCount, 1);

	for (i = 0; i < fieldCount; i++)
	{
		void* pOutputData;
		pTest = &pFields[i];
		pOutputData = ((char*)pBaseData + pTest->fieldOffset);

		// UNDONE: Must we do this twice?
		if (DataEmpty((const char*)pOutputData, pTest->fieldSize * gSizes[pTest->fieldType]))
			continue;

		switch (pTest->fieldType)
		{
		case FIELD_FLOAT:
			WriteFloat(pTest->fieldName, (float*)pOutputData, pTest->fieldSize);
			break;
		case FIELD_TIME:
			WriteTime(pTest->fieldName, (float*)pOutputData, pTest->fieldSize);
			break;
		case FIELD_MODELNAME:
		case FIELD_SOUNDNAME:
		case FIELD_STRING:
			WriteString(pTest->fieldName, (int*)pOutputData, pTest->fieldSize);
			break;
		case FIELD_CLASSPTR:
		case FIELD_EVARS:
		case FIELD_EDICT:
		case FIELD_ENTITY:
		case FIELD_EHANDLE:
			if (pTest->fieldSize > MAX_ENTITYARRAY)
				ALERT(at_error, "Can't save more than %d entities in an array!!!\n", MAX_ENTITYARRAY);
			for (j = 0; j < pTest->fieldSize; j++)
			{
				switch (pTest->fieldType)
				{
				case FIELD_EVARS:
					entityArray[j] = EntityIndex(((entvars_t**)pOutputData)[j]);
					break;
				case FIELD_CLASSPTR:
					entityArray[j] = EntityIndex(((CBaseEntity**)pOutputData)[j]);
					break;
				case FIELD_EDICT:
					entityArray[j] = EntityIndex(((edict_t**)pOutputData)[j]);
					break;
				case FIELD_ENTITY:
					entityArray[j] = EntityIndex(((EOFFSET*)pOutputData)[j]);
					break;
				case FIELD_EHANDLE:
					entityArray[j] = EntityIndex((CBaseEntity*)(((EHANDLE*)pOutputData)[j]));
					break;
				}
			}
			WriteInt(pTest->fieldName, entityArray, pTest->fieldSize);
			break;
		case FIELD_POSITION_VECTOR:
			WritePositionVector(pTest->fieldName, (float*)pOutputData, pTest->fieldSize);
			break;
		case FIELD_VECTOR:
			WriteVector(pTest->fieldName, (float*)pOutputData, pTest->fieldSize);
			break;

		case FIELD_BOOLEAN:
		{
			//TODO: need to refactor save game stuff to make this cleaner and reusable
			//Convert booleans to bytes
			for (j = 0; j < pTest->fieldSize; j++)
			{
				boolArray[j] = ((bool*)pOutputData)[j] ? 1 : 0;
			}

			WriteData(pTest->fieldName, pTest->fieldSize, (char*)boolArray);
		}
		break;

		case FIELD_INTEGER:
			WriteInt(pTest->fieldName, (int*)pOutputData, pTest->fieldSize);
			break;

		case FIELD_INT64:
			WriteData(pTest->fieldName, sizeof(std::uint64_t) * pTest->fieldSize, ((char*)pOutputData));
			break;

		case FIELD_SHORT:
			WriteData(pTest->fieldName, 2 * pTest->fieldSize, ((char*)pOutputData));
			break;

		case FIELD_CHARACTER:
			WriteData(pTest->fieldName, pTest->fieldSize, ((char*)pOutputData));
			break;

		// For now, just write the address out, we're not going to change memory while doing this yet!
		case FIELD_POINTER:
			WriteInt(pTest->fieldName, (int*)(char*)pOutputData, pTest->fieldSize);
			break;

		case FIELD_FUNCTION:
			WriteFunction(cname, pTest->fieldName, (void**)pOutputData, pTest->fieldSize);
			break;
		default:
			ALERT(at_error, "Bad field type\n");
		}
	}

	return true;
}


void CSave::BufferString(char* pdata, int len)
{
	char c = 0;

	BufferData(pdata, len); // Write the string
	BufferData(&c, 1);		// Write a null terminator
}


bool CSave::DataEmpty(const char* pdata, int size)
{
	for (int i = 0; i < size; i++)
	{
		if (0 != pdata[i])
			return false;
	}
	return true;
}


void CSave::BufferField(const char* pname, int size, const char* pdata)
{
	BufferHeader(pname, size);
	BufferData(pdata, size);
}


void CSave::BufferHeader(const char* pname, int size)
{
	short hashvalue = TokenHash(pname);
	if (size > 1 << (sizeof(short) * 8))
		ALERT(at_error, "CSave::BufferHeader() size parameter exceeds 'short'!\n");
	BufferData((const char*)&size, sizeof(short));
	BufferData((const char*)&hashvalue, sizeof(short));
}


void CSave::BufferData(const char* pdata, int size)
{
	if (m_data.size + size > m_data.bufferSize)
	{
		ALERT(at_error, "Save/Restore overflow!\n");
		m_data.size = m_data.bufferSize;
		return;
	}

	memcpy(m_data.pCurrentData, pdata, size);
	m_data.pCurrentData += size;
	m_data.size += size;
}



// --------------------------------------------------------------
//
// CRestore
//
// --------------------------------------------------------------

int CRestore::ReadField(void* pBaseData, TYPEDESCRIPTION* pFields, int fieldCount, int startField, int size, char* pName, void* pData)
{
	int i, j, stringCount, fieldNumber, entityIndex;
	TYPEDESCRIPTION* pTest;
	float timeData;
	Vector position;
	edict_t* pent;
	char* pString;

	position = Vector(0, 0, 0);

	if (0 != m_data.fUseLandmark)
		position = m_data.vecLandmarkOffset;

	for (i = 0; i < fieldCount; i++)
	{
		fieldNumber = (i + startField) % fieldCount;
		pTest = &pFields[fieldNumber];
		if ((pTest->fieldName != nullptr) && !stricmp(pTest->fieldName, pName))
		{
			if (!m_global || (pTest->flags & FTYPEDESC_GLOBAL) == 0)
			{
				for (j = 0; j < pTest->fieldSize; j++)
				{
					void* pOutputData = ((char*)pBaseData + pTest->fieldOffset + (j * gSizes[pTest->fieldType]));
					void* pInputData = (char*)pData + j * gSizes[pTest->fieldType];

					switch (pTest->fieldType)
					{
					case FIELD_TIME:
						timeData = *(float*)pInputData;
						// Re-base time variables
						timeData += m_data.time;
						*((float*)pOutputData) = timeData;
						break;
					case FIELD_FLOAT:
						*((float*)pOutputData) = *(float*)pInputData;
						break;
					case FIELD_MODELNAME:
					case FIELD_SOUNDNAME:
					case FIELD_STRING:
						// Skip over j strings
						pString = (char*)pData;
						for (stringCount = 0; stringCount < j; stringCount++)
						{
							while ('\0' != *pString)
								pString++;
							pString++;
						}
						pInputData = pString;
						if (strlen((char*)pInputData) == 0)
							*((int*)pOutputData) = 0;
						else
						{
							int string;

							string = ALLOC_STRING((char*)pInputData);

							*((int*)pOutputData) = string;

							if (!FStringNull(string) && m_precache)
							{
								if (pTest->fieldType == FIELD_MODELNAME)
									PRECACHE_MODEL((char*)STRING(string));
								else if (pTest->fieldType == FIELD_SOUNDNAME)
									PRECACHE_SOUND((char*)STRING(string));
							}
						}
						break;
					case FIELD_EVARS:
						entityIndex = *(int*)pInputData;
						pent = EntityFromIndex(entityIndex);
						if (pent != nullptr)
							*((entvars_t**)pOutputData) = VARS(pent);
						else
							*((entvars_t**)pOutputData) = nullptr;
						break;
					case FIELD_CLASSPTR:
						entityIndex = *(int*)pInputData;
						pent = EntityFromIndex(entityIndex);
						if (pent != nullptr)
							*((CBaseEntity**)pOutputData) = CBaseEntity::Instance(pent);
						else
						{
							*((CBaseEntity**)pOutputData) = nullptr;
							if (entityIndex != -1)
								ALERT(at_console, "## Restore: invalid entitynum %d\n", entityIndex);
						}
						break;
					case FIELD_EDICT:
						entityIndex = *(int*)pInputData;
						pent = EntityFromIndex(entityIndex);
						*((edict_t**)pOutputData) = pent;
						break;
					case FIELD_EHANDLE:
						// Input and Output sizes are different!
						pInputData = (char*)pData + j * sizeof(int);
						entityIndex = *(int*)pInputData;
						pent = EntityFromIndex(entityIndex);
						if (pent != nullptr)
							*((EHANDLE*)pOutputData) = CBaseEntity::Instance(pent);
						else
							*((EHANDLE*)pOutputData) = nullptr;
						break;
					case FIELD_ENTITY:
						entityIndex = *(int*)pInputData;
						pent = EntityFromIndex(entityIndex);
						if (pent != nullptr)
							*((EOFFSET*)pOutputData) = OFFSET(pent);
						else
							*((EOFFSET*)pOutputData) = 0;
						break;
					case FIELD_VECTOR:
						((float*)pOutputData)[0] = ((float*)pInputData)[0];
						((float*)pOutputData)[1] = ((float*)pInputData)[1];
						((float*)pOutputData)[2] = ((float*)pInputData)[2];
						break;
					case FIELD_POSITION_VECTOR:
						((float*)pOutputData)[0] = ((float*)pInputData)[0] + position.x;
						((float*)pOutputData)[1] = ((float*)pInputData)[1] + position.y;
						((float*)pOutputData)[2] = ((float*)pInputData)[2] + position.z;
						break;

					case FIELD_BOOLEAN:
					{
						// Input and Output sizes are different!
						pOutputData = (char*)pOutputData + j * (sizeof(bool) - gSizes[pTest->fieldType]);
						const bool value = *((byte*)pInputData) != 0;

						*((bool*)pOutputData) = value;
					}
					break;

					case FIELD_INTEGER:
						*((int*)pOutputData) = *(int*)pInputData;
						break;

					case FIELD_INT64:
						*((std::uint64_t*)pOutputData) = *(std::uint64_t*)pInputData;
						break;

					case FIELD_SHORT:
						*((short*)pOutputData) = *(short*)pInputData;
						break;

					case FIELD_CHARACTER:
						*((char*)pOutputData) = *(char*)pInputData;
						break;

					case FIELD_POINTER:
						*((int*)pOutputData) = *(int*)pInputData;
						break;
					case FIELD_FUNCTION:
						if (strlen((char*)pInputData) == 0)
							*((int*)pOutputData) = 0;
						else
							*((int*)pOutputData) = FUNCTION_FROM_NAME((char*)pInputData);
						break;

					default:
						ALERT(at_error, "Bad field type\n");
					}
				}
			}
#if 0
			else
			{
				ALERT( at_debug, "Skipping global field %s\n", pName );
			}
#endif
			return fieldNumber;
		}
	}

	return -1;
}


bool CRestore::ReadEntVars(const char* pname, entvars_t* pev)
{
	return ReadFields(pname, pev, gEntvarsDescription, ENTVARS_COUNT);
}


bool CRestore::ReadFields(const char* pname, void* pBaseData, TYPEDESCRIPTION* pFields, int fieldCount)
{
	unsigned short i, token;
	int lastField, fileCount;
	HEADER header;

	i = ReadShort();
	ASSERT(i == sizeof(int)); // First entry should be an int

	token = ReadShort();

	// Check the struct name
	if (token != TokenHash(pname)) // Field Set marker
	{
		//		ALERT( at_error, "Expected %s found %s!\n", pname, BufferPointer() );
		BufferRewind(2 * sizeof(short));
		return false;
	}

	// Skip over the struct name
	fileCount = ReadInt(); // Read field count

	lastField = 0; // Make searches faster, most data is read/written in the same order

	// Clear out base data
	for (i = 0; i < fieldCount; i++)
	{
		// Don't clear global fields
		if (!m_global || (pFields[i].flags & FTYPEDESC_GLOBAL) == 0)
			memset(((char*)pBaseData + pFields[i].fieldOffset), 0, pFields[i].fieldSize * gSizes[pFields[i].fieldType]);
	}

	for (i = 0; i < fileCount; i++)
	{
		BufferReadHeader(&header);
		lastField = ReadField(pBaseData, pFields, fieldCount, lastField, header.size, m_data.pTokens[header.token], header.pData);
		lastField++;
	}

	return true;
}


void CRestore::BufferReadHeader(HEADER* pheader)
{
	ASSERT(pheader != nullptr);
	pheader->size = ReadShort();	  // Read field size
	pheader->token = ReadShort();	  // Read field name token
	pheader->pData = BufferPointer(); // Field Data is next
	BufferSkipBytes(pheader->size);	  // Advance to next field
}


short CRestore::ReadShort()
{
	short tmp = 0;

	BufferReadBytes((char*)&tmp, sizeof(short));

	return tmp;
}

int CRestore::ReadInt()
{
	int tmp = 0;

	BufferReadBytes((char*)&tmp, sizeof(int));

	return tmp;
}

int CRestore::ReadNamedInt(const char* pName)
{
	HEADER header;

	BufferReadHeader(&header);
	return ((int*)header.pData)[0];
}

char* CRestore::ReadNamedString(const char* pName)
{
	HEADER header;

	BufferReadHeader(&header);
#ifdef TOKENIZE
	return (char*)(m_pdata->pTokens[*(short*)header.pData]);
#else
	return (char*)header.pData;
#endif
}


char* CRestore::BufferPointer()
{
	return m_data.pCurrentData;
}

void CRestore::BufferReadBytes(char* pOutput, int size)
{
	if (Empty())
		return;

	if ((m_data.size + size) > m_data.bufferSize)
	{
		ALERT(at_error, "Restore overflow!\n");
		m_data.size = m_data.bufferSize;
		return;
	}

	if (pOutput != nullptr)
		memcpy(pOutput, m_data.pCurrentData, size);
	m_data.pCurrentData += size;
	m_data.size += size;
}


void CRestore::BufferSkipBytes(int bytes)
{
	BufferReadBytes(nullptr, bytes);
}

int CRestore::BufferSkipZString()
{
	const int maxLen = m_data.bufferSize - m_data.size;

	int len = 0;
	char* pszSearch = m_data.pCurrentData;
	while ('\0' != *pszSearch++ && len < maxLen)
		len++;

	len++;

	BufferSkipBytes(len);

	return len;
}

bool CRestore::BufferCheckZString(const char* string)
{
	const int maxLen = m_data.bufferSize - m_data.size;
	const int len = strlen(string);
	if (len <= maxLen)
	{
		if (0 == strncmp(string, m_data.pCurrentData, len))
			return true;
	}
	return false;
}

// RENDERERS START
void UTIL_CustomDecal(TraceResult* pTrace, const char* name, int persistent)
{
	if (pTrace->flFraction == 1.0)
		return;

	MESSAGE_BEGIN(MSG_ALL, gmsgCreateDecal, nullptr);
	WRITE_COORD(pTrace->vecEndPos.x);
	WRITE_COORD(pTrace->vecEndPos.y);
	WRITE_COORD(pTrace->vecEndPos.z);
	WRITE_COORD(pTrace->vecPlaneNormal.x);
	WRITE_COORD(pTrace->vecPlaneNormal.y);
	WRITE_COORD(pTrace->vecPlaneNormal.z);
	WRITE_BYTE(persistent);
	WRITE_STRING(name);
	MESSAGE_END();
}

void UTIL_StudioDecal(const Vector& normal, const Vector& position, const char* name, int entindex)
{
	MESSAGE_BEGIN(MSG_BROADCAST, gmsgStudioDecal);
	WRITE_COORD(position.x);
	WRITE_COORD(position.y);
	WRITE_COORD(position.z);
	WRITE_COORD(normal.x);
	WRITE_COORD(normal.y);
	WRITE_COORD(normal.z);
	WRITE_SHORT(entindex);
	WRITE_STRING(name);
	MESSAGE_END();
}

void UTIL_Particle(const char* szName, const Vector& vecOrigin, const Vector& vDirection, int iType)
{
	MESSAGE_BEGIN(MSG_ALL, gmsgCreateSystem, nullptr);
	WRITE_COORD(vecOrigin.x);
	WRITE_COORD(vecOrigin.y);
	WRITE_COORD(vecOrigin.z);
	WRITE_COORD(vDirection.x);
	WRITE_COORD(vDirection.y);
	WRITE_COORD(vDirection.z);
	WRITE_BYTE(iType);
	WRITE_STRING(szName);
	WRITE_LONG(0);
	MESSAGE_END();
}
// RENDERERS END

//for trigger_viewset
bool HaveCamerasInPVS(edict_t* edict)
{
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBaseEntity* pEntity = UTIL_PlayerByIndex(i);
		CBasePlayer* pPlayer = (CBasePlayer*)pEntity;
		if ((pPlayer->viewFlags & 1) != 0) // custom view active
		{
			CBaseEntity* pViewEnt = UTIL_FindEntityByTargetname(nullptr, STRING(pPlayer->viewEntity));
			if (pViewEnt == nullptr)
			{
				ALERT(at_error, "bad entity string in CamerasInPVS\n");
				return false;
			}
			edict_t* view = pViewEnt->edict();
			edict_t* pent = UTIL_EntitiesInPVS(edict);

			while (!FNullEnt(pent))
			{
				if (pent == view)
				{
					//	ALERT(at_console, "CamerasInPVS found camera named %s\n", STRING(pPlayer->viewEntity));
					return true;
				}
				pent = pent->v.chain;
			}
		}
	}
	return false;
}
