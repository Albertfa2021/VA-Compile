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
#include <VAStruct.h>
#include <cassert>
#include <cstring>
#include <sstream>

CVAStructValue m_kDummyKey;

CVAStruct::CVAStruct( ) {}

CVAStruct::CVAStruct( const CVAStruct& rhs )
{
	*this = rhs;
}

CVAStruct::~CVAStruct( )
{
	// Recursively destruct associated keys
	CVAStruct::iterator it = Begin( );
	while( it != End( ) )
		m_mData.erase( it++ );
}

int CVAStruct::Size( ) const
{
	return (int)m_mData.size( );
}

bool CVAStruct::IsEmpty( ) const
{
	return m_mData.empty( );
}

bool CVAStruct::HasKey( const std::string& sKey ) const
{
	// Check if the key exists
	return m_mData.find( sKey ) != m_mData.end( );
}

void CVAStruct::Clear( )
{
	m_mData.clear( );
}

bool CVAStruct::RemoveKey( const std::string& sKey )
{
	// Check if the key exists
	MapIt it = m_mData.find( sKey );
	if( it == m_mData.end( ) )
		return false; // Key not found
	m_mData.erase( it );
	return true;
}

CVAStructValue* CVAStruct::Traverse( const std::string& sPath, size_t iPathCursor, const char cPathSeparator )
{
	// Find next path element
	size_t n;
	n = sPath.find( cPathSeparator, iPathCursor );
	if( n == std::string::npos )
	{
		// Last element on path
		Map::iterator it = m_mData.find( sPath.substr( iPathCursor, sPath.length( ) - iPathCursor ) );
		return ( it != m_mData.end( ) ? &( it->second ) : nullptr );
	}
	else
	{
		// Next element on path (at least one more follows)
		std::string t    = sPath.substr( iPathCursor, n - iPathCursor );
		Map::iterator it = m_mData.find( t );
		if( it == m_mData.end( ) )
			return nullptr; // Error: Path not found

		CVAStructValue& oNode = it->second;
		if( !oNode.IsStruct( ) )
			return nullptr; // Error: Node is a leaf, paths goes further...

		// Traverse into subtree
		return oNode.GetStruct( ).Traverse( sPath, n + 1, cPathSeparator );
	}
}

const CVAStructValue* CVAStruct::Traverse( const std::string& sPath, size_t iPathCursor, const char cPathSeparator ) const
{
	// Find next path element
	size_t n;
	n = sPath.find( cPathSeparator, iPathCursor );
	if( n == std::string::npos )
	{
		// Last element on path
		Map::const_iterator it = m_mData.find( sPath.substr( iPathCursor, sPath.length( ) - iPathCursor ) );
		return ( it != m_mData.end( ) ? &( it->second ) : nullptr );
	}
	else
	{
		// Next element on path (at least one more follows)
		std::string t          = sPath.substr( iPathCursor, n - iPathCursor );
		Map::const_iterator it = m_mData.find( t );
		if( it == m_mData.end( ) )
			return nullptr; // Error: Path not found

		const CVAStructValue& oNode = it->second;
		if( !oNode.IsStruct( ) )
			return nullptr; // Error: Node is a leaf, paths goes further...

		// Traverse into subtree
		return oNode.GetStruct( ).Traverse( sPath, n + 1, cPathSeparator );
	}
}

const CVAStructValue* CVAStruct::GetValue( const std::string& sPath, const char cPathSeparator ) const
{
	// Path resolution
	return Traverse( sPath, 0, cPathSeparator );
}

CVAStructValue* CVAStruct::GetValue( const std::string& sPath, const char cPathSeparator )
{
	// Path resolution
	return Traverse( sPath, 0, cPathSeparator );
}

const CVAStructValue& CVAStruct::operator[]( const char* pcKey ) const
{
	// Check if the key exists
	Map::const_iterator cit = m_mData.find( pcKey );

	if( cit != m_mData.end( ) )
		return cit->second;

	// Key does not exist
	assert( false );
	return m_kDummyKey;

	// TODO: How to handle this
}

CVAStructValue& CVAStruct::operator[]( const char* pcKey )
{
	// Check if the key exists
	Map::iterator it = m_mData.find( pcKey );

	if( it != m_mData.end( ) )
	{
		// If the key already exists return this
		return it->second;
	}
	else
	{
		// Otherwise: create an unassigned key with the given name
		it               = m_mData.insert( m_mData.begin( ), std::pair<std::string, CVAStructValue>( pcKey, CVAStructValue( ) ) );
		it->second.psKey = &it->first;
		return it->second;
	}
}

