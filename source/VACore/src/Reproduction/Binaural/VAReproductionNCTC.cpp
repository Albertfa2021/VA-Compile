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

#include "VAReproductionNCTC.h"

#if VACORE_WITH_REPRODUCTION_BINAURAL_NCTC

#	include "../../Scene/VAMotionState.h"
#	include "../../Scene/VASceneState.h"
#	include "../../Scene/VASoundReceiverState.h"
#	include "../../Utils/VAUtils.h"
#	include "../../VAHardwareSetup.h"
#	include "../../VALog.h"
#	include "../../directivities/VADirectivityDAFFHRIR.h"

#	include <DAFF.h>
#	include <ITAFFTUtils.h>
#	include <ITAFastMath.h>
#	include <ITAFileSystemUtils.h>
#	include <ITANCTC.h>
#	include <ITANCTCStreamFilter.h>
#	include <ITAUPConvolution.h>
#	include <VistaBase/VistaQuaternion.h>
#	include <VistaBase/VistaTransformMatrix.h>
#	include <VistaBase/VistaVector3D.h>


CVANCTCReproduction::CVANCTCReproduction( const CVAAudioReproductionInitParams& oParams )
    : m_oParams( oParams )
    , m_pdsStreamFilter( NULL )
    , m_iListenerID( -1 )
    , m_dGain( 1.0f )
    , m_bTrackedListenerHRIR( false )
    , m_pDefaultHRIR( NULL )
{
	CVAConfigInterpreter conf( *( m_oParams.pConfig ) );

	conf.OptInteger( "TrackedListenerID", m_iListenerID, 1 );

	// Validate outputs
	if( m_oParams.vpOutputs.size( ) != 1 )
		VA_EXCEPT2( INVALID_PARAMETER, "Only one output target per NCTC reproduction instance allowed" );

	// NCTC configuration
	ITANCTC::Config oNCTCConf;
	conf.OptInteger( "CTCFilterLength", oNCTCConf.iCTCFilterLength, 4096 );
	oNCTCConf.fSampleRate = float( m_oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.dSampleRate );
	double dSpeedOfSound; // @todo jst: take from core homogeneous medium
	conf.OptNumber( "SpeedOfSound", dSpeedOfSound, 340.0f );
	oNCTCConf.fSpeedOfSound = float( dSpeedOfSound );

	std::string sOptimization;
	conf.OptString( "Optimization", sOptimization );
	// ...
	oNCTCConf.iOptimization = ITANCTC::Config::OPTIMIZATION_NONE;

	oNCTCConf.N = int( m_oParams.vpOutputs[0]->GetPhysicalOutputChannels( ).size( ) );

	// Loudspeaker
	oNCTCConf.voLoudspeaker.resize( oNCTCConf.N );
	for( int i = 0; i < oNCTCConf.N; i++ )
	{
		ITANCTC::Config::Loudspeaker& oLS( oNCTCConf.voLoudspeaker[i] );
		const CVAHardwareDevice* pLSOut( m_oParams.vpOutputs[0]->vpDevices[i] );
		oLS.oPose.vPos.SetValues( pLSOut->vPos.x, pLSOut->vPos.y, pLSOut->vPos.z );

		VAVec3 v3View, v3Up;
		ConvertQuaternionToViewUp( pLSOut->qOrient, v3View, v3Up );
		oLS.oPose.vView.SetValues( v3View.x, v3View.y, v3View.z );
		oLS.oPose.vUp.SetValues( v3Up.x, v3Up.y, v3Up.z );

		if( pLSOut->sDataFileName.empty( ) == false )
		{
			std::string sDataFilePath = m_oParams.pCore->GetCoreConfig( )->mMacros.SubstituteMacros( pLSOut->sDataFileName );

			DAFFReader* pReader = DAFFReader::create( );
			if( pReader->openFile( correctPath( sDataFilePath ) ) == 0 )
			{
				DAFFContentDFT* pLSEQ = dynamic_cast<DAFFContentDFT*>( pReader->getContent( ) );

				if( pLSEQ == nullptr )
					VA_EXCEPT2( INVALID_PARAMETER, "Could not read DAFF content of loudspeaker " + pLSOut->sIdentifier );

				if( pLSEQ->getNumDFTCoeffs( ) != ( oNCTCConf.iCTCFilterLength / 2 + 1 ) )
					VA_EXCEPT2( INVALID_PARAMETER, "Invalid DFT size of this loudspeaker, unable to use the data for NCTC" );

				oLS.pDirectivity = pLSEQ;
			}
			else
			{
				delete pReader;
			}
		}

		m_vpSpectra.push_back( new ITABase::CHDFTSpectra( oNCTCConf.fSampleRate, 2, oNCTCConf.iCTCFilterLength + 1, true ) );
	}

#	ifdef VACORE_REPRODUCTION_NCTC_WITH_SWEET_SPOT_WIDENING
	double dCrossTalkCancellationFactor = -1;
	conf.OptNumber( "CrossTalkCancellationFactor", dCrossTalkCancellationFactor, 1.0f );
	oNCTCConf.fCrossTalkCancellationFactor       = float( dCrossTalkCancellationFactor );
	double dWaveIncidenceAngleCompensationFactor = -1;
	conf.OptNumber( "WaveIncidenceAngleCompensationFactor", dWaveIncidenceAngleCompensationFactor, 1.0f );
	oNCTCConf.fWaveIncidenceAngleCompensationFactor = float( dWaveIncidenceAngleCompensationFactor );
#	endif

	double dBeta;
	conf.OptNumber( "RegularizationBeta", dBeta, 0.001f );
	oNCTCConf.fRegularizationFactor = float( dBeta );

	// Create NCTC instance
	m_pNCTC = new ITANCTC( oNCTCConf );

	int iDelaySamples;
	conf.OptInteger( "DelaySamples", iDelaySamples, oNCTCConf.iCTCFilterLength / 2 );
	m_pNCTC->SetDelayTime( float( iDelaySamples / oNCTCConf.fSampleRate ) );

	// NCTC Stream filter
	ITANCTCStreamFilter::Config oNCTCStreamConf;
	oNCTCStreamConf.N             = m_pNCTC->GetNumChannels( );
	oNCTCStreamConf.dSampleRate   = m_oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.dSampleRate;
	oNCTCStreamConf.iBlockLength  = m_oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.iBuffersize;
	oNCTCStreamConf.iFilterLength = oNCTCConf.iCTCFilterLength;

	conf.OptInteger( "CTCFilterCrossfadeLength", oNCTCStreamConf.iFilterCrossfadeLength, 128 );
	conf.OptInteger( "CTCFilterExchangeMode", oNCTCStreamConf.iFilterExchangeMode, ITABase::FadingFunction::COSINE_SQUARE );

	conf.OptBool( "UseTrackedListenerHRIR", m_bTrackedListenerHRIR, false );

	std::string sCTCDefaultHRIR_raw;
	bool bEntryPresent          = conf.OptString( "CTCDefaultHRIR", sCTCDefaultHRIR_raw );
	std::string sCTCDefaultHRIR = m_oParams.pCore->FindFilePath( sCTCDefaultHRIR_raw );
	if( bEntryPresent && sCTCDefaultHRIR.empty( ) && !m_bTrackedListenerHRIR )
		VA_EXCEPT2( INVALID_PARAMETER, "Could not find default HRIR file for NCTC reproduction module. Switch to tracked listener HRIR or specify correct path." )

	double dSampleRate = m_oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.dSampleRate;
	if( sCTCDefaultHRIR.empty( ) == false )
		m_pDefaultHRIR = new CVADirectivityDAFFHRIR( sCTCDefaultHRIR, "CTCDefaultHRIR", dSampleRate );

	m_pdsStreamFilter = new ITANCTCStreamFilter( oNCTCStreamConf );

	return;
}

