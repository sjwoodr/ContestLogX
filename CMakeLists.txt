cmake_minimum_required(VERSION 3.16)

project(ContestLogX VERSION 0.6.13 LANGUAGES CXX)

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
    Xml
)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Source files
set(SOURCES
    src/main.cpp
    src/ui/mainWindow.cpp
    src/ui/rigControlDialog.cpp
    src/ui/freqModeDialog.cpp
    src/ui/cwWindow.cpp
    src/ui/cwMemoriesDialog.cpp
    src/ui/ssbMemoriesDialog.cpp
    src/ui/ssbKeyingSetupDialog.cpp
    src/ui/ssbMemoriesWidget.cpp
    src/ui/dxClusterPanel.cpp
    src/ui/bandMapWidget.cpp
    src/ui/scoreWidget.cpp
    src/ui/rateWidget.cpp
    src/ui/multiplierWidget.cpp
    src/ui/preferencesDialog.cpp
    src/stationClassDialog.cpp
    src/contestSelectDialog.cpp
    src/ui/shortcutsDialog.cpp
    src/ui/qsoEditDialog.cpp
    src/ui/cabrilloDialog.cpp
    src/ui/callHistoryDialog.cpp
    src/ui/scpDialog.cpp
    src/ui/scpWidget.cpp
    src/ui/scpLineEdit.cpp
    src/ui/termsDialog.cpp
    src/contestEngine.cpp
    src/core/qsoRecord.cpp
    src/core/qsoListModel.cpp
    src/core/fileHandler.cpp
    src/core/contestInfo.cpp
    src/core/stationInfo.cpp
    src/core/exchangeField.cpp
    src/core/loadingWorker.cpp
    src/core/scoringWorker.cpp
    src/callHistory.cpp
    src/superCheckPartial.cpp
    src/fileformats/clxFile.cpp
    src/fileformats/adifFile.cpp
    src/fileformats/cabrilloFile.cpp
    src/fileformats/csvFile.cpp
    src/rig/flrigClient.cpp
    src/utils/settings.cpp
    src/utils/debugLogger.cpp
    src/utils/bandPlan.cpp
    src/utils/callsignUtils.cpp
    src/utils/ttsManager.cpp
    src/utils/piperManager.cpp
    src/database/dxccDatabase.cpp
    src/qrzcqApi.cpp
    src/qrzApi.cpp
)

set(HEADERS
    include/mainWindow.h
    include/rigControlDialog.h
    include/stationClassDialog.h
    include/contestSelectDialog.h
    include/shortcutsDialog.h
    include/preferencesDialog.h
    include/theme.h
    include/cabrilloDialog.h
    include/cabrilloFile.h
    include/adifFile.h
    include/csvFile.h
    include/callHistoryDialog.h
    include/scpDialog.h
    include/scpWidget.h
    include/scpLineEdit.h
    include/termsDialog.h
    include/qsoEditDialog.h
    include/contestEngine.h
    include/scoreWidget.h
    include/rateWidget.h
    include/multiplierWidget.h
    src/ui/freqModeDialog.h
    src/ui/cwWindow.h
    src/ui/cwMemoriesDialog.h
    src/ui/ssbMemoriesDialog.h
    src/ui/ssbKeyingSetupDialog.h
    include/ssbMemoriesWidget.h
    src/ui/dxClusterPanel.h
    include/bandMapWidget.h
    include/qsoRecord.h
    include/qsoListModel.h
    include/fileHandler.h
    include/contestInfo.h
    include/stationInfo.h
    include/exchangeField.h
    include/loadingWorker.h
     include/scoringWorker.h
     include/clxFile.h
     include/flrigClient.h
     include/settings.h
     include/callHistory.h
     include/superCheckPartial.h
     include/debugLogger.h
     include/ttsManager.h
     include/piperManager.h
     src/utils/bandPlan.h
     src/utils/callsignUtils.h
     include/dxccDatabase.h
     include/qrzcqApi.h
     include/qrzApi.h
)

# UI files
set(UI_FILES
    ui/mainWindow.ui
)

# Qt resources
set(RESOURCES
    resources/resources.qrc
)

# Create executable
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${UI_FILES}
    ${RESOURCES}
)

# Link Qt libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Xml
)

# Installation
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

install(DIRECTORY contests/
    DESTINATION share/contestlogx/contests
    FILES_MATCHING PATTERN "*.json"
)

install(FILES data/default_layout.json
    DESTINATION share/contestlogx/data
)

install(FILES ContestLogX.desktop
    DESTINATION share/applications
)

install(FILES resources/contestlogx.png
    DESTINATION share/icons/hicolor/256x256/apps
)

# Enable testing
option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
