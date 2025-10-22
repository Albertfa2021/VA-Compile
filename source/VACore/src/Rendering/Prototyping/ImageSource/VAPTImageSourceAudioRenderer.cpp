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

#include "VAPTImageSourceAudioRenderer.h"

#ifdef VACORE_WITH_RENDERER_PROTOTYPE_IMAGE_SOURCE

// VA includes
#	include "../../../Scene/VAScene.h"
#	include "../../../Utils/VAUtils.h"
#	include "../../../VACoreConfig.h"
#	include "../../../VALog.h"
#	include "../../../core/core.h"
#	include "../../../directivities/VADirectivityDAFFHRIR.h"

#	include <VAReferenceableObject.h>

// ITA includes
#	include <ITAAtomicPrimitives.h>
#	include <ITAConstants.h>
#	include <ITADataSourceRealization.h>
#	include <ITAUPConvolution.h>
#	include <ITAUPFilter.h>
#	include <ITAUPFilterPool.h>
#	include <ITAVariableDelayLine.h>


// Vista includes
#	include <VistaInterProcComm/Concurrency/VistaThreadEvent.h>

// 3rdParty includes
#	include <tbb/concurrent_queue.h>

// STL includes
#	include <atomic>
#	include <cassert>
#	include <cmath>
#	include <vector>

//! Generic sound path
class CVAPTImageSourceSoundPath : public CVAPoolObject
{
public:
	virtual ~CVAPTImageSourceSoundPath( );
	CVAPTImageSourceAudioRenderer::CVAPTGPSource* pSource;
	CVAPTImageSourceAudioRenderer::CVAPTGPListener* pListener;
	ITAAtomicBool bDelete;

	CITAVariableDelayLine* pVariableDelayLine;
	std::vector<ITAUPConvolution*> vpFIRConvolver; // N-channel convolver

	inline void PreRequest( )
	{
		pSource   = nullptr;
		pListener = nullptr;

		pVariableDelayLine->Clear( );

		for( size_t n = 0; n < vpFIRConvolver.size( ); n++ )
			vpFIRConvolver[n]->clear( );
	};

private:
	CVAPTImageSourceSoundPath( );
	CVAPTImageSourceSoundPath( double dSamplerate, int iBlocklength, int iNumChannels, int iIRFilterLength );

	friend class CVAPTImageSourceSoundPathFactory;
};

class CVAPTImageSourceSoundPathFactory : public IVAPoolObjectFactory
{
public:
	inline CVAPTImageSourceSoundPathFactory( double dSampleRate, int iBlockLength, int iNumChannels, int iIRFilterLength )
	    : m_dSampleRate( dSampleRate )
	    , m_iBlockLength( iBlockLength )
	    , m_iNumChannels( iNumChannels )
	    , m_iIRFilterLengthSamples( iIRFilterLength ) { };

	inline CVAPoolObject* CreatePoolObject( ) { return new CVAPTImageSourceSoundPath( m_dSampleRate, m_iBlockLength, m_iNumChannels, m_iIRFilterLengthSamples ); };

private:
	double m_dSampleRate;
	int m_iBlockLength;
	int m_iIRFilterLengthSamples;
	int m_iNumChannels;
};

class CVAPTISListenerPoolFactory : public IVAPoolObjectFactory
{
public:
	inline CVAPTISListenerPoolFactory( CVACoreImpl* pCore ) : m_pCore( pCore ) { };

	inline CVAPoolObject* CreatePoolObject( )
	{
		CVAPTImageSourceAudioRenderer::CVAPTGPListener* pListener;
		pListener        = new CVAPTImageSourceAudioRenderer::CVAPTGPListener( );
		pListener->pCore = m_pCore;
		return pListener;
	};

private:
	CVACoreImpl* m_pCore;
};

class CVAPTISSourcePoolFactory : public IVAPoolObjectFactory
{
public:
	inline CVAPTISSourcePoolFactory( ) { };

	inline CVAPoolObject* CreatePoolObject( )
	{
		CVAPTImageSourceAudioRenderer::CVAPTGPSource* pSource;
		pSource = new CVAPTImageSourceAudioRenderer::CVAPTGPSource( );
		return pSource;
	};
};


// Class CVAPTImageSourceSoundPath

CVAPTImageSourceSoundPath::CVAPTImageSourceSoundPath( double dSamplerate, int iBlocklength, int iNumChannels, int iIRFilterLength )
{
	if( iNumChannels < 1 )
		VA_EXCEPT2( INVALID_PARAMETER, "Number of channels must be positive" );

	const EVDLAlgorithm iAlgorithm = EVDLAlgorithm::CUBIC_SPLINE_INTERPOLATION;
	pVariableDelayLine   = new CITAVariableDelayLine( dSamplerate, iBlocklength, 6 * dSamplerate, iAlgorithm );

	for( int n = 0; n < iNumChannels; n++ )
	{
		ITAUPConvolution* pFIRConvolver = new ITAUPConvolution( iBlocklength, iIRFilterLength );
		pFIRConvolver->SetFilterExchangeFadingFunction( ITABase::FadingFunction::COSINE_SQUARE );
		pFIRConvolver->SetFilterCrossfadeLength( (std::min)( iBlocklength, 32 ) );
		pFIRConvolver->SetGain( 1.0f, true );
		ITAUPFilter* pHRIRFilterChL = pFIRConvolver->RequestFilter( );
		pHRIRFilterChL->Zeros( );
		pFIRConvolver->ExchangeFilter( pHRIRFilterChL );
		pFIRConvolver->ReleaseFilter( pHRIRFilterChL );

		vpFIRConvolver.push_back( pFIRConvolver );
	}
}

CVAPTImageSourceSoundPath::~CVAPTImageSourceSoundPath( )
{
	delete pVariableDelayLine;

	for( size_t n = 0; n < vpFIRConvolver.size( ); n++ )
		delete vpFIRConvolver[n];
}


// Renderer

CVAPTImageSourceAudioRenderer::CVAPTImageSourceAudioRenderer( const CVAAudioRendererInitParams& oParams )
    : m_pCore( oParams.pCore )
    , m_pCurSceneState( nullptr )
    , m_iIRFilterLengthSamples( -1 )
    , m_iNumChannels( -1 )
    , m_oParams( oParams )
{
	// read config
	Init( *oParams.pConfig );

	m_pOutput = new ITADatasourceRealization( m_iNumChannels, oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.dSampleRate,
	                                          oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.iBuffersize );
	m_pOutput->SetStreamEventHandler( this );

	IVAPoolObjectFactory* pListenerFactory = new CVAPTISListenerPoolFactory( m_pCore );
	m_pListenerPool                        = IVAObjectPool::Create( 16, 2, pListenerFactory, true );

	IVAPoolObjectFactory* pSourceFactory = new CVAPTISSourcePoolFactory( );
	m_pSourcePool                        = IVAObjectPool::Create( 16, 2, pSourceFactory, true );

	m_pSoundPathFactory = new CVAPTImageSourceSoundPathFactory( m_pOutput->GetSampleRate( ), m_pOutput->GetBlocklength( ), m_iNumChannels, m_iIRFilterLengthSamples );

	m_pSoundPathPool = IVAObjectPool::Create( 64, 8, m_pSoundPathFactory, true );

	m_pUpdateMessagePool = IVAObjectPool::Create( 2, 1, new CVAPoolObjectDefaultFactory<CVAPTGPUpdateMessage>, true );

	ctxAudio.m_iResetFlag = 0; // Normal operation mode
	ctxAudio.m_iStatus    = 0; // Stopped

	m_iCurGlobalAuralizationMode = IVAInterface::VA_AURAMODE_DEFAULT;

	m_sfTempBuffer.Init( oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.iBuffersize, true );
}

