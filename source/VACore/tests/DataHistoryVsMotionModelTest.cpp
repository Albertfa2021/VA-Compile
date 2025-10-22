#include "../src/Motion/VAMotionModelBase.cpp"
#include "../src/Motion/VAMotionModelBase.h"
#include "../src/Scene/VAMotionState.cpp"
#include "../src/Scene/VAMotionState.h"
#include "../src/Scene/VASceneStateBase.cpp"
#include "../src/Utils/VADebug.cpp"
#include "../src/Utils/VAUtils.cpp"
#include "../src/VALog.cpp"


//#include "../src/DataHistory/VABoolHistoryModel.h"
//#include "../src/DataHistory/VADoubleHistoryModel.h"
//#include "../src/DataHistory/VADoubleHistoryModel.cpp"
//#include "../src/DataHistory/VASpectrumHistoryModel.h"
//#include "../src/DataHistory/VASpectrumHistoryModel.cpp"

#include "../src/DataHistory/VADataHistoryConfig.h"
#include "../src/DataHistory/VADataHistoryModel.h"
#include "../src/DataHistory/VADataHistoryModel_impl.h"
#include "../src/DataHistory/VAVec3HistoryModel.cpp"
#include "../src/DataHistory/VAVec3HistoryModel.h"

#include <VAException.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <math.h>
#include <memory>

using namespace std;


void CreateParabolicTrajectory( std::vector<double>& vdTimestamps, std::vector<VAVec3>& vv3TrajectoryPositions, double deltaT = 0.5 )
{
	vdTimestamps.clear( );
	vv3TrajectoryPositions.clear( );

	const double tMin   = 0.0;
	const double tMax   = 5.0;
	const double xMin   = 0.0;
	const double xMax   = 40.0;
	const double deltaX = deltaT / tMax * xMax;

	int iNumPos = std::ceil( xMax / deltaX );
	vv3TrajectoryPositions.reserve( iNumPos );
	vdTimestamps.reserve( iNumPos );

	double t = tMin;
	for( double x = xMin; x <= xMax; x += deltaX )
	{
		double y = 10e-11 * std::pow( x, 8 ) / 25;
		vv3TrajectoryPositions.push_back( VAVec3( x, y, 0 ) );
		vdTimestamps.push_back( t );
		t += deltaT;
	}
}

void test( )
{
	double dDeltaTIn  = 0.5;
	double dDeltaTOut = 0.01;

	CVABasicMotionModel::Config oConfMotion;
	oConfMotion.bLogEstimatedOutputEnabled = true;
	oConfMotion.bLogInputEnabled           = true;
	oConfMotion.iNumHistoryKeys            = 100;
	oConfMotion.dWindowDelay               = 0.1 * 5;
	oConfMotion.dWindowSize                = 0.2 * 5;

	CVADataHistoryConfig oConfHistory;
	oConfHistory.bLogOutputEnabled = true;
	oConfHistory.bLogInputEnabled  = true;
	oConfHistory.iBufferSize       = oConfMotion.iNumHistoryKeys;
	oConfHistory.dWindowDelay      = oConfMotion.dWindowDelay;
	oConfHistory.dWindowSize       = oConfMotion.dWindowSize;


	CVABasicMotionModel oMotionModel( oConfMotion );
	CVAVec3HistoryModel oVAVec3HistoryModel( oConfHistory, CVAHistoryEstimationMethod::EMethod::TriangularWindow );

	oMotionModel.SetName( "ParabolicTrajectoryMotionModel" );
	oVAVec3HistoryModel.SetLoggingBaseFilename( "ParabolicTrajectoryDataHistory" );

	std::vector<double> vdTimestamps;
	std::vector<VAVec3> vv3DiscretePos;
	CreateParabolicTrajectory( vdTimestamps, vv3DiscretePos, dDeltaTIn );

	VAQuat qOrientation( 0, 0, 0 );
	for( int idx = 0; idx < vdTimestamps.size( ); idx++ )
	{
		oMotionModel.InputMotionKey( vdTimestamps[idx], vv3DiscretePos[idx], qOrientation );
		oVAVec3HistoryModel.Push( vdTimestamps[idx], std::make_unique<VAVec3>( vv3DiscretePos[idx] ) );
	}
	oVAVec3HistoryModel.Update( );

	VAVec3 v3Dummy;
	// VAQuat qDummy;
	for( double t = vdTimestamps.front( ); t <= vdTimestamps.back( ); t += dDeltaTOut )
	{
		oMotionModel.EstimatePosition( t, v3Dummy );
		oVAVec3HistoryModel.Estimate( t, v3Dummy );
	}
}


int main( )
{
	try
	{
		test( );
	}
	catch( CVAException& e )
	{
		cerr << "Error: " << e << endl;
		return e.GetErrorCode( );
	}

	return 0;
}