CVANCTCReproduction::~CVANCTCReproduction( )
{
	delete m_pdsStreamFilter;
	m_pdsStreamFilter = NULL;


	for( int i = 0; i < m_pNCTC->GetNumChannels( ); i++ )
	{
		delete m_vpSpectra[i];

		const ITANCTC::Config::Loudspeaker& oLS( m_pNCTC->GetConfig( ).voLoudspeaker[i] );
		if( oLS.pDirectivity != nullptr )
			delete oLS.pDirectivity->getParent( );
	}

	delete m_pNCTC;
	m_pNCTC = NULL;

	delete m_pDefaultHRIR;
	m_pDefaultHRIR = NULL;
}

void CVANCTCReproduction::SetInputDatasource( ITADatasource* p )
{
	m_pdsStreamFilter->SetInputDatasource( p );
}

ITADatasource* CVANCTCReproduction::GetOutputDatasource( )
{
	return m_pdsStreamFilter->GetOutputDatasource( );
}

std::vector<const CVAHardwareOutput*> CVANCTCReproduction::GetTargetOutputs( ) const
{
	return m_vpTargetOutputs;
}

void CVANCTCReproduction::SetTrackedListener( int iID )
{
	m_iListenerID = iID;
}

void CVANCTCReproduction::UpdateScene( CVASceneState* pNewState )
{
	if( m_iListenerID == -1 )
		return;

	CVAReceiverState* pListenerState( pNewState->GetReceiverState( m_iListenerID ) );
	if( pListenerState == nullptr )
		return;

	if( pListenerState->GetMotionState( ) == nullptr )
		return;

	const CVADirectivityDAFFHRIR* pHRIRSetDAFF2D = NULL;
	if( m_bTrackedListenerHRIR )
	{
		pHRIRSetDAFF2D = static_cast<const CVADirectivityDAFFHRIR*>( pListenerState->GetDirectivity( ) );
		if( pHRIRSetDAFF2D == nullptr )
			return;

		m_pNCTC->SetHRIR( pHRIRSetDAFF2D->GetDAFFContent( ) );
	}
	else
	{
		if( m_pDefaultHRIR == nullptr )
			return;

		m_pNCTC->SetHRIR( m_pDefaultHRIR->GetDAFFContent( ) );
	}

	VAVec3 vPos, vView, vUp;
	m_oParams.pCore->GetSoundReceiverRealWorldPositionOrientationVU( m_iListenerID, vPos, vView, vUp );

	ITANCTC::Pose oHeadPose;
	oHeadPose.vPos.SetValues( vPos.x, vPos.y, vPos.z );
	/* @todo jst
	oHeadPose.qOrient.SetToNeutralQuaternion();
	oHeadPose.qOrient *= VistaQuaternion( Vista::UpVector, VistaVector3D( ux, uy, uz ) );
	oHeadPose.qOrient *= VistaQuaternion( Vista::ViewVector, VistaVector3D( vx, vy, vz ) );
	*/
	oHeadPose.vView.SetValues( vView.x, vView.y, vView.z );
	oHeadPose.vUp.SetValues( vUp.x, vUp.y, vUp.z );

	m_pNCTC->UpdateHeadPose( oHeadPose );
	m_pNCTC->CalculateFilter( m_vpSpectra );

	if( m_iDebugExportCTCFilters > 0 )
	{
		m_iDebugExportCTCFilters--;
		if( m_sDebugExportCTCFiltersBaseName.empty( ) )
			m_sDebugExportCTCFiltersBaseName = "VANCTC_" + m_oParams.sID + "_CTCFilter_";

		for( size_t n = 0; n < m_vpSpectra.size( ); n++ )
			ITAFFTUtils::Export( m_vpSpectra[n], m_sDebugExportCTCFiltersBaseName + IntToString( int( n + 1 ) ) );
	}

	m_pdsStreamFilter->ExchangeFilters( m_vpSpectra );

	return;
}

