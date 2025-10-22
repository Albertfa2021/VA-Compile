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

#include "VADirectivityManager.h"

#include "VADirectivityDAFFEnergetic.h"
#include "VADirectivityDAFFHATOHRIR.h"
#include "VADirectivityDAFFHRIR.h"
//#include "VAHRIRDatasetSH.h"
#include "../VALog.h"

#include <DAFF.h>
#include <ITAASCIITable.h>
#include <ITAException.h>
#include <ITAFileSystemUtils.h>
#include <ITAFunctors.h>
#include <ITAStringUtils.h>
#include <VACore.h>
#include <VAException.h>
#include <algorithm>

CVADirectivityManager::CVADirectivityManager( IVAInterface* pAssociatedCore, const double dDesiredSamplerate )
    : m_pAssociatedCore( pAssociatedCore )
    , m_dSampleRate( dDesiredSamplerate )
{
	assert( m_pAssociatedCore );
}

CVADirectivityManager::~CVADirectivityManager( ) {}

void CVADirectivityManager::Initialize( ) {}

void CVADirectivityManager::Finalize( )
{
	Reset( );
}

void CVADirectivityManager::Reset( )
{
	VA_VERBOSE( "DirectivityManager", "Resetting" );
	for( CVAObjectContainer<IVADirectivity>::iterator it = m_oDirectivities.begin( ); it != m_oDirectivities.end( ); ++it )
	{
		delete it->second;
	}

	m_oDirectivities.Clear( );
}

int CVADirectivityManager::CreateDirectivity( const CVAStruct& oParams, const std::string& sName )
{
	VA_VERBOSE( "DirectivityManager", "Creating directivity with name '" << sName << "'" );

	// Try to identify the directivity type by parsing parameter arguments
	if( oParams.HasKey( "filepath" ) )
	{
		const std::string sFilePath     = oParams["filepath"];
		const std::string sDestFilePath = m_pAssociatedCore->FindFilePath( sFilePath );
		if( sDestFilePath.empty( ) )
			VA_EXCEPT2( INVALID_PARAMETER, "Looked everywhere, but could not find file '" + sFilePath + "'" );

		// Format entscheiden und Implementierung wählen
		IVADirectivity* pDirectivity = nullptr;
		std::string sSuffix          = toLowercase( getFilenameSuffix( sDestFilePath ) );

		if( sSuffix == "daff" )
		{
			// Der DAFF-Reader wirft std::exceptions
			DAFFReader* pReader = DAFFReader::create( );

			int iError = pReader->openFile( sDestFilePath );
			if( iError != DAFF_NO_ERROR )
			{
				delete pReader;
				std::string sErrorStr = DAFFUtils::StrError( iError );
				VA_EXCEPT1( std::string( "Could not load directivity dataset from file \"" ) + sDestFilePath + std::string( "\". " ) + sErrorStr + std::string( "." ) );
			}

			if( pReader->getContentType( ) == DAFF_IMPULSE_RESPONSE )
			{
				if( pReader->getMetadata( )->hasKey( "HATO" ) )
				{
					VA_VERBOSE( "DirectivityManager", "Detected HATO HRTF, will load now" );
					delete pReader;
					pDirectivity = new CVADirectivityDAFFHATOHRIR( sDestFilePath, sName, m_dSampleRate );
				}
				else
				{
					VA_VERBOSE( "DirectivityManager", "Loading a DAFF HRIR file" );
					delete pReader;
					pDirectivity = new CVADirectivityDAFFHRIR( sDestFilePath, sName, m_dSampleRate );
				}
			}
			else if( pReader->getContentType( ) == DAFF_MAGNITUDE_SPECTRUM )
			{
				VA_VERBOSE( "DirectivityManager", "Loading a DAFF energetic magnitude spectrum directivity" );
				delete pReader;
				pDirectivity = new CVADirectivityDAFFEnergetic( sDestFilePath, sName );
			}
			else
			{
				delete pReader;
				VA_EXCEPT2( INVALID_PARAMETER, "DAFF file does not contain recognized content type" );
			}
		}
		else if( sSuffix == "xml" )
		{
			// Monopoles, multipoles, etc
			VA_EXCEPT_NOT_IMPLEMENTED;
		}
		else if( sSuffix == "sofa" )
		{
			// Someone else, please
			VA_EXCEPT_NOT_IMPLEMENTED;
		}
		else if( sSuffix == "hsh" )
		{
			VA_EXCEPT_NOT_IMPLEMENTED;

			// @todo: reactivate one day
			// pDirectivity = new CVAHRIRDatasetSH( sDestFilePath, sName, m_dSamplerate );
		}

		if( !pDirectivity )
			VA_EXCEPT2( INVALID_PARAMETER, "Directivity format not recognized or unsupported" );

		const int iID = m_oDirectivities.Add( pDirectivity );
		VA_VERBOSE( "DirectivityManager", "Successfully created directivity with ID " << iID );

		return iID;
	}
	else
	{
		VA_EXCEPT2( INVALID_PARAMETER, "Could not interpret parameters to create a directivity" );
	}
}

