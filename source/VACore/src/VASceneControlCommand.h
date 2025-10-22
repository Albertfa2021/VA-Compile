/*
 *
 *    VVV        VVV A
 *     VVV      VVV AAA        Virtual Acoustics
 *      VVV    VVV   AAA       Real-time auralisation for virtual reality
 *       VVV  VVV     AAA
 *        VVVVVV       AAA     (c) Copyright Institut für Technische Akustik (ITA)
 *         VVVV         AAA        RWTH Aachen (http://www.akustik.rwth-aachen.de)
 *
 *  ---------------------------------------------------------------------------------
 *
 *    Datei:			VACoreImpl.h
 *
 *    Zweck:			Implementierungsklasse des VA-Kerns
 *
 *    Autor(en):		Frank Wefers (Frank.Wefers@akustik.rwth-aachen.de
 *
 *  ---------------------------------------------------------------------------------
 */

// $Id: VASceneControlCommand.h 2729 2012-06-26 13:23:36Z fwefers $

#ifndef __VA_COMMAND_H__
#define __VA_COMMAND_H__

class CVASceneControlCommand
{
public:
	// Command types
	enum
	{
		VA_COMMAND_SETSOUNDSOURCEAURALIZATIONMODE = 0,
		VA_COMMAND_SETSOUNDSOURCEVOLUME,
		VA_COMMAND_SETSOUNDSOURCEMUTED,
	};

	// Command type
	int iCommand;

	// Command parameters
	int iObjectID;
	int iParamID;
	std::string sFilename;
	std::string sName;
	bool bValue;
	int iValue;
	double dValue;
	double dPosX, dPosY, dPosZ, dViewX, dViewY, dViewZ, dUpX, dUpY, dUpZ, dYaw, dPitch, dRoll;

	CVACommand( );
	virtual ~CVACommand( );

	//! Führt den gespeicherten Befehl auf einem Dienstleister aus
	void Exec( IVASceneControlInterface* pServiceProvider );

	// --= Befehle =-----------------------------------------------

	void SetSoundSourceAuralizationMode( int iSoundSourceID, int iAuralizationMode );
	void SetSoundSourceVolume( int iSoundSourceID, double dGain );
	void SetSoundSourceMuted( int iSoundSourceID, bool bMuted );

	void SetSoundSourcePositionOrientationVU( int iSoundSourceID, double px, double py, double pz, double vx, double vy, double vz, double ux, double uy, double uz );
};

#endif // __VA_COMMAND_H__
