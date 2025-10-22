#include "VABinauralOutdoorWaveFront.h"

#include "VABinauralOutdoorSource.h"

// VA core includes
#include "../../../Motion/VAMotionModelBase.h"
#include "../../../Motion/VASharedMotionModel.h"
#include "../../../Scene/VAScene.h"

// ITA includes
#include <ITAException.h>
#include <ITAIIRCoefficients.h>
#include <ITAIIRFilterGenerator.h>
#include <ITASIMOVariableDelayLineBase.h>
#include <ITASampleBuffer.h>
#include <ITAThirdOctaveFIRFilterGenerator.h>

// STL includes
#include <cmath>


CVABinauralOutdoorWaveFront::CVABinauralOutdoorWaveFront( const CVABinauralOutdoorWaveFront::Config& oConf )
    : oConf( oConf )
    , m_bClusteringPoseSet( false )
    , m_bWaveFrontOriginSet( false )
    , m_oDelayHistory( oConf.iHistoryBufferSize, CVAHistoryEstimationMethod::EMethod::TriangularWindow, 2.0, 0.0 )
    , m_oGainHistory( oConf.iHistoryBufferSize, CVAHistoryEstimationMethod::EMethod::NearestNeighbor )
    , m_oThirdOctaveFilterHistory( oConf.iHistoryBufferSize, CVAHistoryEstimationMethod::EMethod::Linear )
    , m_oReceiverWFOriginHistory( oConf.iHistoryBufferSize, CVAHistoryEstimationMethod::EMethod::NearestNeighbor )
    , m_oAudibilityHistory( oConf.iHistoryBufferSize )
{
	if( oConf.iFilterDesignAlgorithm == ITADSP::BURG )
	{
		m_iIIRFilterCoeffs.Initialise( oConf.iIIRFilterOrder, false );
		m_iIIRFilterCoeffs.iDesignAlgorithm = ITADSP::BURG;
	}
	else
	{
		VA_EXCEPT1( "Filter coefficient design algorithm not recognised / not yet implemented" );
	}

	m_oIIRFilterEngine.Initialise( oConf.iIIRFilterOrder, false ); // initialise the interal IIR filter

	m_sbIIRFeed.Init( oConf.iBufferSize, true ); // initialise the buffers to a size which will not be exceeded
	m_sbTemp.Init( oConf.iBufferSize, true );    // initialise the buffers to a size which will not be exceeded
	m_sbInterpolated.Init( oConf.iBufferSize, true );

#ifdef BINAURAL_OUTDOOR_NOISE_IIR_OUT_DEBUG
	m_sbDebug.Init( 60 * oConf.dSampleRate, true );
	m_ulDebugWriteCursor = 0;
#endif

	if( oConf.iInterpolationFunction == ITABase::InterpolationFunctions::LINEAR )
		m_pInterpolationRoutine = new CITASampleLinearInterpolation( );
	else
		m_pInterpolationRoutine = new CITASampleCubicSplineInterpolation( );

	m_iITDDifferenceNew = 0;
	m_bAudible          = false;
	m_fGainCur          = 0.0f;
	m_fGainNew          = 0.0f;

	if( oConf.iHistoryBufferSize ) // also when using history, wavefront is diabled at first
		m_oAudibilityHistory.Push( 0.0, std::make_unique<bool>( false ) );
}

CVABinauralOutdoorWaveFront::~CVABinauralOutdoorWaveFront( )
{
	delete m_pInterpolationRoutine;

#ifdef BINAURAL_OUTDOOR_NOISE_IIR_OUT_DEBUG
	if( m_ulDebugWriteCursor > 0 )
		m_sbDebug.Store( "BONR_IIR_outstream.wav" );
#endif
}

void CVABinauralOutdoorWaveFront::PreRequest( )
{
	m_bClusteringPoseSet  = false;
	m_bWaveFrontOriginSet = false;
	m_iITDDifferenceNew   = 0;
	m_iITDDifferenceOld   = 0;
	m_bAudible            = false;
	m_fGainCur            = 0.0f;
	m_fGainNew            = 0.0f;

	m_oAudibilityHistory.Reset( );
	m_oDelayHistory.Reset( );
	m_oGainHistory.Reset( );
	m_oReceiverWFOriginHistory.Reset( );
	m_oThirdOctaveFilterHistory.Reset( );
}

void CVABinauralOutdoorWaveFront::PreRelease( ) {}

