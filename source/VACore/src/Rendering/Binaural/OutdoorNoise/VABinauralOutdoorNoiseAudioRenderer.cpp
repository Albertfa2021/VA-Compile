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

#include "VABinauralOutdoorNoiseAudioRenderer.h"

#if VACORE_WITH_RENDERER_BINAURAL_OUTDOOR_NOISE

#	include <atomic>
#	include <chrono>
#	include <ctime>
#	include <fstream>
#	include <iostream>
#	include <sstream>
#	include <string>
#	include <time.h>
#	include <vector>

// VA Includes
#	include "../../../Utils/VAUtils.h"
#	include "../../../core/core.h"

#	include <VA.h>
#	include <VAObjectPool.h>

// VA Core Includes
#	include "../../../Scene/VAScene.h"
#	include "../../../Scene/VASoundSourceDesc.h"

// ITA includes
#	include <ITA/SimulationScheduler/OutdoorAcoustics/outdoor_simulation_result.h>
#	include <ITA/SimulationScheduler/scheduler.h>
#	include <ITAException.h>
#	include <ITAFastMath.h>
#	include <ITASampleBuffer.h>
#	include <ITAStreamInfo.h>
#	include <ITAThirdOctaveFilterbank.h>
#	include <ITAVariableDelayLine.h>

// Vista includes
#	include <VistaTools/VistaIniFileParser.h>

// Utils
#	include "../Clustering/Receiver/VABinauralClusteringReceiverPoolFactory.h"
#	include "VABinauralOutdoorSource.h"
#	include "VABinauralOutdoorWaveFront.h"
#	include "VABinauralOutdoorWaveFrontPoolFactory.h"


CVABinauralOutdoorNoiseRenderer::CVABinauralOutdoorNoiseRenderer( const CVAAudioRendererInitParams& oParams )
    : ITADatasourceRealization( 2, oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.dSampleRate, oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.iBuffersize )
    , m_oParams( oParams )
    , m_pCore( oParams.pCore )
    , m_pNewSceneState( NULL )
    , m_pCurSceneState( NULL )
    , m_bIndicateReset( false )
    , m_bResetAck( false )
{
	Init( *m_oParams.pConfig );

	IVAPoolObjectFactory* pReceiverFactory = new CVABinauralClusteringReceiverPoolFactory( m_pCore, m_oDefaultReceiverConf );
	m_pReceiverPool                        = IVAObjectPool::Create( 16, 2, pReceiverFactory, true );

	IVAPoolObjectFactory* pWaveFrontFactory = new CVABinauralOutdoorWaveFrontPoolFactory( m_oDefaultWaveFrontConf );
	m_pWaveFrontPool                        = IVAObjectPool::Create( 16, 2, pWaveFrontFactory, true );

	m_iCurGlobalAuralizationMode = IVAInterface::VA_AURAMODE_DEFAULT;

	m_sbSourceInputTemp.Init( GetBlocklength( ), false );
}

CVABinauralOutdoorNoiseRenderer::~CVABinauralOutdoorNoiseRenderer( )
{
#	ifdef BINAURAL_OUTDOOR_NOISE_INSOURCE_BENCHMARKS
	std::cout << "BinauralOutdoorNoiseRenderer GetOutput statistics: " << m_swProcessStream << std::endl;
#	endif

	// we detach the result handler so that we do not get results in this thread after it ended.
	if( m_pSimulationScheduler )
		m_pSimulationScheduler->DetachResultHandler( this );

	delete m_pClusterEngine;

	delete m_pReceiverPool;
	delete m_pWaveFrontPool;
}

