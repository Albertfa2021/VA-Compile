#include "VABinauralClusteringRenderer.h"

// VA Includes
#include <VA.h>
#include <VAObjectPool.h>

// VA Core Includes
#include "../../../Scene/VAScene.h"
#include "../../../Utils/VAUtils.h"
#include "../../../core/core.h"

// ITA includes
#include <ITAException.h>
#include <ITAFastMath.h>
#include <ITAStreamInfo.h>
#include <ITAVariableDelayLine.h>

// Utils
#include "Receiver/VABinauralClusteringReceiverPoolFactory.h"
#include "WaveFront/VABinauralWaveFrontBasePoolFactory.h"

// STL
#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <time.h>

CVABinauralClusteringRenderer::CVABinauralClusteringRenderer( const CVAAudioRendererInitParams& oParams )
    : ITADatasourceRealization( 2, oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.dSampleRate, oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.iBuffersize )
    , m_oParams( oParams )
    , m_pCore( oParams.pCore )
    , m_pNewSceneState( NULL )
    , m_pCurSceneState( NULL )
    , m_bIndicateReset( false )
    , m_bResetAck( false )
{
	Init( *m_oParams.pConfig );

	IVAPoolObjectFactory* listenerFactory = new CVABinauralClusteringReceiverPoolFactory( m_pCore, m_oDefaultReceiverConf );
	m_pReceiverPool                       = IVAObjectPool::Create( 16, 2, listenerFactory, true );

	IVAPoolObjectFactory* sourceFactory = new CVABinauralWaveFrontBasePoolFactory( m_oDefaultWaveFrontConf );
	m_pSourcePool                       = IVAObjectPool::Create( 16, 2, sourceFactory, true );

	m_iCurGlobalAuralizationMode = IVAInterface::VA_AURAMODE_DEFAULT;
}

CVABinauralClusteringRenderer::~CVABinauralClusteringRenderer( ) {}

void CVABinauralClusteringRenderer::Init( const CVAStruct& oArgs )
{
	CVAConfigInterpreter conf( oArgs );

	// set interpolation algorithm
	std::string vldInterpolationAlgorithm;
	conf.OptString( "SwitchingAlgorithm", vldInterpolationAlgorithm, "linear" );
	vldInterpolationAlgorithm = toLowercase( vldInterpolationAlgorithm );

	if( vldInterpolationAlgorithm == "switch" )
		m_iDefaultVDLSwitchingAlgorithm = EVDLAlgorithm::SWITCH;
	else if( vldInterpolationAlgorithm == "crossfade" )
		m_iDefaultVDLSwitchingAlgorithm = EVDLAlgorithm::CROSSFADE;
	else if( vldInterpolationAlgorithm == "linear" )
		m_iDefaultVDLSwitchingAlgorithm = EVDLAlgorithm::LINEAR_INTERPOLATION;
	else if( vldInterpolationAlgorithm == "cubicspline" )
		m_iDefaultVDLSwitchingAlgorithm = EVDLAlgorithm::CUBIC_SPLINE_INTERPOLATION;
	else if( vldInterpolationAlgorithm == "windowedsinc" )
		m_iDefaultVDLSwitchingAlgorithm = EVDLAlgorithm::WINDOWED_SINC_INTERPOLATION;
	else
		ITA_EXCEPT1( INVALID_PARAMETER, "Unrecognized interpolation algorithm '" + vldInterpolationAlgorithm + "' in BinauralFreefieldAudioRendererConfig" );

	// HRTF filter length
	conf.OptInteger( "HRIRFilterLength", m_iHRIRFilterLength, 256 );

	// additional static delay
	conf.OptNumber( "AdditionalStaticDelaySeconds", m_dAdditionalStaticDelaySeconds, 0.0f );


	// default listener config, @todo rest
	m_oDefaultReceiverConf.bMotionModelLogEstimated = false;
	m_oDefaultReceiverConf.bMotionModelLogInput     = false;
	conf.OptInteger( "MotionModelNumHistoryKeys", m_oDefaultReceiverConf.iMotionModelNumHistoryKeys, 1000 );

	if( m_oDefaultReceiverConf.iMotionModelNumHistoryKeys < 1 )
		VA_EXCEPT2( INVALID_PARAMETER, "Basic motion model history needs to be greater than zero" );

	conf.OptNumber( "MotionModelWindowSize", m_oDefaultReceiverConf.dMotionModelWindowSize, 0.1f );
	conf.OptNumber( "MotionModelWindowDelay", m_oDefaultReceiverConf.dMotionModelWindowDelay, 0.1f );

	if( ( m_oDefaultReceiverConf.dMotionModelWindowSize <= 0 ) || ( m_oDefaultReceiverConf.dMotionModelWindowDelay < 0 ) )
		VA_EXCEPT2( INVALID_PARAMETER, "Basic motion model window parameters parse error (zero or negative?)" );

	// default source config
	conf.OptInteger( "MotionModelNumHistoryKeys", m_oDefaultWaveFrontConf.iMotionModelNumHistoryKeys, 1000 );

	if( m_oDefaultWaveFrontConf.iMotionModelNumHistoryKeys < 1 )
		VA_EXCEPT2( INVALID_PARAMETER, "Basic motion model history needs to be greater than zero" );

	conf.OptNumber( "MotionModelWindowSize", m_oDefaultWaveFrontConf.dMotionModelWindowSize, 0.1f );
	conf.OptNumber( "MotionModelWindowDelay", m_oDefaultWaveFrontConf.dMotionModelWindowDelay, 0.1f );

	if( ( m_oDefaultWaveFrontConf.dMotionModelWindowSize <= 0 ) || ( m_oDefaultWaveFrontConf.dMotionModelWindowDelay < 0 ) )
		VA_EXCEPT2( INVALID_PARAMETER, "Basic motion model window parameters parse error (zero or negative?)" );

	m_oDefaultWaveFrontConf.pCore = m_pCore;

	conf.OptInteger( "NumClusters", m_iNumClusters, 12 );
	conf.OptNumber( "AngularDistanceThreshold", m_dAngularDistanceThreshold, 4.0f / float( m_iNumClusters ) );

	// initialize after config
	m_pClusterEngine = new CVABinauralClusteringEngine( GetBlocklength( ), m_iHRIRFilterLength );
}

