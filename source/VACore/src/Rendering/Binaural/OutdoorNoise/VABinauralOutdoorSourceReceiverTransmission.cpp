#include "VABinauralOutdoorSourceReceiverTransmission.h"

// VA core includes
#include "../../../Motion/VAMotionModelBase.h"
#include "../../../Motion/VASharedMotionModel.h"
#include "../../../Scene/VAScene.h"

// ITA includes
#include <ITAException.h>

// STL includes
#include <cmath>


CVABinauralOutdoorSourceReceiverTransmission::CVABinauralOutdoorSourceReceiverTransmission( CVABinauralOutdoorSource* pSource, CVABinauralClusteringReceiver* pReceiver )
    : m_pSource( pSource )
    , m_pReceiver( pReceiver )
{
}

CVABinauralOutdoorSourceReceiverTransmission::~CVABinauralOutdoorSourceReceiverTransmission( ) {}


void CVABinauralOutdoorSourceReceiverTransmission::SetSource( CVABinauralOutdoorSource* pSource )
{
	m_pSource = pSource;
	for( auto itPath = m_mSoundPaths.begin( ); itPath != m_mSoundPaths.end( ); itPath++ )
		itPath->second->SetSource( pSource );
}

void CVABinauralOutdoorSourceReceiverTransmission::SetReceiver( CVABinauralClusteringReceiver* pReceiver )
{
	m_pReceiver = pReceiver;
	for( auto itPath = m_mSoundPaths.begin( ); itPath != m_mSoundPaths.end( ); itPath++ )
		itPath->second->SetReceiver( pReceiver );
}

void CVABinauralOutdoorSourceReceiverTransmission::AddWavefront( std::string sID, CVABinauralOutdoorWaveFront* pWavefront )
{
	if( !pWavefront )
		ITA_EXCEPT_INVALID_PARAMETER( "Given wavefront is empty" );

	m_mSoundPaths[sID] = pWavefront;
}

// void CVABinauralOutdoorSourceReceiverTransmission::RemoveWavefront(CVABinauralOutdoorWaveFront* pWavefront)
//{
// for (auto it = m_mSoundPaths.begin(); it != m_mSoundPaths.end(); it++)
//{
//	if (it->second == pWavefront)
//	{
//		m_mSoundPaths.erase(it);
//		return;
//	}
//}
//}

void CVABinauralOutdoorSourceReceiverTransmission::RemoveWavefront( const std::string& sID )
{
	m_mSoundPaths.erase( sID );
}

CVABinauralOutdoorWaveFront* CVABinauralOutdoorSourceReceiverTransmission::GetWavefront( const std::string& sID )
{
	auto it = m_mSoundPaths.find( sID );
	if( it == m_mSoundPaths.end( ) )
		return nullptr;

	return it->second;
}

void CVABinauralOutdoorSourceReceiverTransmission::UpdateFilterPropertyHistories( double dTime )
{
	for( auto itPath = m_mSoundPaths.begin( ); itPath != m_mSoundPaths.end( ); itPath++ )
		itPath->second->UpdateFilterPropertyHistories( dTime );
}

bool CVABinauralOutdoorSourceReceiverTransmission::IsEqual( const CVABinauralOutdoorSource* pSource, const CVABinauralClusteringReceiver* pReceiver )
{
	return pSource == m_pSource && pReceiver == m_pReceiver;
}