void CVABinauralOutdoorNoiseRenderer::Init( const CVAStruct& oArgs )
{
	CVAConfigInterpreter oConf( oArgs );

	// Set interpolation algorithm
	std::string sVDLInterpolationAlgorithm;
	oConf.OptString( "SwitchingAlgorithm", sVDLInterpolationAlgorithm, "linear" );
	sVDLInterpolationAlgorithm = toLowercase( sVDLInterpolationAlgorithm );

	if( sVDLInterpolationAlgorithm == "linear" )
		m_oDefaultWaveFrontConf.iInterpolationFunction = ITABase::InterpolationFunctions::LINEAR;
	else if( sVDLInterpolationAlgorithm == "cubicspline" )
		m_oDefaultWaveFrontConf.iInterpolationFunction = ITABase::InterpolationFunctions::CUBIC_SPLINE;
	else
		ITA_EXCEPT1( INVALID_PARAMETER, "Unrecognized interpolation algorithm '" + sVDLInterpolationAlgorithm + "' in BinauralOutdoorAudioRendererConfig" );

	oConf.OptInteger( "MaxDelaySamples", m_iMaxDelaySamples, 3 * 44100 );

	// Filter property histories
	oConf.OptBool( "FilterPropertyHistoriesEnabled", m_bFilterPropertyHistoriesEnabled, false );

	// HRTF filter length
	oConf.OptInteger( "HRIRFilterLength", m_iHRIRFilterLength, 256 );

	// Default listener config, @todo rest
	m_oDefaultReceiverConf.bMotionModelLogEstimated = false;
	m_oDefaultReceiverConf.bMotionModelLogInput     = false;
	oConf.OptInteger( "MotionModelNumHistoryKeys", m_oDefaultReceiverConf.iMotionModelNumHistoryKeys, 1000 );

	if( m_oDefaultReceiverConf.iMotionModelNumHistoryKeys < 1 )
		VA_EXCEPT2( INVALID_PARAMETER, "Basic motion model history needs to be greater than zero" );

	oConf.OptNumber( "MotionModelWindowSize", m_oDefaultReceiverConf.dMotionModelWindowSize, 0.1f );
	oConf.OptNumber( "MotionModelWindowDelay", m_oDefaultReceiverConf.dMotionModelWindowDelay, 0.1f );

	if( ( m_oDefaultReceiverConf.dMotionModelWindowSize <= 0 ) || ( m_oDefaultReceiverConf.dMotionModelWindowDelay < 0 ) )
		VA_EXCEPT2( INVALID_PARAMETER, "Basic motion model window parameters parse error (zero or negative?)" );

	// default source config
	oConf.OptInteger( "MotionModelNumHistoryKeys", m_oDefaultWaveFrontConf.iMotionModelNumHistoryKeys, 1000 );

	if( m_oDefaultWaveFrontConf.iMotionModelNumHistoryKeys < 1 )
		VA_EXCEPT2( INVALID_PARAMETER, "Basic motion model history needs to be greater than zero" );

	oConf.OptNumber( "MotionModelWindowSize", m_oDefaultWaveFrontConf.dMotionModelWindowSize, 0.1f );
	oConf.OptNumber( "MotionModelWindowDelay", m_oDefaultWaveFrontConf.dMotionModelWindowDelay, 0.1f );

	if( ( m_oDefaultWaveFrontConf.dMotionModelWindowSize <= 0 ) || ( m_oDefaultWaveFrontConf.dMotionModelWindowDelay < 0 ) )
		VA_EXCEPT2( INVALID_PARAMETER, "Basic motion model window parameters parse error (zero or negative?)" );

	m_oDefaultWaveFrontConf.iBlockLength = GetBlocklength( );
	m_oDefaultWaveFrontConf.dSampleRate  = GetSampleRate( );

	oConf.OptInteger( "IIRFilterOrder", m_oDefaultWaveFrontConf.iIIRFilterOrder, 4 );
	oConf.OptInteger( "BurgFIRLength", m_oDefaultWaveFrontConf.iFIRFilterLength, 1024 );
	oConf.OptInteger( "InterpolationBufferSize", m_oDefaultWaveFrontConf.iBufferSize, GetBlocklength( ) * 10 );

	m_oDefaultWaveFrontConf.iFilterDesignAlgorithm = ITADSP::BURG; // No other algorithms currently implemented

	oConf.OptInteger( "NumClusters", m_iNumClusters, 12 );
	oConf.OptNumber( "AngularDistanceThreshold", m_dAngularDistanceThreshold, 4.0f / float( m_iNumClusters ) );

	// Initialize clustering
	m_pClusterEngine = new CVABinauralClusteringEngine( GetBlocklength( ), m_iHRIRFilterLength );

	// Filterproperty histories
	oConf.OptInteger( "FilterPropertyHistorySize", m_oDefaultWaveFrontConf.iHistoryBufferSize, 1000 );
	if( m_oDefaultWaveFrontConf.iHistoryBufferSize < 1 )
		VA_EXCEPT2( INVALID_PARAMETER, "Filter property history needs to be greater than zero" );

	std::string sSimulationConfigFilePathRaw;
	oConf.OptString( "SimulationConfigFilePath", sSimulationConfigFilePathRaw );

	if( !sSimulationConfigFilePathRaw.empty( ) )
	{
		auto sSimConfigFilePath = m_pCore->FindFilePath( sSimulationConfigFilePathRaw );

		VA_VERBOSE( "BinauralOutdoorNoiseRenderer", "Attempting to load file from path '" << sSimConfigFilePath << "' as simulation config file" );

		ITA::SimulationScheduler::CScheduler::LocalSchedulerConfig oLocalSchedConf;
		VistaPropertyList oSimulationProperties;

		size_t i = sSimConfigFilePath.rfind( '.', sSimConfigFilePath.length( ) );
		if( i != std::string::npos )
		{
			const auto sFileExtension = sSimConfigFilePath.substr( i + 1, sSimConfigFilePath.length( ) - i );

			if( toLowercase( sFileExtension ) == "json" )
			{
				oLocalSchedConf.Load( ITA::SimulationScheduler::Utils::JSONConfigUtils::LoadJSONConfig( sSimConfigFilePath ) );
			}
			else if( toLowercase( sFileExtension ) == "ini" )
			{
				if( !VistaIniFileParser::ReadProplistFromFile( sSimConfigFilePath, oSimulationProperties ) )
				{
					VA_INFO( "BinauralOutdoorNoiseRenderer", "Using default simulation scheduler config." );
				}
				else
				{
					oLocalSchedConf.Load( oSimulationProperties );
				}
			}
			else
				VA_EXCEPT_NOT_IMPLEMENTED;
		}

		// Initialize simulation backend
		m_pSimulationScheduler = std::make_unique<ITA::SimulationScheduler::CScheduler>( oLocalSchedConf );

		/* please implement on scheduler side and uncomment
		if( m_pMasterSimulationController->GetNumChannels() != m_pOutput->GetNumberOfChannels() )
		VA_EXCEPT2( INVALID_PARAMETER, "Binaural outdoor noise renderer and simulation scheduler have mismatching channel number" );
		*/

		m_pSimulationScheduler->AttachResultHandler( this );
	}
}

