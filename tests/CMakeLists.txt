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
    target_link_libraries(${test_name} PRIVATE Qt6::Test Qt6::Core Qt6::Widgets Qt6::Network Qt6::Xml)
    add_test(NAME ${test_name} COMMAND ${test_name})
    set_tests_properties(${test_name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

# QsoRecord tests
add_clx_test(test_qsorecord test_qsorecord.cpp 
    ${CMAKE_SOURCE_DIR}/src/core/qsoRecord.cpp)

# Settings tests
add_clx_test(test_settings test_settings.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/settings.cpp)

# CallsignUtils tests
add_clx_test(test_callsignutils test_callsignutils.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/callsignUtils.cpp)

# BandPlan tests
add_clx_test(test_bandplan test_bandplan.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/bandPlan.cpp)

# Dupe Checking tests
add_clx_test(test_dupecheck test_dupecheck.cpp
    ${CMAKE_SOURCE_DIR}/src/core/qsoRecord.cpp)

# TODO: Portable Callsigns tests
# test_portable_callsigns.cpp provides comprehensive unit tests for:
#   - US portable operation (/P, /MM, /AE, /AG, /M suffixes)
#   - US portable with alternate location (/W[0-9] suffixes)
#   - DX portable operation (/JA, /VE, /VP2, /EA8, etc.)
#   - ITU zone and region mapping for portable calls
#   - stripPortableSuffixes() function validation
# Run with: ctest -V -R portable

# BandMap tests (header included so AUTOMOC generates moc for Q_OBJECT classes)
add_clx_test(test_bandmap test_bandmap.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/bandMapWidget.cpp
    ${CMAKE_SOURCE_DIR}/include/bandMapWidget.h)

# TODO: Add ContestEngine multiplier tests
# Requires refactoring ContestEngine to be more testable (mock DxccDatabase)