CVAStruct CVANCTCReproduction::GetParameters( const CVAStruct& oArgs ) const
{
	CVAStruct oReturn;

	if( oArgs.HasKey( "debug" ) )
	{
		const CVAStruct& oDebugArgs( oArgs["debug"] );
		if( oDebugArgs.HasKey( "export_ctc_filters" ) )
		{
			if( oDebugArgs["export_ctc_filters"].IsNumeric( ) )
				m_iDebugExportCTCFilters = oDebugArgs["export_ctc_filters"];
			if( oDebugArgs.HasKey( "export_ctc_filters_base_name" ) )
				m_sDebugExportCTCFiltersBaseName = std::string( oDebugArgs["export_ctc_filters_base_name"] );
		}
	}

	if( oArgs.HasKey( "info" ) )
	{
		oReturn["Cross_Talk_Cancellation_Factor"]    = m_pNCTC->GetCrossTalkCancellationFactor( );
		oReturn["Wave_Incidence_Angle_Compensation"] = m_pNCTC->GetWaveIncidenceAngleCompensation( );
		oReturn["Num_Channels"]                      = m_pNCTC->GetNumChannels( );
		oReturn["Regularization_Factor"]             = m_pNCTC->GetRegularizationFactor( );
	}

	// Commands: print, get, set

	const CVAStructValue* pStruct;
	if( ( pStruct = oArgs.GetValue( "print" ) ) != nullptr )
	{
		VA_PRINT( "Available commands for " + m_oParams.sID + ": print, set, get" );
		VA_PRINT( "PRINT:" );
		VA_PRINT( "\t'print', 'help'" );
		VA_PRINT( "GET:" );
		VA_PRINT( "\t'get', 'gain'" );
		VA_PRINT( "SET:" );
		VA_PRINT( "\t'set', 'gain', 'value', <number>" );

		oReturn["Return"] = true; // dummy return value, otherwise Problem with MATLAB
		return oReturn;
	}

	if( ( pStruct = oArgs.GetValue( "get" ) ) != nullptr )
	{
		if( pStruct->GetDatatype( ) != CVAStructValue::STRING )
			VA_EXCEPT2( INVALID_PARAMETER, "GET command must be a string" );
		std::string sGetCommand = toUppercase( *pStruct );

		if( sGetCommand == "gain" )
		{
			oReturn["gain"] = m_dGain;
			return oReturn;
		}
		else
		{
			VA_EXCEPT2( INVALID_PARAMETER, "Unrecognized GET command " + sGetCommand );
		}
	}
}

