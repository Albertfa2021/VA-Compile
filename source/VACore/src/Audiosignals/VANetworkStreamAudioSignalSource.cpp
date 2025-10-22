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

#include <VANetworkStreamAudioSignalSource.h>
#include <VistaInterProcComm/Connections/VistaConnectionIP.h>
#include <VistaInterProcComm/IPNet/VistaTCPServer.h>
#include <VistaInterProcComm/IPNet/VistaTCPSocket.h>

CVANetworkStreamAudioSignalSource::CVANetworkStreamAudioSignalSource( ) : m_pServer( NULL ), m_pSocket( NULL ), m_pConnection( NULL ) {}

bool CVANetworkStreamAudioSignalSource::Start( const std::string& sServerBindAddress /* = "localhost" */, const int iServerBindPort /* = 12480 */ )
{
	m_pServer = new VistaTCPServer( sServerBindAddress, iServerBindPort );

	m_pSocket = m_pServer->GetNextClient( );
	if( !m_pSocket )
		return false;

	if( m_pSocket->GetIsConnected( ) )
		m_pConnection = new VistaConnectionIP( m_pSocket );

	if( !m_pConnection )
		return false;

	return true;
}

void CVANetworkStreamAudioSignalSource::Stop( )
{
	delete m_pConnection;
	m_pConnection = NULL;

	m_pSocket = NULL;

	delete m_pServer;
	m_pServer = NULL;
}

CVANetworkStreamAudioSignalSource::~CVANetworkStreamAudioSignalSource( )
{
	delete m_pConnection;
	delete m_pServer;
}

bool CVANetworkStreamAudioSignalSource::GetIsConnected( ) const
{
	return m_pConnection ? true : false;
}

int CVANetworkStreamAudioSignalSource::GetNumQueuedSamples( )
{
	return 0;
}

int CVANetworkStreamAudioSignalSource::Transmit( const std::vector<CVASampleBuffer>& )
{
	return 0;
}