#define NOMINMAX

#include "VABinauralClusteringDirection.h"

#include <math.h>
#include <stdlib.h>

// VA includes
#include "../../../../Utils/VABinauralUtils.h"
#include "../../../../directivities/VADirectivityDAFFHRIR.h"

// ITA includes
#include <ITAConstants.h>
#include <ITAUPFilter.h>
#include <ITAUPFilterPool.h>
#include <ITAVariableDelayLine.h>


CVABinauralClusteringDirection::CVABinauralClusteringDirection( int iBlockLength, int iHRIRFilterLength )
    : m_iLastHRIRIndex( -1 )
    , m_pClusteringDirectionReceiver( NULL )
    , m_pFIRConvolverChL( NULL )
    , m_pFIRConvolverChR( NULL )
    , iNumWaveFronts( 0 )
{
	m_psfOutput   = new ITASampleFrame( 2, iBlockLength, true );
	m_psfTempHRIR = new ITASampleFrame( 2, iHRIRFilterLength, true );
}

CVABinauralClusteringDirection::~CVABinauralClusteringDirection( ) {}

void CVABinauralClusteringDirection::Init( int iID, IVABinauralWaveFront* pWaveFront, CVABinauralClusteringReceiver* pReceiver, ITAUPConvolution* pFIRConvolverChL,
                                           ITAUPConvolution* pFIRConvolverChR )
{
	m_pClusteringDirectionReceiver = pReceiver;
	m_pClusteringDirectionReceiver->AddReference( );

	m_v3PrincipleDirection = pWaveFront->GetWaveFrontOrigin( ) - m_pClusteringDirectionReceiver->v3PredictedPosition; // maybe switch to wave front normal directly
	m_v3PrincipleDirection.Norm( );

	m_pFIRConvolverChL = pFIRConvolverChL;
	m_pFIRConvolverChR = pFIRConvolverChR;

	int iOutputLength = m_pClusteringDirectionReceiver->pOutput->GetLength( );

	if( m_psfOutput->length( ) != iOutputLength )
		m_psfOutput->init( 2, iOutputLength, true );

	if( m_sbTempChL.length( ) != iOutputLength || m_sbTempChR.length( ) != iOutputLength )
	{
		m_sbTempChL.Init( iOutputLength, true );
		m_sbTempChR.Init( iOutputLength, true );
	}

	dMaxError = GetDistError( pWaveFront );

	m_mWaveFronts.insert( std::pair<int, IVABinauralWaveFront*>( iID, pWaveFront ) );

	pWaveFront->AddReference( );

	++iNumWaveFronts;
}

void CVABinauralClusteringDirection::Init( CVABinauralClusteringDirection* pClusterDirection )
{
	m_pClusteringDirectionReceiver = pClusterDirection->m_pClusteringDirectionReceiver;
	m_pClusteringDirectionReceiver->AddReference( );

	m_v3PrincipleDirection = pClusterDirection->m_v3PrincipleDirection;

	const int iOutputLength = m_pClusteringDirectionReceiver->pOutput->GetLength( );

	if( m_psfOutput->length( ) != iOutputLength )
		m_psfOutput->init( 2, iOutputLength, true );

	m_sbTempChL.Init( m_pClusteringDirectionReceiver->pOutput->GetLength( ), true );
	m_sbTempChR.Init( m_pClusteringDirectionReceiver->pOutput->GetLength( ), true );

	iNumWaveFronts = 0;
}

