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

#include "VABinauralEncoding.h"

#include "../Scene/VAMotionState.h"
#include "../Scene/VASoundReceiverState.h"
#include "../VALog.h"
#include "../directivities/VADirectivityDAFFHATOHRIR.h"
#include "../directivities/VADirectivityDAFFHRIR.h"
#include "VAException.h"

#include <ITANumericUtils.h>
#include <ITASampleBuffer.h>
#include <ITASampleFrame.h>
#include <ITAUPConvolution.h>
#include <ITAUPFilter.h>
#include <ITAUPFilterPool.h>
#include <VistaBase/VistaQuaternion.h>
#include <string>


CVABinauralEncoding::CVABinauralEncoding( const IVASpatialEncoding::Config& oConf )
{
	const int iNumChannels     = 2;

	m_pFIRConvolverChL = std::make_unique<ITAUPConvolution>( oConf.iBlockSize, oConf.iHRIRFilterLength );
	m_pFIRConvolverChL->SetFilterExchangeFadingFunction( ITABase::FadingFunction::COSINE_SQUARE );
	m_pFIRConvolverChL->SetFilterCrossfadeLength( (std::min)( oConf.iBlockSize, 32 ) );

	m_pFIRConvolverChR = std::make_unique<ITAUPConvolution>( oConf.iBlockSize, oConf.iHRIRFilterLength );
	m_pFIRConvolverChR->SetFilterExchangeFadingFunction( ITABase::FadingFunction::COSINE_SQUARE );
	m_pFIRConvolverChR->SetFilterCrossfadeLength( (std::min)( oConf.iBlockSize, 32 ) );

	m_sfHRIRTemp.init( iNumChannels, oConf.iHRIRFilterLength, false );

	Reset( );
}

void CVABinauralEncoding::Reset( )
{
	//Clear internal convolution buffers and then set default initial values
	m_pFIRConvolverChL->clear( );
	m_pFIRConvolverChR->clear( );

	m_pFIRConvolverChL->SetGain( 0.0f, true );
	ITAUPFilter* pHRIRFilterChL = m_pFIRConvolverChL->RequestFilter( );
	pHRIRFilterChL->identity( );
	m_pFIRConvolverChL->ExchangeFilter( pHRIRFilterChL );
	m_pFIRConvolverChL->ReleaseFilter( pHRIRFilterChL );

	m_pFIRConvolverChR->SetGain( 0.0f, true );
	ITAUPFilter* pHRIRFilterChR = m_pFIRConvolverChR->RequestFilter( );
	pHRIRFilterChR->identity( );
	m_pFIRConvolverChR->ExchangeFilter( pHRIRFilterChR );
	m_pFIRConvolverChR->ReleaseFilter( pHRIRFilterChR );
}

void CVABinauralEncoding::Process( const ITASampleBuffer& sbInputData, ITASampleFrame& sfOutput, float fGain, double dAzimuthDeg, double dElevationDeg,
                                   const CVAReceiverState& pReceiverState )
{
	if( sfOutput.GetNumChannels( ) < 2 )
	{
		VA_EXCEPT2( INVALID_PARAMETER, "Binaural encoding requries an output stream with two channels." );
	}
	else if( sfOutput.GetNumChannels( ) > 2 )
	{
		VA_WARN( "BinauralEncoding", "Output stream has more than two channels. Additional channels will be skipped." );
	}

	UpdateHRIR( pReceiverState, (float)dAzimuthDeg, (float)dElevationDeg );

	m_pFIRConvolverChL->SetGain( fGain );
	m_pFIRConvolverChR->SetGain( fGain );

	m_pFIRConvolverChL->Process( sbInputData.data( ), sfOutput[0].data( ), ITABase::MixingMethod::ADD );
	m_pFIRConvolverChR->Process( sbInputData.data( ), sfOutput[1].data( ), ITABase::MixingMethod::ADD );
}


