cmake_minimum_required (VERSION 3.20 FATAL_ERROR)

if (NOT WIN32)
	message (FATAL_ERROR "VAMatlab can currently only be built for windows since it requires NatNetSDK.")
endif ()

project (
	VAMatlab
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

# "Show VA banner on Matlab init" (used to be an option)
set (ITA_VAMATLAB_SHOW_BANNER ON)

# Build the Matlab class VA automatically
option (ITA_VAMATLAB_POSTBUILD_CREATE_MATLAB_CLASS "Auto-create Matlab convenience class after build [for future use]"
		OFF
)
mark_as_advanced (ITA_VAMATLAB_POSTBUILD_CREATE_MATLAB_CLASS)

# Build VAMatlab with internal core (not properly tested)
option (ITA_VAMATLAB_BUILD_WITH_INTERNAL_CORE
		"Build a VACore into VAMatlab instead of using a network adapter (not recommended)" OFF
)
mark_as_advanced (ITA_VAMATLAB_BUILD_WITH_INTERNAL_CORE)

# ---End: Options---

# external dependencies
add_subdirectory (external_libs)

# Find package settings
if (NOT WITH_IHTA_TRACKING)
	message (FATAL_ERROR "IHTA Utility Libs is configured without IHTATracking.")
endif ()

# Create mex (instead of add_library)
find_package (Matlab REQUIRED COMPONENTS MAIN_PROGRAM) # Matlab required for matlab_add_mex
# Todo: With CMake >3.20 -> require Matlab version 9.6 == R2019a and above. Then, the batch mode of matlab can be used!

matlab_add_mex (
	NAME ${PROJECT_NAME}
	SHARED
	SRC src/VAMatlabFunctionMapping.h
		src/VAMatlabHelpers.h
		src/VAMatlabHelpers.cpp
		src/VAMatlabConnection.cpp
		src/VAMatlabConnection.h
		src/VAMatlabExecutable.cpp
		VAMatlab.def
	OUTPUT_NAME ${PROJECT_NAME}
	LINK_TO VA::VABase VA::VANet IHTA::IHTATracking
)
# Source files for OptiTrack
target_sources (${PROJECT_NAME} PRIVATE src/VAMatlabTracking.h src/VAMatlabTracking.cpp)

if (ITA_VAMATLAB_BUILD_WITH_INTERNAL_CORE)
	# TODO: Test! Still required?
	target_link_libraries (${PROJECT_NAME} PRIVATE VA::VACore)
endif ()

# ---Definitions---
# Version
math (EXPR RELEASE_LETTER "${PROJECT_VERSION_MINOR}+97")
string (ASCII ${RELEASE_LETTER} RELEASE_LETTER)
target_compile_definitions (${PROJECT_NAME} PRIVATE VAMATLAB_VERSION_MAJOR=\"${PROJECT_VERSION_MAJOR}\")
target_compile_definitions (${PROJECT_NAME} PRIVATE VAMATLAB_VERSION_MINOR=\"${RELEASE_LETTER}\")

if (ITA_VAMATLAB_BUILD_WITH_INTERNAL_CORE)
	target_compile_definitions (${PROJECT_NAME} PRIVATE VAMATLAB_INTERNAL_CORE)
endif ()
if (ITA_VAMATLAB_SHOW_BANNER)
	target_compile_definitions (${PROJECT_NAME} PRIVATE VAMATLAB_SHOW_BANNER)
endif ()

# TODO: Required? I think VAMatlab works in plane C Required compiler features
target_compile_features (${PROJECT_NAME} PUBLIC cxx_std_17)

# If build outside of VA project, change IDE folder
if (NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	set_property (TARGET ${PROJECT_NAME} PROPERTY FOLDER "Bindings/VA")
endif ()

# Disable postfix for library in debug mode
set_target_properties (${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX "")

# ---Create Matlab wrapper
add_custom_command (
	TARGET ${PROJECT_NAME}
	POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/matlab/VA_generate_wrapper.m
			${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
	COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/matlab/VA_generate_stubs.m
			${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
	COMMAND
		${Matlab_MAIN_PROGRAM} -nojvm -nodisplay -nosplash -nodesktop -wait -r
		"VA_generate_wrapper('${CMAKE_RUNTIME_OUTPUT_DIRECTORY}', '${CMAKE_CURRENT_SOURCE_DIR}/matlab/VA.m.proto'), exit"
	WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
	COMMENT "Running matlab VA wrapper generator"
	VERBATIM
)

# ---Install---

set (VA_MATLAB_OUTPUT_FOLDER "VAMatlab_v${PROJECT_VERSION_MAJOR}${RELEASE_LETTER}")

install (TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${VA_MATLAB_OUTPUT_FOLDER}/@VA/private COMPONENT ${PROJECT_NAME})

if (WIN32)
	# https://stackoverflow.com/questions/62884439/how-to-use-cmake-file-get-runtime-dependencies-in-an-install-statement

	# Transfer variables into the install script
	install (CODE "set(Matlab_ROOT_DIR \"${Matlab_ROOT_DIR}\")" COMPONENT ${PROJECT_NAME})
	install (CODE "set(VA_MATLAB_OUTPUT_FOLDER \"${VA_MATLAB_OUTPUT_FOLDER}\")" COMPONENT ${PROJECT_NAME})

	install (
		CODE [[
		file (
			GET_RUNTIME_DEPENDENCIES
			LIBRARIES
			$<TARGET_FILE:VAMatlab>
			RESOLVED_DEPENDENCIES_VAR
			_r_deps
			UNRESOLVED_DEPENDENCIES_VAR
			_u_deps
			POST_EXCLUDE_REGEXES
			Windows WINDOWS Matlab MATLAB
			DIRECTORIES
			${Matlab_ROOT_DIR}/bin/win64
		)
		foreach (_file ${_r_deps})
			file (
				INSTALL
				DESTINATION ${CMAKE_INSTALL_PREFIX}/${VA_MATLAB_OUTPUT_FOLDER}/@VA/private
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

install (
	FILES ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/VA.m
	DESTINATION ${VA_MATLAB_OUTPUT_FOLDER}/@VA
	COMPONENT ${PROJECT_NAME}
)

install (
	FILES matlab/VA_setup.fig matlab/VA_setup.m
	DESTINATION ${VA_MATLAB_OUTPUT_FOLDER}/@VA/private
	COMPONENT ${PROJECT_NAME}
)

install (
	DIRECTORY "matlab/examples/"
	DESTINATION ${VA_MATLAB_OUTPUT_FOLDER}
	COMPONENT ${PROJECT_NAME}
)

install (
	FILES matlab/va_matlab2openGL.m matlab/va_openGL2matlab.m
	DESTINATION ${VA_MATLAB_OUTPUT_FOLDER}
	COMPONENT ${PROJECT_NAME}
)
