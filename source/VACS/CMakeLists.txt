cmake_minimum_required (VERSION 3.20 FATAL_ERROR)

project (
	VANetCSWrapper
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
option (ITA_VACS_DEPLOY_ASSEMBLY "Install VACS assembly (C# dll) into deploy binary directory" ON)

# Library - must be shared
add_library (${PROJECT_NAME} SHARED include/VANetCSWrapper.h include/VACSDefinitions.h src/VANetCSWrapper.cpp)
add_library (VA::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
target_compile_definitions (${PROJECT_NAME} PUBLIC VACS_EXPORTS) # Wrapper can only be shared, so export definitions are
# required

# Include Directory
target_include_directories (
	${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
						   $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/VACS>
)

# Linking
target_link_libraries (
	${PROJECT_NAME}
	PUBLIC VA::VABase
	PRIVATE VA::VANet
)

# Required compiler features
target_compile_features (${PROJECT_NAME} PUBLIC cxx_std_17)

# If build outside of VA project, change IDE folder
if (NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	set_property (TARGET ${PROJECT_NAME} PROPERTY FOLDER "Bindings/VA")
endif ()

# tests
if (ITA_VA_WITH_TESTS)
	add_subdirectory ("tests")
endif ()

# ---Install---
math (EXPR RELEASE_LETTER "${PROJECT_VERSION_MINOR}+97")
string (ASCII ${RELEASE_LETTER} RELEASE_LETTER)
set (VACS_OUTPUT_FOLDER "VACS_v${PROJECT_VERSION_MAJOR}${RELEASE_LETTER}")

# Target
install (TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${VACS_OUTPUT_FOLDER} COMPONENT ${PROJECT_NAME})
# Copy CS wrapper vista_install_files_by_extension( csharp/src "csharp" "cs" )
install (
	DIRECTORY "csharp/src/"
	DESTINATION ${VACS_OUTPUT_FOLDER}
	COMPONENT ${PROJECT_NAME}
	FILES_MATCHING
	PATTERN "*.cs"
)

if (WIN32)
	# Install system libraries
	include (InstallRequiredSystemLibraries)
	install (
		FILES ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}
		DESTINATION ${VACS_OUTPUT_FOLDER}
		COMPONENT ${PROJECT_NAME}
	)

	# https://stackoverflow.com/questions/62884439/how-to-use-cmake-file-get-runtime-dependencies-in-an-install-statement

	# Transfer variables into the install script
	install (CODE "set(VACS_OUTPUT_FOLDER \"${VACS_OUTPUT_FOLDER}\")" COMPONENT ${PROJECT_NAME})

	install (
		CODE [[
		file (
			GET_RUNTIME_DEPENDENCIES
			LIBRARIES
			$<TARGET_FILE:VANetCSWrapper>
			RESOLVED_DEPENDENCIES_VAR
			_r_deps
			UNRESOLVED_DEPENDENCIES_VAR
			_u_deps
			POST_EXCLUDE_REGEXES
			Windows WINDOWS
		)
		foreach (_file ${_r_deps})
			file (
				INSTALL
				DESTINATION ${CMAKE_INSTALL_PREFIX}/${VACS_OUTPUT_FOLDER}
				FOLLOW_SYMLINK_CHAIN FILES "${_file}"
			)
		endforeach ()
		list (LENGTH _u_deps _u_length)
		if ("${_u_length}" GREATER 0)
			message (WARNING \"Unresolved dependencies detected!\n${_u_deps}\")
		endif ()
	]]
		COMPONENT ${PROJECT_NAME}
	)
endif ()

# --- --- --- C# --- --- ---

# C# project

if (ITA_VACS_DEPLOY_ASSEMBLY)
	set (VACS_OUTPUT_PATH "${CMAKE_INSTALL_PREFIX}/bin")
elseif ()
	set (VACS_OUTPUT_PATH "bin")
endif ()

set (VACS_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/csharp/src/VACS.cs")
if (WIN32)
	string (REGEX REPLACE "/" "\\\\" VACS_OUTPUT_PATH ${VACS_OUTPUT_PATH})
	string (REGEX REPLACE "/" "\\\\" VACS_SOURCE_FILES ${VACS_SOURCE_FILES})
endif ()

configure_file ("csharp/VACS.csproj.proto" "csharp/VACS.csproj" @ONLY)

message (STATUS "Find generated VACS (C#) projects here: ${CMAKE_CURRENT_BINARY_DIR}/csharp")

# C# tests
if (ITA_VA_WITH_TESTS)

	set (VACSDUMMY_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/csharp/tests/VACSDummy.cs")
	if (WIN32)
		string (REGEX REPLACE "/" "\\\\" VACSDUMMY_SOURCE_FILES ${VACSDUMMY_SOURCE_FILES})
	endif ()

	set (VACSDUMMY_WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}/csharp")
	configure_file ("csharp/VACSDummy.csproj.proto" "csharp/VACSDummy.csproj" @ONLY)
	configure_file ("csharp/VACSDummy.csproj.user.proto" "csharp/VACSDummy.csproj.user" @ONLY)

	set (VACSTEST_SOURCE_FILES
		 "${CMAKE_CURRENT_SOURCE_DIR}/csharp/tests/VACSTest.cs\;${CMAKE_CURRENT_SOURCE_DIR}/csharp/src/VACS.cs"
	)
	if (WIN32)
		string (REGEX REPLACE "/" "\\\\" VACSTEST_SOURCE_FILES ${VACSTEST_SOURCE_FILES})
	endif ()

	set (VACSTEST_WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}/csharp")
	configure_file ("csharp/VACSTest.csproj.proto" "csharp/VACSTest.csproj" @ONLY)
	configure_file ("csharp/VACSTest.csproj.user.proto" "csharp/VACSTest.csproj.user" @ONLY)

endif ()
