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

#include "VAHardwareSetup.h"

#include "Utils/VAUtils.h"
#include "VALog.h"

#include <ITAFileSystemUtils.h>
#include <assert.h>

std::vector<const CVAHardwareDevice*> CVAHardwareSetup::GetDeviceListFromOutputGroup( const std::string& sIdentifier ) const
{
	for( size_t i = 0; i < voOutputs.size( ); i++ )
	{
		const CVAHardwareOutput& oGroup( voOutputs[i] );
		if( toUppercase( oGroup.sIdentifier ) == toUppercase( sIdentifier ) )
			return oGroup.vpDevices;
	}

	std::vector<const CVAHardwareDevice*> oEmptyList;
	return oEmptyList;
}

std::vector<const CVAHardwareDevice*> CVAHardwareSetup::GetDeviceListFromInputGroup( const std::string& sInputIdentifier ) const
{
	for( size_t i = 0; i < voInputs.size( ); i++ )
	{
		const CVAHardwareInput& oGroup( voInputs[i] );
		if( toUppercase( oGroup.sIdentifier ) == toUppercase( sInputIdentifier ) )
			return oGroup.vpDevices;
	}

	std::vector<const CVAHardwareDevice*> oEmptyList;
	return oEmptyList;
}

void CVAHardwareSetup::Init( const CVAStruct& oArgs )
{
	CVAConfigInterpreter conf( oArgs );

	// Hardware devices
	CVAStruct::const_iterator cit = oArgs.Begin( );
	while( cit != oArgs.End( ) )
	{
		const std::string& sKey( cit->first );

		std::vector<std::string> vsKeyParts = splitString( sKey, ':' );
		if( vsKeyParts.size( ) == 2 && cit->second.GetDatatype( ) == CVAStructValue::STRUCT )
		{
			std::string sCategory( toUppercase( vsKeyParts[0] ) ); // uppercase
			if( sCategory == "OUTPUTDEVICE" )
			{
				CVAConfigInterpreter devconf( cit->second );

				std::string sIdentifier = vsKeyParts[1];

				std::string sDeviceType;
				devconf.OptString( "Type", sDeviceType, "LS" );

				// @todo:	discuss how to create type-specific objects,
				//			i.e. with LS-EQ-DAFF pointer or LSEQ filter

				CVAHardwareDevice device;

				device.sIdentifier = sIdentifier;
				device.sType       = sDeviceType;

				devconf.OptString( "Description", device.sDesc );

				std::string sChannelList;
				devconf.ReqString( "Channels", sChannelList );
				if( sChannelList == "true" ) // hack, VAStruct returns the string 'true' if a number '1' is parsed instead of a string '1'
					device.viChannels.push_back( 1 );
				else
					device.viChannels = StringToIntVec( sChannelList );

				std::string sPos;
				devconf.OptString( "Position", sPos, "0,0,0" );

				std::vector<std::string> vsPosComponents = splitString( sPos, ',' );
				assert( vsPosComponents.size( ) == 3 );
				device.vPos.x = StringToFloat( vsPosComponents[0] );
				device.vPos.y = StringToFloat( vsPosComponents[1] );
				device.vPos.z = StringToFloat( vsPosComponents[2] );

				VAVec3 v3View;
				std::string sView;
				devconf.OptString( "View", sView, "0,0,-1" );

				std::vector<std::string> vsViewComponents = splitString( sView, ',' );
				assert( vsViewComponents.size( ) == 3 );
				v3View.x = StringToFloat( vsViewComponents[0] );
				v3View.y = StringToFloat( vsViewComponents[1] );
				v3View.z = StringToFloat( vsViewComponents[2] );

				VAVec3 v3Up;
				std::string sUp;
				devconf.OptString( "View", sUp, "0,0,-1" );

				std::vector<std::string> vsUpComponents = splitString( sUp, ',' );
				assert( vsUpComponents.size( ) == 3 );
				v3Up.x = StringToFloat( vsUpComponents[0] );
				v3Up.y = StringToFloat( vsUpComponents[1] );
				v3Up.z = StringToFloat( vsUpComponents[2] );

				ConvertViewUpToQuaternion( v3View, v3Up, device.qOrient );

				devconf.OptString( "DataFileName", device.sDataFileName );

				voHardwareOutputDevices.push_back( device );
			}
			else if( sCategory == "INPUTDEVICE" )
			{
				CVAConfigInterpreter devconf( cit->second );

				CVAHardwareDevice device;

				device.sIdentifier = vsKeyParts[1];

				devconf.OptString( "Type", device.sType, "MIC" );

				devconf.OptString( "Description", device.sDesc );

				std::string sChannelList;
				devconf.ReqString( "Channels", sChannelList );
				if( sChannelList == "true" )
					device.viChannels.push_back( 1 );
				else
					device.viChannels = StringToIntVec( sChannelList );

				std::string sPos;
				devconf.OptString( "Position", sPos, "0,0,0" );

				std::vector<std::string> vsPosComponents = splitString( sPos, ',' );
				assert( vsPosComponents.size( ) == 3 );
				device.vPos.x = StringToFloat( vsPosComponents[0] );
				device.vPos.y = StringToFloat( vsPosComponents[1] );
				device.vPos.z = StringToFloat( vsPosComponents[2] );

				VAVec3 v3View;
				std::string sView;
				devconf.OptString( "View", sView, "0,0,-1" );

				std::vector<std::string> vsViewComponents = splitString( sView, ',' );
				assert( vsViewComponents.size( ) == 3 );
				v3View.x = StringToFloat( vsViewComponents[0] );
				v3View.y = StringToFloat( vsViewComponents[1] );
				v3View.z = StringToFloat( vsViewComponents[2] );

				VAVec3 v3Up;
				std::string sUp;
				devconf.OptString( "View", sUp, "0,0,-1" );

				std::vector<std::string> vsUpComponents = splitString( sUp, ',' );
				assert( vsUpComponents.size( ) == 3 );
				v3Up.x = StringToFloat( vsUpComponents[0] );
				v3Up.y = StringToFloat( vsUpComponents[1] );
				v3Up.z = StringToFloat( vsUpComponents[2] );

				ConvertViewUpToQuaternion( v3View, v3Up, device.qOrient );

				voHardwareInputDevices.push_back( device );
			}
			else
			{
				// some other key with parts and a ':' seperator ...
				// ... not of our concern
			}
		}
		cit++;
	}

	// Re-run for intputs and outputs
	cit = oArgs.Begin( );
	while( cit != oArgs.End( ) )
	{
		const std::string& sKey( cit->first );

		std::vector<std::string> vsKeyParts = splitString( sKey, ":" );
		if( vsKeyParts.size( ) == 2 && cit->second.GetDatatype( ) == CVAStructValue::STRUCT )
		{
			std::string sCategory( toUppercase( vsKeyParts[0] ) ); // uppercase
			if( sCategory == "OUTPUT" )
			{
				CVAConfigInterpreter outconf( cit->second );
				CVAHardwareOutput oOutput;
				oOutput.sIdentifier = vsKeyParts[1];
				outconf.OptString( "Description", oOutput.sDesc );
				outconf.OptBool( "Enabled", oOutput.bEnabled, true );

				// Connect output devices to this output
				std::string sDevices;
				outconf.ReqString( "Devices", sDevices );
				std::vector<std::string> vsDeviceIdentifier = StringToStringVec( sDevices );
				for( size_t i = 0; i < vsDeviceIdentifier.size( ); i++ )
					for( size_t j = 0; j < voHardwareOutputDevices.size( ); j++ )
						if( vsDeviceIdentifier[i] == voHardwareOutputDevices[j].sIdentifier )
							oOutput.vpDevices.push_back( &voHardwareOutputDevices[j] );

				voOutputs.push_back( oOutput );
			}
			else if( sCategory == "INPUT" )
			{
				CVAConfigInterpreter inconf( cit->second );
				CVAHardwareInput oInput;
				oInput.sIdentifier = vsKeyParts[1];
				inconf.OptString( "Description", oInput.sDesc );
				inconf.OptBool( "Active", oInput.bActive, true );

				// Connect output devices to this output
				std::string sDevices;
				inconf.ReqString( "Devices", sDevices );
				std::vector<std::string> vsDeviceIdentifier = StringToStringVec( sDevices );
				for( size_t i = 0; i < vsDeviceIdentifier.size( ); i++ )
					for( size_t j = 0; j < voHardwareInputDevices.size( ); j++ )
						if( vsDeviceIdentifier[i] == voHardwareInputDevices[j].sIdentifier )
							oInput.vpDevices.push_back( &voHardwareInputDevices[j] );

				voInputs.push_back( oInput );
			}
			else
			{
				// some other key with parts and a ':' seperator ...
				// ... not of our concern
			}
		}
		cit++;
	}

	return;
}

