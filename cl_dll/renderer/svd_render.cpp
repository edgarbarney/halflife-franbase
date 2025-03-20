//========= Copyright � 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "PlatformHeaders.h"
#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "com_model.h"
#include "studio.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "dlight.h"
#include "triangleapi.h"

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <math.h>

#include "studio_util.h"
#include "r_studioint.h"

//RENDERERS START
#include "bsprenderer.h"
#include "propmanager.h"
#include "particle_engine.h"
#include "watershader.h"
#include "mirrormanager.h"

#include "StudioModelRenderer.h"
#include "GameStudioModelRenderer.h"

extern CGameStudioModelRenderer g_StudioRenderer;
//RENDERERS END


#include "pmtrace.h"
#include "r_efx.h"
#include "event_api.h"
#include "event_args.h"
#include "in_defs.h"
#include "pm_defs.h"
#include "svd_format.h"
#include "svd_render.h"

// Quake definitions
#define	SURF_PLANEBACK		2
#define	SURF_DRAWSKY		4
#define SURF_DRAWSPRITE		8
#define SURF_DRAWTURB		0x10
#define SURF_DRAWTILED		0x20
#define SURF_DRAWBACKGROUND	0x40
#define SURF_UNDERWATER		0x80
#define SURF_DONTWARP		0x100
#define BACKFACE_EPSILON	0.01

// 0-2 are axial planes
#define	PLANE_X			0
#define	PLANE_Y			1
#define	PLANE_Z			2

// Globals used by shadow rendering
model_t*	g_pWorld;
int			g_visFrame;
int			g_frameCount;
Vector		g_viewOrigin;

// msurface_t struct size
int			g_msurfaceStructSize = 0;


/*
====================
Mod_PointInLeaf

====================
*/
extern mleaf_t* Mod_PointInLeaf(Vector p, model_t* model); // quake's func


/*
====================
SVD_DrawBrushModel

====================
*/
void SVD_DrawBrushModel(cl_entity_t* pentity)
{
	model_t* pmodel = pentity->model;

	Vector vlocalview;
	Vector vmins, vmaxs;

	// set model-local view origin
	vlocalview = g_viewOrigin;

	if ((pentity->angles[0] != 0.0f) || (pentity->angles[1] != 0.0f) || (pentity->angles[2] != 0.0f))
	{
		for (int i = 0; i < 3; i++)
		{
			vmins[i] = pentity->origin[i] - pmodel->radius;
			vmaxs[i] = pentity->origin[i] + pmodel->radius;
		}
	}
	else
	{
		vmins = pentity->origin + pmodel->mins;
		vmaxs = pentity->origin + pmodel->maxs;
	}
	vlocalview = vlocalview - pentity->origin;

	if ((pentity->angles[0] != 0.0f) || (pentity->angles[1] != 0.0f) || (pentity->angles[2] != 0.0f))
	{
		Vector	vtemp, vforward, vright, vup;
		vtemp = vlocalview;
		AngleVectors(pentity->angles, vforward, vright, vup);
		vlocalview[0] = DotProduct(vtemp, vforward);
		vlocalview[1] = -DotProduct(vtemp, vright);
		vlocalview[2] = DotProduct(vtemp, vup);
	}

	if ((pentity->curstate.origin[0] != 0.0f) || (pentity->curstate.origin[1] != 0.0f) || (pentity->curstate.origin[2] != 0.0f)
		|| (pentity->curstate.angles[0] != 0.0f) || (pentity->curstate.angles[1] != 0.0f) || (pentity->curstate.angles[2] != 0.0f))
	{
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();

		glTranslatef(pentity->curstate.origin[0], pentity->curstate.origin[1], pentity->curstate.origin[2]);

		glRotatef(pentity->curstate.angles[1], 0, 0, 1);
		glRotatef(pentity->curstate.angles[0], 0, 1, 0);
		glRotatef(pentity->curstate.angles[2], 1, 0, 0);
	}

	byte* pfirstsurfbyteptr = reinterpret_cast<byte*>(g_pWorld->surfaces);
	for (int i = 0; i < pmodel->nummodelsurfaces; i++)
	{
		msurface_t* psurface = reinterpret_cast<msurface_t*>(pfirstsurfbyteptr + g_msurfaceStructSize * (pmodel->firstmodelsurface + i));
		mplane_t* pplane = psurface->plane;

		float fldot = DotProduct(vlocalview, pplane->normal) - pplane->dist;

		if ((((psurface->flags & SURF_PLANEBACK) != 0) && (fldot < -BACKFACE_EPSILON))
			|| (((psurface->flags & SURF_PLANEBACK) == 0) && (fldot > BACKFACE_EPSILON)))
		{
			if ((psurface->flags & SURF_DRAWSKY) != 0)
				continue;

			if ((psurface->flags & SURF_DRAWTURB) != 0)
				continue;

			glpoly_t* p = psurface->polys;
			float* v = p->verts[0];

			glBegin(GL_POLYGON);
			for (int j = 0; j < p->numverts; j++, v += VERTEXSIZE)
			{
				glTexCoord2f(v[3], v[4]);
				glVertex3fv(v);
			}
			glEnd();
		}
	}

	if ((pentity->curstate.origin[0] != 0.0f) || (pentity->curstate.origin[1] != 0.0f) || (pentity->curstate.origin[2] != 0.0f)
		|| (pentity->curstate.angles[0] != 0.0f) || (pentity->curstate.angles[1] != 0.0f) || (pentity->curstate.angles[2] != 0.0f))
	{
		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();
	}
}

