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

#include "VAAmbisonicsEncoding.h"

#include "VAException.h"

#include <ITANumericUtils.h>
#include <ITASampleBuffer.h>
#include <ITASampleFrame.h>
#include <math.h>


CVAAmbisonicsEncoding::CVAAmbisonicsEncoding( const IVASpatialEncoding::Config& oConf ) : m_iMaxOrder( oConf.iAmbisonicsOrder ) {}

void CVAAmbisonicsEncoding::Process( const ITASampleBuffer& sbInputData, ITASampleFrame& sfOutput, float fGain, double dAzimuthDeg, double dElevationDeg,
                                     const CVAReceiverState& )
{
	const double dElevationDegAmbisonics = 90.0 - dElevationDeg; // Different convention for elevation in SH domain than for HRTFs
	const int iNumChannels               = sfOutput.GetNumChannels( );
	const std::vector<double> vdSHGains  = SHRealvaluedBasefunctions( dElevationDegAmbisonics / 180.0 * M_PI, dAzimuthDeg / 180.0 * M_PI, m_iMaxOrder );

	if( (int)vdSHGains.size( ) < iNumChannels )
	{
		VA_EXCEPT2( INVALID_PARAMETER, "Not enough SH base functions for requested number of channels" );
	}

	for( int i = 0; i < iNumChannels; i++ )
	{
		sfOutput[i].MulAdd( sbInputData, vdSHGains[i] * fGain, 0, 0, sbInputData.GetLength( ) );
	}
}
