/*
 *  --------------------------------------------------------------------------------------------
 *
 *    VVV        VVV A           Virtual Acoustics (VA) | http://www.virtualacoustics.org
 *     VVV      VVV AAA          Licensed under the Apache License, Version 2.0
 *      VVV    VVV   AAA
 *       VVV  VVV     AAA        Copyright 2015-2021
 *        VVVVVV       AAA       Institute of Technical Acoustics (ITA)
 *         VVVV         AAA      RWTH Aachen University
 *
 *  --------------------------------------------------------------------------------------------
 */

#ifndef IW_VALUA_SHELL
#define IW_VALUA_SHELL

#include <VALuaDefs.h>

#include <iostream>
#include <ostream>
#include <string>

// Vorwärtsdeklarationen
class IVACore;

/**
 * Diese abstrakte Klasse definiert die Schnittstelle für eine Shell
 * welche einen Lua-Interpreter beherbergt und es erlaubt einen VACore
 * mittels Lua anzusprechen und zu steuern.
 *
 * Konkrete Instanzen der Klasse müssen mit der Factory method erzeugt werden.
 */

class VALUA_API IVALuaShell {
public:
	//! Factory method
	static IVALuaShell* Create();

	//! Version
	static std::string GetVersionStr();

	virtual ~IVALuaShell();

	// Kern-Instanz setzen
	virtual void SetCoreInstance(IVACore* pVACore)=0;

	// Ausgabe-Stream setzen
	virtual void SetOutputStream(std::ostream* posStdOut)=0;

	// Fehler-Stream setzen
	virtual void SetErrorStream(std::ostream* posStdErr)=0;

	// Status zurückgeben
	virtual int GetStatus() const=0;

	// Status setzen (dieser kann in Lua abgefragt werden
	// und zur externen Steuerung eines Skripts verwendet werden)
	virtual void SetStatus(int iStatus)=0;

	// Ein Lua-Skript ausführen (von Datei)
	virtual void ExecuteLuaScript(const std::string& sFilename)=0;

	// Eine Eingabezeile verarbeiten
	virtual void HandleInputLine(const std::string& sLine)=0;
};

#endif // IW_VALUA_SHELL