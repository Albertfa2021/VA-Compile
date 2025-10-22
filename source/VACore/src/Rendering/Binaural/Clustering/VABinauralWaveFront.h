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

#ifndef IW_VACORE_BINAURAL_WAVE_FRONT
#define IW_VACORE_BINAURAL_WAVE_FRONT

// VA includes
#include <VA.h>

// Forwards
class ITASampleBuffer;

//! Abstract interface class for binaural wave fronts
class IVABinauralWaveFront
{
public:
	inline IVABinauralWaveFront( ) { };
	virtual inline ~IVABinauralWaveFront( ) { };

	//! Sets the clustering / receiver pose and the clustering principle direction (actual HRTF convolution)
	virtual void SetClusteringMetrics( const VAVec3& v3ClusteringPos, const VAVec3& v3ClusteringView, const VAVec3& v3ClusteringUp,
	                                   const VAVec3& v3PrincipleDirectionOrigin ) = 0;

	//! Returns true, if the wave front has a valid position (in 3D space, can happen that a newly created wave front has not been placed yet)
	virtual bool GetValidWaveFrontOrigin( ) const = 0;

	//! Returns true, if the clustering pose has been set
	virtual bool GetValidClusteringPose( ) const = 0;

	//! Returns the (virtual) wave front origin (can also be a normalized direction point relative to clustering receiver position)
	virtual VAVec3 GetWaveFrontOrigin( ) const = 0;

	//! Updates the (virtual) wave front origin (can also be a normalized direction point relative to clustering receiver position)
	virtual void SetWaveFrontOrigin( const VAVec3& v3Origin ) = 0;

	virtual void GetOutput( ITASampleBuffer* pfLeftChannel, ITASampleBuffer* pfRightChannel ) = 0;

	virtual int GetSourceID( ) const = 0;

	//! For object pooling
	virtual inline int AddReference( ) { return -1; };
	virtual inline int RemoveReference( ) { return -1; };
};

#endif // IW_VACORE_BINAURAL_WAVE_FRONT
