/*
Trinity Rendering Engine - Copyright Andrew Lucas 2009-2012

The Trinity Engine is free software, distributed in the hope th-
at it will be useful, but WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU Lesser General Public License for more det-
ails.

Client side entity manager
Written by Andrew Lucas
Transparency code by Neil "Jed" Jedrzejewski
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
#include "bsprenderer.h"

#include "r_efx.h"
#include "r_studioint.h"
#include "studio_util.h"

#include "textureloader.h"
#include "particle_engine.h"

#include "StudioModelRenderer.h"
#include "GameStudioModelRenderer.h"
extern CGameStudioModelRenderer g_StudioRenderer;

/*
====================
Shutdown

====================
*/
void CPropManager::Shutdown()
{
	Reset();
}

/*
====================
Reset

====================
*/
void CPropManager::Reset()
{
	if (m_iNumEntities != 0)
	{
		memset(m_pEntities, 0, sizeof(m_pEntities));
		m_iNumEntities = 0;
	}

	if (m_iNumModelLights != 0)
	{
		memset(m_pModelLights, 0, sizeof(m_pModelLights));
		m_iNumModelLights = 0;
	}

	if (m_iNumDecals != 0)
	{
		memset(m_pDecals, 0, sizeof(m_pDecals));
		m_iNumDecals = 0;
	}

	if (m_iNumExtraData != 0)
	{
		memset(m_pExtraData, 0, sizeof(m_pExtraData));
		memset(m_pExtraInfo, 0, sizeof(m_pExtraInfo));
		m_pCurrentExtraData = nullptr;
		m_iNumExtraData = 0;
	}

	if (m_iNumHeaders != 0)
	{
		for (int i = 0; i < m_iNumHeaders; i++)
		{
			if (m_pHeaders[i].pHdr != nullptr)
			{
				for (int j = 0; j < m_pHeaders[i].pVBOHeader.numsubmodels; j++)
					delete[] m_pHeaders[i].pVBOHeader.submodels[j].meshes;

				delete[] m_pHeaders[i].pVBOHeader.submodels;
			}

			if (m_pHeaders[i].pVBOHeader.pBufferData != nullptr)
			{
				delete[] m_pHeaders[i].pVBOHeader.pBufferData;
				m_pHeaders[i].pVBOHeader.pBufferData = nullptr;
			}

			if (m_pHeaders[i].pVBOHeader.indexes != nullptr)
			{
				delete[] m_pHeaders[i].pVBOHeader.indexes;
				m_pHeaders[i].pVBOHeader.indexes = nullptr;
			}
		}

		memset(m_pHeaders, 0, sizeof(m_pHeaders));
		m_iNumHeaders = NULL;
	}

	ClearEntityData();

	if (m_pEntData != nullptr)
	{
		m_pEntData = nullptr;
		m_iEntDataSize = NULL;
	}

	if (m_pVertexData != nullptr)
	{
		delete[] m_pVertexData;
		m_pVertexData = nullptr;
		m_iNumTotalVerts = NULL;
	}

	if (m_pIndexBuffer != nullptr)
	{
		delete[] m_pIndexBuffer;
		m_pIndexBuffer = nullptr;
	}

	if (m_iNumCables != 0)
	{
		memset(m_pCables, 0, sizeof(m_pCables));
		m_iNumCables = NULL;
	}
}

/*
====================
Init

====================
*/
void CPropManager::Init()
{
	m_pCvarDrawClientEntities = CVAR_CREATE("te_client_entities", "1", 0);
}

/*
====================
VidInit

====================
*/
void CPropManager::VidInit()
{
	Reset();
}

/*
====================
ClearEntityData

====================
*/
void CPropManager::ClearEntityData()
{
	if (m_iNumBSPEntities == 0)
		return;

	for (int i = 0; i < m_iNumBSPEntities; i++)
	{
		epair_t* pPair = m_pBSPEntities[i].epairs;
		while (pPair != nullptr)
		{
			epair_t* pFree = pPair;
			pPair = pFree->next;

			delete[] pFree->key;
			delete[] pFree->value;
			delete[] pFree;
		}
	}
	memset(m_pBSPEntities, 0, sizeof(m_pBSPEntities));
	m_iNumBSPEntities = 0;
}

