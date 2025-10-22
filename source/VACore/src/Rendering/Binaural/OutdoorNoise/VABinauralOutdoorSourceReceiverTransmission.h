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

#ifndef IW_VACORE_BINAURAL_OUTDOOR_SOURCE_RECEIVER_TRANSMISSION
#define IW_VACORE_BINAURAL_OUTDOOR_SOURCE_RECEIVER_TRANSMISSION

// VA includes
#include <VA.h>
//#include <VAPoolObject.h>
#include "../Clustering/Receiver/VABinauralClusteringReceiver.h"
#include "VABinauralOutdoorSource.h"
#include "VABinauralOutdoorWaveFront.h"

// ITA includes

// STL
#include <map>
#include <string.h>

//! Represents ALL wave fronts emitted from a sound source for a specific receiver in an outdoor scenario
class CVABinauralOutdoorSourceReceiverTransmission
{
public:
	CVABinauralOutdoorSourceReceiverTransmission( CVABinauralOutdoorSource* pSource, CVABinauralClusteringReceiver* pReceiver );
	~CVABinauralOutdoorSourceReceiverTransmission( );

	inline CVABinauralOutdoorSource* GetSource( ) { return m_pSource; };
	inline CVABinauralClusteringReceiver* GetReceiver( ) { return m_pReceiver; };

	//! Sets the source for this object and all connected wavefronts
	void SetSource( CVABinauralOutdoorSource* source );
	//! Sets the receiver for this object and all connected wavefronts
	void SetReceiver( CVABinauralClusteringReceiver* sound_receiver );

	void AddWavefront( std::string sID, CVABinauralOutdoorWaveFront* pWavefront );
	// void RemoveWavefront( CVABinauralOutdoorWaveFront* pWavefront );
	//! If a wavefront with the given ID exists, it is removed
	void RemoveWavefront( const std::string& sID );
	//! Returns the wavefront with the given identifier. Returns nullptr, if wavefront is not found.
	CVABinauralOutdoorWaveFront* GetWavefront( const std::string& sID );
	//! Returns a const reference on the wavefront map
	inline const std::map<std::string, CVABinauralOutdoorWaveFront*>& GetWavefrontMap( ) { return m_mSoundPaths; };

	void UpdateFilterPropertyHistories( double dTime );

	bool IsEqual( const CVABinauralOutdoorSource* pSource, const CVABinauralClusteringReceiver* pReceiver );

private:
	CVABinauralOutdoorSource* m_pSource;
	CVABinauralClusteringReceiver* m_pReceiver;

	std::map<std::string, CVABinauralOutdoorWaveFront*> m_mSoundPaths;
};

#endif // IW_VACORE_BINAURAL_OUTDOOR_SOURCE_RECEIVER_PAIR
