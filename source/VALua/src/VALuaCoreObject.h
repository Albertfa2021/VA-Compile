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

#ifndef IW_VALUA_CORE_OBJECT
#define IW_VALUA_CORE_OBJECT

#include "luna.h"

#include <VA.h>

// Forwards
class IVAInterface;

/**
 * This abstract class realises a Lua object that provides the same interface as IVACore.
 * Calls are delegated to a real IVACore in C++.
 */
class CVALuaCoreObject
{
public:
	static const char className[];						// Klassenname in Lua
	static Luna<CVALuaCoreObject>::RegType methods[];	// Methoden-Bindings

	// Konstruktor. Nimmt als Argument den Pointer zum Core vom Lua-Stack
	CVALuaCoreObject( lua_State *L );
	~CVALuaCoreObject();

	int GetVersionInfo( lua_State *L );
	int GetState( lua_State *L );
	int Reset( lua_State *L );

	/* +----------------------------------------------------------+ *
	 * |                                                          | *
	 * |   Module interface                                       | *
	 * |                                                          | *
	 * +----------------------------------------------------------+ */

	int EnumerateModules( lua_State *L );
	int CallModule( lua_State *L );

	/* +----------------------------------------------------------+ *
	 * |                                                          | *
	 * |   Richtcharakteristiken (directivities)                  | *
	 * |                                                          | *
	 * +----------------------------------------------------------+ */

	int LoadDirectivity( lua_State *L );
	int FreeDirectivity( lua_State *L );
	// TODO: int GetDirectivityInfo(lua_State *L);
	int GetDirectivityInfos( lua_State *L );

	/* +----------------------------------------------------------+ *
	 * |                                                          | *
	 * |   Head-related impulse responses (HRIRs)                 | *
	 * |                                                          | *
	 * +----------------------------------------------------------+ */

	int LoadHRIRDataset( lua_State *L );
	int FreeHRIRDataset( lua_State *L );
	// TODO: int GetHRIRInfo(lua_State *L);
	int GetHRIRInfos( lua_State *L );

	/* +----------------------------------------------------------+ *
	 * |                                                          | *
	 * |   Sounds                                                 | *
	 * |                                                          | *
	 * +----------------------------------------------------------+ */

	int LoadSound( lua_State *L );
	int FreeSound( lua_State *L );
	int PlaySound( lua_State *L );

	/* +----------------------------------------------------------+ *
	 * |                                                          | *
	 * |   Globale Funktionen                                     | *
	 * |                                                          | *
	 * +----------------------------------------------------------+ */

	int GetInputGain( lua_State *L );
	int SetInputGain( lua_State *L );
	int GetOutputGain( lua_State *L );
	int SetOutputGain( lua_State *L );
	int IsOutputMuted( lua_State *L );
	int SetOutputMuted( lua_State *L );
	int GetGlobalAuralizationMode( lua_State *L );
	int SetGlobalAuralizationMode( lua_State *L );
	int GetActiveListener( lua_State *L );
	int SetActiveListener( lua_State *L );
	int GetVolumeStrDecibel( lua_State *L );
	int GetCoreClock( lua_State *L );
	int SetCoreClock( lua_State *L );

	/* +----------------------------------------------------------+ *
	 * |                                                          | *
	 * |   Rendering & Reproduction                               | *
	 * |                                                          | *
	 * +----------------------------------------------------------+ */

	int GetRenderingModules( lua_State* );
	int SetRenderingModuleMuted( lua_State *L );
	int IsRenderingModuleMuted( lua_State *L );
	int SetRenderingModuleGain( lua_State *L );
	int GetRenderingModuleGain( lua_State *L );

	int GetReproductionModules( lua_State* );
	int SetReproductionModuleMuted( lua_State *L );
	int IsReproductionModuleMuted( lua_State *L );
	int SetReproductionModuleGain( lua_State *L );
	int GetReproductionModuleGain( lua_State *L );


	/* +----------------------------------------------------------+ *
	 * |                                                          | *
	 * |   Signalquellen                                          | *
	 * |                                                          | *
	 * +----------------------------------------------------------| */

	int CreateAudiofileSignalSource( lua_State *L );
	int CreateSequencerSignalSource( lua_State *L );
	int CreateNetstreamSignalSource( lua_State *L );
	int CreateEngineSignalSource( lua_State* );
	int CreateMachineSignalSource( lua_State* );
	int DeleteSignalSource( lua_State *L );
	int GetAudiofileSignalSourcePlaybackState( lua_State* );
	int SetAudiofileSignalSourcePlaybackAction( lua_State* );
	int SetAudiofileSignalSourcePlaybackPosition( lua_State* );
	int SetAudiofileSignalSourceIsLooping( lua_State* );
	int GetAudiofileSignalSourceIsLooping( lua_State* );

	int StartMachineSignalSource( lua_State* );
	int HaltMachineSignalSource( lua_State* );
	int GetMachineSignalSourceStateStr( lua_State* );
	int GetMachineSignalSourceSpeed( lua_State* );
	int SetMachineSignalSourceSpeed( lua_State* );
	int SetMachineSignalSourceStartFile( lua_State* );
	int SetMachineSignalSourceIdleFile( lua_State* );
	int SetMachineSignalSourceStopFile( lua_State* );

	int SetSignalSourceParameters( lua_State* );
	int GetSignalSourceParameters( lua_State* );

	/* +----------------------------------------------------------+ *
	 * |                                                          | *
	 * |   Schallquellen                                          | *
	 * |                                                          | *
	 * +----------------------------------------------------------+ */

	int CreateSoundSource( lua_State *L );
	int DeleteSoundSource( lua_State *L );
	int GetSoundSourceName( lua_State *L );
	int SetSoundSourceName( lua_State *L );
	int GetSoundSourceSignalSource( lua_State *L );
	int SetSoundSourceSignalSource( lua_State *L );
	int RemoveSoundSourceSignalSource( lua_State* );
	int GetSoundSourceAuralizationMode( lua_State *L );
	int SetSoundSourceAuralizationMode( lua_State *L );
	int GetSoundSourceParameters( lua_State* );
	int SetSoundSourceParameters( lua_State* );
	int GetSoundSourceDirectivity( lua_State *L );
	int SetSoundSourceDirectivity( lua_State *L );
	int GetSoundSourceVolume( lua_State *L );
	int SetSoundSourceVolume( lua_State *L );
	int SetSoundSourceMuted( lua_State *L );
	int GetSoundSourcePosition( lua_State *L );
	int SetSoundSourcePosition( lua_State *L );
	int SetSoundSourceOrientationYPR( lua_State *L );
	int GetSoundSourceOrientationYPR( lua_State *L );
	int SetSoundSourceOrientationVU( lua_State *L );
	int GetSoundSourceOrientationVU( lua_State *L );
	int SetSoundSourcePositionOrientationYPR( lua_State *L );
	int GetSoundSourcePositionOrientationYPR( lua_State *L );
	int SetSoundSourcePositionOrientationVU( lua_State *L );
	int GetSoundSourcePositionOrientationVU( lua_State *L );
	int SetSoundSourcePositionOrientationVelocityYPR( lua_State *L );
	int GetSoundSourcePositionOrientationVelocityYPR( lua_State *L );
	int SetSoundSourcePositionOrientationVelocityVU( lua_State *L );
	int GetSoundSourcePositionOrientationVelocityVU( lua_State *L );

	int IsSoundSourceMuted( lua_State *L );

	int AddSoundPlayback( lua_State *L );
	int RemoveSoundPlayback( lua_State *L );

	/* +----------------------------------------------------------+ *
	 * |                                                          | *
	 * |   Listener                                               | *
	 * |                                                          | *
	 * +----------------------------------------------------------| */

	int CreateListener( lua_State *L );
	int DeleteListener( lua_State *L );
	int GetListenerName( lua_State *L );
	int SetListenerName( lua_State *L );
	int SetListenerAuralizationMode( lua_State *L );
	int GetListenerAuralizationMode( lua_State *L );
	int GetListenerParameters( lua_State* );
	int SetListenerParameters( lua_State* );
	int GetListenerHRIRDataset( lua_State *L );
	int SetListenerHRIRDataset( lua_State *L );
	int SetListenerPosition( lua_State *L );
	int GetListenerPosition( lua_State *L );
	int SetListenerOrientationYPR( lua_State *L );
	int GetListenerOrientationYPR( lua_State *L );
	int SetListenerOrientationVU( lua_State *L );
	int GetListenerOrientationVU( lua_State *L );
	int SetListenerPositionOrientationYPR( lua_State *L );
	int GetListenerPositionOrientationYPR( lua_State *L );
	int SetListenerPositionOrientationVU( lua_State *L );
	int GetListenerPositionOrientationVU( lua_State *L );
	int SetListenerPositionOrientationVelocityYPR( lua_State *L );
	int GetListenerPositionOrientationVelocityYPR( lua_State *L );
	int SetListenerPositionOrientationVelocityVU( lua_State *L );
	int GetListenerPositionOrientationVelocityVU( lua_State *L );
	int GetListenerRealWorldHeadPositionOrientationVU( lua_State *L );
	int SetListenerRealWorldHeadPositionOrientationVU( lua_State *L );

	/* +----------------------------------------------------------+ *
	 * |                                                          | *
	 * |   Szene                                                  | *
	 * |                                                          | *
	 * +----------------------------------------------------------| */

	int LoadScene( lua_State *L );
	int IsSceneLoaded( lua_State *L );
	//int GetSceneInfo(lua_State *L);
	int GetPortalName( lua_State *L );
	int SetPortalName( lua_State *L );
	int GetPortalState( lua_State *L );
	int SetPortalState( lua_State *L );

	/* +----------------------------------------------------------+ *
	 * |                                                          | *
	 * |   sonstiges                                              | *
	 * |                                                          | *
	 * +----------------------------------------------------------| */
	int LockScene( lua_State *L );
	int UnlockScene( lua_State *L );
	int IsSceneLocked( lua_State *L );

private:
	IVAInterface* m_pCore;
};

const char CVALuaCoreObject::className[] = "VACore";

