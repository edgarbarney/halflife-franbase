/*
Trinity Rendering Engine - Copyright Andrew Lucas 2009-2012

The Trinity Engine is free software, distributed in the hope th-
at it will be useful, but WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU Lesser General Public License for more det-
ails.

BSP Renderer
Original code by Buzer and Id Software
Extended and/or recoded by Andrew Lucas
*/

#if !defined(BSPRENDERER_H)
#define BSPRENDERER_H
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

/*
====================
CBSPRenderer

====================
*/
class CBSPRenderer
{
public:
	void Init();
	void VidInit();
	void Shutdown();
	void SetupRenderer();
	void SetupPreFrame(ref_params_t* vieworg);
	void CheckTextures();

	void DrawShadowPasses();
	void CreateShadowMap();
	void DrawWorldSolid();
	void SetupSpotlightVis();

	void DrawDetailsSolid();
	void RecursiveWorldNodeSolid(mnode_t* node);
	void DrawBrushModelSolid(cl_entity_t* pEntity);

	void ClearDetailObjects();
	void LoadDetailFile();
	void DrawDetails();

	void RendererRefDef(ref_params_t* pparams);
	void DrawNormalTriangles();
	void DrawTransparentTriangles();
	void RenderFirstPass(bool bSecond = false);
	void RenderFinalPasses(bool bSecond = false);
	void DrawWorld();

	void GetRenderEnts();
	void AddEntity(cl_entity_t* pEntity);
	int FilterEntities(int type, struct cl_entity_s* pEntity, const char* modelname); //

	void DecayLights();
	bool HasDynLights();
	void GetAdditionalLights();
	cl_dlight_t* CL_AllocDLight(int key);
	int MsgDynLight(const char* pszName, int iSize, void* pbuf);

	void SetupDynLight();
	void FinishDynLight();
	void SetDynLightBBox();
	void SetupSpotLight();
	void FinishSpotLight();
	bool LightCanShadow();
	int CullDynLightBBox(Vector mins, Vector maxs);

	void PushDynLights();
	void MarkLights(cl_dlight_t* pLight, int iBit, mnode_t* node);
	void MarkBrushFaces(Vector mins, Vector maxs);
	void AddDynamicLights(msurface_t* surf);

	void DisableWorldDrawing(ref_params_t* pparam);
	void RestoreWorldDrawing();

	void CreateTextures();

	void FreeBuffer();
	void GenerateVertexArray();
	void EnableVertexArray();
	void DisableVertexArray();

	void DrawBrushModel(cl_entity_t* pEntity, bool bStatic = false);
	void RecursiveWorldNode(mnode_t* node);

	void SurfaceToChain(msurface_t* psurfbase, msurface_t* s, bool dynlit);
	void DrawScrollingPoly(msurface_t* pbaseptr, msurface_t* psurf);
	void EmitWaterPolys(msurface_t* psurf);
	void DrawPolyFromArray(msurface_t* pbaseptr, msurface_t* psurf);

	bool DynamicLighted(const Vector& vmins, const Vector& vmaxs);
	void DrawDynamicLightsForWorld();
	void DrawDynamicLightsForDetails();
	void RecursiveWorldNodeLight(mnode_t* node);
	void DrawDynamicLightsForEntity(cl_entity_t* pEntity);
	void DrawEntityFacesForLight(cl_entity_t* pEntity);

	void InitSky();
	void DrawSky();
	void RemoveSky();
	int MsgSkyMarker_Sky(const char* pszName, int iSize, void* pbuf);
	int MsgSkyMarker_World(const char* pszName, int iSize, void* pbuf);

	void PrepareRenderer();
	void ResetRenderer();
	void ResetCache();

	bool ExtensionSupported(const char* ext);
	cl_texture_t* LoadDetailTexture(const std::string& texname);
	void ParseDetailTextureFile();
	void LoadDetailTextures();

	void AnimateLight();
	void UploadLightmaps();
	void BuildLightmap(msurface_t* surf, int surfindex, color24* out);
	void AddLightStyle(size_t index, const std::string& strStyle);
	void ClearLightStyleValues();
	int GetLightStyleValue(int index);

