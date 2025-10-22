target_sources (
	${PROJECT_NAME} PRIVATE VAAudioDriverBackend.cpp VAAudioDriverBackend.h VAAudioDriverConfig.cpp
							VAAudioDriverConfig.h
)

if (ITA_VACORE_WITH_AUDIO_BACKEND_ASIO)
	target_sources (${PROJECT_NAME} PRIVATE VAASIOBackend.cpp VAASIOBackend.h)
endif ()
if (ITA_VACORE_WITH_AUDIO_BACKEND_PORTAUDIO)
	target_sources (${PROJECT_NAME} PRIVATE VAPortaudioBackend.cpp VAPortaudioBackend.h)
endif ()
if (ITA_VACORE_WITH_AUDIO_BACKEND_VIRTUAL)
	target_sources (${PROJECT_NAME} PRIVATE VAVirtualAudioDriverBackend.cpp VAVirtualAudioDriverBackend.h)
endif ()
