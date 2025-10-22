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

#include <ITAStreamInfo.h>
#include <VANetAudioStreamServer.h>

CVANetAudioStreamServerImpl::CVANetAudioStreamServerImpl( CVANetAudioStreamServer* pParent, const double dSampleRate, const int iBlockLength )
    : m_pParent( pParent )
    , m_pSampleServer( NULL )
    , CITASampleProcessor( 1, dSampleRate, iBlockLength )
{
}

CVANetAudioStreamServerImpl::~CVANetAudioStreamServerImpl( )
{
	delete m_pSampleServer;
}

void CVANetAudioStreamServerImpl::Process( const ITAStreamInfo* pStreamInfo )
{
	m_oStreamInfo.bSyncMod   = false;
	m_oStreamInfo.bTimeReset = false;
	m_oStreamInfo.dCoreTime  = pStreamInfo->dStreamTimeCode;
	m_oStreamInfo.dSysTime   = pStreamInfo->dSysTimeCode;
	m_oStreamInfo.i64Sample  = pStreamInfo->nSamples;
	m_pParent->Process( m_sfStreamTransmitBuffer, m_oStreamInfo );
}

bool CVANetAudioStreamServerImpl::InitializeInternal( const std::string& sBindInterface, const int iBindPort )
{
	m_pSampleServer = new CITANetAudioSampleServer( this );

	const double dSyncTimeInterval = 0.001f; // 1 ms
	return m_pSampleServer->Start( sBindInterface, iBindPort, dSyncTimeInterval );
}
