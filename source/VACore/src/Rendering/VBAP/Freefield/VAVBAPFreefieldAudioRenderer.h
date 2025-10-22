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

#ifndef IW_VACORE_VBAPFREEFIELDAUDIORENDERER
#define IW_VACORE_VBAPFREEFIELDAUDIORENDERER

#ifdef VACORE_WITH_RENDERER_VBAP_FREE_FIELD

// VA includes
#	include "../../../Motion/VASharedMotionModel.h"
#	include "../../../Rendering/VAAudioRenderer.h"
#	include "../../../Rendering/VAAudioRendererRegistry.h"
#	include "../../../Scene/VAScene.h"
#	include "../../../VAHardwareSetup.h"

#	include <VABase.h>

// ITA includes
#	include <ITABase/Math/Triangulation.h>
#	include <ITADataSource.h>
#	include <ITADataSourceRealization.h>
#	include <ITAStreamInfo.h>

// Vista includes
#	include <VistaBase/VistaVector3D.h>
#	include <VistaMath/VistaGeometries.h>

// STL Includes
#	include <list>
#	include <string>
#	include <vector>

using namespace ITABase::Math;

//! Vector-Base Amplitude Panning Audio Renderer
/**
 * Verwaltet Schallausbreitungspfade mit VBAP Synthese
 * für beliebige Lautsprecheranordnungen
 *
 * Implementiert:
 *
 * - Gain-Verteilung der Lautsprecher
 * - 1/R-Gesetz
 *
 */
