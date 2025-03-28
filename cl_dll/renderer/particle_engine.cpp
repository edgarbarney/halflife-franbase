/*
Trinity Rendering Engine - Copyright Andrew Lucas 2009-2012
FranBase Modbase - Copyright FranticDreamer 2020-2025

The Trinity Engine is free software, distributed in the hope th-
at it will be useful, but WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU Lesser General Public License for more det-
ails.

Particle Engine
Written by Andrew Lucas
*/

#if defined(_WIN32)
#include "windows.h"
#include <GL/glu.h>
#endif
#include "hud.h"
#include "cl_util.h"

#include "const.h"
#include "studio.h"
#include "entity_state.h"
#include "triangleapi.h"
#include "event_api.h"
#include "pm_defs.h"

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <math.h>

#include "propmanager.h"
#include "particle_engine.h"
#include "bsprenderer.h"

#include "r_efx.h"
#include "r_studioint.h"
#include "studio_util.h"
#include "event_api.h"
#include "event_args.h"

#include "StudioModelRenderer.h"
#include "GameStudioModelRenderer.h"
extern CGameStudioModelRenderer g_StudioRenderer;

enum ParticleQuality
{
	verylow = 0,
	low = 1,
	medium = 2,
	high = 3,
};

/*
====================
Init

====================
*/
void CParticleEngine::Init()
{
	m_pCvarDrawParticles = CVAR_CREATE("te_particles", "1", FCVAR_ARCHIVE);
	m_pCvarParticleDebug = CVAR_CREATE("te_particles_debug", "0", 0);
	m_pCvarParticleMaxPart = CVAR_CREATE("te_particle_quality", "2", FCVAR_ARCHIVE);
	m_pCvarGravity = gEngfuncs.pfnGetCvarPointer("sv_gravity");
};

/*
====================
Shutdown

====================
*/
void CParticleEngine::Shutdown()
{
	VidInit();
}

/*
====================
VidInit

====================
*/
void CParticleEngine::VidInit()
{
	if (m_pSystemHeader != nullptr)
	{
		particle_system_t* next = m_pSystemHeader;
		while (next != nullptr)
		{
			particle_system_t* pfree = next;
			next = pfree->next;

			cl_particle_t* pparticle = pfree->particleheader;
			while (pparticle != nullptr)
			{
				cl_particle_t* pfreeparticle = pparticle;
				pparticle = pfreeparticle->next;

				m_iNumFreedParticles++;
				delete[] pfreeparticle;
			}

			m_iNumFreedSystems++;
			delete[] pfree;
		}

		m_pSystemHeader = nullptr;
	}
};

/*
====================
AllocSystem

====================
*/
particle_system_t* CParticleEngine::AllocSystem()
{
	// Allocate memory
	particle_system_t* pSystem = new particle_system_t;
	memset(pSystem, 0, sizeof(particle_system_t));

	// Add system into pointer array
	if (m_pSystemHeader != nullptr)
	{
		m_pSystemHeader->prev = pSystem;
		pSystem->next = m_pSystemHeader;
	}

	m_iNumCreatedSystems++;
	m_pSystemHeader = pSystem;
	return pSystem;
}

/*
====================
AllocParticle

====================
*/
cl_particle_t* CParticleEngine::AllocParticle(particle_system_t* pSystem)
{
	// Allocate memory
	cl_particle_t* pParticle = new cl_particle_t;
	memset(pParticle, 0, sizeof(cl_particle_t));

	// Add system into pointer array
	if (pSystem->particleheader != nullptr)
	{
		pSystem->particleheader->prev = pParticle;
		pParticle->next = pSystem->particleheader;
	}

	m_iNumCreatedParticles++;
	pSystem->particleheader = pParticle;
	return pParticle;
}

/*
====================
CreateCluster

====================
*/
void CParticleEngine::CreateCluster(const std::string& szPath, Vector origin, Vector dir, int iId)
{
	std::string filePath = "/scripts/particles/" + szPath;

	if (!m_particleClusterDataCache.contains(filePath))
	{
		FranUtils::FileSystem::StringMap outputData;
		bool result = FranUtils::FileSystem::ParseBasicFile(filePath, outputData);

		if (!result)
		{
			gEngfuncs.Con_Printf("Could not load particle cluster file: %s!\n", szPath.c_str());
			return;
		}

		m_particleClusterDataCache[filePath] = outputData;
	}

	for (const auto& data : m_particleClusterDataCache.at(filePath))
	{
		CreateSystem(data.first, origin, dir, iId);
	}
}

