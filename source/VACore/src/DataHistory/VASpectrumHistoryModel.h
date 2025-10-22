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

#ifndef IW_VACORE_SPECTRUM_HISTORY_MODEL
#define IW_VACORE_SPECTRUM_HISTORY_MODEL

#include "VADataHistoryConfig.h"
#include "VADataHistoryModel_impl.h" //Since this is a template class, we need to include the implemenatation (.h is included also implicitly)

// ITABase
#include <ITASpectrum.h>

//! Class for history of filter properties such as gains
class CVASpectrumHistoryModel : public CVADataHistoryModel<CITASpectrum>
{
public:
	CVASpectrumHistoryModel( int iBufferSize, CVAHistoryEstimationMethod::EMethod eMethod = CVAHistoryEstimationMethod::EMethod::NearestNeighbor );

	bool Estimate( const double& dTime, CITASpectrum& oOut ) override;
};

#endif // IW_VACORE_SPECTRUM_HISTORY_MODEL
