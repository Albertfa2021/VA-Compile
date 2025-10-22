#include "VABinauralClusteringDirectionPoolFactory.h"

#include "VABinauralClusteringDirection.h"

CVABinauralClusteringDirectionPoolFactory::CVABinauralClusteringDirectionPoolFactory( int iBlockLength, int iHRIRFilterLength )
    : m_iBlockLength( iBlockLength )
    , m_iHRIRFilterLength( iHRIRFilterLength )
{
}

CVABinauralClusteringDirectionPoolFactory::~CVABinauralClusteringDirectionPoolFactory( ) {}

CVAPoolObject* CVABinauralClusteringDirectionPoolFactory::CreatePoolObject( )
{
	return new CVABinauralClusteringDirection( m_iBlockLength, m_iHRIRFilterLength );
}
