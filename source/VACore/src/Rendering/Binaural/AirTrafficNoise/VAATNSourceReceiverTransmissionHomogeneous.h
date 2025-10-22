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

#ifndef IW_VACORE_BINAURAL_ATN_SOURCE_RECEIVER_TRANSMISSION_HOMOGENEOUS
#define IW_VACORE_BINAURAL_ATN_SOURCE_RECEIVER_TRANSMISSION_HOMOGENEOUS

#include "../../../Medium/VAHomogeneousMedium.h"
#include "VAATNSourceReceiverTransmission.h"

//! Represents the sound transmission between a single source and a binaural receiver based on a simple image source model
/**
 * Two sound paths (direct + reflection) are considered for the total sound transmission
 */
class CVABATNSourceReceiverTransmissionHomogeneous : public CVABATNSourceReceiverTransmission
{
public:
	inline virtual ~CVABATNSourceReceiverTransmissionHomogeneous( ) { };


	//! Bestimmt die relativen Größen der Pfade
	/**
	 * Diese berechneten Größen dienen als Grundlage zur Bestimmung der ausgewählten
	 * Datensätze und Einstellungen der DSP-Elemente. Ein weiteres Update der einzelnen
	 * DSP-Elemente führt z.B. zum Filteraustausch, wenn die Statusänderung Auswirkungen hat
	 * (tatsächlich ein neuer Datensatz geholt werden muss).
	 *
	 * Diese Methode ist besonders leichtgewichtig, da sie im StreamProcess genutzt wird.
	 *
	 * // später -> \return Gibt false zurück, falls die retardierten Werte noch nicht zur Verfügung stehen.
	 */
	virtual void UpdateSoundPaths( );

	//! Aktualisiert die Richtcharakteristik auf dem Pfad
	// void UpdateSoundSourceDirectivity();

	//! Aktualisiert die Mediumsausbreitung auf dem Pfad
	virtual void UpdateMediumPropagation( );

	//! Updates the temporal variations model of medium shift fluctuation
	// void UpdateTemporalVariation();

	//! Updates the air attenuation (damping)
	virtual void UpdateAirAttenuation( );

	//! Aktualisiert den Lautstärkeabfall nach dem 1/r-Gesetzt (Inverse Distance Decrease/Law)
	virtual void UpdateSpreadingLoss( );

	//! Aktualisiert die HRIR auf dem Pfad
	// void UpdateSoundReceiverDirectivity();

private:
	// //! Reference to medium used in VA Core
	const CVAHomogeneousMedium& m_oHomogeneousMedium;

	//! Standard-Konstruktor deaktivieren
	CVABATNSourceReceiverTransmissionHomogeneous( );

	//! Konstruktor
	CVABATNSourceReceiverTransmissionHomogeneous( const CVAHomogeneousMedium& oMedium, double dSamplerate, int iBlocklength, int iHRIRFilterLength, int iDirFilterLength,
	                                              int iFilterBankType = CITAThirdOctaveFilterbank::IIR_BIQUADS_ORDER10 );

	friend class CVABATNSourceReceiverTransmissionHomogeneousFactory;
};

// Factory

class CVABATNSourceReceiverTransmissionHomogeneousFactory : public CVABATNSourceReceiverTransmissionFactory
{
private:
	const CVAHomogeneousMedium& m_oHomogeneousMedium;

public:
	inline CVABATNSourceReceiverTransmissionHomogeneousFactory( const CVAHomogeneousMedium& oMedium, double dSamplerate, int iBlocklength, int iHRIRFilterLength,
	                                                            int iDirFilterLength, int iFilterBankType = CITAThirdOctaveFilterbank::IIR_BIQUADS_ORDER10 )
	    : CVABATNSourceReceiverTransmissionFactory( dSamplerate, iBlocklength, iHRIRFilterLength, iDirFilterLength, iFilterBankType )
	    , m_oHomogeneousMedium( oMedium ) { };

	inline CVAPoolObject* CreatePoolObject( )
	{
		return new CVABATNSourceReceiverTransmissionHomogeneous( m_oHomogeneousMedium, m_dSamplerate, m_iBlocklength, m_iHRIRFilterLength, m_iDirFilterLength,
		                                                         m_iFilterBankType );
	};
};

#endif // IW_VACORE_BINAURAL_ATN_SOURCE_RECEIVER_TRANSMISSION_HOMOGENEOUS
