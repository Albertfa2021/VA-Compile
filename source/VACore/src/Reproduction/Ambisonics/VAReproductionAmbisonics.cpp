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

#include "VAReproductionAmbisonics.h"

#ifdef VACORE_WITH_REPRODUCTION_AMBISONICS

// VA includes
#	include "VAAmbisonicsDecoders.h"
#	include "../../core/core.h"
#	include "../../Utils/VAUtils.h"
#	include "../../VALog.h"

// ITA includes
#	include <ITANumericUtils.h>
#	include <ITAStringUtils.h>

// Vista includes
#	include <VistaTools/VistaIniFileParser.h>

// STL includes
#	include <algorithm>

//! Constructor and Destructor
CVAAmbisonicsReproduction::CVAAmbisonicsReproduction( const CVAAudioReproductionInitParams& oParams ) : m_oParams( oParams )
{
	/* Get core parameters */
	double dSampleRate = m_oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.dSampleRate;
	int iBlockLength   = m_oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.iBuffersize;
	// Decoder input (B-Format with implicit channels)
	m_pDecoderMatrixPatchBay = new ITAStreamPatchbay( dSampleRate, iBlockLength );

	/* Get required parameters */
	CVAConfigInterpreter conf( *( m_oParams.pConfig ) );
	conf.ReqInteger( "TruncationOrder", m_iReproductionTruncationOrder );
	conf.ReqStringList( "Outputs", m_cReproductionConfig.m_vsOutputs );

	/* Optional parameters */
	// Reproduction center
	std::string sReproductionCenterPositions;
	conf.OptString( "ReproductionCenterPos", sReproductionCenterPositions, "AUTO" );
	m_cReproductionConfig.m_vsCenterPositions = splitString( sReproductionCenterPositions, ";" );
	// Decoder strategy
	conf.OptString( "Decoder", m_sDecoder, "MMAD" );
	if( m_sDecoder != "SAD" && m_sDecoder != "MMAD" && m_sDecoder != "AllRAD" && m_sDecoder != "EPAD" && m_sDecoder != "AllRAD+" )
		VA_EXCEPT2( INVALID_PARAMETER, "Don't know decoder strategy " + m_sDecoder + "! Try SAD, MMAD, AllRAD, AllRAD+ or EPAD" );
	if( m_sDecoder == "AllRAD" || m_sDecoder == "AllRAD+" )
	{
		m_bIsAllRAD = true;
		// Virtual output for Ambisonic decoding
		std::string sVirtualOutputs;
		conf.ReqString( "VirtualOutput", sVirtualOutputs );
		m_cReproductionConfig.m_vsVirtualOutputs = splitString( sVirtualOutputs, "," );

		// Handle convex hulls
		std::string sTriangulationFiles;
		conf.OptString( "OutputTriangulation", sTriangulationFiles );
		m_cReproductionConfig.m_vsTriangulationFiles = splitString( sTriangulationFiles, "," );
	}

	conf.OptBool( "UseRemax", m_bUseRemax, true );
	conf.OptBool( "UseNearFieldCompensation", m_bUseNFC, false );

	InitOutputConfigs( );
}

