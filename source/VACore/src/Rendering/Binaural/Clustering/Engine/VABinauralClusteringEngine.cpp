#include "VABinauralClusteringEngine.h"

// VA includes
#include "VABinauralClusteringPoolFactory.h"

#include <VAObjectPool.h>


CVABinauralClusteringEngine::CVABinauralClusteringEngine( int iBlockLength, int iHRIRFilterLength )
{
	IVAPoolObjectFactory* clusteringFactory = new CVABinauralClusteringPoolFactory( iBlockLength, iHRIRFilterLength );
	m_pClusteringPool                       = IVAObjectPool::Create( 16, 2, clusteringFactory, true );
}

CVABinauralClusteringEngine::~CVABinauralClusteringEngine( ) {}

void CVABinauralClusteringEngine::Update( )
{
	for( auto const& clustering: m_mReceiverClusteringInstances )
	{
		clustering.second->Update( );
	}
}

CVABinauralClustering* CVABinauralClusteringEngine::GetClustering( const int iReceiverID )
{
	const int id = iReceiverID;

	auto it = m_mReceiverClusteringInstances.find( id );

	return it->second;
}

int CVABinauralClusteringEngine::AddWaveFront( IVABinauralWaveFront* pWaveFront )
{
	std::map<int, CVABinauralClustering*>::iterator it;
	for( it = m_mReceiverClusteringInstances.begin( ); it != m_mReceiverClusteringInstances.end( ); ++it )
	{
		it->second->AddWaveFront( m_iWavefrontID, pWaveFront );
	}

	m_mCurrentWaveFronts.insert( std::pair<int, IVABinauralWaveFront*>( m_iWavefrontID, pWaveFront ) );
	return m_iWavefrontID++;
}

void CVABinauralClusteringEngine::RemoveWaveFront( IVABinauralWaveFront* pWavefront )
{
	int iID = -1;
	std::map<int, IVABinauralWaveFront*>::iterator itWFMap;
	for( itWFMap = m_mCurrentWaveFronts.begin( ); itWFMap != m_mCurrentWaveFronts.end( ); ++itWFMap )
	{
		if( itWFMap->second == pWavefront )
		{
			iID = itWFMap->first;
			break;
		}
	}
	if( iID == -1 )
		return;

	for( auto itClusteringMap = m_mReceiverClusteringInstances.begin( ); itClusteringMap != m_mReceiverClusteringInstances.end( ); ++itClusteringMap )
	{
		CVABinauralClustering* pClusteringInstance( itClusteringMap->second );
		pClusteringInstance->RemoveWaveFront( iID );
	}
	m_mCurrentWaveFronts.erase( itWFMap );
}

void CVABinauralClusteringEngine::AddReceiver( int iSoundReceiverID, CVABinauralClusteringReceiver* pReceiver, Config& oConf, double dAngularDistanceThreshold )
{
	CVABinauralClustering* pReceiverClustering = dynamic_cast<CVABinauralClustering*>( m_pClusteringPool->RequestObject( ) ); // Reference = 1

	pReceiverClustering->Init( iSoundReceiverID, pReceiver, oConf.iNumClusters, dAngularDistanceThreshold );

	// add local reference
	m_mReceiverClusteringInstances.insert( std::pair<int, CVABinauralClustering*>( iSoundReceiverID, pReceiverClustering ) );
	m_mReceivers.insert( std::pair<int, CVABinauralClusteringReceiver*>( iSoundReceiverID, pReceiver ) );

	// add preexisting wave fronts
	for( auto const& cit: m_mCurrentWaveFronts )
	{
		int iWaveFrontID = cit.first;
		IVABinauralWaveFront* pWaveFront( cit.second );
		pReceiverClustering->AddWaveFront( iWaveFrontID, pWaveFront );
	}
}

void CVABinauralClusteringEngine::RemoveReceiver( int iSoundReceiverID )
{
	std::map<int, CVABinauralClustering*>::iterator it = m_mReceiverClusteringInstances.find( iSoundReceiverID );
	CVABinauralClustering* clustering                  = it->second;
	m_mReceiverClusteringInstances.erase( it );

	clustering->RemoveReference( );

	/*
	    TODO: foreach cluster in CL delete cluster
	    */
}

void CVABinauralClusteringEngine::RemoveWaveFrontsOfSource( int iSourceID )
{
	auto itWFMap = m_mCurrentWaveFronts.begin( );
	while( itWFMap != m_mCurrentWaveFronts.end( ) )
	{
		if( itWFMap->second->GetSourceID( ) == iSourceID )
		{
			auto itWFDelete = itWFMap++; // This is necessary since item will be removed from m_mCurrentWaveFronts
			RemoveWaveFront( itWFDelete->second );
		}
	}
}