/*
====================
CreateSystem

====================
*/
particle_system_t* CParticleEngine::CreateSystem(const std::string& path, Vector origin, Vector dir, int iId, particle_system_t* parent)
{
	if (path.empty())
		return nullptr;

	std::string filePath = "/scripts/particles/" + path;

	// Parse if not already cached
	if (!m_particleDataCache.contains(filePath))
	{
		FranUtils::FileSystem::StringMap outputData;
		bool result = FranUtils::FileSystem::ParseBasicFile(filePath, outputData);

		if (!result)
		{
			gEngfuncs.Con_Printf("Could not load particle definitions file: %s!\n", path.c_str());
			return nullptr;
		}

		m_particleDataCache[filePath] = outputData;
	}

	particle_system_t* pSystem = AllocSystem();

	if (pSystem == nullptr)
	{
		gEngfuncs.Con_Printf("Warning! Exceeded max number of particle systems!\n");
		return nullptr;
	}

	// Fill in default values
	pSystem->id = iId;
	pSystem->mainalpha = 1;
	pSystem->spawntime = gEngfuncs.GetClientTime();
	pSystem->dir = dir;

	const auto& outputData = m_particleDataCache[filePath];

	if (outputData.contains("systemshape"))
		pSystem->shapetype = std::stoi(outputData.at("systemshape"));
	if (outputData.contains("minvel"))
		pSystem->minvel = std::stof(outputData.at("minvel"));
	if (outputData.contains("maxvel"))
		pSystem->maxvel = std::stof(outputData.at("maxvel"));
	if (outputData.contains("maxofs"))
		pSystem->maxofs = std::stof(outputData.at("maxofs"));
	if (outputData.contains("fadein"))
		pSystem->fadeintime = std::stof(outputData.at("fadein"));
	if (outputData.contains("fadedelay"))
		pSystem->fadeoutdelay = std::stof(outputData.at("fadedelay"));
	if (outputData.contains("mainalpha"))
		pSystem->mainalpha = std::stof(outputData.at("mainalpha"));
	if (outputData.contains("veldamp"))
		pSystem->velocitydamp = std::stof(outputData.at("veldamp"));
	if (outputData.contains("veldampdelay"))
		pSystem->veldampdelay = std::stof(outputData.at("veldampdelay"));
	if (outputData.contains("life"))
		pSystem->maxlife = std::stof(outputData.at("life"));
	if (outputData.contains("lifevar"))
		pSystem->maxlifevar = std::stof(outputData.at("lifevar"));
	if (outputData.contains("pcolr"))
		pSystem->primarycolor.x = std::stoi(outputData.at("pcolr")) / 255.0f;
	if (outputData.contains("pcolg"))
		pSystem->primarycolor.y = std::stoi(outputData.at("pcolg")) / 255.0f;
	if (outputData.contains("pcolb"))
		pSystem->primarycolor.z = std::stoi(outputData.at("pcolb")) / 255.0f;
	if (outputData.contains("scolr"))
		pSystem->secondarycolor.x = std::stoi(outputData.at("scolr")) / 255.0f;
	if (outputData.contains("scolg"))
		pSystem->secondarycolor.y = std::stoi(outputData.at("scolg")) / 255.0f;
	if (outputData.contains("scolb"))
		pSystem->secondarycolor.z = std::stoi(outputData.at("scolb")) / 255.0f;
	if (outputData.contains("ctransd"))
		pSystem->transitiondelay = std::stof(outputData.at("ctransd"));
	if (outputData.contains("ctranst"))
		pSystem->transitiontime = std::stof(outputData.at("ctranst"));
	if (outputData.contains("ctransv"))
		pSystem->transitionvar = std::stof(outputData.at("ctransv"));
	if (outputData.contains("scale"))
		pSystem->scale = std::stof(outputData.at("scale"));
	if (outputData.contains("scalevar"))
		pSystem->scalevar = std::stof(outputData.at("scalevar"));
	if (outputData.contains("scaledampdelay"))
		pSystem->scaledampdelay = std::stof(outputData.at("scaledampdelay"));
	if (outputData.contains("scaledampfactor"))
		pSystem->scaledampfactor = std::stof(outputData.at("scaledampfactor"));
	if (outputData.contains("gravity"))
		pSystem->gravity = std::stof(outputData.at("gravity"));
	if (outputData.contains("systemsize"))
		pSystem->systemsize = std::stoi(outputData.at("systemsize"));
	if (outputData.contains("maxparticles"))
		pSystem->maxparticles = std::stoi(outputData.at("maxparticles"));
	if (outputData.contains("intensity"))
		pSystem->particlefreq = std::stof(outputData.at("intensity"));
	if (outputData.contains("startparticles"))
		pSystem->startparticles = std::stoi(outputData.at("startparticles"));
	if (outputData.contains("maxparticlevar"))
		pSystem->maxparticlevar = std::stoi(outputData.at("maxparticlevar"));
	if (outputData.contains("lightmaps"))
		pSystem->lightcheck = std::stoi(outputData.at("lightmaps"));
	if (outputData.contains("collision"))
		pSystem->collision = std::stoi(outputData.at("collision"));
	if (outputData.contains("colwater"))
		pSystem->colwater = std::stoi(outputData.at("colwater"));
	if (outputData.contains("rendermode"))
		pSystem->rendermode = std::stoi(outputData.at("rendermode"));
	if (outputData.contains("display"))
		pSystem->displaytype = std::stoi(outputData.at("display"));
	if (outputData.contains("impactdamp"))
		pSystem->impactdamp = std::stof(outputData.at("impactdamp"));
	if (outputData.contains("rotationvar"))
		pSystem->rotationvar = std::stof(outputData.at("rotationvar"));
	if (outputData.contains("rotationvel"))
		pSystem->rotationvel = std::stof(outputData.at("rotationvel"));
	if (outputData.contains("rotationdamp"))
		pSystem->rotationdamp = std::stof(outputData.at("rotationdamp"));
	if (outputData.contains("rotationdampdelay"))
		pSystem->rotationdampdelay = std::stof(outputData.at("rotationdampdelay"));
	if (outputData.contains("rotxvar"))
		pSystem->rotxvar = std::stof(outputData.at("rotxvar"));
	if (outputData.contains("rotxvel"))
		pSystem->rotxvel = std::stof(outputData.at("rotxvel"));
	if (outputData.contains("rotxdamp"))
		pSystem->rotxdamp = std::stof(outputData.at("rotxdamp"));
	if (outputData.contains("rotxdampdelay"))
		pSystem->rotxdampdelay = std::stof(outputData.at("rotxdampdelay"));
	if (outputData.contains("rotyvar"))
		pSystem->rotyvar = std::stof(outputData.at("rotyvar"));
	if (outputData.contains("rotyvel"))
		pSystem->rotyvel = std::stof(outputData.at("rotyvel"));
	if (outputData.contains("rotydamp"))
		pSystem->rotydamp = std::stof(outputData.at("rotydamp"));
	if (outputData.contains("rotydampdelay"))
		pSystem->rotydampdelay = std::stof(outputData.at("rotydampdelay"));
	if (outputData.contains("randomdir"))
		pSystem->randomdir = std::stoi(outputData.at("randomdir"));
	if (outputData.contains("overbright"))
		pSystem->overbright = std::stoi(outputData.at("overbright"));
	if (outputData.contains("create"))
		std::strcpy(pSystem->create, outputData.at("create").c_str());
	if (outputData.contains("deathcreate"))
		std::strcpy(pSystem->deathcreate,outputData.at("deathcreate").c_str());
	if (outputData.contains("watercreate"))
		std::strcpy(pSystem->watercreate, outputData.at("watercreate").c_str());
	if (outputData.contains("windx"))
		pSystem->windx = std::stof(outputData.at("windx"));
	if (outputData.contains("windy"))
		pSystem->windy = std::stof(outputData.at("windy"));
	if (outputData.contains("windvar"))
		pSystem->windvar = std::stof(outputData.at("windvar"));
	if (outputData.contains("windtype"))
		pSystem->windtype = std::stoi(outputData.at("windtype"));
	if (outputData.contains("windmult"))
		pSystem->windmult = std::stof(outputData.at("windmult"));
	if (outputData.contains("windmultvar"))
		pSystem->windmultvar = std::stof(outputData.at("windmultvar"));
	if (outputData.contains("stuckdie"))
		pSystem->stuckdie = std::stof(outputData.at("stuckdie"));
	if (outputData.contains("maxheight"))
		pSystem->maxheight = std::stof(outputData.at("maxheight"));
	if (outputData.contains("tracerdist"))
		pSystem->tracerdist = std::stof(outputData.at("tracerdist"));
	if (outputData.contains("fadedistnear"))
		pSystem->fadedistnear = std::stoi(outputData.at("fadedistnear"));
	if (outputData.contains("fadedistfar"))
		pSystem->fadedistfar = std::stoi(outputData.at("fadedistfar"));
	if (outputData.contains("numframes"))
		pSystem->numframes = std::stoi(outputData.at("numframes"));
	if (outputData.contains("framesizex"))
		pSystem->framesizex = std::stoi(outputData.at("framesizex"));
	if (outputData.contains("framesizey"))
		pSystem->framesizey = std::stoi(outputData.at("framesizey"));
	if (outputData.contains("framerate"))
		pSystem->framerate = std::stoi(outputData.at("framerate"));
	if (outputData.contains("texture"))
	{
		int iOriginalBind;
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &iOriginalBind);

		std::string texPath = "gfx/textures/particles/" + outputData.at("texture") + ".dds";

		pSystem->texture = gTextureLoader.LoadTexture(texPath.c_str());

		if (pSystem->texture == nullptr)
		{
			// Remove system
			m_pSystemHeader = pSystem->next;
			m_pSystemHeader->prev = nullptr;
			delete[] pSystem;

			return nullptr;
		}

		glBindTexture(GL_TEXTURE_2D, pSystem->texture->iIndex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glBindTexture(GL_TEXTURE_2D, iOriginalBind);
	}

	if ((int)m_pCvarParticleMaxPart->value == ParticleQuality::verylow)
	{
		pSystem->maxparticles = 1;
	}
	else if ((int)m_pCvarParticleMaxPart->value == ParticleQuality::low)
	{
		pSystem->maxparticles = 2;
	}
	else if ((int)m_pCvarParticleMaxPart->value == ParticleQuality::medium)
	{
		pSystem->maxparticles = 4;
	}
	else if ((int)m_pCvarParticleMaxPart->value == ParticleQuality::high) // Placeholder
	{
		// pSystem->maxparticles = 4;
	}
	// pSystem->maxparticles = m_pCvarParticleMaxPart->value;

	if (pSystem->shapetype != SYSTEM_SHAPE_PLANE_ABOVE_PLAYER)
	{
		if (parent == nullptr)
		{
			model_t* pWorld = IEngineStudio.GetModelByIndex(1);
			pSystem->origin = origin;

			if (pWorld != nullptr)
				pSystem->leaf = Mod_PointInLeaf(pSystem->origin, pWorld);
		}
		else
		{
			pSystem->leaf = parent->leaf;
		}
	}
	else
	{
		pmtrace_t tr;
		gEngfuncs.pEventAPI->EV_SetTraceHull(2);
		gEngfuncs.pEventAPI->EV_PlayerTrace(origin, origin + Vector(0, 0, 160000), PM_STUDIO_IGNORE, -1, &tr);

		if (tr.fraction == 1.0)
		{
			// Remove system
			m_pSystemHeader = pSystem->next;
			m_pSystemHeader->prev = nullptr;
			delete[] pSystem;

			return nullptr;
		}

		pSystem->skyheight = tr.endpos.z;
	}

	if (pSystem->collision != PARTICLE_COLLISION_DECAL)
	{
		if (pSystem->create[0] != 0)
			pSystem->createsystem = CreateSystem(pSystem->create, pSystem->origin, pSystem->dir, 0, pSystem);

		if (pSystem->createsystem == nullptr)
			memset(pSystem->create, 0, sizeof(pSystem->create));
	}

	if (pSystem->watercreate[0] != 0)
		pSystem->watersystem = CreateSystem(pSystem->watercreate, pSystem->origin, pSystem->dir, 0, pSystem);

	if (pSystem->watersystem == nullptr)
		memset(pSystem->watercreate, 0, sizeof(pSystem->watercreate));

	if (parent != nullptr)
	{
		// Child systems cannot spawn on their own
		pSystem->parentsystem = parent;
		pSystem->maxparticles = NULL;
		pSystem->particlefreq = NULL;
	}
	else
	{
		if ((pSystem->shapetype != SYSTEM_SHAPE_PLANE_ABOVE_PLAYER) && (pSystem->shapetype != SYSTEM_SHAPE_BOX_AROUND_PLAYER))
		{
			// create all starting particles
			for (int i = 0; i < pSystem->startparticles; i++)
				CreateParticle(pSystem);
		}
		else
		{
			// Create particles at random heights
			EnvironmentCreateFirst(pSystem);
		}
	}

	return pSystem;
}