CVAStruct& CVAStruct::operator=( const CVAStruct& rhs )
{
	m_mData.clear( );

	// Autonome Kopien aller Schlüssel einfügen
	for( Map::const_iterator cit = rhs.Begin( ); cit != rhs.End( ); ++cit )
	{
		CVAStructValue& oKey = m_mData[cit->first];
		oKey.psKey           = &cit->first;
		oKey                 = cit->second;
	}

	return *this;
}

CVAStruct::iterator CVAStruct::Begin( )
{
	return m_mData.begin( );
}

CVAStruct::const_iterator CVAStruct::Begin( ) const
{
	return m_mData.begin( );
}

CVAStruct::iterator CVAStruct::End( )
{
	return m_mData.end( );
}

CVAStruct::const_iterator CVAStruct::End( ) const
{
	return m_mData.end( );
}

void CVAStruct::Merge( const CVAStruct& rhs, const bool bUnique )
{
	for( CVAStruct::const_iterator cit = rhs.Begin( ); cit != rhs.End( ); ++cit )
	{
		MapIt it = m_mData.find( cit->first );

		if( bUnique && it != m_mData.end( ) )
			VA_EXCEPT1( "Failed to merge structs. Keys not disjoint." );

		m_mData[cit->first] = cit->second;
	}
}

std::string CVAStruct::ToString( const int iIndent ) const
{
	std::stringstream ss;
	std::string margin( iIndent, ' ' );

	for( Map::const_iterator cit = m_mData.begin( ); cit != m_mData.end( ); ++cit )
	{
		const CVAStructValue& k = cit->second;
		switch( k.iDatatype )
		{
			case CVAStructValue::BOOL:
			{
				ss << margin << cit->first << " = " << cit->second.ToString( ) << " [bool]" << std::endl;
				break;
			}
			case CVAStructValue::INT:
			{
				ss << margin << cit->first << " = " << cit->second.ToString( ) << " [int]" << std::endl;
				break;
			}
			case CVAStructValue::DOUBLE:
			{
				ss << margin << cit->first << " = " << cit->second.ToString( ) << " [double]" << std::endl;
				break;
			}
			case CVAStructValue::STRING:
			{
				ss << margin << cit->first << " = \"" << cit->second.ToString( ) << "\" [string]" << std::endl;
				break;
			}
			case CVAStructValue::STRUCT:
			{
				const CVAStruct& oKey = cit->second;
				ss << margin << cit->first << " = [struct] { " << std::endl << margin << oKey.ToString( iIndent + 4 ) << margin << "}" << std::endl;
				break;
			}
			case CVAStructValue::DATA:
			{
				ss << margin << cit->first << " = [data: " << cit->second.GetDataSize( ) << " bytes]" << std::endl;
				break;
			}
			case CVAStructValue::SAMPLEBUFFER:
			{
				ss << margin << cit->first << " = [samplebuffer: " << cit->second.ToString( ) << " samples]" << std::endl;
				break;
			}
		}
	}

	return ss.str( );
}

// ----------------------------------------------------

// Constructors
CVAStructValue::CVAStructValue( ) : iDatatype( UNASSIGNED ), pStruct( NULL ) { };
CVAStructValue::CVAStructValue( const bool value ) : iDatatype( BOOL ), bValue( value ), pStruct( NULL ) { };
CVAStructValue::CVAStructValue( const int value ) : iDatatype( INT ), iValue( value ), pStruct( NULL ) { };
CVAStructValue::CVAStructValue( const double value ) : iDatatype( DOUBLE ), dValue( value ), pStruct( NULL ) { };
CVAStructValue::CVAStructValue( const char* value ) : iDatatype( STRING ), sValue( value ), pStruct( NULL ) { };
CVAStructValue::CVAStructValue( const std::string& value ) : iDatatype( STRING ), sValue( value ), pStruct( NULL ) { };
CVAStructValue::CVAStructValue( const CVAStruct& value ) : iDatatype( STRUCT ), pStruct( new CVAStruct( value ) ) { };

CVAStructValue::CVAStructValue( void* pData, const int iBytes ) : iDatatype( DATA ), pStruct( NULL )
{
	SetData( pData, iBytes );
}

CVAStructValue::CVAStructValue( const CVASampleBuffer& oSampleBuffer ) : iDatatype( SAMPLEBUFFER ), sbValue( oSampleBuffer ) {}

