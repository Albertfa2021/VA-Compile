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

#include "VAAudioRendererBase.h"

// VA includes
#include "VAAudioRendererSource.h"
#include "VAAudioRendererReceiver.h"
#	include "../../Motion/VAMotionModelBase.h"
#	include "../../Motion/VASampleAndHoldMotionModel.h"
#	include "../../Motion/VASharedMotionModel.h"
#	include "../../Scene/VAScene.h"
#include "../../Scene/VAMotionState.h"
#	include "../../Utils/VAUtils.h"
#	include "../../VAAudiostreamTracker.h"
#	include "../../VACoreConfig.h"
#	include "../../VALog.h"
#	include "../../VASourceTargetMetrics.h"
#	include "../../core/core.h"
#	include "../../directivities/VADirectivity.h"

#	include <VA.h>
#	include <VAObjectPool.h>
#	include <VAPoolObject.h>
#	include <VAReferenceableObject.h>

// ITA includes
#	include <DAFF.h>
#	include <ITAClock.h>
#	include <ITAConstants.h>
#	include <ITACriticalSection.h>
#	include <ITADataSourceRealization.h>
#	include <ITAFastMath.h>
#	include <ITANumericUtils.h>
#	include <ITASampleBuffer.h>
#	include <ITASampleFrame.h>
#	include <ITAStopWatch.h>
#	include <ITAStreamInfo.h>
#	include <ITAThirdOctaveFilterbank.h>
#	include <ITAThirdOctaveMagnitudeSpectrum.h>
#	include <ITAUPConvolution.h>
#	include <ITAUPFilter.h>
#	include <ITAUPFilterPool.h>
#	include <ITAVariableDelayLine.h>
//#	include <ITAStringUtils.h>

// Vista includes
#	include <VistaInterProcComm/Concurrency/VistaThreadEvent.h>

// 3rdParty includes
#	include <tbb/concurrent_queue.h>

// STL includes
#	include <algorithm>
#	include <atomic>
#	include <cassert>
#	include <fstream>
#	include <iomanip>


//----------------------------
//---------- CONFIG ----------
//----------------------------

CVAAudioRendererBase::Config::Config( const CVAAudioRendererInitParams& oParams, const Config& oDefaultValues )
{
	const CVAStruct& oArgs = *oParams.pConfig;
	CVAConfigInterpreter conf( oArgs );
	const std::string sExceptionMsgPrefix = "Renderer ID '" + oParams.sID + "': ";


	dSampleRate        = oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.dSampleRate;
	iBlockSize         = oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.iBuffersize;
	oCoreConfig        = *oParams.pCore->GetCoreConfig( );
	oHomogeneousMedium = oParams.pCore->oHomogeneousMedium;

	const std::string sVDLSwitchKey = oArgs.HasKey( "VDLSwitchingAlgorithm" ) ? "VDLSwitchingAlgorithm" : "SwitchingAlgorithm"; //For backwards compatability
	conf.OptString( sVDLSwitchKey, sVDLSwitchingAlgorithm, oDefaultValues.sVDLSwitchingAlgorithm );
	const std::string sVDLSwitchingAlgorithm_lower = toLowercase( sVDLSwitchingAlgorithm );

	if( sVDLSwitchingAlgorithm_lower == "switch" )
		iVDLSwitchingAlgorithm = EVDLAlgorithm::SWITCH;
	else if( sVDLSwitchingAlgorithm_lower == "crossfade" )
		iVDLSwitchingAlgorithm = EVDLAlgorithm::CROSSFADE;
	else if( sVDLSwitchingAlgorithm_lower == "linear" )
		iVDLSwitchingAlgorithm = EVDLAlgorithm::LINEAR_INTERPOLATION;
	else if( sVDLSwitchingAlgorithm_lower == "cubicspline" )
		iVDLSwitchingAlgorithm = EVDLAlgorithm::CUBIC_SPLINE_INTERPOLATION;
	else if( sVDLSwitchingAlgorithm_lower == "windowedsinc" )
		iVDLSwitchingAlgorithm = EVDLAlgorithm::WINDOWED_SINC_INTERPOLATION;
	else
		VA_EXCEPT2( INVALID_PARAMETER, sExceptionMsgPrefix + "Unrecognized interpolation algorithm '" + sVDLSwitchingAlgorithm + "' in configuration" );


	// TODO: Do we need this additional delay
	conf.OptNumber( "AdditionalStaticDelaySeconds", dAdditionalStaticDelaySeconds, 0.0f );

	// Receiver motion model
	std::string sKey;
	sKey = oArgs.HasKey( "ReceiverMotionModelNumHistoryKeys" ) ? "ReceiverMotionModelNumHistoryKeys" : "MotionModelNumHistoryKeys";
	conf.OptInteger( sKey, oReceiverMotionModel.iNumHistoryKeys, oDefaultValues.oReceiverMotionModel.iNumHistoryKeys );
	sKey = oArgs.HasKey( "ReceiverMotionModelWindowSize" ) ? "ReceiverMotionModelWindowSize" : "MotionModelWindowSize";
	conf.OptNumber( sKey, oReceiverMotionModel.dWindowSize, oDefaultValues.oReceiverMotionModel.dWindowSize );
	sKey = oArgs.HasKey( "ReceiverMotionModelWindowDelay" ) ? "ReceiverMotionModelWindowDelay" : "MotionModelWindowDelay";
	conf.OptNumber( sKey, oReceiverMotionModel.dWindowDelay, oDefaultValues.oReceiverMotionModel.dWindowDelay );
	conf.OptBool( "ReceiverMotionModelLogInput", oReceiverMotionModel.bLogInputEnabled, oDefaultValues.oReceiverMotionModel.bLogInputEnabled );
	conf.OptBool( "ReceiverMotionModelLogOutput", oReceiverMotionModel.bLogEstimatedOutputEnabled, oDefaultValues.oReceiverMotionModel.bLogEstimatedOutputEnabled );
	if( oReceiverMotionModel.iNumHistoryKeys < 1 )
		VA_EXCEPT2( INVALID_PARAMETER, "Basic motion model history needs to be greater than zero" );
	if( ( oReceiverMotionModel.dWindowSize <= 0 ) || ( oReceiverMotionModel.dWindowDelay < 0 ) )
		VA_EXCEPT2( INVALID_PARAMETER, "Basic motion model window parameters parse error (zero or negative?)" );

	// Source motion model
	sKey = oArgs.HasKey( "SourceMotionModelNumHistoryKeys" ) ? "SourceMotionModelNumHistoryKeys" : "MotionModelNumHistoryKeys";
	conf.OptInteger( sKey, oSourceMotionModel.iNumHistoryKeys, oDefaultValues.oSourceMotionModel.iNumHistoryKeys );
	sKey = oArgs.HasKey( "SourceMotionModelWindowSize" ) ? "SourceMotionModelWindowSize" : "MotionModelWindowSize";
	conf.OptNumber( sKey, oSourceMotionModel.dWindowSize, oDefaultValues.oSourceMotionModel.dWindowSize );
	sKey = oArgs.HasKey( "SourceMotionModelWindowDelay" ) ? "SourceMotionModelWindowDelay" : "MotionModelWindowDelay";
	conf.OptNumber( sKey, oSourceMotionModel.dWindowDelay, oDefaultValues.oSourceMotionModel.dWindowDelay );
	conf.OptBool( "SourceMotionModelLogInput", oSourceMotionModel.bLogInputEnabled, oDefaultValues.oSourceMotionModel.bLogInputEnabled );
	conf.OptBool( "SourceMotionModelLogOutput", oSourceMotionModel.bLogEstimatedOutputEnabled, oDefaultValues.oSourceMotionModel.bLogEstimatedOutputEnabled );
	if( oSourceMotionModel.iNumHistoryKeys < 1 )
		VA_EXCEPT2( INVALID_PARAMETER, "Basic motion model history needs to be greater than zero" );
	if( ( oSourceMotionModel.dWindowSize <= 0 ) || ( oSourceMotionModel.dWindowDelay < 0 ) )
		VA_EXCEPT2( INVALID_PARAMETER, "Basic motion model window parameters parse error (zero or negative?)" );

	conf.OptString( "SimulationConfigFilePath", sSimulationConfigFilePath );
	if( !sSimulationConfigFilePath.empty( ) )
		sSimulationConfigFilePath = oParams.pCore->FindFilePath( sSimulationConfigFilePath );
}


