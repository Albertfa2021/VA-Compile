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

#include "VASoundPathRendererBase.h"

#include "../../core/core.h"
#include "../../Scene/VASoundReceiverState.h"
#include "../../Scene/VASoundSourceDesc.h"
#include "../../Scene/VASoundSourceState.h"
#include "../../Utils/VAUtils.h"
#include "../../directivities/VADirectivityDAFFEnergetic.h"
#include "../../VALog.h"
#include "VAAudioRendererReceiver.h"
#include "VAAudioRendererSource.h"

#include <DAFFContentMS.h>
#include <ITASampleFrame.h>
#include <ITAThirdOctaveFilterbank.h>
#include <ITAVariableDelayLine.h>
#include <ITAISO9613.h>



CVASoundPathRendererBase::Config::Config( const CVAAudioRendererInitParams& oParams, const Config& oDefaultValues )
    : CVAAudioRendererBase::Config( oParams, oDefaultValues )
{
	CVAConfigInterpreter conf( *oParams.pConfig );
	const std::string sExceptionMsgPrefix = "Renderer ID '" + oParams.sID + "': ";

	oSpatialEncoding.iBlockSize = iBlockSize;

	conf.OptString( "SpatialEncodingType", sSpatialEncodingType, oDefaultValues.sSpatialEncodingType );
	const std::string sSpatialEncodingType_lower = toLowercase( sSpatialEncodingType );
	if( sSpatialEncodingType_lower == "binaural" )
		oSpatialEncoding.eType = IVASpatialEncoding::EType::Binaural;
	else if( sSpatialEncodingType_lower == "ambisonics" )
		oSpatialEncoding.eType = IVASpatialEncoding::EType::Ambisonics;
	else if( sSpatialEncodingType_lower == "vbap" )
		oSpatialEncoding.eType = IVASpatialEncoding::EType::VBAP;
	else
		VA_EXCEPT2( INVALID_PARAMETER, sExceptionMsgPrefix + "Unrecognized spatial encoding type '" + sSpatialEncodingType + "' in configuration" );

	conf.OptInteger( "HRIRFilterLength", oSpatialEncoding.iHRIRFilterLength, oDefaultValues.oSpatialEncoding.iHRIRFilterLength );
	conf.OptInteger( "AmbisonicsOrder", oSpatialEncoding.iAmbisonicsOrder, oDefaultValues.oSpatialEncoding.iAmbisonicsOrder );

	if( oSpatialEncoding.eType == IVASpatialEncoding::EType::VBAP )
	{
		conf.ReqString( "VBAPLoudspeakerSetup", oSpatialEncoding.sVBAPLoudSpreakerSetup );
		conf.ReqString( "VBAPTriangulationFile", oSpatialEncoding.sVBAPTriangulationFile );
		if( !oSpatialEncoding.sVBAPTriangulationFile.empty( ) )
			oSpatialEncoding.sVBAPTriangulationFile = oParams.pCore->FindFilePath( oSpatialEncoding.sVBAPTriangulationFile );

		std::string sLSCenterPos;
		conf.ReqString( "VBAPCenterPos", sLSCenterPos );
		std::vector<std::string> vsPosComponents = splitString( sLSCenterPos, ',' );
		if( vsPosComponents.size( ) != 3 )
			VA_EXCEPT2( INVALID_PARAMETER, sExceptionMsgPrefix + "VBAPCenterPos must be a 3D-Vector (e.g. '0, 1, 0')" );
		oSpatialEncoding.v3VBAPSetupCenter.Set( StringToFloat( vsPosComponents[0] ), StringToFloat( vsPosComponents[1] ), StringToFloat( vsPosComponents[2] ) );

		conf.OptInteger( "MDAPNumSpreadingSources", oSpatialEncoding.iMDAPSpreadingSources, oDefaultValues.oSpatialEncoding.iMDAPSpreadingSources );
		conf.OptNumber( "MDAPSpreadingAngleDegrees", oSpatialEncoding.dMDAPSpreadingAngleDeg, oDefaultValues.oSpatialEncoding.dMDAPSpreadingAngleDeg );
	}
	

	conf.OptString( "FilterBankType", sFilterBankType, oDefaultValues.sFilterBankType );
	const std::string sFilterBankType_lower = toLowercase( sFilterBankType );
	if( sFilterBankType_lower == "fir_spline_linear_phase" || sFilterBankType_lower == "fir" )
		iFilterBankType = CITAThirdOctaveFilterbank::FIR_SPLINE_LINEAR_PHASE;
	else if( sFilterBankType_lower == "iir_burg_order4" )
		iFilterBankType = CITAThirdOctaveFilterbank::IIR_BURG_ORDER4;
	else if( sFilterBankType_lower == "iir_burg_order10" || sFilterBankType_lower == "iir" )
		iFilterBankType = CITAThirdOctaveFilterbank::IIR_BURG_ORDER10;
	else if( sFilterBankType_lower == "iir_biquads_order10" )
		iFilterBankType = CITAThirdOctaveFilterbank::IIR_BIQUADS_ORDER10;
	else
		VA_EXCEPT2( INVALID_PARAMETER, sExceptionMsgPrefix + "Unrecognized filter bank type '" + sFilterBankType + "' in configuration" );

	conf.OptBool( "AuralizationParameterHistoriesEnabled", bAuralizationParameterHistory, oDefaultValues.bAuralizationParameterHistory );

	// External auralization parameter updates?
	const bool bOverwriteDefault = oParams.pConfig->HasKey( "ExternalSoundPathSimulation" );
	bool bExternalSoundPaths;
	conf.OptBool( "ExternalSoundPathSimulation", bExternalSoundPaths, false );

	const std::string sKeyAtt  = oParams.pConfig->HasKey( "AirAttenuationExternalSimulation" ) ? "AirAttenuationExternalSimulation" : "AirAbsorptionExternalSimulation";
	const std::string sKeyTurb = oParams.pConfig->HasKey( "TurbulenceExternalSimulation" ) ? "TurbulenceExternalSimulation" : "TemporalVariationsExternalSimulation";
	const std::string sKeyLaunchDir =
	    oParams.pConfig->HasKey( "LaunchDirectionExternalSimulation" ) ? "LaunchDirectionExternalSimulation" : "SourceWavefrontNormalExternalSimulation";
	const std::string sKeyIncDir =
	    oParams.pConfig->HasKey( "IncidentDirectionExternalSimulation" ) ? "IncidentDirectionExternalSimulation" : "ReceiverWavefrontNormalExternalSimulation";

	conf.OptBool( "PropagationDelayExternalSimulation", oExternalSimulation.bPropagationDelay,
	              bOverwriteDefault ? bExternalSoundPaths : oDefaultValues.oExternalSimulation.bPropagationDelay );
	conf.OptBool( "SpreadingLossExternalSimulation", oExternalSimulation.bSpreadingLoss,
	              bOverwriteDefault ? bExternalSoundPaths : oDefaultValues.oExternalSimulation.bSpreadingLoss );
	conf.OptBool( sKeyLaunchDir, oExternalSimulation.bSourceWFNormal, bOverwriteDefault ? bExternalSoundPaths : oDefaultValues.oExternalSimulation.bSourceWFNormal );
	conf.OptBool( sKeyIncDir, oExternalSimulation.bReceiverWFNormal, bOverwriteDefault ? bExternalSoundPaths : oDefaultValues.oExternalSimulation.bReceiverWFNormal );
	conf.OptBool( sKeyAtt, oExternalSimulation.bAirAttenuation, bOverwriteDefault ? bExternalSoundPaths : oDefaultValues.oExternalSimulation.bAirAttenuation );
	conf.OptBool( sKeyTurb, oExternalSimulation.bTurbulence, bOverwriteDefault ? bExternalSoundPaths : oDefaultValues.oExternalSimulation.bTurbulence );
	conf.OptBool( "ReflectionExternalSimulation", oExternalSimulation.bReflection,
	              bOverwriteDefault ? bExternalSoundPaths : oDefaultValues.oExternalSimulation.bReflection );
	conf.OptBool( "DiffractionExternalSimulation", oExternalSimulation.bDiffraction,
	              bOverwriteDefault ? bExternalSoundPaths : oDefaultValues.oExternalSimulation.bDiffraction );

	conf.OptBool( "DirectivityExternalSimulation", oExternalSimulation.bDirectivity, oDefaultValues.oExternalSimulation.bDirectivity );
}




