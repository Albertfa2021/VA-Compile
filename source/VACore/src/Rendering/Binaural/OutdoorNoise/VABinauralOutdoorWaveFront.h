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

#ifndef IW_VACORE_BINAURAL_OUTDOOR_WAVE_FRONT
#define IW_VACORE_BINAURAL_OUTDOOR_WAVE_FRONT

// VA includes
#include "../../../DataHistory/VABoolHistoryModel.h"
#include "../../../DataHistory/VADoubleHistoryModel.h"
#include "../../../DataHistory/VASpectrumHistoryModel.h"
#include "../../../DataHistory/VAVec3HistoryModel.h"
#include "../Clustering/Receiver/VABinauralClusteringReceiver.h"
#include "../Clustering/VABinauralWaveFront.h"

#include <VA.h>
#include <VAPoolObject.h>

// ITA includes
#include <ITAIIRCoefficients.h>
#include <ITAIIRFilterEngine.h>
#include <ITAIIRUtils.h>
#include <ITAInterpolation.h>
#include <ITASIMOVariableDelayLineBase.h>
#include <ITAThirdOctaveMagnitudeSpectrum.h>

// Forwards
class CVASoundSourceState;
class CVASoundSourceDesc;
class CVASharedMotionModel;
class CITAVariableDelayLine;
class ITASampleBuffer;
class CVABinauralOutdoorSource;

//! Represents a wave front emitted from a sound source for an outdoor scenario
/**
 * @henry: The implementation of this class is not nice (yet), but focus on the DSP output and leave it as is.
 */
class CVABinauralOutdoorWaveFront
    : public IVABinauralWaveFront
    , public CVAPoolObject
{
public:
	struct Config
	{
		bool bMotionModelLogInput;
		bool bMotionModelLogEstimated;

		double dMotionModelWindowSize;
		double dMotionModelWindowDelay;

		int iMotionModelNumHistoryKeys;

		double dSampleRate;
		int iBlockLength;

		int iIIRFilterOrder;
		int iFIRFilterLength;
		int iFilterDesignAlgorithm;
		int iBufferSize;
		int iHistoryBufferSize;

		int iInterpolationFunction;
	};

	const CVABinauralOutdoorWaveFront::Config oConf;

	CVABinauralOutdoorWaveFront( const CVABinauralOutdoorWaveFront::Config& conf );
	~CVABinauralOutdoorWaveFront( );

	void PreRequest( );
	void PreRelease( );

	void GetOutput( ITASampleBuffer* pfLeftChannel, ITASampleBuffer* pfRightChannel );

	void SetParameters( const CVAStruct& oInArgs );

	void SetFilterCoefficients( const ITABase::CThirdOctaveGainMagnitudeSpectrum& oMags );

	void SetDelay( const double dDelay );
	void SetDelaySamples( const int delay ); //@todo psc: This might be OK to become private
	void SetGain( float fGain );
	void SetMotion( );

	double GetLastHistoryTimestamp( ) const;
	void PushAudibility( const double& dTimeStamp, bool bAudible );
	void PushDelay( const double& dTimeStamp, const double& dDelay );
	void PushGain( const double& dTimeStamp, const double& dGain );
	void PushFilterCoefficients( const double& dTimeStamp, std::unique_ptr<ITABase::CThirdOctaveGainMagnitudeSpectrum> pFilterCoeffs );
	void PushWFOrigin( const double& dTimeStamp, std::unique_ptr<VAVec3> pv3WFAtReceiver );
	void UpdateFilterPropertyHistories( const double& dTime );

	void SetSource( CVABinauralOutdoorSource* source );
	int GetSourceID( ) const override;

	void SetReceiver( CVABinauralClusteringReceiver* sound_receiver );

	void setITDDifference( const float itdDiff );

	bool GetValidWaveFrontOrigin( ) const;
	bool GetValidClusteringPose( ) const;
	void SetClusteringMetrics( const VAVec3& v3Pos, const VAVec3& v3View, const VAVec3& v3Up, const VAVec3& v3PrincipleDirectionOrigin );
	void SetWaveFrontOrigin( const VAVec3& v3Origin ); //@todo psc: Should be removed once history is working. Also change to WF normal.
	VAVec3 GetWaveFrontOrigin( ) const;

	int AddReference( );
	int RemoveReference( );

	void SetAudible( bool bAudible );

private:
	CVABinauralOutdoorSource* m_pSource;
	CVABinauralClusteringReceiver* m_pReceiver;

	double m_dCreationTimeStamp;

	ITABase::CThirdOctaveGainMagnitudeSpectrum m_oMags;
	ITADSP::CFilterCoefficients m_iIIRFilterCoeffs;
	CITAIIRFilterEngine m_oIIRFilterEngine;

	CITASIMOVariableDelayLineBase* m_pSIMOVDL;
	IITASampleInterpolationRoutine* m_pInterpolationRoutine;

	int m_iCursorID;

	ITASampleBuffer m_sbIIRFeed;
	ITASampleBuffer m_sbTemp;
	ITASampleBuffer m_sbInterpolated;

#ifdef BINAURAL_OUTDOOR_NOISE_IIR_OUT_DEBUG
	ITASampleBuffer m_sbDebug;
	unsigned long m_ulDebugWriteCursor;
#endif

	int m_iITDDifferenceNew;
	int m_iITDDifferenceOld;
	bool m_bAudible; //!< Indicates a valid but inaudible path
	float m_fGainCur;
	float m_fGainNew;

	bool m_bWaveFrontOriginSet;
	bool m_bClusteringPoseSet;

	VAVec3 m_v3ClusteringPos;
	VAVec3 m_v3ClusteringView;
	VAVec3 m_v3ClusteringUp;
	VAVec3 m_v3ClusteringPrincipleDirectionOrigin;

	VAVec3 m_v3WaveFrontOrigin;

	CVADoubleHistoryModel m_oDelayHistory;               //!< History storing doubles with propagation delays
	CVADoubleHistoryModel m_oGainHistory;                //!< History storing doubles of gain
	CVASpectrumHistoryModel m_oThirdOctaveFilterHistory; //!< History storing third-octave magnitude spectra
	CVAVec3HistoryModel m_oReceiverWFOriginHistory;      //!< History storing the origin position of the wavefront propagating towards the receiver
	CVABoolHistoryModel m_oAudibilityHistory;            //!< History storing booleans indicating audibility of wavefront


#ifdef BINAURAL_OUTDOOR_NOISE_INSOURCE_BENCHMARKS
	ITAStopWatch m_swGetOutput;
	ITAStopWatch m_swIIRFilter;
	ITAStopWatch m_swVDL;
	ITAStopWatch m_swInterpolation;
#endif
};

#endif // IW_VACORE_BINAURAL_OUTDOOR_WAVE_FRONT
