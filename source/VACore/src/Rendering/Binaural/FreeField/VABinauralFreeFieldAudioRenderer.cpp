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

#include "VABinauralFreefieldAudioRenderer.h"

#ifdef VACORE_WITH_RENDERER_BINAURAL_FREE_FIELD

// VA includes
#	include <VA.h>

// VA core includes
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
#	include "../../../directivities/VADirectivityDAFFHATOHRIR.h"
#	include "../../../directivities/VADirectivityDAFFHRIR.h"

#	include <VAObjectPool.h>

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
#	include <ITAUPConvolution.h>
#	include <ITAUPFilter.h>
#	include <ITAUPFilterPool.h>
#	include <ITAVariableDelayLine.h>

// Vista includes
#	include <VistaBase/VistaQuaternion.h>
#	include <VistaInterProcComm/Concurrency/VistaThreadEvent.h>

// 3rdParty includes
#	include <tbb/concurrent_queue.h>

// STL includes
#	include <algorithm>
#	include <atomic>
#	include <cassert>
#	include <fstream>
#	include <iomanip>


class CVABFFSoundPath : public CVAPoolObject
{
public:
	virtual ~CVABFFSoundPath( );

	//! Retarded metrics of sound path
	class Metrics
	{
	public:
		double dRetardedDistance;               //!< Metrical distance to retarded sound position
		VAQuat qOrientationRetSourceToReceiver; //!< Immediate angle of incidence to retarded source position in receiver reference frame in YPR convention
		VAQuat qOrientationReceiverToRetSource; //!< Retarded angle of incidence to receiver in source reference frame in YPR convention
	};

	//! State of directivity
	class CDirectivityState
	{
	public:
		inline CDirectivityState( ) : pData( NULL ), iRecord( -1 ), bDirectivityEnabled( true ) { };

		IVADirectivity* pData; //!< Directivity data, may be NULL
		int iRecord;           //!< Directivity index
		bool bDirectivityEnabled;

		inline bool operator==( const CDirectivityState& rhs ) const
		{
			bool bBothEnabled     = ( bDirectivityEnabled == rhs.bDirectivityEnabled );
			bool bSameRecordIndex = ( iRecord == rhs.iRecord );
			bool bSameData        = ( pData == rhs.pData );

			return ( bBothEnabled && bSameRecordIndex && bSameData );
		};
	};

	class CHRIRState
	{
	public:
		inline CHRIRState( ) : pData( NULL ), iRecord( -1 ), fDistance( 1.0f ), dHATODeg( 0.0f ) { };

		IVADirectivity* pData; //!< HRIR data, may be NULL
		int iRecord;           //!< HRIR index
		float fDistance;       //!< HRIR dataset distance
		double dHATODeg;       //!< HRIR head-above-torso orientation

		inline bool operator!=( const CHRIRState& rhs ) const
		{
			if( pData != rhs.pData )
				return true;
			if( fDistance != rhs.fDistance )
				return true;
			if( iRecord != rhs.iRecord )
				return true;
			if( dHATODeg != rhs.dHATODeg )
				return true;
			return false;
		};
	};

	CVABinauralFreeFieldAudioRenderer::CVABFFSource* pSource;
	CVABinauralFreeFieldAudioRenderer::CVABFFReceiver* pReceiver;

	CVASourceTargetMetrics oRelations; //!< Informatioen on source and receiver relations (distances & angles)

	CDirectivityState oDirectivityStateCur;
	CDirectivityState oDirectivityStateNew;

	CHRIRState oHRIRStateCur;
	CHRIRState oHRIRStateNew;

	std::atomic<bool> bDelete;

	CITAThirdOctaveFilterbank* pThirdOctaveFilterBank;
	CITAVariableDelayLine* pVariableDelayLineChL;
	CITAVariableDelayLine* pVariableDelayLineChR;
	ITAUPConvolution* pFIRConvolverChL;
	ITAUPConvolution* pFIRConvolverChR;

	inline void PreRequest( )
	{
		pSource   = nullptr;
		pReceiver = nullptr;

		// Reset DSP elements
		pThirdOctaveFilterBank->Clear( );
		pVariableDelayLineChL->Clear( );
		pVariableDelayLineChR->Clear( );
		pFIRConvolverChL->clear( );
		pFIRConvolverChR->clear( );
	};

	void UpdateSourceDirectivity( bool bDIRAuraModeEnabled );
	void UpdateMediumPropagation( double dSpeedOfSound, double dAdditionalDelaySeconds = 0.0f );
	double CalculateInverseDistanceDecrease( const double dMinimumDistance ) const;
	void UpdateHRIR( const CVAReceiverState::CVAAnthropometricParameter&, const double dHATODeg );

	//! Bestimmt die relativen Gren des Pfades
	/**
	 * Diese berechneten Gren dienen als Grundlage zur Bestimmung der ausgewhlten
	 * Datenstze und Einstellungen der DSP-Elemente. Ein weiteres Update der einzelnen
	 * DSP-Elemente fhrt z.B. zum Filteraustausch, wenn die Statusnderung Auswirkungen hat
	 * (tatschlich ein neuer Datensatz geholt werden muss).
	 *
	 * Diese Methode ist besonders leichtgewichtig, da sie im StreamProcess genutzt wird.
	 *
	 * // spter -> \return Gibt false zurck, falls die retardierten Werte noch nicht zur Verfgung stehen.
	 */
	void UpdateMetrics( );
	// bool UpdateMetrics( double dTimestamp, double dSpeedOfSound ); // For retarded information

private:
	//! Standard-Konstruktor deaktivieren
	CVABFFSoundPath( );

	//! Konstruktor
	CVABFFSoundPath( double dSamplerate, int iBlocklength, int iHRIRFilterLength, int iDirFilterLength, int iFilterBankType );

	ITASampleFrame m_sfHRIRTemp;         //!< Intern verwendeter Zwischenspeicher fr HRIR Datentze
	EVDLAlgorithm m_iDefaultVDLSwitchingAlgorithm; //!< Umsetzung der Verzgerungsnderung

	friend class CVABFFSoundPathFactory;
};

class CVABFFSoundPathFactory : public IVAPoolObjectFactory
{
public:
	CVABFFSoundPathFactory( double dSamplerate, int iBlocklength, int iHRIRFilterLength, int iDirFilterLength, int iFilterBankType )
	    : m_dSamplerate( dSamplerate )
	    , m_iBlocklength( iBlocklength )
	    , m_iHRIRFilterLength( iHRIRFilterLength )
	    , m_iDirFilterLength( iDirFilterLength )
	    , m_iFilterBankType( iFilterBankType ) { };

	CVAPoolObject* CreatePoolObject( ) { return new CVABFFSoundPath( m_dSamplerate, m_iBlocklength, m_iHRIRFilterLength, m_iDirFilterLength, m_iFilterBankType ); };

private:
	double m_dSamplerate;    //!< Abtastrate
	int m_iBlocklength;      //!< Blocklnge
	int m_iHRIRFilterLength; //!< Filterlnge der HRIR
	int m_iDirFilterLength;  //!< Filterlnge der Richtcharakteristik
	int m_iFilterBankType;   //!< IIR or FIR
};

class CVABFFReceiverPoolFactory : public IVAPoolObjectFactory
{
public:
	CVABFFReceiverPoolFactory( CVACoreImpl* pCore, const CVABinauralFreeFieldAudioRenderer::CVABFFReceiver::Config& oConf )
	    : m_pCore( pCore )
	    , m_oReceiverConf( oConf ) { };

	CVAPoolObject* CreatePoolObject( )
	{
		CVABinauralFreeFieldAudioRenderer::CVABFFReceiver* pReceiver;
		pReceiver = new CVABinauralFreeFieldAudioRenderer::CVABFFReceiver( m_pCore, m_oReceiverConf );
		return pReceiver;
	};

private:
	CVACoreImpl* m_pCore;
	const CVABinauralFreeFieldAudioRenderer::CVABFFReceiver::Config& m_oReceiverConf;

	//! Not for use, avoid C4512
	inline CVABFFReceiverPoolFactory operator=( const CVABFFReceiverPoolFactory& ) { VA_EXCEPT_NOT_IMPLEMENTED; };
};

class CVABFFSourcePoolFactory : public IVAPoolObjectFactory
{
public:
	CVABFFSourcePoolFactory( const CVABinauralFreeFieldAudioRenderer::CVABFFSource::Config& oConf ) : m_oSourceConf( oConf ) { };

	CVAPoolObject* CreatePoolObject( )
	{
		CVABinauralFreeFieldAudioRenderer::CVABFFSource* pSource;
		pSource = new CVABinauralFreeFieldAudioRenderer::CVABFFSource( m_oSourceConf );
		return pSource;
	};

private:
	const CVABinauralFreeFieldAudioRenderer::CVABFFSource::Config& m_oSourceConf;