/*
====================
EnvironmentCreateFirst

====================
*/
void CParticleEngine::EnvironmentCreateFirst(particle_system_t* pSystem)
{
	Vector vOrigin;
	int iNumParticles = pSystem->particlefreq * 4;
	Vector vPlayer = gEngfuncs.GetLocalPlayer()->origin;

	// Spawn particles inbetween the view origin and maxheight
	for (int i = 0; i < iNumParticles; i++)
	{
		if (pSystem->shapetype == SYSTEM_SHAPE_PLANE_ABOVE_PLAYER)
		{
			vOrigin[0] = vPlayer[0] + gEngfuncs.pfnRandomLong(-pSystem->systemsize, pSystem->systemsize);
			vOrigin[1] = vPlayer[1] + gEngfuncs.pfnRandomLong(-pSystem->systemsize, pSystem->systemsize);

			if (pSystem->maxheight != 0.0f)
			{
				vOrigin[2] = vPlayer[2] + pSystem->maxheight;

				if (vOrigin[2] > pSystem->skyheight)
					vOrigin[2] = pSystem->skyheight;
			}
			else
			{
				vOrigin[2] = pSystem->skyheight;
			}

			vOrigin[2] = gEngfuncs.pfnRandomFloat(vPlayer[2], vOrigin[2]);

			pmtrace_t pmtrace;
			gEngfuncs.pEventAPI->EV_SetTraceHull(2);
			gEngfuncs.pEventAPI->EV_PlayerTrace(vOrigin, Vector(vOrigin[0], vOrigin[1], pSystem->skyheight - 8), PM_STUDIO_IGNORE, -1, &pmtrace);

			if ((pmtrace.allsolid != 0) || pmtrace.fraction != 1.0)
				continue;
		}
		else if (pSystem->shapetype == SYSTEM_SHAPE_BOX_AROUND_PLAYER)
		{
			if ((gEngfuncs.GetLocalPlayer() != nullptr) && (gHUD.pparams != nullptr))
			{
				Vector vPlayer = gEngfuncs.GetLocalPlayer()->origin;
				Vector vSpeed = gHUD.pparams->simvel;

				vOrigin[0] = vPlayer[0] + vSpeed[0] + gEngfuncs.pfnRandomLong(-pSystem->systemsize, pSystem->systemsize);
				vOrigin[1] = vPlayer[1] + vSpeed[1] + gEngfuncs.pfnRandomLong(-pSystem->systemsize, pSystem->systemsize);
				vOrigin[2] = vPlayer[2] + vSpeed[2] + gEngfuncs.pfnRandomLong(-pSystem->systemsize, pSystem->systemsize);
			}

			// gEngfuncs.Con_Printf("idk if this works \n");
		}

		CreateParticle(pSystem, vOrigin);
	}
}

