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

#ifndef IW_VACORE_VEC3_HISTORY_MODEL
#define IW_VACORE_VEC3_HISTORY_MODEL

#include "VADataHistoryConfig.h"
#include "VADataHistoryModel_impl.h" //Since this is a template class, we need to include the implemenatation (.h is included also implicitly)

#include <VABase.h>

// ITA
#include <ITADataLog.h>

//! Class for history of filter properties such as gains
class CVAVec3HistoryModel : public CVADataHistoryModel<VAVec3>
{
public:
	inline CVAVec3HistoryModel( const CVADataHistoryConfig& oConf, CVAHistoryEstimationMethod::EMethod eMethod = CVAHistoryEstimationMethod::EMethod::NearestNeighbor )
	    : CVADataHistoryModel( oConf, eMethod ) { };
	inline CVAVec3HistoryModel( int iBufferSize, CVAHistoryEstimationMethod::EMethod eMethod = CVAHistoryEstimationMethod::EMethod::NearestNeighbor,
	                            double dWindowSize = VADataHistory_DefaultWindowSize, double dWindowDelay = VADataHistory_DefaultWindowDelay )
	    : CVADataHistoryModel( iBufferSize, eMethod, dWindowSize, dWindowDelay ) { };

	bool Estimate( const double& dTime, VAVec3& dOut ) override;

private:
	bool CubicSplineInterpolation( const double& dTime, VAVec3& dOut );


	// --- LOGGING ---
	// ---------------
public:
	void SetLoggingBaseFilename( const std::string& sBaseFilename );

private:
	virtual void LogInputData( const double& dTime, const VAVec3& oData ) override;
	virtual void LogOutputData( const double& dTime, const VAVec3& oData ) override;

	class CLogData : public ITALogDataBase
	{
	public:
		static std::ostream& outputDesc( std::ostream& os );
		std::ostream& outputData( std::ostream& os ) const override;
		double dTime; //!< Time [s]
		VAVec3 oData; //!< Vector
	};

	ITABufferedDataLogger<CLogData> m_oInputDataLog;  //!< Logger for input history data
	ITABufferedDataLogger<CLogData> m_oOutputDataLog; //!< Logger for estimated data (output)
};

#endif // IW_VACORE_VEC3_HISTORY_MODEL
