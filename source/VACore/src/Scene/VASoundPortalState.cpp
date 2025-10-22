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

#include "VASoundPortalState.h"

#include <cassert>

void CVAPortalState::Initialize( double dModificationTime )
{
	// Grundzustand => geschlossen
	data.fState = 0;
	SetModificationTime( dModificationTime );
}

void CVAPortalState::Copy( const CVAPortalState* pSrc, double dModificationTime )
{
	assert( pSrc );
	// Zusatz-Check: Kopieren nur von fixierten Zuständen erlauben
	assert( pSrc->IsFixed( ) );

	// Daten kopieren
	data = pSrc->data;
	SetFixed( false );
	SetModificationTime( dModificationTime );
}

void CVAPortalState::Fix( )
{
	// INFO: Noch keine Unterobjekte die es zu fixeren gilt

	// Selbst fixieren
	CVASceneStateBase::Fix( );
}

void CVAPortalState::PreRelease( )
{
	// TODO: Wichtig. Unbedingt daran denken:
	// Alle Referenzen auf abhängige Objekte entfernen

	// Funktion der Oberklasse aufrufen
	CVASceneStateBase::PreRelease( );
}

float CVAPortalState::GetState( ) const
{
	return data.fState;
}

void CVAPortalState::SetState( float fState )
{
	data.fState = fState;
}
