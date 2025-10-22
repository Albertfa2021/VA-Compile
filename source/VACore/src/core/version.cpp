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

#include "core.h"

void CVACoreImpl::GetVersionInfo( CVAVersionInfo* pVersionInfo ) const
{
	if( !pVersionInfo )
		return;

	std::stringstream ss;
	ss << VACORE_VERSION_MAJOR << "." << VACORE_VERSION_MINOR;
	pVersionInfo->sVersion = ss.str( );
	ss.clear( );
#ifdef VACORE_CMAKE_DATE
	ss << VACORE_CMAKE_DATE;
#else
	ss << "Unkown date";
#endif
	pVersionInfo->sDate  = ss.str( );
	pVersionInfo->sFlags = "";

#ifdef DEBUG
	pVersionInfo->sComments = "debug";
#else
	pVersionInfo->sComments = "release";
#endif
}
