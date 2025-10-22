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

#include "VAAudioDriverConfig.h"

#include "../../Utils/VAUtils.h"

#include <VAException.h>

const int CVAAudioDriverConfig::AUTO                  = -1;
const double CVAAudioDriverConfig::DEFAULT_SAMPLERATE = 44100.0;

CVAAudioDriverConfig::CVAAudioDriverConfig( ) {}

CVAAudioDriverConfig::~CVAAudioDriverConfig( ) {}

void CVAAudioDriverConfig::Init( const CVAStruct& oArgs )
{
	CVALiterals<int> lits;
	lits.Add( "AUTO", AUTO );

	CVAConfigInterpreter conf( oArgs );
	conf.OptNonEmptyString( "Driver", sDriver );
	conf.OptString( "Device", sDevice, "AUTO" );
	conf.OptNumber( "SampleRate", dSampleRate, DEFAULT_SAMPLERATE );
	conf.OptInteger( "BufferSize", iBuffersize, AUTO, &lits );
	conf.OptInteger( "InputChannels", iInputChannels, AUTO, &lits );
	conf.OptInteger( "OutputChannels", iOutputChannels, AUTO, &lits );

	// Parameter prüfen
	if( dSampleRate <= 0 )
		VA_EXCEPT1( "Invalid samplerate specified" );

	if( ( iBuffersize < 0 ) && ( iBuffersize != AUTO ) )
		VA_EXCEPT1( "Invalid buffersize specified" );

	if( ( iInputChannels < 0 ) && ( iInputChannels != AUTO ) )
		VA_EXCEPT1( "Invalid number of input channels specified (autodetect = -1 or AUTO)" );

	if( ( iOutputChannels < 1 ) && ( iOutputChannels != AUTO ) )
		VA_EXCEPT1( "Invalid number of output channels specified (at least one required, if not using autodetect ... -1 or AUTO)" );
}
