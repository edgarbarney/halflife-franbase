/***
*
*	Copyright (c) 1999, 2000 Valve LLC. All rights reserved.
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
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "customentity.h"
#include "effects.h"
#include "weapons.h"
#include "decals.h"
#include "func_break.h"
#include "shake.h"
#include "player.h"	  //LRC - footstep stuff | Fran - Trinity also uses this
#include "locus.h"	  //LRC - locus utilities
#include "movewith.h" //LRC - the DesiredThink system
#include "UserMessages.h"

#define SF_FUNNEL_REVERSE 1	   // funnel effect repels particles instead of attracting them.
#define SF_FUNNEL_REPEATABLE 2 // allows a funnel to be refired

#define SF_GIBSHOOTER_REPEATABLE 1 // allows a gibshooter to be refired


//LRC - make info_target an entity class in its own right
class CInfoTarget : public CPointEntity
{
public:
	void Spawn() override;
	void Precache() override;
};

LINK_ENTITY_TO_CLASS(info_target, CInfoTarget);

//LRC- force an info_target to use the sprite null.spr
#define SF_TARGET_HACK_VISIBLE 1

// Landmark class
void CInfoTarget::Spawn()
{
	//Precache();
	pev->solid = SOLID_NOT;
	if ((pev->spawnflags & SF_TARGET_HACK_VISIBLE) != 0)
	{
		PRECACHE_MODEL("sprites/null.spr");
		SET_MODEL(ENT(pev), "sprites/null.spr");
		UTIL_SetSize(pev, g_vecZero, g_vecZero);
	}
}

void CInfoTarget::Precache()
{
	if ((pev->spawnflags & SF_TARGET_HACK_VISIBLE) != 0)
		PRECACHE_MODEL("sprites/null.spr");
}


class CBubbling : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	bool KeyValue(KeyValueData* pkvd) override;

	void EXPORT FizzThink();
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	int ObjectCaps() override { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	static TYPEDESCRIPTION m_SaveData[];

	int m_density;
	int m_frequency;
	int m_bubbleModel;
	bool m_state;

	STATE GetState() override { return m_state ? STATE_ON : STATE_OFF; };
};

LINK_ENTITY_TO_CLASS(env_bubbles, CBubbling);

TYPEDESCRIPTION CBubbling::m_SaveData[] =
	{
		DEFINE_FIELD(CBubbling, m_density, FIELD_INTEGER),
		DEFINE_FIELD(CBubbling, m_frequency, FIELD_INTEGER),
		DEFINE_FIELD(CBubbling, m_state, FIELD_BOOLEAN),
		// Let spawn restore this!
		//	DEFINE_FIELD( CBubbling, m_bubbleModel, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE(CBubbling, CBaseEntity);


#define SF_BUBBLES_STARTOFF 0x0001

void CBubbling::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), STRING(pev->model)); // Set size

	pev->solid = SOLID_NOT; // Remove model & collisions
	pev->renderamt = 0;		// The engine won't draw this model if this is set to 0 and blending is on
	pev->rendermode = kRenderTransTexture;
	int speed = pev->speed > 0 ? pev->speed : -pev->speed;

	// HACKHACK!!! - Speed in rendercolor
	pev->rendercolor.x = speed >> 8;
	pev->rendercolor.y = speed & 255;
	pev->rendercolor.z = (pev->speed < 0) ? 1 : 0;


	if ((pev->spawnflags & SF_BUBBLES_STARTOFF) == 0)
	{
		SetThink(&CBubbling::FizzThink);
		SetNextThink(2.0);
		m_state = true;
	}
	else
		m_state = false;
}

void CBubbling::Precache()
{
	m_bubbleModel = PRECACHE_MODEL("sprites/bubble.spr"); // Precache bubble sprite
}

void CBubbling::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (ShouldToggle(useType, m_state))
		m_state = !m_state;

	if (m_state)
	{
		SetThink(&CBubbling::FizzThink);
		SetNextThink(0.1);
	}
	else
	{
		SetThink(nullptr);
		DontThink();
	}
}


bool CBubbling::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "density"))
	{
		m_density = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "frequency"))
	{
		m_frequency = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "current"))
	{
		pev->speed = atoi(pkvd->szValue);
		return true;
	}

	return CBaseEntity::KeyValue(pkvd);
}


void CBubbling::FizzThink()
{
	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, VecBModelOrigin(pev));
	WRITE_BYTE(TE_FIZZ);
	WRITE_SHORT((short)ENTINDEX(edict()));
	WRITE_SHORT((short)m_bubbleModel);
	WRITE_BYTE(m_density);
	MESSAGE_END();

	if (m_frequency > 19) // frequencies above 20 are treated as 20.
		SetNextThink(0.5);
	else
		SetNextThink(2.5 - (0.1 * m_frequency));
}

// --------------------------------------------------
//
// Beams
//
// --------------------------------------------------

LINK_ENTITY_TO_CLASS(beam, CBeam);

void CBeam::Spawn()
{
	pev->solid = SOLID_NOT; // Remove model & collisions
	Precache();
}

void CBeam::Precache()
{
	if (pev->owner != nullptr)
		SetStartEntity(ENTINDEX(pev->owner));
	if (pev->aiment != nullptr)
		SetEndEntity(ENTINDEX(pev->aiment));
}

void CBeam::SetStartEntity(int entityIndex)
{
	pev->sequence = (entityIndex & 0x0FFF) | (pev->sequence & 0xF000);
	pev->owner = g_engfuncs.pfnPEntityOfEntIndex(entityIndex);
}

void CBeam::SetEndEntity(int entityIndex)
{
	pev->skin = (entityIndex & 0x0FFF) | (pev->skin & 0xF000);
	pev->aiment = g_engfuncs.pfnPEntityOfEntIndex(entityIndex);
}


// These don't take attachments into account
const Vector& CBeam::GetStartPos()
{
	if (GetType() == BEAM_ENTS)
	{
		edict_t* pent = g_engfuncs.pfnPEntityOfEntIndex(GetStartEntity());
		return pent->v.origin;
	}
	return pev->origin;
}


const Vector& CBeam::GetEndPos()
{
	int type = GetType();
	if (type == BEAM_POINTS || type == BEAM_HOSE)
	{
		return pev->angles;
	}

	edict_t* pent = g_engfuncs.pfnPEntityOfEntIndex(GetEndEntity());
	if (pent != nullptr)
		return pent->v.origin;
	return pev->angles;
}


CBeam* CBeam::BeamCreate(const char* pSpriteName, int width)
{
	// Create a new entity with CBeam private data
	CBeam* pBeam = GetClassPtr((CBeam*)nullptr);
	pBeam->pev->classname = MAKE_STRING("beam");

	pBeam->BeamInit(pSpriteName, width);

	return pBeam;
}


void CBeam::BeamInit(const char* pSpriteName, int width)
{
	pev->flags |= FL_CUSTOMENTITY;
	SetColor(255, 255, 255);
	SetBrightness(255);
	SetNoise(0);
	SetFrame(0);
	SetScrollRate(0);
	pev->model = MAKE_STRING(pSpriteName);
	SetTexture(PRECACHE_MODEL((char*)pSpriteName));
	SetWidth(width);
	pev->skin = 0;
	pev->sequence = 0;
	pev->rendermode = 0;
}


void CBeam::PointsInit(const Vector& start, const Vector& end)
{
	SetType(BEAM_POINTS);
	SetStartPos(start);
	SetEndPos(end);
	SetStartAttachment(0);
	SetEndAttachment(0);
	RelinkBeam();
}


void CBeam::HoseInit(const Vector& start, const Vector& direction)
{
	SetType(BEAM_HOSE);
	SetStartPos(start);
	SetEndPos(direction);
	SetStartAttachment(0);
	SetEndAttachment(0);
	RelinkBeam();
}


void CBeam::PointEntInit(const Vector& start, int endIndex)
{
	SetType(BEAM_ENTPOINT);
	SetStartPos(start);
	SetEndEntity(endIndex);
	SetStartAttachment(0);
	SetEndAttachment(0);
	RelinkBeam();
}

void CBeam::EntsInit(int startIndex, int endIndex)
{
	SetType(BEAM_ENTS);
	SetStartEntity(startIndex);
	SetEndEntity(endIndex);
	SetStartAttachment(0);
	SetEndAttachment(0);
	RelinkBeam();
}


void CBeam::RelinkBeam()
{
	const Vector &startPos = GetStartPos(), &endPos = GetEndPos();

	pev->mins.x = V_min(startPos.x, endPos.x);
	pev->mins.y = V_min(startPos.y, endPos.y);
	pev->mins.z = V_min(startPos.z, endPos.z);
	pev->maxs.x = V_max(startPos.x, endPos.x);
	pev->maxs.y = V_max(startPos.y, endPos.y);
	pev->maxs.z = V_max(startPos.z, endPos.z);
	pev->mins = pev->mins - pev->origin;
	pev->maxs = pev->maxs - pev->origin;

	UTIL_SetSize(pev, pev->mins, pev->maxs);
	UTIL_SetOrigin(this, pev->origin);
}

#if 0
void CBeam::SetObjectCollisionBox()
{
	const Vector &startPos = GetStartPos(), &endPos = GetEndPos();

	pev->absmin.x = V_min( startPos.x, endPos.x );
	pev->absmin.y = V_min( startPos.y, endPos.y );
	pev->absmin.z = V_min( startPos.z, endPos.z );
	pev->absmax.x = V_max( startPos.x, endPos.x );
	pev->absmax.y = V_max( startPos.y, endPos.y );
	pev->absmax.z = V_max( startPos.z, endPos.z );
}
#endif


void CBeam::TriggerTouch(CBaseEntity* pOther)
{
	if ((pOther->pev->flags & (FL_CLIENT | FL_MONSTER)) != 0)
	{
		if (pev->owner != nullptr)
		{
			CBaseEntity* pOwner = CBaseEntity::Instance(pev->owner);
			pOwner->Use(pOther, this, USE_TOGGLE, 0);
		}
		ALERT(at_debug, "Firing targets!!!\n");
	}
}


CBaseEntity* CBeam::RandomTargetname(const char* szName)
{
	int total = 0;

	CBaseEntity* pEntity = nullptr;
	CBaseEntity* pNewEntity = nullptr;
	while ((pNewEntity = UTIL_FindEntityByTargetname(pNewEntity, szName)) != nullptr)
	{
		total++;
		if (RANDOM_LONG(0, total - 1) < 1)
			pEntity = pNewEntity;
	}
	return pEntity;
}


void CBeam::DoSparks(const Vector& start, const Vector& end)
{
	if ((pev->spawnflags & (SF_BEAM_SPARKSTART | SF_BEAM_SPARKEND)) != 0)
	{
		if ((pev->spawnflags & SF_BEAM_SPARKSTART) != 0)
		{
			UTIL_Sparks(start);
		}
		if ((pev->spawnflags & SF_BEAM_SPARKEND) != 0)
		{
			UTIL_Sparks(end);
		}
	}
}

class CLightning : public CBeam
{
public:
	void Spawn() override;
	void Precache() override;
	bool KeyValue(KeyValueData* pkvd) override;
	void Activate() override;

	void EXPORT StrikeThink();
	void EXPORT TripThink();
	void RandomArea();
	void RandomPoint(Vector& vecSrc);
	void Zap(const Vector& vecSrc, const Vector& vecDest);
	void EXPORT StrikeUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	void EXPORT ToggleUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);

	inline bool ServerSide()
	{
		if (m_life == 0 && (pev->spawnflags & SF_BEAM_RING) == 0)
			return true;
		return false;
	}

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	void BeamUpdatePoints(); //LRC
	void BeamUpdateVars();

	STATE GetState() override { return m_active ? STATE_OFF : STATE_ON; };

	bool m_active;
	int m_iszStartEntity;
	int m_iszEndEntity;
	float m_life;
	int m_boltWidth;
	int m_noiseAmplitude;
	int m_brightness;
	int m_speed;
	float m_restrike;
	int m_spriteTexture;
	int m_iszSpriteName;
	int m_frameStart;
	int m_iStartAttachment;
	int m_iEndAttachment;

	float m_radius;
};

LINK_ENTITY_TO_CLASS(env_lightning, CLightning);
LINK_ENTITY_TO_CLASS(env_beam, CLightning);

// UNDONE: Jay -- CTripBeam is only a test
#if _DEBUG
class CTripBeam : public CLightning
{
	void Spawn() override;
};
LINK_ENTITY_TO_CLASS(trip_beam, CTripBeam);

void CTripBeam::Spawn()
{
	CLightning::Spawn();
	SetTouch(&CBeam::TriggerTouch);
	pev->solid = SOLID_TRIGGER;
	RelinkBeam();
}
#endif



TYPEDESCRIPTION CLightning::m_SaveData[] =
	{
		DEFINE_FIELD(CLightning, m_active, FIELD_BOOLEAN),
		DEFINE_FIELD(CLightning, m_iszStartEntity, FIELD_STRING),
		DEFINE_FIELD(CLightning, m_iStartAttachment, FIELD_INTEGER),
		DEFINE_FIELD(CLightning, m_iszEndEntity, FIELD_STRING),
		DEFINE_FIELD(CLightning, m_iEndAttachment, FIELD_INTEGER),
		DEFINE_FIELD(CLightning, m_life, FIELD_FLOAT),
		DEFINE_FIELD(CLightning, m_boltWidth, FIELD_INTEGER),
		DEFINE_FIELD(CLightning, m_noiseAmplitude, FIELD_INTEGER),
		DEFINE_FIELD(CLightning, m_brightness, FIELD_INTEGER),
		DEFINE_FIELD(CLightning, m_speed, FIELD_INTEGER),
		DEFINE_FIELD(CLightning, m_restrike, FIELD_FLOAT),
		DEFINE_FIELD(CLightning, m_spriteTexture, FIELD_INTEGER),
		DEFINE_FIELD(CLightning, m_iszSpriteName, FIELD_STRING),
		DEFINE_FIELD(CLightning, m_frameStart, FIELD_INTEGER),
		DEFINE_FIELD(CLightning, m_radius, FIELD_FLOAT),
};

IMPLEMENT_SAVERESTORE(CLightning, CBeam);


void CLightning::Spawn()
{
	if (FStringNull(m_iszSpriteName))
	{
		SetThink(&CLightning::SUB_Remove);
		return;
	}
	pev->solid = SOLID_NOT; // Remove model & collisions
	Precache();

	pev->dmgtime = gpGlobals->time;

	//LRC- a convenience for mappers. Will this mess anything up?
	if (pev->rendercolor == g_vecZero)
		pev->rendercolor = Vector(255, 255, 255);

	if (pev->frags == 0)
	{
		pev->frags = DMG_ENERGYBEAM;
	}

	if (ServerSide())
	{
		SetThink(nullptr);
		if (pev->dmg != 0 || !FStringNull(pev->target))
		{
			SetThink(&CLightning::TripThink);
			SetNextThink(0.1);
		}
		if (!FStringNull(pev->targetname))
		{
			if ((pev->spawnflags & SF_BEAM_STARTON) == 0)
			{
				pev->effects = EF_NODRAW;
				m_active = false;
				DontThink();
			}
			else
				m_active = true;

			SetUse(&CLightning::ToggleUse);
		}
	}
	else
	{
		m_active = false;
		if (!FStringNull(pev->targetname))
		{
			SetUse(&CLightning::StrikeUse);
		}
		if (FStringNull(pev->targetname) || FBitSet(pev->spawnflags, SF_BEAM_STARTON))
		{
			SetThink(&CLightning::StrikeThink);
			SetNextThink(1.0);
		}
	}
}

void CLightning::Precache()
{
	m_spriteTexture = PRECACHE_MODEL((char*)STRING(m_iszSpriteName));
	CBeam::Precache();
}


void CLightning::Activate()
{
	if (ServerSide())
		BeamUpdateVars();

	CBeam::Activate();
}


bool CLightning::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "LightningStart"))
	{
		m_iszStartEntity = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "LightningStartAttachment"))
	{
		m_iStartAttachment = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "LightningEnd"))
	{
		m_iszEndEntity = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "LightningEndAttachment"))
	{
		m_iEndAttachment = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "life"))
	{
		m_life = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "BoltWidth"))
	{
		m_boltWidth = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "NoiseAmplitude"))
	{
		m_noiseAmplitude = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "TextureScroll"))
	{
		m_speed = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "StrikeTime"))
	{
		m_restrike = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "texture"))
	{
		m_iszSpriteName = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "framestart"))
	{
		m_frameStart = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "Radius"))
	{
		m_radius = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "damage"))
	{
		pev->dmg = atof(pkvd->szValue);
		return true;
	}

	return CBeam::KeyValue(pkvd);
}


void CLightning::ToggleUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!ShouldToggle(useType, m_active))
		return;
	if (m_active)
	{
		m_active = false;
		SUB_UseTargets(this, USE_OFF, 0); //LRC
		pev->effects |= EF_NODRAW;
		DontThink();
	}
	else
	{
		m_active = true;
		SUB_UseTargets(this, USE_ON, 0); //LRC
		BeamUpdatePoints();
		pev->effects &= ~EF_NODRAW;
		DoSparks(GetStartPos(), GetEndPos());
		if (pev->dmg > 0)
		{
			SetNextThink(0);
			pev->dmgtime = gpGlobals->time;
		}
	}
}


void CLightning::StrikeUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!ShouldToggle(useType, m_active))
		return;

	if (m_active)
	{
		m_active = false;
		SetThink(nullptr);
	}
	else
	{
		SetThink(&CLightning::StrikeThink);
		SetNextThink(0.1);
	}

	if (!FBitSet(pev->spawnflags, SF_BEAM_TOGGLE))
		SetUse(nullptr);
}


bool IsPointEntity(CBaseEntity* pEnt)
{
	//	ALERT(at_console, "IsPE: %s, %d\n", STRING(pEnt->pev->classname), pEnt->pev->modelindex);
	if ((pEnt->pev->modelindex != 0) && ((pEnt->pev->flags & FL_CUSTOMENTITY) == 0)) //LRC- follow (almost) any entity that has a model
		return false;
	else
		return true;
}


void CLightning::StrikeThink()
{
	if (m_life != 0 && m_restrike != -1) //LRC non-restriking beams! what an idea!
	{
		if ((pev->spawnflags & SF_BEAM_RANDOM) != 0)
			SetNextThink(m_life + RANDOM_FLOAT(0, m_restrike));
		else
			SetNextThink(m_life + m_restrike);
	}
	m_active = true;

	if (FStringNull(m_iszEndEntity))
	{
		if (FStringNull(m_iszStartEntity))
		{
			RandomArea();
		}
		else
		{
			CBaseEntity* pStart = RandomTargetname(STRING(m_iszStartEntity));
			if (pStart != nullptr)
				RandomPoint(pStart->pev->origin);
			else
				ALERT(at_debug, "env_beam: unknown entity \"%s\"\n", STRING(m_iszStartEntity));
		}
		return;
	}

	CBaseEntity* pStart = RandomTargetname(STRING(m_iszStartEntity));
	CBaseEntity* pEnd = RandomTargetname(STRING(m_iszEndEntity));

	if (pStart != nullptr && pEnd != nullptr)
	{
		if (IsPointEntity(pStart) || IsPointEntity(pEnd))
		{
			if ((pev->spawnflags & SF_BEAM_RING) != 0)
			{
				// don't work
				//LRC- FIXME: tell the user there's a problem.
				return;
			}
		}

		MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
		if (IsPointEntity(pStart) || IsPointEntity(pEnd))
		{
			if (!IsPointEntity(pEnd)) // One point entity must be in pEnd
			{
				CBaseEntity* pTemp;
				pTemp = pStart;
				pStart = pEnd;
				pEnd = pTemp;
			}

			if (!IsPointEntity(pStart)) // One sided
			{
				WRITE_BYTE(TE_BEAMENTPOINT);
				WRITE_SHORT(pStart->entindex());
				WRITE_COORD(pEnd->pev->origin.x);
				WRITE_COORD(pEnd->pev->origin.y);
				WRITE_COORD(pEnd->pev->origin.z);
			}
			else
			{
				WRITE_BYTE(TE_BEAMPOINTS);
				WRITE_COORD(pStart->pev->origin.x);
				WRITE_COORD(pStart->pev->origin.y);
				WRITE_COORD(pStart->pev->origin.z);
				WRITE_COORD(pEnd->pev->origin.x);
				WRITE_COORD(pEnd->pev->origin.y);
				WRITE_COORD(pEnd->pev->origin.z);
			}
		}
		else
		{
			if ((pev->spawnflags & SF_BEAM_RING) != 0)
				WRITE_BYTE(TE_BEAMRING);
			else
				WRITE_BYTE(TE_BEAMENTS);
			WRITE_SHORT(pStart->entindex());
			WRITE_SHORT(pEnd->entindex());
		}

		WRITE_SHORT(m_spriteTexture);
		WRITE_BYTE(m_frameStart);			 // framestart
		WRITE_BYTE((int)pev->framerate);	 // framerate
		WRITE_BYTE((int)(m_life * 10.0));	 // life
		WRITE_BYTE(m_boltWidth);			 // width
		WRITE_BYTE(m_noiseAmplitude);		 // noise
		WRITE_BYTE((int)pev->rendercolor.x); // r, g, b
		WRITE_BYTE((int)pev->rendercolor.y); // r, g, b
		WRITE_BYTE((int)pev->rendercolor.z); // r, g, b
		WRITE_BYTE(pev->renderamt);			 // brightness
		WRITE_BYTE(m_speed);				 // speed
		MESSAGE_END();
		DoSparks(pStart->pev->origin, pEnd->pev->origin);
		if ((pev->dmg != 0.0f) || !FStringNull(pev->target))
		{
			TraceResult tr;
			UTIL_TraceLine(pStart->pev->origin, pEnd->pev->origin, dont_ignore_monsters, nullptr, &tr);
			if (pev->dmg != 0.0f)
				BeamDamageInstant(&tr, pev->dmg);

			//LRC - tripbeams
			CBaseEntity* pTrip;
			if (!FStringNull(pev->target) && (pTrip = GetTripEntity(&tr)) != nullptr)
				FireTargets(STRING(pev->target), pTrip, this, USE_TOGGLE, 0);
		}
	}
}


CBaseEntity* CBeam::GetTripEntity(TraceResult* ptr)
{
	CBaseEntity* pTrip;

	if (ptr->flFraction == 1.0 || ptr->pHit == nullptr)
		return nullptr;

	pTrip = CBaseEntity::Instance(ptr->pHit);
	if (pTrip == nullptr)
		return nullptr;

	if (FStringNull(pev->netname))
	{
		if ((pTrip->pev->flags & (FL_CLIENT | FL_MONSTER)) != 0)
			return pTrip;
		else
			return nullptr;
	}
	else if (FClassnameIs(pTrip->pev, STRING(pev->netname)))
		return pTrip;
	else if (FStrEq(STRING(pTrip->pev->targetname), STRING(pev->netname)))
		return pTrip;
	else
		return nullptr;
}

void CBeam::BeamDamage(TraceResult* ptr)
{
	RelinkBeam();
	if (ptr->flFraction != 1.0 && ptr->pHit != nullptr)
	{
		CBaseEntity* pHit = CBaseEntity::Instance(ptr->pHit);
		if (pHit != nullptr)
		{
			if (pev->dmg > 0)
			{
				ClearMultiDamage();
				pHit->TraceAttack(pev, pev->dmg * (gpGlobals->time - pev->dmgtime), (ptr->vecEndPos - pev->origin).Normalize(), ptr, pev->frags);
				ApplyMultiDamage(pev, pev);
				if ((pev->spawnflags & SF_BEAM_DECALS) != 0)
				{
					if (pHit->IsBSPModel())
						UTIL_DecalTrace(ptr, DECAL_BIGSHOT1 + RANDOM_LONG(0, 4));
				}
			}
			else
			{
				//LRC - beams that heal people
				pHit->TakeHealth(-(pev->dmg * (gpGlobals->time - pev->dmgtime)), DMG_GENERIC);
			}
		}
	}
	pev->dmgtime = gpGlobals->time;
}

//LRC - used to be DamageThink, but now it's more general.
void CLightning::TripThink()
{
	SetNextThink(0.1);
	TraceResult tr;

	//ALERT(at_console,"TripThink\n");

	if (pev->dmg != 0)
	{
		UTIL_TraceLine(GetStartPos(), GetEndPos(), dont_ignore_monsters, nullptr, &tr);
		BeamDamage(&tr);
	}

	//LRC - tripbeams
	if (!FStringNull(pev->target))
	{
		// nicked from monster_tripmine:
		//HACKHACK Set simple box using this really nice global!
		gpGlobals->trace_flags = FTRACE_SIMPLEBOX;
		UTIL_TraceLine(GetStartPos(), GetEndPos(), dont_ignore_monsters, nullptr, &tr);
		CBaseEntity* pTrip = GetTripEntity(&tr);
		if (pTrip != nullptr)
		{
			if (!FBitSet(pev->spawnflags, SF_BEAM_TRIPPED))
			{
				FireTargets(STRING(pev->target), pTrip, this, USE_TOGGLE, 0);
				pev->spawnflags |= SF_BEAM_TRIPPED;
			}
		}
		else
		{
			pev->spawnflags &= ~SF_BEAM_TRIPPED;
		}
	}
}



void CLightning::Zap(const Vector& vecSrc, const Vector& vecDest)
{
#if 1
	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_BEAMPOINTS);
	WRITE_COORD(vecSrc.x);
	WRITE_COORD(vecSrc.y);
	WRITE_COORD(vecSrc.z);
	WRITE_COORD(vecDest.x);
	WRITE_COORD(vecDest.y);
	WRITE_COORD(vecDest.z);
	WRITE_SHORT(m_spriteTexture);
	WRITE_BYTE(m_frameStart);			 // framestart
	WRITE_BYTE((int)pev->framerate);	 // framerate
	WRITE_BYTE((int)(m_life * 10.0));	 // life
	WRITE_BYTE(m_boltWidth);			 // width
	WRITE_BYTE(m_noiseAmplitude);		 // noise
	WRITE_BYTE((int)pev->rendercolor.x); // r, g, b
	WRITE_BYTE((int)pev->rendercolor.y); // r, g, b
	WRITE_BYTE((int)pev->rendercolor.z); // r, g, b
	WRITE_BYTE(pev->renderamt);			 // brightness
	WRITE_BYTE(m_speed);				 // speed
	MESSAGE_END();
#else
	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_LIGHTNING);
	WRITE_COORD(vecSrc.x);
	WRITE_COORD(vecSrc.y);
	WRITE_COORD(vecSrc.z);
	WRITE_COORD(vecDest.x);
	WRITE_COORD(vecDest.y);
	WRITE_COORD(vecDest.z);
	WRITE_BYTE(10);
	WRITE_BYTE(50);
	WRITE_BYTE(40);
	WRITE_SHORT(m_spriteTexture);
	MESSAGE_END();
#endif
	DoSparks(vecSrc, vecDest);
}

void CLightning::RandomArea()
{
	int iLoops = 0;

	for (iLoops = 0; iLoops < 10; iLoops++)
	{
		Vector vecSrc = pev->origin;

		Vector vecDir1 = Vector(RANDOM_FLOAT(-1.0, 1.0), RANDOM_FLOAT(-1.0, 1.0), RANDOM_FLOAT(-1.0, 1.0));
		vecDir1 = vecDir1.Normalize();
		TraceResult tr1;
		UTIL_TraceLine(vecSrc, vecSrc + vecDir1 * m_radius, ignore_monsters, ENT(pev), &tr1);

		if (tr1.flFraction == 1.0)
			continue;

		Vector vecDir2;
		do
		{
			vecDir2 = Vector(RANDOM_FLOAT(-1.0, 1.0), RANDOM_FLOAT(-1.0, 1.0), RANDOM_FLOAT(-1.0, 1.0));
		} while (DotProduct(vecDir1, vecDir2) > 0);
		vecDir2 = vecDir2.Normalize();
		TraceResult tr2;
		UTIL_TraceLine(vecSrc, vecSrc + vecDir2 * m_radius, ignore_monsters, ENT(pev), &tr2);

		if (tr2.flFraction == 1.0)
			continue;

		if ((tr1.vecEndPos - tr2.vecEndPos).Length() < m_radius * 0.1)
			continue;

		UTIL_TraceLine(tr1.vecEndPos, tr2.vecEndPos, ignore_monsters, ENT(pev), &tr2);

		if (tr2.flFraction != 1.0)
			continue;

		Zap(tr1.vecEndPos, tr2.vecEndPos);

		break;
	}
}


void CLightning::RandomPoint(Vector& vecSrc)
{
	int iLoops = 0;

	for (iLoops = 0; iLoops < 10; iLoops++)
	{
		Vector vecDir1 = Vector(RANDOM_FLOAT(-1.0, 1.0), RANDOM_FLOAT(-1.0, 1.0), RANDOM_FLOAT(-1.0, 1.0));
		vecDir1 = vecDir1.Normalize();
		TraceResult tr1;
		UTIL_TraceLine(vecSrc, vecSrc + vecDir1 * m_radius, ignore_monsters, ENT(pev), &tr1);

		if ((tr1.vecEndPos - vecSrc).Length() < m_radius * 0.1)
			continue;

		if (tr1.flFraction == 1.0)
			continue;

		Zap(vecSrc, tr1.vecEndPos);
		break;
	}
}


// LRC: Called whenever the beam gets turned on, in case an alias changed or one of the points has moved.
void CLightning::BeamUpdatePoints()
{
	int beamType;
	bool pointStart, pointEnd;

	CBaseEntity* pStart = UTIL_FindEntityByTargetname(nullptr, STRING(m_iszStartEntity));
	CBaseEntity* pEnd = UTIL_FindEntityByTargetname(nullptr, STRING(m_iszEndEntity));
	if ((pStart == nullptr) || (pEnd == nullptr))
		return;
	pointStart = IsPointEntity(pStart);
	pointEnd = IsPointEntity(pEnd);

	beamType = BEAM_ENTS;
	if (pointStart || pointEnd)
	{
		if (!pointStart) // One point entity must be in pStart
		{
			CBaseEntity* pTemp;
			// Swap start & end
			pTemp = pStart;
			pStart = pEnd;
			pEnd = pTemp;
			bool swap = pointStart;
			pointStart = pointEnd;
			pointEnd = swap;
		}
		if (!pointEnd)
			beamType = BEAM_ENTPOINT;
		else
			beamType = BEAM_POINTS;
	}

	SetType(beamType);
	if (beamType == BEAM_POINTS || beamType == BEAM_ENTPOINT || beamType == BEAM_HOSE)
	{
		SetStartPos(pStart->pev->origin);
		if (beamType == BEAM_POINTS || beamType == BEAM_HOSE)
			SetEndPos(pEnd->pev->origin);
		else
		{
			SetEndEntity(ENTINDEX(ENT(pEnd->pev)));
			SetEndAttachment(m_iEndAttachment);
		}
	}
	else
	{
		SetStartEntity(ENTINDEX(ENT(pStart->pev)));
		SetStartAttachment(m_iStartAttachment);
		SetEndEntity(ENTINDEX(ENT(pEnd->pev)));
		SetEndAttachment(m_iEndAttachment);
	}

	RelinkBeam();
}

void CLightning::BeamUpdateVars()
{
	pev->skin = 0;
	pev->sequence = 0;
	pev->rendermode = 0;
	pev->flags |= FL_CUSTOMENTITY;
	pev->model = m_iszSpriteName;
	SetTexture(m_spriteTexture);

	BeamUpdatePoints(); //LRC

	SetWidth(m_boltWidth);
	SetNoise(m_noiseAmplitude);
	SetFrame(m_frameStart);
	SetScrollRate(m_speed);
	if ((pev->spawnflags & SF_BEAM_SHADEIN) != 0)
		SetFlags(BEAM_FSHADEIN);
	else if ((pev->spawnflags & SF_BEAM_SHADEOUT) != 0)
		SetFlags(BEAM_FSHADEOUT);
	else if ((pev->spawnflags & SF_BEAM_SOLID) != 0)
		SetFlags(BEAM_FSOLID);
}


LINK_ENTITY_TO_CLASS(env_laser, CLaser);

TYPEDESCRIPTION CLaser::m_SaveData[] =
	{
		DEFINE_FIELD(CLaser, m_pStartSprite, FIELD_CLASSPTR),
		DEFINE_FIELD(CLaser, m_pEndSprite, FIELD_CLASSPTR),
		DEFINE_FIELD(CLaser, m_iszStartSpriteName, FIELD_STRING),
		DEFINE_FIELD(CLaser, m_iszEndSpriteName, FIELD_STRING),
		DEFINE_FIELD(CLaser, m_firePosition, FIELD_POSITION_VECTOR),
		DEFINE_FIELD(CLaser, m_iProjection, FIELD_INTEGER),
		DEFINE_FIELD(CLaser, m_iStoppedBy, FIELD_INTEGER),
		DEFINE_FIELD(CLaser, m_iszStartPosition, FIELD_STRING),
};

IMPLEMENT_SAVERESTORE(CLaser, CBeam);

void CLaser::Spawn()
{
	if (FStringNull(pev->model))
	{
		SetThink(&CLaser::SUB_Remove);
		return;
	}
	pev->solid = SOLID_NOT; // Remove model & collisions
	Precache();

	SetThink(&CLaser::StrikeThink);
	pev->flags |= FL_CUSTOMENTITY;
}

void CLaser::PostSpawn()
{
	if (m_iszStartSpriteName != 0)
	{
		//LRC: allow the spritename to be the name of an env_sprite
		CBaseEntity* pTemp = UTIL_FindEntityByTargetname(nullptr, STRING(m_iszStartSpriteName));
		if (pTemp == nullptr)
		{
			m_pStartSprite = CSprite::SpriteCreate(STRING(m_iszStartSpriteName), pev->origin, true);
			if (m_pStartSprite != nullptr)
				m_pStartSprite->SetTransparency(kRenderGlow, pev->rendercolor.x, pev->rendercolor.y, pev->rendercolor.z, pev->renderamt, pev->renderfx);
		}
		else if (!FClassnameIs(pTemp->pev, "env_sprite"))
		{
			ALERT(at_error, "env_laser \"%s\" found startsprite %s, but can't use: not an env_sprite\n", STRING(pev->targetname), STRING(m_iszStartSpriteName));
			m_pStartSprite = nullptr;
		}
		else
		{
			// use an env_sprite defined by the mapper
			m_pStartSprite = (CSprite*)pTemp;
			m_pStartSprite->pev->movetype = MOVETYPE_NOCLIP;
		}
	}
	else if ((pev->spawnflags & SF_LASER_INTERPOLATE) != 0) // interpolated lasers must have sprites at the start
	{
		m_pStartSprite = CSprite::SpriteCreate("sprites/null.spr", pev->origin, true);
	}
	else
		m_pStartSprite = nullptr;


	if (m_iszEndSpriteName != 0)
	{
		CBaseEntity* pTemp = UTIL_FindEntityByTargetname(nullptr, STRING(m_iszEndSpriteName));
		if (pTemp == nullptr)
		{
			m_pEndSprite = CSprite::SpriteCreate(STRING(m_iszEndSpriteName), pev->origin, true);
			if (m_pEndSprite != nullptr)
				m_pEndSprite->SetTransparency(kRenderGlow, pev->rendercolor.x, pev->rendercolor.y, pev->rendercolor.z, pev->renderamt, pev->renderfx);
		}
		else if (!FClassnameIs(pTemp->pev, "env_sprite"))
		{
			ALERT(at_error, "env_laser \"%s\" found endsprite %s, but can't use: not an env_sprite\n", STRING(pev->targetname), STRING(m_iszEndSpriteName));
			m_pEndSprite = nullptr;
		}
		else
		{
			// use an env_sprite defined by the mapper
			m_pEndSprite = (CSprite*)pTemp;
			m_pEndSprite->pev->movetype = MOVETYPE_NOCLIP;
		}
	}
	else if ((pev->spawnflags & SF_LASER_INTERPOLATE) != 0) // interpolated lasers must have sprites at the end
	{
		m_pEndSprite = CSprite::SpriteCreate("sprites/null.spr", pev->origin, true);
	}
	else
		m_pEndSprite = nullptr;

	//LRC
	if ((m_pStartSprite != nullptr) && (m_pEndSprite != nullptr) && ((pev->spawnflags & SF_LASER_INTERPOLATE) != 0))
		EntsInit(m_pStartSprite->entindex(), m_pEndSprite->entindex());
	else
		PointsInit(pev->origin, pev->origin);

	if (!FStringNull(pev->targetname) && (pev->spawnflags & SF_BEAM_STARTON) == 0)
		TurnOff();
	else
		TurnOn();
}

void CLaser::Precache()
{
	PRECACHE_MODEL("sprites/null.spr");
	pev->modelindex = PRECACHE_MODEL((char*)STRING(pev->model));
	if (m_iszStartSpriteName != 0)
	{
		// UGLY HACK to check whether this is a filename: does it contain a dot?
		const char* c = STRING(m_iszStartSpriteName);
		while (*c != 0)
		{
			if (*c == '.')
			{
				PRECACHE_MODEL((char*)STRING(m_iszStartSpriteName));
				break;
			}
			c++; // the magic word?
		}
	}

	if (m_iszEndSpriteName != 0)
	{
		const char* c = STRING(m_iszEndSpriteName);
		while (*c != 0)
		{
			if (*c == '.')
			{
				PRECACHE_MODEL((char*)STRING(m_iszEndSpriteName));
				break;
			}
			c++;
		}
	}
}


bool CLaser::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "LaserStart"))
	{
		m_iszStartPosition = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "LaserTarget"))
	{
		pev->message = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iTowardsMode"))
	{
		m_iTowardsMode = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "width"))
	{
		SetWidth((int)atof(pkvd->szValue));
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "NoiseAmplitude"))
	{
		SetNoise(atoi(pkvd->szValue));
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "TextureScroll"))
	{
		SetScrollRate(atoi(pkvd->szValue));
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "texture"))
	{
		pev->model = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "StartSprite"))
	{
		m_iszStartSpriteName = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "EndSprite"))
	{
		m_iszEndSpriteName = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "framestart"))
	{
		pev->frame = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "damage"))
	{
		pev->dmg = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iProjection"))
	{
		m_iProjection = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iStoppedBy"))
	{
		m_iStoppedBy = atoi(pkvd->szValue);
		return true;
	}
	return CBeam::KeyValue(pkvd);
}

void CLaser::TurnOff()
{
	pev->effects |= EF_NODRAW;
	DontThink();
	if (m_pStartSprite != nullptr)
	{
		m_pStartSprite->TurnOff();
		UTIL_SetVelocity(m_pStartSprite, g_vecZero); //LRC
	}
	if (m_pEndSprite != nullptr)
	{
		m_pEndSprite->TurnOff();
		UTIL_SetVelocity(m_pEndSprite, g_vecZero); //LRC
	}
}


void CLaser::TurnOn()
{
	pev->effects &= ~EF_NODRAW;

	if (m_pStartSprite != nullptr)
		m_pStartSprite->TurnOn();

	if (m_pEndSprite != nullptr)
		m_pEndSprite->TurnOn();

	pev->dmgtime = gpGlobals->time;

	if ((pev->spawnflags & SF_BEAM_SHADEIN) != 0)
		SetFlags(BEAM_FSHADEIN);
	else if ((pev->spawnflags & SF_BEAM_SHADEOUT) != 0)
		SetFlags(BEAM_FSHADEOUT);
	else if ((pev->spawnflags & SF_BEAM_SOLID) != 0)
		SetFlags(BEAM_FSOLID);

	SetNextThink(0);
}


void CLaser::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	bool active = (GetState() == STATE_ON);

	if (!ShouldToggle(useType, active))
		return;
	if (active)
		TurnOff();
	else
	{
		m_hActivator = pActivator; //AJH Storage variable to allow *locus start/end positions
		TurnOn();
	}
}


void CLaser::FireAtPoint(Vector startpos, TraceResult& tr)
{
	if (((pev->spawnflags & SF_LASER_INTERPOLATE) != 0) && (m_pStartSprite != nullptr) && (m_pEndSprite != nullptr))
	{
		UTIL_SetVelocity(m_pStartSprite, (startpos - m_pStartSprite->pev->origin) * 10);
		UTIL_SetVelocity(m_pEndSprite, (tr.vecEndPos - m_pEndSprite->pev->origin) * 10);
	}
	else
	{
		if (m_pStartSprite != nullptr)
			UTIL_AssignOrigin(m_pStartSprite, startpos);

		if (m_pEndSprite != nullptr)
			UTIL_AssignOrigin(m_pEndSprite, tr.vecEndPos);

		SetStartPos(startpos);
		SetEndPos(tr.vecEndPos);
	}

	BeamDamage(&tr);
	DoSparks(startpos, tr.vecEndPos);
}

void CLaser::StrikeThink()
{
	Vector startpos = pev->origin;
	bool success = true;

	if (m_iszStartPosition != 0)
	{
		startpos = CalcLocus_Position(this, m_hActivator, STRING(m_iszStartPosition)); //AJH allow *locus start/end positions
	}

	if (m_iTowardsMode != 0)
	{
		m_firePosition = startpos + CalcLocus_Velocity(this, m_hActivator, STRING(pev->message)); //AJH allow *locus start/end positions
	}
	else
	{
		CBaseEntity* pEnd = RandomTargetname(STRING(pev->message));

		if (pEnd != nullptr)
		{
			pEnd->CalcPosition(m_hActivator, &m_firePosition);
		}
		else
		{
			m_firePosition = CalcLocus_Position(this, m_hActivator, STRING(pev->message));
		}
	}

	TraceResult tr;

	//LRC
	//	UTIL_TraceLine( pev->origin, m_firePosition, dont_ignore_monsters, NULL, &tr );
	IGNORE_GLASS iIgnoreGlass;
	if ((m_iStoppedBy % 2) != 0) // if it's an odd number
		iIgnoreGlass = ignore_glass;
	else
		iIgnoreGlass = dont_ignore_glass;

	IGNORE_MONSTERS iIgnoreMonsters;
	if (m_iStoppedBy <= 1)
		iIgnoreMonsters = dont_ignore_monsters;
	else if (m_iStoppedBy <= 3)
		iIgnoreMonsters = missile;
	else
		iIgnoreMonsters = ignore_monsters;

	if (m_iProjection != 0)
	{
		Vector vecProject = startpos + 4096 * ((m_firePosition - startpos).Normalize());
		UTIL_TraceLine(startpos, vecProject, iIgnoreMonsters, iIgnoreGlass, nullptr, &tr);
	}
	else
	{
		UTIL_TraceLine(startpos, m_firePosition, iIgnoreMonsters, iIgnoreGlass, nullptr, &tr);
	}

	FireAtPoint(startpos, tr);

	//LRC - tripbeams
	if (pev->target != 0u)
	{
		// nicked from monster_tripmine:
		//HACKHACK Set simple box using this really nice global!
		gpGlobals->trace_flags = FTRACE_SIMPLEBOX;
		UTIL_TraceLine(startpos, m_firePosition, dont_ignore_monsters, nullptr, &tr);
		CBaseEntity* pTrip = GetTripEntity(&tr);
		if (pTrip != nullptr)
		{
			if (!FBitSet(pev->spawnflags, SF_BEAM_TRIPPED))
			{
				FireTargets(STRING(pev->target), pTrip, this, USE_TOGGLE, 0);
				pev->spawnflags |= SF_BEAM_TRIPPED;
			}
		}
		else
		{
			pev->spawnflags &= ~SF_BEAM_TRIPPED;
		}
	}
	SetNextThink(0.1);
}



class CGlow : public CPointEntity
{
public:
	void Spawn() override;
	void Think() override;
	void Animate(float frames);
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	float m_lastTime;
	float m_maxFrame;
};

LINK_ENTITY_TO_CLASS(env_glow, CGlow);

TYPEDESCRIPTION CGlow::m_SaveData[] =
	{
		DEFINE_FIELD(CGlow, m_lastTime, FIELD_TIME),
		DEFINE_FIELD(CGlow, m_maxFrame, FIELD_FLOAT),
};

IMPLEMENT_SAVERESTORE(CGlow, CPointEntity);

void CGlow::Spawn()
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->effects = 0;
	pev->frame = 0;

	PRECACHE_MODEL((char*)STRING(pev->model));
	SET_MODEL(ENT(pev), STRING(pev->model));

	m_maxFrame = (float)MODEL_FRAMES(pev->modelindex) - 1;
	if (m_maxFrame > 1.0 && pev->framerate != 0)
		SetNextThink(0.1);

	m_lastTime = gpGlobals->time;
}


void CGlow::Think()
{
	Animate(pev->framerate * (gpGlobals->time - m_lastTime));

	SetNextThink(0.1);
	m_lastTime = gpGlobals->time;
}


void CGlow::Animate(float frames)
{
	if (m_maxFrame > 0)
		pev->frame = fmod(pev->frame + frames, m_maxFrame);
}


LINK_ENTITY_TO_CLASS(env_sprite, CSprite);

TYPEDESCRIPTION CSprite::m_SaveData[] =
	{
		DEFINE_FIELD(CSprite, m_lastTime, FIELD_TIME),
		DEFINE_FIELD(CSprite, m_maxFrame, FIELD_FLOAT),
};

IMPLEMENT_SAVERESTORE(CSprite, CPointEntity);

void CSprite::Spawn()
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->effects = 0;
	pev->frame = 0;

	Precache();
	SET_MODEL(ENT(pev), STRING(pev->model));

	m_maxFrame = (float)MODEL_FRAMES(pev->modelindex) - 1;
	if (!FStringNull(pev->targetname) && (pev->spawnflags & SF_SPRITE_STARTON) == 0)
		TurnOff();
	else
		TurnOn();

	// Worldcraft only sets y rotation, copy to Z
	if (pev->angles.y != 0 && pev->angles.z == 0)
	{
		pev->angles.z = pev->angles.y;
		pev->angles.y = 0;
	}
}


void CSprite::Precache()
{
	PRECACHE_MODEL((char*)STRING(pev->model));

	// Reset attachment after save/restore
	if (pev->aiment != nullptr)
		SetAttachment(pev->aiment, pev->body);
	else
	{
		// Clear attachment
		pev->skin = 0;
		pev->body = 0;
	}
}


void CSprite::SpriteInit(const char* pSpriteName, const Vector& origin)
{
	pev->model = MAKE_STRING(pSpriteName);
	pev->origin = origin;
	Spawn();
}

CSprite* CSprite::SpriteCreate(const char* pSpriteName, const Vector& origin, bool animate)
{
	CSprite* pSprite = GetClassPtr((CSprite*)nullptr);
	pSprite->SpriteInit(pSpriteName, origin);
	pSprite->pev->classname = MAKE_STRING("env_sprite");
	pSprite->pev->solid = SOLID_NOT;
	pSprite->pev->movetype = MOVETYPE_NOCLIP;
	if (animate)
		pSprite->TurnOn();

	return pSprite;
}


void CSprite::AnimateThink()
{
	Animate(pev->framerate * (gpGlobals->time - m_lastTime));

	SetNextThink(0.1);
	m_lastTime = gpGlobals->time;
}

void CSprite::AnimateUntilDead()
{
	if (gpGlobals->time > pev->dmgtime)
		UTIL_Remove(this);
	else
	{
		AnimateThink();
		SetNextThink(0);
	}
}

void CSprite::Expand(float scaleSpeed, float fadeSpeed)
{
	pev->speed = scaleSpeed;
	pev->health = fadeSpeed;
	SetThink(&CSprite::ExpandThink);

	SetNextThink(0);
	m_lastTime = gpGlobals->time;
}


void CSprite::ExpandThink()
{
	float frametime = gpGlobals->time - m_lastTime;
	pev->scale += pev->speed * frametime;
	pev->renderamt -= pev->health * frametime;
	if (pev->renderamt <= 0)
	{
		pev->renderamt = 0;
		UTIL_Remove(this);
	}
	else
	{
		SetNextThink(0.1);
		m_lastTime = gpGlobals->time;
	}
}


void CSprite::Animate(float frames)
{
	pev->frame += frames;
	if (pev->frame > m_maxFrame)
	{
		if ((pev->spawnflags & SF_SPRITE_ONCE) != 0)
		{
			TurnOff();
		}
		else
		{
			if (m_maxFrame > 0)
				pev->frame = fmod(pev->frame, m_maxFrame);
		}
	}
}


void CSprite::TurnOff()
{
	pev->effects = EF_NODRAW;
	DontThink();
}


void CSprite::TurnOn()
{
	if (pev->message != 0u)
	{
		CBaseEntity* pTemp = UTIL_FindEntityByTargetname(nullptr, STRING(pev->message));
		if (pTemp != nullptr)
			SetAttachment(pTemp->edict(), pev->frags);
		else
			return;
	}
	pev->effects = 0;
	if ((0 != pev->framerate && m_maxFrame > 1.0) || (pev->spawnflags & SF_SPRITE_ONCE) != 0)
	{
		SetThink(&CSprite::AnimateThink);
		SetNextThink(0);
		m_lastTime = gpGlobals->time;
	}
	pev->frame = 0;
}


void CSprite::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	bool on = pev->effects != EF_NODRAW;
	if (ShouldToggle(useType, on))
	{
		if (on)
		{
			SUB_UseTargets(this, USE_OFF, 0); //LRC
			TurnOff();
		}
		else
		{
			SUB_UseTargets(this, USE_ON, 0); //LRC
			TurnOn();
		}
	}
}

//=================================================================
// env_model: like env_sprite, except you can specify a sequence.
//=================================================================
#define SF_ENVMODEL_OFF 1
#define SF_ENVMODEL_DROPTOFLOOR 2
#define SF_ENVMODEL_SOLID 4

class CEnvModel : public CBaseAnimating
{
	void Spawn() override;
	void Precache() override;
	void EXPORT Think() override;
	bool KeyValue(KeyValueData* pkvd) override;
	STATE GetState() override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	int ObjectCaps() override { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	void SetSequence();

	string_t m_iszSequence_On;
	string_t m_iszSequence_Off;
	int m_iAction_On;
	int m_iAction_Off;
};

TYPEDESCRIPTION CEnvModel::m_SaveData[] =
	{
		DEFINE_FIELD(CEnvModel, m_iszSequence_On, FIELD_STRING),
		DEFINE_FIELD(CEnvModel, m_iszSequence_Off, FIELD_STRING),
		DEFINE_FIELD(CEnvModel, m_iAction_On, FIELD_INTEGER),
		DEFINE_FIELD(CEnvModel, m_iAction_Off, FIELD_INTEGER),
};

IMPLEMENT_SAVERESTORE(CEnvModel, CBaseAnimating);
LINK_ENTITY_TO_CLASS(env_model, CEnvModel);

bool CEnvModel::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "m_iszSequence_On"))
	{
		m_iszSequence_On = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszSequence_Off"))
	{
		m_iszSequence_Off = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iAction_On"))
	{
		m_iAction_On = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iAction_Off"))
	{
		m_iAction_Off = atoi(pkvd->szValue);
		return true;
	}
	return CBaseAnimating::KeyValue(pkvd);
}

void CEnvModel::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), STRING(pev->model));
	UTIL_SetOrigin(this, pev->origin);

	//	UTIL_AssignOrigin(this, pev->oldorigin); //AJH - WTF is this here for?

	if ((pev->spawnflags & SF_ENVMODEL_SOLID) != 0)
	{
		pev->solid = SOLID_SLIDEBOX;
		UTIL_SetSize(pev, Vector(-10, -10, -10), Vector(10, 10, 10)); //LRCT
	}

	if ((pev->spawnflags & SF_ENVMODEL_DROPTOFLOOR) != 0)
	{
		pev->origin.z += 1;
		DROP_TO_FLOOR(ENT(pev));
	}
	SetBoneController(0, 0);
	SetBoneController(1, 0);

	SetSequence();

	SetNextThink(0.1);
}

void CEnvModel::Precache()
{
	PRECACHE_MODEL((char*)STRING(pev->model));
}

STATE CEnvModel::GetState()
{
	if ((pev->spawnflags & SF_ENVMODEL_OFF) != 0)
		return STATE_OFF;
	else
		return STATE_ON;
}

void CEnvModel::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (ShouldToggle(useType, (pev->spawnflags & SF_ENVMODEL_OFF) == 0))
	{
		if ((pev->spawnflags & SF_ENVMODEL_OFF) != 0)
			pev->spawnflags &= ~SF_ENVMODEL_OFF;
		else
			pev->spawnflags |= SF_ENVMODEL_OFF;

		SetSequence();
		SetNextThink(0.1);
	}
}

void CEnvModel::Think()
{
	int iTemp;

	//	ALERT(at_console, "env_model Think fr=%f\n", pev->framerate);

	StudioFrameAdvance(); // set m_fSequenceFinished if necessary

	//	if (m_fSequenceLoops)
	//	{
	//		SetNextThink( 1E6 );
	//		return; // our work here is done.
	//	}
	if (m_fSequenceFinished && !m_fSequenceLoops)
	{
		if ((pev->spawnflags & SF_ENVMODEL_OFF) != 0)
			iTemp = m_iAction_Off;
		else
			iTemp = m_iAction_On;

		switch (iTemp)
		{
			//		case 1: // loop
			//			pev->animtime = gpGlobals->time;
			//			m_fSequenceFinished = false;
			//			m_flLastEventCheck = gpGlobals->time;
			//			pev->frame = 0;
			//			break;
		case 2: // change state
			if ((pev->spawnflags & SF_ENVMODEL_OFF) != 0)
				pev->spawnflags &= ~SF_ENVMODEL_OFF;
			else
				pev->spawnflags |= SF_ENVMODEL_OFF;
			SetSequence();
			break;
		default: //remain frozen
			return;
		}
	}
	SetNextThink(0.1);
}

void CEnvModel::SetSequence()
{
	int iszSeq;

	if ((pev->spawnflags & SF_ENVMODEL_OFF) != 0)
		iszSeq = m_iszSequence_Off;
	else
		iszSeq = m_iszSequence_On;

	if (iszSeq == 0)
		return;
	pev->sequence = LookupSequence(STRING(iszSeq));

	if (pev->sequence == -1)
	{
		if (pev->targetname != 0u)
			ALERT(at_error, "env_model %s: unknown sequence \"%s\"\n", STRING(pev->targetname), STRING(iszSeq));
		else
			ALERT(at_error, "env_model: unknown sequence \"%s\"\n", STRING(pev->targetname), STRING(iszSeq));
		pev->sequence = 0;
	}

	pev->frame = 0;
	ResetSequenceInfo();

	if ((pev->spawnflags & SF_ENVMODEL_OFF) != 0)
	{
		if (m_iAction_Off == 1)
			m_fSequenceLoops = true;
		else
			m_fSequenceLoops = false;
	}
	else
	{
		if (m_iAction_On == 1)
			m_fSequenceLoops = true;
		else
			m_fSequenceLoops = false;
	}
}


#define SF_GIBSHOOTER_REPEATABLE 1 // allows a gibshooter to be refired
#define SF_GIBSHOOTER_DEBUG 4	   //LRC

class CGibShooter : public CBaseDelay
{
public:
	void Spawn() override;
	void Precache() override;
	bool KeyValue(KeyValueData* pkvd) override;
	void EXPORT ShootThink();
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;

	virtual CBaseEntity* CreateGib(Vector vecPos, Vector vecVel);

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	int m_iGibs;
	int m_iGibCapacity;
	int m_iGibMaterial;
	int m_iGibModelIndex;
	//	float m_flGibVelocity;
	float m_flVariance;
	float m_flGibLife;
	int m_iszTargetname;
	int m_iszPosition;
	int m_iszVelocity;
	int m_iszVelFactor;
	int m_iszSpawnTarget;
	int m_iBloodColor;
};

TYPEDESCRIPTION CGibShooter::m_SaveData[] =
	{
		DEFINE_FIELD(CGibShooter, m_iGibs, FIELD_INTEGER),
		DEFINE_FIELD(CGibShooter, m_iGibCapacity, FIELD_INTEGER),
		DEFINE_FIELD(CGibShooter, m_iGibMaterial, FIELD_INTEGER),
		DEFINE_FIELD(CGibShooter, m_iGibModelIndex, FIELD_INTEGER),
		//	DEFINE_FIELD( CGibShooter, m_flGibVelocity, FIELD_FLOAT ),
		DEFINE_FIELD(CGibShooter, m_flVariance, FIELD_FLOAT),
		DEFINE_FIELD(CGibShooter, m_flGibLife, FIELD_FLOAT),
		DEFINE_FIELD(CGibShooter, m_iszTargetname, FIELD_STRING),
		DEFINE_FIELD(CGibShooter, m_iszPosition, FIELD_STRING),
		DEFINE_FIELD(CGibShooter, m_iszVelocity, FIELD_STRING),
		DEFINE_FIELD(CGibShooter, m_iszVelFactor, FIELD_STRING),
		DEFINE_FIELD(CGibShooter, m_iszSpawnTarget, FIELD_STRING),
		DEFINE_FIELD(CGibShooter, m_iBloodColor, FIELD_INTEGER),
};

IMPLEMENT_SAVERESTORE(CGibShooter, CBaseDelay);
LINK_ENTITY_TO_CLASS(gibshooter, CGibShooter);


void CGibShooter::Precache()
{
	if (g_Language == LANGUAGE_GERMAN)
	{
		m_iGibModelIndex = PRECACHE_MODEL("models/germanygibs.mdl");
	}
	else if (m_iBloodColor == BLOOD_COLOR_YELLOW)
	{
		m_iGibModelIndex = PRECACHE_MODEL("models/agibs.mdl");
	}
	else
	{
		m_iGibModelIndex = PRECACHE_MODEL("models/hgibs.mdl");
	}
}


bool CGibShooter::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "m_iGibs"))
	{
		m_iGibs = m_iGibCapacity = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_flVelocity"))
	{
		m_iszVelFactor = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_flVariance"))
	{
		m_flVariance = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_flGibLife"))
	{
		m_flGibLife = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszTargetName"))
	{
		m_iszTargetname = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszPosition"))
	{
		m_iszPosition = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszVelocity"))
	{
		m_iszVelocity = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszVelFactor"))
	{
		m_iszVelFactor = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszSpawnTarget"))
	{
		m_iszSpawnTarget = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iBloodColor"))
	{
		m_iBloodColor = atoi(pkvd->szValue);
		return true;
	}

	return CBaseDelay::KeyValue(pkvd);
}

void CGibShooter::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	m_hActivator = pActivator;
	SetThink(&CGibShooter::ShootThink);
	SetNextThink(0);
}

void CGibShooter::Spawn()
{
	Precache();

	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	//	if ( m_flDelay == 0 )
	//	{
	//		m_flDelay = 0.1;
	//	}

	if (m_flGibLife == 0)
	{
		m_flGibLife = 25;
	}

	SetMovedir(pev);
	if (pev->body == 0)
		pev->body = MODEL_FRAMES(m_iGibModelIndex);
}


CBaseEntity* CGibShooter::CreateGib(Vector vecPos, Vector vecVel)
{
	if (CVAR_GET_FLOAT("violence_hgibs") == 0)
		return nullptr;

	CGib* pGib = GetClassPtr((CGib*)nullptr);

	//	if (pGib)
	//		ALERT(at_console, "Gib created ok\n");

	pGib->pev->origin = vecPos;
	pGib->pev->velocity = vecVel;

	if (m_iBloodColor == BLOOD_COLOR_YELLOW)
	{
		pGib->Spawn("models/agibs.mdl");
		pGib->m_bloodColor = BLOOD_COLOR_YELLOW;
	}
	else if (m_iBloodColor != 0)
	{
		pGib->Spawn("models/hgibs.mdl");
		pGib->m_bloodColor = m_iBloodColor;
	}
	else
	{
		pGib->Spawn("models/hgibs.mdl");
		pGib->m_bloodColor = BLOOD_COLOR_RED;
	}

	if (pev->body <= 1)
	{
		ALERT(at_aiconsole, "GibShooter Body is <= 1!\n");
	}

	pGib->pev->body = RANDOM_LONG(1, pev->body - 1); // avoid throwing random amounts of the 0th gib. (skull).

	float thinkTime = pGib->m_fNextThink - gpGlobals->time;

	pGib->m_lifeTime = (m_flGibLife * RANDOM_FLOAT(0.95, 1.05)); // +/- 5%
	if (pGib->m_lifeTime < thinkTime)
	{
		pGib->SetNextThink(pGib->m_lifeTime);
		pGib->m_lifeTime = 0;
	}

	pGib->pev->avelocity.x = RANDOM_FLOAT(100, 200);
	pGib->pev->avelocity.y = RANDOM_FLOAT(100, 300);

	return pGib;
}


void CGibShooter::ShootThink()
{
	int i;
	if (m_flDelay == 0) // LRC - delay is 0, fire them all at once.
	{
		i = m_iGibs;
	}
	else
	{
		i = 1;
		SetNextThink(m_flDelay);
	}

	while (i > 0)
	{
		Vector vecShootDir;
		Vector vecPos;
		float flGibVelocity;
		if (!FStringNull(m_iszVelFactor))
			flGibVelocity = CalcLocus_Number(m_hActivator, STRING(m_iszVelFactor));
		else
			flGibVelocity = 1;

		if (!FStringNull(m_iszVelocity))
		{
			vecShootDir = CalcLocus_Velocity(this, m_hActivator, STRING(m_iszVelocity));
			flGibVelocity = flGibVelocity * vecShootDir.Length();
			vecShootDir = vecShootDir.Normalize();
		}
		else
			vecShootDir = pev->movedir;

		vecShootDir = vecShootDir + gpGlobals->v_right * RANDOM_FLOAT(-1, 1) * m_flVariance;
		vecShootDir = vecShootDir + gpGlobals->v_forward * RANDOM_FLOAT(-1, 1) * m_flVariance;
		vecShootDir = vecShootDir + gpGlobals->v_up * RANDOM_FLOAT(-1, 1) * m_flVariance;

		vecShootDir = vecShootDir.Normalize();

		if (!FStringNull(m_iszPosition))
			vecPos = CalcLocus_Position(this, m_hActivator, STRING(m_iszPosition));
		else
			vecPos = pev->origin;
		CBaseEntity* pGib = CreateGib(vecPos, vecShootDir * flGibVelocity);

		if (pGib != nullptr)
		{
			pGib->pev->targetname = m_iszTargetname;
			//			pGib->pev->velocity = vecShootDir * flGibVelocity;

			if ((pev->spawnflags & SF_GIBSHOOTER_DEBUG) != 0)
				ALERT(at_debug, "DEBUG: %s \"%s\" creates a shot at %f %f %f; vel %f %f %f; pos \"%s\"\n", STRING(pev->classname), STRING(pev->targetname), pGib->pev->origin.x, pGib->pev->origin.y, pGib->pev->origin.z, pGib->pev->velocity.x, pGib->pev->velocity.y, pGib->pev->velocity.z, STRING(m_iszPosition));

			if (m_iszSpawnTarget != 0)
				FireTargets(STRING(m_iszSpawnTarget), pGib, this, USE_TOGGLE, 0);
		}

		i--;
		m_iGibs--;
	}

	if (m_iGibs <= 0)
	{
		if ((pev->spawnflags & SF_GIBSHOOTER_REPEATABLE) != 0)
		{
			m_iGibs = m_iGibCapacity;
			SetThink(nullptr);
			DontThink();
		}
		else
		{
			SetThink(&CGibShooter::SUB_Remove);
			SetNextThink(0);
		}
	}
}


// Shooter particle
class CShot : public CSprite
{
public:
	void Touch(CBaseEntity* pOther) override;
};

void CShot::Touch(CBaseEntity* pOther)
{
	if (pev->teleport_time > gpGlobals->time)
		return;
	// don't fire too often in collisions!
	// teleport_time is the soonest this can be touched again.
	pev->teleport_time = gpGlobals->time + 0.1;

	if (pev->netname != 0u)
		FireTargets(STRING(pev->netname), this, this, USE_TOGGLE, 0);
	if ((pev->message != 0u) && (pOther != nullptr) && pOther != g_pWorld)
		FireTargets(STRING(pev->message), pOther, this, USE_TOGGLE, 0);
}

class CEnvShooter : public CGibShooter
{
	void Precache() override;
	bool KeyValue(KeyValueData* pkvd) override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	void Spawn() override;

	static TYPEDESCRIPTION m_SaveData[];

	CBaseEntity* CreateGib(Vector vecPos, Vector vecVel) override;

	int m_iszTouch;
	int m_iszTouchOther;
	int m_iPhysics;
	float m_fFriction;
	Vector m_vecSize;
};

TYPEDESCRIPTION CEnvShooter::m_SaveData[] =
	{
		DEFINE_FIELD(CEnvShooter, m_iszTouch, FIELD_STRING),
		DEFINE_FIELD(CEnvShooter, m_iszTouchOther, FIELD_STRING),
		DEFINE_FIELD(CEnvShooter, m_iPhysics, FIELD_INTEGER),
		DEFINE_FIELD(CEnvShooter, m_fFriction, FIELD_FLOAT),
		DEFINE_FIELD(CEnvShooter, m_vecSize, FIELD_VECTOR),
};

IMPLEMENT_SAVERESTORE(CEnvShooter, CGibShooter);
LINK_ENTITY_TO_CLASS(env_shooter, CEnvShooter);

void CEnvShooter::Spawn()
{
	int iBody = pev->body;
	CGibShooter::Spawn();
	pev->body = iBody;
}

bool CEnvShooter::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "shootmodel"))
	{
		pev->model = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "shootsounds"))
	{
		int iNoise = atoi(pkvd->szValue);

		switch (iNoise)
		{
		case 0:
			m_iGibMaterial = matGlass;
			break;
		case 1:
			m_iGibMaterial = matWood;
			break;
		case 2:
			m_iGibMaterial = matMetal;
			break;
		case 3:
			m_iGibMaterial = matFlesh;
			break;
		case 4:
			m_iGibMaterial = matRocks;
			break;

		default:
		case -1:
			m_iGibMaterial = matNone;
			break;
		}

		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszTouch"))
	{
		m_iszTouch = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszTouchOther"))
	{
		m_iszTouchOther = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iPhysics"))
	{
		m_iPhysics = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_fFriction"))
	{
		m_fFriction = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_vecSize"))
	{
		UTIL_StringToVector((float*)m_vecSize, pkvd->szValue);
		m_vecSize = m_vecSize / 2;
		return true;
	}
	return CGibShooter::KeyValue(pkvd);
}


void CEnvShooter::Precache()
{
	if (pev->model != 0u)
		m_iGibModelIndex = PRECACHE_MODEL((char*)STRING(pev->model));
	CBreakable::MaterialSoundPrecache((Materials)m_iGibMaterial);
}


CBaseEntity* CEnvShooter::CreateGib(Vector vecPos, Vector vecVel)
{
	if (pev->noise != 0u)
		pev->scale = CalcLocus_Number(this, STRING(pev->noise), nullptr); //AJH / MJB - allow locus_ratio for scale
	if (m_iPhysics <= 1)											// normal gib or sticky gib
	{
		CGib* pGib = GetClassPtr((CGib*)nullptr);

		pGib->pev->origin = vecPos;
		pGib->pev->velocity = vecVel;

		pGib->Spawn(STRING(pev->model));
		if (m_iPhysics != 0) // sticky gib
		{
			pGib->pev->movetype = MOVETYPE_TOSS;
			pGib->pev->solid = SOLID_BBOX;
			UTIL_SetSize(pGib->pev, Vector(0, 0, 0), Vector(0, 0, 0));
			pGib->SetTouch(&CGib::StickyGibTouch);
		}
		if (pev->body > 0)
			pGib->pev->body = RANDOM_LONG(0, pev->body - 1);
		if (m_iBloodColor != 0)
			pGib->m_bloodColor = m_iBloodColor;
		else
			pGib->m_bloodColor = DONT_BLEED;
		pGib->m_material = m_iGibMaterial;
		pGib->pev->rendermode = pev->rendermode;
		pGib->pev->renderamt = pev->renderamt;
		pGib->pev->rendercolor = pev->rendercolor;
		pGib->pev->renderfx = pev->renderfx;
		if (pev->scale >= 100)
			pGib->pev->scale = 1.0; //G-Cont. for fix with laarge gibs :) (MJB i know, but sometimes people deliberately want large models?)
		else
			pGib->pev->scale = pev->scale;
		pGib->pev->skin = pev->skin;

		float thinkTime = pGib->m_fNextThink - gpGlobals->time;

		pGib->m_lifeTime = (m_flGibLife * RANDOM_FLOAT(0.95, 1.05)); // +/- 5%
		if (pGib->m_lifeTime < thinkTime)
		{
			pGib->SetNextThink(pGib->m_lifeTime);
			pGib->m_lifeTime = 0;
		}

		pGib->pev->avelocity.x = RANDOM_FLOAT(100, 200);
		pGib->pev->avelocity.y = RANDOM_FLOAT(100, 300);

		return pGib;
	}

	// special shot
	CShot* pShot = GetClassPtr((CShot*)nullptr);
	if (FStringNull(m_iPhysics))
		pShot->pev->movetype = MOVETYPE_BOUNCE; //G-Cont. fix for blank field m_iPhysics, e. g. - original HL
	pShot->pev->classname = MAKE_STRING("shot");
	pShot->pev->solid = SOLID_SLIDEBOX;
	pShot->pev->origin = vecPos;
	pShot->pev->velocity = vecVel;
	SET_MODEL(ENT(pShot->pev), STRING(pev->model));
	UTIL_SetSize(pShot->pev, -m_vecSize, m_vecSize);
	pShot->pev->renderamt = pev->renderamt;
	pShot->pev->rendermode = pev->rendermode;
	pShot->pev->rendercolor = pev->rendercolor;
	pShot->pev->renderfx = pev->renderfx;
	pShot->pev->netname = m_iszTouch;
	pShot->pev->message = m_iszTouchOther;
	pShot->pev->skin = pev->skin;
	pShot->pev->body = pev->body;
	pShot->pev->scale = pev->scale;
	pShot->pev->frame = pev->frame;
	pShot->pev->framerate = pev->framerate;
	pShot->pev->friction = m_fFriction;

	switch (m_iPhysics)
	{
	case 2:
		pShot->pev->movetype = MOVETYPE_NOCLIP;
		pShot->pev->solid = SOLID_NOT;
		break;
	case 3:
		pShot->pev->movetype = MOVETYPE_FLYMISSILE;
		break;
	case 4:
		pShot->pev->movetype = MOVETYPE_BOUNCEMISSILE;
		break;
	case 5:
		pShot->pev->movetype = MOVETYPE_TOSS;
		break;
	case 6:
		pShot->pev->movetype = MOVETYPE_BOUNCE;
		break;
	}

	if (pShot->pev->framerate != 0.0f)
	{
		pShot->m_maxFrame = (float)MODEL_FRAMES(pShot->pev->modelindex) - 1;
		if (pShot->m_maxFrame > 1.0)
		{
			if (m_flGibLife != 0.0f)
			{
				pShot->pev->dmgtime = gpGlobals->time + m_flGibLife;
				pShot->SetThink(&CShot::AnimateUntilDead);
			}
			else
			{
				pShot->SetThink(&CShot::AnimateThink);
			}
			pShot->SetNextThink(0);
			pShot->m_lastTime = gpGlobals->time;
			return pShot;
		}
	}

	// if it's not animating
	if (m_flGibLife != 0.0f)
	{
		pShot->SetThink(&CShot::SUB_Remove);
		pShot->SetNextThink(m_flGibLife);
	}
	return pShot;
}




class CTestEffect : public CBaseDelay
{
public:
	void Spawn() override;
	void Precache() override;
	// bool	KeyValue( KeyValueData *pkvd );
	void EXPORT TestThink();
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;

	int m_iLoop;
	int m_iBeam;
	CBeam* m_pBeam[24];
	float m_flBeamTime[24];
	float m_flStartTime;
};


LINK_ENTITY_TO_CLASS(test_effect, CTestEffect);

void CTestEffect::Spawn()
{
	Precache();
}

void CTestEffect::Precache()
{
	PRECACHE_MODEL("sprites/lgtning.spr");
}

void CTestEffect::TestThink()
{
	int i;
	float t = (gpGlobals->time - m_flStartTime);

	if (m_iBeam < 24)
	{
		CBeam* pbeam = CBeam::BeamCreate("sprites/lgtning.spr", 100);

		TraceResult tr;

		Vector vecSrc = pev->origin;
		Vector vecDir = Vector(RANDOM_FLOAT(-1.0, 1.0), RANDOM_FLOAT(-1.0, 1.0), RANDOM_FLOAT(-1.0, 1.0));
		vecDir = vecDir.Normalize();
		UTIL_TraceLine(vecSrc, vecSrc + vecDir * 128, ignore_monsters, ENT(pev), &tr);

		pbeam->PointsInit(vecSrc, tr.vecEndPos);
		// pbeam->SetColor( 80, 100, 255 );
		pbeam->SetColor(255, 180, 100);
		pbeam->SetWidth(100);
		pbeam->SetScrollRate(12);

		m_flBeamTime[m_iBeam] = gpGlobals->time;
		m_pBeam[m_iBeam] = pbeam;
		m_iBeam++;

#if 0
		Vector vecMid = (vecSrc + tr.vecEndPos) * 0.5;
		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE(TE_DLIGHT);
			WRITE_COORD(vecMid.x);	// X
			WRITE_COORD(vecMid.y);	// Y
			WRITE_COORD(vecMid.z);	// Z
			WRITE_BYTE( 20 );		// radius * 0.1
			WRITE_BYTE( 255 );		// r
			WRITE_BYTE( 180 );		// g
			WRITE_BYTE( 100 );		// b
			WRITE_BYTE( 20 );		// time * 10
			WRITE_BYTE( 0 );		// decay * 0.1
		MESSAGE_END( );
#endif
	}

	if (t < 3.0)
	{
		for (i = 0; i < m_iBeam; i++)
		{
			t = (gpGlobals->time - m_flBeamTime[i]) / (3 + m_flStartTime - m_flBeamTime[i]);
			m_pBeam[i]->SetBrightness(255 * t);
			// m_pBeam[i]->SetScrollRate( 20 * t );
		}
		SetNextThink(0.1);
	}
	else
	{
		for (i = 0; i < m_iBeam; i++)
		{
			UTIL_Remove(m_pBeam[i]);
		}
		m_flStartTime = gpGlobals->time;
		m_iBeam = 0;
		SetThink(nullptr);
	}
}


void CTestEffect::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	SetThink(&CTestEffect::TestThink);
	SetNextThink(0.1);
	m_flStartTime = gpGlobals->time;
}



// Blood effects
class CBlood : public CPointEntity
{
public:
	void Spawn() override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	bool KeyValue(KeyValueData* pkvd) override;

	inline int Color() { return pev->impulse; }
	inline float BloodAmount() { return pev->dmg; }

	inline void SetColor(int color) { pev->impulse = color; }
	inline void SetBloodAmount(float amount) { pev->dmg = amount; }

	Vector Direction(CBaseEntity* pActivator); //LRC - added pActivator, for locus system
	Vector BloodPosition(CBaseEntity* pActivator);

private:
};

LINK_ENTITY_TO_CLASS(env_blood, CBlood);



#define SF_BLOOD_RANDOM 0x0001
#define SF_BLOOD_STREAM 0x0002
#define SF_BLOOD_PLAYER 0x0004
#define SF_BLOOD_DECAL 0x0008

void CBlood::Spawn()
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->effects = 0;
	pev->frame = 0;
	SetMovedir(pev);
	if (Color() == 0)
		SetColor(BLOOD_COLOR_RED);
}


bool CBlood::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "color"))
	{
		int color = atoi(pkvd->szValue);
		switch (color)
		{
		case 1:
			SetColor(BLOOD_COLOR_YELLOW);
			break;
		default:
			SetColor(BLOOD_COLOR_RED);
			break;
		}

		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "amount"))
	{
		SetBloodAmount(atof(pkvd->szValue));
		return true;
	}

	return CPointEntity::KeyValue(pkvd);
}


Vector CBlood::Direction(CBaseEntity* pActivator)
{
	if ((pev->spawnflags & SF_BLOOD_RANDOM) != 0)
		return UTIL_RandomBloodVector();
	else if (pev->netname != 0u)
		return CalcLocus_Velocity(this, pActivator, STRING(pev->netname));
	else
		return pev->movedir;
}


Vector CBlood::BloodPosition(CBaseEntity* pActivator)
{
	if ((pev->spawnflags & SF_BLOOD_PLAYER) != 0)
	{
		CBaseEntity* pPlayer;

		if ((pActivator != nullptr) && pActivator->IsPlayer())
		{
			pPlayer = pActivator;
		}
		else
			pPlayer = UTIL_GetLocalPlayer();
		if (pPlayer != nullptr)
			return (pPlayer->pev->origin + pPlayer->pev->view_ofs) + Vector(RANDOM_FLOAT(-10, 10), RANDOM_FLOAT(-10, 10), RANDOM_FLOAT(-10, 10));
		// if no player found, fall through
	}
	else if (pev->target != 0u)
	{
		return CalcLocus_Position(this, pActivator, STRING(pev->target));
	}

	return pev->origin;
}


void CBlood::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if ((pev->spawnflags & SF_BLOOD_STREAM) != 0)
		UTIL_BloodStream(BloodPosition(pActivator), Direction(pActivator), (Color() == BLOOD_COLOR_RED) ? 70 : Color(), BloodAmount());
	else
		UTIL_BloodDrips(BloodPosition(pActivator), Direction(pActivator), Color(), BloodAmount());

	if ((pev->spawnflags & SF_BLOOD_DECAL) != 0)
	{
		Vector forward = Direction(pActivator);
		Vector start = BloodPosition(pActivator);
		TraceResult tr;

		UTIL_TraceLine(start, start + forward * BloodAmount() * 2, ignore_monsters, nullptr, &tr);
		if (tr.flFraction != 1.0)
			UTIL_BloodDecalTrace(&tr, Color());
	}
}



// Screen shake
class CShake : public CPointEntity
{
public:
	void Spawn() override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	bool KeyValue(KeyValueData* pkvd) override;

	inline float Amplitude() { return pev->scale; }
	inline float Frequency() { return pev->dmg_save; }
	inline float Duration() { return pev->dmg_take; }
	inline float Radius() { return pev->dmg; }

	inline void SetAmplitude(float amplitude) { pev->scale = amplitude; }
	inline void SetFrequency(float frequency) { pev->dmg_save = frequency; }
	inline void SetDuration(float duration) { pev->dmg_take = duration; }
	inline void SetRadius(float radius) { pev->dmg = radius; }

	STATE m_iState;									 //LRC
	STATE GetState() override { return m_iState; };	 //LRC
	void Think() override { m_iState = STATE_OFF; }; //LRC
private:
};

LINK_ENTITY_TO_CLASS(env_shake, CShake);

// pev->scale is amplitude
// pev->dmg_save is frequency
// pev->dmg_take is duration
// pev->dmg is radius
// radius of 0 means all players
// NOTE: UTIL_ScreenShake() will only shake players who are on the ground

#define SF_SHAKE_EVERYONE 0x0001 // Don't check radius
// UNDONE: These don't work yet
#define SF_SHAKE_DISRUPT 0x0002 // Disrupt controls
#define SF_SHAKE_INAIR 0x0004	// Shake players in air

void CShake::Spawn()
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->effects = 0;
	pev->frame = 0;

	m_iState = STATE_OFF; //LRC

	if ((pev->spawnflags & SF_SHAKE_EVERYONE) != 0)
		pev->dmg = 0;
}


bool CShake::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "amplitude"))
	{
		SetAmplitude(atof(pkvd->szValue));
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "frequency"))
	{
		SetFrequency(atof(pkvd->szValue));
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "duration"))
	{
		SetDuration(atof(pkvd->szValue));
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "radius"))
	{
		SetRadius(atof(pkvd->szValue));
		return true;
	}

	return CPointEntity::KeyValue(pkvd);
}


void CShake::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	UTIL_ScreenShake(pev->origin, Amplitude(), Frequency(), Duration(), Radius());
	m_iState = STATE_ON;	  //LRC
	SetNextThink(Duration()); //LRC
}


class CFade : public CPointEntity
{
public:
	void Spawn() override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	bool KeyValue(KeyValueData* pkvd) override;

	STATE GetState() override { return m_iState; }; // LRC
	void Think() override;							//LRC

	inline float Duration() { return pev->dmg_take; }
	inline float HoldTime() { return pev->dmg_save; }

	inline void SetDuration(float duration) { pev->dmg_take = duration; }
	inline void SetHoldTime(float hold) { pev->dmg_save = hold; }

	STATE m_iState; // LRC. Don't saverestore this value, it's not worth it.
private:
};

LINK_ENTITY_TO_CLASS(env_fade, CFade);

// pev->dmg_take is duration
// pev->dmg_save is hold duration
#define SF_FADE_IN 0x0001		// Fade in, not out
#define SF_FADE_MODULATE 0x0002 // Modulate, don't blend
#define SF_FADE_ONLYONE 0x0004
#define SF_FADE_PERMANENT 0x0008 //LRC - hold permanently
#define SF_FADE_CAMERA 0x0010	 //fading only for camera

void CFade::Spawn()
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->effects = 0;
	pev->frame = 0;

	m_iState = STATE_OFF; //LRC
}


bool CFade::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "duration"))
	{
		SetDuration(atof(pkvd->szValue));
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "holdtime"))
	{
		SetHoldTime(atof(pkvd->szValue));
		return true;
	}

	return CPointEntity::KeyValue(pkvd);
}


void CFade::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	int fadeFlags = 0;

	if ((pev->spawnflags & SF_FADE_CAMERA) != 0)
	{
		if ((pActivator == nullptr) || !pActivator->IsPlayer())
		{
			pActivator = CBaseEntity::Instance(g_engfuncs.pfnPEntityOfEntIndex(1));
		}
		if ((pActivator != nullptr) && pActivator->IsPlayer())
		{
			if (((CBasePlayer*)pActivator)->viewFlags == 0)
			{
				// 				ALERT(at_console, "player is not curently see in camera\n");
				return;
			}
		}
	}

	m_iState = STATE_TURN_ON; //LRC
	SetNextThink(Duration()); //LRC

	if ((pev->spawnflags & SF_FADE_IN) == 0)
		fadeFlags |= FFADE_OUT;

	if ((pev->spawnflags & SF_FADE_MODULATE) != 0)
		fadeFlags |= FFADE_MODULATE;

	if ((pev->spawnflags & SF_FADE_PERMANENT) != 0) //LRC
		fadeFlags |= FFADE_STAYOUT;			 //LRC

	if ((pev->spawnflags & SF_FADE_ONLYONE) != 0)
	{
		if (pActivator->IsNetClient())
		{
			UTIL_ScreenFade(pActivator, pev->rendercolor, Duration(), HoldTime(), pev->renderamt, fadeFlags);
		}
	}
	else
	{
		UTIL_ScreenFadeAll(pev->rendercolor, Duration(), HoldTime(), pev->renderamt, fadeFlags);
	}
	SUB_UseTargets(this, USE_TOGGLE, 0);
}

//LRC: a bolt-on state!
void CFade::Think()
{
	if (m_iState == STATE_TURN_ON)
	{
		m_iState = STATE_ON;
		if ((pev->spawnflags & SF_FADE_PERMANENT) == 0)
			SetNextThink(HoldTime());
	}
	else
		m_iState = STATE_OFF;
}


class CMessage : public CPointEntity
{
public:
	void Spawn() override;
	void Precache() override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	bool KeyValue(KeyValueData* pkvd) override;

private:
};

LINK_ENTITY_TO_CLASS(env_message, CMessage);


void CMessage::Spawn()
{
	Precache();

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;

	switch (pev->impulse)
	{
	case 1: // Medium radius
		pev->speed = ATTN_STATIC;
		break;

	case 2: // Large radius
		pev->speed = ATTN_NORM;
		break;

	case 3: //EVERYWHERE
		pev->speed = ATTN_NONE;
		break;

	default:
	case 0: // Small radius
		pev->speed = ATTN_IDLE;
		break;
	}
	pev->impulse = 0;

	// No volume, use normal
	if (pev->scale <= 0)
		pev->scale = 1.0;
}


void CMessage::Precache()
{
	if (pev->noise != 0u)
		PRECACHE_SOUND((char*)STRING(pev->noise));
}

bool CMessage::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "messagesound"))
	{
		pev->noise = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "messagevolume"))
	{
		pev->scale = atof(pkvd->szValue) * 0.1;
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "messageattenuation"))
	{
		pev->impulse = atoi(pkvd->szValue);
		return true;
	}
	return CPointEntity::KeyValue(pkvd);
}


void CMessage::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	CBaseEntity* pPlayer = nullptr;

	if ((pev->spawnflags & SF_MESSAGE_ALL) != 0)
		UTIL_ShowMessageAll(STRING(pev->message));
	else
	{
		if ((pActivator != nullptr) && pActivator->IsPlayer())
			pPlayer = pActivator;
		else
		{
			pPlayer = UTIL_GetLocalPlayer();
		}
		if (pPlayer != nullptr)
			UTIL_ShowMessage(STRING(pev->message), pPlayer);
	}
	if (pev->noise != 0u)
	{
		EMIT_SOUND(edict(), CHAN_BODY, STRING(pev->noise), pev->scale, pev->speed);
	}
	if ((pev->spawnflags & SF_MESSAGE_ONCE) != 0)
		UTIL_Remove(this);

	SUB_UseTargets(this, USE_TOGGLE, 0);
}



//=========================================================
// FunnelEffect
//=========================================================
class CEnvFunnel : public CBaseDelay
{
public:
	void Spawn() override;
	void Precache() override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;

	int m_iSprite; // Don't save, precache
};

void CEnvFunnel::Precache()
{
	//LRC
	if (pev->netname != 0u)
		m_iSprite = PRECACHE_MODEL((char*)STRING(pev->netname));
	else
		m_iSprite = PRECACHE_MODEL("sprites/flare6.spr");
}

LINK_ENTITY_TO_CLASS(env_funnel, CEnvFunnel);

void CEnvFunnel::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	//LRC
	Vector vecPos;
	if (pev->message != 0u)
		vecPos = CalcLocus_Position(this, pActivator, STRING(pev->message));
	else
		vecPos = pev->origin;

	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_LARGEFUNNEL);
	WRITE_COORD(vecPos.x);
	WRITE_COORD(vecPos.y);
	WRITE_COORD(vecPos.z);
	WRITE_SHORT(m_iSprite);

	if ((pev->spawnflags & SF_FUNNEL_REVERSE) != 0) // funnel flows in reverse
		WRITE_SHORT(1);
	else
		WRITE_SHORT(0);
	MESSAGE_END();

	if ((pev->spawnflags & SF_FUNNEL_REPEATABLE) == 0)
	{
		SetThink(&CEnvFunnel::SUB_Remove);
		SetNextThink(0);
	}
}

void CEnvFunnel::Spawn()
{
	Precache();
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
}


//=========================================================
// LRC -  All the particle effects from Quake 1
//=========================================================
#define SF_QUAKEFX_REPEATABLE 1
class CEnvQuakeFx : public CPointEntity
{
public:
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
};

LINK_ENTITY_TO_CLASS(env_quakefx, CEnvQuakeFx);

void CEnvQuakeFx::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	Vector vecPos;
	if (pev->message != 0u)
		vecPos = CalcLocus_Position(this, pActivator, STRING(pev->message));
	else
		vecPos = pev->origin;

	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(pev->impulse);
	WRITE_COORD(vecPos.x);
	WRITE_COORD(vecPos.y);
	WRITE_COORD(vecPos.z);
	if (pev->impulse == TE_PARTICLEBURST)
	{
		WRITE_SHORT(pev->armortype);  // radius
		WRITE_BYTE(pev->frags);		  // particle colour
		WRITE_BYTE(pev->health * 10); // duration
	}
	else if (pev->impulse == TE_EXPLOSION2)
	{
		// these fields seem to have no effect - except that it
		// crashes when I send "0" for the number of colours..
		WRITE_BYTE(0); // colour
		WRITE_BYTE(1); // number of colours
	}
	MESSAGE_END();

	if ((pev->spawnflags & SF_QUAKEFX_REPEATABLE) == 0)
	{
		SetThink(&CEnvQuakeFx::SUB_Remove);
		SetNextThink(0);
	}
}


//=========================================================
// LRC - Beam Trail effect
//=========================================================
#define SF_BEAMTRAIL_OFF 1
class CEnvBeamTrail : public CPointEntity
{
public:
	void Spawn() override;
	void Precache() override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	STATE GetState() override;
	void EXPORT StartTrailThink();
	void Affect(CBaseEntity* pTarget, USE_TYPE useType);

	int m_iSprite; // Don't save, precache
};

void CEnvBeamTrail::Precache()
{
	if (pev->target != 0u)
		PRECACHE_MODEL("sprites/null.spr");
	if (pev->netname != 0u)
		m_iSprite = PRECACHE_MODEL((char*)STRING(pev->netname));
}

LINK_ENTITY_TO_CLASS(env_beamtrail, CEnvBeamTrail);

STATE CEnvBeamTrail::GetState()
{
	if ((pev->spawnflags & SF_BEAMTRAIL_OFF) != 0)
		return STATE_OFF;
	else
		return STATE_ON;
}

void CEnvBeamTrail::StartTrailThink()
{
	pev->spawnflags |= SF_BEAMTRAIL_OFF; // fake turning off, so the Use turns it on properly
	Use(this, this, USE_ON, 0);
}

void CEnvBeamTrail::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (pev->target != 0u)
	{
		CBaseEntity* pTarget = UTIL_FindEntityByTargetname(nullptr, STRING(pev->target), pActivator);
		while (pTarget != nullptr)
		{
			Affect(pTarget, useType);
			pTarget = UTIL_FindEntityByTargetname(pTarget, STRING(pev->target), pActivator);
		}
	}
	else
	{
		if (!ShouldToggle(useType))
			return;
		Affect(this, useType);
	}

	if (useType == USE_ON)
		pev->spawnflags &= ~SF_BEAMTRAIL_OFF;
	else if (useType == USE_OFF)
		pev->spawnflags |= SF_BEAMTRAIL_OFF;
	else if (useType == USE_TOGGLE)
	{
		if ((pev->spawnflags & SF_BEAMTRAIL_OFF) != 0)
			pev->spawnflags &= ~SF_BEAMTRAIL_OFF;
		else
			pev->spawnflags |= SF_BEAMTRAIL_OFF;
	}
}

void CEnvBeamTrail::Affect(CBaseEntity* pTarget, USE_TYPE useType)
{
	if (useType == USE_ON || ((pev->spawnflags & SF_BEAMTRAIL_OFF) != 0))
	{
		MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
		WRITE_BYTE(TE_BEAMFOLLOW);
		WRITE_SHORT(pTarget->entindex()); // entity
		WRITE_SHORT(m_iSprite);			  // model
		WRITE_BYTE(pev->health * 10);	  // life
		WRITE_BYTE(pev->armorvalue);	  // width
		WRITE_BYTE(pev->rendercolor.x);	  // r, g, b
		WRITE_BYTE(pev->rendercolor.y);	  // r, g, b
		WRITE_BYTE(pev->rendercolor.z);	  // r, g, b
		WRITE_BYTE(pev->renderamt);		  // brightness
		MESSAGE_END();
	}
	else
	{
		MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
		WRITE_BYTE(TE_KILLBEAM);
		WRITE_SHORT(pTarget->entindex());
		MESSAGE_END();
	}
}

void CEnvBeamTrail::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "sprites/null.spr");
	UTIL_SetSize(pev, Vector(0, 0, 0), Vector(0, 0, 0));

	if ((pev->spawnflags & SF_BEAMTRAIL_OFF) == 0)
	{
		SetThink(&CEnvBeamTrail::StartTrailThink);
		UTIL_DesiredThink(this);
	}
}


//=========================================================
// LRC -  custom footstep sounds
//=========================================================
#define SF_FOOTSTEPS_SET 1
#define SF_FOOTSTEPS_ONCE 2

class CEnvFootsteps : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	int ObjectCaps() override { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	STATE GetState() override;
	STATE GetState(CBaseEntity* pEnt) override;
	void PrecacheNoise(const char* szNoise);
};

LINK_ENTITY_TO_CLASS(env_footsteps, CEnvFootsteps);

void CEnvFootsteps::Spawn()
{
	Precache();
}

void CEnvFootsteps::PrecacheNoise(const char* szNoise)
{
	static char szBuf[128];
	int i = 0, j = 0;
	for (i = 0; szNoise[i] != 0; i++)
	{
		if (szNoise[i] == '?')
		{
			strcpy(szBuf, szNoise);
			for (j = 0; j < 4; j++)
			{
				szBuf[i] = j + '1';
				PRECACHE_SOUND(szBuf);
			}
		}
	}
	if (j == 0)
		PRECACHE_SOUND((char*)szNoise);
}

void CEnvFootsteps::Precache()
{
	if (pev->noise != 0u)
		PrecacheNoise(STRING(pev->noise));
	if (pev->noise1 != 0u)
		PrecacheNoise(STRING(pev->noise1));
	if (pev->noise2 != 0u)
		PrecacheNoise(STRING(pev->noise2));
	if (pev->noise3 != 0u)
		PrecacheNoise(STRING(pev->noise3));
}

STATE CEnvFootsteps::GetState()
{
	if ((pev->spawnflags & SF_FOOTSTEPS_SET) != 0)
		return STATE_OFF;
	return (pev->impulse != 0) ? STATE_ON : STATE_OFF;
}

STATE CEnvFootsteps::GetState(CBaseEntity* pEnt)
{
	if ((pev->spawnflags & SF_FOOTSTEPS_SET) != 0)
		return STATE_OFF;
	if (pEnt->IsPlayer())
	{
		// based on trigger_hurt code
		int playerMask = 1 << (pEnt->entindex() - 1);

		if ((pev->impulse & playerMask) != 0)
			return STATE_ON;
		else
			return STATE_OFF;
	}
	else
		return GetState();
}

void CEnvFootsteps::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	//	union floatToString ftsTemp;

	//CONSIDER: add an "all players" spawnflag, like game_text?
	if ((pActivator != nullptr) && pActivator->IsPlayer())
	{
		int playerMask = 1 << (pActivator->entindex() - 1);

		if (((pev->spawnflags & SF_FOOTSTEPS_SET) != 0) || (((pev->impulse & playerMask) == 0) && (useType == USE_ON || useType == USE_TOGGLE)))
		{
			pev->impulse |= playerMask;
			if (pev->frags != 0.0f)
			{
				char sTemp[4];
				sprintf(sTemp, "%d", (int)pev->frags);
				g_engfuncs.pfnSetPhysicsKeyValue(pActivator->edict(), "stype", sTemp);
				//pActivator->pev->iFootstepType = pev->frags;
			}
			else if (pev->noise != 0u)
			{
				g_engfuncs.pfnSetPhysicsKeyValue(pActivator->edict(), "ssnd", STRING(pev->noise));
			}
			if (pev->noise1 != 0u)
			{
				g_engfuncs.pfnSetPhysicsKeyValue(pActivator->edict(), "lsnd", STRING(pev->noise1));
			}
			if (pev->noise2 != 0u)
			{
				g_engfuncs.pfnSetPhysicsKeyValue(pActivator->edict(), "wsnd", STRING(pev->noise2));
			}
			if (pev->noise3 != 0u)
			{
				g_engfuncs.pfnSetPhysicsKeyValue(pActivator->edict(), "psnd", STRING(pev->noise3));
			}
			// workaround for physinfo string bug: force the engine to null-terminate it
			g_engfuncs.pfnSetPhysicsKeyValue(pActivator->edict(), "x", "0");
			//ALERT(at_console, "ON, InfoString = %s\n", g_engfuncs.pfnGetPhysicsInfoString(pActivator->edict()));
			if (((pev->spawnflags & SF_FOOTSTEPS_SET) != 0) && ((pev->spawnflags & SF_FOOTSTEPS_ONCE) != 0))
			{
				UTIL_Remove(this);
			}
		}
		else if (((pev->impulse & playerMask) != 0) && (useType == USE_OFF || useType == USE_TOGGLE))
		{
			pev->impulse &= ~playerMask;
			if (pev->frags != 0.0f)
			{
				g_engfuncs.pfnSetPhysicsKeyValue(pActivator->edict(), "stype", "0");
			}
			else if (pev->noise != 0u)
			{
				g_engfuncs.pfnSetPhysicsKeyValue(pActivator->edict(), "ssnd", "0");
			}
			if (pev->noise1 != 0u)
			{
				g_engfuncs.pfnSetPhysicsKeyValue(pActivator->edict(), "lsnd", "0");
			}
			if (pev->noise2 != 0u)
			{
				g_engfuncs.pfnSetPhysicsKeyValue(pActivator->edict(), "wsnd", "0");
			}
			if (pev->noise3 != 0u)
			{
				g_engfuncs.pfnSetPhysicsKeyValue(pActivator->edict(), "psnd", "0");
			}
			// workaround for physinfo string bug: force the engine to null-terminate it
			g_engfuncs.pfnSetPhysicsKeyValue(pActivator->edict(), "x", "0");
			//ALERT(at_console, "OFF, InfoString = %s\n", g_engfuncs.pfnGetPhysicsInfoString(pActivator->edict()));
			if ((pev->spawnflags & SF_FOOTSTEPS_ONCE) != 0)
			{
				UTIL_Remove(this);
			}
		}
		else
		{
			//ALERT(at_console, "NO EFFECT\n");
		}
	}
}

//==================================================================
//LRC- Xen monsters' warp-in effect, for those too lazy to build it. :)
//==================================================================
class CEnvWarpBall : public CBaseEntity
{
public:
	void Precache() override;
	void Spawn() override { Precache(); }
	void Think() override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	int ObjectCaps() override { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
};

LINK_ENTITY_TO_CLASS(env_warpball, CEnvWarpBall);

void CEnvWarpBall::Precache()
{
	PRECACHE_MODEL("sprites/lgtning.spr");
	PRECACHE_MODEL("sprites/Fexplo1.spr");
	PRECACHE_MODEL("sprites/XFlare1.spr");
	PRECACHE_SOUND("debris/beamstart2.wav");
	PRECACHE_SOUND("debris/beamstart7.wav");
}

void CEnvWarpBall::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	int iTimes = 0;
	int iDrawn = 0;
	TraceResult tr;
	Vector vecDest;
	CBeam* pBeam;
	while (iDrawn < pev->frags && iTimes < (pev->frags * 3)) // try to draw <frags> beams, but give up after 3x<frags> tries.
	{
		vecDest = pev->health * (Vector(RANDOM_FLOAT(-1, 1), RANDOM_FLOAT(-1, 1), RANDOM_FLOAT(-1, 1)).Normalize());
		UTIL_TraceLine(pev->origin, pev->origin + vecDest, ignore_monsters, nullptr, &tr);
		if (tr.flFraction != 1.0)
		{
			// we hit something.
			iDrawn++;
			pBeam = CBeam::BeamCreate("sprites/lgtning.spr", 200);
			pBeam->PointsInit(pev->origin, tr.vecEndPos);
			pBeam->SetColor(197, 243, 169);
			pBeam->SetNoise(65);
			pBeam->SetBrightness(150);
			pBeam->SetWidth(18);
			pBeam->SetScrollRate(35);
			pBeam->SetThink(&CEnvWarpBall::SUB_Remove);
			pBeam->SetNextThink(1);
		}
		iTimes++;
	}
	EMIT_SOUND(edict(), CHAN_BODY, "debris/beamstart2.wav", 1, ATTN_NORM);

	CSprite* pSpr = CSprite::SpriteCreate("sprites/Fexplo1.spr", pev->origin, true);
	pSpr->AnimateAndDie(10);
	pSpr->SetTransparency(kRenderGlow, 77, 210, 130, 255, kRenderFxNoDissipation);

	pSpr = CSprite::SpriteCreate("sprites/XFlare1.spr", pev->origin, true);
	pSpr->AnimateAndDie(10);
	pSpr->SetTransparency(kRenderGlow, 184, 250, 214, 255, kRenderFxNoDissipation);

	SetNextThink(0.5);
}

void CEnvWarpBall::Think()
{
	EMIT_SOUND(edict(), CHAN_ITEM, "debris/beamstart7.wav", 1, ATTN_NORM);
	SUB_UseTargets(this, USE_TOGGLE, 0);
}

//==================================================================
//LRC- Shockwave effect, like when a Houndeye attacks.
//==================================================================
#define SF_SHOCKWAVE_CENTERED 1
#define SF_SHOCKWAVE_REPEATABLE 2

class CEnvShockwave : public CPointEntity
{
public:
	void Precache() override;
	void Spawn() override { Precache(); }
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	bool KeyValue(KeyValueData* pkvd) override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	void DoEffect(Vector vecPos);

	int m_iTime;
	int m_iRadius;
	int m_iHeight;
	int m_iScrollRate;
	int m_iNoise;
	int m_iFrameRate;
	int m_iStartFrame;
	int m_iSpriteTexture;
	char m_cType;
	int m_iszPosition;
};

LINK_ENTITY_TO_CLASS(env_shockwave, CEnvShockwave);

TYPEDESCRIPTION CEnvShockwave::m_SaveData[] =
	{
		DEFINE_FIELD(CEnvShockwave, m_iHeight, FIELD_INTEGER),
		DEFINE_FIELD(CEnvShockwave, m_iTime, FIELD_INTEGER),
		DEFINE_FIELD(CEnvShockwave, m_iRadius, FIELD_INTEGER),
		DEFINE_FIELD(CEnvShockwave, m_iScrollRate, FIELD_INTEGER),
		DEFINE_FIELD(CEnvShockwave, m_iNoise, FIELD_INTEGER),
		DEFINE_FIELD(CEnvShockwave, m_iFrameRate, FIELD_INTEGER),
		DEFINE_FIELD(CEnvShockwave, m_iStartFrame, FIELD_INTEGER),
		DEFINE_FIELD(CEnvShockwave, m_iSpriteTexture, FIELD_INTEGER),
		DEFINE_FIELD(CEnvShockwave, m_cType, FIELD_CHARACTER),
		DEFINE_FIELD(CEnvShockwave, m_iszPosition, FIELD_STRING),
};

IMPLEMENT_SAVERESTORE(CEnvShockwave, CBaseEntity);

void CEnvShockwave::Precache()
{
	m_iSpriteTexture = PRECACHE_MODEL((char*)STRING(pev->netname));
}

bool CEnvShockwave::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "m_iTime"))
	{
		m_iTime = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iRadius"))
	{
		m_iRadius = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iHeight"))
	{
		m_iHeight = atoi(pkvd->szValue) / 2; //LRC- the actual height is doubled when drawn
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iScrollRate"))
	{
		m_iScrollRate = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iNoise"))
	{
		m_iNoise = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iFrameRate"))
	{
		m_iFrameRate = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iStartFrame"))
	{
		m_iStartFrame = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszPosition"))
	{
		m_iszPosition = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_cType"))
	{
		m_cType = atoi(pkvd->szValue);
		return true;
	}
	return CBaseEntity::KeyValue(pkvd);
}

void CEnvShockwave::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	Vector vecPos;
	if (m_iszPosition != 0)
		vecPos = CalcLocus_Position(this, pActivator, STRING(m_iszPosition));
	else
		vecPos = pev->origin;

	if ((pev->spawnflags & SF_SHOCKWAVE_CENTERED) == 0)
		vecPos.z += m_iHeight;

	if (pev->target != 0u)
		FireTargets(STRING(pev->target), pActivator, pCaller, useType, value); //AJH

	// blast circle
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
	if (m_cType != 0)
		WRITE_BYTE(m_cType);
	else
		WRITE_BYTE(TE_BEAMCYLINDER);
	WRITE_COORD(vecPos.x); // coord coord coord (center position)
	WRITE_COORD(vecPos.y);
	WRITE_COORD(vecPos.z);
	WRITE_COORD(vecPos.x); // coord coord coord (axis and radius)
	WRITE_COORD(vecPos.y);
	WRITE_COORD(vecPos.z + m_iRadius);
	WRITE_SHORT(m_iSpriteTexture);	// short (sprite index)
	WRITE_BYTE(m_iStartFrame);		// byte (starting frame)
	WRITE_BYTE(m_iFrameRate);		// byte (frame rate in 0.1's)
	WRITE_BYTE(m_iTime);			// byte (life in 0.1's)
	WRITE_BYTE(m_iHeight);			// byte (line width in 0.1's)
	WRITE_BYTE(m_iNoise);			// byte (noise amplitude in 0.01's)
	WRITE_BYTE(pev->rendercolor.x); // byte,byte,byte (color)
	WRITE_BYTE(pev->rendercolor.y);
	WRITE_BYTE(pev->rendercolor.z);
	WRITE_BYTE(pev->renderamt); // byte (brightness)
	WRITE_BYTE(m_iScrollRate);	// byte (scroll speed in 0.1's)
	MESSAGE_END();

	if ((pev->spawnflags & SF_SHOCKWAVE_REPEATABLE) == 0)
	{
		SetThink(&CEnvShockwave::SUB_Remove);
		SetNextThink(0);
	}
}

//=========================================================
// Beverage Dispenser
// overloaded pev->frags, is now a flag for whether or not a can is stuck in the dispenser.
// overloaded pev->health, is now how many cans remain in the machine.
//=========================================================
class CEnvBeverage : public CBaseDelay
{
public:
	void Spawn() override;
	void Precache() override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;

	// it's 'on' while there are cans left
	STATE GetState() override { return (pev->health > 0) ? STATE_ON : STATE_OFF; };
};

void CEnvBeverage::Precache()
{
	PRECACHE_MODEL("models/can.mdl");
	PRECACHE_SOUND("weapons/g_bounce3.wav");
}

LINK_ENTITY_TO_CLASS(env_beverage, CEnvBeverage);

void CEnvBeverage::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (pev->frags != 0 || pev->health <= 0)
	{
		// no more cans while one is waiting in the dispenser, or if I'm out of cans.
		return;
	}

	Vector vecPos;
	if (pev->target != 0u)
		vecPos = CalcLocus_Position(this, pActivator, STRING(pev->target));
	else
		vecPos = pev->origin;

	CBaseEntity* pCan = CBaseEntity::Create("item_sodacan", vecPos, pev->angles, edict());

	if (pev->skin == 6)
	{
		// random
		pCan->pev->skin = RANDOM_LONG(0, 5);
	}
	else
	{
		pCan->pev->skin = pev->skin;
	}

	pev->frags = 1;
	pev->health--;
}

void CEnvBeverage::Spawn()
{
	Precache();
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->frags = 0;

	if (pev->health == 0)
	{
		pev->health = 10;
	}
}

//=========================================================
// Soda can
//=========================================================
class CItemSoda : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	void EXPORT CanThink();
	void EXPORT CanTouch(CBaseEntity* pOther);
};

void CItemSoda::Precache()
{
	// added for Nemo1024  --LRC
	PRECACHE_MODEL("models/can.mdl");
	PRECACHE_SOUND("weapons/g_bounce3.wav");
}

LINK_ENTITY_TO_CLASS(item_sodacan, CItemSoda);

void CItemSoda::Spawn()
{
	Precache();
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_TOSS;

	SET_MODEL(ENT(pev), "models/can.mdl");
	UTIL_SetSize(pev, Vector(0, 0, 0), Vector(0, 0, 0));

	SetThink(&CItemSoda::CanThink);
	SetNextThink(0.5);
}

void CItemSoda::CanThink()
{
	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/g_bounce3.wav", 1, ATTN_NORM);

	pev->solid = SOLID_TRIGGER;
	UTIL_SetSize(pev, Vector(-8, -8, 0), Vector(8, 8, 8));
	SetThink(nullptr);
	SetTouch(&CItemSoda::CanTouch);
}

void CItemSoda::CanTouch(CBaseEntity* pOther)
{
	if (!pOther->IsPlayer())
	{
		return;
	}

	// spoit sound here

	pOther->TakeHealth(1, DMG_GENERIC); // a bit of health.

	if (!FNullEnt(pev->owner))
	{
		// tell the machine the can was taken
		pev->owner->v.frags = 0;
	}

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->effects = EF_NODRAW;
	SetTouch(nullptr);
	SetThink(&CItemSoda::SUB_Remove);
	SetNextThink(0);
}

// RENDERERS START
//=======================
//  ClientFog
//=======================
#define SF_FOG_STARTON 1

CClientFog* CClientFog::FogCreate()
{
	CClientFog* pFog = GetClassPtr((CClientFog*)nullptr);
	pFog->pev->classname = MAKE_STRING("env_fog");
	pFog->Spawn();

	return pFog;
}
bool CClientFog::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "startdist"))
	{
		m_iStartDist = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "enddist"))
	{
		m_iEndDist = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "affectsky"))
	{
		m_bDontAffectSky = (bool)atoi(pkvd->szValue);
		return true;
	}
	else
		return CBaseEntity::KeyValue(pkvd);
}

void CClientFog::Spawn()
{
	pev->effects |= EF_NODRAW;

	if (FStringNull(pev->targetname))
		pev->spawnflags |= 1;

	if ((pev->spawnflags & SF_FOG_STARTON) != 0)
		m_fActive = true;
}
void CClientFog::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	switch (useType)
	{
	case USE_OFF:
		m_fActive = false;
		break;
	case USE_ON:
		m_fActive = true;
		break;
	default:
		m_fActive = !m_fActive;
		break;
	}

	SendInitMessage(nullptr);
};
void CClientFog::SendInitMessage(CBasePlayer* player)
{
	if ((player != nullptr) && !m_fActive)
		return;

	if (m_fActive)
	{
		if (player != nullptr)
			MESSAGE_BEGIN(MSG_ONE, gmsgSetFog, nullptr, player->pev);
		else
			MESSAGE_BEGIN(MSG_ALL, gmsgSetFog, nullptr);

		WRITE_SHORT(pev->rendercolor.x);
		WRITE_SHORT(pev->rendercolor.y);
		WRITE_SHORT(pev->rendercolor.z);
		WRITE_SHORT(m_iStartDist);
		WRITE_SHORT(m_iEndDist);
		WRITE_SHORT(static_cast<int>(m_bDontAffectSky));
		MESSAGE_END();
	}
	else
	{
		if (player != nullptr)
			MESSAGE_BEGIN(MSG_ONE, gmsgSetFog, nullptr, player->pev);
		else
			MESSAGE_BEGIN(MSG_ALL, gmsgSetFog, nullptr);

		WRITE_SHORT(0);
		WRITE_SHORT(0);
		WRITE_SHORT(0);
		WRITE_SHORT(0);
		WRITE_SHORT(0);
		WRITE_SHORT(0);
		MESSAGE_END();
	}
}
LINK_ENTITY_TO_CLASS(env_fog, CClientFog);

TYPEDESCRIPTION CClientFog::m_SaveData[] =
	{
		DEFINE_FIELD(CClientFog, m_fActive, FIELD_BOOLEAN),
		DEFINE_FIELD(CClientFog, m_iStartDist, FIELD_INTEGER),
		DEFINE_FIELD(CClientFog, m_iEndDist, FIELD_INTEGER),
		DEFINE_FIELD(CClientFog, m_bDontAffectSky, FIELD_BOOLEAN),
};

IMPLEMENT_SAVERESTORE(CClientFog, CBaseEntity);
//=========================================================
// Generic Item
//=========================================================
class CItemGeneric : public CBaseAnimating
{
public:
	void Spawn() override;
	void Precache() override;
	bool KeyValue(KeyValueData* pkvd) override;

	int ObjectCaps() override { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	bool m_fDisableShadows;
	bool m_fDisableDrawing;
};

LINK_ENTITY_TO_CLASS(item_generic, CItemGeneric);
LINK_ENTITY_TO_CLASS(item_prop, CItemGeneric);
LINK_ENTITY_TO_CLASS(prop_physics, CItemGeneric);

TYPEDESCRIPTION CItemGeneric::m_SaveData[] =
	{
		DEFINE_FIELD(CItemGeneric, m_fDisableShadows, FIELD_BOOLEAN),
		DEFINE_FIELD(CItemGeneric, m_fDisableDrawing, FIELD_BOOLEAN),
};

IMPLEMENT_SAVERESTORE(CItemGeneric, CBaseAnimating);

bool CItemGeneric::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "DisableShadows"))
	{
		m_fDisableShadows = (bool)atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "DisableDrawing"))
	{
		m_fDisableDrawing = (bool)atoi(pkvd->szValue);
		return true;
	}
	else
		return CBaseAnimating::KeyValue(pkvd);
}

void CItemGeneric::Precache()
{
	PRECACHE_MODEL((char*)STRING(pev->model));
}

void CItemGeneric::Spawn()
{
	if ((pev->targetname != 0u) || (strcmp(STRING(pev->classname), "prop_physics") == 0))
	{
		Precache();
		SET_MODEL(ENT(pev), STRING(pev->model));
	}
	else
	{
		UTIL_Remove(this);
	}

	pev->movetype = MOVETYPE_NONE;

	if (m_fDisableShadows == true)
		pev->effects |= FL_NOSHADOW;

	if (m_fDisableDrawing == true)
		pev->effects |= FL_NOMODEL;

	pev->framerate = 1.0;
}
//===============================================
// Dynamic Light - Used to create dynamic lights in
// the level. Can be either entity, world or shadow
// light.
//===============================================

#define SF_DYNLIGHT_STARTON 1
#define SF_DYNLIGHT_NOPVS 2

class CDynamicLight : public CPointEntity
{
public:
	void Spawn() override;
	void Precache() override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;

	void EXPORT LightThink();
};

LINK_ENTITY_TO_CLASS(env_elight, CDynamicLight);
LINK_ENTITY_TO_CLASS(env_dlight, CDynamicLight);
LINK_ENTITY_TO_CLASS(env_spotlight, CDynamicLight);
void CDynamicLight::Spawn()
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;

	if (FStringNull(pev->targetname) && FClassnameIs(pev, "env_elight"))
	{
		UTIL_Remove(this);
		return;
	}

	if (FStringNull(pev->targetname))
		pev->spawnflags |= SF_DYNLIGHT_STARTON;

	if ((pev->spawnflags & SF_DYNLIGHT_STARTON) == 0)
		pev->effects |= EF_NODRAW;

	if (FClassnameIs(pev, "env_elight"))
		pev->effects |= FL_ELIGHT; // entity light

	if (FClassnameIs(pev, "env_dlight"))
		pev->effects |= FL_DLIGHT; // dynamic light

	if (FClassnameIs(pev, "env_spotlight"))
		pev->effects = FL_SPOTLIGHT;

	if (FClassnameIs(pev, "env_elight") && !FStringNull(pev->target))
	{
		edict_t* pentFind = FIND_ENTITY_BY_TARGETNAME(nullptr, STRING(pev->target));
		pev->aiment = pentFind;
		pev->skin = pev->impulse; // Attachment point;
	}

	Precache();
	SET_MODEL(ENT(pev), "sprites/null.spr"); // should be visible to send to client
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
}
void CDynamicLight::Precache()
{
	PRECACHE_MODEL("sprites/null.spr");
}
void CDynamicLight::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (useType == USE_ON)
		pev->effects &= ~EF_NODRAW;
	else if (useType == USE_OFF)
		pev->effects |= EF_NODRAW;
	else if (useType == USE_TOGGLE)
	{
		if ((pev->effects & EF_NODRAW) != 0)
			pev->effects &= ~EF_NODRAW;
		else
			pev->effects |= EF_NODRAW;
	}

	if ((pev->effects & EF_NODRAW) == 0)
	{
		if (FClassnameIs(pev, "env_elight") && !FStringNull(pev->target))
		{
			edict_t* pentFind = FIND_ENTITY_BY_TARGETNAME(nullptr, STRING(pev->target));
			if (ENTINDEX(pentFind) != 0)
			{
				pev->aiment = pentFind;
				pev->skin = pev->impulse; // Attachment point;
			}
			else
			{
				SetThink(&CDynamicLight::LightThink);
				pev->nextthink = gpGlobals->time + 0.5;
			}
		}
	}
};
void CDynamicLight::LightThink()
{
	edict_t* pentFind = FIND_ENTITY_BY_TARGETNAME(nullptr, STRING(pev->target));

	if (ENTINDEX(pentFind) == 0)
	{
		SetThink(&CDynamicLight::LightThink);
		pev->nextthink = gpGlobals->time + 0.5;
	}
	else
	{
		pev->aiment = pentFind;
		pev->skin = pev->impulse; // Attachment point;
	}
}

// =================================
// buz: 3d sky info messages
//
// envpos_sky: sets view origin in 3d sky
//
// envpos_world: sets view origin in world (when sky movement requed)
//
// =================================
class CEnvPos_Sky : public CPointEntity
{
public:
	void Spawn() override;
	void SendInitMessage(CBasePlayer* player) override;
	bool KeyValue(KeyValueData* pkvd) override;

	int ObjectCaps() override { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

public:
	int m_iStartDist;
	int m_iEndDist;
	bool m_bDontAffectSky;
};
LINK_ENTITY_TO_CLASS(envpos_sky, CEnvPos_Sky);

TYPEDESCRIPTION CEnvPos_Sky::m_SaveData[] =
	{
		DEFINE_FIELD(CEnvPos_Sky, m_iStartDist, FIELD_INTEGER),
		DEFINE_FIELD(CEnvPos_Sky, m_iEndDist, FIELD_INTEGER),
		DEFINE_FIELD(CEnvPos_Sky, m_bDontAffectSky, FIELD_BOOLEAN),
};

IMPLEMENT_SAVERESTORE(CEnvPos_Sky, CPointEntity);

void CEnvPos_Sky::Spawn()
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->effects |= EF_NODRAW;
}

bool CEnvPos_Sky::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "enddist"))
	{
		m_iEndDist = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "startdist"))
	{
		m_iStartDist = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "affectsky"))
	{
		m_bDontAffectSky = (atoi(pkvd->szValue) != 0);
		return true;
	}
	else
	{
		return CPointEntity::KeyValue(pkvd);
	}
}

void CEnvPos_Sky::SendInitMessage(CBasePlayer* player)
{
	MESSAGE_BEGIN(MSG_ONE, gmsgSkyMark_Sky, nullptr, player->pev);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_SHORT(m_iEndDist);
	WRITE_SHORT(m_iStartDist);
	WRITE_BYTE(pev->rendercolor.x);
	WRITE_BYTE(pev->rendercolor.y);
	WRITE_BYTE(pev->rendercolor.z);
	WRITE_SHORT(static_cast<int>(m_bDontAffectSky));
	MESSAGE_END();
}

class CEnvPos_World : public CPointEntity
{
public:
	void Spawn() override
	{
		pev->solid = SOLID_NOT;
		pev->movetype = MOVETYPE_NONE;
		pev->effects |= EF_NODRAW;
	}

	void SendInitMessage(CBasePlayer* player) override
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgSkyMark_World, nullptr, player->pev);
		WRITE_COORD(pev->origin.x);
		WRITE_COORD(pev->origin.y);
		WRITE_COORD(pev->origin.z);
		WRITE_COORD(pev->health);
		MESSAGE_END();
	}
};

LINK_ENTITY_TO_CLASS(envpos_world, CEnvPos_World);

/*
====================
stristr

====================
*/
char* stristr(const char* string, const char* string2)
{
	int c, len;
	c = tolower(*string2);
	len = strlen(string2);

	while (string != nullptr)
	{
		for (; (*string != 0) && tolower(*string) != c; string++)
			;
		if (*string != 0)
		{
			if (strnicmp(string, string2, len) == 0)
			{
				break;
			}
			string++;
		}
		else
		{
			return nullptr;
		}
	}
	return (char*)string;
}