/*
====================
CreateParticle

====================
*/
void CParticleEngine::CreateParticle(particle_system_t* pSystem, float* flOrigin, float* flNormal)
{
	Vector vBaseOrigin;
	Vector vForward, vUp, vRight;
	cl_particle_t* pParticle = AllocParticle(pSystem);

	if (pParticle == nullptr)
		return;

	pParticle->pSystem = pSystem;
	pParticle->spawntime = gEngfuncs.GetClientTime();
	pParticle->frame = -1;

	if (pSystem->shapetype == SYSTEM_SHAPE_PLANE_ABOVE_PLAYER || pSystem->shapetype == SYSTEM_SHAPE_BOX_AROUND_PLAYER)
	{
		vForward[0] = 0;
		vForward[1] = 0;
		vForward[2] = -1;
	}
	else if (pSystem->randomdir != 0)
	{
		vForward[0] = gEngfuncs.pfnRandomFloat(-1, 1);
		vForward[1] = gEngfuncs.pfnRandomFloat(-1, 1);
		vForward[2] = gEngfuncs.pfnRandomFloat(-1, 1);
	}
	else if ((flOrigin != nullptr) && (flNormal != nullptr))
	{
		vForward[0] = flNormal[0];
		vForward[1] = flNormal[1];
		vForward[2] = flNormal[2];
	}
	else
	{
		vForward[0] = pSystem->dir[0];
		vForward[1] = pSystem->dir[1];
		vForward[2] = pSystem->dir[2];
	}

	if (flNormal != nullptr)
	{
		pParticle->normal[0] = flNormal[0];
		pParticle->normal[1] = flNormal[1];
		pParticle->normal[2] = flNormal[2];
	}
	else
	{
		pParticle->normal[0] = pSystem->dir[0];
		pParticle->normal[1] = pSystem->dir[1];
		pParticle->normal[2] = pSystem->dir[2];
	}

	VectorClear(vUp);
	VectorClear(vRight);

	gBSPRenderer.GetUpRight(vForward, vUp, vRight);
	VectorMASSE(pParticle->velocity, gEngfuncs.pfnRandomFloat(pSystem->minvel, pSystem->maxvel), vForward, pParticle->velocity);
	VectorMASSE(pParticle->velocity, gEngfuncs.pfnRandomFloat(-pSystem->maxofs, pSystem->maxofs), vRight, pParticle->velocity);
	VectorMASSE(pParticle->velocity, gEngfuncs.pfnRandomFloat(-pSystem->maxofs, pSystem->maxofs), vUp, pParticle->velocity);

	if (pSystem->maxlife == -1)
		pParticle->life = pSystem->maxlife;
	else
		pParticle->life = gEngfuncs.GetClientTime() + pSystem->maxlife + gEngfuncs.pfnRandomFloat(-pSystem->maxlifevar, pSystem->maxlifevar);

	pParticle->scale = pSystem->scale + gEngfuncs.pfnRandomFloat(-pSystem->scalevar, pSystem->scalevar);
	pParticle->rotationvel = pSystem->rotationvel + gEngfuncs.pfnRandomFloat(-pSystem->rotationvar, pSystem->rotationvar);
	pParticle->rotxvel = pSystem->rotxvel + gEngfuncs.pfnRandomFloat(-pSystem->rotxvar, pSystem->rotxvar);
	pParticle->rotyvel = pSystem->rotyvel + gEngfuncs.pfnRandomFloat(-pSystem->rotyvar, pSystem->rotyvar);

	if (flOrigin != nullptr)
	{
		VectorCopy(flOrigin, vBaseOrigin);

		if (flNormal != nullptr)
			VectorMA(vBaseOrigin, 0.1, flNormal, vBaseOrigin);
	}
	else
	{
		VectorCopy(pSystem->origin, vBaseOrigin);
	}

	if (pSystem->shapetype == SYSTEM_SHAPE_POINT)
	{
		VectorCopy(vBaseOrigin, pParticle->origin);
	}
	else if (pSystem->shapetype == SYSTEM_SHAPE_BOX)
	{
		pParticle->origin[0] = vBaseOrigin[0] + gEngfuncs.pfnRandomLong(-pSystem->systemsize, pSystem->systemsize);
		pParticle->origin[1] = vBaseOrigin[1] + gEngfuncs.pfnRandomLong(-pSystem->systemsize, pSystem->systemsize);
		pParticle->origin[2] = vBaseOrigin[2] + gEngfuncs.pfnRandomLong(-pSystem->systemsize, pSystem->systemsize);
	}
	else if (pSystem->shapetype == SYSTEM_SHAPE_BOX_AROUND_PLAYER)
	{
		if (gHUD.pparams == nullptr)
			return;

		Vector vPlayer = gEngfuncs.GetLocalPlayer()->origin;
		Vector vSpeed = gHUD.pparams->simvel;
		pParticle->origin[0] = vPlayer[0] + vSpeed[0] + gEngfuncs.pfnRandomLong(-pSystem->systemsize, pSystem->systemsize);
		pParticle->origin[1] = vPlayer[1] + vSpeed[1] + gEngfuncs.pfnRandomLong(-pSystem->systemsize, pSystem->systemsize);
		pParticle->origin[2] = vPlayer[2] + vSpeed[2] + gEngfuncs.pfnRandomLong(-pSystem->systemsize, pSystem->systemsize);

		// gEngfuncs.Con_Printf("idk if this works \n");
	}
	else if (pSystem->shapetype == SYSTEM_SHAPE_PLANE_ABOVE_PLAYER)
	{
		if (flOrigin == nullptr)
		{
			Vector vPlayer = gEngfuncs.GetLocalPlayer()->origin;
			pParticle->origin[0] = vPlayer[0] + gEngfuncs.pfnRandomLong(-pSystem->systemsize, pSystem->systemsize);
			pParticle->origin[1] = vPlayer[1] + gEngfuncs.pfnRandomLong(-pSystem->systemsize, pSystem->systemsize);

			if (pSystem->maxheight != 0.0f)
			{
				pParticle->origin[2] = vPlayer[2] + pSystem->maxheight;

				if (pParticle->origin[2] > pSystem->skyheight)
					pParticle->origin[2] = pSystem->skyheight;
			}
			else
			{
				pParticle->origin[2] = pSystem->skyheight;
			}
		}
		else
		{
			VectorCopy(flOrigin, pParticle->origin);
		}
	}

	if (pParticle->rotationvel != 0.0f)
		pParticle->rotation = gEngfuncs.pfnRandomFloat(0, 360);

	if (pParticle->rotxvel != 0.0f)
		pParticle->rotx = gEngfuncs.pfnRandomFloat(0, 360);

	if (pParticle->rotyvel != 0.0f)
		pParticle->roty = gEngfuncs.pfnRandomFloat(0, 360);

	if (pSystem->fadeintime == 0.0f)
		pParticle->alpha = 1;

	if (pSystem->fadeoutdelay != 0.0f)
		pParticle->fadeoutdelay = pSystem->fadeoutdelay;

	if (pSystem->scaledampdelay != 0.0f)
		pParticle->scaledampdelay = gEngfuncs.GetClientTime() + pSystem->scaledampdelay + gEngfuncs.pfnRandomFloat(-pSystem->scalevar, pSystem->scalevar);

	if ((pSystem->transitiondelay != 0.0f) && (pSystem->transitiontime != 0.0f))
	{
		pParticle->secondarydelay = gEngfuncs.GetClientTime() + pSystem->transitiondelay + gEngfuncs.pfnRandomFloat(-pSystem->transitionvar, pSystem->transitionvar);
		pParticle->secondarytime = pSystem->transitiontime + gEngfuncs.pfnRandomFloat(-pSystem->transitionvar, pSystem->transitionvar);
	}

	if (pSystem->windtype != 0)
	{
		pParticle->windmult = pSystem->windmult + gEngfuncs.pfnRandomFloat(-pSystem->windmultvar, pSystem->windmultvar);
		pParticle->windxvel = pSystem->windx + gEngfuncs.pfnRandomFloat(-pSystem->windvar, pSystem->windvar);
		pParticle->windyvel = pSystem->windy + gEngfuncs.pfnRandomFloat(-pSystem->windvar, pSystem->windvar);
	}

	if (pSystem->numframes == 0)
	{
		pParticle->texcoords[0][0] = 0;
		pParticle->texcoords[0][1] = 0;
		pParticle->texcoords[1][0] = 1;
		pParticle->texcoords[1][1] = 0;
		pParticle->texcoords[2][0] = 1;
		pParticle->texcoords[2][1] = 1;
		pParticle->texcoords[3][0] = 0;
		pParticle->texcoords[3][1] = 1;
	}
	else
	{
		// Calculate these only once
		float flFractionWidth = (float)pSystem->framesizex / (float)pSystem->texture->iWidth;
		float flFractionHeight = (float)pSystem->framesizey / (float)pSystem->texture->iHeight;

		// Calculate top left coordinate
		pParticle->texcoords[0][0] = flFractionWidth;
		pParticle->texcoords[0][1] = 0;

		// Calculate top right coordinate
		pParticle->texcoords[1][0] = 0;
		pParticle->texcoords[1][1] = 0;

		// Calculate bottom right coordinate
		pParticle->texcoords[2][0] = 0;
		pParticle->texcoords[2][1] = flFractionHeight;

		// Calculate bottom left coordinate
		pParticle->texcoords[3][0] = flFractionWidth;
		pParticle->texcoords[3][1] = flFractionHeight;
	}

	VectorCopy(pSystem->primarycolor, pParticle->color);
	VectorCopy(pSystem->secondarycolor, pParticle->scolor);
	VectorCopy(pParticle->origin, pParticle->lastspawn);

	for (int i = 0; i < 3; i++)
	{
		if (pParticle->scolor[i] == -1)
			pParticle->scolor[i] = gEngfuncs.pfnRandomFloat(0, 1);
	}

	if (pSystem->lightcheck != PARTICLE_LIGHTCHECK_NONE)
	{
		if (pSystem->lightcheck == PARTICLE_LIGHTCHECK_NORMAL)
		{
			pParticle->color = LightForParticle(pParticle);
		}
		else if (pSystem->lightcheck == PARTICLE_LIGHTCHECK_SCOLOR)
		{
			pParticle->scolor = LightForParticle(pParticle);
		}
		else if (pSystem->lightcheck == PARTICLE_LIGHTCHECK_MIXP)
		{
			pParticle->color = LightForParticle(pParticle);
			pParticle->color.x = pParticle->color.x * pSystem->primarycolor.x;
			pParticle->color.y = pParticle->color.y * pSystem->primarycolor.y;
			pParticle->color.z = pParticle->color.z * pSystem->primarycolor.z;
		}
	}
}

/*
====================
Update

====================
*/
void CParticleEngine::Update()
{
	// moved to imgui_manager.cpp
	/*
	if(m_pCvarParticleDebug->value)
	{
		gEngfuncs.Con_Printf("Created Particles: %i, Freed Particles %i, Active Particles: %i\nCreated Systems: %i, Freed Systems: %i, Active Systems: %i\n\n",
			m_iNumCreatedParticles, m_iNumFreedParticles,m_iNumCreatedParticles-m_iNumFreedParticles, m_iNumCreatedSystems, m_iNumFreedSystems, m_iNumCreatedSystems-m_iNumFreedSystems);
	}
	*/

	if (m_pCvarDrawParticles->value < 1)
		return;

	m_flFrameTime = gEngfuncs.GetClientTime() - m_flLastDraw;
	m_flLastDraw = gEngfuncs.GetClientTime();

	if (m_flFrameTime > 1)
		m_flFrameTime = 1;

	if (m_flFrameTime <= 0)
		return;

	// No systems to check on
	if (m_pSystemHeader == nullptr)
		return;

	UpdateSystems();

	// Update all particles
	particle_system_t* psystem = m_pSystemHeader;
	while (psystem != nullptr)
	{
		cl_particle_t* pparticle = psystem->particleheader;
		while (pparticle != nullptr)
		{
			if (!UpdateParticle(pparticle))
			{
				cl_particle_t* pfree = pparticle;
				pparticle = pfree->next;

				if (pfree->prev == nullptr)
				{
					psystem->particleheader = pparticle;
					if (pparticle != nullptr)
						pparticle->prev = nullptr;
				}
				else
				{
					pfree->prev->next = pparticle;
					if (pparticle != nullptr)
						pparticle->prev = pfree->prev;
				}

				m_iNumFreedParticles++;
				delete[] pfree;
				continue;
			}
			cl_particle_t* pnext = pparticle->next;
			pparticle = pnext;
		}

		particle_system_t* pnext = psystem->next;
		psystem = pnext;
	}
}

