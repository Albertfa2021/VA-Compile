#include "VABinauralWaveFrontBase.h"

#include "../../../../Motion/VAMotionModelBase.h"
#include "../../../../Motion/VASharedMotionModel.h"
#include "../../../../Scene/VAScene.h"
#include "../../../../Utils/VABinauralUtils.h"
#include "../../../../core/core.h"

#include <ITASampleBuffer.h>
#include <ITAVariableDelayLine.h>

CVABinauralWaveFrontBase::CVABinauralWaveFrontBase( const Config& m_oConf ) : m_oConf( m_oConf )
{
	assert( m_oConf.pCore );

	float fSampleRate = (float)m_oConf.pCore->GetCoreConfig( )->oAudioDriverConfig.dSampleRate;
	int iBlockLength  = m_oConf.pCore->GetCoreConfig( )->oAudioDriverConfig.iBuffersize;

	m_pVDLChL = new CITAVariableDelayLine( fSampleRate, iBlockLength, (float)3.0f * fSampleRate, EVDLAlgorithm::CUBIC_SPLINE_INTERPOLATION );
	m_pVDLChR = new CITAVariableDelayLine( fSampleRate, iBlockLength, (float)3.0f * fSampleRate, EVDLAlgorithm::CUBIC_SPLINE_INTERPOLATION );
}

CVABinauralWaveFrontBase::~CVABinauralWaveFrontBase( )
{
	delete m_pVDLChR;
	delete m_pVDLChL;
}

void CVABinauralWaveFrontBase::PreRequest( )
{
	CVABasicMotionModel::Config pSourceMotionConf;

	pSourceMotionConf.bLogEstimatedOutputEnabled = m_oConf.bMotionModelLogEstimated;
	pSourceMotionConf.bLogInputEnabled           = m_oConf.bMotionModelLogInput;
	pSourceMotionConf.dWindowDelay               = m_oConf.dMotionModelWindowDelay;
	pSourceMotionConf.dWindowSize                = m_oConf.dMotionModelWindowSize;
	pSourceMotionConf.iNumHistoryKeys            = m_oConf.iMotionModelNumHistoryKeys;

	pMotionModel     = new CVASharedMotionModel( new CVABasicMotionModel( pSourceMotionConf ), true );
	pState           = nullptr;
	pSoundSourceData = nullptr;

	m_bClusteringPoseSet  = false;
	m_bWaveFrontOriginSet = false;

	/* @todo implement reset functionality for VDL
	vdlChL->Reset();
	vdlChR->Reset();
	*/
}

void CVABinauralWaveFrontBase::PreRelease( )
{
	delete pMotionModel;
	// @todo use reset of motion model instead of new / delete in request and release of pool
}