	void SetTexEnvs(int env0 = 0, int env1 = 0, int env2 = 0, int env3 = 0);
	void SetTexEnv_Internal(int env);
	void SetTexPointer(int unitnum, int tc);
	void Bind2DTexture(GLenum texture, GLuint id);

	texture_t* TextureAnimation(texture_t* base, int frame);

public:
	void DrawDecals();
	void LoadDecals();
	void DeleteDecals();

	cl_texture_t* LoadDecalTexture(const std::string& texname);
	std::string GetRandomDecalFromGroup(const std::string& group);
	std::string FindGroupByDecalName(const std::string& name);

	bool CullDecalBBox(Vector mins, Vector maxs);
	void CreateDecal(Vector endpos, Vector pnormal, const std::string& name, bool persistent = false);
	void RecursiveCreateDecal(mnode_t* node, DecalTexture& texture, CustomDecal* pDecal, Vector endpos, Vector pnormal);
	void DecalSurface(msurface_t* surf, DecalTexture& texture, cl_entity_t* pEntity, CustomDecal* pDecal, Vector endpos, Vector pnormal);

	int MsgCustomDecal(const char* pszName, int iSize, void* pbuf);

	void CreateCachedDecals();
	void DrawSingleDecal(const CustomDecal& decal);

	size_t AllocDecal();
	size_t AllocStaticDecal();

	void GetUpRight(Vector forward, Vector& up, Vector& right);
	int ClipPolygonByPlane(const std::vector<Vector>& vecIn, Vector normal, Vector planepoint, std::vector<Vector>& vecOut);
	Vector FindIntersectionPoint(const Vector& p1, const Vector& p2, const Vector& normal, const Vector& planepoint);

public:
	GLuint m_uiBufferIndex;
	brushvertex_t* m_pBufferData;
	brushface_t* m_pFacesExtraData;

	GLuint m_uiCurrentBinds[16];

	cl_entity_t* m_pCurrentEntity;
	cl_dlight_t* m_pCurrentDynLight;
	model_t* m_pWorld;
	byte* m_pPVS;
	mleaf_t* m_pViewLeaf;

	dlight_t* m_pFirstDLight;
	dlight_t* m_pFirstELight;

	int m_iTotalVertCount;
	int m_iTotalFaceCount;
	int m_iTotalTriCount;

	int m_iTexPointer[4];
	int m_iEnvStates[4];
	int m_iTUSupport;

	int m_iVisFrame;
	int m_iFrameCount;

	cl_texture_t* m_pFlashlightTextures[MAX_SPOTLIGHT_TEXTURES];
	int m_iNumFlashlightTextures;

	int m_iAttenuation1DTexture;

	bool m_bCanDraw;
	bool m_bDrawSky;
	bool m_bSecondPassNeeded;
	bool m_bMirroring;
	bool m_bLightShadow;

	bool m_bReloaded;
	bool m_bRadialFogSupport;
	bool m_bShaderSupport;
	bool m_bShadowSupport;
	bool m_bShadowPCFSupport;
	bool m_bSpecialFog;
	bool m_bGotAdditional;

	bool m_bDontPromptShaders;
	bool m_bDontPromptShadersError;
	bool m_bDontPromptShadow;
	bool m_bDontPromptShadowPCF;
	bool m_bDontPromptParanoia;

	Vector m_vRenderOrigin;
	Vector m_vViewAngles;
	Vector m_vVecToEyes;

	Vector m_vSkyOrigin;
	Vector m_vSkyWorldOrigin;

	Vector m_vCurDLightOrigin;
	Vector m_vCurSpotForward;

	cvar_t* m_pCvarSpeeds;
	cvar_t* m_pCvarDetailTextures;
	cvar_t* m_pCvarDynamic;
	cvar_t* m_pCvarDrawWorld;
	cvar_t* m_pCvarWireFrame;
	cvar_t* m_pCvarWorldShaders;
	cvar_t* m_pCvarRadialFog;
	cvar_t* m_pCvarPCFShadows;
	cvar_t* m_pCvarShadows;

