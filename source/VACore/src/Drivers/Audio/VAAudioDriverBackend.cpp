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

#include "VAAudioDriverBackend.h"

#include <ITAException.h>
#include <ITAStringUtils.h>

#ifdef VACORE_WITH_AUDIO_BACKEND_ASIO
#	include "VAASIOBackend.h"
#endif

//// Singleton mittels statischer Instanz
// static CVAAudioDriverArchBackendRegistry theRegistry;
//
// CVAAudioDriverArchBackendRegistry* CVAAudioDriverArchBackendRegistry::getInstance()
//{
//	return &theRegistry;
//}
//
// IVAAudioDriverBackend* CVAAudioDriverArchBackendRegistry::requestBackend(const std::string& sBackendName)
//{
//	/*
//	 *  Wichtig: Hier werden alle Audiotreiber-Backends eingetragen!
//	 */
//
//	IVAAudioDriverBackend* pBackend=nullptr;
//
//	if (toUppercase(sBackendName) == "ASIO")
//		pBackend = new CVAASIOBackend;
//
//	if (!pBackend)
//		ITA_EXCEPT1(INVALID_PARAMETER, "Invalid driver architecture name or driver architecture not supported");
//
//	return pBackend;
//}
