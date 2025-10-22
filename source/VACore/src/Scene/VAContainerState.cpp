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

#include "VAContainerState.h"

#include "../Utils/VAUtils.h"

#include <VA.h>
#include <algorithm>
#include <cassert>
#include <iterator>

#ifdef VA_CONTAINER_STATE_USE_STD_SET_ALGORITHM

void CVAContainerState::Initialize( const double dModificationTime )
{
	data.sElements.clear( );
	SetModificationTime( dModificationTime );
}

void CVAContainerState::Copy( const CVAContainerState* pSrc, const double dModificationTime )
{
	assert( pSrc );
	// Zusatz-Check: Kopieren nur von fixierten Zuständen erlauben
	assert( pSrc->IsFixed( ) );

	// Daten kopieren
	data = pSrc->data;
	SetModificationTime( dModificationTime );
}

void CVAContainerState::Fix( )
{
	// Alle enthaltenen Objekte fixieren
	for( std::vector<ElemType>::iterator it = data.vElements.begin( ); it != data.vElements.end( ); ++it )
	{
		assert( it->second );
		it->second->Fix( );
	}

	// Selbst fixieren
	CVASceneStateBase::Fix( );
}

void CVAContainerState::PreRelease( )
{
	// Alle Referenzen auf abhängige Objekte entfernen
	for( std::vector<ElemType>::iterator it = data.vElements.begin( ); it != data.vElements.end( ); ++it )
	{
		assert( it->second );
		it->second->RemoveReference( );
	}

	// Funktion der Oberklasse aufrufen
	CVASceneStateBase::PreRelease( );
}

void CVAContainerState::Diff( const CVAContainerState* pComp, CVAContainerDiff* pDiff ) const
{
	assert( pComp && pDiff );

	const std::set<int>& A = pComp->data.sElements;
	const std::set<int>& B = this->data.sElements;

	pDiff->liCom.clear( );
	pDiff->liNew.clear( );
	pDiff->liDel.clear( );

	std::insert_iterator<std::vector<int> > iCom( pDiff->liCom, pDiff->liCom.begin( ) );
	std::insert_iterator<std::vector<int> > iNew( pDiff->liNew, pDiff->liNew.begin( ) );
	std::insert_iterator<std::vector<int> > iDel( pDiff->liDel, pDiff->liDel.begin( ) );

	// Elemente in beiden Kontainern
	std::set_intersection( A.begin( ), A.end( ), B.begin( ), B.end( ), iCom );

	// Elemente nur in diesem Kontainer
	std::set_difference( B.begin( ), B.end( ), A.begin( ), A.end( ), iNew );

	// Elemente nur im Vergleichskontainer
	std::set_difference( A.begin( ), A.end( ), B.begin( ), B.end( ), iDel );
}

void CVAContainerState::GetIDs( std::vector<int>* pviDest ) const
{
	pviDest->reserve( data.sElements.size( ) );
	pviDest->clear( );
	std::copy( data.sElements.begin( ), data.sElements.end( ), pviDest->begin( ) );
}

void CVAContainerState::GetIDs( std::list<int>* pliDest ) const
{
	pliDest->clear( );
	std::copy( data.sElements.begin( ), data.sElements.end( ), std::back_inserter( *pliDest ) );
}

void CVAContainerState::GetIDs( std::set<int>* psiDest ) const
{
	psiDest->clear( );
	std::copy( data.sElements.begin( ), data.sElements.end( ), std::inserter( *psiDest, psiDest->end( ) ) );
}

void CVAContainerState::AddObject( const int iID, CVASceneStateBase* pObject )
{
	data.sElements.insert( iID );
}

void CVAContainerState::RemoveObject( const int iID )
{
	data.sElements.erase( iID );
}

#else

void CVAContainerState::Initialize( const double dModificationTime )
{
	data.vElements.clear( );
	data.bDirty = false;
	SetModificationTime( dModificationTime );
}

void CVAContainerState::Copy( const CVAContainerState* pSrc, const double dModificationTime )
{
	assert( pSrc );

	// Abhängige Objekte des Vorlagenobjektes autonom referenzieren
	for( std::vector<ElemType>::iterator it = pSrc->data.vElements.begin( ); it != pSrc->data.vElements.end( ); ++it )
	{
		assert( it->second );
		it->second->AddReference( );
	}

	// Daten kopieren
	data = pSrc->data;
	SetFixed( false );
	SetModificationTime( dModificationTime );
}

void CVAContainerState::Fix( )
{
	// Alle enthaltenen Objekte fixieren
	for( std::vector<ElemType>::iterator it = data.vElements.begin( ); it != data.vElements.end( ); ++it )
	{
		assert( it->second );
		it->second->Fix( );
	}

	// Selbst fixieren
	CVASceneStateBase::Fix( );
}

void CVAContainerState::PreRelease( )
{
	// Alle Referenzen auf abhängige Objekte entfernen
	for( std::vector<ElemType>::iterator it = data.vElements.begin( ); it != data.vElements.end( ); ++it )
	{
		assert( it->second );
		it->second->RemoveReference( );
	}

	// Funktion der Oberklasse aufrufen
	CVASceneStateBase::PreRelease( );
}

