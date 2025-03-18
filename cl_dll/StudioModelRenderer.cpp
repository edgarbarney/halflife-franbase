/*
Trinity Rendering Engine - Copyright Andrew Lucas 2009-2012
FranBase Modbase - Copyright FranticDreamer 2020-2025

The Trinity Engine is free software, distributed in the hope th-
at it will be useful, but WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU Lesser General Public License for more det-
ails.

Model Renderer
Original code by Valve
Additional code written by Andrew Lucas
Transparency code by Neil "Jed" Jedrzejewski
*/

#if defined( _WIN32 )
#include "windows.h"
#endif

#include <cstdio>
#include <cstdlib>
#include <cmath>

#include "GL/gl.h"
#include "GL/glext.h"

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "com_model.h"
#include "studio.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "dlight.h"
#include "triangleapi.h"

#include "event_api.h"
#include "pmtrace.h"

#include <string.h>
#include <memory.h>

#include "studio_util.h"
#include "r_studioint.h"

#include "renderer/rendererdefs.h"
#include "renderer/propmanager.h"
#include "renderer/bsprenderer.h"
#include "StudioModelRenderer.h"
#include "mathlib.h"

#include "FranUtils/FranUtils_Maths.hpp"
#include "FranUtils/FranUtils_String.hpp"

viewinfo_s g_viewinfo;

// Global engine <-> studio model rendering code interface
engine_studio_api_t IEngineStudio;

cvar_t* te_render_distance = nullptr;

struct cl_stored_light
{
	int index = 0;
	Vector color = Vector(0, 0, 0);
};

std::vector<cl_stored_light> StoredLightBuffer;

//===========================================
//	ARB SHADER
//===========================================
char arb_basic_depth[] =
	"!!ARBvp1.0"
	"TEMP R0;"
	"DP3 R0.w, vertex.normal, program.local[0];"
	"MAD R0.xyz, program.local[2], -R0.w, program.local[1];"

	"MAX R0.xyz, R0, 0;"
	"MIN R0.xyz, R0, 1;"
	"MOV result.color.xyz, R0;"
	"MOV result.color.w, vertex.color.w;"

	"DP4 R0.x, program.local[3], vertex.position;"
	"DP4 R0.y, program.local[4], vertex.position;"
	"DP4 R0.z, program.local[5], vertex.position;"
	"DP4 R0.w, program.local[6], vertex.position;"

	"DP4 result.position.x, program.local[7], R0;"
	"DP4 result.position.y, program.local[8], R0;"
	"DP4 result.position.w, program.local[10], R0;"
	"DP4 R0.x, program.local[9], R0;"

	"MOV result.position.z, R0.x;"
	"MOV result.fogcoord.x, R0.x;"

	"MUL result.texcoord[0].x, vertex.texcoord[0].x, program.local[11].x;"
	"MUL result.texcoord[0].y, vertex.texcoord[0].y, program.local[11].y;"
	"END";

char arb_1light_depth[] =
	"!!ARBvp1.0"
	"TEMP R0;"
	"TEMP R1;"
	"TEMP R2;"
	"DP3 R0.w, vertex.normal, program.local[0];"
	"MAD R0.xyz, program.local[2], -R0.w, program.local[1];"

	// Light 1
	"SUB R1.xyz, vertex.position, program.local[12];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[14];"
	"SUB R2.x, 1, program.local[14].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[14].w;"
	"MUL R2.x, program.local[13].w, program.local[13].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[12].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[13], R1.x, R0;"

	"MAX R0.xyz, R0, 0;"
	"MIN R0.xyz, R0, 1;"
	"MOV result.color.xyz, R0;"
	"MOV result.color.w, vertex.color.w;"

	"DP4 R0.x, program.local[3], vertex.position;"
	"DP4 R0.y, program.local[4], vertex.position;"
	"DP4 R0.z, program.local[5], vertex.position;"
	"DP4 R0.w, program.local[6], vertex.position;"

	"DP4 result.position.x, program.local[7], R0;"
	"DP4 result.position.y, program.local[8], R0;"
	"DP4 result.position.w, program.local[10], R0;"
	"DP4 R0.x, program.local[9], R0;"

	"MOV result.position.z, R0.x;"
	"MOV result.fogcoord.x, R0.x;"

	"MUL result.texcoord[0].x, vertex.texcoord[0].x, program.local[11].x;"
	"MUL result.texcoord[0].y, vertex.texcoord[0].y, program.local[11].y;"
	"END";

char arb_2lights_depth[] =
	"!!ARBvp1.0"
	"TEMP R0;"
	"TEMP R1;"
	"TEMP R2;"
	"DP3 R0.w, vertex.normal, program.local[0];"
	"MAD R0.xyz, program.local[2], -R0.w, program.local[1];"

	// Light 1
	"SUB R1.xyz, vertex.position, program.local[12];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[14];"
	"SUB R2.x, 1, program.local[14].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[14].w;"
	"MUL R2.x, program.local[13].w, program.local[13].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[12].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[13], R1.x, R0;"

	// Light 2
	"SUB R1.xyz, vertex.position, program.local[15];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[17];"
	"SUB R2.x, 1, program.local[17].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[17].w;"
	"MUL R2.x, program.local[16].w, program.local[16].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[15].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[16], R1.x, R0;"

	"MAX R0.xyz, R0, 0;"
	"MIN R0.xyz, R0, 1;"
	"MOV result.color.xyz, R0;"
	"MOV result.color.w, vertex.color.w;"

	"DP4 R0.x, program.local[3], vertex.position;"
	"DP4 R0.y, program.local[4], vertex.position;"
	"DP4 R0.z, program.local[5], vertex.position;"
	"DP4 R0.w, program.local[6], vertex.position;"

	"DP4 result.position.x, program.local[7], R0;"
	"DP4 result.position.y, program.local[8], R0;"
	"DP4 result.position.w, program.local[10], R0;"
	"DP4 R0.x, program.local[9], R0;"

	"MOV result.position.z, R0.x;"
	"MOV result.fogcoord.x, R0.x;"

	"MUL result.texcoord[0].x, vertex.texcoord[0].x, program.local[11].x;"
	"MUL result.texcoord[0].y, vertex.texcoord[0].y, program.local[11].y;"
	"END";

char arb_3lights_depth[] =
	"!!ARBvp1.0"
	"TEMP R0;"
	"TEMP R1;"
	"TEMP R2;"
	"DP3 R0.w, vertex.normal, program.local[0];"
	"MAD R0.xyz, program.local[2], -R0.w, program.local[1];"

	// Light 1
	"SUB R1.xyz, vertex.position, program.local[12];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[14];"
	"SUB R2.x, 1, program.local[14].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[14].w;"
	"MUL R2.x, program.local[13].w, program.local[13].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[12].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[13], R1.x, R0;"

	// Light 2
	"SUB R1.xyz, vertex.position, program.local[15];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[17];"
	"SUB R2.x, 1, program.local[17].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[17].w;"
	"MUL R2.x, program.local[16].w, program.local[16].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[15].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[16], R1.x, R0;"

	// Light 3
	"SUB R1.xyz, vertex.position, program.local[18];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[20];"
	"SUB R2.x, 1, program.local[20].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[20].w;"
	"MUL R2.x, program.local[19].w, program.local[19].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[18].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[19], R1.x, R0;"

	"MAX R0.xyz, R0, 0;"
	"MIN R0.xyz, R0, 1;"
	"MOV result.color.xyz, R0;"
	"MOV result.color.w, vertex.color.w;"

	"DP4 R0.x, program.local[3], vertex.position;"
	"DP4 R0.y, program.local[4], vertex.position;"
	"DP4 R0.z, program.local[5], vertex.position;"
	"DP4 R0.w, program.local[6], vertex.position;"

	"DP4 result.position.x, program.local[7], R0;"
	"DP4 result.position.y, program.local[8], R0;"
	"DP4 result.position.w, program.local[10], R0;"
	"DP4 R0.x, program.local[9], R0;"

	"MOV result.position.z, R0.x;"
	"MOV result.fogcoord.x, R0.x;"

	"MUL result.texcoord[0].x, vertex.texcoord[0].x, program.local[11].x;"
	"MUL result.texcoord[0].y, vertex.texcoord[0].y, program.local[11].y;"
	"END";

char arb_4lights_depth[] =
	"!!ARBvp1.0"
	"TEMP R0;"
	"TEMP R1;"
	"TEMP R2;"
	"DP3 R0.w, vertex.normal, program.local[0];"
	"MAD R0.xyz, program.local[2], -R0.w, program.local[1];"

	// Light 1
	"SUB R1.xyz, vertex.position, program.local[12];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[14];"
	"SUB R2.x, 1, program.local[14].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[14].w;"
	"MUL R2.x, program.local[13].w, program.local[13].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[12].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[13], R1.x, R0;"

	// Light 2
	"SUB R1.xyz, vertex.position, program.local[15];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[17];"
	"SUB R2.x, 1, program.local[17].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[17].w;"
	"MUL R2.x, program.local[16].w, program.local[16].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[15].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[16], R1.x, R0;"

	// Light 3
	"SUB R1.xyz, vertex.position, program.local[18];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[20];"
	"SUB R2.x, 1, program.local[20].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[20].w;"
	"MUL R2.x, program.local[19].w, program.local[19].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[18].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[19], R1.x, R0;"

	// Light 4
	"SUB R1.xyz, vertex.position, program.local[21];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[23];"
	"SUB R2.x, 1, program.local[23].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[23].w;"
	"MUL R2.x, program.local[22].w, program.local[22].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[21].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[22], R1.x, R0;"

	"MAX R0.xyz, R0, 0;"
	"MIN R0.xyz, R0, 1;"
	"MOV result.color.xyz, R0;"
	"MOV result.color.w, vertex.color.w;"

	"DP4 R0.x, program.local[3], vertex.position;"
	"DP4 R0.y, program.local[4], vertex.position;"
	"DP4 R0.z, program.local[5], vertex.position;"
	"DP4 R0.w, program.local[6], vertex.position;"

	"DP4 result.position.x, program.local[7], R0;"
	"DP4 result.position.y, program.local[8], R0;"
	"DP4 result.position.w, program.local[10], R0;"
	"DP4 R0.x, program.local[9], R0;"

	"MOV result.position.z, R0.x;"
	"MOV result.fogcoord.x, R0.x;"

	"MUL result.texcoord[0].x, vertex.texcoord[0].x, program.local[11].x;"
	"MUL result.texcoord[0].y, vertex.texcoord[0].y, program.local[11].y;"
	"END";

char arb_5lights_depth[] =
	"!!ARBvp1.0"
	"TEMP R0;"
	"TEMP R1;"
	"TEMP R2;"
	"DP3 R0.w, vertex.normal, program.local[0];"
	"MAD R0.xyz, program.local[2], -R0.w, program.local[1];"

	// Light 1
	"SUB R1.xyz, vertex.position, program.local[12];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[14];"
	"SUB R2.x, 1, program.local[14].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[14].w;"
	"MUL R2.x, program.local[13].w, program.local[13].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[12].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[13], R1.x, R0;"

	// Light 2
	"SUB R1.xyz, vertex.position, program.local[15];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[17];"
	"SUB R2.x, 1, program.local[17].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[17].w;"
	"MUL R2.x, program.local[16].w, program.local[16].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[15].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[16], R1.x, R0;"

	// Light 3
	"SUB R1.xyz, vertex.position, program.local[18];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[20];"
	"SUB R2.x, 1, program.local[20].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[20].w;"
	"MUL R2.x, program.local[19].w, program.local[19].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[18].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[19], R1.x, R0;"

	// Light 4
	"SUB R1.xyz, vertex.position, program.local[21];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[23];"
	"SUB R2.x, 1, program.local[23].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[23].w;"
	"MUL R2.x, program.local[22].w, program.local[22].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[21].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[22], R1.x, R0;"

	// Light 5
	"SUB R1.xyz, vertex.position, program.local[24];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[26];"
	"SUB R2.x, 1, program.local[26].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[26].w;"
	"MUL R2.x, program.local[25].w, program.local[25].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[24].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[25], R1.x, R0;"

	"MAX R0.xyz, R0, 0;"
	"MIN R0.xyz, R0, 1;"
	"MOV result.color.xyz, R0;"
	"MOV result.color.w, vertex.color.w;"

	"DP4 R0.x, program.local[3], vertex.position;"
	"DP4 R0.y, program.local[4], vertex.position;"
	"DP4 R0.z, program.local[5], vertex.position;"
	"DP4 R0.w, program.local[6], vertex.position;"

	"DP4 result.position.x, program.local[7], R0;"
	"DP4 result.position.y, program.local[8], R0;"
	"DP4 result.position.w, program.local[10], R0;"
	"DP4 R0.x, program.local[9], R0;"

	"MOV result.position.z, R0.x;"
	"MOV result.fogcoord.x, R0.x;"

	"MUL result.texcoord[0].x, vertex.texcoord[0].x, program.local[11].x;"
	"MUL result.texcoord[0].y, vertex.texcoord[0].y, program.local[11].y;"
	"END";

char arb_6lights_depth[] =
	"!!ARBvp1.0"
	"TEMP R0;"
	"TEMP R1;"
	"TEMP R2;"
	"DP3 R0.w, vertex.normal, program.local[0];"
	"MAD R0.xyz, program.local[2], -R0.w, program.local[1];"

	// Light 1
	"SUB R1.xyz, vertex.position, program.local[12];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[14];"
	"SUB R2.x, 1, program.local[14].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[14].w;"
	"MUL R2.x, program.local[13].w, program.local[13].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[12].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[13], R1.x, R0;"

	// Light 2
	"SUB R1.xyz, vertex.position, program.local[15];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[17];"
	"SUB R2.x, 1, program.local[17].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[17].w;"
	"MUL R2.x, program.local[16].w, program.local[16].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[15].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[16], R1.x, R0;"

	// Light 3
	"SUB R1.xyz, vertex.position, program.local[18];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[20];"
	"SUB R2.x, 1, program.local[20].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[20].w;"
	"MUL R2.x, program.local[19].w, program.local[19].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[18].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[19], R1.x, R0;"

	// Light 4
	"SUB R1.xyz, vertex.position, program.local[21];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[23];"
	"SUB R2.x, 1, program.local[23].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[23].w;"
	"MUL R2.x, program.local[22].w, program.local[22].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[21].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[22], R1.x, R0;"

	// Light 5
	"SUB R1.xyz, vertex.position, program.local[24];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[26];"
	"SUB R2.x, 1, program.local[26].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[26].w;"
	"MUL R2.x, program.local[25].w, program.local[25].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[24].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[25], R1.x, R0;"

	// Light 5
	"SUB R1.xyz, vertex.position, program.local[27];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[29];"
	"SUB R2.x, 1, program.local[29].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[29].w;"
	"MUL R2.x, program.local[28].w, program.local[28].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[27].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[28], R1.x, R0;"

	"MAX R0.xyz, R0, 0;"
	"MIN R0.xyz, R0, 1;"
	"MOV result.color.xyz, R0;"
	"MOV result.color.w, vertex.color.w;"

	"DP4 R0.x, program.local[3], vertex.position;"
	"DP4 R0.y, program.local[4], vertex.position;"
	"DP4 R0.z, program.local[5], vertex.position;"
	"DP4 R0.w, program.local[6], vertex.position;"

	"DP4 result.position.x, program.local[7], R0;"
	"DP4 result.position.y, program.local[8], R0;"
	"DP4 result.position.w, program.local[10], R0;"
	"DP4 R0.x, program.local[9], R0;"

	"MOV result.position.z, R0.x;"
	"MOV result.fogcoord.x, R0.x;"

	"MUL result.texcoord[0].x, vertex.texcoord[0].x, program.local[11].x;"
	"MUL result.texcoord[0].y, vertex.texcoord[0].y, program.local[11].y;"
	"END";

char arb_basic_radial[] =
	"!!ARBvp1.0"
	"TEMP R0;"
	"TEMP R1;"
	"TEMP R2;"
	"DP3 R0.w, vertex.normal, program.local[0];"
	"MAD R0.xyz, program.local[2], -R0.w, program.local[1];"

	"MAX R0.xyz, R0, 0;"
	"MIN R0.xyz, R0, 1;"
	"MOV result.color.xyz, R0;"
	"MOV result.color.w, vertex.color.w;"

	"DP4 R0.x, program.local[3], vertex.position;"
	"DP4 R0.y, program.local[4], vertex.position;"
	"DP4 R0.z, program.local[5], vertex.position;"
	"DP4 R0.w, program.local[6], vertex.position;"

	"DP3 R1.x, R0, R0;"
	"RSQ R1.x, R1.x;"
	"RCP result.fogcoord.x, R1.x;"

	"DP4 result.position.x, program.local[7], R0;"
	"DP4 result.position.y, program.local[8], R0;"
	"DP4 result.position.z, program.local[9], R0;"
	"DP4 result.position.w, program.local[10], R0;"

	"MUL result.texcoord[0].x, vertex.texcoord[0].x, program.local[11].x;"
	"MUL result.texcoord[0].y, vertex.texcoord[0].y, program.local[11].y;"
	"END";

char arb_1light_radial[] =
	"!!ARBvp1.0"
	"TEMP R0;"
	"TEMP R1;"
	"TEMP R2;"
	"DP3 R0.w, vertex.normal, program.local[0];"
	"MAD R0.xyz, program.local[2], -R0.w, program.local[1];"

	"SUB R1.xyz, vertex.position, program.local[12];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[14];"
	"SUB R2.x, 1, program.local[14].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[14].w;"
	"MUL R2.x, program.local[13].w, program.local[13].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[12].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[13], R1.x, R0;"

	"MAX R0.xyz, R0, 0;"
	"MIN R0.xyz, R0, 1;"
	"MOV result.color.xyz, R0;"
	"MOV result.color.w, vertex.color.w;"

	"DP4 R0.x, program.local[3], vertex.position;"
	"DP4 R0.y, program.local[4], vertex.position;"
	"DP4 R0.z, program.local[5], vertex.position;"
	"DP4 R0.w, program.local[6], vertex.position;"

	"DP3 R1.x, R0, R0;"
	"RSQ R1.x, R1.x;"
	"RCP result.fogcoord.x, R1.x;"

	"DP4 result.position.x, program.local[7], R0;"
	"DP4 result.position.y, program.local[8], R0;"
	"DP4 result.position.z, program.local[9], R0;"
	"DP4 result.position.w, program.local[10], R0;"

	"MUL result.texcoord[0].x, vertex.texcoord[0].x, program.local[11].x;"
	"MUL result.texcoord[0].y, vertex.texcoord[0].y, program.local[11].y;"
	"END";

char arb_2lights_radial[] =
	"!!ARBvp1.0"
	"TEMP R0;"
	"TEMP R1;"
	"TEMP R2;"
	"DP3 R0.w, vertex.normal, program.local[0];"
	"MAD R0.xyz, program.local[2], -R0.w, program.local[1];"

	// Light 1
	"SUB R1.xyz, vertex.position, program.local[12];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[14];"
	"SUB R2.x, 1, program.local[14].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[14].w;"
	"MUL R2.x, program.local[13].w, program.local[13].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[12].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[13], R1.x, R0;"

	// Light 2
	"SUB R1.xyz, vertex.position, program.local[15];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[17];"
	"SUB R2.x, 1, program.local[17].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[17].w;"
	"MUL R2.x, program.local[16].w, program.local[16].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[15].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[16], R1.x, R0;"

	"MAX R0.xyz, R0, 0;"
	"MIN R0.xyz, R0, 1;"
	"MOV result.color.xyz, R0;"
	"MOV result.color.w, vertex.color.w;"

	"DP4 R0.x, program.local[3], vertex.position;"
	"DP4 R0.y, program.local[4], vertex.position;"
	"DP4 R0.z, program.local[5], vertex.position;"
	"DP4 R0.w, program.local[6], vertex.position;"

	"DP3 R1.x, R0, R0;"
	"RSQ R1.x, R1.x;"
	"RCP result.fogcoord.x, R1.x;"

	"DP4 result.position.x, program.local[7], R0;"
	"DP4 result.position.y, program.local[8], R0;"
	"DP4 result.position.z, program.local[9], R0;"
	"DP4 result.position.w, program.local[10], R0;"

	"MUL result.texcoord[0].x, vertex.texcoord[0].x, program.local[11].x;"
	"MUL result.texcoord[0].y, vertex.texcoord[0].y, program.local[11].y;"
	"END";

char arb_3lights_radial[] =
	"!!ARBvp1.0"
	"TEMP R0;"
	"TEMP R1;"
	"TEMP R2;"
	"DP3 R0.w, vertex.normal, program.local[0];"
	"MAD R0.xyz, program.local[2], -R0.w, program.local[1];"

	// Light 1
	"SUB R1.xyz, vertex.position, program.local[12];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[14];"
	"SUB R2.x, 1, program.local[14].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[14].w;"
	"MUL R2.x, program.local[13].w, program.local[13].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[12].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[13], R1.x, R0;"

	// Light 2
	"SUB R1.xyz, vertex.position, program.local[15];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[17];"
	"SUB R2.x, 1, program.local[17].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[17].w;"
	"MUL R2.x, program.local[16].w, program.local[16].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[15].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[16], R1.x, R0;"

	// Light 3
	"SUB R1.xyz, vertex.position, program.local[18];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[20];"
	"SUB R2.x, 1, program.local[20].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[20].w;"
	"MUL R2.x, program.local[19].w, program.local[19].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[18].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[19], R1.x, R0;"

	"MAX R0.xyz, R0, 0;"
	"MIN R0.xyz, R0, 1;"
	"MOV result.color.xyz, R0;"
	"MOV result.color.w, vertex.color.w;"

	"DP4 R0.x, program.local[3], vertex.position;"
	"DP4 R0.y, program.local[4], vertex.position;"
	"DP4 R0.z, program.local[5], vertex.position;"
	"DP4 R0.w, program.local[6], vertex.position;"

	"DP3 R1.x, R0, R0;"
	"RSQ R1.x, R1.x;"
	"RCP result.fogcoord.x, R1.x;"

	"DP4 result.position.x, program.local[7], R0;"
	"DP4 result.position.y, program.local[8], R0;"
	"DP4 result.position.z, program.local[9], R0;"
	"DP4 result.position.w, program.local[10], R0;"

	"MUL result.texcoord[0].x, vertex.texcoord[0].x, program.local[11].x;"
	"MUL result.texcoord[0].y, vertex.texcoord[0].y, program.local[11].y;"
	"END";

char arb_4lights_radial[] =
	"!!ARBvp1.0"
	"TEMP R0;"
	"TEMP R1;"
	"TEMP R2;"
	"DP3 R0.w, vertex.normal, program.local[0];"
	"MAD R0.xyz, program.local[2], -R0.w, program.local[1];"

	// Light 1
	"SUB R1.xyz, vertex.position, program.local[12];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[14];"
	"SUB R2.x, 1, program.local[14].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[14].w;"
	"MUL R2.x, program.local[13].w, program.local[13].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[12].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[13], R1.x, R0;"

	// Light 2
	"SUB R1.xyz, vertex.position, program.local[15];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[17];"
	"SUB R2.x, 1, program.local[17].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[17].w;"
	"MUL R2.x, program.local[16].w, program.local[16].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[15].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[16], R1.x, R0;"

	// Light 3
	"SUB R1.xyz, vertex.position, program.local[18];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[20];"
	"SUB R2.x, 1, program.local[20].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[20].w;"
	"MUL R2.x, program.local[19].w, program.local[19].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[18].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[19], R1.x, R0;"

	// Light 4
	"SUB R1.xyz, vertex.position, program.local[21];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[23];"
	"SUB R2.x, 1, program.local[23].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[23].w;"
	"MUL R2.x, program.local[22].w, program.local[22].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[21].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[22], R1.x, R0;"

	"MAX R0.xyz, R0, 0;"
	"MIN R0.xyz, R0, 1;"
	"MOV result.color.xyz, R0;"
	"MOV result.color.w, vertex.color.w;"

	"DP4 R0.x, program.local[3], vertex.position;"
	"DP4 R0.y, program.local[4], vertex.position;"
	"DP4 R0.z, program.local[5], vertex.position;"
	"DP4 R0.w, program.local[6], vertex.position;"

	"DP3 R1.x, R0, R0;"
	"RSQ R1.x, R1.x;"
	"RCP result.fogcoord.x, R1.x;"

	"DP4 result.position.x, program.local[7], R0;"
	"DP4 result.position.y, program.local[8], R0;"
	"DP4 result.position.z, program.local[9], R0;"
	"DP4 result.position.w, program.local[10], R0;"

	"MUL result.texcoord[0].x, vertex.texcoord[0].x, program.local[11].x;"
	"MUL result.texcoord[0].y, vertex.texcoord[0].y, program.local[11].y;"
	"END";

char arb_5lights_radial[] =
	"!!ARBvp1.0"
	"TEMP R0;"
	"TEMP R1;"
	"TEMP R2;"
	"DP3 R0.w, vertex.normal, program.local[0];"
	"MAD R0.xyz, program.local[2], -R0.w, program.local[1];"

	// Light 1
	"SUB R1.xyz, vertex.position, program.local[12];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[14];"
	"SUB R2.x, 1, program.local[14].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[14].w;"
	"MUL R2.x, program.local[13].w, program.local[13].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[12].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[13], R1.x, R0;"

	// Light 2
	"SUB R1.xyz, vertex.position, program.local[15];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[17];"
	"SUB R2.x, 1, program.local[17].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[17].w;"
	"MUL R2.x, program.local[16].w, program.local[16].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[15].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[16], R1.x, R0;"

	// Light 3
	"SUB R1.xyz, vertex.position, program.local[18];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[20];"
	"SUB R2.x, 1, program.local[20].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[20].w;"
	"MUL R2.x, program.local[19].w, program.local[19].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[18].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[19], R1.x, R0;"

	// Light 4
	"SUB R1.xyz, vertex.position, program.local[21];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[23];"
	"SUB R2.x, 1, program.local[23].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[23].w;"
	"MUL R2.x, program.local[22].w, program.local[22].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[21].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[22], R1.x, R0;"

	// Light 5
	"SUB R1.xyz, vertex.position, program.local[24];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[26];"
	"SUB R2.x, 1, program.local[26].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[26].w;"
	"MUL R2.x, program.local[25].w, program.local[25].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[24].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[25], R1.x, R0;"

	"MAX R0.xyz, R0, 0;"
	"MIN R0.xyz, R0, 1;"
	"MOV result.color.xyz, R0;"
	"MOV result.color.w, vertex.color.w;"

	"DP4 R0.x, program.local[3], vertex.position;"
	"DP4 R0.y, program.local[4], vertex.position;"
	"DP4 R0.z, program.local[5], vertex.position;"
	"DP4 R0.w, program.local[6], vertex.position;"

	"DP3 R1.x, R0, R0;"
	"RSQ R1.x, R1.x;"
	"RCP result.fogcoord.x, R1.x;"

	"DP4 result.position.x, program.local[7], R0;"
	"DP4 result.position.y, program.local[8], R0;"
	"DP4 result.position.z, program.local[9], R0;"
	"DP4 result.position.w, program.local[10], R0;"

	"MUL result.texcoord[0].x, vertex.texcoord[0].x, program.local[11].x;"
	"MUL result.texcoord[0].y, vertex.texcoord[0].y, program.local[11].y;"
	"END";

