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

#include "VAReferenceableObject.h"

#include <ITAAtomicOps.h>

CVAReferenceableObject::CVAReferenceableObject( ) : m_iRefCount( 0 ) {}

CVAReferenceableObject::~CVAReferenceableObject( ) {}

bool CVAReferenceableObject::HasReferences( ) const
{
	return ( GetNumReferences( ) > 0 );
}

int CVAReferenceableObject::GetNumReferences( ) const
{
	int iRefCount = atomic_read_int( (volatile int*)&m_iRefCount );
	return iRefCount;
}

// REMOVE:
void CVAReferenceableObject::ResetReferences( )
{
	atomic_write_int( &m_iRefCount, 0 );
}

int CVAReferenceableObject::AddReference( ) const
{
	// Read-modify-write
	int iCurRefCount, iNewRefCount;
	do
	{
		iCurRefCount = atomic_read_int( &m_iRefCount );
		iNewRefCount = iCurRefCount + 1;
	} while( !atomic_cas_int( &m_iRefCount, iCurRefCount, iNewRefCount ) );

	return iNewRefCount;
}

int CVAReferenceableObject::RemoveReference( ) const
{
	// DEBUG:

	// Read-modify-write
	int iCurRefCount, iNewRefCount;
	do
	{
		iCurRefCount = atomic_read_int( &m_iRefCount );

		// If no reference, do nothing
		assert( iCurRefCount > 0 );
		if( iCurRefCount == 0 )
			return 0;

		iNewRefCount = iCurRefCount - 1;
	} while( !atomic_cas_int( &m_iRefCount, iCurRefCount, iNewRefCount ) );

	return iNewRefCount;
}
