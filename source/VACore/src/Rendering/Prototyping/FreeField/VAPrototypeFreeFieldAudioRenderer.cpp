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

#include "VAPrototypeFreeFieldAudioRenderer.h"

#ifdef VACORE_WITH_RENDERER_PROTOTYPE_FREE_FIELD

// VA includes
#	include <VA.h>

// VACore includes
#	include "../../../Motion/VAMotionModelBase.h"
#	include "../../../Motion/VASampleAndHoldMotionModel.h"
#	include "../../../Motion/VASharedMotionModel.h"
#	include "../../../Scene/VAScene.h"
#	include "../../../Utils/VAUtils.h"
#	include "../../../VAAudiostreamTracker.h"
#	include "../../../VACoreConfig.h"
#	include "../../../VALog.h"
#	include "../../../core/core.h"
#	include "../../../directivities/VADirectivity.h"
#	include "../../../directivities/VADirectivityDAFFHRIR.h"

// ITA includes
#	include <DAFF.h>
#	include <ITAClock.h>
#	include <ITAConfigUtils.h>
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
#	include <ITAVariableDelayLine.h>

// Vista includes
#	include <VistaInterProcComm/Concurrency/VistaThreadEvent.h>
#	include <VistaTools/VistaFileSystemDirectory.h>

// STL includes
#	include <algorithm>
#	include <atomic>
#	include <cassert>
#	include <fstream>
#	include <iomanip>

// 3rdParty includes
#	include <tbb/concurrent_queue.h>

class CVAPFFSoundPath : public CVAPoolObject
{
public:
	virtual ~CVAPFFSoundPath( );

	//! Retarded metrics of sound path
	class Metrics
	{
	public:
		double dRetardedDistance;           //!< Metrical distance to retarded sound position
		VAQuat yprAngleRetSourceToListener; //!< Immediate angle of incidence to retarded source position in listener reference frame in YPR convention
		VAQuat yprAngleListenerToRetSource; //!< Retarded angle of incidence to listener in source reference frame in YPR convention
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

	//! State of sound receiver directivity
	class CSoundReceiverDirectivityState
	{
	public:
		CSoundReceiverDirectivityState( ) : pData( NULL ), iRecord( -1 ), bDirectivityEnabled( true ) { };

		const CVADirectivityDAFFHRIR* pData; //!< Directivity data, may be NULL
		int iRecord;                         //!< Directivity index
		bool bDirectivityEnabled;

		inline bool operator==( const CSoundReceiverDirectivityState& rhs ) const
		{
			bool bBothEnabled     = ( bDirectivityEnabled == rhs.bDirectivityEnabled );
			bool bSameRecordIndex = ( iRecord == rhs.iRecord );
			bool bSameData        = ( pData == rhs.pData );

			return ( bBothEnabled && bSameRecordIndex && bSameData );
		};
	};

	CVAPrototypeFreeFieldAudioRenderer::CVAPFFSource* pSource;
	CVAPrototypeFreeFieldAudioRenderer::CVAPFFReceiver* pReceiver;

	CVASourceTargetMetrics oRelations; //!< Information on source and receiver relations (distances & angles)

	CDirectivityState oDirectivityStateCur;
	CDirectivityState oDirectivityStateNew;
	CSoundReceiverDirectivityState oSoundReceiverDirectivityStateCur;
	CSoundReceiverDirectivityState oSoundReceiverDirectivityStateNew;

	std::atomic<bool> bDelete;

	CITAThirdOctaveFilterbank* pThirdOctaveFilterBank;
	CITAVariableDelayLine* pVariableDelayLine;
	std::vector<ITAUPConvolution*> vpFIRFilterBanks;
	ITASampleBuffer m_sbSoundReceiverDirectivityTemp;

	float fPrevGain;

	inline void PreRequest( )
	{
		pSource   = nullptr;
		pReceiver = nullptr;
		fPrevGain = 0;
		// Reset DSP elements
		pThirdOctaveFilterBank->Clear( );
	};

	void UpdateSoundSourceDirectivity( const bool bDIRAuraModeEnabled );
	void UpdateSoundReceiverDirectivity( const bool bDIRAuraModeEnabled );

	void UpdateMediumPropagation( double dSpeedOfSound );
	double CalculateInverseDistanceDecrease( ) const;

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
	CVAPFFSoundPath( );

	//! Konstruktor
	CVAPFFSoundPath( const double dSampleRate, const int iBlockLength, const int iSoundSourceDirectivityFilterLength, const int iNumSoundReceiverChannels,
	                 const int iMaxSoundReceiverDirectivityFilterLength, int iFilterBankType );

	EVDLAlgorithm m_iDefaultVDLSwitchingAlgorithm; //!< Umsetzung der Verzgerungsnderung

	friend class CVAPFFSoundPathFactory;
};


class CVAPFFSoundPathFactory : public IVAPoolObjectFactory
{
public:
	CVAPFFSoundPathFactory( const double dSampleRate, const int iBlockLength, const int iSoundSourceDirectivityFilterLength, const int iNumSoundReceiverChannels,
	                        const int iMaxSoundReceiverDirectivityFilterLength, int iFilterBankType )
	    : m_dSampleRate( dSampleRate )
	    , m_iBlockLength( iBlockLength )
	    , m_iSoundSourceDirectivityFilterLength( iSoundSourceDirectivityFilterLength )
	    , m_iNumSoundReceiverChannels( iNumSoundReceiverChannels )
	    , m_iMaxSoundReceiverDirectivityFilterLength( iMaxSoundReceiverDirectivityFilterLength )
	    , m_iFilterBankType( iFilterBankType ) { };

	inline CVAPoolObject* CreatePoolObject( )
	{
		return new CVAPFFSoundPath( m_dSampleRate, m_iBlockLength, m_iSoundSourceDirectivityFilterLength, m_iNumSoundReceiverChannels,
		                            m_iMaxSoundReceiverDirectivityFilterLength, m_iFilterBankType );
	};

private:
	double m_dSampleRate;                      //!< Abtastrate
	int m_iBlockLength;                        //!< Blocklnge
	int m_iSoundSourceDirectivityFilterLength; //!< Filterlnge der Richtcharakteristik
	int m_iNumSoundReceiverChannels;
	int m_iMaxSoundReceiverDirectivityFilterLength;
	int m_iFilterBankType;
};

class CVAPFFListenerPoolFactory : public IVAPoolObjectFactory
{
public:
	inline CVAPFFListenerPoolFactory( CVACoreImpl* pCore, const CVAPrototypeFreeFieldAudioRenderer::CVAPFFReceiver::Config& oConf )
	    : m_pCore( pCore )
	    , m_oListenerConf( oConf ) { };

