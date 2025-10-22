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

#include "VABinauralArtificialReverb.h"

#ifdef VACORE_WITH_RENDERER_BINAURAL_ARTIFICIAL_REVERB

// VA includes
#	include "../../../Motion/VAMotionModelBase.h"
#	include "../../../Motion/VASharedMotionModel.h"
#	include "../../../Scene/VAScene.h"
#	include "../../../Utils/VAUtils.h"
#	include "../../../VAAudiostreamTracker.h"
#	include "../../../VACoreConfig.h"
#	include "../../../VALog.h"
#	include "../../../core/core.h"
#	include "../../../directivities/VADirectivityDAFFEnergetic.h"
#	include "../../../directivities/VADirectivityDAFFHRIR.h"

#	include <VAObjectPool.h>
#	include <VAReferenceableObject.h>

// ITA includes
#	include <DAFF.h>
#	include <ITAClock.h>
#	include <ITAConfigUtils.h>
#	include <ITAConstants.h>
#	include <ITACriticalSection.h>
#	include <ITADataSourceRealization.h>
#	include <ITAException.h>
#	include <ITAFastMath.h>
#	include <ITASampleBuffer.h>
#	include <ITASampleFrame.h>
#	include <ITAStopWatch.h>
#	include <ITAStreamInfo.h>
#	include <ITAThirdOctaveFilterbank.h>
#	include <ITAUPConvolution.h>
#	include <ITAUPFilter.h>
#	include <ITAUPFilterPool.h>
#	include <ITAVariableDelayLine.h>

// Vista includes
#	include <VistaBase/VistaTimeUtils.h>
#	include <VistaInterProcComm/Concurrency/VistaThreadEvent.h>
#	include <VistaInterProcComm/Concurrency/VistaThreadLoop.h>
#	include <VistaTools/VistaRandomNumberGenerator.h>

// External includes
#	include <DspFilters/Dsp.h>
#	include <tbb/concurrent_queue.h>

// STL includes
#	include <algorithm>
#	include <atomic>
#	include <cassert>
#	include <iomanip>
#	include <set>
#	include <vector>

using namespace ITAConstants;

const double g_dMinReverberationTime = 0.25f;                 // Minimum allowed reverberation time [s]
const double g_dMinRoomVolume        = 3 * 2 * 1;             // Minimum allowed room volume [m^3]
const double g_dMinRoomSurfaceArea   = 2 * ( 3 * 2 + 3 + 2 ); // Minimum allowed room surface area [m^2]

//! Binaural artificial reverberation path (listener<->source)
class CBARPath : public CVAPoolObject
{
public:
	virtual ~CBARPath( );

	CVABinauralArtificialReverbAudioRenderer::Source* pSource;
	CVABinauralArtificialReverbAudioRenderer::Listener* pListener;

	CVASourceTargetMetrics oRelations; //!< Positions, orientations

	CITAVariableDelayLine* pVariableDelayLine; //!< DSP

	std::atomic<bool> bMarkedForDeletion;

	void PreRequest( )
	{
		pVariableDelayLine->Clear( );
		pSource   = nullptr;
		pListener = nullptr;
	};

	void PreRelease( ) { };

	void UpdateMetrics( );

private:
	CBARPath( );
	CBARPath( const double dSampleRate, const int iBlockLength );

	EVDLAlgorithm m_iDefaultVDLSwitchingAlgorithm;

	friend class CBARPathFactory;
};

//! Binaural artificial reverberation path factory class
class CBARPathFactory : public IVAPoolObjectFactory
{
public:
	inline CBARPathFactory( const double dSampleRate, const int iBlockLength ) : m_dSampleRate( dSampleRate ), m_iBlockLength( iBlockLength ) { };

	inline CVAPoolObject* CreatePoolObject( ) { return new CBARPath( m_dSampleRate, m_iBlockLength ); };

private:
	double m_dSampleRate;
	int m_iBlockLength;
};

//! Binaural artificial reverberation simulation class
/**
 * The simulation instance receives AR tasks and calculates
 * the impulse responses in an own thread. Task assignment is non-blocking,
 * the actual simulations are performed in the thread loop. Multiple
 * tasks per listener will be merged into one calculation. Calculation
 * has to be triggered via the public event member.
 */
class CBARSimulator : public VistaThreadLoop
{
public:
	inline CBARSimulator( CVABinauralArtificialReverbAudioRenderer* pParent ) : m_pParentRenderer( pParent )
	{
		Run( );
		m_iState = 0; // idle
	};

	inline ~CBARSimulator( ) {
		// StopGently( true );
	};

	inline bool LoopBody( )
	{
		evStartSimulationTrigger.WaitForEvent( true );

		CalculateSimulationTasks( );

		return true;
	};

	inline void AddARUpdateTask( CVABinauralArtificialReverbAudioRenderer::Listener* pL )
	{
		// Decline during reset
		if( m_iState > 1 )
			return;

		pL->AddReference( );
		m_lTasks.push( pL );
	};

	inline void CalculateSimulationTasks( )
	{
		CVABinauralArtificialReverbAudioRenderer::Listener* pListener( NULL );

		while( m_lTasks.try_pop( pListener ) )
		{
			if( ( pListener->bARFilterUpdateRequired ) && ( m_iState != 2 ) )
			{
				m_pParentRenderer->UpdateArtificialReverbFilter( pListener ); // blocking simulation call
				pListener->vLastARUpdatePos        = pListener->vPredPos;
				pListener->vLastARUpdateView       = pListener->vPredView;
				pListener->vLastARUpdateUp         = pListener->vPredUp;
				pListener->bARFilterUpdateRequired = false;
			}
			pListener->RemoveReference( );
		}

		if( m_iState == 2 )
			m_iState = 3; // reset ack
	};

	inline void Reset( )
	{
		m_iState = 2; // reset request

		evStartSimulationTrigger.SignalEvent( );

		// Wait for reset ack
		while( m_iState != 3 )
			VistaTimeUtils::Sleep( 100 );

		m_iState = 0; // idle
	};

	VistaThreadEvent evStartSimulationTrigger; // Trigger update loop

private:
	tbb::concurrent_queue<CVABinauralArtificialReverbAudioRenderer::Listener*> m_lTasks;
	CVABinauralArtificialReverbAudioRenderer* m_pParentRenderer;
	std::atomic<int> m_iState;
};

const int iOrder    = 10;
const int iChannels = 1;
int iFilterLength   = (int)pow( 2, 12 );

class CLowPassFilter : public CVABinauralArtificialReverbAudioRenderer::CFilterBase
{
public:
	inline CLowPassFilter( float fLowerFrequencyCuttoff, float fSampleRate ) { oFilterEngine.setup( iOrder, fSampleRate, fLowerFrequencyCuttoff ); };

	inline void Process( int iNumSamples, float* pfInOut, bool bAutoReset = true )
	{
		if( bAutoReset )
			oFilterEngine.reset( );

		oFilterEngine.process( iNumSamples, &pfInOut );
	};

	Dsp::SimpleFilter<Dsp::Butterworth::LowPass<iOrder>, iChannels> oFilterEngine;
};

class CHighPassFilter : public CVABinauralArtificialReverbAudioRenderer::CFilterBase
{
public:
	inline CHighPassFilter( float fLowerFrequencyCuttoff, float fSampleRate ) { oFilterEngine.setup( iOrder, fSampleRate, fLowerFrequencyCuttoff ); };

	inline void Process( int iNumSamples, float* pfInOut, bool bAutoReset = true )
	{
		if( bAutoReset )
			oFilterEngine.reset( );

		oFilterEngine.process( iNumSamples, &pfInOut );
	};

	Dsp::SimpleFilter<Dsp::Butterworth::HighPass<iOrder>, iChannels> oFilterEngine;
};

class CBandPassFilter : public CVABinauralArtificialReverbAudioRenderer::CFilterBase
{
public:
	inline CBandPassFilter( float fCenterFrequency, float fBandwidth, float fSampleRate ) { oFilterEngine.setup( iOrder, fSampleRate, fCenterFrequency, fBandwidth ); };

	inline void Process( int iNumSamples, float* pfInOut, bool bAutoReset = true )
	{
		if( bAutoReset )
			oFilterEngine.reset( );

		oFilterEngine.process( iNumSamples, &pfInOut );
	};

	Dsp::SimpleFilter<Dsp::Butterworth::BandPass<iOrder>, iChannels> oFilterEngine;
};


// --= Renderer =--

CVABinauralArtificialReverbAudioRenderer::CVABinauralArtificialReverbAudioRenderer( const CVAAudioRendererInitParams& oParams )
    : ITADatasourceRealization( 2, oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.dSampleRate, oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.iBuffersize )
    , CVAObject( oParams.sClass + ":" + oParams.sID )
    , m_oParams( oParams )
    , m_pCore( oParams.pCore )
    , m_pCurSceneState( NULL )
    , m_pListenerPool( IVAObjectPool::Create( 2, 2, new CVAPoolObjectDefaultFactory<Listener>, true ) )
    , m_pSourcePool( IVAObjectPool::Create( 30, 2, new CVAPoolObjectDefaultFactory<Source>, true ) )
    , m_pARSimulator( NULL )
    , m_bForceARUpdateOnce( false )
{
	Init( *oParams.pConfig );

	m_pCore->RegisterModule( this );

	m_pARSimulator = new CBARSimulator( this );

	m_pARPathFactory = new CBARPathFactory( GetSampleRate( ), GetBlocklength( ) );

	m_pARPathPool = IVAObjectPool::Create( 64, 8, m_pARPathFactory, true );

	ctxAudio.m_sbTempBuf1.Init( GetBlocklength( ), false );
	ctxAudio.m_iResetFlag = 0; // Normal operation mode
	ctxAudio.m_iStatus    = 0; // Stopped

	m_iCurGlobalAuralizationMode = IVAInterface::VA_AURAMODE_DIFFUSE_DECAY | IVAInterface::VA_AURAMODE_DOPPLER;
}

