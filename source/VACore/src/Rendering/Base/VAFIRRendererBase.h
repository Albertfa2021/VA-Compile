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

#ifndef IW_VACORE_FIRRENDERERBASE
#define IW_VACORE_FIRRENDERERBASE

// VA includes
#include "../../SpatialEncoding/VASpatialEncoding.h"
#include "VAAudioRendererBase.h"
#include "VAAudioRendererSourceDirectivityState.h"

// ITA includes
#include <ITAThirdOctaveMagnitudeSpectrum.h>
#include <ITASampleFrame.h>
#include <ITASampleBuffer.h>
#include <ITAVariableDelayLine.h>

// STD includes
#include <list>
#include <memory>

// VA forwrads
class CVASoundSourceState;
class CVAReceiverState;

// ITA forwards
//class CITAVariableDelayLine;
class ITAUPConvolution;


/// Base class for renderers using FIR-based convolution.
/// Defines source-receiver pairs that hold an N-channel convolution engine and a variable-delay line. Audio processing of a source-receiver pair is done by based on these DSP parameters:
/// - Propagation delay
/// - N-channel FIR filter
/// - Bool indicating whether source-receiver pair should be muted (e.g. source outside of a room)
/// Those parameters must be calculated in derived renderers by overloading the PrepareDelayAndFilters() method in a derived source-receiver pair class.
/// The actual audio processing is done in this renderer including:
/// - Applying gain based on sound source power
/// - Potential mute of source / source-receiver pair (using a gain of 0.0)
/// - Doppler shift via VDL
/// - N-channel convolution based on given FIR filters
/// NOTE: All auralization modes must be handled in derived renderers!
class CVAFIRRendererBase : public CVAAudioRendererBase
{
public:
	struct Config : public CVAAudioRendererBase::Config
	{
		int iNumChannels            = -1;              /// Number of output channels
		int iMaxFilterLengthSamples = 22100;           /// Maximum length of FIR filters in samples

		/// Default constructor setting default values
		Config( ) { };
		/// Constructor parsing renderer ini-file parameters. Takes a given config for default values.
		/// Note, that part of the parsing process is done in the parent renderer's config method.
		Config( const CVAAudioRendererInitParams& oParams, const Config& oDefaultValues );
	};

	/// Constructor expecting ini-file renderer parameters and a config object with default values which are used if a setting is not specified in ini-file.
	CVAFIRRendererBase( const CVAAudioRendererInitParams& oParams, const Config& oDefaultValues );

	/// Interface to load a user requested scene (e.g. file to geometric data).
	/// Does nothing per default. Overload in derived renderers to implement functionality.
	virtual void LoadScene( const std::string& ) override { };
	/// Handles a set rendering module parameters call()
	/// Calls base renderer function. Overload in derived renderers to implement functionality.
	virtual void SetParameters( const CVAStruct& oParams ) override;
	
	/// Handles a get rendering module parameters call()
	/// Returns an empty struct per default. Overload in derived renderers to implement functionality.
	virtual CVAStruct GetParameters( const CVAStruct& ) const override { return CVAStruct( ); };

private:
	const Config m_oConf; /// Renderer configuration parsed from VACore.ini (downcasted copy of child renderer)

protected:

	class SourceReceiverPair : public CVAAudioRendererBase::SourceReceiverPair
	{
	private:
		const Config& m_oConf; /// Copy of renderer config

		bool m_bInaudible          = false; /// Bool indicating whether this source-receiver pair is inaudible
		double m_dPropagationDelay = 0.0;   /// Propagation delay used for VDL
		ITASampleFrame m_sfFIRFilters;      /// N-channel buffer storing the FIR filters for next convolution step
		ITASampleBuffer m_sbTemp;           /// Temporary buffer used in the signal processing chain

		std::unique_ptr<CITAVariableDelayLine> m_pVariableDelayLine = nullptr; /// DSP elemnt for propagation delay / Doppler shift
		std::vector<std::shared_ptr<ITAUPConvolution> > m_vpConvolvers;        /// List of DSP element for the convolution of all channels


	public:
		SourceReceiverPair( const Config& oConf );

		/// Pool Constructor
		virtual void PreRequest( ) override;
		/// Pool Destructor
		virtual void PreRelease( ) override;

		/// If implemented, runs a simulation for this SR-pair. Then, loops through all sound paths and calls their Process() method
		void Process( double dTimeStamp, const AuralizationMode& oAuraMode, const CVASoundSourceState& oSourceState, const CVAReceiverState& oReceiverState ) override;

	private:
		/// Resets all DSP elements to their original state (after constructor call)
		void Reset( );
		/// Updates the parameters of the DSP elements using the FIR filters and other parameters updated by the derived class
		void UpdateDSPElements( const AuralizationMode& oAuralizationMode, const CVASoundSourceState& oSourceState );
		/// Processes the input signal through the VDL and runs the convolution for each channel
		void ProcessDSPElements( );
		

	protected:
		/// Overload in derived renderer to set propagation delay, FIR filters. Also allows to indicate whether sound transfer is inaudible (e.g. source outside of room).
		virtual void PrepareDelayAndFilters( double dTimeStamp, const AuralizationMode& oAuraMode, double& dPropagationDelay, ITASampleFrame& sfFIRFilters,
		                                     bool& bInaudible ) = 0;

		/// Implement this in derived classes to run a simulation just before this source-receiver pair is processed
		virtual void RunSimulation( double dTimeStamp ) { };
	};
};

#endif // IW_VACORE_FIRRENDERERBASE
