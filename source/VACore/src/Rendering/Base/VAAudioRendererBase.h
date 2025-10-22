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

#ifndef IW_VACORE_AUDIORENDERERBASE
#define IW_VACORE_AUDIORENDERERBASE

// VA includes
#include "../../Motion/VAMotionModelBase.h"
#include "../VAAudioRenderer.h"
#include "../VAAudioRendererRegistry.h"
#include "../../SpatialEncoding/VASpatialEncoding.h"
#include "../../Medium/VAHomogeneousMedium.h"
#include "../../VACoreConfig.h"

#include <VA.h>
#include <VAPoolObject.h>

// ITA includes
#include <ITADataSourceRealization.h>
#include <ITASampleBuffer.h>
#include <ITAVariableDelayLine.h>


// 3rdParty includes
#include <tbb/concurrent_queue.h>

// STL Includes
#include <list>
#include <set>

// VA forwards
class IVAObjectPool;
class CVASceneState;
class CVASceneStateDiff;
class CVASoundSourceState;
class CVASoundReceiverState;
class CVARendererSource;
class CVARendererReceiver;



/// Base renderer for other renderers to derive from. Provides a basic structure for the processing by introducing source and receiver objects and respective source-receiver pairs.
/// Does the general handling of scene updates including:
/// - Creation/deletion of sources, receivers and respective pairs
/// - Insert data to motion models for source and receiver trajectories
/// - Resetting
/// Defines the general audio processing loop including:
/// - Sync data between user and audio context
/// - Estimate source and receiver poses using motion model
/// - Set receiver output buffers to zero
/// - Get the auralization mode for current source and receiver pair
/// - Audio processing of each source-receiver pair (to be overloaded in derived renderers)
/// - Accumulate output signals of all receivers
class CVAAudioRendererBase
    : public IVAAudioRenderer
    , public ITADatasourceRealizationEventHandler
{
public:
	/// Config for RendererBase.
	struct Config
	{
		double dSampleRate           = -1.0;     /// Sampling rate taken from Core
		int iBlockSize               = -1;       /// Block size taken from Core
		CVACoreConfig oCoreConfig;               /// Config taken from Core
		CVAHomogeneousMedium oHomogeneousMedium; /// Homogeneous medium taken from Core

		double dAdditionalStaticDelaySeconds = 0.0;       /// Static additional delay (used for delay compensation)
		float fMaxDelaySeconds               = 3.0f;      /// Initial length of VDL buffers in seconds
		std::string sVDLSwitchingAlgorithm   = "linear";  /// String representing VDL switching algorithm in .ini file
		EVDLAlgorithm iVDLSwitchingAlgorithm = EVDLAlgorithm::SWITCH;        /// Actual VDL switching algorithm (set while parsing .ini file)
		CVABasicMotionModel::Config oSourceMotionModel;   /// Motion model settings for sources
		CVABasicMotionModel::Config oReceiverMotionModel; /// Motion model settings for receivers

		std::string sSimulationConfigFilePath; /// Optional extra config file for the simulation.

		/// Default constructor setting default values
		Config( )
		{
			oSourceMotionModel.iNumHistoryKeys              = 1000;
			oSourceMotionModel.dWindowSize                  = 0.1;
			oSourceMotionModel.dWindowDelay                 = 0.1;
			oSourceMotionModel.bLogInputEnabled             = false;
			oSourceMotionModel.bLogEstimatedOutputEnabled   = false;
			oReceiverMotionModel.iNumHistoryKeys            = 1000;
			oReceiverMotionModel.dWindowSize                = 0.1;
			oReceiverMotionModel.dWindowDelay               = 0.1;
			oReceiverMotionModel.bLogInputEnabled           = false;
			oReceiverMotionModel.bLogEstimatedOutputEnabled = false;
		};
		/// Constructor parsing renderer ini-file parameters. Takes a given config for default values.
		/// Note, that part of the parsing process is done in the parent renderer's config method.
		Config( const CVAAudioRendererInitParams& oParams, const Config& oDefaultValues );
	};

	/// Constructor expecting ini-file renderer parameters and a config object with default values which are used if a setting is not specified in ini-file.
	CVAAudioRendererBase( const CVAAudioRendererInitParams& oParams, const Config& oDefaultValues );
	virtual ~CVAAudioRendererBase( );

	/// Handle a user requested resets
	/// This method resets the entire scene by removing all entities. This call is blocking the audio stream until the reset is done.
	void Reset( ) override;

	/// Interface to load a user requested scene (e.g. file to geometric data).
	/// Does nothing per default. Overload in derived renderers to implement functionality.
	virtual void LoadScene( const std::string& ) override { };

	/// Handle a scene state change
	/// This method updates the internal representation of the VA Scene by setting up or deleting the source-receiver pair
	/// entities as well as modifying existing ones that have changed their state, i.e. pose or dataset.
	void UpdateScene( CVASceneState* pNewSceneState ) override;

	/// Handles a state change in global auralisation mode
	void UpdateGlobalAuralizationMode( int iGlobalAuralizationMode ) override;

	/// Handles a set rendering module parameters call()
	/// Overload in derived renderers to implement additional functionality.
	virtual void SetParameters( const CVAStruct& oParams ) override;
	;
	/// Handles a get rendering module parameters call()
	/// Returns an empty struct per default. Overload in derived renderers to implement functionality.
	virtual CVAStruct GetParameters( const CVAStruct& ) const override { return CVAStruct( ); };

	//! Returns the renderers output stream datasource
	ITADatasource* GetOutputDatasource( );
	
	/// Renders one output sample blocks
	/// This method defines the general audio processing chain (see class description). The actual implementation
	/// of handling the sound propagation as well as source and receiver directivities is handled in the source-receiver
	/// pairs of derived renderers.
	void HandleProcessStream( ITADatasourceRealization*, const ITAStreamInfo* pStreamInfo );

protected:
	/// Sets the number of output channels and initializes all objects that are related to this number.
	/// MUST BE CALLED in the constructor of a derived renderer after the number of output channels is clear.
	void InitBaseDSPEntities( int iNumChannels );
	/// Initializes the pool for the source-receiver pairs using the given factory.
	/// IMPORTANT: Factory will be deleted by Pool => No memory handling required for factory.
	/// MUST BE CALLED in the constructor of the derived renderer implementing the functionality of the source-receiver pairs
	void InitSourceReceiverPairPool( IVAPoolObjectFactory* pFactory, int iNumInitialObjects = 64, int iDeltaNumObjects = 8 );

	double GetSampleRate( ) const { return m_oConf.dSampleRate; };
	int GetBlockSize( ) const { return m_oConf.iBlockSize; };
	int GetNumOutputChannels( ) const { return m_iNumChannels; };

	/// An easily readable representation of the auralization mode "bit vector"
	struct AuralizationMode
	{
		bool bDirectSound;        /// Direct sound
		bool bEarlyReflections;   /// (Early) Reflections
		bool bDiffuseDecay;       /// Diffuse decay in RA simulations
		bool bSourceDirectivity;  /// Directivity for sources
		bool bMediumAbsorption;   /// Air attenuation
		bool bTemporalVariations; /// Turbulence
		bool bScattering;         /// Scattering
		bool bDiffraction;        /// Edge diffraction
		bool bDoppler;            /// Doppler shift
		bool bSpreadingLoss;      /// Spreading loss
		bool bSoundTransmission;  /// Sound transmission (through walls)
		bool bMaterialAbsorption; /// Absorption caused by materials and walls
	};
	/// Returns the overall auralization mode for a specific source-receiver pair
	AuralizationMode OverallAuralizationMode( CVASoundSourceState* pSourceState, CVAReceiverState* pReceiverState ) const;

	/// Represents a source-receiver pair in the rendering process. To be overloaded in derived renderers.
	class SourceReceiverPair : public CVAPoolObject
	{
	private:
		std::atomic<bool> bDelete      = false;   /// Indicates whether object is marked for deletion

	protected:
		CVARendererSource* pSource     = nullptr; /// Pointer to source
		CVARendererReceiver* pReceiver = nullptr; /// Pointer to receiver

	public:
		/// Pool-Constructor
		virtual void PreRequest( ) override;
		/// Pool-Destructor
		virtual void PreRelease( ) override;

		virtual void InitSourceAndReceiver( CVARendererSource* pSource_, CVARendererReceiver* pReceiver_ );

		bool IsDeleted( ) const { return bDelete; };
		void MarkDeleted( ) { bDelete = true; };

		CVARendererSource* GetSource( ) const { return pSource; };
		CVARendererReceiver* GetReceiver( ) const { return pReceiver; };
		
		/// Does the signal processing for this particular source-receiver pair and adds the result to the receiver's ITASampleFrame object.
		virtual void Process( double dTimeStamp, const AuralizationMode& oAuraMode, const CVASoundSourceState& oSourceState,
		                      const CVAReceiverState& oReceiverState ) = 0;
	};

	/// Returns the source-receiver pair (audio context) referring to the respective source and receiver ID. Return nullptr if IDs do not match
	SourceReceiverPair* GetSourceReceiverPair( int iSourceID, int iReceiverID ) const;
	/// Returns all source-receiver pairs of the current audio context. SourceReceiverPairs are Pool objects, so use this with care.
	std::vector<const SourceReceiverPair*> GetSourceReceiverPairs( ) const;

	const CVAAudioRendererInitParams m_oParams; /// Create a const copy of the init params


private:
	CVACoreImpl* m_pCore;                                  /// Pointer to VACore
	std::unique_ptr<ITADatasourceRealization> m_pdsOutput; /// Output datasource
	int m_iCurGlobalAuralizationMode;                      /// "Bitvector" containing the current global auralization modes

	const Config m_oConf; /// Renderer configuration parsed from VACore.ini (downcasted copy of child renderer)
	int m_iNumChannels;   /// Number of output channels


	bool m_bDumpReceivers;
	double m_dDumpReceiversGain;
	std::atomic<int> m_iDumpReceiversFlag;

	/// Message created during UpdateScene() including new/deleted sources and receivers
	struct CVARendererSceneUpdateMessage
	{
		std::list<CVARendererSource*> vNewSources;
		std::list<CVARendererSource*> vDelSources;
		std::list<CVARendererReceiver*> vNewReceivers;
		std::list<CVARendererReceiver*> vDelReceivers;
		std::list<SourceReceiverPair*> vNewSourceReceiverPairs;
		std::list<SourceReceiverPair*> vDelSourceReceiverPairs;
	};
	typedef std::shared_ptr<CVARendererSceneUpdateMessage> RendererSceneUpdateMessagePtr;
	RendererSceneUpdateMessagePtr m_pUpdateMessage;

	// Scene processing in user context. Is updated synchronously (see UpdateScene()).
	class UserContext
	{
	public:
		IVAObjectPool* m_pSourceReceiverPairPool                = nullptr; /// Pool for creation of source-receiver pairs
		IVAPoolObjectFactory* m_pSourceReceiverPairFactory = nullptr; /// Factory for creation of source-receiver pairs
		std::list<SourceReceiverPair*> m_lSourceReceiverPairs;             /// List of source-receiver pairs in user context (VACore calls)

		IVAObjectPool* m_pSourcePool   = nullptr;         /// Pool for source creation
		IVAObjectPool* m_pReceiverPool = nullptr;         /// Pool for receiver creation
		std::map<int, CVARendererSource*> m_mSources;     /// Map with sources in user context (using Source IDs as keys)
		std::map<int, CVARendererReceiver*> m_mReceivers; /// Map with receivers in user context (using Receiver IDs as keys)

		UserContext( const CVAAudioRendererInitParams& oParams ) : m_pCore( oParams.pCore ), m_sClass( oParams.sClass ), m_sRendererID( oParams.sID ) { };
		/// Deletes all entities of the user context (sources, receivers, ...)
		void Reset( );
		/// Updates user context data
		RendererSceneUpdateMessagePtr Update( CVASceneState* pNewSceneState );

	private:
		void UpdateSourceReceiverPairs( const CVASceneState* pCurScene, const CVASceneState* pNewScene, const CVASceneStateDiff* pDiff );
		CVARendererReceiver* CreateReceiver( int iID, const CVAReceiverState* );
		void DeleteReceiver( int iID );
		CVARendererSource* CreateSource( int iID, const CVASoundSourceState* );
		void DeleteSource( int iID );
		void CreateSourceReceiverPair( CVARendererSource*, CVARendererReceiver* );
		void DeleteSourceReceiverPair( SourceReceiverPair* );

		void UpdateTrajectories( );

		const CVACoreImpl* m_pCore;                               /// Pointer to VACore
		const std::string m_sClass;                               /// Renderer class name
		const std::string m_sRendererID;                          /// Renderer ID
		CVASceneState* m_pCurSceneState                = nullptr; /// Pointer to currently used scene state
		CVASceneState* m_pNewSceneState                = nullptr; /// Pointer to new scene state
		RendererSceneUpdateMessagePtr m_pUpdateMessage = nullptr; /// Shared pointer to currently generated update message which is used to sync with audio context
	} ctxUser;

	/// Enum indicating the current flag regarding a reset request
	enum class EResetFlag : unsigned char
	{
		NormalOperation   = 0,
		ResetRequest      = 1,
		ResetAcknowledged = 2
	};
	/// Scene data in context of audio process. Must be updated without interfering with the audio stream -> asynchronous update.
	class AudioContext
	{
	public:
		CVASceneState* m_pCurSceneState = nullptr;                                /// Current scene state
		tbb::concurrent_queue<RendererSceneUpdateMessagePtr> m_qpUpdateMessages;  /// Queue with update messages created from user context
		std::list<SourceReceiverPair*> m_lSourceReceiverPairs;                    /// List of source-receiver pairs in audio context
		std::list<CVARendererSource*> m_lSources;                                 /// List of sources in audio context
		std::list<CVARendererReceiver*> m_lReceivers;                             /// List of receivers in audio context
		ITASampleBuffer m_sbTemp;                                                 /// Temporally used buffer to store a block of samples during processing
		std::atomic<EResetFlag> m_eResetFlag;                                     /// Reset status flag: 0=NormalOperation, 1=ResetRequest, 2=ResetAcknowledged
		std::atomic<bool> m_bRunning;                                             /// Indicates whether audio stream is running

		AudioContext( const CVAAudioRendererInitParams& oParams ) : m_sClass( oParams.sClass ) { };
		/// Deletes all entities of the audio context (sources, receivers, ...)
		void Reset( );
		// Synchronize sources/receivers/... of user context with audio context based on the queued update messages
		void SyncSceneUpdates( );
		/// Updates positions and orientations of sources and receivers based on the underlying motion model and given time stamp
		void UpdateSourceAndReceiverPoses( double dTime );

	private:
		const std::string m_sClass; /// Renderer class name
	} ctxAudio;


	friend class CVAReceiverPoolFactory;
	friend class CVASourcePoolFactory;

};

#endif // IW_VACORE_AUDIORENDERERBASE
