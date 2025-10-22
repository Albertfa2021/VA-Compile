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

#include "VADebug.h"

#include "../VALog.h"

#include <ITACriticalSection.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef WIN32

#	include <windows.h>

// Ausgabe in Visual Studio? Andernfalls Konsole (stdout)
#	define DEBUG_PRINTF_MSVC

#endif

#define DEBUG_PRINTF_BUFSIZE 16384

// Lock für den Puffer in den Ausgabe-Routinen
static ITACriticalSection g_csDebugPrintf;
static char g_pszDebugPrintfBuf[16384];

void VA_DEBUG_PRINTF( const char* format, ... )
{
	VistaMutexLock oLock( VALog_getOutputStreamMutex( ) );

#ifdef DEBUG_PRINTF_MSVC
	g_csDebugPrintf.enter( );
	va_list args;
	va_start( args, format );
	vsprintf_s( g_pszDebugPrintfBuf, DEBUG_PRINTF_BUFSIZE, format, args );
	va_end( args );

	OutputDebugStringA( g_pszDebugPrintfBuf );
	g_csDebugPrintf.leave( );
#else
	va_list args;
	va_start( args, format );
	vfprintf( stdout, format, args );
	va_end( args );
#endif
}

void VA_DEBUG_PRINTF( int, int, const char* format, ... )
{
	VistaMutexLock oLock( VALog_getOutputStreamMutex( ) );

#ifdef DEBUG_PRINTF_MSVC
	g_csDebugPrintf.enter( );
	va_list args;
	va_start( args, format );
	vsprintf_s( g_pszDebugPrintfBuf, DEBUG_PRINTF_BUFSIZE, format, args );
	va_end( args );

	OutputDebugStringA( g_pszDebugPrintfBuf );
	g_csDebugPrintf.leave( );
#else
	va_list args;
	va_start( args, format );
	vfprintf( stdout, format, args );
	va_end( args );
#endif
}
