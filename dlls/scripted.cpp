/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/
/*


===== scripted.cpp ========================================================

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "player.h"
#include "animation.h"
#include "saverestore.h"
#include "schedule.h"
#include "scripted.h"
#include "defaultai.h"
#include "movewith.h"



/*
classname "scripted_sequence"
targetname "me" - there can be more than one with the same name, and they act in concert
target "the_entity_I_want_to_start_playing" or "class entity_classname" will pick the closest inactive scientist
play "name_of_sequence"
idle "name of idle sequence to play before starting"
donetrigger "whatever" - can be any other triggerable entity such as another sequence, train, door, or a special case like "die" or "remove"
moveto - if set the monster first moves to this nodes position
range # - only search this far to find the target
spawnflags - (stop if blocked, stop if player seen)
*/


//
// Cache user-entity-field values until spawn is called.
//

bool CCineMonster::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "m_iszIdle"))
	{
		m_iszIdle = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszPlay"))
	{
		m_iszPlay = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszEntity"))
	{
		m_iszEntity = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszAttack"))
	{
		m_iszAttack = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszMoveTarget"))
	{
		m_iszMoveTarget = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszFireOnBegin"))
	{
		m_iszFireOnBegin = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_fMoveTo"))
	{
		m_fMoveTo = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_fTurnType"))
	{
		m_fTurnType = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_fAction"))
	{
		m_fAction = atoi(pkvd->szValue);
		return true;
	}
	// LRC	else if (FStrEq(pkvd->szKeyName, "m_flRepeat"))
	//	{
	//		m_flRepeat = atof( pkvd->szValue );
	//		return true;
	//	}
	else if (FStrEq(pkvd->szKeyName, "m_flRadius"))
	{
		m_flRadius = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iRepeats"))
	{
		m_iRepeats = atoi(pkvd->szValue);
		m_iRepeatsLeft = m_iRepeats;
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_fRepeatFrame"))
	{
		m_fRepeatFrame = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iFinishSchedule"))
	{
		m_iFinishSchedule = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iPriority"))
	{
		m_iPriority = atoi(pkvd->szValue);
		return true;
	}

	return CBaseMonster::KeyValue(pkvd);
}

TYPEDESCRIPTION CCineMonster::m_SaveData[] =
	{
		DEFINE_FIELD(CCineMonster, m_iState, FIELD_INTEGER), //LRC
		DEFINE_FIELD(CCineMonster, m_iszIdle, FIELD_STRING),
		DEFINE_FIELD(CCineMonster, m_iszPlay, FIELD_STRING),
		DEFINE_FIELD(CCineMonster, m_iszEntity, FIELD_STRING),
		DEFINE_FIELD(CCineMonster, m_iszAttack, FIELD_STRING),	   //LRC
		DEFINE_FIELD(CCineMonster, m_iszMoveTarget, FIELD_STRING), //LRC
		DEFINE_FIELD(CCineMonster, m_iszFireOnBegin, FIELD_STRING),
		DEFINE_FIELD(CCineMonster, m_fMoveTo, FIELD_INTEGER),
		DEFINE_FIELD(CCineMonster, m_fTurnType, FIELD_INTEGER),
		DEFINE_FIELD(CCineMonster, m_fAction, FIELD_INTEGER),
		//LRC- this is unused	DEFINE_FIELD( CCineMonster, m_flRepeat, FIELD_FLOAT ),
		DEFINE_FIELD(CCineMonster, m_flRadius, FIELD_FLOAT),

		DEFINE_FIELD(CCineMonster, m_iDelay, FIELD_INTEGER),
		DEFINE_FIELD(CCineMonster, m_startTime, FIELD_TIME),

		DEFINE_FIELD(CCineMonster, m_saved_movetype, FIELD_INTEGER),
		DEFINE_FIELD(CCineMonster, m_saved_solid, FIELD_INTEGER),
		DEFINE_FIELD(CCineMonster, m_saved_effects, FIELD_INTEGER),
		DEFINE_FIELD(CCineMonster, m_iFinishSchedule, FIELD_INTEGER),
		DEFINE_FIELD(CCineMonster, m_interruptable, FIELD_BOOLEAN),

		//LRC
		DEFINE_FIELD(CCineMonster, m_iRepeats, FIELD_INTEGER),
		DEFINE_FIELD(CCineMonster, m_iRepeatsLeft, FIELD_INTEGER),
		DEFINE_FIELD(CCineMonster, m_fRepeatFrame, FIELD_FLOAT),
		DEFINE_FIELD(CCineMonster, m_iPriority, FIELD_INTEGER),
};


IMPLEMENT_SAVERESTORE(CCineMonster, CBaseMonster);

LINK_ENTITY_TO_CLASS(scripted_sequence, CCineMonster);
LINK_ENTITY_TO_CLASS(scripted_action, CCineMonster); //LRC

LINK_ENTITY_TO_CLASS(aiscripted_sequence, CCineMonster); //LRC - aiscripted sequences don't need to be seperate


void CCineMonster::Spawn()
{
	// pev->solid = SOLID_TRIGGER;
	// UTIL_SetSize(pev, Vector(-8, -8, -8), Vector(8, 8, 8));
	pev->solid = SOLID_NOT;

	m_iState = STATE_OFF; //LRC

	if (FStringNull(m_iszIdle) && FStringNull(pev->targetname)) // if no targetname, start now
	{
		SetThink(&CCineMonster::CineThink);
		SetNextThink(1.0);
	}
	else if (!FStringNull(m_iszIdle))
	{
		SetThink(&CCineMonster::InitIdleThink);
		SetNextThink(1.0);
	}
	if ((pev->spawnflags & SF_SCRIPT_NOINTERRUPT) != 0)
		m_interruptable = false;
	else
		m_interruptable = true;

	//LRC - the only difference between AI and normal sequences
	if (FClassnameIs(pev, "aiscripted_sequence") || (pev->spawnflags & SF_SCRIPT_OVERRIDESTATE) != 0)
	{
		m_iPriority |= SS_INTERRUPT_ANYSTATE;
	}
}

//
// CineStart
//
void CCineMonster::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	// do I already know who I should use
	CBaseEntity* pEntity = m_hTargetEnt;
	CBaseMonster* pTarget = nullptr;

	if (pEntity != nullptr)
		pTarget = pEntity->MyMonsterPointer();

	if (pTarget != nullptr)
	{
		//		ALERT(at_console, "Sequence \"%s\" triggered, already has a target.\n", STRING(pev->targetname));
		// am I already playing the script?
		if (pTarget->m_scriptState == SCRIPT_PLAYING)
			return;

		m_startTime = gpGlobals->time + 0.05; //why the delay? -- LRC
	}
	else
	{
		//		ALERT(at_console, "Sequence \"%s\" triggered, can't find target; searching\n", STRING(pev->targetname));
		m_hActivator = pActivator;
		// if not, try finding them
		SetThink(&CCineMonster::CineThink);
		//		SetNextThink( 0 );
		CineThink(); //LRC
	}
}


// This doesn't really make sense since only MOVETYPE_PUSH get 'Blocked' events
void CCineMonster::Blocked(CBaseEntity* pOther)
{
}

void CCineMonster::Touch(CBaseEntity* pOther)
{
	/*
	ALERT( at_aiconsole, "Cine Touch\n" );
	if (m_pentTarget && OFFSET(pOther->pev) == OFFSET(m_pentTarget))
	{
		CBaseMonster *pTarget = GetClassPtr((CBaseMonster *)VARS(m_pentTarget));
		pTarget->m_monsterState == MONSTERSTATE_SCRIPT;
	}
*/
}


/*
	entvars_t *pevOther = VARS( gpGlobals->other );

	if ( !FBitSet ( pevOther->flags , FL_MONSTER ) ) 
	{// touched by a non-monster.
		return;
	}

	pevOther->origin.z += 1;
	
	if ( FBitSet ( pevOther->flags, FL_ONGROUND ) ) 
	{// clear the onground so physics don't bitch
		pevOther->flags -= FL_ONGROUND;
	}

	// toss the monster!
	pevOther->velocity = pev->movedir * pev->speed;
	pevOther->velocity.z += m_flHeight;


	pev->solid = SOLID_NOT;// kill the trigger for now !!!UNDONE
}
*/


//
// ********** Cinematic DIE **********
//
void CCineMonster::Die()
{
	SetThink(&CCineMonster::SUB_Remove);
}

//
// ********** Cinematic PAIN **********
//
void CCineMonster::Pain()
{
}

//
// ********** Cinematic Think **********
//

//LRC: now redefined... find a viable entity with the given name, and return it (or NULL if not found).
CBaseMonster* CCineMonster::FindEntity(const char* sName, CBaseEntity* pActivator)
{
	CBaseEntity* pEntity;

	pEntity = UTIL_FindEntityByTargetname(nullptr, sName, pActivator);
	//m_hTargetEnt = NULL;
	CBaseMonster* pMonster = nullptr;

	while (pEntity != nullptr)
	{
		if (FBitSet(pEntity->pev->flags, FL_MONSTER))
		{
			pMonster = pEntity->MyMonsterPointer();
			if ((pMonster != nullptr) && pMonster->CanPlaySequence(m_iPriority | SS_INTERRUPT_ALERT))
			{
				return pMonster;
			}
			ALERT(at_debug, "Found %s, but can't play!\n", sName);
		}
		pEntity = UTIL_FindEntityByTargetname(pEntity, sName, pActivator);
		pMonster = nullptr;
	}

	// couldn't find something with the given targetname; assume it's a classname instead.
	if (pMonster == nullptr)
	{
		pEntity = nullptr;
		while ((pEntity = UTIL_FindEntityInSphere(pEntity, pev->origin, m_flRadius)) != nullptr)
		{
			if (FClassnameIs(pEntity->pev, sName))
			{
				if (FBitSet(pEntity->pev->flags, FL_MONSTER))
				{
					pMonster = pEntity->MyMonsterPointer();
					if ((pMonster != nullptr) && pMonster->CanPlaySequence(m_iPriority))
					{
						return pMonster;
					}
				}
			}
		}
	}
	return nullptr;
}

// make the entity enter a scripted sequence
void CCineMonster::PossessEntity()
{
	CBaseEntity* pEntity = m_hTargetEnt;
	CBaseMonster* pTarget = nullptr;
	if (pEntity != nullptr)
		pTarget = pEntity->MyMonsterPointer();

	//	ALERT( at_console, "Possess: pEntity %s, pTarget %s\n", STRING(pEntity->pev->targetname), STRING(pTarget->pev->targetname));

	if (pTarget != nullptr)
	{
		if (pTarget->m_pCine != nullptr)
		{
			pTarget->m_pCine->CancelScript();
		}

		pTarget->m_pCine = this;
		if (m_iszAttack != 0)
		{
			// anything with that name?
			pTarget->m_hTargetEnt = UTIL_FindEntityByTargetname(nullptr, STRING(m_iszAttack), m_hActivator);
			if (pTarget->m_hTargetEnt == nullptr)
			{ // nothing. Anything with that classname?
				while ((pTarget->m_hTargetEnt = UTIL_FindEntityInSphere(pTarget->m_hTargetEnt, pev->origin, m_flRadius)) != nullptr)
				{
					if (FClassnameIs(pTarget->m_hTargetEnt->pev, STRING(m_iszAttack)))
						break;
				}
			}
			if (pTarget->m_hTargetEnt == nullptr)
			{ // nothing. Oh well.
				ALERT(at_debug, "%s %s has a missing \"turn target\": %s\n", STRING(pev->classname), STRING(pev->targetname), STRING(m_iszAttack));
				pTarget->m_hTargetEnt = this;
			}
		}
		else
		{
			pTarget->m_hTargetEnt = this;
		}

		if (m_iszMoveTarget != 0)
		{
			// anything with that name?
			pTarget->m_pGoalEnt = UTIL_FindEntityByTargetname(nullptr, STRING(m_iszMoveTarget), m_hActivator);
			if (pTarget->m_pGoalEnt == nullptr)
			{ // nothing. Oh well.
				ALERT(at_debug, "%s %s has a missing \"move target\": %s\n", STRING(pev->classname), STRING(pev->targetname), STRING(m_iszMoveTarget));
				pTarget->m_pGoalEnt = this;
			}
		}
		else
		{
			pTarget->m_pGoalEnt = this;
		}
		//		if (IsAction())
		//		  pTarget->PushEnemy(this,pev->origin);

		m_saved_movetype = pTarget->pev->movetype;
		m_saved_solid = pTarget->pev->solid;
		m_saved_effects = pTarget->pev->effects;
		pTarget->pev->effects |= pev->effects;

		//		ALERT(at_console, "script. IsAction = %d",IsAction());

		m_iState = STATE_ON; // LRC: assume we'll set it to 'on', unless proven otherwise...
		switch (m_fMoveTo)
		{
		case 1:
		case 2:
			DelayStart(true);
			m_iState = STATE_TURN_ON;
			// fall through...
		case 0:
		case 4:
			//G-Cont. this is a not a better way :(
			//in my new project this bug will be removed
			//in Spirit... as is. Sorry about that.
			//If we interesting - decomment UTIL_AssignOrigin
			//and run c1a4i with tentacle script - no comments
			//UTIL_AssignOrigin( pTarget, pev->origin );
			pTarget->pev->ideal_yaw = pev->angles.y;
			pTarget->pev->avelocity = Vector(0, 0, 0);
			pTarget->pev->velocity = Vector(0, 0, 0);
			pTarget->pev->effects |= EF_NOINTERP;
			pTarget->m_scriptState = SCRIPT_WAIT;
			//m_startTime = gpGlobals->time + 1E6;
			break;
		case 5:
		case 6:
			pTarget->m_scriptState = SCRIPT_WAIT;
			break;
		}

		/* handle instant turn */
		if (m_fMoveTo == 4 || m_fMoveTo == 6)
		{
			pTarget->pev->angles.y = pev->angles.y;
		}

		//		ALERT( at_aiconsole, "\"%s\" found and used (INT: %s)\n", STRING( pTarget->pev->targetname ), FBitSet(pev->spawnflags, SF_SCRIPT_NOINTERRUPT)?"No":"Yes" );

		pTarget->m_IdealMonsterState = MONSTERSTATE_SCRIPT;
		//		if (m_iszIdle)
		//		{
		//			ALERT(at_console, "Possess: Play idle sequence\n");
		//			StartSequence( pTarget, m_iszIdle, false );
		//			if (FStrEq( STRING(m_iszIdle), STRING(m_iszPlay)))
		//			{
		//				pTarget->pev->framerate = 0;
		//			}
		//		}
		//		ALERT(at_console, "Finished PossessEntity, ms %d, ims %d\n", pTarget->m_MonsterState, pTarget->m_IdealMonsterState);
	}
}


// at the beginning of the level, set up the idle animation. --LRC
void CCineMonster::InitIdleThink()
{
	if ((m_hTargetEnt = FindEntity(STRING(m_iszEntity), nullptr)) != nullptr)
	{
		PossessEntity();
		m_startTime = gpGlobals->time + 1E6;
		ALERT(at_aiconsole, "script \"%s\" using monster \"%s\"\n", STRING(pev->targetname), STRING(m_iszEntity));
	}
	else
	{
		CancelScript();
		ALERT(at_aiconsole, "script \"%s\" can't find monster \"%s\"\n", STRING(pev->targetname), STRING(m_iszEntity));
		SetNextThink(1.0);
	}
}

void CCineMonster::CineThink()
{
	//	ALERT(at_console, "Sequence think, activator %s\n", STRING(m_hActivator->pev->targetname));
	if ((m_hTargetEnt = FindEntity(STRING(m_iszEntity), m_hActivator)) != nullptr)
	{
		//		ALERT(at_console, "Sequence found %s \"%s\"\n", STRING(m_hTargetEnt->pev->classname), STRING(m_hTargetEnt->pev->targetname));
		PossessEntity();
		ALERT(at_aiconsole, "script \"%s\" using monster \"%s\"\n", STRING(pev->targetname), STRING(m_iszEntity));
	}
	else
	{
		//		ALERT(at_console, "Sequence found nothing called %s\n", STRING(m_iszEntity));
		CancelScript();
		ALERT(at_aiconsole, "script \"%s\" can't find monster \"%s\"\n", STRING(pev->targetname), STRING(m_iszEntity));
		SetNextThink(1.0);
	}
}


// lookup a sequence name and setup the target monster to play it
bool CCineMonster::StartSequence(CBaseMonster* pTarget, int iszSeq, bool completeOnEmpty)
{
	//	ALERT( at_console, "StartSequence %s \"%s\"\n", STRING(pev->classname), STRING(pev->targetname));

	if (FStringNull(iszSeq) && completeOnEmpty)
	{
		SequenceDone(pTarget);
		return false;
	}

	pTarget->pev->sequence = pTarget->LookupSequence(STRING(iszSeq));
	if (pTarget->pev->sequence == -1)
	{
		ALERT(at_error, "%s: unknown scripted sequence \"%s\"\n", STRING(pTarget->pev->targetname), STRING(iszSeq));
		pTarget->pev->sequence = 0;
		// return false;
	}

#if 0
	char *s;
	if ( pev->spawnflags & SF_SCRIPT_NOINTERRUPT ) 
		s = "No";
	else
		s = "Yes";

	ALERT( at_debug, "%s (%s): started \"%s\":INT:%s\n", STRING( pTarget->pev->targetname ), STRING( pTarget->pev->classname ), STRING( iszSeq), s );
#endif

	pTarget->pev->frame = 0;
	pTarget->ResetSequenceInfo();
	return true;
}

//=========================================================
// SequenceDone - called when a scripted sequence animation
// sequence is done playing ( or when an AI Scripted Sequence
// doesn't supply an animation sequence to play ). Expects
// the CBaseMonster pointer to the monster that the sequence
// possesses.
//=========================================================
void CCineMonster::SequenceDone(CBaseMonster* pMonster)
{
	m_iRepeatsLeft = m_iRepeats; //LRC - reset the repeater count
	m_iState = STATE_OFF;		 // we've finished.
								 //	ALERT( at_console, "Sequence %s finished\n", STRING(pev->targetname));//STRING( m_pCine->m_iszPlay ) );

	if ((pev->spawnflags & SF_SCRIPT_REPEATABLE) == 0)
	{
		SetThink(&CCineMonster::SUB_Remove);
		SetNextThink(0.1);
	}

	// This is done so that another sequence can take over the monster when triggered by the first

	pMonster->CineCleanup();

	FixScriptMonsterSchedule(pMonster);

	// This may cause a sequence to attempt to grab this guy NOW, so we have to clear him out
	// of the existing sequence
	SUB_UseTargets(nullptr, USE_TOGGLE, 0);
}

//=========================================================
// When a monster finishes a scripted sequence, we have to
// fix up its state and schedule for it to return to a
// normal AI monster.
//
// Scripted sequences just dirty the Schedule and drop the
// monster in Idle State.
//
// or select a specific AMBUSH schedule, regardless of state. //LRC
//=========================================================
void CCineMonster::FixScriptMonsterSchedule(CBaseMonster* pMonster)
{
	if (pMonster->m_IdealMonsterState != MONSTERSTATE_DEAD)
		pMonster->m_IdealMonsterState = MONSTERSTATE_IDLE;
	//	pMonster->ClearSchedule();

	switch (m_iFinishSchedule)
	{
	case SCRIPT_FINISHSCHED_DEFAULT:
		pMonster->ClearSchedule();
		break;
	case SCRIPT_FINISHSCHED_AMBUSH:
		pMonster->ChangeSchedule(pMonster->GetScheduleOfType(SCHED_AMBUSH));
		break;
	default:
		ALERT(at_aiconsole, "FixScriptMonsterSchedule - no case!\n");
		pMonster->ClearSchedule();
		break;
	}
}

bool CBaseMonster::ExitScriptedSequence()
{
	if (pev->deadflag == DEAD_DYING)
	{
		// is this legal?
		// BUGBUG -- This doesn't call Killed()
		m_IdealMonsterState = MONSTERSTATE_DEAD;
		return false;
	}

	if (m_pCine != nullptr)
	{
		m_pCine->CancelScript();
	}

	return true;
}


void CCineMonster::AllowInterrupt(bool fAllow)
{
	if ((pev->spawnflags & SF_SCRIPT_NOINTERRUPT) != 0)
		return;
	m_interruptable = fAllow;
}


bool CCineMonster::CanInterrupt()
{
	if (!m_interruptable)
		return false;

	CBaseEntity* pTarget = m_hTargetEnt;

	if (pTarget != nullptr && pTarget->pev->deadflag == DEAD_NO)
		return true;

	return false;
}


int CCineMonster::IgnoreConditions()
{
	if (CanInterrupt())
		return 0;

	// Big fat BUG: This is an IgnoreConditions function - we need to return the conditions
	// that _shouldn't_ be able to break the script, instead of the conditions that _should_!!
	return SCRIPT_BREAK_CONDITIONS;
}


void ScriptEntityCancel(edict_t* pentCine)
{
	// make sure they are a scripted_sequence
	if (FClassnameIs(pentCine, "scripted_sequence") || FClassnameIs(pentCine, "scripted_action"))
	{
		CCineMonster* pCineTarget = GetClassPtr((CCineMonster*)VARS(pentCine));
		if (pCineTarget->pev == nullptr)
			return;
		pCineTarget->m_iState = STATE_OFF;

		// make sure they have a monster in mind for the script
		CBaseEntity* pEntity = pCineTarget->m_hTargetEnt;
		CBaseMonster* pTarget = nullptr;
		if (pEntity != nullptr)
			pTarget = pEntity->MyMonsterPointer();

		if (pTarget != nullptr)
		{
			// make sure their monster is actually playing a script
			if (pTarget->m_MonsterState == MONSTERSTATE_SCRIPT)
			{
				// tell them do die
				pTarget->m_scriptState = CCineMonster::SCRIPT_CLEANUP;
				// do it now
				pTarget->CineCleanup();
				//LRC - clean up so that if another script is starting immediately, the monster will notice it.
				pTarget->ClearSchedule();
			}
		}
	}
}


// find all the cinematic entities with my targetname and stop them from playing
void CCineMonster::CancelScript()
{
	ALERT(at_aiconsole, "Cancelling script: %s\n", STRING(m_iszPlay));

	if (FStringNull(pev->targetname))
	{
		ScriptEntityCancel(edict());
		return;
	}

	CBaseEntity* pCineTarget = UTIL_FindEntityByTargetname(nullptr, STRING(pev->targetname));

	while (pCineTarget != nullptr)
	{
		ScriptEntityCancel(ENT(pCineTarget->pev));
		pCineTarget = UTIL_FindEntityByTargetname(pCineTarget, STRING(pev->targetname));
	}
}


// find all the cinematic entities with my targetname and tell them whether to wait before starting
void CCineMonster::DelayStart(bool state)
{
	CBaseEntity* pCine = UTIL_FindEntityByTargetname(nullptr, STRING(pev->targetname));

	while (pCine != nullptr)
	{
		if (FClassnameIs(pCine->pev, "scripted_sequence") || FClassnameIs(pCine->pev, "scripted_action"))
		{
			CCineMonster* pTarget = GetClassPtr((CCineMonster*)(pCine->pev));
			if (state)
			{
				//				ALERT(at_console, "Delaying start\n");
				pTarget->m_iDelay++;
			}
			else
			{
				//				ALERT(at_console, "Undelaying start\n");
				pTarget->m_iDelay--;
				if (pTarget->m_iDelay <= 0)
				{
					pTarget->m_iState = STATE_ON;									  //LRC
					FireTargets(STRING(m_iszFireOnBegin), this, this, USE_TOGGLE, 0); //LRC
					pTarget->m_startTime = gpGlobals->time + 0.05;					  // why the delay? -- LRC
				}
			}
		}
		pCine = UTIL_FindEntityByTargetname(pCine, STRING(pev->targetname));
	}
}



// Find an entity that I'm interested in and precache the sounds he'll need in the sequence.
void CCineMonster::Activate()
{
	CBaseEntity* pEntity;
	CBaseMonster* pTarget;

	// The entity name could be a target name or a classname
	// Check the targetname
	pEntity = UTIL_FindEntityByTargetname(nullptr, STRING(m_iszEntity));
	pTarget = nullptr;

	while ((pTarget == nullptr) && (pEntity != nullptr))
	{
		if (FBitSet(pEntity->pev->flags, FL_MONSTER))
		{
			pTarget = pEntity->MyMonsterPointer();
		}
		pEntity = UTIL_FindEntityByTargetname(pEntity, STRING(m_iszEntity));
	}

	// If no entity with that targetname, check the classname
	if (pTarget == nullptr)
	{
		pEntity = UTIL_FindEntityByClassname(nullptr, STRING(m_iszEntity));
		while ((pTarget == nullptr) && (pEntity != nullptr))
		{
			pTarget = pEntity->MyMonsterPointer();
			pEntity = UTIL_FindEntityByClassname(pEntity, STRING(m_iszEntity));
		}
	}
	// Found a compatible entity
	if (pTarget != nullptr)
	{
		void* pmodel;
		pmodel = GET_MODEL_PTR(pTarget->edict());
		if (pmodel != nullptr)
		{
			// Look through the event list for stuff to precache
			SequencePrecache(pmodel, STRING(m_iszIdle));
			SequencePrecache(pmodel, STRING(m_iszPlay));
		}
	}

	CBaseMonster::Activate();
}


bool CBaseMonster::CineCleanup()
{
	CCineMonster* pOldCine = m_pCine;

	// am I linked to a cinematic?
	if (m_pCine != nullptr)
	{
		// okay, reset me to what it thought I was before
		m_pCine->m_hTargetEnt = nullptr;
		pev->movetype = m_pCine->m_saved_movetype;
		pev->solid = m_pCine->m_saved_solid;
		pev->effects = m_pCine->m_saved_effects;

		if ((m_pCine->pev->spawnflags & SF_SCRIPT_STAYDEAD) != 0)
			pev->deadflag = DEAD_DYING;
	}
	else
	{
		// arg, punt
		pev->movetype = MOVETYPE_STEP; // this is evil
		pev->solid = SOLID_SLIDEBOX;
	}
	m_pCine = nullptr;
	m_hTargetEnt = nullptr;
	m_pGoalEnt = nullptr;
	if (pev->deadflag == DEAD_DYING)
	{
		// last frame of death animation?
		pev->health = 0;
		pev->framerate = 0.0;
		pev->solid = SOLID_NOT;
		SetState(MONSTERSTATE_DEAD);
		pev->deadflag = DEAD_DEAD;
		UTIL_SetSize(pev, pev->mins, Vector(pev->maxs.x, pev->maxs.y, pev->mins.z + 2));

		if ((pOldCine != nullptr) && FBitSet(pOldCine->pev->spawnflags, SF_SCRIPT_LEAVECORPSE))
		{
			SetUse(nullptr);	// BUGBUG -- This doesn't call Killed()
			SetThink(nullptr); // This will probably break some stuff
			SetTouch(nullptr);
		}
		else
			SUB_StartFadeOut(); // SetThink( SUB_DoNothing );
		// This turns off animation & physics in case their origin ends up stuck in the world or something
		StopAnimation();
		pev->movetype = MOVETYPE_NONE;
		pev->effects |= EF_NOINTERP; // Don't interpolate either, assume the corpse is positioned in its final resting place
		return false;
	}

	// If we actually played a sequence
	if ((pOldCine != nullptr) && !FStringNull(pOldCine->m_iszPlay))
	{
		if ((pOldCine->pev->spawnflags & SF_SCRIPT_NOSCRIPTMOVEMENT) == 0)
		{
			// reset position
			Vector new_origin, new_angle;
			GetBonePosition(0, new_origin, new_angle);

			// Figure out how far they have moved
			// We can't really solve this problem because we can't query the movement of the origin relative
			// to the sequence.  We can get the root bone's position as we do here, but there are
			// cases where the root bone is in a different relative position to the entity's origin
			// before/after the sequence plays.  So we are stuck doing this:

			// !!!HACKHACK: Float the origin up and drop to floor because some sequences have
			// irregular motion that can't be properly accounted for.

			// UNDONE: THIS SHOULD ONLY HAPPEN IF WE ACTUALLY PLAYED THE SEQUENCE.
			Vector oldOrigin = pev->origin;

			// UNDONE: ugly hack.  Don't move monster if they don't "seem" to move
			// this really needs to be done with the AX,AY,etc. flags, but that aren't consistantly
			// being set, so animations that really do move won't be caught.
			if ((oldOrigin - new_origin).Length2D() < 8.0)
				new_origin = oldOrigin;

			pev->origin.x = new_origin.x;
			pev->origin.y = new_origin.y;
			pev->origin.z += 1;

			pev->flags |= FL_ONGROUND;
			int drop = DROP_TO_FLOOR(ENT(pev));

			// Origin in solid?  Set to org at the end of the sequence
			if (drop < 0)
				pev->origin = oldOrigin;
			else if (drop == 0) // Hanging in air?
			{
				pev->origin.z = new_origin.z;
				pev->flags &= ~FL_ONGROUND;
			}
			// else entity hit floor, leave there

			// pEntity->pev->origin.z = new_origin.z + 5.0; // damn, got to fix this

			UTIL_SetOrigin(this, pev->origin);
			pev->effects |= EF_NOINTERP;
		}

		// We should have some animation to put these guys in, but for now it's idle.
		// Due to NOINTERP above, there won't be any blending between this anim & the sequence
		m_Activity = ACT_RESET;
	}
	// set them back into a normal state
	pev->enemy = nullptr;
	if (pev->health > 0)
		m_IdealMonsterState = MONSTERSTATE_IDLE; // m_previousState;
	else
	{
		// Dropping out because he got killed
		// Can't call killed() no attacker and weirdness (late gibbing) may result
		m_IdealMonsterState = MONSTERSTATE_DEAD;
		SetConditions(bits_COND_LIGHT_DAMAGE);
		pev->deadflag = DEAD_DYING;
		FCheckAITrigger();
		pev->deadflag = DEAD_NO;
	}


	//	SetAnimation( m_MonsterState );
	//LRC- removed, was never implemented. ClearBits(pev->spawnflags, SF_MONSTER_WAIT_FOR_SCRIPT );

	return true;
}




class CScriptedSentence : public CBaseToggle
{
public:
	void Spawn() override;
	bool KeyValue(KeyValueData* pkvd) override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	void EXPORT FindThink();
	void EXPORT DelayThink();
	void EXPORT DurationThink();
	int ObjectCaps() override { return (CBaseToggle::ObjectCaps() & ~FCAP_ACROSS_TRANSITION); }

	STATE GetState() override { return m_playing ? STATE_ON : STATE_OFF; }

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;

	static TYPEDESCRIPTION m_SaveData[];

	CBaseToggle* FindEntity(CBaseEntity* pActivator);
	bool AcceptableSpeaker(CBaseToggle* pMonster);
	bool StartSentence(CBaseToggle* pTarget);


private:
	int m_iszSentence;	// string index for idle animation
	int m_iszEntity;	// entity that is wanted for this sentence
	float m_flRadius;	// range to search
	float m_flDuration; // How long the sentence lasts
	float m_flRepeat;	// maximum repeat rate
	float m_flAttenuation;
	float m_flVolume;
	bool m_active;	   // is the sentence enabled? (for m_flRepeat)
	bool m_playing;	   //LRC- is the sentence playing? (for GetState)
	int m_iszListener; // name of entity to look at while talking
};

#define SF_SENTENCE_ONCE 0x0001
#define SF_SENTENCE_FOLLOWERS 0x0002  // only say if following player
#define SF_SENTENCE_INTERRUPT 0x0004  // force talking except when dead
#define SF_SENTENCE_CONCURRENT 0x0008 // allow other people to keep talking

TYPEDESCRIPTION CScriptedSentence::m_SaveData[] =
	{
		DEFINE_FIELD(CScriptedSentence, m_iszSentence, FIELD_STRING),
		DEFINE_FIELD(CScriptedSentence, m_iszEntity, FIELD_STRING),
		DEFINE_FIELD(CScriptedSentence, m_flRadius, FIELD_FLOAT),
		DEFINE_FIELD(CScriptedSentence, m_flDuration, FIELD_FLOAT),
		DEFINE_FIELD(CScriptedSentence, m_flRepeat, FIELD_FLOAT),
		DEFINE_FIELD(CScriptedSentence, m_flAttenuation, FIELD_FLOAT),
		DEFINE_FIELD(CScriptedSentence, m_flVolume, FIELD_FLOAT),
		DEFINE_FIELD(CScriptedSentence, m_active, FIELD_BOOLEAN),
		DEFINE_FIELD(CScriptedSentence, m_playing, FIELD_BOOLEAN),
		DEFINE_FIELD(CScriptedSentence, m_iszListener, FIELD_STRING),
};


IMPLEMENT_SAVERESTORE(CScriptedSentence, CBaseToggle);

LINK_ENTITY_TO_CLASS(scripted_sentence, CScriptedSentence);

bool CScriptedSentence::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "sentence"))
	{
		m_iszSentence = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "entity"))
	{
		m_iszEntity = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "duration"))
	{
		m_flDuration = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "radius"))
	{
		m_flRadius = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "refire"))
	{
		m_flRepeat = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "attenuation"))
	{
		pev->impulse = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "volume"))
	{
		m_flVolume = atof(pkvd->szValue) * 0.1;
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "listener"))
	{
		m_iszListener = ALLOC_STRING(pkvd->szValue);
		return true;
	}

	return CBaseToggle::KeyValue(pkvd);
}


