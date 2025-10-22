cmake_minimum_required (VERSION 3.20 FATAL_ERROR)

project (VANetTestClient)

add_executable (VANetTestClient VANetClientTest.cpp)
target_link_libraries (VANetTestClient PRIVATE VA::VANet vista::vista_base)
set_property (TARGET VANetTestClient PROPERTY FOLDER "Tests/VA/VANet")
set_property (TARGET VANetTestClient PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
install (TARGETS VANetTestClient RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
