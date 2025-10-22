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

#ifndef IW_VACORE_AUDIORENDERER_SOURCE
#define IW_VACORE_AUDIORENDERER_SOURCE

// VA includes
#include "VAAudioRendererMovingObject.h"

// VA forwards
class CVASoundSourceDesc;

/// Source representation in rendering context
class CVARendererSource : public CVARendererMovingObject
{
public:
	CVASoundSourceDesc* pData = nullptr; //!< (Unversioned) Source description

	CVARendererSource( const CVABasicMotionModel::Config& oConf_ ) : CVARendererMovingObject( oConf_ ) { };

	/// Pool-Constructor
	virtual void PreRequest( ) override
	{
		CVARendererMovingObject::PreRequest( );
		pData = nullptr;
	};
	/// Pool-Destructor
	virtual void PreRelease( ) override { CVARendererMovingObject::PreRelease( ); };
};

/// Factory for sources in rendering context
class CVASourcePoolFactory : public IVAPoolObjectFactory
{
public:
	CVASourcePoolFactory( const CVABasicMotionModel::Config& oConf ) : m_oMotionModelConf( oConf ) { };

	CVAPoolObject* CreatePoolObject( ) { return new CVARendererSource( m_oMotionModelConf ); };

private:
	const CVABasicMotionModel::Config m_oMotionModelConf;
};

#endif // IW_VACORE_AUDIORENDERER_SOURCE
