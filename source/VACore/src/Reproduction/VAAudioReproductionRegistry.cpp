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

#include "VAAudioReproductionRegistry.h"

#include "../VALog.h"
#include "Ambisonics/VAReproductionAmbisonics.h"
#include "Ambisonics/VAReproductionAmbisonicsBinauralMixdown.h"
#include "Binaural/VAReproductionNCTC.h"
#include "Mixer/VAReproductionBinauralMixdown.h"
#include "Mixer/VAReproductionHeadphones.h"
#include "Mixer/VAReproductionLowFrequencyMixer.h"
#include "Mixer/VAReproductionTalkthrough.h"

#include <ITAStringUtils.h>

static CVAAudioReproductionRegistry* g_pReproductionRegistryInstance;

CVAAudioReproductionRegistry* CVAAudioReproductionRegistry::GetInstance( )
{
	if( !g_pReproductionRegistryInstance )
		g_pReproductionRegistryInstance = new CVAAudioReproductionRegistry( );
	return g_pReproductionRegistryInstance;
}

CVAAudioReproductionRegistry::CVAAudioReproductionRegistry( ) : m_bInternalCoreFactoriesRegistered( false ) {}

void CVAAudioReproductionRegistry::RegisterFactory( IVAAudioReproductionFactory* pFactory )
{
	assert( pFactory );
	m_pFactories.push_back( pFactory );

	VA_TRACE( "AudioReproductionModuleRegistry", "Registering reproduction factory '" + pFactory->GetClassIdentifier( ) + "'" );
}

IVAAudioReproductionFactory* CVAAudioReproductionRegistry::FindFactory( const std::string& sClassName )
{
	std::string s = toUppercase( sClassName );
	for( std::vector<IVAAudioReproductionFactory*>::iterator it = m_pFactories.begin( ); it != m_pFactories.end( ); ++it )
	{
		if( toUppercase( ( *it )->GetClassIdentifier( ) ) == s )
			return *it;
	}
	return nullptr;
}

void CVAAudioReproductionRegistry::RegisterInternalCoreFactoryMethods( )
{
	// A little protection against double factory creation
	if( m_bInternalCoreFactoriesRegistered )
		return;

		// Ambisonics
#ifdef VACORE_WITH_REPRODUCTION_AMBISONICS
	RegisterReproductionDefaultFactory<CVAAmbisonicsReproduction>( "Ambisonics" );
#endif

	// Binaural
#ifdef VACORE_WITH_REPRODUCTION_AMBISONICS_BINAURAL_MIXDOWN
	RegisterReproductionDefaultFactory<CVAAmbisonicsBinauralMixdownReproduction>( "AmbisonicsBinauralMixdown" );
#endif
#ifdef VACORE_WITH_REPRODUCTION_BINAURAL_MIXDOWN
	RegisterReproductionDefaultFactory<CVABinauralMixdownReproduction>( "BinauralMixdown" );
#endif
#ifdef VACORE_WITH_REPRODUCTION_HEADPHONES
	RegisterReproductionDefaultFactory<CVAHeadphonesReproduction>( "Headphones" );
#endif
#ifdef VACORE_WITH_REPRODUCTION_BINAURAL_NCTC
	RegisterReproductionDefaultFactory<CVANCTCReproduction>( "NCTC" );
#endif

	// Talkthrough
#ifdef VACORE_WITH_REPRODUCTION_TALKTHROUGH
	RegisterReproductionDefaultFactory<CVAReproductionTalkthrough>( "Talkthrough" );
#endif

	// Mixer
#ifdef VACORE_WITH_REPRODUCTION_MIXER_LOW_FREQUENCY
	RegisterReproductionDefaultFactory<CVAReproductionLowFrequencyMixer>( "LowFrequencyMixer" );
#endif

	m_bInternalCoreFactoriesRegistered = true;
}
