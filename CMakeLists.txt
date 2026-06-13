cmake_minimum_required(VERSION 3.10)
project(ParsecLite)

set(CMAKE_CXX_STANDARD 17)

# Set up include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/cpp)

# Sources
set(COMMON_SOURCES
    cpp/common/network_manager.cpp
    cpp/common/logger.cpp
    cpp/common/config.cpp
)

set(HOST_SOURCES
    cpp/host/capture_dxgi.cpp
    cpp/host/encoder_hw.cpp
    cpp/host/input_injector.cpp
)

set(CLIENT_SOURCES
    cpp/client/receiver.cpp
    cpp/client/decoder_hw.cpp
    cpp/client/input_capture.cpp
    cpp/client/jitter_buffer.cpp
    cpp/client/renderer_d3d11.cpp
    cpp/client/overlay.cpp
    cpp/client/launcher.cpp
    cpp/third_party/imgui/imgui.cpp
    cpp/third_party/imgui/imgui_draw.cpp
    cpp/third_party/imgui/imgui_tables.cpp
    cpp/third_party/imgui/imgui_widgets.cpp
    cpp/third_party/imgui/imgui_impl_win32.cpp
    cpp/third_party/imgui/imgui_impl_d3d11.cpp
)

# Executable
add_executable(parsec-lite
    cpp/main.cpp
    ${COMMON_SOURCES}
    ${HOST_SOURCES}
    ${CLIENT_SOURCES}
)

# Dependencies
if(WIN32)
    target_link_libraries(parsec-lite ws2_32 iphlpapi d3d11 dxgi xinput ViGEmClient)
else()
    target_link_libraries(parsec-lite pthread)
endif()
