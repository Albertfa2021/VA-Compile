#include "VAATNSourceReceiverTransmission.h"


CVABATNSourceReceiverTransmission::CVABATNSourceReceiverTransmission( double dSamplerate, int iBlocklength, int iHRIRFilterLength, int iDirFilterLength,
                                                                      int iFilterBankType )
    : dGroundReflectionPlanePosition( 0 )
{
	oDirectSoundPath.pThirdOctaveFilterBank    = CITAThirdOctaveFilterbank::Create( dSamplerate, iBlocklength, iFilterBankType );
	oReflectedSoundPath.pThirdOctaveFilterBank = CITAThirdOctaveFilterbank::Create( dSamplerate, iBlocklength, iFilterBankType );

	oDirectSoundPath.pThirdOctaveFilterBank->SetIdentity( );
	oReflectedSoundPath.pThirdOctaveFilterBank->SetIdentity( );

	float fReserverdMaxDelaySamples        = (float)( 6 * dSamplerate ); // 6 Sekunden ~ 2km Entfernung
	m_iDefaultVDLSwitchingAlgorithm        = EVDLAlgorithm::CUBIC_SPLINE_INTERPOLATION;
	oDirectSoundPath.pVariableDelayLine    = new CITAVariableDelayLine( dSamplerate, iBlocklength, fReserverdMaxDelaySamples, m_iDefaultVDLSwitchingAlgorithm );
	oReflectedSoundPath.pVariableDelayLine = new CITAVariableDelayLine( dSamplerate, iBlocklength, fReserverdMaxDelaySamples, m_iDefaultVDLSwitchingAlgorithm );

	oDirectSoundPath.pFIRConvolverChL    = new ITAUPConvolution( iBlocklength, iHRIRFilterLength );
	oReflectedSoundPath.pFIRConvolverChL = new ITAUPConvolution( iBlocklength, iHRIRFilterLength );
	oDirectSoundPath.pFIRConvolverChR    = new ITAUPConvolution( iBlocklength, iHRIRFilterLength );
	oReflectedSoundPath.pFIRConvolverChR = new ITAUPConvolution( iBlocklength, iHRIRFilterLength );
	oDirectSoundPath.pFIRConvolverChL->SetFilterExchangeFadingFunction( ITABase::FadingFunction::COSINE_SQUARE );
	oReflectedSoundPath.pFIRConvolverChL->SetFilterExchangeFadingFunction( ITABase::FadingFunction::COSINE_SQUARE );
	oDirectSoundPath.pFIRConvolverChR->SetFilterExchangeFadingFunction( ITABase::FadingFunction::COSINE_SQUARE );
	oReflectedSoundPath.pFIRConvolverChR->SetFilterExchangeFadingFunction( ITABase::FadingFunction::COSINE_SQUARE );
	oDirectSoundPath.pFIRConvolverChL->SetFilterCrossfadeLength( (std::min)( iBlocklength, 32 ) );
	oReflectedSoundPath.pFIRConvolverChL->SetFilterCrossfadeLength( (std::min)( iBlocklength, 32 ) );
	oDirectSoundPath.pFIRConvolverChR->SetFilterCrossfadeLength( (std::min)( iBlocklength, 32 ) );
	oReflectedSoundPath.pFIRConvolverChR->SetFilterCrossfadeLength( (std::min)( iBlocklength, 32 ) );
	oDirectSoundPath.pFIRConvolverChL->SetGain( 0.0f, true );
	oReflectedSoundPath.pFIRConvolverChL->SetGain( 0.0f, true );
	oDirectSoundPath.pFIRConvolverChR->SetGain( 0.0f, true );
	oReflectedSoundPath.pFIRConvolverChR->SetGain( 0.0f, true );
	ITAUPFilter* pDHRIRFilterChL = oDirectSoundPath.pFIRConvolverChL->RequestFilter( );
	ITAUPFilter* pRHRIRFilterChL = oReflectedSoundPath.pFIRConvolverChL->RequestFilter( );
	ITAUPFilter* pDHRIRFilterChR = oDirectSoundPath.pFIRConvolverChR->RequestFilter( );
	ITAUPFilter* pRHRIRFilterChR = oReflectedSoundPath.pFIRConvolverChR->RequestFilter( );
	pDHRIRFilterChL->identity( );
	pRHRIRFilterChL->identity( );
	pDHRIRFilterChR->identity( );
	pRHRIRFilterChR->identity( );
	oDirectSoundPath.pFIRConvolverChL->ExchangeFilter( pDHRIRFilterChL );
	oReflectedSoundPath.pFIRConvolverChL->ExchangeFilter( pRHRIRFilterChL );
	oDirectSoundPath.pFIRConvolverChR->ExchangeFilter( pDHRIRFilterChR );
	oReflectedSoundPath.pFIRConvolverChR->ExchangeFilter( pRHRIRFilterChR );

	// Auto-release filter after it is not used anymore
	oDirectSoundPath.pFIRConvolverChL->ReleaseFilter( pDHRIRFilterChL );
	oReflectedSoundPath.pFIRConvolverChL->ReleaseFilter( pRHRIRFilterChL );
	oDirectSoundPath.pFIRConvolverChR->ReleaseFilter( pDHRIRFilterChR );
	oReflectedSoundPath.pFIRConvolverChR->ReleaseFilter( pRHRIRFilterChR );

	m_sfHRIRTemp.init( 2, iHRIRFilterLength, false );
}

