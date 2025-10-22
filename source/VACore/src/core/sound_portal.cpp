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

int CVACoreImpl::CreateSoundPortal( const std::string& sName /*= "" */ )
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
	// m_pSceneManager->CreatePortalDesc()
}

void CVACoreImpl::GetSoundPortalIDs( std::vector<int>& vPortalIDs )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY { m_pSceneManager->GetHeadSceneState( )->GetPortalIDs( &vPortalIDs ); }
	VA_RETHROW;
}

CVASoundPortalInfo CVACoreImpl::GetSoundPortalInfo( const int iID ) const
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

std::string CVACoreImpl::GetSoundPortalName( const int iPortalID ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY { return m_pSceneManager->GetPortalName( iPortalID ); }
	VA_RETHROW;
}

void CVACoreImpl::SetSoundPortalName( const int iPortalID, const std::string& sName )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		m_pSceneManager->SetPortalName( iPortalID, sName );

		CVAEvent ev;
		ev.iEventType = CVAEvent::SOUND_PORTAL_CHANGED_NAME;
		ev.pSender    = this;
		ev.iObjectID  = iPortalID;
		ev.sName      = sName;
		m_pEventManager->BroadcastEvent( ev );
	}
	VA_RETHROW;
}

void CVACoreImpl::SetSoundPortalMaterial( const int iSoundPortalID, const int iMaterialID )
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

int CVACoreImpl::GetSoundPortalMaterial( const int iSoundPortalID ) const
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

void CVACoreImpl::SetSoundPortalNextPortal( const int iSoundPortalID, const int iNextSoundPortalID )
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

int CVACoreImpl::GetSoundPortalNextPortal( const int iSoundPortalID ) const
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

void CVACoreImpl::SetSoundPortalSoundReceiver( const int iSoundPortalID, const int iSoundReceiverID )
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

int CVACoreImpl::GetSoundPortalSoundReceiver( const int iSoundPortalID ) const
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

void CVACoreImpl::SetSoundPortalSoundSource( const int iSoundPortalID, const int iSoundSourceID )
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

int CVACoreImpl::GetSoundPortalSoundSource( const int iSoundPortalID ) const
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

bool CVACoreImpl::GetSoundPortalEnabled( const int iPortalID ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY { VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION; }
	VA_RETHROW;
}

CVAStruct CVACoreImpl::GetSoundPortalParameters( const int iID, const CVAStruct& oArgs ) const
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

void CVACoreImpl::SetSoundPortalParameters( const int iID, const CVAStruct& oParams )
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

void CVACoreImpl::SetSoundPortalPosition( const int iSoundPortalID, const VAVec3& vPos )
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

VAVec3 CVACoreImpl::GetSoundPortalPosition( const int iSoundPortalID ) const
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

void CVACoreImpl::SetSoundPortalOrientation( const int iSoundPortalID, const VAQuat& qOrient )
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

VAQuat CVACoreImpl::GetSoundPortalOrientation( const int iSoundPortalID ) const
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

void CVACoreImpl::SetSoundPortalEnabled( const int iID, const bool bEnabled )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		VA_EXCEPT_NOT_IMPLEMENTED_NEXT_VERSION;
		// m_pSceneManager->SetPortalEnabled( iID, bEnabled );
	}
	VA_RETHROW;
}