	//! Not for use, avoid C4512
	inline CVABFFSourcePoolFactory operator=( const CVABFFSourcePoolFactory& ) { VA_EXCEPT_NOT_IMPLEMENTED; };
};

// Renderer

CVABinauralFreeFieldAudioRenderer::CVABinauralFreeFieldAudioRenderer( const CVAAudioRendererInitParams& oParams )
    : ITADatasourceRealization( 2, oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.dSampleRate, oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.iBuffersize )
    , CVAObject( oParams.sClass + ":" + oParams.sID )
    , m_pCore( oParams.pCore )
    , m_pCurSceneState( nullptr )
    , m_bDumpReceivers( false )
    , m_dDumpReceiversGain( 1.0 )
    , m_iHRIRFilterLength( -1 )
    , m_dAdditionalStaticDelaySeconds( 0.0f )
    , m_oParams( oParams )
{
	// read config
	Init( *oParams.pConfig );

	IVAPoolObjectFactory* pReceiverFactory = new CVABFFReceiverPoolFactory( m_pCore, m_oDefaultReceiverConf );
	m_pReceiverPool                        = IVAObjectPool::Create( 16, 2, pReceiverFactory, true );

	IVAPoolObjectFactory* pSourceFactory = new CVABFFSourcePoolFactory( m_oDefaultSourceConf );
	m_pSourcePool                        = IVAObjectPool::Create( 16, 2, pSourceFactory, true );

	m_pSoundPathFactory = new CVABFFSoundPathFactory( GetSampleRate( ), GetBlocklength( ), m_iHRIRFilterLength, 128, m_iFilterBankType );

	m_pSoundPathPool = IVAObjectPool::Create( 64, 8, m_pSoundPathFactory, true );

	m_pUpdateMessagePool = IVAObjectPool::Create( 2, 1, new CVAPoolObjectDefaultFactory<CVABFFUpdateMessage>, true );

	ctxAudio.m_sbTempL.Init( GetBlocklength( ), true );
	ctxAudio.m_sbTempR.Init( GetBlocklength( ), true );
	ctxAudio.m_iResetFlag = 0; // Normal operation mode
	ctxAudio.m_iStatus    = 0; // Stopped

	m_iCurGlobalAuralizationMode = IVAInterface::VA_AURAMODE_DEFAULT;

	// Register the renderer as a module
	oParams.pCore->RegisterModule( this );
}

CVABinauralFreeFieldAudioRenderer::~CVABinauralFreeFieldAudioRenderer( )
{
	delete m_pSoundPathPool;
	delete m_pUpdateMessagePool;
}

void CVABinauralFreeFieldAudioRenderer::Init( const CVAStruct& oArgs )
{
	CVAConfigInterpreter conf( oArgs );

	std::string sVLDInterpolationAlgorithm;
	conf.OptString( "SwitchingAlgorithm", sVLDInterpolationAlgorithm, "linear" );
	sVLDInterpolationAlgorithm = toLowercase( sVLDInterpolationAlgorithm );

	if( sVLDInterpolationAlgorithm == "switch" )
		m_iDefaultVDLSwitchingAlgorithm = EVDLAlgorithm::SWITCH;
	else if( sVLDInterpolationAlgorithm == "crossfade" )
		m_iDefaultVDLSwitchingAlgorithm = EVDLAlgorithm::CROSSFADE;
	else if( sVLDInterpolationAlgorithm == "linear" )
		m_iDefaultVDLSwitchingAlgorithm = EVDLAlgorithm::LINEAR_INTERPOLATION;
	else if( sVLDInterpolationAlgorithm == "cubicspline" )
		m_iDefaultVDLSwitchingAlgorithm = EVDLAlgorithm::CUBIC_SPLINE_INTERPOLATION;
	else if( sVLDInterpolationAlgorithm == "windowedsinc" )
		m_iDefaultVDLSwitchingAlgorithm = EVDLAlgorithm::WINDOWED_SINC_INTERPOLATION;
	else
		ITA_EXCEPT1( INVALID_PARAMETER, "Unrecognized interpolation algorithm '" + sVLDInterpolationAlgorithm + "' in BinauralFreefieldAudioRendererConfig" );

	std::string sFilterBankType;
	conf.OptString( "FilterBankType", sFilterBankType, "iir_burg_order10" );
	if( toLowercase( sFilterBankType ) == "fir_spline_linear_phase" || toLowercase( sFilterBankType ) == "fir" )
		m_iFilterBankType = CITAThirdOctaveFilterbank::FIR_SPLINE_LINEAR_PHASE;
	else if( toLowercase( sFilterBankType ) == "iir_burg_order4" )
		m_iFilterBankType = CITAThirdOctaveFilterbank::IIR_BURG_ORDER4;
	else if( toLowercase( sFilterBankType ) == "iir_burg_order10" || toLowercase( sFilterBankType ) == "iir" )
		m_iFilterBankType = CITAThirdOctaveFilterbank::IIR_BURG_ORDER10;
	else if( toLowercase( sFilterBankType ) == "iir_biquads_order10" )
		m_iFilterBankType = CITAThirdOctaveFilterbank::IIR_BIQUADS_ORDER10;
	else
		VA_EXCEPT2( INVALID_PARAMETER, "Unrecognized filter bank type '" + sFilterBankType + "' in configuration" );

	conf.OptInteger( "HRIRFilterLength", m_iHRIRFilterLength, 256 );

	conf.OptNumber( "AdditionalStaticDelaySeconds", m_dAdditionalStaticDelaySeconds, 0.0f );

	// Motion model Receiver
	conf.OptInteger( "MotionModelNumHistoryKeys", m_oDefaultReceiverConf.iMotionModelNumHistoryKeys, 1000 );

	if( m_oDefaultReceiverConf.iMotionModelNumHistoryKeys < 1 )
		VA_EXCEPT2( INVALID_PARAMETER, "Basic motion model history needs to be greater than zero" );

	conf.OptNumber( "MotionModelWindowSize", m_oDefaultReceiverConf.dMotionModelWindowSize, 0.1f );
	conf.OptNumber( "MotionModelWindowDelay", m_oDefaultReceiverConf.dMotionModelWindowDelay, 0.1f );

	if( ( m_oDefaultReceiverConf.dMotionModelWindowSize <= 0 ) || ( m_oDefaultReceiverConf.dMotionModelWindowDelay < 0 ) )
		VA_EXCEPT2( INVALID_PARAMETER, "Basic motion model window parameters parse error (zero or negative?)" );

	conf.OptBool( "MotionModelLogInputReceiver", m_oDefaultReceiverConf.bMotionModelLogInputEnabled, false );
	conf.OptBool( "MotionModelLogEstimatedOutputReceiver", m_oDefaultReceiverConf.bMotionModelLogEstimatedEnabled, false );

	// Motion model Source
	conf.OptInteger( "MotionModelNumHistoryKeys", m_oDefaultSourceConf.iMotionModelNumHistoryKeys, 1000 );

	if( m_oDefaultSourceConf.iMotionModelNumHistoryKeys < 1 )
		VA_EXCEPT2( INVALID_PARAMETER, "Basic motion model history needs to be greater than zero" );

	conf.OptNumber( "MotionModelWindowSize", m_oDefaultSourceConf.dMotionModelWindowSize, 0.1f );
	conf.OptNumber( "MotionModelWindowDelay", m_oDefaultSourceConf.dMotionModelWindowDelay, 0.1f );

	if( ( m_oDefaultSourceConf.dMotionModelWindowSize <= 0 ) || ( m_oDefaultSourceConf.dMotionModelWindowDelay < 0 ) )
		VA_EXCEPT2( INVALID_PARAMETER, "Basic motion model window parameters parse error (zero or negative?)" );

	conf.OptBool( "MotionModelLogInputSources", m_oDefaultSourceConf.bMotionModelLogInputEnabled, false );
	conf.OptBool( "MotionModelLogEstimatedOutputSources", m_oDefaultSourceConf.bMotionModelLogEstimatedEnabled, false );

	return;
}

