
/*
 *  --------------------------------------------------------------------------------------------
 *
 *    VVV        VVV A           Virtual Acoustics (VA) | http://www.virtualacoustics.org
 *     VVV      VVV AAA          Licensed under the Apache License, Version 2.0
 *      VVV    VVV   AAA
 *       VVV  VVV     AAA        Copyright 2015-2022
 *        VVVVVV       AAA       Institute of Technical Acoustics (ITA)
 *         VVVV         AAA      RWTH Aachen University
 *
 *  --------------------------------------------------------------------------------------------
 */

#ifndef IW_VACORE_BINAURAL_WAVE_FRONT_POOL_FACTORY
#define IW_VACORE_BINAURAL_WAVE_FRONT_POOL_FACTORY

// Utils
#include "VABinauralWaveFrontBase.h"

class CVABinauralWaveFrontBasePoolFactory : public IVAPoolObjectFactory
{
public:
	CVABinauralWaveFrontBasePoolFactory( CVABinauralWaveFrontBase::Config oConf );
	~CVABinauralWaveFrontBasePoolFactory( );
	CVAPoolObject* CreatePoolObject( );

private:
	const CVABinauralWaveFrontBase::Config m_oConf;
};

#endif // IW_VACORE_BINAURAL_WAVE_FRONT_POOL_FACTORY
