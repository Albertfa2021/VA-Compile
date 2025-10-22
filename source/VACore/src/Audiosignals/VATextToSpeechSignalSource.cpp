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

#include "VATextToSpeechSignalSource.h"

#include "../VAAudiostreamTracker.h"
#include "../VALog.h"
#include "../core/core.h"

#include <ITAAudiofileReader.h>
#include <ITABufferDataSource.h>
#include <ITAConstants.h>
#include <ITAException.h>
#include <ITASampleFrame.h>
#include <ITASampleTypeConversion.h>
#include <VA.h>
#include <VistaTools/VistaFileSystemDirectory.h>
#include <VistaTools/VistaFileSystemFile.h>
#include <assert.h>
#include <iomanip>
#include <math.h>
#include <sstream>


#ifdef VACORE_WITH_TTS_SIGNAL_SOURCE
#	include <cerevoice_eng.h>
#endif

//#define TTS_USE_WAV

CVATextToSpeechSignalSource::CVATextToSpeechSignalSource( const double dSampleRate, const int iBlockLength )
    : ITADatasourceRealization( 1, dSampleRate, (unsigned int)( iBlockLength ) )
    , m_pAssociatedCore( NULL )
    , m_pBufferDataSource( NULL )
    , m_pFrameToDelete( NULL )
{
	m_sbOut.Init( GetBlocklength( ), true );
	TTSEngine::getInstance( ); // so it is initialized!
	m_iCurrentPlayState = IVAInterface::VA_PLAYBACK_ACTION_STOP;
}


CVATextToSpeechSignalSource::~CVATextToSpeechSignalSource( ) {}

int CVATextToSpeechSignalSource::GetType( ) const
{
	return IVAAudioSignalSource::VA_SS_TEXT_TO_SPEECH;
}

std::string CVATextToSpeechSignalSource::GetTypeString( ) const
{
	return "tts";
}

std::string CVATextToSpeechSignalSource::GetDesc( ) const
{
	return std::string( "Creates spoken text and facial movements from a text" );
}

IVAInterface* CVATextToSpeechSignalSource::GetAssociatedCore( ) const
{
	return m_pAssociatedCore;
}

std::vector<const float*> CVATextToSpeechSignalSource::GetStreamBlock( const CVAAudiostreamState* pStreamInfo )
{
	// This is the live update function that is called by the audio streaming.
	// We will deliver either zeros for a quit talker, or take speech samples from
	// the WAv file or internal sample buffer.

	m_sbOut.Zero( );

	if( m_pBufferDataSource != NULL )
	{
		int iResidualSamples   = m_pBufferDataSource->GetCursor( ) - m_pBufferDataSource->GetCapacity( );
		bool bEndOfFileReached = ( iResidualSamples >= 0 );
		if( bEndOfFileReached && m_iCurrentPlayState == IVAInterface::VA_PLAYBACK_ACTION_PLAY )
		{
			m_iCurrentPlayState = IVAInterface::VA_PLAYBACK_ACTION_STOP;
			if( m_pFrameToDelete != NULL )
			{
				delete m_pFrameToDelete;
				m_pFrameToDelete = NULL;
			}
			// VA_INFO("TextToSpeechSignalSource", "TTS to VA_PLAYBACK_ACTION_STOP");
		}
	}


	if( m_iCurrentPlayState == IVAInterface::VA_PLAYBACK_ACTION_PLAY )
	{
		const float* pfData = m_pBufferDataSource->GetBlockPointer( 0, dynamic_cast<const CVAAudiostreamStateImpl*>( pStreamInfo ) );
		if( pfData )
			m_sbOut.write( pfData, m_pBufferDataSource->GetBlocklength( ) );
		m_pBufferDataSource->IncrementBlockPointer( );
		// if (pAudioFile != NULL)
		//	return pAudioFile->GetStreamBlock(pStreamInfo);
	}


	return { m_sbOut.data( ) };
}

void CVATextToSpeechSignalSource::HandleRegistration( IVAInterface* pCore )
{
	m_pAssociatedCore      = pCore;
	std::string voices_dir = pCore->SubstituteMacros( "$(voices_dir)" );
	if( voices_dir != "$(voices_dir)" )
		TTSEngine::getInstance( ).SetAdditionalVoicePath( voices_dir );
	if( TTSEngine::getInstance( ).getEngine( ) == NULL )
		TTSEngine::getInstance( ).Init( );
}

