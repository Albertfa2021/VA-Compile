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

#include "VACoreThread.h"

#include "Utils/VADebug.h"
#include "core/core.h"

#include <VAException.h>
#include <VistaInterProcComm/Concurrency/VistaPriority.h>

CVACoreThread::CVACoreThread( CVACoreImpl* pParent )
    : m_pParent( pParent )
    , m_evTrigger( VistaThreadEvent::NON_WAITABLE_EVENT )
    , m_evPaused( VistaThreadEvent::NON_WAITABLE_EVENT )
    , m_bStop( false )
    , m_iTriggerCnt( 0 )
    , m_bPaused( false )
    , m_bReadyForPause( true )
{
	SetThreadName( "VACore::MainThread" );

	// High thread priority
	int iPriority = VistaPriority::VISTA_MIN_PRIORITY + (int)( 0.8 * ( VistaPriority::VISTA_MAX_PRIORITY - VistaPriority::VISTA_MIN_PRIORITY ) );
	SetPriority( iPriority );

	if( !Run( ) )
		VA_EXCEPT1( "Core thread could not be started" );
}

CVACoreThread::~CVACoreThread( )
{
	// Thread sauber beenden
	m_bStop = true;
	m_evTrigger.SignalEvent( );
	StopGently( true );
}

void CVACoreThread::Trigger( )
{
	++m_iTriggerCnt;
	m_evTrigger.SignalEvent( );
}

void CVACoreThread::Break( )
{
	m_bPaused = true;
	Trigger( );
	// Warten bis Loop-Durchlauf definitiv abgeschlossen
	m_evPaused.WaitForEvent( true );
}

bool CVACoreThread::TryBreak( )
{
	if( !m_bReadyForPause )
		return false;

	// Break possible, do it.
	Break( );

	return true;
}


void CVACoreThread::Continue( )
{
	m_bPaused = false;
	Trigger( );
}

bool CVACoreThread::LoopBody( )
{
	while( m_bPaused && !m_bStop )
	{
		m_evPaused.SignalEvent( );
		m_evTrigger.WaitForEvent( true );
	}
	m_evPaused.ResetThisEvent( );

	if( ( m_iTriggerCnt == 0 ) && !m_bStop )
	{
		m_evTrigger.WaitForEvent( true );
		m_evTrigger.ResetThisEvent( );
	}

	if( m_bStop )
	{
		IndicateLoopEnd( );
		return false;
	}

	if( m_bPaused )
		return false;

	m_iTriggerCnt = 0;

	// Thread-Aufruf zurück in die CoreImpl leiten,
	// keine Pause zulassen, da sonst ein deadlock
	// auftreten kann
	m_bReadyForPause = false;
	m_pParent->CoreThreadLoop( );
	m_bReadyForPause = true;

	// Kein Wechsel zu einem Thread (no yield)
	return false;
}
