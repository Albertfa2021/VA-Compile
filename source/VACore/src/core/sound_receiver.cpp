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

void CVACoreImpl::GetSoundReceiverIDs( std::vector<int>& vSoundReceiverIDs ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY { m_pSceneManager->GetHeadSceneState( )->GetListenerIDs( &vSoundReceiverIDs ); }
	VA_RETHROW;
}

int CVACoreImpl::CreateSoundReceiver( const std::string& sName )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		int iNumSoundReceivers = VACORE_MAX_NUM_SOUND_RECEIVERS;
		if( ( iNumSoundReceivers != 0 ) && m_pNewSceneState )
		{
			const int iListenersRemain = VACORE_MAX_NUM_SOUND_RECEIVERS - m_pNewSceneState->GetNumSoundReceivers( );
			if( iListenersRemain <= 0 )
			{
				std::stringstream ss;
				ss << "Maximum number of listeners reached. This version of VA only supports up to " << VACORE_MAX_NUM_SOUND_RECEIVERS << " listeners.";
				VA_EXCEPT2( INVALID_PARAMETER, ss.str( ) );
			}
		}

		const bool bSync = GetUpdateLocked( );
		if( !bSync )
			LockUpdate( );

		// Hörer anlegen
		const int iID = m_pNewSceneState->AddListener( );
		assert( iID != -1 );

		// HINWEIS: Zunächst hier die statische Beschreibung der Quelle anlegen
		m_pSceneManager->CreateSoundReceiverDesc( iID );

		// Initiale Werte setzen
		m_pSceneManager->SetSoundReceiverName( iID, sName );
		CVAReceiverState* pReceiverState = m_pNewSceneState->AlterReceiverState( iID );
		assert( pReceiverState );

		// Ereignis generieren
		CVAEvent ev;
		ev.iEventType = CVAEvent::SOUND_RECEIVER_CREATED;
		ev.pSender    = this;
		ev.iObjectID  = iID;
		ev.sName      = sName;

		m_pEventManager->EnqueueEvent( ev );

		if( !bSync )
			UnlockUpdate( );

		VA_INFO( "Core", "Created sound receiver '" << sName << "' and assigned ID " << iID );

		return iID;
	}
	VA_RETHROW;
}

int CVACoreImpl::DeleteSoundReceiver( const int iID )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	bool bSync = GetUpdateLocked( );
	if( !bSync )
		LockUpdate( );

	VA_TRY
	{
		int iCurrentDirectivityID = m_pNewSceneState->GetReceiverState( iID )->GetDirectivityID( );

		m_pNewSceneState->RemoveReceiver( iID );

		if( iCurrentDirectivityID != -1 )
			m_pDirectivityManager->ReleaseDirectivity( iCurrentDirectivityID );

		// Ereignis generieren, falls erfolgreich
		CVAEvent ev;
		ev.iEventType = CVAEvent::SOUND_RECEIVER_DELETED;
		ev.pSender    = this;
		ev.iObjectID  = iID;
		m_pEventManager->EnqueueEvent( ev );

		if( !bSync )
			UnlockUpdate( );

		VA_INFO( "Core", "Deleted sound receiver " << iID );

		return 0;
	}
	VA_RETHROW;
}

CVASoundReceiverInfo CVACoreImpl::GetSoundReceiverInfo( const int iID ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		const CVASoundReceiverDesc* oDesc = m_pSceneManager->GetSoundReceiverDesc( iID );
		CVASoundReceiverInfo oInfo;
		oInfo.iID   = oDesc->iID;
		oInfo.sName = oDesc->sName;
		return oInfo;
		// @todo improve
	}
	VA_RETHROW;
}

