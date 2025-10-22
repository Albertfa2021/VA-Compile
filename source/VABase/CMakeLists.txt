cmake_minimum_required (VERSION 3.20 FATAL_ERROR)

project (
	VABase
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

# Options

# external dependencies
add_subdirectory (external_libs)

# Library
add_library (
	${PROJECT_NAME}
	"include/VA.h"
	"include/VAAudioSignalSource.h"
	"include/VABase.h"
	"include/VABaseDefinitions.h"
	"include/VAEvent.h"
	"include/VAEventHandlerGlobalLock.h"
	"include/VAException.h"
	"include/VAInterface.h"
	"include/VAObject.h"
	"include/VAObjectRegistry.h"
	"include/VASamples.h"
	"include/VAStruct.h"
	"include/VAVersion.h"
	"src/VABase.cpp"
	"src/VAEvent.cpp"
	"src/VAEventHandlerGlobalLock.cpp"
	"src/VAException.cpp"
	"src/VAInterface.cpp"
	"src/VAObject.cpp"
	"src/VAObjectRegistry.cpp"
	"src/VASamples.cpp"
	"src/VAStruct.cpp"
	"src/VAVersion.cpp"
)
add_library (VA:${PROJECT_NAME} ALIAS ${PROJECT_NAME})

# Include Directory
target_include_directories (
	${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
						   $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}>
)

# Linking target_link_libraries (${PROJECT_NAME} PUBLIC ITALibName::ITALibName)

# Definitions for Shared/Static
target_compile_definitions (${PROJECT_NAME} PUBLIC $<IF:$<BOOL:${BUILD_SHARED_LIBS}>,VABASE_EXPORTS,VABASE_STATIC>)
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

# tests
if (ITA_VA_WITH_TESTS)
	set (VABASE_COMMON_BUILD TRUE)
	add_subdirectory ("${CMAKE_CURRENT_SOURCE_DIR}/tests")
endif ()
