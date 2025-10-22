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

#ifndef IW_VACORE_BINAURAL_CLUSTERING_ENGINE
#define IW_VACORE_BINAURAL_CLUSTERING_ENGINE

// VA includes
#include <VA.h>

// Utils
#include "../Receiver/VABinauralClusteringReceiver.h"
#include "../WaveFront/VABinauralWaveFrontBase.h"
#include "VABinauralClustering.h"
#include "VABinauralClusteringDirection.h"

//! Single managing instance that handles the entire clustering for all receivers
/**
 */
class CVABinauralClusteringEngine
{
public:
	struct Config
	{
		int iNumClusters;
	};

	//! Initialize clustering engine
	CVABinauralClusteringEngine( int iBlockLength, int iHRIRFilterLength );
	~CVABinauralClusteringEngine( );

	// Handles scene updates
	void Update( );

	//! Returns pointer to clustering instance for a receiver
	CVABinauralClustering* GetClustering( const int iReceiverID );

	//! Adds a new wave front and returns the internal ID
	int AddWaveFront( IVABinauralWaveFront* pWaveFront );

	//! Removes a wave front
	void RemoveWaveFront( IVABinauralWaveFront* pWavefront );

	//! Adds a new receiver
	void AddReceiver( int iListenerID, CVABinauralClusteringReceiver* pReceiver, Config& oConf, double dAngularDistanceThreshold );

	//! Removes a receiver
	void RemoveReceiver( int iListenerID );

	//! Removes all wavefronts corresponding to the source with given ID
	void RemoveWaveFrontsOfSource( int iSourceID );

	std::map<int, CVABinauralClustering*> m_mReceiverClusteringInstances;

private:
	int m_iWavefrontID = 0; //!< Internal IDs for wavefronts

	// IVAObjectPool* m_pWaveFrontPool;
	IVAObjectPool* m_pClusteringPool;

	// std::map< int, CVABinauralWaveFrontBase* > m_mNewWaveFronts;
	std::map<int, IVABinauralWaveFront*> m_mCurrentWaveFronts;

	std::map<int, CVABinauralClusteringReceiver*> m_mReceivers;
};

#endif // IW_VACORE_BINAURAL_CLUSTERING_ENGINE
