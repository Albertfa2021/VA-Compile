#include <ITAConstants.h>
#include <VA.h>
#include <VACore.h>
#include <VistaBase/VistaTimeUtils.h>
#include <iostream>

using namespace std;

IVAInterface* core = NULL;

void TestHATO( )
{
	// Load directivity / HATO-compatible HRTF
	// int iHATOHRTFID = core->CreateDirectivityFromFile( "$(DefaultHRIR)" );
	int iHATOHRTFID = core->CreateDirectivityFromFile( "FABIAN_HATO_5x5x5_256_44100Hz.v17.ir.daff" );

	// Set up scene
	int iListenerID = core->CreateSoundReceiver( "Listener with HATO HRTF" );
	core->SetSoundReceiverPosition( iListenerID, VAVec3( 0, 1.7, 0 ) );
	core->SetSoundReceiverDirectivity( iListenerID, iHATOHRTFID );

	int iSourceID = core->CreateSoundSource( "Source" );
	core->SetSoundSourcePosition( iSourceID, VAVec3( 3, 1.7, 0 ) ); // from right direction on horizontal plane

	// Load input data & start playback
	string sFileSignalID = core->CreateSignalSourceBufferFromFile( "$(DemoSound)" );
	core->SetSignalSourceBufferPlaybackAction( sFileSignalID, IVAInterface::VA_PLAYBACK_ACTION_PLAY );
	core->SetSignalSourceBufferLooping( sFileSignalID, true );
	core->SetSoundSourceSignalSource( iSourceID, sFileSignalID );

	// Rotate head (above torso)
	for( float fAngleDeg = -60.0f; fAngleDeg <= 60.0f; fAngleDeg += 5.0f )
	{
		VistaTimeUtils::Sleep( 0.5e3 ); // timeout
		cout << "HATO angle is now " << fAngleDeg << " degree" << endl;

		// @todo: quaternions verstehen und entsprechend rotation der HAT orientierung setzen ... ab hier steht potenzieller quatsch!

		const double dAngleQuaternionValue = sin( fAngleDeg / 180.0f * 2.0f * ITAConstants::PI_D );

		// Set pose for rendering modules
		core->SetSoundReceiverOrientation( iListenerID, VAQuat( 0, 1, 0, 0 ) );
		core->SetSoundReceiverHeadAboveTorsoOrientation( iListenerID, VAQuat( 0, 1, 0, dAngleQuaternionValue ) );

		// Set pose for reproduction modules
		core->SetSoundReceiverRealWorldPose( iListenerID, VAVec3( 0, 1.7, 0 ), VAQuat( 0, 1, 0, 0 ) );
		core->SetSoundReceiverRealWorldHeadAboveTorsoOrientation( iListenerID, VAQuat( 0, 1, 0, dAngleQuaternionValue ) );
	}
}

int main( )
{
	try
	{
		core = VACore::CreateCoreInstance( "../conf/VACore.ini" );
		core->Initialize( );

		core->AddSearchPath( "../data" );
		TestHATO( );

		core->Finalize( );
	}
	catch( CVAException& e )
	{
		cerr << "Error: " << e << endl;
		int iErrorCode = e.GetErrorCode( );
		delete core;

		return iErrorCode;
	}

	delete core;

	return 0;
}
