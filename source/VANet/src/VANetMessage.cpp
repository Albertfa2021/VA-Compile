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

#include "VANetMessage.h"

#include "VANetNetworkProtocol.h"

#include <VistaBase/VistaExceptionBase.h>
#include <VistaBase/VistaStreamUtils.h>
#include <VistaInterProcComm/Connections/VistaConnectionIP.h>
#include <cassert>
#include <cstring>

static int S_nMessageIds = 0;

CVANetMessage::CVANetMessage( const VistaSerializingToolset::ByteOrderSwapBehavior bSwapBuffers )
    : m_vecIncomingBuffer( 2048 )
    , m_oOutgoing( 2048 )
    , m_pConnection( NULL )
{
	m_oOutgoing.SetByteorderSwapFlag( bSwapBuffers );
	m_oIncoming.SetByteorderSwapFlag( bSwapBuffers );
	ResetMessage( );
}

void CVANetMessage::ResetMessage( )
{
	if( m_oIncoming.GetTailSize( ) > 0 )
		vstr::err( ) << "CVANetMessage::ResetMessage() called before message was fully processed!" << std::endl;

	// wait till sending is complete -> this prevents us
	// from deleting the buffer while it is still being read
	// by the connection
	if( m_pConnection )
		m_pConnection->WaitForSendFinish( );

	m_nMessageId = S_nMessageIds++;

	m_oOutgoing.ClearBuffer( );
	m_oOutgoing.WriteInt32( 0 ); // size dummy
	m_oOutgoing.WriteInt32( 0 ); // type dummy
	m_oOutgoing.WriteInt32( 0 ); // exceptmode dummy
	m_oOutgoing.WriteInt32( 0 ); // ID

	m_oIncoming.SetBuffer( NULL, 0 );

	m_nExceptionMode = CVANetNetworkProtocol::VA_NP_INVALID;
	m_nMessageType   = CVANetNetworkProtocol::VA_NP_INVALID;
	m_nAnswerType    = CVANetNetworkProtocol::VA_NP_INVALID;

	m_pConnection = NULL;
}

void CVANetMessage::SetConnection( VistaConnectionIP* pConn )
{
	m_pConnection = pConn;
}

void CVANetMessage::WriteMessage( )
{
	VANetCompat::byte* pBuffer = (VANetCompat::byte*)m_oOutgoing.GetBuffer( );
	VANetCompat::sint32 iSwapDummy;

	// rewrite size dummy
	iSwapDummy = m_oOutgoing.GetBufferSize( ) - sizeof( VANetCompat::sint32 );
	if( m_oOutgoing.GetByteorderSwapFlag( ) )
		VistaSerializingToolset::Swap4( &iSwapDummy );
	memcpy( pBuffer, &iSwapDummy, sizeof( VANetCompat::sint32 ) );

	pBuffer += sizeof( VANetCompat::sint32 );

	// rewrite type dummy
	iSwapDummy = m_nMessageType;
	if( m_oOutgoing.GetByteorderSwapFlag( ) )
		VistaSerializingToolset::Swap4( &iSwapDummy );
	memcpy( pBuffer, &iSwapDummy, sizeof( VANetCompat::sint32 ) );

	pBuffer += sizeof( VANetCompat::sint32 );

	// rewrite exceptmode dummy
	iSwapDummy = m_nExceptionMode;
	if( m_oOutgoing.GetByteorderSwapFlag( ) )
		VistaSerializingToolset::Swap4( &iSwapDummy );
	memcpy( pBuffer, &iSwapDummy, sizeof( VANetCompat::sint32 ) );

	pBuffer += sizeof( VANetCompat::sint32 );

	// rewrite messageid dummy
	iSwapDummy = m_nMessageId;
	if( m_oOutgoing.GetByteorderSwapFlag( ) )
		VistaSerializingToolset::Swap4( &iSwapDummy );
	memcpy( pBuffer, &iSwapDummy, sizeof( VANetCompat::sint32 ) );

	try
	{
		// It appears to be safe to transmit an entire buffer of arbitrary size on sender side
		int nRet = m_pConnection->WriteRawBuffer( m_oOutgoing.GetBuffer( ), m_oOutgoing.GetBufferSize( ) );
		m_pConnection->WaitForSendFinish( );
		if( nRet != m_oOutgoing.GetBufferSize( ) )
			VISTA_THROW( "Could not write the expected number of bytes", 255 );
	}
	catch( VistaExceptionBase& ex )
	{
		VA_EXCEPT2( NETWORK_ERROR, ex.GetExceptionText( ) );
	}
}

