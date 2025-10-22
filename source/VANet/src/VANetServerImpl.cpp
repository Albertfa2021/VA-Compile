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

#include "VANetServerImpl.h"

#include "VANetMessage.h"
#include "VANetNetworkProtocol.h"
#include "VANetVistaCompatibility.h"

#include <VA.h>
#include <VistaBase/VistaDefaultTimerImp.h>
#include <VistaBase/VistaExceptionBase.h>
#include <VistaBase/VistaTimerImp.h>
#include <VistaInterProcComm/AsyncIO/VistaIOHandleBasedMultiplexer.h>
#include <VistaInterProcComm/Concurrency/VistaMutex.h>
#include <VistaInterProcComm/Concurrency/VistaThreadLoop.h>
#include <VistaInterProcComm/Connections/VistaConnectionIP.h>
#include <VistaInterProcComm/IPNet/VistaTCPServer.h>
#include <VistaInterProcComm/IPNet/VistaTCPSocket.h>
#include <algorithm>
#include <cassert>


/***********************************************************************/
/**** class CConnectionAccepter                                     ****/
/***********************************************************************/

class CVANetServerImpl::CConnectionAccepter : public VistaThreadLoop
{
public:
	inline CConnectionAccepter( CVANetServerImpl* pParent, const std::string& sServerIP, const unsigned int iServerPort )
	    : VistaThreadLoop( )
	    , m_pParent( pParent )
	    , m_sServerIP( sServerIP )
	    , m_iServerPort( iServerPort )
	{
		SetThreadName( "VAServer::ConnectionAccepter" );

		m_pServer = new VistaTCPServer( m_sServerIP, m_iServerPort, 3 );
		if( m_pServer->GetIsValid( ) )
		{
#ifdef VANET_SERVER_VERBOSE
			std::cout << "VA Server: listening for client connections on IP " << m_sServerIP << ":" << m_iServerPort << std::endl;
#endif
		}
		else
		{
#ifdef VANET_SERVER_VERBOSE
			std::cerr << "VA Server: could not establish client connector socket on IP " << m_sServerIP << ":" << m_iServerPort << std::endl;
#endif
		}
	};

	inline ~CConnectionAccepter( ) { delete m_pServer; };

	inline bool GetIsValid( ) { return m_pServer->GetIsValid( ); };

	inline virtual bool LoopBody( )
	{
		if( m_pServer->GetIsValid( ) == false )
		{
			std::cerr << "VA Server: Connection Accepter failed - TCPServer is invalid" << std::endl;
			IndicateLoopEnd( );
			return false;
		}

		VistaTCPSocket* pSocket = m_pServer->GetNextClient( 3 );
		if( pSocket == NULL )
		{
			std::cerr << "VA Server: Connection Accepter failed - TCPServer returned invalid socket" << std::endl;
			IndicateLoopEnd( );
			return false;
		}

		VistaConnectionIP* pConnection = new VistaConnectionIP( pSocket );
		pConnection->SetIsBlocking( true );
		pConnection->SetByteorderSwapFlag( VistaSerializingToolset::DOES_NOT_SWAP_MULTIBYTE_VALUES );
		m_pParent->AddNewClient( pConnection );
		pConnection->Close( );
		delete pConnection;

		return false;
	};

	inline virtual void PreLoop( )
	{
#ifdef VANET_SERVER_VERBOSE
		std::cout << "VA Server: starting connection accepter thread\n" << std::flush;
#endif
	};

	inline virtual void PostLoop( )
	{
#ifdef VANET_SERVER_VERBOSE
		std::cout << "VA Server: stopping connection accepter thread\n" << std::flush;
#endif
		m_pServer->StopAccept( );
		delete m_pServer;
	};

private:
	VistaTCPServer* m_pServer;
	std::string m_sServerIP;
	unsigned int m_iServerPort;

	CVANetServerImpl* m_pParent;
};

/***********************************************************************/
/**** class CConnectionObserver                                     ****/
/***********************************************************************/

class CVANetServerImpl::CConnectionObserver : public VistaThreadLoop
{
public:
	CConnectionObserver( CVANetServerImpl* pParent, CVANetNetworkProtocol* pProtocol )
	    : VistaThreadLoop( )
	    , m_pParent( pParent )
	    , m_oConnMultiplexer( )
	    , m_iIDCounter( 2 ) // our minumum ticket number
	    , m_pProtocol( pProtocol )
	{
		SetThreadName( "VAServer::ConnectionObserver" );
	}

