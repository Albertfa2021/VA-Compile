#include <VAException.h>
#include <VASamples.h>
#include <VAStruct.h>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <map>
#include <sstream>

using namespace std;


int main( )
{
	std::map<std::string, int> map;

	try
	{
		std::vector<char> vcSomeData( 1e7 );
		vcSomeData[0]                      = 1;
		vcSomeData[vcSomeData.size( ) - 1] = -1;
		{
			CVAStruct s;
			CVAStructValue v = CVAStructValue( &vcSomeData[0], sizeof( char ) * vcSomeData.size( ) );
			s["ch1"]         = v;
		}

		CVAStruct s;
		s["a"]                                         = CVAStruct( );
		s[string( "a" )][string( "b" )]                = 24;
		s[string( "a" )][string( "c" )]                = CVAStruct( );
		s[string( "a" )][string( "c" )][string( "d" )] = "Hui";

		s.GetValue( "a/c/d", '/' );
		cout << s << endl;

		// Iteration example
		const CVAStruct& t = s["a"];
		auto cit           = t.Begin( );
		while( cit != t.End( ) )
		{
			std::string sKey             = cit->first;
			const CVAStructValue& oValue = cit->second;
			cout << sKey << ": " << oValue << std::endl;
			++cit;
		}

		for( size_t i = 0; i < 10e2; i++ )
		{
			CVAStruct s;
			CVAStructValue v = CVAStructValue( &vcSomeData[0], sizeof( char ) * vcSomeData.size( ) );
			s["ch1"]         = v;
		}

		CVASampleBuffer oBuf( 1024 );
		oBuf.GetData( )[0]    = -1.0f;
		oBuf.GetData( )[1023] = 1023.0f;
		CVAStructValue v( (void*)oBuf.GetData( ), oBuf.GetNumSamples( ) * sizeof( float ) );
		s["BRIR_S0_R0"] = v;

		CVASampleBuffer oBuf2( 1024, false );
		float* pvData = (float*)(void*)s["BRIR_S0_R0"];

		for( size_t i = 0; i < oBuf2.GetNumSamples( ); i++ )
			oBuf2.GetData( )[i] = pvData[i];
	}
	catch( CVAException& e )
	{
		cerr << "Error: " << e << endl;
		return e.GetErrorCode( );
	}

	return 0;
}
