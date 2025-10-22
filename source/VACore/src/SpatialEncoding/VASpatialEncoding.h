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

#ifndef IW_VACORE_SPATIALENCODING
#define IW_VACORE_SPATIALENCODING

#include <VABase.h>
#include <VAException.h>
#include <string>
#include <map>

class ITASampleFrame;
class ITASampleBuffer;
class CVAReceiverState;

/// Interface for encoding spatial information to an input signal.
/// Can process a 1-channel input signal to an N-channel output depending on the respective backend. In this process, also the overall signal gain is applied.
/// Currently supported backends: Binaural filtering / ambisonics encoding
class IVASpatialEncoding
{
public:
	enum class EType
	{
		Binaural   = 0,
		Ambisonics = 1,
		VBAP = 2
	};
	struct Config
	{
		EType eType                        = EType::Binaural; // Specifies the spatial encoding backend
		int iBlockSize                     = -1;              // BlockSize for audio processing. Should be set by renderer based on the VACore settings.
		int iHRIRFilterLength              = 256;             // Maximum length for binaural filtering
		int iAmbisonicsOrder               = 4;               // Order for ambisonics encoding
		std::string sVBAPLoudSpreakerSetup = "";              // ID of LS setup (see .ini file) used for VBAP encoding
		VAVec3 v3VBAPSetupCenter;                             // Center position of LS setup for VBAP
		std::string sVBAPTriangulationFile = "";              // Filename including triangulation info for VBAP
		int iMDAPSpreadingSources          = 0;               // Number of spreading sources if using MDAP extension for VBAP
		double dMDAPSpreadingAngleDeg      = 0.0;             // Spreading angle if using MDAP extension for VBAP (degrees)
	};

	virtual ~IVASpatialEncoding( ) { };

	/// Resets all DSP elements to their original state (after constructor call)
	virtual void Reset( ) = 0;

	/// <summary>
	/// Spatially encode the input signal depending on incoming direction. Also applies overall gain.
	/// </summary>
	/// <param name="sbInputData">Sample buffer with "raw" input signal</param>
	/// <param name="sfOutput">Multi-channel sample frame to write output data</param>
	/// <param name="fGain">Overall gain to be applied to the input signal</param>
	/// <param name="dAzimuthDeg">Azimuth angle of incident direction in degrees</param>
	/// <param name="dElevationDeg">Elevation angle of incident direction in degrees</param>
	/// <param name="pReceiverState">Contains additional information on the receiver (e.g. HRIR)</param>
	virtual void Process( const ITASampleBuffer& sbInputData, ITASampleFrame& sfOutput, float fGain, double dAzimuthDeg, double dElevationDeg,
	                      const CVAReceiverState& pReceiverState ) = 0;
};

//Forward declarations
class CVAHardwareSetup;
class CVAVBAPLoudspeakerSetup;

/// Factory for creating class objects for different spatial encoding types
class CVASpatialEncodingFactory
{
private:
	inline static std::map<std::string, std::shared_ptr<const CVAVBAPLoudspeakerSetup>> m_mLoudspeakerSetups =
	    std::map<std::string, std::shared_ptr<const CVAVBAPLoudspeakerSetup>>( ); /// Map containing registered VBAP LS setups. Keys are the respective IDs (strings).

public:
	/// Registers a LS setup if given config refers to a VBAP encoding
	static void RegisterLSSetupIfVBAP( const IVASpatialEncoding::Config& oConf, const CVAHardwareSetup& oHardwareSetup );

	/// Factory function to create SpatialEncoding objects. LS Setup must be registered for VBAP before this is called
	static std::unique_ptr<IVASpatialEncoding> Create( IVASpatialEncoding::Config oSpatialEncodingConf );

	/// Returns number of input channels for given config. Make sure LS Setup was registered before calling this
	static int GetNumChannels( const IVASpatialEncoding::Config& oConf );

private:
	/// Constructor disabled (singleton class)
	CVASpatialEncodingFactory( ) = delete;
};

#endif // IW_VACORE_SPATIALENCODING