CVAPTImageSourceAudioRenderer::~CVAPTImageSourceAudioRenderer( )
{
	delete m_pSoundPathPool;
	delete m_pUpdateMessagePool;
}

void CVAPTImageSourceAudioRenderer::Init( const CVAStruct& oArgs )
{
	CVAConfigInterpreter conf( oArgs );
	conf.ReqInteger( "NumChannels", m_iNumChannels );
	conf.ReqNumber( "RoomLength", m_RoomLength );
	conf.ReqNumber( "RoomWidth", m_RoomWidth );
	conf.ReqNumber( "RoomHeight", m_RoomHeight );
	conf.ReqInteger( "MaxOrder", m_MaxOrder );
	conf.ReqBool( "DirectSound", m_direct_sound );

	conf.ReqNumber( "Betax1", m_beta[0] );
	conf.ReqNumber( "Betax2", m_beta[1] );
	conf.ReqNumber( "Betay1", m_beta[2] );
	conf.ReqNumber( "Betay2", m_beta[3] );
	conf.ReqNumber( "Betaz1", m_beta[4] );
	conf.ReqNumber( "Betaz2", m_beta[5] );

	conf.OptInteger( "HRIRFilterLength", m_HRIR_length, 256 );
	conf.OptInteger( "IRFilterLengthSamples", m_iIRFilterLengthSamples_ref, 10000 );

	SetIRFilterLength( );

	conf.OptNumber( "RoomVolume", m_dRoomVolume, 301 );

	if( m_iIRFilterLengthSamples < 0 )
		VA_EXCEPT2( INVALID_PARAMETER, "IR filter size must be positive" );

	conf.OptBool( "OutputMonitoring", m_bOutputMonitoring, false );

	return;
}

void CVAPTImageSourceAudioRenderer::Reset( )
{
	ctxAudio.m_iResetFlag = 1; // Request reset

	if( ctxAudio.m_iStatus == 0 || m_oParams.bOfflineRendering )
	{
		// if no streaming active, reset manually
		// SyncInternalData();
		ResetInternalData( );
	}

	// Wait for last streaming block before internal reset
	while( ctxAudio.m_iResetFlag != 2 )
	{
		VASleep( 100 ); // Wait for acknowledge
	}

	// Iterate over sound pathes and free items
	std::list<CVAPTImageSourceSoundPath*>::iterator it = m_lSoundPaths.begin( );
	while( it != m_lSoundPaths.end( ) )
	{
		CVAPTImageSourceSoundPath* pPath = *it;

		int iNumRefs = pPath->GetNumReferences( );
		assert( iNumRefs == 1 );
		pPath->RemoveReference( );

		++it;
	}
	m_lSoundPaths.clear( );

	// Iterate over sound receiver and free items
	std::map<int, CVAPTGPListener*>::const_iterator lcit = m_mListeners.begin( );
	while( lcit != m_mListeners.end( ) )
	{
		CVAPTGPListener* pListener( lcit->second );
		pListener->pData->RemoveReference( );
		assert( pListener->GetNumReferences( ) == 1 );
		pListener->RemoveReference( );
		lcit++;
	}
	m_mListeners.clear( );

	// Iterate over sources and free items
	std::map<int, CVAPTGPSource*>::const_iterator scit = m_mSources.begin( );
	while( scit != m_mSources.end( ) )
	{
		CVAPTGPSource* pSource( scit->second );
		pSource->pData->RemoveReference( );
		assert( pSource->GetNumReferences( ) == 1 );
		pSource->RemoveReference( );
		scit++;
	}
	m_mSources.clear( );

	// Scene frei geben
	if( m_pCurSceneState )
	{
		m_pCurSceneState->RemoveReference( );
		m_pCurSceneState = nullptr;
	}

	ctxAudio.m_iResetFlag = 0; // Enter normal mode
}

void CVAPTImageSourceAudioRenderer::SetIRFilterLength( )
{
	// called whenever the room dimensions or image source max order are changed to set a value for the IR filter length which will not be exceeded
	//-----------------------------------------sort dimensions (bubble sort)--------------------------------------
	float first = m_RoomLength, second = m_RoomWidth, third = m_RoomHeight;
	if( first > second )
		std::swap( first, second );
	if( second > third )
		std::swap( second, third );
	if( first > second )
		std::swap( first, second );

	int sampling_rate     = (int)m_oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.dSampleRate;   // get the sampling rate from the core config
	float c               = m_oParams.pCore->GetHomogeneousMediumSoundSpeed( );                       // get speed of sound from within VA
	float max_distance    = 2 * sqrt( pow( third, 2 ) + pow( second / 2, 2 ) + pow( third / 2, 2 ) ); // max distance for a first order image source ray
	int max_filter_length = ceil( ( m_MaxOrder + 1 ) * max_distance / c * sampling_rate );
	m_iIRFilterLengthSamples =
	    (std::min)( m_iIRFilterLengthSamples_ref,
	                max_filter_length ); // if the max. possible filter length is less than the value given in the config file, set to max filter length

	if( m_iIRFilterLengthSamples_ref != m_iIRFilterLengthSamples ) // if different filter length to user setting is used
		VA_WARN( "PTImageSourceAudioRenderer", "Filter length shortened from user input to maximum needed with given room dimensions" );
}

void CVAPTImageSourceAudioRenderer::UpdateFilter( )
{
	// int iListenerID = 1; //only update filter for a specific listener

	std::list<CVAPTImageSourceSoundPath*>::const_iterator spcit = m_lSoundPaths.begin( ); // loop iteration index over all sound paths
	while( spcit != m_lSoundPaths.end( ) )                                                // iterates over every sound path
	{
		CVAPTImageSourceSoundPath* pPath( *spcit++ ); // path for this iteration -> pPath, and increment iteration variable
		// if (pPath->pListener->pData->iID == iListenerID) { //finds if there are any sound paths between the source and listerner we want
		//-----------------------------Image Source Model Calculations-------------------------------
		// function which uses the IS model to generate impulse response from the new scene, and returns the variable sbIR
		ITASampleBuffer sbIRL( m_iIRFilterLengthSamples ); // create variable where left impulse response will be stored
		ITASampleBuffer sbIRR( m_iIRFilterLengthSamples ); // create variable where right impulse response will be stored

		// pPath->pSource->pData->bInitPositionOrientation.get
		sbIRL.Zero( ); // initialise to 0
		sbIRR.Zero( ); // initialise to 0

		CalculateImageSourceImpulseResponse( sbIRL, sbIRR, pPath ); // calculates IR for this source and receiver ID from IS model and stores in sbIR L/R

		ITAUPConvolution* pConvolverL( pPath->vpFIRConvolver[0] );              // For left channel
		ITAUPFilter* pFilterL = pConvolverL->RequestFilter( );                  // create variable of type which is accepted by ExchangeFilter() function
		pFilterL->Zeros( );                                                     // initialise to 0
		int iLengthL = (std::min)( m_iIRFilterLengthSamples, sbIRL.length( ) ); // Filter length is the minimum of sbIR and m_iIRFilterLengthSamples
		pFilterL->Load( sbIRL.data( ), iLengthL ); // populate pFilter with the filter generated and stored in sbIR (cropped if sbIR > m_iIRFilterLengthSamples)
		pConvolverL->ExchangeFilter( pFilterL );   // send the impulse response (sbIR) to be convolved with the signal
		pFilterL->Release( );                      // tidy up variable

		ITAUPConvolution* pConvolverR( pPath->vpFIRConvolver[1] );              // for right channel
		ITAUPFilter* pFilterR = pConvolverR->RequestFilter( );                  // create variable of type which is accepted by ExchangeFilter() function
		pFilterR->Zeros( );                                                     // initialise to 0
		int iLengthR = (std::min)( m_iIRFilterLengthSamples, sbIRR.length( ) ); // Filter length is the minimum of sbIR and m_iIRFilterLengthSamples
		pFilterR->Load( sbIRR.data( ), iLengthR ); // populate pFilter with the filter generated and stored in sbIR (cropped if sbIR > m_iIRFilterLengthSamples)
		pConvolverR->ExchangeFilter( pFilterR );   // send the impulse response (sbIR) to be convolved with the signal
		pFilterR->Release( );                      // tidy up variable
		//}
	}
}

