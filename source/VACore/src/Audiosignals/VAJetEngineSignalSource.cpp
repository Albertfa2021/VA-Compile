#include "VAJetEngineSignalSource.h"

// VA includes
#include "../VAAudiostreamTracker.h"
#include "../core/core.h"

// ITADSP includes
#include <ITADSP/PD/JetEngine.h>

// STL includes


CVAJetEngineSignalSource::CVAJetEngineSignalSource( const Config& oConf ) : m_pAssociatedCore( nullptr )
{
	m_sbBuffer.Init( oConf.pCore->GetCoreConfig( )->oAudioDriverConfig.iBuffersize, true );

	double dSampleRate = oConf.pCore->GetCoreConfig( )->oAudioDriverConfig.dSampleRate;
	float fRPMInit     = oConf.fRPMInit;
	bool bColdStart    = oConf.bColdStart;
	m_pJetEngine       = new ITADSP::PD::CJetEngine( dSampleRate, fRPMInit, bColdStart );

	m_bHoldOn = oConf.bDelayedStart;
}

CVAJetEngineSignalSource::~CVAJetEngineSignalSource( )
{
	delete m_pJetEngine;
}

void CVAJetEngineSignalSource::HandleRegistration( IVAInterface* pParentCore )
{
	m_pAssociatedCore = pParentCore;
}

void CVAJetEngineSignalSource::HandleUnregistration( IVAInterface* pParentCore )
{
	m_pAssociatedCore = nullptr;
}

std::vector<const float*> CVAJetEngineSignalSource::GetStreamBlock( const CVAAudiostreamState* pStreamInfo )
{
	if( !m_bHoldOn )
		m_pJetEngine->Process( m_sbBuffer.GetData( ), m_sbBuffer.GetLength( ) );
	std::vector<const float*> vpfBuffer = { m_sbBuffer.GetData( ) };
	return vpfBuffer;
}

CVAStruct CVAJetEngineSignalSource::GetParameters( const CVAStruct& ) const
{
	CVAStruct oRetVals;
	return oRetVals;
}

void CVAJetEngineSignalSource::SetParameters( const CVAStruct& oIn )
{
	if( oIn.HasKey( "rpm" ) )
	{
		double dRPM = oIn["rpm"];
		m_pJetEngine->SetRPM( (float)dRPM );
	}

	if( oIn.HasKey( "start" ) )
	{
		m_bHoldOn = false;
	}
}
