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

#ifndef IW_VACORE_BINAURAL_WAVE_FRONT_BASE
#define IW_VACORE_BINAURAL_WAVE_FRONT_BASE

// VA includes
#include "../VABinauralWaveFront.h"

#include <VA.h>
#include <VAPoolObject.h>

// Forwards
class CVASoundSourceState;
class CVASoundSourceDesc;
class CVASharedMotionModel;
class CITAVariableDelayLine;
class CVACoreImpl;

//! Represents a wave front emitted from a sound source for a free-field clustering approach
/**
 * Base class for instances subject to wave front clustering / direction clustering by the clustering engine.
 * Create class derived from abstract interface if you want to use it in another rendering module.
 *
 * @sa BinauralClusteringDirection
 *
 *
 */
class CVABinauralWaveFrontBase
    : public IVABinauralWaveFront
    , public CVAPoolObject
{
public:
	CVASoundSourceState* pState;
	CVASoundSourceDesc* pSoundSourceData;
	CVASharedMotionModel* pMotionModel;

	struct Config
	{
		bool bMotionModelLogInput;
		bool bMotionModelLogEstimated;

		double dMotionModelWindowSize;
		double dMotionModelWindowDelay;

		int iMotionModelNumHistoryKeys;

		const CVACoreImpl* pCore;
	};

	CVABinauralWaveFrontBase( const Config& conf );
	virtual ~CVABinauralWaveFrontBase( );

	virtual void PreRequest( );
	virtual void PreRelease( );

	virtual void GetOutput( ITASampleBuffer* pfLeftChannel, ITASampleBuffer* pfRightChannel );
	virtual void SetClusteringMetrics( const VAVec3& v3Pos, const VAVec3& v3View, const VAVec3& v3Up, const VAVec3& v3PrincipleDirectionOrigin );
	virtual void SetWaveFrontOrigin( const VAVec3& v3Origin );

	virtual bool GetValidWaveFrontOrigin( ) const;
	virtual bool GetValidClusteringPose( ) const;

	virtual VAVec3 GetWaveFrontOrigin( ) const;

	virtual int AddReference( );
	virtual int RemoveReference( );

	int GetSourceID( ) const override;

private:
	const Config m_oConf;
	double m_dCreationTimeStamp;

	CITAVariableDelayLine* m_pVDLChL;
	CITAVariableDelayLine* m_pVDLChR;

	bool m_bWaveFrontOriginSet;
	bool m_bClusteringPoseSet;

	VAVec3 m_v3ClusteringPos;
	VAVec3 m_v3ClusteringView;
	VAVec3 m_v3ClusteringUp;
	VAVec3 m_v3ClusteringPrincipleDirectionOrigin;

	VAVec3 m_v3WaveFrontOrigin;
};

#endif // IW_VACORE_BINAURAL_WAVE_FRONT_BASE
