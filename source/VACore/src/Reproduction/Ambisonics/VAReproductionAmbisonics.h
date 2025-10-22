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

#ifndef IW_VACORE_AMBISONICSREPRODUCTION
#define IW_VACORE_AMBISONICSREPRODUCTION

#ifdef VACORE_WITH_REPRODUCTION_AMBISONICS

// VA includes
#	include "../VAAudioReproduction.h"
#	include "../../Scene/VASceneState.h"
#	include "../../VAHardwareSetup.h"

#	include <VABase.h>

// ITA includes
#	include <ITABase/Math/Triangulation.h>
#	include <ITADataSourceRealization.h>
#	include <ITAStreamPatchbay.h>

// Vista includes
#	include <VistaBase/VistaVector3D.h>

// Third party includes
#	include "Eigen\Core"

// STL includes
#	include <string>
#	include <vector>

using namespace ITABase::Math;

class CVAAmbisonicsReproduction : public IVAAudioReproduction
{
public:
	CVAAmbisonicsReproduction( const CVAAudioReproductionInitParams& oParams );
	~CVAAmbisonicsReproduction( );

	void SetInputDatasource( ITADatasource* );
	ITADatasource* GetOutputDatasource( );
	void UpdateScene( CVASceneState* pNewState );

	int GetNumInputChannels( ) const;

private:
	struct SReproductionConfig
	{
		std::vector<std::string> m_vsOutputs, m_vsCenterPositions, m_vsVirtualOutputs, m_vsTriangulationFiles;
	};

	struct SOutputConfig
	{
		const CVAHardwareOutput* m_pOutput;
		VAVec3 m_vReproductioncenterPos;
		const CVAHardwareOutput* m_pVirtualOutput;
		CTriangulation* m_pTriangulation;
		int iReproductionOrder;
	};

	// Methods
	void InitOutputConfigs( );
	const CVAHardwareOutput* GetHardwareOutput( std::string& sOutputName, bool bVirtual = false );
	void GetReproductionCenterPos( const std::string& sReproductionCenterPos, const CVAHardwareOutput* pOutput, VAVec3& vec3CalcPos );
	void ReadTriangulationFromFile( const std::string& sTriangulationFile, std::vector<VistaVector3D>& vvTriangulationIndices );

	void CalculateDecoderMatrix( const struct SOutputConfig& sOutputConfig, Eigen::MatrixXd& matDecoder );

	// Members
	CVAAudioReproductionInitParams m_oParams;
	ITAStreamPatchbay* m_pDecoderMatrixPatchBay;

	SReproductionConfig m_cReproductionConfig;
	std::vector<SOutputConfig> m_vpOutputConfigs;

	int m_iReproductionTruncationOrder;
	std::string m_sDecoder;
	bool m_bIsAllRAD = false;
	bool m_bUseRemax;
	bool m_bUseNFC;
};

#endif // VACORE_WITH_REPRODUCTION_AMBISONICS

#endif // IW_VACORE_AMBISONICSREPRODUCTION
