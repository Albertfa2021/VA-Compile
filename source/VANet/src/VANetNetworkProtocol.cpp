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

#include "VANetNetworkProtocol.h"

#include "VANetClientImpl.h"
#include "VANetMessage.h"
#include "VANetServerImpl.h"
#include "VANetVistaCompatibility.h"

#include <VA.h>
#include <VistaBase/VistaExceptionBase.h>
#include <VistaInterProcComm/Connections/VistaByteBufferDeSerializer.h>
#include <VistaInterProcComm/Connections/VistaByteBufferSerializer.h>
#include <VistaInterProcComm/Connections/VistaConnectionIP.h>
#include <assert.h>
#include <sstream>


//////////////////////////////////////////////////
//////////       Main Implementation     /////////
//////////////////////////////////////////////////

CVANetNetworkProtocol::CVANetNetworkProtocol( )
    : m_pRealCore( NULL )
    , m_pServer( NULL )
    , m_pClient( NULL )
    , m_pLastAnswerConnection( NULL )
    , m_pCommandChannel( NULL )
    , m_pHeadChannel( NULL )
    , m_oIncomingEvent( )
    , m_vecEventBuffer( 128 )
    , m_iReceiveTimeout( 0 )
    , m_iExceptionHandlingMode( IVANetClient::EXC_CLIENT_THROW )
    , m_bBufferingActive( false )
    , m_bBufferSynchronisedModification( false )
    , m_pMessage( NULL )
    , m_pBufferedMessage( NULL )
    , m_nCurrentMessageMode( MESSAGE_WITH_ANSWER )
    , m_pEventMessage( NULL )
{
}

CVANetNetworkProtocol::~CVANetNetworkProtocol( )
{
	if( m_pLastAnswerConnection )
		m_pLastAnswerConnection->WaitForSendFinish( );

	delete m_pMessage;
	delete m_pBufferedMessage;
}

bool CVANetNetworkProtocol::InitializeAsClient( CVANetClientImpl* pClient, VistaConnectionIP* pCommandChannel, VistaConnectionIP* pHeadChannel,
                                                const int iExceptionhandlingmode, const bool bBufferSynchronisedModification )
{
	m_iExceptionHandlingMode          = iExceptionhandlingmode;
	m_bBufferSynchronisedModification = bBufferSynchronisedModification;
	m_pClient                         = pClient;
	m_pCommandChannel                 = pCommandChannel;
	m_pHeadChannel                    = pHeadChannel;

	m_pMessage         = new CVANetMessage( m_pCommandChannel->GetByteorderSwapFlag( ) );
	m_pBufferedMessage = new CVANetMessage( m_pCommandChannel->GetByteorderSwapFlag( ) );
	m_pEventMessage    = new CVANetMessage( m_pCommandChannel->GetByteorderSwapFlag( ) );

	SetExceptionHandlingMode( m_iExceptionHandlingMode );
	return true;
}

bool CVANetNetworkProtocol::InitializeAsServer( CVANetServerImpl* pServer )
{
	m_pServer = pServer;

	m_pMessage      = new CVANetMessage( VistaSerializingToolset::DOES_NOT_SWAP_MULTIBYTE_VALUES );
	m_pEventMessage = new CVANetMessage( VistaSerializingToolset::DOES_NOT_SWAP_MULTIBYTE_VALUES );
	return true;
}

void CVANetNetworkProtocol::SetExceptionHandlingMode( const int nMode )
{
	m_iExceptionHandlingMode = nMode;
}

int CVANetNetworkProtocol::GetExceptionHandlingMode( ) const
{
	return m_iExceptionHandlingMode;
}

void CVANetNetworkProtocol::SetRealVACore( IVAInterface* pCore )
{
	m_pRealCore = pCore;
}

#define SERVER_FUNCTION_MAPPING( ID, FUNCTION ) \
	case( ID ):                                 \
	{                                           \
		FUNCTION;                               \
		break;                                  \
	}
//#define SERVER_FUNCTION_MAPPING(ID,FUNCTION) case (ID): { std::cout << "Processing " << #ID << std::endl; FUNCTION; break; }

