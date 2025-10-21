# FindNatNetSDK.cmake
# Locate NatNet SDK
#
# This module defines:
#  NatNetSDK_FOUND - if false, do not try to link to NatNetSDK
#  NatNetSDK_LIBRARIES - libraries to link against
#  NatNetSDK_INCLUDE_DIRS - where to find NatNetSDK headers
#  NatNetSDK_LIBRARY_DIRS - where to find NatNetSDK libraries

if(WIN32)
    if(DEFINED NatNetSDK_DIR)
        set(NatNetSDK_ROOT_DIR ${NatNetSDK_DIR})
    endif()

    find_path(NatNetSDK_INCLUDE_DIR
        NAMES NatNetTypes.h
        PATHS
            ${NatNetSDK_ROOT_DIR}/include
            ${NatNetSDK_DIR}/include
        NO_DEFAULT_PATH
    )

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(NATNET_LIB_SUFFIX "x64")
    else()
        set(NATNET_LIB_SUFFIX "")
    endif()

    find_library(NatNetSDK_LIBRARY
        NAMES NatNet NatNetLib
        PATHS
            ${NatNetSDK_ROOT_DIR}/lib
            ${NatNetSDK_ROOT_DIR}/lib/${NATNET_LIB_SUFFIX}
            ${NatNetSDK_DIR}/lib
            ${NatNetSDK_DIR}/lib/${NATNET_LIB_SUFFIX}
        NO_DEFAULT_PATH
    )

    if(NatNetSDK_INCLUDE_DIR AND NatNetSDK_LIBRARY)
        set(NatNetSDK_FOUND TRUE)
        set(NatNetSDK_INCLUDE_DIRS ${NatNetSDK_INCLUDE_DIR})
        set(NatNetSDK_LIBRARIES ${NatNetSDK_LIBRARY})
        get_filename_component(NatNetSDK_LIBRARY_DIRS ${NatNetSDK_LIBRARY} DIRECTORY)

        # Create imported target
        if(NOT TARGET NatNetSDK::NatNetSDK)
            add_library(NatNetSDK::NatNetSDK UNKNOWN IMPORTED)
            set_target_properties(NatNetSDK::NatNetSDK PROPERTIES
                IMPORTED_LOCATION "${NatNetSDK_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${NatNetSDK_INCLUDE_DIR}"
            )
        endif()

        if(NOT NatNetSDK_FIND_QUIETLY)
            message(STATUS "Found NatNetSDK: ${NatNetSDK_LIBRARY}")
        endif()
    else()
        set(NatNetSDK_FOUND FALSE)
        if(NatNetSDK_FIND_REQUIRED)
            message(FATAL_ERROR "Could not find NatNetSDK")
        endif()
    endif()
else()
    set(NatNetSDK_FOUND FALSE)
    if(NatNetSDK_FIND_REQUIRED)
        message(FATAL_ERROR "NatNetSDK is only available on Windows")
    endif()
endif()

mark_as_advanced(
    NatNetSDK_INCLUDE_DIR
    NatNetSDK_LIBRARY
)