CVASoundPathRendererBase::CVASoundPathRendererBase( const CVAAudioRendererInitParams& oParams, const Config& oDefaultValues )
     : CVAAudioRendererBase( oParams, oDefaultValues )
     , m_oConf( Config( oParams, oDefaultValues ) )
{
	CVASpatialEncodingFactory::RegisterLSSetupIfVBAP( m_oConf.oSpatialEncoding, oParams.pCore->GetCoreConfig( )->oHardwareSetup );
	InitBaseDSPEntities( CVASpatialEncodingFactory::GetNumChannels( m_oConf.oSpatialEncoding ) );
}


 void CVASoundPathRendererBase::SetParameters( const CVAStruct& oParams )
 {
	 CVAAudioRendererBase::SetParameters( oParams );

	 if( oParams.HasKey( "sound_source_id" ) && oParams.HasKey( "sound_receiver_id" ) )
	 {
		 const int iSourceID   = oParams["sound_source_id"];
		 const int iReceiverID = oParams["sound_receiver_id"];
		 
		 VA_VERBOSE( m_oParams.sClass, "Updating sound path for sound source id " << iSourceID << " and sound receiver id " << iReceiverID );

		 auto pSRPairATN = dynamic_cast<SourceReceiverPair*>( GetSourceReceiverPair( iSourceID, iReceiverID ) );
		 if( pSRPairATN )
			 pSRPairATN->ExternalUpdate( oParams );
	 }
 }


