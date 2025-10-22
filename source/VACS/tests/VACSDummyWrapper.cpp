#include "VACSDummyWrapper.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

using namespace std;


struct MyStruct
{
	bool bMyBool;
	int iMyInt;
	float fMyFloat;
	double dMyDouble;
};

class MyCppClass
{
public:
	int iDataSize;
};

static std::vector<char> g_vcBuffer;

int NativeSetBuffer( int iMaxDataSize, unsigned char* pBuf )
{
	cout << "Buffer max data size = " << iMaxDataSize << endl;

	g_vcBuffer.resize( 2 * sizeof( int ) );
	for( size_t i = 0; i < 2; i++ )
	{
		int* pIntCpp = (int*)( &g_vcBuffer[0] ) + i;
		int* pIntCs  = (int*)( pBuf ) + i;
		*pIntCpp     = 4 - i;
		cout << " C++ in " << i << " = " << *pIntCpp << endl;
		cout << " C# in " << i << " = " << *pIntCs << endl;
	}

	return 0;

	if( iMaxDataSize < g_vcBuffer.size( ) )
		return int( iMaxDataSize - g_vcBuffer.size( ) ); // Return negative (underrun value)

	int* pOutInt = (int*)( pBuf );
	*pOutInt     = -1;

	for( size_t i = 0; i < g_vcBuffer.size( ); i++ )
	{
		// pBuf[ i ] = g_vcBuffer[i];
		// char* pChar = ( char* ) ( pBuf + i );
		cout << "in byte = " << g_vcBuffer[i] << " out byte = " << pBuf[i] << endl;
	}


	return int( g_vcBuffer.size( ) );
}

int NativeGetBuffer( int iDataSize, const unsigned char* pBuf )
{
	cout << "Buffer data size = " << iDataSize << endl;

	g_vcBuffer.resize( iDataSize );
	for( size_t i = 0; i < g_vcBuffer.size( ); i++ )
	{
		g_vcBuffer[i] = pBuf[i];
		cout << "Data " << i << ": '" << g_vcBuffer[i] << "'" << endl;
	}

	int j = *(int*)( pBuf );
	cout << "j: " << j << endl;
	bool b = *(bool*)( pBuf + 0 );
	// cout << "b: " << b << endl;

	return 0;
}

bool NativeMyClassIO( const MyCppClass* pIn, MyCppClass* pOut )
{
	return false;
	// pOut->vcData.resize( pOut->iDataSize );
	cout << "In [" << pIn << "]: data size = " << pIn->iDataSize << endl;
	return true;
	cout << "Out (pre)  [" << pOut << "]: data size = " << pOut->iDataSize << endl;
	// pOut->iDataSize = pIn->iDataSize;
	cout << "Out (post) [" << pOut << "]: data size = " << pOut->iDataSize << endl;
	return true;
}

bool NativeMyStruct( const MyStruct* pIn, MyStruct* pOut )
{
	bool bPrintExtraInfo = false;
	if( bPrintExtraInfo )
	{
		cout << "C++ StructInP" << pIn << endl;
		cout << "C++ StructOutP" << pOut << endl;

		cout << "C++ C++ In Bool: " << pIn->bMyBool << endl;
		cout << "In Int: " << pIn->iMyInt << endl;
		cout << "C++ In Float: " << pIn->fMyFloat << endl;
		cout << "C++ In Double: " << pIn->dMyDouble << endl;

		cout << "C++ Out Bool pre: " << pOut->bMyBool << endl;
		cout << "C++ Out Int pre: " << pOut->iMyInt << endl;
		cout << "C++ Out Float pre: " << pOut->fMyFloat << endl;
		cout << "C++ Out Double pre: " << pOut->dMyDouble << endl;
	}

	// Assign
	pOut->bMyBool   = pIn->bMyBool;
	pOut->iMyInt    = pIn->iMyInt;
	pOut->fMyFloat  = pIn->fMyFloat;
	pOut->dMyDouble = pIn->dMyDouble;

	if( bPrintExtraInfo )
	{
		cout << "C++ Out Bool post: " << pOut->bMyBool << endl;
		cout << "C++ Out Int post: " << pOut->iMyInt << endl;
		cout << "C++ Out Float post: " << pOut->fMyFloat << endl;
		cout << "C++ Out Double post: " << pOut->dMyDouble << endl;
	}

	return true;
}

int NativeSetSampleBuffer( double* pSampleBuffer, int iNumSamples )
{
	if( iNumSamples <= 0 )
		throw std::exception( "Invalid number of samples, must be gerater zero" );

	std::vector<double> vdSampleBuffer( iNumSamples );
	for( size_t n = 0; n < vdSampleBuffer.size( ); n++ )
		vdSampleBuffer[n] = pSampleBuffer[n];

	return vdSampleBuffer.size( ); // Size of written samples
}

int NativeGetSampleBuffer( double* pSampleBuffer, int iNumRequestedSamples )
{
	if( iNumRequestedSamples <= 0 )
		throw std::exception( "Invalid number of requested samples, must be gerater zero" );

	std::vector<double> vdSampleBuffer( 123 );
	for( size_t n = 0; n < vdSampleBuffer.size( ); n++ )
		vdSampleBuffer[n] = n;

	for( size_t n = 0; n < std::min( (size_t)iNumRequestedSamples, vdSampleBuffer.size( ) ); n++ )
		pSampleBuffer[n] = vdSampleBuffer[n];

	return std::max( (size_t)0, vdSampleBuffer.size( ) - (size_t)iNumRequestedSamples );
}

double NativeSampleBuffer( MySample* pSampleBuffer, int iSize )
{
	double dTestResult = 0;

	MySample* pCur = pSampleBuffer;
	for( int i = 0; i < iSize; i++ )
		dTestResult += pCur++->dAmplitude;

	return dTestResult;
}

bool NativeMyFunction( const char* pcInput, char* pcOutput )
{
	string sInput( pcInput );
	string sOutputRaw( pcOutput );
	cout << sOutputRaw << endl;
	strcpy( pcOutput, sInput.c_str( ) );

	return true;
}

bool NativeMyIntVector( const std::vector<int>* pviInput, std::vector<int>* pviOutput )
{
	pviOutput = new std::vector<int>( );
	//	for( size_t i = 0; i< pviOutput->s )
	pviOutput->push_back( 99 );
	return true;
}

void NativeCauseException( )
{
	throw std::exception( "Native exception thrown." );
}
