#include "../src/DataHistory/VABoolHistoryModel.h"
#include "../src/DataHistory/VADoubleHistoryModel.cpp"
#include "../src/DataHistory/VADoubleHistoryModel.h"
#include "../src/DataHistory/VASpectrumHistoryModel.cpp"
#include "../src/DataHistory/VASpectrumHistoryModel.h"
#include "../src/DataHistory/VAVec3HistoryModel.cpp"
#include "../src/DataHistory/VAVec3HistoryModel.h"
//#include "../src/DataHistory/VADataHistoryModel.h"
//#include "../src/DataHistory/VADataHistoryModel.cpp"

// VA
#include <VAException.h>

// ITA
#include <ITAFileSystemUtils.h>
#include <ITAMagnitudeSpectrum.h>

// GTest
#include "gtest/gtest.h"

// STL
#include <assert.h>
#include <iostream>
#include <memory>
#include <stdio.h>


int main( int argc, char** argv )
{
	testing::InitGoogleTest( &argc, argv );
	return RUN_ALL_TESTS( );
}


//----------------------------------------
//---------PARAMETERS FOR TESTING---------
//----------------------------------------

enum EDataPolynomOrder
{
	Linear = 1,
	Quadratic,
	Cubic
};

struct HistoryBaseTestParam
{
	CVAHistoryEstimationMethod eEstimationMethod;
	EDataPolynomOrder eDataPolynomOrder;
	double dEvaluationTime;
	double dExpectedValue;
	bool bExpectSuccess;
	double dWindowSize;
	double dWindowDelay;

	std::vector<double> vdTimeStamps;
	std::vector<double> vdDataValues;

	inline HistoryBaseTestParam( CVAHistoryEstimationMethod::EMethod eMethod, EDataPolynomOrder ePolyOrder, double dEvalTime, double dExpect, bool bSuccess,
	                             double dWindowSize = 2.0, double dWindowDelay = 0.0 )
	    : eEstimationMethod( CVAHistoryEstimationMethod( eMethod ) )
	    , eDataPolynomOrder( ePolyOrder )
	    , dEvaluationTime( dEvalTime )
	    , dExpectedValue( dExpect )
	    , bExpectSuccess( bSuccess )
	    , dWindowSize( dWindowSize )
	    , dWindowDelay( dWindowDelay )
	{
		vdTimeStamps = { 0, 1, 2, 3, 4, 5 };
		vdDataValues.resize( vdTimeStamps.size( ), 1.0 );
		for( int idx = 0; idx < vdTimeStamps.size( ); idx++ )
		{
			for( int iPoly = 0; iPoly < eDataPolynomOrder; iPoly++ )
				vdDataValues[idx] *= idx;
		}
	};
	friend std::ostream& operator<<( std::ostream& os, const HistoryBaseTestParam& obj )
	{
		return os << "estimation method: " << obj.eEstimationMethod.ToString( ) << ", data polynom: " << obj.ToString( obj.eDataPolynomOrder )
		          << ", eval time: " << obj.dEvaluationTime << ", expected value: " << obj.dExpectedValue << ", success: " << obj.bExpectSuccess
		          << ", window size: " << obj.dWindowSize << ", window delay:" << obj.dWindowDelay;
		//<< " time stamps: " << obj.vdTimeStamps
		//<< " data values: " << obj.vdDataValues
	};

private:
	static std::string ToString( EDataPolynomOrder eOrder )
	{
		switch( eOrder )
		{
			case EDataPolynomOrder::Linear:
				return "Linear";
			case EDataPolynomOrder::Quadratic:
				return "Quadratic";
			case EDataPolynomOrder::Cubic:
				return "Cubic";
			default:
				return "Unknown";
		}
	}
};


//------------------------------------
//------------TEST CLASSES------------
//------------------------------------

struct GeneralHistoryTest : public testing::Test
{
	std::unique_ptr<CVADoubleHistoryModel> pHistory;
	const int iBufferSize = 10;

