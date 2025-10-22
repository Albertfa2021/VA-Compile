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

#include <VAException.h>
#include <VAInterface.h>
#include <VAStruct.h>
#include <VAVersion.h>
#include <algorithm>
#include <iomanip>
#include <sstream>

const double DECIBEL_MINUS_INFINITY = -1.5E-308;

#define VACORE_REGISTER_LITERAL( L ) push_back( CVAIntLiteral( "VACore", #L, IVAInterface::L ) )

class CVALiteralsTable : public std::vector<CVAIntLiteral>
{
public:
	CVALiteralsTable( )
	{
		// Make shure that you always keep this list consistent
		// Lua, Matlab, etc. take their definitions from here.

		VACORE_REGISTER_LITERAL( VA_CORESTATE_CREATED );
		VACORE_REGISTER_LITERAL( VA_CORESTATE_READY );
		VACORE_REGISTER_LITERAL( VA_CORESTATE_FAIL );

		VACORE_REGISTER_LITERAL( VA_PLAYBACK_STATE_PAUSED );
		VACORE_REGISTER_LITERAL( VA_PLAYBACK_STATE_PLAYING );
		VACORE_REGISTER_LITERAL( VA_PLAYBACK_STATE_STOPPED );

		VACORE_REGISTER_LITERAL( VA_PLAYBACK_ACTION_NONE );
		VACORE_REGISTER_LITERAL( VA_PLAYBACK_ACTION_STOP );
		VACORE_REGISTER_LITERAL( VA_PLAYBACK_ACTION_PLAY );
		VACORE_REGISTER_LITERAL( VA_PLAYBACK_ACTION_PAUSE );

		VACORE_REGISTER_LITERAL( VA_AURAMODE_NOTHING );
		VACORE_REGISTER_LITERAL( VA_AURAMODE_DIRECT_SOUND );
		VACORE_REGISTER_LITERAL( VA_AURAMODE_EARLY_REFLECTIONS );
		VACORE_REGISTER_LITERAL( VA_AURAMODE_DIFFUSE_DECAY );
		VACORE_REGISTER_LITERAL( VA_AURAMODE_SOURCE_DIRECTIVITY );
		VACORE_REGISTER_LITERAL( VA_AURAMODE_MEDIUM_ABSORPTION );
		VACORE_REGISTER_LITERAL( VA_AURAMODE_SCATTERING );
		VACORE_REGISTER_LITERAL( VA_AURAMODE_DIFFRACTION );
		VACORE_REGISTER_LITERAL( VA_AURAMODE_NEARFIELD );
		VACORE_REGISTER_LITERAL( VA_AURAMODE_DOPPLER );
		VACORE_REGISTER_LITERAL( VA_AURAMODE_SPREADING_LOSS );
		VACORE_REGISTER_LITERAL( VA_AURAMODE_TRANSMISSION );
		VACORE_REGISTER_LITERAL( VA_AURAMODE_DEFAULT );
		VACORE_REGISTER_LITERAL( VA_AURAMODE_ALL );
	};
};

#undef VACORE_REGISTER_LITERAL

// Global literals table
CVALiteralsTable g_oCoreLiterals;

IVAInterface::IVAInterface( ) {}

IVAInterface::~IVAInterface( ) {}

bool IVAInterface::AddSearchPath( const std::string& sPath )
{
	CVAStruct oArg;
	oArg["addsearchpath"] = sPath;
	CVAStruct oRet        = CallModule( "VACore", oArg );
	const bool bPathFound = oRet["pathvalid"];
	return bPathFound;
}

bool IVAInterface::GetAuralizationModeValid( const int iAuralizationMode )
{
	return ( iAuralizationMode >= 0 ) && ( iAuralizationMode <= VA_AURAMODE_LAST );
}

bool IVAInterface::GetVolumeValid( const double dVolume )
{
	return ( dVolume >= 0 );
}

bool IVAInterface::GetSignalSourceBufferPlaybackActionValid( const int iAction )
{
	if( iAction == IVAInterface::VA_PLAYBACK_ACTION_PAUSE )
		return true;
	if( iAction == IVAInterface::VA_PLAYBACK_ACTION_PLAY )
		return true;
	if( iAction == IVAInterface::VA_PLAYBACK_ACTION_STOP )
		return true;

	return false;
}