char arb_6lights_radial[] =
	"!!ARBvp1.0"
	"TEMP R0;"
	"TEMP R1;"
	"TEMP R2;"
	"DP3 R0.w, vertex.normal, program.local[0];"
	"MAD R0.xyz, program.local[2], -R0.w, program.local[1];"

	// Light 1
	"SUB R1.xyz, vertex.position, program.local[12];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[14];"
	"SUB R2.x, 1, program.local[14].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[14].w;"
	"MUL R2.x, program.local[13].w, program.local[13].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[12].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[13], R1.x, R0;"

	// Light 2
	"SUB R1.xyz, vertex.position, program.local[15];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[17];"
	"SUB R2.x, 1, program.local[17].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[17].w;"
	"MUL R2.x, program.local[16].w, program.local[16].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[15].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[16], R1.x, R0;"

	// Light 3
	"SUB R1.xyz, vertex.position, program.local[18];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[20];"
	"SUB R2.x, 1, program.local[20].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[20].w;"
	"MUL R2.x, program.local[19].w, program.local[19].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[18].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[19], R1.x, R0;"

	// Light 4
	"SUB R1.xyz, vertex.position, program.local[21];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[23];"
	"SUB R2.x, 1, program.local[23].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[23].w;"
	"MUL R2.x, program.local[22].w, program.local[22].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[21].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[22], R1.x, R0;"

	// Light 5
	"SUB R1.xyz, vertex.position, program.local[24];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[26];"
	"SUB R2.x, 1, program.local[26].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[26].w;"
	"MUL R2.x, program.local[25].w, program.local[25].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[24].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[25], R1.x, R0;"

	// Light 5
	"SUB R1.xyz, vertex.position, program.local[27];"
	"DP3 R1.w, R1, R1;"
	"RSQ R2.x, R1.w;"
	"MUL R1.xyz, R2.x, R1;"
	"DP3 R2.y, R1, program.local[29];"
	"SUB R2.x, 1, program.local[29].w;"
	"RCP R2.z, R2.x;"
	"MAX R2.y, R2, 0;"
	"SUB R2.y, R2, program.local[29].w;"
	"MUL R2.x, program.local[28].w, program.local[28].w;"
	"RCP R2.x, R2.x;"
	"MAD R1.w, R1, R2.x, -1;"
	"MUL R2.y, R2, R2.z;"
	"MIN R2.y, R2, 1;"
	"MAX R2.x, R2.y, 0;"
	"MAX R1.w, -R1, 0;"
	"ADD R2.x, -R2, 1;"
	"MUL R2.x, R2, R1.w;"
	"MAD R1.w, -R2.x, program.local[27].w, R1;"
	"DP3 R1.x, vertex.normal, R1;"
	"MUL R1.x, -R1, R1.w;"
	"MAX R1.x, R1, 0;"
	"MAD R0.xyz, program.local[28], R1.x, R0;"

	"MAX R0.xyz, R0, 0;"
	"MIN R0.xyz, R0, 1;"
	"MOV result.color.xyz, R0;"
	"MOV result.color.w, vertex.color.w;"

	"DP4 R0.x, program.local[3], vertex.position;"
	"DP4 R0.y, program.local[4], vertex.position;"
	"DP4 R0.z, program.local[5], vertex.position;"
	"DP4 R0.w, program.local[6], vertex.position;"

	"DP3 R1.x, R0, R0;"
	"RSQ R1.x, R1.x;"
	"RCP result.fogcoord.x, R1.x;"

	"DP4 result.position.x, program.local[7], R0;"
	"DP4 result.position.y, program.local[8], R0;"
	"DP4 result.position.z, program.local[9], R0;"
	"DP4 result.position.w, program.local[10], R0;"

	"MUL result.texcoord[0].x, vertex.texcoord[0].x, program.local[11].x;"
	"MUL result.texcoord[0].y, vertex.texcoord[0].y, program.local[11].y;"
	"END";

char arb_frag_reg[] =
	"!!ARBfp1.0"
	"OPTION ARB_precision_hint_fastest;"
	"TEMP R0;"
	"TEX R0, fragment.texcoord[0], texture[0], 2D;"
	"MUL R0.xyz, R0, 2;"

	"MUL result.color, R0, fragment.color.primary;"
	"END";

char arb_frag_fog[] =
	"!!ARBfp1.0"
	"OPTION ARB_precision_hint_fastest;"
	"OPTION ARB_fog_linear;"
	"TEMP R0;"
	"TEX R0, fragment.texcoord[0], texture[0], 2D;"
	"MUL R0.xyz, R0, 2;"

	"MUL result.color, R0, fragment.color.primary;"
	"END";
//===========================================
//	ARB SHADER
//===========================================

/*
====================
Init

====================
*/
void CStudioModelRenderer::Init()
{
	// Set up some variables shared with engine
	m_pCvarHiModels = IEngineStudio.GetCvar("cl_himodels");
	m_pCvarDeveloper = IEngineStudio.GetCvar("developer");
	m_pCvarDrawEntities = IEngineStudio.GetCvar("r_drawentities");

	m_pCvarSkyColorX = IEngineStudio.GetCvar("sv_skycolor_r");
	m_pCvarSkyColorY = IEngineStudio.GetCvar("sv_skycolor_g");
	m_pCvarSkyColorZ = IEngineStudio.GetCvar("sv_skycolor_b");

	m_pCvarSkyVecX = IEngineStudio.GetCvar("sv_skyvec_x");
	m_pCvarSkyVecY = IEngineStudio.GetCvar("sv_skyvec_y");
	m_pCvarSkyVecZ = IEngineStudio.GetCvar("sv_skyvec_z");

	m_pCvarGlowShellFreq = IEngineStudio.GetCvar("r_glowshellfreq");

	m_pCvarDrawModels = gEngfuncs.pfnRegisterVariable("te_models", "1", 0);
	m_pCvarModelShaders = gEngfuncs.pfnRegisterVariable("te_model_shaders", "1", 0);
	m_pCvarModelDecals = gEngfuncs.pfnRegisterVariable("te_model_decals", "1", 0);

	m_pCvarModelsBBoxDebug = gEngfuncs.pfnRegisterVariable("te_models_debug_bbox", "0", 0);
	m_pCvarModelsLightDebug = gEngfuncs.pfnRegisterVariable("te_models_debug_light", "0", 0);

	IEngineStudio.GetModelCounters(&m_pStudioModelCount, &m_pModelsDrawn);

	// Get pointers to engine data structures
	m_pbonetransform = (float(*)[MAXSTUDIOBONES][3][4])IEngineStudio.StudioGetBoneTransform();
	m_plighttransform = (float(*)[MAXSTUDIOBONES][3][4])IEngineStudio.StudioGetLightTransform();
	m_paliastransform = (float(*)[3][4])IEngineStudio.StudioGetAliasTransform();
	m_protationmatrix = (float(*)[3][4])IEngineStudio.StudioGetRotationMatrix();
	m_pChromeSprite = IEngineStudio.GetChromeSprite();

	m_pCvarDrawShadows = CVAR_CREATE("gl_shadows", "2", FCVAR_ARCHIVE);

	m_pCvarRenderDistance = CVAR_CREATE("te_render_distance", "2000", FCVAR_ARCHIVE);

	//
	// Load ARB shaders
	//

	// Don't bother if support is already gone
	if (!gBSPRenderer.m_bShaderSupport)
		return;

	// Load non-fog fragment shader
	GLint iErrorPos, iIsNative;
	glEnable(GL_FRAGMENT_PROGRAM_ARB);
	gBSPRenderer.glGenProgramsARB(1, &m_uiFragmentShaders[0]);
	gBSPRenderer.glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, m_uiFragmentShaders[0]);

	gBSPRenderer.glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, sizeof(arb_frag_reg) - 1, arb_frag_reg);
	glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &iErrorPos);
	gBSPRenderer.glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB, &iIsNative);
	glDisable(GL_FRAGMENT_PROGRAM_ARB);

	if (iErrorPos != -1 || (iIsNative == 0))
	{
		gBSPRenderer.m_bShaderSupport = false;
		gBSPRenderer.m_bDontPromptShadersError = false;
		return;
	}

	// Load fogged fragment shader
	glEnable(GL_FRAGMENT_PROGRAM_ARB);
	gBSPRenderer.glGenProgramsARB(1, &m_uiFragmentShaders[1]);
	gBSPRenderer.glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, m_uiFragmentShaders[1]);

	gBSPRenderer.glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, sizeof(arb_frag_fog) - 1, arb_frag_fog);
	glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &iErrorPos);
	gBSPRenderer.glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB, &iIsNative);
	glDisable(GL_FRAGMENT_PROGRAM_ARB);

	if (iErrorPos != -1 || (iIsNative == 0))
	{
		gBSPRenderer.m_bShaderSupport = false;
		gBSPRenderer.m_bDontPromptShadersError = false;
		return;
	}

	glEnable(GL_VERTEX_PROGRAM_ARB);
	gBSPRenderer.glGenProgramsARB(1, &m_uiVertexShaders[0]); // No light depth
	gBSPRenderer.glBindProgramARB(GL_VERTEX_PROGRAM_ARB, m_uiVertexShaders[0]);

	gBSPRenderer.glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, sizeof(arb_basic_depth) - 1, arb_basic_depth);
	glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &iErrorPos);
	gBSPRenderer.glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB, &iIsNative);
	glDisable(GL_VERTEX_PROGRAM_ARB);

	if (iErrorPos != -1 || (iIsNative == 0))
	{
		gBSPRenderer.m_bShaderSupport = false;
		gBSPRenderer.m_bDontPromptShadersError = false;
		return;
	}
	glEnable(GL_VERTEX_PROGRAM_ARB);
	gBSPRenderer.glGenProgramsARB(1, &m_uiVertexShaders[1]); // 1 light depth
	gBSPRenderer.glBindProgramARB(GL_VERTEX_PROGRAM_ARB, m_uiVertexShaders[1]);

	gBSPRenderer.glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, sizeof(arb_1light_depth) - 1, arb_1light_depth);
	glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &iErrorPos);
	gBSPRenderer.glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB, &iIsNative);
	glDisable(GL_VERTEX_PROGRAM_ARB);

	if (iErrorPos != -1 || (iIsNative == 0))
	{
		gBSPRenderer.m_bShaderSupport = false;
		gBSPRenderer.m_bDontPromptShadersError = false;
		return;
	}
	glEnable(GL_VERTEX_PROGRAM_ARB);
	gBSPRenderer.glGenProgramsARB(1, &m_uiVertexShaders[2]); // 2 lights depth
	gBSPRenderer.glBindProgramARB(GL_VERTEX_PROGRAM_ARB, m_uiVertexShaders[2]);

	gBSPRenderer.glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, sizeof(arb_2lights_depth) - 1, arb_2lights_depth);
	glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &iErrorPos);
	gBSPRenderer.glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB, &iIsNative);
	glDisable(GL_VERTEX_PROGRAM_ARB);

	if (iErrorPos != -1 || (iIsNative == 0))
	{
		gBSPRenderer.m_bShaderSupport = false;
		gBSPRenderer.m_bDontPromptShadersError = false;
		return;
	}
	glEnable(GL_VERTEX_PROGRAM_ARB);
	gBSPRenderer.glGenProgramsARB(1, &m_uiVertexShaders[3]); // 3 lights depth
	gBSPRenderer.glBindProgramARB(GL_VERTEX_PROGRAM_ARB, m_uiVertexShaders[3]);

	gBSPRenderer.glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, sizeof(arb_3lights_depth) - 1, arb_3lights_depth);
	glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &iErrorPos);
	gBSPRenderer.glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB, &iIsNative);
	glDisable(GL_VERTEX_PROGRAM_ARB);

	if (iErrorPos != -1 || (iIsNative == 0))
	{
		gBSPRenderer.m_bShaderSupport = false;
		gBSPRenderer.m_bDontPromptShadersError = false;
		return;
	}
	glEnable(GL_VERTEX_PROGRAM_ARB);
	gBSPRenderer.glGenProgramsARB(1, &m_uiVertexShaders[4]); // 4 lights depth
	gBSPRenderer.glBindProgramARB(GL_VERTEX_PROGRAM_ARB, m_uiVertexShaders[4]);

	gBSPRenderer.glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, sizeof(arb_4lights_depth) - 1, arb_4lights_depth);
	glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &iErrorPos);
	gBSPRenderer.glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB, &iIsNative);
	glDisable(GL_VERTEX_PROGRAM_ARB);

	if (iErrorPos != -1 || (iIsNative == 0))
	{
		gBSPRenderer.m_bShaderSupport = false;
		gBSPRenderer.m_bDontPromptShadersError = false;
		return;
	}
	glEnable(GL_VERTEX_PROGRAM_ARB);
	gBSPRenderer.glGenProgramsARB(1, &m_uiVertexShaders[5]); // 5 lights depth
	gBSPRenderer.glBindProgramARB(GL_VERTEX_PROGRAM_ARB, m_uiVertexShaders[5]);

	gBSPRenderer.glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, sizeof(arb_5lights_depth) - 1, arb_5lights_depth);
	glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &iErrorPos);
	gBSPRenderer.glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB, &iIsNative);
	glDisable(GL_VERTEX_PROGRAM_ARB);

	if (iErrorPos != -1 || (iIsNative == 0))
	{
		gBSPRenderer.m_bShaderSupport = false;
		gBSPRenderer.m_bDontPromptShadersError = false;
		return;
	}
	glEnable(GL_VERTEX_PROGRAM_ARB);
	gBSPRenderer.glGenProgramsARB(1, &m_uiVertexShaders[6]); // 6 lights depth
	gBSPRenderer.glBindProgramARB(GL_VERTEX_PROGRAM_ARB, m_uiVertexShaders[6]);

	gBSPRenderer.glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, sizeof(arb_6lights_depth) - 1, arb_6lights_depth);
	glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &iErrorPos);
	gBSPRenderer.glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB, &iIsNative);
	glDisable(GL_VERTEX_PROGRAM_ARB);

	if (iErrorPos != -1 || (iIsNative == 0))
	{
		gBSPRenderer.m_bShaderSupport = false;
		gBSPRenderer.m_bDontPromptShadersError = false;
		return;
	}
	glEnable(GL_VERTEX_PROGRAM_ARB);
	gBSPRenderer.glGenProgramsARB(1, &m_uiVertexShaders[7]);
	gBSPRenderer.glBindProgramARB(GL_VERTEX_PROGRAM_ARB, m_uiVertexShaders[7]);

	gBSPRenderer.glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, sizeof(arb_basic_radial) - 1, arb_basic_radial);
	glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &iErrorPos);
	gBSPRenderer.glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB, &iIsNative);
	glDisable(GL_VERTEX_PROGRAM_ARB);

	if (iErrorPos != -1 || (iIsNative == 0))
	{
		gBSPRenderer.m_bShaderSupport = false;
		gBSPRenderer.m_bDontPromptShadersError = false;
		return;
	}
	glEnable(GL_VERTEX_PROGRAM_ARB);
	gBSPRenderer.glGenProgramsARB(1, &m_uiVertexShaders[8]); // 1 light radial
	gBSPRenderer.glBindProgramARB(GL_VERTEX_PROGRAM_ARB, m_uiVertexShaders[8]);

	gBSPRenderer.glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, sizeof(arb_1light_radial) - 1, arb_1light_radial);
	glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &iErrorPos);
	gBSPRenderer.glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB, &iIsNative);
	glDisable(GL_VERTEX_PROGRAM_ARB);

	if (iErrorPos != -1 || (iIsNative == 0))
	{
		gBSPRenderer.m_bShaderSupport = false;
		gBSPRenderer.m_bDontPromptShadersError = false;
		return;
	}
	glEnable(GL_VERTEX_PROGRAM_ARB);
	gBSPRenderer.glGenProgramsARB(1, &m_uiVertexShaders[9]); // 2 lights radial
	gBSPRenderer.glBindProgramARB(GL_VERTEX_PROGRAM_ARB, m_uiVertexShaders[9]);

	gBSPRenderer.glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, sizeof(arb_2lights_radial) - 1, arb_2lights_radial);
	glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &iErrorPos);
	gBSPRenderer.glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB, &iIsNative);
	glDisable(GL_VERTEX_PROGRAM_ARB);

	if (iErrorPos != -1 || (iIsNative == 0))
	{
		gBSPRenderer.m_bShaderSupport = false;
		gBSPRenderer.m_bDontPromptShadersError = false;
		return;
	}
	glEnable(GL_VERTEX_PROGRAM_ARB);
	gBSPRenderer.glGenProgramsARB(1, &m_uiVertexShaders[10]); // 3 lights radial
	gBSPRenderer.glBindProgramARB(GL_VERTEX_PROGRAM_ARB, m_uiVertexShaders[10]);

	gBSPRenderer.glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, sizeof(arb_3lights_radial) - 1, arb_3lights_radial);
	glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &iErrorPos);
	gBSPRenderer.glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB, &iIsNative);
	glDisable(GL_VERTEX_PROGRAM_ARB);

	if (iErrorPos != -1 || (iIsNative == 0))
	{
		gBSPRenderer.m_bShaderSupport = false;
		gBSPRenderer.m_bDontPromptShadersError = false;
		return;
	}
	glEnable(GL_VERTEX_PROGRAM_ARB);
	gBSPRenderer.glGenProgramsARB(1, &m_uiVertexShaders[11]); // 4 lights radial
	gBSPRenderer.glBindProgramARB(GL_VERTEX_PROGRAM_ARB, m_uiVertexShaders[11]);

	gBSPRenderer.glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, sizeof(arb_4lights_radial) - 1, arb_4lights_radial);
	glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &iErrorPos);
	gBSPRenderer.glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB, &iIsNative);
	glDisable(GL_VERTEX_PROGRAM_ARB);
	if (iErrorPos != -1 || (iIsNative == 0))
	{
		gBSPRenderer.m_bShaderSupport = false;
		gBSPRenderer.m_bDontPromptShadersError = false;
		return;
	}
	glEnable(GL_VERTEX_PROGRAM_ARB);
	gBSPRenderer.glGenProgramsARB(1, &m_uiVertexShaders[12]); // 5 lights radial
	gBSPRenderer.glBindProgramARB(GL_VERTEX_PROGRAM_ARB, m_uiVertexShaders[12]);

	gBSPRenderer.glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, sizeof(arb_5lights_radial) - 1, arb_5lights_radial);
	glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &iErrorPos);
	gBSPRenderer.glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB, &iIsNative);

	if (iErrorPos != -1 || (iIsNative == 0))
	{
		gBSPRenderer.m_bShaderSupport = false;
		gBSPRenderer.m_bDontPromptShadersError = false;
		return;
	}
	glEnable(GL_VERTEX_PROGRAM_ARB);
	gBSPRenderer.glGenProgramsARB(1, &m_uiVertexShaders[13]); // 6 lights radial
	gBSPRenderer.glBindProgramARB(GL_VERTEX_PROGRAM_ARB, m_uiVertexShaders[13]);

	gBSPRenderer.glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, sizeof(arb_6lights_radial) - 1, arb_6lights_radial);
	glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &iErrorPos);
	gBSPRenderer.glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB, &iIsNative);
	glDisable(GL_VERTEX_PROGRAM_ARB);

	if (iErrorPos != -1 || (iIsNative == 0))
	{
		gBSPRenderer.m_bShaderSupport = false;
		gBSPRenderer.m_bDontPromptShadersError = false;
		return;
	}
}

/*
====================
VidInit

====================
*/
void CStudioModelRenderer::VidInit()
{
	StoredLightBuffer.clear();

	int iCurrentBinding;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &iCurrentBinding);

	if (m_iNumStudioDecals != 0)
	{
		for (int i = 0; i < m_iNumStudioDecals; i++)
		{
			if (m_pStudioDecals[i].numpolys != 0)
			{
				for (int j = 0; j < m_pStudioDecals[i].numpolys; j++)
					delete[] m_pStudioDecals[i].polys[j].verts;

				delete[] m_pStudioDecals[i].polys;
			}

			if (m_pStudioDecals[i].numverts != 0)
				delete[] m_pStudioDecals[i].verts;
		}

		memset(m_pStudioDecals, NULL, sizeof(m_pStudioDecals));
		m_iNumStudioDecals = NULL;
	}

	if (m_iNumExtraInfo != 0)
	{
		memset(m_pExtraInfo, NULL, sizeof(m_pExtraInfo));
		m_iNumExtraInfo = NULL;
	}

	// The texture cache is cleared at vidinit, reload textures
	char szPath[64];
	std::string strModel;
	std::string strTexture;
	for (int i = 0; i < m_iNumStudioModels; i++)
	{
		strModel = FilenameFromPath(m_pStudioModels[i].name);
		FranUtils::StringUtils::LowerCase_Ref(strModel);

		studiohdr_t* pHeader = (studiohdr_t*)m_pStudioModels[i].cache.data;
		mstudiotexture_t* ptextures = (mstudiotexture_t*)((byte*)pHeader + pHeader->textureindex);

		for (int j = 0; j < pHeader->numtextures; j++)
		{
			strTexture = FilenameFromPath(ptextures[j].name);
			FranUtils::StringUtils::LowerCase_Ref(strTexture);

			if (gTextureLoader.TextureHasFlag(strModel, strTexture, TEXFLAG_ALTERNATE))
			{
				sprintf(szPath, "gfx/textures/models/%s/%s.dds", strModel.c_str(), strTexture.c_str());
				gTextureLoader.LoadTexture(szPath, ptextures[j].index, true, (ptextures[j].flags & STUDIO_NF_NOMIPMAP ? 1 : 0) != 0);
			}
		}
	}

	glBindTexture(GL_TEXTURE_2D, iCurrentBinding);
}

/*
====================
CStudioModelRenderer

====================
*/
CStudioModelRenderer::CStudioModelRenderer()
{
	m_fDoInterp = 1;
	m_fGaitEstimation = 1;
	m_pCurrentEntity = nullptr;
	m_pCvarHiModels = nullptr;
	m_pCvarDeveloper = nullptr;
	m_pCvarDrawEntities = nullptr;
	m_pChromeSprite = nullptr;
	m_pStudioModelCount = nullptr;
	m_pModelsDrawn = nullptr;
	m_protationmatrix = nullptr;
	m_paliastransform = nullptr;
	m_pbonetransform = nullptr;
	m_plighttransform = nullptr;
	m_pStudioHeader = nullptr;
	m_pBodyPart = nullptr;
	m_pSubModel = nullptr;
	m_pPlayerInfo = nullptr;
	m_pRenderModel = nullptr;
}

/*
====================
~CStudioModelRenderer

====================
*/
CStudioModelRenderer::~CStudioModelRenderer()
{
	if (m_iNumStudioModels > 0)
	{
		for (int i = 0; i < m_iNumStudioModels; i++)
			delete m_pStudioModels[i].cache.data;
	}

	if (m_iNumStudioDecals != 0)
	{
		for (int i = 0; i < m_iNumStudioDecals; i++)
		{
			if (m_pStudioDecals[i].numpolys != 0)
			{
				for (int j = 0; j < m_pStudioDecals[i].numpolys; j++)
					delete[] m_pStudioDecals[i].polys[j].verts;

				delete[] m_pStudioDecals[i].polys;
			}

			if (m_pStudioDecals[i].numverts != 0)
				delete[] m_pStudioDecals[i].verts;
		}

		memset(m_pStudioDecals, NULL, sizeof(m_pStudioDecals));
		m_iNumStudioDecals = NULL;
	}
}

/*
====================
StudioCalcBoneAdj

====================
*/
void CStudioModelRenderer::StudioCalcBoneAdj(float dadt, float* adj, const byte* pcontroller1, const byte* pcontroller2, byte mouthopen)
{
	int i, j;
	float value;
	mstudiobonecontroller_t* pbonecontroller;

	pbonecontroller = (mstudiobonecontroller_t*)((byte*)m_pStudioHeader + m_pStudioHeader->bonecontrollerindex);

	for (j = 0; j < m_pStudioHeader->numbonecontrollers; j++)
	{
		i = pbonecontroller[j].index;
		if (i <= 3)
		{
			// check for 360% wrapping
			if ((pbonecontroller[j].type & STUDIO_RLOOP) != 0)
			{
				if (abs(pcontroller1[i] - pcontroller2[i]) > 128)
				{
					int a, b;
					a = (pcontroller1[j] + 128) % 256;
					b = (pcontroller2[j] + 128) % 256;
					value = ((a * dadt) + (b * (1 - dadt)) - 128) * (360.0 / 256.0) + pbonecontroller[j].start;
				}
				else
				{
					value = ((pcontroller1[i] * dadt + (pcontroller2[i]) * (1.0 - dadt))) * (360.0 / 256.0) + pbonecontroller[j].start;
				}
			}
			else
			{
				value = (pcontroller1[i] * dadt + pcontroller2[i] * (1.0 - dadt)) / 255.0;
				if (value < 0)
					value = 0;
				if (value > 1.0)
					value = 1.0;
				value = (1.0 - value) * pbonecontroller[j].start + value * pbonecontroller[j].end;
			}
			// Con_DPrintf( "%d %d %f : %f\n", m_pCurrentEntity->curstate.controller[j], m_pCurrentEntity->latched.prevcontroller[j], value, dadt );
		}
		else
		{
			value = mouthopen / 64.0;
			if (value > 1.0)
				value = 1.0;
			value = (1.0 - value) * pbonecontroller[j].start + value * pbonecontroller[j].end;
			// Con_DPrintf("%d %f\n", mouthopen, value );
		}
		switch (pbonecontroller[j].type & STUDIO_TYPES)
		{
		case STUDIO_XR:
		case STUDIO_YR:
		case STUDIO_ZR:
			adj[j] = value * (M_PI / 180.0);
			break;
		case STUDIO_X:
		case STUDIO_Y:
		case STUDIO_Z:
			adj[j] = value;
			break;
		}
	}
}


/*
====================
StudioCalcBoneQuaterion

====================
*/
void CStudioModelRenderer::StudioCalcBoneQuaterion(int frame, float s, mstudiobone_t* pbone, mstudioanim_t* panim, float* adj, float* q)
{
	int j, k;
	vec4_t q1, q2;
	Vector angle1, angle2;
	mstudioanimvalue_t* panimvalue;

	for (j = 0; j < 3; j++)
	{
		if (panim->offset[j + 3] == 0)
		{
			angle2[j] = angle1[j] = pbone->value[j + 3]; // default;
		}
		else
		{
			panimvalue = (mstudioanimvalue_t*)((byte*)panim + panim->offset[j + 3]);
			k = frame;
			// DEBUG
			if (panimvalue->num.total < panimvalue->num.valid)
				k = 0;
			while (panimvalue->num.total <= k)
			{
				k -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;
				// DEBUG
				if (panimvalue->num.total < panimvalue->num.valid)
					k = 0;
			}
			// Bah, missing blend!
			if (panimvalue->num.valid > k)
			{
				angle1[j] = panimvalue[k + 1].value;

				if (panimvalue->num.valid > k + 1)
				{
					angle2[j] = panimvalue[k + 2].value;
				}
				else
				{
					if (panimvalue->num.total > k + 1)
						angle2[j] = angle1[j];
					else
						angle2[j] = panimvalue[panimvalue->num.valid + 2].value;
				}
			}
			else
			{
				angle1[j] = panimvalue[panimvalue->num.valid].value;
				if (panimvalue->num.total > k + 1)
				{
					angle2[j] = angle1[j];
				}
				else
				{
					angle2[j] = panimvalue[panimvalue->num.valid + 2].value;
				}
			}
			angle1[j] = pbone->value[j + 3] + angle1[j] * pbone->scale[j + 3];
			angle2[j] = pbone->value[j + 3] + angle2[j] * pbone->scale[j + 3];
		}

		if (pbone->bonecontroller[j + 3] != -1)
		{
			angle1[j] += adj[pbone->bonecontroller[j + 3]];
			angle2[j] += adj[pbone->bonecontroller[j + 3]];
		}
	}

	if (!VectorCompare(angle1, angle2))
	{
		AngleQuaternion(angle1, q1);
		AngleQuaternion(angle2, q2);
		QuaternionSlerp(q1, q2, s, q);
	}
	else
	{
		AngleQuaternion(angle1, q);
	}
}

/*
====================
StudioCalcBonePosition

====================
*/
void CStudioModelRenderer::StudioCalcBonePosition(int frame, float s, mstudiobone_t* pbone, mstudioanim_t* panim, float* adj, float* pos)
{
	int j, k;
	mstudioanimvalue_t* panimvalue;

	for (j = 0; j < 3; j++)
	{
		pos[j] = pbone->value[j]; // default;
		if (panim->offset[j] != 0)
		{
			panimvalue = (mstudioanimvalue_t*)((byte*)panim + panim->offset[j]);
			/*
			if (i == 0 && j == 0)
				Con_DPrintf("%d  %d:%d  %f\n", frame, panimvalue->num.valid, panimvalue->num.total, s );
			*/

			k = frame;
			// DEBUG
			if (panimvalue->num.total < panimvalue->num.valid)
				k = 0;
			// find span of values that includes the frame we want
			while (panimvalue->num.total <= k)
			{
				k -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;
				// DEBUG
				if (panimvalue->num.total < panimvalue->num.valid)
					k = 0;
			}
			// if we're inside the span
			if (panimvalue->num.valid > k)
			{
				// and there's more data in the span
				if (panimvalue->num.valid > k + 1)
				{
					pos[j] += (panimvalue[k + 1].value * (1.0 - s) + s * panimvalue[k + 2].value) * pbone->scale[j];
				}
				else
				{
					pos[j] += panimvalue[k + 1].value * pbone->scale[j];
				}
			}
			else
			{
				// are we at the end of the repeating values section and there's another section with data?
				if (panimvalue->num.total <= k + 1)
				{
					pos[j] += (panimvalue[panimvalue->num.valid].value * (1.0 - s) + s * panimvalue[panimvalue->num.valid + 2].value) * pbone->scale[j];
				}
				else
				{
					pos[j] += panimvalue[panimvalue->num.valid].value * pbone->scale[j];
				}
			}
		}
		if (pbone->bonecontroller[j] != -1 && (adj != nullptr))
		{
			pos[j] += adj[pbone->bonecontroller[j]];
		}
	}
}

/*
====================
StudioSlerpBones

====================
*/
void CStudioModelRenderer::StudioSlerpBones(vec4_t q1[], float pos1[][3], vec4_t q2[], float pos2[][3], float s)
{
	int i;
	vec4_t q3;
	float s1;

	if (s < 0)
		s = 0;
	else if (s > 1.0)
		s = 1.0;

	s1 = 1.0 - s;

	for (i = 0; i < m_pStudioHeader->numbones; i++)
	{
		QuaternionSlerp(q1[i], q2[i], s, q3);
		q1[i][0] = q3[0];
		q1[i][1] = q3[1];
		q1[i][2] = q3[2];
		q1[i][3] = q3[3];
		pos1[i][0] = pos1[i][0] * s1 + pos2[i][0] * s;
		pos1[i][1] = pos1[i][1] * s1 + pos2[i][1] * s;
		pos1[i][2] = pos1[i][2] * s1 + pos2[i][2] * s;
	}
}

