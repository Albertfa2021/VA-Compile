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

#include "VAAudiofileSignalSource.h"

#include "../VAAudiostreamTracker.h"
#include "../VALog.h"

#include <ITAAudioSample.h>
#include <ITABufferDataSource.h>
#include <ITAException.h>
#include <ITASampleFrame.h>
#include <VA.h>
#include <assert.h>
#include <sstream>

CVAAudiofileSignalSource::CVAAudiofileSignalSource( int iNumChannels, int iBufferLength, double dSampleRate, int iBlockLength )
    : m_pAssociatedCore( NULL )
    , m_pBufferDataSource( NULL )
    , m_iCurrentPlayState( IVAInterface::VA_PLAYBACK_STATE_INVALID )
    , m_iRequestedPlaybackAction( IVAInterface::VA_PLAYBACK_ACTION_NONE )
    , m_iActiveAudioBufferChannel( 0 )
    , m_bCrossfadeChannelSwitch( true )
{
	m_sfAudioBuffer.init( iNumChannels, iBufferLength, true );

	std::vector<float*> vfBufferPointer;
	for( int i = 0; i < m_sfAudioBuffer.GetNumChannels( ); i++ )
		vfBufferPointer.push_back( m_sfAudioBuffer[i].GetData( ) );

	m_pBufferDataSource = new ITABufferDatasource( vfBufferPointer, iBufferLength, dSampleRate, iBlockLength );

	m_pBufferDataSource->SetPaused( true );
	m_pBufferDataSource->SetLoopMode( false );

	SetDatasourceDelegatorTarget( m_pBufferDataSource );

	m_sfOutBuffer.Init( iNumChannels, iBlockLength, true );

	for( int i = 0; i < m_sfOutBuffer.GetNumChannels( ); i++ )
		m_vpfOutBuffer.push_back( m_sfOutBuffer[i].GetData( ) );
}

CVAAudiofileSignalSource::CVAAudiofileSignalSource( const std::string& sFileName, double dSampleRate, int iBlockLength )
    : m_pAssociatedCore( NULL )
    , m_pBufferDataSource( NULL )
    , m_iCurrentPlayState( IVAInterface::VA_PLAYBACK_STATE_INVALID )
    , m_iRequestedPlaybackAction( IVAInterface::VA_PLAYBACK_ACTION_NONE )
    , m_iActiveAudioBufferChannel( 0 )
    , m_bCrossfadeChannelSwitch( true )
{
	CITAAudioSample sfFileBuffer( (float)dSampleRate );
	sfFileBuffer.LoadWithSampleRateConversion( sFileName );

	if( sfFileBuffer.GetNumChannels( ) > 1 )
		VA_INFO( "BufferSignalSource", "More than one channel in file '" << sFileName << "' found, using first." );

	m_sfAudioBuffer = sfFileBuffer;

	std::vector<float*> vfBufferPointer;
	for( int i = 0; i < m_sfAudioBuffer.channels( ); i++ )
		vfBufferPointer.push_back( m_sfAudioBuffer[i].data( ) );

	m_pBufferDataSource = new ITABufferDatasource( vfBufferPointer, m_sfAudioBuffer.length( ), dSampleRate, iBlockLength );
	m_pBufferDataSource->SetPaused( true );
	m_pBufferDataSource->SetLoopMode( false );

	SetDatasourceDelegatorTarget( m_pBufferDataSource );

	m_sfOutBuffer.Init( m_sfAudioBuffer.GetNumChannels( ), iBlockLength, true );

	for( int i = 0; i < m_sfOutBuffer.GetNumChannels( ); i++ )
		m_vpfOutBuffer.push_back( m_sfOutBuffer[i].GetData( ) );
}

CVAAudiofileSignalSource::~CVAAudiofileSignalSource( )
{
	delete m_pBufferDataSource;
	m_pBufferDataSource = NULL;
}

int CVAAudiofileSignalSource::GetPlaybackState( ) const
{
	return m_iCurrentPlayState;
}

void CVAAudiofileSignalSource::SetPlaybackAction( int iPlayStateAction )
{
	// Store new play state, will be updated on next process of stream (if no sync mod)
	switch( iPlayStateAction )
	{
		case IVAInterface::VA_PLAYBACK_ACTION_PAUSE:
		case IVAInterface::VA_PLAYBACK_ACTION_PLAY:
		case IVAInterface::VA_PLAYBACK_ACTION_STOP:
			m_iRequestedPlaybackAction = iPlayStateAction;
			break;
		case IVAInterface::VA_PLAYBACK_ACTION_NONE:
			VA_WARN( "BufferSignal", "Ignoring playback action '" << iPlayStateAction << "' (NONE)" );
			break;
		default:
			VA_ERROR( "BufferSignal", "Could not apply unkown playback action '" << iPlayStateAction << "'" );
			break;
	}

	return;
}

