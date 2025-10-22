#include "VAEigenUtils.h"

#include "VistaMath/VistaMathTools.h"

void VAEigenUtils::VAVec32Eigen( const VAVec3 vaIn, Eigen::Vector3d &eigenOut )
{
	eigenOut( 0 ) = vaIn.x;
	eigenOut( 1 ) = vaIn.y;
	eigenOut( 2 ) = vaIn.z;
}

void VAEigenUtils::Eigen2VAVec3( const Eigen::Vector3d eigenIn, VAVec3 &vaOut )
{
	vaOut.y = eigenIn( 0 );
	vaOut.z = eigenIn( 1 );
	vaOut.x = eigenIn( 2 );
}

double VAEigenUtils::Eigen2AzimuthRAD( const Eigen::Vector3d eigenIn )
{
	return std::acos( eigenIn( 2 ) / eigenIn( 0 ) );
}

double VAEigenUtils::Eigen2AzimuthDEG( const Eigen::Vector3d eigenIn )
{
	return std::acos( eigenIn( 2 ) / eigenIn( 0 ) ) / 3.14159265359 * 180;
}

double VAEigenUtils::Eigen2ElevationDEG( const Eigen::Vector3d eigenIn )
{
	return 0;
}