/*
====================
StudioGetAnim

====================
*/
mstudioanim_t* CStudioModelRenderer::StudioGetAnim(model_t* m_pSubModel, mstudioseqdesc_t* pseqdesc)
{
	mstudioseqgroup_t* pseqgroup;
	cache_user_t* paSequences;

	pseqgroup = (mstudioseqgroup_t*)((byte*)m_pStudioHeader + m_pStudioHeader->seqgroupindex) + pseqdesc->seqgroup;

	if (pseqdesc->seqgroup == 0)
	{
		return (mstudioanim_t*)((byte*)m_pStudioHeader + pseqgroup->data + pseqdesc->animindex);
	}

	paSequences = (cache_user_t*)m_pSubModel->submodels;

	if (paSequences == nullptr)
	{
		paSequences = (cache_user_t*)IEngineStudio.Mem_Calloc(16, sizeof(cache_user_t)); // UNDONE: leak!
		m_pSubModel->submodels = (dmodel_t*)paSequences;
	}

	if (IEngineStudio.Cache_Check((struct cache_user_s*)&(paSequences[pseqdesc->seqgroup])) == nullptr)
	{
		gEngfuncs.Con_DPrintf("loading %s\n", pseqgroup->name);
		IEngineStudio.LoadCacheFile(pseqgroup->name, (struct cache_user_s*)&paSequences[pseqdesc->seqgroup]);
	}
	return (mstudioanim_t*)((byte*)paSequences[pseqdesc->seqgroup].data + pseqdesc->animindex);
}

/*
====================
StudioPlayerBlend

====================
*/
void CStudioModelRenderer::StudioPlayerBlend(mstudioseqdesc_t* pseqdesc, int* pBlend, float* pPitch)
{
	// calc up/down pointing
	*pBlend = (*pPitch * 3);
	if (*pBlend < pseqdesc->blendstart[0])
	{
		*pPitch -= pseqdesc->blendstart[0] / 3.0;
		*pBlend = 0;
	}
	else if (*pBlend > pseqdesc->blendend[0])
	{
		*pPitch -= pseqdesc->blendend[0] / 3.0;
		*pBlend = 255;
	}
	else
	{
		if (pseqdesc->blendend[0] - pseqdesc->blendstart[0] < 0.1) // catch qc error
			*pBlend = 127;
		else
			*pBlend = 255 * (*pBlend - pseqdesc->blendstart[0]) / (pseqdesc->blendend[0] - pseqdesc->blendstart[0]);
		*pPitch = 0;
	}
}

