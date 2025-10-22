#include <Data/VAHRIRDataset.h>
#include <Data/VAHRIRDatasetSH.h>
#include <Utils/VADebug.h>
#include <iostream>
#include <string>

const unsigned int iBlockLength      = 256;
const double dSampleRate             = 44.1e3;
const float fMaxReservedDelaySamples = 2 * iBlockLength;
const float fSimulateSeconds         = 6;
const float fInitialDelaySamples     = 3 * iBlockLength - 1;

void testSH( )
{
	std::cout << " * SH test" << std::endl;

	std::string sFilename   = "test.sh";
	CVAHRIRDatasetSH* pHRIR = new CVAHRIRDatasetSH( sFilename, sFilename, 44.1e3 );
	// CVAHRIRDatasetSH HRIR(sFilename, sFilename, 44.1e3);

	delete pHRIR;
}

int main( )
{
	testSH( );
	return 0;
}