void CVANCTCReproduction::SetParameters( const CVAStruct& oArgs )
{
	const CVAStructValue* pStruct;

	if( ( pStruct = oArgs.GetValue( "gain" ) ) != nullptr )
	{
		if( pStruct->GetDatatype( ) != CVAStructValue::DOUBLE )
			VA_EXCEPT2( INVALID_PARAMETER, "Gain value must be numerical" );

		m_dGain = oArgs["gain"];
		std::vector<float> vfGains( m_pNCTC->GetNumChannels( ) );
		for( int n = 0; n < m_pNCTC->GetNumChannels( ); n++ )
			vfGains[n] = float( m_dGain );
		m_pdsStreamFilter->SetGains( vfGains );
	}

	if( ( pStruct = oArgs.GetValue( "AdditionalDelayTime" ) ) != nullptr )
	{
		if( pStruct->GetDatatype( ) != CVAStructValue::DATA )
			VA_EXCEPT2( INVALID_PARAMETER, "Delay must be a vector of numeric values" );

		if( pStruct->GetDataSize( ) != m_pNCTC->GetNumChannels( ) * sizeof( float ) )
			VA_EXCEPT2( INVALID_PARAMETER, "Dimension mismatch, N delay values required" );

		const float* pfAdditionalDelayData = (const float*)( pStruct->GetData( ) );
		std::vector<float> vfDelayTime     = m_pNCTC->GetDelayTime( );
		for( int n = 0; n < m_pNCTC->GetNumChannels( ); n++ )
			vfDelayTime[n] += pfAdditionalDelayData[n];

		m_pNCTC->SetDelayTime( vfDelayTime );
	}

	if( ( pStruct = oArgs.GetValue( "export_ctc_filters" ) ) != nullptr )
	{
		if( pStruct->GetDatatype( ) != CVAStructValue::STRING )
			VA_EXCEPT2( INVALID_PARAMETER, "Export base name must be a string" );

		std::string sExportFileBaseName = *pStruct;
		if( sExportFileBaseName.empty( ) )
			sExportFileBaseName = "VANCTC_CTCFilter_";

		for( size_t n = 0; n < m_vpSpectra.size( ); n++ )
			ITAFFTUtils::Export( m_vpSpectra[n], sExportFileBaseName + IntToString( int( n + 1 ) ) );
	}

	if( ( pStruct = oArgs.GetValue( "export_hrirs" ) ) != nullptr )
	{
		if( pStruct->GetDatatype( ) != CVAStructValue::STRING )
			VA_EXCEPT2( INVALID_PARAMETER, "Export base name must be a string" );

		std::string sExportFileBaseName = *pStruct;
		if( sExportFileBaseName.empty( ) )
			sExportFileBaseName = "VANCTC_HRIR_";

		std::vector<ITABase::CHDFTSpectra*> vpHRTFs;
		if( m_pNCTC->GetHRTF( vpHRTFs ) )
			for( size_t n = 0; n < vpHRTFs.size( ); n++ )
				ITAFFTUtils::Export( vpHRTFs[n], sExportFileBaseName + IntToString( int( n + 1 ) ) );
	}

#	ifdef VACORE_REPRODUCTION_NCTC_WITH_SWEET_SPOT_WIDENING

	if( ( pStruct = oArgs.GetValue( "CrossTalkCancellationFactor" ) ) != nullptr )
	{
		if( pStruct->GetDatatype( ) != CVAStructValue::DOUBLE )
			VA_EXCEPT2( INVALID_PARAMETER, "CrossTalkCancellationFactor value must be numerical" );

		m_pNCTC->SetCrossTalkCancellationFactor( float( double( *pStruct ) ) );

		// Force update
		m_pNCTC->CalculateFilter( m_vpSpectra );
		m_pdsStreamFilter->ExchangeFilters( m_vpSpectra );
	}

	if( ( pStruct = oArgs.GetValue( "WaveIncidenceAngleCompensation" ) ) != nullptr )
	{
		if( pStruct->GetDatatype( ) != CVAStructValue::DOUBLE )
			VA_EXCEPT2( INVALID_PARAMETER, "WaveIncidenceAngleCompensation value must be numerical" );

		m_pNCTC->SetWaveIncidenceAngleCompensationFactor( float( double( *pStruct ) ) );

		// Force update
		m_pNCTC->CalculateFilter( m_vpSpectra );
		m_pdsStreamFilter->ExchangeFilters( m_vpSpectra );
	}

#	endif
}

int CVANCTCReproduction::GetNumInputChannels( ) const
{
	return 2;
}

#endif // VACORE_WITH_REPRODUCTION_BINAURAL_NCTC
