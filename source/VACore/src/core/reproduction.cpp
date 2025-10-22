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

void CVACoreImpl::SetReproductionModuleParameters( const std::string& sModuleID, const CVAStruct& oParams )
{
	for( size_t n = 0; n < m_voReproductionModules.size( ); n++ )
	{
		const CVAAudioReproductionModuleDesc& oRep( m_voReproductionModules[n] );
		if( oRep.sID == sModuleID )
		{
			// Check for recording parameters and apply if necessary

			if( oParams.HasKey( "RecordInputEnabled" ) )
				VA_EXCEPT2( INVALID_PARAMETER, "Recording has to be enabled before streaming is active" );

			if( oRep.pInputRecorder )
			{
				VistaFileSystemFile oFilePath( oRep.pInputRecorder->GetFilePath( ) );
				std::string sFileName   = oFilePath.GetLocalName( );
				std::string sBaseFolder = oFilePath.GetParentDirectory( );

				bool bUpdateRecordInputPath = false;
				if( oParams.HasKey( "RecordInputFileName" ) )
				{
					sFileName              = std::string( oParams["RecordInputFileName"] );
					bUpdateRecordInputPath = true;
				}
				if( oParams.HasKey( "RecordInputBaseFolder" ) )
				{
					sBaseFolder            = std::string( oParams["RecordInputBaseFolder"] );
					bUpdateRecordInputPath = true;
				}

				if( bUpdateRecordInputPath )
				{
					VistaFileSystemFile oFile( sFileName );
					VistaFileSystemDirectory oFolder( sBaseFolder );

					VistaFileSystemFile oFilePath( oFolder.GetName( ) + "/" + oFile.GetLocalName( ) );
					VA_INFO( "Core", "Updating reproduction input recording file path to " << oFilePath.GetName( ) );

					if( oFilePath.Exists( ) )
						VA_INFO( "Core", "Record reproduction input recording file path exists, will overwrite" );

					if( !oFolder.Exists( ) )
						if( !oFolder.CreateWithParentDirectories( ) )
							VA_EXCEPT2( INVALID_PARAMETER, "Could not create reproduction input recording directory " + oFolder.GetName( ) );

					oRep.pInputRecorder->SetFilePath( oFilePath.GetName( ) );
				}
			}

			if( oParams.HasKey( "RecordOutputEnabled" ) )
				VA_EXCEPT2( INVALID_PARAMETER, "Recording has to be enabled before streaming is active" );

			if( oRep.pOutputRecorder )
			{
				VistaFileSystemFile oFilePath( oRep.pOutputRecorder->GetFilePath( ) );
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
					VA_INFO( "Core", "Updating reproduction output recording file path to " << oFilePath.GetName( ) );

					if( oFilePath.Exists( ) )
						VA_INFO( "Core", "Record reproduction output recording file path exists, will overwrite" );

					if( !oFolder.Exists( ) )
						if( !oFolder.CreateWithParentDirectories( ) )
							VA_EXCEPT2( INVALID_PARAMETER, "Could not create reproduction output recording directory " + oFolder.GetName( ) );

					oRep.pOutputRecorder->SetFilePath( oFilePath.GetName( ) );
				}
			}

			// Propagate parameters
			oRep.pInstance->SetParameters( oParams );
			return;
		}
	}

	VA_ERROR( "Core", "Could not find Reproduction module '" << sModuleID << "'" );
}

CVAStruct CVACoreImpl::GetReproductionModuleParameters( const std::string& sModuleID, const CVAStruct& oParams ) const
{
	for( size_t n = 0; n < m_voReproductionModules.size( ); n++ )
	{
		const CVAAudioReproductionModuleDesc& oRep( m_voReproductionModules[n] );
		if( oRep.sID == sModuleID )
			return oRep.pInstance->GetParameters( oParams );
	}

	VA_ERROR( "Core", "Could not find Reproduction module '" << sModuleID << "'" );
	return CVAStruct( );
}