CVAStructValue::CVAStructValue( const CVAStructValue& rhs ) : iDatatype( UNASSIGNED ), pStruct( NULL )
{
	*this = rhs;
}

CVAStructValue::~CVAStructValue( )
{
	delete pStruct;
}

int CVAStructValue::GetDatatype( ) const
{
	return iDatatype;
}

int CVAStructValue::GetDataSize( ) const
{
	return iDataSize;
}

const void* CVAStructValue::GetData( ) const
{
	if( iDatatype != DATA )
		VA_EXCEPT1( "Key value is no binary data" );

	return &( vcData[0] );
}

void* CVAStructValue::GetData( )
{
	if( iDatatype != DATA )
		VA_EXCEPT1( "Key value is no binary data" );

	return &( vcData[0] );
}

void CVAStructValue::SetData( const void* pIncomingData, const int iIncomingBytes )
{
	assert( iDatatype == DATA );

	if( iDatatype != DATA )
		return;

	if( pIncomingData == NULL )
	{
		assert( iIncomingBytes == 0 );
	}
	else
	{
		assert( iIncomingBytes > 0 );
	}

	// Free previous data
	vcData.clear( );

	// Allocate new buffer and copy the data
	if( pIncomingData )
	{
		if( vcData.size( ) < iIncomingBytes )
			vcData.resize( iIncomingBytes );

		iDataSize = iIncomingBytes;
		// for( size_t i = 0; i < vcData.size(); i++ )
		// vcData[ i ] = pData[ i ];

		memcpy( GetData( ), pIncomingData, iIncomingBytes );
	}
}

const CVAStruct& CVAStructValue::GetStruct( ) const
{
	if( iDatatype != STRUCT )
		VA_EXCEPT1( "Key value is not a structure" );
	assert( pStruct );
	return *pStruct;
}

CVAStruct& CVAStructValue::GetStruct( )
{
	if( iDatatype != STRUCT )
		VA_EXCEPT1( "Key value is not a structure" );
	assert( pStruct );
	return *pStruct;
}

bool CVAStructValue::IsAssigned( ) const
{
	return ( iDatatype != UNASSIGNED );
}

bool CVAStructValue::IsBool( ) const
{
	return ( iDatatype == BOOL );
}

bool CVAStructValue::IsInt( ) const
{
	return ( iDatatype == INT );
}

bool CVAStructValue::IsDouble( ) const
{
	return ( iDatatype == DOUBLE );
}

bool CVAStructValue::IsString( ) const
{
	return ( iDatatype == STRING );
}

bool CVAStructValue::IsStruct( ) const
{
	return ( iDatatype == STRUCT );
}

bool CVAStructValue::IsNumeric( ) const
{
	return ( ( iDatatype == INT ) || ( iDatatype == DOUBLE ) );
}

bool CVAStructValue::IsData( ) const
{
	return ( iDatatype == DATA );
}

bool CVAStructValue::IsSampleBuffer( ) const
{
	return ( iDatatype == SAMPLEBUFFER );
}

CVAStructValue& CVAStructValue::operator=( const CVAStructValue& rhs )
{
	// Take care of self-assignment!
	if( this == &rhs )
		return *this;

	// Free associated struct first
	delete pStruct;
	iDataSize = 0;

	// Fast assignment. Copy only what is necessary ...
	iDatatype = rhs.iDatatype;
	switch( iDatatype )
	{
		case BOOL:
			bValue = rhs.bValue;
			break;

		case INT:
			iValue = rhs.iValue;
			break;

		case DOUBLE:
			dValue = rhs.dValue;
			break;

		case STRING:
			sValue = rhs.sValue;
			break;

		case STRUCT:
			// Autonome Kopie der Schüssel Struktur erzeugen
			pStruct = new CVAStruct( *rhs.pStruct );
			break;

		case DATA:
			// Autonome Kopie des Schüssel BLOBs erzeugen
			SetData( rhs.GetData( ), rhs.iDataSize );
			break;

		case SAMPLEBUFFER:
			sbValue = rhs.sbValue;
			break;
	}

	return *this;
}

CVAStructValue& CVAStructValue::operator=( const bool rhs )
{
	iDatatype = BOOL;
	bValue    = rhs;
	return *this;
}

CVAStructValue& CVAStructValue::operator=( const int rhs )
{
	iDatatype = INT;
	iValue    = rhs;
	return *this;
}