void CVABinauralClusteringRenderer::Reset( )
{
	VA_ERROR( "BinauralClusteringRenderer", "Reset hast not yet been implemented" );
}

ITADatasource* CVABinauralClusteringRenderer::GetOutputDatasource( )
{
	return this;
}

void CVABinauralClusteringRenderer::UpdateGlobalAuralizationMode( int iGlobalAuralizationMode )
{
	if( m_iCurGlobalAuralizationMode != iGlobalAuralizationMode )
		m_iCurGlobalAuralizationMode = iGlobalAuralizationMode;
}

void CVABinauralClusteringRenderer::ProcessStream( const ITAStreamInfo* streamInfo )
{
#ifdef BINAURAL_CLUSTERING_RENDERER_WITH_BENCHMARKING

	auto start = std::chrono::high_resolution_clock::now( );
	LARGE_INTEGER frequency; // ticks per second
	LARGE_INTEGER t1, t2;    // ticks
	double elapsedTime;

	// get ticks per second
	QueryPerformanceFrequency( &frequency );

	// start timer
	QueryPerformanceCounter( &t1 );

#endif // BINAURAL_CLUSTERING_RENDERER_WITH_BENCHMARKING

	/////
	ITASampleFrame* psfOutput;
	float* pfOutputL = GetWritePointer( 0 );
	float* pfOutputR = GetWritePointer( 1 );

	fm_zero( pfOutputL, GetBlocklength( ) );
	fm_zero( pfOutputR, GetBlocklength( ) );

	/* @todo jst: why is this outcommented? Trajectories must be updated here for Doppler shifts ...
	const CVAAudiostreamState* streamState = dynamic_cast< const CVAAudiostreamState* >( streamInfo );
	double time = streamState->dSysTime;
	updateTrajectories(time);
	*/

	// -- create output for every listener
	for( auto const& cit: m_pClusterEngine->m_mReceiverClusteringInstances )
	{
		CVABinauralClustering* pClustering( cit.second );
		int iID        = pClustering->GetSoundReceiverID( );
		auto pReceiver = m_mBinauralReceivers[iID];
		if( !pReceiver->pData->bMuted )
		{
			psfOutput = pClustering->GetOutput( );
			fm_add( pfOutputL, ( *psfOutput )[0].data( ), m_uiBlocklength );
			fm_add( pfOutputR, ( *psfOutput )[1].data( ), m_uiBlocklength );
		}
	}

#ifdef BINAURAL_CLUSTERING_RENDERER_WITH_BENCHMARKING

	// stop timer
	QueryPerformanceCounter( &t2 );

	// compute and print the elapsed time in millisec
	elapsedTime = ( t2.QuadPart - t1.QuadPart ) * 1000.0 / frequency.QuadPart;

	std::ofstream outfile( "X:/Sciebo/master-thesis/data/benchmarks/Stream_128.txt", std::fstream::app );
	outfile << elapsedTime << std::endl;
	outfile.close( );

#endif // BINAURAL_CLUSTERING_RENDERER_WITH_BENCHMARKING

	IncrementWritePointer( );
}

