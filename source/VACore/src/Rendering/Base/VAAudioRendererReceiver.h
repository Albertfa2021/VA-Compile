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

#ifndef IW_VACORE_AUDIORENDERER_RECEIVER
#define IW_VACORE_AUDIORENDERER_RECEIVER

// VA includes
#include "VAAudioRendererMovingObject.h"

#include <ITABufferedAudioFileWriter.h> //Forwarding is not enough when using a unique pointer
#include <ITASampleFrame.h>             //Forwarding is not enough when using a unique pointer


// VA forwards
class CVASoundReceiverDesc;


/// Receiver representation in rendering context
class CVARendererReceiver : public CVARendererMovingObject
{
public:
	const int iNumChannels;                                                       /// Number of output channels for respective renderer
	const int iBlockSize;                                                         /// Block size for audio processing
	CVASoundReceiverDesc* pData                                        = nullptr; /// (Unversioned) Receiver description
	std::unique_ptr<ITASampleFrame> psfOutput                          = nullptr; /// Accumulated listener output signals
	std::unique_ptr<ITABufferedAudiofileWriter> pOutputAudioFileWriter = nullptr; /// File writer used for dumping the listener signals


	CVARendererReceiver( const CVABasicMotionModel::Config& oConf_, int iNumChannels_, int iBlockSize_ )
	    : CVARendererMovingObject( oConf_ )
	    , iNumChannels( iNumChannels_ )
	    , iBlockSize( iBlockSize_ ) { };

	/// Pool-Constructor: Initializes output sample frame and calls CVARendererMovingObject::PreRequest()
	void PreRequest( )
	{
		pData = nullptr;
		CVARendererMovingObject::PreRequest( );

		pOutputAudioFileWriter = nullptr;
		psfOutput                      = std::make_unique<ITASampleFrame>( iNumChannels, iBlockSize, true );
	};
	/// Pool-Destructor: Deletes output sample frame and calls CVARendererMovingObject::PreRelease()
	void PreRelease( )
	{
		CVARendererMovingObject::PreRelease( );
		psfOutput = nullptr;
	};

	void InitDump( const std::string& sFilename )
	{
		// TODO: Do we still need that?
		// std::string sOutput( sFilename );
		// sOutput = SubstituteMacro( sOutput, "ListenerName", pData->sName );
		// sOutput = SubstituteMacro( sOutput, "ListenerID", IntToString( pData->iID ) );

		// ITAAudiofileProperties props;
		// props.dSampleRate              = pCore->GetCoreConfig( )->oAudioDriverConfig.dSampleRate;
		// props.eDomain                  = ITADomain::ITA_TIME_DOMAIN;
		// props.eQuantization            = ITAQuantization::ITA_FLOAT;
		// props.iChannels                = 2;
		// props.iLength                  = 0;
		// pOutputAudioFileWriter = std::make_unique<ITABufferedAudiofileWriter>( ITABufferedAudiofileWriter::create( props ) );
		// pOutputAudioFileWriter->SetFilePath( sOutput );
	}
	void FinalizeDump( ) { pOutputAudioFileWriter = nullptr; };
};

/// Factory for receivers in rendering context
class CVAReceiverPoolFactory : public IVAPoolObjectFactory
{
public:
	CVAReceiverPoolFactory( int iNumChannels, int iBlockSize, const CVABasicMotionModel::Config& oConf )
	    : m_iNumChannels( iNumChannels )
	    , m_iBlockSize( iBlockSize )
	    , m_oMotionModelConf( oConf ) { };

	CVAPoolObject* CreatePoolObject( ) { return new CVARendererReceiver( m_oMotionModelConf, m_iNumChannels, m_iBlockSize ); };

private:
	const int m_iNumChannels;
	const int m_iBlockSize;
	const CVABasicMotionModel::Config m_oMotionModelConf;
};

#endif // IW_VACORE_AUDIORENDERER_RECEIVER