int CVACoreImpl::CreateSoundReceiverExplicitRenderer( const std::string& sRendererID, const std::string& sName )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	bool bSync = GetUpdateLocked( );
	if( !bSync )
		LockUpdate( );

	VA_TRY
	{
		int iNumSoundReceivers = VACORE_MAX_NUM_SOUND_RECEIVERS;
		if( ( iNumSoundReceivers != 0 ) && m_pNewSceneState )
		{
			int iListenersRemain = VACORE_MAX_NUM_SOUND_RECEIVERS - m_pNewSceneState->GetNumSoundReceivers( );
			if( iListenersRemain <= 0 )
			{
				std::stringstream ss;
				ss << "Maximum number of listeners reached. This version of VA only supports up to " << VACORE_MAX_NUM_SOUND_RECEIVERS << " listeners.";
				VA_EXCEPT2( INVALID_PARAMETER, ss.str( ) );
			}
		}

		int iListenerID = m_pNewSceneState->AddListener( );
		assert( iListenerID != -1 );

		CVASoundReceiverDesc* pDesc = m_pSceneManager->CreateSoundReceiverDesc( iListenerID );
		pDesc->sExplicitRendererID  = sRendererID;

		m_pSceneManager->SetSoundReceiverName( iListenerID, sName );
		CVAReceiverState* pReceiverState = m_pNewSceneState->AlterReceiverState( iListenerID );
		assert( pReceiverState );

		CVAEvent ev;
		ev.iEventType        = CVAEvent::SOUND_RECEIVER_CREATED;
		ev.pSender           = this;
		ev.iObjectID         = iListenerID;
		ev.sName             = sName;
		ev.iAuralizationMode = VA_AURAMODE_ALL;
		ev.dVolume           = 1.0f;
		m_pEventManager->EnqueueEvent( ev );

		if( !bSync )
			UnlockUpdate( );

		VA_INFO( "Core", "Created sound receiver (ID=" << iListenerID << ", Name=\"" << sName << "\") explicitly for renderer '" << sRendererID << "' only" );

		return iListenerID;
	}
	VA_FINALLY
	{
		if( bSync )
			UnlockUpdate( );
		throw;
	}
}

void CVACoreImpl::SetSoundReceiverEnabled( const int iID, const bool bEnabled )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		CVASoundReceiverDesc* pDesc = m_pSceneManager->GetSoundReceiverDesc( iID );

		if( pDesc->bEnabled != bEnabled )
		{
			pDesc->bEnabled = bEnabled;

			CVAEvent ev;
			ev.iEventType = CVAEvent::SOUND_RECEIVER_CHANGED_NAME; // @todo JST new event type
			ev.pSender    = this;
			ev.iObjectID  = iID;
			// ev.bEnabled = bEnabled; // @todo JST
			m_pEventManager->EnqueueEvent( ev );

			// Trigger core thread
			m_pCoreThread->Trigger( );

			VA_INFO( "Core", "Set sound receiver with id " << iID << ( bEnabled ? " enabled" : " disabled" ) );
		}
	}
	VA_RETHROW;
}

bool CVACoreImpl::GetSoundReceiverEnabled( const int iSoundReceiverID ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		const CVASoundReceiverDesc* pDesc = m_pSceneManager->GetSoundReceiverDesc( iSoundReceiverID );
		return pDesc->bEnabled;
	}
	VA_RETHROW;
}

std::string CVACoreImpl::GetSoundReceiverName( const int iID ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY { return m_pSceneManager->GetSoundReceiverName( iID ); }
	VA_RETHROW;
}

void CVACoreImpl::SetSoundReceiverName( const int iID, const std::string& sName )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		m_pSceneManager->SetSoundReceiverName( iID, sName );

		// Ereignis generieren, falls erfolgreich
		CVAEvent ev;
		ev.iEventType = CVAEvent::SOUND_RECEIVER_CHANGED_NAME;
		ev.pSender    = this;
		ev.iObjectID  = iID;
		ev.sName      = sName;
		m_pEventManager->BroadcastEvent( ev );

		VA_INFO( "Core", "Set sound receiver name of " << iID << " to " << sName );
	}
	VA_RETHROW;
}

int CVACoreImpl::GetSoundReceiverAuralizationMode( const int iID ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		CVASceneState* pHeadState              = m_pSceneManager->GetHeadSceneState( );
		const CVAReceiverState* pReceiverState = pHeadState->GetReceiverState( iID );

		if( !pReceiverState )
			// Hörer existiert nicht
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid sound receiver ID" );

		return pReceiverState->GetAuralizationMode( );
	}
	VA_RETHROW;
}