CVABinauralArtificialReverbAudioRenderer::~CVABinauralArtificialReverbAudioRenderer( )
{
	delete m_pARPathPool; // Also deletes factory
	delete m_pARSimulator;
}

void CVABinauralArtificialReverbAudioRenderer::Init( const CVAStruct& oArgs )
{
	CVAConfigInterpreter conf( oArgs );

	std::string sReverberationTimes;
	conf.OptString( "ReverberationTimes", sReverberationTimes );

	m_vdReverberationTimes = StringToDoubleVec( sReverberationTimes );

	std::string sReverberationTime;
	conf.OptString( "ReverberationTime", sReverberationTime );

	if( sReverberationTimes.empty( ) && !sReverberationTime.empty( ) )
	{
		VA_WARN( "BinauralArtificialReverbAudioRenderer",
		         "Deprecated ReverberationTime used in configuration, please switch to ReverberationTimes and provide a list of e.g. 3 values (low, mid, high). "
		         "Approximating reverb." );
		double dReverberationTime = StringToDouble( sReverberationTime );

		m_vdReverberationTimes = { dReverberationTime * sqrt( 2 ), dReverberationTime, dReverberationTime / sqrt( 2 ) };
	}
	else
	{
		if( !( m_vdReverberationTimes.size( ) == 3 || m_vdReverberationTimes.size( ) == 8 ) )
			ITA_EXCEPT_INVALID_PARAMETER( "ReverberationTimes: please provide 3 (low, mid high) or 8 octave band values from 63Hz to 8kHz" );
	}

	DesignEQFilters( );

	conf.OptNumber( "RoomVolume", m_dRoomVolume, 10.0f * 10.0f * 2.0f );
	conf.OptNumber( "RoomSurfaceArea", m_dRoomSurfaceArea, 4 * ( 10.0f * 2.0f ) + 2 * ( 2.0f * 2.0f ) );
	conf.OptNumber( "SoundPowerCorrectionFactor", m_dSoundPowerCorrectionFactor, 0.05 );

	const std::string sFIRLengthKey = oArgs.HasKey("MaxFilterLengthSamples") ? "MaxFilterLengthSamples" : "MaxReverbFilterLengthSamples"; //Backwards compatibility
	conf.OptInteger( sFIRLengthKey, m_iMaxReverbFilterLengthSamples, int( 2 * GetSampleRate( ) ) );

	conf.OptNumber( "PositionThreshold", m_dPosThreshold, 1.0f );
	conf.OptNumber( "AngleThresholdDegree", m_dAngleThreshold, 30.0f );

	conf.OptNumber( "TimeSlotResolution", m_dTimeSlotResolution, 0.005f );
	conf.OptNumber( "MaxReflectionDensity", m_dMaxReflectionDensity, 12000.0f );
	conf.OptNumber( "ScatteringCoefficient", m_dScatteringCoefficient, 0.1f );

	std::string sVLDInterpolationAlgorithm;
	conf.OptString( "VDLSwitchingAlgorithm", sVLDInterpolationAlgorithm, "linear" );
	sVLDInterpolationAlgorithm = toLowercase( sVLDInterpolationAlgorithm );

	if( sVLDInterpolationAlgorithm == "switch" )
		m_iDefaultVDLSwitchingAlgorithm = EVDLAlgorithm::SWITCH;
	else if( sVLDInterpolationAlgorithm == "crossfade" )
		m_iDefaultVDLSwitchingAlgorithm = EVDLAlgorithm::CROSSFADE;
	else if( sVLDInterpolationAlgorithm == "linear" )
		m_iDefaultVDLSwitchingAlgorithm = EVDLAlgorithm::LINEAR_INTERPOLATION;
	else if( sVLDInterpolationAlgorithm == "cubicspline" )
		m_iDefaultVDLSwitchingAlgorithm = EVDLAlgorithm::CUBIC_SPLINE_INTERPOLATION;
	else if( sVLDInterpolationAlgorithm == "windowedsinc" )
		m_iDefaultVDLSwitchingAlgorithm = EVDLAlgorithm::WINDOWED_SINC_INTERPOLATION;
	else
		ITA_EXCEPT1( INVALID_PARAMETER, "Unrecognized interpolation algorithm '" + sVLDInterpolationAlgorithm + "' in BinauralFreefieldAudioRendererConfig" );

	for( size_t i = 0; i < m_vdReverberationTimes.size( ); i++ )
	{
		auto dReverberationTime = m_vdReverberationTimes[i];
		if( dReverberationTime < g_dMinReverberationTime )
		{
			VA_WARN( "BinauralArtificialReverbAudioRenderer",
			         "Requested reverberation time of " << dReverberationTime << " s too small, minimum is " << g_dMinReverberationTime << " s." );
			m_vdReverberationTimes[i] = g_dMinReverberationTime;
		}
	}

	if( m_dRoomSurfaceArea < g_dMinRoomSurfaceArea )
	{
		VA_WARN( "BinauralArtificialReverbAudioRenderer",
		         "Requested room surface of " << m_dRoomSurfaceArea << " m^2 too small, minimum is " << g_dMinRoomSurfaceArea << " m^2." );
		m_dRoomSurfaceArea = g_dMinRoomSurfaceArea;
	}

	if( m_dRoomVolume < g_dMinRoomVolume )
	{
		VA_WARN( "BinauralArtificialReverbAudioRenderer", "Requested room volume of " << m_dRoomVolume << " m^3 too small, minimum is " << g_dMinRoomVolume << " m^3." );
		m_dRoomVolume = g_dMinRoomVolume;
	}

	return;
}

void CVABinauralArtificialReverbAudioRenderer::DesignEQFilters( )
{
	m_mxFilterDesign.lock( );

	if( m_vdReverberationTimes.size( ) == 3 )
	{
		m_vpFilter.clear( );
		m_vpFilter.push_back( std::make_shared<CLowPassFilter>( 330.0f, GetSampleRate( ) ) );
		m_vpFilter.push_back( std::make_shared<CBandPassFilter>( 2300.0f, 4000.0f, GetSampleRate( ) ) );
		m_vpFilter.push_back( std::make_shared<CHighPassFilter>( 3900.0f, GetSampleRate( ) ) );
	}
	else if( m_vdReverberationTimes.size( ) == 8 )
	{
		m_vpFilter.clear( );
		float fLowPassFrequencyCuttoff = OCTAVE_CENTER_FREQUENCIES_ISO_F[0] * sqrt( 2.0f );
		m_vpFilter.push_back( std::make_shared<CLowPassFilter>( fLowPassFrequencyCuttoff, GetSampleRate( ) ) );

		for( size_t i = 1; i < OCTAVE_CENTER_FREQUENCIES_ISO_F.size( ) - 1; i++ )
		{
			float fMidCenterFrequency = OCTAVE_CENTER_FREQUENCIES_ISO_F[i];
			float fMidBandWidth       = ( OCTAVE_CENTER_FREQUENCIES_ISO_F[i + 1] - OCTAVE_CENTER_FREQUENCIES_ISO_F[i - 1] ) / 2.15f;
			m_vpFilter.push_back( std::make_shared<CBandPassFilter>( fMidCenterFrequency, fMidBandWidth, GetSampleRate( ) ) );
		}

		float fHighPassFrequencyCuttoff = OCTAVE_CENTER_FREQUENCIES_ISO_F[0] / sqrt( 2.0f );
		m_vpFilter.push_back( std::make_shared<CHighPassFilter>( fHighPassFrequencyCuttoff, GetSampleRate( ) ) );
	}
	else
	{
		VA_ERROR( "BinauralArtificialReverbAudioRenderer",
		          "Current implementation only accepts 3 reverb times for low, mid & high frequencies or octave resolution (8 values from 63Hz to 8kHz)" );
	}
	m_mxFilterDesign.unlock( );
}

CVAObjectInfo CVABinauralArtificialReverbAudioRenderer::GetObjectInfo( ) const
{
	CVAObjectInfo oInfo;
	oInfo.iID   = CVAObject::GetObjectID( );
	oInfo.sName = CVAObject::GetObjectName( );
	oInfo.sDesc = "Binaural Artificial Reverberation Audio Renderer";

	return oInfo;
}

