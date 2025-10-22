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

if (NOT TARGET IHTAUtilityLibs)
	CPMAddPackage (
		NAME IHTAUtilityLibs
		GIT_REPOSITORY https://git.rwth-aachen.de/ita/ihtautilitylibs
		GIT_TAG master
	)
endif ()
