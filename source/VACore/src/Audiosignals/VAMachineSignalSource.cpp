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

#include "VAMachineSignalSource.h"

#include "../VAAudiostreamTracker.h"
#include "../VALog.h"
#include "../core/core.h"

#include <ITAAudiofileReader.h>
#include <ITAConstants.h>
#include <ITAException.h>
#include <ITAFade.h>
#include <ITAFileSystemUtils.h>
#include <ITAInterpolation.h>
#include <ITASampleFrame.h>
#include <assert.h>
#include <math.h>
#include <sstream>

CVAMachineSignalSource::CVAMachineSignalSource( const CVAMachineSignalSource::Config& oConfig )
    : ITADatasourceRealization( 1, oConfig.pCore->GetCoreConfig( )->oAudioDriverConfig.dSampleRate, oConfig.pCore->GetCoreConfig( )->oAudioDriverConfig.iBuffersize )
    , m_oConfig( oConfig )
    , m_iCurrentState( MACHINE_STATE_INVALID )
    , m_iNewTransition( MACHINE_TRANSITION_NONE )
    , m_fMachineSpeed( 1.0f )
    , m_bHasStartSound( false )
    , m_bHasIdleSound( false )
    , m_bHasStopSound( false )
    , m_bCrossfadeSounds( oConfig.bCrossfadeSounds )
    , m_pAssociatedCore( NULL )
{
	if( !( oConfig.bRequiresStartSound && oConfig.bRequiresIdleSound && oConfig.bRequiresStopSound ) )
		VA_EXCEPT2( NOT_IMPLEMENTED, "Current implementation of machine signal source require start, idle and stop sound. Please activate in config first." );

	m_sbOut.Init( GetBlocklength( ), true );

	ResetReadCursors( );
	m_iCurrentState = MACHINE_STATE_INVALID;

	m_bStartSoundNew = m_bIdleSoundNew = m_bStopSoundNew = false;

	if( m_oConfig.bUseSplineInterpolation )
		m_pInterpRoutine = new CITASampleCubicSplineInterpolation;
	else
		m_pInterpRoutine = new CITASampleLinearInterpolation;

	int iLeft, iRight;
	m_pInterpRoutine->GetOverlapSamples( iLeft, iRight );
	int iMaxInterpolationBufferLength = iLeft + int( ceil( m_oConfig.dMaximumSpeed * GetBlocklength( ) ) ) + iRight;
	m_sbInterpolationSrc.Init( iMaxInterpolationBufferLength, true );
}

CVAMachineSignalSource::~CVAMachineSignalSource( ) {}

int CVAMachineSignalSource::GetType( ) const
{
	return IVAAudioSignalSource::VA_SS_MACHINE;
}

std::string CVAMachineSignalSource::GetTypeString( ) const
{
	return "machine";
}

std::string CVAMachineSignalSource::GetDesc( ) const
{
	return std::string( "Creates a machine that can be started and stopped" );
}

IVAInterface* CVAMachineSignalSource::GetAssociatedCore( ) const
{
	return m_oConfig.pCore;
}