CVABATNSourceReceiverTransmission::~CVABATNSourceReceiverTransmission( )
{
	delete oDirectSoundPath.pThirdOctaveFilterBank;
	delete oReflectedSoundPath.pThirdOctaveFilterBank;
	delete oDirectSoundPath.pVariableDelayLine;
	delete oReflectedSoundPath.pVariableDelayLine;
	delete oDirectSoundPath.pFIRConvolverChL;
	delete oReflectedSoundPath.pFIRConvolverChL;
	delete oDirectSoundPath.pFIRConvolverChR;
	delete oReflectedSoundPath.pFIRConvolverChR;
}

void CVABATNSourceReceiverTransmission::UpdateSoundSourceDirectivity( )
{
	// Direct sound path
	CVADirectivityDAFFEnergetic* pDirectivityDataNew = (CVADirectivityDAFFEnergetic*)oDirectSoundPath.oDirectivityStateNew.pData;

	oDirectSoundPath.oDirectivityStateNew.bDirectivityEnabled = true;

	if( pDirectivityDataNew == nullptr || !pDirectivityDataNew->GetDAFFContent( ) )
	{
		// Directivity not yet set ... or removed
		oDirectSoundPath.oSoundSourceDirectivityMagnitudes.SetIdentity( );
		oDirectSoundPath.oDirectivityStateNew.iRecord = -1;
	}
	else
	{
		// Update directivity data set
		pDirectivityDataNew->GetDAFFContent( )->getNearestNeighbour( DAFF_OBJECT_VIEW, float( oDirectSoundPath.oRelations.dAzimuthS2T ),
		                                                             float( oDirectSoundPath.oRelations.dElevationS2T ), oDirectSoundPath.oDirectivityStateNew.iRecord );
		if( oDirectSoundPath.oDirectivityStateCur.iRecord != oDirectSoundPath.oDirectivityStateNew.iRecord )
		{
			std::vector<float> vfGains( ITABase::CThirdOctaveMagnitudeSpectrum::GetNumBands( ) );
			pDirectivityDataNew->GetDAFFContent( )->getMagnitudes( oDirectSoundPath.oDirectivityStateNew.iRecord, 0, &vfGains[0] );

			oDirectSoundPath.oSoundSourceDirectivityMagnitudes.SetMagnitudes( vfGains );
		}
	}

	// Acknowledge new state
	oDirectSoundPath.oDirectivityStateCur = oDirectSoundPath.oDirectivityStateNew;


	// Reflected sound path
	pDirectivityDataNew = (CVADirectivityDAFFEnergetic*)oReflectedSoundPath.oDirectivityStateNew.pData;

	oReflectedSoundPath.oDirectivityStateNew.bDirectivityEnabled = true;

	if( pDirectivityDataNew == nullptr )
	{
		// Directivity not yet set ... or removed
		oReflectedSoundPath.oSoundSourceDirectivityMagnitudes.SetIdentity( ); // set identity once
		oReflectedSoundPath.oDirectivityStateNew.iRecord = -1;
	}
	else
	{
		// Update directivity data set
		pDirectivityDataNew->GetDAFFContent( )->getNearestNeighbour( DAFF_OBJECT_VIEW, float( oReflectedSoundPath.oRelations.dAzimuthS2T ),
		                                                             float( oReflectedSoundPath.oRelations.dElevationS2T ),
		                                                             oReflectedSoundPath.oDirectivityStateNew.iRecord );
		if( oReflectedSoundPath.oDirectivityStateCur.iRecord != oReflectedSoundPath.oDirectivityStateNew.iRecord )
		{
			std::vector<float> vfGains( ITABase::CThirdOctaveMagnitudeSpectrum::GetNumBands( ) );
			pDirectivityDataNew->GetDAFFContent( )->getMagnitudes( oReflectedSoundPath.oDirectivityStateNew.iRecord, 0, &vfGains[0] );

			oReflectedSoundPath.oSoundSourceDirectivityMagnitudes.SetMagnitudes( vfGains );
		}
	}

	// Acknowledge new state
	oReflectedSoundPath.oDirectivityStateCur = oReflectedSoundPath.oDirectivityStateNew;

	return;
}

