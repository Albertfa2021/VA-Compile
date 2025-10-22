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

CPMAddPackage (
	NAME vista
	GIT_REPOSITORY https://git.rwth-aachen.de/ita/ViSTA
	GIT_TAG feature/ita
	DOWNLOAD_ONLY YES
)

if (vista_ADDED)
	find_package (Git 2.29 REQUIRED)
	execute_process (
		COMMAND ${GIT_EXECUTABLE} apply --check "${CMAKE_CURRENT_LIST_DIR}/patches/remove_unary_function.patch"
		RESULT_VARIABLE patch_ok
		WORKING_DIRECTORY ${vista_SOURCE_DIR}
		ERROR_QUIET
	)

	if (${patch_ok} STREQUAL "0")
		execute_process (
			COMMAND ${GIT_EXECUTABLE} apply --whitespace=fix
					"${CMAKE_CURRENT_LIST_DIR}/patches/remove_unary_function.patch"
			RESULT_VARIABLE patch_ok
			WORKING_DIRECTORY ${vista_SOURCE_DIR}
		)
	endif ()

	# ##################################################################################################################
	# vista_base
	# ##################################################################################################################

	file (GLOB_RECURSE vista_base_sources ${vista_SOURCE_DIR}/VistaCoreLibs/VistaBase/**.cpp)

	file (GLOB_RECURSE vista_base_headers ${vista_SOURCE_DIR}/VistaCoreLibs/VistaBase/**.h)

	add_library (vista_base ${vista_base_sources} ${vista_base_headers})
	add_library (vista::vista_base ALIAS vista_base)

	set_target_properties (vista_base PROPERTIES PUBLIC_HEADER "${vista_base_headers}")
	target_compile_features (vista_base PUBLIC cxx_std_17)

	target_include_directories (
		vista_base PUBLIC $<BUILD_INTERFACE:${vista_SOURCE_DIR}/VistaCoreLibs>
						  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/VistaBase>
	)

	set_property (TARGET vista_base PROPERTY FOLDER "vista_libs")

	if (BUILD_SHARED_LIBS AND NOT ITA_VISTA_BUILD_STATIC)
		target_compile_definitions (vista_base PUBLIC VISTABASE_EXPORTS)
	else ()
		target_compile_definitions (vista_base PUBLIC VISTABASE_STATIC)
	endif ()

	packageProject (
		NAME vista_base
		VERSION 1.16.0
		BINARY_DIR ${PROJECT_BINARY_DIR}
		INCLUDE_DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/VistaBase
		NAMESPACE vista
		DISABLE_VERSION_SUFFIX YES
	)

	# ##################################################################################################################
	# vista_aspects
	# ##################################################################################################################

	file (GLOB_RECURSE vista_aspects_sources ${vista_SOURCE_DIR}/VistaCoreLibs/VistaAspects/**.cpp)

	file (GLOB_RECURSE vista_aspects_headers ${vista_SOURCE_DIR}/VistaCoreLibs/VistaAspects/**.h)

	add_library (vista_aspects ${vista_aspects_sources} ${vista_aspects_headers})
	add_library (vista::vista_aspects ALIAS vista_aspects)

	set_target_properties (vista_aspects PROPERTIES PUBLIC_HEADER "${vista_aspects_headers}")
	target_compile_features (vista_aspects PUBLIC cxx_std_17)

	target_include_directories (
		vista_aspects PUBLIC $<BUILD_INTERFACE:${vista_SOURCE_DIR}/VistaCoreLibs>
							 $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/VistaAspects>
	)

	target_link_libraries (vista_aspects PUBLIC vista::vista_base)

	if (UNIX AND NOT APPLE)
		target_link_libraries (vista_aspects PUBLIC rt)
	endif ()

	set_property (TARGET vista_aspects PROPERTY FOLDER "vista_libs")

	if (BUILD_SHARED_LIBS AND NOT ITA_VISTA_BUILD_STATIC)
		target_compile_definitions (vista_aspects PUBLIC VISTAASPECTS_EXPORTS)
	else ()
		target_compile_definitions (vista_aspects PUBLIC VISTAASPECTS_STATIC)
	endif ()

	packageProject (
		NAME vista_aspects
		VERSION 1.16.0
		BINARY_DIR ${PROJECT_BINARY_DIR}
		INCLUDE_DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/VistaAspects
		NAMESPACE vista
		DISABLE_VERSION_SUFFIX YES
	)

	# ##################################################################################################################
	# vista_inter_proc_comm
	# ##################################################################################################################

	file (GLOB_RECURSE vista_inter_proc_comm_sources ${vista_SOURCE_DIR}/VistaCoreLibs/VistaInterProcComm/**.cpp)

	file (GLOB_RECURSE vista_inter_proc_comm_headers ${vista_SOURCE_DIR}/VistaCoreLibs/VistaInterProcComm/**.h)

	add_library (vista_inter_proc_comm ${vista_inter_proc_comm_sources} ${vista_inter_proc_comm_headers})
	add_library (vista::vista_inter_proc_comm ALIAS vista_inter_proc_comm)

	set_target_properties (vista_inter_proc_comm PROPERTIES PUBLIC_HEADER "${vista_inter_proc_comm_headers}")
	target_compile_features (vista_inter_proc_comm PUBLIC cxx_std_17)
	target_compile_definitions (vista_inter_proc_comm PRIVATE _HAS_STD_BYTE=0)

	target_include_directories (
		vista_inter_proc_comm PUBLIC $<BUILD_INTERFACE:${vista_SOURCE_DIR}/VistaCoreLibs>
									 $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/VistaInterProcComm>
	)

	target_link_libraries (vista_inter_proc_comm PUBLIC vista::vista_base vista::vista_aspects)

	if (WIN32)
		target_compile_definitions (vista_inter_proc_comm PUBLIC _USE_WIN32_MUTEX)
		target_link_libraries (vista_inter_proc_comm PUBLIC winmm ws2_32)
	elseif (UNIX)
		target_link_libraries (vista_inter_proc_comm PUBLIC pthread)
	endif ()

	option (VISTACORELIBS_IPC_USE_EXCEPTIONS "Specifies wether or nor the IPC throws exceptions when errors occur" ON)

	if (VISTACORELIBS_IPC_USE_EXCEPTIONS)
		target_compile_definitions (vista_inter_proc_comm PUBLIC VISTA_IPC_USE_EXCEPTIONS)
	endif (VISTACORELIBS_IPC_USE_EXCEPTIONS)

	set_property (TARGET vista_inter_proc_comm PROPERTY FOLDER "vista_libs")

	if (BUILD_SHARED_LIBS AND NOT ITA_VISTA_BUILD_STATIC)
		target_compile_definitions (vista_inter_proc_comm PUBLIC VISTAINTERPROCCOMM_EXPORTS)
	else ()
		target_compile_definitions (vista_inter_proc_comm PUBLIC VISTAINTERPROCCOMM_STATIC)
	endif ()

	packageProject (
		NAME vista_inter_proc_comm
		VERSION 1.16.0
		BINARY_DIR ${PROJECT_BINARY_DIR}
		INCLUDE_DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/VistaInterProcComm
		NAMESPACE vista
		DISABLE_VERSION_SUFFIX YES
	)

	# ##################################################################################################################
	# vista_math
	# ##################################################################################################################

	file (GLOB_RECURSE vista_math_sources ${vista_SOURCE_DIR}/VistaCoreLibs/VistaMath/**.cpp)

	file (GLOB_RECURSE vista_math_headers ${vista_SOURCE_DIR}/VistaCoreLibs/VistaMath/**.h)

	add_library (vista_math ${vista_math_sources} ${vista_math_headers})
	add_library (vista::vista_math ALIAS vista_math)

	set_target_properties (vista_math PROPERTIES PUBLIC_HEADER "${vista_math_headers}")
	target_compile_features (vista_math PUBLIC cxx_std_17)

	target_include_directories (
		vista_math PUBLIC $<BUILD_INTERFACE:${vista_SOURCE_DIR}/VistaCoreLibs>
						  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/VistaMath>
	)

	target_link_libraries (vista_math PUBLIC vista::vista_base)

	set_property (TARGET vista_math PROPERTY FOLDER "vista_libs")

	if (BUILD_SHARED_LIBS AND NOT ITA_VISTA_BUILD_STATIC)
		target_compile_definitions (vista_math PUBLIC VISTAMATH_EXPORTS)
	else ()
		target_compile_definitions (vista_math PUBLIC VISTAMATH_STATIC)
	endif ()

	packageProject (
		NAME vista_math
		VERSION 1.16.0
		BINARY_DIR ${PROJECT_BINARY_DIR}
		INCLUDE_DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/VistaMath
		NAMESPACE vista
		DISABLE_VERSION_SUFFIX YES
	)

	# ##################################################################################################################
	# vista_tools
	# ##################################################################################################################

	file (GLOB vista_tools_sources ${vista_SOURCE_DIR}/VistaCoreLibs/VistaTools/**.cpp)

	file (GLOB vista_tools_headers ${vista_SOURCE_DIR}/VistaCoreLibs/VistaTools/**.h)

	add_library (vista_tools ${vista_tools_sources} ${vista_tools_headers})
	add_library (vista::vista_tools ALIAS vista_tools)

	set_target_properties (vista_tools PROPERTIES PUBLIC_HEADER "${vista_tools_headers}")
	target_compile_features (vista_tools PUBLIC cxx_std_17)

	target_include_directories (
		vista_tools PUBLIC $<BUILD_INTERFACE:${vista_SOURCE_DIR}/VistaCoreLibs>
						   $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/VistaTools>
	)

	target_link_libraries (
		vista_tools PUBLIC vista::vista_base vista::vista_aspects vista::vista_inter_proc_comm vista::vista_math
	)

	if (WIN32)
		target_link_libraries (vista_tools PUBLIC PowrProf dbghelp Psapi)
	else ()
		target_link_libraries (vista_tools PUBLIC dl)
	endif ()

	set_property (TARGET vista_tools PROPERTY FOLDER "vista_libs")

	if (BUILD_SHARED_LIBS AND NOT ITA_VISTA_BUILD_STATIC)
		target_compile_definitions (vista_tools PUBLIC VISTATOOLS_EXPORTS)
	else ()
		target_compile_definitions (vista_tools PUBLIC VISTATOOLS_STATIC)
	endif ()

	packageProject (
		NAME vista_tools
		VERSION 1.16.0
		BINARY_DIR ${PROJECT_BINARY_DIR}
		INCLUDE_DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/VistaTools
		NAMESPACE vista
		DISABLE_VERSION_SUFFIX YES
	)

endif ()
