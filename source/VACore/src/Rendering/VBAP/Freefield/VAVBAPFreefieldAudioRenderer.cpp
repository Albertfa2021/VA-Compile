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

#include "VAVBAPFreefieldAudioRenderer.h"

#ifdef VACORE_WITH_RENDERER_VBAP_FREE_FIELD

// VA includes
#	include "../../../Utils/VAUtils.h"
#	include "../../../core/core.h"
#	include "../VAPanningFunctions.h"

// ITA includes
#	include <ITAConstants.h>
#	include <ITAFastMath.h>
#	include <ITASampleBuffer.h>

// Vista includes
#	include <VistaBase/VistaVector3D.h>
#	include <VistaMath/VistaGeometries.h>
#	include <VistaTools/VistaIniFileParser.h>

// STL includes
#	include <iostream>
#	include <math.h>


// -------- Constructor and Destructor --------
CVAVBAPFreeFieldAudioRenderer::CVAVBAPFreeFieldAudioRenderer( const CVAAudioRendererInitParams& oParams )
    : CVAObject( oParams.sClass + ":" + oParams.sID )
    , m_pCurSceneState( nullptr )
    , m_bReceiverPosValid( false )
    , m_oParams( oParams )
{
	VA_WARN( "CVAVBAPFreeFieldAudioRenderer",
	         "The renderer class 'VBAPFreeField' is deprecated and might be removed in the future. Use the class 'FreeField' with 'SpatialEncodingType = "
	         "VBAP' instead." );

	CVAConfigInterpreter conf( *oParams.pConfig );

	// if( !conf.OptInteger( "SetupDimension", m_iSetupDimension, 2 ) )
	// VA_WARN( "VBAPFreefieldRenderer", "No setup dimension given, using 2D plane (ignoring horizontal Y component)" );
	m_iSetupDimension = 3;

	// Hardware output setup
	std::string sOutput;
	conf.ReqString( "Output", sOutput );
	m_pOutput = GetHardwareOutput( sOutput );

	std::string sReproCenterPos;
	conf.OptString( "ReproductionCenterPos", sReproCenterPos, "AUTO" );
	GetReproductionCenterPos( sReproCenterPos, m_pOutput, m_vReproCenterReal );

	std::string sOutputTriangulation;
	conf.OptString( "OutputTriangulation", sOutputTriangulation, "" );
	std::vector<VistaVector3D> vv3LoudspeakerPositions;
	std::vector<int> viID, viChannel;
	std::vector<std::string> vsID;
	for( int l = 0; l < m_pOutput->vpDevices.size( ); l++ )
	{
		const CVAHardwareDevice* pDevice = m_pOutput->vpDevices[l];
		m_vv3RelLoudspeakerPos.push_back( pDevice->vPos - m_vReproCenterReal );
		vv3LoudspeakerPositions.push_back( VAVec3ToVistaVector3D( pDevice->vPos ) );
		vsID.push_back( pDevice->sIdentifier );
		viID.push_back( l );
		viChannel.push_back( pDevice->viChannels[0] );
	}
	if( sOutputTriangulation.empty( ) )
	{
		VA_WARN( m_oParams.sID, "Entering LEGACY mode!" );
		m_pTriangulation = new CTriangulation( vv3LoudspeakerPositions, vsID, viID, viChannel );
	}
	else
	{
		std::vector<VistaVector3D> vvTriangulationIndices;
		ReadTriangulationFromFile( sOutputTriangulation, vvTriangulationIndices );
		m_pTriangulation = new CTriangulation( vv3LoudspeakerPositions, vvTriangulationIndices, VAVec3ToVistaVector3D( m_vReproCenterReal ) );
	}

	// MDAP stuff
	conf.OptInteger( "NumSpreadingSources", m_iNumSpreadingSources, 0 );
	conf.OptNumber( "SpreadingAngleDegrees", m_dSpreadingAngle, 0 );
	m_dSpreadingAngle = m_dSpreadingAngle * ITAConstants::PI_D / 180.0;

	// Init renderer output
	int iNumChannels   = int( m_pOutput->vpDevices.size( ) );
	double dSampleRate = m_oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.dSampleRate;
	int iBlockLength   = m_oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.iBuffersize;

	m_pdsOutput = new ITADatasourceRealization( iNumChannels, dSampleRate, iBlockLength );
	m_pdsOutput->SetStreamEventHandler( this );

	ctxAudio.m_iResetFlag = 0;
	ctxAudio.m_iStatus    = 0;

	// Register the renderer as a module
	oParams.pCore->RegisterModule( this );
}

const CVAHardwareOutput* CVAVBAPFreeFieldAudioRenderer::GetHardwareOutput( std::string& sOutputName )
{
	const CVAHardwareOutput* pOutput = m_oParams.pCore->GetCoreConfig( )->oHardwareSetup.GetOutput( sOutputName );
	if( pOutput == nullptr )
		VA_EXCEPT2( INVALID_PARAMETER, "Unrecognized output '" + sOutputName + "' for VBAP FreeField Renderer" );
	return pOutput;
}