CVASoundPathRendererBase::SoundPathBase::SoundPathBase( const Config& oConf, CVARendererSource* pSource, CVARendererReceiver* pReceiver )
    : m_oConf( oConf )
    , m_pSource( pSource )
    , m_pReceiver( pReceiver )
{
	if( !m_pSource || !m_pReceiver )
		VA_EXCEPT2( INVALID_PARAMETER, "Using invalid source or receiver while creating SoundPath" );

	m_pSource->AddReference( );
	m_pReceiver->AddReference( );

	m_oSourceDirectivityMagnitudes.SetIdentity( );
	m_oAirAttenuationMagnitudes.SetIdentity( );
	m_oSoundPathMagnitudes.SetIdentity( );

	const float fMaxDelaySamples = m_oConf.fMaxDelaySeconds * m_oConf.dSampleRate;
	m_pVariableDelayLine          = std::make_unique<CITAVariableDelayLine>( m_oConf.dSampleRate, m_oConf.iBlockSize, fMaxDelaySamples, oConf.iVDLSwitchingAlgorithm );
	m_pThirdOctaveFilterBank      = CITAThirdOctaveFilterbank::CreateUnique( m_oConf.dSampleRate, m_oConf.iBlockSize, oConf.iFilterBankType );
	m_pSpatialEncoding            = CVASpatialEncodingFactory::Create( oConf.oSpatialEncoding );
}

CVASoundPathRendererBase::SoundPathBase::~SoundPathBase( )
{
	if( m_pSource )
		m_pSource->RemoveReference( );
	if( m_pReceiver )
		m_pReceiver->RemoveReference( );
}


void CVASoundPathRendererBase::SoundPathBase::SetReflectionOrder( int iOrder )
{
	if( iOrder < 0 )
		VA_EXCEPT2( INVALID_PARAMETER, "Reflection order of sound path must be >= 0." );
	m_iReflectionOrder = iOrder;
}

void CVASoundPathRendererBase::SoundPathBase::SetDiffractionOrder( int iOrder )
{
	if( iOrder < 0 )
		VA_EXCEPT2( INVALID_PARAMETER, "Diffraction order of sound path must be >= 0." );
	m_iDiffractionOrder = iOrder;
}

const VAVec3& CVASoundPathRendererBase::SoundPathBase::GetSourcePos( ) const
{
	return m_pSource->PredictedPosition( );
}

const VAVec3& CVASoundPathRendererBase::SoundPathBase::GetReceiverPos( ) const
{
	return m_pReceiver->PredictedPosition( );
}