void CVATextToSpeechSignalSource::HandleUnregistration( IVAInterface* )
{
	m_pAssociatedCore = NULL;
}

std::string CVATextToSpeechSignalSource::GetStateString( ) const
{
	return "undefined";
}

void CVATextToSpeechSignalSource::Reset( )
{
	VA_WARN( "TextToSpeechSignalSource", "Reset is not yet implemented." );
}

CVAStruct CVATextToSpeechSignalSource::GetParameters( const CVAStruct& oArgs ) const
{
	CVAStruct oRet;

#ifndef VACORE_WITH_TTS_SIGNAL_SOURCE
	oRet["error"] = "TTS signal sources not activated in your VACore";
	VA_WARN( "TextToSpeechSignalSource", "TTS signal sources was requested but is not activated in your VACore" );
#else

	/*
	 * get_visemes_for
	 */

	if( oArgs.HasKey( "get_visemes_for" ) && oArgs["get_visemes_for"].IsString( ) )
	{
		std::string id = oArgs["get_visemes_for"];

		auto it = m_Visemes.find( id );
		if( it == m_Visemes.end( ) )
		{
			VA_WARN( "TextToSpeechSignalSource", "Visemes for requested id \"" + id + "\" do not exist." );
			oRet["visemes"] = "";
			return oRet;
		}

		oRet["visemes"] = it->second;

		return oRet;
	}


	/*
	 * list_voices
	 */

	if( oArgs.HasKey( "list_voices" ) && oArgs["list_voices"] )
	{
		// list the available voices
		int num_voices = CPRCEN_engine_get_voice_count( TTSEngine::getInstance( ).getEngine( ) );
		oRet["number"] = num_voices;
		for( int i = 0; i < num_voices; i++ )
		{
			CVAStruct oVoice;
			std::string voicename     = CPRCEN_engine_get_voice_info( TTSEngine::getInstance( ).getEngine( ), i, "VOICE_NAME" );
			oVoice["name"]            = voicename;
			std::string language      = CPRCEN_engine_get_voice_info( TTSEngine::getInstance( ).getEngine( ), i, "LANGUAGE_CODE_ISO" );
			oVoice["language"]        = language;
			std::string country       = CPRCEN_engine_get_voice_info( TTSEngine::getInstance( ).getEngine( ), i, "COUNTRY_CODE_ISO" );
			oVoice["country"]         = country;
			std::string sex           = CPRCEN_engine_get_voice_info( TTSEngine::getInstance( ).getEngine( ), i, "SEX" );
			oVoice["sex"]             = sex;
			oRet[std::to_string( i )] = oVoice;
		}
		return oRet;
	}

	/*
	 * default
	 */

	if( oArgs.IsEmpty( ) )
	{
		oRet["info"] = "see VATextToSpeechSignalSource.h documentation for SetParameters() for usage information";
		VA_INFO( "TextToSpeechSignalSource",
		         "GetParameters called with empty argument, see VATextToSpeechSignalSource.h documentation for SetParameters() for usage information." );
		return oRet;
	}

	oRet["info"] = "Unknown parameters! see VATextToSpeechSignalSource.h documentation for SetParameters() for usage information";
	VA_INFO( "TextToSpeechSignalSource", "Unknonwn parameters: " + oArgs.ToString( ) );
#endif

	return oRet;
}