void CVAAmbisonicsReproduction::InitOutputConfigs( )
{
	// Check config conditions
	int iOutputs = m_cReproductionConfig.m_vsOutputs.size( ), iCenterPos = m_cReproductionConfig.m_vsCenterPositions.size( ),
	    iVirtualOutputs     = m_bIsAllRAD ? m_cReproductionConfig.m_vsVirtualOutputs.size( ) : 0,
	    iTriangulationFiles = m_bIsAllRAD ? m_cReproductionConfig.m_vsTriangulationFiles.size( ) : 0;

	if( iCenterPos > 1 && iCenterPos != iOutputs )
		VA_EXCEPT2( INVALID_PARAMETER, "Either specify one reproduction center for all outputs, or equally many positions seperated by semicolons (;)" )

	if( iVirtualOutputs > 1 && iVirtualOutputs != iOutputs )
		VA_EXCEPT2( INVALID_PARAMETER, "Please specify one virtual output for all outputs, or equally many outputs and virtual outputs" );

	if( iTriangulationFiles >= 1 && iTriangulationFiles != iOutputs )
		VA_EXCEPT2( INVALID_PARAMETER,
		            "Please specify equally many triangulation ini files as there are outputs, or leave corresponding positions empty for calculated triangulations" );

	// Initialize
	int iHardwareTruncationOrder, iCenterPosIdx, iVirtualOutputIdx, iTriangulationfileIdx;
	CTriangulation* ptTriangulation = nullptr;
	VAVec3 v3ReproductionCenterPos;
	VistaVector3D vReproductionCenterPos;
	std::vector<VistaVector3D> vvLoudspeakerPositions;
	for( int i = 0; i < iOutputs; i++ ) //
	{
		const CVAHardwareOutput* pOutput = GetHardwareOutput( m_cReproductionConfig.m_vsOutputs[i] );

		iCenterPosIdx = iCenterPos == 1 ? 0 : i;
		GetReproductionCenterPos( m_cReproductionConfig.m_vsCenterPositions[iCenterPosIdx], pOutput, v3ReproductionCenterPos );
		vReproductionCenterPos = VAVec3ToVistaVector3D( v3ReproductionCenterPos );

		if( !m_bIsAllRAD )
		{
			// Non-AllRAD Decoder only need setup info, setup center, truncation order
			iHardwareTruncationOrder = std::floor( std::sqrt( pOutput->vpDevices.size( ) - 1 ) );
			if( iHardwareTruncationOrder < m_iReproductionTruncationOrder )
				VA_WARN( m_oParams.sClass,
				         "Truncation order of output " + pOutput->sIdentifier + " is lower than truncation order given for reproduction. Higher orders will be dropped" );

			SOutputConfig sOutputConfig = { pOutput, v3ReproductionCenterPos, nullptr, nullptr, std::min( iHardwareTruncationOrder, m_iReproductionTruncationOrder ) };
			m_vpOutputConfigs.push_back( sOutputConfig );
		}
		else
		{
			// AllRAD Decoder needs t-Design + truncation order, real setup triangulation
			int j =
			    iVirtualOutputs == 1 ? 0 : i; // Use same virtual output for all outputs, if only one is given, otherwise iterate with the outputs for a 1-to-1 relation
			const CVAHardwareOutput* pVirtualOutput = GetHardwareOutput( m_cReproductionConfig.m_vsVirtualOutputs[j], true );

			iHardwareTruncationOrder =
			    std::floor( std::sqrt( pVirtualOutput->vpDevices.size( ) -
			                           1 ) ); // we don't care about the reproduction truncation order in AllRAD because it soley depends on the virtual output
			if( iHardwareTruncationOrder < m_iReproductionTruncationOrder )
				VA_WARN( m_oParams.sClass, "Truncation order of virtual output " + pVirtualOutput->sIdentifier +
				                               " is lower than truncation order given for reproduction. Higher orders will be dropped" );


			for( int l = 0; l < pOutput->vpDevices.size( ); l++ )
			{
				vvLoudspeakerPositions.push_back( VAVec3ToVistaVector3D( pOutput->vpDevices[l]->vPos ) );
			}

			int k = iTriangulationFiles == 1 ? 0 : i;
			if( m_cReproductionConfig.m_vsTriangulationFiles[k].empty( ) || m_cReproductionConfig.m_vsTriangulationFiles[k] == " " )
			{
				ptTriangulation = new CTriangulation( vvLoudspeakerPositions, vReproductionCenterPos );
			}
			else
			{
				std::vector<VistaVector3D> vvTriangulationIndices;
				ReadTriangulationFromFile( m_cReproductionConfig.m_vsTriangulationFiles[k], vvTriangulationIndices );
				ptTriangulation = new CTriangulation( vvLoudspeakerPositions, vvTriangulationIndices, vReproductionCenterPos );
			}

			if( !ptTriangulation->IsDelaunayTriangulation( 1e-3 ) )
			{
				VA_WARN( m_oParams.sClass, "Triangulation does not conform to Delaunay condition" );
			}

			SOutputConfig sOutputConfig = { pOutput, v3ReproductionCenterPos, pVirtualOutput, ptTriangulation, iHardwareTruncationOrder };
			m_vpOutputConfigs.push_back( sOutputConfig );
		}
	}
}

