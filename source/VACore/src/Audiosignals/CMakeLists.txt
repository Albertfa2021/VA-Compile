target_sources (
	${PROJECT_NAME}
	PRIVATE VAAudioSignalSourceManager.cpp
			VAAudioSignalSourceManager.h
			VAAudiofileSignalSource.cpp
			VAAudiofileSignalSource.h
			VADeviceInputSignalSource.cpp
			VADeviceInputSignalSource.h
			VAEngineSignalSource.cpp
			VAEngineSignalSource.h
			VAJetEngineSignalSource.cpp
			VAJetEngineSignalSource.h
			VAMachineSignalSource.cpp
			VAMachineSignalSource.h
			VANetstreamSignalSource.cpp
			VANetstreamSignalSource.h
			# VAPureDataSignalSource.cpp VAPureDataSignalSource.h
			VANetworkStreamAudioSignalSource.cpp
			VASequencerSignalSource.cpp
			VASequencerSignalSource.h
			VATextToSpeechSignalSource.h
			VATextToSpeechSignalSource.cpp
)
