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

#include "VAMatlabTracking.h"

#include "VAMatlabConnection.h"

#include <VA.h>

// Tracking callback function
void TrackerDataHandler( const std::vector<IHTA::Tracking::ITrackingInterface::STrackingDataPoint>& vDataPoints, CVAMatlabTracker* pVAMatlabTracker )
{
	if( !pVAMatlabTracker )
		return;

	IVAInterface* pVACore = pVAMatlabTracker->pVACore;
	if( !pVACore )
		return;

	// SoundReceiver
	int iTrackedReceiverID                 = pVAMatlabTracker->iTrackedSoundReceiverID;
	int iTrackedReceiverRigidBodyIndex     = pVAMatlabTracker->iTrackedSoundReceiverHeadRigidBodyIndex;
	int iTrackedReceiverHATORigidBodyIndex = pVAMatlabTracker->iTrackedSoundReceiverTorsoRigidBodyIndex;

	if( iTrackedReceiverID != -1 )
	{
		// Tracked receiver
		if( ( iTrackedReceiverRigidBodyIndex <= vDataPoints.size( ) ) && ( iTrackedReceiverRigidBodyIndex > 0 ) &&
		    vDataPoints.at( iTrackedReceiverRigidBodyIndex - 1 ).bValid )
		{
			try
			{
				VistaVector3D vPosOffsetLocalCoordinateSystem = pVAMatlabTracker->vTrackedSoundReceiverTranslation;
				VistaQuaternion qOrientRotation               = pVAMatlabTracker->qTrackedSoundReceiverRotation;

				const auto& oBodyData = vDataPoints.at( iTrackedReceiverRigidBodyIndex - 1 );
				VistaVector3D vPosPivotPoint( oBodyData.oLocation[0], oBodyData.oLocation[1], oBodyData.oLocation[2] );
				VistaQuaternion qOrientRaw( oBodyData.oQuaternion[0], oBodyData.oQuaternion[1], oBodyData.oQuaternion[2], oBodyData.oQuaternion[3] );

				VistaQuaternion qOrientRigidBody = qOrientRotation * qOrientRaw;
				VistaVector3D vViewRigidBody     = qOrientRigidBody.GetViewDir( );
				VistaVector3D vUpRigidBody       = qOrientRigidBody.GetUpDir( );

				VistaVector3D vPosOffsetGlobalCoordinateSystem = qOrientRigidBody.Rotate( vPosOffsetLocalCoordinateSystem );
				VistaVector3D vPosRigidBody                    = vPosPivotPoint + vPosOffsetGlobalCoordinateSystem;

				pVACore->SetSoundReceiverPosition( iTrackedReceiverID, VAVec3( vPosRigidBody[0], vPosRigidBody[1], vPosRigidBody[2] ) );
				pVACore->SetSoundReceiverOrientationVU( iTrackedReceiverID, VAVec3( vViewRigidBody[0], vViewRigidBody[1], vViewRigidBody[2] ),
				                                        VAVec3( vUpRigidBody[0], vUpRigidBody[1], vUpRigidBody[2] ) );

				// HATO orientation
				if( ( iTrackedReceiverHATORigidBodyIndex <= vDataPoints.size( ) ) && ( iTrackedReceiverHATORigidBodyIndex > 0 ) )
				{
					const auto& oBodyData = vDataPoints.at( iTrackedReceiverHATORigidBodyIndex - 1 );
					VistaQuaternion qOrientRaw( oBodyData.oQuaternion[0], oBodyData.oQuaternion[1], oBodyData.oQuaternion[2], oBodyData.oQuaternion[3] );

					VistaQuaternion qHATOOrient = qOrientRaw.GetInverted( ) * qOrientRigidBody;
					VAQuat qFinalHATO           = VAQuat( qHATOOrient[Vista::X], qHATOOrient[Vista::Y], qHATOOrient[Vista::Z], qHATOOrient[Vista::W] );

					pVACore->SetSoundReceiverHeadAboveTorsoOrientation( iTrackedReceiverID, qFinalHATO );
				}
			}
			catch( ... )
			{
			}
		}
	}

	// Real-world sound receiver
	int iTrackedRealWorldSoundReceiverID                  = pVAMatlabTracker->iTrackedRealWorldSoundReceiverID;
	int iTrackedRealWorldSoundReceiverHeadRigidBodyIndex  = pVAMatlabTracker->iTrackedRealWorldSoundReceiverHeadRigidBodyIndex;
	int iTrackedRealWorldSoundReceiverTorsoRigidBodyIndex = pVAMatlabTracker->iTrackedRealWorldSoundReceiverTorsoRigidBodyIndex;

	if( iTrackedRealWorldSoundReceiverID != -1 )
	{
		// Real world receiver tracking
		if( ( iTrackedRealWorldSoundReceiverHeadRigidBodyIndex <= vDataPoints.size( ) ) && ( iTrackedRealWorldSoundReceiverHeadRigidBodyIndex > 0 ) &&
		    vDataPoints.at( iTrackedRealWorldSoundReceiverHeadRigidBodyIndex - 1 ).bValid )
		{
			try
			{
				VistaVector3D vPosOffsetLocalCoordinateSystem = pVAMatlabTracker->vTrackedRealWorldSoundReceiverTranslation;
				VistaQuaternion qOrientRotation               = pVAMatlabTracker->qTrackedRealWorldSoundReceiverRotation;

				const auto& oBodyData = vDataPoints.at( iTrackedRealWorldSoundReceiverHeadRigidBodyIndex - 1 );
				VistaVector3D vPosPivotPoint( oBodyData.oLocation[0], oBodyData.oLocation[1], oBodyData.oLocation[2] );
				VistaQuaternion qOrientRaw( oBodyData.oQuaternion[0], oBodyData.oQuaternion[1], oBodyData.oQuaternion[2], oBodyData.oQuaternion[3] );

				VistaQuaternion qOrientRigidBody = qOrientRotation * qOrientRaw;
				VistaVector3D vViewRigidBody     = qOrientRigidBody.GetViewDir( );
				VistaVector3D vUpRigidBody       = qOrientRigidBody.GetUpDir( );

				VistaVector3D vPosOffsetGlobalCoordinateSystem = qOrientRigidBody.Rotate( vPosOffsetLocalCoordinateSystem );
				VistaVector3D vPosRigidBody                    = vPosPivotPoint + vPosOffsetGlobalCoordinateSystem;

				pVACore->SetSoundReceiverRealWorldPositionOrientationVU( iTrackedRealWorldSoundReceiverID, VAVec3( vPosRigidBody[0], vPosRigidBody[1], vPosRigidBody[2] ),
				                                                         VAVec3( vViewRigidBody[0], vViewRigidBody[1], vViewRigidBody[2] ),
				                                                         VAVec3( vUpRigidBody[0], vUpRigidBody[1], vUpRigidBody[2] ) );


				// Real world receiver HATO orientation
				if( ( iTrackedRealWorldSoundReceiverTorsoRigidBodyIndex <= vDataPoints.size( ) ) && ( iTrackedRealWorldSoundReceiverTorsoRigidBodyIndex > 0 ) )
				{
					const auto& oBodyData = vDataPoints.at( iTrackedRealWorldSoundReceiverTorsoRigidBodyIndex - 1 );
					const VistaQuaternion qTorsoOrientRaw( oBodyData.oQuaternion[0], oBodyData.oQuaternion[1], oBodyData.oQuaternion[2], oBodyData.oQuaternion[3] );

					const VistaQuaternion qTorsoOrient = qTorsoOrientRaw.GetInverted( ) * qOrientRigidBody;

					const VAQuat qFinalOrient( qTorsoOrient[Vista::X], qTorsoOrient[Vista::Y], qTorsoOrient[Vista::Z], qTorsoOrient[Vista::W] );
					pVACore->SetSoundReceiverRealWorldHeadAboveTorsoOrientation( iTrackedReceiverID, qFinalOrient );
				}
			}
			catch( ... )
			{
			}
		}
	}

	// Source
	int iTrackedSourceID             = pVAMatlabTracker->iTrackedSoundSourceID;
	int iTrackedSourceRigidBodyIndex = pVAMatlabTracker->iTrackedSoundSourceRigidBodyIndex;

	if( ( iTrackedSourceID != -1 ) && ( iTrackedSourceRigidBodyIndex <= vDataPoints.size( ) ) && ( iTrackedSourceRigidBodyIndex > 0 ) &&
	    vDataPoints.at( iTrackedSourceRigidBodyIndex - 1 ).bValid )
	{
		try
		{
			VistaVector3D vPosOffsetLocalCoordinateSystem = pVAMatlabTracker->vTrackedSoundSourceTranslation;
			VistaQuaternion qOrientRotation               = pVAMatlabTracker->qTrackedSoundSourceRotation;

			const auto& oBodyData = vDataPoints.at( iTrackedSourceRigidBodyIndex - 1 );
			VistaVector3D vPosPivotPoint( oBodyData.oLocation[0], oBodyData.oLocation[1], oBodyData.oLocation[2] );
			VistaQuaternion qOrientRaw( oBodyData.oQuaternion[0], oBodyData.oQuaternion[1], oBodyData.oQuaternion[2], oBodyData.oQuaternion[3] );

			VistaQuaternion qOrientRigidBody = qOrientRotation * qOrientRaw;
			VistaVector3D vViewRigidBody     = qOrientRigidBody.GetViewDir( );
			VistaVector3D vUpRigidBody       = qOrientRigidBody.GetUpDir( );

			VistaVector3D vPosOffsetGlobalCoordinateSystem = qOrientRigidBody.Rotate( vPosOffsetLocalCoordinateSystem );
			VistaVector3D vPosRigidBody                    = vPosPivotPoint + vPosOffsetGlobalCoordinateSystem;

			pVACore->SetSoundSourcePosition( iTrackedSourceID, VAVec3( vPosRigidBody[0], vPosRigidBody[1], vPosRigidBody[2] ) );
			pVACore->SetSoundSourceOrientationVU( iTrackedSourceID, VAVec3( vViewRigidBody[0], vViewRigidBody[1], vViewRigidBody[2] ),
			                                      VAVec3( vUpRigidBody[0], vUpRigidBody[1], vUpRigidBody[2] ) );
		}
		catch( ... )
		{
		}
	}

	return;
}

