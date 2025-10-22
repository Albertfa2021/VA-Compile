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

#include "VASoundReceiverState.h"

#include "../VALog.h"
#include "VAMotionState.h"
#include "VASceneManager.h"

#include <VA.h>
#include <cassert>

void CVAReceiverState::Initialize( double dModificationTime )
{
	data.pMotionState   = nullptr; // undefined position
	data.iAuraMode      = IVAInterface::VA_AURAMODE_ALL;
	data.iDirectivityID = -1;
	data.pDirectivity   = nullptr;

	SetModificationTime( dModificationTime );
}

void CVAReceiverState::Copy( const CVAReceiverState* pSrc, double dModificationTime )
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

void CVAReceiverState::Fix( )
{
	// Alle enthaltenen Objekte fixieren
	if( data.pMotionState )
		data.pMotionState->Fix( );

	// Selbst fixieren
	CVASceneStateBase::Fix( );
}

void CVAReceiverState::PreRelease( )
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

const CVAMotionState* CVAReceiverState::GetMotionState( ) const
{
	return data.pMotionState;
}

int CVAReceiverState::GetDirectivityID( ) const
{
	return data.iDirectivityID;
}

const IVADirectivity* CVAReceiverState::GetDirectivity( ) const
{
	return data.pDirectivity;
}

CVAMotionState* CVAReceiverState::AlterMotionState( )
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
			double dCreationTime = GetModificationTime( );
			pNewState->Copy( data.pMotionState, dCreationTime );
			data.pMotionState->RemoveReference( );

			data.pMotionState = pNewState;
		}
	}

	VA_TRACE( "SoundReceiverState", "Requested new motion state" );
	return data.pMotionState;
}


int CVAReceiverState::GetAuralizationMode( ) const
{
	return data.iAuraMode;
}

void CVAReceiverState::SetAuralizationMode( int iAuralizationMode )
{
	assert( !IsFixed( ) );
	data.iAuraMode = iAuralizationMode;
}

void CVAReceiverState::SetDirectivityID( int iID )
{
	assert( !IsFixed( ) );
	data.iDirectivityID = iID;
}

void CVAReceiverState::SetDirectivity( const IVADirectivity* pDirectivity )
{
	assert( !IsFixed( ) );
	data.pDirectivity = pDirectivity;
}

std::string CVAReceiverState::ToString( ) const
{
	/*
	std::stringstream ss;
	ss << "Sound source state {\n"
	<< "
	*/
	return "";
}

const CVAReceiverState::CVAAnthropometricParameter& CVAReceiverState::GetAnthropometricData( ) const
{
	return data.oAnthroData;
}

void CVAReceiverState::SetAnthropometricData( const CVAAnthropometricParameter& oAnthroData )
{
	data.oAnthroData = oAnthroData;
}

void CVAReceiverState::SetParameters( const CVAStruct& oParams )
{
	VA_VERBOSE( "ListenerState", "Parameters changed" );

	if( oParams.HasKey( "anthroparams" ) )
	{
		const CVAStruct& oAnthroParams( oParams["anthroparams"] );
		if( oAnthroParams.HasKey( "headwidth" ) )
			data.oAnthroData.dHeadWidth = oAnthroParams["headwidth"];
		if( oAnthroParams.HasKey( "headheight" ) )
			data.oAnthroData.dHeadHeight = oAnthroParams["headheight"];
		if( oAnthroParams.HasKey( "headdepth" ) )
			data.oAnthroData.dHeadDepth = oAnthroParams["headdepth"];
	}
}

CVAStruct CVAReceiverState::GetParameters( const CVAStruct& ) const
{
	VA_VERBOSE( "ListenerState", "Parameters requested" );

	CVAStruct oRet;

	oRet["auramode"]                                                 = data.iAuraMode;
	oRet["hrirdatasetid"]                                            = data.iDirectivityID;
	oRet[std::string( "anthroparams" )]                              = CVAStruct( );
	oRet[std::string( "anthroparams" )][std::string( "headwidth" )]  = data.oAnthroData.dHeadWidth;
	oRet[std::string( "anthroparams" )][std::string( "headheight" )] = data.oAnthroData.dHeadHeight;
	oRet[std::string( "anthroparams" )][std::string( "headdepth" )]  = data.oAnthroData.dHeadDepth;

	return oRet;
}

double CVAReceiverState::CVAAnthropometricParameter::GetHeadDepthParameterFromLUT( double dInvidividualDepth ) const
{
	// Listener size
	size_t a = size_t( std::round( dHeadDepth * 100 ) - 3 );

	if( a >= m_vvdHeadDepthParameterLUT.size( ) )
		a = m_vvdHeadDepthParameterLUT.size( ) - 1;

	if( a <= 0 )
		a = 0;

	// Individual subject size
	size_t b = size_t( std::round( dInvidividualDepth * 100 ) - 3 );

	if( b >= m_vvdHeadDepthParameterLUT[0].size( ) )
		b = m_vvdHeadDepthParameterLUT[0].size( ) - 1;

	if( b <= 0 )
		b = 0;

	return ( m_vvdHeadDepthParameterLUT[a][b] );
}
