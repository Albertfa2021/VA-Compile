#define _USE_MATH_DEFINES

#include <VA.h>
#include <VACore.h>
#include <VistaBase/VistaTimeUtils.h>
#include <Windows.h>
#include <cassert>
#include <iostream>
#include <math.h>
#include <stdlib.h>
#include <vector>


IVAInterface* va = NULL;
CVAStruct timingStruct;
CVAStruct trigger;
int nSources = 100;

std::vector<std::string> ssIDs;
std::vector<int> viSourceIDs( nSources );

std::vector<std::string> GetFileNamesInDirectory( std::string directory )
{
	std::vector<std::string> files;
	WIN32_FIND_DATA fileData;
	HANDLE hFind;

	if( !( ( hFind = FindFirstFile( directory.c_str( ), &fileData ) ) == INVALID_HANDLE_VALUE ) )
	{
		while( FindNextFile( hFind, &fileData ) )
		{
			files.push_back( fileData.cFileName );
		}
	}

	FindClose( hFind );
	return files;
}

void SetupScene( )
{
	for( int i = 0; i < nSources; ++i )
	{
		float azimuth   = ( ( rand( ) % 3600 ) / 1800.0 ) * M_PI;
		float elevation = ( ( rand( ) % 900 ) / 1800.0 ) * M_PI;

		float x = sin( elevation ) * cos( azimuth ) * 5;
		float y = cos( elevation ) * 5;
		float z = sin( azimuth ) * sin( elevation ) * 5;

		std::string source_name = "itaVA_Source_" + std::to_string( i );
		viSourceIDs[i]          = va->CreateSoundSource( source_name );

		va->SetSoundSourceSignalSource( viSourceIDs[i], ssIDs[rand( ) % ssIDs.size( )] );

		va->SetSoundSourcePosition( viSourceIDs[i], VAVec3( x, 1.7 + y, z ) );
	}
}

void Scene2( )
{
	for( int i = 0; i < nSources; ++i )
	{
		float z = static_cast<float>( rand( ) ) / ( static_cast<float>( RAND_MAX / 24 ) );

		std::string source_name = "itaVA_Source_" + std::to_string( i );
		viSourceIDs[i]          = va->CreateSoundSource( source_name );

		va->SetSoundSourceSignalSource( viSourceIDs[i], ssIDs[rand( ) % ssIDs.size( )] );

		if( i % 2 == 0 )
		{
			va->SetSoundSourcePosition( viSourceIDs[i], VAVec3( -5, 1.7, -z ) );
		}
		else
		{
			va->SetSoundSourcePosition( viSourceIDs[i], VAVec3( 5, 1.7, -z ) );
		}
	}
}

void Test( )
{
	std::string source_name = "itaVA_Source";
	viSourceIDs[0]          = va->CreateSoundSource( source_name );

	va->SetSoundSourceSignalSource( viSourceIDs[0], ssIDs[rand( ) % ssIDs.size( )] );

	va->SetSoundSourcePosition( viSourceIDs[0], VAVec3( -5, 1.7, 0 ) );
}


int main( )
{
	int numsteps = 6000;

	double clock    = 0;
	double timestep = 64 / 44100.;

	try
	{
		va = VACore::CreateCoreInstance( "BinauralClusteringRendererTest.VACore.ini" );
		va->Initialize( );

		va->AddSearchPath( "X:/Sciebo/master-thesis/input_files" );
		va->AddSearchPath( "D:/Users/stienen/dev/VA/VACore/data" );
		va->AddSearchPath( "D:/Users/stienen/Sciebo/ITA/Lehre/Masterarbeiten/2018 Lucas Mösch/2018 MA Lucas Mösch Auralization/input_files" );
		va->AddSearchPath( "D:/Users/jonas/sciebo/ITA/Lehre/Masterarbeiten/2018 Lucas Mösch/2018 MA Lucas Mösch Auralization/input_files" );
		va->AddSearchPath( "C:/Users/jonas/sciebo/ITA/Lehre/Masterarbeiten/2018 Lucas Mösch/2018 MA Lucas Mösch Auralization/input_files" );


		std::vector<std::string> vFileNames =
		    GetFileNamesInDirectory( "D:/Users/stienen/Sciebo/ITA/Lehre/Masterarbeiten/2018 Lucas Mösch/2018 MA Lucas Mösch Auralization/input_files/*.wav" );

		int receiverID    = va->CreateSoundReceiver( "ITAListener" );
		int directivityID = va->CreateDirectivityFromFile( "ITA_Artificial_Head_5x5_44kHz_128.v17.ir.daff", "HRTF" );
		va->SetSoundReceiverDirectivity( receiverID, directivityID );
		va->SetSoundReceiverPosition( receiverID, VAVec3( 0, 1.7, 0 ) );

		for( auto const& value: vFileNames )
		{
			std::string ssID = va->CreateSignalSourceBufferFromFile( value );
			va->SetSignalSourceBufferPlaybackAction( ssID, 2 );
			va->SetSignalSourceBufferLooping( ssID, true );
			va->SetSignalSourceBufferPlaybackPosition( ssID, static_cast<float>( rand( ) ) / ( static_cast<float>( RAND_MAX / 2 ) ) );
			ssIDs.push_back( ssID );
		}

		assert( ssIDs.size( ) > 0 );

		// SetupScene();
		Scene2( );
		// Test();

		va->SetCoreClock( 0 );

		for( int i = 0; i < numsteps; i++ )
		{
			va->SetSoundReceiverPosition( receiverID, VAVec3( 0, 1.7, -clock * 3 ) );
			clock += timestep;
			timingStruct["time"] = clock;
			va->CallModule( "manualclock", timingStruct );

			trigger["trigger"] = true;
			va->CallModule( "virtualaudiodevice", trigger );

			std::cout << clock << std::endl;
		}

		va->Finalize( );
	}
	catch( CVAException& e )
	{
		std::cerr << "Error: " << e << std::endl;
		int iErrorCode = e.GetErrorCode( );
		delete va;
		VistaTimeUtils::Sleep( 60000 );
		return iErrorCode;
	}

	VistaTimeUtils::Sleep( 1000 );

	return 0;
}
