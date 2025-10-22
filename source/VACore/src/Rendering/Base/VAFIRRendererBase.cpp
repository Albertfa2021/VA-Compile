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

#include "VAFIRRendererBase.h"

#include "../../Scene/VASoundSourceDesc.h"
#include "../../Scene/VASoundSourceState.h"
#include "../../Utils/VAUtils.h"
#include "VAAudioRendererReceiver.h"
#include "VAAudioRendererSource.h"

#include <ITASampleFrame.h>
#include <ITAVariableDelayLine.h>
#include <ITAUPConvolution.h>
#include <ITAUPFilter.h>



CVAFIRRendererBase::Config::Config( const CVAAudioRendererInitParams& oParams, const Config& oDefaultValues ) : CVAAudioRendererBase::Config( oParams, oDefaultValues )
{
	CVAConfigInterpreter conf( *oParams.pConfig );
	const std::string sExceptionMsgPrefix = "Renderer ID '" + oParams.sID + "': ";

	conf.ReqInteger( "NumChannels", iNumChannels );
	if( iNumChannels < 1 )
		VA_EXCEPT2( INVALID_PARAMETER, sExceptionMsgPrefix + "Number of channels must be >= 1" );

	conf.OptInteger( "MaxFilterLengthSamples", iMaxFilterLengthSamples, oDefaultValues.iMaxFilterLengthSamples );
	if( iMaxFilterLengthSamples < 0 )
		VA_EXCEPT2( INVALID_PARAMETER, sExceptionMsgPrefix + "IR filter size must be positive" );
}



CVAFIRRendererBase::CVAFIRRendererBase( const CVAAudioRendererInitParams& oParams, const Config& oDefaultValues )
     : CVAAudioRendererBase( oParams, oDefaultValues )
     , m_oConf( Config( oParams, oDefaultValues ) )
 {
	 InitBaseDSPEntities( m_oConf.iNumChannels );
 }


 void CVAFIRRendererBase::SetParameters( const CVAStruct& oParams )
 {
	 CVAAudioRendererBase::SetParameters( oParams );
 }




CVAFIRRendererBase::SourceReceiverPair::SourceReceiverPair( const Config& oConf ) : m_oConf( oConf )
{
	const int iBlockSize = m_oConf.oCoreConfig.oAudioDriverConfig.iBuffersize;
	const int iChannels  = m_oConf.iNumChannels;

	m_sbTemp       = ITASampleBuffer( m_oConf.iBlockSize );
	m_sfFIRFilters = ITASampleFrame( iChannels, m_oConf.iMaxFilterLengthSamples, true );

	const float fMaxDelaySamples = m_oConf.fMaxDelaySeconds * m_oConf.dSampleRate;
	m_pVariableDelayLine         = std::make_unique<CITAVariableDelayLine>( m_oConf.dSampleRate, m_oConf.iBlockSize, fMaxDelaySamples, oConf.iVDLSwitchingAlgorithm );
	m_vpConvolvers.resize( iChannels );
	for( auto&& pConvolver: m_vpConvolvers )
	{
		pConvolver = std::make_shared<ITAUPConvolution>( iBlockSize, m_oConf.iMaxFilterLengthSamples );
		pConvolver->SetFilterExchangeFadingFunction( ITABase::FadingFunction::COSINE_SQUARE );
		pConvolver->SetFilterCrossfadeLength( (std::min)( iBlockSize, 32 ) );
	}
	Reset( );
}

void CVAFIRRendererBase::SourceReceiverPair::PreRequest( )
{
	CVAAudioRendererBase::SourceReceiverPair::PreRequest( );
}
void CVAFIRRendererBase::SourceReceiverPair::PreRelease( )
{
	CVAAudioRendererBase::SourceReceiverPair::PreRelease( );
	Reset( );
}


void CVAFIRRendererBase::SourceReceiverPair::Process( double dTimeStamp, const AuralizationMode& oAuraMode, const CVASoundSourceState& oSourceState,
                                                            const CVAReceiverState& oReceiverState )
{
	RunSimulation( dTimeStamp );

	//TODO: Do we need a multi-threading lock or equal for m_sfFIRFilters?
	PrepareDelayAndFilters( dTimeStamp, oAuraMode, m_dPropagationDelay, m_sfFIRFilters, m_bInaudible );
	UpdateDSPElements( oAuraMode, oSourceState );
	ProcessDSPElements( );
}



void CVAFIRRendererBase::SourceReceiverPair::Reset( )
{
	m_pVariableDelayLine->Clear( );
	for( auto pConvolver: m_vpConvolvers )
	{
		pConvolver->clear( );
		pConvolver->SetGain( 0.0f, true );
	}
}

void CVAFIRRendererBase::SourceReceiverPair::UpdateDSPElements( const AuralizationMode& oAuralizationMode, const CVASoundSourceState& oSourceState )
{
	bool bDPEnabledCurrent = ( m_pVariableDelayLine->GetAlgorithm( ) != EVDLAlgorithm::SWITCH ); // switch = disabled
	bool bDPStatusChanged  = ( bDPEnabledCurrent != oAuralizationMode.bDoppler );
	if( bDPStatusChanged )
		m_pVariableDelayLine->SetAlgorithm( !bDPEnabledCurrent ? m_oConf.iVDLSwitchingAlgorithm : EVDLAlgorithm::SWITCH );
	m_pVariableDelayLine->SetDelayTime( float( m_dPropagationDelay ) ); // TODO: Should we keep the delay fixed if DP is disabled?


	const bool bMuted = m_bInaudible || GetSource( )->pData->bMuted;
	const float fGain = bMuted ? 0.0f : (float)oSourceState.GetVolume( m_oConf.oCoreConfig.dDefaultAmplitudeCalibration );

	int iChannel = 0;
	for( auto pConvolver: m_vpConvolvers )
	{
		auto pFilter = pConvolver->RequestFilter( );
		auto pData   = m_sfFIRFilters[iChannel].data( );

		const int iCount = (std::min)( m_sfFIRFilters.GetLength( ), pConvolver->GetMaxFilterlength( ) );
		pFilter->Load( pData, iCount );
		pConvolver->ExchangeFilter( pFilter );
		pConvolver->ReleaseFilter( pFilter );

		pConvolver->SetGain( fGain );
		iChannel++;
	}
}

void CVAFIRRendererBase::SourceReceiverPair::ProcessDSPElements( )
{
	CVASoundSourceDesc* pSourceData = GetSource( )->pData;
	const ITASampleFrame& sbInput( *pSourceData->psfSignalSourceInputBuf ); // Always use first channel of SignalSource
	assert( pSourceData->iID >= 0 ); // Knallt es hier, dann wurde dem SoundPath unterm Hintern die Quelle entzogen! -> Problem mit Referenzierung und Reset?
	ITASampleFrame& sfOutput = *GetReceiver( )->psfOutput.get( );
	assert( sfOutput.GetNumChannels( ) == m_vpConvolvers.size( ) );

	m_pVariableDelayLine->Process( &(sbInput[0]), &( m_sbTemp ) );

	for( int iChannel = 0; iChannel < m_vpConvolvers.size( ); iChannel++ )
		m_vpConvolvers[iChannel]->Process( m_sbTemp.data( ), sfOutput[iChannel].data( ), ITABase::MixingMethod::ADD );
}
