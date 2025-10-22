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

#ifndef IW_VACORE_VBAPLOUDSPEAKERSETUP
#define IW_VACORE_VBAPLOUDSPEAKERSETUP

#include "VABase.h"

#include <ITABase/Math/Triangulation.h>
#include <memory>
#include <vector>
#include <string>

// Forward declaration
class CVAHardwareOutput;
class CVAHardwareDevice;

/// Represents a loudspeaker setup including a triangulation for VBAP processing
class CVAVBAPLoudspeakerSetup
{
private:
	VAVec3 m_v3LSSetupCenter;
	const CVAHardwareOutput& m_oHardwareOutput;
	const std::vector<const CVAHardwareDevice*> m_vpLoudspeakers;

	std::shared_ptr<const ITABase::Math::CTriangulation> m_pTriangulation; // Triangulation of LS
	std::vector<double> m_vdLSDistanceToCenter;                            // Vector with distance from center of LS setup to each LS

public:
	CVAVBAPLoudspeakerSetup( const CVAHardwareOutput& oHardwareOutput, const VAVec3& v3LSSetupCenter, const std::string& sTriangulationFile );

	int GetNumChannels( ) const { return m_vpLoudspeakers.size( ); };
	std::shared_ptr<const ITABase::Math::CTriangulation> GetLSTriangulation( ) const { return m_pTriangulation; };
	const std::vector<double>& GetLSDistanceToCenter( ) const { return m_vdLSDistanceToCenter; };
	const VAVec3& GetCenterPos( ) const { return m_v3LSSetupCenter; };

};

#endif // IW_VACORE_VBAPLOUDSPEAKERSETUP
