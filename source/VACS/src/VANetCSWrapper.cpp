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

#include <VA.h>
#include <VANet.h>
#include <VANetCSWrapper.h>
#include <iostream>
#include <string>

// our way of using try-catch macros generates a lot of these warnings, but we are aware as an exception is raised anyway.
#pragma warning( disable : 4715 )

// @todo jst: it would be way better to delegate C++ exceptions to C#. Unfortunately, C# crashes if discovering a VA exception ... so we print to cout and return undone
// until this issue is fixed.
#define VA_CS_TRY \
	{             \
		try
#define VA_CS_CATCH_AND_PRINT_ERROR                                                 \
	catch( CVAException e ) { std::cout << "[VANetCSWrapper] " << e << std::endl; } \
	}

#define VA_CS_TRY_2 \
	try             \
	{
#define VA_CS_CATCH_AND_PRINT_ERROR_2 \
	}                                 \
	catch( const CVAException& e ) { std::cout << "[VANetCSWrapper] " << e << std::endl; }

#define VA_CS_REQUIRE_CONNECTED                    \
	if( !pClient || !pClient->GetCoreInstance( ) ) \
		throw CVAException( CVAException::NETWORK_ERROR, "Not connected to a VA server" );


//! Wrapper class around VA net client for C# binding
/**
 * Create a c-style API for a native C# integration by passing the wrapper class pointer as first arguments to the functions.
 * Not so nice, but unmanaged C++ classes can not be used in managed C#.
 */
class CUnmanagedVANetClient
{
public:
	inline CUnmanagedVANetClient( ) { m_pVANetClient = IVANetClient::Create( ); };

	virtual ~CUnmanagedVANetClient( )
	{
		delete m_pVANetClient;
		m_pVANetClient = NULL;
	};

	inline bool Connect( )
	{
		if( m_pVANetClient->IsConnected( ) )
			m_pVANetClient->Disconnect( );

		bool bSuccess = ( m_pVANetClient->Initialize( "localhost" ) == IVANetClient::VA_NO_ERROR );
		return bSuccess;
	};

	inline bool Connect( const std::string& sServerIP )
	{
		if( m_pVANetClient->IsConnected( ) )
			m_pVANetClient->Disconnect( );

		bool bSuccess = ( m_pVANetClient->Initialize( sServerIP ) == IVANetClient::VA_NO_ERROR );
		return bSuccess;
	};

	inline bool Connect( const std::string& sServerIP, int iPort )
	{
		if( m_pVANetClient->IsConnected( ) )
			m_pVANetClient->Disconnect( );

		bool bSuccess = ( m_pVANetClient->Initialize( sServerIP, iPort ) == IVANetClient::VA_NO_ERROR );
		return bSuccess;
	};

	inline bool IsConnected( ) { return m_pVANetClient->IsConnected( ); };

	inline bool Disconnect( ) { return ( m_pVANetClient->Disconnect( ) == IVANetClient::VA_NO_ERROR ); };

	inline IVAInterface* GetCoreInstance( )
	{
		if( !m_pVANetClient )
			return nullptr;

		if( !m_pVANetClient->IsConnected( ) )
			return nullptr;

		return m_pVANetClient->GetCoreInstance( );
	};

private:
	IVANetClient* m_pVANetClient;
};


CUnmanagedVANetClient* NativeCreateNetClient( )
{
	return new CUnmanagedVANetClient( );
}

void NativeDisposeNetClient( CUnmanagedVANetClient* pClient )
{
	delete pClient;
}

bool NativeConnectNetClient( CUnmanagedVANetClient* pClient, const char* pcServerIP, int iPort )
{
	if( !pClient )
		return false;

	return pClient->Connect( std::string( pcServerIP ), iPort );
}

bool NativeConnectLocalNetClient( CUnmanagedVANetClient* pClient )
{
	if( !pClient )
		return false;

	return pClient->Connect( );
}

bool NativeDisconnectNetClient( CUnmanagedVANetClient* pClient )
{
	if( !pClient )
		return false;

	return pClient->Disconnect( );
}

bool NativeGetNetClientConnected( CUnmanagedVANetClient* pClient )
{
	if( !pClient )
		return false;

	return pClient->IsConnected( );
}

bool NativeAddSearchPath( CUnmanagedVANetClient* pClient, const char* cpSearchPath )
{
	if( !pClient )
		return false;
	return pClient->GetCoreInstance( )->AddSearchPath( cpSearchPath );
}


// Little helper
void CopyFromString( const std::string& s, char* pc )
{
	// Check if buffer size is exceeded and crop, if necessary
	size_t iBufferLength = s.length( ) + 1;
	if( iBufferLength > 10 * 256 )
	{
		std::string sCroppedString( s );
		sCroppedString.resize( 10 * 256 );
		std::strcpy( pc, sCroppedString.c_str( ) );
	}
	else
	{
		std::strcpy( pc, s.c_str( ) );
	}
}


// C-style API with pointer to wrapper class as first argument

void NativeGetVersionInfo( CUnmanagedVANetClient* pClient, CVAVersionInfo* pVersionInfo )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;
	pClient->GetCoreInstance( )->GetVersionInfo( pVersionInfo );
	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

int NativeGetState( CUnmanagedVANetClient* pClient )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;
	return pClient->GetCoreInstance( )->GetState( );
	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeInitialize( CUnmanagedVANetClient* pClient )
{
	pClient->GetCoreInstance( )->Initialize( );
}

void NativeFinalize( CUnmanagedVANetClient* pClient )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;
	pClient->GetCoreInstance( )->Finalize( );
	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeReset( CUnmanagedVANetClient* pClient )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;
	pClient->GetCoreInstance( )->Reset( );
	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeAttachCoreEventHandler( CUnmanagedVANetClient* pClient, IVAEventHandler* pCoreEventHandler )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;
	pClient->GetCoreInstance( )->AttachEventHandler( pCoreEventHandler );
	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeDetachCoreEventHandler( CUnmanagedVANetClient* pClient, IVAEventHandler* pCoreEventHandler )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;
	pClient->GetCoreInstance( )->DetachEventHandler( pCoreEventHandler );
	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeGetModules( CUnmanagedVANetClient* pClient, std::vector<CVAModuleInfo>& viModuleInfos )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;
	pClient->GetCoreInstance( )->GetModules( viModuleInfos );
	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeCallModule( CUnmanagedVANetClient* pClient, const char* pcModuleName, const CVAStruct* oArgs, CVAStruct* oReturn )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;
	*oReturn = pClient->GetCoreInstance( )->CallModule( std::string( pcModuleName ), *oArgs );
	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

