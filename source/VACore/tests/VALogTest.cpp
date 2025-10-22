#include <VALog.h>
#include <Windows.h>
#include <iostream>

void testVALog( )
{
	std::cout << " * VALog test" << std::endl;

	CVALogItem oLogItem1( 0.0f, "VALogTest", "Component test of VALog ... first entry." );

	CVARealtimeLogStream* pRTLStream1 = new CVARealtimeLogStream( );
	pRTLStream1->SetName( "Realtime log stream test 1" );

	pRTLStream1->Printf( "Hula hoop 1.1" );

	CVARealtimeLogStream* pRTLStream2 = new CVARealtimeLogStream( );
	pRTLStream2->SetName( "Realtime Log Stream test 2" );

	pRTLStream2->Printf( "Hula hoop 2.1" );

	Sleep( (DWORD)1e3 );

	pRTLStream1->Printf( "Hula hoop 1.2" );
	pRTLStream1->Printf( "Hula hoop 1.3" );
	pRTLStream1->Printf( "Hula hoop 1.4" );

	Sleep( (DWORD)1e3 );

	delete pRTLStream1;

	pRTLStream2->Printf( "Hula hoop 2.2" );

	Sleep( (DWORD)1e3 );

	delete pRTLStream2;
}


int main( )
{
	testVALog( );

	return 0;
}
