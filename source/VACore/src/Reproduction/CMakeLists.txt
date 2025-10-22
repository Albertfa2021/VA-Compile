target_sources (
	${PROJECT_NAME} PRIVATE VAAudioReproduction.h VAAudioReproductionRegistry.cpp VAAudioReproductionRegistry.h
)

add_subdirectory (Ambisonics)
add_subdirectory (Binaural)
add_subdirectory (Mixer)
