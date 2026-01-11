cmake_minimum_required(VERSION 3.16)

project(ContestLogX VERSION 0.0.9 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS 
    Core 
    Widgets 
    Network 
    SerialPort
    Xml
)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../WLOGTOOL
)

# Source files
set(SOURCES
    src/main.cpp
    src/ui/mainwindow.cpp
    src/ui/rigcontroldialog.cpp
    src/ui/freqmodedialog.cpp
    src/ui/cwwindow.cpp
    src/ui/cwmemoriesdialog.cpp
    src/ui/dxclusterpanel.cpp
    src/ui/scorewidget.cpp
    src/ui/stationsetupdialog.cpp
    src/stationclassdialog.cpp
    src/contestselectdialog.cpp
    src/ui/shortcutsdialog.cpp
    src/ui/qsoeditdialog.cpp
    src/ui/cabrillodialog.cpp
    src/ui/callhistorydialog.cpp
    src/ui/scpdialog.cpp
    src/ui/scpwidget.cpp
    src/ui/scplineedit.cpp
    src/contestengine.cpp
    src/core/qsorecord.cpp
    src/core/qsolistmodel.cpp
    src/core/filehandler.cpp
    src/core/contestinfo.cpp
    src/core/stationinfo.cpp
    src/core/exchangefield.cpp
    src/core/loadingworker.cpp
    src/core/scoringworker.cpp
    src/core/cabrilloexport.cpp
    src/callhistory.cpp
    src/supercheckpartial.cpp
    src/fileformats/clxfile.cpp
    src/rig/flrigclient.cpp
    src/utils/settings.cpp
    src/utils/debuglogger.cpp
    src/utils/bandplan.cpp
    src/utils/callsignutils.cpp
    src/DxccDatabase.cpp
)

set(HEADERS
    include/mainwindow.h
    include/rigcontroldialog.h
    include/stationsetupdialog.h
    include/stationclassdialog.h
    include/contestselectdialog.h
    include/shortcutsdialog.h
    include/cabrillodialog.h
    include/cabrilloexport.h
    include/callhistorydialog.h
    include/scpdialog.h
    include/scpwidget.h
    include/scplineedit.h
    include/qsoeditdialog.h
    include/contestengine.h
    include/scorewidget.h
    src/ui/freqmodedialog.h
    src/ui/cwwindow.h
    src/ui/cwmemoriesdialog.h
    src/ui/dxclusterpanel.h
    include/qsorecord.h
    include/qsolistmodel.h
    include/filehandler.h
    include/contestinfo.h
    include/stationinfo.h
    include/exchangefield.h
    include/loadingworker.h
    include/scoringworker.h
    include/clxfile.h
    include/flrigclient.h
    include/settings.h
    include/callhistory.h
    include/supercheckpartial.h
    include/debuglogger.h
    src/utils/bandplan.h
    src/utils/callsignutils.h
    include/DxccDatabase.h
)

# UI files
set(UI_FILES
    ui/mainwindow.ui
)

# Create executable
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${UI_FILES}
)

# Link Qt libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::SerialPort
    Qt6::Xml
)

# Installation
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# Enable testing
option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