CVAStruct CVABinauralArtificialReverbAudioRenderer::CallObject( const CVAStruct& oArgs )
{
	CVAStruct oReturn;

	const CVAStructValue* pStruct;

	if( ( pStruct = oArgs.GetValue( "PRINT" ) ) != nullptr )
	{
		if( pStruct->GetDatatype( ) != CVAStructValue::STRING )
			VA_EXCEPT2( INVALID_PARAMETER, "Print value must be a string" );
		std::string sValue = toUppercase( *pStruct );
		if( sValue == "HELP" )
		{
			VA_PRINT( "Available '" << GetObjectName( ) << "' commands: 'print' 'help|status'" );
			return oReturn;
		}

		if( sValue == "STATUS" )
		{
			VA_PRINT( "RoomReverberationTimes: " << DoubleVecToString( m_vdReverberationTimes ) << " seconds" );
			VA_PRINT( "MaxReverbFilterLengthSamples: " << m_iMaxReverbFilterLengthSamples << " samples (" << m_iMaxReverbFilterLengthSamples / GetSampleRate( )
			                                           << " seconds)" );
			VA_PRINT( "RoomVolume: " << m_dRoomVolume << " m^3" );
			VA_PRINT( "RoomSurfaceArea: " << m_dRoomSurfaceArea << " m^2" );
			VA_PRINT( "SoundPowerCorrectionFactor: " << m_dSoundPowerCorrectionFactor << " (linear factor)" );
			return oReturn;
		}
	}
	else if( ( pStruct = oArgs.GetValue( "SET" ) ) != nullptr )
	{
		if( pStruct->GetDatatype( ) != CVAStructValue::STRING )
			VA_EXCEPT2( INVALID_PARAMETER, "Setter key name must be a string" );

		std::string sValue = toUppercase( *pStruct );
		// VA_TRACE( GetObjectID(), "Receiving parameter, updating '" << sValue << "'" << ", value: " << *pStruct );

		if( sValue == "ROOMREVERBERATIONTIME" )
		{
			pStruct = oArgs.GetValue( "VALUE" );
			if( ( pStruct->GetDatatype( ) == CVAStructValue::DOUBLE ) || ( pStruct->GetDatatype( ) == CVAStructValue::INT ) ||
			    ( pStruct->GetDatatype( ) == CVAStructValue::BOOL ) )
			{
				double dRoomReverberationTime = *pStruct;

				if( m_vdReverberationTimes.size( ) == 3 )
				{
					m_vdReverberationTimes[0] = sqrt( 2 ) * dRoomReverberationTime;
					m_vdReverberationTimes[1] = dRoomReverberationTime;
					m_vdReverberationTimes[2] = dRoomReverberationTime / sqrt( 2 );
				}
				else if( m_vdReverberationTimes.size( ) == 8 )
				{
					for( size_t i = 0; i < m_vdReverberationTimes.size( ); i++ )
						m_vdReverberationTimes[i] = dRoomReverberationTime * 2 * ( 8 - i ) / 4.0f;
				}
				else
				{
					VA_ERROR( "BinauralArtificialReverbAudioRenderer",
					          "Found an odd number of reverberation times ... num = " + IntToString( (int)m_vdReverberationTimes.size( ) ) );
				}

				for( size_t i = 0; i < m_vdReverberationTimes.size( ); i++ )
					if( m_vdReverberationTimes[i] < g_dMinReverberationTime )
					{
						VA_WARN( "BinauralArtificialReverbAudioRenderer",
						         "Requested reverberation time of " << dRoomReverberationTime << "s too small, minimum is " << g_dMinReverberationTime << "s." );
						m_vdReverberationTimes[i] = g_dMinReverberationTime;
					}
				VA_INFO( "BinauralArtificialReverbAudioRenderer", "Set reverberation time to " << dRoomReverberationTime );


				if( dRoomReverberationTime * GetSampleRate( ) > m_iMaxReverbFilterLengthSamples )
					VA_WARN( "BinauralArtificialReverbAudioRenderer", "Reverberation time greater than target filter length, reverb will be truncated at "
					                                                      << m_iMaxReverbFilterLengthSamples / GetSampleRate( ) << " seconds" );
			}

			m_bForceARUpdateOnce = true;
			DesignEQFilters( );
			UpdateArtificialReverbPaths( );

			return oReturn;
		}
		else if( sValue == "ROOMVOLUME" )
		{
			pStruct = oArgs.GetValue( "VALUE" );
			if( ( pStruct->GetDatatype( ) == CVAStructValue::DOUBLE ) || ( pStruct->GetDatatype( ) == CVAStructValue::INT ) ||
			    ( pStruct->GetDatatype( ) == CVAStructValue::BOOL ) )
			{
				double dRoomVolume = *pStruct;
				if( dRoomVolume < g_dMinRoomVolume )
				{
					VA_WARN( "BinauralArtificialReverbAudioRenderer",
					         "Requested room volume of " << dRoomVolume << " m^3 too small, minimum is " << g_dMinRoomVolume << " m^3." );
					m_dRoomVolume = g_dMinRoomVolume;
				}
				else
				{
					m_dRoomVolume = dRoomVolume;
				}
				VA_INFO( "BinauralArtificialReverbAudioRenderer", "Set room volume to " << m_dRoomVolume << " m^3" );
			}

			m_bForceARUpdateOnce = true;
			UpdateArtificialReverbPaths( );

			return oReturn;
		}
		else if( sValue == "ROOMSURFACEAREA" )
		{
			pStruct = oArgs.GetValue( "VALUE" );
			if( ( pStruct->GetDatatype( ) == CVAStructValue::DOUBLE ) || ( pStruct->GetDatatype( ) == CVAStructValue::INT ) ||
			    ( pStruct->GetDatatype( ) == CVAStructValue::BOOL ) )
			{
				double dRoomSurfaceArea = *pStruct;
				if( dRoomSurfaceArea < g_dMinRoomSurfaceArea )
				{
					VA_WARN( "BinauralArtificialReverbAudioRenderer",
					         "Requested room surface area of " << dRoomSurfaceArea << " m^2 too small, setting to minimum of " << g_dMinRoomSurfaceArea << " m^2." );
					m_dRoomVolume = g_dMinRoomVolume;
				}
				else
				{
					m_dRoomSurfaceArea = dRoomSurfaceArea;
				}
				VA_INFO( "BinauralArtificialReverbAudioRenderer", "Set room surface area to " << m_dRoomSurfaceArea << " m^2" );
			}

			m_bForceARUpdateOnce = true;
			UpdateArtificialReverbPaths( );

			return oReturn;
		}
		else if( sValue == "SOUNDPOWERCORRECTIONFACTOR" )
		{
			pStruct = oArgs.GetValue( "VALUE" );
			if( ( pStruct->GetDatatype( ) == CVAStructValue::DOUBLE ) || ( pStruct->GetDatatype( ) == CVAStructValue::INT ) ||
			    ( pStruct->GetDatatype( ) == CVAStructValue::BOOL ) )
			{
				double dSoundPowerCorrectionFactor = *pStruct;
				if( dSoundPowerCorrectionFactor > 0 )
				{
					m_dSoundPowerCorrectionFactor = dSoundPowerCorrectionFactor;
				}
				else
				{
					ITA_EXCEPT1( INVALID_PARAMETER, "Sound power correction must be positive" )
				}
				VA_INFO( "BinauralArtificialReverbAudioRenderer", "Set sound power correction to " << m_dSoundPowerCorrectionFactor );
			}

			m_bForceARUpdateOnce = true;
			UpdateArtificialReverbPaths( );

			return oReturn;
		}
	}
	else if( ( pStruct = oArgs.GetValue( "GET" ) ) != nullptr )
	{
		if( pStruct->GetDatatype( ) != CVAStructValue::STRING )
			VA_EXCEPT2( INVALID_PARAMETER, "Setter key name must be a string" );

		std::string sValue = toUppercase( *pStruct );
		if( sValue == "ROOMREVERBERATIONTIME" )
			oReturn["Return"] = m_vdReverberationTimes[0];
		else if( sValue == "ROOMREVERBERATIONTIMES" )
			oReturn["Return"] = DoubleVecToString( m_vdReverberationTimes );
		else if( sValue == "ROOMVOLUME" )
			oReturn["Return"] = m_dRoomVolume;
		else if( sValue == "ROOMSURFACEAREA" )
			oReturn["Return"] = m_dRoomSurfaceArea;
		else if( sValue == "MAXROOMREVERBERATIONTIME" )
			oReturn["Return"] = double( m_iMaxReverbFilterLengthSamples / GetSampleRate( ) );
		else if( sValue == "MAXREVERBFILTERLENGTHSAMPLES" )
			oReturn["Return"] = m_iMaxReverbFilterLengthSamples;
		else if( sValue == "SOUNDPOWERCORRECTIONFACTOR" )
			oReturn["Return"] = m_dSoundPowerCorrectionFactor;
		else
			VA_EXCEPT2( INVALID_PARAMETER, "Could not interpret object call getter '" + sValue + "'" );

		return oReturn;
	}
	else
	{
		VA_EXCEPT2( INVALID_PARAMETER, "Could not interpret object call in '" + GetObjectName( ) + "'" );
	}

	return oReturn;
}

