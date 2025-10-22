#include <VA.h>
#include <VACore.h>
#include <VistaBase/VistaTimeUtils.h>
#include <iostream>

using namespace std;

IVAInterface* pCore = NULL;

const int g_iBlockLength   = 128;
const double g_dSampleRate = 44100.0f;


//! Trigger virtual audio device block increment in audio processing thread
/**
 * A special prototype call to control the virtual audio device and increment a block
 * of the audio processing by user. Produces one more block of audio samples and increments
 * the pCore time by user-defined time steps.
 */
void TriggerVirtualAudioDeviceBlockIncrement( );

//! Manually update the core clock
/**
 * ... used for scene modification time stamps. This lets the user control the
 * asynchronous update beaviour independent from the audio processing  thread.
 */
void ManualCoreClockUpdate( const double );

void TestOfflineSimulationVirtualAudioDevice( )
{
	// Load directivity
	int iHRTFID = pCore->CreateDirectivityFromFile( "ITA_Artificial_Head_5x5_44kHz_128.v17.ir.daff" );

	// Set up scene
	int iListenerID = pCore->CreateSoundReceiver( "Listener with HRTF" );
	pCore->SetSoundReceiverPosition( iListenerID, VAVec3( 0, 1.7, 0 ) );
	pCore->SetSoundReceiverDirectivity( iListenerID, iHRTFID );

	int iSourceID = pCore->CreateSoundSource( "Source" );

	// Load input data & start playback
	string sFileSignalID = pCore->CreateSignalSourceBufferFromFile( "WelcomeToVA.wav" );
	pCore->SetSignalSourceBufferPlaybackAction( sFileSignalID, IVAInterface::VA_PLAYBACK_ACTION_PLAY );
	pCore->SetSignalSourceBufferLooping( sFileSignalID, true );
	pCore->SetSoundSourceSignalSource( iSourceID, sFileSignalID );


	// --- Simulation loop using time steps and synchronized scene & audio updates --- //

	const int iNumSteps             = 3445;                                     // will produce 10 seconds of scene updates & audio processing
	const double dTimeStepIncrement = double( g_iBlockLength ) / g_dSampleRate; // Set to audio stream block length for synced example
	double dManualCoreTime          = 0.0f;

	// Motion
	float fXPos                = -100.0f;
	const float fXPosIncrement = 0.05f;

	for( int n = 0; n < iNumSteps; n++ )
	{
		pCore->SetSoundSourcePosition( iSourceID, VAVec3( fXPos, 1.7f, -2.0f ) );
		fXPos += fXPosIncrement;

		dManualCoreTime += dTimeStepIncrement;
		ManualCoreClockUpdate( dManualCoreTime ); // Scene clock increment

		TriggerVirtualAudioDeviceBlockIncrement( ); // Audio processing increment
	}
}

//! Creates an example configuration for two-channel offline simulation and recording
CVAStruct CreateOfflineSimulationVirtualDeviceConfig( )
{
	CVAStruct oConfig, oAudioDevice;

	CVAStruct oSectionDebug;
	oSectionDebug["loglevel"] = IVAInterface::VA_LOG_LEVEL_VERBOSE;
	oConfig["debug"]          = oSectionDebug;

	CVAStruct oSectionDriver;
	oSectionDriver["driver"] = "Virtual";

	// Default user-triggered settings
	oSectionDriver["Device"]         = "Trigger";
	oSectionDriver["BufferSize"]     = g_iBlockLength;
	oSectionDriver["SampleRate"]     = g_dSampleRate;
	oSectionDriver["OutputChannels"] = 2;

	// Timeout settings
	// oSectionDriver[ "Device" ] = "Timout";
	// oSectionDriver[ "UpdateRate" ] = 300;

	oConfig["audio driver"] = oSectionDriver;

	CVAStruct oDevice1;
	oDevice1["type"]             = "HP";
	oDevice1["channels"]         = "1,2";
	oConfig["OutputDevice:MyHP"] = oDevice1;

	CVAStruct oOutput1;
	oOutput1["devices"]           = "MyHP";
	oConfig["Output:MyDesktopHP"] = oOutput1;

	CVAStruct oReproduction1;
	oReproduction1["class"]                         = "Talkthrough";
	oReproduction1["outputs"]                       = "MyDesktopHP";
	oConfig["Reproduction:MyTalkthroughHeadphones"] = oReproduction1;

	CVAStruct oRenderer1;
	oRenderer1["class"]                           = "BinauralFreeField";
	oRenderer1["Reproductions"]                   = "MyTalkthroughHeadphones";
	oRenderer1["RecordOutputEnabled"]             = true;
	oRenderer1["RecordOutputFilePath"]            = "OfflineSimulationVirtualDeviceTest.wav";
	oConfig["Renderer:BFF_OfflineSimulationTest"] = oRenderer1;

	return oConfig;
}

void TriggerVirtualAudioDeviceBlockIncrement( )
{
	CVAStruct oTriggerMessage;
	oTriggerMessage["trigger"] = true;
	CVAStruct oAnswer          = pCore->CallModule( "VirtualAudioDevice", oTriggerMessage );
}

void ManualCoreClockUpdate( const double dNewCoreClockTimeSeconds )
{
	CVAStruct oCoreClockIncrement;
	oCoreClockIncrement["time"] = dNewCoreClockTimeSeconds;
	CVAStruct oAnswer           = pCore->CallModule( "manualclock", oCoreClockIncrement );
}

int main( )
{
	try
	{
		CVAStruct opCoreConfig = CreateOfflineSimulationVirtualDeviceConfig( );
		pCore                  = VACore::CreateCoreInstance( opCoreConfig );
		pCore->Initialize( );

		pCore->AddSearchPath( "../data" );
		TestOfflineSimulationVirtualAudioDevice( );

		CVAStruct oManualReset;
		oManualReset["prepare_manual_reset"] = true;
		pCore->SetRenderingModuleParameters( "BFF_OfflineSimulationTest", oManualReset );

		pCore->Finalize( ); // Executes reset, which results in deadlock of no audio streaming thread active
	}
	catch( CVAException& e )
	{
		cerr << "Error: " << e << endl;
		int iErrorCode = e.GetErrorCode( );
		delete pCore;

		return iErrorCode;
	}

	delete pCore;

	return 0;
}
