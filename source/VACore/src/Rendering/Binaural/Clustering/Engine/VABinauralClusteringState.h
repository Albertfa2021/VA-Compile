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

#ifndef IW_VACORE_BINAURAL_CLUSTERING_STATE
#define IW_VACORE_BINAURAL_CLUSTERING_STATE

// VA Includes
#include <VA.h>
#include <VAPoolObject.h>

// ITA includes
#include <ITAUPConvolution.h>

// Utils
#include "../Receiver/VABinauralClusteringReceiver.h"
#include "../WaveFront/VABinauralWaveFrontBase.h"
#include "VABinauralClusteringDirection.h"

// STL
#include <queue>

//! State of a clustering stage
class CVABinauralClusteringState
{
public:
	CVABinauralClusteringReceiver* pReceiver;
	std::map<int, CVABinauralClusteringDirection*> m_mPrincipleDirections; //!< Clusters that combine sound sources @todo make private

	CVABinauralClusteringState( int iNumClusters, CVABinauralClusteringReceiver* pReceiver, IVAObjectPool* pClusteringDirectionPool,
	                            std::map<int, ITAUPConvolution*>* pFIRConvolversL, std::map<int, ITAUPConvolution*>* pFIRConvolversR );
	CVABinauralClusteringState( const CVABinauralClusteringState& oState );
	~CVABinauralClusteringState( );

	//! Adds a wave front to the clustering state that will be assigned to a principle direction based on the allowed distance error (threshold)
	void AddWaveFront( int iWaveFrontID, IVABinauralWaveFront* pWaveFront, double dThreshold, int iNumBlockedClusters );

	//! Removes an existing wave front from the state
	void RemoveWaveFront( int iWaveFrontID );

	std::pair<int, CVABinauralClusteringDirection*> CreateCluster( int iWaveFrontID, IVABinauralWaveFront* pWaveFront );

	std::pair<int, CVABinauralClusteringDirection*> CreateCluster( int iPrincipleDirectionID, CVABinauralClusteringDirection* pPrincipleDirection );

	double GetMaxError( );


private:
	IVAObjectPool* m_pClusteringDirectionPool;
	int m_iNumClusters;

	std::queue<int> m_mFreeClusterIDs;
	std::map<int, int> m_mWaveFrontClusteringMap;

	std::map<int, ITAUPConvolution*>* m_pFIRConvolversMapL;
	std::map<int, ITAUPConvolution*>* m_pFIRConvolversMapR;
};

#endif // IW_VACORE_BINAURAL_CLUSTERING_STATE
