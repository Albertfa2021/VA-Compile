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

#include "VAUtils.h"

#include "../Scene/VAMotionState.h"

#include <ITAConfigUtils.h>
#include <ITAConstants.h>
#include <ITAFileSystemUtils.h>
#include <ITANumericUtils.h>
#include <ITAStringUtils.h>
#include <VistaBase/VistaQuaternion.h>
#include <VistaBase/VistaTimeUtils.h>
#include <VistaTools/VistaFileSystemFile.h>
#include <cassert>

#ifdef WIN32
#	include <windows.h>
#endif

std::tuple<VistaVector3D, VistaQuaternion> ConvertToVistaPositionAndOrientation( const VAVec3& vPos, const VAVec3& vView, const VAVec3& vUp )
{
	VistaVector3D v3Pos( vPos.comp );
	VistaQuaternion qOrient;
	qOrient.SetFromViewAndUpDir( -VistaVector3D( vView.comp ), VistaVector3D( vUp.comp ) );

	return std::tie( v3Pos, qOrient );
}

void VASleep( int iMilliseconds )
{
	VistaTimeUtils::Sleep( iMilliseconds );
}

long getCurrentThreadID( )
{
#ifdef WIN32
	return GetCurrentThreadId( );
#else
	assert( !"Not implemented" );
	return -1;
#endif
}

float getCurrentProcessSystemLoad( )
{
	/*
	FILETIME ftSysIdle, ftSysKernel, ftSysUser;
	FILETIME ftProcCreation, ftProcExit, ftProcKernel, ftProcUser;

	if (SUCCEEDED(GetSystemTimes(&ftSysIdle, &ftSysKernel, &ftSysUser)) &&
	SUCCEEDED(GetProcessTimes(GetCurrentProcess(), &ftProcCreation, &ftProcExit, &ftProcKernel, &ftProcUser)))
	{

	}
	*/
	return 0.0f;
}

CVAException getDefaultUnexpectedVAException( )
{
	return CVAException( CVAException::UNSPECIFIED, "An unspecified exception occured. Please contact the developer." );
}

CVAException convert2VAException( const ITAException& e )
{
	// TODO: Refine
	return CVAException( CVAException::UNSPECIFIED, e.sModule + ": " + e.sReason );
}

CVAException convert2VAException( const VistaExceptionBase& e )
{
	// TODO: Refine
	return CVAException( CVAException::UNSPECIFIED, e.GetExceptionText( ) );
}

void ConvertQuaternionToViewUp( const VAQuat& qOrient, VAVec3& v3View, VAVec3& v3Up )
{
	VistaQuaternion qVistaQuat;
	qVistaQuat[Vista::X] = float( qOrient.x );
	qVistaQuat[Vista::Y] = float( qOrient.y );
	qVistaQuat[Vista::Z] = float( qOrient.z );
	qVistaQuat[Vista::W] = float( qOrient.w );

	VistaVector3D vVistaView = qVistaQuat.GetViewDir( );
	vVistaView.Normalize( );
	v3View.Set( vVistaView[Vista::X], vVistaView[Vista::Y], vVistaView[Vista::Z] );

	VistaVector3D vVistaUp = qVistaQuat.GetUpDir( );
	vVistaUp.Normalize( );
	v3Up.Set( vVistaUp[Vista::X], vVistaUp[Vista::Y], vVistaUp[Vista::Z] );
}

void ConvertViewUpToQuaternion( const VAVec3& vView, const VAVec3& vUp, VAQuat& qOrient )
{
	VistaVector3D vVistaView( float( vView.x ), float( vView.y ), float( vView.z ) );
	vVistaView.Normalize( );
	VistaVector3D vVistaUp( float( vUp.x ), float( vUp.y ), float( vUp.z ) );
	vVistaUp.Normalize( );

	VistaQuaternion qVistaQuat;
	qVistaQuat.SetFromViewAndUpDir( vVistaView, vVistaUp );
	qOrient.x = qVistaQuat[Vista::X];
	qOrient.y = qVistaQuat[Vista::Y];
	qOrient.z = qVistaQuat[Vista::Z];
	qOrient.w = qVistaQuat[Vista::W];
}

