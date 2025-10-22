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

#ifndef IW_VACORE_AIRTRAFFICNOISERENDERER
#define IW_VACORE_AIRTRAFFICNOISERENDERER

#ifdef VACORE_WITH_RENDERER_BINAURAL_AIR_TRAFFIC_NOISE

#include "../Base/VASoundPathRendererBase.h"
#include "../../Filtering/VATemporalVariations.h"

// ITA includes
#include <ITAThirdOctaveMagnitudeSpectrum.h>

// ART includes
#include <ITAGeo/Atmosphere/StratifiedAtmosphere.h>
#include <ITAPropagationPathSim/AtmosphericRayTracing/EigenraySearch/EigenrayEngine.h>
#include <ITAPropagationPathSim/AtmosphericRayTracing/Rays.h>
#include <VistaBase/VistaVector3D.h>

typedef ITAPropagationPathSim::AtmosphericRayTracing::EigenraySearch::CEngine CARTEngine;
typedef ITAGeo::CStratifiedAtmosphere CStratifiedAtmosphere;
typedef ITAPropagationPathSim::AtmosphericRayTracing::CRay CARTRay;


/// Sound path based renderer considering direct sound and a single reflection assuming a flat ground which are reasonable assumptions for aircraft flyovers.
/// Either uses straight paths based on image source method (homogeneous medium) or curved sound paths based on the Atmospheric Ray Tracing method (stratified, moving medium).
/// The renderer considers the following acoustic effects:
/// - Spreading loss
/// - Propagation delay / Doppler shift
/// - Source directivity
/// - Air attenuation
/// - Turbulence (temporal variations)
/// - Reflection factor (only for ground reflection)
/// - Spatial encoding at receiver
/// For details on the actual processing, see CVASoundPathRendererBase.
class CVAAirTrafficNoiseRenderer : public CVASoundPathRendererBase
{
public:
	enum class EMediumType
	{
		Homogeneous   = 0, /// A homogeneous medium with straight sound paths
		Inhomogeneous = 1  /// Stratified, moving medium with curved sound paths based on ART framework
	};
	/// Config for AirTrafficNoiseRenderer, allows specifing the medium and ground plane altitude.
	struct Config : public CVASoundPathRendererBase::Config
	{
		EMediumType eMediumType                   = EMediumType::Homogeneous; /// Medium type used for sound path simulations
		std::string sMediumType                   = "homogeneous";            /// String representing medium type in .ini file
		std::string sStratifiedAtmosphereFilepath = "";                       /// Filepath to load a StratifiedAtmosphere configuration from a json file (empty = default)
		double dGroundPlanePosition               = 0.0;                      /// Altitude-position of ground plane (y = ...)

		/// Constructor setting default values
		Config( )
		{
			sVDLSwitchingAlgorithm          = "cubicspline";
			sFilterBankType                 = "iir_biquads_order10";
			oSourceMotionModel.dWindowSize  = 1.0;
			oSourceMotionModel.dWindowDelay = 0.5;
			InitGeometricSoundPathsRequired( );
		};
		/// Constructor parsing renderer ini-file parameters. Takes a given config for default values.
		/// Note, that part of the parsing process is done in the parent renderer's config method.
		Config( const CVAAudioRendererInitParams& oParams, const Config& oDefaultValues );

		/// Returns true, if geometric paths are not required, since all respective auralization parameters are set externally
		bool GeometricSoundPathsRequired( ) const { return m_bGeometricSoundPathsRequired; };
		bool ARTSimulationRequired( ) const { return eMediumType == EMediumType::Inhomogeneous && GeometricSoundPathsRequired( ); };

	private:
		bool m_bGeometricSoundPathsRequired = true;
		void InitGeometricSoundPathsRequired( );
	};

	CVAAirTrafficNoiseRenderer( const CVAAudioRendererInitParams& oParams );

	/// Handles a set rendering module parameters call()
	/// Calls base renderer function. Overload in derived renderers to implement functionality.
	virtual void SetParameters( const CVAStruct& oParams ) override;

private:
	const Config m_oConf;                          /// Renderer configuration parsed from VACore.ini (downcasted copy of child renderer)
	CStratifiedAtmosphere m_oStratifiedAtmosphere; /// Stratified atmosphere used if medium type is inhomogeneous

	/// Base class for sound paths of the AirTrafficNoiseRenderer (specialization for in-/homogeneous medium)
	class ATNSoundPath : public CVASoundPathRendererBase::SoundPathBase
	{
	protected:
		const Config& m_oConf; /// Copy of renderer config

		CVAAtmosphericTemporalVariations oATVGenerator;                         /// Generator for temporal variation magnitudes caused by turbulence
		ITABase::CThirdOctaveGainMagnitudeSpectrum oTurbulenceMagnitudes;       /// Magnitudes for temporal variation due to turbulence
		ITABase::CThirdOctaveGainMagnitudeSpectrum oGroundReflectionMagnitudes; /// Ground reflection magnitudes (only for reflected path)

	public:
		ATNSoundPath( const Config& oConf, CVARendererSource* pSource, CVARendererReceiver* pReceiver );
		/// Called during an external auralization parameter update.
		/// Calls base function and additionally updates air attenuation, turbulence and ground reflection magnitudes
		void ExternalUpdate( const CVAStruct& oUpdate );

