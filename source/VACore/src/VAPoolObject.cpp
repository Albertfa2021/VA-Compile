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

#include <VAObjectPool.h>
#include <VAPoolObject.h>
#include <VAReferenceableObject.h>
#include <cassert>

CVAPoolObject::CVAPoolObject( ) : m_pParentPool( nullptr ) {}

CVAPoolObject::~CVAPoolObject( ) {}

int CVAPoolObject::RemoveReference( ) const
{
	assert( GetNumReferences( ) >= 1 );

	int iRefs = CVAReferenceableObject::RemoveReference( );
	if( ( iRefs <= 0 ) && m_pParentPool )
		m_pParentPool->ReleaseObject( const_cast<CVAPoolObject*>( this ) );

	return iRefs;
}
