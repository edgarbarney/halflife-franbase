//==============================================================================//
//			Copyright (c) 2010 - 2011, Richard Roh·Ë, All rights reserved.		//
//==============================================================================//

#pragma once
#ifndef FRUSTUM_H
#define FRUSTUM_H

#include "windows.h"
#include "gl/gl.h"
#include "pm_defs.h"
#include "cl_entity.h"
#include "com_model.h"
#include "ref_params.h"
#include "dlight.h"
#include "parsemsg.h"
#include "cvardef.h"

#define PITCH 0
#define YAW 1
#define ROLL 2

#define FARCLIP_OFF 0
#define FARCLIP_DEPTH 1
#define FARCLIP_RADIAL 2

//typedef float Vector[3];

typedef struct Q_mplane_s
{
	Vector vNormal;
	float flDist;
	byte type;
	byte signbits;
	byte pad[2];
} Q_mplane_t;

/*
===============
CFrustum

===============
*/
class FrustumCheck
{
public:
	void SetFrustum(Vector vAngles, Vector vOrigin, float flFOV, float flFarDist = 0, bool bView = false);
	bool CullBox(Vector vMins, Vector vMaxs);
	bool RadialCullBox(Vector vMins, Vector vMaxs);
	bool ExtraCullBox(Vector vMins, Vector vMaxs);

	void SetExtraCullBox(Vector vMins, Vector vMaxs);
	void DisableExtraCullBox(void);

	float CalcFov(float flFovX, float flWidth, float flHeight);
	void Q_AngleVectors(Vector vAngles, Vector vForward, Vector vRight, Vector vUp);
	int Q_BoxOnPlaneSide(Vector emins, Vector emaxs, Q_mplane_t* p);
	int Q_SignbitsForPlane(Q_mplane_t* pOut);
	void Q_RotatePointAroundVector(Vector vDest, const Vector vDir, Vector vPoint, float flDegrees);
	void Q_CrossProduct(Vector v1, Vector v2, Vector cross);

private:
	Q_mplane_t m_sFrustum[5];
	int m_iFarClip;

	Vector m_vCullBoxMins;
	Vector m_vCullBoxMaxs;

	bool m_bExtraCull;
	Vector m_vExtraCullMins;
	Vector m_vExtraCullMaxs;
};
#endif
