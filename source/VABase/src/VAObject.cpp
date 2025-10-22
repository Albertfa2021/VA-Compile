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

#include <VAObject.h>
#include <cassert>

CVAObject::CVAObject( ) : m_iObjectID( 0 ) {}

CVAObject::CVAObject( const char* pszName ) : CVAObject( std::string( pszName ) ) {}

CVAObject::CVAObject( const std::string& sName ) : m_iObjectID( 0 ), m_sObjectName( sName ) {}

CVAObject::~CVAObject( ) {}

int CVAObject::GetObjectID( ) const
{
	return m_iObjectID;
}

void CVAObject::SetObjectID( const int iID )
{
	m_iObjectID = iID;
}

std::string CVAObject::GetObjectName( ) const
{
	return m_sObjectName;
}

void CVAObject::SetObjectName( const std::string& sName )
{
	m_sObjectName = sName;
}

CVAObjectInfo CVAObject::GetObjectInfo( ) const
{
	CVAObjectInfo oInfo;
	oInfo.iID   = GetObjectID( );
	oInfo.sName = GetObjectName( );
	oInfo.sDesc = "";
	return oInfo;
}
