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

#include "VAAudioRendererRegistry.h"

#include "../VALog.h"
#include "AirTrafficNoise/VAAirTrafficNoiseRenderer.h"
#include "Ambient/Mixer//VAAmbientMixerAudioRenderer.h"
#include "Ambisonics/Freefield/VAAmbisonicsFreefieldAudioRenderer.h"
#include "Binaural/AirTrafficNoise/VAAirTrafficNoiseAudioRenderer.h"
#include "Binaural/ArtificialReverb/VABinauralArtificialReverb.h"
#include "Binaural/Clustering/VABinauralClusteringRenderer.h"
#include "Binaural/FreeField/VABinauralFreefieldAudioRenderer.h"
#include "Binaural/OutdoorNoise/VABinauralOutdoorNoiseAudioRenderer.h"
#include "Freefield/VAFreeFieldRenderer.h"
#include "Prototyping/Dummy/VAPTDummyAudioRenderer.h"
#include "Prototyping/FreeField/VAPrototypeFreeFieldAudioRenderer.h"
#include "Prototyping/GenericPath/VAPTGenericPathAudioRenderer.h"
#include "Prototyping/HearingAid/VAPTHearingAidRenderer.h"
#include "Prototyping/ImageSource/VAPTImageSourceAudioRenderer.h"
#include "RoomAcoustics/VARoomAcousticsRenderer.h"
#include "VBAP/Freefield/VAVBAPFreefieldAudioRenderer.h"

#include <ITAStringUtils.h>
#include <cassert>


static CVAAudioRendererRegistry* g_pInstance;

CVAAudioRendererRegistry* CVAAudioRendererRegistry::GetInstance( )
{
	if( !g_pInstance )
	{
		g_pInstance = new CVAAudioRendererRegistry( );
	}
	return g_pInstance;
}

void CVAAudioRendererRegistry::RegisterFactory( IVAAudioRendererFactory* pFactory )
{
	assert( pFactory );
	m_pFactories.push_back( pFactory );

	VA_TRACE( "AudioRendererRegistry", "Registering renderer factory '" + pFactory->GetClassIdentifier( ) + "'" );
}

IVAAudioRendererFactory* CVAAudioRendererRegistry::FindFactory( const std::string& sClassName )
{
	std::string s = toUppercase( sClassName );
	for( std::vector<IVAAudioRendererFactory*>::iterator it = m_pFactories.begin( ); it != m_pFactories.end( ); ++it )
	{
		IVAAudioRendererFactory* pFactory( *it );
		if( toUppercase( pFactory->GetClassIdentifier( ) ) == s )
			return pFactory;
	}
	return nullptr;
}

void CVAAudioRendererRegistry::RegisterInternalCoreFactoryMethods( )
{
	// A little protection against double factory creation
	if( m_bInternalCoreFactoriesRegistered )
		return;


		// Ambient

#ifdef VACORE_WITH_RENDERER_AMBIENT_MIXER
	RegisterRendererDefaultFactory<CVAAmbientMixerAudioRenderer>( "AmbientMixer" );
#endif

	RegisterRendererDefaultFactory<CVAFreefieldRenderer>( "FreeField" );


	// Ambisonics

#ifdef VACORE_WITH_RENDERER_AMBISONICS_FREE_FIELD
	RegisterRendererDefaultFactory<CVAAmbisonicsFreeFieldAudioRenderer>( "AmbisonicsFreeField" );
#endif


	// Binaural

#ifdef VACORE_WITH_RENDERER_BINAURAL_FREE_FIELD
	RegisterRendererDefaultFactory<CVABinauralFreeFieldAudioRenderer>( "BinauralFreeField" );
#endif // VACORE_WITH_RENDERER_BINAURAL_FREE_FIELD

#ifdef VACORE_WITH_RENDERER_BINAURAL_AIR_TRAFFIC_NOISE
	RegisterRendererDefaultFactory<CVAAirTrafficNoiseRenderer>( "AirTrafficNoise" );
	RegisterRendererDefaultFactory<CVABinauralAirTrafficNoiseAudioRenderer>( "BinauralAirTrafficNoise" );
#endif // VACORE_WITH_RENDERER_BINAURAL_AIR_TRAFFIC_NOISE

#ifdef VACORE_WITH_RENDERER_BINAURAL_ARTIFICIAL_REVERB
	RegisterRendererDefaultFactory<CVABinauralArtificialReverbAudioRenderer>( "BinauralArtificialReverb" );
#endif // VACORE_WITH_RENDERER_BINAURAL_ARTIFICIAL_REVERB

#ifdef VACORE_WITH_RENDERER_ROOM_ACOUSTICS
	RegisterRendererDefaultFactory<CVARoomAcousticsRenderer>( "RoomAcoustics" );
#endif // VACORE_WITH_RENDERER_PROTOTYPE_ROOM_ACOUSTICS

#ifdef VACORE_WITH_RENDERER_BINAURAL_OUTDOOR_NOISE
	RegisterRendererDefaultFactory<CVABinauralOutdoorNoiseRenderer>( "BinauralOutdoorNoise" );
#endif // VACORE_WITH_RENDERER_BINAURAL_OUTDOOR_NOISE


#ifdef VACORE_WITH_RENDERER_BINAURAL_CLUSTERING
	RegisterRendererDefaultFactory<CVABinauralClusteringRenderer>( "BinauralClustering" );
#endif // VACORE_WITH_RENDERER_BINAURAL_CLUSTERING


	// Prototyping

#ifdef VACORE_WITH_RENDERER_PROTOTYPE_FREE_FIELD
	RegisterRendererDefaultFactory<CVAPrototypeFreeFieldAudioRenderer>( "PrototypeFreeField" );
#endif // VACORE_WITH_RENDERER_PROTOTYPE_FREE_FIELD

#ifdef VACORE_WITH_RENDERER_PROTOTYPE_DUMMY
	RegisterRendererDefaultFactory<CVAPTDummyAudioRenderer>( "PrototypeDummy" );
#endif // VACORE_WITH_RENDERER_PROTOTYPE_DUMMY

#ifdef VACORE_WITH_RENDERER_PROTOTYPE_GENERIC_PATH
	RegisterRendererDefaultFactory<CVAPTGenericPathAudioRenderer>( "PrototypeGenericPath" );
	RegisterRendererDefaultFactory<CVAPTGenericPathAudioRenderer>( "GenericFIR" );
#endif // VACORE_WITH_RENDERER_PROTOTYPE_GENERIC_PATH

#ifdef VACORE_WITH_RENDERER_PROTOTYPE_IMAGE_SOURCE
	RegisterRendererDefaultFactory<CVAPTImageSourceAudioRenderer>( "PrototypeImageSource" );
#endif // VACORE_WITH_RENDERER_PROTOTYPE_IMAGE_SOURCE

#ifdef VACORE_WITH_RENDERER_PROTOTYPE_HEARING_AID
	RegisterRendererDefaultFactory<CVAPTHearingAidRenderer>( "PrototypeHearingAid" );
#endif // VACORE_WITH_RENDERER_PROTOTYPE_HEARING_AID


	// VBAP

#ifdef VACORE_WITH_RENDERER_VBAP_FREE_FIELD
	RegisterRendererDefaultFactory<CVAVBAPFreeFieldAudioRenderer>( "VBAPFreeField" );
#endif // VACORE_WITH_RENDERER_VBAP_FREE_FIELD


	m_bInternalCoreFactoriesRegistered = true;
}
