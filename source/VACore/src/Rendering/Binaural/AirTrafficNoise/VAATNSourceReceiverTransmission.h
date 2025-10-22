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

#ifndef IW_VACORE_BINAURAL_ATN_SOURCE_RECEIVER_TRANSMISSION
#define IW_VACORE_BINAURAL_ATN_SOURCE_RECEIVER_TRANSMISSION

#include "VAAirTrafficNoiseSoundSource.h"
#include "VAAirTrafficNoiseSoundReceiver.h"

// VA includes
#include <VA.h>
#include <VAObjectPool.h>

#include "../../../Motion/VAMotionModelBase.h"
#include "../../../Motion/VASharedMotionModel.h"
#include "../../../Scene/VAScene.h"
#include "../../../VASourceTargetMetrics.h"
#include "../../../core/core.h"
#include "../../../Filtering/VATemporalVariations.h"
#include "../../../directivities/VADirectivityDAFFEnergetic.h"
#include "../../../directivities/VADirectivityDAFFHRIR.h"

// ITA includes
#include <ITADataSourceRealization.h>
#include <ITASampleBuffer.h>
#include <ITAVariableDelayLine.h>
#include <ITAThirdOctaveMagnitudeSpectrum.h>
#include <ITAUPConvolution.h>
#include <ITAUPFilter.h>
#include <ITAUPFilterPool.h>
#include <ITAThirdOctaveFilterbank.h>

// 3rdParty includes
#include <tbb/concurrent_queue.h>

// STL Includes

//! Represents the sound transmission between a single source and a binaural receiver
/**
  * Two sound paths (direct + reflection) are considered for the total sound transmission
  */
class CVABATNSourceReceiverTransmission : public CVAPoolObject
{
public:
	virtual ~CVABATNSourceReceiverTransmission();

	// //! Retarded metrics of sound path
	//class Metrics
	//{
	//public:
	//	double dRetardedDistance;		  //!< Metrical distance to retarded sound position
	//	VAQuat qAngleRetSourceToListener; //!< Immediate angle of incidence to retarded source position in listener reference frame in YPR convention
	//	VAQuat qAngleListenerToRetSource; //!< Retarded angle of incidence to listener in source reference frame in YPR convention
	//};

	//! State of directivity
	class CSoundSourceDirectivityState
	{
	public:
		inline CSoundSourceDirectivityState()
			: pData( NULL )
			, iRecord( -1 )
			, bDirectivityEnabled( true ) {};

		IVADirectivity* pData;	//!< Directivity data, may be NULL
		int iRecord;			//!< Directivity index
		bool bDirectivityEnabled;

		inline bool operator==( const CSoundSourceDirectivityState& rhs ) const
		{
			bool bBothEnabled = ( bDirectivityEnabled == rhs.bDirectivityEnabled );
			bool bSameRecordIndex = ( iRecord == rhs.iRecord );
			bool bSameData = ( pData == rhs.pData );

			return ( bBothEnabled && bSameRecordIndex && bSameData );
		};
	};

	class CSoundReceiverDirectivityState
	{
	public:
		inline CSoundReceiverDirectivityState()
			: pData( NULL )
			, iRecord( -1 )
			, fDistance( 1.0f ) {};

		IVADirectivity* pData;	 //!< HRIR data, may be NULL
		int iRecord;			 //!< HRIR index
		float fDistance;		 //!< HRIR dataset distance

		inline bool operator!=( const CSoundReceiverDirectivityState& rhs ) const
		{
			if( pData != rhs.pData ) return true;
			if( fDistance != rhs.fDistance ) return true;
			if( iRecord != rhs.iRecord ) return true;
			return false;
		};
	};

	CVABATNSoundSource* pSoundSource;		//!< Verknpfte Quelle
	CVABATNSoundReceiver* pSoundReceiver;	//!< Verknpfter Hrer

	struct CPropagationPath
	{
		CVASourceTargetMetrics oRelations; //!< Informationen ber die Relation von Quelle und Hrer (Position & Orientierung)

		double dPropagationTime; //!< Time that the sound waves took to be propagated to the receiver
		double dGeometricalSpreadingLoss; //!< Spherical / geometrical spreading loss (usually 1-by-R distance law)

		CITAVariableDelayLine* pVariableDelayLine;	//!< DSP-Element zur Bercksichtigung der Mediumsausbreitung in instationren Schallfeldern (inkl. Doppler-Effekt, falls aktiviert)


		CSoundSourceDirectivityState oDirectivityStateCur;	//!< Aktueller Status der Richtcharakteristik
		CSoundSourceDirectivityState oDirectivityStateNew;	//!< Neuer Status der Richtcharakteristik
		ITABase::CThirdOctaveGainMagnitudeSpectrum oSoundSourceDirectivityMagnitudes; //!< Magnitudes for directivity

		CVAAtmosphericTemporalVariations oATVGenerator;	//!< Generator for temporal variation magnitudes of the time signal by medium shifts during propagation
		ITABase::CThirdOctaveGainMagnitudeSpectrum oTemporalVariationMagnitudes;		//!< Magnitudes for temporal variation of the time signal by medium shifts during propagation

		ITABase::CThirdOctaveGainMagnitudeSpectrum oAirAttenuationMagnitudes; //!< Magnitudes for air attenuation (damping)

		ITABase::CThirdOctaveGainMagnitudeSpectrum oGroundReflectionMagnitudes; //!< Ground reflection parameters (only for reflection paths)