//----------------------------
//--- SOURCE-RECEIVER PAIR ---
//----------------------------

void CVAAudioRendererBase::SourceReceiverPair::PreRequest( )
{
	bDelete   = false;
}
void CVAAudioRendererBase::SourceReceiverPair::PreRelease( )
{
	if( pSource )
		pSource->RemoveReference( );
	if( pReceiver )
		pReceiver->RemoveReference( );
	pSource   = nullptr;
	pReceiver = nullptr;
}

void CVAAudioRendererBase::SourceReceiverPair::InitSourceAndReceiver( CVARendererSource* pSource_, CVARendererReceiver* pReceiver_ )
{
	if( !pSource_ || !pReceiver_ )
		VA_EXCEPT2( INVALID_PARAMETER, "Trying to set invalid source or receiver for SourceReceiverPairFactory" );
	pSource_->AddReference( );
	pReceiver_->AddReference( );
	pSource   = pSource_;
	pReceiver = pReceiver_;
}



//-----------------------------
//----- RENDERER - PUBLIC -----
//-----------------------------

CVAAudioRendererBase::CVAAudioRendererBase( const CVAAudioRendererInitParams& oParams, const Config& oDefaultValues )
    : m_pCore( oParams.pCore )
    , m_oParams( oParams )
    , m_oConf( Config(oParams, oDefaultValues) )
    , m_iNumChannels( -1 )
    , m_bDumpReceivers( false )
    , m_dDumpReceiversGain( 1.0 )
    , m_iCurGlobalAuralizationMode( IVAInterface::VA_AURAMODE_DEFAULT )
    , ctxUser( UserContext( oParams ) )
    , ctxAudio( AudioContext( oParams) )
{
	ctxAudio.m_eResetFlag = EResetFlag::NormalOperation;
	ctxAudio.m_bRunning   = false;
}

CVAAudioRendererBase::~CVAAudioRendererBase( )
{
	if( ctxUser.m_pSourcePool )
		delete ctxUser.m_pSourcePool;
	if( ctxUser.m_pReceiverPool )
		delete ctxUser.m_pReceiverPool;
	if( ctxUser.m_pSourceReceiverPairPool )
		delete ctxUser.m_pSourceReceiverPairPool;
}


