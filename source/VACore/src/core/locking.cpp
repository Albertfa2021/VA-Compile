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

bool CVACoreImpl::GetUpdateLocked( ) const
{
	VA_CHECK_INITIALIZED;
	return ( m_lSyncModOwner != -1 );
}

void CVACoreImpl::LockUpdate( )
{
	m_dSyncEntryTime = m_pClock->getTime( );

	VA_CHECK_INITIALIZED;
	VA_LOCK_REENTRANCE;

	if( m_lSyncModOwner != -1 )
	{
		// Thread already owner, increment spin counter
		if( getCurrentThreadID( ) == m_lSyncModOwner )
		{
			++m_lSyncModSpinCount;
			VA_UNLOCK_REENTRANCE;
			VA_VERBOSE( "Core", "Attempt to enter synchronized scene modification ignored - already in a synchronized scene modification" );
			return;
		}
	}

	VA_UNLOCK_REENTRANCE;

	VA_TRACE( "Core", "Beginning synchronized scene modification" );

	/*
	 *  Blocking wait des aufrufenden Thread.
	 *  Wichtig: Dies muss ausserhalb des Reentrance-Lock geschehen,
	 *           da sonst kein anderer Thread mehr auf der Schnittstelle aufrufen kann.
	 */
	m_mxSyncModLock.Lock( );

	VA_LOCK_REENTRANCE;

	m_lSyncModOwner = getCurrentThreadID( );
	++m_lSyncModSpinCount;

	VA_TRY
	{
		m_pNewSceneState = m_pSceneManager->CreateDerivedSceneState( m_pSceneManager->GetHeadSceneStateID( ), m_dSyncEntryTime );
		VA_UNLOCK_REENTRANCE;
	}
	VA_FINALLY
	{
		m_mxSyncModLock.Unlock( );
		m_lSyncModOwner = -1;
		VA_UNLOCK_REENTRANCE;
		throw;
	}
}

int CVACoreImpl::UnlockUpdate( )
{
	VA_CHECK_INITIALIZED;
	VA_LOCK_REENTRANCE;

	int iNewSceneID = -1;

	VA_TRACE( "Core", "Unlocking scene" );

	// Sicherheitscheck: Gar nicht innerhalb Synchronisation => Fehler
	if( m_lSyncModOwner == -1 )
	{
		VA_UNLOCK_REENTRANCE;
		VA_EXCEPT2( MODAL_ERROR, "Not within a synchronized modification phase" );
	}

	if( m_lSyncModOwner != getCurrentThreadID( ) )
	{
		VA_UNLOCK_REENTRANCE;
		VA_EXCEPT2( MODAL_ERROR, "Synchronized modification may only be ended by the same thread that begun it" );
	}

	if( --m_lSyncModSpinCount > 0 )
	{
		// Nicht die letzte Freigabe des Token.
		VA_UNLOCK_REENTRANCE;
		return -1;
	}

	VA_TRY
	{
		m_pNewSceneState->Fix( );

		iNewSceneID = m_pNewSceneState->GetID( );
		m_pSceneManager->SetHeadSceneState( iNewSceneID );

		m_lSyncModOwner = -1;
		m_mxSyncModLock.Unlock( );

		VA_UNLOCK_REENTRANCE;
	}
	VA_FINALLY
	{
		VA_UNLOCK_REENTRANCE;
		throw;
	}
	VA_TRY
	{
		m_pCoreThread->Trigger( );
		m_pEventManager->BroadcastEvents( );
		return iNewSceneID;
	}
	VA_RETHROW;
}
