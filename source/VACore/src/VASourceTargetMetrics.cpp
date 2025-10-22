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

#include "VASourceTargetMetrics.h"

#include "Scene/VAMotionState.h"
#include "Scene/VASoundReceiverState.h"
#include "Scene/VASoundSourceState.h"
#include "Utils/VAUtils.h"

#include <VAException.h>

CVASourceTargetMetrics::CVASourceTargetMetrics( ) : dAzimuthT2S( 0 ), dAzimuthS2T( 0 ), dElevationT2S( 0 ), dElevationS2T( 0 ), dDistance( 0 ) {}

CVASourceTargetMetrics::CVASourceTargetMetrics( const CVASoundSourceState* pSourceState, const CVAReceiverState* pReceiverState )
{
	assert( pSourceState );
	assert( pReceiverState );

	const CVAMotionState* pSourceMotion   = pSourceState->GetMotionState( );
	const CVAMotionState* pReceiverMotion = pReceiverState->GetMotionState( );

	if( !pSourceMotion || !pReceiverMotion )
	{
		dDistance = dAzimuthT2S = dElevationT2S = dAzimuthS2T = dElevationS2T = 0;
	}
	else
	{
		Calc( pReceiverMotion->GetPosition( ), pReceiverMotion->GetView( ), pReceiverMotion->GetUp( ), pSourceMotion->GetPosition( ), pSourceMotion->GetView( ),
		      pSourceMotion->GetUp( ) );
	}
}

void CVASourceTargetMetrics::Calc( const VAVec3& v3SourcePos, const VAVec3& v3SourceView, const VAVec3& v3SourceUp, const VAVec3& v3TargetPos, const VAVec3& v3TargetView,
                                   const VAVec3& v3TargetUp )
{
	dAzimuthT2S   = GetAzimuthOnTarget_DEG( v3TargetPos, v3TargetView, v3TargetUp, v3SourcePos );
	dAzimuthS2T   = GetAzimuthOnTarget_DEG( v3SourcePos, v3SourceView, v3SourceUp, v3TargetPos );
	dElevationT2S = GetElevationOnTarget_DEG( v3TargetPos, v3TargetUp, v3SourcePos );
	dElevationS2T = GetElevationOnTarget_DEG( v3SourcePos, v3SourceUp, v3TargetPos );

	const VAVec3 v3DirectionVector( v3TargetPos - v3SourcePos );
	dDistance = v3DirectionVector.Length( );
}
