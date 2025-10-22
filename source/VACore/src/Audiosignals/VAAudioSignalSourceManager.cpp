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

#include "VAAudioSignalSourceManager.h"

#include "../VAAudiostreamTracker.h"
#include "../VALog.h"
#include "../core/core.h"
#include "VAAudiofileSignalSource.h"
#include "VADeviceInputSignalSource.h"
#include "VAEngineSignalSource.h"
#include "VAJetEngineSignalSource.h"
#include "VAMachineSignalSource.h"
#include "VANetstreamSignalSource.h"
#include "VASequencerSignalSource.h"
#include "VATextToSpeechSignalSource.h"

#include <ITAException.h>
#include <ITAStringUtils.h>
#include <VAAudioSignalSource.h>
#include <VAException.h>
#include <VistaBase/VistaTimeUtils.h>
#include <algorithm>
#include <cassert>
#include <sstream>


CVAAudioSignalSourceManager::CVAAudioSignalSourceManager( CVACoreImpl* pParentCore, const CVAAudioDriverConfig& oAudioDriverConfig, ITADatasource* pDeviceInputSource )
    : m_pParentCore( pParentCore )
    , m_dSamplerate( oAudioDriverConfig.dSampleRate )
    , m_iBlocklength( oAudioDriverConfig.iBuffersize )
    , m_iDeviceInputs( oAudioDriverConfig.iInputChannels )
    , m_pDeviceInputSource( pDeviceInputSource )
    , m_vsbDeviceInputBuffers( oAudioDriverConfig.iInputChannels )
    , m_evStreamCounterInc( VistaThreadEvent::NON_WAITABLE_EVENT )
    , m_iStreamCounter( 0 )
{
	m_sfSilence.Init( 1, oAudioDriverConfig.iBuffersize, true );

	// Puffer und Signalquellen f�r Audio-Eing�nge anlegen
	for( int i = 0; i < oAudioDriverConfig.iInputChannels; i++ )
	{
		m_vsbDeviceInputBuffers[i].Init( oAudioDriverConfig.iBuffersize, false );
		std::stringstream ss;
		ss << "Audio device input " << ( i + 1 );
		IVAAudioSignalSource* pSource = new CVADeviceInputSignalSource( &m_vsbDeviceInputBuffers[i], ss.str( ) );
		m_vsDeviceInputSourceIDs.push_back( RegisterSignalSource( pSource, ss.str( ), true, false ) );
	}
}

CVAAudioSignalSourceManager::~CVAAudioSignalSourceManager( )
{
	Reset( );
}

void CVAAudioSignalSourceManager::Reset( )
{
	m_csSignalSourceAccess.enter( );

	std::vector<IVAAudioSignalSource*> vpDelayedRemoveList;

	// Alle gemanageten Quellen freigeben
	for( SignalSourceMapIt it = m_mSignalSources.begin( ); it != m_mSignalSources.end( ); )
	{
		// Forced reference count reset - OK here
		it->second.iRefCount = 0;

		// Remove only those who are managed by the manager and can be removed
		// during runtime (dynamic signal sources)
		if( it->second.bManaged && it->second.bDynamic )
		{
			// More signal source to delete request list
			m_qpDelReqSources.push( &( it->second ) );
			it->second.pSource->HandleUnregistration( m_pParentCore );

			vpDelayedRemoveList.push_back( it->second.pSource );

			// Only remove from record list, save erase later
			it = m_mSignalSources.erase( it );
		}
		else
		{
			++it;
		}
	}

	// Warten bis alle L�schauftr�ge abgearbeitet wurden
	bool bFirstWaitTimeout = true;
	while( !m_qpDelReqSources.empty( ) )
	{
		SyncSignalSources( );

		// Falls kein Streaming l�uft. Direkt die Queue leeren.
		if( !m_pParentCore->IsStreaming( ) )
			m_qpDelReqSources.clear( );

		if( bFirstWaitTimeout == false )
			VA_WARN( "AudioSignalSourceManager", "Waiting for sources to be deleted by FetchInput call from audio hardware" );

		// Schleife etwas ausbremsen und erneut versuchen
		VistaTimeUtils::Sleep( 500 );
		bFirstWaitTimeout = false;
	}

	// Now delete signal source
	for( size_t i = 0; i < vpDelayedRemoveList.size( ); i++ )
		delete vpDelayedRemoveList[i];

	m_mMnemonicCount.clear( );

	m_csSignalSourceAccess.leave( );
}

