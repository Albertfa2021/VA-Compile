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

#include "VAMotionState.h"

#include "../Utils/VAUtils.h"

#include <VA.h>
#include <assert.h>

void CVAMotionState::Initialize( double dModificationTime )
{
	data.vPos.Set( 0, 0, 0 );
	data.vView.Set( 0, 0, -1 );
	data.vUp.Set( 0, 1, 0 );

	data.oRealWorldPose.Reset( );

	SetModificationTime( dModificationTime );
}

void CVAMotionState::Copy( const CVAMotionState* pSrc, double dModificationTime )
{
	assert( pSrc );
	// Only allowed for fixed source motion states
	assert( pSrc->IsFixed( ) );

	// copy
	data = pSrc->data;
	SetFixed( false );
	SetModificationTime( dModificationTime );
}

void CVAMotionState::Fix( )
{
	CVASceneStateBase::Fix( );
}

VAVec3 CVAMotionState::GetPosition( ) const
{
	return data.vPos;
}

VAVec3 CVAMotionState::GetView( ) const
{
	return data.vView;
}

VAVec3 CVAMotionState::GetUp( ) const
{
	return data.vUp;
}

CVAMotionState::CVAPose CVAMotionState::GetRealWorldPose( ) const
{
	return data.oRealWorldPose;
}

void CVAMotionState::SetRealWorldPose( const CVAPose& oNewPose )
{
	data.oRealWorldPose = oNewPose;
}

VAQuat CVAMotionState::GetOrientation( ) const
{
	VAQuat oOrient;
	ConvertViewUpToQuaternion( data.vView, data.vUp, oOrient );
	return oOrient;
}

void CVAMotionState::SetPosition( const VAVec3& vPos )
{
	assert( !IsFixed( ) );
	data.vPos = vPos;
}

void CVAMotionState::SetOrientation( const VAQuat& qOrient )
{
	assert( !IsFixed( ) );
	ConvertQuaternionToViewUp( qOrient, data.vView, data.vUp );
}

void CVAMotionState::SetOrientationVU( const VAVec3& vView, const VAVec3& vUp )
{
	assert( !IsFixed( ) );
	data.vView = vView;
	data.vUp   = vUp;
}

VAQuat CVAMotionState::GetHeadAboveTorsoOrientation( ) const
{
	return data.qHATO;
}

void CVAMotionState::SetHeadAboveTorsoOrientation( const VAQuat& qOrient )
{
	data.qHATO = qOrient;
}

VAQuat CVAMotionState::GetRealWorldHeadAboveTorsoOrientation( ) const
{
	return data.qRealWorldHATO;
}

void CVAMotionState::SetRealWorldHeadAboveTorsoOrientation( const VAQuat& qOrient )
{
	data.qRealWorldHATO = qOrient;
}
