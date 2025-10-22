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

#include "VALog.h"

#include <ITACriticalSection.h>
#include <VAObjectPool.h>
#include <VAPoolObject.h>
#include <cassert>
#include <set>

// Debugging-Messages
#define POOL_DEBUG_MESSAGES 0

// Wiederverwendung von Pool-Objekten unterbinden (nur new/delete)
#define POOL_NO_REUSE 0

// ObjectPool class
class CVAObjectPool : public IVAObjectPool
{
public:
	// Konstruktor
	/**
	 * Erzeugt einen Pool mit der angegebenen Anzahl von Objekten.
	 * Der zweite Parameter legt die Vergrößerung im Falle eines Überlaufes fest.
	 *
	 * \param iInitialSize		Initiale Anzahl Objekte
	 * \param iDelta            Vergrößerung wenn keine freien Objekte mehr
	 * \param pFactory			Factory zur Erzeugung der Pool-Objekte
	 * \param bDeleteFactory	Übergebene Factory im Destruktor freigeben?
	 */
	CVAObjectPool( const int iInitialSize, const int iDelta, IVAPoolObjectFactory* pFactory, const bool bDeleteFactory );

	// Destruktor
	virtual ~CVAObjectPool( );

	// Namen setzen (fürs Debugging)
	void SetName( const std::string& sName );

	// Zurücksetzen. Setzt alle Objekte auf unbenutzt. Ändert die Pool-Größe nicht.
	void Reset( );

	// Anzahl der freien Objekte zurückgeben
	int GetNumFree( ) const;

	// Anzahl der benutzten Objekte zurückgeben
	int GetNumUsed( ) const;

	// Anzahl der Objekte (insgesamt) zurückgeben
	int GetSize( ) const;

	// Anzahl der (freien) Objekte vergrößern
	// (Rückgabe: Anzahl der erzeugten Elemente)
	int Grow( const int iDelta );

	// Freies Objekt anfordern.
	// (Setzt den Referenzzähler des Objektes auf 1)
	CVAPoolObject* RequestObject( );

private:
	// Alias Typen
	typedef std::set<CVAPoolObject*> ObjSet;
	typedef ObjSet::iterator ObjSetIt;
	typedef ObjSet::const_iterator ObjSetCit;

	std::string m_sName;
	int m_iDelta;
	IVAPoolObjectFactory* m_pFactory;
	bool m_bDeleteFactory;

	ITACriticalSection m_csLists;

	ObjSet m_sFree; // Liste der freien Objekte
	ObjSet m_sUsed; // Liste der sich in Benutzung befindlichen Objekte

	// Diese Methode darf nur von den Pool-Objekten aufgerufen werden,
	// wenn diese keine weiteren Referenzen mehr haben.
	void ReleaseObject( CVAPoolObject* pObject );

	friend class CVAPoolObject;
};


// -= Implementierung object pool =----------------------------------------------------------------

//! Factory method for ObjectPool interface
IVAObjectPool* IVAObjectPool::Create( const int iInitialSize, const int iDelta, IVAPoolObjectFactory* pFactory, const bool bDeleteFactory )
{
	return new CVAObjectPool( iInitialSize, iDelta, pFactory, bDeleteFactory );
}

CVAObjectPool::CVAObjectPool( const int iInitialSize, const int iDelta, IVAPoolObjectFactory* pFactory, const bool bDeleteFactory )
    : m_iDelta( iDelta )
    , m_pFactory( pFactory )
    , m_bDeleteFactory( bDeleteFactory )
{
	// Objekte vorerzeugen
	Grow( iInitialSize );
}

CVAObjectPool::~CVAObjectPool( )
{
	ITACriticalSectionLock oLock( m_csLists );

	// Alle Objekte freigeben
	for( ObjSetIt it = m_sFree.begin( ); it != m_sFree.end( ); ++it )
		delete( *it );

	for( ObjSetIt it = m_sUsed.begin( ); it != m_sUsed.end( ); ++it )
		delete( *it );

	if( m_bDeleteFactory )
		delete m_pFactory;
};

void CVAObjectPool::SetName( const std::string& sName )
{
	m_sName = sName;
}