int NativeCreateDirectivityFromFile( CUnmanagedVANetClient* pClient, const char* pcFilePath, const char* pcName )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;
	return pClient->GetCoreInstance( )->CreateDirectivityFromFile( std::string( pcFilePath ), std::string( pcName ) );
	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

bool NativeDeleteDirectivity( CUnmanagedVANetClient* pClient, int iDirID )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;
	return pClient->GetCoreInstance( )->DeleteDirectivity( iDirID );
	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeGetDirectivityInfos( CUnmanagedVANetClient* pClient, std::vector<CVADirectivityInfo>& vdiDest )
{
	VA_CS_TRY
	{
		VA_CS_REQUIRE_CONNECTED;
		pClient->GetCoreInstance( )->GetDirectivityInfos( vdiDest );
	}
	VA_CS_CATCH_AND_PRINT_ERROR
}

int NativeCreateSignalSourceBufferFromFile( CUnmanagedVANetClient* pClient, const char* pcFilePath, const char* pcName, char* pcIdentifier )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;
	std::string sIdentifier = pClient->GetCoreInstance( )->CreateSignalSourceBufferFromFile( std::string( pcFilePath ), std::string( pcName ) );
	CopyFromString( sIdentifier, pcIdentifier );
	return int( sIdentifier.length( ) );
	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeCreateSignalSourceSequencer( CUnmanagedVANetClient* pClient, const char* pcName, char* pcIdentifier )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;
	std::string sIdentifier = pClient->GetCoreInstance( )->CreateSignalSourceSequencer( std::string( pcName ) );
	CopyFromString( sIdentifier, pcIdentifier );
	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeCreateSignalSourceNetworkStream( CUnmanagedVANetClient* pClient, const char* pcInterface, int iPort, const char* pcName, char* pcIdentifier )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;
	std::string sIdentifier = pClient->GetCoreInstance( )->CreateSignalSourceNetworkStream( std::string( pcInterface ), iPort, std::string( pcName ) );
	CopyFromString( sIdentifier, pcIdentifier );
	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeCreateSignalSourceTextToSpeech( CUnmanagedVANetClient* pClient, const char* pcName, char* pcIdentifier )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	std::string sIdentifier = pClient->GetCoreInstance( )->CreateSignalSourceTextToSpeech( std::string( pcName ) );
	CopyFromString( sIdentifier, pcIdentifier );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

bool NativeDeleteSignalSource( CUnmanagedVANetClient* pClient, const char* pcID )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;
	return pClient->GetCoreInstance( )->DeleteSignalSource( std::string( pcID ) );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
	return false;
}

void NativeGetSignalSourceInfo( CUnmanagedVANetClient* pClient, const char* pcSignalSourceID, CVASignalSourceInfo& oInfo )
{
	VA_CS_TRY
	{
		VA_CS_REQUIRE_CONNECTED;
		oInfo = pClient->GetCoreInstance( )->GetSignalSourceInfo( std::string( pcSignalSourceID ) );
	}
	VA_CS_CATCH_AND_PRINT_ERROR
}

void NativeGetSignalSourceInfos( CUnmanagedVANetClient* pClient, std::vector<CVASignalSourceInfo>& vssiDest )
{
	VA_CS_TRY
	{
		VA_CS_REQUIRE_CONNECTED;
		pClient->GetCoreInstance( )->GetSignalSourceInfos( vssiDest );
	}
	VA_CS_CATCH_AND_PRINT_ERROR
}

void NativeGetSignalSourceBufferPlaybackState( CUnmanagedVANetClient* pClient, const char* pcSignalSourceID, char* pcPlaybackState )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	int iPlaybackState         = pClient->GetCoreInstance( )->GetSignalSourceBufferPlaybackState( std::string( pcSignalSourceID ) );
	std::string sPlaybackState = pClient->GetCoreInstance( )->GetPlaybackStateStr( iPlaybackState );
	CopyFromString( sPlaybackState, pcPlaybackState );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetSignalSourceBufferPlaybackAction( CUnmanagedVANetClient* pClient, const char* pcSignalSourceID, const char* pcPlaybackAction )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	int iPlaybackAction = pClient->GetCoreInstance( )->ParsePlaybackAction( std::string( pcPlaybackAction ) );
	pClient->GetCoreInstance( )->SetSignalSourceBufferPlaybackAction( std::string( pcSignalSourceID ), iPlaybackAction );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetSignalSourceBufferPlaybackPosition( CUnmanagedVANetClient* pClient, const char* pcSignalSourceID, double dPlaybackPosition )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	pClient->GetCoreInstance( )->SetSignalSourceBufferPlaybackPosition( std::string( pcSignalSourceID ), dPlaybackPosition );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

bool NativeGetSignalSourceBufferLooping( CUnmanagedVANetClient* pClient, const char* pcSignalSourceID )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	return pClient->GetCoreInstance( )->GetSignalSourceBufferLooping( std::string( pcSignalSourceID ) );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetSignalSourceBufferLooping( CUnmanagedVANetClient* pClient, const char* pcSignalSourceID, bool bLooping )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	pClient->GetCoreInstance( )->SetSignalSourceBufferLooping( std::string( pcSignalSourceID ), bLooping );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetSignalSourceParameters( CUnmanagedVANetClient* pClient, const char* pcSignalSourceID, CVAStruct* oParams )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	pClient->GetCoreInstance( )->SetSignalSourceParameters( std::string( pcSignalSourceID ), *oParams );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeGetSignalSourceParameters( CUnmanagedVANetClient* pClient, const char* pcSignalSourceID, const CVAStruct* oParams, CVAStruct* oResult )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	*oResult = pClient->GetCoreInstance( )->GetSignalSourceParameters( std::string( pcSignalSourceID ), *oParams );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

bool NativeGetUpdateLocked( CUnmanagedVANetClient* pClient )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	return pClient->GetCoreInstance( )->GetUpdateLocked( );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeLockUpdate( CUnmanagedVANetClient* pClient )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	pClient->GetCoreInstance( )->LockUpdate( );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

int NativeUnlockUpdate( CUnmanagedVANetClient* pClient )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	return pClient->GetCoreInstance( )->UnlockUpdate( );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeGetSoundSourceIDs( CUnmanagedVANetClient* pClient, std::vector<int>& vSoundSourceIDs )
{
	VA_CS_TRY
	{
		VA_CS_REQUIRE_CONNECTED;
		return pClient->GetCoreInstance( )->GetSoundSourceIDs( vSoundSourceIDs );
	}
	VA_CS_CATCH_AND_PRINT_ERROR
}

