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

const CVAAudioSignalSourceManager* CVACoreImpl::GetSignalSourceManager( ) const
{
	return m_pSignalSourceManager;
}

std::string CVACoreImpl::CreateSignalSourceBufferFromParameters( const CVAStruct& oParams, const std::string& sName )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		if( oParams.HasKey( "filepath" ) )
		{
			const std::string sFilePath     = oParams["filepath"];
			const std::string sDestFilePath = FindFilePath( sFilePath );
			if( sDestFilePath.empty( ) )
			{
				std::string sErrorMessage = "Looked everywhere, but could not find file '" + sFilePath + "'";
				VA_ERROR( "core", sErrorMessage );
				VA_EXCEPT2( INVALID_PARAMETER, sErrorMessage );
			}

			std::string sID = m_pSignalSourceManager->CreateAudiofileSignalSource( sDestFilePath, sName );
			assert( !sID.empty( ) );

			CVAEvent ev;
			ev.iEventType = CVAEvent::SIGNALSOURCE_CREATED;
			ev.pSender    = this;
			ev.sObjectID  = sID;
			m_pEventManager->BroadcastEvent( ev );

			VA_INFO( "Core", "Created signal source buffer from audiofile (ID=" << sID << ", Name=\"" << sName << "\", Filename=\"" << sDestFilePath << "\")" );

			return sID;
		}
		else
		{
			auto sErrorMessage = "Could not interpret parameter arguments to create a buffer signal source";
			VA_ERROR( "core", sErrorMessage );
			VA_EXCEPT2( INVALID_PARAMETER, sErrorMessage );
		}
	}
	VA_RETHROW;
}

std::string CVACoreImpl::CreateSignalSourceTextToSpeech( const std::string& sName /*= "" */ )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		std::string sID = m_pSignalSourceManager->CreateTextToSpeechSignalSource( sName );
		assert( !sID.empty( ) );

		CVAEvent ev;
		ev.iEventType = CVAEvent::SIGNALSOURCE_CREATED;
		ev.pSender    = this;
		ev.sObjectID  = sID;
		m_pEventManager->BroadcastEvent( ev );

		VA_INFO( "Core", "Created text-to-speech signal source ( ID=" << sID << ", Name=\"" << sName << "\" )" );

		return sID;
	}
	VA_RETHROW;
}

std::string CVACoreImpl::CreateSignalSourceSequencer( const std::string& sName )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		std::string sID = m_pSignalSourceManager->CreateSequencerSignalSource( sName );

		// Ereignis generieren, wenn Operation erfolgreich
		CVAEvent ev;
		ev.iEventType = CVAEvent::SIGNALSOURCE_CREATED;
		ev.pSender    = this;
		ev.sObjectID  = sID;
		m_pEventManager->BroadcastEvent( ev );

		VA_INFO( "Core", "Created sequencer signal source (ID=" << sID << ", Name=\"" << sName << "\")" );

		return sID;
	}
	VA_RETHROW;
}

std::string CVACoreImpl::CreateSignalSourceNetworkStream( const std::string& sInterface, const int iPort, const std::string& sName )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		VA_TRACE( "Core", "Attempting to connect to a netstream signal source on " << sInterface << " with port " << iPort );
		std::string sID = m_pSignalSourceManager->CreateNetstreamSignalSource( sInterface, iPort, sName );
		assert( !sID.empty( ) );

		// Ereignis generieren, wenn Operation erfolgreich
		CVAEvent ev;
		ev.iEventType = CVAEvent::SIGNALSOURCE_CREATED;
		ev.pSender    = this;
		ev.sObjectID  = sID;
		m_pEventManager->BroadcastEvent( ev );

		VA_INFO( "Core", "Created network stream signal source (ID=" << sID << ", Name='" << sName << "', Interface=" << sInterface << ":" << iPort << ")" );

		return sID;
	}
	VA_RETHROW;
}

