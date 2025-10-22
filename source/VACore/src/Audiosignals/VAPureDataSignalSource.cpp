#include "VAPureDataSignalSource.h"

// VA includes
#include "../VAAudiostreamTracker.h"

#include <VACore.h>

// STL includes


CVAPureDataSignalSource::CVAPureDataSignalSource( const std::string& sPatchFilePath, const double dSamplerate, const int iBlockSize )
    : m_pAssociatedCore( nullptr )
    , m_sPatchFilePath( sPatchFilePath )
{
}

CVAPureDataSignalSource::~CVAPureDataSignalSource( ) {}

void CVAPureDataSignalSource::HandleRegistration( IVACore* pParentCore )
{
	m_pAssociatedCore = pParentCore;
}

void CVAPureDataSignalSource::HandleUnregistration( IVACore* pParentCore )
{
	m_pAssociatedCore = nullptr;
}

const float* CVAPureDataSignalSource::GetStreamBlock( const CVAAudiostreamState* pStreamInfo )
{
	const float* pfData = nullptr;

	m_sbBuffer.Zero( );

	return m_sbBuffer.data( );
}