	inline bool AddObservedConnection( VistaConnectionIP* pConnection )
	{
		m_mapConnections[m_iIDCounter] = pConnection;
		if( m_oConnMultiplexer.AddMultiplexPoint( pConnection->GetConnectionWaitForDescriptor( ), m_iIDCounter,
		                                          VistaIOMultiplexer::eIODir( VistaIOMultiplexer::MP_IOALL ) ) == false )
		{
			m_mapConnections.erase( m_mapConnections.find( m_iIDCounter ) );
			return false;
		}
		++m_iIDCounter;
		return true;
	};

	inline bool RemoveObservedConnection( VistaConnectionIP* pConnection )
	{
		std::map<int, VistaConnectionIP*>::iterator itEntry = m_mapConnections.begin( );
		bool bFound                                         = false;
		while( itEntry != m_mapConnections.end( ) )
		{
			if( ( *itEntry ).second == pConnection )
			{
				bFound = true;
				break;
			}
			else
			{
				++itEntry;
			}
		}

		if( bFound == false )
			return false;

		if( m_oConnMultiplexer.RemMultiplexPointByTicket( ( *itEntry ).first ) == false )
			return false;

		m_mapConnections.erase( itEntry );
		return true;
	};

	inline bool LoopBody( )
	{
#ifdef VANET_SERVER_VERBOSE
		std::cout << "[ VANetServer ] Loop body enter" << std::endl;
#endif

		int nUpdateID = m_oConnMultiplexer.Demultiplex( );

#ifdef VANET_SERVER_VERBOSE
		std::cout << "[ VANetServer ] Received an update id " << nUpdateID << " and there are " << m_mapConnections.size( ) << " available connections" << std::endl;
#endif

		if( nUpdateID == 0 )
		{
			return false;
		}
		else if( nUpdateID < 0 )
		{
			std::cerr << "VA Server: Connection Observer encountered Demultiplex error" << std::endl;
			IndicateLoopEnd( );
			return false;
		}

		try
		{
			m_mapConnections[nUpdateID]->SetWaitForDescriptorEventSelectIsEnabled( false );
			if( m_pProtocol->ProcessMessageFromClient( m_mapConnections[nUpdateID] ) == false )
			{
				m_pParent->RemoveClient( m_mapConnections[nUpdateID], CVANetNetworkProtocol::VA_NET_SERVER_DISCONNECT );
			}
			else
			{
				if( m_mapConnections[nUpdateID] )
					m_mapConnections[nUpdateID]->SetWaitForDescriptorEventSelectIsEnabled( true );
			}
		}
#ifdef VANET_SERVER_VERBOSE
		catch( CVAException& e )
		{
			std::cerr << "VA Server: caught exception and will disconnect now (" << e << ")" << std::endl;
			m_pParent->RemoveClient( m_mapConnections[nUpdateID], CVANetNetworkProtocol::VA_NET_SERVER_DISCONNECT );
		}
		catch( ... )
		{
#	ifdef VANET_SERVER_VERBOSE
			std::cout << "[ VANetServer ] Caught unkown exception." << std::endl;
#	endif
		}
#else
		catch( CVAException& )
		{
			m_pParent->RemoveClient( m_mapConnections[nUpdateID], CVANetNetworkProtocol::VA_NET_SERVER_DISCONNECT );
		}
#endif
		return false;
	};

	inline void PreLoop( )
	{
#ifdef VANET_SERVER_VERBOSE
		std::cout << "VA Server: starting connection update loop" << std::endl;
#endif
	};

	inline void PostRun( )
	{
#ifdef VANET_SERVER_VERBOSE
		std::cout << "VA Server: starting connection update loop" << std::endl;
#endif
		m_oConnMultiplexer.Shutdown( );
	};

private:
	int m_iIDCounter;
	std::map<int, VistaConnectionIP*> m_mapConnections;
	CVANetServerImpl* m_pParent;
	VistaIOHandleBasedIOMultiplexer m_oConnMultiplexer;
	CVANetNetworkProtocol* m_pProtocol;
};

