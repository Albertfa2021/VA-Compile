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
#include <VistaBase/VistaTimeUtils.h>
#include <iostream>

using namespace std;

int main( int, char** )
{
	IVANetClient* pClient = IVANetClient::Create( );

	std::string sServerIP = "127.0.0.1";
	int iPort             = 12340;

	if( pClient->Initialize( sServerIP, iPort ) != IVANetClient::VA_NO_ERROR )
	{
		return -1;
	}

	pClient->SetExceptionHandlingMode( IVANetClient::EXC_SERVER_PRINT );

	int nState = pClient->GetCoreInstance( )->GetState( );
	if( nState != IVAInterface::VA_CORESTATE_READY )
	{
		cout << "Server ready but not audio streaming not started, initializing ... ";
		pClient->GetCoreInstance( )->Initialize( );
		cout << "done" << endl;
	}

	cout << "Type d,m,s,b,e,j,q + ENTER for test actions" << endl;

	bool bContinue = true;
	bool bMute     = false;
	while( bContinue )
	{
		char c = char( std::cin.get( ) );
		try
		{
			switch( c )
			{
				case 'q':
					bContinue = false;
					break;
				case 'd':
				{
					int iRet = pClient->GetCoreInstance( )->CreateDirectivityFromFile( "ITA_Kunstkopf.v17.ir.daff", "Test HRIR" );
					std::cout << "CreateDirectivityFromFile( 'ITA_Kunstkopf.v17.ir.daff', 'Test HRIR' ) = " << iRet << std::endl;
					break;
				}
				case 'm':
				{
					bMute = !bMute;
					pClient->GetCoreInstance( )->SetOutputMuted( bMute );
					std::cout << "SetOutputMuted( " << bMute << " ) called" << std::endl;
					bool bIsMuted = pClient->GetCoreInstance( )->GetOutputMuted( );
					std::cout << "GetOutputMuted() = " << bIsMuted << std::endl;
					break;
				}
				case 's':
				{
					auto id = pClient->GetCoreInstance( )->CreateSoundReceiver( );
					std::cout << "CreateSoundReceiver() called" << std::endl;
					pClient->GetCoreInstance( )->SetSoundReceiverPosition( id, VAVec3( 1, 2, 3 ) );
					std::cout << "SetSoundReceiverPosition( ... ) called" << std::endl;
					break;
				}
				case 'b':
				{
					pClient->GetCoreInstance( )->LockUpdate( );
					std::cout << "LockUpdate() called" << std::endl;
					break;
				}
				case 'e':
				{
					int iNum = pClient->GetCoreInstance( )->UnlockUpdate( );
					std::cout << "UnlockUpdate() = " << iNum << std::endl;
					break;
				}
				case 'j':
				{
					string sModuleID = "MyGenericRenderer";
					CVAStruct oArgs, oRet;

					oArgs["info"] = true;
					oRet          = pClient->GetCoreInstance( )->CallModule( sModuleID, oArgs );
					cout << oRet << endl;

					oArgs.Clear( );
					oArgs["listener"] = pClient->GetCoreInstance( )->CreateSoundReceiver( "PGP_test_listener" );
					oArgs["source"]   = pClient->GetCoreInstance( )->CreateSoundSource( "PGP_test_source" );
					std::vector<float> vfData( 900000 );
					for( size_t i = 0; i < vfData.size( ); i++ )
						vfData[i] = float( i / vfData.size( ) );
					oArgs["ch1"]     = CVAStructValue( &vfData[0], int( vfData.size( ) * sizeof( float ) ) );
					oArgs["ch2"]     = CVAStructValue( &vfData[0], int( vfData.size( ) * sizeof( float ) ) );
					oArgs["verbose"] = true;
					pClient->GetCoreInstance( )->SetRenderingModuleParameters( sModuleID, oArgs );
					std::cout << "Called SetRenderingModuleParameters( ... ) with some parameters" << std::endl;

					break;
				}
				default:
					break;
			}
		}
		catch( CVAException& oExc )
		{
			std::cerr << "Caught VA Exception: \n" << oExc << std::endl;
		}
	}

	pClient->Disconnect( );
	delete pClient;

	return 0;
}