/*
====================
LoadBSPFile

====================
*/
void CPropManager::GenerateEntityList()
{
	// reset all entity data
	Reset();

	// get pointer to world model
	model_t* pWorld = IEngineStudio.GetModelByIndex(1);
	if (pWorld == nullptr)
	{
		gEngfuncs.pfnClientCmd("escape\n");
		MessageBox(nullptr, "FATAL ERROR: Failed to get world!\n\nPress Ok to quit the game.\n", "ERROR", MB_OK);
		exit(-1);
	}

	// world model already adds pointer to ent data
	m_iEntDataSize = strlen(pWorld->entities);
	m_pEntData = pWorld->entities;

	ParseEntities();
	LoadEntVars();
	SetupVBO();
}

/*
====================
GetHeader

====================
*/
modeldata_t* CPropManager::GetHeader(const char* name)
{
	if (m_iNumHeaders != 0)
	{
		for (int i = 0; i < m_iNumHeaders; i++)
		{
			if (strcmp(m_pHeaders[i].name, name) == 0)
				return &m_pHeaders[i];
		}
	}
	return nullptr;
}

/*
====================
ValueForKey

====================
*/
char* CPropManager::ValueForKey(entity_t* ent, const char* key)
{
	for (epair_t* pEPair = ent->epairs; pEPair != nullptr; pEPair = pEPair->next)
	{
		if (strcmp(pEPair->key, key) == 0)
			return pEPair->value;
	}
	return nullptr;
}

/*
====================
ParseEntities

====================
*/
void CPropManager::ParseEntities()
{
	// Entity parser done by me, parses nicely, no errors detected ever.
	char* pCurText = m_pEntData;
	while ((pCurText != nullptr) && pCurText - m_pEntData < m_iEntDataSize)
	{
		if (m_iNumBSPEntities == MAXRENDERENTS)
			break;

		while (true)
		{
			if (pCurText[0] == '{')
				break;

			if (pCurText - m_pEntData >= m_iEntDataSize)
				break;

			pCurText++;
		}

		if (pCurText - m_pEntData >= m_iEntDataSize)
			break;

		entity_t* pEntity = &m_pBSPEntities[m_iNumBSPEntities];
		m_iNumBSPEntities++;

		while (true)
		{
			// skip to next token
			while (true)
			{
				if (pCurText[0] == '}')
					break;

				if (pCurText[0] == '"')
				{
					pCurText++;
					break;
				}

				pCurText++;
			}

			// end of ent
			if (pCurText[0] == '}')
				break;

			epair_t* pEPair = new epair_t;
			memset(pEPair, 0, sizeof(epair_t));

			if (pEntity->epairs != nullptr)
				pEPair->next = pEntity->epairs;

			pEntity->epairs = pEPair;

			int iLength = 0;
			char* pTemp = pCurText;
			while (true)
			{
				if (pTemp[0] == '"')
					break;

				if (pCurText[0] == '}')
				{
					gEngfuncs.Con_Printf("BSP LOADER ERROR :: Entity data is corrupt!\n");
					m_bAvailable = false;
					return;
				}

				iLength++;
				pTemp++;
			}

			pEPair->key = new char[iLength + 1];
			pEPair->key[iLength] = NULL; // terminator

			memcpy(pEPair->key, pCurText, sizeof(char) * iLength);
			pCurText += iLength + 1;

			// skip to next token
			while (true)
			{
				if (pCurText[0] == '}')
				{
					gEngfuncs.Con_Printf("BSP LOADER ERROR :: Entity data is corrupt!\n");
					m_bAvailable = false;
					return;
				}

				if (pCurText[0] == '"')
				{
					pCurText++;
					break;
				}

				pCurText++;
			}

			iLength = 0;
			pTemp = pCurText;
			while (true)
			{
				if (pCurText[0] == '}')
				{
					gEngfuncs.Con_Printf("BSP LOADER ERROR :: Entity data is corrupt!\n");
					m_bAvailable = false;
					return;
				}

				if (pTemp[0] == '"')
					break;

				iLength++;
				pTemp++;
			}

			pEPair->value = new char[iLength + 1];
			pEPair->value[iLength] = NULL;

			memcpy(pEPair->value, pCurText, sizeof(char) * iLength);
			pCurText += iLength + 1;
		}
	}

	// Get sky name for bsp renderer
	char* szSky = ValueForKey(&m_pBSPEntities[0], "skyname");

	if (szSky != nullptr)
		strcpy(gBSPRenderer.m_szSkyName, szSky);
	else
		sprintf(gBSPRenderer.m_szSkyName, "desert");

	// See if special fog is set
	char* szSpecial = ValueForKey(&m_pBSPEntities[0], "specialfog");

	if (szSpecial != nullptr)
		gBSPRenderer.m_bSpecialFog = true;
}

