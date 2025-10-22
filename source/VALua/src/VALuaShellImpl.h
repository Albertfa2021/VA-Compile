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

#ifndef IW_VALUA_SHELL_IMPL
#define IW_VALUA_SHELL_IMPL

#include "VALuaShell.h"

extern "C"
{
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

class CVALuaShellImpl : public IVALuaShell
{
public:
	/*
	 *  pCore  VA-Kern den die Shell steuert
	 *  stdout Ausgabestream
	 *  stderr Ausgabestream für Fehler
	 */
	CVALuaShellImpl();

	~CVALuaShellImpl();

	// TODO: Wie bauen?
	static IVACore* GetCoreInstance();

	void SetCoreInstance( IVACore* pVACore );
	void SetOutputStream( std::ostream* posStdOut );
	void SetErrorStream( std::ostream* posStdErr );

	int GetStatus() const;
	void SetStatus( int iStatus );

	// Ein Lua-Skript ausführen (von Datei)
	void ExecuteLuaScript( const std::string& sFilename );

	// Eine Eingabezeile verarbeiten
	void HandleInputLine( const std::string& sLine );

private:
	std::ostream* m_posStdOut;
	std::ostream* m_posStdErr;
	lua_State* L;	// Lua VM

	// Nimmt einen Fehler vom Stack der Lua-VM und gibt ihn auf dem Error-Stream aus
	void report_errors( lua_State* L, int status );
};

#endif // IW_VALUA_SHELL_IMPL