std::string correctPathForLUA( const std::string& sPath )
{
	// Convert into system path
	std::string s = correctPath( sPath );

	// Einfache Backslashes in doppelte Backslashes umwandeln
	std::string sSingleSeperator = std::string( 1, PATH_SEPARATOR );
	std::string sDoubleSeperator = std::string( 1, PATH_SEPARATOR ) + std::string( 1, PATH_SEPARATOR );
	size_t pos                   = 0;
	while( ( pos = s.find( sSingleSeperator, pos ) ) != std::string::npos )
	{
		s.replace( pos, 1, sDoubleSeperator.c_str( ), 2 );
		++++pos;
	}

	return s;
}

CVAStructValue interpretStructKey( const std::string& s )
{
	ITAConversion conv( ITAConversion::STRICT_MODE );
	bool b;
	int i;
	double d;
	if( conv.StringToBool( s, b ) )
		return CVAStructValue( b );
	if( conv.StringToInt( s, i ) )
		return CVAStructValue( i );
	if( conv.StringToDouble( s, d ) )
		return CVAStructValue( d );
	// Fallback solution: Interpret as string
	return CVAStructValue( s );
}

void LoadStructFromINIFIle( const std::string& sFilePath, CVAStruct& oData )
{
	if( !VistaFileSystemFile( sFilePath ).Exists( ) )
		VA_EXCEPT2( FILE_NOT_FOUND, std::string( "INI file \"" ) + sFilePath + std::string( "\" not found" ) );

	INIFileUseFile( sFilePath );

	oData.Clear( );
	auto vsSections = INIFileGetSections( sFilePath );

	for( std::vector<std::string>::iterator it = vsSections.begin( ); it != vsSections.end( ); ++it )
	{
		std::string& sSection( *it );
		INIFileUseSection( sSection );

		std::vector<std::string> vsKeys = INIFileGetKeys( sSection );

		// Spezialfall: Section [] nach Root abbilden
		if( it->empty( ) )
		{
			for( std::vector<std::string>::iterator jt = vsKeys.begin( ); jt != vsKeys.end( ); ++jt )
			{
				std::string& sKey( *jt );
				if( oData.HasKey( sKey ) )
					VA_EXCEPT2( INVALID_PARAMETER, "Uniqueness violation in " + sFilePath + ": multiple detection of key '" + sKey + "' in section '" + sSection + "'" );
				const std::string sValue = INIFileReadString( sKey );
				oData[sKey]              = interpretStructKey( sValue );
			}
		}
		else
		{
			CVAStruct oSubData;
			for( std::vector<std::string>::iterator jt = vsKeys.begin( ); jt != vsKeys.end( ); ++jt )
			{
				std::string& sKey( *jt );
				if( oData.HasKey( sKey ) )
					VA_EXCEPT2( INVALID_PARAMETER, "Uniqueness violation in " + sFilePath + ": multiple detection of key '" + sKey + "' in section '" + sSection + "'" );

				const std::string sValue = INIFileReadString( sKey );
				oSubData[sKey]           = interpretStructKey( sValue );
			}
			oData[sSection] = oSubData;
		}
	}
}