void CVANetNetworkProtocol::ServerCallFunctionByMessageType( const int nMessageType, VistaConnectionIP* pConnection )
{
	switch( nMessageType )
	{
		SERVER_FUNCTION_MAPPING( VA_NP_GET_VERSION_INFO, ServerGetVersionInfo( ) );

		// Base functions
		SERVER_FUNCTION_MAPPING( VA_NP_GET_STATE, ServerGetState( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_INITIALIZE, ServerInitialize( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_FINALIZE, ServerFinalize( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_RESET, ServerReset( ) );

		// Module interface
		SERVER_FUNCTION_MAPPING( VA_NP_GET_MODULES, ServerGetModules( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_CALL_MODULE, ServerCallModule( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SEARCH_PATHS, ServerGetSearchPaths( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_FIND_FILE_PATH, ServerFindFilePath( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_CORE_CONFIGURATION, ServerGetCoreConfiguration( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_HARDWARE_CONFIGURATION, ServerGetHardwareConfiguration( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_FILE_LIST, ServerGetFileList( ) );

		// Event handling
		SERVER_FUNCTION_MAPPING( VA_NP_ATTACH_EVENT_HANDLER, ServerAttachEventHandler( pConnection ) );
		SERVER_FUNCTION_MAPPING( VA_NP_DETACH_EVENT_HANDLER, ServerDetachEventHandler( pConnection ) );

		// Directivities
		SERVER_FUNCTION_MAPPING( VA_NP_CREATE_DIRECTIVITY, ServerCreateDirectivityFromParameters( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_DELETE_DIRECTIVITY, ServerDeleteDirectivity( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_DIRECTIVITY_INFO, ServerGetDirectivityInfo( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_DIRECTIVITY_INFOS, ServerGetDirectivityInfos( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_DIRECTIVITY_PARAMETERS, ServerGetDirectivityParameters( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_DIRECTIVITY_PARAMETERS, ServerSetDirectivityParameters( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_DIRECTIVITY_NAME, ServerGetDirectivityName( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_DIRECTIVITY_NAME, ServerSetDirectivityName( ) );

		// Signal sources
		SERVER_FUNCTION_MAPPING( VA_NP_CREATE_SIGNAL_SOURCE_BUFFER_FROM_PARAMETERS, ServerCreateSignalSourceBufferFromParameters( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_CREATE_SIGNAL_SOURCE_TEXT_TO_SPEECH, ServerCreateTextToSpeechSignalSource( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_CREATE_SIGNAL_SOURCE_SEQUENCER, ServerCreateSequencerSignalSource( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_CREATE_SIGNAL_SOURCE_NETWORK_STREAM, ServerCreateNetworkStreamSignalSource( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_CREATE_SIGNAL_SOURCE_ENGINE, ServerCreateEngineSignalSource( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_CREATE_SIGNAL_SOURCE_MACHINE, ServerCreateSignalSourceMachine( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_CREATE_SIGNAL_SOURCE_PROTOTYPE_FROM_PARAMETERS, ServerCreateSignalSourcePrototypeFromParameters( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_DELETE_SIGNALSOURCE, ServerDeleteSignalSource( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SIGNALSOURCE_INFO, ServerGetSignalSourceInfo( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SIGNALSOURCE_INFOS, ServerGetSignalSourceInfos( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SIGNAL_SOURCE_BUFFER_PLAYBACK_STATE, ServerGetSignalSourceBufferPlaybackState( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SIGNAL_SOURCE_BUFFER_PLAYBACK_ACTION, ServerSetSignalSourceBufferPlaybackAction( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SIGNAL_SOURCE_BUFFER_PLAYBACK_POSITION, ServerSetAudiofileSignalSourcePlaybackPosition( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SIGNAL_SOURCE_BUFFER_LOOPING, ServerSetSignalSourceBufferLooping( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SIGNAL_SOURCE_BUFFER_LOOPING, ServerGetSignalSourceBufferLooping( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SIGNAL_SOURCE_PARAMETERS, ServerGetSignalSourceParameters( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SIGNALSOURCE_PARAMETERS, ServerSetSignalSourceParameters( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_ADD_SIGNAL_SOURCE_SEQUENCER_SAMPLE, ServerAddSignalSourceSequencerSample( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_ADD_SIGNAL_SOURCE_SEQUENCER_PLAYBACK, ServerAddSignalSourceSequencerPlayback( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_REMOVE_SIGNAL_SOURCE_SEQUENCER_SAMPLE, ServerRemoveSignalSourceSequencerSample( ) );

		// Synchronization functions
		SERVER_FUNCTION_MAPPING( VA_NP_LOCK_UPDATE, ServerLockUpdate( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_UNLOCK_UPDATE, ServerUnlockUpdate( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_UPDATE_LOCKED, ServerGetUpdateLocked( ) );

		// Sound sources
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_SOURCE_IDS, ServerGetSoundSourceIDs( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_CREATE_SOUND_SOURCE, ServerCreateSoundSource( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_CREATE_SOUND_SOURCE_EXPLICIT_RENDERER, ServerCreateSoundSourceExplicitRenderer( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_DELETE_SOUND_SOURCE, ServerDeleteSoundSource( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SOUND_SOURCE_ENABLED, ServerSetSoundSourceEnabled( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_SOURCE_ENABLED, ServerGetSoundSourceEnabled( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SOUND_SOURCE_NAME, ServerSetSoundSourceName( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_SOURCE_NAME, ServerGetSoundSourceName( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SOUND_SOURCE_SIGNALSOURCE, ServerSetSoundSourceSignalSource( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_SOURCE_SIGNALSOURCE, ServerGetSoundSourceSignalSource( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SOUND_SOURCE_AURAMODE, ServerSetSoundSourceAuralizationMode( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_SOURCE_AURAMODE, ServerGetSoundSourceAuralizationMode( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SOUND_SOURCE_PARAMETERS, ServerSetSoundSourceParameters( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_SOURCE_PARAMETERS, ServerGetSoundSourceParameters( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SOUND_SOURCE_DIRECTIVITY, ServerSetSoundSourceDirectivity( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_SOURCE_DIRECTIVITY, ServerGetSoundSourceDirectivity( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SOUND_SOURCE_SOUND_POWER, ServerSetSoundSourceSoundPower( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_SOURCE_SOUND_POWER, ServerGetSoundSourceSoundPower( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SOUND_SOURCE_MUTED, ServerSetSoundSourceMuted( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_SOURCE_MUTED, ServerGetSoundSourceMuted( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SOUND_SOURCE_POSE, ServerSetSoundSourcePose( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_SOURCE_POSE, ServerGetSoundSourcePose( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SOUND_SOURCE_POSITION, ServerSetSoundSourcePosition( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_SOURCE_POSITION, ServerGetSoundSourcePosition( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SOUND_SOURCE_ORIENTATION, ServerSetSoundSourceOrientation( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_SOURCE_ORIENTATION, ServerGetSoundSourceOrientation( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SOUND_SOURCE_ORIENTATION_VU, ServerSetSoundSourceOrientationVU( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_SOURCE_ORIENTATION_VU, ServerGetSoundSourceOrientationVU( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_SOURCE_INFO, ServerGetSoundSourceInfo( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_SOURCE_GEOMETRY_MESH, ServerGetSoundSourceGeometryMesh( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SOUND_SOURCE_GEOMETRY_MESH, ServerGetSoundSourceGeometryMesh( ) );

		// SoundReceivers
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_RECEIVER_IDS, ServerGetSoundReceiverIDs( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_RECEIVER_INFO, ServerGetSoundReceiverInfo( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_CREATE_SOUND_RECEIVER, ServerCreateSoundReceiver( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_CREATE_SOUND_RECEIVER_EXPLICIT_RENDERER, ServerCreateSoundReceiverExplicitRenderer( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_DELETE_SOUND_RECEIVER, ServerDeleteSoundReceiver( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SOUND_RECEIVER_ENABLED, ServerSetSoundReceiverEnabled( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_RECEIVER_ENABLED, ServerGetSoundReceiverEnabled( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_RECEIVER_NAME, ServerGetSoundReceiverName( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SOUND_RECEIVER_NAME, ServerSetSoundReceiverName( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_RECEIVER_AURALIZATION_MODE, ServerGetSoundReceiverAuralizationMode( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SOUND_RECEIVER_AURALIZATION_MODE, ServerSetSoundReceiverAuralizationMode( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SOUND_RECEIVER_PARAMETERS, ServerSetSoundReceiverParameters( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_RECEIVER_PARAMETERS, ServerGetSoundReceiverParameters( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_RECEIVER_DIRECTIVITY, ServerGetSoundReceiverDirectivity( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SOUND_RECEIVER_DIRECTIVITY, ServerSetSoundReceiverDirectivity( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SOUND_RECEIVER_POSE, ServerSetSoundReceiverPose( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_RECEIVER_POSE, ServerGetSoundReceiverPose( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SOUND_RECEIVER_POSITION, ServerSetSoundReceiverPosition( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_RECEIVER_POSITION, ServerGetSoundReceiverPosition( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SOUND_RECEIVER_ORIENTATION, ServerSetSoundReceiverOrientation( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_RECEIVER_ORIENTATION, ServerGetSoundReceiverOrientation( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SOUND_RECEIVER_ORIENTATION_VU, ServerSetSoundReceiverOrientationVU( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_RECEIVER_ORIENTATION_VU, ServerGetSoundReceiverOrientationVU( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SOUND_RECEIVER_HEAD_ABOVE_TORSO_ORIENTATION, ServerSetSoundReceiverHeadAboveTorsoOrientation( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_RECEIVER_HEAD_ABOVE_TORSO_ORIENTATION, ServerGetSoundReceiverHeadAboveTorsoOrientation( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SOUND_RECEIVER_REAL_WORLD_POSITION_ORIENTATION_VU, ServerSetSoundReceiverRealWorldPositionOrientationVU( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_RECEIVER_REAL_WORLD_POSITION_ORIENTATION_VU, ServerGetSoundReceiverRealWorldPositionOrientationVU( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SOUND_RECEIVER_GEOMETRY_MESH, ServerSetSoundReceiverGeometryMesh( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_RECEIVER_GEOMETRY_MESH, ServerGetSoundReceiverGeometryMesh( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SOUND_RECEIVER_REAL_WORLD_POSE, ServerSetSoundReceiverRealWorldPose( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_RECEIVER_REAL_WORLD_POSE, ServerGetSoundReceiverRealWorldPose( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SOUND_RECEIVER_REAL_WORLD_TORSO_ORIENTATION, ServerSetSoundReceiverRealWorldTorsoOrientation( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_RECEIVER_REAL_WORLD_TORSO_ORIENTATION, ServerGetSoundReceiverRealWorldTorsoOrientation( ) );

		// Scene
		SERVER_FUNCTION_MAPPING( VA_NP_SCENE_CREATE, ServerCreateScene( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SCENE_IDS, ServerGetSceneIDs( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SCENE_INFO, ServerGetSceneInfo( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SCENE_NAME, ServerGetSceneName( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SCENE_NAME, ServerGetSceneName( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SCENE_ENABLED, ServerGetSceneEnabled( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SCENE_ENABLED, ServerGetSceneEnabled( ) );

		SERVER_FUNCTION_MAPPING( VA_NP_CREATE_SOUND_PORTAL, ServerCreateSoundPortal( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_PORTAL_NAME, ServerGetSoundPortalName( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SOUND_PORTAL_NAME, ServerSetSoundPortalName( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_PORTAL_IDS, ServerGetSoundPortalIDs( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_PORTAL_INFO, ServerGetSoundPortalInfo( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_PORTAL_PARAMETERS, ServerGetSoundPortalParameters( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SOUND_PORTAL_PARAMETERS, ServerSetSoundPortalParameters( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_PORTAL_POSITION, ServerGetSoundPortalPosition( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SOUND_PORTAL_POSITION, ServerSetSoundPortalPosition( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_PORTAL_ORIENTATION, ServerGetSoundPortalOrientation( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SOUND_PORTAL_ORIENTATION, ServerSetSoundPortalOrientation( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_PORTAL_ENABLED, ServerGetSoundPortalEnabled( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SOUND_PORTAL_ENABLED, ServerSetSoundPortalEnabled( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_PORTAL_MATERIAL, ServerGetSoundPortalMaterial( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SOUND_PORTAL_MATERIAL, ServerSetSoundPortalMaterial( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_PORTAL_NEXT_SOUND_PORTAL, ServerGetSoundPortalNextPortal( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SOUND_PORTAL_NEXT_SOUND_PORTAL, ServerSetSoundPortalNextPortal( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_PORTAL_SOUND_SOURCE, ServerGetSoundPortalSoundSource( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SOUND_PORTAL_SOUND_SOURCE, ServerSetSoundPortalSoundSource( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_SOUND_PORTAL_SOUND_RECEIVER, ServerGetSoundPortalSoundReceiver( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_SOUND_PORTAL_SOUND_RECEIVER, ServerSetSoundPortalSoundReceiver( ) );

		// Geometry meshes
		SERVER_FUNCTION_MAPPING( VA_NP_CREATE_GEOMETRY_MESH, ServerCreateGeometryMesh( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_CREATE_GEOMETRY_MESH_FROM_PARAMETERS, ServerCreateGeometryMeshFromParameters( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_DELETE_GEOMETRY_MESH, ServerDeleteGeometryMesh( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_GEOMETRY_MESH, ServerGetGeometryMesh( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_GEOMETRY_MESH_IDS, ServerGetGeometryMeshIDs( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_GEOMETRY_MESH_PARAMETERS, ServerGetGeometryMeshParameters( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_GEOMETRY_MESH_PARAMETERS, ServerSetGeometryMeshParameters( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_GEOMETRY_MESH_NAME, ServerGetGeometryMeshName( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_GEOMETRY_MESH_NAME, ServerSetGeometryMeshEnabled( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_GEOMETRY_MESH_ENABLED, ServerGetGeometryMeshEnabled( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_GEOMETRY_MESH_ENABLED, ServerSetGeometryMeshEnabled( ) );

		// Acoustic materials
		SERVER_FUNCTION_MAPPING( VA_NP_CREATE_ACOUSTIC_MATERIAL, ServerCreateAcousticMaterial( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_CREATE_ACOUSTIC_MATERIAL_FROM_PARAMETERS, ServerCreateAcousticMaterialFromParameters( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_DELETE_ACOUSTIC_MATERIAL, ServerDeleteAcousticMaterial( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_ACOUSTIC_MATERIAL_INFO, ServerGetAcousticMaterialInfo( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_ACOUSTIC_MATERIAL_INFOS, ServerGetAcousticMaterialInfos( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_ACOUSTIC_MATERIAL_NAME, ServerGetAcousticMaterialName( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_ACOUSTIC_MATERIAL_NAME, ServerSetAcousticMaterialName( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_ACOUSTIC_MATERIAL_PARAMETERS, ServerGetAcousticMaterialParameters( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_ACOUSTIC_MATERIAL_PARAMETERS, ServerSetAcousticMaterialParameters( ) );

		// Medium
		SERVER_FUNCTION_MAPPING( VA_NP_GET_HOMOGENEOUS_MEDIUM_SOUND_SPEED, ServerGetHomogeneousMediumSoundSpeed( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_HOMOGENEOUS_MEDIUM_SOUND_SPEED, ServerSetHomogeneousMediumSoundSpeed( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_HOMOGENEOUS_MEDIUM_TEMPERATURE, ServerGetHomogeneousMediumTemperature( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_HOMOGENEOUS_MEDIUM_TEMPERATURE, ServerSetHomogeneousMediumTemperature( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_HOMOGENEOUS_MEDIUM_STATIC_PRESSURE, ServerGetHomogeneousMediumStaticPressure( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_HOMOGENEOUS_MEDIUM_STATIC_PRESSURE, ServerSetHomogeneousMediumStaticPressure( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_HOMOGENEOUS_MEDIUM_RELATIVE_HUMIDITY, ServerGetHomogeneousMediumRelativeHumidity( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_HOMOGENEOUS_MEDIUM_RELATIVE_HUMIDITY, ServerSetHomogeneousMediumRelativeHumidity( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_HOMOGENEOUS_MEDIUM_SHIFT_SPEED, ServerGetHomogeneousMediumShiftSpeed( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_HOMOGENEOUS_MEDIUM_SHIFT_SPEED, ServerSetHomogeneousMediumShiftSpeed( ) );
		;
		SERVER_FUNCTION_MAPPING( VA_NP_GET_HOMOGENEOUS_MEDIUM_PARAMETERS, ServerGetHomogeneousMediumParameters( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_HOMOGENEOUS_MEDIUM_PARAMETERS, ServerSetHomogeneousMediumParameters( ) );

		// Global functions
		SERVER_FUNCTION_MAPPING( VA_NP_GET_INPUT_GAIN, ServerGetInputGain( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_INPUT_GAIN, ServerSetInputGain( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_INPUT_MUTED, ServerGetInputMuted( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_INPUT_MUTED, ServerSetInputMuted( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_OUTPUT_GAIN, ServerGetOutputGain( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_OUTPUT_GAIN, ServerSetOutputGain( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_OUTPUT_MUTED, ServerGetOutputMuted( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_OUTPUT_MUTED, ServerSetOutputMuted( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_GLOBAL_AURALIZATION_MODE, ServerGetGlobalAuralizationMode( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_GLOBAL_AURALIZATION_MODE, ServerSetGlobalAuralizationMode( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_GET_CORE_CLOCK, ServerGetCoreClock( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SET_CORE_CLOCK, ServerSetCoreClock( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_SUBSTITUTE_MACROS, ServerSubstituteMacros( ) );

		SERVER_FUNCTION_MAPPING( VA_NP_RENDERER_GET_INFOS, ServerGetRenderingModuleInfos( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_RENDERER_GET_GAIN, ServerGetRenderingModuleGain( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_RENDERER_SET_GAIN, ServerSetRenderingModuleGain( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_RENDERER_GET_MUTED, ServerGetRenderingModuleMuted( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_RENDERER_SET_MUTED, ServerSetRenderingModuleMuted( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_RENDERER_GET_PARAMETERS, ServerGetRenderingModuleParameters( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_RENDERER_SET_PARAMETERS, ServerSetRenderingModuleParameters( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_RENDERER_GET_AURALIZATION_MODE, ServerGetRenderingModuleAuralizationMode( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_RENDERER_SET_AURALIZATION_MODE, ServerSetRenderingModuleAuralizationMode( ) );

		SERVER_FUNCTION_MAPPING( VA_NP_REPRODUCTION_GET_INFOS, ServerGetReproductionModuleInfos( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_REPRODUCTION_GET_GAIN, ServerGetReproductionModuleGain( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_REPRODUCTION_SET_GAIN, ServerSetReproductionModuleGain( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_REPRODUCTION_GET_MUTED, ServerGetReproductionModuleMuted( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_REPRODUCTION_SET_MUTED, ServerSetReproductionModuleMuted( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_REPRODUCTION_GET_PARAMETERS, ServerGetReproductionModuleParameters( ) );
		SERVER_FUNCTION_MAPPING( VA_NP_REPRODUCTION_SET_PARAMETERS, ServerSetReproductionModuleParameters( ) );

		case VA_NP_INVALID:
		default:
		{
			/** @todo: disconnect */
			std::ostringstream oStream;
			oStream << "VANetServer: Unkown Message Type " << nMessageType;
			VA_EXCEPT2( PROTOCOL_ERROR, oStream.str( ) );
		}
	}
}

bool CVANetNetworkProtocol::ProcessMessageFromClient( VistaConnectionIP* pConnection )
{
#ifdef VANET_NETWORK_COM_PRINT_DEBUG_INFOS
	std::cout << "[ VANetProtocol ][ DEBUG ] Processing message from client" << std::endl;
#endif

	assert( m_pRealCore != NULL );

	// just to avoid false connection notifies - these can happen
	if( pConnection->PendingDataSize( ) == 0 )
		return true;

	// Falls wir zuletzt eine Antwort gesendet haben, müssen wir nun warten,
	// bis das senden abgeschlossen ist, bevor wir die message leeren
	if( m_pLastAnswerConnection != NULL )
		m_pLastAnswerConnection->WaitForSendFinish( );

	try
	{
		m_pMessage->ResetMessage( );
		m_pMessage->SetConnection( pConnection );
		m_pMessage->ReadMessage( );
	}
	catch( VistaExceptionBase& oException )
	{
		std::cerr << "VANetNetworkProtocol: caught connection exception - " << oException.GetExceptionText( ) << std::endl;
		return false;
	}

	int nMessageType = m_pMessage->GetMessageType( );

	// we assume an answer - if an exception occurs, or no answer is
	// sent, the value will be rewritten
	m_pMessage->SetAnswerType( VA_NP_SPECIAL_ANSWER );

	// Es kann eine gebufferete Nachricht kommen.
	// In dem fall müssen wir mehrfach Befehle parsen
	// dies tun wir, bis der MessageBuffer leer ist
	try
	{
		if( nMessageType == VA_NP_SPECIAL_BUFFERED_MESSAGE )
		{
			while( m_pMessage->GetIncomingMessageSize( ) > 0 )
			{
				nMessageType = m_pMessage->ReadInt( );
				ServerCallFunctionByMessageType( nMessageType, pConnection );
			}
		}
		else if( nMessageType == VA_NP_SPECIAL_CLIENT_EVENT )
		{
			int nEventID = m_pMessage->ReadInt( );
			m_pServer->HandleClientEvent( nEventID, pConnection );
			return true;
		}
		else
		{
			ServerCallFunctionByMessageType( nMessageType, pConnection );
		}
	}
	catch( CVAException& oException )
	{
		if( oException.GetErrorCode( ) == CVAException::PROTOCOL_ERROR )
			throw;

		switch( m_pMessage->GetExceptionMode( ) )
		{
			case IVANetClient::EXC_CLIENT_THROW:
			{
				m_pMessage->SetAnswerType( VA_NP_SPECIAL_EXCEPTION );
				m_pMessage->WriteException( oException );
				m_pMessage->WriteAnswer( );
				return true;
			}
			case IVANetClient::EXC_SERVER_PRINT:
			{
				std::cerr << "[IVANetServer]: Caught and swallowed exception :\n" << oException.ToString( ) << std::endl;
				return true;
			}
		}
	}

	if( m_pMessage->GetOutgoingMessageHasData( ) )
	{
		m_pMessage->WriteAnswer( );
	}
	else if( m_pMessage->GetExceptionMode( ) == IVANetClient::EXC_CLIENT_THROW )
	{
		m_pMessage->SetAnswerType( VA_NP_SPECIAL_NO_EXCEPTION );
		m_pMessage->WriteAnswer( );
	}

#ifdef VANET_NETWORK_COM_PRINT_DEBUG_INFOS
	std::cout << "[ VANetProtocol ][ DEBUG ] Processing has successfully processed messagew from client" << std::endl;
#endif

	return true;
}

bool CVANetNetworkProtocol::GetBufferSynchronisedModification( ) const
{
	return m_bBufferSynchronisedModification;
}

void CVANetNetworkProtocol::SetBufferSynchronisedModifications( const bool bSet )
{
	m_bBufferSynchronisedModification = bSet;
}

void CVANetNetworkProtocol::ClientPrepareMessageBuffering( )
{
	if( m_bBufferSynchronisedModification == false )
		return;

	// @todo: how to handle this? ignore, still buffer command, abort?
	if( m_bBufferingActive == true )
		return; // we are already buffering

	m_pBufferedMessage->ResetMessage( );
	m_pBufferedMessage->SetConnection( m_pCommandChannel );
	m_pBufferedMessage->SetExceptionMode( IVANetClient::EXC_CLIENT_THROW );
	m_pBufferedMessage->SetMessageType( VA_NP_SPECIAL_BUFFERED_MESSAGE );

	m_bBufferingActive = true;
}

int CVANetNetworkProtocol::ClientEndMessageBuffering( )
{
	assert( m_bBufferingActive );

	m_bBufferingActive = false;

	int iRet = -1;
	try
	{
		m_pBufferedMessage->SetExceptionMode( m_iExceptionHandlingMode );
		m_pBufferedMessage->WriteMessage( );

		m_pBufferedMessage->ReadAnswer( );
		iRet = m_pBufferedMessage->ReadInt( );
	}
	catch( const VistaExceptionBase& oException )
	{
		std::cerr << "VANetNetworkProtocol: caught exception during ClientEndMessageBuffering - " << oException.GetExceptionText( ) << std::endl;
	}

	return iRet;
}

int CVANetNetworkProtocol::ClientProcessEventMessageFromServer( VistaConnectionIP* pConnection, std::list<IVAEventHandler*> liCoreEventHandler )
{
	m_pEventMessage->SetConnection( pConnection );

	try
	{
		m_pEventMessage->ReadMessage( );

		switch( m_pEventMessage->GetMessageType( ) )
		{
			case VA_EVENT_CORE_EVENT:
			{
				CVAEvent oEvent = m_pEventMessage->ReadEvent( );
				for( std::list<IVAEventHandler*>::iterator itHandler = liCoreEventHandler.begin( ); itHandler != liCoreEventHandler.end( ); ++itHandler )
				{
					( *itHandler )->HandleVAEvent( &oEvent );
				}
				break;
			}
			case VA_EVENT_NET_EVENT:
			{
				m_pClient->ProcessNetEvent( m_pEventMessage->ReadInt( ) );
				break;
			}
			default:
			{
				VA_EXCEPT2( PROTOCOL_ERROR, std::string( "Unknown event message type" ) );
			}
		}
	}
	catch( VistaExceptionBase& oException )
	{
		VA_EXCEPT2( PROTOCOL_ERROR, std::string( "VANetNetworkProtocol: caught connection exception - " + oException.GetExceptionText( ) ) );
	}

	return m_pEventMessage->GetMessageType( );
}


void CVANetNetworkProtocol::ServerPrepareEventMessage( const CVAEvent* pEvent, CVANetMessage* pMessage )
{
	pMessage->ResetMessage( );
	pMessage->SetMessageType( VA_EVENT_CORE_EVENT );
	pMessage->WriteEvent( *pEvent );
}

void CVANetNetworkProtocol::HandleConnectionClose( VistaConnectionIP* pConnection )
{
	if( m_pLastAnswerConnection == pConnection )
		m_pLastAnswerConnection = NULL;
	if( m_pMessage->GetConnection( ) == pConnection )
		m_pMessage->ClearConnection( );
}

CVANetMessage* CVANetNetworkProtocol::ServerGetMessage( )
{
	return m_pMessage;
}
void CVANetNetworkProtocol::ServerSendAnswer( CVANetMessage* )
{
	// nothing to do
}

CVANetMessage* CVANetNetworkProtocol::ClientInitMessage( int nCommandId, MESSAGE_MODE eMessageMode )
{
	m_nCurrentMessageMode = eMessageMode;
	if( eMessageMode == MESSAGE_ALLOWS_BUFFERING && m_bBufferingActive )
	{
		m_pBufferedMessage->WriteInt( nCommandId );
		return m_pBufferedMessage;
	}
	else
	{
		m_pMessage->ResetMessage( );
		m_pMessage->SetConnection( m_pCommandChannel );
		m_pMessage->SetMessageType( nCommandId );
		return m_pMessage;
	}
}
void CVANetNetworkProtocol::ClientSendCommand( CVANetMessage* pMessage )
{
#ifdef VANET_NETWORK_COM_PRINT_DEBUG_INFOS
	std::cout << "[ VANetMessage ][ DEBUG ] Client is trying to send a commad now" << std::endl;
#endif

	if( pMessage != m_pBufferedMessage )
	{
		int nExceptionMode = -1;
		switch( m_nCurrentMessageMode )
		{
			case MESSAGE_WITH_ANSWER:
			case MESSAGE_ENFORCED_EXCEPTION:
			{
				nExceptionMode = IVANetClient::EXC_CLIENT_THROW;
				break;
			}
			case MESSAGE_CONDITIONAL_EXCEPTION:
			case MESSAGE_ALLOWS_BUFFERING:
			{
				nExceptionMode = m_iExceptionHandlingMode;
				break;
			}
		}

		try
		{
#ifdef VANET_NETWORK_COM_PRINT_DEBUG_INFOS
			std::cout << "[ VANetMessage ][ DEBUG ] Client is contacting the server" << std::endl;
#endif

			pMessage->SetExceptionMode( nExceptionMode );
			pMessage->WriteMessage( );

#ifdef VANET_NETWORK_COM_PRINT_DEBUG_INFOS
			std::cout << "[ VANetMessage ][ DEBUG ] Client sent the request and is waiting for a respond" << std::endl;
#endif

			if( m_nCurrentMessageMode == MESSAGE_WITH_ANSWER )
			{
				pMessage->ReadAnswer( );
				if( pMessage->GetAnswerType( ) == VA_NP_SPECIAL_EXCEPTION )
				{
					throw( pMessage->ReadException( ) );
				}
				else
				{
					assert( pMessage->GetAnswerType( ) == VA_NP_SPECIAL_ANSWER );
				}
			}
			else if( nExceptionMode == IVANetClient::EXC_CLIENT_THROW )
			{
				pMessage->ReadAnswer( );
				if( pMessage->GetAnswerType( ) == VA_NP_SPECIAL_EXCEPTION )
				{
					throw( pMessage->ReadException( ) );
				}
				else
				{
					assert( pMessage->GetAnswerType( ) == VA_NP_SPECIAL_NO_EXCEPTION );
				}
			}

#ifdef VANET_NETWORK_COM_PRINT_DEBUG_INFOS
			std::cout << "[ VANetMessage ][ DEBUG ] Client received answer message, no exception on server side detected" << std::endl;
#endif
		}
		catch( const VistaExceptionBase& )
		{
			//@todo
		}
	}
}

void CVANetNetworkProtocol::ClientGetVersionInfo( CVAVersionInfo* pVersionInfo )
{
	if( !pVersionInfo )
		return;
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_VERSION_INFO, MESSAGE_WITH_ANSWER );
	ClientSendCommand( pMsg );

	*pVersionInfo = pMsg->ReadVersionInfo( );
}

void CVANetNetworkProtocol::ServerGetVersionInfo( )
{
	CVANetMessage* pMsg = ServerGetMessage( );

	CVAVersionInfo oVersionInfo;
	m_pRealCore->GetVersionInfo( &oVersionInfo );

	pMsg->WriteVersionInfo( oVersionInfo );
}

int CVANetNetworkProtocol::ClientGetState( )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_STATE, MESSAGE_WITH_ANSWER );
	ClientSendCommand( pMsg );

	return pMsg->ReadInt( );
}

void CVANetNetworkProtocol::ServerGetState( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	int iState          = m_pRealCore->GetState( );
	pMsg->WriteInt( iState );
}

void CVANetNetworkProtocol::ClientInitialize( )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_INITIALIZE, MESSAGE_ENFORCED_EXCEPTION );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerInitialize( )
{
	m_pRealCore->Initialize( );
}

void CVANetNetworkProtocol::ClientFinalize( )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_FINALIZE, MESSAGE_ENFORCED_EXCEPTION );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerFinalize( )
{
	m_pRealCore->Finalize( );
}

void CVANetNetworkProtocol::ClientReset( )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_RESET, MESSAGE_ENFORCED_EXCEPTION );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerReset( )
{
	m_pRealCore->Reset( );
}

void CVANetNetworkProtocol::ClientAttachEventHandler( )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_ATTACH_EVENT_HANDLER, MESSAGE_ENFORCED_EXCEPTION );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerAttachEventHandler( VistaConnectionIP* pConn )
{
	m_pServer->AttachCoreEventHandler( pConn );
}

void CVANetNetworkProtocol::ClientDetachEventHandler( )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_DETACH_EVENT_HANDLER, MESSAGE_ENFORCED_EXCEPTION );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerDetachEventHandler( VistaConnectionIP* pConn )
{
	m_pServer->DetachCoreEventHandler( pConn );
}

void CVANetNetworkProtocol::ClientGetModules( std::vector<CVAModuleInfo>& m_viModuleInfos )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_MODULES, MESSAGE_WITH_ANSWER );
	ClientSendCommand( pMsg );
	const int nModules = pMsg->ReadInt( );
	m_viModuleInfos.clear( );
	m_viModuleInfos.resize( nModules );
	for( int i = 0; i < nModules; i++ )
		pMsg->ReadModuleInfo( m_viModuleInfos[i] );
}

void CVANetNetworkProtocol::ServerGetModules( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	std::vector<CVAModuleInfo> viModules;
	m_pRealCore->GetModules( viModules );
	pMsg->WriteInt( (int)viModules.size( ) );
	for( int i = 0; i < (int)viModules.size( ); i++ )
		pMsg->WriteModuleInfo( viModules[i] );
}

CVAStruct CVANetNetworkProtocol::ClientGetSearchPaths( )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SEARCH_PATHS, MESSAGE_WITH_ANSWER );
	ClientSendCommand( pMsg );
	CVAStruct oPaths;
	pMsg->ReadVAStruct( oPaths );
	return oPaths;
}

void CVANetNetworkProtocol::ServerGetSearchPaths( )
{
	CVANetMessage* pMsg    = ServerGetMessage( );
	const CVAStruct oPaths = m_pRealCore->GetSearchPaths( );
	pMsg->WriteVAStruct( oPaths );
}

std::string CVANetNetworkProtocol::ClientFindFilePath( const std::string& sFilePath )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_FIND_FILE_PATH, MESSAGE_WITH_ANSWER );
	pMsg->WriteString( sFilePath );
	ClientSendCommand( pMsg );
	return pMsg->ReadString( );
}

void CVANetNetworkProtocol::ServerFindFilePath( )
{
	CVANetMessage* pMsg              = ServerGetMessage( );
	const std::string sFilePath      = pMsg->ReadString( );
	const std::string sFoundFilePath = m_pRealCore->FindFilePath( sFilePath );
	pMsg->WriteString( sFoundFilePath );
}

CVAStruct CVANetNetworkProtocol::ClientGetCoreConfiguration( const bool bFilterEnabled )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_CORE_CONFIGURATION, MESSAGE_WITH_ANSWER );
	pMsg->WriteBool( bFilterEnabled );
	ClientSendCommand( pMsg );
	CVAStruct oCoreConfig;
	pMsg->ReadVAStruct( oCoreConfig );
	return oCoreConfig;
}

void CVANetNetworkProtocol::ServerGetCoreConfiguration( )
{
	CVANetMessage* pMsg         = ServerGetMessage( );
	const bool bFilterEnabled   = pMsg->ReadBool( );
	const CVAStruct oCoreConfig = m_pRealCore->GetCoreConfiguration( bFilterEnabled );
	pMsg->WriteVAStruct( oCoreConfig );
}

CVAStruct CVANetNetworkProtocol::ClientGetHardwareConfiguration( )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_HARDWARE_CONFIGURATION, MESSAGE_WITH_ANSWER );
	ClientSendCommand( pMsg );
	CVAStruct oHWConfig;
	pMsg->ReadVAStruct( oHWConfig );
	return oHWConfig;
}
void CVANetNetworkProtocol::ServerGetHardwareConfiguration( )
{
	CVANetMessage* pMsg       = ServerGetMessage( );
	const CVAStruct oHWConfig = m_pRealCore->GetHardwareConfiguration( );
	pMsg->WriteVAStruct( oHWConfig );
}

CVAStruct CVANetNetworkProtocol::ClientGetFileList( const bool bRecursive, const std::string& sSuffixFilter )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_FILE_LIST, MESSAGE_WITH_ANSWER );
	pMsg->WriteBool( bRecursive );
	pMsg->WriteString( sSuffixFilter );
	ClientSendCommand( pMsg );
	CVAStruct oFileList;
	pMsg->ReadVAStruct( oFileList );
	return oFileList;
}

void CVANetNetworkProtocol::ServerGetFileList( )
{
	CVANetMessage* pMsg               = ServerGetMessage( );
	const bool bRecursive             = pMsg->ReadBool( );
	const std::string sFileSuffixMask = pMsg->ReadString( );
	const CVAStruct oFileList         = m_pRealCore->GetFileList( bRecursive, sFileSuffixMask );
	pMsg->WriteVAStruct( oFileList );
}

CVAStruct CVANetNetworkProtocol::ClientCallModule( const std::string& sModuleName, const CVAStruct& oArgs )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_CALL_MODULE, MESSAGE_WITH_ANSWER );
	pMsg->WriteString( sModuleName );
	pMsg->WriteVAStruct( oArgs );
	ClientSendCommand( pMsg );
	CVAStruct oReturn;
	pMsg->ReadVAStruct( oReturn );
	return oReturn;
}

void CVANetNetworkProtocol::ServerCallModule( )
{
	CVANetMessage* pMsg     = ServerGetMessage( );
	std::string sModuleName = pMsg->ReadString( );
	CVAStruct oArgs;
	pMsg->ReadVAStruct( oArgs );
	CVAStruct oReturn = m_pRealCore->CallModule( sModuleName, oArgs );
	pMsg->WriteVAStruct( oReturn );
}

int CVANetNetworkProtocol::ClientCreateDirectivityFromParameters( const CVAStruct& oParams, const std::string& sName )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_CREATE_DIRECTIVITY, MESSAGE_WITH_ANSWER );
	pMsg->WriteVAStruct( oParams );
	pMsg->WriteString( sName );

	ClientSendCommand( pMsg );

	return pMsg->ReadInt( );
}

void CVANetNetworkProtocol::ServerCreateDirectivityFromParameters( )
{
	CVANetMessage* pMsg = ServerGetMessage( );

	CVAStruct oParams;
	pMsg->ReadVAStruct( oParams );
	std::string sName = pMsg->ReadString( );

	const int iDirID = m_pRealCore->CreateDirectivityFromParameters( oParams, sName );

	pMsg->WriteInt( iDirID );
}

bool CVANetNetworkProtocol::ClientDeleteDirectivity( const int iDirID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_DELETE_DIRECTIVITY, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iDirID );

	ClientSendCommand( pMsg );

	return pMsg->ReadBool( );
}

void CVANetNetworkProtocol::ServerDeleteDirectivity( )
{
	CVANetMessage* pMsg = ServerGetMessage( );

	const int iDirID = pMsg->ReadInt( );

	const bool bRes = m_pRealCore->DeleteDirectivity( iDirID );
	pMsg->WriteBool( bRes );
}

CVADirectivityInfo CVANetNetworkProtocol::ClientGetDirectivityInfo( const int iDirID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_DIRECTIVITY_INFO, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iDirID );

	ClientSendCommand( pMsg );

	CVADirectivityInfo oDirInfo = pMsg->ReadDirectivityInfo( );
	return oDirInfo;
}

void CVANetNetworkProtocol::ServerGetDirectivityInfo( )
{
	CVANetMessage* pMsg = ServerGetMessage( );

	int iDirID = pMsg->ReadInt( );

	CVADirectivityInfo oDirInfo = m_pRealCore->GetDirectivityInfo( iDirID );

	pMsg->WriteDirectivityInfo( oDirInfo );
}

void CVANetNetworkProtocol::ClientGetDirectivityInfos( std::vector<CVADirectivityInfo>& vdiDest )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_DIRECTIVITY_INFOS, MESSAGE_WITH_ANSWER );
	ClientSendCommand( pMsg );
	vdiDest.clear( );
	const int iNumDirectivities = pMsg->ReadInt( );
	for( int i = 0; i < iNumDirectivities; i++ )
		vdiDest.push_back( pMsg->ReadDirectivityInfo( ) );
}

void CVANetNetworkProtocol::ServerGetDirectivityInfos( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	std::vector<CVADirectivityInfo> vInfos;
	m_pRealCore->GetDirectivityInfos( vInfos );
	pMsg->WriteInt( (int)vInfos.size( ) );
	for( size_t i = 0; i < vInfos.size( ); i++ )
		pMsg->WriteDirectivityInfo( vInfos[i] );
}

void CVANetNetworkProtocol::ClientSetDirectivityName( const int iID, const std::string& sName )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_DIRECTIVITY_NAME, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iID );
	pMsg->WriteString( sName );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetDirectivityName( )
{
	CVANetMessage* pMsg     = ServerGetMessage( );
	const int iID           = pMsg->ReadInt( );
	const std::string sName = pMsg->ReadString( );
	m_pRealCore->SetDirectivityName( iID, sName );
}

std::string CVANetNetworkProtocol::ClientGetDirectivityName( const int iID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_DIRECTIVITY_NAME, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iID );
	ClientSendCommand( pMsg );
	const std::string sName = pMsg->ReadString( );
	return sName;
}

void CVANetNetworkProtocol::ServerGetDirectivityName( )
{
	CVANetMessage* pMsg     = ServerGetMessage( );
	const int iID           = pMsg->ReadInt( );
	const std::string sName = m_pRealCore->GetDirectivityName( iID );
	pMsg->WriteString( sName );
}

void CVANetNetworkProtocol::ClientSetDirectivityParameters( const int iID, const CVAStruct& oParams )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_DIRECTIVITY_PARAMETERS, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iID );
	pMsg->WriteVAStruct( oParams );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetDirectivityParameters( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const int iID       = pMsg->ReadInt( );
	CVAStruct oParams;
	pMsg->ReadVAStruct( oParams );
	m_pRealCore->SetDirectivityParameters( iID, oParams );
}

CVAStruct CVANetNetworkProtocol::ClientGetDirectivityParameters( const int iID, const CVAStruct& oParams )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_DIRECTIVITY_PARAMETERS, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iID );
	pMsg->WriteVAStruct( oParams );
	ClientSendCommand( pMsg );
	CVAStruct oReturn;
	pMsg->ReadVAStruct( oReturn );
	return oReturn;
}

void CVANetNetworkProtocol::ServerGetDirectivityParameters( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const int iID       = pMsg->ReadInt( );
	CVAStruct oParams;
	pMsg->ReadVAStruct( oParams );
	const CVAStruct oReturn = m_pRealCore->GetDirectivityParameters( iID, oParams );
	pMsg->WriteVAStruct( oReturn );
}

std::string CVANetNetworkProtocol::ClientCreateSignalSourceBufferFromParameters( const CVAStruct& oParams, const std::string& sName )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_CREATE_SIGNAL_SOURCE_BUFFER_FROM_PARAMETERS, MESSAGE_WITH_ANSWER );
	pMsg->WriteVAStruct( oParams );
	pMsg->WriteString( sName );
	ClientSendCommand( pMsg );
	return pMsg->ReadString( );
}

void CVANetNetworkProtocol::ServerCreateSignalSourceBufferFromParameters( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	CVAStruct oParams;
	pMsg->ReadVAStruct( oParams );
	const std::string sName = pMsg->ReadString( );
	const std::string sID   = m_pRealCore->CreateSignalSourceBufferFromParameters( oParams, sName );
	pMsg->WriteString( sID );
}

std::string CVANetNetworkProtocol::ClientCreateSignalSourcePrototypeFromParameters( const CVAStruct& oParams, const std::string& sName )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_CREATE_SIGNAL_SOURCE_PROTOTYPE_FROM_PARAMETERS, MESSAGE_WITH_ANSWER );
	pMsg->WriteVAStruct( oParams );
	pMsg->WriteString( sName );
	ClientSendCommand( pMsg );
	return pMsg->ReadString( );
}

void CVANetNetworkProtocol::ServerCreateSignalSourcePrototypeFromParameters( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	CVAStruct oParams;
	pMsg->ReadVAStruct( oParams );
	const std::string sName = pMsg->ReadString( );
	const std::string sID   = m_pRealCore->CreateSignalSourcePrototypeFromParameters( oParams, sName );
	pMsg->WriteString( sID );
}

std::string CVANetNetworkProtocol::ClientCreateTextToSpeechSignalSource( const std::string& sName )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_CREATE_SIGNAL_SOURCE_TEXT_TO_SPEECH, MESSAGE_WITH_ANSWER );
	pMsg->WriteString( sName );
	ClientSendCommand( pMsg );
	return pMsg->ReadString( );
}

void CVANetNetworkProtocol::ServerCreateTextToSpeechSignalSource( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	std::string sName   = pMsg->ReadString( );
	std::string sID     = m_pRealCore->CreateSignalSourceTextToSpeech( sName );
	pMsg->WriteString( sID );
}

std::string CVANetNetworkProtocol::ClientCreateSequencerSignalSource( const std::string& sName )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_CREATE_SIGNAL_SOURCE_SEQUENCER, MESSAGE_WITH_ANSWER );
	pMsg->WriteString( sName );
	ClientSendCommand( pMsg );
	return pMsg->ReadString( );
}

void CVANetNetworkProtocol::ServerCreateSequencerSignalSource( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	std::string sName   = pMsg->ReadString( );
	std::string sID     = m_pRealCore->CreateSignalSourceSequencer( sName );
	pMsg->WriteString( sID );
}

std::string CVANetNetworkProtocol::ClientCreateNetworkStreamSignalSource( const std::string& sInterface, const int iPort, const std::string& sName )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_CREATE_SIGNAL_SOURCE_NETWORK_STREAM, MESSAGE_WITH_ANSWER );

	pMsg->WriteString( sInterface );
	pMsg->WriteInt( iPort );
	pMsg->WriteString( sName );

	ClientSendCommand( pMsg );

	return pMsg->ReadString( );
}

void CVANetNetworkProtocol::ServerCreateNetworkStreamSignalSource( )
{
	CVANetMessage* pMsg = ServerGetMessage( );

	std::string sInterface = pMsg->ReadString( );
	int iPort              = pMsg->ReadInt( );
	std::string sName      = pMsg->ReadString( );

	std::string iID = m_pRealCore->CreateSignalSourceNetworkStream( sInterface, iPort, sName );

	pMsg->WriteString( iID );
}

std::string CVANetNetworkProtocol::ClientCreateEngineSignalSource( const CVAStruct& oParams, const std::string& sName )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_CREATE_SIGNAL_SOURCE_ENGINE, MESSAGE_WITH_ANSWER );

	pMsg->WriteVAStruct( oParams );
	pMsg->WriteString( sName );

	ClientSendCommand( pMsg );

	return pMsg->ReadString( );
}

void CVANetNetworkProtocol::ServerCreateEngineSignalSource( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	CVAStruct oParams;
	pMsg->ReadVAStruct( oParams );
	std::string sName = pMsg->ReadString( );
	std::string sID   = m_pRealCore->CreateSignalSourceEngine( oParams, sName );
	pMsg->WriteString( sID );
}

std::string CVANetNetworkProtocol::ClientCreateSignalSourceMachine( const CVAStruct& oParams, const std::string& sName )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_CREATE_SIGNAL_SOURCE_MACHINE, MESSAGE_WITH_ANSWER );
	pMsg->WriteVAStruct( oParams );
	pMsg->WriteString( sName );
	ClientSendCommand( pMsg );
	return pMsg->ReadString( );
}

void CVANetNetworkProtocol::ServerCreateSignalSourceMachine( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	CVAStruct oParams;
	pMsg->ReadVAStruct( oParams );
	std::string sName = pMsg->ReadString( );
	std::string sID   = m_pRealCore->CreateSignalSourceMachine( oParams, sName );
	pMsg->WriteString( sID );
}

bool CVANetNetworkProtocol::ClientDeleteSignalSource( const std::string& sID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_DELETE_SIGNALSOURCE, MESSAGE_WITH_ANSWER );
	pMsg->WriteString( sID );
	ClientSendCommand( pMsg );
	return pMsg->ReadBool( );
}

void CVANetNetworkProtocol::ServerDeleteSignalSource( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	std::string sID     = pMsg->ReadString( );
	bool bResult        = m_pRealCore->DeleteSignalSource( sID );
	pMsg->WriteBool( bResult );
}

std::string CVANetNetworkProtocol::ClientGetSoundSourceSignalSource( const int iSoundSourceID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_SOURCE_SIGNALSOURCE, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iSoundSourceID );
	ClientSendCommand( pMsg );
	return pMsg->ReadString( );
}

void CVANetNetworkProtocol::ServerGetSoundSourceSignalSource( )
{
	CVANetMessage* pMsg         = ServerGetMessage( );
	const int iID               = pMsg->ReadInt( );
	std::string sSignalSourceID = m_pRealCore->GetSoundSourceSignalSource( iID );
	pMsg->WriteString( sSignalSourceID );
}

CVASignalSourceInfo CVANetNetworkProtocol::ClientGetSignalSourceInfo( const std::string& sSignalSourceID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SIGNALSOURCE_INFO, MESSAGE_WITH_ANSWER );
	pMsg->WriteString( sSignalSourceID );
	ClientSendCommand( pMsg );
	CVASignalSourceInfo oSignalSourceInfo = pMsg->ReadSignalSourceInfo( );
	return oSignalSourceInfo;
}

void CVANetNetworkProtocol::ServerGetSignalSourceInfo( )
{
	CVANetMessage* pMsg                   = ServerGetMessage( );
	std::string sSignalSourceID           = pMsg->ReadString( );
	CVASignalSourceInfo oSignalSourceInfo = m_pRealCore->GetSignalSourceInfo( sSignalSourceID );
	pMsg->WriteSignalSourceInfo( oSignalSourceInfo );
}

void CVANetNetworkProtocol::ClientGetSignalSourceInfos( std::vector<CVASignalSourceInfo>& vssiDest )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SIGNALSOURCE_INFOS, MESSAGE_WITH_ANSWER );
	ClientSendCommand( pMsg );
	vssiDest.clear( );
	int iNumSignalSources = pMsg->ReadInt( );
	for( int i = 0; i < iNumSignalSources; i++ )
		vssiDest.push_back( pMsg->ReadSignalSourceInfo( ) );
}

void CVANetNetworkProtocol::ServerGetSignalSourceInfos( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	std::vector<CVASignalSourceInfo> vInfos;
	m_pRealCore->GetSignalSourceInfos( vInfos );
	pMsg->WriteInt( (int)vInfos.size( ) );
	for( size_t i = 0; i < vInfos.size( ); i++ )
		pMsg->WriteSignalSourceInfo( vInfos[i] );
}

int CVANetNetworkProtocol::ClientGetSignalSourceBufferPlaybackState( const std::string& sSignalSourceID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SIGNAL_SOURCE_BUFFER_PLAYBACK_STATE, MESSAGE_WITH_ANSWER );
	pMsg->WriteString( sSignalSourceID );
	ClientSendCommand( pMsg );
	const int iPlayState = pMsg->ReadInt( );
	return iPlayState;
}

void CVANetNetworkProtocol::ServerGetSignalSourceBufferPlaybackState( )
{
	CVANetMessage* pMsg         = ServerGetMessage( );
	std::string sSignalSourceID = pMsg->ReadString( );
	const int iPlayState        = m_pRealCore->GetSignalSourceBufferPlaybackState( sSignalSourceID );
	pMsg->WriteInt( iPlayState );
}

void CVANetNetworkProtocol::ClientSetSignalSourceBufferPlaybackAction( const std::string& sSignalSourceID, const int iPlayAction )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SIGNAL_SOURCE_BUFFER_PLAYBACK_ACTION, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteString( sSignalSourceID );
	pMsg->WriteInt( iPlayAction );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSignalSourceBufferPlaybackAction( )
{
	CVANetMessage* pMsg         = ServerGetMessage( );
	std::string sSignalSourceID = pMsg->ReadString( );
	int iPlayAction             = pMsg->ReadInt( );
	m_pRealCore->SetSignalSourceBufferPlaybackAction( sSignalSourceID, iPlayAction );
}

void CVANetNetworkProtocol::ClientSetSignalSourceBufferLooping( const std::string& sSignalSourceID, const bool bLooping )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SIGNAL_SOURCE_BUFFER_LOOPING, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteString( sSignalSourceID );
	pMsg->WriteBool( bLooping );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSignalSourceBufferLooping( )
{
	CVANetMessage* pMsg         = ServerGetMessage( );
	std::string sSignalSourceID = pMsg->ReadString( );
	bool bLooping               = pMsg->ReadBool( );
	m_pRealCore->SetSignalSourceBufferLooping( sSignalSourceID, bLooping );
}

bool CVANetNetworkProtocol::ClientGetSignalSourceBufferLooping( const std::string& sSignalSourceID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SIGNAL_SOURCE_BUFFER_LOOPING, MESSAGE_WITH_ANSWER );
	pMsg->WriteString( sSignalSourceID );
	ClientSendCommand( pMsg );
	bool bLooping = pMsg->ReadBool( );
	return bLooping;
}

void CVANetNetworkProtocol::ServerGetSignalSourceBufferLooping( )
{
	CVANetMessage* pMsg         = ServerGetMessage( );
	std::string sSignalSourceID = pMsg->ReadString( );
	bool bLooping               = m_pRealCore->GetSignalSourceBufferLooping( sSignalSourceID );
	pMsg->WriteBool( bLooping );
}

void CVANetNetworkProtocol::ClientSetSignalSourceBufferPlaybackPosition( const std::string& sSignalSourceID, const double dPlaybackPosition )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SIGNAL_SOURCE_BUFFER_PLAYBACK_POSITION, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteString( sSignalSourceID );
	pMsg->WriteDouble( dPlaybackPosition );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetAudiofileSignalSourcePlaybackPosition( )
{
	CVANetMessage* pMsg         = ServerGetMessage( );
	std::string sSignalSourceID = pMsg->ReadString( );
	double dPlaybackPosition    = pMsg->ReadDouble( );
	m_pRealCore->SetSignalSourceBufferPlaybackPosition( sSignalSourceID, dPlaybackPosition );
}

void CVANetNetworkProtocol::ClientSetSignalSourceParameters( const std::string& sSignalSourceID, const CVAStruct& oParams )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SIGNALSOURCE_PARAMETERS, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteString( sSignalSourceID );
	pMsg->WriteVAStruct( oParams );
	ClientSendCommand( pMsg );
	return;
}

void CVANetNetworkProtocol::ServerSetSignalSourceParameters( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	std::string sID     = pMsg->ReadString( );
	CVAStruct oParams;
	pMsg->ReadVAStruct( oParams );
	m_pRealCore->SetSignalSourceParameters( sID, oParams );
	return;
}

CVAStruct CVANetNetworkProtocol::ClientGetSignalSourceParameters( const std::string& sSignalSourceID, const CVAStruct& oParams )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SIGNAL_SOURCE_PARAMETERS, MESSAGE_WITH_ANSWER );
	pMsg->WriteString( sSignalSourceID );
	pMsg->WriteVAStruct( oParams );
	ClientSendCommand( pMsg );
	CVAStruct oRet;
	pMsg->ReadVAStruct( oRet );
	return oRet;
}

void CVANetNetworkProtocol::ServerGetSignalSourceParameters( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	std::string sID     = pMsg->ReadString( );
	CVAStruct oParams;
	pMsg->ReadVAStruct( oParams );
	CVAStruct oRet = m_pRealCore->GetSignalSourceParameters( sID, oParams );
	pMsg->WriteVAStruct( oRet );
	return;
}

int CVANetNetworkProtocol::ClientAddSignalSourceSequencerSample( const std::string& sSignalSourceID, const CVAStruct& oArgs )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_ADD_SIGNAL_SOURCE_SEQUENCER_SAMPLE, MESSAGE_WITH_ANSWER );
	pMsg->WriteString( sSignalSourceID );
	pMsg->WriteVAStruct( oArgs );
	ClientSendCommand( pMsg );
	return pMsg->ReadInt( );
}

void CVANetNetworkProtocol::ServerAddSignalSourceSequencerSample( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	std::string sID     = pMsg->ReadString( );
	CVAStruct oParams;
	pMsg->ReadVAStruct( oParams );
	const int iSoundID = m_pRealCore->AddSignalSourceSequencerSample( sID, oParams );
	pMsg->WriteInt( iSoundID );
}

int CVANetNetworkProtocol::ClientAddSignalSourceSequencerPlayback( const std::string& sSignalSourceID, const int iSoundID, const int iFlags, const double dTimeCode )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_ADD_SIGNAL_SOURCE_SEQUENCER_PLAYBACK, MESSAGE_WITH_ANSWER );
	pMsg->WriteString( sSignalSourceID );
	pMsg->WriteInt( iSoundID );
	pMsg->WriteInt( iFlags );
	pMsg->WriteDouble( dTimeCode );
	ClientSendCommand( pMsg );
	const int iRet = pMsg->ReadInt( );
	return iRet;
}

void CVANetNetworkProtocol::ServerAddSignalSourceSequencerPlayback( )
{
	CVANetMessage* pMsg    = ServerGetMessage( );
	const std::string sID  = pMsg->ReadString( );
	const int iSoundID     = pMsg->ReadInt( );
	const int iFlags       = pMsg->ReadInt( );
	const double dTimeCode = pMsg->ReadDouble( );
	const int iRet         = m_pRealCore->AddSignalSourceSequencerPlayback( sID, iSoundID, iFlags, dTimeCode );
	pMsg->WriteInt( iRet );
	return;
}

void CVANetNetworkProtocol::ClientRemoveSignalSourceSequencerSample( const std::string& sSignalSourceID, const int iSoundID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_REMOVE_SIGNAL_SOURCE_SEQUENCER_SAMPLE, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteString( sSignalSourceID );
	pMsg->WriteInt( iSoundID );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerRemoveSignalSourceSequencerSample( )
{
	CVANetMessage* pMsg   = ServerGetMessage( );
	const std::string sID = pMsg->ReadString( );
	const int iSoundID    = pMsg->ReadInt( );
	m_pRealCore->RemoveSignalSourceSequencerSample( sID, iSoundID );
}

void CVANetNetworkProtocol::ClientLockUpdate( )
{
	ClientPrepareMessageBuffering( );
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_LOCK_UPDATE, MESSAGE_ALLOWS_BUFFERING );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerLockUpdate( )
{
	m_pRealCore->LockUpdate( );
}

int CVANetNetworkProtocol::ClientUnlockUpdate( )
{
	if( m_bBufferingActive == false )
	{
		CVANetMessage* pMsg = ClientInitMessage( VA_NP_UNLOCK_UPDATE, MESSAGE_WITH_ANSWER );
		ClientSendCommand( pMsg );
		return pMsg->ReadInt( );
	}
	else
	{
		ClientInitMessage( VA_NP_UNLOCK_UPDATE, MESSAGE_ALLOWS_BUFFERING );
		return ClientEndMessageBuffering( );
	}
}

void CVANetNetworkProtocol::ServerUnlockUpdate( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	int iResult         = m_pRealCore->UnlockUpdate( );
	pMsg->WriteInt( iResult );
}

bool CVANetNetworkProtocol::ClientGetUpdateLocked( )
{
	if( m_bBufferingActive )
		return true;

	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_UPDATE_LOCKED, MESSAGE_WITH_ANSWER );
	ClientSendCommand( pMsg );
	return pMsg->ReadBool( );
}

void CVANetNetworkProtocol::ServerGetUpdateLocked( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const bool bResult  = m_pRealCore->GetUpdateLocked( );
	pMsg->WriteBool( bResult );
}


void CVANetNetworkProtocol::ClientGetSoundSourceIDs( std::vector<int>& vSoundSourceIDs )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_SOURCE_IDS, MESSAGE_WITH_ANSWER );
	ClientSendCommand( pMsg );
	vSoundSourceIDs.clear( );
	const int iNumElems = pMsg->ReadInt( );
	for( int i = 0; i < iNumElems; i++ )
		vSoundSourceIDs.push_back( pMsg->ReadInt( ) );
}

void CVANetNetworkProtocol::ServerGetSoundSourceIDs( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	std::vector<int> vIDs;
	m_pRealCore->GetSoundSourceIDs( vIDs );
	pMsg->WriteInt( (int)vIDs.size( ) );
	for( size_t i = 0; i < vIDs.size( ); i++ )
		pMsg->WriteInt( vIDs[i] );
}

int CVANetNetworkProtocol::ClientCreateSoundSource( const std::string& sName )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_CREATE_SOUND_SOURCE, MESSAGE_WITH_ANSWER );
	pMsg->WriteString( sName );
	ClientSendCommand( pMsg );
	return pMsg->ReadInt( );
}

void CVANetNetworkProtocol::ServerCreateSoundSource( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	std::string sName   = pMsg->ReadString( );
	const int iID       = m_pRealCore->CreateSoundSource( sName );
	pMsg->WriteInt( iID );
}

int CVANetNetworkProtocol::ClientCreateSoundSourceExplicitRenderer( const std::string& sRendererID, const std::string& sName )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_CREATE_SOUND_SOURCE_EXPLICIT_RENDERER, MESSAGE_WITH_ANSWER );
	pMsg->WriteString( sRendererID );
	pMsg->WriteString( sName );
	ClientSendCommand( pMsg );
	return pMsg->ReadInt( );
}

void CVANetNetworkProtocol::ServerCreateSoundSourceExplicitRenderer( )
{
	CVANetMessage* pMsg     = ServerGetMessage( );
	std::string sRendererID = pMsg->ReadString( );
	std::string sName       = pMsg->ReadString( );
	const int iID           = m_pRealCore->CreateSoundSourceExplicitRenderer( sRendererID, sName );
	pMsg->WriteInt( iID );
}

int CVANetNetworkProtocol::ClientDeleteSoundSource( const int iSoundSourceID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_DELETE_SOUND_SOURCE, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iSoundSourceID );
	ClientSendCommand( pMsg );
	return pMsg->ReadInt( );
}

void CVANetNetworkProtocol::ServerDeleteSoundSource( )
{
	CVANetMessage* pMsg      = ServerGetMessage( );
	const int iSoundSourceID = pMsg->ReadInt( );
	const int iReturn        = m_pRealCore->DeleteSoundSource( iSoundSourceID );
	pMsg->WriteInt( iReturn );
}

void CVANetNetworkProtocol::ClientSetSoundSourceEnabled( const int iID, const bool bEnabled )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SOUND_SOURCE_ENABLED, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iID );
	pMsg->WriteBool( bEnabled );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSoundSourceEnabled( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const int iID       = pMsg->ReadInt( );
	const bool bEnabled = pMsg->ReadBool( );
	m_pRealCore->SetSoundSourceEnabled( iID, bEnabled );
}

bool CVANetNetworkProtocol::ClientGetSoundSourceEnabled( const int iID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_SOURCE_ENABLED, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iID );
	ClientSendCommand( pMsg );
	return pMsg->ReadBool( );
}

void CVANetNetworkProtocol::ServerGetSoundSourceEnabled( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const int iID       = pMsg->ReadInt( );
	const bool bEnabled = m_pRealCore->GetSoundSourceEnabled( iID );
	pMsg->WriteBool( bEnabled );
}

CVASoundSourceInfo CVANetNetworkProtocol::ClientGetSoundSourceInfo( const int iID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_SOURCE_INFO, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iID );
	ClientSendCommand( pMsg );
	return pMsg->ReadSoundSourceInfo( );
}

void CVANetNetworkProtocol::ServerGetSoundSourceInfo( )
{
	CVANetMessage* pMsg            = ServerGetMessage( );
	const int iID                  = pMsg->ReadInt( );
	const CVASoundSourceInfo oInfo = m_pRealCore->GetSoundSourceInfo( iID );
	pMsg->WriteSoundSourceInfo( oInfo );
}

void CVANetNetworkProtocol::ClientSetSoundSourceName( const int iSoundSourceID, const std::string& sName )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SOUND_SOURCE_NAME, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iSoundSourceID );
	pMsg->WriteString( sName );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSoundSourceName( )
{
	CVANetMessage* pMsg      = ServerGetMessage( );
	const int iSoundSourceID = pMsg->ReadInt( );
	std::string sName        = pMsg->ReadString( );
	m_pRealCore->SetSoundSourceName( iSoundSourceID, sName );
}

std::string CVANetNetworkProtocol::ClientGetSoundSourceName( const int iSoundSourceID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_SOURCE_NAME, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iSoundSourceID );
	ClientSendCommand( pMsg );
	return pMsg->ReadString( );
}

void CVANetNetworkProtocol::ServerGetSoundSourceName( )
{
	CVANetMessage* pMsg      = ServerGetMessage( );
	const int iSoundSourceID = pMsg->ReadInt( );
	std::string sName        = m_pRealCore->GetSoundSourceName( iSoundSourceID );

	pMsg->WriteString( sName );
}

void CVANetNetworkProtocol::ClientSetSoundSourceAuralizationMode( int iSoundSourceID, int iAuralizationMode )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SOUND_SOURCE_AURAMODE, MESSAGE_ALLOWS_BUFFERING );

	pMsg->WriteInt( iSoundSourceID );
	pMsg->WriteInt( iAuralizationMode );

	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSoundSourceAuralizationMode( )
{
	CVANetMessage* pMsg = ServerGetMessage( );

	int iSoundSourceID    = pMsg->ReadInt( );
	int iAuralizationMode = pMsg->ReadInt( );

	m_pRealCore->SetSoundSourceAuralizationMode( iSoundSourceID, iAuralizationMode );
}

int CVANetNetworkProtocol::ClientGetSoundSourceAuralizationMode( int iSoundSourceID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_SOURCE_AURAMODE, MESSAGE_WITH_ANSWER );

	pMsg->WriteInt( iSoundSourceID );

	ClientSendCommand( pMsg );

	return pMsg->ReadInt( );
}

void CVANetNetworkProtocol::ServerGetSoundSourceAuralizationMode( )
{
	CVANetMessage* pMsg = ServerGetMessage( );

	int iSoundSourceID = pMsg->ReadInt( );

	int iAuralizationMode = m_pRealCore->GetSoundSourceAuralizationMode( iSoundSourceID );

	pMsg->WriteInt( iAuralizationMode );
}


CVAStruct CVANetNetworkProtocol::ClientGetSoundSourceParameters( const int iID, const CVAStruct& oArgs )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_SOURCE_PARAMETERS, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iID );
	pMsg->WriteVAStruct( oArgs );

	ClientSendCommand( pMsg );

	CVAStruct oRet;
	pMsg->ReadVAStruct( oRet );

	return oRet;
}

void CVANetNetworkProtocol::ServerGetSoundSourceParameters( )
{
	CVANetMessage* pMsg = ServerGetMessage( );

	int iID = pMsg->ReadInt( );
	CVAStruct oArgs;
	pMsg->ReadVAStruct( oArgs );

	CVAStruct oRet = m_pRealCore->GetSoundSourceParameters( iID, oArgs );

	pMsg->WriteVAStruct( oRet );

	return;
}

void CVANetNetworkProtocol::ClientSetSoundSourceParameters( int iID, const CVAStruct& oParams )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SOUND_SOURCE_PARAMETERS, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iID );
	pMsg->WriteVAStruct( oParams );

	ClientSendCommand( pMsg );

	return;
}

void CVANetNetworkProtocol::ServerSetSoundSourceParameters( )
{
	CVANetMessage* pMsg = ServerGetMessage( );

	int iID = pMsg->ReadInt( );
	CVAStruct oParams;
	pMsg->ReadVAStruct( oParams );

	m_pRealCore->SetSoundSourceParameters( iID, oParams );

	return;
}


int CVANetNetworkProtocol::ClientGetSoundSourceDirectivity( int iSoundSourceID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_SOURCE_DIRECTIVITY, MESSAGE_WITH_ANSWER );

	pMsg->WriteInt( iSoundSourceID );

	ClientSendCommand( pMsg );

	return pMsg->ReadInt( );
}

void CVANetNetworkProtocol::ServerGetSoundSourceDirectivity( )
{
	CVANetMessage* pMsg = ServerGetMessage( );

	int iSoundSourceID = pMsg->ReadInt( );

	int iDirectivityID = m_pRealCore->GetSoundSourceDirectivity( iSoundSourceID );

	pMsg->WriteInt( iDirectivityID );
}

void CVANetNetworkProtocol::ClientSetSoundSourceDirectivity( int iSoundSourceID, int iDirectivityID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SOUND_SOURCE_DIRECTIVITY, MESSAGE_ALLOWS_BUFFERING );

	pMsg->WriteInt( iSoundSourceID );
	pMsg->WriteInt( iDirectivityID );

	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSoundSourceDirectivity( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	int iSoundSourceID  = pMsg->ReadInt( );
	int iDirectivityID  = pMsg->ReadInt( );
	m_pRealCore->SetSoundSourceDirectivity( iSoundSourceID, iDirectivityID );
}

void CVANetNetworkProtocol::ClientSetSoundSourceSoundPower( const int iSoundSourceID, const double dPower )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SOUND_SOURCE_SOUND_POWER, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iSoundSourceID );
	pMsg->WriteDouble( dPower );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSoundSourceSoundPower( )
{
	CVANetMessage* pMsg      = ServerGetMessage( );
	const int iSoundSourceID = pMsg->ReadInt( );
	const double dPower      = pMsg->ReadDouble( );
	m_pRealCore->SetSoundSourceSoundPower( iSoundSourceID, dPower );
}

double CVANetNetworkProtocol::ClientGetSoundSourceSoundPower( const int iSoundSourceID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_SOURCE_SOUND_POWER, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iSoundSourceID );
	ClientSendCommand( pMsg );
	return pMsg->ReadDouble( );
}

void CVANetNetworkProtocol::ServerGetSoundSourceSoundPower( )
{
	CVANetMessage* pMsg      = ServerGetMessage( );
	const int iSoundSourceID = pMsg->ReadInt( );
	const double dPower      = m_pRealCore->GetSoundSourceSoundPower( iSoundSourceID );
	pMsg->WriteDouble( dPower );
}

void CVANetNetworkProtocol::ClientSetSoundSourceMuted( const int iSoundSourceID, const bool bMuted )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SOUND_SOURCE_MUTED, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iSoundSourceID );
	pMsg->WriteBool( bMuted );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSoundSourceMuted( )
{
	CVANetMessage* pMsg      = ServerGetMessage( );
	const int iSoundSourceID = pMsg->ReadInt( );
	const bool bMuted        = pMsg->ReadBool( );
	m_pRealCore->SetSoundSourceMuted( iSoundSourceID, bMuted );
}

bool CVANetNetworkProtocol::ClientGetSoundSourceMuted( const int iSoundSourceID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_SOURCE_MUTED, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iSoundSourceID );
	ClientSendCommand( pMsg );
	return pMsg->ReadBool( );
}

void CVANetNetworkProtocol::ServerGetSoundSourceMuted( )
{
	CVANetMessage* pMsg      = ServerGetMessage( );
	const int iSoundSourceID = pMsg->ReadInt( );
	const bool bMuted        = m_pRealCore->GetSoundSourceMuted( iSoundSourceID );
	pMsg->WriteBool( bMuted );
}


void CVANetNetworkProtocol::ClientSetSoundSourcePose( const int iSoundSourceID, const VAVec3& vPos, const VAQuat& qOrient )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SOUND_SOURCE_POSE, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iSoundSourceID );
	pMsg->WriteVec3( vPos );
	pMsg->WriteQuat( qOrient );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSoundSourcePose( )
{
	CVANetMessage* pMsg  = ServerGetMessage( );
	int iSoundSourceID   = pMsg->ReadInt( );
	const VAVec3 v3Pos   = pMsg->ReadVec3( );
	const VAQuat qOrient = pMsg->ReadQuat( );
	m_pRealCore->SetSoundSourcePose( iSoundSourceID, v3Pos, qOrient );
}

void CVANetNetworkProtocol::ClientGetSoundSourcePose( const int iID, VAVec3& vPos, VAQuat& qOrient )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_SOURCE_POSE, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iID );
	ClientSendCommand( pMsg );
	vPos    = pMsg->ReadVec3( );
	qOrient = pMsg->ReadQuat( );
}

void CVANetNetworkProtocol::ServerGetSoundSourcePose( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const int iID       = pMsg->ReadInt( );
	VAVec3 v3Pos;
	VAQuat qOrient;
	m_pRealCore->GetSoundSourcePose( iID, v3Pos, qOrient );
	pMsg->WriteVec3( v3Pos );
	pMsg->WriteQuat( qOrient );
}


void CVANetNetworkProtocol::ClientSetSoundSourcePosition( const int iSoundSourceID, const VAVec3& vPos )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SOUND_SOURCE_POSITION, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iSoundSourceID );
	pMsg->WriteVec3( vPos );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSoundSourcePosition( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	int iSoundSourceID  = pMsg->ReadInt( );
	const VAVec3 v3Pos  = pMsg->ReadVec3( );
	m_pRealCore->SetSoundSourcePosition( iSoundSourceID, v3Pos );
}

VAVec3 CVANetNetworkProtocol::ClientGetSoundSourcePosition( const int iSoundSourceID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_SOURCE_POSITION, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iSoundSourceID );
	ClientSendCommand( pMsg );
	const VAVec3 vPos = pMsg->ReadVec3( );
	return vPos;
}

void CVANetNetworkProtocol::ServerGetSoundSourcePosition( )
{
	CVANetMessage* pMsg      = ServerGetMessage( );
	const int iSoundSourceID = pMsg->ReadInt( );
	VAVec3 v3Pos             = m_pRealCore->GetSoundSourcePosition( iSoundSourceID );
	pMsg->WriteVec3( v3Pos );
}

void CVANetNetworkProtocol::ClientSetSoundSourceOrientation( const int iID, const VAQuat& qOrient )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SOUND_SOURCE_ORIENTATION, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iID );
	pMsg->WriteQuat( qOrient );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSoundSourceOrientation( )
{
	CVANetMessage* pMsg  = ServerGetMessage( );
	const int iID        = pMsg->ReadInt( );
	const VAQuat qOrient = pMsg->ReadQuat( );
	m_pRealCore->SetSoundSourceOrientation( iID, qOrient );
}

VAQuat CVANetNetworkProtocol::ClientGetSoundSourceOrientation( const int iID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_SOURCE_ORIENTATION, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iID );
	ClientSendCommand( pMsg );
	const VAQuat qOrient = pMsg->ReadQuat( );
	return qOrient;
}

void CVANetNetworkProtocol::ServerGetSoundSourceOrientation( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const int iID       = pMsg->ReadInt( );
	VAQuat qOrient      = m_pRealCore->GetSoundSourceOrientation( iID );
	pMsg->WriteQuat( qOrient );
}

void CVANetNetworkProtocol::ClientSetSoundSourceOrientationVU( const int iID, const VAVec3& v3View, const VAVec3& v3Up )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SOUND_SOURCE_ORIENTATION_VU, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iID );
	pMsg->WriteVec3( v3View );
	pMsg->WriteVec3( v3Up );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSoundSourceOrientationVU( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const int iID       = pMsg->ReadInt( );
	const VAVec3 v3View = pMsg->ReadVec3( );
	const VAVec3 v3Up   = pMsg->ReadVec3( );
	m_pRealCore->SetSoundSourceOrientationVU( iID, v3View, v3Up );
}

int CVANetNetworkProtocol::ClientGetSoundSourceGeometryMesh( const int iID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_SOURCE_GEOMETRY_MESH, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iID );
	ClientSendCommand( pMsg );
	return pMsg->ReadInt( );
}

void CVANetNetworkProtocol::ServerGetSoundSourceGeometryMesh( )
{
	CVANetMessage* pMsg       = ServerGetMessage( );
	const int iID             = pMsg->ReadInt( );
	const int iGeometryMeshID = m_pRealCore->GetSoundSourceGeometryMesh( iID );
	pMsg->WriteInt( iGeometryMeshID );
}

void CVANetNetworkProtocol::ClientSetSoundSourceGeometryMesh( const int iSoundSourceID, const int iGeometryMeshID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SOUND_SOURCE_GEOMETRY_MESH, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iSoundSourceID );
	pMsg->WriteInt( iGeometryMeshID );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSoundSourceGeometryMesh( )
{
	CVANetMessage* pMsg       = ServerGetMessage( );
	const int iSoundSourceID  = pMsg->ReadInt( );
	const int iGeometryMeshID = pMsg->ReadInt( );
	m_pRealCore->SetSoundSourceGeometryMesh( iSoundSourceID, iGeometryMeshID );
}

void CVANetNetworkProtocol::ClientGetSoundSourceOrientationVU( const int iID, VAVec3& v3View, VAVec3& v3Up )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_SOURCE_ORIENTATION_VU, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iID );
	ClientSendCommand( pMsg );
	v3View = pMsg->ReadVec3( );
	v3Up   = pMsg->ReadVec3( );
}

void CVANetNetworkProtocol::ServerGetSoundSourceOrientationVU( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const int iID       = pMsg->ReadInt( );
	VAVec3 v3View;
	VAVec3 v3Up;
	m_pRealCore->GetSoundSourceOrientationVU( iID, v3View, v3Up );
	pMsg->WriteVec3( v3View );
	pMsg->WriteVec3( v3Up );
}

void CVANetNetworkProtocol::ClientSetSoundSourceSignalSource( const int iSoundSourceID, const std::string& sSignalSourceID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SOUND_SOURCE_SIGNALSOURCE, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iSoundSourceID );
	pMsg->WriteString( sSignalSourceID );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSoundSourceSignalSource( )
{
	CVANetMessage* pMsg      = ServerGetMessage( );
	const int iSoundSourceID = pMsg->ReadInt( );
	std::string sSignalID    = pMsg->ReadString( );
	m_pRealCore->SetSoundSourceSignalSource( iSoundSourceID, sSignalID );
}


void CVANetNetworkProtocol::ClientGetSoundReceiverIDs( std::vector<int>& vIDs )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_RECEIVER_IDS, MESSAGE_WITH_ANSWER );
	ClientSendCommand( pMsg );
	vIDs.clear( );
	const int iNumElems = pMsg->ReadInt( );
	for( int i = 0; i < iNumElems; i++ )
		vIDs.push_back( pMsg->ReadInt( ) );
}

void CVANetNetworkProtocol::ServerGetSoundReceiverIDs( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	std::vector<int> vIDs;
	m_pRealCore->GetSoundReceiverIDs( vIDs );
	pMsg->WriteInt( (int)vIDs.size( ) );
	for( size_t i = 0; i < vIDs.size( ); i++ )
		pMsg->WriteInt( vIDs[i] );
}

int CVANetNetworkProtocol::ClientCreateSoundReceiver( const std::string& sName )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_CREATE_SOUND_RECEIVER, MESSAGE_WITH_ANSWER );
	pMsg->WriteString( sName );
	ClientSendCommand( pMsg );
	return pMsg->ReadInt( );
}

void CVANetNetworkProtocol::ServerCreateSoundReceiver( )
{
	CVANetMessage* pMsg     = ServerGetMessage( );
	const std::string sName = pMsg->ReadString( );
	const int iID           = m_pRealCore->CreateSoundReceiver( sName );
	pMsg->WriteInt( iID );
}

int CVANetNetworkProtocol::ClientCreateSoundReceiverExplicitRenderer( const std::string& sRendererID, const std::string& sName )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_CREATE_SOUND_RECEIVER_EXPLICIT_RENDERER, MESSAGE_WITH_ANSWER );
	pMsg->WriteString( sRendererID );
	pMsg->WriteString( sName );
	ClientSendCommand( pMsg );
	return pMsg->ReadInt( );
}

void CVANetNetworkProtocol::ServerCreateSoundReceiverExplicitRenderer( )
{
	CVANetMessage* pMsg           = ServerGetMessage( );
	const std::string sRendererID = pMsg->ReadString( );
	const std::string sName       = pMsg->ReadString( );
	const int iID                 = m_pRealCore->CreateSoundReceiverExplicitRenderer( sRendererID, sName );
	pMsg->WriteInt( iID );
}

int CVANetNetworkProtocol::ClientDeleteSoundReceiver( const int iID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_DELETE_SOUND_RECEIVER, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iID );
	ClientSendCommand( pMsg );
	return pMsg->ReadInt( );
}

void CVANetNetworkProtocol::ServerDeleteSoundReceiver( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const int iID       = pMsg->ReadInt( );
	const int iResult   = m_pRealCore->DeleteSoundReceiver( iID );
	pMsg->WriteInt( iResult );
}

CVASoundReceiverInfo CVANetNetworkProtocol::ClientGetSoundReceiverInfo( const int iID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_RECEIVER_INFO, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iID );
	ClientSendCommand( pMsg );
	return pMsg->ReadSoundReceiverInfo( );
}

void CVANetNetworkProtocol::ServerGetSoundReceiverInfo( )
{
	CVANetMessage* pMsg              = ServerGetMessage( );
	const int iID                    = pMsg->ReadInt( );
	const CVASoundReceiverInfo oInfo = m_pRealCore->GetSoundReceiverInfo( iID );
	pMsg->WriteSoundReceiverInfo( oInfo );
}


void CVANetNetworkProtocol::ClientSetSoundReceiverEnabled( const int iID, const bool bEnabled )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SOUND_RECEIVER_ENABLED, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iID );
	pMsg->WriteBool( bEnabled );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSoundReceiverEnabled( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const int iID       = pMsg->ReadInt( );
	const bool bEnabled = pMsg->ReadBool( );
	m_pRealCore->SetSoundReceiverEnabled( iID, bEnabled );
}

bool CVANetNetworkProtocol::ClientGetSoundReceiverEnabled( const int iID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_RECEIVER_ENABLED, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iID );
	ClientSendCommand( pMsg );
	return pMsg->ReadBool( );
}

void CVANetNetworkProtocol::ServerGetSoundReceiverEnabled( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const int iID       = pMsg->ReadInt( );
	const bool bEnabled = m_pRealCore->GetSoundReceiverEnabled( iID );
	pMsg->WriteBool( bEnabled );
}

std::string CVANetNetworkProtocol::ClientGetSoundReceiverName( const int iID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_RECEIVER_NAME, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iID );
	ClientSendCommand( pMsg );
	return pMsg->ReadString( );
}

void CVANetNetworkProtocol::ServerGetSoundReceiverName( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const int iID       = pMsg->ReadInt( );
	std::string sName   = m_pRealCore->GetSoundReceiverName( iID );
	pMsg->WriteString( sName );
}

void CVANetNetworkProtocol::ClientSetSoundReceiverName( int iID, const std::string& sName )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SOUND_RECEIVER_NAME, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iID );
	pMsg->WriteString( sName );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSoundReceiverName( )
{
	CVANetMessage* pMsg     = ServerGetMessage( );
	const int iID           = pMsg->ReadInt( );
	const std::string sName = pMsg->ReadString( );
	m_pRealCore->SetSoundReceiverName( iID, sName );
}

void CVANetNetworkProtocol::ClientSetSoundReceiverAuralizationMode( const int iID, const int iAuralizationMode )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SOUND_RECEIVER_AURALIZATION_MODE, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iID );
	pMsg->WriteInt( iAuralizationMode );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSoundReceiverAuralizationMode( )
{
	CVANetMessage* pMsg         = ServerGetMessage( );
	const int iID               = pMsg->ReadInt( );
	const int iAuralizationMode = pMsg->ReadInt( );
	m_pRealCore->SetSoundReceiverAuralizationMode( iID, iAuralizationMode );
}

int CVANetNetworkProtocol::ClientGetSoundReceiverAuralizationMode( const int iID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_RECEIVER_AURALIZATION_MODE, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iID );
	ClientSendCommand( pMsg );
	return pMsg->ReadInt( );
}

void CVANetNetworkProtocol::ServerGetSoundReceiverAuralizationMode( )
{
	CVANetMessage* pMsg         = ServerGetMessage( );
	const int iID               = pMsg->ReadInt( );
	const int iAuralizationMode = m_pRealCore->GetSoundReceiverAuralizationMode( iID );
	pMsg->WriteInt( iAuralizationMode );
}

CVAStruct CVANetNetworkProtocol::ClientGetSoundReceiverParameters( const int iID, const CVAStruct& oArgs )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_RECEIVER_PARAMETERS, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iID );
	pMsg->WriteVAStruct( oArgs );
	ClientSendCommand( pMsg );
	CVAStruct oRet;
	pMsg->ReadVAStruct( oRet );
	return oRet;
}

void CVANetNetworkProtocol::ServerGetSoundReceiverParameters( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const int iID       = pMsg->ReadInt( );
	CVAStruct oArgs;
	pMsg->ReadVAStruct( oArgs );
	CVAStruct oRet = m_pRealCore->GetSoundReceiverParameters( iID, oArgs );
	pMsg->WriteVAStruct( oRet );
}

void CVANetNetworkProtocol::ClientSetSoundReceiverParameters( const int iID, const CVAStruct& oParams )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SOUND_RECEIVER_PARAMETERS, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iID );
	pMsg->WriteVAStruct( oParams );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSoundReceiverParameters( )
{
	CVANetMessage* pMsg = ServerGetMessage( );

	int iID = pMsg->ReadInt( );
	CVAStruct oParams;
	pMsg->ReadVAStruct( oParams );

	m_pRealCore->SetSoundReceiverParameters( iID, oParams );

	return;
}

int CVANetNetworkProtocol::ClientGetSoundReceiverDirectivity( const int iID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_RECEIVER_DIRECTIVITY, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iID );
	ClientSendCommand( pMsg );
	return pMsg->ReadInt( );
}

void CVANetNetworkProtocol::ServerGetSoundReceiverDirectivity( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const int iID       = pMsg->ReadInt( );
	int iHRIRID         = m_pRealCore->GetSoundReceiverDirectivity( iID );
	pMsg->WriteInt( iHRIRID );
}

void CVANetNetworkProtocol::ClientSetSoundReceiverDirectivity( const int iID, const int iDirectivityID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SOUND_RECEIVER_DIRECTIVITY, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iID );
	pMsg->WriteInt( iDirectivityID );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSoundReceiverDirectivity( )
{
	CVANetMessage* pMsg    = ServerGetMessage( );
	const int iID          = pMsg->ReadInt( );
	const int iDirectivity = pMsg->ReadInt( );
	m_pRealCore->SetSoundReceiverDirectivity( iID, iDirectivity );
}

void CVANetNetworkProtocol::ClientSetSoundReceiverMuted( const int iID, const bool bMuted )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SOUND_RECEIVER_MUTED, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iID );
	pMsg->WriteBool( bMuted );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSoundReceiverMuted( )
{
	CVANetMessage* pMsg        = ServerGetMessage( );
	const int iSoundReceiverID = pMsg->ReadInt( );
	const bool bMuted          = pMsg->ReadBool( );
	m_pRealCore->SetSoundReceiverMuted( iSoundReceiverID, bMuted );
}

bool CVANetNetworkProtocol::ClientGetSoundReceiverMuted( const int iSoundReceiverID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_RECEIVER_MUTED, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iSoundReceiverID );
	ClientSendCommand( pMsg );
	return pMsg->ReadBool( );
}

void CVANetNetworkProtocol::ServerGetSoundReceiverMuted( )
{
	CVANetMessage* pMsg        = ServerGetMessage( );
	const int iSoundReceiverID = pMsg->ReadInt( );
	const bool bMuted          = m_pRealCore->GetSoundReceiverMuted( iSoundReceiverID );
	pMsg->WriteBool( bMuted );
}


void CVANetNetworkProtocol::ClientGetSoundReceiverPose( const int iID, VAVec3& v3Pos, VAQuat& qOrient )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_RECEIVER_POSE, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iID );
	ClientSendCommand( pMsg );
	v3Pos   = pMsg->ReadVec3( );
	qOrient = pMsg->ReadQuat( );
}

void CVANetNetworkProtocol::ServerGetSoundReceiverPose( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const int iID       = pMsg->ReadInt( );
	VAVec3 v3Pos;
	VAQuat qOrient;
	m_pRealCore->GetSoundReceiverPose( iID, v3Pos, qOrient );
	pMsg->WriteVec3( v3Pos );
	pMsg->WriteQuat( qOrient );
}

void CVANetNetworkProtocol::ClientSetSoundReceiverPose( const int iID, const VAVec3& v3Pos, const VAQuat& qOrient )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SOUND_RECEIVER_POSE, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iID );
	pMsg->WriteVec3( v3Pos );
	pMsg->WriteQuat( qOrient );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSoundReceiverPose( )
{
	CVANetMessage* pMsg  = ServerGetMessage( );
	const int iID        = pMsg->ReadInt( );
	const VAVec3 v3Pos   = pMsg->ReadVec3( );
	const VAQuat qOrient = pMsg->ReadQuat( );
	m_pRealCore->SetSoundReceiverPose( iID, v3Pos, qOrient );
}

VAVec3 CVANetNetworkProtocol::ClientGetSoundReceiverPosition( const int iID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_RECEIVER_POSITION, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iID );
	ClientSendCommand( pMsg );
	return pMsg->ReadVec3( );
}

void CVANetNetworkProtocol::ServerGetSoundReceiverPosition( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const int iID       = pMsg->ReadInt( );
	const VAVec3 v3Pos  = m_pRealCore->GetSoundReceiverPosition( iID );
	pMsg->WriteVec3( v3Pos );
}

void CVANetNetworkProtocol::ClientSetSoundReceiverPosition( const int iID, const VAVec3& v3Pos )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SOUND_RECEIVER_POSITION, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iID );
	pMsg->WriteVec3( v3Pos );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSoundReceiverPosition( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const int iID       = pMsg->ReadInt( );
	const VAVec3 v3Pos  = pMsg->ReadVec3( );
	m_pRealCore->SetSoundReceiverPosition( iID, v3Pos );
}

VAQuat CVANetNetworkProtocol::ClientGetSoundReceiverOrientation( const int iID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_RECEIVER_ORIENTATION, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iID );
	ClientSendCommand( pMsg );
	return pMsg->ReadQuat( );
}

void CVANetNetworkProtocol::ServerGetSoundReceiverOrientation( )
{
	CVANetMessage* pMsg  = ServerGetMessage( );
	const int iID        = pMsg->ReadInt( );
	const VAQuat qOrient = m_pRealCore->GetSoundReceiverOrientation( iID );
	pMsg->WriteQuat( qOrient );
}

void CVANetNetworkProtocol::ClientSetSoundReceiverOrientation( const int iID, const VAQuat& qOrient )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SOUND_RECEIVER_ORIENTATION, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iID );
	pMsg->WriteQuat( qOrient );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSoundReceiverOrientation( )
{
	CVANetMessage* pMsg  = ServerGetMessage( );
	const int iID        = pMsg->ReadInt( );
	const VAQuat qOrient = pMsg->ReadQuat( );
	m_pRealCore->SetSoundReceiverOrientation( iID, qOrient );
}

void CVANetNetworkProtocol::ClientGetSoundReceiverOrientationVU( const int iID, VAVec3& v3View, VAVec3& v3Up )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_RECEIVER_ORIENTATION_VU, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iID );
	ClientSendCommand( pMsg );
	v3View = pMsg->ReadVec3( );
	v3Up   = pMsg->ReadVec3( );
}

void CVANetNetworkProtocol::ServerGetSoundReceiverOrientationVU( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const int iID       = pMsg->ReadInt( );
	VAVec3 v3View, v3Up;
	m_pRealCore->GetSoundReceiverOrientationVU( iID, v3View, v3Up );
	pMsg->WriteVec3( v3View );
	pMsg->WriteVec3( v3Up );
}

void CVANetNetworkProtocol::ClientSetSoundReceiverOrientationVU( const int iID, const VAVec3& v3View, const VAVec3& v3Up )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SOUND_RECEIVER_ORIENTATION_VU, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iID );
	pMsg->WriteVec3( v3View );
	pMsg->WriteVec3( v3Up );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSoundReceiverOrientationVU( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const int iID       = pMsg->ReadInt( );
	const VAVec3 v3View = pMsg->ReadVec3( );
	const VAVec3 v3Up   = pMsg->ReadVec3( );
	m_pRealCore->SetSoundReceiverOrientationVU( iID, v3View, v3Up );
}

VAQuat CVANetNetworkProtocol::ClientGetSoundReceiverHeadAboveTorsoOrientation( const int iID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_RECEIVER_HEAD_ABOVE_TORSO_ORIENTATION, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iID );
	ClientSendCommand( pMsg );
	return pMsg->ReadQuat( );
}

void CVANetNetworkProtocol::ServerGetSoundReceiverHeadAboveTorsoOrientation( )
{
	CVANetMessage* pMsg      = ServerGetMessage( );
	const int iID            = pMsg->ReadInt( );
	const VAQuat qHATOOrient = m_pRealCore->GetSoundReceiverHeadAboveTorsoOrientation( iID );
	pMsg->WriteQuat( qHATOOrient );
}

void CVANetNetworkProtocol::ClientSetSoundReceiverHeadAboveTorsoOrientation( const int iID, const VAQuat& qOrient )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SOUND_RECEIVER_HEAD_ABOVE_TORSO_ORIENTATION, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iID );
	pMsg->WriteQuat( qOrient );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSoundReceiverHeadAboveTorsoOrientation( )
{
	CVANetMessage* pMsg      = ServerGetMessage( );
	const int iID            = pMsg->ReadInt( );
	const VAQuat qHATOOrient = pMsg->ReadQuat( );
	m_pRealCore->SetSoundReceiverHeadAboveTorsoOrientation( iID, qHATOOrient );
}

int CVANetNetworkProtocol::ClientGetSoundReceiverGeometryMesh( const int iID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_RECEIVER_GEOMETRY_MESH, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iID );
	ClientSendCommand( pMsg );
	return pMsg->ReadInt( );
}

void CVANetNetworkProtocol::ServerGetSoundReceiverGeometryMesh( )
{
	CVANetMessage* pMsg       = ServerGetMessage( );
	const int iID             = pMsg->ReadInt( );
	const int iGeometryMeshID = m_pRealCore->GetSoundReceiverGeometryMesh( iID );
	pMsg->WriteInt( iGeometryMeshID );
}

void CVANetNetworkProtocol::ClientSetSoundReceiverGeometryMesh( const int iID, const int iGeometryMeshID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SOUND_RECEIVER_GEOMETRY_MESH, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iID );
	pMsg->WriteInt( iGeometryMeshID );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSoundReceiverGeometryMesh( )
{
	CVANetMessage* pMsg       = ServerGetMessage( );
	const int iID             = pMsg->ReadInt( );
	const int iGeometryMeshID = pMsg->ReadInt( );
	m_pRealCore->SetSoundReceiverGeometryMesh( iID, iGeometryMeshID );
}

void CVANetNetworkProtocol::ClientGetSoundReceiverRealWorldPose( const int iID, VAVec3& v3Pos, VAQuat& qOrient )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_RECEIVER_REAL_WORLD_POSE, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iID );
	ClientSendCommand( pMsg );
	v3Pos   = pMsg->ReadVec3( );
	qOrient = pMsg->ReadQuat( );
}

void CVANetNetworkProtocol::ServerGetSoundReceiverRealWorldPose( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const int iID       = pMsg->ReadInt( );
	VAVec3 v3Pos;
	VAQuat qOrient;
	m_pRealCore->GetSoundReceiverRealWorldPose( iID, v3Pos, qOrient );
	pMsg->WriteVec3( v3Pos );
	pMsg->WriteQuat( qOrient );
}

void CVANetNetworkProtocol::ClientSetSoundReceiverRealWorldPose( const int iID, const VAVec3& v3Pos, const VAQuat& qOrient )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SOUND_RECEIVER_REAL_WORLD_POSE, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iID );
	pMsg->WriteVec3( v3Pos );
	pMsg->WriteQuat( qOrient );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSoundReceiverRealWorldPose( )
{
	CVANetMessage* pMsg  = ServerGetMessage( );
	const int iID        = pMsg->ReadInt( );
	const VAVec3 v3Pos   = pMsg->ReadVec3( );
	const VAQuat qOrient = pMsg->ReadQuat( );
	m_pRealCore->SetSoundReceiverRealWorldPose( iID, v3Pos, qOrient );
}

void CVANetNetworkProtocol::ClientGetSoundReceiverRealWorldPositionOrientationVU( const int iID, VAVec3& v3Pos, VAVec3& v3View, VAVec3& v3Up )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_RECEIVER_REAL_WORLD_POSITION_ORIENTATION_VU, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iID );
	ClientSendCommand( pMsg );
	v3Pos  = pMsg->ReadVec3( );
	v3View = pMsg->ReadVec3( );
	v3Up   = pMsg->ReadVec3( );
}

void CVANetNetworkProtocol::ServerGetSoundReceiverRealWorldPositionOrientationVU( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const int iID       = pMsg->ReadInt( );
	VAVec3 v3Pos, v3View, v3Up;
	m_pRealCore->GetSoundReceiverRealWorldPositionOrientationVU( iID, v3Pos, v3View, v3Up );
	pMsg->WriteVec3( v3Pos );
	pMsg->WriteVec3( v3View );
	pMsg->WriteVec3( v3Up );
}

void CVANetNetworkProtocol::ClientSetSoundReceiverRealWorldPositionOrientationVU( const int iID, const VAVec3& v3Pos, const VAVec3& v3View, const VAVec3& v3Up )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SOUND_RECEIVER_REAL_WORLD_POSITION_ORIENTATION_VU, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iID );
	pMsg->WriteVec3( v3Pos );
	pMsg->WriteVec3( v3View );
	pMsg->WriteVec3( v3Up );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSoundReceiverRealWorldPositionOrientationVU( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const int iID       = pMsg->ReadInt( );
	const VAVec3 v3Pos  = pMsg->ReadVec3( );
	const VAVec3 v3View = pMsg->ReadVec3( );
	const VAVec3 v3Up   = pMsg->ReadVec3( );
	m_pRealCore->SetSoundReceiverRealWorldPositionOrientationVU( iID, v3Pos, v3View, v3Up );
}

VAQuat CVANetNetworkProtocol::ClientGetSoundReceiverRealWorldTorsoOrientation( const int iID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_RECEIVER_REAL_WORLD_TORSO_ORIENTATION, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iID );
	ClientSendCommand( pMsg );
	return pMsg->ReadQuat( );
}

void CVANetNetworkProtocol::ServerGetSoundReceiverRealWorldTorsoOrientation( )
{
	CVANetMessage* pMsg      = ServerGetMessage( );
	const int iID            = pMsg->ReadInt( );
	const VAQuat qHATOOrient = m_pRealCore->GetSoundReceiverRealWorldHeadAboveTorsoOrientation( iID );
	pMsg->WriteQuat( qHATOOrient );
}

void CVANetNetworkProtocol::ClientSetSoundReceiverRealWorldTorsoOrientation( const int iID, const VAQuat& qOrient )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SOUND_RECEIVER_REAL_WORLD_TORSO_ORIENTATION, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iID );
	pMsg->WriteQuat( qOrient );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSoundReceiverRealWorldTorsoOrientation( )
{
	CVANetMessage* pMsg      = ServerGetMessage( );
	const int iID            = pMsg->ReadInt( );
	const VAQuat qHATOOrient = pMsg->ReadQuat( );
	m_pRealCore->SetSoundReceiverRealWorldHeadAboveTorsoOrientation( iID, qHATOOrient );
}


std::string CVANetNetworkProtocol::ClientCreateScene( const CVAStruct& oParams, const std::string& sName )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SCENE_CREATE, MESSAGE_WITH_ANSWER );
	pMsg->WriteVAStruct( oParams );
	pMsg->WriteString( sName );
	ClientSendCommand( pMsg );
	return pMsg->ReadString( );
}

void CVANetNetworkProtocol::ServerCreateScene( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	CVAStruct oParams;
	pMsg->ReadVAStruct( oParams );
	const std::string sName = pMsg->ReadString( );
	const std::string sID   = m_pRealCore->CreateScene( oParams, sName );
	pMsg->WriteString( sID );
}

void CVANetNetworkProtocol::ClientGetSceneIDs( std::vector<std::string>& vsIDs )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SCENE_IDS, MESSAGE_WITH_ANSWER );
	ClientSendCommand( pMsg );
	const int iNumElems = pMsg->ReadInt( );
	for( int i = 0; i < iNumElems; i++ )
		vsIDs.push_back( pMsg->ReadString( ) );
}

void CVANetNetworkProtocol::ServerGetSceneIDs( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	std::vector<std::string> vsIDs;
	m_pRealCore->GetSceneIDs( vsIDs );
	pMsg->WriteInt( int( vsIDs.size( ) ) );
	for( size_t i = 0; i < vsIDs.size( ); i++ )
		pMsg->WriteString( vsIDs[i] );
}

CVASceneInfo CVANetNetworkProtocol::ClientGetSceneInfo( const std::string& sID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SCENE_INFO, MESSAGE_WITH_ANSWER );
	pMsg->WriteString( sID );
	ClientSendCommand( pMsg );
	CVASceneInfo oSceneInfo = pMsg->ReadSceneInfo( );
	return oSceneInfo;
}

void CVANetNetworkProtocol::ServerGetSceneInfo( )
{
	CVANetMessage* pMsg     = ServerGetMessage( );
	const std::string sID   = pMsg->ReadString( );
	CVASceneInfo oSceneInfo = m_pRealCore->GetSceneInfo( sID );
	pMsg->WriteSceneInfo( oSceneInfo );
}

std::string CVANetNetworkProtocol::ClientGetSceneName( const std::string& sID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SCENE_NAME, MESSAGE_WITH_ANSWER );
	pMsg->WriteString( sID );
	ClientSendCommand( pMsg );
	return pMsg->ReadString( );
}

void CVANetNetworkProtocol::ServerGetSceneName( )
{
	CVANetMessage* pMsg     = ServerGetMessage( );
	const std::string sID   = pMsg->ReadString( );
	const std::string sName = m_pRealCore->GetSceneName( sID );
	pMsg->WriteString( sName );
}

void CVANetNetworkProtocol::ClientSetSceneName( const std::string& sID, const std::string& sName )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SCENE_NAME, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteString( sID );
	pMsg->WriteString( sName );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSceneName( )
{
	CVANetMessage* pMsg     = ServerGetMessage( );
	const std::string sID   = pMsg->ReadString( );
	const std::string sName = pMsg->ReadString( );
	m_pRealCore->SetSceneName( sID, sName );
}

bool CVANetNetworkProtocol::ClientGetSceneEnabled( const std::string& sID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SCENE_ENABLED, MESSAGE_WITH_ANSWER );
	pMsg->WriteString( sID );
	ClientSendCommand( pMsg );
	return pMsg->ReadBool( );
}

void CVANetNetworkProtocol::ServerGetSceneEnabled( )
{
	CVANetMessage* pMsg   = ServerGetMessage( );
	const std::string sID = pMsg->ReadString( );
	const bool bEnabled   = m_pRealCore->GetSceneEnabled( sID );
	pMsg->WriteBool( bEnabled );
}

void CVANetNetworkProtocol::ClientSetSceneEnabled( const std::string& sID, const bool bEnabled )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SCENE_ENABLED, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteString( sID );
	pMsg->WriteBool( bEnabled );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSceneEnabled( )
{
	CVANetMessage* pMsg   = ServerGetMessage( );
	const std::string sID = pMsg->ReadString( );
	const bool bEnabled   = pMsg->ReadBool( );
	m_pRealCore->SetSceneEnabled( sID, bEnabled );
}


int CVANetNetworkProtocol::ClientCreateSoundPortal( const std::string& sName )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_CREATE_SOUND_PORTAL, MESSAGE_WITH_ANSWER );
	pMsg->WriteString( sName );
	ClientSendCommand( pMsg );
	return pMsg->ReadInt( );
}

void CVANetNetworkProtocol::ServerCreateSoundPortal( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const int iID       = m_pRealCore->CreateSoundPortal( );
	pMsg->WriteInt( iID );
}

void CVANetNetworkProtocol::ClientGetSoundPortalIDs( std::vector<int>& vPortalIDs )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_PORTAL_IDS, MESSAGE_WITH_ANSWER );
	ClientSendCommand( pMsg );
	vPortalIDs.clear( );
	int iNumElems = pMsg->ReadInt( );
	for( int i = 0; i < iNumElems; i++ )
		vPortalIDs.push_back( pMsg->ReadInt( ) );
}

void CVANetNetworkProtocol::ServerGetSoundPortalIDs( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	std::vector<int> vIDs;
	m_pRealCore->GetSoundPortalIDs( vIDs );
	pMsg->WriteInt( (int)vIDs.size( ) );
	for( size_t i = 0; i < vIDs.size( ); i++ )
		pMsg->WriteInt( vIDs[i] );
}

CVASoundPortalInfo CVANetNetworkProtocol::ClientGetSoundPortalInfo( const int iID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_PORTAL_INFO, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iID );
	ClientSendCommand( pMsg );
	return pMsg->ReadSoundPortalInfo( );
}

void CVANetNetworkProtocol::ServerGetSoundPortalInfo( )
{
	CVANetMessage* pMsg            = ServerGetMessage( );
	const int iID                  = pMsg->ReadInt( );
	const CVASoundPortalInfo oInfo = m_pRealCore->GetSoundPortalInfo( iID );
	pMsg->WriteSoundPortalInfo( oInfo );
}

void CVANetNetworkProtocol::ClientSetSoundPortalName( const int iPortalID, const std::string& sName )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SOUND_PORTAL_NAME, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iPortalID );
	pMsg->WriteString( sName );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSoundPortalName( )
{
	CVANetMessage* pMsg           = ServerGetMessage( );
	const int iPortalID           = pMsg->ReadInt( );
	const std::string sPortalName = pMsg->ReadString( );
	m_pRealCore->SetSoundPortalName( iPortalID, sPortalName );
}

void CVANetNetworkProtocol::ClientSetSoundPortalMaterial( const int iSoundPortalID, const int iMaterialID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SOUND_PORTAL_MATERIAL, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iSoundPortalID );
	pMsg->WriteInt( iMaterialID );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSoundPortalMaterial( )
{
	CVANetMessage* pMsg      = ServerGetMessage( );
	const int iSoundPortalID = pMsg->ReadInt( );
	const int iMaterialID    = pMsg->ReadInt( );
	m_pRealCore->SetSoundPortalMaterial( iSoundPortalID, iMaterialID );
}

int CVANetNetworkProtocol::ClientGetSoundPortalMaterial( const int iSoundPortalID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_PORTAL_MATERIAL, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iSoundPortalID );
	ClientSendCommand( pMsg );
	return pMsg->ReadInt( );
}

void CVANetNetworkProtocol::ServerGetSoundPortalMaterial( )
{
	CVANetMessage* pMsg   = ServerGetMessage( );
	const int iID         = pMsg->ReadInt( );
	const int iMaterialID = m_pRealCore->GetSoundPortalMaterial( iID );
	pMsg->WriteInt( iMaterialID );
}

void CVANetNetworkProtocol::ClientSetSoundPortalNextPortal( const int iSoundPortalID, const int iNextSoundPortalID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SOUND_PORTAL_NEXT_SOUND_PORTAL, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iSoundPortalID );
	pMsg->WriteInt( iNextSoundPortalID );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSoundPortalNextPortal( )
{
	CVANetMessage* pMsg          = ServerGetMessage( );
	const int iSoundPortalID     = pMsg->ReadInt( );
	const int iNextSoundPortalID = pMsg->ReadInt( );
	m_pRealCore->SetSoundPortalNextPortal( iSoundPortalID, iNextSoundPortalID );
}

int CVANetNetworkProtocol::ClientGetSoundPortalNextPortal( const int iSoundPortalID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_PORTAL_NEXT_SOUND_PORTAL, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iSoundPortalID );
	ClientSendCommand( pMsg );
	return pMsg->ReadInt( );
}

void CVANetNetworkProtocol::ServerGetSoundPortalNextPortal( )
{
	CVANetMessage* pMsg          = ServerGetMessage( );
	const int iID                = pMsg->ReadInt( );
	const int iNextSoundPortalID = m_pRealCore->GetSoundPortalNextPortal( iID );
	pMsg->WriteInt( iNextSoundPortalID );
}

void CVANetNetworkProtocol::ClientSetSoundPortalSoundReceiver( const int iSoundPortalID, const int iSoundReceiverID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SOUND_PORTAL_SOUND_RECEIVER, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iSoundPortalID );
	pMsg->WriteInt( iSoundReceiverID );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSoundPortalSoundReceiver( )
{
	CVANetMessage* pMsg        = ServerGetMessage( );
	const int iID              = pMsg->ReadInt( );
	const int iSoundReceiverID = pMsg->ReadInt( );
	m_pRealCore->SetSoundPortalSoundReceiver( iID, iSoundReceiverID );
}

int CVANetNetworkProtocol::ClientGetSoundPortalSoundReceiver( const int iSoundPortalID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_PORTAL_SOUND_RECEIVER, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iSoundPortalID );
	ClientSendCommand( pMsg );
	return pMsg->ReadInt( );
}

void CVANetNetworkProtocol::ServerGetSoundPortalSoundReceiver( )
{
	CVANetMessage* pMsg        = ServerGetMessage( );
	const int iID              = pMsg->ReadInt( );
	const int iSoundReceiverID = m_pRealCore->GetSoundPortalSoundReceiver( iID );
	pMsg->WriteInt( iSoundReceiverID );
}

void CVANetNetworkProtocol::ClientSetSoundPortalSoundSource( const int iSoundPortalID, const int iSoundSourceID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SOUND_PORTAL_SOUND_SOURCE, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iSoundPortalID );
	pMsg->WriteInt( iSoundSourceID );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSoundPortalSoundSource( )
{
	CVANetMessage* pMsg      = ServerGetMessage( );
	const int iSoundPortalID = pMsg->ReadInt( );
	const int iSoundSourceID = pMsg->ReadInt( );
	m_pRealCore->SetSoundPortalSoundSource( iSoundPortalID, iSoundSourceID );
}

int CVANetNetworkProtocol::ClientGetSoundPortalSoundSource( const int iSoundPortalID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_PORTAL_SOUND_SOURCE, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iSoundPortalID );
	ClientSendCommand( pMsg );
	return pMsg->ReadInt( );
}

void CVANetNetworkProtocol::ServerGetSoundPortalSoundSource( )
{
	CVANetMessage* pMsg      = ServerGetMessage( );
	const int iID            = pMsg->ReadInt( );
	const int iSoundSourceID = m_pRealCore->GetSoundPortalSoundSource( iID );
	pMsg->WriteInt( iSoundSourceID );
}

void CVANetNetworkProtocol::ClientSetSoundPortalPosition( const int iSoundPortalID, const VAVec3& vPos )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SOUND_PORTAL_POSITION, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iSoundPortalID );
	pMsg->WriteVec3( vPos );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSoundPortalPosition( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	int iSoundPortalID  = pMsg->ReadInt( );
	const VAVec3 v3Pos  = pMsg->ReadVec3( );
	m_pRealCore->SetSoundPortalPosition( iSoundPortalID, v3Pos );
}

VAVec3 CVANetNetworkProtocol::ClientGetSoundPortalPosition( const int iSoundPortalID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_PORTAL_POSITION, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iSoundPortalID );
	ClientSendCommand( pMsg );
	const VAVec3 vPos = pMsg->ReadVec3( );
	return vPos;
}

void CVANetNetworkProtocol::ServerGetSoundPortalPosition( )
{
	CVANetMessage* pMsg      = ServerGetMessage( );
	const int iSoundPortalID = pMsg->ReadInt( );
	VAVec3 v3Pos             = m_pRealCore->GetSoundPortalPosition( iSoundPortalID );
	pMsg->WriteVec3( v3Pos );
}

void CVANetNetworkProtocol::ClientSetSoundPortalOrientation( const int iID, const VAQuat& qOrient )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SOUND_PORTAL_ORIENTATION, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iID );
	pMsg->WriteQuat( qOrient );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSoundPortalOrientation( )
{
	CVANetMessage* pMsg  = ServerGetMessage( );
	const int iID        = pMsg->ReadInt( );
	const VAQuat qOrient = pMsg->ReadQuat( );
	m_pRealCore->SetSoundPortalOrientation( iID, qOrient );
}

VAQuat CVANetNetworkProtocol::ClientGetSoundPortalOrientation( const int iID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_PORTAL_ORIENTATION, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iID );
	ClientSendCommand( pMsg );
	const VAQuat qOrient = pMsg->ReadQuat( );
	return qOrient;
}

void CVANetNetworkProtocol::ServerGetSoundPortalOrientation( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const int iID       = pMsg->ReadInt( );
	VAQuat qOrient      = m_pRealCore->GetSoundPortalOrientation( iID );
	pMsg->WriteQuat( qOrient );
}

void CVANetNetworkProtocol::ClientSetSoundPortalEnabled( const int iID, const bool bEnabled )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SOUND_PORTAL_ENABLED, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iID );
	pMsg->WriteBool( bEnabled );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSoundPortalEnabled( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const int iID       = pMsg->ReadInt( );
	const bool bEnabled = pMsg->ReadBool( );
	m_pRealCore->SetSoundPortalEnabled( iID, bEnabled );
}

bool CVANetNetworkProtocol::ClientGetSoundPortalEnabled( const int iID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_PORTAL_ENABLED, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iID );
	ClientSendCommand( pMsg );
	return pMsg->ReadBool( );
}

void CVANetNetworkProtocol::ServerGetSoundPortalEnabled( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const int iID       = pMsg->ReadInt( );
	const bool bEnabled = m_pRealCore->GetSoundPortalEnabled( iID );
	pMsg->WriteBool( bEnabled );
}

std::string CVANetNetworkProtocol::ClientGetSoundPortalName( const int iPortalID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_PORTAL_NAME, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iPortalID );
	ClientSendCommand( pMsg );
	return pMsg->ReadString( );
}

void CVANetNetworkProtocol::ServerGetSoundPortalName( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const int iPortalID = pMsg->ReadInt( );
	std::string sName   = m_pRealCore->GetSoundPortalName( iPortalID );
	pMsg->WriteString( sName );
}

CVAStruct CVANetNetworkProtocol::ClientGetSoundPortalParameters( const int iID, const CVAStruct& oArgs )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_SOUND_PORTAL_PARAMETERS, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iID );
	pMsg->WriteVAStruct( oArgs );
	ClientSendCommand( pMsg );
	CVAStruct oParams;
	pMsg->ReadVAStruct( oParams );
	return oParams;
}

void CVANetNetworkProtocol::ServerGetSoundPortalParameters( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const int iID       = pMsg->ReadInt( );
	CVAStruct oArgs;
	pMsg->ReadVAStruct( oArgs );
	CVAStruct oParams = m_pRealCore->GetSoundPortalParameters( iID, oArgs );
	pMsg->WriteVAStruct( oParams );
}

void CVANetNetworkProtocol::ClientSetSoundPortalParameters( const int iPortalID, const CVAStruct& oParams )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_SOUND_PORTAL_PARAMETERS, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iPortalID );
	pMsg->WriteVAStruct( oParams );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetSoundPortalParameters( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	int iPortalID       = pMsg->ReadInt( );
	CVAStruct oParams;
	pMsg->ReadVAStruct( oParams );
	m_pRealCore->SetSoundPortalParameters( iPortalID, oParams );
}


int CVANetNetworkProtocol::ClientCreateGeometryMesh( const CVAGeometryMesh& oMesh, const std::string& sName )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_CREATE_GEOMETRY_MESH, MESSAGE_WITH_ANSWER );
	pMsg->WriteGeometryMesh( oMesh );
	pMsg->WriteString( sName );
	ClientSendCommand( pMsg );
	const int iID = pMsg->ReadInt( );
	return iID;
}

void CVANetNetworkProtocol::ServerCreateGeometryMesh( )
{
	CVANetMessage* pMsg         = ServerGetMessage( );
	const CVAGeometryMesh oMesh = pMsg->ReadGeometryMesh( );
	const std::string sName     = pMsg->ReadString( );
	const int iID               = m_pRealCore->CreateGeometryMesh( oMesh, sName );
	pMsg->WriteInt( iID );
}

int CVANetNetworkProtocol::ClientCreateGeometryMeshFromParameters( const CVAStruct& oStruct, const std::string& sName )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_CREATE_GEOMETRY_MESH_FROM_PARAMETERS, MESSAGE_WITH_ANSWER );
	pMsg->WriteVAStruct( oStruct );
	pMsg->WriteString( sName );
	ClientSendCommand( pMsg );
	const int iID = pMsg->ReadInt( );
	return iID;
}

void CVANetNetworkProtocol::ServerCreateGeometryMeshFromParameters( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	CVAStruct oParams;
	pMsg->ReadVAStruct( oParams );
	const std::string sName = pMsg->ReadString( );
	const int iID           = m_pRealCore->CreateGeometryMeshFromParameters( oParams, sName );
	pMsg->WriteInt( iID );
}

bool CVANetNetworkProtocol::ClientDeleteGeometryMesh( const int iID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_DELETE_GEOMETRY_MESH, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iID );
	ClientSendCommand( pMsg );
	return pMsg->ReadBool( );
}

void CVANetNetworkProtocol::ServerDeleteGeometryMesh( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const int iID       = pMsg->ReadInt( );
	const bool bRet     = m_pRealCore->DeleteGeometryMesh( iID );
	pMsg->WriteBool( bRet );
}

CVAGeometryMesh CVANetNetworkProtocol::ClientGetGeometryMesh( const int iID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_GEOMETRY_MESH, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iID );
	ClientSendCommand( pMsg );
	return pMsg->ReadGeometryMesh( );
}

void CVANetNetworkProtocol::ServerGetGeometryMesh( )
{
	CVANetMessage* pMsg         = ServerGetMessage( );
	const int iID               = pMsg->ReadInt( );
	const CVAGeometryMesh oMesh = m_pRealCore->GetGeometryMesh( iID );
	pMsg->WriteGeometryMesh( oMesh );
}

void CVANetNetworkProtocol::ClientGetGeometryMeshIDs( std::vector<int>& viIDs )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_GEOMETRY_MESH_IDS, MESSAGE_WITH_ANSWER );
	ClientSendCommand( pMsg );
	viIDs.clear( );
	const int iNumElems = pMsg->ReadInt( );
	for( int i = 0; i < iNumElems; i++ )
		viIDs.push_back( pMsg->ReadInt( ) );
}

void CVANetNetworkProtocol::ServerGetGeometryMeshIDs( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	std::vector<int> vIDs;
	m_pRealCore->GetSoundSourceIDs( vIDs );
	pMsg->WriteInt( (int)vIDs.size( ) );
	for( size_t i = 0; i < vIDs.size( ); i++ )
		pMsg->WriteInt( vIDs[i] );
}

void CVANetNetworkProtocol::ClientSetGeometryMeshName( const int iID, const std::string& sName )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_GEOMETRY_MESH_NAME, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iID );
	pMsg->WriteString( sName );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerClientSetGeometryMeshName( )
{
	CVANetMessage* pMsg     = ServerGetMessage( );
	const int iID           = pMsg->ReadInt( );
	const std::string sName = pMsg->ReadString( );
	m_pRealCore->SetGeometryMeshName( iID, sName );
}

std::string CVANetNetworkProtocol::ClientGetGeometryMeshName( const int iID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_GEOMETRY_MESH_NAME, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iID );
	ClientSendCommand( pMsg );
	return pMsg->ReadString( );
}

void CVANetNetworkProtocol::ServerGetGeometryMeshName( )
{
	CVANetMessage* pMsg          = ServerGetMessage( );
	const int iID                = pMsg->ReadInt( );
	const CVAGeometryMesh& oMesh = m_pRealCore->GetGeometryMesh( iID );
	pMsg->WriteGeometryMesh( oMesh );
}

CVAStruct CVANetNetworkProtocol::ClientGetGeometryMeshParameters( const int iID, const CVAStruct& oArgs )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_GEOMETRY_MESH_PARAMETERS, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iID );
	pMsg->WriteVAStruct( oArgs );
	ClientSendCommand( pMsg );
	CVAStruct oParams;
	pMsg->ReadVAStruct( oParams );
	return oParams;
}

void CVANetNetworkProtocol::ServerGetGeometryMeshParameters( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const int iID       = pMsg->ReadInt( );
	CVAStruct oArgs;
	pMsg->ReadVAStruct( oArgs );
	CVAStruct oParams = m_pRealCore->GetGeometryMeshParameters( iID, oArgs );
	pMsg->WriteVAStruct( oParams );
}

void CVANetNetworkProtocol::ClientSetGeometryMeshParameters( const int iPortalID, const CVAStruct& oParams )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_GEOMETRY_MESH_PARAMETERS, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iPortalID );
	pMsg->WriteVAStruct( oParams );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetGeometryMeshParameters( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const int iID       = pMsg->ReadInt( );
	CVAStruct oParams;
	pMsg->ReadVAStruct( oParams );
	m_pRealCore->SetGeometryMeshParameters( iID, oParams );
}

