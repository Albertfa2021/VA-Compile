
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

#include "VAReproductionLowFrequencyMixer.h"

#ifdef VACORE_WITH_REPRODUCTION_MIXER_LOW_FREQUENCY

#	include "../../Utils/VAUtils.h"
#	include "../../core/core.h"

#	include <ITAFastMath.h>
#	include <ITAStringUtils.h>

CVAReproductionLowFrequencyMixer::CVAReproductionLowFrequencyMixer( const CVAAudioReproductionInitParams& oParams )
    : m_oParams( oParams )
    , m_pdsInputDataSource( NULL )
    , m_pdsMixingTable( NULL )
{
	CVAConfigInterpreter conf( *oParams.pConfig );

	std::string sMixingChannelsRAW;
	conf.OptString( "MixingChannels", sMixingChannelsRAW, "ALL" );

	if( sMixingChannelsRAW != "ALL" && !sMixingChannelsRAW.empty( ) )
	{
		m_viMixingChannels = StringToIntVec( sMixingChannelsRAW );
	}
	else
	{
		m_viMixingChannels.push_back( 1 ); // Use first channel as default input mixing channel
	}

	m_pdsMixingTable = new ITADatasourceRealization( int( m_viMixingChannels.size( ) ), oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.dSampleRate,
	                                                 oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.iBuffersize );
	m_pdsMixingTable->SetStreamEventHandler( this );
}

CVAReproductionLowFrequencyMixer::~CVAReproductionLowFrequencyMixer( )
{
	delete m_pdsMixingTable;
}

void CVAReproductionLowFrequencyMixer::SetInputDatasource( ITADatasource* pDataSource )
{
	m_pdsInputDataSource = pDataSource;

	for( size_t i = 0; i < m_viMixingChannels.size( ); i++ )
		if( m_viMixingChannels[i] > int( m_pdsInputDataSource->GetNumberOfChannels( ) ) )
			VA_EXCEPT2( INVALID_PARAMETER, "Could not connect this data source to LFE mixer because mixing channel exceeds number of input channels" );
}

ITADatasource* CVAReproductionLowFrequencyMixer::GetOutputDatasource( )
{
	return m_pdsMixingTable;
}

void CVAReproductionLowFrequencyMixer::HandleProcessStream( ITADatasourceRealization*, const ITAStreamInfo* pStreamInfo )
{
	if( m_pdsInputDataSource == nullptr )
		return;

	for( int i = 0; i < int( m_pdsMixingTable->GetNumberOfChannels( ) ); i++ )
	{
		float* pfOut = m_pdsMixingTable->GetWritePointer( i );
		fm_zero( pfOut, m_pdsMixingTable->GetBlocklength( ) );

		for( size_t j = 0; j < m_viMixingChannels.size( ); j++ )
		{
			const float* pfIn = m_pdsInputDataSource->GetBlockPointer( m_viMixingChannels[j], pStreamInfo );
			fm_add( pfOut, pfIn, m_pdsMixingTable->GetBlocklength( ) );
		}

		if( m_viMixingChannels.size( ) > 1 )
			fm_mul( pfOut, 1 / float( m_viMixingChannels.size( ) ), m_pdsMixingTable->GetBlocklength( ) );
	}

	m_pdsMixingTable->IncrementWritePointer( );

	return;
}

void CVAReproductionLowFrequencyMixer::HandlePostIncrementBlockPointer( ITADatasourceRealization* pSender )
{
	if( m_pdsInputDataSource == nullptr )
		return;

	m_pdsInputDataSource->IncrementBlockPointer( );

	return;
}

int CVAReproductionLowFrequencyMixer::GetNumInputChannels( ) const
{
	return m_pdsMixingTable->GetNumberOfChannels( );
}

#endif // VACORE_WITH_REPRODUCTION_MIXER_LOW_FREQUENCY