/*
====================
R_DetermineSurfaceStructSize

====================
*/
int R_DetermineSurfaceStructSize()
{
	model_t* pworld = IEngineStudio.GetModelByIndex(1);
	assert(pworld);

	mplane_t* pplanes = pworld->planes;
	msurface_t* psurfaces = pworld->surfaces;

	// Try to find second texinfo ptr
	byte* psecondsurfbytedata = reinterpret_cast<byte*>(&psurfaces[1]);

	// Size of msurface_t with that stupid displaylist junk
	static const int MAXOFS = 108;

	int byteoffs = 0;
	while (byteoffs <= MAXOFS)
	{
		mplane_t** pplaneptr = reinterpret_cast<mplane_t**>(psecondsurfbytedata + byteoffs);

		int i = 0;
		for (; i < pworld->numplanes; i++)
		{
			if (&pplanes[i] == *pplaneptr)
				break;
		}

		if (i != pworld->numplanes)
		{
			break;
		}

		byteoffs++;
	}

	if (byteoffs >= MAXOFS)
	{
		gEngfuncs.Con_Printf("%s - Failed to determine msurface_t struct size.\n");
		return sizeof(msurface_t);
	}
	else
	{
		mplane_t** pfirstsurftexinfoptr = &psurfaces[0].plane;
		byte* psecondptr = reinterpret_cast<byte*>(psecondsurfbytedata) + byteoffs;
		byte* ptr = reinterpret_cast<byte*>(pfirstsurftexinfoptr);
		return ((unsigned int)psecondptr - (unsigned int)ptr);
	}
}

/*
====================
SVD_RecursiveDrawWorld

====================
*/
void SVD_RecursiveDrawWorld ( mnode_t *node )
{
	if (g_msurfaceStructSize == 0)
		g_msurfaceStructSize = R_DetermineSurfaceStructSize();

	if (node->contents == CONTENTS_SOLID)
		return;

	if (node->visframe != g_visFrame)
		return;
	
	if (node->contents < 0)
		return;		// faces already marked by engine

	// recurse down the children, Order doesn't matter
	SVD_RecursiveDrawWorld (node->children[0]);
	SVD_RecursiveDrawWorld (node->children[1]);

	// draw stuff
	int c = node->numsurfaces;
	if (c != 0)
	{
		msurface_t	*surf = g_pWorld->surfaces + node->firstsurface;

		for ( ; c != 0 ; c--, surf++)
		{
			if (surf->visframe != g_frameCount)
				continue;

			if ((surf->flags & (SURF_DRAWSKY|SURF_DRAWTURB|SURF_UNDERWATER)) != 0)
				continue;

			glpoly_t *p = surf->polys;
			float *v = p->verts[0];

			glBegin (GL_POLYGON);			
			for (int i = 0; i < p->numverts; i++, v+= VERTEXSIZE)
			{
				glTexCoord2f (v[3], v[4]);
				glVertex3fv (v);
			}
			glEnd ();
		}
	}
}

