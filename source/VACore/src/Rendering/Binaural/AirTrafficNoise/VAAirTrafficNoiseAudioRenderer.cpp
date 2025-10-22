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

#include "VAAirTrafficNoiseAudioRenderer.h"

#ifdef VACORE_WITH_RENDERER_BINAURAL_AIR_TRAFFIC_NOISE

#	include "VAATNSourceReceiverTransmission.h"
#	include "VAATNSourceReceiverTransmissionHomogeneous.h"
#	include "VAATNSourceReceiverTransmissionInhomogeneous.h"

// VA includes
#	include "../../../Filtering/VATemporalVariations.h"
#	include "../../../Motion/VAMotionModelBase.h"
#	include "../../../Motion/VASampleAndHoldMotionModel.h"
#	include "../../../Motion/VASharedMotionModel.h"
#	include "../../../Scene/VAScene.h"
#	include "../../../Utils/VAUtils.h"
#	include "../../../VAAudiostreamTracker.h"
#	include "../../../VACoreConfig.h"
#	include "../../../VALog.h"
#	include "../../../core/core.h"
#	include "../../../directivities/VADirectivityDAFFEnergetic.h"
#	include "../../../directivities/VADirectivityDAFFHRIR.h"

#	include <VA.h>
#	include <VAObjectPool.h>
#	include <VAReferenceableObject.h>

// ITA includes
#	include <DAFF.h>
#	include <ITAClock.h>
#	include <ITACriticalSection.h>
#	include <ITADataSourceRealization.h>
#	include <ITAFastMath.h>
#	include <ITAGeo/Utils/JSON/Atmosphere.h>
#	include <ITAISO9613.h>
#	include <ITASampleBuffer.h>
#	include <ITASampleFrame.h>
#	include <ITAStopWatch.h>
#	include <ITAStreamInfo.h>
#	include <ITAThirdOctaveFilterbank.h>
#	include <ITAUPConvolution.h>
#	include <ITAUPFilter.h>
#	include <ITAUPFilterPool.h>

// Vista includes
#	include <VistaInterProcComm/Concurrency/VistaThreadEvent.h>

// STL includes
#	include <atomic>
#	include <cassert>
#	include <fstream>
#	include <iomanip>

// 3rdParty includes
#	include <tbb/concurrent_queue.h>

// Helper for external simulation
void ExternalSimulationUpdate( CVABATNSourceReceiverTransmission::CPropagationPath&, const CVAStruct& );


CVABinauralAirTrafficNoiseAudioRenderer::CVABinauralAirTrafficNoiseAudioRenderer( const CVAAudioRendererInitParams& oParams )
    : ITADatasourceRealization( 2, oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.dSampleRate, oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.iBuffersize )
    , m_pCore( oParams.pCore )
    , m_pCurSceneState( nullptr )
    , m_iHRIRFilterLength( -1 )
    , m_oParams( oParams )
{
	Init( *oParams.pConfig );

	IVAPoolObjectFactory* pListenerFactory = new CVABATNSoundReceiverPoolFactory( m_pCore, m_oDefaultListenerConf );
	m_pListenerPool                        = IVAObjectPool::Create( 16, 2, pListenerFactory, true );

	IVAPoolObjectFactory* pSourceFactory = new CVABATNSourcePoolFactory( m_oDefaultSourceConf );
	m_pSourcePool                        = IVAObjectPool::Create( 16, 2, pSourceFactory, true );

	if( m_sMediumType.compare( "homogeneous" ) == 0 )
		m_pSourceReceiverTransmissionFactory = new CVABATNSourceReceiverTransmissionHomogeneousFactory( m_pCore->oHomogeneousMedium, GetSampleRate( ), GetBlocklength( ),
		                                                                                                m_iHRIRFilterLength, 128, m_iFilterBankType );
	else if( m_sMediumType.compare( "inhomogeneous" ) == 0 )
	{
		if( m_sStratifiedAtmosphereFilepath.compare( "" ) != 0 )
			SetStratifiedAtmosphere( m_sStratifiedAtmosphereFilepath ); // Otherwise default constructor is used

		m_pSourceReceiverTransmissionFactory = new CVABATNSourceReceiverTransmissionInhomogeneousFactory( m_oStratifiedAtmosphere, GetSampleRate( ), GetBlocklength( ),
		                                                                                                  m_iHRIRFilterLength, 128, m_iFilterBankType );
	}
	else
		ITA_EXCEPT_INVALID_PARAMETER( "Unknown parameter for MediumType" );

	m_pSourceReceiverTransmissionPool = IVAObjectPool::Create( 64, 8, m_pSourceReceiverTransmissionFactory, true );

	m_pUpdateMessagePool = IVAObjectPool::Create( 2, 1, new CVAPoolObjectDefaultFactory<CUpdateMessage>, true );

	ctxAudio.m_sbTempBufD.Init( GetBlocklength( ), true );
	ctxAudio.m_sbTempBufR.Init( GetBlocklength( ), true );
	ctxAudio.m_iResetFlag = 0; // Normal operation mode
	ctxAudio.m_iStatus    = 0; // Stopped

	m_iCurGlobalAuralizationMode = IVAInterface::VA_AURAMODE_DEFAULT;
}

CVABinauralAirTrafficNoiseAudioRenderer::~CVABinauralAirTrafficNoiseAudioRenderer( )
{
	delete m_pSourceReceiverTransmissionPool;
	delete m_pUpdateMessagePool;
}

