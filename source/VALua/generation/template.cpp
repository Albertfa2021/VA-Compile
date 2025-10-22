#ifndef __VACVALuaCoreObject_H__
#define __VACVALuaCoreObject_H__

#include "luna.h"

#include <VACore.h>
#include <VACoreVersion.h>
#include <VAException.h>

// Vorwärtsdeklarationen
class IVACore;

/**
 * Diese abstrakte Klasse realisiert ein Lua-Objekt, welches die gleiche Schnittstelle
 * anbietet wie IVACore und Methoden-Aufrufe aus Lua an eine Instanz des IVACore delegiert.
 * Sie fungiert also als Adapterklasse zwischen Lua und C++.
 */

class CVALuaCoreObject {
public:
	static const char className[];						// Klassenname in Lua
	static Luna<CVALuaCoreObject>::RegType methods[];	// Methoden-Bindings

	// Konstruktor. Nimmt als Argument den Pointer zum Core vom Lua-Stack
	CVALuaCoreObject(lua_State *L);
	~CVALuaCoreObject();

	int GetVersionInfo(lua_State *L);
	int GetState(lua_State *L);
	int Reset(lua_State *L);

/*** GENERATE_METHOD_DECLARATIONS ***/
	
private:
	IVACore* m_pCore;
};

const char CVALuaCoreObject::className[] = "VACore";

#define method(class, name) {#name, &class::name}
Luna<CVALuaCoreObject>::RegType CVALuaCoreObject::methods[] = {
/*** GENERATE_METHOD_REGISTRATIONS ***/
	{0,0}
};

// Makros für Try-/Catch-Blöcke in den Funktionsumsetzern (Erspart Tipparbeit)
// Hierbei werden C++ Exceptions abgefangen und in Lua-Errors umgesetzt
#define VASHELL_TRY(); try {
#define VASHELL_CATCH(); } \
catch (CVAException& e) { luaL_error(L, e.ToString().c_str()); return 1; } \
catch (...) { luaL_error(L, "Unknown error occured"); return 1; }

/*** GENERATE_METHOD_IMPLEMENTATIONS ***/

#endif // __VACVALuaCoreObject_H__