void CVABinauralOutdoorNoiseRenderer::Reset( )
{
	if( m_pSimulationScheduler )
		m_pSimulationScheduler->PushUpdate(
		    std::make_unique<ITA::SimulationScheduler::CUpdateConfig>( ITA::SimulationScheduler::CUpdateConfig::ConfigChangeType::resetAll ) );

	VA_WARN( "BinauralOutdoorNoiseRenderer", "Reset hast not yet been fully implemented" );
}

ITADatasource* CVABinauralOutdoorNoiseRenderer::GetOutputDatasource( )
{
	return this;
}

void CVABinauralOutdoorNoiseRenderer::SetParameters( const CVAStruct& oInArgs )
{
	auto it = oInArgs.Begin( );
	while( it != oInArgs.End( ) )
	{
		const CVAStructValue& oPathValue( it++->second ); // extract data for the current path from the iterator in a useful form; increment iterator, too

		if( !oPathValue.IsStruct( ) ) // Ignore other elements than structs
			continue;

		const CVAStruct& oPath( oPathValue );
		if( !oPath.HasKey( "identifier" ) )
		{
			VA_WARN( "BinauralOutdoorNoiseRenderer", "Called SetParameters with a path missing the 'identifier' key, skipping this path." );
			continue;
		}
		// This is the unique string which identifies the current path for which data is being given (stored in "path")
		std::string sIdentifierHash = oPath["identifier"];

		// Source
		if( !oPath.HasKey( "source" ) )
		{
			VA_WARN( "BinauralOutdoorNoiseRenderer", "Called SetParameters with a path missing the 'source' key, skipping this path." );
			continue;
		}
		const int iPathSourceID = oPath["source"];
		auto its                = m_mSources.find( iPathSourceID );
		if( its == m_mSources.end( ) )
		{
			VA_WARN( "BinauralOutdoorNoiseRenderer", "Source ID not (yet) found, make sure the source associated with each path exists." );
			continue;
		}
		CVABinauralOutdoorSource* pSource = its->second;

		// Receiver
		if( !oPath.HasKey( "receiver" ) )
		{
			VA_WARN( "BinauralOutdoorNoiseRenderer", "Called SetParameters with a path missing the 'receiver' key, skipping this path." );
			continue;
		}
		const int iPathReceiverID = oPath["receiver"];
		auto itr                  = m_mBinauralReceivers.find( iPathReceiverID );
		if( itr == m_mBinauralReceivers.end( ) )
		{
			VA_WARN( "BinauralOutdoorNoiseRenderer", "Receiver ID not (yet) found, make sure the source associated with each path exists." );
			continue;
		}
		CVABinauralClusteringReceiver* pReceiver = itr->second;

		auto pSRPair = m_lpSourceReceiverPairs.Find( pSource, pReceiver );
		if( !pSRPair )
		{
			VA_WARN( "BinauralOutdoorNoiseRenderer", "Source-receiver pair not found." );
			continue;
		}

		CVABinauralOutdoorWaveFront* pWavefront = pSRPair->GetWavefront( sIdentifierHash );

		bool bDeletePathRequested = false;
		if( oPath.HasKey( "delete" ) )
			bDeletePathRequested = oPath["delete"];

		if( bDeletePathRequested )
		{
			if( pWavefront )
			{
				// @todo fix removing wave fronts!
				// enable lines below after fix:
				// m_pClusterEngine->RemoveWaveFront(pWavefront);
				// pSRPair->RemoveWavefront(sIdentifierHash);

				pWavefront->SetAudible( false ); // override audibility as long as path is not removed correctly, do not change
			}
		}
		else
		{
			if( !pWavefront )
			{
				pWavefront = dynamic_cast<CVABinauralOutdoorWaveFront*>( m_pWaveFrontPool->RequestObject( ) ); // request a new path from the pool
				pWavefront->SetSource( pSource );
				pWavefront->SetReceiver( pReceiver );
				pSRPair->AddWavefront( sIdentifierHash, pWavefront );
				m_pClusterEngine->AddWaveFront( pWavefront ); // add new wave front to the clustering engine
			}

			//@todo psc: Do we really need this? Wouldn't it be better to skip this path if not all keys are available?
			bool bAudible = false; // if this is the first time this path is used, and it does not have the right keys,needs to be set inaudible

			double dTimeAtReceiver;
			if( oPath.HasKey( "delay" ) )
			{
				const double dDelaySeconds = oPath["delay"];
				dTimeAtReceiver            = dDelaySeconds + m_pCore->GetCoreClock( );
				if( m_bFilterPropertyHistoriesEnabled )
					pWavefront->PushDelay( dTimeAtReceiver, dDelaySeconds );
				else
					pWavefront->SetDelay( dDelaySeconds );
			}
			else if( m_bFilterPropertyHistoriesEnabled )
				VA_EXCEPT2( INVALID_PARAMETER, "CVABinauralOutdoorNoiseRenderer: delay is a required parameter if using filter property histories." );


			auto pThirdOctMags = std::make_unique<ITABase::CThirdOctaveGainMagnitudeSpectrum>( );
			pThirdOctMags->SetIdentity( );
			if( oPath.HasKey( "frequency_magnitudes" ) )
			{
				const CVAStructValue& sMags = oPath["frequency_magnitudes"];          //
				int iNumValues              = sMags.GetDataSize( ) / sizeof( float ); // currently only accepts third octave values, this is a safety check
				if( iNumValues != pThirdOctMags->GetNumBands( ) )
					VA_EXCEPT1( "CVABinauralOutdoorNoiseRenderer: Expected 31 frequency magnitudes." );

				const float* pfMags = (const float*)( sMags.GetData( ) ); // convert to float values
				for( int i = 0; i < pThirdOctMags->GetNumBands( ); i++ )
					pThirdOctMags->SetMagnitude( i, pfMags[i] );

				bAudible = true;
			}
			if( oPath.HasKey( "source_wavefront_normal" ) )
			{
				CVAStructValue position = oPath["source_wavefront_normal"];
				int num_values          = position.GetDataSize( ) / sizeof( float ); // safety check to make sure a 3D vector is passed
				if( num_values != 3 )
					VA_EXCEPT1( "CVABinauralOutdoorNoiseRenderer: Expected a source_wavefront_normal vector with 3 components." );
				const float* pfPosition = (const float*)( position.GetData( ) ); // convert to float values

				auto v3SourceWFNormal                                   = VAVec3( pfPosition[0], pfPosition[1], pfPosition[2] );
				ITABase::CThirdOctaveGainMagnitudeSpectrum oTmpSpectrum = pSource->GetDirectivitySpectrum( m_pCore->GetCoreClock( ), v3SourceWFNormal );
				pThirdOctMags->Multiply( oTmpSpectrum );
			}
			if( m_bFilterPropertyHistoriesEnabled )
				pWavefront->PushFilterCoefficients( dTimeAtReceiver, std::move( pThirdOctMags ) );
			else
				pWavefront->SetFilterCoefficients( *pThirdOctMags );

			if( oPath.HasKey( "gain" ) )
			{
				double dGain = oPath["gain"];
				if( m_bFilterPropertyHistoriesEnabled )
					pWavefront->PushGain( dTimeAtReceiver, dGain );
				else
					pWavefront->SetGain( (float)dGain ); // set the gain to be used for each wave front
				bAudible = true;
			}

			if( oPath.HasKey( "position" ) ) // @todo: rename to incoming_wavefont_normal
			{
				CVAStructValue position = oPath["position"];
				int num_values          = position.GetDataSize( ) / sizeof( float ); // safety check to make sure a 3D vector is passed
				if( num_values != 3 )
					VA_EXCEPT1( "CVABinauralOutdoorNoiseRenderer: Expected a position vector with 3 components." );
				void* pvposition        = position.GetData( );
				const float* pfPosition = (const float*)pvposition; // convert to float values

				auto v3WFOrigin = std::make_unique<VAVec3>( pfPosition[0], pfPosition[1], pfPosition[2] );
				if( m_bFilterPropertyHistoriesEnabled )
					pWavefront->PushWFOrigin( dTimeAtReceiver, std::move( v3WFOrigin ) );
				else
					pWavefront->SetWaveFrontOrigin( *v3WFOrigin );
				bAudible = true;
			}

			if( oPath.HasKey( "print_benchmarks" ) )
				pWavefront->SetParameters( oPath );

			if( oPath.HasKey( "audible" ) )
				bAudible = bAudible && oPath["audible"];

			if( m_bFilterPropertyHistoriesEnabled )
				pWavefront->PushAudibility( dTimeAtReceiver, bAudible );
			else
				pWavefront->SetAudible( bAudible );


			//@todo psc: Check if it is necessary to add WF to Cluster-Engine in the very end
			// Add the newly created wavefront to the clustering engine (but only if audible ok)
			// if( bCreatedNewPath && ( bAudible == true ) )
			//	m_pClusterEngine->AddWaveFront( pWavefront ); //add new wave front to the clustering engine
		}
	}

	VA_TRACE( "BinauralOutdoorNoiseRenderer", "Received input arguments: " + oInArgs.ToString( ) );
}

