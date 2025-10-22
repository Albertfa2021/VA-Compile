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

#include "VAReproductionCTC4.h"

#ifdef VACORE_REPRODUCTION_BINAURAL_CTC4

#	include "VAHRIRDataset.h"
#	include "VAHRIRDatasetDAFF2D.h"

#	include <DAFF.h>
#	include <ITADataSource.h>
#	include <ITAQuadCTC.h>
#	include <Scene/VAMotionState.h>
#	include <Scene/VASceneState.h>
#	include <Scene/VASoundReceiverState.h>
#	include <Utils/VAUtils.h>
#	include <VACoreConfig.h>
#	include <VACoreImpl.h>
#	include <VAException.h>
#	include <VAHardwareSetup.h>
#	include <cassert>

CVACTC4Reproduction::CVACTC4Reproduction( const CVAAudioReproductionInitParams& oParams ) : m_oParams( oParams ), m_pCTC( NULL ), m_pHRIR( NULL ), m_iListenerID( -1 )
{
	/*m_params.sID = "CTC4";
	m_params.sName = "CTC4";
	m_params.sDesc = "Four-channel dynamic crosstalk-cancellation";
	m_params.iNumOutputChannels = 4;
	m_params.bTracked = true;*/

	CVAConfigInterpreter conf( *( m_oParams.pConfig ) );
	double dSampleRate = m_oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.dSampleRate;
	int iBlockLength   = m_oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.iBuffersize;

	// Validate outputs
	for( size_t i = 0; i < m_oParams.vpOutputs.size( ); i++ )
	{
		const CVAHardwareOutput* pTargetOutput = m_oParams.vpOutputs[i];
		if( pTargetOutput == nullptr )
			VA_EXCEPT2( INVALID_PARAMETER, "Unrecognized output '" + pTargetOutput->sIdentifier + "' for binaural mixdown reproduction" );
		if( pTargetOutput->GetPhysicalOutputChannels( ).size( ) != 2 )
			VA_EXCEPT2( INVALID_PARAMETER, "Expecting two channels for binaural downmix target, problem with output '" + pTargetOutput->sIdentifier + "' detected" );
		m_vpTargetOutputs.push_back( pTargetOutput );
	}

	std::string sHRIRDatasetFile;
	conf.ReqString( "HRIRDatasetFile", sHRIRDatasetFile );
	sHRIRDatasetFile = m_oParams.pCore->GetCoreConfig( )->mMacros.SubstituteMacros( sHRIRDatasetFile );

	// HRIR-Datenbank laden
	m_pHRIR = new CVADirectivityDAFFHRIR( sHRIRDatasetFile, "CTC4ReproHRIR", dSampleRate );

	// CTC-Engine aufsetzen
	std::string sCTC4ConfigFile;
	conf.ReqString( "CTC4ConfigFile", sCTC4ConfigFile );
	sCTC4ConfigFile = m_oParams.pCore->GetCoreConfig( )->mMacros.SubstituteMacros( sCTC4ConfigFile );
	m_pCTC          = new ITAQuadCTC( dSampleRate, iBlockLength, sCTC4ConfigFile, m_pHRIR->GetDAFFContent( ) );
}

CVACTC4Reproduction::~CVACTC4Reproduction( )
{
	delete m_pCTC;
	m_pCTC = NULL;

	delete m_pHRIR;
	m_pHRIR = NULL;
}

void CVACTC4Reproduction::SetInputDatasource( ITADatasource* pdsInput )
{
	m_pCTC->setInputDatasource( pdsInput );
}

ITADatasource* CVACTC4Reproduction::GetOutputDatasource( )
{
	return m_pCTC->getOutputDatasource( );
}

std::vector<const CVAHardwareOutput*> CVACTC4Reproduction::GetTargetOutputs( ) const
{
	return m_vpTargetOutputs;
}


void CVACTC4Reproduction::UpdateScene( CVASceneState* pNewState )
{
	// @todo: get real world position and HRIR from listener

	if( m_iListenerID == -1 )
		return;

	CVAReceiverState* pLIstenerState( pNewState->GetReceiverState( m_iListenerID ) );
	if( pLIstenerState == nullptr )
		return;

	IVADirectivity* pHRIRSet = pLIstenerState->GetDirectivity( );

	if( pHRIRSet == nullptr )
		return;

	double x, y, z, vx, vy, vz, ux, uy, uz;
	m_oParams.pCore->GetListenerRealWorldHeadPositionOrientationVU( m_iListenerID, x, y, z, vx, vy, vz, ux, uy, uz );
	m_pCTC->updateHeadPosition( x, y, z, ux, uy, uz, vx, vy, vz );
}

int CVACTC4Reproduction::GetNumInputChannels( ) const
{
	return 2;
}


/*
 *  Legacy CTC by Tobias Lentz
 */

#	if( VACORE_TLE_CTC == 1 )

#		include <ITADualCTC-TLE.h>

CVACTC4ReproductionTLE::CVACTC4ReproductionTLE( ) : m_pCTC( NULL )
{
	m_params.sID                = "CTC4-TLE";
	m_params.sName              = "CTC4-TLE";
	m_params.sDesc              = "Four-channel dynamic crosstalk-cancellation";
	m_params.iNumOutputChannels = 4;
	m_params.bTracked           = true;
}

CVACTC4ReproductionTLE::~CVACTC4ReproductionTLE( ) {}

void CVACTC4ReproductionTLE::Initialize( const CVACoreConfig* pCoreConfig, ITADatasource* pdsInput )
{
	assert( pCoreConfig );
	assert( pdsInput );

	std::string sHRIRDataset    = pCoreConfig->mMacros.SubstituteMacros( pCoreConfig->sCTC4HRIRDatasetFile );
	std::string sCTC4ConfigFile = pCoreConfig->mMacros.SubstituteMacros( pCoreConfig->sCTC4ConfigFile );
	m_pCTC                      = new ITADualCTC_TLE( pdsInput, sHRIRDataset, sCTC4ConfigFile );
}

void CVACTC4ReproductionTLE::Finalize( )
{
	delete m_pCTC;
	m_pCTC = NULL;
}

const CVAReproductionModuleParams& CVACTC4ReproductionTLE::GetParams( ) const
{
	return m_params;
}

ITADatasource* CVACTC4ReproductionTLE::GetOutputStreamingDatasource( ) const
{
	return m_pCTC;
}

void CVACTC4ReproductionTLE::updateHeadPosition( double px, double py, double pz, double vx, double vy, double vz, double ux, double uy, double uz )
{
	RG_Vector vPos( px, py, pz ), vView( vx, vy, vz ), vUp( ux, uy, uz );
	m_pCTC->updateHeadPosition( vPos, vView, vUp );
}

#	endif // VACORE_REPRODUCTION_BINAURAL_CTC4

#endif // (VACORE_TLE_CTC==1)