int NativeCreateSoundSource( CUnmanagedVANetClient* pClient, const char* pcName )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	return pClient->GetCoreInstance( )->CreateSoundSource( std::string( pcName ) );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

int NativeCreateSoundSourceExplicitRenderer( CUnmanagedVANetClient* pClient, const char* pcName, const char* pcRendererID )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	return pClient->GetCoreInstance( )->CreateSoundSourceExplicitRenderer( std::string( pcRendererID ), std::string( pcName ) );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

int NativeDeleteSoundSource( CUnmanagedVANetClient* pClient, int iSoundSourceID )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	return pClient->GetCoreInstance( )->DeleteSoundSource( iSoundSourceID );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetSoundSourceEnabled( CUnmanagedVANetClient* pClient, int iSoundSourceID, bool bEnabled )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	pClient->GetCoreInstance( )->SetSoundSourceEnabled( iSoundSourceID, bEnabled );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

bool NativeGetSoundSourceEnabled( CUnmanagedVANetClient* pClient, int iSoundSourceID )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	return pClient->GetCoreInstance( )->GetSoundSourceEnabled( iSoundSourceID );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeGetSoundSourceName( CUnmanagedVANetClient* pClient, int iSoundSourceID, char* pcName )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	std::string sName = pClient->GetCoreInstance( )->GetSoundSourceName( iSoundSourceID );
	CopyFromString( sName, pcName );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetSoundSourceName( CUnmanagedVANetClient* pClient, int iSoundSourceID, const char* pcName )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	pClient->GetCoreInstance( )->SetSoundSourceName( iSoundSourceID, std::string( pcName ) );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeGetSoundSourceSignalSource( CUnmanagedVANetClient* pClient, int iSoundSourceID, char* pcIdentifier )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	std::string sIdentifier = pClient->GetCoreInstance( )->GetSoundSourceSignalSource( iSoundSourceID );
	CopyFromString( sIdentifier, pcIdentifier );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetSoundSourceSignalSource( CUnmanagedVANetClient* pClient, int iSoundSourceID, const char* pcSignalSourceID )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	pClient->GetCoreInstance( )->SetSoundSourceSignalSource( iSoundSourceID, std::string( pcSignalSourceID ) );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

int NativeGetSoundSourceAuralizationMode( CUnmanagedVANetClient* pClient, int iSoundSourceID )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	return pClient->GetCoreInstance( )->GetSoundSourceAuralizationMode( iSoundSourceID );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetSoundSourceAuralizationMode( CUnmanagedVANetClient* pClient, int iSoundSourceID, const char* pcAuralizationMode )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	int iCurrentAM        = pClient->GetCoreInstance( )->GetSoundSourceAuralizationMode( iSoundSourceID );
	int iAuralizationMode = IVAInterface::ParseAuralizationModeStr( std::string( pcAuralizationMode ), iCurrentAM );
	pClient->GetCoreInstance( )->SetSoundSourceAuralizationMode( iSoundSourceID, iAuralizationMode );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeGetSoundSourceParameters( IVANetClient* pClient, int iID, const CVAStruct* oArgs, CVAStruct* oResult )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	*oResult = pClient->GetCoreInstance( )->GetSoundSourceParameters( iID, *oArgs );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetSoundSourceParameters( CUnmanagedVANetClient* pClient, int iID, CVAStruct* oParams )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	pClient->GetCoreInstance( )->SetSoundSourceParameters( iID, *oParams );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

int NativeGetSoundSourceDirectivity( CUnmanagedVANetClient* pClient, int iSoundSourceID )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	return pClient->GetCoreInstance( )->GetSoundSourceDirectivity( iSoundSourceID );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetSoundSourceDirectivity( CUnmanagedVANetClient* pClient, int iSoundSourceID, int iDirectivityID )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	pClient->GetCoreInstance( )->SetSoundSourceDirectivity( iSoundSourceID, iDirectivityID );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

double NativeGetSoundSourceSoundPower( CUnmanagedVANetClient* pClient, int iSoundSourceID )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	return pClient->GetCoreInstance( )->GetSoundSourceSoundPower( iSoundSourceID );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetSoundSourceSoundPower( CUnmanagedVANetClient* pClient, int iSoundSourceID, double dPower )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	pClient->GetCoreInstance( )->SetSoundSourceSoundPower( iSoundSourceID, dPower );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

bool NativeGetSoundSourceMuted( CUnmanagedVANetClient* pClient, int iSoundSourceID )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	return pClient->GetCoreInstance( )->GetSoundSourceMuted( iSoundSourceID );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetSoundSourceMuted( CUnmanagedVANetClient* pClient, int iSoundSourceID, bool bMuted )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	pClient->GetCoreInstance( )->SetSoundSourceMuted( iSoundSourceID, bMuted );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeGetSoundSourcePose( CUnmanagedVANetClient* pClient, int iSoundSourceID, double& x, double& y, double& z, double& ox, double& oy, double& oz, double& ow )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	VAVec3 v3Pos;
	VAQuat qOrient;
	pClient->GetCoreInstance( )->GetSoundSourcePose( iSoundSourceID, v3Pos, qOrient );
	x  = v3Pos.x;
	y  = v3Pos.z;
	z  = v3Pos.x;
	ox = qOrient.x;
	oy = qOrient.z;
	oz = qOrient.x;
	ow = qOrient.w;

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetSoundSourcePose( CUnmanagedVANetClient* pClient, int iSoundSourceID, double x, double y, double z, double ox, double oy, double oz, double ow )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	pClient->GetCoreInstance( )->SetSoundSourcePose( iSoundSourceID, VAVec3( x, y, z ), VAQuat( ox, oy, oz, ow ) );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeGetSoundSourceOrientation( CUnmanagedVANetClient* pClient, int iSoundSourceID, double& ox, double& oy, double& oz, double& ow )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	VAQuat qOrient = pClient->GetCoreInstance( )->GetSoundSourceOrientation( iSoundSourceID );
	ox             = qOrient.x;
	oy             = qOrient.z;
	oz             = qOrient.x;
	ow             = qOrient.w;

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetSoundSourceOrientation( CUnmanagedVANetClient* pClient, int iSoundSourceID, double ox, double oy, double oz, double ow )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	pClient->GetCoreInstance( )->SetSoundSourceOrientation( iSoundSourceID, VAQuat( ox, oy, oz, ow ) );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeGetSoundSourcePosition( CUnmanagedVANetClient* pClient, int iSoundSourceID, double& x, double& y, double& z )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	VAVec3 v3Pos = pClient->GetCoreInstance( )->GetSoundSourcePosition( iSoundSourceID );
	x            = v3Pos.x;
	y            = v3Pos.z;
	z            = v3Pos.x;

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetSoundSourcePosition( CUnmanagedVANetClient* pClient, int iSoundSourceID, double x, double y, double z )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	pClient->GetCoreInstance( )->SetSoundSourcePosition( iSoundSourceID, VAVec3( x, y, z ) );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeGetSoundSourceOrientationVU( CUnmanagedVANetClient* pClient, int iSoundSourceID, double& vx, double& vy, double& vz, double& ux, double& uy, double& uz )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	VAVec3 v, u;
	pClient->GetCoreInstance( )->GetSoundSourceOrientationVU( iSoundSourceID, v, u );
	vx = v.x;
	vy = v.y;
	vz = v.z;
	ux = u.x;
	uy = u.y;
	uz = u.z;

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetSoundSourceOrientationVU( CUnmanagedVANetClient* pClient, int iSoundSourceID, double vx, double vy, double vz, double ux, double uy, double uz )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	pClient->GetCoreInstance( )->SetSoundSourceOrientationVU( iSoundSourceID, VAVec3( vx, vy, vz ), VAVec3( ux, uy, uz ) );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

