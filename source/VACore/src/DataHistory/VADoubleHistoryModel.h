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

#ifndef IW_VACORE_DOUBLE_HISTORY_MODEL
#define IW_VACORE_DOUBLE_HISTORY_MODEL

#include "VADataHistoryConfig.h"
#include "VADataHistoryModel_impl.h" //Since this is a template class, we need to include the implemenatation (.h is included also implicitly)

// ITA
#include <ITADataLog.h>

//! Class for history of filter properties such as gains
class CVADoubleHistoryModel : public CVADataHistoryModel<double>
{
public:
	inline CVADoubleHistoryModel( CVADataHistoryConfig& oConf, CVAHistoryEstimationMethod::EMethod eMethod = CVAHistoryEstimationMethod::EMethod::NearestNeighbor )
	    : CVADataHistoryModel( oConf, eMethod ) { };
	inline CVADoubleHistoryModel( int iBufferSize, CVAHistoryEstimationMethod::EMethod eMethod = CVAHistoryEstimationMethod::EMethod::NearestNeighbor,
	                              double dWindowSize = VADataHistory_DefaultWindowSize, double dWindowDelay = VADataHistory_DefaultWindowDelay )
	    : CVADataHistoryModel( iBufferSize, eMethod, dWindowSize, dWindowDelay ) { };

	bool Estimate( const double& dTime, double& dOut ) override;

private:
	bool CubicSplineInterpolation( const double& dTime, double& dOut );


	// --- LOGGING ---
	// ---------------
public:
	void SetLoggingBaseFilename( const std::string& sBaseFilename );

private:
	virtual void LogInputData( const double& dTime, const double& oData ) override;
	virtual void LogOutputData( const double& dTime, const double& oData ) override;

	class CLogData : public ITALogDataBase
	{
	public:
		static std::ostream& outputDesc( std::ostream& os );
		std::ostream& outputData( std::ostream& os ) const override;
		double dTime; //!< Time [s]
		double oData; //!< Double value
	};

	ITABufferedDataLogger<CLogData> m_oInputDataLog;  //!< Logger for input history data
	ITABufferedDataLogger<CLogData> m_oOutputDataLog; //!< Logger for estimated data (output)
};

#endif // IW_VACORE_DOUBLE_HISTORY_MODEL