//===============================================
// Custom Decal
//===============================================

#define MAX_PATH_LENGTH 32
#define SF_DECAL_WAITTRIGGER 1

class CEnvDecal : public CPointEntity
{
public:
	void Spawn() override;
	bool KeyValue(KeyValueData* pkvd) override;
	void SendInitMessage(CBasePlayer* player) override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	bool m_bActive;

	Vector m_vImpactNormal;
	Vector m_vImpactPosition;

	char m_szDecalName[MAX_PATH_LENGTH];
	char m_szDecalOrigName[MAX_PATH_LENGTH];
};

TYPEDESCRIPTION CEnvDecal::m_SaveData[] =
	{
		DEFINE_FIELD(CEnvDecal, m_bActive, FIELD_BOOLEAN),
		DEFINE_FIELD(CEnvDecal, m_vImpactPosition, FIELD_VECTOR),
		DEFINE_FIELD(CEnvDecal, m_vImpactNormal, FIELD_VECTOR),
		DEFINE_ARRAY(CEnvDecal, m_szDecalName, FIELD_CHARACTER, MAX_PATH_LENGTH),
};
IMPLEMENT_SAVERESTORE(CEnvDecal, CBaseEntity);

LINK_ENTITY_TO_CLASS(env_decal, CEnvDecal);

