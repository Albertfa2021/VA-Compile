cmake_minimum_required (VERSION 3.20 FATAL_ERROR)

project (VANetTest)

add_subdirectory (SimpleExample)
add_subdirectory (TestClient)

if (ITA_VA_WITH_CORE)
	add_subdirectory (TestServer)
endif ()
