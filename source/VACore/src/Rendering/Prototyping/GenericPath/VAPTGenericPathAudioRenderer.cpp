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

#include "VAPTGenericPathAudioRenderer.h"

#ifdef VACORE_WITH_RENDERER_PROTOTYPE_GENERIC_PATH

// VA includes
#	include "../../../Scene/VAScene.h"
#	include "../../../Utils/VAUtils.h"
#	include "../../../VACoreConfig.h"
#	include "../../../VALog.h"
#	include "../../../core/core.h"

#	include <VAReferenceableObject.h>

// ITA includes
#	include <ITADataSourceRealization.h>
#	include <ITAFastMath.h>
#	include <ITAUPConvolution.h>
#	include <ITAUPFilter.h>
#	include <ITAUPFilterPool.h>
#	include <ITAVariableDelayLine.h>

// Vista includes
#	include <VistaInterProcComm/Concurrency/VistaThreadEvent.h>

// 3rdParty includes
#	include <tbb/concurrent_queue.h>

// STL includes
#	include <algorithm>
#	include <atomic>
#	include <cassert>
#	include <cmath>
#	include <vector>


//! Generic sound path
class CVAPTGenericSoundPath : public CVAPoolObject
{
public:
	virtual ~CVAPTGenericSoundPath( );
	CVAPTGenericPathAudioRenderer::CVAPTGPSource* pSource;
	CVAPTGenericPathAudioRenderer::CVAPTGPListener* pListener;
	std::atomic<bool> bDelete;

	CITAVariableDelayLine* pVariableDelayLine;
	std::vector<ITAUPConvolution*> vpFIRConvolver; // N-channel convolver

	inline void PreRequest( )
	{
		pSource   = nullptr;
		pListener = nullptr;

		pVariableDelayLine->Clear( );

		for( size_t n = 0; n < vpFIRConvolver.size( ); n++ )
			vpFIRConvolver[n]->clear( );
	};

private:
	CVAPTGenericSoundPath( );
	CVAPTGenericSoundPath( double dSamplerate, int iBlocklength, int iNumChannels, int iIRFilterLength );

	friend class CVAPTGenericSoundPathFactory;
};

class CVAPTGenericSoundPathFactory : public IVAPoolObjectFactory
{
public:
	inline CVAPTGenericSoundPathFactory( double dSampleRate, int iBlockLength, int iNumChannels, int iIRFilterLength )
	    : m_dSampleRate( dSampleRate )
	    , m_iBlockLength( iBlockLength )
	    , m_iNumChannels( iNumChannels )
	    , m_iIRFilterLengthSamples( iIRFilterLength ) { };

	inline CVAPoolObject* CreatePoolObject( ) { return new CVAPTGenericSoundPath( m_dSampleRate, m_iBlockLength, m_iNumChannels, m_iIRFilterLengthSamples ); };

private:
	double m_dSampleRate;
	int m_iBlockLength;
	int m_iIRFilterLengthSamples;
	int m_iNumChannels;
};

class CVAPTGPListenerPoolFactory : public IVAPoolObjectFactory
{
public:
	inline CVAPTGPListenerPoolFactory( CVACoreImpl* pCore ) : m_pCore( pCore ) { };

	inline CVAPoolObject* CreatePoolObject( )
	{
		CVAPTGenericPathAudioRenderer::CVAPTGPListener* pListener;
		pListener        = new CVAPTGenericPathAudioRenderer::CVAPTGPListener( );
		pListener->pCore = m_pCore;
		return pListener;
	};

private:
	CVACoreImpl* m_pCore;
};

class CVAPTGPSourcePoolFactory : public IVAPoolObjectFactory
{
public:
	inline CVAPTGPSourcePoolFactory( ) { };

	inline CVAPoolObject* CreatePoolObject( )
	{
		CVAPTGenericPathAudioRenderer::CVAPTGPSource* pSource;
		pSource = new CVAPTGenericPathAudioRenderer::CVAPTGPSource( );
		return pSource;
	};
};


// Class CVAPTGenericSoundPath

CVAPTGenericSoundPath::CVAPTGenericSoundPath( double dSamplerate, int iBlocklength, int iNumChannels, int iIRFilterLength )
{
	if( iNumChannels < 1 )
		VA_EXCEPT2( INVALID_PARAMETER, "Number of channels must be positive" );

	const EVDLAlgorithm iAlgorithm = EVDLAlgorithm::CUBIC_SPLINE_INTERPOLATION;
	pVariableDelayLine   = new CITAVariableDelayLine( dSamplerate, iBlocklength, 6 * dSamplerate, iAlgorithm );

	for( int n = 0; n < iNumChannels; n++ )
	{
		ITAUPConvolution* pFIRConvolver = new ITAUPConvolution( iBlocklength, iIRFilterLength );
		pFIRConvolver->SetFilterExchangeFadingFunction( ITABase::FadingFunction::COSINE_SQUARE );
		pFIRConvolver->SetFilterCrossfadeLength( (std::min)( iBlocklength, 32 ) );
		pFIRConvolver->SetGain( 1.0f, true );
		ITAUPFilter* pHRIRFilterChL = pFIRConvolver->RequestFilter( );
		pHRIRFilterChL->Zeros( );
		pFIRConvolver->ExchangeFilter( pHRIRFilterChL );
		pFIRConvolver->ReleaseFilter( pHRIRFilterChL );

		vpFIRConvolver.push_back( pFIRConvolver );
	}
}