void CVATextToSpeechSignalSource::SetParameters( const CVAStruct& oParams )
{
#ifndef VACORE_WITH_TTS_SIGNAL_SOURCE
	VA_WARN( "TextToSpeechSignalSource", "TTS signal sources was requested but is not activated in your VACore" );
#else

	/*
	 * prepare_text
	 */

	if( oParams.HasKey( "prepare_text" ) )
	{
		if( !oParams["prepare_text"].IsString( ) )
		{
			VA_WARN( "TextToSpeechSignalSource", "No Text given!" );
			return;
		}
		std::string sText = oParams["prepare_text"];

		std::string sVoice = "Heather";
		if( oParams.HasKey( "voice" ) && oParams["voice"].IsString( ) )
			sVoice = oParams["voice"];

		bool direct_playback = false;
		if( oParams.HasKey( "direct_playback" ) && oParams["direct_playback"].IsBool( ) && oParams["direct_playback"] )
		{
			direct_playback = true;
		}

		std::string id = "tmp";
		if( !oParams.HasKey( "id" ) || !oParams["id"].IsString( ) )
		{
			if( !direct_playback )
			{
				VA_WARN( "TextToSpeechSignalSource", "No id is given for the prepare speech request, the user application has to give an unique id." );
				return;
			}
		}
		else
			id = oParams["id"];

		if( m_Visemes.find( id ) != m_Visemes.end( ) && id.compare( "tmp" ) != 0 )
		{
			VA_WARN( "TextToSpeechSignalSource", "The id \"" + id + "\" was used before, make sure you do not overwrite anything still needed." );
		}


		// Prepare a WAV file or sample buffer with text-to-speech engine output

		// std::string visemes = "<?xml version=\"1.0\" encoding=\"UTF - 8\"?>\n<speak>\n";
		UserCallbackData data;
		data.visemes = "<speech type=\"text/plain\" id=\"" + id + "\">\n";

		data.visemes += "<sync id=\"start\" time=\"0.0\"/>\n"; // This added to avoid warning, when replaying

		CPRCEN_channel_handle chan = CPRCEN_engine_open_channel( TTSEngine::getInstance( ).getEngine( ), "", "", sVoice.c_str( ), "" );
		CPRCEN_engine_channel_reset( TTSEngine::getInstance( ).getEngine( ), chan );
		CPRCEN_engine_clear_callback( TTSEngine::getInstance( ).getEngine( ), chan );
		CPRCEN_engine_set_callback( TTSEngine::getInstance( ).getEngine( ), chan, (void*)&data, VisemeProcessing );
#	ifdef TTS_USE_WAV
		CPRCEN_engine_channel_to_file( TTSEngine::getInstance( ).getEngine( ), chan, "D:/work/tts.wav", CPRCEN_RIFF ); /* File output on channel */
#	endif
		CPRC_abuf* buf = CPRCEN_engine_channel_speak( TTSEngine::getInstance( ).getEngine( ), chan, sText.c_str( ), sText.length( ), true );
		if( buf == NULL )
		{
			VA_WARN( "TextToSpeechSignalSource", "Cannot create an audio file, probably no voice is available!" );
			return;
		}

		data.visemes += "</speech>\n";
		// data.visemes += "<event id=\"" + id + "\" start=\"0\" message=\"speech started\"  />";

		m_Visemes[id] = data.visemes;

#	ifdef TTS_USE_WAV
		ITASampleFrame* pAudioBuffer = new ITASampleFrame( "D:/work/tts.wav" );

		CITAAudioSample* pAudioSample = new CITAAudioSample( );
		// pAudioSample->Load(*pAudioBuffer, 44100.0f);
		pAudioSample->Load( *pAudioBuffer, TTSEngine::getInstance( ).getSampleRate( ) );
		m_AudioSampleFrames[id] = pAudioSample;
#	else
		ITASampleFrame* pAudioBuffer = new ITASampleFrame( );
		pAudioBuffer->init( 1, data.floatBuffer.size( ), false );
		( *pAudioBuffer )[0].write( &data.floatBuffer[0], data.floatBuffer.size( ) );
		pAudioBuffer->mul_scalar( 0.95 ); // this way clipping is avoided

		CITAAudioSample* pAudioSample = new CITAAudioSample( );
		pAudioSample->Load( *pAudioBuffer, TTSEngine::getInstance( ).getSampleRate( ) );
		m_AudioSampleFrames[id] = pAudioSample;
#	endif


		if( direct_playback )
		{
			CVAStruct oParams_play;
			oParams_play["play_speech"] = id;
			oParams_play["free_after"]  = true;
			SetParameters( oParams_play );
		}

		VA_INFO( "TextToSpeechSignalSource", "VA creatted audio, to say \"" + sText + "\", with id: \"" + id + "\"" );
		return;
	}


	/*
	 * play_speech
	 */


	if( oParams.HasKey( "play_speech" ) )
	{
		std::string id = "tmp";
		if( oParams["play_speech"].IsString( ) )
			id = oParams["play_speech"];
		else
			VA_WARN( "TextToSpeechSignalSource", "play_speech does not hold an identificator for the speech to be played, use \"tmp\"" );

		// pAudioFile = new CVAAudiofileSignalSource("D:/work/tts.wav", GetSampleRate(), GetBlocklength());
		// pAudioFile->SetIsLooping(true);
		// pAudioFile->SetPlaybackAction(IVAInterface::VA_PLAYBACK_ACTION_PLAY);


		// ITASampleFrame sfFileBuffer("D:/work/tts.wav");

		/*std::size_t bufferLength = CPRC_abuf_wav_sz(buf);
		std::vector<float> floatBuffer;
		floatBuffer.resize(bufferLength);
		stc_sint16_to_float(&floatBuffer[0], CPRC_abuf_wav_data(buf), bufferLength);
		*/

		auto it = m_AudioSampleFrames.find( id );
		if( it == m_AudioSampleFrames.end( ) || it->second == NULL )
		{
			VA_WARN( "TextToSpeechSignalSource", "There is no audio created for id: \"" + id + "\"" );
			return;
		}

		ITASampleFrame* pAudioBuffer = it->second;


		if( m_pBufferDataSource != NULL )
			delete m_pBufferDataSource;
		m_pBufferDataSource = new ITABufferDatasource( ( *pAudioBuffer )[0].data( ), pAudioBuffer->length( ), GetSampleRate( ), GetBlocklength( ) );
		m_pBufferDataSource->SetPaused( false );
		m_pBufferDataSource->SetLoopMode( false );
		m_pBufferDataSource->Rewind( );

		m_iCurrentPlayState = IVAInterface::VA_PLAYBACK_ACTION_PLAY;

		if( oParams.HasKey( "free_after" ) && oParams["free_after"].IsBool( ) && oParams["free_after"] )
		{
			m_pFrameToDelete        = pAudioBuffer; // so it will be deleted once the sound is fully played
			m_AudioSampleFrames[id] = NULL;
			m_Visemes[id]           = "";
		}

		VA_INFO( "TextToSpeechSignalSource", "Play TTS for id: \"" + id + "\"" );

		return;
	}

	VA_WARN( "TextToSpeechSignalSource", "Could not interpret parameters for text-to-speech signal source setter method, use empty getter for help." );
#endif
}