bool CVADirectivityManager::DeleteDirectivity( const int iID )
{
	VA_VERBOSE( "DirectivityManager", "Attempting to delete directivity with ID " << iID );

	// Prüfen ob die ID gültig ist
	int iRefCount = m_oDirectivities.GetRefCount( iID );

	if( iRefCount != 0 )
	{
		// Noch Referenzen vorhanden oder ungültige ID (-1)
		VA_VERBOSE( "DirectivityManager", "Deleting directivity with id " << iID << " failed, still in use by a user or invalid ID." );
		return false;
	}

	IVADirectivity* pDirectivity = m_oDirectivities[iID];
	delete pDirectivity;
	m_oDirectivities.Remove( iID );

	VA_VERBOSE( "DirectivityManager", "Directivity successfully deleted" );

	return true;
}

IVADirectivity* CVADirectivityManager::RequestDirectivity( const int iID )
{
	IVADirectivity* pDirectivity = m_oDirectivities.Request( iID ); // Refence counter +1
	return pDirectivity;
}

int CVADirectivityManager::ReleaseDirectivity( const int iID )
{
	int iRefCount = m_oDirectivities.Release( iID );
	return iRefCount;
}

int CVADirectivityManager::GetDirectivityRefCount( const int iID ) const
{
	return m_oDirectivities.GetRefCount( iID );
}

CVADirectivityInfo CVADirectivityManager::GetDirectivityInfo( const int iID )
{
	IVADirectivity* pDirectivity = m_oDirectivities[iID];
	if( !pDirectivity )
		VA_EXCEPT2( INVALID_ID, "Invalid directivity dataset ID" );

	CVADirectivityInfo oInfo;
	oInfo.iID         = iID;
	oInfo.iClass      = -1;
	oInfo.iReferences = -1;
	oInfo.sName       = pDirectivity->GetName( );
	oInfo.sDesc       = pDirectivity->GetDesc( );

	// @todo more

	return oInfo;
}

void CVADirectivityManager::GetDirectivityInfos( std::vector<CVADirectivityInfo>& voInfos )
{
	voInfos.clear( );
	voInfos.reserve( m_oDirectivities.size( ) );
	for( CVAObjectContainer<IVADirectivity>::const_iterator cit = m_oDirectivities.begin( ); cit != m_oDirectivities.end( ); ++cit )
	{
		voInfos.push_back( GetDirectivityInfo( cit->first ) );
	}
}

void CVADirectivityManager::PrintInfos( )
{
	std::vector<CVADirectivityInfo> v;
	GetDirectivityInfos( v );

	ITAASCIITable t( (unsigned int)v.size( ), 4 );
	t.setColumnTitle( 0, "ID" );
	t.setColumnTitle( 1, "Name" );
	t.setColumnTitle( 2, "Class" );
	t.setColumnTitle( 3, "Description" );
	for( int i = 0; i < (int)v.size( ); i++ )
	{
		t.setContent( i, 0, IntToString( v[i].iID ) );
		t.setContent( i, 1, v[i].sName );
		t.setContent( i, 2, IntToString( v[i].iClass ) );
		t.setContent( i, 3, v[i].sDesc );
	}

	VA_INFO( "DirectivityManager", t.toString( ) );
}
