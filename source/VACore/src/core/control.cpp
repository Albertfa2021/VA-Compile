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

bool CVACoreImpl::GetInputMuted( ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	return m_bInputMuted;
}

void CVACoreImpl::SetInputMuted( const bool bMuted )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		if( m_bInputMuted == bMuted )
			return;

		m_bInputMuted = bMuted;

		if( m_pInputAmp )
		{
			if( m_bInputMuted )
				m_pInputAmp->SetGain( 0.0f );
			else
				m_pInputAmp->SetGain( (float)m_dInputGain );
		}

		VA_INFO( "Core", "Input mute toggled, now " << ( m_bInputMuted ? "muted" : "not muted" ) );

		CVAEvent ev;
		ev.iEventType = CVAEvent::INPUT_MUTING_CHANGED;
		ev.pSender    = this;
		ev.bMuted     = m_bInputMuted;
		m_pEventManager->BroadcastEvent( ev );
	}
	VA_RETHROW;
}

double CVACoreImpl::GetInputGain( ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	return m_dInputGain;
}

void CVACoreImpl::SetInputGain( double dGain )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		if( !GetVolumeValid( dGain ) )
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid gain" );

		if( m_dInputGain == dGain )
			return;
		m_dInputGain = dGain;

		if( m_pInputAmp )
		{
			if( m_bInputMuted )
				m_pInputAmp->SetGain( 0 );
			else
				m_pInputAmp->SetGain( (float)dGain );
		}

		// Ereignis generieren
		CVAEvent ev;
		ev.iEventType = CVAEvent::INPUT_GAIN_CHANGED;
		ev.pSender    = this;
		ev.dVolume    = dGain;
		m_pEventManager->BroadcastEvent( ev );

		VA_VERBOSE( "Core", "New input gain: " << IVAInterface::GetVolumeStrDecibel( dGain ) << "dB" );
	}
	VA_RETHROW;
}

double CVACoreImpl::GetOutputGain( ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	return m_dOutputGain;
}

void CVACoreImpl::SetOutputGain( const double dGain )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		if( !GetVolumeValid( dGain ) )
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid gain" );

		if( m_dOutputGain == dGain )
			return;
		m_dOutputGain = dGain;
		if( m_pOutputPatchbay )
		{
			if( m_bOutputMuted )
				m_pOutputPatchbay->SetOutputGain( 0, 0.0f );
			else
				m_pOutputPatchbay->SetOutputGain( 0, dGain );
		}

		// Ereignis generieren
		CVAEvent ev;
		ev.iEventType = CVAEvent::OUTPUT_GAIN_CHANGED;
		ev.pSender    = this;
		ev.dVolume    = dGain;
		m_pEventManager->BroadcastEvent( ev );

		VA_VERBOSE( "Core", "New output gain: " << IVAInterface::GetVolumeStrDecibel( dGain ) << "dB" );
	}
	VA_RETHROW;
}

bool CVACoreImpl::GetOutputMuted( ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	return m_bOutputMuted;
}

void CVACoreImpl::SetOutputMuted( const bool bMuted )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		if( m_bOutputMuted == bMuted )
			return;
		m_bOutputMuted = bMuted;
		VA_INFO( "Core", "Output mute toggled, now " << ( m_bOutputMuted ? "muted" : "not muted" ) );

		if( m_pOutputPatchbay )
			m_pOutputPatchbay->SetOutputMuted( 0, bMuted );

		// Ereignis generieren
		CVAEvent ev;
		ev.iEventType = CVAEvent::OUTPUT_MUTING_CHANGED;
		ev.pSender    = this;
		ev.bMuted     = m_bOutputMuted;
		m_pEventManager->BroadcastEvent( ev );
	}
	VA_RETHROW;
}

int CVACoreImpl::GetGlobalAuralizationMode( ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	return m_iGlobalAuralizationMode;
}

void CVACoreImpl::SetGlobalAuralizationMode( const int iAuralizationMode )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		if( !GetAuralizationModeValid( iAuralizationMode ) )
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid auralization mode" );

		if( m_iGlobalAuralizationMode == iAuralizationMode )
		{
			return;
		}

		m_iGlobalAuralizationMode = iAuralizationMode;

		// Neuberechnung der Schallpfade triggern
		m_pCoreThread->Trigger( );

		// Ereignis generieren
		CVAEvent ev;
		ev.iEventType        = CVAEvent::GLOBAL_AURALIZATION_MODE_CHANGED;
		ev.pSender           = this;
		ev.iAuralizationMode = iAuralizationMode;
		m_pEventManager->BroadcastEvent( ev );

		VA_INFO( "Core", "Global auralization mode has been updated, now " << IVAInterface::GetAuralizationModeStr( iAuralizationMode, true ) );
	}
	VA_RETHROW;
}
