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

#ifndef IW_VACORE_BOOL_HISTORY_MODEL
#define IW_VACORE_BOOL_HISTORY_MODEL

#include "VADataHistoryConfig.h"
#include "VADataHistoryModel_impl.h" //Since this is a template class, we need to include the implemenatation (.h is included also implicitly)


//! Class for history of filter properties such as audibility switches. Always uses sample and hold method.
class CVABoolHistoryModel : public CVADataHistoryModel<bool>
{
public:
	inline CVABoolHistoryModel( int iBufferSize ) : CVADataHistoryModel( iBufferSize, CVAHistoryEstimationMethod::EMethod::SampleAndHold ) { };

	inline bool Estimate( const double& dTime, bool& bOut ) override { return SampleAndHold( dTime, bOut ); };
};

#endif // IW_VACORE_BOOL_HISTORY_MODEL
