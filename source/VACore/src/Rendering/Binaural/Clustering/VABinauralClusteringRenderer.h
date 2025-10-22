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

#ifndef IW_VACORE_BINAURAL_CLUSTERING_RENDERER
#define IW_VACORE_BINAURAL_CLUSTERING_RENDERER

// VA Includes
#include "../../VAAudioRenderer.h"

#include <VA.h>

// ITA includes
#include <ITAVariableDelayLine.h>
#include <ITADataSourceRealization.h>

// Other
#include "Engine/VABinauralClusteringEngine.h"
#include "Receiver/VABinauralClusteringReceiver.h"
#include "WaveFront/VABinauralWaveFrontBase.h"

//! A binaural renderer that handles clustering of incoming wave fronts (default: free-field conditions with one wave front per sound source)
/**
 *  @note To use the clustering engine for another render implementation, derive BinauralWaveFrontBase and ovverride the Update() and Process() methods.
 *
 */
class CVABinauralClusteringRenderer
    : public IVAAudioRenderer
    , public ITADatasourceRealization
{
public:
	CVABinauralClusteringRenderer( const CVAAudioRendererInitParams& );
	~CVABinauralClusteringRenderer( );

	inline void LoadScene( const std::string& ) { };

	void ProcessStream( const ITAStreamInfo* pStreamInfo );

	//! Handles scene updates for clustering stages
	void UpdateScene( CVASceneState* pNewSceneState );

	void Reset( );

	void UpdateGlobalAuralizationMode( int iGlobalAuralizationMode );

	ITADatasource* GetOutputDatasource( );

private:
	const CVAAudioRendererInitParams m_oParams;

	std::map<int, CVABinauralWaveFrontBase*> m_mWaveFronts;
	std::map<int, CVABinauralClusteringReceiver*> m_mBinauralReceivers;

	CVABinauralClusteringEngine* m_pClusterEngine;

	CVACoreImpl* m_pCore;

	CVASceneState* m_pNewSceneState;
	CVASceneState* m_pCurSceneState;

	IVAObjectPool* m_pSourcePool;
	IVAObjectPool* m_pReceiverPool;

	std::atomic<bool> m_bIndicateReset, m_bResetAck;

	CVABinauralClusteringReceiver::Config m_oDefaultReceiverConf; //!< Default listener config for factory object creation
	CVABinauralWaveFrontBase::Config m_oDefaultWaveFrontConf;

	EVDLAlgorithm m_iDefaultVDLSwitchingAlgorithm;
	int m_iHRIRFilterLength;
	int m_iCurGlobalAuralizationMode; // @todo use in UpdateScene
	double m_dAdditionalStaticDelaySeconds;
	double m_dAngularDistanceThreshold;
	int m_iNumClusters;

	void Init( const CVAStruct& oArgs );

	void UpdateSoundReceivers( CVASceneStateDiff* diff );
	void UpdateSoundSources( CVASceneStateDiff* diff );

	void CreateSoundReceiver( const int iID, const CVAReceiverState* pState );
	void CreateSoundSource( const int iID, const CVASoundSourceState* pState );

	void DeleteSoundReceiver( const int iID );
	void DeleteSoundSource( const int iiD );

	void UpdateMotionStates( );
	void UpdateTrajectories( double dTime );
};

#endif // IW_VACORE_BINAURAL_CLUSTERING_RENDERER
