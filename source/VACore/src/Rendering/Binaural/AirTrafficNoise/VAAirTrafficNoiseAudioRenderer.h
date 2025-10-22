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

#ifndef IW_VACORE_BINAURALAIRTRAFFICNOISEAUDIORENDERER
#define IW_VACORE_BINAURALAIRTRAFFICNOISEAUDIORENDERER

#if VACORE_WITH_RENDERER_BINAURAL_AIR_TRAFFIC_NOISE

#	include "VAAirTrafficNoiseSoundReceiver.h"
#	include "VAAirTrafficNoiseSoundSource.h"

// VA includes
#	include "../../../Motion/VAMotionModelBase.h"
#	include "../../../Motion/VASharedMotionModel.h"
#	include "../../../Rendering/VAAudioRenderer.h"
#	include "../../../Rendering/VAAudioRendererRegistry.h"
#	include "../../../Scene/VAScene.h"
#	include "../../../VASourceTargetMetrics.h"
#	include "../../../core/core.h"

#	include <VA.h>
#	include <VAObjectPool.h>

// ITA includes
#	include <ITADataSourceRealization.h>
#	include <ITASampleBuffer.h>
#	include <ITAVariableDelayLine.h>

// ITA Geometrical Acoustics includes
#	include <ITAGeo/Atmosphere/StratifiedAtmosphere.h>

// 3rdParty includes
#	include <tbb/concurrent_queue.h>

// STL Includes
#	include <list>
#	include <set>

// VA forwards
class CVASceneState;
class CVASceneStateDiff;
class CVASignalSourceManager;
class CVASoundSourceDesc;

// Internal forward declarations
class CVABATNSourceReceiverTransmission;
class CVABATNSourceReceiverTransmissionFactory;


//! Air Traffic Noise Audio Renderer (VATSS project)
/**
 * Manages sound pathes from jet plane sound sources to a
 * binaural receiver including multiple audio effects:
 *		- Directivity
 *		- Doppler-Shifts
 *		- Air-Absorption
 *		- 1/r-Distance-Law
 *
 */