	inline GeneralHistoryTest( ) { pHistory = std::make_unique<CVADoubleHistoryModel>( iBufferSize ); };
	inline void AddDefaultValues( )
	{
		for( int idx = iBufferSize * iCallsAddDefaultValues++; idx < iCallsAddDefaultValues * iBufferSize; idx++ )
			pHistory->Push( idx, std::make_unique<double>( idx ) );
		pHistory->Update( );
	};
	inline void QueueHistoryData( const std::vector<double>& vdTimeStamps, const std::vector<double>& vdDataValues )
	{
		assert( vdTimeStamps.size( ) == vdDataValues.size( ) );
		for( int idx = 0; idx < vdTimeStamps.size( ); idx++ )
			pHistory->Push( vdTimeStamps[idx], std::make_unique<double>( vdDataValues[idx] ) );
	};
	inline void SetHistoryData( const std::vector<double>& vdTimeStamps, const std::vector<double>& vdDataValues )
	{
		pHistory->Reset( );
		QueueHistoryData( vdTimeStamps, vdDataValues );
		pHistory->Update( );
	};

private:
	int iCallsAddDefaultValues = 0;
};

struct HistoryLoggingTest : public testing::Test
{
	const std::string sDoubleFileBase = "TestDouble";
	const std::string sVAVec3FileBase = "TestVAVec3";

	std::unique_ptr<CVADoubleHistoryModel> pDoubleHistory;
	std::unique_ptr<CVAVec3HistoryModel> pVAVec3History;
	std::unique_ptr<CVASpectrumHistoryModel> pSpectrumHistory;
	const int iBufferSize = 10;

private:
	CVADataHistoryConfig m_oConf;

public:
	inline HistoryLoggingTest( )
	{
		m_oConf.iBufferSize       = iBufferSize;
		m_oConf.bLogInputEnabled  = true;
		m_oConf.bLogOutputEnabled = true;
	};
	inline void RunDoubleHistory( )
	{
		pDoubleHistory = std::make_unique<CVADoubleHistoryModel>( m_oConf, CVAHistoryEstimationMethod::EMethod::Linear );
		pDoubleHistory->SetLoggingBaseFilename( sDoubleFileBase );
		AddDefaultValues( );
		DoEstimations( );
		pDoubleHistory = nullptr;
	};
	inline void RunVAVec3History( )
	{
		pVAVec3History = std::make_unique<CVAVec3HistoryModel>( m_oConf, CVAHistoryEstimationMethod::EMethod::Linear );
		pVAVec3History->SetLoggingBaseFilename( sVAVec3FileBase );
		AddDefaultValues( );
		DoEstimations( );
		pVAVec3History = nullptr;
	};
	// inline void InitIllegalLoggingSpectrumHistory()
	//{
	//	pSpectrumHistory = std::make_unique<CVASpectrumHistoryModel>(m_oConf, CVAHistoryEstimationMethod::EMethod::Linear);
	//};
private:
	inline void AddDefaultValues( )
	{
		for( int idx = 0; idx < iBufferSize; idx++ )
		{
			if( pDoubleHistory )
				pDoubleHistory->Push( idx, std::make_unique<double>( idx ) );
			VAVec3 v3Data = VAVec3( idx, idx, idx );
			if( pVAVec3History )
				pVAVec3History->Push( idx, std::make_unique<VAVec3>( v3Data ) );
		}
		if( pDoubleHistory )
			pDoubleHistory->Update( );
		if( pVAVec3History )
			pVAVec3History->Update( );
	};
	inline void DoEstimations( )
	{
		VAVec3 v3Vec;
		double dResult;
		for( double dTime = 0.0; dTime < 10; dTime += 0.5 )
		{
			if( pDoubleHistory )
				pDoubleHistory->Estimate( dTime, dResult );
			if( pVAVec3History )
				pVAVec3History->Estimate( dTime, v3Vec );
		}
	};
};


struct BoolHistoryTest : public testing::Test
{
	CVABoolHistoryModel oHistory;
	const int iBufferSize;

	inline BoolHistoryTest( ) : iBufferSize( 10 ), oHistory( 10 ) { };
};


struct EstimationFromHistoryTest : testing::TestWithParam<HistoryBaseTestParam>
{
	const int iBufferSize = 10;
	const int iNumBands   = 4;
	const double dAllowedError;
	std::unique_ptr<CVADoubleHistoryModel> pDoubleHistory;
	std::unique_ptr<CVAVec3HistoryModel> pVAVec3History;
	std::unique_ptr<CVASpectrumHistoryModel> pSpectrumHistory;
	const HistoryBaseTestParam& oParams;