/*
====================
StudioSetUpTransform

====================
*/
void CStudioModelRenderer::StudioSetUpTransform(int trivial_accept)
{
	int i;
	Vector angles;
	Vector modelpos;


	VectorCopy(m_pCurrentEntity->origin, modelpos);

	// TODO: should really be stored with the entity instead of being reconstructed
	// TODO: should use a look-up table
	// TODO: could cache lazily, stored in the entity
	angles[ROLL] = m_pCurrentEntity->curstate.angles[ROLL];
	angles[PITCH] = m_pCurrentEntity->curstate.angles[PITCH];
	angles[YAW] = m_pCurrentEntity->curstate.angles[YAW];

	// Con_DPrintf("Angles %4.2f prev %4.2f for %i\n", angles[PITCH], m_pCurrentEntity->index);
	// Con_DPrintf("movetype %d %d\n", m_pCurrentEntity->movetype, m_pCurrentEntity->aiment );
	if (m_pCurrentEntity->curstate.movetype == MOVETYPE_STEP)
	{
		float f = 0;
		float d;

		mstudioseqdesc_t* pseqdesc; // acess to studio flags
		pseqdesc = (mstudioseqdesc_t*)((byte*)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->curstate.sequence;

		// don't do it if the goalstarttime hasn't updated in a while.

		// NOTE:  Because we need to interpolate multiplayer characters, the interpolation time limit
		//  was increased to 1.0 s., which is 2x the max lag we are accounting for.

		if ((m_clTime < m_pCurrentEntity->curstate.animtime + 1.0f) &&
			(m_pCurrentEntity->curstate.animtime != m_pCurrentEntity->latched.prevanimtime))
		{
			f = (m_clTime - m_pCurrentEntity->curstate.animtime) / (m_pCurrentEntity->curstate.animtime - m_pCurrentEntity->latched.prevanimtime);
			// Con_DPrintf("%4.2f %.2f %.2f\n", f, m_pCurrentEntity->curstate.animtime, m_clTime);
		}

		if (m_fDoInterp != 0)
		{
			// ugly hack to interpolate angle, position. current is reached 0.1 seconds after being set
			f = f - 1.0;
		}
		else
		{
			f = 0;
		}

		if (((pseqdesc->motiontype & STUDIO_LX) != 0) || ((m_pCurrentEntity->curstate.eflags & EFLAG_SLERP) != 0)) // uncle misha
		{
			for (i = 0; i < 3; i++)
			{
				modelpos[i] += (m_pCurrentEntity->origin[i] - m_pCurrentEntity->latched.prevorigin[i]) * f;
			}
		}

		// NOTE:  Because multiplayer lag can be relatively large, we don't want to cap
		//  f at 1.5 anymore.
		// if (f > -1.0 && f < 1.5) {}

		//			gEngfuncs.Con_DPrintf("%.0f %.0f\n",m_pCurrentEntity->angles[0][YAW], m_pCurrentEntity->angles[1][YAW] );
		for (i = 0; i < 3; i++)
		{
			float ang1, ang2;

			ang1 = m_pCurrentEntity->angles[i];
			ang2 = m_pCurrentEntity->latched.prevangles[i];

			d = ang1 - ang2;
			if (d > 180)
			{
				d -= 360;
			}
			else if (d < -180)
			{
				d += 360;
			}

			angles[i] += d * f;
		}
		// Con_DPrintf("%.3f \n", f );
	}
	else if (m_pCurrentEntity->curstate.movetype != MOVETYPE_NONE)
	{
		VectorCopy(m_pCurrentEntity->angles, angles);
	}

	// Con_DPrintf("%.0f %0.f %0.f\n", modelpos[0], modelpos[1], modelpos[2] );
	//	gEngfuncs.Con_DPrintf("%.0f %0.f %0.f\n", angles[0], angles[1], angles[2] );


	angles[PITCH] = -angles[PITCH];
	AngleMatrix(angles, (*m_protationmatrix));

	(*m_protationmatrix)[0][3] = modelpos[0];
	(*m_protationmatrix)[1][3] = modelpos[1];
	(*m_protationmatrix)[2][3] = modelpos[2];
}


/*
====================
StudioEstimateInterpolant

====================
*/
float CStudioModelRenderer::StudioEstimateInterpolant()
{
	float dadt = 1.0;

	if ((m_fDoInterp != 0) && (m_pCurrentEntity->curstate.animtime >= m_pCurrentEntity->latched.prevanimtime + 0.01))
	{
		dadt = (m_clTime - m_pCurrentEntity->curstate.animtime) / 0.1;
		if (dadt > 2.0)
		{
			dadt = 2.0;
		}
	}
	return dadt;
}

/*
====================
StudioCalcRotations

====================
*/
void CStudioModelRenderer::StudioCalcRotations(float pos[][3], vec4_t* q, mstudioseqdesc_t* pseqdesc, mstudioanim_t* panim, float f)
{
	int i;
	int frame;
	mstudiobone_t* pbone;

	float s;
	float adj[MAXSTUDIOCONTROLLERS];
	float dadt;

	if (f > pseqdesc->numframes - 1)
	{
		f = 0; // bah, fix this bug with changing sequences too fast
	}
	// BUG ( somewhere else ) but this code should validate this data.
	// This could cause a crash if the frame # is negative, so we'll go ahead
	//  and clamp it here
	else if (f < -0.01)
	{
		f = -0.01;
	}

	frame = (int)f;

	// Con_DPrintf("%d %.4f %.4f %.4f %.4f %d\n", m_pCurrentEntity->curstate.sequence, m_clTime, m_pCurrentEntity->animtime, m_pCurrentEntity->frame, f, frame );

	// Con_DPrintf( "%f %f %f\n", m_pCurrentEntity->angles[ROLL], m_pCurrentEntity->angles[PITCH], m_pCurrentEntity->angles[YAW] );

	// Con_DPrintf("frame %d %d\n", frame1, frame2 );


	dadt = StudioEstimateInterpolant();
	s = (f - frame);

	// add in programtic controllers
	pbone = (mstudiobone_t*)((byte*)m_pStudioHeader + m_pStudioHeader->boneindex);

	StudioCalcBoneAdj(dadt, adj, m_pCurrentEntity->curstate.controller, m_pCurrentEntity->latched.prevcontroller, m_pCurrentEntity->mouth.mouthopen);

	for (i = 0; i < m_pStudioHeader->numbones; i++, pbone++, panim++)
	{
		StudioCalcBoneQuaterion(frame, s, pbone, panim, adj, q[i]);

		StudioCalcBonePosition(frame, s, pbone, panim, adj, pos[i]);
		// if (0 && i == 0)
		//	Con_DPrintf("%d %d %d %d\n", m_pCurrentEntity->curstate.sequence, frame, j, k );
	}

	if ((pseqdesc->motiontype & STUDIO_X) != 0)
	{
		pos[pseqdesc->motionbone][0] = 0.0;
	}
	if ((pseqdesc->motiontype & STUDIO_Y) != 0)
	{
		pos[pseqdesc->motionbone][1] = 0.0;
	}
	if ((pseqdesc->motiontype & STUDIO_Z) != 0)
	{
		pos[pseqdesc->motionbone][2] = 0.0;
	}

	s = 0 * ((1.0 - (f - (int)(f))) / (pseqdesc->numframes)) * m_pCurrentEntity->curstate.framerate;

	if ((pseqdesc->motiontype & STUDIO_LX) != 0)
	{
		pos[pseqdesc->motionbone][0] += s * pseqdesc->linearmovement[0];
	}
	if ((pseqdesc->motiontype & STUDIO_LY) != 0)
	{
		pos[pseqdesc->motionbone][1] += s * pseqdesc->linearmovement[1];
	}
	if ((pseqdesc->motiontype & STUDIO_LZ) != 0)
	{
		pos[pseqdesc->motionbone][2] += s * pseqdesc->linearmovement[2];
	}
}

/*
====================
Studio_FxTransform

====================
*/
void CStudioModelRenderer::StudioFxTransform(cl_entity_t* ent, float transform[3][4])
{
	if (ent->curstate.renderfx != kRenderFxExplode && ent->curstate.scale > 0)
	{
		transform[0][0] *= ent->curstate.scale;
		transform[1][0] *= ent->curstate.scale;
		transform[2][0] *= ent->curstate.scale;

		transform[0][1] *= ent->curstate.scale;
		transform[1][1] *= ent->curstate.scale;
		transform[2][1] *= ent->curstate.scale;

		transform[0][2] *= ent->curstate.scale;
		transform[1][2] *= ent->curstate.scale;
		transform[2][2] *= ent->curstate.scale;
	}

	switch (ent->curstate.renderfx)
	{
	case kRenderFxDistort:
	case kRenderFxHologram:
		if (gEngfuncs.pfnRandomLong(0, 49) == 0)
		{
			int axis = gEngfuncs.pfnRandomLong(0, 1);
			if (axis == 1) // Choose between x & z
				axis = 2;
			VectorScale(transform[axis], gEngfuncs.pfnRandomFloat(1, 1.484), transform[axis]);
		}
		else if (gEngfuncs.pfnRandomLong(0, 49) == 0)
		{
			float offset;
			int axis = gEngfuncs.pfnRandomLong(0, 1);
			if (axis == 1) // Choose between x & z
				axis = 2;
			offset = gEngfuncs.pfnRandomFloat(-10, 10);
			transform[gEngfuncs.pfnRandomLong(0, 2)][3] += offset;
		}
		break;
	case kRenderFxExplode:
	{
		float scale;

		scale = 1.0 + (m_clTime - ent->curstate.animtime) * 10.0;
		if (scale > 2) // Don't blow up more than 200%
			scale = 2;
		transform[0][1] *= scale;
		transform[1][1] *= scale;
		transform[2][1] *= scale;
	}
	break;
	}
}

/*
====================
StudioEstimateFrame

====================
*/
float CStudioModelRenderer::StudioEstimateFrame(mstudioseqdesc_t* pseqdesc)
{
	double dfdt, f;

	if (m_fDoInterp != 0)
	{
		if (m_clTime < m_pCurrentEntity->curstate.animtime)
		{
			dfdt = 0;
		}
		else
		{
			dfdt = (m_clTime - m_pCurrentEntity->curstate.animtime) * m_pCurrentEntity->curstate.framerate * pseqdesc->fps;
		}
	}
	else
	{
		dfdt = 0;
	}

	if (pseqdesc->numframes <= 1)
	{
		f = 0;
	}
	else
	{
		f = (m_pCurrentEntity->curstate.frame * (pseqdesc->numframes - 1)) / 256.0;
	}

	f += dfdt;

	if ((pseqdesc->flags & STUDIO_LOOPING) != 0)
	{
		if (pseqdesc->numframes > 1)
		{
			f -= (int)(f / (pseqdesc->numframes - 1)) * (pseqdesc->numframes - 1);
		}
		if (f < 0)
		{
			f += (pseqdesc->numframes - 1);
		}
	}
	else
	{
		if (f >= pseqdesc->numframes - 1.001)
		{
			f = pseqdesc->numframes - 1.001;
		}
		if (f < 0.0)
		{
			f = 0.0;
		}
	}
	return f;
}

/*
====================
StudioSetupBones

====================
*/
void CStudioModelRenderer::StudioSetupBones()
{
	int i;
	double f;

	mstudiobone_t* pbones;
	mstudioseqdesc_t* pseqdesc;
	mstudioanim_t* panim;

	static float pos[MAXSTUDIOBONES][3];
	static vec4_t q[MAXSTUDIOBONES];
	float bonematrix[3][4];

	static float pos2[MAXSTUDIOBONES][3];
	static vec4_t q2[MAXSTUDIOBONES];
	static float pos3[MAXSTUDIOBONES][3];
	static vec4_t q3[MAXSTUDIOBONES];
	static float pos4[MAXSTUDIOBONES][3];
	static vec4_t q4[MAXSTUDIOBONES];

	if (m_pCurrentEntity->curstate.sequence >= m_pStudioHeader->numseq || m_pCurrentEntity->curstate.sequence < 0)
	{
		m_pCurrentEntity->curstate.sequence = 0;
	}

	pseqdesc = (mstudioseqdesc_t*)((byte*)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->curstate.sequence;

	f = StudioEstimateFrame(pseqdesc);

	if (m_pCurrentEntity->latched.prevframe > f)
	{
		// Con_DPrintf("%f %f\n", m_pCurrentEntity->prevframe, f );
	}

	panim = StudioGetAnim(m_pRenderModel, pseqdesc);
	StudioCalcRotations(pos, q, pseqdesc, panim, f);

	if (pseqdesc->numblends > 1)
	{
		float s;
		float dadt;

		panim += m_pStudioHeader->numbones;
		StudioCalcRotations(pos2, q2, pseqdesc, panim, f);

		dadt = StudioEstimateInterpolant();
		s = (m_pCurrentEntity->curstate.blending[0] * dadt + m_pCurrentEntity->latched.prevblending[0] * (1.0 - dadt)) / 255.0;

		StudioSlerpBones(q, pos, q2, pos2, s);

		if (pseqdesc->numblends == 4)
		{
			panim += m_pStudioHeader->numbones;
			StudioCalcRotations(pos3, q3, pseqdesc, panim, f);

			panim += m_pStudioHeader->numbones;
			StudioCalcRotations(pos4, q4, pseqdesc, panim, f);

			s = (m_pCurrentEntity->curstate.blending[0] * dadt + m_pCurrentEntity->latched.prevblending[0] * (1.0 - dadt)) / 255.0;
			StudioSlerpBones(q3, pos3, q4, pos4, s);

			s = (m_pCurrentEntity->curstate.blending[1] * dadt + m_pCurrentEntity->latched.prevblending[1] * (1.0 - dadt)) / 255.0;
			StudioSlerpBones(q, pos, q3, pos3, s);
		}
	}

	if ((m_fDoInterp != 0) &&
		(m_pCurrentEntity->latched.sequencetime != 0.0f) &&
		(m_pCurrentEntity->latched.sequencetime + 0.01f > m_clTime) &&
		(m_pCurrentEntity->latched.prevsequence < m_pStudioHeader->numseq))
	{
		// blend from last sequence
		static float pos1b[MAXSTUDIOBONES][3];
		static vec4_t q1b[MAXSTUDIOBONES];
		float s;

		pseqdesc = (mstudioseqdesc_t*)((byte*)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->latched.prevsequence;
		panim = StudioGetAnim(m_pRenderModel, pseqdesc);
		// clip prevframe
		StudioCalcRotations(pos1b, q1b, pseqdesc, panim, m_pCurrentEntity->latched.prevframe);

		if (pseqdesc->numblends > 1)
		{
			panim += m_pStudioHeader->numbones;
			StudioCalcRotations(pos2, q2, pseqdesc, panim, m_pCurrentEntity->latched.prevframe);

			s = (m_pCurrentEntity->latched.prevseqblending[0]) / 255.0;
			StudioSlerpBones(q1b, pos1b, q2, pos2, s);

			if (pseqdesc->numblends == 4)
			{
				panim += m_pStudioHeader->numbones;
				StudioCalcRotations(pos3, q3, pseqdesc, panim, m_pCurrentEntity->latched.prevframe);

				panim += m_pStudioHeader->numbones;
				StudioCalcRotations(pos4, q4, pseqdesc, panim, m_pCurrentEntity->latched.prevframe);

				s = (m_pCurrentEntity->latched.prevseqblending[0]) / 255.0;
				StudioSlerpBones(q3, pos3, q4, pos4, s);

				s = (m_pCurrentEntity->latched.prevseqblending[1]) / 255.0;
				StudioSlerpBones(q1b, pos1b, q3, pos3, s);
			}
		}

		s = 1.0 - (m_clTime - m_pCurrentEntity->latched.sequencetime) / 0.01f;
		StudioSlerpBones(q, pos, q1b, pos1b, s);
	}
	else
	{
		// Con_DPrintf("prevframe = %4.2f\n", f);
		m_pCurrentEntity->latched.prevframe = f;
	}

	pbones = (mstudiobone_t*)((byte*)m_pStudioHeader + m_pStudioHeader->boneindex);

	// calc gait animation
	if ((m_pPlayerInfo != nullptr) && m_pPlayerInfo->gaitsequence != 0)
	{
		pseqdesc = (mstudioseqdesc_t*)((byte*)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pPlayerInfo->gaitsequence;

		panim = StudioGetAnim(m_pRenderModel, pseqdesc);
		StudioCalcRotations(pos2, q2, pseqdesc, panim, m_pPlayerInfo->gaitframe);

		for (i = 0; i < m_pStudioHeader->numbones; i++)
		{
			if (strcmp(pbones[i].name, "Bip01 Spine") == 0)
				break;
			memcpy(pos[i], pos2[i], sizeof(pos[i]));
			memcpy(q[i], q2[i], sizeof(q[i]));
		}
	}


	for (i = 0; i < m_pStudioHeader->numbones; i++)
	{
		QuaternionMatrix(q[i], bonematrix);

		bonematrix[0][3] = pos[i][0];
		bonematrix[1][3] = pos[i][1];
		bonematrix[2][3] = pos[i][2];

		if (pbones[i].parent == -1)
		{
			ConcatTransforms((*m_protationmatrix), bonematrix, (*m_pbonetransform)[i]);

			// Apply client-side effects to the transformation matrix
			StudioFxTransform(m_pCurrentEntity, (*m_pbonetransform)[i]);
		}
		else
		{
			ConcatTransforms((*m_pbonetransform)[pbones[i].parent], bonematrix, (*m_pbonetransform)[i]);
		}
	}
}


/*
====================
StudioSaveBones

====================
*/
void CStudioModelRenderer::StudioSaveBones()
{
	int i;

	mstudiobone_t* pbones;
	pbones = (mstudiobone_t*)((byte*)m_pStudioHeader + m_pStudioHeader->boneindex);

	m_nCachedBones = m_pStudioHeader->numbones;

	for (i = 0; i < m_pStudioHeader->numbones; i++)
	{
		strcpy(m_nCachedBoneNames[i], pbones[i].name);
		MatrixCopy((*m_pbonetransform)[i], m_rgCachedBoneTransform[i]);
	}
}


/*
====================
StudioMergeBones

====================
*/
void CStudioModelRenderer::StudioMergeBones(model_t* m_pSubModel)
{
	int i, j;
	double f;
	int do_hunt = 1;

	mstudiobone_t* pbones;
	mstudioseqdesc_t* pseqdesc;
	mstudioanim_t* panim;

	static float pos[MAXSTUDIOBONES][3];
	float bonematrix[3][4];
	static vec4_t q[MAXSTUDIOBONES];

	if (m_pCurrentEntity->curstate.sequence >= m_pStudioHeader->numseq || m_pCurrentEntity->curstate.sequence < 0)
	{
		m_pCurrentEntity->curstate.sequence = 0;
	}

	pseqdesc = (mstudioseqdesc_t*)((byte*)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->curstate.sequence;

	f = StudioEstimateFrame(pseqdesc);

	if (m_pCurrentEntity->latched.prevframe > f)
	{
		// Con_DPrintf("%f %f\n", m_pCurrentEntity->prevframe, f );
	}

	panim = StudioGetAnim(m_pSubModel, pseqdesc);
	StudioCalcRotations(pos, q, pseqdesc, panim, f);

	pbones = (mstudiobone_t*)((byte*)m_pStudioHeader + m_pStudioHeader->boneindex);


	for (i = 0; i < m_pStudioHeader->numbones; i++)
	{
		for (j = 0; j < m_nCachedBones; j++)
		{
			if (stricmp(pbones[i].name, m_nCachedBoneNames[j]) == 0)
			{
				MatrixCopy(m_rgCachedBoneTransform[j], (*m_pbonetransform)[i]);
				break;
			}
		}
		if (j >= m_nCachedBones)
		{
			QuaternionMatrix(q[i], bonematrix);

			bonematrix[0][3] = pos[i][0];
			bonematrix[1][3] = pos[i][1];
			bonematrix[2][3] = pos[i][2];

			if (pbones[i].parent == -1)
			{
				ConcatTransforms((*m_protationmatrix), bonematrix, (*m_pbonetransform)[i]);

				// Apply client-side effects to the transformation matrix
				StudioFxTransform(m_pCurrentEntity, (*m_pbonetransform)[i]);
			}
			else
			{
				ConcatTransforms((*m_pbonetransform)[pbones[i].parent], bonematrix, (*m_pbonetransform)[i]);
			}
		}
	}
}

/*
====================
StudioDrawModel

====================
*/
int CStudioModelRenderer::StudioDrawModel(int flags)
{
	if ((IsEntityTransparent(m_pCurrentEntity) != 0) && m_pCurrentEntity->curstate.renderamt == NULL)
		return 1;

	m_bExternalEntity = false; // reset this no matter what
	IEngineStudio.GetTimes(&m_nFrameCount, &m_clTime, &m_clOldTime);
	IEngineStudio.GetViewInfo(m_vRenderOrigin, m_vUp, m_vRight, m_vNormal);

	if (m_pCurrentEntity->curstate.renderfx == kRenderFxDeadPlayer)
	{
		entity_state_t deadplayer;

		int result;
		int save_interp;

		if (m_pCurrentEntity->curstate.renderamt <= 0 || m_pCurrentEntity->curstate.renderamt > gEngfuncs.GetMaxClients())
			return 0;

		// get copy of player
		deadplayer = *(IEngineStudio.GetPlayerState(m_pCurrentEntity->curstate.renderamt - 1)); // cl.frames[cl.parsecount & CL_UPDATE_MASK].playerstate[m_pCurrentEntity->curstate.renderamt-1];

		// clear weapon, movement state
		deadplayer.number = m_pCurrentEntity->curstate.renderamt;
		deadplayer.weaponmodel = 0;
		deadplayer.gaitsequence = 0;

		deadplayer.movetype = MOVETYPE_NONE;
		VectorCopy(m_pCurrentEntity->curstate.angles, deadplayer.angles);
		VectorCopy(m_pCurrentEntity->curstate.origin, deadplayer.origin);

		save_interp = m_fDoInterp;
		m_fDoInterp = 0;

		// draw as though it were a player
		result = StudioDrawPlayer(flags, &deadplayer);

		m_fDoInterp = save_interp;
		return result;
	}

	m_pRenderModel = m_pCurrentEntity->model;
	m_pStudioHeader = (studiohdr_t*)IEngineStudio.Mod_Extradata(m_pRenderModel);

	IEngineStudio.StudioSetHeader(m_pStudioHeader);
	IEngineStudio.SetRenderModel(m_pRenderModel);
	StudioSetupTextureHeader();

	// bluenighthawk - viewmodel animation interpolating
	if (m_pCurrentEntity == gEngfuncs.GetViewModel())
	{
		static model_s* pPrevModel = nullptr;

		m_pCurrentEntity->latched.sequencetime = m_pCurrentEntity->curstate.animtime;

		if (pPrevModel != m_pRenderModel)
		{
			m_pCurrentEntity->latched.prevsequence = m_pCurrentEntity->curstate.sequence;
			m_pCurrentEntity->latched.prevframe = 0.0f;
			pPrevModel = m_pRenderModel;
		}
	}

	if (m_pTextureHeader == nullptr)
		return 1;

	if ((flags & STUDIO_RENDER) != 0)
	{
		// see if the bounding box lets us trivially reject, also sets
		if (StudioCheckBBox() != 0)
			return 0;

		(*m_pModelsDrawn)++;
		(*m_pStudioModelCount)++; // render data cache cookie

		if (m_pStudioHeader->numbodyparts == 0)
			return 1;
	}

	StudioSetUpTransform(0);
	StudioSetupBones();

	if (m_pCurrentEntity == gEngfuncs.GetViewModel()) // Skins
		m_pCurrentEntity->curstate.skin = gHUD.m_iViewmodelSkin;

	if ((flags & STUDIO_EVENTS) != 0)
	{
		StudioCalcAttachments();
		if (m_pCurrentEntity == gEngfuncs.GetViewModel())
			StudioClientEvents();
		else
			IEngineStudio.StudioClientEvents();


		// copy attachments into global entity array
		if (m_pCurrentEntity->index > 0)
		{
			cl_entity_t* ent = gEngfuncs.GetEntityByIndex(m_pCurrentEntity->index);
			memcpy(ent->attachment, m_pCurrentEntity->attachment, sizeof(Vector) * 4);
		}
	}

	StudioSetupLighting();
	StudioEntityLight();

	if (((flags & STUDIO_RENDER) != 0) && ((m_pCurrentEntity->curstate.effects & FL_NOMODEL) == 0) && m_pCvarDrawModels->value >= 1)
	{
		StudioRenderModel();
	}

	if (m_pStudioHeader != nullptr)
		g_viewinfo.phdr = m_pStudioHeader;

	// get bone angles and calculate base angles using fake entity
	if (m_pCurrentEntity == gEngfuncs.GetViewModel())
	{
		gHUD.m_prevstate.sequence = m_pCurrentEntity->curstate.sequence;
		cl_entity_s temp = *gEngfuncs.GetViewModel();
		temp.angles = temp.curstate.angles = temp.origin = temp.curstate.origin = Vector(0, 0, 0);
		m_pCurrentEntity = &temp;
		m_pRenderModel = m_pCurrentEntity->model;
		m_pStudioHeader = (studiohdr_t*)IEngineStudio.Mod_Extradata(m_pRenderModel);
		IEngineStudio.StudioSetHeader(m_pStudioHeader);
		IEngineStudio.SetRenderModel(m_pRenderModel);

		StudioSetUpTransform(0);
		StudioSetupBones();
		for (int i = 0; i < m_pStudioHeader->numbones; i++)
		{
			FranUtils::Maths::MatrixAngles((*m_pbonetransform)[i], g_viewinfo.boneangles[i], g_viewinfo.bonepos[i]);
			NormalizeAngles((float*)&g_viewinfo.boneangles[i]);
		}
		temp = *gEngfuncs.GetViewModel();
		temp.angles = temp.curstate.angles = temp.origin = temp.curstate.origin = Vector(0, 0, 0);
		temp.curstate.sequence = 0;
		temp.curstate.frame = 0;
		temp.curstate.animtime = 0;
		temp.latched.prevframe = 0;
		m_pCurrentEntity = &temp;

		StudioSetUpTransform(0);
		StudioSetupBones();
		for (int i = 0; i < m_pStudioHeader->numbones; i++)
		{
			FranUtils::Maths::MatrixAngles((*m_pbonetransform)[i], g_viewinfo.prevboneangles[i], g_viewinfo.prevbonepos[i]);
			NormalizeAngles((float*)&g_viewinfo.prevboneangles[i]);
		}
	}

	return 1;
}

/*
====================
StudioEstimateGait

====================
*/
void CStudioModelRenderer::StudioEstimateGait(entity_state_t* pplayer)
{
	float dt;
	Vector est_velocity;

	dt = (m_clTime - m_clOldTime);
	if (dt < 0)
		dt = 0;
	else if (dt > 1.0)
		dt = 1;

	if (dt == 0 || m_pPlayerInfo->renderframe == m_nFrameCount)
	{
		m_flGaitMovement = 0;
		return;
	}

	// VectorAdd( pplayer->velocity, pplayer->prediction_error, est_velocity );
	if (m_fGaitEstimation != 0)
	{
		VectorSubtract(m_pCurrentEntity->origin, m_pPlayerInfo->prevgaitorigin, est_velocity);
		VectorCopy(m_pCurrentEntity->origin, m_pPlayerInfo->prevgaitorigin);
		m_flGaitMovement = Length(est_velocity);
		if (dt <= 0 || m_flGaitMovement / dt < 5)
		{
			m_flGaitMovement = 0;
			est_velocity[0] = 0;
			est_velocity[1] = 0;
		}
	}
	else
	{
		VectorCopy(pplayer->velocity, est_velocity);
		m_flGaitMovement = Length(est_velocity) * dt;
	}

	if (est_velocity[1] == 0 && est_velocity[0] == 0)
	{
		float flYawDiff = m_pCurrentEntity->angles[YAW] - m_pPlayerInfo->gaityaw;
		flYawDiff = flYawDiff - (int)(flYawDiff / 360) * 360;
		if (flYawDiff > 180)
			flYawDiff -= 360;
		if (flYawDiff < -180)
			flYawDiff += 360;

		if (dt < 0.25)
			flYawDiff *= dt * 4;
		else
			flYawDiff *= dt;

		m_pPlayerInfo->gaityaw += flYawDiff;
		m_pPlayerInfo->gaityaw = m_pPlayerInfo->gaityaw - (int)(m_pPlayerInfo->gaityaw / 360) * 360;

		m_flGaitMovement = 0;
	}
	else
	{
		m_pPlayerInfo->gaityaw = (atan2(est_velocity[1], est_velocity[0]) * 180 / M_PI);
		if (m_pPlayerInfo->gaityaw > 180)
			m_pPlayerInfo->gaityaw = 180;
		if (m_pPlayerInfo->gaityaw < -180)
			m_pPlayerInfo->gaityaw = -180;
	}
}

/*
====================
StudioProcessGait

====================
*/
void CStudioModelRenderer::StudioProcessGait(entity_state_t* pplayer)
{
	mstudioseqdesc_t* pseqdesc;
	float dt;
	int iBlend;
	float flYaw; // view direction relative to movement

	pseqdesc = (mstudioseqdesc_t*)((byte*)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->curstate.sequence;

	StudioPlayerBlend(pseqdesc, &iBlend, &m_pCurrentEntity->angles[PITCH]);

	m_pCurrentEntity->latched.prevangles[PITCH] = m_pCurrentEntity->angles[PITCH];
	m_pCurrentEntity->curstate.blending[0] = iBlend;
	m_pCurrentEntity->latched.prevblending[0] = m_pCurrentEntity->curstate.blending[0];
	m_pCurrentEntity->latched.prevseqblending[0] = m_pCurrentEntity->curstate.blending[0];

	// Con_DPrintf("%f %d\n", m_pCurrentEntity->angles[PITCH], m_pCurrentEntity->blending[0] );

	dt = (m_clTime - m_clOldTime);
	if (dt < 0)
		dt = 0;
	else if (dt > 1.0)
		dt = 1;

	StudioEstimateGait(pplayer);

	// Con_DPrintf("%f %f\n", m_pCurrentEntity->angles[YAW], m_pPlayerInfo->gaityaw );

	// calc side to side turning
	flYaw = m_pCurrentEntity->angles[YAW] - m_pPlayerInfo->gaityaw;
	flYaw = flYaw - (int)(flYaw / 360) * 360;
	if (flYaw < -180)
		flYaw = flYaw + 360;
	if (flYaw > 180)
		flYaw = flYaw - 360;

	if (flYaw > 120)
	{
		m_pPlayerInfo->gaityaw = m_pPlayerInfo->gaityaw - 180;
		m_flGaitMovement = -m_flGaitMovement;
		flYaw = flYaw - 180;
	}
	else if (flYaw < -120)
	{
		m_pPlayerInfo->gaityaw = m_pPlayerInfo->gaityaw + 180;
		m_flGaitMovement = -m_flGaitMovement;
		flYaw = flYaw + 180;
	}

	// adjust torso
	m_pCurrentEntity->curstate.controller[0] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	m_pCurrentEntity->curstate.controller[1] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	m_pCurrentEntity->curstate.controller[2] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	m_pCurrentEntity->curstate.controller[3] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	m_pCurrentEntity->latched.prevcontroller[0] = m_pCurrentEntity->curstate.controller[0];
	m_pCurrentEntity->latched.prevcontroller[1] = m_pCurrentEntity->curstate.controller[1];
	m_pCurrentEntity->latched.prevcontroller[2] = m_pCurrentEntity->curstate.controller[2];
	m_pCurrentEntity->latched.prevcontroller[3] = m_pCurrentEntity->curstate.controller[3];

	m_pCurrentEntity->angles[YAW] = m_pPlayerInfo->gaityaw;
	if (m_pCurrentEntity->angles[YAW] < -0)
		m_pCurrentEntity->angles[YAW] += 360;
	m_pCurrentEntity->latched.prevangles[YAW] = m_pCurrentEntity->angles[YAW];

	pseqdesc = (mstudioseqdesc_t*)((byte*)m_pStudioHeader + m_pStudioHeader->seqindex) + pplayer->gaitsequence;

	// calc gait frame
	if (pseqdesc->linearmovement[0] > 0)
	{
		m_pPlayerInfo->gaitframe += (m_flGaitMovement / pseqdesc->linearmovement[0]) * pseqdesc->numframes;
	}
	else
	{
		m_pPlayerInfo->gaitframe += pseqdesc->fps * dt;
	}

	// do modulo
	m_pPlayerInfo->gaitframe = m_pPlayerInfo->gaitframe - (int)(m_pPlayerInfo->gaitframe / pseqdesc->numframes) * pseqdesc->numframes;
	if (m_pPlayerInfo->gaitframe < 0)
		m_pPlayerInfo->gaitframe += pseqdesc->numframes;
}

/*
====================
StudioDrawPlayer

====================
*/
int CStudioModelRenderer::StudioDrawPlayer(int flags, entity_state_t* pplayer)
{
	m_bExternalEntity = false;

	IEngineStudio.GetTimes(&m_nFrameCount, &m_clTime, &m_clOldTime);
	IEngineStudio.GetViewInfo(m_vRenderOrigin, m_vUp, m_vRight, m_vNormal);

	m_nPlayerIndex = pplayer->number - 1;

	if (m_nPlayerIndex < 0 || m_nPlayerIndex >= gEngfuncs.GetMaxClients())
		return 0;

	m_pRenderModel = m_pCurrentEntity->model;
	if (m_pRenderModel == nullptr)
		return 0;

	m_pStudioHeader = (studiohdr_t*)IEngineStudio.Mod_Extradata(m_pRenderModel);
	IEngineStudio.StudioSetHeader(m_pStudioHeader);
	IEngineStudio.SetRenderModel(m_pRenderModel);
	StudioSetupTextureHeader();

	if (m_pTextureHeader == nullptr)
		return 1;

	if ((flags & STUDIO_RENDER) != 0)
	{
		// see if the bounding box lets us trivially reject, also sets
		if (StudioCheckBBox() != 0)
			return 0;

		(*m_pModelsDrawn)++;
		(*m_pStudioModelCount)++; // render data cache cookie

		if (m_pStudioHeader->numbodyparts == 0)
			return 1;
	}

	if (pplayer->gaitsequence != 0)
	{
		Vector orig_angles;
		m_pPlayerInfo = IEngineStudio.PlayerInfo(m_nPlayerIndex);

		VectorCopy(m_pCurrentEntity->angles, orig_angles);

		StudioProcessGait(pplayer);

		m_pPlayerInfo->gaitsequence = pplayer->gaitsequence;
		m_pPlayerInfo = nullptr;

		StudioSetUpTransform(0);
		VectorCopy(orig_angles, m_pCurrentEntity->angles);
	}
	else
	{
		m_pCurrentEntity->curstate.controller[0] = 127;
		m_pCurrentEntity->curstate.controller[1] = 127;
		m_pCurrentEntity->curstate.controller[2] = 127;
		m_pCurrentEntity->curstate.controller[3] = 127;
		m_pCurrentEntity->latched.prevcontroller[0] = m_pCurrentEntity->curstate.controller[0];
		m_pCurrentEntity->latched.prevcontroller[1] = m_pCurrentEntity->curstate.controller[1];
		m_pCurrentEntity->latched.prevcontroller[2] = m_pCurrentEntity->curstate.controller[2];
		m_pCurrentEntity->latched.prevcontroller[3] = m_pCurrentEntity->curstate.controller[3];

		m_pPlayerInfo = IEngineStudio.PlayerInfo(m_nPlayerIndex);
		m_pPlayerInfo->gaitsequence = 0;

		StudioSetUpTransform(0);
	}

	m_pPlayerInfo = IEngineStudio.PlayerInfo(m_nPlayerIndex);
	StudioSetupBones();
	StudioSaveBones();
	m_pPlayerInfo->renderframe = m_nFrameCount;

	m_pPlayerInfo = nullptr;

	if ((flags & STUDIO_EVENTS) != 0)
	{
		StudioCalcAttachments();
		if (m_pCurrentEntity == gEngfuncs.GetViewModel())
			StudioClientEvents();
		else
			IEngineStudio.StudioClientEvents();

		// copy attachments into global entity array
		if (m_pCurrentEntity->index > 0)
		{
			cl_entity_t* ent = gEngfuncs.GetEntityByIndex(m_pCurrentEntity->index);

			memcpy(ent->attachment, m_pCurrentEntity->attachment, sizeof(Vector) * 4);
		}
	}

	if (((flags & STUDIO_RENDER) != 0) && ((m_pCurrentEntity->curstate.effects & FL_NOMODEL) == 0) && m_pCvarDrawModels->value >= 1)
	{
		if ((m_pCvarHiModels->value != 0.0f) && m_pRenderModel != m_pCurrentEntity->model)
		{
			// show highest resolution multiplayer model
			m_pCurrentEntity->curstate.body = 255;
		}

		if (!(m_pCvarDeveloper->value == 0 && gEngfuncs.GetMaxClients() == 1) && (m_pRenderModel == m_pCurrentEntity->model))
		{
			m_pCurrentEntity->curstate.body = 1; // force helmet
		}

		StudioSetupLighting();
		StudioEntityLight();

		m_pPlayerInfo = IEngineStudio.PlayerInfo(m_nPlayerIndex);

		// get remap colors
		m_nTopColor = m_pPlayerInfo->topcolor;
		m_nBottomColor = m_pPlayerInfo->bottomcolor;
		if (m_nTopColor < 0)
			m_nTopColor = 0;
		if (m_nTopColor > 360)
			m_nTopColor = 360;
		if (m_nBottomColor < 0)
			m_nBottomColor = 0;
		if (m_nBottomColor > 360)
			m_nBottomColor = 360;

		IEngineStudio.StudioSetRemapColors(m_nTopColor, m_nBottomColor);

		StudioRenderModel();
		m_pPlayerInfo = nullptr;

		if (pplayer->weaponmodel != 0)
		{
			cl_entity_t saveent = *m_pCurrentEntity;
			model_t* savedmdl = m_pRenderModel;

			model_t* pweaponmodel = IEngineStudio.GetModelByIndex(pplayer->weaponmodel);
			m_pRenderModel = pweaponmodel;

			m_pStudioHeader = (studiohdr_t*)IEngineStudio.Mod_Extradata(pweaponmodel);
			IEngineStudio.StudioSetHeader(m_pStudioHeader);
			StudioSetupTextureHeader();

			StudioMergeBones(pweaponmodel);
			StudioRenderModel();
			StudioCalcAttachments();

			*m_pCurrentEntity = saveent;
			m_pRenderModel = savedmdl;
		}
	}

	return 1;
}

/*
====================
StudioCalcAttachments

====================
*/
void CStudioModelRenderer::StudioCalcAttachments()
{
	int i;
	mstudioattachment_t* pattachment;

	if (m_pStudioHeader->numattachments > 4)
	{
		gEngfuncs.Con_DPrintf("Too many attachments on %s\n", m_pCurrentEntity->model->name);
		exit(-1);
	}

	// calculate attachment points
	pattachment = (mstudioattachment_t*)((byte*)m_pStudioHeader + m_pStudioHeader->attachmentindex);
	for (i = 0; i < m_pStudioHeader->numattachments; i++)
	{
		VectorTransformSSE(pattachment[i].org, (*m_pbonetransform)[pattachment[i].bone], m_pCurrentEntity->attachment[i]);
	}
}

extern GLuint g_cloudShader;

#include "Exports.h"
/*
====================
StudioCalcAttachments

====================
*/
void CStudioModelRenderer::StudioClientEvents()
{
	mstudioseqdesc_t* pseqdesc;
	mstudioevent_t* pevent;
	cl_entity_t* e = m_pCurrentEntity;
	int i, sequence;
	float end, start;

	if ((e->curstate.effects & EF_MUZZLEFLASH) != 0)
	{
		dlight_t* el = gEngfuncs.pEfxAPI->CL_AllocElight(0);

		e->curstate.effects &= ~EF_MUZZLEFLASH;
		VectorCopy(e->attachment[0], el->origin);
		el->die = m_clTime + 0.05f;
		el->color.r = 255;
		el->color.g = 192;
		el->color.b = 64;
		el->decay = 320;
		el->radius = 24;
	}

	sequence = std::clamp(e->curstate.sequence, 0, m_pStudioHeader->numseq - 1);
	pseqdesc = (mstudioseqdesc_t*)((byte*)m_pStudioHeader + m_pStudioHeader->seqindex) + sequence;

	// no events for this animation
	if (pseqdesc->numevents == 0)
		return;

	end = StudioEstimateFrame(pseqdesc);
	start = end - e->curstate.framerate * (m_clTime - m_clOldTime) * pseqdesc->fps;
	pevent = (mstudioevent_t*)((byte*)m_pStudioHeader + pseqdesc->eventindex);

	if (e->latched.sequencetime == e->curstate.animtime)
	{
		if ((pseqdesc->flags & STUDIO_LOOPING) != 0)
			start = -0.01f;
	}

	for (i = 0; i < pseqdesc->numevents; i++)
	{
		// ignore all non-client-side events
		if (pevent[i].event < 5000)
			continue;

		if ((float)pevent[i].frame > start && pevent[i].frame <= end)
			HUD_StudioEvent(&pevent[i], e);
	}
}

/*
====================
StudioRenderModel

====================
*/
void CStudioModelRenderer::StudioRenderModel()
{
	// HACK HACK - replace hardcoded textures
	std::vector<mstudiotexture_t> savedtexture;
	studiohdr_t* pHdr = (studiohdr_t*)m_pStudioHeader;
	mstudiotexture_t* pTexture = (mstudiotexture_t*)((byte*)m_pRenderModel->cache.data + pHdr->textureindex);

	//bool needToRestoreTexture = false;
	//if (!strcmp(m_pCurrentEntity->model->name, "models/skysphere.mdl"))
	//{
	//	if (pHdr->textureindex > 0)
	//	{
	//		for (int i = 0; i < pHdr->numtextures; i++)
	//		{
	//			savedtexture.push_back(pTexture[i]);
	//			// memcpy(&pTexture[i], &pTexture[pHdr->numtextures + 1], sizeof(mstudiotexture_t));
	//			if (!strcmp(pTexture[i].name, "white.bmp"))
	//			{
	//				pTexture[i].index = g_cloudShader;
	//			}
	//		}
	//	}
	//
	//	needToRestoreTexture = true;
	//}

	// bacontsu - render distance, credit to Aynekko (Diffusion)
	if ((m_pCurrentEntity->curstate.origin - gEngfuncs.GetLocalPlayer()->curstate.origin).Length() > m_pCvarRenderDistance->value && m_pCurrentEntity != gEngfuncs.GetViewModel())
		return;

	if ((m_pCurrentEntity->curstate.origin - gEngfuncs.GetLocalPlayer()->curstate.origin).Length() > m_pCvarRenderDistance->value - 100.0f && m_pCurrentEntity != gEngfuncs.GetViewModel())
	{
		float diff = m_pCvarRenderDistance->value - (m_pCurrentEntity->curstate.origin - gEngfuncs.GetLocalPlayer()->curstate.origin).Length();

		m_pCurrentEntity->curstate.rendermode = kRenderTransTexture;
		m_pCurrentEntity->curstate.renderamt = diff * 255.0f / 100.0f;
	}
	else
	{
		m_pCurrentEntity->curstate.renderamt = m_pCurrentEntity->baseline.renderamt;
		m_pCurrentEntity->curstate.rendermode = m_pCurrentEntity->baseline.rendermode;
	}


	// Save texture states before rendering, so we don't
	// cause any bugs in HL by changing texture binds, etc
	R_SaveGLStates();

	if (m_pCurrentEntity->curstate.renderfx == kRenderFxGlowShell)
	{
		m_pCurrentEntity->curstate.renderfx = kRenderFxNone;
		StudioRenderFinal();

		glFogfv(GL_FOG_COLOR, g_vecZero);
		m_bChromeShell = true;

		m_pCurrentEntity->curstate.renderfx = kRenderFxGlowShell;
		StudioRenderFinal();

		glFogfv(GL_FOG_COLOR, gHUD.m_pFogSettings.color);
		m_bChromeShell = false;
	}
	else
	{
		StudioRenderFinal();
	}

	// Restore saved states
	R_RestoreGLStates();

	//if (needToRestoreTexture)
	//{
	//	for (int i = 0; i < pHdr->numtextures; i++)
	//	{
	//		memcpy(&pTexture[i], &savedtexture[i], sizeof(mstudiotexture_t));
	//	}
	//}
}

/*
====================
StudioRenderFinal

====================
*/
void CStudioModelRenderer::StudioRenderFinal()
{
	if (StudioShouldDrawShadow())
	{
		StudioDrawShadow();
	}

	StudioSetupRenderer(m_pCurrentEntity->curstate.rendermode);
	StudioSetChromeVectors();

	for (int i = 0; i < m_pStudioHeader->numbodyparts; i++)
	{
		// draw actual model
		StudioSetupModel(i);
		StudioDrawPoints();

		/*
		// draw shadow pass
		StudioSetupModel(i, (void**)&m_pBodyPart, (void**)&m_pSubModel);
		StudioGetVerts();
		GL_StudioDrawShadow();
		*/
	}

	/*
	* if (StudioShouldDrawShadow())
	{
		StudioDrawShadow();
	}
	*/

	StudioRestoreRenderer();
	StudioDrawDecals();

	// Restore this here, so decals won't mess up
	if (m_pCurrentEntity->curstate.rendermode != kRenderNormal && m_iEngineBinding != m_iCurrentBinding)
		glBindTexture(GL_TEXTURE_2D, m_iEngineBinding);

	if (gBSPRenderer.m_pCvarWireFrame->value != 0.0f)
		StudioDrawWireframe();

	if (m_pCvarModelsBBoxDebug->value > 0)
		StudioDrawBBox();
}

/*
====================
StudioDrawWireframe

====================
*/
void CStudioModelRenderer::StudioDrawWireframe()
{
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_CULL_FACE);
	glColor4f(0.0, 1.0, 0.0, 1.0);
	glLineWidth(1);

	if (gBSPRenderer.m_pCvarWireFrame->value >= 3)
	{
		glDisable(GL_DEPTH_TEST);

		if (gHUD.m_pFogSettings.active)
			glDisable(GL_FOG);
	}

	for (int i = 0; i < m_pStudioHeader->numbodyparts; i++)
	{
		StudioSetupModel(i);
		StudioDrawPoints();
	}

	glPolygonOffset(-1, -1);
	glEnable(GL_POLYGON_OFFSET_FILL);
	glColor4f(1.0, 0.0, 0.5, 1.0);

	studiodecal_t* pnext = (studiodecal_t*)m_pCurrentEntity->efrag;
	while (pnext != nullptr)
	{
		for (int i = 0; i < pnext->numverts; i++)
			VectorTransformSSE(pnext->verts[i].position, (*m_pbonetransform)[pnext->verts[i].boneindex], m_vVertexTransform[i]);

		for (int i = 0; i < pnext->numpolys; i++)
		{
			decalvert_t* verts = &pnext->polys[i].verts[0];
			glBegin(GL_POLYGON);
			for (int j = 0; j < pnext->polys[i].numverts; j++)
			{
				glTexCoord2f(verts[j].texcoord[0], verts[j].texcoord[1]);
				glVertex3fv(m_pVertexTransform[verts[j].vertindex]);
			}
			glEnd();
		}

		studiodecal_t* next = pnext->next;
		pnext = next;
	}

	glDisable(GL_POLYGON_OFFSET_FILL);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_CULL_FACE);

	if (gBSPRenderer.m_pCvarWireFrame->value >= 2)
	{
		glEnable(GL_DEPTH_TEST);

		if (gHUD.m_pFogSettings.active)
			glEnable(GL_FOG);
	}
}

/*
====================
StudioSetupModel

====================
*/
void CStudioModelRenderer::StudioSetupModel(int bodypart)
{
	m_pBodyPart = (mstudiobodyparts_t*)((byte*)m_pStudioHeader + m_pStudioHeader->bodypartindex) + bodypart;

	int index = m_pCurrentEntity->curstate.body / m_pBodyPart->base;
	index = index % m_pBodyPart->nummodels;

	m_pSubModel = (mstudiomodel_t*)((byte*)m_pStudioHeader + m_pBodyPart->modelindex) + index;
}

/*
====================
StudioAllocExtraInfo

====================
*/
entextrainfo_t* CStudioModelRenderer::StudioAllocExtraInfo()
{
	if (m_iNumExtraInfo == MAXRENDERENTS)
		m_iNumExtraInfo = NULL;

	if (m_pExtraInfo[m_iNumExtraInfo].pEntity != nullptr)
	{
		m_pExtraInfo[m_iNumExtraInfo].pEntity->topnode = nullptr;
		m_pExtraInfo[m_iNumExtraInfo].pEntity = nullptr;
		memset(&m_pExtraInfo[m_iNumExtraInfo], 0, sizeof(entextrainfo_t));
	}

	m_iNumExtraInfo++;
	return &m_pExtraInfo[m_iNumExtraInfo - 1];
}

/*
====================
StudioSetupTextureHeader

====================
*/
void CStudioModelRenderer::StudioSetupTextureHeader()
{
	if ((m_pStudioHeader->numtextures != 0) && (m_pStudioHeader->textureindex != 0))
	{
		m_pTextureHeader = m_pStudioHeader;
		return;
	}

	if (m_pRenderModel->lightdata != nullptr)
	{
		m_pTextureHeader = (studiohdr_t*)((model_t*)m_pRenderModel->lightdata)->cache.data;
		return;
	}

	char szName[64];
	strcpy(szName, m_pRenderModel->name);
	strcpy(&szName[(strlen(szName) - 4)], "T.mdl");

	// Load the model in using my code, Valve's sucks dick
	model_t* pModel = Mod_LoadModel(szName);

	if (pModel == nullptr)
		return;

	m_pTextureHeader = (studiohdr_t*)pModel->cache.data;
	m_pRenderModel->lightdata = (color24*)pModel;
}

/*
====================
StudioSwapEngineCache

====================
*/
void CStudioModelRenderer::StudioSwapEngineCache()
{
	// TODO: Properly convert to c++ string

	char szFile[256];
	std::string strModelName;
	std::string strTexture;

	m_iNumEngineCacheModels = NULL;
	for (int i = 0; i < 1024; i++)
	{
		model_t* pModel = IEngineStudio.GetModelByIndex((i + 1));

		if (pModel == nullptr)
			break;

		m_iNumEngineCacheModels++;

		if (pModel->type != mod_studio)
			continue;

		m_pRenderModel = pModel;
		m_pStudioHeader = (studiohdr_t*)IEngineStudio.Mod_Extradata(pModel);
		StudioSetupTextureHeader();

		if (m_pTextureHeader == nullptr)
			continue;

		strModelName = FilenameFromPath(pModel->name);
		FranUtils::StringUtils::LowerCase_Ref(strModelName);

		mstudiotexture_t* ptexture = (mstudiotexture_t*)((byte*)m_pTextureHeader + m_pTextureHeader->textureindex);
		for (int j = 0; j < m_pTextureHeader->numtextures; j++, ptexture++)
		{
			strTexture = FilenameFromPath(ptexture->name);
			FranUtils::StringUtils::LowerCase_Ref(strTexture);

			if (!gTextureLoader.TextureHasFlag(strModelName, strTexture, TEXFLAG_ALTERNATE))
				continue;

			if (m_pCvarDeveloper->value > 1)
				gEngfuncs.Con_Printf("Model '%s' has '%s' marked as using an alternate texture.\n", pModel->name, ptexture->name);

			bool bNoMipMap = gTextureLoader.TextureHasFlag(strModelName, strTexture, TEXFLAG_NOMIPMAP);
			sprintf(szFile, "gfx/textures/models/%s/%s.dds", strModelName.c_str(), strTexture.c_str());
			cl_texture_t* pTexture = gTextureLoader.LoadTexture(szFile, ptexture->index, false, bNoMipMap ? true : false);

			if ((pTexture != nullptr) && m_pCvarDeveloper->value > 1)
				gEngfuncs.Con_Printf("Loaded '%s'\n", szFile);
		}
	}
}

/*
====================
StudioSetupRenderer

====================
*/
void CStudioModelRenderer::StudioSetupRenderer(int rendermode)
{
	m_pVertexTransform = &m_vVertexTransform[0];
	m_pNormalTransform = &m_vNormalTransform[0];

	m_fAlpha = 1;
	m_bUseBlending = false;

	// Set transparency
	glShadeModel(GL_SMOOTH);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);

	// Make sure texturing is enabled only where needed
	gBSPRenderer.glActiveTextureARB(GL_TEXTURE3_ARB);
	glDisable(GL_TEXTURE_2D);

	gBSPRenderer.glActiveTextureARB(GL_TEXTURE2_ARB);
	glDisable(GL_TEXTURE_2D);

	gBSPRenderer.glActiveTextureARB(GL_TEXTURE1_ARB);
	glDisable(GL_TEXTURE_2D);

	// First only
	gBSPRenderer.glActiveTextureARB(GL_TEXTURE0_ARB);
	glEnable(GL_TEXTURE_2D);

	// Set up texturing mode
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PRIMARY_COLOR_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 2);

	// Make sure this is correct
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();

	glMatrixMode(GL_MODELVIEW);

	// Get current binding
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &m_iCurrentBinding);
	if (m_pCurrentEntity->curstate.rendermode != kRenderNormal)
	{
		// Keep bindings synced with the engine when drawn together with sprites
		m_iEngineBinding = m_iCurrentBinding;
	}

	if (rendermode == kRenderTransAdd)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		m_fAlpha = (float)m_pCurrentEntity->curstate.renderamt / 255.0;
		m_bUseBlending = TRUE;
	}

	if (rendermode == kRenderTransAlpha || rendermode == kRenderTransTexture)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		m_fAlpha = (float)m_pCurrentEntity->curstate.renderamt / 255.0;
		m_bUseBlending = TRUE;
	}

	// Set current alpha
	glColor4f(GL_ONE, GL_ONE, GL_ONE, m_fAlpha);

	if (m_bChromeShell)
	{
		glEnable(GL_BLEND);
		glDepthMask(GL_FALSE);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		m_bUseBlending = TRUE;

		glColor4ub(m_pCurrentEntity->curstate.rendercolor.r,
			m_pCurrentEntity->curstate.rendercolor.g,
			m_pCurrentEntity->curstate.rendercolor.b, 255);

		msprite_t* pSprite = (msprite_t*)m_pChromeSprite->cache.data;
		glBindTexture(GL_TEXTURE_2D, pSprite->frames[0].frameptr->gl_texturenum);
		m_iCurrentBinding = pSprite->frames[0].frameptr->gl_texturenum;
	}

	if (m_bExternalEntity && m_pCurrentEntity->curstate.scale != 0)
		glScalef(m_pCurrentEntity->curstate.scale, m_pCurrentEntity->curstate.scale, m_pCurrentEntity->curstate.scale);

	if (!m_bChromeShell)
	{
		if (gBSPRenderer.m_bShaderSupport && m_pCvarModelShaders->value > 0)
		{
			float flMatrix[16];
			glEnable(GL_FRAGMENT_PROGRAM_ARB);
			glEnable(GL_VERTEX_PROGRAM_ARB);

			if (gHUD.m_pFogSettings.active)
				gBSPRenderer.glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, m_uiFragmentShaders[1]);
			else
				gBSPRenderer.glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, m_uiFragmentShaders[0]);

			if (!gBSPRenderer.m_bRadialFogSupport || gBSPRenderer.m_pCvarRadialFog->value < 1)
				gBSPRenderer.glBindProgramARB(GL_VERTEX_PROGRAM_ARB, m_uiVertexShaders[m_iNumModelLights]);
			else
				gBSPRenderer.glBindProgramARB(GL_VERTEX_PROGRAM_ARB, m_uiVertexShaders[(7 + m_iNumModelLights)]);

			glGetFloatv(GL_MODELVIEW_MATRIX, flMatrix);
			gBSPRenderer.glProgramLocalParameter4fARB(GL_VERTEX_PROGRAM_ARB, 3, flMatrix[0], flMatrix[4], flMatrix[8], flMatrix[12]);
			gBSPRenderer.glProgramLocalParameter4fARB(GL_VERTEX_PROGRAM_ARB, 4, flMatrix[1], flMatrix[5], flMatrix[9], flMatrix[13]);
			gBSPRenderer.glProgramLocalParameter4fARB(GL_VERTEX_PROGRAM_ARB, 5, flMatrix[2], flMatrix[6], flMatrix[10], flMatrix[14]);
			gBSPRenderer.glProgramLocalParameter4fARB(GL_VERTEX_PROGRAM_ARB, 6, flMatrix[3], flMatrix[7], flMatrix[11], flMatrix[15]);

			glGetFloatv(GL_PROJECTION_MATRIX, flMatrix);
			gBSPRenderer.glProgramLocalParameter4fARB(GL_VERTEX_PROGRAM_ARB, 7, flMatrix[0], flMatrix[4], flMatrix[8], flMatrix[12]);
			gBSPRenderer.glProgramLocalParameter4fARB(GL_VERTEX_PROGRAM_ARB, 8, flMatrix[1], flMatrix[5], flMatrix[9], flMatrix[13]);
			gBSPRenderer.glProgramLocalParameter4fARB(GL_VERTEX_PROGRAM_ARB, 9, flMatrix[2], flMatrix[6], flMatrix[10], flMatrix[14]);
			gBSPRenderer.glProgramLocalParameter4fARB(GL_VERTEX_PROGRAM_ARB, 10, flMatrix[3], flMatrix[7], flMatrix[11], flMatrix[15]);

			gBSPRenderer.glProgramLocalParameter4fARB(GL_VERTEX_PROGRAM_ARB, 0, m_pLighting.lightdir[0], m_pLighting.lightdir[1], m_pLighting.lightdir[2], 0);
			gBSPRenderer.glProgramLocalParameter4fARB(GL_VERTEX_PROGRAM_ARB, 1, m_pLighting.ambientlight[0], m_pLighting.ambientlight[1], m_pLighting.ambientlight[2], 0);
			gBSPRenderer.glProgramLocalParameter4fARB(GL_VERTEX_PROGRAM_ARB, 2, m_pLighting.diffuselight[0], m_pLighting.diffuselight[1], m_pLighting.diffuselight[2], 0);
		}
		else
		{
			// Damn, I hate this, but GL_LIGHTING won't work otherwise
			float flDirection[4] = {-m_pLighting.lightdir[0], -m_pLighting.lightdir[1], -m_pLighting.lightdir[2], 0.0f};

			glEnable(GL_LIGHTING);
			glEnable(GL_LIGHT0);

			glLightfv(GL_LIGHT0, GL_POSITION, flDirection);
			glLightfv(GL_LIGHT0, GL_AMBIENT, m_pLighting.ambientlight);
			glLightfv(GL_LIGHT0, GL_DIFFUSE, m_pLighting.ambientlight + m_pLighting.diffuselight);
		}

		for (int i = 0; i < m_iNumModelLights; i++)
		{
			float flPosition[] = {m_pModelLights[i]->origin[0], m_pModelLights[i]->origin[1], m_pModelLights[i]->origin[2], 1.0};
			float flForward[] = {m_pModelLights[i]->forward[0], m_pModelLights[i]->forward[1], m_pModelLights[i]->forward[2], 0.0};

			if (m_bExternalEntity)
			{
				Vector vTemp, vVector;
				VectorSubtract(flPosition, m_pCurrentEntity->origin, flPosition);

				// Transform position
				VectorCopy(flPosition, vTemp);
				VectorIRotate(vTemp, (*m_protationmatrix), vVector);
				VectorCopy(vVector, flPosition);

				// Transform forward vector
				VectorCopy(flForward, vTemp);
				VectorIRotate(vTemp, (*m_protationmatrix), vVector);
				VectorCopy(vVector, flForward);
			}

			if (gBSPRenderer.m_bShaderSupport && m_pCvarModelShaders->value > 0)
			{
				gBSPRenderer.glProgramLocalParameter4fARB(GL_VERTEX_PROGRAM_ARB, 12 + (i * 3), flPosition[0], flPosition[1], flPosition[2], m_pModelLights[i]->spotcos > 0 ? 1 : 0);
				gBSPRenderer.glProgramLocalParameter4fARB(GL_VERTEX_PROGRAM_ARB, 13 + (i * 3), m_pModelLights[i]->color.x, m_pModelLights[i]->color.y, m_pModelLights[i]->color.z, m_pModelLights[i]->radius);
				gBSPRenderer.glProgramLocalParameter4fARB(GL_VERTEX_PROGRAM_ARB, 14 + (i * 3), flForward[0], flForward[1], flForward[2], cos((m_pModelLights[i]->spotcos * 2) * 0.3 * (M_PI * 2 / 360)));
			}
			else
			{
				glEnable(GL_LIGHT1 + i);
				glLightfv(GL_LIGHT1 + i, GL_AMBIENT, Vector(0, 0, 0));
				glLightfv(GL_LIGHT1 + i, GL_SPECULAR, Vector(0, 0, 0));
				glLightfv(GL_LIGHT1 + i, GL_POSITION, flPosition);
				glLightfv(GL_LIGHT1 + i, GL_DIFFUSE, m_pModelLights[i]->color);

				glLightf(GL_LIGHT1 + i, GL_QUADRATIC_ATTENUATION, 0);
				glLightf(GL_LIGHT1 + i, GL_CONSTANT_ATTENUATION, 0);
				glLightf(GL_LIGHT1 + i, GL_LINEAR_ATTENUATION, 1 / (m_pModelLights[i]->radius * 0.2));

				if (m_pModelLights[i]->spotcos != 0.0f)
				{
					glLightfv(GL_LIGHT1 + i, GL_SPOT_DIRECTION, flForward);
					glLightf(GL_LIGHT1 + i, GL_SPOT_CUTOFF, m_pModelLights[i]->spotcos * 0.5);
				}
				else
				{
					glLightfv(GL_LIGHT1 + i, GL_SPOT_DIRECTION, g_vecZero);
					glLightf(GL_LIGHT1 + i, GL_SPOT_CUTOFF, 180);
				}
			}
		}
	}

	if (m_pCvarModelsLightDebug->value > 0)
	{
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_FRAGMENT_PROGRAM_ARB);
	}
}