void CVABinauralClusteringRenderer::UpdateScene( CVASceneState* pNewSceneState )
{
	assert( pNewSceneState );

	m_pNewSceneState = pNewSceneState;
	if( m_pNewSceneState == m_pCurSceneState )
		return;

	// get reference to new scene
	m_pNewSceneState->AddReference( );

	// get difference between old scene state and current scene state
	CVASceneStateDiff diff;
	m_pNewSceneState->Diff( m_pCurSceneState, &diff );

#ifdef BINAURAL_CLUSTERING_RENDERER_WITH_BENCHMARKING

	LARGE_INTEGER frequency; // ticks per second
	LARGE_INTEGER t1, t2;    // ticks
	double elapsedTime;

	// get ticks per second
	QueryPerformanceFrequency( &frequency );

	// start timer
	QueryPerformanceCounter( &t1 );

#endif // BINAURAL_CLUSTERING_RENDERER_WITH_BENCHMARKING

	// Update receivers
	UpdateSoundReceivers( &diff );

	// Update wave fronts
	UpdateSoundSources( &diff );

	m_pClusterEngine->Update( );

#ifdef BINAURAL_CLUSTERING_RENDERER_WITH_BENCHMARKING

	// stop timer
	QueryPerformanceCounter( &t2 );

	// compute and print the elapsed time in millisec
	elapsedTime = ( t2.QuadPart - t1.QuadPart ) * 1000.0 / frequency.QuadPart;

	std::ofstream outfile( "X:/Sciebo/master-thesis/data/benchmarks/UpdateScene_128.txt", std::fstream::app );
	outfile << elapsedTime << std::endl;
	outfile.close( );

#endif // BINAURAL_CLUSTERING_RENDERER_WITH_BENCHMARKING

	// Alte Szene freigeben (dereferenzieren)
	if( m_pCurSceneState )
		m_pCurSceneState->RemoveReference( );

	m_pCurSceneState = m_pNewSceneState;
	m_pNewSceneState = nullptr;
}

void CVABinauralClusteringRenderer::UpdateSoundSources( CVASceneStateDiff* pDiff )
{
	typedef std::vector<int>::const_iterator icit_t;

	// Delete sources
	for( icit_t it = pDiff->viDelSoundSourceIDs.begin( ); it != pDiff->viDelSoundSourceIDs.end( ); )
	{
		const int& iID( *it++ );
		DeleteSoundSource( iID );
	}

	// Add sources
	for( icit_t it = pDiff->viNewSoundSourceIDs.begin( ); it != pDiff->viNewSoundSourceIDs.end( ); )
	{
		const int& iID( *it++ );
		CreateSoundSource( iID, m_pNewSceneState->GetSoundSourceState( iID ) );
	}

	// Update wave front trajectories
	std::map<int, CVABinauralWaveFrontBase*>::iterator it;

	for( it = m_mWaveFronts.begin( ); it != m_mWaveFronts.end( ); ++it )
	{
		int iWaveFrontID                     = it->first;
		CVABinauralWaveFrontBase* pWaveFront = it->second;

		CVASoundSourceState* pSourceStateCur = ( m_pCurSceneState ? m_pCurSceneState->GetSoundSourceState( iWaveFrontID ) : nullptr );
		CVASoundSourceState* pSourceStateNew = ( m_pNewSceneState ? m_pNewSceneState->GetSoundSourceState( iWaveFrontID ) : nullptr );

		if( pSourceStateNew && ( pSourceStateNew != pSourceStateCur ) )
		{
			pWaveFront->pState = pSourceStateNew;
		}

		const CVAMotionState* pMotionStateCur = ( pSourceStateCur ? pSourceStateCur->GetMotionState( ) : nullptr );
		const CVAMotionState* pMotionStateNew = ( pSourceStateNew ? pSourceStateNew->GetMotionState( ) : nullptr );

		if( pMotionStateNew && ( pMotionStateNew != pMotionStateCur ) )
		{
			pWaveFront->pMotionModel->InputMotionKey( pMotionStateNew );

			// Dirty Hack

			VAVec3 v3SourcePos = pMotionStateNew->GetPosition( );
			pWaveFront->SetWaveFrontOrigin( v3SourcePos );
		}
	}
}