CVAPTGenericSoundPath::~CVAPTGenericSoundPath( )
{
	delete pVariableDelayLine;

	for( size_t n = 0; n < vpFIRConvolver.size( ); n++ )
		delete vpFIRConvolver[n];
}


// Renderer

CVAPTGenericPathAudioRenderer::CVAPTGenericPathAudioRenderer( const CVAAudioRendererInitParams& oParams )
    : m_pCore( oParams.pCore )
    , m_pCurSceneState( nullptr )
    , m_iIRFilterLengthSamples( -1 )
    , m_iNumChannels( -1 )
    , m_oParams( oParams )
{
	// read config
	Init( *oParams.pConfig );

	m_pOutput = new ITADatasourceRealization( m_iNumChannels, oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.dSampleRate,
	                                          oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.iBuffersize );
	m_pOutput->SetStreamEventHandler( this );

	IVAPoolObjectFactory* pListenerFactory = new CVAPTGPListenerPoolFactory( m_pCore );
	m_pListenerPool                        = IVAObjectPool::Create( 16, 2, pListenerFactory, true );

	IVAPoolObjectFactory* pSourceFactory = new CVAPTGPSourcePoolFactory( );
	m_pSourcePool                        = IVAObjectPool::Create( 16, 2, pSourceFactory, true );

	m_pSoundPathFactory = new CVAPTGenericSoundPathFactory( m_pOutput->GetSampleRate( ), m_pOutput->GetBlocklength( ), m_iNumChannels, m_iIRFilterLengthSamples );

	m_pSoundPathPool = IVAObjectPool::Create( 64, 8, m_pSoundPathFactory, true );

	m_pUpdateMessagePool = IVAObjectPool::Create( 2, 1, new CVAPoolObjectDefaultFactory<CVAPTGPUpdateMessage>, true );

	ctxAudio.m_iResetFlag = 0; // Normal operation mode
	ctxAudio.m_iStatus    = 0; // Stopped

	m_iCurGlobalAuralizationMode = IVAInterface::VA_AURAMODE_DEFAULT;

	m_sfTempBuffer.Init( oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.iBuffersize, true );
}

CVAPTGenericPathAudioRenderer::~CVAPTGenericPathAudioRenderer( )
{
	delete m_pSoundPathPool;
	delete m_pUpdateMessagePool;
}

void CVAPTGenericPathAudioRenderer::Init( const CVAStruct& oArgs )
{
	CVAConfigInterpreter conf( oArgs );

	conf.ReqInteger( "NumChannels", m_iNumChannels );

	const std::string sFIRLengthKey = oArgs.HasKey("MaxFilterLengthSamples") ? "MaxFilterLengthSamples" : "IRFilterLengthSamples"; //Backwards compatibility
	conf.OptInteger( sFIRLengthKey, m_iIRFilterLengthSamples, 22100 );

	if( m_iIRFilterLengthSamples < 0 )
		VA_EXCEPT2( INVALID_PARAMETER, "IR filter size must be positive" );

	conf.OptBool( "OutputMonitoring", m_bOutputMonitoring, false );

	return;
}

void CVAPTGenericPathAudioRenderer::Reset( )
{
	ctxAudio.m_iResetFlag = 1; // Request reset

	if( ctxAudio.m_iStatus == 0 || m_oParams.bOfflineRendering )
	{
		// if no streaming active, reset manually
		// SyncInternalData();
		ResetInternalData( );
	}

	// Wait for last streaming block before internal reset
	while( ctxAudio.m_iResetFlag != 2 )
	{
		VASleep( 100 ); // Wait for acknowledge
	}

	// Iterate over sound pathes and free items
	std::list<CVAPTGenericSoundPath*>::iterator it = m_lSoundPaths.begin( );
	while( it != m_lSoundPaths.end( ) )
	{
		CVAPTGenericSoundPath* pPath = *it;

		int iNumRefs = pPath->GetNumReferences( );
		assert( iNumRefs == 1 );
		pPath->RemoveReference( );

		++it;
	}
	m_lSoundPaths.clear( );

	// Iterate over sound receiver and free items
	std::map<int, CVAPTGPListener*>::const_iterator lcit = m_mListeners.begin( );
	while( lcit != m_mListeners.end( ) )
	{
		CVAPTGPListener* pListener( lcit->second );
		pListener->pData->RemoveReference( );
		assert( pListener->GetNumReferences( ) == 1 );
		pListener->RemoveReference( );
		lcit++;
	}
	m_mListeners.clear( );

	// Iterate over sources and free items
	std::map<int, CVAPTGPSource*>::const_iterator scit = m_mSources.begin( );
	while( scit != m_mSources.end( ) )
	{
		CVAPTGPSource* pSource( scit->second );
		pSource->pData->RemoveReference( );
		assert( pSource->GetNumReferences( ) == 1 );
		pSource->RemoveReference( );
		scit++;
	}
	m_mSources.clear( );

	// Scene frei geben
	if( m_pCurSceneState )
	{
		m_pCurSceneState->RemoveReference( );
		m_pCurSceneState = nullptr;
	}

	ctxAudio.m_iResetFlag = 0; // Enter normal mode
}