class CVABinauralAirTrafficNoiseAudioRenderer
    : public IVAAudioRenderer
    , public ITADatasourceRealization
{
public:
	CVABinauralAirTrafficNoiseAudioRenderer( const CVAAudioRendererInitParams& oParams );
	virtual ~CVABinauralAirTrafficNoiseAudioRenderer( );

	//! Reset scene
	void Reset( );

	//! Dummy
	inline void LoadScene( const std::string& ) { };

	//! Handle a scene state change
	/**
	 * This method updates the internal representation of the VA Scene
	 * by setting up or deleting the sound path entities as well as
	 * modifying existing ones that have changed their state, i.e.
	 * pose or dataset
	 */
	void UpdateScene( CVASceneState* pNewSceneState );

	//! Handle a state change in global auralisation mode
	/**
	 * This method updates internal settings for the global auralisation
	 * mode affecting the activation/deactivation of certain components
	 * of the sound path entities
	 */
	void UpdateGlobalAuralizationMode( int iGlobalAuralizationMode );

	//! Render output sample blocks
	/**
	 * This method renders the sound propagation based on the binaural approach
	 * by evaluating motion and events that are retarded in time, i.e. it switches
	 * filter parts and magnitudes of the HRIR or Directivity. It also considers
	 * the effective auralisation mode.
	 */
	void ProcessStream( const ITAStreamInfo* pStreamInfo );

	//! Returns the renderers output stream datasource
	ITADatasource* GetOutputDatasource( );

	CVAStruct GetParameters( const CVAStruct& oArgs );
	void SetParameters( const CVAStruct& oArgs );

	//! Sets the stratified atmosphere using a json format (either as string or via file)
	void SetStratifiedAtmosphere( const std::string& sAtmosphereJSONFilepathOrContent );

private:
	const CVAAudioRendererInitParams m_oParams; //!< Create a const copy of the init params

	CVACoreImpl* m_pCore; //!< Pointer to VACore

	CVASceneState* m_pCurSceneState;
	CVASceneState* m_pNewSceneState;

	int m_iCurGlobalAuralizationMode;

	std::string m_sMediumType;
	std::string m_sStratifiedAtmosphereFilepath;

	ITAGeo::CStratifiedAtmosphere m_oStratifiedAtmosphere;

	IVAObjectPool* m_pSourceReceiverTransmissionPool;
	CVABATNSourceReceiverTransmissionFactory* m_pSourceReceiverTransmissionFactory; //!< Erzeuger fr Schallbertragungen eines Quell-Empfnger-Paares als Pool-Objekte
	std::list<CVABATNSourceReceiverTransmission*>
	    m_lSourceReceiverTransmissions; //!< Liste der Schallbertragungen aller Quell-Empfnger-Paaren (im Thread-Kontext: VACore)

	IVAObjectPool* m_pSourcePool;
	IVAObjectPool* m_pListenerPool;

	std::map<int, CVABATNSoundSource*> m_mSources;     //!< Interne Abbildung der verfgbaren Quellen
	std::map<int, CVABATNSoundReceiver*> m_mListeners; //!< Interne Abbildung der verfgbaren Hrer

	double m_dGroundPlanePosition; //!< Position of ground plane (height) for reflection calculation

	int m_iHRIRFilterLength; //!< Length of the HRIR filter DSP module
	int m_iFilterBankType;   //!< Filter bank type (FIR,IIR)

	EVDLAlgorithm m_iDefaultVDLSwitchingAlgorithm; //!< Umsetzungsalgorithmus der Variablen Verzgerungsleitung


	bool m_bPropagationDelayExternalSimulation;   //!< If true, internal simulation is skipped and parameters are expected to be set using external SetParameters() call
	bool m_bGroundReflectionExternalSimulation;   //!< If true, internal simulation is skipped and parameters are expected to be set using external SetParameters() call
	bool m_bTemporalVariationsExternalSimulation; //!< If true, internal simulation is skipped and parameters are expected to be set using external SetParameters() call
	bool m_bDirectivityExternalSimulation;        //!< If true, internal simulation is skipped and parameters are expected to be set using external SetParameters() call
	bool m_bAirAbsorptionExternalSimulation;      //!< If true, internal simulation is skipped and parameters are expected to be set using external SetParameters() call
	bool m_bSpreadingLossExternalSimulation;      //!< If true, internal simulation is skipped and parameters are expected to be set using external SetParameters() call

	CVABATNSoundReceiver::Config m_oDefaultListenerConf; //!< Default listener config for factory object creation
	CVABATNSoundSource::Config m_oDefaultSourceConf;     //!< Default source config for factory object creation

	class CUpdateMessage : public CVAPoolObject
	{
	public:
		std::list<CVABATNSoundSource*> vNewSources;
		std::list<CVABATNSoundSource*> vDelSources;
		std::list<CVABATNSoundReceiver*> vNewListeners;
		std::list<CVABATNSoundReceiver*> vDelListeners;
		std::list<CVABATNSourceReceiverTransmission*> vNewPaths;
		std::list<CVABATNSourceReceiverTransmission*> vDelPaths;

		inline void PreRequest( )
		{
			vNewSources.clear( );
			vDelSources.clear( );
			vNewListeners.clear( );
			vDelListeners.clear( );
			vNewPaths.clear( );
			vDelPaths.clear( );
		}
	};

	IVAObjectPool* m_pUpdateMessagePool; // really necessary?
	CUpdateMessage* m_pUpdateMessage;

	//! Data in context of audio process
	struct
	{
		tbb::concurrent_queue<CUpdateMessage*> m_qpUpdateMessages;                    //!< Update messages list
		std::list<CVABATNSourceReceiverTransmission*> m_lSourceReceiverTransmissions; //!< List of sound transmissions between source-receiver pairs
		std::list<CVABATNSoundSource*> m_lSources;                                    //!< List of sources
		std::list<CVABATNSoundReceiver*> m_lListener;                                 //!< List of listeners
		ITASampleBuffer m_sbTempBufD;  //!< Temporally used buffer to store a block of samples during processing (direct sound)
		ITASampleBuffer m_sbTempBufR;  //!< Temporally used buffer to store a block of samples during processing (reflected sound)
		std::atomic<int> m_iResetFlag; //!< Reset status flag: 0=normal_op, 1=reset_request, 2=reset_ack
		std::atomic<int> m_iStatus;    //!< Current status flag: 0=stopped, 1=running
	} ctxAudio;

	void Init( const CVAStruct& oArgs );

	//! Removes deleted source-receiver transmission, sources and receiver. Adds new transmissions for new sources and receivers.
	void ManageSoundReceiverTransmissions( const CVASceneState* pCurScene, const CVASceneState* pNewScene, const CVASceneStateDiff* pDiff );

	void UpdateSources( );
	CVABATNSoundReceiver* CreateSoundReceiver( int iID, const CVAReceiverState* );
	void DeleteListener( int iID );
	CVABATNSoundSource* CreateSoundSource( int iID, const CVASoundSourceState* );
	void DeleteSource( int iID );
	CVABATNSourceReceiverTransmission* CreateSoundPath( CVABATNSoundSource*, CVABATNSoundReceiver* );
	void DeleteSourceReceiverTransmission( CVABATNSourceReceiverTransmission* );

	void UpdateTrajectories( );
	//! Updates the directivity data sets for all SoundReceiverTransmissions
	void UpdateDirectivitySets( );

	void SampleTrajectoriesInternal( double dTime );

	void SyncInternalData( );
	void ResetInternalData( );

	friend class CVABATNSourceReceiverTransmission;
	friend class CVABATNSoundReceiverPoolFactory;
	friend class CVABATNSourcePoolFactory;

	//! Not for use, avoid C4512
	inline CVABinauralAirTrafficNoiseAudioRenderer operator=( const CVABinauralAirTrafficNoiseAudioRenderer& ) { VA_EXCEPT_NOT_IMPLEMENTED; };
};

#endif // VACORE_WITH_RENDERER_BINAURAL_AIR_TRAFFIC_NOISE

#endif // IW_VACORE_BINAURALAIRTRAFFICNOISEAUDIORENDERER
