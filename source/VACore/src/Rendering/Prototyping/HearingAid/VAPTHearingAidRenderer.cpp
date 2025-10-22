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

#include "VAPTHearingAidRenderer.h"

#ifdef VACORE_WITH_RENDERER_PROTOTYPE_HEARING_AID

// VA includes
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

#	include <VAInterface.h>
#	include <VAObjectPool.h>
#	include <VAReferenceableObject.h>

// ITA includes
#	include <DAFF.h>
#	include <ITAClock.h>
#	include <ITAConfigUtils.h>
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
#	include <VistaInterProcComm/Concurrency/VistaThreadEvent.h>

// 3rdParty includes
#	include <tbb/concurrent_queue.h>

// STL includes
#	include <atomic>
#	include <cassert>
#	include <fstream>
#	include <iomanip>

//! Binaural Freefield sound path
class CVAPTHASoundPath : public CVAPoolObject
{
public:
	virtual ~CVAPTHASoundPath( );

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
		CDirectivityState( ) : pData( NULL ), iRecord( -1 ), bDirectivityEnabled( true ) {}

		IVADirectivity* pData; //!< Directivity data, may be NULL
		int iRecord;           //!< Directivity index
		bool bDirectivityEnabled;

		bool operator==( const CDirectivityState& rhs ) const
		{
			bool bBothEnabled     = ( bDirectivityEnabled == rhs.bDirectivityEnabled );
			bool bSameRecordIndex = ( iRecord == rhs.iRecord );
			bool bSameData        = ( pData == rhs.pData );

			return ( bBothEnabled && bSameRecordIndex && bSameData );
		}
	};

	class CHARIRState
	{
	public:
		CHARIRState( ) : pData( NULL ), iRecord( -1 ), fDistance( 1.0f ) {}

		IVADirectivity* pData; //!< HRIR data, may be NULL
		int iRecord;           //!< HRIR index
		float fDistance;       //!< HRIR dataset distance

		bool operator!=( const CHARIRState& rhs ) const
		{
			if( pData != rhs.pData )
				return true;
			if( fDistance != rhs.fDistance )
				return true;
			if( iRecord != rhs.iRecord )
				return true;
			return false;
		}
	};

	CVAPTHearingAidRenderer::Source* pSource;
	CVAPTHearingAidRenderer::Listener* pListener;

	CVASourceTargetMetrics oRelations; //!< Informatioen on source and receiver relations (distances & angles)

	CDirectivityState oDirectivityStateCur;
	CDirectivityState oDirectivityStateNew;

	CHARIRState oHRIRStateCur;
	CHARIRState oHRIRStateNew;

	std::atomic<bool> bDelete;

	CITAThirdOctaveFilterbank* pThirdOctaveFilterBank;
	CITAVariableDelayLine* pVariableDelayLine;
	ITAUPConvolution* pFIRConvolverChL1;
	ITAUPConvolution* pFIRConvolverChL2;
	ITAUPConvolution* pFIRConvolverChR1;
	ITAUPConvolution* pFIRConvolverChR2;

	void PreRequest( )
	{
		pSource   = nullptr;
		pListener = nullptr;

		// Reset DSP elements
		pThirdOctaveFilterBank->Clear( );
		pVariableDelayLine->Clear( );
		pFIRConvolverChL1->clear( );
		pFIRConvolverChL2->clear( );
		pFIRConvolverChR1->clear( );
		pFIRConvolverChR2->clear( );
	};

	void UpdateDir( bool bDIRAuraModeEnabled );
	void UpdateMediumPropagation( double dSpeedOfSound );
	double CalculateInverseDistanceDecrease( ) const;
	void UpdateHARIR( );

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
	CVAPTHASoundPath( );

	//! Konstruktor
	CVAPTHASoundPath( double dSamplerate, int iBlocklength, int iHARIRFilterLength, int iDirFilterLength, std::vector<int> viHARTFInputchannels, double dRenderingDelay,
	                  int iFilterBankType );

	ITASampleFrame m_sfHARIRTemp;        //!< Intern verwendeter Zwischenspeicher fr HARIR Datentze
	EVDLAlgorithm m_iDefaultVDLSwitchingAlgorithm; //!< Umsetzung der Verzgerungsnderung

	int m_iMaxDaffChannelNumber;  //!< Hchster Kanal fr HARTF
	double m_dRenderingDelayInMs; //!< Delay line, kann zur Synchronisation genutzt werden

	std::vector<int> m_viHARTFInputChannels; //!< Hier sind die Kanle der HARTF-DAFF file gelistet, welche fr den renderer genutzt werden (Anzahl muss mit der Anzahl
	                                         //!< der Kanle bereinstimmen)


	friend class CVAPTHASoundPathFactory;
};

class CVAPTHASoundPathFactory : public IVAPoolObjectFactory
{
public:
	CVAPTHASoundPathFactory( double dSamplerate, int iBlocklength, int iHRIRFilterLength, int iDirFilterLength, std::vector<int> viHARIRchannels, double dRenderingDelay,
	                         int iFilterBankType )
	    : m_dSamplerate( dSamplerate )
	    , m_iBlocklength( iBlocklength )
	    , m_iHARIRFilterLength( iHRIRFilterLength )
	    , m_iDirFilterLength( iDirFilterLength )
	    , m_viHARIRchannels( viHARIRchannels )
	    , m_dRenderingDelayInMs( dRenderingDelay )
	    , m_iFilterBankType( iFilterBankType )
	{
	}

	CVAPoolObject* CreatePoolObject( )
	{
		return new CVAPTHASoundPath( m_dSamplerate, m_iBlocklength, m_iHARIRFilterLength, m_iDirFilterLength, m_viHARIRchannels, m_dRenderingDelayInMs,
		                             m_iFilterBankType );
	}

private:
	double m_dSamplerate;               //!< Abtastrate
	int m_iBlocklength;                 //!< Blocklnge
	int m_iHARIRFilterLength;           //!< Filterlnge der HARIR
	int m_iDirFilterLength;             //!< Filterlnge der Richtcharakteristik
	std::vector<int> m_viHARIRchannels; //!< Liste der gewnschten HARIR Kanle in der DAFF-file (=> bestimmt Anzahl der Kanle)
	double m_dRenderingDelayInMs;       //!< Verzgerung in ms des renderings
	int m_iFilterBankType;              //!< IIR or FIR
};

class CVAPTHAListenerPoolFactory : public IVAPoolObjectFactory
{
public:
	CVAPTHAListenerPoolFactory( CVACoreImpl* pCore, const CVAPTHearingAidRenderer::Listener::Config& oConf ) : m_pCore( pCore ), m_oListenerConf( oConf ) { };

	CVAPoolObject* CreatePoolObject( )
	{
		CVAPTHearingAidRenderer::Listener* pListener;
		pListener = new CVAPTHearingAidRenderer::Listener( m_pCore, m_oListenerConf );
		return pListener;
	};

private:
	CVACoreImpl* m_pCore;
	const CVAPTHearingAidRenderer::Listener::Config& m_oListenerConf;

	//! Not for use, avoid C4512
	inline CVAPTHAListenerPoolFactory operator=( const CVAPTHAListenerPoolFactory& ) { VA_EXCEPT_NOT_IMPLEMENTED; };
};