void CVACoreImpl::SetSoundReceiverAuralizationMode( const int iID, const int iAuralizationMode )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		// Parameter überprüfen
		if( !GetAuralizationModeValid( iAuralizationMode ) )
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid auralization mode" );

		bool bSync = GetUpdateLocked( );
		if( !bSync )
			LockUpdate( );

		CVAReceiverState* pReceiverState = m_pNewSceneState->AlterReceiverState( iID );

		if( !pReceiverState )
		{
			// Hörer existiert nicht
			if( !bSync )
				UnlockUpdate( );
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid sound receiver ID" );
		}

		pReceiverState->SetAuralizationMode( iAuralizationMode );

		// Ereignis generieren, falls erfolgreich
		CVAEvent ev;
		ev.iEventType        = CVAEvent::SOUND_RECEIVER_CHANGED_AURALIZATIONMODE;
		ev.pSender           = this;
		ev.iObjectID         = iID;
		ev.iAuralizationMode = iAuralizationMode;
		m_pEventManager->EnqueueEvent( ev );

		if( !bSync )
			UnlockUpdate( );

		VA_INFO( "Core", "Changed sound receiver " << iID << " auralization mode: " << IVAInterface::GetAuralizationModeStr( iAuralizationMode, true ) );
	}
	VA_RETHROW;
}

void CVACoreImpl::SetSoundReceiverParameters( const int iID, const CVAStruct& oParams )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		bool bSync = GetUpdateLocked( );
		if( !bSync )
			LockUpdate( );

		CVAReceiverState* pReceiverState = m_pNewSceneState->AlterReceiverState( iID );

		if( !pReceiverState )
		{
			if( !bSync )
				UnlockUpdate( );

			VA_EXCEPT2( INVALID_PARAMETER, "Invalid sound receiver ID " + std::to_string( long( iID ) ) );
		}

		pReceiverState->SetParameters( oParams );

		// Ereignis generieren, falls erfolgreich
		CVAEvent ev;
		ev.iEventType = CVAEvent::SOUND_RECEIVER_CHANGED_NAME; // @todo create own core event "parameter changed"
		ev.pSender    = this;
		ev.iObjectID  = iID;
		// ev.oStruct @todo: add a struct to the event
		m_pEventManager->EnqueueEvent( ev );

		if( !bSync )
			UnlockUpdate( );

		VA_INFO( "Core", "Changed sound receiver " << iID << " parameters" );
	}
	VA_RETHROW;
}

CVAStruct CVACoreImpl::GetSoundReceiverParameters( const int iID, const CVAStruct& oArgs ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		CVASceneState* pHeadState              = m_pSceneManager->GetHeadSceneState( );
		const CVAReceiverState* pReceiverState = pHeadState->GetReceiverState( iID );

		if( !pReceiverState )
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid sound receiver ID" );

		return pReceiverState->GetParameters( oArgs );
	}
	VA_RETHROW;
}

int CVACoreImpl::GetSoundReceiverDirectivity( const int iID ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		CVASceneState* pHeadState              = m_pSceneManager->GetHeadSceneState( );
		const CVAReceiverState* pReceiverState = pHeadState->GetReceiverState( iID );

		if( !pReceiverState )
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid sound receiver ID" );

		return pReceiverState->GetDirectivityID( );
	}
	VA_RETHROW;
}

void CVACoreImpl::SetSoundReceiverDirectivity( const int iID, const int iDirectivityID )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		// Parameter überprüfen
		const IVADirectivity* pDirectivity = nullptr;
		if( iDirectivityID != -1 )
		{
			pDirectivity = m_pDirectivityManager->RequestDirectivity( iDirectivityID ); // Add a reference
			if( !pDirectivity )
				VA_EXCEPT2( INVALID_ID, "Invalid directivity dataset for ID " + std::to_string( iID ) );
		}

		bool bSync = GetUpdateLocked( );
		if( !bSync )
			LockUpdate( );

		CVAReceiverState* pReceiverState = m_pNewSceneState->AlterReceiverState( iID );

		if( !pReceiverState )
		{
			// Quelle existiert nicht
			if( !bSync )
				UnlockUpdate( );

			if( pDirectivity )
				m_pDirectivityManager->ReleaseDirectivity( iDirectivityID ); // Remove reference again, if receiver ID was invalid

			VA_EXCEPT2( INVALID_PARAMETER, "Invalid sound receiver ID " + std::to_string( iID ) );
		}

		auto iCurrentDirectivity = pReceiverState->GetDirectivityID( );
		if( iCurrentDirectivity != -1 )
			m_pDirectivityManager->ReleaseDirectivity( iCurrentDirectivity );

		pReceiverState->SetDirectivityID( iDirectivityID );
		pReceiverState->SetDirectivity( pDirectivity );

		// Ereignis generieren, falls erfolgreich
		CVAEvent ev;
		ev.iEventType = CVAEvent::SOUND_RECEIVER_CHANGED_DIRECTIVITY;
		ev.pSender    = this;
		ev.iObjectID  = iID;
		ev.iParamID   = iDirectivityID;
		m_pEventManager->EnqueueEvent( ev );

		if( !bSync )
			UnlockUpdate( );

		VA_INFO( "Core", "Linked sound receiver " + std::to_string( long( iID ) ) + " with receiver directivity dataset " + std::to_string( long( iDirectivityID ) ) );
	}
	VA_RETHROW;
}