class CVAVBAPFreeFieldAudioRenderer
	: public IVAAudioRenderer
	, public ITADatasourceRealizationEventHandler
	, public CVAObject
{
public:
	CVAVBAPFreeFieldAudioRenderer( const CVAAudioRendererInitParams& oParams );
	virtual ~CVAVBAPFreeFieldAudioRenderer( );

	// Scene handling
	void Reset( );
	void LoadScene( const std::string& sFilename );
	//! Handle a scene state change
	/**
	 * This method updates the internal representation of the VA Scene
	 * by setting up or deleting the sound path entities as well as
	 * modifying existing ones that have changed their state, i.e.
	 * pose or dataset
	 */
	void UpdateScene( CVASceneState* pNewSceneState );

	// Auralization mode handling
	//! Handle a state change in global auralisation mode
	/**
	 * This method updates internal settings for the global auralisation
	 * mode affecting the activation/deactivation of certain components
	 * of the sound path entities
	 */
	void UpdateGlobalAuralizationMode( int iGlobalAuralizationMode );

	// Datastream handling
	ITADatasource* GetOutputDatasource( );
	void HandleProcessStream( ITADatasourceRealization* pSender, const ITAStreamInfo* pStreamInfo );

	CVAStruct CallObject( const CVAStruct& oArgs )
	{
		CVAStruct oReturn;
		return oReturn;
	};
	void SetParameters( const CVAStruct& oParams ) { };
	CVAStruct GetParameters( const CVAStruct& ) { return CVAStruct( *m_oParams.pConfig ); };

private:
	// Internal definitions
	class CLoudspeaker
	{
	public:
		std::string sIdentifier;
		int iIdentifier;
		int iChannel;
		VAVec3 pos;
	};
	class SoundPath
	{
	public:
		SoundPath( const unsigned int iNumChannels ) : m_vdNewGains( iNumChannels ), m_vdOldGains( iNumChannels ), bMarkDeleted( false ), iSourceID( -1 ) { };

		// Provide a copy constructor
		SoundPath( const SoundPath& oOther )
		{
			m_vdNewGains = oOther.m_vdNewGains;
			m_vdOldGains = oOther.m_vdOldGains;
			iSourceID    = oOther.iSourceID;
			bMarkDeleted.exchange( oOther.bMarkDeleted );
		};

		std::vector<double> m_vdNewGains; //!< New gains for current process block
		std::vector<double> m_vdOldGains; //!< Old gains from prior process block (for crossfade)
		int iSourceID;                    //!< Source identifier
		std::atomic<bool> bMarkDeleted;
	};

	// Member
	const CVAAudioRendererInitParams m_oParams; //!< Create a const copy of the init params
	const CVAHardwareOutput* m_pOutput;         //!< Hardware output group with information on devices
	ITADatasourceRealization* m_pdsOutput;      //!< Output datasource

	bool m_bReceiverPosValid;      //!< Indicates whether the receiver position is initialized
	VAVec3 m_vUserPosVirtualScene; //!< Listener position in the virtual scene
	VAVec3 m_vUserPosRealWorld;    //!< Listener position in the real world
	VAVec3 m_vReproCenterVirtual;  //!< Center position of loudspeaker array in the virtual scene
	VAVec3 m_vReproCenterReal;     //!< Center position of loudspeaker array in the real world

	int m_iNumSpreadingSources; //!< Number of spreading sources for multiple direction amplitude panning (MDAP)
	double m_dSpreadingAngle;   //!< Aperture angle between virtual source and spreding sources

	CTriangulation* m_pTriangulation;          //!< Output triangulation
	std::vector<VAVec3> m_vv3RelLoudspeakerPos;//!< Loudspeaker positions relative to center of VBAP setup
	std::vector<CLoudspeaker> m_voLoudspeaker; //!< LEGACY: List of all loudspeaker for the VBAP setup
	std::vector<std::vector<CLoudspeaker> >
		m_voTriangulationLoudspeaker; //!< LEGACY: Triangulation all loudspeaker for the VBAP setup; Entries are the actual loudspeaker according to m_voLoudspeaker

	//! Data in context of audio process
	struct
	{
		std::atomic<int> m_iResetFlag; //!< Reset status flag: 0=normal_op, 1=reset_request, 2=reset_ack
		std::atomic<int> m_iStatus;    //!< Current status flag: 0=stopped, 1=running
	} ctxAudio;
	std::list<SoundPath> m_lSoundPaths;    //!< List of active sound paths
	std::list<SoundPath> m_lNewSoundPaths; //!< List of added sound paths

	CVASceneState* m_pCurSceneState;  //!< Pointer to currently rendered scene
	CVASceneState* m_pNewSceneState;  //!< Pointer to new scene which is to be rendered
	int m_iCurGlobalAuralizationMode; //!< Der aktuelle Auralisierungsmodues, mit dem der Renderer arbeitet

	int m_iSetupDimension; //!< Dimension of setup, see \DIMENSIONS


	// Methods
	const CVAHardwareOutput* GetHardwareOutput( std::string& sOutputName );
	void GetReproductionCenterPos( const std::string& sReproductionCenterPos, const CVAHardwareOutput* pOutput, VAVec3& vec3CalcPos );
	void ReadTriangulationFromFile( const std::string& sTriangulationFile, std::vector<VistaVector3D>& vvTriangulationIndices );

	//! Calculates loudspeaker gains for a 3D loudspeaker set-up
	/**
	 * Calculates loudspeaker gains using members of the class
	 *
	 * \param vSoundSource	Position of sound source
	 * \param vdLoudspeakerGains Call-by-reference loudspeaker gains, will be calculated
	 *
	 * \return False, if calculation was not possible
	 */
	bool CalculateLoudspeakerGains3D( const VAVec3& vSoundSource, std::vector<double>& vdLoudpeakerGains ) const;


	//! Calculates loudspeaker gains for a 2D loudspeaker set-up
	/**
	 * Calculates loudspeaker gains using members of the class
	 *
	 * \param vSoundSource	Position of sound source
	 * \param vdLoudspeakerGains Call-by-reference loudspeaker gains, will be calculated
	 *
	 * \return False, if calculation was not possible
	 *
	 */
	bool CalculateLoudspeakerGains2D( const VAVec3& vSoundSource, std::vector<double>& vdLoudpeakerGains ) const;


	//! Helper function to retrieve real world position of a loudspeaker by identifier string
	/**
	 * \sIdentifier	Loudspeaker identifier string (has to match with hardware setup from VACore)
	 * \pSetup			VA core hardware setup pointer
	 *
	 * \return RG_Vector of loudspeaker position, zero vector if not found
	 */
	// RG_Vector GetLSPosition( const int iIdentifier, const CVAHardwareSetup* pSetup ) const;

	//! Helper function to retrieve channel number of a loudspeaker by identifier string
	/**
	 * \sIdentifier	Loudspeaker identifier string (has to match with hardware setup from VACore)
	 * \pSetup			VA core hardware setup pointer
	 *
	 * \return channel of loudspeaker in HardwareSetup, minus one if not found
	 */
	// int GetLSChannel( const int iIdentifier, const CVAHardwareSetup* pSetup ) const;


	//! Calculate the inverse for a three dimensional matrix
	/**
	 * \dOriginalMatrix	Original matrix array
	 * \dInverseMatrix		Target for inverse matrix
	 */
	void CalculateInverseMatrix3x3( const double* dOriginalMatrix, double* dInverseMatrix ) const;


	//! Calculate source position in relation to virtual position of CAVE
	/**
	 */
	VAVec3 GetSourcePosition( const CVAMotionState* pMotionState );


	//! Manage sound pathes
	/**
	 *
	 */
	void ManageSoundPaths( const CVASceneStateDiff* pDiff );


	void SyncInternalData( );
};

#endif // VACORE_WITH_RENDERER_VBAP_FREE_FIELD

#endif // IW_VACORE_VBAPFREEFIELDAUDIORENDERER