void CVAAudioRendererBase::Reset( )
{
	VA_VERBOSE( m_oParams.sClass, "Received reset call, indicating reset now" );
	ctxAudio.m_eResetFlag = EResetFlag::ResetRequest;

	if( ctxAudio.m_bRunning == false || m_oParams.bOfflineRendering )
	{
		VA_VERBOSE( m_oParams.sClass, "Was not streaming, will reset manually" );
		// if no streaming active, reset manually
		// SyncInternalData();
		ctxAudio.Reset( );
	}
	else
	{
		VA_VERBOSE( m_oParams.sClass, "Still streaming, will now wait for reset acknownledge" );
	}

	// Wait for last streaming block before internal reset
	while( ctxAudio.m_eResetFlag != EResetFlag::ResetAcknowledged )
	{
		VASleep( 10 ); // Wait for acknowledge
	}

	VA_VERBOSE( m_oParams.sClass, "Operation reset has green light, clearing items" );

	// Now that audio stream finished, we can reset data from user context
	ctxUser.Reset( );

	ctxAudio.m_eResetFlag = EResetFlag::NormalOperation;

	VA_VERBOSE( m_oParams.sClass, "Reset successful" );
}

void CVAAudioRendererBase::UpdateScene( CVASceneState* pNewSceneState )
{
	// Apply update to user context (synchronous update)
	RendererSceneUpdateMessagePtr pUpdateMsg = ctxUser.Update( pNewSceneState );

	if( pUpdateMsg == nullptr ) // No scene change
		return;

	// Queue update message in audio context (asynchronous update)
	ctxAudio.m_qpUpdateMessages.push( pUpdateMsg );

	//Update scene state of audio context //TODO: Should also update this asynchroniously?
	pNewSceneState->AddReference( );
	if( ctxAudio.m_pCurSceneState )
		ctxAudio.m_pCurSceneState->RemoveReference( );
	ctxAudio.m_pCurSceneState = pNewSceneState;
}

void CVAAudioRendererBase::UpdateGlobalAuralizationMode( int iGlobalAuralizationMode )
{
	if( m_iCurGlobalAuralizationMode == iGlobalAuralizationMode )
		return;

	m_iCurGlobalAuralizationMode = iGlobalAuralizationMode;
}

void CVAAudioRendererBase::SetParameters( const CVAStruct& oParams )
{
	//// TODO: If we still use additional delay, add protected variable m_dAdditionalStaticDelaySeconds and set it here
	//if( oParams.HasKey( "AdditionalStaticDelaySeconds" ) )
	//{
	//	const CVAStructValue* pValue = oParams.GetValue("AdditionalStaticDelaySeconds");
	//	if( pValue->GetDatatype( ) != CVAStructValue::DOUBLE )
	//		VA_EXCEPT2( INVALID_PARAMETER, "Additional delay must be a double" );
	//	//m_dAdditionalStaticDelaySeconds = *pValue;
	//}

	////TODO: Check whether we still require the "dump" receiver function
	//CVAConfigInterpreter oConfig( oParams );
	//std::string sCommandOriginal;
	//oConfig.OptString( "Command", sCommandOriginal );
	//std::string sCommand = toLowercase( sCommandOriginal );
	//if( sCommand == "startdumpreceivers" )
	//{
	//	oConfig.OptNumber( "Gain", m_dDumpReceiversGain, 1.0 );
	//	std::string sFormat;
	//	oConfig.OptString( "FilenameFormat", sFormat, "Receiver$(ReceiverID).wav" );
	//	onStartDumpReceivers( sFormat );
	//}
	//else if( sCommand == "stopdumpreceivers" )
	//{
	//	onStopDumpReceivers( );
	//}
}



ITADatasource* CVAAudioRendererBase::GetOutputDatasource( )
{
	return m_pdsOutput.get();
}

