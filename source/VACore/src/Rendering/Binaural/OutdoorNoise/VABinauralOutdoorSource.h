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

#ifndef IW_VACORE_BINAURAL_OUTDOOR_SOURCE
#define IW_VACORE_BINAURAL_OUTDOOR_SOURCE

#include "../../../Motion/VASharedMotionModel.h"
#include "../../../Scene/VASoundSourceDesc.h"
#include "../../../Scene/VASoundSourceState.h"
#include "../../../directivities/VADirectivity.h"

#include <ITASIMOVariableDelayLineBase.h>
#include <ITAThirdOctaveMagnitudeSpectrum.h>

//!
/**
 * @henry:
 */
class CVABinauralOutdoorSource
{
public:
	CVABinauralOutdoorSource( int iMaxDelaySamples );
	~CVABinauralOutdoorSource( );

	CVASoundSourceState* pState;
	CVASoundSourceDesc* pData;
	CVASharedMotionModel* pMotionModel;

	IVADirectivity* pDirectivity;

	CITASIMOVariableDelayLineBase* pVDL;

	//! Returns the directiviy spectrum for a given timestamp (referring to source orientation) and an outgoing wavefront normal
	ITABase::CThirdOctaveGainMagnitudeSpectrum GetDirectivitySpectrum( const double& dTime, const VAVec3& v3SourceWFNormal );
	//! Returns the directiviy spectrum for a given view and up vector and an outgoing wavefront normal
	ITABase::CThirdOctaveGainMagnitudeSpectrum GetDirectivitySpectrum( const VAVec3& v3SourceView, const VAVec3& v3SourceUp, const VAVec3& v3SourceWFNormal );

private:
};

#endif // IW_VACORE_BINAURAL_OUTDOOR_SOURCE
