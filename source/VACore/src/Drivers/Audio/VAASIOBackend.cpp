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

#include "VAASIOBackend.h"

#include "../../Utils/VADebug.h"

#include <ITAAsioInterface.h>
#include <ITAException.h>
#include <ITANumericUtils.h>
#include <ITAStringUtils.h>
#include <cassert>

// Mediator-Thread benutzen?
#define USE_MEDIATOR_THREAD

CVAASIOBackend::CVAASIOBackend( CVAAudioDriverConfig* pConfig ) : m_pMediator( nullptr ), m_pConfig( pConfig ), m_bStreaming( false )
{
#ifdef USE_MEDIATOR_THREAD
	m_pMediator = new MediatorThread( this );
#endif
}

CVAASIOBackend::~CVAASIOBackend( )
{
	delete m_pMediator;
}

std::string CVAASIOBackend::getDriverName( ) const
{
	return "ASIO";
}

std::string CVAASIOBackend::getDeviceName( ) const
{
	return m_pConfig->sDevice;
}

int CVAASIOBackend::getNumberOfInputs( ) const
{
	return m_pConfig->iInputChannels;
}

const ITAStreamProperties* CVAASIOBackend::getOutputStreamProperties( ) const
{
	return &m_oOutputStreamProps;
}

ITADatasource* CVAASIOBackend::getInputStreamDatasource( ) const
{
	return ITAsioGetRecordDatasource( );
}

void CVAASIOBackend::setOutputStreamDatasource( ITADatasource* pDatasource )
{
	ITAsioSetPlaybackDatasource( pDatasource );
}

void CVAASIOBackend::initialize( )
{
#ifdef USE_MEDIATOR_THREAD
	m_pMediator->doInitialize( );
#else
	threadInitialize( );
#endif // USE_MEDIATOR_THREAD
}

void CVAASIOBackend::threadInitialize( )
{
	ASIOError ae;

	ITAsioInitializeLibrary( );

	if( ( ae = ITAsioInitializeDriver( m_pConfig->sDevice.c_str( ) ) ) != ASE_OK )
	{
		ITA_EXCEPT1( INVALID_PARAMETER, std::string( "Failed to initialize the ASIO driver \"" ) + m_pConfig->sDevice + std::string( "\". " ) + ITAsioGetErrorStr( ae ) +
		                                    std::string( " (ASIO Errorcode " ) + IntToString( ae ) + std::string( ")" ) );
	}

	// Anzahl der verfügbaren Kanäle ermitteln
	long lInputs, lOutputs;
	if( ae = ITAsioGetChannels( &lInputs, &lOutputs ) )
		ITA_EXCEPT1( INVALID_PARAMETER, std::string( "Failed to query the ASIO driver \"" ) + m_pConfig->sDevice + std::string( "\". " ) + ITAsioGetErrorStr( ae ) +
		                                    std::string( " (ASIO Errorcode " ) + IntToString( ae ) + std::string( ")" ) );

	// Anzahlen der Kanäle automatisch ermitteln
	if( m_pConfig->iInputChannels == CVAAudioDriverConfig::AUTO )
		m_pConfig->iInputChannels = lInputs;

	if( m_pConfig->iOutputChannels == CVAAudioDriverConfig::AUTO )
		m_pConfig->iOutputChannels = lOutputs;

	if( ( m_pConfig->iInputChannels > lInputs ) || ( m_pConfig->iInputChannels > lOutputs ) )
		ITA_EXCEPT1( INVALID_PARAMETER,
		             std::string( "The ASIO driver \"" ) + m_pConfig->sDevice + std::string( "\" does not support for the desired number of inputs or outputs" ) );

	// Abtastrate überprüfen
	if( ITAsioCanSampleRate( m_pConfig->dSampleRate ) != ASE_OK )
		ITA_EXCEPT1( INVALID_PARAMETER, std::string( "The ASIO driver \"" ) + m_pConfig->sDevice + std::string( "\" does not support the desired sampling rate" ) );

	// Buffergröße automatisch bestimmen, falls nicht vorgegeben
	if( m_pConfig->iBuffersize == CVAAudioDriverConfig::AUTO )
	{
		long lMin, lMax, lPref, lGran;
		if( ( ae = ITAsioGetBufferSize( &lMin, &lMax, &lPref, &lGran ) ) != ASE_OK )
			ITA_EXCEPT1( INVALID_PARAMETER, std::string( "Failed to query the buffersize of the ASIO driver \"" ) + m_pConfig->sDevice + std::string( "\". " ) +
			                                    ITAsioGetErrorStr( ae ) + std::string( " (ASIO Errorcode " ) + IntToString( ae ) + std::string( ")" ) );

		// Wir nehmen die bevorzugte Puffergröße
		if( lPref == 0 )
			ITA_EXCEPT1( INVALID_PARAMETER,
			             std::string( "Failed to query the buffersize of the ASIO driver \"" ) + m_pConfig->sDevice + std::string( "\". (Preferred buffersize 0)" ) );

		m_pConfig->iBuffersize = lPref;
	}

	if( ( ae = ITAsioCreateBuffers( m_pConfig->iInputChannels, m_pConfig->iOutputChannels, m_pConfig->iBuffersize ) ) != ASE_OK )
		ITA_EXCEPT1( INVALID_PARAMETER, std::string( "Failed to setup the ASIO driver \"" ) + m_pConfig->sDevice + std::string( "\". " ) + ITAsioGetErrorStr( ae ) +
		                                    std::string( " (ASIO Errorcode " ) + IntToString( ae ) + std::string( ")" ) );

	m_oOutputStreamProps.dSamplerate   = m_pConfig->dSampleRate;
	m_oOutputStreamProps.uiChannels    = (unsigned int)m_pConfig->iOutputChannels;
	m_oOutputStreamProps.uiBlocklength = (unsigned int)m_pConfig->iBuffersize;
}

