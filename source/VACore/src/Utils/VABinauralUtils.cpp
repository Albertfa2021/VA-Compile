#include "VABinauralUtils.h"

#include <ITAConstants.h>
#include <VAException.h>
#include <math.h>

double VABinauralUtils::TimeOfArrivalModel::SphericalShape( const VAVec3& v3EarDirection, const VAVec3& v3DirectionOfIncidentWaveFront, double dSphereRadius,
                                                            double dSpeedOfSound )
{
	if( dSpeedOfSound <= 0 )
		VA_EXCEPT2( INVALID_PARAMETER, "Speed of sound can't be zero or negative." );

	if( dSphereRadius <= 0 )
		VA_EXCEPT2( INVALID_PARAMETER, "Sphere radius must be greater zero or negative" );

	VAVec3 v3EarDirectionNormalized( v3EarDirection );
	VAVec3 v3DirectionOfIncidentWaveFrontDirection( v3DirectionOfIncidentWaveFront );
	double dDotProduct = VAVec3( v3EarDirectionNormalized ).Dot( v3DirectionOfIncidentWaveFrontDirection );
	double dAlpha      = std::acos( dDotProduct );

	double dDelay = dSphereRadius / dSpeedOfSound;


	if( dAlpha <= ITAConstants::PI_D )
	{
		return dDelay * ( 1.0f - cos( dAlpha ) );
	}
	else
	{
		return dDelay * ( 1.0f + dAlpha - ITAConstants::HALF_PI_D );
	}
}

double VABinauralUtils::TimeOfArrivalModel::SphericalShapeGetLeft( double dAzimuth, double dElevation, double dSphereRadius, double dSpeedOfSound )
{
	double dDelay = dSphereRadius / dSpeedOfSound;
	return dDelay * ( ( sin( dAzimuth ) * cos( dElevation ) + 1.0f ) / 2.0f );
}

double VABinauralUtils::TimeOfArrivalModel::SphericalShapeGetRight( double dAzimuth, double dElevation, double dSphereRadius, double dSpeedOfSound )
{
	double dDelay = dSphereRadius / dSpeedOfSound;
	return dDelay * ( ( sin( dAzimuth - ITAConstants::PI_D ) * cos( dElevation ) + 1.0f ) / 2.0f );
}