bool CVACoreImpl::GetSoundReceiverMuted( const int iID ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		const CVASoundReceiverDesc* pDesc = m_pSceneManager->GetSoundReceiverDesc( iID );
		return pDesc->bMuted;
	}
	VA_RETHROW;
}

void CVACoreImpl::SetSoundReceiverMuted( const int iID, const bool bMuted )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		CVASoundReceiverDesc* pDesc = m_pSceneManager->GetSoundReceiverDesc( iID );

		if( pDesc->bMuted != bMuted )
		{
			pDesc->bMuted = bMuted;

			CVAEvent ev;
			ev.iEventType = CVAEvent::SOUND_RECEIVER_CHANGED_MUTING;
			ev.pSender    = this;
			ev.iObjectID  = iID;
			ev.bMuted     = bMuted;
			m_pEventManager->EnqueueEvent( ev );

			// Trigger core thread
			m_pCoreThread->Trigger( );
		}
	}
	VA_RETHROW;
}

int CVACoreImpl::GetSoundReceiverGeometryMesh( const int iID ) const
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

void CVACoreImpl::SetSoundReceiverGeometryMesh( const int iSoundReceiverID, const int iGeometryMeshID )
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

void CVACoreImpl::GetSoundReceiverPose( const int iID, VAVec3& v3Pos, VAQuat& qOrient ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		CVASceneState* pHeadState              = m_pSceneManager->GetHeadSceneState( );
		const CVAReceiverState* pReceiverState = pHeadState->GetReceiverState( iID );

		if( !pReceiverState )
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid sound receiver ID" );

		const CVAMotionState* pMotionState = pReceiverState->GetMotionState( );
		if( !pMotionState )
			VA_EXCEPT2( INVALID_PARAMETER, "SoundReceiver has invalid motion state, probably it has not been positioned, yet?" );

		v3Pos   = pMotionState->GetPosition( );
		qOrient = pMotionState->GetOrientation( );
	}
	VA_RETHROW;
}

void CVACoreImpl::SetSoundReceiverPose( const int iID, const VAVec3& v3Pos, const VAQuat& qOrient )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		bool bSync = GetUpdateLocked( );
		if( !bSync )
			LockUpdate( );

		CVAReceiverState* pState = m_pNewSceneState->AlterReceiverState( iID );

		if( !pState )
		{
			if( !bSync )
				UnlockUpdate( );
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid sound receiver ID, not found in scene" );
		}

		CVAMotionState* pNewMotionState = pState->AlterMotionState( );
		pNewMotionState->SetPosition( v3Pos );
		pNewMotionState->SetOrientation( qOrient );

		// m_pSceneManager->GetListenerDesc( iID )->bInitPositionOrientation = true; // @todo

		// Ereignis generieren, falls erfolgreich
		CVAEvent ev;
		ev.iEventType = CVAEvent::SOUND_RECEIVER_CHANGED_POSE;
		ev.pSender    = this;
		ev.iObjectID  = iID;
		ev.vPos       = v3Pos;
		// ev.qOrient = qOrient; // @todo
		ConvertQuaternionToViewUp( qOrient, ev.vView, ev.vUp );
		SetCoreEventParams( ev, pNewMotionState );

		m_pEventManager->EnqueueEvent( ev );

		if( !bSync )
			UnlockUpdate( );

		VA_TRACE( "Core", "Updated sound receiver " << iID << " pose" );
	}
	VA_RETHROW;
}