void CVABinauralAirTrafficNoiseAudioRenderer::Init( const CVAStruct& oArgs )
{
	CVAConfigInterpreter conf( oArgs );

	conf.OptString( "MediumType", m_sMediumType, "homogeneous" );
	conf.OptString( "StratifiedAtmosphereFilepath", m_sStratifiedAtmosphereFilepath, "" );

	conf.OptNumber( "GroundPlanePosition", m_dGroundPlanePosition, 0.0f );

	conf.OptBool( "PropagationDelayExternalSimulation", m_bPropagationDelayExternalSimulation, false );
	conf.OptBool( "GroundReflectionExternalSimulation", m_bGroundReflectionExternalSimulation, false );
	conf.OptBool( "DirectivityExternalSimulation", m_bDirectivityExternalSimulation, false );
	conf.OptBool( "AirAbsorptionExternalSimulation", m_bAirAbsorptionExternalSimulation, false );
	conf.OptBool( "SpreadingLossExternalSimulation", m_bSpreadingLossExternalSimulation, false );
	conf.OptBool( "TemporalVariationsExternalSimulation", m_bTemporalVariationsExternalSimulation, false );

	std::string sFilterBankType;
	conf.OptString( "FilterBankType", sFilterBankType, "IIR" );
	if( sFilterBankType == "FIR" )
		m_iFilterBankType = CITAThirdOctaveFilterbank::FIR_SPLINE_LINEAR_PHASE;
	else if( sFilterBankType == "IIR" )
		m_iFilterBankType = CITAThirdOctaveFilterbank::IIR_BIQUADS_ORDER10;
	else
		ITA_EXCEPT1( INVALID_PARAMETER, "Unrecognized filter bank switch '" + sFilterBankType + "', use 'FIR' (spline linear phase) or 'IIR' (10th order biquads)" );

	std::string sVLDInterpolationAlgorithm;
	conf.OptString( "SwitchingAlgorithm", sVLDInterpolationAlgorithm, "linear" );
	sVLDInterpolationAlgorithm = toLowercase( sVLDInterpolationAlgorithm );

	if( sVLDInterpolationAlgorithm == "switch" )
		m_iDefaultVDLSwitchingAlgorithm = CITAVariableDelayLine::SWITCH;
	else if( sVLDInterpolationAlgorithm == "crossfade" )
		m_iDefaultVDLSwitchingAlgorithm = CITAVariableDelayLine::CROSSFADE;
	else if( sVLDInterpolationAlgorithm == "linear" )
		m_iDefaultVDLSwitchingAlgorithm = CITAVariableDelayLine::LINEAR_INTERPOLATION;
	else if( sVLDInterpolationAlgorithm == "cubicspline" )
		m_iDefaultVDLSwitchingAlgorithm = CITAVariableDelayLine::CUBIC_SPLINE_INTERPOLATION;
	else if( sVLDInterpolationAlgorithm == "windowedsinc" )
		m_iDefaultVDLSwitchingAlgorithm = CITAVariableDelayLine::WINDOWED_SINC_INTERPOLATION;
	else
		ITA_EXCEPT1( INVALID_PARAMETER, "Unrecognized interpolation algorithm '" + sVLDInterpolationAlgorithm + "' in BinauralFreefieldAudioRendererConfig" );

	conf.OptInteger( "FIRFilterLength", m_iHRIRFilterLength, 256 );

	// Motion model Listener
	conf.OptInteger( "MotionModelNumHistoryKeys", m_oDefaultListenerConf.iMotionModelNumHistoryKeys, 1000 );

	if( m_oDefaultListenerConf.iMotionModelNumHistoryKeys < 1 )
		VA_EXCEPT2( INVALID_PARAMETER, "Basic motion model history needs to be greater than zero" );

	conf.OptNumber( "MotionModelWindowSize", m_oDefaultListenerConf.dMotionModelWindowSize, 0.1f );
	conf.OptNumber( "MotionModelWindowDelay", m_oDefaultListenerConf.dMotionModelWindowDelay, 0.1f );

	if( ( m_oDefaultListenerConf.dMotionModelWindowSize <= 0 ) || ( m_oDefaultListenerConf.dMotionModelWindowDelay < 0 ) )
		VA_EXCEPT2( INVALID_PARAMETER, "Basic motion model window parameters parse error (zero or negative?)" );

	// Motion model Source
	conf.OptInteger( "MotionModelNumHistoryKeys", m_oDefaultSourceConf.iMotionModelNumHistoryKeys, 1000 );

	if( m_oDefaultSourceConf.iMotionModelNumHistoryKeys < 1 )
		VA_EXCEPT2( INVALID_PARAMETER, "Basic motion model history needs to be greater than zero" );

	conf.OptNumber( "MotionModelWindowSize", m_oDefaultSourceConf.dMotionModelWindowSize, 1.0f );
	conf.OptNumber( "MotionModelWindowDelay", m_oDefaultSourceConf.dMotionModelWindowDelay, 0.5f );

	return;
}

void CVABinauralAirTrafficNoiseAudioRenderer::Reset( )
{
	ctxAudio.m_iResetFlag = 1; // Request reset

	if( ctxAudio.m_iStatus == 0 || m_oParams.bOfflineRendering )
	{
		// if no streaming active, reset manually
		// SyncInternalData();
		ResetInternalData( );
	}

	// Wait for last streaming block before internal reset
	while( ctxAudio.m_iResetFlag != 2 )
	{
		VASleep( 10 ); // Wait for acknowledge
	}

	// Iterate over sound pathes and free items
	for( CVABATNSourceReceiverTransmission* pSRTransmission: m_lSourceReceiverTransmissions )
	{
		int iNumRefs = pSRTransmission->GetNumReferences( );
		assert( iNumRefs == 1 );
		pSRTransmission->RemoveReference( );
	}
	m_lSourceReceiverTransmissions.clear( );

	// Iterate over listener and free items
	for( auto citListenerIDPair = m_mListeners.cbegin( ); citListenerIDPair != m_mListeners.cend( ); ++citListenerIDPair )
	{
		CVABATNSoundReceiver* pListener( citListenerIDPair->second );
		pListener->pData->RemoveReference( );
		assert( pListener->GetNumReferences( ) == 1 );
		pListener->RemoveReference( );
	}
	m_mListeners.clear( );

	// Iterate over sources and free items
	for( auto citSourceIDPair = m_mSources.cbegin( ); citSourceIDPair != m_mSources.cend( ); ++citSourceIDPair )
	{
		CVABATNSoundSource* pSource( citSourceIDPair->second );
		pSource->pData->RemoveReference( );
		assert( pSource->GetNumReferences( ) == 1 );
		pSource->RemoveReference( );
	}
	m_mSources.clear( );

	// Scene frei geben
	if( m_pCurSceneState )
	{
		m_pCurSceneState->RemoveReference( );
		m_pCurSceneState = nullptr;
	}

	ctxAudio.m_iResetFlag = 0; // Enter normal mode
}

void CVABinauralAirTrafficNoiseAudioRenderer::UpdateScene( CVASceneState* pNewSceneState )
{
	assert( pNewSceneState );

	m_pNewSceneState = pNewSceneState;
	if( m_pNewSceneState == m_pCurSceneState )
		return;

	// Neue Szene referenzieren (gegen Freigabe sperren)
	m_pNewSceneState->AddReference( );

	// Unterschiede ermitteln: Neue Szene vs. alte Szene
	CVASceneStateDiff oDiff;
	pNewSceneState->Diff( m_pCurSceneState, &oDiff );

	// Leere Update-Nachricht zusammenstellen
	m_pUpdateMessage = dynamic_cast<CUpdateMessage*>( m_pUpdateMessagePool->RequestObject( ) );

	// Quellen, H�rer und Pfade verwalten
	ManageSoundReceiverTransmissions( m_pCurSceneState, pNewSceneState, &oDiff );

	// Bewegungsinformationen der Quellen und H�rer aktualisieren
	UpdateTrajectories( );

	// Entit�ten der Schallpfade aktualisieren
	UpdateDirectivitySets( );

	// Update-Nachricht an den Audiokontext schicken
	ctxAudio.m_qpUpdateMessages.push( m_pUpdateMessage );

	// Alte Szene freigeben (dereferenzieren)
	if( m_pCurSceneState )
		m_pCurSceneState->RemoveReference( );
	m_pCurSceneState = m_pNewSceneState;
	m_pNewSceneState = nullptr;
}

ITADatasource* CVABinauralAirTrafficNoiseAudioRenderer::GetOutputDatasource( )
{
	return this;
}