/*
====================
LoadEntVars

====================
*/
void CPropManager::LoadEntVars()
{
	for (int i = 0; i < m_iNumBSPEntities; i++)
	{
		char* pValue = ValueForKey(&m_pBSPEntities[i], "classname");

		if (pValue == nullptr)
			continue;

		if (strcmp(pValue, "env_elight") == 0)
		{
			pValue = ValueForKey(&m_pBSPEntities[i], "targetname");

			if (pValue != nullptr)
				continue;

			memset(&m_pModelLights[m_iNumModelLights], 0, sizeof(cl_entity_t));

			pValue = ValueForKey(&m_pBSPEntities[i], "origin");
			if (pValue != nullptr)
			{
				sscanf(pValue, "%f %f %f", &m_pModelLights[m_iNumModelLights].origin[0],
					&m_pModelLights[m_iNumModelLights].origin[1],
					&m_pModelLights[m_iNumModelLights].origin[2]);

				VectorCopy(m_pModelLights[m_iNumModelLights].origin, m_pModelLights[m_iNumModelLights].curstate.origin);
			}

			pValue = ValueForKey(&m_pBSPEntities[i], "renderamt");
			if (pValue != nullptr)
			{
				sscanf(pValue, "%d", &m_pModelLights[m_iNumModelLights].curstate.renderamt);
			}

			pValue = ValueForKey(&m_pBSPEntities[i], "rendercolor");
			if (pValue != nullptr)
			{
				int iColR, iColG, iColB;
				sscanf(pValue, "%d %d %d", &iColR, &iColG, &iColB);
				m_pModelLights[m_iNumModelLights].curstate.rendercolor.r = iColR;
				m_pModelLights[m_iNumModelLights].curstate.rendercolor.g = iColG;
				m_pModelLights[m_iNumModelLights].curstate.rendercolor.b = iColB;
			}

			model_t* pWorld = IEngineStudio.GetModelByIndex(1);
			mleaf_t* pLeaf = Mod_PointInLeaf(m_pModelLights[m_iNumModelLights].origin, pWorld);

			if (pLeaf != nullptr)
			{
				// In-void entities can go eat a dick
				m_pModelLights[m_iNumModelLights].visframe = pLeaf - pWorld->leafs - 1;
				m_iNumModelLights++;
			}
		}
		if (strcmp(pValue, "env_cable") == 0)
		{
			if (SetupCable(&m_pCables[m_iNumCables], &m_pBSPEntities[i]))
				m_iNumCables++;
		}
		else if (strcmp(pValue, "env_decal") == 0)
		{
			pValue = ValueForKey(&m_pBSPEntities[i], "targetname");

			if (pValue != nullptr)
				continue;

			// Always TRUE
			m_pDecals[m_iNumDecals].persistent = TRUE;

			pValue = ValueForKey(&m_pBSPEntities[i], "origin");
			if (pValue != nullptr)
			{
				sscanf(pValue, "%f %f %f", &m_pDecals[m_iNumDecals].pos[0],
					&m_pDecals[m_iNumDecals].pos[1],
					&m_pDecals[m_iNumDecals].pos[2]);
			}

			pValue = ValueForKey(&m_pBSPEntities[i], "message");

			if (pValue == nullptr)
				continue;

			if (strlen(pValue) == 0u)
				continue;

			strcpy(m_pDecals[m_iNumDecals].name, pValue);
			m_iNumDecals++;
		}
		else if (strcmp(pValue, "item_generic") == 0)
		{
			pValue = ValueForKey(&m_pBSPEntities[i], "targetname");

			if (pValue != nullptr)
				continue;

			pValue = ValueForKey(&m_pBSPEntities[i], "model");

			if (pValue == nullptr)
				continue;

			if (stristr(pValue, ".mdl") == nullptr)
				continue;

			m_pCurrentExtraData = &m_pExtraData[m_iNumExtraData];
			entextrainfo_t* pExtraInfo = &m_pExtraInfo[m_iNumExtraData];

			if (!LoadMDL(pValue, &m_pEntities[m_iNumEntities], &m_pBSPEntities[i]))
			{
				gEngfuncs.Con_Printf("BSP Loader: Failed to model load %s on the client!\n", pValue);
				continue;
			}

			memset(&m_pEntities[m_iNumEntities], 0, sizeof(cl_entity_t));
			m_pEntities[m_iNumEntities].index = m_iNumEntities + 4096;
			m_pEntities[m_iNumEntities].topnode = (struct mnode_s*)pExtraInfo;
			m_pEntities[m_iNumEntities].visframe = -1;

			pExtraInfo->pExtraData = m_pCurrentExtraData;
			m_iNumExtraData++;

			pValue = ValueForKey(&m_pBSPEntities[i], "origin");
			if (pValue != nullptr)
			{
				sscanf(pValue, "%f %f %f", &m_pEntities[m_iNumEntities].origin[0],
					&m_pEntities[m_iNumEntities].origin[1],
					&m_pEntities[m_iNumEntities].origin[2]);

				VectorCopy(m_pEntities[m_iNumEntities].origin, m_pEntities[m_iNumEntities].curstate.origin);
			}

			pValue = ValueForKey(&m_pBSPEntities[i], "angles");
			if (pValue != nullptr)
			{
				// set the yaw angle...
				sscanf(pValue, "%f %f %f", &m_pEntities[m_iNumEntities].angles[0],
					&m_pEntities[m_iNumEntities].angles[1],
					&m_pEntities[m_iNumEntities].angles[2]);
				m_pEntities[m_iNumEntities].curstate.angles = m_pEntities[m_iNumEntities].angles;
			}

			pValue = ValueForKey(&m_pBSPEntities[i], "renderamt");
			if (pValue != nullptr)
			{
				sscanf(pValue, "%d", &m_pEntities[m_iNumEntities].curstate.renderamt);
			}

			pValue = ValueForKey(&m_pBSPEntities[i], "sequence");

			if (pValue != nullptr)
				sscanf(pValue, "%d", &m_pEntities[m_iNumEntities].curstate.sequence);

			pValue = ValueForKey(&m_pBSPEntities[i], "body");

			if (pValue != nullptr)
				sscanf(pValue, "%d", &m_pEntities[m_iNumEntities].curstate.body);

			pValue = ValueForKey(&m_pBSPEntities[i], "skin");

			if (pValue != nullptr)
				sscanf(pValue, "%hd", &m_pEntities[m_iNumEntities].curstate.skin);


			pValue = ValueForKey(&m_pBSPEntities[i], "scale");

			if (pValue != nullptr)
				sscanf(pValue, "%f", &m_pEntities[m_iNumEntities].curstate.scale);


			pValue = ValueForKey(&m_pBSPEntities[i], "renderfx");

			if (pValue != nullptr)
				sscanf(pValue, "%d", &m_pEntities[m_iNumEntities].curstate.renderfx);

			pValue = ValueForKey(&m_pBSPEntities[i], "DisableShadows");

			if (pValue != nullptr)
				m_pEntities[m_iNumEntities].curstate.iuser2 = FL_NOSHADOW;

			pValue = ValueForKey(&m_pBSPEntities[i], "rendercolor");
			if (pValue != nullptr)
			{
				int iColR, iColG, iColB;
				sscanf(pValue, "%d %d %d", &iColR, &iColG, &iColB);
				m_pEntities[m_iNumEntities].curstate.rendercolor.r = iColR;
				m_pEntities[m_iNumEntities].curstate.rendercolor.g = iColG;
				m_pEntities[m_iNumEntities].curstate.rendercolor.b = iColB;
			}

			pValue = ValueForKey(&m_pBSPEntities[i], "lightorigin");
			if ((pValue != nullptr) && (strlen(pValue) != 0u))
			{
				char szLightTarget[32];
				strcpy(szLightTarget, pValue);

				int j = 0;
				for (; j < m_iNumBSPEntities; j++)
				{
					pValue = ValueForKey(&m_pBSPEntities[j], "classname");

					if (strcmp(pValue, "info_light_origin") != 0)
						continue;

					pValue = ValueForKey(&m_pBSPEntities[j], "targetname");

					if (pValue != nullptr)
					{
						if (strcmp(pValue, szLightTarget) == 0)
						{
							pValue = ValueForKey(&m_pBSPEntities[j], "origin");
							if (pValue != nullptr)
							{
								sscanf(pValue, "%f %f %f", &m_pCurrentExtraData->lightorigin[0],
									&m_pCurrentExtraData->lightorigin[1],
									&m_pCurrentExtraData->lightorigin[2]);

								break;
							}
						}
					}
				}

				if (j == m_iNumBSPEntities)
				{
					m_pCurrentExtraData->lightorigin = m_pEntities[m_iNumEntities].origin;
				}
			}
			else
			{
				m_pCurrentExtraData->lightorigin = m_pEntities[m_iNumEntities].origin;
			}

			g_StudioRenderer.m_bExternalEntity = true;
			g_StudioRenderer.m_pCurrentEntity = &m_pEntities[m_iNumEntities];
			g_StudioRenderer.m_pStudioHeader = m_pCurrentExtraData->pModelData->pHdr;
			g_StudioRenderer.m_pStudioHeader = m_pCurrentExtraData->pModelData->pTexHdr;

			g_StudioRenderer.StudioSaveUniqueData(m_pCurrentExtraData);
			m_iNumEntities++;
		}
	}
}