	inline EstimationFromHistoryTest( ) : oParams( GetParam( ) ), dAllowedError( std::abs( GetParam( ).dExpectedValue ) * 0.001 ) { };

protected:
	inline void InitDoubleHistory( )
	{
		pDoubleHistory = std::make_unique<CVADoubleHistoryModel>( iBufferSize, oParams.eEstimationMethod.Enum( ), oParams.dWindowSize, oParams.dWindowDelay );
		for( int idx = 0; idx < oParams.vdTimeStamps.size( ); idx++ )
			pDoubleHistory->Push( oParams.vdTimeStamps[idx], std::make_unique<double>( oParams.vdDataValues[idx] ) );
		pDoubleHistory->Update( );
	};
	inline void InitVAVec3History( )
	{
		pVAVec3History = std::make_unique<CVAVec3HistoryModel>( iBufferSize, oParams.eEstimationMethod.Enum( ), oParams.dWindowSize, oParams.dWindowDelay );
		for( int idx = 0; idx < oParams.vdTimeStamps.size( ); idx++ )
		{
			auto pVec3 = std::make_unique<VAVec3>( 1, 1, 1 );
			*pVec3 *= oParams.vdDataValues[idx];
			pVAVec3History->Push( oParams.vdTimeStamps[idx], std::move( pVec3 ) );
		}
		pVAVec3History->Update( );
	};
	inline void InitSpectrumHistory( )
	{
		pSpectrumHistory = std::make_unique<CVASpectrumHistoryModel>( iBufferSize, oParams.eEstimationMethod.Enum( ) );
		for( int idx = 0; idx < oParams.vdTimeStamps.size( ); idx++ )
		{
			auto pSpectrum = std::make_unique<ITABase::CGainMagnitudeSpectrum>( iNumBands );
			pSpectrum->SetIdentity( );
			pSpectrum->Multiply( oParams.vdDataValues[idx] );
			pSpectrumHistory->Push( oParams.vdTimeStamps[idx], std::move( pSpectrum ) );
		}
		pSpectrumHistory->Update( );
	};
};


//------------------------------------
//------------ACTUAL TESTS------------
//------------------------------------

TEST_F( GeneralHistoryTest, TimeMonotonyEnsured )
{
	EXPECT_ANY_THROW( pHistory->Push( -1.0, std::make_unique<double>( 1.0 ) ); );

	pHistory->Push( 1.0, std::make_unique<double>( 1.0 ) );
	EXPECT_ANY_THROW( pHistory->Push( 0.0, std::make_unique<double>( 1.0 ) ); );
	EXPECT_NO_THROW( pHistory->Update( ); );
}
TEST_F( GeneralHistoryTest, NumberOfSamplesTest )
{
	EXPECT_EQ( 0, pHistory->GetNumHistorySamples( ) );
	AddDefaultValues( ); // time = 0...9
	EXPECT_EQ( iBufferSize, pHistory->GetNumHistorySamples( ) );
	AddDefaultValues( ); // time = 10...19
	EXPECT_EQ( iBufferSize, pHistory->GetNumHistorySamples( ) );
	const double dTimeLimit = 2 * iBufferSize - 4;
	const int iLeftSamples  = 4 + 1;
	pHistory->Update( dTimeLimit ); // time = 15...19 (one in the past)
	EXPECT_EQ( iLeftSamples, pHistory->GetNumHistorySamples( ) );
	pHistory->Reset( );
	EXPECT_EQ( 0, pHistory->GetNumHistorySamples( ) );
}
TEST_F( GeneralHistoryTest, EstimationFailsTest )
{
	double dResult;
	EXPECT_FALSE( pHistory->Estimate( 0.0, dResult ) ); // false if history is empty
	pHistory->Push( 1.0, std::make_unique<double>( 1.0 ) );
	EXPECT_FALSE( pHistory->Estimate( 0.0, dResult ) ); // false, if time stamp is in past
}

TEST_F( HistoryLoggingTest, DoubleLoggingTest )
{
	const std::string sInputFile  = sDoubleFileBase + "_Input.log";
	const std::string sOutputFile = sDoubleFileBase + "_Output.log";

	std::remove( sInputFile.c_str( ) );
	std::remove( sOutputFile.c_str( ) );

	EXPECT_NO_THROW( RunDoubleHistory( ); );

	EXPECT_TRUE( doesFileExist( sInputFile ) );
	EXPECT_TRUE( doesFileExist( sOutputFile ) );
}
TEST_F( HistoryLoggingTest, VAVec3LoggingTest )
{
	const std::string sInputFile  = sVAVec3FileBase + "_Input.log";
	const std::string sOutputFile = sVAVec3FileBase + "_Output.log";

	std::remove( sInputFile.c_str( ) );
	std::remove( sOutputFile.c_str( ) );

	EXPECT_NO_THROW( RunVAVec3History( ); );

	EXPECT_TRUE( doesFileExist( sInputFile ) );
	EXPECT_TRUE( doesFileExist( sOutputFile ) );
}


