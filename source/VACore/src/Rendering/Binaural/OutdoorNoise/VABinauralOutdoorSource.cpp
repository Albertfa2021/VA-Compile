#include "VABinauralOutdoorSource.h"

#include "../../../Utils/VAUtils.h"
#include "../../../directivities/VADirectivityDAFFEnergetic.h"

#include <DAFFContentMS.h>

CVABinauralOutdoorSource::CVABinauralOutdoorSource( int iMaxDelaySamples )
{
	pVDL = new CITASIMOVariableDelayLineBase( iMaxDelaySamples );

	pState       = nullptr;
	pData        = nullptr;
	pMotionModel = nullptr;
}

CVABinauralOutdoorSource::~CVABinauralOutdoorSource( )
{
	delete pVDL;
}

ITABase::CThirdOctaveGainMagnitudeSpectrum CVABinauralOutdoorSource::GetDirectivitySpectrum( const double& dTime, const VAVec3& v3SourceWFNormal )
{
	VAVec3 v3SourceView, v3SourceUp;
	pMotionModel->EstimateOrientation( dTime, v3SourceView, v3SourceUp );
	return GetDirectivitySpectrum( v3SourceView, v3SourceUp, v3SourceWFNormal );
}

ITABase::CThirdOctaveGainMagnitudeSpectrum CVABinauralOutdoorSource::GetDirectivitySpectrum( const VAVec3& v3SourceView, const VAVec3& v3SourceUp,
                                                                                             const VAVec3& v3SourceWFNormal )
{
	ITABase::CThirdOctaveGainMagnitudeSpectrum oOutSpectrum;
	CVADirectivityDAFFEnergetic* pSourceDirectivity = (CVADirectivityDAFFEnergetic*)pDirectivity;
	if( !pSourceDirectivity || !pSourceDirectivity->GetDAFFContent( ) )
	{
		oOutSpectrum.SetIdentity( );
		return oOutSpectrum;
	}

	double dAzimuth   = GetAzimuthFromDirection_DEG( v3SourceView, v3SourceUp, v3SourceWFNormal );
	double dElevation = GetElevationFromDirection_DEG( v3SourceUp, v3SourceWFNormal );

	// Update directivity data set
	int idxDirectivity;
	pSourceDirectivity->GetDAFFContent( )->getNearestNeighbour( DAFF_OBJECT_VIEW, float( dAzimuth ), float( dElevation ), idxDirectivity );
	std::vector<float> vfGains( ITABase::CThirdOctaveMagnitudeSpectrum::GetNumBands( ) );
	pSourceDirectivity->GetDAFFContent( )->getMagnitudes( idxDirectivity, 0, &vfGains[0] );

	oOutSpectrum.SetMagnitudes( vfGains );
	return oOutSpectrum;
}