std::string CVATextToSpeechSignalSource::to_string_with_precision( float a_value, const int n /* = 3*/ )
{
	std::ostringstream out;
	out << std::setprecision( n ) << a_value;
	return out.str( );
}


void CVATextToSpeechSignalSource::VisemeProcessing( CPRC_abuf* abuf, void* userdata )
{
#ifdef VACORE_WITH_TTS_SIGNAL_SOURCE


	// this callback is called per sentence in the text, so we need to append to the other visemes and also time-wise!
	UserCallbackData* data = (UserCallbackData*)userdata;
	float endTime          = 0.0f;

	if( abuf == NULL )
	{
		VA_WARN( "TextToSpeechSignalSource", "The buffer is NULL, cannot extract visemes!" );
		return;
	}

	if( userdata == NULL )
	{
		VA_WARN( "TextToSpeechSignalSource", "The userdata viseme string is NULL, cannot extract visemes!" );
		return;
	}

	/* Transcriptions contain markers, phonetic information, a
	list of these items is available for each audio buffer. */
	const CPRC_abuf_trans* trans;
	std::string label;
	float start, end;
	/* Process the transcription buffer items and print information. */
	for( int i = 0; i < CPRC_abuf_trans_sz( abuf ); i++ )
	{
		trans = CPRC_abuf_get_trans( abuf, i );
		start = CPRC_abuf_trans_start( trans ); /* Start time in seconds */
		end   = CPRC_abuf_trans_end( trans );   /* End time in seconds */
		label = CPRC_abuf_trans_name( trans );  /* Label, type dependent */
		if( CPRC_abuf_trans_type( trans ) == CPRC_ABUF_TRANS_PHONE )
		{
			// VA_INFO("TextToSpeechSignalSource", "Phoneme: " + std::to_string(start) + " " + std::to_string(end) + " " + label);
			// visemes_time->first.append( "\t<viseme start=\"" + to_string_with_precision(start+visemes_time->second) + "\" articulation=\"1\" type=\"" +
			// PhonemeToViseme(label) + "\" />\n");
			std::string viseme = std::string( "\t<lips " ) + "viseme=\"" + TTSEngine::getInstance( ).PhonemeToViseme( label ) + "\" " + "articulation=\"1.0\" " +
			                     "start=\"" + to_string_with_precision( start + data->lastEnd ) + "\" " + "ready=\"" + to_string_with_precision( start + data->lastEnd ) +
			                     "\" " + "relax=\"" + to_string_with_precision( end + data->lastEnd ) + "\" " + "end=\"" +
			                     to_string_with_precision( end + data->lastEnd ) + "\" " + "/>\n";
			data->visemes.append( viseme );
			endTime = end;
		}
		/*else if (CPRC_abuf_trans_type(trans) == CPRC_ABUF_TRANS_WORD) {
		    VA_INFO("TextToSpeechSignalSource", "Word: " + std::to_string(start) + " " + std::to_string(end) + " " + label);
		}
		else if (CPRC_abuf_trans_type(trans) == CPRC_ABUF_TRANS_MARK) {
		    VA_INFO("TextToSpeechSignalSource", "Marker: " + std::to_string(start) + " " + std::to_string(end) + " " + label);
		}*/
		else if( CPRC_abuf_trans_type( trans ) == CPRC_ABUF_TRANS_ERROR )
		{
			VA_INFO( "TextToSpeechSignalSource", "ERROR: could not retrieve transcription at " + std::to_string( i ) );
		}
	}
	data->lastEnd += endTime;

	// we basically append the data in abuf to the data in floatBuffer and on the way convert it from short to float (since that's what we need for the SoundSignal)
	std::size_t bufferLength   = CPRC_abuf_wav_sz( abuf );
	std::size_t floatBufLength = data->floatBuffer.size( );
	data->floatBuffer.resize( bufferLength + data->floatBuffer.size( ) );
	stc_sint16_to_float( &data->floatBuffer[floatBufLength], CPRC_abuf_wav_data( abuf ), bufferLength );

#endif
}

