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

#include <VAVersion.h>
#include <sstream>

std::string CVAVersionInfo::ToString( ) const
{
	// Output formatting: "VACore 1.24 [FLAGS] (COMMENT)"
	std::stringstream ss;
	ss << "VACore " << sVersion;
	if( !sFlags.empty( ) )
		ss << " [" << sFlags << "]";
	if( !sComments.empty( ) )
		ss << " (" << sComments << ")";
	return ss.str( );
}
