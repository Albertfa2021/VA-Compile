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

#include "VADirectivityDAFFHATOHRIR.h"

#include "../VALog.h"

#include <DAFF.h>
#include <ITANumericUtils.h>
#include <ITASampleFrame.h>
#include <ITAStringUtils.h>
#include <VAException.h>
#include <cassert>
#include <math.h>
#include <sstream>

CVADirectivityDAFFHATOHRIR::CVADirectivityDAFFHATOHRIR( const std::string& sFilename, const std::string& sName, const double dDesiredSamplerate )
    : m_sName( sName )
    , m_pReader( nullptr )
{
	m_iType = IVADirectivity::DAFF_HATO_HRIR;

	// Der DAFF-Reader wirft std::exceptions
	m_pReader = DAFFReader::create( );

	int iError = m_pReader->openFile( sFilename );
	if( iError != DAFF_NO_ERROR )
	{
		delete m_pReader;
		std::string sErrorStr = DAFFUtils::StrError( iError );
		VA_EXCEPT1( std::string( "Could not load HRIR dataset from file \"" ) + sFilename + std::string( "\". " ) + sErrorStr + std::string( "." ) );
	}

	if( m_pReader->getContentType( ) != DAFF_IMPULSE_RESPONSE )
	{
		delete m_pReader;
		VA_EXCEPT1( std::string( "The file \"" ) + sFilename + std::string( "\" does not contain impulse response content." ) );
	}

	m_pContent = dynamic_cast<DAFFContentIR*>( m_pReader->getContent( ) );

	if( m_pContent->getSamplerate( ) != dDesiredSamplerate )
	{
		delete m_pReader;
		VA_EXCEPT1( std::string( "The file \"" ) + sFilename + std::string( "\" does not have the required sampling rate of " ) + DoubleToString( dDesiredSamplerate ) +
		            std::string( " Hz." ) );
	}

	// Sicherheitshalber, auch wenn was paranoid ...
	if( m_pContent->getMaxEffectiveFilterLength( ) == 0 )
	{
		delete m_pReader;
		VA_EXCEPT1( std::string( "The file \"" ) + sFilename + std::string( "\" contains empty filters." ) );
	}

	m_pMetadata = m_pReader->getMetadata( );

	m_fLatency = 0.0f;

	// Filter-Latenz
	if( !m_pMetadata->hasKey( "DELAY_SAMPLES" ) )
	{
		VA_WARN( "DirectivityDAFFIR", std::string( "The file \"" ) + sFilename + std::string( "\" is missing the meta tag DELAY_SAMPLES." ) );
	}
	else
	{
		if( ( m_pMetadata->getKeyType( "DELAY_SAMPLES" ) != DAFFMetadata::DAFF_INT ) && ( m_pMetadata->getKeyType( "DELAY_SAMPLES" ) != DAFFMetadata::DAFF_FLOAT ) )
		{
			delete m_pReader;
			VA_EXCEPT1( std::string( "In file \"" ) + sFilename + std::string( "\": " ) + std::string( " The meta tag DELAY_SAMPLES must be numeric." ) );
		}
		m_fLatency = (float)m_pMetadata->getKeyFloat( "DELAY_SAMPLES" );
	}

	if( m_fLatency < 0 )
	{
		delete m_pReader;
		VA_EXCEPT1( std::string( "In file \"" ) + sFilename + std::string( "\": " ) + std::string( " The meta tag DELAY_SAMPLES must be positive." ) );
	}

	m_iMinOffset    = m_pContent->getMinEffectiveFilterOffset( );
	m_iFilterLength = m_pContent->getFilterLength( );

	// Warnung, falls keine Vollkugel
	if( !m_pReader->getProperties( )->coversFullSphere( ) )
		VA_WARN( "DirectivityDAFFIR", "The HRIR dataset file " << sFilename << " does not cover all directions" );

	// Eigenschaften definieren
	m_oProps.sFilename          = sFilename;
	m_oProps.sName              = sName;
	m_oProps.iFilterLength      = m_iFilterLength;
	m_oProps.fFilterLatency     = m_fLatency;
	m_oProps.bFullSphere        = m_pReader->getProperties( )->coversFullSphere( );
	m_oProps.bSpaceDiscrete     = true;
	m_oProps.bDistanceDependent = false;

	if( m_pReader->getProperties( )->getNumberOfChannels( ) % 2 == 1 )
		VA_EXCEPT1( std::string( "In file \"" ) + sFilename + std::string( "\": odd number detected for HATO HRIR DAFF file" ) );
	m_iNumHATODirections = m_pReader->getProperties( )->getNumberOfChannels( ) / 2;

	if( !m_pMetadata->hasKey( "HATO_START_DEG" ) )
		VA_EXCEPT2( INVALID_PARAMETER,
		            std::string( "In file \"" ) + sFilename + std::string( "\": missing 'HATO_START_DEG' metadata key, can't use this DAFF file for a HATO HRIR" ) );
	if( !m_pMetadata->hasKey( "HATO_END_DEG" ) )
		VA_EXCEPT2( INVALID_PARAMETER,
		            std::string( "In file \"" ) + sFilename + std::string( "\": missing 'HATO_END_DEG' metadata key, can't use this DAFF file for a HATO HRIR" ) );

	m_dHATOStartDeg = m_pMetadata->getKeyFloat( "HATO_START_DEG" );
	m_dHATOEndDeg   = m_pMetadata->getKeyFloat( "HATO_END_DEG" );
}

