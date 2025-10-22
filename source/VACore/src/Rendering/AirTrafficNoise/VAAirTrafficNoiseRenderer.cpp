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

#include "VAAirtrafficNoiseRenderer.h"

#ifdef VACORE_WITH_RENDERER_BINAURAL_AIR_TRAFFIC_NOISE

#include "../Base/VAAudioRendererReceiver.h"
#include "../Base/VAAudioRendererSource.h"
#include "../../Utils/VAUtils.h"
#include "../../VALog.h"

#include <ITAGeo/Utils/JSON/Atmosphere.h>
#include <ITAPropagationModels/Atmosphere/AirAttenuation.h>
#include <ITAException.h>

#include <utility>



VistaVector3D VAtoARTVector( const VAVec3& v3VA )
{
	return VistaVector3D( v3VA.x, -v3VA.z, v3VA.y );
}
VAVec3 ARTtoVAVector( const VistaVector3D& v3RT )
{
	return VAVec3( v3RT[Vista::X], v3RT[Vista::Z], -v3RT[Vista::Y] );
}


void ParseStratifiedAtmosphere( const std::string& sAtmosphereJSONFilepathOrContent, CStratifiedAtmosphere& oAtmosphere )
{
	try
	{
		if( sAtmosphereJSONFilepathOrContent.find( ".json" ) != std::string::npos )
			ITAGeo::Utils::JSON::Import( oAtmosphere, sAtmosphereJSONFilepathOrContent );
		else
			ITAGeo::Utils::JSON::Decode( oAtmosphere, sAtmosphereJSONFilepathOrContent );
	}
	catch( const ITAException& ex )
	{
		VA_EXCEPT2( INVALID_PARAMETER, "Error while parsing stratified atmosphere: " + ex.sReason );
	}
	catch( std::exception& ex )
	{
		VA_EXCEPT2( INVALID_PARAMETER, "Error while parsing stratified atmosphere: " + std::string( ex.what( ) ) );
	}
}


CVAAirTrafficNoiseRenderer::Config::Config( const CVAAudioRendererInitParams& oParams, const Config& oDefaultValues )
    : CVASoundPathRendererBase::Config( oParams, oDefaultValues )
{
	CVAConfigInterpreter conf( *oParams.pConfig );
	const std::string sExceptionMsgPrefix = "Renderer ID '" + oParams.sID + "': ";

	conf.OptString( "MediumType", sMediumType, oDefaultValues.sMediumType );
	conf.OptNumber( "GroundPlanePosition", dGroundPlanePosition, oDefaultValues.dGroundPlanePosition );
	conf.OptString( "StratifiedAtmosphereFilepath", sStratifiedAtmosphereFilepath, oDefaultValues.sStratifiedAtmosphereFilepath );

	const std::string sMediumType_lower = toLowercase( sMediumType );
	if( sMediumType_lower == "homogeneous" )
		eMediumType = EMediumType::Homogeneous;
	else if( sMediumType_lower == "inhomogeneous" )
		eMediumType = EMediumType::Inhomogeneous;
	else
		VA_EXCEPT2( INVALID_PARAMETER, sExceptionMsgPrefix + "Unrecognized medium type '" + sMediumType + "' in configuration" );


	InitGeometricSoundPathsRequired( );
}
void CVAAirTrafficNoiseRenderer::Config::InitGeometricSoundPathsRequired( )
{
	m_bGeometricSoundPathsRequired =
	    !( oExternalSimulation.bPropagationDelay && oExternalSimulation.bSpreadingLoss && ( oExternalSimulation.bDirectivity || oExternalSimulation.bSourceWFNormal ) &&
	       oExternalSimulation.bReceiverWFNormal && oExternalSimulation.bAirAttenuation );
}



