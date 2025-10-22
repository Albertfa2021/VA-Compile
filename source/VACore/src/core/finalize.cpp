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

void CVACoreImpl::Finalize( )
{
	VA_NO_REENTRANCE;

	// VA_TRACE("Core", __FUNCTION__ << " entry");
	VA_INFO( "Core", "Finalizing core" );

	VA_TRY
	{
		// Mehrfaches Finialisieren führt nicht zu Fehlern
		if( m_iState == VA_CORESTATE_CREATED )
			return;

		if( m_iState == VA_CORESTATE_FAIL )
			VA_EXCEPT2( MODAL_ERROR, "Core corrupted, finalization impossible" );

		// Core-Thread anhalten (wenn frei ist)
		while( !m_pCoreThread->TryBreak( ) )
			VASleep( 10 );
		// m_pCoreThread->Break(); << deadlock

		// Alle Filterketten löschen und warten bis Zustand sicher übernommen
		// Wichtig: Dies muss vor dem Beenden des Streamings geschehen

		// Reset audio renderers
		for( std::vector<CVAAudioRendererDesc>::iterator it = m_voRenderers.begin( ); it != m_voRenderers.end( ); ++it )
			it->pInstance->Reset( );

		// Peak-Nachrichten stoppen
		m_pTicker->StopTicker( );

		// Audio-Streaming beenden
		m_pAudioDriverBackend->stopStreaming( );

		InitProgress( "Stopping auralization threads", "", 2 );

		// Stop and delete ticker
		m_pTicker->SetAfterPulseFunctor( NULL );
		delete m_pTicker;
		m_pTicker = NULL;

		// Hauptthread beenden und freigeben
		delete m_pCoreThread;
		m_pCoreThread = nullptr;

		SetProgress( "Releasing audio hardware", "", 1 );
		FinalizeAudioDriver( );
		FinalizeRenderingModules( );
		FinalizeReproductionModules( );

		SetProgress( "Cleaning up resources", "", 2 );
		delete m_pInputAmp;
		m_pInputAmp = nullptr;

		if( m_pInputStreamDetector )
			if( m_pInputStreamDetector->GetProfilerEnabled( ) )
				VA_VERBOSE( "Core", "Input stream detector profiler: " << m_pInputStreamDetector->GetProfilerResult( ) );
		delete m_pInputStreamDetector;
		m_pInputStreamDetector = nullptr;

		m_voReproductionModules.clear( );

		delete m_pR2RPatchbay;
		m_pR2RPatchbay = nullptr;

		delete m_pOutputPatchbay;
		m_pOutputPatchbay = nullptr;

		if( m_pOutputStreamDetector->GetProfilerEnabled( ) )
			VA_VERBOSE( "Core", "Output stream detector profiler: " << m_pOutputStreamDetector->GetProfilerResult( ) );
		delete m_pOutputStreamDetector;
		m_pOutputStreamDetector = nullptr;

		delete m_pOutputTracker;
		m_pOutputTracker = nullptr;

		delete m_pStreamProbeDeviceInput;
		m_pStreamProbeDeviceInput = nullptr;

		delete m_pStreamProbeDeviceOutput;
		m_pStreamProbeDeviceOutput = nullptr;

		delete m_pSignalSourceManager;
		m_pSignalSourceManager = nullptr;

		delete m_pGlobalSampler;
		m_pGlobalSampler = nullptr;

		delete m_pGlobalSamplePool;
		m_pGlobalSamplePool = nullptr;

		m_pDirectivityManager->Finalize( );
		delete m_pDirectivityManager;
		m_pDirectivityManager = nullptr;

		m_pSceneManager->Finalize( );
		delete m_pSceneManager;
		m_pSceneManager = nullptr;

		// Finalisierung erfolgreich. Nun wieder im Grundzustand!
		m_iState = VA_CORESTATE_CREATED;

		FinishProgress( );
	}
	VA_FINALLY
	{
		// Nochmals versuchen aufzuräumen
		Tidyup( );

		// Allgemein: Fehler beim Finalisieren? => Core im Sack
		m_iState = VA_CORESTATE_FAIL;

		// VAExceptions unverändert nach aussen leiten
		throw;
	}
}

void CVACoreImpl::FinalizeAudioDriver( )
{
	if( m_pAudioDriverBackend )
	{
		VA_INFO( "Core", "Finalizing audio device \"" << m_oCoreConfig.oAudioDriverConfig.sDevice << "\" [" << m_oCoreConfig.oAudioDriverConfig.sDriver << "]" );

		m_pAudioDriverBackend->finalize( );
		delete m_pAudioDriverBackend;
		m_pAudioDriverBackend = nullptr;
	}
}

void CVACoreImpl::FinalizeRenderingModules( )
{
	for( size_t i = 0; i < m_voRenderers.size( ); i++ )
	{
		m_voRenderers[i].Finalize( );
	}
}

void CVACoreImpl::FinalizeReproductionModules( )
{
	for( size_t i = 0; i < m_voReproductionModules.size( ); i++ )
	{
		m_voReproductionModules[i].Finalize( );
	}
}