TEST_F( BoolHistoryTest, SampleAndHoldTest )
{
	oHistory.Push( 0.0, std::make_unique<bool>( false ) );
	oHistory.Push( 1.0, std::make_unique<bool>( true ) );
	oHistory.Push( 2.0, std::make_unique<bool>( false ) );
	oHistory.Push( 3.0, std::make_unique<bool>( true ) );
	oHistory.Update( );

	bool bResult;

	EXPECT_FALSE( oHistory.Estimate( -1.0, bResult ) );

	EXPECT_TRUE( oHistory.Estimate( 0.0, bResult ) );
	EXPECT_FALSE( bResult );

	EXPECT_TRUE( oHistory.Estimate( 0.1, bResult ) );
	EXPECT_FALSE( bResult );

	EXPECT_TRUE( oHistory.Estimate( 0.5, bResult ) );
	EXPECT_FALSE( bResult );

	EXPECT_TRUE( oHistory.Estimate( 1.7, bResult ) );
	EXPECT_TRUE( bResult );

	EXPECT_TRUE( oHistory.Estimate( 3.0, bResult ) );
	EXPECT_TRUE( bResult );
}


TEST_P( EstimationFromHistoryTest, Double )
{
	// Init data
	EXPECT_NO_THROW( InitDoubleHistory( ) );

	double dResult;
	bool bSuccess = pDoubleHistory->Estimate( oParams.dEvaluationTime, dResult );
	EXPECT_EQ( bSuccess, oParams.bExpectSuccess );
	if( !bSuccess )
		return;
	EXPECT_NEAR( oParams.dExpectedValue, dResult, dAllowedError );
}
TEST_P( EstimationFromHistoryTest, VAVec3 )
{
	// Init data
	EXPECT_NO_THROW( InitVAVec3History( ) );

	VAVec3 v3Result;
	bool bSuccess = pVAVec3History->Estimate( oParams.dEvaluationTime, v3Result );
	EXPECT_EQ( bSuccess, oParams.bExpectSuccess );
	if( !bSuccess )
		return;
	EXPECT_NEAR( oParams.dExpectedValue, v3Result.x, dAllowedError );
	EXPECT_NEAR( oParams.dExpectedValue, v3Result.y, dAllowedError );
	EXPECT_NEAR( oParams.dExpectedValue, v3Result.z, dAllowedError );
}
TEST_P( EstimationFromHistoryTest, ITASpectrum )
{
	// Init data
	bool bSpectrumInterpolationAllowed =
	    oParams.eEstimationMethod.IsInterpolation( ) && oParams.eEstimationMethod.Enum( ) != CVAHistoryEstimationMethod::EMethod::CubicSpline;
	if( bSpectrumInterpolationAllowed )
		EXPECT_NO_THROW( InitSpectrumHistory( ) );
	else
	{
		EXPECT_ANY_THROW( InitSpectrumHistory( ) );
		return;
	}

	auto oResultSpectrum = ITABase::CGainMagnitudeSpectrum( iNumBands );
	bool bSuccess        = pSpectrumHistory->Estimate( oParams.dEvaluationTime, oResultSpectrum );
	EXPECT_EQ( bSuccess, oParams.bExpectSuccess );
	if( !bSuccess )
		return;
	for( int idx = 0; idx < iNumBands; idx++ )
		EXPECT_NEAR( oParams.dExpectedValue, oResultSpectrum[idx], dAllowedError );
}
INSTANTIATE_TEST_CASE_P( SampleAndHold, EstimationFromHistoryTest,
                         testing::Values( HistoryBaseTestParam( CVAHistoryEstimationMethod::EMethod::SampleAndHold, EDataPolynomOrder::Linear, -1.0, 0.0, false ),
                                          HistoryBaseTestParam( CVAHistoryEstimationMethod::EMethod::SampleAndHold, EDataPolynomOrder::Linear, 0.0, 0.0, true ),
                                          HistoryBaseTestParam( CVAHistoryEstimationMethod::EMethod::SampleAndHold, EDataPolynomOrder::Linear, 1.1, 1.0, true ),
                                          HistoryBaseTestParam( CVAHistoryEstimationMethod::EMethod::SampleAndHold, EDataPolynomOrder::Linear, 2.5, 2.0, true ),
                                          HistoryBaseTestParam( CVAHistoryEstimationMethod::EMethod::SampleAndHold, EDataPolynomOrder::Linear, 3.7, 3.0, true ) ) );