void CVABinauralAirTrafficNoiseAudioRenderer::ManageSoundReceiverTransmissions( const CVASceneState* pCurScene, const CVASceneState* pNewScene,
                                                                                const CVASceneStateDiff* pDiff )
{
	// Iterate over current paths and mark deleted (will be removed within internal sync of audio context thread)
	std::list<CVABATNSourceReceiverTransmission*>::iterator itp = m_lSourceReceiverTransmissions.begin( );
	while( itp != m_lSourceReceiverTransmissions.end( ) )
	{
		CVABATNSourceReceiverTransmission* pSRTransmission( *itp );
		int iSourceID            = pSRTransmission->pSoundSource->pData->iID;
		int iListenerID          = pSRTransmission->pSoundReceiver->pData->iID;
		bool bDeletetionRequired = false;

		// Source deleted?
		for( const int& iIDDeletedSource: pDiff->viDelSoundSourceIDs )
		{
			// Remove only required, if not skipped by this renderer
			bool bSkippedByThisRenderer                = false;
			const CVASoundSourceDesc* pSoundSourceDesc = m_pCore->GetSceneManager( )->GetSoundSourceDesc( iIDDeletedSource );
			if( !pSoundSourceDesc->sExplicitRendererID.empty( ) && pSoundSourceDesc->sExplicitRendererID != m_oParams.sID )
				bSkippedByThisRenderer = true;

			if( iSourceID == iIDDeletedSource && !bSkippedByThisRenderer )
			{
				bDeletetionRequired = true; // Source removed, deletion required
				break;
			}
		}

		if( bDeletetionRequired == false )
		{
			// Receiver deleted?
			for( const int& iIDListenerDeleted: pDiff->viDelReceiverIDs )
			{
				if( iListenerID == iIDListenerDeleted )
				{
					bDeletetionRequired = true; // Listener removed, deletion required
					break;
				}
			}
		}

		if( bDeletetionRequired )
		{
			DeleteSourceReceiverTransmission( pSRTransmission );
			itp = m_lSourceReceiverTransmissions.erase( itp ); // Increment via erase on path list
		}
		else
			++itp; // no deletion detected, continue
	}

	// Deleted sources
	for( const int& iID: pDiff->viDelSoundSourceIDs )
	{
		// Remove only required, if not skipped by this renderer
		const CVASoundSourceDesc* pSoundSourceDesc = m_pCore->GetSceneManager( )->GetSoundSourceDesc( iID );
		if( pSoundSourceDesc->sExplicitRendererID.empty( ) || pSoundSourceDesc->sExplicitRendererID == m_oParams.sID )
			DeleteSource( iID );
	}
	// Deleted receivers
	for( const int& iID: pDiff->viDelReceiverIDs )
		DeleteListener( iID );

	// New sources
	for( const int& iID: pDiff->viNewSoundSourceIDs )
	{
		const CVASoundSourceDesc* pSoundSourceDesc = m_pCore->GetSceneManager( )->GetSoundSourceDesc( iID );
		if( pSoundSourceDesc->sExplicitRendererID.empty( ) || pSoundSourceDesc->sExplicitRendererID == m_oParams.sID )
			CVABATNSoundSource* pSource = CreateSoundSource( iID, pNewScene->GetSoundSourceState( iID ) );
	}
	// New receivers
	for( const int& iID: pDiff->viNewReceiverIDs )
		CVABATNSoundReceiver* pListener = CreateSoundReceiver( iID, pNewScene->GetReceiverState( iID ) );


	// New paths: (1) new receivers, current sources
	for( const int& iListenerID: pDiff->viNewReceiverIDs )
	{
		CVABATNSoundReceiver* pListener = m_mListeners[iListenerID];

		for( const int& iSourceID: pDiff->viComSoundSourceIDs )
		{
			CVABATNSoundSource* pSource = m_mSources[iSourceID];

			bool bForThisRenderer                      = false;
			const CVASoundSourceDesc* pSoundSourceDesc = m_pCore->GetSceneManager( )->GetSoundSourceDesc( iSourceID );
			if( pSoundSourceDesc->sExplicitRendererID.empty( ) || pSoundSourceDesc->sExplicitRendererID == m_oParams.sID )
				bForThisRenderer = true;

			if( !pSource->bDeleted && bForThisRenderer ) // only, if not marked for deletion and not skipped by renderer
				CreateSoundPath( pSource, pListener );
		}
	}

	// New paths: (2) new sources, current receivers
	for( const int& iSourceID: pDiff->viNewSoundSourceIDs )
	{
		CVABATNSoundSource* pSource = m_mSources[iSourceID];

		bool bForThisRenderer                      = false;
		const CVASoundSourceDesc* pSoundSourceDesc = m_pCore->GetSceneManager( )->GetSoundSourceDesc( iSourceID );
		if( pSoundSourceDesc->sExplicitRendererID.empty( ) || pSoundSourceDesc->sExplicitRendererID == m_oParams.sID )
			bForThisRenderer = true;

		for( const int& iListenerID: pDiff->viComReceiverIDs )
		{
			CVABATNSoundReceiver* pListener = m_mListeners[iListenerID];

			if( !pListener->bDeleted && bForThisRenderer )
				CreateSoundPath( pSource, pListener );
		}
	}

	// New paths: (3) new sources, new receivers
	for( const int& iSourceID: pDiff->viNewSoundSourceIDs )
	{
		CVABATNSoundSource* pSource = m_mSources[iSourceID];

		const CVASoundSourceDesc* pSoundSourceDesc = m_pCore->GetSceneManager( )->GetSoundSourceDesc( iSourceID );
		if( !pSoundSourceDesc->sExplicitRendererID.empty( ) && pSoundSourceDesc->sExplicitRendererID != m_oParams.sID )
			continue;

		for( const int& iListenerID: pDiff->viNewReceiverIDs )
		{
			CVABATNSoundReceiver* pListener = m_mListeners[iListenerID];
			CreateSoundPath( pSource, pListener );
		}
	}

	return;
}