void CVAAudioRendererBase::HandleProcessStream( ITADatasourceRealization*, const ITAStreamInfo* pStreamInfo )
{
	if( ctxAudio.m_bRunning == false )
		ctxAudio.m_bRunning = true;

	ctxAudio.SyncSceneUpdates( );

	// Init output stream with zeros
	std::vector<float*> pfOutputCh;
	for( int i = 0; i < m_iNumChannels; i++ )
	{
		float* pfCurrentOutputChannel = m_pdsOutput->GetWritePointer( i );
		fm_zero( pfCurrentOutputChannel, m_pdsOutput->GetBlocklength( ) );
		pfOutputCh.push_back( pfCurrentOutputChannel );
	}

	const CVAAudiostreamState* pStreamState = dynamic_cast<const CVAAudiostreamState*>( pStreamInfo );
	const double dReceiverTime              = pStreamState->dSysTime;

	// Check for reset request
	if( ctxAudio.m_eResetFlag == EResetFlag::ResetRequest )
	{
		VA_VERBOSE( m_oParams.sClass, "Process stream detecting reset request, will reset internally now" );
		ctxAudio.Reset( );
		return;
	}
	else if( ctxAudio.m_eResetFlag == EResetFlag::ResetAcknowledged )
	{
		VA_VERBOSE( m_oParams.sClass, "Process stream detecting ongoing reset, will stop processing here" );
		return;
	}

	ctxAudio.UpdateSourceAndReceiverPoses( dReceiverTime );

	// Init receiver outputs with zeros
	std::list<CVARendererReceiver*>::iterator lit = ctxAudio.m_lReceivers.begin( );
	while( lit != ctxAudio.m_lReceivers.end( ) )
	{
		CVARendererReceiver* pReceiver( *( lit++ ) );
		pReceiver->psfOutput->zero( );
	}

	// Iterate through source-receiver pairs and process them
	for( SourceReceiverPair* pSRPair: ctxAudio.m_lSourceReceiverPairs )
	{
		CVAReceiverState* pReceiverState  = ( ctxAudio.m_pCurSceneState ? ctxAudio.m_pCurSceneState->GetReceiverState( pSRPair->GetReceiver( )->pData->iID ) : nullptr );
		CVASoundSourceState* pSourceState = ( ctxAudio.m_pCurSceneState ? ctxAudio.m_pCurSceneState->GetSoundSourceState( pSRPair->GetSource( )->pData->iID ) : nullptr );

		
		if( pReceiverState == nullptr || pSourceState == nullptr ) // Skip pairs with insufficient source or receiver state
			continue;

		if( !pSRPair->GetReceiver( )->HasValidTrajectory( ) || !pSRPair->GetSource( )->HasValidTrajectory( ) ) // Skip pairs without valid position for source or receiver
			continue;

		pSRPair->Process( dReceiverTime, OverallAuralizationMode( pSourceState, pReceiverState ), *pSourceState,
		                  *pReceiverState );
	}

	// Accumulate output signals of all receivers
	for( auto it: ctxAudio.m_lReceivers )
	{
		CVARendererReceiver* pActiveReceiver( it );
		for( int i = 0; i < m_iNumChannels; i++ )
			if( !pActiveReceiver->pData->bMuted )
				fm_add( pfOutputCh[i], ( *pActiveReceiver->psfOutput )[i].data( ), m_pdsOutput->GetBlocklength( ) ); // initial data should be zero.
	}

	/// TODO: Check whether we still need the dump receiver function
	//// Receiver dumping
	//for( auto it: ctxAudio.m_lReceivers )
	//{
	//	if( m_iDumpReceiversFlag > 0 )
	//	{
	//		std::map<int, CVARendererReceiver*>::iterator it = m_mReceivers.begin( );
	//		while( it != m_mReceivers.end( ) )
	//		{
	//			CVARendererReceiver* pReceiver = it++->second;
	//			pReceiver->psfOutput->mul_scalar( float( m_dDumpReceiversGain ) );
	//			pReceiver->pOutputAudioFileWriter->write( pReceiver->psfOutput );
	//		}

	//		// Ack on dump stop
	//		if( m_iDumpReceiversFlag == 2 )
	//			m_iDumpReceiversFlag = 0;
	//	}
	//}

	m_pdsOutput->IncrementWritePointer( );
}

void CVAAudioRendererBase::InitBaseDSPEntities( int iNumChannels )
{
	if( m_pdsOutput )
		VA_EXCEPT2( INVALID_PARAMETER, "Renderer ID '" + m_oParams.sID + "': DSP Elements can only be initialized once! Duplicate call of InitDSPElements()!" );

	m_iNumChannels = iNumChannels;
	const double dSampleRate = m_pCore->GetCoreConfig( )->oAudioDriverConfig.dSampleRate;
	const int iBlockLength   = m_pCore->GetCoreConfig( )->oAudioDriverConfig.iBuffersize;

	m_pdsOutput = std::make_unique<ITADatasourceRealization>( m_iNumChannels, dSampleRate, iBlockLength );
	m_pdsOutput->SetStreamEventHandler( this );

	IVAPoolObjectFactory* pReceiverFactory = new CVAReceiverPoolFactory( m_iNumChannels, iBlockLength, m_oConf.oReceiverMotionModel );
	ctxUser.m_pReceiverPool                = IVAObjectPool::Create( 16, 2, pReceiverFactory, true );

	IVAPoolObjectFactory* pSourceFactory = new CVASourcePoolFactory( m_oConf.oSourceMotionModel );
	ctxUser.m_pSourcePool                = IVAObjectPool::Create( 16, 2, pSourceFactory, true );

	ctxAudio.m_sbTemp.Init( iBlockLength, true );
}

void CVAAudioRendererBase::InitSourceReceiverPairPool( IVAPoolObjectFactory* pFactory, int iNumInitialObjects, int iDeltaNumObjects )
{
	if( ctxUser.m_pSourceReceiverPairPool )
		VA_EXCEPT2( INVALID_PARAMETER, "Renderer ID '" + m_oParams.sID + "': SourceReceiverPairPool can only be initialized once! Duplicate call of InitSourceReceiverPairPool()!" );

	if( !pFactory )
		VA_EXCEPT2( INVALID_PARAMETER, "Renderer ID '" + m_oParams.sID + "': Invalid pool object in InitSourceReceiverPairPool()!" );

	ctxUser.m_pSourceReceiverPairFactory = pFactory;
	ctxUser.m_pSourceReceiverPairPool    = IVAObjectPool::Create( iNumInitialObjects, iDeltaNumObjects, pFactory, true );
}


//--------------------------------
//----- RENDERER - PROTECTED -----
//--------------------------------

