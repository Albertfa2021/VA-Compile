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

#include "VAPTDummyAudioRenderer.h"

// VA includes
#include "../../../Utils/VAUtils.h"
#include "../../../VAHardwareSetup.h"
#include "../../../core/core.h"

#ifdef VACORE_WITH_RENDERER_PROTOTYPE_DUMMY

CVAPTDummyAudioRenderer::CVAPTDummyAudioRenderer( const CVAAudioRendererInitParams& oParams ) : oParams( oParams )
{
	CVAConfigInterpreter conf( *oParams.pConfig );

	std::string sOutput;
	conf.ReqString( "OutputGroup", sOutput );
	const CVAHardwareOutput* pOutput = oParams.pCore->GetCoreConfig( )->oHardwareSetup.GetOutput( sOutput );

	if( pOutput == nullptr )
		VA_EXCEPT2( INVALID_PARAMETER, "DummyAudioRenderer: '" + sOutput + "' is not a valid hardware output group. Disabled?" );

	int iNumChannels   = int( pOutput->GetPhysicalOutputChannels( ).size( ) );
	double dSampleRate = oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.dSampleRate;
	int iBlockLength   = oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.iBuffersize;
	m_pDataSource      = new ITADatasourceRealization( iNumChannels, dSampleRate, iBlockLength );
	m_pDataSource->SetStreamEventHandler( this );
}

CVAPTDummyAudioRenderer::~CVAPTDummyAudioRenderer( )
{
	delete m_pDataSource;
	m_pDataSource = NULL;
};

void CVAPTDummyAudioRenderer::HandleProcessStream( ITADatasourceRealization*, const ITAStreamInfo* )
{
	for( int n = 0; n < int( m_pDataSource->GetNumberOfChannels( ) ); n++ )
		for( int i = 0; i < int( m_pDataSource->GetBlocklength( ) ); i++ )
			m_pDataSource->GetWritePointer( n )[i] = 0.0f;
	m_pDataSource->IncrementWritePointer( );
}

#endif // VACORE_WITH_RENDERER_PROTOTYPE_DUMMY