void CVAAudiofileSignalSource::SetCursorSeconds( double dPos )
{
	double dDuration = m_pBufferDataSource->GetCapacity( ) / m_pBufferDataSource->GetSampleRate( );
	int iPos;
	if( dPos < 0 )
		iPos = 0;
	else if( dPos > dDuration )
		iPos = m_pBufferDataSource->GetCapacity( );
	else
		iPos = (int)floor( dPos * m_pBufferDataSource->GetSampleRate( ) );

	m_pBufferDataSource->SetCursor( iPos );
}

int CVAAudiofileSignalSource::GetType( ) const
{
	return IVAAudioSignalSource::VA_SS_AUDIOFILE;
}

std::string CVAAudiofileSignalSource::GetTypeString( ) const
{
	return "audiofile";
}

std::string CVAAudiofileSignalSource::GetDesc( ) const
{
	return std::string( "Plays samples from an audio buffer" );
}

std::string CVAAudiofileSignalSource::GetStateString( ) const
{
	return GetStateString( m_iCurrentPlayState );
}

std::string CVAAudiofileSignalSource::GetStateString( int iPlayState ) const
{
	std::string s;

	if( iPlayState == IVAInterface::VA_PLAYBACK_STATE_PLAYING )
		m_pBufferDataSource->GetLoopMode( ) ? s = "playing and looping" : s = "playing";
	else if( iPlayState == IVAInterface::VA_PLAYBACK_STATE_PAUSED )
		s = "paused";
	else if( iPlayState == IVAInterface::VA_PLAYBACK_STATE_STOPPED )
		s = "stopped";
	else
		s = "unkown";

	return s;
}

IVAInterface* CVAAudiofileSignalSource::GetAssociatedCore( ) const
{
	return m_pAssociatedCore;
}

void CVAAudiofileSignalSource::HandleRegistration( IVAInterface* pParentCore )
{
	m_pAssociatedCore = pParentCore;
}

void CVAAudiofileSignalSource::HandleUnregistration( IVAInterface* )
{
	m_pAssociatedCore = nullptr;
}

