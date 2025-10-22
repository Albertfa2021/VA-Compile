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

#include "VARoomAcousticsRenderer.h"

#ifdef VACORE_WITH_RENDERER_ROOM_ACOUSTICS

#	include "../../Scene/VASoundReceiverDesc.h"
#	include "../../Scene/VASoundSourceDesc.h"
#	include "../../Utils/VAUtils.h"
#	include "../../VALog.h"
#	include "../../core/core.h"
#	include "../Base/VAAudioRendererReceiver.h"
#	include "../Base/VAAudioRendererSource.h"
#	include "ITA/SimulationScheduler/profiler.h"
#	include "VistaTools/VistaIniFileParser.h"

#	include <ITA/SimulationScheduler/RoomAcoustics/master_simulation_controller.h>


class ComplexSoundPath;

CVARoomAcousticsRenderer::Config::Config( const CVAAudioRendererInitParams& oParams, const Config& oDefaultValues )
    : CVAFIRRendererBase::Config( oParams, oDefaultValues )
{
	const CVAConfigInterpreter conf( *oParams.pConfig );

	conf.OptBool( "StoreProfileData", bStoreProfileData, oDefaultValues.bStoreProfileData );
}

CVARoomAcousticsRenderer::CVARoomAcousticsRenderer( const CVAAudioRendererInitParams& oParams ) : CVAFIRRendererBase( oParams, Config( ) ), m_oConf( oParams, Config( ) )
{
	if( m_oConf.iNumChannels != 2 )
		VA_EXCEPT2( INVALID_PARAMETER, "Renderer ID '" + oParams.sID + "': " + "Number of channels must be 2, currently only binaural reproduction is supported." );

	const auto sSimConfigFilePath = m_oConf.sSimulationConfigFilePath;

	VA_VERBOSE( "RoomAcousticsRenderer", "Attempting to load file from path '" << sSimConfigFilePath << "' as simulation config file" );

	ITA::SimulationScheduler::RoomAcoustics::CMasterSimulationController::MasterSimulationControllerConfig oMasterSimulationControllerConfig;
	VistaPropertyList oSimulationProperties;

	size_t i = sSimConfigFilePath.rfind( '.', sSimConfigFilePath.length( ) );
	if( i != std::string::npos )
	{
		const auto sFileExtension = sSimConfigFilePath.substr( i + 1, sSimConfigFilePath.length( ) - i );

		if( toLowercase( sFileExtension ) == "json" )
		{
			oMasterSimulationControllerConfig.Load( ITA::SimulationScheduler::Utils::JSONConfigUtils::LoadJSONConfig( sSimConfigFilePath ) );
		}
		else if( toLowercase( sFileExtension ) == "ini" )
		{
			if( !VistaIniFileParser::ReadProplistFromFile( sSimConfigFilePath, oSimulationProperties ) )
			{
				VA_INFO( "RoomAcousticsRenderer", "Using default Master Simulation Scheduler Config." );
			}
			else
			{
				oMasterSimulationControllerConfig.Load( oSimulationProperties );
			}
		}
		else
			VA_EXCEPT_NOT_IMPLEMENTED;
	}

	m_pMasterSimulationController = std::make_unique<ITA::SimulationScheduler::RoomAcoustics::CMasterSimulationController>( oMasterSimulationControllerConfig );

	m_pMasterSimulationController->AttachResultHandler( this );

	// First create the MasterSimulationController before passing it to the SourceReceiverPair Factory
	IVAPoolObjectFactory* pFactory = (IVAPoolObjectFactory*)new SourceReceiverPairFactory( m_oConf, m_pMasterSimulationController.get( ) );
	InitSourceReceiverPairPool( pFactory );
}

CVARoomAcousticsRenderer::~CVARoomAcousticsRenderer( )
{
	// we detach the result handler so that we do not get results in this thread after it ended.
	m_pMasterSimulationController->DetachResultHandler( this );

	m_pMasterSimulationController->PushUpdate(
	    std::make_unique<ITA::SimulationScheduler::CUpdateConfig>( ITA::SimulationScheduler::CUpdateConfig::ConfigChangeType::shutdown ) );

	if( m_oConf.bStoreProfileData )
	{
		VA_INFO( "RoomAcousticsRenderer", "Storing Profile Data" );
		ITA::SimulationScheduler::StoreProfilerData( "SimulationProfile.json" );
		VA_INFO( "RoomAcousticsRenderer", "Finished Storing Profile Data" );
	}
}