/*
====================
SetupVBO

====================
*/
void CPropManager::SetupVBO()
{
	if (m_iNumHeaders == 0)
		return;

	m_iNumTotalVerts = NULL;
	for (int i = 0; i < m_iNumHeaders; i++)
		m_iNumTotalVerts += m_pHeaders[i].pVBOHeader.numverts;

	int iTotalIndexes = 0;
	for (int i = 0; i < m_iNumHeaders; i++)
		iTotalIndexes += m_pHeaders[i].pVBOHeader.numindexes;

	m_pVertexData = new brushvertex_t[m_iNumTotalVerts];
	memset(m_pVertexData, 0, sizeof(brushvertex_t) * m_iNumTotalVerts);

	m_pIndexBuffer = new unsigned int[iTotalIndexes];
	memset(m_pIndexBuffer, 0, sizeof(unsigned int) * iTotalIndexes);

	int iVertexOffset = 0;
	int iIndexOffset = 0;
	for (int i = 0; i < m_iNumHeaders; i++)
	{
		memcpy(&m_pVertexData[iVertexOffset], m_pHeaders[i].pVBOHeader.pBufferData,
			sizeof(brushvertex_t) * m_pHeaders[i].pVBOHeader.numverts);

		for (int j = 0; j < m_pHeaders[i].pVBOHeader.numindexes; j++)
			m_pHeaders[i].pVBOHeader.indexes[j] += iVertexOffset;

		memcpy(&m_pIndexBuffer[iIndexOffset], m_pHeaders[i].pVBOHeader.indexes,
			sizeof(unsigned int) * m_pHeaders[i].pVBOHeader.numindexes);

		for (int j = 0; j < m_pHeaders[i].pVBOHeader.numsubmodels; j++)
		{
			for (int k = 0; k < m_pHeaders[i].pVBOHeader.submodels[j].nummeshes; k++)
			{
				m_pHeaders[i].pVBOHeader.submodels[j].meshes[k].start_vertex += iIndexOffset;
			}
		}

		iVertexOffset += m_pHeaders[i].pVBOHeader.numverts;
		iIndexOffset += m_pHeaders[i].pVBOHeader.numindexes;
	}

	gBSPRenderer.glGenBuffersARB(1, &m_uiIndexBuffer);
	gBSPRenderer.glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, m_uiIndexBuffer);
	gBSPRenderer.glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, iTotalIndexes * sizeof(unsigned int), m_pIndexBuffer, GL_STATIC_DRAW_ARB);
	gBSPRenderer.glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
}

