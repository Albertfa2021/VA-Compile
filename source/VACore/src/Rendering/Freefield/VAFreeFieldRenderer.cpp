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

#include "VAFreefieldRenderer.h"

#include "../Base/VAAudioRendererSource.h"
#include "../Base/VAAudioRendererReceiver.h"

#include <utility>



CVAFreefieldRenderer::CVAFreefieldRenderer( const CVAAudioRendererInitParams& oParams )
    : CVASoundPathRendererBase( oParams, Config( ) )
    , m_oConf( oParams, Config( ) )
{
	IVAPoolObjectFactory* pFactory = (IVAPoolObjectFactory*)new SourceReceiverPairFactory( m_oConf );
	InitSourceReceiverPairPool( pFactory );
}


void CVAFreefieldRenderer::FFSoundPath::UpdateAuralizationParameters( double, const AuralizationMode& oAuraMode, double& dDelay, double& dSpreadingLoss,
                                                                      VAVec3& v3SourceWavefrontNormal, VAVec3& v3ReceiverWavefrontNormal,
                                                                      ITABase::CThirdOctaveGainMagnitudeSpectrum& oAirAttenuationMagnitudes,
                                                                      ITABase::CThirdOctaveGainMagnitudeSpectrum& )
{
	const VAVec3 v3SourceToReceiver = GetReceiverPos( ) - GetSourcePos( );
	const double dDistance          = v3SourceToReceiver.Length( );
	if( dDistance == 0.0 )
		return;

	dDelay = dDistance / m_oConf.oHomogeneousMedium.dSoundSpeed;

	const double dR = (std::max)( dDistance, m_oConf.oCoreConfig.dDefaultMinimumDistance );
	dSpreadingLoss = 1.0 / dR;

	v3SourceWavefrontNormal   = v3SourceToReceiver / dDistance;
	v3ReceiverWavefrontNormal = v3SourceWavefrontNormal;

	if( oAuraMode.bMediumAbsorption )
		HomogeneousAirAbsorption( dDistance, m_oConf.oHomogeneousMedium, oAirAttenuationMagnitudes );
	else
		oAirAttenuationMagnitudes.SetIdentity( );
}

CVAFreefieldRenderer::SourceReceiverPair::SourceReceiverPair( const Config& oConf ) : CVASoundPathRendererBase::SourceReceiverPair( oConf ), m_oConf( oConf ) {}

void CVAFreefieldRenderer::SourceReceiverPair::InitSourceAndReceiver( CVARendererSource* pSource_, CVARendererReceiver* pReceiver_ )
{
	CVASoundPathRendererBase::SourceReceiverPair::InitSourceAndReceiver( pSource_, pReceiver_ );
	auto pSoundPath = std::make_shared<FFSoundPath>( m_oConf, pSource, pReceiver );
	AddSoundPath( pSoundPath );
}

