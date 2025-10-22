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

#include "VAVec3HistoryModel.h"

// VA
#include <VAException.h>

// ITA Base
#include <ITABase/Math/Spline.h>


bool CVAVec3HistoryModel::Estimate( const double& dTime, VAVec3& v3Out )
{
	bool ok = false;
	switch( m_oEstimationMethod.Enum( ) )
	{
		case CVAHistoryEstimationMethod::EMethod::SampleAndHold:
			ok = SampleAndHold( dTime, v3Out );
			break;
		case CVAHistoryEstimationMethod::EMethod::NearestNeighbor:
			ok = NearestNeighbor( dTime, v3Out );
			break;
		case CVAHistoryEstimationMethod::EMethod::Linear:
			ok = LinearInterpolation( dTime, v3Out );
			break;
		case CVAHistoryEstimationMethod::EMethod::CubicSpline:
			ok = CubicSplineInterpolation( dTime, v3Out );
			break;
		case CVAHistoryEstimationMethod::EMethod::TriangularWindow:
			ok = TriangularWindow( dTime, v3Out );
			break;
		default:
			VA_EXCEPT_NOT_IMPLEMENTED( "[CVAVec3HistoryModel]: Estimation method " + m_oEstimationMethod.ToString( ) + " is not implemented for this data type." );
	}

	if( m_bLogOutputEnabled && ok )
		LogOutputData( dTime, v3Out );

	return ok;
}


bool CVAVec3HistoryModel::CubicSplineInterpolation( const double& dTime, VAVec3& v3Out )
{
	if( m_iNumSamples < 4 )
		return LinearInterpolation( dTime, v3Out );

	int iLeftBound, iRightBound;
	BoundaryIndices( dTime, iLeftBound, iRightBound );

	// Either timestamp is before first data point or no data available
	if( iLeftBound == -1 )
		return false;

	//@todo psc: Since splines cannot extrapolate at the moment, we use last known sample. Remove following lines later.
	if( iRightBound == -1              // Extrapolation: Use last known sample
	    || iRightBound == iLeftBound ) // Or: Exact match
	{
		v3Out = *m_vpSamples[iLeftBound]->pData;
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

	std::vector<double> vdDataValues = { m_vpSamples[viIndices[0]]->pData->x, m_vpSamples[viIndices[1]]->pData->x, m_vpSamples[viIndices[2]]->pData->x,
		                                 m_vpSamples[viIndices[3]]->pData->x };
	v3Out.x                          = ITABase::Math::CubicSpline( vdTimeStamps, vdDataValues, dTime );
	vdDataValues = { m_vpSamples[viIndices[0]]->pData->y, m_vpSamples[viIndices[1]]->pData->y, m_vpSamples[viIndices[2]]->pData->y, m_vpSamples[viIndices[3]]->pData->y };
	v3Out.y      = ITABase::Math::CubicSpline( vdTimeStamps, vdDataValues, dTime );
	vdDataValues = { m_vpSamples[viIndices[0]]->pData->z, m_vpSamples[viIndices[1]]->pData->z, m_vpSamples[viIndices[2]]->pData->z, m_vpSamples[viIndices[3]]->pData->z };
	v3Out.z      = ITABase::Math::CubicSpline( vdTimeStamps, vdDataValues, dTime );
	return true;
}


// ------------------
// ---DATA LOGGING---
// ------------------

void CVAVec3HistoryModel::SetLoggingBaseFilename( const std::string& sBaseFilename )
{
	// If no input file is set, logger will not export to a file.
	if( m_bLogInputEnabled )
		m_oInputDataLog.setOutputFile( sBaseFilename + "_Input.log" );
	if( m_bLogOutputEnabled )
		m_oOutputDataLog.setOutputFile( sBaseFilename + "_Output.log" );
}

void CVAVec3HistoryModel::LogInputData( const double& dTime, const VAVec3& oData )
{
	CLogData logData;
	logData.dTime = dTime;
	logData.oData = oData;
	m_oInputDataLog.log( logData );
}

void CVAVec3HistoryModel::LogOutputData( const double& dTime, const VAVec3& oData )
{
	CLogData logData;
	logData.dTime = dTime;
	logData.oData = oData;
	m_oOutputDataLog.log( logData );
}

std::ostream& CVAVec3HistoryModel::CLogData::outputDesc( std::ostream& os )
{
	os << "Timestamp"
	   << "\t"
	   << "Value [x]"
	   << "\t"
	   << "Value [y]"
	   << "\t"
	   << "Value [z]" << std::endl;
	return os;
};
std::ostream& CVAVec3HistoryModel::CLogData::outputData( std::ostream& os ) const
{
	os.precision( 12 );
	os << dTime << "\t";
	os << oData.x << "\t" << oData.y << "\t" << oData.z;
	os << std::endl;
	return os;
};