void CVAPTGenericPathAudioRenderer::UpdateScene( CVASceneState* pNewSceneState )
{
	assert( pNewSceneState );

	m_pNewSceneState = pNewSceneState;
	if( m_pNewSceneState == m_pCurSceneState )
		return;

	// Neue Szene referenzieren (gegen Freigabe sperren)
	m_pNewSceneState->AddReference( );

	// Unterschiede ermitteln: Neue Szene vs. alte Szene
	CVASceneStateDiff oDiff;
	pNewSceneState->Diff( m_pCurSceneState, &oDiff );

	// Leere Update-Nachricht zusammenstellen
	m_pUpdateMessage = dynamic_cast<CVAPTGPUpdateMessage*>( m_pUpdateMessagePool->RequestObject( ) );

	ManageSoundPaths( m_pCurSceneState, pNewSceneState, &oDiff );

	// Update-Nachricht an den Audiokontext schicken
	ctxAudio.m_qpUpdateMessages.push( m_pUpdateMessage );

	// Alte Szene freigeben (dereferenzieren)
	if( m_pCurSceneState )
		m_pCurSceneState->RemoveReference( );

	m_pCurSceneState = m_pNewSceneState;
	m_pNewSceneState = nullptr;
}

ITADatasource* CVAPTGenericPathAudioRenderer::GetOutputDatasource( )
{
	return m_pOutput;
}

void CVAPTGenericPathAudioRenderer::ManageSoundPaths( const CVASceneState* pCurScene, const CVASceneState* pNewScene, const CVASceneStateDiff* pDiff )
{
	// Iterate over current paths and mark deleted (will be removed within internal sync of audio context thread)
	std::list<CVAPTGenericSoundPath*>::iterator itp = m_lSoundPaths.begin( );
	while( itp != m_lSoundPaths.end( ) )
	{
		CVAPTGenericSoundPath* pPath( *itp );
		int iSourceID            = pPath->pSource->pData->iID;
		int iListenerID          = pPath->pListener->pData->iID;
		bool bDeletetionRequired = false;

		// Source deleted?
		std::vector<int>::const_iterator cits = pDiff->viDelSoundSourceIDs.begin( );
		while( cits != pDiff->viDelSoundSourceIDs.end( ) )
		{
			const int& iIDDeletedSource( *cits++ );
			if( iSourceID == iIDDeletedSource )
			{
				bDeletetionRequired = true; // Source removed, deletion required
				break;
			}
		}

		if( bDeletetionRequired == false )
		{
			// Receiver deleted?
			std::vector<int>::const_iterator citr = pDiff->viDelReceiverIDs.begin( );
			while( citr != pDiff->viDelReceiverIDs.end( ) )
			{
				const int& iIDListenerDeleted( *citr++ );
				if( iListenerID == iIDListenerDeleted )
				{
					bDeletetionRequired = true; // Listener removed, deletion required
					break;
				}
			}
		}

		if( bDeletetionRequired )
		{
			DeleteSoundPath( pPath );
			itp = m_lSoundPaths.erase( itp ); // Increment via erase on path list
		}
		else
		{
			++itp; // no deletion detected, continue
		}
	}

	// Deleted sources
	std::vector<int>::const_iterator cits = pDiff->viDelSoundSourceIDs.begin( );
	while( cits != pDiff->viDelSoundSourceIDs.end( ) )
	{
		const int& iID( *cits++ );
		DeleteSource( iID );
	}

	// Deleted receivers
	std::vector<int>::const_iterator citr = pDiff->viDelReceiverIDs.begin( );
	while( citr != pDiff->viDelReceiverIDs.end( ) )
	{
		const int& iID( *citr++ );
		DeleteListener( iID );
	}

	// New sources
	cits = pDiff->viNewSoundSourceIDs.begin( );
	while( cits != pDiff->viNewSoundSourceIDs.end( ) )
	{
		const int& iID( *cits++ );
		CVAPTGPSource* pSource = CreateSource( iID, pNewScene->GetSoundSourceState( iID ) );
	}

	// New receivers
	citr = pDiff->viNewReceiverIDs.begin( );
	while( citr != pDiff->viNewReceiverIDs.end( ) )
	{
		const int& iID( *citr++ );
		CVAPTGPListener* pListener = CreateListener( iID, pNewScene->GetReceiverState( iID ) );
	}

	// New paths: (1) new receivers, current sources
	citr = pDiff->viNewReceiverIDs.begin( );
	while( citr != pDiff->viNewReceiverIDs.end( ) )
	{
		int iListenerID            = ( *citr++ );
		CVAPTGPListener* pListener = m_mListeners[iListenerID];

		for( size_t i = 0; i < pDiff->viComSoundSourceIDs.size( ); i++ )
		{
			int iSourceID          = pDiff->viComSoundSourceIDs[i];
			CVAPTGPSource* pSource = m_mSources[iSourceID];
			if( !pSource->bDeleted ) // only, if not marked for deletion
				CVAPTGenericSoundPath* pPath = CreateSoundPath( pSource, pListener );
		}
	}

	// New paths: (2) new sources, current receivers
	cits = pDiff->viNewSoundSourceIDs.begin( );
	while( cits != pDiff->viNewSoundSourceIDs.end( ) )
	{
		const int& iSourceID( *cits++ );
		CVAPTGPSource* pSource = m_mSources[iSourceID];

		for( size_t i = 0; i < pDiff->viComReceiverIDs.size( ); i++ )
		{
			int iListenerID            = pDiff->viComReceiverIDs[i];
			CVAPTGPListener* pListener = m_mListeners[iListenerID];
			if( !pListener->bDeleted )
				CVAPTGenericSoundPath* pPath = CreateSoundPath( pSource, pListener );
		}
	}

	// New paths: (3) new sources, new receivers
	cits = pDiff->viNewSoundSourceIDs.begin( );
	while( cits != pDiff->viNewSoundSourceIDs.end( ) )
	{
		const int& iSourceID( *cits++ );
		CVAPTGPSource* pSource = m_mSources[iSourceID];

		citr = pDiff->viNewReceiverIDs.begin( );
		while( citr != pDiff->viNewReceiverIDs.end( ) )
		{
			const int& iListenerID( *citr++ );
			CVAPTGPListener* pListener   = m_mListeners[iListenerID];
			CVAPTGenericSoundPath* pPath = CreateSoundPath( pSource, pListener );
		}
	}

	return;
}

