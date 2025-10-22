#include "VABinauralClusteringReceiver.h"


CVABinauralClusteringReceiver::CVABinauralClusteringReceiver( const CVACoreImpl* pCore, const Config& oConf ) : m_pCore( pCore ), m_oConf( oConf ) {}

CVABinauralClusteringReceiver::~CVABinauralClusteringReceiver( ) {}

void CVABinauralClusteringReceiver::PreRequest( )
{
	CVABasicMotionModel::Config oListenerMotionConf;

	oListenerMotionConf.bLogEstimatedOutputEnabled = m_oConf.bMotionModelLogEstimated;
	oListenerMotionConf.bLogInputEnabled           = m_oConf.bMotionModelLogInput;
	oListenerMotionConf.dWindowDelay               = m_oConf.dMotionModelWindowDelay;
	oListenerMotionConf.dWindowSize                = m_oConf.dMotionModelWindowSize;
	oListenerMotionConf.iNumHistoryKeys            = m_oConf.iMotionModelNumHistoryKeys;

	pOutput = new ITASampleFrame( 2, m_pCore->GetCoreConfig( )->oAudioDriverConfig.iBuffersize, true );

	pMotionModel = new CVASharedMotionModel( new CVABasicMotionModel( oListenerMotionConf ), true );

	pData = nullptr;
}

void CVABinauralClusteringReceiver::PreRelease( )
{
	delete pMotionModel;
	/* @todo use reset of motion model instead of new / delete in request and release of pool
	motionModel->Reset();
	motionModel->Reset();
	*/
}
