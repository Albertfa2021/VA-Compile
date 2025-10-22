#ifndef IW_VA_JET_ENGINE_SIGNAL_SOURCE
#define IW_VA_JET_ENGINE_SIGNAL_SOURCE

// STL includes
#include <string>

// VA includes
#include <ITADataSourceDelegator.h>
#include <ITASampleBuffer.h>
#include <VAAudioSignalSource.h>

class CVACoreImpl;
namespace ITADSP
{
	namespace PD
	{
		class CJetEngine;
	}
} // namespace ITADSP

/**
 * Jet engine signal source based on the
 * pure data patch by Andy Farnell / Designing Sound.
 */
class CVAJetEngineSignalSource : public IVAAudioSignalSource
{
public:
	class Config
	{
	public:
		// std::map< double, double > lFreqModesSpectrum; //!< Mode spectrum [Hz], Amplitude
		bool bColdStart;
		float fRPMInit;
		bool bDelayedStart;
		CVACoreImpl* pCore;

		inline Config( ) : pCore( NULL )
		{
			bColdStart    = true;
			fRPMInit      = 1000.0f;
			bDelayedStart = false;
		};
	};

	CVAJetEngineSignalSource( const Config& );
	virtual ~CVAJetEngineSignalSource( );

	// IVAAudioSignalSource interface
	int GetType( ) const { return VA_SS_JETENGINE; };
	std::string GetTypeString( ) const { return "jetengine"; };
	std::string GetDesc( ) const { return "Jet engine (turbine & burn) signal source"; };
	std::string GetStateString( ) const { return "running"; };
	IVAInterface* GetAssociatedCore( ) const { return m_pAssociatedCore; };
	std::vector<const float*> GetStreamBlock( const CVAAudiostreamState* pStreamInfo );
	CVAStruct GetParameters( const CVAStruct& ) const;
	void SetParameters( const CVAStruct& oIn );

private:
	IVAInterface* m_pAssociatedCore;
	ITASampleBuffer m_sbBuffer;

	ITADSP::PD::CJetEngine* m_pJetEngine;
	bool m_bHoldOn; //!< Used to delay start

	void HandleRegistration( IVAInterface* pParentCore );
	void HandleUnregistration( IVAInterface* pParentCore );
};

#endif // IW_VA_JET_ENGINE_SIGNAL_SOURCE
