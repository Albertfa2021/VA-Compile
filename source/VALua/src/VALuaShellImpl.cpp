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

#include "VALuaShellImpl.h"

#include "VALuaCoreObject.h"

#include <VACore.h>
#include <VACoreVersion.h>

#include <VistaTools/VistaFileSystemFile.h>
#include <VistaBase/VistaTimeUtils.h>

#include <atomic>
#include <cmath>
#include <iostream>


// TODO: Memory-Leak wenn Core überschrieben wird
static IVACore* pGlobalVACore = NULL;
static std::ostream* pGlobalStdOut = NULL;
static std::ostream* pGlobalStdErr = NULL;
static std::atomic< int > g_iGlobalStatus = 0;

#ifdef WIN32
// This is required for timers. Not available on other platforms.
#include <windows.h>
static HANDLE g_hWaitableTimer = 0;
#endif


// Überladene Print-Funktion
static int lPrint( lua_State *L )
{
	std::string s = luaL_checkstring( L, 1 );
	( *pGlobalStdOut ) << s << std::endl;
	return 0;
}

// Überladene Error-Funktion
static int lError( lua_State *L )
{
	std::string s = luaL_checkstring( L, 1 );
	( *pGlobalStdErr ) << s << std::endl;
	return 0;
}

static int lWelcome( lua_State *L )
{
	( *pGlobalStdOut ) << std::endl
		<< "  Welcome at the Virtual Acoustics console!" << std::endl << std::endl
		<< "  This is VAShell " << IVALuaShell::GetVersionStr() << std::endl << std::endl
		<< "  Type 'man() / help()' for help and more information" << std::endl
		<< std::endl;

	return 0;
}

