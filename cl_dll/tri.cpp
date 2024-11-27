//========= Copyright � 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

// Triangle rendering, if any

#include "hud.h"
#include "cl_util.h"

// Triangle rendering apis are in gEngfuncs.pTriAPI

#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "triangleapi.h"
#include "com_model.h"
#include "studio_util.h"

// RENDERERS START
#include "renderer/bsprenderer.h"
#include "renderer/propmanager.h"
#include "renderer/particle_engine.h"
#include "renderer/watershader.h"
#include "renderer/mirrormanager.h"

#include "studio.h"
#include "StudioModelRenderer.h"
#include "GameStudioModelRenderer.h"

extern CGameStudioModelRenderer g_StudioRenderer;
// RENDERERS END

#include "Exports.h"
#include "tri.h"

#include "glInclude.h"

extern int g_iWaterLevel;
extern Vector v_origin;

int UseTexture(SpriteHandle_t& hsprSpr, char* str)
{
	if (hsprSpr == 0)
	{
		char sz[256];
		sprintf(sz, str);
		hsprSpr = SPR_Load(sz);
	}

	return gEngfuncs.pTriAPI->SpriteTexture((struct model_s*)gEngfuncs.GetSpritePointer(hsprSpr), 0);
}

//
//-----------------------------------------------------
//

void SetPoint(float x, float y, float z, float (*matrix)[4])
{
	Vector point, result;
	point[0] = x;
	point[1] = y;
	point[2] = z;

	VectorTransform(point, matrix, result);

	gEngfuncs.pTriAPI->Vertex3f(result[0], result[1], result[2]);
}

/*
=================
HUD_DrawNormalTriangles

Non-transparent triangles-- add them here
=================
*/
void DLLEXPORT HUD_DrawNormalTriangles()
{
	//	RecClDrawNormalTriangles();

// RENDERERS START
	// 2012-02-25
	R_DrawNormalTriangles();
// RENDERERS END

	gHUD.m_Spectator.DrawOverview();
}


/*
=================
HUD_DrawTransparentTriangles

Render any triangles with transparent rendermode needs here
=================
*/
void DLLEXPORT HUD_DrawTransparentTriangles()
{
	//	RecClDrawTransparentTriangles();
	
// RENDERERS START
	// 2012-02-25
	R_DrawTransparentTriangles();
// RENDERERS END

	/*
	// LRC: find out the time elapsed since the last redraw
	static float fOldTime, fTime;
	fOldTime = fTime;
	fTime = gEngfuncs.GetClientTime();
	*/
}
