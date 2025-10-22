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

#ifndef IW_VACORE_BINAURAL_CLUSTERING_DIRECTION
#define IW_VACORE_BINAURAL_CLUSTERING_DIRECTION

// VA Includes
#include <VA.h>

// ITA includes
#include <ITAUPConvolution.h>

// Utils
#include "../Receiver/VABinauralClusteringReceiver.h"
#include "../WaveFront/VABinauralWaveFrontBase.h"

//! Class representing a binaural clustering direction that combines and processes multiple incident wave fronts in the sector of the direction
/**
 * The GetOutput method subsequently processes all linked wave fronts / propagation paths
 *
 */
class CVABinauralClusteringDirection : public CVAPoolObject
{
public:
	int iNumWaveFronts;
	double dMaxError;

	CVABinauralClusteringDirection( int iBlockLength, int iHRIRFilterLength );
	~CVABinauralClusteringDirection( );

	void Init( int iID, IVABinauralWaveFront* pWaveFront, CVABinauralClusteringReceiver* pClusteringDirectionReceiver, ITAUPConvolution* pFIRRight,
	           ITAUPConvolution* pFIRLeft );
	void Init( CVABinauralClusteringDirection* pClusteringDirection );

	ITASampleFrame* GetOutput( );

	double GetDistError( IVABinauralWaveFront* pWaveFront );

	void AddWaveFront( int iID, IVABinauralWaveFront* pWaveFront );
	void AddWaveFront( int iID, IVABinauralWaveFront* pWaveFront, double dError ); // @todo jst: appears to be unused, confirm
	void RemoveWaveFront( int iID );

	void PreRequest( );
	void PreRelease( );

private:
	ITASampleBuffer m_sbTempChL;
	ITASampleBuffer m_sbTempChR;

	ITASampleFrame* m_psfOutput;

	ITASampleFrame* m_psfTempHRIR;

	CVABinauralClusteringReceiver* m_pClusteringDirectionReceiver; // Receiver instance connected to this clustering direction (principle direction)

	ITAUPConvolution* m_pFIRConvolverChL;
	ITAUPConvolution* m_pFIRConvolverChR;

	VAVec3 m_v3ReceiverPos;
	VAVec3 m_v3PrincipleDirection; //!< Target of the clustering direction projected to the unity sphere around receiver ( norm == 1 )
	VAVec3 m_v3ClusteringDirectionToReceiverPos;

	std::map<int, IVABinauralWaveFront*> m_mWaveFronts;

	int m_iLastHRIRIndex;

	double CummulativeMovingAverage( int n, double average, double value );
};

#endif // IW_VACORE_BINAURAL_CLUSTERING_DIRECTION