void CVABinauralFreeFieldAudioRenderer::Reset( )
{
	VA_VERBOSE( "BinauralFreeFieldAudioRenderer", "Received reset call, indicating reset now" );
	ctxAudio.m_iResetFlag = 1; // Request reset

	if( ctxAudio.m_iStatus == 0 || m_oParams.bOfflineRendering )
	{
		VA_VERBOSE( "BinauralFreeFieldAudioRenderer", "Was not streaming, will reset manually" );
		// if no streaming active, reset manually
		// SyncInternalData();
		ResetInternalData( );
	}
	else
	{
		VA_VERBOSE( "BinauralFreeFieldAudioRenderer", "Still streaming, will now wait for reset acknownledge" );
	}

	// Wait for last streaming block before internal reset
	while( ctxAudio.m_iResetFlag != 2 )
	{
		VASleep( 10 ); // Wait for acknowledge
	}

	VA_VERBOSE( "BinauralFreeFieldAudioRenderer", "Operation reset has green light, clearing items" );

	// Iterate over sound pathes and free items
	std::list<CVABFFSoundPath*>::iterator it = m_lSoundPaths.begin( );
	while( it != m_lSoundPaths.end( ) )
	{
		CVABFFSoundPath* pPath = *it;

		int iNumRefs = pPath->GetNumReferences( );
		assert( iNumRefs == 1 );
		pPath->RemoveReference( );

		++it;
	}
	m_lSoundPaths.clear( );

	// Iterate over receiver and free items
	std::map<int, CVABFFReceiver*>::const_iterator lcit = m_mReceivers.begin( );
	while( lcit != m_mReceivers.end( ) )
	{
		CVABFFReceiver* pReceiver( lcit->second );
		pReceiver->pData->RemoveReference( );
		assert( pReceiver->GetNumReferences( ) == 1 );
		pReceiver->RemoveReference( );
		lcit++;
	}
	m_mReceivers.clear( );

	// Iterate over sources and free items
	std::map<int, CVABFFSource*>::const_iterator scit = m_mSources.begin( );
	while( scit != m_mSources.end( ) )
	{
		CVABFFSource* pSource( scit->second );
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

	ctxAudio.m_iResetFlag = 0; // Enter normal mode

	VA_VERBOSE( "BinauralFreeFieldAudioRenderer", "Reset successful" );
}

void CVABinauralFreeFieldAudioRenderer::UpdateScene( CVASceneState* pNewSceneState )
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
	m_pUpdateMessage = dynamic_cast<CVABFFUpdateMessage*>( m_pUpdateMessagePool->RequestObject( ) );

	// Quellen, Hrer und Pfade verwalten
	ManageSoundPaths( m_pCurSceneState, pNewSceneState, &oDiff );

	// Bewegungsinformationen der Quellen und Hrer aktualisieren
	UpdateTrajectories( );

	// Entitten der Schallpfade aktualisieren
	UpdateSoundPaths( );

	// Update-Nachricht an den Audiokontext schicken
	ctxAudio.m_qpUpdateMessages.push( m_pUpdateMessage );

	// Alte Szene freigeben (dereferenzieren)
	if( m_pCurSceneState )
		m_pCurSceneState->RemoveReference( );

	m_pCurSceneState = m_pNewSceneState;
	m_pNewSceneState = nullptr;
}

ITADatasource* CVABinauralFreeFieldAudioRenderer::GetOutputDatasource( )
{
	return this;
}

void CVABinauralFreeFieldAudioRenderer::ManageSoundPaths( const CVASceneState* pCurScene, const CVASceneState* pNewScene, const CVASceneStateDiff* pDiff )
{
	// Warning: take care for explicit sources and receivers for this renderer!

	// Iterate over current paths and mark deleted (will be removed within internal sync of audio context thread)
	std::list<CVABFFSoundPath*>::iterator itp = m_lSoundPaths.begin( );
	while( itp != m_lSoundPaths.end( ) )
	{
		CVABFFSoundPath* pPath( *itp );
		int iSourceID            = pPath->pSource->pData->iID;
		int iReceiverID          = pPath->pReceiver->pData->iID;
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
					bDeletetionRequired = true; // receiver removed, deletion required
					break;
				}
			}
		}

		if( bDeletetionRequired )
		{
			DeleteSoundPath( pPath );
			itp = m_lSoundPaths.erase( itp ); // Increment via erase on path list
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
		if( pSoundSourceDesc->sExplicitRendererID.empty( ) || pSoundSourceDesc->sExplicitRendererID == m_oParams.sID )
			CVABFFSource* pSource = CreateSource( iID, pNewScene->GetSoundSourceState( iID ) );
	}

	// New receivers
	citr = pDiff->viNewReceiverIDs.begin( );
	while( citr != pDiff->viNewReceiverIDs.end( ) )
	{
		const int& iID( *citr++ );

		// Only add, if no other renderer has been connected explicitly with this receiver
		const auto pReceiverDesc = m_pCore->GetSceneManager( )->GetSoundReceiverDesc( iID );
		if( pReceiverDesc->sExplicitRendererID.empty( ) || pReceiverDesc->sExplicitRendererID == m_oParams.sID )
			CVABFFReceiver* pReceiver = CreateReceiver( iID, pNewScene->GetReceiverState( iID ) );
	}

	// New paths: (1) new receivers, current sources
	citr = pDiff->viNewReceiverIDs.begin( );
	while( citr != pDiff->viNewReceiverIDs.end( ) )
	{
		int iReceiverID = ( *citr++ );

		auto it = m_mReceivers.find( iReceiverID );
		if( it == m_mReceivers.end( ) )
			continue; // Probably explicit receiver for another renderer

		CVABFFReceiver* pReceiver( it->second );

		for( size_t i = 0; i < pDiff->viComSoundSourceIDs.size( ); i++ )
		{
			// Only add, if no other renderer has been connected explicitly with this source
			// and only, if not marked for deletion
			int iSourceID                             = pDiff->viComSoundSourceIDs[i];
			std::map<int, CVABFFSource*>::iterator it = m_mSources.find( iSourceID );
			if( it == m_mSources.end( ) )
				continue; // This source is skipped by the renderer
			CVABFFSource* pSource = it->second;

			const CVASoundSourceDesc* pSoundSourceDesc = m_pCore->GetSceneManager( )->GetSoundSourceDesc( iSourceID );
			if( !pSource->bDeleted && ( pSoundSourceDesc->sExplicitRendererID.empty( ) || pSoundSourceDesc->sExplicitRendererID == m_oParams.sID ) )
				CVABFFSoundPath* pPath = CreateSoundPath( pSource, pReceiver );
		}
	}

	// New paths: (2) new sources, current receivers
	cits = pDiff->viNewSoundSourceIDs.begin( );
	while( cits != pDiff->viNewSoundSourceIDs.end( ) )
	{
		const int& iSourceID( *cits++ );
		std::map<int, CVABFFSource*>::iterator it = m_mSources.find( iSourceID );
		if( it == m_mSources.end( ) )
			continue; // Explicit source is not connected to this renderer

		CVABFFSource* pSource( it->second );
		for( size_t i = 0; i < pDiff->viComReceiverIDs.size( ); i++ )
		{
			int iReceiverID = pDiff->viComReceiverIDs[i];
			auto it         = m_mReceivers.find( iReceiverID );
			if( it != m_mReceivers.end( ) )
			{
				CVABFFReceiver* pReceiver( it->second );
				if( !pReceiver->bDeleted )
					CVABFFSoundPath* pPath = CreateSoundPath( pSource, pReceiver );
			}
		}
	}

	// New paths: (3) new sources, new receivers
	cits = pDiff->viNewSoundSourceIDs.begin( );
	while( cits != pDiff->viNewSoundSourceIDs.end( ) )
	{
		const int& iSourceID( *cits++ );
		std::map<int, CVABFFSource*>::iterator it = m_mSources.find( iSourceID );

		if( it == m_mSources.end( ) )
			continue;

		CVABFFSource* pSource( it->second );

		citr = pDiff->viNewReceiverIDs.begin( );
		while( citr != pDiff->viNewReceiverIDs.end( ) )
		{
			const int& iReceiverID( *citr++ );

			auto it = m_mReceivers.find( iReceiverID );
			if( it != m_mReceivers.end( ) )
			{
				CVABFFReceiver* pReceiver( it->second );
				CVABFFSoundPath* pPath = CreateSoundPath( pSource, pReceiver );
			}
		}
	}
}

