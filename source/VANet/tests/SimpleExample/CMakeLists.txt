cmake_minimum_required (VERSION 3.20 FATAL_ERROR)

project (VANetSimpleExampleScene)

add_executable (VANetSimpleExampleScene VANetSimpleExampleScene.cpp)
target_link_libraries (VANetSimpleExampleScene PRIVATE VA::VANet vista::vista_base)
set_property (TARGET VANetSimpleExampleScene PROPERTY FOLDER "Tests/VA/VANet")
set_property (TARGET VANetSimpleExampleScene PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
install (TARGETS VANetSimpleExampleScene RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
