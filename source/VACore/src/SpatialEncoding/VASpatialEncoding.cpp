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

#include "VASpatialEncoding.h"

#include "VAAmbisonicsEncoding.h"
#include "VABinauralEncoding.h"
#include "VAVBAPEncoding.h"
#include "../VAHardwareSetup.h"
#include "VBAP/VAVBAPLoudspeakerSetup.h"

#include <VAException.h>
#include <memory>


void CVASpatialEncodingFactory::RegisterLSSetupIfVBAP( const IVASpatialEncoding::Config& oConf, const CVAHardwareSetup& oHardwareSetup )
{
	if( oConf.eType == IVASpatialEncoding::EType::VBAP )
	{
		// Setup already initialized
		if( m_mLoudspeakerSetups.find( oConf.sVBAPLoudSpreakerSetup ) != m_mLoudspeakerSetups.end( ) )
			return;

		auto pHardwareOutput = oHardwareSetup.GetOutput( oConf.sVBAPLoudSpreakerSetup );
		if( pHardwareOutput == nullptr )
			VA_EXCEPT2( INVALID_PARAMETER, "Unrecognized loudspeaker setup '" + oConf.sVBAPLoudSpreakerSetup + "' for VBAP encoding" );

		m_mLoudspeakerSetups[oConf.sVBAPLoudSpreakerSetup] = std::make_unique<CVAVBAPLoudspeakerSetup>( *pHardwareOutput, oConf.v3VBAPSetupCenter, oConf.sVBAPTriangulationFile );
	}
}

std::unique_ptr<IVASpatialEncoding> CVASpatialEncodingFactory::Create( IVASpatialEncoding::Config oSpatialEncodingConf )
{
	switch( oSpatialEncodingConf.eType )
	{
		case IVASpatialEncoding::EType::Binaural:
			return std::make_unique<CVABinauralEncoding>( oSpatialEncodingConf );
		case IVASpatialEncoding::EType::Ambisonics:
			return std::make_unique<CVAAmbisonicsEncoding>( oSpatialEncodingConf );
		case IVASpatialEncoding::EType::VBAP:
		{
			if( m_mLoudspeakerSetups.find( oSpatialEncodingConf.sVBAPLoudSpreakerSetup ) == m_mLoudspeakerSetups.end( ) )
				VA_EXCEPT2( INVALID_PARAMETER, "Create(): VBAP loudspeaker setup is not properly initialized" );
			auto pVBAPLoudspeakerSetup = m_mLoudspeakerSetups[oSpatialEncodingConf.sVBAPLoudSpreakerSetup];
			return std::make_unique<CVAVBAPEncoding>( oSpatialEncodingConf, pVBAPLoudspeakerSetup );
		}
		default:
			VA_EXCEPT2( INVALID_PARAMETER, "It seems, a new spatial encoding type was added without adding a method to the respective factory" );
	}
}


int CVASpatialEncodingFactory::GetNumChannels( const IVASpatialEncoding::Config& oSpatialEncodingConf )
{
	switch( oSpatialEncodingConf.eType )
	{
		case IVASpatialEncoding::EType::Binaural:
			return 2;
		case IVASpatialEncoding::EType::Ambisonics:
			return ( oSpatialEncodingConf.iAmbisonicsOrder + 1 ) * ( oSpatialEncodingConf.iAmbisonicsOrder + 1 );
		case IVASpatialEncoding::EType::VBAP:
		{
			if( m_mLoudspeakerSetups.find( oSpatialEncodingConf.sVBAPLoudSpreakerSetup ) == m_mLoudspeakerSetups.end( ) )
				VA_EXCEPT2( INVALID_PARAMETER, "GetNumChannels(): VBAP loudspeaker setup is not properly initialized" );
			auto pVBAPLoudspeakerSetup = m_mLoudspeakerSetups[oSpatialEncodingConf.sVBAPLoudSpreakerSetup];
			return pVBAPLoudspeakerSetup->GetNumChannels( );
		}
		default:
			VA_EXCEPT2( INVALID_PARAMETER, "GetNumChannels(): Unknown value for spatial encoding type." );
	}
}
