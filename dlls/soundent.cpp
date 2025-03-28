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
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "soundent.h"


LINK_ENTITY_TO_CLASS(soundent, CSoundEnt);

//=========================================================
// CSound - Clear - zeros all fields for a sound
//=========================================================
void CSound::Clear()
{
	m_vecOrigin = g_vecZero;
	m_iType = 0;
	m_iVolume = 0;
	m_flExpireTime = 0;
	m_iNext = SOUNDLIST_EMPTY;
	m_iNextAudible = 0;
}

//=========================================================
// Reset - clears the volume, origin, and type for a sound,
// but doesn't expire or unlink it.
//=========================================================
void CSound::Reset()
{
	m_vecOrigin = g_vecZero;
	m_iType = 0;
	m_iVolume = 0;
	m_iNext = SOUNDLIST_EMPTY;
}

//=========================================================
// FIsSound - returns true if the sound is an Audible sound
//=========================================================
bool CSound::FIsSound()
{
	if ((m_iType & (bits_SOUND_COMBAT | bits_SOUND_WORLD | bits_SOUND_PLAYER | bits_SOUND_DANGER)) != 0)
	{
		return true;
	}

	return false;
}

//=========================================================
// FIsScent - returns true if the sound is actually a scent
//=========================================================
bool CSound::FIsScent()
{
	if ((m_iType & (bits_SOUND_CARCASS | bits_SOUND_MEAT | bits_SOUND_GARBAGE)) != 0)
	{
		return true;
	}

	return false;
}

//=========================================================
// Spawn
//=========================================================
void CSoundEnt::Spawn()
{
	pev->solid = SOLID_NOT;
	Initialize();

	SetNextThink(1);
}

//=========================================================
// Think - at interval, the entire active sound list is checked
// for sounds that have ExpireTimes less than or equal
// to the current world time, and these sounds are deallocated.
//=========================================================
void CSoundEnt::Think()
{
	int iSound;
	int iPreviousSound;

	SetNextThink(0.3); // how often to check the sound list.

	iPreviousSound = SOUNDLIST_EMPTY;
	iSound = m_iActiveSound;

	while (iSound != SOUNDLIST_EMPTY)
	{
		if (m_SoundPool[iSound].m_flExpireTime <= gpGlobals->time && m_SoundPool[iSound].m_flExpireTime != SOUND_NEVER_EXPIRE)
		{
			int iNext = m_SoundPool[iSound].m_iNext;

			// move this sound back into the free list
			FreeSound(iSound, iPreviousSound);

			iSound = iNext;
		}
		else
		{
			iPreviousSound = iSound;
			iSound = m_SoundPool[iSound].m_iNext;
		}
	}

	if (m_fShowReport)
	{
		ALERT(at_aiconsole, "Soundlist: %d / %d  (%d)\n", ISoundsInList(SOUNDLISTTYPE_ACTIVE), ISoundsInList(SOUNDLISTTYPE_FREE), ISoundsInList(SOUNDLISTTYPE_ACTIVE) - m_cLastActiveSounds);
		m_cLastActiveSounds = ISoundsInList(SOUNDLISTTYPE_ACTIVE);
	}
}

//=========================================================
// Precache - dummy function
//=========================================================
void CSoundEnt::Precache()
{
}

//=========================================================
// FreeSound - clears the passed active sound and moves it
// to the top of the free list. TAKE CARE to only call this
// function for sounds in the Active list!!
//=========================================================
void CSoundEnt::FreeSound(int iSound, int iPrevious)
{
	if (pSoundEnt == nullptr)
	{
		// no sound ent!
		return;
	}

	if (iPrevious != SOUNDLIST_EMPTY)
	{
		// iSound is not the head of the active list, so
		// must fix the index for the Previous sound
		//		pSoundEnt->m_SoundPool[ iPrevious ].m_iNext = m_SoundPool[ iSound ].m_iNext;
		pSoundEnt->m_SoundPool[iPrevious].m_iNext = pSoundEnt->m_SoundPool[iSound].m_iNext;
	}
	else
	{
		// the sound we're freeing IS the head of the active list.
		pSoundEnt->m_iActiveSound = pSoundEnt->m_SoundPool[iSound].m_iNext;
	}

	// make iSound the head of the Free list.
	pSoundEnt->m_SoundPool[iSound].m_iNext = pSoundEnt->m_iFreeSound;
	pSoundEnt->m_iFreeSound = iSound;
}

//=========================================================
// IAllocSound - moves a sound from the Free list to the
// Active list returns the index of the alloc'd sound
//=========================================================
int CSoundEnt::IAllocSound()
{
	int iNewSound;

	if (m_iFreeSound == SOUNDLIST_EMPTY)
	{
		// no free sound!
		ALERT(at_debug, "Free Sound List is full!\n");
		return SOUNDLIST_EMPTY;
	}

	// there is at least one sound available, so move it to the
	// Active sound list, and return its SoundPool index.

	iNewSound = m_iFreeSound; // copy the index of the next free sound

	m_iFreeSound = m_SoundPool[m_iFreeSound].m_iNext; // move the index down into the free list.

	m_SoundPool[iNewSound].m_iNext = m_iActiveSound; // point the new sound at the top of the active list.

	m_iActiveSound = iNewSound; // now make the new sound the top of the active list. You're done.

	return iNewSound;
}