void CVAASIOBackend::finalize( )
{
#ifdef USE_MEDIATOR_THREAD
	m_pMediator->doFinalize( );
#else
	threadFinalize( );
#endif // USE_MEDIATOR_THREAD
}

void CVAASIOBackend::threadFinalize( )
{
	// Räumt alles automatisch ab
	ITAsioFinalizeLibrary( );
}

bool CVAASIOBackend::isStreaming( )
{
	return m_bStreaming;
}

void CVAASIOBackend::startStreaming( )
{
	// [fwe] Wichtig: Direkt die Streaming-Flag setzen, auch schon bevor es eigentlich los geht.
	m_bStreaming = true;

#ifdef USE_MEDIATOR_THREAD
	m_pMediator->doStartStreaming( );
#else
	threadStartStreaming( );
#endif // USE_MEDIATOR_THREAD
}

void CVAASIOBackend::threadStartStreaming( )
{
	ASIOError ae;

	if( ( ae = ITAsioStart( ) ) != ASE_OK )
	{
		// [fwe] Streaming-Flag löschen
		m_bStreaming = false;

		ITA_EXCEPT1( INVALID_PARAMETER, std::string( "Failed to start the ASIO streaming. " ) + ITAsioGetErrorStr( ae ) + std::string( " (ASIO Errorcode " ) +
		                                    IntToString( ae ) + std::string( ")" ) );
	}
}

void CVAASIOBackend::stopStreaming( )
{
#ifdef USE_MEDIATOR_THREAD
	m_pMediator->doStopStreaming( );
#else
	threadStopStreaming( );
#endif // USE_MEDIATOR_THREAD

	// [fwe] Wichtig: Direkt die Streaming-Flag erst löschen, wenn wirklich alles zuende ist
	m_bStreaming = true;
}

void CVAASIOBackend::threadStopStreaming( )
{
	/*
	 *  Wenn das Stoppen nicht klappt...
	 *  Dann geht die Welt unter...
	 *  Und dann braucht man auch nicht mehr aufräumen...
	 */
	ITAsioStop( );
}

//

CVAASIOBackend::MediatorThread::MediatorThread( CVAASIOBackend* pParent )
    : m_pParent( pParent )
    , m_iOperation( OPERATION_NOTHING )
    ,
    // TODO: What about the Posix-Flag in VistaThreadEvent?
    m_evStart( VistaThreadEvent::NON_WAITABLE_EVENT )
    , m_evFinish( VistaThreadEvent::NON_WAITABLE_EVENT )
{
	SetThreadName( "VACore::ASIOMediatorThread" );
	Run( );
}

CVAASIOBackend::MediatorThread::~MediatorThread( )
{
	m_iOperation = OPERATION_STOP_THREAD;
	m_evStart.SignalEvent( );
	StopGently( true );
}

int CVAASIOBackend::MediatorThread::doOperation( int iOperation )
{
	m_iOperation = iOperation;
	m_evStart.SignalEvent( );
	m_evFinish.WaitForEvent( true );
	m_evFinish.ResetThisEvent( );

	if( m_iResult != 0 )
	{
		// Rethrow exception
		throw m_oException;
	}

	return 0;
}

int CVAASIOBackend::MediatorThread::doInitialize( )
{
	return doOperation( OPERATION_INITIALIZE );
}

int CVAASIOBackend::MediatorThread::doFinalize( )
{
	return doOperation( OPERATION_FINALIZE );
}

int CVAASIOBackend::MediatorThread::doStartStreaming( )
{
	return doOperation( OPERATION_START_STREAMING );
}

int CVAASIOBackend::MediatorThread::doStopStreaming( )
{
	return doOperation( OPERATION_STOP_STREAMING );
}

ITAException CVAASIOBackend::MediatorThread::getException( ) const
{
	return m_oException;
}

bool CVAASIOBackend::MediatorThread::LoopBody( )
{
	m_evStart.WaitForEvent( true );
	if( m_iOperation == OPERATION_STOP_THREAD )
	{
		IndicateLoopEnd( );
		return true;
	}

	m_evStart.ResetThisEvent( );

	m_iResult = 0;

	try
	{
		switch( m_iOperation )
		{
			case OPERATION_INITIALIZE:
				m_pParent->threadInitialize( );
				break;

			case OPERATION_FINALIZE:
				m_pParent->threadFinalize( );
				break;

			case OPERATION_START_STREAMING:
				m_pParent->threadStartStreaming( );
				break;

			case OPERATION_STOP_STREAMING:
				m_pParent->threadStopStreaming( );
				break;
		}
	}
	catch( ITAException& e )
	{
		m_iResult    = -1;
		m_oException = e;
	}
	catch( ... )
	{
		// Hier sollte nie etwas ankommen...
		assert( false );
	}

	m_evFinish.SignalEvent( );

	// No yield.
	return false;
}