int NativeCreateSoundReceiver( CUnmanagedVANetClient* pClient, const char* pcName )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	return pClient->GetCoreInstance( )->CreateSoundReceiver( std::string( pcName ) );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

int NativeCreateSoundReceiverExplicitRenderer( CUnmanagedVANetClient* pClient, const char* pcName, const char* pcRendererID )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	return pClient->GetCoreInstance( )->CreateSoundReceiverExplicitRenderer( std::string( pcRendererID ), std::string( pcName ) );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

int NativeDeleteSoundReceiver( CUnmanagedVANetClient* pClient, int iID )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	return pClient->GetCoreInstance( )->DeleteSoundReceiver( iID );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetSoundReceiverEnabled( CUnmanagedVANetClient* pClient, int iID, bool bEnabled )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	pClient->GetCoreInstance( )->SetSoundReceiverEnabled( iID, bEnabled );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

bool NativeGetSoundReceiverEnabled( CUnmanagedVANetClient* pClient, int iID )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	return pClient->GetCoreInstance( )->GetSoundReceiverEnabled( iID );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeGetSoundReceiverName( CUnmanagedVANetClient* pClient, int iID, char* pcName )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	std::string sName = pClient->GetCoreInstance( )->GetSoundReceiverName( iID );
	CopyFromString( sName, pcName );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetSoundReceiverName( CUnmanagedVANetClient* pClient, int iID, const char* pcName )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	pClient->GetCoreInstance( )->SetSoundReceiverName( iID, std::string( pcName ) );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

int NativeGetSoundReceiverAuralizationMode( CUnmanagedVANetClient* pClient, int iID )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	return pClient->GetCoreInstance( )->GetSoundReceiverAuralizationMode( iID );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetSoundReceiverAuralizationMode( CUnmanagedVANetClient* pClient, int iID, const char* pcAuralizationMode )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	int iCurrentAM        = pClient->GetCoreInstance( )->GetSoundReceiverAuralizationMode( iID );
	int iAuralizationMode = IVAInterface::ParseAuralizationModeStr( std::string( pcAuralizationMode ), iCurrentAM );
	pClient->GetCoreInstance( )->SetSoundReceiverAuralizationMode( iID, iAuralizationMode );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeGetSoundReceiverParameters( IVANetClient* pClient, int iID, const CVAStruct* oArgs, CVAStruct* oResult )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	*oResult = pClient->GetCoreInstance( )->GetSoundReceiverParameters( iID, *oArgs );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetSoundReceiverParameters( CUnmanagedVANetClient* pClient, int iID, CVAStruct* oParams )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	pClient->GetCoreInstance( )->SetSoundReceiverParameters( iID, *oParams );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

int NativeGetSoundReceiverDirectivity( CUnmanagedVANetClient* pClient, int iID )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	return pClient->GetCoreInstance( )->GetSoundReceiverDirectivity( iID );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetSoundReceiverDirectivity( CUnmanagedVANetClient* pClient, int iID, int iDirectivityID )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	pClient->GetCoreInstance( )->SetSoundReceiverDirectivity( iID, iDirectivityID );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

bool NativeGetSoundReceiverMuted( CUnmanagedVANetClient* pClient, int iID )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	return pClient->GetCoreInstance( )->GetSoundReceiverMuted( iID );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetSoundReceiverMuted( CUnmanagedVANetClient* pClient, int iID, bool bMuted )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	pClient->GetCoreInstance( )->SetSoundReceiverMuted( iID, bMuted );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeGetSoundReceiverPose( CUnmanagedVANetClient* pClient, int iID, double& x, double& y, double& z, double& ox, double& oy, double& oz, double& ow )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	VAVec3 v3Pos;
	VAQuat qOrient;
	pClient->GetCoreInstance( )->GetSoundReceiverPose( iID, v3Pos, qOrient );
	x  = v3Pos.x;
	y  = v3Pos.z;
	z  = v3Pos.x;
	ox = qOrient.x;
	oy = qOrient.z;
	oz = qOrient.x;
	ow = qOrient.w;

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetSoundReceiverPose( CUnmanagedVANetClient* pClient, int iID, double x, double y, double z, double ox, double oy, double oz, double ow )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	pClient->GetCoreInstance( )->SetSoundReceiverPose( iID, VAVec3( x, y, z ), VAQuat( ox, oy, oz, ow ) );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeGetSoundReceiverOrientation( CUnmanagedVANetClient* pClient, int iID, double& ox, double& oy, double& oz, double& ow )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	VAQuat qOrient = pClient->GetCoreInstance( )->GetSoundReceiverOrientation( iID );
	ox             = qOrient.x;
	oy             = qOrient.z;
	oz             = qOrient.x;
	ow             = qOrient.w;

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetSoundReceiverOrientation( CUnmanagedVANetClient* pClient, int iID, double ox, double oy, double oz, double ow )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	pClient->GetCoreInstance( )->SetSoundReceiverOrientation( iID, VAQuat( ox, oy, oz, ow ) );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeGetSoundReceiverPosition( CUnmanagedVANetClient* pClient, int iID, double& x, double& y, double& z )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	VAVec3 v3Pos = pClient->GetCoreInstance( )->GetSoundReceiverPosition( iID );
	x            = v3Pos.x;
	y            = v3Pos.z;
	z            = v3Pos.x;

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetSoundReceiverPosition( CUnmanagedVANetClient* pClient, int iID, double x, double y, double z )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	pClient->GetCoreInstance( )->SetSoundReceiverPosition( iID, VAVec3( x, y, z ) );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeGetSoundReceiverOrientationVU( CUnmanagedVANetClient* pClient, int iID, double& vx, double& vy, double& vz, double& ux, double& uy, double& uz )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	VAVec3 v, u;
	pClient->GetCoreInstance( )->GetSoundReceiverOrientationVU( iID, v, u );
	vx = v.x;
	vy = v.y;
	vz = v.z;
	ux = u.x;
	uy = u.y;
	uz = u.z;

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetSoundReceiverOrientationVU( CUnmanagedVANetClient* pClient, int iID, double vx, double vy, double vz, double ux, double uy, double uz )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	pClient->GetCoreInstance( )->SetSoundReceiverOrientationVU( iID, VAVec3( vx, vy, vz ), VAVec3( ux, uy, uz ) );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}


