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

#include "VANetClientImpl.h"

#include "VANetNetworkProtocol.h"
#include "VANetVistaCompatibility.h"

#include <VA.h>

// ViSTA includes
#include <VistaBase/VistaDefaultTimerImp.h>
#include <VistaBase/VistaExceptionBase.h>
#include <VistaBase/VistaSerializingToolset.h>
#include <VistaBase/VistaTimerImp.h>
#include <VistaInterProcComm/Concurrency/VistaMutex.h>
#include <VistaInterProcComm/Concurrency/VistaThreadLoop.h>
#include <VistaInterProcComm/Connections/VistaConnectionIP.h>
#include <algorithm>
#include <cassert>


// Ensures that any possible call to the client
// is executed stricly serial. No two commands may
// be entered at the same time. This ensures that
// multiple client thread can use the client safely.
#define VA_MUTAL_EXCLUDE VistaMutexLock oLock( m_oCommandMutex )

// State checking macro
#define VA_REQUIRE_CONNECTED                             \
	{                                                    \
		if( m_pParent->IsConnected( ) == false )         \
			VA_EXCEPT2( MODAL_ERROR, "Not connected." ); \
	}


////////////////////////////////////////////////
/////// NetworkedVACore                 ////////
////////////////////////////////////////////////

class CVANetClientImpl::CNetworkedVACore : public IVAInterface
{
public:
	inline CNetworkedVACore( CVANetNetworkProtocol* pProtocol, CVANetClientImpl* pParent, VistaConnectionIP* pCommandChannel, VistaConnectionIP* pHeadChannel )
	    : IVAInterface( )
	    , m_pParent( pParent )
	    , m_pCommandChannel( pCommandChannel )
	    , m_pHeadChannel( pHeadChannel )
	    , m_pProtocol( pProtocol )
	{
		if( m_pHeadChannel )
			m_pHeadMutex = new VistaMutex;
		else
			m_pHeadMutex = &m_oCommandMutex;
	};

	inline ~CNetworkedVACore( )
	{
		if( m_pHeadChannel )
			delete m_pHeadMutex;
	};

	inline VistaMutex* GetCommandMutex( ) { return &m_oCommandMutex; };

	inline VistaMutex* GetHeadMutex( ) { return m_pHeadMutex; };

	inline void GetVersionInfo( CVAVersionInfo* pVersionInfo ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetVersionInfo( pVersionInfo );
	};

	inline void SetOutputStream( std::ostream* ) { VA_EXCEPT_NOT_IMPLEMENTED; };

	inline int GetState( ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetState( );
	};

