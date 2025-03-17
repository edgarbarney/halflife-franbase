/*
Trinity Rendering Engine - Copyright Andrew Lucas 2009-2012

The Trinity Engine is free software, distributed in the hope th-
at it will be useful, but WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU Lesser General Public License for more det-
ails.

Particle Engine
Written by Andrew Lucas
*/

#if !defined(PARTICLE_ENGINE_H)
#define PARTICLE_ENGINE_H
#if defined(_WIN32)
#pragma once
#include "windows.h"
#endif

#include "GL/gl.h"
#include "pm_defs.h"
#include "cl_entity.h"
#include "ref_params.h"
#include "dlight.h"
#include "parsemsg.h"
#include "cvardef.h"
#include "textureloader.h"
#include "rendererdefs.h"

#include "FranUtils/FranUtils_FileSystem.hpp"

/*
====================
CParticleEngine

====================
*/
class CParticleEngine
{
public:
	void Init();
	void VidInit();
	void Shutdown();

	void CreateCluster(const std::string& szPath, Vector origin, Vector dir, int iId);
	particle_system_t* CreateSystem(const std::string& path, Vector origin, Vector dir, int iId, particle_system_t* parent = nullptr);
	void RemoveSystem(int iId);

	particle_system_t* AllocSystem();
	cl_particle_t* AllocParticle(particle_system_t* pSystem);

	void Update();
	void DrawParticles();
	void CullSystems();
	void UpdateSystems();

	Vector LightForParticle(cl_particle_t* pParticle);
	bool CheckLightBBox(cl_particle_t* pParticle, cl_dlight_t* pLight);

	void EnvironmentCreateFirst(particle_system_t* pSystem);

	void CreateParticle(particle_system_t* pSystem, float* flOrigin = nullptr, float* flNormal = nullptr);
	bool UpdateParticle(cl_particle_t* pParticle);
	void RenderParticle(cl_particle_t* pParticle, float flUp, float flRight);

	int MsgCreateSystem(const char* pszName, int iSize, void* pbuf);

public:
	particle_system_t* m_pSystemHeader;

	// Caches for parsed particle data
	// Structure is filename -> key-value pairs
	std::unordered_map<std::string, FranUtils::FileSystem::StringMap> m_particleDataCache;
	std::unordered_map<std::string, FranUtils::FileSystem::StringMap> m_particleClusterDataCache;

	cvar_t* m_pCvarDrawParticles;
	cvar_t* m_pCvarParticleDebug;
	cvar_t* m_pCvarParticleMaxPart;
	cvar_t* m_pCvarGravity;

	float m_flLastDraw;
	float m_flFrameTime;

	int m_iNumParticles;

	int m_iNumFreedParticles;
	int m_iNumCreatedParticles;

	int m_iNumFreedSystems;
	int m_iNumCreatedSystems;

	Vector m_vForward;
	Vector m_vRight;
	Vector m_vUp;

	Vector m_vRRight;
	Vector m_vRUp;
};
extern CParticleEngine gParticleEngine;
#endif