void CScriptedSentence::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!m_active)
		return;
	//	ALERT( at_console, "Firing sentence: %s\n", STRING(m_iszSentence) );
	m_hActivator = pActivator;
	SetThink(&CScriptedSentence::FindThink);
	SetNextThink(0);
}


void CScriptedSentence::Spawn()
{
	pev->solid = SOLID_NOT;

	m_active = true;
	m_playing = false; //LRC
	// if no targetname, start now
	if (FStringNull(pev->targetname))
	{
		SetThink(&CScriptedSentence::FindThink);
		SetNextThink(1.0);
	}

	switch (pev->impulse)
	{
	case 1: // Medium radius
		m_flAttenuation = ATTN_STATIC;
		break;

	case 2: // Large radius
		m_flAttenuation = ATTN_NORM;
		break;

	case 3: //EVERYWHERE
		m_flAttenuation = ATTN_NONE;
		break;

	default:
	case 0: // Small radius
		m_flAttenuation = ATTN_IDLE;
		break;
	}
	pev->impulse = 0;

	// No volume, use normal
	if (m_flVolume <= 0)
		m_flVolume = 1.0;
}


void CScriptedSentence::FindThink()
{
	if (m_iszEntity == 0) //LRC- no target monster given: speak through HEV
	{
		CBasePlayer* pPlayer = (CBasePlayer*)UTIL_FindEntityByClassname(nullptr, "player");
		if (pPlayer != nullptr)
		{
			m_playing = true;
			if ((STRING(m_iszSentence))[0] == '!')
				pPlayer->SetSuitUpdate((char*)STRING(m_iszSentence), false, 0);
			else
				pPlayer->SetSuitUpdate((char*)STRING(m_iszSentence), true, 0);
			if ((pev->spawnflags & SF_SENTENCE_ONCE) != 0)
				UTIL_Remove(this);
			SetThink(&CScriptedSentence::DurationThink);
			SetNextThink(m_flDuration);
			m_active = false;
		}
		else
			ALERT(at_debug, "ScriptedSentence: can't find \"player\" to play HEV sentence!?\n");
		return;
	}

	CBaseToggle* pEnt = FindEntity(m_hActivator);
	if (pEnt != nullptr)
	{
		m_playing = true;
		StartSentence(pEnt);
		if ((pev->spawnflags & SF_SENTENCE_ONCE) != 0)
			UTIL_Remove(this);
		SetThink(&CScriptedSentence::DurationThink);
		SetNextThink(m_flDuration);
		m_active = false;
		//		ALERT( at_console, "%s: found monster %s\n", STRING(m_iszSentence), STRING(m_iszEntity) );
	}
	else
	{
		//		ALERT( at_console, "%s: can't find monster %s\n", STRING(m_iszSentence), STRING(m_iszEntity) );
		SetNextThink(m_flRepeat + 0.5);
	}
}