void CVABinauralEncoding::UpdateHRIR( const CVAReceiverState& pReceiverState, float fAzimuthDeg, float fElevationDeg )
{
	// Retrieve new HRIR state
	CHRIRState oNewHRIRState;
	oNewHRIRState.pHRIRData                     = pReceiverState.GetDirectivity( );
	const CVADirectivityDAFFHRIR* pHRIR         = dynamic_cast<const CVADirectivityDAFFHRIR*>( oNewHRIRState.pHRIRData );
	const CVADirectivityDAFFHATOHRIR* pHatoHRIR = dynamic_cast<const CVADirectivityDAFFHATOHRIR*>( oNewHRIRState.pHRIRData );

	int iNewFilterLength = -1;
	if( pHRIR )
	{
		oNewHRIRState.bSpatiallyDiscrete = pHRIR->GetProperties( )->bSpaceDiscrete;
		pHRIR->GetNearestNeighbour( fAzimuthDeg, fElevationDeg, &oNewHRIRState.iRecord );

		iNewFilterLength = pHRIR->GetProperties( )->iFilterLength;
	}
	else if( pHatoHRIR )
	{
		oNewHRIRState.bSpatiallyDiscrete = pHatoHRIR->GetProperties( )->bSpaceDiscrete;
		pHatoHRIR->GetNearestSpatialNeighbourIndex( fAzimuthDeg, fElevationDeg, &oNewHRIRState.iRecord );
		const VistaQuaternion qHATO( pReceiverState.GetMotionState( )->GetHeadAboveTorsoOrientation( ).comp );
		const double dHATODeg  = rad2grad( qHATO.GetAngles( ).b );
		oNewHRIRState.dHATODeg = dHATODeg;

		iNewFilterLength = pHatoHRIR->GetProperties( )->iFilterLength;
	}

	// Only update filter if required
	if( m_oCurrentHRIRState == oNewHRIRState )
		return;


	// Check buffer size and filter length
	if( m_sfHRIRTemp.length( ) != iNewFilterLength )
	{
		m_sfHRIRTemp.init( 2, iNewFilterLength, false );
	}
	if( iNewFilterLength > m_pFIRConvolverChL->GetMaxFilterlength( ) )
	{
		VA_WARN( "BinauralEncoding", "HRIR too long for convolver, cropping. Maximum filter length " + std::to_string( m_pFIRConvolverChL->GetMaxFilterlength( ) ) +
		                                 ", HRIR length " + std::to_string( iNewFilterLength ) +
		                                 ". Increase HRIR filter length in Renderer configuration." );
		iNewFilterLength = m_pFIRConvolverChL->GetMaxFilterlength( );
	}


	// Retrieve correct HRIR filter from data set
	if( pHRIR )
	{
		const float fDistance = 1.0f; // Obsolete parameter: Function expects a float value which is not used
		pHRIR->GetHRIRByIndex( &m_sfHRIRTemp, oNewHRIRState.iRecord, fDistance );
	}
	else if( pHatoHRIR )
		pHatoHRIR->GetHRIRByIndexAndHATO( &m_sfHRIRTemp, oNewHRIRState.iRecord, oNewHRIRState.dHATODeg );


	// Actual filter exchange
	ITAUPFilter* pHRIRFilterChL = m_pFIRConvolverChL->GetFilterPool( )->RequestFilter( );
	ITAUPFilter* pHRIRFilterChR = m_pFIRConvolverChR->GetFilterPool( )->RequestFilter( );
	if( oNewHRIRState.pHRIRData == nullptr ) // Omni-directional directivity if no HRIR is specified
	{
		pHRIRFilterChL->identity( );
		pHRIRFilterChR->identity( );
	}
	else
	{
		pHRIRFilterChL->Load( m_sfHRIRTemp[0].data( ), iNewFilterLength );
		pHRIRFilterChR->Load( m_sfHRIRTemp[1].data( ), iNewFilterLength );
	}
	m_pFIRConvolverChL->ExchangeFilter( pHRIRFilterChL );
	m_pFIRConvolverChR->ExchangeFilter( pHRIRFilterChR );
	m_pFIRConvolverChL->ReleaseFilter( pHRIRFilterChL );
	m_pFIRConvolverChR->ReleaseFilter( pHRIRFilterChR );


	// Update HRIR state
	m_oCurrentHRIRState = oNewHRIRState;
}