std::string IVAInterface::GetLogLevelStr( const int iLogLevel )
{
	if( iLogLevel == IVAInterface::VA_LOG_LEVEL_QUIET )
		return "QUIET";
	if( iLogLevel == IVAInterface::VA_LOG_LEVEL_ERROR )
		return "ERRORS";
	if( iLogLevel == IVAInterface::VA_LOG_LEVEL_WARNING )
		return "WARNINGS";
	if( iLogLevel == IVAInterface::VA_LOG_LEVEL_INFO )
		return "INFOS";
	if( iLogLevel == IVAInterface::VA_LOG_LEVEL_VERBOSE )
		return "VERBOSE";
	if( iLogLevel == IVAInterface::VA_LOG_LEVEL_TRACE )
		return "TRACE";
	return "UNDEFINED";
}

// Parses a token of an auralization mode part (must be uppercase, used below)
int ParseAuralizationModeToken( const std::string& t )
{
	if( t == "DS" )
		return IVAInterface::VA_AURAMODE_DIRECT_SOUND;
	if( t == "ER" )
		return IVAInterface::VA_AURAMODE_EARLY_REFLECTIONS;
	if( t == "DD" )
		return IVAInterface::VA_AURAMODE_DIFFUSE_DECAY;
	if( t == "SD" )
		return IVAInterface::VA_AURAMODE_SOURCE_DIRECTIVITY;
	if( t == "MA" )
		return IVAInterface::VA_AURAMODE_MEDIUM_ABSORPTION;
	if( t == "TV" )
		return IVAInterface::VA_AURAMODE_TEMP_VAR;
	if( t == "SC" )
		return IVAInterface::VA_AURAMODE_SCATTERING;
	if( t == "DF" )
		return IVAInterface::VA_AURAMODE_DIFFRACTION;
	if( t == "NF" )
		return IVAInterface::VA_AURAMODE_NEARFIELD;
	if( t == "DP" )
		return IVAInterface::VA_AURAMODE_DOPPLER;
	if( t == "SL" )
		return IVAInterface::VA_AURAMODE_SPREADING_LOSS;
	if( t == "TR" )
		return IVAInterface::VA_AURAMODE_TRANSMISSION;
	if( t == "AB" )
		return IVAInterface::VA_AURAMODE_ABSORPTION;
	return -1;
}

