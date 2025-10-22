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

#include "VASceneManager.h"

#include "../Utils/VADebug.h"
#include "VAContainerState.h"
#include "VAMotionState.h"
#include "VASceneState.h"
#include "VASoundPortalDesc.h"
#include "VASoundPortalState.h"
#include "VASoundReceiverDesc.h"
#include "VASoundReceiverState.h"
#include "VASoundSourceDesc.h"
#include "VASoundSourceState.h"
#include "VASurfaceState.h"

#include <ITAClock.h>
#include <ITAFunctors.h>
#include <VAException.h>
#include <VAObjectPool.h>
#include <algorithm>

CVASceneManager::CVASceneManager( ITAClock* pClock )
    : m_pClock( pClock )
    , m_pPoolSceneStates( nullptr )
    , m_pPoolSoundSourceStates( nullptr )
    , m_pPoolSoundReceiverStates( nullptr )
    , m_pPoolPortalStates( nullptr )
    , m_pPoolSurfaceStates( nullptr )
    , m_pPoolMotionStates( nullptr )
    , m_pPoolContainerStates( nullptr )
{
}

CVASceneManager::~CVASceneManager( ) {}

void CVASceneManager::Initialize( )
{
	ITACriticalSectionLock oLock( m_csStates );

	m_pPoolSceneStates = IVAObjectPool::Create( 64, 16, new CVAPoolObjectDefaultFactory<CVASceneState>, true );
	m_pPoolSceneStates->SetName( "SceneStates" );
	m_pPoolSoundSourceStates = IVAObjectPool::Create( 64, 16, new CVAPoolObjectDefaultFactory<CVASoundSourceState>, true );
	m_pPoolSoundSourceStates->SetName( "SourceStates" );
	m_pPoolSoundReceiverStates = IVAObjectPool::Create( 64, 16, new CVAPoolObjectDefaultFactory<CVAReceiverState>, true );
	m_pPoolSoundReceiverStates->SetName( "ListenerStates" );
	m_pPoolPortalStates = IVAObjectPool::Create( 64, 16, new CVAPoolObjectDefaultFactory<CVAPortalState>, true );
	m_pPoolPortalStates->SetName( "PortalStates" );
	m_pPoolSurfaceStates = IVAObjectPool::Create( 64, 16, new CVAPoolObjectDefaultFactory<CVASurfaceState>, true );
	m_pPoolSurfaceStates->SetName( "SurfaceStates" );
	m_pPoolMotionStates = IVAObjectPool::Create( 1024, 256, new CVAPoolObjectDefaultFactory<CVAMotionState>, true );
	m_pPoolMotionStates->SetName( "MotionStates" );
	m_pPoolContainerStates = IVAObjectPool::Create( 64, 16, new CVAPoolObjectDefaultFactory<CVAContainerState>, true );
	m_pPoolContainerStates->SetName( "ContainerStates" );

	m_pPoolSourceDesc = IVAObjectPool::Create( 64, 16, new CVAPoolObjectDefaultFactory<CVASoundSourceDesc>, true );
	m_pPoolSourceDesc->SetName( "SourceDesc" );

	m_pPoolListenerDesc = IVAObjectPool::Create( 64, 16, new CVAPoolObjectDefaultFactory<CVASoundReceiverDesc>, true );
	m_pPoolListenerDesc->SetName( "ListenerDesc" );

	m_pPoolPortalDesc = IVAObjectPool::Create( 64, 16, new CVAPoolObjectDefaultFactory<CVAPortalDesc>, true );
	m_pPoolPortalDesc->SetName( "PortalDesc" );

	Reset( );
}

