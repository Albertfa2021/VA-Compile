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

#ifndef IW_VACORE_SOUNDPATHRENDERERBASE
#define IW_VACORE_SOUNDPATHRENDERERBASE

// VA includes
#include "../../SpatialEncoding/VASpatialEncoding.h"
#include "VAAudioRendererBase.h"
#include "VAAudioRendererSourceDirectivityState.h"

// ITA includes
#include <ITAThirdOctaveMagnitudeSpectrum.h>

// STD includes
#include <list>
#include <memory>

// VA forwrads
class CVASoundSourceState;
class CVAReceiverState;

// ITA forwards
class CITAVariableDelayLine;
class CITAThirdOctaveFilterbank;


/// Base class for renderers using sound paths (instead of IRs).
/// Defines a source-receiver pair that holds potentially N sound paths. Audio processing of a source-receiver pair is done by processing each sound path individually. Each sound path holds a set of auralization parameters:
/// - Propagation delay
/// - Spreading loss
/// - Wavefront normal at source
/// - Wavefront normal at receiver
/// - Third-octave magnitude spectrum representing sound propagation effects
/// Those parameters must be calculated in derived renderers by overloading the UpdateAuralizationParameters() method in a derived sound path class.
/// The actual audio processing is done in this renderer including:
/// - Source directivity
/// - Setting overall gain (also checking for muted sources/receivers)
/// - Doppler shift via VDL
/// - Third-octave filter bank based on magnitude spectrum (source directivity + sound propagation)
/// - Spatial encoding (binaural synthesis / ambisonics encoding)
/// Additionally, this renderer handles most auralization modes. To distinguish between direct sound, reflections and diffraction, the reflection and diffraction order should be set when creating sound paths in derived renderers.
/// NOTE: All other auralization modes regarding sound propagation must be handled in derived renderers!
class CVASoundPathRendererBase : public CVAAudioRendererBase
{
public:
	struct Config : public CVAAudioRendererBase::Config
	{
		std::string sSpatialEncodingType = "Binaural"; /// String representing spatial encoding type in .ini file
		IVASpatialEncoding::Config oSpatialEncoding;   /// Config for spatial encoding
		std::string sFilterBankType = "IIR";           /// String representing filter bank type in .ini file
		int iFilterBankType         = -1;              /// Actual filter bank type (set during parsing .ini file)
		bool bAuralizationParameterHistory = false;    /// If enabled, uses histories for auralization parameters instead of setting them directly

		struct ExternalSimulation
		{
			bool bPropagationDelay = false; /// If true propagation delay is only updated via SetParameters()
			bool bSpreadingLoss    = false; /// If true, spreading loss is only updated via SetParameters()
			bool bSourceWFNormal   = false; /// If true, wavefront normal at source is only updated via SetParameters()
			bool bReceiverWFNormal = false; /// If true, wavefront normal at receiver is only updated via SetParameters()
			bool bAirAttenuation   = false; /// If true, air attenuation magnitudes are only updated via SetParameters()
			bool bTurbulence       = false; /// If true, turbulence magnitudes are only updated via SetParameters()
			bool bReflection       = false; /// If true, reflection magnitudes are only updated via SetParameters()
			bool bDiffraction      = false; /// If true, diffraction magnitudes are only updated via SetParameters()

			bool bDirectivity = false; /// If true, source directivity magnitudes are only updated via SetParameters()
		} oExternalSimulation;

		/// Default constructor setting default values
		Config( ) { };
		/// Constructor parsing renderer ini-file parameters. Takes a given config for default values.
		/// Note, that part of the parsing process is done in the parent renderer's config method.
		Config( const CVAAudioRendererInitParams& oParams, const Config& oDefaultValues );
	};

	/// Constructor expecting ini-file renderer parameters and a config object with default values which are used if a setting is not specified in ini-file.
	CVASoundPathRendererBase( const CVAAudioRendererInitParams& oParams, const Config& oDefaultValues );