/***********************************************************************/
/**** class CEventHandler                                           ****/
/***********************************************************************/

class CVANetServerImpl::CEventHandler : public IVAEventHandler
{
public:
	inline CEventHandler( CVANetServerImpl* pParent, CVANetNetworkProtocol* pProtocol, VistaMutex* pEventChannelLock, IVAInterface* pRealCore )
	    : IVAEventHandler( )
	    , m_pProtocol( pProtocol )
	    , m_pEventChannelLock( pEventChannelLock )
	    , m_pRealCore( pRealCore )
	    , m_pParent( pParent )
	    , m_pMessage( new CVANetMessage( VistaSerializingToolset::DOES_NOT_SWAP_MULTIBYTE_VALUES ) ) { };

	inline ~CEventHandler( ) { m_pRealCore->DetachEventHandler( this ); };

	inline bool AddClientConnection( VistaConnectionIP* pConnection )
	{
		VistaMutexLock oLock( *m_pEventChannelLock );
		std::vector<VistaConnectionIP*>::iterator itEntry = std::find( m_vecRegisteredClients.begin( ), m_vecRegisteredClients.end( ), pConnection );
		if( itEntry != m_vecRegisteredClients.end( ) )
			return false;

		if( m_vecRegisteredClients.empty( ) )
			m_pRealCore->AttachEventHandler( this );

		m_vecRegisteredClients.push_back( pConnection );
		return true;
	};

	inline bool RemoveClientConnection( VistaConnectionIP* pConnection )
	{
		VistaMutexLock oLock( *m_pEventChannelLock );
		std::vector<VistaConnectionIP*>::iterator itEntry = std::find( m_vecRegisteredClients.begin( ), m_vecRegisteredClients.end( ), pConnection );
		if( itEntry == m_vecRegisteredClients.end( ) )
			return false;
		m_vecRegisteredClients.erase( itEntry );

		if( m_vecRegisteredClients.empty( ) )
			m_pRealCore->DetachEventHandler( this );
		return true;
	};

	inline void HandleVAEvent( const CVAEvent* pEvent )
	{
		VistaMutexLock oLock( *m_pEventChannelLock );
		m_pProtocol->ServerPrepareEventMessage( pEvent, m_pMessage );
		std::list<VistaConnectionIP*> liDeadConnections;
		for( std::vector<VistaConnectionIP*>::iterator itCon = m_vecRegisteredClients.begin( ); itCon != m_vecRegisteredClients.end( ); ++itCon )
		{
			try
			{
				m_pMessage->SetConnection( ( *itCon ) );
				m_pMessage->WriteMessage( );
			}
			catch( VistaExceptionBase& oException )
			{
				std::cerr << "VANetServer: caught connection exception - " << oException.GetExceptionText( ) << std::endl;
				liDeadConnections.push_back( ( *itCon ) );
			}
			catch( CVAException& oException )
			{
				std::cerr << "VANetServer: caught connection exception - " << oException.ToString( ) << std::endl;
				liDeadConnections.push_back( ( *itCon ) );
			}
		}
		for( std::list<VistaConnectionIP*>::iterator itDead = liDeadConnections.begin( ); itDead != liDeadConnections.end( ); ++itDead )
		{
			m_pParent->RemoveClient( ( *itDead ), CVANetNetworkProtocol::VA_NET_SERVER_DISCONNECT );
		}

		if( pEvent->iEventID == CVAEvent::DESTROY )
		{
			// Important: Core instance was deleted. We should disable the server now
			// TODO: Implement
		}
	};

private:
	CVANetServerImpl* m_pParent;
	VistaMutex* m_pEventChannelLock;
	std::vector<VistaConnectionIP*> m_vecRegisteredClients;
	CVANetNetworkProtocol* m_pProtocol;
	CVANetMessage* m_pMessage;
	IVAInterface* m_pRealCore;
};

/***********************************************************************/
/**** class CVANetServerImpl                                 ****/
/***********************************************************************/