const CVAHardwareInput* CVAHardwareSetup::GetInput( const std::string& sInput ) const
{
	for( size_t i = 0; i < voInputs.size( ); i++ )
		if( toUppercase( sInput ) == toUppercase( voInputs[i].sIdentifier ) )
			return &voInputs[i];
	return nullptr;
}

std::vector<int> CVAHardwareInput::GetPhysicalInputChannels( ) const
{
	std::vector<int> viChannels;

	for( size_t i = 0; i < vpDevices.size( ); i++ )
	{
		const CVAHardwareDevice* pDevice( vpDevices[i] );
		for( size_t j = 0; j < pDevice->viChannels.size( ); j++ )
			viChannels.push_back( pDevice->viChannels[j] );
	}

	return viChannels;
}

bool CVAHardwareOutput::IsEnabled( ) const
{
	return bEnabled;
}

const CVAHardwareOutput* CVAHardwareSetup::GetOutput( const std::string& sOutput ) const
{
	for( size_t i = 0; i < voOutputs.size( ); i++ )
		if( toUppercase( sOutput ) == toUppercase( voOutputs[i].sIdentifier ) )
			return &voOutputs[i];
	return nullptr;
}

CVAStruct CVAHardwareSetup::GetStruct( ) const
{
	CVAStruct oHWInputs, oHWOutputs, oHWDevice;
	for( size_t i = 0; i < voHardwareInputDevices.size( ); i++ )
	{
		const CVAHardwareDevice& oDevice( voHardwareInputDevices[i] );
		oHWDevice["orientation"]  = oDevice.qOrient.GetAsStruct( );
		oHWDevice["position"]     = oDevice.vPos.GetAsStruct( );
		oHWDevice["datafilename"] = oDevice.sDataFileName;
		oHWDevice["description"]  = oDevice.sDesc;
		oHWDevice["identifier"]   = oDevice.sIdentifier;
		oHWDevice["type"]         = oDevice.sType;
		oHWDevice["channels"]     = CVAStruct( );
		for( size_t j = 0; j < oDevice.viChannels.size( ); j++ )
			oHWDevice["channels"][std::to_string( long( j ) )] = oDevice.viChannels[j];
		oHWDevice["numchannels"] = int( oDevice.viChannels.size( ) );

		oHWInputs[oDevice.sIdentifier] = oHWDevice;
	}

	for( size_t i = 0; i < voHardwareOutputDevices.size( ); i++ )
	{
		const CVAHardwareDevice& oDevice( voHardwareOutputDevices[i] );
		oHWDevice["orientation"]  = oDevice.qOrient.GetAsStruct( );
		oHWDevice["position"]     = oDevice.vPos.GetAsStruct( );
		oHWDevice["datafilename"] = oDevice.sDataFileName;
		oHWDevice["description"]  = oDevice.sDesc;
		oHWDevice["identifier"]   = oDevice.sIdentifier;
		oHWDevice["type"]         = oDevice.sType;
		oHWDevice["channels"]     = CVAStruct( );
		for( size_t j = 0; j < oDevice.viChannels.size( ); j++ )
			oHWDevice["channels"][std::to_string( long( j ) )] = oDevice.viChannels[j];
		oHWDevice["numchannels"]        = int( oDevice.viChannels.size( ) );
		oHWOutputs[oDevice.sIdentifier] = oHWDevice;
	}

	CVAStruct oHW;
	oHW["inputedevices"] = oHWInputs;
	oHW["outputdevices"] = oHWOutputs;

	return oHW;
}

std::vector<int> CVAHardwareOutput::GetPhysicalOutputChannels( ) const
{
	std::vector<int> viChannels;

	for( size_t i = 0; i < vpDevices.size( ); i++ )
	{
		const CVAHardwareDevice* pDevice( vpDevices[i] );
		for( size_t j = 0; j < pDevice->viChannels.size( ); j++ )
			viChannels.push_back( pDevice->viChannels[j] );
	}

	return viChannels;
}

bool CVAHardwareInput::IsActive( ) const
{
	return bActive;
}