class CVAPTHASourcePoolFactory : public IVAPoolObjectFactory
{
public:
	CVAPTHASourcePoolFactory( const CVAPTHearingAidRenderer::Source::Config& oConf ) : m_oSourceConf( oConf ) { };

	CVAPoolObject* CreatePoolObject( )
	{
		CVAPTHearingAidRenderer::Source* pSource;
		pSource = new CVAPTHearingAidRenderer::Source( m_oSourceConf );
		return pSource;
	};

private:
	const CVAPTHearingAidRenderer::Source::Config& m_oSourceConf;

	//! Not for use, avoid C4512
	CVAPTHASourcePoolFactory operator=( const CVAPTHASourcePoolFactory& ) { VA_EXCEPT_NOT_IMPLEMENTED; }
};

// Renderer
// oParams.pCore->GetCoreConfig()->oAudioDriverConfig.iOutputChannels
CVAPTHearingAidRenderer::CVAPTHearingAidRenderer( const CVAAudioRendererInitParams& oParams )
    : ITADatasourceRealization( 4, oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.dSampleRate, oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.iBuffersize )
    , CVAObject( oParams.sClass + ":" + oParams.sID )
    , m_pCore( oParams.pCore )
    , m_pCurSceneState( nullptr )
    , m_bDumpListeners( false )
    , m_dDumpListenersGain( 1.0 )
    , m_iHRIRFilterLength( -1 )
    , oParams( oParams )
{
	// read config
	Init( *oParams.pConfig );

	IVAPoolObjectFactory* pListenerFactory = new CVAPTHAListenerPoolFactory( m_pCore, m_oDefaultListenerConf );
	m_pListenerPool                        = IVAObjectPool::Create( 16, 2, pListenerFactory, true );

	IVAPoolObjectFactory* pSourceFactory = new CVAPTHASourcePoolFactory( m_oDefaultSourceConf );
	m_pSourcePool                        = IVAObjectPool::Create( 16, 2, pSourceFactory, true );


	// TODO_LAS: check if number of viHARTFInputChannels > GetNumberOfChannels()
	if( m_viHARIRChannels.size( ) > GetNumberOfChannels( ) )
		VA_EXCEPT2( INVALID_PARAMETER, "CVAPTHearingAidRenderer: Number of HARIR channels is greater than maximum output channels of sound card" );

	m_pSoundPathFactory =
	    new CVAPTHASoundPathFactory( GetSampleRate( ), GetBlocklength( ), m_iHRIRFilterLength, 128, m_viHARIRChannels, m_dRenderingDelayInMs, m_iFilterBankType );

	m_pSoundPathPool = IVAObjectPool::Create( 64, 8, m_pSoundPathFactory, true );

	m_pUpdateMessagePool = IVAObjectPool::Create( 2, 1, new CVAPoolObjectDefaultFactory<UpdateMessage>, true );

	ctxAudio.m_sbTemp.Init( GetBlocklength( ), true );
	ctxAudio.m_iResetFlag = 0; // Normal operation mode
	ctxAudio.m_iStatus    = 0; // Stopped

	m_iCurGlobalAuralizationMode = IVAInterface::VA_AURAMODE_DEFAULT;

	// Register the renderer as a module
	oParams.pCore->RegisterModule( this );
}

CVAPTHearingAidRenderer::~CVAPTHearingAidRenderer( )
{
	delete m_pSoundPathPool;
	delete m_pUpdateMessagePool;
}