void CVABinauralArtificialReverbAudioRenderer::Reset( )
{
	ctxAudio.m_iResetFlag = 1; // Request reset

	if( ctxAudio.m_iStatus == 0 || m_oParams.bOfflineRendering )
	{
		// if no streaming active, reset manually
		ResetInternalData( );
	}

	// Wait for last streaming block before internal reset
	while( ctxAudio.m_iResetFlag != 2 )
	{
		VASleep( 10 ); // Wait for acknowledge
	}

	// Iterate over sound pathes and free items
	std::list<CBARPath*>::iterator it = m_lARPaths.begin( );
	while( it != m_lARPaths.end( ) )
	{
		CBARPath* pPath( *it );

		assert( pPath->GetNumReferences( ) == 1 );
		pPath->RemoveReference( );

		++it;
	}
	m_lARPaths.clear( );

	m_pARSimulator->Reset( );

	// Iterate over listener and free items
	std::map<int, Listener*>::const_iterator lcit = m_mListener.begin( );
	while( lcit != m_mListener.end( ) )
	{
		Listener* pListener( lcit->second );
		pListener->pData->RemoveReference( );
		assert( pListener->GetNumReferences( ) == 1 );
		pListener->RemoveReference( );
		lcit++;
	}
	m_mListener.clear( );

	// Iterate over sources and free items
	std::map<int, Source*>::const_iterator scit = m_mSources.begin( );
	while( scit != m_mSources.end( ) )
	{
		Source* pSource( scit->second );
		pSource->pData->RemoveReference( );
		assert( pSource->GetNumReferences( ) == 1 );
		pSource->RemoveReference( );
		scit++;
	}
	m_mSources.clear( );

	if( m_pCurSceneState )
	{
		m_pCurSceneState->RemoveReference( );
		m_pCurSceneState = nullptr;
	}

	ctxAudio.m_iResetFlag = 0; // Enter normal mode
}

void CVABinauralArtificialReverbAudioRenderer::UpdateScene( CVASceneState* pNewSceneState )
{
	m_pNewSceneState = pNewSceneState;
	if( m_pNewSceneState == m_pCurSceneState )
		return;

	m_pNewSceneState->AddReference( );

	CVASceneStateDiff oDiff;
	pNewSceneState->Diff( m_pCurSceneState, &oDiff );

	ManageArtificialReverbPaths( &oDiff );

	UpdateTrajectories( );

	UpdateArtificialReverbPaths( false ); // Unforced update validation

	if( m_pCurSceneState )
		m_pCurSceneState->RemoveReference( );

	m_pCurSceneState = m_pNewSceneState;
	m_pNewSceneState = NULL;

	return;
}

ITADatasource* CVABinauralArtificialReverbAudioRenderer::GetOutputDatasource( )
{
	return this;
}

CVAStruct CVABinauralArtificialReverbAudioRenderer::GetParameters( const CVAStruct& oInArgs ) const
{
	CVAStruct oRet;

	std::vector<float> vf;
	for( auto d: m_vdReverberationTimes )
		vf.push_back( (float)d );
	oRet["room_reverberation_times"] = CVAStructValue( (void*)&vf[0], vf.size( ) * sizeof( float ) );

	return oRet;
}

void CVABinauralArtificialReverbAudioRenderer::SetParameters( const CVAStruct& oInArgs )
{
	if( oInArgs.HasKey( "room_reverberation_times" ) )
	{
		const auto& oReverbTimes( oInArgs["room_reverberation_times"] );
		if( oReverbTimes.IsDouble( ) )
		{
			double dReverbTime = oReverbTimes;

			if( dReverbTime < g_dMinReverberationTime )
			{
				VA_WARN( "BinauralArtificialReverbAudioRenderer",
				         "Requested reverberation time of " << dReverbTime << " s too small, minimum is " << g_dMinReverberationTime << " s." );
				dReverbTime = g_dMinReverberationTime;
			}

			m_vdReverberationTimes = { dReverbTime * sqrt( 2 ), dReverbTime, dReverbTime / sqrt( 2 ) };
			DesignEQFilters( );
			UpdateArtificialReverbPaths( true );
		}
		else if( oReverbTimes.IsData( ) )
		{
			int iNumReverbTimes = int( oReverbTimes.GetDataSize( ) / sizeof( float ) );
			if( iNumReverbTimes == 3 || iNumReverbTimes == 8 )
			{
				m_vdReverberationTimes.resize( iNumReverbTimes );
				const auto pfReverbTimes = (const float*)oReverbTimes.GetData( );
				for( size_t i = 0; i < iNumReverbTimes; i++ )
					if( pfReverbTimes[i] < g_dMinReverberationTime )
					{
						VA_WARN( "BinauralArtificialReverbAudioRenderer",
						         "Requested reverberation time of " << pfReverbTimes[i] << " s too small, minimum is " << g_dMinReverberationTime << " s." );
						m_vdReverberationTimes[i] = g_dMinReverberationTime;
					}
					else
						m_vdReverberationTimes[i] = pfReverbTimes[i];
			}
			else
				VA_EXCEPT2( INVALID_PARAMETER, "Reverberation times must containt 1, 3 (low, mid, high) or 8 (octave) values" );

			DesignEQFilters( );
			UpdateArtificialReverbPaths( true );
		}
	}
}

void CVABinauralArtificialReverbAudioRenderer::ManageArtificialReverbPaths( const CVASceneStateDiff* pDiff )
{
	// �ber aktuelle Pfade iterieren und gel�schte markieren
	std::list<CBARPath*>::iterator it = m_lARPaths.begin( );
	while( it != m_lARPaths.end( ) )
	{
		int iSourceID   = ( *it )->pSource->pData->iID;
		int iListenerID = ( *it )->pListener->pData->iID;
		bool bDelete    = false;

		// Schallquelle gel�scht? (Quellen-ID in L�schliste)
		for( std::vector<int>::const_iterator cit = pDiff->viDelSoundSourceIDs.begin( ); cit != pDiff->viDelSoundSourceIDs.end( ); ++cit )
		{
			if( iSourceID == ( *cit ) )
			{
				bDelete = true; // Pfad zum L�schen markieren
				break;
			}
		}

		if( !bDelete )
		{
			// H�rer gel�scht? (H�rer-ID in L�schliste)
			for( std::vector<int>::const_iterator cit = pDiff->viDelReceiverIDs.begin( ); cit != pDiff->viDelReceiverIDs.end( ); ++cit )
			{
				if( iListenerID == ( *cit ) )
				{
					bDelete = true; // Pfad zum L�schen markieren
					break;
				}
			}
		}

		if( bDelete )
		{
			// Remove only required, if not skipped by this renderer
			const CVASoundSourceDesc* pSoundSourceDesc = m_pCore->GetSceneManager( )->GetSoundSourceDesc( iSourceID );
			if( pSoundSourceDesc->sExplicitRendererID.empty( ) || pSoundSourceDesc->sExplicitRendererID == m_oParams.sID )
			{
				DeleteArtificialReverbPath( *it );
				it = m_lARPaths.erase( it );
			}
			else
			{
				++it;
			}
		}
		else
		{
			++it;
		}
	}

	// �ber aktuelle Quellen und H�rer iterieren und gel�schte entfernen
	for( std::vector<int>::const_iterator cit = pDiff->viDelSoundSourceIDs.begin( ); cit != pDiff->viDelSoundSourceIDs.end( ); ++cit )
	{
		DeleteSource( *cit );
	}

	for( std::vector<int>::const_iterator cit = pDiff->viDelReceiverIDs.begin( ); cit != pDiff->viDelReceiverIDs.end( ); ++cit )
	{
		DeleteListener( *cit );
	}

	// Neue Pfade anlegen: ( neue H�rer ) verkn�pfen mit ( aktuellen Quellen + neuen Quellen )
	for( std::vector<int>::const_iterator lcit = pDiff->viNewReceiverIDs.begin( ); lcit != pDiff->viNewReceiverIDs.end( ); ++lcit )
	{
		const int iListenerID = ( *lcit );

		// Neuen H�rer erstellen
		Listener* pListener = CreateListener( iListenerID );

		// Aktuelle Quellen
		for( int i = 0; i < (int)pDiff->viComSoundSourceIDs.size( ); i++ )
		{
			// Neuen Pfad erzeugen zu aktueller Quelle, falls nicht gel�scht
			int iSourceID   = pDiff->viComSoundSourceIDs[i];
			Source* pSource = m_mSources[iSourceID];

			// Only add, if no other renderer has been connected explicitly with this source
			const CVASoundSourceDesc* pSoundSourceDesc = m_pCore->GetSceneManager( )->GetSoundSourceDesc( iSourceID );
			if( !pSoundSourceDesc->sExplicitRendererID.empty( ) && pSoundSourceDesc->sExplicitRendererID != m_oParams.sID )
				continue;

			if( !pSource->bDeleted )
				CreateArtificialReverbPath( pSource, pListener );
		}

		// Neue Quellen
		for( int i = 0; i < (int)pDiff->viNewSoundSourceIDs.size( ); i++ )
		{
			// Neuen Pfad erzeugen zu neuer Quelle
			int iSourceID = pDiff->viNewSoundSourceIDs[i];

			// Only add, if no other renderer has been connected explicitly with this source
			const CVASoundSourceDesc* pSoundSourceDesc = m_pCore->GetSceneManager( )->GetSoundSourceDesc( iSourceID );
			if( !pSoundSourceDesc->sExplicitRendererID.empty( ) && pSoundSourceDesc->sExplicitRendererID != m_oParams.sID )
				continue;

			Source* pSource = CreateSource( iSourceID );
			CreateArtificialReverbPath( pSource, pListener );
		}
	}

	// Neue Pfade anlegen: ( neue Quellen ) verkn�pfen mit ( aktuellen H�rern )
	for( std::vector<int>::const_iterator scit = pDiff->viNewSoundSourceIDs.begin( ); scit != pDiff->viNewSoundSourceIDs.end( ); ++scit )
	{
		const int iSourceID = ( *scit );

		// Only add, if no other renderer has been connected explicitly with this source
		const CVASoundSourceDesc* pSoundSourceDesc = m_pCore->GetSceneManager( )->GetSoundSourceDesc( iSourceID );
		if( !pSoundSourceDesc->sExplicitRendererID.empty( ) && pSoundSourceDesc->sExplicitRendererID != m_oParams.sID )
			continue;

		Source* pSource = CreateSource( iSourceID );

		// Aktuelle H�rer
		for( int i = 0; i < (int)pDiff->viComReceiverIDs.size( ); i++ )
		{
			// Neuen Pfad erzeugen zu neuer Quelle
			int iListenerID     = pDiff->viComReceiverIDs[i];
			Listener* pListener = m_mListener[iListenerID];
			CreateArtificialReverbPath( pSource, pListener );
		}
	}

	return;
}

