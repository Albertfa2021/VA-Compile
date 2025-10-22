#include<stdio.h>
#include <algorithm>
#include<vector>

#include<VistaBase/VistaTimeUtils.h>
#include<VistaBase/VistaQuaternion.h>

#include<ITAConstants.h>
#include<ITANumericUtils.h>

#include<VA.h>
#include<VACore.h>
#include"VAInterface.h"

using namespace std;
using namespace ITAConstants;

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

		va = VACore::CreateCoreInstance("E:\\Work\\_dev\\ITASuite\\VA\\VACore\\tests\\VBAPFreefieldRendererTest.ini");
		va->Initialize( );

		std::string ssID = va->CreateSignalSourceBufferFromFile("WelcomeToVA.wav");
		va->SetSignalSourceBufferPlaybackAction(ssID, 2);
		va->SetSignalSourceBufferLooping(ssID, false);
		
		int sourceID = va->CreateSoundSource("Vss1");
		va->SetSoundSourceSignalSource(sourceID, ssID);
		va->SetSoundSourcePosition(sourceID, VAVec3(0, 0, -2));
		va->SetSoundSourceOrientationVU(sourceID, VAVec3(0, 0, 1), VAVec3(0, 1, 0));

		int receiverID = va->CreateSoundReceiver("ITAListener");
		int directivityID = va->CreateDirectivityFromFile("ITA_Artificial_Head_5x5_44kHz_128.v17.ir.daff", "HRTF");
		va->SetSoundReceiverDirectivity(receiverID, directivityID);
		va->SetSoundReceiverPosition(receiverID, VAVec3(0, 0, 0));

		//va->SetSoundReceiverRealWorldPositionOrientationVU(receiverID, VAVec3(0, 0.1, 0), VAVec3(0, 0, -1), VAVec3(0, 1, 0));

		va->SetCoreClock(0);

		float  alpha;
		VAVec3 pos;
		VistaVector3D v3pos, v3newPos;
		VistaQuaternion q(VistaAxisAndAngle(VistaVector3D(1, 1, 0), 2.f * PI_F/180.0f));

		for (int i = 0; i < numsteps; i++)
		{
			pos = va->GetSoundSourcePosition(sourceID);
			alpha = atan2f(-pos.x, -pos.z) * 180.f / PI_F;
			cout << "alpha = " << alpha << endl;

			clock += timestep;

			timer["time"] = clock;
			va->CallModule("manualclock", timer);

			trigger["trigger"] = true;
			va->CallModule("virtualaudiodevice", trigger);

			v3pos = VistaVector3D(pos.comp);
			v3newPos = q.Rotate(v3pos);
			pos = VAVec3(v3newPos[0], v3newPos[1], v3newPos[2]);

			va->SetSoundSourcePosition(sourceID, pos);
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