CVAStruct CVABinauralOutdoorNoiseRenderer::GetParameters( const CVAStruct& ) const
{
	CVAStruct oOutArgs;
	oOutArgs["info"] = "Nothing meaningful to return, yet";
	return oOutArgs;
}

void CVABinauralOutdoorNoiseRenderer::UpdateGlobalAuralizationMode( int iGlobalAuralizationMode )
{
	if( m_iCurGlobalAuralizationMode != iGlobalAuralizationMode )
		m_iCurGlobalAuralizationMode = iGlobalAuralizationMode;
}

void CVABinauralOutdoorNoiseRenderer::ProcessStream( const ITAStreamInfo* streamInfo )
{
#	ifdef BINAURAL_OUTDOOR_NOISE_INSOURCE_BENCHMARKS
	m_swProcessStream.start( );
#	endif

	float* pfOutputL = GetWritePointer( 0 );
	float* pfOutputR = GetWritePointer( 1 );

	fm_zero( pfOutputL, GetBlocklength( ) );
	fm_zero( pfOutputR, GetBlocklength( ) );


	const CVAAudiostreamState* streamState = dynamic_cast<const CVAAudiostreamState*>( streamInfo );
	double dTime                           = streamState->dSysTime;


	// Issue simulation updates

	UpdateTrajectories( dTime );

	if( m_bFilterPropertyHistoriesEnabled )
		UpdateFilterPropertyHistories( dTime );

	if( m_pSimulationScheduler )
		RequestSimulationUpdate( dTime );


	// Copy new samples from source' signal stream and feed the SIMO VDLs

	for( auto oSource: m_mSources )
	{
		CVABinauralOutdoorSource* pSource = oSource.second;
		CVASoundSourceDesc* pSourceDesc   = pSource->pData;

		const ITASampleFrame& sfInputBuffer( *pSourceDesc->psfSignalSourceInputBuf );
		const ITASampleBuffer* psbInput( &( sfInputBuffer[0] ) );

		// Apply sound source sound power
		float fSoundSourceGain = float( pSource->pState->GetVolume( m_oParams.pCore->GetCoreConfig( )->dDefaultAmplitudeCalibration ) );
		m_sbSourceInputTemp.write( psbInput, GetBlocklength( ) );
		m_sbSourceInputTemp.mul_scalar( fSoundSourceGain );

		pSource->pVDL->Write( GetBlocklength( ), m_sbSourceInputTemp.GetData( ) );
	}


	// Create output for every receiver (query output per receiver clustering instance)

	for( auto const& cit: m_pClusterEngine->m_mReceiverClusteringInstances )
	{
		CVABinauralClustering* pReceiverInstance( cit.second );
		auto pReceiver = m_mBinauralReceivers[cit.first];

		if( !pReceiver->pData->bMuted )
		{
			ITASampleFrame* psfOutput = pReceiverInstance->GetOutput( );
			fm_add( pfOutputL, ( *psfOutput )[0].GetData( ), GetBlocklength( ) );
			fm_add( pfOutputR, ( *psfOutput )[1].GetData( ), GetBlocklength( ) );
		}
	}


	// Increment write cursors / write pointers

	for( auto oSource: m_mSources )
	{
		CVABinauralOutdoorSource* pSource( oSource.second );
		pSource->pVDL->Increment( GetBlocklength( ) );
	}

	IncrementWritePointer( );

#	ifdef BINAURAL_OUTDOOR_NOISE_INSOURCE_BENCHMARKS
	m_swProcessStream.stop( );
#	endif
}

