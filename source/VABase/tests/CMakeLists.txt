cmake_minimum_required (VERSION 3.20 FATAL_ERROR)

project (VABaseTest)
set (VABASE_TEST_FOLDER "Tests/VA/VABase")

add_executable (ObjectTest ObjectTest.cpp)
target_link_libraries (ObjectTest PUBLIC VA::VABase)
set_property (TARGET ObjectTest PROPERTY FOLDER ${VABASE_TEST_FOLDER})
set_property (TARGET ObjectTest PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
install (TARGETS ObjectTest RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable (StructTest StructTest.cpp)
target_link_libraries (StructTest PUBLIC VA::VABase)
set_property (TARGET StructTest PROPERTY FOLDER ${VABASE_TEST_FOLDER})
set_property (TARGET StructTest PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
install (TARGETS StructTest RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
