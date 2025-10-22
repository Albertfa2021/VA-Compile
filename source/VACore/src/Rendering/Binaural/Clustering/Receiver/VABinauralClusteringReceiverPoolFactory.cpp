#include "VABinauralClusteringReceiverPoolFactory.h"

CVABinauralClusteringReceiverPoolFactory::CVABinauralClusteringReceiverPoolFactory( CVACoreImpl* pCore, const CVABinauralClusteringReceiver::Config& oConf )
    : m_pCore( pCore )
    , m_oConf( oConf )
{
}

CVABinauralClusteringReceiverPoolFactory::~CVABinauralClusteringReceiverPoolFactory( ) {}

CVAPoolObject* CVABinauralClusteringReceiverPoolFactory::CreatePoolObject( )
{
	return new CVABinauralClusteringReceiver( m_pCore, m_oConf );
}
