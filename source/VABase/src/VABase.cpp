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

#include <VABase.h>
#include <iomanip>
#include <iostream>

std::ostream& operator<<( std::ostream& os, const VAVec3& oVec )
{
	return os << std::fixed << std::setprecision( 3 ) << "< " << oVec.x << ", " << oVec.y << ", " << oVec.z << " >";
}

std::ostream& operator<<( std::ostream& os, const VAQuat& oOrient )
{
	return os << std::fixed << std::setprecision( 2 ) << "< " << oOrient.x << ", " << oOrient.y << ", " << oOrient.z << ", '" << oOrient.w << "' >";
}
