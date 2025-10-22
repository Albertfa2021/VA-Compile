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

void CVACoreImpl::GetSoundSourceIDs( std::vector<int>& vSoundSourceIDs )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY { m_pSceneManager->GetHeadSceneState( )->GetSoundSourceIDs( &vSoundSourceIDs ); }
	VA_RETHROW;
}

CVASoundSourceInfo CVACoreImpl::GetSoundSourceInfo( const int iID ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		const CVASoundSourceDesc* oDesc = m_pSceneManager->GetSoundSourceDesc( iID );
		CVASoundSourceInfo oInfo;
		oInfo.iID          = oDesc->iID;
		oInfo.bMuted       = oDesc->bMuted;
		oInfo.dSpoundPower = oDesc->fSoundPower;
		oInfo.sName        = oDesc->sName;
		return oInfo;
	}
	VA_RETHROW;
}

int CVACoreImpl::CreateSoundSource( const std::string& sName )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	const bool bInSyncMode = GetUpdateLocked( );
	if( !bInSyncMode )
		LockUpdate( );

	VA_TRY
	{
		int iNumSoundSource = VACORE_MAX_NUM_SOUND_SOURCES;
		if( ( iNumSoundSource != 0 ) && m_pNewSceneState )
		{
			int iSourcesRemain = VACORE_MAX_NUM_SOUND_SOURCES - m_pNewSceneState->GetNumSoundSources( );
			if( iSourcesRemain <= 0 )
			{
				std::stringstream ss;
				ss << "Maximum number of sound sources reached. This version of VA only supports up to " << VACORE_MAX_NUM_SOUND_SOURCES << " sound sources.";
				VA_EXCEPT2( INVALID_PARAMETER, ss.str( ) );
			}
		}

		// Schallquelle anlegen
		int iSourceID = m_pNewSceneState->AddSoundSource( );
		assert( iSourceID != -1 );

		// HINWEIS: Zunächst hier die statische Beschreibung der Quelle anlegen
		CVASoundSourceDesc* pDesc = m_pSceneManager->CreateSoundSourceDesc( iSourceID );

		// Keine Signalquelle zugewiesen. Wichtig: Stillepuffer einsetzen! Removing const is intended here ...
		pDesc->psfSignalSourceInputBuf = m_pSignalSourceManager->GetSilenceBuffer( );

		// Initiale Werte setzen
		m_pSceneManager->SetSoundSourceName( iSourceID, sName );
		CVASoundSourceState* pSourceState = m_pNewSceneState->AlterSoundSourceState( iSourceID );
		assert( pSourceState );

		// Ereignis generieren
		CVAEvent ev;
		ev.iEventType = CVAEvent::SOUND_SOURCE_CREATED;
		ev.pSender    = this;
		ev.iObjectID  = iSourceID;
		ev.sName      = sName;

		m_pEventManager->EnqueueEvent( ev );

		if( !bInSyncMode )
			UnlockUpdate( );

		VA_INFO( "Core", "Created sound source '" << sName << "' and assigned ID " << iSourceID );

		return iSourceID;
	}
	VA_FINALLY
	{
		if( bInSyncMode )
			UnlockUpdate( );

		throw;
	}
}

int CVACoreImpl::CreateSoundSourceExplicitRenderer( const std::string& sRendererID, const std::string& sName )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	bool bSync = GetUpdateLocked( );
	if( !bSync )
		LockUpdate( );

	VA_TRY
	{
		int iNumSoundSource = VACORE_MAX_NUM_SOUND_SOURCES;
		if( ( iNumSoundSource != 0 ) && m_pNewSceneState )
		{
			int iSourcesRemain = VACORE_MAX_NUM_SOUND_SOURCES - m_pNewSceneState->GetNumSoundSources( );
			if( iSourcesRemain <= 0 )
			{
				std::stringstream ss;
				ss << "Maximum number of sound sources reached. This version of VA only supports up to " << VACORE_MAX_NUM_SOUND_SOURCES << " sound sources.";
				VA_EXCEPT2( INVALID_PARAMETER, ss.str( ) );
			}
		}

		int iSourceID = m_pNewSceneState->AddSoundSource( );
		assert( iSourceID != -1 );

		CVASoundSourceDesc* pDesc      = m_pSceneManager->CreateSoundSourceDesc( iSourceID );
		pDesc->sExplicitRendererID     = sRendererID;
		pDesc->psfSignalSourceInputBuf = (ITASampleFrame*)m_pSignalSourceManager->GetSilenceBuffer( ); // Removing const is intended here ...

		m_pSceneManager->SetSoundSourceName( iSourceID, sName );
		CVASoundSourceState* pSourceState = m_pNewSceneState->AlterSoundSourceState( iSourceID );
		assert( pSourceState );

		CVAEvent ev;
		ev.iEventType        = CVAEvent::SOUND_SOURCE_CREATED;
		ev.pSender           = this;
		ev.iObjectID         = iSourceID;
		ev.sName             = sName;
		ev.iAuralizationMode = VA_AURAMODE_ALL;
		ev.dVolume           = 1.0f;
		m_pEventManager->EnqueueEvent( ev );

		if( !bSync )
			UnlockUpdate( );

		VA_INFO( "Core", "Created sound receiver '" << sName << "' and assigned ID " << iSourceID << " explicitly for renderer '" << sRendererID << "' only" );

		return iSourceID;
	}
	VA_FINALLY
	{
		if( bSync )
			UnlockUpdate( );
		throw;
	}
}

