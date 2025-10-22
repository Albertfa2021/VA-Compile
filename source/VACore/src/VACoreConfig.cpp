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

#include "VACoreConfig.h"

#include "Reproduction/VAAudioReproduction.h"
#include "Utils/VAUtils.h"
#include "VALog.h"

#include <VABase.h>
#include <VAException.h>
#include <VAStruct.h>

// ITA includes
#include <ITAStringUtils.h>
#include <VistaTools/VistaFileSystemDirectory.h>

// STL includes
#include <iostream>
#include <set>

void CVACoreConfig::Init( const CVAStruct& oData )
{
	m_oData = oData;

	CVAConfigInterpreter conf( oData );

	// Get debug level first, will affect further config parsing outputs
	int iVALogLevel;
	conf.OptInteger( "Debug/LogLevel", iVALogLevel, VACORE_DEFAULT_LOG_LEVEL );
	VALog_SetLogLevel( iVALogLevel );

	conf.OptInteger( "Debug/TriggerUpdateMilliseconds", iTriggerUpdateMilliseconds, 100 );

	VA_TRACE( "Config", oData );

	CVAStruct oMacroStruct( conf.OptStruct( "Macros" ) );
	CVAStruct::const_iterator cit = oMacroStruct.Begin( );
	while( cit != oMacroStruct.End( ) )
	{
		const std::string& sMacroName( cit->first );
		const CVAStructValue& oMacroValue( cit->second );
		if( oMacroValue.IsString( ) )
		{
			std::string sMacroValue = mMacros.SubstituteMacros( oMacroValue );
			mMacros.AddMacro( "$(" + sMacroName + ")", sMacroValue );
			VA_VERBOSE( "Config", "Added macro $(" + sMacroName + ") = " + sMacroValue );
		}
		cit++;
	}


	CVAStruct oPathsStruct( conf.OptStruct( "Paths" ) );

	std::string sWorkingDirectory = VistaFileSystemDirectory::GetCurrentWorkingDirectory( );
	VA_INFO( "Core", "Current working directory: '" << sWorkingDirectory << "'" );
	if( oPathsStruct.HasKey( "WorkingDirectory" ) )
	{
		std::string sPath = oPathsStruct["WorkingDirectory"];
		VistaFileSystemDirectory oDir( sPath );
		if( oDir.Exists( ) )
		{
			VistaFileSystemDirectory::SetCurrentWorkingDirectory( sPath );
			std::string sWorkingDirectory = VistaFileSystemDirectory::GetCurrentWorkingDirectory( );
			VA_INFO( "Config", "Found valid working directory path entry, switching to: '" << sWorkingDirectory << "'" );
		}
		else
		{
			std::string sWorkingDirectory = VistaFileSystemDirectory::GetCurrentWorkingDirectory( );
			VA_WARN( "Config", "Requested working directory path '" << sPath << "' not found, will fall back to '" << sWorkingDirectory << "'" );
		}
	}

	cit = oPathsStruct.Begin( );
	while( cit != oPathsStruct.End( ) )
	{
		const std::string& sPathName( cit->first );
		const CVAStructValue& oPathValue( cit++->second );
		if( oPathValue.IsString( ) )
		{
			if( toLowercase( sPathName ) == "workingdirectory" )
				continue;

			std::string sPath = oPathValue;
			VistaFileSystemDirectory oDir( sPath );

			if( oDir.Exists( ) )
			{
				vsSearchPaths.push_back( sPath );
				mMacros.AddMacro( "$(" + sPathName + ")", sPath );
				VA_INFO( "Config", "Added search path '" + sPath + "'" );
			}
			else
			{
				VA_INFO( "Config", "Could not find path '" + sPath + "', removed from search path list." );
			}
		}
		else
		{
			VA_WARN( "Config", "Path with key '" + sPathName + "' is not a string value, skipping." );
		}
	}

	oAudioDriverConfig.Init( conf.OptStruct( "Audio driver" ) );

	oHardwareSetup.Init( oData );

	conf.OptBool( "Audio driver/RecordInputEnabled", bRecordDeviceInputEnabled, false );
	conf.OptString( "Audio driver/RecordInputFileName", sRecordDeviceInputFileName, "device_in.wav" );
	sRecordDeviceInputFileName = mMacros.SubstituteMacros( sRecordDeviceInputFileName );
	conf.OptString( "Audio driver/RecordInputBaseFolder", sRecordDeviceInputBaseFolder, "recordings/device" );
	sRecordDeviceInputBaseFolder = mMacros.SubstituteMacros( sRecordDeviceInputBaseFolder );

	conf.OptBool( "Audio driver/RecordOutputEnabled", bRecordDeviceOutputEnabled, false );
	conf.OptString( "Audio driver/RecordOutputFileName", sRecordDeviceOutputFileName, "device_out.wav" );
	sRecordDeviceOutputFileName = mMacros.SubstituteMacros( sRecordDeviceOutputFileName );
	conf.OptString( "Audio driver/RecordOutputBaseFolder", sRecordDeviceOutputBaseFolder, "recordings/device" );
	sRecordDeviceOutputBaseFolder = mMacros.SubstituteMacros( sRecordDeviceOutputBaseFolder );

	conf.OptNumber( "HomogeneousMedium/DefaultRelativeHumidity", oInitialHomogeneousMedium.dRelativeHumidityPercent, g_dDefaultRelativeHumidity );
	conf.OptNumber( "HomogeneousMedium/DefaultSoundSpeed", oInitialHomogeneousMedium.dSoundSpeed, g_dDefaultSpeedOfSound );
	conf.OptNumber( "HomogeneousMedium/DefaultStaticPressure", oInitialHomogeneousMedium.dStaticPressurePascal, g_dDefaultStaticPressure );
	conf.OptNumber( "HomogeneousMedium/DefaultTemperature", oInitialHomogeneousMedium.dTemperatureDegreeCentigrade, g_dDefaultTemperature );

	std::string sDefaultAmplitudeCalibrationMode;
	conf.OptString( "Calibration/DefaultAmplitudeCalibrationMode", sDefaultAmplitudeCalibrationMode, "94dB" );
	dDefaultAmplitudeCalibration = g_dSoundPower_94dB_SPL_1m;
	if( sDefaultAmplitudeCalibrationMode == "124dB" )
		dDefaultAmplitudeCalibration = g_dSoundPower_128dB_SPL_1m;

	conf.OptNumber( "Calibration/DefaultMinimumDistance", dDefaultMinimumDistance, 0.25f ); // translates to max +12 dB SPL
	conf.OptNumber( "Calibration/DefaultDistance", dDefaultDistance, 2.0f );                // translates to -6 dB SPL


	conf.OptBool( "RemoteShutdownAllowed", bRemoteShutdownAllowed, true );
}

const CVAStruct& CVACoreConfig::GetStruct( ) const
{
	return m_oData;
}