bool CVANetNetworkProtocol::ClientGetGeometryMeshEnabled( const int iID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_GEOMETRY_MESH_ENABLED, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iID );
	ClientSendCommand( pMsg );
	return pMsg->ReadBool( );
}

void CVANetNetworkProtocol::ServerGetGeometryMeshEnabled( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const int iID       = pMsg->ReadInt( );
	const bool bEnabled = m_pRealCore->GetGeometryMeshEnabled( iID );
	pMsg->WriteBool( bEnabled );
}

void CVANetNetworkProtocol::ClientSetGeometryMeshEnabled( const int iID, const bool bEnabled )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_GEOMETRY_MESH_ENABLED, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iID );
	pMsg->WriteBool( bEnabled );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetGeometryMeshEnabled( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const int iID       = pMsg->ReadInt( );
	const bool bEnabled = pMsg->ReadBool( );
	m_pRealCore->SetGeometryMeshEnabled( iID, bEnabled );
}


void CVANetNetworkProtocol::ClientSetInputGain( const double dGain )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_INPUT_GAIN, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteDouble( dGain );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetInputGain( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const double dGain  = pMsg->ReadDouble( );
	m_pRealCore->SetInputGain( dGain );
}

double CVANetNetworkProtocol::ClientGetInputGain( )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_INPUT_GAIN, MESSAGE_WITH_ANSWER );
	ClientSendCommand( pMsg );
	return pMsg->ReadDouble( );
}

