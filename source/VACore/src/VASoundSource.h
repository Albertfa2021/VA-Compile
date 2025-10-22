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
 *    Datei:			VASoundSource.h
 *
 *    Zweck:			Datenklasse zur Beschreibung des Zustands einer Schallquelle
 *
 *    Autor(en):		Frank Wefers (Frank.Wefers@akustik.rwth-aachen.de)
 *
 *  ---------------------------------------------------------------------------------
 */

// $Id: VASoundSource.h 2729 2012-06-26 13:23:36Z fwefers $

#ifndef __VA_SOUNDSOURCE_H__
#define __VA_SOUNDSOURCE_H__

#include <RG_Vector.h>
#include <string>

// Datenklasse welche den Zustand einer Schallquelle darstellt
class CVASoundSourceState
{
public:
	RG_Vector vPos, vView, vUp; // Position und Orientierung
	bool bMuted;                // Stummgeschaltet?
	int iAuralizationMode;      // Auralisierungsmodus
	double dVolume;             // Volume
};

// Klasse welche Schallquellen darstellt
class CVASoundSource
{
public:
	CVASoundSource( );
	virtual ~CVASoundSource( );

	int getID( ) const;
	void setID( int iID );

	std::string getName( ) const;
	void setName( const std::string& sName );

private:
	int m_iID;
	std::string m_sName;
};

#endif // __VA_SOUNDSOURCE_H__
