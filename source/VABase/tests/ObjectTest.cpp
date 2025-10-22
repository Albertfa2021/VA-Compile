#include <VAException.h>
#include <VAObject.h>
#include <VAObjectRegistry.h>
#include <VAStruct.h>
#include <iostream>

using namespace std;

class TestObject : public CVAObject
{
public:
	inline TestObject( ) : CVAObject( "TestObject" ) { };

	inline CVAStruct CallObject( const CVAStruct& ) { return CVAStruct( ); };

	inline int HandleMessage( const CVAStruct* pArgumentMessage, CVAStruct* pReturnMessage )
	{
		// No arguments => Do nothing
		if( !pArgumentMessage )
			return 0;

		const CVAStructValue* pCommand = pArgumentMessage->GetValue( "command" );

		// No command => Do nothing
		if( !pCommand )
			return 0;

		// Command must be a string
		if( !pCommand->IsString( ) )
			return -1;

		std::string sCommand = *pCommand;
		for( auto& c: sCommand )
			c = char( toupper( c ) );

		if( sCommand == "SAYHELLO" )
		{
			cout << "Object \"" << GetObjectName( ) << "\" says hello!" << endl;

			// No return values
			return 0;
		}

		if( sCommand == "PRINTID" )
		{
			cout << "Object \"" << GetObjectName( ) << "\" has ID " << GetObjectID( ) << endl;

			// No return values
			return 0;
		}

		if( sCommand == "TEST" )
		{
			cout << "Object \"" << GetObjectName( ) << "\" was called >test< " << endl;
			pReturnMessage->Clear( );
			( *pReturnMessage )["Number"] = 4711;
			return pReturnMessage->Size( );
		}

		// Invalid command
		return -1;
	};
};

int main( int, char** )
{
	try
	{
		CVAObjectRegistry reg;
		TestObject obj;

		reg.RegisterObject( &obj );
		cout << "Test object registered. Got ID " << obj.GetObjectID( ) << endl;

		// Search for object
		CVAObject* pObj = reg.GetObjectByName( "TestObject" );
		cout << pObj << endl;

		CVAStruct in, out;
		int result;

		in["command"] = "SayHello";
		result        = obj.HandleMessage( &in, &out );
		cout << "Result = " << result << endl;
		if( result != 0 )
			cout << "Return message = " << out << endl;

		in["command"] = "PrintID";
		result        = obj.HandleMessage( &in, &out );
		cout << "Result = " << result << endl;
		if( result != 0 )
			cout << "Return message = " << out << endl;

		in["command"] = "Test";
		result        = obj.HandleMessage( &in, &out );
		cout << "Result = " << result << endl;
		if( result != 0 )
			cout << "Return message = " << out << endl;
	}
	catch( CVAException& e )
	{
		cerr << "Error: " << e << endl;
		return e.GetErrorCode( );
	}

	return 0;
}
