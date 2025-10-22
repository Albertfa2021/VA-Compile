cmake_minimum_required( VERSION 2.8 )

project( VALua )

list( APPEND CMAKE_MODULE_PATH "$ENV{VISTA_CMAKE_COMMON}" )
include( VistaCommon )

# dependencies
vista_use_package( Lua REQUIRED )
vista_use_package( VistaCoreLibs REQUIRED COMPONENTS VistaTools FIND_DEPENDENCIES )
vista_use_package( VABase REQUIRED FIND_DEPENDENCIES )


# Build settings
if( BUILD_SHARED_LIBS )
	add_definitions( -DVALUA_EXPORTS )
else( )
	add_definitions( -DVALUA_STATIC -DVABASE_STATIC )
endif( )

if( ITA_VISTA_BUILD_STATIC )
	add_definitions( -DVISTA_STATIC )
endif( )

if( NOT WIN32 )
	add_definitions( -std=gnu++11 )
endif( )


# includes
include_directories( "include" )


# Including the source files of all source subfolders recursively
include( "include/_SourceFiles.cmake" )
include( "src/_SourceFiles.cmake" )


# linker
add_library( VALua ${ProjectSources} )
target_link_libraries( VALua ${VISTA_USE_PACKAGE_LIBRARIES} )


# config
vista_configure_lib( VALua )
vista_install( VALua )
set( VALUA_INCLUDE_OUTDIR "${CMAKE_CURRENT_SOURCE_DIR}/include" )
vista_create_cmake_configs( VALua )
vista_create_default_info_file( VALua )

set_property( TARGET VALua PROPERTY FOLDER "VA/Bindings" )
