#include "../src/Motion/VAMotionModelBase.cpp"
#include "../src/Motion/VAMotionModelBase.h"
#include "../src/Scene/VAMotionState.cpp"
#include "../src/Scene/VAMotionState.h"
#include "../src/Scene/VASceneStateBase.cpp"
#include "../src/Utils/VADebug.cpp"
#include "../src/Utils/VAUtils.cpp"
#include "../src/VALog.cpp"

#include <VAException.h>
#include <fstream>
#include <iomanip>
#include <iostream>

using namespace std;

void test( )
{
	CVABasicMotionModel::Config oConf;
	oConf.SetDefaults( );

	CVABasicMotionModel m( oConf );
	VAVec3 p, u, v;
	VAQuat q;

	// Query ohne vorherige Werte
	m.EstimateMotion( 0, p, q );
	cout << p << endl << q << endl << endl;

	CVAMotionState oNewState;
	oNewState.SetPosition( VAVec3( 0, 0, 0 ) );
	oNewState.SetOrientation( VAQuat( 0, 0, 0 ) );
	oNewState.Initialize( 0.0f );

	m.InputMotionKey( &oNewState );
	m.EstimateMotion( 0, p, q );
	cout << p << endl << q << endl << endl;

	CVAMotionState oNewState2;
	oNewState2.Initialize( 1.0f );
	m.InputMotionKey( &oNewState2 );

	double t;
	for( t = 1; t <= 2; t += 0.100 )
	{
		m.EstimateMotion( t, p, q );
		cout << "t=" << t << ": p = " << p << "; q = " << q << endl;
	}
	cout << endl;


	CVAMotionState oNewState3;
	oNewState3.SetPosition( VAVec3( 1, 0, 0 ) );
	oNewState3.SetOrientation( VAQuat( sqrt( 2 ), 0, 0, sqrt( 2 ) ) );
	oNewState3.Initialize( 2 );
	m.InputMotionKey( &oNewState3 );

	for( ; t <= 3; t += 0.100 )
	{
		m.EstimateMotion( t, p, q );
		cout << "t=" << t << ": p = " << p << "; q = " << q << endl;
	}
	cout << endl;

	CVAMotionState oNewState4;
	oNewState4.SetPosition( VAVec3( 2, 0, 0 ) );
	oNewState4.SetOrientation( VAQuat( 1, 0, 0 ) );
	oNewState4.Initialize( 2 );
	m.InputMotionKey( &oNewState4 );

	for( ; t <= 4; t += 0.100 )
	{
		m.EstimatePosition( t, p );
		cout << "t=" << t << ": p = " << p << endl;
	}
	cout << endl;
}

void test_to_file( )
{
	CVABasicMotionModel::Config oConf;
	oConf.SetDefaults( );

	CVABasicMotionModel m( oConf );
	VAVec3 p, u, v;
	VAQuat q;

	const double timestep = 0.0029;
	double t              = 0;
	ofstream fout( "motion.dat" );

	for( int i = 0; i < 60; i++ )
	{
		if( i == 0 )
		{
			cout << "!!!" << endl;
			CVAMotionState oState;
			oState.Initialize( t );
			oState.SetPosition( VAVec3( ) );
			oState.SetOrientation( VAQuat( ) );
			m.InputMotionKey( &oState );
		}

		if( i == 10 )
		{
			CVAMotionState oState;
			oState.Initialize( t );
			oState.SetPosition( VAVec3( 100 * t + 0.001, 0, 0 ) );
			oState.SetOrientation( VAQuat( 100 * t + 0.001, 0, 0, 1 ) );
			m.InputMotionKey( &oState );
			cout << "!!!" << endl;
		}

		m.EstimateMotion( t, p, q );
		cout << "t=" << t << ": p = " << p << "; q = " << q << endl;

		fout << t << "\t" << p.x << "\t" << p.y << "\t" << p.z << endl;
		fout << t << "\t" << q.x << "\t" << q.y << "\t" << q.z << "\t" << q.w << endl;

		t += timestep;
	}

	cout << endl;
	fout.close( );
}

int main( )
{
	try
	{
		test( );
		test_to_file( );
	}
	catch( CVAException& e )
	{
		cerr << "Error: " << e << endl;
		return e.GetErrorCode( );
	}

	return 0;
}