void CVABinauralFreeFieldAudioRenderer::ProcessStream( const ITAStreamInfo* pStreamInfo )
{
	if( ctxAudio.m_iStatus == 0 )
	{
		// If streaming is active, set to 1
		ctxAudio.m_iStatus = 1;
	}

	// Schallpfade abgleichen
	SyncInternalData( );

	float* pfOutputChL = GetWritePointer( 0 );
	float* pfOutputChR = GetWritePointer( 1 );

	fm_zero( pfOutputChL, GetBlocklength( ) );
	fm_zero( pfOutputChR, GetBlocklength( ) );

	const CVAAudiostreamState* pStreamState = dynamic_cast<const CVAAudiostreamState*>( pStreamInfo );
	double dReceiverTime                    = pStreamState->dSysTime;

	// Check for reset request
	if( ctxAudio.m_iResetFlag == 1 )
	{
		VA_VERBOSE( "BinauralFreeFieldAudioRenderer", "Process stream detecting reset request, will reset internally now" );
		ResetInternalData( );

		return;
	}
	else if( ctxAudio.m_iResetFlag == 2 )
	{
		VA_VERBOSE( "BinauralFreeFieldAudioRenderer", "Process stream detecting ongoing reset, will stop processing here" );

		return;
	}

	SampleTrajectoriesInternal( dReceiverTime );

	std::list<CVABFFReceiver*>::iterator lit = ctxAudio.m_lReceivers.begin( );
	while( lit != ctxAudio.m_lReceivers.end( ) )
	{
		CVABFFReceiver* pReceiver( *( lit++ ) );
		pReceiver->psfOutput->zero( );
	}

	// Update sound pathes
	std::list<CVABFFSoundPath*>::iterator spit = ctxAudio.m_lSoundPaths.begin( );
	while( spit != ctxAudio.m_lSoundPaths.end( ) )
	{
		CVABFFSoundPath* pPath( *spit );
		CVAReceiverState* pReceiverState  = ( m_pCurSceneState ? m_pCurSceneState->GetReceiverState( pPath->pReceiver->pData->iID ) : NULL );
		CVASoundSourceState* pSourceState = ( m_pCurSceneState ? m_pCurSceneState->GetSoundSourceState( pPath->pSource->pData->iID ) : NULL );

		if( pReceiverState == nullptr || pSourceState == nullptr )
		{
			// Skip if no data is present
			spit++;
			continue;
		}

		if( !pPath->pReceiver->bValidTrajectoryPresent || !pPath->pSource->bValidTrajectoryPresent )
		{
			// Skip if no valid trajectory data is present
			spit++;
			continue;
		}

		// --= Parameter update =--

		pPath->UpdateMetrics( );

		// VDL Doppler shift settings
		bool bDPEnabledGlobal   = ( m_iCurGlobalAuralizationMode & IVAInterface::VA_AURAMODE_DOPPLER ) > 0;
		bool bDPEnabledReceiver = ( pReceiverState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_DOPPLER ) > 0;
		bool bDPEnabledSource   = ( pSourceState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_DOPPLER ) > 0;
		bool bDPEnabledCurrent  = ( pPath->pVariableDelayLineChL->GetAlgorithm( ) != EVDLAlgorithm::SWITCH ); // switch = disabled
		bool bDPStatusChanged   = ( bDPEnabledCurrent != ( bDPEnabledGlobal && bDPEnabledReceiver && bDPEnabledSource ) );
		if( bDPStatusChanged )
		{
			pPath->pVariableDelayLineChL->SetAlgorithm( !bDPEnabledCurrent ? m_iDefaultVDLSwitchingAlgorithm : EVDLAlgorithm::SWITCH );
			pPath->pVariableDelayLineChR->SetAlgorithm( !bDPEnabledCurrent ? m_iDefaultVDLSwitchingAlgorithm : EVDLAlgorithm::SWITCH );
		}
		pPath->UpdateMediumPropagation( m_pCore->oHomogeneousMedium.dSoundSpeed, m_dAdditionalStaticDelaySeconds );

		// Spherical spreading loss
		bool bSSLEnabledSource   = ( pSourceState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_SPREADING_LOSS ) > 0;
		bool bSSLEnabledReceiver = ( pReceiverState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_SPREADING_LOSS ) > 0;
		bool bSSLEnabledGlobal   = ( m_iCurGlobalAuralizationMode & IVAInterface::VA_AURAMODE_SPREADING_LOSS ) > 0;
		bool bSSLEnabled         = ( bSSLEnabledSource && bSSLEnabledReceiver && bSSLEnabledGlobal );
		double dDistanceDecrease = pPath->CalculateInverseDistanceDecrease( m_pCore->GetCoreConfig( )->dDefaultMinimumDistance );
		if( bSSLEnabled == false )
			dDistanceDecrease = m_pCore->GetCoreConfig( )->dDefaultDistance;

		bool bSDEnabledSource   = ( pSourceState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_SOURCE_DIRECTIVITY ) > 0;
		bool bSDEnabledReceiver = ( pReceiverState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_SOURCE_DIRECTIVITY ) > 0;
		bool bSDEnabledGlobal   = ( m_iCurGlobalAuralizationMode & IVAInterface::VA_AURAMODE_SOURCE_DIRECTIVITY ) > 0;
		bool bSDEnabled         = ( bSDEnabledSource && bSDEnabledReceiver && bSDEnabledGlobal );
		pPath->UpdateSourceDirectivity( bSDEnabled );

		// HATO
		const VistaQuaternion qHATO( pReceiverState->GetMotionState( )->GetHeadAboveTorsoOrientation( ).comp );
		const double dHATODeg = rad2grad( qHATO.GetAngles( ).b );
		pPath->UpdateHRIR( pReceiverState->GetAnthropometricData( ), dHATODeg );

		// Sound source gain / direct sound audibility via AuraMode flags
		bool bDSSourceStatusEnabled   = ( pSourceState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_DIRECT_SOUND );
		bool bDSReceiverStatusEnabled = ( pReceiverState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_DIRECT_SOUND );
		bool bDSGlobalStatusEnabled   = ( m_iCurGlobalAuralizationMode & IVAInterface::VA_AURAMODE_DIRECT_SOUND );
		bool bDSEnabled               = bDSSourceStatusEnabled && bDSReceiverStatusEnabled && bDSGlobalStatusEnabled;

		float fSoundSourceGain = float( dDistanceDecrease * pSourceState->GetVolume( m_oParams.pCore->GetCoreConfig( )->dDefaultAmplitudeCalibration ) );
		if( pPath->pSource->pData->bMuted || ( bDSEnabled == false ) )
			fSoundSourceGain = 0.0f;
		pPath->pFIRConvolverChL->SetGain( fSoundSourceGain );
		pPath->pFIRConvolverChR->SetGain( fSoundSourceGain );

		// --= DSP =--

		CVASoundSourceDesc* pSourceData = pPath->pSource->pData;
		const ITASampleFrame& sfInputBuffer( *pSourceData->psfSignalSourceInputBuf );
		const ITASampleBuffer* psbInput( &( sfInputBuffer[0] ) );
		assert( psbInput );              // Knallt es hier, dann ist die Eingabequelle noch nicht gesetzt
		assert( pSourceData->iID >= 0 ); // Knallt es hier, dann wurde dem SoundPath unterm hintern die Quelle entzogen! -> Problem mit Referenzierung und Reset?

		const float* pfInput = psbInput->GetData( );
		assert( pfInput );

		pPath->pThirdOctaveFilterBank->Process( pfInput, ctxAudio.m_sbTempL.data( ) );
		pPath->pVariableDelayLineChR->Process( &( ctxAudio.m_sbTempL ), &( ctxAudio.m_sbTempR ) );
		pPath->pVariableDelayLineChL->Process( &( ctxAudio.m_sbTempL ), &( ctxAudio.m_sbTempL ) ); // inplace
		pPath->pFIRConvolverChL->Process( ctxAudio.m_sbTempL.data( ), ( *pPath->pReceiver->psfOutput )[0].data( ), ITABase::MixingMethod::ADD );
		pPath->pFIRConvolverChR->Process( ctxAudio.m_sbTempR.data( ), ( *pPath->pReceiver->psfOutput )[1].data( ), ITABase::MixingMethod::ADD );

		spit++;
	}

	// Mix receiver outputs into rendering output
	for( CVABFFReceiver* pReceiver: ctxAudio.m_lReceivers )
	{
		if( !pReceiver->pData->bMuted )
		{
			fm_add( pfOutputChL, ( *pReceiver->psfOutput )[0].data( ), m_uiBlocklength );
			fm_add( pfOutputChR, ( *pReceiver->psfOutput )[1].data( ), m_uiBlocklength );
		}
	}

	// Receiver dumping (if active)
	if( m_iDumpReceiversFlag > 0 )
	{
		for( auto it: m_mReceivers )
		{
			CVABFFReceiver* pReceiver = it.second;
			pReceiver->psfOutput->mul_scalar( float( m_dDumpReceiversGain ) );
			pReceiver->pReceiverOutputAudioFileWriter->write( pReceiver->psfOutput );
		}

		// Ack on dump stop
		if( m_iDumpReceiversFlag == 2 )
			m_iDumpReceiversFlag = 0;
	}

	IncrementWritePointer( );

	return;
}

