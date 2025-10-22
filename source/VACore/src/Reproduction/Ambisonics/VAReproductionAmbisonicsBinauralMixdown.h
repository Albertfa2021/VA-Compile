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

#ifndef IW_VACORE_AMBISONICSBINAURALMIXDOWNREPRODUCTION
#define IW_VACORE_AMBISONICSBINAURALMIXDOWNREPRODUCTION

#ifdef VACORE_WITH_REPRODUCTION_AMBISONICS_BINAURAL_MIXDOWN

#	include "../../core/core.h"
#	include "../VAAudioReproduction.h"
#	include "../VAAudioReproductionRegistry.h"

#	include <ITASampleFrame.h>

// 3rd party
#	include <Eigen\Jacobi>
#	include <Eigen\SVD>
#	include <Eigen\dense>

// STL
#	include <atomic>

class ITADatasource;
class ITAStreamPatchbay;
class CMixdownStreamFilter;
class CVADirectivityDAFFHRIR;
class CVABasicMotionModel;

class CVAAmbisonicsBinauralMixdownReproduction : public IVAAudioReproduction
{
public:
	CVAAmbisonicsBinauralMixdownReproduction( const CVAAudioReproductionInitParams& oParams );
	~CVAAmbisonicsBinauralMixdownReproduction( );

	void SetInputDatasource( ITADatasource* );
	ITADatasource* GetOutputDatasource( );
	int GetNumInputChannels( ) const;
	void SetParameters( const CVAStruct& oParams );
	CVAStruct GetParameters( const CVAStruct& ) const;

	int GetAmbisonicsTruncationOrder( ) const;

	//! Returns number of virtual loudspeaker
	int GetNumVirtualLoudspeaker( ) const;

	//! Sets the active listener of this reproduction module
	/**
	 * Information on virtual position of listener is used
	 * for binaural downmix with related HRIR.
	 */
	void SetTrackedListener( const int iListenerID );
	Eigen::MatrixXd CalculatePseudoInverse( Eigen::MatrixXd );
	void UpdateScene( CVASceneState* pNewState );
	void GetCalculatedReproductionCenterPos( VAVec3& vec3CalcPos );

private:
	std::string m_sName;
	CVAAudioReproductionInitParams m_oParams;
	bool m_bBFormatIsInit;
	int m_iHRIRFilterLength;
	int m_iAmbisonicsTruncationOrder;
	std::string m_sRotationMode;
	Eigen::MatrixXd m_matYinv;
	std::vector<double> m_vdRemaxWeights;
	Eigen::MatrixXd m_orderMatrices[5];

	VAVec3 m_v3ReproductionCenterPos;
	std::atomic<double> m_dTrackingDelaySeconds;
	std::vector<const CVAHardwareOutput*> m_vpTargetOutputs;
	const CVAHardwareOutput* m_pVirtualOutput;
	CVABasicMotionModel* m_pMotionModel;

	int m_iListenerID;


	const CVADirectivityDAFFHRIR* m_pDefaultHRIR;

	ITASampleFrame m_sfHRIRTemp;
	CMixdownStreamFilter* m_pdsStreamFilter;
	ITAStreamPatchbay* m_pDecoderMatrixPatchBay;

	std::vector<int> m_viLastHRIRIndex;
};


#endif // VACORE_WITH_REPRODUCTION_AMBISONICS_BINAURAL_MIXDOWN

#endif // IW_VACORE_AMBISONICSBINAURALMIXDOWNREPRODUCTION