std::vector<const float*> CVAMachineSignalSource::GetStreamBlock( const CVAAudiostreamState* )
{
	const int iNewTransition = m_iNewTransition;

	m_sbOut.Zero( );

	// Missing sounds of machine not implemented yet
	if( !( m_oConfig.bRequiresIdleSound && m_oConfig.bRequiresIdleSound && m_oConfig.bRequiresStopSound ) )
		return { m_sbOut.data( ) };

	// Audiostream-synced data copy (todo use pointer)
	if( m_bStartSoundNew )
	{
		m_sbStartSound   = m_sbStartSoundNew;
		m_bStartSoundNew = false;
	}
	if( m_bIdleSoundNew )
	{
		m_sbIdleSound   = m_sbIdleSoundNew;
		m_bIdleSoundNew = false;
	}
	if( m_bStopSoundNew )
	{
		m_sbStopSound   = m_sbStopSoundNew;
		m_bStopSoundNew = false;
	}

	switch( m_iCurrentState )
	{
		case MACHINE_STATE_INVALID:
			if( m_bHasStartSound || m_bHasIdleSound )
				m_iCurrentState = MACHINE_STATE_STOPPED;
			break;
		case MACHINE_STATE_STOPPED:
			if( iNewTransition == MACHINE_TRANSITION_START ) // Startup
			{
				int iSamplesWritten;
				bool bEndReached;
				AddSamplesStarting( m_sbOut, iSamplesWritten, bEndReached );

				if( bEndReached )
				{
					// Short starting sample, jump directly into idle state
					AddSamplesIdling( m_sbOut, iSamplesWritten );
					m_iCurrentState = MACHINE_STATE_IDLING;
				}
				else
				{
					m_iCurrentState = MACHINE_STATE_STARTING;
				}
			}
			break;

		case MACHINE_STATE_STARTING:
			if( iNewTransition == MACHINE_TRANSITION_HALT )
			{
				int iSamplesWritten;
				bool bEndReached;
				AddSamplesStopping( m_sbOut, iSamplesWritten, bEndReached );

				if( bEndReached )
				{
					m_iCurrentState = MACHINE_STATE_STOPPED;
					ResetReadCursors( );
				}
			}
			else if( iNewTransition == MACHINE_TRANSITION_START )
			{
				int iSamplesWritten;
				bool bEndReached;
				AddSamplesStarting( m_sbOut, iSamplesWritten, bEndReached );

				if( bEndReached )
				{
					m_iCurrentState = MACHINE_STATE_IDLING;
					AddSamplesIdling( m_sbOut, iSamplesWritten );
				}
			}
			else
			{
				assert( false );
			}
			break;

		case MACHINE_STATE_IDLING:
			if( iNewTransition == MACHINE_TRANSITION_HALT )
			{
				AddSamplesIdling( m_sbOut, false );

				int iSamplesWritten;
				bool bEndReached;
				ITASampleBuffer sbOutTemp( m_sbOut );
				AddSamplesStopping( sbOutTemp, iSamplesWritten, bEndReached );

				if( m_bCrossfadeSounds )
				{
					// Fade in stopping sample buffer
					m_sbOut.Crossfade( sbOutTemp, 0, iSamplesWritten, ITABase::CrossfadeDirection::FROM_SOURCE, ITABase::FadingFunction::COSINE_SQUARE );
				}
				else
				{
					m_sbOut.write( sbOutTemp, iSamplesWritten );
					if( iSamplesWritten < m_sbOut.length( ) )
						m_sbOut.Zero( iSamplesWritten, m_sbOut.length( ) - iSamplesWritten );
				}

				if( bEndReached )
				{
					m_iCurrentState = MACHINE_STATE_STOPPED;
					ResetReadCursors( );
				}
				else
				{
					m_iCurrentState = MACHINE_STATE_HALTING;
				}
			}
			else
			{
				// Idling ...
				AddSamplesIdling( m_sbOut, false );
			}
			break;

		case MACHINE_STATE_HALTING:
			int iSamplesWritten;
			bool bEndReached;
			AddSamplesStopping( m_sbOut, iSamplesWritten, bEndReached );
			if( bEndReached )
			{
				m_iCurrentState = MACHINE_STATE_STOPPED;
				ResetReadCursors( );
			}
			break;

		default:
			m_sbOut.Zero( );
	}

	for( int i = 0; i < m_sbOut.length( ); i++ )
	{
		if( m_sbOut[i] > 1.0f || m_sbOut[i] < -1.0f )
			VA_WARN( "MachineSignalSource", "high energy detected" );
	}

	return { m_sbOut.data( ) };
}

void CVAMachineSignalSource::HandleRegistration( IVAInterface* pCore )
{
	m_pAssociatedCore = pCore;
}

void CVAMachineSignalSource::HandleUnregistration( IVAInterface* )
{
	m_pAssociatedCore = NULL;
}

std::string CVAMachineSignalSource::GetStateString( ) const
{
	MachineState iState = ( MachineState )(int)m_iCurrentState;
	return GetMachineStateString( iState );
}