	inline CVAPoolObject* CreatePoolObject( )
	{
		CVAPrototypeFreeFieldAudioRenderer::CVAPFFReceiver* pListener;
		pListener = new CVAPrototypeFreeFieldAudioRenderer::CVAPFFReceiver( m_pCore, m_oListenerConf );
		return pListener;
	};

private:
	CVACoreImpl* m_pCore;
	const CVAPrototypeFreeFieldAudioRenderer::CVAPFFReceiver::Config& m_oListenerConf;

	//! Not for use, avoid C4512
	inline CVAPFFListenerPoolFactory operator=( const CVAPFFListenerPoolFactory& ) { VA_EXCEPT_NOT_IMPLEMENTED; };
};

class CVAPFFSourcePoolFactory : public IVAPoolObjectFactory
{
public:
	inline CVAPFFSourcePoolFactory( const CVAPrototypeFreeFieldAudioRenderer::CVAPFFSource::Config& oConf ) : m_oSourceConf( oConf ) { };

	inline CVAPoolObject* CreatePoolObject( )
	{
		CVAPrototypeFreeFieldAudioRenderer::CVAPFFSource* pSource;
		pSource = new CVAPrototypeFreeFieldAudioRenderer::CVAPFFSource( m_oSourceConf );
		return pSource;
	};

private:
	const CVAPrototypeFreeFieldAudioRenderer::CVAPFFSource::Config& m_oSourceConf;

	//! Not for use, avoid C4512
	inline CVAPFFSourcePoolFactory operator=( const CVAPFFSourcePoolFactory& ) { VA_EXCEPT_NOT_IMPLEMENTED; };
};

// Renderer

CVAPrototypeFreeFieldAudioRenderer::CVAPrototypeFreeFieldAudioRenderer( const CVAAudioRendererInitParams& oParams )
    : ITADatasourceRealization( 1, oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.dSampleRate, oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.iBuffersize )
    , CVAObject( oParams.sClass + ":" + oParams.sID )
    , m_pCore( oParams.pCore )
    , m_pCurSceneState( nullptr )
    , m_bRecordSoundReceiverOutputSignals( false )
    , m_dRecordSoundReceiverOutputGain( 1.0f )
    , m_oParams( oParams )
{
	// read config
	Init( *oParams.pConfig );

	IVAPoolObjectFactory* pListenerFactory = new CVAPFFListenerPoolFactory( m_pCore, m_oDefaultListenerConf );
	m_pListenerPool                        = IVAObjectPool::Create( 16, 2, pListenerFactory, true );

	IVAPoolObjectFactory* pSourceFactory = new CVAPFFSourcePoolFactory( m_oDefaultSourceConf );
	m_pSourcePool                        = IVAObjectPool::Create( 16, 2, pSourceFactory, true );

	m_pSoundPathFactory = new CVAPFFSoundPathFactory( GetSampleRate( ), GetBlocklength( ), 128, m_iRecordSoundReceiversNumChannels, 128, m_iFilterBankType );

	m_pSoundPathPool = IVAObjectPool::Create( 64, 8, m_pSoundPathFactory, true );

	m_pUpdateMessagePool = IVAObjectPool::Create( 2, 1, new CVAPoolObjectDefaultFactory<CVAPFFUpdateMessage>, true );

	ctxAudio.m_sbTemp.Init( GetBlocklength( ), true );
	ctxAudio.m_iResetFlag = 0; // Normal operation mode
	ctxAudio.m_iStatus    = 0; // Stopped

	m_iCurGlobalAuralizationMode = IVAInterface::VA_AURAMODE_DEFAULT;

	// Register the renderer as a module
	oParams.pCore->RegisterModule( this );
}

CVAPrototypeFreeFieldAudioRenderer::~CVAPrototypeFreeFieldAudioRenderer( )
{
	delete m_pSoundPathPool;
	delete m_pUpdateMessagePool;
}

void CVAPrototypeFreeFieldAudioRenderer::Init( const CVAStruct& oArgs )
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
		ITA_EXCEPT1( INVALID_PARAMETER, "Unrecognized interpolation algorithm '" + sVLDInterpolationAlgorithm + "' in PrototypeFreefieldAudioRendererConfig" );

	std::string sFilterBankType;
	conf.OptString( "FilterBankType", sFilterBankType, "iir" );
	if( sFilterBankType == "fir" )
		m_iFilterBankType = CITAThirdOctaveFilterbank::FIR_SPLINE_LINEAR_PHASE;
	else
		m_iFilterBankType = CITAThirdOctaveFilterbank::IIR_BIQUADS_ORDER10;

	conf.OptString( "BaseFolder", m_sBaseFolder, "" );
	VistaFileSystemDirectory oNode( m_sBaseFolder );
	if( m_sBaseFolder.empty( ) == false && oNode.Exists( ) == false )
		oNode.CreateWithParentDirectories( );

	conf.OptInteger( "RecordSoundReceiversNumChannels", m_iRecordSoundReceiversNumChannels, 1 );
	conf.OptBool( "RecordSoundReceivers", m_bRecordSoundReceiverOutputSignals, false );
	conf.OptNumber( "RecordSoundReceiversGain", m_dRecordSoundReceiverOutputGain, 1.0f );
	if( m_iRecordSoundReceiversNumChannels <= 0 && m_bRecordSoundReceiverOutputSignals )
		VA_EXCEPT2( INVALID_PARAMETER, "Can not record sound receiver stream with invalid number of channels, at least one channel required" );

	// Motion model Listener
	conf.OptInteger( "MotionModelNumHistoryKeys", m_oDefaultListenerConf.iMotionModelNumHistoryKeys, 1000 );

	if( m_oDefaultListenerConf.iMotionModelNumHistoryKeys < 1 )
		VA_EXCEPT2( INVALID_PARAMETER, "Basic motion model history needs to be greater than zero" );

	conf.OptNumber( "MotionModelWindowSize", m_oDefaultListenerConf.dMotionModelWindowSize, 0.1f );
	conf.OptNumber( "MotionModelWindowDelay", m_oDefaultListenerConf.dMotionModelWindowDelay, 0.1f );

	if( ( m_oDefaultListenerConf.dMotionModelWindowSize <= 0 ) || ( m_oDefaultListenerConf.dMotionModelWindowDelay < 0 ) )
		VA_EXCEPT2( INVALID_PARAMETER, "Basic motion model window parameters parse error (zero or negative?)" );