void CVABinauralFreeFieldAudioRenderer::UpdateTrajectories( )
{
	// Neue Quellendaten bernehmen
	for( std::map<int, CVABFFSource*>::iterator it = m_mSources.begin( ); it != m_mSources.end( ); ++it )
	{
		int iSourceID         = it->first;
		CVABFFSource* pSource = it->second;

		CVASoundSourceState* pSourceCur = ( m_pCurSceneState ? m_pCurSceneState->GetSoundSourceState( iSourceID ) : nullptr );
		CVASoundSourceState* pSourceNew = ( m_pNewSceneState ? m_pNewSceneState->GetSoundSourceState( iSourceID ) : nullptr );

		const CVAMotionState* pMotionCur = ( pSourceCur ? pSourceCur->GetMotionState( ) : nullptr );
		const CVAMotionState* pMotionNew = ( pSourceNew ? pSourceNew->GetMotionState( ) : nullptr );

		if( pMotionNew && ( pMotionNew != pMotionCur ) )
		{
			VA_TRACE( "BinauralFreeFieldAudioRenderer", "Source " << iSourceID << " new motion state" );
			pSource->pMotionModel->InputMotionKey( pMotionNew );
		}
	}

	// Neue Hrerdaten bernehmen
	for( std::map<int, CVABFFReceiver*>::iterator it = m_mReceivers.begin( ); it != m_mReceivers.end( ); ++it )
	{
		int iReceiverID           = it->first;
		CVABFFReceiver* pReceiver = it->second;

		CVAReceiverState* pReceiverCur = ( m_pCurSceneState ? m_pCurSceneState->GetReceiverState( iReceiverID ) : nullptr );
		CVAReceiverState* pReceiverNew = ( m_pNewSceneState ? m_pNewSceneState->GetReceiverState( iReceiverID ) : nullptr );

		const CVAMotionState* pMotionCur = ( pReceiverCur ? pReceiverCur->GetMotionState( ) : nullptr );
		const CVAMotionState* pMotionNew = ( pReceiverNew ? pReceiverNew->GetMotionState( ) : nullptr );

		if( pMotionNew && ( pMotionNew != pMotionCur ) )
		{
			VA_TRACE( "BinauralFreeFieldAudioRenderer", "Receiver " << iReceiverID << " new position " ); // << *pMotionNew);
			pReceiver->pMotionModel->InputMotionKey( pMotionNew );
		}
	}
}

void CVABinauralFreeFieldAudioRenderer::SampleTrajectoriesInternal( double dTime )
{
	bool bValid = true;
	for( std::list<CVABFFSource*>::iterator it = ctxAudio.m_lSources.begin( ); it != ctxAudio.m_lSources.end( ); ++it )
	{
		CVABFFSource* pSource = *it;

		pSource->pMotionModel->HandleMotionKeys( );
		bValid &= pSource->pMotionModel->EstimatePosition( dTime, pSource->vPredPos );
		bValid &= pSource->pMotionModel->EstimateOrientation( dTime, pSource->vPredView, pSource->vPredUp );
		pSource->bValidTrajectoryPresent = bValid;
	}

	bValid = true;
	for( std::list<CVABFFReceiver*>::iterator it = ctxAudio.m_lReceivers.begin( ); it != ctxAudio.m_lReceivers.end( ); ++it )
	{
		CVABFFReceiver* pReceiver = *it;

		pReceiver->pMotionModel->HandleMotionKeys( );
		bValid &= pReceiver->pMotionModel->EstimatePosition( dTime, pReceiver->vPredPos );
		bValid &= pReceiver->pMotionModel->EstimateOrientation( dTime, pReceiver->vPredView, pReceiver->vPredUp );
		pReceiver->bValidTrajectoryPresent = bValid;
	}
}

CVABFFSoundPath* CVABinauralFreeFieldAudioRenderer::CreateSoundPath( CVABFFSource* pSource, CVABFFReceiver* pReceiver )
{
	int iSourceID   = pSource->pData->iID;
	int iReceiverID = pReceiver->pData->iID;

	assert( !pSource->bDeleted && !pReceiver->bDeleted );

	VA_VERBOSE( "BinauralFreeFieldAudioRenderer", "Creating sound path from source " << iSourceID << " -> receiver " << iReceiverID );

	CVABFFSoundPath* pPath = dynamic_cast<CVABFFSoundPath*>( m_pSoundPathPool->RequestObject( ) );

	pPath->pSource   = pSource;
	pPath->pReceiver = pReceiver;

	pPath->bDelete = false;

	CVASoundSourceState* pSourceNew = ( m_pNewSceneState ? m_pNewSceneState->GetSoundSourceState( iSourceID ) : nullptr );
	if( pSourceNew != nullptr )
		pPath->oDirectivityStateNew.pData = (IVADirectivity*)pSourceNew->GetDirectivityData( );

	CVAReceiverState* pReceiverNew = ( m_pNewSceneState ? m_pNewSceneState->GetReceiverState( iReceiverID ) : nullptr );
	if( pReceiverNew != nullptr )
		pPath->oHRIRStateNew.pData = (IVADirectivity*)pReceiverNew->GetDirectivity( );

	// Configure DSP elements
	pPath->pVariableDelayLineChL->SetAlgorithm( m_iDefaultVDLSwitchingAlgorithm );
	pPath->pVariableDelayLineChR->SetAlgorithm( m_iDefaultVDLSwitchingAlgorithm );


	m_lSoundPaths.push_back( pPath );
	m_pUpdateMessage->vNewPaths.push_back( pPath );

	return pPath;
}

void CVABinauralFreeFieldAudioRenderer::DeleteSoundPath( CVABFFSoundPath* pPath )
{
	VA_VERBOSE( "BinauralFreeFieldAudioRenderer",
	            "Marking sound path from source " << pPath->pSource->pData->iID << " -> receiver " << pPath->pReceiver->pData->iID << " for deletion" );

	pPath->bDelete = true;
	pPath->RemoveReference( );
	m_pUpdateMessage->vDelPaths.push_back( pPath );
}

CVABinauralFreeFieldAudioRenderer::CVABFFReceiver* CVABinauralFreeFieldAudioRenderer::CreateReceiver( const int iID, const CVAReceiverState* pReceiverState )
{
	VA_VERBOSE( "BinauralFreeFieldAudioRenderer", "Creating receiver with ID " << iID );

	CVABFFReceiver* pReceiver = dynamic_cast<CVABFFReceiver*>( m_pReceiverPool->RequestObject( ) ); // Reference = 1

	pReceiver->pData = m_pCore->GetSceneManager( )->GetSoundReceiverDesc( iID );
	pReceiver->pData->AddReference( );

	// Move to prerequest of pool?
	pReceiver->psfOutput = new ITASampleFrame( 2, m_pCore->GetCoreConfig( )->oAudioDriverConfig.iBuffersize, true );
	assert( pReceiver->pData );
	pReceiver->bDeleted = false;

	// Motion model
	CVABasicMotionModel* pMotionInstance = dynamic_cast<CVABasicMotionModel*>( pReceiver->pMotionModel->GetInstance( ) );
	pMotionInstance->SetName( std::string( "bfrend_mm_listener_" + pReceiver->pData->sName ) );
	pMotionInstance->Reset( );

	m_mReceivers.insert( std::pair<int, CVABinauralFreeFieldAudioRenderer::CVABFFReceiver*>( iID, pReceiver ) );

	m_pUpdateMessage->vNewReceivers.push_back( pReceiver );

	return pReceiver;
}

void CVABinauralFreeFieldAudioRenderer::DeleteReceiver( int iReceiverID )
{
	VA_VERBOSE( "BinauralFreeFieldAudioRenderer", "Marking receiver with ID " << iReceiverID << " for removal" );
	std::map<int, CVABFFReceiver*>::iterator it = m_mReceivers.find( iReceiverID );

	if( it != m_mReceivers.end( ) )
	{
		CVABFFReceiver* pReceiver = it->second;
		m_mReceivers.erase( it );
		pReceiver->bDeleted = true;
		pReceiver->pData->RemoveReference( );
		pReceiver->RemoveReference( );

		m_pUpdateMessage->vDelReceivers.push_back( pReceiver );
	}
}

CVABinauralFreeFieldAudioRenderer::CVABFFSource* CVABinauralFreeFieldAudioRenderer::CreateSource( int iID, const CVASoundSourceState* pSourceState )
{
	VA_VERBOSE( "BinauralFreeFieldAudioRenderer", "Creating source with ID " << iID );
	CVABFFSource* pSource = dynamic_cast<CVABFFSource*>( m_pSourcePool->RequestObject( ) );

	pSource->pData = m_pCore->GetSceneManager( )->GetSoundSourceDesc( iID );
	pSource->pData->AddReference( );

	pSource->bDeleted = false;

	CVABasicMotionModel* pMotionInstance = dynamic_cast<CVABasicMotionModel*>( pSource->pMotionModel->GetInstance( ) );
	pMotionInstance->SetName( std::string( "bfrend_mm_source_" + pSource->pData->sName ) );
	pMotionInstance->Reset( );

	m_mSources.insert( std::pair<int, CVABFFSource*>( iID, pSource ) );

	m_pUpdateMessage->vNewSources.push_back( pSource );

	return pSource;
}

void CVABinauralFreeFieldAudioRenderer::DeleteSource( int iSourceID )
{
	VA_VERBOSE( "BinauralFreeFieldAudioRenderer", "Marking source with ID " << iSourceID << " for removal" );
	std::map<int, CVABFFSource*>::iterator it = m_mSources.find( iSourceID );

	if( it != m_mSources.end( ) )
	{
		CVABFFSource* pSource = it->second;
		m_mSources.erase( it );
		pSource->bDeleted = true;
		pSource->pData->RemoveReference( );
		pSource->RemoveReference( );
	}
}