void CEnvDecal::Spawn()
{
	TraceResult tr;

	Vector temp;
	Vector angles;
	Vector forward;
	Vector right;
	Vector up;

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->effects = EF_NODRAW;

	if (FStringNull(pev->targetname))
		return;

	strcpy(m_szDecalName, STRING(pev->message));

	if (strlen(m_szDecalName) == NULL)
		UTIL_Remove(this);

	// Z AXIS
	UTIL_TraceLine(pev->origin + Vector(0, 0, 10), pev->origin + Vector(0, 0, -10), ignore_monsters, edict(), &tr);

	if (tr.flFraction == 1.0)
		UTIL_TraceLine(pev->origin + Vector(0, 0, -10), pev->origin + Vector(0, 0, 10), ignore_monsters, edict(), &tr);


	// Y AXIS
	if (tr.flFraction == 1.0)
		UTIL_TraceLine(pev->origin + Vector(0, -10, 0), pev->origin + Vector(0, 10, 0), ignore_monsters, edict(), &tr);

	if (tr.flFraction == 1.0)
		UTIL_TraceLine(pev->origin + Vector(0, 10, 0), pev->origin + Vector(0, -10, 0), ignore_monsters, edict(), &tr);


	// X AXIS
	if (tr.flFraction == 1.0)
		UTIL_TraceLine(pev->origin + Vector(10, 0, 0), pev->origin + Vector(-10, 0, 0), ignore_monsters, edict(), &tr);

	if (tr.flFraction == 1.0)
		UTIL_TraceLine(pev->origin + Vector(-10, 0, 0), pev->origin + Vector(10, 0, 0), ignore_monsters, edict(), &tr);

	if (tr.flFraction == 1.0)
		UTIL_Remove(this);

	m_vImpactPosition = tr.vecEndPos;
	m_vImpactNormal = tr.vecPlaneNormal;
}
void CEnvDecal::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	m_bActive = true;
	SendInitMessage(nullptr);
};
void CEnvDecal::SendInitMessage(CBasePlayer* player)
{
	if (!m_bActive)
		return;

	if (player == nullptr)
		MESSAGE_BEGIN(MSG_ALL, gmsgCreateDecal, nullptr);
	else
		MESSAGE_BEGIN(MSG_ONE, gmsgCreateDecal, nullptr, player->pev);

	WRITE_COORD(m_vImpactPosition.x);
	WRITE_COORD(m_vImpactPosition.y);
	WRITE_COORD(m_vImpactPosition.z);
	WRITE_COORD(m_vImpactNormal.x);
	WRITE_COORD(m_vImpactNormal.y);
	WRITE_COORD(m_vImpactNormal.z);
	WRITE_BYTE(1);
	WRITE_STRING(m_szDecalName);
	MESSAGE_END();
}
bool CEnvDecal::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "texture"))
	{
		strcpy(m_szDecalOrigName, pkvd->szValue);
		return true;
	}
	else
		return CBaseEntity::KeyValue(pkvd);
}


