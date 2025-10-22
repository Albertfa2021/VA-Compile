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

#include "VAAmbisonicsDecoders.h"

#include "../../Rendering/VBAP/VAPanningFunctions.h"

#include <ITABase/Math/Triangulation.h>
#include <ITAConstants.h>
#include <ITANumericUtils.h>

# include "Eigen/SVD"

using namespace ITABase::Math;

void CalculateSADMatrix( const CVAHardwareOutput* pOutput, const VAVec3& reproductionCenterPos, int reproductionOrder, Eigen::MatrixXd& decoder )
{
	int iNumOutputDevices = pOutput->vpDevices.size( ), iNumHoaChannels = ( reproductionOrder + 1 ) * ( reproductionOrder + 1 );
	decoder.setZero( iNumHoaChannels, iNumOutputDevices );

	std::vector<double> vdSHLoudspeaker_i;

	for( size_t i = 0; i < iNumOutputDevices; i++ )
	{
		const CVAHardwareDevice* pDevice = pOutput->vpDevices[i];
		GetSHAtOutputDevice( pDevice->vPos, reproductionCenterPos, reproductionOrder, vdSHLoudspeaker_i );

		for( size_t j = 0; j < vdSHLoudspeaker_i.size( ); j++ )
		{
			decoder( j, i ) = 4 * ITAConstants::PI_D / iNumOutputDevices * vdSHLoudspeaker_i[j];
		}
	}

	decoder.transposeInPlace( );
}

void CalculateMMADMatrix( const CVAHardwareOutput* pOutput, const VAVec3& reproductionCenterPos, int reproductionOrder, Eigen::MatrixXd& decoder, double invTolerance )
{
	int iNumOutputDevices = pOutput->vpDevices.size( ), iNumHoaChannels = ( reproductionOrder + 1 ) * ( reproductionOrder + 1 );
	decoder.setZero( iNumHoaChannels, iNumOutputDevices );

	std::vector<double> vdSHLoudspeaker_i;

	for( size_t i = 0; i < iNumOutputDevices; i++ )
	{
		const CVAHardwareDevice* pDevice = pOutput->vpDevices[i];
		GetSHAtOutputDevice( pDevice->vPos, reproductionCenterPos, reproductionOrder, vdSHLoudspeaker_i );

		for( size_t j = 0; j < vdSHLoudspeaker_i.size( ); j++ )
		{
			decoder( j, i ) = vdSHLoudspeaker_i[j];
		}
	}

	Eigen::JacobiSVD<Eigen::MatrixXd> svd( decoder, Eigen::ComputeFullU | Eigen::ComputeFullV );

	Eigen::MatrixXd singularValues = svd.singularValues( );
	Eigen::MatrixXd singularValues_inv;
	singularValues_inv = singularValues_inv.Zero( decoder.cols( ), decoder.rows( ) );

	for( int i = 0; i < singularValues.size( ); ++i )
	{
		if( singularValues( i ) > invTolerance )
			singularValues_inv( i, i ) = 1.0 / singularValues( i );
		else
			singularValues_inv( i, i ) = 0;
	}

	Eigen::MatrixXd V = svd.matrixV( );
	Eigen::MatrixXd U = svd.matrixU( );

	Eigen::MatrixXd Diag = singularValues_inv;

	Eigen::MatrixXd S1     = V * Diag;
	Eigen::MatrixXd UTrans = U.transpose( );

	decoder = S1 * UTrans;
}

