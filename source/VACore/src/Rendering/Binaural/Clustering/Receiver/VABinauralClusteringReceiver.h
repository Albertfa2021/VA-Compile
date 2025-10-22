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

#ifndef IW_VACORE_BINAURAL_CLUSTERING_RECEIVER
#define IW_VACORE_BINAURAL_CLUSTERING_RECEIVER

// VA includes
#include <VA.h>
#include <VAPoolObject.h>

// VA core includes
#include "../../../../Motion/VAMotionModelBase.h"
#include "../../../../Motion/VASharedMotionModel.h"
#include "../../../../Scene/VAScene.h"
#include "../../../../core/core.h"

// ITA includes
#include <ITASampleFrame.h>

//! Binaural clustering direction receiver
class CVABinauralClusteringReceiver : public CVAPoolObject
{
public:
	struct Config
	{
		bool bMotionModelLogInput;
		bool bMotionModelLogEstimated;

		double dMotionModelWindowSize;
		double dMotionModelWindowDelay;

		int iMotionModelNumHistoryKeys;
	};

	bool bHasValidTrajectory;

	CVASoundReceiverDesc* pData;
	CVASharedMotionModel* pMotionModel;
	IVADirectivity* pDirectivity;

	VAVec3 v3PredictedPosition; // @todo: remove and replace by a getter using the motion model.
	VAVec3 v3PredictedView;
	VAVec3 v3PredictedUp;

	ITASampleFrame* pOutput;

	CVABinauralClusteringReceiver( const CVACoreImpl* pCore, const Config& oConf );
	~CVABinauralClusteringReceiver( );

	void PreRequest( );
	void PreRelease( );

private:
	const Config m_oConf;
	const CVACoreImpl* m_pCore;

	double m_dCreationTimeStamp;
};

#endif // IW_VACORE_BINAURAL_CLUSTERING_RECEIVER