#define SF_PARTICLE_STARTON 1
#define SF_PARTICLE_KILLFIRE 2

class CEnvParticle : public CPointEntity
{
public:
	void Spawn() override;
	bool KeyValue(KeyValueData* pkvd) override;
	void SendInitMessage(CBasePlayer* player) override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	void EXPORT ParticleThink();
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	bool m_bActive;
	bool m_bSent;
};

TYPEDESCRIPTION CEnvParticle::m_SaveData[] =
	{
		DEFINE_FIELD(CEnvParticle, m_bActive, FIELD_BOOLEAN),
};
IMPLEMENT_SAVERESTORE(CEnvParticle, CBaseEntity);

LINK_ENTITY_TO_CLASS(env_particle_system, CEnvParticle);

void CEnvParticle::Spawn()
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->effects = EF_NODRAW;

	if (FStringNull(pev->targetname) || ((pev->spawnflags & SF_PARTICLE_STARTON) != 0))
		m_bActive = true;
}

bool CEnvParticle::KeyValue(KeyValueData* pkvd)
{
	return CBaseEntity::KeyValue(pkvd);
}

void CEnvParticle::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	switch (useType)
	{
	case USE_OFF:
		m_bActive = true;
		break;
	case USE_ON:
		m_bActive = true;
		break;
	default:
		if (m_bActive)
			m_bActive = false;
		else if (!m_bActive)
			m_bActive = true;

		break;
	}

	m_bSent = false;
	SendInitMessage(nullptr);
};
void CEnvParticle::SendInitMessage(CBasePlayer* player)
{
	if (m_bActive && m_bSent)
		return;

	// Use think function, otherwise it isn't recieved
	SetThink(&CEnvParticle::ParticleThink);
	pev->nextthink = gpGlobals->time + 0.01;
}