int CVACoreImpl::DeleteSoundSource( const int iSoundSourceID )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	bool bSync = GetUpdateLocked( );
	if( !bSync )
		LockUpdate( );

	VA_TRY
	{
		m_pNewSceneState->RemoveSoundSource( iSoundSourceID );

		// Ereignis generieren, wenn erfolgreich
		CVAEvent ev;
		ev.iEventType = CVAEvent::SOUND_SOURCE_DELETED;
		ev.pSender    = this;
		ev.iObjectID  = iSoundSourceID;
		m_pEventManager->EnqueueEvent( ev );

		if( !bSync )
			UnlockUpdate( );

		VA_INFO( "Core", "Deleted sound source " << iSoundSourceID );

		return 0;
	}
	VA_FINALLY
	{
		if( !bSync )
			UnlockUpdate( );
		throw;
	}
}

void CVACoreImpl::SetSoundSourceEnabled( const int iSoundSourceID, const bool bEnabled )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		CVASoundSourceDesc* pDesc = m_pSceneManager->GetSoundSourceDesc( iSoundSourceID );

		if( pDesc->bEnabled != bEnabled )
		{
			pDesc->bEnabled = bEnabled;

			CVAEvent ev;
			ev.iEventType = CVAEvent::SOUND_SOURCE_CHANGED_MUTING; // @todo JST new event type
			ev.pSender    = this;
			ev.iObjectID  = iSoundSourceID;
			// ev.bEnabled = bEnabled; // @todo JST
			m_pEventManager->EnqueueEvent( ev );

			// Trigger core thread
			m_pCoreThread->Trigger( );
		}
	}
	VA_RETHROW;
}

bool CVACoreImpl::GetSoundSourceEnabled( const int iSoundSourceID ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		const CVASoundSourceDesc* pDesc = m_pSceneManager->GetSoundSourceDesc( iSoundSourceID );
		return pDesc->bEnabled;
	}
	VA_RETHROW;
}

std::string CVACoreImpl::GetSoundSourceName( const int iSoundSourceID ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY { return m_pSceneManager->GetSoundSourceName( iSoundSourceID ); }
	VA_RETHROW;
}

void CVACoreImpl::SetSoundSourceName( const int iSoundSourceID, const std::string& sName )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		m_pSceneManager->SetSoundSourceName( iSoundSourceID, sName );

		// Ereignis generieren, falls erfolgreich
		CVAEvent ev;
		ev.iEventType = CVAEvent::SOUND_SOURCE_CHANGED_NAME;
		ev.pSender    = this;
		ev.iObjectID  = iSoundSourceID;
		ev.sName      = sName;
		m_pEventManager->BroadcastEvent( ev );
	}
	VA_RETHROW;
}

std::string CVACoreImpl::GetSoundSourceSignalSource( int iSoundSourceID ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		CVASoundSourceDesc* pDesc = m_pSceneManager->GetSoundSourceDesc( iSoundSourceID );
		if( pDesc->pSignalSource.load( ) != nullptr )
			return m_pSignalSourceManager->GetSignalSourceID( pDesc->pSignalSource );
		else
			return "";
	}
	VA_RETHROW;
}

int CVACoreImpl::GetSoundSourceGeometryMesh( const int iID ) const
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

