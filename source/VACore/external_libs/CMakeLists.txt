set (CPM_DOWNLOAD_VERSION 0.32.2)
set (CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")

if (NOT (EXISTS ${CPM_DOWNLOAD_LOCATION}))
	message (STATUS "Downloading CPM.cmake")
	file (DOWNLOAD https://github.com/TheLartians/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake
		  ${CPM_DOWNLOAD_LOCATION}
	)
else (NOT (EXISTS ${CPM_DOWNLOAD_LOCATION}))
	message ("CPM already exists, do not need to download")
endif (NOT (EXISTS ${CPM_DOWNLOAD_LOCATION}))

include (${CPM_DOWNLOAD_LOCATION})

if (NOT TARGET ITACoreLibs)
	CPMAddPackage (
		NAME ITACoreLibs
		GIT_REPOSITORY https://git.rwth-aachen.de/ita/ITACoreLibs
		GIT_TAG master
		OPTIONS "ITA_BASE_WITH_JSON_SUPPORT ON" "ITA_CORE_LIBS_WITH_APPS OFF"
	)
endif ()

if (ITA_VACORE_WITH_RENDERER_BINAURAL_AIR_TRAFFIC_NOISE AND NOT TARGET ITAGeometricalAcoustics)
	CPMAddPackage (
		NAME ITAGeometricalAcoustics
		GIT_REPOSITORY https://git.rwth-aachen.de/ita/ITAGeometricalAcoustics
		GIT_TAG master
		OPTIONS "ITA_GEOMETRICAL_ACOUSTICS_WITH_APPS OFF"
	)
endif ()

if ((ITA_VACORE_WITH_RENDERER_BINAURAL_OUTDOOR_NOISE
	OR ITA_VACORE_WITH_RENDERER_PROTOTYPE_ROOM_ACOUSTICS
	OR ITA_VACORE_BINAURAL_OUTDOOR_NOISE_INSOURCE_BENCHMARKS)
	AND NOT TARGET ITASimulationScheduler
)
	CPMAddPackage (
		NAME ITASimulationScheduler
		GIT_REPOSITORY https://git.rwth-aachen.de/ita/ITASimulationScheduler
		GIT_TAG master
		OPTIONS ""
	)
endif ()

CPMAddPackage (
	NAME eigen
	GIT_REPOSITORY https://gitlab.com/libeigen/eigen
	GIT_TAG 3.4.0
	OPTIONS "EIGEN_BUILD_DOC OFF" "BUILD_TESTING OFF"
)

if (eigen_ADDED)
	set_target_properties (
		blas
		demos
		eigen_blas
		eigen_blas_static
		lapack
		eigen_lapack
		eigen_lapack_static
		uninstall
		PROPERTIES FOLDER external_libs/eigen
	)
endif ()
