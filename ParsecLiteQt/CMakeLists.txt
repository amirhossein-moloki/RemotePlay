cmake_minimum_required(VERSION 3.16)

project(ParsecLiteQt VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)

qt_add_executable(ParsecLiteQt
    src/main.cpp
    src/core/AppBridge.cpp
    src/core/AppBridge.hpp
)

qt_add_qml_module(ParsecLiteQt
    URI ParsecLite.UI
    VERSION 1.0
    QML_FILES
        src/ui/main.qml
        src/ui/Style.qml
        src/ui/components/NavButton.qml
        src/ui/components/Card.qml
        src/ui/components/CustomButton.qml
        src/ui/pages/Dashboard.qml
        src/ui/pages/Settings.qml
        src/ui/pages/About.qml
)

target_link_libraries(ParsecLiteQt PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)

install(TARGETS ParsecLiteQt
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
