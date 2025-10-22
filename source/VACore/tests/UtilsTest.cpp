#include <ITANumericUtils.h>
#include <Utils/VAUtils.h>
#include <iostream>

using namespace std;

void printConversionYPR2VU( double y, double p, double r )
{
	double vx, vy, vz, ux, uy, uz;
	convertYPR2VU( grad2radf( y ), grad2radf( p ), grad2radf( r ), vx, vy, vz, ux, uy, uz );
	cout.precision( 2 );
	cout << fixed << "y=" << y << ", p=" << p << ", r=" << r << "   -->   v = (" << vx << "," << vy << "," << vz << "), u = (" << ux << "," << uy << "," << uz << ")"
	     << endl;
}

void testConversionYPR2VU( )
{
	printConversionYPR2VU( 0, 0, 0 );
	printConversionYPR2VU( 90, 0, 0 );
	printConversionYPR2VU( 0, 90, 0 );
	printConversionYPR2VU( 0, 0, 90 );
	printConversionYPR2VU( 90, 90, 0 );
	printConversionYPR2VU( 90, 0, 90 );
	printConversionYPR2VU( 0, 90, 90 );
}

void printConversionVU2YPR( double vx, double vy, double vz, double ux, double uy, double uz )
{
	double y, p, r;
	convertVU2YPR( vx, vy, vz, ux, uy, uz, y, p, r );
	cout.precision( 2 );
	cout << fixed << "v = (" << vx << "," << vy << "," << vz << "), u = (" << ux << "," << uy << "," << uz << ")   -->   y = " << rad2gradf( y )
	     << ", p=" << rad2gradf( p ) << ", r=" << rad2gradf( r ) << endl;
}

void testConversionVU2YPR( )
{
	float q = sqrt( 2.0F ) / 2;

	// Unrotated
	printConversionVU2YPR( 0, 0, -1, 0, 1, 0 );

	// Yaw 45°
	printConversionVU2YPR( -q, 0, -q, 0, 1, 0 );

	// Pitch 90°
	printConversionVU2YPR( 0, 1, 0, 0, 0, 1 );

	// Pitch 45°
	printConversionVU2YPR( 0, q, -q, 0, q, q );

	// Yaw 90° + Pitch 90°
	printConversionVU2YPR( 0, 1, 0, 1, 0, 0 );

	// Yaw 90° + Pitch -90°
	printConversionVU2YPR( 0, -1, 0, -1, 0, 0 );

	// Roll 90°
	printConversionVU2YPR( 0, 0, -1, 1, 0, 0 );

	// Pitch 45°, Roll 90°
	printConversionVU2YPR( 0, q, -q, 1, 0, 0 );

	// Pitch 45°, Roll -90°
	printConversionVU2YPR( 0, q, -q, -1, 0, 0 );

	/*
	// Pitch 90°
	printConversionVU2YPR(0,1,0, 0,0,1);

	// Pitch 90° + Yaw 90°
	printConversionVU2YPR(0,1,0, 1,0,0);

	// Pitch 90° + Yaw -90°
	printConversionVU2YPR(0,1,0, -1,0,0);

	// Pitch -90° + Yaw 90°
	printConversionVU2YPR(0,-1,0, -1,0,0);
	*/
}

int main( )
{
	// testConversionYPR2VU();
	testConversionVU2YPR( );

	return 0;
}
