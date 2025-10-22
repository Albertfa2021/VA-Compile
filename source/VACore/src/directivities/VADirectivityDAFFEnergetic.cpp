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

#include "VADirectivityDAFFEnergetic.h"

#include "../VALog.h"

#include <DAFF.h>
#include <ITANumericUtils.h>
#include <ITASampleFrame.h>
#include <ITAStringUtils.h>
#include <VAException.h>
#include <cassert>
#include <sstream>

CVADirectivityDAFFEnergetic::CVADirectivityDAFFEnergetic( const std::string& sFilename, const std::string& sName ) : m_sName( sName ), m_pReader( nullptr )
{
	m_iType = IVADirectivity::DAFF_ENERGETIC;

	// Der DAFF-Reader wirft std::exceptions
	m_pReader = DAFFReader::create( );

	int iError = m_pReader->openFile( sFilename );
	if( iError != DAFF_NO_ERROR )
	{
		delete m_pReader;
		std::string sErrorStr = DAFFUtils::StrError( iError );
		VA_EXCEPT1( std::string( "Could not load HRIR dataset from file \"" ) + sFilename + std::string( "\". " ) + sErrorStr + std::string( "." ) );
	}

	if( m_pReader->getContentType( ) != DAFF_MAGNITUDE_SPECTRUM )
	{
		delete m_pReader;
		VA_EXCEPT1( std::string( "The file \"" ) + sFilename + std::string( "\" does not contain (energetic) magnitude spectrum content." ) );
	}

	if( m_pReader->getProperties( )->getNumberOfChannels( ) != 1 )
	{
		VA_WARN( "DirectivityManager", std::string( "The file \"" ) + sFilename + std::string( "\" does not have a single channel, will use first." ) );
	}

	m_pContent = dynamic_cast<DAFFContentMS*>( m_pReader->getContent( ) );

	m_pMetadata = m_pReader->getMetadata( );

	m_fLatency = 0.0f;

	// Warnung, falls keine Vollkugel
	if( !m_pReader->getProperties( )->coversFullSphere( ) )
		VA_WARN( "DirectivityDAFFEnergetic", "The directivity file " << sFilename << " does not cover all directions" );
}

CVADirectivityDAFFEnergetic::~CVADirectivityDAFFEnergetic( )
{
	if( m_pReader )
		m_pReader->closeFile( );
	delete m_pReader;
}

std::string CVADirectivityDAFFEnergetic::GetFilename( ) const
{
	return m_pReader->getFilename( );
}

std::string CVADirectivityDAFFEnergetic::GetName( ) const
{
	return m_sName;
}

std::string CVADirectivityDAFFEnergetic::GetDesc( ) const
{
	char buf[1024];
	sprintf( buf, "DAFF, 2D, discrete %0.0fx%0.0f degrees", m_pContent->getProperties( )->getAlphaResolution( ), m_pContent->getProperties( )->getBetaResolution( ) );
	return buf;
}

void CVADirectivityDAFFEnergetic::GetNearestNeighbour( const float fAzimuthDeg, const float fElevationDeg, int* piIndex, bool* pbOutOfBounds ) const
{
	assert( m_pContent );

	if( !piIndex )
		return;

	bool bOutOfBounds;
	m_pContent->getNearestNeighbour( DAFF_OBJECT_VIEW, fAzimuthDeg, fElevationDeg, *piIndex, bOutOfBounds );
	if( pbOutOfBounds )
		*pbOutOfBounds = bOutOfBounds;
}


void CVADirectivityDAFFEnergetic::GetHRIRByIndex( ITASampleFrame* psfDest, const int iIndex, const float ) const
{
	assert( m_pContent );

	if( psfDest->channels( ) > m_pReader->getProperties( )->getNumberOfChannels( ) )
		VA_EXCEPT1( std::string( "HRIRDatasetDAFF2D::GetHRIRByIndex - Target SampleFrame contains more channels than HRIR database" ) );

	int iResult;
	for( int iChan = 0; iChan < psfDest->channels( ); iChan++ )
	{
		iResult = m_pContent->getMagnitudes( iIndex, iChan, ( *psfDest )[iChan].data( ) );
		if( iResult != DAFF_NO_ERROR )
			VA_EXCEPT1( DAFFUtils::StrError( iResult ) );
	}
}

void CVADirectivityDAFFEnergetic::GetHRIR( ITASampleFrame* psfDest, const float fAzimuthDeg, const float fElevationDeg, const float fDistanceMeters, int* piIndex,
                                           bool* pbOutOfBounds ) const
{
	int i;
	GetNearestNeighbour( fAzimuthDeg, fElevationDeg, &i, pbOutOfBounds );
	if( piIndex )
		*piIndex = i;
	GetHRIRByIndex( psfDest, i, fDistanceMeters );
}

DAFFContentMS* CVADirectivityDAFFEnergetic::GetDAFFContent( ) const
{
	return m_pContent;
}