/*
====================
RenderModels

====================
*/
void CPropManager::RenderProps()
{
	if (m_pCvarDrawClientEntities->value < 1)
		return;

	if (g_StudioRenderer.m_pCvarDrawModels->value < 1)
		return;

	if (g_StudioRenderer.m_pCvarDrawEntities->value < 1)
		return;

	if (m_pCvarDrawClientEntities->value == 2)
		glDisable(GL_DEPTH_TEST);

	gBSPRenderer.glClientActiveTextureARB(GL_TEXTURE0_ARB);
	gBSPRenderer.glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, m_uiIndexBuffer);
	glTexCoordPointer(2, GL_FLOAT, sizeof(brushvertex_t), OFFSET(brushvertex_t, texcoord));

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	for (int i = 0; i < m_iNumEntities; i++)
	{
		if (m_pEntities[i].curstate.renderfx == 70)
			continue;

		entextradata_t* pExtraData = ((entextrainfo_t*)m_pEntities[i].topnode)->pExtraData;

		if (pExtraData == nullptr)
			return;

		int j = 0;
		for (; j < pExtraData->num_leafs; j++)
			if ((gBSPRenderer.m_pPVS[pExtraData->leafnums[j] >> 3] & (1 << (pExtraData->leafnums[j] & 7))) != 0)
				break;

		if (j == pExtraData->num_leafs)
			continue;

		g_StudioRenderer.StudioDrawExternalEntity(&m_pEntities[i]);
	}

	gBSPRenderer.glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	if (m_pCvarDrawClientEntities->value == 2)
		glEnable(GL_DEPTH_TEST);
}