CVANetServerImpl::CVANetServerImpl( )
    : m_pConnectionAccepter( NULL )
    , m_pProtocol( NULL )
    , m_pEventHandler( NULL )
    , m_pConnObserver( NULL )
    , m_pEventChannelLock( new VistaMutex )
    , m_pServerChangeLock( new VistaMutex )
    , m_pEventHandlerLock( new VistaMutex )
    , m_iServerPort( -1 )
    , m_bRunning( false )
    , m_bInitialized( false )
    , m_iMaxNumClients( -1 )
    , m_pRealCore( NULL )
{
}

CVANetServerImpl::~CVANetServerImpl( )
{
	Finalize( );
	delete m_pEventChannelLock;
	delete m_pServerChangeLock;
	delete m_pEventHandlerLock;
}

int CVANetServerImpl::Initialize( const std::string& sInterface, int iServerPort, int iFreePortMin, int iFreePortMax, int iMaxNumClients )
{
	assert( iServerPort > 0 );
	assert( ( iFreePortMin > 0 ) && ( iFreePortMax > 0 ) && ( iFreePortMin <= iFreePortMax ) );

	// Create the port list
	tPortList liFreePorts;
	liFreePorts.push_back( tPortRange( iFreePortMin, iFreePortMax ) );

	return Initialize( sInterface, iServerPort, liFreePorts, iMaxNumClients );
}

int CVANetServerImpl::Initialize( const std::string& sInterface, int iServerPort, const tPortList& liFreePorts, int iMaxNumClients )
{
	VistaMutexLock oLock( *m_pServerChangeLock );

	m_pProtocol = new CVANetNetworkProtocol;

	m_iMaxNumClients = iMaxNumClients;

	if( m_bInitialized )
		return VA_ALREADY_INITIALIZED;

	if( IVistaTimerImp::GetSingleton( ) == NULL )
		IVistaTimerImp::SetSingleton( new VistaDefaultTimerImp );

	m_sServerIP   = sInterface;
	m_iServerPort = iServerPort;
	m_liFreePorts = liFreePorts;

	m_pConnectionAccepter = new CConnectionAccepter( this, m_sServerIP, iServerPort );
	if( m_pConnectionAccepter->GetIsValid( ) == false )
	{
		delete m_pConnectionAccepter;
		return VA_CONNECTION_FAILED;
	}

	m_pConnObserver = new CConnectionObserver( this, m_pProtocol );

	m_pProtocol->SetRealVACore( m_pRealCore );
	m_pProtocol->InitializeAsServer( this );

	m_pConnectionAccepter->Run( );
	m_pConnObserver->Run( );

	m_bInitialized = true;

	return VA_NO_ERROR;
}

int CVANetServerImpl::Finalize( )
{
	VistaMutexLock oLock( *m_pServerChangeLock );
	if( m_bInitialized == false )
		return VA_NOT_INITIALIZED;

#ifdef VANET_SERVER_VERBOSE
	std::cout << "VANetServer: Shutting down" << std::endl;
#endif

	if( m_pConnectionAccepter )
		m_pConnectionAccepter->Abort( );
	delete m_pConnectionAccepter;
	m_pConnectionAccepter = NULL;

	oLock.Unlock( );
	while( m_vecClients.empty( ) == false )
	{
		RemoveClient( 0, CVANetNetworkProtocol::VA_NET_SERVER_CLOSE );
	}
	oLock.Lock( );

	if( m_pEventHandler )
		m_pRealCore->DetachEventHandler( m_pEventHandler );
	delete m_pEventHandler;
	m_pEventHandler = NULL;

	/** @todo: detach */

	delete m_pProtocol;
	m_pProtocol = NULL;

	m_pConnObserver->Abort( );
	delete m_pConnObserver;
	m_pConnObserver = NULL;

	m_bRunning     = false;
	m_bInitialized = false;

	return VA_NO_ERROR;
}

void CVANetServerImpl::Reset( )
{
	VistaMutexLock oLock( *m_pServerChangeLock );
#ifdef VANET_SERVER_VERBOSE
	std::cout << "VANetServer: Resetting" << std::endl;
#endif

	while( m_vecClients.empty( ) == false )
	{
		RemoveClient( 0, CVANetNetworkProtocol::VA_NET_SERVER_RESET );
	}

	/** @todo: reset core? */
}

std::string CVANetServerImpl::GetServerAdress( ) const
{
	return m_sServerIP;
}

int CVANetServerImpl::GetServerPort( ) const
{
	return m_iServerPort;
}

