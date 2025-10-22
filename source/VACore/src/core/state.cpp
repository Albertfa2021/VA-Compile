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

int CVACoreImpl::GetState( ) const
{
	VA_VERBOSE( "Core", "Core state requested, current state is " << m_iState );
	return m_iState;
}

bool CVACoreImpl::IsStreaming( ) const
{
	return ( m_pAudioDriverBackend ? m_pAudioDriverBackend->isStreaming( ) : false );
}