void CVANetNetworkProtocol::ServerGetInputGain( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	double dGain        = m_pRealCore->GetInputGain( );
	pMsg->WriteDouble( dGain );
}

void CVANetNetworkProtocol::ClientSetInputMuted( const bool bMuted )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_INPUT_MUTED, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteBool( bMuted );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetInputMuted( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	m_pRealCore->SetInputMuted( pMsg->ReadBool( ) );
}

bool CVANetNetworkProtocol::ClientGetInputMuted( )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_INPUT_MUTED, MESSAGE_WITH_ANSWER );
	ClientSendCommand( pMsg );
	return pMsg->ReadBool( );
}

void CVANetNetworkProtocol::ServerGetInputMuted( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	bool bMuted         = m_pRealCore->GetInputMuted( );
	pMsg->WriteBool( bMuted );
}

void CVANetNetworkProtocol::ClientSetOutputGain( const double dGain )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_OUTPUT_GAIN, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteDouble( dGain );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetOutputGain( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	double dGain        = pMsg->ReadDouble( );
	m_pRealCore->SetOutputGain( dGain );
}

double CVANetNetworkProtocol::ClientGetOutputGain( )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_OUTPUT_GAIN, MESSAGE_WITH_ANSWER );
	ClientSendCommand( pMsg );
	return pMsg->ReadDouble( );
}