void CVAVBAPFreeFieldAudioRenderer::GetReproductionCenterPos( const std::string& sReproductionCenterPos, const CVAHardwareOutput* pOutput, VAVec3& vec3CenterPos )
{
	if( sReproductionCenterPos == "CALC" )
	{
		// Calculate center from device config
		int iNumberDevices = 0;
		VAVec3 vec3Old;
		vec3CenterPos.Set( 0, 0, 0 );
		vec3Old.Set( 0, 0, 0 );
		for( int idxDev = 0; idxDev < pOutput->vpDevices.size( ); idxDev++ )
		{
			iNumberDevices++;
			vec3Old       = vec3CenterPos;
			vec3CenterPos = vec3Old + pOutput->vpDevices[idxDev]->vPos;
		}
		vec3Old = vec3CenterPos;
		vec3CenterPos.Set( vec3Old.x / iNumberDevices, vec3Old.y / iNumberDevices, vec3Old.z / iNumberDevices );
		VA_WARN( m_oParams.sClass, "Ambisonics reproduction center set to " + std::to_string( vec3CenterPos.x ) + ", " + std::to_string( vec3CenterPos.y ) + ", " +
		                               std::to_string( vec3CenterPos.z ) );
	}
	else if( sReproductionCenterPos == "AUTO" )
	{
		vec3CenterPos.Set( 0, 0, 0 );
		VA_WARN( this, "Reproduction center set to 0 0 0" );
	}
	else
	{
		std::vector<std::string> vsPosComponents = splitString( sReproductionCenterPos, ',' );
		assert( vsPosComponents.size( ) == 3 );
		vec3CenterPos.Set( StringToFloat( vsPosComponents[0] ), StringToFloat( vsPosComponents[1] ), StringToFloat( vsPosComponents[2] ) );
	}
}

void CVAVBAPFreeFieldAudioRenderer::ReadTriangulationFromFile( const std::string& sTriangulationFile, std::vector<VistaVector3D>& vvTriangulationIndices )
{
	// Read triangulation from INI
	std::string sTriangulationFileAbs = m_oParams.pCore->FindFilePath( sTriangulationFile );

	VistaPropertyList vplTriangulation;
	VistaIniFileParser::ReadProplistFromFile( sTriangulationFileAbs, vplTriangulation );
	std::string sTriangulation = vplTriangulation( "Triangulation" ).GetValue( );

	std::vector<std::string> vsTriangulation = splitString( sTriangulation, ";" );
	for( int m = 0; m < vsTriangulation.size( ); m++ )
	{
		std::vector<std::string> vsTriangle = splitString( vsTriangulation[m], "," );
		vvTriangulationIndices.push_back( VistaVector3D( StringToInt( vsTriangle[0] ), StringToInt( vsTriangle[1] ), StringToInt( vsTriangle[2] ) ) );
	}
}

CVAVBAPFreeFieldAudioRenderer::~CVAVBAPFreeFieldAudioRenderer( )
{
	delete m_pdsOutput;
}

// -------- Scene Handling --------
void CVAVBAPFreeFieldAudioRenderer::Reset( )
{
	VA_VERBOSE( "VBAPFreeFieldAudioRenderer", "Received reset call, indicating reset now" );
	ctxAudio.m_iResetFlag = 1; // Request reset

	if( ctxAudio.m_iStatus == 0 || m_oParams.bOfflineRendering )
	{
		VA_VERBOSE( "VBAPFreeFieldAudioRenderer", "Was not streaming, will reset manually" );
		// if no streaming active, reset manually
		// SyncInternalData();
		m_lSoundPaths.clear( );
		m_lNewSoundPaths.clear( );
		ctxAudio.m_iResetFlag = 2;
		// ResetInternalData();
	}
	else
	{
		VA_VERBOSE( "VBAPFreeFieldAudioRenderer", "Still streaming, will now wait for reset acknownledge" );
	}

	// Wait for last streaming block before internal reset
	while( ctxAudio.m_iResetFlag != 2 )
	{
		VASleep( 10 ); // Wait for acknowledge
	}

	VA_VERBOSE( "VBAPFreeFieldAudioRenderer", "Operation reset has green light, clearing items" );

	m_lSoundPaths.clear( );
	m_lNewSoundPaths.clear( );

	// Scene frei geben
	if( m_pCurSceneState )
	{
		m_pCurSceneState->RemoveReference( );
		m_pCurSceneState = nullptr;
	}

	ctxAudio.m_iResetFlag = 0; // Enter normal mode

	VA_VERBOSE( "VBAPFreeFieldAudioRenderer", "Reset successful" );
}

void CVAVBAPFreeFieldAudioRenderer::LoadScene( const std::string& sFilename ) {}