void NativeGetSoundReceiverRealWorldPose( CUnmanagedVANetClient* pClient, int iSoundSourceID, double& x, double& y, double& z, double& ox, double& oy, double& oz,
                                          double& ow )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	VAVec3 v3Pos;
	VAQuat qOrient;
	pClient->GetCoreInstance( )->GetSoundReceiverRealWorldPose( iSoundSourceID, v3Pos, qOrient );
	x  = v3Pos.x;
	y  = v3Pos.z;
	z  = v3Pos.x;
	ox = qOrient.x;
	oy = qOrient.z;
	oz = qOrient.x;
	ow = qOrient.w;

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetSoundReceiverRealWorldPose( CUnmanagedVANetClient* pClient, int iSoundSourceID, double x, double y, double z, double ox, double oy, double oz, double ow )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	pClient->GetCoreInstance( )->SetSoundReceiverRealWorldPose( iSoundSourceID, VAVec3( x, y, z ), VAQuat( ox, oy, oz, ow ) );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeGetSoundReceiverHeadAboveTorsoOrientation( CUnmanagedVANetClient* pClient, int iSoundSourceID, double& ox, double& oy, double& oz, double& ow )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	VAQuat qOrient = pClient->GetCoreInstance( )->GetSoundReceiverHeadAboveTorsoOrientation( iSoundSourceID );
	ox             = qOrient.x;
	oy             = qOrient.z;
	oz             = qOrient.x;
	ow             = qOrient.w;

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetSoundReceiverHeadAboveTorsoOrientation( CUnmanagedVANetClient* pClient, int iSoundSourceID, double ox, double oy, double oz, double ow )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	pClient->GetCoreInstance( )->SetSoundReceiverHeadAboveTorsoOrientation( iSoundSourceID, VAQuat( ox, oy, oz, ow ) );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetSoundReceiverRealWorldHeadAboveTorsoOrientation( CUnmanagedVANetClient* pClient, int iSoundSourceID, double ox, double oy, double oz, double ow )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	pClient->GetCoreInstance( )->SetSoundReceiverRealWorldHeadAboveTorsoOrientation( iSoundSourceID, VAQuat( ox, oy, oz, ow ) );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetHomogeneousMediumSoundSpeed( CUnmanagedVANetClient* pClient, const double dSoundSpeed )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;
	pClient->GetCoreInstance( )->SetHomogeneousMediumSoundSpeed( dSoundSpeed );
	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

double NativeGetHomogeneousMediumSoundSpeed( CUnmanagedVANetClient* pClient )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;
	return pClient->GetCoreInstance( )->GetHomogeneousMediumSoundSpeed( );
	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetHomogeneousMediumTemperature( CUnmanagedVANetClient* pClient, const double dDegreesCentigrade )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;
	pClient->GetCoreInstance( )->SetHomogeneousMediumTemperature( dDegreesCentigrade );
	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

double NativeGetHomogeneousMediumTemperature( CUnmanagedVANetClient* pClient )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;
	return pClient->GetCoreInstance( )->GetHomogeneousMediumTemperature( );
	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetHomogeneousMediumStaticPressure( CUnmanagedVANetClient* pClient, const double dPressurePascal )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;
	pClient->GetCoreInstance( )->SetHomogeneousMediumStaticPressure( dPressurePascal );
	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

double NativeGetHomogeneousMediumStaticPressure( CUnmanagedVANetClient* pClient )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;
	return pClient->GetCoreInstance( )->GetHomogeneousMediumStaticPressure( );
	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetHomogeneousMediumRelativeHumidity( CUnmanagedVANetClient* pClient, const double delativeHumidity )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;
	pClient->GetCoreInstance( )->SetHomogeneousMediumRelativeHumidity( delativeHumidity );
	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

double NativeGetHomogeneousMediumRelativeHumidity( CUnmanagedVANetClient* pClient )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;
	return pClient->GetCoreInstance( )->GetHomogeneousMediumRelativeHumidity( );
	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetHomogeneousMediumShiftSpeed( CUnmanagedVANetClient* pClient, const double x, const double y, const double z )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;
	pClient->GetCoreInstance( )->SetHomogeneousMediumShiftSpeed( VAVec3( x, y, z ) );
	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeGetHomogeneousMediumShiftSpeed( CUnmanagedVANetClient* pClient, double& x, double& y, double& z )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;
	VAVec3 v3ShiftSpeed = pClient->GetCoreInstance( )->GetHomogeneousMediumShiftSpeed( );
	x                   = v3ShiftSpeed.x;
	y                   = v3ShiftSpeed.y;
	z                   = v3ShiftSpeed.z;
	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeGetSoundReceiverRealWorldHeadAboveTorsoOrientation( CUnmanagedVANetClient* pClient, int iSoundSourceID, double& x, double& y, double& z, double& w )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	VAQuat q = pClient->GetCoreInstance( )->GetSoundReceiverRealWorldHeadAboveTorsoOrientation( iSoundSourceID );
	x        = q.x;
	y        = q.y;
	z        = q.z;
	w        = q.x;

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeGetSoundReceiverRealWorldPositionOrientationVU( CUnmanagedVANetClient* pClient, int iSoundSourceID, double& px, double& py, double& pz, double& vx, double& vy,
                                                           double& vz, double& ux, double& uy, double& uz )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	VAVec3 p, v, u;
	pClient->GetCoreInstance( )->GetSoundReceiverRealWorldPositionOrientationVU( iSoundSourceID, p, v, u );
	px = p.x;
	py = p.y;
	pz = p.z;
	vx = v.x;
	vy = v.y;
	vz = v.z;
	ux = u.x;
	uy = u.y;
	uz = u.z;

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}


