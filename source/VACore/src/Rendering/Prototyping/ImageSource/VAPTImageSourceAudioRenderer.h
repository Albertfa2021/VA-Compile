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


#ifdef VACORE_WITH_RENDERER_PROTOTYPE_IMAGE_SOURCE

// VA includes
#	include "../../../Scene/VAScene.h"
#	include "../../../core/core.h"
#	include "../../VAAudioRenderer.h"
#	include "../../VAAudioRendererRegistry.h"

#	include <VA.h>
#	include <VAObjectPool.h>

// ITA includes
#	include <ITADataSourceRealization.h>
#	include <ITASampleBuffer.h>
#	include <ITASampleFrame.h>
#	include <ITAStreamInfo.h>

// 3rdParty Includes
#	include <tbb/concurrent_queue.h>

// STL Includes
#	include <list>
#	include <set>

// VA forwards
class CVASceneState;
class CVASceneStateDiff;
class CVASignalSourceManager;
class CVASoundSourceDesc;
class ITADatasourceRealization;

// Internal forwards
class CVAPTImageSourceSoundPath;
class CVAPTImageSourceSoundPathFactory;

//! Generic sound path audio renderer
/**
 * The generic sound path audio renderer is a convolution-only
 * engine that renders single input multiple channel impulse responses
 * using the sound source signals. For each sound path (source -> listener)
 * a convolution processor is created that can be feeded with an arbitrary IR
 * by the user, hence the name 'generic'.
 *
 */