std::string CVACoreImpl::CreateSignalSourceEngine( const CVAStruct&, const std::string& sName )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		std::string sID = m_pSignalSourceManager->CreateEngineSignalSource( sName );

		// Ereignis generieren, wenn Operation erfolgreich
		CVAEvent ev;
		ev.iEventType = CVAEvent::SIGNALSOURCE_CREATED;
		ev.pSender    = this;
		ev.sObjectID  = sID;
		m_pEventManager->BroadcastEvent( ev );

		VA_INFO( "Core", "Created engine signal source (ID=" << sID << ", Name=\"" << sName << "\")" );

		return sID;
	}
	VA_RETHROW;
}

std::string CVACoreImpl::CreateSignalSourceMachine( const CVAStruct&, const std::string& sName /*="" */ )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		std::string sID = m_pSignalSourceManager->CreateMachineSignalSource( sName );

		CVAEvent ev;
		ev.iEventType = CVAEvent::SIGNALSOURCE_CREATED;
		ev.pSender    = this;
		ev.sObjectID  = sID;
		m_pEventManager->BroadcastEvent( ev );

		VA_INFO( "Core", "Created machine signal source (ID=" << sID << ", Name=\"" << sName << "\")" );

		return sID;
	}
	VA_RETHROW;
}


std::string CVACoreImpl::CreateSignalSourcePrototypeFromParameters( const CVAStruct& oParams, const std::string& sName /*="" */ )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		if( !oParams.HasKey( "class" ) )
		{
			VA_EXCEPT2( INVALID_PARAMETER, "Key 'class' missing, could not create the prototype signal source from given parameters" );
		}
		if( !oParams.GetValue( "class" )->IsString( ) )
		{
			VA_EXCEPT2( INVALID_PARAMETER, "Key 'class' is not a string, could not create the prototype signal source from given parameters" );
		}

		std::string sID;
		std::string sPrototypeClass = oParams["class"];
		if( sPrototypeClass == "jet_engine" )
		{
			sID = m_pSignalSourceManager->CreateJetEngineSignalSource( sName, oParams );
		}
		else if( sPrototypeClass == "car_engine" )
		{
			VA_EXCEPT2( NOT_IMPLEMENTED, "The required prototype class '" + sPrototypeClass + "' is not implemented, yet" );
		}
		else
		{
			VA_EXCEPT2( NOT_IMPLEMENTED, "The required prototype class '" + sPrototypeClass + "' is not recognized by this core version" );
		}

		CVAEvent ev;
		ev.iEventType = CVAEvent::SIGNALSOURCE_CREATED;
		ev.pSender    = this;
		ev.sObjectID  = sID;
		m_pEventManager->BroadcastEvent( ev );

		VA_INFO( "Core", "Created prototype signal source (ID=" << sID << ", Name=\"" << sName << "\")" );

		return sID;
	}
	VA_RETHROW;
}


bool CVACoreImpl::DeleteSignalSource( const std::string& sID )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		m_pSignalSourceManager->DeleteSignalSource( sID );

		// Ereignis generieren, wenn Operation erfolgreich
		CVAEvent ev;
		ev.iEventType = CVAEvent::SIGNALSOURCE_DELETED;
		ev.pSender    = this;
		ev.sObjectID  = sID;
		m_pEventManager->BroadcastEvent( ev );

		VA_INFO( "Core", "Deleted signal source " << sID );
		return true;
	}
	VA_RETHROW;
}

std::string CVACoreImpl::RegisterSignalSource( IVAAudioSignalSource* pSignalSource, const std::string& sName )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		std::string sID = m_pSignalSourceManager->RegisterSignalSource( pSignalSource, sName, false, false );
		assert( !sID.empty( ) );

		// Ereignis generieren, wenn Operation erfolgreich
		CVAEvent ev;
		ev.iEventType = CVAEvent::SIGNALSOURCE_REGISTERED;
		ev.pSender    = this;
		ev.sObjectID  = sID;
		m_pEventManager->BroadcastEvent( ev );

		return sID;
	}
	VA_RETHROW;
}