VAVec3 CVACoreImpl::GetSoundReceiverPosition( const int iID ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		CVASceneState* pHeadState                   = m_pSceneManager->GetHeadSceneState( );
		const CVAReceiverState* pSoundReceiverState = pHeadState->GetReceiverState( iID );

		if( !pSoundReceiverState )
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid sound receiver ID" );

		const CVAMotionState* pMotionState = pSoundReceiverState->GetMotionState( );
		if( !pMotionState )
			VA_EXCEPT2( INVALID_PARAMETER, "SoundReceiver has invalid motion state, probably it has not been positioned, yet?" );

		return pMotionState->GetPosition( );
	}
	VA_RETHROW;
}

void CVACoreImpl::SetSoundReceiverPosition( const int iID, const VAVec3& v3Pos )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		bool bSync = GetUpdateLocked( );
		if( !bSync )
			LockUpdate( );

		CVAReceiverState* pReceiverState = m_pNewSceneState->AlterReceiverState( iID );

		if( !pReceiverState )
		{
			if( !bSync )
				UnlockUpdate( );
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid sound receiver ID" );
		}

		CVAMotionState* pNewMotionState = pReceiverState->AlterMotionState( );
		pNewMotionState->SetPosition( v3Pos );

		// Ereignis generieren
		CVAEvent ev;
		ev.iEventType = CVAEvent::SOUND_RECEIVER_CHANGED_POSE;
		ev.pSender    = this;
		ev.iObjectID  = iID;
		SetCoreEventParams( ev, pNewMotionState );

		m_pEventManager->EnqueueEvent( ev );

		if( !bSync )
			UnlockUpdate( );

		VA_TRACE( "Core", "Updated sound receiver " << iID << " position" );
	}
	VA_RETHROW;
}

void CVACoreImpl::GetSoundReceiverOrientationVU( const int iID, VAVec3& vView, VAVec3& vUp ) const
{
	ConvertQuaternionToViewUp( GetSoundReceiverOrientation( iID ), vView, vUp );
}

void CVACoreImpl::SetSoundReceiverOrientationVU( const int iID, const VAVec3& v3View, const VAVec3& v3Up )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		const bool bSync = GetUpdateLocked( );
		if( !bSync )
			LockUpdate( );

		CVAReceiverState* pReceiverState = m_pNewSceneState->AlterReceiverState( iID );

		if( !pReceiverState )
		{
			if( !bSync )
				UnlockUpdate( );
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid sound receiver ID" );
		}

		CVAMotionState* pNewMotionState = pReceiverState->AlterMotionState( );
		pNewMotionState->SetOrientationVU( v3View, v3Up );

		// Ereignis generieren
		CVAEvent ev;
		ev.iEventType = CVAEvent::SOUND_RECEIVER_CHANGED_POSE;
		ev.pSender    = this;
		ev.iObjectID  = iID;
		SetCoreEventParams( ev, pNewMotionState );

		m_pEventManager->EnqueueEvent( ev );

		if( !bSync )
			UnlockUpdate( );
	}
	VA_RETHROW;
}

VAQuat CVACoreImpl::GetSoundReceiverOrientation( const int iID ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		CVASceneState* pHeadState                   = m_pSceneManager->GetHeadSceneState( );
		const CVAReceiverState* pSoundReceiverState = pHeadState->GetReceiverState( iID );

		if( !pSoundReceiverState )
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid sound receiver ID" );

		const CVAMotionState* pMotionState = pSoundReceiverState->GetMotionState( );
		if( !pMotionState )
			VA_EXCEPT2( INVALID_PARAMETER, "Sound receiver has invalid motion state, probably it has not been positioned, yet?" );

		return pMotionState->GetOrientation( );
	}
	VA_RETHROW;
}

void CVACoreImpl::SetSoundReceiverOrientation( const int iID, const VAQuat& qOrient )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		const bool bSync = GetUpdateLocked( );
		if( !bSync )
			LockUpdate( );

		CVAReceiverState* pReceiverState = m_pNewSceneState->AlterReceiverState( iID );

		if( !pReceiverState )
		{
			if( !bSync )
				UnlockUpdate( );
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid sound receiver ID" );
		}

		CVAMotionState* pNewMotionState = pReceiverState->AlterMotionState( );
		pNewMotionState->SetOrientation( qOrient );

		// Ereignis generieren
		CVAEvent ev;
		ev.iEventType = CVAEvent::SOUND_RECEIVER_CHANGED_POSE;
		ev.pSender    = this;
		ev.iObjectID  = iID;
		SetCoreEventParams( ev, pNewMotionState );

		m_pEventManager->EnqueueEvent( ev );

		if( !bSync )
			UnlockUpdate( );

		VA_TRACE( "Core", "Updated sound receiver " << iID << " orientation" );
	}
	VA_RETHROW;
}