void StoreStructToINIFile( const std::string& sFilePath, const CVAStruct& oData )
{
	CVAStruct::const_iterator cit = oData.Begin( );
	while( cit != oData.End( ) )
	{
		const std::string& sKey( cit->first );
		const CVAStructValue& oValue( cit->second );
		cit++;


		// Root-level plain key-value pairs, a bit unusual though

		switch( oValue.GetDatatype( ) )
		{
			case CVAStructValue::BOOL:
			{
				INIFileWriteBool( sKey, bool( oValue ) );
				break;
			}
			case CVAStructValue::INT:
			{
				INIFileWriteInt( sKey, int( oValue ) );
				break;
			}
			case CVAStructValue::DOUBLE:
			{
				INIFileWriteDouble( sKey, double( oValue ) );
				break;
			}
			case CVAStructValue::STRING:
			{
				INIFileWriteString( sKey, std::string( oValue ) );
				break;
			}
			case CVAStructValue::STRUCT:
			{
				const std::string& sSection( sKey );
				const CVAStruct& oSection( oValue );

				CVAStruct::const_iterator cit_section = oSection.Begin( );
				while( cit_section != oSection.End( ) )
				{
					const std::string& sKeyInSection( cit_section->first );
					const CVAStructValue& oSectionValue( cit_section->second );
					cit_section++;


					// Section-level key-value pairs

					switch( oSectionValue.GetDatatype( ) )
					{
						case CVAStructValue::BOOL:
						{
							INIFileWriteBool( sFilePath, sSection, sKeyInSection, bool( oSectionValue ) );
							break;
						}
						case CVAStructValue::INT:
						{
							INIFileWriteInt( sFilePath, sSection, sKeyInSection, int( oSectionValue ) );
							break;
						}
						case CVAStructValue::DOUBLE:
						{
							INIFileWriteDouble( sFilePath, sSection, sKeyInSection, double( oSectionValue ) );
							break;
						}
						case CVAStructValue::STRING:
						{
							INIFileWriteString( sFilePath, sSection, sKeyInSection, std::string( oSectionValue ) );
							break;
						}
					}
				}
				break;
			}
		}
	}
}

double GetAzimuthOnTarget_DEG( const VAVec3& vOriginPos, const VAVec3& vView, const VAVec3& vUp, const VAVec3& vTargetPos )
{
	return GetAzimuthFromDirection_DEG( vView, vUp, vTargetPos - vOriginPos );
}

double GetAzimuthFromDirection_DEG( const VAVec3& vView, const VAVec3& vUp, VAVec3 vDirection )
{
	vDirection.Norm( );
	VAVec3 vViewMinusZ            = vView * ( -1.0f );        // local z axis
	const VAVec3 vRight           = vViewMinusZ.Cross( vUp ); // local x axis
	const double dAzimuthAngleDeg = atan2( vDirection.Dot( vRight ), vDirection.Dot( vView ) ) * 180.0f / ITAConstants::PI_D;
	return ( ( dAzimuthAngleDeg < 0.0f ) ? ( dAzimuthAngleDeg + 360.0f ) : dAzimuthAngleDeg );
}

double GetElevationOnTarget_DEG( const VAVec3& vOriginPos, const VAVec3& vUp, const VAVec3& vTargetPos )
{
	return GetElevationFromDirection_DEG( vUp, vTargetPos - vOriginPos );
}

double GetElevationFromDirection_DEG( const VAVec3& vUp, VAVec3 vDirection )
{
	vDirection.Norm( );
	const double dElevationAngleDeg = asin( vDirection.Dot( vUp ) ) * 180.0f / ITAConstants::PI_D;
	return dElevationAngleDeg;
}

VistaVector3D VAVec3ToVistaVector3D( const VAVec3& vaVec )
{
	return VistaVector3D( vaVec.x, vaVec.y, vaVec.z );
}

VAVec3 VistaVector3DToVAVec3( const VistaVector3D& vistaVec )
{
	return VAVec3( vistaVec[0], vistaVec[1], vistaVec[2] );
}

void SetCoreEventParams( CVAEvent& oEvent, const CVAMotionState* pMotionState )
{
	assert( pMotionState );
	oEvent.vPos         = pMotionState->GetPosition( );
	oEvent.vView        = pMotionState->GetView( );
	oEvent.vUp          = pMotionState->GetUp( );
	oEvent.oOrientation = pMotionState->GetOrientation( );
	oEvent.qHATO        = pMotionState->GetHeadAboveTorsoOrientation( );
}