/*
====================
UpdateSystems

====================
*/
void CParticleEngine::UpdateSystems()
{
	float flTime = gEngfuncs.GetClientTime();

	// check if any systems are available for removal
	particle_system_t* next = m_pSystemHeader;
	while (next != nullptr)
	{
		if (next->maxparticles != 0)
		{
			particle_system_t* pnext = next->next;
			next = pnext;
			continue;
		}

		if (next->parentsystem != nullptr)
		{
			particle_system_t* pnext = next->next;
			next = pnext;
			continue;
		}

		// Has related particles
		if (next->particleheader != nullptr)
		{
			particle_system_t* pnext = next->next;
			next = pnext;
			continue;
		}

		// Unparent these and let the engine handle them
		if (next->createsystem != nullptr)
			next->createsystem->parentsystem = nullptr;

		if (next->watersystem != nullptr)
			next->watersystem->parentsystem = nullptr;

		particle_system_t* pfree = next;
		next = pfree->next;

		if (pfree->prev == nullptr)
		{
			m_pSystemHeader = next;
			if (next != nullptr)
				next->prev = nullptr;
		}
		else
		{
			pfree->prev->next = next;
			if (next != nullptr)
				next->prev = pfree->prev;
		}

		// Delete from memory
		m_iNumFreedSystems++;
		delete[] pfree;
	}

	// Update systems
	next = m_pSystemHeader;
	while (next != nullptr)
	{
		// Parented systems cannot spawn particles themselves
		if (next->parentsystem != nullptr)
		{
			particle_system_t* pnext = next->next;
			next = pnext;
			continue;
		}

		float flLife = gEngfuncs.GetClientTime() - next->spawntime;
		float flFreq = 1 / (float)next->particlefreq;
		int iTimesSpawn = flLife / flFreq;

		if (iTimesSpawn <= next->numspawns)
		{
			particle_system_t* pnext = next->next;
			next = pnext;
			continue;
		}

		int iNumSpawn = iTimesSpawn - next->numspawns;

		// cap if finite
		if (next->maxparticles != -1)
		{
			if (next->maxparticles < iNumSpawn)
				iNumSpawn = next->maxparticles;
		}

		if (next->maxparticlevar != 0)
		{
			// Calculate variation
			int iNewAmount = iNumSpawn + abs((sin(flTime) / 2.4492) * next->maxparticlevar);

			// Create new particles
			for (int j = 0; j < iNewAmount; j++)
				CreateParticle(next);

			// Add to counter
			next->numspawns += iNumSpawn;

			// don't take off for infinite systems
			if (next->maxparticles != -1)
				next->maxparticles -= iNumSpawn;
		}
		else
		{
			// Create new particles
			for (int j = 0; j < iNumSpawn; j++)
				CreateParticle(next);

			// Add to counter
			next->numspawns += iNumSpawn;

			// don't take off for infinite systems
			if (next->maxparticles != -1)
				next->maxparticles -= iNumSpawn;
		}

		particle_system_t* pnext = next->next;
		next = pnext;
	}
}

/*
====================
CheckLightBBox

====================
*/
bool CParticleEngine::CheckLightBBox(cl_particle_t* pParticle, cl_dlight_t* pLight)
{
	if (pParticle->origin[0] > (pLight->origin[0] - pLight->radius) && pParticle->origin[1] > (pLight->origin[1] - pLight->radius) && pParticle->origin[2] > (pLight->origin[2] - pLight->radius) && pParticle->origin[0] < (pLight->origin[0] + pLight->radius) && pParticle->origin[1] < (pLight->origin[1] + pLight->radius) && pParticle->origin[2] < (pLight->origin[2] + pLight->radius))
		return false;

	return true;
}

/*
====================
LightForParticle

====================
*/
Vector CParticleEngine::LightForParticle(cl_particle_t* pParticle)
{
	float flRad;
	float flDist;
	float flAtten;
	float flCos;

	Vector vDir;
	Vector vNorm;
	Vector vForward;

	float flTime = gEngfuncs.GetClientTime();
	model_t* pWorld = IEngineStudio.GetModelByIndex(1);
	Vector vEndPos = pParticle->origin - Vector(0, 0, 8964);
	Vector vColor = Vector(0, 0, 0);

	g_StudioRenderer.StudioRecursiveLightPoint(nullptr, pWorld->nodes, pParticle->origin, vEndPos, vColor, false, true);
	cl_dlight_t* pLight = gBSPRenderer.m_pDynLights;

	for (int i = 0; i < MAX_DYNLIGHTS; i++, pLight++)
	{
		if (pLight->die < flTime || (pLight->radius == 0.0f))
			continue;

		if (pLight->cone_size != 0.0f)
		{
			if (pLight->frustum.CullBox(pParticle->origin, pParticle->origin))
				continue;

			Vector vAngles = pLight->angles;
			FixVectorForSpotlight(vAngles);
			AngleVectors(vAngles, vForward, nullptr, nullptr);
		}
		else
		{
			if (CheckLightBBox(pParticle, pLight))
				continue;
		}

		flRad = pLight->radius * pLight->radius;
		VectorSubtract(pParticle->origin, pLight->origin, vDir);
		DotProductSSE(&flDist, vDir, vDir);
		flAtten = (flDist / flRad - 1) * -1;

		if (pLight->cone_size != 0.0f)
		{
			VectorNormalizeFast(vDir);
			flCos = cos((pLight->cone_size * 2) * 0.3 * (M_PI * 2 / 360));
			DotProductSSE(&flDist, vForward, vDir);

			if (flDist < 0 || flDist < flCos)
				continue;

			flAtten *= (flDist - flCos) / (1.0 - flCos);
		}

		if (flAtten <= 0)
			continue;

		VectorMASSE(vColor, flAtten, pLight->color, vColor);
	}

	return vColor;
}