void NativeSetSoundReceiverRealWorldPositionOrientationVU( CUnmanagedVANetClient* pClient, int iSoundSourceID, double px, double py, double pz, double vx, double vy,
                                                           double vz, double ux, double uy, double uz )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;
	pClient->GetCoreInstance( )->SetSoundReceiverRealWorldPositionOrientationVU( iSoundSourceID, VAVec3( px, py, pz ), VAVec3( vx, vy, vz ), VAVec3( ux, uy, uz ) );
	VA_CS_CATCH_AND_PRINT_ERROR_2;
}


void NativeGetPortalName( CUnmanagedVANetClient* pClient, int iPortalID, char* pcName )
{
	VA_CS_TRY
	{
		VA_CS_REQUIRE_CONNECTED;
		std::string sName = pClient->GetCoreInstance( )->GetSoundPortalName( iPortalID );
		CopyFromString( sName, pcName );
	}
	VA_CS_CATCH_AND_PRINT_ERROR
}

void NativeSetPortalName( CUnmanagedVANetClient* pClient, int iPortalID, const char* pcName )
{
	VA_CS_TRY
	{
		VA_CS_REQUIRE_CONNECTED;
		pClient->GetCoreInstance( )->SetSoundPortalName( iPortalID, std::string( pcName ) );
	}
	VA_CS_CATCH_AND_PRINT_ERROR
}

bool NativeGetInputMuted( CUnmanagedVANetClient* pClient )
{
	VA_CS_TRY
	{
		VA_CS_REQUIRE_CONNECTED;
		return pClient->GetCoreInstance( )->GetInputMuted( );
	}
	VA_CS_CATCH_AND_PRINT_ERROR
}

void NativeSetInputMuted( CUnmanagedVANetClient* pClient, bool bMuted )
{
	VA_CS_TRY
	{
		VA_CS_REQUIRE_CONNECTED;
		pClient->GetCoreInstance( )->SetInputMuted( bMuted );
	}
	VA_CS_CATCH_AND_PRINT_ERROR
}

double NativeGetInputGain( CUnmanagedVANetClient* pClient )
{
	VA_CS_TRY
	{
		VA_CS_REQUIRE_CONNECTED;
		return pClient->GetCoreInstance( )->GetInputGain( );
	}
	VA_CS_CATCH_AND_PRINT_ERROR
}

void NativeSetInputGain( CUnmanagedVANetClient* pClient, double dGain )
{
	VA_CS_TRY
	{
		VA_CS_REQUIRE_CONNECTED;
		pClient->GetCoreInstance( )->SetInputGain( dGain );
	}
	VA_CS_CATCH_AND_PRINT_ERROR
}

double NativeGetOutputGain( CUnmanagedVANetClient* pClient )
{
	VA_CS_TRY
	{
		VA_CS_REQUIRE_CONNECTED;
		return pClient->GetCoreInstance( )->GetOutputGain( );
	}
	VA_CS_CATCH_AND_PRINT_ERROR
}

void NativeSetOutputGain( CUnmanagedVANetClient* pClient, double dGain )
{
	VA_CS_TRY
	{
		VA_CS_REQUIRE_CONNECTED;
		pClient->GetCoreInstance( )->SetOutputGain( dGain );
	}
	VA_CS_CATCH_AND_PRINT_ERROR
}

bool NativeGetOutputMuted( CUnmanagedVANetClient* pClient )
{
	VA_CS_TRY
	{
		VA_CS_REQUIRE_CONNECTED;
		return pClient->GetCoreInstance( )->GetOutputMuted( );
	}
	VA_CS_CATCH_AND_PRINT_ERROR
}

void NativeSetOutputMuted( CUnmanagedVANetClient* pClient, bool bMuted )
{
	VA_CS_TRY
	{
		VA_CS_REQUIRE_CONNECTED;
		pClient->GetCoreInstance( )->SetOutputMuted( bMuted );
	}
	VA_CS_CATCH_AND_PRINT_ERROR
}

int NativeGetGlobalAuralizationMode( CUnmanagedVANetClient* pClient )
{
	VA_CS_TRY
	{
		VA_CS_REQUIRE_CONNECTED;
		return pClient->GetCoreInstance( )->GetGlobalAuralizationMode( );
	}
	VA_CS_CATCH_AND_PRINT_ERROR
}

void NativeSetGlobalAuralizationMode( CUnmanagedVANetClient* pClient, const char* pcAuralizationMode )
{
	VA_CS_TRY
	{
		VA_CS_REQUIRE_CONNECTED;
		int iCurrentAM        = pClient->GetCoreInstance( )->GetGlobalAuralizationMode( );
		int iAuralizationMode = IVAInterface::ParseAuralizationModeStr( std::string( pcAuralizationMode ), iCurrentAM );
		pClient->GetCoreInstance( )->SetGlobalAuralizationMode( iAuralizationMode );
	}
	VA_CS_CATCH_AND_PRINT_ERROR
}

double NativeGetCoreClock( CUnmanagedVANetClient* pClient )
{
	VA_CS_TRY
	{
		VA_CS_REQUIRE_CONNECTED;
		return pClient->GetCoreInstance( )->GetCoreClock( );
	}
	VA_CS_CATCH_AND_PRINT_ERROR
}

void NativeSetCoreClock( CUnmanagedVANetClient* pClient, double dSeconds )
{
	VA_CS_TRY
	{
		VA_CS_REQUIRE_CONNECTED;
		pClient->GetCoreInstance( )->SetCoreClock( dSeconds );
	}
	VA_CS_CATCH_AND_PRINT_ERROR
}

void NativeSubstituteMacros( CUnmanagedVANetClient* pClient, const char* pcStr, char* pcSubstitutedStr )
{
	VA_CS_TRY
	{
		VA_CS_REQUIRE_CONNECTED;
		std::string sSubstitutedStr = pClient->GetCoreInstance( )->SubstituteMacros( std::string( pcStr ) );
		CopyFromString( sSubstitutedStr, pcSubstitutedStr );
	}
	VA_CS_CATCH_AND_PRINT_ERROR
}