void CVACoreImpl::GetReproductionModules( std::vector<CVAAudioReproductionInfo>& vRepros, const bool bFilterEnabled /* = true */ ) const
{
	vRepros.clear( );
	for( size_t i = 0; i < m_voReproductionModules.size( ); i++ )
	{
		const CVAAudioReproductionModuleDesc& oRepro( m_voReproductionModules[i] );
		CVAAudioReproductionInfo oRepInfo;
		oRepInfo.sID                   = oRepro.sID;
		oRepInfo.sClass                = oRepro.sClass;
		oRepInfo.sDescription          = oRepro.sDescription;
		oRepInfo.bEnabled              = oRepro.bEnabled;
		oRepInfo.bInputDetectorEnabled = oRepro.bInputDetectorEnabled;
		if( oRepro.pInputRecorder )
			oRepInfo.sInputRecordingFilePath = oRepro.pInputRecorder->GetFilePath( );
		oRepInfo.bOutputDetectorEnabled = oRepro.bOutputDetectorEnabled;
		if( oRepro.pOutputRecorder )
			oRepInfo.sOutputRecordingFilePath = oRepro.pOutputRecorder->GetFilePath( );
		oRepInfo.oParams = oRepro.oParams;
		if( !bFilterEnabled || oRepInfo.bEnabled )
			vRepros.push_back( oRepInfo );
	}
}

bool CVACoreImpl::GetReproductionModuleMuted( const std::string& sModuleID ) const
{
	for( size_t n = 0; n < m_voReproductionModules.size( ); n++ )
	{
		const CVAAudioReproductionModuleDesc& oRepro( m_voReproductionModules[n] );
		if( oRepro.sID == sModuleID )
			return m_pOutputPatchbay->IsInputMuted( oRepro.iOutputPatchBayInput );
	}

	VA_ERROR( "Core", "Could not find reproduction module '" << sModuleID << "'" );
	return true;
}

void CVACoreImpl::SetReproductionModuleGain( const std::string& sModuleID, const double dGain )
{
	for( size_t n = 0; n < m_voReproductionModules.size( ); n++ )
	{
		const CVAAudioReproductionModuleDesc& oRepro( m_voReproductionModules[n] );
		if( oRepro.sID == sModuleID )
		{
			m_pOutputPatchbay->SetInputGain( oRepro.iOutputPatchBayInput, dGain );
			return;
		}
	}

	VA_ERROR( "Core", "Could not find reproduction module '" << sModuleID << "'" );
}

double CVACoreImpl::GetReproductionModuleGain( const std::string& sModuleID ) const
{
	for( size_t n = 0; n < m_voReproductionModules.size( ); n++ )
	{
		const CVAAudioReproductionModuleDesc& oRepro( m_voReproductionModules[n] );
		if( oRepro.sID == sModuleID )
			return m_pOutputPatchbay->GetInputGain( oRepro.iOutputPatchBayInput );
	}

	VA_ERROR( "Core", "Could not find reproduction module '" << sModuleID << "'" );
	return 0.0f;
}

void CVACoreImpl::SetReproductionModuleMuted( const std::string& sModuleID, const bool bMuted )
{
	for( size_t n = 0; n < m_voReproductionModules.size( ); n++ )
	{
		const CVAAudioReproductionModuleDesc& oRepro( m_voReproductionModules[n] );
		if( oRepro.sID == sModuleID )
		{
			m_pOutputPatchbay->SetInputMuted( oRepro.iOutputPatchBayInput, bMuted );
			return;
		}
	}

	VA_ERROR( "Core", "Could not find reproduction module '" << sModuleID << "'" );
}

void CVACoreImpl::SendReproductionModuleOIDetectorsUpdateEvents( )
{
	for( size_t i = 0; i < m_voReproductionModules.size( ); i++ )
	{
		CVAAudioReproductionModuleDesc& oReproduction( m_voReproductionModules[i] );

		CVAEvent ev;
		ev.iEventType = CVAEvent::MEASURES_UPDATE;
		ev.pSender    = this;
		ev.sObjectID  = oReproduction.sID;
		ev.sParam     = "ReproductionModule"; // Use generic parameter slot to mark reproduction module source

		if( oReproduction.pInputDetector )
		{
			ev.vfInputRMSs.resize( oReproduction.pInputDetector->GetNumberOfChannels( ) );
			oReproduction.pInputDetector->GetRMSs( ev.vfInputRMSs, true );
			ev.vfInputPeaks.resize( oReproduction.pInputDetector->GetNumberOfChannels( ) );
			oReproduction.pInputDetector->GetPeaks( ev.vfInputPeaks, true );
		}

		if( oReproduction.pOutputDetector )
		{
			ev.vfOutputRMSs.resize( oReproduction.pOutputDetector->GetNumberOfChannels( ) );
			oReproduction.pOutputDetector->GetRMSs( ev.vfOutputRMSs, true );
			ev.vfOutputPeaks.resize( oReproduction.pOutputDetector->GetNumberOfChannels( ) );
			oReproduction.pOutputDetector->GetPeaks( ev.vfOutputPeaks, true );
		}

		m_pEventManager->BroadcastEvent( ev );
	}
}