/*
====================
PostLoadModel

====================
*/
bool CPropManager::PostLoadModel(const char* modelname, studiohdr_t* hdr, cl_entity_t* pEntity)
{
	// preload textures
	if (hdr->numtextures == 0)
	{
		char texturename[256];

		strcpy(texturename, modelname);
		strcpy(&texturename[strlen(texturename) - 4], "T.mdl");

		model_t* pModel = g_StudioRenderer.Mod_LoadModel(texturename);

		if (pModel == nullptr)
			return false;

		m_pHeaders[m_iNumHeaders].pTexHdr = (studiohdr_t*)pModel->cache.data;
	}
	else
	{
		m_pHeaders[m_iNumHeaders].pTexHdr = m_pHeaders[m_iNumHeaders].pHdr;
	}
	return true;
}

/*
====================
LoadMDL

====================
*/
bool CPropManager::LoadMDL(const char* name, cl_entity_t* pEntity, entity_t* pBSPEntity)
{
	if (m_pCurrentExtraData->pModelData = GetHeader(name) != nullptr)
		return true;

	if (m_iNumHeaders == MAXRENDERENTS)
	{
		gEngfuncs.Con_Printf("BSP Loader: The client side limit of 4096 models has been reached!\n Not caching.\n");
		return false;
	}

	model_t* pModel = g_StudioRenderer.Mod_LoadModel(name);

	if (pModel == nullptr)
		return false;

	m_pHeaders[m_iNumHeaders].pHdr = (studiohdr_t*)pModel->cache.data;
	strcpy(m_pHeaders[m_iNumHeaders].name, name);

	if (m_iNumHeaders == MAXRENDERENTS)
	{
		gEngfuncs.Con_Printf("BSP Loader: The client side limit of 4096 models has been reached!\n Not caching.\n");
		return false;
	}

	if (!PostLoadModel(name, m_pHeaders[m_iNumHeaders].pHdr, pEntity))
		return false;

	// not very nice, but we don't support animations anyway
	cl_entity_t* pTempEnt = new cl_entity_t;
	memset(pTempEnt, 0, sizeof(cl_entity_t));
	model_t* pTempModel = new model_t;
	memset(pTempModel, 0, sizeof(model_t));
	strcpy(pTempModel->name, name);

	g_StudioRenderer.m_bExternalEntity = true;
	g_StudioRenderer.m_pCurrentEntity = pTempEnt;
	g_StudioRenderer.m_pStudioHeader = m_pHeaders[m_iNumHeaders].pHdr;
	g_StudioRenderer.m_pTextureHeader = m_pHeaders[m_iNumHeaders].pTexHdr;
	g_StudioRenderer.m_pRenderModel = pTempModel;

	g_StudioRenderer.StudioSetUpTransform(0);
	g_StudioRenderer.StudioSetupBones();

	g_StudioRenderer.StudioSaveModelData(&m_pHeaders[m_iNumHeaders]);

	delete[] pTempEnt;
	delete[] pTempModel;

	m_pCurrentExtraData->pModelData = &m_pHeaders[m_iNumHeaders];
	m_iNumHeaders++;
	return true;
}