std::string CVAMachineSignalSource::GetMachineStateString( MachineState iState ) const
{
	switch( iState )
	{
		case MACHINE_STATE_IDLING:
			return "idling";
		case MACHINE_STATE_INVALID:
			return "invalid";
		case MACHINE_STATE_STARTING:
			return "starting";
		case MACHINE_STATE_HALTING:
			return "halting";
		case MACHINE_STATE_STOPPED:
			return "stopped";
		default:
			return "unkown";
	}
}

void CVAMachineSignalSource::Reset( )
{
	m_iCurrentState  = MACHINE_STATE_INVALID;
	m_iNewTransition = MACHINE_TRANSITION_NONE;
	ResetReadCursors( );
	m_bStartSoundNew = m_bIdleSoundNew = m_bStopSoundNew = false;
}

void CVAMachineSignalSource::SetSpeed( double dSpeed )
{
	if( dSpeed <= 0 )
		VA_EXCEPT2( INVALID_PARAMETER, "Machine speed must be positive" );
	if( m_oConfig.bDisableInterpolation == true )
		VA_EXCEPT2( MODAL_ERROR, "Machine speed can not be set beause interpolation is deactivated" );
	m_fMachineSpeed = float( dSpeed );
}

double CVAMachineSignalSource::GetSpeed( ) const
{
	return double( m_fMachineSpeed );
}

void CVAMachineSignalSource::SetStartSound( const std::string& sFilePath )
{
	std::string sDestFilePath = m_pAssociatedCore->FindFilePath( sFilePath );
	if( sDestFilePath.empty( ) )
		VA_EXCEPT2( INVALID_PARAMETER, "Looked everywhere, but could not find file '" + sFilePath + "'" );

	ITAAudiofileReader* pReader = ITAAudiofileReader::create( sDestFilePath );
	int N                       = pReader->getAudiofileProperties( ).iLength;
	m_sbStartSoundNew.Init( N, false );
	m_sbStartSoundNew.write( pReader->read( N )[0], N );
	m_bHasStartSound = true;
	m_bStartSoundNew = true;
}

void CVAMachineSignalSource::SetIdleSound( const std::string& sFilePath )
{
	std::string sDestFilePath = m_pAssociatedCore->FindFilePath( sFilePath );
	if( sDestFilePath.empty( ) )
		VA_EXCEPT2( INVALID_PARAMETER, "Looked everywhere, but could not find file '" + sFilePath + "'" );

	ITAAudiofileReader* pReader = ITAAudiofileReader::create( sDestFilePath );
	int N                       = pReader->getAudiofileProperties( ).iLength;
	m_sbIdleSoundNew.Init( N, false );
	m_sbIdleSoundNew.write( pReader->read( N )[0], N );
	m_bHasIdleSound = true;
	m_bIdleSoundNew = true;
}

void CVAMachineSignalSource::SetStopSound( const std::string& sFilePath )
{
	std::string sDestFilePath = m_pAssociatedCore->FindFilePath( sFilePath );
	if( sDestFilePath.empty( ) )
		VA_EXCEPT2( INVALID_PARAMETER, "Looked everywhere, but could not find file '" + sFilePath + "'" );

	ITAAudiofileReader* pReader = ITAAudiofileReader::create( sDestFilePath );
	int N                       = pReader->getAudiofileProperties( ).iLength;
	m_sbStopSoundNew.Init( N, false );
	m_sbStopSoundNew.write( pReader->read( N )[0], N );
	m_bHasStopSound = true;
	m_bStopSoundNew = true;
}

CVAStruct CVAMachineSignalSource::GetParameters( const CVAStruct& oArgs ) const
{
	CVAStruct oRet;
	if( oArgs.HasKey( "GET" ) )
	{
		if( toUppercase( oArgs["GET"] ) == "STATE" )
			oRet["STATE"] = GetStateString( );
		else if( toUppercase( oArgs["GET"] ) == "SPEED" )
			oRet["SPEED"] = GetSpeed( );
		else
			VA_EXCEPT2( INVALID_PARAMETER, std::string( "Unkown command: '" + oArgs["GET"].ToString( ) + "'" ) );
	}
	return oRet;
}