void NativeSetRenderingModuleMuted( CUnmanagedVANetClient* pClient, const char* pcModuleID, bool bMuted )
{
	VA_CS_TRY
	{
		VA_CS_REQUIRE_CONNECTED;
		pClient->GetCoreInstance( )->SetRenderingModuleMuted( std::string( pcModuleID ), bMuted );
	}
	VA_CS_CATCH_AND_PRINT_ERROR
}

bool NativeGetRenderingModuleMuted( CUnmanagedVANetClient* pClient, const char* pcModuleID )
{
	VA_CS_TRY
	{
		VA_CS_REQUIRE_CONNECTED;
		return pClient->GetCoreInstance( )->GetRenderingModuleMuted( std::string( pcModuleID ) );
	}
	VA_CS_CATCH_AND_PRINT_ERROR
}

void NativeSetRenderingModuleGain( CUnmanagedVANetClient* pClient, const char* pcModuleID, double dGain )
{
	VA_CS_TRY
	{
		VA_CS_REQUIRE_CONNECTED;
		pClient->GetCoreInstance( )->SetRenderingModuleGain( std::string( pcModuleID ), dGain );
	}
	VA_CS_CATCH_AND_PRINT_ERROR
}

double NativeGetRenderingModuleGain( CUnmanagedVANetClient* pClient, const char* pcModuleID )
{
	VA_CS_TRY
	{
		VA_CS_REQUIRE_CONNECTED;
		return pClient->GetCoreInstance( )->GetRenderingModuleGain( std::string( pcModuleID ) );
	}
	VA_CS_CATCH_AND_PRINT_ERROR
}

void NativeSetReproductionModuleMuted( CUnmanagedVANetClient* pClient, const char* pcModuleID, bool bMuted )
{
	VA_CS_TRY
	{
		VA_CS_REQUIRE_CONNECTED;
		pClient->GetCoreInstance( )->SetReproductionModuleMuted( std::string( pcModuleID ), bMuted );
	}
	VA_CS_CATCH_AND_PRINT_ERROR
}

bool NativeGetReproductionModuleMuted( CUnmanagedVANetClient* pClient, const char* pcModuleID )
{
	VA_CS_TRY
	{
		VA_CS_REQUIRE_CONNECTED;
		return pClient->GetCoreInstance( )->GetReproductionModuleMuted( std::string( pcModuleID ) );
	}
	VA_CS_CATCH_AND_PRINT_ERROR
}

void NativeSetReproductionModuleGain( CUnmanagedVANetClient* pClient, const char* pcModuleID, double dGain )
{
	VA_CS_TRY
	{
		VA_CS_REQUIRE_CONNECTED;
		pClient->GetCoreInstance( )->SetReproductionModuleGain( std::string( pcModuleID ), dGain );
	}
	VA_CS_CATCH_AND_PRINT_ERROR
}