void CVAPTImageSourceAudioRenderer::UpdateScene( CVASceneState* pNewSceneState )
{
	// This is called automatically from within VA whenever the scene changes
	assert( pNewSceneState );

	m_pNewSceneState = pNewSceneState;
	if( m_pNewSceneState == m_pCurSceneState )
	{ // check if the new scene passed to the function as an input is different from the current scene state
		// if it is the same, return as no changes are needed
		return;
	}

	UpdateFilter( ); // calculate the new impulse response with the IS model, and update the sound paths

	// Neue Szene referenzieren (gegen Freigabe sperren)
	m_pNewSceneState->AddReference( ); // prevent current scene state from being accidentally deleted

	// Unterschiede ermitteln: Neue Szene vs. alte Szene
	CVASceneStateDiff oDiff;
	pNewSceneState->Diff( m_pCurSceneState, &oDiff ); // update the difference property of the new scene state to reflect the last change

	// Leere Update-Nachricht zusammenstellen
	m_pUpdateMessage = dynamic_cast<CVAPTGPUpdateMessage*>( m_pUpdateMessagePool->RequestObject( ) );

	ManageSoundPaths( m_pCurSceneState, pNewSceneState, &oDiff ); // handle transition to new scene

	// Update-Nachricht an den Audiokontext schicken
	ctxAudio.m_qpUpdateMessages.push( m_pUpdateMessage ); // push an update message to the terminal saying that the scene has been updated(?)

	// Alte Szene freigeben (dereferenzieren)
	if( m_pCurSceneState )
		m_pCurSceneState->RemoveReference( ); // free up the old scene state

	m_pCurSceneState = m_pNewSceneState;
	m_pNewSceneState = nullptr;
}


void CVAPTImageSourceAudioRenderer::CalculateImageSourceImpulseResponse( ITASampleBuffer& sbIRL, ITASampleBuffer& sbIRR, CVAPTImageSourceSoundPath* pPath )
{
	// Uses the image source model to calculate the impulse response of a shoebox room which is stored in sbIR
	//---------------------------Input parameters ------------------------------
	int receiver_ID                  = pPath->pListener->pData->iID;
	int source_ID                    = pPath->pSource->pData->iID;
	CVAReceiverState* pListenerState = ( m_pCurSceneState ? m_pCurSceneState->GetReceiverState( receiver_ID ) : NULL );

	VAVec3 source_position = m_oParams.pCore->GetSoundSourcePosition( source_ID );

	VAVec3 receiver_position = m_oParams.pCore->GetSoundReceiverPosition( receiver_ID );

	float c = m_oParams.pCore->GetHomogeneousMediumSoundSpeed( ); // get speed of sound from within VA

	int sampling_rate = (int)m_oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.dSampleRate; // get the sampling rate from the core config

	float pi = ITAConstants::PI_F;

	const CVADirectivityDAFFHRIR* pDAFFDirectivityListener = dynamic_cast<const CVADirectivityDAFFHRIR*>( pListenerState->GetDirectivity( ) );

	//-------------------------------------------------------experimental source directivity---------------------------------------------------------------
	/*
	CVASoundSourceState* pSourceNew = (m_pNewSceneState ? m_pNewSceneState->GetSoundSourceState(source_ID) : nullptr);
	IVADirectivity *test;
	if (pSourceNew != nullptr)
	test = (IVADirectivity*)pSourceNew->GetDirectivityData();
	*/


	//-------------------------------------------------------------------------------
	/*  //calculate the amplitude of the 0th order (direct) ray for normalisation
	VAVec3 	p_zero = source_position - receiver_position; //for 0th order
	double d_zero = p_zero.Length();
	double amplitude_zero = 1.0 / (4.0 * pi*d_zero); //amplitude of the initial signal used for normalisation
	*/
	VAVec3 p_hat, image_source_position, rcvr_is_polar;
	int total_sources = 0;
	int NumSamples    = sbIRL.length( ); // the number of samples in the impulse response
	int order = 0, delay_s = 0, ITD_delay_s;
	double d = 0.0, delay = 0.0, amplitude = 0.0;

	for( int l = -m_MaxOrder; l <= m_MaxOrder; l++ )
	{
		for( int m = -m_MaxOrder; m <= m_MaxOrder; m++ )
		{
			for( int n = -m_MaxOrder; n <= m_MaxOrder; n++ )
			{
				for( int u = 0; u <= 1; u++ )
				{
					for( int v = 0; v <= 1; v++ )
					{
						for( int w = 0; w <= 1; w++ )
						{
							order = abs( l ) + abs( m ) + abs( n ) + abs( l - u ) + abs( m - v ) + abs( n - w );
							if( order > m_MaxOrder ) // check if over max image order
								continue;
							else if( order == 0 && m_direct_sound == false )
							{ // if direct sound is disabled, skip 0th order source
								continue;
							}

							total_sources++;
							p_hat.x = ( 1 - 2 * u ) * source_position.x + 2 * l * m_RoomLength - receiver_position.x;
							p_hat.y = ( 1 - 2 * v ) * source_position.y + 2 * m * m_RoomWidth - receiver_position.y;
							p_hat.z = ( 1 - 2 * w ) * source_position.z + 2 * n * m_RoomHeight - receiver_position.z;

							image_source_position = p_hat + receiver_position;

							//--------calculate delay and add to filter------
							d       = p_hat.Length( );                // distance traveled by ray
							delay   = d / c;                          // work out the delay in s
							delay_s = round( delay * sampling_rate ); // work out the delay in number of samples

							amplitude = ( pow( m_beta[0], abs( l - u ) ) * pow( m_beta[1], abs( l ) ) * pow( m_beta[2], abs( m - v ) ) * pow( m_beta[3], abs( m ) ) *
							              pow( m_beta[4], abs( n - w ) ) * pow( m_beta[5], abs( n ) ) ) /
							            ( 4.0 * pi * d );
							// aplitude = amplitude / amplitude_zero; //normalise such that 0th order ray amplitude = 1


							//======================================================================================================
							/*
							VAVec3 p_hat_dash, source_angle_polar;
							p_hat_dash.x = -p_hat.x * pow(-1, abs(l - u) + abs(l));
							p_hat_dash.y = -p_hat.y * pow(-1, abs(m - v) + abs(m));
							p_hat_dash.z = -p_hat.z * pow(-1, abs(n - w) + abs(n));
							CartVec2PolarVec(p_hat_dash, &source_angle_polar); //finds the angle from the ray leaving the source
							//get attenuation factor for this angle
							//scale amplitude
							*/
							//======================================================================================================


							if( pDAFFDirectivityListener )
							{ // if there is a receiver directivity

								CartVec2PolarVec( image_source_position - receiver_position,
								                  &rcvr_is_polar ); // find polar coords for vector from receiver -> image source

								ITASampleFrame m_sfHRIRTemp( 2, m_HRIR_length,
								                             1 ); // variable where the HRIR data is stored before being copied over to the L/R impulse response
								pDAFFDirectivityListener->GetHRIR( &m_sfHRIRTemp, rcvr_is_polar.x, rcvr_is_polar.y,
								                                   rcvr_is_polar.z ); // get HRIR data based on angles sound is arriving from

								CVAReceiverState::CVAAnthropometricParameter oAnthroParameters = pListenerState->GetAnthropometricData( );
								double daw                                                     = -8.7231e-4;
								double dbw                                                     = 0.0029;
								double dah                                                     = -3.942e-4;
								double dbh                                                     = 6.0476e-4;
								double dad                                                     = 4.2308e-4;
								double test                                                    = pDAFFDirectivityListener->GetProperties( )->oAnthroParams.dHeadWidth;
								double dbd                 = oAnthroParameters.GetHeadDepthParameterFromLUT( oAnthroParameters.dHeadDepth );
								double dDeltaWidth         = oAnthroParameters.dHeadWidth - pDAFFDirectivityListener->GetProperties( )->oAnthroParams.dHeadWidth;
								double dDeltaHeight        = oAnthroParameters.dHeadHeight - pDAFFDirectivityListener->GetProperties( )->oAnthroParams.dHeadHeight;
								double dDeltaDepth         = oAnthroParameters.dHeadDepth - pDAFFDirectivityListener->GetProperties( )->oAnthroParams.dHeadDepth;
								double dPhiRad             = rcvr_is_polar.x * ITAConstants::PI_F / 180.0;
								double dThetaRad           = -( rcvr_is_polar.y * ITAConstants::PI_F / 180.0 - ITAConstants::PI_F / 2.0f );
								double dAmplitude          = dPhiRad * ( dDeltaWidth * ( dThetaRad * dThetaRad * daw + dThetaRad * dbw ) +
                                                                dDeltaHeight * ( dThetaRad * dThetaRad * dah + dThetaRad * dbh ) +
                                                                dDeltaDepth * ( dThetaRad * dThetaRad * dad + dThetaRad * dbd ) );
								double dITDCorrectionShift = std::sin( dPhiRad ) * dAmplitude; // seems to do nothing
								ITD_delay_s                = round( dITDCorrectionShift * sampling_rate );

								if( ( delay_s + ITD_delay_s + m_HRIR_length < NumSamples - 1 ) && ( delay_s - ITD_delay_s > 0 ) )
								{                                         // makes sure that a value larger than the length of the impulse response is not added
									m_sfHRIRTemp.mul_scalar( amplitude ); // scales the HRIR by the amplitude
									sbIRL.AddSamples( m_sfHRIRTemp[0].data( ), m_HRIR_length, delay_s - ITD_delay_s ); // add samples to IR
									sbIRR.AddSamples( m_sfHRIRTemp[1].data( ), m_HRIR_length, delay_s + ITD_delay_s );
								}
							}
							else
							{ // when there is no receiver directivity
								if( delay_s < NumSamples - 1 )
								{ // if the delay is less than the buffer length
									( sbIRL.data( ) )[delay_s] = amplitude;
									( sbIRR.data( ) )[delay_s] = amplitude;
								}
							}

						} // w
					}     // v
				}         // u
			}             // n
		}                 // m
	}                     // l

	/* //normalise to avoid clipping
	float L_peak, R_peak, peak;
	L_peak = sbIRL.FindPeak();
	R_peak = sbIRR.FindPeak();
	peak = max(L_peak, R_peak);
	sbIRL.div_scalar(peak);
	sbIRR.div_scalar(peak);
	*/

	/* // example how to export to a wave file
	writeAudiofile("image_source_irL.wav", &sbIRL, sampling_rate, ITAQuantization::ITA_FLOAT);
	writeAudiofile("image_source_irR.wav", &sbIRR, sampling_rate, ITAQuantization::ITA_FLOAT);
	*/
}


