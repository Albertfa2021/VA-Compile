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

#ifndef IW_VACORE_AMBIENT_MIXER_AUDIO_RENDERER
#define IW_VACORE_AMBIENT_MIXER_AUDIO_RENDERER

#ifdef VACORE_WITH_RENDERER_AMBIENT_MIXER

// VA includes
#	include "../../VAAudioRenderer.h"
#	include "../../VAAudioRendererRegistry.h"

#	include <VA.h>

// ITA includes
#	include <ITADataSourceRealization.h>
#	include <ITAStreamInfo.h>

// STL
#	include <atomic>


// Forward declarations
class ITASoundSampler;

//! Ambient Mixer Audio Renderer
/**
 * The mixer for ambient sound either routes all sound sources with
 * a signal (that are not explicitly used by another renderer) to
 * the output. It applies the gains in the chain but does
 * not consider any auralization modes. It also provides an ambient
 * sound sampler to load multi-track samples and control the playback
 * via the parameter setter method.
 *
 */
class CVAAmbientMixerAudioRenderer
    : public IVAAudioRenderer
    , ITADatasourceRealizationEventHandler
{
public:
	CVAAmbientMixerAudioRenderer( const CVAAudioRendererInitParams& );
	virtual ~CVAAmbientMixerAudioRenderer( );

	void Reset( );
	inline void LoadScene( const std::string& ) { };
	void UpdateScene( CVASceneState* );

	inline void UpdateGlobalAuralizationMode( int ) { };

	void SetParameters( const CVAStruct& );
	CVAStruct GetParameters( const CVAStruct& ) const;

	ITADatasource* GetOutputDatasource( );

	void HandleProcessStream( ITADatasourceRealization*, const ITAStreamInfo* );
	inline void HandlePostIncrementBlockPointer( ITADatasourceRealization* ) { };

	CVAStruct GetInfo( ) const;

private:
	const CVAAudioRendererInitParams m_oParams; //!< Create a const copy of the init params

	ITADatasourceRealization* m_pDataSource; //!< Data source of the ambient mixer
	ITASoundSampler* m_pSampler;             //!< Multi-channel multi-track ambient sound sampler
	ITAStreamInfo oSamplerStreamInfo;        //!< Stream info

	bool m_bSamplerEnabled;            //!< Flag for activated sampler in config
	bool m_bSignalSourceMixingEnabled; //!< Flag for activated signal mixing in config

	CVASceneState* m_pNewSceneState;
	CVASceneState* m_pCurSceneState;
	std::atomic<bool> m_bIndicateReset;
	std::atomic<bool> m_bResetAck;

	inline CVAAmbientMixerAudioRenderer operator=( const CVAAmbientMixerAudioRenderer& ) { VA_EXCEPT_NOT_IMPLEMENTED; };
};

#endif // VACORE_WITH_RENDERER_AMBIENT_MIXER

#endif // IW_VACORE_AMBIENT_MIXER_AUDIO_RENDERER
