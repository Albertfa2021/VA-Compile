cmake_minimum_required (VERSION 3.20 FATAL_ERROR)

project (
	VA
	VERSION 2022.0
	LANGUAGES CXX C
)

# Set the install root folder if not specified by the user
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	set (
		CMAKE_INSTALL_PREFIX
		${CMAKE_CURRENT_BINARY_DIR}/dist
		CACHE PATH "Install root directory" FORCE
	)
	set (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT OFF)
endif (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

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

# Shared libs switch
option (BUILD_SHARED_LIBS "Build shared libraries" ON)

# Switches for components
option (ITA_VA_WITH_CORE "Include VA core library" OFF)
option (ITA_VA_WITH_SERVER_APP "Include VA server application (requires ITA_VA_WITH_CORE)" OFF)
option (ITA_VA_WITH_TESTS "Include VA tests" OFF)
option (ITA_VA_GENERATE_DOCUMENTATION "Generate source VA documentation (requires Doxygen)" OFF)
option (ITA_VA_WITH_BENCHMARKS "Include VA benchmarks" OFF)

# Bindings
option (ITA_VA_WITH_BINDING_MATLAB "Include Matlab binding for VA (requires Matlab and NatNetSDK)" OFF)
option (ITA_VA_WITH_BINDING_CS "Include C# binding for VA" ON)
option (ITA_VA_WITH_BINDING_PYTHON "Include Python binding for VA (requires Python)" OFF)
option (ITA_VA_WITH_UNITY_SCRIPTS "Include VA Unity C# scripts" ON)
option (ITA_VA_WITH_UNITY_EXTENSIONS_SCRIPTS
		"Include VA Unity Extensions C# scripts (requires further Unity packages like LHM and VRTK)" OFF
)

# Version
math (EXPR VA_RELEASE_LETTER "${PROJECT_VERSION_MINOR}+97")
string (ASCII ${VA_RELEASE_LETTER} VA_RELEASE_LETTER)

# ---End: Options---

# ---VA submodules---

add_subdirectory (VABase)
add_subdirectory (VANet)

if (ITA_VA_WITH_CORE)
	add_subdirectory (VACore)
	if (ITA_VA_COPYING_GNU_GPL)
		message (
			"The VACore configuration you have selected requires copying under the terms of the GNU General Public License. This license includes a copyleft. If you are linking against this library version, make sure that the license agreement is not violated. Will add the required COPYING file to the distribution."
		)

	endif ()
endif ()

if (ITA_VA_WITH_SERVER_APP)
	if (NOT ITA_VA_WITH_CORE)
		message (SEND_ERROR "Cannot build VAServer without VACore. Activate ITA_VA_WITH_CORE first")
	endif ()
	add_subdirectory (VAServer)
endif ()

if (ITA_VA_WITH_BINDING_MATLAB)
	add_subdirectory (VAMatlab)
endif ()

if (ITA_VA_WITH_BINDING_CS)
	add_subdirectory (VACS)
endif ()

if (ITA_VA_WITH_BINDING_PYTHON)
	add_subdirectory (VAPython)
endif ()

# ---END: VA submodules---

# Add all-build target (can be used to check whether RAVEN libraries are already included)
add_custom_target (
	${PROJECT_NAME}
	DEPENDS VA::VABase VA::VANet
	COMMAND ${CMAKE_COMMAND} -E echo "---------------------------------------------------------"
	COMMAND ${CMAKE_COMMAND} -E echo "Finished building all Virtual Acoustics (VA) subprojects."
	COMMAND ${CMAKE_COMMAND} -E echo "---------------------------------------------------------"
)
if (ITA_VA_WITH_CORE)
	add_dependencies (${PROJECT_NAME} VA::VACore)
endif ()
if (ITA_VA_WITH_BINDING_CS)
	add_dependencies (${PROJECT_NAME} VA::VANetCSWrapper)
endif ()
if (ITA_VA_WITH_BINDING_MATLAB)
	add_dependencies (${PROJECT_NAME} VAMatlab)
endif ()
if (ITA_VA_WITH_BINDING_PYTHON)
	add_dependencies (${PROJECT_NAME} VA::VAPython)
endif ()
if (ITA_VA_WITH_SERVER_APP)
	add_dependencies (${PROJECT_NAME} VAServer)
endif ()
set_property (TARGET ${PROJECT_NAME} PROPERTY FOLDER "BuildProjectCollections")

# Documentation
if (ITA_VA_GENERATE_DOCUMENTATION)

	set (VAWEBSITE_FOUND OFF)
	if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/VAWebsite")
		set (VAWEBSITE_FOUND ON)
		set (VAWEBSITE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/VAWebsite")
	endif ()

	add_subdirectory ("doc")

endif ()

# ---Install---

# (Selective) license, readme and logo
install (
	FILES README.md
	DESTINATION .
	COMPONENT ${PROJECT_NAME}
)
install (
	DIRECTORY logo
	DESTINATION .
	COMPONENT ${PROJECT_NAME}
)

if (ITA_VA_COPYING_GNU_GPL)

	set (SELECTED_LICENSE "GNU General Public License")
	set (GPL_DEPENDENCIES "FFTW3")
	configure_file (COPYING.proto "${CMAKE_CURRENT_SOURCE_DIR}/COPYING" @ONLY)
	install (
		FILES COPYING
		DESTINATION .
		COMPONENT ${PROJECT_NAME}
	)

else ()

	install (
		FILES LICENSE.md
		DESTINATION .
		COMPONENT ${PROJECT_NAME}
	)

endif ()

# Unity scripts
if (ITA_VA_WITH_UNITY_SCRIPTS)
	install (
		DIRECTORY "VAUnity"
		DESTINATION "VAUnity_v${PROJECT_VERSION_MAJOR}${VA_RELEASE_LETTER}"
		COMPONENT ${PROJECT_NAME}
	)
endif ()
if (ITA_VA_WITH_UNITY_EXTENSIONS_SCRIPTS)
	install (
		DIRECTORY "VAUnityExtensions"
		DESTINATION "VAUnityExtensions_v${PROJECT_VERSION_MAJOR}${VA_RELEASE_LETTER}"
		COMPONENT ${PROJECT_NAME}
	)
endif ()
