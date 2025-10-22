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

// STL includes
#ifdef WIN32
#	include <conio.h>
#else
#	include <ncurses.h>
#endif

#include <iostream>
#include <signal.h>

// ITA includes
#include <ITAException.h>
#include <ITAFileSystemUtils.h>
#include <ITANumericUtils.h>
#include <ITAStringUtils.h>

// VA includes
#include <VA.h>
#include <VACore.h>
#include <VANet.h>
#include <VistaBase/VistaTimeUtils.h>
#include <Windows.h>

using namespace std;

// Core

IVAInterface* pCore                      = NULL;
IVANetServer* pServer                    = NULL;
IVAEventHandler* pServerCoreEventHandler = NULL;

void StopServer( )
{
	try
	{
		if( pServerCoreEventHandler )
			pCore->DetachEventHandler( pServerCoreEventHandler );

		if( pServer )
		{
			delete pServer;
			pServer = nullptr;
		}

		if( pCore )
		{
			pCore->Finalize( );
			delete pCore;
			pCore = nullptr;
		}
	}
	catch( CVAException& e )
	{
		cerr << "[ VAServer  ][ Error ] " << e << endl;

		delete pServer;
		if( pCore )
			pCore->Finalize( );
		delete pCore;

		exit( 255 );
	}
	catch( ... )
	{
		delete pServer;
		if( pCore )
			pCore->Finalize( );
		delete pCore;

		cerr << "[ VAServer  ][ Error ] An unknown error occured" << endl;
		exit( 255 );
	}
}

class CServerCoreEventHandler : public IVAEventHandler
{
public:
	inline void HandleVAEvent( const CVAEvent* pEvent )
	{
		if( pEvent->iEventType == CVAEvent::SHOTDOWN_REQUEST )
			raise( SIGINT );
	};
};

void SignalHandler( int iSignum )
{
	StopServer( );
	exit( iSignum );
}

