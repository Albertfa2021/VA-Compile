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

#include "VASoundSourceState.h"

#include "../VALog.h"
#include "VAMotionState.h"
#include "VASceneManager.h"

#include <VA.h>
#include <cassert>

void CVASoundSourceState::Initialize( double dModificationTime )
{
	data.pMotionState   = nullptr; // undefined position
	data.dSoundPower    = g_dSoundPower_94dB_SPL_1m;
	data.iAuraMode      = IVAInterface::VA_AURAMODE_ALL;
	data.iDirectivityID = -1;
	data.pDirectivity   = nullptr;

	SetModificationTime( dModificationTime );
}

void CVASoundSourceState::Copy( const CVASoundSourceState* pSrc, double dModificationTime )
{
	assert( pSrc );
	// Zusatz-Check: Kopieren nur von fixierten Zuständen erlauben
	assert( pSrc->IsFixed( ) );

	// Abhängige Objekte des Vorlagenobjektes autonom referenzieren
	if( pSrc->data.pMotionState )
	{
		assert( pSrc->data.pMotionState->GetNumReferences( ) > 0 );
		pSrc->data.pMotionState->AddReference( );
	}

	// Daten kopieren
	data = pSrc->data;
	SetFixed( false );
	SetModificationTime( dModificationTime );
}

void CVASoundSourceState::Fix( )
{
	// Alle enthaltenen Objekte fixieren
	if( data.pMotionState )
		data.pMotionState->Fix( );

	// Selbst fixieren
	CVASceneStateBase::Fix( );
}

void CVASoundSourceState::PreRelease( )
{
	// Alle Referenzen auf abhängige Objekte entfernen
	if( data.pMotionState )
	{
		assert( data.pMotionState->GetNumReferences( ) > 0 );
		data.pMotionState->RemoveReference( );
		data.pMotionState = NULL;
	}

	// Funktion der Oberklasse aufrufen
	CVASceneStateBase::PreRelease( );
}

const CVAMotionState* CVASoundSourceState::GetMotionState( ) const
{
	return data.pMotionState;
}

CVAMotionState* CVASoundSourceState::AlterMotionState( )
{
	assert( !IsFixed( ) );

	// Falls der Zustand finalisiert => Zustand aus der Basiskonfig => Autonomen Zustand erzeugen
	// Falls der Zustand nicht finalisiert => Bereits erzeugter autonomen Zustand => Diesen zurückgeben

	if( !data.pMotionState )
	{
		data.pMotionState = GetManager( )->RequestMotionState( );
		data.pMotionState->Initialize( GetModificationTime( ) );
	}
	else
	{
		if( data.pMotionState->IsFixed( ) )
		{
			// Autonomen Zustand ableiten
			CVAMotionState* pNewState = GetManager( )->RequestMotionState( );

			// Änderungszeit des übergeordneten Szenezustands übernehmen
			pNewState->Copy( data.pMotionState, GetModificationTime( ) );
			data.pMotionState->RemoveReference( );

			data.pMotionState = pNewState;
		}
	}

	VA_TRACE( "SoundSourceState", "Requested new motion state" );
	return data.pMotionState;
}

double CVASoundSourceState::GetVolume( const double dAmplitudeCalibration ) const
{
	if( dAmplitudeCalibration <= 0.0f )
		VA_EXCEPT2( INVALID_PARAMETER, "Aplitude calibration factor for converting sound source power can not be zero or negative." );
	return data.dSoundPower / dAmplitudeCalibration;
}

void CVASoundSourceState::SetSoundPower( const double dSoundPower )
{
	assert( !IsFixed( ) );
	data.dSoundPower = dSoundPower;
	VA_VERBOSE( "SoundSourceState", "Sound power changed" );
}

double CVASoundSourceState::GetSoundPower( ) const
{
	return data.dSoundPower;
}

int CVASoundSourceState::GetDirectivityID( ) const
{
	return data.iDirectivityID;
}

const IVADirectivity* CVASoundSourceState::GetDirectivityData( ) const
{
	return data.pDirectivity;
}

void CVASoundSourceState::SetDirectivityID( int iDirectivityID )
{
	assert( !IsFixed( ) );
	data.iDirectivityID = iDirectivityID;
	VA_VERBOSE( "SoundSourceState", "Directivity identifier changed" );
}

void CVASoundSourceState::SetDirectivityData( const IVADirectivity* pDirData )
{
	assert( !IsFixed( ) );
	data.pDirectivity = pDirData;
	VA_VERBOSE( "SoundSourceState", "Directivity dataset changed" );
}


int CVASoundSourceState::GetAuralizationMode( ) const
{
	return data.iAuraMode;
}

void CVASoundSourceState::SetAuralizationMode( int iAuralizationMode )
{
	assert( !IsFixed( ) );
	// TODO: Validation
	data.iAuraMode = iAuralizationMode;
}

void CVASoundSourceState::SetParameters( const CVAStruct& oParams )
{
	VA_VERBOSE( "SoundSourceState", "Parameters changed" );
	data.oParams = oParams;
}

CVAStruct CVASoundSourceState::GetParameters( const CVAStruct& ) const
{
	VA_VERBOSE( "SoundSourceState", "Parameters requested" );
	return data.oParams;
}