// Überladene Man-Funktion
static int lMan( lua_State *L )
{
	( *pGlobalStdOut ) << std::endl
		<< "  VA core methods overview" << std::endl
		<< "  ========================" << std::endl << std::endl

		<< "  Scene control:" << std::endl << std::endl
		<< "           core:PrintSceneState()                     Return information on scene state" << std::endl
		<< "           core:BeginSynchronizedSceneModification()  Begins a synchronized scene modification" << std::endl
		<< "     [int] core:EndSynchronizedSceneModification()    Finished a synchronized scene modification and return its ID" << std::endl

		<< std::endl << std::endl

		<< "  Sound samples:" << std::endl << std::endl
		<< "    [int]  core:LoadSample(filename, sample_name)     Loads a sample and return its ID" << std::endl
		<< "    [bool] core:FreeSample( sampleID )                Release a sample and returns true if its successfully released" << std::endl
		<< "           core:PrintSampleInfos()                    Prints information on loaded samples" << std::endl

		<< std::endl << std::endl

		<< "  Directivities:" << std::endl << std::endl
		<< "     [int] core:LoadDirectivity( filename, name )     Loads a directivity and return its ID" << std::endl
		<< "           core:FreeDirectivity( dirID )              Delete a directivity" << std::endl
		<< "           core:PrintDirectivityInfos()               Return information on loaded directivities" << std::endl
		//<< "           core:GetDirectivityInfo( dirID )           Return information on loaded directivity" << std::endl

		<< std::endl << std::endl

		<< "  Signal sources:" << std::endl << std::endl
		<< "     [string]  core:CreateAudiofileSignalSource( filename, name )              Creates a new signal source to play audiofile and returns its id" << std::endl
		<< "     [string]  core:CreateSequencerSignalSource( name )						Creates a new sampler-signal source and returns its id" << std::endl
		<< "     [string]  core:CreateEngineSignalSource( name )							Creates an engine signal source and returns its id" << std::endl
		<< "     [int]     core:CreateNetworkStreamSignalSource( interface, port, name )   Creates a new signal source for network-streams and returns its id" << std::endl
		<< "     [bool]    core:DeleteSignalSource( ID )                                   Delete signal source and return true if the Signal Source deleted, else return false" << std::endl

		<< std::endl << std::endl

		<< "  Sound sources:" << std::endl << std::endl
		<< "     [int]    core:CreateSoundSource( name, AuralizationMode, volume )               Creates a new sound source and returns its id" << std::endl
		<< "     [int]    core:DeleteSoundSource( soundSourceID )                                Delete sound source and return 0 falls a sound source is deleted, else return -1" << std::endl
		<< "     [string] core:GetSoundSourceName( soundSourceID )                               Return sound source name" << std::endl
		<< "              core:SetSoundSourceName( soundSourceID, name )                         Change a sound source name" << std::endl
		<< "     [int]    core:GetSoundSourceSignalSource( soundSourceID )                       Return sound source on signal source (ID)" << std::endl
		<< "              core:SetSoundSourceSignalSource( soundSourceID, signalSourceID )       Change sound source on signal source" << std::endl
		<< "     [int]    core:GetSoundSourceAuralizationMode( soundSourceID )                   Return auralization mode" << std::endl
		<< "              core:SetSoundSourceAuralizationMode( soundSourceID, auralizationMode)  Change sound source auralization mode" << std::endl
		<< "     [double] core:GetSoundSourceVolume( soundSourceID )                             Return sound source volume" << std::endl
		<< "              core:SetSoundSourceVolume( soundSourceID, dGain )                      Change sound source volume" << std::endl
		<< "     [bool]   core:IsSoundSourceMuted( soundSourceID )                               Return true if sound source is mute, else return false" << std::endl
		<< "     [bool]   core:SetSoundSourceMuted( soundSourceID, bMuted )                      Set sound source muted" << std::endl
		<< std::endl << std::endl

		<< "     core:SetSoundSourcePosition( soundSourceID, x, y, z)                                           Set sound source position" << std::endl
		<< "     core:SetSoundSourceOrientationYPR( soundSourceID, yaw, pitch, roll)                            Set sound source orientation (yaw, pitch, roll)" << std::endl
		<< "     core:SetSoundSourceOrientationVU( soundSourceID, vx, vy, vz, ux, uy, uz )                      Set sound source orientation (view- and up-vector) " << std::endl
		<< "     core:SetSoundSourcePositionOrientationYPR( soundSourceID, x, y, z, yaw, pitch, roll )          Set sound source position and orientation (yaw-pitch-roll)" << std::endl
		<< "     core:SetSoundSourcePositionOrientationVU( soundSourceID, px, py, pz, vx, vy, vz, ux, uy, uz )  Set sound source position and orientation (position-, view- and up-vector)" << std::endl
		<< "     core:SetSoundSourcePositionOrientationVelocityYPR( soundSourceID, x, y, z, yaw, pitch, roll, vx, velx, vely, velz )	  Set sound source position, orientation (yaw-pitch-roll) and velocity" << std::endl
		<< "     core:SetSoundSourcePositionOrientationVelocityVU( soundSourceID, px, py, pz, vx, vy, vz, ux, uy, uz, velx, vely, velz ) Set sound source position, orientation (position-, view- and up-vector) and velocity" << std::endl

		<< "     x,y,z                      = core:GetSoundSourcePosition( soundSourceID )                      Return sound source position (position is a Vector)" << std::endl
		<< "     yaw,pitch,roll             = core:GetSoundSourceOrientationYPR( soundSourceID )                Return sound source orientation (yaw-pitch-roll angle)" << std::endl
		<< "     vx,vy,vz,ux,uy,uz          = core:GetSoundSourceOrientationVU( soundSourceID )                 Return sound source orientation (view- and up-vector)" << std::endl
		<< "     x,y,z,yaw,pitch,roll       = core:GetSoundSourcePositionOrientationYPR( soundSourceID )        Return sound source position and orientation (yaw-pitch-roll)" << std::endl
		<< "     px,py,pz,vx,vy,vz,ux,uy,uz = core:GetSoundSourcePositionOrientationVU( soundSourceID )         Return sound source position and orientation (position-, view- and up-vector)" << std::endl
		<< "     x,y,z,yaw,pitch,roll,velx,vely,velz		= core:GetSoundSourcePositionOrientationVelocityYPR( soundSourceID ) Return sound source position, orientation (yaw-pitch-roll) and velocity" << std::endl
		<< "     px,py,pz,vx,vy,vz,ux,uy,uz,velx,vely,velz = core:GetSoundSourcePositionOrientationVelocityVU( soundSourceID )	 Return sound source position, orientation (position-, view- and up-vector) and velocity" << std::endl

		<< std::endl << std::endl

		<< "  Listeners:" << std::endl << std::endl
		<< "     [int]    core:CreateListener( name, auralizationMode )                     Creates a new listener and returns its ID" << std::endl
		<< "     [int]    core:DeleteListener( listenerID )                                 Delete listener and return 0 if a listener successfully deleted, else return -1" << std::endl
		<< "     [string] core:GetListenerName( listenerID )                                Get listener name" << std::endl
		<< "              core:SetListenerName( listenerID, name )                          Set listener name" << std::endl
		<< "     [int]    core:GetListenerAuralizationMode( listenerID )                    Return listener auralization mode " << std::endl
		<< "              core:SetListenerAuralizationMode( listenerID, auralizationMode )  Set listener auralization mode" << std::endl
		<< "     [int]    core:GetActiveListener()                                          Return active listeners ID" << std::endl
		<< "              core:SetActiveListener( listenerID )                              Set active listener" << std::endl
		<< std::endl << std::endl
		<< "              core:SetListenerPosition( listenerID, x,y,z )                                             Set listener position" << std::endl
		<< "              core:SetListenerOrientationYPR( listenerID, yaw, pitch, roll)                             Set listener orientation (yaw, pitch, roll)" << std::endl
		<< "              core:SetListenerOrientationVU( listenerID, vx, vy, vz, ux, uy, uz )                       Set listener orientation (view- and up-vector)" << std::endl
		<< "              core:SetListenerPositionOrientationYPR( listenerID, x, y, z, yaw, pitch, roll )           Set listener position and orientation (yaw-pitch-roll)" << std::endl
		<< "              core:SetListenerPositionOrientationVU( listenerID, px, py, pz, vx, vy, vz, ux, uy, uz )   Set listener position and orientation (position-, view- and up-vector)" << std::endl
		<< "              core:SetListenerPositionOrientationVelocityYPR( listenerID, x, y, z, yaw, pitch, roll, vx, velx, vely, velz )         Set listener position, orientation (yaw-pitch-roll) and velocity" << std::endl
		<< "              core:SetListenerPositionOrientationVelocityVU( listenerID, px, py, pz, vx, vy, vz, ux, uy, uz, vx, velx, vely, velz ) Set listener position, orientation (position-, view- and up-vector) and velocity" << std::endl

		<< "              x,y,z                      = core:GetListenerPosition( listenerID )                       Get listener position" << std::endl
		<< "              yaw,pitch,roll             = core:GetListenerOrientationYPR( listenerID )                 Get listener orientation (yaw-pitch-roll angle)" << std::endl
		<< "              vx,vy,vz,ux,uy,uz          = core:GetListenerOrientationVU( listenerID )                  Get listener orientation (view- and up- vector)" << std::endl
		<< "              x,y,z,yaw,pitch,roll       = core:GetListenerPositionOrientationYPR( listenerID )         Get listener position and orientation (yaw-pitch-roll)" << std::endl
		<< "              px,py,pz,vx,vy,vz,ux,uy,uz = core:GetListenerPositionOrientationVU( listenerID )          Get listener position and orientation (position-, view- and up-vector)" << std::endl
		<< "              x,y,z,yaw,pitch,roll,velx,vely,velz       = core:GetListenerPositionOrientationVelocityYPR( listenerID )	Get listener position, orientation (yaw-pitch-roll) and velocity" << std::endl
		<< "              px,py,pz,vx,vy,vz,ux,uy,uz,velx,vely,velz = core:GetListenerPositionOrientationVelocityVU( listenerID )	Get listener position, orientation (position-, view- and up-vector) and velocity" << std::endl

		<< std::endl << std::endl

		<< "  Portals:" << std::endl << std::endl
		<< "     [String] core:GetPortalName( portalID )         Return portal name" << std::endl
		<< "              core:SetPortalName( portalID, name )   Set portal name" << std::endl

		<< std::endl << std::endl

		<< "  Globale functions:" << std::endl << std::endl
		<< "     []       core:Reset()                                        Reset the core" << std::endl
		<< "     [double] core:GetInputGain( input )                          Return input gain" << std::endl
		<< "              core:SetInputGain( input, dGain )                   Set input gain" << std::endl
		<< "     [double] core:GetOutputGain()                                Return output gain" << std::endl
		<< "              core:SetOutputGain( dGain )                         Set output gain" << std::endl
		<< "     [bool]   core:IsOutputMuted()                                Return true if the global is muted, else return false" << std::endl
		<< "              core:SetOutputMuted( bMuted )                       Set global muted" << std::endl
		<< "     [int]    core:GetGlobalAuralizationMode()                    Return global auralization mode" << std::endl
		<< "              core:SetGlobalAuralizationMode( auralizationMode )  Set Global auralization mode" << std::endl

		<< std::endl << std::endl

		<< "  Conversion functions:" << std::endl << std::endl
		<< "     [string] core:GetAuralizationModeStr( auralizationMode )     Convert auralization mode to string" << std::endl
		<< "     [string] core:GetVolumeStrDecibel( volume )                  Convert volume to decibel" << std::endl

		<< std::endl;

	return 0;
}

