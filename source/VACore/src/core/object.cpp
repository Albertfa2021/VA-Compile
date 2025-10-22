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

CVAObjectInfo CVACoreImpl::GetObjectInfo( ) const
{
	CVAObjectInfo oCoreModuleInfo;
	oCoreModuleInfo.iID   = GetObjectID( );
	oCoreModuleInfo.sName = GetObjectName( );
	oCoreModuleInfo.sDesc = "VA core module";
	return oCoreModuleInfo;
}

// Handle calls to the kernel from the module interface
CVAStruct CVACoreImpl::CallObject( const CVAStruct& oArgs )
{
	VA_VERBOSE( "Core", "Core module has been called" );
	CVAStruct oReturn;

	if( oArgs.HasKey( "help" ) || oArgs.HasKey( "info" ) || oArgs.IsEmpty( ) )
	{
		oReturn["help"]           = "Get comprehensive help online at http://www.virtualacoustics.org ...";
		oReturn["version"]        = "Returns version information";
		oReturn["addsearchpath"]  = "Adds a search path";
		oReturn["getsearchpaths"] = "Returns all search paths";
		oReturn["getloglevel"]    = "Returns current core log level (integer)";
		oReturn["setloglevel"]    = "Sets current core log level (integer)";
		oReturn["structrebound"]  = "Rebounds argument of any content to client (for testing VA structs)";
	}

	if( oArgs.HasKey( "version" ) )
	{
		CVAVersionInfo oVersionInfo;
		GetVersionInfo( &oVersionInfo );
		oReturn["comments"] = oVersionInfo.sComments;
		oReturn["date"]     = oVersionInfo.sDate;
		oReturn["flags"]    = oVersionInfo.sFlags;
		oReturn["version"]  = oVersionInfo.sVersion;
		oReturn["string"]   = oVersionInfo.ToString( );
	}

	if( oArgs.HasKey( "addsearchpath" ) )
	{
		if( oArgs["addsearchpath"].IsString( ) )
		{
			std::string sPath = oArgs["addsearchpath"];
			VistaFileSystemDirectory oDir( sPath );
			oReturn["pathvalid"] = false;
			if( oDir.Exists( ) )
			{
				m_oCoreConfig.vsSearchPaths.push_back( sPath );
				oReturn["pathvalid"] = true;
			}
		}
	}

	if( oArgs.HasKey( "getsearchpaths" ) )
	{
		CVAStruct oSearchPaths;
		for( size_t i = 0; i < m_oCoreConfig.vsSearchPaths.size( ); i++ )
			oSearchPaths["path_" + std::to_string( long( i ) )] = m_oCoreConfig.vsSearchPaths[i];
		oReturn["searchpaths"] = oSearchPaths;
	}

	if( oArgs.HasKey( "getmacros" ) )
	{
		CVAStruct oMacros;
		for( auto& macro: m_oCoreConfig.mMacros.GetMacroMapCopy( ) )
			oMacros[macro.first] = macro.second;
		oReturn["macros"] = oMacros;
	}

	if( oArgs.HasKey( "getloglevel" ) )
	{
		oReturn["loglevel"] = VALog_GetLogLevel( );
	}

	if( oArgs.HasKey( "setloglevel" ) )
	{
		int iLogLevel = oArgs["setloglevel"];
		VALog_SetLogLevel( iLogLevel );
	}
	if( oArgs.HasKey( "structrebound" ) )
	{
		oReturn = oArgs["structrebound"];
	}

	if( oArgs.HasKey( "shutdown" ) || oArgs.HasKey( "finalize" ) || oArgs.HasKey( "stop" ) )
	{
		VA_WARN( "Core", "Received shutdown request" );
		if( GetCoreConfig( )->bRemoteShutdownAllowed )
		{
			VA_TRACE( "Core", "Accepting remote shutdown request, will broadcast shutdown event" );

			CVAEvent ev;
			ev.pSender    = this;
			ev.iEventType = CVAEvent::SHOTDOWN_REQUEST;
			ev.sParam     = "shutdown";
			m_pEventManager->BroadcastEvent( ev );

			VA_TRACE( "Core", "Shutdown performed" );
		}
		else
		{
			VA_WARN( "Core", "Shutdown request denied, the core configuration does not accept a remote shutdown" );
		}
	}

	bool bUpdateRecordInputPath = false;
	if( oArgs.HasKey( "Audio device/RecordInputFileName" ) )
	{
		m_oCoreConfig.sRecordDeviceInputFileName = std::string( oArgs["Audio device/RecordInputFileName"] );
		bUpdateRecordInputPath                   = true;
	}
	if( oArgs.HasKey( "Audio device/RecordInputBaseFolder" ) )
	{
		m_oCoreConfig.sRecordDeviceInputBaseFolder = std::string( oArgs["Audio device/RecordInputBaseFolder"] );
		bUpdateRecordInputPath                     = true;
	}

	if( bUpdateRecordInputPath )
	{
		VistaFileSystemFile oFile( m_oCoreConfig.sRecordDeviceInputFileName );
		VistaFileSystemDirectory oFolder( m_oCoreConfig.sRecordDeviceInputBaseFolder );

		VistaFileSystemFile oFilePath( oFolder.GetName( ) + "/" + oFile.GetLocalName( ) );
		VA_INFO( "Core", "Updating record device input file path to " << oFilePath.GetName( ) );

		if( oFilePath.Exists( ) )
			VA_INFO( "Core", "Record device input file path exists, will overwrite" );

		if( !oFolder.Exists( ) )
			if( !oFolder.CreateWithParentDirectories( ) )
				VA_EXCEPT2( INVALID_PARAMETER, "Could not create record device output directory " + oFolder.GetName( ) );

		m_pStreamProbeDeviceInput->SetFilePath( oFilePath.GetName( ) );
	}


	bool bUpdateRecordOutputPath = false;
	if( oArgs.HasKey( "Audio device/RecordOutputFileName" ) )
	{
		m_oCoreConfig.sRecordDeviceOutputFileName = std::string( oArgs["Audio device/RecordOutputFileName"] );
		bUpdateRecordOutputPath                   = true;
	}
	if( oArgs.HasKey( "Audio device/RecordOutputBaseFolder" ) )
	{
		m_oCoreConfig.sRecordDeviceOutputBaseFolder = std::string( oArgs["Audio device/RecordOutputBaseFolder"] );
		bUpdateRecordOutputPath                     = true;
	}

	if( bUpdateRecordOutputPath )
	{
		VistaFileSystemFile oFile( m_oCoreConfig.sRecordDeviceOutputFileName );
		VistaFileSystemDirectory oFolder( m_oCoreConfig.sRecordDeviceOutputBaseFolder );

		VistaFileSystemFile oFilePath( oFolder.GetName( ) + "/" + oFile.GetLocalName( ) );
		VA_INFO( "Core", "Updating record device output file path to " << oFilePath.GetName( ) );

		if( oFilePath.Exists( ) )
			VA_INFO( "Core", "Record device output file path exists, will overwrite" );

		if( !oFolder.Exists( ) )
			if( !oFolder.CreateWithParentDirectories( ) )
				VA_EXCEPT2( INVALID_PARAMETER, "Could not create record device output directory " + oFolder.GetName( ) );

		m_pStreamProbeDeviceOutput->SetFilePath( oFilePath.GetName( ) );
	}


	VA_VERBOSE( "Core", "Core module will transmit answer now" );
	return oReturn;
}