void CVAMachineSignalSource::SetParameters( const CVAStruct& oParams )
{
	const CVAStructValue* pStruct;

	if( ( pStruct = oParams.GetValue( "PRINT" ) ) != nullptr )
	{
		if( pStruct->GetDatatype( ) != CVAStructValue::STRING )
			VA_EXCEPT2( INVALID_PARAMETER, "Print value must be a string" );
		std::string sValue = toUppercase( *pStruct );
		if( sValue == "HELP" )
		{
			VA_PRINT( "Available commands: 'print' 'help|status'; 'get' 'S'; 'set' 'S' 'value' <double>" );
			return;
		}

		if( sValue == "STATUS" )
		{
			VA_PRINT( "Machine speed: " << m_fMachineSpeed );
			return;
		}
	}
	else if( ( pStruct = oParams.GetValue( "SET" ) ) != nullptr )
	{
		if( pStruct->GetDatatype( ) != CVAStructValue::STRING )
			VA_EXCEPT2( INVALID_PARAMETER, "Setter key name must be a string" );

		std::string sValue = toUppercase( *pStruct );
		if( sValue == "S" || sValue == "SPEED" )
		{
			pStruct = oParams.GetValue( "VALUE" );
			if( ( pStruct->GetDatatype( ) == CVAStructValue::DOUBLE ) || ( pStruct->GetDatatype( ) == CVAStructValue::INT ) ||
			    ( pStruct->GetDatatype( ) == CVAStructValue::BOOL ) )
			{
				double dS = *pStruct;
				if( dS > 0 )
					SetSpeed( dS );
				else
					VA_EXCEPT2( INVALID_PARAMETER, "Machine speed must be greater than zero" );
			}
			return;
		}
		else if( sValue == "ACTION" )
		{
			std::string sItem = toUppercase( *oParams.GetValue( "VALUE" ) );
			if( sItem == "START" )
				Start( );
			else if( sItem == "STOP" )
				Halt( );
			else
				VA_EXCEPT2( INVALID_PARAMETER, "Could not read action value" );
		}
		else if( sValue == "STARTSOUNDFILEPATH" )
		{
			pStruct = oParams.GetValue( "VALUE" );
			if( pStruct->GetDatatype( ) != CVAStructValue::STRING )
				VA_EXCEPT2( INVALID_PARAMETER, "File path has to be a string" );

			const std::string& sPathRaw = *pStruct;
			std::string sPath           = m_oConfig.pCore->SubstituteMacros( sPathRaw );
			SetStartSound( sPath );
		}
		else if( sValue == "IDLESOUNDFILEPATH" )
		{
			pStruct = oParams.GetValue( "VALUE" );
			if( pStruct->GetDatatype( ) != CVAStructValue::STRING )
				VA_EXCEPT2( INVALID_PARAMETER, "File path has to be a string" );

			const std::string& sPathRaw = *pStruct;
			std::string sPath           = m_oConfig.pCore->SubstituteMacros( sPathRaw );
			SetIdleSound( sPath );
		}
		else if( sValue == "STOPSOUNDFILEPATH" )
		{
			pStruct = oParams.GetValue( "VALUE" );
			if( pStruct->GetDatatype( ) != CVAStructValue::STRING )
				VA_EXCEPT2( INVALID_PARAMETER, "File path has to be a string" );

			const std::string& sPathRaw = *pStruct;
			std::string sPath           = m_oConfig.pCore->SubstituteMacros( sPathRaw );
			SetStopSound( sPath );
		}
		return;
	}
	else
	{
		VA_EXCEPT2( INVALID_PARAMETER, "Could not interpret parameters" );
	}

	return;
}

void CVAMachineSignalSource::SetCrossfadeSoundsEnabled( bool bEnabled )
{
	m_bCrossfadeSounds = bEnabled;
}

bool CVAMachineSignalSource::GetCrossfadeSoundsEnabled( ) const
{
	return m_bCrossfadeSounds;
}