bool CVACoreImpl::UnregisterSignalSource( IVAAudioSignalSource* pSignalSource )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		// ID der Quelle suchen
		std::string sID = m_pSignalSourceManager->GetSignalSourceID( pSignalSource );
		if( sID.empty( ) )
			VA_EXCEPT2( INVALID_ID, "Invalid signal source ID" );

		m_pSignalSourceManager->UnregisterSignalSource( sID );

		// Ereignis generieren, wenn Operation erfolgreich
		CVAEvent ev;
		ev.iEventType = CVAEvent::SIGNALSOURCE_UNREGISTERED;
		ev.pSender    = this;
		ev.sObjectID  = sID;
		m_pEventManager->BroadcastEvent( ev );
	}
	VA_RETHROW;

	return true;
}

CVASignalSourceInfo CVACoreImpl::GetSignalSourceInfo( const std::string& sSignalSourceID ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		CVASignalSourceInfo ssi = m_pSignalSourceManager->GetSignalSourceInfo( sSignalSourceID );
		return ssi;
	}
	VA_RETHROW;
}

void CVACoreImpl::GetSignalSourceInfos( std::vector<CVASignalSourceInfo>& vssiDest ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY { m_pSignalSourceManager->GetSignalSourceInfos( vssiDest ); }
	VA_RETHROW;
}

int CVACoreImpl::GetSignalSourceBufferPlaybackState( const std::string& sSignalSourceID ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		IVAAudioSignalSource* pSource = m_pSignalSourceManager->RequestSignalSource( sSignalSourceID );

		if( pSource->GetType( ) != IVAAudioSignalSource::VA_SS_AUDIOFILE )
		{
			m_pSignalSourceManager->ReleaseSignalSource( pSource );
			VA_EXCEPT2( INVALID_PARAMETER, "Cannot set playback action for this type of signal source" );
		}

		CVAAudiofileSignalSource* pAudiofileSource = dynamic_cast<CVAAudiofileSignalSource*>( pSource );

		int iState = pAudiofileSource->GetPlaybackState( );
		m_pSignalSourceManager->ReleaseSignalSource( pSource );

		return iState;
	}
	VA_RETHROW;
}

CVAStruct CVACoreImpl::GetSignalSourceParameters( const std::string& sSignalSourceID, const CVAStruct& oParams ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		IVAAudioSignalSource* pSource = m_pSignalSourceManager->RequestSignalSource( sSignalSourceID );
		CVAStruct oRet                = pSource->GetParameters( oParams );
		m_pSignalSourceManager->ReleaseSignalSource( pSource );
		return oRet;
	}
	VA_RETHROW;
}

int CVACoreImpl::AddSignalSourceSequencerSample( const std::string& sSignalSourceID, const CVAStruct& oArgs )
{
	VA_EXCEPT_NOT_IMPLEMENTED;
}

void CVACoreImpl::SetSignalSourceParameters( const std::string& sSignalSourceID, const CVAStruct& oParams )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		IVAAudioSignalSource* pSource = m_pSignalSourceManager->RequestSignalSource( sSignalSourceID );
		pSource->SetParameters( oParams );

		CVAEvent ev;
		ev.iEventType = CVAEvent::SIGNALSOURCE_STATE_CHANGED;
		ev.pSender    = this;
		ev.sObjectID  = sSignalSourceID;
		m_pEventManager->BroadcastEvent( ev );

		m_pSignalSourceManager->ReleaseSignalSource( pSource );

		VA_INFO( "Core", "Changed parameters of signal source " << sSignalSourceID );
	}
	VA_RETHROW;
}