	conf.OptBool( "MotionModelLogInputListener", m_oDefaultListenerConf.bMotionModelLogInputEnabled, false );
	conf.OptBool( "MotionModelLogEstimatedOutputListener", m_oDefaultListenerConf.bMotionModelLogEstimatedEnabled, false );

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

void CVAPrototypeFreeFieldAudioRenderer::Reset( )
{
	VA_VERBOSE( "PrototypeFreeFieldAudioRenderer", "Received reset call, indicating reset now" );
	ctxAudio.m_iResetFlag = 1; // Request reset

	if( ctxAudio.m_iStatus == 0 || m_oParams.bOfflineRendering )
	{
		VA_VERBOSE( "PrototypeFreeFieldAudioRenderer", "Was not streaming, will reset manually" );
		// if no streaming active, reset manually
		// SyncInternalData();
		ResetInternalData( );
	}
	else
	{
		VA_VERBOSE( "PrototypeFreeFieldAudioRenderer", "Still streaming, will now wait for reset acknownledge" );
	}

	// Wait for last streaming block before internal reset
	while( ctxAudio.m_iResetFlag != 2 )
	{
		VASleep( 10 ); // Wait for acknowledge
	}

	VA_VERBOSE( "PrototypeFreeFieldAudioRenderer", "Operation reset has green light, clearing items" );

	// Iterate over sound pathes and free items
	std::list<CVAPFFSoundPath*>::iterator it = m_lSoundPaths.begin( );
	while( it != m_lSoundPaths.end( ) )
	{
		CVAPFFSoundPath* pPath = *it;

		int iNumRefs = pPath->GetNumReferences( );
		assert( iNumRefs == 1 );
		pPath->RemoveReference( );

		++it;
	}
	m_lSoundPaths.clear( );

	// Iterate over listener and free items
	std::map<int, CVAPFFReceiver*>::const_iterator lcit = m_mReceivers.begin( );
	while( lcit != m_mReceivers.end( ) )
	{
		CVAPFFReceiver* pListener( lcit->second );
		pListener->pData->RemoveReference( );
		assert( pListener->GetNumReferences( ) == 1 );
		pListener->RemoveReference( );
		lcit++;
	}
	m_mReceivers.clear( );

	// Iterate over sources and free items
	std::map<int, CVAPFFSource*>::const_iterator scit = m_mSources.begin( );
	while( scit != m_mSources.end( ) )
	{
		CVAPFFSource* pSource( scit->second );
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

	VA_VERBOSE( "PrototypeFreeFieldAudioRenderer", "Reset successful" );
}

void CVAPrototypeFreeFieldAudioRenderer::UpdateScene( CVASceneState* pNewSceneState )
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
	m_pUpdateMessage = dynamic_cast<CVAPFFUpdateMessage*>( m_pUpdateMessagePool->RequestObject( ) );

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

ITADatasource* CVAPrototypeFreeFieldAudioRenderer::GetOutputDatasource( )
{
	return this;
}

void CVAPrototypeFreeFieldAudioRenderer::ManageSoundPaths( const CVASceneState* pCurScene, const CVASceneState* pNewScene, const CVASceneStateDiff* pDiff )
{
	// Warning: take care for explicit sources and listeners for this renderer!

	// Iterate over current paths and mark deleted (will be removed within internal sync of audio context thread)
	std::list<CVAPFFSoundPath*>::iterator itp = m_lSoundPaths.begin( );
	while( itp != m_lSoundPaths.end( ) )
	{
		CVAPFFSoundPath* pPath( *itp );
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
					bDeletetionRequired = true; // Receiver removed, deletion required
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
			CVAPFFSource* pSource = CreateSource( iID, pNewScene->GetSoundSourceState( iID ) );
	}

	// New receivers
	citr = pDiff->viNewReceiverIDs.begin( );
	while( citr != pDiff->viNewReceiverIDs.end( ) )
	{
		const int& iID( *citr++ );
		CVAPFFReceiver* pReceiver = CreateReceiver( iID, pNewScene->GetReceiverState( iID ) );
	}

	// New paths: (1) new receivers, current sources
	citr = pDiff->viNewReceiverIDs.begin( );
	while( citr != pDiff->viNewReceiverIDs.end( ) )
	{
		int iReceiverID            = ( *citr++ );
		CVAPFFReceiver* pRecceiver = m_mReceivers[iReceiverID];

		for( size_t i = 0; i < pDiff->viComSoundSourceIDs.size( ); i++ )
		{
			// Only add, if no other renderer has been connected explicitly with this source
			// and only, if not marked for deletion
			int iSourceID                             = pDiff->viComSoundSourceIDs[i];
			std::map<int, CVAPFFSource*>::iterator it = m_mSources.find( iSourceID );
			if( it == m_mSources.end( ) )
				continue; // This source is skipped by the renderer
			CVAPFFSource* pSource = it->second;

			const CVASoundSourceDesc* pSoundSourceDesc = m_pCore->GetSceneManager( )->GetSoundSourceDesc( iSourceID );
			if( !pSource->bDeleted && ( pSoundSourceDesc->sExplicitRendererID.empty( ) || pSoundSourceDesc->sExplicitRendererID == m_oParams.sID ) )
				CVAPFFSoundPath* pPath = CreateSoundPath( pSource, pRecceiver );
		}
	}

	// New paths: (2) new sources, current receivers
	cits = pDiff->viNewSoundSourceIDs.begin( );
	while( cits != pDiff->viNewSoundSourceIDs.end( ) )
	{
		const int& iSourceID( *cits++ );
		std::map<int, CVAPFFSource*>::iterator it = m_mSources.find( iSourceID );
		if( it == m_mSources.end( ) )
			continue; // Explicit source is not connected to this renderer

		CVAPFFSource* pSource = it->second;
		for( size_t i = 0; i < pDiff->viComReceiverIDs.size( ); i++ )
		{
			int iReceiverID           = pDiff->viComReceiverIDs[i];
			CVAPFFReceiver* pReceiver = m_mReceivers[iReceiverID];
			if( !pReceiver->bDeleted )
				CVAPFFSoundPath* pPath = CreateSoundPath( pSource, pReceiver );
		}
	}

	// New paths: (3) new sources, new receivers
	cits = pDiff->viNewSoundSourceIDs.begin( );
	while( cits != pDiff->viNewSoundSourceIDs.end( ) )
	{
		const int& iSourceID( *cits++ );
		std::map<int, CVAPFFSource*>::iterator it = m_mSources.find( iSourceID );

		if( it == m_mSources.end( ) )
			continue;

		CVAPFFSource* pSource = it->second;
		assert( pSource );

		citr = pDiff->viNewReceiverIDs.begin( );
		while( citr != pDiff->viNewReceiverIDs.end( ) )
		{
			const int& iListenerID( *citr++ );
			CVAPFFReceiver* pListener = m_mReceivers[iListenerID];
			CVAPFFSoundPath* pPath    = CreateSoundPath( pSource, pListener );
		}
	}

	return;
}

void CVAPrototypeFreeFieldAudioRenderer::ProcessStream( const ITAStreamInfo* pStreamInfo )
{
	if( ctxAudio.m_iStatus == 0 )
	{
		// If streaming is active, set to 1
		ctxAudio.m_iStatus = 1;
	}

	// Schallpfade abgleichen
	SyncInternalData( );

	float* pfOutput = GetWritePointer( 0 );

	fm_zero( pfOutput, GetBlocklength( ) );

	const CVAAudiostreamState* pStreamState = dynamic_cast<const CVAAudiostreamState*>( pStreamInfo );
	double dListenerTime                    = pStreamState->dSysTime;

	// Check for reset request
	if( ctxAudio.m_iResetFlag == 1 )
	{
		VA_VERBOSE( "PrototypeFreeFieldAudioRenderer", "Process stream detecting reset request, will reset internally now" );
		ResetInternalData( );

		return;
	}
	else if( ctxAudio.m_iResetFlag == 2 )
	{
		VA_VERBOSE( "PrototypeFreeFieldAudioRenderer", "Process stream detecting ongoing reset, will stop processing here" );

		return;
	}

	SampleTrajectoriesInternal( dListenerTime );

	std::list<CVAPFFReceiver*>::iterator lit = ctxAudio.m_lReceivers.begin( );
	while( lit != ctxAudio.m_lReceivers.end( ) )
	{
		CVAPFFReceiver* pListener( *( lit++ ) );
		pListener->psfOutput->zero( );
	}

	// Update sound pathes
	std::list<CVAPFFSoundPath*>::iterator spit = ctxAudio.m_lSoundPaths.begin( );
	while( spit != ctxAudio.m_lSoundPaths.end( ) )
	{
		CVAPFFSoundPath* pPath( *spit );
		CVAReceiverState* pListenerState  = ( m_pCurSceneState ? m_pCurSceneState->GetReceiverState( pPath->pReceiver->pData->iID ) : NULL );
		CVASoundSourceState* pSourceState = ( m_pCurSceneState ? m_pCurSceneState->GetSoundSourceState( pPath->pSource->pData->iID ) : NULL );

		if( pListenerState == nullptr || pSourceState == nullptr )
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
		bool bDPEnabledListener = ( pListenerState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_DOPPLER ) > 0;
		bool bDPEnabledSource   = ( pSourceState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_DOPPLER ) > 0;
		bool bDPEnabledCurrent  = ( pPath->pVariableDelayLine->GetAlgorithm( ) != EVDLAlgorithm::SWITCH ); // switch = disabled
		bool bDPStatusChanged   = ( bDPEnabledCurrent != ( bDPEnabledGlobal && bDPEnabledListener && bDPEnabledSource ) );
		if( bDPStatusChanged )
			pPath->pVariableDelayLine->SetAlgorithm( !bDPEnabledCurrent ? m_iDefaultVDLSwitchingAlgorithm : EVDLAlgorithm::SWITCH );
		pPath->UpdateMediumPropagation( m_pCore->oHomogeneousMedium.dSoundSpeed );

		// Spherical spreading loss
		bool bSSLEnabledSource   = ( pSourceState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_SPREADING_LOSS ) > 0;
		bool bSSLEnabledListener = ( pListenerState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_SPREADING_LOSS ) > 0;
		bool bSSLEnabledGlobal   = ( m_iCurGlobalAuralizationMode & IVAInterface::VA_AURAMODE_SPREADING_LOSS ) > 0;
		bool bSSLEnabled         = ( bSSLEnabledSource && bSSLEnabledListener && bSSLEnabledGlobal );
		double dDistanceDecrease = pPath->CalculateInverseDistanceDecrease( );
		if( !bSSLEnabled )
			dDistanceDecrease = 1.0f;

		bool bDIREnabledSource   = ( pSourceState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_SOURCE_DIRECTIVITY ) > 0;
		bool bDIREnabledListener = ( pListenerState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_SOURCE_DIRECTIVITY ) > 0;
		bool bDIREnabledGlobal   = ( m_iCurGlobalAuralizationMode & IVAInterface::VA_AURAMODE_SOURCE_DIRECTIVITY ) > 0;
		bool bDIREnabled         = ( bDIREnabledSource && bDIREnabledListener && bDIREnabledGlobal );
		pPath->UpdateSoundSourceDirectivity( bDIREnabled );
		pPath->UpdateSoundReceiverDirectivity( bDIREnabled );

		// Sound source gain / direct sound audibility via AuraMode flags
		bool bDSSourceStatusEnabled   = ( pSourceState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_DIRECT_SOUND );
		bool bDSListenerStatusEnabled = ( pListenerState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_DIRECT_SOUND );
		bool bDSGlobalStatusEnabled   = ( m_iCurGlobalAuralizationMode & IVAInterface::VA_AURAMODE_DIRECT_SOUND );
		bool bDSEnabled               = bDSSourceStatusEnabled && bDSListenerStatusEnabled && bDSGlobalStatusEnabled;

		float fSoundSourceGain = float( dDistanceDecrease * pSourceState->GetVolume( m_oParams.pCore->GetCoreConfig( )->dDefaultAmplitudeCalibration ) );
		if( pPath->pSource->pData->bMuted || ( bDSEnabled == false ) )
			fSoundSourceGain = 0.0f;

		// --= DSP =--

		CVASoundSourceDesc* pSourceData = pPath->pSource->pData;
		const ITASampleFrame& sfInputBuffer( *pSourceData->psfSignalSourceInputBuf );
		const ITASampleBuffer* psbInput( &( sfInputBuffer[0] ) );
		assert( psbInput );              // Knallt es hier, dann ist die Eingabequelle noch nicht gesetzt
		assert( pSourceData->iID >= 0 ); // Knallt es hier, dann wurde dem SoundPath unterm hintern die Quelle entzogen! -> Problem mit Referenzierung und Reset?

		pPath->pVariableDelayLine->Process( psbInput, &( ctxAudio.m_sbTemp ) );
		pPath->pThirdOctaveFilterBank->Process( ctxAudio.m_sbTemp.data( ), ctxAudio.m_sbTemp.data( ) );

		ctxAudio.m_sbTemp.Envelope( pPath->fPrevGain, fSoundSourceGain );
		pPath->fPrevGain = fSoundSourceGain;

		assert( m_iRecordSoundReceiversNumChannels == pPath->vpFIRFilterBanks.size( ) );
		for( size_t i = 0; i < pPath->vpFIRFilterBanks.size( ); i++ )
		{
			ITASampleBuffer& sfTarget       = ( *pPath->pReceiver->psfOutput )[i];
			ITAUPConvolution* pFIRConvolver = pPath->vpFIRFilterBanks[i];
			pFIRConvolver->Process( ctxAudio.m_sbTemp.data( ), sfTarget.GetData( ), ITABase::MixingMethod::ADD );
		}

		spit++;
	}


	lit = ctxAudio.m_lReceivers.begin( );
	while( lit != ctxAudio.m_lReceivers.end( ) )
	{
		CVAPFFReceiver* pReceiver( *( lit++ ) );

		if( !pReceiver->pData->bMuted )
			fm_add( pfOutput, ( *pReceiver->psfOutput )[0].data( ), m_uiBlocklength );

		if( m_bRecordSoundReceiverOutputSignals )
		{
			pReceiver->psfOutput->mul_scalar( float( m_dRecordSoundReceiverOutputGain ) );
			pReceiver->pListenerOutputAudioFileWriter->write( pReceiver->psfOutput );
		}
	}

	IncrementWritePointer( );

	return;
}

void CVAPrototypeFreeFieldAudioRenderer::UpdateTrajectories( )
{
	// Neue Quellendaten bernehmen
	for( std::map<int, CVAPFFSource*>::iterator it = m_mSources.begin( ); it != m_mSources.end( ); ++it )
	{
		int iSourceID         = it->first;
		CVAPFFSource* pSource = it->second;

		CVASoundSourceState* pSourceCur = ( m_pCurSceneState ? m_pCurSceneState->GetSoundSourceState( iSourceID ) : nullptr );
		CVASoundSourceState* pSourceNew = ( m_pNewSceneState ? m_pNewSceneState->GetSoundSourceState( iSourceID ) : nullptr );

		const CVAMotionState* pMotionCur = ( pSourceCur ? pSourceCur->GetMotionState( ) : nullptr );
		const CVAMotionState* pMotionNew = ( pSourceNew ? pSourceNew->GetMotionState( ) : nullptr );

		if( pMotionNew && ( pMotionNew != pMotionCur ) )
		{
			VA_TRACE( "PrototypeFreeFieldAudioRenderer", "Source " << iSourceID << " new motion state" );
			pSource->pMotionModel->InputMotionKey( pMotionNew );
		}
	}

	// Neue Hrerdaten bernehmen
	for( std::map<int, CVAPFFReceiver*>::iterator it = m_mReceivers.begin( ); it != m_mReceivers.end( ); ++it )
	{
		int iListenerID           = it->first;
		CVAPFFReceiver* pListener = it->second;

		CVAReceiverState* pListenerCur = ( m_pCurSceneState ? m_pCurSceneState->GetReceiverState( iListenerID ) : nullptr );
		CVAReceiverState* pListenerNew = ( m_pNewSceneState ? m_pNewSceneState->GetReceiverState( iListenerID ) : nullptr );

		const CVAMotionState* pMotionCur = ( pListenerCur ? pListenerCur->GetMotionState( ) : nullptr );
		const CVAMotionState* pMotionNew = ( pListenerNew ? pListenerNew->GetMotionState( ) : nullptr );

		if( pMotionNew && ( pMotionNew != pMotionCur ) )
		{
			VA_TRACE( "PrototypeFreeFieldAudioRenderer", "Listener " << iListenerID << " new position " ); // << *pMotionNew);
			pListener->pMotionModel->InputMotionKey( pMotionNew );
		}
	}
}

void CVAPrototypeFreeFieldAudioRenderer::SampleTrajectoriesInternal( double dTime )
{
	bool bValid = true;
	for( std::list<CVAPFFSource*>::iterator it = ctxAudio.m_lSources.begin( ); it != ctxAudio.m_lSources.end( ); ++it )
	{
		CVAPFFSource* pSource = *it;

		pSource->pMotionModel->HandleMotionKeys( );
		bValid &= pSource->pMotionModel->EstimatePosition( dTime, pSource->vPredPos );
		bValid &= pSource->pMotionModel->EstimateOrientation( dTime, pSource->vPredView, pSource->vPredUp );
		pSource->bValidTrajectoryPresent = bValid;
	}

	bValid = true;
	for( std::list<CVAPFFReceiver*>::iterator it = ctxAudio.m_lReceivers.begin( ); it != ctxAudio.m_lReceivers.end( ); ++it )
	{
		CVAPFFReceiver* pListener = *it;

		pListener->pMotionModel->HandleMotionKeys( );
		bValid &= pListener->pMotionModel->EstimatePosition( dTime, pListener->vPredPos );
		bValid &= pListener->pMotionModel->EstimateOrientation( dTime, pListener->vPredView, pListener->vPredUp );
		pListener->bValidTrajectoryPresent = bValid;
	}
}

CVAPFFSoundPath* CVAPrototypeFreeFieldAudioRenderer::CreateSoundPath( CVAPFFSource* pSource, CVAPFFReceiver* pListener )
{
	int iSourceID   = pSource->pData->iID;
	int iListenerID = pListener->pData->iID;

	assert( !pSource->bDeleted && !pListener->bDeleted );

	VA_VERBOSE( "PrototypeFreeFieldAudioRenderer", "Creating sound path from source " << iSourceID << " -> listener " << iListenerID );

	CVAPFFSoundPath* pPath = dynamic_cast<CVAPFFSoundPath*>( m_pSoundPathPool->RequestObject( ) );

	pPath->pSource   = pSource;
	pPath->pReceiver = pListener;

	pPath->bDelete = false;

	CVASoundSourceState* pSourceNew = ( m_pNewSceneState ? m_pNewSceneState->GetSoundSourceState( iSourceID ) : nullptr );
	if( pSourceNew != nullptr )
		pPath->oDirectivityStateNew.pData = (IVADirectivity*)pSourceNew->GetDirectivityData( );

	CVAReceiverState* pSoundReceiverNew = ( m_pNewSceneState ? m_pNewSceneState->GetReceiverState( iListenerID ) : nullptr );
	if( pSoundReceiverNew != nullptr )
		pPath->oSoundReceiverDirectivityStateNew.pData = dynamic_cast<const CVADirectivityDAFFHRIR*>( pSoundReceiverNew->GetDirectivity( ) );

	m_lSoundPaths.push_back( pPath );
	m_pUpdateMessage->vNewPaths.push_back( pPath );

	return pPath;
}

void CVAPrototypeFreeFieldAudioRenderer::DeleteSoundPath( CVAPFFSoundPath* pPath )
{
	VA_VERBOSE( "PrototypeFreeFieldAudioRenderer",
	            "Marking sound path from source " << pPath->pSource->pData->iID << " -> listener " << pPath->pReceiver->pData->iID << " for deletion" );

	pPath->bDelete = true;
	pPath->RemoveReference( );
	m_pUpdateMessage->vDelPaths.push_back( pPath );
}

CVAPrototypeFreeFieldAudioRenderer::CVAPFFReceiver* CVAPrototypeFreeFieldAudioRenderer::CreateReceiver( const int iID, const CVAReceiverState* pListenerState )
{
	VA_VERBOSE( "PrototypeFreeFieldAudioRenderer", "Creating listener with ID " << iID );

	CVAPFFReceiver* pListener = dynamic_cast<CVAPFFReceiver*>( m_pListenerPool->RequestObject( ) ); // Reference = 1

	pListener->pData = m_pCore->GetSceneManager( )->GetSoundReceiverDesc( iID );
	pListener->pData->AddReference( );

	// Move to prerequest of pool?
	pListener->psfOutput = new ITASampleFrame( m_iRecordSoundReceiversNumChannels, m_pCore->GetCoreConfig( )->oAudioDriverConfig.iBuffersize, true );

	if( m_bRecordSoundReceiverOutputSignals )
	{
		std::string sFileBaseName = m_oParams.sID + "_Listener" + IntToString( pListener->pData->iID ) + ".wav";
		if( m_sBaseFolder.empty( ) == false )
			sFileBaseName = m_sBaseFolder + "/" + sFileBaseName;

		ITAAudiofileProperties props;
		props.dSampleRate                         = m_pCore->GetCoreConfig( )->oAudioDriverConfig.dSampleRate;
		props.eDomain                             = ITADomain::ITA_TIME_DOMAIN;
		props.eQuantization                       = ITAQuantization::ITA_FLOAT;
		props.iChannels                           = m_iRecordSoundReceiversNumChannels;
		props.iLength                             = 0;
		pListener->pListenerOutputAudioFileWriter = ITABufferedAudiofileWriter::create( props );
		pListener->pListenerOutputAudioFileWriter->SetFilePath( sFileBaseName );
		VA_INFO( "PrototypeFreeFieldAudioRenderer", "Will record listener output signal and export to file '"
		                                                << sFileBaseName << "' using " << m_iRecordSoundReceiversNumChannels << " channels after deletion" );
	}

	assert( pListener->pData );
	pListener->bDeleted = false;

	// Motion model
	CVABasicMotionModel* pMotionInstance = dynamic_cast<CVABasicMotionModel*>( pListener->pMotionModel->GetInstance( ) );
	pMotionInstance->SetName( std::string( "mfrend_mm_listener_" + pListener->pData->sName ) );
	pMotionInstance->Reset( );

	m_mReceivers.insert( std::pair<int, CVAPrototypeFreeFieldAudioRenderer::CVAPFFReceiver*>( iID, pListener ) );

	m_pUpdateMessage->vNewReceivers.push_back( pListener );

	return pListener;
}

void CVAPrototypeFreeFieldAudioRenderer::DeleteReceiver( int iListenerID )
{
	VA_VERBOSE( "PrototypeFreeFieldAudioRenderer", "Marking listener with ID " << iListenerID << " for removal" );
	std::map<int, CVAPFFReceiver*>::iterator it = m_mReceivers.find( iListenerID );
	CVAPFFReceiver* pListener                   = it->second;
	m_mReceivers.erase( it );
	pListener->bDeleted = true;
	pListener->pData->RemoveReference( );
	pListener->RemoveReference( );

	m_pUpdateMessage->vDelReceivers.push_back( pListener );

	return;
}

CVAPrototypeFreeFieldAudioRenderer::CVAPFFSource* CVAPrototypeFreeFieldAudioRenderer::CreateSource( const int iID, const CVASoundSourceState* pSourceState )
{
	VA_VERBOSE( "PrototypeFreeFieldAudioRenderer", "Creating source with ID " << iID );
	CVAPFFSource* pSource = dynamic_cast<CVAPFFSource*>( m_pSourcePool->RequestObject( ) );

	pSource->pData = m_pCore->GetSceneManager( )->GetSoundSourceDesc( iID );
	pSource->pData->AddReference( );

	pSource->bDeleted = false;

	CVABasicMotionModel* pMotionInstance = dynamic_cast<CVABasicMotionModel*>( pSource->pMotionModel->GetInstance( ) );
	std::string sName                    = std::string( m_oParams.sID + "_Source" + IntToString( pSource->pData->iID ) + "_MotionModel" );
	if( m_sBaseFolder.empty( ) == false )
		sName = m_sBaseFolder + "/" + sName;
	pMotionInstance->SetName( sName );
	pMotionInstance->Reset( );

	m_mSources.insert( std::pair<int, CVAPFFSource*>( iID, pSource ) );

	m_pUpdateMessage->vNewSources.push_back( pSource );

	return pSource;
}

void CVAPrototypeFreeFieldAudioRenderer::DeleteSource( int iSourceID )
{
	VA_VERBOSE( "PrototypeFreeFieldAudioRenderer", "Marking source with ID " << iSourceID << " for removal" );
	std::map<int, CVAPFFSource*>::iterator it = m_mSources.find( iSourceID );
	if( it == m_mSources.end( ) ) // Not found in internal list ...
	{
		CVASoundSourceDesc* pDesc = m_pCore->GetSceneManager( )->GetSoundSourceDesc( iSourceID );
		if( !pDesc->sExplicitRendererID.empty( ) || pDesc->sExplicitRendererID == GetObjectName( ) )
			VA_WARN( "PrototypeFreeFieldAudioRenderer", "Attempted to remote an explicit sound source for this renderer which could not be found." );
		return;
	}

	CVAPFFSource* pSource = it->second;
	m_mSources.erase( it );
	pSource->bDeleted = true;
	pSource->pData->RemoveReference( );
	pSource->RemoveReference( );

	m_pUpdateMessage->vDelSources.push_back( pSource );

	return;
}

void CVAPrototypeFreeFieldAudioRenderer::SyncInternalData( )
{
	CVAPFFUpdateMessage* pUpdate;
	while( ctxAudio.m_qpUpdateMessages.try_pop( pUpdate ) )
	{
		std::list<CVAPFFSoundPath*>::const_iterator citp = pUpdate->vDelPaths.begin( );
		while( citp != pUpdate->vDelPaths.end( ) )
		{
			CVAPFFSoundPath* pPath( *citp++ );
			ctxAudio.m_lSoundPaths.remove( pPath );
			pPath->RemoveReference( );
		}

		citp = pUpdate->vNewPaths.begin( );
		while( citp != pUpdate->vNewPaths.end( ) )
		{
			CVAPFFSoundPath* pPath( *citp++ );
			pPath->AddReference( );
			ctxAudio.m_lSoundPaths.push_back( pPath );
		}

		std::list<CVAPFFSource*>::const_iterator cits = pUpdate->vDelSources.begin( );
		while( cits != pUpdate->vDelSources.end( ) )
		{
			CVAPFFSource* pSource( *cits++ );
			ctxAudio.m_lSources.remove( pSource );
			pSource->pData->RemoveReference( );
			pSource->RemoveReference( );
		}

		cits = pUpdate->vNewSources.begin( );
		while( cits != pUpdate->vNewSources.end( ) )
		{
			CVAPFFSource* pSource( *cits++ );
			pSource->AddReference( );
			pSource->pData->AddReference( );
			ctxAudio.m_lSources.push_back( pSource );
		}

		std::list<CVAPFFReceiver*>::const_iterator citl = pUpdate->vDelReceivers.begin( );
		while( citl != pUpdate->vDelReceivers.end( ) )
		{
			CVAPFFReceiver* pListener( *citl++ );
			ctxAudio.m_lReceivers.remove( pListener );
			pListener->pData->RemoveReference( );
			pListener->RemoveReference( );
		}

		citl = pUpdate->vNewReceivers.begin( );
		while( citl != pUpdate->vNewReceivers.end( ) )
		{
			CVAPFFReceiver* pListener( *citl++ );
			pListener->AddReference( );
			pListener->pData->AddReference( );
			ctxAudio.m_lReceivers.push_back( pListener );
		}

		pUpdate->RemoveReference( );
	}

	return;
}

void CVAPrototypeFreeFieldAudioRenderer::ResetInternalData( )
{
	VA_VERBOSE( "PrototypeFreeFieldAudioRenderer", "Resetting internally" );

	std::list<CVAPFFSoundPath*>::const_iterator citp = ctxAudio.m_lSoundPaths.begin( );
	while( citp != ctxAudio.m_lSoundPaths.end( ) )
	{
		CVAPFFSoundPath* pPath( *citp++ );
		pPath->RemoveReference( );
	}

	ctxAudio.m_lSoundPaths.clear( );

	std::list<CVAPFFReceiver*>::const_iterator itl = ctxAudio.m_lReceivers.begin( );
	while( itl != ctxAudio.m_lReceivers.end( ) )
	{
		CVAPFFReceiver* pListener( *itl++ );
		pListener->pData->RemoveReference( );
		pListener->RemoveReference( );
	}
	ctxAudio.m_lReceivers.clear( );

	std::list<CVAPFFSource*>::const_iterator cits = ctxAudio.m_lSources.begin( );
	while( cits != ctxAudio.m_lSources.end( ) )
	{
		CVAPFFSource* pSource( *cits++ );
		pSource->pData->RemoveReference( );
		pSource->RemoveReference( );
	}
	ctxAudio.m_lSources.clear( );

	ctxAudio.m_iResetFlag = 2; // set ack
	VA_VERBOSE( "PrototypeFreeFieldAudioRenderer", "Reset acknowledged" );
}

void CVAPrototypeFreeFieldAudioRenderer::UpdateSoundPaths( )
{
	int iGlobalAuralisationMode = m_iCurGlobalAuralizationMode;

	// Check for new data
	std::list<CVAPFFSoundPath*>::iterator it = m_lSoundPaths.begin( );
	while( it != m_lSoundPaths.end( ) )
	{
		CVAPFFSoundPath* pPath( *it++ );

		CVAPFFSource* pSource     = pPath->pSource;
		CVAPFFReceiver* pListener = pPath->pReceiver;

		int iSourceID   = pSource->pData->iID;
		int iListenerID = pListener->pData->iID;

		CVASoundSourceState* pSourceCur = ( m_pCurSceneState ? m_pCurSceneState->GetSoundSourceState( iSourceID ) : nullptr );
		CVASoundSourceState* pSourceNew = ( m_pNewSceneState ? m_pNewSceneState->GetSoundSourceState( iSourceID ) : nullptr );

		CVAReceiverState* pListenerCur = ( m_pCurSceneState ? m_pCurSceneState->GetReceiverState( iListenerID ) : nullptr );
		CVAReceiverState* pListenerNew = ( m_pNewSceneState ? m_pNewSceneState->GetReceiverState( iListenerID ) : nullptr );

		if( pSourceNew == nullptr )
		{
			pPath->oDirectivityStateNew.pData = nullptr;
		}
		else
		{
			pPath->oDirectivityStateNew.pData = (IVADirectivity*)pSourceNew->GetDirectivityData( );
		}
	}

	return;
}

void CVAPrototypeFreeFieldAudioRenderer::UpdateGlobalAuralizationMode( const int iGlobalAuralizationMode )
{
	if( m_iCurGlobalAuralizationMode == iGlobalAuralizationMode )
		return;

	m_iCurGlobalAuralizationMode = iGlobalAuralizationMode;

	return;
}


// Class CPFFSoundPath

CVAPFFSoundPath::CVAPFFSoundPath( const double dSamplerate, const int iBlocklength, const int iMaxSoundSourceDirectivityFilterLength, const int iNumSoundReceiverChannels,
                                  const int iMaxSoundReceiverDirectivityFilterLength, int iFilterBankType )
{
	pThirdOctaveFilterBank = CITAThirdOctaveFilterbank::Create( dSamplerate, iBlocklength, iFilterBankType );
	pThirdOctaveFilterBank->SetIdentity( );

	float fReserverdMaxDelaySamples = (float)( 3 * dSamplerate ); // 3 Sekunden ~ 1km Entfernung
	m_iDefaultVDLSwitchingAlgorithm = EVDLAlgorithm::CUBIC_SPLINE_INTERPOLATION;
	pVariableDelayLine              = new CITAVariableDelayLine( dSamplerate, iBlocklength, fReserverdMaxDelaySamples, m_iDefaultVDLSwitchingAlgorithm );

	assert( iNumSoundReceiverChannels > 0 );
	for( int i = 0; i < iNumSoundReceiverChannels; i++ )
	{
		ITAUPConvolution* pFIRConvolver = new ITAUPConvolution( iBlocklength, iMaxSoundReceiverDirectivityFilterLength );
		vpFIRFilterBanks.push_back( pFIRConvolver );
	}

	m_sbSoundReceiverDirectivityTemp.Init( iMaxSoundReceiverDirectivityFilterLength, true );
}

CVAPFFSoundPath::~CVAPFFSoundPath( )
{
	delete pThirdOctaveFilterBank;
	delete pVariableDelayLine;
	for( size_t i = 0; i < vpFIRFilterBanks.size( ); i++ )
		delete vpFIRFilterBanks[i];
}

void CVAPFFSoundPath::UpdateMetrics( )
{
	if( pSource->vPredPos != pReceiver->vPredPos )
		oRelations.Calc( pSource->vPredPos, pSource->vPredView, pSource->vPredUp, pReceiver->vPredPos, pReceiver->vPredView, pReceiver->vPredUp );
}


void CVAPFFSoundPath::UpdateSoundSourceDirectivity( const bool bDIRAuraModeEnabled )
{
	if( bDIRAuraModeEnabled == true )
	{
		DAFFContentMS* pDirectivityDataNew = (DAFFContentMS*)oDirectivityStateNew.pData;
		DAFFContentMS* pDirectivityDataCur = (DAFFContentMS*)oDirectivityStateCur.pData;

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
			pDirectivityDataNew->getNearestNeighbour( DAFF_OBJECT_VIEW, float( oRelations.dAzimuthS2T ), float( oRelations.dElevationS2T ),
			                                          oDirectivityStateNew.iRecord );
			if( oDirectivityStateCur.iRecord != oDirectivityStateNew.iRecord )
			{
				ITABase::CThirdOctaveGainMagnitudeSpectrum oDirectivityMagnitudes;
				std::vector<float> vfMags( oDirectivityMagnitudes.GetNumBands( ) );
				pDirectivityDataNew->getMagnitudes( oDirectivityStateNew.iRecord, 0, &vfMags[0] );
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

void CVAPFFSoundPath::UpdateSoundReceiverDirectivity( const bool bDIRAuraModeEnabled )
{
	if( bDIRAuraModeEnabled == true )
	{
		CVADirectivityDAFFHRIR* pDirectivityDataNew = (CVADirectivityDAFFHRIR*)oSoundReceiverDirectivityStateNew.pData;
		CVADirectivityDAFFHRIR* pDirectivityDataCur = (CVADirectivityDAFFHRIR*)oSoundReceiverDirectivityStateCur.pData;

		oSoundReceiverDirectivityStateNew.bDirectivityEnabled = true;

		if( pDirectivityDataNew == nullptr )
		{
			// Directivity removed
			if( pDirectivityDataCur != nullptr )
			{
				for( size_t i = 0; i < vpFIRFilterBanks.size( ); i++ )
				{
					ITAUPFilter* pFilter = vpFIRFilterBanks[i]->RequestFilter( );
					pFilter->identity( );
					vpFIRFilterBanks[i]->ExchangeFilter( pFilter );
				}
				oSoundReceiverDirectivityStateNew.iRecord = -1;
			}
		}
		else
		{
			// Update directivity data set
			pDirectivityDataNew->GetDAFFContent( )->getNearestNeighbour( DAFF_OBJECT_VIEW, float( oRelations.dAzimuthT2S ), float( oRelations.dElevationT2S ),
			                                                             oSoundReceiverDirectivityStateNew.iRecord );
			if( oSoundReceiverDirectivityStateCur.iRecord != oSoundReceiverDirectivityStateNew.iRecord )
			{
				if( m_sbSoundReceiverDirectivityTemp.GetLength( ) < pDirectivityDataNew->GetDAFFContent( )->getFilterLength( ) )
					m_sbSoundReceiverDirectivityTemp.Init( pDirectivityDataNew->GetDAFFContent( )->getFilterLength( ), true );

				const int iMaxProcessingChannels =
				    (std::min)( int( vpFIRFilterBanks.size( ) ), pDirectivityDataNew->GetDAFFContent( )->getProperties( )->getNumberOfChannels( ) );
				for( size_t i = 0; i < iMaxProcessingChannels; i++ )
				{
					ITAUPConvolution* pFIRConvolver = vpFIRFilterBanks[i];

					pDirectivityDataNew->GetDAFFContent( )->getEffectiveFilterCoeffs( oSoundReceiverDirectivityStateNew.iRecord, int( i ),
					                                                                  m_sbSoundReceiverDirectivityTemp.GetData( ) );

					const int iFilterTaps = (std::min)( pFIRConvolver->GetMaxFilterlength( ), m_sbSoundReceiverDirectivityTemp.GetLength( ) );
					ITAUPFilter* pFilter  = pFIRConvolver->RequestFilter( );
					pFilter->Load( m_sbSoundReceiverDirectivityTemp.GetData( ), iFilterTaps );

					pFIRConvolver->ExchangeFilter( pFilter );
				}
			}
		}
	}
	else
	{
		if( oSoundReceiverDirectivityStateCur.bDirectivityEnabled == true )
		{
			// Switch to disabled DIR
			for( size_t i = 0; i < vpFIRFilterBanks.size( ); i++ )
			{
				ITAUPFilter* pFilter = vpFIRFilterBanks[i]->RequestFilter( );
				pFilter->identity( );
				vpFIRFilterBanks[i]->ExchangeFilter( pFilter );
			}
			oSoundReceiverDirectivityStateNew.bDirectivityEnabled = false;
			oSoundReceiverDirectivityStateNew.iRecord             = -1;
		}
	}

	// Acknowledge new state
	oSoundReceiverDirectivityStateCur = oSoundReceiverDirectivityStateNew;

	return;
}

void CVAPFFSoundPath::UpdateMediumPropagation( double dSpeedOfSound )
{
	assert( dSpeedOfSound > 0 );

	double dDelay = oRelations.dDistance / dSpeedOfSound;
	pVariableDelayLine->SetDelayTime( float( dDelay ) );
}

double CVAPFFSoundPath::CalculateInverseDistanceDecrease( ) const
{
	// Gain limiter
	const double MINIMUM_DISTANCE = 1 / db20_to_ratio( 10 );

	double dDistance = (std::max)( (double)oRelations.dDistance, MINIMUM_DISTANCE );

	double fInverseDistanceDecrease = 1.0f / dDistance;

	return fInverseDistanceDecrease;
}


CVAStruct CVAPrototypeFreeFieldAudioRenderer::CallObject( const CVAStruct& oArgs )
{
	CVAStruct oReturn;

	if( oArgs.HasKey( "basefolder" ) )
	{
		if( oArgs["basefolder"].IsString( ) )
		{
			std::string sNewBaseFolder = oArgs["basefolder"];
			VistaFileSystemDirectory oDir( sNewBaseFolder );
			if( oDir.Exists( ) == false )
				oDir.CreateWithParentDirectories( );
			m_sBaseFolder = sNewBaseFolder;

			VA_INFO( "PrototypeFreeFieldAudioRenderer", "Setting new base folder to '" << m_sBaseFolder << "'" );
		}
	}

	return oReturn;
}
#endif // VACORE_WITH_RENDERER_PROTOTPE_FREE_FIELD