void CVABinauralAirTrafficNoiseAudioRenderer::ProcessStream( const ITAStreamInfo* pStreamInfo )
{
	// If streaming is active, set to 1
	ctxAudio.m_iStatus = 1;

	// Schallpfade abgleichen
	SyncInternalData( );

	float* pfOutputChL = GetWritePointer( 0 );
	float* pfOutputChR = GetWritePointer( 1 );

	fm_zero( pfOutputChL, GetBlocklength( ) );
	fm_zero( pfOutputChR, GetBlocklength( ) );

	const CVAAudiostreamState* pStreamState = dynamic_cast<const CVAAudiostreamState*>( pStreamInfo );
	double dListenerTime                    = pStreamState->dSysTime;

	// Check for reset request
	if( ctxAudio.m_iResetFlag == 1 )
	{
		ResetInternalData( );

		return;
	}
	else if( ctxAudio.m_iResetFlag == 2 )
	{
		// Reset active, skip until finished
		return;
	}

	SampleTrajectoriesInternal( dListenerTime );

	// Set all listener output sample frames to zero
	for( auto itListenerIDPair = m_mListeners.begin( ); itListenerIDPair != m_mListeners.end( ); itListenerIDPair++ )
		itListenerIDPair->second->psfOutput->zero( );

	// Update sound pathes
	for( CVABATNSourceReceiverTransmission* pSRTransmission: ctxAudio.m_lSourceReceiverTransmissions )
	{
		if( pSRTransmission == nullptr )
			continue;

		CVAReceiverState* pListenerState  = ( m_pCurSceneState ? m_pCurSceneState->GetReceiverState( pSRTransmission->pSoundReceiver->pData->iID ) : NULL );
		CVASoundSourceState* pSourceState = ( m_pCurSceneState ? m_pCurSceneState->GetSoundSourceState( pSRTransmission->pSoundSource->pData->iID ) : NULL );

		if( pListenerState == nullptr || pSourceState == nullptr )
			continue; // Skip if no data is present

		if( pSRTransmission->pSoundReceiver->bValidTrajectoryPresent == false || pSRTransmission->pSoundSource->bValidTrajectoryPresent == false )
			continue; // Skip if no valid trajectory data is present

		// --= Parameter update =--

		if( !( m_bAirAbsorptionExternalSimulation && m_bPropagationDelayExternalSimulation && m_bSpreadingLossExternalSimulation && m_bDirectivityExternalSimulation ) )
			pSRTransmission->UpdateSoundPaths( );

		bool bMASourceStatusEnabled   = ( pSourceState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_MEDIUM_ABSORPTION ) > 0;
		bool bMAListenerStatusEnabled = ( pListenerState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_MEDIUM_ABSORPTION ) > 0;
		bool bMAGlobalStatusEnabled   = ( m_iCurGlobalAuralizationMode & IVAInterface::VA_AURAMODE_MEDIUM_ABSORPTION ) > 0;
		bool bAirAttenuationEnabled   = bMASourceStatusEnabled && bMAListenerStatusEnabled && bMAGlobalStatusEnabled;

		if( !m_bAirAbsorptionExternalSimulation )
			pSRTransmission->UpdateAirAttenuation( );

		// Temporal variations
		bool bTVSourceStatusEnabled     = ( pSourceState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_TEMP_VAR ) > 0;
		bool bTVListenerStatusEnabled   = ( pListenerState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_TEMP_VAR ) > 0;
		bool bTVGlobalStatusEnabled     = ( m_iCurGlobalAuralizationMode & IVAInterface::VA_AURAMODE_TEMP_VAR ) > 0;
		bool bTemporalVariationsEnabled = bTVSourceStatusEnabled && bTVListenerStatusEnabled && bTVGlobalStatusEnabled;

		if( !m_bTemporalVariationsExternalSimulation )
			pSRTransmission->UpdateTemporalVariation( );

		bool bDPEnabledGlobal      = ( m_iCurGlobalAuralizationMode & IVAInterface::VA_AURAMODE_DOPPLER ) > 0;
		bool bDPEnabledListener    = ( pListenerState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_DOPPLER ) > 0;
		bool bDPEnabledSource      = ( pSourceState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_DOPPLER ) > 0;
		bool bDPEnabledCurrent     = ( pSRTransmission->oDirectSoundPath.pVariableDelayLine->GetAlgorithm( ) != CITAVariableDelayLine::SWITCH ); // switch = disabled
		bool bDopplerStatusChanged = ( bDPEnabledCurrent != ( bDPEnabledGlobal && bDPEnabledListener && bDPEnabledSource ) );
		if( bDopplerStatusChanged )
		{
			pSRTransmission->oDirectSoundPath.pVariableDelayLine->SetAlgorithm( !bDPEnabledCurrent ? m_iDefaultVDLSwitchingAlgorithm : CITAVariableDelayLine::SWITCH );
			pSRTransmission->oReflectedSoundPath.pVariableDelayLine->SetAlgorithm( !bDPEnabledCurrent ? m_iDefaultVDLSwitchingAlgorithm : CITAVariableDelayLine::SWITCH );
		}

		if( !m_bPropagationDelayExternalSimulation )
			pSRTransmission->UpdateMediumPropagation( );

		// DSP: set VDL delay time
		pSRTransmission->oDirectSoundPath.pVariableDelayLine->SetDelayTime( pSRTransmission->oDirectSoundPath.dPropagationTime );
		pSRTransmission->oReflectedSoundPath.pVariableDelayLine->SetDelayTime( pSRTransmission->oReflectedSoundPath.dPropagationTime );

		// Spherical spreading loss
		bool bSSLEnabledSource     = ( pSourceState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_SPREADING_LOSS ) > 0;
		bool bSSLEnabledListener   = ( pListenerState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_SPREADING_LOSS ) > 0;
		bool bSSLEnabledGlobal     = ( m_iCurGlobalAuralizationMode & IVAInterface::VA_AURAMODE_SPREADING_LOSS ) > 0;
		bool bSpreadingLossEnabled = ( bSSLEnabledSource && bSSLEnabledListener && bSSLEnabledGlobal );

		if( !m_bSpreadingLossExternalSimulation )
			pSRTransmission->UpdateSpreadingLoss( );

		if( !bSpreadingLossEnabled ) // Override in case spreading loss is disabled
		{
			pSRTransmission->oDirectSoundPath.dGeometricalSpreadingLoss = 1.0f;
			pSRTransmission->oDirectSoundPath.dGeometricalSpreadingLoss = 1.0f;
		}

		bool bDIREnabledSoure    = ( pSourceState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_SOURCE_DIRECTIVITY ) > 0;
		bool bDIREnabledListener = ( pListenerState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_SOURCE_DIRECTIVITY ) > 0;
		bool bDIREnabledGlobal   = ( m_iCurGlobalAuralizationMode & IVAInterface::VA_AURAMODE_SOURCE_DIRECTIVITY ) > 0;
		bool bDirectivityEnabled = ( bDIREnabledSoure && bDIREnabledListener && bDIREnabledGlobal );

		if( !m_bDirectivityExternalSimulation )
			pSRTransmission->UpdateSoundSourceDirectivity( );

		pSRTransmission->UpdateSoundReceiverDirectivity( );

		// Sound source gain / direct sound audibility via AuraMode flags
		bool bDSSourceStatusEnabled   = ( pSourceState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_DIRECT_SOUND ) > 0;
		bool bDSListenerStatusEnabled = ( pListenerState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_DIRECT_SOUND ) > 0;
		bool bDSGlobalStatusEnabled   = ( m_iCurGlobalAuralizationMode & IVAInterface::VA_AURAMODE_DIRECT_SOUND ) > 0;
		bool bDirectSoundEnabled      = bDSSourceStatusEnabled && bDSListenerStatusEnabled && bDSGlobalStatusEnabled;

		// Reflection (early reflections) via AuraMode flags
		bool bERSourceStatusEnabled   = ( pSourceState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_EARLY_REFLECTIONS ) > 0;
		bool bERListenerStatusEnabled = ( pListenerState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_EARLY_REFLECTIONS ) > 0;
		bool bERGlobalStatusEnabled   = ( m_iCurGlobalAuralizationMode & IVAInterface::VA_AURAMODE_EARLY_REFLECTIONS ) > 0;
		bool bReflectionEnabled       = bERSourceStatusEnabled && bERListenerStatusEnabled && bERGlobalStatusEnabled;

		float fDirectSoundPathOverallGain =
		    float( pSRTransmission->oDirectSoundPath.dGeometricalSpreadingLoss * pSourceState->GetVolume( m_pCore->GetCoreConfig( )->dDefaultAmplitudeCalibration ) );
		float fReflectedSoundPathOverallGain =
		    float( pSRTransmission->oReflectedSoundPath.dGeometricalSpreadingLoss * pSourceState->GetVolume( m_pCore->GetCoreConfig( )->dDefaultAmplitudeCalibration ) );
		if( pSRTransmission->pSoundSource->pData->bMuted )
		{
			fDirectSoundPathOverallGain    = 0.0f;
			fReflectedSoundPathOverallGain = 0.0f;
		}
		else
		{
			if( bDirectSoundEnabled == false )
				fDirectSoundPathOverallGain = 0.0f;
			if( bReflectionEnabled == false )
				fReflectedSoundPathOverallGain = 0.0f;
		}

		// DSP: set overall gain
		pSRTransmission->oDirectSoundPath.pFIRConvolverChL->SetGain( fDirectSoundPathOverallGain );
		pSRTransmission->oReflectedSoundPath.pFIRConvolverChL->SetGain( fReflectedSoundPathOverallGain );
		pSRTransmission->oDirectSoundPath.pFIRConvolverChR->SetGain( fDirectSoundPathOverallGain );
		pSRTransmission->oReflectedSoundPath.pFIRConvolverChR->SetGain( fReflectedSoundPathOverallGain );

		// Assemble and set third octave gains
		pSRTransmission->oDirectSoundPath.oThirdOctaveFilterMagnitudes.SetIdentity( );
		pSRTransmission->oReflectedSoundPath.oThirdOctaveFilterMagnitudes.SetIdentity( );
		if( bAirAttenuationEnabled )
		{
			pSRTransmission->oDirectSoundPath.oThirdOctaveFilterMagnitudes.Multiply( pSRTransmission->oDirectSoundPath.oAirAttenuationMagnitudes );
			pSRTransmission->oReflectedSoundPath.oThirdOctaveFilterMagnitudes.Multiply( pSRTransmission->oReflectedSoundPath.oAirAttenuationMagnitudes );
		}
		if( bDirectivityEnabled )
		{
			pSRTransmission->oDirectSoundPath.oThirdOctaveFilterMagnitudes.Multiply( pSRTransmission->oDirectSoundPath.oSoundSourceDirectivityMagnitudes );
			pSRTransmission->oReflectedSoundPath.oThirdOctaveFilterMagnitudes.Multiply( pSRTransmission->oReflectedSoundPath.oSoundSourceDirectivityMagnitudes );
		}
		if( bTemporalVariationsEnabled )
		{
			pSRTransmission->oDirectSoundPath.oThirdOctaveFilterMagnitudes.Multiply( pSRTransmission->oDirectSoundPath.oTemporalVariationMagnitudes );
			pSRTransmission->oReflectedSoundPath.oThirdOctaveFilterMagnitudes.Multiply( pSRTransmission->oReflectedSoundPath.oTemporalVariationMagnitudes );
		}

		if( bReflectionEnabled )
		{
			pSRTransmission->oReflectedSoundPath.oThirdOctaveFilterMagnitudes.Multiply( pSRTransmission->oReflectedSoundPath.oGroundReflectionMagnitudes );
		}

		// DSP: set overall magnitudes
		pSRTransmission->oDirectSoundPath.pThirdOctaveFilterBank->SetMagnitudes( pSRTransmission->oDirectSoundPath.oThirdOctaveFilterMagnitudes );
		pSRTransmission->oReflectedSoundPath.pThirdOctaveFilterBank->SetMagnitudes( pSRTransmission->oReflectedSoundPath.oThirdOctaveFilterMagnitudes );


		// Digital signal processing

		CVASoundSourceDesc* pSourceData = pSRTransmission->pSoundSource->pData;
		const ITASampleFrame& sfInputBuffer( *pSourceData->psfSignalSourceInputBuf );
		const ITASampleBuffer* psbInput( &( sfInputBuffer[0] ) );

		pSRTransmission->oDirectSoundPath.pVariableDelayLine->Process( psbInput, &( ctxAudio.m_sbTempBufD ) );
		pSRTransmission->oReflectedSoundPath.pVariableDelayLine->Process( psbInput, &( ctxAudio.m_sbTempBufR ) );

		pSRTransmission->oDirectSoundPath.pThirdOctaveFilterBank->Process( ctxAudio.m_sbTempBufD.data( ), ctxAudio.m_sbTempBufD.data( ) );    // inplace
		pSRTransmission->oReflectedSoundPath.pThirdOctaveFilterBank->Process( ctxAudio.m_sbTempBufR.data( ), ctxAudio.m_sbTempBufR.data( ) ); // inplace


		pSRTransmission->oDirectSoundPath.pFIRConvolverChL->Process( ctxAudio.m_sbTempBufD.data( ), ( *pSRTransmission->pSoundReceiver->psfOutput )[0].data( ),
		                                                             ITABase::MixingMethod::ADD );
		if( bReflectionEnabled )
			pSRTransmission->oReflectedSoundPath.pFIRConvolverChL->Process( ctxAudio.m_sbTempBufR.data( ), ( *pSRTransmission->pSoundReceiver->psfOutput )[0].data( ),
			                                                                ITABase::MixingMethod::ADD );

		pSRTransmission->oDirectSoundPath.pFIRConvolverChR->Process( ctxAudio.m_sbTempBufD.data( ), ( *pSRTransmission->pSoundReceiver->psfOutput )[1].data( ),
		                                                             ITABase::MixingMethod::ADD );
		if( bReflectionEnabled )
			pSRTransmission->oReflectedSoundPath.pFIRConvolverChR->Process( ctxAudio.m_sbTempBufR.data( ), ( *pSRTransmission->pSoundReceiver->psfOutput )[1].data( ),
			                                                                ITABase::MixingMethod::ADD );
	}

	// TODO: Select active listener
	for( auto it: m_mListeners )
	{
		CVABATNSoundReceiver* pReceiver = it.second;
		if( !pReceiver->pData->bMuted )
		{
			fm_add( pfOutputChL, ( *pReceiver->psfOutput )[0].data( ), m_uiBlocklength );
			fm_add( pfOutputChR, ( *pReceiver->psfOutput )[1].data( ), m_uiBlocklength );
		}
	}

	IncrementWritePointer( );
}

