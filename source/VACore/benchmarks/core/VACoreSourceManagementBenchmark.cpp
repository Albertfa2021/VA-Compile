
#define _USE_MATH_DEFINES

#include <VA.h>
#include <VACoreFactory.h>

#include <stdlib.h> 
#include <iostream>
#include <vector>
#include <math.h>

#include <VistaBase/VistaTimeUtils.h>
#include <ITAStopWatch.h>

#include <stdlib.h>

using namespace std;

CVAStruct GetCoreConfig()
{
	CVAStruct oConfig;

	CVAStruct oSectionDebug;
	oSectionDebug["loglevel"] = 0;
	oConfig["debug"] = oSectionDebug;

	CVAStruct oSectionDriver;
	oSectionDriver["driver"] = "Virtual";
	oSectionDriver["buffersize"] = 64;
	oSectionDriver["outputchannels"] = 2;
	oConfig["audio driver"] = oSectionDriver;

	CVAStruct oDevice1;
	oDevice1["type"] = "Trigger";
	oDevice1["channels"] = "1,2";

	oConfig["OutputDevice:MyHP"] = oDevice1;

	CVAStruct oOutput1;
	oOutput1["devices"] = "MyHP";
	oConfig["Output:MyDesktopHP"] = oOutput1;

	CVAStruct oReproduction1;
	oReproduction1["class"] = "Talkthrough";
	oReproduction1["outputs"] = "MyDesktopHP";
	oConfig["Reproduction:Talkthrough"] = oReproduction1;

	CVAStruct oRenderer1;
	oRenderer1["class"] = "BinauralRealTime";
	oRenderer1["Reproductions"] = "MyTalkthroughHeadphones";
	oConfig["Renderer:BRT_CoreTest"] = oRenderer1;


	return oConfig;
}

CVAStruct timingStruct;
CVAStruct trigger;

int main( int, char** )
{
	IVAInterface* pCore = NULL;
	try
	{
		pCore = VACore::CreateCoreInstance(GetCoreConfig());
		pCore->Initialize();

		int iterations = 10;
		int _parallelSources = 1000;

		double clock = 0;
		double timestep = 64 / 44100.;

		//auto d = pCore->CreateDire

		int receiverID = pCore->CreateSoundReceiver("ITAListener");
		int directivityID = pCore->CreateDirectivityFromFile("C:/dev/VA/VACore/data/ITA_Artificial_Head_5x5_44kHz_128.v17.ir.daff", "HRTF");
		pCore->SetSoundReceiverDirectivity(receiverID, directivityID);
		
		pCore->LockUpdate();

		std::vector< int > viSourceIDs(_parallelSources);

		for (int k = 0; k < _parallelSources; k++)
		{

			viSourceIDs[k] = pCore->CreateSoundSource("BenchmarkSoundSource");
		}
		pCore->UnlockUpdate();

		for (int j = 0; j < iterations; j++)
		{
			pCore->LockUpdate();
			for (int k = 0; k < (_parallelSources); k++)
			{

				float azimuth = ((rand() % 3600) / 1800.0) * M_PI;
				float elevation = ((rand() % 1800) / 1800.0) * M_PI;

				float x = sin(elevation) * cos(azimuth) * 5;
				float y = cos(elevation) * 5;
				float z = sin(azimuth) * sin(elevation) * 5;
				pCore->SetSoundSourcePosition(viSourceIDs[k], VAVec3(x, y, z));
			}
			pCore->UnlockUpdate();
			_sleep(100);
		}

		string audio = pCore->CreateSignalSourceBufferFromFile("X:/Sciebo/2018 MA Lucas Mösch Auralization/input_files/Schluesselbund.wav");
		pCore->SetSignalSourceBufferPlaybackAction(audio, 2);
		pCore->SetSignalSourceBufferLooping(audio, true);

		for (int k = 0; k < _parallelSources; k++)
		{
			pCore->SetSoundSourceSignalSource(viSourceIDs[k], audio);
		}

		_sleep(2000);

		for (int j = 0; j < iterations; j++)
		{
			clock += timestep;
			timingStruct["time"] = clock;
			pCore->CallModule("manualclock", timingStruct );

			trigger["trigger"] = true;
			pCore->CallModule("virtualaudiodevice", trigger);
		}
		
		delete pCore;

	}
	catch( CVAException& e )
	{
		delete pCore;
		cerr << "Error with core " << e.GetErrorCode() << ": " << e << endl;
		return 255;
	}

	return 0;
}