	/// Interface to load a user requested scene (e.g. file to geometric data).
	/// Does nothing per default. Overload in derived renderers to implement functionality.
	virtual void LoadScene( const std::string& ) override { };
	/// Handles a set rendering module parameters call()
	/// Calls base renderer function. Overload in derived renderers to implement functionality.
	virtual void SetParameters( const CVAStruct& oParams ) override;
	
	/// Handles a get rendering module parameters call()
	/// Returns an empty struct per default. Overload in derived renderers to implement functionality.
	virtual CVAStruct GetParameters( const CVAStruct& ) const override { return CVAStruct( ); };

private:
	const Config m_oConf; /// Renderer configuration parsed from VACore.ini (downcasted copy of child renderer)

protected:
	class SoundPathBase
	{
	private:
		CVARendererSource* m_pSource     = nullptr; /// Pointer to source
		CVARendererReceiver* m_pReceiver = nullptr; /// Pointer to receiver

		const Config& m_oConf;         /// Copy of renderer config
		int m_iReflectionOrder  = 0;   /// Order referring to number of reflections
		int m_iDiffractionOrder = 0;   /// Order referring to number of edge diffractions
		double m_dGain          = 0.0; /// Actual gain used for processing

		double m_dPropagationDelay = -1.0;                                      /// Time that the sound wave required to propagate to the receiver
		double m_dSpreadingLoss    = 0.0;                                       /// Geometrical spreading loss (usually 1-by-R distance law)
		VAVec3 m_v3SourceWavefrontNormal;                                       /// Outgoing wavefront normal at source
		VAVec3 m_v3ReceiverWavefrontNormal;                                     /// Incoming wavefront normal at receiver
		ITABase::CThirdOctaveGainMagnitudeSpectrum m_oAirAttenuationMagnitudes; /// Magnitude spectrum representing air attenuation
		ITABase::CThirdOctaveGainMagnitudeSpectrum
		    m_oSoundPathMagnitudes; /// Accumulated magnitude spectrum caused by sound propagation (without air attenuation and directivities)

		CVARendererSourceDirectivityState m_oDirectivityStateCur;                  /// Current status for source directivity
		CVARendererSourceDirectivityState m_oDirectivityStateNew;                  /// New status for source directivity
		ITABase::CThirdOctaveGainMagnitudeSpectrum m_oSourceDirectivityMagnitudes; /// Magnitudes for source directivity

		std::unique_ptr<CITAVariableDelayLine> m_pVariableDelayLine         = nullptr; /// DSP element realizing propagation delay and doppler shift
		std::unique_ptr<CITAThirdOctaveFilterbank> m_pThirdOctaveFilterBank = nullptr; /// DSP element for filtering the accumulated third-octave magnitudes
		std::unique_ptr<IVASpatialEncoding> m_pSpatialEncoding              = nullptr; /// DSP element for rendering a spatial encoded signal

	public:
		SoundPathBase( const Config& oConf, CVARendererSource* pSource, CVARendererReceiver* pReceiver );
		virtual ~SoundPathBase( );

		void SetReflectionOrder( int iOrder );
		void SetDiffractionOrder( int iOrder );
		bool IsDirectSound( ) const { return ( m_iReflectionOrder + m_iDiffractionOrder ) == 0; };
		int ReflectionOrder( ) const { return m_iReflectionOrder; };
		int DiffractionOrder( ) const { return m_iDiffractionOrder; };

		const VAVec3& GetSourcePos( ) const;
		const VAVec3& GetReceiverPos( ) const;

		/// Defines the general processing order for sound paths
		void Process( double dTimeStamp, const AuralizationMode& oAuraMode, const CVASoundSourceState& oSourceState, const CVAReceiverState& oReceiverState );
		/// Called during an external auralization parameter update.
		/// Updates propagation delay, spreading loss and source directivity magnitudes
		virtual void ExternalUpdate( const CVAStruct& oUpdate );