void CVABinauralFreeFieldAudioRenderer::SyncInternalData( )
{
	CVABFFUpdateMessage* pUpdate;
	while( ctxAudio.m_qpUpdateMessages.try_pop( pUpdate ) )
	{
		std::list<CVABFFSoundPath*>::const_iterator citp = pUpdate->vDelPaths.begin( );
		while( citp != pUpdate->vDelPaths.end( ) )
		{
			CVABFFSoundPath* pPath( *citp++ );
			ctxAudio.m_lSoundPaths.remove( pPath );
			pPath->RemoveReference( );
		}

		citp = pUpdate->vNewPaths.begin( );
		while( citp != pUpdate->vNewPaths.end( ) )
		{
			CVABFFSoundPath* pPath( *citp++ );
			pPath->AddReference( );
			ctxAudio.m_lSoundPaths.push_back( pPath );
		}

		std::list<CVABFFSource*>::const_iterator cits = pUpdate->vDelSources.begin( );
		while( cits != pUpdate->vDelSources.end( ) )
		{
			CVABFFSource* pSource( *cits++ );
			ctxAudio.m_lSources.remove( pSource );
			pSource->pData->RemoveReference( );
			pSource->RemoveReference( );
		}

		cits = pUpdate->vNewSources.begin( );
		while( cits != pUpdate->vNewSources.end( ) )
		{
			CVABFFSource* pSource( *cits++ );
			pSource->AddReference( );
			pSource->pData->AddReference( );
			ctxAudio.m_lSources.push_back( pSource );
		}

		std::list<CVABFFReceiver*>::const_iterator citl = pUpdate->vDelReceivers.begin( );
		while( citl != pUpdate->vDelReceivers.end( ) )
		{
			CVABFFReceiver* pReceiver( *citl++ );
			ctxAudio.m_lReceivers.remove( pReceiver );
			pReceiver->pData->RemoveReference( );
			pReceiver->RemoveReference( );
		}

		citl = pUpdate->vNewReceivers.begin( );
		while( citl != pUpdate->vNewReceivers.end( ) )
		{
			CVABFFReceiver* pReceiver( *citl++ );
			pReceiver->AddReference( );
			pReceiver->pData->AddReference( );
			ctxAudio.m_lReceivers.push_back( pReceiver );
		}

		pUpdate->RemoveReference( );
	}

	return;
}

void CVABinauralFreeFieldAudioRenderer::ResetInternalData( )
{
	VA_VERBOSE( "BinauralFreeFieldAudioRenderer", "Resetting internally" );

	std::list<CVABFFSoundPath*>::const_iterator citp = ctxAudio.m_lSoundPaths.begin( );
	while( citp != ctxAudio.m_lSoundPaths.end( ) )
	{
		CVABFFSoundPath* pPath( *citp++ );
		pPath->RemoveReference( );
	}
	ctxAudio.m_lSoundPaths.clear( );

	std::list<CVABFFReceiver*>::const_iterator itl = ctxAudio.m_lReceivers.begin( );
	while( itl != ctxAudio.m_lReceivers.end( ) )
	{
		CVABFFReceiver* pReceiver( *itl++ );
		pReceiver->pData->RemoveReference( );
		pReceiver->RemoveReference( );
	}
	ctxAudio.m_lReceivers.clear( );

	std::list<CVABFFSource*>::const_iterator cits = ctxAudio.m_lSources.begin( );
	while( cits != ctxAudio.m_lSources.end( ) )
	{
		CVABFFSource* pSource( *cits++ );
		pSource->pData->RemoveReference( );
		pSource->RemoveReference( );
	}
	ctxAudio.m_lSources.clear( );

	ctxAudio.m_iResetFlag = 2; // set ack
}

void CVABinauralFreeFieldAudioRenderer::UpdateSoundPaths( )
{
	// Check for new data
	std::list<CVABFFSoundPath*>::iterator it = m_lSoundPaths.begin( );
	while( it != m_lSoundPaths.end( ) )
	{
		CVABFFSoundPath* pPath( *it++ );

		CVABFFSource* pSource     = pPath->pSource;
		CVABFFReceiver* pReceiver = pPath->pReceiver;

		int iSourceID   = pSource->pData->iID;
		int iReceiverID = pReceiver->pData->iID;

		CVASoundSourceState* pSourceCur = ( m_pCurSceneState ? m_pCurSceneState->GetSoundSourceState( iSourceID ) : nullptr );
		CVASoundSourceState* pSourceNew = ( m_pNewSceneState ? m_pNewSceneState->GetSoundSourceState( iSourceID ) : nullptr );

		CVAReceiverState* pReceiverCur = ( m_pCurSceneState ? m_pCurSceneState->GetReceiverState( iReceiverID ) : nullptr );
		CVAReceiverState* pReceiverNew = ( m_pNewSceneState ? m_pNewSceneState->GetReceiverState( iReceiverID ) : nullptr );

		if( pSourceNew == nullptr )
		{
			pPath->oDirectivityStateNew.pData = nullptr;
		}
		else
		{
			pPath->oDirectivityStateNew.pData = (IVADirectivity*)pSourceNew->GetDirectivityData( );
		}

		if( pReceiverNew == nullptr )
		{
			pPath->oHRIRStateNew.pData = nullptr;
		}
		else
		{
			pPath->oHRIRStateNew.pData = (IVADirectivity*)pReceiverNew->GetDirectivity( );
		}
	}

	return;
}

void CVABinauralFreeFieldAudioRenderer::UpdateGlobalAuralizationMode( int iGlobalAuralizationMode )
{
	if( m_iCurGlobalAuralizationMode == iGlobalAuralizationMode )
		return;

	m_iCurGlobalAuralizationMode = iGlobalAuralizationMode;

	return;
}


// Class CVABFFSoundPath

CVABFFSoundPath::CVABFFSoundPath( double dSamplerate, int iBlocklength, int iHRIRFilterLength, int iDirFilterLength, int iFilterBankType )
{
	pThirdOctaveFilterBank = CITAThirdOctaveFilterbank::Create( dSamplerate, iBlocklength, iFilterBankType );
	pThirdOctaveFilterBank->SetIdentity( );

	float fReserverdMaxDelaySamples = (float)( 3 * dSamplerate ); // 3 Sekunden ~ 1km Entfernung
	m_iDefaultVDLSwitchingAlgorithm = EVDLAlgorithm::CUBIC_SPLINE_INTERPOLATION;
	pVariableDelayLineChL           = new CITAVariableDelayLine( dSamplerate, iBlocklength, fReserverdMaxDelaySamples, m_iDefaultVDLSwitchingAlgorithm );
	pVariableDelayLineChR           = new CITAVariableDelayLine( dSamplerate, iBlocklength, fReserverdMaxDelaySamples, m_iDefaultVDLSwitchingAlgorithm );

	pFIRConvolverChL = new ITAUPConvolution( iBlocklength, iHRIRFilterLength );
	pFIRConvolverChL->SetFilterExchangeFadingFunction( ITABase::FadingFunction::COSINE_SQUARE );
	pFIRConvolverChL->SetFilterCrossfadeLength( (std::min)( iBlocklength, 32 ) );
	pFIRConvolverChL->SetGain( 0.0f, true );
	ITAUPFilter* pHRIRFilterChL = pFIRConvolverChL->RequestFilter( );
	pHRIRFilterChL->identity( );
	pFIRConvolverChL->ExchangeFilter( pHRIRFilterChL );

	pFIRConvolverChR = new ITAUPConvolution( iBlocklength, iHRIRFilterLength );
	pFIRConvolverChR->SetFilterExchangeFadingFunction( ITABase::FadingFunction::COSINE_SQUARE );
	pFIRConvolverChR->SetFilterCrossfadeLength( (std::min)( iBlocklength, 32 ) );
	pFIRConvolverChR->SetGain( 0.0f, true );
	ITAUPFilter* pHRIRFilterChR = pFIRConvolverChR->RequestFilter( );
	pHRIRFilterChR->identity( );
	pFIRConvolverChL->ExchangeFilter( pHRIRFilterChR );

	// Auto-release filter after it is not used anymore
	pFIRConvolverChL->ReleaseFilter( pHRIRFilterChL );
	pFIRConvolverChR->ReleaseFilter( pHRIRFilterChR );

	m_sfHRIRTemp.init( 2, iHRIRFilterLength, false );
}

CVABFFSoundPath::~CVABFFSoundPath( )
{
	delete pThirdOctaveFilterBank;
	delete pVariableDelayLineChL;
	delete pVariableDelayLineChR;
	delete pFIRConvolverChL;
	delete pFIRConvolverChR;
}