/*
====================
UpdateParticle

====================
*/
bool CParticleEngine::UpdateParticle(cl_particle_t* pParticle)
{
	pmtrace_t pmtrace;
	bool bColWater = false;

	float flTime = gEngfuncs.GetClientTime();
	Vector vFinalVelocity = pParticle->velocity;
	particle_system_t* pSystem = pParticle->pSystem;

	//
	// Check if the particle is ready to die
	//
	if (pParticle->life != -1)
	{
		if (pParticle->life <= flTime)
		{
			if (pSystem->deathcreate[0] != 0)
				CreateSystem(pSystem->deathcreate, pParticle->origin, pParticle->velocity.Normalize(), 0);

			return false; // remove
		}
	}

	//
	// Damp velocity
	//
	if ((pSystem->velocitydamp != 0.0f) && (pParticle->spawntime + pSystem->veldampdelay) < flTime)
		VectorScale(vFinalVelocity, (1.0 - pSystem->velocitydamp * m_flFrameTime), vFinalVelocity);

	//
	// Add gravity before collision test
	//
	vFinalVelocity.z -= m_pCvarGravity->value * pSystem->gravity * m_flFrameTime;

	//
	// Add in wind on either axes
	//
	if (pSystem->windtype != 0)
	{
		if (pParticle->windxvel != 0.0f)
		{
			if (pSystem->windtype == PARTICLE_WIND_LINEAR)
				vFinalVelocity.x += pParticle->windxvel * m_flFrameTime;
			else
				vFinalVelocity.x += sin((flTime * pParticle->windmult)) * pParticle->windxvel * m_flFrameTime;
		}
		if (pParticle->windyvel != 0.0f)
		{
			if (pSystem->windtype == PARTICLE_WIND_LINEAR)
				vFinalVelocity.y += pParticle->windyvel * m_flFrameTime;
			else
				vFinalVelocity.y += sin((flTime * pParticle->windmult)) * pParticle->windyvel * m_flFrameTime;
		}
	}

	//
	// Calculate rotation on all axes
	//
	if (pSystem->rotationvel != 0.0f)
	{
		if ((pSystem->rotationdamp != 0.0f) && (pParticle->rotationvel != 0.0f))
		{
			if ((pSystem->rotationdampdelay + pParticle->spawntime) < flTime)
				pParticle->rotationvel = pParticle->rotationvel * (1.0 - pSystem->rotationdamp);
		}

		pParticle->rotation += pParticle->rotationvel * m_flFrameTime;

		if (pParticle->rotation < 0)
			pParticle->rotation += 360;
		if (pParticle->rotation > 360)
			pParticle->rotation -= 360;
	}
	if (pSystem->rotxvel != 0.0f)
	{
		if ((pSystem->rotxdamp != 0.0f) && (pParticle->rotxvel != 0.0f))
		{
			if ((pSystem->rotxdampdelay + pParticle->spawntime) < flTime)
				pParticle->rotxvel = pParticle->rotxvel * (1.0 - pSystem->rotxdamp);
		}

		pParticle->rotx += pParticle->rotxvel * m_flFrameTime;

		if (pParticle->rotx < 0)
			pParticle->rotx += 360;
		if (pParticle->rotx > 360)
			pParticle->rotx -= 360;
	}
	if (pSystem->rotyvel != 0.0f)
	{
		if ((pSystem->rotydamp != 0.0f) && (pParticle->rotyvel != 0.0f))
		{
			if ((pSystem->rotydampdelay + pParticle->spawntime) < flTime)
				pParticle->rotyvel = pParticle->rotyvel * (1.0 - pSystem->rotydamp);
		}

		pParticle->roty += pParticle->rotyvel * m_flFrameTime;

		if (pParticle->roty < 0)
			pParticle->roty += 360;
		if (pParticle->roty > 360)
			pParticle->roty -= 360;
	}

	//
	// Collision detection
	//
	if (pSystem->collision != 0)
	{
		gEngfuncs.pEventAPI->EV_SetTraceHull(2);
		gEngfuncs.pEventAPI->EV_PlayerTrace(pParticle->origin, (pParticle->origin + vFinalVelocity * m_flFrameTime), PM_WORLD_ONLY, -1, &pmtrace);

		if (pmtrace.allsolid != 0)
			return false; // Probably spawned inside a solid

		if (pSystem->colwater != 0)
		{
			if (gEngfuncs.PM_PointContents(pParticle->origin + vFinalVelocity * m_flFrameTime, nullptr) == CONTENTS_WATER)
			{
				pmtrace.endpos = pParticle->origin + vFinalVelocity * m_flFrameTime;
				int iEntity = gEngfuncs.PM_WaterEntity(pParticle->origin + vFinalVelocity * m_flFrameTime);

				if (iEntity != 0)
				{
					cl_entity_t* pEntity = gEngfuncs.GetEntityByIndex(iEntity);
					pmtrace.endpos.z = pEntity->model->maxs.z + 0.001;
				}

				pmtrace.plane.normal = Vector(0, 0, 1);
				pmtrace.fraction = 0;
				bColWater = true;
			}
		}

		if (pmtrace.fraction != 1.0)
		{
			if (pSystem->collision == PARTICLE_COLLISION_STUCK)
			{
				if (gEngfuncs.PM_PointContents(pmtrace.endpos, nullptr) == CONTENTS_SKY)
					return false;

				if (pParticle->life == -1 && (pSystem->stuckdie != 0.0f))
				{
					pParticle->life = gEngfuncs.GetClientTime() + pSystem->stuckdie;
					pParticle->fadeoutdelay = gEngfuncs.GetClientTime() - pParticle->spawntime;
				}
				VectorMASSE(pParticle->origin, pmtrace.fraction * m_flFrameTime, vFinalVelocity, pParticle->origin);

				pParticle->rotationvel = NULL;
				pParticle->rotxvel = NULL;
				pParticle->rotyvel = NULL;

				VectorClear(pParticle->velocity);
				VectorClear(vFinalVelocity);
			}
			else if (pSystem->collision == PARTICLE_COLLISION_BOUNCE)
			{
				float fProj /* = DotProduct(vFinalVelocity, pmtrace.plane.normal)*/;
				DotProductSSE(&fProj, vFinalVelocity, pmtrace.plane.normal);

				VectorMASSE(vFinalVelocity, -fProj * 2, pmtrace.plane.normal, pParticle->velocity);
				VectorScale(pParticle->velocity, pSystem->impactdamp, pParticle->velocity);
				VectorScale(vFinalVelocity, pmtrace.fraction, vFinalVelocity);

				if (pParticle->rotationvel != 0.0f)
					pParticle->rotationvel *= -fProj * 2 * pSystem->impactdamp * m_flFrameTime;

				if (pParticle->rotxvel != 0.0f)
					pParticle->rotxvel *= -fProj * 2 * pSystem->impactdamp * m_flFrameTime;

				if (pParticle->rotyvel != 0.0f)
					pParticle->rotyvel *= -fProj * 2 * pSystem->impactdamp * m_flFrameTime;
			}
			else if (pSystem->collision == PARTICLE_COLLISION_DECAL)
			{
				gBSPRenderer.CreateDecal(pmtrace.endpos, pmtrace.plane.normal, pSystem->create);
				return false;
			}
			else if (pSystem->collision == PARTICLE_COLLISION_NEW_SYSTEM)
			{
				if (bColWater && pSystem->watercreate[0] != 0)
				{
					for (int i = 0; i < pSystem->watersystem->startparticles; i++)
						CreateParticle(pSystem->watersystem, pmtrace.endpos, pmtrace.plane.normal);
				}
				if (pSystem->deathcreate[0] != 0)
				{
					// gEngfuncs.Con_Printf("CALLED!\n");
					CreateSystem(pSystem->deathcreate, pParticle->origin, pParticle->velocity.Normalize(), 0);
				}
				if (gEngfuncs.PM_PointContents(pmtrace.endpos, nullptr) != CONTENTS_SKY && pSystem->create[0] != 0)
				{
					for (int i = 0; i < pSystem->createsystem->startparticles; i++)
						CreateParticle(pSystem->createsystem, pmtrace.endpos, pmtrace.plane.normal);
				}
				return false;
			}
			else
			{
				// kill it
				return false;
			}
		}
		else
		{
			VectorCopy(vFinalVelocity, pParticle->velocity);
		}
	}
	else
	{
		VectorCopy(vFinalVelocity, pParticle->velocity);
	}

	//
	// Add in the final velocity
	//
	VectorMASSE(pParticle->origin, m_flFrameTime, vFinalVelocity, pParticle->origin);

	//
	// Always reset to 1.0
	//
	pParticle->alpha = 1.0;

	//
	// Fading in
	//
	if (pSystem->fadeintime != 0.0f)
	{
		if ((pParticle->spawntime + pSystem->fadeintime) > flTime)
		{
			float flFadeTime = pParticle->spawntime + pSystem->fadeintime;
			float flTimeToFade = flFadeTime - flTime;

			pParticle->alpha = 1.0 - (flTimeToFade / pSystem->fadeintime);
		}
	}

	//
	// Fade out
	//
	if (pParticle->fadeoutdelay != 0.0f)
	{
		if ((pParticle->fadeoutdelay + pParticle->spawntime) < flTime)
		{
			float flTimeToDeath = pParticle->life - flTime;
			float flFadeTime = pParticle->fadeoutdelay + pParticle->spawntime;
			float flFadeFrac = pParticle->life - flFadeTime;

			pParticle->alpha = flTimeToDeath / flFadeFrac;
		}
	}

	//
	// Minimum and maximum distance fading
	//
	if ((pSystem->fadedistfar != 0) && (pSystem->fadedistnear != 0))
	{
		float flDist = (pParticle->origin - gBSPRenderer.m_vRenderOrigin).Length();
		float flAlpha = 1.0 - ((pSystem->fadedistfar - flDist) / (pSystem->fadedistfar - pSystem->fadedistnear));

		if (flAlpha < 0)
			flAlpha = 0;
		if (flAlpha > 1)
			flAlpha = 1;

		pParticle->alpha *= flAlpha;
	}

	//
	// Dampen scale
	//
	if ((pSystem->scaledampfactor != 0.0f) && (pParticle->scaledampdelay < flTime))
		pParticle->scale = pParticle->scale - m_flFrameTime * pSystem->scaledampfactor;

	if (pParticle->scale <= 0)
		return false;

	//
	// Check if lighting is required
	//
	if (pSystem->lightcheck != PARTICLE_LIGHTCHECK_NONE)
	{
		if (pSystem->lightcheck == PARTICLE_LIGHTCHECK_NORMAL)
		{
			pParticle->color = LightForParticle(pParticle);
		}
		else if (pSystem->lightcheck == PARTICLE_LIGHTCHECK_SCOLOR)
		{
			pParticle->scolor = LightForParticle(pParticle);
		}
		else if (pSystem->lightcheck == PARTICLE_LIGHTCHECK_MIXP)
		{
			pParticle->color = LightForParticle(pParticle);
			pParticle->color.x = pParticle->color.x * pSystem->primarycolor.x;
			pParticle->color.y = pParticle->color.y * pSystem->primarycolor.y;
			pParticle->color.z = pParticle->color.z * pSystem->primarycolor.z;
		}
	}

	//
	// See if we need to blend colors
	//
	if (pSystem->lightcheck != PARTICLE_LIGHTCHECK_NORMAL)
	{
		if ((pParticle->secondarydelay < flTime) && (flTime < (pParticle->secondarydelay + pParticle->secondarytime)))
		{
			float flTimeFull = (pParticle->secondarydelay + pParticle->secondarytime) - flTime;
			float flColFrac = flTimeFull / pParticle->secondarytime;

			pParticle->color[0] = pParticle->scolor[0] * (1.0 - flColFrac) + pSystem->primarycolor[0] * flColFrac;
			pParticle->color[1] = pParticle->scolor[1] * (1.0 - flColFrac) + pSystem->primarycolor[1] * flColFrac;
			pParticle->color[2] = pParticle->scolor[2] * (1.0 - flColFrac) + pSystem->primarycolor[2] * flColFrac;
		}
	}

	//
	// Spawn tracer particles
	//
	if (pSystem->tracerdist != 0.0f)
	{
		Vector vDistance;
		VectorSubtract(pParticle->origin, pParticle->lastspawn, vDistance);

		if (vDistance.Length() > pSystem->tracerdist)
		{
			Vector vDirection = pParticle->origin - pParticle->lastspawn;
			int iNumTraces = vDistance.Length() / pSystem->tracerdist;

			for (int i = 0; i < iNumTraces; i++)
			{
				float flFraction = (i + 1) / (float)iNumTraces;
				Vector vOrigin = pParticle->lastspawn + vDirection * flFraction;
				CreateParticle(pSystem->createsystem, vOrigin, pParticle->velocity.Normalize());
			}

			VectorCopy(pParticle->origin, pParticle->lastspawn);
		}
	}

	//
	// Calculate texcoords
	//
	if (pSystem->numframes != 0)
	{
		// Get desired frame
		int iFrame = ((int)((flTime - pParticle->spawntime) * pSystem->framerate));
		iFrame = iFrame % pSystem->numframes;

		// Check if we actually have to set the frame
		if (iFrame != pParticle->frame)
		{
			cl_texture_t* pTexture = pSystem->texture;

			int iNumFramesX = pTexture->iWidth / pSystem->framesizex;
			int iNumFramesY = pTexture->iHeight / pSystem->framesizey;

			int iColumn = iFrame % iNumFramesX;
			int iRow = (iFrame / iNumFramesX) % iNumFramesY;

			// Calculate these only once
			float flFractionWidth = (float)pSystem->framesizex / (float)pTexture->iWidth;
			float flFractionHeight = (float)pSystem->framesizey / (float)pTexture->iHeight;

			// Calculate top left coordinate
			pParticle->texcoords[0][0] = (iColumn + 1) * flFractionWidth;
			pParticle->texcoords[0][1] = iRow * flFractionHeight;

			// Calculate top right coordinate
			pParticle->texcoords[1][0] = iColumn * flFractionWidth;
			pParticle->texcoords[1][1] = iRow * flFractionHeight;

			// Calculate bottom right coordinate
			pParticle->texcoords[2][0] = iColumn * flFractionWidth;
			pParticle->texcoords[2][1] = (iRow + 1) * flFractionHeight;

			// Calculate bottom left coordinate
			pParticle->texcoords[3][0] = (iColumn + 1) * flFractionWidth;
			pParticle->texcoords[3][1] = (iRow + 1) * flFractionHeight;

			// Fill in current frame
			pParticle->frame = iFrame;
		}
	}

	// All went well, particle is still active
	return true;
}

