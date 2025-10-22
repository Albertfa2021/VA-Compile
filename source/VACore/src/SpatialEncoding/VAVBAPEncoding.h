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

#ifndef IW_VACORE_VBAPENCODING
#define IW_VACORE_VBAPENCODING

#include "VASpatialEncoding.h"
#include "VABase.h"
#include <memory>
#include <vector>

//Forward declaration
class CVAVBAPLoudspeakerSetup;

/// Class for N-channel VBAP (spatial) encoding.
/// Applies real-valued VBAP gains depending on the incoming direction and given loudspeaker setup to an input signal.
class CVAVBAPEncoding : public IVASpatialEncoding
{
private:
	std::shared_ptr<const CVAVBAPLoudspeakerSetup> m_pVBAPLoudspeakerSetup; // Loudspeaker setup including triangulation
	int m_iMDAPSpreadingSources;                                            // Number of spreading sources if using MDAP extension for VBAP
	double m_dMDAPSpreadingAngle;                                           // Spreading angle if using MDAP extension for VBAP (radiants)


	std::vector<double> m_vdLSGainsCurrent; // Gains used for current audio block
	std::vector<double> m_vdLSGainsLast;    // Gains used in last audio block (required for crossfade)

public:
	CVAVBAPEncoding( const IVASpatialEncoding::Config& oConf, std::shared_ptr<const CVAVBAPLoudspeakerSetup> pVBAPLoudspeakerSetup );

	/// Must be overloaded from base class. Nothing to reset in this class.
	void Reset( ) override { };

	/// Applies N real-valued SH gains depending on the incoming direction to an input signal. Also applies general signal gain.
	void Process( const ITASampleBuffer& sbInputData, ITASampleFrame& sfOutput, float fGain, double dAzimuthDeg, double dElevationDeg, const CVAReceiverState&  ) override;

private:
	VAVec3 RealWorldSourceDirection( double dAzimuthDeg, double dElevationDeg );
	void UpdateLSGains( float fGain, const VAVec3& v3RealWorldSourceDir );
	void ApplyGainsWithCrossFade( const ITASampleBuffer& sbInputData, ITASampleFrame& sfOutput );
};

#endif // IW_VACORE_VBAPENCODING
