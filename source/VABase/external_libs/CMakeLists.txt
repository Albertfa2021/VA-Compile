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
	NAME GrpSrcByDir
	GITHUB_REPOSITORY TheLartians/GroupSourcesByFolder.cmake
	VERSION 1.0
	DOWNLOAD_ONLY YES # Delay the inclusion of this to after the patch
)

if (GrpSrcByDir_ADDED)
	find_package (Git 2.29 REQUIRED)
	execute_process (
		COMMAND ${GIT_EXECUTABLE} apply --check "${CMAKE_CURRENT_LIST_DIR}/patches/source_group_rel_path.patch"
		RESULT_VARIABLE patch_ok
		WORKING_DIRECTORY ${GrpSrcByDir_SOURCE_DIR}
		ERROR_QUIET
	)

	if (${patch_ok} STREQUAL "0")
		execute_process (
			COMMAND ${GIT_EXECUTABLE} apply --whitespace=fix
					"${CMAKE_CURRENT_LIST_DIR}/patches/source_group_rel_path.patch"
			RESULT_VARIABLE patch_ok
			WORKING_DIRECTORY ${GrpSrcByDir_SOURCE_DIR}
		)
	endif ()

	# The file is now pathced, add it
	add_subdirectory (${GrpSrcByDir_SOURCE_DIR} ${GrpSrcByDir_BINARY_DIR})
endif ()

CPMAddPackage (
	NAME PkgProj
	VERSION 1.8.0
	GITHUB_REPOSITORY TheLartians/PackageProject.cmake
	DOWNLOAD_ONLY YES # Delay the inclusion of this to after the patch
)

if (PkgProj_ADDED)
	find_package (Git 2.29 REQUIRED)
	execute_process (
		COMMAND ${GIT_EXECUTABLE} apply --check "${CMAKE_CURRENT_LIST_DIR}/patches/change_install_dir.patch"
		RESULT_VARIABLE patch_ok
		WORKING_DIRECTORY ${PkgProj_SOURCE_DIR}
		ERROR_QUIET
	)

	if (${patch_ok} STREQUAL "0")
		execute_process (
			COMMAND ${GIT_EXECUTABLE} apply --whitespace=fix
					"${CMAKE_CURRENT_LIST_DIR}/patches/change_install_dir.patch"
			RESULT_VARIABLE patch_ok
			WORKING_DIRECTORY ${PkgProj_SOURCE_DIR}
		)
	endif ()

	# The file is now pathced, add it
	add_subdirectory (${PkgProj_SOURCE_DIR} ${PkgProj_BINARY_DIR})
endif ()

CPMAddPackage (
	NAME Format.cmake
	VERSION 1.7.2
	GITHUB_REPOSITORY TheLartians/Format.cmake
	OPTIONS "FORMAT_SKIP_CMAKE NO"
)

if (Format.cmake_ADDED)
	set_target_properties (format check-format fix-format PROPERTIES FOLDER "format_tools")
	set_target_properties (
		clang-format cmake-format check-clang-format check-cmake-format fix-clang-format fix-cmake-format
		PROPERTIES FOLDER "format_tools/helpers"
	)
endif ()
