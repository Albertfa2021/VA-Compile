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

#include <VANetVersion.h>
#include <sstream>

std::string CVANetVersionInfo::ToString( ) const
{
	// Output formatting: "VANet v2016.a (COMMENT)"
	std::stringstream ss;
	ss << "VANet " << sVersion;
	if( !sComments.empty( ) )
		ss << " (" << sComments << ")";
	return ss.str( );
}

void GetVANetVersionInfo( CVANetVersionInfo* pVersionInfo )
{
	if( !pVersionInfo )
		return;

	std::stringstream ss;
	ss << VANET_VERSION_MAJOR << "." << VANET_VERSION_MINOR;
	pVersionInfo->sVersion = ss.str( );

#ifdef NDEBUG
	pVersionInfo->sComments = "release";
#else
	pVersionInfo->sComments = "debug";
#endif
}
