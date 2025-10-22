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

#include "VAMacroMap.h"

#include "Utils/VAUtils.h"

void CVAMacroMap::Clear( )
{
	m_mMacroMap.clear( );
}

void CVAMacroMap::AddMacro( const std::string& sName, const std::string& sValue )
{
	m_mMacroMap.insert( std::pair<std::string, std::string>( sName, correctPathForLUA( sValue ) ) );
}

std::string CVAMacroMap::SubstituteMacros( const std::string& sStr ) const
{
	std::string sOutput( sStr );

	std::string sRef;
	do
	{
		sRef = sOutput;

		// Solange alle Makros substituieren, bis sich nichts mehr ändert
		std::map<std::string, std::string>::const_iterator cit = m_mMacroMap.begin( );
		while( cit != m_mMacroMap.end( ) )
		{
			size_t pos = 0;
			const std::string& sMacroKey( cit->first );
			const std::string& sMacroValue( cit->second );
			while( ( pos = sOutput.find( sMacroKey, pos ) ) != std::string::npos )
			{
				sOutput.replace( pos, sMacroKey.length( ), sMacroValue );
				pos += sMacroValue.length( );
			}

			cit++;
		}

	} while( sOutput != sRef );

	return sOutput;
}

std::map<std::string, std::string> CVAMacroMap::GetMacroMapCopy( ) const
{
	return m_mMacroMap;
}
