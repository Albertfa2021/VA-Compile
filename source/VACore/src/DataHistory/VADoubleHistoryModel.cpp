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

#include "VADoubleHistoryModel.h"

// VA
#include <VAException.h>

// ITA Base
#include <ITABase/Math/Spline.h>

// STL
#include <string>

// For Compile-Testing:
// Since template functions will only be compiled if they are used, we need to compile this function for testing.
// void CompileDoubleTest()
//{
//    auto oHistory = CVADoubleHistoryModel(100);
//    oHistory.Push(0.0, std::make_unique<double>(1.0));
//    oHistory.Push(1.0, std::make_unique<double>(2.0));
//    double dTest;
//    oHistory.Estimate(0.5, dTest);
//    oHistory.Update(0.5);
//}


bool CVADoubleHistoryModel::Estimate( const double& dTime, double& dOut )
{
	bool ok = false;
	switch( m_oEstimationMethod.Enum( ) )
	{
		case CVAHistoryEstimationMethod::EMethod::SampleAndHold:
			ok = SampleAndHold( dTime, dOut );
			break;
		case CVAHistoryEstimationMethod::EMethod::NearestNeighbor:
			ok = NearestNeighbor( dTime, dOut );
			break;
		case CVAHistoryEstimationMethod::EMethod::Linear:
			ok = LinearInterpolation( dTime, dOut );
			break;
		case CVAHistoryEstimationMethod::EMethod::CubicSpline:
			ok = CubicSplineInterpolation( dTime, dOut );
			break;
		case CVAHistoryEstimationMethod::EMethod::TriangularWindow:
			ok = TriangularWindow( dTime, dOut );
			break;
		default:
			VA_EXCEPT_NOT_IMPLEMENTED( "[CVADoubleHistoryModel]: Estimation method " + m_oEstimationMethod.ToString( ) + " is not implemented for this data type." );
	}

	if( m_bLogOutputEnabled && ok )
		LogOutputData( dTime, dOut );

	return ok;
}


bool CVADoubleHistoryModel::CubicSplineInterpolation( const double& dTime, double& dOut )
{
	if( m_iNumSamples < 4 )
		return LinearInterpolation( dTime, dOut );

	int iLeftBound, iRightBound;
	BoundaryIndices( dTime, iLeftBound, iRightBound );

	// Either timestamp is before first data point or no data available
	if( iLeftBound == -1 )
		return false;

	//@todo psc: Since splines cannot extrapolate at the moment, we use last known sample. Remove following lines later.
	if( iRightBound == -1              // Extrapolation: Use last known sample
	    || iRightBound == iLeftBound ) // Or: Exact match
	{
		dOut = *m_vpSamples[iLeftBound]->pData;
		return true;
	}

	// Find the four samples relevant for the interpolation
	int idxStart;
	if( iRightBound == -1 || iRightBound == m_iTail )
		idxStart = GetIdxPerLookback( 3 );
	else if( iLeftBound == m_iFront )
		idxStart = m_iFront;
	else
		idxStart = DecrementIdx( iLeftBound );

	std::vector<int> viIndices = { idxStart, IterateIdx( idxStart, 1 ), IterateIdx( idxStart, 2 ), IterateIdx( idxStart, 3 ) };

	//@todo psc: Maybe include additional points if available?
	std::vector<double> vdTimeStamps = { m_vpSamples[viIndices[0]]->dTime, m_vpSamples[viIndices[1]]->dTime, m_vpSamples[viIndices[2]]->dTime,
		                                 m_vpSamples[viIndices[3]]->dTime };
	std::vector<double> vdDataValues = { *m_vpSamples[viIndices[0]]->pData, *m_vpSamples[viIndices[1]]->pData, *m_vpSamples[viIndices[2]]->pData,
		                                 *m_vpSamples[viIndices[3]]->pData };
	dOut                             = ITABase::Math::CubicSpline( vdTimeStamps, vdDataValues, dTime );
	return true;
}


// ------------------
// ---DATA LOGGING---
// ------------------

void CVADoubleHistoryModel::SetLoggingBaseFilename( const std::string& sBaseFilename )
{
	// If no input file is set, logger will not export to a file.
	if( m_bLogInputEnabled )
		m_oInputDataLog.setOutputFile( sBaseFilename + "_Input.log" );
	if( m_bLogOutputEnabled )
		m_oOutputDataLog.setOutputFile( sBaseFilename + "_Output.log" );
}

void CVADoubleHistoryModel::LogInputData( const double& dTime, const double& oData )
{
	CLogData logData;
	logData.dTime = dTime;
	logData.oData = oData;
	m_oInputDataLog.log( logData );
}

void CVADoubleHistoryModel::LogOutputData( const double& dTime, const double& oData )
{
	CLogData logData;
	logData.dTime = dTime;
	logData.oData = oData;
	m_oOutputDataLog.log( logData );
}

std::ostream& CVADoubleHistoryModel::CLogData::outputDesc( std::ostream& os )
{
	os << "Timestamp"
	   << "\t"
	   << "Value" << std::endl;
	return os;
};
std::ostream& CVADoubleHistoryModel::CLogData::outputData( std::ostream& os ) const
{
	os.precision( 12 );
	os << dTime << "\t";
	os << oData;
	os << std::endl;
	return os;
};