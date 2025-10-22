#include "VAATNSourceReceiverTransmissionInhomogeneous.h"

#ifdef VACORE_WITH_RENDERER_BINAURAL_AIR_TRAFFIC_NOISE

#	include <ITAPropagationModels/Atmosphere/AirAttenuation.h>
#	include <vector>


using namespace ITAPropagationPathSim::AtmosphericRayTracing;

VistaVector3D VAtoRTEngineVector( const VAVec3& v3VA )
{
	return VistaVector3D( v3VA.x, -v3VA.z, v3VA.y );
}
VAVec3 RTEngineToVAVector( const VistaVector3D& v3RT )
{
	return VAVec3( v3RT[Vista::X], v3RT[Vista::Z], -v3RT[Vista::Y] );
}

VistaVector3D CVABATNSourceReceiverTransmissionInhomogeneous::VAtoRTEngineCoord( const VAVec3& v3VA )
{
	return VistaVector3D( v3VA.x, -v3VA.z, v3VA.y - dGroundReflectionPlanePosition );
}
VAVec3 CVABATNSourceReceiverTransmissionInhomogeneous::RTEngineToVACoords( const VistaVector3D& v3RT )
{
	return VAVec3( v3RT[Vista::X], v3RT[Vista::Z] + dGroundReflectionPlanePosition, -v3RT[Vista::Y] );
}

CVASourceTargetMetrics CVABATNSourceReceiverTransmissionInhomogeneous::CalculateDirectivityAngles( std::shared_ptr<const CRay> pRay )
{
	CVASourceTargetMetrics oMetrics;
	oMetrics.dDistance = 1.0; // NOTE: shouldn't be used, but make sure this is not zero just in case

	if( pRay )
	{
		const VAVec3 v3VirtualSourcePos = pSoundReceiver->vPredPos - RTEngineToVAVector( pRay->LastWavefrontNormal( ) );
		oMetrics.dAzimuthT2S            = GetAzimuthOnTarget_DEG( pSoundReceiver->vPredPos, pSoundReceiver->vPredView, pSoundReceiver->vPredUp, v3VirtualSourcePos );
		oMetrics.dElevationT2S          = GetElevationOnTarget_DEG( pSoundReceiver->vPredPos, pSoundReceiver->vPredUp, v3VirtualSourcePos );

		const VAVec3 v3VirtualReceiverPos = pSoundSource->vPredPos + RTEngineToVAVector( pRay->InitialDirection( ) );
		oMetrics.dAzimuthS2T              = GetAzimuthOnTarget_DEG( pSoundSource->vPredPos, pSoundSource->vPredView, pSoundSource->vPredUp, v3VirtualReceiverPos );
		oMetrics.dElevationS2T            = GetElevationOnTarget_DEG( pSoundSource->vPredPos, pSoundSource->vPredUp, v3VirtualReceiverPos );
	}

	return oMetrics;
}

CVABATNSourceReceiverTransmissionInhomogeneous::CVABATNSourceReceiverTransmissionInhomogeneous( const ITAGeo::CStratifiedAtmosphere& oAtmosphere, double dSamplerate,
                                                                                                int iBlocklength, int iHRIRFilterLength, int iDirFilterLength,
                                                                                                int iFilterBankType )
    : CVABATNSourceReceiverTransmission( dSamplerate, iBlocklength, iHRIRFilterLength, iDirFilterLength, iFilterBankType )
    , m_oAtmosphere( oAtmosphere )
{
	m_oEigenrayEngine.eigenraySettings.rayTracing.maxTime                       = 45;
	m_oEigenrayEngine.eigenraySettings.rayAdaptation.advancedRayZooming.bActive = false;
	m_oEigenrayEngine.simulationSettings.bMultiThreading                        = false;
}

void CVABATNSourceReceiverTransmissionInhomogeneous::UpdateSoundPaths( )
{
	if( pSoundSource->vPredPos != pSoundReceiver->vPredPos )
	{
		// TODO: Is it necessary to work on a copy here to make sure the data does not change during run-time?
		// const ITAGeo::CStratifiedAtmosphere oAtmosphere = m_oAtmosphere;

		const VistaVector3D v3SourcePos                = VAtoRTEngineCoord( pSoundSource->vPredPos );
		const VistaVector3D v3ReceiverPos              = VAtoRTEngineCoord( pSoundReceiver->vPredPos );
		std::vector<std::shared_ptr<CRay>> vpEigenrays = m_oEigenrayEngine.Run( m_oAtmosphere, v3SourcePos, v3ReceiverPos );

		m_pDirectRay    = ( vpEigenrays.size( ) > 0 ) ? vpEigenrays.front( ) : nullptr;
		m_pReflectedRay = ( vpEigenrays.size( ) > 1 ) ? vpEigenrays[1] : nullptr;

		if( m_pDirectRay )
			oDirectSoundPath.oRelations = CalculateDirectivityAngles( m_pDirectRay );
		if( m_pReflectedRay )
			oReflectedSoundPath.oRelations = CalculateDirectivityAngles( m_pReflectedRay );
	}
}


void CVABATNSourceReceiverTransmissionInhomogeneous::UpdateMediumPropagation( )
{
	if( m_pDirectRay )
		oDirectSoundPath.dPropagationTime = m_pDirectRay->LastTimeStamp( );
	if( m_pReflectedRay )
		oReflectedSoundPath.dPropagationTime = m_pReflectedRay->LastTimeStamp( );
}


void CVABATNSourceReceiverTransmissionInhomogeneous::UpdateAirAttenuation( )
{
	if( m_pDirectRay )
		ITAPropagationModels::Atmosphere::AirAttenuationSpectrum( oDirectSoundPath.oAirAttenuationMagnitudes, *m_pDirectRay, m_oAtmosphere );
	if( m_pReflectedRay )
		ITAPropagationModels::Atmosphere::AirAttenuationSpectrum( oReflectedSoundPath.oAirAttenuationMagnitudes, *m_pReflectedRay, m_oAtmosphere );
}

void CVABATNSourceReceiverTransmissionInhomogeneous::UpdateSpreadingLoss( )
{
	// Gain limiter
	const double MAXIMUM_SPREADINGLOSS_GAIN = db20_to_ratio( 10 );

	if( m_pDirectRay )
		oDirectSoundPath.dGeometricalSpreadingLoss = std::min( m_pDirectRay->SpreadingLoss( ), MAXIMUM_SPREADINGLOSS_GAIN );
	if( m_pReflectedRay )
		oReflectedSoundPath.dGeometricalSpreadingLoss = std::min( m_pReflectedRay->SpreadingLoss( ), MAXIMUM_SPREADINGLOSS_GAIN );
}

#endif // VACORE_WITH_RENDERER_BINAURAL_AIR_TRAFFIC_NOISE
