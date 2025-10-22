#include "VAVirtualAudioDriverBackend.h"

#include "../../Utils/VADebug.h"

#include <ITAException.h>
#include <ITANumericUtils.h>
#include <ITAStringUtils.h>
#include <VAException.h>
#include <cassert>

CVAVirtualAudioDriverBackend::CVAVirtualAudioDriverBackend( const CVAAudioDriverConfig* pConfig )
    : CVAObject( "VirtualAudioDevice" )
    , m_oConfig( *pConfig )
    , m_pDataSource( NULL )
    , m_bStarted( false )
{
	m_oOutputStreamProps.dSamplerate   = m_oConfig.dSampleRate;
	m_oOutputStreamProps.uiChannels    = (unsigned int)m_oConfig.iOutputChannels;
	m_oOutputStreamProps.uiBlocklength = (unsigned int)m_oConfig.iBuffersize;
}

CVAVirtualAudioDriverBackend::~CVAVirtualAudioDriverBackend( ) {}

std::string CVAVirtualAudioDriverBackend::getDriverName( ) const
{
	return "Virtual";
}

std::string CVAVirtualAudioDriverBackend::getDeviceName( ) const
{
	return "Trigger";
}

int CVAVirtualAudioDriverBackend::getNumberOfInputs( ) const
{
	return m_oConfig.iInputChannels;
}

const ITAStreamProperties* CVAVirtualAudioDriverBackend::getOutputStreamProperties( ) const
{
	return &m_oOutputStreamProps;
}

void CVAVirtualAudioDriverBackend::setOutputStreamDatasource( ITADatasource* pDatasource )
{
	m_pDataSource = pDatasource;
}

ITADatasource* CVAVirtualAudioDriverBackend::getInputStreamDatasource( ) const
{
	return nullptr;
}

void CVAVirtualAudioDriverBackend::initialize( ) {}

void CVAVirtualAudioDriverBackend::finalize( ) {}

void CVAVirtualAudioDriverBackend::startStreaming( )
{
	m_bStarted = true;
}

bool CVAVirtualAudioDriverBackend::isStreaming( )
{
	return m_bStarted;
}

void CVAVirtualAudioDriverBackend::stopStreaming( )
{
	m_bStarted = false;
}

CVAStruct CVAVirtualAudioDriverBackend::CallObject( const CVAStruct& oArgs )
{
	CVAStruct oReturn;

	if( oArgs.HasKey( "trigger" ) && m_pDataSource )
	{
		// Trigger block pointer getter, then increment
		for( int n = 0; n < (int)m_pDataSource->GetNumberOfChannels( ); n++ )
			m_pDataSource->GetBlockPointer( n, &m_oStreamInfo );
		m_pDataSource->IncrementBlockPointer( );
	}

	if( oArgs.HasKey( "help" ) || oArgs.HasKey( "info" ) )
	{
		oReturn["usage"] = "Call this virtual audio device with key 'trigger' to trigger another audio block increment";
	}

	return oReturn;
}

CVAVirtualAudioDriverBackend::ManualClock::ManualClock( ) : CVAObject( "ManualClock" ), m_dTime( 0.0f ) {}

CVAVirtualAudioDriverBackend::ManualClock::~ManualClock( ) {}

double CVAVirtualAudioDriverBackend::ManualClock::getTime( )
{
	m_csTime.enter( );
	double dTime = m_dTime;
	m_csTime.leave( );
	return dTime;
}

void CVAVirtualAudioDriverBackend::ManualClock::SetTime( double dNow )
{
	m_csTime.enter( );
	assert( m_dTime < dNow );
	m_dTime = dNow;
	m_csTime.leave( );
}

CVAStruct CVAVirtualAudioDriverBackend::ManualClock::CallObject( const CVAStruct& oArgs )
{
	if( oArgs.HasKey( "info" ) || oArgs.HasKey( "help" ) )
	{
		CVAStruct oRet;
		oRet["usage"] = "Set the manual clock with the key 'time' and a floating point value. Time has to be strict monotonously increasing";
		return oRet;
	}
	else if( oArgs.HasKey( "time" ) )
	{
		SetTime( oArgs["time"] );
		return CVAStruct( );
	}
	else
	{
		VA_EXCEPT2( INVALID_PARAMETER, "Could npt understand call, 'time' key missing. use 'help' for more information" );
	}
}