void CalculateEPADMatrix( const CVAHardwareOutput* pOutput, const VAVec3& reproductionCenterPos, int reproductionOrder, Eigen::MatrixXd& decoder )
{
	int iNumOutputDevices = pOutput->vpDevices.size( ), iNumHoaChannels = ( reproductionOrder + 1 ) * ( reproductionOrder + 1 );
	decoder.setZero( iNumHoaChannels, iNumOutputDevices );

	std::vector<double> vdSHLoudspeaker_i;

	for( size_t i = 0; i < iNumOutputDevices; i++ )
	{
		const CVAHardwareDevice* pDevice = pOutput->vpDevices[i];
		GetSHAtOutputDevice( pDevice->vPos, reproductionCenterPos, reproductionOrder, vdSHLoudspeaker_i );

		for( size_t j = 0; j < vdSHLoudspeaker_i.size( ); j++ )
		{
			decoder( j, i ) = vdSHLoudspeaker_i[j];
		}
	}

	Eigen::JacobiSVD<Eigen::MatrixXd> svd( decoder, Eigen::ComputeThinU | Eigen::ComputeThinV );

	Eigen::MatrixXd V = svd.matrixV( );
	Eigen::MatrixXd U = svd.matrixU( );

	Eigen::MatrixXd UTrans = U.transpose( );

	decoder = V * UTrans;
}

void CalculateAllRADMatrix( const CVAHardwareOutput* pOutput, const CVAHardwareOutput* pVirtualOutput, const VAVec3& reproductionCenterPos,
                            const CTriangulation* pOutputTriangulation, int reproductionOrder, Eigen::MatrixXd& decoder )
{
	int iNumOutputDevices = pOutput->vpDevices.size( ), iNumVirtualDevices = pVirtualOutput->vpDevices.size( ),
	    iNumHoaChannels = ( reproductionOrder + 1 ) * ( reproductionOrder + 1 );

	/* virtual HOA decoding */
	Eigen::MatrixXd virtualDecoder;
	CalculateSADMatrix( pVirtualOutput, reproductionCenterPos, reproductionOrder, virtualDecoder );

	/* calculate VBAP matrix*/
	Eigen::MatrixXd matVBAP( iNumOutputDevices, iNumVirtualDevices );
	for( int i = 0; i < iNumVirtualDevices; i++ )
	{
		const CVAHardwareDevice* pVirtualDevice = pVirtualOutput->vpDevices[i];
		VistaVector3D v3VirtualSourcePos( pVirtualDevice->vPos.x, pVirtualDevice->vPos.y, pVirtualDevice->vPos.z );
		std::vector<double> vdLsGains;
		CalculateVBAPGains3D( pOutputTriangulation, v3VirtualSourcePos, vdLsGains );

		for( size_t j = 0; j < vdLsGains.size( ); j++ )
		{
			matVBAP( j, i ) = vdLsGains[j];
		}
	}

	/* construct decoder matrix */
	decoder = matVBAP * virtualDecoder;
}

void CalculateAllRADPlusMatrix( const CVAHardwareOutput* pOutput, const CVAHardwareOutput* pVirtualOutput, const VAVec3& reproductionCenterPos,
                                const CTriangulation* pOutputTriangulation, int reproductionOrder, Eigen::MatrixXd& decoder )
{
	/* SAD decoding */
	Eigen::MatrixXd sadDecoder;
	CalculateSADMatrix( pOutput, reproductionCenterPos, reproductionOrder, sadDecoder );

	/* AllRAD decding */
	Eigen::MatrixXd allradDecoder;
	CalculateAllRADMatrix( pOutput, pVirtualOutput, reproductionCenterPos, pOutputTriangulation, reproductionOrder, allradDecoder );

	/* construct decoder matrix */
	decoder = allradDecoder / std::sqrt( 2 ) + sadDecoder / std::sqrt( 8 );
}


void GetSHAtOutputDevice( const VAVec3& vv3DevicePosition, const VAVec3& v3ReproductionCenterPos, int N, std::vector<double>& vdY )
{
	VAVec3 view( 0, 0, -1 ), up( 0, 1, 0 );
	double dAzimuth = GetAzimuthOnTarget_DEG( v3ReproductionCenterPos, view, up, vv3DevicePosition ) * ITAConstants::PI_D / 180.0,
	       dZenith  = ( 90.0 - GetElevationOnTarget_DEG( v3ReproductionCenterPos, up, vv3DevicePosition ) ) * ITAConstants::PI_D / 180.0;

	vdY = SHRealvaluedBasefunctions( dZenith, dAzimuth, N );
}