std::vector<const float*> CVAAudiofileSignalSource::GetStreamBlock( const CVAAudiostreamState* pStreamInfo )
{
	const int iNewPlayAction = m_iRequestedPlaybackAction;
	const bool bSyncMod      = pStreamInfo->bSyncMod; // If true, hold changes back until false

	int iResidualSamples   = m_pBufferDataSource->GetCursor( ) - m_pBufferDataSource->GetCapacity( );
	bool bEndOfFileReached = ( iResidualSamples >= 0 );

	int iNewPlaybackState = IVAInterface::VA_PLAYBACK_STATE_INVALID;
	switch( m_iCurrentPlayState )
	{
		case IVAInterface::VA_PLAYBACK_STATE_STOPPED:
			// Do not apply any changes during synced modification
			if( bSyncMod )
				break;

			if( iNewPlayAction == IVAInterface::VA_PLAYBACK_ACTION_PLAY )
			{
				// Auto-rewind on play action
				if( bEndOfFileReached )
					m_pBufferDataSource->Rewind( );
				m_pBufferDataSource->SetPaused( false );
				iNewPlaybackState = IVAInterface::VA_PLAYBACK_STATE_PLAYING;
			}
			else if( iNewPlayAction == IVAInterface::VA_PLAYBACK_ACTION_PAUSE )
			{
				// Auto-rewind on pause action
				if( bEndOfFileReached )
					m_pBufferDataSource->Rewind( );
				m_pBufferDataSource->SetPaused( true );
				iNewPlaybackState = IVAInterface::VA_PLAYBACK_STATE_PLAYING;
			}
			else
			{
				iNewPlaybackState = IVAInterface::VA_PLAYBACK_STATE_STOPPED;
			}

			break;

		case IVAInterface::VA_PLAYBACK_STATE_PAUSED:
			// Do not apply any changes during synced modification
			if( bSyncMod )
				break;

			if( iNewPlayAction == IVAInterface::VA_PLAYBACK_ACTION_PLAY )
			{
				if( bEndOfFileReached )
					m_pBufferDataSource->Rewind( );
				m_pBufferDataSource->SetPaused( false );
				iNewPlaybackState = IVAInterface::VA_PLAYBACK_STATE_PLAYING;
			}
			else if( iNewPlayAction == IVAInterface::VA_PLAYBACK_ACTION_STOP )
			{
				if( bEndOfFileReached )
					m_pBufferDataSource->Rewind( );
				if( m_pBufferDataSource->GetLoopMode( ) == false )
					m_pBufferDataSource->SetPaused( true );
				m_pBufferDataSource->Rewind( );
				m_pBufferDataSource->SetPaused( true );
				iNewPlaybackState = IVAInterface::VA_PLAYBACK_STATE_STOPPED;
			}
			else
			{
				iNewPlaybackState = IVAInterface::VA_PLAYBACK_STATE_PAUSED;
			}
			break;

		case IVAInterface::VA_PLAYBACK_STATE_PLAYING:
			// Do not apply any changes during synced modification, but process if required
			if( bSyncMod )
			{
				// Special case: if running into EOF, state change may be required
				if( bEndOfFileReached )
				{
					m_pBufferDataSource->Rewind( );
					if( m_pBufferDataSource->GetLoopMode( ) == false )
					{
						m_iCurrentPlayState = IVAInterface::VA_PLAYBACK_STATE_STOPPED; // Attention, directly modifying member, only OK here.
						VA_INFO( "BufferSignal", "Playback stop transition forced during locked scene." );
					}
				}
				break;
			}

			if( iNewPlayAction == IVAInterface::VA_PLAYBACK_ACTION_PAUSE )
			{
				if( bEndOfFileReached )
					m_pBufferDataSource->Rewind( );
				m_pBufferDataSource->SetPaused( true );
				iNewPlaybackState = IVAInterface::VA_PLAYBACK_STATE_PAUSED;
			}
			else if( iNewPlayAction == IVAInterface::VA_PLAYBACK_ACTION_STOP )
			{
				m_pBufferDataSource->Rewind( );
				m_pBufferDataSource->SetPaused( true );
				iNewPlaybackState = IVAInterface::VA_PLAYBACK_STATE_STOPPED;
			}
			else
			{
				// Still playing
				assert( iNewPlayAction == IVAInterface::VA_PLAYBACK_ACTION_PLAY || iNewPlayAction == IVAInterface::VA_PLAYBACK_ACTION_NONE );
				iNewPlaybackState = IVAInterface::VA_PLAYBACK_STATE_PLAYING; // Overwrite invalid play action (NONE)
				if( bEndOfFileReached )
				{
					m_pBufferDataSource->Rewind( );
					if( m_pBufferDataSource->GetLoopMode( ) == false )
						iNewPlaybackState = IVAInterface::VA_PLAYBACK_STATE_STOPPED;
				}
			}
			break;

		case IVAInterface::VA_PLAYBACK_STATE_INVALID:
			// Do not apply any changes during synced modification
			if( bSyncMod )
				break;

			if( iNewPlayAction == IVAInterface::VA_PLAYBACK_ACTION_PLAY )
			{
				m_pBufferDataSource->Rewind( );
				m_pBufferDataSource->SetPaused( false );
				iNewPlaybackState = IVAInterface::VA_PLAYBACK_STATE_PLAYING;
			}
			else if( iNewPlayAction == IVAInterface::VA_PLAYBACK_ACTION_PAUSE )
			{
				m_pBufferDataSource->Rewind( );
				m_pBufferDataSource->SetPaused( true );
				iNewPlaybackState = IVAInterface::VA_PLAYBACK_STATE_PAUSED;
			}
			else if( iNewPlayAction == IVAInterface::VA_PLAYBACK_ACTION_STOP )
			{
				m_pBufferDataSource->Rewind( );
				m_pBufferDataSource->SetPaused( true );
				iNewPlaybackState = IVAInterface::VA_PLAYBACK_STATE_STOPPED;
			}
			else
			{
				iNewPlaybackState = IVAInterface::VA_PLAYBACK_STATE_INVALID;
			}
			break;
	}

	if( m_iCurrentPlayState != iNewPlaybackState && !bSyncMod )
	{
		VA_INFO( "BufferSignal", "Playback transition from '" << GetStateString( m_iCurrentPlayState ) << "'"
		                                                      << " to '" << GetStateString( iNewPlaybackState ) << "'" );

		assert( iNewPlaybackState != IVAInterface::VA_PLAYBACK_STATE_INVALID );
		m_iCurrentPlayState = iNewPlaybackState;
	}

	// Process audio file signal source: copy samples from buffer
	if( m_iCurrentPlayState == IVAInterface::VA_PLAYBACK_STATE_PLAYING )
	{
		for( int i = 0; i < (int)m_pBufferDataSource->GetNumberOfChannels( ); i++ )
		{
			const float* pfData = m_pBufferDataSource->GetBlockPointer( i, dynamic_cast<const CVAAudiostreamStateImpl*>( pStreamInfo ) );
			if( pfData )
				m_sfOutBuffer[i].write( pfData, m_pBufferDataSource->GetBlocklength( ) );
			else
				m_sfOutBuffer[i].Zero( );
		}
		m_pBufferDataSource->IncrementBlockPointer( );
	}
	else
	{
		m_sfOutBuffer.zero( );
	}

	// No intermediate playback actions received? Then ack.
	if( m_iRequestedPlaybackAction == iNewPlayAction && !bSyncMod )
		m_iRequestedPlaybackAction = IVAInterface::VA_PLAYBACK_ACTION_NONE;

	return m_vpfOutBuffer; // Vector of float* always pointing to m_sfOutBuffer (as per interface required)
}

int CVAAudiofileSignalSource::GetNumChannels( ) const
{
	return m_pBufferDataSource->GetNumberOfChannels( );
}

bool CVAAudiofileSignalSource::GetIsLooping( ) const
{
	return m_pBufferDataSource->GetLoopMode( );
}

void CVAAudiofileSignalSource::SetIsLooping( bool bLooping )
{
	m_pBufferDataSource->SetLoopMode( bLooping );
}