/*
====================
SetupCable

====================
*/
bool CPropManager::SetupCable(cabledata_t* cable, entity_t* entity)
{
	char sz[64];
	Vector vdroppoint;
	Vector vposition1;
	Vector vposition2;
	Vector vdirection;
	Vector vmidpoint;
	Vector vendpoint;

	// Get our origin
	char* pValue = ValueForKey(entity, "origin");

	if (pValue == nullptr)
		return false;

	sscanf(pValue, "%f %f %f", &vposition1[0], &vposition1[1], &vposition1[2]);

	// Find our target entity
	pValue = ValueForKey(entity, "target");

	if (pValue == nullptr)
		return false;

	strcpy(sz, pValue);

	for (int i = 0; i < m_iNumBSPEntities; i++)
	{
		pValue = ValueForKey(&m_pBSPEntities[i], "targetname");

		if (pValue == nullptr)
			continue;

		if (strcmp(pValue, sz) == 0)
		{
			pValue = ValueForKey(&m_pBSPEntities[i], "origin");

			if (pValue == nullptr)
				return false;

			// Copy origin over
			sscanf(pValue, "%f %f %f", &vposition2[0], &vposition2[1], &vposition2[2]);
		}
	}

	// Get our falling depth
	pValue = ValueForKey(entity, "falldepth");

	if (pValue == nullptr)
		return false;

	// Calculate dropping point
	VectorSubtract(vposition2, vposition1, vdirection);
	VectorMASSE(vposition1, 0.5, vdirection, vmidpoint);
	vdroppoint = Vector(vmidpoint[0], vmidpoint[1], vmidpoint[2] - atoi(pValue));

	// Get sprite width
	pValue = ValueForKey(entity, "spritewidth");

	if (pValue == nullptr)
		return false;

	cable->iwidth = atoi(pValue);

	// Get segment count
	pValue = ValueForKey(entity, "segments");

	if (pValue == nullptr)
		return false;

	cable->isegments = atoi(pValue);
	cable->inumpoints = cable->isegments + 1;

	cable->vmins = Vector(4096, 4096, 4096);
	cable->vmaxs = Vector(-4096, -4096, -4096);

	for (int i = 0; i < cable->inumpoints; i++)
	{
		float f = (float)i / (float)cable->isegments;
		cable->vpoints[i][0] = vposition1[0] * ((1 - f) * (1 - f)) + vdroppoint[0] * ((1 - f) * f * 2) + vposition2[0] * (f * f);
		cable->vpoints[i][1] = vposition1[1] * ((1 - f) * (1 - f)) + vdroppoint[1] * ((1 - f) * f * 2) + vposition2[1] * (f * f);
		cable->vpoints[i][2] = vposition1[2] * ((1 - f) * (1 - f)) + vdroppoint[2] * ((1 - f) * f * 2) + vposition2[2] * (f * f);

		for (int j = 0; j < 3; j++)
		{
			if ((cable->vpoints[i][j] + cable->iwidth) > cable->vmaxs[j])
				cable->vmaxs[j] = cable->vpoints[i][j] + cable->iwidth;

			if ((cable->vpoints[i][j] - cable->iwidth) > cable->vmaxs[j])
				cable->vmaxs[j] = cable->vpoints[i][j] - cable->iwidth;

			if ((cable->vpoints[i][j] + cable->iwidth) < cable->vmins[j])
				cable->vmins[j] = cable->vpoints[i][j] + cable->iwidth;

			if ((cable->vpoints[i][j] - cable->iwidth) < cable->vmins[j])
				cable->vmins[j] = cable->vpoints[i][j] - cable->iwidth;
		}
	}

	entextradata_t pdata;
	memset(&pdata, 0, sizeof(pdata));

	VectorCopy(cable->vmaxs, pdata.absmax);
	VectorCopy(cable->vmins, pdata.absmin);
	SV_FindTouchedLeafs(&pdata, gBSPRenderer.m_pWorld->nodes);

	memcpy(cable->leafnums, pdata.leafnums, sizeof(short) * MAX_ENT_LEAFS);
	cable->num_leafs = pdata.num_leafs;

	return true;
}