VAQuat CVACoreImpl::GetSoundReceiverHeadAboveTorsoOrientation( const int iID ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		CVASceneState* pHeadState              = m_pSceneManager->GetHeadSceneState( );
		const CVAReceiverState* pReceiverState = pHeadState->GetReceiverState( iID );

		if( !pReceiverState )
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid sound receiver ID" );

		const CVAMotionState* pMotionState = pReceiverState->GetMotionState( );
		if( !pMotionState )
			VA_EXCEPT2( INVALID_PARAMETER, "Sound receiver has invalid motion state, probably it has not been positioned, yet?" );

		return pMotionState->GetHeadAboveTorsoOrientation( );
	}
	VA_RETHROW;
}

void CVACoreImpl::SetSoundReceiverHeadAboveTorsoOrientation( const int iID, const VAQuat& qOrient )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		const bool bSync = GetUpdateLocked( );
		if( !bSync )
			LockUpdate( );

		CVAReceiverState* pReceiverState = m_pNewSceneState->AlterReceiverState( iID );

		if( !pReceiverState )
		{
			if( !bSync )
				UnlockUpdate( );
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid sound receiver ID" );
		}

		CVAMotionState* pNewMotionState = pReceiverState->AlterMotionState( );
		pNewMotionState->SetHeadAboveTorsoOrientation( qOrient );

		CVAEvent ev;
		ev.iEventType = CVAEvent::SOUND_RECEIVER_CHANGED_POSE;
		ev.pSender    = this;
		ev.iObjectID  = iID;
		SetCoreEventParams( ev, pNewMotionState );
		m_pEventManager->EnqueueEvent( ev );

		if( !bSync )
			UnlockUpdate( );

		VA_TRACE( "Core", "Updated sound receiver " << iID << " head-above-torso orientation" );
	}
	VA_RETHROW;
}

void CVACoreImpl::GetSoundReceiverRealWorldPositionOrientationVU( const int iID, VAVec3& v3Pos, VAVec3& v3View, VAVec3& v3Up ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		CVASceneState* pHeadState              = m_pSceneManager->GetHeadSceneState( );
		const CVAReceiverState* pReceiverState = pHeadState->GetReceiverState( iID );

		if( !pReceiverState )
			// Hörer existiert nicht
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid sound receiver ID" );

		const CVAMotionState* pMotionState = pReceiverState->GetMotionState( );
		if( !pMotionState )
			VA_EXCEPT2( INVALID_PARAMETER, "Sound receiver has invalid motion state, probably it has not been positioned, yet?" );

		v3Pos          = pMotionState->GetRealWorldPose( ).vPos;
		VAQuat qOrient = pMotionState->GetRealWorldPose( ).qOrient;
		ConvertQuaternionToViewUp( qOrient, v3View, v3Up );
	}
	VA_RETHROW;
}

void CVACoreImpl::SetSoundReceiverRealWorldPositionOrientationVU( const int iID, const VAVec3& v3Pos, const VAVec3& v3View, const VAVec3& v3Up )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		const bool bSync = GetUpdateLocked( );
		if( !bSync )
			LockUpdate( );

		CVAReceiverState* pReceiverState = m_pNewSceneState->AlterReceiverState( iID );

		if( !pReceiverState )
		{
			if( !bSync )
				UnlockUpdate( );
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid sound receiver ID" );
		}

		CVAMotionState* pNewMotionState = pReceiverState->AlterMotionState( );
		CVAMotionState::CVAPose oNewPose;
		oNewPose.vPos = v3Pos;
		ConvertViewUpToQuaternion( v3View, v3Up, oNewPose.qOrient );
		pNewMotionState->SetRealWorldPose( oNewPose );

		CVAEvent ev;
		ev.iEventType = CVAEvent::SOUND_RECEIVER_REAL_WORLD_POSE_CHANGED;
		ev.pSender    = this;
		ev.iObjectID  = iID;
		SetCoreEventParams( ev, pNewMotionState );
		m_pEventManager->EnqueueEvent( ev );

		if( !bSync )
			UnlockUpdate( );

		VA_TRACE( "Core", "Updated sound receiver " << iID << " real-world position and view/up orientation" );
	}
	VA_RETHROW;
}