std::string CVAAudioSignalSourceManager::RegisterSignalSource( IVAAudioSignalSource* pSource, const std::string& sName, bool bManaged, bool bDynamic )
{
	// Quelle darf nicht bei einem anderen Kernel registriert sein
	if( ( pSource->GetAssociatedCore( ) != nullptr ) && ( pSource->GetAssociatedCore( ) != m_pParentCore ) )
		VA_EXCEPT2( INVALID_PARAMETER, "Audio signal already registered by another core instance" );

	m_csSignalSourceAccess.enter( );

	// Fehler, falls die Quelle bereits enthalten ist
	if( FindSignalSource( pSource ) != m_mSignalSources.end( ) )
	{
		m_csSignalSourceAccess.leave( );
		VA_EXCEPT2( INVALID_PARAMETER, "Audio signal source already contained" );
	}

	// Neue ID (Mnemonic + Zahl) generieren
	// TODO:
	std::string sMnemonic = toLowercase( pSource->GetTypeString( ) );
	// std::string sMnemonic = "ss";
	int iNumber;
	std::map<std::string, int>::iterator it = m_mMnemonicCount.find( sMnemonic );
	if( it == m_mMnemonicCount.end( ) )
	{
		// Es wurde noch keine Quelle mit diesem Mnemonic hinzugef�gt -> Z�hler initialisieren
		iNumber = 1;
		m_mMnemonicCount.insert( std::pair<std::string, int>( sMnemonic, iNumber ) );
	}
	else
	{
		// Es wurden schon Quellen mit diesem Mnemonic hinzugef�gt -> Z�hler inkrementieren
		iNumber = ++( it->second );
	}
	std::string sID = sMnemonic + IntToString( iNumber );

	// Quelle hinzuf�gen
	SignalSourceMapIt jt =
	    m_mSignalSources.insert( m_mSignalSources.begin( ), SignalSourceItem( sID, CAudioSignalSource( pSource, sName, m_iBlocklength, bManaged, bDynamic ) ) );

	m_csSignalSourceAccess.leave( );

	// Quelle dem Audio-Streaming �bergeben
	m_qpNewSources.push( &( jt->second ) );

	// Quelle �ber Registrierung informieren
	pSource->HandleRegistration( m_pParentCore );

	return sID;
}

void CVAAudioSignalSourceManager::UnregisterSignalSource( const std::string& sID )
{
	m_csSignalSourceAccess.enter( );

	SignalSourceMapIt it = FindSignalSource( sID );

	if( it == m_mSignalSources.end( ) )
	{
		m_csSignalSourceAccess.leave( );
		VA_EXCEPT2( INVALID_PARAMETER, "Unregister failed, audio signal source " + sID + "not found" );
	}

	if( !it->second.bDynamic )
	{
		m_csSignalSourceAccess.leave( );
		VA_EXCEPT2( MODAL_ERROR, "Unregister failed, audio signal source is static and can't be removed" );
	}

	// Wird die Quelle vom Manager verwaltet, darf man sie nicht entfernen.
	// In diesem Fall muss sie gel�scht werden
	if( it->second.bManaged )
	{
		m_csSignalSourceAccess.leave( );
		VA_EXCEPT2( MODAL_ERROR, "Unregister failed, audio signal source is managed and can't be removed" );
	}

	// Entfernen nur erlaubt wenn keine Referenzen auf die Quelle mehr vorliegen
	if( it->second.iRefCount > 0 )
	{
		m_csSignalSourceAccess.leave( );
		VA_EXCEPT2( MODAL_ERROR, "Unregister failed, audio signal source has references and can't be removed right now" );
	}

	// Audio-Stream das L�schen der Quelle melden
	m_qpDelReqSources.push( &( it->second ) );

	// Warten bis der L�schauftrag abgearbeitet wurden
	while( !m_qpDelReqSources.empty( ) )
		SyncSignalSources( );

	// Quelle �ber deregistrierung informieren
	it->second.pSource->HandleUnregistration( m_pParentCore );

	// TODO: Aufr�umer muss noch sauber implementiert werden!!!
	m_mSignalSources.erase( it );

	m_csSignalSourceAccess.leave( );

	VA_VERBOSE( "UnregisterSignalSource", "Signal source " << sID << " unregistered" );
}

