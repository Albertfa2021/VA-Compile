cmake_minimum_required (VERSION 3.20 FATAL_ERROR)

project (VAPython LANGUAGES CXX C)

set (CMAKE_DEBUG_POSTFIX "-d")

if (NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY AND WIN32)
	set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/bin)
	set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/lib)
	set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/lib)
endif ()

if (CMAKE_GENERATOR MATCHES "Visual Studio")
	set_property (GLOBAL PROPERTY USE_FOLDERS ON)
endif ()

# If build outside of VA project, eg via pip, ...
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	# ... build dependecies static so a single file contains everything needed.
	set (BUILD_SHARED_LIBS OFF)

	# Additionally, download all dependencies.
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

	if (NOT TARGET VABase)
		CPMAddPackage (
			NAME VABase
			GIT_REPOSITORY https://git.rwth-aachen.de/ita/VABase
			GIT_TAG master
		)
	endif ()

	if (NOT TARGET VANet)
		CPMAddPackage (
			NAME VANet
			GIT_REPOSITORY https://git.rwth-aachen.de/ita/VANet
			GIT_TAG master
		)
	endif ()
endif ()

if (PYTHON_SETUP_PY_VERSION)
	list (APPEND PYTHON_SETUP_PY_VERSION EXACT)
	message("${PYTHON_SETUP_PY_VERSION}")
endif ()

find_package (Python ${PYTHON_SETUP_PY_VERSION} REQUIRED COMPONENTS Interpreter Development)

# --- Version ---
# get the version from setup.py
execute_process (
	COMMAND ${Python_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/setup.py --version OUTPUT_VARIABLE VAPython_version
)
string (REGEX MATCHALL "([0-9]+)\\.([0-9]+)" _output ${VAPython_version})
unset (_output)

# use the result to set the project version
set (PROJECT_VERSION ${CMAKE_MATCH_0})
set (PROJECT_VERSION_MAJOR ${CMAKE_MATCH_1})
set (PROJECT_VERSION_MINOR ${CMAKE_MATCH_2})

# set the project version in cache for installation and packaging only
if (DEV_HELPERS)
	set (
		${PROJECT_NAME}_VERSION
		${PROJECT_VERSION}
		CACHE INTERNAL "Cache entry of the project versions"
	)
endif ()

# Add target 'va', the project name is *not* used here, so in python the module is called 'va'.
python_add_library (
	${PROJECT_NAME}
	MODULE
	src/vasingleton.cpp
	src/vasingletondoc.hpp
	src/vasingletonmethods.hpp
	MANIFEST.in
	setup.py
)
add_library (VA::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

# Linking
target_link_libraries (${PROJECT_NAME} PRIVATE VA::VANet)

# Required compiler features
target_compile_features (${PROJECT_NAME} PUBLIC cxx_std_17)

# If build outside of VA project, change IDE folder
if (NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	set_property (TARGET ${PROJECT_NAME} PROPERTY FOLDER "Bindings/VA")
endif ()

# Remove the debug postfix, so the module always has the same name. Also set the output dir to bin, so if shared build is enabled, the module can find all the dlls.
set_target_properties (${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX "" OUTPUT_NAME "va" 
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/bin")

# Only run this, if we are not building from setup.py
if (NOT PYTHON_SETUP_PY_BUILD)

	execute_process (COMMAND py --list OUTPUT_VARIABLE python_versions)
	string (REGEX MATCHALL "([0-9]+\\.[0-9]+)" python_versions ${python_versions})

	add_custom_target (${PROJECT_NAME}_wheel ALL)

	foreach (py_version ${python_versions})
		add_custom_command (
			TARGET ${PROJECT_NAME}_wheel
			POST_BUILD
			COMMAND py -${py_version} setup.py bdist_wheel
			WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
			COMMENT "Running distutils for version ${py_version}"
			VERBATIM
		)
	endforeach ()

	set_property (TARGET ${PROJECT_NAME}_wheel PROPERTY FOLDER "Bindings/VA")

	# ---Install---

	# prepare the install folder name
	math (EXPR RELEASE_LETTER "${PROJECT_VERSION_MINOR}+97")
	string (ASCII ${RELEASE_LETTER} RELEASE_LETTER)
	set (VA_PYTHON_OUTPUT_FOLDER "VAPython_v${PROJECT_VERSION_MAJOR}${RELEASE_LETTER}")

	# Readme
	install (
		FILES "README.md"
		DESTINATION ${VA_PYTHON_OUTPUT_FOLDER}
		COMPONENT ${PROJECT_NAME}
	)

	# Wheel files for installation on user machine
	install (
		DIRECTORY "dist/"
		DESTINATION ${VA_PYTHON_OUTPUT_FOLDER}
		COMPONENT ${PROJECT_NAME}
		FILES_MATCHING
		PATTERN "*.whl"
	)

	# Tests
	install (
		DIRECTORY "tests"
		DESTINATION ${VA_PYTHON_OUTPUT_FOLDER}
		COMPONENT ${PROJECT_NAME}
	)

	# Examples
	install (
		DIRECTORY "examples"
		DESTINATION ${VA_PYTHON_OUTPUT_FOLDER}
		COMPONENT ${PROJECT_NAME}
	)

endif ()