void CVACoreImpl::GetSoundReceiverRealWorldPose( const int iID, VAVec3& v3Pos, VAQuat& qOrient ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		CVASceneState* pHeadState              = m_pSceneManager->GetHeadSceneState( );
		const CVAReceiverState* pReceiverState = pHeadState->GetReceiverState( iID );

		if( !pReceiverState )
			// Hörer existiert nicht
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid sound receiver ID" );

		const CVAMotionState* pMotionState = pReceiverState->GetMotionState( );
		if( !pMotionState )
			VA_EXCEPT2( INVALID_PARAMETER, "Sound receiver has invalid motion state, probably it has not been positioned, yet?" );

		v3Pos   = pMotionState->GetRealWorldPose( ).vPos;
		qOrient = pMotionState->GetRealWorldPose( ).qOrient;
	}
	VA_RETHROW;
}

void CVACoreImpl::SetSoundReceiverRealWorldPose( const int iID, const VAVec3& v3Pos, const VAQuat& qOrient )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		const bool bSync = GetUpdateLocked( );
		if( !bSync )
			LockUpdate( );

		CVAReceiverState* pReceiverState = m_pNewSceneState->AlterReceiverState( iID );

		if( !pReceiverState )
		{
			if( !bSync )
				UnlockUpdate( );
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid sound receiver ID" );
		}

		CVAMotionState* pNewMotionState = pReceiverState->AlterMotionState( );
		CVAMotionState::CVAPose oNewPose;
		oNewPose.vPos    = v3Pos;
		oNewPose.qOrient = qOrient;
		pNewMotionState->SetRealWorldPose( oNewPose );

		CVAEvent ev;
		ev.iEventType = CVAEvent::SOUND_RECEIVER_CHANGED_POSE;
		ev.pSender    = this;
		ev.iObjectID  = iID;
		SetCoreEventParams( ev, pNewMotionState );
		m_pEventManager->EnqueueEvent( ev );

		if( !bSync )
			UnlockUpdate( );

		VA_TRACE( "Core", "Updated sound receiver " << iID << " real-world pose" );
	}
	VA_RETHROW;
}

VAQuat CVACoreImpl::GetSoundReceiverRealWorldHeadAboveTorsoOrientation( const int iID ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		CVASceneState* pHeadState              = m_pSceneManager->GetHeadSceneState( );
		const CVAReceiverState* pReceiverState = pHeadState->GetReceiverState( iID );

		if( !pReceiverState )
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid sound receiver ID" );

		const CVAMotionState* pMotionState = pReceiverState->GetMotionState( );
		if( !pMotionState )
			VA_EXCEPT2( INVALID_PARAMETER, "Sound receiver has invalid motion state, probably it has not been positioned, yet?" );

		return pMotionState->GetRealWorldHeadAboveTorsoOrientation( );
	}
	VA_RETHROW;
}

void CVACoreImpl::SetSoundReceiverRealWorldHeadAboveTorsoOrientation( const int iID, const VAQuat& qOrient )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		const bool bSync = GetUpdateLocked( );
		if( !bSync )
			LockUpdate( );

		CVAReceiverState* pReceiverState = m_pNewSceneState->AlterReceiverState( iID );

		if( !pReceiverState )
		{
			if( !bSync )
				UnlockUpdate( );
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid sound receiver ID" );
		}

		CVAMotionState* pNewMotionState = pReceiverState->AlterMotionState( );
		pNewMotionState->SetRealWorldHeadAboveTorsoOrientation( qOrient );

		CVAEvent ev;
		ev.iEventType = CVAEvent::SOUND_RECEIVER_CHANGED_POSE;
		ev.pSender    = this;
		ev.iObjectID  = iID;
		SetCoreEventParams( ev, pNewMotionState );
		m_pEventManager->EnqueueEvent( ev );

		if( !bSync )
			UnlockUpdate( );

		VA_TRACE( "Core", "Updated sound receiver " << iID << " real-world head-above-torso orientation" );
	}
	VA_RETHROW;
}
