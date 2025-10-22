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
#include "VALog.h"

#include <VistaInterProcComm/Concurrency/VistaPriority.h>
#include <algorithm>


std::ostream* VA_STDOUT = &std::cout;
void VALog_setOutputStream( std::ostream* os )
{
	VA_STDOUT = os;
}

std::ostream* VA_STDERR = &std::cerr;
void VALog_setErrorStream( std::ostream* os )
{
	VA_STDERR = os;
}

static int g_iVALogLevel = VACORE_DEFAULT_LOG_LEVEL;
int VALog_GetLogLevel( )
{
	return g_iVALogLevel;
}

void VALog_SetLogLevel( int iLevel )
{
	if( iLevel < 0 )
		VA_EXCEPT2( INVALID_PARAMETER, "Log level cannot be negative, zero will put VACore to quite mode." );
	g_iVALogLevel = iLevel;
}


VistaMutex g_mxOutputStream;
VistaMutex& VALog_getOutputStreamMutex( )
{
	return g_mxOutputStream;
}

std::ostream& operator<<( std::ostream& out, const CVALogItem& item )
{
	out << "[" << item.dTimestamp << "] ";
	if( !item.sLogger.empty( ) )
		out << "<" << item.sLogger << "> ";
	out << item.sMsg << std::endl;
	return out;
}

CVARealtimeLogger::CVARealtimeLogger( ) : m_evTrigger( VistaThreadEvent::WAITABLE_EVENT ), m_bStop( false ), m_iTriggerCnt( 0 )
{
	SetThreadName( "VACore::RealtimeLogger" );

	// Low thread priority
	// int iPriority = VistaPriority::VISTA_MIN_PRIORITY;
	// SetPriority(iPriority); // TODO stienen/wefers: klären wie man eine low priority setzen kann

	// @todo: sort out how to properly use vista threads.

	Run( );
}

CVARealtimeLogger::~CVARealtimeLogger( )
{
	m_bStop = true;
	m_evTrigger.SignalEvent( );
	m_evTrigger.WaitForEvent( true );
	StopGently( false );
}

static CVARealtimeLogger g_oInstance;
CVARealtimeLogger* CVARealtimeLogger::GetInstance( )
{
	return &g_oInstance;
}

void CVARealtimeLogger::Register( CVARealtimeLogStream* pStream )
{
	m_mxRegistration.Lock( );
	m_lpStreams.push_back( pStream );
	m_mxRegistration.Unlock( );

	Trigger( );
}

void CVARealtimeLogger::Unregister( CVARealtimeLogStream* pStream )
{
	m_mxRegistration.Lock( );
	m_lpStreams.remove( pStream );
	m_mxRegistration.Unlock( );

	Trigger( );
}

void CVARealtimeLogger::Trigger( )
{
	++m_iTriggerCnt;
	m_evTrigger.SignalEvent( );
}

bool CVARealtimeLogger::LoopBody( )
{
	if( ( m_iTriggerCnt == 0 ) && !m_bStop )
	{
		// Blockierend warten
		m_evTrigger.WaitForEvent( true );

		if( m_bStop == true )
		{
			m_evTrigger.SignalEvent( );
			return false;
		}

		// Daten greifen
		std::vector<CVALogItem> vLogItems;
		m_mxRegistration.Lock( );
		std::list<CVARealtimeLogStream*>::const_iterator it;
		for( it = m_lpStreams.begin( ); it != m_lpStreams.end( ); ++it )
		{
			CVARealtimeLogStream* pStream = *it;
			CVALogItem oLogItem;
			while( pStream->m_qLog.try_pop( oLogItem ) )
				vLogItems.push_back( oLogItem );
		}
		m_mxRegistration.Unlock( );

		// Daten sortieren (mittels überlademen Vergleichsoperator
		if( vLogItems.size( ) > 1 )
			std::sort( vLogItems.begin( ), vLogItems.end( ) );

		// Daten ausgeben

		// Trigger-Event zurücksetzen
		m_evTrigger.ResetThisEvent( );
	}

	// Stop-Flag überprüfen, ggf. selbst beenden
	if( m_bStop )
	{
		IndicateLoopEnd( );
		return false;
	}

	m_iTriggerCnt = 0;

	// Kein Wechsel zu einem Thread (no yield)
	return false;
}

//! Kleiner-Operator
bool CVALogItem::operator<( CVALogItem const& rhs )
{
	if( this->dTimestamp < rhs.dTimestamp )
		return true;
	return false;
}

//! Größer-Operator
bool CVALogItem::operator>( CVALogItem const& rhs )
{
	if( this->dTimestamp > rhs.dTimestamp )
		return true;
	return false;
}