void CVACoreImpl::SetSoundSourceGeometryMesh( const int iSoundSourceID, const int iGeometryMeshID )
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

void CVACoreImpl::SetSoundSourceSignalSource( int iSoundSourceID, const std::string& sSignalSourceID )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	// Keine Signalquelle zugewiesen. Wichtig: Stillepuffer einsetzen!
	IVAAudioSignalSource* pSignalSourceNew( nullptr ); // Neu oder zusätzlich ...
	const ITASampleFrame* psfSignalSourceInputBufNew( m_pSignalSourceManager->GetSilenceBuffer( ) );

	VA_TRY
	{
		CVASoundSourceDesc* pDesc = m_pSceneManager->GetSoundSourceDesc( iSoundSourceID );

		// Sicherstellen, das die Signalquellen ID gültig ist .. dann ID und Qu
		if( !sSignalSourceID.empty( ) )
			pSignalSourceNew = m_pSignalSourceManager->RequestSignalSource( sSignalSourceID, &psfSignalSourceInputBufNew );

		// Vorherige Bindung auflösen und Signalquelle freigeben
		if( pDesc->pSignalSource.load( ) != nullptr )
			m_pSignalSourceManager->ReleaseSignalSource( pDesc->pSignalSource );

		// Vorhandene Signalquelle zu dieser SoundSource zuordnen (atomare Ops)
		pDesc->pSignalSource           = pSignalSourceNew;
		pDesc->psfSignalSourceInputBuf = psfSignalSourceInputBufNew; // Removing const is intended here ...

		// Ereignis generieren
		CVAEvent ev;
		ev.iEventType = CVAEvent::SOUND_SOURCE_CHANGED_SIGNALSOURCE;
		ev.pSender    = this;
		ev.iObjectID  = iSoundSourceID;
		ev.sParam     = sSignalSourceID;
		m_pEventManager->EnqueueEvent( ev );

		if( pSignalSourceNew == nullptr )
		{
			VA_INFO( "Core", "Removed signal source from sound source " << iSoundSourceID << " (now uses silence buffer samples)" );
		}
		else
		{
			VA_INFO( "Core", "Set sound source " << iSoundSourceID << " signal source '" << sSignalSourceID << "'" );
		}

		return;
	}
	VA_CATCH( ex )
	{
		// Referenz auf die neue Signalquelle wieder entfernen
		if( pSignalSourceNew )
			m_pSignalSourceManager->ReleaseSignalSource( pSignalSourceNew );
		throw ex;
	}
}

int CVACoreImpl::GetSoundSourceAuralizationMode( int iSoundSourceID ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		CVASceneState* pHeadState               = m_pSceneManager->GetHeadSceneState( );
		const CVASoundSourceState* pSourceState = pHeadState->GetSoundSourceState( iSoundSourceID );

		if( !pSourceState )
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid sound source ID" );

		return pSourceState->GetAuralizationMode( );
	}
	VA_RETHROW;
}

void CVACoreImpl::SetSoundSourceAuralizationMode( int iSoundSourceID, int iAuralizationMode )
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

		CVASoundSourceState* pSourceState = m_pNewSceneState->AlterSoundSourceState( iSoundSourceID );

		if( !pSourceState )
		{
			// Quelle existiert nicht
			if( !bSync )
				UnlockUpdate( );
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid sound source ID" );
		}

		pSourceState->SetAuralizationMode( iAuralizationMode );

		// Ereignis generieren, falls erfolgreich
		CVAEvent ev;
		ev.iEventType        = CVAEvent::SOUND_SOURCE_CHANGED_AURALIZATIONMODE;
		ev.pSender           = this;
		ev.iObjectID         = iSoundSourceID;
		ev.iAuralizationMode = iAuralizationMode;
		m_pEventManager->EnqueueEvent( ev );

		if( !bSync )
			UnlockUpdate( );

		VA_INFO( "Core", "Changed sound source " << iSoundSourceID << " auralization mode: " << IVAInterface::GetAuralizationModeStr( iAuralizationMode, true ) );
	}
	VA_RETHROW;
}