CVADirectivityDAFFHATOHRIR::~CVADirectivityDAFFHATOHRIR( )
{
	if( m_pReader )
		m_pReader->closeFile( );

	delete m_pReader;
}

std::string CVADirectivityDAFFHATOHRIR::GetFilename( ) const
{
	return m_pReader->getFilename( );
}

std::string CVADirectivityDAFFHATOHRIR::GetName( ) const
{
	return m_sName;
}

std::string CVADirectivityDAFFHATOHRIR::GetDesc( ) const
{
	char buf[1024];
	sprintf( buf, "DAFF, 2D, %s, discrete %0.0fx%0.0f°, %d taps", ( m_oProps.bFullSphere ? "full sphere" : "partial sphere" ),
	         m_pContent->getProperties( )->getAlphaResolution( ), m_pContent->getProperties( )->getBetaResolution( ), m_oProps.iFilterLength );
	return buf;
}

const CVAHRIRProperties* CVADirectivityDAFFHATOHRIR::GetProperties( ) const
{
	return &m_oProps;
}

void CVADirectivityDAFFHATOHRIR::GetNearestSpatialNeighbourIndex( const float fAzimuthDeg, const float fElevationDeg, int* piIndex, bool* pbOutOfBounds ) const
{
	assert( m_pContent );

	if( !piIndex )
		return;

	bool bOutOfBounds;
	int iDAFFIndex;
	m_pContent->getNearestNeighbour( DAFF_OBJECT_VIEW, fAzimuthDeg, fElevationDeg, iDAFFIndex, bOutOfBounds );

	if( piIndex )
		*piIndex = iDAFFIndex;

	if( pbOutOfBounds )
		*pbOutOfBounds = bOutOfBounds;
}

int CVADirectivityDAFFHATOHRIR::GetHATOChannelIndexLeft( const double dHATODeg ) const
{
	// Convert index to artificial HATO index, @todo daniel
	double dHATODegProjected = dHATODeg > 180.0f ? dHATODeg - 360.0f : dHATODeg;

	if( dHATODegProjected > m_dHATOEndDeg )
		dHATODegProjected = m_dHATOEndDeg;
	if( dHATODegProjected < m_dHATOStartDeg )
		dHATODegProjected = m_dHATOStartDeg;

	if( dHATODegProjected == 0.0f )
		return 0;

	int iNumHATODirections = m_pContent->getProperties( )->getNumberOfChannels( ) / 2;
	assert( iNumHATODirections > 2 ); // laut Kommentar mindestens 6 HATO Channels. Falls vorher schon geprüft, überflüssig.

	double dHATORes   = ( m_dHATOEndDeg - m_dHATOStartDeg ) / ( iNumHATODirections - 1 );
	double dDeviation = fmod( dHATODegProjected, dHATORes ); // bin nicht sicher ob math.h (für fmod) bereits includiert, daher habe ich das oben hinzugefügt.

	double dNearestHATODeg = .0f;
	if( dDeviation < ( dHATORes / 2 ) )
		dNearestHATODeg = dHATODegProjected - dDeviation;
	else
		dNearestHATODeg = dHATODegProjected + dHATORes - dDeviation;

	if( dNearestHATODeg == 0.0f )
		return 0;

	int iResult;
	if( dNearestHATODeg > 0 )
		iResult = (int)( ( m_dHATOEndDeg + dNearestHATODeg ) / dHATORes );
	else
		iResult = (int)( ( m_dHATOEndDeg + dNearestHATODeg ) / dHATORes ) + 1;

	return 2 * iResult; // even! channel index (starting at 0)
}

int CVADirectivityDAFFHATOHRIR::GetHATOChannelIndexRight( const double dHATODeg ) const
{
	return GetHATOChannelIndexLeft( dHATODeg ) + 1; // odd! channel index (indexing starts at 0, first right channels is 1)
}

void CVADirectivityDAFFHATOHRIR::GetHRIRByIndexAndHATO( ITASampleFrame* psfDest, const int iIndex, const double dHATODeg ) const
{
	assert( m_pContent );

	if( psfDest->channels( ) > m_pReader->getProperties( )->getNumberOfChannels( ) )
		VA_EXCEPT1( std::string( "CVADirectivityDAFFHATOHRIR::GetHRIRByIndex - Target SampleFrame contains more channels than HRIR database" ) );

	const int iHATOChannelIndexL = GetHATOChannelIndexLeft( dHATODeg );
	int iResult                  = -1;

	iResult = m_pContent->getFilterCoeffs( iIndex, iHATOChannelIndexL, ( *psfDest )[0].data( ) );
	if( iResult != DAFF_NO_ERROR )
		VA_EXCEPT1( DAFFUtils::StrError( iResult ) );

	const int iHATOChannelIndexR = GetHATOChannelIndexRight( dHATODeg );
	iResult                      = m_pContent->getFilterCoeffs( iIndex, iHATOChannelIndexR, ( *psfDest )[1].data( ) );
	if( iResult != DAFF_NO_ERROR )
		VA_EXCEPT1( DAFFUtils::StrError( iResult ) );
}

DAFFContentIR* CVADirectivityDAFFHATOHRIR::GetDAFFContent( ) const
{
	return m_pContent;
}
