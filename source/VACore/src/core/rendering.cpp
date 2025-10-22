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

#include "core.h"


void CVACoreImpl::SetRenderingModuleParameters( const std::string& sModuleID, const CVAStruct& oParams )
{
	for( size_t n = 0; n < m_voRenderers.size( ); n++ )
	{
		const CVAAudioRendererDesc& oRend( m_voRenderers[n] );
		if( oRend.sID == sModuleID )
		{
			// Check for recording parameters and apply if necessary

			if( oParams.HasKey( "RecordOutputEnabled" ) )
				VA_EXCEPT2( INVALID_PARAMETER, "Recording has to be enabled before streaming is active" );

			if( oRend.pOutputRecorder ) // If enabled ...
			{
				VistaFileSystemFile oFilePath( oRend.pOutputRecorder->GetFilePath( ) );
				std::string sFileName   = oFilePath.GetLocalName( );
				std::string sBaseFolder = oFilePath.GetParentDirectory( );

				bool bUpdateRecordOutputPath = false;
				if( oParams.HasKey( "RecordOutputFileName" ) )
				{
					sFileName               = std::string( oParams["RecordOutputFileName"] );
					bUpdateRecordOutputPath = true;
				}
				if( oParams.HasKey( "RecordOutputBaseFolder" ) )
				{
					sBaseFolder             = std::string( oParams["RecordOutputBaseFolder"] );
					bUpdateRecordOutputPath = true;
				}

				if( bUpdateRecordOutputPath )
				{
					VistaFileSystemFile oFile( sFileName );
					VistaFileSystemDirectory oFolder( sBaseFolder );

					VistaFileSystemFile oFilePath( oFolder.GetName( ) + "/" + oFile.GetLocalName( ) );
					VA_INFO( "Core", "Updating rendering output recording file path to " << oFilePath.GetName( ) );

					if( oFilePath.Exists( ) )
						VA_INFO( "Core", "Record rendering output recording file path exists, will overwrite" );

					if( !oFolder.Exists( ) )
						if( !oFolder.CreateWithParentDirectories( ) )
							VA_EXCEPT2( INVALID_PARAMETER, "Could not create rendering output recording directory " + oFolder.GetName( ) );

					oRend.pOutputRecorder->SetFilePath( oFilePath.GetName( ) );
				}
			}

			// Propagate parameters
			oRend.pInstance->SetParameters( oParams );

			return;
		}
	}

	VA_ERROR( "Core", "Could not find rendering module '" << sModuleID << "'" );
}

CVAStruct CVACoreImpl::GetRenderingModuleParameters( const std::string& sModuleID, const CVAStruct& oParams ) const
{
	for( size_t n = 0; n < m_voRenderers.size( ); n++ )
	{
		const CVAAudioRendererDesc& oRend( m_voRenderers[n] );
		if( oRend.sID == sModuleID )
			return oRend.pInstance->GetParameters( oParams );
	}

	VA_ERROR( "Core", "Could not find rendering module '" << sModuleID << "'" );
	return CVAStruct( );
}

void CVACoreImpl::GetRenderingModules( std::vector<CVAAudioRendererInfo>& vRenderers, const bool bFilterEnabled /* = true */ ) const
{
	vRenderers.clear( );
	for( size_t i = 0; i < m_voRenderers.size( ); i++ )
	{
		const CVAAudioRendererDesc& oRenderer( m_voRenderers[i] );
		CVAAudioRendererInfo oRendererInfo;
		oRendererInfo.sID                    = oRenderer.sID;
		oRendererInfo.sClass                 = oRenderer.sClass;
		oRendererInfo.sDescription           = oRendererInfo.sDescription;
		oRendererInfo.bEnabled               = oRenderer.bEnabled;
		oRendererInfo.bOutputDetectorEnabled = oRenderer.bOutputDetectorEnabled;
		oRendererInfo.oParams                = oRenderer.oParams;
		if( oRenderer.pOutputRecorder )
			oRendererInfo.sOutputRecordingFilePath = oRenderer.pOutputRecorder->GetFilePath( );
		if( !bFilterEnabled || oRendererInfo.bEnabled )
			vRenderers.push_back( oRendererInfo );
	}
}