void CVAVBAPFreeFieldAudioRenderer::UpdateScene( CVASceneState* pNewSceneState )
{
	m_pNewSceneState = pNewSceneState;
	if( m_pNewSceneState == m_pCurSceneState )
		return;

	// Neue Szene referenzieren (gegen Freigabe sperren)
	m_pNewSceneState->AddReference( );

	// Unterschiede ermitteln: Neue Szene vs. alte Szene
	CVASceneStateDiff oDiff;
	pNewSceneState->Diff( m_pCurSceneState, &oDiff );

	// Schallpfade aktualisieren
	ManageSoundPaths( &oDiff );

	// Try to receive position from user (aka first listener)
	int iListenerID;
	if( m_pNewSceneState->GetNumSoundReceivers( ) > 0 )
	{
		std::vector<int> viListenerIDs;
		m_pNewSceneState->GetListenerIDs( &viListenerIDs );

		// Use first listener as the user of VBAP system
		iListenerID         = viListenerIDs[0];
		m_bReceiverPosValid = m_pNewSceneState->GetReceiverState( iListenerID ) && m_pNewSceneState->GetReceiverState( iListenerID )->GetMotionState( );

		if( m_bReceiverPosValid )
		{
			m_vUserPosVirtualScene = m_pNewSceneState->GetReceiverState( iListenerID )->GetMotionState( )->GetPosition( );
			VAVec3 p, v, u;
			m_oParams.pCore->GetSoundReceiverRealWorldPositionOrientationVU( iListenerID, p, v, u );
			//		convertVU2YPR( VAVec3( vx, vy, vz ), VAVec3( ux, uy, uz ), m_oUserYPRRealWorldRAD );
			m_vUserPosRealWorld.Set( float( p.x ), float( p.y ), float( p.z ) );

			// Define the center of the reproduction system in the virtual scene as the virtual listeners position
			// plus the distance vector from the user to the real reproduction center
			m_vReproCenterVirtual = m_vUserPosVirtualScene + ( m_vReproCenterReal - m_vUserPosRealWorld );
		}
	}

	// Alte Szene freigeben (deferenzieren)
	if( m_pCurSceneState )
		m_pCurSceneState->RemoveReference( );
	m_pCurSceneState = m_pNewSceneState;
	m_pNewSceneState = nullptr;
}

void CVAVBAPFreeFieldAudioRenderer::ManageSoundPaths( const CVASceneStateDiff* pDiff )
{
	// Über aktuelle Pfade iterieren und gelöschte markieren
	std::list<SoundPath>::iterator it = m_lSoundPaths.begin( );
	while( it != m_lSoundPaths.end( ) )
	{
		SoundPath& oPath( *it++ );
		int iSourceID = oPath.iSourceID;

		for( std::vector<int>::const_iterator cit = pDiff->viDelSoundSourceIDs.begin( ); cit != pDiff->viDelSoundSourceIDs.end( ); ++cit )
		{
			if( iSourceID == ( *cit ) )
			{
				oPath.bMarkDeleted = true; // Pfad zum Löschen markieren
				break;
			}
		}
	}

	// Add new sources in internal list
	for( std::vector<int>::const_iterator cit = pDiff->viNewSoundSourceIDs.begin( ); cit != pDiff->viNewSoundSourceIDs.end( ); ++cit )
	{
		SoundPath oPath( m_pdsOutput->GetNumberOfChannels( ) );
		oPath.iSourceID = *cit;

		// Only add, if no other renderer has been connected explicitly with this source
		const CVASoundSourceDesc* pSoundSourceDesc = m_oParams.pCore->GetSceneManager( )->GetSoundSourceDesc( oPath.iSourceID );
		if( pSoundSourceDesc->sExplicitRendererID.empty( ) || pSoundSourceDesc->sExplicitRendererID == m_oParams.sID )
			m_lNewSoundPaths.push_back( oPath );
	}

	return;
}

// -------- Auralization mode handling --------
void CVAVBAPFreeFieldAudioRenderer::UpdateGlobalAuralizationMode( int iAuraMode )
{
	m_iCurGlobalAuralizationMode = iAuraMode;
}

// -------- Datastream handling --------
ITADatasource* CVAVBAPFreeFieldAudioRenderer::GetOutputDatasource( )
{
	return m_pdsOutput;
}

