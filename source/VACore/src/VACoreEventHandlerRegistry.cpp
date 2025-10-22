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

#include "VACoreEventHandlerRegistry.h"

#include <ITAException.h>
#include <VAEvent.h>

CVACoreEventHandlerRegistry::CVACoreEventHandlerRegistry( ) {}

CVACoreEventHandlerRegistry::~CVACoreEventHandlerRegistry( )
{
	DetachAllHandlers( );
}

void CVACoreEventHandlerRegistry::AttachHandler( IVAEventHandler* pHandler )
{
	m_csHandlers.enter( );
	m_spHandlers.insert( pHandler );
	m_csHandlers.leave( );
}

void CVACoreEventHandlerRegistry::DetachHandler( IVAEventHandler* pHandler )
{
	m_csHandlers.enter( );
	m_spHandlers.erase( pHandler );
	m_csHandlers.leave( );
}

void CVACoreEventHandlerRegistry::DetachAllHandlers( )
{
	m_csHandlers.enter( );
	m_spHandlers.clear( );
	m_csHandlers.leave( );
}

void CVACoreEventHandlerRegistry::BroadcastEvent( const CVAEvent* evCoreEvent )
{
#ifdef VACORE_EVENTS_ENABLED
	m_csHandlers.enter( );
	for( std::set<IVAEventHandler*>::iterator it = m_spHandlers.begin( ); it != m_spHandlers.end( ); ++it )
		( *it )->HandleVAEvent( evCoreEvent );
	m_csHandlers.leave( );
#endif
}