std::string CVAAudioSignalSourceManager::GetAudioDeviceInputSignalSource( int iInput ) const
{
	// Parameter pr�fen
	if( ( iInput < 0 ) || ( iInput > m_iDeviceInputs ) )
		VA_EXCEPT2( INVALID_PARAMETER, "Audio device input index out of range" );

	return m_vsDeviceInputSourceIDs[iInput];
}

std::string CVAAudioSignalSourceManager::CreateAudiofileSignalSource( const std::string& sFilename, const std::string& sName )
{
	// File signals are always dynamic and managed
	CVAAudiofileSignalSource* pSource = new CVAAudiofileSignalSource( sFilename, m_dSamplerate, m_iBlocklength );
	return RegisterSignalSource( pSource, sName, true, true );
}

std::string CVAAudioSignalSourceManager::CreateTextToSpeechSignalSource( const std::string& sName )
{
	// TTS signals are always dynamic and managed
	CVATextToSpeechSignalSource* pSource = new CVATextToSpeechSignalSource( m_dSamplerate, m_iBlocklength );
	return RegisterSignalSource( pSource, sName, true, true );
}

std::string CVAAudioSignalSourceManager::CreateSequencerSignalSource( const std::string& sName )
{
#ifdef VACORE_WITH_SAMPLER_SUPPORT
	// Sampler-Quellen sind immer managed und dynamisch
	CVASequencerSignalSource* pSource = new CVASequencerSignalSource( m_dSamplerate, m_iBlocklength );
	return RegisterSignalSource( pSource, sName, true, true );
#else
	VA_EXCEPT2( INVALID_PARAMETER, "Sequecer signal source was not activated for this core library" );
#endif
}

std::string CVAAudioSignalSourceManager::CreateNetstreamSignalSource( const std::string& sBindAddress, int iRecvPort, const std::string& sName )
{
	// Netstream audio sources are managed
	CVANetstreamSignalSource* pSource = new CVANetstreamSignalSource( m_dSamplerate, m_iBlocklength, sBindAddress, iRecvPort );
	return RegisterSignalSource( pSource, sName, true, true );
}

std::string CVAAudioSignalSourceManager::CreateEngineSignalSource( const std::string& sName )
{
	// Engine-quellen sind immer managed und dynamisch
	CVAEngineSignalSource::Config oESSConfig;
	oESSConfig.pCore = m_pParentCore;
	oESSConfig.lFreqModesSpectrum.insert( std::pair<double, double>( 153 / 13.4f, 0.1 * std::pow( 10, -27 / 10 ) ) );
	oESSConfig.lFreqModesSpectrum.insert( std::pair<double, double>( 277 / 13.4f, 0.1 * std::pow( 10, -32 / 10 ) ) );
	oESSConfig.lFreqModesSpectrum.insert( std::pair<double, double>( 301 / 13.4f, 0.1 * std::pow( 10, -23 / 10 ) ) );
	oESSConfig.lFreqModesSpectrum.insert( std::pair<double, double>( 458 / 13.4f, 0.1 * std::pow( 10, -24 / 10 ) ) );
	oESSConfig.lFreqModesSpectrum.insert( std::pair<double, double>( 584 / 13.4f, 0.1 * std::pow( 10, -4 / 10 ) ) );
	oESSConfig.lFreqModesSpectrum.insert( std::pair<double, double>( 922 / 13.4f, 0.1 * std::pow( 10, -29 / 10 ) ) );
	oESSConfig.lFreqModesSpectrum.insert( std::pair<double, double>( 1152 / 13.4f, 0.1 * std::pow( 10, -30 / 10 ) ) );
	oESSConfig.lFreqModesSpectrum.insert( std::pair<double, double>( 1552 / 13.4f, 0.1 * std::pow( 10, -14 / 10 ) ) );
	oESSConfig.lFreqModesSpectrum.insert( std::pair<double, double>( 2382 / 13.4f, 0.1 * std::pow( 10, -27 / 10 ) ) );
	oESSConfig.lFreqModesSpectrum.insert( std::pair<double, double>( 2883 / 13.4f, 0.1 * std::pow( 10, -18 / 10 ) ) );
	oESSConfig.lFreqModesSpectrum.insert( std::pair<double, double>( 3472 / 13.4f, 0.1 * std::pow( 10, -16 / 10 ) ) );
	CVAEngineSignalSource* pSource = new CVAEngineSignalSource( oESSConfig );
	return RegisterSignalSource( pSource, sName, true, true );
}