void CVATextToSpeechSignalSource::TTSEngine::Init( )
{
#ifdef VACORE_WITH_TTS_SIGNAL_SOURCE

	if( m_pTTSEngine != NULL )
		CPRCEN_engine_delete( m_pTTSEngine );


	m_pTTSEngine = CPRCEN_engine_new( );

	SetAdditionalVoicePath( CEREVOICE_VOICES_PATH ); // this is defined by the FindVCereVoice cmake script, but cannot be used e.g. for deployed VAServers

	for( std::string voices_path: m_VoicePaths )
	{
		VA_INFO( "TextToSpeechSignalSource", "CereVoice voices are searched in \"" + voices_path + "\"" );

		VistaFileSystemDirectory voicesDir( voices_path );
		if( !voicesDir.Exists( ) )
		{
			VA_WARN( "TextToSpeechSignalSource", "The voices directory does not exist!" );
			continue;
		}

		for( auto it = voicesDir.begin( ); it != voicesDir.end( ); ++it )
		{
			std::string name       = ( *it )->GetName( );
			std::size_t suffix_pos = name.find( ".voice" );
			if( suffix_pos == std::string::npos )
				continue;
			std::string licence_file = name.substr( 0, suffix_pos ) + ".lic";
			VistaFileSystemFile licenseFile( licence_file );
			if( !licenseFile.Exists( ) )
			{
				VA_WARN( "TextToSpeechSignalSource", "The associated license file (" + licence_file + ") does not exist, cannot load voice" );
				continue;
			}

			if( m_loadedVoices.find( ( *it )->GetLocalName( ) ) != m_loadedVoices.end( ) )
				continue; // so we do not load the same voice multiple times if it is in multiple directories in which we search

			CPRCEN_engine_load_voice( m_pTTSEngine, licence_file.c_str( ), NULL, name.c_str( ), CPRC_VOICE_LOAD );

			VA_INFO( "TextToSpeechSignalSource", "Loaded voice \"" + name + "\"" );

			m_loadedVoices.insert( ( *it )->GetLocalName( ) );
		}
	}

	// now check for the sampled voices which sample rate we have
	m_sampleRate   = -1.0;
	int num_voices = CPRCEN_engine_get_voice_count( getEngine( ) );
	if( num_voices == 0 )
	{
		VA_WARN( "TextToSpeechSignalSource", "No voices were loaded!!!!! TTS will not work!!!!!" );
	}
	for( int i = 0; i < num_voices; i++ )
	{
		std::string strSamplerate = CPRCEN_engine_get_voice_info( getEngine( ), i, "SAMPLE_RATE" );
		float rate                = std::stof( strSamplerate );
		if( m_sampleRate < 0.0 )
			m_sampleRate = rate;
		if( rate != m_sampleRate )
			VA_WARN( "TextToSpeechSignalSource",
			         "Voices with different sample rates are used namely " + std::to_string( rate ) + " and " + std::to_string( m_sampleRate ) );
	}

	SetupPhonemeMapping( );

	// std::string licence_file = voices_path + "cerevoice_heather_4.0.0_48k.lic";
	// std::string voice_file = voices_path + "cerevoice_heather_4.0.0_48k.voice";

	// m_pTTSEngine = CPRCEN_engine_load(licence_file.c_str(), voice_file.c_str());
#endif
}


