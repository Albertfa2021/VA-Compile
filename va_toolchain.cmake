# VA2022a Toolchain File
# This file configures the build environment for VA

# Add VistaCMakeCommon to module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/dependencies/VistaCMakeCommon")

# Set NatNetSDK directory (will be downloaded by CMake if not present)
# Note: This path will be created during CMake configuration
set(NatNetSDK_DIR "${CMAKE_CURRENT_LIST_DIR}/dependencies/NatNetSDK" CACHE PATH "NatNetSDK directory")

# Windows-specific settings
if(WIN32)
    # Prevent Windows.h from defining min/max macros
    add_compile_definitions(NOMINMAX)

    # Enable math constants like M_PI
    add_compile_definitions(_USE_MATH_DEFINES)

    # Use Unicode
    add_compile_definitions(UNICODE _UNICODE)
endif()

# Set installation directory
set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_LIST_DIR}/dist" CACHE PATH "Installation directory")

message(STATUS "VA Toolchain loaded")
message(STATUS "  CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}")
message(STATUS "  NatNetSDK_DIR: ${NatNetSDK_DIR}")
message(STATUS "  CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