void CVABinauralClusteringRenderer::UpdateSoundReceivers( CVASceneStateDiff* diff )
{
	typedef std::vector<int>::const_iterator icit_t;

	// Delete receivers
	for( icit_t it = diff->viDelReceiverIDs.begin( ); it != diff->viDelReceiverIDs.end( ); )
	{
		const int& iID( *it++ );
		DeleteSoundReceiver( iID );
	}

	// Add receivers
	for( icit_t it = diff->viNewReceiverIDs.begin( ); it != diff->viNewReceiverIDs.end( ); )
	{
		const int& iID( *it++ );
		CreateSoundReceiver( iID, m_pNewSceneState->GetReceiverState( iID ) );
	}

	// Update receiver trajectories and directivity
	for( std::map<int, CVABinauralClusteringReceiver*>::iterator it = m_mBinauralReceivers.begin( ); it != m_mBinauralReceivers.end( ); ++it )
	{
		int iReceiverID                          = it->first;
		CVABinauralClusteringReceiver* pReceiver = it->second;

		CVAReceiverState* pReceiverStateCur = ( m_pCurSceneState ? m_pCurSceneState->GetReceiverState( iReceiverID ) : nullptr );
		CVAReceiverState* pReceiverStateNew = ( m_pNewSceneState ? m_pNewSceneState->GetReceiverState( iReceiverID ) : nullptr );

		const CVAMotionState* pMotionCur = ( pReceiverStateCur ? pReceiverStateCur->GetMotionState( ) : nullptr );
		const CVAMotionState* pMotionNew = ( pReceiverStateNew ? pReceiverStateNew->GetMotionState( ) : nullptr );


		if( pMotionNew && ( pMotionNew != pMotionCur ) )
		{
			pReceiver->pMotionModel->InputMotionKey( pMotionNew );

			// Dirty Hack

			pReceiver->v3PredictedPosition = pMotionNew->GetPosition( );
			pReceiver->v3PredictedView     = pMotionNew->GetView( );
			pReceiver->v3PredictedUp       = pMotionNew->GetUp( );
			pReceiver->bHasValidTrajectory = true;
		}

		if( pReceiverStateNew != nullptr )
		{
			pReceiver->pDirectivity = (IVADirectivity*)pReceiverStateNew->GetDirectivity( );
		}
	}
}

void CVABinauralClusteringRenderer::CreateSoundSource( const int iSoundSourceID, const CVASoundSourceState* pState )
{
	CVABinauralWaveFrontBase* pSource = dynamic_cast<CVABinauralWaveFrontBase*>( m_pSourcePool->RequestObject( ) ); // Reference = 1

	// set state
	pSource->pState = (CVASoundSourceState*)pState;
	// set internal data
	pSource->pSoundSourceData = m_pCore->GetSceneManager( )->GetSoundSourceDesc( iSoundSourceID );
	pSource->pSoundSourceData->AddReference( );

	// set motion model
	CVABasicMotionModel* pMotionModel = dynamic_cast<CVABasicMotionModel*>( pSource->pMotionModel->GetInstance( ) );
	pMotionModel->SetName( std::string( "bfrend_mm_source_" + pSource->pSoundSourceData->sName ) );
	pMotionModel->Reset( );

	// add local reference
	m_mWaveFronts.insert( std::pair<int, CVABinauralWaveFrontBase*>( iSoundSourceID, pSource ) );

	// add source to clustering
	m_pClusterEngine->AddWaveFront( pSource );
}

void CVABinauralClusteringRenderer::DeleteSoundSource( int iSoundSourceID )
{
	// remove local source reference
	std::map<int, CVABinauralWaveFrontBase*>::iterator it = m_mWaveFronts.find( iSoundSourceID );
	CVABinauralWaveFrontBase* pSource                     = it->second;
	m_mWaveFronts.erase( it );

	// remove listener reference from clustering
	m_pClusterEngine->RemoveWaveFront( pSource );
	pSource->RemoveReference( );
}

