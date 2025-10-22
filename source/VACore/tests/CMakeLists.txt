cmake_minimum_required (VERSION 3.20 FATAL_ERROR)

project (VACoreTest)
set (VACORE_TEST_FOLDER "Tests/VA/VACore")

CPMAddPackage (
	NAME googletest
	GITHUB_REPOSITORY google/googletest
	GIT_TAG release-1.11.0
	VERSION 1.11.0
	OPTIONS "INSTALL_GTEST OFF" "gtest_force_shared_crt"
)

if (googletest_ADDED)
	set_target_properties (gmock gmock_main gtest gtest_main PROPERTIES FOLDER external_libs)
endif ()

add_executable (CoreTest CoreTest.cpp)
target_link_libraries (CoreTest PRIVATE VA::VACore)
set_property (TARGET CoreTest PROPERTY FOLDER ${VACORE_TEST_FOLDER})
set_property (TARGET CoreTest PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
install (TARGETS CoreTest RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable (MotionModelTest MotionModelTest.cpp)
target_link_libraries (MotionModelTest PRIVATE VA::VACore TBB::tbb)
set_property (TARGET MotionModelTest PROPERTY FOLDER ${VACORE_TEST_FOLDER})
set_property (TARGET MotionModelTest PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
install (TARGETS MotionModelTest RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable (DataHistoryModelTest DataHistoryModelTest.cpp)
target_link_libraries (DataHistoryModelTest PRIVATE VA::VACore gtest TBB::tbb)
set_property (TARGET DataHistoryModelTest PROPERTY FOLDER ${VACORE_TEST_FOLDER})
set_property (TARGET DataHistoryModelTest PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
install (TARGETS DataHistoryModelTest RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_custom_command (
	TARGET DataHistoryModelTest
	POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:gtest> $<TARGET_FILE_DIR:DataHistoryModelTest>
)
install (
	FILES $<TARGET_FILE:gtest>
	DESTINATION ${CMAKE_INSTALL_BINDIR}
	COMPONENT DataHistoryModelTest
)

add_executable (DataHistoryVsMotionModelTest DataHistoryVsMotionModelTest.cpp)
target_link_libraries (DataHistoryVsMotionModelTest PRIVATE VA::VACore TBB::tbb)
set_property (TARGET DataHistoryVsMotionModelTest PROPERTY FOLDER ${VACORE_TEST_FOLDER})
set_property (
	TARGET DataHistoryVsMotionModelTest PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
install (TARGETS DataHistoryVsMotionModelTest RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable (SceneConfigTest SceneConfigTest.cpp)
target_link_libraries (SceneConfigTest PRIVATE VA::VACore TBB::tbb)
set_property (TARGET SceneConfigTest PROPERTY FOLDER ${VACORE_TEST_FOLDER})
set_property (TARGET SceneConfigTest PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
install (TARGETS SceneConfigTest RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable (GenericPathRendererTest GenericPathRendererTest.cpp)
target_link_libraries (GenericPathRendererTest PRIVATE VA::VACore)
set_property (TARGET GenericPathRendererTest PROPERTY FOLDER ${VACORE_TEST_FOLDER})
set_property (TARGET GenericPathRendererTest PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
install (TARGETS GenericPathRendererTest RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable (NetworkStreamAudioSignalSourceTest NetworkStreamAudioSignalSourceTest.cpp)
target_link_libraries (NetworkStreamAudioSignalSourceTest PRIVATE VA::VACore)
set_property (TARGET NetworkStreamAudioSignalSourceTest PROPERTY FOLDER ${VACORE_TEST_FOLDER})
set_property (
	TARGET NetworkStreamAudioSignalSourceTest PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
install (TARGETS NetworkStreamAudioSignalSourceTest RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable (OfflineSimulationVirtualDeviceTest OfflineSimulationVirtualDeviceTest.cpp)
target_link_libraries (OfflineSimulationVirtualDeviceTest PRIVATE VA::VACore)
set_property (TARGET OfflineSimulationVirtualDeviceTest PROPERTY FOLDER ${VACORE_TEST_FOLDER})
set_property (
	TARGET OfflineSimulationVirtualDeviceTest PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
install (TARGETS OfflineSimulationVirtualDeviceTest RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable (HATORenderingTest HATORenderingTest.cpp)
target_link_libraries (HATORenderingTest PRIVATE VA::VACore)
set_property (TARGET HATORenderingTest PROPERTY FOLDER ${VACORE_TEST_FOLDER})
set_property (TARGET HATORenderingTest PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
install (TARGETS HATORenderingTest RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable (BinauralClusteringRendererTest BinauralClusteringRendererTest.cpp)
target_link_libraries (BinauralClusteringRendererTest PRIVATE VA::VACore)
set_property (TARGET BinauralClusteringRendererTest PROPERTY FOLDER ${VACORE_TEST_FOLDER})
set_property (
	TARGET BinauralClusteringRendererTest PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
install (TARGETS BinauralClusteringRendererTest RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable (BinauralOutdoorNoiseRendererTest BinauralOutdoorNoiseRendererTest.cpp)
target_link_libraries (BinauralOutdoorNoiseRendererTest PRIVATE VA::VACore)
set_property (TARGET BinauralOutdoorNoiseRendererTest PROPERTY FOLDER ${VACORE_TEST_FOLDER})
set_property (
	TARGET BinauralOutdoorNoiseRendererTest PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
install (TARGETS BinauralOutdoorNoiseRendererTest RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable (VBAPFreefieldRendererTest VBAPFreefieldRendererTest.cpp)
target_link_libraries (VBAPFreefieldRendererTest PRIVATE VA::VACore)
set_property (TARGET VBAPFreefieldRendererTest PROPERTY FOLDER ${VACORE_TEST_FOLDER})
set_property (
	TARGET VBAPFreefieldRendererTest PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
install (TARGETS VBAPFreefieldRendererTest RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable (AmbisonicsReproductionTest AmbisonicsReproductionTest.cpp)
target_link_libraries (AmbisonicsReproductionTest PRIVATE VA::VACore)
set_property (TARGET AmbisonicsReproductionTest PROPERTY FOLDER ${VACORE_TEST_FOLDER})
set_property (
	TARGET AmbisonicsReproductionTest PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
install (TARGETS AmbisonicsReproductionTest RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
