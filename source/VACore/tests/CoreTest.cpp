#include <VA.h>
#include <VACore.h>
#include <VistaBase/VistaTimeUtils.h>
#include <iostream>

using namespace std;

IVAInterface* va = NULL;

class EventBasedGenericRenderingController : public IVAEventHandler
{
public:
	inline void HandleVAEvent( const CVAEvent* pEvent )
	{
		if( pEvent->iEventType & CVAEvent::SOUND_RECEIVER_CHANGED_POSE )
		{
			cout << "[ListenerUpdate] " << pEvent->ToString( );

			// Determine new file path for filter update
			std::string sNewFilePath;
			VAVec3 vNewListenerView = pEvent->vView;
			VAVec3 vNewListenerUp   = pEvent->vUp;
			if( true ) // Some geometric expression
				sNewFilePath = "$(VADataDir)/rooms/shoebox_room_brir_azi020_ele000.wav";

			CVAStruct oArgs, oReturn;
			oArgs["COMMAND"]    = "UPDATE";
			oArgs["TYPE"]       = "FILE"; // or DATA that sends ITASampleFrame from this controller context
			oArgs["FilePath"]   = sNewFilePath;
			oArgs["ListenerID"] = pEvent->iObjectID;
			oArgs["SourceID"]   = 1;
			try
			{
				oReturn = va->CallModule( "GenericPath:MyGenericRenderer", oArgs );
			}
			catch( CVAException& e )
			{
				cerr << e << endl;
			}
		}
	}
};
EventBasedGenericRenderingController rendering_controller;


class CoreEventDumper : public IVAEventHandler
{
public:
	inline void HandleVAEvent( const CVAEvent* pEvent )
	{
		// Filter example
		if( pEvent->iEventType & ~CVAEvent::MEASURES_UPDATE )
			cout << "[CoreEventDumper] " << pEvent->ToString( );
	}
};
CoreEventDumper dumper;

void bugfix_scene_pool_clearance_issue( );

void TestModuleInterface( );
void AudioRenderers( );
void AudioReproductions( );

void CreateHRIRs( );
void CreateAudioSignals( );
void CreateScene( );
void ConnectSignals( );
void TestSignalAndSourceConnections( );

int main( int, char** )
{
	try
	{
		va = VACore::CreateCoreInstance( "../conf/VACore.ini" );
		// va = VACore::CreateCoreInstance( GetCoreConfig() );

		// Attach (register) event handler
		// va->AttachEventHandler( &dumper );
		va->AttachEventHandler( &rendering_controller );

		CVAVersionInfo ver;
		va->GetVersionInfo( &ver );
		cout << ver.ToString( ) << endl;

		va->Initialize( );

		va->AddSearchPath( "../data" );

		/*
		TestModuleInterface();
		AudioRenderers();
		AudioReproductions();

		CreateHRIRs();
		CreateAudioSignals();
		CreateScene();
		//ConnectSignals();
		TestSignalAndSourceConnections();
		*/

		bugfix_scene_pool_clearance_issue( );

		va->SetGlobalAuralizationMode( IVAInterface::VA_AURAMODE_ALL );

		VistaTimeUtils::Sleep( 10000 );

		va->Finalize( );
	}
	catch( CVAException& e )
	{
		cerr << "Error: " << e << endl;
		int iErrorCode = e.GetErrorCode( );
		delete va;

		return iErrorCode;
	}

	delete va;

	return 0;
}

void AudioRenderers( )
{
	std::vector<CVAAudioRendererInfo> voRenderers;
	va->GetRenderingModules( voRenderers );
	if( voRenderers.size( ) )
		cout << "Found " << voRenderers.size( ) << " audio renderer" << endl;

	for( size_t i = 0; i < voRenderers.size( ); i++ )
	{
		cout << " + Renderer #" << i + 1 << ": " << voRenderers[i].sID << " [" << voRenderers[i].sClass << "] " << ( voRenderers[i].bEnabled ? "[enabled]" : "disabled" )
		     << " ( " << voRenderers[i].sDescription << " )" << endl;
	}
}

void AudioReproductions( )
{
	std::vector<CVAAudioReproductionInfo> voReproductions;
	va->GetReproductionModules( voReproductions );
	if( voReproductions.size( ) )
		cout << "Found " << voReproductions.size( ) << " audio renderer" << endl;

	for( size_t i = 0; i < voReproductions.size( ); i++ )
	{
		cout << " + Renderer #" << i + 1 << ": " << voReproductions[i].sID << " [" << voReproductions[i].sClass << "] "
		     << ( voReproductions[i].bEnabled ? "[enabled]" : "disabled" ) << " ( " << voReproductions[i].sDescription << " )" << endl;
	}
}

