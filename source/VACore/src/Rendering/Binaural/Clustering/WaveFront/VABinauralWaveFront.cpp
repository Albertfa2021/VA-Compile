#include "VABinauralWaveFront.h"
#include "../VARelationMetrics.h"
#include "../VAConfig.h"

#include "../../../../Motion/VAMotionModelBase.h"
#include "../../../../Motion/VASharedMotionModel.h"
#include "../../../../Scene/VAScene.h"

#include <ITAVariableDelayLine.h>
#include <ITASampleBuffer.h>
#include <ITASampleFrame.h>

CVABinauralWaveFrontBase::CVABinauralWaveFrontBase( const Config& conf )
	: oConf( conf )
{
	vdlChL = new CITAVariableDelayLine( conf.sampleRate, conf.blockLength, ( const float ) ( 3. * conf.sampleRate ), CITAVariableDelayLine::CUBIC_SPLINE_INTERPOLATION );
	vdlChR = new CITAVariableDelayLine( conf.sampleRate, conf.blockLength, ( const float ) ( 3. * conf.sampleRate ), CITAVariableDelayLine::CUBIC_SPLINE_INTERPOLATION );
}

CVABinauralWaveFrontBase::~CVABinauralWaveFrontBase()
{
	delete vdlChR;
	delete vdlChL;
}

void CVABinauralWaveFrontBase::PreRequest()
{
	CVABasicMotionModel::Config sourceMotionConf;

	sourceMotionConf.bLogEstimatedOutputEnabled = oConf.motionModelLogEstimated;
	sourceMotionConf.bLogInputEnabled = oConf.motionModelLogInput;
	sourceMotionConf.dWindowDelay = oConf.motionModelWindowDelay;
	sourceMotionConf.dWindowSize = oConf.motionModelWindowSize;
	sourceMotionConf.iNumHistoryKeys = oConf.motionModelNumHistoryKeys;

	pMotionModel = new CVASharedMotionModel( new CVABasicMotionModel( sourceMotionConf ), true );
	pState = nullptr;
	pData = nullptr;
	bHasValidTrajectory = false;

	/* @todo implement reset functionality for VDL
	vdlChL->Reset();
	vdlChR->Reset();
	*/
}

void CVABinauralWaveFrontBase::PreRelease()
{
	delete pMotionModel;
	/* @todo use reset of motion model instead of new / delete in request and release of pool
	motionModel->Reset();
	motionModel->Reset();
	*/
}

void CVABinauralWaveFrontBase::GetOutput( ITASampleBuffer* pfLeftChannel, ITASampleBuffer* pfRightChannel )
{
	// Determine ToA difference
	VARelationMetrics sourceMetrics;
	sourceMetrics.calc( m_v3PredictedClusteringPos, m_v3PredictedClusteringView, m_v3PredictedClusteringUp, v3PredictedPos );

	//double toaDistance = sourceMetrics.dist / 343; // TODO: get sound speed from core homogeneous medium

	//double toaSourceChL = _listener->toaEstimator->getTOALeft(sourceMetrics.phi, sourceMetrics.theta);
	//double toSourceaChR = _listener->toaEstimator->getTOARight(sourceMetrics.phi, sourceMetrics.theta);

	// @todo finalize and use TOA estimation
	//		vdlChL->SetDelayTime(std::max(0., toaDistance + toaSourceChL - toaHRTFChL));
	//		vdlChR->SetDelayTime(std::max(0., toaDistance + toaSourceChR - toaHRTFChR));

	// @todo jst: there is something wrong with the following two lines ... toaDistance seems to jump
	//vdlChL->SetDelayTime( toaDistance );
	//vdlChR->SetDelayTime( toaDistance );

	const ITASampleFrame& sfInputBuffer( *pData->psfSignalSourceInputBuf ); // Has at least one channel
	vdlChL->Process( &( sfInputBuffer[ 0 ] ), pfLeftChannel );
	vdlChL->Process( &( sfInputBuffer[ 0 ] ), pfRightChannel );

	float gain = float( ( 1 / sourceMetrics.dist ) * pState->GetVolume( VAConfig::amplitudeCalibration ) );
	pfLeftChannel->mul_scalar( gain );
	pfRightChannel->mul_scalar( gain );
}

void CVABinauralWaveFrontBase::SetClusteringPose( const VAVec3& v3Pos, const VAVec3& v3View, const VAVec3& v3Up )
{
	m_v3PredictedClusteringPos = v3Pos;
	m_v3PredictedClusteringView = v3View;
	m_v3PredictedClusteringUp = v3Up;
}
