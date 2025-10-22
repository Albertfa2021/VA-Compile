cmake_minimum_required (VERSION 3.20 FATAL_ERROR)

project (
	VACore
	VERSION 2022.0
	LANGUAGES CXX C
)

set (CMAKE_DEBUG_POSTFIX "-d")

if (NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY AND WIN32)
	set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/bin)
	set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/lib)
	set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/lib)
endif ()

if (CMAKE_GENERATOR MATCHES "Visual Studio")
	set_property (GLOBAL PROPERTY USE_FOLDERS ON)
endif ()

# Macro to add compiler options to a target The string "ITA_" is prepended to the cmake option but NOT the compile
# definition.
macro (add_ita_compile_option option_name description value target)
	option (ITA_${option_name} ${description} ${value})

	if (ITA_${option_name})
		target_compile_definitions (${target} PRIVATE ${option_name})
	endif ()
endmacro ()

# ---Options / Settings---
# TODO: Check which ones are really required

# Static/Shared? Still required option(ITA_VA_BUILD_STATIC "Build static VA libs" OFF) Tests
option (ITA_VACORE_WITH_TESTS "Build VACore with tests" OFF)
# FFTW? GPL license?
option (ITA_VA_CORE_LIBS_FFT_USES_FFTW3
		"If ITAFFT library of ITACoreLibs uses FFTW3, VACore has to fall back to GNU GPL license." ON
)
if (ITA_VA_CORE_LIBS_FFT_USES_FFTW3)
	set (
		ITA_VA_COPYING_GNU_GPL
		ON
		PARENT_SCOPE
	)
endif ()
# Can those options be deleted?
option (ITA_VACORE_DEPLOY_WITH_COMMON_DATA
		"Will deploy VACore with common data (HRIR, Directivity, WAV file, HPEQ, Room Model, etc)" ON
)
option (ITA_VACORE_DEPLOY_WITH_COMMON_CONFIGS
		"Will deploy VACore with common configuration files (generic VACore.ini, setups for laboraties)" ON
)
option (ITA_VACORE_DEPLOY_WITH_COMMON_SCRIPTS "Will deploy VACore with common scripts (start VAServer from console)" ON)

# General settings
option (ITA_VACORE_WITH_TTS_SIGNAL_SOURCE "VACore with text-to-speech signal source support" ${TTSReleay})
if (NOT DEFINED ITA_VACORE_DEFAULT_CONFIG_FILE_NAME)
	set (
		ITA_VACORE_DEFAULT_CONFIG_FILE_NAME
		VACore.ini
		CACHE STRING "Default VACore configuration file name (INI file)"
	)
endif ()
if (NOT DEFINED ITA_VACORE_DEFAULT_DEBUG_LEVEL)
	set (
		ITA_VACORE_DEFAULT_DEBUG_LEVEL
		3
		CACHE INT "Default VACore debug level (0-5)"
	)
endif ()
if (NOT DEFINED ITA_VACORE_MAX_NUM_SOUND_SOURCES)
	set (
		ITA_VACORE_MAX_NUM_SOUND_SOURCES
		0
		CACHE INT "VACore maximum number of sound sources ( 0 = unlimited )"
	)
endif ()
if (NOT DEFINED ITA_VACORE_MAX_NUM_SOUND_RECEIVERS)
	set (
		ITA_VACORE_MAX_NUM_SOUND_RECEIVERS
		0
		CACHE INT "VACore maximum number of sound receivers ( 0 = unlimited )"
	)
endif ()
if (WIN32)
	option (ITA_VACORE_WITH_SAMPLER_SUPPORT "VACore with sequencer (sampler) signal source support (only Windows)" ON)
else ()
	option (ITA_VACORE_WITH_SAMPLER_SUPPORT "VACore with sequencer (sampler) signal source support (only Windows)" OFF)
	mark_as_advanced (ITA_VACORE_WITH_SAMPLER_SUPPORT)
endif ()

# Data directories
if (NOT DEFINED ITA_VACORE_BIG_DATA_DIR)
	set (
		ITA_VACORE_BIG_DATA_DIR
		""
		CACHE PATH "VACore big data dir with more files for auralization (will be added to search path)"
	)
