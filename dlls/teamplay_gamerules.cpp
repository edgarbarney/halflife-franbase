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
//
// teamplay_gamerules.cpp
//
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"
#include "teamplay_gamerules.h"
#include "game.h"
#include "UserMessages.h"

//TODO: these should be members of CHalfLifeTeamplay
static char team_names[MAX_TEAMS][MAX_TEAMNAME_LENGTH];
static int team_scores[MAX_TEAMS];
static int num_teams = 0;

CHalfLifeTeamplay::CHalfLifeTeamplay()
{
	m_DisableDeathMessages = false;
	m_DisableDeathPenalty = false;

	memset(team_names, 0, sizeof(team_names));
	memset(team_scores, 0, sizeof(team_scores));
	num_teams = 0;

	// Copy over the team from the server config
	m_szTeamList[0] = 0;

	// Cache this because the team code doesn't want to deal with changing this in the middle of a game
	strncpy(m_szTeamList, teamlist.string, TEAMPLAY_TEAMLISTLENGTH);

	edict_t* pWorld = CWorld::World->edict();
	if ((pWorld != nullptr) && !FStringNull(pWorld->v.team))
	{
		if (0 != teamoverride.value)
		{
			const char* pTeamList = STRING(pWorld->v.team);
			if ((pTeamList != nullptr) && 0 != strlen(pTeamList))
			{
				strncpy(m_szTeamList, pTeamList, TEAMPLAY_TEAMLISTLENGTH);
			}
		}
	}
	// Has the server set teams
	if (0 != strlen(m_szTeamList))
		m_teamLimit = true;
	else
		m_teamLimit = false;

	RecountTeams();
}

#include "voice_gamemgr.h"
extern CVoiceGameMgr g_VoiceGameMgr;

void CHalfLifeTeamplay::Think()
{
	///// Check game rules /////
	static int last_frags;
	static int last_time;

	int frags_remaining = 0;
	int time_remaining = 0;

	g_VoiceGameMgr.Update(gpGlobals->frametime);

	if (g_fGameOver) // someone else quit the game already
	{
		CHalfLifeMultiplay::Think();
		return;
	}

	float flTimeLimit = CVAR_GET_FLOAT("mp_timelimit") * 60;

	time_remaining = (int)(0 != flTimeLimit ? (flTimeLimit - gpGlobals->time) : 0);

	if (flTimeLimit != 0 && gpGlobals->time >= flTimeLimit)
	{
		GoToIntermission();
		return;
	}

	float flFragLimit = fraglimit.value;
	if (0 != flFragLimit)
	{
		int bestfrags = 9999;
		int remain;

		// check if any team is over the frag limit
		for (int i = 0; i < num_teams; i++)
		{
			if (team_scores[i] >= flFragLimit)
			{
				GoToIntermission();
				return;
			}

			remain = flFragLimit - team_scores[i];
			if (remain < bestfrags)
			{
				bestfrags = remain;
			}
		}
		frags_remaining = bestfrags;
	}

	// Updates when frags change
	if (frags_remaining != last_frags)
	{
		g_engfuncs.pfnCvar_DirectSet(&fragsleft, UTIL_VarArgs("%i", frags_remaining));
	}

	// Updates once per second
	if (timeleft.value != last_time)
	{
		g_engfuncs.pfnCvar_DirectSet(&timeleft, UTIL_VarArgs("%i", time_remaining));
	}

	last_frags = frags_remaining;
	last_time = time_remaining;
}

//=========================================================
// ClientCommand
// the user has typed a command which is unrecognized by everything else;
// this check to see if the gamerules knows anything about the command
//=========================================================
bool CHalfLifeTeamplay::ClientCommand(CBasePlayer* pPlayer, const char* pcmd)
{
	if (g_VoiceGameMgr.ClientCommand(pPlayer, pcmd))
		return true;

	if (FStrEq(pcmd, "menuselect"))
	{
		if (CMD_ARGC() < 2)
			return true;

		int slot = atoi(CMD_ARGV(1));

		// select the item from the current menu

		return true;
	}

	return false;
}

void CHalfLifeTeamplay::UpdateGameMode(CBasePlayer* pPlayer)
{
	MESSAGE_BEGIN(MSG_ONE, gmsgGameMode, nullptr, pPlayer->edict());
	WRITE_BYTE(1); // game mode teamplay
	MESSAGE_END();
}


