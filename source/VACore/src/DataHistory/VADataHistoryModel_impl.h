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

// Need to include implementation for template classes, thus we also need an include watcher here
#ifndef IW_VACORE_DATA_HISTORY_MODEL_IMPL
#define IW_VACORE_DATA_HISTORY_MODEL_IMPL

#include "VADataHistoryModel.h"

// VA
//#include "../Utils/VAUtils.h"
//#include "../VALog.h"
#include <VAException.h>


// STL
#include <assert.h>
#include <math.h>
//#include <algorithm>

//-----HISTORY MODEL-----
//-----------------------
#pragma region HistoryModel

template<class DataType>
CVADataHistoryModel<DataType>::CVADataHistoryModel( const CVADataHistoryConfig& oConf, CVAHistoryEstimationMethod::EMethod eMethod )
    : CVADataHistoryModel( oConf.iBufferSize, eMethod, oConf.dWindowSize, oConf.dWindowDelay )
{
	m_bLogInputEnabled  = oConf.bLogInputEnabled;
	m_bLogOutputEnabled = oConf.bLogOutputEnabled;
}

template<class DataType>
CVADataHistoryModel<DataType>::CVADataHistoryModel( int iBufferSize, CVAHistoryEstimationMethod::EMethod eMethod, double dWindowSize /*= ...*/,
                                                    double dWindowDelay /*= ...*/ )
    : m_oEstimationMethod( CVAHistoryEstimationMethod( eMethod ) )
    , m_dWindowSize( dWindowSize )
    , m_dWindowDelay( dWindowDelay )
{
	m_vpSamples.resize( iBufferSize );

	if( m_dWindowSize <= 0 )
		VA_EXCEPT2( INVALID_PARAMETER, "[CVADataHistoryModel]: Window size must be > 0." );

	if( m_dWindowDelay < 0 )
		VA_EXCEPT2( INVALID_PARAMETER, "[CVADataHistoryModel]: Window delay must be positive." );
}

template<class DataType>
void CVADataHistoryModel<DataType>::Reset( )
{
	m_iNumSamples    = 0;
	m_iFront         = 0;
	m_iTail          = -1;
	m_dLastTimestamp = -1;
}


template<class DataType>
double CVADataHistoryModel<DataType>::GetLastTimestamp( ) const
{
	if( m_iNumSamples )
		return m_vpSamples[m_iTail]->dTime;
	return -1.0;
}

template<class DataType>
bool CVADataHistoryModel<DataType>::CheckTimestampValidity( const double& dTimestamp )
{
	return dTimestamp >= 0 && dTimestamp > m_dLastTimestamp;
}

template<class DataType>
void CVADataHistoryModel<DataType>::Push( const double& dTimestamp, ConstDataPtr pNewData )
{
	if( !pNewData )
		VA_EXCEPT2( INVALID_PARAMETER, "[CVADataHistoryModel]: Given pointer to data item is a nullptr." );

	if( !CheckTimestampValidity( dTimestamp ) )
		VA_EXCEPT2( INVALID_PARAMETER, "[CVADataHistoryModel]: Timestamps must be positive and strictly monotonically increasing." );

	m_dLastTimestamp = dTimestamp;
	m_qpNewSamples.push( std::make_unique<CHistorySample>( dTimestamp, std::move( pNewData ) ) );
}

template<class DataType>
void CVADataHistoryModel<DataType>::Update( const double& dLowerTimeLimit /*= -1*/ )
{
	HistorySamplePtr pData;
	while( m_qpNewSamples.try_pop( pData ) )
		InsertSample( std::move( pData ) );

	RemoveOldSamples( dLowerTimeLimit );
}

template<class DataType>
void CVADataHistoryModel<DataType>::SetBufferSize( int iNewSize )
{
	throw CVAException( CVAException::NOT_IMPLEMENTED, "Not implemented" );
}

template<class DataType>
void CVADataHistoryModel<DataType>::InsertSample( HistorySamplePtr pSample )
{
	assert( pSample != nullptr );
	if( m_iNumSamples && pSample->dTime < m_vpSamples[m_iTail]->dTime )
		VA_EXCEPT2( INVALID_PARAMETER, "[CVADataHistoryModel]: Timestamps must be strictly monotonically increasing." );

	if( m_bLogInputEnabled )
		LogInputData( pSample->dTime, *pSample->pData );

	m_iTail              = IncrementIdx( m_iTail );
	m_vpSamples[m_iTail] = std::move( pSample );

	// Update number of elements
	if( m_iNumSamples < GetBufferSize( ) )
		m_iNumSamples++;
	else //@todo psc: Before deleting data that we actually might need: Should we through an exception here or increase the buffer size?
		m_iFront = IncrementIdx( m_iFront );
}

template<class DataType>
void CVADataHistoryModel<DataType>::RemoveOldSamples( const double& dLowerTimeLimit )
{
	if( m_oEstimationMethod.IsSlidingWindow( ) )
		RemoveOldSamplesMovingWindow( dLowerTimeLimit );
	else
		RemoveOldSamplesInterpolation( dLowerTimeLimit );
}

