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

#include "VAMatlabConnection.h"

#include "VAMatlabTracking.h"

#include <VA.h>
#include <VANet.h>

CVAMatlabConnection::CVAMatlabConnection( )
{
	pClient          = IVANetClient::Create( );
	pCoreInterface   = NULL;
	pVAMatlabTracker = new CVAMatlabTracker;
}

CVAMatlabConnection::~CVAMatlabConnection( )
{
	if( pVAMatlabTracker->IsConnected( ) )
		pVAMatlabTracker->Uninitialize( );

	delete pVAMatlabTracker;
	pVAMatlabTracker = NULL;

	delete pClient;
	pClient = NULL;
}