		CITAThirdOctaveFilterbank* pThirdOctaveFilterBank; //!< DSP-Element zur Filterung von Third-Octave Datenstzen der Richtcharakteristik
		ITABase::CThirdOctaveGainMagnitudeSpectrum oThirdOctaveFilterMagnitudes; //!< DSP magnitudes (third octave band resolution)

		CSoundReceiverDirectivityState oSoundReceiverDirectivityStateCur;	//!< Aktueller Status der HRIR
		CSoundReceiverDirectivityState oSoundReceiverDirectivityStateNew;	//!< Neuer Status der HRIR

		ITAUPConvolution* pFIRConvolverChL;	//!< DSP-Element zur Faltung mit der Kopfbezogenen bertragungsfunktion im Zeitbereich (HRIR) - Links
		ITAUPConvolution* pFIRConvolverChR;	//!< DSP-Element zur Faltung mit der Kopfbezogenen bertragungsfunktion im Zeitbereich (HRIR) - Rechts

		inline CPropagationPath()
			: dPropagationTime( 0.0f )
			, dGeometricalSpreadingLoss( 1.0f )
			, pVariableDelayLine( NULL )
			, pThirdOctaveFilterBank( NULL )
			, pFIRConvolverChL( NULL )
			, pFIRConvolverChR( NULL )
		{
			oGroundReflectionMagnitudes.SetIdentity();
		};
	};

	CPropagationPath oDirectSoundPath; //!< Direct sound propagation path
	CPropagationPath oReflectedSoundPath; //!< Propagation path including ground reflection

	double dGroundReflectionPlanePosition; //!< Height of ground, defaults to 0

	std::atomic< bool > bDelete; //!< Schallpfad zur Lschung markiert?

	inline void PreRequest()
	{
		pSoundSource = nullptr;
		pSoundReceiver = nullptr;

		// Reset DSP elements
		oDirectSoundPath.pThirdOctaveFilterBank->Clear();
		oReflectedSoundPath.pThirdOctaveFilterBank->Clear();
		oDirectSoundPath.pVariableDelayLine->Clear();
		oReflectedSoundPath.pVariableDelayLine->Clear();
		oDirectSoundPath.pFIRConvolverChL->clear();
		oReflectedSoundPath.pFIRConvolverChL->clear();
		oDirectSoundPath.pFIRConvolverChR->clear();
		oReflectedSoundPath.pFIRConvolverChR->clear();
	};


	//! Updates the two sound paths depending on the underlying model
	/**
	  * The geometric properties of the sound paths are then used to create the acoustic filters
	  */
	virtual void UpdateSoundPaths() = 0;

	//! Aktualisiert die einzelnen Komponenten des Schallpfades
	void UpdateEntities( int iEffectiveAuralisationMode );

	//! Aktualisiert die Richtcharakteristik auf dem Pfad
	void UpdateSoundSourceDirectivity();

	//! Updates propagation delay
	virtual void UpdateMediumPropagation() = 0;

	//! Updates the temporal variations model of medium shift fluctuation
	void UpdateTemporalVariation();

	//! Updates the air attenuation (damping)
	virtual void UpdateAirAttenuation() = 0;

	//! Updates spreading loss
	virtual void UpdateSpreadingLoss() = 0;

	//! Aktualisiert die HRIR auf dem Pfad
	void UpdateSoundReceiverDirectivity();

	//! Aktiviert/Deaktiviert Doppler-Verschiebung
	void SetDopplerShiftsEnabled( bool bEnabled );

protected:

	//! Standard-Konstruktor deaktivieren
	CVABATNSourceReceiverTransmission();

	//! Konstruktor
	CVABATNSourceReceiverTransmission( double dSamplerate, int iBlocklength, int iHRIRFilterLength, int iDirFilterLength, int iFilterBankType = CITAThirdOctaveFilterbank::IIR_BIQUADS_ORDER10 );

	ITASampleFrame m_sfHRIRTemp;		 //!< Intern verwendeter Zwischenspeicher fr HRIR Datentze
	EVDLAlgorithm m_iDefaultVDLSwitchingAlgorithm; //!< Umsetzung der Verzgerungsnderung


	friend class CVABATNSourceReceiverTransmissionFactory;
};


// Factory

class CVABATNSourceReceiverTransmissionFactory : public IVAPoolObjectFactory
{
public:

	inline CVABATNSourceReceiverTransmissionFactory( double dSamplerate, int iBlocklength, int iHRIRFilterLength, int iDirFilterLength, int iFilterBankType = CITAThirdOctaveFilterbank::IIR_BIQUADS_ORDER10 )
		: m_dSamplerate( dSamplerate )
		, m_iBlocklength( iBlocklength )
		, m_iHRIRFilterLength( iHRIRFilterLength )
		, m_iDirFilterLength( iDirFilterLength )
		, m_iFilterBankType( iFilterBankType )
	{};

	//inline CVAPoolObject* CreatePoolObject()
	//{
	//	return new CVABATNSourceReceiverTransmission( m_dSamplerate, m_iBlocklength, m_iHRIRFilterLength, m_iDirFilterLength, m_iFilterBankType );
	//};

protected:
	double m_dSamplerate;		//!< Abtastrate
	int m_iBlocklength;			//!< Blocklnge
	int m_iHRIRFilterLength;	//!< Filterlnge der HRIR
	int m_iDirFilterLength;		//!< Filterlnge der Richtcharakteristik
	int m_iFilterBankType;		//!< Filter bank type (FIR, IIR)
};

#endif // IW_VACORE_BINAURAL_ATN_SOURCE_RECEIVER_TRANSMISSION