void CVASoundPathRendererBase::SoundPathBase::Process( double dTimeStamp, const AuralizationMode& oAuralizationMode, const CVASoundSourceState& oSourceState,
                                                       const CVAReceiverState& oReceiverState )
{
	UpdateAuralizationParameters( dTimeStamp, oAuralizationMode, m_dPropagationDelay, m_dSpreadingLoss, m_v3SourceWavefrontNormal, m_v3ReceiverWavefrontNormal,
	                              m_oAirAttenuationMagnitudes, m_oSoundPathMagnitudes );

	UpdateDSPElements( oAuralizationMode, oSourceState );
	ProcessDSPElements( oAuralizationMode, oReceiverState );
}

void CVASoundPathRendererBase::SoundPathBase::ExternalUpdate( const CVAStruct& oUpdate )
{	
	if( oUpdate.HasKey( "propagation_time" ) )
		m_dPropagationDelay = oUpdate["propagation_time"];
	if( oUpdate.HasKey( "geometrical_spreading_loss" ) )
	    m_dSpreadingLoss = oUpdate["geometrical_spreading_loss"];
	if( oUpdate.HasKey( "directivity_third_octaves" ) )
	    ParseSpectrum( oUpdate["directivity_third_octaves"], m_oSourceDirectivityMagnitudes );
	if( oUpdate.HasKey( "launch_direction" ) )
		ParseVAVec( oUpdate["launch_direction"], m_v3SourceWavefrontNormal );
	else if( oUpdate.HasKey( "source_wavefront_normal" ) )
		ParseVAVec( oUpdate["source_wavefront_normal"], m_v3SourceWavefrontNormal );
	if( oUpdate.HasKey( "incident_direction" ) )
		ParseVAVec( oUpdate["incident_direction"], m_v3ReceiverWavefrontNormal );
	else if( oUpdate.HasKey( "receiver_wavefront_normal" ) )
		ParseVAVec( oUpdate["receiver_wavefront_normal"], m_v3ReceiverWavefrontNormal );
	if( oUpdate.HasKey( "air_attenuation_third_octaves" ) )
		ParseSpectrum( oUpdate["air_attenuation_third_octaves"], m_oAirAttenuationMagnitudes );
}

void CVASoundPathRendererBase::SoundPathBase::UpdateDSPElements( const AuralizationMode& oAuralizationMode, const CVASoundSourceState& oSourceState )
{
	const bool bSoundPathMuted = !oAuralizationMode.bDirectSound && IsDirectSound( ) || !oAuralizationMode.bEarlyReflections && ReflectionOrder( ) ||
	                             !oAuralizationMode.bDiffraction && DiffractionOrder( );
	const bool bMuted = m_pSource->pData->bMuted || bSoundPathMuted;
	if( bMuted )
		m_dGain = 0.0;
	else
	{
		m_dGain                       = oAuralizationMode.bSpreadingLoss ? m_dSpreadingLoss : ( 1.0 / m_oConf.oCoreConfig.dDefaultDistance );
		const double dSoundSourceGain = oSourceState.GetVolume( m_oConf.oCoreConfig.dDefaultAmplitudeCalibration );
		m_dGain *= dSoundSourceGain;
	}


	bool bDPEnabledCurrent = ( m_pVariableDelayLine->GetAlgorithm( ) != EVDLAlgorithm::SWITCH ); // switch = disabled
	bool bDPStatusChanged  = ( bDPEnabledCurrent != oAuralizationMode.bDoppler );
	if( bDPStatusChanged )
		m_pVariableDelayLine->SetAlgorithm( !bDPEnabledCurrent ? m_oConf.iVDLSwitchingAlgorithm : EVDLAlgorithm::SWITCH );
	m_pVariableDelayLine->SetDelayTime( float( m_dPropagationDelay ) ); // TODO: Should we keep the delay fixed if DP is disabled?

	if( !m_oConf.oExternalSimulation.bDirectivity)
		UpdateSourceDirectivity( oAuralizationMode.bSourceDirectivity, oSourceState );


	ITABase::CThirdOctaveGainMagnitudeSpectrum oThirdOctaveFilterSpectrum = m_oSoundPathMagnitudes;
	if( oAuralizationMode.bMediumAbsorption )
		oThirdOctaveFilterSpectrum.Multiply( m_oAirAttenuationMagnitudes );
	if( oAuralizationMode.bSourceDirectivity )
		oThirdOctaveFilterSpectrum.Multiply( m_oSourceDirectivityMagnitudes );
	m_pThirdOctaveFilterBank->SetMagnitudes( oThirdOctaveFilterSpectrum );
}

