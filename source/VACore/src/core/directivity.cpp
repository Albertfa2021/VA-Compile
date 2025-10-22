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

int CVACoreImpl::CreateDirectivityFromParameters( const CVAStruct& oParams, const std::string& sName )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		int iDirID = m_pDirectivityManager->CreateDirectivity( oParams, sName );

		assert( iDirID != -1 );

		CVAEvent ev;
		ev.iEventType = CVAEvent::DIRECTIVITY_LOADED;
		ev.pSender    = this;
		ev.iObjectID  = iDirID;
		m_pEventManager->BroadcastEvent( ev );

		VA_INFO( "Core", "Directivity successfully created, assigned identifier " << iDirID );

		return iDirID;
	}
	VA_RETHROW;
}

bool CVACoreImpl::DeleteDirectivity( const int iDirID )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		bool bSuccess = m_pDirectivityManager->DeleteDirectivity( iDirID );
		// assert( bSuccess );

		// Ereignis generieren, wenn Operation erfolgreich
		CVAEvent ev;
		ev.iEventType = CVAEvent::DIRECTIVITY_DELETED;
		ev.pSender    = this;
		ev.iObjectID  = iDirID;
		m_pEventManager->BroadcastEvent( ev );

		VA_INFO( "Core", "Released directivity " << iDirID );

		return bSuccess;
	}
	VA_RETHROW;
}

CVADirectivityInfo CVACoreImpl::GetDirectivityInfo( int iDirID ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY { return m_pDirectivityManager->GetDirectivityInfo( iDirID ); }
	VA_RETHROW;
}

void CVACoreImpl::GetDirectivityInfos( std::vector<CVADirectivityInfo>& vdiDest ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY { m_pDirectivityManager->GetDirectivityInfos( vdiDest ); }
	VA_RETHROW;
}

void CVACoreImpl::SetDirectivityName( const int iID, const std::string& sName )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		VA_EXCEPT_NOT_IMPLEMENTED_NEXT_VERSION;
		// m_pDirectivityManager->SetName( iID, sName );
	}
	VA_RETHROW;
}

std::string CVACoreImpl::GetDirectivityName( const int iID ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		CVADirectivityInfo oInfo = m_pDirectivityManager->GetDirectivityInfo( iID );
		return oInfo.sName;
	}
	VA_RETHROW;
}

void CVACoreImpl::SetDirectivityParameters( const int iID, const CVAStruct& oParams )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		VA_EXCEPT_NOT_IMPLEMENTED_NEXT_VERSION;
		// m_pDirectivityManager->SetParameters( iID, oParams );
	}
	VA_RETHROW;
}

CVAStruct CVACoreImpl::GetDirectivityParameters( const int iID, const CVAStruct& ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		CVADirectivityInfo oInfo = m_pDirectivityManager->GetDirectivityInfo( iID );
		return oInfo.oParams;

		// @todo
		// return m_pDirectivityManager->GetDirectivityParameters( iID, oParams );
	}
	VA_RETHROW;
}