void CVABinauralWaveFrontBase::GetOutput( ITASampleBuffer* pfLeftChannel, ITASampleBuffer* pfRightChannel )
{
	float fGain                = 0.0f;
	const double dSpeedOfSound = 343.0f; // @todo get from VA environment

	if( GetValidWaveFrontOrigin( ) && GetValidClusteringPose( ) )
	{
		// Determine ToA difference from clustering pose to (clustering) principle direction (the actual direction of HRTF convolution)
		CVASourceTargetMetrics oClusteringMetrics;
		oClusteringMetrics.Calc( m_v3ClusteringPos, m_v3ClusteringView, m_v3ClusteringUp, m_v3ClusteringPrincipleDirectionOrigin, VAVec3( 0, 0, -1 ), VAVec3( 0, 1, 0 ) );

		double dPrincipleDirectionHRTFToALeft =
		    VABinauralUtils::TimeOfArrivalModel::SphericalShapeGetLeft( oClusteringMetrics.dAzimuthS2T, oClusteringMetrics.dElevationS2T, 0.09, dSpeedOfSound );
		double dPrincipleDirectionHRTFToARight =
		    VABinauralUtils::TimeOfArrivalModel::SphericalShapeGetLeft( oClusteringMetrics.dAzimuthS2T, oClusteringMetrics.dElevationS2T, 0.09, dSpeedOfSound );

		// Determine ToA difference from clustering pose to wave front incidence direction / wave front origin
		CVASourceTargetMetrics oWaveFrontMetrics;
		oWaveFrontMetrics.Calc( m_v3ClusteringPos, m_v3ClusteringView, m_v3ClusteringUp, m_v3WaveFrontOrigin, VAVec3( 0, 0, -1 ), VAVec3( 0, 1, 0 ) );

		double dWaveFrontHRTFToALeft =
		    VABinauralUtils::TimeOfArrivalModel::SphericalShapeGetLeft( oWaveFrontMetrics.dAzimuthS2T, oWaveFrontMetrics.dElevationS2T, 0.09, dSpeedOfSound );
		double dWaveFrontHRTFToARight =
		    VABinauralUtils::TimeOfArrivalModel::SphericalShapeGetLeft( oWaveFrontMetrics.dAzimuthS2T, oWaveFrontMetrics.dElevationS2T, 0.09, dSpeedOfSound );


		float fFinalDelayLeft  = float( dPrincipleDirectionHRTFToALeft - dWaveFrontHRTFToALeft ) + float( oWaveFrontMetrics.dDistance / dSpeedOfSound );
		float fFinalDelayRight = float( dPrincipleDirectionHRTFToARight - dWaveFrontHRTFToARight ) + float( oWaveFrontMetrics.dDistance / dSpeedOfSound );

		m_pVDLChL->SetDelayTime( fFinalDelayLeft );
		m_pVDLChL->SetDelayTime( fFinalDelayRight );

		double dAmplitudeCorrection = m_oConf.pCore->GetCoreConfig( )->dDefaultAmplitudeCalibration;
		fGain                       = float( ( 1 / oWaveFrontMetrics.dDistance ) * pState->GetVolume( dAmplitudeCorrection ) );
	}

	// Override for now, as something is wrong with the parameters! @todo jst fix!
	m_pVDLChL->SetDelayTime( 0 );
	m_pVDLChL->SetDelayTime( 0 );
	// fGain = 1.0f;

	const ITASampleFrame& sfInputBuffer( *pSoundSourceData->psfSignalSourceInputBuf );
	const ITASampleBuffer* psbInput( &( sfInputBuffer[0] ) );

	m_pVDLChL->Process( psbInput, pfLeftChannel );
	m_pVDLChL->Process( psbInput, pfRightChannel );

	pfLeftChannel->mul_scalar( fGain );
	pfRightChannel->mul_scalar( fGain );
}

void CVABinauralWaveFrontBase::SetClusteringMetrics( const VAVec3& v3Pos, const VAVec3& v3View, const VAVec3& v3Up, const VAVec3& v3PrincipleDirectionOrigin )
{
	m_v3ClusteringPos                      = v3Pos;
	m_v3ClusteringView                     = v3View;
	m_v3ClusteringUp                       = v3Up;
	m_v3ClusteringPrincipleDirectionOrigin = v3PrincipleDirectionOrigin;

	m_bClusteringPoseSet = true;
}

bool CVABinauralWaveFrontBase::GetValidWaveFrontOrigin( ) const
{
	return m_bWaveFrontOriginSet;
}

bool CVABinauralWaveFrontBase::GetValidClusteringPose( ) const
{
	return m_bClusteringPoseSet;
}

void CVABinauralWaveFrontBase::SetWaveFrontOrigin( const VAVec3& v3Origin )
{
	m_v3WaveFrontOrigin   = v3Origin;
	m_bWaveFrontOriginSet = true;
}

VAVec3 CVABinauralWaveFrontBase::GetWaveFrontOrigin( ) const
{
	return m_v3WaveFrontOrigin;
}

int CVABinauralWaveFrontBase::AddReference( )
{
	return CVAPoolObject::AddReference( );
}

int CVABinauralWaveFrontBase::RemoveReference( )
{
	return CVAPoolObject::RemoveReference( );
}

int CVABinauralWaveFrontBase::GetSourceID( ) const
{
	if( !pSoundSourceData )
		return -1;

	return pSoundSourceData->iID;
}
