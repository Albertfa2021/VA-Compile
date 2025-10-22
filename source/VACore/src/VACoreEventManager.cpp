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

#include "VACoreEventManager.h"

#include "VALog.h"

#include <VAException.h>
#include <VistaInterProcComm/Concurrency/VistaPriority.h>

CVACoreEventManager::CVACoreEventManager( ) : m_evInnerEventQueue( VistaThreadEvent::NON_WAITABLE_EVENT ), m_bStop( false ), m_iEventIDCounter( 1 )
{
	SetThreadName( "VACore::EventManagerThread" );

	// Low thread priority
	int iPriority = VistaPriority::VISTA_MIN_PRIORITY + (int)( 0.25 * ( VistaPriority::VISTA_MAX_PRIORITY - VistaPriority::VISTA_MIN_PRIORITY ) );
	SetPriority( iPriority );

	m_evInnerEventQueue.ResetThisEvent( );

	if( !Run( ) )
		VA_EXCEPT2( UNSPECIFIED, "Failed to start event manager thread" );
}

CVACoreEventManager::~CVACoreEventManager( )
{
	m_oReg.DetachAllHandlers( );

	// Thread sauber beenden
	m_bStop = true;
	m_evInnerEventQueue.SignalEvent( );
	StopGently( true );
}

void CVACoreEventManager::AttachHandler( IVAEventHandler* pHandler )
{
	VA_VERBOSE( "Core", __FUNCTION__ << "(" << pHandler << ")" );
	m_oReg.AttachHandler( pHandler );
}

void CVACoreEventManager::DetachHandler( IVAEventHandler* pHandler )
{
	VA_VERBOSE( "Core", __FUNCTION__ << "(" << pHandler << ")" );
	m_oReg.DetachHandler( pHandler );
}

void CVACoreEventManager::EnqueueEvent( const CVAEvent& evCoreEvent )
{
	m_csOuterEventQueue.enter( );
	m_qOuterEventQueue.push_back( evCoreEvent );
	m_csOuterEventQueue.leave( );
}

void CVACoreEventManager::BroadcastEvents( )
{
	m_csOuterEventQueue.enter( );
	m_csInnerEventQueue.enter( );

	// Copy events from outer queue into the inner one
	std::deque<CVAEvent>::const_iterator cit = m_qOuterEventQueue.begin( );
	while( cit != m_qOuterEventQueue.end( ) )
		m_qInnerEventQueue.push_back( *cit++ );

	m_qOuterEventQueue.clear( );

	m_csInnerEventQueue.leave( );
	m_csOuterEventQueue.leave( );
	m_evInnerEventQueue.SignalEvent( );
}

void CVACoreEventManager::BroadcastEvent( const CVAEvent& evCoreEvent )
{
	// Event direkt in die innere Queue
	m_csInnerEventQueue.enter( );
	m_qInnerEventQueue.push_back( evCoreEvent );
	m_csInnerEventQueue.leave( );
	m_evInnerEventQueue.SignalEvent( );
}

bool CVACoreEventManager::LoopBody( )
{
	m_evInnerEventQueue.WaitForEvent( true );
	if( m_bStop )
	{
		IndicateLoopEnd( );
		return true;
	}
	m_evInnerEventQueue.ResetThisEvent( );

	m_csInnerEventQueue.enter( );
	while( !m_qInnerEventQueue.empty( ) )
	{
		// Get the next event. Unlock...
		CVAEvent* ev = &m_qInnerEventQueue.front( );
		m_csInnerEventQueue.leave( );

		// Assign an unique event ID
		ev->iEventID = m_iEventIDCounter++;

		// Broadcast
		m_oReg.BroadcastEvent( ev );

		// Remove the event ...
		m_csInnerEventQueue.enter( );
		m_qInnerEventQueue.pop_front( );
	}
	m_csInnerEventQueue.leave( );

	// Kein Yield.
	return false;
}