void CVABinauralArtificialReverbAudioRenderer::ProcessStream( const ITAStreamInfo* pStreamInfo )
{
	// If streaming is active, set
	ctxAudio.m_iStatus = 1;

	// Swap forced update and check later if another process requests another forced update
	bool bForceARUpdateOnceLocalCopy = m_bForceARUpdateOnce;
	m_bForceARUpdateOnce             = false;

	SyncInternalData( );

	float* pfOutputCh0 = GetWritePointer( 0 );
	float* pfOutputCh1 = GetWritePointer( 1 );

	fm_zero( pfOutputCh0, GetBlocklength( ) );
	fm_zero( pfOutputCh1, GetBlocklength( ) );

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

	SampleTrajectoriesInternal( dListenerTime );

	// Update artificial reverb paths
	std::list<CBARPath*>::iterator apit = ctxAudio.m_lARPaths.begin( );
	while( apit != ctxAudio.m_lARPaths.end( ) )
	{
		CBARPath* pPath( *apit );

		CVAReceiverState* pListenerState  = ( m_pCurSceneState ? m_pCurSceneState->GetReceiverState( pPath->pListener->pData->iID ) : NULL );
		CVASoundSourceState* pSourceState = ( m_pCurSceneState ? m_pCurSceneState->GetSoundSourceState( pPath->pSource->pData->iID ) : NULL );

		if( pListenerState == nullptr || pSourceState == nullptr )
		{
			// Skip if no data is present
			apit++;
			continue;
		}

		// Positions, orientations, distances
		pPath->UpdateMetrics( );

		// VDL delay
		assert( m_pCore->oHomogeneousMedium.dSoundSpeed > 0 );
		double dDelay = pPath->oRelations.dDistance / m_pCore->oHomogeneousMedium.dSoundSpeed;
		pPath->pVariableDelayLine->SetDelayTime( float( dDelay ) );

		// VDL Doppler shift
		bool bDPEnabledGlobal   = ( m_iCurGlobalAuralizationMode & IVAInterface::VA_AURAMODE_DOPPLER ) > 0;
		bool bDPEnabledListener = ( pListenerState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_DOPPLER ) > 0;
		bool bDPEnabledSource   = ( pSourceState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_DOPPLER ) > 0;
		bool bDPEnabledCurrent  = ( pPath->pVariableDelayLine->GetAlgorithm( ) != EVDLAlgorithm::SWITCH ); // switch = disabled
		bool bDPStatusChanged   = ( bDPEnabledCurrent != ( bDPEnabledGlobal && bDPEnabledListener && bDPEnabledSource ) );
		if( bDPStatusChanged )
			pPath->pVariableDelayLine->SetAlgorithm( !bDPEnabledCurrent ? m_iDefaultVDLSwitchingAlgorithm : EVDLAlgorithm::SWITCH );

		// Artificial reverb (don't trigger AR update, but mark for update)

		Listener* pListener = pPath->pListener;
		VAVec3 vDiffPos     = pListener->vPredPos - pListener->vLastARUpdatePos;
		if( vDiffPos.Length( ) > m_dPosThreshold )
			pListener->bARFilterUpdateRequired = true;

		double dDotProductResult1 = pListener->vPredView.Dot( pListener->vLastARUpdateView );
		double dDotProductResult2 = pListener->vPredUp.Dot( pListener->vLastARUpdateUp );
		if( ( acos( dDotProductResult1 ) > m_dAngleThreshold * ITAConstants::PI_F / 180.0f ) ||
		    ( acos( dDotProductResult2 ) > m_dAngleThreshold * ITAConstants::PI_F / 180.0f ) )
			pListener->bARFilterUpdateRequired = true;

		if( bForceARUpdateOnceLocalCopy )
			pListener->bARFilterUpdateRequired = true;

		if( pListener->bARFilterUpdateRequired ) // feed update task if required
			m_pARSimulator->AddARUpdateTask( pListener );


		// --= Process samples =--

		auto pSourceData = pPath->pSource->pData;
		const ITASampleFrame& sfInputBuffer( *pSourceData->psfSignalSourceInputBuf );
		const ITASampleBuffer* psbInput( &( sfInputBuffer[0] ) );

		pPath->pVariableDelayLine->Process( psbInput, &ctxAudio.m_sbTempBuf1 );

		float fSoundSourceGain =
		    pPath->pSource->pData->bMuted ? 0.0f : float( pSourceState->GetVolume( m_oParams.pCore->GetCoreConfig( )->dDefaultAmplitudeCalibration ) );

		// Diffuse decay auralization mode, react upon source aura mode here
		bool bDDEnabledSource = ( pSourceState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_DIFFUSE_DECAY ) > 0;

		// add source signal to accumulating listener input signal buffer
		if( bDDEnabledSource )
			pPath->pListener->psbInput->MulAdd( ctxAudio.m_sbTempBuf1, fSoundSourceGain, 0, 0, GetBlocklength( ) );

		apit++;
	}

	// Artificial reverb convolution (for each listener's accumulated and delayed input signals)
	std::map<int, Listener*>::iterator lit = m_mListener.begin( );
	while( lit != m_mListener.end( ) )
	{
		Listener* pListener              = lit->second;
		CVAReceiverState* pListenerState = ( m_pCurSceneState ? m_pCurSceneState->GetReceiverState( pListener->pData->iID ) : NULL );

		const float* pfIn = pListener->psbInput->data( );
		float* pfOutL     = ( *pListener->psfOutput )[0].data( );
		float* pfOutR     = ( *pListener->psfOutput )[1].data( );
		pListener->pConvolverL->Process( pfIn, pfOutL, ITABase::MixingMethod::OVERWRITE );
		pListener->pConvolverR->Process( pfIn, pfOutR, ITABase::MixingMethod::OVERWRITE );

		pListener->psbInput->Zero( ); // clear input for next block

		if( pListenerState != nullptr )
		{
			// Use convolver gain for Diffuse Decay auramode flag of listener
			bool bDDEnabledGlobal   = ( m_iCurGlobalAuralizationMode & IVAInterface::VA_AURAMODE_DIFFUSE_DECAY ) > 0;
			bool bDDEnabledListener = ( pListenerState->GetAuralizationMode( ) & IVAInterface::VA_AURAMODE_DIFFUSE_DECAY ) > 0;
			bool bDDEnabledCurrent  = ( pListener->pConvolverL->GetGain( ) != 0.0f ); // treat zero gain as disabled DD
			bool bDDStatusChanged   = ( bDDEnabledCurrent != ( bDDEnabledGlobal && bDDEnabledListener ) );
			if( bDDStatusChanged )
			{
				float fNewGain = ( !bDDEnabledCurrent ) ? 1.0f : 0.0f;
				pListener->pConvolverL->SetGain( fNewGain );
				pListener->pConvolverR->SetGain( fNewGain );
			}
		}

		lit++;
	}

	for( auto it: m_mListener )
	{
		Listener* pReceiver = it.second;
		if( !pReceiver->pData->bMuted )
		{
			fm_add( pfOutputCh0, ( *pReceiver->psfOutput )[0].data( ), GetBlocklength( ) );
			fm_add( pfOutputCh1, ( *pReceiver->psfOutput )[1].data( ), GetBlocklength( ) );
		}
	}

	// Trigger AR updates
	m_pARSimulator->evStartSimulationTrigger.SignalEvent( ); // non-blocking

	IncrementWritePointer( );
}

void CVABinauralArtificialReverbAudioRenderer::UpdateTrajectories( )
{
	// Feed motion models with new trajectory samples

	std::map<int, Source*>::iterator sit = m_mSources.begin( );
	while( sit != m_mSources.end( ) )
	{
		int iSourceID   = sit->first;
		Source* pSource = sit->second;

		CVASoundSourceState* pSourceCur = ( m_pCurSceneState ? m_pCurSceneState->GetSoundSourceState( iSourceID ) : nullptr );
		CVASoundSourceState* pSourceNew = ( m_pNewSceneState ? m_pNewSceneState->GetSoundSourceState( iSourceID ) : nullptr );

		const CVAMotionState* pMotionCur = ( pSourceCur ? pSourceCur->GetMotionState( ) : nullptr );
		const CVAMotionState* pMotionNew = ( pSourceNew ? pSourceNew->GetMotionState( ) : nullptr );

		if( pMotionNew && ( pMotionNew != pMotionCur ) )
		{
			VA_TRACE( "BinauralArtificialReverbAudioRenderer", "Source " << iSourceID << " new motion state" );
			pSource->pMotionModel->InputMotionKey( pMotionNew );
		}

		sit++;
	}

	std::map<int, Listener*>::iterator lit = m_mListener.begin( );
	while( lit != m_mListener.end( ) )
	{
		int iListenerID     = lit->first;
		Listener* pListener = lit->second;

		CVAReceiverState* pListenerCur = ( m_pCurSceneState ? m_pCurSceneState->GetReceiverState( iListenerID ) : nullptr );
		CVAReceiverState* pListenerNew = ( m_pNewSceneState ? m_pNewSceneState->GetReceiverState( iListenerID ) : nullptr );

		const CVAMotionState* pMotionCur = ( pListenerCur ? pListenerCur->GetMotionState( ) : nullptr );
		const CVAMotionState* pMotionNew = ( pListenerNew ? pListenerNew->GetMotionState( ) : nullptr );

		if( pMotionNew && ( pMotionNew != pMotionCur ) )
		{
			VA_TRACE( "BinauralAudioRenderer", "Listener " << iListenerID << " new position " );
			pListener->pMotionModel->InputMotionKey( pMotionNew );
		}

		lit++;
	}

	return;
}

