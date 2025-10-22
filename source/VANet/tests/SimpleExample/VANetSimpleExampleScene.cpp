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

// @sa http://www.virtualacoustics.org/overview.html

#include <VA.h>
#include <VANet.h>
#include <iostream>
#include <string>

using namespace std;

int main( int, char** )
{
	IVANetClient* pVANet = IVANetClient::Create( );
	pVANet->Initialize( "localhost" );

	if( !pVANet->IsConnected( ) )
		return 255;

	IVAInterface* pVA = pVANet->GetCoreInstance( );

	try
	{
		if( pVA->GetState( ) != IVAInterface::VA_CORESTATE_READY )
			pVA->Initialize( );

		pVA->Reset( );

		const std::string sSignalSourceID = pVA->CreateSignalSourceBufferFromFile( "ita_demosound.wav" );
		pVA->SetSignalSourceBufferPlaybackAction( sSignalSourceID, IVAInterface::VA_PLAYBACK_ACTION_PLAY );
		pVA->SetSignalSourceBufferLooping( sSignalSourceID, true );

		const int iSoundSourceID = pVA->CreateSoundSource( "C++ Sound Source" );
		pVA->SetSoundSourcePose( iSoundSourceID, VAVec3( -2.0f, 1.7f, -2.0f ), VAQuat( 0.0f, 0.0f, 0.0f, 1.0f ) );

		pVA->SetSoundSourceSignalSource( iSoundSourceID, sSignalSourceID );

		const int iHRIR = pVA->CreateDirectivityFromFile( "NeumannKU100.v17.ir.daff" );

		const int iSoundReceiverID = pVA->CreateSoundReceiver( "C++ Listener" );
		pVA->SetSoundReceiverPose( iSoundReceiverID, VAVec3( 0.0f, 1.7f, 0.0f ), VAQuat( 0.0f, 0.0f, 0.0f, 1.0f ) );
		pVA->SetSoundReceiverDirectivity( iSoundReceiverID, iHRIR );

		// do something that suspends the program ...
	}
	catch( CVAException& e )
	{
		cerr << "Caught a VA exception on server side: " << e << endl;
	}

	pVANet->Disconnect( );
	delete pVANet;

	return 0;
}
