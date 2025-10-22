#include "VABinauralOutdoorWaveFrontPoolFactory.h"

#include "VABinauralOutdoorWaveFront.h"

CVABinauralOutdoorWaveFrontPoolFactory::CVABinauralOutdoorWaveFrontPoolFactory( CVABinauralOutdoorWaveFront::Config oConf ) : m_oConf( oConf ) {}

CVABinauralOutdoorWaveFrontPoolFactory::~CVABinauralOutdoorWaveFrontPoolFactory( ) {}

CVAPoolObject* CVABinauralOutdoorWaveFrontPoolFactory::CreatePoolObject( )
{
	return new CVABinauralOutdoorWaveFront( m_oConf );
}