void CVANetNetworkProtocol::ServerGetOutputGain( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const double dGain  = m_pRealCore->GetOutputGain( );
	pMsg->WriteDouble( dGain );
}

void CVANetNetworkProtocol::ClientSetGlobalAuralizationMode( const int iAuralizationMode )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_GLOBAL_AURALIZATION_MODE, MESSAGE_CONDITIONAL_EXCEPTION );
	pMsg->WriteInt( iAuralizationMode );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetGlobalAuralizationMode( )
{
	CVANetMessage* pMsg         = ServerGetMessage( );
	const int iAuralizationMode = pMsg->ReadInt( );
	m_pRealCore->SetGlobalAuralizationMode( iAuralizationMode );
}

int CVANetNetworkProtocol::ClientGetGlobalAuralizationMode( )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_GLOBAL_AURALIZATION_MODE, MESSAGE_WITH_ANSWER );
	ClientSendCommand( pMsg );
	return pMsg->ReadInt( );
}

void CVANetNetworkProtocol::ServerGetGlobalAuralizationMode( )
{
	CVANetMessage* pMsg         = ServerGetMessage( );
	const int iAuralizationMode = m_pRealCore->GetGlobalAuralizationMode( );
	pMsg->WriteInt( iAuralizationMode );
}

void CVANetNetworkProtocol::ClientSetOutputMuted( const bool bMuted )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_OUTPUT_MUTED, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteBool( bMuted );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetOutputMuted( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	m_pRealCore->SetOutputMuted( pMsg->ReadBool( ) );
}

