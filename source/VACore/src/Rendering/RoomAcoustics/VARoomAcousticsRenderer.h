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

#ifndef IW_VACORE_ROOMACOUSTICSRENDERER
#define IW_VACORE_ROOMACOUSTICSRENDERER

#if VACORE_WITH_RENDERER_ROOM_ACOUSTICS

#	include "../Base/VAFIRRendererBase.h"

// ITA simulation scheduler includes
#	include <ITA/SimulationScheduler/result_handler.h>

namespace ITA::SimulationScheduler::RoomAcoustics
{
	class CMasterSimulationController;
}


///
/// \brief FIR based renderer for room acoustics.
///
/// This renderer uses the ITASimulationScheduler with its room acosutics module to simulate room acoustics.
/// This simulation can be configured via the configuration file specified via the "SimulationConfigFilePath" configuration option.
/// For further information on this configuration visit the repository for the ITASimulationScheduler.
///
/// The acoustic effects considered are depended on the utilized simulation backend.
///
/// \see https://git.rwth-aachen.de/ita/itasimulationscheduler
///
class CVARoomAcousticsRenderer
    : public CVAFIRRendererBase
    , public ITA::SimulationScheduler::IResultHandler
{
public:
	///
	/// \brief Configuration struct for the room acoustic renderer.
	///
	struct Config : public CVAFIRRendererBase::Config
	{
		bool bStoreProfileData = false; ///< If true and the ITASimulationScheduler is build with the profiler, the profile is stored at destruction.

		///
		/// Constructor setting default values
		///
		Config( ) { sSimulationConfigFilePath = "VASimulationRAVEN.json"; };

		///
		/// Constructor parsing renderer ini-file parameters
		///
		Config( const CVAAudioRendererInitParams& oParams, const Config& oDefaultValues );
	};

	///
	/// \brief Constuctor for the CVARoomAcousticsRenderer.
	/// \param oParams initialization parameters used to construct the renderer.
	///
	CVARoomAcousticsRenderer( const CVAAudioRendererInitParams& oParams );

	///
	/// \brief Destructor for the CVARoomAcousticsRenderer.
	///
	~CVARoomAcousticsRenderer( ) override;

	///
	/// \copydoc ITA::simulation_scheduler::room_acoustics::IResultHandler::postResultReceived
	///
	void PostResultReceived( std::unique_ptr<ITA::SimulationScheduler::CSimulationResult> pResult ) override;

	///
	/// \brief Get parameters from the renderer.
	///
	/// Currently, this method always returns information about how many simulations were received for each of the SourceReceiverPair%s.
	/// \param oArgs struct containing the parameter request.
	/// \return a CVAStruct with the requested parameters.
	///
	CVAStruct GetParameters( const CVAStruct& oArgs ) const override;

	///
	/// \brief Set parameters in the renderer.
	///
	/// This method can be used to set parameters of the renderer or to call additional methods on it.
	/// Currently the following options are implemented:
	/// | Key    | Value               | Method                                                    |
	/// |:------:|:-------------------:|:----------------------------------------------------------|
	/// | export | [combined/DS/ER/DD] | Export the current RIR(part)s for each SourceReceiverPair |
	///
	/// \remark The RIR is stored as a wav file in the bin folder in which the application is run.
	/// \param oParams the parameters or instructions send to the renderer.
	///
	void SetParameters( const CVAStruct& oParams ) override;

	void Reset( ) override;

private:
	const Config m_oConf; ///< Renderer configuration parsed from VACore.ini

	///
	/// \brief Instance of the ::CMasterSimulationController
	///
	std::unique_ptr<ITA::SimulationScheduler::RoomAcoustics::CMasterSimulationController> m_pMasterSimulationController;

	///
	/// \brief The source receiver pair class for this renderer.
	///
	class SourceReceiverPair : public CVAFIRRendererBase::SourceReceiverPair
	{
	public:
		///
		/// \brief Struct containing information about how many simulations are received by the renderer.
		///
		/// This is primarily used for the ::GetParameters method at the moment.
		///
		struct SSimulationUpdateInfo
		{
			unsigned int issuedUpdate;
			unsigned int receivedUpdateDS;
			unsigned int receivedUpdateER;
			unsigned int receivedUpdateDD;
		};

	private:
		///
		/// \brief Update a part of the RIR using a new simulation result.
		/// \note This currently does not support RIR parts with striped zeros.
		/// \param iLeadingZeroOffset the number of leading zeros (not used at the moment).
		/// \param pSourceFrame the ITASampleFrame with the new simulation result.
		/// \param pTargetFrame the ITASampleFrame which should be updated with the new simulation result.
		///
		void UpdateFilterPart( int iLeadingZeroOffset, const ITASampleFrame& pSourceFrame, ITASampleFrame& pTargetFrame ) const;

		const Config& m_oConf; ///< Copy of renderer config

		///
		/// \brief Pointer to the ::CMasterSimulationController
		///
		/// Used to issue ::CSceneUpdate%s to the scheduler for this SourceReceiverPair.
		///
		ITA::SimulationScheduler::RoomAcoustics::CMasterSimulationController* m_pScheduler;

		///
		/// \brief Mutex used for thread safe exchange of the RIR parts.
		/// \note These are mutable such that they can be used in ::ExportRIR which is a \p const method. Do NOT overuse mutable!
		///
		mutable std::mutex m_DirectSoundMutex, m_EarlyReflectionMutex, m_DiffuseDecayMutex;

		///
		/// \brief The current number of the direct sounds leading zeros.
		///
		/// This is stored in order to define the delay for the CITAVariableDelayLine.
		///
		int m_iCurrentLeadingZeros = 0;

		ITASampleFrame m_sfRIRDirectSoundPart;      ///< Sample frames for generating the full RIR for the convolver; direct sound part.
		ITASampleFrame m_sfRIREarlyReflectionsPart; ///< Sample frames for generating the full RIR for the convolver; early reflections.
		ITASampleFrame m_sfRIRDiffuseDecayPart;     ///< Sample frames for generating the full RIR for the convolver; diffuse decay.

		///
		/// \brief Simulation update info for the SourceReceiverPair.
		///
		SSimulationUpdateInfo m_oSimulationUpdateInfo;

	public:
		///
		/// \brief Constructor for the SourceReceiverPair.
		/// \param oConf configuration of the parent renderer.
		/// \param pScheduler pointer to the scheduler used to issue ::CSceneUpdate%s
		///
		SourceReceiverPair( const Config& oConf, ITA::SimulationScheduler::RoomAcoustics::CMasterSimulationController* pScheduler );

		///
		/// \brief Update only the direct sound portion of the RIR.
		///
		/// Uses ::UpdateFilterPart and locks the access via ::m_DirectSoundMutex.
		/// \param iLeadingZeroOffset  the number of leading zeros (not used at the moment).
		/// \param pFrame ITASampleFrame with the new simulation result.
		///
		void UpdateDirectSound( const int iLeadingZeroOffset, const ITASampleFrame& pFrame );

		///
		/// \brief Update only the early reflection portion of the RIR.
		///
		/// Uses ::UpdateFilterPart and locks the access via ::m_EarlyReflectionMutex.
		/// \param iLeadingZeroOffset the number of leading zeros (not used at the moment).
		/// \param pFrame ITASampleFrame with the new simulation result.
		///
		void UpdateEarlyReflections( const int iLeadingZeroOffset, const ITASampleFrame& pFrame );

		///
		/// \brief Update only the diffuse decay portion of the RIR.
		///
		/// Uses ::UpdateFilterPart and locks the access via ::m_DiffuseDecayMutex.
		/// \param iLeadingZeroOffset the number of leading zeros (not used at the moment).
		/// \param pFrame ITASampleFrame with the new simulation result.
		///
		void UpdateDiffuseDecay( const int iLeadingZeroOffset, const ITASampleFrame& pFrame );

		///
		/// \brief Set the currently leading zeros for the calculation of the VDL delay.
		/// \param iZeros number of leading zero samples.
		/// \remark This should be the number of leading zeros of the direct sound!
		///
		void SetCurrentLeadingZeros( int iZeros );

		///
		/// \brief Export the current RIR.
		/// \param sFileName filename relative to the executable.
		/// \param bDS if true, the direct sound part of the RIR will be included in the exported file.
		/// \param bER if true, the early reflection part of the RIR will be included in the exported file.
		/// \param bDD if true, the diffuse decay part of the RIR will be included in the exported file.
		///
		void ExportRIR( const std::string& sFileName, const bool bDS = true, const bool bER = true, const bool bDD = true ) const;

		///
		/// \brief Get the current simulation update info for the SourceReceiverPair.
		/// \return the current simulation update info for the SourceReceiverPair.
		///
		const SSimulationUpdateInfo& GetSimulationUpdateInfo( ) const;

	protected:
		///
		/// \brief Prepare the DSP elements/parameters for the audio frame.
		/// \param dTimeStamp timestamp of the current audio frame.
		/// \param oAuraMode current auralization modes
		/// \param[out] dPropagationDelay new propagation delay for the VDL.
		/// \param[out] sfFIRFilters new filters for the convolvers.
		/// \param[out] bInaudible indicate if the source receiver pair should be muted (e.g. source outside of room).
		///
		void PrepareDelayAndFilters( double dTimeStamp, const AuralizationMode& oAuraMode, double& dPropagationDelay, ITASampleFrame& sfFIRFilters,
		                             bool& bInaudible ) override;

		///
		/// \brief Run/initiate a simulation.
		///
		///	For this renderer, an ITA::SimulationScheduler::CUpdateScene is created and issued to the scheduler.
		/// \param dTimeStamp timestamp of the current audio frame.
		///
		void RunSimulation( double dTimeStamp ) override;
	};

	///
	/// \brief Factory for the Renderer specific SourceReceiverPair.
	///
	class SourceReceiverPairFactory : IVAPoolObjectFactory
	{
	private:
		const Config& m_oConf; ///< Copy of renderer config

		///
		/// \brief Pointer to the ::CMasterSimulationController.
		///
		/// This gets passed to the SourceReceiverPair%s in order for them to issue ITA::SimulationScheduler::CUpdateScene%s to the scheduler.
		///
		ITA::SimulationScheduler::RoomAcoustics::CMasterSimulationController* m_pScheduler;

	public:
		///
		/// \brief Constructor for the factory.
		/// \param oConf renderer config for passing to the SourceReceiverPair%s.
		/// \param pScheduler ::CMasterSimulationController pointer for passing to the SourceReceiverPair%s.
		///
		SourceReceiverPairFactory( const CVARoomAcousticsRenderer::Config& oConf, ITA::SimulationScheduler::RoomAcoustics::CMasterSimulationController* pScheduler )
		    : m_oConf( oConf )
		    , m_pScheduler( pScheduler ) { };


		///
		/// \brief Create an object for the pool.
		/// \return the new object.
		///
		CVAPoolObject* CreatePoolObject( ) override { return new SourceReceiverPair( m_oConf, m_pScheduler ); }
	};
};

#endif

#endif // IW_VACORE_ROOMACOUSTICSRENDERER
