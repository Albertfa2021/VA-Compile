#include "VABinauralClusteringPoolFactory.h"

#include "VABinauralClustering.h"

CVABinauralClusteringPoolFactory::CVABinauralClusteringPoolFactory( int iBlockLength, int iHRIRFilterLength )
    : m_iBlockLength( iBlockLength )
    , m_iHRIRFilterLength( iHRIRFilterLength )
{
}

CVABinauralClusteringPoolFactory::~CVABinauralClusteringPoolFactory( ) {}

CVAPoolObject* CVABinauralClusteringPoolFactory::CreatePoolObject( )
{
	return new CVABinauralClustering( m_iBlockLength, m_iHRIRFilterLength );
}