INSTANTIATE_TEST_CASE_P( NearestNeighbor, EstimationFromHistoryTest,
                         testing::Values( HistoryBaseTestParam( CVAHistoryEstimationMethod::EMethod::NearestNeighbor, EDataPolynomOrder::Quadratic, 0.0, 0.0, true ),
                                          HistoryBaseTestParam( CVAHistoryEstimationMethod::EMethod::NearestNeighbor, EDataPolynomOrder::Quadratic, 5.0, 25.0, true ),
                                          HistoryBaseTestParam( CVAHistoryEstimationMethod::EMethod::NearestNeighbor, EDataPolynomOrder::Quadratic, 3.5, 16.0, true ),
                                          HistoryBaseTestParam( CVAHistoryEstimationMethod::EMethod::NearestNeighbor, EDataPolynomOrder::Quadratic, -1.0, 0.0, false ),
                                          HistoryBaseTestParam( CVAHistoryEstimationMethod::EMethod::NearestNeighbor, EDataPolynomOrder::Quadratic, 10.0, 25.0,
                                                                true ) ) );
INSTANTIATE_TEST_CASE_P( Linear, EstimationFromHistoryTest,
                         testing::Values( HistoryBaseTestParam( CVAHistoryEstimationMethod::EMethod::Linear, EDataPolynomOrder::Quadratic, 0.0, 0.0, true ),
                                          HistoryBaseTestParam( CVAHistoryEstimationMethod::EMethod::Linear, EDataPolynomOrder::Quadratic, 5.0, 25.0, true ),
                                          HistoryBaseTestParam( CVAHistoryEstimationMethod::EMethod::Linear, EDataPolynomOrder::Quadratic, 3.5, 12.5, true ),
                                          HistoryBaseTestParam( CVAHistoryEstimationMethod::EMethod::Linear, EDataPolynomOrder::Quadratic, -1.0, 0.0, false ),
                                          HistoryBaseTestParam( CVAHistoryEstimationMethod::EMethod::Linear, EDataPolynomOrder::Quadratic, 10.0, 25.0, true ) ) );
INSTANTIATE_TEST_CASE_P( CubicSpline, EstimationFromHistoryTest,
                         testing::Values( HistoryBaseTestParam( CVAHistoryEstimationMethod::EMethod::CubicSpline, EDataPolynomOrder::Quadratic, 0.0, 0.0, true ),
                                          HistoryBaseTestParam( CVAHistoryEstimationMethod::EMethod::CubicSpline, EDataPolynomOrder::Quadratic, 5.0, 25.0, true ),
                                          HistoryBaseTestParam( CVAHistoryEstimationMethod::EMethod::CubicSpline, EDataPolynomOrder::Quadratic, 3.5, 12.2, true ),
                                          HistoryBaseTestParam( CVAHistoryEstimationMethod::EMethod::CubicSpline, EDataPolynomOrder::Quadratic, -1.0, 0.0, false ),
                                          HistoryBaseTestParam( CVAHistoryEstimationMethod::EMethod::CubicSpline, EDataPolynomOrder::Quadratic, 10.0, 25.0, true ),
                                          HistoryBaseTestParam( CVAHistoryEstimationMethod::EMethod::CubicSpline, EDataPolynomOrder::Cubic, 3.0, 27.0, true ),
                                          HistoryBaseTestParam( CVAHistoryEstimationMethod::EMethod::CubicSpline, EDataPolynomOrder::Cubic, 2.5, 15.25, true ) ) );
INSTANTIATE_TEST_CASE_P( TriangularWindow, EstimationFromHistoryTest,
                         testing::Values( HistoryBaseTestParam( CVAHistoryEstimationMethod::EMethod::TriangularWindow, EDataPolynomOrder::Quadratic, 2.5, 7.0, true, 2.0,
                                                                0.0 ) ) );