ITASampleFrame* CVABinauralClusteringDirection::GetOutput( )
{
	// Reset output buffer for this direction
	m_psfOutput->zero( );

	// Process wave fronts connected to this clustering direction (binaurally)
	std::map<int, IVABinauralWaveFront*>::const_iterator it;
	for( it = m_mWaveFronts.begin( ); it != m_mWaveFronts.end( ); ++it )
	{
		IVABinauralWaveFront* pWaveFront( it->second );

		pWaveFront->SetClusteringMetrics( m_pClusteringDirectionReceiver->v3PredictedPosition, m_pClusteringDirectionReceiver->v3PredictedView,
		                                  m_pClusteringDirectionReceiver->v3PredictedUp, m_v3PrincipleDirection );

		pWaveFront->GetOutput( &( m_sbTempChL ), &( m_sbTempChR ) );

		( *m_psfOutput )[0] += m_sbTempChL;
		( *m_psfOutput )[1] += m_sbTempChR;
	}


	// Convolution of HRIR

	// Get time-of-arrival for this clustering direction
	CVASourceTargetMetrics oMetrics;
	oMetrics.Calc( m_pClusteringDirectionReceiver->v3PredictedPosition, m_pClusteringDirectionReceiver->v3PredictedView, m_pClusteringDirectionReceiver->v3PredictedUp,
	               m_pClusteringDirectionReceiver->v3PredictedPosition + m_v3PrincipleDirection, VAVec3( 0, 0, -1 ),
	               VAVec3( 0, 1, 0 ) ); // Target orientation unkown and irrelevant here

	CVADirectivityDAFFHRIR* pHRIR = (CVADirectivityDAFFHRIR*)m_pClusteringDirectionReceiver->pDirectivity;
	if( pHRIR )
	{
		const int iHRIRFilterLength = pHRIR->GetProperties( )->iFilterLength;
		if( m_psfTempHRIR->GetLength( ) != iHRIRFilterLength )
			m_psfTempHRIR->Init( 2, iHRIRFilterLength, false );

		float fAzimuthDeg   = (float)oMetrics.dAzimuthS2T;
		float fElevationDeg = (float)oMetrics.dElevationS2T;
		float fDistance     = (float)oMetrics.dDistance; // Distance is actually unkown, but should be 1m because principle direction is normed

		int iIndex;
		pHRIR->GetNearestNeighbour( fAzimuthDeg, fElevationDeg, &iIndex );

		if( m_iLastHRIRIndex != iIndex )
		{
			pHRIR->GetHRIRByIndex( m_psfTempHRIR, iIndex, fDistance );

			ITAUPFilter* pHRIRFilterChL = m_pFIRConvolverChL->GetFilterPool( )->RequestFilter( );
			ITAUPFilter* pHRIRFilterChR = m_pFIRConvolverChR->GetFilterPool( )->RequestFilter( );

			pHRIRFilterChL->Zeros( );
			pHRIRFilterChR->Zeros( );

			pHRIRFilterChL->Load( ( *m_psfTempHRIR )[0].data( ), iHRIRFilterLength );
			pHRIRFilterChR->Load( ( *m_psfTempHRIR )[1].data( ), iHRIRFilterLength );

			m_pFIRConvolverChL->ExchangeFilter( pHRIRFilterChL );
			m_pFIRConvolverChR->ExchangeFilter( pHRIRFilterChR );

			m_pFIRConvolverChL->ReleaseFilter( pHRIRFilterChL );
			m_pFIRConvolverChR->ReleaseFilter( pHRIRFilterChR );

			m_iLastHRIRIndex = iIndex;
		}

		m_pFIRConvolverChL->Process( ( *m_psfOutput )[0].data( ), ( *m_psfOutput )[0].data( ), ITABase::MixingMethod::OVERWRITE );
		m_pFIRConvolverChR->Process( ( *m_psfOutput )[1].data( ), ( *m_psfOutput )[1].data( ), ITABase::MixingMethod::OVERWRITE );
	}

	return m_psfOutput;
}

double CVABinauralClusteringDirection::GetDistError( IVABinauralWaveFront* pWaveFront )
{
	VAVec3 v3WaveFrontIncidenceDir = pWaveFront->GetWaveFrontOrigin( ) - m_pClusteringDirectionReceiver->v3PredictedPosition;
	v3WaveFrontIncidenceDir.Norm( );

	VAVec3 v3DirectionToEmissionDir;
	v3DirectionToEmissionDir = v3WaveFrontIncidenceDir - m_v3PrincipleDirection;

	return v3DirectionToEmissionDir.Dot( v3DirectionToEmissionDir ); // Error metric = squared distance
}