void CVARoomAcousticsRenderer::PostResultReceived( std::unique_ptr<ITA::SimulationScheduler::CSimulationResult> pResult )
{
	if( const auto pRIRResult = dynamic_cast<ITA::SimulationScheduler::RoomAcoustics::CRIRSimulationResult*>( pResult.get( ) ) )
	{
		if( pRIRResult->bZerosStripped )
		{
			VA_ERROR( "RoomAcousticsRenderer", "Received simulation result with zeros striped. This is not supported yet. Skipping!" )
			return;
		}

		const auto psfResult = pRIRResult->sfResult;
		int iLeadingZeros    = pRIRResult->iLeadingZeros;

		if( iLeadingZeros < 0 )
		{
			for( int channel = 0; channel < psfResult.GetNumChannels( ); ++channel )
			{
				for( int sample = 0; sample < psfResult.GetLength( ); ++sample )
				{
					if( psfResult[channel][sample] != 0.0f )
					{
						iLeadingZeros = sample;
						break;
					}
				}
			}
		}

		if( const auto pSourceReceiverPair = dynamic_cast<CVARoomAcousticsRenderer::SourceReceiverPair*>(
		        GetSourceReceiverPair( pResult->sourceReceiverPair.source->GetId( ), pResult->sourceReceiverPair.receiver->GetId( ) ) ) )
		{
			if( pRIRResult->eResultType == ITA::SimulationScheduler::RoomAcoustics::FieldOfDuty::directSound )
			{
				pSourceReceiverPair->SetCurrentLeadingZeros( iLeadingZeros );

				pSourceReceiverPair->UpdateDirectSound( iLeadingZeros, psfResult );
			}

			if( pRIRResult->eResultType == ITA::SimulationScheduler::RoomAcoustics::FieldOfDuty::earlyReflections )
			{
				pSourceReceiverPair->UpdateEarlyReflections( iLeadingZeros, psfResult );
			}

			if( pRIRResult->eResultType == ITA::SimulationScheduler::RoomAcoustics::FieldOfDuty::diffuseDecay )
			{
				pSourceReceiverPair->UpdateDiffuseDecay( iLeadingZeros, psfResult );
			}
		}
	}
}

CVAStruct CVARoomAcousticsRenderer::GetParameters( const CVAStruct& oArgs ) const
{
	CVAStruct oReturn;

	for( auto&& pSRP: GetSourceReceiverPairs( ) )
	{
		if( const auto pSourceReceiverPair = dynamic_cast<const SourceReceiverPair*>( pSRP ) )
		{
			const auto iSourceID   = pSourceReceiverPair->GetSource( )->pData->iID;
			const auto iReceiverID = pSourceReceiverPair->GetReceiver( )->pData->iID;
			const auto& oInfo      = pSourceReceiverPair->GetSimulationUpdateInfo( );

			oReturn["SRP_" + std::to_string( iSourceID ) + "_" + std::to_string( iReceiverID ) + "_DS"] = std::to_string( oInfo.receivedUpdateDS );
			oReturn["SRP_" + std::to_string( iSourceID ) + "_" + std::to_string( iReceiverID ) + "_ER"] = std::to_string( oInfo.receivedUpdateER );
			oReturn["SRP_" + std::to_string( iSourceID ) + "_" + std::to_string( iReceiverID ) + "_DD"] = std::to_string( oInfo.receivedUpdateDD );
		}
	}

	return oReturn;
}

void CVARoomAcousticsRenderer::SetParameters( const CVAStruct& oParams )
{
	CVAFIRRendererBase::SetParameters( oParams );

	if( oParams.HasKey( "export" ) )
	{
		const auto sRequestedRIRParts = oParams.GetValue( "export" )->ToString( );

		for( auto&& pSRP: GetSourceReceiverPairs( ) )
		{
			if( const auto pSourceReceiverPair = dynamic_cast<const SourceReceiverPair*>( pSRP ) )
			{
				const auto iSourceID   = pSourceReceiverPair->GetSource( )->pData->iID;
				const auto iReceiverID = pSourceReceiverPair->GetReceiver( )->pData->iID;

				std::string sFileName = "SRP_" + std::to_string( iSourceID ) + "_" + std::to_string( iReceiverID );
				if( toUppercase( sRequestedRIRParts ) == "COMBINED" )
				{
					sFileName += "_Combined.wav";
					pSourceReceiverPair->ExportRIR( sFileName, true, true, true );
				}
				if( toUppercase( sRequestedRIRParts ) == "DS" )
				{
					sFileName += "_DS.wav";
					pSourceReceiverPair->ExportRIR( sFileName, true, false, false );
				}
				if( toUppercase( sRequestedRIRParts ) == "ER" )
				{
					sFileName += "_ER.wav";
					pSourceReceiverPair->ExportRIR( sFileName, false, true, false );
				}
				if( toUppercase( sRequestedRIRParts ) == "DD" )
				{
					sFileName += "_DD.wav";
					pSourceReceiverPair->ExportRIR( sFileName, false, false, true );
				}
			}
		}
	}
}