void CVABinauralOutdoorNoiseRenderer::UpdateScene( CVASceneState* pNewSceneState )
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

	// Update receivers
	UpdateSoundReceivers( &diff );

	// Update wave fronts
	UpdateSoundSources( &diff );

	m_pClusterEngine->Update( );

	// Alte Szene freigeben (dereferenzieren)
	if( m_pCurSceneState )
		m_pCurSceneState->RemoveReference( );

	m_pCurSceneState = m_pNewSceneState;
	m_pNewSceneState = nullptr;
}

void CVABinauralOutdoorNoiseRenderer::UpdateSoundSources( CVASceneStateDiff* diff )
{
	// This function is useless until we handle the wave fronts internally
	// Currently only possible via DSP coefficient update using SetParameter()
	// and a corresponding update struct.

	typedef std::vector<int>::const_iterator icit_t;

	// Delete sources
	for( icit_t it = diff->viDelSoundSourceIDs.begin( ); it != diff->viDelSoundSourceIDs.end( ); )
	{
		const int& iID( *it++ );
		DeleteSoundSource( iID );
	}

	// Add sources
	for( icit_t it = diff->viNewSoundSourceIDs.begin( ); it != diff->viNewSoundSourceIDs.end( ); )
	{
		const int& iID( *it++ );
		CreateSoundSource( iID, m_pNewSceneState->GetSoundSourceState( iID ) );
	}

	// Update wave front trajectories
	std::map<int, CVABinauralOutdoorSource*>::iterator it;

	for( it = m_mSources.begin( ); it != m_mSources.end( ); ++it )
	{
		int iSourceID                     = it->first;
		CVABinauralOutdoorSource* pSource = it->second;

		CVASoundSourceState* sourceCur = ( m_pCurSceneState ? m_pCurSceneState->GetSoundSourceState( iSourceID ) : nullptr );
		CVASoundSourceState* sourceNew = ( m_pNewSceneState ? m_pNewSceneState->GetSoundSourceState( iSourceID ) : nullptr );

		if( sourceNew && ( sourceNew != sourceCur ) )
		{
			pSource->pState = sourceNew;
		}

		const CVAMotionState* motionCur = ( sourceCur ? sourceCur->GetMotionState( ) : nullptr );
		const CVAMotionState* motionNew = ( sourceNew ? sourceNew->GetMotionState( ) : nullptr );

		if( motionNew && ( motionNew != motionCur ) )
		{
			pSource->pMotionModel->InputMotionKey( motionNew );
		}

		if( sourceNew != nullptr )
			pSource->pDirectivity = (IVADirectivity*)sourceNew->GetDirectivityData( );
	}
}