/*
====================
StudioRestoreRenderer

====================
*/
void CStudioModelRenderer::StudioRestoreRenderer()
{
	if (m_bUseBlending || m_bChromeShell)
	{
		glDepthMask(GL_TRUE);
		glDisable(GL_BLEND);
	}

	glShadeModel(GL_FLAT);
	glColor4f(GL_ONE, GL_ONE, GL_ONE, GL_ONE);
	glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	if (m_pCvarModelsLightDebug->value > 0)
		glEnable(GL_TEXTURE_2D);

	if (!m_bChromeShell)
	{
		if (gBSPRenderer.m_bShaderSupport && m_pCvarModelShaders->value > 0)
		{
			glDisable(GL_FRAGMENT_PROGRAM_ARB);
			glDisable(GL_VERTEX_PROGRAM_ARB);
		}
		else
		{
			for (int i = 0; i < m_iNumModelLights; i++)
				glDisable(GL_LIGHT1 + i);

			glDisable(GL_LIGHTING);
			glDisable(GL_LIGHT0);
		}
	}
}

/*
====================
StudioSetupLighting

====================
*/
void CStudioModelRenderer::StudioSetupLighting(bool bStatic)
{
	int iret = 0;
	Vector color;
	Vector end;
	Vector point;

	entextrainfo_t* pInfo = (entextrainfo_t*)m_pCurrentEntity->topnode;

	Vector eorigin;
	eorigin[0] = (*m_protationmatrix)[0][3];
	eorigin[1] = (*m_protationmatrix)[1][3];
	eorigin[2] = (*m_protationmatrix)[2][3];

	if (pInfo == nullptr)
	{
		if (m_pCurrentEntity->index > 0 && m_pCurrentEntity != gEngfuncs.GetViewModel())
		{
			pInfo = StudioAllocExtraInfo();
			pInfo->pEntity = m_pCurrentEntity;

			m_pCurrentEntity->topnode = (mnode_s*)pInfo;
		}
	}
	else
	{
		msurface_t* msurf = &gBSPRenderer.m_pWorld->surfaces[pInfo->surfindex];
		clientsurfdata_t* csurf = &gBSPRenderer.m_pSurfaces[pInfo->surfindex];

		int i = 0;
		for (; i < MAXLIGHTMAPS && msurf->styles[i] != 255; i++)
		{
			if (csurf->cached_light[i] != pInfo->lightstyles[i])
				break;
		}

		if (pInfo->prevpos == eorigin && i == MAXLIGHTMAPS && pInfo->run_count >= 2)
		{
			memcpy(&m_pLighting, &pInfo->pLighting, sizeof(lighting_ext));
			pInfo->run_count++;
			return;
		}
	}

	if (m_pCurrentEntity->model != nullptr)
	{
		if ((m_pCurrentEntity->curstate.effects & EF_INVLIGHT) != 0)
			point = eorigin - Vector(0, 0, 5);
		else
			point = eorigin + Vector(0, 0, 5);
	}
	else
	{
		if ((m_pCurrentEntity->curstate.effects & EF_INVLIGHT) != 0)
			point = pInfo->pExtraData->lightorigin - Vector(0, 0, 5);
		else
			point = pInfo->pExtraData->lightorigin + Vector(0, 0, 5);
	}

	end.x = point.x;
	end.y = point.y;

	if ((m_pCurrentEntity->curstate.effects & EF_INVLIGHT) != 0)
		end.z = point.z + 8136;
	else
		end.z = point.z - 8136;

	if (gBSPRenderer.m_pWorld->lightdata != nullptr)
		iret = StudioRecursiveLightPoint(pInfo, gBSPRenderer.m_pWorld->nodes, point, end, color, bStatic);

	if (iret == 0)
	{
		m_pLighting.diffuselight.x = ((float)m_pCvarSkyColorX->value / 255) * 0.55;
		m_pLighting.diffuselight.y = ((float)m_pCvarSkyColorY->value / 255) * 0.55;
		m_pLighting.diffuselight.z = ((float)m_pCvarSkyColorZ->value / 255) * 0.55;

		m_pLighting.ambientlight.x = ((float)m_pCvarSkyColorX->value / 255) * 0.45;
		m_pLighting.ambientlight.y = ((float)m_pCvarSkyColorY->value / 255) * 0.45;
		m_pLighting.ambientlight.z = ((float)m_pCvarSkyColorZ->value / 255) * 0.45;

		m_pLighting.lightdir.x = m_pCvarSkyVecX->value;
		m_pLighting.lightdir.y = m_pCvarSkyVecY->value;
		m_pLighting.lightdir.z = m_pCvarSkyVecZ->value;
		return;
	}


	m_pLighting.diffuselight.x = color.x * 0.55;
	m_pLighting.diffuselight.y = color.y * 0.55;
	m_pLighting.diffuselight.z = color.z * 0.55;

	m_pLighting.ambientlight.x = color.x * 0.45;
	m_pLighting.ambientlight.y = color.y * 0.45;
	m_pLighting.ambientlight.z = color.z * 0.45;

	m_pLighting.lightdir.x = 0;
	m_pLighting.lightdir.y = 0;
	m_pLighting.lightdir.z = -1;

	if (pInfo != nullptr)
	{
		memcpy(&pInfo->pLighting, &m_pLighting, sizeof(lighting_ext));
		if (m_pCurrentEntity->model == nullptr)
			pInfo->prevpos = eorigin;
		else
			pInfo->prevpos = m_pCurrentEntity->origin;
	}
}

/*
====================
StudioRecursiveLightPoint

====================
*/
int CStudioModelRenderer::StudioRecursiveLightPoint(entextrainfo_t* ext, mnode_t* node, const Vector& start, const Vector& end, Vector& color, bool bStatic, bool isParticle)
{
	float front, back, frac;
	int side;
	mplane_t* plane;
	Vector mid;
	msurface_t* surf;
	int s, t, ds, dt;
	int i;
	mtexinfo_t* tex;
	color24* lightmap;

	if (node->contents < 0)
		return FALSE; // didn't hit anything

	plane = node->plane;
	front = DotProduct(start, plane->normal) - plane->dist;
	back = DotProduct(end, plane->normal) - plane->dist;
	side = static_cast<int>(front < 0);

	if (static_cast<int>(back < 0) == side)
		return StudioRecursiveLightPoint(ext, node->children[side], start, end, color, bStatic);

	frac = front / (front - back);
	mid[0] = start[0] + (end[0] - start[0]) * frac;
	mid[1] = start[1] + (end[1] - start[1]) * frac;
	mid[2] = start[2] + (end[2] - start[2]) * frac;

	// go down front side
	int r = StudioRecursiveLightPoint(ext, node->children[side], start, mid, color, bStatic);

	if (r != 0)
		return TRUE;

	if (static_cast<int>(back < 0) == side)
		return FALSE;

	model_t* world = gBSPRenderer.m_pWorld;
	surf = world->surfaces + node->firstsurface;
	for (i = 0; i < node->numsurfaces; i++, surf++)
	{
		if ((surf->flags & (SURF_DRAWTILED | SURF_DRAWSKY)) != 0)
			continue; // no lightmaps

		int index = node->firstsurface + i;
		tex = surf->texinfo;
		s = DotProduct(mid, tex->vecs[0]) + tex->vecs[0][3];
		t = DotProduct(mid, tex->vecs[1]) + tex->vecs[1][3];

		if (s < surf->texturemins[0] ||
			t < surf->texturemins[1])
			continue;

		ds = s - surf->texturemins[0];
		dt = t - surf->texturemins[1];

		if (ds > surf->extents[0] || dt > surf->extents[1])
			continue;

		if (surf->samples == nullptr)
			continue;

		ds >>= 4;
		dt >>= 4;

		lightmap = surf->samples;
		if ((lightmap != nullptr) && (m_pCurrentEntity != nullptr))
		{
			int surfindex = node->firstsurface + i;
			int size = ((surf->extents[1] >> 4) + 1) * ((surf->extents[0] >> 4) + 1);
			lightmap += dt * ((surf->extents[0] >> 4) + 1) + ds;

			float flIntensity = (lightmap->r + lightmap->g + lightmap->b) / 3;
			float flScale = flIntensity / 50;

			if (flScale > 1.0)
				flScale = 1.0;

			// bacontsu - smoothed lightmap while moving
			// scan for existing index
			bool bFoundStoredLight = false;
			size_t iFoundIndex = 0;

			// make sure we're not lerping particles
			if (!isParticle)
			{
				for (size_t k = 0; k < StoredLightBuffer.size(); k++)
				{
					if (StoredLightBuffer[k].index == m_pCurrentEntity->index)
					{
						bFoundStoredLight = true;
						iFoundIndex = k;
					}
				}

				// not found? add!
				if (!bFoundStoredLight)
				{
					cl_stored_light local;
					local.index = m_pCurrentEntity->index;
					local.color = Vector((float)(lightmap->r * flScale) / 255, (float)(lightmap->g * flScale) / 255, (float)(lightmap->b * flScale) / 255);
					StoredLightBuffer.push_back(local);
				}
			}


			if (bFoundStoredLight)
			{
				StoredLightBuffer[iFoundIndex].color.x = FranUtils::Maths::FastLerp(gHUD.m_flTimeDelta * 2.0f, StoredLightBuffer [iFoundIndex].color.x, (float)(lightmap->r * flScale) / 255);
				StoredLightBuffer[iFoundIndex].color.y = FranUtils::Maths::FastLerp(gHUD.m_flTimeDelta * 2.0f, StoredLightBuffer[iFoundIndex].color.y, (float)(lightmap->g * flScale) / 255);
				StoredLightBuffer[iFoundIndex].color.z = FranUtils::Maths::FastLerp(gHUD.m_flTimeDelta * 2.0f, StoredLightBuffer[iFoundIndex].color.z, (float)(lightmap->b * flScale) / 255);

				color.x = StoredLightBuffer[iFoundIndex].color.x;
				color.y = StoredLightBuffer[iFoundIndex].color.y;
				color.z = StoredLightBuffer[iFoundIndex].color.z;
			}
			else
			{
				color[0] = (float)(lightmap->r * flScale) / 255;
				color[1] = (float)(lightmap->g * flScale) / 255;
				color[2] = (float)(lightmap->b * flScale) / 255;
			}


			// if(bStatic)
			// gEngfuncs.Con_DPrintf("rgb val: %f %f %f\n", (float)(int)lightmap->r, (float)(int)lightmap->g, (float)(int)lightmap->b);

			if (ext != nullptr)
				ext->lightstyles[0] = gBSPRenderer.GetLightStyleValue(surf->styles[0]);

			if (ext != nullptr)
				ext->surfindex = node->firstsurface + i;
		}
		else
		{
			color[0] = color[1] = color[2] = 1.0;
		}
		return TRUE;
	}

	// go down back side
	return StudioRecursiveLightPoint(ext, node->children[side == 0], mid, end, color, bStatic);
}

/*
====================
StudioCullBBox

====================
*/
bool CStudioModelRenderer::StudioCullBBox(const Vector& mins, const Vector& maxs)
{
	if (m_vMins[0] > maxs[0])
		return true;

	if (m_vMins[1] > maxs[1])
		return true;

	if (m_vMins[2] > maxs[2])
		return true;

	if (m_vMaxs[0] < mins[0])
		return true;

	if (m_vMaxs[1] < mins[1])
		return true;

	if (m_vMaxs[2] < mins[2])
		return true;

	return false;
}

/*
====================
StudioEntityLight

====================
*/
void CStudioModelRenderer::StudioEntityLight()
{
	pmtrace_t pmtrace;
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);

	Vector vCenter;
	VectorAdd(m_vMins, m_vMaxs, vCenter);
	VectorScale(vCenter, 0.5, vCenter);

	m_iNumModelLights = NULL;
	mlight_t* mlight = &gBSPRenderer.m_pModelLights[0];

	for (int i = 0; i < gBSPRenderer.m_iNumModelLights; i++, mlight++)
	{
		if (!mlight->flashlight)
		{
			if (m_iNumModelLights == MAX_MODEL_LIGHTS)
				continue;
		}

		if (mlight->radius != 0.0f)
		{
			if (mlight->spotcos != 0.0f)
			{
				if (mlight->frustum->CullBox(m_vMins, m_vMaxs) && !(mlight->flashlight && (m_pCurrentEntity == gEngfuncs.GetViewModel())))
					continue;
			}
			else
			{
				if (StudioCullBBox(mlight->mins, mlight->maxs))
					continue;
			}

			// perform trace
			gEngfuncs.pEventAPI->EV_PlayerTrace(vCenter, mlight->origin, PM_WORLD_ONLY, -1, &pmtrace);

			if (pmtrace.fraction < 1.0 && (pmtrace.startsolid == 0))
				continue; // blocked

			if (mlight->flashlight && m_iNumModelLights == MAX_MODEL_LIGHTS)
			{
				// Flashlight must get on this list.
				m_pModelLights[(m_iNumModelLights - 1)] = mlight;
			}
			else
			{
				m_pModelLights[m_iNumModelLights] = mlight;
				m_iNumModelLights++;
			}
		}
	}
}

/*
====================
StudioSetChromeVectors

====================
*/
void CStudioModelRenderer::StudioSetChromeVectors()
{
	Vector tmp;
	Vector chromeupvec;
	Vector chromerightvec;
	float flSin = 0;

	if (m_bChromeShell)
		flSin = sin(gEngfuncs.GetClientTime()) * m_pCvarGlowShellFreq->value;

	//mstudiobone_t* pbones = (mstudiobone_t*)((byte*)m_pStudioHeader + m_pStudioHeader->boneindex);
	for (int i = 0; i < m_pStudioHeader->numbones; i++)
	{
		// TODO: OPTIMISE THIS
		//
		// if(!(pbones[i].flags & STUDIO_HAS_CHROME) && !m_bChromeShell)
		// 	continue;

		VectorScale(gBSPRenderer.m_vRenderOrigin, -1, tmp);
		tmp[0] += (*m_pbonetransform)[i][0][3];
		tmp[1] += (*m_pbonetransform)[i][1][3];
		tmp[2] += (*m_pbonetransform)[i][2][3];

		VectorNormalizeFast(tmp);
		CrossProduct(tmp, m_vRight, chromeupvec);
		VectorNormalizeFast(chromeupvec);
		chromeupvec.z += flSin;
		CrossProduct(tmp, chromeupvec, chromerightvec);
		chromerightvec.z += flSin;
		VectorNormalizeFast(chromerightvec);

		VectorIRotate(chromeupvec, (*m_pbonetransform)[i], m_vChromeUp[i]);
		VectorIRotate(chromerightvec, (*m_pbonetransform)[i], m_vChromeRight[i]);
	}
}

/*
====================
StudioChromeForMesh

====================
*/
void CStudioModelRenderer::StudioChromeForMesh(int j, mstudiomesh_t* pmesh)
{
	byte* pnormbone = ((byte*)m_pStudioHeader + m_pSubModel->norminfoindex);
	Vector* pstudionorms = (Vector*)((byte*)m_pStudioHeader + m_pSubModel->normindex);

	float n;
	for (int i = 0, k = j; i < pmesh->numnorms; i++, k++)
	{
		DotProductSSE(&n, pstudionorms[k], m_vChromeRight[pnormbone[k]]);
		// bacontsu - fake specular
		m_fChrome[k][0] = (n + 1.0) * 32 + (m_pCurrentEntity == gEngfuncs.GetViewModel() ? gEngfuncs.GetLocalPlayer()->curstate.origin.x * 0.1f : 0) + (m_pCurrentEntity == gEngfuncs.GetViewModel() ? fabs(gEngfuncs.GetLocalPlayer()->curstate.angles[YAW]) * 0.25f : 0);

		DotProductSSE(&n, pstudionorms[k], m_vChromeUp[pnormbone[k]]);
		// bacontsu - fake specular
		m_fChrome[k][1] = (n + 1.0) * 32 + (m_pCurrentEntity == gEngfuncs.GetViewModel() ? gEngfuncs.GetLocalPlayer()->curstate.origin.y * 0.1f : 0);
	}

	// if (m_pCurrentEntity == gEngfuncs.GetViewModel())
	// gEngfuncs.Con_Printf("haha %f\n", gEngfuncs.GetLocalPlayer()->curstate.angles[YAW]);
}

/*
====================
StudioDrawPoints

====================
*/
void CStudioModelRenderer::StudioDrawPoints()
{
	if (m_pTextureHeader == nullptr)
		return;

	mstudiotexture_t* ptexture = (mstudiotexture_t*)((byte*)m_pTextureHeader + m_pTextureHeader->textureindex);
	mstudiomesh_t* pmeshes = (mstudiomesh_t*)((byte*)m_pStudioHeader + m_pSubModel->meshindex);

	Vector* pstudioverts = (Vector*)((byte*)m_pStudioHeader + m_pSubModel->vertindex);
	Vector* pstudionorms = (Vector*)((byte*)m_pStudioHeader + m_pSubModel->normindex);

	byte* pvertbone = ((byte*)m_pStudioHeader + m_pSubModel->vertinfoindex);
	byte* pnormbone = ((byte*)m_pStudioHeader + m_pSubModel->norminfoindex);

	int skinnum = m_pCurrentEntity->curstate.skin;

	if (skinnum < 0)
		skinnum = 0;

	short* pskinref = (short*)((byte*)m_pTextureHeader + m_pTextureHeader->skinindex);

	if (skinnum != 0 && skinnum < m_pTextureHeader->numskinfamilies)
		pskinref += (skinnum * m_pTextureHeader->numskinref);

	//
	// Rotate normals by bone matrices.
	//
	for (int i = 0; i < m_pSubModel->numnorms; i++)
		VectorRotateSSE((float*)pstudionorms[i], (*m_pbonetransform)[pnormbone[i]], m_pNormalTransform[i]);

	if (!m_bChromeShell)
	{
		//
		// Transform vetrices by bone matrices.
		//
		for (int i = 0; i < m_pSubModel->numverts; i++)
			VectorTransformSSE(pstudioverts[i], (*m_pbonetransform)[pvertbone[i]], m_pVertexTransform[i]);
	}
	else
	{
		Vector vTemp;
		float flScale = 1.0 + ((float)m_pCurrentEntity->curstate.renderamt / 255);

		for (int i = 0; i < m_pSubModel->numverts; i++)
		{
			VectorScale(pstudioverts[i], flScale, vTemp);
			VectorTransformSSE(vTemp, (*m_pbonetransform)[pvertbone[i]], m_pVertexTransform[i]);
		}
	}
	//
	// Calculate chrome texcoords
	//
	for (int i = 0, j = 0; i < m_pSubModel->nummesh; i++)
	{
		//if ((stristr(ptexture[pskinref[pmeshes[i].skinref]].name, "chrome") != NULL) || m_bChromeShell)
		if (((ptexture[pskinref[pmeshes[i].skinref]].flags & STUDIO_NF_CHROME) != 0) || m_bChromeShell)
			StudioChromeForMesh(j, &pmeshes[i]);

		// Increment anyway
		j += pmeshes[i].numnorms;
	}

	//
	// Render meshes
	//
	for (int j = 0; j < m_pSubModel->nummesh; j++)
	{
		mstudiotexture_t* ptex = &ptexture[pskinref[pmeshes[j].skinref]];

		if (((ptex->flags & STUDIO_NF_ADDITIVE) != 0) && !m_bUseBlending)
			continue;

		if (((ptex->flags & STUDIO_NF_ALPHATEST) != 0) && !m_bUseBlending)
		{
			glEnable(GL_ALPHA_TEST);
			glAlphaFunc(GL_GREATER, 0.5);
		}

		StudioDrawMesh(&pmeshes[j], ptex);

		if (((ptex->flags & STUDIO_NF_ALPHATEST) != 0) && !m_bUseBlending)
		{
			glDisable(GL_ALPHA_TEST);
			glAlphaFunc(GL_GREATER, 0);
		}
	}

	if (!m_bUseBlending)
	{
		if (gHUD.m_pFogSettings.active)
			glFogfv(GL_FOG_COLOR, g_vecZero);

		glEnable(GL_BLEND);
		glDepthMask(GL_FALSE);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);

		for (int j = 0; j < m_pSubModel->nummesh; j++)
		{
			mstudiotexture_t* ptex = &ptexture[pskinref[pmeshes[j].skinref]];

			if ((ptex->flags & STUDIO_NF_ADDITIVE) == 0)
				continue;

			StudioDrawMesh(&pmeshes[j], ptex);
		}

		glDepthMask(GL_TRUE);
		glDisable(GL_BLEND);

		if (gHUD.m_pFogSettings.active)
			glFogfv(GL_FOG_COLOR, gHUD.m_pFogSettings.color);
	}
}

