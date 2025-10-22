/*
 *  --------------------------------------------------------------------------------------------
 *
 *    VVV        VVV A           Virtual Acoustics (VA) | http://www.virtualacoustics.org
 *     VVV      VVV AAA          Licensed under the Apache License, Version 2.0
 *      VVV    VVV   AAA
 *       VVV  VVV     AAA        Copyright 2015-2021
 *        VVVVVV       AAA       Institute of Technical Acoustics (ITA)
 *         VVVV         AAA      RWTH Aachen University
 *
 *  --------------------------------------------------------------------------------------------
 */

#include "VAMatlabConnection.h"
#include "VAMatlabFunctionMapping.h"
#include "VAMatlabHelpers.h"
#include "VAMatlabTracking.h"

// Matlab includes
#include <matrix.h>
#include <mex.h>

// VA includes
#include <VA.h>
#include <VANet.h>

// STL includes
#include <algorithm>
#include <map>
#include <sstream>
#include <stdarg.h>

#ifdef WIN32
#	include <windows.h>
#endif

// Unreferenced formal parameters appear a lot with the generic mex API, we will ignore them
#pragma warning( disable : 4100 )

// Maximum number of parallel connections
const int VAMATLAB_MAX_CONNECTIONS = 16;

// Connection handle datatype
typedef int32_t ConnectionHandle;
const mxClassID CONNECTIONHANDLE_CLASS_ID = mxINT32_CLASS;
const mxClassID ID_CLASS_ID               = mxINT32_CLASS;


bool g_bFirst = true;                                                                     // First call of Matlab mexFunction
std::vector<CVAMatlabConnection *> g_vpConnections( VAMATLAB_MAX_CONNECTIONS + 1, NULL ); // Hashtable handles -> connections (1st element always NULL)
ConnectionHandle g_iLastConnectionHandle = 0;                                             // Global connection ID counter
int g_iConnectionsEstablished            = 0;                                             // Number of established connections
IVAInterface *g_pOwnCore                 = NULL;                                          // Own core instance (deploy mode 1)
CVAMatlabConnection g_oDummyConnection;                                                   // Dummy 'connection' to own core (deploy mode 1)

#define VERBOSE_LEVEL_QUIET  0
#define VERBOSE_LEVEL_NORMAL 1
int g_iVerboseLevel = VERBOSE_LEVEL_NORMAL; // Verbosity level


/* +------------------------------------------------------+
   |                                                      |
   |   Auxilary functions                                 |
   |                                                      |
   +------------------------------------------------------+ */

// Verbose functions

void INFO( const char *format, ... )
{
	if( g_iVerboseLevel == VERBOSE_LEVEL_QUIET )
		return; // Quiet?

	const int bufsize = 1024;
	char buf[bufsize];

	va_list args;
	va_start( args, format );
	vsprintf_s( buf, bufsize, format, args );
	va_end( args );

	mexPrintf( buf );
}

// Generate a new connection handle (ID)
ConnectionHandle GenerateConnectionHandle( )
{
	// Scan for free entries starting from the last handle
	ConnectionHandle hHandle = -1;
	int i                    = g_iLastConnectionHandle;
	do
	{
		i = ( i + 1 ) % VAMATLAB_MAX_CONNECTIONS;
		if( i == 0 )
			i++; // Skip placeholder

		if( g_vpConnections[i] == NULL )
		{
			g_iLastConnectionHandle = i;
			return i;
		}
	} while( hHandle == -1 );

	// Should not reach this line!
	VA_EXCEPT1( "An internal error occured - Please contact the developer" );
}

// Get connection handle from Matlab arguments and validate it
ConnectionHandle GetConnectionHandle( const mxArray *pArray, bool bAllowNullHandle = false )
{
	// Strict typing! Make sure that the parameter has a valid type (scalar + handle datatype)
	if( matlabIsScalar( pArray ) && ( mxGetClassID( pArray ) == CONNECTIONHANDLE_CLASS_ID ) )
	{
		ConnectionHandle hHandle = *( (ConnectionHandle *)mxGetData( pArray ) );
		if( ( hHandle >= 0 ) && ( hHandle <= VAMATLAB_MAX_CONNECTIONS ) )
		{
			if( g_vpConnections[hHandle] || ( ( hHandle == 0 ) && bAllowNullHandle ) )
				return hHandle;
		}
	}

	VA_EXCEPT1( "Invalid connection handle" );
}

// Checks if the correct excat number of arguments is provided
// and causes a Matlab error if not...
// USED FOR? Convenience baby! This saves some lines of code below ...
// NOTE: In this MEX we are always checking for the exact number of arguments
//       Optional values are predefined in the Matlab facade class
void CheckInputArguments( int nrhs, int iRequiredNumArgs )
{
	if( nrhs != iRequiredNumArgs )
	{
		switch( iRequiredNumArgs )
		{
			case 0:
				VA_EXCEPT1( "This VAMatlab function does not take any arguments" );
			case 1:
				VA_EXCEPT1( "This VAMatlab function takes exactly one argument" );
			case 2:
				VA_EXCEPT1( "This VAMatlab function takes exactly two arguments" );
			case 3:
				VA_EXCEPT1( "This VAMatlab function takes exactly three arguments" );
			case 4:
				VA_EXCEPT1( "This VAMatlab function takes exactly four arguments" );
			case 5:
				VA_EXCEPT1( "This VAMatlab function takes exactly five arguments" );
			case 6:
				VA_EXCEPT1( "This VAMatlab function takes exactly six arguments" );
				// Do we need more?
			default:
			{
				char buf[64];
				sprintf_s( buf, 64, "This VAMatlab function takes exactly %d arguments", iRequiredNumArgs );
				VA_EXCEPT1( buf );
			}
		}
	}
}

// Displays information in the executable in Matlab
void banner( )
{
	mexPrintf( "/*\n" );
	mexPrintf( " *  --------------------------------------------------------------------------------------------\n" );
	mexPrintf( " *\n" );
	mexPrintf( " *    VVV        VVV A           Virtual Acoustics (VA) | http://www.virtualacoustics.org\n" );
	mexPrintf( " *     VVV      VVV AAA          Licensed under the Apache License, Version 2.0\n" );
	mexPrintf( " *      VVV    VVV   AAA\n" );
	mexPrintf( " *       VVV  VVV     AAA        Copyright 2015-2022\n" );
	mexPrintf( " *        VVVVVV       AAA       Institute for Hearing Technology and Acoustics (IHTA)\n" );
	mexPrintf( " *         VVVV         AAA      RWTH Aachen University\n" );
	mexPrintf( " *\n" );
	mexPrintf( " *  --------------------------------------------------------------------------------------------\n" );
#ifdef VAMATLAB_INTEGRATED
	mexPrintf( " *                               VA Matlab with integrated core, version %s.%s\n", VAMATLAB_VERSION_MAJOR, VAMATLAB_VERSION_MINOR );
#else
	mexPrintf( " *    VA Matlab network client, version %s%s\n", VAMATLAB_VERSION_MAJOR, VAMATLAB_VERSION_MINOR );
#endif
	mexPrintf( " *  --------------------------------------------------------------------------------------------\n" );
	mexPrintf( " */\n" );
}

// Helper macro
#define REQUIRE_INPUT_ARGS( N ) CheckInputArguments( nrhs, N )

/* +------------------------------------------------------+
   |                                                      |
   |   MEX Entry function                                 |
   |                                                      |
   +------------------------------------------------------+ */

void mexFunction( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	if( g_bFirst )
	{
		g_bFirst = false;
#ifdef VAMATLAB_SHOW_BANNER
		banner( );
#endif
	}

	// At least one argument must be specified
	if( nrhs < 1 )
		mexErrMsgTxt( "Command argument expected" );

	try
	{
		std::string sCommand = matlabGetString( prhs[0], "command" );

		// Convert command string to lowercase
		std::transform( sCommand.begin( ), sCommand.end( ), sCommand.begin( ), toupper );
		FunctionMapIterator it = g_mFunctionMap.find( sCommand );
		if( it == g_mFunctionMap.end( ) )
			VA_EXCEPT1( "Invalid command argument '" + sCommand + "'" );

		// Call function (Skip first rhs 'command' argument for internal function calls)
		// Convert C++ exceptions into Matlab errors
		( *it->second.pAddr )( nlhs, plhs, nrhs - 1, &prhs[1] );
	}
	catch( CVAException &e )
	{
		mexErrMsgTxt( e.GetErrorMessage( ).c_str( ) );
	}
	catch( ... )
	{
		mexErrMsgTxt( "An internal error occured - Please contact the developer" );
	}
}


/* +------------------------------------------------------+
   |                                                      |
   |   Command functions                                  |
   |                                                      |
   +------------------------------------------------------+ */

// Reflexion function. Returns cell-array of structs with information on the command functions.
// Used for code generation of the Matlab MEX facade class [private]

REGISTER_PRIVATE_FUNCTION( enumerate_functions );

void enumerate_functions( int nlhs, mxArray *plhs[], int nrhs, const mxArray ** )
{
	// Count public functions
	mwSize nPublicFuncs = 0;
	for( FunctionMapIterator it = g_mFunctionMap.begin( ); it != g_mFunctionMap.end( ); ++it )
		if( it->second.bPublic )
			nPublicFuncs++;

	const mwSize nFields         = 5;
	const char *ppszFieldNames[] = { "name", "inargs", "outargs", "desc", "doc" };

	mxArray *pStruct = mxCreateStructMatrix( 1, nPublicFuncs, nFields, ppszFieldNames );
	plhs[0]          = pStruct;

	mwIndex i = 0;
	for( FunctionMapIterator it = g_mFunctionMap.begin( ); it != g_mFunctionMap.end( ); ++it )
	{
		if( !it->second.bPublic )
			continue;

		// Input arguments
		mxSetField( pStruct, i, ppszFieldNames[0], mxCreateString( it->second.sName.c_str( ) ) );
		mxSetField( pStruct, i, ppszFieldNames[1], CreateFunctionInputArgumentStruct( it->second.vInputArgs ) );
		mxSetField( pStruct, i, ppszFieldNames[2], CreateFunctionOutputArgumentStruct( it->second.vOutputArgs ) );
		mxSetField( pStruct, i, ppszFieldNames[3], mxCreateString( it->second.sDesc.c_str( ) ) );
		mxSetField( pStruct, i, ppszFieldNames[4], mxCreateString( it->second.sDoc.c_str( ) ) );

		i++;
	}
}

// ------------------------------------------------------------

REGISTER_PRIVATE_FUNCTION( get_version );
void get_version( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	std::stringstream ss;
	ss << "VAMatlab " << VAMATLAB_VERSION_MAJOR << "." << VAMATLAB_VERSION_MINOR;
#ifdef DEBUG
	ss << " (debug)";
#else
	ss << " (release)";
#endif

	if( nlhs == 1 )
		plhs[0] = mxCreateString( ss.str( ).c_str( ) );
	else
		mexPrintf( "%s", ss.str( ).c_str( ) );
}

// ------------------------------------------------------------

REGISTER_PRIVATE_FUNCTION( set_verbose_mode );
void set_verbose_mode( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 1 );

	std::string sMode = matlabGetString( prhs[0], "mode" );
	std::transform( sMode.begin( ), sMode.end( ), sMode.begin( ), toupper );

	if( sMode == "QUIET" )
	{
		g_iVerboseLevel = VERBOSE_LEVEL_QUIET;
		return;
	}

	if( sMode == "NORMAL" )
	{
		g_iVerboseLevel = VERBOSE_LEVEL_NORMAL;
		return;
	}

	VA_EXCEPT1( "Invalid verbose mode, use 'quiet' or 'normal'" );
}

// ------------------------------------------------------------

REGISTER_PRIVATE_FUNCTION( get_connected );

#if VAMATLAB_INTEGRATED == 1

void get_connected( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 1 );

	plhs[0] = mxCreateLogicalScalar( true );
};

#else // VAMATLAB_INTEGRATED

void get_connected( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 1 );

	ConnectionHandle hHandle = GetConnectionHandle( prhs[0], true );
	if( hHandle == 0 )
	{
		plhs[0] = mxCreateLogicalScalar( false );
		return;
	}

	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];
	bool bConnected = pConnection->pClient->IsConnected( );
	plhs[0] = mxCreateLogicalScalar( bConnected );
};

#endif // VAMATLAB_INTEGRATED

// ------------------------------------------------------------

REGISTER_PRIVATE_FUNCTION( connect );

#if VAMATLAB_INTEGRATED == 1

void connect( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 1 );

	// Always connection ID 1
	ConnectionHandle hHandle = 1;

	if( g_iConnectionsEstablished == 0 )
	{
		try
		{
			std::string sPath       = getDirectoryFromPath( VACore::GetCoreLibFilename( ) );
			std::string sConfigFile = combinePath( sPath, VA_DEFAULT_CONFIGFILE );
			g_pOwnCore              = VACore::CreateCoreInstance( sConfigFile );
			g_pOwnCore->Initialize( );
		}
		catch( CVAException &e )
		{
			delete g_pOwnCore;
			g_pOwnCore = NULL;

			char buf[4096];
			sprintf_s( buf, 4096, "Failed to initialize VACore. %s.", e.GetErrorMessage( ).c_str( ) );
			mexErrMsgTxt( buf );
		}
		catch( ... )
		{
			delete g_pOwnCore;
			g_pOwnCore = NULL;

			mexErrMsgTxt( "Failed to initialize VACore" );
		}

		mexPrintf( "VACore initialized\n" );

		// First connection = Dummy connection to internal core
		g_oDummyConnection.pClient        = NULL;
		g_oDummyConnection.pCoreInterface = g_pOwnCore;
		g_vpConnections[hHandle]          = &g_oDummyConnection;
	}

	++g_iConnectionsEstablished;

	// Return the handle
	plhs[0] = matlabCreateID( hHandle );
}

#else // VAMATLAB_INTEGRATED

void connect( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 1 );

	std::string sRemoteAddress = matlabGetString( prhs[0], "address" );

	std::string sAddress;
	int iPort;
	SplitServerString( sRemoteAddress, sAddress, iPort );

	if( g_iConnectionsEstablished == VAMATLAB_MAX_CONNECTIONS )
	{
		VA_EXCEPT1( "Maximum number of connections reached. Close another opened connection first." );
		return;
	}

	CVAMatlabConnection *pConnection = new CVAMatlabConnection;
	// TODO: pConnection->pClient->AttachEventHandler(&g_oNetHandler);

	try
	{
		pConnection->pClient->Initialize( sAddress, iPort, IVANetClient::EXC_CLIENT_THROW, false );

		if( !pConnection->pClient->IsConnected( ) )
		{
			// TODO: Delete object. Here were some error with double destruction. Exception in destr?
			std::stringstream ss;
			ss << "Connection to server \"" << sAddress << "\" failed";
			VA_EXCEPT1( ss.str( ) );
			return;
		}

		pConnection->pCoreInterface = pConnection->pClient->GetCoreInstance( );
		pConnection->pVAMatlabTracker->pVACore = pConnection->pCoreInterface;

		CVAVersionInfo oRemoteCoreVersion;
		pConnection->pCoreInterface->GetVersionInfo( &oRemoteCoreVersion );

		CVANetVersionInfo oNetVersion;
		GetVANetVersionInfo( &oNetVersion );
	}
	catch( ... )
	{
		delete pConnection;
		throw;
	}

	ConnectionHandle hHandle = GenerateConnectionHandle( );
	g_vpConnections[hHandle] = pConnection;
	++g_iConnectionsEstablished;

	// Return the handle
	plhs[0] = matlabCreateID( hHandle );

	INFO( "Connection to VA server \"%s\" established (connection handle: %d)\n", pConnection->pClient->GetServerAddress( ).c_str( ), hHandle );
}

#endif // VAMATLAB_INTEGRATED

// ------------------------------------------------------------

REGISTER_PRIVATE_FUNCTION( disconnect );

#if VAMATLAB_INTEGRATED == 1

void disconnect( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 1 );

	// Nothing to do ... Dummy function ...
	if( g_iConnectionsEstablished < 1 )
	{
		VA_EXCEPT1( "No connections established" );
		return;
	}

	--g_iConnectionsEstablished;
	g_vpConnections[1] = NULL;

	if( g_iConnectionsEstablished == 0 )
	{
		// Aufräumen
		try
		{
			g_pOwnCore->Finalize( );
			delete g_pOwnCore;
			g_pOwnCore = NULL;
		}
		catch( CVAException &e )
		{
			delete g_pOwnCore;
			g_pOwnCore = NULL;

			char buf[4096];
			sprintf_s( buf, 4096, "Failed to finalize VACore. %s.", e.GetErrorMessage( ).c_str( ) );
			mexErrMsgTxt( buf );
		}
		catch( ... )
		{
			delete g_pOwnCore;
			g_pOwnCore = NULL;

			mexErrMsgTxt( "Failed to finalize VACore. An unknown error occured." );
		}

		mexPrintf( "VACore finalized\n" );
	}
}

