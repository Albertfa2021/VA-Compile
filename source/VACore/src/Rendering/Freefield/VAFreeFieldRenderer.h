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

#ifndef IW_VACORE_FREEFIELDRENDERER
#define IW_VACORE_FREEFIELDRENDERER

#include "../Base/VASoundPathRendererBase.h"


/// Sound path based renderer assuming a homogeneous medium and free-field conditions considering the following acoustic effects:
/// - Spherical spreading loss
/// - Propagation delay / Doppler shift
/// - Source directivity
/// - Spatial encoding at receiver
/// For details on the actual processing, see CVASoundPathRendererBase.
class CVAFreefieldRenderer : public CVASoundPathRendererBase
{
public:
	/// Config for freefield renderer, same as config for SoundPathRendererBase
	struct Config : public CVASoundPathRendererBase::Config
	{
		// Note: For other renderers, additional settings might be required.
		//       Default values can be overwritten by creating a constructor.
		//       Otherwise the default values of the parent classes will be used.

		/// Constructor setting default values
		Config( ) { };
		/// Constructor parsing renderer ini-file parameters
		Config( const CVAAudioRendererInitParams& oParams, const Config& oDefaultValues ) : CVASoundPathRendererBase::Config( oParams, oDefaultValues ) { };
	};

	CVAFreefieldRenderer( const CVAAudioRendererInitParams& oParams );

private:
	const Config m_oConf;

	class FFSoundPath : public CVASoundPathRendererBase::SoundPathBase
	{
	private:
		const Config& m_oConf; /// Copy of renderer config
	public:
		FFSoundPath( const Config& oConf, CVARendererSource* pSource, CVARendererReceiver* pReceiver )
		    : CVASoundPathRendererBase::SoundPathBase( oConf, pSource, pReceiver ), m_oConf(oConf) { };

	private:
		void UpdateAuralizationParameters( double dTimeStamp, const AuralizationMode& oAuraMode, double& dDelay, double& dSpreadingLoss, VAVec3& v3SourceWavefrontNormal,
		                                   VAVec3& v3ReceiverWavefrontNormal, ITABase::CThirdOctaveGainMagnitudeSpectrum& oAirAttenuationMagnitudes,
		                                   ITABase::CThirdOctaveGainMagnitudeSpectrum& ) override;
	};

	class SourceReceiverPair : public CVASoundPathRendererBase::SourceReceiverPair
	{
	private:
		const Config& m_oConf; /// Copy of renderer config
	public:
		SourceReceiverPair( const Config& oConf );
		void InitSourceAndReceiver( CVARendererSource* pSource_, CVARendererReceiver* pReceiver_ ) override;
	};

	class SourceReceiverPairFactory : IVAPoolObjectFactory
	{
	private:
		const Config& m_oConf; /// Copy of renderer config

	public:
		SourceReceiverPairFactory( const CVAFreefieldRenderer::Config& oConf ) : m_oConf( oConf ) { };
		CVAPoolObject* CreatePoolObject( ) override { return new SourceReceiverPair( m_oConf ); }
	};
};


#endif // IW_VACORE_FREEFIELDRENDERER