/*
====================
StudioDrawMesh

====================
*/
void CStudioModelRenderer::StudioDrawMesh(mstudiomesh_t* pmesh, mstudiotexture_t* ptex)
{
	if (gBSPRenderer.m_bShaderSupport && m_pCvarModelShaders->value > 0 && ((ptex->flags & STUDIO_NF_FULLBRIGHT) == 0) && !m_bChromeShell)
	{
		gBSPRenderer.glProgramLocalParameter4fARB(GL_VERTEX_PROGRAM_ARB, 11, 1.0 / (float)ptex->width, 1.0 / (float)ptex->height, 0, 0);
	}
	else
	{
		glMatrixMode(GL_TEXTURE);
		glScalef(1.0 / (float)ptex->width, 1.0 / (float)ptex->height, 1);
	}

	if ((ptex->flags & STUDIO_NF_FULLBRIGHT) != 0)
	{
		glColor4f(0.5, 0.5, 0.5, m_fAlpha);

		if (m_pCvarModelShaders->value > 0 && gBSPRenderer.m_bShaderSupport)
		{
			glDisable(GL_VERTEX_PROGRAM_ARB);
			glDisable(GL_VERTEX_PROGRAM_ARB);
		}
		else
		{
			glDisable(GL_LIGHTING);
		}
	}

	if (!m_bChromeShell)
	{
		if (ptex->index != m_iCurrentBinding)
		{
			glBindTexture(GL_TEXTURE_2D, ptex->index);
			m_iCurrentBinding = ptex->index;
		}
	}

	int i;
	short* ptricmds = (short*)((byte*)m_pStudioHeader + pmesh->triindex);

	//if((stristr(ptex->name, "chrome") != NULL) || m_bChromeShell)
	if (((ptex->flags & STUDIO_NF_CHROME) != 0) || m_bChromeShell)
	{
		while ((i = *(ptricmds++)) != 0)
		{
			if (i < 0)
			{
				glBegin(GL_TRIANGLE_FAN);
				i = -i;
			}
			else
			{
				glBegin(GL_TRIANGLE_STRIP);
			}

			for (; i > 0; i--, ptricmds += 4)
			{
				glTexCoord2f(m_fChrome[ptricmds[1]][0], m_fChrome[ptricmds[1]][1]);
				glNormal3fv(m_vNormalTransform[ptricmds[1]]);
				glVertex3fv(m_vVertexTransform[ptricmds[0]]);
			}
			glEnd();
		}
	}
	else
	{
		while ((i = *(ptricmds++)) != 0)
		{
			if (i < 0)
			{
				glBegin(GL_TRIANGLE_FAN);
				i = -i;
			}
			else
			{
				glBegin(GL_TRIANGLE_STRIP);
			}

			for (; i > 0; i--, ptricmds += 4)
			{
				glTexCoord2f(ptricmds[2], ptricmds[3]);
				glNormal3fv(m_vNormalTransform[ptricmds[1]]);
				glVertex3fv(m_vVertexTransform[ptricmds[0]]);
			}
			glEnd();
		}
	}

	if (!gBSPRenderer.m_bShaderSupport || m_pCvarModelShaders->value < 1 || ((ptex->flags & STUDIO_NF_FULLBRIGHT) != 0) || m_bChromeShell)
	{
		glMatrixMode(GL_TEXTURE);
		glLoadIdentity();

		glMatrixMode(GL_MODELVIEW);
	}

	if ((ptex->flags & STUDIO_NF_FULLBRIGHT) != 0)
	{
		glColor4f(GL_ONE, GL_ONE, GL_ONE, m_fAlpha);

		if ((m_pCvarModelShaders->value != 0.0f) && gBSPRenderer.m_bShaderSupport)
		{
			glEnable(GL_VERTEX_PROGRAM_ARB);
			glEnable(GL_VERTEX_PROGRAM_ARB);
		}
		else
		{
			glEnable(GL_LIGHTING);
		}
	}

	gBSPRenderer.m_iStudioPolyCounter += pmesh->numtris;
}

/*
====================
StudioCheckBBox

====================
*/
qboolean CStudioModelRenderer::StudioCheckBBox()
{
	// Build full bounding box
	mstudioseqdesc_t* pseqdesc = (mstudioseqdesc_t*)((byte*)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->curstate.sequence;

	Vector vTemp;
	static Vector vBounds[8];

	for (int i = 0; i < 8; i++)
	{
		Vector vTemp;

		if ((i & 1) != 0)
			vTemp[0] = pseqdesc->bbmin[0];
		else
			vTemp[0] = pseqdesc->bbmax[0];

		if ((i & 2) != 0)
			vTemp[1] = pseqdesc->bbmin[1];
		else
			vTemp[1] = pseqdesc->bbmax[1];

		if ((i & 4) != 0)
			vTemp[2] = pseqdesc->bbmin[2];
		else
			vTemp[2] = pseqdesc->bbmax[2];

		VectorCopy(vTemp, vBounds[i]);
	}

	m_pCurrentEntity->angles[PITCH] = -m_pCurrentEntity->angles[PITCH];
	AngleMatrix(m_pCurrentEntity->angles, (*m_protationmatrix));
	m_pCurrentEntity->angles[PITCH] = -m_pCurrentEntity->angles[PITCH];

	for (int i = 0; i < 8; i++)
	{
		VectorCopy(vBounds[i], vTemp);
		VectorRotateSSE(vTemp, (*m_protationmatrix), vBounds[i]);
	}

	if (m_pCvarModelsBBoxDebug->value > 0)
	{
		glDisable(GL_CULL_FACE);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glDisable(GL_TEXTURE_2D);
		glBegin(GL_TRIANGLE_STRIP);
		glColor3f(GL_ONE, GL_ONE, GL_ONE);

		for (int i = 0; i < 8; i++)
			glVertex3fv(m_pCurrentEntity->origin + vBounds[i]);

		glEnd();
		glEnable(GL_TEXTURE_2D);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glEnable(GL_CULL_FACE);
	}

	// Set the bounding box
	Vector vMins(9999, 9999, 9999);
	Vector vMaxs(-9999, -9999, -9999);
	for (int i = 0; i < 8; i++)
	{
		// Mins
		if (vBounds[i][0] < vMins[0])
			vMins[0] = vBounds[i][0];
		if (vBounds[i][1] < vMins[1])
			vMins[1] = vBounds[i][1];
		if (vBounds[i][2] < vMins[2])
			vMins[2] = vBounds[i][2];

		// Maxs
		if (vBounds[i][0] > vMaxs[0])
			vMaxs[0] = vBounds[i][0];
		if (vBounds[i][1] > vMaxs[1])
			vMaxs[1] = vBounds[i][1];
		if (vBounds[i][2] > vMaxs[2])
			vMaxs[2] = vBounds[i][2];
	}

	// Make sure stuff like barnacles work fine
	if (m_pStudioHeader->numbonecontrollers != 0)
	{
		mstudiobonecontroller_t* pbonecontroller = (mstudiobonecontroller_t*)((byte*)m_pStudioHeader + m_pStudioHeader->bonecontrollerindex);
		byte* pcontroller1 = m_pCurrentEntity->curstate.controller;
		byte* pcontroller2 = m_pCurrentEntity->latched.prevcontroller;
		float flInterp = StudioEstimateInterpolant();

		for (int j = 0; j < m_pStudioHeader->numbonecontrollers; j++)
		{
			if ((pbonecontroller[j].type & STUDIO_Y) == 0)
				continue;

			if ((pbonecontroller[j].type & STUDIO_RLOOP) != 0)
				continue;

			int iIndex = pbonecontroller[j].index;
			float flValue = (pcontroller1[iIndex] * flInterp + pcontroller2[iIndex] * (1.0 - flInterp)) / 255.0;

			if (flValue < 0)
				flValue = 0;
			if (flValue > 1)
				flValue = 1;

			vMins[2] += (1.0 - flValue) * pbonecontroller[j].start + flValue * pbonecontroller[j].end;
		}
	}

	// Add in origin
	VectorAdd(m_pCurrentEntity->origin, vMins, m_vMins);
	VectorAdd(m_pCurrentEntity->origin, vMaxs, m_vMaxs);

	// Copy it over to the entity
	VectorCopy(vMins, m_pCurrentEntity->curstate.mins);
	VectorCopy(vMaxs, m_pCurrentEntity->curstate.maxs);

	// View entity is always present
	if (m_pCurrentEntity == gEngfuncs.GetViewModel())
		return 0;

	return static_cast<qboolean>(gHUD.viewFrustum.CullBox(m_vMins, m_vMaxs));
}

/*
====================
StudioDrawBBox

====================
*/
void CStudioModelRenderer::StudioDrawBBox()
{
	Vector v[8];

	v[0][0] = m_vMins[0];
	v[0][1] = m_vMaxs[1];
	v[0][2] = m_vMins[2];
	v[1][0] = m_vMins[0];
	v[1][1] = m_vMins[1];
	v[1][2] = m_vMins[2];
	v[2][0] = m_vMaxs[0];
	v[2][1] = m_vMaxs[1];
	v[2][2] = m_vMins[2];
	v[3][0] = m_vMaxs[0];
	v[3][1] = m_vMins[1];
	v[3][2] = m_vMins[2];

	v[4][0] = m_vMaxs[0];
	v[4][1] = m_vMaxs[1];
	v[4][2] = m_vMaxs[2];
	v[5][0] = m_vMaxs[0];
	v[5][1] = m_vMins[1];
	v[5][2] = m_vMaxs[2];
	v[6][0] = m_vMins[0];
	v[6][1] = m_vMaxs[1];
	v[6][2] = m_vMaxs[2];
	v[7][0] = m_vMins[0];
	v[7][1] = m_vMins[1];
	v[7][2] = m_vMaxs[2];

	glDisable(GL_CULL_FACE);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glDisable(GL_TEXTURE_2D);

	glBegin(GL_QUAD_STRIP);
	for (int i = 0; i < 10; i++)
	{
		glColor4f(0, 0, 1.0, 0.5);
		glVertex3fv(v[i & 7]);
	}
	glEnd();

	glBegin(GL_QUAD_STRIP);
	glColor4f(0, 0, 1.0, 0.5);
	glVertex3fv(v[6]);
	glColor4f(0, 0, 1.0, 0.5);
	glVertex3fv(v[0]);
	glColor4f(0, 0, 1.0, 0.5);
	glVertex3fv(v[4]);
	glColor4f(0, 0, 1.0, 0.5);
	glVertex3fv(v[2]);
	glEnd();

	glBegin(GL_QUAD_STRIP);
	glColor4f(0, 0, 1.0, 0.5);
	glVertex3fv(v[1]);
	glColor4f(0, 0, 1.0, 0.5);
	glVertex3fv(v[7]);
	glColor4f(0, 0, 1.0, 0.5);
	glVertex3fv(v[3]);
	glColor4f(0, 0, 1.0, 0.5);
	glVertex3fv(v[5]);
	glEnd();

	glEnable(GL_TEXTURE_2D);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glEnable(GL_CULL_FACE);
}

/*
====================
StudioDrawExternalEntity

====================
*/
void CStudioModelRenderer::StudioDrawExternalEntity(cl_entity_t* pEntity)
{
	Vector vRealOrigin;
	Vector vTransOrigin;

	m_pCurrentEntity = pEntity;
	IEngineStudio.GetTimes(&m_nFrameCount, &m_clTime, &m_clOldTime);
	IEngineStudio.GetViewInfo(m_vRenderOrigin, m_vUp, m_vRight, m_vNormal);

	entextradata_t* pExtraData = ((entextrainfo_t*)m_pCurrentEntity->topnode)->pExtraData;
	m_pStudioHeader = pExtraData->pModelData->pHdr;
	m_pTextureHeader = pExtraData->pModelData->pTexHdr;
	m_pVBOHeader = &pExtraData->pModelData->pVBOHeader;

	if ((m_pStudioHeader == nullptr) || (m_pTextureHeader == nullptr) || (m_pVBOHeader == nullptr))
		return;

	if (m_pStudioHeader->numbodyparts == 0)
		return;

	m_bExternalEntity = true;

	VectorCopy(pExtraData->absmin, m_vMins);
	VectorCopy(pExtraData->absmax, m_vMaxs);

	if (m_pCurrentEntity->curstate.renderfx == 70)
	{
		if (gBSPRenderer.m_fSkySpeed == 0.0f)
		{
			vTransOrigin = (m_pCurrentEntity->origin - gBSPRenderer.m_vSkyOrigin) + gBSPRenderer.m_vRenderOrigin;
		}
		else
		{
			vTransOrigin = (m_pCurrentEntity->origin - gBSPRenderer.m_vSkyOrigin) + m_vRenderOrigin;
			vTransOrigin = vTransOrigin - (gBSPRenderer.m_vRenderOrigin - gBSPRenderer.m_vSkyWorldOrigin) / gBSPRenderer.m_fSkySpeed;
		}

		VectorCopy(m_pCurrentEntity->origin, vRealOrigin);
		VectorCopy(vTransOrigin, m_pCurrentEntity->origin);
	}
	else
	{
		if (gHUD.viewFrustum.CullBox(m_vMins, m_vMaxs))
			return;
	}

	m_pCurrentEntity->angles[PITCH] = -m_pCurrentEntity->angles[PITCH];
	AngleMatrix(m_pCurrentEntity->angles, (*m_protationmatrix));
	m_pCurrentEntity->angles[PITCH] = -m_pCurrentEntity->angles[PITCH];
	StudioSetupLighting(true);
	StudioEntityLight();
	StudioRenderModelEXT();

	// Restore origin
	if (m_pCurrentEntity->curstate.renderfx == 70)
		VectorCopy(vRealOrigin, m_pCurrentEntity->origin);

	m_bExternalEntity = false;
}

/*
====================
StudioSaveModelData

====================
*/
void CStudioModelRenderer::StudioSaveModelData(modeldata_t* pExtraData)
{
	if (m_pTextureHeader == nullptr)
		return;

	mstudiobodyparts_t* bp = (mstudiobodyparts_t*)((byte*)m_pStudioHeader + m_pStudioHeader->bodypartindex);

	int n = 0;
	for (int i = 0; i < m_pStudioHeader->numbodyparts; i++)
		n += bp[i].nummodels;

	if (n == 0)
		return;

	m_pVBOHeader = &pExtraData->pVBOHeader;
	m_pVBOHeader->numsubmodels = n;
	m_pVBOHeader->submodels = new vbosubmodel_t[n];
	memset(m_pVBOHeader->submodels, 0, sizeof(vbosubmodel_t) * n);

	m_pVertexTransform = &m_vVertexTransform[0];
	m_pNormalTransform = &m_vNormalTransform[0];

	n = 0;
	for (int i = 0; i < m_pStudioHeader->numbodyparts; i++)
	{
		m_pSubModel = (mstudiomodel_t*)((byte*)m_pStudioHeader + bp[i].modelindex);
		for (int k = 0; k < bp[i].nummodels; k++)
		{
			vbosubmodel_t* pvbosubmodel = &pExtraData->pVBOHeader.submodels[n];
			n++;
			mstudiomesh_t* pmeshes = (mstudiomesh_t*)((byte*)m_pStudioHeader + m_pSubModel[k].meshindex);
			m_iCurStart = m_iNumRefVerts;

			pvbosubmodel->nummeshes = m_pSubModel[k].nummesh;
			pvbosubmodel->meshes = new vbomesh_t[m_pSubModel[k].nummesh];
			memset(pvbosubmodel->meshes, 0, sizeof(vbomesh_t) * m_pSubModel[k].nummesh);

			byte* pvertbone = ((byte*)m_pStudioHeader + m_pSubModel[k].vertinfoindex);
			byte* pnormbone = ((byte*)m_pStudioHeader + m_pSubModel[k].norminfoindex);

			Vector* pstudionorms = (Vector*)((byte*)m_pStudioHeader + m_pSubModel[k].normindex);
			Vector* pstudioverts = (Vector*)((byte*)m_pStudioHeader + m_pSubModel[k].vertindex);

			for (int j = 0; j < m_pSubModel[k].numnorms; j++)
				VectorRotateSSE((float*)pstudionorms[j], (*m_pbonetransform)[pnormbone[j]], m_pNormalTransform[j]);

			for (int j = 0; j < m_pSubModel[k].numverts; j++)
				VectorTransformSSE((float*)pstudioverts[j], (*m_pbonetransform)[pvertbone[j]], m_pVertexTransform[j]);

			for (int l = 0; l < m_pSubModel[k].nummesh; l++)
			{
				vbomesh_t* pvbomesh = &pvbosubmodel->meshes[l];
				pvbomesh->start_vertex = m_iNumIndexes;

				int j = 0;
				short* ptricmds = (short*)((byte*)m_pStudioHeader + pmeshes[l].triindex);
				while ((j = *(ptricmds++)) != 0)
				{
					if (j > 0)
					{
						// convert triangle strip
						j -= 3;
						studiovert_t indices[3];
						for (int i = 0; i < 3; i++, ptricmds += 4)
						{
							indices[i].vertindex = ptricmds[0];
							indices[i].normindex = ptricmds[1];
							indices[i].texcoord[0] = ptricmds[2];
							indices[i].texcoord[1] = ptricmds[3];
							StudioManageVertex(&indices[i]);
						}

						bool reverse = false;
						for (; j > 0; j--, ptricmds += 4)
						{
							indices[0] = indices[1];
							indices[1] = indices[2];
							indices[2].vertindex = ptricmds[0];
							indices[2].normindex = ptricmds[1];
							indices[2].texcoord[0] = ptricmds[2];
							indices[2].texcoord[1] = ptricmds[3];

							if (!reverse)
							{
								StudioManageVertex(&indices[2]);
								StudioManageVertex(&indices[1]);
								StudioManageVertex(&indices[0]);
							}
							else
							{
								StudioManageVertex(&indices[0]);
								StudioManageVertex(&indices[1]);
								StudioManageVertex(&indices[2]);
							}
							reverse = !reverse;
						}
					}
					else
					{
						// convert triangle fan
						j = -j - 3;
						studiovert_t indices[3];
						for (int i = 0; i < 3; i++, ptricmds += 4)
						{
							indices[i].vertindex = ptricmds[0];
							indices[i].normindex = ptricmds[1];
							indices[i].texcoord[0] = ptricmds[2];
							indices[i].texcoord[1] = ptricmds[3];
							StudioManageVertex(&indices[i]);
						}

						for (; j > 0; j--, ptricmds += 4)
						{
							indices[1] = indices[2];
							indices[2].vertindex = ptricmds[0];
							indices[2].normindex = ptricmds[1];
							indices[2].texcoord[0] = ptricmds[2];
							indices[2].texcoord[1] = ptricmds[3];

							StudioManageVertex(&indices[0]);
							StudioManageVertex(&indices[1]);
							StudioManageVertex(&indices[2]);
						}
					}
				}

				// Number of indexes to cache
				pvbomesh->num_vertexes = m_iNumIndexes - pvbomesh->start_vertex;
			}
		}
	}

	// Copy over all VBO vertexes
	m_pVBOHeader->numverts = m_iNumVBOVerts;
	m_pVBOHeader->pBufferData = new brushvertex_t[m_iNumVBOVerts];
	memcpy(m_pVBOHeader->pBufferData, &m_pVBOVerts, sizeof(brushvertex_t) * m_iNumVBOVerts);

	// Copy over index array
	m_pVBOHeader->numindexes = m_iNumIndexes;
	m_pVBOHeader->indexes = new unsigned int[m_iNumIndexes];
	memcpy(m_pVBOHeader->indexes, &m_usIndexes, sizeof(unsigned int) * m_iNumIndexes);

	// Reset this
	m_iNumRefVerts = NULL;
	m_iNumVBOVerts = NULL;
	m_iNumIndexes = NULL;
}

/*
====================
StudioManageVertex

====================
*/
void CStudioModelRenderer::StudioManageVertex(studiovert_t* pvert)
{
	for (int i = m_iCurStart; i < m_iNumRefVerts; i++)
	{
		if (pvert->normindex == m_pRefArray[i].normindex && pvert->vertindex == m_pRefArray[i].vertindex && pvert->texcoord[0] == m_pRefArray[i].texcoord[0] && pvert->texcoord[1] == m_pRefArray[i].texcoord[1])
		{
			m_usIndexes[m_iNumIndexes] = i;
			m_iNumIndexes++;
			return;
		}
	}

	m_usIndexes[m_iNumIndexes] = m_iNumRefVerts;
	m_iNumIndexes++;

	m_pRefArray[m_iNumRefVerts].vertindex = pvert->vertindex;
	m_pRefArray[m_iNumRefVerts].normindex = pvert->normindex;
	m_pRefArray[m_iNumRefVerts].texcoord[0] = pvert->texcoord[0];
	m_pRefArray[m_iNumRefVerts].texcoord[1] = pvert->texcoord[1];
	m_iNumRefVerts++;

	m_pVBOVerts[m_iNumVBOVerts].pos = m_pVertexTransform[pvert->vertindex];
	m_pVBOVerts[m_iNumVBOVerts].normal = m_pNormalTransform[pvert->normindex];
	m_pVBOVerts[m_iNumVBOVerts].texcoord[0] = pvert->texcoord[0];
	m_pVBOVerts[m_iNumVBOVerts].texcoord[1] = pvert->texcoord[1];
	m_iNumVBOVerts++;
}

/*
====================
StudioSaveUniqueData

====================
*/
void CStudioModelRenderer::StudioSaveUniqueData(entextradata_t* pExtraData)
{
	Vector vBounds[8];

	Vector vTemp;
	Vector vMins(9999, 9999, 9999);
	Vector vMaxs(-9999, -9999, -9999);

	m_pVBOHeader = &pExtraData->pModelData->pVBOHeader;
	m_pStudioHeader = pExtraData->pModelData->pHdr;

	m_pCurrentEntity->angles[PITCH] = -m_pCurrentEntity->angles[PITCH];
	AngleMatrix(m_pCurrentEntity->angles, (*m_protationmatrix));
	m_pCurrentEntity->angles[PITCH] = -m_pCurrentEntity->angles[PITCH];

	int baseindex = 0;
	for (int i = 0; i < m_pStudioHeader->numbodyparts; i++)
	{
		m_pBodyPart = (mstudiobodyparts_t*)((byte*)m_pStudioHeader + m_pStudioHeader->bodypartindex) + i;
		int index = (m_pCurrentEntity->curstate.body / m_pBodyPart->base) % m_pBodyPart->nummodels;

		m_pSubModel = (mstudiomodel_t*)((byte*)m_pStudioHeader + m_pBodyPart->modelindex) + index;
		m_pVBOSubModel = &m_pVBOHeader->submodels[baseindex + index];

		for (int j = 0; j < m_pVBOSubModel->nummeshes; j++)
		{
			vbomesh_t* pvbomesh = &m_pVBOSubModel->meshes[j];
			for (int k = 0; k < pvbomesh->num_vertexes; k++)
			{
				VectorRotateSSE(m_pVBOHeader->pBufferData[m_pVBOHeader->indexes[(pvbomesh->start_vertex + k)]].pos, (*m_protationmatrix), vTemp);

				if (m_pCurrentEntity->curstate.scale != 0.0f)
					VectorScale(vTemp, m_pCurrentEntity->curstate.scale, vTemp);

				if (vTemp.x < vMins.x)
					vMins.x = vTemp.x;
				if (vTemp.y < vMins.y)
					vMins.y = vTemp.y;
				if (vTemp.z < vMins.z)
					vMins.z = vTemp.z;

				if (vTemp.x > vMaxs.x)
					vMaxs.x = vTemp.x;
				if (vTemp.y > vMaxs.y)
					vMaxs.y = vTemp.y;
				if (vTemp.z > vMaxs.z)
					vMaxs.z = vTemp.z;
			}
		}

		baseindex += m_pBodyPart->nummodels;
	}

	m_bExternalEntity = true;
	StudioSetUpTransform(0);
	StudioSetupBones();

	memcpy(pExtraData->pbones, m_pbonetransform, sizeof(float) * 12 * pExtraData->pModelData->pHdr->numbones);
	m_bExternalEntity = false;

	VectorCopy(vMins, m_pCurrentEntity->curstate.mins);
	VectorCopy(vMaxs, m_pCurrentEntity->curstate.maxs);

	VectorAdd(vMins, m_pCurrentEntity->origin, pExtraData->absmin);
	VectorAdd(vMaxs, m_pCurrentEntity->origin, pExtraData->absmax);

	SV_FindTouchedLeafs(pExtraData, gBSPRenderer.m_pWorld->nodes);
}

/*
====================
StudioRenderModelEXT

====================
*/
void CStudioModelRenderer::StudioRenderModelEXT()
{
	// bacontsu - render distance, credit to Aynekko (Diffusion)
	if ((m_pCurrentEntity->curstate.origin - gEngfuncs.GetLocalPlayer()->curstate.origin).Length() > m_pCvarRenderDistance->value && m_pCurrentEntity != gEngfuncs.GetViewModel())
		return;

	if ((m_pCurrentEntity->curstate.origin - gEngfuncs.GetLocalPlayer()->curstate.origin).Length() > m_pCvarRenderDistance->value - 100.0f && m_pCurrentEntity != gEngfuncs.GetViewModel())
	{
		float diff = m_pCvarRenderDistance->value - (m_pCurrentEntity->curstate.origin - gEngfuncs.GetLocalPlayer()->curstate.origin).Length();

		m_pCurrentEntity->curstate.rendermode = kRenderTransTexture;
		m_pCurrentEntity->curstate.renderamt = diff * 255.0f / 100.0f;
	}
	else
	{
		m_pCurrentEntity->curstate.renderamt = m_pCurrentEntity->baseline.renderamt;
		m_pCurrentEntity->curstate.rendermode = m_pCurrentEntity->baseline.rendermode;
	}

	// Save texture states before rendering, so we don't
	// cause any bugs in HL by changing texture binds, etc
	R_SaveGLStates();

	// I don't give a shit, make sure
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);

	R_RotateForEntity(m_pCurrentEntity);
	StudioSetupRenderer(m_pCurrentEntity->curstate.rendermode);

	int baseindex = 0;
	for (int i = 0; i < m_pStudioHeader->numbodyparts; i++)
	{
		int index;
		m_pBodyPart = (mstudiobodyparts_t*)((byte*)m_pStudioHeader + m_pStudioHeader->bodypartindex) + i;

		index = m_pCurrentEntity->curstate.body / m_pBodyPart->base;
		index = index % m_pBodyPart->nummodels;

		m_pSubModel = (mstudiomodel_t*)((byte*)m_pStudioHeader + m_pBodyPart->modelindex) + index;
		m_pVBOSubModel = &m_pVBOHeader->submodels[baseindex + index];

		StudioDrawPointsEXT();
		baseindex += m_pBodyPart->nummodels;
	}

	StudioRestoreRenderer();
	StudioDrawDecals();

	if (gBSPRenderer.m_pCvarWireFrame->value != 0.0f)
		StudioDrawWireframeEXT();

	// Restore before drawing bbox
	glPopMatrix();

	if (m_pCvarModelsBBoxDebug->value > 0)
		StudioDrawBBox();

	// Restore saved states
	R_RestoreGLStates();
}

/*
====================
StudioDrawPointsEXT

====================
*/
void CStudioModelRenderer::StudioDrawPointsEXT()
{
	if (m_pTextureHeader == nullptr)
		return;

	int skinnum = m_pCurrentEntity->curstate.skin; // for short..

	if (skinnum < 0)
		skinnum = 0;

	short* pskinref = (short*)((byte*)m_pTextureHeader + m_pTextureHeader->skinindex);

	if (skinnum != 0 && skinnum < m_pTextureHeader->numskinfamilies)
		pskinref += (skinnum * m_pTextureHeader->numskinref);

	mstudiomesh_t* pmesh = (mstudiomesh_t*)((byte*)m_pStudioHeader + m_pSubModel->meshindex);
	mstudiotexture_t* ptexture = (mstudiotexture_t*)((byte*)m_pTextureHeader + m_pTextureHeader->textureindex);

	for (int i = 0; i < m_pSubModel->nummesh; i++)
	{
		vbomesh_t* pvbomesh = &m_pVBOSubModel->meshes[i];
		mstudiotexture_t* ptex = &ptexture[pskinref[pmesh[i].skinref]];

		if (((ptex->flags & STUDIO_NF_ADDITIVE) != 0) && !m_bUseBlending)
			continue;

		if (((ptex->flags & STUDIO_NF_ALPHATEST) != 0) && !m_bUseBlending)
		{
			glEnable(GL_ALPHA_TEST);
			glAlphaFunc(GL_GREATER, 0.5);
		}

		StudioDrawMeshEXT(ptex, pvbomesh);
		gBSPRenderer.m_iStudioPolyCounter += pmesh[i].numtris;

		if (((ptex->flags & STUDIO_NF_ALPHATEST) != 0) && !m_bUseBlending)
		{
			glDisable(GL_ALPHA_TEST);
			glAlphaFunc(GL_GREATER, 0);
		}
	}

	if (!m_bUseBlending)
	{
		if (gHUD.m_pFogSettings.active)
			glFogfv(GL_FOG_COLOR, g_vecZero);

		for (int i = 0; i < m_pSubModel->nummesh; i++)
		{
			vbomesh_t* pvbomesh = &m_pVBOSubModel->meshes[i];
			mstudiotexture_t* ptex = &ptexture[pskinref[pmesh[i].skinref]];

			if ((ptex->flags & STUDIO_NF_ADDITIVE) == 0) // buz
				continue;

			glEnable(GL_BLEND);
			glDepthMask(GL_FALSE);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);

			StudioDrawMeshEXT(ptex, pvbomesh);
			gBSPRenderer.m_iStudioPolyCounter += pmesh[i].numtris;

			glDepthMask(GL_TRUE);
			glDisable(GL_BLEND);
		}

		if (gHUD.m_pFogSettings.active)
			glFogfv(GL_FOG_COLOR, gHUD.m_pFogSettings.color);
	}
}

