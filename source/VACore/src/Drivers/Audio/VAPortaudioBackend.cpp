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

#include "VAPortaudioBackend.h"

#include "../../Utils/VADebug.h"

#include <ITAException.h>
#include <ITANumericUtils.h>
#include <ITAPortaudioInterface.h>
#include <ITAStringUtils.h>
#include <cassert>

// Mediator-Thread benutzen?
//#define USE_MEDIATOR_THREAD

CVAPortaudioBackend::CVAPortaudioBackend( CVAAudioDriverConfig* pConfig ) : m_pMediator( nullptr ), m_pConfig( pConfig ), m_bStreaming( false )
{
#ifdef USE_MEDIATOR_THREAD
	m_pMediator = new MediatorThread( this );
#endif
	if( m_pConfig->iBuffersize <= 0 )
		m_pConfig->iBuffersize = ITAPortaudioInterface::GetPreferredBufferSize( );

	m_pITAPA = new ITAPortaudioInterface( m_pConfig->dSampleRate, m_pConfig->iBuffersize );

	// An diesen Einstellungen ist nicht mehr zu drehen ...
	m_oOutputStreamProps.dSamplerate   = m_pConfig->dSampleRate;
	m_oOutputStreamProps.uiBlocklength = (unsigned int)m_pConfig->iBuffersize;
	m_oOutputStreamProps.uiChannels    = (unsigned int)m_pConfig->iOutputChannels;
}

CVAPortaudioBackend::~CVAPortaudioBackend( )
{
	delete m_pMediator;
	delete m_pITAPA;
}

std::string CVAPortaudioBackend::getDriverName( ) const
{
	return "Portaudio";
}

std::string CVAPortaudioBackend::getDeviceName( ) const
{
	return m_pConfig->sDevice;
}

int CVAPortaudioBackend::getNumberOfInputs( ) const
{
	return m_pConfig->iInputChannels;
}

const ITAStreamProperties* CVAPortaudioBackend::getOutputStreamProperties( ) const
{
	return &m_oOutputStreamProps;
}

ITADatasource* CVAPortaudioBackend::getInputStreamDatasource( ) const
{
	return m_pITAPA->GetRecordDatasource( );
}

void CVAPortaudioBackend::setOutputStreamDatasource( ITADatasource* pDatasource )
{
	ITAPortaudioInterface::ITA_PA_ERRORCODE e;
	if( ( e = m_pITAPA->SetPlaybackDatasource( pDatasource ) ) != ITAPortaudioInterface::ITA_PA_NO_ERROR )
		ITA_EXCEPT1( INVALID_PARAMETER, ITAPortaudioInterface::GetErrorCodeString( e ).c_str( ) );
}

void CVAPortaudioBackend::initialize( )
{
#ifdef USE_MEDIATOR_THREAD
	m_pMediator->doInitialize( );
#else
	threadInitialize( );
#endif // USE_MEDIATOR_THREAD
}

void CVAPortaudioBackend::threadInitialize( )
{
	ITAPortaudioInterface::ITA_PA_ERRORCODE e;

	int iDriverID = -1;
	try
	{
		iDriverID = StringToInt( m_pConfig->sDevice );
	}
	catch( ITAException& e )
	{
		if( e.iErrorCode != ITAException::PARSE_ERROR )
			throw e;
	}

	if( iDriverID == -1 )
	{
		if( ( e = m_pITAPA->Initialize( ) ) != ITAPortaudioInterface::ITA_PA_NO_ERROR )
			ITA_EXCEPT1( INVALID_PARAMETER, ITAPortaudioInterface::GetErrorCodeString( e ).c_str( ) );
		iDriverID = m_pITAPA->GetDefaultOutputDevice( ); // Schneller, wenn Portaudio initialisiert ist
	}
	else
	{
		if( ( e = m_pITAPA->Initialize( iDriverID ) ) != ITAPortaudioInterface::ITA_PA_NO_ERROR )
			ITA_EXCEPT1( INVALID_PARAMETER, ITAPortaudioInterface::GetErrorCodeString( e ).c_str( ) );
	}

	// Anzahl Kanäle in die Config übernehmen
	m_pConfig->iInputChannels  = m_pITAPA->GetNumInputChannels( iDriverID );
	m_pConfig->iOutputChannels = m_pITAPA->GetNumOutputChannels( iDriverID );

	m_pITAPA->SetRecordEnabled( m_pConfig->iInputChannels > 0 );
	m_pITAPA->SetPlaybackEnabled( m_pConfig->iOutputChannels > 0 );

	double dSampleRate;
	m_pITAPA->GetDriverSampleRate( iDriverID, dSampleRate );

	if( m_pConfig->dSampleRate != dSampleRate )
		ITA_EXCEPT1( INVALID_PARAMETER, "Samplerate " + DoubleToString( m_pConfig->dSampleRate ) + " is invalid for this audio device." );

	m_oOutputStreamProps.uiBlocklength = (unsigned int)m_pConfig->iBuffersize;
	m_oOutputStreamProps.uiChannels    = (unsigned int)m_pITAPA->GetNumOutputChannels( iDriverID );
}

void CVAPortaudioBackend::finalize( )
{
#ifdef USE_MEDIATOR_THREAD
	m_pMediator->doFinalize( );
#else
	threadFinalize( );
#endif // USE_MEDIATOR_THREAD
}

