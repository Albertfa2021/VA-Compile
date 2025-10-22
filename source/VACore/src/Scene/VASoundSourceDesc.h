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

#ifndef IW_VACORE_SOUNDSOURCEDESC
#define IW_VACORE_SOUNDSOURCEDESC

#include <VAAudioSignalSource.h>
#include <VAPoolObject.h>
#include <atomic>

class ITASampleBuffer;
class ITASampleFrame;

#include <atomic>


//! This class describes a static unversioned sound source
class CVASoundSourceDesc : public CVAPoolObject
{
public:
	int iID;           //!< Identifier
	std::string sName; //!< Verbose name (for debugging)

	std::atomic<bool> bMuted;                   //!< Muting switch (jst: Berechnungen sollen trotzdem durchgeführt werden? bActive dazu?)
	std::atomic<bool> bEnabled;                 //!< Enabled/disable from rendering
	std::atomic<bool> bInitPositionOrientation; //!< Flag to indicate if a pose has been set, which is required for spatialisation

	std::atomic<IVAAudioSignalSource*> pSignalSource;           //!< Zugeordnete Signalquelle
	std::atomic<const ITASampleFrame*> psfSignalSourceInputBuf; //!< Puffer der Eingangsdaten der Signalquelle

	float fSoundPower;
	std::string sExplicitRendererID; //!< Explicit renderer for this sound source (empty = all)

	inline void PreRequest( )
	{
		iID   = -1;
		sName = "";

		bMuted                   = false;
		bEnabled                 = true;
		bInitPositionOrientation = false;

		psfSignalSourceInputBuf = NULL;
		pSignalSource           = NULL;

		fSoundPower         = 0.0f;
		sExplicitRendererID = "";
	};
};

#endif // IW_VACORE_SOUNDSOURCEDESC