void CVAMachineSignalSource::AddSamplesStarting( ITASampleBuffer& sfOut, int& iSamplesWritten, bool& bEndReached )
{
	if( !m_bHasStartSound )
	{
		iSamplesWritten = 0;
		bEndReached     = true;
		return;
	}

	int iSamples = m_sbStartSound.length( ) - m_iStartingSoundCursor;
	if( iSamples > 0 )
	{
		iSamples = ( std::min )( iSamples, sfOut.length( ) );
		sfOut.write( m_sbStartSound, iSamples, m_iStartingSoundCursor );
		iSamplesWritten = iSamples;

		m_iStartingSoundCursor += iSamples;
		if( m_iStartingSoundCursor >= m_sbStartSound.length( ) )
			bEndReached = true;
		else
			bEndReached = false;
	}
	else
	{
		iSamplesWritten        = 0;
		bEndReached            = true;
		m_iStartingSoundCursor = 0;
	}
}

void CVAMachineSignalSource::AddSamplesIdling( ITASampleBuffer& sbOut, int iOutputOffset /*=0 */ )
{
	if( !m_bHasIdleSound )
	{
		return;
	}

	assert( sbOut.length( ) == int( GetBlocklength( ) ) );
	assert( iOutputOffset >= 0 && iOutputOffset < sbOut.length( ) );
	assert( iOutputOffset < sbOut.length( ) );

	int iSize;
	if( m_oConfig.bDisableInterpolation == true )
	{
		iSize = GetBlocklength( ) - iOutputOffset;
		m_sbIdleSound.cyclic_read( sbOut.data( ), iSize, m_iIdlingSoundCursor );
	}
	else
	{
		int iLeft, iRight;
		m_pInterpRoutine->GetOverlapSamples( iLeft, iRight );
		iSize = int( m_fMachineSpeed * GetBlocklength( ) ) - iOutputOffset;
		assert( iLeft + iSize + iRight <= m_sbInterpolationSrc.length( ) );
		int iSrcOffset = m_iIdlingSoundCursor - iLeft;
		if( iSrcOffset < 0 )
			iSrcOffset += m_sbIdleSound.length( );
		m_sbIdleSound.cyclic_read( m_sbInterpolationSrc.data( ), iLeft + iSize + iRight, iSrcOffset ); // copy

		int iInputLength = iLeft + iSize;
		m_pInterpRoutine->Interpolate( &m_sbInterpolationSrc, iInputLength, iLeft, &sbOut, sbOut.length( ) - iOutputOffset, iOutputOffset );
	}

	m_iIdlingSoundCursor += iSize;
	if( m_iIdlingSoundCursor >= m_sbIdleSound.length( ) )
		m_iIdlingSoundCursor = m_iIdlingSoundCursor % m_sbIdleSound.length( );
}

void CVAMachineSignalSource::AddSamplesStopping( ITASampleBuffer& sfOut, int& iSamplesWritten, bool& bEndReached )
{
	if( !m_bHasStopSound )
	{
		iSamplesWritten = 0;
		bEndReached     = true;
		return;
	}

	int iResidualSamples = m_sbStopSound.length( ) - m_iStoppingSoundCursor;
	if( iResidualSamples > 0 )
	{
		iResidualSamples = ( std::min )( iResidualSamples, sfOut.length( ) );
		sfOut.write( m_sbStopSound, iResidualSamples, m_iStoppingSoundCursor );
		iSamplesWritten = iResidualSamples;

		m_iStoppingSoundCursor += iResidualSamples;
		if( m_iStoppingSoundCursor >= m_sbStopSound.length( ) )
			bEndReached = true;
		else
			bEndReached = false;
	}
	else
	{
		iSamplesWritten        = 0;
		bEndReached            = true;
		m_iStoppingSoundCursor = 0;
	}
}

void CVAMachineSignalSource::ResetReadCursors( )
{
	m_iStartingSoundCursor = 0;
	m_iIdlingSoundCursor   = 0;
	m_iStoppingSoundCursor = 0;
}

void CVAMachineSignalSource::Start( )
{
	m_iNewTransition = MACHINE_TRANSITION_START;
}

void CVAMachineSignalSource::Halt( )
{
	m_iNewTransition = MACHINE_TRANSITION_HALT;
}