static int lHelp( lua_State *L )
{
	( *pGlobalStdOut ) << std::endl
		<< "  VA core methods overview" << std::endl
		<< "  ========================" << std::endl << std::endl

		<< "  Scene control:" << std::endl << std::endl
		<< "           core:PrintSceneState()                     Return information on scene state" << std::endl
		<< "           core:BeginSynchronizedSceneModification()  Begins a synchronized scene modification" << std::endl
		<< "     [int] core:EndSynchronizedSceneModification()    Finished a synchronized scene modification and return its ID" << std::endl

		<< std::endl << std::endl

		<< "  Sound samples:" << std::endl << std::endl
		<< "    [int]  core:LoadSample(filename, sample_name)     Loads a sampleand return its ID" << std::endl
		<< "    [bool] core:FreeSample( sampleID )                Release a sample and returns true if its successfully released" << std::endl
		<< "           core:PrintSampleInfos()                    Prints information on loaded samples" << std::endl

		<< std::endl << std::endl

		<< "  Directivities:" << std::endl << std::endl
		<< "     [int] core:LoadDirectivity( filename, name )     Loads a directivityand return its ID" << std::endl
		<< "           core:FreeDirectivity( dirID )              Delete a directivity" << std::endl
		<< "           core:PrintDirectivityInfos()               Return information on loaded directivities" << std::endl
		//<< "           core:GetDirectivityInfo( dirID )           Return information on loaded directivity" << std::endl

		<< std::endl << std::endl

		<< "  Signal sources:" << std::endl << std::endl
		<< "     [int]  core:CreateAudiofileSignalSource( filename, name )             Creates a new signal source to play audiofile and returns its id" << std::endl
		<< "     [int]  core:CreateSequencerSignalSource( name )                       Creates a new sampler-signal source and returns its id" << std::endl
		<< "     [int]  core:CreateEngineSignalSource( name )							Creates an engine signal source and returns its id" << std::endl
		<< "     [int]  core:CreateNetworkStreamSignalSource( interface, port, name )	Creates a new signal source for network-streams and returns its id" << std::endl
		<< "     [bool] core:DeleteSignalSource( ID )                                  Delete signal source and return true if the directivity erased, else return false" << std::endl

		<< std::endl << std::endl

		<< "  Sound sources:" << std::endl << std::endl
		<< "     [int]    core:CreateSoundSource( name, AuralizationMode, volume )               Creates a new sound source and returns its id" << std::endl
		<< "     [int]    core:DeleteSoundSource( soundSourceID )                                Delete sound source and return 0 falls a sound source is deleted, else return -1" << std::endl
		<< "     [string] core:GetSoundSourceName( soundSourceID )                               Return sound source name" << std::endl
		<< "              core:SetSoundSourceName( soundSourceID, name )                         Change a sound source name" << std::endl
		<< "     [int]    core:GetSoundSourceSignalSource( soundSourceID )                       Return sound source on signal source (ID)" << std::endl
		<< "              core:SetSoundSourceSignalSource( soundSourceID, signalSourceID )       Change sound source on signal source" << std::endl
		<< "     [int]    core:GetSoundSourceAuralizationMode( soundSourceID )                   Return auralization mode" << std::endl
		<< "              core:SetSoundSourceAuralizationMode( soundSourceID, auralizationMode)  Change sound source auralization mode" << std::endl
		<< "     [double] core:GetSoundSourceVolume( soundSourceID )                             Return sound source volume" << std::endl
		<< "              core:SetSoundSourceVolume( soundSourceID, dGain )                      Change sound source volume" << std::endl
		<< "     [bool]   core:IsSoundSourceMuted( soundSourceID )                               Return true if sound source is mute, else return false" << std::endl
		<< "     [bool]   core:SetSoundSourceMuted( soundSourceID, bMuted )                      Set sound source muted" << std::endl
		<< std::endl << std::endl

		<< "     core:SetSoundSourcePosition( soundSourceID, x, y, z)                                           Set sound source position" << std::endl
		<< "     core:SetSoundSourceOrientationYPR( soundSourceID, yaw, pitch, roll)                            Set sound source orientation (yaw, pitch, roll)" << std::endl
		<< "     core:SetSoundSourceOrientationVU( soundSourceID, vx, vy, vz, ux, uy, uz )                      Set sound source orientation (view- and up-vector) " << std::endl
		<< "     core:SetSoundSourcePositionOrientationYPR( soundSourceID, x, y, z, yaw, pitch, roll )          Set sound source position and orientation (yaw-pitch-roll)" << std::endl
		<< "     core:SetSoundSourcePositionOrientationVU( soundSourceID, px, py, pz, vx, vy, vz, ux, uy, uz )  Set sound source position and orientation (position-, view- and up-vector)" << std::endl
		<< "     core:SetSoundSourcePositionOrientationVelocityYPR( soundSourceID, x, y, z, yaw, pitch, roll, velx, vely, velz )          Set sound source position, orientation (yaw-pitch-roll) and velocity" << std::endl
		<< "     core:SetSoundSourcePositionOrientationVelocityVU( soundSourceID, px, py, pz, vx, vy, vz, ux, uy, uz, velx, vely, velz )  Set sound source position, orientation (position-, view- and up-vector) and velocity" << std::endl

		<< "     x,y,z                      = core:GetSoundSourcePosition( soundSourceID )                      Return sound source position (position is a Vector)" << std::endl
		<< "     yaw,pitch,roll             = core:GetSoundSourceOrientationYPR( soundSourceID )                Return sound source orientation (yaw-pitch-roll angle)" << std::endl
		<< "     vx,vy,vz,ux,uy,uz          = core:GetSoundSourceOrientationVU( soundSourceID )                 Return sound source orientation (view- and up-vector)" << std::endl
		<< "     x,y,z,yaw,pitch,roll       = core:GetSoundSourcePositionOrientationYPR( soundSourceID )        Return sound source position and orientation (yaw-pitch-roll)" << std::endl
		<< "     px,py,pz,vx,vy,vz,ux,uy,uz = core:GetSoundSourcePositionOrientationVU( soundSourceID )         Return sound source position and orientation (position-, view- and up-vector)" << std::endl
		<< "     x,y,z,yaw,pitch,roll,velx,vely,velz       = core:GetSoundSourcePositionOrientationVelocityYPR( soundSourceID ) Return sound source position, orientation (yaw-pitch-roll) and velocity" << std::endl
		<< "     px,py,pz,vx,vy,vz,ux,uy,uz,velx,vely,velz = core:GetSoundSourcePositionOrientationVelocityVU( soundSourceID )	 Return sound source position, orientation (position-, view- and up-vector) and velocity" << std::endl


		<< std::endl << std::endl

		<< "  Listeners:" << std::endl << std::endl
		<< "     [int]    core:CreateListener( name, auralizationMode )                     Creates a new listener and returns its ID" << std::endl
		<< "     [int]    core:DeleteListener( listenerID )                                 Delete listener and return 0 if a listener successfully deleted, else return -1" << std::endl
		<< "     [string] core:GetListenerName( listenerID )                                Get listener name" << std::endl
		<< "              core:SetListenerName( listenerID, name )                          Set listener name" << std::endl
		<< "     [int]    core:GetListenerAuralizationMode( listenerID )                    Return listener auralization mode " << std::endl
		<< "              core:SetListenerAuralizationMode( listenerID, auralizationMode )  Set listener auralization mode" << std::endl
		<< "     [int]    core:GetActiveListener()                                          Return active listeners ID" << std::endl
		<< "              core:SetActiveListener( listenerID )                              Set active listener" << std::endl
		<< std::endl << std::endl
		<< "              core:SetListenerPosition( listenerID, x,y,z )                                             Set listener position" << std::endl
		<< "              core:SetListenerOrientationYPR( listenerID, yaw, pitch, roll)                             Set listener orientation (yaw, pitch, roll)" << std::endl
		<< "              core:SetListenerOrientationVU( listenerID, vx, vy, vz, ux, uy, uz )                       Set listener orientation (view- and up-vector)" << std::endl
		<< "              core:SetListenerPositionOrientationYPR( listenerID, x, y, z, yaw, pitch, roll )           Set listener position and orientation (yaw-pitch-roll)" << std::endl
		<< "              core:SetListenerPositionOrientationVU( listenerID, px, py, pz, vx, vy, vz, ux, uy, uz )   Set listener position and orientation (position-, view- and up-vector)" << std::endl
		<< "              core:SetListenerPositionOrientationVelocityYPR( listenerID, x, y, z, yaw, pitch, roll, velx,vely,velz )         Set listener position, orientation (yaw-pitch-roll) and velocity" << std::endl
		<< "              core:SetListenerPositionOrientatioVelocitynVU( listenerID, px, py, pz, vx, vy, vz, ux, uy, uz, velx,vely,velz ) Set listener position, orientation (position-, view- and up-vector) and velocity" << std::endl

		<< "              x,y,z                      = core:GetListenerPosition( listenerID )                       Get listener position" << std::endl
		<< "              yaw,pitch,roll             = core:GetListenerOrientationYPR( listenerID )                 Get listener orientation (yaw-pitch-roll angle)" << std::endl
		<< "              vx,vy,vz,ux,uy,uz          = core:GetListenerOrientationVU( listenerID )                  Get listener orientation (view- and up- vector)" << std::endl
		<< "              x,y,z,yaw,pitch,roll       = core:GetListenerPositionOrientationYPR( listenerID )         Get listener position and orientation (yaw-pitch-roll)" << std::endl
		<< "              px,py,pz,vx,vy,vz,ux,uy,uz = core:GetListenerPositionOrientationVU( listenerID )          Get listener position and orientation (position-, view- and up-vector)" << std::endl
		<< "              x,y,z,yaw,pitch,roll,velx,vely,velz		 = core:GetListenerPositionOrientationVelocityYPR( listenerID ) Get listener position, orientation (yaw-pitch-roll) and velocity" << std::endl
		<< "              px,py,pz,vx,vy,vz,ux,uy,uz,velx,vely,velz = core:GetListenerPositionOrientationVelocityVU( listenerID )  Get listener position, orientation (position-, view- and up-vector) and velocity" << std::endl

		<< std::endl << std::endl

		<< "  Portals:" << std::endl << std::endl
		<< "     [String] core:GetPortalName( portalID )         Return portal name" << std::endl
		<< "              core:SetPortalName( portalID, name )   Set portal name" << std::endl

		<< std::endl << std::endl

		<< "  Globale functions:" << std::endl << std::endl
		<< "     [double] core:GetInputGain( input )                          Return input gain" << std::endl
		<< "              core:SetInputGain( input, dGain )                   Set input gain" << std::endl
		<< "     [double] core:GetOutputGain()                                Return output gain" << std::endl
		<< "              core:SetOutputGain( dGain )                         Set output gain" << std::endl
		<< "     [bool]   core:IsOutputMuted()                                Return true if the global is muted, else return false" << std::endl
		<< "              core:SetOutputMuted( bMuted )                       Set global muted" << std::endl
		<< "     [int]    core:GetGlobalAuralizationMode()                    Return global auralization mode" << std::endl
		<< "              core:SetGlobalAuralizationMode( auralizationMode )  Set Global auralization mode" << std::endl

		<< std::endl << std::endl

		<< "  Conversion functions:" << std::endl << std::endl
		<< "     [string] core:GetAuralizationModeStr( auralizationMode )     Convert auralization mode to string" << std::endl
		<< "     [string] core:GetVolumeStrDecibel( volume )                  Convert volume to decibel" << std::endl

		<< std::endl;

	return 0;
}