void CVASoundPathRendererBase::SoundPathBase::ProcessDSPElements( const AuralizationMode& oAuralizationMode, const CVAReceiverState& oReceiverState )
{
	CVASoundSourceDesc* pSourceData = m_pSource->pData;
	const ITASampleFrame& sbInput( *pSourceData->psfSignalSourceInputBuf ); // Always use first channel of SignalSource
	assert( pSourceData->iID >= 0 ); // Knallt es hier, dann wurde dem SoundPath unterm Hintern die Quelle entzogen! -> Problem mit Referenzierung und Reset?


	ITASampleBuffer sbTemp = ITASampleBuffer( m_oConf.iBlockSize );
	m_pThirdOctaveFilterBank->Process( sbInput[0].GetData( ), sbTemp.data( ) );
	m_pVariableDelayLine->Process( &( sbTemp ), &( sbTemp ) );

	const VAVec3 v3DirLookingFromReceiver = m_v3ReceiverWavefrontNormal * ( -1.0 );
	const double dAzimuthDeg              = GetAzimuthFromDirection_DEG( m_pReceiver->PredictedViewVec( ), m_pReceiver->PredictedUpVec( ), v3DirLookingFromReceiver );
	const double dElevationDeg            = GetElevationFromDirection_DEG( m_pReceiver->PredictedUpVec( ), v3DirLookingFromReceiver );
	ITASampleFrame& sfOutput              = *m_pReceiver->psfOutput.get( );
	m_pSpatialEncoding->Process( sbTemp, sfOutput, m_dGain, dAzimuthDeg, dElevationDeg, oReceiverState );
}

void CVASoundPathRendererBase::SoundPathBase::UpdateSourceDirectivity( bool bSourceDirEnabled, const CVASoundSourceState& oSourceState )
{
	m_oDirectivityStateNew.bDirectivityEnabled = bSourceDirEnabled;
	m_oDirectivityStateNew.pData               = (IVADirectivity*)oSourceState.GetDirectivityData( );

	if( bSourceDirEnabled == true )
	{
		CVADirectivityDAFFEnergetic* pDirectivityDataNew = (CVADirectivityDAFFEnergetic*)m_oDirectivityStateNew.pData;

		if( pDirectivityDataNew == nullptr || !pDirectivityDataNew->GetDAFFContent( ) )
		{
			// Directivity not set or removed
			m_oSourceDirectivityMagnitudes.SetIdentity( );
			m_oDirectivityStateNew.iRecord = -1;
		}
		else
		{
			// Get new directivity index
			const double dAzimuthDeg   = GetAzimuthFromDirection_DEG( m_pSource->PredictedViewVec( ), m_pSource->PredictedUpVec( ), m_v3SourceWavefrontNormal );
			const double dElevationDeg = GetElevationFromDirection_DEG( m_pSource->PredictedUpVec( ), m_v3SourceWavefrontNormal );
			pDirectivityDataNew->GetDAFFContent( )->getNearestNeighbour( DAFF_OBJECT_VIEW, float( dAzimuthDeg ), float( dElevationDeg ), m_oDirectivityStateNew.iRecord );
			// Update magnitudes if required
			if( m_oDirectivityStateNew != m_oDirectivityStateCur )
			{
				std::vector<float> vfGains( ITABase::CThirdOctaveMagnitudeSpectrum::GetNumBands( ) );
				pDirectivityDataNew->GetDAFFContent( )->getMagnitudes( m_oDirectivityStateNew.iRecord, 0, &vfGains[0] );
				m_oSourceDirectivityMagnitudes.SetMagnitudes( vfGains );
			}
		}
	}
	else if( m_oDirectivityStateCur.bDirectivityEnabled )
	{
		// Switch to disabled DIR (= omni-directional)
		m_oSourceDirectivityMagnitudes.SetIdentity( );
		m_oDirectivityStateNew.iRecord = -1;
	}

	// Acknowledge new state
	m_oDirectivityStateCur = m_oDirectivityStateNew;
}

