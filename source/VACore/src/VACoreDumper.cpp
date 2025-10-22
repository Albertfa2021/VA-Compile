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
 *    Datei:			VACoreDumper.cpp
 *
 *    Zweck:			Dumper-Klasse welche VACore-Calls als Strings ausgibt
 *
 *    Autor(en):		Frank Wefers (Frank.Wefers@akustik.rwth-aachen.de
 *
 *  ---------------------------------------------------------------------------------
 */

// $Id: VACoreDumper.cpp 2729 2012-06-26 13:23:36Z fwefers $

#include "VACoreDumper.h"

#include "VACoreDumperImpl.h"

CVACoreDumper::CVACoreDumper( IVACore* pTarget = NULL, std::ostream* pOutputStream = &std::cout ) : m_pTarget( pTarget ), m_pStream( pOutputStream ) {}

CVACoreDumper::~CVACoreDumper( ) {}

IVACore* CVACoreDumper::getCoreInstance( ) const
{
	return m_pTarget;
}

void CVACoreDumper::setCoreInstance( IVACore* pTarget )
{
	m_pTarget = pTarget;
}

std::ostream* CVACoreDumper::getOutputStream( ) const
{
	return m_pStream;
}

void CVACoreDumper::setOutputStream( std::ostream* pOutputStream ) const
{
	m_pStream = pOutputStream;
}