	protected:
		void UpdateAuralizationParameters( double dTimeStamp, const AuralizationMode& oAuraMode, double& dDelay, double& dSpreadingLoss, VAVec3& v3SourceWavefrontNormal,
		                                   VAVec3& v3ReceiverWavefrontNormal, ITABase::CThirdOctaveGainMagnitudeSpectrum& oAirAttenuationMagnitudes,
		                                   ITABase::CThirdOctaveGainMagnitudeSpectrum& oSoundPathMagnitudes ) override;
		/// Updates auralization parameters which are medium specific. Overloaded in child classes.
		virtual void UpdateMediumSpecificAuralizationParameters( double dTimeStamp, double& dDelay, double& dSpreadingLoss, VAVec3& v3SourceWavefrontNormal,
		                                                         VAVec3& v3ReceiverWavefrontNormal,
		                                                         ITABase::CThirdOctaveGainMagnitudeSpectrum& oAirAttenuationMagnitudes ) = 0;
	};
	/// Sound path for homogeneous medium
	class ATNSoundPathHomogeneous : public ATNSoundPath
	{
	public:
		ATNSoundPathHomogeneous( const Config& oConf, CVARendererSource* pSource, CVARendererReceiver* pReceiver ) : ATNSoundPath( oConf, pSource, pReceiver ) { };

	protected:
		/// Calculates auralization parameters from straight sound paths. Uses image source method if sound path refers to ground reflection.
		void UpdateMediumSpecificAuralizationParameters( double dTimeStamp, double& dDelay, double& dSpreadingLoss, VAVec3& v3SourceWavefrontNormal,
		                                                 VAVec3& v3ReceiverWavefrontNormal,
		                                                 ITABase::CThirdOctaveGainMagnitudeSpectrum& oAirAttenuationMagnitudes ) override;
	};
	/// Sound path for inhomogeneous medium (based on AtmosphericRayTracing simulations)
	class ATNSoundPathInhomogeneous : public ATNSoundPath
	{
	public:
		ATNSoundPathInhomogeneous( const Config& oConf, CVARendererSource* pSource, CVARendererReceiver* pReceiver, const CStratifiedAtmosphere& oAtmosphere )
		    : ATNSoundPath( oConf, pSource, pReceiver )
		    , m_oStratifiedAtmosphere( oAtmosphere ) { };

		/// Allows the SR-Pair to set the respective ray from an ART simulation
		void SetARTRay( std::shared_ptr<CARTRay> pRay ) { m_pRay = pRay; };

	protected:
		/// Parses the auralization parameters from an ART ray
		void UpdateMediumSpecificAuralizationParameters( double dTimeStamp, double& dDelay, double& dSpreadingLoss, VAVec3& v3SourceWavefrontNormal,
		                                                 VAVec3& v3ReceiverWavefrontNormal,
		                                                 ITABase::CThirdOctaveGainMagnitudeSpectrum& oAirAttenuationMagnitudes ) override;

		std::shared_ptr<CARTRay> m_pRay = nullptr;            /// Pointer to ray from ART simulation
		const CStratifiedAtmosphere& m_oStratifiedAtmosphere; /// Copy of stratified atmophere utilized for simulations
	};

	/// Source-Receiver pair of the AirTrafficNoiseRenderer.
	/// Holds a sound path for direct sound and ground reflection, either corresponding to a homogeneous or inhomogeneous medium.
	class SourceReceiverPair : public CVASoundPathRendererBase::SourceReceiverPair
	{
	private:
		const Config& m_oConf;                                /// Copy of renderer config
		const CStratifiedAtmosphere& m_oStratifiedAtmosphere; /// Copy of renderer atmosphere

		CARTEngine m_oARTEngine; /// ART simulation engine

		VistaVector3D VAtoARTCoords( const VAVec3& v3VA ) { return VistaVector3D( v3VA.x, -v3VA.z, v3VA.y - m_oConf.dGroundPlanePosition ); };
		VAVec3 ARTtoVACoords( const VistaVector3D& v3RT ) { return VAVec3( v3RT[Vista::X], v3RT[Vista::Z] + m_oConf.dGroundPlanePosition, -v3RT[Vista::Y] ); };

		/// Runs an ART simulation if using inhomogeneous medium
		void RunSimulation( ) override;

	public:
		SourceReceiverPair( const Config& oConf, const CStratifiedAtmosphere& oAtmos );
		void InitSourceAndReceiver( CVARendererSource* pSource_, CVARendererReceiver* pReceiver_ ) override;

		void ExternalUpdate( const CVAStruct& oParams ) override;
	};

	class SourceReceiverPairFactory : IVAPoolObjectFactory
	{
	private:
		const Config& m_oConf;                                /// Copy of renderer config
		const CStratifiedAtmosphere& m_oStratifiedAtmosphere; /// Copy of renderer atmosphere

	public:
		SourceReceiverPairFactory( const CVAAirTrafficNoiseRenderer::Config& oConf, const CStratifiedAtmosphere& oAtmos )
		    : m_oConf( oConf )
		    , m_oStratifiedAtmosphere( oAtmos ) { };
		CVAPoolObject* CreatePoolObject( ) override { return new SourceReceiverPair( m_oConf, m_oStratifiedAtmosphere ); }
	};
};

#endif // VACORE_WITH_RENDERER_BINAURAL_AIR_TRAFFIC_NOISE

#endif // IW_VACORE_AIRTRAFFICNOISERENDERER