int IVAInterface::ParseAuralizationModeStr( const std::string& sModeStr, const int iBaseMode )
{
	// Remove all whitespaces, valid chars: Alphabetic + *+-,
	std::string s;
	for( size_t k = 0; k < sModeStr.size( ); k++ )
	{
		if( isspace( sModeStr[k] ) )
			continue;

		if( isalpha( sModeStr[k] ) )
		{
			s += char( toupper( sModeStr[k] ) );
			continue;
		}

		if( ( sModeStr[k] == '*' ) || ( sModeStr[k] == '+' ) || ( sModeStr[k] == '-' ) || ( sModeStr[k] == ',' ) )
		{
			s += char( toupper( sModeStr[k] ) );
			continue;
		}

		VA_EXCEPT2( INVALID_PARAMETER, "Auralization mode specification contains invalid characters" );
	}

	// Trivial cases: Empty string, "null", "none", "default", "all"
	if( s.empty( ) )
		return VA_AURAMODE_NOTHING;
	if( ( s == "NULL" ) || ( s == "NONE" ) )
		return VA_AURAMODE_NOTHING;
	if( s == "DEFAULT" )
		return VA_AURAMODE_DEFAULT;
	if( ( s == "ALL" ) || ( s == "*" ) )
		return VA_AURAMODE_ALL;

	// Format: List of tokens seperated by commas (possibly whitespaces)
	// [fwe] For not adding PCRE this is implemented by hand here
	size_t i = 0, j = 0, op = 0;
	bool err = false;
	int m    = 0; // Scanning modes: 0 => none, 1 => token
	int def = 0, plus = 0, minus = 0;
	std::string sErrorMsg;

	for( i; i < s.length( ); i++ )
	{
		if( isalpha( s[i] ) )
		{
			if( m == 0 )
			{
				// None => Begin token
				j = i;
				m = 1;
			}
			else
			{
				// Token => extend
				continue;
			}
		}

		if( s[i] == '+' )
		{
			if( ( m == 1 ) || ( op != 0 ) )
			{ // Scanning toking || multiple operands => Error
				err       = true;
				sErrorMsg = "Multiple plus operand found in auralisation mode";
				break;
			}

			op = 1;
		}

		if( s[i] == '-' )
		{
			if( ( m == 1 ) || ( op != 0 ) )
			{ // Scanning toking || multiple operands => Error
				err       = true;
				sErrorMsg = "Multiple minus operand found in auralisation mode";
				break;
			}

			op = 2;
		}

		if( s[i] == ',' )
		{
			if( m == 0 )
			{ // No token => Error
				err       = true;
				sErrorMsg = "No token in auralisation mode found";
				break;
			}
			else
			{ // Finished token
				std::string t = s.substr( j, i - j );
				int p         = ParseAuralizationModeToken( t );
				if( p == -1 )
				{
					// Invalid token
					sErrorMsg = "Invalid token '" + t + "' found in auralisation mode";
					err       = true;
					break;
				}

				if( op == 0 )
					def |= p; // No operator
				if( op == 1 )
					plus |= p; // Plus operator
				if( op == 2 )
					minus |= p; // Minus operator

				op = 0;
				j  = 0;
				m  = 0;
			}
		}
	}

	// Unfinished token?
	if( m != 0 )
	{
		std::string t = s.substr( j, i - j );
		int p         = ParseAuralizationModeToken( t );
		if( p == -1 )
		{
			// Invalid token
			sErrorMsg = "Invalid token '" + t + "' found in auralisation mode";
			err       = true;
		}

		if( op == 0 )
			def |= p; // No operator
		if( op == 1 )
			plus |= p; // Plus operator
		if( op == 2 )
			minus |= p; // Minus operator
	}

	if( err )
		VA_EXCEPT2( INVALID_PARAMETER, "Invalid auralization mode specification, " + sErrorMsg );


	// Combine modes:
	int result = ( iBaseMode & VA_AURAMODE_ALL );
	if( def != 0 ) // Assignment => Forget about the base
		result = def;

	// Plus flags
	result |= plus;

	// Minus flags (stronger than plus)
	result &= ~minus;

	return result;
}

std::string IVAInterface::GetAuralizationModeStr( const int iAuralizationMode, const bool bShortFormat )
{
	std::stringstream ss;

	if( !GetAuralizationModeValid( iAuralizationMode ) )
	{
		ss << "Invalid auralization mode (" << iAuralizationMode << ")";
		VA_EXCEPT2( INVALID_PARAMETER, ss.str( ) );
	}

	if( bShortFormat )
	{
		if( iAuralizationMode == 0 )
			return "NULL";

		if( iAuralizationMode & VA_AURAMODE_DIRECT_SOUND )
			ss << "DS, ";
		if( iAuralizationMode & VA_AURAMODE_EARLY_REFLECTIONS )
			ss << "ER, ";
		if( iAuralizationMode & VA_AURAMODE_DIFFUSE_DECAY )
			ss << "DD, ";
		if( iAuralizationMode & VA_AURAMODE_SOURCE_DIRECTIVITY )
			ss << "SD, ";
		if( iAuralizationMode & VA_AURAMODE_MEDIUM_ABSORPTION )
			ss << "MA, ";
		if( iAuralizationMode & VA_AURAMODE_TEMP_VAR )
			ss << "TV, ";
		if( iAuralizationMode & VA_AURAMODE_SCATTERING )
			ss << "SC, ";
		if( iAuralizationMode & VA_AURAMODE_DIFFRACTION )
			ss << "DF, ";
		if( iAuralizationMode & VA_AURAMODE_NEARFIELD )
			ss << "NF, ";
		if( iAuralizationMode & VA_AURAMODE_DOPPLER )
			ss << "DP, ";
		if( iAuralizationMode & VA_AURAMODE_SPREADING_LOSS )
			ss << "SL, ";
		if( iAuralizationMode & VA_AURAMODE_TRANSMISSION )
			ss << "TR, ";
		if( iAuralizationMode & VA_AURAMODE_ABSORPTION )
			ss << "AB, ";
	}
	else
	{
		if( iAuralizationMode == 0 )
			return "None (0)";

		if( iAuralizationMode & VA_AURAMODE_DIRECT_SOUND )
			ss << "direct sound, ";
		if( iAuralizationMode & VA_AURAMODE_EARLY_REFLECTIONS )
			ss << "early reflections, ";
		if( iAuralizationMode & VA_AURAMODE_DIFFUSE_DECAY )
			ss << "diffuse decay, ";
		if( iAuralizationMode & VA_AURAMODE_SOURCE_DIRECTIVITY )
			ss << "source directivity, ";
		if( iAuralizationMode & VA_AURAMODE_MEDIUM_ABSORPTION )
			ss << "medium absorption, ";
		if( iAuralizationMode & VA_AURAMODE_TEMP_VAR )
			ss << "atmospheric temporal variations, ";
		if( iAuralizationMode & VA_AURAMODE_SCATTERING )
			ss << "scattering, ";
		if( iAuralizationMode & VA_AURAMODE_DIFFRACTION )
			ss << "diffraction, ";
		if( iAuralizationMode & VA_AURAMODE_NEARFIELD )
			ss << "near-field effects, ";
		if( iAuralizationMode & VA_AURAMODE_DOPPLER )
			ss << "doppler shifts, ";
		if( iAuralizationMode & VA_AURAMODE_SPREADING_LOSS )
			ss << "spherical spreading loss, ";
		if( iAuralizationMode & VA_AURAMODE_TRANSMISSION )
			ss << "transmission, ";
		if( iAuralizationMode & VA_AURAMODE_ABSORPTION )
			ss << "absorption, ";
	}

	// Remove last ,_
	std::string s( ss.str( ) );
	s[0] = char( toupper( s[0] ) );
	return s.substr( 0, s.length( ) - 2 );
}