CVAAudioRendererBase::AuralizationMode CVAAudioRendererBase::OverallAuralizationMode( CVASoundSourceState* pSourceState, CVAReceiverState* pReceiverState ) const
{
	int iAuraMode = m_iCurGlobalAuralizationMode & pSourceState->GetAuralizationMode( ) & pReceiverState->GetAuralizationMode( );

	AuralizationMode oAuraMode;
	oAuraMode.bDirectSound        = ( iAuraMode & IVAInterface::VA_AURAMODE_DIRECT_SOUND ) > 0;
	oAuraMode.bEarlyReflections   = ( iAuraMode & IVAInterface::VA_AURAMODE_EARLY_REFLECTIONS ) > 0;
	oAuraMode.bDiffuseDecay       = ( iAuraMode & IVAInterface::VA_AURAMODE_DIFFUSE_DECAY ) > 0;
	oAuraMode.bSourceDirectivity  = ( iAuraMode & IVAInterface::VA_AURAMODE_SOURCE_DIRECTIVITY ) > 0;
	oAuraMode.bMediumAbsorption   = ( iAuraMode & IVAInterface::VA_AURAMODE_MEDIUM_ABSORPTION ) > 0;
	oAuraMode.bTemporalVariations = ( iAuraMode & IVAInterface::VA_AURAMODE_TEMP_VAR ) > 0;
	oAuraMode.bScattering         = ( iAuraMode & IVAInterface::VA_AURAMODE_SCATTERING ) > 0;
	oAuraMode.bDiffraction        = ( iAuraMode & IVAInterface::VA_AURAMODE_DIFFRACTION ) > 0;
	oAuraMode.bDoppler            = ( iAuraMode & IVAInterface::VA_AURAMODE_DOPPLER ) > 0;
	oAuraMode.bSpreadingLoss      = ( iAuraMode & IVAInterface::VA_AURAMODE_SPREADING_LOSS ) > 0;
	oAuraMode.bSoundTransmission  = ( iAuraMode & IVAInterface::VA_AURAMODE_TRANSMISSION ) > 0;
	oAuraMode.bMaterialAbsorption = ( iAuraMode & IVAInterface::VA_AURAMODE_ABSORPTION ) > 0;
	return oAuraMode;
}

CVAAudioRendererBase::SourceReceiverPair* CVAAudioRendererBase::GetSourceReceiverPair( int iSourceID, int iReceiverID ) const
{
	for( auto pSRPair: ctxAudio.m_lSourceReceiverPairs )
	{
		if( !pSRPair->GetSource( ) || !pSRPair->GetReceiver( ) )
			continue;
		
		if( pSRPair->GetSource( )->pData->iID == iSourceID && pSRPair->GetReceiver( )->pData->iID == iReceiverID )
			return pSRPair;
	}
	return nullptr;
}

std::vector<const CVAAudioRendererBase::SourceReceiverPair*> CVAAudioRendererBase::GetSourceReceiverPairs( ) const
{
	std::vector<const SourceReceiverPair*> vSourceReceiverPairs( ctxAudio.m_lSourceReceiverPairs.size() );
	int idx = 0;
	for( auto pSRPair: ctxAudio.m_lSourceReceiverPairs )
	{
		vSourceReceiverPairs[idx++] = pSRPair;
	}
	return vSourceReceiverPairs;
}


//-----------------------------------------
//----- RENDERER PRIVATE USER-CONTEXT -----
//-----------------------------------------

void CVAAudioRendererBase::UserContext::Reset( )
{
	// Iterate over sound pathes and free items
	std::list<SourceReceiverPair*>::iterator it = m_lSourceReceiverPairs.begin( );
	for( SourceReceiverPair* pSRPair: m_lSourceReceiverPairs )
	{
		const int iNumRefs = pSRPair->GetNumReferences( );
		assert( iNumRefs == 1 );
		pSRPair->RemoveReference( );
	}
	m_lSourceReceiverPairs.clear( );

	// Iterate over receivers and free items
	std::map<int, CVARendererReceiver*>::const_iterator lcit = m_mReceivers.begin( );
	while( lcit != m_mReceivers.end( ) )
	{
		CVARendererReceiver* pReceiver( lcit->second );
		pReceiver->pData->RemoveReference( );
		assert( pReceiver->GetNumReferences( ) == 1 );
		pReceiver->RemoveReference( );
		lcit++;
	}
	m_mReceivers.clear( );

	// Iterate over sources and free items
	std::map<int, CVARendererSource*>::const_iterator scit = m_mSources.begin( );
	while( scit != m_mSources.end( ) )
	{
		CVARendererSource* pSource( scit->second );
		pSource->pData->RemoveReference( );
		assert( pSource->GetNumReferences( ) == 1 );
		pSource->RemoveReference( );
		scit++;
	}
	m_mSources.clear( );

	// Scene frei geben
	if( m_pCurSceneState )
	{
		m_pCurSceneState->RemoveReference( );
		m_pCurSceneState = nullptr;
	}
}

CVAAudioRendererBase::RendererSceneUpdateMessagePtr CVAAudioRendererBase::UserContext::Update( CVASceneState* pNewSceneState )
{
	assert( pNewSceneState );

	m_pNewSceneState = pNewSceneState;
	if( m_pNewSceneState == m_pCurSceneState )
		return nullptr;

	// Add reference to new scene to avoid deletion
	m_pNewSceneState->AddReference( );

	CVASceneStateDiff oDiff;
	pNewSceneState->Diff( m_pCurSceneState, &oDiff );

	m_pUpdateMessage = std::make_shared<CVARendererSceneUpdateMessage>( );
	UpdateSourceReceiverPairs( m_pCurSceneState, pNewSceneState, &oDiff );
	UpdateTrajectories( );

	// Free old scene (de-referencing)
	if( m_pCurSceneState )
		m_pCurSceneState->RemoveReference( );
	m_pCurSceneState = m_pNewSceneState;
	m_pNewSceneState = nullptr;

	return m_pUpdateMessage;
}