const CVAHardwareOutput* CVAAmbisonicsReproduction::GetHardwareOutput( std::string& sOutputName, bool bVirtual )
{
	const CVAHardwareOutput* pOutput = m_oParams.pCore->GetCoreConfig( )->oHardwareSetup.GetOutput( sOutputName );
	if( pOutput == nullptr )
		VA_EXCEPT2( INVALID_PARAMETER, "Unrecognized output '" + sOutputName + "' for Ambisonics reproduction" );
	if( !( bVirtual || pOutput->bEnabled ) )
		VA_WARN( m_oParams.sClass, "Output " + pOutput->sIdentifier + " is not enabled, no sound will be played back over this output" );
	return pOutput;
}

void CVAAmbisonicsReproduction::GetReproductionCenterPos( const std::string& sReproductionCenterPos, const CVAHardwareOutput* pOutput, VAVec3& vec3CenterPos )
{
	if( sReproductionCenterPos == "AUTO" )
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
	else
	{
		std::vector<std::string> vsPosComponents = splitString( sReproductionCenterPos, ',' );
		assert( vsPosComponents.size( ) == 3 );
		vec3CenterPos.Set( StringToFloat( vsPosComponents[0] ), StringToFloat( vsPosComponents[1] ), StringToFloat( vsPosComponents[2] ) );
	}
}

void CVAAmbisonicsReproduction::ReadTriangulationFromFile( const std::string& sTriangulationFile, std::vector<VistaVector3D>& vvTriangulationIndices )
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

CVAAmbisonicsReproduction::~CVAAmbisonicsReproduction( )
{
	delete m_pDecoderMatrixPatchBay;
	m_pDecoderMatrixPatchBay = NULL;
}


//! Methods
void CVAAmbisonicsReproduction::SetInputDatasource( ITADatasource* p )
{
	// Receive data from renderer
	m_pDecoderMatrixPatchBay->AddInput( p );

	// Check renderer and reproduction truncation orders
	int iRenderingSHOrder = std::floor( std::sqrt( int( p->GetNumberOfChannels( ) ) - 1 ) );

	for( int i = 0; i < m_vpOutputConfigs.size( ); i++ )
	{
		if( iRenderingSHOrder != m_vpOutputConfigs[i].iReproductionOrder )
		{
			if( m_bIsAllRAD )
			{
				VA_INFO( m_oParams.sClass, m_oParams.sID + " with output " + m_vpOutputConfigs[i].m_pOutput->sIdentifier + " supports higher orders than rendered" );
			}
			else
			{
				VA_WARN( m_oParams.sClass, "Ambisonics order missmatch in renderer and reproduction modules, truncating higher orders" );
			}
			m_vpOutputConfigs[i].iReproductionOrder = std::min( iRenderingSHOrder, m_vpOutputConfigs[i].iReproductionOrder );
		}
	}
}

