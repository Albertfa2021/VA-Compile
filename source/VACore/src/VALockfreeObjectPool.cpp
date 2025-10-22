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

#include "VALockfreeObjectPool.h"

#include "Utils/VADebug.h"

#include <VAPoolObject.h>

// Debugging-Messages 1=warning 2=normal 3=verbose
#define LFPOOL_DEBUG_MESSAGES 0

// Wiederverwendung von Pool-Objekten unterbinden (nur new/delete)
#define LFPOOL_NO_REUSE 0

CVALockfreeObjectPool::CVALockfreeObjectPool( int iInitialSize, int iDelta, IVAPoolObjectFactory* pFactory, bool bDeleteFactory )
    : m_iDelta( iDelta )
    , m_pFactory( pFactory )
    , m_bDeleteFactory( bDeleteFactory )
{
	m_nFree = m_nUsed = m_nTotal = 0;

	// Objekte vorerzeugen
	Grow( iInitialSize );
}

CVALockfreeObjectPool::~CVALockfreeObjectPool( )
{
#if( LFPOOL_DEBUG_MESSAGES >= 2 )
	VA_DEBUG_PRINTF( "[CVALockfreeObjectPool \"%s\" 0x%08Xh] Indicating destruction (%d free, %d used, %d total)\n", m_sName.c_str( ), this, GetNumFree( ), GetNumUsed( ),
	                 GetSize( ) );
#endif

	// Alle Objekte freigeben
	CVAPoolObject* pObject;

	while( m_qFree.try_pop( pObject ) )
		delete pObject;
	assert( m_qFree.empty( ) );

	while( m_qUsed.try_pop( pObject ) )
	{
		delete pObject;
	}
	assert( m_qUsed.empty( ) );

	if( m_bDeleteFactory )
		delete m_pFactory;
};

void CVALockfreeObjectPool::SetName( const std::string& sName )
{
	m_sName = sName;
}

void CVALockfreeObjectPool::Reset( )
{
	CVAPoolObject* pObject;
	while( m_qUsed.try_pop( pObject ) )
	{
		--m_nUsed;
		assert( pObject );
		pObject->ResetReferences( );
		pObject->PreRelease( );
		m_qFree.push( pObject );
		++m_nFree;
	}

	assert( m_qUsed.empty( ) );
}

int CVALockfreeObjectPool::GetNumFree( ) const
{
	return m_nFree;
}

int CVALockfreeObjectPool::GetNumUsed( ) const
{
	return m_nUsed;
}

int CVALockfreeObjectPool::GetSize( ) const
{
	return m_nTotal;
}

int CVALockfreeObjectPool::Grow( int iDelta )
{
	for( int i = 0; i < iDelta; i++ )
	{
		CVAPoolObject* pObject = m_pFactory->CreatePoolObject( );
		assert( pObject != nullptr );

		pObject->m_pParentPool = this;

		m_qFree.push( pObject );
		++m_nFree;
		++m_nTotal;
	}

#if( LFPOOL_DEBUG_MESSAGES >= 1 )
	VA_DEBUG_PRINTF( "[CVALockfreeObjectPool \"%s\" 0x%08Xh] Growing by %i Objects (%d free, %d used, %d total)\n", m_sName.c_str( ), this, iDelta, GetNumFree( ),
	                 GetNumUsed( ), GetSize( ) );
#endif

	return iDelta;
}

CVAPoolObject* CVALockfreeObjectPool::RequestObject( )
{
#if( LFPOOL_NO_REUSE == 1 )

	// Direkte Implementierung mit new-operator (keine Wiederverwendung). Nur zum Debuggen.
	CVAPoolObject* pObject = m_pFactory->CreatePoolObject( );
	pObject->pParentPool   = this;
	pObject->AddReference( );
	pObject->PreRequest( );
	++m_nUsed;
	++m_nTotal;

#else

	CVAPoolObject* pObject( nullptr );

	// Standard-Implementierung (mit Wiederverwendung)
	while( !m_qFree.try_pop( pObject ) )
		Grow( m_iDelta );

	assert( pObject );
	assert( !pObject->HasReferences( ) );
	--m_nFree;
	++m_nUsed;
	pObject->AddReference( );
	m_qUsed.push( pObject );

	pObject->PreRequest( );

#endif // LFPOOL_NO_REUSE

#if( LFPOOL_DEBUG_MESSAGES == 3 )
	VA_DEBUG_PRINTF( "[CVALockfreeObjectPool \"%s\" 0x%08Xh] Requested object 0x%08Xh (%d free, %d used, %d total)\n", m_sName.c_str( ), this, pObject, GetNumFree( ),
	                 GetNumUsed( ), GetSize( ) );
#endif

	return pObject;
}

void CVALockfreeObjectPool::ReleaseObject( CVAPoolObject* pObject )
{
	// Wichtig: ReleaseObjekt wird von den PoolObjekten aufrufen, wenn diese KEINE Referenzen mehr haben!
	assert( !pObject->HasReferences( ) );

	pObject->PreRelease( );

#if( LFPOOL_NO_REUSE == 1 )

	// Direkte Implementierung mit delete-operator (keine Wiederverwendung). Nur zum Debuggen.
	delete pObject;
	--m_nUsed;
	--m_nTotal;

#else

	// Standard-Implementierung (mit Wiederverwendung)

	// Keine Referenzen mehr => Objekt wieder freigeben

	// Sicherheitscheck: Wirklich ein Objekt dieses Pools
	assert( pObject->m_pParentPool == this );

	// Objekt aus der Used-Queue entfernen
	// TODO: Evtl. gegen unsicherere O(1) Strategie austauschen (diese ist in O(N))
#	pragma warning( disable : 4127 ) // We allow for a constant expression in the do-while loop
	CVAPoolObject* pUsedObject( nullptr );
	do
	{
		m_qUsed.try_pop( pUsedObject );

		assert( pUsedObject != nullptr );

		if( pUsedObject == pObject )
			break; // Gefunden und Löschen
		else
			m_qUsed.push( pUsedObject );

	} while( true );
#	pragma warning( default : 4127 )

	--m_nUsed;
	m_qFree.push( pObject );
	++m_nFree;

#endif // LFPOOL_NO_REUSE

#if( LFPOOL_DEBUG_MESSAGES == 3 )
	VA_DEBUG_PRINTF( "[CVALockfreeObjectPool \"%s\" 0x%08Xh] Released object 0x%08Xh (%d free, %d used, %d total)\n", m_sName.c_str( ), this, pObject, GetNumFree( ),
	                 GetNumUsed( ), GetSize( ) );
#endif
}
