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

#include <VAException.h>
#include <VANetUtils.h>
#include <sstream>

void SplitServerString( const std::string& sServerString, std::string& sAddress, int& iPort, const int iDefaultPort )
{
	// Rückwarts nach einem Doppelpunkt suchen
	size_t i = sServerString.rfind( ':' );

	// Adresse darf nicht leer sein bzw. mit Doppelpunkt beginnen
	if( i == 0 )
		VA_EXCEPT2( INVALID_PARAMETER, "Invalid address format, server name missing" );

	// Kein Doppelpunkt oder keine Doppelpunkt ohne Portnummer => Ist OK, aber Standardport verwenden
	if( ( i == std::string::npos ) || ( i == sServerString.length( ) - 1 ) )
	{
		// Default Port setzen
		iPort = iDefaultPort;

		// Serveradresse auslesen
		if( i == sServerString.length( ) - 1 )
			sAddress = sServerString.substr( 0, i ); // Nur Doppelpunkt ohne Port
		else
			sAddress = sServerString; // Kein Doppelpunkt

		// Adresse darf nicht leer sein
		if( sAddress.empty( ) )
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid address format, server name missing" );

		return;
	}

	sAddress      = sServerString.substr( 0, i );
	std::string t = sServerString.substr( i + 1, sServerString.length( ) - i - 1 );
	std::stringstream ss( t );

	if( ( ss >> iPort ).fail( ) )
		VA_EXCEPT2( INVALID_PARAMETER, "Invalid address format" );
}