/*
====================
StudioDrawMeshEXT

====================
*/
#define BUFFER_OFFSET(i) ((unsigned int*)NULL + (i))
void CStudioModelRenderer::StudioDrawMeshEXT(mstudiotexture_t* ptex, vbomesh_t* pmesh)
{
	if (gBSPRenderer.m_bShaderSupport && m_pCvarModelShaders->value > 0 && ((ptex->flags & STUDIO_NF_FULLBRIGHT) == 0))
	{
		gBSPRenderer.glProgramLocalParameter4fARB(GL_VERTEX_PROGRAM_ARB, 11, 1.0 / (float)ptex->width, 1.0 / (float)ptex->height, 0, 0);
	}
	else
	{
		glMatrixMode(GL_TEXTURE);
		glScalef(1.0 / (float)ptex->width, 1.0 / (float)ptex->height, 1);
	}

	if ((ptex->flags & STUDIO_NF_FULLBRIGHT) != 0)
	{
		glColor4f(0.5, 0.5, 0.5, m_fAlpha);

		if ((m_pCvarModelShaders->value != 0.0f) && gBSPRenderer.m_bShaderSupport)
		{
			glDisable(GL_VERTEX_PROGRAM_ARB);
			glDisable(GL_VERTEX_PROGRAM_ARB);
		}
		else
		{
			glDisable(GL_LIGHTING);
		}
	}

	if (ptex->index != m_iCurrentBinding)
	{
		glBindTexture(GL_TEXTURE_2D, ptex->index);
		m_iCurrentBinding = ptex->index;
	}

	glDrawElements(GL_TRIANGLES, pmesh->num_vertexes, GL_UNSIGNED_INT, BUFFER_OFFSET(pmesh->start_vertex));

	if (!gBSPRenderer.m_bShaderSupport || m_pCvarModelShaders->value < 1 || ((ptex->flags & STUDIO_NF_FULLBRIGHT) != 0))
	{
		glMatrixMode(GL_TEXTURE);
		glLoadIdentity();

		glMatrixMode(GL_MODELVIEW);
	}

	if ((ptex->flags & STUDIO_NF_FULLBRIGHT) != 0)
	{
		glColor4f(GL_ONE, GL_ONE, GL_ONE, m_fAlpha);

		if ((m_pCvarModelShaders->value != 0.0f) && gBSPRenderer.m_bShaderSupport)
		{
			glEnable(GL_VERTEX_PROGRAM_ARB);
			glEnable(GL_VERTEX_PROGRAM_ARB);
		}
		else
		{
			glEnable(GL_LIGHTING);
		}
	}
}

/*
====================
StudioDrawWireframeEXT

====================
*/
void CStudioModelRenderer::StudioDrawWireframeEXT()
{
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_CULL_FACE);
	glColor4f(0.0, 0.0, 1.0, 1.0);
	glLineWidth(1);

	if (gBSPRenderer.m_pCvarWireFrame->value >= 3)
	{
		glDisable(GL_DEPTH_TEST);

		if (gHUD.m_pFogSettings.active)
			glDisable(GL_FOG);
	}

	int baseindex = 0;
	for (int i = 0; i < m_pStudioHeader->numbodyparts; i++)
	{
		int index;
		m_pBodyPart = (mstudiobodyparts_t*)((byte*)m_pStudioHeader + m_pStudioHeader->bodypartindex) + i;

		index = m_pCurrentEntity->curstate.body / m_pBodyPart->base;
		index = index % m_pBodyPart->nummodels;

		m_pSubModel = (mstudiomodel_t*)((byte*)m_pStudioHeader + m_pBodyPart->modelindex) + index;
		m_pVBOSubModel = &m_pVBOHeader->submodels[baseindex + index];

		StudioDrawPointsEXT();
		baseindex += m_pBodyPart->nummodels;
	}

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_CULL_FACE);

	if (gBSPRenderer.m_pCvarWireFrame->value >= 2)
	{
		glEnable(GL_DEPTH_TEST);

		if (gHUD.m_pFogSettings.active)
			glEnable(GL_FOG);
	}
}

/*
====================
StudioAllocDecalSlot

====================
*/
studiodecal_t* CStudioModelRenderer::StudioAllocDecalSlot()
{
	if (m_iNumStudioDecals == MAX_CUSTOMDECALS)
		m_iNumStudioDecals = NULL;

	studiodecal_t* pDecal = &m_pStudioDecals[m_iNumStudioDecals];
	m_iNumStudioDecals++;

	if (pDecal->numverts != 0)
	{
		delete[] pDecal->verts;
		pDecal->verts = nullptr;
		pDecal->numverts = 0;
	}

	if (pDecal->numpolys != 0)
	{
		for (int i = 0; i < pDecal->numpolys; i++)
			delete[] pDecal->polys[i].verts;

		delete[] pDecal->polys;
		pDecal->polys = nullptr;
		pDecal->numpolys = 0;
	}

	// Make sure nothing references this decal
	for (int i = 0; i < m_iNumStudioDecals; i++)
	{
		if (m_pStudioDecals[i].next == pDecal)
			m_pStudioDecals[i].next = pDecal->next;
	}

	memset(pDecal, 0, sizeof(studiodecal_t));
	return pDecal;
};

/*
====================
StudioAllocDecal

====================
*/
studiodecal_t* CStudioModelRenderer::StudioAllocDecal()
{
	if (m_pCurrentEntity->efrag == nullptr)
	{
		studiodecal_t* pDecal = StudioAllocDecalSlot();
		pDecal->totaldecals = 1;

		m_pCurrentEntity->efrag = (struct efrag_s*)pDecal;
		return pDecal;
	}

	// What this code does is basically set up a linked list as long
	// as it can, and once the max amount of decals have been reached
	// it starts recursing again, replacing each original decal.
	studiodecal_t* pfirst = (studiodecal_t*)m_pCurrentEntity->efrag;
	studiodecal_t* pnext = pfirst;

	if (pfirst->totaldecals == MAX_MODEL_DECALS)
		pfirst->totaldecals = 0;

	for (int i = 0; i < MAX_MODEL_DECALS; i++)
	{
		if (i == pfirst->totaldecals)
		{
			pfirst->totaldecals++;

			if (pnext->numverts != 0)
			{
				delete[] pnext->verts;
				pnext->verts = nullptr;
				pnext->numverts = 0;
			}

			if (pnext->numpolys != 0)
			{
				for (int k = 0; k < pnext->numpolys; k++)
					delete[] pnext->polys[k].verts;

				delete[] pnext->polys;
				pnext->polys = nullptr;
				pnext->numpolys = 0;
			}

			return pnext;
		}

		if (pnext->next == nullptr)
		{
			studiodecal_t* pDecal = StudioAllocDecalSlot();
			pnext->next = pDecal;
			pfirst->totaldecals++;

			return pDecal;
		}

		studiodecal_t* next = pnext->next;
		pnext = next;
	}
	return nullptr;
}

/*
====================
StudioDecalForEntity

====================
*/
void CStudioModelRenderer::StudioDecalForEntity(Vector position, Vector normal, const char* szName, cl_entity_t* pEntity)
{
	if (pEntity->model == nullptr)
		return;

	if (pEntity->model->type != mod_studio)
		return;

	if (pEntity == gEngfuncs.GetViewModel())
		return;

	decalgroup_t* group = gBSPRenderer.FindGroup(szName);

	if (group == nullptr)
		return;

	const decalgroupentry_t* texptr = gBSPRenderer.GetRandomDecal(group);

	if (texptr == nullptr)
		return;

	m_pCurrentEntity = pEntity;
	m_pRenderModel = pEntity->model;
	m_pStudioHeader = (studiohdr_t*)IEngineStudio.Mod_Extradata(m_pRenderModel);

	studiodecal_t* pDecal = StudioAllocDecal();

	if (pDecal == nullptr)
		return;

	pDecal->entindex = m_pCurrentEntity->index;
	pDecal->texture = texptr;

	StudioSetupTextureHeader();
	StudioSetUpTransform(0);
	StudioSetupBones();

	for (int i = 0; i < m_pStudioHeader->numbodyparts; i++)
	{
		StudioSetupModel(i);
		StudioDecalForSubModel(position, normal, pDecal);
	}
}

/*
====================
StudioDecalTriangle

====================
*/
void CStudioModelRenderer::StudioDecalTriangle(studiotri_t* tri, Vector position, Vector normal, studiodecal_t* decal)
{
	Vector dverts1[10];
	Vector dverts2[10];

	Vector norm, v1, v2, v3;
	VectorSubtract(m_vVertexTransform[tri->verts[1].vertindex], m_vVertexTransform[tri->verts[0].vertindex], v1);
	VectorSubtract(m_vVertexTransform[tri->verts[2].vertindex], m_vVertexTransform[tri->verts[1].vertindex], v2);
	CrossProduct(v2, v1, norm);

	if (DotProduct(normal, norm) < 0.0)
		return;

	Vector right, up;
	gBSPRenderer.GetUpRight(normal, up, right);
	float texc_orig_x = DotProduct(position, right);
	float texc_orig_y = DotProduct(position, up);

	int xsize = decal->texture->xsize;
	int ysize = decal->texture->ysize;

	for (int i = 0; i < 3; i++)
		VectorCopy(m_vVertexTransform[tri->verts[i].vertindex], dverts1[i]);

	int nv;
	Vector planepoint;
	VectorMASSE(position, -xsize, right, planepoint);
	nv = gBSPRenderer.ClipPolygonByPlane(dverts1, 3, right, planepoint, dverts2);

	if (nv < 3)
		return;

	VectorMASSE(position, xsize, right, planepoint);
	nv = gBSPRenderer.ClipPolygonByPlane(dverts2, nv, right * -1, planepoint, dverts1);

	if (nv < 3)
		return;

	VectorMASSE(position, -ysize, up, planepoint);
	nv = gBSPRenderer.ClipPolygonByPlane(dverts1, nv, up, planepoint, dverts2);

	if (nv < 3)
		return;

	VectorMASSE(position, ysize, up, planepoint);
	nv = gBSPRenderer.ClipPolygonByPlane(dverts2, nv, up * -1, planepoint, dverts1);

	if (nv < 3)
		return;

	// Only allow cut polys if the poly is only transformed by one bone
	if (nv > 3 && (tri->verts[0].boneindex != tri->verts[1].boneindex || tri->verts[0].boneindex != tri->verts[2].boneindex || tri->verts[1].boneindex != tri->verts[2].boneindex))
		return;

	// Check if the poly was cut
	if ((dverts1[0] != m_vVertexTransform[tri->verts[2].vertindex] || dverts1[1] != m_vVertexTransform[tri->verts[0].vertindex] || dverts1[2] != m_vVertexTransform[tri->verts[1].vertindex]) && (tri->verts[0].boneindex != tri->verts[1].boneindex || tri->verts[0].boneindex != tri->verts[2].boneindex || tri->verts[1].boneindex != tri->verts[2].boneindex))
		return;

	byte indexes[10];
	if (nv == 3 && dverts1[0] == m_vVertexTransform[tri->verts[2].vertindex] && dverts1[1] == m_vVertexTransform[tri->verts[0].vertindex] && dverts1[2] == m_vVertexTransform[tri->verts[1].vertindex])
	{
		indexes[0] = tri->verts[2].boneindex;
		indexes[1] = tri->verts[0].boneindex;
		indexes[2] = tri->verts[1].boneindex;
	}
	else
	{
		for (int i = 0; i < nv; i++)
			indexes[i] = tri->verts[0].boneindex;
	}

	decal->polys = (decalpoly_t*)ResizeArray((byte*)decal->polys, sizeof(decalpoly_t), decal->numpolys);
	decalpoly_t* polygon = &decal->polys[decal->numpolys];
	decal->numpolys++;

	polygon->verts = new decalvert_t[nv];
	polygon->numverts = nv;

	for (int i = 0; i < nv; i++)
	{
		float texc_x = (DotProduct(dverts1[i], right) - texc_orig_x) / xsize;
		float texc_y = (DotProduct(dverts1[i], up) - texc_orig_y) / ysize;
		polygon->verts[i].texcoord[0] = ((texc_x + 1) / 2);
		polygon->verts[i].texcoord[1] = ((texc_y + 1) / 2);

		Vector temp, fpos; // PINGAS
		temp[0] = dverts1[i][0] - (*m_pbonetransform)[indexes[i]][0][3];
		temp[1] = dverts1[i][1] - (*m_pbonetransform)[indexes[i]][1][3];
		temp[2] = dverts1[i][2] - (*m_pbonetransform)[indexes[i]][2][3];
		VectorIRotate(temp, (*m_pbonetransform)[indexes[i]], fpos);

		int j = 0;
		for (; j < decal->numverts; j++)
		{
			if (decal->verts[j].position == fpos)
			{
				polygon->verts[i].vertindex = j;
				break;
			}
		}

		if (j == decal->numverts)
		{
			decal->verts = (decalvertinfo_t*)ResizeArray((byte*)decal->verts, sizeof(decalvertinfo_t), decal->numverts);
			decal->verts[decal->numverts].boneindex = indexes[i];
			decal->verts[decal->numverts].position = fpos;

			polygon->verts[i].vertindex = decal->numverts;
			decal->numverts++;
		}
	}
}

/*
====================
StudioDecalForSubModel

====================
*/
void CStudioModelRenderer::StudioDecalForSubModel(Vector position, Vector normal, studiodecal_t* decal)
{
	byte* pvertbone = ((byte*)m_pStudioHeader + m_pSubModel->vertinfoindex);
	Vector* pstudioverts = (Vector*)((byte*)m_pStudioHeader + m_pSubModel->vertindex);

	for (int i = 0; i < m_pSubModel->numverts; i++)
		VectorTransformSSE(pstudioverts[i], (*m_pbonetransform)[pvertbone[i]], m_vVertexTransform[i]);

	for (int i = 0; i < m_pSubModel->nummesh; i++)
	{
		mstudiomesh_t* pmesh = (mstudiomesh_t*)((byte*)m_pStudioHeader + m_pSubModel->meshindex) + i;
		short* ptricmds = (short*)((byte*)m_pStudioHeader + pmesh->triindex);

		int j;
		while ((j = *(ptricmds++)) != 0)
		{
			if (j > 0)
			{
				// convert triangle strip
				j -= 3;
				studiotri_t triangle;
				triangle.verts[0].vertindex = ptricmds[0];
				triangle.verts[0].boneindex = pvertbone[ptricmds[0]];
				ptricmds += 4;

				triangle.verts[1].vertindex = ptricmds[0];
				triangle.verts[1].boneindex = pvertbone[ptricmds[0]];
				ptricmds += 4;

				triangle.verts[2].vertindex = ptricmds[0];
				triangle.verts[2].boneindex = pvertbone[ptricmds[0]];
				ptricmds += 4;

				StudioDecalTriangle(&triangle, position, normal, decal);

				bool reverse = false;
				for (; j > 0; j--, ptricmds += 4)
				{
					studiotri_t tritemp;
					triangle.verts[0] = triangle.verts[1];
					triangle.verts[1] = triangle.verts[2];

					triangle.verts[2].vertindex = ptricmds[0];
					triangle.verts[2].boneindex = pvertbone[ptricmds[0]];

					if (!reverse)
					{
						tritemp.verts[0] = triangle.verts[2];
						tritemp.verts[1] = triangle.verts[1];
						tritemp.verts[2] = triangle.verts[0];
					}
					else
					{
						tritemp.verts[0] = triangle.verts[0];
						tritemp.verts[1] = triangle.verts[1];
						tritemp.verts[2] = triangle.verts[2];
					}
					StudioDecalTriangle(&tritemp, position, normal, decal);
					reverse = !reverse;
				}
			}
			else
			{
				// convert triangle fan
				j = -j - 3;
				studiotri_t triangle;
				triangle.verts[0].vertindex = ptricmds[0];
				triangle.verts[0].boneindex = pvertbone[ptricmds[0]];
				ptricmds += 4;

				triangle.verts[1].vertindex = ptricmds[0];
				triangle.verts[1].boneindex = pvertbone[ptricmds[0]];
				ptricmds += 4;

				triangle.verts[2].vertindex = ptricmds[0];
				triangle.verts[2].boneindex = pvertbone[ptricmds[0]];
				ptricmds += 4;

				StudioDecalTriangle(&triangle, position, normal, decal);

				for (; j > 0; j--, ptricmds += 4)
				{
					triangle.verts[1] = triangle.verts[2];
					triangle.verts[2].vertindex = ptricmds[0];
					triangle.verts[2].boneindex = pvertbone[ptricmds[0]];

					StudioDecalTriangle(&triangle, position, normal, decal);
				}
			}
		}
	}
}

/*
====================
StudioDrawDecals

====================
*/
void CStudioModelRenderer::StudioDrawDecals()
{
	if (m_pCvarModelDecals->value < 1)
		return;

	if (m_pCurrentEntity->efrag == nullptr)
		return;

	if (gHUD.m_pFogSettings.active && (m_pCvarModelShaders->value != 0.0f) && gBSPRenderer.m_bShaderSupport)
	{
		glDisable(GL_FOG);
		glEnable(GL_FRAGMENT_PROGRAM_ARB);
		gBSPRenderer.glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, gBSPRenderer.m_iDecalFragmentID);
	}
	else if (gHUD.m_pFogSettings.active)
	{
		glDisable(GL_FOG);
	}

	// Just to make sure
	if (m_pCurrentEntity == gEngfuncs.GetViewModel())
		return;

	glEnable(GL_BLEND);
	glDepthMask(GL_FALSE);
	glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
	glPolygonOffset(-1, -1);
	glEnable(GL_POLYGON_OFFSET_FILL);

	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.1);

	gBSPRenderer.glActiveTextureARB(GL_TEXTURE0_ARB);
	glEnable(GL_TEXTURE_2D);

	studiodecal_t* pnext = (studiodecal_t*)m_pCurrentEntity->efrag;
	while (pnext != nullptr)
	{
		if (pnext->texture->gl_texid != m_iCurrentBinding)
		{
			glBindTexture(GL_TEXTURE_2D, pnext->texture->gl_texid);
			m_iCurrentBinding = pnext->texture->gl_texid;
		}

		if (!m_bExternalEntity)
		{
			for (int i = 0; i < pnext->numverts; i++)
			{
				if (pnext->verts[i].boneindex > m_pStudioHeader->numbones)
				{
					goto resetgl; // every time this happens, i shit bricks
					return;
				}

				VectorTransformSSE(pnext->verts[i].position, (*m_pbonetransform)[pnext->verts[i].boneindex], m_vVertexTransform[i]);
			}

			for (int i = 0; i < pnext->numpolys; i++)
			{
				decalvert_t* verts = &pnext->polys[i].verts[0];
				glBegin(GL_POLYGON);
				for (int j = 0; j < pnext->polys[i].numverts; j++)
				{
					glTexCoord2f(verts[j].texcoord[0], verts[j].texcoord[1]);
					glVertex3fv(m_pVertexTransform[verts[j].vertindex]);
				}
				glEnd();
			}
		}
		else
		{
			for (int i = 0; i < pnext->numpolys; i++)
			{
				decalvert_t* verts = &pnext->polys[i].verts[0];
				glBegin(GL_POLYGON);
				for (int j = 0; j < pnext->polys[i].numverts; j++)
				{
					glTexCoord2f(verts[j].texcoord[0], verts[j].texcoord[1]);
					glVertex3fv(pnext->verts[verts[j].vertindex].position);
				}
				glEnd();
			}
		}

		studiodecal_t* next = pnext->next;
		pnext = next;
	}

resetgl:
	glDisable(GL_POLYGON_OFFSET_FILL);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);
	glAlphaFunc(GL_GREATER, 0);

	if (gHUD.m_pFogSettings.active && (m_pCvarModelShaders->value != 0.0f) && gBSPRenderer.m_bShaderSupport)
	{
		glDisable(GL_FRAGMENT_PROGRAM_ARB);
		glEnable(GL_FOG);
	}
	else if (gHUD.m_pFogSettings.active)
	{
		glEnable(GL_FOG);
	}
}

/*
====================
StudioDecalExternal

====================
*/
void CStudioModelRenderer::StudioDecalExternal(Vector vpos, Vector vnorm, const char* name)
{
	int nv;

	Vector vtemp;
	Vector planepoint, right, up;
	Vector norm, v1, v2, v3;
	Vector vdecalmins, vdecalmaxs;
	Vector vtranspos, vtransnorm;

	Vector dverts1[10];
	Vector dverts2[10];

	decalgroup_t* group = gBSPRenderer.FindGroup(name);

	if (group == nullptr)
		return;

	const decalgroupentry_t* texptr = gBSPRenderer.GetRandomDecal(group);

	if (texptr == nullptr)
		return;

	float radius = (texptr->xsize > texptr->ysize) ? texptr->xsize : texptr->ysize;

	vdecalmins[0] = vpos[0] - radius;
	vdecalmins[1] = vpos[1] - radius;
	vdecalmins[2] = vpos[2] - radius;
	vdecalmaxs[0] = vpos[0] + radius;
	vdecalmaxs[1] = vpos[1] + radius;
	vdecalmaxs[2] = vpos[2] + radius;

	cl_entity_t* pEntity = gPropManager.m_pEntities;
	for (int i = 0; i < gPropManager.m_iNumEntities; i++, pEntity++)
	{
		if (pEntity->topnode == nullptr)
			continue;

		entextrainfo_t* pInfo = (entextrainfo_t*)pEntity->topnode;
		VectorCopy(pInfo->pExtraData->absmax, m_vMaxs);
		VectorCopy(pInfo->pExtraData->absmin, m_vMins);

		if (StudioCullBBox(vdecalmins, vdecalmaxs))
			continue;

		pEntity->angles[PITCH] = -pEntity->angles[PITCH];
		AngleMatrix(pEntity->angles, (*m_protationmatrix));
		pEntity->angles[PITCH] = -pEntity->angles[PITCH];

		VectorSubtract(vpos, pEntity->origin, vtemp);
		VectorIRotate(vtemp, (*m_protationmatrix), vtranspos);
		VectorIRotate(vnorm, (*m_protationmatrix), vtransnorm);

		m_pCurrentEntity = pEntity;
		studiodecal_t* pDecal = StudioAllocDecal();

		if (pDecal == nullptr)
			continue;

		pDecal->entindex = pEntity->index;
		pDecal->texture = texptr;

		m_pStudioHeader = pInfo->pExtraData->pModelData->pHdr;
		m_pVBOHeader = &pInfo->pExtraData->pModelData->pVBOHeader;
		m_pTextureHeader = pInfo->pExtraData->pModelData->pTexHdr;

		int baseindex = 0;
		for (int j = 0; j < m_pStudioHeader->numbodyparts; j++)
		{
			int index;
			m_pBodyPart = (mstudiobodyparts_t*)((byte*)m_pStudioHeader + m_pStudioHeader->bodypartindex) + j;

			index = pEntity->curstate.body / m_pBodyPart->base;
			index = index % m_pBodyPart->nummodels;

			m_pSubModel = (mstudiomodel_t*)((byte*)m_pStudioHeader + m_pBodyPart->modelindex) + index;
			m_pVBOSubModel = &m_pVBOHeader->submodels[baseindex + index];
			baseindex += m_pBodyPart->nummodels;

			int skinnum = m_pCurrentEntity->curstate.skin; // for short..

			if (skinnum < 0)
				skinnum = 0;

			short* pskinref = (short*)((byte*)m_pTextureHeader + m_pTextureHeader->skinindex);

			if (skinnum != 0 && skinnum < m_pTextureHeader->numskinfamilies)
				pskinref += (skinnum * m_pTextureHeader->numskinref);

			mstudiomesh_t* pmesh = (mstudiomesh_t*)((byte*)m_pStudioHeader + m_pSubModel->meshindex);
			mstudiotexture_t* ptexture = (mstudiotexture_t*)((byte*)m_pTextureHeader + m_pTextureHeader->textureindex);

			for (int k = 0; k < m_pVBOSubModel->nummeshes; k++)
			{
				if ((ptexture[pskinref[pmesh[k].skinref]].flags & STUDIO_NF_ALPHATEST) != 0)
					continue;

				vbomesh_t* pmesh = &m_pVBOSubModel->meshes[k];
				for (int l = 0; l < pmesh->num_vertexes; l += 3)
				{
					brushvertex_t* pv1 = &gPropManager.m_pVertexData[gPropManager.m_pIndexBuffer[(pmesh->start_vertex + l)]];
					brushvertex_t* pv2 = &gPropManager.m_pVertexData[gPropManager.m_pIndexBuffer[(pmesh->start_vertex + l + 1)]];
					brushvertex_t* pv3 = &gPropManager.m_pVertexData[gPropManager.m_pIndexBuffer[(pmesh->start_vertex + l + 2)]];

					VectorSubtract(pv2->pos, pv1->pos, v1);
					VectorSubtract(pv3->pos, pv2->pos, v2);
					CrossProduct(v2, v1, norm);

					if (DotProduct(vtransnorm, norm) < 0.0)
						continue;

					gBSPRenderer.GetUpRight(vtransnorm, up, right);
					float texc_orig_x = DotProduct(vtranspos, right);
					float texc_orig_y = DotProduct(vtranspos, up);

					int xsize = texptr->xsize;
					int ysize = texptr->ysize;

					VectorCopy(&pv1->pos, &dverts1[0]);
					VectorCopy(&pv2->pos, &dverts1[1]);
					VectorCopy(&pv3->pos, &dverts1[2]);

					VectorMASSE(vtranspos, -xsize, right, planepoint);
					nv = gBSPRenderer.ClipPolygonByPlane(dverts1, 3, right, planepoint, dverts2);

					if (nv < 3)
						continue;

					VectorMASSE(vtranspos, xsize, right, planepoint);
					nv = gBSPRenderer.ClipPolygonByPlane(dverts2, nv, right * -1, planepoint, dverts1);

					if (nv < 3)
						continue;

					VectorMASSE(vtranspos, -ysize, up, planepoint);
					nv = gBSPRenderer.ClipPolygonByPlane(dverts1, nv, up, planepoint, dverts2);

					if (nv < 3)
						continue;

					VectorMASSE(vtranspos, ysize, up, planepoint);
					nv = gBSPRenderer.ClipPolygonByPlane(dverts2, nv, up * -1, planepoint, dverts1);

					if (nv < 3)
						continue;

					pDecal->polys = (decalpoly_t*)ResizeArray((byte*)pDecal->polys, sizeof(decalpoly_t), pDecal->numpolys);
					decalpoly_t* polygon = &pDecal->polys[pDecal->numpolys];
					pDecal->numpolys++;
					polygon->verts = new decalvert_t[nv];
					polygon->numverts = nv;

					for (int m = 0; m < nv; m++)
					{
						float texc_x = (DotProduct(dverts1[m], right) - texc_orig_x) / xsize;
						float texc_y = (DotProduct(dverts1[m], up) - texc_orig_y) / ysize;
						polygon->verts[m].texcoord[0] = ((texc_x + 1) / 2);
						polygon->verts[m].texcoord[1] = ((texc_y + 1) / 2);

						int n = 0;
						for (; n < pDecal->numverts; n++)
						{
							if (pDecal->verts[n].position == dverts1[m])
							{
								polygon->verts[m].vertindex = n;
								break;
							}
						}

						if (n == pDecal->numverts)
						{
							pDecal->verts = (decalvertinfo_t*)ResizeArray((byte*)pDecal->verts, sizeof(decalvertinfo_t), pDecal->numverts);
							pDecal->verts[pDecal->numverts].boneindex = NULL;
							pDecal->verts[pDecal->numverts].position = dverts1[m];

							polygon->verts[m].vertindex = pDecal->numverts;
							pDecal->numverts++;
						}
					}
				}
			}
		}
	}
}

/*
====================
Mod_LoadModel

====================
*/
model_t* CStudioModelRenderer::Mod_LoadModel(const std::string& strName)
{
	// Try and find it in our cache
	for (int i = 0; i < m_iNumStudioModels; i++)
	{
		if (m_pStudioModels[i].name == strName)
			return &m_pStudioModels[i];
	}

	// Otherwise load it in
	int iSize = NULL;
	byte* pFile = gEngfuncs.COM_LoadFile(strName.c_str(), 5, &iSize);

	if (pFile == nullptr)
		return nullptr;

	// Copy over and free the file
	byte* pBuffer = new byte[iSize];
	memcpy(pBuffer, pFile, sizeof(byte) * iSize);
	gEngfuncs.COM_FreeFile(pFile);

	studiohdr_t* pHdr = (studiohdr_t*)pBuffer;
	mstudiotexture_t* pTexture = (mstudiotexture_t*)(pBuffer + pHdr->textureindex);

	if ((strncmp((const char*)pBuffer, "IDST", 4) != 0) && (strncmp((const char*)pBuffer, "IDSQ", 4) != 0))
	{
		delete[] pBuffer;
		return nullptr;
	}

	if (pHdr->textureindex != 0)
	{
		for (int i = 0; i < pHdr->numtextures; i++)
			Mod_LoadTexture(&pTexture[i], pBuffer, strName);
	}

	model_t* pModel = &m_pStudioModels[m_iNumStudioModels];
	m_iNumStudioModels++;

	strcpy(pModel->name, strName.c_str());
	pModel->cache.data = (void*)pBuffer;
	pModel->type = mod_studio;

	return pModel;
}

