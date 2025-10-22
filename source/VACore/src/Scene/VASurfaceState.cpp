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

#include "VASurfaceState.h"

//#include <VAInterface.h>
#include <cassert>

void CVASurfaceState::Initialize( double dModificationTime )
{
	// Grundzustand => geschlossen
	data.iMaterial = -1;
	SetModificationTime( dModificationTime );
}

void CVASurfaceState::Copy( const CVASurfaceState* pSrc, double dModificationTime )
{
	assert( pSrc );
	// Zusatz-Check: Kopieren nur von fixierten Zuständen erlauben
	assert( pSrc->IsFixed( ) );

	// Daten kopieren
	data = pSrc->data;
	SetFixed( false );
	SetModificationTime( dModificationTime );
}

void CVASurfaceState::Fix( )
{
	// INFO: Noch keine Unterobjekte die es zu fixeren gilt

	// Selbst fixieren
	CVASceneStateBase::Fix( );
}

void CVASurfaceState::PreRelease( )
{
	// TODO: Wichtig. Unbedingt daran denken:
	// Alle Referenzen auf abhängige Objekte entfernen

	// Funktion der Oberklasse aufrufen
	CVASceneStateBase::PreRelease( );
}

int CVASurfaceState::GetMaterial( ) const
{
	return data.iMaterial;
}

void CVASurfaceState::SetMaterial( int iMaterialID )
{
	data.iMaterial = iMaterialID;
}
