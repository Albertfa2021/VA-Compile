target_sources (
	${PROJECT_NAME}
	PRIVATE VAAudiostreamTracker.h
			VACoreConfig.cpp
			VACoreConfig.h
			VACoreEventHandlerRegistry.cpp
			VACoreEventHandlerRegistry.h
			VACoreEventManager.cpp
			VACoreEventManager.h
			VACoreThread.cpp
			VACoreThread.h
			VAHardwareSetup.cpp
			VAHardwareSetup.h
			VALockfreeObjectPool.cpp
			VALockfreeObjectPool.h
			VALog.cpp
			VALog.h
			VAMacroMap.cpp
			VAMacroMap.h
			VAObjectContainer.h
			VAObjectPool.cpp
			VAPoolObject.cpp
			VAProfiler.cpp
			VAProfiler.h
			VAReferenceableObject.cpp
			VASourceTargetMetrics.cpp
			VASourceTargetMetrics.h
			VAStringtable.h
)

add_subdirectory (Audiosignals)
add_subdirectory (core)
add_subdirectory (DataHistory)
add_subdirectory (directivities)
add_subdirectory (Drivers/Audio)
add_subdirectory (Filtering)
add_subdirectory (Medium)
add_subdirectory (Motion)
add_subdirectory (Rendering)
add_subdirectory (Reproduction)
add_subdirectory (Scene)
add_subdirectory (SpatialEncoding)
add_subdirectory (Utils)