void CVABFFSoundPath::UpdateMetrics( )
{
	if( pSource->vPredPos != pReceiver->vPredPos )
		oRelations.Calc( pSource->vPredPos, pSource->vPredView, pSource->vPredUp, pReceiver->vPredPos, pReceiver->vPredView, pReceiver->vPredUp );
}


void CVABFFSoundPath::UpdateSourceDirectivity( bool bSDAuraModeEnabled )
{
	if( bSDAuraModeEnabled == true )
	{
		CVADirectivityDAFFEnergetic* pDirectivityDataNew = (CVADirectivityDAFFEnergetic*)oDirectivityStateNew.pData;
		CVADirectivityDAFFEnergetic* pDirectivityDataCur = (CVADirectivityDAFFEnergetic*)oDirectivityStateCur.pData;

		oDirectivityStateNew.bDirectivityEnabled = true;

		if( pDirectivityDataNew == nullptr )
		{
			// Directivity removed
			if( pDirectivityDataCur != nullptr )
			{
				pThirdOctaveFilterBank->SetIdentity( ); // set identity once
				oDirectivityStateNew.iRecord = -1;
			}
		}
		else
		{
			// Update directivity data set
			pDirectivityDataNew->GetDAFFContent( )->getNearestNeighbour( DAFF_OBJECT_VIEW, float( oRelations.dAzimuthS2T ), float( oRelations.dElevationS2T ),
			                                                             oDirectivityStateNew.iRecord );
			if( oDirectivityStateCur.iRecord != oDirectivityStateNew.iRecord || pDirectivityDataCur != pDirectivityDataNew )
			{
				std::vector<float> vfMags( ITABase::CThirdOctaveMagnitudeSpectrum::GetNumBands( ) );
				pDirectivityDataNew->GetDAFFContent( )->getMagnitudes( oDirectivityStateNew.iRecord, 0, &vfMags[0] );
				ITABase::CThirdOctaveGainMagnitudeSpectrum oDirectivityMagnitudes;
				oDirectivityMagnitudes.SetMagnitudes( vfMags );
				pThirdOctaveFilterBank->SetMagnitudes( oDirectivityMagnitudes );
			}
		}
	}
	else
	{
		if( oDirectivityStateCur.bDirectivityEnabled == true )
		{
			// Switch to disabled DIR
			pThirdOctaveFilterBank->SetIdentity( );
			oDirectivityStateNew.bDirectivityEnabled = false;
			oDirectivityStateNew.iRecord             = -1;
		}
	}

	// Acknowledge new state
	oDirectivityStateCur = oDirectivityStateNew;

	return;
}

void CVABFFSoundPath::UpdateMediumPropagation( double dSpeedOfSound, double dAdditionalStaticDelaySeconds )
{
	assert( dSpeedOfSound > 0 );

	double dDistanceDelay = oRelations.dDistance / dSpeedOfSound;
	double dDelay         = (std::max)( double( 0.0f ), ( dDistanceDelay + dAdditionalStaticDelaySeconds ) );
	pVariableDelayLineChL->SetDelayTime( float( dDelay ) );
	pVariableDelayLineChR->SetDelayTime( float( dDelay ) );
}

double CVABFFSoundPath::CalculateInverseDistanceDecrease( const double dMinimumDistance ) const
{
	double dDistance = (std::max)( (double)oRelations.dDistance, dMinimumDistance );
	assert( dDistance > 0.0f );
	double fInverseDistanceDecrease = 1.0f / dDistance;
	return fInverseDistanceDecrease;
}

