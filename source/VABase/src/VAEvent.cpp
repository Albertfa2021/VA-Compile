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

#include <VAEvent.h>
#include <VAInterface.h>
#include <iomanip>
#include <iostream>
#include <sstream>

std::string CVAEvent::ToString( ) const
{
	std::stringstream ss;

	switch( iEventType )
	{
		case NOTHING:
			ss << "Nothing";
			break;

		case INITIALIZED:
			ss << "Core " << pSender << " initialized";
			break;

		case PROGRESS_UPDATE:
			ss << "Core " << pSender << " progress update";
			break;

		case RESET:
			ss << "Core reset";
			break;

		case FINALIZED:
			ss << "Core " << pSender << " finalized";
			break;

		case DIRECTIVITY_LOADED:
			ss << "Directivity loaded (ID = " << iObjectID << ")";
			break;

		case DIRECTIVITY_DELETED:
			ss << "Directivity " << iObjectID << " freed";
			break;

		case SIGNALSOURCE_CREATED:
			ss << "Signal source created (ID = " << sObjectID << ")";
			break;

		case SIGNALSOURCE_DELETED:
			ss << "Signal source " << sObjectID << " deleted";
			break;

		case SAMPLE_LOADED:
			ss << "Sample loaded (ID = " << iObjectID << ")";
			break;

		case SAMPLE_FREED:
			ss << "Sample " << iObjectID << " freed";
			break;

		case SOUND_SOURCE_CREATED:
			ss << "Sound source created (ID = " << iObjectID << ", amode = " << IVAInterface::GetAuralizationModeStr( iAuralizationMode )
			   << ", volume = " << IVAInterface::GetVolumeStrDecibel( dVolume ) << ")";
			break;

		case SOUND_SOURCE_DELETED:
			ss << "Sound source " << iObjectID << " deleted";
			break;

		case SOUND_SOURCE_CHANGED_NAME:
			ss << "Sound source " << iObjectID << " changed name to \"" << sName << "\"";
			break;

		case SOUND_SOURCE_CHANGED_SIGNALSOURCE:
			ss << "Sound source " << iObjectID << " changed signal source to \"" << sParam << "\"";
			break;

		case SOUND_SOURCE_CHANGED_AURALIZATIONMODE:
			ss << "Sound source " << iObjectID << " changed auralization mode to " << IVAInterface::GetAuralizationModeStr( iAuralizationMode );
			break;

		case SOUND_SOURCE_CHANGED_SOUND_POWER:
			ss << "Sound source " << iObjectID << " changed volume to " << IVAInterface::GetVolumeStrDecibel( dVolume );
			break;

		case SOUND_SOURCE_CHANGED_MUTING:
			ss << "Sound source " << iObjectID << ( bMuted ? " muted" : " unmuted" );
			break;

		case SOUND_SOURCE_CHANGED_POSE:
			ss << "Sound source " << iObjectID << " changed position orientation" << std::endl
			   << "P=" << vPos << ", "
			   << "V=" << vView << ", "
			   << "U=" << vUp;
			break;

		case SOUND_SOURCE_CHANGED_DIRECTIVITY:
			ss << "Sound source " << iObjectID << " changed directivity to " << iParamID;
			break;

		case SOUND_RECEIVER_CREATED:
			ss << "Sound receiver created (ID = " << iObjectID << ", amode = " << IVAInterface::GetAuralizationModeStr( iAuralizationMode ) << ")";
			break;

		case SOUND_RECEIVER_DELETED:
			ss << "Sound receiver " << iObjectID << " deleted";
			break;

		case SOUND_RECEIVER_CHANGED_NAME:
			ss << "Sound receiver " << iObjectID << " changed name to \"" << sName << "\"";
			break;

		case SOUND_RECEIVER_CHANGED_AURALIZATIONMODE:
			ss << "Sound receiver " << iObjectID << " changed auralization mode to " << IVAInterface::GetAuralizationModeStr( iAuralizationMode );
			break;

		case SOUND_RECEIVER_CHANGED_DIRECTIVITY:
			ss << "Sound receiver " << iObjectID << " is assigned HRIR dataset " << iParamID;
			break;

		case SOUND_RECEIVER_CHANGED_POSE:
			ss << "Sound receiver " << iObjectID << " changed position orientation" << std::endl
			   << "P=" << vPos << ", "
			   << "V=" << vView << ", "
			   << "U=" << vUp;
			break;

		case SOUND_PORTAL_CHANGED_NAME:
			ss << "Portal " << iObjectID << " changed name to \"" << sName << "\"";
			break;

		case SOUND_PORTAL_CHANGED_PARAMETER:
			ss << "Portal " << iObjectID << " changed state to \"" << std::setprecision( 3 ) << std::fixed << dState << "\"";
			break;

		case ACTIVE_SOUND_RECEIVER_CHANGED:
			ss << "Sound receiver " << iObjectID << " set as active sound receiver";
			break;

		case INPUT_GAIN_CHANGED:
			ss << "Gain of input " << iIndex << " set to " << IVAInterface::GetVolumeStrDecibel( dVolume );
			break;

		case OUTPUT_GAIN_CHANGED:
			ss << "Output gain set to " << IVAInterface::GetVolumeStrDecibel( dVolume );
			break;

		case OUTPUT_MUTING_CHANGED:
			ss << "Global muting " << ( bMuted ? "enabled" : "disabled" );
			break;

		case GLOBAL_AURALIZATION_MODE_CHANGED:
			ss << "Global auralization mode set to " << IVAInterface::GetAuralizationModeStr( iAuralizationMode );
			break;

		case MEASURES_UPDATE:
			/* TODO:
			ss << "Audiostream peaks update: inputs=(" << FloatVecToString(vfInputPeaks, 3)
			<< "), outputs=(" << FloatVecToString(vfOutputPeaks, 3) << ")";
			*/
			ss << "Audiostream peaks update";
			break;

		default:
			ss << "Invalid core event";
	}

	ss << std::endl;
	return ss.str( );
}