void CVABinauralClusteringDirection::AddWaveFront( int iWaveFrontID, IVABinauralWaveFront* pWaveFront )
{
	VAVec3 v3WaveFrontEmssissionPosToReceiverPos = pWaveFront->GetWaveFrontOrigin( ) - m_v3ReceiverPos;
	v3WaveFrontEmssissionPosToReceiverPos.Norm( );

	m_v3PrincipleDirection = m_v3PrincipleDirection + ( v3WaveFrontEmssissionPosToReceiverPos - m_v3PrincipleDirection ) / ( iNumWaveFronts + 1 );
	m_v3PrincipleDirection.Norm( );

	double dError = GetDistError( pWaveFront );
	dMaxError     = std::max( dError, dMaxError );

	m_mWaveFronts.insert( std::pair<int, IVABinauralWaveFront*>( iWaveFrontID, pWaveFront ) );

	// --
	pWaveFront->AddReference( );

	++iNumWaveFronts;
}

void CVABinauralClusteringDirection::AddWaveFront( int iWaveFrontID, IVABinauralWaveFront* pWaveFront, double dError )
{
	VAVec3 v3WaveFrontEmittingPosToReceiverPos = pWaveFront->GetWaveFrontOrigin( ) - m_v3ReceiverPos;
	v3WaveFrontEmittingPosToReceiverPos.Norm( );

	double dDistanceError = GetDistError( pWaveFront );
	dMaxError             = std::max( dDistanceError, dMaxError );

	m_v3PrincipleDirection = m_v3PrincipleDirection + ( v3WaveFrontEmittingPosToReceiverPos - m_v3PrincipleDirection ) / ( iNumWaveFronts + 1 );
	m_v3PrincipleDirection.Norm( ); // Projected to unity sphere ...

	m_v3ClusteringDirectionToReceiverPos = m_v3PrincipleDirection - m_v3ReceiverPos;

	m_mWaveFronts.insert( std::pair<int, IVABinauralWaveFront*>( iWaveFrontID, pWaveFront ) );

	pWaveFront->AddReference( );

	++iNumWaveFronts;
}

void CVABinauralClusteringDirection::RemoveWaveFront( int iWaveFrontID )
{
	std::map<int, IVABinauralWaveFront*>::const_iterator it = m_mWaveFronts.find( iWaveFrontID );
	IVABinauralWaveFront* pWaveFront                        = it->second;

	m_mWaveFronts.erase( it );

	m_v3PrincipleDirection = ( m_v3PrincipleDirection * iNumWaveFronts - pWaveFront->GetWaveFrontOrigin( ) ) / ( iNumWaveFronts - 1 );

	pWaveFront->RemoveReference( );

	// TODO: MaxError
	--iNumWaveFronts;
}

void CVABinauralClusteringDirection::PreRequest( )
{
	iNumWaveFronts = 0;
	assert( m_mWaveFronts.empty( ) );
	// m_mWaveFronts.clear();

	dMaxError = 0.0f;
	assert( m_pClusteringDirectionReceiver == nullptr );
}

void CVABinauralClusteringDirection::PreRelease( )
{
	IVABinauralWaveFront* pWaveFront;
	std::map<int, IVABinauralWaveFront*>::const_iterator it;

	// clear all references from this cluster
	for( it = m_mWaveFronts.begin( ); it != m_mWaveFronts.end( ); )
	{
		pWaveFront = it->second;
		it         = m_mWaveFronts.erase( it );

		pWaveFront->RemoveReference( );
	}

	if( m_pClusteringDirectionReceiver )
	{
		m_pClusteringDirectionReceiver->RemoveReference( );
		m_pClusteringDirectionReceiver = nullptr;
	}
}
