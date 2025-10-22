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

#include "VADeviceInputSignalSource.h"

#include <ITASampleBuffer.h>

CVADeviceInputSignalSource::CVADeviceInputSignalSource( ITASampleBuffer* psbInputData, const std::string& sDesc )
    : m_pAssociatedCore( nullptr )
    , m_psbInputData( psbInputData )
    , m_sDesc( sDesc )
{
}

CVADeviceInputSignalSource::~CVADeviceInputSignalSource( ) {}

int CVADeviceInputSignalSource::GetType( ) const
{
	return IVAAudioSignalSource::VA_SS_DEVICE_INPUT;
}

std::string CVADeviceInputSignalSource::GetTypeString( ) const
{
	return "audioinput";
}

std::string CVADeviceInputSignalSource::GetDesc( ) const
{
	return m_sDesc;
}

std::string CVADeviceInputSignalSource::GetStateString( ) const
{
	return "";
}

IVAInterface* CVADeviceInputSignalSource::GetAssociatedCore( ) const
{
	return m_pAssociatedCore;
}

void CVADeviceInputSignalSource::HandleRegistration( IVAInterface* pParentCore )
{
	m_pAssociatedCore = pParentCore;
}

void CVADeviceInputSignalSource::HandleUnregistration( IVAInterface* )
{
	m_pAssociatedCore = nullptr;
}

std::vector<const float*> CVADeviceInputSignalSource::GetStreamBlock( const CVAAudiostreamState* )
{
	// Return samples from external buffer, no unternal processing required
	return { m_psbInputData->data( ) };
}
