/*
 *  --------------------------------------------------------------------------------------------
 *
 *    VVV        VVV A           Virtual Acoustics (VA) | http://www.virtualacoustics.org
 *     VVV      VVV AAA          Licensed under the Apache License, Version 2.0
 *      VVV    VVV   AAA
 *       VVV  VVV     AAA        Copyright 2015-2022
 *        VVVVVV       AAA       Institute of Technical Acoustics (ITA)
 *         VVVV         AAA      RWTH Aachen University
 *
 *  --------------------------------------------------------------------------------------------
 */

#ifndef IW_VACORE_CORETHREAD
#define IW_VACORE_CORETHREAD

#include <VistaInterProcComm/Concurrency/VistaThreadEvent.h>
#include <VistaInterProcComm/Concurrency/VistaThreadLoop.h>
#include <atomic>

class CVACoreImpl;

class CVACoreThread : public VistaThreadLoop
{
public:
	CVACoreThread( CVACoreImpl* pParent );
	~CVACoreThread( );

	// Thread anschubsen (nach Aktion)
	void Trigger( );

	// Versucht den Thread anzuhalten
	bool TryBreak( );

	// Thread anhalten
	void Break( );

	// Thread weiter fortsetzen
	void Continue( );

	// -= Definition der virtuellen Methoden in VistaThread =-

	// void PreLoop();
	// void PostLoop();
	bool LoopBody( );

private:
	CVACoreImpl* m_pParent;
	VistaThreadEvent m_evTrigger;
	VistaThreadEvent m_evPaused;
	std::atomic<int> m_iTriggerCnt;
	std::atomic<bool> m_bPaused;
	std::atomic<bool> m_bStop;
	std::atomic<bool> m_bReadyForPause;
};

#endif // IW_VACORE_CORETHREAD