void CVABinauralAirTrafficNoiseAudioRenderer::UpdateTrajectories( )
{
	// Neue Quellendaten �bernehmen
	for( std::map<int, CVABATNSoundSource*>::iterator it = m_mSources.begin( ); it != m_mSources.end( ); ++it )
	{
		int iSourceID               = it->first;
		CVABATNSoundSource* pSource = it->second;

		CVASoundSourceState* pSourceCur = ( m_pCurSceneState ? m_pCurSceneState->GetSoundSourceState( iSourceID ) : nullptr );
		CVASoundSourceState* pSourceNew = ( m_pNewSceneState ? m_pNewSceneState->GetSoundSourceState( iSourceID ) : nullptr );

		const CVAMotionState* pMotionCur = ( pSourceCur ? pSourceCur->GetMotionState( ) : nullptr );
		const CVAMotionState* pMotionNew = ( pSourceNew ? pSourceNew->GetMotionState( ) : nullptr );

		if( pMotionNew && ( pMotionNew != pMotionCur ) )
		{
			VA_TRACE( "BinauralAirTrafficNoiseAudioRenderer", "Source " << iSourceID << " new motion state" );
			pSource->pMotionModel->InputMotionKey( pMotionNew );
		}
	}

	// Neue H�rerdaten �bernehmen
	for( std::map<int, CVABATNSoundReceiver*>::iterator it = m_mListeners.begin( ); it != m_mListeners.end( ); ++it )
	{
		int iListenerID                 = it->first;
		CVABATNSoundReceiver* pListener = it->second;

		CVAReceiverState* pListenerCur = ( m_pCurSceneState ? m_pCurSceneState->GetReceiverState( iListenerID ) : nullptr );
		CVAReceiverState* pListenerNew = ( m_pNewSceneState ? m_pNewSceneState->GetReceiverState( iListenerID ) : nullptr );

		const CVAMotionState* pMotionCur = ( pListenerCur ? pListenerCur->GetMotionState( ) : nullptr );
		const CVAMotionState* pMotionNew = ( pListenerNew ? pListenerNew->GetMotionState( ) : nullptr );

		if( pMotionNew && ( pMotionNew != pMotionCur ) )
		{
			VA_TRACE( "BinauralAirTrafficNoiseAudioRenderer", "Listener " << iListenerID << " new position " ); // << *pMotionNew);
			pListener->pMotionModel->InputMotionKey( pMotionNew );
		}
	}
}

