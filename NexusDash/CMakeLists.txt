cmake_minimum_required(VERSION 3.16)

project(NexusDash VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)

qt_standard_project_setup()
if(COMMAND qt_policy)
    qt_policy(SET QTP0001 NEW)
    qt_policy(SET QTP0004 NEW)
endif()

qt_add_executable(appNexusDash
    src/main.cpp
    src/core/AppEngine.cpp src/core/AppEngine.hpp
    src/services/ThemeService.cpp src/services/ThemeService.hpp
    src/services/SystemService.cpp src/services/SystemService.hpp
)

target_include_directories(appNexusDash PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../cpp
)

set_source_files_properties(qml/theme/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(appNexusDash
    URI NexusDash
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    WORKING_DIRECTORY qml
    QML_FILES
        main.qml
        theme/Theme.qml
        components/NexusButton.qml
        components/NetworkStatusIndicator.qml
        components/DashboardHeader.qml
        components/NexusGraph.qml
        components/NexusComboBox.qml
        components/NexusCard.qml
        components/NexusInput.qml
        components/NexusTable.qml
        components/NexusDialog.qml
        components/NexusNotification.qml
        components/Sidebar.qml
        components/SidebarItem.qml
        pages/DashboardPage.qml
        pages/SettingsPage.qml
        pages/AboutPage.qml
)

target_link_libraries(appNexusDash PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    ParsecLiteCore
)

if(MSVC)
    target_link_options(appNexusDash PRIVATE "/MANIFESTUAC:NO")
endif()

include(FeatureSummary)
feature_summary(WHAT ALL)

# Deployment for Qt
if(WIN32)
    get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")

    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET appNexusDash POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                    --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml"
                    --no-translations
                    --no-system-d3d-compiler
                    --no-opengl-sw
                    "$<TARGET_FILE:appNexusDash>"
            COMMENT "Running windeployqt for appNexusDash"
        )
    endif()
endif()
