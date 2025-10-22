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

void CVACoreImpl::Reset( )
{
	VA_CHECK_INITIALIZED;

	// Wait for core thread
	while( !m_pCoreThread->TryBreak( ) )
		VASleep( 20 );

	VA_NO_REENTRANCE;

	if( GetUpdateLocked( ) )
	{
		VA_WARN( "Core", "Encountered locked scene during reset. Please unlock before resetting, skipping." );
		m_pCoreThread->Continue( );
	}

	VA_TRY
	{
		VA_INFO( "Core", "Resetting core" );

		// Reset audio renderers
		std::vector<CVAAudioRendererDesc>::iterator it = m_voRenderers.begin( );
		while( it != m_voRenderers.end( ) )
		{
			CVAAudioRendererDesc& oRend( *it++ );
			oRend.pInstance->Reset( );
		}

		if( m_pCurSceneState )
		{
			// Referenz entfernen welche in CoreThreadLoop hinzugefügt wurde
			m_pCurSceneState->RemoveReference( );
			m_pCurSceneState = nullptr;
		}

		// Alle Szenenobjekte löschen
		m_pSceneManager->Reset( );
		m_pSignalSourceManager->Reset( );
		m_pDirectivityManager->Reset( );

		// TODO: Check if the pool and sampler must really be recreated
		/*
		delete m_pGlobalSamplePool;
		m_pGlobalSamplePool = nullptr;
		m_pGlobalSamplePool = ITASoundSamplePool::Create(1, m_oCoreConfig.oAudioDriverConfig.dSamplerate);

		// This causes a crash in patch bay
		delete m_pGlobalSampler;
		m_pGlobalSampler = nullptr;
		m_pGlobalSampler = ITASoundSampler::Create(1, m_oCoreConfig.oAudioDriverConfig.dSamplerate, m_oCoreConfig.oAudioDriverConfig.iBuffersize, m_pGlobalSamplePool);
		m_pGlobalSampler->AddMonoTrack();
		*/
		// m_pGlobalSampler->RemoveAllPlaybacks();

		// Werte neusetzen
		m_pCurSceneState = m_pSceneManager->GetHeadSceneState( );

		m_pCoreThread;

		// Core-Thread fortsetzen
		m_pCoreThread->Continue( );

		// Ereignis generieren, wenn Operation erfolgreich
		CVAEvent ev;
		ev.iEventType = CVAEvent::RESET;
		ev.pSender    = this;
		m_pEventManager->BroadcastEvent( ev );
	}
	VA_FINALLY
	{
		Tidyup( );
		throw;
	}
}