void CVABinauralArtificialReverbAudioRenderer::SampleTrajectoriesInternal( const double dTime )
{
	std::list<Source*>::iterator sit = ctxAudio.m_lSources.begin( );
	while( sit != ctxAudio.m_lSources.end( ) )
	{
		Source* pSource( *sit );

		pSource->pMotionModel->HandleMotionKeys( );
		pSource->pMotionModel->EstimatePosition( dTime, pSource->vPredPos );
		pSource->pMotionModel->EstimateOrientation( dTime, pSource->vPredView, pSource->vPredUp );

		sit++;
	}

	std::list<Listener*>::iterator lit = ctxAudio.m_lListener.begin( );
	while( lit != ctxAudio.m_lListener.end( ) )
	{
		Listener* pListener( *lit );

		pListener->pMotionModel->HandleMotionKeys( );
		pListener->pMotionModel->EstimatePosition( dTime, pListener->vPredPos );
		pListener->pMotionModel->EstimateOrientation( dTime, pListener->vPredView, pListener->vPredUp );

		lit++;
	}

	return;
}

void CVABinauralArtificialReverbAudioRenderer::CreateArtificialReverbPath( Source* pSource, Listener* pListener )
{
	int iSourceID   = pSource->pData->iID;
	int iListenerID = pListener->pData->iID;

	assert( !pSource->bDeleted && !pListener->bDeleted );

	VA_VERBOSE( "BinauralArtificialReverbAudioRenderer", "Creating AR path from source " << iSourceID << " -> listener " << iListenerID );

	CBARPath* pPath = dynamic_cast<CBARPath*>( m_pARPathPool->RequestObject( ) );

	pPath->pSource   = pSource;
	pPath->pListener = pListener;

	pPath->bMarkedForDeletion = false;

	m_lARPaths.push_back( pPath );

	ctxAudio.m_qpNewARPaths.push( pPath );
}

void CVABinauralArtificialReverbAudioRenderer::DeleteArtificialReverbPath( CBARPath* pPath )
{
	VA_VERBOSE( "BinauralArtificialReverbAudioRenderer",
	            "Marking AR path from source " << pPath->pSource->pData->iID << " -> listener " << pPath->pListener->pData->iID << " for deletion" );

	// Zur L�schung markieren
	pPath->bMarkedForDeletion = true;
	pPath->RemoveReference( );

	// Audio-Streaming den Pfad zum L�schen �bergeben
	ctxAudio.m_qpDelARPaths.push( pPath );
}

CVABinauralArtificialReverbAudioRenderer::Listener* CVABinauralArtificialReverbAudioRenderer::CreateListener( const int iID )
{
	VA_VERBOSE( "BinauralAudioRenderer", "Creating listener with ID " << iID );

	Listener* pListener = dynamic_cast<Listener*>( m_pListenerPool->RequestObject( ) ); // Reference = 1

	pListener->pData = m_pCore->GetSceneManager( )->GetSoundReceiverDesc( iID );
	pListener->pData->AddReference( );

	// TODO move to factory

	pListener->psfOutput = new ITASampleFrame( 2, GetBlocklength( ), true );
	pListener->psbInput  = new ITASampleBuffer( GetBlocklength( ), true );

	pListener->pConvolverL = new ITAUPConvolution( GetBlocklength( ), m_iMaxReverbFilterLengthSamples );
	pListener->pConvolverL->SetFilterExchangeFadingFunction( ITABase::FadingFunction::COSINE_SQUARE );
	pListener->pConvolverL->SetFilterCrossfadeLength( (std::min)( int( GetBlocklength( ) ), 32 ) );

	pListener->pConvolverR = new ITAUPConvolution( GetBlocklength( ), m_iMaxReverbFilterLengthSamples );
	pListener->pConvolverR->SetFilterExchangeFadingFunction( ITABase::FadingFunction::COSINE_SQUARE );
	pListener->pConvolverR->SetFilterCrossfadeLength( (std::min)( int( GetBlocklength( ) ), 32 ) );

	assert( pListener->pData );
	pListener->bDeleted = false;

	m_mListener.insert( std::pair<int, Listener*>( iID, pListener ) );

	ctxAudio.m_qpNewListeners.push( pListener );

	return pListener;
}

void CVABinauralArtificialReverbAudioRenderer::DeleteListener( const int iListenerID )
{
	VA_VERBOSE( "BinauralAudioRenderer", "Marking listener with ID " << iListenerID << " for removal" );
	std::map<int, Listener*>::iterator it = m_mListener.find( iListenerID );
	Listener* pListener                   = it->second;
	m_mListener.erase( it );
	pListener->bDeleted = true;
	pListener->pData->RemoveReference( );
	pListener->RemoveReference( );

	delete pListener->pConvolverL;
	delete pListener->pConvolverR;
	delete pListener->psbInput;
	delete pListener->psfOutput;

	ctxAudio.m_qpDelListeners.push( pListener );

	return;
}

CVABinauralArtificialReverbAudioRenderer::Source* CVABinauralArtificialReverbAudioRenderer::CreateSource( const int iID )
{
	VA_VERBOSE( "BinauralArtificialReverbAudioRenderer", "Creating source with ID " << iID );

	Source* pSource = dynamic_cast<Source*>( m_pSourcePool->RequestObject( ) );

	pSource->pData = m_pCore->GetSceneManager( )->GetSoundSourceDesc( iID );
	pSource->pData->AddReference( );

	pSource->bDeleted = false;

	m_mSources.insert( std::pair<int, Source*>( iID, pSource ) );

	// �bergabe der neuen Quelle an das Audio-Streaming
	ctxAudio.m_qpNewSources.push( pSource );

	return pSource;
}

void CVABinauralArtificialReverbAudioRenderer::DeleteSource( const int iSourceID )
{
	VA_VERBOSE( "BinauralAudioRenderer", "Marking source with ID " << iSourceID << " for removal" );
	std::map<int, Source*>::iterator it = m_mSources.find( iSourceID );
	Source* pSource                     = it->second;
	m_mSources.erase( it );
	pSource->bDeleted = true;
	pSource->pData->RemoveReference( );
	pSource->RemoveReference( );
	ctxAudio.m_qpDelSources.push( pSource );

	return;
}

void CVABinauralArtificialReverbAudioRenderer::SyncInternalData( )
{
	// Neue Schallpfade ermitteln
	CBARPath* pPath( nullptr );
	while( ctxAudio.m_qpNewARPaths.try_pop( pPath ) )
	{
		pPath->AddReference( );
		ctxAudio.m_lARPaths.push_back( pPath );
	}

	// Verwaiste Schallpfade freigeben (automatische Verwaltung durch den Pool)
	while( ctxAudio.m_qpDelARPaths.try_pop( pPath ) )
	{
		ctxAudio.m_lARPaths.remove( pPath );
		pPath->RemoveReference( );
	}

	// Neue Quellen ermitteln
	Source* pSource( nullptr );
	while( ctxAudio.m_qpNewSources.try_pop( pSource ) )
	{
		pSource->AddReference( );
		pSource->pData->AddReference( );
		ctxAudio.m_lSources.push_back( pSource );
	}

	// Verwaiste Quellen freigeben (automatische Verwaltung durch den Pool)
	while( ctxAudio.m_qpDelSources.try_pop( pSource ) )
	{
		ctxAudio.m_lSources.remove( pSource );
		pSource->pData->RemoveReference( );
		pSource->RemoveReference( );
	}

	// Neue H�rer ermitteln
	Listener* pListener( nullptr );
	while( ctxAudio.m_qpNewListeners.try_pop( pListener ) )
	{
		pListener->AddReference( );
		pListener->pData->AddReference( );
		ctxAudio.m_lListener.push_back( pListener );
	}

	// Verwaiste H�rer freigeben (automatische Verwaltung durch den Pool)
	while( ctxAudio.m_qpDelListeners.try_pop( pListener ) )
	{
		ctxAudio.m_lListener.remove( pListener );
		pListener->pData->RemoveReference( );
		pListener->RemoveReference( );
	}

	return;
}