CVATextToSpeechSignalSource::TTSEngine::~TTSEngine( )
{
#ifdef VACORE_WITH_TTS_SIGNAL_SOURCE
	CPRCEN_engine_delete( m_pTTSEngine );
#endif
}

CPRCEN_engine* CVATextToSpeechSignalSource::TTSEngine::getEngine( ) const
{
	return m_pTTSEngine;
}

float CVATextToSpeechSignalSource::TTSEngine::getSampleRate( ) const
{
	return m_sampleRate;
}

std::string CVATextToSpeechSignalSource::TTSEngine::PhonemeToViseme( std::string phoneme )
{
	auto it = m_phonemeToId.find( phoneme );
	if( it == m_phonemeToId.end( ) )
	{
		VA_WARN( "TextToSpeechSignalSource", "There exists no mapping for the phoneme: \"" + phoneme + "\"" );
		return phoneme;
	}

	auto it2 = m_idToViseme.find( it->second );
	if( it2 == m_idToViseme.end( ) )
	{
		VA_WARN( "TextToSpeechSignalSource", "There exists no mapping for viseme id: " + it->second );
		return phoneme;
	}
	return it2->second;
}

void CVATextToSpeechSignalSource::TTSEngine::SetAdditionalVoicePath( std::string _path )
{
	if( _path.empty( ) )
		return;

	for( std::string contained_path: m_VoicePaths )
	{
		if( contained_path == _path )
			return;
	}
	m_VoicePaths.push_back( _path );
}