static int lGetShellStatus( lua_State *L )
{
	lua_pushnumber( L, g_iGlobalStatus );
	return 1;
}

static int lSetShellStatus( lua_State *L )
{
	g_iGlobalStatus = ( const int ) luaL_checkinteger( L, 1 );
	return 0;
}

static int lSleep( lua_State *L )
{
	const double dTimeSeconds = luaL_checknumber( L, 1 );
	if(dTimeSeconds <= 0 )
		return luaL_error( L, "Time must be greater zero" );

	VistaTimeUtils::Sleep( ceil( 1000.0f * dTimeSeconds ) );
	return 0;
}

// Auf Tastendruck warten
static int lWaitForKey( lua_State *L )
{
	getchar();
	return 0;
}

#ifdef WIN32
// Timer only available on Windows platforms.

static int lSetTimer( lua_State *L )
{
	double dPeriod = luaL_checknumber( L, 1 );
	if( dPeriod <= 0 )
		return luaL_error( L, "Timer period must be greater zero" );

	// Neuen Timer erzeugen, falls noch nicht geschehen
	if( g_hWaitableTimer == 0 )
	{
		if( FAILED( g_hWaitableTimer = CreateWaitableTimer( NULL, FALSE, NULL ) ) )
			return luaL_error( L, "Failed to create timer" );
	}

	LARGE_INTEGER liDueTime;
	liDueTime.QuadPart = 0;
	long lPeriod = ( long ) std::floor( dPeriod * 1000 );	// Dauer in Millisekunden

	if( FAILED( SetWaitableTimer( g_hWaitableTimer, &liDueTime, lPeriod, NULL, NULL, FALSE ) ) )
	{
		CloseHandle( g_hWaitableTimer );
		g_hWaitableTimer = 0;
		return luaL_error( L, "Failed to set timer" );
	}

	// Initial (skipped) wait, next call will be effective
	// in user space.
	WaitForSingleObject( g_hWaitableTimer, INFINITE );

	return 0;
}

