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

double CVACoreImpl::GetCoreClock( ) const
{
	// Clock ist immer da, reentrance hier erlaubt
	double dNow    = m_pClock->getTime( );
	double dOffset = (double)m_fCoreClockOffset;
	return ( dNow - dOffset );
}

void CVACoreImpl::SetCoreClock( const double dSeconds )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		if( dSeconds < 0 )
			VA_EXCEPT2( INVALID_PARAMETER, "Time must not be negative" );

		// Aktuelle Zeit holen
		double dNow    = m_pClock->getTime( );
		double dOffset = dSeconds - dNow;

		// TODO: Sollte eigentlich über doubles gehen. Leider noch keine AtomicDoubles...
		m_fCoreClockOffset = (float)dOffset;

		VA_VERBOSE( "Core", "Set clock to " << timeToString( dSeconds ) );
	}
	VA_RETHROW;
}