void CVANetMessage::ReadMessage( )
{
	try
	{
		VANetCompat::sint32 nMessageSize;
		int nReturn = m_pConnection->ReadInt32( nMessageSize );

		if( nReturn != sizeof( VANetCompat::sint32 ) )
			VA_EXCEPT2( NETWORK_ERROR, "Received less bytes than expected" );

		// we need at least the three protocol ints
		assert( nMessageSize >= 3 * sizeof( VANetCompat::sint32 ) );

		if( nMessageSize > (int)m_vecIncomingBuffer.size( ) )
			m_vecIncomingBuffer.resize( nMessageSize );

		int iBytesReadTotal = 0;
		while( nMessageSize != iBytesReadTotal )
		{
			int iIncommingBytes = m_pConnection->WaitForIncomingData( 0 );
			int iBytesRead      = 0;
			if( iBytesReadTotal + iIncommingBytes > nMessageSize )
				iBytesRead = m_pConnection->ReadRawBuffer( &m_vecIncomingBuffer[iBytesReadTotal], nMessageSize - iBytesReadTotal ); // read residual bytes
			else
				iBytesRead = m_pConnection->ReadRawBuffer( &m_vecIncomingBuffer[iBytesReadTotal], iIncommingBytes ); // read all incoming bytes

			iBytesReadTotal += iBytesRead;
		}
		assert( iBytesReadTotal == nMessageSize );

		m_oIncoming.SetBuffer( (VistaType::byte*)&m_vecIncomingBuffer[0], nMessageSize, false );
	}
	catch( VistaExceptionBase& ex )
	{
		VA_EXCEPT2( NETWORK_ERROR, ex.GetExceptionText( ) );
	}

	m_nMessageType   = ReadInt( );
	m_nExceptionMode = ReadInt( );
	m_nMessageId     = ReadInt( );
}

void CVANetMessage::WriteAnswer( )
{
#ifdef VANET_NETWORK_COM_PRINT_DEBUG_INFOS
	std::cout << "[ VANetMessage ][ DEBUG ] Writing answer" << std::endl;
#endif

	VANetCompat::byte* pBuffer = (VANetCompat::byte*)m_oOutgoing.GetBuffer( );
	VANetCompat::sint32 iSwapDummy;

	// rewrite size dummy
	iSwapDummy = m_oOutgoing.GetBufferSize( ) - sizeof( VANetCompat::sint32 );
	if( m_oOutgoing.GetByteorderSwapFlag( ) )
		VistaSerializingToolset::Swap4( &iSwapDummy );
	memcpy( pBuffer, &iSwapDummy, sizeof( VANetCompat::sint32 ) );

	pBuffer += sizeof( VANetCompat::sint32 );

	// rewrite type dummy
	iSwapDummy = m_nAnswerType;
	if( m_oOutgoing.GetByteorderSwapFlag( ) )
		VistaSerializingToolset::Swap4( &iSwapDummy );
	memcpy( pBuffer, &iSwapDummy, sizeof( VANetCompat::sint32 ) );

	pBuffer += sizeof( VANetCompat::sint32 );

	// rewrite exceptmode dummy
	iSwapDummy = m_nExceptionMode;
	if( m_oOutgoing.GetByteorderSwapFlag( ) )
		VistaSerializingToolset::Swap4( &iSwapDummy );
	memcpy( pBuffer, &iSwapDummy, sizeof( VANetCompat::sint32 ) );

	pBuffer += sizeof( VANetCompat::sint32 );

	// rewrite message dummy
	iSwapDummy = m_nMessageId;
	if( m_oOutgoing.GetByteorderSwapFlag( ) )
		VistaSerializingToolset::Swap4( &iSwapDummy );
	memcpy( pBuffer, &iSwapDummy, sizeof( VANetCompat::sint32 ) );

	try
	{
		if( !m_pConnection )
			VISTA_THROW( "Seems like the VA connections is lost, can not write VA net message answer", 255 );

#ifdef VANET_NETWORK_COM_PRINT_DEBUG_INFOS
		std::cout << "[ VANetMessage ][ DEBUG ] All seems good, will hand over buffer to network connection now" << std::endl;
#endif

		// It appears to be safe to transmit an entire buffer of arbitrary size on sender side
		int nRet = m_pConnection->WriteRawBuffer( m_oOutgoing.GetBuffer( ), m_oOutgoing.GetBufferSize( ) );
		m_pConnection->WaitForSendFinish( );

#ifdef VANET_NETWORK_COM_PRINT_DEBUG_INFOS
		std::cout << "[ VANetMessage ][ DEBUG ] ... acknowledged." << std::endl;
#endif

		if( nRet != m_oOutgoing.GetBufferSize( ) )
			VISTA_THROW( "Could not write the expected number of bytes", 255 );
	}
	catch( VistaExceptionBase& ex )
	{
		VA_EXCEPT2( NETWORK_ERROR, ex.GetExceptionText( ) );
	}

#ifdef VANET_NETWORK_COM_PRINT_DEBUG_INFOS
	std::cout << "[ VANetMessage ][ DEBUG ] Transmitted " << m_oOutgoing.GetBufferSize( ) << " bytes" << std::endl;
#endif
}