ITADatasource* CVAAmbisonicsReproduction::GetOutputDatasource( )
{
	for( size_t i = 0; i < m_vpOutputConfigs.size( ); i++ )
	{
		SOutputConfig sCurrentConfig = m_vpOutputConfigs[i];
		int iNumHOAChannels          = ( sCurrentConfig.iReproductionOrder + 1 ) * ( sCurrentConfig.iReproductionOrder + 1 );

		// Add Output to patchbay
		m_pDecoderMatrixPatchBay->AddOutput( int( sCurrentConfig.m_pOutput->GetPhysicalOutputChannels( ).size( ) ) );

		// Calculate decoder matrix
		Eigen::MatrixXd matDecoder;
		CalculateDecoderMatrix( sCurrentConfig, matDecoder );

		// Get max rE weights
		std::vector<double> vdReMaxWeights;
		if( m_bUseRemax )
			vdReMaxWeights = HOARemaxWeights( sCurrentConfig.iReproductionOrder );
		else
		{
			for( int n = 0; n < sCurrentConfig.iReproductionOrder; n++ )
				vdReMaxWeights.push_back( 1 );
		}

		// Apply weights to streaming patchbay
		//	  j   0  ... (N+1)^2
		// k  \  Y00 ...   Ynm
		// 1 LS1
		// :  :
		// n LSn
		int iCurrentOrder;
		double dGain;
		for( int j = 0; j < iNumHOAChannels; j++ )
		{
			iCurrentOrder = (int)std::floor( std::sqrt( j ) );
			for( size_t k = 0; k < sCurrentConfig.m_pOutput->GetPhysicalOutputChannels( ).size( ); k++ )
			{
				dGain = vdReMaxWeights[iCurrentOrder] * matDecoder( k, j );
				m_pDecoderMatrixPatchBay->ConnectChannels( 0, j, i, k, dGain );
			}
		}
	}

	return m_pDecoderMatrixPatchBay->GetOutputDatasource( 0 );
}

void CVAAmbisonicsReproduction::UpdateScene( CVASceneState* )
{
	// There is no need for dynamic modification of the ambisonics reproduction
	return;
}

int CVAAmbisonicsReproduction::GetNumInputChannels( ) const
{
	return ( m_iReproductionTruncationOrder + 1 ) * ( m_iReproductionTruncationOrder + 1 );
}

//! Decoder implementations
void CVAAmbisonicsReproduction::CalculateDecoderMatrix( const struct SOutputConfig& sOutputConfig, Eigen::MatrixXd& matDecoder )
{
	int iNumLoudspeakers   = !m_bIsAllRAD ? sOutputConfig.m_pOutput->vpDevices.size( ) : sOutputConfig.m_pVirtualOutput->vpDevices.size( ),
	    iReproductionOrder = sOutputConfig.iReproductionOrder, iNumHOAChannels = ( iReproductionOrder + 1 ) * ( iReproductionOrder + 1 );

	if( m_sDecoder == "SAD" )
	{
		CalculateSADMatrix( sOutputConfig.m_pOutput, sOutputConfig.m_vReproductioncenterPos, iReproductionOrder, matDecoder );
	}
	else if( m_sDecoder == "MMAD" )
	{
		CalculateMMADMatrix( sOutputConfig.m_pOutput, sOutputConfig.m_vReproductioncenterPos, iReproductionOrder, matDecoder );
	}
	else if( m_sDecoder == "EPAD" )
	{
		CalculateEPADMatrix( sOutputConfig.m_pOutput, sOutputConfig.m_vReproductioncenterPos, iReproductionOrder, matDecoder );
	}
	else if( m_sDecoder == "AllRAD" )
	{
		CalculateAllRADMatrix( sOutputConfig.m_pOutput, sOutputConfig.m_pVirtualOutput, sOutputConfig.m_vReproductioncenterPos, sOutputConfig.m_pTriangulation,
		                       iReproductionOrder, matDecoder );
	}
	else if( m_sDecoder == "AllRAD+" )
	{
		CalculateAllRADPlusMatrix( sOutputConfig.m_pOutput, sOutputConfig.m_pVirtualOutput, sOutputConfig.m_vReproductioncenterPos, sOutputConfig.m_pTriangulation,
		                           iReproductionOrder, matDecoder );
	}
	else
	{
		VA_EXCEPT2( INVALID_PARAMETER, "Unknown decoder strategy" + m_sDecoder + "! This error should not occur, decoder string change in runtime!" );
	}
}


#endif // VACORE_WITH_REPRODUCTION_AMBISONICS