template<class DataType>
int CVADataHistoryModel<DataType>::GetIdxPerLookback( int iLookback ) const
{
	if( m_iNumSamples <= iLookback )
		return -1;

	assert( iLookback >= 0 && iLookback < GetBufferSize( ) );
	return IterateIdx( m_iTail, -iLookback );
}

template<class DataType>
int CVADataHistoryModel<DataType>::GetIdxPerLookforward( int iLookforward ) const
{
	if( m_iNumSamples <= iLookforward )
		return -1;
	assert( iLookforward >= 0 && iLookforward < GetBufferSize( ) );
	return IterateIdx( m_iFront, iLookforward );
}

template<class DataType>
int CVADataHistoryModel<DataType>::IterateIdx( int idxStart, int iShift ) const
{
	assert( idxStart >= 0 && idxStart < GetBufferSize( ) );
	assert( std::abs( iShift ) >= 0 && std::abs( iShift ) < GetBufferSize( ) );
	// idxShift = idxShift % GetBufferSize();
	return ( idxStart + iShift + GetBufferSize( ) ) % GetBufferSize( );
}

template<class DataType>
int CVADataHistoryModel<DataType>::IncrementIdx( int idx ) const
{
	assert( idx >= -1 && idx < GetBufferSize( ) ); // allow -1 since it is start value of m_iTail
	return ( idx + 1 ) % GetBufferSize( );
}

template<class DataType>
int CVADataHistoryModel<DataType>::DecrementIdx( int idx ) const
{
	assert( idx >= 0 && idx < GetBufferSize( ) );
	return ( idx - 1 + GetBufferSize( ) ) % GetBufferSize( );
}

template<class DataType>
void CVADataHistoryModel<DataType>::BoundaryIndices( const double& dTime, int& iLeft, int& iRight )
{
	iLeft  = -1;
	iRight = -1;
	if( m_iNumSamples < 1 )
		return;


	int idx = m_iFront - 1;
	for( int iCount = 0; iCount < m_iNumSamples; iCount++ )
	{
		idx = IncrementIdx( idx );

		if( m_vpSamples[idx]->dTime == dTime ) // Exact match of time stamps
		{
			iLeft  = idx;
			iRight = idx;
			return;
		}
		if( m_vpSamples[idx]->dTime >= dTime )
			break;
		if( idx == m_iTail ) // dTime is outside the right bound of the history
		{
			iLeft = idx;
			return;
		}
	}
	if( idx == m_iFront ) // dTime is outside the left bound of the history
	{
		iRight = idx;
		return;
	}
	iLeft  = DecrementIdx( idx );
	iRight = idx;
}

template<class DataType>
inline bool CVADataHistoryModel<DataType>::SampleAndHold( const double& dTime, DataType& oDataOut )
{
	int iLeftBound, iRightBound;
	BoundaryIndices( dTime, iLeftBound, iRightBound );

	// Either timestamp is before first data point or no data available
	if( iLeftBound == -1 )
		return false;

	oDataOut = *m_vpSamples[iLeftBound]->pData;
	return true;
}

template<class DataType>
bool CVADataHistoryModel<DataType>::NearestNeighbor( const double& dTime, DataType& oDataOut )
{
	int iLeftBound, iRightBound;
	BoundaryIndices( dTime, iLeftBound, iRightBound );

	// Either timestamp is before first data point or no data available
	if( iLeftBound == -1 )
		return false;

	if( iRightBound == -1              // Extrapolation: Use last known sample
	    || iRightBound == iLeftBound ) // Or: Exact match
	{
		oDataOut = *m_vpSamples[iLeftBound]->pData;
		return true;
	}

	// Check which boundary is closer
	double dDeltaLeft  = dTime - m_vpSamples[iLeftBound]->dTime;
	double dDeltaRight = m_vpSamples[iRightBound]->dTime - dTime;
	if( dDeltaLeft < dDeltaRight )
		oDataOut = *m_vpSamples[iLeftBound]->pData;
	else
		oDataOut = *m_vpSamples[iRightBound]->pData;

	return true;
}

template<class DataType>
bool CVADataHistoryModel<DataType>::LinearInterpolation( const double& dTime, DataType& oDataOut )
{
	int iLeftBound, iRightBound;
	BoundaryIndices( dTime, iLeftBound, iRightBound );

	// Either timestamp is before first data point or no data available
	if( iLeftBound == -1 )
		return false;

	if( iRightBound == -1              // Extrapolation: Use last known sample
	    || iRightBound == iLeftBound ) // Or: Exact match
		oDataOut = *m_vpSamples[iLeftBound]->pData;

	else // Actual interpolation
	{
		DataType* pData0 = m_vpSamples[iLeftBound]->pData.get( );
		DataType* pData1 = m_vpSamples[iRightBound]->pData.get( );

		const double& dT0       = m_vpSamples[iLeftBound]->dTime;
		const double& dT1       = m_vpSamples[iRightBound]->dTime;
		const double dPrefactor = ( ( dTime - dT0 ) / ( dT1 - dT0 ) );

		// Implement the following but step by step
		// oDataOut = *pData0 + ( (dTime - dT0) / (dT1 - dT0) ) * (*pData1 - *pData0) ;
		oDataOut = *pData1;
		oDataOut -= *pData0;
		oDataOut *= dPrefactor;
		oDataOut += *pData0;
	}
	return true;
}

