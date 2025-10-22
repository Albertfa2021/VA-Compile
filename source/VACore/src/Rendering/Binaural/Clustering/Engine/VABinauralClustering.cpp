#define NOMINMAX

#include <algorithm>
#include <limits>
#include <math.h>
#include <memory>

// VA
#include "../../../../Utils/VABinauralUtils.h"

#include <VAObjectPool.h>

// ITA includes
#include <ITAUPFilter.h>

// Utils
#include "VABinauralClustering.h"
#include "VABinauralClusteringDirectionPoolFactory.h"

CVABinauralClustering::CVABinauralClustering( int iBlockLength, int iHRIRFilterLength )
    : m_iBlockLength( iBlockLength )
    , m_iHRIRFilterLength( iHRIRFilterLength )
    , m_pCurState( nullptr )
    , m_pNextState( nullptr )
{
	IVAPoolObjectFactory* pFactory = new CVABinauralClusteringDirectionPoolFactory( m_iBlockLength, m_iHRIRFilterLength );
	pClusteringPool                = IVAObjectPool::Create( 16, 2, pFactory, true );
}

CVABinauralClustering::~CVABinauralClustering( ) {}

void CVABinauralClustering::Init( int iSoundReceiverID, CVABinauralClusteringReceiver* pSoundReceiver, int iNumClusters, double dDistanceThreshold )
{
	m_iSoundReceiverID   = iSoundReceiverID;
	m_pReceiver          = pSoundReceiver;
	m_iNumClusters       = iNumClusters;
	m_dDistanceThreshold = dDistanceThreshold; //  4. / iNumClusters;

	m_pOutput = new ITASampleFrame( 2, pSoundReceiver->pOutput->GetLength( ), true );

	for( int i = 0; i < iNumClusters; ++i )
	{
		// initialize left channel convolver for each cluster
		ITAUPConvolution* pFIRConvolverChL = new ITAUPConvolution( m_iBlockLength, m_iHRIRFilterLength );
		pFIRConvolverChL->SetFilterExchangeFadingFunction( ITABase::FadingFunction::COSINE_SQUARE );
		pFIRConvolverChL->SetFilterCrossfadeLength( (std::min)( m_iBlockLength, 32 ) );
		pFIRConvolverChL->SetGain( 1.0f, true );

		ITAUPFilter* pHRIRFilterChL = pFIRConvolverChL->RequestFilter( );
		pHRIRFilterChL->identity( );
		pFIRConvolverChL->ExchangeFilter( pHRIRFilterChL );
		pFIRConvolverChL->ReleaseFilter( pHRIRFilterChL );

		m_mFIRConvolversL.insert( std::pair<int, ITAUPConvolution*>( i, pFIRConvolverChL ) );

		// initialize right channel convolver for each cluster

		ITAUPConvolution* pFIRConvolverChR = new ITAUPConvolution( m_iBlockLength, m_iHRIRFilterLength );

		pFIRConvolverChR->SetFilterExchangeFadingFunction( ITABase::FadingFunction::COSINE_SQUARE );
		pFIRConvolverChR->SetFilterCrossfadeLength( (std::min)( m_iBlockLength, 32 ) );
		pFIRConvolverChR->SetGain( 1.0f, true );

		ITAUPFilter* pHRIRFilterChR = pFIRConvolverChR->RequestFilter( );
		pHRIRFilterChR->identity( );

		pFIRConvolverChR->ExchangeFilter( pHRIRFilterChR );
		pFIRConvolverChR->ReleaseFilter( pHRIRFilterChR );

		m_mFIRConvolversR.insert( std::pair<int, ITAUPConvolution*>( i, pFIRConvolverChR ) );
	}

	m_pCurState.reset( new CVABinauralClusteringState( m_iNumClusters, m_pReceiver, pClusteringPool, &m_mFIRConvolversL, &m_mFIRConvolversR ) );
}

void CVABinauralClustering::AddWaveFront( int iWaveFrontID, IVABinauralWaveFront* pWaveFront )
{
	m_mUnassignedWaveFronts.insert( std::pair<int, IVABinauralWaveFront*>( iWaveFrontID, pWaveFront ) );
}

void CVABinauralClustering::RemoveWaveFront( int iWaveFrontID )
{
	m_iDelWaveFrontIDs.insert( iWaveFrontID );
}

int CVABinauralClustering::GetSoundReceiverID( ) const
{
	return m_iSoundReceiverID;
}

ITASampleFrame* CVABinauralClustering::GetOutput( )
{
	m_pOutput->zero( );

	// swap out clustering state
	if( m_pNextState != nullptr )
		m_pCurState = m_pNextState;

	std::map<int, CVABinauralClusteringDirection*>::const_iterator cit;

	for( cit = m_pCurState->m_mPrincipleDirections.begin( ); cit != m_pCurState->m_mPrincipleDirections.end( ); ++cit )
	{
		CVABinauralClusteringDirection* pPrincipleDirection( cit->second );

		ITASampleFrame* pClusteringDirectionOutput = pPrincipleDirection->GetOutput( );
		( *m_pOutput )[0] += ( *pClusteringDirectionOutput )[0];
		( *m_pOutput )[1] += ( *pClusteringDirectionOutput )[1];
		// *pOutput += *pClusteringDirectionOutput; // @todo jst: switch to this implementation
	}

	return m_pOutput;
}

void CVABinauralClustering::Update( )
{
	// remove deleted sources
	std::set<int>::const_iterator cit_wf;

	for( cit_wf = m_iDelWaveFrontIDs.begin( ); cit_wf != m_iDelWaveFrontIDs.end( ); ++cit_wf )
	{
		const int iWaveFrontID = *cit_wf;

		// remove if in unassigned sources
		m_mUnassignedWaveFronts.erase( iWaveFrontID );

		// remove if in assigned sources
		m_mAssignedWaveFronts.erase( iWaveFrontID );
	}
	m_iDelWaveFrontIDs.clear( );

	// add unassigned sources
	std::map<int, IVABinauralWaveFront*>::iterator it_wf = m_mUnassignedWaveFronts.begin( );

	while( it_wf != m_mUnassignedWaveFronts.end( ) )
	{
		if( it_wf->second->GetValidWaveFrontOrigin( ) )
		{
			std::pair<int, IVABinauralWaveFront*> a( it_wf->first, it_wf->second );
			m_mAssignedWaveFronts.insert( a );
		}
		++it_wf;
	}

	// Add WFs to state and remove from unassigned WFs
	std::shared_ptr<CVABinauralClusteringState> pState =
	    std::make_shared<CVABinauralClusteringState>( m_iNumClusters, m_pReceiver, pClusteringPool, &m_mFIRConvolversL, &m_mFIRConvolversR );
	for( it_wf = m_mAssignedWaveFronts.begin( ); it_wf != m_mAssignedWaveFronts.end( ); ++it_wf )
	{
		m_mUnassignedWaveFronts.erase( it_wf->first );
		pState->AddWaveFront( it_wf->first, it_wf->second, m_dDistanceThreshold, 0 );
	}

	m_pNextState = pState;
}

void CVABinauralClustering::PreRequest( ) {}

void CVABinauralClustering::PreRelease( ) {}