CVAPTGenericSoundPath* CVAPTGenericPathAudioRenderer::CreateSoundPath( CVAPTGPSource* pSource, CVAPTGPListener* pListener )
{
	int iSourceID   = pSource->pData->iID;
	int iListenerID = pListener->pData->iID;

	assert( !pSource->bDeleted && !pListener->bDeleted );

	VA_VERBOSE( "PTGenericPathAudioRenderer", "Creating sound path from source " << iSourceID << " -> sound receiver " << iListenerID );

	CVAPTGenericSoundPath* pPath = dynamic_cast<CVAPTGenericSoundPath*>( m_pSoundPathPool->RequestObject( ) );

	pPath->pSource   = pSource;
	pPath->pListener = pListener;

	pPath->bDelete = false;

	m_lSoundPaths.push_back( pPath );
	m_pUpdateMessage->vNewPaths.push_back( pPath );

	return pPath;
}

void CVAPTGenericPathAudioRenderer::DeleteSoundPath( CVAPTGenericSoundPath* pPath )
{
	VA_VERBOSE( "PTGenericPathAudioRenderer",
	            "Marking sound path from source " << pPath->pSource->pData->iID << " -> sound receiver " << pPath->pListener->pData->iID << " for deletion" );

	pPath->bDelete = true;
	pPath->RemoveReference( );
	m_pUpdateMessage->vDelPaths.push_back( pPath );
}

CVAPTGenericPathAudioRenderer::CVAPTGPListener* CVAPTGenericPathAudioRenderer::CreateListener( const int iID, const CVAReceiverState* pListenerState )
{
	VA_VERBOSE( "PTGenericPathAudioRenderer", "Creating sound receiver with ID " << iID );

	CVAPTGPListener* pListener = dynamic_cast<CVAPTGPListener*>( m_pListenerPool->RequestObject( ) ); // Reference = 1

	pListener->pData = m_pCore->GetSceneManager( )->GetSoundReceiverDesc( iID );
	pListener->pData->AddReference( );

	// Move to prerequest of pool?
	pListener->psfOutput = new ITASampleFrame( m_iNumChannels, m_pCore->GetCoreConfig( )->oAudioDriverConfig.iBuffersize, true );
	assert( pListener->pData );
	pListener->bDeleted = false;

	m_mListeners.insert( std::pair<int, CVAPTGenericPathAudioRenderer::CVAPTGPListener*>( iID, pListener ) );

	m_pUpdateMessage->vNewListeners.push_back( pListener );

	return pListener;
}

void CVAPTGenericPathAudioRenderer::DeleteListener( int iListenerID )
{
	VA_VERBOSE( "PTGenericPathAudioRenderer", "Marking sound receiver with ID " << iListenerID << " for removal" );
	std::map<int, CVAPTGPListener*>::iterator it = m_mListeners.find( iListenerID );
	CVAPTGPListener* pListener                   = it->second;
	m_mListeners.erase( it );
	pListener->bDeleted = true;
	pListener->pData->RemoveReference( );
	pListener->RemoveReference( );

	m_pUpdateMessage->vDelListeners.push_back( pListener );

	return;
}

CVAPTGenericPathAudioRenderer::CVAPTGPSource* CVAPTGenericPathAudioRenderer::CreateSource( int iID, const CVASoundSourceState* pSourceState )
{
	VA_VERBOSE( "PTGenericPathAudioRenderer", "Creating source with ID " << iID );
	CVAPTGPSource* pSource = dynamic_cast<CVAPTGPSource*>( m_pSourcePool->RequestObject( ) );

	pSource->pData = m_pCore->GetSceneManager( )->GetSoundSourceDesc( iID );
	pSource->pData->AddReference( );

	pSource->bDeleted = false;

	m_mSources.insert( std::pair<int, CVAPTGPSource*>( iID, pSource ) );

	m_pUpdateMessage->vNewSources.push_back( pSource );

	return pSource;
}

void CVAPTGenericPathAudioRenderer::DeleteSource( int iSourceID )
{
	VA_VERBOSE( "PTGenericPathAudioRenderer", "Marking source with ID " << iSourceID << " for removal" );
	std::map<int, CVAPTGPSource*>::iterator it = m_mSources.find( iSourceID );
	CVAPTGPSource* pSource                     = it->second;
	m_mSources.erase( it );
	pSource->bDeleted = true;
	pSource->pData->RemoveReference( );
	pSource->RemoveReference( );

	m_pUpdateMessage->vDelSources.push_back( pSource );

	return;
}