void CVABinauralOutdoorNoiseRenderer::UpdateSoundReceivers( CVASceneStateDiff* pDiff )
{
	typedef std::vector<int>::const_iterator icit_t;

	// Delete listeners
	for( icit_t it = pDiff->viDelReceiverIDs.begin( ); it != pDiff->viDelReceiverIDs.end( ); )
	{
		const int& iID( *it++ );
		DeleteSoundReceiver( iID );
	}

	// Add listeners
	for( icit_t it = pDiff->viNewReceiverIDs.begin( ); it != pDiff->viNewReceiverIDs.end( ); )
	{
		const int& iID( *it++ );
		CreateSoundReceiver( iID, m_pNewSceneState->GetReceiverState( iID ) );
	}

	// Update receiver trajectories and directivity
	for( std::map<int, CVABinauralClusteringReceiver*>::iterator it = m_mBinauralReceivers.begin( ); it != m_mBinauralReceivers.end( ); ++it )
	{
		int iReceiverID                          = it->first;
		CVABinauralClusteringReceiver* pReceiver = it->second;

		CVAReceiverState* pReceiverStateCur  = ( m_pCurSceneState ? m_pCurSceneState->GetReceiverState( iReceiverID ) : nullptr );
		CVAReceiverState* pReceiverStaterNew = ( m_pNewSceneState ? m_pNewSceneState->GetReceiverState( iReceiverID ) : nullptr );

		const CVAMotionState* pMotionStateCur = ( pReceiverStateCur ? pReceiverStateCur->GetMotionState( ) : nullptr );
		const CVAMotionState* pMotionStateNew = ( pReceiverStaterNew ? pReceiverStaterNew->GetMotionState( ) : nullptr );

		if( pMotionStateNew && ( pMotionStateNew != pMotionStateCur ) )
		{
			pReceiver->pMotionModel->InputMotionKey( pMotionStateNew );
		}

		if( pReceiverStaterNew != nullptr )
		{
			pReceiver->pDirectivity = (IVADirectivity*)pReceiverStaterNew->GetDirectivity( );
		}
	}
}

void CVABinauralOutdoorNoiseRenderer::CreateSoundSource( const int iSourceID, const CVASoundSourceState* pSourceState )
{
	auto pSource = new CVABinauralOutdoorSource( m_iMaxDelaySamples );

	// set state
	pSource->pState = (CVASoundSourceState*)pSourceState;
	// set internal data
	pSource->pData = m_pCore->GetSceneManager( )->GetSoundSourceDesc( iSourceID );
	pSource->pData->AddReference( );

	CVABasicMotionModel::Config oSourceMotionConf;
	oSourceMotionConf.SetDefaults( );
	pSource->pMotionModel = new CVASharedMotionModel( new CVABasicMotionModel( oSourceMotionConf ), true );
	// set motion model
	CVABasicMotionModel* pMotionInstance = dynamic_cast<CVABasicMotionModel*>( pSource->pMotionModel->GetInstance( ) ); // Problem HERE***
	pMotionInstance->SetName( std::string( "bfrend_mm_source_" + pSource->pData->sName ) );
	pMotionInstance->Reset( );

	// set directivity
	pSource->pDirectivity = (IVADirectivity*)pSourceState->GetDirectivityData( );

	// add local reference
	m_mSources.insert( std::pair<int, CVABinauralOutdoorSource*>( iSourceID, pSource ) );

	// create source-receiver pairs
	for( auto itReceiver = m_mBinauralReceivers.begin( ); itReceiver != m_mBinauralReceivers.end( ); itReceiver++ )
		m_lpSourceReceiverPairs.Add( pSource, itReceiver->second );
}

void CVABinauralOutdoorNoiseRenderer::DeleteSoundSource( int iSoundSourceID )
{
	std::map<int, CVABinauralOutdoorSource*>::iterator it = m_mSources.find( iSoundSourceID );
	auto pSource                                          = it->second;

	// remove from source-receiver pairs
	m_lpSourceReceiverPairs.remove_if( [&]( SourceReceiverPairPtr pSRPair ) { return !pSRPair || pSRPair->GetSource( ) == pSource; } );

	// remove local source reference
	m_mSources.erase( it );

	// remove from clustering
	m_pClusterEngine->RemoveWaveFrontsOfSource( iSoundSourceID );
}

void CVABinauralOutdoorNoiseRenderer::CreateSoundReceiver( int iSoundReceiverID, const CVAReceiverState* pState )
{
	CVABinauralClusteringReceiver* pReceiver = dynamic_cast<CVABinauralClusteringReceiver*>( m_pReceiverPool->RequestObject( ) ); // Reference = 1

	// set internal data
	pReceiver->pData = m_pCore->GetSceneManager( )->GetSoundReceiverDesc( iSoundReceiverID );
	pReceiver->pData->AddReference( );

	// set motion model
	CVABasicMotionModel* pMotionModel = dynamic_cast<CVABasicMotionModel*>( pReceiver->pMotionModel->GetInstance( ) ); // foo possible error Get Instance
	pMotionModel->SetName( std::string( "bonr_mm_receiver_" + pReceiver->pData->sName ) );
	pMotionModel->Reset( );

	// set directivity
	pReceiver->pDirectivity = (IVADirectivity*)pState->GetDirectivity( );

	// add local reference
	m_mBinauralReceivers.insert( std::pair<int, CVABinauralClusteringReceiver*>( iSoundReceiverID, pReceiver ) );

	// add listener to clustering
	CVABinauralClusteringEngine::Config oConfig = { m_iNumClusters };
	m_pClusterEngine->AddReceiver( iSoundReceiverID, pReceiver, oConfig, m_dAngularDistanceThreshold );

	// create source-receiver pairs
	for( auto itSource = m_mSources.begin( ); itSource != m_mSources.end( ); itSource++ )
		m_lpSourceReceiverPairs.Add( itSource->second, pReceiver );
}