void CVAObjectPool::Reset( )
{
	// Alle benutzten Objekte zurück in die Freiliste
	for( ObjSetIt it = m_sUsed.begin( ); it != m_sUsed.end( ); ++it )
	{
		( *it )->ResetReferences( );
		( *it )->PreRelease( );
		m_sFree.insert( *it );
	}
	m_sUsed.clear( );
}

int CVAObjectPool::GetNumFree( ) const
{
	return (int)m_sFree.size( );
}

int CVAObjectPool::GetNumUsed( ) const
{
	return (int)m_sUsed.size( );
}

int CVAObjectPool::GetSize( ) const
{
	return (int)( m_sFree.size( ) + m_sUsed.size( ) );
}

int CVAObjectPool::Grow( int iDelta )
{
	ITACriticalSectionLock oLock( m_csLists );

	for( int i = 0; i < iDelta; i++ )
	{
		CVAPoolObject* pObject = m_pFactory->CreatePoolObject( );
		assert( pObject != nullptr );

		pObject->m_pParentPool = this;

		m_sFree.insert( pObject );
	}

#if POOL_DEBUG_MESSAGES == 1
	VA_VERBOSE( "ObjectPool" + m_sName, "Growing pool by " << iDelta << "." );
#endif

	return iDelta;
}

CVAPoolObject* CVAObjectPool::RequestObject( )
{
#if( POOL_NO_REUSE == 1 )

	// Direkte Implementierung mit new-operator (keine Wiederverwendung). Nur zum Debuggen.
	CVAPoolObject* pObject = m_pFactory->CreatePoolObject( );
	pObject->pParentPool   = this;
	pObject->AddReference( );
	pObject->PreRequest( );

#else

	// Lock auf Listen erzwingen
	ITACriticalSectionLock oLock( m_csLists );

	// Standard-Implementierung (mit Wiederverwendung)
	if( m_sFree.empty( ) )
	{
		Grow( m_iDelta );
		VA_TRACE( "ObjectPool", "Pool " << m_sName << " growing by " << m_iDelta << " objects required" );
	}

	assert( !m_sFree.empty( ) );
	CVAPoolObject* pObject = *( m_sFree.begin( ) );
	pObject->AddReference( );

	m_sFree.erase( pObject );
	m_sUsed.insert( pObject );

	pObject->PreRequest( );

#endif

#if( POOL_DEBUG_MESSAGES == 1 )
	VA_DEBUG_PRINTF( "[ObjectPool \"%s\" 0x%08Xh] Requested object 0x%08Xh (%d free, %d used, %d total)\n", m_sName.c_str( ), this, pObject, GetNumFree( ), GetNumUsed( ),
	                 GetSize( ) );
#endif

	return pObject;
}

void CVAObjectPool::ReleaseObject( CVAPoolObject* pObject )
{
	// Wichtig: ReleaseObjekt wird von den PoolObjekten aufrufen, wenn diese KEINE Referenzen mehr haben!
	assert( pObject->GetNumReferences( ) == 0 );

	pObject->PreRelease( );

#if( POOL_NO_REUSE == 1 )

	// Direkte Implementierung mit delete-operator (keine Wiederverwendung). Nur zum Debuggen.
	delete pObject;

#else

	// Lock auf Listen erzwingen
	ITACriticalSectionLock oLock( m_csLists );

	// Standard-Implementierung (mit Wiederverwendung)

	// Keine Referenzen mehr => Objekt wieder freigeben

	// Sicherheitscheck: Rückgabe = Anzahl gelöschter Objekte
	m_sUsed.erase( pObject );

	// TODO: Wichtig. Referenz-Management für Container beim Reset handhaben
	m_sFree.insert( pObject );
#endif // POOL_NO_REUSE

#if( POOL_DEBUG_MESSAGES == 1 )
	VA_DEBUG_PRINTF( "[CVAObjectPool \"%s\" 0x%08Xh] Released object 0x%08Xh (%d free, %d used, %d total)\n", m_sName.c_str( ), this, pObject, GetNumFree( ),
	                 GetNumUsed( ), GetSize( ) );
#endif
}