void CVAPTGenericPathAudioRenderer::SyncInternalData( )
{
	CVAPTGPUpdateMessage* pUpdate;
	while( ctxAudio.m_qpUpdateMessages.try_pop( pUpdate ) )
	{
		std::list<CVAPTGenericSoundPath*>::const_iterator citp = pUpdate->vDelPaths.begin( );
		while( citp != pUpdate->vDelPaths.end( ) )
		{
			CVAPTGenericSoundPath* pPath( *citp++ );
			ctxAudio.m_lSoundPaths.remove( pPath );
			pPath->RemoveReference( );
		}

		citp = pUpdate->vNewPaths.begin( );
		while( citp != pUpdate->vNewPaths.end( ) )
		{
			CVAPTGenericSoundPath* pPath( *citp++ );
			pPath->AddReference( );
			ctxAudio.m_lSoundPaths.push_back( pPath );
		}

		std::list<CVAPTGPSource*>::const_iterator cits = pUpdate->vDelSources.begin( );
		while( cits != pUpdate->vDelSources.end( ) )
		{
			CVAPTGPSource* pSource( *cits++ );
			ctxAudio.m_lSources.remove( pSource );
			pSource->pData->RemoveReference( );
			pSource->RemoveReference( );
		}

		cits = pUpdate->vNewSources.begin( );
		while( cits != pUpdate->vNewSources.end( ) )
		{
			CVAPTGPSource* pSource( *cits++ );
			pSource->AddReference( );
			pSource->pData->AddReference( );
			ctxAudio.m_lSources.push_back( pSource );
		}

		std::list<CVAPTGPListener*>::const_iterator citl = pUpdate->vDelListeners.begin( );
		while( citl != pUpdate->vDelListeners.end( ) )
		{
			CVAPTGPListener* pListener( *citl++ );
			ctxAudio.m_lListener.remove( pListener );
			pListener->pData->RemoveReference( );
			pListener->RemoveReference( );
		}

		citl = pUpdate->vNewListeners.begin( );
		while( citl != pUpdate->vNewListeners.end( ) )
		{
			CVAPTGPListener* pListener( *citl++ );
			pListener->AddReference( );
			pListener->pData->AddReference( );
			ctxAudio.m_lListener.push_back( pListener );
		}

		pUpdate->RemoveReference( );
	}

	return;
}

void CVAPTGenericPathAudioRenderer::ResetInternalData( )
{
	std::list<CVAPTGenericSoundPath*>::const_iterator citp = ctxAudio.m_lSoundPaths.begin( );
	while( citp != ctxAudio.m_lSoundPaths.end( ) )
	{
		CVAPTGenericSoundPath* pPath( *citp++ );
		pPath->RemoveReference( );
	}
	ctxAudio.m_lSoundPaths.clear( );

	std::list<CVAPTGPListener*>::const_iterator itl = ctxAudio.m_lListener.begin( );
	while( itl != ctxAudio.m_lListener.end( ) )
	{
		CVAPTGPListener* pListener( *itl++ );
		pListener->pData->RemoveReference( );
		pListener->RemoveReference( );
	}
	ctxAudio.m_lListener.clear( );

	std::list<CVAPTGPSource*>::const_iterator cits = ctxAudio.m_lSources.begin( );
	while( cits != ctxAudio.m_lSources.end( ) )
	{
		CVAPTGPSource* pSource( *cits++ );
		pSource->pData->RemoveReference( );
		pSource->RemoveReference( );
	}
	ctxAudio.m_lSources.clear( );

	ctxAudio.m_iResetFlag = 2; // set ack
}

void CVAPTGenericPathAudioRenderer::UpdateGenericSoundPath( int iListenerID, int iSourceID, const std::string& sIRFilePath )
{
	try
	{
		ITASampleFrame sfIR( sIRFilePath );
		UpdateGenericSoundPath( iListenerID, iSourceID, sfIR );
	}
	catch( ITAException e )
	{
		VA_ERROR( "PTGenericPathAudioRenderer", "IR file path '" << sIRFilePath << "' could not be loaded for an update in generic path renderer." );
	}
}

void CVAPTGenericPathAudioRenderer::UpdateGenericSoundPath( int iListenerID, int iSourceID, int iChannelIndex, const std::string& sIRFilePath )
{
	if( iChannelIndex >= m_iNumChannels || iChannelIndex < 0 )
		VA_EXCEPT2( INVALID_PARAMETER, "Requested filter channel greater than output channels or smaller than 1, can not update." );

	ITASampleFrame sfIR( sIRFilePath );
	if( sfIR.channels( ) != 1 )
	{
		if( sfIR.channels( ) != m_iNumChannels )
		{
			VA_ERROR( "PTGenericPathAudioRenderer", "Filter has mismatching and more than one channel. Don't know what to do - refusing this update." );
		}
		else
		{
			VA_WARN( "PTGenericPathAudioRenderer", "Filter has more than one channel, updating only requested channel index." );
			UpdateGenericSoundPath( iListenerID, iSourceID, iChannelIndex, sfIR[iChannelIndex] );
		}
	}
	else
	{
		UpdateGenericSoundPath( iListenerID, iSourceID, iChannelIndex, sfIR[0] );
	}
}
void CVAPTGenericPathAudioRenderer::UpdateGenericSoundPath( const int iListenerID, const int iSourceID, const double dDelaySeconds )
{
	if( dDelaySeconds < 0.0f )
		VA_WARN( "PTGenericPathAudioRenderer", "Delay in variable delay line can not be anti-causal. Shift IR if necessary." );

	std::list<CVAPTGenericSoundPath*>::const_iterator spcit = m_lSoundPaths.begin( );
	while( spcit != m_lSoundPaths.end( ) )
	{
		CVAPTGenericSoundPath* pPath( *spcit++ );
		if( pPath->pListener->pData->iID == iListenerID && pPath->pSource->pData->iID == iSourceID )
		{
			pPath->pVariableDelayLine->SetDelayTime( dDelaySeconds );
			return;
		}
	}

	VA_ERROR( "PTGenericPathAudioRenderer", "Could not find sound path for sound receiver " << iListenerID << " and source " << iSourceID );
}