CVAMatlabTracker::CVAMatlabTracker( )
{
	pVACore = NULL;
	Reset( );
	m_bConnected = false;
}

void CVAMatlabTracker::Reset( )
{
	iTrackedSoundReceiverID                  = -1;
	iTrackedSoundReceiverHeadRigidBodyIndex  = 1;
	iTrackedSoundReceiverTorsoRigidBodyIndex = 1;
	vTrackedSoundReceiverTranslation.SetToZeroVector( );
	qTrackedSoundReceiverRotation.SetToNeutralQuaternion( );

	iTrackedRealWorldSoundReceiverID                  = -1;
	iTrackedRealWorldSoundReceiverHeadRigidBodyIndex  = 1;
	iTrackedRealWorldSoundReceiverTorsoRigidBodyIndex = 1;
	vTrackedRealWorldSoundReceiverTranslation.SetToZeroVector( );
	qTrackedRealWorldSoundReceiverRotation.SetToNeutralQuaternion( );

	iTrackedSoundSourceID             = -1;
	iTrackedSoundSourceRigidBodyIndex = 1;
	vTrackedSoundSourceTranslation.SetToZeroVector( );
	qTrackedSoundSourceRotation.SetToNeutralQuaternion( );
}

bool CVAMatlabTracker::Initialize( const std::string& sServerAddress, const std::string& sLocalAddress, const std::string& sType )
{
	auto eType = IHTA::Tracking::CTrackingFactory::Types::Optitrack;

	if( sType == "ART" )
	{
		eType = IHTA::Tracking::CTrackingFactory::Types::ART;
	}

	m_pTrackerClient = IHTA::Tracking::CTrackingFactory::CreateTrackingClass( eType );

	m_pTrackerClient->RegisterCallback( [this]( const std::vector<IHTA::Tracking::ITrackingInterface::STrackingDataPoint>& vDataPoints )
	                                    { TrackerDataHandler( vDataPoints, this ); } );

	int a;

	m_bConnected = m_pTrackerClient->Initialize( sLocalAddress, sServerAddress );

	return m_bConnected;
}

bool CVAMatlabTracker::Uninitialize( )
{
	m_pTrackerClient.reset( nullptr );
	m_bConnected = false;
	return true;
}

bool CVAMatlabTracker::IsConnected( ) const
{
	return m_bConnected;
}