endif ()
if (NOT DEFINED INSIDE_SCENE_DATA)
	set (
		INSIDE_SCENE_DATA
		""
		CACHE PATH "InsideSceneData folder for Inside scenes"
	)
endif ()
if (NOT DEFINED ITA_RAVEN_DATA_PATH)
	set (
		ITA_RAVEN_DATA_PATH
		""
		CACHE PATH "RAVEN data base path (where to find models, HRTFs, materials etc. ...)"
	)
endif ()
# ---END: Options / Settings---

# External libs
add_subdirectory (external_libs)

# Library
add_library (${PROJECT_NAME} "")
add_library (VA::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

# ---Compile options--- (must be initialized before adding source files) Audio drivers (do we really need to turn them
# off?)
add_ita_compile_option (
	VACORE_WITH_AUDIO_BACKEND_ASIO "Build VACore with ASIO audio backend support" ON ${PROJECT_NAME}
)
add_ita_compile_option (
	VACORE_WITH_AUDIO_BACKEND_PORTAUDIO "Build VACore with Portaudio audio backend support" ON ${PROJECT_NAME}
)
add_ita_compile_option (
	VACORE_WITH_AUDIO_BACKEND_VIRTUAL "Build VACore with virtual audio backend support (for offline simulations)" ON
	${PROJECT_NAME}
)

# Rendering modules (double-check which ones we really need to be able to turn off)
add_ita_compile_option (
	VACORE_WITH_RENDERER_AMBIENT_MIXER "Build VACore with rendering module: ambient mixer" ON ${PROJECT_NAME}
)
add_ita_compile_option (
	VACORE_WITH_RENDERER_AMBISONICS_FREE_FIELD "Build VACore with rendering module: ambisonics free-field" ON
	${PROJECT_NAME}
)

add_ita_compile_option (
	VACORE_WITH_RENDERER_BINAURAL_FREE_FIELD "Build VACore with rendering module: binaural free-field" ON
	${PROJECT_NAME}
)
add_ita_compile_option (
	VACORE_WITH_RENDERER_BINAURAL_ARTIFICIAL_REVERB "Build VACore with rendering module: artificial reverb" ON
	${PROJECT_NAME}
)
add_ita_compile_option (
	VACORE_WITH_RENDERER_BINAURAL_AIR_TRAFFIC_NOISE
	"Build VACore with rendering module: binaural air traffic noise (VATSS)" ON ${PROJECT_NAME}
)
add_ita_compile_option (
	VACORE_WITH_RENDERER_BINAURAL_OUTDOOR_NOISE "Build VACore with rendering module: binaural outdoor noise" ON
	${PROJECT_NAME}
)
add_ita_compile_option (
	VACORE_WITH_RENDERER_BINAURAL_CLUSTERING "Build VACore with rendering module: binaural clustering renderer" ON
	${PROJECT_NAME}
)

add_ita_compile_option (
	VACORE_WITH_RENDERER_PROTOTYPE_FREE_FIELD "Build VACore with rendering module: prototype free-field" ON
	${PROJECT_NAME}
)
add_ita_compile_option (
	VACORE_WITH_RENDERER_PROTOTYPE_DUMMY "Build VACore with rendering module: prototype dummy" OFF ${PROJECT_NAME}
)
add_ita_compile_option (
	VACORE_WITH_RENDERER_PROTOTYPE_HEARING_AID "Build VACore with rendering module: prototype hearing aid (iCARE)" OFF
	${PROJECT_NAME}
)
add_ita_compile_option (
	VACORE_WITH_RENDERER_PROTOTYPE_GENERIC_PATH "Build VACore with rendering module: prototype generic path" ON
	${PROJECT_NAME}
)
add_ita_compile_option (
	VACORE_WITH_RENDERER_PROTOTYPE_IMAGE_SOURCE "Build VACore with rendering module: prototype image source" ON
	${PROJECT_NAME}
)
add_ita_compile_option (
	VACORE_WITH_RENDERER_ROOM_ACOUSTICS "Build VACore with rendering module: room acoustics (e.g. with RAVEN backend)"
	OFF ${PROJECT_NAME}
)

add_ita_compile_option (
	VACORE_WITH_RENDERER_VBAP_FREE_FIELD "Build VACore with rendering module: vector-base amplitude panning free-field"
	ON ${PROJECT_NAME}
)

# Reproduction modules
add_ita_compile_option (
	VACORE_WITH_REPRODUCTION_TALKTHROUGH "Build VACore with reproduction module: talkthrough" ON ${PROJECT_NAME}
)
add_ita_compile_option (
	VACORE_WITH_REPRODUCTION_HEADPHONES "Build VACore with reproduction module: headphones (equalization)" ON
	${PROJECT_NAME}
)
add_ita_compile_option (
	VACORE_WITH_REPRODUCTION_AMBISONICS "Build VACore with reproduction module: ambisonics (B-format decoder)" ON
	${PROJECT_NAME}
)
add_ita_compile_option (
	VACORE_WITH_REPRODUCTION_BINAURAL_NCTC "Build VACore with reproduction module: binaural N-CTC" ON ${PROJECT_NAME}
)
add_ita_compile_option (
	VACORE_WITH_REPRODUCTION_BINAURAL_MIXDOWN "Build VACore with reproduction module: binaural mixdown" ON
	${PROJECT_NAME}
)
add_ita_compile_option (
	VACORE_WITH_REPRODUCTION_AMBISONICS_BINAURAL_MIXDOWN
	"Build VACore with reproduction module: ambisonics binaural mixdown (B-format decoder)" ON ${PROJECT_NAME}
)
add_ita_compile_option (
	VACORE_WITH_REPRODUCTION_MIXER_LOW_FREQUENCY
	"Build VACore with reproduction module: low frequency (subwoofer) mixer" ON ${PROJECT_NAME}
)

# Development
add_ita_compile_option (
	VACORE_REPRODUCTION_NCTC_WITH_SWEET_SPOT_WIDENING "Crosstalk-Cancellation sweet spot widening (development)" ON
	${PROJECT_NAME}
)

# General settings
add_ita_compile_option (VACORE_MODULE_INTERFACE_ENABLED "Build VACore with module interface" ON ${PROJECT_NAME})
add_ita_compile_option (
	VACORE_NO_MODULE_INTERFACE_THROW_EXCEPTION
	"Throw exception during module call attempt if VACore was build without module interface" ON ${PROJECT_NAME}
)
add_ita_compile_option (VACORE_EVENTS_ENABLED "VACore event handling" ON ${PROJECT_NAME})

# Benchmarking
add_ita_compile_option (
	VACORE_BINAURAL_OUTDOOR_NOISE_INSOURCE_BENCHMARKS
	"Enable in-source benchmarking for binaural outdoor noise renderer" OFF ${PROJECT_NAME}
)
# ---END: Compile options---

# Source files
add_subdirectory ("include")
add_subdirectory ("src")

# Include Directory
target_include_directories (
	${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
						   $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/VACore>
)

# Linking
target_link_libraries (
	${PROJECT_NAME}
	PUBLIC VA::VABase ITABase::ITABase
	PRIVATE ITADSP::ITADSP
			ITADataSources::ITADataSources
			ITAConvolution::ITAConvolution
			ITACTC::ITACTC
			DAFF::DAFF
			Spline::spline
			Eigen3::Eigen
			TBB::tbb
)
if (ITA_VACORE_WITH_SAMPLER_SUPPORT)
	target_link_libraries (${PROJECT_NAME} PRIVATE ITASampler::ITASampler)
endif ()
if (ITA_VACORE_WITH_TTS_SIGNAL_SOURCE)
	target_link_libraries (${PROJECT_NAME} PRIVATE CereVoice::CereVoice)
	target_link_libraries (${PROJECT_NAME} PRIVATE SampleRate::samplerate)
endif ()
if (ITA_VACORE_WITH_RENDERER_BINAURAL_ARTIFICIAL_REVERB)
	target_link_libraries (${PROJECT_NAME} PRIVATE DSPFilters)
endif ()
if (ITA_VACORE_WITH_RENDERER_BINAURAL_AIR_TRAFFIC_NOISE)
	target_link_libraries (${PROJECT_NAME} PRIVATE ITAGeo::ITAGeo)
	target_link_libraries (${PROJECT_NAME} PRIVATE ITAPropagationPathSim::ITAPropagationPathSim)
	target_link_libraries (${PROJECT_NAME} PRIVATE ITAPropagationModels::ITAPropagationModels)
endif ()
if (ITA_VACORE_WITH_RENDERER_BINAURAL_OUTDOOR_NOISE
	OR ITA_VACORE_WITH_RENDERER_PROTOTYPE_ROOM_ACOUSTICS
	OR ITA_VACORE_BINAURAL_OUTDOOR_NOISE_INSOURCE_BENCHMARKS
)
	target_link_libraries (${PROJECT_NAME} PRIVATE ITASimulationScheduler::ITASimulationScheduler)
endif ()

# Definitions for Shared/Static
target_compile_definitions (${PROJECT_NAME} PUBLIC $<IF:$<BOOL:${BUILD_SHARED_LIBS}>,VACORE_EXPORTS,VACORE_STATIC>)

# Platform-specific definitions
if(WIN32)
	target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX _USE_MATH_DEFINES)
endif()

# Required compiler features
target_compile_features (${PROJECT_NAME} PUBLIC cxx_std_17)

# If build out of VA project, add parent folder "VA"
if (NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	set_property (TARGET ${PROJECT_NAME} PROPERTY FOLDER "VA")
endif ()

# Organize sources in folders
GroupSourcesByFolder (${PROJECT_NAME})

# Install & export
packageProject (
	NAME ${PROJECT_NAME}
	VERSION ${PROJECT_VERSION}
	BINARY_DIR ${PROJECT_BINARY_DIR}
	INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include
	INCLUDE_DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}
	DEPENDENCIES ""
	COMPATIBILITY ExactVersion
	NAMESPACE VA
	DISABLE_VERSION_SUFFIX YES
)

# ---Definitions---
math (EXPR RELEASE_LETTER "${PROJECT_VERSION_MINOR}+97")
string (ASCII ${RELEASE_LETTER} RELEASE_LETTER)
target_compile_definitions (${PROJECT_NAME} PUBLIC VACORE_DEFAULT_LOG_LEVEL=${ITA_VACORE_DEFAULT_DEBUG_LEVEL})
target_compile_definitions (${PROJECT_NAME} PRIVATE VACORE_VERSION_MAJOR=\"${PROJECT_VERSION_MAJOR}\")
target_compile_definitions (${PROJECT_NAME} PRIVATE VACORE_VERSION_MINOR=\"${RELEASE_LETTER}\")

target_compile_definitions (
	${PROJECT_NAME} PRIVATE VACORE_DEFAULT_CONFIGFILE=\"${ITA_VACORE_DEFAULT_CONFIG_FILE_NAME}\"
)
target_compile_definitions (${PROJECT_NAME} PRIVATE VACORE_MAX_NUM_SOUND_SOURCES=${ITA_VACORE_MAX_NUM_SOUND_SOURCES})
target_compile_definitions (
	${PROJECT_NAME} PRIVATE VACORE_MAX_NUM_SOUND_RECEIVERS=${ITA_VACORE_MAX_NUM_SOUND_RECEIVERS}
)

string (TIMESTAMP VACORE_CMAKE_DATE "%Y-%m-%d")
target_compile_definitions (${PROJECT_NAME} PRIVATE VACORE_CMAKE_DATE=\"${VACORE_CMAKE_DATE}\")

if (ITA_VACORE_WITH_TTS_SIGNAL_SOURCE)
	target_compile_definitions (${PROJECT_NAME} PRIVATE VACORE_WITH_TTS_SIGNAL_SOURCE)
endif ()
if (ITA_VACORE_WITH_SAMPLER_SUPPORT)
	target_compile_definitions (${PROJECT_NAME} PRIVATE VACORE_WITH_SAMPLER_SUPPORT)
endif ()

# ---END: Definitions---

# ---Config & Data files---

# VACore default configuration file
if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/conf/${ITA_VACORE_DEFAULT_CONFIG_FILE_NAME}")
	set (
		ITA_VACORE_DEVELOPMENT_PATHS
		"# Developer paths
conf_dir_dev = ${CMAKE_CURRENT_SOURCE_DIR}/conf
data_dir_dev = ${CMAKE_CURRENT_SOURCE_DIR}/data"
	)
	configure_file ("conf/VACore.ini.proto" "${CMAKE_BINARY_DIR}/conf/${ITA_VACORE_DEFAULT_CONFIG_FILE_NAME}" @ONLY)
else ()
	message (
		"conf/VACore.ini exists, skipping overwrite. Manually remove generated conf/VACore.ini first, if you want to re-generate from default prototype config file."
	)
endif ()

# Copy certain files in the conf and data folders to the build directory, but only if they not exist. The method is a
# bit brute force, but it works for now.
file (
	GLOB VA_CONF_AND_DATA_FILES
	RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
	conf/*.json conf/*.ini data/*.wav data/*.daff data/*.skp
)
foreach (CURRENT_VA_FILE ${VA_CONF_AND_DATA_FILES})
	if (NOT EXISTS "${CMAKE_BINARY_DIR}/${CURRENT_VA_FILE}")
		configure_file (${CURRENT_VA_FILE} "${CMAKE_BINARY_DIR}/${CURRENT_VA_FILE}" COPYONLY)
	endif ()
endforeach ()

# Add all config files in build directory to target sources
file (GLOB VA_BUILD_CONF_FILES "${CMAKE_BINARY_DIR}/conf/*.json" "${CMAKE_BINARY_DIR}/conf/*.ini")
target_sources (${PROJECT_NAME} PRIVATE ${VA_BUILD_CONF_FILES})

# ---END: Config & Data files---

# ---Install---
if (ITA_VACORE_DEPLOY_WITH_COMMON_DATA)
	install (
		DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/data/"
		DESTINATION data
		COMPONENT ${PROJECT_NAME}
		FILES_MATCHING
		PATTERN "*.wav"
		PATTERN "*.daff"
		PATTERN "*.md"
	)
endif ()

if (ITA_VACORE_DEPLOY_WITH_COMMON_CONFIGS)
	install (
		DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/conf/"
		DESTINATION conf
		COMPONENT ${PROJECT_NAME}
		FILES_MATCHING
		PATTERN "*.ini"
		PATTERN "*.json"
	)
	install (CODE "set(VA_CORE_PROTO \"${CMAKE_CURRENT_SOURCE_DIR}/conf/VACore.ini.proto\")" COMPONENT ${PROJECT_NAME})
	install (CODE "set(ITA_VACORE_DEFAULT_DEBUG_LEVEL \"${ITA_VACORE_DEFAULT_DEBUG_LEVEL}\")" COMPONENT ${PROJECT_NAME})
	install (CODE "set(ITA_VACORE_DEFAULT_CONFIG_FILE_NAME \"${ITA_VACORE_DEFAULT_CONFIG_FILE_NAME}\")"
			 COMPONENT ${PROJECT_NAME}
	)
	install (
		CODE [[
	configure_file (
			 ${VA_CORE_PROTO} ${CMAKE_INSTALL_PREFIX}/conf/${ITA_VACORE_DEFAULT_CONFIG_FILE_NAME} @ONLY
		)
		]]
		COMPONENT ${PROJECT_NAME}
	)
endif ()

if (ITA_VACORE_DEPLOY_WITH_COMMON_SCRIPTS)
	# nothing yet
endif ()

# tests
if (ITA_VA_WITH_TESTS OR ITA_VACORE_WITH_TESTS)
	add_subdirectory ("tests")
endif ()
# benchmarks
if (ITA_VA_WITH_BENCHMARKS OR ITA_VACORE_WITH_BENCHMARKS)
	add_subdirectory ("benchmarks/core")
endif ()