bool CVANetNetworkProtocol::ClientGetOutputMuted( )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_OUTPUT_MUTED, MESSAGE_WITH_ANSWER );
	ClientSendCommand( pMsg );
	return pMsg->ReadBool( );
}

void CVANetNetworkProtocol::ServerGetOutputMuted( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	bool bMuted         = m_pRealCore->GetOutputMuted( );
	pMsg->WriteBool( bMuted );
}

double CVANetNetworkProtocol::ClientGetCoreClock( )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_CORE_CLOCK, MESSAGE_WITH_ANSWER );
	ClientSendCommand( pMsg );
	return pMsg->ReadDouble( );
}

void CVANetNetworkProtocol::ServerGetCoreClock( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const double dClock = m_pRealCore->GetCoreClock( );
	pMsg->WriteDouble( dClock );
}

void CVANetNetworkProtocol::ClientSetCoreClock( const double dSeconds )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_CORE_CLOCK, MESSAGE_ENFORCED_EXCEPTION );
	pMsg->WriteDouble( dSeconds );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetCoreClock( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	double dClock       = pMsg->ReadDouble( );
	m_pRealCore->SetCoreClock( dClock );
}

std::string CVANetNetworkProtocol::ClientSubstituteMacros( const std::string& sStr )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SUBSTITUTE_MACROS, MESSAGE_WITH_ANSWER );
	pMsg->WriteString( sStr );
	ClientSendCommand( pMsg );
	return pMsg->ReadString( );
}