	private:
		/// Updates the parameters of the DSP elements using the auralization parameters updated by the derived class
		void UpdateDSPElements( const AuralizationMode& oAuralizationMode, const CVASoundSourceState& oSourceState );
		/// Processes the DSP elements and adds the output to the receiver's sample buffer
		void ProcessDSPElements( const AuralizationMode& oAuralizationMode, const CVAReceiverState& oReceiverState );

		/// Updates m_oSourceDirectivityMagnitudes (called in UpdateDSPElements())
		void UpdateSourceDirectivity( bool bSourceDirEnabled, const CVASoundSourceState& oSourceState );

	protected:

		/// <summary>
		/// Overload in derived class to set the auralization parameters during audio processing
		/// </summary>
		/// <param name="dTimeStamp">[Input] Refers to time sound arrives at receiver (required when using DataHistories)</param>
		/// <param name="dDelay">[Output] Propagation delay in seconds</param>
		/// <param name="dSpreadingLoss">[Output] Factor referring to spreading loss</param>
		/// <param name="v3SourceWavefrontNormal">[Output] Outgoing direction of sound wave at source</param>
		/// <param name="v3ReceiverWavefrontNormal">[Output] Incoming direction of sound wave at receiver</param>
		/// <param name="oAirAttenuationMagnitudes">[Output] One-third octave magnitudes caused by air attenuation</param>
		/// <param name="oSoundPathMagnitudes">[Output] One-third octave magnitudes caused by all other sound propagation effects (excluding directivities and air attenuation)</param>
		virtual void UpdateAuralizationParameters( double dTimeStamp, const AuralizationMode& oAuraMode, double& dDelay, double& dSpreadingLoss,
		                                           VAVec3& v3SourceWavefrontNormal, VAVec3& v3ReceiverWavefrontNormal,
		                                           ITABase::CThirdOctaveGainMagnitudeSpectrum& oAirAttenuationMagnitudes,
		                                           ITABase::CThirdOctaveGainMagnitudeSpectrum& oSoundPathMagnitudes ) = 0;
		/// Calculates the air attenuation according to ISO9613-1 assuming a homogeneous medium.
		void HomogeneousAirAbsorption( double dDistance, const CVAHomogeneousMedium& oMedium, ITABase::CThirdOctaveGainMagnitudeSpectrum& oOutputSpectrum );

		/// Helper function to parse a VAStruct into a third-octave magnitude spectrum
		static void ParseSpectrum( const CVAStructValue& oSpectrumData, ITABase::CThirdOctaveGainMagnitudeSpectrum& oSpectrum );
		static void ParseVAVec( const CVAStructValue& o3DVecData, VAVec3& v3Vec );
	};

	class SourceReceiverPair : public CVAAudioRendererBase::SourceReceiverPair
	{
	private:
		const Config& m_oConf;                                    /// Copy of renderer config
		std::list<std::shared_ptr<SoundPathBase>> m_lpSoundPaths; /// List of sound paths belonging to this source-receiver pair

	protected:
		const std::list<std::shared_ptr<SoundPathBase>>& GetSoundPaths( ) const { return m_lpSoundPaths; };
		void AddSoundPath( std::shared_ptr<SoundPathBase> pSoundPath ) { m_lpSoundPaths.push_back( pSoundPath ); };

		/// Implement this in derived classes to run a sound path simulation just before sound paths are processed
		virtual void RunSimulation( ) { };

	public:
		SourceReceiverPair( const Config& oConf ) : m_oConf( oConf ) { };
		/// Pool Constructor
		virtual void PreRequest( ) override;
		/// Pool Destructor
		virtual void PreRelease( ) override;

		/// If implemented, runs a simulation for this SR-pair. Then, loops through all sound paths and calls their Process() method
		void Process( double dTimeStamp, const AuralizationMode& oAuraMode, const CVASoundSourceState& oSourceState, const CVAReceiverState& oReceiverState ) override;

		/// Overload in derived renderers to allow external update of auralization parameters via SetParameters(). Should call ExternalUpdate() function of sound paths.
		virtual void ExternalUpdate( const CVAStruct& oParams ) { };
	};
};

#endif // IW_VACORE_SOUNDPATHRENDERERBASE
