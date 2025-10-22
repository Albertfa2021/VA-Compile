#define NOMINMAX


#include "VABinauralClusteringState.h"

#include <VAObjectPool.h>
#include <algorithm>


CVABinauralClusteringState::CVABinauralClusteringState( int iNumClusters, CVABinauralClusteringReceiver* pReceiver, IVAObjectPool* pClusteringDirectionPool,
                                                        std::map<int, ITAUPConvolution*>* pFIRConvolversL, std::map<int, ITAUPConvolution*>* pFIRConvolversR )
    : m_iNumClusters( iNumClusters )
    , pReceiver( pReceiver )
    , m_pClusteringDirectionPool( pClusteringDirectionPool )
    , m_pFIRConvolversMapL( pFIRConvolversL )
    , m_pFIRConvolversMapR( pFIRConvolversR )
{
	for( int i = iNumClusters - 1; i >= 0; --i )
		m_mFreeClusterIDs.push( i );
}

CVABinauralClusteringState::CVABinauralClusteringState( const CVABinauralClusteringState& oClusteringState )
    : m_iNumClusters( oClusteringState.m_iNumClusters )
    , pReceiver( oClusteringState.pReceiver )
    , m_mWaveFrontClusteringMap( oClusteringState.m_mWaveFrontClusteringMap )
    , m_mFreeClusterIDs( oClusteringState.m_mFreeClusterIDs )
    , m_pClusteringDirectionPool( oClusteringState.m_pClusteringDirectionPool )
    , m_pFIRConvolversMapL( oClusteringState.m_pFIRConvolversMapL )
    , m_pFIRConvolversMapR( oClusteringState.m_pFIRConvolversMapR )
{
	std::map<int, CVABinauralClusteringDirection*>::const_iterator it;
	for( it = oClusteringState.m_mPrincipleDirections.begin( ); it != oClusteringState.m_mPrincipleDirections.end( ); ++it )
	{
		CreateCluster( it->first, it->second );
	}
}

CVABinauralClusteringState::~CVABinauralClusteringState( )
{
	for( auto cit: m_mPrincipleDirections )
		cit.second->RemoveReference( );
}

void CVABinauralClusteringState::AddWaveFront( int iWaveFrontID, IVABinauralWaveFront* pWaveFront, double dThreshold, int iNumBlockedClusters )
{
	int iNumFreeClusters = m_iNumClusters - iNumBlockedClusters - (int)m_mPrincipleDirections.size( );
	double dErr = 0, dMinErr = std::numeric_limits<double>::infinity( );

	int iNearestPrincipleDirectionID                           = -1;
	CVABinauralClusteringDirection* pNearestPrincipleDirection = nullptr;

	std::map<int, CVABinauralClusteringDirection*>::iterator it;

	for( it = m_mPrincipleDirections.begin( ); it != m_mPrincipleDirections.end( ); ++it )
	{
		dErr = it->second->GetDistError( pWaveFront );
		if( dErr < dMinErr )
		{
			dMinErr                      = dErr;
			iNearestPrincipleDirectionID = it->first;
			pNearestPrincipleDirection   = it->second;
		}
	}

	if( ( dMinErr > dThreshold ) && ( iNumFreeClusters > 0 ) )
	{
		std::pair<int, CVABinauralClusteringDirection*> p;
		p = CreateCluster( iWaveFrontID, pWaveFront ); // already adds wave front

		iNearestPrincipleDirectionID = p.first;
		pNearestPrincipleDirection   = p.second;
	}
	else
	{
		pNearestPrincipleDirection->AddWaveFront( iWaveFrontID, pWaveFront, dMinErr );
	}

	m_mWaveFrontClusteringMap.insert( std::pair<int, int>( iWaveFrontID, iNearestPrincipleDirectionID ) );
}

void CVABinauralClusteringState::RemoveWaveFront( int iWaveFrontID )
{
	std::map<int, int>::const_iterator cit_wf = m_mWaveFrontClusteringMap.find( iWaveFrontID );
	if( cit_wf == m_mWaveFrontClusteringMap.end( ) )
		return;

	std::map<int, CVABinauralClusteringDirection*>::const_iterator cit_pd = m_mPrincipleDirections.find( cit_wf->second );
	CVABinauralClusteringDirection* pPrincipleDirection                   = cit_pd->second;

	pPrincipleDirection->RemoveWaveFront( iWaveFrontID );

	if( pPrincipleDirection->iNumWaveFronts <= 0 )
	{
		m_mPrincipleDirections.erase( cit_pd );
		m_mFreeClusterIDs.push( cit_wf->second );

		pPrincipleDirection->RemoveReference( );
	}
}

std::pair<int, CVABinauralClusteringDirection*> CVABinauralClusteringState::CreateCluster( int iWaveFrontID, IVABinauralWaveFront* pWaveFront )
{
	int iClusterID                                      = m_mFreeClusterIDs.front( );
	CVABinauralClusteringDirection* pPrincipleDirection = dynamic_cast<CVABinauralClusteringDirection*>( m_pClusteringDirectionPool->RequestObject( ) ); // Reference = 1

	pPrincipleDirection->Init( iWaveFrontID, pWaveFront, pReceiver, m_pFIRConvolversMapL->find( iClusterID )->second, m_pFIRConvolversMapR->find( iClusterID )->second );
	m_mPrincipleDirections.insert( std::pair<int, CVABinauralClusteringDirection*>( iClusterID, pPrincipleDirection ) );

	m_mFreeClusterIDs.pop( );

	return std::pair<int, CVABinauralClusteringDirection*>( iClusterID, pPrincipleDirection );
}

std::pair<int, CVABinauralClusteringDirection*> CVABinauralClusteringState::CreateCluster( int iPrincipleDirectionID,
                                                                                           CVABinauralClusteringDirection* pPrincipleDirection )
{
	CVABinauralClusteringDirection* pPrincipleDirectionNew = dynamic_cast<CVABinauralClusteringDirection*>( m_pClusteringDirectionPool->RequestObject( ) );

	pPrincipleDirectionNew->Init( pPrincipleDirection );
	m_mPrincipleDirections.insert( std::pair<int, CVABinauralClusteringDirection*>( iPrincipleDirectionID, pPrincipleDirectionNew ) );

	return std::pair<int, CVABinauralClusteringDirection*>( iPrincipleDirectionID, pPrincipleDirectionNew );
}

double CVABinauralClusteringState::GetMaxError( )
{
	double dMaxValue = 0;
	std::map<int, CVABinauralClusteringDirection*>::iterator it;
	for( it = m_mPrincipleDirections.begin( ); it != m_mPrincipleDirections.end( ); ++it )
	{
		dMaxValue = std::max( dMaxValue, it->second->dMaxError );
	}

	return dMaxValue;
}