void CVACoreImpl::SetSoundSourceParameters( int iID, const CVAStruct& oParams )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		bool bSync = GetUpdateLocked( );
		if( !bSync )
			LockUpdate( );

		CVASoundSourceState* pSoundSourceState = m_pNewSceneState->AlterSoundSourceState( iID );

		if( !pSoundSourceState )
		{
			if( !bSync )
				UnlockUpdate( );

			VA_EXCEPT2( INVALID_PARAMETER, "Invalid sound receiver ID" );
		}

		pSoundSourceState->SetParameters( oParams );

		CVAEvent ev;
		ev.iEventType = CVAEvent::SOUND_SOURCE_CHANGED_NAME; // @todo create own core event "parameter changed"
		ev.pSender    = this;
		ev.iObjectID  = iID;
		// ev.oStruct @todo: add a struct to the event
		m_pEventManager->EnqueueEvent( ev );

		if( !bSync )
			UnlockUpdate( );
	}
	VA_RETHROW;
}

CVAStruct CVACoreImpl::GetSoundSourceParameters( int iID, const CVAStruct& oArgs ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		CVASceneState* pHeadState                    = m_pSceneManager->GetHeadSceneState( );
		const CVASoundSourceState* pSoundSourceState = pHeadState->GetSoundSourceState( iID );

		if( !pSoundSourceState )
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid sound receiver ID" );

		return pSoundSourceState->GetParameters( oArgs );
	}
	VA_RETHROW;
}

int CVACoreImpl::GetSoundSourceDirectivity( int iSoundSourceID ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		CVASceneState* pHeadState               = m_pSceneManager->GetHeadSceneState( );
		const CVASoundSourceState* pSourceState = pHeadState->GetSoundSourceState( iSoundSourceID );

		if( !pSourceState )
			// Quelle existiert nicht
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid sound source ID" );

		return pSourceState->GetDirectivityID( );
	}
	VA_RETHROW;
}

void CVACoreImpl::SetSoundSourceDirectivity( const int iSoundSourceID, const int iDirectivityID )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		bool bSync = GetUpdateLocked( );
		if( !bSync )
			LockUpdate( );

		CVASoundSourceState* pSourceState = m_pNewSceneState->AlterSoundSourceState( iSoundSourceID );

		if( !pSourceState )
		{
			if( !bSync )
				UnlockUpdate( );
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid sound source ID" );
		}

		const IVADirectivity* pNewDirectivity = nullptr;
		if( iDirectivityID != -1 )
		{
			pNewDirectivity = m_pDirectivityManager->RequestDirectivity( iDirectivityID );
			if( !pNewDirectivity )
			{
				if( !bSync )
					UnlockUpdate( );
				VA_EXCEPT2( INVALID_PARAMETER, "Invalid directivity ID" );
				// TODO: Release des veränderten State?
			}
		}

		pSourceState->SetDirectivityID( iDirectivityID );
		pSourceState->SetDirectivityData( pNewDirectivity );

		// Ereignis generieren, falls erfolgreich
		CVAEvent ev;
		ev.iEventType = CVAEvent::SOUND_SOURCE_CHANGED_DIRECTIVITY;
		ev.pSender    = this;
		ev.iObjectID  = iSoundSourceID;
		ev.iParamID   = iDirectivityID;
		m_pEventManager->EnqueueEvent( ev );

		if( !bSync )
			UnlockUpdate( );
	}
	VA_RETHROW;
}

double CVACoreImpl::GetSoundSourceSoundPower( const int iSoundSourceID ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		CVASceneState* pHeadState               = m_pSceneManager->GetHeadSceneState( );
		const CVASoundSourceState* pSourceState = pHeadState->GetSoundSourceState( iSoundSourceID );

		if( !pSourceState )
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid sound source ID" );

		return pSourceState->GetSoundPower( );
	}
	VA_RETHROW;
}

void CVACoreImpl::SetSoundSourceSoundPower( const int iSoundSourceID, const double dSoundPower )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		// Parameter überprüfen
		if( !GetVolumeValid( dSoundPower ) )
		{
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid volume" );
		}

		bool bSync = GetUpdateLocked( );
		if( !bSync )
			LockUpdate( );

		CVASoundSourceState* pSourceState = m_pNewSceneState->AlterSoundSourceState( iSoundSourceID );

		if( !pSourceState )
		{
			// Quelle existiert nicht
			if( !bSync )
				UnlockUpdate( );
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid sound source ID" );
		}

		pSourceState->SetSoundPower( dSoundPower );

		// Ereignis generieren, falls erfolgreich
		CVAEvent ev;
		ev.iEventType = CVAEvent::SOUND_SOURCE_CHANGED_SOUND_POWER;
		ev.pSender    = this;
		ev.iObjectID  = iSoundSourceID;
		ev.dVolume    = dSoundPower;
		m_pEventManager->EnqueueEvent( ev );

		if( !bSync )
			UnlockUpdate( );
	}
	VA_RETHROW;
}