class CVAPTImageSourceAudioRenderer
    : public IVAAudioRenderer
    , public ITADatasourceRealizationEventHandler
{
public:
	CVAPTImageSourceAudioRenderer( const CVAAudioRendererInitParams& oParams );
	virtual ~CVAPTImageSourceAudioRenderer( );
	void Reset( );
	inline void LoadScene( const std::string& ) { };
	void UpdateScene( CVASceneState* );
	void UpdateGlobalAuralizationMode( int );
	ITADatasource* GetOutputDatasource( );

	void HandleProcessStream( ITADatasourceRealization*, const ITAStreamInfo* );
	void HandlePostIncrementBlockPointer( ITADatasourceRealization* ) { };

	void SetParameters( const CVAStruct& );
	CVAStruct GetParameters( const CVAStruct& ) const;

	std::string HelpText( ) const;


protected:
	//! Internal source representation
	class CVAPTGPSource : public CVAPoolObject
	{
	public:
		CVASoundSourceDesc* pData; //!< (Unversioned) Source description
		bool bDeleted;
	};


	//! Internal listener representation
	class CVAPTGPListener : public CVAPoolObject
	{
	public:
		CVACoreImpl* pCore;
		CVASoundReceiverDesc* pData; //!< (Unversioned) Listener description
		bool bDeleted;
		ITASampleFrame* psfOutput;
	};

private:
	void UpdateFilter( ); // Update the IR filter and sound for changes to the internal IS parameters are made

	void CalculateImageSourceImpulseResponse( ITASampleBuffer& sbIRL, ITASampleBuffer& sbIRR, CVAPTImageSourceSoundPath* pPath );
	void CartVec2PolarVec( VAVec3 in, VAVec3* out );
	void SetIRFilterLength( );

	const CVAAudioRendererInitParams m_oParams; //!< Create a const copy of the init params

	CVACoreImpl* m_pCore; //!< Pointer to VACore

	CVASceneState* m_pCurSceneState;
	CVASceneState* m_pNewSceneState;

	int m_iCurGlobalAuralizationMode;

	IVAObjectPool* m_pSoundPathPool;
	CVAPTImageSourceSoundPathFactory* m_pSoundPathFactory;
	std::list<CVAPTImageSourceSoundPath*> m_lSoundPaths; //!< List of sound paths in user context (VACore calls)

	IVAObjectPool* m_pSourcePool;
	IVAObjectPool* m_pListenerPool;

	std::map<int, CVAPTGPSource*> m_mSources;     //!< Internal list of sources
	std::map<int, CVAPTGPListener*> m_mListeners; //!< Internal list of listener

	int m_iIRFilterLengthSamples;                   //!< Length of the HRIR filter DSP module
	int m_iNumChannels;                             //!< Number of channels per sound path
	double m_RoomLength, m_RoomWidth, m_RoomHeight; // The dimensions of the shoebox room used for the image source IR calculation
	double m_beta[6];                               // room reflection coefficients
	int m_MaxOrder;                                 // The maximum order of image source computed
	bool m_direct_sound;                            // set to true to include 0th order (direct sound) image source, or false to disregard
	int m_HRIR_length;                              // set the the length of the HRTF used
	int m_iIRFilterLengthSamples_ref;               // reference value for user input
	bool m_bOutputMonitoring;                       //!< Shows output infos / warnings if the overall listener output is zero (no filter loaded)
	ITADatasourceRealization* m_pOutput;
	ITASampleBuffer m_sfTempBuffer;

	double m_dRoomVolume;


	class CVAPTGPUpdateMessage : public CVAPoolObject
	{
	public:
		std::list<CVAPTGPSource*> vNewSources;
		std::list<CVAPTGPSource*> vDelSources;
		std::list<CVAPTGPListener*> vNewListeners;
		std::list<CVAPTGPListener*> vDelListeners;
		std::list<CVAPTImageSourceSoundPath*> vNewPaths;
		std::list<CVAPTImageSourceSoundPath*> vDelPaths;

		inline void PreRequest( )
		{
			vNewSources.clear( );
			vDelSources.clear( );
			vNewListeners.clear( );
			vDelListeners.clear( );
			vNewPaths.clear( );
			vDelPaths.clear( );
		};
	};

	IVAObjectPool* m_pUpdateMessagePool; // really necessary?
	CVAPTGPUpdateMessage* m_pUpdateMessage;

	//! Data in context of audio process
	struct
	{
		tbb::concurrent_queue<CVAPTGPUpdateMessage*> m_qpUpdateMessages; //!< Update messages list
		std::list<CVAPTImageSourceSoundPath*> m_lSoundPaths;             //!< List of sound paths
		std::list<CVAPTGPSource*> m_lSources;                            //!< List of sources
		std::list<CVAPTGPListener*> m_lListener;                         //!< List of listeners
		std::atomic<int> m_iResetFlag;                                   //!< Reset status flag: 0=normal_op, 1=reset_request, 2=reset_ack
		std::atomic<int> m_iStatus;                                      //!< Current status flag: 0=stopped, 1=running
	} ctxAudio;

	void Init( const CVAStruct& );

	void ManageSoundPaths( const CVASceneState* pCurScene, const CVASceneState* pNewScene, const CVASceneStateDiff* pDiff );
	void UpdateSources( );
	CVAPTGPListener* CreateListener( int iID, const CVAReceiverState* );
	void DeleteListener( int iID );
	CVAPTGPSource* CreateSource( int iID, const CVASoundSourceState* );
	void DeleteSource( int iID );
	CVAPTImageSourceSoundPath* CreateSoundPath( CVAPTGPSource*, CVAPTGPListener* );
	void DeleteSoundPath( CVAPTImageSourceSoundPath* );

	void SyncInternalData( );
	void ResetInternalData( );

	void UpdateGenericSoundPath( int iListenerID, int iSourceID, const std::string& sIRFilePath );
	void UpdateGenericSoundPath( int iListenerID, int iSourceID, int iChannel, const std::string& sIRFilePath );
	void UpdateGenericSoundPath( int iListenerID, int iSourceID, ITASampleFrame& sfIR );
	void UpdateGenericSoundPath( int iListenerID, int iSourceID, int iChannel, ITASampleBuffer& sbIR );
	void UpdateGenericSoundPath( const int iListenerID, const int iSourceID, const double dDelaySeconds );

	friend class CVAPTImageSourceSoundPath;
	friend class CVAPTISListenerPoolFactory;
	friend class CVAPTISSourcePoolFactory;

	//! Not for use, avoid C4512
	inline CVAPTImageSourceAudioRenderer operator=( const CVAPTImageSourceAudioRenderer& ) { VA_EXCEPT_NOT_IMPLEMENTED; };
};

#endif // VACORE_WITH_RENDERER_PROTOTYPE_IMAGE_SOURCES