void CVABinauralOutdoorWaveFront::GetOutput( ITASampleBuffer* pfLeftChannel, ITASampleBuffer* pfRightChannel )
{
#ifdef BINAURAL_OUTDOOR_NOISE_INSOURCE_BENCHMARKS
	m_swGetOutput.start( );
#endif

	assert( m_iITDDifferenceNew == 0 );

	/*
	 * This fn is called from the clustering direction, and outputs the left and right audio channels for a path.
	 * First get the data from the VDL
	 * Then apply the IIR filter to the single channel
	 * Then interpolate delay
	 * Then split into two channels and adjust for ITD to nearest sample
	 */

	float fGainOld = m_fGainCur;
	float fGainNew = m_bAudible ? m_fGainNew : 0.0f; // Switch to new gain or fade out

	int iCurrentDelay = m_pSIMOVDL->GetCurrentDelaySamples( m_iCursorID ); // Get current and new delay in samples
	int iNewDelay     = m_pSIMOVDL->GetNewDelaySamples( m_iCursorID );


#ifdef BINAURAL_OUTDOOR_NOISE_INSOURCE_BENCHMARKS
	m_swVDL.start( );
#endif
	int iInterpolateLeft, iInterpolateRight;
	m_pInterpolationRoutine->GetOverlapSamples( iInterpolateLeft, iInterpolateRight ); // Get number of overlap samples needed for the interpolation

	m_pSIMOVDL->SetOverlapSamples( m_iCursorID, iInterpolateLeft + m_iITDDifferenceNew, iInterpolateRight ); // Set the overlap to be read from the VDL

	int iEffectiveBufferSize = m_pSIMOVDL->GetEffectiveReadLength( m_iCursorID, oConf.iBlockLength );

	if( m_sbTemp.GetLength( ) < iEffectiveBufferSize )
		m_sbTemp.Init( iEffectiveBufferSize * 2 ); // Init should be avoided, so extend to twice the size in hope to avoid this call

	m_pSIMOVDL->Read( m_iCursorID, oConf.iBlockLength, m_sbTemp.GetData( ) ); // Take out variable-sized block from VDL (not interpolated yet!)

#ifdef BINAURAL_OUTDOOR_NOISE_INSOURCE_BENCHMARKS
	m_swVDL.stop( );
	m_swIIRFilter.start( );
#endif

	const int iInputLength = iEffectiveBufferSize - iInterpolateLeft - iInterpolateRight;
	if( m_sbIIRFeed.GetLength( ) < iInputLength )
		m_sbIIRFeed.Init( iInputLength * 2 ); // Init should be avoided, so extend to twice the size in hope to avoid this call

	m_oIIRFilterEngine.Process( m_sbTemp.GetData( ), m_sbIIRFeed.GetData( ) + iInterpolateLeft + iInterpolateRight, iInputLength, fGainOld, fGainNew,
	                            ITABase::MixingMethod::OVERWRITE ); // apply the IIR filter to the data taken from the VDL

#ifdef BINAURAL_OUTDOOR_NOISE_IIR_OUT_DEBUG
	int iCurretSamplesBatch = iIIRFeedLength;
	m_sbDebug.WriteSamples( m_sbIIRFeed.GetData( ), iCurretSamplesBatch, m_ulDebugWriteCursor ); // includes old overlap from last block at beginning of iir feed buffer!
	m_ulDebugWriteCursor += iCurretSamplesBatch;
#endif

#ifdef BINAURAL_OUTDOOR_NOISE_INSOURCE_BENCHMARKS
	m_swIIRFilter.stop( );
	m_swInterpolation.start( );
#endif

	unsigned int iInterpolatedSignalSize = oConf.iBlockLength + abs( m_iITDDifferenceNew );

	bool bInterpolationRequired = true;
	if( iCurrentDelay + m_iITDDifferenceOld == iNewDelay + m_iITDDifferenceNew )
		bInterpolationRequired = false;

	bool bSkipInterpoltionOnCorruptBlockLength = bool( iInputLength < 1 );
	if( !bSkipInterpoltionOnCorruptBlockLength && bInterpolationRequired )
	{
		m_pInterpolationRoutine->Interpolate( &m_sbIIRFeed, iInputLength, iInterpolateLeft, &m_sbInterpolated, iInterpolatedSignalSize );
		// Output size decreased to be blocklength + left/right margin for ITD difference offset
	}
	else
	{
		// Directly switch from new to old delay: no interpolation skip to end
		m_sbInterpolated.write( m_sbIIRFeed, iInterpolatedSignalSize, iInterpolateLeft, 0 );
		if( bSkipInterpoltionOnCorruptBlockLength )
			VA_WARN( "BinauralOutdoorNoiseRenderer",
			         "Could not perform propagation delay interpolation, distance change was above speed of sound. Hard-switching to new delay." );
	}
	// Store overlap for next pocessing block
	m_sbIIRFeed.WriteSamples( m_sbIIRFeed.GetData( ) + iInputLength, iInterpolateLeft + iInterpolateRight, 0 );


	// Apply ITD correction
	if( m_iITDDifferenceNew <= 0 )
	{
		// Adjust where data is read from the buffer depending on whether ITD diff is positive or negative
		pfLeftChannel->write( m_sbInterpolated, oConf.iBlockLength );
		pfRightChannel->write( m_sbInterpolated, oConf.iBlockLength, m_iITDDifferenceNew ); //@todo psc: -m_iITDDifference ?
	}
	else
	{
		pfLeftChannel->write( m_sbInterpolated, oConf.iBlockLength, m_iITDDifferenceNew );
		pfRightChannel->write( m_sbInterpolated, oConf.iBlockLength );
	}


#ifdef BINAURAL_OUTDOOR_NOISE_INSOURCE_BENCHMARKS
	m_swInterpolation.stop( );
#endif

	m_fGainCur = fGainNew;

#ifdef BINAURAL_OUTDOOR_NOISE_INSOURCE_BENCHMARKS
	m_swGetOutput.stop( );
#endif
}

