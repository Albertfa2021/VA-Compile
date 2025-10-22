cmake_minimum_required (VERSION 3.20 FATAL_ERROR)

project (VACSDummyWrapper)

if (NOT BUILD_SHARED_LIBS)
	message (
		"Ignoring activated BUILD_SHARED_LIBS temporary because static VA requested, but a C# compatible native wrapper has to be a shared library"
	)
endif (NOT BUILD_SHARED_LIBS)

add_library (VACSDummyWrapper SHARED VACSDummyWrapper.h VACSDummyWrapper.cpp)
target_include_directories (
	VACSDummyWrapper PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
							$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/VACS>
)

#target_link_libraries( VACSDummyWrapper PRIVATE VA::VANetCSWrapper ) #TODO: VANetCSWrapper required?
set_property (TARGET VACSDummyWrapper PROPERTY FOLDER "Tests/VA/VACS")
set_property (TARGET VACSDummyWrapper PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

target_link_libraries (${PROJECT_NAME} PUBLIC VA::VANetCSWrapper)

# install by copying DLL/SO next to C# wrapper

install (TARGETS VACSDummyWrapper RUNTIME DESTINATION "csharp" COMPONENT ${PROJECT_NAME})

# TODO: Whether code above has same results set( SHARED_LIBRARY_EXTENSION "dll" ) if( NOT WIN32 ) set(
# SHARED_LIBRARY_EXTENSION "so" ) endif( )

#if( EXISTS( "${VANET_LIBRARY_DIRS}/VACSDummyWrapper.${SHARED_LIBRARY_EXTENSION}" ) )
#	install( FILES "${VANET_LIBRARY_DIRS}/VACSDummyWrapper.${SHARED_LIBRARY_EXTENSION}" DESTINATION "csharp" )
#endif( )

#if( EXISTS( "${VANET_LIBRARY_DIRS}/VACSDummyWrapperD.${SHARED_LIBRARY_EXTENSION}" ) )
#	install( FILES "${VANET_LIBRARY_DIRS}/VACSDummyWrapperD.${SHARED_LIBRARY_EXTENSION}" DESTINATION "csharp" )
#endif( )