#define method(class, name) {#name, &class::name}
Luna<CVALuaCoreObject>::RegType CVALuaCoreObject::methods[] = {
	method( CVALuaCoreObject, GetVersionInfo ),
	method( CVALuaCoreObject, GetState ),
	method( CVALuaCoreObject, Reset ),

	method( CVALuaCoreObject, EnumerateModules ),
	method( CVALuaCoreObject, CallModule ),

	method( CVALuaCoreObject, LoadDirectivity ),
	method( CVALuaCoreObject, FreeDirectivity ),
	method( CVALuaCoreObject, GetDirectivityInfos ),


	method( CVALuaCoreObject, LoadHRIRDataset ),
	method( CVALuaCoreObject, FreeHRIRDataset ),
	method( CVALuaCoreObject, GetHRIRInfos ),

	method( CVALuaCoreObject, LoadSound ),
	method( CVALuaCoreObject, FreeSound ),
	method( CVALuaCoreObject, PlaySound ),

	method( CVALuaCoreObject, CreateSoundSource ),
	method( CVALuaCoreObject, DeleteSoundSource ),
	method( CVALuaCoreObject, GetSoundSourceName ),
	method( CVALuaCoreObject, SetSoundSourceName ),
	method( CVALuaCoreObject, SetSoundSourcePositionOrientationVU ),
	method( CVALuaCoreObject, GetSoundSourcePositionOrientationVU ),
	method( CVALuaCoreObject, SetSoundSourcePositionOrientationVelocityVU ),
	method( CVALuaCoreObject, GetSoundSourcePositionOrientationVelocityVU ),

	method( CVALuaCoreObject, GetInputGain ),
	method( CVALuaCoreObject, SetInputGain ),
	method( CVALuaCoreObject, GetOutputGain ),
	method( CVALuaCoreObject, SetOutputGain ),
	method( CVALuaCoreObject, IsOutputMuted ),
	method( CVALuaCoreObject, SetOutputMuted ),
	method( CVALuaCoreObject, GetGlobalAuralizationMode ),
	method( CVALuaCoreObject, GetVolumeStrDecibel ),
	method( CVALuaCoreObject, SetGlobalAuralizationMode ),
	method( CVALuaCoreObject, GetActiveListener ),
	method( CVALuaCoreObject, SetActiveListener ),
	method( CVALuaCoreObject, GetCoreClock ),
	method( CVALuaCoreObject, SetCoreClock ),

	method( CVALuaCoreObject, CreateAudiofileSignalSource ),
	method( CVALuaCoreObject, CreateSequencerSignalSource ),
	method( CVALuaCoreObject, CreateNetstreamSignalSource ),
	method( CVALuaCoreObject, CreateEngineSignalSource ),
	method( CVALuaCoreObject, CreateMachineSignalSource ),
	method( CVALuaCoreObject, DeleteSignalSource ),
	method( CVALuaCoreObject, GetAudiofileSignalSourcePlaybackState ),
	method( CVALuaCoreObject, SetAudiofileSignalSourcePlaybackAction ),
	method( CVALuaCoreObject, SetAudiofileSignalSourceIsLooping ),
	method( CVALuaCoreObject, GetAudiofileSignalSourceIsLooping ),
	method( CVALuaCoreObject, SetAudiofileSignalSourcePlaybackPosition ),
	method( CVALuaCoreObject, AddSoundPlayback ),
	method( CVALuaCoreObject, RemoveSoundPlayback ),
	method( CVALuaCoreObject, SetSignalSourceParameters ),
	method( CVALuaCoreObject, GetSignalSourceParameters ),

	method( CVALuaCoreObject, StartMachineSignalSource ),
	method( CVALuaCoreObject, HaltMachineSignalSource ),
	method( CVALuaCoreObject, GetMachineSignalSourceStateStr ),
	method( CVALuaCoreObject, GetMachineSignalSourceSpeed ),
	method( CVALuaCoreObject, SetMachineSignalSourceSpeed ),
	method( CVALuaCoreObject, SetMachineSignalSourceStartFile ),
	method( CVALuaCoreObject, SetMachineSignalSourceIdleFile ),
	method( CVALuaCoreObject, SetMachineSignalSourceStopFile ),

	method( CVALuaCoreObject, GetSoundSourceSignalSource ),
	method( CVALuaCoreObject, SetSoundSourceSignalSource ),
	method( CVALuaCoreObject, GetSoundSourceAuralizationMode ),
	method( CVALuaCoreObject, SetSoundSourceAuralizationMode ),
	method( CVALuaCoreObject, GetSoundSourceDirectivity ),
	method( CVALuaCoreObject, SetSoundSourceDirectivity ),
	method( CVALuaCoreObject, GetSoundSourceVolume ),
	method( CVALuaCoreObject, SetSoundSourceVolume ),
	method( CVALuaCoreObject, SetSoundSourceMuted ),
	method( CVALuaCoreObject, GetSoundSourcePosition ),
	method( CVALuaCoreObject, SetSoundSourcePosition ),
	method( CVALuaCoreObject, SetSoundSourceOrientationYPR ),
	method( CVALuaCoreObject, GetSoundSourceOrientationYPR ),
	method( CVALuaCoreObject, SetSoundSourceOrientationVU ),
	method( CVALuaCoreObject, GetSoundSourceOrientationVU ),
	method( CVALuaCoreObject, SetSoundSourcePositionOrientationYPR ),
	method( CVALuaCoreObject, GetSoundSourcePositionOrientationYPR ),
	method( CVALuaCoreObject, SetSoundSourcePositionOrientationVelocityYPR ),
	method( CVALuaCoreObject, GetSoundSourcePositionOrientationVelocityYPR ),
	method( CVALuaCoreObject, IsSoundSourceMuted ),

	method( CVALuaCoreObject, CreateListener ),
	method( CVALuaCoreObject, DeleteListener ),
	method( CVALuaCoreObject, GetListenerName ),
	method( CVALuaCoreObject, SetListenerName ),
	method( CVALuaCoreObject, GetListenerAuralizationMode ),
	method( CVALuaCoreObject, SetListenerAuralizationMode ),
	method( CVALuaCoreObject, GetListenerHRIRDataset ),
	method( CVALuaCoreObject, SetListenerHRIRDataset ),
	method( CVALuaCoreObject, SetListenerPosition ),
	method( CVALuaCoreObject, GetListenerPosition ),
	method( CVALuaCoreObject, SetListenerOrientationYPR ),
	method( CVALuaCoreObject, GetListenerOrientationYPR ),
	method( CVALuaCoreObject, SetListenerOrientationVU ),
	method( CVALuaCoreObject, GetListenerOrientationVU ),
	method( CVALuaCoreObject, SetListenerPositionOrientationYPR ),
	method( CVALuaCoreObject, GetListenerPositionOrientationYPR ),
	method( CVALuaCoreObject, SetListenerPositionOrientationVU ),
	method( CVALuaCoreObject, GetListenerPositionOrientationVU ),
	method( CVALuaCoreObject, SetListenerPositionOrientationVelocityYPR ),
	method( CVALuaCoreObject, GetListenerPositionOrientationVelocityYPR ),
	method( CVALuaCoreObject, SetListenerPositionOrientationVelocityVU ),
	method( CVALuaCoreObject, GetListenerPositionOrientationVelocityVU ),
	method( CVALuaCoreObject, GetListenerRealWorldHeadPositionOrientationVU ),
	method( CVALuaCoreObject, SetListenerRealWorldHeadPositionOrientationVU ),

	method( CVALuaCoreObject, LoadScene ),
	method( CVALuaCoreObject, IsSceneLoaded ),
	//method(CVALuaCoreObject, GetSceneInfo),
	method( CVALuaCoreObject, SetPortalName ),
	method( CVALuaCoreObject, GetPortalName ),
	method( CVALuaCoreObject, GetPortalState ),
	method( CVALuaCoreObject, SetPortalState ),

	method( CVALuaCoreObject, SetRenderingModuleGain ),
	method( CVALuaCoreObject, SetRenderingModuleMuted ),
	method( CVALuaCoreObject, GetRenderingModuleGain ),
	method( CVALuaCoreObject, IsRenderingModuleMuted ),
	method( CVALuaCoreObject, SetReproductionModuleGain ),
	method( CVALuaCoreObject, SetReproductionModuleMuted ),
	method( CVALuaCoreObject, GetReproductionModuleGain ),
	method( CVALuaCoreObject, IsReproductionModuleMuted ),

	/****************/
	method( CVALuaCoreObject, LockScene ),
	method( CVALuaCoreObject, UnlockScene ),
	method( CVALuaCoreObject, IsSceneLocked ),

	{ 0, 0 }
};

// Makros für Try-/Catch-Blöcke in den Funktionsumsetzern (Erspart Tipparbeit)
// Hierbei werden C++ Exceptions abgefangen und in Lua-Errors umgesetzt
#define VALUASHELL_TRY(); try {
#define VALUASHELL_CATCH(); } \
catch (CVAException& e) { luaL_error(L, e.ToString().c_str()); return 1; } \
catch (...) { luaL_error(L, "Unknown error occured"); return 1; }

CVALuaCoreObject::CVALuaCoreObject( lua_State *L ) {
	printf( "Created CVALuaCoreObject (%p)\n", this );
	m_pCore = CVALuaShellImpl::GetCoreInstance();
	printf( "Core instance (%p)\n", m_pCore );
}

CVALuaCoreObject::~CVALuaCoreObject(){
	printf( "Deleted CVALuaCoreObject (%p)\n", this );
}