/*
====================
DrawCables

====================
*/
void CPropManager::DrawCables()
{
	Vector vVertex;
	Vector vTangent;
	Vector vDir;
	Vector vRight;

	if (m_pCvarDrawClientEntities->value < 1)
		return;

	glDisable(GL_TEXTURE_2D);
	glDisable(GL_CULL_FACE);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PRIMARY_COLOR_ARB);

	for (int i = 0; i < m_iNumCables; i++)
	{
		int j = 0;
		for (; j < m_pCables[i].num_leafs; j++)
			if ((gBSPRenderer.m_pPVS[m_pCables[i].leafnums[j] >> 3] & (1 << (m_pCables[i].leafnums[j] & 7))) != 0)
				break;

		if (j == m_pCables[i].num_leafs)
			continue;

		if (gHUD.viewFrustum.CullBox(m_pCables[i].vmins, m_pCables[i].vmaxs))
			continue;

		glBegin(GL_TRIANGLE_STRIP);
		for (int j = 0; j < m_pCables[i].inumpoints; j++)
		{
			if (j == 0)
			{
				VectorSubtract(m_pCables[i].vpoints[0], m_pCables[i].vpoints[1], vTangent);
			}
			else
			{
				VectorSubtract(m_pCables[i].vpoints[0], m_pCables[i].vpoints[j], vTangent);
			}

			VectorSubtract(m_pCables[i].vpoints[j], gBSPRenderer.m_vRenderOrigin, vDir);
			vRight = CrossProduct(vTangent, -vDir);
			VectorNormalizeFast(vRight);

			glColor3f(GL_ZERO, GL_ZERO, GL_ZERO);
			VectorMASSE(m_pCables[i].vpoints[j], m_pCables[i].iwidth, vRight, vVertex);
			glVertex3fv(vVertex);

			VectorMASSE(m_pCables[i].vpoints[j], -m_pCables[i].iwidth, vRight, vVertex);
			glVertex3fv(vVertex);
		}
		glEnd();
	}

	glEnable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_2D);
	glColor3f(GL_ONE, GL_ONE, GL_ONE);
}

/*
====================
RenderPropsSolid

====================
*/
void CPropManager::RenderPropsSolid()
{
	if (m_pCvarDrawClientEntities->value < 1)
		return;

	if (g_StudioRenderer.m_pCvarDrawModels->value < 1)
		return;

	if (g_StudioRenderer.m_pCvarDrawEntities->value < 1)
		return;

	gBSPRenderer.glClientActiveTextureARB(GL_TEXTURE0_ARB);
	gBSPRenderer.glActiveTextureARB(GL_TEXTURE0_ARB);
	gBSPRenderer.glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, m_uiIndexBuffer);
	glTexCoordPointer(2, GL_FLOAT, sizeof(brushvertex_t), OFFSET(brushvertex_t, texcoord));

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	for (int i = 0; i < m_iNumEntities; i++)
	{
		if (m_pEntities[i].curstate.renderfx == 70)
			continue;

		entextradata_t* pExtraData = ((entextrainfo_t*)m_pEntities[i].topnode)->pExtraData;

		if (pExtraData == nullptr)
			return;

		int j = 0;
		for (; j < pExtraData->num_leafs; j++)
			if ((gBSPRenderer.m_pPVS[pExtraData->leafnums[j] >> 3] & (1 << (pExtraData->leafnums[j] & 7))) != 0)
				break;

		if (j == pExtraData->num_leafs)
			continue;

		g_StudioRenderer.StudioDrawExternalEntitySolid(&m_pEntities[i]);
	}

	gBSPRenderer.glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

/*
====================
RenderSkyProps

====================
*/
void CPropManager::RenderSkyProps()
{
	if (m_pCvarDrawClientEntities->value < 1)
		return;

	if (g_StudioRenderer.m_pCvarDrawModels->value < 1)
		return;

	if (g_StudioRenderer.m_pCvarDrawEntities->value < 1)
		return;

	gBSPRenderer.glClientActiveTextureARB(GL_TEXTURE0_ARB);
	gBSPRenderer.glActiveTextureARB(GL_TEXTURE0_ARB);
	gBSPRenderer.glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, m_uiIndexBuffer);
	glTexCoordPointer(2, GL_FLOAT, sizeof(brushvertex_t), OFFSET(brushvertex_t, texcoord));

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	for (int i = 0; i < m_iNumEntities; i++)
	{
		if (m_pEntities[i].curstate.renderfx != 70)
			continue;

		g_StudioRenderer.StudioDrawExternalEntity(&m_pEntities[i]);
	}

	gBSPRenderer.glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}