	inline void Initialize( )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientInitialize( );
	};

	inline void Finalize( )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientFinalize( );
	};

	inline void Reset( )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientReset( );
	};

	inline void AttachEventHandler( IVAEventHandler* pEventHandler )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pParent->AttachEventHandler( pEventHandler );
	};

	inline void DetachEventHandler( IVAEventHandler* pEventHandler )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pParent->DetachEventHandler( pEventHandler );
	};

	inline CVAStruct GetSearchPaths( ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSearchPaths( );
	};

	inline std::string FindFilePath( const std::string& sFilePath ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientFindFilePath( sFilePath );
	};

	inline CVAStruct GetCoreConfiguration( const bool bFilterEnabled = true ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetCoreConfiguration( bFilterEnabled );
	};

	inline CVAStruct GetHardwareConfiguration( ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetHardwareConfiguration( );
	};

	inline CVAStruct GetFileList( const bool bRecursive = true, const std::string& sFileSuffixMask = "*" ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetFileList( bRecursive, sFileSuffixMask );
	};

	inline void GetModules( std::vector<CVAModuleInfo>& viModuleInfos ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientGetModules( viModuleInfos );
	};

	inline CVAStruct CallModule( const std::string& sModuleName, const CVAStruct& oArgs )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientCallModule( sModuleName, oArgs );
	};


	inline int CreateDirectivityFromParameters( const CVAStruct& oParams, const std::string& sName /* = "" */ )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientCreateDirectivityFromParameters( oParams, sName );
	};

	inline bool DeleteDirectivity( const int iID )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientDeleteDirectivity( iID );
	};

	inline CVADirectivityInfo GetDirectivityInfo( const int iDirID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetDirectivityInfo( iDirID );
	};

	inline void GetDirectivityInfos( std::vector<CVADirectivityInfo>& vdiDest ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientGetDirectivityInfos( vdiDest );
	};

	inline void SetDirectivityName( const int iID, const std::string& sName )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetDirectivityName( iID, sName );
	};

	inline std::string GetDirectivityName( const int iID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetDirectivityName( iID );
	};

	inline void SetDirectivityParameters( const int iID, const CVAStruct& oParams )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetDirectivityParameters( iID, oParams );
	};

	inline CVAStruct GetDirectivityParameters( const int iID, const CVAStruct& oArgs ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetDirectivityParameters( iID, oArgs );
	};


	inline std::string CreateSignalSourceBufferFromParameters( const CVAStruct& oParams, const std::string& sName = "" )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientCreateSignalSourceBufferFromParameters( oParams, sName );
	};

	inline std::string CreateSignalSourcePrototypeFromParameters( const CVAStruct& oParams, const std::string& sName = "" )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientCreateSignalSourcePrototypeFromParameters( oParams, sName );
	};

	inline std::string CreateSignalSourceTextToSpeech( const std::string& sName = "" )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientCreateTextToSpeechSignalSource( sName );
	};

	inline std::string CreateSignalSourceSequencer( const std::string& sName = "" )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientCreateSequencerSignalSource( sName );
	};

	inline std::string CreateSignalSourceNetworkStream( const std::string& sInterface, const int iPort, const std::string& sName = "" )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientCreateNetworkStreamSignalSource( sInterface, iPort, sName );
	};

	inline std::string CreateSignalSourceEngine( const CVAStruct& oParams, const std::string& sName = "" )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientCreateEngineSignalSource( oParams, sName );
	};

	inline std::string CreateSignalSourceMachine( const CVAStruct& oParams, const std::string& sName = "" )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientCreateSignalSourceMachine( oParams, sName );
	};

	inline bool DeleteSignalSource( const std::string& sID )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientDeleteSignalSource( sID );
	};

	inline std::string RegisterSignalSource( IVAAudioSignalSource*, const std::string& )
	{
		VA_EXCEPT2( NOT_IMPLEMENTED, "This function is not available when operating on a remote server" );
	};

	inline bool UnregisterSignalSource( IVAAudioSignalSource* ) { VA_EXCEPT2( NOT_IMPLEMENTED, "This function is not available when operating on a remote server" ); };

	inline CVASignalSourceInfo GetSignalSourceInfo( const std::string& sSignalSourceID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSignalSourceInfo( sSignalSourceID );
	};

	inline void GetSignalSourceInfos( std::vector<CVASignalSourceInfo>& vssiDest ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSignalSourceInfos( vssiDest );
	};

	inline int GetSignalSourceBufferPlaybackState( const std::string& sSignalSourceID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSignalSourceBufferPlaybackState( sSignalSourceID );
	};

	inline void SetSignalSourceBufferPlaybackAction( const std::string& sSignalSourceID, const int iPlayState )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetSignalSourceBufferPlaybackAction( sSignalSourceID, iPlayState );
	};

	inline void SetSignalSourceBufferPlaybackPosition( const std::string& sSignalSourceID, const double dPlaybackPosition )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetSignalSourceBufferPlaybackPosition( sSignalSourceID, dPlaybackPosition );
	};

	inline bool GetSignalSourceBufferLooping( const std::string& sSignalSourceID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSignalSourceBufferLooping( sSignalSourceID );
	};

	inline void SetSignalSourceBufferLooping( const std::string& sSignalSourceID, const bool bLooping )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetSignalSourceBufferLooping( sSignalSourceID, bLooping );
	};

	inline void SetSignalSourceParameters( const std::string& sSignalSourceID, const CVAStruct& oParams )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetSignalSourceParameters( sSignalSourceID, oParams );
	};

	inline CVAStruct GetSignalSourceParameters( const std::string& sSignalSourceID, const CVAStruct& oParams ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSignalSourceParameters( sSignalSourceID, oParams );
	};

	inline int AddSignalSourceSequencerSample( const std::string& sSignalSourceID, const CVAStruct& oArgs )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientAddSignalSourceSequencerSample( sSignalSourceID, oArgs );
	};

	inline int AddSignalSourceSequencerPlayback( const std::string& sSignalSourceID, const int iSoundID, const int iFlags, const double dTimeCode )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientAddSignalSourceSequencerPlayback( sSignalSourceID, iSoundID, iFlags, dTimeCode );
	};

	inline void RemoveSignalSourceSequencerSample( const std::string& sSignalSourceID, const int iSoundID )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientRemoveSignalSourceSequencerSample( sSignalSourceID, iSoundID );
	};


	inline bool GetUpdateLocked( ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetUpdateLocked( );
	};

	inline void LockUpdate( )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientLockUpdate( );
	};

	inline int UnlockUpdate( )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientUnlockUpdate( );
	};


	inline void GetSoundSourceIDs( std::vector<int>& vSoundSourceIDs )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSoundSourceIDs( vSoundSourceIDs );
	};

	inline int CreateSoundSource( const std::string& sName = "" )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientCreateSoundSource( sName );
	};

	inline int CreateSoundSourceExplicitRenderer( const std::string& sRendererID, const std::string& sName = "" )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientCreateSoundSourceExplicitRenderer( sRendererID, sName );
	};

	inline int DeleteSoundSource( const int iID )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientDeleteSoundSource( iID );
	};

	inline void SetSoundSourceEnabled( const int iSoundSourceID, const bool bEnabled = true )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientSetSoundSourceEnabled( iSoundSourceID, bEnabled );
	};

	inline bool GetSoundSourceEnabled( const int iSoundSourceID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSoundSourceEnabled( iSoundSourceID );
	};

	inline CVASoundSourceInfo GetSoundSourceInfo( const int iSoundSourceID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSoundSourceInfo( iSoundSourceID );
	};

	inline std::string GetSoundSourceName( const int iSoundSourceID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSoundSourceName( iSoundSourceID );
	};

	inline void SetSoundSourceName( const int iSoundSourceID, const std::string& sName )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetSoundSourceName( iSoundSourceID, sName );
	};

	inline std::string GetSoundSourceSignalSource( const int iSoundSourceID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSoundSourceSignalSource( iSoundSourceID );
	};

	inline void SetSoundSourceSignalSource( const int iSoundSourceID, const std::string& sSignalSourceID )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetSoundSourceSignalSource( iSoundSourceID, sSignalSourceID );
	};

	inline int GetSoundSourceAuralizationMode( const int iSoundSourceID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSoundSourceAuralizationMode( iSoundSourceID );
	};

	inline void SetSoundSourceAuralizationMode( const int iSoundSourceID, const int iAuralizationMode )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetSoundSourceAuralizationMode( iSoundSourceID, iAuralizationMode );
	};

	inline CVAStruct GetSoundSourceParameters( const int iID, const CVAStruct& oArgs ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSoundSourceParameters( iID, oArgs );
	};

	inline void SetSoundSourceParameters( const int iID, const CVAStruct& oParams )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetSoundSourceParameters( iID, oParams );
	};

	inline int GetSoundSourceDirectivity( const int iSoundSourceID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSoundSourceDirectivity( iSoundSourceID );
	};

	inline void SetSoundSourceDirectivity( const int iSoundSourceID, const int iDirectivityID )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientSetSoundSourceDirectivity( iSoundSourceID, iDirectivityID );
	};

	inline double GetSoundSourceSoundPower( const int iSoundSourceID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSoundSourceSoundPower( iSoundSourceID );
	};

	inline void SetSoundSourceSoundPower( const int iSoundSourceID, const double dPower )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetSoundSourceSoundPower( iSoundSourceID, dPower );
	};

	inline bool GetSoundSourceMuted( const int iSoundSourceID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSoundSourceMuted( iSoundSourceID );
	};

	inline void SetSoundSourceMuted( const int iSoundSourceID, const bool bMuted = true )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetSoundSourceMuted( iSoundSourceID, bMuted );
	};

	inline void GetSoundSourcePose( const int iID, VAVec3& v3Pos, VAQuat& qOrient ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientGetSoundSourcePose( iID, v3Pos, qOrient );
	};

	inline void SetSoundSourcePose( const int iID, const VAVec3& v3Pos, const VAQuat& qOrient )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetSoundSourcePose( iID, v3Pos, qOrient );
	};

	inline void SetSoundSourcePosition( const int iID, const VAVec3& v3Pos )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetSoundSourcePosition( iID, v3Pos );
	};

	inline void SetSoundSourceOrientation( const int iID, const VAQuat& qOrient )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetSoundSourceOrientation( iID, qOrient );
	};

	inline void SetSoundSourceOrientationVU( const int iID, const VAVec3& v3View, const VAVec3& v3Up )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetSoundSourceOrientationVU( iID, v3View, v3Up );
	};

	inline VAVec3 GetSoundSourcePosition( const int iID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSoundSourcePosition( iID );
	};

	inline VAQuat GetSoundSourceOrientation( const int iSoundSourceID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSoundSourceOrientation( iSoundSourceID );
	};

	inline void GetSoundSourceOrientationVU( const int iSoundSourceID, VAVec3& v3View, VAVec3& v3Up ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientGetSoundSourceOrientationVU( iSoundSourceID, v3View, v3Up );
	};


	inline int GetSoundSourceGeometryMesh( const int iID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSoundSourceGeometryMesh( iID );
	};

	inline void SetSoundSourceGeometryMesh( const int iSoundSourceID, const int iGeometryMeshID )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetSoundSourceGeometryMesh( iSoundSourceID, iGeometryMeshID );
	};


	inline void GetSoundReceiverIDs( std::vector<int>& vSoundReceiverIDs ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientGetSoundReceiverIDs( vSoundReceiverIDs );
	};

	inline int CreateSoundReceiver( const std::string& sName = "" )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientCreateSoundReceiver( sName );
	};

	inline int CreateSoundReceiverExplicitRenderer( const std::string& sRendererID, const std::string& sName )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientCreateSoundReceiverExplicitRenderer( sRendererID, sName );
	};

	inline int DeleteSoundReceiver( const int iSoundReceiverID )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientDeleteSoundReceiver( iSoundReceiverID );
	};

	inline CVASoundReceiverInfo GetSoundReceiverInfo( const int iID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSoundReceiverInfo( iID );
	};

	inline void SetSoundReceiverEnabled( const int iSoundReceiverID, const bool bEnabled = true )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientSetSoundReceiverEnabled( iSoundReceiverID, bEnabled );
	};

	inline bool GetSoundReceiverEnabled( const int iSoundReceiverID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSoundReceiverEnabled( iSoundReceiverID );
	};

	inline std::string GetSoundReceiverName( const int iSoundReceiverID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSoundReceiverName( iSoundReceiverID );
	};

	inline void SetSoundReceiverName( const int iSoundReceiverID, const std::string& sName )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetSoundReceiverName( iSoundReceiverID, sName );
	};

	inline int GetSoundReceiverAuralizationMode( const int iSoundReceiverID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSoundReceiverAuralizationMode( iSoundReceiverID );
	}

	inline void SetSoundReceiverAuralizationMode( const int iSoundReceiverID, const int iAuralizationMode )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetSoundReceiverAuralizationMode( iSoundReceiverID, iAuralizationMode );
	};

	inline CVAStruct GetSoundReceiverParameters( const int iID, const CVAStruct& oArgs ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSoundReceiverParameters( iID, oArgs );
	};

	inline void SetSoundReceiverParameters( const int iID, const CVAStruct& oParams )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetSoundReceiverParameters( iID, oParams );
	};

	inline int GetSoundReceiverDirectivity( const int iSoundReceiverID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSoundReceiverDirectivity( iSoundReceiverID );
	};

	inline void SetSoundReceiverDirectivity( const int iSoundReceiverID, const int iDirectivityID )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetSoundReceiverDirectivity( iSoundReceiverID, iDirectivityID );
	};

	inline bool GetSoundReceiverMuted( const int iID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSoundReceiverMuted( iID );
	};

	inline void SetSoundReceiverMuted( const int iID, const bool bMuted = true )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetSoundReceiverMuted( iID, bMuted );
	};

	inline void GetSoundReceiverPose( const int iID, VAVec3& v3Pos, VAQuat& qOrient ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientGetSoundReceiverPose( iID, v3Pos, qOrient );
	};

	inline void SetSoundReceiverPose( const int iID, const VAVec3& v3Pos, const VAQuat& qOrient )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetSoundReceiverPose( iID, v3Pos, qOrient );
	};

	inline VAVec3 GetSoundReceiverPosition( const int iID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSoundReceiverPosition( iID );
	};

	inline void SetSoundReceiverPosition( const int iID, const VAVec3& v3Pos )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetSoundReceiverPosition( iID, v3Pos );
	};

	inline VAQuat GetSoundReceiverOrientation( const int iSoundReceiverID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSoundReceiverOrientation( iSoundReceiverID );
	};

	inline void SetSoundReceiverOrientation( const int iID, const VAQuat& qOrient )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetSoundReceiverOrientation( iID, qOrient );
	};


	inline void GetSoundReceiverOrientationVU( const int iSoundReceiverID, VAVec3& v3View, VAVec3& v3Up ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientGetSoundReceiverOrientationVU( iSoundReceiverID, v3View, v3Up );
	};

	inline void SetSoundReceiverOrientationVU( const int iID, const VAVec3& v3View, const VAVec3& v3Up )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetSoundReceiverOrientationVU( iID, v3View, v3Up );
	};

	inline VAQuat GetSoundReceiverHeadAboveTorsoOrientation( const int iID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSoundReceiverHeadAboveTorsoOrientation( iID );
	};

	inline void SetSoundReceiverHeadAboveTorsoOrientation( const int iID, const VAQuat& qOrient )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetSoundReceiverHeadAboveTorsoOrientation( iID, qOrient );
	};

	inline int GetSoundReceiverGeometryMesh( const int iID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSoundReceiverGeometryMesh( iID );
	};

	inline void SetSoundReceiverGeometryMesh( const int iSoundReceiverID, const int iGeometryMeshID )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetSoundReceiverGeometryMesh( iSoundReceiverID, iGeometryMeshID );
	};

	inline void GetSoundReceiverRealWorldPose( const int iID, VAVec3& v3Pos, VAQuat& qOrient ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientGetSoundReceiverRealWorldPose( iID, v3Pos, qOrient );
	};

	inline void SetSoundReceiverRealWorldPose( const int iID, const VAVec3& v3Pos, const VAQuat& qOrient )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetSoundReceiverRealWorldPose( iID, v3Pos, qOrient );
	};

	inline VAQuat GetSoundReceiverRealWorldHeadAboveTorsoOrientation( const int iID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSoundReceiverRealWorldTorsoOrientation( iID );
	};

	inline void SetSoundReceiverRealWorldHeadAboveTorsoOrientation( const int iID, const VAQuat& qOrient )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetSoundReceiverRealWorldTorsoOrientation( iID, qOrient );
	};

	inline void GetSoundReceiverRealWorldPositionOrientationVU( const int iID, VAVec3& v3Pos, VAVec3& v3View, VAVec3& v3Up ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientGetSoundReceiverRealWorldPositionOrientationVU( iID, v3Pos, v3View, v3Up );
	};

	inline void SetSoundReceiverRealWorldPositionOrientationVU( const int iID, const VAVec3& v3Pos, const VAVec3& v3View, const VAVec3& v3Up )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetSoundReceiverRealWorldPositionOrientationVU( iID, v3Pos, v3View, v3Up );
	};


	inline void SetHomogeneousMediumSoundSpeed( const double dSoundSpeed )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetHomogeneousMediumSoundSpeed( dSoundSpeed );
	};

	inline double GetHomogeneousMediumSoundSpeed( ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetHomogeneousMediumSoundSpeed( );
	};

	inline void SetHomogeneousMediumTemperature( const double dDegreesCentigrade )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetHomogeneousMediumTemperature( dDegreesCentigrade );
	};

	inline double GetHomogeneousMediumTemperature( ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetHomogeneousMediumTemperature( );
	};

	inline void SetHomogeneousMediumStaticPressure( const double dPressurePascal )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetHomogeneousMediumStaticPressure( dPressurePascal );
	};

	inline double GetHomogeneousMediumStaticPressure( ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetHomogeneousMediumStaticPressure( );
	};

	inline void SetHomogeneousMediumRelativeHumidity( const double dRelativeHumidityPercent )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetHomogeneousMediumRelativeHumidity( dRelativeHumidityPercent );
	};

	inline double GetHomogeneousMediumRelativeHumidity( )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetHomogeneousMediumRelativeHumidity( );
	};

	inline void SetHomogeneousMediumShiftSpeed( const VAVec3& v3TranslationSpeed )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetHomogeneousMediumShiftSpeed( v3TranslationSpeed );
	};

	inline VAVec3 GetHomogeneousMediumShiftSpeed( ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetHomogeneousMediumShiftSpeed( );
	};

	inline void SetHomogeneousMediumParameters( const CVAStruct& oParams )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetHomogeneousMediumParameters( oParams );
	};

	inline CVAStruct GetHomogeneousMediumParameters( const CVAStruct& oArgs )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetHomogeneousMediumParameters( oArgs );
	};


	inline std::string CreateScene( const CVAStruct& oParams, const std::string& sName = "" )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientCreateScene( oParams, sName );
	};

	inline void GetSceneIDs( std::vector<std::string>& vsIDs ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientGetSceneIDs( vsIDs );
	};

	inline void SetSceneName( const std::string& sID, const std::string& sName )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetSceneName( sID, sName );
	};

	inline std::string GetSceneName( const std::string& sID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSceneName( sID );
	};

	inline CVASceneInfo GetSceneInfo( const std::string& sID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSceneInfo( sID );
	};

	inline void SetSceneEnabled( const std::string& sID, const bool bEnabled = true )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientSetSceneEnabled( sID, bEnabled );
	};

	inline bool GetSceneEnabled( const std::string& sID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSceneEnabled( sID );
	};

	inline int CreateSoundPortal( const std::string& sName = "" )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientCreateSoundPortal( sName );
	};

	inline void GetSoundPortalIDs( std::vector<int>& viIDs )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientGetSoundPortalIDs( viIDs );
	};

	inline std::string GetSoundPortalName( const int iID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSoundPortalName( iID );
	};

	inline void SetSoundPortalName( const int iPortalID, const std::string& sName )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetSoundPortalName( iPortalID, sName );
	};

	inline CVAStruct GetSoundPortalParameters( const int iPortalID, const CVAStruct& oArgs ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSoundPortalParameters( iPortalID, oArgs );
	};

	inline void SetSoundPortalParameters( const int iPortalID, const CVAStruct& oParams )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetSoundPortalParameters( iPortalID, oParams );
	};

	inline CVASoundPortalInfo GetSoundPortalInfo( const int iID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSoundPortalInfo( iID );
	};

	inline void SetSoundPortalMaterial( const int iSoundPortalID, const int iMaterialID )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetSoundPortalMaterial( iSoundPortalID, iMaterialID );
	};

	inline int GetSoundPortalMaterial( const int iSoundPortalID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSoundPortalMaterial( iSoundPortalID );
	};

	inline void SetSoundPortalNextPortal( const int iSoundPortalID, const int iNextSoundPortalID )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetSoundPortalNextPortal( iSoundPortalID, iNextSoundPortalID );
	};

	inline int GetSoundPortalNextPortal( const int iSoundPortalID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSoundPortalNextPortal( iSoundPortalID );
	};

	inline void SetSoundPortalSoundReceiver( const int iSoundPortalID, const int iSoundReceiverID )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetSoundPortalSoundReceiver( iSoundPortalID, iSoundReceiverID );
	};

	inline int GetSoundPortalSoundReceiver( const int iSoundPortalID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSoundPortalSoundReceiver( iSoundPortalID );
	};

	inline void SetSoundPortalSoundSource( const int iSoundPortalID, const int iSoundSourceID )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetSoundPortalSoundSource( iSoundPortalID, iSoundSourceID );
	};

	inline int GetSoundPortalSoundSource( const int iSoundPortalID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSoundPortalSoundSource( iSoundPortalID );
	};

	inline void SetSoundPortalPosition( const int iSoundPortalID, const VAVec3& vPos )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetSoundPortalPosition( iSoundPortalID, vPos );
	};

	inline VAVec3 GetSoundPortalPosition( const int iSoundPortalID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSoundPortalPosition( iSoundPortalID );
	};

	inline void SetSoundPortalOrientation( const int iSoundPortalID, const VAQuat& qOrient )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetSoundPortalOrientation( iSoundPortalID, qOrient );
	};

	inline VAQuat GetSoundPortalOrientation( const int iSoundPortalID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSoundPortalOrientation( iSoundPortalID );
	};

	inline void SetSoundPortalEnabled( const int iSoundPortalID, const bool bEnabled = true )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetSoundPortalEnabled( iSoundPortalID, bEnabled );
	};

	inline bool GetSoundPortalEnabled( const int iSoundPortalID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetSoundPortalEnabled( iSoundPortalID );
	};


	inline bool GetInputMuted( ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetInputMuted( );
	};

	inline void SetInputMuted( const bool bMuted = true )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetInputMuted( bMuted );
	}

	inline double GetInputGain( ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetInputGain( );
	}

	inline void SetInputGain( const double dGain )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetInputGain( dGain );
	}

	inline double GetOutputGain( ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetOutputGain( );
	}

	inline void SetOutputGain( const double dGain )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetOutputGain( dGain );
	};

	inline bool GetOutputMuted( ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetOutputMuted( );
	};

	inline void SetOutputMuted( const bool bMuted = true )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetOutputMuted( bMuted );
	};

	inline int GetGlobalAuralizationMode( ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetGlobalAuralizationMode( );
	}

	inline void SetGlobalAuralizationMode( const int iAuralizationMode )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetGlobalAuralizationMode( iAuralizationMode );
	}

	inline double GetCoreClock( ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetCoreClock( );
	};

	inline void SetCoreClock( const double dSeconds )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetCoreClock( dSeconds );
	};

	inline std::string SubstituteMacros( const std::string& sStr ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientSubstituteMacros( sStr );
	};


	inline void GetRenderingModules( std::vector<CVAAudioRendererInfo>& voRenderer, const bool bFilterEnabled = true ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientGetRenderingModuleInfos( voRenderer, bFilterEnabled );
	};

	inline void SetRenderingModuleMuted( const std::string& sModuleID, const bool bMuted = true )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetRenderingModuleMuted( sModuleID, bMuted );
	};

	inline bool GetRenderingModuleMuted( const std::string& sModuleID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetRenderingModuleMuted( sModuleID );
	};

	inline void SetRenderingModuleAuralizationMode( const std::string& sModuleID, const int iAuraMode )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetRenderingModuleAuralizationMode( sModuleID, iAuraMode );
	};

	inline int GetRenderingModuleAuralizationMode( const std::string& sModuleID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetRenderingModuleAuralizationMode( sModuleID );
	};

	inline void SetRenderingModuleParameters( const std::string& sID, const CVAStruct& oParams )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetRenderingModuleParameters( sID, oParams );
	};

	inline CVAStruct GetRenderingModuleParameters( const std::string& sID, const CVAStruct& oParams ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetRenderingModuleParameters( sID, oParams );
	};

	inline void SetRenderingModuleGain( const std::string& sModuleID, const double dGain )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetRenderingModuleGain( sModuleID, dGain );
	};

	inline double GetRenderingModuleGain( const std::string& sModuleID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetRenderingModuleGain( sModuleID );
	};

	inline void GetReproductionModules( std::vector<CVAAudioReproductionInfo>& voRepros, const bool bFilterEnabled = true ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientGetReproductionModuleInfos( voRepros, bFilterEnabled );
	}

	inline void SetReproductionModuleMuted( const std::string& sModuleID, const bool bMuted = true )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetReproductionModuleMuted( sModuleID, bMuted );
	};

	inline bool GetReproductionModuleMuted( const std::string& sModuleID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetReproductionModuleMuted( sModuleID );
	};

	inline void SetReproductionModuleGain( const std::string& sModuleID, const double dGain )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetReproductionModuleGain( sModuleID, dGain );
	};

	inline double GetReproductionModuleGain( const std::string& sModuleID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetReproductionModuleGain( sModuleID );
	};

	inline void SetReproductionModuleParameters( const std::string& sID, const CVAStruct& oParams )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetReproductionModuleParameters( sID, oParams );
	};

	inline CVAStruct GetReproductionModuleParameters( const std::string& sID, const CVAStruct& oParams ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetReproductionModuleParameters( sID, oParams );
	};


	inline int CreateAcousticMaterial( const CVAAcousticMaterial& oMaterial, const std::string& sName = "" )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientCreateAcousticMaterial( oMaterial, sName );
	};

	inline int CreateAcousticMaterialFromParameters( const CVAStruct& oParams, const std::string& sName = "" )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientCreateAcousticMaterialFromParameters( oParams, sName );
	};

	inline bool DeleteAcousticMaterial( const int iID )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientDeleteAcousticMaterial( iID );
	};

	inline CVAAcousticMaterial GetAcousticMaterialInfo( const int iID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetAcousticMaterialInfo( iID );
	};

	inline void GetAcousticMaterialInfos( std::vector<CVAAcousticMaterial>& voDest ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetAcousticMaterialInfos( voDest );
	};

	inline void SetAcousticMaterialName( const int iID, const std::string& sName )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientSetAcousticMaterialName( iID, sName );
	};

	inline std::string GetAcousticMaterialName( const int iID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetAcousticMaterialName( iID );
	};

	inline CVAStruct GetAcousticMaterialParameters( const int iID, const CVAStruct& oParams ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetAcousticMaterialParameters( iID, oParams );
	};

	inline void SetAcousticMaterialParameters( const int iID, const CVAStruct& oParams )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetAcousticMaterialParameters( iID, oParams );
	};


	inline int CreateGeometryMesh( const CVAGeometryMesh& oMesh, const std::string& sName = "" )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientCreateGeometryMesh( oMesh, sName );
	};

	inline int CreateGeometryMeshFromParameters( const CVAStruct& oParams, const std::string& sName = "" )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientCreateGeometryMeshFromParameters( oParams, sName );
	};

	inline bool DeleteGeometryMesh( const int iID )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientDeleteGeometryMesh( iID );
	};

	inline CVAGeometryMesh GetGeometryMesh( const int iID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetGeometryMesh( iID );
	};

	inline void GetGeometryMeshIDs( std::vector<int>& viIDs ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientGetGeometryMeshIDs( viIDs );
	};

	inline void SetGeometryMeshEnabled( const int iID, const bool bEnabled = true )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetGeometryMeshEnabled( iID, bEnabled );
	};

	inline bool GetGeometryMeshEnabled( const int iID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetGeometryMeshEnabled( iID );
	};

	inline void SetGeometryMeshName( const int iID, const std::string& sName )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetGeometryMeshName( iID, sName );
	};

	inline std::string GetGeometryMeshName( const int iID ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetGeometryMeshName( iID );
	};

	inline CVAStruct GetGeometryMeshParameters( const int iID, const CVAStruct& oParams ) const
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		return m_pProtocol->ClientGetGeometryMeshParameters( iID, oParams );
	};

	inline void SetGeometryMeshParameters( const int iID, const CVAStruct& oParams )
	{
		VA_REQUIRE_CONNECTED;
		VA_MUTAL_EXCLUDE;
		m_pProtocol->ClientSetGeometryMeshParameters( iID, oParams );
	};