void CVARoomAcousticsRenderer::Reset( )
{
	CVAFIRRendererBase::Reset( );

	m_pMasterSimulationController->PushUpdate(
	    std::make_unique<ITA::SimulationScheduler::CUpdateConfig>( ITA::SimulationScheduler::CUpdateConfig::ConfigChangeType::resetAll ) );
}

CVARoomAcousticsRenderer::SourceReceiverPair::SourceReceiverPair( const Config& oConf, ITA::SimulationScheduler::RoomAcoustics::CMasterSimulationController* pScheduler )
    : CVAFIRRendererBase::SourceReceiverPair( oConf )
    , m_oConf( oConf )
    , m_pScheduler( pScheduler )
{
	m_sfRIRDirectSoundPart.init( m_oConf.iNumChannels, m_oConf.iMaxFilterLengthSamples, true );
	m_sfRIREarlyReflectionsPart.init( m_oConf.iNumChannels, m_oConf.iMaxFilterLengthSamples, true );
	m_sfRIRDiffuseDecayPart.init( m_oConf.iNumChannels, m_oConf.iMaxFilterLengthSamples, true );
}

void CVARoomAcousticsRenderer::SourceReceiverPair::UpdateDirectSound( const int iLeadingZeroOffset, const ITASampleFrame& pFrame )
{
	m_oSimulationUpdateInfo.receivedUpdateDS++;
	const std::lock_guard lock( m_DirectSoundMutex );
	UpdateFilterPart( iLeadingZeroOffset, pFrame, m_sfRIRDirectSoundPart );
}

void CVARoomAcousticsRenderer::SourceReceiverPair::UpdateEarlyReflections( const int iLeadingZeroOffset, const ITASampleFrame& pFrame )
{
	m_oSimulationUpdateInfo.receivedUpdateER++;
	const std::lock_guard lock( m_EarlyReflectionMutex );
	UpdateFilterPart( iLeadingZeroOffset, pFrame, m_sfRIREarlyReflectionsPart );
}

void CVARoomAcousticsRenderer::SourceReceiverPair::UpdateDiffuseDecay( const int iLeadingZeroOffset, const ITASampleFrame& pFrame )
{
	m_oSimulationUpdateInfo.receivedUpdateDD++;
	const std::lock_guard lock( m_DiffuseDecayMutex );
	UpdateFilterPart( iLeadingZeroOffset, pFrame, m_sfRIRDiffuseDecayPart );
}

void CVARoomAcousticsRenderer::SourceReceiverPair::SetCurrentLeadingZeros( int iZeros )
{
	m_iCurrentLeadingZeros = iZeros;
}

void CVARoomAcousticsRenderer::SourceReceiverPair::ExportRIR( const std::string& sFileName, const bool bDS, const bool bER, const bool bDD ) const
{
	ITASampleFrame sfExport( m_sfRIRDirectSoundPart.GetNumChannels( ), m_sfRIRDirectSoundPart.GetLength( ), true );

	if( bDS )
	{
		const std::lock_guard lock( m_DirectSoundMutex );
		sfExport.add_frame( m_sfRIRDirectSoundPart );
	}

	if( bER )
	{
		const std::lock_guard lock( m_EarlyReflectionMutex );
		sfExport.add_frame( m_sfRIREarlyReflectionsPart );
	}

	if( bDD )
	{
		const std::lock_guard lock( m_DiffuseDecayMutex );
		sfExport.add_frame( m_sfRIRDiffuseDecayPart );
	}

	writeAudiofile( sFileName, &sfExport, m_oConf.dSampleRate, ITAQuantization::ITA_FLOAT );
}

const CVARoomAcousticsRenderer::SourceReceiverPair::SSimulationUpdateInfo& CVARoomAcousticsRenderer::SourceReceiverPair::GetSimulationUpdateInfo( ) const
{
	return m_oSimulationUpdateInfo;
}