void CVABinauralOutdoorWaveFront::SetParameters( const CVAStruct& oInArgs )
{
#ifdef BINAURAL_OUTDOOR_NOISE_INSOURCE_BENCHMARKS
	if( oInArgs.HasKey( "print_benchmarks" ) )
	{
		std::cout << "WaveFront GetOutput statistics: " << m_swGetOutput << std::endl;
		std::cout << "WaveFront VDL statistics: " << m_swVDL << std::endl;
		std::cout << "WaveFront IIR filter statistics: " << m_swIIRFilter << std::endl;
		std::cout << "WaveFront Interpolation statistics: " << m_swInterpolation << std::endl;
	}
#else
	VA_INFO( "CVABinauralOutdoorWaveFront", "SetParameters called, but this function does not do anything" );
#endif
}

void CVABinauralOutdoorWaveFront::SetFilterCoefficients( const ITABase::CThirdOctaveGainMagnitudeSpectrum& oMags )
{
	ITABase::CFiniteImpulseResponse oIR( oConf.iFIRFilterLength, float( oConf.dSampleRate ) ); // create an empty impulse response

	CITAThirdOctaveFIRFilterGenerator oIRGenerator( oConf.dSampleRate,
	                                                oConf.iFIRFilterLength ); // convert the given third octave frequency magnitudes to an impulse response

	if( oConf.iFilterDesignAlgorithm == ITADSP::BURG )
	{
		oIRGenerator.GenerateFilter( oMags, oIR.GetData( ), false ); // set to false for normal, true for minimum phase needed for yulewalk
		ITADSP::IIRFilterGenerator::Burg( oIR, m_iIIRFilterCoeffs ); // using the impulse response as input, calculate the IIR filter coefficients.
	}
	else
		VA_EXCEPT1( "Filter design algorithm not recognised / not yet implemented" );

	m_oIIRFilterEngine.SetCoefficients( m_iIIRFilterCoeffs ); // set coefficients of internal IIR filter
}

void CVABinauralOutdoorWaveFront::SetDelay( const double dDelaySeconds )
{
	unsigned int iDelaySamples = (unsigned int)round( dDelaySeconds * oConf.dSampleRate ); // unsigned as can not have negative delay (causality)
	SetDelaySamples( iDelaySamples );
}

void CVABinauralOutdoorWaveFront::SetDelaySamples( const int iDelaySamples )
{
	m_pSIMOVDL->SetDelaySamples( m_iCursorID, iDelaySamples );
}

void CVABinauralOutdoorWaveFront::SetGain( float fGain )
{
	m_fGainNew = fGain;
}

void CVABinauralOutdoorWaveFront::SetMotion( )
{
	ITA_EXCEPT_NOT_IMPLEMENTED;
}

double CVABinauralOutdoorWaveFront::GetLastHistoryTimestamp( ) const
{
	return m_oAudibilityHistory.GetLastTimestamp( );
}

void CVABinauralOutdoorWaveFront::PushAudibility( const double& dTimeStamp, bool bAudible )
{
	m_oAudibilityHistory.Push( dTimeStamp, std::make_unique<bool>( bAudible ) );
}

void CVABinauralOutdoorWaveFront::PushDelay( const double& dTimeStamp, const double& dDelay )
{
	m_oDelayHistory.Push( dTimeStamp, std::make_unique<double>( dDelay ) );
}

