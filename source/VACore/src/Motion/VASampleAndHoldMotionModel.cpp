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

#include "VASampleAndHoldMotionModel.h"

#include "../Scene/VAMotionState.h"

#ifdef VA_MOTIONMODEL_DATA_LOG_ESTIMATED

std::ostream& CVASampleAndHoldMotionModel::MotionLogDataOutput::outputDesc( std::ostream& os ) const
{
	os << "Estimation time"
	   << "\t"
	   << "Estimated Position [x]"
	   << "\t"
	   << "Estimated Position [y]"
	   << "\t"
	   << "Estimated Position [z]" << std::endl;
	return os;
}

std::ostream& CVASampleAndHoldMotionModel::MotionLogDataOutput::outputData( std::ostream& os ) const
{
	os.precision( 12 );
	os << dTime << "\t" << vPos.x << "\t" << vPos.y << "\t" << vPos.z << std::endl;
	return os;
}

#endif

#ifdef VA_MOTIONMODEL_DATA_LOG_INPUT
std::ostream& CVASampleAndHoldMotionModel::MotionLogDataInput::outputDesc( std::ostream& os ) const
{
	os << "Input time"
	   << "\t"
	   << "Input Position [x]"
	   << "\t"
	   << "Input Position [y]"
	   << "\t"
	   << "Input Position [z]" << std::endl;
	return os;
}

std::ostream& CVASampleAndHoldMotionModel::MotionLogDataInput::outputData( std::ostream& os ) const
{
	os.precision( 12 );
	os << dTime << "\t" << vPos.x << "\t" << vPos.y << "\t" << vPos.z << std::endl;
	return os;
}

#endif