void CVABinauralArtificialReverbAudioRenderer::ResetInternalData( )
{
	// Referenzen auf Schallpfade aus der Streaming-Liste entfernen
	for( std::list<CBARPath*>::iterator it = ctxAudio.m_lARPaths.begin( ); it != ctxAudio.m_lARPaths.end( ); ++it )
	{
		CBARPath* pPath = *it;
		pPath->RemoveReference( );
	}

	ctxAudio.m_lARPaths.clear( );

	// Referenzen auf H�rer aus der Streaming-Liste entfernen
	for( std::list<Listener*>::iterator it = ctxAudio.m_lListener.begin( ); it != ctxAudio.m_lListener.end( ); ++it )
	{
		Listener* pListener = *it;
		pListener->pData->RemoveReference( );
		pListener->RemoveReference( );
	}
	ctxAudio.m_lListener.clear( );

	// Referenzen auf Quellen aus der Streaming-Liste entfernen
	for( std::list<Source*>::iterator it = ctxAudio.m_lSources.begin( ); it != ctxAudio.m_lSources.end( ); ++it )
	{
		Source* pSource = *it;
		pSource->pData->RemoveReference( );
		pSource->RemoveReference( );
	}
	ctxAudio.m_lSources.clear( );

	ctxAudio.m_iResetFlag = 2; // set ack
}

void CVABinauralArtificialReverbAudioRenderer::UpdateArtificialReverbPaths( const bool bForceUpdate /* = true */ )
{
	// Recalculate artificial reverberation filters where required (listeners only)
	std::map<int, Listener*>::const_iterator lcit = m_mListener.begin( );
	while( lcit != m_mListener.end( ) )
	{
		int iID = lcit->first;
		Listener* pListener( lcit->second );

		CVAReceiverState* pListenerCur = ( m_pCurSceneState ? m_pCurSceneState->GetReceiverState( iID ) : NULL );
		CVAReceiverState* pListenerNew = ( m_pNewSceneState ? m_pNewSceneState->GetReceiverState( iID ) : NULL );

		// New or none HRIR triggers AR update for the listener of this path
		if( pListenerNew == nullptr )
		{
			pListener->pNewHRIR                = NULL;
			pListener->bARFilterUpdateRequired = true;
		}
		else
		{
			IVADirectivity* pCurHRIR = NULL;
			if( pListenerCur )
				pCurHRIR = (IVADirectivity*)pListenerCur->GetDirectivity( );

			IVADirectivity* pNewHRIR = (IVADirectivity*)pListenerNew->GetDirectivity( );

			if( pCurHRIR != pNewHRIR )
			{
				pListener->pNewHRIR                = pNewHRIR;
				pListener->bARFilterUpdateRequired = true;
			}
		}

		VAVec3 vDiffPos = pListener->vPredPos - pListener->vLastARUpdatePos;
		if( vDiffPos.Length( ) > m_dPosThreshold )
			pListener->bARFilterUpdateRequired = true;

		double fDotProductResult1 = pListener->vPredView.Dot( pListener->vLastARUpdateView );
		double fDotProductResult2 = pListener->vPredUp.Dot( pListener->vLastARUpdateUp );
		if( ( acos( fDotProductResult1 ) > m_dAngleThreshold * ITAConstants::PI_F / 180.0f ) ||
		    ( acos( fDotProductResult2 ) > m_dAngleThreshold * ITAConstants::PI_F / 180.0f ) )
			pListener->bARFilterUpdateRequired = true;

		if( bForceUpdate )
			pListener->bARFilterUpdateRequired = true;

		if( pListener->bARFilterUpdateRequired )
		{
			m_pARSimulator->AddARUpdateTask( pListener ); // non-blocking
			VA_TRACE( "BinauralArtificialReverbAudioRenderer", "Adding new AR task for listener " << pListener->pData->iID );
		}

		lcit++;
	}

	// Trigger AR updates
	m_pARSimulator->evStartSimulationTrigger.SignalEvent( ); // non-blocking

	return;
}

void CVABinauralArtificialReverbAudioRenderer::UpdateGlobalAuralizationMode( int iGlobalAuralizationMode )
{
	if( m_iCurGlobalAuralizationMode == iGlobalAuralizationMode )
		return;

	m_iCurGlobalAuralizationMode = iGlobalAuralizationMode;

	return;
}