inline void CVAPTImageSourceAudioRenderer::CartVec2PolarVec( VAVec3 in, VAVec3* out )
{
	// converts a cartesian input vector "in" to a spherical polar output vector "out" in degrees
	// out[0] = azimuthal angle, out[1] = elevation angle, out[2] = radius/ distance
	out->z = in.Length( );
	out->x = ( atan2( in.y, in.x ) * 180 / ITAConstants::PI_F ) - 90;
	out->y = ( acos( in.z / out->z ) * 180 / ITAConstants::PI_F ) - 90;
}


ITADatasource* CVAPTImageSourceAudioRenderer::GetOutputDatasource( )
{
	return m_pOutput;
}

void CVAPTImageSourceAudioRenderer::ManageSoundPaths( const CVASceneState* pCurScene, const CVASceneState* pNewScene, const CVASceneStateDiff* pDiff )
{
	// Iterate over current paths and mark deleted (will be removed within internal sync of audio context thread)
	std::list<CVAPTImageSourceSoundPath*>::iterator itp = m_lSoundPaths.begin( );
	while( itp != m_lSoundPaths.end( ) )
	{
		CVAPTImageSourceSoundPath* pPath( *itp );
		int iSourceID            = pPath->pSource->pData->iID;
		int iListenerID          = pPath->pListener->pData->iID;
		bool bDeletetionRequired = false;

		// Source deleted?
		std::vector<int>::const_iterator cits = pDiff->viDelSoundSourceIDs.begin( );
		while( cits != pDiff->viDelSoundSourceIDs.end( ) )
		{
			const int& iIDDeletedSource( *cits++ );
			if( iSourceID == iIDDeletedSource )
			{
				bDeletetionRequired = true; // Source removed, deletion required
				break;
			}
		}

		if( bDeletetionRequired == false )
		{
			// Receiver deleted?
			std::vector<int>::const_iterator citr = pDiff->viDelReceiverIDs.begin( );
			while( citr != pDiff->viDelReceiverIDs.end( ) )
			{
				const int& iIDListenerDeleted( *citr++ );
				if( iListenerID == iIDListenerDeleted )
				{
					bDeletetionRequired = true; // Listener removed, deletion required
					break;
				}
			}
		}

		if( bDeletetionRequired )
		{
			DeleteSoundPath( pPath );
			itp = m_lSoundPaths.erase( itp ); // Increment via erase on path list
		}
		else
		{
			++itp; // no deletion detected, continue
		}
	}

	// Deleted sources
	std::vector<int>::const_iterator cits = pDiff->viDelSoundSourceIDs.begin( );
	while( cits != pDiff->viDelSoundSourceIDs.end( ) )
	{
		const int& iID( *cits++ );
		DeleteSource( iID );
	}

	// Deleted receivers
	std::vector<int>::const_iterator citr = pDiff->viDelReceiverIDs.begin( );
	while( citr != pDiff->viDelReceiverIDs.end( ) )
	{
		const int& iID( *citr++ );
		DeleteListener( iID );
	}

	// New sources
	cits = pDiff->viNewSoundSourceIDs.begin( );
	while( cits != pDiff->viNewSoundSourceIDs.end( ) )
	{
		const int& iID( *cits++ );
		CVAPTGPSource* pSource = CreateSource( iID, pNewScene->GetSoundSourceState( iID ) );
	}

	// New receivers
	citr = pDiff->viNewReceiverIDs.begin( );
	while( citr != pDiff->viNewReceiverIDs.end( ) )
	{
		const int& iID( *citr++ );
		CVAPTGPListener* pListener = CreateListener( iID, pNewScene->GetReceiverState( iID ) );
	}

	// New paths: (1) new receivers, current sources
	citr = pDiff->viNewReceiverIDs.begin( );
	while( citr != pDiff->viNewReceiverIDs.end( ) )
	{
		int iListenerID            = ( *citr++ );
		CVAPTGPListener* pListener = m_mListeners[iListenerID];

		for( size_t i = 0; i < pDiff->viComSoundSourceIDs.size( ); i++ )
		{
			int iSourceID          = pDiff->viComSoundSourceIDs[i];
			CVAPTGPSource* pSource = m_mSources[iSourceID];
			if( !pSource->bDeleted ) // only, if not marked for deletion
				CVAPTImageSourceSoundPath* pPath = CreateSoundPath( pSource, pListener );
		}
	}

	// New paths: (2) new sources, current receivers
	cits = pDiff->viNewSoundSourceIDs.begin( );
	while( cits != pDiff->viNewSoundSourceIDs.end( ) )
	{
		const int& iSourceID( *cits++ );
		CVAPTGPSource* pSource = m_mSources[iSourceID];

		for( size_t i = 0; i < pDiff->viComReceiverIDs.size( ); i++ )
		{
			int iListenerID            = pDiff->viComReceiverIDs[i];
			CVAPTGPListener* pListener = m_mListeners[iListenerID];
			if( !pListener->bDeleted )
				CVAPTImageSourceSoundPath* pPath = CreateSoundPath( pSource, pListener );
		}
	}

	// New paths: (3) new sources, new receivers
	cits = pDiff->viNewSoundSourceIDs.begin( );
	while( cits != pDiff->viNewSoundSourceIDs.end( ) )
	{
		const int& iSourceID( *cits++ );
		CVAPTGPSource* pSource = m_mSources[iSourceID];

		citr = pDiff->viNewReceiverIDs.begin( );
		while( citr != pDiff->viNewReceiverIDs.end( ) )
		{
			const int& iListenerID( *citr++ );
			CVAPTGPListener* pListener       = m_mListeners[iListenerID];
			CVAPTImageSourceSoundPath* pPath = CreateSoundPath( pSource, pListener );
		}
	}

	return;
}

