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

ITACriticalSection m_csProgress;
CVAProgress m_Progress;

void CVACoreImpl::InitProgress( const std::string& sAction, const std::string& sSubaction, const int iMaxStep )
{
	m_csProgress.enter( );

	m_Progress.sAction      = sAction;
	m_Progress.sSubaction   = sSubaction;
	m_Progress.iCurrentStep = 0;
	m_Progress.iMaxStep     = iMaxStep;

	// Fortschritts-Ereignis senden
	CVAEvent ev;
	ev.iEventType = CVAEvent::PROGRESS_UPDATE;
	ev.pSender    = this;
	ev.oProgress  = m_Progress;
	m_pEventManager->BroadcastEvent( ev );

	m_csProgress.leave( );
}

void CVACoreImpl::SetProgress( const std::string& sAction, const std::string& sSubaction, const int iCurrentStep )
{
	m_csProgress.enter( );

	m_Progress.sAction      = sAction;
	m_Progress.sSubaction   = sSubaction;
	m_Progress.iCurrentStep = iCurrentStep;

	// Fortschritts-Ereignis senden
	CVAEvent ev;
	ev.iEventType = CVAEvent::PROGRESS_UPDATE;
	ev.pSender    = this;
	ev.oProgress  = m_Progress;
	m_pEventManager->BroadcastEvent( ev );

	m_csProgress.leave( );
}

void CVACoreImpl::FinishProgress( )
{
	m_csProgress.enter( );

	// Die Aktionen bleiben, nur die aktuelle Schritt = maximaler Schritt
	m_Progress.iCurrentStep = m_Progress.iMaxStep;

	// Fortschritts-Ereignis senden
	CVAEvent ev;
	ev.iEventType = CVAEvent::PROGRESS_UPDATE;
	ev.pSender    = this;
	ev.oProgress  = m_Progress;
	m_pEventManager->BroadcastEvent( ev );

	m_csProgress.leave( );
}
