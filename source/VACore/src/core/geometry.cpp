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

int CVACoreImpl::CreateGeometryMesh( const CVAGeometryMesh& oMesh, const std::string& sName )
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

int CVACoreImpl::CreateGeometryMeshFromParameters( const CVAStruct& oParams, const std::string& sName )
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

bool CVACoreImpl::DeleteGeometryMesh( const int iID )
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

CVAGeometryMesh CVACoreImpl::GetGeometryMesh( const int iID ) const
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

void CVACoreImpl::GetGeometryMeshIDs( std::vector<int>& viIDs ) const
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

void CVACoreImpl::SetGeometryMeshName( const int iID, const std::string& sName )
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

std::string CVACoreImpl::GetGeometryMeshName( const int iID ) const
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

void CVACoreImpl::SetGeometryMeshParameters( const int iID, const CVAStruct& oParams )
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

CVAStruct CVACoreImpl::GetGeometryMeshParameters( const int iID, const CVAStruct& oParams ) const
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

void CVACoreImpl::SetGeometryMeshEnabled( const int iID, const bool bEnabled )
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

bool CVACoreImpl::GetGeometryMeshEnabled( const int iID ) const
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}
