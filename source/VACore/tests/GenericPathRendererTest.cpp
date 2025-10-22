#include <VA.h>
#include <VACore.h>
#include <iostream>

using namespace std;

IVAInterface* core = NULL;

void TestModuleInterface( )
{
	std::vector<CVAModuleInfo> viModuleInfos;
	core->GetModules( viModuleInfos );

	if( viModuleInfos.size( ) )
		cout << "Found " << viModuleInfos.size( ) << " modules" << endl;

	for( size_t i = 0; i < viModuleInfos.size( ); i++ )
	{
		cout << " + Module #" << i + 1 << ": " << viModuleInfos[i].sName << " ( " << viModuleInfos[i].sDesc << " )" << endl;
	}
}

int main( )
{
	try
	{
		core = VACore::CreateCoreInstance( );
		core->Initialize( );

		TestModuleInterface( );

		int iListenerID = core->CreateSoundReceiver( "MyGPListener" );
		core->SetSoundReceiverPosition( iListenerID, VAVec3( 0, 0, 0 ) );
		int iSourceID = core->CreateSoundSource( "MyGPSoundSource" );
		core->SetSoundSourcePosition( iSourceID, VAVec3( 0, 0, -2 ) );

		cout << "q+Enter to close." << endl;
		char cIn;
		cin >> cIn;

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
