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

#include <VAEventHandlerGlobalLock.h>
#include <mutex>

// Implementation class
class CVAEventHandlerGlobalLockImpl : public IVAEventHandlerGlobalLock
{
public:
	mutable std::mutex m_oLock;

	inline void Lock( ) const { m_oLock.lock( ); };

	inline void Unlock( ) const { m_oLock.unlock( ); };

	inline CVAEventHandlerGlobalLockImpl( ) { };
	inline ~CVAEventHandlerGlobalLockImpl( ) { };
};

// Singleton instance
CVAEventHandlerGlobalLockImpl g_oEventHandlerGlobalLock;

IVAEventHandlerGlobalLock& IVAEventHandlerGlobalLock::GetInstance( )
{
	return g_oEventHandlerGlobalLock;
}