void CVANetMessage::ReadAnswer( )
{
#ifdef VANET_NETWORK_COM_PRINT_DEBUG_INFOS
	std::cout << "[ VANetMessage ][ DEBUG ] Reading answer" << std::endl;
#endif

	VANetCompat::sint32 nMessageSize;

	try
	{
		int nReturn;
		try
		{
			nReturn = m_pConnection->ReadInt32( nMessageSize );
		}
		catch( ... )
		{
			nReturn = -1; // Network connection error
		}

		if( nReturn != sizeof( VANetCompat::sint32 ) )
		{
			VA_EXCEPT2( NETWORK_ERROR, "Received less bytes than expected during ReadAnswer of VANetMessage. Connection broken?" );
		}

		// we need at least the three protocol types
		assert( nMessageSize >= 3 * sizeof( VANetCompat::sint32 ) );

		if( nMessageSize > (int)m_vecIncomingBuffer.size( ) )
			m_vecIncomingBuffer.resize( nMessageSize );

		int iBytesReceivedTotal = 0;
		while( nMessageSize > iBytesReceivedTotal )
		{
			int iIncommingBytes = m_pConnection->WaitForIncomingData( 0 );
			int iBytesReceived  = m_pConnection->ReadRawBuffer( &m_vecIncomingBuffer[iBytesReceivedTotal], iIncommingBytes );
			iBytesReceivedTotal += iBytesReceived;
		}

		if( iBytesReceivedTotal != nMessageSize )
			VA_EXCEPT2( NETWORK_ERROR, "Reading message, but received less bytes than expected." );

		m_oIncoming.SetBuffer( (VistaType::byte*)&m_vecIncomingBuffer[0], nMessageSize, false );
	}
	catch( VistaExceptionBase& ex )
	{
		VA_EXCEPT2( NETWORK_ERROR, ex.GetExceptionText( ) );
	}

	m_nAnswerType = ReadInt( );
	ReadInt( ); // protocol overhead - just read and ignore
	int nMessageID = ReadInt( );
	assert( nMessageID == m_nMessageId );
	m_nMessageId = nMessageID;

#ifdef VANET_NETWORK_COM_PRINT_DEBUG_INFOS
	std::cout << "[ VANetMessage ][ DEBUG ] Received " << nMessageSize << " bytes" << std::endl;
#endif
}

int CVANetMessage::GetExceptionMode( ) const
{
	return m_nExceptionMode;
}
int CVANetMessage::GetMessageType( ) const
{
	return m_nMessageType;
}
int CVANetMessage::GetAnswerType( ) const
{
	return m_nAnswerType;
}

void CVANetMessage::SetExceptionMode( const int nMode )
{
	m_nExceptionMode = nMode;
}
void CVANetMessage::SetMessageType( const int nType )
{
	m_nMessageType = nType;
}
void CVANetMessage::SetAnswerType( const int nType )
{
	m_nAnswerType = nType;
}

int CVANetMessage::GetIncomingMessageSize( ) const
{
	return m_oIncoming.GetTailSize( );
}
int CVANetMessage::GetOutgoingMessageSize( ) const
{
	return m_oOutgoing.GetBufferSize( );
}

bool CVANetMessage::GetOutgoingMessageHasData( ) const
{
	return ( m_oOutgoing.GetBufferSize( ) > 4 * sizeof( VANetCompat::sint32 ) );
}

void CVANetMessage::WriteString( const std::string& sValue )
{
	m_oOutgoing.WriteInt32( (VANetCompat::sint32)sValue.size( ) );
	if( !sValue.empty( ) )
		m_oOutgoing.WriteString( sValue );
}
void CVANetMessage::WriteInt( const int iValue )
{
	m_oOutgoing.WriteInt32( (VANetCompat::sint32)iValue );
}
void CVANetMessage::WriteUInt64( const uint64_t iValue )
{
	m_oOutgoing.WriteUInt64( (VANetCompat::uint64)iValue );
}
void CVANetMessage::WriteBool( const bool bValue )
{
	m_oOutgoing.WriteBool( bValue );
}
void CVANetMessage::WriteFloat( const float fValue )
{
	m_oOutgoing.WriteFloat32( fValue );
}
void CVANetMessage::WriteDouble( const double dValue )
{
	m_oOutgoing.WriteFloat64( dValue );
}

void CVANetMessage::WriteVec3( const VAVec3& oVec )
{
	WriteDouble( oVec.x );
	WriteDouble( oVec.y );
	WriteDouble( oVec.z );
}

void CVANetMessage::WriteQuat( const VAQuat& qOrient )
{
	WriteDouble( qOrient.x );
	WriteDouble( qOrient.y );
	WriteDouble( qOrient.z );
	WriteDouble( qOrient.w );
}

std::string CVANetMessage::ReadString( )
{
	VANetCompat::sint32 nSize;
	int nReturn = m_oIncoming.ReadInt32( nSize );
	assert( nReturn == sizeof( VANetCompat::sint32 ) );

	// Empty string?
	if( nSize == 0 )
		return "";

	std::string sValue;
	nReturn = m_oIncoming.ReadString( sValue, nSize );
	assert( nReturn == nSize );
	return sValue;
}

int CVANetMessage::ReadInt( )
{
	VANetCompat::sint32 nValue;
	int nReturn = m_oIncoming.ReadInt32( nValue );
	assert( nReturn == sizeof( VANetCompat::sint32 ) );
	return nValue;
}

uint64_t CVANetMessage::ReadUInt64( )
{
	VANetCompat::uint64 nValue;
	int nReturn = m_oIncoming.ReadUInt64( nValue );
	assert( nReturn == sizeof( VANetCompat::uint64 ) );
	return nValue;
}

bool CVANetMessage::ReadBool( )
{
	bool bValue;
	int nReturn = m_oIncoming.ReadBool( bValue );
	assert( nReturn == sizeof( bool ) );
	return bValue;
}