//=========================================================
// InsertSound - Allocates a free sound and fills it with
// sound info.
//=========================================================
void CSoundEnt::InsertSound(int iType, const Vector& vecOrigin, int iVolume, float flDuration)
{
	int iThisSound;

	if (pSoundEnt == nullptr)
	{
		// no sound ent!
		return;
	}

	iThisSound = pSoundEnt->IAllocSound();

	if (iThisSound == SOUNDLIST_EMPTY)
	{
		ALERT(at_debug, "Could not AllocSound() for InsertSound() (DLL)\n");
		return;
	}

	pSoundEnt->m_SoundPool[iThisSound].m_vecOrigin = vecOrigin;
	pSoundEnt->m_SoundPool[iThisSound].m_iType = iType;
	pSoundEnt->m_SoundPool[iThisSound].m_iVolume = iVolume;
	pSoundEnt->m_SoundPool[iThisSound].m_flExpireTime = gpGlobals->time + flDuration;
}

//=========================================================
// Initialize - clears all sounds and moves them into the
// free sound list.
//=========================================================
void CSoundEnt::Initialize()
{
	int i;
	int iSound;

	m_cLastActiveSounds;
	m_iFreeSound = 0;
	m_iActiveSound = SOUNDLIST_EMPTY;

	for (i = 0; i < MAX_WORLD_SOUNDS; i++)
	{ // clear all sounds, and link them into the free sound list.
		m_SoundPool[i].Clear();
		m_SoundPool[i].m_iNext = i + 1;
	}

	m_SoundPool[i - 1].m_iNext = SOUNDLIST_EMPTY; // terminate the list here.


	// now reserve enough sounds for each client
	for (i = 0; i < gpGlobals->maxClients; i++)
	{
		iSound = pSoundEnt->IAllocSound();

		if (iSound == SOUNDLIST_EMPTY)
		{
			ALERT(at_debug, "Could not AllocSound() for Client Reserve! (DLL)\n");
			return;
		}

		pSoundEnt->m_SoundPool[iSound].m_flExpireTime = SOUND_NEVER_EXPIRE;
	}

	if (CVAR_GET_FLOAT("displaysoundlist") == 1)
	{
		m_fShowReport = true;
	}
	else
	{
		m_fShowReport = false;
	}
}

//=========================================================
// ISoundsInList - returns the number of sounds in the desired
// sound list.
//=========================================================
int CSoundEnt::ISoundsInList(int iListType)
{
	int iThisSound = [=]()
	{
		switch (iListType)
		{
		case SOUNDLISTTYPE_FREE: return m_iFreeSound;
		case SOUNDLISTTYPE_ACTIVE: return m_iActiveSound;
		default:
			ALERT(at_console, "Unknown Sound List Type!\n");
			return SOUNDLIST_EMPTY;
		}
	}();

	int i = 0;

	while (iThisSound != SOUNDLIST_EMPTY)
	{
		i++;

		iThisSound = m_SoundPool[iThisSound].m_iNext;
	}

	return i;
}

//=========================================================
// ActiveList - returns the head of the active sound list
//=========================================================
int CSoundEnt::ActiveList()
{
	if (pSoundEnt == nullptr)
	{
		return SOUNDLIST_EMPTY;
	}

	return pSoundEnt->m_iActiveSound;
}

//=========================================================
// FreeList - returns the head of the free sound list
//=========================================================
int CSoundEnt::FreeList()
{
	if (pSoundEnt == nullptr)
	{
		return SOUNDLIST_EMPTY;
	}

	return pSoundEnt->m_iFreeSound;
}

//=========================================================
// SoundPointerForIndex - returns a pointer to the instance
// of CSound at index's position in the sound pool.
//=========================================================
CSound* CSoundEnt::SoundPointerForIndex(int iIndex)
{
	if (pSoundEnt == nullptr)
	{
		return nullptr;
	}

	if (iIndex > (MAX_WORLD_SOUNDS - 1))
	{
		ALERT(at_debug, "SoundPointerForIndex() - Index too large!\n");
		return nullptr;
	}

	if (iIndex < 0)
	{
		ALERT(at_debug, "SoundPointerForIndex() - Index < 0!\n");
		return nullptr;
	}

	return &pSoundEnt->m_SoundPool[iIndex];
}

//=========================================================
// Clients are numbered from 1 to MAXCLIENTS, but the client
// reserved sounds in the soundlist are from 0 to MAXCLIENTS - 1,
// so this function ensures that a client gets the proper index
// to his reserved sound in the soundlist.
//=========================================================
int CSoundEnt::ClientSoundIndex(edict_t* pClient)
{
	int iReturn = ENTINDEX(pClient) - 1;

#ifdef _DEBUG
	if (iReturn < 0 || iReturn > gpGlobals->maxClients)
	{
		ALERT(at_debug, "** ClientSoundIndex returning a bogus value! **\n");
	}
#endif // _DEBUG

	return iReturn;
}
