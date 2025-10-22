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

#include "VAReproductionBinauralMixdown.h"

#ifdef VACORE_WITH_REPRODUCTION_BINAURAL_MIXDOWN

#	include "../../Scene/VAMotionState.h"
#	include "../../Scene/VASceneState.h"
#	include "../../Scene/VASoundReceiverState.h"
#	include "../../Utils/VAUtils.h"
#	include "../../VAHardwareSetup.h"
#	include "../../directivities/VADirectivityDAFFHRIR.h"

#	include <ITADataSourceRealization.h>
#	include <ITAFastMath.h>
#	include <ITANumericUtils.h>
#	include <ITAUPConvolution.h>
#	include <ITAUPFilter.h>
#	include <ITAUPFilterPool.h>
#	include <ITAUPTrigger.h>
#	include <VistaBase/VistaQuaternion.h>
#	include <VistaBase/VistaTransformMatrix.h>
#	include <VistaBase/VistaVector3D.h>
#	include <xutility>

class StreamFilter : public ITADatasourceRealization
{
public:
	ITADatasource* pdsInput;
	ITAUPFilterPool* pFilterPool;
	ITAUPTrigger* pTrigger;
	ITAUPFilter* pDirac;

	std::vector<ITAUPConvolution*> vpConvolver;

	StreamFilter( int iNumChannels, double dSampleRate, int iBlockLength, int iMaxFilterLength );
	~StreamFilter( );
	void SetIdentity( );
	void SetFilter( const int iIndex, ITAUPFilter* pFilterChL, ITAUPFilter* pFilterChR );
	void ProcessStream( const ITAStreamInfo* pStreamInfo );
	void PostIncrementBlockPointer( );
};

CVABinauralMixdownReproduction::CVABinauralMixdownReproduction( const CVAAudioReproductionInitParams& oParams )
    : m_oParams( oParams )
    , m_pdsStreamFilter( NULL )
    , m_pVirtualOutput( NULL )
    , m_iListenerID( -1 )
{
	CVAConfigInterpreter conf( *( m_oParams.pConfig ) );

	conf.OptInteger( "HRIRFilterLength", m_iHRIRFilterLength, 128 );
	double dSampleRate = m_oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.dSampleRate;
	m_sfHRIRTemp.init( 2, m_iHRIRFilterLength, true );

	// Validate outputs
	for( size_t i = 0; i < m_oParams.vpOutputs.size( ); i++ )
	{
		const CVAHardwareOutput* pTargetOutput = m_oParams.vpOutputs[i];
		if( pTargetOutput == nullptr )
			VA_EXCEPT2( INVALID_PARAMETER, "Unrecognized output '" + pTargetOutput->sIdentifier + "' for binaural mixdown reproduction" );
		if( pTargetOutput->GetPhysicalOutputChannels( ).size( ) != 2 )
			VA_EXCEPT2( INVALID_PARAMETER, "Expecting two channels for binaural downmix target, problem with output '" + pTargetOutput->sIdentifier + "' detected" );
		m_vpTargetOutputs.push_back( pTargetOutput );
	}

	std::string sVirtualOutput;
	conf.ReqString( "VirtualOutput", sVirtualOutput );

	m_pVirtualOutput = m_oParams.pCore->GetCoreConfig( )->oHardwareSetup.GetOutput( sVirtualOutput );
	if( m_pVirtualOutput == nullptr )
		VA_EXCEPT2( INVALID_PARAMETER, "Unrecognized virtual output '" + sVirtualOutput + "' for binaural mixdown reproduction" );

	conf.OptInteger( "TrackedListenerID", m_iListenerID, 1 );

	int iNumChannels  = int( m_pVirtualOutput->GetPhysicalOutputChannels( ).size( ) );
	int iBlockLength  = oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.iBuffersize;
	m_pdsStreamFilter = new StreamFilter( iNumChannels, dSampleRate, iBlockLength, m_iHRIRFilterLength );

	m_viLastHRIRIndex.resize( iNumChannels );

	return;
}

CVABinauralMixdownReproduction::~CVABinauralMixdownReproduction( )
{
	delete m_pdsStreamFilter;
	m_pdsStreamFilter = NULL;
}

void CVABinauralMixdownReproduction::SetInputDatasource( ITADatasource* p )
{
	m_pdsStreamFilter->pdsInput = p;
}

ITADatasource* CVABinauralMixdownReproduction::GetOutputDatasource( )
{
	return m_pdsStreamFilter;
}

std::vector<const CVAHardwareOutput*> CVABinauralMixdownReproduction::GetTargetOutputs( ) const
{
	return m_vpTargetOutputs;
}

void CVABinauralMixdownReproduction::SetTrackedListener( const int iID )
{
	m_iListenerID = iID;
}

int CVABinauralMixdownReproduction::GetNumVirtualLoudspeaker( ) const
{
	size_t iSize = m_pVirtualOutput->vpDevices.size( );
	return int( iSize );
}

