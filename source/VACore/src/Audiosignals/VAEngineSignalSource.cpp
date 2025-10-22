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

#include "VAEngineSignalSource.h"

#include "../VAAudiostreamTracker.h"
#include "../VALog.h"
#include "../core/core.h"

#include <ITAConstants.h>
#include <ITAException.h>
#include <assert.h>
#include <math.h>
#include <sstream>

CVAEngineSignalSource::CVAEngineSignalSource( const CVAEngineSignalSource::Config& oConfig )
    : ITADatasourceRealization( 1, oConfig.pCore->GetCoreConfig( )->oAudioDriverConfig.dSampleRate, oConfig.pCore->GetCoreConfig( )->oAudioDriverConfig.iBuffersize )
    , m_oConfig( oConfig )
    , m_pAssociatedCore( oConfig.pCore )
{
	m_sbBuffer.Init( GetBlocklength( ), true );

	std::map<double, double>::const_iterator cit = oConfig.lFreqModesSpectrum.begin( );
	while( cit != oConfig.lFreqModesSpectrum.end( ) )
	{
		const double& dFrequency( cit->first );
		m_lFreqModesPhase.insert( std::pair<double, double>( dFrequency, 0.0f ) ); // second = phase information
		cit++;
	}

	m_fK = 1.0f; // idle engine
}

CVAEngineSignalSource::~CVAEngineSignalSource( ) {}

int CVAEngineSignalSource::GetType( ) const
{
	return IVAAudioSignalSource::VA_SS_ENGINE;
}

std::string CVAEngineSignalSource::GetTypeString( ) const
{
	return "engine";
}

std::string CVAEngineSignalSource::GetDesc( ) const
{
	return std::string( "Creates the modal spectrum of a rotating engine" );
}

IVAInterface* CVAEngineSignalSource::GetAssociatedCore( ) const
{
	return m_pAssociatedCore;
}

std::vector<const float*> CVAEngineSignalSource::GetStreamBlock( const CVAAudiostreamState* )
{
	m_sbBuffer.Zero( );
	double dOmega;
	double dK = m_fK;

	std::map<double, double>::const_iterator cit = m_oConfig.lFreqModesSpectrum.begin( );
	while( cit != m_oConfig.lFreqModesSpectrum.end( ) )
	{
		const double& dBaseFrequency( cit->first );
		const double& dAmplitude( cit->second );
		const double dPeriodSamples = dK * GetSampleRate( ) / dBaseFrequency;
		double& dPhase( m_lFreqModesPhase[dBaseFrequency] );
		dOmega = ITAConstants::TWO_PI_F / dPeriodSamples; // 2*pi/T

		for( unsigned int i = 0; i < GetBlocklength( ); i++ )
		{
			m_sbBuffer[i] += float( dAmplitude * sin( dOmega * i + dPhase ) );
		}

		dPhase = fmod( double( GetBlocklength( ) * dOmega + dPhase ), double( ITAConstants::TWO_PI_F ) );

		cit++;
	}

	return { m_sbBuffer.data( ) };
}

void CVAEngineSignalSource::HandleRegistration( IVAInterface* pCore )
{
	m_pAssociatedCore = pCore;
}

void CVAEngineSignalSource::HandleUnregistration( IVAInterface* )
{
	m_pAssociatedCore = NULL;
}

void CVAEngineSignalSource::SetParameters( const CVAStruct& oParams )
{
	const CVAStructValue* pStruct;

	if( ( pStruct = oParams.GetValue( "PRINT" ) ) != nullptr )
	{
		if( pStruct->GetDatatype( ) != CVAStructValue::STRING )
			VA_EXCEPT2( INVALID_PARAMETER, "Print value must be a string" );
		std::string sValue = toUppercase( *pStruct );
		if( sValue == "HELP" )
		{
			VA_PRINT( "Available commands: 'print' 'help|status'; 'get' 'K'; 'set' 'K' 'value' <double>" );
			return;
		}

		if( sValue == "STATUS" )
		{
			VA_PRINT( "Engine number K: " << m_fK );
			return;
		}
	}
	else if( ( pStruct = oParams.GetValue( "SET" ) ) != nullptr )
	{
		if( pStruct->GetDatatype( ) != CVAStructValue::STRING )
			VA_EXCEPT2( INVALID_PARAMETER, "Setter key name must be a string" );

		std::string sValue = toUppercase( *pStruct );
		if( sValue == "K" )
		{
			pStruct = oParams.GetValue( "VALUE" );
			if( ( pStruct->GetDatatype( ) == CVAStructValue::DOUBLE ) || ( pStruct->GetDatatype( ) == CVAStructValue::INT ) ||
			    ( pStruct->GetDatatype( ) == CVAStructValue::BOOL ) )
			{
				double dK = *pStruct;
				if( dK > 0 )
					SetEngineNumber( dK );
				else
					VA_EXCEPT2( INVALID_PARAMETER, "Engine number K must be greater than zero" );
			}
			return;
		}
	}
	else
	{
		VA_EXCEPT2( INVALID_PARAMETER, "Could not interpret parameters" );
	}

	return;
}

void CVAEngineSignalSource::SetEngineNumber( const double& dK )
{
	assert( dK > 0.0f );
	m_fK = float( dK );
}

CVAStruct CVAEngineSignalSource::GetParameters( const CVAStruct& ) const
{
	CVAStruct oRet;
	oRet["k"] = m_fK;

	return oRet;
}
