cmake_minimum_required (VERSION 3.20 FATAL_ERROR)

project (
	VAServer
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

# ---Options / Settings---
if (NOT ITA_VASERVER_DEFAULT_SERVER_ADDRESS)
	set (
		ITA_VASERVER_DEFAULT_SERVER_ADDRESS
		"localhost"
		CACHE STRING "Default server bind address"
	)
endif ()
if (NOT ITA_VASERVER_DEFAULT_SERVER_PORT)
	set (
		ITA_VASERVER_DEFAULT_SERVER_PORT
		12340
		CACHE INT "Default server bind port"
	)
endif ()
option (ITA_VASERVER_DEPLOY_WITH_COMMON_SCRIPTS "Install VAServer starting scripts" ON)

# Executable
add_executable (${PROJECT_NAME} VAServer.cpp)

# Linking
target_link_libraries (${PROJECT_NAME} PRIVATE VA::VABase VA::VANet VA::VACore vista::vista_tools)
if (NOT WIN32)
	target_link_libraries (${PROJECT_NAME} PRIVATE ncurses)
endif ()

# Required compiler features
target_compile_features (${PROJECT_NAME} PUBLIC cxx_std_17)

# Put into apps folder in IDE
set_property (TARGET ${PROJECT_NAME} PROPERTY FOLDER "Apps/VAServer")
# Set working directory for being run out of IDE
set_property (TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

# ---Install---
# Target
math (EXPR RELEASE_LETTER "${PROJECT_VERSION_MINOR}+97")
string (ASCII ${RELEASE_LETTER} RELEASE_LETTER)
set (VA_SERVER_OUTPUT_FOLDER "VAServer_v${PROJECT_VERSION_MAJOR}${RELEASE_LETTER}")

# Install the server both to its own folder and to the "full" install
install (TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT VA)
install (TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${VA_SERVER_OUTPUT_FOLDER}/bin COMPONENT ${PROJECT_NAME})

if (WIN32)
	# https://stackoverflow.com/questions/62884439/how-to-use-cmake-file-get-runtime-dependencies-in-an-install-statement

	# Transfer variables into the install script
	install (CODE "set(VA_SERVER_OUTPUT_FOLDER \"${VA_SERVER_OUTPUT_FOLDER}\")" COMPONENT ${PROJECT_NAME})

	install (
		CODE [[
		file (
			GET_RUNTIME_DEPENDENCIES
			LIBRARIES
			$<TARGET_FILE:VAServer>
			RESOLVED_DEPENDENCIES_VAR
			_r_deps
			UNRESOLVED_DEPENDENCIES_VAR
			_u_deps
			POST_EXCLUDE_REGEXES
			Windows WINDOWS
			PRE_EXCLUDE_REGEXES
			api-ms-win-crt               # Remove Matlab dll; no idea how they get here
		)
		foreach (_file ${_r_deps})
			file (
				INSTALL
				DESTINATION ${CMAKE_INSTALL_PREFIX}/${VA_SERVER_OUTPUT_FOLDER}/bin
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

# Batch scripts Install the batch scripts both to its own folder and to the "full" install
if (ITA_VASERVER_DEPLOY_WITH_COMMON_SCRIPTS)
	set (ITA_VA_SERVER_START_SCRIPT_ADDRESS ${ITA_VASERVER_DEFAULT_SERVER_ADDRESS})
	set (ITA_VA_SERVER_START_SCRIPT_PORT ${ITA_VASERVER_DEFAULT_SERVER_PORT})
	if (WIN32)
		set (ITA_VA_SERVER_START_SCRIPT_INI VACore.ini)
		configure_file ("run_VAServer.bat.proto" "${CMAKE_CURRENT_BINARY_DIR}/run_VAServer.bat" @ONLY)
		install (
			FILES "${CMAKE_CURRENT_BINARY_DIR}/run_VAServer.bat"
			DESTINATION .
			COMPONENT VA
		)
		install (
			FILES "${CMAKE_CURRENT_BINARY_DIR}/run_VAServer.bat"
			DESTINATION ${VA_SERVER_OUTPUT_FOLDER}
			COMPONENT ${PROJECT_NAME}
		)
		set (ITA_VA_SERVER_START_SCRIPT_INI VACore.recording.ini)
		set (ITA_VA_SERVER_START_SCRIPT_RC "allow_remote_control_shutdown")
		configure_file ("run_VAServer.bat.proto" "${CMAKE_CURRENT_BINARY_DIR}/run_VAServer_recording.bat" @ONLY)
		install (
			FILES "${CMAKE_CURRENT_BINARY_DIR}/run_VAServer_recording.bat"
			DESTINATION .
			COMPONENT VA
		)
		install (
			FILES "${CMAKE_CURRENT_BINARY_DIR}/run_VAServer_recording.bat"
			DESTINATION ${VA_SERVER_OUTPUT_FOLDER}
			COMPONENT ${PROJECT_NAME}
		)
	else ()
		set (ITA_VA_SERVER_START_SCRIPT_INI VACore.ini)
		configure_file ("run_VAServer.sh.proto" "${CMAKE_CURRENT_BINARY_DIR}/run_VAServer.sh" @ONLY)
		install (
			FILES "${CMAKE_CURRENT_BINARY_DIR}/run_VAServer.sh"
			DESTINATION DESTINATION
			.
			COMPONENT VA
		)
		install (
			FILES "${CMAKE_CURRENT_BINARY_DIR}/run_VAServer.sh"
			DESTINATION ${VA_SERVER_OUTPUT_FOLDER}
			COMPONENT ${PROJECT_NAME}
		)
		set (ITA_VA_SERVER_START_SCRIPT_INI VACore.recording.ini)
		set (ITA_VA_SERVER_START_SCRIPT_RC "allow_remote_control_shutdown")
		configure_file ("run_VAServer.sh.proto" "${CMAKE_CURRENT_BINARY_DIR}/run_VAServer_recording.sh" @ONLY)
		install (
			FILES "${CMAKE_CURRENT_BINARY_DIR}/run_VAServer_recording.sh"
			DESTINATION DESTINATION
			.
			COMPONENT VA
		)
		install (
			FILES "${CMAKE_CURRENT_BINARY_DIR}/run_VAServer_recording.sh"
			DESTINATION ${VA_SERVER_OUTPUT_FOLDER}
			COMPONENT ${PROJECT_NAME}
		)
	endif ()
endif ()

# Misc folders
install (
	FILES README.md
	DESTINATION ${VA_SERVER_OUTPUT_FOLDER}
	COMPONENT ${PROJECT_NAME}
)

if (ITA_VA_COPYING_GNU_GPL)
	set (SELECTED_LICENSE "GNU General Public License")
	set (GPL_DEPENDENCIES "FFTW3")
	configure_file (../COPYING.proto "${CMAKE_CURRENT_BINARY_DIR}/COPYING" @ONLY)
	install (
		FILES ${CMAKE_CURRENT_BINARY_DIR}/COPYING
		DESTINATION ${VA_SERVER_OUTPUT_FOLDER}
		COMPONENT ${PROJECT_NAME}
	)
else ()
	install (
		FILES LICENSE.md
		DESTINATION ${VA_SERVER_OUTPUT_FOLDER}
		COMPONENT ${PROJECT_NAME}
	)
endif ()

if (ITA_VACORE_DEPLOY_WITH_COMMON_DATA)
	install (
		DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../VACore/data/
		DESTINATION ${VA_SERVER_OUTPUT_FOLDER}/data
		COMPONENT ${PROJECT_NAME}
		FILES_MATCHING
		PATTERN "*.wav"
		PATTERN "*.daff"
		PATTERN "*.md"
	)
endif ()

if (ITA_VACORE_DEPLOY_WITH_COMMON_CONFIGS)
	install (
		DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../VACore/conf/
		DESTINATION ${VA_SERVER_OUTPUT_FOLDER}/conf
		COMPONENT ${PROJECT_NAME}
		FILES_MATCHING
		PATTERN "*.ini"
		PATTERN "*.json"
	)
endif ()