CVAPTImageSourceSoundPath* CVAPTImageSourceAudioRenderer::CreateSoundPath( CVAPTGPSource* pSource, CVAPTGPListener* pListener )
{
	int iSourceID   = pSource->pData->iID;
	int iListenerID = pListener->pData->iID;

	assert( !pSource->bDeleted && !pListener->bDeleted );

	VA_VERBOSE( "PTImageSourceAudioRenderer", "Creating sound path from source " << iSourceID << " -> sound receiver " << iListenerID );

	CVAPTImageSourceSoundPath* pPath = dynamic_cast<CVAPTImageSourceSoundPath*>( m_pSoundPathPool->RequestObject( ) );

	pPath->pSource   = pSource;
	pPath->pListener = pListener;

	pPath->bDelete = false;

	m_lSoundPaths.push_back( pPath );
	m_pUpdateMessage->vNewPaths.push_back( pPath );

	return pPath;
}

void CVAPTImageSourceAudioRenderer::DeleteSoundPath( CVAPTImageSourceSoundPath* pPath )
{
	VA_VERBOSE( "PTImageSourceAudioRenderer",
	            "Marking sound path from source " << pPath->pSource->pData->iID << " -> sound receiver " << pPath->pListener->pData->iID << " for deletion" );

	pPath->bDelete = true;
	pPath->RemoveReference( );
	m_pUpdateMessage->vDelPaths.push_back( pPath );
}

CVAPTImageSourceAudioRenderer::CVAPTGPListener* CVAPTImageSourceAudioRenderer::CreateListener( const int iID, const CVAReceiverState* pListenerState )
{
	VA_VERBOSE( "PTImageSourceAudioRenderer", "Creating sound receiver with ID " << iID );

	CVAPTGPListener* pListener = dynamic_cast<CVAPTGPListener*>( m_pListenerPool->RequestObject( ) ); // Reference = 1

	pListener->pData = m_pCore->GetSceneManager( )->GetSoundReceiverDesc( iID );
	pListener->pData->AddReference( );

	// Move to prerequest of pool?
	pListener->psfOutput = new ITASampleFrame( m_iNumChannels, m_pCore->GetCoreConfig( )->oAudioDriverConfig.iBuffersize, true );
	assert( pListener->pData );
	pListener->bDeleted = false;

	m_mListeners.insert( std::pair<int, CVAPTImageSourceAudioRenderer::CVAPTGPListener*>( iID, pListener ) );

	m_pUpdateMessage->vNewListeners.push_back( pListener );

	return pListener;
}

void CVAPTImageSourceAudioRenderer::DeleteListener( int iListenerID )
{
	VA_VERBOSE( "PTImageSourceAudioRenderer", "Marking sound receiver with ID " << iListenerID << " for removal" );
	std::map<int, CVAPTGPListener*>::iterator it = m_mListeners.find( iListenerID );
	CVAPTGPListener* pListener                   = it->second;
	m_mListeners.erase( it );
	pListener->bDeleted = true;
	pListener->pData->RemoveReference( );
	pListener->RemoveReference( );

	m_pUpdateMessage->vDelListeners.push_back( pListener );

	return;
}

CVAPTImageSourceAudioRenderer::CVAPTGPSource* CVAPTImageSourceAudioRenderer::CreateSource( int iID, const CVASoundSourceState* pSourceState )
{
	VA_VERBOSE( "PTImageSourceAudioRenderer", "Creating source with ID " << iID );
	auto pSource = dynamic_cast<CVAPTImageSourceAudioRenderer::CVAPTGPSource*>( m_pSourcePool->RequestObject( ) );

	pSource->pData = m_pCore->GetSceneManager( )->GetSoundSourceDesc( iID );
	pSource->pData->AddReference( );

	pSource->bDeleted = false;

	m_mSources.insert( std::pair<int, CVAPTGPSource*>( iID, pSource ) );

	m_pUpdateMessage->vNewSources.push_back( pSource );

	return pSource;
}

void CVAPTImageSourceAudioRenderer::DeleteSource( int iSourceID )
{
	VA_VERBOSE( "PTImageSourceAudioRenderer", "Marking source with ID " << iSourceID << " for removal" );
	std::map<int, CVAPTGPSource*>::iterator it = m_mSources.find( iSourceID );
	CVAPTGPSource* pSource                     = it->second;
	m_mSources.erase( it );
	pSource->bDeleted = true;
	pSource->pData->RemoveReference( );
	pSource->RemoveReference( );

	m_pUpdateMessage->vDelSources.push_back( pSource );

	return;
}