CVAAirTrafficNoiseRenderer::CVAAirTrafficNoiseRenderer( const CVAAudioRendererInitParams& oParams )
    : CVASoundPathRendererBase( oParams, Config( ) )
    , m_oConf( oParams, Config( ) )
{
	IVAPoolObjectFactory* pFactory = (IVAPoolObjectFactory*)new SourceReceiverPairFactory( m_oConf, m_oStratifiedAtmosphere );
	InitSourceReceiverPairPool( pFactory );

	if( m_oConf.eMediumType == EMediumType::Inhomogeneous )
	{
		if( !m_oConf.sStratifiedAtmosphereFilepath.empty( ) ) // Otherwise default constructor is used
			ParseStratifiedAtmosphere( m_oConf.sStratifiedAtmosphereFilepath, m_oStratifiedAtmosphere );
	}
}

void CVAAirTrafficNoiseRenderer::SetParameters( const CVAStruct& oParams )
{
	CVASoundPathRendererBase::SetParameters( oParams );

	if( oParams.HasKey( "stratified_atmosphere" ) )
	{
		// TODO: Maybe it is necessary to wait for audio context is idle before changing atmosphere
		VA_VERBOSE( m_oParams.sClass, "Updating stratified atmosphere for renderer with ID '" + m_oParams.sID + "' used for inhomogeneous transmission." );
		ParseStratifiedAtmosphere( oParams["stratified_atmosphere"], m_oStratifiedAtmosphere );
	}
}



CVAAirTrafficNoiseRenderer::ATNSoundPath::ATNSoundPath( const Config& oConf, CVARendererSource* pSource, CVARendererReceiver* pReceiver )
    : CVASoundPathRendererBase::SoundPathBase( oConf, pSource, pReceiver )
    , m_oConf( oConf )
{
	oTurbulenceMagnitudes.SetIdentity( );
	oGroundReflectionMagnitudes.SetIdentity( );
}

void CVAAirTrafficNoiseRenderer::ATNSoundPath::ExternalUpdate( const CVAStruct& oUpdate )
{
	CVASoundPathRendererBase::SoundPathBase::ExternalUpdate( oUpdate );

	if( oUpdate.HasKey( "turbulence_third_octaves" ) )
		ParseSpectrum( oUpdate["turbulence_third_octaves"], oTurbulenceMagnitudes );
	else if( oUpdate.HasKey( "temporal_variation_third_octaves" ) )
		ParseSpectrum( oUpdate["temporal_variation_third_octaves"], oTurbulenceMagnitudes );
	if( oUpdate.HasKey( "ground_reflection_third_octaves" ) )
		ParseSpectrum( oUpdate["ground_reflection_third_octaves"], oGroundReflectionMagnitudes );
}


void CVAAirTrafficNoiseRenderer::ATNSoundPath::UpdateAuralizationParameters( double dTimeStamp, const AuralizationMode& oAuraMode, double& dDelay, double& dSpreadingLoss,
                                                                             VAVec3& v3SourceWavefrontNormal, VAVec3& v3ReceiverWavefrontNormal,
                                                                             ITABase::CThirdOctaveGainMagnitudeSpectrum& oAirAttenuationMagnitudes,
                                                                             ITABase::CThirdOctaveGainMagnitudeSpectrum& oSoundPathMagnitudes )
{
	if( m_oConf.GeometricSoundPathsRequired( ) )
		UpdateMediumSpecificAuralizationParameters( dTimeStamp, dDelay, dSpreadingLoss, v3SourceWavefrontNormal, v3ReceiverWavefrontNormal, oAirAttenuationMagnitudes );
	
	// Limiting spreading loss
	const double dMaxSpreadingLossFactor = 1.0 / m_oConf.oCoreConfig.dDefaultMinimumDistance;
	dSpreadingLoss                       = std::min( dSpreadingLoss, dMaxSpreadingLossFactor );

	if( !m_oConf.oExternalSimulation.bTurbulence )
		oATVGenerator.GetMagnitudes( oTurbulenceMagnitudes );

	//// TODO: Add a way to set reflection without SetParameters?
	// if( ReflectionOrder( ) && !m_oConf.bReflectionExternalSimulation )
	//{
	//	//oGroundReflectionMagnitudes = ...
	//}

	oSoundPathMagnitudes.SetIdentity( );
	if( oAuraMode.bTemporalVariations )
		oSoundPathMagnitudes *= oTurbulenceMagnitudes;
	if( ReflectionOrder( ) && oAuraMode.bEarlyReflections )
		oSoundPathMagnitudes *= oGroundReflectionMagnitudes;
}

