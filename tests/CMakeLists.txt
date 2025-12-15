# Tests CMakeLists.txt for ContestLogX

cmake_minimum_required(VERSION 3.16)

# Enable testing
enable_testing()

# Find Qt Test module
find_package(Qt6 REQUIRED COMPONENTS Test)

# Include parent directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src)

# Helper function to create a test
function(add_clx_test test_name test_source)
    add_executable(${test_name} ${test_source} ${ARGN})
    target_link_libraries(${test_name} PRIVATE Qt6::Test Qt6::Core Qt6::Widgets Qt6::Network)
    add_test(NAME ${test_name} COMMAND ${test_name})
    set_tests_properties(${test_name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

# QsoRecord tests
add_clx_test(test_qsorecord test_qsorecord.cpp 
    ${CMAKE_SOURCE_DIR}/src/core/qsorecord.cpp)

# Settings tests
add_clx_test(test_settings test_settings.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/settings.cpp)

# CallsignUtils tests
add_clx_test(test_callsignutils test_callsignutils.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/callsignutils.cpp)