float CVANetMessage::ReadFloat( )
{
	float fValue;
	int nReturn = m_oIncoming.ReadFloat32( fValue );
	assert( nReturn == sizeof( float ) );
	return fValue;
}

double CVANetMessage::ReadDouble( )
{
	double dValue;
	int nReturn = m_oIncoming.ReadFloat64( dValue );
	assert( nReturn == sizeof( double ) );
	return dValue;
}

VAVec3 CVANetMessage::ReadVec3( )
{
	double x = ReadDouble( );
	double y = ReadDouble( );
	double z = ReadDouble( );
	return VAVec3( x, y, z );
}

VAQuat CVANetMessage::ReadQuat( )
{
	double x = ReadDouble( );
	double y = ReadDouble( );
	double z = ReadDouble( );
	double w = ReadDouble( );
	return VAQuat( x, y, z, w );
}

void CVANetMessage::WriteEvent( const CVAEvent& oEvent )
{
	WriteUInt64( (size_t)oEvent.pSender );
	WriteInt( oEvent.iEventID );
	WriteUInt64( oEvent.iEventType );
	WriteInt( oEvent.iObjectID );
	WriteString( oEvent.sObjectID );
	WriteInt( oEvent.iParamID );
	WriteString( oEvent.sParam );
	WriteInt( oEvent.iIndex );
	WriteInt( oEvent.iAuralizationMode );
	WriteDouble( oEvent.dVolume );
	WriteDouble( oEvent.dState );
	WriteBool( oEvent.bMuted );
	WriteString( oEvent.sName );
	WriteString( oEvent.sFilenPath );
	WriteVec3( oEvent.vPos );
	WriteVec3( oEvent.vView );
	WriteVec3( oEvent.vUp );
	WriteQuat( oEvent.qHATO );
	WriteQuat( oEvent.oOrientation );

	WriteInt( (int)oEvent.vfInputPeaks.size( ) );
	for( size_t i = 0; i < oEvent.vfInputPeaks.size( ); i++ )
		WriteFloat( oEvent.vfInputPeaks[i] );
	WriteInt( (int)oEvent.vfInputRMSs.size( ) );
	for( size_t i = 0; i < oEvent.vfInputRMSs.size( ); i++ )
		WriteFloat( oEvent.vfInputRMSs[i] );

	WriteInt( (int)oEvent.vfOutputPeaks.size( ) );
	for( size_t i = 0; i < oEvent.vfOutputPeaks.size( ); i++ )
		WriteFloat( oEvent.vfOutputPeaks[i] );
	WriteInt( (int)oEvent.vfOutputRMSs.size( ) );
	for( size_t i = 0; i < oEvent.vfOutputRMSs.size( ); i++ )
		WriteFloat( oEvent.vfOutputRMSs[i] );

	WriteFloat( oEvent.fSysLoad );
	WriteFloat( oEvent.fDSPLoad );
	WriteDouble( oEvent.dCoreClock );

	WriteInt( oEvent.oProgress.iCurrentStep );
	WriteInt( oEvent.oProgress.iMaxStep );
	WriteString( oEvent.oProgress.sAction );
	WriteString( oEvent.oProgress.sSubaction );

	WriteVAStruct( oEvent.oPrototypeParams );
}

CVAEvent CVANetMessage::ReadEvent( )
{
	CVAEvent oEvent;
	oEvent.pSender           = (IVAInterface*)ReadUInt64( );
	oEvent.iEventID          = ReadInt( );
	oEvent.iEventType        = ReadUInt64( );
	oEvent.iObjectID         = ReadInt( );
	oEvent.sObjectID         = ReadString( );
	oEvent.iParamID          = ReadInt( );
	oEvent.sParam            = ReadString( );
	oEvent.iIndex            = ReadInt( );
	oEvent.iAuralizationMode = ReadInt( );
	oEvent.dVolume           = ReadDouble( );
	oEvent.dState            = ReadDouble( );
	oEvent.bMuted            = ReadBool( );
	oEvent.sName             = ReadString( );
	oEvent.sFilenPath        = ReadString( );
	oEvent.vPos              = ReadVec3( );
	oEvent.vView             = ReadVec3( );
	oEvent.vUp               = ReadVec3( );
	oEvent.qHATO             = ReadQuat( );
	oEvent.oOrientation      = ReadQuat( );

	int n = ReadInt( );
	for( int i = 0; i < n; i++ )
		oEvent.vfInputPeaks.push_back( ReadFloat( ) );
	n = ReadInt( );
	for( int i = 0; i < n; i++ )
		oEvent.vfInputRMSs.push_back( ReadFloat( ) );
	n = ReadInt( );
	for( int i = 0; i < n; i++ )
		oEvent.vfOutputPeaks.push_back( ReadFloat( ) );
	n = ReadInt( );
	for( int i = 0; i < n; i++ )
		oEvent.vfOutputRMSs.push_back( ReadFloat( ) );

	oEvent.fSysLoad   = ReadFloat( );
	oEvent.fDSPLoad   = ReadFloat( );
	oEvent.dCoreClock = ReadDouble( );

	oEvent.oProgress.iCurrentStep = ReadInt( );
	oEvent.oProgress.iMaxStep     = ReadInt( );
	oEvent.oProgress.sAction      = ReadString( );
	oEvent.oProgress.sSubaction   = ReadString( );

	ReadVAStruct( oEvent.oPrototypeParams );

	return oEvent;
}