int CVALuaCoreObject::GetVersionInfo( lua_State *L ) {

	VALUASHELL_TRY();

	CVAVersionInfo vi;
	m_pCore->GetVersionInfo( &vi );
	lua_pushstring( L, vi.ToString().c_str() );
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::Reset( lua_State *L ) {

	VALUASHELL_TRY();

	m_pCore->Reset();
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::GetState( lua_State *L ) {

	VALUASHELL_TRY();

	lua_pushinteger( L, m_pCore->GetState() );
	return 1;

	VALUASHELL_CATCH();
}

/* +----------------------------------------------------------+ *
 * |                                                          | *
 * |   Module interface                                       | *
 * |                                                          | *
 * +----------------------------------------------------------+ */

int CVALuaCoreObject::EnumerateModules( lua_State *L )
{
	/* [list of tables] = core:EnumerateModules() */
	VALUASHELL_TRY();

	std::vector<CVAModuleInfo> v;
	m_pCore->EnumerateModules( v );

	// TODO ...
	// Currently just print them 


	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::CallModule( lua_State *L )
{
	/* [table of return vals] = core:CallModule(name, [argname, argval], ...) */
	VALUASHELL_TRY();

	int argc = lua_gettop( L );
	std::string sModuleName = luaL_checkstring( L, 1 );

	CVAStruct oArgs, oReturn;
	if( ( ( argc - 1 ) % 2 ) != 0 )
		VA_EXCEPT2( INVALID_PARAMETER, "Missing value argument, only key/value structure allowed" );

	for( int i = 2; i <= argc; i += 2 )
	{
		std::string sArgName = luaL_checkstring( L, i );

		if( lua_isnumber( L, i + 1 ) ) {
			oArgs[ sArgName ] = luaL_checknumber( L, i + 1 );
			continue;
		}

		if( lua_isstring( L, i + 1 ) ) {
			oArgs[ sArgName ] = luaL_checkstring( L, i + 1 );
			continue;
		}

		if( lua_isboolean( L, i + 1 ) ) {
			oArgs[ sArgName ] = ( luaL_checkinteger( L, i + 1 ) != 0 );
			continue;
		}

		VA_EXCEPT2( INVALID_PARAMETER, "Unsupported Lua argument type" );
	}

	// Just a test
	m_pCore->CallModule( sModuleName, oArgs, oReturn );

	// Workaround: statisches key/value Paar namens RETURN verwenden, bis wir Struct nach LUA pushen können
	const std::string sReturnKey = "RETURN";
	if( oReturn.HasKey( sReturnKey ) )
	{
		const CVAStructValue& s = oReturn[ sReturnKey ];
		if( s.IsString() )
			lua_pushstring( L, std::string( s ).c_str() );
		if( s.IsBool() )
			lua_pushboolean( L, s );
		if( s.IsNumeric() )
			lua_pushnumber( L, s );

		return 1;
	}

	return 0;

	VALUASHELL_CATCH();
}



int CVALuaCoreObject::LockScene( lua_State* L )
{
	/* core:LockScene() */

	VALUASHELL_TRY();
	m_pCore->LockScene();
	return 0;
	VALUASHELL_CATCH();
}

int CVALuaCoreObject::UnlockScene( lua_State* L )
{
	/* core:UnlockScene() */
	VALUASHELL_TRY();
	m_pCore->UnlockScene();
	return 0;
	VALUASHELL_CATCH();
}

int CVALuaCoreObject::IsSceneLocked( lua_State *L )
{
	VALUASHELL_TRY();

	lua_pushboolean( L, m_pCore->IsSceneLocked() );
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::LoadDirectivity( lua_State *L ) {

	/* [int] core:LoadDirectivity( filename, name ) */

	VALUASHELL_TRY();

	std::string sFilename;
	std::string sName;

	int argc = lua_gettop( L );
	sFilename = luaL_checkstring( L, 1 );
	if( argc >= 2 ) sName = luaL_checkstring( L, 2 );

	lua_pushinteger( L, m_pCore->LoadDirectivity( sFilename, sName ) );
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::FreeDirectivity( lua_State *L ) {

	/* [bool] core:FreeDirectivity( dirID ) */

	VALUASHELL_TRY();

	int iID = luaL_checkint( L, 1 );

	lua_pushboolean( L, m_pCore->FreeDirectivity( iID ) );
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::GetDirectivityInfos( lua_State *L )
{
	// TODO: Implement
	VALUASHELL_TRY();

	std::vector<CVADirectivityInfo> vdiDest;
	int n = ( int ) vdiDest.size();

	VA_EXCEPT0( NOT_IMPLEMENTED );

	return 0;

	VALUASHELL_CATCH();
}

/* +----------------------------------------------------------+ *
 * |                                                          | *
 * |   Head-related impulse responses (HRIRs)                 | *
 * |                                                          | *
 * +----------------------------------------------------------+ */

int CVALuaCoreObject::LoadHRIRDataset( lua_State *L ) {

	/* [int] core:LoadHRIRDataset( filename, name ) */

	VALUASHELL_TRY();

	std::string sFilename;
	std::string sName;

	int argc = lua_gettop( L );
	sFilename = luaL_checkstring( L, 1 );
	if( argc >= 2 ) sName = luaL_checkstring( L, 2 );

	lua_pushinteger( L, m_pCore->LoadHRIRDataset( sFilename, sName ) );
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::FreeHRIRDataset( lua_State *L ) {

	/* [bool] core:FreeHRIRDataset( dirID ) */

	VALUASHELL_TRY();

	int iID = luaL_checkint( L, 1 );

	lua_pushboolean( L, m_pCore->FreeHRIRDataset( iID ) );
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::GetHRIRInfos( lua_State *L ) {
	// TODO: Implement
	VALUASHELL_TRY();

	std::vector<CVAHRIRInfo> vhiDest;
	int n = ( int ) vhiDest.size();
	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::LoadSound( lua_State *L ) {

	/* [int]  core:LoadSound(filename, Sound_name) */
	VALUASHELL_TRY();

	std::string sFilename;
	std::string sName;

	int argc = lua_gettop( L );
	sFilename = luaL_checkstring( L, 1 );
	if( argc >= 2 ) sName = luaL_checkstring( L, 2 );

	lua_pushinteger( L, m_pCore->LoadSound( sFilename, sName ) );
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::FreeSound( lua_State *L ) {

	/* [bool] core:FreeSound( SoundID ) */

	VALUASHELL_TRY();

	int iID = luaL_checkint( L, 1 );

	lua_pushboolean( L, m_pCore->FreeSound( iID ) );
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::PlaySound( lua_State *L ) {

	/* [bool] core:FreeSound( SoundID ) */

	VALUASHELL_TRY();

	int argc = lua_gettop( L );
	int iID = luaL_checkint( L, 1 );
	double dVolume( 1 );
	if( argc == 2 ) dVolume = luaL_checknumber( L, 2 );

	lua_pushinteger( L, m_pCore->PlaySound( iID, dVolume ) );
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::CreateSoundSource( lua_State *L ) {

	/* [int] core:CreateSoundSource( name, AuralizationMode, volume )  */

	VALUASHELL_TRY();

	// Variablen (Eingabe + Ausgabe)
	std::string sName;
	int iAuraMode = IVACore::VA_AURAMODE_ALL;
	double dVolume = 1.0;

	// Parameter parsen
	int argc = lua_gettop( L );
	sName = luaL_checkstring( L, 1 );
	if( argc >= 2 ) {
		std::string sAuraMode = luaL_checkstring( L, 2 );
		iAuraMode = m_pCore->ParseAuralizationModeStr( sAuraMode );
	}
	if( argc >= 3 )
		dVolume = luaL_checknumber( L, 3 );

	int iResult = m_pCore->CreateSoundSource( sName, iAuraMode, dVolume );

	// Rückgabe
	lua_pushnumber( L, iResult );
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::DeleteSoundSource( lua_State *L ) {

	/* [int]  core:DeleteSoundSource( soundSourceID )  */

	VALUASHELL_TRY();

	// Variablen (Eingabe + Ausgabe)
	int iID = -1;

	// Parameter parsen
	int argc = lua_gettop( L );
	iID = luaL_checkint( L, 1 );

	int iResult = m_pCore->DeleteSoundSource( iID );

	// Rückgabe
	lua_pushnumber( L, iResult );
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::GetSoundSourceName( lua_State *L ) {

	/* [string] core:GetSoundSourceName( soundSourceID )  */

	VALUASHELL_TRY();

	// Variablen (Eingabe + Ausgabe)
	int iID = -1;

	// Parameter parsen
	int argc = lua_gettop( L );
	iID = luaL_checkint( L, 1 );
	std::string sResult = m_pCore->GetSoundSourceName( iID );

	// Rückgabe
	lua_pushstring( L, sResult.c_str() );
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetSoundSourceName( lua_State *L )
{
	/* core:SetSoundSourceName( soundSourceID, name ) */

	VALUASHELL_TRY();

	std::string sName;

	// Variablen (Eingabe + Ausgabe)
	int iID = luaL_checkint( L, 1 );
	sName = luaL_checkstring( L, 2 );

	m_pCore->SetSoundSourceName( iID, sName );
	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetSoundSourcePositionOrientationVU( lua_State *L ) {

	/* core:SetSoundSourcePositionOrientationVU( soundSourceID, px, py, pz, vx, vy, vz, ux, uy, uz ) */

	VALUASHELL_TRY();

	// Variablen (Eingabe + Ausgabe)
	int iSoundSourceID = luaL_checkint( L, 1 );
	double px = luaL_checknumber( L, 2 );
	double py = luaL_checknumber( L, 3 );
	double pz = luaL_checknumber( L, 4 );
	double vx = luaL_checknumber( L, 5 );
	double vy = luaL_checknumber( L, 6 );
	double vz = luaL_checknumber( L, 7 );
	double ux = luaL_checknumber( L, 8 );
	double uy = luaL_checknumber( L, 9 );
	double uz = luaL_checknumber( L, 10 );

	m_pCore->SetSoundSourcePositionOrientationVU( iSoundSourceID, px, py, pz, vx, vy, vz, ux, uy, uz );
	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::GetSoundSourcePositionOrientationVU( lua_State *L ) {

	/* px,py,pz,vx,vy,vz,ux,uy,uz = core:GetSoundSourcePositionOrientationVU( soundSourceID ) */

	VALUASHELL_TRY();

	int iSoundSourceID = luaL_checkint( L, 1 );
	double px = 0, py = 0, pz = 0, vx = 0, vy = 0, vz = 0, ux = 0, uy = 0, uz = 0;

	m_pCore->GetSoundSourcePositionOrientationVU( iSoundSourceID, px, py, pz, vx, vy, vz, ux, uy, uz );
	lua_pushnumber( L, px );
	lua_pushnumber( L, py );
	lua_pushnumber( L, pz );
	lua_pushnumber( L, vx );
	lua_pushnumber( L, vy );
	lua_pushnumber( L, vz );
	lua_pushnumber( L, ux );
	lua_pushnumber( L, uy );
	lua_pushnumber( L, uz );
	return 9;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetSoundSourcePositionOrientationVelocityVU( lua_State *L ) {

	/* core:SetSoundSourcePositionOrientationVU( soundSourceID, px, py, pz, vx, vy, vz, ux, uy, uz, velx, vely, velz ) */

	VALUASHELL_TRY();

	// Variablen (Eingabe + Ausgabe)
	int iSoundSourceID = luaL_checkint( L, 1 );
	double px = luaL_checknumber( L, 2 );
	double py = luaL_checknumber( L, 3 );
	double pz = luaL_checknumber( L, 4 );
	double vx = luaL_checknumber( L, 5 );
	double vy = luaL_checknumber( L, 6 );
	double vz = luaL_checknumber( L, 7 );
	double ux = luaL_checknumber( L, 8 );
	double uy = luaL_checknumber( L, 9 );
	double uz = luaL_checknumber( L, 10 );
	double velx = luaL_checknumber( L, 11 );
	double vely = luaL_checknumber( L, 12 );
	double velz = luaL_checknumber( L, 13 );

	m_pCore->SetSoundSourcePositionOrientationVelocityVU( iSoundSourceID, px, py, pz, vx, vy, vz, ux, uy, uz, velx, vely, velz );
	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::GetSoundSourcePositionOrientationVelocityVU( lua_State *L ) {

	/* px,py,pz,vx,vy,vz,ux,uy,uz,velx,vely,velz = core:GetSoundSourcePositionOrientationVU( soundSourceID ) */

	VALUASHELL_TRY();

	int iSoundSourceID = luaL_checkint( L, 1 );
	double px = 0, py = 0, pz = 0, vx = 0, vy = 0, vz = 0, ux = 0, uy = 0, uz = 0, velx = 0, vely = 0, velz = 0;

	m_pCore->GetSoundSourcePositionOrientationVelocityVU( iSoundSourceID, px, py, pz, vx, vy, vz, ux, uy, uz, velx, vely, velz );
	lua_pushnumber( L, px );
	lua_pushnumber( L, py );
	lua_pushnumber( L, pz );
	lua_pushnumber( L, vx );
	lua_pushnumber( L, vy );
	lua_pushnumber( L, vz );
	lua_pushnumber( L, ux );
	lua_pushnumber( L, uy );
	lua_pushnumber( L, uz );
	lua_pushnumber( L, velx );
	lua_pushnumber( L, vely );
	lua_pushnumber( L, velz );
	return 12;

	VALUASHELL_CATCH();
}


/* +----------------------------------------------------------+ *
 * |                                                          | *
 * |   Globale Funktionen                                     | *
 * |                                                          | *
 * +----------------------------------------------------------+ */

int CVALuaCoreObject::GetInputGain( lua_State *L ) {

	/* [double] core:GetInputGain() */

	VALUASHELL_TRY();

	lua_pushnumber( L, m_pCore->GetInputGain() );
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetInputGain( lua_State *L ) {

	/* core:SetInputGain( dGain ) */

	VALUASHELL_TRY();

	double dGain = luaL_checknumber( L, 1 );

	m_pCore->SetInputGain( dGain );
	return 0;

	VALUASHELL_CATCH();
}


int CVALuaCoreObject::GetOutputGain( lua_State *L ) {

	/* [double] core:GetOutputGain() */

	VALUASHELL_TRY();

	lua_pushnumber( L, m_pCore->GetOutputGain() );
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetOutputGain( lua_State *L ) {

	/* core:SetOutputGain( dGain )  */

	VALUASHELL_TRY();

	double dGain = luaL_checknumber( L, 1 );
	m_pCore->SetOutputGain( dGain );
	return 0;

	VALUASHELL_CATCH();
}


int CVALuaCoreObject::IsOutputMuted( lua_State *L ) {

	/* [bool] core:IsOutputMuted() */

	VALUASHELL_TRY();

	lua_pushboolean( L, m_pCore->IsOutputMuted() ? 1 : 0 );
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetOutputMuted( lua_State *L ) {

	/* core:SetOutputMuted( bMuted ) */

	VALUASHELL_TRY();

	bool bMuted = ( luaL_checkint( L, 1 ) != 0 );

	m_pCore->SetOutputMuted( bMuted );
	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::GetGlobalAuralizationMode( lua_State *L ) {

	/* [string]    core:GetGlobalAuralizationMode() */

	VALUASHELL_TRY();

	int iAuraMode = m_pCore->GetGlobalAuralizationMode();
	std::string sAuraMode = m_pCore->GetAuralizationModeStr( iAuraMode, true );
	lua_pushstring( L, sAuraMode.c_str() );
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::GetVolumeStrDecibel( lua_State *L )
{
	/* [string] core:GetVolumeStrDecibel( volume ) */

	VALUASHELL_TRY();

	double dVolume = luaL_checkint( L, 1 );

	lua_pushstring( L, m_pCore->GetVolumeStrDecibel( dVolume ).c_str() );
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetGlobalAuralizationMode( lua_State *L ) {

	/* core:SetGlobalAuralizationMode( auralizationMode ) */

	VALUASHELL_TRY();

	std::string sAuraMode = luaL_checkstring( L, 1 );
	int iAuraModeCurrent = m_pCore->GetGlobalAuralizationMode();
	int iAuraMode = m_pCore->ParseAuralizationModeStr( sAuraMode, iAuraModeCurrent );
	m_pCore->SetGlobalAuralizationMode( iAuraMode );
	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::GetActiveListener( lua_State *L ) {

	/* [int] core:GetActiveListener()  */

	VALUASHELL_TRY();

	lua_pushinteger( L, m_pCore->GetActiveListener() );
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetActiveListener( lua_State *L ) {

	/* core:SetActiveListener( listenerID ) */

	VALUASHELL_TRY();

	int iListener = luaL_checkint( L, 1 );

	m_pCore->SetActiveListener( iListener );
	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::GetCoreClock( lua_State *L ) {
	/* [double] core:GetCoreClock() */
	VALUASHELL_TRY();

	lua_pushnumber( L, m_pCore->GetCoreClock() );
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetCoreClock( lua_State *L ) {
	/* core:SetCoreClock( seconds ) */

	VALUASHELL_TRY();

	double dSeconds = luaL_checknumber( L, 1 );

	m_pCore->SetCoreClock( dSeconds );
	return 0;

	VALUASHELL_CATCH();
}

/* +----------------------------------------------------------+ *
 * |                                                          | *
 * |   Rendering & Reproduction                               | *
 * |                                                          | *
 * +----------------------------------------------------------+ */

int CVALuaCoreObject::GetRenderingModules( lua_State *L )
{
	/* core:GetRenderingModuleInfos() */

	VALUASHELL_TRY();

	std::vector< CVAAudioRendererInfo > voRenderer;
	m_pCore->GetRenderingModules( voRenderer );

	//@todo implemet
	VA_EXCEPT0( NOT_IMPLEMENTED );

	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetRenderingModuleMuted( lua_State *L )
{
	/* core:SetRenderingModuleMuted( id, muted ) */

	VALUASHELL_TRY();

	std::string sID = luaL_checkstring( L, 1 );
	bool bMuted = bool( luaL_checkinteger( L, 2 ) != 0 );

	m_pCore->SetRenderingModuleMuted( sID, bMuted );

	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::IsRenderingModuleMuted( lua_State *L )
{

	/* [boolean] core:IsRenderingModuleMuted( id ) */

	VALUASHELL_TRY();

	std::string sID = luaL_checkstring( L, 1 );
	bool bMuted = m_pCore->IsRenderingModuleMuted( sID );
	lua_pushboolean( L, bMuted );
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetRenderingModuleGain( lua_State *L )
{
	/* core:SetRenderingModuleGain( id, gain ) */

	VALUASHELL_TRY();

	std::string sID = luaL_checkstring( L, 1 );
	double dGain = luaL_checknumber( L, 2 );
	m_pCore->SetRenderingModuleGain( sID, dGain );
	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::GetRenderingModuleGain( lua_State *L )
{

	/* [number] core:GetRenderingModuleGain( id ) */

	VALUASHELL_TRY();

	std::string sID = luaL_checkstring( L, 1 );
	double dGain = m_pCore->GetRenderingModuleGain( sID );
	lua_pushnumber( L, dGain );
	return 1;

	VALUASHELL_CATCH();
}


int CVALuaCoreObject::GetReproductionModules( lua_State *L )
{
	/* core:GetReproductionModules() */

	VALUASHELL_TRY();

	std::vector< CVAAudioReproductionInfo > voRepros;
	m_pCore->GetReproductionModules( voRepros );

	//@todo implemet
	VA_EXCEPT0( NOT_IMPLEMENTED );

	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetReproductionModuleMuted( lua_State *L )
{
	/* core:SetReproductionModuleMuted( id, muted ) */

	VALUASHELL_TRY();

	std::string sID = luaL_checkstring( L, 1 );
	bool bMuted = bool( luaL_checkinteger( L, 2 ) != 0 );

	m_pCore->SetReproductionModuleMuted( sID, bMuted );

	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::IsReproductionModuleMuted( lua_State *L )
{

	/* [boolean] core:IsReproductionModuleMuted( id ) */

	VALUASHELL_TRY();

	std::string sID = luaL_checkstring( L, 1 );
	bool bMuted = m_pCore->IsReproductionModuleMuted( sID );
	lua_pushboolean( L, bMuted );
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetReproductionModuleGain( lua_State *L )
{
	/* core:SetReproductionModuleGain( id, gain ) */

	VALUASHELL_TRY();

	std::string sID = luaL_checkstring( L, 1 );
	double dGain = luaL_checknumber( L, 2 );
	m_pCore->SetReproductionModuleGain( sID, dGain );
	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::GetReproductionModuleGain( lua_State *L )
{

	/* [number] core:GetReproductionModuleGain( id ) */

	VALUASHELL_TRY();

	std::string sID = luaL_checkstring( L, 1 );
	double dGain = m_pCore->GetReproductionModuleGain( sID );
	lua_pushnumber( L, dGain );
	return 1;

	VALUASHELL_CATCH();
}

/* +----------------------------------------------------------+ *
 * |                                                          | *
 * |   Signalquellen                                          | *
 * |                                                          | *
 * +----------------------------------------------------------+ */

int CVALuaCoreObject::CreateAudiofileSignalSource( lua_State *L )
{
	/* [string]  core:CreateAudiofileSignalSource( filename, name ) */

	VALUASHELL_TRY();

	int argc = lua_gettop( L );

	std::string sFilename = luaL_checkstring( L, 1 );
	std::string sName;
	if( argc == 2 ) sName = luaL_checkstring( L, 2 );

	lua_pushstring( L, m_pCore->CreateAudiofileSignalSource( sFilename, sName ).c_str() );
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::CreateSequencerSignalSource( lua_State *L )
{
	/* [string]  core:CreateSequencerSignalSource( name ) */

	VALUASHELL_TRY();

	int argc = lua_gettop( L );

	std::string sName;
	if( argc == 1 ) sName = luaL_checkstring( L, 1 );

	lua_pushstring( L, m_pCore->CreateSequencerSignalSource( sName ).c_str() );
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::CreateNetstreamSignalSource( lua_State *L )
{
	/* [int] core:CreateNetworkStreamSignalSource( interface, port, name ) */

	VALUASHELL_TRY();

	int argc = lua_gettop( L );

	std::string sInterface = luaL_checkstring( L, 1 );
	int iPort = luaL_checkint( L, 2 );
	std::string sName;
	if( argc == 3 ) sName = luaL_checkstring( L, 3 );

	lua_pushstring( L, m_pCore->CreateNetworkStreamSignalSource( sInterface, iPort, sName ).c_str() );
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::CreateEngineSignalSource( lua_State *L )
{
	/* [string]  core:CreateEngineSignalSource( name ) */

	VALUASHELL_TRY();

	int argc = lua_gettop( L );

	std::string sName = luaL_checkstring( L, 1 );

	lua_pushstring( L, m_pCore->CreateEngineSignalSource( sName ).c_str() );
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::CreateMachineSignalSource( lua_State* L )
{
	/* [string]  core:CreateMachineSignalSource( name ) */

	VALUASHELL_TRY();

	int argc = lua_gettop( L );

	std::string sName = luaL_checkstring( L, 1 );

	lua_pushstring( L, m_pCore->CreateMachineSignalSource( sName ).c_str() );
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::DeleteSignalSource( lua_State *L )
{
	/* 	[bool] core:DeleteSignalSource( ID ) */

	VALUASHELL_TRY();

	std::string sID = luaL_checkstring( L, 1 );

	lua_pushboolean( L, m_pCore->DeleteSignalSource( sID ) );
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::GetAudiofileSignalSourcePlaybackState( lua_State* L )
{
	/* 	[string] core:GetAudiofileSignalSourcePlaybackState( ID ) */

	VALUASHELL_TRY();
	std::string sSignalSourceID = luaL_checkstring( L, 1 );
	int iPlayState = m_pCore->GetAudiofileSignalSourcePlaybackState( sSignalSourceID );
	std::string sPlayState = m_pCore->GetPlaybackStateStr( iPlayState );
	lua_pushstring( L, sPlayState.c_str() );
	return 1;
	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetAudiofileSignalSourcePlaybackAction( lua_State* L )
{
	/*  core:SetAudiofileSignalSourcePlaybackAction( ID, PlaybackAction ) */
	VALUASHELL_TRY();
	std::string sSignalSourceID = luaL_checkstring( L, 1 );
	std::string sPlaybackAction = luaL_checkstring( L, 2 );
	int iPlayState = m_pCore->ParsePlaybackAction( sPlaybackAction );
	m_pCore->SetAudiofileSignalSourcePlaybackAction( sSignalSourceID, iPlayState );
	return 0;
	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetAudiofileSignalSourcePlaybackPosition( lua_State *L )
{
	/*  core:SetAudiofileSignalSourcePlaybackPosition( ID, PlayPosition ) */
	VALUASHELL_TRY();

	std::string sSignalSourceID = luaL_checkstring( L, 1 );
	double dPlayPosition = luaL_checknumber( L, 2 );
	m_pCore->SetAudiofileSignalSourcePlaybackPosition( sSignalSourceID, dPlayPosition );
	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetAudiofileSignalSourceIsLooping( lua_State *L )
{
	/*  core:SetAudiofileSignalSourcePlaybackIsLooping( ID, Looping ) */
	VALUASHELL_TRY();
	std::string sSignalSourceID = luaL_checkstring( L, 1 );
	bool bLooping = bool( luaL_checknumber( L, 2 ) == 1 );
	m_pCore->SetAudiofileSignalSourceIsLooping( sSignalSourceID, bLooping );
	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::GetAudiofileSignalSourceIsLooping( lua_State *L )
{
	/*  core:GetAudiofileSignalSourcePlaybackIsLooping( ID, Looping ) */
	VALUASHELL_TRY();
	std::string sSignalSourceID = luaL_checkstring( L, 1 );
	bool bLooping = m_pCore->GetAudiofileSignalSourceIsLooping( sSignalSourceID );
	lua_pushboolean( L, bLooping );
	return 1;
	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetSignalSourceParameters( lua_State* L )
{
	/*  core:SetSignalSourceParameters( ID, [argname, argval], ... ) */
	VALUASHELL_TRY();

	std::string sID = luaL_checkstring( L, 1 );
	CVAStruct oParams;
	int argc = lua_gettop( L );
	if( ( ( argc - 1 ) % 2 ) != 0 )
		VA_EXCEPT2( INVALID_PARAMETER, "Missing value argument, only key/value structure allowed" );

	for( int i = 2; i <= argc; i += 2 )
	{
		std::string sArgName = luaL_checkstring( L, i );

		if( lua_isnumber( L, i + 1 ) )
		{
			oParams[ sArgName ] = luaL_checknumber( L, i + 1 );
			continue;
		}

		if( lua_isstring( L, i + 1 ) )
		{
			oParams[ sArgName ] = luaL_checkstring( L, i + 1 );
			continue;
		}

		if( lua_isboolean( L, i + 1 ) )
		{
			oParams[ sArgName ] = ( luaL_checkinteger( L, i + 1 ) != 0 );
			continue;
		}

		VA_EXCEPT2( INVALID_PARAMETER, "Unsupported Lua argument type" );
	}

	m_pCore->SetSignalSourceParameters( sID, oParams );
	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::GetSignalSourceParameters( lua_State* L )
{
	/*  [ret] core:GetSignalSourceParameters( ID ) */
	VALUASHELL_TRY();

	std::string sID = luaL_checkstring( L, 1 );
	CVAStruct oParams;
	m_pCore->GetSignalSourceParameters( sID, oParams );

	lua_pushstring( L, "VASruct to Lua struct not implemented" );
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::GetMachineSignalSourceSpeed( lua_State* L )
{
	/* core:GetMachineSignalSourceSpeed( ID ) */
	VALUASHELL_TRY();
	std::string sID = luaL_checkstring( L, 1 );
	double dSpeed = m_pCore->GetMachineSignalSourceSpeed( sID );
	lua_pushnumber( L, dSpeed );
	return 1;
	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetMachineSignalSourceSpeed( lua_State* L )
{
	/*  core:SetMachineSignalSourceSpeed( ID, speed ) */
	VALUASHELL_TRY();
	std::string sID = luaL_checkstring( L, 1 );
	double dSpeed = luaL_checknumber( L, 2 );
	m_pCore->SetMachineSignalSourceSpeed( sID, dSpeed );
	return 0;
	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetMachineSignalSourceStartFile( lua_State* L )
{
	/*  core:SetMachineSignalSourceStartFile( ID, file ) */
	VALUASHELL_TRY();
	std::string sID = luaL_checkstring( L, 1 );
	std::string sFilePath = luaL_checkstring( L, 2 );
	m_pCore->SetMachineSignalSourceStartFile( sID, sFilePath );
	return 0;
	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetMachineSignalSourceIdleFile( lua_State* L )
{
	/*  core:SetMachineSignalSourceIdleFile( ID, file ) */
	VALUASHELL_TRY();
	std::string sID = luaL_checkstring( L, 1 );
	std::string sFilePath = luaL_checkstring( L, 2 );
	m_pCore->SetMachineSignalSourceIdleFile( sID, sFilePath );
	return 0;
	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetMachineSignalSourceStopFile( lua_State* L )
{
	/*  core:SetMachineSignalSourceStopFile( ID, file ) */
	VALUASHELL_TRY();
	std::string sID = luaL_checkstring( L, 1 );
	std::string sFilePath = luaL_checkstring( L, 2 );
	m_pCore->SetMachineSignalSourceStopFile( sID, sFilePath );
	return 0;
	VALUASHELL_CATCH();
}

int CVALuaCoreObject::StartMachineSignalSource( lua_State* L )
{
	/* core:StartMachineSignalSource( ID ) */
	VALUASHELL_TRY();
	std::string sID = luaL_checkstring( L, 1 );
	m_pCore->StartMachineSignalSource( sID );
	return 0;
	VALUASHELL_CATCH();
}

int CVALuaCoreObject::HaltMachineSignalSource( lua_State* L )
{
	/*  core:HaltMachineSignalSource( ID ) */
	VALUASHELL_TRY();
	std::string sID = luaL_checkstring( L, 1 );
	m_pCore->HaltMachineSignalSource( sID );
	return 0;
	VALUASHELL_CATCH();
}

int CVALuaCoreObject::GetMachineSignalSourceStateStr( lua_State* L )
{
	/*  [string] core:GetMachineSignalSourceStateStr( ID ) */
	VALUASHELL_TRY();
	std::string sID = luaL_checkstring( L, 1 );
	std::string sState = m_pCore->GetMachineSignalSourceStateStr( sID );
	lua_pushstring( L, sState.c_str() );
	return 1;
	VALUASHELL_CATCH();
}

int CVALuaCoreObject::GetSoundSourceSignalSource( lua_State *L )
{
	/* [string] core:GetSoundSourceSignalSource( soundSourceID ) */

	VALUASHELL_TRY();

	int iSoundSourceID = luaL_checkint( L, 1 );

	lua_pushstring( L, m_pCore->GetSoundSourceSignalSource( iSoundSourceID ).c_str() );
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetSoundSourceSignalSource( lua_State *L )
{
	/* core:SetSoundSourceSignalSource( soundSourceID, signalSourceID ) */

	VALUASHELL_TRY();

	int iSoundSourceID = luaL_checkint( L, 1 );
	std::string sSignalSourceID = luaL_checkstring( L, 2 );

	m_pCore->SetSoundSourceSignalSource( iSoundSourceID, sSignalSourceID );

	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::RemoveSoundSourceSignalSource( lua_State *L )
{
	/* core:RemoveSoundSourceSignalSource( iID ) */

	VALUASHELL_TRY();

	int iSoundSourceID = luaL_checkint( L, 1 );
	m_pCore->RemoveSoundSourceSignalSource( iSoundSourceID );
	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::GetSoundSourceAuralizationMode( lua_State *L )
{
	/* [string] core:GetSoundSourceAuralizationMode( soundSourceID ) */

	VALUASHELL_TRY();

	int iSoundSourceID = luaL_checkint( L, 1 );
	int iAuraMode = m_pCore->GetSoundSourceAuralizationMode( iSoundSourceID );
	std::string sAuraMode = m_pCore->GetAuralizationModeStr( iAuraMode, true );
	lua_pushstring( L, sAuraMode.c_str() );
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetSoundSourceAuralizationMode( lua_State *L )
{
	/* core:SetSoundSourceAuralizationMode( soundSourceID, auralizationMode) */

	VALUASHELL_TRY();

	int iSoundSourceID = luaL_checkint( L, 1 );
	std::string sAuraMode = luaL_checkstring( L, 2 );
	int iAuraModeCurrent = m_pCore->GetSoundSourceAuralizationMode( iSoundSourceID );
	int iAuraMode = m_pCore->ParseAuralizationModeStr( sAuraMode, iAuraModeCurrent );
	m_pCore->SetSoundSourceAuralizationMode( iSoundSourceID, iAuraMode );
	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::GetSoundSourceParameters( lua_State *L )
{
	/* [struct]    core:GetSoundSourceParameters( iID, oArgs ) */
	VALUASHELL_TRY();

	VA_EXCEPT2( NOT_IMPLEMENTED, "VAStruct i/o sreaming not implemented in Lua" );

	int iID = luaL_checkint( L, 1 );
	CVAStruct oArgs;
	CVAStruct oRet = m_pCore->GetSoundSourceParameters( iID, oArgs );
	// push oRet;
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetSoundSourceParameters( lua_State *L )
{
	/* core:SetSoundSourceParameters( iID, oParams ) */

	VALUASHELL_TRY();

	VA_EXCEPT2( NOT_IMPLEMENTED, "VAStruct i/o sreaming not implemented in Lua" );

	int iID = luaL_checkint( L, 1 );
	CVAStruct oParams;// = luaL_checkstr( L, 2 );
	m_pCore->SetSoundSourceParameters( iID, oParams );
	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::GetSoundSourceDirectivity( lua_State *L )
{
	/* [int] core:GetSoundSourceDirectivity( soundSourceID ) */

	VALUASHELL_TRY();

	int iSoundSourceID = luaL_checkint( L, 1 );
	lua_pushinteger( L, m_pCore->GetSoundSourceDirectivity( iSoundSourceID ) );
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetSoundSourceDirectivity( lua_State *L )
{
	/* core:SetSoundSourceDirectivity( soundSourceID, directivityID ) */

	VALUASHELL_TRY();

	int iSoundSourceID = luaL_checkint( L, 1 );
	int iDirectivityID = luaL_checkint( L, 2 );
	m_pCore->SetSoundSourceDirectivity( iSoundSourceID, iDirectivityID );
	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::GetSoundSourceVolume( lua_State *L )
{
	/* [double] core:GetSoundSourceVolume( soundSourceID ) */

	VALUASHELL_TRY();

	int iSoundSourceID = luaL_checkint( L, 1 );
	lua_pushnumber( L, m_pCore->GetSoundSourceVolume( iSoundSourceID ) );
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetSoundSourceVolume( lua_State *L )
{
	/* core:SetSoundSourceVolume( soundSourceID, dGain ) */

	VALUASHELL_TRY();

	int iSoundSourceID = luaL_checkint( L, 1 );
	double dGain = luaL_checknumber( L, 2 );
	m_pCore->SetSoundSourceVolume( iSoundSourceID, dGain );
	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetSoundSourceMuted( lua_State *L )
{
	/* [bool] core:SetSoundSourceMuted( soundSourceID, bMuted ) */

	VALUASHELL_TRY();

	int iSoundSourceID = luaL_checkint( L, 1 );
	bool bMuted = ( bool ) ( luaL_checkint( L, 2 ) != 0 );
	m_pCore->SetSoundSourceMuted( iSoundSourceID, bMuted );
	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::GetSoundSourcePosition( lua_State *L )
{
	/* x,y,z = core:GetSoundSourcePosition( soundSourceID ) */

	VALUASHELL_TRY();

	int iSoundSourceID = luaL_checkint( L, 1 );
	double x = 0, y = 0, z = 0;
	m_pCore->GetSoundSourcePosition( iSoundSourceID, x, y, z );
	lua_pushnumber( L, x );
	lua_pushnumber( L, y );
	lua_pushnumber( L, z );
	return 3;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetSoundSourcePosition( lua_State *L )
{
	/* core:SetSoundSourcePosition( soundSourceID, x, y, z ) */

	VALUASHELL_TRY();

	int iSoundSourceID = luaL_checkint( L, 1 );
	double x = luaL_checknumber( L, 2 );
	double y = luaL_checknumber( L, 3 );
	double z = luaL_checknumber( L, 4 );
	m_pCore->SetSoundSourcePosition( iSoundSourceID, x, y, z );
	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetSoundSourceOrientationYPR( lua_State *L )
{
	/* core:SetSoundSourceOrientationYPR( soundSourceID, yaw, pitch, roll) */

	VALUASHELL_TRY();

	int iSoundSourceID = luaL_checkint( L, 1 );
	double yaw = luaL_checknumber( L, 2 );
	double pitch = luaL_checknumber( L, 3 );
	double roll = luaL_checknumber( L, 4 );
	m_pCore->SetSoundSourceOrientationYPR( iSoundSourceID, yaw, pitch, roll );
	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::GetSoundSourceOrientationYPR( lua_State *L )
{
	/* yaw,pitch,roll = core:GetSoundSourceOrientationYPR( soundSourceID ) */

	VALUASHELL_TRY();

	int iSoundSourceID = luaL_checkint( L, 1 );
	double yaw = 0, pitch = 0, roll = 0;

	m_pCore->GetSoundSourceOrientationYPR( iSoundSourceID, yaw, pitch, roll );
	lua_pushnumber( L, yaw );
	lua_pushnumber( L, pitch );
	lua_pushnumber( L, roll );
	return 3;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetSoundSourceOrientationVU( lua_State *L )
{
	/* core:SetSoundSourceOrientationVU( soundSourceID, vx, vy, vz, ux, uy, uz ) */

	VALUASHELL_TRY();

	int iSoundSourceID = luaL_checkint( L, 1 );
	double vx = luaL_checknumber( L, 2 );
	double vy = luaL_checknumber( L, 3 );
	double vz = luaL_checknumber( L, 4 );
	double ux = luaL_checknumber( L, 5 );
	double uy = luaL_checknumber( L, 6 );
	double uz = luaL_checknumber( L, 7 );

	m_pCore->SetSoundSourceOrientationVU( iSoundSourceID, vx, vy, vz, ux, uy, uz );
	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::GetSoundSourceOrientationVU( lua_State *L )
{
	/* vx,vy,vz,ux,uy,uz = core:GetSoundSourceOrientationVU( soundSourceID )  */

	VALUASHELL_TRY();

	int iSoundSourceID = luaL_checkint( L, 1 );
	double vx = 0, vy = 0, vz = 0, ux = 0, uy = 0, uz = 0;

	m_pCore->GetSoundSourceOrientationVU( iSoundSourceID, vx, vy, vz, ux, uy, uz );
	lua_pushnumber( L, vx );
	lua_pushnumber( L, vy );
	lua_pushnumber( L, vz );
	lua_pushnumber( L, ux );
	lua_pushnumber( L, uy );
	lua_pushnumber( L, uz );
	return 6;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetSoundSourcePositionOrientationYPR( lua_State *L )
{
	/* core:SetSoundSourcePositionOrientationYPR( soundSourceID, x, y, z, yaw, pitch, roll ) */

	VALUASHELL_TRY();

	int iSoundSourceID = luaL_checkint( L, 1 );
	double x = luaL_checknumber( L, 2 );
	double y = luaL_checknumber( L, 3 );
	double z = luaL_checknumber( L, 4 );
	double yaw = luaL_checknumber( L, 5 );
	double pitch = luaL_checknumber( L, 6 );
	double roll = luaL_checknumber( L, 7 );

	m_pCore->SetSoundSourcePositionOrientationYPR( iSoundSourceID, x, y, z, yaw, pitch, roll );
	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::GetSoundSourcePositionOrientationYPR( lua_State *L )
{
	/* x,y,z,yaw,pitch,roll       = core:GetSoundSourcePositionOrientationYPR( soundSourceID ) */

	VALUASHELL_TRY();

	int iSoundSourceID = luaL_checkint( L, 1 );
	double x = 0, y = 0, z = 0, yaw = 0, pitch = 0, roll = 0;

	m_pCore->GetSoundSourcePositionOrientationYPR( iSoundSourceID, x, y, z, yaw, pitch, roll );
	lua_pushnumber( L, x );
	lua_pushnumber( L, y );
	lua_pushnumber( L, z );
	lua_pushnumber( L, yaw );
	lua_pushnumber( L, pitch );
	lua_pushnumber( L, roll );
	return 6;

	VALUASHELL_CATCH();
}
int CVALuaCoreObject::SetSoundSourcePositionOrientationVelocityYPR( lua_State *L )
{
	/* core:SetSoundSourcePositionOrientationYPR( soundSourceID, x, y, z, yaw, pitch, roll, velx, vely, velz ) */

	VALUASHELL_TRY();

	int iSoundSourceID = luaL_checkint( L, 1 );
	double x = luaL_checknumber( L, 2 );
	double y = luaL_checknumber( L, 3 );
	double z = luaL_checknumber( L, 4 );
	double yaw = luaL_checknumber( L, 5 );
	double pitch = luaL_checknumber( L, 6 );
	double roll = luaL_checknumber( L, 7 );
	double velx = luaL_checknumber( L, 8 );
	double vely = luaL_checknumber( L, 9 );
	double velz = luaL_checknumber( L, 10 );

	m_pCore->SetSoundSourcePositionOrientationVelocityYPR( iSoundSourceID, x, y, z, yaw, pitch, roll, velx, vely, velz );
	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::GetSoundSourcePositionOrientationVelocityYPR( lua_State *L )
{
	/* x,y,z,yaw,pitch,roll,velx,vely,velz       = core:GetSoundSourcePositionOrientationVelocityYPR( soundSourceID ) */

	VALUASHELL_TRY();

	int iSoundSourceID = luaL_checkint( L, 1 );
	double x = 0, y = 0, z = 0, yaw = 0, pitch = 0, roll = 0, velx = 0, vely = 0, velz = 0;

	m_pCore->GetSoundSourcePositionOrientationVelocityYPR( iSoundSourceID, x, y, z, yaw, pitch, roll, velx, vely, velz );
	lua_pushnumber( L, x );
	lua_pushnumber( L, y );
	lua_pushnumber( L, z );
	lua_pushnumber( L, yaw );
	lua_pushnumber( L, pitch );
	lua_pushnumber( L, roll );
	lua_pushnumber( L, velx );
	lua_pushnumber( L, vely );
	lua_pushnumber( L, velz );
	return 9;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::IsSoundSourceMuted( lua_State *L )
{
	/* [bool]   core:IsSoundSourceMuted( soundSourceID ) */

	VALUASHELL_TRY();

	int iSoundSourceID = luaL_checkint( L, 1 );

	lua_pushboolean( L, m_pCore->IsSoundSourceMuted( iSoundSourceID ) ? 1 : 0 );
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::AddSoundPlayback( lua_State *L ) {
	/* 	 [int]    core:AddSoundPlayback( signalSourceID, soundID, flags, timecode=0 ) */

	VALUASHELL_TRY();

	int argc = lua_gettop( L );  // the index of the Top Element in the Stack

	std::string sSignalSourceID = luaL_checkstring( L, 1 );
	int iSoundID = luaL_checkint( L, 2 );
	int iFlags = luaL_checkint( L, 3 );
	double dTimecode = 0;

	if( argc >= 3 ) dTimecode = luaL_checknumber( L, 4 );

	int iResult = m_pCore->AddSoundPlayback( sSignalSourceID, iSoundID, iFlags, dTimecode );

	lua_pushnumber( L, iResult );
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::RemoveSoundPlayback( lua_State *L ) {
	/* 	 [bool]    core:RemoveSoundPlayback(playbackID) */

	VALUASHELL_TRY();

	int argc = lua_gettop( L );  // the index of the Top Element in the Stack

	int iPlaybackID = luaL_checkint( L, 1 );

	bool bResult = m_pCore->RemoveSoundPlayback( iPlaybackID );

	lua_pushboolean( L, bResult );
	return 1;

	VALUASHELL_CATCH();
}

/* +----------------------------------------------------------+ *
 * |                                                          | *
 * |   Listener                                               | *
 * |                                                          | *
 * +----------------------------------------------------------+ */

int CVALuaCoreObject::CreateListener( lua_State *L )
{
	/* 	 [int]    core:CreateListener( name, auralizationMode, HRIRID ) */

	VALUASHELL_TRY();

	// Variablen( Eingabe + Ausgabe )
	std::string sName;
	int iAuraMode = IVACore::VA_AURAMODE_ALL;
	int iHRIRID = -1;

	int argc = lua_gettop( L );  // the index of the Top Element in the Stack
	sName = luaL_checkstring( L, 1 );

	if( argc >= 2 ) {
		std::string sAuraMode = luaL_checkstring( L, 2 );
		iAuraMode = m_pCore->ParseAuralizationModeStr( sAuraMode, iAuraMode );
	}
	if( argc >= 3 ) iHRIRID = luaL_checkint( L, 3 );


	int iResult = m_pCore->CreateListener( sName, iAuraMode, iHRIRID );

	/* Rückgabe */
	lua_pushnumber( L, iResult );
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::DeleteListener( lua_State *L )
{
	/* [int]    core:DeleteListener( listenerID ) */

	VALUASHELL_TRY();

	// Variablen (Eingabe + Ausgabe)
	int iID = -1;

	int argc = lua_gettop( L );
	iID = luaL_checkint( L, 1 );

	int iResult = m_pCore->DeleteListener( iID );

	/* Rückgabe */
	lua_pushnumber( L, iResult );
	return 1;

	VALUASHELL_CATCH();
}
int CVALuaCoreObject::GetListenerName( lua_State *L )
{
	/* [string] core:GetListenerName( listenerID )  */

	VALUASHELL_TRY();

	// Variablen (Eingabe + Ausgabe)
	int iID = luaL_checkint( L, 1 );

	std::string iResult = m_pCore->GetListenerName( iID );

	// Rückgabe
	lua_pushstring( L, iResult.c_str() );
	return 1;

	VALUASHELL_CATCH();
}
int CVALuaCoreObject::SetListenerName( lua_State *L )
{
	/* core:SetListenerName( listenerID, name ) */

	VALUASHELL_TRY();

	// Variablen (Eingabe + Ausgabe)
	int iID = luaL_checkint( L, 1 );
	std::string sName = luaL_checkstring( L, 2 );

	m_pCore->SetListenerName( iID, sName );
	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::GetListenerAuralizationMode( lua_State *L )
{
	/* [string]    core:GetListenerAuralizationMode( listenerID ) */
	VALUASHELL_TRY();

	int iListenerID = luaL_checkint( L, 1 );
	int iAuraMode = m_pCore->GetListenerAuralizationMode( iListenerID );
	std::string sAuraMode = m_pCore->GetAuralizationModeStr( iAuraMode, true );
	lua_pushstring( L, sAuraMode.c_str() );
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetListenerAuralizationMode( lua_State *L )
{
	/* core:SetListenerAuralizationMode( listenerID, auralizationMode ) */

	VALUASHELL_TRY();

	// Variablen (Eingabe + Ausgabe)
	int iListenerID = luaL_checkint( L, 1 );
	std::string sAuraMode = luaL_checkstring( L, 2 );
	int iCurrentAuraMode = m_pCore->GetListenerAuralizationMode( iListenerID );
	int iAuraMode = m_pCore->ParseAuralizationModeStr( sAuraMode, iCurrentAuraMode );
	m_pCore->SetListenerAuralizationMode( iListenerID, iAuraMode );
	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::GetListenerParameters( lua_State *L )
{
	/* [struct]    core:GetListenerParameters( iID, oArgs ) */
	VALUASHELL_TRY();

	VA_EXCEPT2( NOT_IMPLEMENTED, "VAStruct i/o sreaming not implemented in Lua" );

	int iID = luaL_checkint( L, 1 );
	CVAStruct oArgs;
	CVAStruct oRet = m_pCore->GetListenerParameters( iID, oArgs );
	// push oRet;
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetListenerParameters( lua_State *L )
{
	/* core:SetListenerParameters( iID, oParams ) */

	VALUASHELL_TRY();

	VA_EXCEPT2( NOT_IMPLEMENTED, "VAStruct i/o sreaming not implemented in Lua" );

	int iID = luaL_checkint( L, 1 );
	CVAStruct oParams;// = luaL_checkstr( L, 2 );
	m_pCore->SetListenerParameters( iID, oParams );
	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::GetListenerHRIRDataset( lua_State *L ) {
	/* [int]    core:GetListenerHRIRDataset( listenerID ) */
	VALUASHELL_TRY();

	int iListenerID = luaL_checkint( L, 1 );

	lua_pushinteger( L, m_pCore->GetListenerHRIRDataset( iListenerID ) );
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetListenerHRIRDataset( lua_State *L ) {
	/* core:SetListenerHRIRDataset( listenerID, hrirDatasetID ) */

	VALUASHELL_TRY();

	// Variablen (Eingabe + Ausgabe)
	int iListenerID = luaL_checkint( L, 1 );
	int iHRIRDatasetID = luaL_checkint( L, 2 );

	m_pCore->SetListenerHRIRDataset( iListenerID, iHRIRDatasetID );
	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetListenerPosition( lua_State *L )
{
	/* 	core:SetListenerPosition( listenerID, x,y,z ) */

	VALUASHELL_TRY();

	// Variablen (Eingabe + Ausgabe)
	int iID = luaL_checkint( L, 1 );
	double x = luaL_checknumber( L, 2 );
	double y = luaL_checknumber( L, 3 );
	double z = luaL_checknumber( L, 4 );

	m_pCore->SetListenerPosition( iID, x, y, z );
	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::GetListenerPosition( lua_State *L )
{
	/* x,y,z = core:GetListenerPosition( listenerID ) */

	VALUASHELL_TRY();

	int iListenerID = luaL_checkint( L, 1 );
	double x = 0, y = 0, z = 0;

	m_pCore->GetListenerPosition( iListenerID, x, y, z );

	lua_pushnumber( L, x );
	lua_pushnumber( L, y );
	lua_pushnumber( L, z );
	return 3;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetListenerOrientationYPR( lua_State *L )
{
	/* core:SetListenerOrientationYPR( listenerID, yaw, pitch, roll) */

	VALUASHELL_TRY();

	// Variablen (Eingabe + Ausgabe)
	int iID = luaL_checkint( L, 1 );
	double yaw = luaL_checknumber( L, 2 );
	double pitch = luaL_checknumber( L, 3 );
	double roll = luaL_checknumber( L, 4 );

	m_pCore->SetListenerOrientationYPR( iID, yaw, pitch, roll );
	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::GetListenerOrientationYPR( lua_State *L )
{
	/* yaw,pitch,roll = core:GetListenerOrientationYPR( listenerID ) */

	VALUASHELL_TRY();

	int iID = luaL_checkint( L, 1 );
	double yaw = 0, pitch = 0, roll = 0;

	m_pCore->GetListenerOrientationYPR( iID, yaw, pitch, roll );
	lua_pushnumber( L, yaw );
	lua_pushnumber( L, pitch );
	lua_pushnumber( L, roll );

	return 3;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetListenerOrientationVU( lua_State *L )
{
	/* core:SetListenerOrientationVU( listenerID, vx, vy, vz, ux, uy, uz ) */

	VALUASHELL_TRY();

	// Variablen (Eingabe + Ausgabe)
	int iID = luaL_checkint( L, 1 );
	double vx = luaL_checknumber( L, 2 );
	double vy = luaL_checknumber( L, 3 );
	double vz = luaL_checknumber( L, 4 );
	double ux = luaL_checknumber( L, 5 );
	double uy = luaL_checknumber( L, 6 );
	double uz = luaL_checknumber( L, 7 );

	m_pCore->SetListenerOrientationVU( iID, vx, vy, vz, ux, uy, uz );
	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::GetListenerOrientationVU( lua_State *L )
{
	/* vx,vy,vz,ux,uy,uz = core:GetListenerOrientationVU( listenerID ) */

	VALUASHELL_TRY();

	int iID = luaL_checkint( L, 1 );
	double vx = 0, vy = 0, vz = 0;
	double ux = 0, uy = 0, uz = 0;

	m_pCore->GetListenerOrientationVU( iID, vx, vy, vz, ux, uy, uz );
	lua_pushnumber( L, vx );
	lua_pushnumber( L, vy );
	lua_pushnumber( L, vz );
	lua_pushnumber( L, ux );
	lua_pushnumber( L, uy );
	lua_pushnumber( L, uz );
	return 6;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetListenerPositionOrientationYPR( lua_State *L )
{
	/* core:SetListenerPositionOrientationYPR( listenerID, x, y, z, yaw, pitch, roll ) */

	VALUASHELL_TRY();

	int iID = luaL_checkint( L, 1 );
	double x = luaL_checknumber( L, 2 );
	double y = luaL_checknumber( L, 3 );
	double z = luaL_checknumber( L, 4 );
	double yaw = luaL_checknumber( L, 5 );
	double pitch = luaL_checknumber( L, 6 );
	double roll = luaL_checknumber( L, 7 );

	m_pCore->SetListenerPositionOrientationYPR( iID, x, y, z, yaw, pitch, roll );
	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::GetListenerPositionOrientationYPR( lua_State *L )
{
	/* x,y,z,yaw,pitch,roll = core:GetListenerPositionOrientationYPR( listenerID ) */

	VALUASHELL_TRY();

	int iListenerID = luaL_checkint( L, 1 );
	double x = 0, y = 0, z = 0, yaw = 0, pitch = 0, roll = 0;

	m_pCore->GetListenerPositionOrientationYPR( iListenerID, x, y, z, yaw, pitch, roll );
	lua_pushnumber( L, x );
	lua_pushnumber( L, y );
	lua_pushnumber( L, z );
	lua_pushnumber( L, yaw );
	lua_pushnumber( L, pitch );
	lua_pushnumber( L, roll );
	return 6;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetListenerPositionOrientationVelocityYPR( lua_State *L )
{
	/* core:SetListenerPositionOrientationVelocityYPR( listenerID, x, y, z, yaw, pitch, roll,velx,vely,velz ) */

	VALUASHELL_TRY();

	int iID = luaL_checkint( L, 1 );
	double x = luaL_checknumber( L, 2 );
	double y = luaL_checknumber( L, 3 );
	double z = luaL_checknumber( L, 4 );
	double yaw = luaL_checknumber( L, 5 );
	double pitch = luaL_checknumber( L, 6 );
	double roll = luaL_checknumber( L, 7 );
	double velx = luaL_checknumber( L, 8 );
	double vely = luaL_checknumber( L, 9 );
	double velz = luaL_checknumber( L, 10 );

	m_pCore->SetListenerPositionOrientationVelocityYPR( iID, x, y, z, yaw, pitch, roll, velx, vely, velz );
	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::GetListenerPositionOrientationVelocityYPR( lua_State *L )
{
	/* x,y,z,yaw,pitch,roll,velx,vely,velz = core:GetListenerPositionOrientationVelocityYPR( listenerID ) */

	VALUASHELL_TRY();

	int iListenerID = luaL_checkint( L, 1 );
	double x = 0, y = 0, z = 0, yaw = 0, pitch = 0, roll = 0, velx = 0, vely = 0, velz = 0;

	m_pCore->GetListenerPositionOrientationVelocityYPR( iListenerID, x, y, z, yaw, pitch, roll, velx, vely, velz );
	lua_pushnumber( L, x );
	lua_pushnumber( L, y );
	lua_pushnumber( L, z );
	lua_pushnumber( L, yaw );
	lua_pushnumber( L, pitch );
	lua_pushnumber( L, roll );
	lua_pushnumber( L, velx );
	lua_pushnumber( L, vely );
	lua_pushnumber( L, velz );
	return 9;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetListenerPositionOrientationVU( lua_State *L )
{
	/* core:SetListenerPositionOrientationVU( listenerID, px, py, pz, vx, vy, vz, ux, uy, uz ) */

	VALUASHELL_TRY();

	int iListenerID = luaL_checkint( L, 1 );
	double px = luaL_checknumber( L, 2 );
	double py = luaL_checknumber( L, 3 );
	double pz = luaL_checknumber( L, 4 );

	double vx = luaL_checknumber( L, 5 );
	double vy = luaL_checknumber( L, 6 );
	double vz = luaL_checknumber( L, 7 );

	double ux = luaL_checknumber( L, 8 );
	double uy = luaL_checknumber( L, 9 );
	double uz = luaL_checknumber( L, 10 );

	m_pCore->SetListenerPositionOrientationVU( iListenerID, px, py, pz, vx, vy, vz, ux, uy, uz );
	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::GetListenerPositionOrientationVU( lua_State *L )
{
	/* px,py,pz,vx,vy,vz,ux,uy,uz = core:GetListenerPositionOrientationVU( listenerID ) */

	VALUASHELL_TRY();

	int iListenerID = luaL_checkint( L, 1 );
	double px = 0, py = 0, pz = 0, vx = 0, vy = 0, vz = 0, ux = 0, uy = 0, uz = 0;

	m_pCore->GetListenerPositionOrientationVU( iListenerID, px, py, pz, vx, vy, vz, ux, uy, uz );

	lua_pushnumber( L, px );
	lua_pushnumber( L, py );
	lua_pushnumber( L, pz );
	lua_pushnumber( L, vx );
	lua_pushnumber( L, vy );
	lua_pushnumber( L, vz );
	lua_pushnumber( L, ux );
	lua_pushnumber( L, uy );
	lua_pushnumber( L, uz );
	return 9;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetListenerPositionOrientationVelocityVU( lua_State *L )
{
	/* core:SetListenerPositionOrientationVelocityVU( listenerID, px, py, pz, vx, vy, vz, ux, uy, uz, velx, vely, velz ) */

	VALUASHELL_TRY();

	int iListenerID = luaL_checkint( L, 1 );
	double px = luaL_checknumber( L, 2 );
	double py = luaL_checknumber( L, 3 );
	double pz = luaL_checknumber( L, 4 );

	double vx = luaL_checknumber( L, 5 );
	double vy = luaL_checknumber( L, 6 );
	double vz = luaL_checknumber( L, 7 );

	double ux = luaL_checknumber( L, 8 );
	double uy = luaL_checknumber( L, 9 );
	double uz = luaL_checknumber( L, 10 );

	double velx = luaL_checknumber( L, 8 );
	double vely = luaL_checknumber( L, 9 );
	double velz = luaL_checknumber( L, 10 );

	m_pCore->SetListenerPositionOrientationVelocityVU( iListenerID, px, py, pz, vx, vy, vz, ux, uy, uz, velx, vely, velz );
	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::GetListenerPositionOrientationVelocityVU( lua_State *L )
{
	/* px,py,pz,vx,vy,vz,ux,uy,uz,velx,vely,velzz = core:GetListenerPositionOrientationVelocityVU( listenerID ) */

	VALUASHELL_TRY();

	int iListenerID = luaL_checkint( L, 1 );
	double px = 0, py = 0, pz = 0, vx = 0, vy = 0, vz = 0, ux = 0, uy = 0, uz = 0, velx = 0, vely = 0, velz = 0;

	m_pCore->GetListenerPositionOrientationVelocityVU( iListenerID, px, py, pz, vx, vy, vz, ux, uy, uz, velx, vely, velz );

	lua_pushnumber( L, px );
	lua_pushnumber( L, py );
	lua_pushnumber( L, pz );
	lua_pushnumber( L, vx );
	lua_pushnumber( L, vy );
	lua_pushnumber( L, vz );
	lua_pushnumber( L, ux );
	lua_pushnumber( L, uy );
	lua_pushnumber( L, uz );
	lua_pushnumber( L, velx );
	lua_pushnumber( L, vely );
	lua_pushnumber( L, velz );
	return 12;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::GetListenerRealWorldHeadPositionOrientationVU( lua_State *L )
{
	/* px,py,pz,vx,vy,vz,ux,uy,uz = core:GetListenerRealWorldHeadPositionOrientationVU( listenerID ) */

	VALUASHELL_TRY();

	int iListenerID = luaL_checkint( L, 1 );
	double px = 0, py = 0, pz = 0, vx = 0, vy = 0, vz = 0, ux = 0, uy = 0, uz = 0;

	m_pCore->GetListenerRealWorldHeadPositionOrientationVU( iListenerID, px, py, pz, vx, vy, vz, ux, uy, uz );

	lua_pushnumber( L, px );
	lua_pushnumber( L, py );
	lua_pushnumber( L, pz );
	lua_pushnumber( L, vx );
	lua_pushnumber( L, vy );
	lua_pushnumber( L, vz );
	lua_pushnumber( L, ux );
	lua_pushnumber( L, uy );
	lua_pushnumber( L, uz );
	return 9;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetListenerRealWorldHeadPositionOrientationVU( lua_State *L )
{
	/* core:SetListenerRealWorldHeadPositionOrientationVU( listenerID, px, py, pz, vx, vy, vz, ux, uy, uz ) */

	VALUASHELL_TRY();

	int iListenerID = luaL_checkint( L, 1 );
	double px = luaL_checknumber( L, 2 );
	double py = luaL_checknumber( L, 3 );
	double pz = luaL_checknumber( L, 4 );

	double vx = luaL_checknumber( L, 5 );
	double vy = luaL_checknumber( L, 6 );
	double vz = luaL_checknumber( L, 7 );

	double ux = luaL_checknumber( L, 8 );
	double uy = luaL_checknumber( L, 9 );
	double uz = luaL_checknumber( L, 10 );

	m_pCore->SetListenerRealWorldHeadPositionOrientationVU( iListenerID, px, py, pz, vx, vy, vz, ux, uy, uz );
	return 0;

	VALUASHELL_CATCH();
}

/* +----------------------------------------------------------+ *
 * |                                                          | *
 * |   Szene                                                  | *
 * |                                                          | *
 * +----------------------------------------------------------+ */

int CVALuaCoreObject::LoadScene( lua_State *L ) {
	/* core:LoadScene( filename ) */

	VALUASHELL_TRY();

	// Variablen (Eingabe + Ausgabe)
	std::string sFilename = luaL_checkstring( L, 1 );

	m_pCore->LoadScene( sFilename );
	return 0;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::IsSceneLoaded( lua_State *L ) {
	/* [boolean] core:IsSceneLoaded( ) */

	VALUASHELL_TRY();

	lua_pushboolean( L, m_pCore->IsSceneLoaded() );
	return 1;

	VALUASHELL_CATCH();
}

/*
int CVALuaCoreObject::GetSceneInfo(lua_State *L) {
// TODO ...
}
*/

int CVALuaCoreObject::GetPortalName( lua_State *L )
{
	/* [String] core:GetPortalName( portalID ) */

	VALUASHELL_TRY();

	// Variablen (Eingabe + Ausgabe)
	int iID = luaL_checkint( L, 1 );

	lua_pushstring( L, m_pCore->GetPortalName( iID ).c_str() );
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetPortalName( lua_State *L )
{
	/* core:SetPortalName( portalID, name ) */

	VALUASHELL_TRY();

	// Variablen (Eingabe + Ausgabe)
	int iID = luaL_checkint( L, 1 );
	std::string sName = luaL_checkstring( L, 2 );

	m_pCore->SetPortalName( iID, sName );
	return 0;

	VALUASHELL_CATCH();
}


int CVALuaCoreObject::GetPortalState( lua_State *L ) {
	/* [number] core:GetPortalState( portalID ) */

	VALUASHELL_TRY();

	// Variablen (Eingabe + Ausgabe)
	int iID = luaL_checkint( L, 1 );

	lua_pushnumber( L, m_pCore->GetPortalState( iID ) );
	return 1;

	VALUASHELL_CATCH();
}

int CVALuaCoreObject::SetPortalState( lua_State *L ) {
	/* core:SetPortalState( portalID, state ) */

	VALUASHELL_TRY();

	std::string sName;

	// Variablen (Eingabe + Ausgabe)
	int iID = luaL_checkint( L, 1 );
	double dState = luaL_checknumber( L, 2 );

	m_pCore->SetPortalState( iID, dState );
	return 0;

	VALUASHELL_CATCH();
}


#endif // IW_VALUA_CORE_OBJECT
