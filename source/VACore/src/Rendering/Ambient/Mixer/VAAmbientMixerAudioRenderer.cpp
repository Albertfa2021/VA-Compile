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

#include "VAAmbientMixerAudioRenderer.h"

// VA includes
#include "../../../Audiosignals/VAAudiofileSignalSource.h"
#include "../../../Scene/VASceneManager.h"
#include "../../../Scene/VASoundSourceDesc.h"
#include "../../../Scene/VASoundSourceState.h"
#include "../../../Utils/VAUtils.h"
#include "../../../VAHardwareSetup.h"
#include "../../../core/core.h"

#include <VAAudioSignalSource.h>

#ifdef VACORE_WITH_SAMPLER_SUPPORT
#	include <ITASoundSampler.h>
#endif


// ITA includes
#include <ITAFastMath.h>
#include <ITASampleFrame.h>

// STL includes
#include <assert.h>
#include <stdlib.h>
#include <string>

#ifdef VACORE_WITH_RENDERER_AMBIENT_MIXER

CVAAmbientMixerAudioRenderer::CVAAmbientMixerAudioRenderer( const CVAAudioRendererInitParams& oParams )
    : m_oParams( oParams )
    , m_pNewSceneState( NULL )
    , m_pCurSceneState( NULL )
    , m_bIndicateReset( false )
    , m_bResetAck( false )
    , m_pSampler( NULL )
    , m_pDataSource( NULL )
{
	assert( m_oParams.vsReproductions.size( ) > 0 );

	double dSampleRate = m_oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.dSampleRate;
	int iBlockLength   = m_oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.iBuffersize;
	int iNumChannels   = -1;

	CVAConfigInterpreter conf( *m_oParams.pConfig );

	std::string sOutput;
	conf.OptString( "OutputGroup", sOutput );

	if( !sOutput.empty( ) )
	{
		const CVAHardwareOutput* pOutput = m_oParams.pCore->GetCoreConfig( )->oHardwareSetup.GetOutput( sOutput );

		if( pOutput == nullptr )
			VA_EXCEPT2( INVALID_PARAMETER, "AmbientMixerAudioRenderer: '" + sOutput + "' is not a valid hardware output group. Is the output group disabled?" );

		iNumChannels = int( pOutput->GetPhysicalOutputChannels( ).size( ) );
	}

	int iNumChannelsExplicit = -1;
	if( conf.OptInteger( "NumChannels", iNumChannelsExplicit, -1 ) )
	{
		if( iNumChannels > 0 && iNumChannelsExplicit != iNumChannels )
			VA_EXCEPT2( INVALID_PARAMETER, "AmbientMixerAudioRenderer: '" + sOutput + "' and explicitly requested number of '" +
			                                   std::to_string( long( iNumChannelsExplicit ) ) + "' channels do not match." );
		iNumChannels = iNumChannelsExplicit;
	}

	m_pDataSource = new ITADatasourceRealization( iNumChannels, dSampleRate, iBlockLength );
	m_pDataSource->SetStreamEventHandler( this );


	conf.OptBool( "SignalSourceMixingEnabled", m_bSignalSourceMixingEnabled, true );
	conf.OptBool( "SamplerEnabled", m_bSamplerEnabled, false );

	if( m_bSamplerEnabled )
	{
#	ifdef VACORE_WITH_SAMPLER_SUPPORT
		m_pSampler = ITASoundSampler::Create( iNumChannels, dSampleRate, iBlockLength );
		m_pSampler->AddTrack( iNumChannels ); // Add one track with all channels
#	else
		VA_EXCEPT_NOT_IMPLEMENTED;
#	endif
	}
}

CVAAmbientMixerAudioRenderer::~CVAAmbientMixerAudioRenderer( )
{
	delete m_pDataSource;
	m_pDataSource = nullptr;
#	ifdef VACORE_WITH_SAMPLER_SUPPORT
	delete m_pSampler;
	m_pSampler = nullptr;
#	endif
}

