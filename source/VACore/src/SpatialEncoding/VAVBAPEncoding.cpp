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

#include "VAVBAPEncoding.h"

#include "../Rendering/VBAP/VAPanningFunctions.h"
#include "../Utils/VAUtils.h"
#include "../VALog.h"
#include "VBAP/VAVBAPLoudspeakerSetup.h"

#include <VAException.h>
#include <ITABase/Math/Triangulation.h>
#include <ITASampleBuffer.h>
#include <ITASampleFrame.h>
#include <VistaBase/VistaVector3D.h>
#include <math.h>

//TODO: Update Panning functions
// - functions should work using the CVAVBAPLoudspeakerSetup class instead of triangulation (then remove Triangulation.h)
// - in this context, also get rid of VistaVector3D
//	 -> then VAUtils.h andVistaVector3D.h must not be included anymore


CVAVBAPEncoding::CVAVBAPEncoding( const IVASpatialEncoding::Config& oConf, std::shared_ptr<const CVAVBAPLoudspeakerSetup> pVBAPLoudspeakerSetup )
    : m_pVBAPLoudspeakerSetup( pVBAPLoudspeakerSetup )
    , m_iMDAPSpreadingSources( oConf.iMDAPSpreadingSources )
    , m_dMDAPSpreadingAngle( oConf.dMDAPSpreadingAngleDeg * M_PI / 180.0 )
{
	if( !pVBAPLoudspeakerSetup )
		VA_EXCEPT2( INVALID_PARAMETER, "VBAP Loudspeaker Setup not properly initialized." );
	m_vdLSGainsCurrent.resize( pVBAPLoudspeakerSetup->GetNumChannels( ), 0.0 );
	m_vdLSGainsLast.resize( pVBAPLoudspeakerSetup->GetNumChannels( ), 0.0 );
}

void CVAVBAPEncoding::Process( const ITASampleBuffer& sbInputData, ITASampleFrame& sfOutput, float fGain, double dAzimuthDeg, double dElevationDeg,
                               const CVAReceiverState& )
{
	const VAVec3 v3SourceRWDir = RealWorldSourceDirection( dAzimuthDeg, dElevationDeg );
	UpdateLSGains( fGain, v3SourceRWDir );
	ApplyGainsWithCrossFade( sbInputData, sfOutput );
}



VAVec3 CVAVBAPEncoding::RealWorldSourceDirection( double dAzimuthDeg, double dElevationDeg )
{
	const double dAzimuth      = dAzimuthDeg * M_PI / 180.0;
	const double dElevation    = dElevationDeg * M_PI / 180.0;
	const double dCosElevation = std::cos( dElevation );

	const double dWeightView = std::cos( dAzimuth ) * dCosElevation;
	const double dWeightLeft = std::sin( dAzimuth ) * dCosElevation;
	const double dWeightUp   = std::sin( dElevation );

	// OpenGL default coordinates: front = -z, top = y, right = x:
	return VAVec3( -dWeightLeft, dWeightUp, -dWeightView );
}

void CVAVBAPEncoding::UpdateLSGains( float fGain, const VAVec3& v3RealWorldSourceDir )
{
	for( int idxCh = 0; idxCh < m_vdLSGainsCurrent.size( ); idxCh++ )
	{
		m_vdLSGainsLast[idxCh]    = m_vdLSGainsCurrent[idxCh];
		m_vdLSGainsCurrent[idxCh] = 0.0f;
	}

	bool bOK = true;
	if( m_iMDAPSpreadingSources > 0 )
	{
		std::vector<VAVec3> pvSpreadingSources;
		CalcualteSpreadingSources( v3RealWorldSourceDir, VAVec3( 0, 0, -1 ), VAVec3( 0, 1, 0 ), (float)m_dMDAPSpreadingAngle, m_iMDAPSpreadingSources,
		                           pvSpreadingSources );

		std::vector<VistaVector3D> vvSpreadingSources( m_iMDAPSpreadingSources );
		for( int i = 0; i < m_iMDAPSpreadingSources; i++ )
			vvSpreadingSources[i] = VAVec3ToVistaVector3D( pvSpreadingSources[i] );

		bOK = CalculateMDAPGains3D( m_pVBAPLoudspeakerSetup->GetLSTriangulation().get(), vvSpreadingSources, m_vdLSGainsCurrent );
	}
	else
		bOK = CalculateVBAPGains3D( m_pVBAPLoudspeakerSetup->GetLSTriangulation( ).get( ), VAVec3ToVistaVector3D( v3RealWorldSourceDir ), m_vdLSGainsCurrent );

	if( !bOK )
		VA_WARN( "VBAPEncoding", "Could not calculate VBAP gains" );

	for( int idxCh = 0; idxCh < m_vdLSGainsCurrent.size( ); idxCh++ )
	{
		// Apply virtual gain & compensate LS distance law
		m_vdLSGainsCurrent[idxCh] *= fGain * m_pVBAPLoudspeakerSetup->GetLSDistanceToCenter()[idxCh];
		// Limiter (1.0)
		m_vdLSGainsCurrent[idxCh] = std::min( 1.0, m_vdLSGainsCurrent[idxCh] ); //TODO: Is is really required? Should we give a warning in this case?
	}
}

void CVAVBAPEncoding::ApplyGainsWithCrossFade( const ITASampleBuffer& sbInputData, ITASampleFrame& sfOutput )
{
	const float* pfInputData = sbInputData.data( );
	const int iBlocklength   = sbInputData.GetLength( );
	const int iNumChannels   = sfOutput.channels( );

	for( unsigned int idxCh = 0; idxCh < iNumChannels; idxCh++ )
	{
		float* pfOutputData = sfOutput[idxCh].GetData( );

		if( ( m_vdLSGainsLast[idxCh] == 0 ) && ( m_vdLSGainsCurrent[idxCh] == 0 ) )
			continue;

		const double dDeltaGain = m_vdLSGainsLast[idxCh] - m_vdLSGainsCurrent[idxCh];

		for( int k = 0; k < iBlocklength; k++ )
		{
			const double dCos = std::cos( k / iBlocklength * M_PI_2 );
			pfOutputData[k] += pfInputData[k] * float( dCos * dCos * dDeltaGain + m_vdLSGainsCurrent[idxCh] );
		}

	}
}

