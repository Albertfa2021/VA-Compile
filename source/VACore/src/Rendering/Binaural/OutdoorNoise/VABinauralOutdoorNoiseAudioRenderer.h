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

#ifndef IW_VACORE_BINAURAL_OUTDOOR_NOISE_AUDIO_RENDERER
#define IW_VACORE_BINAURAL_OUTDOOR_NOISE_AUDIO_RENDERER

#if VACORE_WITH_RENDERER_BINAURAL_OUTDOOR_NOISE

// VA Includes
#	include "../../VAAudioRenderer.h"

#	include <ITAThirdOctaveFilterbank.h>
#	include <VA.h>

// ITA simulation scheduler includes
#	include <ITA/SimulationScheduler/result_handler.h>

// ITA includes
#	include <ITADataSourceRealization.h>

// Other
#	include "../Clustering/Engine/VABinauralClusteringEngine.h"
#	include "../Clustering/Receiver/VABinauralClusteringReceiver.h"
#	include "../Clustering/VABinauralWaveFront.h"
#	include "VABinauralOutdoorSourceReceiverTransmission.h"
#	include "VABinauralOutdoorWaveFront.h"

// STL
#	include <list>
#	include <memory>

class CVABinauralOutdoorWaveFront;
class CVABinauralOutdoorSource;

namespace ITA
{
	namespace SimulationScheduler
	{
		class CScheduler;
	}
} // namespace ITA

//! A binaural outdoor noise renderer that handles propagation paths efficiently
class CVABinauralOutdoorNoiseRenderer
    : public IVAAudioRenderer
    , public ITADatasourceRealization
    , public ITA::SimulationScheduler::IResultHandler
{
	typedef std::shared_ptr<CVABinauralOutdoorSourceReceiverTransmission> SourceReceiverPairPtr;

	struct CSourceReceiverPairList : public std::list<SourceReceiverPairPtr>
	{
		//! Finds and returns the source-receiver pair corresponding to the given source and receiver. Returns nullptr if pair wasn't found.
		inline SourceReceiverPairPtr Find( const CVABinauralOutdoorSource* pSource, const CVABinauralClusteringReceiver* pReceiver )
		{
			for( auto pSRPair: *this )
				if( pSRPair->IsEqual( pSource, pReceiver ) )
					return pSRPair;
			return nullptr;
		};
		//! Creates and adds a new source-receiver pair to this list
		inline void Add( CVABinauralOutdoorSource* pSource, CVABinauralClusteringReceiver* pReceiver )
		{
			push_back( std::make_shared<CVABinauralOutdoorSourceReceiverTransmission>( pSource, pReceiver ) );
		};
	};

public:
	CVABinauralOutdoorNoiseRenderer( const CVAAudioRendererInitParams& );
	~CVABinauralOutdoorNoiseRenderer( );

	inline void LoadScene( const std::string& ) { };

	void ProcessStream( const ITAStreamInfo* pStreamInfo );

	//! Handles scene updates for clustering stages
	void UpdateScene( CVASceneState* pNewSceneState );

	void Reset( ) override;

	void UpdateGlobalAuralizationMode( int iGlobalAuralizationMode );

	ITADatasource* GetOutputDatasource( );

	void SetParameters( const CVAStruct& );
	CVAStruct GetParameters( const CVAStruct& ) const;

private:
	const CVAAudioRendererInitParams m_oParams;


	CSourceReceiverPairList m_lpSourceReceiverPairs;

	std::map<int, CVABinauralOutdoorSource*> m_mSources;
	std::map<int, CVABinauralClusteringReceiver*> m_mBinauralReceivers;

	std::unique_ptr<ITA::SimulationScheduler::CScheduler> m_pSimulationScheduler;

	CVABinauralClusteringEngine* m_pClusterEngine;

	CVACoreImpl* m_pCore;

	CVASceneState* m_pNewSceneState;
	CVASceneState* m_pCurSceneState;

	IVAObjectPool* m_pWaveFrontPool;
	IVAObjectPool* m_pReceiverPool;

	std::atomic<bool> m_bIndicateReset, m_bResetAck;

	CVABinauralClusteringReceiver::Config m_oDefaultReceiverConf; //!< Default listener config for factory object creation
	CVABinauralOutdoorWaveFront::Config m_oDefaultWaveFrontConf;

	bool m_bFilterPropertyHistoriesEnabled;
	int m_iMaxDelaySamples;
	int m_iHRIRFilterLength;
	int m_iCurGlobalAuralizationMode; // @todo use in UpdateScene
	ITASampleBuffer m_sbSourceInputTemp;

	void Init( const CVAStruct& oArgs );

	void UpdateSoundReceivers( CVASceneStateDiff* diff );
	void UpdateSoundSources( CVASceneStateDiff* diff );

	void CreateSoundReceiver( const int iID, const CVAReceiverState* pState );
	void CreateSoundSource( const int iID, const CVASoundSourceState* pState );

	void DeleteSoundReceiver( const int iID );
	void DeleteSoundSource( const int iiD );

	void UpdateTrajectories( double dTime );
	void UpdateFilterPropertyHistories( double dTime );

	void RequestSimulationUpdate( double dTime );
	void PostResultReceived( std::unique_ptr<ITA::SimulationScheduler::CSimulationResult> pResult ) override;

	double m_dAngularDistanceThreshold;
	int m_iNumClusters;

#	ifdef BINAURAL_OUTDOOR_NOISE_INSOURCE_BENCHMARKS
	ITAStopWatch m_swProcessStream;
#	endif
};

#endif // VACORE_WITH_RENDERER_BINAURAL_OUTDOOR_NOISE

#endif // IW_VACORE_BINAURAL_OUTDOOR_NOISE_AUDIO_RENDERER