/*
====================
RenderParticle

====================
*/
void CParticleEngine::RenderParticle(cl_particle_t* pParticle, float flUp, float flRight)
{
	float flDot;
	Vector vTemp;
	Vector vPoint;
	Vector vDir;
	Vector vAngles;

	if (pParticle->alpha == 0)
		return;

	/*
	if (pParticle->pSystem->shapetype == SYSTEM_SHAPE_BOX_AROUND_PLAYER)
	{
		pParticle->velocity[0] = gHUD.pparams->simvel[0];
		pParticle->velocity[1] = gHUD.pparams->simvel[1];
	}
	*/

	VectorSubtract(pParticle->origin, gBSPRenderer.m_vRenderOrigin, vDir);
	if (gHUD.m_pFogSettings.active)
	{
		if (vDir.Length() > gHUD.m_pFogSettings.end)
			return;
	}

	VectorNormalizeFast(vDir);
	DotProductSSE(&flDot, vDir, m_vForward);

	// z clipped
	if (flDot < 0)
		return;

	cl_texture_t* pTexture = pParticle->pSystem->texture;
	if (pParticle->pSystem->rendermode == SYSTEM_RENDERMODE_ADDITIVE)
	{
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		glColor4f(pParticle->color[0], pParticle->color[1], pParticle->color[2], pParticle->alpha * pParticle->pSystem->mainalpha);
		glFogfv(GL_FOG_COLOR, g_vecZero);
	}
	else if (pParticle->pSystem->rendermode == SYSTEM_RENDERMODE_ALPHABLEND)
	{
		glBlendFunc(GL_ONE, GL_ONE);
		glColor3f(pParticle->alpha * pParticle->pSystem->mainalpha, pParticle->alpha * pParticle->pSystem->mainalpha, pParticle->alpha * pParticle->pSystem->mainalpha);
		glFogfv(GL_FOG_COLOR, g_vecZero);
	}
	else
	{
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glColor4f(pParticle->color[0], pParticle->color[1], pParticle->color[2], pParticle->alpha * pParticle->pSystem->mainalpha);
		glFogfv(GL_FOG_COLOR, gHUD.m_pFogSettings.color);
	}

	if (pParticle->pSystem->displaytype == SYSTEM_DISPLAY_PLANAR)
	{
		gBSPRenderer.GetUpRight(pParticle->normal, m_vRUp, m_vRRight);
	}
	else if ((pParticle->rotation != 0.0f) || (pParticle->rotx != 0.0f) || (pParticle->roty != 0.0f))
	{
		VectorCopy(gBSPRenderer.m_vViewAngles, vAngles);

		if (pParticle->rotx != 0.0f)
			vAngles[0] = pParticle->rotx;
		if (pParticle->roty != 0.0f)
			vAngles[1] = pParticle->roty;
		if (pParticle->rotation != 0.0f)
			vAngles[2] = pParticle->rotation;

		AngleVectors(vAngles, nullptr, m_vRRight, m_vRUp);
	}

	if (pParticle->pSystem->displaytype == SYSTEM_DISPLAY_PARALELL)
	{
		glBegin(GL_TRIANGLE_FAN);
		vPoint = pParticle->origin + m_vRUp * flUp * pParticle->scale * 2;
		vPoint = vPoint + m_vRRight * flRight * (-pParticle->scale);
		glTexCoord2f(pParticle->texcoords[0][0], pParticle->texcoords[0][1]);
		glVertex3fv(vPoint);

		vPoint = pParticle->origin + m_vRUp * flUp * pParticle->scale * 2;
		vPoint = vPoint + m_vRRight * flRight * pParticle->scale;
		glTexCoord2f(pParticle->texcoords[1][0], pParticle->texcoords[1][1]);
		glVertex3fv(vPoint);

		vPoint = pParticle->origin + m_vRRight * flRight * pParticle->scale;
		glTexCoord2f(pParticle->texcoords[2][0], pParticle->texcoords[2][1]);
		glVertex3fv(vPoint);

		vPoint = pParticle->origin + m_vRRight * flRight * (-pParticle->scale);
		glTexCoord2f(pParticle->texcoords[3][0], pParticle->texcoords[3][1]);
		glVertex3fv(vPoint);
		glEnd();
	}
	else
	{
		glBegin(GL_TRIANGLE_FAN);
		vPoint = pParticle->origin + m_vRUp * flUp * pParticle->scale;
		vPoint = vPoint + m_vRRight * flRight * (-pParticle->scale);
		glTexCoord2f(pParticle->texcoords[0][0], pParticle->texcoords[0][1]);
		glVertex3fv(vPoint);

		vPoint = pParticle->origin + m_vRUp * flUp * pParticle->scale;
		vPoint = vPoint + m_vRRight * flRight * pParticle->scale;
		glTexCoord2f(pParticle->texcoords[1][0], pParticle->texcoords[1][1]);
		glVertex3fv(vPoint);

		vPoint = pParticle->origin + m_vRUp * flUp * (-pParticle->scale);
		vPoint = vPoint + m_vRRight * flRight * pParticle->scale;
		glTexCoord2f(pParticle->texcoords[2][0], pParticle->texcoords[2][1]);
		glVertex3fv(vPoint);

		vPoint = pParticle->origin + m_vRUp * flUp * (-pParticle->scale);
		vPoint = vPoint + m_vRRight * flRight * (-pParticle->scale);
		glTexCoord2f(pParticle->texcoords[3][0], pParticle->texcoords[3][1]);
		glVertex3fv(vPoint);
		glEnd();
	}

	if (gBSPRenderer.m_pCvarWireFrame->value > 0)
	{
		glDisable(GL_BLEND);
		glDepthMask(GL_TRUE);
		glDisable(GL_TEXTURE_2D);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glColor3f(1.0, 0.0, 0.0);
		glLineWidth(1);

		if (gBSPRenderer.m_pCvarWireFrame->value > 1)
			glDisable(GL_DEPTH_TEST);

		if (pParticle->pSystem->displaytype == SYSTEM_DISPLAY_PARALELL)
		{
			glBegin(GL_TRIANGLE_FAN);
			vPoint = pParticle->origin + m_vRUp * flUp * pParticle->scale * 2;
			vPoint = vPoint + m_vRRight * flRight * (-pParticle->scale);
			glVertex3fv(vPoint);

			vPoint = pParticle->origin + m_vRUp * flUp * pParticle->scale * 2;
			vPoint = vPoint + m_vRRight * flRight * pParticle->scale;
			glVertex3fv(vPoint);

			vPoint = pParticle->origin + m_vRRight * flRight * pParticle->scale;
			glVertex3fv(vPoint);

			vPoint = pParticle->origin + m_vRRight * flRight * (-pParticle->scale);
			glVertex3fv(vPoint);
			glEnd();
		}
		else
		{
			glBegin(GL_TRIANGLE_FAN);
			vPoint = pParticle->origin + m_vRUp * flUp * pParticle->scale;
			vPoint = vPoint + m_vRRight * flRight * (-pParticle->scale);
			glVertex3fv(vPoint);

			vPoint = pParticle->origin + m_vRUp * flUp * pParticle->scale;
			vPoint = vPoint + m_vRRight * flRight * pParticle->scale;
			glVertex3fv(vPoint);

			vPoint = pParticle->origin + m_vRUp * flUp * (-pParticle->scale);
			vPoint = vPoint + m_vRRight * flRight * pParticle->scale;
			glVertex3fv(vPoint);

			vPoint = pParticle->origin + m_vRUp * flUp * (-pParticle->scale);
			vPoint = vPoint + m_vRRight * flRight * (-pParticle->scale);
			glVertex3fv(vPoint);
			glEnd();
		}

		if (gBSPRenderer.m_pCvarWireFrame->value > 1)
			glEnable(GL_DEPTH_TEST);

		glEnable(GL_BLEND);
		glDepthMask(GL_FALSE);
		glEnable(GL_TEXTURE_2D);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}

	m_iNumParticles++;
}