void CVABinauralOutdoorNoiseRenderer::DeleteSoundReceiver( int iSoundReceiverID )
{
	// remove local listener reference
	std::map<int, CVABinauralClusteringReceiver*>::iterator it = m_mBinauralReceivers.find( iSoundReceiverID );
	CVABinauralClusteringReceiver* pReceiver                   = it->second;
	m_mBinauralReceivers.erase( it );

	// remove listener reference from clustering
	m_pClusterEngine->RemoveReceiver( iSoundReceiverID );

	// remove from source-receiver pairs
	m_lpSourceReceiverPairs.remove_if( [&]( SourceReceiverPairPtr pSRPair ) { return !pSRPair || pSRPair->GetReceiver( ) == pReceiver; } );

	// pReceiver->bDeleted = true;
	pReceiver->pData->RemoveReference( );
	pReceiver->RemoveReference( );
}

void CVABinauralOutdoorNoiseRenderer::UpdateTrajectories( double dTime )
{
	for( auto const& cit: m_mSources )
	{
		CVABinauralOutdoorSource* pSource = cit.second;
		pSource->pMotionModel->HandleMotionKeys( );
	}

	for( auto const& cit: m_mBinauralReceivers )
	{
		CVABinauralClusteringReceiver* pReceiver = cit.second;
		pReceiver->pMotionModel->HandleMotionKeys( );

		bool bValid = true;
		bValid &= pReceiver->pMotionModel->EstimatePosition( dTime, pReceiver->v3PredictedPosition );
		bValid &= pReceiver->pMotionModel->EstimateOrientation( dTime, pReceiver->v3PredictedView, pReceiver->v3PredictedUp );
		pReceiver->bHasValidTrajectory = bValid;
	}
}

void CVABinauralOutdoorNoiseRenderer::UpdateFilterPropertyHistories( double dTime )
{
	for( auto pSRPair: m_lpSourceReceiverPairs )
		pSRPair->UpdateFilterPropertyHistories( dTime );
}

void CVABinauralOutdoorNoiseRenderer::RequestSimulationUpdate( double dNow )
{
	for( const auto scit: m_mSources )
	{
		for( const auto rcit: m_mBinauralReceivers )
		{
			int iReceiverID = rcit.first;
			auto pReceiver  = rcit.second;

			int iSourceID = scit.first;
			auto pSource  = scit.second;

			VAVec3 vPos;
			VAQuat qOrient;

			if( !pSource->pMotionModel->EstimatePosition( dNow, vPos ) || !pSource->pMotionModel->EstimateOrientation( dNow, qOrient ) )
				continue;

			auto pSourceObject = std::make_unique<ITA::SimulationScheduler::C3DObject>( VistaVector3D( vPos.comp ), VistaQuaternion( qOrient.comp ),
			                                                                            ITA::SimulationScheduler::C3DObject::Type::source, iSourceID );

			if( !pReceiver->pMotionModel->EstimatePosition( dNow, vPos ) || !pReceiver->pMotionModel->EstimateOrientation( dNow, qOrient ) )
				continue;

			auto pReceiverObject = std::make_unique<ITA::SimulationScheduler::C3DObject>( VistaVector3D( vPos.comp ), VistaQuaternion( qOrient.comp ),
			                                                                              ITA::SimulationScheduler::C3DObject::Type::receiver, iReceiverID );

			auto pUpdate = std::make_unique<ITA::SimulationScheduler::CUpdateScene>( dNow );
			pUpdate->SetSourceReceiverPair( std::move( pSourceObject ), std::move( pReceiverObject ) );

			m_pSimulationScheduler->PushUpdate( std::move( pUpdate ) );
		}
	}
}