void CVAPTImageSourceAudioRenderer::SyncInternalData( )
{
	CVAPTGPUpdateMessage* pUpdate;
	while( ctxAudio.m_qpUpdateMessages.try_pop( pUpdate ) )
	{
		std::list<CVAPTImageSourceSoundPath*>::const_iterator citp = pUpdate->vDelPaths.begin( );
		while( citp != pUpdate->vDelPaths.end( ) )
		{
			CVAPTImageSourceSoundPath* pPath( *citp++ );
			ctxAudio.m_lSoundPaths.remove( pPath );
			pPath->RemoveReference( );
		}

		citp = pUpdate->vNewPaths.begin( );
		while( citp != pUpdate->vNewPaths.end( ) )
		{
			CVAPTImageSourceSoundPath* pPath( *citp++ );
			pPath->AddReference( );
			ctxAudio.m_lSoundPaths.push_back( pPath );
		}

		std::list<CVAPTGPSource*>::const_iterator cits = pUpdate->vDelSources.begin( );
		while( cits != pUpdate->vDelSources.end( ) )
		{
			CVAPTGPSource* pSource( *cits++ );
			ctxAudio.m_lSources.remove( pSource );
			pSource->pData->RemoveReference( );
			pSource->RemoveReference( );
		}

		cits = pUpdate->vNewSources.begin( );
		while( cits != pUpdate->vNewSources.end( ) )
		{
			CVAPTGPSource* pSource( *cits++ );
			pSource->AddReference( );
			pSource->pData->AddReference( );
			ctxAudio.m_lSources.push_back( pSource );
		}

		std::list<CVAPTGPListener*>::const_iterator citl = pUpdate->vDelListeners.begin( );
		while( citl != pUpdate->vDelListeners.end( ) )
		{
			CVAPTGPListener* pListener( *citl++ );
			ctxAudio.m_lListener.remove( pListener );
			pListener->pData->RemoveReference( );
			pListener->RemoveReference( );
		}

		citl = pUpdate->vNewListeners.begin( );
		while( citl != pUpdate->vNewListeners.end( ) )
		{
			CVAPTGPListener* pListener( *citl++ );
			pListener->AddReference( );
			pListener->pData->AddReference( );
			ctxAudio.m_lListener.push_back( pListener );
		}

		pUpdate->RemoveReference( );
	}

	return;
}

void CVAPTImageSourceAudioRenderer::ResetInternalData( )
{
	std::list<CVAPTImageSourceSoundPath*>::const_iterator citp = ctxAudio.m_lSoundPaths.begin( );
	while( citp != ctxAudio.m_lSoundPaths.end( ) )
	{
		CVAPTImageSourceSoundPath* pPath( *citp++ );
		pPath->RemoveReference( );
	}
	ctxAudio.m_lSoundPaths.clear( );

	std::list<CVAPTGPListener*>::const_iterator itl = ctxAudio.m_lListener.begin( );
	while( itl != ctxAudio.m_lListener.end( ) )
	{
		CVAPTGPListener* pListener( *itl++ );
		pListener->pData->RemoveReference( );
		pListener->RemoveReference( );
	}
	ctxAudio.m_lListener.clear( );

	std::list<CVAPTGPSource*>::const_iterator cits = ctxAudio.m_lSources.begin( );
	while( cits != ctxAudio.m_lSources.end( ) )
	{
		CVAPTGPSource* pSource( *cits++ );
		pSource->pData->RemoveReference( );
		pSource->RemoveReference( );
	}
	ctxAudio.m_lSources.clear( );

	ctxAudio.m_iResetFlag = 2; // set ack
}

void CVAPTImageSourceAudioRenderer::UpdateGenericSoundPath( int iListenerID, int iSourceID, const std::string& sIRFilePath )
{
	ITASampleFrame sfIR( sIRFilePath );
	if( sfIR.channels( ) != m_pOutput->GetNumberOfChannels( ) )
		VA_EXCEPT2( INVALID_PARAMETER, "Filter has mismatching channels" );
	UpdateGenericSoundPath( iListenerID, iSourceID, sfIR );
}

void CVAPTImageSourceAudioRenderer::UpdateGenericSoundPath( int iListenerID, int iSourceID, int iChannelIndex, const std::string& sIRFilePath )
{
	if( iChannelIndex >= m_iNumChannels || iChannelIndex < 0 )
		VA_EXCEPT2( INVALID_PARAMETER, "Requested filter channel greater than output channels or smaller than 1, can not update." );

	ITASampleFrame sfIR( sIRFilePath );
	if( sfIR.channels( ) != 1 )
	{
		if( sfIR.channels( ) != m_iNumChannels )
		{
			VA_ERROR( "PTImageSourceAudioRenderer", "Filter has mismatching and more than one channel. Don't know what to do - refusing this update." );
		}
		else
		{
			VA_WARN( "PTImageSourceAudioRenderer", "Filter has more than one channel, updating only requested channel index." );
			UpdateGenericSoundPath( iListenerID, iSourceID, iChannelIndex, sfIR[iChannelIndex] );
		}
	}
	else
	{
		UpdateGenericSoundPath( iListenerID, iSourceID, iChannelIndex, sfIR[0] );
	}
}
void CVAPTImageSourceAudioRenderer::UpdateGenericSoundPath( const int iListenerID, const int iSourceID, const double dDelaySeconds )
{
	if( dDelaySeconds < 0.0f )
		VA_WARN( "PTImageSourceAudioRenderer", "Delay in variable delay line can not be anti-causal. Shift IR if necessary." );

	std::list<CVAPTImageSourceSoundPath*>::const_iterator spcit = m_lSoundPaths.begin( );
	while( spcit != m_lSoundPaths.end( ) )
	{
		CVAPTImageSourceSoundPath* pPath( *spcit++ );
		if( pPath->pListener->pData->iID == iListenerID && pPath->pSource->pData->iID == iSourceID )
		{
			pPath->pVariableDelayLine->SetDelayTime( dDelaySeconds );
			return;
		}
	}

	VA_ERROR( "PTImageSourceAudioRenderer", "Could not find sound path for sound receiver " << iListenerID << " and source " << iSourceID );
}

void CVAPTImageSourceAudioRenderer::UpdateGenericSoundPath( int iListenerID, int iSourceID, ITASampleFrame& sfIR )
{
	if( sfIR.length( ) > m_iIRFilterLengthSamples )
		VA_WARN( "PTImageSourceAudioRenderer", "Filter length for generic sound path too long, cropping." );

	std::list<CVAPTImageSourceSoundPath*>::const_iterator spcit = m_lSoundPaths.begin( );
	while( spcit != m_lSoundPaths.end( ) )
	{
		CVAPTImageSourceSoundPath* pPath( *spcit++ );
		if( pPath->pListener->pData->iID == iListenerID && pPath->pSource->pData->iID == iSourceID )
		{
			for( int n = 0; n < m_iNumChannels; n++ )
			{
				ITAUPConvolution* pConvolver( pPath->vpFIRConvolver[n] );
				ITAUPFilter* pFilter = pConvolver->RequestFilter( );
				pFilter->Zeros( );
				int iLength = (std::min)( m_iIRFilterLengthSamples, sfIR.length( ) );
				pFilter->Load( sfIR[n].data( ), iLength );

				pConvolver->ExchangeFilter( pFilter );
				pFilter->Release( );
			}
			return;
		}
	}

	VA_ERROR( "PTImageSourceAudioRenderer", "Could not find sound path for sound receiver " << iListenerID << " and source " << iSourceID );
}


