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

#include "core.h"

void CVACoreImpl::CoreThreadLoop( )
{
	m_oCoreThreadLoopTotalDuration.start( );

	assert( m_pCurSceneState != nullptr );
	assert( m_pCurSceneState->GetNumReferences( ) >= 1 );

	// Auf Änderungen der Szene überprüfen
	CVASceneState* pNewSceneState = m_pSceneManager->GetHeadSceneState( );

	assert( pNewSceneState != nullptr );
	assert( pNewSceneState->GetNumReferences( ) >= 1 );

	pNewSceneState->AddReference( );

	// TODO: Aktiver Hörer wird erstmal ignoriert.
	// int iNewActiveListener = m_iNewActiveListener;

	if( pNewSceneState != m_pCurSceneState )
	{
		VA_TRACE( "CoreThread", "Update scene state " << m_pCurSceneState->GetID( ) << " -> " << pNewSceneState->GetID( ) );

		// Audio renderer
		std::vector<CVAAudioRendererDesc>::iterator rendit = m_voRenderers.begin( );
		while( rendit != m_voRenderers.end( ) )
		{
			CVAAudioRendererDesc& rendesc( *rendit );
			rendesc.pInstance->UpdateScene( pNewSceneState );
			rendit++;
		}

		// Audio reproduction modules
		std::vector<CVAAudioReproductionModuleDesc>::iterator repit = m_voReproductionModules.begin( );
		while( repit != m_voReproductionModules.end( ) )
		{
			CVAAudioReproductionModuleDesc& repdesc( *repit );
			repdesc.pInstance->UpdateScene( pNewSceneState );
			repit++;
		}
	}


	// Änderung des globalen Auralisierungsmodus in den Schallpfaden berücksichtigen
	/**
	 * Da es keine Versionierung des AuraModes gibt, muss der Audio
	 * Renderer selbst entscheiden, was zu tun ist.
	 */
	std::vector<CVAAudioRendererDesc>::iterator rendit = m_voRenderers.begin( );
	while( rendit != m_voRenderers.end( ) )
	{
		CVAAudioRendererDesc& rendesc( *rendit );
		rendesc.pInstance->UpdateGlobalAuralizationMode( m_iGlobalAuralizationMode );

		rendit++;
	}

	// Referenzen verwalten: Alter Szenezustand wird nun nicht mehr benötigt
	if( m_pCurSceneState != pNewSceneState )
	{
		// Alter Zustand wird nicht mehr benötigt
		m_pCurSceneState->RemoveReference( );

		// Aktuelle Szene ist nun die neu erzeugte (abgeleitete) Szene
		m_pCurSceneState = pNewSceneState;
	}
	else
	{
		// Remove reference of new/current state again
		pNewSceneState->RemoveReference( );
	}

	m_oCoreThreadLoopTotalDuration.stop( );

	// @todo: signal event to process object calls (in-sync exec with internal core)

	return;
}


bool CVACoreImpl::operator( )( )
{
#ifndef DO_NOT_SEND_MEASURE_UPDATE_EVENTS

	// Audio device I/O detectors
	SendAudioDeviceDetectorUpdateEvent( );

	// Rendering module output detectors
	SendRenderingModuleOutputDetectorsUpdateEvents( );

	// Rerproduction module I/O detectors
	SendReproductionModuleOIDetectorsUpdateEvents( );

	return true;
#else
	return false;
#endif
}