IVAInterface* CVANetServerImpl::GetCoreInstance( ) const
{
	return m_pRealCore;
}

void CVANetServerImpl::SetCoreInstance( IVAInterface* pCore )
{
	m_pRealCore = pCore;
	if( m_pProtocol )
		m_pProtocol->SetRealVACore( pCore );
}

bool CVANetServerImpl::IsClientConnected( ) const
{
	return ( m_vecClients.empty( ) == false );
}

int CVANetServerImpl::GetNumConnectedClients( ) const
{
	return (int)m_vecClients.size( );
}

std::string CVANetServerImpl::GetClientHostname( const int iClientIndex ) const
{
	VistaMutexLock oLock( *m_pServerChangeLock );
	if( iClientIndex < 0 || iClientIndex > (int)m_vecClients.size( ) )
		return "";
	return m_vecClients[iClientIndex].m_sHost;
}

bool CVANetServerImpl::AttachEventHandler( IEventHandler* pHandler )
{
	VistaMutexLock oLock( *m_pEventChannelLock );
	std::list<IEventHandler*>::iterator itEntry = std::find( m_liEventHandlers.begin( ), m_liEventHandlers.end( ), pHandler );
	if( itEntry != m_liEventHandlers.end( ) )
		return false;
	m_liEventHandlers.push_back( pHandler );
	return true;
}
bool CVANetServerImpl::DetachEventHandler( IEventHandler* pHandler )
{
	VistaMutexLock oLock( *m_pEventChannelLock );
	std::list<IEventHandler*>::iterator itEntry = std::find( m_liEventHandlers.begin( ), m_liEventHandlers.end( ), pHandler );
	if( itEntry == m_liEventHandlers.end( ) )
		return false;
	m_liEventHandlers.erase( itEntry );
	return true;
}

