#include <VAException.h>
#include <VANetworkStreamAudioSignalSource.h>
#include <VASamples.h>
#include <VistaBase/VistaTimeUtils.h>
#include <VistaInterProcComm/Concurrency/VistaTicker.h>
#include <algorithm>
#include <iostream>

using namespace std;
const static int g_iBlockLength   = 128;
const static double g_dSampleRate = 44.1e3;


//! Example for a sample generator class
struct CVATestSampleGenerator
{
	inline CVATestSampleGenerator( )
	{
		voBuffer.push_back( CVASampleBuffer( g_iBlockLength ) ); // single channel
		dFrequency = 567.8f;
	};

	//! Generate a sinusoidal signal
	inline void CalculateMoreSamples( const int nSamples )
	{
		if( voBuffer[0].GetNumSamples( ) != nSamples )
			voBuffer[0] = CVASampleBuffer( nSamples );

		for( int i = 0; i < nSamples; i++ )
		{
			dVal += ( i + 1 ) / double( nSamples ) * 2.0f * 3.1415f * dFrequency;
			voBuffer[0].GetData( )[i] = float( sin( dVal ) );
		}
		dVal = fmod( dVal, 2 * 3.1415f );
	};

	std::vector<CVASampleBuffer> voBuffer;
	double dFrequency;

private:
	double dVal;
};

//! Example sample server class that provides a network audio feed based on an own timeout
class CVATestSampleServer
    : public CVANetworkStreamAudioSignalSource
    , public VistaTicker::AfterPulseFunctor
{
public:
	inline CVATestSampleServer( ) : CVANetworkStreamAudioSignalSource( )
	{
		double dBlockRate        = g_dSampleRate / double( g_iBlockLength );
		int iTimeoutMilliseconds = int( std::round( dBlockRate * 1.0e-3 ) );
		m_pTimeoutTicker         = new VistaTicker( );
		m_pTimeoutTicker->AddTrigger( new VistaTicker::TriggerContext( iTimeoutMilliseconds, true ) );
		m_pTimeoutTicker->StartTicker( );
		m_pTimeoutTicker->SetAfterPulseFunctor( this );
	};

	inline ~CVATestSampleServer( )
	{
		m_pTimeoutTicker->SetAfterPulseFunctor( NULL );
		m_pTimeoutTicker->StopTicker( );
		delete m_pTimeoutTicker;
	};

	// Overload VistaTicker AfterPulseFunctor callback function
	virtual inline bool operator( )( )
	{
		if( !CVANetworkStreamAudioSignalSource::GetIsConnected( ) )
			return false;

		int nNumSamples = CVANetworkStreamAudioSignalSource::GetNumQueuedSamples( );

		// Choose your strategy ...

		if( nNumSamples > 0 )
		{
			m_oSampleGenerator.CalculateMoreSamples( nNumSamples );
			CVANetworkStreamAudioSignalSource::Transmit( m_oSampleGenerator.voBuffer );
		}
		return true;
	};

private:
	VistaTicker* m_pTimeoutTicker;
	CVATestSampleGenerator m_oSampleGenerator;
};

int main( int, char** )
{
	/*
	 *
	 * Use VA and create a network audio stream to connect to this test server.
	 *
	 */

	CVATestSampleServer oSampleServer;

	cout << "Starting server on default bind address and port" << endl;
	bool bSuccess = oSampleServer.Start( ); // Waits for connections on default bind socket localhost:12480
	if( bSuccess )
		cout << "Server started" << endl;

	if( oSampleServer.GetIsConnected( ) )
		cout << "Connection established" << endl;
	else
		cout << "Conection failed" << endl;

	cout << "Sleeping for a while ...";
	VistaTimeUtils::Sleep( int( 10e3 ) );

	return 0;
}
