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

#include "VANetAudioStreamServerImpl.h"

#include <VANetAudioStreamServer.h>

CVANetAudioStreamServer::CVANetAudioStreamServer( const double dSampleRate, const int iBlockLength )
{
	m_pImpl = new CVANetAudioStreamServerImpl( this, dSampleRate, iBlockLength );
}

CVANetAudioStreamServer::~CVANetAudioStreamServer( )
{
	delete m_pImpl;
}

bool CVANetAudioStreamServer::Initialize( const std::string& sBindInterface, const int iBindPort )
{
	return m_pImpl->InitializeInternal( sBindInterface, iBindPort );
}
