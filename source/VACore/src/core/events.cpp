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

void CVACoreImpl::AttachEventHandler( IVAEventHandler* pCoreEventHandler )
{
	VA_TRY
	{
		// Immer möglich. Unabhängig vom Zustand. Thread-safety wird im Manager geregelt.
		m_pEventManager->AttachHandler( pCoreEventHandler );
	}
	VA_RETHROW;
}

void CVACoreImpl::DetachEventHandler( IVAEventHandler* pCoreEventHandler )
{
	VA_TRY
	{
		// Immer möglich. Unabhängig vom Zustand. Thread-safety wird im Manager geregelt.
		m_pEventManager->DetachHandler( pCoreEventHandler );
	}
	VA_RETHROW;
}

void CVACoreImpl::SendAudioDeviceDetectorUpdateEvent( )
{
	CVAEvent ev;
	ev.iEventType = CVAEvent::MEASURES_UPDATE;
	ev.pSender    = this;

	if( m_pInputStreamDetector )
	{
		ev.vfInputPeaks.resize( m_oCoreConfig.oAudioDriverConfig.iInputChannels );
		m_pInputStreamDetector->GetPeaks( ev.vfInputPeaks, true );
		ev.vfInputRMSs.resize( m_oCoreConfig.oAudioDriverConfig.iInputChannels );
		m_pInputStreamDetector->GetRMSs( ev.vfInputRMSs, true );
	}

	if( m_pOutputStreamDetector )
	{
		ev.vfOutputPeaks.resize( m_oCoreConfig.oAudioDriverConfig.iOutputChannels );
		m_pOutputStreamDetector->GetPeaks( ev.vfOutputPeaks, true );
		ev.vfOutputRMSs.resize( m_oCoreConfig.oAudioDriverConfig.iOutputChannels );
		m_pOutputStreamDetector->GetRMSs( ev.vfOutputRMSs, true );
	}

	ev.fSysLoad = 0;
	// TODO: Sollte am Ende gemessen werden! => StreamTracker
	ev.fDSPLoad = 0;
	// ev.fDSPLoad = m_pAudiostreamProcessor->GetDSPLoad();

	m_pEventManager->BroadcastEvent( ev );
}