void CVAPTGenericPathAudioRenderer::UpdateGenericSoundPath( int iListenerID, int iSourceID, ITASampleFrame& sfIR )
{
	if( sfIR.length( ) > m_iIRFilterLengthSamples )
		VA_WARN( "PTGenericPathAudioRenderer", "Filter length for generic sound path too long, cropping." );

	int iNumChannels = (std::min)( m_iNumChannels, sfIR.GetNumChannels( ) );
	if( m_iNumChannels != sfIR.GetNumChannels( ) )
		VA_WARN( "PTGenericPathAudioRenderer", "Found " << sfIR.GetNumChannels( ) << " channels in the IR file, but renderer is running with " << m_iNumChannels
		                                                << ". Will update only first " << iNumChannels << " channel(s)." );


	std::list<CVAPTGenericSoundPath*>::const_iterator spcit = m_lSoundPaths.begin( );
	while( spcit != m_lSoundPaths.end( ) )
	{
		CVAPTGenericSoundPath* pPath( *spcit++ );
		if( pPath->pListener->pData->iID == iListenerID && pPath->pSource->pData->iID == iSourceID )
		{
			for( int n = 0; n < iNumChannels; n++ )
			{
				ITAUPConvolution* pConvolver( pPath->vpFIRConvolver[n] );
				ITAUPFilter* pFilter = pConvolver->RequestFilter( );
				pFilter->Zeros( );
				int iLength = (std::min)( m_iIRFilterLengthSamples, sfIR.length( ) );
				pFilter->Load( sfIR[n].data( ), iLength );

				pConvolver->ExchangeFilter( pFilter );
				pFilter->Release( );
			}
			return;
		}
	}

	VA_ERROR( "PTGenericPathAudioRenderer", "Could not find sound path for sound receiver " << iListenerID << " and source " << iSourceID );
}


void CVAPTGenericPathAudioRenderer::UpdateGenericSoundPath( int iListenerID, int iSourceID, int iChannelIndex, ITASampleBuffer& sbIR )
{
	if( iChannelIndex >= m_iNumChannels || iChannelIndex < 0 )
		VA_EXCEPT2( INVALID_PARAMETER, "Requested filter channel index of generic sound path out of bounds" );

	if( sbIR.length( ) > m_iIRFilterLengthSamples )
		VA_WARN( "PTGenericPathAudioRenderer", "Filter length for generic sound path channel too long, cropping." );

	std::list<CVAPTGenericSoundPath*>::const_iterator spcit = m_lSoundPaths.begin( );
	while( spcit != m_lSoundPaths.end( ) )
	{
		CVAPTGenericSoundPath* pPath( *spcit++ );
		if( pPath->pListener->pData->iID == iListenerID && pPath->pSource->pData->iID == iSourceID )
		{
			ITAUPConvolution* pConvolver( pPath->vpFIRConvolver[iChannelIndex] );
			ITAUPFilter* pFilter = pConvolver->RequestFilter( );
			pFilter->Zeros( );
			int iLength = (std::min)( m_iIRFilterLengthSamples, sbIR.length( ) );
			pFilter->Load( sbIR.data( ), iLength );
			pConvolver->ExchangeFilter( pFilter );
			pFilter->Release( );

			return;
		}
	}

	VA_ERROR( "PTGenericPathAudioRenderer", "Could not find sound path for sound receiver " << iListenerID << " and source " << iSourceID );
}

void CVAPTGenericPathAudioRenderer::UpdateGlobalAuralizationMode( int )
{
	return;
}