//LRC
void CScriptedSentence::DurationThink()
{
	m_playing = false;
	SetNextThink(m_flRepeat);
	SetThink(&CScriptedSentence::DelayThink);
}

void CScriptedSentence::DelayThink()
{
	m_active = true;
	if (FStringNull(pev->targetname))
		SetNextThink(0.1);
	SetThink(&CScriptedSentence::FindThink);
}


bool CScriptedSentence::AcceptableSpeaker(CBaseToggle* pTarget)
{
	CBaseMonster* pMonster;
	pMonster = nullptr;

	if (pTarget != nullptr)
	{
		pMonster = pTarget->MyMonsterPointer();
	}

	if (pMonster != nullptr)
	{
		if ((pev->spawnflags & SF_SENTENCE_FOLLOWERS) != 0)
		{
			if (pMonster->m_hTargetEnt == nullptr || !FClassnameIs(pMonster->m_hTargetEnt->pev, "player"))
				return false;
		}
		bool override;
		if ((pev->spawnflags & SF_SENTENCE_INTERRUPT) != 0)
			override = true;
		else
			override = false;
		if (pMonster->CanPlaySentence(override))
			return true;
	}
	else
	{
		// targeting something other than a monster, sure it can speak
		if ((pTarget != nullptr) && pTarget->IsAllowedToSpeak())
			return true;
	}

	return false;
}