void CVASceneManager::Reset( )
{
	ITACriticalSectionLock oLock( m_csStates );

	// Pools zurücksetzen
	// TODO: Maximale Anzahl Elemente angeben um Speicher zu begrenzen?
	m_pPoolSceneStates->Reset( );
	m_pPoolSoundSourceStates->Reset( );
	m_pPoolSoundReceiverStates->Reset( );
	m_pPoolPortalStates->Reset( );
	m_pPoolSurfaceStates->Reset( );
	m_pPoolMotionStates->Reset( );
	m_pPoolContainerStates->Reset( );

	m_pPoolSourceDesc->Reset( );
	m_pPoolListenerDesc->Reset( );
	m_pPoolPortalDesc->Reset( );

	// Dynamische Szenezustände freigeben
	m_mSceneStates.clear( );

	m_csDesc.enter( );
	m_mSourceDesc.clear( );
	m_mListenerDesc.clear( );
	m_mPortalDesc.clear( );
	m_csDesc.leave( );

	// Wurzel-Szenezustand ohne Objekte erzeugen (Konfiguration 0)
	CVASceneState* pInitState = dynamic_cast<CVASceneState*>( m_pPoolSceneStates->RequestObject( ) );
	pInitState->SetManager( this );
	pInitState->Initialize( 0, m_pClock->getTime( ) );
	pInitState->Fix( );
	m_mSceneStates.insert( SSMapItem( 0, pInitState ) );

	// Zähler initialisieren
	m_iIDCounterScene          = 1;
	m_iIDCounterSources        = 1;
	m_iIDCounterSoundReceivers = 1;
	m_iIDCounterPortals        = 1;
	m_iIDCounterSurfaces       = 1;
	m_iHeadState               = 0;
}

void CVASceneManager::Finalize( )
{
	ITACriticalSectionLock oLock( m_csStates );

	delete m_pPoolSoundSourceStates;
	m_pPoolSoundSourceStates = nullptr;

	delete m_pPoolSoundReceiverStates;
	m_pPoolSoundReceiverStates = nullptr;

	delete m_pPoolPortalStates;
	m_pPoolPortalStates = nullptr;

	delete m_pPoolSurfaceStates;
	m_pPoolSurfaceStates = nullptr;

	delete m_pPoolSceneStates;
	m_pPoolSceneStates = nullptr;

	delete m_pPoolMotionStates;
	m_pPoolMotionStates = nullptr;

	delete m_pPoolContainerStates;
	m_pPoolContainerStates = nullptr;

	delete m_pPoolSourceDesc;
	m_pPoolSourceDesc = nullptr;

	delete m_pPoolListenerDesc;
	m_pPoolListenerDesc = nullptr;

	delete m_pPoolPortalDesc;
	m_pPoolPortalDesc = nullptr;
}

int CVASceneManager::GetRootSceneStateID( ) const
{
	// Root => immer Zustand 0
	return 0;
}

int CVASceneManager::GetHeadSceneStateID( ) const
{
	return m_iHeadState;
}

void CVASceneManager::SetHeadSceneState( const int iSceneStateID )
{
	m_iHeadState = iSceneStateID;
}

CVASceneState* CVASceneManager::GetHeadSceneState( ) const
{
	int iHeadStateID          = GetHeadSceneStateID( );
	CVASceneState* pHeadState = GetSceneState( iHeadStateID );
	assert( pHeadState );
	return pHeadState;
}

CVASceneState* CVASceneManager::GetSceneState( const int iSceneStateID ) const
{
	ITACriticalSectionLock oLock( m_csStates );

	SSMap::const_iterator cit = m_mSceneStates.find( iSceneStateID );
	assert( cit != m_mSceneStates.end( ) );
	return ( cit == m_mSceneStates.end( ) ? nullptr : cit->second );
}

CVASceneState* CVASceneManager::CreateDerivedSceneState( const int iBaseStateID, const double dModificationTime )
{
	ITACriticalSectionLock oLock( m_csStates );

	SSMap::const_iterator cit = m_mSceneStates.find( iBaseStateID );
	assert( cit != m_mSceneStates.end( ) );
	const CVASceneState* pBaseState = cit->second;

	// Base state must be fixated.
	assert( pBaseState->IsFixed( ) );

	// Derive
	const int iNewStateID    = m_iIDCounterScene++;
	CVASceneState* pNewState = dynamic_cast<CVASceneState*>( m_pPoolSceneStates->RequestObject( ) );
	pNewState->SetManager( this );
	pNewState->Copy( iNewStateID, dModificationTime, pBaseState );

	// Speichere diese (temporär)
	m_mSceneStates.insert( SSMapItem( iNewStateID, pNewState ) );

	return pNewState;
}

CVASceneState* CVASceneManager::CreateDerivedSceneState( const CVASceneState* pBaseState, const double dModificationTime )
{
	return CreateDerivedSceneState( pBaseState->GetID( ), dModificationTime );
}

CVASoundSourceState* CVASceneManager::RequestSoundSourceState( )
{
	CVASoundSourceState* pState = dynamic_cast<CVASoundSourceState*>( m_pPoolSoundSourceStates->RequestObject( ) );
	pState->SetManager( this );
	assert( pState->GetNumReferences( ) == 1 );
	return pState;
}