#else // VAMATLAB_INTEGRATED

void disconnect( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 1 );

	ConnectionHandle hHandle = GetConnectionHandle( prhs[0], true );
	if( hHandle == 0 )
		return;

	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::string sAddr( pConnection->pClient->GetServerAddress( ) );

	g_vpConnections[hHandle] = NULL;
	--g_iConnectionsEstablished;
	delete pConnection;

	INFO( "Disconnected from VA server \"%s\" (connection handle: %d)\n", sAddr.c_str( ), hHandle );
}

#endif // VAMATLAB_INTEGRATED

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( get_server_address, "Returns for an opened connection the server it is connected to", "" );
DECLARE_FUNCTION_OUTARG( get_server_address, addr, "string", "Server address" );

#if VAMATLAB_INTEGRATED == 1

void get_server_address( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 1 );

	// We not really have a connection. But definitely the core is on this machine... :-)
	prhs[0] = mxCreateString( "localhost" );
}


#else // VAMATLAB_INTEGRATED

void get_server_address( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 1 );

	ConnectionHandle hHandle = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	prhs[0] = mxCreateString( pConnection->pClient->GetServerAddress( ).c_str( ) );
}

#endif // VAMATLAB_INTEGRATED


// -------------------------- TRACKER ----------------------------------

REGISTER_PRIVATE_FUNCTION( connect_tracker );
DECLARE_FUNCTION_OPTIONAL_INARG( connect_tracker, serverIP, "string", "Server IP", "''" );
DECLARE_FUNCTION_OPTIONAL_INARG( connect_tracker, localIP, "string", "Local IP", "''" );
DECLARE_FUNCTION_OPTIONAL_INARG( connect_tracker, trackerType, "string", "Tracking Type [OptiTrack(default), ART]", "OptiTrack" );
void connect_tracker( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	if( nrhs == 0 )
		VA_EXCEPT2( INVALID_PARAMETER, "Missing VA connection handle in ConnectTracker" );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];
	CVAMatlabTracker *pTracker       = pConnection->pVAMatlabTracker;

	std::string sServerAdress = "127.0.0.1";
	if( nrhs > 1 )
		sServerAdress = std::string( matlabGetString( prhs[1], "serverIP" ) );

	std::string sLocalAdress = "127.0.0.1";
	if( nrhs > 2 )
		sLocalAdress = std::string( matlabGetString( prhs[2], "localIP" ) );

	std::string sType = "OptiTrack";
	if( nrhs > 3 )
		sType = std::string( matlabGetString( prhs[3], "trackerType" ) );

	if( pTracker->Initialize( sServerAdress, sLocalAdress, sType ) == false )
		VA_EXCEPT2( INVALID_PARAMETER, "Could not initialize connection to tracker with remote adress '" + sServerAdress + "' and local adress '" + sLocalAdress + "'" );
}

REGISTER_PRIVATE_FUNCTION( disconnect_tracker );
void disconnect_tracker( int, mxArray *[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 1 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	if( pConnection->pVAMatlabTracker->Uninitialize( ) == false )
		VA_EXCEPT2( INVALID_PARAMETER, "Could not disconnect from tracker" );

	pConnection->pVAMatlabTracker->Reset( );
}

REGISTER_PRIVATE_FUNCTION( get_tracker_connected );
DECLARE_FUNCTION_OUTARG( get_tracker_connected, connected, "logical scalar", "True if a tracker connection is established" );
void get_tracker_connected( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 1 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];
	CVAMatlabTracker *pTracker       = pConnection->pVAMatlabTracker;

	bool bIsConnected = pTracker->IsConnected( );
	if( nlhs != 1 )
	{
		if( bIsConnected )
		{
			VA_EXCEPT2( INVALID_PARAMETER, "Can not return value, please provide exactly one left-hand side target value. But tracker is connected." );
		}
		else
		{
			VA_EXCEPT2( INVALID_PARAMETER, "Can not return value, please provide exactly one left-hand side target value. And tracker is not connected." );
		}
	}

	plhs[0] = mxCreateLogicalScalar( bIsConnected );

	return;
}

// Tracked sound receiver

REGISTER_PRIVATE_FUNCTION( set_tracked_sound_receiver );
DECLARE_FUNCTION_REQUIRED_INARG( set_tracked_sound_receiver, soundreceiverID, "scalar number", "Tracked sound receiver ID" );
void set_tracked_sound_receiver( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int isoundreceiverID                                   = matlabGetIntegerScalar( prhs[1], "soundreceiverID" );
	pConnection->pVAMatlabTracker->iTrackedSoundReceiverID = isoundreceiverID;
}

REGISTER_PRIVATE_FUNCTION( set_tracked_sound_receiver_head_rigid_body_index );
DECLARE_FUNCTION_REQUIRED_INARG( set_tracked_sound_receiver_head_rigid_body_index, index, "scalar number",
                                 "Tracked sound receiver head rigid body index (default is 1)" );
void set_tracked_sound_receiver_head_rigid_body_index( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int iTrackedRigidBodyIndex = matlabGetIntegerScalar( prhs[1], "index" );

	pConnection->pVAMatlabTracker->iTrackedSoundReceiverHeadRigidBodyIndex = iTrackedRigidBodyIndex;
}

REGISTER_PRIVATE_FUNCTION( set_tracked_sound_receiver_torso_rigid_body_index );
DECLARE_FUNCTION_REQUIRED_INARG( set_tracked_sound_receiver_torso_rigid_body_index, index, "scalar number",
                                 "Tracked hato sound receiver torso rigid body index (default is 1)" );
void set_tracked_sound_receiver_torso_rigid_body_index( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int iTrackedRigidBodyIndex = matlabGetIntegerScalar( prhs[1], "index" );

	pConnection->pVAMatlabTracker->iTrackedSoundReceiverTorsoRigidBodyIndex = iTrackedRigidBodyIndex;
}

REGISTER_PRIVATE_FUNCTION( set_tracked_sound_receiver_head_rigid_body_translation );
DECLARE_FUNCTION_REQUIRED_INARG( set_tracked_sound_receiver_head_rigid_body_translation, offset, "real 1x3", "Tracked sound receiver rigid body position offset" );
void set_tracked_sound_receiver_head_rigid_body_translation( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	VAVec3 v3Pos;
	matlabGetRealVector3( prhs[1], "offset", v3Pos );

	pConnection->pVAMatlabTracker->vTrackedSoundReceiverTranslation = VistaVector3D( float( v3Pos.x ), float( v3Pos.y ), float( v3Pos.z ) );
}

REGISTER_PRIVATE_FUNCTION( set_tracked_sound_receiver_head_rigid_body_rotation );
DECLARE_FUNCTION_REQUIRED_INARG( set_tracked_sound_receiver_head_rigid_body_rotation, rotation, "real 1x4",
                                 "Tracked sound receiver rigid body rotation (quaternion values with w (real), i, j, k)" );
void set_tracked_sound_receiver_head_rigid_body_rotation( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	VAQuat qOrient;
	matlabGetQuaternion( prhs[1], "rotation", qOrient );

	pConnection->pVAMatlabTracker->qTrackedSoundReceiverRotation = VistaQuaternion( float( qOrient.x ), float( qOrient.y ), float( qOrient.z ), float( qOrient.w ) );
}

// Tracked real-world sound receiver

REGISTER_PRIVATE_FUNCTION( set_tracked_real_world_sound_receiver );
DECLARE_FUNCTION_REQUIRED_INARG( set_tracked_real_world_sound_receiver, realworldsoundreceiverID, "scalar number", "Tracked real-world sound receiver ID" );
void set_tracked_real_world_sound_receiver( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int iRealWorldsoundreceiverID = matlabGetIntegerScalar( prhs[1], "realworldsoundreceiverID" );

	pConnection->pVAMatlabTracker->iTrackedRealWorldSoundReceiverID = iRealWorldsoundreceiverID;
}

REGISTER_PRIVATE_FUNCTION( set_tracked_real_world_sound_receiver_head_rigid_body_index );
DECLARE_FUNCTION_REQUIRED_INARG( set_tracked_real_world_sound_receiver_head_rigid_body_index, index, "scalar number",
                                 "Tracked real-world sound receiver rigid body index (default is 1)" );
void set_tracked_real_world_sound_receiver_head_rigid_body_index( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int iTrackedRigidBodyIndex = matlabGetIntegerScalar( prhs[1], "index" );

	pConnection->pVAMatlabTracker->iTrackedRealWorldSoundReceiverHeadRigidBodyIndex = iTrackedRigidBodyIndex;
}

REGISTER_PRIVATE_FUNCTION( set_tracked_real_world_sound_receiver_torso_rigid_body_index );
DECLARE_FUNCTION_REQUIRED_INARG( set_tracked_real_world_sound_receiver_torso_rigid_body_index, index, "scalar number",
                                 "Tracked real-world head-above-torso sound receiver rigid body index (default is 1)" );
void set_tracked_real_world_sound_receiver_torso_rigid_body_index( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int iTrackedRigidBodyIndex = matlabGetIntegerScalar( prhs[1], "index" );

	pConnection->pVAMatlabTracker->iTrackedRealWorldSoundReceiverTorsoRigidBodyIndex = iTrackedRigidBodyIndex;
}

REGISTER_PRIVATE_FUNCTION( set_tracked_real_world_sound_receiver_head_rigid_body_translation );
DECLARE_FUNCTION_REQUIRED_INARG( set_tracked_real_world_sound_receiver_head_rigid_body_translation, offset, "real 1x3",
                                 "Tracked real-world sound receiver rigid body position offset" );
void set_tracked_real_world_sound_receiver_head_rigid_body_translation( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	VAVec3 v3Pos;
	matlabGetRealVector3( prhs[1], "offset", v3Pos );

	pConnection->pVAMatlabTracker->vTrackedRealWorldSoundReceiverTranslation = VistaVector3D( float( v3Pos.x ), float( v3Pos.y ), float( v3Pos.z ) );
}

REGISTER_PRIVATE_FUNCTION( set_tracked_real_world_sound_receiver_head_rigid_body_rotation );
DECLARE_FUNCTION_REQUIRED_INARG( set_tracked_real_world_sound_receiver_head_rigid_body_rotation, rotation, "real 1x4",
                                 "Tracked real-world sound receiver rigid body rotation (quaternion values with w (real), i, j, k)" );
void set_tracked_real_world_sound_receiver_head_rigid_body_rotation( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	VAQuat qOrient;
	matlabGetQuaternion( prhs[1], "rotation", qOrient );

	pConnection->pVAMatlabTracker->qTrackedRealWorldSoundReceiverRotation =
	    VistaQuaternion( float( qOrient.x ), float( qOrient.y ), float( qOrient.z ), float( qOrient.w ) );
}


// Tracked source

REGISTER_PRIVATE_FUNCTION( set_tracked_sound_source );
DECLARE_FUNCTION_REQUIRED_INARG( set_tracked_sound_source, sourceID, "scalar number", "Tracked source ID" );
void set_tracked_sound_source( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int iSourceID = matlabGetIntegerScalar( prhs[1], "sourceID" );

	pConnection->pVAMatlabTracker->iTrackedSoundSourceID = iSourceID;
}

REGISTER_PRIVATE_FUNCTION( set_tracked_sound_source_rigid_body_index );
DECLARE_FUNCTION_REQUIRED_INARG( set_tracked_sound_source_rigid_body_index, index, "scalar number", "Tracked sound source rigid body index (default is 1)" );
void set_tracked_sound_source_rigid_body_index( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int iTrackedRigidBodyIndex = matlabGetIntegerScalar( prhs[1], "index" );

	pConnection->pVAMatlabTracker->iTrackedSoundSourceRigidBodyIndex = iTrackedRigidBodyIndex;
}

REGISTER_PRIVATE_FUNCTION( set_tracked_sound_source_rigid_body_translation );
DECLARE_FUNCTION_REQUIRED_INARG( set_tracked_sound_source_rigid_body_translation, offset, "real 1x3", "Tracked source rigid body position offset" );
void set_tracked_sound_source_rigid_body_translation( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	VAVec3 v3Pos;
	matlabGetRealVector3( prhs[1], "offset", v3Pos );

	pConnection->pVAMatlabTracker->vTrackedSoundSourceTranslation = VistaVector3D( v3Pos.comp );
}

REGISTER_PRIVATE_FUNCTION( set_tracked_sound_source_rigid_body_rotation );
DECLARE_FUNCTION_REQUIRED_INARG( set_tracked_sound_source_rigid_body_rotation, rotation, "real 1x4",
                                 "Tracked sound source rigid body rotation (quaternion values with w (real), i, j, k)" );
void set_tracked_sound_source_rigid_body_rotation( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	VAQuat qOrient;
	matlabGetQuaternion( prhs[1], "rotation", qOrient );

	pConnection->pVAMatlabTracker->qTrackedSoundSourceRotation = VistaQuaternion( qOrient.comp );
}

REGISTER_PRIVATE_FUNCTION( get_tracker_info );
DECLARE_FUNCTION_OUTARG( get_tracker_info, state, "struct", "Returns the current state of the tracker configuration" );
void get_tracker_info( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	CVAStruct oTrackerInfo;
	oTrackerInfo["IsConnected"] = pConnection->pVAMatlabTracker->IsConnected( );

	// Sound receiver
	oTrackerInfo["TrackedSoundReceiverID"] = pConnection->pVAMatlabTracker->iTrackedSoundReceiverID;

	oTrackerInfo["TrackedSoundReceiverHeadRigidBodyIndex"]  = pConnection->pVAMatlabTracker->iTrackedSoundReceiverHeadRigidBodyIndex;
	oTrackerInfo["TrackedSoundReceiverTorsoRigidBodyIndex"] = pConnection->pVAMatlabTracker->iTrackedSoundReceiverTorsoRigidBodyIndex;
	// oTrackerInfo["TrackedSoundReceiverTranslation"] = pConnection->pVAMatlabTracker->vTrackedSoundReceiverTranslation;
	// oTrackerInfo["TrackedSoundReceiverRotation"] = pConnection->pVAMatlabTracker->qTrackedSoundReceiverRotation;

	// Real world sound receiver
	oTrackerInfo["TrackedRealWorldSoundReceiverID"]                  = pConnection->pVAMatlabTracker->iTrackedRealWorldSoundReceiverID;
	oTrackerInfo["TrackedRealWorldSoundReceiverHeadRigidBodyIndex"]  = pConnection->pVAMatlabTracker->iTrackedRealWorldSoundReceiverHeadRigidBodyIndex;
	oTrackerInfo["TrackedRealWorldSoundReceiverTorsoRigidBodyIndex"] = pConnection->pVAMatlabTracker->iTrackedRealWorldSoundReceiverTorsoRigidBodyIndex;

	// Sound source
	oTrackerInfo["TrackedSoundSourceID"]             = pConnection->pVAMatlabTracker->iTrackedSoundSourceID;
	oTrackerInfo["TrackedSoundSourceRigidBodyIndex"] = pConnection->pVAMatlabTracker->iTrackedSoundSourceRigidBodyIndex;

	plhs[0] = matlabCreateStruct( oTrackerInfo );
}


REGISTER_PRIVATE_FUNCTION( get_server_state );
void get_server_state( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 1 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	plhs[0] = matlabCreateID( pConnection->pCoreInterface->GetState( ) );
}

// ------------------------------------------------------------

#ifdef VAMATLAB_INTEGRATED

REGISTER_PUBLIC_FUNCTION( reset, "Resets the VA server", "" );
void reset( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 1 );

	INFO( "Resetting VA core\n" );

	g_pOwnCore->Reset( );
}

#else // VAMATLAB_INTEGRATED

REGISTER_PUBLIC_FUNCTION( reset, "Resets the VA server", "" );
void reset( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 1 );

	ConnectionHandle hHandle = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	INFO( "Resetting VA server \"%s\" (connection handle: %d)\n", pConnection->pClient->GetServerAddress( ).c_str( ), hHandle );

	pConnection->pCoreInterface->Reset( );
}

#endif // VAMATLAB_INTEGRATED

