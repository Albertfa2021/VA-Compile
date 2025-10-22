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


#include "VAPanningFunctions.h"

#include "../../Utils/VAUtils.h"

#include <ITAConstants.h>


bool CalculateVBAPGains3D( const CTriangulation* pTriangulation, const VistaVector3D& v3SourcePos, std::vector<double>& vdGains )
{
	vdGains.resize( pTriangulation->NumPoints( ) );
	// Assume loudspeaker array and source to be described in the same coordinate space
	VistaVector3D v3BarycentricCoords, v3LsIndices;
	int iTriangleIndex;
	const VistaTriangle* vt = pTriangulation->GetTriangleIntersectedByPointDirection( v3SourcePos, v3BarycentricCoords, iTriangleIndex, true );

	// Normalize gains
	v3BarycentricCoords.Normalize( );
	if( nullptr != vt )
	{
		v3LsIndices = ( *pTriangulation )( iTriangleIndex );

		for( size_t i = 0; i < vdGains.size( ); i++ )
		{
			// Assign gains
			if( i == v3LsIndices[0] )
				vdGains[i] = v3BarycentricCoords[0];
			else if( i == v3LsIndices[1] )
				vdGains[i] = v3BarycentricCoords[1];
			else if( i == v3LsIndices[2] )
				vdGains[i] = v3BarycentricCoords[2];
			else
				vdGains[i] = 0;
		}
		return true;
	}
	return false;
}

bool CalculateMDAPGains3D( const CTriangulation* pTriangulation, const std::vector<VistaVector3D>& v3SourcePos, std::vector<double>& vdGains )
{
	vdGains.resize( pTriangulation->NumPoints( ) );
	// Assume loudspeaker array and sources to be described in the same coordinate space
	VistaVector3D v3BarycentricCoords, v3LsIndices;
	int iTriangleIndex;
	double dSquaredGainSum = 0;

	for( size_t i = 0; i < v3SourcePos.size( ); i++ )
	{
		const VistaTriangle* vt = pTriangulation->GetTriangleIntersectedByPointDirection( v3SourcePos[i], v3BarycentricCoords, iTriangleIndex, true );

		// Normalize gains
		v3BarycentricCoords.Normalize( );
		if( nullptr != vt )
		{
			v3LsIndices = ( *pTriangulation )( iTriangleIndex );

			for( size_t j = 0; j < vdGains.size( ); j++ )
			{
				if( i == 0 )
					vdGains[j] = 0;

				// Assign gains and apply distance law
				if( j == v3LsIndices[0] )
					vdGains[j] += v3BarycentricCoords[0];
				else if( j == v3LsIndices[1] )
					vdGains[j] += v3BarycentricCoords[1];
				else if( j == v3LsIndices[2] )
					vdGains[j] += v3BarycentricCoords[2];
				else
					vdGains[j] += 0;
			}
		}
		else
			return false;
	}

	// Normalize
	for( size_t j = 0; j < vdGains.size( ); j++ )
		dSquaredGainSum += vdGains[j] * vdGains[j];

	for( size_t j = 0; j < vdGains.size( ); j++ )
		vdGains[j] /= std::sqrt( dSquaredGainSum );

	return true;
}

void CalcualteSpreadingSources( const VAVec3& v3SourcPos, const VAVec3& view, const VAVec3& up, const float& spreadingAngle, const int& numSpreadingSources,
                                std::vector<VAVec3>& vv3SpreadingSources )
{
	// Rotate to horizontal plane
	VistaVector3D vSource = VAVec3ToVistaVector3D( v3SourcPos );
	VistaQuaternion p( VistaVector3D( view.x, view.y, view.z ), vSource );

	VistaQuaternion q( VistaAxisAndAngle( VAVec3ToVistaVector3D( up ), spreadingAngle / 2 ) );
	VistaQuaternion r( VistaAxisAndAngle( -VAVec3ToVistaVector3D( view ), 2.0f * ITAConstants::PI_F / numSpreadingSources ) );

	VistaVector3D vSpreadingSource;
	for( int i = 0; i < numSpreadingSources; i++ )
	{
		if( i == 0 )
		{
			vSpreadingSource = q.Rotate( VistaVector3D( 0, 0, -1 ) * vSource.GetLength( ) );
			vv3SpreadingSources.push_back( VistaVector3DToVAVec3( p.Rotate( vSpreadingSource ) ) );
			continue;
		}

		vSpreadingSource = r.Rotate( vSpreadingSource );
		vv3SpreadingSources.push_back( VistaVector3DToVAVec3( p.Rotate( vSpreadingSource ) ) );
	}
}
