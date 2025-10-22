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

#ifndef IW_VACORE_SOURCELISTENERMETRICS
#define IW_VACORE_SOURCELISTENERMETRICS

#include <VA.h>

class CVASoundSourceState;
class CVAReceiverState;

/**
 * \note All angular metrics are in degrees
 */
class CVASourceTargetMetrics
{
public:
	double dAzimuthS2T, dElevationS2T; //!< Angular direction source target [°]
	double dAzimuthT2S, dElevationT2S; //!< Angular direction target source [°]
	double dDistance;                  //!< Distance source target (may be zero) [m]

	CVASourceTargetMetrics( );

	CVASourceTargetMetrics( const CVASoundSourceState*, const CVAReceiverState* );

	//! Determine metrics (relative angles and distance) in degree/meters
	void Calc( const VAVec3& v3SourcePos, const VAVec3& v3SourceView, const VAVec3& v3SourceUp, const VAVec3& v3TargetPos, const VAVec3& v3TargetView,
	           const VAVec3& v3TargetUp );
};

#endif // IW_VACORE_SOURCELISTENERMETRICS