int CVANetServerImpl::AddNewClient( VistaConnectionIP* pTemporaryConnection )
{
	VistaMutexLock oLock( *m_pServerChangeLock );

	pTemporaryConnection->WriteInt32( 1 ); // we write an initial defined value
	// to check if there's an endianess difference

	// we write our protocol to ensure that it is the same
	pTemporaryConnection->WriteInt32( (VANetCompat::sint32)CVANetNetworkProtocol::VA_NET_PROTOCOL_VERSION_MAJOR );
	pTemporaryConnection->WriteInt32( (VANetCompat::sint32)CVANetNetworkProtocol::VA_NET_PROTOCOL_VERSION_MINOR );

	bool bVersionMatch = false;
	pTemporaryConnection->ReadBool( bVersionMatch );
	if( bVersionMatch == false )
		return VA_CONNECTION_FAILED;

	CClientData oNewData;

	if( m_iMaxNumClients >= 0 && (int)m_vecClients.size( ) >= m_iMaxNumClients )
	{
		std::cerr << "VA Server: Client applied, but too many clients already connected" << std::endl;
		pTemporaryConnection->WriteInt32( -2 );
		return VA_CONNECTION_FAILED;
	}

	VANetCompat::sint32 iCommandPort = GetNextFreePort( );
	if( iCommandPort == -1 )
	{
		std::cerr << "VA Server: Client applied, but no more free ports available" << std::endl;
		pTemporaryConnection->WriteInt32( -1 );
		return VA_CONNECTION_FAILED;
	}

	VistaTCPServer oCommandServer( m_sServerIP, iCommandPort );
	if( oCommandServer.GetIsValid( ) == false )
	{
		std::cerr << "VA Server: Client applied, but could not open "
		          << "command socket on port [" << iCommandPort << "]" << std::endl;
		pTemporaryConnection->WriteInt32( -1 );
		return VA_CONNECTION_FAILED;
	}

	pTemporaryConnection->WriteInt32( iCommandPort );

	VistaTCPSocket* pCommandSocket = oCommandServer.GetNextClient( );
	oCommandServer.StopAccept( );
	if( pCommandSocket == NULL )
	{
		std::cerr << "VA Server: Client applied, but did not connect to "
		          << "command socket on port [" << iCommandPort << "]" << std::endl;
		return VA_CONNECTION_FAILED;
	}
	oNewData.m_pCommandChannel = new VistaConnectionIP( pCommandSocket );
	oNewData.m_pCommandChannel->SetIsBlocking( true );
	oNewData.m_pCommandChannel->SetByteorderSwapFlag( VistaSerializingToolset::DOES_NOT_SWAP_MULTIBYTE_VALUES );
#ifdef VANET_SERVER_SHOW_RAW_TRAFFIC
	oNewData.m_pCommandChannel->SetShowRawSendAndReceive( true );
#else
	oNewData.m_pCommandChannel->SetShowRawSendAndReceive( false );
#endif

	// check for optional head connection
	VANetCompat::sint32 iHeadChannelMode;
	pTemporaryConnection->ReadInt32( iHeadChannelMode );
	if( iHeadChannelMode == 0 )
	{
		// no separate head connection
		oNewData.m_pHeadChannel = NULL; // only created upon client request
	}
	else if( iHeadChannelMode == 1 )
	{
		// separate TCP connection
		VANetCompat::sint32 iHeadPort = GetNextFreePort( );
		VistaTCPServer oHeadServer( m_sServerIP, iHeadPort );
		if( oCommandServer.GetIsValid( ) == false )
		{
			std::cerr << "VA Server: Client applied, but could not open "
			          << "head socket on port [" << iHeadPort << "]" << std::endl;
			pTemporaryConnection->WriteInt32( -1 );
			delete oNewData.m_pCommandChannel;
			oNewData.m_pCommandChannel = NULL;
			return VA_CONNECTION_FAILED;
		}
		pTemporaryConnection->WriteInt32( iHeadPort );
		VistaTCPSocket* pHeadSocket = oHeadServer.GetNextClient( );
		oHeadServer.StopAccept( );
		if( pHeadSocket == NULL )
		{
			std::cerr << "VA Server: Client applied, but did not connect to "
			          << "head socket on port [" << iHeadPort << "]" << std::endl;
			delete oNewData.m_pCommandChannel;
			oNewData.m_pCommandChannel = NULL;
			return VA_CONNECTION_FAILED;
		}
		oNewData.m_pHeadChannel = new VistaConnectionIP( pHeadSocket );
	}
	else if( iHeadChannelMode == 2 )
	{
		// separate UDP connection
		VANetCompat::sint32 iHeadPort = GetNextFreePort( );
		oNewData.m_pHeadChannel       = new VistaConnectionIP( m_sServerIP, iHeadPort );
		if( oNewData.m_pHeadChannel->GetIsConnected( ) == false )
		{
			std::cerr << "VA Server: Client applied, but could not open "
			          << "UDP receiver on port [" << iHeadPort << "]" << std::endl;
			pTemporaryConnection->WriteInt32( -1 );
			delete oNewData.m_pHeadChannel;
			oNewData.m_pHeadChannel = NULL;
			delete oNewData.m_pCommandChannel;
			oNewData.m_pCommandChannel = NULL;
			return VA_CONNECTION_FAILED;
		}
		pTemporaryConnection->WriteInt32( iHeadPort );
	}

	// Create Event Channel - while the client may not necessarily want to receive
	// core events/progress updatzes, we still need this to pass other net-specific
	// events like server close/reset

	VANetCompat::sint32 iEventPort = GetNextFreePort( );
	if( iEventPort == -1 )
	{
		std::cerr << "VA Server: Client applied, but no more free ports available" << std::endl;
	}

	VistaTCPServer oEventServer( m_sServerIP, iEventPort );
	if( oEventServer.GetIsValid( ) == false )
	{
		std::cerr << "VA Server: Client applied, but could not open "
		          << "event socket on port [" << iEventPort << "]" << std::endl;
		pTemporaryConnection->WriteInt32( -1 );
		delete oNewData.m_pHeadChannel;
		oNewData.m_pHeadChannel = NULL;
		delete oNewData.m_pCommandChannel;
		oNewData.m_pCommandChannel = NULL;
		return VA_CONNECTION_FAILED;
	}

	pTemporaryConnection->WriteInt32( iEventPort );

	VistaTCPSocket* pEventSocket = oEventServer.GetNextClient( );
	oEventServer.StopAccept( );
	if( pEventSocket == NULL )
	{
		std::cerr << "VA Server: Client applied, but did not connect to "
		          << "event socket on port [" << iEventPort << "]" << std::endl;
		delete oNewData.m_pHeadChannel;
		oNewData.m_pHeadChannel = NULL;
		delete oNewData.m_pCommandChannel;
		oNewData.m_pCommandChannel = NULL;
		return VA_CONNECTION_FAILED;
	}
	oNewData.m_pEventChannel = new VistaConnectionIP( pEventSocket );
	oNewData.m_pEventChannel->SetIsBlocking( true );
	oNewData.m_pEventChannel->SetByteorderSwapFlag( VistaSerializingToolset::DOES_NOT_SWAP_MULTIBYTE_VALUES );

	m_pConnObserver->AddObservedConnection( oNewData.m_pCommandChannel );
	if( oNewData.m_pHeadChannel )
	{
		m_pConnObserver->AddObservedConnection( oNewData.m_pHeadChannel );
		oNewData.m_pHeadChannel->SetIsBlocking( true );
		oNewData.m_pHeadChannel->SetByteorderSwapFlag( VistaSerializingToolset::DOES_NOT_SWAP_MULTIBYTE_VALUES );
	}

	oNewData.m_sHost = pTemporaryConnection->GetPeerName( );
	m_vecClients.push_back( oNewData );

	CEvent oEvent( CEvent::EVENT_CLIENT_CONNECT, oNewData.m_sHost );
	EmitEvent( oEvent );

	return VA_NO_ERROR;
}

