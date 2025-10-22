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

void CVACoreImpl::RegisterModule( CVAObject* pModule )
{
	m_oModules.RegisterObject( pModule );
}

void CVACoreImpl::GetModules( std::vector<CVAModuleInfo>& viModuleInfos ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

#ifdef VACORE_MODULE_INTERFACE_ENABLED
	VA_TRY
	{
		std::vector<CVAObjectInfo> v;
		m_oModules.GetObjectInfos( v );

		VA_PRINT( "Available modules (" << v.size( ) << ")" );

		viModuleInfos.clear( );
		viModuleInfos.resize( v.size( ) );
		for( size_t i = 0; i < v.size( ); i++ )
		{
			VA_PRINT( "'" << v[i].sName << "'\t\t\t" << v[i].sDesc );
			viModuleInfos[i].sName = v[i].sName;
			viModuleInfos[i].sDesc = v[i].sDesc;
		}
	}
	VA_RETHROW;

#else // VACORE_MODULE_INTERFACE_ENABLED

	VA_EXCEPT1( "This VACore version does not provide modules" );

#endif // VACORE_MODULE_INTERFACE_ENABLED
}

CVAStruct CVACoreImpl::CallModule( const std::string& sModuleName, const CVAStruct& oArgs )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

#ifdef VACORE_MODULE_INTERFACE_ENABLED

	VA_TRY
	{
		CVAObject* pModule = m_oModules.FindObjectByName( sModuleName );
		if( !pModule )
		{
			VA_EXCEPT2( INVALID_PARAMETER, "Module '" + sModuleName + "' not found" );
		}

#	ifdef VACORE_MODULE_INTERFACE_MECHANISM_EVENT_BASED

		CVAEvent ev;
		ev.iEventType = CVAEvent::SIGNALSOURCE_STATE_CHANGED;
		ev.pSender    = this;
		ev.sObjectID  = sModuleName;
		ev;
		m_pEventManager->BroadcastEvent( ev );
		m_pEventManager->

#	else // not VACORE_MODULE_INTERFACE_MECHANISM_EVENT_BASED

		return pModule->CallObject( oArgs );

#	endif // VACORE_MODULE_INTERFACE_MECHANISM_EVENT_BASED
	}
	VA_RETHROW;

#else // VACORE_MODULE_INTERFACE_ENABLED

#	ifdef VACORE_NO_MODULE_INTERFACE_THROW_EXCEPTION
	VA_EXCEPT1( "This VACore version does not provide modules" );
#	endif // VACORE_NO_MODULE_INTERFACE_THROW_EXCEPTION

#endif // VACORE_MODULE_INTERFACE_ENABLED
}