void CVANetMessage::WriteException( const CVAException& oException )
{
	WriteInt( oException.GetErrorCode( ) );
	WriteString( oException.GetErrorMessage( ) );
}

// Deserialize a VAException instance from a byte buffer (returns number of bytes read)
// (returns number of bytes read if successfull, otherwise -1)
CVAException CVANetMessage::ReadException( )
{
	const int nType = ReadInt( );
	return CVAException( nType, ReadString( ) );
}

void CVANetMessage::WriteVersionInfo( const CVAVersionInfo& oInfo )
{
	WriteString( oInfo.sVersion );
	WriteString( oInfo.sFlags );
	WriteString( oInfo.sDate );
	WriteString( oInfo.sComments );
}

CVAVersionInfo CVANetMessage::ReadVersionInfo( )
{
	CVAVersionInfo oInfo;
	oInfo.sVersion  = ReadString( );
	oInfo.sFlags    = ReadString( );
	oInfo.sDate     = ReadString( );
	oInfo.sComments = ReadString( );
	return oInfo;
}

void CVANetMessage::WriteDirectivityInfo( const CVADirectivityInfo& oInfo )
{
	WriteInt( oInfo.iID );
	WriteInt( oInfo.iClass );
	WriteString( oInfo.sName );
	WriteString( oInfo.sDesc );
	WriteInt( oInfo.iReferences );
	WriteVAStruct( oInfo.oParams );
}

CVADirectivityInfo CVANetMessage::ReadDirectivityInfo( )
{
	CVADirectivityInfo oInfo;
	oInfo.iID         = ReadInt( );
	oInfo.iClass      = ReadInt( );
	oInfo.sName       = ReadString( );
	oInfo.sDesc       = ReadString( );
	oInfo.iReferences = ReadInt( );
	ReadVAStruct( oInfo.oParams );
	return oInfo;
}

void CVANetMessage::WriteSignalSourceInfo( const CVASignalSourceInfo& oInfo )
{
	WriteString( oInfo.sID );
	WriteInt( oInfo.iType );
	WriteString( oInfo.sName );
	WriteString( oInfo.sDesc );
	WriteString( oInfo.sState );
	WriteInt( oInfo.iReferences );
}

CVASignalSourceInfo CVANetMessage::ReadSignalSourceInfo( )
{
	CVASignalSourceInfo oInfo;
	oInfo.sID         = ReadString( );
	oInfo.iType       = ReadInt( );
	oInfo.sName       = ReadString( );
	oInfo.sDesc       = ReadString( );
	oInfo.sState      = ReadString( );
	oInfo.iReferences = ReadInt( );
	return oInfo;
}

void CVANetMessage::WriteSceneInfo( const CVASceneInfo& oInfo )
{
	WriteString( oInfo.sID );
	WriteString( oInfo.sName );
	WriteBool( oInfo.bEnabled );
	WriteVAStruct( oInfo.oParams );
}

CVASceneInfo CVANetMessage::ReadSceneInfo( )
{
	CVASceneInfo oInfo;
	oInfo.sID      = ReadString( );
	oInfo.sName    = ReadString( );
	oInfo.bEnabled = ReadBool( );
	ReadVAStruct( oInfo.oParams );
	return oInfo;
}

void CVANetMessage::WriteModuleInfo( const CVAModuleInfo& oInfo )
{
	WriteString( oInfo.sName );
	WriteString( oInfo.sDesc );
	WriteVAStruct( oInfo.oParams );
}

void CVANetMessage::ReadModuleInfo( CVAModuleInfo& oInfo )
{
	oInfo.sName = ReadString( );
	oInfo.sDesc = ReadString( );
	ReadVAStruct( oInfo.oParams );
}

void CVANetMessage::WriteBlob( const void* pBuf, const int nBytes )
{
	m_oOutgoing.WriteRawBuffer( pBuf, nBytes );
}

void CVANetMessage::ReadBlob( void* pBuf, const int nBytes )
{
	int nReturn = m_oIncoming.ReadRawBuffer( pBuf, nBytes );
	assert( nReturn == nBytes );
}

void CVANetMessage::WriteVAStruct( const CVAStruct& oStruct )
{
	// Anzahl der Keys schreiben
	WriteInt( oStruct.Size( ) );

	int nBytes;
	for( CVAStruct::const_iterator cit = oStruct.Begin( ); cit != oStruct.End( ); ++cit )
	{
		// Schlüsselnamen
		WriteString( cit->first );

		// Wert-Typ
		int iDatatype = cit->second.GetDatatype( );
		WriteInt( iDatatype );

		// Wert
		switch( iDatatype )
		{
			case CVAStructValue::UNASSIGNED:
				// Keinen Wert => Nichts schreiben
				break;

			case CVAStructValue::BOOL:
				WriteBool( cit->second );
				break;

			case CVAStructValue::INT:
				WriteInt( cit->second );
				break;

			case CVAStructValue::DOUBLE:
				WriteDouble( cit->second );
				break;

			case CVAStructValue::STRING:
				WriteString( cit->second );
				break;

			case CVAStructValue::STRUCT:
				WriteVAStruct( cit->second.GetStruct( ) );
				break;

			case CVAStructValue::DATA:
				nBytes = cit->second.GetDataSize( );
				WriteInt( nBytes );
				WriteBlob( cit->second.GetData( ), nBytes );
				break;

			case CVAStructValue::SAMPLEBUFFER:
			{
				const CVASampleBuffer& oSampleBuffer( cit->second );
				WriteInt( oSampleBuffer.GetNumSamples( ) );
				for( int i = 0; i < oSampleBuffer.GetNumSamples( ); i++ )
					WriteFloat( oSampleBuffer.vfSamples[i] );
			}
			break;

			default:
				// Implementierung vergessen? Oder neuer Datentyp?
				VA_EXCEPT2( PROTOCOL_ERROR, "Unsupported key datatype" );
		}
	}
}