void CVAAmbientMixerAudioRenderer::HandleProcessStream( ITADatasourceRealization*, const ITAStreamInfo* )
{
	if( m_bIndicateReset )
	{
		m_bResetAck      = true;
		m_bIndicateReset = false;
	}

	// Clear output buffers first
	for( int n = 0; n < int( GetOutputDatasource( )->GetNumberOfChannels( ) ); n++ )
	{
		float* pfOutBuf = m_pDataSource->GetWritePointer( (unsigned int)( n ) );
		fm_zero( pfOutBuf, m_pDataSource->GetBlocklength( ) );
	}

	// Mix sound source signals

	if( m_pNewSceneState )
	{
		if( m_pCurSceneState )
			m_pCurSceneState->RemoveReference( );
		m_pCurSceneState = m_pNewSceneState;
		m_pNewSceneState = NULL;
	}

	if( m_bSignalSourceMixingEnabled && m_pCurSceneState )
	{
		// Gather all incoming audio streams from sound sources and mix them to the output stream
		std::vector<int> viSoundSourceIDs;
		m_pCurSceneState->GetSoundSourceIDs( &viSoundSourceIDs );

		for( auto iID: viSoundSourceIDs )
		{
			CVASoundSourceDesc* pDesc   = m_oParams.pCore->GetSceneManager( )->GetSoundSourceDesc( iID );
			CVASoundSourceState* pState = m_pCurSceneState->GetSoundSourceState( iID );

			assert( pDesc && pState );

			bool bForThisRenderer = ( pDesc->sExplicitRendererID == m_oParams.sID || pDesc->sExplicitRendererID.empty( ) );
			if( !bForThisRenderer || pDesc->bMuted )
				continue;

			const double dGainFactor = pState->GetVolume( m_oParams.pCore->GetCoreConfig( )->dDefaultAmplitudeCalibration );

			const ITASampleFrame& sfInput( *pDesc->psfSignalSourceInputBuf );

			// Add samples to out buffer
			for( int n = 0; n < int( m_pDataSource->GetNumberOfChannels( ) ); n++ )
			{
				float* pfOutBuf     = m_pDataSource->GetWritePointer( (unsigned int)( n ) );
				int iInChannelIndex = (std::min)( int( sfInput.GetNumChannels( ) - 1 ), n );

				for( int j = 0; j < int( m_pDataSource->GetBlocklength( ) ); j++ )
					pfOutBuf[j] += sfInput[iInChannelIndex].GetData( )[j] * float( dGainFactor ); // mixing, if possible
			}
		}
	}

#	ifdef VACORE_WITH_SAMPLER_SUPPORT
	// Mix sound sampler output
	if( m_bSamplerEnabled && m_pSampler )
	{
		// Add samples to out buffer
		for( unsigned int n = 0; n < m_pSampler->GetNumberOfChannels( ); n++ )
		{
			const float* pfInBuf = m_pSampler->GetBlockPointer( n, &oSamplerStreamInfo );
			float* pfOutBuf      = m_pDataSource->GetWritePointer( (unsigned int)( n ) );
			fm_add( pfOutBuf, pfInBuf, m_pDataSource->GetBlocklength( ) ); // mixing
		}

		oSamplerStreamInfo.dSysTimeCode = ITAClock::getDefaultClock( )->getTime( );
		oSamplerStreamInfo.nSamples += m_pSampler->GetBlocklength( );
		double dSampleRate = m_pSampler->GetSampleRate( );
		assert( dSampleRate );
		oSamplerStreamInfo.dStreamTimeCode = (double)( oSamplerStreamInfo.nSamples ) / dSampleRate;

		m_pSampler->IncrementBlockPointer( );
	}
#	endif

	m_pDataSource->IncrementWritePointer( );
}

ITADatasource* CVAAmbientMixerAudioRenderer::GetOutputDatasource( )
{
	return m_pDataSource;
}

void CVAAmbientMixerAudioRenderer::Reset( )
{
	m_bIndicateReset = true;

	while( !m_bResetAck )
		VASleep( 50 );

	if( m_pCurSceneState )
		m_pCurSceneState->RemoveReference( );

	m_pCurSceneState = NULL;

	m_bResetAck = false;
}

void CVAAmbientMixerAudioRenderer::UpdateScene( CVASceneState* pNewState )
{
	pNewState->AddReference( );
	m_pNewSceneState = pNewState;
}

void CVAAmbientMixerAudioRenderer::SetParameters( const CVAStruct& oInArgs )
{
#	ifdef VACORE_WITH_SAMPLER_SUPPORT
	if( oInArgs.HasKey( "Sampler" ) )
	{
		const CVAStruct& oSamplerUpdate( oInArgs["Sampler"] );
		int iTrack = 0; // First track
		if( oSamplerUpdate.HasKey( "NewTrack" ) )
		{
			iTrack = m_pSampler->AddTrack( oSamplerUpdate["NewTrack"] );
		}
		if( oSamplerUpdate.HasKey( "SampleFilePath" ) )
		{
			std::string sFilePathRaw = oSamplerUpdate["SampleFilePath"];
			std::string sFilePath    = m_oParams.pCore->FindFilePath( sFilePathRaw );
			int iNewSampleID         = m_pSampler->LoadSample( sFilePath );
			int iSampleCount         = 0; // Immediate playback
			m_pSampler->AddPlaybackBySamplecount( iNewSampleID, iTrack, iSampleCount );
		}
	}
#	endif
}

CVAStruct CVAAmbientMixerAudioRenderer::GetParameters( const CVAStruct& oInArgs ) const
{
	CVAStruct oParameters;
	oParameters["SamplerEnabled"]            = m_bSamplerEnabled;
	oParameters["SignalSourceMixingEnabled"] = m_bSignalSourceMixingEnabled;
	oParameters["InitParams"]                = *( m_oParams.pConfig );

#	ifdef VACORE_WITH_SAMPLER_SUPPORT
	if( m_pSampler )
	{
		CVAStruct oSamplerInfo;
		oSamplerInfo["NumChannels"]  = (int)m_pSampler->GetNumberOfChannels( );
		oSamplerInfo["SamplingRate"] = m_pSampler->GetSampleRate( );
		oSamplerInfo["BlockLength"]  = (int)m_pSampler->GetBlocklength( );
		oSamplerInfo["SampleCount"]  = m_pSampler->GetSampleCount( );

		oParameters["Sampler"] = oSamplerInfo;
	}
#	endif

	if( oInArgs.HasKey( "help" ) || oInArgs.HasKey( "doc" ) || oInArgs.HasKey( "info" ) )
	{
		CVAStruct oHelpSampler;
		oHelpSampler["AddTrack"] = "To add a new sample, use  ...";

		CVAStruct oHelp;
		oHelp["Sampler"] = oHelpSampler;
	}

	return oParameters;
}

#endif // VACORE_WITH_RENDERER_AMBIENT_MIXER
