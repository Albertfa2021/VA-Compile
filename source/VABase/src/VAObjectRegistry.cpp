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

#include <VAException.h>
#include <VAObject.h>
#include <VAObjectRegistry.h>
#include <algorithm>
#include <cassert>

#define VA_OBJECTREGISTRY_INITIAL_SIZE 32
#define VA_OBJECTREGISTRY_GROW_SIZE    32

CVAObjectRegistry::CVAObjectRegistry( ) : m_vpObjects( VA_OBJECTREGISTRY_INITIAL_SIZE, NULL ), m_nObjects( 0 ), m_iIDLast( 0 ) {}

CVAObjectRegistry::~CVAObjectRegistry( ) {}

void CVAObjectRegistry::Clear( )
{
	m_vpObjects.clear( );
	m_vpObjects.resize( VA_OBJECTREGISTRY_INITIAL_SIZE, NULL );
	m_mpObjects.clear( );
	m_nObjects = 0;
	m_iIDLast  = 0;
}

int CVAObjectRegistry::RegisterObject( CVAObject* pObject )
{
	if( !pObject )
		VA_EXCEPT2( INVALID_PARAMETER, "Null pointer now allowed" );

	int iCurID = pObject->GetObjectID( );
	if( iCurID < 0 )
		VA_EXCEPT2( INVALID_PARAMETER, "Object has an invalid ID" );

	// Ensure that it is not part of another registry
	if( iCurID != 0 )
		VA_EXCEPT2( INVALID_PARAMETER, "Object is part of another object registry" );

	std::string sName( pObject->GetObjectName( ) );
	if( pObject->m_sObjectName.empty( ) )
		VA_EXCEPT2( INVALID_PARAMETER, "Object must have a name" );
	std::transform( sName.begin( ), sName.end( ), sName.begin( ), toupper );

	// Ensure that the object has a unique name
	if( m_mpObjects.find( sName ) != m_mpObjects.end( ) )
		VA_EXCEPT2( INVALID_PARAMETER, "Object with name '" + sName + "' already registered as a VA object" );

	// Grow the internal lookup table if there is no free space left
	if( m_nObjects == m_vpObjects.size( ) )
	{
		m_vpObjects.resize( m_vpObjects.size( ) + VA_OBJECTREGISTRY_GROW_SIZE, NULL );
	}

	// Generate a new ID. Search until the end is reached. Then start again at the front.
	int i = m_iIDLast, iID = 0;
	while( iID == 0 )
	{
		i = ( i + 1 ) % (int)m_vpObjects.size( );
		if( m_vpObjects[i] == 0 )
		{
			iID = i;
			break;
		}
	}

	pObject->SetObjectID( iID );
	m_vpObjects[iID] = pObject;
	m_mpObjects.insert( std::make_pair( sName, pObject ) );
	m_iIDLast = iID;
	++m_nObjects;

	return iID;
}

void CVAObjectRegistry::UnregisterObject( CVAObject* pObject )
{
	if( !pObject )
		VA_EXCEPT2( INVALID_PARAMETER, "Null pointer now allowed" );

	int iCurID = pObject->GetObjectID( );
	if( iCurID <= 0 )
		VA_EXCEPT2( INVALID_PARAMETER, "Object has an invalid ID" );

	if( iCurID >= (int)m_vpObjects.size( ) )
		VA_EXCEPT2( INVALID_PARAMETER, "Object not registered" );

	if( m_vpObjects[iCurID] != pObject )
		VA_EXCEPT2( INVALID_PARAMETER, "Object not registered" );

	m_vpObjects[iCurID] = NULL;
	std::string sName( pObject->GetObjectName( ) );
	std::transform( sName.begin( ), sName.end( ), sName.begin( ), toupper );
	m_mpObjects.erase( sName );
	--m_nObjects;
}

void CVAObjectRegistry::GetObjectIDs( std::vector<int>& viIDs ) const
{
	viIDs.clear( );
	if( m_nObjects == 0 )
		return;

	viIDs.reserve( m_nObjects );
	for( size_t i = 0; i < m_vpObjects.size( ); i++ )
		if( m_vpObjects[i] )
			viIDs.push_back( (int)i );
}

void CVAObjectRegistry::GetObjectInfos( std::vector<CVAObjectInfo>& viInfos ) const
{
	viInfos.clear( );
	if( m_nObjects == 0 )
		return;

	viInfos.reserve( m_nObjects );
	for( size_t i = 0; i < m_vpObjects.size( ); i++ )
		if( m_vpObjects[i] )
			viInfos.push_back( m_vpObjects[i]->GetObjectInfo( ) );
}

CVAObject* CVAObjectRegistry::FindObjectByID( const int iID ) const
{
	if( ( iID >= 0 ) || ( iID < (int)m_vpObjects.size( ) ) )
		return m_vpObjects[iID];
	return NULL;
}

CVAObject* CVAObjectRegistry::FindObjectByName( const std::string& sName ) const
{
	std::string sQuery( sName );
	std::transform( sQuery.begin( ), sQuery.end( ), sQuery.begin( ), toupper );
	std::map<std::string, CVAObject*>::const_iterator cit = m_mpObjects.find( sQuery );
	return ( cit == m_mpObjects.end( ) ? NULL : cit->second );
}

CVAObject* CVAObjectRegistry::GetObjectByID( const int iID ) const
{
	CVAObject* pObject = FindObjectByID( iID );
	if( !pObject )
		VA_EXCEPT2( INVALID_PARAMETER, "Invalid object ID" );
	return pObject;
}

CVAObject* CVAObjectRegistry::GetObjectByName( const std::string& sName ) const
{
	CVAObject* pObject = FindObjectByName( sName );
	if( !pObject )
		VA_EXCEPT2( INVALID_PARAMETER, "Object not found" );
	return pObject;
}

CVAStruct CVAObjectRegistry::CallObjectByID( int iID, const CVAStruct& oArgs ) const
{
	return GetObjectByID( iID )->CallObject( oArgs );
}

CVAStruct CVAObjectRegistry::CallObjectByName( const std::string& sName, const CVAStruct& oArgs ) const
{
	return GetObjectByName( sName )->CallObject( oArgs );
}
