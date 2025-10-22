cmake_minimum_required (VERSION 3.20 FATAL_ERROR)

project (VACoreBenchmarks)

add_executable (VACoreSourceManagementBenchmark VACoreSourceManagementBenchmark.cpp)
target_link_libraries (VACoreSourceManagementBenchmark PRIVATE VA::VACore)
set_property (TARGET VACoreSourceManagementBenchmark PROPERTY FOLDER "Benchmarks/VA/VACore")
install (TARGETS VACoreSourceManagementBenchmark RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