void CVABinauralMixdownReproduction::UpdateScene( CVASceneState* pNewState )
{
	if( m_iListenerID == -1 )
		return;

	CVAReceiverState* pLIstenerState( pNewState->GetReceiverState( m_iListenerID ) );
	if( pLIstenerState == nullptr )
		return;

	const CVADirectivityDAFFHRIR* pHRIRSet = (CVADirectivityDAFFHRIR*)pLIstenerState->GetDirectivity( );

	if( pHRIRSet == nullptr )
		return;

	VAVec3 vListenerPos, vListenerView, vListenerUp;
	m_oParams.pCore->GetSoundReceiverRealWorldPositionOrientationVU( m_iListenerID, vListenerPos, vListenerView, vListenerUp );

	for( int i = 0; i < GetNumVirtualLoudspeaker( ); i++ )
	{
		int iIndexLeft  = 2 * i + 0;
		int iIndexRight = 2 * i + 1;

		const CVAHardwareDevice* pDevice( m_pVirtualOutput->vpDevices[i] );

		double dAzimuth   = GetAzimuthOnTarget_DEG( vListenerPos, vListenerView, vListenerUp, pDevice->vPos );
		double dElevation = GetElevationOnTarget_DEG( vListenerPos, vListenerUp, pDevice->vPos );

		double dDistance = ( pDevice->vPos - vListenerPos ).Length( );

		// Re-init temp buffer to match with current HRIR length
		int iHRIRFilterLength = pHRIRSet->GetProperties( )->iFilterLength;
		if( m_sfHRIRTemp.length( ) != iHRIRFilterLength )
		{
			m_sfHRIRTemp.init( 2, iHRIRFilterLength, false );
		}
		int iIndex;
		pHRIRSet->GetNearestNeighbour( float( dAzimuth ), float( dElevation ), &iIndex );

		// Skip if still same HRIR
		if( iIndex == m_viLastHRIRIndex[i] )
			continue;

		pHRIRSet->GetHRIRByIndex( &m_sfHRIRTemp, iIndex, 0 );
		m_viLastHRIRIndex[i] = iIndex;

		ITAUPFilter* pFilterChL = m_pdsStreamFilter->pFilterPool->RequestFilter( );
		ITAUPFilter* pFilterChR = m_pdsStreamFilter->pFilterPool->RequestFilter( );

		int iLoadFilter = (std::min)( m_iHRIRFilterLength, iHRIRFilterLength );
		pFilterChL->Zeros( );
		pFilterChL->Load( m_sfHRIRTemp[0].data( ), iLoadFilter );
		pFilterChR->Zeros( );
		pFilterChR->Load( m_sfHRIRTemp[1].data( ), iLoadFilter );

		// Update
		m_pdsStreamFilter->vpConvolver[iIndexLeft]->ExchangeFilter( pFilterChL );
		m_pdsStreamFilter->vpConvolver[iIndexRight]->ExchangeFilter( pFilterChR );

		pFilterChL->Release( );
		pFilterChR->Release( );
	}

	m_pdsStreamFilter->pTrigger->trigger( );

	return;
}

int CVABinauralMixdownReproduction::GetNumInputChannels( ) const
{
	return m_pdsStreamFilter->GetNumberOfChannels( );
}

// --= Stream filter =--

StreamFilter::StreamFilter( int iNumChannels, double dSampleRate, int iBlockLength, int iMaxFilterLength )
    : ITADatasourceRealization( 2, dSampleRate, iBlockLength )
    , pdsInput( NULL )
    , pFilterPool( NULL )
    , pTrigger( NULL )
{
	pFilterPool = new ITAUPFilterPool( (int)m_uiBlocklength, iMaxFilterLength, 24 );
	pDirac      = pFilterPool->RequestFilter( );
	pDirac->identity( );

	pTrigger = new ITAUPTrigger;

	for( int i = 0; i < 2 * iNumChannels; i++ )
	{
		ITAUPConvolution* pConvolver = new ITAUPConvolution( (int)GetBlocklength( ), iMaxFilterLength, pFilterPool );
		pConvolver->SetFilterExchangeTrigger( pTrigger );
		pConvolver->SetFilterExchangeFadingFunction( ITABase::FadingFunction::COSINE_SQUARE );
		pConvolver->SetFilterCrossfadeLength( (int)GetBlocklength( ) );
		pConvolver->ExchangeFilter( pDirac );

		vpConvolver.push_back( pConvolver );
	}

	pTrigger->trigger( );
}

StreamFilter::~StreamFilter( )
{
	for( unsigned int i = 0; i < 2 * GetNumberOfChannels( ); i++ )
		delete vpConvolver[i];

	delete pFilterPool;
	delete pTrigger;
}

void StreamFilter::ProcessStream( const ITAStreamInfo* pStreamInfo )
{
	float* pfBinauralOutputDataChL = GetWritePointer( 0 );
	float* pfBinauralOutputDataChR = GetWritePointer( 1 );

	for( unsigned int i = 0; i < pdsInput->GetNumberOfChannels( ); i++ )
	{
		int iIndexLeft           = 2 * i + 0;
		int iIndexRight          = 2 * i + 1;
		const float* pfInputData = pdsInput->GetBlockPointer( i, pStreamInfo );
		int iWriteMode           = ( i == 0 ) ? ITABase::MixingMethod::OVERWRITE : ITABase::MixingMethod::ADD;
		vpConvolver[iIndexLeft]->Process( pfInputData, pfBinauralOutputDataChL, iWriteMode );
		vpConvolver[iIndexRight]->Process( pfInputData, pfBinauralOutputDataChR, iWriteMode );
	}

	IncrementWritePointer( );
}

void StreamFilter::PostIncrementBlockPointer( )
{
	pdsInput->IncrementBlockPointer( );
}

#endif // VACORE_WITH_REPRODUCTION_BINAURAL_MIXDOWN