std::string IVAInterface::GetVolumeStrDecibel( const double dVolume )
{
	if( !GetVolumeValid( dVolume ) )
		return "Invalid";

	std::stringstream ss;
	if( dVolume > 0.0f )
	{
		double dVolume_dB = ( ( dVolume == 0 ) ? DECIBEL_MINUS_INFINITY : 20.0f * log10( dVolume ) );
		ss << std::fixed << std::setprecision( 3 ) << dVolume << " (" << dVolume_dB << " dB)";
	}
	else
	{
		ss << std::fixed << std::setprecision( 3 ) << 0.0f << " (-Inf dB)";
	}

	return ss.str( );
}

int IVAInterface::ParsePlaybackState( const std::string& t )
{
	std::string T = t;
	for( size_t i = 0; i < t.size( ); i++ )
		T[i] = char( toupper( t[i] ) );

	if( T == "PLAYING" )
		return IVAInterface::VA_PLAYBACK_STATE_PLAYING;
	if( T == "STOPPED" )
		return IVAInterface::VA_PLAYBACK_STATE_STOPPED;
	if( T == "PAUSED" )
		return IVAInterface::VA_PLAYBACK_STATE_PAUSED;

	return -1;
}

std::string IVAInterface::GetPlaybackStateStr( int iPlayState )
{
	if( iPlayState == VA_PLAYBACK_STATE_PAUSED )
		return "PAUSED";
	if( iPlayState == VA_PLAYBACK_STATE_PLAYING )
		return "PLAYING";
	if( iPlayState == VA_PLAYBACK_STATE_STOPPED )
		return "STOPPED";
	return "UNKOWN PLAYBACK STATE";
}

int IVAInterface::ParsePlaybackAction( const std::string& t )
{
	std::string T = t;
	for( size_t i = 0; i < t.size( ); i++ )
		T[i] = char( toupper( t[i] ) );

	if( T == "PLAY" )
		return IVAInterface::VA_PLAYBACK_ACTION_PLAY;
	if( T == "STOP" )
		return IVAInterface::VA_PLAYBACK_ACTION_STOP;
	if( T == "PAUSE" )
		return IVAInterface::VA_PLAYBACK_ACTION_PAUSE;
	return -1;
}

std::string IVAInterface::GetPlaybackActionStr( int iPlaybackAction )
{
	if( iPlaybackAction == VA_PLAYBACK_ACTION_PLAY )
		return "PLAY";
	if( iPlaybackAction == VA_PLAYBACK_ACTION_STOP )
		return "STOP";
	if( iPlaybackAction == VA_PLAYBACK_ACTION_PAUSE )
		return "PAUSE";
	return "UNKOWN PLAYBACK ACTION";
}

const std::vector<CVAIntLiteral>& IVAInterface::GetLiterals( )
{
	return g_oCoreLiterals;
}
