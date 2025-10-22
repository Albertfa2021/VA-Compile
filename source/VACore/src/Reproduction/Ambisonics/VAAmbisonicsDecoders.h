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

#ifndef IW_VACORE_AMBISONICSDECODERS
#define IW_VACORE_AMBISONICSDECODERS

#include "../../VAHardwareSetup.h"

#include <ITABase/Math/Triangulation.h>

# include "Eigen/Core"

using namespace ITABase::Math;

void CalculateSADMatrix( const CVAHardwareOutput* pOutput, const VAVec3& reproductionCenterPos, int reproductionOrder, Eigen::MatrixXd& decoder );
void CalculateMMADMatrix( const CVAHardwareOutput* pOutput, const VAVec3& reproductionCenterPos, int reproductionOrder, Eigen::MatrixXd& decoder,
                          double invTolerance = 1e-6 );
void CalculateEPADMatrix( const CVAHardwareOutput* pOutput, const VAVec3& reproductionCenterPos, int reproductionOrder, Eigen::MatrixXd& decoder );
void CalculateAllRADMatrix( const CVAHardwareOutput* pOutput, const CVAHardwareOutput* pVirtualOutput, const VAVec3& reproductionCenterPos,
                            const CTriangulation* pOutputTriangulation, int reproductionOrder, Eigen::MatrixXd& decoder );
void CalculateAllRADPlusMatrix( const CVAHardwareOutput* pOutput, const CVAHardwareOutput* pVirtualOutput, const VAVec3& reproductionCenterPos,
                                const CTriangulation* pOutputTriangulation, int reproductionOrder, Eigen::MatrixXd& decoder );

void GetSHAtOutputDevice( const VAVec3& vv3DevicePosition, const VAVec3& v3ReproductionCenterPos, int N, std::vector<double>& vdY );

#endif // !IW_VACORE_AMBISONICSDECODERS