std::string CVAAudioSignalSourceManager::CreateMachineSignalSource( const std::string& sName )
{
	CVAMachineSignalSource::Config oMSSConfig( m_pParentCore );
	CVAMachineSignalSource* pSource = new CVAMachineSignalSource( oMSSConfig );

	return RegisterSignalSource( pSource, sName, true, true );
}

std::string CVAAudioSignalSourceManager::CreateJetEngineSignalSource( const std::string& sName, const CVAStruct& oParams )
{
	CVAJetEngineSignalSource::Config oMSSConfig;
	oMSSConfig.pCore = m_pParentCore;
	auto pSource     = new CVAJetEngineSignalSource( oMSSConfig );

	return RegisterSignalSource( pSource, sName, true, true );
}

void CVAAudioSignalSourceManager::DeleteSignalSource( const std::string& sID )
{
	m_csSignalSourceAccess.enter( );

	SignalSourceMapIt it = FindSignalSource( sID );

	if( it == m_mSignalSources.end( ) )
	{
		m_csSignalSourceAccess.leave( );
		VA_EXCEPT2( INVALID_PARAMETER, "Deletion failed, audio signal source " + sID + " not found" );
	}

	if( !it->second.bDynamic )
	{
		m_csSignalSourceAccess.leave( );
		VA_EXCEPT2( MODAL_ERROR, "Deletion failed, audio signal source is static and may not be deleted" );
	}

	// Wird die Quelle nicht vom Manager verwaltet, darf man sie nicht gel�scht werden,
	// da die Freigabe der Quelle in anderer Hand liegt. Solche Quellen d�rfen nur entfernt werden.
	if( !it->second.bManaged )
	{
		m_csSignalSourceAccess.leave( );
		VA_EXCEPT2( MODAL_ERROR, "Deletion failed, audio signal source is not managed and may not be deleted" );
	}

	// L�schen nur erlaubt wenn keine Referenzen auf die Quelle mehr vorliegen
	if( it->second.iRefCount > 0 )
	{
		m_csSignalSourceAccess.leave( );
		VA_EXCEPT2( MODAL_ERROR, "Deletion failed, audio signal source has references and may not be deleted" );
	}

	// Audio-Stream das L�schen der Quelle melden
	m_qpDelReqSources.push( &( it->second ) );

	// Warten bis der L�schauftrag abgearbeitet wurden
	while( !m_qpDelReqSources.empty( ) )
		SyncSignalSources( );

	// Quelle �ber deregistrierung informieren
	it->second.pSource->HandleUnregistration( m_pParentCore );

	// Quelle freigeben
	delete it->second.pSource;
	m_mSignalSources.erase( it );

	m_csSignalSourceAccess.leave( );
}

CVASignalSourceInfo CVAAudioSignalSourceManager::GetSignalSourceInfo( const std::string& sID ) const
{
	m_csSignalSourceAccess.enter( );

	SignalSourceMapCit it = FindSignalSource( sID );

	if( it == m_mSignalSources.end( ) )
	{
		m_csSignalSourceAccess.leave( );
		VA_EXCEPT2( INVALID_PARAMETER, "Could not get information on " + sID + ", audio signal source not found" );
	}

	// Informationen zusammenstellen#
	IVAAudioSignalSource* pSource = it->second.pSource;

	CVASignalSourceInfo oInfo;
	oInfo.sID         = sID;
	oInfo.iType       = pSource->GetType( );
	oInfo.sName       = it->second.sName;
	oInfo.sDesc       = pSource->GetDesc( );
	oInfo.sState      = pSource->GetStateString( );
	oInfo.iReferences = it->second.iRefCount;

	m_csSignalSourceAccess.leave( );

	return oInfo;
}