static void ClearTimer()
{
	if( g_hWaitableTimer != 0 )
	{
		CloseHandle( g_hWaitableTimer );
		g_hWaitableTimer = 0;
	}

}

static int lWaitForTimer( lua_State *L )
{
	// No timer => no waiting ...
	if( g_hWaitableTimer == 0 )
		return 0;

	WaitForSingleObject( g_hWaitableTimer, INFINITE );
	return 0;
}

#else // NOT WIN32

static int lSetTimer(lua_State *L)
{
	VA_EXCEPT_NOT_IMPLEMENTED;
}

static void ClearTimer()
{
	VA_EXCEPT_NOT_IMPLEMENTED;
}

static int lWaitForTimer(lua_State *L)
{
	VA_EXCEPT_NOT_IMPLEMENTED;
}

#endif // WIN32

static int lSin( lua_State *L ) {
	double x = luaL_checknumber( L, 1 );
	lua_pushnumber( L, sin( x ) );
	return 1;
}

static int lCos( lua_State *L )
{
	double x = luaL_checknumber( L, 1 );
	lua_pushnumber( L, cos( x ) );
	return 1;
}

static int lExec( lua_State *L )
{
	const char* pszFilename = luaL_checkstring( L, 1 );
	int error = luaL_dofile( L, pszFilename );
	if( error )
	{
		( *pGlobalStdErr ) << "Error: " << lua_tostring( L, -1 ) << std::endl;
		lua_pop( L, 1 );  // Pop error message from the stack
	}
	return 0;
}