void CVABinauralAirTrafficNoiseAudioRenderer::SampleTrajectoriesInternal( double dTime )
{
	bool bEstimationPossible = true;
	for( std::list<CVABATNSoundSource*>::iterator it = ctxAudio.m_lSources.begin( ); it != ctxAudio.m_lSources.end( ); ++it )
	{
		CVABATNSoundSource* pSource = *it;

		pSource->pMotionModel->HandleMotionKeys( );
		bEstimationPossible &= pSource->pMotionModel->EstimatePosition( dTime, pSource->vPredPos );
		bEstimationPossible &= pSource->pMotionModel->EstimateOrientation( dTime, pSource->vPredView, pSource->vPredUp );
		pSource->bValidTrajectoryPresent = bEstimationPossible;
	}

	bEstimationPossible = true;
	for( std::list<CVABATNSoundReceiver*>::iterator it = ctxAudio.m_lListener.begin( ); it != ctxAudio.m_lListener.end( ); ++it )
	{
		CVABATNSoundReceiver* pListener = *it;

		pListener->pMotionModel->HandleMotionKeys( );
		bEstimationPossible &= pListener->pMotionModel->EstimatePosition( dTime, pListener->vPredPos );
		bEstimationPossible &= pListener->pMotionModel->EstimateOrientation( dTime, pListener->vPredView, pListener->vPredUp );
		pListener->bValidTrajectoryPresent = bEstimationPossible;
	}
}

CVABATNSourceReceiverTransmission* CVABinauralAirTrafficNoiseAudioRenderer::CreateSoundPath( CVABATNSoundSource* pSource, CVABATNSoundReceiver* pListener )
{
	int iSourceID   = pSource->pData->iID;
	int iListenerID = pListener->pData->iID;

	assert( !pSource->bDeleted && !pListener->bDeleted );

	VA_VERBOSE( "BinauralAirTrafficNoiseAudioRenderer", "Creating sound path from source " << iSourceID << " -> listener " << iListenerID );

	CVABATNSourceReceiverTransmission* pSRTransmission = dynamic_cast<CVABATNSourceReceiverTransmission*>( m_pSourceReceiverTransmissionPool->RequestObject( ) );

	pSRTransmission->pSoundSource   = pSource;
	pSRTransmission->pSoundReceiver = pListener;

	pSRTransmission->bDelete = false;

	CVASoundSourceState* pSourceNew = ( m_pNewSceneState ? m_pNewSceneState->GetSoundSourceState( iSourceID ) : nullptr );
	if( pSourceNew != nullptr )
	{
		pSRTransmission->oDirectSoundPath.oDirectivityStateNew.pData    = (IVADirectivity*)pSourceNew->GetDirectivityData( );
		pSRTransmission->oReflectedSoundPath.oDirectivityStateNew.pData = (IVADirectivity*)pSourceNew->GetDirectivityData( );
	}

	CVAReceiverState* pListenerNew = ( m_pNewSceneState ? m_pNewSceneState->GetReceiverState( iListenerID ) : nullptr );
	if( pListenerNew != nullptr )
	{
		pSRTransmission->oDirectSoundPath.oSoundReceiverDirectivityStateNew.pData    = (IVADirectivity*)pListenerNew->GetDirectivity( );
		pSRTransmission->oReflectedSoundPath.oSoundReceiverDirectivityStateNew.pData = (IVADirectivity*)pListenerNew->GetDirectivity( );
	}

	m_lSourceReceiverTransmissions.push_back( pSRTransmission );
	m_pUpdateMessage->vNewPaths.push_back( pSRTransmission );

	return pSRTransmission;
}

void CVABinauralAirTrafficNoiseAudioRenderer::DeleteSourceReceiverTransmission( CVABATNSourceReceiverTransmission* pSRTransmission )
{
	VA_VERBOSE( "BinauralAirTrafficNoiseAudioRenderer", "Marking sound path from source " << pSRTransmission->pSoundSource->pData->iID << " -> listener "
	                                                                                      << pSRTransmission->pSoundReceiver->pData->iID << " for deletion" );

	pSRTransmission->bDelete = true;
	pSRTransmission->RemoveReference( );
	m_pUpdateMessage->vDelPaths.push_back( pSRTransmission );
}

CVABATNSoundReceiver* CVABinauralAirTrafficNoiseAudioRenderer::CreateSoundReceiver( int iID, const CVAReceiverState* pListenerState )
{
	VA_VERBOSE( "BinauralAirTrafficNoiseAudioRenderer", "Creating listener with ID " << iID );

	CVABATNSoundReceiver* pListener = dynamic_cast<CVABATNSoundReceiver*>( m_pListenerPool->RequestObject( ) );

	pListener->pData = m_pCore->GetSceneManager( )->GetSoundReceiverDesc( iID );
	pListener->pData->AddReference( );

	// Move to prerequest of pool?
	pListener->psfOutput = new ITASampleFrame( 2, m_pCore->GetCoreConfig( )->oAudioDriverConfig.iBuffersize, true );
	assert( pListener->pData );
	pListener->bDeleted = false;

	// Motion model
	CVABasicMotionModel* pMotionInstance = dynamic_cast<CVABasicMotionModel*>( pListener->pMotionModel->GetInstance( ) );
	pMotionInstance->SetName( std::string( "bfrend_mm_listener_" + pListener->pData->sName ) );
	pMotionInstance->Reset( );

	m_mListeners.insert( std::pair<int, CVABATNSoundReceiver*>( iID, pListener ) );

	m_pUpdateMessage->vNewListeners.push_back( pListener );

	return pListener;
}