void CVAPTImageSourceAudioRenderer::UpdateGenericSoundPath( int iListenerID, int iSourceID, int iChannelIndex, ITASampleBuffer& sbIR )
{
	if( iChannelIndex >= m_iNumChannels || iChannelIndex < 0 )
		VA_EXCEPT2( INVALID_PARAMETER, "Requested filter channel index of generic sound path out of bounds" );

	if( sbIR.length( ) > m_iIRFilterLengthSamples )
		VA_WARN( "PTImageSourceAudioRenderer", "Filter length for generic sound path channel too long, cropping." );

	std::list<CVAPTImageSourceSoundPath*>::const_iterator spcit = m_lSoundPaths.begin( );
	while( spcit != m_lSoundPaths.end( ) )
	{
		CVAPTImageSourceSoundPath* pPath( *spcit++ );
		if( pPath->pListener->pData->iID == iListenerID && pPath->pSource->pData->iID == iSourceID )
		{
			ITAUPConvolution* pConvolver( pPath->vpFIRConvolver[iChannelIndex] );
			ITAUPFilter* pFilter = pConvolver->RequestFilter( );
			pFilter->Zeros( );
			int iLength = (std::min)( m_iIRFilterLengthSamples, sbIR.length( ) );
			pFilter->Load( sbIR.data( ), iLength );
			pConvolver->ExchangeFilter( pFilter );
			pFilter->Release( );

			return;
		}
	}

	VA_ERROR( "PTImageSourceAudioRenderer", "Could not find sound path for sound receiver " << iListenerID << " and source " << iSourceID );
}

void CVAPTImageSourceAudioRenderer::UpdateGlobalAuralizationMode( int )
{
	return;
}

void CVAPTImageSourceAudioRenderer::HandleProcessStream( ITADatasourceRealization*, const ITAStreamInfo* pStreamInfo )
{
	// If streaming is active, set to 1
	ctxAudio.m_iStatus = 1;

	// Sync pathes
	SyncInternalData( );

	const CVAAudiostreamState* pStreamState = dynamic_cast<const CVAAudiostreamState*>( pStreamInfo );
	double dListenerTime                    = pStreamState->dSysTime;

	// Check for reset request
	if( ctxAudio.m_iResetFlag == 1 )
	{
		ResetInternalData( );

		return;
	}
	else if( ctxAudio.m_iResetFlag == 2 )
	{
		// Reset active, skip until finished
		return;
	}

	std::map<int, CVAPTGPListener*>::iterator lit = m_mListeners.begin( );
	while( lit != m_mListeners.end( ) )
	{
		CVAPTGPListener* pListener( lit->second );
		pListener->psfOutput->zero( );
		lit++;
	}

	// Update sound pathes
	std::list<CVAPTImageSourceSoundPath*>::iterator spit = ctxAudio.m_lSoundPaths.begin( );
	while( spit != ctxAudio.m_lSoundPaths.end( ) )
	{
		CVAPTImageSourceSoundPath* pPath( *spit );
		CVAReceiverState* pListenerState  = ( m_pCurSceneState ? m_pCurSceneState->GetReceiverState( pPath->pListener->pData->iID ) : NULL );
		CVASoundSourceState* pSourceState = ( m_pCurSceneState ? m_pCurSceneState->GetSoundSourceState( pPath->pSource->pData->iID ) : NULL );

		if( pListenerState == nullptr || pSourceState == nullptr )
		{
			// Skip if no data is present
			spit++;
			continue;
		}

		CVASoundSourceDesc* pSourceData = pPath->pSource->pData;
		const ITASampleFrame& sfInputBuffer( *pSourceData->psfSignalSourceInputBuf );
		const ITASampleBuffer* psbInput( &( sfInputBuffer[0] ) );

		pPath->pVariableDelayLine->Process( psbInput, &m_sfTempBuffer );

		float fSoundSourceGain = float( pSourceState->GetVolume( m_oParams.pCore->GetCoreConfig( )->dDefaultAmplitudeCalibration ) );

		for( int n = 0; n < m_iNumChannels; n++ )
		{
			ITAUPConvolution* pConvolver = pPath->vpFIRConvolver[n];
			pConvolver->SetGain( fSoundSourceGain );
			pConvolver->Process( m_sfTempBuffer.GetData( ), ( *pPath->pListener->psfOutput )[n].data( ), ITABase::MixingMethod::ADD );
		}

		spit++;
	}

	for( int n = 0; n < int( m_pOutput->GetNumberOfChannels( ) ); n++ )
		for( int i = 0; i < int( m_pOutput->GetBlocklength( ) ); i++ )
			m_pOutput->GetWritePointer( n )[i] = 0.0f;

	for( auto it: m_mListeners )
	{
		CVAPTGPListener* pActiveListener = it.second;

		if( !pActiveListener->pData->bMuted )
			for( int n = 0; n < int( m_pOutput->GetNumberOfChannels( ) ); n++ )
				for( int i = 0; i < int( m_pOutput->GetBlocklength( ) ); i++ )
					m_pOutput->GetWritePointer( n )[i] += ( *pActiveListener->psfOutput )[n][i];

		bool bDataPresent = false;
		std::vector<float> vfRMS( m_iNumChannels );
		for( int n = 0; n < int( m_pOutput->GetNumberOfChannels( ) ); n++ )
		{
			vfRMS[n] = 0.0f;
			for( int i = 0; i < int( m_pOutput->GetBlocklength( ) ); i++ )
				vfRMS[n] += pow( std::abs( m_pOutput->GetWritePointer( n )[i] ), 2 );
			vfRMS[n] = std::sqrt( vfRMS[n] ) / m_pOutput->GetBlocklength( );
			if( vfRMS[n] != 0.0f )
				bDataPresent = true;
		}

		if( !pActiveListener->pData->bMuted && m_bOutputMonitoring &&
		    int( pStreamInfo->nSamples / m_pOutput->GetBlocklength( ) ) % int( m_pOutput->GetSampleRate( ) / m_pOutput->GetBlocklength( ) * 2.0f ) == 0 )
		{
			if( bDataPresent )
			{
				VA_TRACE( m_oParams.sID, "RMS at active sound receiver: <Ch1," << vfRMS[0] << "> <Ch2," << vfRMS[1] << "> " );
			}
			else
			{
				VA_WARN( m_oParams.sID, "No data on output channel for active sound receiver present (empty path filter?)" );
			}
		}
	}

	m_pOutput->IncrementWritePointer( );

	return;
}

std::string CVAPTImageSourceAudioRenderer::HelpText( ) const
{
	std::stringstream ss;
	ss << std::endl;
	ss << " --- ImageSource renderer instance '" << m_oParams.sID << "' ---" << std::endl;
	ss << std::endl;
	ss << "[help]" << std::endl;
	ss << "If the call module struct contains a key with the name 'help', this help text will be shown and the return struct will be returned with the key name 'help'."
	   << std::endl;
	ss << std::endl;
	ss << "[info]" << std::endl;
	ss << "If the call module struct contains a key with the name 'info', information on the static configuration of the renderer will be returned." << std::endl;
	ss << std::endl;
	ss << "[update]" << std::endl;
	ss << "For every successful path update, the VA source and sound receiver ID has to be passed like this:" << std::endl;
	ss << " receiver: <int>, the number of the sound receiver identifier" << std::endl;
	ss << " source: <int>, the number of the source identifier" << std::endl;
	ss << std::endl;
	ss << "Updating the path filter (impulse response in time domain) for a sound receiver and a source can be performed in two ways:" << std::endl;
	ss << " a) using a path to a multi-channel WAV file:" << std::endl;
	ss << "    Provide a key with the name 'filepath' and the path to the WAV file (absolute or containing the macro '$(VADataDir)' or relative to the executable) "
	      "[priority given to 'filepath' if b) also applies]"
	   << std::endl;
	ss << " b) sending floating-point data for each channel" << std::endl;
	ss << "    Provide a key for each channel with the generic name 'ch#', where the hash is substituted by the actual number of channel (starting at 1), and the value "
	      "to this key will contain floating point data (or a sample buffer). "
	   << "The call parameter struct does not necessarily have to contain all channels, also single channels will be updated if key is given." << std::endl;
	ss << std::endl;
	ss << "Note: the existence of the key 'verbose' will print update information at server console and will provide the update info as an 'info' key in the returned "
	      "struct."
	   << std::endl;
	return ss.str( );
}