void CVASoundPathRendererBase::SoundPathBase::HomogeneousAirAbsorption( double dDistance, const CVAHomogeneousMedium& oMedium,
                                                                        ITABase::CThirdOctaveGainMagnitudeSpectrum& oOutputSpectrum )
{
	const double& dTemperatur = oMedium.dTemperatureDegreeCentigrade;
	const double& dPressure   = oMedium.dStaticPressurePascal;
	const double& dHumidity   = oMedium.dRelativeHumidityPercent;

	ITABase::ISO9613::AtmosphericAbsorption( oOutputSpectrum, dDistance, dTemperatur, dHumidity, dPressure );

	for( int n = 0; n < oOutputSpectrum.GetNumBands( ); n++ )
		oOutputSpectrum.SetMagnitude( n, 1.0f - oOutputSpectrum[n] );
}

void CVASoundPathRendererBase::SoundPathBase::ParseSpectrum( const CVAStructValue& oSpectrumData, ITABase::CThirdOctaveGainMagnitudeSpectrum& oSpectrum )
{
	if( oSpectrumData.GetDatatype( ) == CVAStructValue::DOUBLE )
	{
		const int iNumValues = oSpectrumData.GetDataSize( ) / sizeof( float ); // currently only accepts third octave values, this is a safety check
		if( iNumValues != oSpectrum.GetNumBands( ) )
			VA_EXCEPT2( INVALID_PARAMETER, "Expected 31 frequency magnitudes for third-octave band spectrum." );

		const float* pfMags = (const float*)( oSpectrumData.GetData( ) ); // convert to float values
		for( int i = 0; i < oSpectrum.GetNumBands( ); i++ )
			oSpectrum.SetMagnitude( i, pfMags[i] );
	}
	else if( oSpectrumData.GetDatatype( ) == CVAStructValue::STRUCT )
	{
		const CVAStruct oSpectrumStruct = CVAStruct( oSpectrumData );
		for( int i = 0; i < oSpectrum.GetNumBands( ); i++ )
		{
			std::string sBandValueKey = "band_" + std::to_string( long( i + 1 ) );
			if( !oSpectrumStruct.HasKey( sBandValueKey ) )
				VA_EXCEPT2( INVALID_PARAMETER, std::string( "Missing third-octave band spectrum data for frequency band " ) + std::to_string( i ) );
			const double dValue = oSpectrumStruct[sBandValueKey];
			oSpectrum.SetMagnitude( i, float( dValue ) );
		}
	}
	else
		VA_EXCEPT2( INVALID_PARAMETER, "Invalid datatype for parsing a magnitude spectrum. Expected double or struct." );
}

void CVASoundPathRendererBase::SoundPathBase::ParseVAVec( const CVAStructValue& o3DVecData, VAVec3& v3Vec )
{
	int num_values = o3DVecData.GetDataSize( ) / sizeof( float ); // safety check to make sure a 3D vector is passed
	if( num_values != 3 )
		VA_EXCEPT2( INVALID_PARAMETER, "Expected 3 values for parsing a VAVec3." );
	const float* pfPosition = (const float*)( o3DVecData.GetData( ) ); // convert to float values
	
	v3Vec.Set( pfPosition[0], pfPosition[1], pfPosition[2] );
}




void CVASoundPathRendererBase::SourceReceiverPair::PreRequest( )
{
	CVAAudioRendererBase::SourceReceiverPair::PreRequest( );
}
void CVASoundPathRendererBase::SourceReceiverPair::PreRelease( )
{
	CVAAudioRendererBase::SourceReceiverPair::PreRelease( );
	m_lpSoundPaths.clear( );
}

void CVASoundPathRendererBase::SourceReceiverPair::Process( double dTimeStamp, const AuralizationMode& oAuraMode, const CVASoundSourceState& oSourceState,
                                                            const CVAReceiverState& oReceiverState )
{
	RunSimulation( );
	for( auto pSoundPath: m_lpSoundPaths )
		pSoundPath->Process( dTimeStamp, oAuraMode, oSourceState, oReceiverState );
}
