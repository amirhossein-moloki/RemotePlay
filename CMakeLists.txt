cmake_minimum_required(VERSION 3.10)
project(ParsecLite)

set(CMAKE_CXX_STANDARD 17)

# Set up include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Sources
set(COMMON_SOURCES
    cpp/common/network_manager.cpp
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
)

# Executable
add_executable(parsec-lite
    cpp/main.cpp
    ${COMMON_SOURCES}
    ${HOST_SOURCES}
    ${CLIENT_SOURCES}
)

# Dependencies (Windows specific)
if(WIN32)
    target_link_libraries(parsec-lite ws2_32 iphlpapi d3d11 dxgi xinput)
endif()
