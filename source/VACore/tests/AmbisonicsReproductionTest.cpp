#include<stdio.h>
#include<vector>

#include<VistaBase/VistaTimeUtils.h>

#include<ITANumericUtils.h>

#include<VA.h>
#include<VACore.h>
#include"VAInterface.h"

using namespace std;

IVAInterface* va = nullptr;
CVAStruct timer;
CVAStruct trigger;

int main(int, char**)
{
	int numsteps = 2411;

	double clock = 0;
	double timestep = 64 / 44100.;

	try
	{
		va = VACore::CreateCoreInstance("E:\\Work\\_dev\\ITASuite\\VA\\VACore\\tests\\AmbisonicsReproductionTest.ini");
		va->Initialize();
		va->AddSearchPath("AmbisonicsReproductionTest");

		std::string ssID = va->CreateSignalSourceBufferFromFile("WelcomeToVA.wav");
		va->SetSignalSourceBufferPlaybackAction(ssID, 2);
		va->SetSignalSourceBufferLooping(ssID, false);
		
		int sourceID = va->CreateSoundSource("Vss1");
		va->SetSoundSourceSignalSource(sourceID, ssID);
		va->SetSoundSourcePosition(sourceID, VAVec3(0, 0, -2));
		va->SetSoundSourceOrientationVU(sourceID, VAVec3(0, 1, 0), VAVec3(0, 0, 1));

		int receiverID = va->CreateSoundReceiver("ITAListener");
		int directivityID = va->CreateDirectivityFromFile("ITA_Artificial_Head_5x5_44kHz_128.v17.ir.daff", "HRTF");
		va->SetSoundReceiverDirectivity(receiverID, directivityID);
		va->SetSoundReceiverPosition(receiverID, VAVec3(0, 0, 0));


		va->SetCoreClock(0);

		for (int i = 0; i < numsteps; i++)
		{
			clock += timestep;
			timer["time"] = clock;
			va->CallModule("manualclock", timer);

			trigger["trigger"] = true;
			va->CallModule("virtualaudiodevice", trigger);

			std::cout << clock << std::endl;
		}

		va->Finalize();

	}
	catch (CVAException& e)
	{
		cerr << "Error: " << e << endl;
		int iErrorCode = e.GetErrorCode();
		delete va;

		return iErrorCode;
	}

	delete va;
	return 0;
}