void CVABinauralOutdoorWaveFront::PushGain( const double& dTimeStamp, const double& dGain )
{
	m_oGainHistory.Push( dTimeStamp, std::make_unique<double>( dGain ) );
}

void CVABinauralOutdoorWaveFront::PushFilterCoefficients( const double& dTimeStamp, std::unique_ptr<ITABase::CThirdOctaveGainMagnitudeSpectrum> pFilterCoeffs )
{
	m_oThirdOctaveFilterHistory.Push( dTimeStamp, std::move( pFilterCoeffs ) );
}

void CVABinauralOutdoorWaveFront::PushWFOrigin( const double& dTimeStamp, std::unique_ptr<VAVec3> pv3WFAtReceiver )
{
	m_oReceiverWFOriginHistory.Push( dTimeStamp, std::move( pv3WFAtReceiver ) );
}

void CVABinauralOutdoorWaveFront::UpdateFilterPropertyHistories( const double& dTime )
{
	m_oAudibilityHistory.Update( dTime );
	m_oDelayHistory.Update( dTime );
	m_oGainHistory.Update( dTime );
	m_oThirdOctaveFilterHistory.Update( dTime );
	m_oReceiverWFOriginHistory.Update( dTime );

	double dDelay, dGain;
	VAVec3 v3RecWFNormal;
	m_bAudible = false;
	m_oAudibilityHistory.Estimate( dTime, m_bAudible );
	if( m_oDelayHistory.Estimate( dTime, dDelay ) )
		SetDelay( dDelay );
	if( m_oGainHistory.Estimate( dTime, dGain ) )
		m_fGainNew = dGain;
	if( m_oThirdOctaveFilterHistory.Estimate( dTime, m_oMags ) )
		SetFilterCoefficients( m_oMags );
	if( m_oReceiverWFOriginHistory.Estimate( dTime, v3RecWFNormal ) )
		SetWaveFrontOrigin( v3RecWFNormal );
}

void CVABinauralOutdoorWaveFront::SetSource( CVABinauralOutdoorSource* pSource )
{
	m_pSIMOVDL = pSource->pVDL; // The variable delay line from the source this path originated from

	m_iCursorID = m_pSIMOVDL->AddCursor( );

	m_pSource = pSource;
}

int CVABinauralOutdoorWaveFront::GetSourceID( ) const
{
	if( !m_pSource )
		return -1;
	return m_pSource->pData->iID;
}

void CVABinauralOutdoorWaveFront::SetReceiver( CVABinauralClusteringReceiver* receiver )
{
	m_pReceiver = receiver;
}

void CVABinauralOutdoorWaveFront::setITDDifference( const float fITDDiff )
{
	m_iITDDifferenceNew = (int)round( fITDDiff * oConf.dSampleRate ); // convert the ITD difference in seconds to samples
	if( abs( m_iITDDifferenceNew ) + oConf.iBlockLength > oConf.iBufferSize )
		VA_EXCEPT1( "ITD Difference set is too large, increase the maximum buffersize for the outdoor renderer in the config file" );
}

void CVABinauralOutdoorWaveFront::SetClusteringMetrics( const VAVec3& v3Pos, const VAVec3& v3View, const VAVec3& v3Up, const VAVec3& v3PrincipleDirectionOrigin )
{
	m_v3ClusteringPos                      = v3Pos;
	m_v3ClusteringView                     = v3View;
	m_v3ClusteringUp                       = v3Up;
	m_v3ClusteringPrincipleDirectionOrigin = v3PrincipleDirectionOrigin;

	m_bClusteringPoseSet = true;
}

void CVABinauralOutdoorWaveFront::SetAudible( bool bAudible )
{
	m_bAudible = bAudible;
}

bool CVABinauralOutdoorWaveFront::GetValidWaveFrontOrigin( ) const
{
	return m_bWaveFrontOriginSet;
}

bool CVABinauralOutdoorWaveFront::GetValidClusteringPose( ) const
{
	return m_bClusteringPoseSet;
}

void CVABinauralOutdoorWaveFront::SetWaveFrontOrigin( const VAVec3& v3Origin )
{
	m_v3WaveFrontOrigin   = v3Origin;
	m_bWaveFrontOriginSet = true;
}

VAVec3 CVABinauralOutdoorWaveFront::GetWaveFrontOrigin( ) const
{
	return m_v3WaveFrontOrigin;
}

int CVABinauralOutdoorWaveFront::AddReference( )
{
	return CVAPoolObject::AddReference( );
}

int CVABinauralOutdoorWaveFront::RemoveReference( )
{
	return CVAPoolObject::RemoveReference( );
}
