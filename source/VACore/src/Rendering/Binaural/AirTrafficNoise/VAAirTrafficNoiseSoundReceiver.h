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

#ifndef IW_VACORE_BINAURAL_AIR_TRAFFIC_SOUND_RECEIVER
#define IW_VACORE_BINAURAL_AIR_TRAFFIC_SOUND_RECEIVER

// VA includes
#include "../../../Filtering/VATemporalVariations.h"
#include "../../../Motion/VAMotionModelBase.h"
#include "../../../Motion/VASharedMotionModel.h"
#include "../../../Scene/VAScene.h"
#include "../../../VASourceTargetMetrics.h"
#include "../../../core/core.h"

#include <VA.h>
#include <VAObjectPool.h>

// ITA includes
#include <ITADataSourceRealization.h>
#include <ITASampleBuffer.h>
#include <ITAThirdOctaveFilterbank.h>
#include <ITAThirdOctaveMagnitudeSpectrum.h>
#include <ITAUPConvolution.h>
#include <ITAUPFilter.h>
#include <ITAUPFilterPool.h>
#include <ITAVariableDelayLine.h>


//! Internal receiver representation
class CVABATNSoundReceiver : public CVAPoolObject
{
public:
	class Config
	{
	public:
		double dMotionModelWindowSize;
		double dMotionModelWindowDelay;

		int iMotionModelNumHistoryKeys;
	};

	inline CVABATNSoundReceiver( CVACoreImpl* pCore, const Config& oConf ) : pCore( pCore ), oConf( oConf ) { };

	CVACoreImpl* pCore;
	const Config oConf;

	CVASoundReceiverDesc* pData; //!< (Unversioned) Listener description
	CVASharedMotionModel* pMotionModel;
	bool bDeleted;
	VAVec3 vPredPos;              //!< Estimated position
	VAVec3 vPredView;             //!< Estimated Orientation (View-Vektor)
	VAVec3 vPredUp;               //!< Estimated Orientation (Up-Vektor)
	bool bValidTrajectoryPresent; //!< Estimation possible -> valid trajectory present

	ITASampleFrame* psfOutput; //!< Accumulated listener output signals @todo check if sample frame is also deleted after usage

	inline void PreRequest( )
	{
		pData = nullptr;

		CVABasicMotionModel::Config oListenerMotionConfig;
		oListenerMotionConfig.dWindowDelay    = oConf.dMotionModelWindowDelay;
		oListenerMotionConfig.dWindowSize     = oConf.dMotionModelWindowSize;
		oListenerMotionConfig.iNumHistoryKeys = oConf.iMotionModelNumHistoryKeys;
		pMotionModel                          = new CVASharedMotionModel( new CVABasicMotionModel( oListenerMotionConfig ), true );

		bValidTrajectoryPresent = false;

		psfOutput = nullptr;
	};

	// Pool-Destruktor
	inline void PreRelease( )
	{
		delete pMotionModel;
		pMotionModel = nullptr;
	};
};

class CVABATNSoundReceiverPoolFactory : public IVAPoolObjectFactory
{
public:
	inline CVABATNSoundReceiverPoolFactory( CVACoreImpl* pCore, const CVABATNSoundReceiver::Config& oConf ) : m_pCore( pCore ), m_oListenerConf( oConf ) { };

	inline CVAPoolObject* CreatePoolObject( )
	{
		CVABATNSoundReceiver* pListener;
		pListener = new CVABATNSoundReceiver( m_pCore, m_oListenerConf );
		return pListener;
	};

private:
	CVACoreImpl* m_pCore;
	const CVABATNSoundReceiver::Config& m_oListenerConf;

	//! Not for use, avoid C4512
	inline CVABATNSoundReceiverPoolFactory operator=( const CVABATNSoundReceiverPoolFactory& ) { VA_EXCEPT_NOT_IMPLEMENTED; };
};

#endif // IW_VACORE_BINAURAL_AIR_TRAFFIC_SOUND_RECEIVER