void CVABinauralClusteringRenderer::CreateSoundReceiver( int iSoundReceiverID, const CVAReceiverState* pReceiverState )
{
	CVABinauralClusteringReceiver* pReceiver = dynamic_cast<CVABinauralClusteringReceiver*>( m_pReceiverPool->RequestObject( ) ); // Reference = 1

	// set internal data
	pReceiver->pData = m_pCore->GetSceneManager( )->GetSoundReceiverDesc( iSoundReceiverID );
	pReceiver->pData->AddReference( );

	// set motion model
	CVABasicMotionModel* pMotionModel = dynamic_cast<CVABasicMotionModel*>( pReceiver->pMotionModel->GetInstance( ) );
	pMotionModel->SetName( std::string( "bfrend_mm_listener_" + pReceiver->pData->sName ) );
	pMotionModel->Reset( );

	// set directivity
	pReceiver->pDirectivity = (IVADirectivity*)pReceiverState->GetDirectivity( );

	// add local reference
	m_mBinauralReceivers.insert( std::pair<int, CVABinauralClusteringReceiver*>( iSoundReceiverID, pReceiver ) );

	// add listener to clustering
	CVABinauralClusteringEngine::Config config = { m_iNumClusters };
	m_pClusterEngine->AddReceiver( iSoundReceiverID, pReceiver, config, m_dAngularDistanceThreshold );
}

void CVABinauralClusteringRenderer::DeleteSoundReceiver( int iSoundReceiverID )
{
	// remove local listener reference
	std::map<int, CVABinauralClusteringReceiver*>::iterator it = m_mBinauralReceivers.find( iSoundReceiverID );
	CVABinauralClusteringReceiver* pReceiver                   = it->second;
	m_mBinauralReceivers.erase( it );

	pReceiver->RemoveReference( );

	// remove listener reference from clustering
	m_pClusterEngine->RemoveReceiver( iSoundReceiverID );
}

void CVABinauralClusteringRenderer::UpdateMotionStates( )
{
	// Neue Quellendaten bernehmen
	for( std::map<int, CVABinauralWaveFrontBase*>::iterator it = m_mWaveFronts.begin( ); it != m_mWaveFronts.end( ); ++it )
	{
		int sourceID                         = it->first;
		CVABinauralWaveFrontBase* pWaveFront = it->second;

		CVASoundSourceState* pSourceStateCur = ( m_pCurSceneState ? m_pCurSceneState->GetSoundSourceState( sourceID ) : nullptr );
		CVASoundSourceState* pSourceStateNew = ( m_pNewSceneState ? m_pNewSceneState->GetSoundSourceState( sourceID ) : nullptr );

		const CVAMotionState* pMotionStateCur = ( pSourceStateCur ? pSourceStateCur->GetMotionState( ) : nullptr );
		const CVAMotionState* pMotionStateNew = ( pSourceStateNew ? pSourceStateNew->GetMotionState( ) : nullptr );

		if( pMotionStateNew && ( pMotionStateNew != pMotionStateCur ) )
		{
			pWaveFront->pMotionModel->InputMotionKey( pMotionStateNew );
		}
	}
}

void CVABinauralClusteringRenderer::UpdateTrajectories( double dTime )
{
	for( auto const& cit_wf: m_mWaveFronts )
	{
		CVABinauralWaveFrontBase* pWaveFront = cit_wf.second;

		pWaveFront->pMotionModel->HandleMotionKeys( );
		VAVec3 v3PredictedWaveFrontOrigin;
		pWaveFront->pMotionModel->EstimatePosition( dTime, v3PredictedWaveFrontOrigin );
		pWaveFront->SetWaveFrontOrigin( v3PredictedWaveFrontOrigin );
	}

	for( auto const& cit_receiver: m_mBinauralReceivers )
	{
		bool isValid                             = true;
		CVABinauralClusteringReceiver* pReceiver = cit_receiver.second;

		pReceiver->pMotionModel->HandleMotionKeys( );
		isValid &= pReceiver->pMotionModel->EstimatePosition( dTime, pReceiver->v3PredictedPosition );
		isValid &= pReceiver->pMotionModel->EstimateOrientation( dTime, pReceiver->v3PredictedView, pReceiver->v3PredictedUp );
		pReceiver->bHasValidTrajectory = isValid;
	}
}