CVAStructValue& CVAStructValue::operator=( const double rhs )
{
	iDatatype = DOUBLE;
	dValue    = rhs;
	return *this;
}

CVAStructValue& CVAStructValue::operator=( const char* rhs )
{
	iDatatype = STRING;
	sValue    = std::string( rhs );
	return *this;
}

CVAStructValue& CVAStructValue::operator=( const std::string& rhs )
{
	iDatatype = STRING;
	sValue    = rhs;
	return *this;
}

CVAStructValue& CVAStructValue::operator=( const CVASampleBuffer& rhs )
{
	iDatatype = SAMPLEBUFFER;
	sbValue   = rhs;
	return *this;
}

const CVAStructValue& CVAStructValue::operator[]( const char* pcKey ) const
{
	if( !IsStruct( ) )
		VA_EXCEPT1( "Key is not a structure" );
	return ( *pStruct )[pcKey];
}

CVAStructValue& CVAStructValue::operator[]( const char* pcKey )
{
	if( !IsStruct( ) )
		VA_EXCEPT1( "Key is not a structure" );
	return ( *pStruct )[pcKey];
}

CVAStructValue::operator bool( ) const
{
	switch( iDatatype )
	{
		case BOOL:
			// bool = Key[bool] obligatory.
			return bValue;

		case INT:
			// bool = Key[int] is allowed. Default semantic like in C/C++ (false == 0).
			return ( iValue != 0 );

		case DOUBLE:
			// bool = Key[double] is imprecise and but not forbidden. Same is for int.
			return ( dValue != 0 );

		case STRING:
			// bool = Key[string] may be possible.
			{
				std::string s( sValue );
				std::transform( s.begin( ), s.end( ), s.begin( ), ( int ( * )( int ) )::toupper );
				if( ( s == "no" ) || ( s == "false" ) )
					return false;
				if( ( s == "yes" ) || ( s == "true" ) )
					return true;
			}

			VA_EXCEPT1( std::string( "Key \"" ) + ( *psKey ) + std::string( "\" cannot be interpreted as an logical value" ) );

		case SAMPLEBUFFER:
			return ( sbValue.GetNumSamples( ) > 0 );
		case STRUCT:
			// bool = Key[struct] is not possible.
			VA_EXCEPT1( "Types cannot be converted" );

		case UNASSIGNED:
			// Key is unassigned
			VA_EXCEPT1( std::string( "Key \"" ) + ( *psKey ) + std::string( "\" does not exist" ) );
	}

	// Invalid datatype.
	assert( false );
	return false;
}

//! Cast to int operator (required for assignments of the form 'int = Key')
CVAStructValue::operator int( ) const
{
	switch( iDatatype )
	{
		case BOOL:
			// int = Key[bool] allowed. Results in 0 or 1.
			return iValue;

		case INT:
			// int = Key[int] obligatory.
			return iValue;

		case DOUBLE:
			// int = Key[double] is dangerous (loss of precision) and but not forbidden.
			return (int)dValue;

		case STRING:
			// int = Key[string] may be possible.
			{
				std::istringstream ss( sValue );
				int i;
				if( !( ss >> i ).fail( ) )
					return i;
				else
					VA_EXCEPT1( std::string( "Key \"" ) + ( *psKey ) + std::string( "\" cannot be interpreted as an integer number" ) );
			}

		case STRUCT:
			// int = Key[struct] is not possible.
			VA_EXCEPT1( "Types cannot be converted" );

		case UNASSIGNED:
			// Key is unassigned
			VA_EXCEPT1( std::string( "Key \"" ) + ( *psKey ) + std::string( "\" does not exist" ) );
	}

	// Invalid datatype.
	assert( false );
	return 0;
}

//! Cast to double operator (required for assignments of the form 'double = Key')
CVAStructValue::operator double( ) const
{
	switch( iDatatype )
	{
		case BOOL:
			// double = Key[bool] allowed. Results in 0 or 1.
			return ( bValue ? 1 : 0 );

		case INT:
			// double = Key[int] is valid.
			return (double)iValue;

		case DOUBLE:
			// double = Key[double] obligatory.
			return dValue;

		case STRING:
			// double = Key[string] may be possible.
			// int = Key[string] may be possible.
			{
				std::istringstream ss( sValue );
				double d;
				if( !( ss >> d ).fail( ) )
					return d;
				else
					VA_EXCEPT1( std::string( "Key \"" ) + ( *psKey ) + std::string( "\" cannot be interpreted as a number" ) );
			}

		case STRUCT:
			// double = Key[struct] is not possible.
			VA_EXCEPT1( "Types cannot be converted" );

		case UNASSIGNED:
			// Key is unassigned
			// VA_EXCEPT1("Key does not exist");
			VA_EXCEPT1( std::string( "Key \"" ) + ( *psKey ) + std::string( "\" does not exist" ) );
	}

	// Invalid datatype.
	assert( false );
	return 0;
}