void CVANetNetworkProtocol::ServerSubstituteMacros( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	std::string sStr    = pMsg->ReadString( );
	pMsg->WriteString( m_pRealCore->SubstituteMacros( sStr ) );
}

void CVANetNetworkProtocol::ClientSetRenderingModuleGain( const std::string& sID, const double dGain )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_RENDERER_SET_GAIN, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteString( sID );
	pMsg->WriteDouble( dGain );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetRenderingModuleGain( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	std::string sID     = pMsg->ReadString( );
	double dGain        = pMsg->ReadDouble( );
	m_pRealCore->SetRenderingModuleGain( sID, dGain );
}

void CVANetNetworkProtocol::ClientSetRenderingModuleMuted( const std::string& sID, const bool bMuted )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_RENDERER_SET_MUTED, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteString( sID );
	pMsg->WriteBool( bMuted );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetRenderingModuleMuted( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	std::string sID     = pMsg->ReadString( );
	bool bMuted         = pMsg->ReadBool( );
	m_pRealCore->SetRenderingModuleMuted( sID, bMuted );
}

double CVANetNetworkProtocol::ClientGetRenderingModuleGain( const std::string& sID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_RENDERER_GET_GAIN, MESSAGE_WITH_ANSWER );
	pMsg->WriteString( sID );
	ClientSendCommand( pMsg );
	return pMsg->ReadDouble( );
}

void CVANetNetworkProtocol::ServerGetRenderingModuleGain( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	std::string sID     = pMsg->ReadString( );
	double dGain        = m_pRealCore->GetRenderingModuleGain( sID );
	pMsg->WriteDouble( dGain );
}

void CVANetNetworkProtocol::ClientSetRenderingModuleAuralizationMode( const std::string& sID, const int iAM )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_RENDERER_SET_AURALIZATION_MODE, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteString( sID );
	pMsg->WriteInt( iAM );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetRenderingModuleAuralizationMode( )
{
	CVANetMessage* pMsg   = ServerGetMessage( );
	const std::string sID = pMsg->ReadString( );
	const int iAM         = pMsg->ReadInt( );
	m_pRealCore->SetRenderingModuleAuralizationMode( sID, iAM );
}

int CVANetNetworkProtocol::ClientGetRenderingModuleAuralizationMode( const std::string& sID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_RENDERER_GET_AURALIZATION_MODE, MESSAGE_WITH_ANSWER );
	pMsg->WriteString( sID );
	ClientSendCommand( pMsg );
	return pMsg->ReadInt( );
}

void CVANetNetworkProtocol::ServerGetRenderingModuleAuralizationMode( )
{
	CVANetMessage* pMsg   = ServerGetMessage( );
	const std::string sID = pMsg->ReadString( );
	const int iAM         = m_pRealCore->GetRenderingModuleAuralizationMode( sID );
	pMsg->WriteInt( iAM );
}

void CVANetNetworkProtocol::ClientSetRenderingModuleParameters( const std::string& sID, const CVAStruct& oParams )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_RENDERER_SET_PARAMETERS, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteString( sID );
	pMsg->WriteVAStruct( oParams );
	ClientSendCommand( pMsg );
	return;
}

void CVANetNetworkProtocol::ServerSetRenderingModuleParameters( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	std::string sID     = pMsg->ReadString( );
	CVAStruct oParams;
	pMsg->ReadVAStruct( oParams );
	m_pRealCore->SetRenderingModuleParameters( sID, oParams );
	return;
}

