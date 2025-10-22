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

#include "VADirectivityDAFFHRIR.h"

#include "../VALog.h"

#include <DAFF.h>
#include <ITANumericUtils.h>
#include <ITASampleFrame.h>
#include <ITAStringUtils.h>
#include <VAException.h>
#include <cassert>
#include <sstream>

CVADirectivityDAFFHRIR::CVADirectivityDAFFHRIR( const std::string& sFilename, const std::string& sName, const double dDesiredSamplerate )
    : m_sName( sName )
    , m_pReader( nullptr )
{
	m_iType = IVADirectivity::DAFF_HRIR;

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
	assert( m_pContent );

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

	// Latency compensation
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

	if( !m_pReader->getProperties( )->coversFullSphere( ) )
		VA_WARN( "DirectivityDAFFIR", "The HRIR dataset file " << sFilename << " does not cover all directions" );

	if( m_pReader->getProperties( )->getNumberOfChannels( ) < 2 )
		VA_EXCEPT1( std::string( "In file \"" ) + sFilename + std::string( "\": " ) + std::string( " Number of channels for a HRIR must be at least two." ) );

	if( m_pReader->getProperties( )->getNumberOfChannels( ) > 2 )
		VA_WARN( "DirectivityDAFFIR", "The HRIR dataset file " << sFilename << " has more than two channels, but will only use first two (left, right)" );

	m_oProps.sFilename          = sFilename;
	m_oProps.sName              = sName;
	m_oProps.iFilterLength      = m_iFilterLength;
	m_oProps.fFilterLatency     = m_fLatency;
	m_oProps.bFullSphere        = m_pReader->getProperties( )->coversFullSphere( );
	m_oProps.bSpaceDiscrete     = true;
	m_oProps.bDistanceDependent = false;
}

CVADirectivityDAFFHRIR::~CVADirectivityDAFFHRIR( )
{
	if( m_pReader )
		m_pReader->closeFile( );
	delete m_pReader;
}

std::string CVADirectivityDAFFHRIR::GetFilename( ) const
{
	return m_pReader->getFilename( );
}

std::string CVADirectivityDAFFHRIR::GetName( ) const
{
	return m_sName;
}

std::string CVADirectivityDAFFHRIR::GetDesc( ) const
{
	char buf[1024];
	sprintf( buf, "DAFF, 2D, %s, discrete %0.0fx%0.0f°, %d taps", ( m_oProps.bFullSphere ? "full sphere" : "partial sphere" ),
	         m_pContent->getProperties( )->getAlphaResolution( ), m_pContent->getProperties( )->getBetaResolution( ), m_oProps.iFilterLength );
	return buf;
}

const CVAHRIRProperties* CVADirectivityDAFFHRIR::GetProperties( ) const
{
	return &m_oProps;
}

void CVADirectivityDAFFHRIR::GetNearestNeighbour( const float fAzimuthDeg, const float fElevationDeg, int* piIndex, bool* pbOutOfBounds ) const
{
	assert( m_pContent );

	if( !piIndex )
		return;

	bool bOutOfBounds;
	m_pContent->getNearestNeighbour( DAFF_OBJECT_VIEW, fAzimuthDeg, fElevationDeg, *piIndex, bOutOfBounds );
	if( pbOutOfBounds )
		*pbOutOfBounds = bOutOfBounds;
}

const DAFFMetadata* CVADirectivityDAFFHRIR::GetMetadataByIndex( const int iIndex ) const
{
	assert( m_pContent );

	return m_pContent->getRecordMetadata( iIndex );
}


void CVADirectivityDAFFHRIR::GetHRIRByIndex( ITASampleFrame* psfDest, const int iIndex, const float ) const
{
	assert( m_pContent );

	if( psfDest->channels( ) > m_pReader->getProperties( )->getNumberOfChannels( ) )
		VA_EXCEPT1( std::string( "HRIRDatasetDAFF2D::GetHRIRByIndex - Target SampleFrame contains more channels than HRIR database" ) );

	int iResult;
	for( int iChan = 0; iChan < psfDest->channels( ); iChan++ )
	{
		iResult = m_pContent->getFilterCoeffs( iIndex, iChan, ( *psfDest )[iChan].data( ) );
		if( iResult != DAFF_NO_ERROR )
			VA_EXCEPT1( DAFFUtils::StrError( iResult ) );
	}
}

void CVADirectivityDAFFHRIR::GetHRIR( ITASampleFrame* psfDest, const float fAzimuthDeg, const float fElevationDeg, const float fDistanceMeters, int* piIndex,
                                      bool* pbOutOfBounds ) const
{
	int i;
	GetNearestNeighbour( fAzimuthDeg, fElevationDeg, &i, pbOutOfBounds );
	if( piIndex )
		*piIndex = i;
	GetHRIRByIndex( psfDest, i, fDistanceMeters );
}

DAFFContentIR* CVADirectivityDAFFHRIR::GetDAFFContent( ) const
{
	return m_pContent;
}