void CVAContainerState::Diff( const CVAContainerState* pComp, std::vector<int>& viNew, std::vector<int>& viDel, std::vector<int>& viCom ) const
{
	viNew.clear( );
	viDel.clear( );
	viCom.clear( );

	// Vergleichszustand Leer => Alles Neu
	if( pComp == nullptr )
	{
		for( std::vector<ElemType>::const_iterator it = data.vElements.begin( ); it != data.vElements.end( ); ++it )
			viNew.push_back( it->first );

		return;
	}

	// Resort arrays when necessary
	if( pComp->data.bDirty )
	{
		std::sort( pComp->data.vElements.begin( ), pComp->data.vElements.end( ), ElemCompare( ) );
		pComp->data.bDirty = false;
	}

	if( data.bDirty )
	{
		std::sort( data.vElements.begin( ), data.vElements.end( ), ElemCompare( ) );
		data.bDirty = false;
	}

	// Turbo vergleich
	std::vector<ElemType>& A           = pComp->data.vElements;
	std::vector<ElemType>& B           = this->data.vElements;
	std::vector<ElemType>::iterator ai = A.begin( );
	std::vector<ElemType>::iterator bi = B.begin( );

	while( true )
	{
		// Keine weiteren Elemente in A
		if( ai == A.end( ) )
		{
			// Alle weiteren Elemente in B => neue Elemente
			for( ; bi != B.end( ); ++bi )
				viNew.push_back( bi->first );
			return;
		}

		// Keine weiteren Elemente in B
		if( bi == B.end( ) )
		{
			// Alle weiteren Elemente in A => gelöschte Elemente
			// Alle weiteren Elemente in B => neue Elemente
			for( ; ai != A.end( ); ++ai )
				viDel.push_back( ai->first );
			return;
		}


		if( ai->first < bi->first )
		{
			// A Element fehlt in B
			viDel.push_back( ai->first );

			// Weiter in A bis B erreicht
			ai++;
			continue;
		}

		if( ai->first > bi->first )
		{
			// B Element fehlt in A
			viNew.push_back( bi->first );

			// Weiter in B bis A erreicht
			bi++;
			continue;
		}

		// *ai = *bi => Element in beiden und weiter
		viCom.push_back( ai->first );
		ai++;
		bi++;
	}
}

int CVAContainerState::GetSize( ) const
{
	return (int)data.vElements.size( );
}

void CVAContainerState::GetIDs( std::vector<int>* pviDest ) const
{
	pviDest->reserve( data.vElements.size( ) );
	pviDest->clear( );

	for( std::vector<ElemType>::iterator i = data.vElements.begin( ); i != data.vElements.end( ); ++i )
		pviDest->push_back( i->first );
}

void CVAContainerState::GetIDs( std::list<int>* pliDest ) const
{
	pliDest->clear( );

	for( std::vector<ElemType>::iterator i = data.vElements.begin( ); i != data.vElements.end( ); ++i )
		pliDest->push_back( i->first );
}

void CVAContainerState::GetIDs( std::set<int>* psiDest ) const
{
	psiDest->clear( );

	for( std::vector<ElemType>::iterator i = data.vElements.begin( ); i != data.vElements.end( ); ++i )
		psiDest->insert( i->first );
}

bool CVAContainerState::HasObject( const int iID ) const
{
	for( std::vector<ElemType>::iterator i = data.vElements.begin( ); i != data.vElements.end( ); ++i )
		if( i->first == iID )
			return true;
	return false;
}

CVASceneStateBase* CVAContainerState::GetObject( const int iID ) const
{
	for( std::vector<ElemType>::iterator i = data.vElements.begin( ); i != data.vElements.end( ); ++i )
	{
		if( i->first == iID )
		{
			// Wenn Objekt in der Liste, dann darf es nicht nullptr sein
			assert( i->second );
			return i->second;
		}
	}

	// Should not happen ...
	assert( true );
	return nullptr;
}

void CVAContainerState::AddObject( const int iID, CVASceneStateBase* pObject )
{
	assert( pObject );
	pObject->AddReference( );
	data.vElements.push_back( std::make_pair( iID, pObject ) );
	data.bDirty = true;
}

void CVAContainerState::RemoveObject( const int iID )
{
	for( std::vector<ElemType>::iterator i = data.vElements.begin( ); i != data.vElements.end( ); ++i )
	{
		if( i->first == iID )
		{
			assert( i->second );
			i->second->RemoveReference( );
			data.vElements.erase( i );
			return;
		}
	}

	// Should not happen ...
	assert( true );
}

void CVAContainerState::SetObject( const int iID, CVASceneStateBase* pObject )
{
	assert( pObject );

	for( std::vector<ElemType>::iterator i = data.vElements.begin( ); i != data.vElements.end( ); ++i )
	{
		if( i->first == iID )
		{
			// Referenz auf altem Objekt entfernen und bei neuem Objekt hinzufügen
			pObject->AddReference( );
			if( i->second )
			{
				assert( i->second->GetNumReferences( ) > 0 );
				i->second->RemoveReference( );
			}

			i->second = pObject;
			return;
		}
	}

	// Should not happen ...
	assert( true );
}

#endif // VA_CONTAINER_STATE_USE_STD_SET_ALGORITHM
