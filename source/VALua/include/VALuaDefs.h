/*
 *  --------------------------------------------------------------------------------------------
 *
 *    VVV        VVV A           Virtual Acoustics (VA) | http://www.virtualacoustics.org
 *     VVV      VVV AAA          Licensed under the Apache License, Version 2.0
 *      VVV    VVV   AAA
 *       VVV  VVV     AAA        Copyright 2015-2021
 *        VVVVVV       AAA       Institute of Technical Acoustics (ITA)
 *         VVVV         AAA      RWTH Aachen University
 *
 *  --------------------------------------------------------------------------------------------
 */

#ifndef IW_VALUA_DEFS
#define IW_VALUA_DEFS

#if ( defined WIN32 ) && ( !( defined VALUA_STATIC ) || !( defined VA_STATIC ) )
#ifdef VALUA_EXPORTS
#define VALUA_API __declspec( dllexport )
#else
#define VALUA_API __declspec( dllimport )
#endif
#else
#define VALUA_API
#endif

// Disable STL template-instantiiation warning with DLLs for Visual C++
#if defined (_MSC_VER)
#pragma warning(disable: 4251)
#endif

// --= Version =-----------------------------------------------

#define VALUA_VERSION_MAJOR 1
#define VALUA_VERSION_MINOR 21

#define VALUA_SVN_DATE     "$Date: 2012-06-26 15:42:59 +0200 (Di, 26 Jun 2012) $"
#define VALUA_SVN_REVISION "$Revision: 2734 $"

#endif // IW_VALUA_DEFS