CVAStruct CVANetNetworkProtocol::ClientGetRenderingModuleParameters( const std::string& sID, const CVAStruct& oParams )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_RENDERER_GET_PARAMETERS, MESSAGE_WITH_ANSWER );
	pMsg->WriteString( sID );
	pMsg->WriteVAStruct( oParams );
	ClientSendCommand( pMsg );
	CVAStruct oRet;
	pMsg->ReadVAStruct( oRet );
	return oRet;
}

void CVANetNetworkProtocol::ServerGetRenderingModuleParameters( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	std::string sID     = pMsg->ReadString( );
	CVAStruct oParams;
	pMsg->ReadVAStruct( oParams );
	CVAStruct oRet = m_pRealCore->GetRenderingModuleParameters( sID, oParams );
	pMsg->WriteVAStruct( oRet );
	return;
}

void CVANetNetworkProtocol::ClientGetRenderingModuleInfos( std::vector<CVAAudioRendererInfo>& voRenderer, const bool bFilterEnabled )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_RENDERER_GET_INFOS, MESSAGE_WITH_ANSWER );
	pMsg->WriteBool( bFilterEnabled );
	ClientSendCommand( pMsg );
	int iSize = pMsg->ReadInt( );
	for( int i = 0; i < iSize; i++ )
		voRenderer.push_back( pMsg->ReadAudioRenderingModuleInfo( ) );
}

void CVANetNetworkProtocol::ServerGetRenderingModuleInfos( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	bool bFilterEnabled = pMsg->ReadBool( );
	std::vector<CVAAudioRendererInfo> voRenderer;
	m_pRealCore->GetRenderingModules( voRenderer, bFilterEnabled );
	pMsg->WriteInt( (int)voRenderer.size( ) );
	for( size_t i = 0; i < voRenderer.size( ); i++ )
		pMsg->WriteAudioRenderingModuleInfo( voRenderer[i] );

	return;
}

void CVANetNetworkProtocol::ClientGetReproductionModuleInfos( std::vector<CVAAudioReproductionInfo>& voRepros, const bool bFilterEnabled )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_REPRODUCTION_GET_INFOS, MESSAGE_WITH_ANSWER );
	pMsg->WriteBool( bFilterEnabled );
	ClientSendCommand( pMsg );
	int iSize = pMsg->ReadInt( );
	for( int i = 0; i < iSize; i++ )
		voRepros.push_back( pMsg->ReadAudioReproductionModuleInfo( ) );
}

void CVANetNetworkProtocol::ServerGetReproductionModuleInfos( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	bool bFilterEnabled = pMsg->ReadBool( );
	std::vector<CVAAudioReproductionInfo> voRepros;
	m_pRealCore->GetReproductionModules( voRepros, bFilterEnabled );
	pMsg->WriteInt( (int)voRepros.size( ) );
	for( size_t i = 0; i < voRepros.size( ); i++ )
		pMsg->WriteAudioReproductionModuleInfo( voRepros[i] );
}

void CVANetNetworkProtocol::ClientSetReproductionModuleParameters( const std::string& sID, const CVAStruct& oParams )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_REPRODUCTION_SET_PARAMETERS, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteString( sID );
	pMsg->WriteVAStruct( oParams );
	ClientSendCommand( pMsg );
	return;
}

void CVANetNetworkProtocol::ServerSetReproductionModuleParameters( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	std::string sID     = pMsg->ReadString( );
	CVAStruct oParams;
	pMsg->ReadVAStruct( oParams );
	m_pRealCore->SetReproductionModuleParameters( sID, oParams );
	return;
}

CVAStruct CVANetNetworkProtocol::ClientGetReproductionModuleParameters( const std::string& sID, const CVAStruct& oParams )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_REPRODUCTION_GET_PARAMETERS, MESSAGE_WITH_ANSWER );
	pMsg->WriteString( sID );
	pMsg->WriteVAStruct( oParams );
	ClientSendCommand( pMsg );
	CVAStruct oRet;
	pMsg->ReadVAStruct( oRet );
	return oRet;
}

void CVANetNetworkProtocol::ServerGetReproductionModuleParameters( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	std::string sID     = pMsg->ReadString( );
	CVAStruct oParams;
	pMsg->ReadVAStruct( oParams );
	CVAStruct oRet = m_pRealCore->GetReproductionModuleParameters( sID, oParams );
	pMsg->WriteVAStruct( oRet );
	return;
}

int CVANetNetworkProtocol::ClientCreateAcousticMaterial( const CVAAcousticMaterial& oMaterial, const std::string& sName )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_CREATE_ACOUSTIC_MATERIAL, MESSAGE_WITH_ANSWER );
	pMsg->WriteAcousticMaterial( oMaterial );
	pMsg->WriteString( sName );
	ClientSendCommand( pMsg );
	return pMsg->ReadInt( );
}

void CVANetNetworkProtocol::ServerCreateAcousticMaterial( )
{
	CVANetMessage* pMsg      = ServerGetMessage( );
	CVAAcousticMaterial oMat = pMsg->ReadAcousticMaterial( );
	const std::string sName  = pMsg->ReadString( );
	const int iID            = m_pRealCore->CreateAcousticMaterial( oMat, sName );
	pMsg->WriteInt( iID );
}

int CVANetNetworkProtocol::ClientCreateAcousticMaterialFromParameters( const CVAStruct& oParams, const std::string& sName )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_CREATE_ACOUSTIC_MATERIAL_FROM_PARAMETERS, MESSAGE_WITH_ANSWER );
	pMsg->WriteVAStruct( oParams );
	pMsg->WriteString( sName );
	ClientSendCommand( pMsg );
	return pMsg->ReadInt( );
}

void CVANetNetworkProtocol::ServerCreateAcousticMaterialFromParameters( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	CVAStruct oParams;
	pMsg->ReadVAStruct( oParams );
	const std::string sName = pMsg->ReadString( );
	const int iID           = m_pRealCore->CreateAcousticMaterialFromParameters( oParams, sName );
	pMsg->WriteInt( iID );
}

bool CVANetNetworkProtocol::ClientDeleteAcousticMaterial( const int iID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_DELETE_ACOUSTIC_MATERIAL, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iID );
	ClientSendCommand( pMsg );
	return pMsg->ReadBool( );
}

void CVANetNetworkProtocol::ServerDeleteAcousticMaterial( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const int iID       = pMsg->ReadInt( );
	const bool bResult  = m_pRealCore->DeleteAcousticMaterial( iID );
	pMsg->WriteBool( bResult );
}


CVAAcousticMaterial CVANetNetworkProtocol::ClientGetAcousticMaterialInfo( const int iID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_ACOUSTIC_MATERIAL_INFO, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iID );
	ClientSendCommand( pMsg );
	return pMsg->ReadAcousticMaterial( );
}

void CVANetNetworkProtocol::ServerGetAcousticMaterialInfo( )
{
	CVANetMessage* pMsg      = ServerGetMessage( );
	const int iID            = pMsg->ReadInt( );
	CVAAcousticMaterial oMat = m_pRealCore->GetAcousticMaterialInfo( iID );
	pMsg->WriteAcousticMaterial( oMat );
}

void CVANetNetworkProtocol::ClientGetAcousticMaterialInfos( std::vector<CVAAcousticMaterial>& voInfos )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_ACOUSTIC_MATERIAL_INFOS, MESSAGE_WITH_ANSWER );
	ClientSendCommand( pMsg );
	const int iNumMaterials = pMsg->ReadInt( );
	for( int i = 0; i < iNumMaterials; i++ )
		voInfos.push_back( pMsg->ReadAcousticMaterial( ) );
}

void CVANetNetworkProtocol::ServerGetAcousticMaterialInfos( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	std::vector<CVAAcousticMaterial> voInfos;
	m_pRealCore->GetAcousticMaterialInfos( voInfos );
	pMsg->WriteInt( int( voInfos.size( ) ) );
	for( size_t i = 0; i < voInfos.size( ); i++ )
		pMsg->WriteAcousticMaterial( voInfos[i] );
}

std::string CVANetNetworkProtocol::ClientGetAcousticMaterialName( const int iID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_ACOUSTIC_MATERIAL_NAME, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iID );
	ClientSendCommand( pMsg );
	return pMsg->ReadString( );
}

void CVANetNetworkProtocol::ServerGetAcousticMaterialName( )
{
	CVANetMessage* pMsg     = ServerGetMessage( );
	const int iID           = pMsg->ReadInt( );
	const std::string sName = m_pRealCore->GetAcousticMaterialName( iID );
	pMsg->WriteString( sName );
}

void CVANetNetworkProtocol::ClientSetAcousticMaterialName( const int iID, const std::string& sName )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_ACOUSTIC_MATERIAL_NAME, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iID );
	pMsg->WriteString( sName );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetAcousticMaterialName( )
{
	CVANetMessage* pMsg     = ServerGetMessage( );
	const int iID           = pMsg->ReadInt( );
	const std::string sName = pMsg->ReadString( );
	m_pRealCore->SetAcousticMaterialName( iID, sName );
}

CVAStruct CVANetNetworkProtocol::ClientGetAcousticMaterialParameters( const int iID, const CVAStruct& oParams )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_ACOUSTIC_MATERIAL_PARAMETERS, MESSAGE_WITH_ANSWER );
	pMsg->WriteInt( iID );
	pMsg->WriteVAStruct( oParams );
	ClientSendCommand( pMsg );
	CVAStruct oReturn;
	pMsg->ReadVAStruct( oReturn );
	return oReturn;
}

void CVANetNetworkProtocol::ServerGetAcousticMaterialParameters( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const int iID       = pMsg->ReadInt( );
	CVAStruct oParams;
	pMsg->ReadVAStruct( oParams );
	const CVAStruct oReturn = m_pRealCore->GetAcousticMaterialParameters( iID, oParams );
	pMsg->WriteVAStruct( oReturn );
}

void CVANetNetworkProtocol::ClientSetAcousticMaterialParameters( const int iID, const CVAStruct& oParams )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_ACOUSTIC_MATERIAL_PARAMETERS, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteInt( iID );
	pMsg->WriteVAStruct( oParams );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetAcousticMaterialParameters( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const int iID       = pMsg->ReadInt( );
	CVAStruct oParams;
	pMsg->ReadVAStruct( oParams );
	m_pRealCore->SetAcousticMaterialParameters( iID, oParams );
}


double CVANetNetworkProtocol::ClientGetHomogeneousMediumSoundSpeed( )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_HOMOGENEOUS_MEDIUM_SOUND_SPEED, MESSAGE_WITH_ANSWER );
	ClientSendCommand( pMsg );
	return pMsg->ReadDouble( );
}

void CVANetNetworkProtocol::ServerGetHomogeneousMediumSoundSpeed( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const double dValue = m_pRealCore->GetHomogeneousMediumSoundSpeed( );
	pMsg->WriteDouble( dValue );
}

void CVANetNetworkProtocol::ClientSetHomogeneousMediumSoundSpeed( const double dSoundSpeed )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_HOMOGENEOUS_MEDIUM_SOUND_SPEED, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteDouble( dSoundSpeed );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetHomogeneousMediumSoundSpeed( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const double dValue = pMsg->ReadDouble( );
	m_pRealCore->SetHomogeneousMediumSoundSpeed( dValue );
}

double CVANetNetworkProtocol::ClientGetHomogeneousMediumTemperature( )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_HOMOGENEOUS_MEDIUM_TEMPERATURE, MESSAGE_WITH_ANSWER );
	ClientSendCommand( pMsg );
	return pMsg->ReadDouble( );
}

void CVANetNetworkProtocol::ServerGetHomogeneousMediumTemperature( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const double dValue = m_pRealCore->GetHomogeneousMediumTemperature( );
	pMsg->WriteDouble( dValue );
}

void CVANetNetworkProtocol::ClientSetHomogeneousMediumTemperature( const double dTemperature )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_HOMOGENEOUS_MEDIUM_TEMPERATURE, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteDouble( dTemperature );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetHomogeneousMediumTemperature( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const double dValue = pMsg->ReadDouble( );
	m_pRealCore->SetHomogeneousMediumTemperature( dValue );
}

double CVANetNetworkProtocol::ClientGetHomogeneousMediumStaticPressure( )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_HOMOGENEOUS_MEDIUM_STATIC_PRESSURE, MESSAGE_WITH_ANSWER );
	ClientSendCommand( pMsg );
	return pMsg->ReadDouble( );
}

void CVANetNetworkProtocol::ServerGetHomogeneousMediumStaticPressure( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const double dValue = m_pRealCore->GetHomogeneousMediumStaticPressure( );
	pMsg->WriteDouble( dValue );
}

void CVANetNetworkProtocol::ClientSetHomogeneousMediumStaticPressure( const double dStaticPressure )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_HOMOGENEOUS_MEDIUM_STATIC_PRESSURE, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteDouble( dStaticPressure );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetHomogeneousMediumStaticPressure( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const double dValue = pMsg->ReadDouble( );
	m_pRealCore->SetHomogeneousMediumStaticPressure( dValue );
}

double CVANetNetworkProtocol::ClientGetHomogeneousMediumRelativeHumidity( )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_HOMOGENEOUS_MEDIUM_RELATIVE_HUMIDITY, MESSAGE_WITH_ANSWER );
	ClientSendCommand( pMsg );
	return pMsg->ReadDouble( );
}

void CVANetNetworkProtocol::ServerGetHomogeneousMediumRelativeHumidity( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const double dValue = m_pRealCore->GetHomogeneousMediumRelativeHumidity( );
	pMsg->WriteDouble( dValue );
}

void CVANetNetworkProtocol::ClientSetHomogeneousMediumRelativeHumidity( const double dRelativeHumidity )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_HOMOGENEOUS_MEDIUM_RELATIVE_HUMIDITY, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteDouble( dRelativeHumidity );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetHomogeneousMediumRelativeHumidity( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	const double dValue = pMsg->ReadDouble( );
	m_pRealCore->SetHomogeneousMediumRelativeHumidity( dValue );
}

VAVec3 CVANetNetworkProtocol::ClientGetHomogeneousMediumShiftSpeed( )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_HOMOGENEOUS_MEDIUM_SHIFT_SPEED, MESSAGE_WITH_ANSWER );
	ClientSendCommand( pMsg );
	return pMsg->ReadVec3( );
}

void CVANetNetworkProtocol::ServerGetHomogeneousMediumShiftSpeed( )
{
	CVANetMessage* pMsg             = ServerGetMessage( );
	const VAVec3 v3TranslationSpeed = m_pRealCore->GetHomogeneousMediumShiftSpeed( );
	pMsg->WriteVec3( v3TranslationSpeed );
}

void CVANetNetworkProtocol::ClientSetHomogeneousMediumShiftSpeed( const VAVec3& v3TranslationSpeed )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_HOMOGENEOUS_MEDIUM_SHIFT_SPEED, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteVec3( v3TranslationSpeed );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetHomogeneousMediumShiftSpeed( )
{
	CVANetMessage* pMsg             = ServerGetMessage( );
	const VAVec3 v3TranslationSpeed = pMsg->ReadVec3( );
	m_pRealCore->SetHomogeneousMediumShiftSpeed( v3TranslationSpeed );
}

void CVANetNetworkProtocol::ClientSetHomogeneousMediumParameters( const CVAStruct& oParams )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_SET_HOMOGENEOUS_MEDIUM_PARAMETERS, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteVAStruct( oParams );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetHomogeneousMediumParameters( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	CVAStruct oParams;
	pMsg->ReadVAStruct( oParams );
	m_pRealCore->SetHomogeneousMediumParameters( oParams );
}

CVAStruct CVANetNetworkProtocol::ClientGetHomogeneousMediumParameters( const CVAStruct& oParams )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_GET_HOMOGENEOUS_MEDIUM_PARAMETERS, MESSAGE_WITH_ANSWER );
	pMsg->WriteVAStruct( oParams );
	ClientSendCommand( pMsg );
	CVAStruct oRet;
	pMsg->ReadVAStruct( oRet );
	return oRet;
}

void CVANetNetworkProtocol::ServerGetHomogeneousMediumParameters( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	std::string sID     = pMsg->ReadString( );
	CVAStruct oParams;
	pMsg->ReadVAStruct( oParams );
	CVAStruct oRet = m_pRealCore->GetHomogeneousMediumParameters( oParams );
	pMsg->WriteVAStruct( oRet );
	return;
}


bool CVANetNetworkProtocol::ClientGetRenderingModuleMuted( const std::string& sID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_RENDERER_GET_MUTED, MESSAGE_WITH_ANSWER );
	pMsg->WriteString( sID );
	ClientSendCommand( pMsg );
	return pMsg->ReadBool( );
}

void CVANetNetworkProtocol::ServerGetRenderingModuleMuted( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	std::string sID     = pMsg->ReadString( );
	const bool bMuted   = m_pRealCore->GetRenderingModuleMuted( sID );
	pMsg->WriteBool( bMuted );
}

void CVANetNetworkProtocol::ClientSetReproductionModuleGain( const std::string& sID, const double dGain )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_REPRODUCTION_SET_GAIN, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteString( sID );
	pMsg->WriteDouble( dGain );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetReproductionModuleGain( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	std::string sID     = pMsg->ReadString( );
	double dGain        = pMsg->ReadDouble( );
	m_pRealCore->SetReproductionModuleGain( sID, dGain );
}

void CVANetNetworkProtocol::ClientSetReproductionModuleMuted( const std::string& sID, const bool bMuted )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_REPRODUCTION_SET_MUTED, MESSAGE_ALLOWS_BUFFERING );
	pMsg->WriteString( sID );
	pMsg->WriteBool( bMuted );
	ClientSendCommand( pMsg );
}

void CVANetNetworkProtocol::ServerSetReproductionModuleMuted( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	std::string sID     = pMsg->ReadString( );
	bool bMuted         = pMsg->ReadBool( );
	m_pRealCore->SetReproductionModuleMuted( sID, bMuted );
}

double CVANetNetworkProtocol::ClientGetReproductionModuleGain( const std::string& sID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_REPRODUCTION_GET_GAIN, MESSAGE_WITH_ANSWER );
	pMsg->WriteString( sID );
	ClientSendCommand( pMsg );
	return pMsg->ReadDouble( );
}

void CVANetNetworkProtocol::ServerGetReproductionModuleGain( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	std::string sID     = pMsg->ReadString( );
	double dGain        = m_pRealCore->GetReproductionModuleGain( sID );
	pMsg->WriteDouble( dGain );
}

bool CVANetNetworkProtocol::ClientGetReproductionModuleMuted( const std::string& sID )
{
	CVANetMessage* pMsg = ClientInitMessage( VA_NP_REPRODUCTION_GET_MUTED, MESSAGE_WITH_ANSWER );
	pMsg->WriteString( sID );
	ClientSendCommand( pMsg );
	return pMsg->ReadBool( );
}

void CVANetNetworkProtocol::ServerGetReproductionModuleMuted( )
{
	CVANetMessage* pMsg = ServerGetMessage( );
	std::string sID     = pMsg->ReadString( );
	bool bMuted         = m_pRealCore->GetReproductionModuleMuted( sID );
	pMsg->WriteBool( bMuted );
}