void CVAAudioRendererBase::UserContext::UpdateSourceReceiverPairs( const CVASceneState* pCurScene, const CVASceneState* pNewScene, const CVASceneStateDiff* pDiff )
{
	// Warning: take care for explicit sources and receivers for this renderer!

	// Iterate over current source-receiver pairs and mark deleted (will be removed within internal sync of audio context thread)
	std::list<SourceReceiverPair*>::iterator itp = m_lSourceReceiverPairs.begin( );
	while( itp != m_lSourceReceiverPairs.end( ) )
	{
		SourceReceiverPair* pSourceReceiverPair( *itp );
		const int iSourceID      = pSourceReceiverPair->GetSource()->pData->iID;
		const int iReceiverID    = pSourceReceiverPair->GetReceiver()->pData->iID;
		bool bDeletetionRequired = false;

		// Source deleted?
		std::vector<int>::const_iterator cits = pDiff->viDelSoundSourceIDs.begin( );
		while( cits != pDiff->viDelSoundSourceIDs.end( ) )
		{
			const int& iIDDeletedSource( *cits++ );
			if( iSourceID == iIDDeletedSource )
			{
				bDeletetionRequired = true; // Source removed, deletion required
				break;
			}
		}

		if( bDeletetionRequired == false )
		{
			// Receiver deleted?
			std::vector<int>::const_iterator citr = pDiff->viDelReceiverIDs.begin( );
			while( citr != pDiff->viDelReceiverIDs.end( ) )
			{
				const int& iIDReceiverDeleted( *citr++ );
				if( iReceiverID == iIDReceiverDeleted )
				{
					bDeletetionRequired = true; // Receiver removed, deletion required
					break;
				}
			}
		}

		if( bDeletetionRequired )
		{
			DeleteSourceReceiverPair( pSourceReceiverPair );
			itp = m_lSourceReceiverPairs.erase( itp ); // Increment via erase on path list
		}
		else
		{
			++itp; // no deletion detected, continue
		}
	}

	// Deleted sources
	std::vector<int>::const_iterator cits = pDiff->viDelSoundSourceIDs.begin( );
	while( cits != pDiff->viDelSoundSourceIDs.end( ) )
	{
		const int& iID( *cits++ );
		DeleteSource( iID );
	}

	// Deleted receivers
	std::vector<int>::const_iterator citr = pDiff->viDelReceiverIDs.begin( );
	while( citr != pDiff->viDelReceiverIDs.end( ) )
	{
		const int& iID( *citr++ );
		DeleteReceiver( iID );
	}

	// New sources
	cits = pDiff->viNewSoundSourceIDs.begin( );
	while( cits != pDiff->viNewSoundSourceIDs.end( ) )
	{
		const int& iID( *cits++ );

		// Only add, if no other renderer has been connected explicitly with this source
		const CVASoundSourceDesc* pSoundSourceDesc = m_pCore->GetSceneManager( )->GetSoundSourceDesc( iID );
		if( pSoundSourceDesc->sExplicitRendererID.empty( ) || pSoundSourceDesc->sExplicitRendererID == m_sRendererID )
			CVARendererSource* pSource = CreateSource( iID, pNewScene->GetSoundSourceState( iID ) );
	}

	// New receivers
	citr = pDiff->viNewReceiverIDs.begin( );
	while( citr != pDiff->viNewReceiverIDs.end( ) )
	{
		const int& iID( *citr++ );
		CVARendererReceiver* pReceiver = CreateReceiver( iID, pNewScene->GetReceiverState( iID ) );
	}

	// New SR-Pairs: (1) new receivers, current sources
	citr = pDiff->viNewReceiverIDs.begin( );
	while( citr != pDiff->viNewReceiverIDs.end( ) )
	{
		int iReceiverID           = ( *citr++ );
		CVARendererReceiver* pReceiver = m_mReceivers[iReceiverID];

		for( size_t i = 0; i < pDiff->viComSoundSourceIDs.size( ); i++ )
		{
			// Only add, if no other renderer has been connected explicitly with this source
			// and only, if not marked for deletion
			int iSourceID                             = pDiff->viComSoundSourceIDs[i];
			std::map<int, CVARendererSource*>::iterator it = m_mSources.find( iSourceID );
			if( it == m_mSources.end( ) )
				continue; // This source is skipped by the renderer
			CVARendererSource* pSource = it->second;

			const CVASoundSourceDesc* pSoundSourceDesc = m_pCore->GetSceneManager( )->GetSoundSourceDesc( iSourceID );
			if( !pSource->IsDeleted( ) && ( pSoundSourceDesc->sExplicitRendererID.empty( ) || pSoundSourceDesc->sExplicitRendererID == m_sRendererID ) )
				CreateSourceReceiverPair( pSource, pReceiver );
		}
	}

	// New SR-Pairs: (2) new sources, current receivers
	cits = pDiff->viNewSoundSourceIDs.begin( );
	while( cits != pDiff->viNewSoundSourceIDs.end( ) )
	{
		const int& iSourceID( *cits++ );
		std::map<int, CVARendererSource*>::iterator it = m_mSources.find( iSourceID );
		if( it == m_mSources.end( ) )
			continue; // Explicit source is not connected to this renderer

		CVARendererSource* pSource = it->second;
		for( size_t i = 0; i < pDiff->viComReceiverIDs.size( ); i++ )
		{
			int iReceiverID                = pDiff->viComReceiverIDs[i];
			CVARendererReceiver* pReceiver = m_mReceivers[iReceiverID];
			if( !pReceiver->IsDeleted( ) )
				CreateSourceReceiverPair( pSource, pReceiver );
		}
	}

	// New SR-Pairs: (3) new sources, new receivers
	cits = pDiff->viNewSoundSourceIDs.begin( );
	while( cits != pDiff->viNewSoundSourceIDs.end( ) )
	{
		const int& iSourceID( *cits++ );
		std::map<int, CVARendererSource*>::iterator it = m_mSources.find( iSourceID );

		if( it == m_mSources.end( ) )
			continue;

		CVARendererSource* pSource = it->second;
		assert( pSource );

		citr = pDiff->viNewReceiverIDs.begin( );
		while( citr != pDiff->viNewReceiverIDs.end( ) )
		{
			const int& iReceiverID( *citr++ );
			CVARendererReceiver* pReceiver = m_mReceivers[iReceiverID];
			CreateSourceReceiverPair( pSource, pReceiver );
		}
	}

}
void CVAAudioRendererBase::UserContext::CreateSourceReceiverPair( CVARendererSource* pSource, CVARendererReceiver* pReceiver )
{
	int iSourceID   = pSource->pData->iID;
	int iReceiverID = pReceiver->pData->iID;

	assert( !pSource->IsDeleted( ) && !pReceiver->IsDeleted( ) );

	VA_VERBOSE( m_sClass, "Creating source-receiver pair from source " << iSourceID << " -> receiver " << iReceiverID );

	SourceReceiverPair* pSRPair = dynamic_cast<SourceReceiverPair*>( m_pSourceReceiverPairPool->RequestObject( ) );
	pSRPair->InitSourceAndReceiver( pSource, pReceiver );

	m_lSourceReceiverPairs.push_back( pSRPair );
	m_pUpdateMessage->vNewSourceReceiverPairs.push_back( pSRPair );
}

