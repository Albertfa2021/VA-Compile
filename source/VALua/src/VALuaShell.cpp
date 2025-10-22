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

#include "VALuaShell.h"

#include "VALuaShellImpl.h"
#include <sstream>

IVALuaShell* IVALuaShell::Create()
{
	return new CVALuaShellImpl();
}

std::string IVALuaShell::GetVersionStr()
{
	/* Format
	#define VALUA_SVN_DATE     "$Date: 2012-06-26 16:37:30 +0200 (Di, 26 Jun 2012) $"
	#define VALUA_SVN_REVISION "$Revision: 2740 $"
	*/

	std::string sDate(VALUA_SVN_DATE);
	sDate = sDate.substr(7, 10);

	std::string sRevision(VALUA_SVN_REVISION);
	sRevision = sRevision.substr(11, sRevision.length()-13);

	std::stringstream ss;
	ss << VALUA_VERSION_MAJOR << "." << VALUA_VERSION_MINOR << " (Rev. " << sRevision << ")";
	return ss.str();
}

IVALuaShell::~IVALuaShell()
{

}
