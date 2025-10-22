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

#ifndef IW_VACORE_BINAURAL_AIR_TRAFFIC_SOUND_SOURCE
#define IW_VACORE_BINAURAL_AIR_TRAFFIC_SOUND_SOURCE

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


//! Interne Beschreibung einer Schallquelle
class CVABATNSoundSource : public CVAPoolObject
{
public:
	class Config
	{
	public:
		double dMotionModelWindowSize;
		double dMotionModelWindowDelay;

		int iMotionModelNumHistoryKeys;
	};

	inline CVABATNSoundSource( const Config& oConf_ ) : oConf( oConf_ ) { };

	const Config oConf;
	CVASoundSourceDesc* pData; //!< (Unversioned) Source description
	CVASharedMotionModel* pMotionModel;
	bool bDeleted;
	VAVec3 vPredPos;              //!< Estimated position
	VAVec3 vPredView;             //!< Estimated Orientation (View-Vektor)
	VAVec3 vPredUp;               //!< Estimated Orientation (Up-Vektor)
	bool bValidTrajectoryPresent; //!< Estimation possible -> valid trajectory present

	// Pool-Konstruktor
	inline void PreRequest( )
	{
		pData = nullptr;

		CVABasicMotionModel::Config oDefaultConfig;
		oDefaultConfig.dWindowDelay    = oConf.dMotionModelWindowDelay;
		oDefaultConfig.dWindowSize     = oConf.dMotionModelWindowSize;
		oDefaultConfig.iNumHistoryKeys = oConf.iMotionModelNumHistoryKeys;
		pMotionModel                   = new CVASharedMotionModel( new CVABasicMotionModel( oDefaultConfig ), true );

		bValidTrajectoryPresent = false;
	};

	inline void PreRelease( )
	{
		delete pMotionModel;
		pMotionModel = nullptr;
	};

	inline double GetCreationTimestamp( ) const { return m_dCreationTimeStamp; };

private:
	double m_dCreationTimeStamp; //!< Date of creation within streaming context
};

class CVABATNSourcePoolFactory : public IVAPoolObjectFactory
{
public:
	inline CVABATNSourcePoolFactory( const CVABATNSoundSource::Config& oConf ) : m_oSourceConf( oConf ) { };

	inline CVAPoolObject* CreatePoolObject( )
	{
		CVABATNSoundSource* pSource;
		pSource = new CVABATNSoundSource( m_oSourceConf );
		return pSource;
	};

private:
	const CVABATNSoundSource::Config& m_oSourceConf;

	//! Not for use, avoid C4512
	inline CVABATNSourcePoolFactory operator=( const CVABATNSourcePoolFactory& ) { VA_EXCEPT_NOT_IMPLEMENTED; };
};

#endif // IW_VACORE_BINAURAL_AIR_TRAFFIC_SOUND_SOURCE