int main( int argc, char* argv[] )
{
	// Register interrupt signal handler
	signal( SIGABRT, SignalHandler );
	signal( SIGBREAK, SignalHandler ); // closing window
	signal( SIGTERM, SignalHandler );
	signal( SIGINT, SignalHandler ); // CTRL+C etc.

	// Arguments

	bool bVersionInfoRequest = false;
	bool bRemoteControlMode  = false;
	std::string sServerAddress, sVACoreConfigPath;

	if( argc == 1 )
	{
		sServerAddress = "0.0.0.0:12340";
		cout << "[ VAServer  ] No bind address and port given, using default: " << sServerAddress << endl;

		const std::string sDefaultConfigFileMain = "conf/VACore.ini";       // If started from VA main folder
		const std::string sDefaultConfigFileBin  = "../conf/VACore.ini";    // If started from bin folder (location of VAServer.exe)
		const std::string sDefaultConfigFileDev  = "../../conf/VACore.ini"; // If started from IDE (Developers only)
		if( doesFileExist( sDefaultConfigFileMain ) )
			sVACoreConfigPath = sDefaultConfigFileMain;
		else if( doesFileExist( sDefaultConfigFileBin ) )
			sVACoreConfigPath = sDefaultConfigFileBin;
		else if( doesFileExist( sDefaultConfigFileDev ) )
			sVACoreConfigPath = sDefaultConfigFileDev;

		if( !sVACoreConfigPath.empty( ) )
			cout << "[ VAServer  ] Using default configuration file '" << sVACoreConfigPath << "'." << endl;
	}
	else
	{
		if( argc == 2 )
		{
			const string sCommand( argv[1] );
			if( sCommand == "--version" || sCommand == "-v" || sCommand == "-version" )
				bVersionInfoRequest = true;
		}

		if( argc >= 2 )
			sServerAddress = argv[1];
		if( argc >= 3 )
			sVACoreConfigPath = argv[2];
		if( argc >= 4 )
			bRemoteControlMode = true;
	}


	try
	{
		int ec;
		if( sVACoreConfigPath.empty( ) )
		{
			CVAStruct oDefaultCoreConfig, oAudioDriver, oRenderer, oReproduction, oOutput, oOutputDevice;

			oAudioDriver["Driver"]             = "Portaudio";
			oAudioDriver["Device"]             = "default";
			oDefaultCoreConfig["Audio driver"] = oAudioDriver;

			oRenderer["Class"]                                      = "BinauralFreefield";
			oRenderer["Enabled"]                                    = true;
			oRenderer["Reproductions"]                              = "DefaultTalkthrough";
			oDefaultCoreConfig["Renderer:DefaultBinauralFreefield"] = oRenderer;

			oReproduction["Class"]                                = "Talkthrough";
			oReproduction["Enabled"]                              = true;
			oReproduction["Name"]                                 = "Default talkthrough reproduction";
			oReproduction["Outputs"]                              = "DefaultOutput";
			oDefaultCoreConfig["Reproduction:DefaultTalkthrough"] = oReproduction;

			oOutput["Devices"]                         = "DefaultStereoChannels";
			oOutput["Description"]                     = "Default stereo channels (headphones)";
			oDefaultCoreConfig["Output:DefaultOutput"] = oOutput;

			oOutputDevice["Type"]                                    = "HP";
			oOutputDevice["Description"]                             = "Default headphone hardware device (two-channels)";
			oOutputDevice["Channels"]                                = "1,2";
			oDefaultCoreConfig["OutputDevice:DefaultStereoChannels"] = oOutputDevice;

			pCore = VACore::CreateCoreInstance( oDefaultCoreConfig );
		}
		else
		{
			cout << "[ VAServer  ] Core configuration path: " << correctPath( sVACoreConfigPath ) << endl;
			pCore = VACore::CreateCoreInstance( sVACoreConfigPath );
		}

		// If only version requested, exit.
		if( bVersionInfoRequest )
		{
			CVAVersionInfo ver;
			pCore->GetVersionInfo( &ver );
			cout << "[ VAServer  ] " << ver.ToString( ) << endl;

			delete pCore;
			return 0;
		}

		pCore->Initialize( );

		if( bRemoteControlMode )
		{
			pServerCoreEventHandler = new CServerCoreEventHandler( );
			pCore->AttachEventHandler( pServerCoreEventHandler );
		}


		// Server
		pServer = IVANetServer::Create( );
		std::string sServer;

		// Set up the network connection
		int iPort;
		SplitServerString( sServerAddress, sServer, iPort );

		pServer->SetCoreInstance( pCore );

		if( ( ec = pServer->Initialize( sServer, iPort ) ) != 0 )
		{
			cerr << "[ VAServer  ][Error ] Failed to initialize network communication (errorcode " << ec << ")" << endl;
			return ec;
		}

		CVAVersionInfo ver;
		pCore->GetVersionInfo( &ver );

		cout << endl << "[ VAServer  ] Core version: " << ver.ToString( ) << endl;
		cout << "[ VAServer  ] Successfully started and listening on " << sServer << ":" << iPort << endl << endl;

		if( bRemoteControlMode )
		{
			cout << "[ VAServer  ] Enering remote control mode. Type CTRL+C to stop server manually or send shutdown message from client" << endl << endl;

			int iMilliseconds = 250;
			while( pCore != nullptr )
				VistaTimeUtils::Sleep( iMilliseconds );

			raise( SIGINT );
		}
		else
		{
			cout << "[ VAServer  ] Controls:\n"
			     << "[ VAServer  ]    m    Toggle output muting" << endl
			     << "[ VAServer  ]    +    Increase output gain by 3dB" << endl
			     << "[ VAServer  ]    0    Output gain 0dB" << endl
			     << "[ VAServer  ]    -    Decrease output gain by 3dB" << endl
			     << "[ VAServer  ]    r    Reset core" << endl
			     << "[ VAServer  ]    l    Circulate log levels" << endl
			     << "[ VAServer  ]    c    List connected clients" << endl
			     << "[ VAServer  ]    s    List search paths" << endl
			     << "[ VAServer  ]    q    Quit" << endl
			     << endl;


			while( true )
			{
				const int CTRL_D = 4;
				int c            = getch( );

				if( c == CTRL_D || c == 'q' )
				{
					raise( SIGINT );
					break;
				}

				if( (char)c == 'm' )
				{
					bool bMuted = pServer->GetCoreInstance( )->GetOutputMuted( );
					cout << "[ VAServer  ] " << ( !bMuted ? "Muting" : "Unmuting" ) << " global output" << endl;
					pServer->GetCoreInstance( )->SetOutputMuted( !bMuted );
				}

				if( (char)c == '0' )
				{
					cout << "[ VAServer  ] Setting output gain to 0 dB" << endl;
					pServer->GetCoreInstance( )->SetOutputGain( 1.0 );
				}

				if( (char)c == '+' )
				{
					double dGain = pServer->GetCoreInstance( )->GetOutputGain( );
					dGain *= 1.4125;
					cout << "[ VAServer  ] Setting output gain to " << ratio_to_db20_str( dGain ) << endl;
					pServer->GetCoreInstance( )->SetOutputGain( dGain );
				}

				if( (char)c == '-' )
				{
					double dGain = pServer->GetCoreInstance( )->GetOutputGain( );
					dGain /= 1.4125;
					cout << "[ VAServer  ] Setting output gain to " << ratio_to_db20_str( dGain ) << endl;
					pServer->GetCoreInstance( )->SetOutputGain( dGain );
				}

				if( (char)c == 'l' )
				{
					CVAStruct oArgs, oReturn, oNewArgs;
					oArgs["getloglevel"]    = true;
					oReturn                 = pServer->GetCoreInstance( )->CallModule( "VACore", oArgs );
					int iCurrentLogLevel    = oReturn["loglevel"];
					int iNewLogLevel        = int( iCurrentLogLevel + 1 ) % 6;
					oNewArgs["setloglevel"] = iNewLogLevel;
					oReturn                 = pServer->GetCoreInstance( )->CallModule( "VACore", oNewArgs );

					cout << "[ VAServer  ] Switched to log level: " << IVAInterface::GetLogLevelStr( iNewLogLevel ) << endl;
				}

				if( (char)c == 's' )
				{
					CVAStruct oArgs, oReturn;
					oArgs["getsearchpaths"] = true;
					oReturn                 = pServer->GetCoreInstance( )->CallModule( "VACore", oArgs );

					CVAStruct& oSearchPaths( oReturn["searchpaths"] );
					if( oSearchPaths.IsEmpty( ) )
						cout << "[ VAServer  ] No search paths added, yet" << endl;
					else
					{
						cout << "[ VAServer  ] Listing all search paths on server side:" << endl;
						CVAStruct::const_iterator cit = oSearchPaths.Begin( );
						while( cit != oSearchPaths.End( ) )
						{
							CVAStructValue oValue( cit->second );
							cit++;
							if( oValue.IsString( ) )
								cout << "[ VAServer  ]  + '" << std::string( oValue ) << "'" << endl;
						}
					}
				}

				if( (char)c == 'r' )
				{
					cout << "[ VAServer  ] Resetting server manually" << endl;
					pServer->GetCoreInstance( )->Reset( );
				}

				if( (char)c == 'c' )
				{
					if( !pServer->IsClientConnected( ) )
					{
						cout << "[ VAServer  ] Nothing yet, still waiting for connections" << endl;
					}
					else
					{
						const int iNumClients = pServer->GetNumConnectedClients( );
						if( iNumClients == 1 )
							cout << "[ VAServer  ] One client connected" << endl;
						else
							cout << "[ VAServer  ] Counting " << iNumClients << " connections" << endl;
						for( int i = 0; i < iNumClients; i++ )
						{
							cout << "[ VAServer  ]    +    " << pServer->GetClientHostname( i ) << endl;
						}
					}
				}
			}
		}

		delete pServer;
		pCore->Finalize( );
		delete pCore;
		delete pServerCoreEventHandler;
	}
	catch( CVAException& e )
	{
		cerr << "[ VAServer  ][ Error ] " << e << endl;

		delete pServer;
		if( pCore )
			pCore->Finalize( );
		delete pCore;

		return 255;
	}
	catch( ... )
	{
		delete pServer;
		if( pCore )
			pCore->Finalize( );
		delete pCore;

		cerr << "[ VAServer  ][ Error ] An unknown error occured" << endl;
		return 255;
	}

	return 0;
}
