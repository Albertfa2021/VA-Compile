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

#include "VANetClientImpl.h"

#include <VANetClient.h>

IVANetClient* IVANetClient::Create( )
{
	return new CVANetClientImpl( );
}

IVANetClient::IVANetClient( ) {}

IVANetClient::~IVANetClient( ) {}