CBaseToggle* CScriptedSentence::FindEntity(CBaseEntity* pActivator)
{
	CBaseEntity* pTarget;
	CBaseToggle* pSpeakingEnt;

	pTarget = UTIL_FindEntityByTargetname(nullptr, STRING(m_iszEntity), pActivator);
	pSpeakingEnt = nullptr;

	while (pTarget != nullptr)
	{
		CBaseEntity* pEnt = pTarget;
		pSpeakingEnt = (pEnt != nullptr) ? pEnt->MyTogglePointer() : nullptr;

		if (pSpeakingEnt != nullptr)
		{
			if (AcceptableSpeaker(pSpeakingEnt))
			{
				// ALERT(at_console, "acceptable speaker\n");
				return pSpeakingEnt;
			}
			// ALERT(at_console, "found unacceptable speaker\n");
		}
		pTarget = UTIL_FindEntityByTargetname(pTarget, STRING(m_iszEntity), pActivator);
	}

	pTarget = nullptr;
	while ((pTarget = UTIL_FindEntityInSphere(pTarget, pev->origin, m_flRadius)) != nullptr)
	{
		if (FClassnameIs(pTarget->pev, STRING(m_iszEntity)))
		{
			if (FBitSet(pTarget->pev->flags, FL_MONSTER))
			{
				pSpeakingEnt = pTarget->MyTogglePointer();
				if (AcceptableSpeaker(pSpeakingEnt))
					return pSpeakingEnt;
			}
		}
	}

	return nullptr;
}