void CVAPTGenericPathAudioRenderer::HandleProcessStream( ITADatasourceRealization*, const ITAStreamInfo* pStreamInfo )
{
	// If streaming is active, set to 1
	ctxAudio.m_iStatus = 1;

	// Sync pathes
	SyncInternalData( );

	const CVAAudiostreamState* pStreamState = dynamic_cast<const CVAAudiostreamState*>( pStreamInfo );
	double dListenerTime                    = pStreamState->dSysTime;

	// Check for reset request
	if( ctxAudio.m_iResetFlag == 1 )
	{
		ResetInternalData( );

		return;
	}
	else if( ctxAudio.m_iResetFlag == 2 )
	{
		// Reset active, skip until finished
		return;
	}

	std::map<int, CVAPTGPListener*>::iterator lit = m_mListeners.begin( );
	while( lit != m_mListeners.end( ) )
	{
		CVAPTGPListener* pListener( lit->second );
		pListener->psfOutput->zero( );
		lit++;
	}

	// Update sound pathes
	std::list<CVAPTGenericSoundPath*>::iterator spit = ctxAudio.m_lSoundPaths.begin( );
	while( spit != ctxAudio.m_lSoundPaths.end( ) )
	{
		CVAPTGenericSoundPath* pPath( *spit );
		CVAReceiverState* pListenerState  = ( m_pCurSceneState ? m_pCurSceneState->GetReceiverState( pPath->pListener->pData->iID ) : NULL );
		CVASoundSourceState* pSourceState = ( m_pCurSceneState ? m_pCurSceneState->GetSoundSourceState( pPath->pSource->pData->iID ) : NULL );

		if( pListenerState == nullptr || pSourceState == nullptr )
		{
			// Skip if no data is present
			spit++;
			continue;
		}

		CVASoundSourceDesc* pSourceData = pPath->pSource->pData;
		const ITASampleFrame& sfInputBuffer( *pSourceData->psfSignalSourceInputBuf );
		const ITASampleBuffer* psbInput( &( sfInputBuffer[0] ) );

		pPath->pVariableDelayLine->Process( psbInput, &m_sfTempBuffer );

		float fSoundSourceGain = float( pSourceState->GetVolume( m_oParams.pCore->GetCoreConfig( )->dDefaultAmplitudeCalibration ) );

		for( int n = 0; n < m_iNumChannels; n++ )
		{
			ITAUPConvolution* pConvolver = pPath->vpFIRConvolver[n];
			pConvolver->SetGain( fSoundSourceGain );
			pConvolver->Process( m_sfTempBuffer.GetData( ), ( *pPath->pListener->psfOutput )[n].data( ), ITABase::MixingMethod::ADD );
		}

		spit++;
	}

	// Zero output
	for( int n = 0; n < int( m_pOutput->GetNumberOfChannels( ) ); n++ )
		fm_zero( m_pOutput->GetWritePointer( n ), m_pOutput->GetBlocklength( ) );

	// Mix receivers
	for( auto it: m_mListeners )
	{
		CVAPTGPListener* pReceiver( it.second );
		if( !pReceiver->pData->bMuted )
			for( int n = 0; n < int( m_pOutput->GetNumberOfChannels( ) ); n++ )
				for( int i = 0; i < int( m_pOutput->GetBlocklength( ) ); i++ )
					m_pOutput->GetWritePointer( n )[i] += ( *pReceiver->psfOutput )[n][i];

		bool bDataPresent = false;
		std::vector<float> vfRMS( m_iNumChannels );
		for( int n = 0; n < int( m_pOutput->GetNumberOfChannels( ) ); n++ )
		{
			vfRMS[n] = 0.0f;
			for( int i = 0; i < int( m_pOutput->GetBlocklength( ) ); i++ )
				vfRMS[n] += pow( std::abs( m_pOutput->GetWritePointer( n )[i] ), 2 );
			vfRMS[n] = std::sqrt( vfRMS[n] ) / m_pOutput->GetBlocklength( );
			if( vfRMS[n] != 0.0f )
				bDataPresent = true;
		}

		if( !pReceiver->pData->bMuted && m_bOutputMonitoring &&
		    int( pStreamInfo->nSamples / m_pOutput->GetBlocklength( ) ) % int( m_pOutput->GetSampleRate( ) / m_pOutput->GetBlocklength( ) * 2.0f ) == 0 )
		{
			if( bDataPresent )
			{
				VA_TRACE( m_oParams.sID, "RMS at active sound receiver: <Ch1," << vfRMS[0] << "> <Ch2," << vfRMS[1] << "> " );
			}
			else
			{
				VA_WARN( m_oParams.sID, "No data on output channel for active sound receiver present (empty path filter?)" );
			}
		}
	}

	m_pOutput->IncrementWritePointer( );
}

std::string CVAPTGenericPathAudioRenderer::HelpText( ) const
{
	std::stringstream ss;
	ss << std::endl;
	ss << " --- GenericPath renderer instance '" << m_oParams.sID << "' ---" << std::endl;
	ss << std::endl;
	ss << "[help]" << std::endl;
	ss << "If the call module struct contains a key with the name 'help', this help text will be shown and the return struct will be returned with the key name 'help'."
	   << std::endl;
	ss << std::endl;
	ss << "[info]" << std::endl;
	ss << "If the call module struct contains a key with the name 'info', information on the static configuration of the renderer will be returned." << std::endl;
	ss << std::endl;
	ss << "[update]" << std::endl;
	ss << "For every successful path update, the VA source and sound receiver ID has to be passed like this:" << std::endl;
	ss << " receiver: <int>, the number of the sound receiver identifier" << std::endl;
	ss << " source: <int>, the number of the source identifier" << std::endl;
	ss << std::endl;
	ss << "Updating the path filter (impulse response in time domain) for a sound receiver and a source can be performed in two ways:" << std::endl;
	ss << " a) using a path to a multi-channel WAV file:" << std::endl;
	ss << "    Provide a key with the name 'filepath' and the path to the WAV file (absolute or containing the macro '$(VADataDir)' or relative to the executable) "
	      "[priority given to 'filepath' if b) also applies]"
	   << std::endl;
	ss << " b) sending floating-point data for each channel" << std::endl;
	ss << "    Provide a key for each channel with the generic name 'ch#', where the hash is substituted by the actual number of channel (starting at 1), and the value "
	      "to this key will contain floating point data (or a sample buffer). "
	   << "The call parameter struct does not necessarily have to contain all channels, also single channels will be updated if key is given." << std::endl;
	ss << std::endl;
	ss << "Note: the existence of the key 'verbose' will print update information at server console and will provide the update info as an 'info' key in the returned "
	      "struct."
	   << std::endl;
	return ss.str( );
}

