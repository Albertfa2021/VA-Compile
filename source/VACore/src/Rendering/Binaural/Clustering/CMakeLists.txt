target_sources (
	${PROJECT_NAME} PRIVATE VABinauralClusteringRenderer.h VABinauralClusteringRenderer.cpp VABinauralWaveFront.h
)

add_subdirectory (Engine)
add_subdirectory (Receiver)
add_subdirectory (WaveFront)