void CVABinauralOutdoorNoiseRenderer::PostResultReceived( std::unique_ptr<ITA::SimulationScheduler::CSimulationResult> pResult )
{
	auto pOutdoorSimResult = dynamic_cast<ITA::SimulationScheduler::OutdoorAcoustics::COutdoorSimulationResult*>( pResult.get( ) );
	if( !pOutdoorSimResult )
		return;

	const int iSourceID                = pOutdoorSimResult->sourceReceiverPair.source->GetId( );
	const int iReceiverID              = pOutdoorSimResult->sourceReceiverPair.receiver->GetId( );
	const VistaVector3D& v3ReceiverPos = pOutdoorSimResult->sourceReceiverPair.receiver->GetPosition( );
	const double& dResultRequestTime   = pOutdoorSimResult->dTimeStamp;

	const VistaQuaternion& qTmp  = pOutdoorSimResult->sourceReceiverPair.source->GetOrientation( );
	const VistaVector3D vViewTmp = qTmp.GetViewDir( ).GetNormalized( );
	const VistaVector3D vUpTmp   = qTmp.GetUpDir( ).GetNormalized( );
	const VAVec3 vSourceView     = VAVec3( vViewTmp[Vista::X], vViewTmp[Vista::Y], vViewTmp[Vista::Z] );
	const VAVec3 vSourceUp       = VAVec3( vUpTmp[Vista::X], vUpTmp[Vista::Y], vUpTmp[Vista::Z] );

	auto its = m_mSources.find( iSourceID );
	if( its == m_mSources.end( ) )
		ITA_EXCEPT_INVALID_PARAMETER( "Source ID returned from simulation scheduler could not be resolved." );
	auto pSource = its->second;

	auto itr = m_mBinauralReceivers.find( iReceiverID );
	if( itr == m_mBinauralReceivers.end( ) )
		ITA_EXCEPT_INVALID_PARAMETER( "Receiver ID returned from simulation scheduler could not be resolved." );
	auto pReceiver = itr->second;

	SourceReceiverPairPtr pSRPair = m_lpSourceReceiverPairs.Find( pSource, pReceiver );
	if( !pSRPair )
		ITA_EXCEPT_INVALID_PARAMETER( "Could not find source-receiver pair for given source and receiver." );

	auto mpUnhandledWavefronts = pSRPair->GetWavefrontMap( );
	for( const auto& oPath: pOutdoorSimResult->voPathProperties )
	{
		if( oPath.sID.empty( ) )
			VA_WARN( "BinauralOutdoorRenderer", "Received a result without identifier, can't assign path" );

		CVABinauralOutdoorWaveFront* pWavefrontOfPath = pSRPair->GetWavefront( oPath.sID );
		if( !pWavefrontOfPath )
		{
			pWavefrontOfPath = dynamic_cast<CVABinauralOutdoorWaveFront*>( m_pWaveFrontPool->RequestObject( ) ); // request a new path from the pool
			pWavefrontOfPath->SetSource( pSource );
			pWavefrontOfPath->SetReceiver( pReceiver );

			// Add wave front to current sound receiver pair (for later reference & update or release)
			pSRPair->AddWavefront( oPath.sID, pWavefrontOfPath );

			// Add new wave front to the cluster engine stage
			m_pClusterEngine->AddWaveFront( pWavefrontOfPath );
		}
		else
			mpUnhandledWavefronts.erase( oPath.sID );

		const VistaVector3D v3WFOrigin = v3ReceiverPos - oPath.v3ReceiverWaveFrontNormal; //@todo psc: Would be better if we could set WF normal directly
		const VAVec3 v3SourceWFNormal =
		    VAVec3( oPath.v3SourceWaveFrontNormal[Vista::X], oPath.v3SourceWaveFrontNormal[Vista::Y], oPath.v3SourceWaveFrontNormal[Vista::Z] );
		ITABase::CThirdOctaveGainMagnitudeSpectrum oTotalSpectrum = pSource->GetDirectivitySpectrum( vSourceView, vSourceUp, v3SourceWFNormal );
		oTotalSpectrum.Multiply( oPath.oAirAttenuationSpectrum ); //@todo psc: Also multiply with oPath.oGeoAttenuationSpectrum
		if( m_bFilterPropertyHistoriesEnabled )
		{
			const double dTimeAtReceiver = dResultRequestTime + oPath.dPropagationDelay;
			if( dTimeAtReceiver <= pWavefrontOfPath->GetLastHistoryTimestamp( ) )
			{
				VA_WARN( "BinauralOutdoorRenderer", "Skipping simulation result: Timestamp is not strictly monotonously increasing." );
				continue;
			}
			pWavefrontOfPath->PushDelay( dTimeAtReceiver, oPath.dPropagationDelay );
			pWavefrontOfPath->PushGain( dTimeAtReceiver, oPath.dSpreadingLoss );
			pWavefrontOfPath->PushFilterCoefficients( dTimeAtReceiver, std::make_unique<ITABase::CThirdOctaveGainMagnitudeSpectrum>( oTotalSpectrum ) );
			pWavefrontOfPath->PushWFOrigin( dTimeAtReceiver, std::make_unique<VAVec3>( v3WFOrigin[Vista::X], v3WFOrigin[Vista::Y], v3WFOrigin[Vista::Z] ) );
			pWavefrontOfPath->PushAudibility( dTimeAtReceiver, true );
		}
		else
		{
			const unsigned int iDelay = (int)round( oPath.dPropagationDelay * m_oDefaultWaveFrontConf.dSampleRate ); // unsigned as can not have negative delay
			                                                                                                         // (causality)
			pWavefrontOfPath->SetDelaySamples( iDelay );
			pWavefrontOfPath->SetGain( oPath.dSpreadingLoss );
			pWavefrontOfPath->SetFilterCoefficients( oTotalSpectrum );
			pWavefrontOfPath->SetWaveFrontOrigin( VAVec3( v3WFOrigin[Vista::X], v3WFOrigin[Vista::Y], v3WFOrigin[Vista::Z] ) );
			pWavefrontOfPath->SetAudible( true );
		}
	}

	for( auto itWF = mpUnhandledWavefronts.begin( ); itWF != mpUnhandledWavefronts.end( ); itWF++ )
	{
		auto pWavefront = itWF->second;
		pWavefront->SetAudible( false );
		//@todo psc: How can we know the right timestamp here if we don't know the propagation delay?
		// pWavefront->PushAudibility(false);

		//@todo psc: When do we have to remove the wavefronts (mark them for deletion)?
		// m_pClusterEngine->RemoveWaveFront(pWavefront);
		// pSRPair->RemoveWavefront(itWF->first);
	}

	m_pClusterEngine->Update( ); //@todo psc: Without this, we hear nothing? Is this expected or a bug?
}

#endif // VACORE_WITH_RENDERER_BINAURAL_OUTDOOR_NOISE