void CVACoreImpl::SetRenderingModuleMuted( const std::string& sModuleID, const bool bMuted )
{
	for( size_t n = 0; n < m_voRenderers.size( ); n++ )
	{
		const CVAAudioRendererDesc& oRend( m_voRenderers[n] );
		if( oRend.sID == sModuleID )
		{
			m_pR2RPatchbay->SetInputMuted( oRend.iR2RPatchBayInput, bMuted );
			return;
		}
	}

	VA_ERROR( "Core", "Could not find rendering module '" << sModuleID << "'" );
}

void CVACoreImpl::SetRenderingModuleGain( const std::string& sModuleID, const double dGain )
{
	for( size_t n = 0; n < m_voRenderers.size( ); n++ )
	{
		const CVAAudioRendererDesc& oRend( m_voRenderers[n] );
		if( oRend.sID == sModuleID )
		{
			m_pR2RPatchbay->SetInputGain( oRend.iR2RPatchBayInput, dGain );
			return;
		}
	}

	VA_ERROR( "Core", "Could not find rendering module '" << sModuleID << "'" );
}

double CVACoreImpl::GetRenderingModuleGain( const std::string& sModuleID ) const
{
	for( size_t n = 0; n < m_voRenderers.size( ); n++ )
	{
		const CVAAudioRendererDesc& oRend( m_voRenderers[n] );
		if( oRend.sID == sModuleID )
			return m_pR2RPatchbay->GetInputGain( oRend.iR2RPatchBayInput );
	}

	VA_ERROR( "Core", "Could not find rendering module '" << sModuleID << "'" );
	return 0.0f;
}

void CVACoreImpl::SetRenderingModuleAuralizationMode( const std::string& sModuleID, const int iAM )
{
	for( size_t n = 0; n < m_voRenderers.size( ); n++ )
	{
		const CVAAudioRendererDesc& oRend( m_voRenderers[n] );
		if( oRend.sID == sModuleID )
		{
			oRend.pInstance->UpdateGlobalAuralizationMode( iAM );
			return;
		}
	}

	VA_ERROR( "Core", "Could not find rendering module '" << sModuleID << "'" );
}

int CVACoreImpl::GetRenderingModuleAuralizationMode( const std::string& sModuleID ) const
{
	for( size_t n = 0; n < m_voRenderers.size( ); n++ )
	{
		const CVAAudioRendererDesc& oRend( m_voRenderers[n] );
		if( oRend.sID == sModuleID )
			return oRend.pInstance->GetAuralizationMode( );
	}

	VA_EXCEPT2( INVALID_PARAMETER, "Could not find rendering module '" + sModuleID + "'" );
}

bool CVACoreImpl::GetRenderingModuleMuted( const std::string& sModuleID ) const
{
	for( size_t n = 0; n < m_voRenderers.size( ); n++ )
	{
		const CVAAudioRendererDesc& oRend( m_voRenderers[n] );
		if( oRend.sID == sModuleID )
			return m_pR2RPatchbay->IsInputMuted( oRend.iR2RPatchBayInput );
	}

	VA_ERROR( "Core", "Could not find rendering module '" << sModuleID << "'" );
	return true;
}

void CVACoreImpl::SendRenderingModuleOutputDetectorsUpdateEvents( )
{
	for( size_t i = 0; i < m_voRenderers.size( ); i++ )
	{
		CVAAudioRendererDesc& oRenderer( m_voRenderers[i] );

		CVAEvent ev;
		ev.iEventType = CVAEvent::MEASURES_UPDATE;
		ev.pSender    = this;
		ev.sObjectID  = oRenderer.sID;
		ev.sParam     = "RenderingModule"; // Use generic parameter slot to mark rendering module source

		if( oRenderer.pOutputDetector )
		{
			ev.vfOutputRMSs.resize( oRenderer.pOutputDetector->GetNumberOfChannels( ) );
			oRenderer.pOutputDetector->GetRMSs( ev.vfOutputRMSs, true );
			ev.vfOutputPeaks.resize( oRenderer.pOutputDetector->GetNumberOfChannels( ) );
			oRenderer.pOutputDetector->GetPeaks( ev.vfOutputPeaks, true );
		}

		m_pEventManager->BroadcastEvent( ev );
	}
}