void CVANetMessage::ReadVAStruct( CVAStruct& oStruct )
{
	oStruct.Clear( );

	// Anzahl der Keys lesen
	int nKeys = ReadInt( );

	for( int i = 0; i < nKeys; i++ )
	{
		// Schlüsselnamen
		std::string sKeyName = ReadString( );

		// Wert-Typ
		int iDatatype = ReadInt( );

		// Wert
		switch( iDatatype )
		{
			case CVAStructValue::UNASSIGNED:
				// Keinen Wert => Leeren Schüssel
				oStruct[sKeyName];
				break;

			case CVAStructValue::BOOL:
				oStruct[sKeyName] = ReadBool( );
				break;

			case CVAStructValue::INT:
				oStruct[sKeyName] = ReadInt( );
				break;

			case CVAStructValue::DOUBLE:
				oStruct[sKeyName] = ReadDouble( );
				break;

			case CVAStructValue::STRING:
				oStruct[sKeyName] = ReadString( );
				break;

			case CVAStructValue::STRUCT:
			{
				CVAStruct oSubstruct;
				ReadVAStruct( oSubstruct );
				oStruct[sKeyName] = oSubstruct;
			}
			break;

			case CVAStructValue::DATA:
			{
				int iDataSize = ReadInt( );
				void* pData   = (void*)new char[iDataSize];
				try
				{
					ReadBlob( pData, iDataSize );
				}
				catch( ... )
				{
					delete[] pData;
					throw;
				}
				oStruct[sKeyName] = CVAStructValue( pData, iDataSize );
			}
			break;

			case CVAStructValue::SAMPLEBUFFER:
			{
				int iNumSamples   = ReadInt( );
				oStruct[sKeyName] = CVASampleBuffer( iNumSamples, false );
				CVASampleBuffer& oSampleBuffer( oStruct[sKeyName.c_str( )] );
				for( int j = 0; j < iNumSamples; j++ )
					oSampleBuffer.vfSamples[j] = ReadFloat( );
			}
			break;

			default:
				// Implementierung vergessen? Oder neuer Datentyp?
				VA_EXCEPT2( PROTOCOL_ERROR, "Structure contains unknown key datatype" );
		}
	}
}

VistaConnectionIP* CVANetMessage::GetConnection( ) const
{
	return m_pConnection;
}

void CVANetMessage::ClearConnection( )
{
	m_pConnection = NULL;
}

CVAAcousticMaterial CVANetMessage::ReadAcousticMaterial( )
{
	CVAAcousticMaterial oMaterial;
	oMaterial.iID   = ReadInt( );
	oMaterial.iType = ReadInt( );
	ReadVAStruct( oMaterial.oParams );
	oMaterial.sName = ReadString( );
	const size_t n  = ReadInt( );
	for( size_t i = 0; i < n; i++ )
		oMaterial.vfAbsorptionValues.push_back( ReadFloat( ) );
	const size_t o = ReadInt( );
	for( size_t i = 0; i < o; i++ )
		oMaterial.vfScatteringValues.push_back( ReadFloat( ) );
	const size_t p = ReadInt( );
	for( size_t i = 0; i < p; i++ )
		oMaterial.vfTransmissionValues.push_back( ReadFloat( ) );
	return oMaterial;
}

void CVANetMessage::WriteAcousticMaterial( const CVAAcousticMaterial& oMaterial )
{
	WriteInt( oMaterial.iID );
	WriteInt( oMaterial.iType );
	WriteVAStruct( oMaterial.oParams );
	WriteString( oMaterial.sName );
	WriteInt( (int)oMaterial.vfAbsorptionValues.size( ) );
	for( size_t i = 0; i < oMaterial.vfAbsorptionValues.size( ); i++ )
		WriteFloat( oMaterial.vfAbsorptionValues[i] );
	WriteInt( (int)oMaterial.vfScatteringValues.size( ) );
	for( size_t i = 0; i < oMaterial.vfScatteringValues.size( ); i++ )
		WriteFloat( oMaterial.vfScatteringValues[i] );
	WriteInt( (int)oMaterial.vfTransmissionValues.size( ) );
	for( size_t i = 0; i < oMaterial.vfTransmissionValues.size( ); i++ )
		WriteFloat( oMaterial.vfTransmissionValues[i] );
}

