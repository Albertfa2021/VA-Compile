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
#include <VACore.h>
#include <VANet.h>
#include <VistaBase/VistaTimeUtils.h>
#include <iostream>

using namespace std;

int main( int, char** )
{
	IVANetServer* pServer = IVANetServer::Create( );

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

	pServer->SetCoreInstance( VACore::CreateCoreInstance( oDefaultCoreConfig ) );

	std::string sServerIP = "0.0.0.0";
	int iPort             = 12340;
	IVANetServer::tPortList liFreePorts;
	liFreePorts.push_back( IVANetServer::tPortRange( 12430, 12480 ) );

	if( pServer->Initialize( sServerIP, iPort, liFreePorts ) != IVANetServer::VA_NO_ERROR )
	{
		cerr << "Could not initialize server" << endl;
		return 255;
	}

	while( true )
	{
		cout << "Sleeping for a while now ... it CTRL+C to abort." << endl;
		VistaTimeUtils::Sleep( 2000 );
	}

	return 0;
}
