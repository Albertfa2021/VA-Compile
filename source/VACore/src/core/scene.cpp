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

const CVASceneManager* CVACoreImpl::GetSceneManager( ) const
{
	return m_pSceneManager;
}

std::string CVACoreImpl::CreateScene( const CVAStruct& oParams, const std::string& sSceneName )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		if( oParams.HasKey( "filepath" ) )
		{
			const std::string sFilePath = oParams["filepath"];
			VA_INFO( "Core", "Loading scene from file '" << sFilePath << "'" );
			std::string sDestFilename = correctPath( m_oCoreConfig.mMacros.SubstituteMacros( sFilePath ) );
			for( std::vector<CVAAudioRendererDesc>::iterator it = m_voRenderers.begin( ); it != m_voRenderers.end( ); ++it )
				it->pInstance->LoadScene( sDestFilename );

			// @todo: create a scene manager and return a proper scene identifier
			return sDestFilename;
		}
		else
		{
			VA_EXCEPT2( INVALID_PARAMETER, "Could not interpret scene parameters, missing key 'filepath'" );
		}
	}
	VA_RETHROW;
}

CVASceneInfo CVACoreImpl::GetSceneInfo( const std::string& sSceneID ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY { VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION; }
	VA_RETHROW;
}

bool CVACoreImpl::GetSceneEnabled( const std::string& sID ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY { VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION; }
	VA_RETHROW;
}

void CVACoreImpl::GetSceneIDs( std::vector<std::string>& vsIDs ) const
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

std::string CVACoreImpl::GetSceneName( const std::string& sID ) const
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

void CVACoreImpl::SetSceneName( const std::string& sID, const std::string& sName )
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}

void CVACoreImpl::SetSceneEnabled( const std::string& sID, const bool bEnabled /*= true */ )
{
	VA_EXCEPT_NOT_IMPLEMENTED_FUTURE_VERSION;
}
