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

#ifndef IW_VACORE_AMBISONICSENCODING
#define IW_VACORE_AMBISONICSENCODING

#include "VASpatialEncoding.h"

/// Class for N-channel ambisonics (spatial) encoding.
/// Applies N real-valued SH gains depending on the incoming direction to an input signal.
class CVAAmbisonicsEncoding : public IVASpatialEncoding
{
private:
	int m_iMaxOrder;

public:
	CVAAmbisonicsEncoding( const IVASpatialEncoding::Config& oConf );

	/// Must be overloaded from base class. Nothing to reset in this class.
	void Reset( ) override { };

	/// Applies N real-valued SH gains depending on the incoming direction to an input signal. Also applies general signal gain.
	void Process( const ITASampleBuffer& sbInputData, ITASampleFrame& sfOutput, float fGain, double dAzimuthDeg, double dElevationDeg, const CVAReceiverState& ) override;
};

#endif // IW_VACORE_AMBISONICSENCODING
