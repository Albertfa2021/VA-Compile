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

#include "VASpectrumHistoryModel.h"

// VA
#include <VAException.h>

// ITA Base
//#include <ITABase/Math/Spline.h>

// STL

// For Compile-Testing:
// Since template functions will only be compiled if they are used, we need to compile this function for testing.
//#include <ITAThirdOctaveMagnitudeSpectrum.h>
// using namespace ITABase;
// void CompileSpectrumTest()
//{
//    auto oHistory = CVASpectrumHistoryModel(100);
//    oHistory.Push(0.0, std::make_unique<CThirdOctaveGainMagnitudeSpectrum>());
//    oHistory.Push(1.0, std::make_unique<CThirdOctaveGainMagnitudeSpectrum>());
//    CThirdOctaveGainMagnitudeSpectrum oTest;
//    oHistory.Estimate(0.5, oTest);
//    oHistory.Update(0.5);
//}


CVASpectrumHistoryModel::CVASpectrumHistoryModel( int iBufferSize,
                                                  CVAHistoryEstimationMethod::EMethod eMethod /*= CVAHistoryEstimationMethod::EMethod::NearestNeighbor*/ )
    : CVADataHistoryModel( iBufferSize, eMethod )
{
	if( eMethod == CVAHistoryEstimationMethod::EMethod::CubicSpline || m_oEstimationMethod.IsSlidingWindow( ) )
		VA_EXCEPT_NOT_IMPLEMENTED( "[CVASpectrumHistoryModel]: Cubic spline interpolation method and sliding windows are not implemented for data type CITASpectrum." );

	if( m_bLogInputEnabled || m_bLogOutputEnabled )
		VA_EXCEPT_NOT_IMPLEMENTED( "[CVASpectrumHistoryModel]: Data logging is not implemented for data type CITASpectrum" );
}

bool CVASpectrumHistoryModel::Estimate( const double& dTime, CITASpectrum& oOut )
{
	switch( m_oEstimationMethod.Enum( ) )
	{
		case CVAHistoryEstimationMethod::EMethod::SampleAndHold:
			return SampleAndHold( dTime, oOut );
		case CVAHistoryEstimationMethod::EMethod::NearestNeighbor:
			return NearestNeighbor( dTime, oOut );
		case CVAHistoryEstimationMethod::EMethod::Linear:
			return LinearInterpolation( dTime, oOut );
		default:
			VA_EXCEPT_NOT_IMPLEMENTED( "[CVASpectrumHistoryModel]: Estimation method " + m_oEstimationMethod.ToString( ) + " is not implemented for this data type." );
	}
}