void CVABinauralArtificialReverbAudioRenderer::UpdateArtificialReverbFilter( Listener* pListener ) const
{
	// Listener has no assigned HRIR? abort
	if( ( pListener->pCurHRIR == nullptr ) && ( pListener->pNewHRIR == nullptr ) )
		return;

	ITACriticalSection oCS;
	oCS.enter( );

	VA_INFO( "BinauralArtificialReverbAudioRenderer", "Calculating artificial reverberation filter for listener '" << pListener->pData->sName << "'" );

	// local copies
	const double dRoomSurfaceArea            = m_dRoomSurfaceArea;
	const double dRoomVolume                 = m_dRoomVolume;
	const double dSoundPowerCorrectionFactor = m_dSoundPowerCorrectionFactor;

	for( auto dReverberationTime: m_vdReverberationTimes )
	{
		assert( dReverberationTime > 0.0f );

		if( ceil( dReverberationTime * GetSampleRate( ) ) > m_iMaxReverbFilterLengthSamples )
			VA_WARN( "BinauralArtificialReverbAudioRenderer", "Target filter length smaller than requested reverberation time, truncating." )
	}

	if( pListener->pNewHRIR != nullptr )
	{
		// Acknowledge new HRIR and overwrite current pointer
		pListener->pCurHRIR = pListener->pNewHRIR;
		pListener->pNewHRIR = NULL;
	}

	CVADirectivityDAFFHRIR* pHRIR = (CVADirectivityDAFFHRIR*)pListener->pCurHRIR;

	if( !pHRIR )
	{
		VA_ERROR( "BinauralArtificialReverbAudioRenderer", "Could not cast receiver HRIR to type CVADirectivityDAFFHRIR, please use a DAFF file with IR content." );
	}

	int iHRIRLength = pHRIR->GetProperties( )->iFilterLength;
	ITASampleFrame sfTempHRIR( 2, iHRIRLength, true ); // TODO member?

	// Some prior assertions
	assert( dRoomSurfaceArea > 0.0f );
	assert( dRoomVolume > 0.0f );
	assert( m_dTimeSlotResolution > 0.0f );

	assert( m_vdReverberationTimes.size( ) == 3 || m_vdReverberationTimes.size( ) == 8 );

	m_mxFilterDesign.lock( );

	if( m_vdReverberationTimes.size( ) != m_vpFilter.size( ) )
	{
		VA_ERROR( "BinauralArtificialReverbAudioRenderer", "EQ filters not equalized, aborting reverb calculation" );
	}

	// START LAS -----------------------------------------------------------------------------------------

	ITASampleFrame sfBRIR_temp( 2, m_iMaxReverbFilterLengthSamples, true ); // 0=L, 1=R, TODO member var? Temporal buffer per frequency
	ITASampleFrame sfBRIR( 2, m_iMaxReverbFilterLengthSamples, true );      // Accumulated reverbs

	for( size_t r = 0; r < m_vdReverberationTimes.size( ); r++ )
	{
		const double& dReverberationTime( m_vdReverberationTimes[r] );
		assert( dReverberationTime > 0.0f );

		srand( 667 ); // fixed poisson sequence

		sfBRIR_temp.zero( );

		const double dAverageLengthRoom = dRoomVolume / dRoomSurfaceArea;
		const double dMeanFreePath      = 4 * dAverageLengthRoom;

		const double dScatterReflectionFactor = 0.75f + ( m_dScatteringCoefficient / 4.0f );

		const double dSamplesPerTimeSlot = GetSampleRate( ) * m_dTimeSlotResolution;

		// get time of last image source
		const double dTimeLastImageSource = dMeanFreePath / m_pCore->oHomogeneousMedium.dSoundSpeed;

		// calc critical distances and initial reverberation energies
		const double dTimeConst = -13.816f / dReverberationTime;
		assert( dTimeConst != 0.0f );

		const double dAreaNormIntegral = ( exp( dTimeConst * double( m_iMaxReverbFilterLengthSamples ) ) - exp( dTimeConst * dTimeLastImageSource ) ) / dTimeConst;
		assert( dAreaNormIntegral > 0.0f );

		// calc critical distance
		double dAverageEquivalentAbsorptionArea = 0.163f * dRoomVolume / dReverberationTime;
		double dCriticalDistance                = sqrt( dAverageEquivalentAbsorptionArea / ( 16.0f * ITAConstants::PI_D ) );
		assert( dCriticalDistance > 0.0f );

		// calc initial energy (formula see diploma thesis Lukas Asp�ck)
		// correction factor as envelope curve has approx 3 times the energy the synth ir has
		double dInitialEnergy = 3.0f / pow( dCriticalDistance, 2 ) / m_pCore->oHomogeneousMedium.dSoundSpeed / dAreaNormIntegral;
		assert( dInitialEnergy > 0.0f );

		double dCurrentScaleFactor; // PROBLEM

		//############ CREATE NOISE SEQUENCES WITH TIME DOMAIN WEIGHTING ############//

		// new loop over fixed time slots
		int iBlockLength       = int( dSamplesPerTimeSlot );
		int iNumberOfTimeSlots = int( m_iMaxReverbFilterLengthSamples / double( iBlockLength ) ) + 1;

		for( int i = 0; i < int( iNumberOfTimeSlots ); i++ )
		{
			int& iTimeSlot( i );
			// calculate reflection density for current block

			// calculates reflection density according to kuttruffs equation
			double dTime                     = ( iTimeSlot * iBlockLength + 1 ) / GetSampleRate( );
			double dCurrentReflectionDensity = 4.0f * ITAConstants::PI_D * m_pCore->oHomogeneousMedium.dSoundSpeed * m_pCore->oHomogeneousMedium.dSoundSpeed *
			                                   m_pCore->oHomogeneousMedium.dSoundSpeed * dTime * dTime / dRoomVolume;

			// if the density exceeds maximumReflectionDensity it will be kept constant at this value
			if( dCurrentReflectionDensity > m_dMaxReflectionDensity )
				dCurrentReflectionDensity = m_dMaxReflectionDensity;

			// calculate reflection density per sample (average scattering also taken into account)
			double dCurrentReflectionDensityPerSample = dCurrentReflectionDensity * dScatterReflectionFactor / GetSampleRate( );

			// calculate number of diracs in current timeslot
			int iNumberDiracsTimeSlot = int( dCurrentReflectionDensityPerSample * iBlockLength );

			// calculate current Poisson Scale Factor in TimeSlot
			double fCurrentScaleFactorConstantPoissonEnergy = ( iNumberDiracsTimeSlot > 1 ) ? sqrt( 1.0f / double( iNumberDiracsTimeSlot ) ) : 1.0f;

			// set all diracs in current time slot
			for( int j = 0; j < iNumberDiracsTimeSlot; j++ )
			{
				// choose random position within timeSlot
				int iCurrentDiracPosition = iTimeSlot * iBlockLength + int( iBlockLength * rand( ) / double( RAND_MAX ) );

				// calculate absolute position (needed to choose if audible or not)
				double dAbsoluteTimePosition = iCurrentDiracPosition / GetSampleRate( ); // + fDelayTimeDirectSound;

				// get random angles
				double dAziAngle = rand( ) / RAND_MAX * 360.0f;
				// double dAziAngleVistaImpl = VistaRandomNumberGenerator::GetStandardRNG()->GenerateDouble( 0.0f, 360.0f );
				double dEleAngle = ( 1.0f - 2.0f * ( rand( ) / RAND_MAX ) ) * 90.0f;

				// get respective HRIRs
				assert( iHRIRLength == sfTempHRIR.length( ) );
				sfTempHRIR.zero( ); // nicht n�tig, wenn assert gilt
				int iIdx = -1;
				pHRIR->GetNearestNeighbour( float( dAziAngle ), float( dEleAngle ), &iIdx ); // view direction
				pHRIR->GetHRIRByIndex( &sfTempHRIR, iIdx, 1.0f );

				// get sign for dirac pulse
				int iRandomSign = 2 * ( rand( ) % 2 ) - 1;

				// Beispiel mit Vista:
				// double dRandomSignVistaImpl = ( int( VistaRandomNumberGenerator::GetStandardRNG()->GenerateInt31() ) > 0 ) ? 1.0f : -1.0f;

				bool bEarlyReflectionsPhase = ( dAbsoluteTimePosition < dTimeLastImageSource );
				bool bLateReflectionPhase   = ( dAbsoluteTimePosition > dTimeLastImageSource ) &&
				                            ( iCurrentDiracPosition + iHRIRLength < m_iMaxReverbFilterLengthSamples ); // PROBLEM, warum dieses Abbruchkrit. hier?


				if( bEarlyReflectionsPhase )
				{
					// phase during early reflections

					// get Scatter coefficient and calc number of early scattered impulses
					int iNumberOfScatteredPulses = int( m_dScatteringCoefficient * 10 );

					assert( dTimeLastImageSource > 0.0f );
					// calc energy of current Sample: apply weighting function (phase ends at dTimeLastImageSource)
					double dSpectralEnergyThisSample = dInitialEnergy                                                  // energy direct sound
					                                   * ( dAbsoluteTimePosition / dTimeLastImageSource )              // weighting function (IS Energy)
					                                   * exp( -13.816f * dAbsoluteTimePosition / dReverberationTime ); // reverb

					// insert scaled dirac in each frequency band
					dCurrentScaleFactor = iRandomSign * sqrt( dSpectralEnergyThisSample );

					// insert scattered energy
					for( int k = 0; k < iNumberOfScatteredPulses; k++ )
					{
						int iScatterDelayInSamples = int( ( ( k + 1 ) / double( iNumberOfScatteredPulses ) ) *
						                                      ( 2.0f * dAverageLengthRoom / m_pCore->oHomogeneousMedium.dSoundSpeed * GetSampleRate( ) ) +
						                                  rand( ) % 30 );

						assert( dSpectralEnergyThisSample > 0.0f );
						double dCurrentScatterFactor =
						    iRandomSign * ( m_dScatteringCoefficient / pow( 2.0f, k ) ) * sqrt( dSpectralEnergyThisSample ) * fCurrentScaleFactorConstantPoissonEnergy;

						int iDestOffset = iCurrentDiracPosition + iScatterDelayInSamples;
						if( iDestOffset + iHRIRLength > sfBRIR_temp.length( ) )
							continue;

						// float* pfDestCh1 = sfBRIR[ 0 ].data();
						sfBRIR_temp.muladd_frame( sfTempHRIR, float( dCurrentScatterFactor ), 0, iDestOffset, iHRIRLength );
					}
				}
				else if( bLateReflectionPhase )
				{
					// normal artificial reverberation tail
					double dSpectralEnergyThisSample = dInitialEnergy                                                  // energy direct sound
					                                   * exp( -13.816f * dAbsoluteTimePosition / dReverberationTime ); // reverb

					// insert scaled dirac in each frequency band
					dCurrentScaleFactor = iRandomSign * sqrt( dSpectralEnergyThisSample ) //  fSpectralEnergyThisSample
					                      * fCurrentScaleFactorConstantPoissonEnergy;
				}
				else
				{
					break; // PROBLEM, kann man das als Prozess nicht klarer programmieren?
				}

				assert( int( iCurrentDiracPosition ) + iHRIRLength <= sfBRIR_temp.length( ) );
				// float* pfDestCh1 = sfBRIR[ 0 ].data();
				sfBRIR_temp.muladd_frame( sfTempHRIR, float( dCurrentScaleFactor ), 0, iCurrentDiracPosition, iHRIRLength );

			} // END PLACING DIRACS IN CURRENT TIMESLOT LOOP
		}     // END TIMESLOT LOOP

		m_vpFilter[r]->Process( sfBRIR_temp[1].GetLength( ), sfBRIR_temp[1].GetData( ) ); // Auto-resets filter engine
		m_vpFilter[r]->Process( sfBRIR_temp[0].GetLength( ), sfBRIR_temp[0].GetData( ) ); // Auto-resets filter engine
		sfBRIR.add_frame( sfBRIR_temp );
	}

	m_mxFilterDesign.unlock( );

	// END LAS -----------------------------------------------------------------------------------------

	sfBRIR.mul_scalar( float( dSoundPowerCorrectionFactor ) );

	float fMaxAbs = 0;
	for( int i = 0; i < sfBRIR.length( ); i++ )
	{
		std::fabs( sfBRIR[0][i] ) > fMaxAbs ? fMaxAbs = std::fabs( sfBRIR[0][i] ) : fMaxAbs;
		std::fabs( sfBRIR[1][i] ) > fMaxAbs ? fMaxAbs = std::fabs( sfBRIR[1][i] ) : fMaxAbs;
	}
	if( fMaxAbs > 1.0f )
	{
		VA_WARN( "BinauralArtificialReverbAudioRenderer", "Reverberation filter clipping detected, normalizing." );
		writeAudiofile( "ArtificialReverb_clipping.wav", &sfBRIR, GetSampleRate( ) );
		sfBRIR.mul_scalar( 1.0f / fMaxAbs );
	}

	VA_INFO( "BinauralArtificialReverbAudioRenderer", "Updating new artificial reverberation filter for listener '" << pListener->pData->sName << "'" );

	ITAUPFilter* pBRIR_L = pListener->pConvolverL->RequestFilter( );
	ITAUPFilter* pBRIR_R = pListener->pConvolverR->RequestFilter( );

	pBRIR_L->Load( ( sfBRIR )[0].data( ), m_iMaxReverbFilterLengthSamples );
	pBRIR_R->Load( ( sfBRIR )[1].data( ), m_iMaxReverbFilterLengthSamples );

	pListener->pConvolverL->ExchangeFilter( pBRIR_L );
	pListener->pConvolverR->ExchangeFilter( pBRIR_R );

	pBRIR_L->Release( );
	pBRIR_R->Release( );

	return;
}

// Class ARPath

CBARPath::CBARPath( const double dSampleRate, const int iBlockLength )
{
	const float fReserverdMaxDelaySamples = (float)( 3 * dSampleRate ); // 3 seconds ~ 1km initial distance
	m_iDefaultVDLSwitchingAlgorithm       = EVDLAlgorithm::CUBIC_SPLINE_INTERPOLATION;
	pVariableDelayLine                    = new CITAVariableDelayLine( dSampleRate, iBlockLength, fReserverdMaxDelaySamples, m_iDefaultVDLSwitchingAlgorithm );

	return;
}

CBARPath::~CBARPath( )
{
	delete pVariableDelayLine;
}

void CBARPath::UpdateMetrics( )
{
	if( pSource->vPredPos != pListener->vPredPos )
		oRelations.Calc( pSource->vPredPos, pSource->vPredView, pSource->vPredUp, pListener->vPredPos, pListener->vPredView, pListener->vPredUp );
}

#endif // VACORE_WITH_RENDERER_BINAURAL_ARTIFICIAL_REVERB
