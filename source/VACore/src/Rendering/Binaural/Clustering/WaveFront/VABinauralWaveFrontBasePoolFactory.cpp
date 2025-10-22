#include "VABinauralWaveFrontBasePoolFactory.h"

CVABinauralWaveFrontBasePoolFactory::CVABinauralWaveFrontBasePoolFactory( CVABinauralWaveFrontBase::Config oConf ) : m_oConf( oConf ) {}

CVABinauralWaveFrontBasePoolFactory::~CVABinauralWaveFrontBasePoolFactory( ) {}

CVAPoolObject* CVABinauralWaveFrontBasePoolFactory::CreatePoolObject( )
{
	return new CVABinauralWaveFrontBase( m_oConf );
}