void CVAAirTrafficNoiseRenderer::ATNSoundPathHomogeneous::UpdateMediumSpecificAuralizationParameters(
    double dTimeStamp, double& dDelay, double& dSpreadingLoss, VAVec3& v3SourceWavefrontNormal, VAVec3& v3ReceiverWavefrontNormal,
    ITABase::CThirdOctaveGainMagnitudeSpectrum& oAirAttenuationMagnitudes )
{
	VAVec3 v3SourcePos = GetSourcePos( );
	if( ReflectionOrder( ) ) // Image source for ground reflection
		v3SourcePos.y = m_oConf.dGroundPlanePosition - ( v3SourcePos.y - m_oConf.dGroundPlanePosition );

	const VAVec3 v3SourceToReceiver = GetReceiverPos( ) - v3SourcePos;
	const double dDistance          = v3SourceToReceiver.Length( );
	if( dDistance == 0.0 )
		return;

	if( !m_oConf.oExternalSimulation.bPropagationDelay )
		dDelay = dDistance / m_oConf.oHomogeneousMedium.dSoundSpeed;
	if( !m_oConf.oExternalSimulation.bSpreadingLoss )
		dSpreadingLoss = 1.0 / dDistance;

	const VAVec3 v3WFNormal = v3SourceToReceiver / dDistance;
	if( !m_oConf.oExternalSimulation.bSourceWFNormal )
		v3SourceWavefrontNormal = v3WFNormal;
	if( !m_oConf.oExternalSimulation.bReceiverWFNormal )
		v3ReceiverWavefrontNormal = v3WFNormal;

	if( !m_oConf.oExternalSimulation.bAirAttenuation )
		HomogeneousAirAbsorption( dDistance, m_oConf.oHomogeneousMedium, oAirAttenuationMagnitudes );
}


void CVAAirTrafficNoiseRenderer::ATNSoundPathInhomogeneous::UpdateMediumSpecificAuralizationParameters(
    double dTimeStamp, double& dDelay, double& dSpreadingLoss, VAVec3& v3SourceWavefrontNormal, VAVec3& v3ReceiverWavefrontNormal,
    ITABase::CThirdOctaveGainMagnitudeSpectrum& oAirAttenuationMagnitudes )
{
	assert( m_pRay );

	if( !m_oConf.oExternalSimulation.bPropagationDelay )
		dDelay                    = m_pRay->LastTimeStamp( );
	if( !m_oConf.oExternalSimulation.bSpreadingLoss )
		dSpreadingLoss            = m_pRay->SpreadingLoss( );

	if( !m_oConf.oExternalSimulation.bSourceWFNormal )
		v3SourceWavefrontNormal   = ARTtoVAVector( m_pRay->InitialDirection( ) );
	if( !m_oConf.oExternalSimulation.bReceiverWFNormal )
		v3ReceiverWavefrontNormal = ARTtoVAVector( m_pRay->LastWavefrontNormal( ) );

	if( !m_oConf.oExternalSimulation.bAirAttenuation )
		ITAPropagationModels::Atmosphere::AirAttenuationSpectrum( oAirAttenuationMagnitudes, *m_pRay, m_oStratifiedAtmosphere );

	m_pRay = nullptr;
}


CVAAirTrafficNoiseRenderer::SourceReceiverPair::SourceReceiverPair( const Config& oConf, const CStratifiedAtmosphere& oAtmos )
    : CVASoundPathRendererBase::SourceReceiverPair( oConf )
    , m_oConf( oConf )
    , m_oStratifiedAtmosphere( oAtmos )
{
	m_oARTEngine.eigenraySettings.rayTracing.maxTime                       = 60;
	m_oARTEngine.eigenraySettings.rayAdaptation.advancedRayZooming.bActive = false;
	m_oARTEngine.simulationSettings.bMultiThreading                        = false;
}