REGISTER_PUBLIC_FUNCTION( get_modules, "Enumerates internal modules of the VA server", "" );
DECLARE_FUNCTION_OUTARG( get_modules, modules, "cell-array of struct-1x1", "Module informations (names, descriptions, etc.)" );
void get_modules( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 1 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::vector<CVAModuleInfo> v;
	pConnection->pCoreInterface->GetModules( v );

	const size_t nDims           = int( v.size( ) );
	const int nFields            = 2;
	const char *ppszFieldNames[] = { "name", "desc" };
	plhs[0]                      = mxCreateStructArray( 1, &nDims, nFields, ppszFieldNames );

	for( size_t i = 0; i < nDims; i++ )
	{
		mxSetField( plhs[0], i, ppszFieldNames[0], mxCreateString( v[i].sName.c_str( ) ) );
		mxSetField( plhs[0], i, ppszFieldNames[1], mxCreateString( v[i].sDesc.c_str( ) ) );
	}

	return;
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( call_module, "Calls an internal module of the VA server", "" );
DECLARE_FUNCTION_REQUIRED_INARG( call_module, module, "string", "Module name" );
DECLARE_FUNCTION_REQUIRED_INARG( call_module, mstruct, "struct", "Matlab structure with key-value content" );
DECLARE_FUNCTION_OUTARG( call_module, ret, "struct-1x1", "Struct containing the return values" );
void call_module( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	if( nrhs != 3 )
		VA_EXCEPT2( INVALID_PARAMETER, "This function takes three arguments (connection, module, params)" );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::string sModuleName = matlabGetString( prhs[1], "module" );
	CVAStruct oArgs         = matlabGetStruct( prhs[2], "mstruct" );

	CVAStruct oReturn;

	oReturn = pConnection->pCoreInterface->CallModule( sModuleName, oArgs );

	if( nlhs == 0 )
		return;

	if( nlhs == 1 )
	{
		mxArray *pReturnArray = matlabCreateStruct( oReturn );
		plhs[0]               = pReturnArray;
	}
	else
	{
		VA_EXCEPT2( INVALID_PARAMETER, "Too many left-hand-side return arguments given, this function returns a single argument (as struct) or nothing" );
	}

	return;
}

REGISTER_PUBLIC_FUNCTION( add_search_path, "adds a search path at core side", "" );
DECLARE_FUNCTION_REQUIRED_INARG( add_search_path, path, "string", "Relative or absolute path" );
DECLARE_FUNCTION_OUTARG( add_search_path, valid, "logical scalar", "True, if path at core side valid" );
void add_search_path( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::string sPath = matlabGetString( prhs[1], "path" );
	bool bValid       = pConnection->pCoreInterface->AddSearchPath( sPath );
	plhs[0]           = mxCreateLogicalScalar( bValid );

	return;
}

REGISTER_PUBLIC_FUNCTION( create_directivity_from_file, "Loads a directivity from a file", "" );
DECLARE_FUNCTION_REQUIRED_INARG( create_directivity_from_file, filename, "string", "Filename" );
DECLARE_FUNCTION_OPTIONAL_INARG( create_directivity_from_file, name, "string", "Displayed name", "''" );
DECLARE_FUNCTION_OUTARG( create_directivity_from_file, directivityID, "integer-1x1", "Directivity ID" );
void create_directivity_from_file( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::string sFilename = matlabGetString( prhs[1], "filename" );
	std::string sName     = matlabGetString( prhs[2], "Name" );

	int iDirectivityID = pConnection->pCoreInterface->CreateDirectivityFromFile( sFilename, sName );
	plhs[0]            = matlabCreateID( iDirectivityID );

	INFO( "Directivity \"%s\" successfully loaded (id: %d)\n", sFilename.c_str( ), iDirectivityID );
}

REGISTER_PUBLIC_FUNCTION( create_directivity_from_parameters, "Creates a directivity based on given parameters", "" );
DECLARE_FUNCTION_REQUIRED_INARG( create_directivity_from_parameters, directivity_args, "struct", "Directivity arguments" );
DECLARE_FUNCTION_OPTIONAL_INARG( create_directivity_from_parameters, name, "string", "Displayed name", "''" );
DECLARE_FUNCTION_OUTARG( create_directivity_from_parameters, directivityID, "integer-1x1", "Directivity ID" );
void create_directivity_from_parameters( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	CVAStruct oDirectivityArgs = matlabGetStruct( prhs[1], "directivity_args" );
	std::string sName          = matlabGetString( prhs[2], "Name" );

	int iDirectivityID = pConnection->pCoreInterface->CreateDirectivityFromParameters( oDirectivityArgs, sName );
	plhs[0]            = matlabCreateID( iDirectivityID );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( delete_directivity, "Frees a directivity and releases its memory",
                          "This is only possible if the directivity is not in use. Otherwise the method will do nothing." );
DECLARE_FUNCTION_REQUIRED_INARG( delete_directivity, directivityID, "integer-1x1", "Directivity ID" );
DECLARE_FUNCTION_OUTARG( delete_directivity, result, "logical-1x1", "Directivity freed?" );
void delete_directivity( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int iDirectivityID = matlabGetIntegerScalar( prhs[1], "directivityID" );

	bool bResult = pConnection->pCoreInterface->DeleteDirectivity( iDirectivityID );
	plhs[0]      = mxCreateLogicalScalar( bResult );
}

REGISTER_PUBLIC_FUNCTION( get_directivity_info, "Returns information on a loaded directivity", "" );
DECLARE_FUNCTION_REQUIRED_INARG( get_directivity_info, directivityID, "integer-1x1", "Directivity ID" );
DECLARE_FUNCTION_OUTARG( get_directivity_info, info, "struct-1x1", "Information struct (name, filename, resolution, etc.)" );
void get_directivity_info( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int iDirectivityID    = matlabGetIntegerScalar( prhs[1], "directivityID" );
	CVADirectivityInfo di = pConnection->pCoreInterface->GetDirectivityInfo( iDirectivityID );
	plhs[0]               = matlabCreateDirectivityInfo( di );
}

REGISTER_PUBLIC_FUNCTION( get_directivity_infos, "Returns information on all loaded directivities", "" );
DECLARE_FUNCTION_OUTARG( get_directivity_infos, info, "cell-array of struct-1x1", "Information structs (name, filename, resolution, etc.)" );
void get_directivity_infos( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 1 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::vector<CVADirectivityInfo> v;
	pConnection->pCoreInterface->GetDirectivityInfos( v );

	const mwSize dims[2] = { 1, v.size( ) };
	mxArray *pCell       = mxCreateCellArray( 2, dims );
	for( size_t i = 0; i < v.size( ); i++ )
		mxSetCell( pCell, i, matlabCreateDirectivityInfo( v[i] ) );

	plhs[0] = pCell;
}

REGISTER_PUBLIC_FUNCTION( create_scene, "Creates a scene from a file resource",
                          "Note: this is a loose method with no clear definition. It may be interpreted differently among renderers." );
DECLARE_FUNCTION_REQUIRED_INARG( create_scene, params, "string", "Parameter struct" );
DECLARE_FUNCTION_OPTIONAL_INARG( create_scene, scene_name, "string", "Scene name", "''" );
DECLARE_FUNCTION_OUTARG( create_scene, scene_id, "string", "Scene identifier" );
void create_scene( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	CVAStruct oParams = matlabGetStruct( prhs[1], "params" );
	std::string sName = matlabGetString( prhs[2], "name" );

	std::string sID = pConnection->pCoreInterface->CreateScene( oParams, sName );
	plhs[0]         = mxCreateString( sID.c_str( ) );
}


REGISTER_PUBLIC_FUNCTION( create_signal_source_buffer_from_file, "Creates a signal source which plays an audiofile",
                          "Note: The audiofile must be mono and its sampling rate must match that of the server." );
DECLARE_FUNCTION_REQUIRED_INARG( create_signal_source_buffer_from_file, filename, "string", "Filename" );
DECLARE_FUNCTION_OPTIONAL_INARG( create_signal_source_buffer_from_file, name, "string", "Displayed name", "''" );
DECLARE_FUNCTION_OUTARG( create_signal_source_buffer_from_file, signalSourceID, "string", "Signal source ID" );
void create_signal_source_buffer_from_file( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::string sFilename = matlabGetString( prhs[1], "filename" );
	std::string sName     = matlabGetString( prhs[2], "name" );

	std::string sID = pConnection->pCoreInterface->CreateSignalSourceBufferFromFile( sFilename, sName );
	plhs[0]         = mxCreateString( sID.c_str( ) );
}

REGISTER_PUBLIC_FUNCTION( create_signal_source_text_to_speech, "Creates a text to speech signal",
                          "Note: depending on mode, either Speech WAV files are generated and then played back, or TTS sentence is directly played back." );
DECLARE_FUNCTION_OPTIONAL_INARG( create_signal_source_text_to_speech, name, "string", "Displayed name", "''" );
DECLARE_FUNCTION_OUTARG( create_signal_source_text_to_speech, signalSourceID, "string", "Signal source ID" );
void create_signal_source_text_to_speech( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];
	std::string sName                = matlabGetString( prhs[1], "name" );
	std::string sID                  = pConnection->pCoreInterface->CreateSignalSourceTextToSpeech( sName );
	plhs[0]                          = mxCreateString( sID.c_str( ) );
}

REGISTER_PUBLIC_FUNCTION( create_signal_source_buffer_from_parameters, "Creates an buffer signal source", "" );
DECLARE_FUNCTION_REQUIRED_INARG( create_signal_source_buffer_from_parameters, params, "struct", "Parameters" );
DECLARE_FUNCTION_OPTIONAL_INARG( create_signal_source_buffer_from_parameters, name, "string", "Displayed name", "''" );
DECLARE_FUNCTION_OUTARG( create_signal_source_buffer_from_parameters, signalSourceID, "string", "Signal source ID" );
void create_signal_source_buffer_from_parameters( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	CVAStruct oParams = matlabGetStruct( prhs[1], "params" );
	std::string sName = matlabGetString( prhs[2], "name" );

	std::string sID = pConnection->pCoreInterface->CreateSignalSourceBufferFromParameters( oParams, sName );
	plhs[0]         = mxCreateString( sID.c_str( ) );
}

REGISTER_PUBLIC_FUNCTION( create_signal_source_engine, "Creates an engine signal source", "" );
DECLARE_FUNCTION_REQUIRED_INARG( create_signal_source_engine, params, "struct", "Parameters" );
DECLARE_FUNCTION_OPTIONAL_INARG( create_signal_source_engine, name, "string", "Displayed name", "''" );
DECLARE_FUNCTION_OUTARG( create_signal_source_engine, signalSourceID, "string", "Signal source ID" );
void create_signal_source_engine( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	CVAStruct oParams = matlabGetStruct( prhs[1], "params" );
	std::string sName = matlabGetString( prhs[2], "name" );

	std::string sID = pConnection->pCoreInterface->CreateSignalSourceEngine( oParams, sName );
	plhs[0]         = mxCreateString( sID.c_str( ) );
}

REGISTER_PUBLIC_FUNCTION( create_signal_source_sequencer, "Creates a sequencer signal source", "" );
DECLARE_FUNCTION_OPTIONAL_INARG( create_signal_source_sequencer, name, "string", "Displayed name", "''" );
DECLARE_FUNCTION_OUTARG( create_signal_source_sequencer, signalSourceID, "string", "Signal source ID" );
void create_signal_source_sequencer( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::string sName = matlabGetString( prhs[1], "name" );

	std::string sID = pConnection->pCoreInterface->CreateSignalSourceSequencer( sName );
	plhs[0]         = mxCreateString( sID.c_str( ) );
}

REGISTER_PUBLIC_FUNCTION( create_signal_source_network_stream, "Creates a signal source which receives audio samples via network", "" );
DECLARE_FUNCTION_REQUIRED_INARG( create_signal_source_network_stream, address, "string", "Hostname or IP address of the audio streaming server" );
DECLARE_FUNCTION_REQUIRED_INARG( create_signal_source_network_stream, port, "integer-1x1", "Server port" );
DECLARE_FUNCTION_OPTIONAL_INARG( create_signal_source_network_stream, name, "string", "Displayed name", "''" );
DECLARE_FUNCTION_OUTARG( create_signal_source_network_stream, signalSourceID, "string", "Signal source ID" );
void create_signal_source_network_stream( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 4 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::string sAddress = matlabGetString( prhs[1], "address" );
	int iPort            = matlabGetIntegerScalar( prhs[2], "port" );
	std::string sName    = matlabGetString( prhs[3], "name" );

	std::string sID = pConnection->pCoreInterface->CreateSignalSourceNetworkStream( sAddress, iPort, sName );
	plhs[0]         = mxCreateString( sID.c_str( ) );
}

REGISTER_PUBLIC_FUNCTION( create_signal_source_prototype_from_parameters, "Creates a prototype signal source, provide 'class' parameter!", "" );
DECLARE_FUNCTION_REQUIRED_INARG( create_signal_source_prototype_from_parameters, params, "struct", "Parameters" );
DECLARE_FUNCTION_OPTIONAL_INARG( create_signal_source_prototype_from_parameters, name, "string", "Displayed name", "''" );
DECLARE_FUNCTION_OUTARG( create_signal_source_prototype_from_parameters, signalSourceID, "string", "Signal source ID" );
void create_signal_source_prototype_from_parameters( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	CVAStruct oParams = matlabGetStruct( prhs[1], "params" );
	std::string sName = matlabGetString( prhs[2], "name" );

	std::string sID = pConnection->pCoreInterface->CreateSignalSourcePrototypeFromParameters( oParams, sName );
	plhs[0]         = mxCreateString( sID.c_str( ) );
}


// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( delete_signal_source, "Deletes a signal source", "A signal source can only be deleted, if it is not in use." );
DECLARE_FUNCTION_REQUIRED_INARG( delete_signal_source, signalSourceID, "string", "Signal source ID" );
DECLARE_FUNCTION_OUTARG( delete_signal_source, result, "logical-1x1", "Signal source deleted?" );

void delete_signal_source( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::string sID = matlabGetString( prhs[1], "signalSourceID" );

	bool bResult = pConnection->pCoreInterface->DeleteSignalSource( sID );
	plhs[0]      = mxCreateLogicalScalar( bResult );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( get_signal_source_info, "Returns information on signal source", "" );
DECLARE_FUNCTION_REQUIRED_INARG( get_signal_source_info, signalSourceID, "string", "Signal source ID" );
DECLARE_FUNCTION_OUTARG( get_signal_source_info, info, "struct-1x1", "Information structs (id, name, type, etc.)" );

void get_signal_source_info( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::string iSourceID   = matlabGetString( prhs[1], "signalSourceID" );
	CVASignalSourceInfo ssi = pConnection->pCoreInterface->GetSignalSourceInfo( iSourceID );
	plhs[0]                 = matlabCreateSignalSourceInfo( ssi );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( get_signal_source_infos, "Returns information on all existing signal sources", "" );
DECLARE_FUNCTION_OUTARG( get_signal_source_infos, info, "cell-array of struct-1x1", "Information structs (id, name, type, etc.)" );

void get_signal_source_infos( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 1 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::vector<CVASignalSourceInfo> v;
	pConnection->pCoreInterface->GetSignalSourceInfos( v );

	const mwSize dims[2] = { 1, v.size( ) };
	mxArray *pCell       = mxCreateCellArray( 2, dims );
	for( size_t i = 0; i < v.size( ); i++ )
		mxSetCell( pCell, i, matlabCreateSignalSourceInfo( v[i] ) );

	plhs[0] = pCell;
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( get_signal_source_buffer_playback_state, "Returns the playback state of an audiofile signal source. Available modes: PLAYING, STOPPED, PAUSED",
                          "" );
DECLARE_FUNCTION_REQUIRED_INARG( get_signal_source_buffer_playback_state, signalSourceID, "string", "Signal source ID" );
DECLARE_FUNCTION_OUTARG( get_signal_source_buffer_playback_state, playState, "string", "Playback state" );

void get_signal_source_buffer_playback_state( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );
	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];
	std::string sSignalSourceID      = matlabGetString( prhs[1], "signalSourceID" );
	int iPlayState                   = pConnection->pCoreInterface->GetSignalSourceBufferPlaybackState( sSignalSourceID );
	std::string sPlayState           = pConnection->pCoreInterface->GetPlaybackStateStr( iPlayState );
	plhs[0]                          = mxCreateString( sPlayState.c_str( ) );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( set_signal_source_buffer_playback_action, "Change the playback state of an audiofile signal source. Available actions: PLAY STOP PAUSE", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_signal_source_buffer_playback_action, signalSourceID, "string", "Signal source ID" );
DECLARE_FUNCTION_REQUIRED_INARG( set_signal_source_buffer_playback_action, playAction, "string", "Playback action" );
void set_signal_source_buffer_playback_action( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );
	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::string sSignalSourceID = matlabGetString( prhs[1], "signalSourceID" );
	std::string sPlayAction     = matlabGetString( prhs[2], "playAction" );
	int iPlayAction             = IVAInterface::ParsePlaybackAction( sPlayAction );
	pConnection->pCoreInterface->SetSignalSourceBufferPlaybackAction( sSignalSourceID, iPlayAction );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( set_signal_source_buffer_looping, "Change the playback state of an audiofile signal source. Available actions: PLAY STOP PAUSE", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_signal_source_buffer_looping, signalSourceID, "string", "Signal source ID" );
DECLARE_FUNCTION_REQUIRED_INARG( set_signal_source_buffer_looping, isLooping, "logical", "Set looping enabled/disabled" );
void set_signal_source_buffer_looping( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );
	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::string sSignalSourceID = matlabGetString( prhs[1], "signalSourceID" );
	bool bIsLooping             = matlabGetBoolScalar( prhs[2], "isLooping" );
	pConnection->pCoreInterface->SetSignalSourceBufferLooping( sSignalSourceID, bIsLooping );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( get_signal_source_buffer_looping, "Returns the playback state of an audiofile signal source. Available modes: PLAYING, STOPPED, PAUSED", "" );
DECLARE_FUNCTION_REQUIRED_INARG( get_signal_source_buffer_looping, signalSourceID, "string", "Signal source ID" );
DECLARE_FUNCTION_OUTARG( get_signal_source_buffer_looping, isLooping, "logical-1x1", "Looping enabled/disabled" );

void get_signal_source_buffer_looping( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );
	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];
	std::string sSignalSourceID      = matlabGetString( prhs[1], "signalSourceID" );
	bool bIsLooping                  = pConnection->pCoreInterface->GetSignalSourceBufferLooping( sSignalSourceID );
	plhs[0]                          = mxCreateLogicalScalar( bIsLooping );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( set_signal_source_buffer_playback_position, "Sets the playback position of an audiofile signal source.", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_signal_source_buffer_playback_position, signalSourceID, "string", "Signal source ID" );
DECLARE_FUNCTION_REQUIRED_INARG( set_signal_source_buffer_playback_position, playPosition, "scalar", "Playback position [s]" );

void set_signal_source_buffer_playback_position( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::string sSignalSourceID = matlabGetString( prhs[1], "signalSourceID" );
	double dPlaybackPosition    = matlabGetRealScalar( prhs[2], "playPosition" );

	pConnection->pCoreInterface->SetSignalSourceBufferPlaybackPosition( sSignalSourceID, dPlaybackPosition );
}


// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( get_signal_source_parameters, "Returns the current signal source parameters", "" );
DECLARE_FUNCTION_REQUIRED_INARG( get_signal_source_parameters, ID, "string", "Signal source identifier" );
DECLARE_FUNCTION_REQUIRED_INARG( get_signal_source_parameters, args, "mstruct", "Requested parameters" );
DECLARE_FUNCTION_OUTARG( get_signal_source_parameters, params, "mstruct", "Parameters" );
void get_signal_source_parameters( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::string sID = matlabGetString( prhs[1], "ID" );
	CVAStruct oArgs = matlabGetStruct( prhs[2], "args" );
	CVAStruct oRet  = pConnection->pCoreInterface->GetSignalSourceParameters( sID, oArgs );
	plhs[0]         = matlabCreateStruct( oRet );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( set_signal_source_parameters, "Sets signal source parameters", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_signal_source_parameters, ID, "string", "Signal source identifier" );
DECLARE_FUNCTION_REQUIRED_INARG( set_signal_source_parameters, params, "mstruct", "Parameters" );
void set_signal_source_parameters( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::string sID   = matlabGetString( prhs[1], "ID" );
	CVAStruct oParams = matlabGetStruct( prhs[2], "params" );
	pConnection->pCoreInterface->SetSignalSourceParameters( sID, oParams );
}


/* +----------------------------------------------------------+ *
 * |                                                          | *
 * |   Synchronization functions                              | *
 * |                                                          | *
 * +----------------------------------------------------------+ */

REGISTER_PUBLIC_FUNCTION( get_update_locked, "Is scene locked?", "" );
DECLARE_FUNCTION_OUTARG( get_update_locked, result, "logical-1x1", "true, if within locked (synchronized) scene modification" );

void get_update_locked( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 1 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	bool bResult = pConnection->pCoreInterface->GetUpdateLocked( );
	plhs[0]      = mxCreateLogicalScalar( bResult );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( lock_update, "Locks the scene (modifications of scene can be applied synchronously)",
                          "During a locked scene, no changes are directly applied. After unlocking, all modifications are syncrhonously applied." );
void lock_update( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 1 );
	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];
	pConnection->pCoreInterface->LockUpdate( );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( unlock_update, "Unlocks scene and applied synchronously modifications made",
                          "returns state ID of the scene if successfully proceeded, -1 if not" );
DECLARE_FUNCTION_OUTARG( unlock_update, newStateID, "integer-1x1", "" );
void unlock_update( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 1 );
	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];
	int iNewStateID                  = pConnection->pCoreInterface->UnlockUpdate( );
	plhs[0]                          = matlabCreateID( iNewStateID );
}

/* +----------------------------------------------------------+ *
 * |                                                          | *
 * |   Sound sources                                          | *
 * |                                                          | *
 * +----------------------------------------------------------+ */

REGISTER_PUBLIC_FUNCTION( get_sound_source_ids, "Returns the IDs of all sound sources in the scene", "" );
DECLARE_FUNCTION_OUTARG( get_sound_source_ids, ids, "integer-1xN", "Vector containing the IDs" );

void get_sound_source_ids( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 1 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::vector<int> v;
	pConnection->pCoreInterface->GetSoundSourceIDs( v );
	plhs[0] = matlabCreateIDList( v );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( create_sound_source, "Creates a sound source", "" );
DECLARE_FUNCTION_OPTIONAL_INARG( create_sound_source, name, "string", "Displayed name", "''" );
DECLARE_FUNCTION_OUTARG( create_sound_source, id, "integer-1x1", "Sound source ID" );

void create_sound_source( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	const std::string sName = matlabGetString( prhs[1], "name" );
	const int iSourceID     = pConnection->pCoreInterface->CreateSoundSource( sName );
	plhs[0]                 = matlabCreateID( iSourceID );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( create_sound_source_explicit_renderer, "Creates a sound source explicitly for a certain renderer", "" );
DECLARE_FUNCTION_REQUIRED_INARG( create_sound_source_explicit_renderer, renderer, "string", "Renderer identifier" );
DECLARE_FUNCTION_REQUIRED_INARG( create_sound_source_explicit_renderer, name, "string", "Name" );
DECLARE_FUNCTION_OUTARG( create_sound_source_explicit_renderer, id, "integer-1x1", "Sound source ID" );
void create_sound_source_explicit_renderer( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::string sRendererID = matlabGetString( prhs[1], "renderer" );
	std::string sName       = matlabGetString( prhs[2], "name" );

	int iSourceID = pConnection->pCoreInterface->CreateSoundSourceExplicitRenderer( sRendererID, sName );
	plhs[0]       = matlabCreateID( iSourceID );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( delete_sound_source, "Deletes an existing sound source in the scene", "" );
DECLARE_FUNCTION_REQUIRED_INARG( delete_sound_source, soundSourceID, "integer-1x1", "Sound source ID" );

void delete_sound_source( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int iSoundSourceID = matlabGetIntegerScalar( prhs[1], "soundSourceID" );

	pConnection->pCoreInterface->DeleteSoundSource( iSoundSourceID );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( get_sound_source_name, "Returns name of a sound source", "" );
DECLARE_FUNCTION_REQUIRED_INARG( get_sound_source_name, soundSourceID, "integer-1x1", "Sound source ID" );
DECLARE_FUNCTION_OUTARG( get_sound_source_name, name, "string", "Displayed name" );

void get_sound_source_name( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int iSoundSourceID = matlabGetIntegerScalar( prhs[1], "soundSourceID" );

	std::string sName = pConnection->pCoreInterface->GetSoundSourceName( iSoundSourceID );
	plhs[0]           = mxCreateString( sName.c_str( ) );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( set_sound_source_name, "Name", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_source_name, soundSourceID, "integer-1x1", "Sound source ID" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_source_name, name, "string", "Displayed name" );

void set_sound_source_name( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int iSoundSourceID = matlabGetIntegerScalar( prhs[1], "soundSourceID" );
	std::string sName  = matlabGetString( prhs[2], "name" );

	pConnection->pCoreInterface->SetSoundSourceName( iSoundSourceID, sName );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( get_sound_source_signal_source, "Returns for a sound source, the attached signal source",
                          "Note: This function returns an empty string, if no signal source is attached." );
DECLARE_FUNCTION_REQUIRED_INARG( get_sound_source_signal_source, soundSourceID, "integer-1x1", "Sound source ID" );
DECLARE_FUNCTION_OUTARG( get_sound_source_signal_source, signalSourceID, "string", "Signal source ID" );

void get_sound_source_signal_source( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int iSoundSourceID = matlabGetIntegerScalar( prhs[1], "soundSourceID" );

	std::string sSignalSourceID = pConnection->pCoreInterface->GetSoundSourceSignalSource( iSoundSourceID );
	plhs[0]                     = mxCreateString( sSignalSourceID.c_str( ) );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( set_sound_source_signal_source, "Sets the signal source of a sound source", "Note: Passing an empty string removes the signal source" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_source_signal_source, soundSourceID, "integer-1x1", "Sound source ID" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_source_signal_source, signalSourceID, "string", "Signal Source ID" );

void set_sound_source_signal_source( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int iSoundSourceID          = matlabGetIntegerScalar( prhs[1], "soundSourceID" );
	std::string sSignalSourceID = matlabGetString( prhs[2], "signalSourceID" );

	pConnection->pCoreInterface->SetSoundSourceSignalSource( iSoundSourceID, sSignalSourceID );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( remove_sound_source_signal_source, "Removes the signal source of a sound source", "Note: will note remove the signal source from manager" );
DECLARE_FUNCTION_REQUIRED_INARG( remove_sound_source_signal_source, ID, "integer-1x1", "Sound source identifier" );
void remove_sound_source_signal_source( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );
	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];
	int iID                          = matlabGetIntegerScalar( prhs[1], "soundSourceID" );
	pConnection->pCoreInterface->RemoveSoundSourceSignalSource( iID );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( get_sound_source_auralization_mode, "Returns the auralization mode of a sound source", "" );
DECLARE_FUNCTION_REQUIRED_INARG( get_sound_source_auralization_mode, soundSourceID, "integer-1x1", "Sound source ID" );
DECLARE_FUNCTION_OUTARG( get_sound_source_auralization_mode, auralizationMode, "string", "Auralization mode" );

void get_sound_source_auralization_mode( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int iSoundSourceID    = matlabGetIntegerScalar( prhs[1], "soundSourceID" );
	int iAuralizationMode = pConnection->pCoreInterface->GetSoundSourceAuralizationMode( iSoundSourceID );
	plhs[0]               = mxCreateString( IVAInterface::GetAuralizationModeStr( iAuralizationMode, true ).c_str( ) );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( set_sound_source_auralization_mode, "Returns the auralization mode of a sound source", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_source_auralization_mode, soundSourceID, "integer-1x1", "Sound source ID" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_source_auralization_mode, auralizationMode, "string", "Auralization mode" );

void set_sound_source_auralization_mode( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int iSoundSourceID            = matlabGetIntegerScalar( prhs[1], "soundSourceID" );
	std::string sAuralizationMode = matlabGetString( prhs[2], "auralizationMode" );

	// Get the current auralization mode first for computing relative modes (+|-)
	int iCurAuralizationMode = pConnection->pCoreInterface->GetSoundSourceAuralizationMode( iSoundSourceID );
	int iNewAuralizationMode = IVAInterface::ParseAuralizationModeStr( sAuralizationMode, iCurAuralizationMode );

	pConnection->pCoreInterface->SetSoundSourceAuralizationMode( iSoundSourceID, iNewAuralizationMode );
}

REGISTER_PUBLIC_FUNCTION( get_sound_source_parameters, "Returns the current sound source parameters", "" );
DECLARE_FUNCTION_REQUIRED_INARG( get_sound_source_parameters, ID, "integer-1x1", "Sound source identifier" );
DECLARE_FUNCTION_REQUIRED_INARG( get_sound_source_parameters, args, "mstruct", "Requested parameters" );
DECLARE_FUNCTION_OUTARG( get_sound_source_parameters, params, "mstruct", "Parameters" );
void get_sound_source_parameters( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int iID         = matlabGetIntegerScalar( prhs[1], "ID" );
	CVAStruct oArgs = matlabGetStruct( prhs[2], "args" );
	CVAStruct oRet  = pConnection->pCoreInterface->GetSoundSourceParameters( iID, oArgs );
	plhs[0]         = matlabCreateStruct( oRet );
}

REGISTER_PUBLIC_FUNCTION( set_sound_source_parameters, "Sets sound source parameters", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_source_parameters, ID, "integer-1x1", "Sound source identifier" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_source_parameters, params, "mstruct", "Parameters" );
void set_sound_source_parameters( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int iID           = matlabGetIntegerScalar( prhs[1], "ID" );
	CVAStruct oParams = matlabGetStruct( prhs[2], "params" );
	pConnection->pCoreInterface->SetSoundSourceParameters( iID, oParams );
}

REGISTER_PUBLIC_FUNCTION( get_sound_source_directivity, "Returns the directivity of a sound source",
                          "Note: If the sound source is not assigned a directivity, the methods returns -1." );
DECLARE_FUNCTION_REQUIRED_INARG( get_sound_source_directivity, soundSourceID, "integer-1x1", "Sound source ID" );
DECLARE_FUNCTION_OUTARG( get_sound_source_directivity, directivityID, "integer-1x1", "Directivity ID" );

void get_sound_source_directivity( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int iSoundSourceID = matlabGetIntegerScalar( prhs[1], "soundSourceID" );
	int iDirectivityID = pConnection->pCoreInterface->GetSoundSourceDirectivity( iSoundSourceID );

	plhs[0] = matlabCreateID( iDirectivityID );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( set_sound_source_directivity, "Sets the directivity of a sound source",
                          "Note: In order to set no directivity (omnidirectional source), you can pass -1 to the method." );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_source_directivity, soundSourceID, "integer-1x1", "Sound source ID" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_source_directivity, directivityID, "integer-1x1", "Directivity ID" );

void set_sound_source_directivity( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int iSoundSourceID = matlabGetIntegerScalar( prhs[1], "soundSourceID" );
	int iDirectivityID = matlabGetIntegerScalar( prhs[2], "directivityID" );

	pConnection->pCoreInterface->SetSoundSourceDirectivity( iSoundSourceID, iDirectivityID );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( get_sound_source_sound_power, "Returns the volume of a sound source",
                          "Note: The method returns the sound power in Watts, no Decibel values!)" );
DECLARE_FUNCTION_REQUIRED_INARG( get_sound_source_sound_power, soundSourceID, "integer-1x1", "Sound source ID" );
DECLARE_FUNCTION_OUTARG( get_sound_source_sound_power, volume, "double-1x1", "Sound source power" );

void get_sound_source_sound_power( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int iSoundSourceID = matlabGetIntegerScalar( prhs[1], "soundSourceID" );

	double dVolume = pConnection->pCoreInterface->GetSoundSourceSoundPower( iSoundSourceID );
	plhs[0]        = mxCreateDoubleScalar( dVolume );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( set_sound_source_sound_power, "Sets the volume of a sound source", "Note: The sound power is defined in Watts (no decibel)" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_source_sound_power, soundSourceID, "integer-1x1", "Sound source ID" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_source_sound_power, soundpower, "double-1x1", "Sound power" );

void set_sound_source_sound_power( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int iSoundSourceID = matlabGetIntegerScalar( prhs[1], "soundSourceID" );
	double dPower      = matlabGetRealScalar( prhs[2], "soundpower" );

	// TODO: Erweiterte Eingaben. Z.b. mit "+3db"
	pConnection->pCoreInterface->SetSoundSourceSoundPower( iSoundSourceID, dPower );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( get_sound_source_muted, "Returns if a sound source is muted", "" );
DECLARE_FUNCTION_REQUIRED_INARG( get_sound_source_muted, soundSourceID, "integer-1x1", "Sound source ID" );
DECLARE_FUNCTION_OUTARG( get_sound_source_muted, result, "logical-1x1", "Muted?" );

void get_sound_source_muted( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int iSoundSourceID = matlabGetIntegerScalar( prhs[1], "soundSourceID" );

	bool bResult = pConnection->pCoreInterface->GetSoundSourceMuted( iSoundSourceID );
	plhs[0]      = mxCreateLogicalScalar( bResult );
}


REGISTER_PUBLIC_FUNCTION( set_sound_source_muted, "Sets a sound source muted or unmuted", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_source_muted, soundSourceID, "integer-1x1", "Sound source ID" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_source_muted, muted, "logical-1x1", "Muted?" );

void set_sound_source_muted( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int iSoundSourceID = matlabGetIntegerScalar( prhs[1], "soundSourceID" );
	bool bMuted        = matlabGetBoolScalar( prhs[2], "muted" );

	pConnection->pCoreInterface->SetSoundSourceMuted( iSoundSourceID, bMuted );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( get_sound_source_position, "Returns the position of a sound source", "" );
DECLARE_FUNCTION_REQUIRED_INARG( get_sound_source_position, soundSourceID, "integer-1x1", "Sound source ID" );
DECLARE_FUNCTION_OUTARG( get_sound_source_position, pos, "double-3", "Position vector [x,y,z] (unit: meters)" );

void get_sound_source_position( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int iSoundSourceID = matlabGetIntegerScalar( prhs[1], "soundSourceID" );

	VAVec3 v3Pos = pConnection->pCoreInterface->GetSoundSourcePosition( iSoundSourceID );
	plhs[0]      = matlabCreateRealVector3( v3Pos );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( get_sound_source_orientation, "Returns the orientation of a sound source", "" );
DECLARE_FUNCTION_REQUIRED_INARG( get_sound_source_orientation, soundSourceID, "integer-1x1", "Sound source ID" );
DECLARE_FUNCTION_OUTARG( get_sound_source_orientation, orient, "double-4", "Rotation as quaternion (w,x,y,z)" );

void get_sound_source_orientation( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int iSoundSourceID = matlabGetIntegerScalar( prhs[1], "soundSourceID" );
	VAQuat qOrient     = pConnection->pCoreInterface->GetSoundSourceOrientation( iSoundSourceID );
	plhs[0]            = matlabCreateQuaternion( qOrient );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( get_sound_source_orientation_view_up, "Returns the orientation of a sound source as view- and up-vector", "" );
DECLARE_FUNCTION_REQUIRED_INARG( get_sound_source_orientation_view_up, soundSourceID, "integer-1x1", "Sound source ID" );
DECLARE_FUNCTION_OUTARG( get_sound_source_orientation_view_up, view, "double-3", "View vector (length: 1)" );
DECLARE_FUNCTION_OUTARG( get_sound_source_orientation_view_up, up, "double-3", "Up vector (length: 1)" );

void get_sound_source_orientation_view_up( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int iSoundSourceID = matlabGetIntegerScalar( prhs[1], "soundSourceID" );

	VAVec3 v3View, v3Up;
	pConnection->pCoreInterface->GetSoundSourceOrientationVU( iSoundSourceID, v3View, v3Up );
	plhs[0] = matlabCreateRealVector3( v3View );
	plhs[1] = matlabCreateRealVector3( v3Up );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( get_sound_source_pose, "Returns the position and orientation of a sound source", "" );
DECLARE_FUNCTION_REQUIRED_INARG( get_sound_source_pose, soundSourceID, "integer-1x1", "Sound source ID" );
DECLARE_FUNCTION_OUTARG( get_sound_source_pose, pos, "double-3", "Position vector [x,y,z] (unit: meters)" );
DECLARE_FUNCTION_OUTARG( get_sound_source_pose, orient, "double-4", "Rotation quaternion (w,x,y,z)" );
void get_sound_source_pose( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int iSoundSourceID = matlabGetIntegerScalar( prhs[1], "soundSourceID" );

	VAVec3 v3Pos;
	VAQuat qOrient;
	pConnection->pCoreInterface->GetSoundSourcePose( iSoundSourceID, v3Pos, qOrient );
	plhs[0] = matlabCreateRealVector3( v3Pos );
	plhs[1] = matlabCreateQuaternion( qOrient );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( set_sound_source_position, "Sets the position of a sound source", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_source_position, id, "integer-1x1", "Sound source ID" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_source_position, pos, "double-3", "Position vector [x,y,z] (unit: meters)" );

void set_sound_source_position( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int iSourceID = matlabGetIntegerScalar( prhs[1], "id" );
	VAVec3 v3Pos;
	matlabGetRealVector3( prhs[2], "pos", v3Pos );

	pConnection->pCoreInterface->SetSoundSourcePosition( iSourceID, v3Pos );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( set_sound_source_orientation, "Sets the orientation of a sound source", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_source_orientation, soundSourceID, "integer-1x1", "Sound source ID" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_source_orientation, orient, "double-4", "Rotation quaterion [w,x,y,z]" );

void set_sound_source_orientation( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int iSoundSourceID = matlabGetIntegerScalar( prhs[1], "soundSourceID" );
	VAQuat qOrient;
	matlabGetQuaternion( prhs[2], "orient", qOrient );

	pConnection->pCoreInterface->SetSoundSourceOrientation( iSoundSourceID, qOrient );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( set_sound_source_orientation_view_up, "Sets the orientation of a sound source (as view- and up-vector)",
                          "Note: The view and up vector must be an orthonormal pair of vectors." );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_source_orientation_view_up, soundSourceID, "integer-1x1", "Sound source ID" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_source_orientation_view_up, view, "double-3", "View vector" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_source_orientation_view_up, up, "double-3", "Up vector" );

void set_sound_source_orientation_view_up( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 4 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int iSoundSourceID = matlabGetIntegerScalar( prhs[1], "soundSourceID" );
	VAVec3 v3View, v3Up;
	matlabGetRealVector3( prhs[2], "view", v3View );
	matlabGetRealVector3( prhs[3], "up", v3Up );

	pConnection->pCoreInterface->SetSoundSourceOrientationVU( iSoundSourceID, v3View, v3Up );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( set_sound_source_pose, "Sets the position and orientation (in yaw, pitch, roll angles) of a sound source", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_source_pose, soundSourceID, "integer-1x1", "Sound source ID" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_source_pose, pos, "double-3", "Position vector [x, y, z] (unit: meters)" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_source_pose, orient, "double-3", "Rotation angles [q,x,y,z]" );

void set_sound_source_pose( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 4 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int iSoundSourceID = matlabGetIntegerScalar( prhs[1], "soundSourceID" );
	VAVec3 v3Pos;
	VAQuat qOrient;
	matlabGetRealVector3( prhs[2], "pos", v3Pos );
	matlabGetQuaternion( prhs[3], "orient", qOrient );

	pConnection->pCoreInterface->SetSoundSourcePose( iSoundSourceID, v3Pos, qOrient );
}


/* +----------------------------------------------------------+ *
 * |                                                          | *
 * |   Sound receivers                                        | *
 * |                                                          | *
 * +----------------------------------------------------------+ */

REGISTER_PUBLIC_FUNCTION( get_sound_receiver_ids, "Returns the IDs of all sound receivers in the scene", "" );
DECLARE_FUNCTION_OUTARG( get_sound_receiver_ids, ids, "integer-1xN", "Vector containing the IDs" );
void get_sound_receiver_ids( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 1 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::vector<int> v;
	pConnection->pCoreInterface->GetSoundReceiverIDs( v );
	plhs[0] = matlabCreateIDList( v );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( create_sound_receiver, "Creates a sound receiver", "" );
DECLARE_FUNCTION_OPTIONAL_INARG( create_sound_receiver, name, "string", "Displayed name", "''" );
DECLARE_FUNCTION_OUTARG( create_sound_receiver, id, "integer-1x1", "Sound receiver ID" );
void create_sound_receiver( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::string sName = matlabGetString( prhs[1], "name" );

	int isoundreceiverID = pConnection->pCoreInterface->CreateSoundReceiver( sName );

	plhs[0] = matlabCreateID( isoundreceiverID );
}

REGISTER_PUBLIC_FUNCTION( create_sound_receiver_explicit_renderer, "Creates a sound receiver explicitly for a certain renderer", "" );
DECLARE_FUNCTION_REQUIRED_INARG( create_sound_receiver_explicit_renderer, renderer, "string", "Renderer identifier" );
DECLARE_FUNCTION_REQUIRED_INARG( create_sound_receiver_explicit_renderer, name, "string", "Name" );
DECLARE_FUNCTION_OUTARG( create_sound_receiver_explicit_renderer, id, "integer-1x1", "Sound receiver ID" );
void create_sound_receiver_explicit_renderer( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::string sRendererID = matlabGetString( prhs[1], "renderer" );
	std::string sName       = matlabGetString( prhs[2], "name" );

	int iID = pConnection->pCoreInterface->CreateSoundReceiverExplicitRenderer( sRendererID, sName );
	plhs[0] = matlabCreateID( iID );
}

REGISTER_PUBLIC_FUNCTION( delete_sound_receiver, "Deletes a sound receiver from the scene", "Note: The active sound receiver cannot be deleted!" );
DECLARE_FUNCTION_REQUIRED_INARG( delete_sound_receiver, soundreceiverID, "integer-1x1", "Sound receiver ID" );
void delete_sound_receiver( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int isoundreceiverID = matlabGetIntegerScalar( prhs[1], "soundreceiverID" );
	pConnection->pCoreInterface->DeleteSoundReceiver( isoundreceiverID );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( get_sound_receiver_name, "Returns name of a sound receiver", "" );
DECLARE_FUNCTION_REQUIRED_INARG( get_sound_receiver_name, soundreceiverID, "integer-1x1", "Sound receiver ID" );
DECLARE_FUNCTION_OUTARG( get_sound_receiver_name, name, "string", "Displayed name" );

void get_sound_receiver_name( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int isoundreceiverID = matlabGetIntegerScalar( prhs[1], "soundreceiverID" );

	std::string sName = pConnection->pCoreInterface->GetSoundReceiverName( isoundreceiverID );
	plhs[0]           = mxCreateString( sName.c_str( ) );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( set_sound_receiver_name, "Sets the name of a sound receiver", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_receiver_name, soundreceiverID, "integer-1x1", "Sound receiver ID" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_receiver_name, name, "string", "Displayed name" );

void set_sound_receiver_name( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int isoundreceiverID = matlabGetIntegerScalar( prhs[1], "soundreceiverID" );
	std::string sName    = matlabGetString( prhs[2], "name" );

	pConnection->pCoreInterface->SetSoundReceiverName( isoundreceiverID, sName );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( get_sound_receiver_auralization_mode, "Returns the auralization mode of a sound receiver", "" );
DECLARE_FUNCTION_REQUIRED_INARG( get_sound_receiver_auralization_mode, soundreceiverID, "integer-1x1", "Sound receiver ID" );
DECLARE_FUNCTION_OUTARG( get_sound_receiver_auralization_mode, auralizationMode, "string", "Auralization mode" );

void get_sound_receiver_auralization_mode( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::vector<int> v;

	int isoundreceiverID = matlabGetIntegerScalar( prhs[1], "soundreceiverID" );

	int iAuralizationMode = pConnection->pCoreInterface->GetSoundReceiverAuralizationMode( isoundreceiverID );
	plhs[0]               = mxCreateString( IVAInterface::GetAuralizationModeStr( iAuralizationMode, true ).c_str( ) );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( set_sound_receiver_auralization_mode, "Sets the auralization mode of a sound receiver", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_receiver_auralization_mode, soundreceiverID, "integer-1x1", "Sound receiver ID" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_receiver_auralization_mode, auralizationMode, "string", "Auralization mode" );

void set_sound_receiver_auralization_mode( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int isoundreceiverID          = matlabGetIntegerScalar( prhs[1], "soundreceiverID" );
	std::string sAuralizationMode = matlabGetString( prhs[2], "auralizationMode" );

	// Get the current auralization mode first for computing relative modes (+|-)
	int iCurAuralizationMode = pConnection->pCoreInterface->GetSoundReceiverAuralizationMode( isoundreceiverID );
	int iNewAuralizationMode = IVAInterface::ParseAuralizationModeStr( sAuralizationMode, iCurAuralizationMode );

	pConnection->pCoreInterface->SetSoundReceiverAuralizationMode( isoundreceiverID, iNewAuralizationMode );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( get_sound_receiver_parameters, "Returns the current sound receiver parameters", "" );
DECLARE_FUNCTION_REQUIRED_INARG( get_sound_receiver_parameters, ID, "integer-1x1", "Sound receiver identifier" );
DECLARE_FUNCTION_REQUIRED_INARG( get_sound_receiver_parameters, args, "mstruct", "Requested parameters" );
DECLARE_FUNCTION_OUTARG( get_sound_receiver_parameters, params, "mstruct", "Parameters" );
void get_sound_receiver_parameters( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int iID         = matlabGetIntegerScalar( prhs[1], "ID" );
	CVAStruct oArgs = matlabGetStruct( prhs[2], "args" );
	CVAStruct oRet  = pConnection->pCoreInterface->GetSoundReceiverParameters( iID, oArgs );
	plhs[0]         = matlabCreateStruct( oRet );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( set_sound_receiver_parameters, "Sets sound receiver parameters", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_receiver_parameters, ID, "integer-1x1", "Sound receiver identifier" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_receiver_parameters, params, "mstruct", "Parameters" );
void set_sound_receiver_parameters( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int iID           = matlabGetIntegerScalar( prhs[1], "ID" );
	CVAStruct oParams = matlabGetStruct( prhs[2], "params" );
	pConnection->pCoreInterface->SetSoundReceiverParameters( iID, oParams );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( get_sound_receiver_directivity, "Returns for a sound receiver the assigned directivity", "" );
DECLARE_FUNCTION_REQUIRED_INARG( get_sound_receiver_directivity, soundreceiverID, "integer-1x1", "Sound receiver ID" );
DECLARE_FUNCTION_OUTARG( get_sound_receiver_directivity, directivityID, "integer-1x1", "Directivity ID" );

void get_sound_receiver_directivity( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int isoundreceiverID = matlabGetIntegerScalar( prhs[1], "soundreceiverID" );
	int idirectivityID   = pConnection->pCoreInterface->GetSoundReceiverDirectivity( isoundreceiverID );
	plhs[0]              = matlabCreateID( idirectivityID );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( set_sound_receiver_directivity, "Set the directivity of a sound receiver",
                          "Note: In order to set no HRIR dataset, you can pass -1 to the method. In this case the sound receiver will be silent." );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_receiver_directivity, soundreceiverID, "integer-1x1", "Sound receiver ID" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_receiver_directivity, directivityID, "integer-1x1", "HRIR dataset ID" );

void set_sound_receiver_directivity( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int isoundreceiverID = matlabGetIntegerScalar( prhs[1], "soundreceiverID" );
	int iDirectivityID   = matlabGetIntegerScalar( prhs[2], "directivityID" );

	pConnection->pCoreInterface->SetSoundReceiverDirectivity( isoundreceiverID, iDirectivityID );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( get_sound_receiver_muted, "Returns if a sound receiver is muted", "" );
DECLARE_FUNCTION_REQUIRED_INARG( get_sound_receiver_muted, soundReceiverID, "integer-1x1", "Sound receiver ID" );
DECLARE_FUNCTION_OUTARG( get_sound_receiver_muted, result, "logical-1x1", "Muted?" );

void get_sound_receiver_muted( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int iSoundReceiverID = matlabGetIntegerScalar( prhs[1], "soundReceiverID" );

	bool bResult = pConnection->pCoreInterface->GetSoundReceiverMuted( iSoundReceiverID );
	plhs[0]      = mxCreateLogicalScalar( bResult );
}


REGISTER_PUBLIC_FUNCTION( set_sound_receiver_muted, "Sets a sound receiver muted or unmuted", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_receiver_muted, soundReceiverID, "integer-1x1", "Sound receiver ID" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_receiver_muted, muted, "logical-1x1", "Muted?" );

void set_sound_receiver_muted( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int iSoundReceiverID = matlabGetIntegerScalar( prhs[1], "soundReceiverID" );
	bool bMuted          = matlabGetBoolScalar( prhs[2], "muted" );

	pConnection->pCoreInterface->SetSoundReceiverMuted( iSoundReceiverID, bMuted );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( get_sound_receiver_position, "Returns the position of a sound receiver", "" );
DECLARE_FUNCTION_REQUIRED_INARG( get_sound_receiver_position, soundreceiverID, "integer-1x1", "Sound receiver ID" );
DECLARE_FUNCTION_OUTARG( get_sound_receiver_position, pos, "double-3", "Position vector [x,y,z] (unit: meters)" );

void get_sound_receiver_position( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];


	int isoundreceiverID = matlabGetIntegerScalar( prhs[1], "soundreceiverID" );
	VAVec3 v3Pos         = pConnection->pCoreInterface->GetSoundReceiverPosition( isoundreceiverID );
	plhs[0]              = matlabCreateRealVector3( v3Pos );
}

REGISTER_PUBLIC_FUNCTION( get_sound_receiver_head_above_torso_orientation, "Returns the head-above-torso (relative) orientation of a sound receiver", "" );
DECLARE_FUNCTION_REQUIRED_INARG( get_sound_receiver_head_above_torso_orientation, soundreceiverID, "integer-1x1", "Sound receiver ID" );
DECLARE_FUNCTION_OUTARG( get_sound_receiver_head_above_torso_orientation, orient, "double-4", "Rotation angles [w,x,y,z]" );
void get_sound_receiver_head_above_torso_orientation( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int isoundreceiverID = matlabGetIntegerScalar( prhs[1], "soundreceiverID" );

	VAQuat qOrient = pConnection->pCoreInterface->GetSoundReceiverHeadAboveTorsoOrientation( isoundreceiverID );
	plhs[0]        = matlabCreateQuaternion( qOrient );
}

REGISTER_PUBLIC_FUNCTION( get_sound_receiver_orientation, "Returns the orientation of a sound receiver", "" );
DECLARE_FUNCTION_REQUIRED_INARG( get_sound_receiver_orientation, soundreceiverID, "integer-1x1", "Sound receiver ID" );
DECLARE_FUNCTION_OUTARG( get_sound_receiver_orientation, orient, "double-4", "Rotation angles [w,x,y,z]" );
void get_sound_receiver_orientation( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int isoundreceiverID = matlabGetIntegerScalar( prhs[1], "soundreceiverID" );

	VAQuat qOrient = pConnection->pCoreInterface->GetSoundReceiverOrientation( isoundreceiverID );
	plhs[0]        = matlabCreateQuaternion( qOrient );
}

REGISTER_PUBLIC_FUNCTION( get_sound_receiver_orientation_view_up, "Returns the orientation of a sound receiver (as view- and up-vector)",
                          "Note: The view and up vector must be an orthonormal pair of vectors." );
DECLARE_FUNCTION_REQUIRED_INARG( get_sound_receiver_orientation_view_up, soundreceiverID, "integer-1x1", "Sound receiver ID" );
DECLARE_FUNCTION_OUTARG( get_sound_receiver_orientation_view_up, view, "double-3", "View vector" );
DECLARE_FUNCTION_OUTARG( get_sound_receiver_orientation_view_up, up, "double-3", "Up vector" );
void get_sound_receiver_orientation_view_up( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int isoundreceiverID = matlabGetIntegerScalar( prhs[1], "soundreceiverID" );

	VAVec3 v3View, v3Up;
	pConnection->pCoreInterface->GetSoundReceiverOrientationVU( isoundreceiverID, v3View, v3Up );
	plhs[0] = matlabCreateRealVector3( v3View );
	plhs[1] = matlabCreateRealVector3( v3Up );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( get_sound_receiver_pose, "Returns the position and orientation of a sound receiver", "" );
DECLARE_FUNCTION_REQUIRED_INARG( get_sound_receiver_pose, soundreceiverID, "integer-1x1", "Sound receiver ID" );
DECLARE_FUNCTION_OUTARG( get_sound_receiver_pose, pos, "double-3", "Position vector [x,y,z] (unit: meters)" );
DECLARE_FUNCTION_OUTARG( get_sound_receiver_pose, quat, "double-4", "Rotation quaternion [w,x,y,z]" );

void get_sound_receiver_pose( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int isoundreceiverID = matlabGetIntegerScalar( prhs[1], "soundreceiverID" );
	VAVec3 v3Pos;
	VAQuat qOrient;

	pConnection->pCoreInterface->GetSoundReceiverPose( isoundreceiverID, v3Pos, qOrient );
	plhs[0] = matlabCreateRealVector3( v3Pos );
	plhs[1] = matlabCreateQuaternion( qOrient );
}


// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( set_sound_receiver_position, "Sets the position of a sound receiver", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_receiver_position, soundreceiverID, "integer-1x1", "Sound receiver ID" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_receiver_position, pos, "double-3", "Position vector [x,y,z] (unit: meters)" );
void set_sound_receiver_position( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int isoundreceiverID = matlabGetIntegerScalar( prhs[1], "soundreceiverID" );
	VAVec3 v3Pos;
	matlabGetRealVector3( prhs[2], "pos", v3Pos );

	pConnection->pCoreInterface->SetSoundReceiverPosition( isoundreceiverID, v3Pos );
}

REGISTER_PUBLIC_FUNCTION( set_sound_receiver_orientation, "Sets the orientation of a sound receiver", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_receiver_orientation, soundreceiverID, "integer-1x1", "Sound receiver ID" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_receiver_orientation, orient, "double-4", "Rotation quaternion [w,x,y,z]" );
void set_sound_receiver_orientation( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int isoundreceiverID = matlabGetIntegerScalar( prhs[1], "soundreceiverID" );
	VAQuat qOrient;
	matlabGetQuaternion( prhs[2], "orient", qOrient );

	pConnection->pCoreInterface->SetSoundReceiverOrientation( isoundreceiverID, qOrient );
}

REGISTER_PUBLIC_FUNCTION( set_sound_receiver_head_above_torso_orientation, "Sets the head-above-torso (relative) orientation of a sound receiver", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_receiver_head_above_torso_orientation, soundreceiverID, "integer-1x1", "Sound receiver ID" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_receiver_head_above_torso_orientation, orient, "double-4", "Rotation quaternion [w,x,y,z]" );
void set_sound_receiver_head_above_torso_orientation( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int isoundreceiverID = matlabGetIntegerScalar( prhs[1], "soundreceiverID" );
	VAQuat qOrient;
	matlabGetQuaternion( prhs[2], "orient", qOrient );

	pConnection->pCoreInterface->SetSoundReceiverHeadAboveTorsoOrientation( isoundreceiverID, qOrient );
}

REGISTER_PUBLIC_FUNCTION( set_sound_receiver_orientation_view_up, "Sets the orientation of a sound receiver (as view- and up-vector)",
                          "Note: The view and up vector must be an orthonormal pair of vectors." );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_receiver_orientation_view_up, soundreceiverID, "integer-1x1", "Sound receiver ID" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_receiver_orientation_view_up, view, "double-3", "View vector" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_receiver_orientation_view_up, up, "double-3", "Up vector" );
void set_sound_receiver_orientation_view_up( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 4 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int isoundreceiverID = matlabGetIntegerScalar( prhs[1], "soundreceiverID" );
	VAVec3 v3View, v3Up;
	matlabGetRealVector3( prhs[2], "view", v3View );
	matlabGetRealVector3( prhs[3], "up", v3Up );

	pConnection->pCoreInterface->SetSoundReceiverOrientationVU( isoundreceiverID, v3View, v3Up );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( set_sound_receiver_pose, "Sets the position and orientation (in yaw-pitch-roll angles) of a sound receiver", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_receiver_pose, soundreceiverID, "integer-1x1", "Sound receiver ID" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_receiver_pose, pos, "double-3", "Position vector [x, y, z] (unit: meters)" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_receiver_pose, quat, "double-4", "Rotation angles [w,x,y,z]" );
void set_sound_receiver_pose( int, mxArray *[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 4 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int isoundreceiverID = matlabGetIntegerScalar( prhs[1], "soundreceiverID" );
	VAVec3 v3Pos;
	VAQuat qOrient;
	matlabGetRealVector3( prhs[2], "pos", v3Pos );
	matlabGetQuaternion( prhs[3], "quat", qOrient );

	pConnection->pCoreInterface->SetSoundReceiverPose( isoundreceiverID, v3Pos, qOrient );
}

REGISTER_PUBLIC_FUNCTION( get_sound_receiver_real_world_head_above_torso_orientation,
                          "Returns the real-world orientation (as quaterion) of the sound receiver's head over the torso",
                          "The orientation is meant to be inversely relative to the real world sound receiver orientation." );
DECLARE_FUNCTION_REQUIRED_INARG( get_sound_receiver_real_world_head_above_torso_orientation, soundreceiverID, "integer-1x1", "Sound receiver ID" );
DECLARE_FUNCTION_OUTARG( get_sound_receiver_real_world_head_above_torso_orientation, view, "double-4", "Rotation quaternion [w,x,y,z]" );
void get_sound_receiver_real_world_head_above_torso_orientation( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int isoundreceiverID = matlabGetIntegerScalar( prhs[1], "soundreceiverID" );

	VAQuat qOrient = pConnection->pCoreInterface->GetSoundReceiverRealWorldHeadAboveTorsoOrientation( isoundreceiverID );
	plhs[0]        = matlabCreateQuaternion( qOrient );
}

REGISTER_PUBLIC_FUNCTION( set_sound_receiver_real_world_head_above_torso_orientation,
                          "Updates the real-world position and orientation (as view- and up vector) of the sound receiver's head",
                          "Note: The view and up vector must be an orthonormal pair of vectors." );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_receiver_real_world_head_above_torso_orientation, soundreceiverID, "integer-1x1", "Sound receiver ID" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_receiver_real_world_head_above_torso_orientation, pos, "double-4", "Rotation quaternion [w, x, y, z]" );
void set_sound_receiver_real_world_head_above_torso_orientation( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	const int isoundreceiverID = matlabGetIntegerScalar( prhs[1], "soundreceiverID" );
	VAQuat qOrient;
	matlabGetQuaternion( prhs[2], "orient", qOrient );

	pConnection->pCoreInterface->SetSoundReceiverRealWorldHeadAboveTorsoOrientation( isoundreceiverID, qOrient );
}

REGISTER_PUBLIC_FUNCTION( get_sound_receiver_real_world_head_position_orientation_view_up,
                          "Returns the real-world position and orientation (as view- and up vector) of the sound receiver's head",
                          "Note: The view and up vector must be an orthonormal pair of vectors. The parameter isoundreceiverID has been added for future versions and is "
                          "currently unsupported. You can set it any value you like." );
DECLARE_FUNCTION_REQUIRED_INARG( get_sound_receiver_real_world_head_position_orientation_view_up, soundreceiverID, "integer-1x1", "Sound receiver ID" );
DECLARE_FUNCTION_OUTARG( get_sound_receiver_real_world_head_position_orientation_view_up, pos, "double-3", "Position vector [x,y,z] (unit: meters)" );
DECLARE_FUNCTION_OUTARG( get_sound_receiver_real_world_head_position_orientation_view_up, view, "double-3", "View vector" );
DECLARE_FUNCTION_OUTARG( get_sound_receiver_real_world_head_position_orientation_view_up, up, "double-3", "Up vector" );
void get_sound_receiver_real_world_head_position_orientation_view_up( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int isoundreceiverID = matlabGetIntegerScalar( prhs[1], "soundreceiverID" );

	VAVec3 v3Pos, v3View, v3Up;
	pConnection->pCoreInterface->GetSoundReceiverRealWorldPositionOrientationVU( isoundreceiverID, v3Pos, v3View, v3Up );
	plhs[0] = matlabCreateRealVector3( v3Pos );
	plhs[1] = matlabCreateRealVector3( v3View );
	plhs[2] = matlabCreateRealVector3( v3Up );
}

REGISTER_PUBLIC_FUNCTION( set_sound_receiver_real_world_position_orientation_view_up,
                          "Updates the real-world position and orientation (as view- and up vector) of the sound receiver's head",
                          "Note: The view and up vector must be an orthonormal pair of vectors." );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_receiver_real_world_position_orientation_view_up, soundreceiverID, "integer-1x1", "Sound receiver ID" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_receiver_real_world_position_orientation_view_up, pos, "double-3", "Position vector [x, y, z] (unit: meters)" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_receiver_real_world_position_orientation_view_up, view, "double-3", "View vector" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_receiver_real_world_position_orientation_view_up, up, "double-3", "Up vector" );
void set_sound_receiver_real_world_position_orientation_view_up( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 5 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int isoundreceiverID = matlabGetIntegerScalar( prhs[1], "soundreceiverID" );
	VAVec3 v3Pos, v3View, v3Up;
	matlabGetRealVector3( prhs[2], "pos", v3Pos );
	matlabGetRealVector3( prhs[3], "view", v3View );
	matlabGetRealVector3( prhs[4], "up", v3Up );

	pConnection->pCoreInterface->SetSoundReceiverRealWorldPositionOrientationVU( isoundreceiverID, v3Pos, v3View, v3Up );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( get_sound_portal_ids, "Return the IDs of all portal in the scene", "" );
DECLARE_FUNCTION_OUTARG( get_sound_portal_ids, ids, "integer-1xN", "Vector containing the IDs" );

void get_sound_portal_ids( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 1 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::vector<int> v;
	pConnection->pCoreInterface->GetSoundPortalIDs( v );
	plhs[0] = matlabCreateIDList( v );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( get_sound_portal_name, "Returns the name of a portal", "" );
DECLARE_FUNCTION_REQUIRED_INARG( get_sound_portal_name, portalID, "integer-1x1", "Portal ID" );
DECLARE_FUNCTION_OUTARG( get_sound_portal_name, name, "string", "Displayed name" );

void get_sound_portal_name( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int iPortalID = matlabGetIntegerScalar( prhs[1], "portalID" );

	std::string sName = pConnection->pCoreInterface->GetSoundPortalName( iPortalID );
	plhs[0]           = mxCreateString( sName.c_str( ) );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( set_sound_portal_name, "Sets the name of a portal", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_portal_name, portalID, "integer-1x1", "Portal ID" );
DECLARE_FUNCTION_REQUIRED_INARG( set_sound_portal_name, name, "string", "Displayed name" );

void set_sound_portal_name( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int iPortalID     = matlabGetIntegerScalar( prhs[1], "portalID" );
	std::string sName = matlabGetString( prhs[2], "name" );

	pConnection->pCoreInterface->SetSoundPortalName( iPortalID, sName );
}


// Homogeneous medium

REGISTER_PUBLIC_FUNCTION( set_homogeneous_medium_sound_speed, "Set homogeneous medium sound speed", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_homogeneous_medium_sound_speed, sound_speed, "double-1x1", "Sound speed [m/s]" );
void set_homogeneous_medium_sound_speed( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	const double dSoundSpeed = matlabGetRealScalar( prhs[1], "sound_speed" );

	pConnection->pCoreInterface->SetHomogeneousMediumSoundSpeed( dSoundSpeed );
}

REGISTER_PUBLIC_FUNCTION( get_homogeneous_medium_sound_speed, "Get homogeneous medium sound speed", "" );
DECLARE_FUNCTION_OUTARG( get_homogeneous_medium_sound_speed, sound_speed, "double-1x1", "Sound speed [m/s]" );
void get_homogeneous_medium_sound_speed( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 1 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	const double dSoundSpeed = pConnection->pCoreInterface->GetHomogeneousMediumSoundSpeed( );
	plhs[0]                  = mxCreateDoubleScalar( dSoundSpeed );
}

REGISTER_PUBLIC_FUNCTION( set_homogeneous_medium_temperature, "Set homogeneous medium temperature", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_homogeneous_medium_temperature, temperature, "double-1x1", "Temperature [degree Celsius]" );
void set_homogeneous_medium_temperature( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	const double dTemperature = matlabGetRealScalar( prhs[1], "temperature" );

	pConnection->pCoreInterface->SetHomogeneousMediumTemperature( dTemperature );
}

REGISTER_PUBLIC_FUNCTION( get_homogeneous_medium_temperature, "Get homogeneous medium temperature", "" );
DECLARE_FUNCTION_OUTARG( get_homogeneous_medium_temperature, temperature, "double-1x1", "Temperature [degree Celsius]" );
void get_homogeneous_medium_temperature( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 1 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	const double dTemperature = pConnection->pCoreInterface->GetHomogeneousMediumTemperature( );
	plhs[0]                   = mxCreateDoubleScalar( dTemperature );
}

REGISTER_PUBLIC_FUNCTION( set_homogeneous_medium_static_pressure, "Set homogeneous medium static pressure", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_homogeneous_medium_static_pressure, static_pressure, "double-1x1", "Static pressure [Pa]" );
void set_homogeneous_medium_static_pressure( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	const double dStaticPressure = matlabGetRealScalar( prhs[1], "static_pressure" );

	pConnection->pCoreInterface->SetHomogeneousMediumStaticPressure( dStaticPressure );
}

REGISTER_PUBLIC_FUNCTION( get_homogeneous_medium_static_pressure, "Get homogeneous medium static pressure", "" );
DECLARE_FUNCTION_OUTARG( get_homogeneous_medium_static_pressure, static_pressure, "double-1x1", "Static pressure [Pa]" );
void get_homogeneous_medium_static_pressure( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 1 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	const double dStaticPressure = pConnection->pCoreInterface->GetHomogeneousMediumStaticPressure( );
	plhs[0]                      = mxCreateDoubleScalar( dStaticPressure );
}

REGISTER_PUBLIC_FUNCTION( set_homogeneous_medium_relative_humidity, "Set homogeneous medium relative humidity", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_homogeneous_medium_relative_humidity, shift_speed, "double-1x1", "Relative humidity [Percent]" );
void set_homogeneous_medium_relative_humidity( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	const double dRelativeHumidity = matlabGetRealScalar( prhs[1], "shift_speed" );

	pConnection->pCoreInterface->SetHomogeneousMediumRelativeHumidity( dRelativeHumidity );
}

REGISTER_PUBLIC_FUNCTION( get_homogeneous_medium_relative_humidity, "Get homogeneous medium relative humidity", "" );
DECLARE_FUNCTION_OUTARG( get_homogeneous_medium_relative_humidity, shift_speed, "double-1x1", "Relative humidity [Percent]" );
void get_homogeneous_medium_relative_humidity( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 1 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	const double dRelativeHumidity = pConnection->pCoreInterface->GetHomogeneousMediumRelativeHumidity( );
	plhs[0]                        = mxCreateDoubleScalar( dRelativeHumidity );
}


REGISTER_PUBLIC_FUNCTION( set_homogeneous_medium_shift_speed, "Set homogeneous medium shift speed", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_homogeneous_medium_shift_speed, shift_speed, "double-3x1", "Shift speed vector [m/s]" );
void set_homogeneous_medium_shift_speed( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	VAVec3 v3ShiftSpeed;
	matlabGetRealVector3( prhs[1], "shift_speed", v3ShiftSpeed );

	pConnection->pCoreInterface->SetHomogeneousMediumShiftSpeed( v3ShiftSpeed );
}

REGISTER_PUBLIC_FUNCTION( get_homogeneous_medium_shift_speed, "Get homogeneous medium shift speed", "" );
DECLARE_FUNCTION_OUTARG( get_homogeneous_medium_shift_speed, shift_speed, "double-3x1", "Shift speed vector [m/s]" );
void get_homogeneous_medium_shift_speed( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 1 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	VAVec3 v3ShiftSpeed = pConnection->pCoreInterface->GetHomogeneousMediumShiftSpeed( );
	plhs[0]             = matlabCreateRealVector3( v3ShiftSpeed );
}

REGISTER_PUBLIC_FUNCTION( get_homogeneous_medium_shift_parameters, "Returns homogeneous medium parameters", "" );
DECLARE_FUNCTION_REQUIRED_INARG( get_homogeneous_medium_shift_parameters, args, "mstruct", "Requested parameters" );
DECLARE_FUNCTION_OUTARG( get_homogeneous_medium_shift_parameters, params, "mstruct", "Parameters" );
void get_homogeneous_medium_shift_parameters( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	CVAStruct oArgs = matlabGetStruct( prhs[1], "args" );
	CVAStruct oRet  = pConnection->pCoreInterface->GetHomogeneousMediumParameters( oArgs );
	plhs[0]         = matlabCreateStruct( oRet );
}

REGISTER_PUBLIC_FUNCTION( set_homogeneous_medium_shift_parameters, "Sets homogeneous medium parameters", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_homogeneous_medium_shift_parameters, params, "mstruct", "Parameters" );
void set_homogeneous_medium_shift_parameters( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	CVAStruct oParams = matlabGetStruct( prhs[1], "params" );
	pConnection->pCoreInterface->SetHomogeneousMediumParameters( oParams );
}


// Material

REGISTER_PUBLIC_FUNCTION( create_acoustic_material_from_file, "Create acoustic material", "" );
DECLARE_FUNCTION_REQUIRED_INARG( create_acoustic_material_from_file, file_path, "string", "Material file path" );
DECLARE_FUNCTION_OPTIONAL_INARG( create_acoustic_material_from_file, material_name, "string", "Material name", "''" );
DECLARE_FUNCTION_OUTARG( create_acoustic_material_from_file, material_id, "double-1x1", "Material identifier" );
void create_acoustic_material_from_file( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::string sFilePath = matlabGetString( prhs[1], "file_path" );
	std::string sName     = matlabGetString( prhs[2], "material_name" );
	const int iID         = pConnection->pCoreInterface->CreateAcousticMaterialFromFile( sFilePath, sName );

	plhs[0] = mxCreateDoubleScalar( iID );
}

REGISTER_PUBLIC_FUNCTION( delete_acoustic_material, "Delete acoustic material", "" );
DECLARE_FUNCTION_REQUIRED_INARG( delete_acoustic_material, material_id, "double-1x1", "Material identifier" );
DECLARE_FUNCTION_OUTARG( delete_acoustic_material, success_flag, "logical-1x1", "Removal success" );
void delete_acoustic_material( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	const int iID       = matlabGetIntegerScalar( prhs[1], "material_id" );
	const bool bSuccess = pConnection->pCoreInterface->DeleteAcousticMaterial( iID );

	plhs[0] = mxCreateLogicalScalar( bSuccess );
}

REGISTER_PUBLIC_FUNCTION( get_acoustic_magerial_parameters, "Acoustic material parameter getter", "" );
DECLARE_FUNCTION_REQUIRED_INARG( get_acoustic_magerial_parameters, material_id, "integer-1x1", "Acoustic material identifier" );
DECLARE_FUNCTION_REQUIRED_INARG( get_acoustic_magerial_parameters, args, "mstruct", "Requested parameters" );
DECLARE_FUNCTION_OUTARG( get_acoustic_magerial_parameters, params, "mstruct", "Parameters" );
void get_acoustic_magerial_parameters( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	const int iID   = matlabGetIntegerScalar( prhs[1], "material_id" );
	CVAStruct oArgs = matlabGetStruct( prhs[2], "args" );
	CVAStruct oRet  = pConnection->pCoreInterface->GetAcousticMaterialParameters( iID, oArgs );
	plhs[0]         = matlabCreateStruct( oRet );
}

REGISTER_PUBLIC_FUNCTION( set_acoustic_magerial_parameters, "Acoustic material parameter setter", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_acoustic_magerial_parameters, material_id, "integer-1x1", "Acoustic material identifier" );
DECLARE_FUNCTION_REQUIRED_INARG( set_acoustic_magerial_parameters, params, "mstruct", "Parameters" );
void set_acoustic_magerial_parameters( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	const int iID     = matlabGetIntegerScalar( prhs[1], "material_id" );
	CVAStruct oParams = matlabGetStruct( prhs[2], "params" );
	pConnection->pCoreInterface->SetAcousticMaterialParameters( iID, oParams );
}


// Geometry


REGISTER_PUBLIC_FUNCTION( create_geometry_mesh_from_file, "Create geometry mesh from file", "" );
DECLARE_FUNCTION_REQUIRED_INARG( create_geometry_mesh_from_file, file_path, "string", "Geometry mesh file path" );
DECLARE_FUNCTION_OPTIONAL_INARG( create_geometry_mesh_from_file, geo_mesh_name, "string", "Geometry mesh name", "''" );
DECLARE_FUNCTION_OUTARG( create_geometry_mesh_from_file, geo_mesh_id, "double-1x1", "Geometry mesh identifier" );
void create_geometry_mesh_from_file( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::string sFilePath = matlabGetString( prhs[1], "file_path" );
	std::string sName     = matlabGetString( prhs[2], "geo_mesh_name" );
	const int iID         = pConnection->pCoreInterface->CreateGeometryMeshFromFile( sFilePath, sName );

	plhs[0] = mxCreateDoubleScalar( iID );
}

REGISTER_PUBLIC_FUNCTION( delete_geometry_mesh, "Delete geometry mesh", "" );
DECLARE_FUNCTION_REQUIRED_INARG( delete_geometry_mesh, geo_mesh_id, "double-1x1", "Geometry mesh identifier" );
DECLARE_FUNCTION_OUTARG( delete_geometry_mesh, success_flag, "logical-1x1", "Removal success" );
void delete_geometry_mesh( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	const int iID       = matlabGetIntegerScalar( prhs[1], "geo_mesh_id" );
	const bool bSuccess = pConnection->pCoreInterface->DeleteGeometryMesh( iID );

	plhs[0] = mxCreateLogicalScalar( bSuccess );
}

REGISTER_PUBLIC_FUNCTION( get_geometry_mesh_enabled, "Geometry mesh enabled getter", "" );
DECLARE_FUNCTION_REQUIRED_INARG( get_geometry_mesh_enabled, geo_mesh_id, "integer-1x1", "Geometry mesh identifier" );
DECLARE_FUNCTION_OUTARG( get_geometry_mesh_enabled, result, "logical-1x1", "Enabled flag" );
void get_geometry_mesh_enabled( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	const int iID      = matlabGetIntegerScalar( prhs[1], "geo_mesh_id" );
	const bool bResult = pConnection->pCoreInterface->GetGeometryMeshEnabled( iID );

	plhs[0] = mxCreateLogicalScalar( bResult );
}

REGISTER_PUBLIC_FUNCTION( set_geometry_mesh_enabled, "Geometry mesh enabled setter", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_geometry_mesh_enabled, geo_mesh_id, "integer-1x1", "Geometry mesh identifier" );
DECLARE_FUNCTION_OPTIONAL_INARG( set_geometry_mesh_enabled, enabled, "logical-1x1", "Enabled flag", "1" );
void set_geometry_mesh_enabled( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	const int iID     = matlabGetIntegerScalar( prhs[1], "geo_mesh_id" );
	const bool bMuted = matlabGetBoolScalar( prhs[2], "enabled" );

	pConnection->pCoreInterface->SetGeometryMeshEnabled( iID, bMuted );
}

REGISTER_PUBLIC_FUNCTION( get_geometry_mesh_parameters, "Geometry mesh parameter getter", "" );
DECLARE_FUNCTION_REQUIRED_INARG( get_geometry_mesh_parameters, geo_mesh_id, "integer-1x1", "Geometry mesh identifier" );
DECLARE_FUNCTION_REQUIRED_INARG( get_geometry_mesh_parameters, args, "mstruct", "Requested parameters" );
DECLARE_FUNCTION_OUTARG( get_geometry_mesh_parameters, params, "mstruct", "Parameters" );
void get_geometry_mesh_parameters( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	const int iID   = matlabGetIntegerScalar( prhs[1], "geo_mesh_id" );
	CVAStruct oArgs = matlabGetStruct( prhs[2], "args" );
	CVAStruct oRet  = pConnection->pCoreInterface->GetGeometryMeshParameters( iID, oArgs );
	plhs[0]         = matlabCreateStruct( oRet );
}

REGISTER_PUBLIC_FUNCTION( set_geometry_mesh_parameters, "Geometry mesh parameter setter", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_geometry_mesh_parameters, geo_mesh_id, "integer-1x1", "Geometry mesh identifier" );
DECLARE_FUNCTION_REQUIRED_INARG( set_geometry_mesh_parameters, params, "mstruct", "Parameters" );
void set_geometry_mesh_parameters( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	const int iID     = matlabGetIntegerScalar( prhs[1], "geo_mesh_id" );
	CVAStruct oParams = matlabGetStruct( prhs[2], "params" );
	pConnection->pCoreInterface->SetGeometryMeshParameters( iID, oParams );
}


// Global methods

REGISTER_PUBLIC_FUNCTION( get_input_gain, "Returns the gain the audio device input channels", "" );
DECLARE_FUNCTION_OUTARG( get_input_gain, gain, "double-1x1", "Input gain (amplification factor >=0)" );
void get_input_gain( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 1 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	const double dGain = pConnection->pCoreInterface->GetInputGain( );
	plhs[0]            = mxCreateDoubleScalar( dGain );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( set_input_gain, "Sets the gain the audio device input channels", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_input_gain, gain, "double-1x1", "Input gain (amplification factor >=0)" );

void set_input_gain( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	double dGain = matlabGetRealScalar( prhs[1], "gain" );

	pConnection->pCoreInterface->SetInputGain( dGain );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( get_input_muted, "Returns if the audio device inputs are muted", "" );
DECLARE_FUNCTION_OUTARG( get_input_muted, result, "logical-1x1", "Inputs muted?" );

void get_input_muted( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 1 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	bool bResult = pConnection->pCoreInterface->GetInputMuted( );
	plhs[0]      = mxCreateLogicalScalar( bResult );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( set_input_muted, "Sets the audio device inputs muted or unmuted", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_input_muted, muted, "logical-1x1", "Muted?" );

void set_input_muted( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	bool bMuted = matlabGetBoolScalar( prhs[1], "muted" );

	pConnection->pCoreInterface->SetInputMuted( bMuted );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( get_output_gain, "Returns the global output gain", "" );
DECLARE_FUNCTION_OUTARG( get_output_gain, gain, "double-1x1", "Output gain (amplification factor >=0)" );

void get_output_gain( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 1 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	double dGain = pConnection->pCoreInterface->GetOutputGain( );
	plhs[0]      = mxCreateDoubleScalar( dGain );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( set_output_gain, "Sets global output gain", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_output_gain, gain, "double-1x1", "Output gain (amplification factor >=0)" );

void set_output_gain( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	double dGain = matlabGetRealScalar( prhs[1], "gain" );

	pConnection->pCoreInterface->SetOutputGain( dGain );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( get_output_muted, "Returns if the global output is muted", "" );
DECLARE_FUNCTION_OUTARG( get_output_muted, result, "logical-1x1", "Output muted?" );

void get_output_muted( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 1 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	bool bResult = pConnection->pCoreInterface->GetOutputMuted( );
	plhs[0]      = mxCreateLogicalScalar( bResult );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( set_output_muted, "Sets the global output muted or unmuted", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_output_muted, muted, "logical-1x1", "Output muted?" );

void set_output_muted( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	bool bMuted = matlabGetBoolScalar( prhs[1], "muted" );

	pConnection->pCoreInterface->SetOutputMuted( bMuted );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( get_global_auralization_mode, "Returns the global auralization mode", "" );
DECLARE_FUNCTION_OUTARG( get_global_auralization_mode, auralizationMode, "string", "Auralization mode" );

void get_global_auralization_mode( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 1 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	int iAuralizationMode = pConnection->pCoreInterface->GetGlobalAuralizationMode( );
	plhs[0]               = mxCreateString( IVAInterface::GetAuralizationModeStr( iAuralizationMode, true ).c_str( ) );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( set_global_auralization_mode, "Sets global auralization mode", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_global_auralization_mode, auralizationMode, "string", "Auralization mode" );

void set_global_auralization_mode( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::string sAuralizationMode = matlabGetString( prhs[1], "auralizationMode" );

	// Get the current auralization mode first for computing relative modes (+|-)
	int iCurAuralizationMode = pConnection->pCoreInterface->GetGlobalAuralizationMode( );
	int iNewAuralizationMode = IVAInterface::ParseAuralizationModeStr( sAuralizationMode, iCurAuralizationMode );

	pConnection->pCoreInterface->SetGlobalAuralizationMode( iNewAuralizationMode );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( get_core_clock, "Returns the current core time", "" );
DECLARE_FUNCTION_OUTARG( get_core_clock, clk, "double-1x1", "Core clock time (unit: seconds)" );

void get_core_clock( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 1 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	double dClk = pConnection->pCoreInterface->GetCoreClock( );
	plhs[0]     = mxCreateDoubleScalar( dClk );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( set_core_clock, "Sets the core clock time", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_core_clock, clk, "double-1x1", "New core clock time (unit: seconds)" );

void set_core_clock( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	double dClk = matlabGetRealScalar( prhs[1], "clk" );

	pConnection->pCoreInterface->SetCoreClock( dClk );
}


// Rendering

REGISTER_PUBLIC_FUNCTION( set_rendering_module_muted, "Mutes a reproduction module", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_rendering_module_muted, sModuleID, "string", "Module identifier" );
DECLARE_FUNCTION_REQUIRED_INARG( set_rendering_module_muted, bMuted, "logical-1x1", "Mute (true) or unmute (false)" );
void set_rendering_module_muted( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::string sID = matlabGetString( prhs[1], "sModuleID" );
	bool bMuted     = matlabGetBoolScalar( prhs[2], "bMuted" );

	pConnection->pCoreInterface->SetRenderingModuleMuted( sID, bMuted );
}

REGISTER_PUBLIC_FUNCTION( get_rendering_module_muted, "Is reproduction module muted?", "" );
DECLARE_FUNCTION_REQUIRED_INARG( get_rendering_module_muted, sModuleID, "string", "Module identifier" );
DECLARE_FUNCTION_OUTARG( get_rendering_module_muted, bMuted, "logical-1x1", "true if muted, false if unmuted" );
void get_rendering_module_muted( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::string sID = matlabGetString( prhs[1], "sModuleID" );
	bool bMuted     = pConnection->pCoreInterface->GetRenderingModuleMuted( sID );

	plhs[0] = mxCreateLogicalScalar( bMuted );
}

REGISTER_PUBLIC_FUNCTION( set_rendering_module_gain, "Sets the output gain of a reproduction module", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_rendering_module_gain, sModuleID, "string", "Module identifier" );
DECLARE_FUNCTION_REQUIRED_INARG( set_rendering_module_gain, dGain, "double-1x1", "gain (factor)" );
void set_rendering_module_gain( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::string sID = matlabGetString( prhs[1], "sModuleID" );
	double dGain    = matlabGetRealScalar( prhs[2], "dGain" );

	pConnection->pCoreInterface->SetRenderingModuleGain( sID, dGain );
}

REGISTER_PUBLIC_FUNCTION( get_rendering_module_gain, "Get rendering module output gain", "" );
DECLARE_FUNCTION_REQUIRED_INARG( get_rendering_module_gain, sModuleID, "string", "Module identifier" );
DECLARE_FUNCTION_OUTARG( get_rendering_module_gain, dGain, "double-1x1", "Gain (scalar)" );

void get_rendering_module_gain( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::string sID = matlabGetString( prhs[1], "sModuleID" );
	double dGain    = pConnection->pCoreInterface->GetRenderingModuleGain( sID );

	plhs[0] = mxCreateDoubleScalar( dGain );
}

REGISTER_PUBLIC_FUNCTION( get_rendering_module_auralization_mode, "Returns the current rendering module parameters", "" );
DECLARE_FUNCTION_REQUIRED_INARG( get_rendering_module_auralization_mode, sModuleID, "string", "Module identifier" );
DECLARE_FUNCTION_OUTARG( get_rendering_module_auralization_mode, auralization_mode, "string", "Auralization mode as string" );
void get_rendering_module_auralization_mode( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::string sID             = matlabGetString( prhs[1], "sModuleID" );
	const int iAuralizationMode = pConnection->pCoreInterface->GetRenderingModuleAuralizationMode( sID );
	plhs[0]                     = mxCreateString( IVAInterface::GetAuralizationModeStr( iAuralizationMode, true ).c_str( ) );
}

REGISTER_PUBLIC_FUNCTION( set_rendering_module_auralization_mode, "Sets the output gain of a reproduction module", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_rendering_module_auralization_mode, sModuleID, "string", "Module identifier" );
DECLARE_FUNCTION_REQUIRED_INARG( set_rendering_module_auralization_mode, am_str, "string", "auralization mode string" );
void set_rendering_module_auralization_mode( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::string sID       = matlabGetString( prhs[1], "sModuleID" );
	const std::string sAM = matlabGetString( prhs[2], "am_str" );
	const int iAM         = IVAInterface::ParseAuralizationModeStr( sAM );
	pConnection->pCoreInterface->SetRenderingModuleAuralizationMode( sID, iAM );
}

REGISTER_PUBLIC_FUNCTION( get_rendering_module_parameters, "Returns the current rendering module parameters", "" );
DECLARE_FUNCTION_REQUIRED_INARG( get_rendering_module_parameters, sModuleID, "string", "Module identifier" );
DECLARE_FUNCTION_REQUIRED_INARG( get_rendering_module_parameters, args, "mstruct", "Requested parameters" );
DECLARE_FUNCTION_OUTARG( get_rendering_module_parameters, params, "mstruct", "Parameters" );
void get_rendering_module_parameters( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::string sID = matlabGetString( prhs[1], "sModuleID" );
	CVAStruct oArgs = matlabGetStruct( prhs[2], "args" );
	CVAStruct oRet  = pConnection->pCoreInterface->GetRenderingModuleParameters( sID, oArgs );
	plhs[0]         = matlabCreateStruct( oRet );
}

REGISTER_PUBLIC_FUNCTION( set_rendering_module_parameters, "Sets rendering module parameters", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_rendering_module_parameters, sModuleID, "string", "Module identifier" );
DECLARE_FUNCTION_REQUIRED_INARG( set_rendering_module_parameters, params, "mstruct", "Parameters" );
void set_rendering_module_parameters( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::string sID   = matlabGetString( prhs[1], "sModuleID" );
	CVAStruct oParams = matlabGetStruct( prhs[2], "params" );
	pConnection->pCoreInterface->SetRenderingModuleParameters( sID, oParams );
}

REGISTER_PUBLIC_FUNCTION( get_rendering_modules, "Get list of rendering modules", "" );
DECLARE_FUNCTION_OPTIONAL_INARG( get_rendering_modules, bFilterEnabled, "boolean-1x1", "Filter activated (true)", "1" );
DECLARE_FUNCTION_OUTARG( get_rendering_modules, renderers, "cell-array of struct-1x1", "Renderer infos (names, descriptions, etc.)" );
void get_rendering_modules( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	bool bFilterEnabled = true;
	if( nrhs > 1 )
		bFilterEnabled = matlabGetBoolScalar( prhs[1], "bFilterEnabled" );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::vector<CVAAudioRendererInfo> voRenderers;
	pConnection->pCoreInterface->GetRenderingModules( voRenderers, bFilterEnabled );


	const size_t nDims           = int( voRenderers.size( ) );
	const int nFields            = 8;
	const char *ppszFieldNames[] = {
		"id", "class", "enabled", "desc", "output_detector_enabled", "output_recording_enabled", "output_recording_file_path", "parameters"
	};
	plhs[0] = mxCreateStructArray( 1, &nDims, nFields, ppszFieldNames );

	for( size_t i = 0; i < nDims; i++ )
	{
		size_t j = 0;
		mxSetField( plhs[0], i, ppszFieldNames[j++], mxCreateString( voRenderers[i].sID.c_str( ) ) );
		mxSetField( plhs[0], i, ppszFieldNames[j++], mxCreateString( voRenderers[i].sClass.c_str( ) ) );
		mxSetField( plhs[0], i, ppszFieldNames[j++], mxCreateLogicalScalar( voRenderers[i].bEnabled ) );
		mxSetField( plhs[0], i, ppszFieldNames[j++], mxCreateString( voRenderers[i].sDescription.c_str( ) ) );
		mxSetField( plhs[0], i, ppszFieldNames[j++], mxCreateLogicalScalar( voRenderers[i].bOutputDetectorEnabled ) );
		mxSetField( plhs[0], i, ppszFieldNames[j++], mxCreateLogicalScalar( voRenderers[i].bOutputRecordingEnabled ) );
		mxSetField( plhs[0], i, ppszFieldNames[j++], mxCreateString( voRenderers[i].sOutputRecordingFilePath.c_str( ) ) );
		mxSetField( plhs[0], i, ppszFieldNames[j++], matlabCreateStruct( voRenderers[i].oParams ) );
	}
}

// Reproduction

REGISTER_PUBLIC_FUNCTION( set_reproduction_module_gain, "Sets the output gain of a reproduction module", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_reproduction_module_gain, sModuleID, "string", "Module identifier" );
DECLARE_FUNCTION_REQUIRED_INARG( set_reproduction_module_gain, dGain, "double-1x1", "gain (factor)" );

void set_reproduction_module_gain( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::string sID = matlabGetString( prhs[1], "sModuleID" );
	double dGain    = matlabGetRealScalar( prhs[2], "dGain" );

	pConnection->pCoreInterface->SetReproductionModuleGain( sID, dGain );
}

REGISTER_PUBLIC_FUNCTION( set_reproduction_module_muted, "Mutes a reproduction module", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_reproduction_module_muted, sModuleID, "string", "Module identifier" );
DECLARE_FUNCTION_REQUIRED_INARG( set_reproduction_module_muted, bMuted, "logical-1x1", "Mute (true) or unmute (false)" );

void set_reproduction_module_muted( int nlhs, mxArray **, int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::string sID = matlabGetString( prhs[1], "sModuleID" );
	bool bMuted     = matlabGetBoolScalar( prhs[2], "bMuted" );

	pConnection->pCoreInterface->SetReproductionModuleMuted( sID, bMuted );
}

REGISTER_PUBLIC_FUNCTION( get_reproduction_module_muted, "Is reproduction module muted?", "" );
DECLARE_FUNCTION_REQUIRED_INARG( get_reproduction_module_muted, sModuleID, "string", "Module identifier" );
DECLARE_FUNCTION_OUTARG( get_reproduction_module_muted, bMuted, "logical-1x1", "true if muted, false if unmuted" );

void get_reproduction_module_muted( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::string sID = matlabGetString( prhs[1], "sModuleID" );
	bool bMuted     = pConnection->pCoreInterface->GetReproductionModuleMuted( sID );

	plhs[0] = mxCreateLogicalScalar( bMuted );
}

REGISTER_PUBLIC_FUNCTION( get_reproduction_module_gain, "Returns the reproduction module output gain", "" );
DECLARE_FUNCTION_REQUIRED_INARG( get_reproduction_module_gain, sModuleID, "string", "Module identifier" );
DECLARE_FUNCTION_OUTARG( get_reproduction_module_gain, dGain, "double-1x1", "Gain (scalar)" );

void get_reproduction_module_gain( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::string sID = matlabGetString( prhs[1], "sModuleID" );
	double dGain    = pConnection->pCoreInterface->GetReproductionModuleGain( sID );

	plhs[0] = mxCreateDoubleScalar( dGain );
}

REGISTER_PUBLIC_FUNCTION( get_reproduction_modules, "Get list of rendering modules", "" );
DECLARE_FUNCTION_OPTIONAL_INARG( get_reproduction_modules, bFilterEnabled, "boolean-1x1", "Filter activated (true)", "1" );
DECLARE_FUNCTION_OUTARG( get_reproduction_modules, reproductionmodules, "cell-array of struct-1x1", "Reproduction module infos (names, descriptions, etc.)" );
void get_reproduction_modules( int, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	bool bFilterEnabled = true;
	if( nrhs > 1 )
		bFilterEnabled = matlabGetBoolScalar( prhs[1], "bFilterEnabled" );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::vector<CVAAudioReproductionInfo> voReproductions;
	pConnection->pCoreInterface->GetReproductionModules( voReproductions, bFilterEnabled );

	const size_t nDims           = int( voReproductions.size( ) );
	const int nFields            = 11;
	const char *ppszFieldNames[] = { "id",
		                             "class",
		                             "enabled",
		                             "desc",
		                             "output_detector_enabled",
		                             "output_recording_enabled",
		                             "output_recording_file_path",
		                             "input_detector_enabled",
		                             "input_recording_enabled",
		                             "input_recording_file_path",
		                             "parameters" };
	plhs[0]                      = mxCreateStructArray( 1, &nDims, nFields, ppszFieldNames );

	for( size_t i = 0; i < nDims; i++ )
	{
		size_t j = 0;
		mxSetField( plhs[0], i, ppszFieldNames[j++], mxCreateString( voReproductions[i].sID.c_str( ) ) );
		mxSetField( plhs[0], i, ppszFieldNames[j++], mxCreateString( voReproductions[i].sClass.c_str( ) ) );
		mxSetField( plhs[0], i, ppszFieldNames[j++], mxCreateLogicalScalar( voReproductions[i].bEnabled ) );
		mxSetField( plhs[0], i, ppszFieldNames[j++], mxCreateString( voReproductions[i].sDescription.c_str( ) ) );
		mxSetField( plhs[0], i, ppszFieldNames[j++], mxCreateLogicalScalar( voReproductions[i].bOutputDetectorEnabled ) );
		mxSetField( plhs[0], i, ppszFieldNames[j++], mxCreateLogicalScalar( voReproductions[i].bOutputRecordingEnabled ) );
		mxSetField( plhs[0], i, ppszFieldNames[j++], mxCreateString( voReproductions[i].sOutputRecordingFilePath.c_str( ) ) );
		mxSetField( plhs[0], i, ppszFieldNames[j++], mxCreateLogicalScalar( voReproductions[i].bInputDetectorEnabled ) );
		mxSetField( plhs[0], i, ppszFieldNames[j++], mxCreateLogicalScalar( voReproductions[i].bInputRecordingEnabled ) );
		mxSetField( plhs[0], i, ppszFieldNames[j++], mxCreateString( voReproductions[i].sInputRecordingFilePath.c_str( ) ) );
		mxSetField( plhs[0], i, ppszFieldNames[j++], matlabCreateStruct( voReproductions[i].oParams ) );
	}
}

REGISTER_PUBLIC_FUNCTION( get_reproduction_module_parameters, "Returns the current reproduction module parameters", "" );
DECLARE_FUNCTION_REQUIRED_INARG( get_reproduction_module_parameters, sModuleID, "string", "Module identifier" );
DECLARE_FUNCTION_REQUIRED_INARG( get_reproduction_module_parameters, args, "mstruct", "Requested parameters" );
DECLARE_FUNCTION_OUTARG( get_reproduction_module_parameters, params, "mstruct", "Parameters" );
void get_reproduction_module_parameters( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::string sID = matlabGetString( prhs[1], "sModuleID" );
	CVAStruct oArgs = matlabGetStruct( prhs[2], "args" );
	CVAStruct oRet  = pConnection->pCoreInterface->GetReproductionModuleParameters( sID, oArgs );
	plhs[0]         = matlabCreateStruct( oRet );
}

REGISTER_PUBLIC_FUNCTION( set_reproduction_module_parameters, "Sets reproduction module parameters", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_reproduction_module_parameters, sModuleID, "string", "Module identifier" );
DECLARE_FUNCTION_REQUIRED_INARG( set_reproduction_module_parameters, params, "mstruct", "Parameters" );
void set_reproduction_module_parameters( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 3 );

	ConnectionHandle hHandle         = GetConnectionHandle( prhs[0] );
	CVAMatlabConnection *pConnection = g_vpConnections[hHandle];

	std::string sID   = matlabGetString( prhs[1], "sModuleID" );
	CVAStruct oParams = matlabGetStruct( prhs[2], "params" );
	pConnection->pCoreInterface->SetReproductionModuleParameters( sID, oParams );
}

/* +----------------------------------------------------------+ *
 * |                                                          | *
 * |   Timer                                                  | *
 * |                                                          | *
 * +----------------------------------------------------------+ */

// Timer only available for Windows
// TODO: Additional platform implementations. Make available for other bindings, e..g Python
#ifdef WIN32

// Class managing the high performance timer
//	Automatically freeing the timer handle when DLL is unloaded
class Timer
{
private:
	HANDLE hHandle;
	bool bMinimumClockPeriodSet                       = false;
	const static unsigned int MINIMUM_CLOCK_PERIOD_MS = 1;

public:
	inline Timer( ) : hHandle( 0 ) { };

	inline ~Timer( ) { Reset( ); };

	// Creates the timer and sets period
	inline void Set( double dPeriod )
	{
		if( hHandle == nullptr )
		{
			if( ( hHandle = CreateWaitableTimer( NULL, FALSE, NULL ) ) == nullptr )
				VA_EXCEPT1( "Failed to create timer" );
		}

		// It seems that by default Windows does not guarantee the highest resolution (since Windows 10, version 2004).
		// See https://docs.microsoft.com/en-us/windows/win32/api/timeapi/nf-timeapi-timebeginperiod -> Remarks
		// Setting it to 1ms, but making sure that it is reset with the timer
		if( timeBeginPeriod( MINIMUM_CLOCK_PERIOD_MS ) == TIMERR_NOERROR )
			bMinimumClockPeriodSet = true;
		else
			mexWarnMsgTxt( "Failed to set minimum clock period to 1ms. The timer might not work with the desired accuracy!" );

		LARGE_INTEGER liDueTime;                                                    // Time till first activation
		liDueTime.QuadPart = -( (long long)std::floor( dPeriod * 10000000 ) + 10 ); // Number of 100ns slots, added 1us to period for first call
		long lPeriod       = (long)std::floor( dPeriod * 1000 );                    // Dauer in Millisekunden

		if( SetWaitableTimer( hHandle, &liDueTime, lPeriod, NULL, NULL, FALSE ) == false )
		{
			Reset( );
			VA_EXCEPT1( "Failed to set waitable timer" );
		}
	}
	// Waits until the timer is triggered.
	inline void WaitFor( )
	{
		if( hHandle == 0 )
			VA_EXCEPT1( "Timer not set" );

		if( WaitForSingleObject( hHandle, INFINITE ) != WAIT_OBJECT_0 ) // This wait is effective.
			VA_EXCEPT1( "Could not wait for waitable timer" );
	}

	inline void Reset( )
	{
		if( hHandle != 0 )
		{
			CloseHandle( hHandle );
			if( bMinimumClockPeriodSet )
				timeEndPeriod( MINIMUM_CLOCK_PERIOD_MS );

			hHandle                = 0;
			bMinimumClockPeriodSet = false;
		}
	};
};

static Timer g_oTimerHandle;

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( set_timer, "Sets up the high-precision timer", "" );
DECLARE_FUNCTION_REQUIRED_INARG( set_timer, period, "double-1x1", "Timer period (unit: seconds)" );

void set_timer( int, mxArray *[], int nrhs, const mxArray *prhs[] )
{
	REQUIRE_INPUT_ARGS( 2 );

	double dPeriod = matlabGetRealScalar( prhs[1], "period" );

	if( dPeriod <= 0 )
		VA_EXCEPT2( INVALID_PARAMETER, "Timer period must be greater zero" );

	g_oTimerHandle.Set( dPeriod );
}

// ------------------------------------------------------------

REGISTER_PUBLIC_FUNCTION( wait_for_timer, "Wait for a signal of the high-precision timer", "" );
void wait_for_timer( int, mxArray *[], int nrhs, const mxArray *[] )
{
	REQUIRE_INPUT_ARGS( 1 );
	g_oTimerHandle.WaitFor( );
}

REGISTER_PUBLIC_FUNCTION( close_timer, "Closes the timer", "" );
void close_timer( int, mxArray *[], int nrhs, const mxArray *[] )
{
	REQUIRE_INPUT_ARGS( 1 );
	g_oTimerHandle.Reset( );
}

#endif // WIN32
