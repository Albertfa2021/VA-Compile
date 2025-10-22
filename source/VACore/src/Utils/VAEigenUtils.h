#pragma once
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

/*#ifndef IW_VACORE_EIGENUTILS
#define IW_VACORE_EIGENUTILS
*/
#include "Eigen/Dense"

#include <ITAException.h>
#include <ITAStringUtils.h>
#include <VACoreEvent.h>
#include <VAException.h>
#include <VAStruct.h>
#include <VistaBase/VistaExceptionBase.h>
#include <cassert>
#include <map>
#include <string>


namespace VAEigenUtils
{
	void VAVec32Eigen( const VAVec3 vaIn, Eigen::Vector3d &eigenOut );
	void Eigen2VAVec3( const Eigen::Vector3d eigenIn, VAVec3 &vaOut );
	double Eigen2AzimuthRAD( const Eigen::Vector3d eigenIn );
	double Eigen2AzimuthDEG( const Eigen::Vector3d eigenIn );
	double Eigen2ElevationDEG( const Eigen::Vector3d eigenIn );
}; // namespace VAEigenUtils
