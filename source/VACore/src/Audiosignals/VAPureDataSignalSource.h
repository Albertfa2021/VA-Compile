#ifndef __VA_PUREDATASIGNALSOURCE_H__
#define __VA_PUREDATASIGNALSOURCE_H__

// STL includes
#include <string>

// VA includes
#include <ITADatasourceDelegator.h>
#include <ITASampleBuffer.h>
#include <VAAudioSignalSource.h>

// Forward declarations
class VAPDPatch;

/**
 * The Pure Data Signal Source can load patches created
 * with Pure Data (pd). It provides general functionality
 * that grabs the patche's output and can be used as
 * a source for VA.
 * More complex patches including messages etc. require
 * a derived version of this class that changes the internal
 * state of the pd network.
 */
class CVAPureDataSignalSource : public IVAAudioSignalSource
{
public:
	CVAPureDataSignalSource( const std::string& sPatchFilePath, const double dSamplerate, const int iBlockSize );
	virtual ~CVAPureDataSignalSource( );

	std::string GetPatchFilePath( ) const { return m_sPatchFilePath; };

	// IVAAudioSignalSource interface
	int GetType( ) const { return VA_SS_PDPATCH; };
	std::string GetTypeString( ) const { return "pdpatch"; };
	std::string GetDesc( ) const { return "Pure Data (pd) patch file sound signal source"; };
	std::string GetStateString( ) const { return "running"; };
	IVACore* GetAssociatedCore( ) const { return m_pAssociatedCore; };
	const float* GetStreamBlock( const CVAAudiostreamState* pStreamInfo );

private:
	IVACore* m_pAssociatedCore;
	VAPDPatch* m_pPDPatch;
	ITASampleBuffer m_sbBuffer;
	const std::string m_sPatchFilePath;

	void HandleRegistration( IVACore* pParentCore );
	void HandleUnregistration( IVACore* pParentCore );
};

#endif // __VA_PUREDATASIGNALSOURCE_H__