void CVARoomAcousticsRenderer::SourceReceiverPair::UpdateFilterPart( int iLeadingZeroOffset, const ITASampleFrame& pSourceFrame, ITASampleFrame& pTargetFrame ) const
{
	const int iFilterPartLength   = pSourceFrame.GetLength( );
	const int iTargetFilterLength = pTargetFrame.GetLength( );

	const int iSourceOffset = m_iCurrentLeadingZeros;
	const int iCount        = std::min( iTargetFilterLength, iFilterPartLength - iSourceOffset );

	pTargetFrame.zero( );
	pTargetFrame.write( pSourceFrame, iCount, iSourceOffset, 0 );
}


void CVARoomAcousticsRenderer::SourceReceiverPair::PrepareDelayAndFilters( double dTimeStamp, const AuralizationMode& oAuraMode, double& dPropagationDelay,
                                                                           ITASampleFrame& sfFIRFilters, bool& bInaudible )
{
	sfFIRFilters.zero( );

	// TODO
	bInaudible = false;

	// Set propagation delay
	dPropagationDelay = m_iCurrentLeadingZeros / m_oConf.dSampleRate;

	// Assemble new RIR
	if( oAuraMode.bDirectSound )
	{
		assert( m_sfRIRDirectSoundPart.GetLength( ) <= sfFIRFilters.GetLength( ) );
		assert( m_sfRIRDirectSoundPart.GetNumChannels( ) == sfFIRFilters.GetNumChannels( ) );

		const std::lock_guard lock( m_DirectSoundMutex );
		for( auto i = 0; i < m_sfRIRDirectSoundPart.GetNumChannels( ); ++i )
		{
			sfFIRFilters[i].add_buf( m_sfRIRDirectSoundPart[i], m_sfRIRDirectSoundPart.GetLength( ) );
		}
	}

	if( oAuraMode.bEarlyReflections )
	{
		assert( m_sfRIREarlyReflectionsPart.GetLength( ) <= sfFIRFilters.GetLength( ) );
		assert( m_sfRIREarlyReflectionsPart.GetNumChannels( ) == sfFIRFilters.GetNumChannels( ) );

		const std::lock_guard lock( m_EarlyReflectionMutex );
		for( auto i = 0; i < m_sfRIREarlyReflectionsPart.GetNumChannels( ); ++i )
		{
			sfFIRFilters[i].add_buf( m_sfRIREarlyReflectionsPart[i], m_sfRIREarlyReflectionsPart.GetLength( ) );
		}
	}

	if( oAuraMode.bDiffuseDecay )
	{
		assert( m_sfRIRDiffuseDecayPart.GetLength( ) <= sfFIRFilters.GetLength( ) );
		assert( m_sfRIRDiffuseDecayPart.GetNumChannels( ) == sfFIRFilters.GetNumChannels( ) );

		const std::lock_guard lock( m_DiffuseDecayMutex );
		for( auto i = 0; i < m_sfRIRDiffuseDecayPart.GetNumChannels( ); ++i )
		{
			sfFIRFilters[i].add_buf( m_sfRIRDiffuseDecayPart[i], m_sfRIRDiffuseDecayPart.GetLength( ) );
		}
	}
}

void CVARoomAcousticsRenderer::SourceReceiverPair::RunSimulation( double dTimeStamp )
{
	auto update = std::make_unique<ITA::SimulationScheduler::CUpdateScene>( dTimeStamp );

	auto sourcePositionAndOrientation = ConvertToVistaPositionAndOrientation( pSource->PredictedPosition( ), pSource->PredictedUpVec( ), pSource->PredictedViewVec( ) );

	auto receiverPositionAndOrientation =
	    ConvertToVistaPositionAndOrientation( pReceiver->PredictedPosition( ), pReceiver->PredictedUpVec( ), pReceiver->PredictedViewVec( ) );

	auto source = std::make_unique<ITA::SimulationScheduler::C3DObject>( std::get<0>( sourcePositionAndOrientation ), std::get<1>( sourcePositionAndOrientation ),
	                                                                     ITA::SimulationScheduler::C3DObject::Type::source, pSource->pData->iID );

	auto receiver = std::make_unique<ITA::SimulationScheduler::C3DObject>( std::get<0>( receiverPositionAndOrientation ), std::get<1>( receiverPositionAndOrientation ),
	                                                                       ITA::SimulationScheduler::C3DObject::Type::receiver, pReceiver->pData->iID );

	update->SetSourceReceiverPair( std::move( source ), std::move( receiver ) );

	m_pScheduler->PushUpdate( std::move( update ) );

	m_oSimulationUpdateInfo.issuedUpdate++;
}

#endif