// --------------------------------------------------------

CVALuaShellImpl::CVALuaShellImpl()
	: m_posStdOut( &std::cout )
	, m_posStdErr( &std::cerr )
	, L( NULL )
{
	// TODO: Core zunächst Global speichern
	pGlobalVACore = NULL;
	pGlobalStdOut = m_posStdOut;
	pGlobalStdErr = m_posStdErr;

	// Lua-VM initialisieren
	L = lua_open();		/* opens Lua */
	luaopen_base( L );    /* opens the basic library */
	luaL_openlibs( L );

	// Objekt testen

	// Register the LuaCoreObject data type with Lua
	Luna<CVALuaCoreObject>::Register( L );

	// Funktionsbindungen Lua -> C++ erstellen

	lua_pushcfunction( L, lPrint );
	lua_setglobal( L, "print" );

	lua_pushcfunction( L, lError );
	lua_setglobal( L, "error" );

	lua_pushcfunction( L, lWelcome );
	lua_setglobal( L, "welcome" );

	lua_pushcfunction( L, lMan );
	lua_setglobal( L, "man" );

	lua_pushcfunction( L, lHelp );
	lua_setglobal( L, "help" );

	lua_pushcfunction( L, lGetShellStatus );
	lua_setglobal( L, "getShellStatus" );

	lua_pushcfunction( L, lSetShellStatus );
	lua_setglobal( L, "setShellStatus" );

	lua_pushcfunction( L, lSetTimer );
	lua_setglobal( L, "setTimer" );

	lua_pushcfunction( L, lWaitForTimer );
	lua_setglobal( L, "waitForTimer" );

	lua_pushcfunction( L, lSleep );
	lua_setglobal( L, "sleep" );

	lua_pushcfunction( L, lWaitForKey );
	lua_setglobal( L, "waitForKey" );

	lua_pushcfunction( L, lSin );
	lua_setglobal( L, "sin" );

	lua_pushcfunction( L, lCos );
	lua_setglobal( L, "cos" );

	lua_pushcfunction( L, lExec );
	lua_setglobal( L, "exec" );

	// VACore Literale definieren

	const std::vector<CVAIntLiteral>& oLits = IVACore::GetLiterals();
	for( size_t i = 0; i < oLits.size(); i++ )
	{
		lua_pushinteger( L, oLits[ i ].iValue );
		lua_setglobal( L, oLits[ i ].sName.c_str() );
	}
}

