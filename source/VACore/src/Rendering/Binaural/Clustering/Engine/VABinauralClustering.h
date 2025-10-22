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

#ifndef IW_VACORE_BINAURAL_CLUSTERING
#define IW_VACORE_BINAURAL_CLUSTERING

#include <queue>

// VA Includes
#include <VA.h>
#include <VAPoolObject.h>

// Ita includes
#include <ITAUPConvolution.h>

// Utils
#include "../Receiver/VABinauralClusteringReceiver.h"
#include "../WaveFront/VABinauralWaveFrontBase.h"
#include "VABinauralClusteringDirection.h"
#include "VABinauralClusteringState.h"

//! Binaural clustering engine providing directional clustering of wave fronts for binaural a sound receiver
/**
 * Processes the output by gathering processing data from all linked wave fronts.
 * Convolves the clustering direction by an HRIR set using an FIR filter engine, while
 * individual wave fronts are expected to individually adjust the time-of-arrival from
 * the difference to the clustering direction kernel (this instance).
 *
 */
class CVABinauralClustering : public CVAPoolObject
{
public:
	CVABinauralClustering( int iBlockLength, int iHRIRFilterLength );
	~CVABinauralClustering( );

	//! Initialize clustering instance for a binaural receiver
	void Init( int iReceiverID, CVABinauralClusteringReceiver* pReceiver, int iNumClusters, double dThreshold );

	//! Processes the audio stream and returns the calculated samples
	ITASampleFrame* GetOutput( );

	void Update( );
	void AddWaveFront( int iWaveFrontID, IVABinauralWaveFront* pWaveFront );
	void RemoveWaveFront( int iWaveFrontID );

	IVAObjectPool* pClusteringPool;

	int GetSoundReceiverID( ) const;

private:
	std::map<int, ITAUPConvolution*> m_mFIRConvolversL;
	std::map<int, ITAUPConvolution*> m_mFIRConvolversR;

	int m_iSoundReceiverID;
	int m_iNumClusters;          //!< Cluster budget (maximum number of usable clusters)
	double m_dDistanceThreshold; //!< Threshold for clustering algorithm (abstract "distance"), see MA 2019 Mösch

	ITASampleFrame* m_pOutput;
	CVABinauralClusteringReceiver* m_pReceiver;

	std::set<int> m_iDelWaveFrontIDs;

	std::shared_ptr<CVABinauralClusteringState> m_pCurState;
	std::shared_ptr<CVABinauralClusteringState> m_pNextState;

	std::map<int, IVABinauralWaveFront*> m_mUnassignedWaveFronts;
	std::map<int, IVABinauralWaveFront*> m_mAssignedWaveFronts;

	void PreRequest( );
	void PreRelease( );

	int m_iBlockLength;
	int m_iHRIRFilterLength;
};

#endif // IW_VACORE_BINAURAL_CLUSTERING