/*
====================
Mod_LoadTexture

====================
*/
void CStudioModelRenderer::Mod_LoadTexture(mstudiotexture_t* ptexture, byte* pbuffer, std::string strModelName)
{
	int i, j;
	int row1[1024], row2[1024], col1[1024], col2[1024];
	byte *pix1, *pix2, *pix3, *pix4;
	byte *tex, *out;
	byte alpha1, alpha2, alpha3, alpha4;

	byte* data = pbuffer + ptexture->index;
	byte* pal = pbuffer + ptexture->height * ptexture->width + ptexture->index;

	std::string strTexture = FilenameFromPath(ptexture->name);
	FranUtils::StringUtils::LowerCase_Ref(strTexture);

	strModelName = FilenameFromPath(strModelName);
	FranUtils::StringUtils::LowerCase_Ref(strModelName);

	if (gTextureLoader.TextureHasFlag(strModelName, strTexture, TEXFLAG_ERASE))
		ptexture->flags = NULL;

	if (gTextureLoader.TextureHasFlag(strModelName, strTexture, TEXFLAG_FULLBRIGHT))
		ptexture->flags |= STUDIO_NF_FULLBRIGHT;

	if (FranUtils::StringUtils::HasSubstring(strTexture, "chrome"))
	{
		ptexture->flags |= STUDIO_NF_CHROME;
		//ptexture->flags |= STUDIO_NF_FLATSHADE; // Chrome Textures has Flatshade
	}

	if (gTextureLoader.TextureHasFlag(strModelName, strTexture, TEXFLAG_NOMIPMAP))
		ptexture->flags |= STUDIO_NF_NOMIPMAP;

	if (gTextureLoader.TextureHasFlag(strModelName, strTexture, TEXFLAG_ALTERNATE))
	{
		std::string strPath = "gfx/textures/models/" + strModelName + "/" + strTexture + ".dds";
		cl_texture_t* pTexture = gTextureLoader.LoadTexture(strPath, NULL, true, (ptexture->flags & STUDIO_NF_NOMIPMAP ? 1 : 0) != 0);

		if (pTexture != nullptr)
		{
			ptexture->index = pTexture->iIndex;
			return;
		}
	}

	// convert texture to power of 2
	int outwidth;
	for (outwidth = 1; outwidth < ptexture->width; outwidth <<= 1)
		;
	if (outwidth > 1024)
		outwidth = 1024;

	int outheight;
	for (outheight = 1; outheight < ptexture->height; outheight <<= 1)
		;
	if (outheight > 1024)
		outheight = 1024;

	tex = out = new byte[outwidth * outheight * 4];

	if (out == nullptr)
		return;

	for (i = 0; i < outwidth; i++)
	{
		col1[i] = (int)((i + 0.25) * (ptexture->width / (float)outwidth));
		col2[i] = (int)((i + 0.75) * (ptexture->width / (float)outwidth));
	}

	for (i = 0; i < outheight; i++)
	{
		row1[i] = (int)((i + 0.25) * (ptexture->height / (float)outheight)) * ptexture->width;
		row2[i] = (int)((i + 0.75) * (ptexture->height / (float)outheight)) * ptexture->width;
	}

	for (i = 0; i < outheight; i++)
	{
		for (j = 0; j < outwidth; j++, out += 4)
		{
			pix1 = &pal[data[row1[i] + col1[j]] * 3];
			pix2 = &pal[data[row1[i] + col2[j]] * 3];
			pix3 = &pal[data[row2[i] + col1[j]] * 3];
			pix4 = &pal[data[row2[i] + col2[j]] * 3];

			if (data[row1[i] + col1[j]] == 0xFF && ((ptexture->flags & STUDIO_NF_ALPHATEST) != 0))
			{
				pix1[0] = 0x00;
				pix1[1] = 0x00;
				pix1[2] = 0x00;
				alpha1 = 0x00;
			}
			else
			{ // isnt transparent
				alpha1 = 0xFF;
			}

			if (data[row1[i] + col2[j]] == 0xFF &&
				((ptexture->flags & STUDIO_NF_ALPHATEST) != 0))
			{
				pix2[0] = 0x00;
				pix2[1] = 0x00;
				pix2[2] = 0x00;
				alpha2 = 0x00;
			}
			else
			{
				alpha2 = 0xFF;
			}

			if (data[row2[i] + col1[j]] == 0xFF && ((ptexture->flags & STUDIO_NF_ALPHATEST) != 0))
			{
				pix3[0] = 0x00;
				pix3[1] = 0x00;
				pix3[2] = 0x00;
				alpha3 = 0x00;
			}
			else
			{
				alpha3 = 0xFF;
			}

			if (data[row2[i] + col2[j]] == 0xFF && ((ptexture->flags & STUDIO_NF_ALPHATEST) != 0))
			{
				pix4[0] = 0x00;
				pix4[1] = 0x00;
				pix4[2] = 0x00;
				alpha4 = 0x00;
			}
			else
			{
				alpha4 = 0xFF;
			}

			out[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0]) >> 2;
			out[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1]) >> 2;
			out[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2]) >> 2;
			out[3] = (alpha1 + alpha2 + alpha3 + alpha4) >> 2;
		}
	}

	ptexture->index = current_ext_texture_id;
	current_ext_texture_id++;

	glBindTexture(GL_TEXTURE_2D, ptexture->index);

	if ((ptexture->flags & STUDIO_NF_NOMIPMAP) == 0)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE);
	}

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, outwidth, outheight, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex);
	delete[] tex;
}

/*
====================
StudioDrawModelSolid

====================
*/
void CStudioModelRenderer::StudioDrawModelSolid()
{
	if ((IsEntityTransparent(m_pCurrentEntity) != 0) && m_pCurrentEntity->curstate.renderamt == NULL)
		return;

	IEngineStudio.GetTimes(&m_nFrameCount, &m_clTime, &m_clOldTime);
	IEngineStudio.GetViewInfo(m_vRenderOrigin, m_vUp, m_vRight, m_vNormal);

	m_pRenderModel = m_pCurrentEntity->model;
	m_pStudioHeader = (studiohdr_t*)IEngineStudio.Mod_Extradata(m_pRenderModel);
	StudioSetupTextureHeader();

	if (m_pTextureHeader == nullptr)
		return;

	if (StudioCheckBBox() != 0)
		return;

	if (m_pStudioHeader->numbodyparts == 0)
		return;

	StudioSetUpTransform(0);
	StudioSetupBones();

	m_pVertexTransform = &m_vVertexTransform[0];
	m_pNormalTransform = &m_vNormalTransform[0];

	for (int i = 0; i < m_pStudioHeader->numbodyparts; i++)
	{
		StudioSetupModel(i);
		StudioDrawPointsSolid();
	}
}

/*
====================
StudioDrawPointsSolid

====================
*/
void CStudioModelRenderer::StudioDrawPointsSolid()
{
	mstudiomesh_t* pmeshes = (mstudiomesh_t*)((byte*)m_pStudioHeader + m_pSubModel->meshindex);
	Vector* pstudioverts = (Vector*)((byte*)m_pStudioHeader + m_pSubModel->vertindex);
	byte* pvertbone = ((byte*)m_pStudioHeader + m_pSubModel->vertinfoindex);
	mstudiotexture_t* ptexture = (mstudiotexture_t*)((byte*)m_pTextureHeader + m_pTextureHeader->textureindex);

	int skinnum = m_pCurrentEntity->curstate.skin;

	if (skinnum < 0)
		skinnum = 0;

	short* pskinref = (short*)((byte*)m_pTextureHeader + m_pTextureHeader->skinindex);

	if (skinnum != 0 && skinnum < m_pTextureHeader->numskinfamilies)
		pskinref += (skinnum * m_pTextureHeader->numskinref);

	//
	// Transform vetrices by bone matrices.
	//
	for (int i = 0; i < m_pSubModel->numverts; i++)
		VectorTransformSSE(pstudioverts[i], (*m_pbonetransform)[pvertbone[i]], m_pVertexTransform[i]);

	//
	// Render meshes
	//
	for (int j = 0; j < m_pSubModel->nummesh; j++)
	{
		mstudiotexture_t* ptex = &ptexture[pskinref[pmeshes[j].skinref]];

		if ((ptex->flags & STUDIO_NF_ALPHATEST) != 0)
		{
			glEnable(GL_ALPHA_TEST);
			glAlphaFunc(GL_GREATER, 0.5);

			gBSPRenderer.SetTexEnvs(ENVSTATE_REPLACE);
			gBSPRenderer.glActiveTextureARB(GL_TEXTURE0_ARB);
			glGetIntegerv(GL_TEXTURE_BINDING_2D, &m_iCurrentBinding);

			glMatrixMode(GL_TEXTURE);
			glScalef(1.0 / (float)ptex->width, 1.0 / (float)ptex->height, 1);

			if (ptex->index != m_iCurrentBinding)
			{
				glBindTexture(GL_TEXTURE_2D, ptex->index);
				m_iCurrentBinding = ptex->index;
			}

			int i;
			short* ptricmds = (short*)((byte*)m_pStudioHeader + pmeshes[j].triindex);

			while ((i = *(ptricmds++)) != 0)
			{
				if (i < 0)
				{
					glBegin(GL_TRIANGLE_FAN);
					i = -i;
				}
				else
				{
					glBegin(GL_TRIANGLE_STRIP);
				}

				for (; i > 0; i--, ptricmds += 4)
				{
					glTexCoord2f(ptricmds[2], ptricmds[3]);
					glVertex3fv(m_vVertexTransform[ptricmds[0]]);
				}
				glEnd();
			}

			gBSPRenderer.SetTexEnvs(ENVSTATE_OFF);
			gBSPRenderer.glActiveTextureARB(GL_TEXTURE0_ARB);
			glMatrixMode(GL_TEXTURE);
			glLoadIdentity();

			glMatrixMode(GL_MODELVIEW);
			glAlphaFunc(GL_GREATER, 0);
		}
		else
		{
			int i;
			short* ptricmds = (short*)((byte*)m_pStudioHeader + pmeshes[j].triindex);

			while ((i = *(ptricmds++)) != 0)
			{
				if (i < 0)
				{
					glBegin(GL_TRIANGLE_FAN);
					i = -i;
				}
				else
				{
					glBegin(GL_TRIANGLE_STRIP);
				}

				for (; i > 0; i--, ptricmds += 4)
				{
					glVertex3fv(m_vVertexTransform[ptricmds[0]]);
				}
				glEnd();
			}
		}
	}
}

/*
====================
StudioDrawExternalEntitySolid

====================
*/
void CStudioModelRenderer::StudioDrawExternalEntitySolid(cl_entity_t* pEntity)
{
	m_pCurrentEntity = pEntity;
	IEngineStudio.GetTimes(&m_nFrameCount, &m_clTime, &m_clOldTime);
	IEngineStudio.GetViewInfo(m_vRenderOrigin, m_vUp, m_vRight, m_vNormal);

	entextradata_t* pExtraData = ((entextrainfo_t*)m_pCurrentEntity->topnode)->pExtraData;
	m_pStudioHeader = pExtraData->pModelData->pHdr;
	m_pTextureHeader = pExtraData->pModelData->pTexHdr;
	m_pVBOHeader = &pExtraData->pModelData->pVBOHeader;

	if ((m_pStudioHeader == nullptr) || (m_pTextureHeader == nullptr) || (m_pVBOHeader == nullptr))
		return;

	if (m_pStudioHeader->numbodyparts == 0)
		return;

	m_bExternalEntity = true;
	VectorCopy(pExtraData->absmin, m_vMins);
	VectorCopy(pExtraData->absmax, m_vMaxs);

	if (gHUD.viewFrustum.CullBox(m_vMins, m_vMaxs))
		return;

	m_pCurrentEntity->angles[PITCH] = -m_pCurrentEntity->angles[PITCH];
	AngleMatrix(m_pCurrentEntity->angles, (*m_protationmatrix));
	m_pCurrentEntity->angles[PITCH] = -m_pCurrentEntity->angles[PITCH];

	glPushMatrix();
	R_RotateForEntity(m_pCurrentEntity);

	int baseindex = 0;
	for (int i = 0; i < m_pStudioHeader->numbodyparts; i++)
	{
		int index;
		m_pBodyPart = (mstudiobodyparts_t*)((byte*)m_pStudioHeader + m_pStudioHeader->bodypartindex) + i;

		index = m_pCurrentEntity->curstate.body / m_pBodyPart->base;
		index = index % m_pBodyPart->nummodels;

		m_pSubModel = (mstudiomodel_t*)((byte*)m_pStudioHeader + m_pBodyPart->modelindex) + index;
		m_pVBOSubModel = &m_pVBOHeader->submodels[baseindex + index];

		StudioDrawPointsSolidEXT();
		baseindex += m_pBodyPart->nummodels;
	}
	glPopMatrix();

	m_bExternalEntity = false;
}


/*
====================
StudioDrawPointsSolidEXT

====================
*/
void CStudioModelRenderer::StudioDrawPointsSolidEXT()
{
	if (m_pTextureHeader == nullptr)
		return;

	int skinnum = m_pCurrentEntity->curstate.skin; // for short..

	if (skinnum < 0)
		skinnum = 0;

	short* pskinref = (short*)((byte*)m_pTextureHeader + m_pTextureHeader->skinindex);

	if (skinnum != 0 && skinnum < m_pTextureHeader->numskinfamilies)
		pskinref += (skinnum * m_pTextureHeader->numskinref);

	mstudiomesh_t* pmesh = (mstudiomesh_t*)((byte*)m_pStudioHeader + m_pSubModel->meshindex);
	mstudiotexture_t* ptexture = (mstudiotexture_t*)((byte*)m_pTextureHeader + m_pTextureHeader->textureindex);

	for (int i = 0; i < m_pSubModel->nummesh; i++)
	{
		vbomesh_t* pvbomesh = &m_pVBOSubModel->meshes[i];
		mstudiotexture_t* ptex = &ptexture[pskinref[pmesh[i].skinref]];

		if ((ptex->flags & STUDIO_NF_ALPHATEST) != 0)
		{
			glEnable(GL_ALPHA_TEST);
			glAlphaFunc(GL_GREATER, 0.5);

			gBSPRenderer.SetTexEnvs(ENVSTATE_REPLACE);
			gBSPRenderer.glActiveTextureARB(GL_TEXTURE0_ARB);
			glGetIntegerv(GL_TEXTURE_BINDING_2D, &m_iCurrentBinding);

			glMatrixMode(GL_TEXTURE);
			glScalef(1.0 / (float)ptex->width, 1.0 / (float)ptex->height, 1);

			if (ptex->index != m_iCurrentBinding)
			{
				glBindTexture(GL_TEXTURE_2D, ptex->index);
				m_iCurrentBinding = ptex->index;
			}
		}

		glDrawElements(GL_TRIANGLES, pvbomesh->num_vertexes, GL_UNSIGNED_INT, BUFFER_OFFSET(pvbomesh->start_vertex));

		if ((ptex->flags & STUDIO_NF_ALPHATEST) != 0)
		{

			gBSPRenderer.SetTexEnvs(ENVSTATE_OFF);
			gBSPRenderer.glActiveTextureARB(GL_TEXTURE0_ARB);
			glMatrixMode(GL_TEXTURE);
			glLoadIdentity();

			glMatrixMode(GL_MODELVIEW);
			glAlphaFunc(GL_GREATER, 0);
		}
	}
}

/*
====================
StudioSetupModelSVD

====================
*/
void CStudioModelRenderer::StudioSetupModelSVD(int bodypart)
{
	if (bodypart > m_pSVDHeader->numbodyparts)
		bodypart = 0;

	svdbodypart_t* pbodypart = (svdbodypart_t*)((byte*)m_pSVDHeader + m_pSVDHeader->bodypartindex) + bodypart;

	int index = m_pCurrentEntity->curstate.body / pbodypart->base;
	index = index % pbodypart->numsubmodels;

	m_pSVDSubModel = (svdsubmodel_t*)((byte*)m_pSVDHeader + pbodypart->submodelindex) + index;
}

/*
====================
StudioDrawShadow

====================
*/
void CStudioModelRenderer::StudioDrawShadow()
{
	StudioSetBuffer();

	// Set SVD header
	m_pSVDHeader = (svdheader_t*)m_pRenderModel->visdata;

	glDepthMask(GL_FALSE);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE); // disable writes to color buffer

	glEnable(GL_STENCIL_TEST);
	glStencilFunc(GL_ALWAYS, 0, ~0);

	for (int i = 0; i < m_pStudioHeader->numbodyparts; i++)
	{
		StudioSetupModelSVD(i);
		StudioDrawShadowVolume();
	}

	glDepthMask(GL_TRUE);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDisable(GL_STENCIL_TEST);

	StudioClearBuffer();
}



/*
====================
StudioDrawShadowVolume

====================
*/
void CStudioModelRenderer::StudioDrawShadowVolume()
{
	float plane[4];
	Vector lightdir;
	Vector *pv1, *pv2, *pv3;

	if (m_pSVDSubModel->numfaces == 0)
		return;

	Vector* psvdverts = (Vector*)((byte*)m_pSVDHeader + m_pSVDSubModel->vertexindex);
	byte* pvertbone = ((byte*)m_pSVDHeader + m_pSVDSubModel->vertinfoindex);
	Vector shadeVector;

	// search for closest elight
	mlight_t closest_elight;
	for (int i = 0; i < MAXRENDERENTS; i++)
	{
		auto light = gBSPRenderer.m_pModelLights[i];
		if (light.radius != 0.0f && !light.flashlight)
		{
			// start
			if (closest_elight.radius == 0.0f)
				closest_elight = light;

			// is this more closer to the entity?
			if (Vector(light.origin - m_pCurrentEntity->curstate.origin).Length() < Vector(closest_elight.origin - m_pCurrentEntity->curstate.origin).Length())
				closest_elight = light;
		}
	}

	if (Vector(closest_elight.origin - m_pCurrentEntity->curstate.origin).Length() < 500.0f)
	{
		m_vShadowLightOrigin = closest_elight.origin;
	}
	else if (m_pCvarSkyVecX->value != 0 || m_pCvarSkyVecY->value != 0 || m_pCvarSkyVecZ->value != 0)
	{
		Vector skyvec(m_pCvarSkyVecX->value, m_pCvarSkyVecY->value, m_pCvarSkyVecZ->value);
		skyvec = skyvec * -1;
		shadeVector[0] = skyvec.x;
		shadeVector[1] = skyvec.y;
		shadeVector[2] = skyvec.z;

		m_vShadowLightOrigin = m_pCurrentEntity->origin + shadeVector * 8196;
	}
	else
	{
		shadeVector[0] = 0.3;
		shadeVector[1] = 0.5;
		shadeVector[2] = 1;

		m_vShadowLightOrigin = m_pCurrentEntity->origin + shadeVector * 8196;
	}

	// Calculate vertex coords
	for (int i = 0, j = 0; i < m_pSVDSubModel->numverts; i++, j += 2)
	{
		VectorTransform(psvdverts[i], (*m_pbonetransform)[pvertbone[i]], m_vertexTransform[j]);

		VectorSubtract(m_vertexTransform[j], m_vShadowLightOrigin, lightdir);
		VectorNormalizeFast(lightdir);

		VectorMA(m_vertexTransform[j], 4096, lightdir, m_vertexTransform[j + 1]);
	}

	// Process the faces
	int numIndexes = 0;
	svdface_t* pfaces = (svdface_t*)((byte*)m_pSVDHeader + m_pSVDSubModel->faceindex);

	for (int i = 0; i < m_pSVDSubModel->numfaces; i++)
	{
		pv1 = &m_vertexTransform[pfaces[i].vertex0];
		pv2 = &m_vertexTransform[pfaces[i].vertex1];
		pv3 = &m_vertexTransform[pfaces[i].vertex2];

		plane[0] = pv1->y * (pv2->z - pv3->z) + pv2->y * (pv3->z - pv1->z) + pv3->y * (pv1->z - pv2->z);
		plane[1] = pv1->z * (pv2->x - pv3->x) + pv2->z * (pv3->x - pv1->x) + pv3->z * (pv1->x - pv2->x);
		plane[2] = pv1->x * (pv2->y - pv3->y) + pv2->x * (pv3->y - pv1->y) + pv3->x * (pv1->y - pv2->y);
		plane[3] = -(pv1->x * (pv2->y * pv3->z - pv3->y * pv2->z) + pv2->x * (pv3->y * pv1->z - pv1->y * pv3->z) + pv3->x * (pv1->y * pv2->z - pv2->y * pv1->z));

		m_trianglesFacingLight[i] = (DotProduct(plane, m_vShadowLightOrigin) + plane[3]) > 0;

		// comment this 'if' block if you want to use z-pass method
		if (m_trianglesFacingLight[i])
		{
			m_shadowVolumeIndexes[numIndexes] = pfaces[i].vertex0;
			m_shadowVolumeIndexes[numIndexes + 1] = pfaces[i].vertex2;
			m_shadowVolumeIndexes[numIndexes + 2] = pfaces[i].vertex1;

			m_shadowVolumeIndexes[numIndexes + 3] = pfaces[i].vertex0 + 1;
			m_shadowVolumeIndexes[numIndexes + 4] = pfaces[i].vertex1 + 1;
			m_shadowVolumeIndexes[numIndexes + 5] = pfaces[i].vertex2 + 1;

			numIndexes += 6;
		}
	}

	// Process the edges
	svdedge_t* pedges = (svdedge_t*)((byte*)m_pSVDHeader + m_pSVDSubModel->edgeindex);
	for (int i = 0; i < m_pSVDSubModel->numedges; i++)
	{
		if (m_trianglesFacingLight[pedges[i].face0])
		{
			if ((pedges[i].face1 != -1) && m_trianglesFacingLight[pedges[i].face1])
				continue;

			m_shadowVolumeIndexes[numIndexes] = pedges[i].vertex0;
			m_shadowVolumeIndexes[numIndexes + 1] = pedges[i].vertex1;
		}
		else
		{
			if ((pedges[i].face1 == -1) || !m_trianglesFacingLight[pedges[i].face1])
				continue;

			m_shadowVolumeIndexes[numIndexes] = pedges[i].vertex1;
			m_shadowVolumeIndexes[numIndexes + 1] = pedges[i].vertex0;
		}

		m_shadowVolumeIndexes[numIndexes + 2] = m_shadowVolumeIndexes[numIndexes] + 1;
		m_shadowVolumeIndexes[numIndexes + 3] = m_shadowVolumeIndexes[numIndexes + 2];
		m_shadowVolumeIndexes[numIndexes + 4] = m_shadowVolumeIndexes[numIndexes + 1];
		m_shadowVolumeIndexes[numIndexes + 5] = m_shadowVolumeIndexes[numIndexes + 1] + 1;
		numIndexes += 6;
	}

	// draw back faces incrementing stencil values when z fails
	glStencilOp(GL_KEEP, GL_INCR, GL_KEEP);
	glCullFace(GL_BACK);
	glDrawElements(GL_TRIANGLES, numIndexes, GL_UNSIGNED_SHORT, m_shadowVolumeIndexes);

	// draw front faces decrementing stencil values when z fails
	glStencilOp(GL_KEEP, GL_DECR, GL_KEEP);
	glCullFace(GL_FRONT);
	glDrawElements(GL_TRIANGLES, numIndexes, GL_UNSIGNED_SHORT, m_shadowVolumeIndexes);
}

/*
====================
StudioSetBuffer

====================
*/
void CStudioModelRenderer::StudioSetBuffer()
{
	glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);

	// Disable these to avoid slowdown bug
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);

	gBSPRenderer.glClientActiveTextureARB(GL_TEXTURE1);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	gBSPRenderer.glClientActiveTextureARB(GL_TEXTURE2);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	gBSPRenderer.glClientActiveTextureARB(GL_TEXTURE3);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	gBSPRenderer.glClientActiveTextureARB(GL_TEXTURE0);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	glVertexPointer(3, GL_FLOAT, sizeof(Vector), m_vertexTransform);
	glEnableClientState(GL_VERTEX_ARRAY);
}

/*
====================
StudioClearBuffer

====================
*/
void CStudioModelRenderer::StudioClearBuffer()
{
	glDisableClientState(GL_VERTEX_ARRAY);

	glPopClientAttrib();
}

/*
====================
StudioShouldDrawShadow

====================
*/
bool CStudioModelRenderer::StudioShouldDrawShadow()
{
	if (m_pCvarDrawShadows->value < 2)
		return false;

	// Entities flagged for no shadowing
	if (m_pCurrentEntity->curstate.renderfx == 101)
		return false;

	// for now only draw NPCs - tagged with renderfx 102
	if (m_pCurrentEntity->curstate.renderfx != 102)
		return false;

	if (IEngineStudio.IsHardware() != 1)
		return false;

	if (m_pRenderModel->visdata == nullptr)
		return false;

	// Fucking butt-ugly hack to make the shadows less annoying
	pmtrace_t tr;
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(m_vRenderOrigin, m_pCurrentEntity->origin + Vector(0, 0, 1), PM_WORLD_ONLY, -1, &tr);

	if (tr.fraction != 1.0)
		return false;

	return true;
}

/*
===============
GL_StudioDrawShadow
g-cont: don't modify this code it's 100% matched with
original GoldSrc code and used in some mods to enable
studio shadows with some asm tricks
===============
*/
void CStudioModelRenderer::GL_StudioDrawShadow()
{
	// magic nipples - shadows | changed r_shadows.value to -> to prevent error
	if (m_pCvarDrawShadows->value != 0.0f)
	{
		// stencil pass start
		glPushAttrib(GL_TEXTURE_BIT);

		glDepthMask(GL_FALSE);
		glDisable(GL_TEXTURE_2D);
		glEnable(GL_CULL_FACE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glColor4f(GL_ZERO, GL_ZERO, GL_ZERO, 0.25f);

		glStencilFunc(GL_NOTEQUAL, 0, ~0);
		glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
		glEnable(GL_STENCIL_TEST);

		StudioDrawPointsShadow();

		glDepthMask(GL_TRUE);
		glEnable(GL_CULL_FACE);
		glDisable(GL_BLEND);
		glDisable(GL_STENCIL_TEST);

		// g_StudioRenderer.StudioClearBuffer();
		glPopAttrib();

		// stencil pass end

		// glDepthMask(GL_TRUE);
	}
}

/*
===============
StudioDrawPointsShadow
===============
*/
void CStudioModelRenderer::StudioDrawPointsShadow()
{
	float* av;
	mstudiomesh_t* pmesh;
	Vector point;
	int i, k;

	// height = g_studio.lightspot[2] + 1.0f;
	// vec_x = -g_studio.lightvec[0] * 8.0f;
	// vec_y = -g_studio.lightvec[1] * 8.0f;

	// magic nipples - no more shadows from lightsources because it looks bad

	for (k = 0; k < m_pSubModel->nummesh; k++)
	{
		short* ptricmds;

		pmesh = (mstudiomesh_t*)((byte*)m_pStudioHeader + m_pSubModel->meshindex) + k;
		ptricmds = (short*)((byte*)m_pStudioHeader + pmesh->triindex);

		while (i = *(ptricmds++) != 0)
		{
			if (i < 0)
			{
				glBegin(GL_TRIANGLE_FAN);
				i = -i;
			}
			else
			{
				glBegin(GL_TRIANGLE_STRIP);
			}


			for (; i > 0; i--, ptricmds += 4)
			{
				av = verts[ptricmds[0]];
				point[0] = av[0];
				point[1] = av[1];
				point[2] = av[2];

				glVertex3fv(point);
			}

			glEnd();
		}
	}
}

void Matrix3x4_VectorTransform(const float (*in)[4], const float v[3], float out[3])
{
	out[0] = v[0] * in[0][0] + v[1] * in[0][1] + v[2] * in[0][2] + in[0][3];
	out[1] = v[0] * in[1][0] + v[1] * in[1][1] + v[2] * in[1][2] + in[1][3];
	out[2] = v[0] * in[2][0] + v[1] * in[2][1] + v[2] * in[2][2] + in[2][3];
}
/*
===============
R_StudioDrawPoints
===============
*/
void CStudioModelRenderer::StudioGetVerts()
{
	int i;
	byte* pvertbone;
	Vector* pstudioverts;

	if (m_pStudioHeader == nullptr)
		return;

	// safety bounding the skinnum
	pvertbone = ((byte*)m_pStudioHeader + m_pSubModel->vertinfoindex);
	pstudioverts = (Vector*)((byte*)m_pStudioHeader + m_pSubModel->vertindex);

	for (i = 0; i < m_pSubModel->numverts; i++)
	{
		Matrix3x4_VectorTransform((*m_pbonetransform)[pvertbone[i]], pstudioverts[i], verts[i]);
	}
}

/*
===============
pfnStudioSetupModel
===============
*/
void CStudioModelRenderer::StudioSetupModel(int bodypart, void** ppbodypart, void** ppsubmodel)
{
	int index;

	if (bodypart > m_pStudioHeader->numbodyparts)
		bodypart = 0;

	m_pBodyPart = (mstudiobodyparts_t*)((byte*)m_pStudioHeader + m_pStudioHeader->bodypartindex) + bodypart;

	index = m_pCurrentEntity->curstate.body / m_pBodyPart->base;
	index = index % m_pBodyPart->nummodels;

	m_pSubModel = (mstudiomodel_t*)((byte*)m_pStudioHeader + m_pBodyPart->modelindex) + index;

	if (ppbodypart != nullptr)
		*ppbodypart = m_pBodyPart;
	if (ppsubmodel != nullptr)
		*ppsubmodel = m_pSubModel;
}