	Vector m_vDLightMins;
	Vector m_vDLightMaxs;

	mnode_t* m_pTrueRootNode;
	mnode_t m_dummyNode;
	float m_fSkySpeed;

	cl_entity_t* m_pRenderEntities[MAXRENDERENTS];
	int m_iNumRenderEntities;

	cl_dlight_t m_pDynLights[MAX_DYNLIGHTS];

	mlight_t m_pModelLights[MAXRENDERENTS];
	int m_iNumModelLights;

	std::vector<LightStyle> m_vectorLightStyles;

	color24 m_pBlockLights[BLOCKLIGHTS_SIZE];
	int m_iNumLightmaps;

	color24 m_pEngineLightmaps[MAX_LIGHTMAPS * BLOCK_WIDTH * BLOCK_HEIGHT];
	int m_iEngineLightmapIndex;

	color24 m_pDetailLightmaps[MAX_LIGHTMAPS * BLOCK_WIDTH * BLOCK_HEIGHT];
	int m_iDetailLightmapIndex;

	brushface_t** m_pSurfacePointersArray;

	clientsurfdata_t* m_pSurfaces;
	int m_iNumSurfaces;

	msurface_t* m_pDetailObjectSurfaces;
	int m_numDetailSurfaces;

	detailobject_t m_pDetailObjects[MAX_MAP_DETAILOBJECTS];
	int m_iNumDetailObjects;
	int m_iNumDetailSurfaces;

	int m_iAtten3DPoint;
	int m_iLightDummy;

	int m_iSkyTextures[6];

	int m_iWorldPolyCounter;  // wpoly counter
	int m_iBrushPolyCounter;  // bmodel poly counter
	int m_iStudioPolyCounter; // studiomodel poly counter

	std::string m_strSkyName;
	std::string m_strMapName;

	std::vector<DetailTexture> m_vectorDetailTextures;

	texture_t m_pNormalTextureList[MAX_MAP_TEXTURES];
	texture_t m_pMultiPassTextureList[MAX_MAP_TEXTURES];
	int m_iNumTextures;

	GLuint m_iFogFragmentID;
	GLuint m_iShadowFragmentID;
	GLuint m_iDecalFragmentID;

	PFNGLCLIENTACTIVETEXTUREARBPROC glClientActiveTextureARB;
	PFNGLACTIVETEXTUREARBPROC glActiveTextureARB;
	PFNGLMULTITEXCOORD2FARBPROC glMultiTexCoord2fARB;

	PFNGLBINDBUFFERARBPROC glBindBufferARB;
	PFNGLGENBUFFERSARBPROC glGenBuffersARB;
	PFNGLBUFFERDATAARBPROC glBufferDataARB;
	PFNGLDELETEBUFFERSARBPROC glDeleteBuffersARB;

	PFNGLLOCKARRAYSEXTPROC glLockArraysEXT;
	PFNGLUNLOCKARRAYSEXTPROC glUnlockArraysEXT;

	PFNGLTEXIMAGE3DEXTPROC glTexImage3DEXT;

	PFNGLGENPROGRAMSARBPROC glGenProgramsARB;
	PFNGLBINDPROGRAMARBPROC glBindProgramARB;
	PFNGLPROGRAMSTRINGARBPROC glProgramStringARB;
	PFNGLGETPROGRAMIVARBPROC glGetProgramivARB;

	PFNGLPROGRAMLOCALPARAMETER4FARBPROC glProgramLocalParameter4fARB;

	PFNGLFOGCOORDPOINTEREXTPROC glFogCoordPointer;
#ifdef HL25_UPDATE
	PFNGLUSEPROGRAMPROC glUseProgram;
#endif

public:
	std::vector<CustomDecal> m_vectorDecals;
	std::vector<CustomDecal> m_vectorStaticDecals;
	std::vector<DecalMessage> m_vectorMsgCache;
	std::unordered_map<std::string, DecalTextureGroup> m_mapDecalTexGroups;

	Vector m_vDecalMins;
	Vector m_vDecalMaxs;
};
extern CBSPRenderer gBSPRenderer;
#endif