/*
====================
DrawParticles

====================
*/
void CParticleEngine::DrawParticles()
{
	if (m_pCvarDrawParticles->value <= 0)
		return;

	AngleVectors(gBSPRenderer.m_vViewAngles, m_vForward, m_vRight, m_vUp);

	RenderFog();
	gBSPRenderer.SetTexEnvs(ENVSTATE_REPLACE, ENVSTATE_OFF, ENVSTATE_OFF, ENVSTATE_OFF);

	gBSPRenderer.glActiveTextureARB(GL_TEXTURE0_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PRIMARY_COLOR_ARB);

	glEnable(GL_BLEND);
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);

	float flUp;
	float flRight;

	particle_system_t* psystem = m_pSystemHeader;
	while (psystem != nullptr)
	{
		// Check if there's any particles at all
		if (psystem->particleheader == nullptr)
		{
			particle_system_t* pnext = psystem->next;
			psystem = pnext;
			continue;
		}

		// Check if it's in VIS
		if (psystem->leaf != nullptr)
		{
			if (psystem->leaf->visframe != gBSPRenderer.m_iVisFrame)
			{
				particle_system_t* pnext = psystem->next;
				psystem = pnext;
				continue;
			}
		}

		// Calculate up and right scalers
		if (psystem->numframes != 0)
		{
			if (psystem->framesizex > psystem->framesizey)
			{
				flUp = (float)psystem->framesizey / (float)psystem->framesizex;
				flRight = 1.0;
			}
			else
			{
				flRight = (float)psystem->framesizex / (float)psystem->framesizey;
				flUp = 1.0;
			}
		}
		else
		{
			if (psystem->texture->iWidth > psystem->texture->iHeight)
			{
				flUp = (float)psystem->texture->iHeight / (float)psystem->texture->iWidth;
				flRight = 1.0;
			}
			else
			{
				flRight = (float)psystem->texture->iWidth / (float)psystem->texture->iHeight;
				flUp = 1.0;
			}
		}

		if (psystem->displaytype == SYSTEM_DISPLAY_PARALELL)
		{
			VectorCopy(m_vRight, m_vRRight);
			VectorClear(m_vRUp);
			m_vRUp[2] = 1;
		}
		else if ((psystem->rotationvel == 0.0f) && (psystem->rotxvel == 0.0f) && (psystem->rotyvel == 0.0f))
		{
			VectorCopy(m_vRight, m_vRRight);
			VectorCopy(m_vUp, m_vRUp);
		}

		if (psystem->overbright != 0)
			glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 2);

		// Bind texture
		gBSPRenderer.Bind2DTexture(GL_TEXTURE0_ARB, psystem->texture->iIndex);

		// Render all particles tied to this system
		cl_particle_t* pparticle = psystem->particleheader;
		while (pparticle != nullptr)
		{
			RenderParticle(pparticle, flUp, flRight);
			cl_particle_t* pnext = pparticle->next;
			pparticle = pnext;
		}

		// Reset
		if (psystem->overbright != 0)
			glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1);

		particle_system_t* pnext = psystem->next;
		psystem = pnext;
	}

	glFogfv(GL_FOG_COLOR, gHUD.m_pFogSettings.color);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	glEnable(GL_CULL_FACE);
	glColor4f(GL_ONE, GL_ONE, GL_ONE, GL_ONE);
}

/*
====================
RemoveSystem

====================
*/
void CParticleEngine::RemoveSystem(int iId)
{
	if (m_pSystemHeader == nullptr)
		return;

	if (iId == 0)
		return;

	particle_system_t* psystem = m_pSystemHeader;
	while (psystem != nullptr)
	{
		if (psystem->id != iId)
		{
			particle_system_t* pnext = psystem->next;
			psystem = pnext;
			continue;
		}

		// Remove all related particles
		cl_particle_t* pparticle = psystem->particleheader;
		while (pparticle != nullptr)
		{
			cl_particle_t* pfree = pparticle;
			pparticle = pfree->next;

			m_iNumFreedParticles++;
			delete[] pfree;
		}

		// Unlink this
		if (psystem->createsystem != nullptr)
			psystem->createsystem->parentsystem = nullptr;

		// Unlink this
		if (psystem->watersystem != nullptr)
			psystem->watersystem->parentsystem = nullptr;

		if (psystem->prev == nullptr)
		{
			m_pSystemHeader = psystem->next;
			if (psystem->next != nullptr)
				psystem->next->prev = nullptr;
		}
		else
		{
			psystem->prev->next = psystem->next;
			if (psystem->next != nullptr)
				psystem->next->prev = psystem->prev;
		}

		m_iNumFreedSystems++;
		delete[] psystem;
		break;
	}
}

/*
====================
MsgCreateSystem

====================
*/
int CParticleEngine::MsgCreateSystem(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	Vector pos;
	pos.x = READ_COORD();
	pos.y = READ_COORD();
	pos.z = READ_COORD();

	Vector ang;
	ang.x = READ_COORD();
	ang.y = READ_COORD();
	ang.z = READ_COORD();

	int iType = READ_BYTE();
	char* szPath = READ_STRING();
	int iId = READ_SHORT();

	if (iType == 2)
		RemoveSystem(iId);
	else if (iType == 1)
		CreateCluster(szPath, pos, ang, iId);
	else
		CreateSystem(szPath, pos, ang, iId);

	return 1;
}
