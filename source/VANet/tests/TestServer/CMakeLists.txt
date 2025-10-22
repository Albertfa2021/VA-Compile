cmake_minimum_required (VERSION 3.20 FATAL_ERROR)

project (VANetServerTest)

add_executable (VANetServerTest VANetServerTest.cpp)
target_link_libraries (VANetServerTest PRIVATE VA::VANet VA::VACore)
set_property (TARGET VANetServerTest PROPERTY FOLDER "Tests/VA/VANet")
set_property (TARGET VANetServerTest PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
install (TARGETS VANetServerTest RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