void CVAPTHearingAidRenderer::Init( const CVAStruct& oArgs )
{
	CVAConfigInterpreter conf( oArgs );

	// update rates
	conf.OptNumber( "UpdateRateDS", m_dUpdateRateDS, 120.0f );
	conf.OptNumber( "UpdateRateIS", m_dUpdateRateIS, 10.0f );
	conf.OptNumber( "UpdateRateRT", m_dUpdateRateRT, 1.0f );

	// rendering delay
	conf.OptNumber( "RenderingDelayInMs", m_dRenderingDelayInMs, 0.0f );

	// rendering gain
	conf.OptNumber( "RenderingGain", m_dRenderingGain, 1.0f );

	// HARIR filter length
	conf.OptInteger( "HRIRFilterLength", m_iHRIRFilterLength, 256 );

	// channel list
	std::string sChannelList;
	conf.OptString( "InputHARIRchannels", sChannelList, "1, 2, 3, 4" );

	if( sChannelList == "true" ) // hack, VAStruct returns the string 'true' if a number '1' is parsed instead of a string '1'
		m_viHARIRChannels.push_back( 1 );
	else
		m_viHARIRChannels = StringToIntVec( sChannelList );


	// interpolation algorithm
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

	std::string sFilterBankType;
	conf.OptString( "FilterBankType", sFilterBankType, "iir" );
	if( toLowercase( sFilterBankType ) == "fir" )
		m_iFilterBankType = CITAThirdOctaveFilterbank::FIR_SPLINE_LINEAR_PHASE;
	else
		m_iFilterBankType = CITAThirdOctaveFilterbank::IIR_BIQUADS_ORDER10;


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

void CVAPTHearingAidRenderer::Reset( )
{
	ctxAudio.m_iResetFlag = 1; // Request reset

	if( ctxAudio.m_iStatus == 0 || oParams.bOfflineRendering )
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
	std::list<CVAPTHASoundPath*>::iterator it = m_lSoundPaths.begin( );
	while( it != m_lSoundPaths.end( ) )
	{
		CVAPTHASoundPath* pPath = *it;

		int iNumRefs = pPath->GetNumReferences( );
		assert( iNumRefs == 1 );
		pPath->RemoveReference( );

		++it;
	}
	m_lSoundPaths.clear( );

	// Iterate over listener and free items
	std::map<int, Listener*>::const_iterator lcit = m_mListeners.begin( );
	while( lcit != m_mListeners.end( ) )
	{
		Listener* pListener( lcit->second );
		pListener->pData->RemoveReference( );
		assert( pListener->GetNumReferences( ) == 1 );
		pListener->RemoveReference( );
		lcit++;
	}
	m_mListeners.clear( );

	// Iterate over sources and free items
	std::map<int, Source*>::const_iterator scit = m_mSources.begin( );
	while( scit != m_mSources.end( ) )
	{
		Source* pSource( scit->second );
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
}

void CVAPTHearingAidRenderer::UpdateScene( CVASceneState* pNewSceneState )
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
	m_pUpdateMessage = dynamic_cast<UpdateMessage*>( m_pUpdateMessagePool->RequestObject( ) );

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

ITADatasource* CVAPTHearingAidRenderer::GetOutputDatasource( )
{
	return this;
}

void CVAPTHearingAidRenderer::ManageSoundPaths( const CVASceneState* pCurScene, const CVASceneState* pNewScene, const CVASceneStateDiff* pDiff )
{
	// ber aktuelle Pfade iterieren und gelschte markieren
	std::list<CVAPTHASoundPath*>::iterator it = m_lSoundPaths.begin( );
	while( it != m_lSoundPaths.end( ) )
	{
		int iSourceID   = ( *it )->pSource->pData->iID;
		int iListenerID = ( *it )->pListener->pData->iID;
		bool bDelete    = false; // Schallpfad lschen?

		// Schallquelle gelscht? (Quellen-ID in Lschliste)
		for( std::vector<int>::const_iterator cit = pDiff->viDelSoundSourceIDs.begin( ); cit != pDiff->viDelSoundSourceIDs.end( ); ++cit )
		{
			if( iSourceID == ( *cit ) )
			{
				bDelete = true; // Pfad zum Lschen markieren
				break;
			}
		}

		if( !bDelete )
		{
			// Hrer gelscht? (Hrer-ID in Lschliste)
			for( std::vector<int>::const_iterator cit = pDiff->viDelReceiverIDs.begin( ); cit != pDiff->viDelReceiverIDs.end( ); ++cit )
			{
				if( iListenerID == ( *cit ) )
				{
					bDelete = true; // Pfad zum Lschen markieren
					break;
				}
			}
		}

		if( bDelete )
		{
			DeleteSoundPath( *it );
			it = m_lSoundPaths.erase( it );
		}
		else
		{
			++it;
		}
	}

	// ber aktuelle Quellen und Hrer iterieren und gelschte entfernen
	for( std::vector<int>::const_iterator cit = pDiff->viDelSoundSourceIDs.begin( ); cit != pDiff->viDelSoundSourceIDs.end( ); ++cit )
	{
		DeleteSource( *cit );
	}

	for( std::vector<int>::const_iterator cit = pDiff->viDelReceiverIDs.begin( ); cit != pDiff->viDelReceiverIDs.end( ); ++cit )
	{
		DeleteListener( *cit );
	}

	// Neue Quellen anlegen
	for( std::vector<int>::const_iterator scit = pDiff->viNewSoundSourceIDs.begin( ); scit != pDiff->viNewSoundSourceIDs.end( ); ++scit )
	{
		int iSourceID   = ( *scit );
		Source* pSource = CreateSource( iSourceID, pNewScene->GetSoundSourceState( iSourceID ) );
	}

	// Neue Hrer anlegen
	for( std::vector<int>::const_iterator lcit = pDiff->viNewReceiverIDs.begin( ); lcit != pDiff->viNewReceiverIDs.end( ); ++lcit )
	{
		int iListenerID     = ( *lcit );
		Listener* pListener = CreateListener( iListenerID, pNewScene->GetReceiverState( iListenerID ) );
	}

	// Neue Pfade anlegen: (1) Neue Hrer mit aktuellen Quellen
	for( std::vector<int>::const_iterator lcit = pDiff->viNewReceiverIDs.begin( ); lcit != pDiff->viNewReceiverIDs.end( ); ++lcit )
	{
		int iListenerID     = ( *lcit );
		Listener* pListener = m_mListeners[iListenerID];

		for( int i = 0; i < (int)pDiff->viComSoundSourceIDs.size( ); i++ )
		{
			// Neuen Pfad erzeugen zu aktueller Quelle, falls nicht gelscht
			int iSourceID   = pDiff->viComSoundSourceIDs[i];
			Source* pSource = m_mSources[iSourceID];
			if( !pSource->bDeleted )
			{
				CVAPTHASoundPath* pPath = CreateSoundPath( pSource, pListener );
			}
		}
	}

	// Neue Pfade anlegen: (2) Neue Quellen mit aktuellen Hrern
	for( std::vector<int>::const_iterator scit = pDiff->viNewSoundSourceIDs.begin( ); scit != pDiff->viNewSoundSourceIDs.end( ); ++scit )
	{
		int iSourceID   = ( *scit );
		Source* pSource = m_mSources[iSourceID];

		for( int i = 0; i < (int)pDiff->viComReceiverIDs.size( ); i++ )
		{
			// Neuen Pfad erzeugen zu aktueller Quelle, falls nicht gelscht
			int iListenerID     = pDiff->viComReceiverIDs[i];
			Listener* pListener = m_mListeners[iListenerID];
			if( !pListener->bDeleted )
			{
				CVAPTHASoundPath* pPath = CreateSoundPath( pSource, pListener );
			}
		}
	}

	// Neue Pfade anlegen: (3) Neue Quellen mit neuen Hrern
	for( std::vector<int>::const_iterator scit = pDiff->viNewSoundSourceIDs.begin( ); scit != pDiff->viNewSoundSourceIDs.end( ); ++scit )
	{
		int iSourceID   = ( *scit );
		Source* pSource = m_mSources[iSourceID];

		for( std::vector<int>::const_iterator lcit = pDiff->viNewReceiverIDs.begin( ); lcit != pDiff->viNewReceiverIDs.end( ); ++lcit )
		{
			int iListenerID         = ( *lcit );
			Listener* pListener     = m_mListeners[iListenerID];
			CVAPTHASoundPath* pPath = CreateSoundPath( pSource, pListener );
		}
	}

	return;
}

void CVAPTHearingAidRenderer::ProcessStream( const ITAStreamInfo* pStreamInfo )
{
	// If streaming is active, set
	ctxAudio.m_iStatus = 1;

	// Schallpfade abgleichen
	SyncInternalData( );

	float* pfOutputChL1 = GetWritePointer( 0 );
	float* pfOutputChR1 = GetWritePointer( 1 );
	float* pfOutputChL2 = GetWritePointer( 2 );
	float* pfOutputChR2 = GetWritePointer( 3 );

	fm_zero( pfOutputChL1, GetBlocklength( ) );
	fm_zero( pfOutputChL2, GetBlocklength( ) );
	fm_zero( pfOutputChR1, GetBlocklength( ) );
	fm_zero( pfOutputChR2, GetBlocklength( ) );

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

	std::map<int, Listener*>::iterator lit = m_mListeners.begin( );
	while( lit != m_mListeners.end( ) )
	{
		lit->second->psfOutput->zero( );
		lit++;
	}

	// Update sound pathes
	std::list<CVAPTHASoundPath*>::iterator spit = ctxAudio.m_lSoundPaths.begin( );
	while( spit != ctxAudio.m_lSoundPaths.end( ) )
	{
		CVAPTHASoundPath* pPath( *spit );
		CVAReceiverState* pListenerState  = ( m_pCurSceneState ? m_pCurSceneState->GetReceiverState( pPath->pListener->pData->iID ) : NULL );
		CVASoundSourceState* pSourceState = ( m_pCurSceneState ? m_pCurSceneState->GetSoundSourceState( pPath->pSource->pData->iID ) : NULL );

		if( pListenerState == nullptr || pSourceState == nullptr )
		{
			// Skip if no data is present
			spit++;
			continue;
		}

		// --= Parameter update =--

		pPath->UpdateMetrics( );

		// VDL Doppler shift settings
		bool bDPEnabledGlobal   = ( m_iCurGlobalAuralizationMode & IVAInterface::VA_AURAMODE_DOPPLER ) > 0;
		bool bDPEnabledListener = ( pListenerState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_DOPPLER ) > 0;
		bool bDPEnabledSource   = ( pSourceState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_DOPPLER ) > 0;
		bool bDPEnabledCurrent  = ( pPath->pVariableDelayLine->GetAlgorithm( ) != CITAVariableDelayLine::SWITCH ); // switch = disabled
		bool bDPStatusChanged   = ( bDPEnabledCurrent != ( bDPEnabledGlobal && bDPEnabledListener && bDPEnabledSource ) );
		if( bDPStatusChanged )
			pPath->pVariableDelayLine->SetAlgorithm( !bDPEnabledCurrent ? m_iDefaultVDLSwitchingAlgorithm : CITAVariableDelayLine::SWITCH );

		pPath->UpdateMediumPropagation( m_pCore->oHomogeneousMedium.dSoundSpeed );
		double dDistanceDecrease = pPath->CalculateInverseDistanceDecrease( );

		bool bDIREnabledSoure    = ( pSourceState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_SOURCE_DIRECTIVITY ) > 0;
		bool bDIREnabledListener = ( pListenerState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_SOURCE_DIRECTIVITY ) > 0;
		bool bDIREnabledGlobal   = ( m_iCurGlobalAuralizationMode & IVAInterface::VA_AURAMODE_SOURCE_DIRECTIVITY ) > 0;
		bool bDIREnabled         = ( bDIREnabledSoure && bDIREnabledListener && bDIREnabledGlobal );
		pPath->UpdateDir( bDIREnabled );

		pPath->UpdateHARIR( );

		// Sound source gain / direct sound audibility via AuraMode flags
		bool bDSSourceStatusEnabled   = ( pSourceState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_DIRECT_SOUND );
		bool bDSListenerStatusEnabled = ( pListenerState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_DIRECT_SOUND );
		bool bDSGlobalStatusEnabled   = ( m_iCurGlobalAuralizationMode & IVAInterface::VA_AURAMODE_DIRECT_SOUND );
		bool bDSEnabled               = bDSSourceStatusEnabled && bDSListenerStatusEnabled && bDSGlobalStatusEnabled;

		float fSoundSourceGain = float( dDistanceDecrease * pSourceState->GetVolume( m_pCore->GetCoreConfig( )->dDefaultAmplitudeCalibration ) );
		if( pPath->pSource->pData->bMuted || ( bDSEnabled == false ) )
			fSoundSourceGain = 0.0f;
		pPath->pFIRConvolverChL1->SetGain( (float)m_dRenderingGain * fSoundSourceGain );
		pPath->pFIRConvolverChL2->SetGain( (float)m_dRenderingGain * fSoundSourceGain );
		pPath->pFIRConvolverChR1->SetGain( (float)m_dRenderingGain * fSoundSourceGain );
		pPath->pFIRConvolverChR2->SetGain( (float)m_dRenderingGain * fSoundSourceGain );
		// --= DSP =--

		CVASoundSourceDesc* pSourceData = pPath->pSource->pData;
		const ITASampleFrame& sfInputBuffer( *pSourceData->psfSignalSourceInputBuf );
		const ITASampleBuffer* psbInput( &( sfInputBuffer[0] ) );
		assert( psbInput );              // Knallt es hier, dann ist die Eingabequelle noch nicht gesetzt
		assert( pSourceData->iID >= 0 ); // Knallt es hier, dann wurde dem SoundPath unterm hintern die Quelle entzogen! -> Problem mit Referenzierung und Reset?

		pPath->pVariableDelayLine->Process( psbInput, &( ctxAudio.m_sbTemp ) );
		pPath->pThirdOctaveFilterBank->Process( ctxAudio.m_sbTemp.data( ), ctxAudio.m_sbTemp.data( ) ); // inplace
		pPath->pFIRConvolverChL1->Process( ctxAudio.m_sbTemp.data( ), ( *pPath->pListener->psfOutput )[0].data( ), ITABase::MixingMethod::ADD );
		pPath->pFIRConvolverChR1->Process( ctxAudio.m_sbTemp.data( ), ( *pPath->pListener->psfOutput )[1].data( ), ITABase::MixingMethod::ADD );
		pPath->pFIRConvolverChL2->Process( ctxAudio.m_sbTemp.data( ), ( *pPath->pListener->psfOutput )[2].data( ), ITABase::MixingMethod::ADD );
		pPath->pFIRConvolverChR2->Process( ctxAudio.m_sbTemp.data( ), ( *pPath->pListener->psfOutput )[3].data( ), ITABase::MixingMethod::ADD );

		spit++;
	}

	// TODO: Select active listener
	if( !m_mListeners.empty( ) )
	{
		Listener* pActiveListener = m_mListeners.begin( )->second;
		fm_copy( pfOutputChL1, ( *pActiveListener->psfOutput )[0].data( ), m_uiBlocklength );
		fm_copy( pfOutputChR1, ( *pActiveListener->psfOutput )[1].data( ), m_uiBlocklength );
		fm_copy( pfOutputChL2, ( *pActiveListener->psfOutput )[2].data( ), m_uiBlocklength );
		fm_copy( pfOutputChR2, ( *pActiveListener->psfOutput )[3].data( ), m_uiBlocklength );
	}

	// Listener dumping
	if( m_iDumpListenersFlag > 0 )
	{
		for( std::map<int, Listener*>::iterator it = m_mListeners.begin( ); it != m_mListeners.end( ); ++it )
		{
			Listener* pListener = it->second;
			pListener->psfOutput->mul_scalar( float( m_dDumpListenersGain ) );
			pListener->pListenerOutputAudioFileWriter->write( pListener->psfOutput );
		}

		// Sign disable request
		if( m_iDumpListenersFlag == 2 )
			m_iDumpListenersFlag = 0;
	}

	IncrementWritePointer( );

	return;
}

void CVAPTHearingAidRenderer::UpdateTrajectories( )
{
	// Neue Quellendaten bernehmen
	for( std::map<int, Source*>::iterator it = m_mSources.begin( ); it != m_mSources.end( ); ++it )
	{
		int iSourceID   = it->first;
		Source* pSource = it->second;

		CVASoundSourceState* pSourceCur = ( m_pCurSceneState ? m_pCurSceneState->GetSoundSourceState( iSourceID ) : nullptr );
		CVASoundSourceState* pSourceNew = ( m_pNewSceneState ? m_pNewSceneState->GetSoundSourceState( iSourceID ) : nullptr );

		const CVAMotionState* pMotionCur = ( pSourceCur ? pSourceCur->GetMotionState( ) : nullptr );
		const CVAMotionState* pMotionNew = ( pSourceNew ? pSourceNew->GetMotionState( ) : nullptr );

		if( pMotionNew && ( pMotionNew != pMotionCur ) )
		{
			VA_TRACE( "HearingAidRenderer", "Source " << iSourceID << " new motion state" );
			pSource->pMotionModel->InputMotionKey( pMotionNew );
		}
	}

	// Neue Hrerdaten bernehmen
	for( std::map<int, Listener*>::iterator it = m_mListeners.begin( ); it != m_mListeners.end( ); ++it )
	{
		int iListenerID     = it->first;
		Listener* pListener = it->second;

		CVAReceiverState* pListenerCur = ( m_pCurSceneState ? m_pCurSceneState->GetReceiverState( iListenerID ) : nullptr );
		CVAReceiverState* pListenerNew = ( m_pNewSceneState ? m_pNewSceneState->GetReceiverState( iListenerID ) : nullptr );

		const CVAMotionState* pMotionCur = ( pListenerCur ? pListenerCur->GetMotionState( ) : nullptr );
		const CVAMotionState* pMotionNew = ( pListenerNew ? pListenerNew->GetMotionState( ) : nullptr );

		if( pMotionNew && ( pMotionNew != pMotionCur ) )
		{
			VA_TRACE( "HearingAidRenderer", "Listener " << iListenerID << " new position " ); // << *pMotionNew);
			pListener->pMotionModel->InputMotionKey( pMotionNew );
		}
	}
}

void CVAPTHearingAidRenderer::SampleTrajectoriesInternal( const double dTime )
{
	for( std::list<Source*>::iterator it = ctxAudio.m_lSources.begin( ); it != ctxAudio.m_lSources.end( ); ++it )
	{
		Source* pSource = *it;

		pSource->pMotionModel->HandleMotionKeys( );
		pSource->pMotionModel->EstimatePosition( dTime, pSource->vPredPos );
		pSource->pMotionModel->EstimateOrientation( dTime, pSource->vPredView, pSource->vPredUp );
	}

	for( std::list<Listener*>::iterator it = ctxAudio.m_lListener.begin( ); it != ctxAudio.m_lListener.end( ); ++it )
	{
		Listener* pListener = *it;

		pListener->pMotionModel->HandleMotionKeys( );
		pListener->pMotionModel->EstimatePosition( dTime, pListener->vPredPos );
		pListener->pMotionModel->EstimateOrientation( dTime, pListener->vPredView, pListener->vPredUp );
	}
}

CVAPTHASoundPath* CVAPTHearingAidRenderer::CreateSoundPath( Source* pSource, Listener* pListener )
{
	int iSourceID   = pSource->pData->iID;
	int iListenerID = pListener->pData->iID;

	assert( !pSource->bDeleted && !pListener->bDeleted );

	VA_VERBOSE( "HearingAidRenderer", "Creating sound path from source " << iSourceID << " -> listener " << iListenerID );

	// Neuen Pfad holen
	CVAPTHASoundPath* pPath = dynamic_cast<CVAPTHASoundPath*>( m_pSoundPathPool->RequestObject( ) );

	pPath->pSource   = pSource;
	pPath->pListener = pListener;

	pPath->bDelete = false;

	// Richtcharakteristik setzen
	CVASoundSourceState* pSourceNew = ( m_pNewSceneState ? m_pNewSceneState->GetSoundSourceState( iSourceID ) : nullptr );
	if( pSourceNew != nullptr )
	{
		pPath->oDirectivityStateNew.pData = (IVADirectivity*)pSourceNew->GetDirectivityData( );
	}

	// HRIR setzen
	CVAReceiverState* pListenerNew = ( m_pNewSceneState ? m_pNewSceneState->GetReceiverState( iListenerID ) : nullptr );
	if( pListenerNew != nullptr )
	{
		pPath->oHRIRStateNew.pData = (IVADirectivity*)pListenerNew->GetDirectivity( );
	}

	// ...

	// Pfad speichern
	m_lSoundPaths.push_back( pPath );

	// bergabe des neuen Pfades an das Audio-Streaming
	// ctxAudio.m_qpNewSoundPaths.push( pPath );
	m_pUpdateMessage->vNewPaths.push_back( pPath );

	return pPath;
}

void CVAPTHearingAidRenderer::DeleteSoundPath( CVAPTHASoundPath* pPath )
{
	VA_VERBOSE( "HearingAidRenderer",
	            "Marking sound path from source " << pPath->pSource->pData->iID << " -> listener " << pPath->pListener->pData->iID << " for deletion" );

	// Zur Lschung markieren
	pPath->bDelete = true;
	pPath->RemoveReference( );

	// Audio-Streaming den Pfad zum Lschen bergeben
	// ctxAudio.m_qpDelSoundPaths.push( pPath );
	m_pUpdateMessage->vDelPaths.push_back( pPath );
}

CVAPTHearingAidRenderer::Listener* CVAPTHearingAidRenderer::CreateListener( const int iID, const CVAReceiverState* pListenerState )
{
	VA_VERBOSE( "HearingAidRenderer", "Creating listener with ID " << iID );

	Listener* pListener = dynamic_cast<Listener*>( m_pListenerPool->RequestObject( ) ); // Reference = 1

	pListener->pData = m_pCore->GetSceneManager( )->GetSoundReceiverDesc( iID );
	pListener->pData->AddReference( );

	pListener->psfOutput = new ITASampleFrame( m_pCore->GetCoreConfig( )->oAudioDriverConfig.iOutputChannels, // TODO_LAS: get this value from hearing aid renderer config
	                                           m_pCore->GetCoreConfig( )->oAudioDriverConfig.iBuffersize, true );
	assert( pListener->pData );
	pListener->bDeleted = false;

	// Motion model
	CVABasicMotionModel* pMotionInstance = dynamic_cast<CVABasicMotionModel*>( pListener->pMotionModel->GetInstance( ) );
	pMotionInstance->SetName( std::string( "bfrend_mm_listener_" + pListener->pData->sName ) );
	pMotionInstance->Reset( );
	// Set the initial position of the object
	pMotionInstance->InputMotionKey( pListenerState->GetMotionState( ) );

	//

	m_mListeners.insert( std::pair<int, CVAPTHearingAidRenderer::Listener*>( iID, pListener ) );

	// m_vNewListeners.push_back( pListener );
	// ctxAudio.m_qpNewListeners.push( pListener );
	m_pUpdateMessage->vNewListeners.push_back( pListener );

	return pListener;
}

void CVAPTHearingAidRenderer::DeleteListener( int iListenerID )
{
	VA_VERBOSE( "HearingAidRenderer", "Marking listener with ID " << iListenerID << " for removal" );
	std::map<int, Listener*>::iterator it = m_mListeners.find( iListenerID );
	Listener* pListener                   = it->second;
	m_mListeners.erase( it );
	pListener->bDeleted = true;
	pListener->pData->RemoveReference( );
	pListener->RemoveReference( );

	// m_vDelListeners.push_back( pListener );
	// ctxAudio.m_qpDelListeners.push( pListener );
	m_pUpdateMessage->vDelListeners.push_back( pListener );

	return;
}

CVAPTHearingAidRenderer::Source* CVAPTHearingAidRenderer::CreateSource( int iID, const CVASoundSourceState* pSourceState )
{
	VA_VERBOSE( "HearingAidRenderer", "Creating source with ID " << iID );
	Source* pSource = dynamic_cast<Source*>( m_pSourcePool->RequestObject( ) );

	pSource->pData = m_pCore->GetSceneManager( )->GetSoundSourceDesc( iID );
	pSource->pData->AddReference( );

	pSource->bDeleted = false;

	// Motion model
	CVABasicMotionModel* pMotionInstance = dynamic_cast<CVABasicMotionModel*>( pSource->pMotionModel->GetInstance( ) );
	// pMotionInstance->SetName( std::string( "bfrend_mm_source_" + pSource->pData->sName ) );
	pMotionInstance->SetName( std::string( "bfrend_mm_source_" + pSource->pData->sName ) );
	pMotionInstance->Reset( );
	// Set the initial position of the object
	// const CVAMotionState* pMotionState = pSourceState->GetMotionState();
	// pMotionInstance->InputMotionKey( pMotionState );


	m_mSources.insert( std::pair<int, Source*>( iID, pSource ) );

	// m_vNewSources.push_back( pSource );
	// ctxAudio.m_qpNewSources.push( pSource );
	m_pUpdateMessage->vNewSources.push_back( pSource );

	return pSource;
}

void CVAPTHearingAidRenderer::DeleteSource( int iSourceID )
{
	VA_VERBOSE( "HearingAidRenderer", "Marking source with ID " << iSourceID << " for removal" );
	std::map<int, Source*>::iterator it = m_mSources.find( iSourceID );
	Source* pSource                     = it->second;
	m_mSources.erase( it );
	pSource->bDeleted = true;
	pSource->pData->RemoveReference( );
	pSource->RemoveReference( );

	// m_vDelSources.push_back( pSource );
	// ctxAudio.m_qpDelSources.push( pSource );
	m_pUpdateMessage->vDelSources.push_back( pSource );

	return;
}

void CVAPTHearingAidRenderer::SyncInternalData( )
{
	UpdateMessage* pUpdate;
	while( ctxAudio.m_qpUpdateMessages.try_pop( pUpdate ) )
	{
		for( std::vector<CVAPTHASoundPath*>::iterator it = pUpdate->vDelPaths.begin( ); it != pUpdate->vDelPaths.end( ); ++it )
		{
			CVAPTHASoundPath* pPath( *it );
			ctxAudio.m_lSoundPaths.remove( *it );
			pPath->RemoveReference( );
		}

		for( std::vector<CVAPTHASoundPath*>::iterator it = pUpdate->vNewPaths.begin( ); it != pUpdate->vNewPaths.end( ); ++it )
		{
			CVAPTHASoundPath* pPath( *it );
			pPath->AddReference( );
			ctxAudio.m_lSoundPaths.push_back( pPath );
		}

		for( std::vector<Source*>::iterator it = pUpdate->vDelSources.begin( ); it != pUpdate->vDelSources.end( ); ++it )
		{
			Source* pSource( *it );
			ctxAudio.m_lSources.remove( pSource );
			pSource->pData->RemoveReference( );
			pSource->RemoveReference( );
		}

		for( std::vector<Source*>::iterator it = pUpdate->vNewSources.begin( ); it != pUpdate->vNewSources.end( ); ++it )
		{
			Source* pSource( *it );
			pSource->AddReference( );
			pSource->pData->AddReference( );
			ctxAudio.m_lSources.push_back( pSource );
		}

		for( std::vector<Listener*>::iterator it = pUpdate->vDelListeners.begin( ); it != pUpdate->vDelListeners.end( ); ++it )
		{
			Listener* pListener( *it );
			ctxAudio.m_lListener.remove( pListener );
			pListener->pData->RemoveReference( );
			pListener->RemoveReference( );
		}

		for( std::vector<Listener*>::iterator it = pUpdate->vNewListeners.begin( ); it != pUpdate->vNewListeners.end( ); ++it )
		{
			Listener* pListener( *it );
			pListener->AddReference( );
			pListener->pData->AddReference( );
			ctxAudio.m_lListener.push_back( pListener );
		}

		pUpdate->RemoveReference( );
	}

	/*
	// Neue Schallpfade ermitteln
	CBFSoundPath* pPath( nullptr );
	while( ctxAudio.m_qpNewSoundPaths.try_pop( pPath ) )
	{
	pPath->AddReference();
	ctxAudio.m_lSoundPaths.push_back( pPath );
	}

	// Verwaiste Schallpfade freigeben (automatische Verwaltung durch den Pool)
	while( ctxAudio.m_qpDelSoundPaths.try_pop( pPath ) )
	{
	ctxAudio.m_lSoundPaths.remove( pPath );
	pPath->RemoveReference();
	}

	// Neue Quellen ermitteln
	Source* pSource( nullptr );
	while( ctxAudio.m_qpNewSources.try_pop( pSource ) )
	{
	pSource->AddReference();
	pSource->pData->AddReference();
	ctxAudio.m_lSources.push_back( pSource );
	}

	// Verwaiste Quellen freigeben (automatische Verwaltung durch den Pool)
	while( ctxAudio.m_qpDelSources.try_pop( pSource ) )
	{

	}

	// Neue Hrer ermitteln
	Listener* pListener( nullptr );
	while( ctxAudio.m_qpNewListeners.try_pop( pListener ) )
	{
	pListener->AddReference();
	pListener->pData->AddReference();
	ctxAudio.m_lListener.push_back( pListener );
	}

	// Verwaiste Hrer freigeben (automatische Verwaltung durch den Pool)
	while( ctxAudio.m_qpDelListeners.try_pop( pListener ) )
	{
	ctxAudio.m_lListener.remove( pListener );
	pListener->pData->RemoveReference();
	pListener->RemoveReference();
	}
	*/

	return;
}

void CVAPTHearingAidRenderer::ResetInternalData( )
{
	// Referenzen auf Schallpfade aus der Streaming-Liste entfernen
	for( std::list<CVAPTHASoundPath*>::iterator it = ctxAudio.m_lSoundPaths.begin( ); it != ctxAudio.m_lSoundPaths.end( ); ++it )
	{
		CVAPTHASoundPath* pPath = *it;
		pPath->RemoveReference( );
	}

	ctxAudio.m_lSoundPaths.clear( );

	// Referenzen auf Hrer aus der Streaming-Liste entfernen
	for( std::list<Listener*>::iterator it = ctxAudio.m_lListener.begin( ); it != ctxAudio.m_lListener.end( ); ++it )
	{
		Listener* pListener = *it;
		pListener->pData->RemoveReference( );
		pListener->RemoveReference( );
	}
	ctxAudio.m_lListener.clear( );

	// Referenzen auf Quellen aus der Streaming-Liste entfernen
	for( std::list<Source*>::iterator it = ctxAudio.m_lSources.begin( ); it != ctxAudio.m_lSources.end( ); ++it )
	{
		Source* pSource = *it;
		pSource->pData->RemoveReference( );
		pSource->RemoveReference( );
	}
	ctxAudio.m_lSources.clear( );

	ctxAudio.m_iResetFlag = 2; // set ack
}

void CVAPTHearingAidRenderer::UpdateSoundPaths( )
{
	int iGlobalAuralisationMode = m_iCurGlobalAuralizationMode;

	// Check for new data
	std::list<CVAPTHASoundPath*>::iterator it = m_lSoundPaths.begin( );
	while( it != m_lSoundPaths.end( ) )
	{
		CVAPTHASoundPath* pPath( *it );

		Source* pSource     = pPath->pSource;
		Listener* pListener = pPath->pListener;

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

		if( pListenerNew == nullptr )
		{
			pPath->oHRIRStateNew.pData = nullptr;
		}
		else
		{
			pPath->oHRIRStateNew.pData = (IVADirectivity*)pListenerNew->GetDirectivity( );
		}
		it++;
	}

	return;
}

void CVAPTHearingAidRenderer::UpdateGlobalAuralizationMode( int iGlobalAuralizationMode )
{
	if( m_iCurGlobalAuralizationMode == iGlobalAuralizationMode )
		return;

	m_iCurGlobalAuralizationMode = iGlobalAuralizationMode;

	return;
}


// Class CHASoundPath

CVAPTHASoundPath::CVAPTHASoundPath( double dSamplerate, int iBlocklength, int iHRIRFilterLength, int iDirFilterLength, std::vector<int> viHARIRchannels,
                                    double dRenderingDelay, int iFilterBankType )
    : m_dRenderingDelayInMs( dRenderingDelay )
{
	m_viHARTFInputChannels = viHARIRchannels;

	// get highest channel number from m_viHARTFInputChannels
	m_iMaxDaffChannelNumber = 1;
	for( unsigned int i = 0; i < m_viHARTFInputChannels.size( ); i++ )
		if( m_viHARTFInputChannels[i] > m_iMaxDaffChannelNumber )
			m_iMaxDaffChannelNumber = m_viHARTFInputChannels[i];

	pThirdOctaveFilterBank = CITAThirdOctaveFilterbank::Create( dSamplerate, iBlocklength, iFilterBankType );
	pThirdOctaveFilterBank->SetIdentity( );

	float fReserverdMaxDelaySamples = (float)( 3 * dSamplerate ); // 3 Sekunden ~ 1km Entfernung
	m_iDefaultVDLSwitchingAlgorithm = CITAVariableDelayLine::CUBIC_SPLINE_INTERPOLATION;
	pVariableDelayLine              = new CITAVariableDelayLine( dSamplerate, iBlocklength, fReserverdMaxDelaySamples, m_iDefaultVDLSwitchingAlgorithm );

	pFIRConvolverChL1 = new ITAUPConvolution( iBlocklength, iHRIRFilterLength );
	pFIRConvolverChL1->SetFilterExchangeFadingFunction( ITABase::FadingFunction::COSINE_SQUARE );
	pFIRConvolverChL1->SetFilterCrossfadeLength( (std::min)( iBlocklength, 32 ) );
	pFIRConvolverChL1->SetGain( 0, true );
	ITAUPFilter* pHRIRFilterChL1 = pFIRConvolverChL1->RequestFilter( );
	pHRIRFilterChL1->identity( );
	pFIRConvolverChL1->ExchangeFilter( pHRIRFilterChL1 );

	pFIRConvolverChL2 = new ITAUPConvolution( iBlocklength, iHRIRFilterLength );
	pFIRConvolverChL2->SetFilterExchangeFadingFunction( ITABase::FadingFunction::COSINE_SQUARE );
	pFIRConvolverChL2->SetFilterCrossfadeLength( (std::min)( iBlocklength, 32 ) );
	pFIRConvolverChL2->SetGain( 0, true );
	ITAUPFilter* pHRIRFilterChL2 = pFIRConvolverChL2->RequestFilter( );
	pHRIRFilterChL2->identity( );
	pFIRConvolverChL2->ExchangeFilter( pHRIRFilterChL2 );

	pFIRConvolverChR1 = new ITAUPConvolution( iBlocklength, iHRIRFilterLength );
	pFIRConvolverChR1->SetFilterExchangeFadingFunction( ITABase::FadingFunction::COSINE_SQUARE );
	pFIRConvolverChR1->SetFilterCrossfadeLength( (std::min)( iBlocklength, 32 ) );
	pFIRConvolverChR1->SetGain( 0, true );
	ITAUPFilter* pHRIRFilterChR1 = pFIRConvolverChR1->RequestFilter( );
	pHRIRFilterChR1->identity( );
	pFIRConvolverChR1->ExchangeFilter( pHRIRFilterChR1 );

	pFIRConvolverChR2 = new ITAUPConvolution( iBlocklength, iHRIRFilterLength );
	pFIRConvolverChR2->SetFilterExchangeFadingFunction( ITABase::FadingFunction::COSINE_SQUARE );
	pFIRConvolverChR2->SetFilterCrossfadeLength( (std::min)( iBlocklength, 32 ) );
	pFIRConvolverChR2->SetGain( 0, true );
	ITAUPFilter* pHRIRFilterChR2 = pFIRConvolverChR2->RequestFilter( );
	pHRIRFilterChR2->identity( );
	pFIRConvolverChR2->ExchangeFilter( pHRIRFilterChR2 );


	// Auto-release filter after it is not used anymore
	pFIRConvolverChL1->ReleaseFilter( pHRIRFilterChL1 );
	pFIRConvolverChL2->ReleaseFilter( pHRIRFilterChL2 );
	pFIRConvolverChR1->ReleaseFilter( pHRIRFilterChR1 );
	pFIRConvolverChR2->ReleaseFilter( pHRIRFilterChR2 );

	m_sfHARIRTemp.init( m_iMaxDaffChannelNumber, iHRIRFilterLength, false );
}

CVAPTHASoundPath::~CVAPTHASoundPath( )
{
	delete pThirdOctaveFilterBank;
	delete pVariableDelayLine;
	delete pFIRConvolverChL1;
	delete pFIRConvolverChR1;
	delete pFIRConvolverChL2;
	delete pFIRConvolverChR2;
}

void CVAPTHASoundPath::UpdateMetrics( )
{
	if( pSource->vPredPos != pListener->vPredPos )
		oRelations.Calc( pSource->vPredPos, pSource->vPredView, pSource->vPredUp, pListener->vPredPos, pListener->vPredView, pListener->vPredUp );
}


void CVAPTHASoundPath::UpdateDir( bool bDIRAuraModeEnabled )
{
	if( bDIRAuraModeEnabled == false )
	{
		if( oDirectivityStateNew.bDirectivityEnabled != false )
		{
			pThirdOctaveFilterBank->SetIdentity( );
			oDirectivityStateNew.bDirectivityEnabled = false;
		}
	}
	else
	{
		// Apply changes
		DAFFContentMS* pDirectivityDataNew = (DAFFContentMS*)oDirectivityStateNew.pData;
		DAFFContentMS* pDirectivityDataCur = (DAFFContentMS*)oDirectivityStateCur.pData;

		if( pDirectivityDataNew == nullptr )
		{
			if( pDirectivityDataCur != nullptr )
				pThirdOctaveFilterBank->SetIdentity( ); // set identity once
		}

		if( pDirectivityDataNew != nullptr )
		{
			pDirectivityDataNew->getNearestNeighbour( DAFF_OBJECT_VIEW, float( oRelations.dAzimuthS2T ), float( oRelations.dElevationS2T ),
			                                          oDirectivityStateNew.iRecord );
			if( oDirectivityStateCur.iRecord != oDirectivityStateNew.iRecord )
			{
				std::vector<float> vfMags( ITABase::CThirdOctaveMagnitudeSpectrum::GetNumBands( ) );
				pDirectivityDataNew->getMagnitudes( oDirectivityStateNew.iRecord, 0, &vfMags[0] );

				ITABase::CThirdOctaveGainMagnitudeSpectrum oDirectivityMagnitudes;
				oDirectivityMagnitudes.SetMagnitudes( vfMags );
				pThirdOctaveFilterBank->SetMagnitudes( oDirectivityMagnitudes );
			}
		}
	}

	// Acknowledge new state
	oDirectivityStateCur = oDirectivityStateNew;

	return;
}

void CVAPTHASoundPath::UpdateMediumPropagation( double dSpeedOfSound )
{
	assert( dSpeedOfSound > 0 );

	double dDelay = oRelations.dDistance / dSpeedOfSound;                                           // TODO_LAS: delay OFFSET
	pVariableDelayLine->SetDelayTime( float( dDelay ) + float( m_dRenderingDelayInMs ) / 1000.0f ); // including additional delay for rendering
}

double CVAPTHASoundPath::CalculateInverseDistanceDecrease( ) const
{
	// Gain limiter
	const double MINIMUM_DISTANCE = 1 / db20_to_ratio( 10 );

	double dDistance = (std::max)( (double)oRelations.dDistance, MINIMUM_DISTANCE );

	float fInverseDistanceDecrease = (float)( 1.0f / dDistance );

	return fInverseDistanceDecrease;
}

void CVAPTHASoundPath::UpdateHARIR( )
{
	CVADirectivityDAFFHRIR* pHRIRData = (CVADirectivityDAFFHRIR*)oHRIRStateNew.pData;
	if( pHRIRData != nullptr )
		pHRIRData->GetNearestNeighbour( float( oRelations.dAzimuthT2S ), float( oRelations.dElevationT2S ), &oHRIRStateNew.iRecord );
	else
		oHRIRStateNew.iRecord = -1;

	if( oHRIRStateCur != oHRIRStateNew )
	{
		ITAUPFilter* pHRIRFilterChL1 = pFIRConvolverChL1->GetFilterPool( )->RequestFilter( );
		ITAUPFilter* pHRIRFilterChL2 = pFIRConvolverChL2->GetFilterPool( )->RequestFilter( );
		ITAUPFilter* pHRIRFilterChR1 = pFIRConvolverChR1->GetFilterPool( )->RequestFilter( );
		ITAUPFilter* pHRIRFilterChR2 = pFIRConvolverChR2->GetFilterPool( )->RequestFilter( );

		if( pHRIRData == nullptr )
		{
			pHRIRFilterChL1->identity( );
			pHRIRFilterChL2->identity( );
			pHRIRFilterChR1->identity( );
			pHRIRFilterChR2->identity( );
		}
		else
		{
			int iNewFilterLength = pHRIRData->GetProperties( )->iFilterLength;
			if( m_sfHARIRTemp.length( ) != iNewFilterLength )
			{
				m_sfHARIRTemp.init( m_iMaxDaffChannelNumber, iNewFilterLength, false );
			}

			if( iNewFilterLength > pFIRConvolverChL1->GetMaxFilterlength( ) )
			{
				VA_WARN( "CHASoundPath", "HARIR too long for convolver, cropping. Increase HARIR filter length in HA-Renderer configuration." );
				iNewFilterLength = pFIRConvolverChL1->GetMaxFilterlength( );
			}

			pHRIRData->GetHRIRByIndex( &m_sfHARIRTemp, oHRIRStateNew.iRecord, oHRIRStateNew.fDistance );

			// assign HARTFs according to vector "InputHARIRchannels" in VACore.ini
			pHRIRFilterChL1->Load( m_sfHARIRTemp[m_viHARTFInputChannels[0] - 1].data( ), iNewFilterLength );
			pHRIRFilterChR1->Load( m_sfHARIRTemp[m_viHARTFInputChannels[1] - 1].data( ), iNewFilterLength );
			pHRIRFilterChL2->Load( m_sfHARIRTemp[m_viHARTFInputChannels[2] - 1].data( ), iNewFilterLength );
			pHRIRFilterChR2->Load( m_sfHARIRTemp[m_viHARTFInputChannels[3] - 1].data( ), iNewFilterLength );
		}

		pFIRConvolverChL1->ExchangeFilter( pHRIRFilterChL1 );
		pFIRConvolverChL2->ExchangeFilter( pHRIRFilterChL2 );
		pFIRConvolverChR1->ExchangeFilter( pHRIRFilterChR1 );
		pFIRConvolverChR2->ExchangeFilter( pHRIRFilterChR2 );


		pFIRConvolverChL1->ReleaseFilter( pHRIRFilterChL1 );
		pFIRConvolverChL2->ReleaseFilter( pHRIRFilterChL2 );
		pFIRConvolverChR1->ReleaseFilter( pHRIRFilterChR1 );
		pFIRConvolverChR2->ReleaseFilter( pHRIRFilterChR2 );

		// Ack
		oHRIRStateCur = oHRIRStateNew;
	}

	return;
}

CVAStruct CVAPTHearingAidRenderer::CallObject( const CVAStruct& oArgs )
{
	CVAStruct oReturn;
	CVAConfigInterpreter oConfig( oArgs );

	const CVAStructValue* pStruct;


	if( ( pStruct = oArgs.GetValue( "PRINT" ) ) != nullptr )
	{
		VA_PRINT( "Available commands for " + CVAObject::GetObjectName( ) + ": print, set, get" );
		VA_PRINT( "PRINT:" );
		VA_PRINT( "\t'print', 'help'" );
		VA_PRINT( "GET:" );
		VA_PRINT( "\t'get', 'GAIN'" );
		VA_PRINT( "SET:" );
		VA_PRINT( "\t'set', 'GAIN', 'value', <number>" );

		oReturn["Return"] = true; // dummy return value, otherwise Problem with MATLAB
		return oReturn;
	}
	else if( ( pStruct = oArgs.GetValue( "GET" ) ) != nullptr )
	{
		if( pStruct->GetDatatype( ) != CVAStructValue::STRING )
			VA_EXCEPT2( INVALID_PARAMETER, "GET command must be a string" );
		std::string sGetCommand = toUppercase( *pStruct );

		if( sGetCommand == "GAIN" )
		{
			oReturn["RETURN"] = m_dRenderingGain;
			return oReturn;
		}
		else
		{
			VA_EXCEPT2( INVALID_PARAMETER, "Unrecognized GET command " + sGetCommand );
		}
	}
	else if( ( pStruct = oArgs.GetValue( "SET" ) ) != nullptr )
	{
		if( pStruct->GetDatatype( ) != CVAStructValue::STRING )
			VA_EXCEPT2( INVALID_PARAMETER, "SET value must be a string" );

		std::string sSetKey = toUppercase( *pStruct );

		if( sSetKey == "GAIN" )
		{
			const CVAStructValue* pGainValue = oArgs.GetValue( "VALUE" );
			if( pGainValue != nullptr )
			{
				if( pGainValue->GetDatatype( ) != CVAStructValue::DOUBLE )
					VA_EXCEPT2( INVALID_PARAMETER, "Gain value must be numerical" );

				m_dRenderingGain = *pGainValue;

				oReturn["Return"] = true; // dummy return value, otherwise Problem with MATLAB
				return oReturn;
			}
		}
		else
		{
			VA_EXCEPT2( INVALID_PARAMETER, "Unrecognized SET value" );
		}
	}
	else
	{
		VA_EXCEPT2( INVALID_PARAMETER, "Unrecognized command called, use 'PRINT' 'HELP' for more information" );
	}

	return oReturn;
}

void CVAPTHearingAidRenderer::onStartDumpListeners( const std::string& sFilenameFormat )
{
	if( m_bDumpListeners )
		VA_EXCEPT2( MODAL_ERROR, "Listeners dumping already started" );

	// Initialize dumping for all listeners
	for( std::map<int, Listener*>::iterator lit = m_mListeners.begin( ); lit != m_mListeners.end( ); ++lit )
		lit->second->InitDump( sFilenameFormat );

	// Turn dumping on globally
	m_bDumpListeners     = true;
	m_iDumpListenersFlag = 1;
}

void CVAPTHearingAidRenderer::onStopDumpListeners( )
{
	if( !m_bDumpListeners )
		VA_EXCEPT2( MODAL_ERROR, "Listeners dumping not started" );

	// Wait until the audio context is finished
	m_iDumpListenersFlag = 2;
	while( m_iDumpListenersFlag != 0 )
		VASleep( 1 );

	// Finalize dumping for all listeners
	for( std::map<int, Listener*>::iterator lit = m_mListeners.begin( ); lit != m_mListeners.end( ); ++lit )
		lit->second->FinalizeDump( );

	m_bDumpListeners = false;
}

#endif // VACORE_WITH_RENDERER_PROTOTYPE_HEARING_AID