void CVABFFSoundPath::UpdateHRIR( const CVAReceiverState::CVAAnthropometricParameter& oAnthroParameters, const double dHATODeg )
{
	double dITDCorrectionShift = .0f;
	IVADirectivity* pCurHRIR   = oHRIRStateCur.pData;
	IVADirectivity* pNewHRIR   = oHRIRStateNew.pData;

	// No new directivity
	if( pNewHRIR == nullptr )
	{
		if( pCurHRIR )
		{
			// Directivity has been removed, set zero filters once

			ITAUPFilter* pHRIRFilterChL = pFIRConvolverChL->GetFilterPool( )->RequestFilter( );
			ITAUPFilter* pHRIRFilterChR = pFIRConvolverChR->GetFilterPool( )->RequestFilter( );
			pHRIRFilterChL->Zeros( );
			pHRIRFilterChR->Zeros( );

			pFIRConvolverChL->ExchangeFilter( pHRIRFilterChL );
			pFIRConvolverChR->ExchangeFilter( pHRIRFilterChR );

			pFIRConvolverChL->ReleaseFilter( pHRIRFilterChL );
			pFIRConvolverChR->ReleaseFilter( pHRIRFilterChR );
		}
		return;
	}

	switch( pNewHRIR->GetType( ) )
	{
		case IVADirectivity::DAFF_HATO_HRIR:
		{
			CVADirectivityDAFFHATOHRIR* pHATOHRIR = (CVADirectivityDAFFHATOHRIR*)pNewHRIR;
			oHRIRStateNew.dHATODeg                = dHATODeg; // Overwrite current HATO

			// Quick check if nearest neighbour is equal
			bool bForceUpdate = false;
			if( pHATOHRIR->GetProperties( )->bSpaceDiscrete )
				pHATOHRIR->GetNearestSpatialNeighbourIndex( float( oRelations.dAzimuthT2S ), float( oRelations.dElevationT2S ), &oHRIRStateNew.iRecord );
			else
				bForceUpdate = true; // Update always required, if non-discrete data

			if( ( oHRIRStateCur != oHRIRStateNew ) || bForceUpdate )
			{
				int iNewFilterLength = pHATOHRIR->GetProperties( )->iFilterLength;
				if( m_sfHRIRTemp.length( ) != iNewFilterLength )
				{
					m_sfHRIRTemp.init( 2, iNewFilterLength, false );
				}

				if( iNewFilterLength > pFIRConvolverChL->GetMaxFilterlength( ) )
				{
					VA_WARN( "BFFSoundPath", "HRIR too long for convolver, cropping. Increase HRIR filter length in BinauralFreefieldAudioRenderer configuration." );
					iNewFilterLength = pFIRConvolverChL->GetMaxFilterlength( );
				}

				if( pHATOHRIR->GetProperties( )->bSpaceDiscrete )
				{
					pHATOHRIR->GetHRIRByIndexAndHATO( &m_sfHRIRTemp, oHRIRStateNew.iRecord, float( dHATODeg ) );
					oHRIRStateNew.fDistance = float( oRelations.dDistance );
				}
				else
				{
					int iRecordIndex = -1;
					pHATOHRIR->GetNearestSpatialNeighbourIndex( float( oRelations.dAzimuthT2S ), float( oRelations.dElevationT2S ), &iRecordIndex );
					pHATOHRIR->GetHRIRByIndexAndHATO( &m_sfHRIRTemp, iRecordIndex, dHATODeg );
				}


				// Update DSP element

				ITAUPFilter* pHRIRFilterChL = pFIRConvolverChL->GetFilterPool( )->RequestFilter( );
				ITAUPFilter* pHRIRFilterChR = pFIRConvolverChR->GetFilterPool( )->RequestFilter( );

				pHRIRFilterChL->Load( m_sfHRIRTemp[0].data( ), iNewFilterLength );
				pHRIRFilterChR->Load( m_sfHRIRTemp[1].data( ), iNewFilterLength );

				pFIRConvolverChL->ExchangeFilter( pHRIRFilterChL );
				pFIRConvolverChR->ExchangeFilter( pHRIRFilterChR );
				pFIRConvolverChL->ReleaseFilter( pHRIRFilterChL );
				pFIRConvolverChR->ReleaseFilter( pHRIRFilterChR );

				// Ack
				oHRIRStateCur = oHRIRStateNew;
			}

			// Calculate individualized ITD based on anthropometric data
			// Source: ???
			double daw          = -8.7231e-4;
			double dbw          = 0.0029;
			double dah          = -3.942e-4;
			double dbh          = 6.0476e-4;
			double dad          = 4.2308e-4;
			double dbd          = oAnthroParameters.GetHeadDepthParameterFromLUT( oAnthroParameters.dHeadDepth );
			double dDeltaWidth  = oAnthroParameters.dHeadWidth - pHATOHRIR->GetProperties( )->oAnthroParams.dHeadWidth;
			double dDeltaHeight = oAnthroParameters.dHeadHeight - pHATOHRIR->GetProperties( )->oAnthroParams.dHeadHeight;
			double dDeltaDepth  = oAnthroParameters.dHeadDepth - pHATOHRIR->GetProperties( )->oAnthroParams.dHeadDepth;
			double dPhiRad      = oRelations.dAzimuthT2S * ITAConstants::PI_F / 180.0;
			double dThetaRad    = -( oRelations.dElevationT2S * ITAConstants::PI_F / 180.0 - ITAConstants::PI_F / 2.0f );
			double dAmplitude =
			    dPhiRad * ( dDeltaWidth * ( dThetaRad * dThetaRad * daw + dThetaRad * dbw ) + dDeltaHeight * ( dThetaRad * dThetaRad * dah + dThetaRad * dbh ) +
			                dDeltaDepth * ( dThetaRad * dThetaRad * dad + dThetaRad * dbd ) );
			dITDCorrectionShift = std::sin( dPhiRad ) * dAmplitude;
		}

		case IVADirectivity::DAFF_HRIR:
		{
			CVADirectivityDAFFHRIR* pHRIRDataNew = (CVADirectivityDAFFHRIR*)pNewHRIR;
			bool bForeUpdate                     = false;
			if( pHRIRDataNew != nullptr )
			{
				// Quick check if nearest neighbour is equal
				if( pHRIRDataNew->GetProperties( )->bSpaceDiscrete )
					pHRIRDataNew->GetNearestNeighbour( float( oRelations.dAzimuthT2S ), float( oRelations.dElevationT2S ), &oHRIRStateNew.iRecord );
				else
					bForeUpdate = true; // Update always required, if non-discrete data
			}

			if( ( oHRIRStateCur != oHRIRStateNew ) || bForeUpdate )
			{
				ITAUPFilter* pHRIRFilterChL = pFIRConvolverChL->GetFilterPool( )->RequestFilter( );
				ITAUPFilter* pHRIRFilterChR = pFIRConvolverChR->GetFilterPool( )->RequestFilter( );

				if( pHRIRDataNew == nullptr )
				{
					pHRIRFilterChL->Zeros( );
					pHRIRFilterChR->Zeros( );
				}
				else
				{
					int iNewFilterLength = pHRIRDataNew->GetProperties( )->iFilterLength;
					if( m_sfHRIRTemp.length( ) != iNewFilterLength )
					{
						m_sfHRIRTemp.init( 2, iNewFilterLength, false );
					}

					if( iNewFilterLength > pFIRConvolverChL->GetMaxFilterlength( ) )
					{
						VA_WARN( "BFFSoundPath", "HRIR too long for convolver, cropping. Increase HRIR filter length in BinauralFreefieldAudioRenderer configuration." );
						iNewFilterLength = pFIRConvolverChL->GetMaxFilterlength( );
					}

					if( pHRIRDataNew->GetProperties( )->bSpaceDiscrete )
					{
						pHRIRDataNew->GetHRIRByIndex( &m_sfHRIRTemp, oHRIRStateNew.iRecord, float( oRelations.dDistance ) );
						oHRIRStateNew.fDistance = float( oRelations.dDistance );
					}
					else
					{
						pHRIRDataNew->GetHRIR( &m_sfHRIRTemp, float( oRelations.dAzimuthT2S ), float( oRelations.dElevationT2S ), float( oRelations.dDistance ) );
					}

					pHRIRFilterChL->Load( m_sfHRIRTemp[0].data( ), iNewFilterLength );
					pHRIRFilterChR->Load( m_sfHRIRTemp[1].data( ), iNewFilterLength );
				}


				pFIRConvolverChL->ExchangeFilter( pHRIRFilterChL );
				pFIRConvolverChR->ExchangeFilter( pHRIRFilterChR );
				pFIRConvolverChL->ReleaseFilter( pHRIRFilterChL );
				pFIRConvolverChR->ReleaseFilter( pHRIRFilterChR );

				// Ack
				oHRIRStateCur = oHRIRStateNew;
			}

			if( pHRIRDataNew )
			{
				// Calculate individualized ITD based on anthropometric data
				// Source: ???
				double daw          = -8.7231e-4;
				double dbw          = 0.0029;
				double dah          = -3.942e-4;
				double dbh          = 6.0476e-4;
				double dad          = 4.2308e-4;
				double dbd          = oAnthroParameters.GetHeadDepthParameterFromLUT( oAnthroParameters.dHeadDepth );
				double dDeltaWidth  = oAnthroParameters.dHeadWidth - pHRIRDataNew->GetProperties( )->oAnthroParams.dHeadWidth;
				double dDeltaHeight = oAnthroParameters.dHeadHeight - pHRIRDataNew->GetProperties( )->oAnthroParams.dHeadHeight;
				double dDeltaDepth  = oAnthroParameters.dHeadDepth - pHRIRDataNew->GetProperties( )->oAnthroParams.dHeadDepth;
				double dPhiRad      = oRelations.dAzimuthT2S * ITAConstants::PI_F / 180.0;
				double dThetaRad    = -( oRelations.dElevationT2S * ITAConstants::PI_F / 180.0 - ITAConstants::PI_F / 2.0f );
				double dAmplitude =
				    dPhiRad * ( dDeltaWidth * ( dThetaRad * dThetaRad * daw + dThetaRad * dbw ) + dDeltaHeight * ( dThetaRad * dThetaRad * dah + dThetaRad * dbh ) +
				                dDeltaDepth * ( dThetaRad * dThetaRad * dad + dThetaRad * dbd ) );
				dITDCorrectionShift = std::sin( dPhiRad ) * dAmplitude;
			}
		}
	}

	// Apply individualized delay
	const float fPreviousDelayTimeL = pVariableDelayLineChL->GetNewDelayTime( );
	const float fPreviousDelayTimeR = pVariableDelayLineChR->GetNewDelayTime( );
	pVariableDelayLineChL->SetDelayTime( fPreviousDelayTimeL - float( dITDCorrectionShift ) );
	pVariableDelayLineChR->SetDelayTime( fPreviousDelayTimeR + float( dITDCorrectionShift ) );

	return;
}

CVAStruct CVABinauralFreeFieldAudioRenderer::CallObject( const CVAStruct& oArgs )
{
	CVAStruct oReturn;
	const CVAStructValue* pStruct;

	if( ( pStruct = oArgs.GetValue( "AdditionalStaticDelaySeconds" ) ) != nullptr )
	{
		if( pStruct->GetDatatype( ) != CVAStructValue::DOUBLE )
			VA_EXCEPT2( INVALID_PARAMETER, "Additional delay must be a double" );

		oReturn["CurrentAdditionalStaticDelaySeconds"] = m_dAdditionalStaticDelaySeconds;
		m_dAdditionalStaticDelaySeconds                = *pStruct;
		oReturn["NewAdditionalStaticDelaySeconds"]     = m_dAdditionalStaticDelaySeconds;

		return oReturn;
	}

	CVAConfigInterpreter oConfig( oArgs );
	std::string sCommandOrg;
	oConfig.OptString( "Command", sCommandOrg );
	std::string sCommand = toUppercase( sCommandOrg );

	// Command resolution
	if( sCommand == "STARTDUMpReceiverS" )
	{
		oConfig.OptNumber( "Gain", m_dDumpReceiversGain, 1 );
		std::string sFormat;
		oConfig.OptString( "FilenameFormat", sFormat, "Receiver$(ReceiverID).wav" );
		onStartDumpReceivers( sFormat );
		return oReturn;
	}

	if( sCommand == "STOPDUMpReceiverS" )
	{
		onStopDumpReceivers( );
		return oReturn;
	}

	return oReturn;
}

void CVABinauralFreeFieldAudioRenderer::SetParameters( const CVAStruct& oParams )
{
	// Only delegate
	CallObject( oParams );
}

CVAStruct CVABinauralFreeFieldAudioRenderer::GetParameters( const CVAStruct& )
{
	return CVAStruct( *m_oParams.pConfig );
}

void CVABinauralFreeFieldAudioRenderer::onStartDumpReceivers( const std::string& sFilenameFormat )
{
	if( m_bDumpReceivers )
		VA_EXCEPT2( MODAL_ERROR, "Receivers dumping already started" );

	std::map<int, CVABFFReceiver*>::const_iterator clit = m_mReceivers.begin( );
	while( clit != m_mReceivers.end( ) )
	{
		CVABFFReceiver* pReceiver( clit++->second );
		pReceiver->InitDump( sFilenameFormat );
	}

	// Turn dumping on globally
	m_bDumpReceivers     = true;
	m_iDumpReceiversFlag = 1;
}

void CVABinauralFreeFieldAudioRenderer::onStopDumpReceivers( )
{
	if( !m_bDumpReceivers )
		VA_EXCEPT2( MODAL_ERROR, "Receivers dumping not started" );

	// Wait until the audio context ack's dump stop
	m_iDumpReceiversFlag = 2;
	while( m_iDumpReceiversFlag != 0 )
		VASleep( 20 );

	std::map<int, CVABFFReceiver*>::const_iterator clit = m_mReceivers.begin( );
	while( clit != m_mReceivers.end( ) )
	{
		CVABFFReceiver* pReceiver( clit++->second );
		pReceiver->FinalizeDump( );
	}

	m_bDumpReceivers = false;
}

#endif // VACORE_WITH_RENDERER_BINAURAL_FREE_FIELD
