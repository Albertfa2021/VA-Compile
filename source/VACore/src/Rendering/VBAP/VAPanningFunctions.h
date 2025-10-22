/*
 *  --------------------------------------------------------------------------------------------
 *
 *    VVV        VVV A           Virtual Acoustics (VA) | http://www.virtualacoustics.org
 *     VVV      VVV AAA          Licensed under the Apache License, Version 2.0
 *      VVV    VVV   AAA
 *       VVV  VVV     AAA        Copyright 2015-2022
 *        VVVVVV       AAA       Institute of Technical Acoustics (ITA)
 *         VVVV         AAA      RWTH Aachen University
 *
 *  --------------------------------------------------------------------------------------------
 */

#ifndef IW_VACORE_PANNINGFUNCTIONS
#define IW_VACORE_PANNINGFUNCTIONS

#	include "../../core/core.h"
#	include <ITABase/Math/Triangulation.h>
#	include <VistaBase/VistaVector3D.h>
#	include <vector>

using namespace ITABase::Math;

// --- Panning functions ---
bool CalculateVBAPGains3D( const CTriangulation* pTriangulation, const VistaVector3D& v3SourcePos, std::vector<double>& vdGains );

bool CalculateMDAPGains3D( const CTriangulation* pTriangulation, const std::vector<VistaVector3D>& v3SourcePos, std::vector<double>& vdGains );

// --- Helpers ---
void CalcualteSpreadingSources( const VAVec3& v3SourcPos, const VAVec3& view, const VAVec3& up, const float& spreadingAngle, const int& numSpreadingSources,
                                std::vector<VAVec3>& vv3SpreadingSources );

#endif