CVAReceiverState* CVASceneManager::RequestSoundReceiverState( )
{
	CVAReceiverState* pState = dynamic_cast<CVAReceiverState*>( m_pPoolSoundReceiverStates->RequestObject( ) );
	pState->SetManager( this );
	return pState;
}

CVAPortalState* CVASceneManager::RequestPortalState( )
{
	CVAPortalState* pState = dynamic_cast<CVAPortalState*>( m_pPoolPortalStates->RequestObject( ) );
	pState->SetManager( this );
	return pState;
}

CVASurfaceState* CVASceneManager::RequestSurfaceState( )
{
	CVASurfaceState* pState = dynamic_cast<CVASurfaceState*>( m_pPoolSurfaceStates->RequestObject( ) );
	pState->SetManager( this );
	return pState;
}

CVAMotionState* CVASceneManager::RequestMotionState( )
{
	CVAMotionState* pState = dynamic_cast<CVAMotionState*>( m_pPoolMotionStates->RequestObject( ) );
	pState->SetManager( this );
	return pState;
}

CVAContainerState* CVASceneManager::RequestContainerState( )
{
	CVAContainerState* pState = dynamic_cast<CVAContainerState*>( m_pPoolContainerStates->RequestObject( ) );
	pState->SetManager( this );
	return pState;
}

int CVASceneManager::GenerateSoundSourceID( )
{
	return m_iIDCounterSources++;
}

int CVASceneManager::GenerateSoundReceiverID( )
{
	return m_iIDCounterSoundReceivers++;
}

int CVASceneManager::GeneratePortalID( )
{
	return m_iIDCounterPortals++;
}

int CVASceneManager::GenerateSurfaceID( )
{
	return m_iIDCounterSurfaces++;
}

std::string CVASceneManager::GetSoundSourceName( int iSoundSourceID ) const
{
	return GetSoundSourceDesc( iSoundSourceID )->sName;
}

void CVASceneManager::SetSoundSourceName( int iSoundSourceID, const std::string& sName )
{
	GetSoundSourceDesc( iSoundSourceID )->sName = sName;
}

std::string CVASceneManager::GetSoundReceiverName( int iListenerID ) const
{
	return GetSoundReceiverDesc( iListenerID )->sName;
}

void CVASceneManager::SetSoundReceiverName( int iListenerID, const std::string& sName )
{
	GetSoundReceiverDesc( iListenerID )->sName = sName;
}

std::string CVASceneManager::GetPortalName( int iPortalID ) const
{
	return GetPortalDesc( iPortalID )->sName;
}

void CVASceneManager::SetPortalName( int iPortalID, const std::string& sName )
{
	GetPortalDesc( iPortalID )->sName = sName;
}

CVASoundSourceDesc* CVASceneManager::CreateSoundSourceDesc( int iSoundSourceID )
{
	m_csDesc.enter( );

	// Sicherstellen das ein solcher Descriptor noch nicht existiert
	if( m_mSourceDesc.find( iSoundSourceID ) != m_mSourceDesc.end( ) )
	{
		m_csDesc.leave( );
		assert( false );
	}

	CVASoundSourceDesc* pDesc = dynamic_cast<CVASoundSourceDesc*>( m_pPoolSourceDesc->RequestObject( ) );
	pDesc->iID                = iSoundSourceID;

	m_mSourceDesc.insert( std::pair<int, CVASoundSourceDesc*>( iSoundSourceID, pDesc ) );

	m_csDesc.leave( );

	return pDesc;
}

void CVASceneManager::DeleteSoundSourceDesc( int iSoundSourceID )
{
	m_csDesc.enter( );

	SourceDescMap::iterator it = m_mSourceDesc.find( iSoundSourceID );

	// Sicherstellen das ein solcher Descriptor tatsächlich existiert
	if( m_mSourceDesc.find( iSoundSourceID ) == m_mSourceDesc.end( ) )
	{
		m_csDesc.leave( );
		assert( false );
	}

	it->second->RemoveReference( );
	m_mSourceDesc.erase( it );

	m_csDesc.leave( );
}

