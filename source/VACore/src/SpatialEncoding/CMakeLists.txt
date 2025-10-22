target_sources (
	${PROJECT_NAME} PRIVATE VASpatialEncoding.h VABinauralEncoding.h VABinauralEncoding.cpp VAAmbisonicsEncoding.h VAAmbisonicsEncoding.cpp VASpatialEncoding.cpp VAVBAPEncoding.h VAVBAPEncoding.cpp
)

add_subdirectory (VBAP)
