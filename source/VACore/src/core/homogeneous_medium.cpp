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

void CVACoreImpl::SetHomogeneousMediumSoundSpeed( const double dSoundSpeed )
{
	VA_NO_REENTRANCE;
	VA_TRY
	{
		if( dSoundSpeed <= 0.0f )
			VA_EXCEPT2( INVALID_PARAMETER, "Speed of sound can not be zero or negative" );
		oHomogeneousMedium.dSoundSpeed = dSoundSpeed;
	}
	VA_RETHROW;
}

double CVACoreImpl::GetHomogeneousMediumSoundSpeed( ) const
{
	VA_NO_REENTRANCE;
	return oHomogeneousMedium.dSoundSpeed;
}

void CVACoreImpl::SetHomogeneousMediumTemperature( const double dDegreesCentigrade )
{
	VA_NO_REENTRANCE;
	VA_TRY
	{
		if( dDegreesCentigrade <= -270.0f )
			VA_EXCEPT2( INVALID_PARAMETER, "Temperature can not be below total zero" );
		oHomogeneousMedium.dTemperatureDegreeCentigrade = dDegreesCentigrade;
	}
	VA_RETHROW;
}

double CVACoreImpl::GetHomogeneousMediumTemperature( ) const
{
	VA_NO_REENTRANCE;
	return oHomogeneousMedium.dTemperatureDegreeCentigrade;
}

void CVACoreImpl::SetHomogeneousMediumStaticPressure( const double dPressurePascal )
{
	VA_NO_REENTRANCE;
	VA_TRY
	{
		if( dPressurePascal <= 0.0f )
			VA_EXCEPT2( INVALID_PARAMETER, "Static pressure can not be zero or negative" );
		oHomogeneousMedium.dStaticPressurePascal = dPressurePascal;
	}
	VA_RETHROW;
}

double CVACoreImpl::GetHomogeneousMediumStaticPressure( ) const
{
	VA_NO_REENTRANCE;
	return oHomogeneousMedium.dStaticPressurePascal;
}

void CVACoreImpl::SetHomogeneousMediumRelativeHumidity( const double dRelativeHumidityPercent )
{
	VA_NO_REENTRANCE;
	VA_TRY
	{
		if( dRelativeHumidityPercent < 0.0f )
			VA_EXCEPT2( INVALID_PARAMETER, "Relative humidity can not be negative" );
		oHomogeneousMedium.dRelativeHumidityPercent = dRelativeHumidityPercent;
	}
	VA_RETHROW;
}

double CVACoreImpl::GetHomogeneousMediumRelativeHumidity( )
{
	VA_NO_REENTRANCE;
	return oHomogeneousMedium.dRelativeHumidityPercent;
}

void CVACoreImpl::SetHomogeneousMediumShiftSpeed( const VAVec3& v3TranslationSpeed )
{
	VA_NO_REENTRANCE;
	VA_TRY
	{
		if( v3TranslationSpeed.Length( ) >= oHomogeneousMedium.dSoundSpeed )
			VA_EXCEPT2( INVALID_PARAMETER, "Medium shift can not be equal or faster than sound speed" );
		oHomogeneousMedium.v3ShiftSpeed = v3TranslationSpeed;
	}
	VA_RETHROW;
}

VAVec3 CVACoreImpl::GetHomogeneousMediumShiftSpeed( ) const
{
	VA_NO_REENTRANCE;
	return oHomogeneousMedium.v3ShiftSpeed;
}

void CVACoreImpl::SetHomogeneousMediumParameters( const CVAStruct& oParams )
{
	VA_NO_REENTRANCE;
	oHomogeneousMedium.oParameters = oParams;
}

CVAStruct CVACoreImpl::GetHomogeneousMediumParameters( const CVAStruct& oArgs )
{
	VA_NO_REENTRANCE;
	return oHomogeneousMedium.oParameters;
}
