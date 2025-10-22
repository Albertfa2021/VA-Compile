cmake_minimum_required (VERSION 3.20 FATAL_ERROR)

project (
	VANet
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

# ---Options---

# Port
if (NOT DEFINED ITA_VANET_DEFAULT_PORT)
	set (
		ITA_VANET_DEFAULT_PORT
		12340
		CACHE STRING "VANet default TCP port"
	)
endif ()

option (ITA_VANET_WITH_NETAUDIO_STREAM_SERVER_EXTENSION
		"VANet with stream server class extension (requires ITADataSources)" OFF
)

# Advanced
option (ITA_VANET_CLIENT_VERBOSE "VANet client side with verbose output" OFF)
mark_as_advanced (ITA_VANET_CLIENT_VERBOSE)
option (ITA_VANET_CLIENT_SHOW_RAW_TRAFFIC "VANet client side print raw network traffic (debugging)" OFF)
mark_as_advanced (ITA_VANET_CLIENT_SHOW_RAW_TRAFFIC)
option (ITA_VANET_SERVER_VERBOSE "VANet server side with verbose output" OFF)
mark_as_advanced (ITA_VANET_SERVER_VERBOSE)
option (ITA_VANET_SERVER_SHOW_RAW_TRAFFIC "VANet server side print raw network traffic (debugging)" OFF)
mark_as_advanced (ITA_VANET_SERVER_SHOW_RAW_TRAFFIC)

# Debug
option (ITA_VANET_NETWORK_COM_PRINT_DEBUG_INFOS "VANet print network commuication infos (for debugging only)" OFF)

# ---End: Options---

# External libs
add_subdirectory (external_libs)

# Library
add_library (
	${PROJECT_NAME}
	include/VANet.h
	include/VANetAudioStreamServer.h
	include/VANetClient.h
	include/VANetDefinitions.h
	include/VANetServer.h
	include/VANetUtils.h
	include/VANetVersion.h
	src/VANetClient.cpp
	src/VANetClientImpl.cpp
	src/VANetClientImpl.h
	src/VANetMessage.cpp
	src/VANetMessage.h
	src/VANetNetworkProtocol.cpp
	src/VANetNetworkProtocol.h
	src/VANetServer.cpp
	src/VANetServerImpl.cpp
	src/VANetServerImpl.h
	src/VANetUtils.cpp
	src/VANetVersion.cpp
	src/VANetVistaCompatibility.h
)
add_library (VA::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

# Include Directory
target_include_directories (
	${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
						   $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/VANet>
)

# Linking
target_link_libraries (${PROJECT_NAME} PUBLIC VA::VABase vista::vista_base vista::vista_inter_proc_comm)
if (ITA_VANET_WITH_NETAUDIO_STREAM_SERVER_EXTENSION)
	target_link_libraries (${PROJECT_NAME} PRIVATE ITADataSources::ITADataSources)
endif ()

# Definitions for Shared/Static
target_compile_definitions (${PROJECT_NAME} PUBLIC $<IF:$<BOOL:${BUILD_SHARED_LIBS}>,VANET_EXPORTS,VANET_STATIC>)

# ---Definitions---

math (EXPR RELEASE_LETTER "${PROJECT_VERSION_MINOR}+97")
string (ASCII ${RELEASE_LETTER} RELEASE_LETTER)
target_compile_definitions (${PROJECT_NAME} PRIVATE VANET_VERSION_MAJOR=\"${PROJECT_VERSION_MAJOR}\")
target_compile_definitions (${PROJECT_NAME} PRIVATE VANET_VERSION_MINOR=\"${RELEASE_LETTER}\")
target_compile_definitions (${PROJECT_NAME} PUBLIC VANET_DEFAULT_PORT=${ITA_VANET_DEFAULT_PORT})

if (ITA_VANET_CLIENT_VERBOSE)
	target_compile_definitions (${PROJECT_NAME} PRIVATE VANET_CLIENT_VERBOSE)
endif ()

if (ITA_VANET_CLIENT_SHOW_RAW_TRAFFIC)
	target_compile_definitions (${PROJECT_NAME} PRIVATE VANET_CLIENT_SHOW_RAW_TRAFFIC)
endif ()

if (ITA_VANET_SERVER_VERBOSE)
	target_compile_definitions (${PROJECT_NAME} PRIVATE VANET_SERVER_VERBOSE)
endif ()

if (ITA_VANET_SERVER_SHOW_RAW_TRAFFIC)
	target_compile_definitions (${PROJECT_NAME} PRIVATE VANET_SERVER_SHOW_RAW_TRAFFIC)
endif ()

if (ITA_VANET_WITH_NETAUDIO_STREAM_SERVER_EXTENSION)
	target_sources (
		${PROJECT_NAME} PRIVATE src/VANetAudioStreamServer.cpp src/VANetAudioStreamServerImpl.cpp
								src/VANetAudioStreamServerImpl.h
	)
	target_compile_definitions (${PROJECT_NAME} PUBLIC VANET_WITH_NETAUDIO_STREAM_SERVER)
endif ()

if (ITA_VANET_NETWORK_COM_PRINT_DEBUG_INFOS)
	target_compile_definitions (${PROJECT_NAME} PRIVATE VANET_NETWORK_COM_PRINT_DEBUG_INFOS)
endif ()

# ---END: Definitions---

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
	NAMESPACE ${PROJECT_NAME}
	DISABLE_VERSION_SUFFIX YES
)

# tests
if (ITA_VA_WITH_TESTS)
	add_subdirectory ("tests")
endif ()
