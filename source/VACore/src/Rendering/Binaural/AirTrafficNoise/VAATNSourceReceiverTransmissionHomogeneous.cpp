#include "VAATNSourceReceiverTransmissionHomogeneous.h"

#include <ITAISO9613.h>

//#include <ITAPropagationPathSim/AtmosphericRayTracing/EigenraySearch/Engine.h>
//#include <ITAGeo/Atmosphere/StratifiedAtmosphere.h>


CVABATNSourceReceiverTransmissionHomogeneous::CVABATNSourceReceiverTransmissionHomogeneous( const CVAHomogeneousMedium& oMedium, double dSamplerate, int iBlocklength,
                                                                                            int iHRIRFilterLength, int iDirFilterLength, int iFilterBankType )
    : CVABATNSourceReceiverTransmission( dSamplerate, iBlocklength, iHRIRFilterLength, iDirFilterLength, iFilterBankType )
    , m_oHomogeneousMedium( oMedium )
{
}


void CVABATNSourceReceiverTransmissionHomogeneous::UpdateSoundPaths( )
{
	if( pSoundSource->vPredPos != pSoundReceiver->vPredPos )
	{
		oDirectSoundPath.oRelations.Calc( pSoundSource->vPredPos, pSoundSource->vPredView, pSoundSource->vPredUp, pSoundReceiver->vPredPos, pSoundReceiver->vPredView,
		                                  pSoundReceiver->vPredUp );

		// Mirror at reflecting plane
		double dDSourcePosY = pSoundSource->vPredPos.y;
		double dRSourcePosY = dGroundReflectionPlanePosition - ( dDSourcePosY - dGroundReflectionPlanePosition );
		VAVec3 vRSourcePos( pSoundSource->vPredPos.x, dRSourcePosY, pSoundSource->vPredPos.z );

		oReflectedSoundPath.oRelations.Calc( vRSourcePos, pSoundSource->vPredView, pSoundSource->vPredUp, pSoundReceiver->vPredPos, pSoundReceiver->vPredView,
		                                     pSoundReceiver->vPredUp );
	}
}


void CVABATNSourceReceiverTransmissionHomogeneous::UpdateMediumPropagation( )
{
	const double dSpeedOfSound = m_oHomogeneousMedium.dSoundSpeed;

	assert( dSpeedOfSound > 0 );

	// Simple time delay by a direct line-of-sight propagation
	oDirectSoundPath.dPropagationTime    = oDirectSoundPath.oRelations.dDistance / dSpeedOfSound;
	oReflectedSoundPath.dPropagationTime = oReflectedSoundPath.oRelations.dDistance / dSpeedOfSound;
}


void CVABATNSourceReceiverTransmissionHomogeneous::UpdateAirAttenuation( )
{
	const double dTemperatur = m_oHomogeneousMedium.dTemperatureDegreeCentigrade;
	const double dPressure   = m_oHomogeneousMedium.dStaticPressurePascal;
	const double dHumidity   = m_oHomogeneousMedium.dRelativeHumidityPercent;

	ITABase::ISO9613::AtmosphericAbsorption( oDirectSoundPath.oAirAttenuationMagnitudes, oDirectSoundPath.oRelations.dDistance, dTemperatur, dHumidity, dPressure );
	ITABase::ISO9613::AtmosphericAbsorption( oReflectedSoundPath.oAirAttenuationMagnitudes, oReflectedSoundPath.oRelations.dDistance, dTemperatur, dHumidity, dPressure );

	for( int n = 0; n < oDirectSoundPath.oAirAttenuationMagnitudes.GetNumBands( ); n++ )
		oDirectSoundPath.oAirAttenuationMagnitudes.SetMagnitude( n, 1 - oDirectSoundPath.oAirAttenuationMagnitudes[n] );

	for( int n = 0; n < oReflectedSoundPath.oAirAttenuationMagnitudes.GetNumBands( ); n++ )
		oReflectedSoundPath.oAirAttenuationMagnitudes.SetMagnitude( n, 1 - oReflectedSoundPath.oAirAttenuationMagnitudes[n] );
}

void CVABATNSourceReceiverTransmissionHomogeneous::UpdateSpreadingLoss( )
{
	// Gain limiter
	const double MINIMUM_DISTANCE = 1 / db20_to_ratio( 10 );

	double dDDistance = (std::max)( (double)oDirectSoundPath.oRelations.dDistance, MINIMUM_DISTANCE );
	double dRDistance = (std::max)( (double)oReflectedSoundPath.oRelations.dDistance, MINIMUM_DISTANCE );

	assert( dDDistance > 0.0f );
	assert( dRDistance > 0.0f );

	oDirectSoundPath.dGeometricalSpreadingLoss    = 1.0f / dDDistance;
	oReflectedSoundPath.dGeometricalSpreadingLoss = 1.0f / dRDistance;
}
