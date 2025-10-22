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

#ifndef IW_VANET_VISTA_COMPABILITY
#define IW_VANET_VISTA_COMPABILITY

#include <VistaBase/VistaBaseTypes.h>
#include <VistaBase/VistaVersion.h>

namespace VANetCompat
{
	typedef VistaType::byte byte;
	typedef VistaType::sint32 sint32;
	typedef VistaType::uint64 uint64;
	typedef char BBSetBuffer;
	typedef unsigned char BBGetBuffer;
}; // namespace VANetCompat

#endif // IW_VANET_VISTA_COMPABILITY