void CVABinauralAirTrafficNoiseAudioRenderer::DeleteListener( int iListenerID )
{
	VA_VERBOSE( "BinauralAirTrafficNoiseAudioRenderer", "Marking listener with ID " << iListenerID << " for removal" );
	std::map<int, CVABATNSoundReceiver*>::iterator it = m_mListeners.find( iListenerID );
	CVABATNSoundReceiver* pListener                   = it->second;
	m_mListeners.erase( it );
	pListener->bDeleted = true;
	pListener->pData->RemoveReference( );
	pListener->RemoveReference( );

	m_pUpdateMessage->vDelListeners.push_back( pListener );

	return;
}

CVABATNSoundSource* CVABinauralAirTrafficNoiseAudioRenderer::CreateSoundSource( int iID, const CVASoundSourceState* pSourceState )
{
	VA_VERBOSE( "BinauralAirTrafficNoiseAudioRenderer", "Creating source with ID " << iID );
	CVABATNSoundSource* pSource = dynamic_cast<CVABATNSoundSource*>( m_pSourcePool->RequestObject( ) );

	pSource->pData = m_pCore->GetSceneManager( )->GetSoundSourceDesc( iID );
	pSource->pData->AddReference( );

	pSource->bDeleted = false;

	CVABasicMotionModel* pMotionInstance = dynamic_cast<CVABasicMotionModel*>( pSource->pMotionModel->GetInstance( ) );
	pMotionInstance->SetName( std::string( "bfrend_mm_source_" + pSource->pData->sName ) );
	pMotionInstance->Reset( );

	m_mSources.insert( std::pair<int, CVABATNSoundSource*>( iID, pSource ) );

	m_pUpdateMessage->vNewSources.push_back( pSource );

	return pSource;
}

void CVABinauralAirTrafficNoiseAudioRenderer::DeleteSource( int iSourceID )
{
	VA_VERBOSE( "BinauralAirTrafficNoiseAudioRenderer", "Marking source with ID " << iSourceID << " for removal" );
	std::map<int, CVABATNSoundSource*>::iterator it = m_mSources.find( iSourceID );
	CVABATNSoundSource* pSource                     = it->second;
	m_mSources.erase( it );
	pSource->bDeleted = true;
	pSource->pData->RemoveReference( );
	pSource->RemoveReference( );

	m_pUpdateMessage->vDelSources.push_back( pSource );

	return;
}

void CVABinauralAirTrafficNoiseAudioRenderer::SyncInternalData( )
{
	CUpdateMessage* pUpdate;
	while( ctxAudio.m_qpUpdateMessages.try_pop( pUpdate ) )
	{
		for( CVABATNSourceReceiverTransmission* pSRTransmission: pUpdate->vDelPaths )
		{
			ctxAudio.m_lSourceReceiverTransmissions.remove( pSRTransmission );
			pSRTransmission->RemoveReference( );
		}

		for( CVABATNSourceReceiverTransmission* pSRTransmission: pUpdate->vNewPaths )
		{
			pSRTransmission->AddReference( );
			ctxAudio.m_lSourceReceiverTransmissions.push_back( pSRTransmission );
		}

		for( CVABATNSoundSource* pSource: pUpdate->vDelSources )
		{
			ctxAudio.m_lSources.remove( pSource );
			pSource->pData->RemoveReference( );
			pSource->RemoveReference( );
		}

		for( CVABATNSoundSource* pSource: pUpdate->vNewSources )
		{
			pSource->AddReference( );
			pSource->pData->AddReference( );
			ctxAudio.m_lSources.push_back( pSource );
		}

		for( CVABATNSoundReceiver* pListener: pUpdate->vDelListeners )
		{
			ctxAudio.m_lListener.remove( pListener );
			pListener->pData->RemoveReference( );
			pListener->RemoveReference( );
		}

		for( CVABATNSoundReceiver* pListener: pUpdate->vNewListeners )
		{
			pListener->AddReference( );
			pListener->pData->AddReference( );
			ctxAudio.m_lListener.push_back( pListener );
		}

		pUpdate->RemoveReference( );
	}

	return;
}

void CVABinauralAirTrafficNoiseAudioRenderer::ResetInternalData( )
{
	for( CVABATNSourceReceiverTransmission* pSRTransmission: ctxAudio.m_lSourceReceiverTransmissions )
		pSRTransmission->RemoveReference( );
	ctxAudio.m_lSourceReceiverTransmissions.clear( );


	for( CVABATNSoundReceiver* pListener: ctxAudio.m_lListener )
	{
		pListener->pData->RemoveReference( );
		pListener->RemoveReference( );
	}
	ctxAudio.m_lListener.clear( );


	for( CVABATNSoundSource* pSource: ctxAudio.m_lSources )
	{
		pSource->pData->RemoveReference( );
		pSource->RemoveReference( );
	}
	ctxAudio.m_lSources.clear( );

	ctxAudio.m_iResetFlag = 2; // set ack
}

void CVABinauralAirTrafficNoiseAudioRenderer::UpdateDirectivitySets( )
{
	const int iGlobalAuralisationMode = m_iCurGlobalAuralizationMode;

	// Check for new data
	for( CVABATNSourceReceiverTransmission* pSRTransmission: m_lSourceReceiverTransmissions )
	{
		CVABATNSoundSource* pSource     = pSRTransmission->pSoundSource;
		CVABATNSoundReceiver* pListener = pSRTransmission->pSoundReceiver;

		int iSourceID   = pSource->pData->iID;
		int iListenerID = pListener->pData->iID;

		CVASoundSourceState* pSourceCur = ( m_pCurSceneState ? m_pCurSceneState->GetSoundSourceState( iSourceID ) : nullptr );
		CVASoundSourceState* pSourceNew = ( m_pNewSceneState ? m_pNewSceneState->GetSoundSourceState( iSourceID ) : nullptr );

		CVAReceiverState* pListenerCur = ( m_pCurSceneState ? m_pCurSceneState->GetReceiverState( iListenerID ) : nullptr );
		CVAReceiverState* pListenerNew = ( m_pNewSceneState ? m_pNewSceneState->GetReceiverState( iListenerID ) : nullptr );

		if( pSourceNew == nullptr )
		{
			pSRTransmission->oDirectSoundPath.oDirectivityStateNew.pData    = nullptr;
			pSRTransmission->oReflectedSoundPath.oDirectivityStateNew.pData = nullptr;
		}
		else
		{
			pSRTransmission->oDirectSoundPath.oDirectivityStateNew.pData    = (IVADirectivity*)pSourceNew->GetDirectivityData( );
			pSRTransmission->oReflectedSoundPath.oDirectivityStateNew.pData = (IVADirectivity*)pSourceNew->GetDirectivityData( );
		}

		if( pListenerNew == nullptr )
		{
			pSRTransmission->oDirectSoundPath.oSoundReceiverDirectivityStateNew.pData    = nullptr;
			pSRTransmission->oReflectedSoundPath.oSoundReceiverDirectivityStateNew.pData = nullptr;
		}
		else
		{
			pSRTransmission->oDirectSoundPath.oSoundReceiverDirectivityStateNew.pData    = (IVADirectivity*)pListenerNew->GetDirectivity( );
			pSRTransmission->oReflectedSoundPath.oSoundReceiverDirectivityStateNew.pData = (IVADirectivity*)pListenerNew->GetDirectivity( );
		}
	}

	return;
}