void CVAAirTrafficNoiseRenderer::SourceReceiverPair::InitSourceAndReceiver( CVARendererSource* pSource_, CVARendererReceiver* pReceiver_ )
{
	CVASoundPathRendererBase::SourceReceiverPair::InitSourceAndReceiver( pSource_, pReceiver_ );

	std::shared_ptr<SoundPathBase> pDirectPath;
	std::shared_ptr<SoundPathBase> pReflectedPath;
	if( m_oConf.eMediumType == EMediumType::Homogeneous )
	{
		pDirectPath    = std::make_shared<ATNSoundPathHomogeneous>( m_oConf, pSource, pReceiver );
		pReflectedPath = std::make_shared<ATNSoundPathHomogeneous>( m_oConf, pSource, pReceiver );
	}
	else if( m_oConf.eMediumType == EMediumType::Inhomogeneous )
	{
		pDirectPath    = std::make_shared<ATNSoundPathInhomogeneous>( m_oConf, pSource, pReceiver, m_oStratifiedAtmosphere );
		pReflectedPath = std::make_shared<ATNSoundPathInhomogeneous>( m_oConf, pSource, pReceiver, m_oStratifiedAtmosphere );
	}
	else
		VA_EXCEPT2( INVALID_PARAMETER, "Invalid medium type!" );

	pReflectedPath->SetReflectionOrder( 1 );
	AddSoundPath( pDirectPath );
	AddSoundPath( pReflectedPath );
}

void CVAAirTrafficNoiseRenderer::SourceReceiverPair::RunSimulation( )
{
	if( m_oConf.ARTSimulationRequired( ) )
	{
		const VistaVector3D v3SourcePos                   = VAtoARTCoords( pSource->PredictedPosition( ) );
		const VistaVector3D v3ReceiverPos                 = VAtoARTCoords( pReceiver->PredictedPosition( ) );
		std::vector<std::shared_ptr<CARTRay>> vpEigenrays = m_oARTEngine.Run( m_oStratifiedAtmosphere, v3SourcePos, v3ReceiverPos );
		if( vpEigenrays.size( ) < 2 )
			VA_EXCEPT2( INVALID_PARAMETER, "ART simulation returned less than 2 rays!" );

		for( auto pSoundPath: GetSoundPaths( ) )
		{
			auto pSoundPathInhomogeneous = std::dynamic_pointer_cast<ATNSoundPathInhomogeneous>( pSoundPath );
			if( !pSoundPathInhomogeneous )
				VA_EXCEPT2( INVALID_PARAMETER, "Expected sound path to refer to an inhomogeneous medium." );

			if( pSoundPathInhomogeneous->ReflectionOrder( ) )
				pSoundPathInhomogeneous->SetARTRay( vpEigenrays[1] );
			else
				pSoundPathInhomogeneous->SetARTRay( vpEigenrays[0] );
		}
	}
}

void CVAAirTrafficNoiseRenderer::SourceReceiverPair::ExternalUpdate( const CVAStruct& oParams )
{
	assert( GetSoundPaths( ).size( ) == 2 );

	auto pDirectPath    = std::dynamic_pointer_cast<ATNSoundPath>( GetSoundPaths( ).front( ) );
	auto pReflectedPath = std::dynamic_pointer_cast<ATNSoundPath>( GetSoundPaths( ).back( ) );

	if( oParams.HasKey( "direct_path" ) )
		pDirectPath->ExternalUpdate( oParams["direct_path"] );
	if( oParams.HasKey( "reflected_path" ) )
		pReflectedPath->ExternalUpdate( oParams["reflected_path"] );
}


#endif // VACORE_WITH_RENDERER_BINAURAL_AIR_TRAFFIC_NOISE