void CVAVBAPFreeFieldAudioRenderer::HandleProcessStream( ITADatasourceRealization*, const ITAStreamInfo* pStreamInfo )
{
	// TODO: Runtime optimization e.g.
	// Synchronize internal lists of pathes
	SyncInternalData( );

	// Clear output buffer
	for( unsigned int i = 0; i < m_pdsOutput->GetNumberOfChannels( ); i++ )
	{
		float* pfOutputData = m_pdsOutput->GetWritePointer( i );
		fm_zero( pfOutputData, m_pdsOutput->GetBlocklength( ) );
	}

	// Iterate over pathes
	std::list<SoundPath>::iterator it = m_lSoundPaths.begin( );
	while( it != m_lSoundPaths.end( ) )
	{
		SoundPath& oPath( *it++ );
		int iID = oPath.iSourceID;

		// If no scene present, immediately return
		if( m_pCurSceneState == nullptr )
			break;

		CVASoundSourceState* pVirtualSourceState = m_pCurSceneState->GetSoundSourceState( iID );
		// If no motion state present, skip this source
		if( pVirtualSourceState == nullptr )
			continue;

		int iAuralizationMode = ( m_iCurGlobalAuralizationMode & pVirtualSourceState->GetAuralizationMode( ) );

		std::vector<double>& vdLoudspeakerGains( oPath.m_vdNewGains );
		std::vector<double>& vdLoudspeakerLastGains( oPath.m_vdOldGains );


		std::vector<int> viListenerIDs;
		m_pCurSceneState->GetListenerIDs( &viListenerIDs );

		const CVAMotionState* pVirtualSourceMotionState = pVirtualSourceState->GetMotionState( );
		if( pVirtualSourceMotionState != nullptr && viListenerIDs.size( ) > 0 )
		{
			// Quellposition relativ zum virtuellen Arraymittelpunkt berechnen
			VAVec3 vSoundSource = pVirtualSourceMotionState->GetPosition( ) - m_vReproCenterVirtual;

			// Quelle und Hörer an der selben Position ... auslassen
			if( vSoundSource.Length( ) == 0.0f )
				continue;

			bool bRet = true;
			switch( m_iSetupDimension )
			{
				case 2:
					bRet = CalculateLoudspeakerGains2D( vSoundSource, vdLoudspeakerGains );
					break;
				case 3:
					if( m_iNumSpreadingSources > 0 )
					{
						std::vector<VAVec3> pvSpreadingSources;
						CalcualteSpreadingSources( vSoundSource, VAVec3( 0, 0, -1 ), VAVec3( 0, 1, 0 ), (float)m_dSpreadingAngle, m_iNumSpreadingSources,
						                           pvSpreadingSources );
						std::vector<VistaVector3D> vvSpreadingSources;
						for( int i = 0; i < m_iNumSpreadingSources; i++ )
							vvSpreadingSources.push_back( VAVec3ToVistaVector3D( pvSpreadingSources[i] ) );
						bRet = CalculateMDAPGains3D( m_pTriangulation, vvSpreadingSources, vdLoudspeakerGains );
					}
					else
						bRet = CalculateVBAPGains3D( m_pTriangulation, VAVec3ToVistaVector3D( vSoundSource ), vdLoudspeakerGains );
					break;
				default:
					continue;
			}

			if( !bRet )
				VA_WARN( m_oParams.sID, "Could not calculate VBAP gains" );

			double dOverallGain = pVirtualSourceState->GetVolume( m_oParams.pCore->GetCoreConfig( )->dDefaultAmplitudeCalibration ); // Lautsärke der Quelle einstellen
			dOverallGain /= vSoundSource.Length( );                                                                                  // 1/r Gesetz
			for( int k = 0; k < vdLoudspeakerGains.size( ); k++ )                                                                    // anwenden
			{
				vdLoudspeakerGains[k] *= dOverallGain * m_vv3RelLoudspeakerPos[k].Length( );
				vdLoudspeakerGains[k] = std::min( 1.0, vdLoudspeakerGains[k] ); // limitieren
			}

			// Cross fade gains
			CVASoundSourceDesc* pSourceData = m_oParams.pCore->GetSceneManager( )->GetSoundSourceDesc( iID );
			const ITASampleFrame& sfInputBuffer( *pSourceData->psfSignalSourceInputBuf );
			const ITASampleBuffer* psbInput( &( sfInputBuffer[0] ) );
			const float* pfInputData = psbInput->data( );
			const int iBlocklength   = m_pdsOutput->GetBlocklength( );
			for( unsigned int j = 0; j < m_pdsOutput->GetNumberOfChannels( ); j++ )
			{
				float* pfOutputData = m_pdsOutput->GetWritePointer( j );

				if( ( vdLoudspeakerLastGains[j] == 0 ) && ( vdLoudspeakerGains[j] == 0 ) )
					continue;

				if( !( iAuralizationMode & IVAInterface::VA_AURAMODE_DIRECT_SOUND ) )
					vdLoudspeakerGains[j] = 0.0f; // No direct sound -> no gains

				for( int k = 0; k < iBlocklength; k++ )
				{
					double dcos = cos( k / iBlocklength * ( ITAConstants::PI_F / 2 ) );
					pfOutputData[k] += pfInputData[k] * float( ( dcos * dcos * ( vdLoudspeakerLastGains[j] - vdLoudspeakerGains[j] ) + vdLoudspeakerGains[j] ) );
				}

				vdLoudspeakerLastGains[j] = vdLoudspeakerGains[j];
				vdLoudspeakerGains[j]     = 0.0f;
			}
		}
	}

	m_pdsOutput->IncrementWritePointer( );

	if( ctxAudio.m_iResetFlag == 2 )
	{
		if( m_pCurSceneState )
		{
			m_pCurSceneState->RemoveReference( );
			m_pCurSceneState = nullptr;
		}
		ctxAudio.m_iResetFlag = 0;
	}
}

