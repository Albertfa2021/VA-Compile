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

int CVACoreImpl::CreateAcousticMaterial( const CVAAcousticMaterial& oMaterial, const std::string& sName )
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

int CVACoreImpl::CreateAcousticMaterialFromParameters( const CVAStruct& oParams, const std::string& sName )
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

bool CVACoreImpl::DeleteAcousticMaterial( const int iID )
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

CVAAcousticMaterial CVACoreImpl::GetAcousticMaterialInfo( const int iID ) const
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

void CVACoreImpl::GetAcousticMaterialInfos( std::vector<CVAAcousticMaterial>& voDest ) const
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

void CVACoreImpl::SetAcousticMaterialName( const int iID, const std::string& sName )
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

std::string CVACoreImpl::GetAcousticMaterialName( const int iID ) const
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

void CVACoreImpl::SetAcousticMaterialParameters( const int iID, const CVAStruct& oParams )
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

CVAStruct CVACoreImpl::GetAcousticMaterialParameters( const int iID, const CVAStruct& oParams ) const
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}
