cmake_minimum_required(VERSION 3.16)

project(NexusDash VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)

qt_standard_project_setup()
qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

qt_add_executable(appNexusDash
    src/main.cpp
    src/core/AppEngine.cpp src/core/AppEngine.hpp
    src/services/ThemeService.cpp src/services/ThemeService.hpp
    src/services/SystemService.cpp src/services/SystemService.hpp
)

target_include_directories(appNexusDash PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../cpp)

qt_add_qml_module(appNexusDash
    URI NexusDash
    VERSION 1.0
    QML_FILES
        qml/main.qml
        qml/theme/Theme.qml
        qml/components/NexusButton.qml
        qml/components/NexusCard.qml
        qml/components/NexusInput.qml
        qml/components/NexusTable.qml
        qml/components/NexusDialog.qml
        qml/components/Sidebar.qml
        qml/components/SidebarItem.qml
        qml/pages/DashboardPage.qml
        qml/pages/SettingsPage.qml
        qml/pages/AboutPage.qml
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
