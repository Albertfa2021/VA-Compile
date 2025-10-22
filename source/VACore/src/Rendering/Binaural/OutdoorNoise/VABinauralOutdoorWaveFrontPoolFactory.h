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

#ifndef IW_VACORE_BINAURAL_OUTDOOR_WAVE_FRONT_POOL_FACTORY
#define IW_VACORE_BINAURAL_OUTDOOR_WAVE_FRONT_POOL_FACTORY

#include "VABinauralOutdoorWaveFront.h"

class CVABinauralOutdoorWaveFrontPoolFactory : public IVAPoolObjectFactory
{
public:
	CVABinauralOutdoorWaveFrontPoolFactory( CVABinauralOutdoorWaveFront::Config oConf );
	~CVABinauralOutdoorWaveFrontPoolFactory( );
	CVAPoolObject* CreatePoolObject( );

private:
	const CVABinauralOutdoorWaveFront::Config m_oConf;
};

#endif // IW_VACORE_BINAURAL_OUTDOOR_WAVE_FRONT_POOL_FACTORY
