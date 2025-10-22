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

#ifndef IW_VACORE_BINAURAL_UTILS
#define IW_VACORE_BINAURAL_UTILS

#include <VABase.h>

namespace VABinauralUtils
{
	namespace TimeOfArrivalModel
	{
		//! Binaural time-of-arrival estimator (ToA)
		/*
		 * @todo Implement for arbitrary HRTFs
		 * @todo Include feature for anthropometric parameters
		 */
		double SphericalShape( const VAVec3& v3EarDirection, const VAVec3& v3DirectionOfIncidentWaveFront, double dSphereRadius, double dSpeedOfSound );
		double SphericalShapeGetLeft( double dAzimuth, double dElevation, double dSphereRadius, double dSpeedOfSound );
		double SphericalShapeGetRight( double dAzimuth, double dElevation, double dSphereRadius, double dSpeedOfSound );
	} // namespace TimeOfArrivalModel
} // namespace VABinauralUtils

#endif // IW_VACORE_BINAURAL_UTILS