//! Cast to std::string operator (required for assignments of the form 'std::string = Key')
CVAStructValue::operator std::string( ) const
{
	switch( iDatatype )
	{
		case BOOL:
			// std::string = Key[bool] allowed. Implicit conversion.
			return ( bValue ? "true" : "false" );

		case INT:
		{
			// std::string = Key[int] allowed. Implicit conversion.
			std::stringstream ss;
			ss << iValue;
			return ss.str( );
		}

		case DOUBLE:
		{
			// std::string = Key[double] allowed. Implicit conversion.
			std::stringstream ss;
			ss << dValue;
			return ss.str( );
		}

		case STRING:
			// std::string = Key[string] obligatory.
			return sValue;

		case STRUCT:
			// std::string = Key[struct] is not possible.
			VA_EXCEPT1( "Types cannot be converted" );

		case UNASSIGNED:
			// Key is unassigned
			VA_EXCEPT1( std::string( "Key \"" ) + ( *psKey ) + std::string( "\" does not exist" ) );
	}

	// Invalid datatype.
	assert( false );
	return "";
}

CVAStructValue::operator void*( ) const
{
	// Note: Assignment only allowed for data keys!

	if( iDatatype == UNASSIGNED )
	{
		// Key is unassigned
		VA_EXCEPT1( std::string( "Key \"" ) + ( *psKey ) + std::string( "\" does not exist" ) );
	}

	if( iDatatype != DATA )
	{
		// Key is unassigned
		VA_EXCEPT1( "Types cannot be converted" );
	}

	return (void*)GetData( );
}

CVAStructValue::operator const CVASampleBuffer&( ) const
{
	if( iDatatype == UNASSIGNED )
		VA_EXCEPT1( std::string( "Key \"" ) + ( *psKey ) + std::string( "\" does not exist" ) );

	if( iDatatype != SAMPLEBUFFER )
		VA_EXCEPT1( "Types cannot be converted" );

	return sbValue;
}

CVAStructValue::operator CVASampleBuffer&( )
{
	if( iDatatype == UNASSIGNED )
		VA_EXCEPT1( std::string( "Key \"" ) + ( *psKey ) + std::string( "\" does not exist" ) );

	if( iDatatype != SAMPLEBUFFER )
		VA_EXCEPT1( "Types cannot be converted" );

	return sbValue;
}

CVAStructValue::operator const CVAStruct&( ) const
{
	if( iDatatype == UNASSIGNED )
	{
		// Key is unassigned
		VA_EXCEPT1( std::string( "Key \"" ) + ( *psKey ) + std::string( "\" does not exist" ) );
	}

	if( iDatatype != STRUCT )
	{
		// Key is unassigned
		VA_EXCEPT1( "Types cannot be converted" );
	}

	return *pStruct;
}

CVAStructValue::operator CVAStruct&( )
{
	if( iDatatype == UNASSIGNED )
	{
		// Key is unassigned
		VA_EXCEPT1( std::string( "Key \"" ) + ( *psKey ) + std::string( "\" does not exist" ) );
	}

	if( iDatatype != STRUCT )
	{
		// Key is unassigned
		VA_EXCEPT1( "Types cannot be converted" );
	}

	return *pStruct;
}

std::string CVAStructValue::ToString( ) const
{
	std::stringstream ss;

	// Just output the value, not name and type
	switch( iDatatype )
	{
		case BOOL:
			return ( bValue ? "TRUE" : "FALSE" );

		case INT:
			ss << iValue;
			break;

		case DOUBLE:
			ss << dValue;
			break;

		case STRING:
			return sValue;

		case STRUCT:
			return pStruct->ToString( );

		case SAMPLEBUFFER:
			ss << sbValue.GetNumSamples( );
			break;

		case UNASSIGNED:
			return "";
	}

	return ss.str( );
}

std::ostream& operator<<( std::ostream& os, const CVAStruct& s )
{
	return os << s.ToString( );
}

std::ostream& operator<<( std::ostream& os, const CVAStructValue& key )
{
	return os << key.ToString( );
}
