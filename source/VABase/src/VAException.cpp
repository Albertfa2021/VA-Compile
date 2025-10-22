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
#include <sstream>

CVAException::CVAException( ) : m_iErrorCode( UNSPECIFIED ) {}

CVAException::CVAException( const int iErrorCode, const std::string& sErrorMessage ) : m_iErrorCode( iErrorCode ), m_sErrorMessage( sErrorMessage ) {}

CVAException::~CVAException( ) {}

int CVAException::GetErrorCode( ) const
{
	return m_iErrorCode;
}

std::string CVAException::GetErrorMessage( ) const
{
	return m_sErrorMessage;
}

std::string CVAException::ToString( ) const
{
	std::stringstream ss;

	switch( m_iErrorCode )
	{
		case MODAL_ERROR:
			if( m_iErrorCode != 0 )
				ss << "Modal error: " << m_sErrorMessage << " (error code " << m_iErrorCode << ")";
			else
				ss << "Modal error: " << m_sErrorMessage;
			break;

		case NETWORK_ERROR:
			if( m_iErrorCode != 0 )
				ss << "Network error: " << m_sErrorMessage << " (error code " << m_iErrorCode << ")";
			else
				ss << "Network error: " << m_sErrorMessage;
			break;

		case PROTOCOL_ERROR:
			if( m_iErrorCode != 0 )
				ss << "Network protocol error: " << m_sErrorMessage << " (error code " << m_iErrorCode << ")";
			else
				ss << "Network protocol error: " << m_sErrorMessage;
			break;

			// Alle anderen bekannten Fehler
		case NOT_IMPLEMENTED:
		case INVALID_PARAMETER:
		case INVALID_ID:
		case RESOURCE_IN_USE:
		case FILE_NOT_FOUND:
			if( m_iErrorCode != 0 )
				ss << m_sErrorMessage << " (error code " << m_iErrorCode << ")";
			else
				ss << m_sErrorMessage;
			break;

		default:
			if( m_sErrorMessage.empty( ) )
			{
				if( m_iErrorCode != 0 )
					ss << "An unspecified error occured (error code " << m_iErrorCode << ")";
				else
					ss << "An unspecified error occured";
			}
			else
			{
				if( m_iErrorCode != 0 )
					ss << m_sErrorMessage << " (error code " << m_iErrorCode << ")";
				else
					ss << m_sErrorMessage;
			}
			break;
	}

	return ss.str( );
}

std::ostream& operator<<( std::ostream& os, const CVAException& ex )
{
	return os << ex.ToString( );
}