bool CScriptedSentence::StartSentence(CBaseToggle* pTarget)
{
	if (pTarget == nullptr)
	{
		ALERT(at_aiconsole, "Not Playing sentence %s\n", STRING(m_iszSentence));
		return false;
	}

	bool bConcurrent = false;
	//LRC: Er... if the "concurrent" flag is NOT set, we make bConcurrent true!?
	if ((pev->spawnflags & SF_SENTENCE_CONCURRENT) == 0)
		bConcurrent = true;

	CBaseEntity* pListener = nullptr;
	if (!FStringNull(m_iszListener))
	{
		float radius = m_flRadius;

		if (FStrEq(STRING(m_iszListener), "player"))
			radius = 4096; // Always find the player

		pListener = UTIL_FindEntityGeneric(STRING(m_iszListener), pTarget->pev->origin, radius);
	}

	pTarget->PlayScriptedSentence(STRING(m_iszSentence), m_flDuration, m_flVolume, m_flAttenuation, bConcurrent, pListener);
	ALERT(at_aiconsole, "Playing sentence %s (%.1f)\n", STRING(m_iszSentence), m_flDuration);
	SUB_UseTargets(nullptr, USE_TOGGLE, 0);
	return true;
}





/*

*/


//=========================================================
// Furniture - this is the cool comment I cut-and-pasted
//=========================================================
class CFurniture : public CBaseMonster
{
public:
	void Spawn() override;
	void Die();
	int Classify() override;
	int ObjectCaps() override { return (CBaseMonster::ObjectCaps() & ~FCAP_ACROSS_TRANSITION); }
};


LINK_ENTITY_TO_CLASS(monster_furniture, CFurniture);


//=========================================================
// Furniture is killed
//=========================================================
void CFurniture::Die()
{
	SetThink(&CFurniture::SUB_Remove);
	SetNextThink(0);
}

//=========================================================
// This used to have something to do with bees flying, but
// now it only initializes moving furniture in scripted sequences
//=========================================================
void CFurniture::Spawn()
{
	PRECACHE_MODEL((char*)STRING(pev->model));
	SET_MODEL(ENT(pev), STRING(pev->model));

	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_BBOX;
	pev->health = 80000;
	pev->takedamage = DAMAGE_AIM;
	pev->effects = 0;
	pev->yaw_speed = 0;
	pev->sequence = 0;
	pev->frame = 0;

	//	pev->nextthink += 1.0;
	//	SetThink (WalkMonsterDelay);

	ResetSequenceInfo();
	pev->frame = 0;
	MonsterInit();
}

//=========================================================
// ID's Furniture as neutral (noone will attack it)
//=========================================================
int CFurniture::Classify()
{
	return (m_iClass != 0) ? m_iClass : CLASS_NONE;
}