/*
====================
SVD_CalcRefDef

====================
*/
void SVD_CalcRefDef ( ref_params_t* pparams )
{
	if(IEngineStudio.IsHardware() != 1)
		return;

	SVD_CheckInit();


	glClear( GL_STENCIL_BUFFER_BIT );

	//g_StudioRenderer.StudioSetBuffer();
}

/*
====================
SVD_DrawNormalTriangles

====================
*/
void SVD_DrawNormalTriangles ( )
{

	if(IEngineStudio.IsHardware() != 1)
		return;

	// stencil pass start
	glPushAttrib(GL_TEXTURE_BIT);

	//gEngfuncs.Con_Printf("hihiih\n");

	// buz: workaround half-life's bug, when multitexturing left enabled after
	// rendering brush entities
	gBSPRenderer.glActiveTextureARB( GL_TEXTURE1_ARB );
	glDisable(GL_TEXTURE_2D);
	gBSPRenderer.glActiveTextureARB( GL_TEXTURE0_ARB );

	glDepthMask(GL_FALSE);
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor4f(GL_ZERO, GL_ZERO, GL_ZERO, 0.4);

	glStencilFunc(GL_NOTEQUAL, 0, ~0);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	glEnable(GL_STENCIL_TEST);

	// Set worldspawn
	g_pWorld = IEngineStudio.GetModelByIndex(1);

	// Set view origin
	g_viewOrigin = gBSPRenderer.m_vRenderOrigin;

	// get current visframe number
	g_visFrame = gBSPRenderer.m_iVisFrame;

	// get current frame number
	g_frameCount = gBSPRenderer.m_iFrameCount;

	// draw world
	SVD_RecursiveDrawWorld( g_pWorld->nodes );

	// draw brushmodels
	
	// Get local player
	cl_entity_t* plocalplayer = gEngfuncs.GetLocalPlayer();

	for (int i = 1; i < 512; i++)
	{
		cl_entity_t* pentity = gEngfuncs.GetEntityByIndex(i);
		if (pentity == nullptr)
			break;

		// brushmodels
		if(!((pentity->model == nullptr) || pentity->model->type != mod_brush) && !(pentity->curstate.messagenum != plocalplayer->curstate.messagenum) && (!(pentity->curstate.rendermode != kRenderNormal) || (pentity->curstate.rendermode == kRenderTransTexture && pentity->curstate.renderamt == 30)))
			SVD_DrawBrushModel(pentity);

		/*
		// studiomodels - not ready yet
		else if (!(!pentity->model || pentity->model->type != mod_studio))
		{
			auto restore = g_StudioRenderer.m_pStudioHeader;
			g_StudioRenderer.m_pStudioHeader = (studiohdr_t*)IEngineStudio.Mod_Extradata(pentity->model);

			for (int i = 0; i < g_StudioRenderer.m_pStudioHeader->numbodyparts; i++)
			{
				g_StudioRenderer.StudioSetupModel(i, (void**)&g_StudioRenderer.m_pBodyPart, (void**)&g_StudioRenderer.m_pSubModel);
				g_StudioRenderer.StudioGetVerts();
				g_StudioRenderer.StudioDrawPointsShadow();
			}

			g_StudioRenderer.m_pStudioHeader = restore;
		}
		*/
	}


	glDepthMask(GL_TRUE);
	glEnable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glDisable(GL_STENCIL_TEST);

	//g_StudioRenderer.StudioClearBuffer();
	glPopAttrib();

	// stencil pass end
}