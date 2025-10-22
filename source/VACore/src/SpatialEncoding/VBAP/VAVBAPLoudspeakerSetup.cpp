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

#include "VAVBAPLoudspeakerSetup.h"

#include "../../VAHardwareSetup.h"
#include "../../VALog.h"
#include "../../Utils/VAUtils.h"

#include <VAException.h>

#include <ITAStringUtils.h>
#include <ITABase/Math/Triangulation.h>
#include <ITASampleBuffer.h>
#include <ITASampleFrame.h>
#include <math.h>

#include <VistaTools/VistaIniFileParser.h>
#include <VistaBase/VistaVector3D.h>

#include <streambuf>
#include <fstream>


std::vector<VistaVector3D> ReadTriangulationFromFile( const std::string& sTriangulationFile )
{
	std::vector<VistaVector3D> vvTriangulationIndices;

	//TODO: Get rid of Vista functions and use std instead. Also remove includes above.
	VistaPropertyList vplTriangulation;
	VistaIniFileParser::ReadProplistFromFile( sTriangulationFile, vplTriangulation );
	std::string sTriangulation = vplTriangulation( "Triangulation" ).GetValue( );

	//TODO: This might be a way to do it. However, I am not sure whether the "Triangulation" header might become a problem
	//std::ifstream fStream( sTriangulationFile );
	//std::string sTriangulation2( ( std::istreambuf_iterator<char>( fStream ) ), std::istreambuf_iterator<char>( ) );

	std::vector<std::string> vsTriangulation = splitString( sTriangulation, ";" );
	for( int m = 0; m < vsTriangulation.size( ); m++ )
	{
		std::vector<std::string> vsTriangle = splitString( vsTriangulation[m], "," );
		vvTriangulationIndices.push_back( VistaVector3D( StringToInt( vsTriangle[0] ), StringToInt( vsTriangle[1] ), StringToInt( vsTriangle[2] ) ) );
	}

	return vvTriangulationIndices;
}



CVAVBAPLoudspeakerSetup::CVAVBAPLoudspeakerSetup( const CVAHardwareOutput& oHardwareOutput, const VAVec3& v3LSSetupCenter, const std::string& sTriangulationFile )
    : m_oHardwareOutput( oHardwareOutput )
    , m_vpLoudspeakers( oHardwareOutput.vpDevices )
    , m_v3LSSetupCenter( v3LSSetupCenter )
{
	std::vector<VistaVector3D> vv3LoudspeakerPositions;
	for( int idxLS = 0; idxLS < m_vpLoudspeakers.size( ); idxLS++ )
	{
		const CVAHardwareDevice* pDevice = m_vpLoudspeakers[idxLS];
		m_vdLSDistanceToCenter.push_back( ( pDevice->vPos - m_v3LSSetupCenter ).Length( ) );
		vv3LoudspeakerPositions.push_back( VAVec3ToVistaVector3D( pDevice->vPos ) );
	}
	std::vector<VistaVector3D> vvTriangulationIndices = ReadTriangulationFromFile( sTriangulationFile );
	m_pTriangulation = std::make_shared<ITABase::Math::CTriangulation>( vv3LoudspeakerPositions, vvTriangulationIndices, VAVec3ToVistaVector3D( m_v3LSSetupCenter ) );
}