void CVAAudioRendererBase::UserContext::DeleteSourceReceiverPair( SourceReceiverPair* pSRPair )
{
	VA_VERBOSE( m_sClass, "Marking source-receiver pair from source " << pSRPair->GetSource( )->pData->iID << " -> receiver " << pSRPair->GetReceiver( )->pData->iID
	                                                                  << " for deletion" );

	pSRPair->MarkDeleted( );
	pSRPair->RemoveReference( );
	m_pUpdateMessage->vDelSourceReceiverPairs.push_back( pSRPair );
}

CVARendererReceiver* CVAAudioRendererBase::UserContext::CreateReceiver( const int iID, const CVAReceiverState* pReceiverState )
{
	VA_VERBOSE( m_sClass, "Creating receiver with ID " << iID );

	CVARendererReceiver* pReceiver = dynamic_cast<CVARendererReceiver*>( m_pReceiverPool->RequestObject( ) ); // Reference = 1

	pReceiver->pData = m_pCore->GetSceneManager( )->GetSoundReceiverDesc( iID );
	pReceiver->pData->AddReference( );
	assert( pReceiver->pData );
	pReceiver->SetMotionModelName( std::string( m_sRendererID + "_mm_receiver_" + pReceiver->pData->sName ) );

	m_mReceivers.insert( std::pair<int, CVARendererReceiver*>( iID, pReceiver ) );
	m_pUpdateMessage->vNewReceivers.push_back( pReceiver );

	return pReceiver;
}
void CVAAudioRendererBase::UserContext::DeleteReceiver( int iReceiverID )
{
	VA_VERBOSE( m_sClass, "Marking receiver with ID " << iReceiverID << " for removal" );
	std::map<int, CVARendererReceiver*>::iterator it = m_mReceivers.find( iReceiverID );

	CVARendererReceiver* pReceiver = it->second;
	m_mReceivers.erase( it );
	pReceiver->MarkDeleted( );
	pReceiver->pData->RemoveReference( );
	pReceiver->RemoveReference( );

	m_pUpdateMessage->vDelReceivers.push_back( pReceiver );
}
CVARendererSource* CVAAudioRendererBase::UserContext::CreateSource( int iID, const CVASoundSourceState* pSourceState )
{
	VA_VERBOSE( m_sClass, "Creating source with ID " << iID );
	CVARendererSource* pSource = dynamic_cast<CVARendererSource*>( m_pSourcePool->RequestObject( ) );

	pSource->pData = m_pCore->GetSceneManager( )->GetSoundSourceDesc( iID );
	pSource->pData->AddReference( );
	pSource->SetMotionModelName( std::string( m_sRendererID + "_mm_source_" + pSource->pData->sName ) );

	m_mSources.insert( std::pair<int, CVARendererSource*>( iID, pSource ) );
	m_pUpdateMessage->vNewSources.push_back( pSource );

	return pSource;
}
void CVAAudioRendererBase::UserContext::DeleteSource( int iSourceID )
{
	VA_VERBOSE( m_sClass, "Marking source with ID " << iSourceID << " for removal" );
	std::map<int, CVARendererSource*>::iterator it = m_mSources.find( iSourceID );

	if( it == m_mSources.end( ) ) // Not found in internal list ...
	{
		CVASoundSourceDesc* pDesc = m_pCore->GetSceneManager( )->GetSoundSourceDesc( iSourceID );
		if( !pDesc->sExplicitRendererID.empty( ) || pDesc->sExplicitRendererID == m_sRendererID )
			VA_WARN( m_sClass, "Attempted to remote an explicit sound source for this renderer which could not be found." );
		return;
	}

	CVARendererSource* pSource = it->second;
	m_mSources.erase( it );
	pSource->MarkDeleted( );
	pSource->pData->RemoveReference( );
	pSource->RemoveReference( );

	m_pUpdateMessage->vDelSources.push_back( pSource );
}