void CEnvParticle ::ParticleThink()
{
	if (!m_bActive)
	{
		MESSAGE_BEGIN(MSG_ALL, gmsgCreateSystem, nullptr);
		WRITE_COORD(NULL);
		WRITE_COORD(NULL);
		WRITE_COORD(NULL);
		WRITE_COORD(NULL);
		WRITE_COORD(NULL);
		WRITE_COORD(NULL);
		WRITE_BYTE(2);
		WRITE_STRING(nullptr);
		WRITE_LONG(this->entindex());
		MESSAGE_END();
	}
	else
	{
		Vector vForward;
		g_engfuncs.pfnAngleVectors(pev->angles, vForward, nullptr, nullptr);
		MESSAGE_BEGIN(MSG_ALL, gmsgCreateSystem, nullptr);
		WRITE_COORD(pev->origin.x); // system origin
		WRITE_COORD(pev->origin.y);
		WRITE_COORD(pev->origin.z);
		WRITE_COORD(vForward.x); // system angles
		WRITE_COORD(vForward.y);
		WRITE_COORD(vForward.z);
		WRITE_BYTE(pev->frags);				// definition = 0; cluster = 1;
		WRITE_STRING(STRING(pev->message)); // path to definitions file
		WRITE_LONG(this->entindex());
		MESSAGE_END();
	}

	m_bSent = true;

	if (m_bActive && ((pev->spawnflags & SF_PARTICLE_KILLFIRE) != 0))
		UTIL_Remove(this);
}
// RENDERERS END