protected:
	CVANetClientImpl* m_pParent;
	VistaConnectionIP* m_pCommandChannel;
	VistaConnectionIP* m_pHeadChannel;
	CVANetNetworkProtocol* m_pProtocol;

	mutable VistaMutex m_oCommandMutex;
	VistaMutex* m_pHeadMutex;
};


class CVANetClientImpl::CEventReceiver : public VistaThreadLoop
{
public:
	inline CEventReceiver( CVANetClientImpl* pParent, CVANetNetworkProtocol* pProtocol, VistaConnectionIP* pConnection, VistaMutex* pEventChannelLock )
	    : VistaThreadLoop( )
	    , m_pParent( pParent )
	    , m_pProtocol( pProtocol )
	    , m_pEventChannel( pConnection )
	    , m_pEventChannelLock( pEventChannelLock )
	{
		SetThreadName( "VA-Server Connection Observer" );
	}

	inline void AttachCoreEventHandler( IVAEventHandler* pCoreEventHandler )
	{
		VistaMutexLock oLock( m_oHandlerListLock );
		std::list<IVAEventHandler*>::iterator itEntry = std::find( m_liCoreEventHandlers.begin( ), m_liCoreEventHandlers.end( ), pCoreEventHandler );
		if( itEntry != m_liCoreEventHandlers.end( ) )
			return;

		m_liCoreEventHandlers.push_back( pCoreEventHandler );
	};