void CVAVBAPFreeFieldAudioRenderer::SyncInternalData( )
{
	// Über aktuelle Pfade iterieren und gelöschte entfernen

	std::list<SoundPath>::iterator it = m_lSoundPaths.begin( );
	while( it != m_lSoundPaths.end( ) )
	{
		SoundPath& oPath( *it );
		int iSourceID = oPath.iSourceID;

		if( oPath.bMarkDeleted )
			it = m_lSoundPaths.erase( it );
		else
			++it;
	}

	// Add new pathes to internal list
	std::list<SoundPath>::const_iterator cit = m_lNewSoundPaths.begin( );
	while( cit != m_lNewSoundPaths.end( ) )
	{
		m_lSoundPaths.push_back( *cit++ );
	}

	m_lNewSoundPaths.clear( );

	return;
}


// -------- Renderer implementation --------
void CVAVBAPFreeFieldAudioRenderer::CalculateInverseMatrix3x3( const double* dOriginalMatrix, double* dInverseMatrix ) const
{
	// Matritzen werden zeilenweise gelesen/geschrieben

	// bennene Koeffizienten
	// Zeile 1
	const double& a11 = dOriginalMatrix[0];
	const double& a12 = dOriginalMatrix[1];
	const double& a13 = dOriginalMatrix[2];
	// Zeile 2
	const double& a21 = dOriginalMatrix[3];
	const double& a22 = dOriginalMatrix[4];
	const double& a23 = dOriginalMatrix[5];
	// Zeile 3
	const double& a31 = dOriginalMatrix[6];
	const double& a32 = dOriginalMatrix[7];
	const double& a33 = dOriginalMatrix[8];

	// finde Determinante
	double dDetOriginalMatrix = ( a11 * a22 * a33 ) + ( a12 * a23 * a31 ) + ( a13 * a21 * a32 ) - ( a13 * a22 * a31 ) - ( a12 * a21 * a33 ) - ( a11 * a23 * a32 );

	// Koeffizienten der Transponierten
	// Zeile 1
	const double& t11 = a11;
	const double& t12 = a21;
	const double& t13 = a31;
	// Zeile 2
	const double& t21 = a12;
	const double& t22 = a22;
	const double& t23 = a32;
	// Zeile 3
	const double& t31 = a13;
	const double& t32 = a23;
	const double& t33 = a33;


	// Finde die Determinanten der Untermatrix
	// Zeile 1
	dInverseMatrix[0] = t22 * t33 - t23 * t32;
	dInverseMatrix[1] = t21 * t33 - t23 * t31;
	dInverseMatrix[2] = t21 * t32 - t22 * t31;
	// Zeile 2
	dInverseMatrix[3] = t12 * t33 - t13 * t32;
	dInverseMatrix[4] = t11 * t33 - t13 * t31;
	dInverseMatrix[5] = t11 * t32 - t12 * t31;
	// Zeile 3
	dInverseMatrix[6] = t12 * t23 - t13 * t22;
	dInverseMatrix[7] = t11 * t23 - t13 * t21;
	dInverseMatrix[8] = t11 * t22 - t12 * t21;


	// Bilde neue Matrix mit Vorzeichen
	// Zeile 1
	dInverseMatrix[0] /= dDetOriginalMatrix;
	dInverseMatrix[1] /= -dDetOriginalMatrix;
	dInverseMatrix[2] /= dDetOriginalMatrix;
	// Zeile 2
	dInverseMatrix[3] /= -dDetOriginalMatrix;
	dInverseMatrix[4] /= dDetOriginalMatrix;
	dInverseMatrix[5] /= -dDetOriginalMatrix;
	// Zeile 3
	dInverseMatrix[6] /= dDetOriginalMatrix;
	dInverseMatrix[7] /= -dDetOriginalMatrix;
	dInverseMatrix[8] /= dDetOriginalMatrix;
}

