/*
 *  --------------------------------------------------------------------------------------------
 *
 *    VVV        VVV A
 *     VVV      VVV AAA        Virtual Acoustics (VA)
 *      VVV    VVV   AAA       Real-time auralisation for virtual reality
 *       VVV  VVV     AAA
 *        VVVVVV       AAA     (c) Copyright Institute of Technical Acoustics (ITA)
 *         VVVV         AAA        RWTH Aachen University (http://www.akustik.rwth-aachen.de)
 *
 *  --------------------------------------------------------------------------------------------
 */

#ifndef INCLUDE_WATCHER_VA_CS_DUMMY_WRAPPER
#define INCLUDE_WATCHER_VA_CS_DUMMY_WRAPPER

#include <VACSDefinitions.h>

/**
 * A dummy wrapper library to investigate how marshaling between unmanaged C++ and C# works.
 *
 */

struct MyStruct;
class MyCppClass;

struct MySample
{
	double dTime;
	double dAmplitude;
};


extern "C"
{
	//! Rears a string and writes it back as call by reference
	VACS_API double NativeSampleBuffer( MySample* pMySampleBuffer, int iSize );
	VACS_API int NativeSetSampleBuffer( double* vdSampleBuffer, int iNumSamples );
	VACS_API int NativeGetSampleBuffer( double* vdSampleBuffer, int iNumRequestedSamples );
	VACS_API bool NativeMyFunction( const char* pcInput, char* pcOutput );
	VACS_API bool NativeMyStruct( const MyStruct* pIn, MyStruct* pOut );
	VACS_API bool NativeMyClassIO( const MyCppClass* pIn, MyCppClass* pOut );
	VACS_API int NativeGetBuffer( int iDataSize, const unsigned char* pBuf );
	VACS_API int NativeSetBuffer( int iMaxDataSize, unsigned char* pBuf );
	VACS_API void NativeCauseException( );
}

#endif // INCLUDE_WATCHER_VA_CS_DUMMY_WRAPPER
