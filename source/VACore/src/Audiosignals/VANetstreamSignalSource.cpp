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

#include "VANetstreamSignalSource.h"

#include "../VAAudiostreamTracker.h"

#include <ITAException.h>
#include <ITANetAudioStream.h>
#include <VAException.h>
#include <sstream>

CVANetstreamSignalSource::CVANetstreamSignalSource( const double dDestinationSampleRate, const int iBlockLength, const std::string& sBindAddress, const int iRecvPort )
    : m_pAssociatedCore( NULL )
{
	m_pSourceStream = new CITANetAudioStream( 1, dDestinationSampleRate, iBlockLength, 12 * iBlockLength );

	try
	{
		if( !m_pSourceStream->Connect( sBindAddress, iRecvPort ) )
			VA_EXCEPT2( INVALID_PARAMETER, "Could not connect to network audio streaming server or connection refused (parameter mismatch?)" );
	}
	catch( ITAException& e )
	{
		throw CVAException( CVAException::INVALID_PARAMETER, "Netstream signal source setup failed: " + e.ToString( ) );
	}
}

CVANetstreamSignalSource::~CVANetstreamSignalSource( )
{
	delete m_pSourceStream;
	m_pSourceStream = NULL;
}

std::string CVANetstreamSignalSource::GetTypeString( ) const
{
	return "netstream";
}

std::string CVANetstreamSignalSource::GetTypeMnemonic( ) const
{
	return "ns";
}

std::string CVANetstreamSignalSource::GetDesc( ) const
{
	std::stringstream ss;
	ss << "Plays a network audio stream";
	return ss.str( );
}

std::string CVANetstreamSignalSource::GetStateString( ) const
{
	if( m_pSourceStream->GetIsConnected( ) )
		return "Connected";
	else
		return "Disconnected";
}

IVAInterface* CVANetstreamSignalSource::GetAssociatedCore( ) const
{
	return m_pAssociatedCore;
}

ITADatasource* CVANetstreamSignalSource::GetStreamingDatasource( ) const
{
	return m_pSourceStream;
}

std::vector<const float*> CVANetstreamSignalSource::GetStreamBlock( const CVAAudiostreamState* pStreamInfo )
{
	const float* pfSignalSourceData = m_pSourceStream->GetBlockPointer( 0, dynamic_cast<const CVAAudiostreamStateImpl*>( pStreamInfo ) );
	m_pSourceStream->IncrementBlockPointer( );
	return { pfSignalSourceData };
}

void CVANetstreamSignalSource::HandleRegistration( IVAInterface* pParentCore )
{
	m_pAssociatedCore = pParentCore;
}

void CVANetstreamSignalSource::HandleUnregistration( IVAInterface* )
{
	m_pAssociatedCore = NULL;
}