template<class DataType>
bool CVADataHistoryModel<DataType>::TriangularWindow( const double& dTime, DataType& oDataOut )
{
	int iNumConsideredElements = 0; // Number of considered history elements
	double dSumOfWeights       = 0; // Sum of weights of considered history elements

	while( iNumConsideredElements < m_iNumSamples )
	{
		int idx                 = GetIdxPerLookback( iNumConsideredElements );
		CHistorySample* pSample = m_vpSamples[idx].get( );
		assert( pSample && pSample->pData );

		double dt = (double)( (long double)dTime - (long double)pSample->dTime );

		// Compute weight
		pSample->dWeight = ComputeWeight( dt );

		// Keyframe out of focus... We do not need to continue
		// bool bOutOfFocus = (m_dWindowSize * 0.5f - m_dWindowDelay) > 0; //@todo psc: This was used for motion model, seems incorrect
		bool bOutOfFocus = ( dt - m_dWindowDelay ) >= m_dWindowSize;
		if( bOutOfFocus && pSample->dWeight <= 0 )
			break;

		dSumOfWeights += pSample->dWeight;
		iNumConsideredElements++;
	}

	// There are history keys, but all are out of the window range => Nearest neighbor
	if( ( iNumConsideredElements == 0 ) || ( dSumOfWeights == 0 ) )
		return NearestNeighbor( dTime, oDataOut );

	// Superposition of all extrapolated positions according to their weight
	for( int idx = 0; idx < iNumConsideredElements; idx++ )
	{
		CHistorySample* pSample = m_vpSamples[GetIdxPerLookback( idx )].get( );

		// float fNormalizedWeight = (float)(pSample->dWeight / dSumOfWeights);
		double dNormalizedWeight = ( pSample->dWeight / dSumOfWeights );
		if( idx == 0 )
			oDataOut = ( *pSample->pData ) * dNormalizedWeight;
		else
			oDataOut += ( *pSample->pData ) * dNormalizedWeight;
	}
	return true;
}

template<class DataType>
double CVADataHistoryModel<DataType>::ComputeWeight( double dt ) const
{
	// Dreiecks-Funktion mit vorgegebener Fensterbreite
	double w = 1 - std::abs( dt - m_dWindowDelay ) / m_dWindowSize;
	return ( w < 0.0 ? 0.0 : w );
}

template<class DataType>
void CVADataHistoryModel<DataType>::RemoveOldSamplesInterpolation( const double& dLowerTimeLimit )
{
	const int iReqSamples = m_oEstimationMethod.GetNumRequiredSamples( );
	const int iMinSamples = 2 > iReqSamples ? 2 : iReqSamples;
	while( m_iNumSamples >= iMinSamples && m_vpSamples[m_iFront]->dTime < dLowerTimeLimit && m_vpSamples[IncrementIdx( m_iFront )]->dTime < dLowerTimeLimit )
	{
		RemoveFront( );
	}
}

template<class DataType>
void CVADataHistoryModel<DataType>::RemoveOldSamplesMovingWindow( const double& dLowerTimeLimit )
{
	double dEarliestTimeBoundOfWindow = dLowerTimeLimit - m_dWindowDelay - m_dWindowSize;

	while( m_iNumSamples && m_vpSamples[m_iFront]->dTime < dEarliestTimeBoundOfWindow )
		RemoveFront( );
}

template<class DataType>
void CVADataHistoryModel<DataType>::RemoveFront( )
{
	assert( m_iNumSamples > 0 );

	m_vpSamples[m_iFront] = nullptr;
	m_iNumSamples--;
	m_iFront = IncrementIdx( m_iFront );
}

#pragma endregion


//-----HISTORY MODEL - LOGGING-----
//---------------------------------
#pragma region HistoryModel - Logging

template<class DataType>
void CVADataHistoryModel<DataType>::LogInputData( const double&, const DataType& )
{
	VA_EXCEPT2( NOT_IMPLEMENTED, "[CVADataHistoryModel]: Loggin input data for this data type is no implemented." );
}
template<class DataType>
void CVADataHistoryModel<DataType>::LogOutputData( const double&, const DataType& )
{
	VA_EXCEPT2( NOT_IMPLEMENTED, "[CVADataHistoryModel]: Loggin input data for this data type is no implemented." );
}
#pragma endregion


//-----HISTORY SAMPLE-----
//------------------------
#pragma region HistorySample

template<class DataType>
CVADataHistoryModel<DataType>::CHistorySample::CHistorySample( const double& dTimestamp, ConstDataPtr pData ) : dTime( dTimestamp )
                                                                                                              , pData( std::move( pData ) )
{
}

#pragma endregion

#endif // IW_VACORE_DATA_HISTORY_MODEL_IMPL