void CVAAudioSignalSourceManager::GetSignalSourceInfos( std::vector<CVASignalSourceInfo>& vssiDest ) const
{
	m_csSignalSourceAccess.enter( );

	// Alphabetisch sortierte Liste der IDs erzeugen
	std::vector<std::string> vsIDs;
	for( SignalSourceMapCit cit = m_mSignalSources.begin( ); cit != m_mSignalSources.end( ); ++cit )
		vsIDs.push_back( cit->first );
	std::sort( vsIDs.begin( ), vsIDs.end( ) );

	// Informationen zusammenstellen
	vssiDest.clear( );
	for( std::vector<std::string>::const_iterator cit = vsIDs.begin( ); cit != vsIDs.end( ); ++cit )
	{
		SignalSourceMapCit cit2       = m_mSignalSources.find( *cit );
		IVAAudioSignalSource* pSource = cit2->second.pSource;

		CVASignalSourceInfo oInfo;
		oInfo.sID         = *cit;
		oInfo.iType       = pSource->GetType( );
		oInfo.sName       = cit2->second.sName;
		oInfo.sDesc       = pSource->GetDesc( );
		oInfo.sState      = pSource->GetStateString( );
		oInfo.iReferences = cit2->second.iRefCount;

		vssiDest.push_back( oInfo );
	}

	m_csSignalSourceAccess.leave( );
}

std::string CVAAudioSignalSourceManager::GetSignalSourceID( IVAAudioSignalSource* pSource ) const
{
	m_csSignalSourceAccess.enter( );

	for( SignalSourceMapCit cit = m_mSignalSources.begin( ); cit != m_mSignalSources.end( ); ++cit )
		if( cit->second.pSource == pSource )
		{
			m_csSignalSourceAccess.leave( );
			return cit->first;
		}

	m_csSignalSourceAccess.leave( );

	// Methode wird extern benutzt. Exception falls nicht gefunden.
	VA_EXCEPT2( INVALID_PARAMETER, "Invalid signal source ID" );
}

CVAAudioSignalSourceManager::SignalSourceMapIt CVAAudioSignalSourceManager::FindSignalSource( const std::string& sID )
{
	return m_mSignalSources.find( toLowercase( sID ) );
}

CVAAudioSignalSourceManager::SignalSourceMapCit CVAAudioSignalSourceManager::FindSignalSource( const std::string& sID ) const
{
	return m_mSignalSources.find( toLowercase( sID ) );
}


CVAAudioSignalSourceManager::SignalSourceMapIt CVAAudioSignalSourceManager::FindSignalSource( IVAAudioSignalSource* pSource )
{
	for( SignalSourceMapIt it = m_mSignalSources.begin( ); it != m_mSignalSources.end( ); ++it )
		if( it->second.pSource == pSource )
			return it;
	return m_mSignalSources.end( );
}

IVAAudioSignalSource* CVAAudioSignalSourceManager::RequestSignalSource( const std::string& sID, const ITASampleFrame** ppsfInputBuf )
{
	m_csSignalSourceAccess.enter( );
	SignalSourceMapIt it = m_mSignalSources.find( sID );

	// Wird von externen Methoden benutzt. Deshalb Exception.
	if( it == m_mSignalSources.end( ) )
	{
		m_csSignalSourceAccess.leave( );
		VA_EXCEPT2( INVALID_PARAMETER, "Invalid signal source ID" );
	}

	it->second.iRefCount++;
	IVAAudioSignalSource* pRet = it->second.pSource;

	//  Return sample fram pointer if pointer to call-by-reference is valid
	if( ppsfInputBuf )
		*ppsfInputBuf = &( it->second.sfOutputBuffer );

	m_csSignalSourceAccess.leave( );


	return pRet;
}

