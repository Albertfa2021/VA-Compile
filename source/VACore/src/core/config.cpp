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

const CVACoreConfig* CVACoreImpl::GetCoreConfig( ) const
{
	return &m_oCoreConfig;
}

void VACore::StoreCoreConfigToFile( const CVAStruct& oConfig, const std::string& sConfigFilePath )
{
	StoreStructToINIFile( sConfigFilePath, oConfig );
}

CVAStruct VACore::LoadCoreConfigFromFile( const std::string& sConfigFilePath )
{
	CVAStruct oFinalCoreConfigStruct, oCurrentConfig;

	VistaFileSystemFile oConfigFile( sConfigFilePath );
	std::list<VistaFileSystemFile> voConfigFiles;

	std::vector<VistaFileSystemDirectory> voIncludePaths;
	if( oConfigFile.Exists( ) && oConfigFile.GetParentDirectory( ).empty( ) == false )
		voIncludePaths.push_back( oConfigFile.GetParentDirectory( ) );

	voConfigFiles.push_back( VistaFileSystemFile( sConfigFilePath ) );

	VA_INFO( "Core", "Working directory: '" << VistaFileSystemDirectory::GetCurrentWorkingDirectory( ) << "'" );

	while( voConfigFiles.empty( ) == false )
	{
		VistaFileSystemFile oCurrentConfigFile( voConfigFiles.front( ) );
		voConfigFiles.pop_front( );

		if( oCurrentConfigFile.Exists( ) == false )
		{
			for( size_t n = 0; n < voIncludePaths.size( ); n++ )
			{
				std::string sCombinedFilePath = voIncludePaths[n].GetName( ) + PATH_SEPARATOR + oCurrentConfigFile.GetLocalName( );
				oCurrentConfigFile.SetName( sCombinedFilePath );
				if( oCurrentConfigFile.Exists( ) && oCurrentConfigFile.IsFile( ) )
				{
					VA_INFO( "Config", "Including further configuration file '" + oCurrentConfigFile.GetLocalName( ) + "' from include path '" +
					                       voIncludePaths[n].GetName( ) + "'" );
					break;
				}
			}

			if( !oCurrentConfigFile.Exists( ) )
			{
				VA_EXCEPT2( FILE_NOT_FOUND, "Configuration file '" + oCurrentConfigFile.GetLocalName( ) + "' not found, aborting." );
			}
		}

		VA_VERBOSE( "Config", std::string( "Reading INI file '" ) + oCurrentConfigFile.GetLocalName( ) + "'" );
		LoadStructFromINIFIle( oCurrentConfigFile.GetName( ), oCurrentConfig );

		if( oCurrentConfig.HasKey( "paths" ) )
		{
			const CVAStruct& oPaths( oCurrentConfig["paths"] );
			CVAStruct::const_iterator it = oPaths.Begin( );
			while( it != oPaths.End( ) )
			{
				const CVAStructValue& oIncludePath( ( it++ )->second );
				VistaFileSystemDirectory oNewPathDir( oIncludePath );
				if( oNewPathDir.Exists( ) && oNewPathDir.IsDirectory( ) )
					voIncludePaths.push_back( oNewPathDir );
			}
		}

		if( oCurrentConfig.HasKey( "files" ) )
		{
			const CVAStruct& oPaths( oCurrentConfig["files"] );
			CVAStruct::const_iterator it = oPaths.Begin( );
			while( it != oPaths.End( ) )
			{
				const CVAStructValue& oIncludeFile( ( it++ )->second );

				voConfigFiles.push_back( VistaFileSystemFile( oIncludeFile ) );
			}
		}

		oCurrentConfig.RemoveKey( "files" );

		// Merge structs (check for uniqueness)
		oFinalCoreConfigStruct.Merge( oCurrentConfig, true );
	}

	return oFinalCoreConfigStruct;
}

CVAStruct CVACoreImpl::GetCoreConfiguration( const bool bFilterEnabled ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	CVAStruct oCoreConfig;

	if( bFilterEnabled )
	{
		CVAStruct::const_iterator cit = m_oCoreConfig.GetStruct( ).Begin( );
		while( cit != m_oCoreConfig.GetStruct( ).End( ) )
		{
			const std::string sKey( cit->first );
			const CVAStructValue& oVal( cit->second );
			++cit;

			if( oVal.IsStruct( ) )
			{
				const CVAStruct& oSection( oVal.GetStruct( ) );
				if( oSection.HasKey( "enabled" ) )
					if( bool( oSection["enabled"] ) == false )
						continue; // Only skip if explicitly not enabled
				oCoreConfig[sKey] = oVal;
			}
		}
	}
	else
	{
		oCoreConfig = m_oCoreConfig.GetStruct( );
	}

	return oCoreConfig;
}

CVAStruct CVACoreImpl::GetHardwareConfiguration( ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;
	return m_oCoreConfig.oHardwareSetup.GetStruct( );
}