int CVANetServerImpl::GetNextFreePort( )
{
	/** @todo: race - although unlikely */
	if( m_liFreePorts.empty( ) )
		return -1;

	int iPort = m_liFreePorts.front( ).first;
	++m_liFreePorts.front( ).first;
	if( m_liFreePorts.front( ).first > m_liFreePorts.front( ).second )
		m_liFreePorts.pop_front( );

	return iPort;
}

bool CVANetServerImpl::AttachCoreEventHandler( VistaConnectionIP* pConnection )
{
	VistaMutexLock oLock( *m_pServerChangeLock );
	// first, we look for the client entry
	std::vector<CClientData>::iterator itClient;
	for( itClient = m_vecClients.begin( ); itClient != m_vecClients.end( ); ++itClient )
	{
		if( ( *itClient ).m_pCommandChannel == pConnection )
			break;
	}
	if( itClient == m_vecClients.end( ) )
		return false;

	if( m_pEventHandler == NULL )
		m_pEventHandler = new CEventHandler( this, m_pProtocol, m_pEventChannelLock, m_pRealCore );
	assert( ( *itClient ).m_pEventChannel != NULL );
	m_pEventHandler->AddClientConnection( ( *itClient ).m_pEventChannel );
	return true;
}

bool CVANetServerImpl::DetachCoreEventHandler( VistaConnectionIP* pConnection )
{
	VistaMutexLock oLock( *m_pServerChangeLock );
	// first, we look for the client entry
	std::vector<CClientData>::iterator itClient;
	for( itClient = m_vecClients.begin( ); itClient != m_vecClients.end( ); ++itClient )
	{
		if( ( *itClient ).m_pCommandChannel == pConnection )
			break;
	}
	if( itClient == m_vecClients.end( ) )
		return false;

	assert( ( *itClient ).m_pEventChannel != NULL );
	m_pEventHandler->RemoveClientConnection( ( *itClient ).m_pEventChannel );
	return true;
}

void CVANetServerImpl::RemoveClient( VistaConnectionIP* pConnection, int iStatusCode )
{
	std::vector<CClientData>::iterator itClient;
	int iID = 0;
	for( itClient = m_vecClients.begin( ); itClient != m_vecClients.end( ); ++itClient, ++iID )
	{
		if( ( *itClient ).m_pCommandChannel == pConnection || ( *itClient ).m_pEventChannel == pConnection || ( *itClient ).m_pHeadChannel == pConnection )
		{
			RemoveClient( iID, iStatusCode );
			return;
		}
	}
}