void CVACoreImpl::SetSignalSourceBufferPlaybackAction( const std::string& sSignalSourceID, const int iPlaybackAction )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		if( !GetSignalSourceBufferPlaybackActionValid( iPlaybackAction ) )
		{
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid playback action" );
		}

		IVAAudioSignalSource* pSource = m_pSignalSourceManager->RequestSignalSource( sSignalSourceID );

		if( pSource->GetType( ) != IVAAudioSignalSource::VA_SS_AUDIOFILE )
		{
			m_pSignalSourceManager->ReleaseSignalSource( pSource );
			VA_EXCEPT2( INVALID_PARAMETER, "Cannot set playback action for this type of signal source" );
		}

		CVAAudiofileSignalSource* pAudiofileSource = dynamic_cast<CVAAudiofileSignalSource*>( pSource );
		pAudiofileSource->SetPlaybackAction( iPlaybackAction );

		CVAEvent ev;
		ev.iEventType = CVAEvent::SIGNALSOURCE_STATE_CHANGED;
		ev.pSender    = this;
		ev.sObjectID  = sSignalSourceID;
		m_pEventManager->BroadcastEvent( ev );

		m_pSignalSourceManager->ReleaseSignalSource( pSource );

		VA_INFO( "Core", "Set audio file signal source '" << sSignalSourceID << "'"
		                                                  << " playstate action to " << IVAInterface::GetPlaybackActionStr( iPlaybackAction ) );
	}
	VA_RETHROW;
}

void CVACoreImpl::SetSignalSourceBufferPlaybackPosition( const std::string& sSignalSourceID, const double dPlaybackPosition )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		// Parameter überprüfen
		if( dPlaybackPosition < 0 )
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid playback position" );

		// Quelle anfordern (prüft auf gültige ID)
		IVAAudioSignalSource* pSource = m_pSignalSourceManager->RequestSignalSource( sSignalSourceID );

		// Playstate kann man nur bei Dateiquellen setzen
		if( pSource->GetType( ) != IVAAudioSignalSource::VA_SS_AUDIOFILE )
		{
			m_pSignalSourceManager->ReleaseSignalSource( pSource );
			VA_EXCEPT2( INVALID_PARAMETER, "Cannot set playstate for this type of signal source" );
		}

		/* TODO:
		// Sicherstellen das es eine Dateiquelle ist
		if (pSource->GetTypeMnemonic() != "af") {
		m_pSignalSourceMan->ReleaseSignalSource(pSource);
		VA_EXCEPT2(INVALID_PARAMETER, "Invalid auralization mode");
		}
		*/

		// Zustand setzen
		CVAAudiofileSignalSource* pAudiofileSource = dynamic_cast<CVAAudiofileSignalSource*>( pSource );
		try
		{
			pAudiofileSource->SetCursorSeconds( dPlaybackPosition );
		}
		catch( ... )
		{
			m_pSignalSourceManager->ReleaseSignalSource( pSource );
			throw;
		}

		m_pSignalSourceManager->ReleaseSignalSource( pSource );

		VA_INFO( "Core", "Set audiofile signal source " << sSignalSourceID << " playback position to " << dPlaybackPosition );
	}
	VA_RETHROW;
}

void CVACoreImpl::SetSignalSourceBufferLooping( const std::string& sSignalSourceID, const bool bLooping )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		IVAAudioSignalSource* pSource = m_pSignalSourceManager->RequestSignalSource( sSignalSourceID );

		if( pSource->GetType( ) != IVAAudioSignalSource::VA_SS_AUDIOFILE )
		{
			m_pSignalSourceManager->ReleaseSignalSource( pSource );
			VA_EXCEPT2( INVALID_PARAMETER, "Cannot set looping mode for this type of signal source" );
		}

		CVAAudiofileSignalSource* pAudiofileSource = dynamic_cast<CVAAudiofileSignalSource*>( pSource );
		pAudiofileSource->SetIsLooping( bLooping );

		CVAEvent ev;
		ev.iEventType = CVAEvent::SIGNALSOURCE_STATE_CHANGED;
		ev.pSender    = this;
		ev.sObjectID  = sSignalSourceID;
		m_pEventManager->BroadcastEvent( ev );

		m_pSignalSourceManager->ReleaseSignalSource( pSource );

		VA_INFO( "Core", "Changed audiofile signal source '" << sSignalSourceID << "'"
		                                                     << " playstate looping mode to " << ( bLooping ? "looping" : "not looping" ) );
	}
	VA_RETHROW;
}

