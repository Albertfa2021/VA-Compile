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

#include "core.h"

IVAInterface* VACore::CreateCoreInstance( const CVAStruct& oArgs, std::ostream* pOutputStream )
{
	VA_TRACE( "Config", oArgs );
	return new CVACoreImpl( oArgs, pOutputStream );
}

CVACoreImpl::CVACoreImpl( const CVAStruct& oArgs, std::ostream* pOutputStream )
    : m_pAudioDriverBackend( nullptr )
    , m_pGlobalSamplePool( nullptr )
    , m_pGlobalSampler( nullptr )
    , m_pSignalSourceManager( nullptr )
    , m_pDirectivityManager( nullptr )
    , m_pSceneManager( nullptr )
    , m_pNewSceneState( nullptr )
    , m_pEventManager( nullptr )
    , m_pCoreThread( nullptr )
    , m_pInputAmp( nullptr )
    , m_pR2RPatchbay( nullptr )
    , m_pOutputPatchbay( nullptr )
    , m_pInputStreamDetector( nullptr )
    , m_pOutputStreamDetector( nullptr )
    , m_pOutputTracker( nullptr )
    , m_pStreamProbeDeviceInput( nullptr )
    , m_pStreamProbeDeviceOutput( nullptr )
    , m_pCurSceneState( nullptr )
    , m_pClock( ITAClock::getDefaultClock( ) )
    , m_pTicker( NULL )
    , m_lSyncModOwner( -1 )
    , m_lSyncModSpinCount( 0 )
    , m_iState( VA_CORESTATE_CREATED )
    , m_iGlobalAuralizationMode( IVAInterface::VA_AURAMODE_ALL )
    , m_dOutputGain( 1 )
    , m_dInputGain( 1 )
    , m_bOutputMuted( false )
    , m_bInputMuted( false )
    , m_dStreamClockOffset( 0 )
    , m_fCoreClockOffset( 0 )
    , m_oCoreThreadLoopTotalDuration( "Core thread loop" )
{
	VA_NO_REENTRANCE;

	if( pOutputStream )
		SetOutputStream( pOutputStream );

	VA_TRY
	{
		// read configuration
		m_oCoreConfig.Init( oArgs );

		// register core itself as a module
		SetObjectName( "VACore" );
		m_oModules.RegisterObject( this );
		VA_VERBOSE( "Core", "Registered core module with name '" << GetObjectName( ) << "'" );

		// Der Event-Manager muss immer verfügbar sein,
		// unabhänging davon ob der Core initialisiert wurde oder nicht.
		m_pEventManager = new CVACoreEventManager;

		m_iState = VA_CORESTATE_CREATED;

		VA_TRACE( "Core", "CVACoreImpl instance created [" << this << "]" );
	}
	VA_RETHROW;
}

CVACoreImpl::~CVACoreImpl( )
{
	VA_NO_REENTRANCE;

	// Implizit finalisieren, falls dies nicht durch den Benutzer geschah
	if( m_iState == VA_CORESTATE_READY )
	{
		VA_TRY { Finalize( ); }
		VA_FINALLY {
			// Fehler beim Finalisieren ignorieren
		};
	}

	// Nachricht senden [blocking], das die Kerninstanz gelöscht wird.
	CVAEvent ev;
	ev.iEventType = CVAEvent::DESTROY;
	ev.pSender    = this;
	m_pEventManager->BroadcastEvent( ev );

	// Module deregistrieren
	m_oModules.Clear( );

	// Nachrichten-Manager freigeben
	VA_TRY { delete m_pEventManager; }
	VA_RETHROW;

	VA_TRACE( "Core", "CVACoreImpl instance deleted [" << this << "]" );

	// Profiling ausgeben
	VA_VERBOSE( "Core", m_oCoreThreadLoopTotalDuration.ToString( ) );
}

void CVACoreImpl::SetOutputStream( std::ostream* posDebug )
{
	VALog_setOutputStream( posDebug );
	VALog_setErrorStream( posDebug );
}

void CVACoreImpl::Tidyup( )
{
	/*
	 *  Hinweis: Diese Hilfsmethode wird nur innerhalb des Reentrance-Locks
	 *           aufgerufen - daher keine weiter Absicherung nötig.
	 */

	VA_TRY
	{
		if( m_pTicker )
		{
			m_pTicker->StopTicker( );
			m_pTicker->SetAfterPulseFunctor( NULL );
		}

		FinalizeAudioDriver( );
		FinalizeRenderingModules( );
		FinalizeReproductionModules( );

		delete m_pTicker;
		m_pTicker = nullptr;

		delete m_pCoreThread;
		m_pCoreThread = nullptr;

		delete m_pInputAmp;
		m_pInputAmp = nullptr;

		delete m_pInputStreamDetector;
		m_pInputStreamDetector = nullptr;

		delete m_pR2RPatchbay;
		m_pR2RPatchbay = nullptr;

		delete m_pOutputPatchbay;
		m_pOutputPatchbay = nullptr;

		delete m_pOutputStreamDetector;
		m_pOutputStreamDetector = nullptr;

		delete m_pOutputTracker;
		m_pOutputTracker = nullptr;

		delete m_pStreamProbeDeviceInput;
		m_pStreamProbeDeviceInput = nullptr;

		delete m_pStreamProbeDeviceOutput;
		m_pStreamProbeDeviceOutput = nullptr;

		delete m_pSignalSourceManager;
		m_pSignalSourceManager = nullptr;

#ifdef VACORE_WITH_SAMPLER_SUPPORT
		delete m_pGlobalSampler;
		m_pGlobalSampler = nullptr;

		delete m_pGlobalSamplePool;
		m_pGlobalSamplePool = nullptr;
#endif

		if( m_pDirectivityManager )
			m_pDirectivityManager->Finalize( );
		delete m_pDirectivityManager;
		m_pDirectivityManager = nullptr;


		if( m_pSceneManager )
			m_pSceneManager->Finalize( );
		delete m_pSceneManager;
		m_pSceneManager = nullptr;

		m_iState = VA_CORESTATE_CREATED;
	}
	VA_FINALLY { m_iState = VA_CORESTATE_FAIL; }
}