void CVANetMessage::WriteAudioRenderingModuleInfo( const CVAAudioRendererInfo& oRenderer )
{
	WriteString( oRenderer.sID );
	WriteString( oRenderer.sClass );
	WriteString( oRenderer.sDescription );
	WriteBool( oRenderer.bEnabled );
	WriteBool( oRenderer.bOutputDetectorEnabled );
	WriteBool( oRenderer.bOutputRecordingEnabled );
	WriteString( oRenderer.sOutputRecordingFilePath );
	WriteVAStruct( oRenderer.oParams );
}

CVAAudioRendererInfo CVANetMessage::ReadAudioRenderingModuleInfo( )
{
	CVAAudioRendererInfo oRenderer;
	oRenderer.sID                      = ReadString( );
	oRenderer.sClass                   = ReadString( );
	oRenderer.sDescription             = ReadString( );
	oRenderer.bEnabled                 = ReadBool( );
	oRenderer.bOutputDetectorEnabled   = ReadBool( );
	oRenderer.bOutputRecordingEnabled  = ReadBool( );
	oRenderer.sOutputRecordingFilePath = ReadString( );
	ReadVAStruct( oRenderer.oParams );
	return oRenderer;
}

void CVANetMessage::WriteAudioReproductionModuleInfo( const CVAAudioReproductionInfo& oRepro )
{
	WriteString( oRepro.sID );
	WriteString( oRepro.sClass );
	WriteString( oRepro.sDescription );
	WriteBool( oRepro.bEnabled );
	WriteBool( oRepro.bInputDetectorEnabled );
	WriteBool( oRepro.bInputRecordingEnabled );
	WriteString( oRepro.sInputRecordingFilePath );
	WriteBool( oRepro.bOutputDetectorEnabled );
	WriteBool( oRepro.bOutputRecordingEnabled );
	WriteString( oRepro.sOutputRecordingFilePath );
	WriteVAStruct( oRepro.oParams );
}

CVAAudioReproductionInfo CVANetMessage::ReadAudioReproductionModuleInfo( )
{
	CVAAudioReproductionInfo oRepro;
	oRepro.sID                      = ReadString( );
	oRepro.sClass                   = ReadString( );
	oRepro.sDescription             = ReadString( );
	oRepro.bEnabled                 = ReadBool( );
	oRepro.bInputDetectorEnabled    = ReadBool( );
	oRepro.bInputRecordingEnabled   = ReadBool( );
	oRepro.sInputRecordingFilePath  = ReadString( );
	oRepro.bOutputDetectorEnabled   = ReadBool( );
	oRepro.bOutputRecordingEnabled  = ReadBool( );
	oRepro.sOutputRecordingFilePath = ReadString( );
	ReadVAStruct( oRepro.oParams );
	return oRepro;
}

void CVANetMessage::WriteSoundSourceInfo( const CVASoundSourceInfo& oInfo )
{
	WriteBool( oInfo.bMuted );
	WriteDouble( oInfo.dSpoundPower );
	WriteInt( oInfo.iAuraMode );
	WriteInt( oInfo.iDirectivityID );
	WriteInt( oInfo.iID );
	WriteInt( oInfo.iSignalSourceID );
	WriteVAStruct( oInfo.oParams );
	WriteQuat( oInfo.qOrient );
	WriteString( oInfo.sName );
	WriteVec3( oInfo.v3Pos );
	WriteVec3( oInfo.v3Up );
	WriteVec3( oInfo.v3View );
}

CVASoundSourceInfo CVANetMessage::ReadSoundSourceInfo( )
{
	CVASoundSourceInfo oInfo;
	oInfo.bMuted          = ReadBool( );
	oInfo.dSpoundPower    = ReadDouble( );
	oInfo.iAuraMode       = ReadInt( );
	oInfo.iDirectivityID  = ReadInt( );
	oInfo.iID             = ReadInt( );
	oInfo.iSignalSourceID = ReadInt( );
	ReadVAStruct( oInfo.oParams );
	oInfo.qOrient = ReadQuat( );
	oInfo.sName   = ReadString( );
	oInfo.v3Pos   = ReadVec3( );
	oInfo.v3Up    = ReadVec3( );
	oInfo.v3View  = ReadVec3( );
	return oInfo;
}

void CVANetMessage::WriteSoundReceiverInfo( const CVASoundReceiverInfo& oInfo )
{
	WriteInt( oInfo.iID );
	WriteString( oInfo.sName );
	WriteInt( oInfo.iDirectivityID );
	WriteInt( oInfo.iAuraMode );
	WriteDouble( oInfo.dSensivitity );
	WriteBool( oInfo.bEnabled );
	WriteVec3( oInfo.v3Pos );
	WriteVec3( oInfo.v3View );
	WriteVec3( oInfo.v3Up );
	WriteQuat( oInfo.qOrient );
	WriteQuat( oInfo.qHeadAboveTorsoOrientation );
	WriteVec3( oInfo.v3RealWorldPos );
	WriteVec3( oInfo.v3RealWorldView );
	WriteVec3( oInfo.v3RealWorldUp );
	WriteQuat( oInfo.qRealWorldOrient );
	WriteQuat( oInfo.qRealWorldHeadAboveTorsoOrientation );
	WriteVAStruct( oInfo.oParams );
}