CVAStruct CVAPTImageSourceAudioRenderer::GetParameters( const CVAStruct& oArgs ) const
{
	CVAStruct oReturn;

	if( oArgs.HasKey( "help" ) || oArgs.IsEmpty( ) )
	{
		// Print and return help text
		VA_PRINT( HelpText( ) );
		oReturn["help"] = HelpText( );
		return oReturn;
	}

	oReturn["numchannels"]           = m_iNumChannels;
	oReturn["irfilterlengthsamples"] = m_iIRFilterLengthSamples;
	oReturn["numpaths"]              = int( m_lSoundPaths.size( ) );
	oReturn["RoomLength"]            = m_RoomLength;
	oReturn["RoomWidth"]             = m_RoomWidth;
	oReturn["RoomHeight"]            = m_RoomHeight;
	oReturn["MaxOrder"]              = m_MaxOrder;

	return oReturn;
}

void CVAPTImageSourceAudioRenderer::SetParameters( const CVAStruct& oArgs )
{
	if( oArgs.HasKey( "RoomLength" ) == true || oArgs.HasKey( "RoomWidth" ) == true || oArgs.HasKey( "RoomHeight" ) == true || oArgs.HasKey( "MaxOrder" ) == true ||
	    oArgs.HasKey( "betax1" ) == true || oArgs.HasKey( "betax2" ) == true || oArgs.HasKey( "betay1" ) == true || oArgs.HasKey( "betay2" ) == true ||
	    oArgs.HasKey( "betaz1" ) == true || oArgs.HasKey( "betaz2" ) == true || oArgs.HasKey( "DirectSound" ) == true )
	{
		bool IR_change = false;
		if( oArgs.HasKey( "RoomLength" ) )
		{ // Set the length of the shoebox room
			m_RoomLength = oArgs["RoomLength"];
			IR_change    = true;
		}

		if( oArgs.HasKey( "RoomWidth" ) )
		{ // Set the width of the shoebox room
			m_RoomWidth = oArgs["RoomWidth"];
			IR_change   = true;
		}

		if( oArgs.HasKey( "RoomHeight" ) )
		{ // Set the height of the shoebox room
			m_RoomHeight = oArgs["RoomHeight"];
			IR_change    = true;
		}

		if( oArgs.HasKey( "MaxOrder" ) )
		{ // set the maximum order of image sources to be computed
			m_MaxOrder = oArgs["MaxOrder"];
			IR_change  = true;
		}

		if( oArgs.HasKey( "DirectSound" ) )
		{ // set whether direct sound is included
			m_direct_sound = oArgs["DirectSound"];
		}

		if( IR_change ) // if a relevant factor was changed, update the filter length
			SetIRFilterLength( );

		std::string var;
		for( int i = 0; i < 6; i++ )
		{
			switch( i )
			{
				case 0:
					var = "betax1";
					break;
				case 1:
					var = "betax2";
					break;
				case 2:
					var = "betay1";
					break;
				case 3:
					var = "betay2";
					break;
				case 4:
					var = "betaz1";
					break;
				case 5:
					var = "betaz2";
					break;
			}
			if( oArgs.HasKey( var ) )
			{
				m_beta[i] = oArgs[var];
			}
		}

		UpdateFilter( );
	}
	else
	{
		if( oArgs.HasKey( "receiver" ) == false || oArgs.HasKey( "source" ) == false )
			VA_EXCEPT2( INVALID_PARAMETER, "Given values do not match any of the accepted parameters for PrototypeImageSource renderer" );
	}
	// Update
	if( oArgs.HasKey( "receiver" ) == false || oArgs.HasKey( "source" ) == false )
		return;
	//	VA_EXCEPT2( INVALID_PARAMETER, "PrototypeImageSource filter update requires a receiver and a source identifier" );

	int iReceiverID = oArgs["receiver"];
	int iSourceID   = oArgs["source"];

	bool bVerbose = false;
	if( oArgs.HasKey( "verbose" ) )
		bVerbose = true;

	if( oArgs.HasKey( "delay" ) )
	{
		const double dDelaySeconds = oArgs["delay"];
		UpdateGenericSoundPath( iReceiverID, iSourceID, dDelaySeconds );
	}


	if( oArgs.HasKey( "filepath" ) )
	{
		const std::string& sFilePathRaw( oArgs["filepath"] );
		std::string sFilePath = m_pCore->FindFilePath( sFilePathRaw );

		if( oArgs.HasKey( "channel" ) )
		{
			int iChannelNumber = oArgs["channel"];
			UpdateGenericSoundPath( iReceiverID, iSourceID, iChannelNumber - 1, sFilePath );
		}
		else
		{
			UpdateGenericSoundPath( iReceiverID, iSourceID, sFilePath );
		}

		std::stringstream ssVerboseText;
		if( bVerbose )
		{
			ssVerboseText << "Updated sound path <L" << iReceiverID << ", S" << iSourceID << "> using (unrolled) file path '" << sFilePath << "'";
			VA_PRINT( ssVerboseText.str( ) );
		}
	}
	else
	{
		ITASampleBuffer sbIR( m_iIRFilterLengthSamples );
		for( int n = 0; n < int( m_pOutput->GetNumberOfChannels( ) ); n++ )
		{
			std::string sChKey = "ch" + IntToString( n + 1 );
			if( !oArgs.HasKey( sChKey ) )
				continue;

			const CVAStructValue& oValue( oArgs[sChKey] );
			int iNumSamples = 0;

			if( oValue.GetDatatype( ) == CVAStructValue::DATA )
			{
				int iBytes  = oValue.GetDataSize( );
				iNumSamples = iBytes / int( sizeof( float ) ); // crop overlapping bytes for safety (by integer division)

				if( iNumSamples > m_iIRFilterLengthSamples )
				{
					VA_WARN( "PTImageSourceAudioRenderer", "Given IR filter too long, cropping to fit internal filter length." );
					iNumSamples = m_iIRFilterLengthSamples;
				}

				const float* pfData = (const float*)( oValue.GetData( ) );
				sbIR.Zero( );
				sbIR.write( pfData, iNumSamples );
			}
			else if( oValue.GetDatatype( ) == CVAStructValue::SAMPLEBUFFER )
			{
				const CVASampleBuffer& oSampleBuffer = oValue;

				iNumSamples = oSampleBuffer.GetNumSamples( ); // crop overlapping bytes for safety (by integer division)

				if( iNumSamples > m_iIRFilterLengthSamples )
				{
					VA_WARN( "PTImageSourceAudioRenderer", "Given IR filter too long, cropping to fit internal filter length." );
					iNumSamples = m_iIRFilterLengthSamples;
				}

				sbIR.Zero( );
				sbIR.write( oSampleBuffer.GetDataReadOnly( ), iNumSamples );
			}

			UpdateGenericSoundPath( iReceiverID, iSourceID, n, sbIR );

			std::stringstream ssVerboseText;
			if( bVerbose )
			{
				ssVerboseText << "Updated sound path <L" << iReceiverID << ", S" << iSourceID << ", C" << n + 1 << "> with " << iNumSamples << " new samples";
				VA_PRINT( ssVerboseText.str( ) );
			}
		}
	}
}

#endif // VACORE_WITH_RENDERER_PROTOTYPE_IMAGE_SOURCE
