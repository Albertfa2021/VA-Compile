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

#include "VAReproductionTalkthrough.h"

#ifdef VACORE_WITH_REPRODUCTION_TALKTHROUGH

#	include "../../core/core.h"

#	include <ITAFastMath.h>

CVAReproductionTalkthrough::CVAReproductionTalkthrough( const CVAAudioReproductionInitParams& oParams )
    : m_oParams( oParams )
    , m_pdsInputDatasource( NULL )
    , m_pdsOutputDatasource( NULL )
{
	// Retrieve num channels from first output group
	int iNumChannels      = int( m_oParams.vpOutputs[0]->GetPhysicalOutputChannels( ).size( ) );
	m_pdsOutputDatasource = new ITADatasourceRealization( iNumChannels, oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.dSampleRate,
	                                                      oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.iBuffersize );
	m_pdsOutputDatasource->SetStreamEventHandler( this );
}

CVAReproductionTalkthrough::~CVAReproductionTalkthrough( )
{
	delete m_pdsOutputDatasource;

	return;
}

void CVAReproductionTalkthrough::SetInputDatasource( ITADatasource* pDatasource )
{
	m_pdsInputDatasource = pDatasource;

	return;
}

ITADatasource* CVAReproductionTalkthrough::GetOutputDatasource( )
{
	return m_pdsOutputDatasource;
}

void CVAReproductionTalkthrough::HandleProcessStream( ITADatasourceRealization*, const ITAStreamInfo* pStreamInfo )
{
	if( m_pdsInputDatasource == nullptr )
		return;

	for( unsigned int i = 0; i < m_pdsOutputDatasource->GetNumberOfChannels( ); i++ )
	{
		float* pfOut      = m_pdsOutputDatasource->GetWritePointer( i );
		const float* pfIn = m_pdsInputDatasource->GetBlockPointer( i, pStreamInfo );
		fm_copy( pfOut, pfIn, m_pdsOutputDatasource->GetBlocklength( ) );
	}

	m_pdsOutputDatasource->IncrementWritePointer( );

	return;
}

void CVAReproductionTalkthrough::HandlePostIncrementBlockPointer( ITADatasourceRealization* )
{
	if( m_pdsInputDatasource == nullptr )
		return;

	m_pdsInputDatasource->IncrementBlockPointer( );

	return;
}

int CVAReproductionTalkthrough::GetNumInputChannels( ) const
{
	return m_pdsOutputDatasource->GetNumberOfChannels( );
}

#endif // VACORE_WITH_REPRODUCTION_TALKTHROUGH