bool CVAVBAPFreeFieldAudioRenderer::CalculateLoudspeakerGains3D( const VAVec3& vSoundSource, std::vector<double>& vdLoudspeakerGains ) const
{
	return CalculateVBAPGains3D( m_pTriangulation, VAVec3ToVistaVector3D( vSoundSource ), vdLoudspeakerGains );

	// Cave Center is given as [0, 0, 0] but speakers are centered in [0,1.34,0]
	VistaVector3D vvCaveCenter = VistaVector3D( m_vReproCenterReal.x, m_vReproCenterReal.y + 1.34, m_vReproCenterReal.z );
	// VistaVector3D  vvCaveCenter = VistaVector3D(m_vReproCenterReal.x, m_vReproCenterReal.y, m_vReproCenterReal.z);


	VistaVector3D ContactPoint; // Will be defined/necessary  in IntersectionTriangle
	// Build a LineSegment from SoundSource to the Listener to find a corresponding triangle in the triangulation


	VistaVector3D vvSoundSource = VistaVector3D( vSoundSource.x, vSoundSource.y, vSoundSource.z );
	// calculate radius of speakers
	double dCenterToSpeaker =
	    VistaLineSegment( VistaVector3D( m_voTriangulationLoudspeaker[0][0].pos.x, m_voTriangulationLoudspeaker[0][0].pos.y, m_voTriangulationLoudspeaker[0][0].pos.z ),
	                      vvCaveCenter )
	        .GetLength( );
	double dSourceToCenter = VistaLineSegment( vvSoundSource, vvCaveCenter ).GetLength( );

	// double the length of the line until it reaches outside of the sphere of loudspeaker positions
	VistaVector3D vvDirectionVector = vvSoundSource - vvCaveCenter;
	while( dCenterToSpeaker > dSourceToCenter )
	{
		vvDirectionVector *= 2;
		dSourceToCenter = VistaLineSegment( vvSoundSource + vvDirectionVector, vvCaveCenter ).GetLength( );
	}


	VistaLineSegment vlSoundSourceToListener = VistaLineSegment( vvSoundSource + vvDirectionVector, vvCaveCenter );


	// To be filled with three speakers corresponding to the triangulation
	std::vector<CLoudspeaker> listLoudspeakerToBeUsed;
	listLoudspeakerToBeUsed.reserve( 3 );


	// ToDo: Which triangle has to be chosen when the linesegment crosses precisely on edge of the triangulation
	// Choose only two spepaker if edge and only one speaker if it hits one node


	for( size_t i = 0; i < m_voTriangulationLoudspeaker.size( ); i++ )
	{
		VistaVector3D speaker0 =
		    VistaVector3D( m_voTriangulationLoudspeaker[i][0].pos.x, m_voTriangulationLoudspeaker[i][0].pos.y, m_voTriangulationLoudspeaker[i][0].pos.z );
		VistaVector3D speaker1 =
		    VistaVector3D( m_voTriangulationLoudspeaker[i][1].pos.x, m_voTriangulationLoudspeaker[i][1].pos.y, m_voTriangulationLoudspeaker[i][1].pos.z );
		VistaVector3D speaker2 =
		    VistaVector3D( m_voTriangulationLoudspeaker[i][2].pos.x, m_voTriangulationLoudspeaker[i][2].pos.y, m_voTriangulationLoudspeaker[i][2].pos.z );

		// find triangle in triangulation
		if( vlSoundSourceToListener.IntersectionTriangle( speaker0, speaker1, speaker2, ContactPoint ) )
		{
			// calculate Distance from sound source to contact point --> closest contact point is the correct triangle
			VistaLineSegment vlSoundSourceToContactPoint = VistaLineSegment( VistaVector3D( vSoundSource.x, vSoundSource.y, vSoundSource.z ), ContactPoint );


			// The line segment will hit triangle of sphere --> caveCenter --> triangle of sphere
			// we only want to find the first triangle
			if( vlSoundSourceToContactPoint.GetLength( ) <= vlSoundSourceToListener.GetLength( ) )
			{
				// store the three loudspeaker that build the wished triangle
				// listLoudspeakerToBeUsed = m_voTriangulationLoudspeaker[i];
				listLoudspeakerToBeUsed.insert( listLoudspeakerToBeUsed.end( ), m_voTriangulationLoudspeaker[i][0] );
				listLoudspeakerToBeUsed.insert( listLoudspeakerToBeUsed.end( ), m_voTriangulationLoudspeaker[i][1] );
				listLoudspeakerToBeUsed.insert( listLoudspeakerToBeUsed.end( ), m_voTriangulationLoudspeaker[i][2] );
			}
		}
	}


	if( listLoudspeakerToBeUsed.empty( ) )
	{
		// VA_ERROR("VBAPFreefieldRenderer", "Could not apply unkown playback action '" << iPlayStateAction << "'");
		VA_WARN( "VBAPFreefieldRenderer", "m_vReproCenterReal is not the center of the loudspeaker array" );
		return false;
	}

	/*
	//ToDo: Result: 2 schnittpunkte; berechne welcher näher zu vSoundSource
	for (size_t i = 0; i < m_voTriangulationLoudspeaker.size(); i++) {
	    //find triangle in triangulation
	    VistaVector3D speaker1 = VistaVector3D(m_voTriangulationLoudspeaker[i][0].pos.x, m_voTriangulationLoudspeaker[i][0].pos.y,
	m_voTriangulationLoudspeaker[i][0].pos.z); VistaVector3D speaker2 = VistaVector3D(m_voTriangulationLoudspeaker[i][1].pos.x, m_voTriangulationLoudspeaker[i][1].pos.y,
	m_voTriangulationLoudspeaker[i][1].pos.z); VistaVector3D speaker3 = VistaVector3D(m_voTriangulationLoudspeaker[i][2].pos.x, m_voTriangulationLoudspeaker[i][2].pos.y,
	m_voTriangulationLoudspeaker[i][2].pos.z); if (vlSoundSourceToListener.IntersectionTriangle(speaker1, speaker2, speaker3, ContactPoint)) {
	        //store the three loudspeaker that build the wished triangle
	        listLoudspeakerToBeUsed = m_voTriangulationLoudspeaker[i];
	    }
	}
	*/


	// Starting from here std::vector<CLoudspeaker> listLoudspeakerToBeUsed are the three loudspeaker which should be used according to the triangulation


	// END NEW STUFF%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%


	// IS THIS PART UNESSARY/OLD?
	/* Create list of all loudspeakers which from all positions are given in relation from virtual source position
	std::vector< CLoudspeaker > voLoudSpeakerFromSoundSource;
	for (size_t i = 0; i < m_voLoudspeaker.size(); i++)
	{
	    // Calculate position of loudspeaker from virtual sound source
	    double positionValues[3];
	    positionValues[0] = m_voLoudspeaker[i].pos.x;
	    positionValues[1] = m_voLoudspeaker[i].pos.y;
	    positionValues[2] = m_voLoudspeaker[i].pos.z;
	    VAVec3 vLoudSpeakerPos(positionValues[0], positionValues[1], positionValues[2]);
	    VAVec3 vPosLoudSpeakerFromSoundSource = vLoudSpeakerPos - vSoundSource - m_vReproCenterReal;
	    VAVec3 NewPosition( vPosLoudSpeakerFromSoundSource.x, vPosLoudSpeakerFromSoundSource.y, vPosLoudSpeakerFromSoundSource.z );
	    // Insert in new Loudspeaker list with new position in relation to virtual sound source position
	    voLoudSpeakerFromSoundSource.push_back(m_voLoudspeaker[i]);
	    voLoudSpeakerFromSoundSource[i].pos = NewPosition;
	}

	std::sort(voLoudSpeakerFromSoundSource.begin(), voLoudSpeakerFromSoundSource.end(), [](CLoudspeaker first, CLoudspeaker second) { return first.pos.Length() <
	second.pos.Length(); } );
	*/


	// VAVec3 posLS1 = m_voLoudspeaker[voLoudSpeakerFromSoundSource[0].iIdentifier].pos - m_vReproCenterReal;
	// VAVec3 posLS2 = m_voLoudspeaker[voLoudSpeakerFromSoundSource[1].iIdentifier].pos - m_vReproCenterReal;
	// VAVec3 posLS3 = m_voLoudspeaker[voLoudSpeakerFromSoundSource[2].iIdentifier].pos - m_vReproCenterReal;


	double* pMatrix;
	double* pInverse;
	double Matrix[9];
	double dInverseMatrix[9];


	//%%%%%%%%%%%%%%%%%%%%%%%NEW%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

	VAVec3 posLS1 = m_voLoudspeaker[listLoudspeakerToBeUsed[0].iIdentifier].pos - m_vReproCenterReal;
	VAVec3 posLS2 = m_voLoudspeaker[listLoudspeakerToBeUsed[1].iIdentifier].pos - m_vReproCenterReal;
	VAVec3 posLS3 = m_voLoudspeaker[listLoudspeakerToBeUsed[2].iIdentifier].pos - m_vReproCenterReal;

	// VAVec3 posLS1 = m_voLoudspeaker[listLoudspeakerToBeUsed[0].iIdentifier].pos - VAVec3(vvCaveCenter[0], vvCaveCenter[1], vvCaveCenter[2]);
	// VAVec3 posLS2 = m_voLoudspeaker[listLoudspeakerToBeUsed[1].iIdentifier].pos - VAVec3(vvCaveCenter[0], vvCaveCenter[1], vvCaveCenter[2]);
	// VAVec3 posLS3 = m_voLoudspeaker[listLoudspeakerToBeUsed[2].iIdentifier].pos - VAVec3(vvCaveCenter[0], vvCaveCenter[1], vvCaveCenter[2]);

	//%%%%%%%%%%%%%%%%%%%%%%%NEW%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

	posLS1.Norm( );
	posLS2.Norm( );
	posLS3.Norm( );


	Matrix[0] = posLS1.x;
	Matrix[1] = posLS1.y;
	Matrix[2] = posLS1.z;

	Matrix[3] = posLS2.x;
	Matrix[4] = posLS2.y;
	Matrix[5] = posLS2.z;

	Matrix[6] = posLS3.x;
	Matrix[7] = posLS3.y;
	Matrix[8] = posLS3.z;


	dInverseMatrix[0] = 0;
	dInverseMatrix[1] = 0;
	dInverseMatrix[2] = 0;

	dInverseMatrix[3] = 0;
	dInverseMatrix[4] = 0;
	dInverseMatrix[5] = 0;

	dInverseMatrix[6] = 0;
	dInverseMatrix[7] = 0;
	dInverseMatrix[8] = 0;

	pInverse = dInverseMatrix;
	pMatrix  = Matrix;
	CalculateInverseMatrix3x3( pMatrix, pInverse );

	/* old
	// Get loudspeaker identifier
	const int &LSID1 = voLoudSpeakerFromSoundSource[0].iIdentifier;
	const int &LSID2 = voLoudSpeakerFromSoundSource[1].iIdentifier;
	const int &LSID3 = voLoudSpeakerFromSoundSource[2].iIdentifier;
	*/

	//%%%%%%%%%%%%%%%%%%%%%%%NEW%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

	// Get loudspeaker identifier
	const int& LSID1 = listLoudspeakerToBeUsed[0].iIdentifier;
	const int& LSID2 = listLoudspeakerToBeUsed[1].iIdentifier;
	const int& LSID3 = listLoudspeakerToBeUsed[2].iIdentifier;

	//%%%%%%%%%%%%%%%%%%%%%%%NEW%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%


	vdLoudspeakerGains[LSID1] = vSoundSource.x * dInverseMatrix[0] + vSoundSource.y * dInverseMatrix[3] + vSoundSource.z * dInverseMatrix[6];
	vdLoudspeakerGains[LSID2] = vSoundSource.x * dInverseMatrix[1] + vSoundSource.y * dInverseMatrix[4] + vSoundSource.z * dInverseMatrix[7];
	vdLoudspeakerGains[LSID3] = vSoundSource.x * dInverseMatrix[2] + vSoundSource.y * dInverseMatrix[5] + vSoundSource.z * dInverseMatrix[8];

	double dCorrection = sqrt( vdLoudspeakerGains[LSID1] * vdLoudspeakerGains[LSID1] + vdLoudspeakerGains[LSID2] * vdLoudspeakerGains[LSID2] +
	                           vdLoudspeakerGains[LSID3] * vdLoudspeakerGains[LSID3] );

	vdLoudspeakerGains[LSID1] /= dCorrection;
	vdLoudspeakerGains[LSID2] /= dCorrection;
	vdLoudspeakerGains[LSID3] /= dCorrection;


	return true;
}