CVASoundSourceDesc* CVASceneManager::GetSoundSourceDesc( int iSoundSourceID ) const
{
	m_csDesc.enter( );

	SourceDescMap::const_iterator it = m_mSourceDesc.find( iSoundSourceID );

	// Exception falls ein solcher Descriptor nicht existiert
	if( m_mSourceDesc.find( iSoundSourceID ) == m_mSourceDesc.end( ) )
	{
		m_csDesc.leave( );
		VA_EXCEPT2( INVALID_PARAMETER, "Invalid sound source ID" );
	}

	CVASoundSourceDesc* pDesc = it->second;

	m_csDesc.leave( );

	return pDesc;
}

CVASoundReceiverDesc* CVASceneManager::CreateSoundReceiverDesc( int iListenerID )
{
	m_csDesc.enter( );

	// Sicherstellen das ein solcher Descriptor noch nicht existiert
	if( m_mListenerDesc.find( iListenerID ) != m_mListenerDesc.end( ) )
	{
		m_csDesc.leave( );
		assert( false );
	}

	CVASoundReceiverDesc* pDesc = dynamic_cast<CVASoundReceiverDesc*>( m_pPoolListenerDesc->RequestObject( ) );
	pDesc->iID                  = iListenerID;

	m_mListenerDesc.insert( std::pair<int, CVASoundReceiverDesc*>( iListenerID, pDesc ) );

	m_csDesc.leave( );

	return pDesc;
}

void CVASceneManager::DeleteSoundReceiverDesc( int iListenerID )
{
	m_csDesc.enter( );

	SoundReceiverDescMap::iterator it = m_mListenerDesc.find( iListenerID );

	// Sicherstellen das ein solcher Descriptor tatsächlich existiert
	if( m_mListenerDesc.find( iListenerID ) == m_mListenerDesc.end( ) )
	{
		m_csDesc.leave( );
		assert( false );
	}

	it->second->RemoveReference( );
	m_mListenerDesc.erase( it );

	m_csDesc.leave( );
}

CVASoundReceiverDesc* CVASceneManager::GetSoundReceiverDesc( int iListenerID ) const
{
	m_csDesc.enter( );

	SoundReceiverDescMap::const_iterator it = m_mListenerDesc.find( iListenerID );

	// Assertion falls ein solcher Descriptor nicht existiert
	// Damit ist sichergestellt, das niemals Null zurückgegeben wird
	if( m_mListenerDesc.find( iListenerID ) == m_mListenerDesc.end( ) )
	{
		m_csDesc.leave( );
		VA_EXCEPT2( INVALID_PARAMETER, "Invalid listener ID" );
	}

	CVASoundReceiverDesc* pDesc = it->second;

	m_csDesc.leave( );

	return pDesc;
}

CVAPortalDesc* CVASceneManager::CreatePortalDesc( int iPortalID )
{
	m_csDesc.enter( );

	// Sicherstellen das ein solcher Descriptor noch nicht existiert
	if( m_mPortalDesc.find( iPortalID ) != m_mPortalDesc.end( ) )
	{
		m_csDesc.leave( );
		assert( false );
	}

	CVAPortalDesc* pDesc = dynamic_cast<CVAPortalDesc*>( m_pPoolPortalDesc->RequestObject( ) );
	pDesc->iID           = iPortalID;

	m_mPortalDesc.insert( std::pair<int, CVAPortalDesc*>( iPortalID, pDesc ) );

	m_csDesc.leave( );

	return pDesc;
}

void CVASceneManager::DeletePortalDesc( int iPortalID )
{
	m_csDesc.enter( );

	PortalDescMap::iterator it = m_mPortalDesc.find( iPortalID );

	// Sicherstellen das ein solcher Descriptor tatsächlich existiert
	if( m_mPortalDesc.find( iPortalID ) == m_mPortalDesc.end( ) )
	{
		m_csDesc.leave( );
		assert( false );
	}

	it->second->RemoveReference( );
	m_mPortalDesc.erase( it );

	m_csDesc.leave( );
}

CVAPortalDesc* CVASceneManager::GetPortalDesc( int iPortalID ) const
{
	m_csDesc.enter( );

	PortalDescMap::const_iterator it = m_mPortalDesc.find( iPortalID );

	// Assertion falls ein solcher Descriptor nicht existiert
	// Damit ist sichergestellt, das niemals Null zurückgegeben wird
	if( m_mPortalDesc.find( iPortalID ) == m_mPortalDesc.end( ) )
	{
		m_csDesc.leave( );
		VA_EXCEPT2( INVALID_PARAMETER, "Invalid portal ID" );
	}

	CVAPortalDesc* pDesc = it->second;

	m_csDesc.leave( );

	return pDesc;
}