void TestModuleInterface( )
{
	std::vector<CVAModuleInfo> viModuleInfos;
	va->GetModules( viModuleInfos );

	if( viModuleInfos.size( ) )
		cout << "Found " << viModuleInfos.size( ) << " modules" << endl;

	for( size_t i = 0; i < viModuleInfos.size( ); i++ )
	{
		cout << " + Module #" << i + 1 << ": " << viModuleInfos[i].sName << " ( " << viModuleInfos[i].sDesc << " )" << endl;
	}
}

CVAStruct GetCoreConfig( )
{
	CVAStruct oConfig;

	CVAStruct oSectionDebug;
	oSectionDebug["loglevel"] = 5;
	oConfig["debug"]          = oSectionDebug;

	CVAStruct oSectionDriver;
	oSectionDriver["driver"] = "Portaudio";
	oConfig["audio driver"]  = oSectionDriver;

	CVAStruct oDevice1;
	oDevice1["type"]             = "HP";
	oDevice1["channels"]         = "1,2";
	oConfig["OutputDevice:MyHP"] = oDevice1;

	CVAStruct oOutput1;
	oOutput1["devices"]           = "MyHP";
	oConfig["Output:MyDesktopHP"] = oOutput1;

	CVAStruct oReproduction1;
	oReproduction1["class"]             = "Talkthrough";
	oReproduction1["outputs"]           = "MyDesktopHP";
	oConfig["Reproduction:Talkthrough"] = oReproduction1;

	CVAStruct oRenderer1;
	oRenderer1["class"]              = "BinauralFreeField";
	oRenderer1["Reproductions"]      = "MyTalkthroughHeadphones";
	oConfig["Renderer:BFF_CoreTest"] = oRenderer1;

	CVAStruct oRenderer2;
	oRenderer2["class"]               = "PrototypeGenericPath";
	oRenderer2["numchannels"]         = 2;
	oRenderer2["Reproductions"]       = "MyTalkthroughHeadphones";
	oConfig["Renderer:PTGP_CoreTest"] = oRenderer2;

	return oConfig;
}

void CreateAudioSignals( )
{
	cout << "Connecting to network stream signal source on localhost:12480" << endl;
	string sNetStreamID;
	try
	{
		sNetStreamID = va->CreateSignalSourceNetworkStream( "localhost", 12480, "CoreTestNetStream" );
	}
	catch( CVAException& e )
	{
		cerr << "Caught exception during stream signal source connection:" << e << endl;
	}

	string sFileID = va->CreateSignalSourceBufferFromFile( "$(DemoSound)" );
	va->SetSignalSourceBufferPlaybackAction( sFileID, IVAInterface::VA_PLAYBACK_ACTION_PLAY );
	va->SetSignalSourceBufferLooping( sFileID, true );
}

void CreateHRIRs( )
{
	va->CreateDirectivityFromFile( "$(DefaultHRIR)" );
}

void CreateScene( )
{
	int iListenerID = va->CreateSoundReceiver( "MyListener" );
	va->SetSoundReceiverAuralizationMode( iListenerID, IVAInterface::VA_AURAMODE_ALL );
	va->SetSoundReceiverOrientationVU( iListenerID, VAVec3( 0.7172f, 0.0f, -0.7172f ), VAVec3( 0, 1, 0 ) );
	va->SetSoundReceiverDirectivity( iListenerID, 1 );

	int iSoundSource = va->CreateSoundSource( "MySoundSource" );
	va->SetSoundSourcePosition( iSoundSource, VAVec3( -2.0f, 0, 0 ) );
}

void TestSignalAndSourceConnections( )
{
	int iSourceID    = va->CreateSoundSource( );
	string sSignalID = va->CreateSignalSourceBufferFromFile( "$(DemoSound)" );

	va->SetSoundSourceSignalSource( iSourceID, sSignalID );
	va->SetSoundSourceSignalSource( iSourceID, "" );
	va->SetSoundSourceSignalSource( iSourceID, sSignalID );
	va->SetSoundSourceSignalSource( iSourceID, sSignalID );
	va->SetSoundSourceSignalSource( iSourceID, sSignalID );
	va->SetSoundSourceSignalSource( iSourceID, sSignalID );
	va->SetSoundSourceSignalSource( iSourceID, "" );
	va->DeleteSignalSource( sSignalID );

	sSignalID = va->CreateSignalSourceBufferFromFile( "$(DemoSound)" );
	va->DeleteSignalSource( sSignalID );

	sSignalID         = va->CreateSignalSourceBufferFromFile( "$(DemoSound)" );
	string sSignalID2 = va->CreateSignalSourceBufferFromFile( "$(DemoSound)" );
	va->SetSoundSourceSignalSource( iSourceID, sSignalID );
	va->SetSoundSourceSignalSource( iSourceID, sSignalID2 );
	try
	{
		va->DeleteSignalSource( sSignalID2 );
	}
	catch( CVAException e )
	{
		; // good
	}
	va->DeleteSoundSource( iSourceID );
	va->DeleteSignalSource( sSignalID2 );
}

void ConnectSignals( )
{
	// va->SetSoundSourceSignalSource( 1, "audiofile1" );
	va->SetSoundSourceSignalSource( 1, "netstream1" );
}