	inline void DetachCoreEventHandler( IVAEventHandler* pCoreEventHandler )
	{
		VistaMutexLock oLock( m_oHandlerListLock );
		std::list<IVAEventHandler*>::iterator itEntry = std::find( m_liCoreEventHandlers.begin( ), m_liCoreEventHandlers.end( ), pCoreEventHandler );
		if( itEntry == m_liCoreEventHandlers.end( ) )
			return;

		m_liCoreEventHandlers.erase( itEntry );
	};

	inline bool LoopBody( )
	{
		try
		{
			// to allow external stopping of this thread, we only block for at most
			// 100ms
			if( m_pEventChannel->WaitForIncomingData( 100 ) == ~0 )
				return false;

			VistaMutexLock oListLock( m_oHandlerListLock );
			VistaMutexLock oChannelLock( *m_pEventChannelLock );
			if( m_pProtocol->ClientProcessEventMessageFromServer( m_pEventChannel, m_liCoreEventHandlers ) == CVANetNetworkProtocol::VA_EVENT_FAIL )
			{
				oListLock.Unlock( );
				oChannelLock.Unlock( );
				IndicateLoopEnd( );
				m_pParent->ProcessEventChannelError( );
			}
		}
#ifdef VANET_CLIENT_VERBOSE
		catch( VistaExceptionBase& oException )
		{
			std::cout << "VANetClient: EventReceiver encountered connection error - " << oException.GetExceptionText( ) << std::endl;
#else
		catch( VistaExceptionBase& )
		{
#endif
			IndicateLoopEnd( );
			m_pParent->ProcessEventChannelError( );
		}
#ifdef VANET_CLIENT_VERBOSE
		catch( CVAException& oException )
		{
			std::cout << "VANetClient: EventReceiver encountered connection error - " << oException.ToString( ) << std::endl;
#else
		catch( CVAException& )
		{
#endif
			IndicateLoopEnd( );
			m_pParent->ProcessEventChannelError( );
		}
		return false;
	};

	inline void PreLoop( )
	{
#ifdef VANET_CLIENT_VERBOSE
		std::cout << "VANetClient: Starting Event Observer" << std::endl;
#endif
	};

	inline void PostRun( )
	{
#ifdef VANET_CLIENT_VERBOSE
		std::cout << "VANetClient: Stopping Event Observer" << std::endl;
#endif
	};

private:
	CVANetClientImpl* m_pParent;
	VistaConnectionIP* m_pEventChannel;
	CVANetNetworkProtocol* m_pProtocol;
	std::list<IVAEventHandler*> m_liCoreEventHandlers;
	VistaMutex m_oHandlerListLock;
	VistaMutex* m_pEventChannelLock;
};


CVANetClientImpl::CVANetClientImpl( )
    : m_pCommandChannel( NULL )
    , m_pEventChannel( NULL )
    , m_pHeadChannel( NULL )
    , m_pVACore( NULL )
    , m_pProtocol( NULL )
    , m_pEventReceiver( NULL )
    , m_bConnected( false )
    , m_pEventChannelLock( new VistaMutex )
    , m_bShutdownFlag( false )
{
}

CVANetClientImpl::~CVANetClientImpl( )
{
	Disconnect( );
	Cleanup( );
}

IVAInterface* CVANetClientImpl::GetCoreInstance( ) const
{
	return m_pVACore;
}

bool CVANetClientImpl::IsConnected( ) const
{
	if( m_bShutdownFlag == true )
	{
		const_cast<CVANetClientImpl*>( this )->Disconnect( );
		return false;
	}
	return ( m_bConnected && m_pCommandChannel && m_pCommandChannel->GetIsConnected( ) );
}

std::string CVANetClientImpl::GetServerAddress( ) const
{
	return m_sServerIP;
}

int CVANetClientImpl::Initialize( const std::string& sServerAddress, const int iServerPort, const int iHeadChannelMode, const int IExceptionHandlingMode,
                                  const bool bBufferSynchronizedCommands )
{
	if( IVistaTimerImp::GetSingleton( ) == NULL )
		IVistaTimerImp::SetSingleton( new VistaDefaultTimerImp );

	m_pProtocol = new CVANetNetworkProtocol;

	m_sServerIP = sServerAddress;

	VistaConnectionIP oRequestConnection( VistaConnectionIP::CT_TCP, m_sServerIP, iServerPort );
	if( oRequestConnection.GetIsConnected( ) == false )
	{
#if VANET_CLIENT_VERBOSE == 1
		std::cout << "VANetClient: Connection to server at [" << m_sServerIP << ":" << iServerPort << "] failed" << std::endl;
#endif
		return VA_SERVER_CONNECTION_FAILED;
	}
	oRequestConnection.SetIsBlocking( true );


	// first, we read the first sent one to check if we have different endianess
	oRequestConnection.SetByteorderSwapFlag( VistaSerializingToolset::DOES_NOT_SWAP_MULTIBYTE_VALUES );
	VistaSerializingToolset::ByteOrderSwapBehavior bSwap = VistaSerializingToolset::DOES_NOT_SWAP_MULTIBYTE_VALUES;
	VANetCompat::sint32 iTest;
	oRequestConnection.ReadInt32( iTest );
	if( iTest != 1 )
	{
		bSwap = VistaSerializingToolset::SWAPS_MULTIBYTE_VALUES;
		oRequestConnection.SetByteorderSwapFlag( VistaSerializingToolset::SWAPS_MULTIBYTE_VALUES );
	}

	// Verify protocol version;
	VANetCompat::sint32 nMajor = -1;
	VANetCompat::sint32 nMinor = -1;
	oRequestConnection.ReadInt32( nMajor );
	oRequestConnection.ReadInt32( nMinor );
	if( nMajor != CVANetNetworkProtocol::VA_NET_PROTOCOL_VERSION_MAJOR || nMinor != CVANetNetworkProtocol::VA_NET_PROTOCOL_VERSION_MINOR )
	{
		oRequestConnection.WriteBool( false );
		oRequestConnection.WaitForSendFinish( );
		Cleanup( );
		return VA_PROTOCOL_INCOMPATIBLE;
	}
	else
		oRequestConnection.WriteBool( true );

	VANetCompat::sint32 iCommandChannelPort = 0;
	int iReturn                             = 0;
	iReturn                                 = oRequestConnection.ReadInt32( iCommandChannelPort );
	if( iReturn != sizeof( VANetCompat::sint32 ) )
	{
		Cleanup( );
		return VA_CONNECTION_ERROR;
	}
	if( iCommandChannelPort == -1 )
	{
		// server responded, but establishing the connection sockets failed
		Cleanup( );
		return VA_CONNECTION_ERROR;
	}
	else if( iCommandChannelPort == -2 )
	{
		// server responded, but it doesn't accept any more connections
		Cleanup( );
		return VA_SERVICE_IN_USE;
	}
	m_pCommandChannel = new VistaConnectionIP( VistaConnectionIP::CT_TCP, m_sServerIP, iCommandChannelPort );
#ifdef VANET_CLIENT_SHOW_RAW_TRAFFIC
	m_pCommandChannel->SetShowRawSendAndReceive( true );
#else
	m_pCommandChannel->SetShowRawSendAndReceive( false );
#endif

	if( m_pCommandChannel->GetIsConnected( ) == false )
	{
#ifdef VANET_CLIENT_VERBOSE
		std::cout << "VANetClient: Connecting command channel at [" << m_sServerIP << ":" << iCommandChannelPort << "] failed" << std::endl;
#endif
		Cleanup( );
		return VA_SERVER_CONNECTION_FAILED;
	}

	switch( iHeadChannelMode )
	{
		case VA_HC_USE_EXISTING:
		{
			oRequestConnection.WriteInt32( 0 );
			m_pHeadChannel = NULL;
			break;
		}
		case VA_HC_SEPARATE_TCP:
		{
			oRequestConnection.WriteInt32( 1 );
			VANetCompat::sint32 iHeadPort = 0;
			oRequestConnection.ReadInt32( iHeadPort );
			m_pHeadChannel = new VistaConnectionIP( VistaConnectionIP::CT_TCP, m_sServerIP, iHeadPort );
			if( m_pHeadChannel->GetIsConnected( ) == false )
			{
				Cleanup( );
				return VA_CONNECTION_ERROR;
			}
			break;
		}
		case VA_HC_SEPARATE_UDP:
		{
			oRequestConnection.WriteInt32( 2 );
			VANetCompat::sint32 iHeadPort = 0;
			oRequestConnection.ReadInt32( iHeadPort );
			m_pHeadChannel = new VistaConnectionIP( VistaConnectionIP::CT_UDP, m_sServerIP, iHeadPort );
			if( m_pHeadChannel->GetIsConnected( ) == false )
			{
				Cleanup( );
				return VA_CONNECTION_ERROR;
			}
			break;
		}
		default:
		{
			Cleanup( );
			return VA_UNKNOWN_ERROR;
		}
	}

	VANetCompat::sint32 iEventPort;
	int nRet = oRequestConnection.ReadInt32( iEventPort );
	if( nRet != sizeof( VANetCompat::sint32 ) )
	{
		std::cerr << "VANetClient: Could not establish event connection - did not receive port!" << std::endl;
		Cleanup( );
		return VA_CONNECTION_ERROR;
	}
	if( iEventPort == -1 )
	{
		std::cerr << "VANetClient: Could not establish event connection - server does not allow more connections!" << std::endl;
		Cleanup( );
		return false;
	}

	m_pEventChannel = new VistaConnectionIP( VistaConnectionIP::CT_TCP, m_sServerIP, iEventPort );
	if( m_pEventChannel->GetIsConnected( ) == false )
	{
		std::cerr << "VANetClient: Could not establish event connection - connect failed!" << std::endl;
		Cleanup( );
		return false;
	}

	m_pCommandChannel->SetByteorderSwapFlag( bSwap );
	m_pCommandChannel->SetIsBlocking( true );
	if( m_pHeadChannel )
	{
		m_pHeadChannel->SetByteorderSwapFlag( bSwap );
		m_pHeadChannel->SetIsBlocking( true );
	}
	m_pEventChannel->SetByteorderSwapFlag( m_pCommandChannel->GetByteorderSwapFlag( ) );
	m_pEventChannel->SetIsBlocking( true );

	m_pVACore = new CNetworkedVACore( m_pProtocol, this, m_pCommandChannel, m_pHeadChannel );

	m_pEventReceiver = new CEventReceiver( this, m_pProtocol, m_pEventChannel, m_pEventChannelLock );
	m_pEventReceiver->Run( );

	oRequestConnection.Close( );

	m_bConnected = true;

	m_pProtocol->InitializeAsClient( this, m_pCommandChannel, m_pHeadChannel, IExceptionHandlingMode, bBufferSynchronizedCommands );
	return VA_NO_ERROR;
}

int CVANetClientImpl::Disconnect( )
{
	if( !m_bConnected || !m_pCommandChannel || !m_pCommandChannel->GetIsConnected( ) )
		return VA_NO_ERROR;

	SendConnectionEvent( CVANetNetworkProtocol::VA_NET_CLIENT_CLOSE );

	VistaMutexLock( *m_pVACore->GetCommandMutex( ) );
	// @TODO: lock head mutex

	if( m_pCommandChannel )
	{
		m_pCommandChannel->Close( );
		delete m_pCommandChannel;
		m_pCommandChannel = NULL;
	}

	if( m_pHeadChannel )
	{
		m_pHeadChannel->Close( );
		delete m_pHeadChannel;
		m_pHeadChannel = NULL;
	}

	m_pEventReceiver->StopGently( true );

	if( m_pEventChannel )
	{
		m_pEventChannel->Close( );
		delete m_pEventChannel;
		m_pEventChannel = NULL;
	}

	m_bConnected    = false;
	m_bShutdownFlag = false;
	return VA_NO_ERROR;
}

void CVANetClientImpl::SendConnectionEvent( const int nId )
{
	try
	{
		m_pCommandChannel->WriteInt32( ( VANetCompat::sint32 )( 4 * sizeof( VANetCompat::sint32 ) ) );
		m_pCommandChannel->WriteInt32( CVANetNetworkProtocol::VA_NP_SPECIAL_CLIENT_EVENT );
		m_pCommandChannel->WriteInt32( 0 );
		m_pCommandChannel->WriteInt32( 0 );
		m_pCommandChannel->WriteInt32( nId );
	}
	catch( VistaExceptionBase& )
	{
		// it's okay here...
	}
}

void CVANetClientImpl::SetExceptionHandlingMode( int nMode )
{
	m_pProtocol->SetExceptionHandlingMode( nMode );
}
int CVANetClientImpl::GetExceptionhandlingMode( ) const
{
	return m_pProtocol->GetExceptionHandlingMode( );
}

void CVANetClientImpl::Cleanup( )
{
	if( m_pCommandChannel )
	{
		m_pCommandChannel->Close( );
		delete m_pCommandChannel;
		m_pCommandChannel = NULL;
	}
	if( m_pHeadChannel )
	{
		m_pHeadChannel->Close( );
		delete m_pHeadChannel;
		m_pHeadChannel = NULL;
	}
	if( m_pEventChannel )
	{
		m_pEventChannel->Close( );
		delete m_pEventChannel;
		m_pEventChannel = NULL;
	}

	if( m_pEventReceiver )
	{
		m_pEventReceiver->StopGently( true );
		m_pEventReceiver->Join( );
		delete m_pEventReceiver;
		m_pEventReceiver = NULL;
	}

	delete m_pVACore;
	delete m_pProtocol;
	delete m_pEventChannelLock;
}

void CVANetClientImpl::ProcessEventChannelError( )
{
	std::cerr << "VANetClient: Lost connection to server" << std::endl;
	EmitEvent( CEvent::EVENT_SERVER_LOST );
	try
	{
		VistaMutexLock( *m_pVACore->GetCommandMutex( ) );
		if( m_pCommandChannel && m_pCommandChannel->GetIsConnected( ) )
			m_pCommandChannel->WriteInt32( CVANetNetworkProtocol::VA_NET_CLIENT_CLOSE );
	}
	catch( VistaExceptionBase& )
	{
		// its okay for this call to fail - the connection may already be closed
	}
	m_bShutdownFlag = true;
}

void CVANetClientImpl::ProcessNetEvent( const int iEventID )
{
	switch( iEventID )
	{
		case CVANetNetworkProtocol::VA_NET_SERVER_CLOSE:
		{
#if VANET_CLIENT_VERBOSE == 1
			std::cout << "VANetClient: Server closed - shutting down" << std::endl;
#endif
			EmitEvent( CEvent::EVENT_SERVER_DISCONNECT );
			m_bShutdownFlag = true;
			break;
		}
		case CVANetNetworkProtocol::VA_NET_SERVER_DISCONNECT:
		{
#if VANET_CLIENT_VERBOSE == 1
			std::cout << "VANetClient: Server disconnecter - shutting down" << std::endl;
#endif
			EmitEvent( CEvent::EVENT_SERVER_DISCONNECT );
			m_bShutdownFlag = true;
			break;
		}
		case CVANetNetworkProtocol::VA_NET_SERVER_RESET:
		{
#if VANET_CLIENT_VERBOSE == 1
			std::cout << "VANetClient: Server resetting - shutting down" << std::endl;
#endif
			EmitEvent( CEvent::EVENT_SERVER_DISCONNECT );
			m_bShutdownFlag = true;
			break;
		}
		default:
		{
#if VANET_CLIENT_VERBOSE == 1
			std::cout << "VANetClient: Received Unknown NET event " << iEventID << std::endl;
#endif
			break;
		}
	}
}

void CVANetClientImpl::EmitEvent( const IVANetClient::CEvent& oEvent )
{
	if( m_liEventHandlers.empty( ) )
		return;
	VistaMutexLock oLock( *m_pEventChannelLock );
	for( std::list<IEventHandler*>::iterator itHandler = m_liEventHandlers.begin( ); itHandler != m_liEventHandlers.end( ); ++itHandler )
	{
		( *itHandler )->HandleEvent( oEvent );
	}
}

void CVANetClientImpl::EmitEvent( const int iID )
{
	IVANetClient::CEvent oEvent( iID, m_sServerIP );
	EmitEvent( oEvent );
}

bool CVANetClientImpl::AttachEventHandler( IVANetClient::IEventHandler* pHandler )
{
	VistaMutexLock oLock( *m_pEventChannelLock );
	std::list<IEventHandler*>::iterator itEntry = std::find( m_liEventHandlers.begin( ), m_liEventHandlers.end( ), pHandler );
	if( itEntry != m_liEventHandlers.end( ) )
		return false;
	m_liEventHandlers.push_back( pHandler );
	return true;
}

bool CVANetClientImpl::DetachEventHandler( IVANetClient::IEventHandler* pHandler )
{
	VistaMutexLock oLock( *m_pEventChannelLock );
	std::list<IEventHandler*>::iterator itEntry = std::find( m_liEventHandlers.begin( ), m_liEventHandlers.end( ), pHandler );
	if( itEntry == m_liEventHandlers.end( ) )
		return false;
	m_liEventHandlers.erase( itEntry );
	return true;
}


void CVANetClientImpl::AttachEventHandler( IVAEventHandler* pCoreEventHandler )
{
	if( IsConnected( ) == false )
		VA_EXCEPT2( MODAL_ERROR, "Not connected." );
	assert( m_pEventReceiver != NULL );
	m_pEventReceiver->AttachCoreEventHandler( pCoreEventHandler );
	m_pProtocol->ClientAttachEventHandler( );
}

void CVANetClientImpl::DetachEventHandler( IVAEventHandler* pCoreEventHandler )
{
	if( IsConnected( ) == false )
		VA_EXCEPT2( MODAL_ERROR, "Not connected." );
	assert( m_pEventReceiver != NULL );
	m_pEventReceiver->DetachCoreEventHandler( pCoreEventHandler );
	m_pProtocol->ClientDetachEventHandler( );
}