CVAStruct CVAPTGenericPathAudioRenderer::GetParameters( const CVAStruct& oArgs ) const
{
	CVAStruct oReturn;

	if( oArgs.HasKey( "help" ) )
	{
		// Print and return help text
		VA_PRINT( HelpText( ) );
		oReturn["help"] = HelpText( );
		return oReturn;
	}

	oReturn["numchannels"]           = m_iNumChannels;
	oReturn["irfilterlengthsamples"] = m_iIRFilterLengthSamples;
	oReturn["numpaths"]              = int( m_lSoundPaths.size( ) );
	return oReturn;
}

void CVAPTGenericPathAudioRenderer::SetParameters( const CVAStruct& oArgs )
{
	// Update
	if( oArgs.HasKey( "receiver" ) == false || oArgs.HasKey( "source" ) == false )
	{
		VA_INFO( "PrototypeGenericPath", "Parameter setter was called without source or receiver id, did not update any generic path filter" );
		return;
	}

	int iReceiverID = oArgs["receiver"];
	int iSourceID   = oArgs["source"];

	bool bVerbose = false;
	if( oArgs.HasKey( "verbose" ) )
		bVerbose = true;

	if( oArgs.HasKey( "delay" ) )
	{
		const double dDelaySeconds = oArgs["delay"];
		UpdateGenericSoundPath( iReceiverID, iSourceID, dDelaySeconds );
	}

	if( oArgs.HasKey( "filepath" ) )
	{
		const std::string& sFilePathRaw( oArgs["filepath"] );
		std::string sFilePath = m_pCore->FindFilePath( sFilePathRaw );

		if( sFilePath.empty( ) )
		{
			VA_WARN( "PTGenericPathAudioRenderer", "Given IR filter '" << sFilePathRaw << "' could not be found. Skipping this update." );
			return;
		}

		if( oArgs.HasKey( "channel" ) )
		{
			int iChannelNumber = oArgs["channel"];
			UpdateGenericSoundPath( iReceiverID, iSourceID, iChannelNumber - 1, sFilePath );
		}
		else
		{
			UpdateGenericSoundPath( iReceiverID, iSourceID, sFilePath );
		}

		std::stringstream ssVerboseText;
		if( bVerbose )
		{
			ssVerboseText << "Updated sound path <L" << iReceiverID << ", S" << iSourceID << "> using (unrolled) file path '" << sFilePath << "'";
			VA_PRINT( ssVerboseText.str( ) );
		}
	}
	else
	{
		ITASampleBuffer sbIR( m_iIRFilterLengthSamples );
		for( int n = 0; n < int( m_pOutput->GetNumberOfChannels( ) ); n++ )
		{
			std::string sChKey = "ch" + IntToString( n + 1 );
			if( !oArgs.HasKey( sChKey ) )
				continue;

			const CVAStructValue& oValue( oArgs[sChKey] );
			int iNumSamples = 0;

			if( oValue.GetDatatype( ) == CVAStructValue::DATA )
			{
				int iBytes  = oValue.GetDataSize( );
				iNumSamples = iBytes / int( sizeof( float ) ); // crop overlapping bytes for safety (by integer division)

				if( iNumSamples > m_iIRFilterLengthSamples )
				{
					VA_WARN( "PTGenericPathAudioRenderer", "Given IR filter too long, cropping to fit internal filter length." );
					iNumSamples = m_iIRFilterLengthSamples;
				}

				const float* pfData = (const float*)( oValue.GetData( ) );
				sbIR.Zero( );
				sbIR.write( pfData, iNumSamples );
			}
			else if( oValue.GetDatatype( ) == CVAStructValue::SAMPLEBUFFER )
			{
				const CVASampleBuffer& oSampleBuffer = oValue;

				iNumSamples = oSampleBuffer.GetNumSamples( ); // crop overlapping bytes for safety (by integer division)

				if( iNumSamples > m_iIRFilterLengthSamples )
				{
					VA_WARN( "PTGenericPathAudioRenderer", "Given IR filter too long, cropping to fit internal filter length." );
					iNumSamples = m_iIRFilterLengthSamples;
				}

				sbIR.Zero( );
				sbIR.write( oSampleBuffer.GetDataReadOnly( ), iNumSamples );
			}

			UpdateGenericSoundPath( iReceiverID, iSourceID, n, sbIR );

			std::stringstream ssVerboseText;
			if( bVerbose )
			{
				ssVerboseText << "Updated sound path <L" << iReceiverID << ", S" << iSourceID << ", C" << n + 1 << "> with " << iNumSamples << " new samples";
				VA_PRINT( ssVerboseText.str( ) );
			}
		}
	}
}

#endif // VACORE_WITH_RENDERER_PROTOTYPE_GENERIC_PATH