CVASoundReceiverInfo CVANetMessage::ReadSoundReceiverInfo( )
{
	CVASoundReceiverInfo oInfo;
	oInfo.iID                                 = ReadInt( );
	oInfo.sName                               = ReadString( );
	oInfo.iDirectivityID                      = ReadInt( );
	oInfo.iAuraMode                           = ReadInt( );
	oInfo.dSensivitity                        = ReadDouble( );
	oInfo.bEnabled                            = ReadBool( );
	oInfo.v3Pos                               = ReadVec3( );
	oInfo.v3View                              = ReadVec3( );
	oInfo.v3Up                                = ReadVec3( );
	oInfo.qOrient                             = ReadQuat( );
	oInfo.qHeadAboveTorsoOrientation          = ReadQuat( );
	oInfo.v3RealWorldPos                      = ReadVec3( );
	oInfo.v3RealWorldView                     = ReadVec3( );
	oInfo.v3RealWorldUp                       = ReadVec3( );
	oInfo.qRealWorldOrient                    = ReadQuat( );
	oInfo.qRealWorldHeadAboveTorsoOrientation = ReadQuat( );
	ReadVAStruct( oInfo.oParams );
	return oInfo;
}

void CVANetMessage::WriteSoundPortalInfo( const CVASoundPortalInfo& oInfo )
{
	WriteBool( oInfo.bEnabled );
	WriteInt( oInfo.iMaterialID );
	WriteInt( oInfo.iNextPortalID );
	WriteInt( oInfo.iSoundReceiverID );
	WriteInt( oInfo.iID );
	WriteInt( oInfo.iSoundSourceID );
	WriteVAStruct( oInfo.oParams );
	WriteQuat( oInfo.qOrient );
	WriteString( oInfo.sName );
	WriteVec3( oInfo.v3Pos );
	WriteVec3( oInfo.v3View );
	WriteVec3( oInfo.v3Up );
}

CVASoundPortalInfo CVANetMessage::ReadSoundPortalInfo( )
{
	CVASoundPortalInfo oInfo;
	oInfo.bEnabled         = ReadBool( );
	oInfo.iMaterialID      = ReadInt( );
	oInfo.iNextPortalID    = ReadInt( );
	oInfo.iSoundReceiverID = ReadInt( );
	oInfo.iID              = ReadInt( );
	oInfo.iSoundSourceID   = ReadInt( );
	ReadVAStruct( oInfo.oParams );
	oInfo.qOrient = ReadQuat( );
	oInfo.sName   = ReadString( );
	oInfo.v3Pos   = ReadVec3( );
	oInfo.v3View  = ReadVec3( );
	oInfo.v3Up    = ReadVec3( );
	return oInfo;
}

void CVANetMessage::WriteGeometryMesh( const CVAGeometryMesh& oMesh )
{
	WriteBool( oMesh.bEnabled );
	WriteInt( oMesh.iID );
	WriteVAStruct( oMesh.oParams );
	WriteInt( int( oMesh.voFaces.size( ) ) );
	for( size_t i = 0; i < oMesh.voFaces.size( ); i++ )
		WriteGeometryMeshFace( ( oMesh.voFaces[i] ) );
	WriteInt( int( oMesh.voVertices.size( ) ) );
	for( size_t i = 0; i < oMesh.voVertices.size( ); i++ )
		WriteGeometryMeshVertex( oMesh.voVertices[i] );
}

CVAGeometryMesh CVANetMessage::ReadGeometryMesh( )
{
	CVAGeometryMesh oMesh;
	oMesh.bEnabled = ReadBool( );
	oMesh.iID      = ReadInt( );
	ReadVAStruct( oMesh.oParams );
	const int iNumFaces = ReadInt( );
	for( int i = 0; i < iNumFaces; i++ )
		oMesh.voFaces.push_back( ReadGeometryMeshFace( ) );
	const int iNumVertices = ReadInt( );
	for( int i = 0; i < iNumVertices; i++ )
		oMesh.voVertices.push_back( ReadGeometryMeshVertex( ) );
	return oMesh;
}

void CVANetMessage::WriteGeometryMeshFace( const CVAGeometryMesh::CVAFace& oFace )
{
	WriteInt( oFace.iID );
	WriteInt( oFace.iMaterialID );
	WriteInt( int( oFace.viVertexList.size( ) ) );
	for( size_t i = 0; i < oFace.viVertexList.size( ); i++ )
		WriteInt( oFace.viVertexList[i] );
}

CVAGeometryMesh::CVAFace CVANetMessage::ReadGeometryMeshFace( )
{
	CVAGeometryMesh::CVAFace oFace;
	oFace.iID              = ReadInt( );
	oFace.iMaterialID      = ReadInt( );
	const int iNumVertices = ReadInt( );
	for( int i = 0; i < iNumVertices; i++ )
		oFace.viVertexList.push_back( ReadInt( ) );
	return oFace;
}

void CVANetMessage::WriteGeometryMeshVertex( const CVAGeometryMesh::CVAVertex& oVertex )
{
	WriteInt( oVertex.iID );
	WriteVec3( oVertex.v3Point );
}

CVAGeometryMesh::CVAVertex CVANetMessage::ReadGeometryMeshVertex( )
{
	CVAGeometryMesh::CVAVertex oVertex;
	oVertex.iID     = ReadInt( );
	oVertex.v3Point = ReadVec3( );
	return oVertex;
}