void CVABATNSourceReceiverTransmission::UpdateTemporalVariation( )
{
	oDirectSoundPath.oATVGenerator.GetMagnitudes( oDirectSoundPath.oTemporalVariationMagnitudes );
	oReflectedSoundPath.oATVGenerator.GetMagnitudes( oReflectedSoundPath.oTemporalVariationMagnitudes );

	return;
}

void CVABATNSourceReceiverTransmission::UpdateSoundReceiverDirectivity( )
{
	CVADirectivityDAFFHRIR* pDirSoundPathHRIRDataNew = (CVADirectivityDAFFHRIR*)( oDirectSoundPath.oSoundReceiverDirectivityStateNew.pData );
	bool bForeUpdate                                 = false;
	if( pDirSoundPathHRIRDataNew != nullptr )
	{
		// Quick check if nearest neighbour is equal
		if( pDirSoundPathHRIRDataNew->GetProperties( )->bSpaceDiscrete )
			pDirSoundPathHRIRDataNew->GetNearestNeighbour( float( oDirectSoundPath.oRelations.dAzimuthT2S ), float( oDirectSoundPath.oRelations.dElevationT2S ),
			                                               &oDirectSoundPath.oSoundReceiverDirectivityStateNew.iRecord );
		else
			bForeUpdate = true; // Update always required, if non-discrete data
	}

	if( ( oDirectSoundPath.oSoundReceiverDirectivityStateCur != oDirectSoundPath.oSoundReceiverDirectivityStateNew ) || bForeUpdate )
	{
		ITAUPFilter* pHRIRFilterChL = oDirectSoundPath.pFIRConvolverChL->GetFilterPool( )->RequestFilter( );
		ITAUPFilter* pHRIRFilterChR = oDirectSoundPath.pFIRConvolverChR->GetFilterPool( )->RequestFilter( );

		if( pDirSoundPathHRIRDataNew == nullptr )
		{
			pHRIRFilterChL->identity( );
			pHRIRFilterChR->identity( );
		}
		else
		{
			int iNewFilterLength = pDirSoundPathHRIRDataNew->GetProperties( )->iFilterLength;
			if( m_sfHRIRTemp.length( ) != iNewFilterLength )
			{
				m_sfHRIRTemp.init( 2, iNewFilterLength, false );
			}

			if( iNewFilterLength > oDirectSoundPath.pFIRConvolverChL->GetMaxFilterlength( ) )
			{
				VA_WARN( "BATNSoundPath", "HRIR too long for convolver, cropping. Increase HRIR filter length in BinauralFreefieldAudioRenderer configuration." );
				iNewFilterLength = oDirectSoundPath.pFIRConvolverChL->GetMaxFilterlength( );
			}

			if( pDirSoundPathHRIRDataNew->GetProperties( )->bSpaceDiscrete )
			{
				pDirSoundPathHRIRDataNew->GetHRIRByIndex( &m_sfHRIRTemp, oDirectSoundPath.oSoundReceiverDirectivityStateNew.iRecord,
				                                          float( oDirectSoundPath.oRelations.dDistance ) );
				oDirectSoundPath.oSoundReceiverDirectivityStateNew.fDistance = float( oDirectSoundPath.oRelations.dDistance );
			}
			else
			{
				pDirSoundPathHRIRDataNew->GetHRIR( &m_sfHRIRTemp, float( oDirectSoundPath.oRelations.dAzimuthT2S ), float( oDirectSoundPath.oRelations.dElevationT2S ),
				                                   float( oDirectSoundPath.oRelations.dDistance ) );
			}

			pHRIRFilterChL->Load( m_sfHRIRTemp[0].data( ), iNewFilterLength );
			pHRIRFilterChR->Load( m_sfHRIRTemp[1].data( ), iNewFilterLength );
		}

		oDirectSoundPath.pFIRConvolverChL->ExchangeFilter( pHRIRFilterChL );
		oDirectSoundPath.pFIRConvolverChR->ExchangeFilter( pHRIRFilterChR );
		oDirectSoundPath.pFIRConvolverChL->ReleaseFilter( pHRIRFilterChL );
		oDirectSoundPath.pFIRConvolverChR->ReleaseFilter( pHRIRFilterChR );

		// Ack
		oDirectSoundPath.oSoundReceiverDirectivityStateCur = oDirectSoundPath.oSoundReceiverDirectivityStateNew;
	}

	// Reflected sound path
	CVADirectivityDAFFHRIR* pRefSoundPathHRIRDataNew = (CVADirectivityDAFFHRIR*)( oReflectedSoundPath.oSoundReceiverDirectivityStateNew.pData );

	bForeUpdate = false;
	if( pRefSoundPathHRIRDataNew != nullptr )
	{
		// Quick check if nearest neighbour is equal
		if( pRefSoundPathHRIRDataNew->GetProperties( )->bSpaceDiscrete )
			pRefSoundPathHRIRDataNew->GetNearestNeighbour( float( oReflectedSoundPath.oRelations.dAzimuthT2S ), float( oReflectedSoundPath.oRelations.dElevationT2S ),
			                                               &oReflectedSoundPath.oSoundReceiverDirectivityStateNew.iRecord );
		else
			bForeUpdate = true; // Update always required, if non-discrete data
	}

	if( ( oReflectedSoundPath.oSoundReceiverDirectivityStateCur != oReflectedSoundPath.oSoundReceiverDirectivityStateNew ) || bForeUpdate )
	{
		ITAUPFilter* pHRIRFilterChL = oReflectedSoundPath.pFIRConvolverChL->GetFilterPool( )->RequestFilter( );
		ITAUPFilter* pHRIRFilterChR = oReflectedSoundPath.pFIRConvolverChR->GetFilterPool( )->RequestFilter( );

		if( pRefSoundPathHRIRDataNew == nullptr )
		{
			pHRIRFilterChL->identity( );
			pHRIRFilterChR->identity( );
		}
		else
		{
			int iNewFilterLength = pRefSoundPathHRIRDataNew->GetProperties( )->iFilterLength;
			if( m_sfHRIRTemp.length( ) != iNewFilterLength )
			{
				m_sfHRIRTemp.init( 2, iNewFilterLength, false );
			}

			if( iNewFilterLength > oReflectedSoundPath.pFIRConvolverChL->GetMaxFilterlength( ) )
			{
				VA_WARN( "BATNSoundPath", "HRIR too long for convolver, cropping. Increase HRIR filter length in BinauralFreefieldAudioRenderer configuration." );
				iNewFilterLength = oReflectedSoundPath.pFIRConvolverChL->GetMaxFilterlength( );
			}

			if( pRefSoundPathHRIRDataNew->GetProperties( )->bSpaceDiscrete )
			{
				pRefSoundPathHRIRDataNew->GetHRIRByIndex( &m_sfHRIRTemp, oReflectedSoundPath.oSoundReceiverDirectivityStateNew.iRecord,
				                                          float( oReflectedSoundPath.oRelations.dDistance ) );
				oReflectedSoundPath.oSoundReceiverDirectivityStateNew.fDistance = float( oReflectedSoundPath.oRelations.dDistance );
			}
			else
			{
				pRefSoundPathHRIRDataNew->GetHRIR( &m_sfHRIRTemp, float( oReflectedSoundPath.oRelations.dAzimuthT2S ),
				                                   float( oReflectedSoundPath.oRelations.dElevationT2S ), float( oReflectedSoundPath.oRelations.dDistance ) );
			}

			pHRIRFilterChL->Load( m_sfHRIRTemp[0].data( ), iNewFilterLength );
			pHRIRFilterChR->Load( m_sfHRIRTemp[1].data( ), iNewFilterLength );
		}

		oReflectedSoundPath.pFIRConvolverChL->ExchangeFilter( pHRIRFilterChL );
		oReflectedSoundPath.pFIRConvolverChR->ExchangeFilter( pHRIRFilterChR );
		oReflectedSoundPath.pFIRConvolverChL->ReleaseFilter( pHRIRFilterChL );
		oReflectedSoundPath.pFIRConvolverChR->ReleaseFilter( pHRIRFilterChR );

		// Ack
		oReflectedSoundPath.oSoundReceiverDirectivityStateCur = oReflectedSoundPath.oSoundReceiverDirectivityStateNew;
	}
}