bool CVACoreImpl::GetSoundSourceMuted( const int iSoundSourceID ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		const CVASoundSourceDesc* pDesc = m_pSceneManager->GetSoundSourceDesc( iSoundSourceID );
		return pDesc->bMuted;
	}
	VA_RETHROW;
}

void CVACoreImpl::SetSoundSourceMuted( const int iSoundSourceID, const bool bMuted )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		CVASoundSourceDesc* pDesc = m_pSceneManager->GetSoundSourceDesc( iSoundSourceID );

		if( pDesc->bMuted != bMuted )
		{
			pDesc->bMuted = bMuted;

			CVAEvent ev;
			ev.iEventType = CVAEvent::SOUND_SOURCE_CHANGED_MUTING;
			ev.pSender    = this;
			ev.iObjectID  = iSoundSourceID;
			ev.bMuted     = bMuted;
			m_pEventManager->EnqueueEvent( ev );

			// Trigger core thread
			m_pCoreThread->Trigger( );
		}
	}
	VA_RETHROW;
}


void CVACoreImpl::GetSoundSourcePose( const int iSoundSourceID, VAVec3& v3Pos, VAQuat& qOrient ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		const CVASceneState* pHeadState         = m_pSceneManager->GetHeadSceneState( );
		const CVASoundSourceState* pSourceState = pHeadState->GetSoundSourceState( iSoundSourceID );

		if( !pSourceState )
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid sound source ID" );

		const CVAMotionState* pMotionState = pSourceState->GetMotionState( );
		if( !pMotionState )
			VA_EXCEPT2( INVALID_PARAMETER, "Sound source has invalid motion state, probably it has not been positioned, yet?" );

		v3Pos   = pMotionState->GetPosition( );
		qOrient = pMotionState->GetOrientation( );
	}
	VA_RETHROW;
}

void CVACoreImpl::SetSoundSourcePose( const int iID, const VAVec3& v3Pos, const VAQuat& qOrient )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		bool bSync = GetUpdateLocked( );
		if( !bSync )
			LockUpdate( );

		CVASoundSourceState* pSourceState = m_pNewSceneState->AlterSoundSourceState( iID );

		if( !pSourceState )
		{
			// Quelle existiert nicht
			if( !bSync )
				UnlockUpdate( );
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid sound source ID" );
		}

		CVAMotionState* pNewMotionState = pSourceState->AlterMotionState( );
		pNewMotionState->SetPosition( v3Pos );
		pNewMotionState->SetOrientation( qOrient );

		m_pSceneManager->GetSoundSourceDesc( iID )->bInitPositionOrientation = true;

		// Ereignis generieren, falls erfolgreich
		CVAEvent ev;
		ev.iEventType = CVAEvent::SOUND_SOURCE_CHANGED_POSE;
		ev.pSender    = this;
		ev.iObjectID  = iID;
		ev.vPos       = v3Pos;
		SetCoreEventParams( ev, pNewMotionState );

		m_pEventManager->EnqueueEvent( ev );

		if( !bSync )
			UnlockUpdate( );
	}
	VA_RETHROW;
}

VAVec3 CVACoreImpl::GetSoundSourcePosition( const int iID ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		const CVASceneState* pHeadState         = m_pSceneManager->GetHeadSceneState( );
		const CVASoundSourceState* pSourceState = pHeadState->GetSoundSourceState( iID );

		if( !pSourceState )
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid sound source ID" );

		const CVAMotionState* pMotionState = pSourceState->GetMotionState( );
		if( !pMotionState )
			VA_EXCEPT2( INVALID_PARAMETER, "Sound source has invalid motion state, probably it has not been positioned, yet?" );

		return pMotionState->GetPosition( );
	}
	VA_RETHROW;
}

