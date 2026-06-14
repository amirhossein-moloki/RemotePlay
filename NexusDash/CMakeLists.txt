cmake_minimum_required(VERSION 3.16)

project(NexusDash VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)

qt_standard_project_setup()

qt_add_executable(appNexusDash
    src/main.cpp
    src/core/AppEngine.cpp src/core/AppEngine.hpp
    src/services/ThemeService.cpp src/services/ThemeService.hpp
    src/services/SystemService.cpp src/services/SystemService.hpp
)

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
    RESOURCES
)

# Qt for Android/iOS/WebAssembly might require different settings,
# but for Desktop (Windows/macOS/Linux) this is standard.

target_link_libraries(appNexusDash PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)

include(FeatureSummary)
show_feature_summary()
