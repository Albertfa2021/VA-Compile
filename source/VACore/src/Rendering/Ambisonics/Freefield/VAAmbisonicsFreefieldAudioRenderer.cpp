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

#include "VAAmbisonicsFreefieldAudioRenderer.h"

#ifdef VACORE_WITH_RENDERER_AMBISONICS_FREE_FIELD

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
#	include "../../../directivities/VADirectivity.h"

#	include <VA.h>
#	include <VAObjectPool.h>
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


class CVAAFFSoundPath : public CVAPoolObject
{
public:
	virtual ~CVAAFFSoundPath( );

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

	CVAAmbisonicsFreeFieldAudioRenderer::CVAAFFSource* pSource;
	CVAAmbisonicsFreeFieldAudioRenderer::CVAAFFListener* pListener;

	CVASourceTargetMetrics oRelations; //!< Informations on source and receiver relations (distances & angles)

	CDirectivityState oDirectivityStateCur;
	CDirectivityState oDirectivityStateNew;

	std::atomic<bool> bDelete;

	CITAThirdOctaveFilterbank* pThirdOctaveFilterBank;
	CITAVariableDelayLine* pVariableDelayLineCh;


	inline void PreRequest( )
	{
		pSource   = nullptr;
		pListener = nullptr;

		// Reset DSP elements
		pThirdOctaveFilterBank->Clear( );
		pVariableDelayLineCh->Clear( );
		// pFIRConvolverCh->clear();
	};

	void UpdateDir( bool bDIRAuraModeEnabled );
	void UpdateMediumPropagation( double dSpeedOfSound, double dAdditionalDelaySeconds = 0.0f );
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
	CVAAFFSoundPath( );

	//! Konstruktor
	CVAAFFSoundPath( double dSamplerate, int iBlocklength, int iDirFilterLength, int iFilterBankType = CITAThirdOctaveFilterbank::FIR_SPLINE_LINEAR_PHASE );

	EVDLAlgorithm m_iDefaultVDLSwitchingAlgorithm; //!< Umsetzung der Verzgerungsnderung

	friend class CVAAFFSoundPathFactory;
};

class CVAAFFSoundPathFactory : public IVAPoolObjectFactory
{
public:
	CVAAFFSoundPathFactory( double dSamplerate, int iBlocklength, int iDirFilterLength )
	    : m_dSamplerate( dSamplerate )
	    , m_iBlocklength( iBlocklength )
	    , m_iDirFilterLength( iDirFilterLength ) { };

	inline CVAPoolObject* CreatePoolObject( ) { return new CVAAFFSoundPath( m_dSamplerate, m_iBlocklength, m_iDirFilterLength ); };

private:
	double m_dSamplerate;   //!< Abtastrate
	int m_iBlocklength;     //!< Blocklnge
	int m_iDirFilterLength; //!< Filterlnge der Richtcharakteristik
};

class CVAAFFListenerPoolFactory : public IVAPoolObjectFactory
{
public:
	CVAAFFListenerPoolFactory( CVACoreImpl* pCore, const CVAAmbisonicsFreeFieldAudioRenderer::CVAAFFListener::Config& oConf )
	    : m_pCore( pCore )
	    , m_oListenerConf( oConf ) { };

	CVAPoolObject* CreatePoolObject( )
	{
		CVAAmbisonicsFreeFieldAudioRenderer::CVAAFFListener* pListener;
		pListener = new CVAAmbisonicsFreeFieldAudioRenderer::CVAAFFListener( m_pCore, m_oListenerConf );
		return pListener;
	};

private:
	CVACoreImpl* m_pCore;
	const CVAAmbisonicsFreeFieldAudioRenderer::CVAAFFListener::Config& m_oListenerConf;

	//! Not for use, avoid C4512
	CVAAFFListenerPoolFactory operator=( const CVAAFFListenerPoolFactory& ) { VA_EXCEPT_NOT_IMPLEMENTED; }
};

class CVAAFFSourcePoolFactory : public IVAPoolObjectFactory
{
public:
	CVAAFFSourcePoolFactory( const CVAAmbisonicsFreeFieldAudioRenderer::CVAAFFSource::Config& oConf ) : m_oSourceConf( oConf ) { };

	CVAPoolObject* CreatePoolObject( )
	{
		CVAAmbisonicsFreeFieldAudioRenderer::CVAAFFSource* pSource;
		pSource = new CVAAmbisonicsFreeFieldAudioRenderer::CVAAFFSource( m_oSourceConf );
		return pSource;
	};

private:
	const CVAAmbisonicsFreeFieldAudioRenderer::CVAAFFSource::Config& m_oSourceConf;

	//! Not for use, avoid C4512
	CVAAFFSourcePoolFactory operator=( const CVAAFFSourcePoolFactory& ) { VA_EXCEPT_NOT_IMPLEMENTED; }
};

// Renderer

CVAAmbisonicsFreeFieldAudioRenderer::CVAAmbisonicsFreeFieldAudioRenderer( const CVAAudioRendererInitParams& oParams )
    : CVAObject( oParams.sClass + ":" + oParams.sID )
    , m_pCore( oParams.pCore )
    , m_pCurSceneState( nullptr )
    , m_bDumpListeners( false )
    , m_dDumpListenersGain( 1.0 )
    , m_dAdditionalStaticDelaySeconds( 0.0f )
    , m_oParams( oParams )
    , m_iMaxOrder( -1 )
    , m_iNumChannels( -1 )
{
	VA_WARN( "CVAAmbisonicsFreeFieldAudioRenderer",
	         "The renderer class 'AmbisonicsFreeField' is deprecated and might be removed in the future. Use the class 'FreeField' with 'SpatialEncodingType = "
	         "Ambisonics' instead." );

	// read config
	Init( *oParams.pConfig );
	double dSampleRate = m_pCore->GetCoreConfig( )->oAudioDriverConfig.dSampleRate;
	int iBlockLength   = m_pCore->GetCoreConfig( )->oAudioDriverConfig.iBuffersize;


	m_pdsOutput = new ITADatasourceRealization( m_iNumChannels, dSampleRate, iBlockLength );
	m_pdsOutput->SetStreamEventHandler( this );

	IVAPoolObjectFactory* pListenerFactory = new CVAAFFListenerPoolFactory( m_pCore, m_oDefaultListenerConf );
	m_pListenerPool                        = IVAObjectPool::Create( 16, 2, pListenerFactory, true );

	IVAPoolObjectFactory* pSourceFactory = new CVAAFFSourcePoolFactory( m_oDefaultSourceConf );
	m_pSourcePool                        = IVAObjectPool::Create( 16, 2, pSourceFactory, true );

	m_pSoundPathFactory = new CVAAFFSoundPathFactory( m_pCore->GetCoreConfig( )->oAudioDriverConfig.dSampleRate, iBlockLength, 128 );

	m_pSoundPathPool = IVAObjectPool::Create( 64, 8, m_pSoundPathFactory, true );

	m_pUpdateMessagePool = IVAObjectPool::Create( 2, 1, new CVAPoolObjectDefaultFactory<CVAAFFUpdateMessage>, true );

	ctxAudio.m_sbTemp.Init( iBlockLength, true );
	ctxAudio.m_iResetFlag = 0; // Normal operation mode
	ctxAudio.m_iStatus    = 0; // Stopped

	m_iCurGlobalAuralizationMode = IVAInterface::VA_AURAMODE_DEFAULT;

	// Register the renderer as a module
	oParams.pCore->RegisterModule( this );
}