void CVACoreImpl::SetSoundSourcePosition( const int iID, const VAVec3& v3Pos )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		bool bSync = GetUpdateLocked( );
		if( !bSync )
			LockUpdate( );

		CVASoundSourceState* pSourceState = m_pNewSceneState->AlterSoundSourceState( iID );

		if( !pSourceState )
		{
			// Quelle existiert nicht
			if( !bSync )
				UnlockUpdate( );
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid sound source ID" );
		}

		CVAMotionState* pNewMotionState = pSourceState->AlterMotionState( );
		pNewMotionState->SetPosition( v3Pos );

		m_pSceneManager->GetSoundSourceDesc( iID )->bInitPositionOrientation = true;

		// Ereignis generieren, falls erfolgreich
		CVAEvent ev;
		ev.iEventType = CVAEvent::SOUND_SOURCE_CHANGED_POSE;
		ev.pSender    = this;
		ev.iObjectID  = iID;
		ev.vPos       = v3Pos;
		SetCoreEventParams( ev, pNewMotionState );

		m_pEventManager->EnqueueEvent( ev );

		if( !bSync )
			UnlockUpdate( );
	}
	VA_RETHROW;
}

VAQuat CVACoreImpl::GetSoundSourceOrientation( const int iID ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		const CVASceneState* pHeadState         = m_pSceneManager->GetHeadSceneState( );
		const CVASoundSourceState* pSourceState = pHeadState->GetSoundSourceState( iID );

		if( !pSourceState )
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid sound source ID" );

		const CVAMotionState* pMotionState = pSourceState->GetMotionState( );
		if( !pMotionState )
			VA_EXCEPT2( INVALID_PARAMETER, "Sound source has invalid motion state, probably it has not been positioned, yet?" );

		return pMotionState->GetOrientation( );
	}
	VA_RETHROW;
}

void CVACoreImpl::SetSoundSourceOrientation( const int iID, const VAQuat& qOrient )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		bool bSync = GetUpdateLocked( );
		if( !bSync )
			LockUpdate( );

		CVASoundSourceState* pSourceState = m_pNewSceneState->AlterSoundSourceState( iID );

		if( !pSourceState )
		{
			// Quelle existiert nicht
			if( !bSync )
				UnlockUpdate( );
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid sound source ID" );
		}

		CVAMotionState* pNewMotionState = pSourceState->AlterMotionState( );
		pNewMotionState->SetOrientation( qOrient );

		m_pSceneManager->GetSoundSourceDesc( iID )->bInitPositionOrientation = true;

		// Ereignis generieren
		CVAEvent ev;
		ev.iEventType = CVAEvent::SOUND_SOURCE_CHANGED_POSE;
		ev.pSender    = this;
		ev.iObjectID  = iID;
		// ev.qOrient = qOrient; // @todo
		ConvertQuaternionToViewUp( qOrient, ev.vView, ev.vUp );
		SetCoreEventParams( ev, pNewMotionState );

		m_pEventManager->EnqueueEvent( ev );

		if( !bSync )
			UnlockUpdate( );
	}
	VA_RETHROW;
}

void CVACoreImpl::GetSoundSourceOrientationVU( const int iID, VAVec3& v3View, VAVec3& v3Up ) const
{
	ConvertQuaternionToViewUp( GetSoundSourceOrientation( iID ), v3View, v3Up );
}

void CVACoreImpl::SetSoundSourceOrientationVU( const int iSoundSourceID, const VAVec3& v3View, const VAVec3& v3Up )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		bool bSync = GetUpdateLocked( );
		if( !bSync )
			LockUpdate( );

		CVASoundSourceState* pSourceState = m_pNewSceneState->AlterSoundSourceState( iSoundSourceID );

		if( !pSourceState )
		{
			// Quelle existiert nicht
			if( !bSync )
				UnlockUpdate( );
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid sound source ID" );
		}

		CVAMotionState* pNewMotionState = pSourceState->AlterMotionState( );
		pNewMotionState->SetOrientationVU( v3View, v3Up );

		m_pSceneManager->GetSoundSourceDesc( iSoundSourceID )->bInitPositionOrientation = true;

		// Ereignis generieren
		CVAEvent ev;
		ev.iEventType = CVAEvent::SOUND_SOURCE_CHANGED_POSE;
		ev.pSender    = this;
		ev.iObjectID  = iSoundSourceID;
		ev.vView      = v3View;
		ev.vUp        = v3Up;
		SetCoreEventParams( ev, pNewMotionState );

		m_pEventManager->EnqueueEvent( ev );

		if( !bSync )
			UnlockUpdate( );
	}
	VA_RETHROW;
}