const char* CHalfLifeTeamplay::SetDefaultPlayerTeam(CBasePlayer* pPlayer)
{
	// copy out the team name from the model
	char* mdls = g_engfuncs.pfnInfoKeyValue(g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model");
	strncpy(pPlayer->m_szTeamName, mdls, TEAM_NAME_LENGTH);

	RecountTeams();

	// update the current player of the team he is joining
	if (pPlayer->m_szTeamName[0] == '\0' || !IsValidTeam(pPlayer->m_szTeamName) || 0 != defaultteam.value)
	{
		const char* pTeamName = nullptr;

		if (0 != defaultteam.value)
		{
			pTeamName = team_names[0];
		}
		else
		{
			pTeamName = TeamWithFewestPlayers();
		}
		strncpy(pPlayer->m_szTeamName, pTeamName, TEAM_NAME_LENGTH);
	}

	return pPlayer->m_szTeamName;
}


//=========================================================
// InitHUD
//=========================================================
void CHalfLifeTeamplay::InitHUD(CBasePlayer* pPlayer)
{
	int i;

	SetDefaultPlayerTeam(pPlayer);
	CHalfLifeMultiplay::InitHUD(pPlayer);

	// Send down the team names
	MESSAGE_BEGIN(MSG_ONE, gmsgTeamNames, nullptr, pPlayer->edict());
	WRITE_BYTE(num_teams);
	for (i = 0; i < num_teams; i++)
	{
		WRITE_STRING(team_names[i]);
	}
	MESSAGE_END();

	RecountTeams();

	char* mdls = g_engfuncs.pfnInfoKeyValue(g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model");
	// update the current player of the team he is joining
	char text[1024];
	if (0 == strcmp(mdls, pPlayer->m_szTeamName))
	{
		sprintf(text, "* you are on team \'%s\'\n", pPlayer->m_szTeamName);
	}
	else
	{
		sprintf(text, "* assigned to team %s\n", pPlayer->m_szTeamName);
	}

	ChangePlayerTeam(pPlayer, pPlayer->m_szTeamName, false, false);
	UTIL_SayText(text, pPlayer);
	int clientIndex = pPlayer->entindex();
	RecountTeams();
	// update this player with all the other players team info
	// loop through all active players and send their team info to the new client
	for (i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBaseEntity* plr = UTIL_PlayerByIndex(i);
		if ((plr != nullptr) && IsValidTeam(plr->TeamID()))
		{
			MESSAGE_BEGIN(MSG_ONE, gmsgTeamInfo, nullptr, pPlayer->edict());
			WRITE_BYTE(plr->entindex());
			WRITE_STRING(plr->TeamID());
			MESSAGE_END();
		}
	}
}


void CHalfLifeTeamplay::ChangePlayerTeam(CBasePlayer* pPlayer, const char* pTeamName, bool bKill, bool bGib)
{
	int damageFlags = DMG_GENERIC;
	int clientIndex = pPlayer->entindex();

	if (!bGib)
	{
		damageFlags |= DMG_NEVERGIB;
	}
	else
	{
		damageFlags |= DMG_ALWAYSGIB;
	}

	if (bKill)
	{
		// kill the player,  remove a death,  and let them start on the new team
		m_DisableDeathMessages = true;
		m_DisableDeathPenalty = true;

		pPlayer->TakeDamage(CWorld::World->pev, CWorld::World->pev, 900, damageFlags);

		m_DisableDeathMessages = false;
		m_DisableDeathPenalty = false;
	}

	// copy out the team name from the model
	strncpy(pPlayer->m_szTeamName, pTeamName, TEAM_NAME_LENGTH);

	g_engfuncs.pfnSetClientKeyValue(clientIndex, g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model", pPlayer->m_szTeamName);
	g_engfuncs.pfnSetClientKeyValue(clientIndex, g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "team", pPlayer->m_szTeamName);

	// notify everyone's HUD of the team change
	MESSAGE_BEGIN(MSG_ALL, gmsgTeamInfo);
	WRITE_BYTE(clientIndex);
	WRITE_STRING(pPlayer->m_szTeamName);
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_ALL, gmsgScoreInfo);
	WRITE_BYTE(clientIndex);
	WRITE_SHORT(pPlayer->pev->frags);
	WRITE_SHORT(pPlayer->m_iDeaths);
	WRITE_SHORT(0);
	WRITE_SHORT(g_pGameRules->GetTeamIndex(pPlayer->m_szTeamName) + 1);
	MESSAGE_END();
}


//=========================================================
// ClientUserInfoChanged
//=========================================================
void CHalfLifeTeamplay::ClientUserInfoChanged(CBasePlayer* pPlayer, char* infobuffer)
{
	char text[1024];

	// prevent skin/color/model changes
	char* mdls = g_engfuncs.pfnInfoKeyValue(infobuffer, "model");

	if (!stricmp(mdls, pPlayer->m_szTeamName))
		return;

	if (0 != defaultteam.value)
	{
		int clientIndex = pPlayer->entindex();

		g_engfuncs.pfnSetClientKeyValue(clientIndex, g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model", pPlayer->m_szTeamName);
		g_engfuncs.pfnSetClientKeyValue(clientIndex, g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "team", pPlayer->m_szTeamName);
		sprintf(text, "* Not allowed to change teams in this game!\n");
		UTIL_SayText(text, pPlayer);
		return;
	}

	if (0 != defaultteam.value || !IsValidTeam(mdls))
	{
		int clientIndex = pPlayer->entindex();

		g_engfuncs.pfnSetClientKeyValue(clientIndex, g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model", pPlayer->m_szTeamName);
		sprintf(text, "* Can't change team to \'%s\'\n", mdls);
		UTIL_SayText(text, pPlayer);
		sprintf(text, "* Server limits teams to \'%s\'\n", m_szTeamList);
		UTIL_SayText(text, pPlayer);
		return;
	}
	// notify everyone of the team change
	sprintf(text, "* %s has changed to team \'%s\'\n", STRING(pPlayer->pev->netname), mdls);
	UTIL_SayTextAll(text, pPlayer);

	UTIL_LogPrintf("\"%s<%i><%s><%s>\" joined team \"%s\"\n",
		STRING(pPlayer->pev->netname),
		GETPLAYERUSERID(pPlayer->edict()),
		GETPLAYERAUTHID(pPlayer->edict()),
		pPlayer->m_szTeamName,
		mdls);

	ChangePlayerTeam(pPlayer, mdls, true, true);
	// recound stuff
	RecountTeams(true);
}

//=========================================================
// Deathnotice.
//=========================================================
void CHalfLifeTeamplay::DeathNotice(CBasePlayer* pVictim, entvars_t* pKiller, entvars_t* pevInflictor)
{
	if (m_DisableDeathMessages)
		return;

	if ((pVictim != nullptr) && (pKiller != nullptr) && (pKiller->flags & FL_CLIENT) != 0)
	{
		CBasePlayer* pk = (CBasePlayer*)CBaseEntity::Instance(pKiller);

		if (pk != nullptr)
		{
			if ((pk != pVictim) && (PlayerRelationship(pVictim, pk) == GR_TEAMMATE))
			{
				MESSAGE_BEGIN(MSG_ALL, gmsgDeathMsg);
				WRITE_BYTE(ENTINDEX(ENT(pKiller)));		// the killer
				WRITE_BYTE(ENTINDEX(pVictim->edict())); // the victim
				WRITE_STRING("teammate");				// flag this as a teammate kill
				MESSAGE_END();
				return;
			}
		}
	}

	CHalfLifeMultiplay::DeathNotice(pVictim, pKiller, pevInflictor);
}

//=========================================================
//=========================================================
void CHalfLifeTeamplay::PlayerKilled(CBasePlayer* pVictim, entvars_t* pKiller, entvars_t* pInflictor)
{
	if (!m_DisableDeathPenalty)
	{
		CHalfLifeMultiplay::PlayerKilled(pVictim, pKiller, pInflictor);
		RecountTeams();
	}
}


//=========================================================
// IsTeamplay
//=========================================================
bool CHalfLifeTeamplay::IsTeamplay()
{
	return true;
}

bool CHalfLifeTeamplay::FPlayerCanTakeDamage(CBasePlayer* pPlayer, CBaseEntity* pAttacker)
{
	if ((pAttacker != nullptr) && PlayerRelationship(pPlayer, pAttacker) == GR_TEAMMATE)
	{
		// my teammate hit me.
		if ((friendlyfire.value == 0) && (pAttacker != pPlayer))
		{
			// friendly fire is off, and this hit came from someone other than myself,  then don't get hurt
			return false;
		}
	}

	return CHalfLifeMultiplay::FPlayerCanTakeDamage(pPlayer, pAttacker);
}

//=========================================================
//=========================================================
int CHalfLifeTeamplay::PlayerRelationship(CBaseEntity* pPlayer, CBaseEntity* pTarget)
{
	// half life multiplay has a simple concept of Player Relationships.
	// you are either on another player's team, or you are not.
	if ((pPlayer == nullptr) || (pTarget == nullptr) || !pTarget->IsPlayer())
		return GR_NOTTEAMMATE;

	if ((*GetTeamID(pPlayer) != '\0') && (*GetTeamID(pTarget) != '\0') && !stricmp(GetTeamID(pPlayer), GetTeamID(pTarget)))
	{
		return GR_TEAMMATE;
	}

	return GR_NOTTEAMMATE;
}

//=========================================================
//=========================================================
bool CHalfLifeTeamplay::ShouldAutoAim(CBasePlayer* pPlayer, edict_t* target)
{
	// always autoaim, unless target is a teammate
	CBaseEntity* pTgt = CBaseEntity::Instance(target);
	if ((pTgt != nullptr) && pTgt->IsPlayer())
	{
		if (PlayerRelationship(pPlayer, pTgt) == GR_TEAMMATE)
			return false; // don't autoaim at teammates
	}

	return CHalfLifeMultiplay::ShouldAutoAim(pPlayer, target);
}

//=========================================================
//=========================================================
int CHalfLifeTeamplay::IPointsForKill(CBasePlayer* pAttacker, CBasePlayer* pKilled)
{
	if (pKilled == nullptr)
		return 0;

	if (pAttacker == nullptr)
		return 1;

	if (pAttacker != pKilled && PlayerRelationship(pAttacker, pKilled) == GR_TEAMMATE)
		return -1;

	return 1;
}

//=========================================================
//=========================================================
const char* CHalfLifeTeamplay::GetTeamID(CBaseEntity* pEntity)
{
	if (pEntity == nullptr || pEntity->pev == nullptr)
		return "";

	// return their team name
	return pEntity->TeamID();
}


int CHalfLifeTeamplay::GetTeamIndex(const char* pTeamName)
{
	if ((pTeamName != nullptr) && *pTeamName != 0)
	{
		// try to find existing team
		for (int tm = 0; tm < num_teams; tm++)
		{
			if (!stricmp(team_names[tm], pTeamName))
				return tm;
		}
	}

	return -1; // No match
}


const char* CHalfLifeTeamplay::GetIndexedTeamName(int teamIndex)
{
	if (teamIndex < 0 || teamIndex >= num_teams)
		return "";

	return team_names[teamIndex];
}


bool CHalfLifeTeamplay::IsValidTeam(const char* pTeamName)
{
	if (!m_teamLimit) // Any team is valid if the teamlist isn't set
		return true;

	return (GetTeamIndex(pTeamName) != -1) ? true : false;
}

const char* CHalfLifeTeamplay::TeamWithFewestPlayers()
{
	int i;
	int minPlayers = MAX_TEAMS;
	int teamCount[MAX_TEAMS];
	char* pTeamName = nullptr;

	memset(teamCount, 0, MAX_TEAMS * sizeof(int));

	// loop through all clients, count number of players on each team
	for (i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBaseEntity* plr = UTIL_PlayerByIndex(i);

		if (plr != nullptr)
		{
			int team = GetTeamIndex(plr->TeamID());
			if (team >= 0)
				teamCount[team]++;
		}
	}

	// Find team with least players
	for (i = 0; i < num_teams; i++)
	{
		if (teamCount[i] < minPlayers)
		{
			minPlayers = teamCount[i];
			pTeamName = team_names[i];
		}
	}

	return pTeamName;
}


//=========================================================
//=========================================================
void CHalfLifeTeamplay::RecountTeams(bool bResendInfo)
{
	char* pName;
	char teamlist[TEAMPLAY_TEAMLISTLENGTH];

	// loop through all teams, recounting everything
	num_teams = 0;

	// Copy all of the teams from the teamlist
	// make a copy because strtok is destructive
	strcpy(teamlist, m_szTeamList);
	pName = teamlist;
	pName = strtok(pName, ";");
	while (pName != nullptr && '\0' != *pName)
	{
		if (GetTeamIndex(pName) < 0)
		{
			strcpy(team_names[num_teams], pName);
			num_teams++;
		}
		pName = strtok(nullptr, ";");
	}

	if (num_teams < 2)
	{
		num_teams = 0;
		m_teamLimit = false;
	}

	// Sanity check
	memset(team_scores, 0, sizeof(team_scores));

	// loop through all clients
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBaseEntity* plr = UTIL_PlayerByIndex(i);

		if (plr != nullptr)
		{
			const char* pTeamName = plr->TeamID();
			// try add to existing team
			int tm = GetTeamIndex(pTeamName);

			if (tm < 0) // no team match found
			{
				if (!m_teamLimit)
				{
					// add to new team
					tm = num_teams;
					num_teams++;
					team_scores[tm] = 0;
					strncpy(team_names[tm], pTeamName, MAX_TEAMNAME_LENGTH);
				}
			}

			if (tm >= 0)
			{
				team_scores[tm] += plr->pev->frags;
			}

			if (bResendInfo) //Someone's info changed, let's send the team info again.
			{
				if ((plr != nullptr) && IsValidTeam(plr->TeamID()))
				{
					MESSAGE_BEGIN(MSG_ALL, gmsgTeamInfo, nullptr);
					WRITE_BYTE(plr->entindex());
					WRITE_STRING(plr->TeamID());
					MESSAGE_END();
				}
			}
		}
	}
}