double NativeGetReproductionModuleGain( CUnmanagedVANetClient* pClient, const char* pcModuleID )
{
	VA_CS_TRY_2;

	VA_CS_REQUIRE_CONNECTED;
	return pClient->GetCoreInstance( )->GetReproductionModuleGain( std::string( pcModuleID ) );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetArtificialReverberationTime( CUnmanagedVANetClient* pClient, const char* pcRendererID, double dReverberationTime )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	std::vector<CVAAudioRendererInfo> voRenderer;
	pClient->GetCoreInstance( )->GetRenderingModules( voRenderer, true );

	for( size_t i = 0; i < voRenderer.size( ); i++ )
	{
		if( voRenderer[i].sClass == "BinauralArtificialReverb" )
		{
			if( voRenderer[i].sID == std::string( pcRendererID ) )
			{
				std::string sModuleID = voRenderer[i].sClass + ":" + voRenderer[i].sID;
				CVAStruct oArgs, oRet;
				oArgs["set"]   = "ROOMREVERBERATIONTIME";
				oArgs["value"] = dReverberationTime;
				oRet           = pClient->GetCoreInstance( )->CallModule( sModuleID, oArgs );
			}
		}
	}

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetArtificialRoomVolume( CUnmanagedVANetClient* pClient, const char* pcRendererID, double dVolume )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	std::vector<CVAAudioRendererInfo> voRenderer;
	pClient->GetCoreInstance( )->GetRenderingModules( voRenderer, true );

	for( size_t i = 0; i < voRenderer.size( ); i++ )
	{
		if( voRenderer[i].sClass == "BinauralArtificialReverb" )
		{
			if( voRenderer[i].sID == std::string( pcRendererID ) )
			{
				std::string sModuleID = voRenderer[i].sClass + ":" + voRenderer[i].sID;
				CVAStruct oArgs, oRet;
				oArgs["set"]   = "ROOMVOLUME";
				oArgs["value"] = dVolume;
				oRet           = pClient->GetCoreInstance( )->CallModule( sModuleID, oArgs );
			}
		}
	}


	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetArtificialSurfaceArea( CUnmanagedVANetClient* pClient, const char* pcRendererID, double dArea )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	std::vector<CVAAudioRendererInfo> voRenderer;
	pClient->GetCoreInstance( )->GetRenderingModules( voRenderer, true );

	for( size_t i = 0; i < voRenderer.size( ); i++ )
	{
		if( voRenderer[i].sClass == "BinauralArtificialReverb" )
		{
			if( voRenderer[i].sID == std::string( pcRendererID ) )
			{
				std::string sModuleID = voRenderer[i].sClass + ":" + voRenderer[i].sID;
				CVAStruct oArgs, oRet;
				oArgs["set"]   = "ROOMSURFACEAREA";
				oArgs["value"] = dArea;
				oRet           = pClient->GetCoreInstance( )->CallModule( sModuleID, oArgs );
			}
		}
	}

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetSoundReceiverAnthropometricData( CUnmanagedVANetClient* pClient, int iSoundReceiverID, double dHeadWidth, double dHeadHeight, double dHeadDepth )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	CVAStruct oAnthroParams;
	oAnthroParams["HeadWidth"]  = dHeadWidth;
	oAnthroParams["HeadHeight"] = dHeadHeight;
	oAnthroParams["HeadDepth"]  = dHeadDepth;
	CVAStruct oSoundReceiverParams;
	oSoundReceiverParams["anthroparams"] = oAnthroParams;
	pClient->GetCoreInstance( )->SetSoundReceiverParameters( iSoundReceiverID, oSoundReceiverParams );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeTextToSpeechPrepareTextAndPlaySpeech( CUnmanagedVANetClient* pClient, const char* pcSignalSourceIdentifier, const char* pcText )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	CVAStruct oTTS;
	oTTS["id"]              = "direct_playback";
	oTTS["prepare_text"]    = std::string( pcText );
	oTTS["direct_playback"] = true;
	pClient->GetCoreInstance( )->SetSignalSourceParameters( std::string( pcSignalSourceIdentifier ), oTTS );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeTextToSpeechPrepareText( CUnmanagedVANetClient* pClient, const char* pcSignalSourceIdentifier, const char* pcTextIdentifier, const char* pcText )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	CVAStruct oTTS;
	oTTS["id"]           = std::string( pcTextIdentifier );
	oTTS["prepare_text"] = std::string( pcText );
	pClient->GetCoreInstance( )->SetSignalSourceParameters( std::string( pcSignalSourceIdentifier ), oTTS );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeTextToSpeechPlaySpeech( CUnmanagedVANetClient* pClient, const char* pcSignalSourceIdentifier, const char* pcTextIdentifier )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	CVAStruct oTTS;
	oTTS["play_speech"] = std::string( pcTextIdentifier );
	pClient->GetCoreInstance( )->SetSignalSourceParameters( std::string( pcSignalSourceIdentifier ), oTTS );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

int NativeCreateGeometryMeshFromFile( CUnmanagedVANetClient* pClient, const char* pcFilePath )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;
	return pClient->GetCoreInstance( )->CreateGeometryMeshFromFile( std::string( pcFilePath ) );
	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeDeleteGeometryMesh( CUnmanagedVANetClient* pClient, int iID )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;
	pClient->GetCoreInstance( )->DeleteGeometryMesh( iID );
	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeGetGeometryMeshName( CUnmanagedVANetClient* pClient, int iID, char* pcName )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	std::string sName = pClient->GetCoreInstance( )->GetGeometryMeshName( iID );
	CopyFromString( sName, pcName );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetGeometryMeshName( CUnmanagedVANetClient* pClient, int iID, const char* pcName )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	pClient->GetCoreInstance( )->SetGeometryMeshName( iID, std::string( pcName ) );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

bool NativeGetGeometryMeshEnabled( CUnmanagedVANetClient* pClient, int iID )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	return pClient->GetCoreInstance( )->GetGeometryMeshEnabled( iID );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetGeometryMeshEnabled( CUnmanagedVANetClient* pClient, int iID, bool bEnabled )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	pClient->GetCoreInstance( )->SetGeometryMeshEnabled( iID, bEnabled );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeCreateSceneFromFile( CUnmanagedVANetClient* pClient, const char* pcFilePath, char* pcSceneID )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	CVAStruct oScene;
	oScene["filepath"] = std::string( pcFilePath );
	std::string sID    = pClient->GetCoreInstance( )->CreateScene( oScene );
	CopyFromString( sID, pcSceneID );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeGetSceneName( CUnmanagedVANetClient* pClient, const char* pcID, char* pcName )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	std::string sName = pClient->GetCoreInstance( )->GetSceneName( std::string( pcID ) );
	CopyFromString( sName, pcName );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetSceneName( CUnmanagedVANetClient* pClient, const char* pcID, const char* pcName )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	pClient->GetCoreInstance( )->SetSceneName( std::string( pcID ), std::string( pcName ) );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

bool NativeGetSceneEnabled( CUnmanagedVANetClient* pClient, const char* pcID )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	return pClient->GetCoreInstance( )->GetSceneEnabled( std::string( pcID ) );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeSetSceneEnabled( CUnmanagedVANetClient* pClient, const char* pcID, bool bEnabled )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	pClient->GetCoreInstance( )->SetSceneEnabled( std::string( pcID ), bEnabled );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}


// --- Generic path renderer ---

void NativeUpdateGenericPath( CUnmanagedVANetClient* pClient, const char* pcRendererID, int iSourceID, int iReceiverID, int iChannel, float fDelaySeconds,
                              int iNumSamples, float* vfSampleBuffer )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	CVASampleBuffer oSampleBuffer( iNumSamples, true );
	for( int i = 0; i < iNumSamples; i++ )
		oSampleBuffer.vfSamples[i] = vfSampleBuffer[i];

	CVAStruct oArgs;
	oArgs["source"]    = iSourceID;
	oArgs["receiver"]  = iReceiverID;
	oArgs["channel"]   = iChannel;
	oArgs["delay"]     = fDelaySeconds;
	std::string sChKey = "ch" + std::to_string( (long)iChannel );
	oArgs[sChKey]      = oSampleBuffer;

	pClient->GetCoreInstance( )->SetRenderingModuleParameters( std::string( pcRendererID ), oArgs );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeUpdateGenericPathFromFile( CUnmanagedVANetClient* pClient, const char* pcRendererID, int iSourceID, int iReceiverID, const char* pcFilePath )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	CVAStruct oArgs;
	oArgs["source"]   = iSourceID;
	oArgs["receiver"] = iReceiverID;
	oArgs["filepath"] = std::string( pcFilePath );

	pClient->GetCoreInstance( )->SetRenderingModuleParameters( std::string( pcRendererID ), oArgs );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

void NativeUpdateGenericPathDelay( CUnmanagedVANetClient* pClient, const char* pcRendererID, int iSourceID, int iReceiverID, float fDelaySeconds )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	CVAStruct oArgs;
	oArgs["source"]   = iSourceID;
	oArgs["receiver"] = iReceiverID;
	oArgs["delay"]    = std::max( fDelaySeconds, 0.0f );

	pClient->GetCoreInstance( )->SetRenderingModuleParameters( std::string( pcRendererID ), oArgs );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}

// --- Ambient mixer renderer ---

void NativeAmbientMixerRendererPlaySampleFromFile( CUnmanagedVANetClient* pClient, const char* pcRendererID, const char* pcFilePath )
{
	VA_CS_TRY_2;
	VA_CS_REQUIRE_CONNECTED;

	CVAStruct oSamplerParams;
	oSamplerParams["SampleFilePath"] = std::string( pcFilePath );

	CVAStruct oArgs;
	oArgs["Sampler"] = oSamplerParams;

	pClient->GetCoreInstance( )->SetRenderingModuleParameters( std::string( pcRendererID ), oArgs );

	VA_CS_CATCH_AND_PRINT_ERROR_2;
}