void CVATextToSpeechSignalSource::TTSEngine::SetupPhonemeMapping( )
{
	if( m_phonemeToId.size( ) > 0 )
		return; // only initialize once

	// this should support all phonemes, for American, Scottish and German speech (see https://www.cereproc.com/files/CereVoicePhoneSets.pdf)

	m_phonemeToId["sil"] = 0;
	m_phonemeToId["@"]   = 6;
	m_phonemeToId["@@"]  = 6;
	m_phonemeToId["a"]   = 3;
	m_phonemeToId["aa"]  = 3;
	m_phonemeToId["ae"]  = 6;
	m_phonemeToId["aeh"] = 6;
	m_phonemeToId["ah"]  = 6;
	m_phonemeToId["ai"]  = 11;
	m_phonemeToId["an"]  = 9;
	m_phonemeToId["ao"]  = 3;
	m_phonemeToId["au"]  = 3;
	m_phonemeToId["aw"]  = 6;
	m_phonemeToId["ax"]  = 6;
	m_phonemeToId["ay"]  = 6;
	m_phonemeToId["b"]   = 21;
	m_phonemeToId["ch"]  = 16;
	m_phonemeToId["d"]   = 19;
	m_phonemeToId["dh"]  = 17;
	m_phonemeToId["dx"]  = 19;
	m_phonemeToId["dzh"] = 16; // not entirely clear
	m_phonemeToId["e"]   = 1;
	m_phonemeToId["e@"]  = 1;
	m_phonemeToId["eh"]  = 6;
	m_phonemeToId["ei"]  = 4;
	m_phonemeToId["en"]  = 4;
	m_phonemeToId["er"]  = 5;
	m_phonemeToId["ey"]  = 6;
	m_phonemeToId["f"]   = 18;
	m_phonemeToId["g"]   = 20;
	m_phonemeToId["h"]   = 12;
	m_phonemeToId["hh"]  = 6;
	m_phonemeToId["i"]   = 6;
	m_phonemeToId["i@"]  = 6;
	m_phonemeToId["ih"]  = 6;
	m_phonemeToId["ii"]  = 6;
	m_phonemeToId["iy"]  = 6;
	m_phonemeToId["j"]   = 6;
	m_phonemeToId["jh"]  = 16;
	m_phonemeToId["k"]   = 20;
	m_phonemeToId["l"]   = 19;
	m_phonemeToId["m"]   = 21;
	m_phonemeToId["n"]   = 20;
	m_phonemeToId["ng"]  = 20;
	m_phonemeToId["o"]   = 8;
	m_phonemeToId["oe"]  = 8;  // actually no real oe viseme, so take o
	m_phonemeToId["oeh"] = 8;  // actually no real oe viseme, so take o
	m_phonemeToId["oen"] = 21; // as in the German Parfum (p_a_rv_f_oen), just took m
	m_phonemeToId["oh"]  = 8;
	m_phonemeToId["oi"]  = 10;
	m_phonemeToId["on"]  = 9;
	m_phonemeToId["oo"]  = 8;
	m_phonemeToId["ou"]  = 8;
	m_phonemeToId["ow"]  = 8;
	m_phonemeToId["oy"]  = 8;
	m_phonemeToId["p"]   = 21;
	m_phonemeToId["pf"]  = 21;
	m_phonemeToId["q"]   = 1; // this is somehow only used in German, before starting as, e.g. in Abend (q_ah_b_@_n_t)
	m_phonemeToId["r"]   = 13;
	m_phonemeToId["rv"]  = 13;
	m_phonemeToId["rl"]  = 5;
	m_phonemeToId["s"]   = 15;
	m_phonemeToId["sh"]  = 16;
	m_phonemeToId["T"]   = 19;
	m_phonemeToId["t"]   = 19;
	m_phonemeToId["th"]  = 17;
	m_phonemeToId["ts"]  = 15;
	m_phonemeToId["tsh"] = 16;
	m_phonemeToId["u"]   = 8;
	m_phonemeToId["u@"]  = 8;
	m_phonemeToId["uh"]  = 8;
	m_phonemeToId["ue"]  = 6; // no real ue
	m_phonemeToId["ueh"] = 6; // no real ue
	m_phonemeToId["uu"]  = 8;
	m_phonemeToId["uw"]  = 8;
	m_phonemeToId["v"]   = 18;
	m_phonemeToId["w"]   = 7;
	m_phonemeToId["x"]   = 20; // actually not really supported, as in Scottish loch (l_o_x)
	m_phonemeToId["y"]   = 7;
	m_phonemeToId["z"]   = 15;
	m_phonemeToId["zh"]  = 16;
	m_phonemeToId["R"]   = 13;


	m_idToViseme[0]  = "_";   /// silence
	m_idToViseme[1]  = "Ah";  /// Viseme for aa, ae, ah
	m_idToViseme[2]  = "Aa";  /// Viseme for aa
	m_idToViseme[3]  = "Ao";  /// ao
	m_idToViseme[4]  = "Eh";  /// ey, eh, uh
	m_idToViseme[5]  = "Er";  /// er
	m_idToViseme[6]  = "Ih";  /// y, iy, ih, ix
	m_idToViseme[7]  = "W";   /// w, uw
	m_idToViseme[8]  = "Ow";  /// ow
	m_idToViseme[9]  = "Aw";  /// aw
	m_idToViseme[10] = "Oy";  /// oy
	m_idToViseme[11] = "Ay";  /// ay
	m_idToViseme[12] = "H";   /// h
	m_idToViseme[13] = "R";   /// r
	m_idToViseme[14] = "L";   /// l
	m_idToViseme[15] = "Z";   /// s, z
	m_idToViseme[16] = "Sh";  /// sh, ch, jh, zh
	m_idToViseme[17] = "Th";  /// th, dh
	m_idToViseme[18] = "F";   /// f, v
	m_idToViseme[19] = "D";   /// d, t, n   - also try NG: 2 to 1 against
	m_idToViseme[20] = "KG";  /// k, g, ,ng   - also try NG: 2 to 1 against
	m_idToViseme[21] = "BMP"; /// p, b, m
}