CVALuaShellImpl::~CVALuaShellImpl()
{
	lua_close( L );

	// Timer hier wegwerfen, auch wenn er global ist.
	ClearTimer();
}

IVACore* CVALuaShellImpl::GetCoreInstance()
{
	return pGlobalVACore;
}

void CVALuaShellImpl::SetCoreInstance( IVACore* pVACore )
{
	pGlobalVACore = pVACore;

}
void CVALuaShellImpl::SetOutputStream( std::ostream* posStdOut )
{
	m_posStdOut = posStdOut;
	// TODO: Remove globals
	pGlobalStdOut = posStdOut;
}

void CVALuaShellImpl::SetErrorStream( std::ostream* posStdErr )
{
	m_posStdErr = posStdErr;
	// TODO: Remove globals
	pGlobalStdErr = posStdErr;
}

int CVALuaShellImpl::GetStatus() const {
	// TODO: Currently this is implemented using a global variable
	return g_iGlobalStatus;
}

void CVALuaShellImpl::SetStatus( int iStatus ) {
	// TODO: Currently this is implemented using a global variable
	g_iGlobalStatus = iStatus;
}

void CVALuaShellImpl::ExecuteLuaScript( const std::string& sFilePath )
{
	VistaFileSystemFile oFile(sFilePath);
	int error = luaL_dofile( L, oFile.GetName().c_str() );
	if( error )
	{
		( *m_posStdErr ) << "Error: " << lua_tostring( L, -1 ) << std::endl;
		lua_pop( L, 1 );  // Pop error message from the stack
	}
}

void CVALuaShellImpl::HandleInputLine( const std::string& sLine )
{
	int error = luaL_loadbuffer( L, sLine.c_str(), ( int ) sLine.length(), "line" ) ||
		lua_pcall( L, 0, 0, 0 );
	if( error )
	{
		( *m_posStdErr ) << "Error: " << lua_tostring( L, -1 ) << std::endl;
		lua_pop( L, 1 );  // Pop error message from the stack
	}
}

void CVALuaShellImpl::report_errors( lua_State* L, int status )
{
	if( status != 0 )
	{
		( *m_posStdErr ) << "-- " << lua_tostring( L, -1 ) << std::endl;
		lua_pop( L, 1 ); // remove error message
	}
}
