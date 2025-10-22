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

#ifndef IW_VACORE_BINAURALENCODING
#define IW_VACORE_BINAURALENCODING

#include "VASpatialEncoding.h"

#include <ITASampleFrame.h>
#include <memory>

class ITAUPConvolution;
class IVADirectivity;

/// Spatial encoding class for two-channel binaural filtering.
/// Convolves an input signal with a two-channel HRIR depending on the incoming direction.
class CVABinauralEncoding : public IVASpatialEncoding
{
private:
	/// Object representing a current HRIR state. Used to determine whether a filter update is required.
	struct CHRIRState
	{
		CHRIRState( ) : pHRIRData( nullptr ), bSpatiallyDiscrete( true ), iRecord( -1 ), dHATODeg( 0.0 ) { };

		const IVADirectivity* pHRIRData; //!< HRIR data, may be a nullptr
		int iRecord;                     //!< HRIR index
		double dHATODeg;                 //!< HRIR head-above-torso orientation
		bool bSpatiallyDiscrete;         //!< HRIR is spatially discrete

		/// If two HRIR states are unequal, a filter update is required
		bool operator!=( const CHRIRState& rhs ) const
		{
			if( pHRIRData != rhs.pHRIRData )
				return true; // Different HRIR set
			if( !bSpatiallyDiscrete )
				return true; // Continous data always requires update
			if( iRecord != rhs.iRecord )
				return true;
			if( dHATODeg != rhs.dHATODeg )
				return true;
			return false;
		};
		/// If two HRIR states are unequal, a filter update is required
		bool operator==( const CHRIRState& rhs ) const { return !operator!=( rhs ); };
	};

public:
	CVABinauralEncoding( const IVASpatialEncoding::Config& oConf );

	/// Resets all DSP elements to their original state (after constructor call)
	void Reset( ) override;

	/// Convolves an input block with a two-channel HRIR depending on the incoming direction. Updates HRIR data using the receiver state. Also applies general signal gain.
	void Process( const ITASampleBuffer& sbInputData, ITASampleFrame& sfOutput, float fGain, double dAzimuthDeg, double dElevationDeg,
	              const CVAReceiverState& pReceiverState ) override;

private:
	// Checks the new HRIR state and updates HRIR filter if required
	void UpdateHRIR( const CVAReceiverState& pReceiverState, float fAzimuthDeg, float fElevationDeg );

	CHRIRState m_oCurrentHRIRState;                       // State of currently used HRIR
	ITASampleFrame m_sfHRIRTemp;                          // Temporary buffer for HRIR filter
	std::unique_ptr<ITAUPConvolution> m_pFIRConvolverChL; // FIR convolution engine for left channel
	std::unique_ptr<ITAUPConvolution> m_pFIRConvolverChR; // FIR convolution engine for right channel
};


#endif // IW_VACORE_BINAURALENCODING
