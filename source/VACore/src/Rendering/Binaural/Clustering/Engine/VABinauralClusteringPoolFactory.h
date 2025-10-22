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

#ifndef IW_VACORE_BINAURAL_CLUSTERING_POOL_FACTORY
#define IW_VACORE_BINAURAL_CLUSTERING_POOL_FACTORY

// VA Includes
#include <VA.h>
#include <VAPoolObject.h>

class CVABinauralClusteringPoolFactory : public IVAPoolObjectFactory
{
public:
	CVABinauralClusteringPoolFactory( int iBlockLength, int iHRIRFilterLength );
	~CVABinauralClusteringPoolFactory( );

	CVAPoolObject* CreatePoolObject( );

private:
	int m_iBlockLength;
	int m_iHRIRFilterLength;
};

#endif // IW_VACORE_BINAURAL_CLUSTERING_POOL_FACTORY