void CVAPortaudioBackend::threadFinalize( )
{
	ITAPortaudioInterface::ITA_PA_ERRORCODE e;

	if( ( e = m_pITAPA->Finalize( ) ) != ITAPortaudioInterface::ITA_PA_NO_ERROR )
		ITA_EXCEPT1( INVALID_PARAMETER, ITAPortaudioInterface::GetErrorCodeString( e ).c_str( ) );
}

bool CVAPortaudioBackend::isStreaming( )
{
	return m_bStreaming;
}

void CVAPortaudioBackend::startStreaming( )
{
	// [fwe] Wichtig: Direkt die Streaming-Flag setzen, auch schon bevor es eigentlich los geht.
	m_bStreaming = true;

#ifdef USE_MEDIATOR_THREAD
	m_pMediator->doStartStreaming( );
#else
	threadStartStreaming( );
#endif // USE_MEDIATOR_THREAD
}

void CVAPortaudioBackend::threadStartStreaming( )
{
	ITAPortaudioInterface::ITA_PA_ERRORCODE e;

	if( ( e = m_pITAPA->Open( ) ) != ITAPortaudioInterface::ITA_PA_NO_ERROR )
		ITA_EXCEPT1( INVALID_PARAMETER, ITAPortaudioInterface::GetErrorCodeString( e ).c_str( ) );

	if( ( e = m_pITAPA->Start( ) ) != ITAPortaudioInterface::ITA_PA_NO_ERROR )
	{
		ITA_EXCEPT1( INVALID_PARAMETER, ITAPortaudioInterface::GetErrorCodeString( e ).c_str( ) );
		m_bStreaming = false;
	}
	else
	{
		m_bStreaming = true;
	}
}

void CVAPortaudioBackend::stopStreaming( )
{
#ifdef USE_MEDIATOR_THREAD
	m_pMediator->doStopStreaming( );
#else
	threadStopStreaming( );
#endif // USE_MEDIATOR_THREAD

	assert( m_bStreaming == false );
}

void CVAPortaudioBackend::threadStopStreaming( )
{
	ITAPortaudioInterface::ITA_PA_ERRORCODE e;

	if( ( e = m_pITAPA->Stop( ) ) != ITAPortaudioInterface::ITA_PA_NO_ERROR )
	{
		ITA_EXCEPT1( INVALID_PARAMETER, ITAPortaudioInterface::GetErrorCodeString( e ).c_str( ) );
	}
	else
	{
		m_bStreaming = false;
	}

	if( ( e = m_pITAPA->Close( ) ) != ITAPortaudioInterface::ITA_PA_NO_ERROR )
		ITA_EXCEPT1( INVALID_PARAMETER, ITAPortaudioInterface::GetErrorCodeString( e ).c_str( ) );
}

/* TODO stienen: klären, ob wir einen Mediator für Portaudio überhaupt benötigen

CVAPortaudioBackend::MediatorThread::MediatorThread(CVAPortaudioBackend* pParent)
: m_pParent(pParent),
m_iOperation(OPERATION_NOTHING),
// TODO: What about the Posix-Flag in VistaThreadEvent?
m_evStart(false),
m_evFinish(false)
{
SetThreadName("VACore::ASIOMediatorThread");
Run();
}

CVAPortaudioBackend::MediatorThread::~MediatorThread() {
m_iOperation = OPERATION_STOP_THREAD;
m_evStart.SignalEvent();
StopGently(true);
}

int CVAPortaudioBackend::MediatorThread::doOperation(int iOperation) {
m_iOperation = iOperation;
m_evStart.SignalEvent();
m_evFinish.WaitForEvent(true);
m_evFinish.ResetThisEvent();

if (m_iResult != 0) {
// Rethrow exception
throw m_oException;
}

return 0;
}

int CVAPortaudioBackend::MediatorThread::doInitialize() {
return doOperation(OPERATION_INITIALIZE);
}

int CVAPortaudioBackend::MediatorThread::doFinalize() {
return doOperation(OPERATION_FINALIZE);
}

int CVAPortaudioBackend::MediatorThread::doStartStreaming() {
return doOperation(OPERATION_START_STREAMING);
}

int CVAPortaudioBackend::MediatorThread::doStopStreaming() {
return doOperation(OPERATION_STOP_STREAMING);
}

ITAException CVAPortaudioBackend::MediatorThread::getException() const {
return m_oException;
}

bool CVAPortaudioBackend::MediatorThread::LoopBody() {
m_evStart.WaitForEvent(true);
if (m_iOperation == OPERATION_STOP_THREAD) {
IndicateLoopEnd();
return true;
}

m_evStart.ResetThisEvent();

m_iResult = 0;

try {
switch (m_iOperation) {
case OPERATION_INITIALIZE:
m_pParent->threadInitialize();
break;

case OPERATION_FINALIZE:
m_pParent->threadFinalize();
break;

case OPERATION_START_STREAMING:
m_pParent->threadStartStreaming();
break;

case OPERATION_STOP_STREAMING:
m_pParent->threadStopStreaming();
break;
}

} catch (ITAException& e) {
m_iResult = -1;
m_oException = e;
} catch (...) {
// Hier sollte nie etwas ankommen...
assert( false );
}

m_evFinish.SignalEvent();

// No yield.
return false;
}
*/
