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

#include "VAAudioRendererMovingObject.h"

// VA includes
#include "../../Motion/VAMotionModelBase.h"
#include "../../Motion/VASampleAndHoldMotionModel.h"
#include "../../Motion/VASharedMotionModel.h"
#include "../../Scene/VAMotionState.h"

#include <cassert>

void CVARendererMovingObject::InsertMotionKey( const CVAMotionState* pMotionState )
{
	assert( pMotionModel );
	pMotionModel->InputMotionKey( pMotionState );
}

void CVARendererMovingObject::EstimateCurrentPose( double dTime )
{
	assert( pMotionModel );
	pMotionModel->HandleMotionKeys( );
	bool bValid = pMotionModel->EstimatePosition( dTime, vPredPos );
	bValid &= pMotionModel->EstimateOrientation( dTime, vPredView, vPredUp );
	bValidTrajectoryPresent = bValid;
}

void CVARendererMovingObject::SetMotionModelName( const std::string& sName )
{
	assert( pMotionModel );
	CVABasicMotionModel* pMotionInstance = dynamic_cast<CVABasicMotionModel*>( pMotionModel->GetInstance( ) );
	pMotionInstance->SetName( sName );
}
