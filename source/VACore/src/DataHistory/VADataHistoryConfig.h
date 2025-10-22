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


#ifndef IW_VACORE_DATA_HISTORY_ESTIMATION_METHOD
#define IW_VACORE_DATA_HISTORY_ESTIMATION_METHOD

// VA
#include <VAException.h>

// STL
#include <string.h>

#define VADataHistory_DefaultWindowSize  0.2
#define VADataHistory_DefaultWindowDelay 0.1

//! Class representing the estimation methods for the VA history models. Basically this extends an enum with a small set of methods.
class CVAHistoryEstimationMethod
{
public:
	//! Enum defining estimation method for the VA history models
	enum class EMethod
	{
		// Interpolation methods < 10

		SampleAndHold = 0, // Sample and hold
		NearestNeighbor,   // Nearest neighbor
		Linear,            // Linear interpolation
		CubicSpline,       // Cubic spline interpolation

		// Sliding windows start at 10

		TriangularWindow = 10 // Triangular sliding window
	};

private:
	EMethod m_eMethod;

public:
	inline CVAHistoryEstimationMethod( EMethod method ) : m_eMethod( method ) { };

	//! \brief Returns the actual enum
	inline EMethod Enum( ) const { return m_eMethod; };


	inline bool IsSlidingWindow( ) const { return (int)m_eMethod >= 10; };
	inline bool IsInterpolation( ) const { return !IsSlidingWindow( ); };

	//! \brief Returns the minimum number of samples required to perform estimation method
	inline int GetNumRequiredSamples( ) const
	{
		switch( m_eMethod )
		{
			case EMethod::SampleAndHold:
				return 1;
			case EMethod::NearestNeighbor:
				return 1;
			case EMethod::Linear:
				return 2;
			case EMethod::CubicSpline:
				return 4;
			case EMethod::TriangularWindow:
				return 1;
			default:
				VA_EXCEPT2( INVALID_PARAMETER, "[CVAHistoryEstimationMethod::GetNumRequiredSamples()]: Unknown estimation method." );
		}
	};

	inline std::string ToString( ) const
	{
		switch( m_eMethod )
		{
			case EMethod::SampleAndHold:
				return "SampleAndHold";
			case EMethod::NearestNeighbor:
				return "NearestNeighbor";
			case EMethod::Linear:
				return "Linear";
			case EMethod::CubicSpline:
				return "CubicSpline";
			case EMethod::TriangularWindow:
				return "TriangularWindow";
			default:
				return "Unknown";
		}
	};
};

struct CVADataHistoryConfig
{
	int iBufferSize = 1000; //!< Number of keys to be stored (size of ring buffer)

	double dWindowSize  = VADataHistory_DefaultWindowSize;  //!< Size of weighting window [s]
	double dWindowDelay = VADataHistory_DefaultWindowDelay; //!< Delay of weighting window [s]

	bool bLogInputEnabled  = false; //!< Input data stream logging enabled
	bool bLogOutputEnabled = false; //!< Output data stream logging enabled
};

#endif // IW_VACORE_DATA_HISTORY_ESTIMATION_METHOD