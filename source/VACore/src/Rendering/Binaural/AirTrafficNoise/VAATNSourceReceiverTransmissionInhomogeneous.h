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

#ifndef IW_VACORE_BINAURAL_ATN_SOURCE_RECEIVER_TRANSMISSION_INHOMOGENEOUS
#define IW_VACORE_BINAURAL_ATN_SOURCE_RECEIVER_TRANSMISSION_INHOMOGENEOUS


#ifdef VACORE_WITH_RENDERER_BINAURAL_AIR_TRAFFIC_NOISE

#	include "VAATNSourceReceiverTransmission.h"

#	include <ITAGeo/Atmosphere/StratifiedAtmosphere.h>
#	include <ITAPropagationPathSim/AtmosphericRayTracing/EigenraySearch/EigenrayEngine.h>
#	include <ITAPropagationPathSim/AtmosphericRayTracing/Rays.h>
#	include <VistaBase/VistaVector3D.h>

// STL includes
#	include <memory>

//! Represents the sound transmission between a single source and a binaural receiver based on a simple image source model
/**
 * Two sound paths (direct + reflection) are considered for the total sound transmission
 */
class CVABATNSourceReceiverTransmissionInhomogeneous : public CVABATNSourceReceiverTransmission
{
	// Conversion methods
private:
	//! Converts a position in the VA world (OpenGL) to a position for atmospheric ray tracing (Mathematic coord system) considering the VA ground offset
	VistaVector3D VAtoRTEngineCoord( const VAVec3& v3VA );
	//! Converts a position in the atmospheric ray tracing world (Mathematic coord system) to a position for VA (OpenGL) considering the VA ground offset
	VAVec3 RTEngineToVACoords( const VistaVector3D& v3RT );
	//! Calculates the directivity angles using the corresponding view/up vectors and the wavefront normals of the given ray
	CVASourceTargetMetrics CalculateDirectivityAngles( std::shared_ptr<const ITAPropagationPathSim::AtmosphericRayTracing::CRay> pRay );

public:
	inline virtual ~CVABATNSourceReceiverTransmissionInhomogeneous( ) { };


	//! Executes ray tracing to calculate eigenrays (sound paths)
	virtual void UpdateSoundPaths( );

	//! Aktualisiert die Richtcharakteristik auf dem Pfad
	// void UpdateSoundSourceDirectivity();

	//! Aktualisiert die Mediumsausbreitung auf dem Pfad
	virtual void UpdateMediumPropagation( );

	//! Updates the temporal variations model of medium shift fluctuation
	// void UpdateTemporalVariation();

	//! Updates the air attenuation (damping)
	virtual void UpdateAirAttenuation( );

	//! Aktualisiert den Lautstärkeabfall nach dem 1/r-Gesetzt (Inverse Distance Decrease/Law)
	virtual void UpdateSpreadingLoss( );

	//! Aktualisiert die HRIR auf dem Pfad
	// void UpdateSoundReceiverDirectivity();

private:
	//! Reference to atmosphere used in AirTrafficNoiseRenderer
	const ITAGeo::CStratifiedAtmosphere& m_oAtmosphere;
	//! Simulation engine
	ITAPropagationPathSim::AtmosphericRayTracing::EigenraySearch::CEngine m_oEigenrayEngine;

	std::shared_ptr<const ITAPropagationPathSim::AtmosphericRayTracing::CRay> m_pDirectRay;
	std::shared_ptr<const ITAPropagationPathSim::AtmosphericRayTracing::CRay> m_pReflectedRay;

	//! Standard-Konstruktor deaktivieren
	CVABATNSourceReceiverTransmissionInhomogeneous( );

	//! Konstruktor
	CVABATNSourceReceiverTransmissionInhomogeneous( const ITAGeo::CStratifiedAtmosphere& oAtmosphere, double dSamplerate, int iBlocklength, int iHRIRFilterLength,
	                                                int iDirFilterLength, int iFilterBankType = CITAThirdOctaveFilterbank::IIR_BIQUADS_ORDER10 );

	friend class CVABATNSourceReceiverTransmissionInhomogeneousFactory;
};

// Factory

class CVABATNSourceReceiverTransmissionInhomogeneousFactory : public CVABATNSourceReceiverTransmissionFactory
{
private:
	//! Reference to atmosphere used in AirTrafficNoiseRenderer
	const ITAGeo::CStratifiedAtmosphere& m_oAtmosphere;

public:
	inline CVABATNSourceReceiverTransmissionInhomogeneousFactory( const ITAGeo::CStratifiedAtmosphere& oAtmosphere, double dSamplerate, int iBlocklength,
	                                                              int iHRIRFilterLength, int iDirFilterLength,
	                                                              int iFilterBankType = CITAThirdOctaveFilterbank::IIR_BIQUADS_ORDER10 )
	    : CVABATNSourceReceiverTransmissionFactory( dSamplerate, iBlocklength, iHRIRFilterLength, iDirFilterLength, iFilterBankType )
	    , m_oAtmosphere( oAtmosphere ) { };

	inline CVAPoolObject* CreatePoolObject( )
	{
		return new CVABATNSourceReceiverTransmissionInhomogeneous( m_oAtmosphere, m_dSamplerate, m_iBlocklength, m_iHRIRFilterLength, m_iDirFilterLength,
		                                                           m_iFilterBankType );
	};
};

#endif // VACORE_WITH_RENDERER_BINAURAL_AIR_TRAFFIC_NOISE

#endif // IW_VACORE_BINAURAL_ATN_SOURCE_RECEIVER_TRANSMISSION_INHOMOGENEOUS