void CVAAudioRendererBase::UserContext::UpdateTrajectories( )
{
	// Sources
	for( std::map<int, CVARendererSource*>::iterator it = m_mSources.begin( ); it != m_mSources.end( ); ++it )
	{
		int iSourceID              = it->first;
		CVARendererSource* pSource = it->second;

		CVASoundSourceState* pSourceCur = ( m_pCurSceneState ? m_pCurSceneState->GetSoundSourceState( iSourceID ) : nullptr );
		CVASoundSourceState* pSourceNew = ( m_pNewSceneState ? m_pNewSceneState->GetSoundSourceState( iSourceID ) : nullptr );

		const CVAMotionState* pMotionCur = ( pSourceCur ? pSourceCur->GetMotionState( ) : nullptr );
		const CVAMotionState* pMotionNew = ( pSourceNew ? pSourceNew->GetMotionState( ) : nullptr );

		if( pMotionNew && ( pMotionNew != pMotionCur ) )
		{
			VA_TRACE( m_sClass, "Source " << iSourceID << " new motion state" );
			pSource->InsertMotionKey( pMotionNew );
		}
	}

	// Receivers
	for( std::map<int, CVARendererReceiver*>::iterator it = m_mReceivers.begin( ); it != m_mReceivers.end( ); ++it )
	{
		int iReceiverID                = it->first;
		CVARendererReceiver* pReceiver = it->second;

		CVAReceiverState* pReceiverCur = ( m_pCurSceneState ? m_pCurSceneState->GetReceiverState( iReceiverID ) : nullptr );
		CVAReceiverState* pReceiverNew = ( m_pNewSceneState ? m_pNewSceneState->GetReceiverState( iReceiverID ) : nullptr );

		const CVAMotionState* pMotionCur = ( pReceiverCur ? pReceiverCur->GetMotionState( ) : nullptr );
		const CVAMotionState* pMotionNew = ( pReceiverNew ? pReceiverNew->GetMotionState( ) : nullptr );

		if( pMotionNew && ( pMotionNew != pMotionCur ) )
		{
			VA_TRACE( m_sClass, "Receiver " << iReceiverID << " new position " ); // << *pMotionNew);
			pReceiver->InsertMotionKey( pMotionNew );
		}
	}
}


//------------------------------------------
//----- RENDERER PRIVATE AUDIO-CONTEXT -----
//------------------------------------------

void CVAAudioRendererBase::AudioContext::Reset( )
{
	VA_VERBOSE( m_sClass, "Resetting internally (audio context)" );

	for( SourceReceiverPair* pSRPair: m_lSourceReceiverPairs )
	{
		pSRPair->RemoveReference( );
	}
	m_lSourceReceiverPairs.clear( );

	for( CVARendererReceiver* pReceiver: m_lReceivers )
	{
		pReceiver->pData->RemoveReference( );
		pReceiver->RemoveReference( );
	}
	m_lReceivers.clear( );

	for( CVARendererSource* pSource: m_lSources )
	{
		pSource->pData->RemoveReference( );
		pSource->RemoveReference( );
	}
	m_lSources.clear( );

	m_eResetFlag = EResetFlag::ResetAcknowledged;
}

void CVAAudioRendererBase::AudioContext::SyncSceneUpdates( )
{
	RendererSceneUpdateMessagePtr pUpdate;
	while( m_qpUpdateMessages.try_pop( pUpdate ) )
	{
		for( SourceReceiverPair* pPath: pUpdate->vDelSourceReceiverPairs )
		{
			m_lSourceReceiverPairs.remove( pPath );
			pPath->RemoveReference( );
		}
		for( SourceReceiverPair* pPath: pUpdate->vNewSourceReceiverPairs )
		{
			pPath->AddReference( );
			m_lSourceReceiverPairs.push_back( pPath );
		}

		std::list<CVARendererSource*>::const_iterator cits = pUpdate->vDelSources.begin( );
		while( cits != pUpdate->vDelSources.end( ) )
		{
			CVARendererSource* pSource( *cits++ );
			m_lSources.remove( pSource );
			pSource->pData->RemoveReference( );
			pSource->RemoveReference( );
		}

		cits = pUpdate->vNewSources.begin( );
		while( cits != pUpdate->vNewSources.end( ) )
		{
			CVARendererSource* pSource( *cits++ );
			pSource->AddReference( );
			pSource->pData->AddReference( );
			m_lSources.push_back( pSource );
		}

		std::list<CVARendererReceiver*>::const_iterator citr = pUpdate->vDelReceivers.begin( );
		while( citr != pUpdate->vDelReceivers.end( ) )
		{
			CVARendererReceiver* pReceiver( *citr++ );
			m_lReceivers.remove( pReceiver );
			pReceiver->pData->RemoveReference( );
			pReceiver->RemoveReference( );
		}

		citr = pUpdate->vNewReceivers.begin( );
		while( citr != pUpdate->vNewReceivers.end( ) )
		{
			CVARendererReceiver* pReceiver( *citr++ );
			pReceiver->AddReference( );
			pReceiver->pData->AddReference( );
			m_lReceivers.push_back( pReceiver );
		}

	}

}

void CVAAudioRendererBase::AudioContext::UpdateSourceAndReceiverPoses( double dTime )
{
	for( std::list<CVARendererSource*>::iterator it = m_lSources.begin( ); it != m_lSources.end( ); ++it )
	{
		CVARendererSource* pSource = *it;
		pSource->EstimateCurrentPose( dTime );
	}

	for( std::list<CVARendererReceiver*>::iterator it = m_lReceivers.begin( ); it != m_lReceivers.end( ); ++it )
	{
		CVARendererReceiver* pReceiver = *it;
		pReceiver->EstimateCurrentPose( dTime );
	}
}
