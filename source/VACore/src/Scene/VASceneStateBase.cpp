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

#include "VASceneStateBase.h"

CVASceneStateBase::CVASceneStateBase( ) : m_pManager( nullptr ), m_bFixed( false ) {}

CVASceneStateBase::~CVASceneStateBase( ) {}

CVASceneManager* CVASceneStateBase::GetManager( ) const
{
	return m_pManager;
}

double CVASceneStateBase::GetModificationTime( ) const
{
	return m_dModificationTime;
}

bool CVASceneStateBase::IsFixed( ) const
{
	return m_bFixed;
}

void CVASceneStateBase::Fix( )
{
	SetFixed( true );
}

void CVASceneStateBase::PreRelease( )
{
	// Vor (erneuter) Benutzung => Nicht-finalisiert
	m_bFixed = false;
}

void CVASceneStateBase::SetManager( CVASceneManager* pSceneManager )
{
	m_pManager = pSceneManager;
}

void CVASceneStateBase::SetModificationTime( const double dModificationTime )
{
	m_dModificationTime = dModificationTime;
}

void CVASceneStateBase::SetFixed( const bool bFixed )
{
	m_bFixed = bFixed;
}
