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

#ifndef IW_VA_SOUND_RECEIVER_DESC
#define IW_VA_SOUND_RECEIVER_DESC

//! This class describes static (unversioned) sound source information
// @todo jst: renamve to CVASoundReceiverDesc
class CVASoundReceiverDesc : public CVAPoolObject
{
public:
	int iID;                                    //!< Sound receiver identifier
	std::string sName;                          //!< Versatile name
	std::atomic<bool> bEnabled;                 //!< Enabled/disabled for rendering
	std::atomic<bool> bMuted;                   //!< Muted status
	std::string sExplicitRendererID;            //!< Explicit renderer identifier, or empty
	std::atomic<bool> bInitPositionOrientation; //!< Flag to indicate if a pose has been set, which is required for spatialisation

	inline void PreRequest( )
	{
		iID                      = -1;
		bEnabled                 = false;
		sName                    = "";
		bInitPositionOrientation = false;
		bMuted                   = false;
	};
};

#endif // IW_VA_SOUND_RECEIVER_DESC