bool CVAVBAPFreeFieldAudioRenderer::CalculateLoudspeakerGains2D( const VAVec3& vSoundSource, std::vector<double>& vdLoudspeakerGains ) const
{
	std::vector<int> viMinIDs; // Die Indices der zu nutzenden Lautsprecher
	viMinIDs.resize( 2 );
	std::vector<double> vdMinValues; // Der Abstand des Lautsprecher zur Quelle
	vdMinValues.resize( 2 );

	for( int j = 0; j < 2; j++ )
	{
		viMinIDs[j]    = -1;
		vdMinValues[j] = 8e9; // unendlich wert spezifizieren (optional)
	}

	// Aktive Lautsprecher finden 2D
	for( size_t j = 0; j < m_voLoudspeaker.size( ); j++ ) // Schleife über LS

	{
		// Entfernung berechnen
		// VistaVector3D vDifference = m_voLoudspeaker[ j ].pos - VistaVector3D( vSoundSource.comp );
		VAVec3 vDifference = m_voLoudspeaker[j].pos - vSoundSource;
		double dDistance   = vDifference.Length( );

		if( dDistance < *max_element( vdMinValues.begin( ), vdMinValues.end( ) ) )
		{
			// @todo: Warning klären ...
			int iIDChange          = int( max_element( vdMinValues.begin( ), vdMinValues.end( ) ) - vdMinValues.begin( ) );
			viMinIDs[iIDChange]    = int( j );
			vdMinValues[iIDChange] = dDistance;
		}
	}

	int iLS1 = viMinIDs[0];
	int iLS2 = viMinIDs[1];

	const VAVec3& posLS1 = m_voLoudspeaker[iLS1].pos;
	const VAVec3& posLS2 = m_voLoudspeaker[iLS2].pos;

	if( ( viMinIDs[0] == -1 ) || ( viMinIDs[1] == -1 ) )
		return false;


	// Berechnung der Gains über die Inverse
	double dDetA = ( -posLS1.z * posLS2.x ) - ( -posLS2.z * posLS1.x );

	if( dDetA == 0.0f )
		return false;

	// Berechnung der Gains nach Pulkki
	vdLoudspeakerGains[viMinIDs[0]] = ( 1 / dDetA * ( ( -vSoundSource.z * posLS2.x ) + ( vSoundSource.x * posLS2.z ) ) );
	vdLoudspeakerGains[viMinIDs[1]] = ( 1 / dDetA * ( ( -vSoundSource.z * -posLS1.x ) + ( vSoundSource.x * -posLS1.z ) ) );

	// Normalisierung nach Pulkki
	double dCorrection = sqrt( vdLoudspeakerGains[viMinIDs[0]] * vdLoudspeakerGains[viMinIDs[0]] + vdLoudspeakerGains[viMinIDs[1]] * vdLoudspeakerGains[viMinIDs[1]] );
	vdLoudspeakerGains[viMinIDs[0]] /= dCorrection;
	vdLoudspeakerGains[viMinIDs[1]] /= dCorrection;

	return true;
}

VAVec3 CVAVBAPFreeFieldAudioRenderer::GetSourcePosition( const CVAMotionState* pMotionState )
{
	VAVec3 vSoundSource;
	vSoundSource.x = ( pMotionState->GetPosition( ).x - m_vReproCenterVirtual.x );
	vSoundSource.y = ( pMotionState->GetPosition( ).y - m_vReproCenterVirtual.y );
	vSoundSource.z = ( pMotionState->GetPosition( ).z - m_vReproCenterVirtual.z );

	return vSoundSource;
}

#endif // VACORE_WITH_RENDERER_VBAP_FREE_FIELD