void CVABinauralAirTrafficNoiseAudioRenderer::UpdateGlobalAuralizationMode( int iGlobalAuralizationMode )
{
	if( m_iCurGlobalAuralizationMode == iGlobalAuralizationMode )
		return;

	m_iCurGlobalAuralizationMode = iGlobalAuralizationMode;

	return;
}

CVAStruct CVABinauralAirTrafficNoiseAudioRenderer::GetParameters( const CVAStruct& oArgs )
{
	return CVAStruct( );
}

void CVABinauralAirTrafficNoiseAudioRenderer::SetParameters( const CVAStruct& oArgs )
{
	if( oArgs.HasKey( "sound_source_id" ) && oArgs.HasKey( "sound_receiver_id" ) )
	{
		const int iSoundSourceID   = oArgs["sound_source_id"];
		const int iSoundReceiverID = oArgs["sound_receiver_id"];

		VA_VERBOSE( "BinauralAirTrafficNoiseRenderer",
		            "Updating binaural air traffic noise renderer path for sound source id " << iSoundSourceID << " and sound receiver id " << iSoundReceiverID );

		for( CVABATNSourceReceiverTransmission* pSRTransmission: ctxAudio.m_lSourceReceiverTransmissions )
		{
			if( pSRTransmission->pSoundSource->pData->iID == iSoundSourceID && pSRTransmission->pSoundReceiver->pData->iID == iSoundReceiverID )
			{
				if( oArgs.HasKey( "direct_path" ) )
					ExternalSimulationUpdate( pSRTransmission->oDirectSoundPath, oArgs["direct_path"] );
				if( oArgs.HasKey( "reflected_path" ) )
					ExternalSimulationUpdate( pSRTransmission->oReflectedSoundPath, oArgs["reflected_path"] );
			}
		}
	}
	if( oArgs.HasKey( "stratified_atmosphere" ) )
	{
		VA_VERBOSE( "BinauralAirTrafficNoiseRenderer", "Updating stratified atmosphere for binaural air traffic noise renderer used for inhomogeneous transmission." );
		SetStratifiedAtmosphere( oArgs["stratified_atmosphere"] );
	}
}

void CVABinauralAirTrafficNoiseAudioRenderer::SetStratifiedAtmosphere( const std::string& sAtmosphereJSONFilepathOrContent )
{
	// TODO: Maybe it is necessary to wait for audio context is idle before changing atmosphere

	if( sAtmosphereJSONFilepathOrContent.find( ".json" ) != std::string::npos )
		ITAGeo::Utils::JSON::Import( m_oStratifiedAtmosphere, sAtmosphereJSONFilepathOrContent );
	else
		ITAGeo::Utils::JSON::Decode( m_oStratifiedAtmosphere, sAtmosphereJSONFilepathOrContent );
}


void ExternalSimulationUpdate( CVABATNSourceReceiverTransmission::CPropagationPath& oPathProperties, const CVAStruct& oUpdate )
{
	if( oUpdate.HasKey( "propagation_time" ) )
	{
		oPathProperties.dPropagationTime = oUpdate["propagation_time"];
	}
	if( oUpdate.HasKey( "geometrical_spreading_loss" ) )
	{
		oPathProperties.dGeometricalSpreadingLoss = oUpdate["geometrical_spreading_loss"];
	}

	if( oUpdate.HasKey( "directivity_third_octaves" ) )
	{
		const CVAStruct& oFilterUpdate( oUpdate["directivity_third_octaves"] );
		ITABase::CThirdOctaveGainMagnitudeSpectrum& oMagnitudeSpectrum( oPathProperties.oSoundSourceDirectivityMagnitudes );
		for( int i = 0; i < oMagnitudeSpectrum.GetNumBands( ); i++ )
		{
			std::string sBandValueKey = "band_" + std::to_string( long( i + 1 ) );
			const double dValue       = oFilterUpdate[sBandValueKey];
			oMagnitudeSpectrum.SetMagnitude( i, float( dValue ) );
		}
	}

	if( oUpdate.HasKey( "air_attenuation_third_octaves" ) )
	{
		const CVAStruct& oFilterUpdate( oUpdate["air_attenuation_third_octaves"] );
		ITABase::CThirdOctaveGainMagnitudeSpectrum& oMagnitudeSpectrum( oPathProperties.oAirAttenuationMagnitudes );
		for( int i = 0; i < oMagnitudeSpectrum.GetNumBands( ); i++ )
		{
			std::string sBandValueKey = "band_" + std::to_string( long( i + 1 ) );
			const double dValue       = oFilterUpdate[sBandValueKey];
			oMagnitudeSpectrum.SetMagnitude( i, float( dValue ) );
		}
	}

	if( oUpdate.HasKey( "temporal_variation_third_octaves" ) )
	{
		const CVAStruct& oFilterUpdate( oUpdate["temporal_variation_third_octaves"] );
		ITABase::CThirdOctaveGainMagnitudeSpectrum& oMagnitudeSpectrum( oPathProperties.oTemporalVariationMagnitudes );
		for( int i = 0; i < oMagnitudeSpectrum.GetNumBands( ); i++ )
		{
			std::string sBandValueKey = "band_" + std::to_string( long( i + 1 ) );
			const double dValue       = oFilterUpdate[sBandValueKey];
			oMagnitudeSpectrum.SetMagnitude( i, float( dValue ) );
		}
	}

	if( oUpdate.HasKey( "ground_reflection_third_octaves" ) )
	{
		const CVAStruct& oFilterUpdate( oUpdate["ground_reflection_third_octaves"] );
		ITABase::CThirdOctaveGainMagnitudeSpectrum& oMagnitudeSpectrum( oPathProperties.oGroundReflectionMagnitudes );
		for( int i = 0; i < oMagnitudeSpectrum.GetNumBands( ); i++ )
		{
			std::string sBandValueKey = "band_" + std::to_string( long( i + 1 ) );
			const double dValue       = oFilterUpdate[sBandValueKey];
			oMagnitudeSpectrum.SetMagnitude( i, float( dValue ) );
		}
	}
}

#endif // VACORE_WITH_RENDERER_BINAURAL_AIR_TRAFFIC_NOISE