void CVAAudioSignalSourceManager::ReleaseSignalSource( IVAAudioSignalSource* pSource )
{
	SignalSourceMapIt it = FindSignalSource( pSource );

	// Ung�ltiger Zeiger (weil nicht registriert)
	assert( it != m_mSignalSources.end( ) );

	it->second.iRefCount--;
}

void CVAAudioSignalSourceManager::SyncSignalSources( )
{
	// Diese Methode wartet einen Stream-Zyklus ab und stellt damit sicher
	// das alle �nderung an den Signalquellen im Audio-Stream Kontext abgearbeitet wurden.

	// Falls Streaming nicht im Gange => Nicht warten
	if( !m_pParentCore->IsStreaming( ) )
		return;

	// Keine Ketten => Nicht warten (sonst Deadlock)
	if( m_mSignalSources.empty( ) )
		return;

	// Aktuellen Stream-Counter ermitteln
	int iEntryCount = m_iStreamCounter;

	// Warten bis tats�chlich incrementiert (mittels energiesparendem Event)
#pragma warning( disable : 4127 ) // We allow for a constant expression in this infinite while loop
	while( true )
	{
		// [fwe] Fix: Falls das Streaming nicht gestartet wurde. Direkt aus dem Wait raus.
		//       Dies geschieht wenn die Audio-Hardware im Sack ist und nicht streamt.
		//       In diesem Falle wird hier unendlich gewartet.

		// [jst]: @todo This code block needs an update. The while( true ) loop appears unnecessary.

		if( ( m_iStreamCounter != iEntryCount ) || ( iEntryCount == 0 ) )
			return;

		m_evStreamCounterInc.WaitForEvent( true ); // Block increment finished.

		return;
	}
#pragma warning( default : 4127 )
}

void CVAAudioSignalSourceManager::FetchInputData( const CVAAudiostreamState* pStreamInfo )
{
	CAudioSignalSource* pRecord;

	// Interne Liste aktualisieren
	while( m_qpNewSources.try_pop( pRecord ) )
	{
		m_spIntSources.insert( pRecord );
	}

	while( m_qpDelReqSources.try_pop( pRecord ) )
	{
		m_spIntSources.erase( pRecord );
	}

	// Zun�chst die Samples aller Audio-Eing�nge vom Backend beziehen
	if( m_pDeviceInputSource )
	{
		for( int i = 0; i < m_iDeviceInputs; i++ )
		{
			// [fwe] Dieser Cast ist in Ordnung. VACore-intern wird immer CVAAudioStreamStateImpl weitergeleitet
			const float* pfInputData = m_pDeviceInputSource->GetBlockPointer( i, dynamic_cast<const CVAAudiostreamStateImpl*>( pStreamInfo ) );

			if( pfInputData )
				m_vsbDeviceInputBuffers[i].write( pfInputData, m_iBlocklength );
			else
				m_vsbDeviceInputBuffers[i].Zero( );
		}
		m_pDeviceInputSource->IncrementBlockPointer( );
	}

	// Danach von allen Datenquellen die Daten beziehen
	// Wichtig: Zwei Phasen! 1. GetBlockPointer auf allen Quellen, 2. danach erst IncrementBlockPointer
	for( std::set<CAudioSignalSource*>::iterator it = m_spIntSources.begin( ); it != m_spIntSources.end( ); ++it )
	{
		auto pAudioSignalSource( *it );
		auto vpfOutputData = pAudioSignalSource->pSource->GetStreamBlock( pStreamInfo );

		if( vpfOutputData.size( ) > 0 )
		{
			int iMinChannels = ( std::min )( int( vpfOutputData.size( ) ), ( *it )->sfOutputBuffer.GetNumChannels( ) );
			for( int i = 0; i < iMinChannels; i++ )
				pAudioSignalSource->sfOutputBuffer[i].write( vpfOutputData[i], m_iBlocklength );
		}
		else
		{
			pAudioSignalSource->sfOutputBuffer.zero( );
		}
	}

	++m_iStreamCounter;
	m_evStreamCounterInc.SignalEvent( );
}

const ITASampleFrame* CVAAudioSignalSourceManager::GetSilenceBuffer( ) const
{
	return &m_sfSilence;
}