bool CVACoreImpl::GetSignalSourceBufferLooping( const std::string& sSignalSourceID ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	VA_TRY
	{
		IVAAudioSignalSource* pSource = m_pSignalSourceManager->RequestSignalSource( sSignalSourceID );

		if( pSource->GetType( ) != IVAAudioSignalSource::VA_SS_AUDIOFILE )
		{
			m_pSignalSourceManager->ReleaseSignalSource( pSource );
			VA_EXCEPT2( INVALID_PARAMETER, "Cannot set looping mode for this type of signal source" );
		}

		CVAAudiofileSignalSource* pAudiofileSource = dynamic_cast<CVAAudiofileSignalSource*>( pSource );
		bool bLooping                              = pAudiofileSource->GetIsLooping( );
		m_pSignalSourceManager->ReleaseSignalSource( pSource );

		return bLooping;
	}
	VA_RETHROW;
}

int CVACoreImpl::AddSignalSourceSequencerPlayback( const std::string& sSignalSourceID, const int iSoundID, const int iFlags, const double dTimecode )
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

#ifdef VACORE_WITH_SAMPLER_SUPPORT


	IVAAudioSignalSource* pSource = nullptr;

	VA_TRY
	{
		// Quelle anfordern (prüft auf gültige ID)
		pSource = m_pSignalSourceManager->RequestSignalSource( sSignalSourceID );

		// Playbacks kann man nur bei Sequencer-Quellen modifizieren
		if( pSource->GetType( ) != IVAAudioSignalSource::VA_SS_SEQUENCER )
		{
			m_pSignalSourceManager->ReleaseSignalSource( pSource );
			VA_EXCEPT2( INVALID_PARAMETER, "Playbacks can only be added to sequencer signal sources" );
		}

		/* TODO:
		// Sicherstellen das es eine Sampler ist
		if (pSource->GetTypeMnemonic() != "af") {
		m_pSignalSourceMan->ReleaseSignalSource(pSource);
		VA_EXCEPT2(INVALID_PARAMETER, "Invalid auralization mode");
		}
		*/

		// Zustand setzen
		int iPlaybackID;
		try
		{
			CVASequencerSignalSource* pSamplerSource = dynamic_cast<CVASequencerSignalSource*>( pSource );
			iPlaybackID                              = pSamplerSource->AddSoundPlayback( iSoundID, iFlags, dTimecode );
		}
		catch( ... )
		{
			m_pSignalSourceManager->ReleaseSignalSource( pSource );
			throw;
		}

		// Signalquelle freigeben
		m_pSignalSourceManager->ReleaseSignalSource( pSource );

		VA_INFO( "Core",
		         "Added sound playback (Signal source=" << sSignalSourceID << ", Sound=" << iSoundID << ", Flags=" << iFlags << ", Timecode=" << dTimecode << ")" );

		return iPlaybackID;
	}
	VA_FINALLY
	{
		// Signalquelle freigeben
		if( pSource )
			m_pSignalSourceManager->ReleaseSignalSource( pSource );

		// VAExceptions unverändert nach aussen leiten
		throw;
	}
#else
	VA_EXCEPT2( INVALID_PARAMETER, "Core was not build with sequencer supports" );
#endif // VACORE_WITH_SAMPLER_SUPPORT
}

void CVACoreImpl::RemoveSignalSourceSequencerSample( const std::string& sSignalSourceID, const int iSoundID )
{
	VA_EXCEPT_NOT_IMPLEMENTED;
}