void CVANetServerImpl::RemoveClient( int iClientID, int iStatusCode )
{
	VistaMutexLock oServerLock( *m_pServerChangeLock );
	VistaMutexLock oEventChannelLock( *m_pEventChannelLock );
	VistaMutexLock oHandlerLock( *m_pEventHandlerLock );

	if( m_vecClients.size( ) == 0 )
		return; // Clients already gone ...

	std::vector<CClientData>::iterator itClient = m_vecClients.begin( ) + iClientID;

#ifdef VANET_SERVER_VERBOSE
	std::cout << "VANetServer: Removing client [" << ( *itClient ).m_sHost << "]" << std::endl;
#endif

	if( iStatusCode != 0 && ( *itClient ).m_pEventChannel && ( *itClient ).m_pEventChannel->GetIsConnected( ) )
	{
		try
		{
			CVANetMessage oMessage( ( *itClient ).m_pEventChannel->GetByteorderSwapFlag( ) );
			oMessage.SetConnection( ( *itClient ).m_pEventChannel );
			oMessage.SetMessageType( CVANetNetworkProtocol::VA_EVENT_NET_EVENT );
			oMessage.WriteInt( iStatusCode );
			oMessage.WriteMessage( );
			( *itClient ).m_pEventChannel->WaitForSendFinish( );
		}
		catch( VistaExceptionBase& )
		{
			// nothing tho do, we're already cleaning up
		}
		catch( CVAException& )
		{
			// nothing tho do, we're already cleaning up
		}
	}

	// we assume that all invalidated connections have been closed, but not deleted before

	// first, we try to send the client the close command
	if( ( *itClient ).m_pCommandChannel != NULL )
	{
		m_pConnObserver->RemoveObservedConnection( ( *itClient ).m_pCommandChannel );
		m_pProtocol->HandleConnectionClose( ( *itClient ).m_pCommandChannel );
		if( ( *itClient ).m_pCommandChannel->GetIsConnected( ) && ( *itClient ).m_pCommandChannel->GetIsOpen( ) )
		{
			( *itClient ).m_pCommandChannel->Close( );
		}
		delete( *itClient ).m_pCommandChannel;
	}

	if( ( *itClient ).m_pHeadChannel != NULL )
	{
		m_pConnObserver->RemoveObservedConnection( ( *itClient ).m_pHeadChannel );
		m_pConnObserver->RemoveObservedConnection( ( *itClient ).m_pHeadChannel );
		if( ( *itClient ).m_pHeadChannel->GetIsConnected( ) && ( *itClient ).m_pHeadChannel->GetIsOpen( ) )
		{
			( *itClient ).m_pHeadChannel->Close( );
		}
		delete( *itClient ).m_pHeadChannel;
	}

	if( ( *itClient ).m_pEventChannel != NULL )
	{
		if( m_pEventHandler )
			m_pEventHandler->RemoveClientConnection( ( *itClient ).m_pEventChannel );
		m_pConnObserver->RemoveObservedConnection( ( *itClient ).m_pEventChannel );
		if( ( *itClient ).m_pEventChannel->GetIsConnected( ) && ( *itClient ).m_pEventChannel->GetIsOpen( ) )
		{
			( *itClient ).m_pEventChannel->Close( );
		}
		delete( *itClient ).m_pEventChannel;
	}

	CEvent oEvent( CEvent::EVENT_CLIENT_DISCONNECT, ( *itClient ).m_sHost );
	m_vecClients.erase( itClient );

	// to avoid deadlocks, we unlock before emitting the event
	oServerLock.Unlock( );
	oHandlerLock.Unlock( );
	oEventChannelLock.Unlock( );
	EmitEvent( oEvent );
}

void CVANetServerImpl::EmitEvent( const CEvent& oEvent )
{
	if( m_liEventHandlers.empty( ) )
		return;
	VistaMutexLock oLock( *m_pEventChannelLock );
	for( std::list<IEventHandler*>::iterator itHandler = m_liEventHandlers.begin( ); itHandler != m_liEventHandlers.end( ); ++itHandler )
	{
		( *itHandler )->HandleEvent( oEvent );
	}
}

void CVANetServerImpl::HandleClientEvent( int iEventID, VistaConnectionIP* pConnection )
{
	switch( iEventID )
	{
		case CVANetNetworkProtocol::VA_NET_CLIENT_CLOSE:
		{
			RemoveClient( pConnection );
			break;
		}
		default:
		{
#ifdef VANET_SERVER_VERBOSE
			std::cout << "VANetServer: Event from client with unknown ID " << iEventID << std::endl;
#endif
			break;
		}
	}
}
