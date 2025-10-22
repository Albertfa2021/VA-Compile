# https://cmake.org/cmake/help/latest/manual/cmake-packages.7.html
# https://cmake.org/cmake/help/latest/manual/cmake-developer.7.html#manual:cmake-developer(7)

# ${NatNetSDK_DIR}
# ${NatNetSDK_SEARCH_DIR}
# $ENV{NatNetSDK_DIR}
# $ENV{NatNetSDK_SEARCH_DIR}

message(DEBUG "FindNatNetSDK")


message(DEBUG "NatNetSDK_DIR: ${NatNetSDK_DIR}")
message(DEBUG "NatNetSDK_DIR (ENV): $ENV{NatNetSDK_DIR}")

if(DEFINED NatNetSDK_DIR OR DEFINED ENV{NatNetSDK_DIR})
  # If an explicit path has been set, use that.
  if (DEFINED NatNetSDK_DIR)
	#get_filename_component() might be depricated in the future (got replaced in 3.20)
    get_filename_component(_NatNetSDK_DIR ${NatNetSDK_DIR} REALPATH)
	#file(REAL_PATH ${NatNetSDK_DIR} _NatNetSDK_DIR)
  else()
    get_filename_component(_NatNetSDK_DIR $ENV{NatNetSDK_DIR} REALPATH)
	#file(REAL_PATH $ENV{NatNetSDK_DIR} _NatNetSDK_DIR)
  endif()
  message(STATUS "Using explicit NatNetSDK directory path: ${_NatNetSDK_DIR}")
  # Don't know how to infer a version if not based on the official filenames.
  if (NOT DEFINED NatNetSDK_VERSION )
	message(STATUS "Unknown NatNetSDK package version")
  endif()
 
else ()
	message(FATAL_ERROR "No NatNetSDK directory specified")
endif()

if(WIN32)
  set(_NatNetSDK_LIBRARY_NAME "NatNetLib") # Standalone C API
  set(_NatNetSDK_LIBRARY_DIR ${_NatNetSDK_DIR}/lib/x64)
  set(_NatNetSDK_INCLUDE_DIR ${_NatNetSDK_DIR}/include/)
#elseif(APPLE)
#  set(_NatNetSDK_LIBRARY_NAME "NatNetLib") # Standalone C API
#  set(_NatNetSDK_LIBRARY_DIR ${_NatNetSDK_DIR})
#  set(_NatNetSDK_INCLUDE_DIR ${_NatNetSDK_DIR})
else()
      message(FATAL_ERROR "NatNetSDK is only available on Win")
endif()

message(DEBUG "_NatNetSDK_LIBRARY_DIR: ${_NatNetSDK_LIBRARY_DIR}")
message(DEBUG "_NatNetSDK_INCLUDE_DIR: ${_NatNetSDK_INCLUDE_DIR}")

find_path(NatNetSDK_INCLUDE_DIR "NatNetClient.h"
  HINTS ${_NatNetSDK_INCLUDE_DIR}
)
# Standalone C API
find_library(NatNetSDK_LIBRARY ${_NatNetSDK_LIBRARY_NAME}
  HINTS ${_NatNetSDK_LIBRARY_DIR}
)


mark_as_advanced(
  NatNetSDK_INCLUDE_DIR
  NatNetSDK_LIBRARY
)

message(DEBUG "NatNetSDK_LIBRARY: ${NatNetSDK_LIBRARY}")
message(DEBUG "NatNetSDK_LIVE_LIBRARY: ${NatNetSDK_LIVE_LIBRARY}")
message(DEBUG "NatNetSDK_INCLUDE_DIR: ${NatNetSDK_INCLUDE_DIR}")
message(DEBUG "NatNetSDK_COMMON_PREFERENCES_LIBRARY: ${NatNetSDK_COMMON_PREFERENCES_LIBRARY}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(NatNetSDK
  REQUIRED_VARS
    NatNetSDK_LIBRARY
    NatNetSDK_INCLUDE_DIR
  VERSION_VAR
    NatNetSDK_VERSION)

if(NatNetSDK_FOUND)
  set(NatNetSDK_LIBRARIES ${NatNetSDK_LIBRARY} )
  set(NatNetSDK_INCLUDE_DIRS ${NatNetSDK_INCLUDE_DIR})

  # These DLLs needs to be loaded by the consumer of the standalone SketchUp C API.
  if(WIN32)
    set(NatNetSDK_BINARIES
      ${_NatNetSDK_LIBRARY_DIR}/${_NatNetSDK_LIBRARY_NAME}.dll
    )
  endif()
endif()

# https://cmake.org/cmake/help/latest/manual/cmake-developer.7.html#a-sample-find-module
# https://cmake.org/cmake/help/latest/manual/cmake-buildsystem.7.html#imported-targets
# Professional CMake - Chapter 5.2.
if(NatNetSDK_FOUND AND NOT TARGET NatNetSDK)
  #if(APPLE)
    #set(NatNetSDK_LIBRARY "${NatNetSDK_LIBRARY}/${_NatNetSDK_LIBRARY_NAME}")
  #endif()

  # Standalone C API
  add_library(NatNetSDK SHARED IMPORTED)
  set_target_properties(NatNetSDK PROPERTIES
    IMPORTED_LOCATION "${NatNetSDK_LIBRARY}"
    IMPORTED_IMPLIB "${NatNetSDK_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${NatNetSDK_INCLUDE_DIR}"
  )

  if(WIN32)
    # Overwrite IMPORTED_LOCATION on Windows to the dll file.
    set_target_properties(NatNetSDK PROPERTIES
      IMPORTED_LOCATION "${NatNetSDK_BINARIES}"
    )
  endif()

  add_library(NatNetSDK::NatNetSDK ALIAS NatNetSDK)
  
endif()