void testLoadDirectivity( )
{
	cout << "Loading a directivity" << endl;

	cout << "Load 1: ID = " << va->CreateDirectivityFromFile( "D:/Temp/Directivity/trompete1.ddb", "Trompete1" ) << endl;
	//
	//// Test: Gleiches Sound nochmal laden...
	// cout << "Load 2: ID = " << (id = va->LoadSound("ding.wav", "Max")) << endl;

	// cout << "Load 3: ID = " << va->LoadSound("D:/ding.wav", "Max") << endl;

	// va->PrintDirectivityInfos();

	// cout << "Freeing Sound " << id << endl;
	// va->FreeSound(id);

	// va->PrintDirectivityInfos();
}

void testLoadSound( )
{
	cout << "Loading a Sound" << endl;

	int id1, id2, id3;

	const std::string sSequencerID = va->CreateSignalSourceSequencer( "MySequencer" );

	CVAStruct oSequencerSample;
	oSequencerSample["name"]     = "Max";
	oSequencerSample["filepath"] = "ding.wav";
	cout << "Load 1: ID = " << ( id1 = va->AddSignalSourceSequencerSample( sSequencerID, oSequencerSample ) ) << endl;
	// Test: Gleiches Sound nochmal laden...
	cout << "Load 2: ID = " << ( id2 = va->AddSignalSourceSequencerSample( sSequencerID, oSequencerSample ) ) << endl;

	oSequencerSample["name"]     = "Moritz";
	oSequencerSample["filepath"] = "dong.wav";
	cout << "Load 3: ID = " << ( id3 = va->AddSignalSourceSequencerSample( sSequencerID, oSequencerSample ) ) << endl;

	// va->PrintSoundInfos();

	cout << "Freeing Sound " << id1 << endl;
	va->RemoveSignalSourceSequencerSample( sSequencerID, id1 );

	// va->PrintSoundInfos();
}

void testScene( )
{
	cout << "Creating a sound source" << endl;

	int iSource1, iSource2, iSource3;

	iSource1 = va->CreateSoundSource( "Source1" );
	// va->SetSoundSourceName(iSource1, "Max der Erste");
	cout << "Create sound source 1: ID = " << iSource1 << endl;

	va->LockUpdate( );

	iSource2 = va->CreateSoundSource( "Source2" );
	// va->SetSoundSourceName(iSource2, "Klaus die Katze");
	cout << "Create sound source 1: ID = " << iSource2 << endl;

	iSource3 = va->CreateSoundSource( "Source3" );
	cout << "Create sound source 1: ID = " << iSource3 << endl;


	// Positions & orientations
	va->SetSoundSourceOrientationVU( iSource1, VAVec3( 1, 2, 3 ), VAVec3( 4, 5, 6 ) );
	va->SetSoundSourcePosition( iSource2, VAVec3( 47, 11, 0 ) );
	// va->SetSoundSourceOrientationYPR(iSource3, 47, 11, 0);

	// cout << "Create listener 1: ID = " << va->CreateListener("Listener1") << endl;
	va->UnlockUpdate( );

	VAVec3 v3Pos;
	VAQuat qOrient;
	va->GetSoundSourcePose( iSource1, v3Pos, qOrient );
	cout << "Sound source 1 pos & ori: p=" << v3Pos << ", q=" << qOrient << endl;

	va->SetSoundSourceName( iSource1, "Klaus die Katze" );
	cout << "Get sound source name source 1: name = " << va->GetSoundSourceName( iSource1 ) << endl;

	cout << "Set sound source volume source 1: volume = 1.23" << endl;
	va->SetSoundSourceSoundPower( iSource1, 1.23e-3 );

	va->LockUpdate( );
	int iListener1 = va->CreateSoundReceiver( "Lauschermann" );
	cout << "Create listener 1: ID = " << iListener1 << endl;

	va->SetSoundReceiverPose( iListener1, VAVec3( 0, -1, 1 ), VAQuat( 0, 0, 1, 1 ) );
	va->UnlockUpdate( );

	cout << "Delete sound source ID=1: result = " << va->DeleteSoundSource( 1 ) << endl;

	// va->PrintSceneState();
}

void testReentrantSync( )
{
	cout << "Single locking" << endl;
	va->LockUpdate( );
	va->UnlockUpdate( );

	cout << "Double locking" << endl;
	va->LockUpdate( );
	va->LockUpdate( );
	va->UnlockUpdate( );
	va->UnlockUpdate( );

	cout << "Over unlocking" << endl;
	va->UnlockUpdate( );
}

void bugfix_scene_pool_clearance_issue( )
{
	for( auto i: std::vector<int>( 100 ) )
	{
		va->Reset( );
		int iID = va->CreateSoundSource( );

		for( auto i: std::vector<int>( 100 ) )
			va->SetSoundSourcePose( iID, VAVec3( ), VAQuat( ) );

		va->DeleteSoundSource( iID );
	}
}