CVAAmbisonicsFreeFieldAudioRenderer::~CVAAmbisonicsFreeFieldAudioRenderer( )
{
	delete m_pSoundPathPool;
	delete m_pUpdateMessagePool;
}

void CVAAmbisonicsFreeFieldAudioRenderer::Init( const CVAStruct& oArgs )
{
	CVAConfigInterpreter conf( oArgs );

	conf.ReqInteger( "TruncationOrder", m_iMaxOrder );
	m_iNumChannels = ( m_iMaxOrder + 1 ) * ( m_iMaxOrder + 1 );


	std::string sReproCenterPos;
	conf.OptString( "ReproductionCenterPos", sReproCenterPos, "AUTO" );
	if( sReproCenterPos == "AUTO" )
	{
		// Mittelpunkt aus der ersten LS Configuration berechnen
		// GetCalculatedReproductionCenterPos(m_v3ReproductionCenterPos);
		VA_WARN( this, "Ambisonics reproduction center set to 0 0 0" );
		m_vReproSystemRealPosition.Set( 0, 0, 0 );
	}
	else
	{
		std::vector<std::string> vsPosComponents = splitString( sReproCenterPos, ',' );
		assert( vsPosComponents.size( ) == 3 );
		m_vReproSystemRealPosition.Set( StringToFloat( vsPosComponents[0] ), StringToFloat( vsPosComponents[1] ), StringToFloat( vsPosComponents[2] ) );
	}
	m_vReproSystemRealUp.Set( 0, 1, 0 );
	m_vReproSystemRealView.Set( 0, 0, -1 );

	std::string sVLDInterpolationAlgorithm;
	conf.OptString( "SwitchingAlgorithm", sVLDInterpolationAlgorithm, "cubicspline" );
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
		ITA_EXCEPT1( INVALID_PARAMETER, "Unrecognized interpolation algorithm '" + sVLDInterpolationAlgorithm + "' in AmbisonicsFreefieldAudioRendererConfig" );

	std::string sFilterBankType;
	conf.OptString( "FilterBankType", sFilterBankType, "iir" );
	if( toLowercase( sFilterBankType ) == "fir" )
		m_iFilterBankType = CITAThirdOctaveFilterbank::FIR_SPLINE_LINEAR_PHASE;
	else
		m_iFilterBankType = CITAThirdOctaveFilterbank::IIR_BIQUADS_ORDER10;

	conf.OptNumber( "AdditionalStaticDelaySeconds", m_dAdditionalStaticDelaySeconds, 0.0f );

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

void CVAAmbisonicsFreeFieldAudioRenderer::Reset( )
{
	VA_VERBOSE( "AmbisonicsFreeFieldAudioRenderer", "Received reset call, indicating reset now" );
	ctxAudio.m_iResetFlag = 1; // Request reset

	if( ctxAudio.m_iStatus == 0 || m_oParams.bOfflineRendering )
	{
		VA_VERBOSE( "AmbisonicsFreeFieldAudioRenderer", "Was not streaming, will reset manually" );
		// if no streaming active, reset manually
		// SyncInternalData();
		ResetInternalData( );
	}
	else
	{
		VA_VERBOSE( "AmbisonicsFreeFieldAudioRenderer", "Still streaming, will now wait for reset acknownledge" );
	}

	// Wait for last streaming block before internal reset
	while( ctxAudio.m_iResetFlag != 2 )
	{
		VASleep( 10 ); // Wait for acknowledge
	}

	VA_VERBOSE( "AmbisonicsFreeFieldAudioRenderer", "Operation reset has green light, clearing items" );

	// Iterate over sound pathes and free items
	std::list<CVAAFFSoundPath*>::iterator it = m_lSoundPaths.begin( );
	while( it != m_lSoundPaths.end( ) )
	{
		CVAAFFSoundPath* pPath = *it;

		int iNumRefs = pPath->GetNumReferences( );
		assert( iNumRefs == 1 );
		pPath->RemoveReference( );

		++it;
	}
	m_lSoundPaths.clear( );

	// Iterate over listener and free items
	std::map<int, CVAAFFListener*>::const_iterator lcit = m_mListeners.begin( );
	while( lcit != m_mListeners.end( ) )
	{
		CVAAFFListener* pListener( lcit->second );
		pListener->pData->RemoveReference( );
		assert( pListener->GetNumReferences( ) == 1 );
		pListener->RemoveReference( );
		lcit++;
	}
	m_mListeners.clear( );

	// Iterate over sources and free items
	std::map<int, CVAAFFSource*>::const_iterator scit = m_mSources.begin( );
	while( scit != m_mSources.end( ) )
	{
		CVAAFFSource* pSource( scit->second );
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

	VA_VERBOSE( "AmbisonicsFreeFieldAudioRenderer", "Reset successful" );
}

void CVAAmbisonicsFreeFieldAudioRenderer::UpdateScene( CVASceneState* pNewSceneState )
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
	m_pUpdateMessage = dynamic_cast<CVAAFFUpdateMessage*>( m_pUpdateMessagePool->RequestObject( ) );

	// Quellen, Hrer und Pfade verwalten
	ManageSoundPaths( m_pCurSceneState, pNewSceneState, &oDiff );

	// Bewegungsinformationen der Quellen und Hrer aktualisieren
	UpdateTrajectories( );

	// Positionen von Hoerer in virtueller Scene aktualisieren
	int iListenerID;
	if( m_pNewSceneState->GetNumSoundReceivers( ) > 0 )
	{
		std::vector<int> viListenerIDs;
		m_pNewSceneState->GetListenerIDs( &viListenerIDs );

		// Use first listener as the user of Ambisonics
		iListenerID             = viListenerIDs[0];

		if( m_pNewSceneState->GetReceiverState( iListenerID ) && m_pNewSceneState->GetReceiverState( iListenerID )->GetMotionState( ) )
		{
			m_vUserPosVirtualScene  = m_pNewSceneState->GetReceiverState( iListenerID )->GetMotionState( )->GetPosition( );
			m_vUserViewVirtualScene = m_pNewSceneState->GetReceiverState( iListenerID )->GetMotionState( )->GetView( );
			m_vUserUpVirtualScene   = m_pNewSceneState->GetReceiverState( iListenerID )->GetMotionState( )->GetUp( );

			VAVec3 p, v, u;
			m_oParams.pCore->GetSoundReceiverRealWorldPositionOrientationVU( iListenerID, p, v, u );
			m_vUserPosRealWorld.Set( float( p.x ), float( p.y ), float( p.z ) );
			m_vUserViewRealWorld.Set( float( v.x ), float( v.y ), float( v.z ) );
			m_vUserUpRealWorld.Set( float( u.x ), float( u.y ), float( u.z ) );

			// Define reproduction system in virtual scene

			m_vUserViewVirtualScene.Norm( );
			m_vUserViewRealWorld.Norm( );
			m_vReproSystemRealView.Norm( );

			m_vUserUpVirtualScene.Norm( );
			m_vUserUpRealWorld.Norm( );
			m_vReproSystemRealUp.Norm( );

			m_vReproSystemVirtualPosition = m_vUserPosVirtualScene - m_vUserPosRealWorld + m_vReproSystemRealPosition;
			m_vReproSystemVirtualView     = m_vUserViewVirtualScene - ( m_vUserViewRealWorld - m_vReproSystemRealView );
			m_vReproSystemVirtualUp       = m_vUserUpVirtualScene - ( m_vUserUpRealWorld - m_vReproSystemRealUp );

			m_vReproSystemVirtualView.Norm( );
			m_vReproSystemVirtualUp.Norm( );
		}
	}

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

ITADatasource* CVAAmbisonicsFreeFieldAudioRenderer::GetOutputDatasource( )
{
	return m_pdsOutput;
}

void CVAAmbisonicsFreeFieldAudioRenderer::ManageSoundPaths( const CVASceneState* pCurScene, const CVASceneState* pNewScene, const CVASceneStateDiff* pDiff )
{
	// Warning: take care for explicit sources and listeners for this renderer!

	// Iterate over current paths and mark deleted (will be removed within internal sync of audio context thread)
	std::list<CVAAFFSoundPath*>::iterator itp = m_lSoundPaths.begin( );
	while( itp != m_lSoundPaths.end( ) )
	{
		CVAAFFSoundPath* pPath( *itp );
		int iSourceID            = pPath->pSource->pData->iID;
		int iListenerID          = pPath->pListener->pData->iID;
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
				const int& iIDListenerDeleted( *citr++ );
				if( iListenerID == iIDListenerDeleted )
				{
					bDeletetionRequired = true; // Listener removed, deletion required
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
		DeleteListener( iID );
	}

	// New sources
	cits = pDiff->viNewSoundSourceIDs.begin( );
	while( cits != pDiff->viNewSoundSourceIDs.end( ) )
	{
		const int& iID( *cits++ );

		// Only add, if no other renderer has been connected explicitly with this source
		const CVASoundSourceDesc* pSoundSourceDesc = m_pCore->GetSceneManager( )->GetSoundSourceDesc( iID );
		if( pSoundSourceDesc->sExplicitRendererID.empty( ) || pSoundSourceDesc->sExplicitRendererID == m_oParams.sID )
			CVAAFFSource* pSource = CreateSource( iID, pNewScene->GetSoundSourceState( iID ) );
	}

	// New receivers
	citr = pDiff->viNewReceiverIDs.begin( );
	while( citr != pDiff->viNewReceiverIDs.end( ) )
	{
		const int& iID( *citr++ );
		CVAAFFListener* pListener = CreateListener( iID, pNewScene->GetReceiverState( iID ) );
	}

	// New paths: (1) new receivers, current sources
	citr = pDiff->viNewReceiverIDs.begin( );
	while( citr != pDiff->viNewReceiverIDs.end( ) )
	{
		int iListenerID           = ( *citr++ );
		CVAAFFListener* pListener = m_mListeners[iListenerID];

		for( size_t i = 0; i < pDiff->viComSoundSourceIDs.size( ); i++ )
		{
			// Only add, if no other renderer has been connected explicitly with this source
			// and only, if not marked for deletion
			int iSourceID                             = pDiff->viComSoundSourceIDs[i];
			std::map<int, CVAAFFSource*>::iterator it = m_mSources.find( iSourceID );
			if( it == m_mSources.end( ) )
				continue; // This source is skipped by the renderer
			CVAAFFSource* pSource = it->second;

			const CVASoundSourceDesc* pSoundSourceDesc = m_pCore->GetSceneManager( )->GetSoundSourceDesc( iSourceID );
			if( !pSource->bDeleted && ( pSoundSourceDesc->sExplicitRendererID.empty( ) || pSoundSourceDesc->sExplicitRendererID == m_oParams.sID ) )
				CVAAFFSoundPath* pPath = CreateSoundPath( pSource, pListener );
		}
	}

	// New paths: (2) new sources, current receivers
	cits = pDiff->viNewSoundSourceIDs.begin( );
	while( cits != pDiff->viNewSoundSourceIDs.end( ) )
	{
		const int& iSourceID( *cits++ );
		std::map<int, CVAAFFSource*>::iterator it = m_mSources.find( iSourceID );
		if( it == m_mSources.end( ) )
			continue; // Explicit source is not connected to this renderer

		CVAAFFSource* pSource = it->second;
		for( size_t i = 0; i < pDiff->viComReceiverIDs.size( ); i++ )
		{
			int iListenerID           = pDiff->viComReceiverIDs[i];
			CVAAFFListener* pListener = m_mListeners[iListenerID];
			if( !pListener->bDeleted )
				CVAAFFSoundPath* pPath = CreateSoundPath( pSource, pListener );
		}
	}

	// New paths: (3) new sources, new receivers
	cits = pDiff->viNewSoundSourceIDs.begin( );
	while( cits != pDiff->viNewSoundSourceIDs.end( ) )
	{
		const int& iSourceID( *cits++ );
		std::map<int, CVAAFFSource*>::iterator it = m_mSources.find( iSourceID );

		if( it == m_mSources.end( ) )
			continue;

		CVAAFFSource* pSource = it->second;
		assert( pSource );

		citr = pDiff->viNewReceiverIDs.begin( );
		while( citr != pDiff->viNewReceiverIDs.end( ) )
		{
			const int& iListenerID( *citr++ );
			CVAAFFListener* pListener = m_mListeners[iListenerID];
			CVAAFFSoundPath* pPath    = CreateSoundPath( pSource, pListener );
		}
	}

	return;
}


double CVAAmbisonicsFreeFieldAudioRenderer::dAzimuthSource2ReproCenter( const CVAMotionState* pMotionState )
{
	// VAVec3 vPosSource2ReceiverVirtualScene = pMotionState->GetPosition() - m_vUserPosVirtualScene;
	return GetAzimuthOnTarget_DEG( m_vReproSystemVirtualPosition, m_vReproSystemVirtualView, m_vReproSystemVirtualUp, pMotionState->GetPosition( ) );
}


double CVAAmbisonicsFreeFieldAudioRenderer::dElevationSource2ReproCenter( const CVAMotionState* pMotionState )
{
	return GetElevationOnTarget_DEG( m_vReproSystemVirtualPosition, m_vReproSystemVirtualUp, pMotionState->GetPosition( ) );
}


void CVAAmbisonicsFreeFieldAudioRenderer::HandleProcessStream( ITADatasourceRealization*, const ITAStreamInfo* pStreamInfo )
{
	if( ctxAudio.m_iStatus == 0 )
	{
		// If streaming is active, set to 1
		ctxAudio.m_iStatus = 1;
	}

	// Schallpfade abgleichen
	SyncInternalData( );
	std::vector<float*> pfOutputCh;

	for( int i = 0; i < m_iNumChannels; i++ )
	{
		float* helper = m_pdsOutput->GetWritePointer( i );
		fm_zero( helper, m_pdsOutput->GetBlocklength( ) );
		pfOutputCh.push_back( helper );
	}

	const CVAAudiostreamState* pStreamState = dynamic_cast<const CVAAudiostreamState*>( pStreamInfo );
	double dListenerTime                    = pStreamState->dSysTime;

	// Check for reset request
	if( ctxAudio.m_iResetFlag == 1 )
	{
		VA_VERBOSE( "AmbisonicsFreeFieldAudioRenderer", "Process stream detecting reset request, will reset internally now" );
		ResetInternalData( );

		return;
	}
	else if( ctxAudio.m_iResetFlag == 2 )
	{
		VA_VERBOSE( "AmbisonicsFreeFieldAudioRenderer", "Process stream detecting ongoing reset, will stop processing here" );

		return;
	}

	SampleTrajectoriesInternal( dListenerTime );

	std::list<CVAAFFListener*>::iterator lit = ctxAudio.m_lListeners.begin( );
	while( lit != ctxAudio.m_lListeners.end( ) )
	{
		CVAAFFListener* pListener( *( lit++ ) );
		pListener->psfOutput->zero( );
	}

	// Update sound pathes
	std::list<CVAAFFSoundPath*>::iterator spit = ctxAudio.m_lSoundPaths.begin( );
	while( spit != ctxAudio.m_lSoundPaths.end( ) )
	{
		CVAAFFSoundPath* pPath( *spit );
		CVAReceiverState* pReceiverState  = ( m_pCurSceneState ? m_pCurSceneState->GetReceiverState( pPath->pListener->pData->iID ) : NULL );
		CVASoundSourceState* pSourceState = ( m_pCurSceneState ? m_pCurSceneState->GetSoundSourceState( pPath->pSource->pData->iID ) : NULL );

		if( pReceiverState == nullptr || pSourceState == nullptr )
		{
			// Skip if no data is present
			spit++;
			continue;
		}

		if( !pPath->pListener->bValidTrajectoryPresent || !pPath->pSource->bValidTrajectoryPresent )
		{
			// Skip if no valid trajectory data is present
			spit++;
			continue;
		}

		// --= Parameter update =--

		pPath->UpdateMetrics( );

		// VDL Doppler shift settings
		bool bDPEnabledGlobal   = ( m_iCurGlobalAuralizationMode & IVAInterface::VA_AURAMODE_DOPPLER ) > 0;
		bool bDPEnabledListener = ( pReceiverState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_DOPPLER ) > 0;
		bool bDPEnabledSource   = ( pSourceState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_DOPPLER ) > 0;
		bool bDPEnabledCurrent  = ( pPath->pVariableDelayLineCh->GetAlgorithm( ) != EVDLAlgorithm::SWITCH ); // switch = disabled
		bool bDPStatusChanged   = ( bDPEnabledCurrent != ( bDPEnabledGlobal && bDPEnabledListener && bDPEnabledSource ) );
		if( bDPStatusChanged )
		{
			pPath->pVariableDelayLineCh->SetAlgorithm( !bDPEnabledCurrent ? m_iDefaultVDLSwitchingAlgorithm : EVDLAlgorithm::SWITCH );
		}
		pPath->UpdateMediumPropagation( m_pCore->oHomogeneousMedium.dSoundSpeed, m_dAdditionalStaticDelaySeconds );

		// Spherical spreading loss
		bool bSSLEnabledSource   = ( pSourceState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_SPREADING_LOSS ) > 0;
		bool bSSLEnabledListener = ( pReceiverState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_SPREADING_LOSS ) > 0;
		bool bSSLEnabledGlobal   = ( m_iCurGlobalAuralizationMode & IVAInterface::VA_AURAMODE_SPREADING_LOSS ) > 0;
		bool bSSLEnabled         = ( bSSLEnabledSource && bSSLEnabledListener && bSSLEnabledGlobal );
		double dDistanceDecrease = pPath->CalculateInverseDistanceDecrease( );
		if( !bSSLEnabled )
			dDistanceDecrease = 1.0f;

		bool bDIREnabledSource   = ( pSourceState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_SOURCE_DIRECTIVITY ) > 0;
		bool bDIREnabledListener = ( pReceiverState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_SOURCE_DIRECTIVITY ) > 0;
		bool bDIREnabledGlobal   = ( m_iCurGlobalAuralizationMode & IVAInterface::VA_AURAMODE_SOURCE_DIRECTIVITY ) > 0;
		bool bDIREnabled         = ( bDIREnabledSource && bDIREnabledListener && bDIREnabledGlobal );
		pPath->UpdateDir( bDIREnabled );


		// Sound source gain / direct sound audibility via AuraMode flags
		bool bDSSourceStatusEnabled   = ( pSourceState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_DIRECT_SOUND );
		bool bDSListenerStatusEnabled = ( pReceiverState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_DIRECT_SOUND );
		bool bDSGlobalStatusEnabled   = ( m_iCurGlobalAuralizationMode & IVAInterface::VA_AURAMODE_DIRECT_SOUND );
		bool bDSEnabled               = bDSSourceStatusEnabled && bDSListenerStatusEnabled && bDSGlobalStatusEnabled;

		float fSoundSourceGain = float( dDistanceDecrease * pSourceState->GetVolume( m_oParams.pCore->GetCoreConfig( )->dDefaultAmplitudeCalibration ) );
		if( pPath->pSource->pData->bMuted || ( bDSEnabled == false ) )
			fSoundSourceGain = 0.0f;


		// --= DSP =--

		CVASoundSourceDesc* pSourceData = pPath->pSource->pData;
		const ITASampleFrame& sbInput( *pSourceData->psfSignalSourceInputBuf ); // Has at least one channel.
		assert( pSourceData->iID >= 0 ); // Knallt es hier, dann wurde dem SoundPath unterm Hintern die Quelle entzogen! -> Problem mit Referenzierung und Reset?


		pPath->pThirdOctaveFilterBank->Process( sbInput[0].GetData( ), ctxAudio.m_sbTemp.data( ) );
		pPath->pVariableDelayLineCh->Process( &( ctxAudio.m_sbTemp ), &( ctxAudio.m_sbTemp ) );

		int iID                            = pPath->pSource->pData->iID;
		CVASoundSourceState* pState        = m_pCurSceneState->GetSoundSourceState( iID );
		const CVAMotionState* pMotionState = pState->GetMotionState( );


		double ddedede = dElevationSource2ReproCenter( pMotionState );
		double dsvdf   = dAzimuthSource2ReproCenter( pMotionState );
		VAVec3 fswdf   = pMotionState->GetPosition( );


		std::vector<double> gains = SHRealvaluedBasefunctions( ( 90 - dElevationSource2ReproCenter( pMotionState ) ) / 180 * 3.14159265359,
		                                                       dAzimuthSource2ReproCenter( pMotionState ) / 180 * 3.14159265359, m_iMaxOrder );

		for( int i = 0; i < m_iNumChannels; i++ )
		{
			( *pPath->pListener->psfOutput )[i].MulAdd( ctxAudio.m_sbTemp, gains[i] * fSoundSourceGain, 0, 0, m_pdsOutput->GetBlocklength( ) );
		}
		spit++;
	}

	for( auto it: ctxAudio.m_lListeners )
	{
		CVAAFFListener* pActiveListener( it );
		for( int i = 0; i < m_iNumChannels; i++ )
			if( !pActiveListener->pData->bMuted )
				fm_add( pfOutputCh[i], ( *pActiveListener->psfOutput )[i].data( ), m_pdsOutput->GetBlocklength( ) ); // initial data should be zero.

		// Listener dumping
		if( m_iDumpListenersFlag > 0 )
		{
			std::map<int, CVAAFFListener*>::iterator it = m_mListeners.begin( );
			while( it != m_mListeners.end( ) )
			{
				CVAAFFListener* pListener = it++->second;
				pListener->psfOutput->mul_scalar( float( m_dDumpListenersGain ) );
				pListener->pListenerOutputAudioFileWriter->write( pListener->psfOutput );
			}

			// Ack on dump stop
			if( m_iDumpListenersFlag == 2 )
				m_iDumpListenersFlag = 0;
		}
	}

	m_pdsOutput->IncrementWritePointer( );

	return;
}

void CVAAmbisonicsFreeFieldAudioRenderer::UpdateTrajectories( )
{
	// Neue Quellendaten bernehmen
	for( std::map<int, CVAAFFSource*>::iterator it = m_mSources.begin( ); it != m_mSources.end( ); ++it )
	{
		int iSourceID         = it->first;
		CVAAFFSource* pSource = it->second;

		CVASoundSourceState* pSourceCur = ( m_pCurSceneState ? m_pCurSceneState->GetSoundSourceState( iSourceID ) : nullptr );
		CVASoundSourceState* pSourceNew = ( m_pNewSceneState ? m_pNewSceneState->GetSoundSourceState( iSourceID ) : nullptr );

		const CVAMotionState* pMotionCur = ( pSourceCur ? pSourceCur->GetMotionState( ) : nullptr );
		const CVAMotionState* pMotionNew = ( pSourceNew ? pSourceNew->GetMotionState( ) : nullptr );

		if( pMotionNew && ( pMotionNew != pMotionCur ) )
		{
			VA_TRACE( "AmbisonicsFreeFieldAudioRenderer", "Source " << iSourceID << " new motion state" );
			pSource->pMotionModel->InputMotionKey( pMotionNew );
		}
	}

	// Neue Hrerdaten bernehmen
	for( std::map<int, CVAAFFListener*>::iterator it = m_mListeners.begin( ); it != m_mListeners.end( ); ++it )
	{
		int iListenerID           = it->first;
		CVAAFFListener* pListener = it->second;

		CVAReceiverState* pListenerCur = ( m_pCurSceneState ? m_pCurSceneState->GetReceiverState( iListenerID ) : nullptr );
		CVAReceiverState* pListenerNew = ( m_pNewSceneState ? m_pNewSceneState->GetReceiverState( iListenerID ) : nullptr );

		const CVAMotionState* pMotionCur = ( pListenerCur ? pListenerCur->GetMotionState( ) : nullptr );
		const CVAMotionState* pMotionNew = ( pListenerNew ? pListenerNew->GetMotionState( ) : nullptr );

		if( pMotionNew && ( pMotionNew != pMotionCur ) )
		{
			VA_TRACE( "AmbisonicsFreeFieldAudioRenderer", "Listener " << iListenerID << " new position " ); // << *pMotionNew);
			pListener->pMotionModel->InputMotionKey( pMotionNew );
		}
	}
}

void CVAAmbisonicsFreeFieldAudioRenderer::SampleTrajectoriesInternal( double dTime )
{
	bool bValid = true;
	for( std::list<CVAAFFSource*>::iterator it = ctxAudio.m_lSources.begin( ); it != ctxAudio.m_lSources.end( ); ++it )
	{
		CVAAFFSource* pSource = *it;

		pSource->pMotionModel->HandleMotionKeys( );
		bValid &= pSource->pMotionModel->EstimatePosition( dTime, pSource->vPredPos );
		bValid &= pSource->pMotionModel->EstimateOrientation( dTime, pSource->vPredView, pSource->vPredUp );
		pSource->bValidTrajectoryPresent = bValid;
	}

	bValid = true;
	for( std::list<CVAAFFListener*>::iterator it = ctxAudio.m_lListeners.begin( ); it != ctxAudio.m_lListeners.end( ); ++it )
	{
		CVAAFFListener* pListener = *it;

		pListener->pMotionModel->HandleMotionKeys( );
		bValid &= pListener->pMotionModel->EstimatePosition( dTime, pListener->vPredPos );
		bValid &= pListener->pMotionModel->EstimateOrientation( dTime, pListener->vPredView, pListener->vPredUp );
		pListener->bValidTrajectoryPresent = bValid;
	}
}

CVAAFFSoundPath* CVAAmbisonicsFreeFieldAudioRenderer::CreateSoundPath( CVAAFFSource* pSource, CVAAFFListener* pListener )
{
	int iSourceID   = pSource->pData->iID;
	int iListenerID = pListener->pData->iID;

	assert( !pSource->bDeleted && !pListener->bDeleted );

	VA_VERBOSE( "AmbisonicsFreeFieldAudioRenderer", "Creating sound path from source " << iSourceID << " -> listener " << iListenerID );

	CVAAFFSoundPath* pPath = dynamic_cast<CVAAFFSoundPath*>( m_pSoundPathPool->RequestObject( ) );

	pPath->pSource   = pSource;
	pPath->pListener = pListener;

	pPath->bDelete = false;

	CVASoundSourceState* pSourceNew = ( m_pNewSceneState ? m_pNewSceneState->GetSoundSourceState( iSourceID ) : nullptr );
	if( pSourceNew != nullptr )
		pPath->oDirectivityStateNew.pData = (IVADirectivity*)pSourceNew->GetDirectivityData( );

	CVAReceiverState* pListenerNew = ( m_pNewSceneState ? m_pNewSceneState->GetReceiverState( iListenerID ) : nullptr );
	// if (pListenerNew != nullptr)
	//	pPath->oHRIRStateNew.pData = (IVAHRIRDataset*)pListenerNew->GetHRIRDataset();

	m_lSoundPaths.push_back( pPath );
	m_pUpdateMessage->vNewPaths.push_back( pPath );

	return pPath;
}

void CVAAmbisonicsFreeFieldAudioRenderer::DeleteSoundPath( CVAAFFSoundPath* pPath )
{
	VA_VERBOSE( "AmbisonicsFreeFieldAudioRenderer",
	            "Marking sound path from source " << pPath->pSource->pData->iID << " -> listener " << pPath->pListener->pData->iID << " for deletion" );

	pPath->bDelete = true;
	pPath->RemoveReference( );
	m_pUpdateMessage->vDelPaths.push_back( pPath );
}

CVAAmbisonicsFreeFieldAudioRenderer::CVAAFFListener* CVAAmbisonicsFreeFieldAudioRenderer::CreateListener( const int iID, const CVAReceiverState* pListenerState )
{
	VA_VERBOSE( "AmbisonicsFreeFieldAudioRenderer", "Creating listener with ID " << iID );

	CVAAFFListener* pListener = dynamic_cast<CVAAFFListener*>( m_pListenerPool->RequestObject( ) ); // Reference = 1

	pListener->pData = m_pCore->GetSceneManager( )->GetSoundReceiverDesc( iID );
	pListener->pData->AddReference( );

	// Move to prerequest of pool?
	pListener->psfOutput = new ITASampleFrame( m_iNumChannels, m_pCore->GetCoreConfig( )->oAudioDriverConfig.iBuffersize, true );
	assert( pListener->pData );
	pListener->bDeleted = false;

	// Motion model
	CVABasicMotionModel* pMotionInstance = dynamic_cast<CVABasicMotionModel*>( pListener->pMotionModel->GetInstance( ) );
	pMotionInstance->SetName( std::string( "bfrend_mm_listener_" + pListener->pData->sName ) );
	pMotionInstance->Reset( );

	m_mListeners.insert( std::pair<int, CVAAmbisonicsFreeFieldAudioRenderer::CVAAFFListener*>( iID, pListener ) );

	m_pUpdateMessage->vNewListeners.push_back( pListener );

	return pListener;
}

void CVAAmbisonicsFreeFieldAudioRenderer::DeleteListener( int iListenerID )
{
	VA_VERBOSE( "AmbisonicsFreeFieldAudioRenderer", "Marking listener with ID " << iListenerID << " for removal" );
	std::map<int, CVAAFFListener*>::iterator it = m_mListeners.find( iListenerID );
	CVAAFFListener* pListener                   = it->second;
	m_mListeners.erase( it );
	pListener->bDeleted = true;
	pListener->pData->RemoveReference( );
	pListener->RemoveReference( );

	m_pUpdateMessage->vDelListeners.push_back( pListener );

	return;
}

CVAAmbisonicsFreeFieldAudioRenderer::CVAAFFSource* CVAAmbisonicsFreeFieldAudioRenderer::CreateSource( int iID, const CVASoundSourceState* pSourceState )
{
	VA_VERBOSE( "AmbisonicsFreeFieldAudioRenderer", "Creating source with ID " << iID );
	CVAAFFSource* pSource = dynamic_cast<CVAAFFSource*>( m_pSourcePool->RequestObject( ) );

	pSource->pData = m_pCore->GetSceneManager( )->GetSoundSourceDesc( iID );
	pSource->pData->AddReference( );

	pSource->bDeleted = false;

	CVABasicMotionModel* pMotionInstance = dynamic_cast<CVABasicMotionModel*>( pSource->pMotionModel->GetInstance( ) );
	pMotionInstance->SetName( std::string( "bfrend_mm_source_" + pSource->pData->sName ) );
	pMotionInstance->Reset( );

	m_mSources.insert( std::pair<int, CVAAFFSource*>( iID, pSource ) );

	m_pUpdateMessage->vNewSources.push_back( pSource );

	return pSource;
}

void CVAAmbisonicsFreeFieldAudioRenderer::DeleteSource( int iSourceID )
{
	VA_VERBOSE( "AmbisonicsFreeFieldAudioRenderer", "Marking source with ID " << iSourceID << " for removal" );
	std::map<int, CVAAFFSource*>::iterator it = m_mSources.find( iSourceID );

	if( it == m_mSources.end( ) ) // Not found in internal list ...
	{
		CVASoundSourceDesc* pDesc = m_pCore->GetSceneManager( )->GetSoundSourceDesc( iSourceID );
		if( !pDesc->sExplicitRendererID.empty( ) || pDesc->sExplicitRendererID == GetObjectName( ) )
			VA_WARN( "AmbisonicsFreeFieldAudioRenderer", "Attempted to remote an explicit sound source for this renderer which could not be found." );
		return;
	}

	CVAAFFSource* pSource = it->second;
	m_mSources.erase( it );
	pSource->bDeleted = true;
	pSource->pData->RemoveReference( );
	pSource->RemoveReference( );

	m_pUpdateMessage->vDelSources.push_back( pSource );

	return;
}

void CVAAmbisonicsFreeFieldAudioRenderer::SyncInternalData( )
{
	CVAAFFUpdateMessage* pUpdate;
	while( ctxAudio.m_qpUpdateMessages.try_pop( pUpdate ) )
	{
		std::list<CVAAFFSoundPath*>::const_iterator citp = pUpdate->vDelPaths.begin( );
		while( citp != pUpdate->vDelPaths.end( ) )
		{
			CVAAFFSoundPath* pPath( *citp++ );
			ctxAudio.m_lSoundPaths.remove( pPath );
			pPath->RemoveReference( );
		}

		citp = pUpdate->vNewPaths.begin( );
		while( citp != pUpdate->vNewPaths.end( ) )
		{
			CVAAFFSoundPath* pPath( *citp++ );
			pPath->AddReference( );
			ctxAudio.m_lSoundPaths.push_back( pPath );
		}

		std::list<CVAAFFSource*>::const_iterator cits = pUpdate->vDelSources.begin( );
		while( cits != pUpdate->vDelSources.end( ) )
		{
			CVAAFFSource* pSource( *cits++ );
			ctxAudio.m_lSources.remove( pSource );
			pSource->pData->RemoveReference( );
			pSource->RemoveReference( );
		}

		cits = pUpdate->vNewSources.begin( );
		while( cits != pUpdate->vNewSources.end( ) )
		{
			CVAAFFSource* pSource( *cits++ );
			pSource->AddReference( );
			pSource->pData->AddReference( );
			ctxAudio.m_lSources.push_back( pSource );
		}

		std::list<CVAAFFListener*>::const_iterator citl = pUpdate->vDelListeners.begin( );
		while( citl != pUpdate->vDelListeners.end( ) )
		{
			CVAAFFListener* pListener( *citl++ );
			ctxAudio.m_lListeners.remove( pListener );
			pListener->pData->RemoveReference( );
			pListener->RemoveReference( );
		}

		citl = pUpdate->vNewListeners.begin( );
		while( citl != pUpdate->vNewListeners.end( ) )
		{
			CVAAFFListener* pListener( *citl++ );
			pListener->AddReference( );
			pListener->pData->AddReference( );
			ctxAudio.m_lListeners.push_back( pListener );
		}

		pUpdate->RemoveReference( );
	}

	return;
}

void CVAAmbisonicsFreeFieldAudioRenderer::ResetInternalData( )
{
	VA_VERBOSE( "AmbisonicsFreeFieldAudioRenderer", "Resetting internally" );

	std::list<CVAAFFSoundPath*>::const_iterator citp = ctxAudio.m_lSoundPaths.begin( );
	while( citp != ctxAudio.m_lSoundPaths.end( ) )
	{
		CVAAFFSoundPath* pPath( *citp++ );
		pPath->RemoveReference( );
	}
	ctxAudio.m_lSoundPaths.clear( );

	std::list<CVAAFFListener*>::const_iterator itl = ctxAudio.m_lListeners.begin( );
	while( itl != ctxAudio.m_lListeners.end( ) )
	{
		CVAAFFListener* pListener( *itl++ );
		pListener->pData->RemoveReference( );
		pListener->RemoveReference( );
	}
	ctxAudio.m_lListeners.clear( );

	std::list<CVAAFFSource*>::const_iterator cits = ctxAudio.m_lSources.begin( );
	while( cits != ctxAudio.m_lSources.end( ) )
	{
		CVAAFFSource* pSource( *cits++ );
		pSource->pData->RemoveReference( );
		pSource->RemoveReference( );
	}
	ctxAudio.m_lSources.clear( );

	ctxAudio.m_iResetFlag = 2; // set ack
}

void CVAAmbisonicsFreeFieldAudioRenderer::UpdateSoundPaths( )
{
	int iGlobalAuralisationMode = m_iCurGlobalAuralizationMode;

	// Check for new data
	std::list<CVAAFFSoundPath*>::iterator it = m_lSoundPaths.begin( );
	while( it != m_lSoundPaths.end( ) )
	{
		CVAAFFSoundPath* pPath( *it++ );

		CVAAFFSource* pSource     = pPath->pSource;
		CVAAFFListener* pListener = pPath->pListener;

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

void CVAAmbisonicsFreeFieldAudioRenderer::UpdateGlobalAuralizationMode( int iGlobalAuralizationMode )
{
	if( m_iCurGlobalAuralizationMode == iGlobalAuralizationMode )
		return;

	m_iCurGlobalAuralizationMode = iGlobalAuralizationMode;

	return;
}


// Class CVAAFFSoundPath

CVAAFFSoundPath::CVAAFFSoundPath( double dSamplerate, int iBlocklength, int iDirFilterLength, int iFilterBankType )
{
	pThirdOctaveFilterBank = CITAThirdOctaveFilterbank::Create( dSamplerate, iBlocklength, iFilterBankType );
	pThirdOctaveFilterBank->SetIdentity( );

	float fReserverdMaxDelaySamples = (float)( 3 * dSamplerate ); // 3 Sekunden ~ 1km Entfernung
	m_iDefaultVDLSwitchingAlgorithm = EVDLAlgorithm::LINEAR_INTERPOLATION;
	pVariableDelayLineCh            = new CITAVariableDelayLine( dSamplerate, iBlocklength, fReserverdMaxDelaySamples, m_iDefaultVDLSwitchingAlgorithm );
}

CVAAFFSoundPath::~CVAAFFSoundPath( )
{
	delete pThirdOctaveFilterBank;
	delete pVariableDelayLineCh;
}

void CVAAFFSoundPath::UpdateMetrics( )
{
	if( pSource->vPredPos != pListener->vPredPos )
		oRelations.Calc( pSource->vPredPos, pSource->vPredView, pSource->vPredUp, pListener->vPredPos, pListener->vPredView, pListener->vPredUp );
}


void CVAAFFSoundPath::UpdateDir( bool bDIRAuraModeEnabled )
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
				std::vector<float> vfMags( ITABase::CThirdOctaveMagnitudeSpectrum::GetNumBands( ) );
				pDirectivityDataNew->getMagnitudes( oDirectivityStateNew.iRecord, 0, &vfMags[0] );
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

void CVAAFFSoundPath::UpdateMediumPropagation( double dSpeedOfSound, double dAdditionalStaticDelaySeconds )
{
	assert( dSpeedOfSound > 0 );

	double dDistanceDelay = oRelations.dDistance / dSpeedOfSound;
	double dDelay         = (std::max)( double( 0.0f ), ( dDistanceDelay + dAdditionalStaticDelaySeconds ) );
	pVariableDelayLineCh->SetDelayTime( float( dDelay ) );
}

double CVAAFFSoundPath::CalculateInverseDistanceDecrease( ) const
{
	// Gain limiter
	const double MINIMUM_DISTANCE = 1 / db20_to_ratio( 10 );

	double dDistance = (std::max)( (double)oRelations.dDistance, MINIMUM_DISTANCE );

	double fInverseDistanceDecrease = 1.0f / dDistance;

	return fInverseDistanceDecrease;
}

CVAStruct CVAAmbisonicsFreeFieldAudioRenderer::CallObject( const CVAStruct& oArgs )
{
	CVAStruct oReturn;
	const CVAStructValue* pStruct;

	if( ( pStruct = oArgs.GetValue( "AdditionalDelaySeconds" ) ) != nullptr )
	{
		if( pStruct->GetDatatype( ) != CVAStructValue::DOUBLE )
			VA_EXCEPT2( INVALID_PARAMETER, "Additional delay must be a double" );

		oReturn["CurrentAdditionalDelaySeconds"] = m_dAdditionalStaticDelaySeconds;
		m_dAdditionalStaticDelaySeconds          = *pStruct;
		oReturn["NewAdditionalDelaySeconds"]     = m_dAdditionalStaticDelaySeconds;

		return oReturn;
	}

	CVAConfigInterpreter oConfig( oArgs );
	std::string sCommandOrg;
	oConfig.ReqNonEmptyString( "Command", sCommandOrg );
	std::string sCommand = toUppercase( sCommandOrg );

	// Command resolution
	if( sCommand == "STARTDUMPLISTENERS" )
	{
		oConfig.OptNumber( "Gain", m_dDumpListenersGain, 1 );
		std::string sFormat;
		oConfig.OptString( "FilenameFormat", sFormat, "Listener$(ListenerID).wav" );
		onStartDumpListeners( sFormat );
		return oReturn;
	}

	if( sCommand == "STOPDUMPLISTENERS" )
	{
		onStopDumpListeners( );
		return oReturn;
	}

	VA_EXCEPT2( INVALID_PARAMETER, "Invalid command (\"" + sCommandOrg + "\")" );
}

void CVAAmbisonicsFreeFieldAudioRenderer::onStartDumpListeners( const std::string& sFilenameFormat )
{
	if( m_bDumpListeners )
		VA_EXCEPT2( MODAL_ERROR, "Listeners dumping already started" );

	std::map<int, CVAAFFListener*>::const_iterator clit = m_mListeners.begin( );
	while( clit != m_mListeners.end( ) )
	{
		CVAAFFListener* pListener( clit++->second );
		pListener->InitDump( sFilenameFormat );
	}

	// Turn dumping on globally
	m_bDumpListeners     = true;
	m_iDumpListenersFlag = 1;
}

void CVAAmbisonicsFreeFieldAudioRenderer::onStopDumpListeners( )
{
	if( !m_bDumpListeners )
		VA_EXCEPT2( MODAL_ERROR, "Listeners dumping not started" );

	// Wait until the audio context ack's dump stop
	m_iDumpListenersFlag = 2;
	while( m_iDumpListenersFlag != 0 )
		VASleep( 20 );

	std::map<int, CVAAFFListener*>::const_iterator clit = m_mListeners.begin( );
	while( clit != m_mListeners.end( ) )
	{
		CVAAFFListener* pListener( clit++->second );
		pListener->FinalizeDump( );
	}

	m_bDumpListeners = false;
}


std::vector<double> CVAAmbisonicsFreeFieldAudioRenderer::vdRealvalued_basefunctions( double elevation, double azimuth, int maxOrder )
{
	std::vector<double> Y;
	Y.resize( ( maxOrder + 1 ) * ( maxOrder + 1 ) );
	Y = dAssociateLegendre( maxOrder, cos( elevation / 180 * 3.14159265359 ) );

	for( int n = 0; n <= maxOrder; n++ )
	{
		for( int m = 1; m <= n; m++ )
		{
			Y[GetIndex( m, n )] *= cos( m * azimuth / 180 * 3.14159265359 );
			Y[GetIndex( -m, n )] *= sin( m * azimuth / 180 * 3.14159265359 );
		}
	}
	return Y;
}

double CVAAmbisonicsFreeFieldAudioRenderer::dNormalizeConst( int m, int n )
{
	double Res = 1;
	if( m % 2 == 1 )
	{
		Res = -1;
	}

	return Res * sqrt( ( 2 * n + 1 ) * ( 2 - iKronecker( m ) ) * factorial( n - m ) / ( 4 * 3.141592565359 * factorial( n + m ) ) );
}

int CVAAmbisonicsFreeFieldAudioRenderer::iKronecker( int m )
{
	if( m == 0 )
		return 1;
	else
		return 0;
}

std::vector<double> CVAAmbisonicsFreeFieldAudioRenderer::dAssociateLegendre( int N, double mu ) // call by ref
{
	assert( abs( mu ) <= 1 );
	double dN = 0;
	std::vector<double> P;
	P.resize( ( N + 1 ) * ( N + 1 ) ); // membervariable
	P[0] = 1.0;                        // *dNormalizeConst(0, 0);
	for( int n = 1; n <= N; n++ )
	{
		P[GetIndex( n, n )]     = ( -( 2 * n - 1 ) * P[GetIndex( ( n - 1 ), ( n - 1 ) )] * sqrt( 1 - ( mu * mu ) ) ); // *dN;
		P[GetIndex( n - 1, n )] = ( 2 * n - 1 ) * mu * P[GetIndex( n - 1, n - 1 )];                                   // *dN; //m-ter Grad
		for( int m = 0; m < ( n - 1 ); m++ )
		{
			P[GetIndex( m, n )] = 1 / ( n - m ) * ( 2 * n - 1 ) * mu * P[GetIndex( m, n - 1 )] - ( n + m - 1 ) * P[GetIndex( m, n - 2 )]; // *dN;
		}
		for( int m = 1; m <= n; m++ )
		{
			P[GetIndex( -m, n )] = P[GetIndex( m, n )];
		}
	}
	for( int n = 0; n < ( N + 1 ); n++ )
	{
		P[GetIndex( 0, n )] *= dNormalizeConst( 0, n );
		for( int m = 1; m <= n; m++ )
		{
			P[GetIndex( m, n )] *= dNormalizeConst( m, n );
			P[GetIndex( -m, n )] = P[GetIndex( m, n )];
		}
	}
	return P;
}


int CVAAmbisonicsFreeFieldAudioRenderer::GetIndex( int m, int n )
{
	return ( n * n + n + m );
}

#endif // VACORE_WITH_RENDERER_AMBISONICS_FREE_FIELD
