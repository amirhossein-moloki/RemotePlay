cmake_minimum_required(VERSION 3.10)
project(ParsecLite)

set(CMAKE_CXX_STANDARD 17)

if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

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

if(WIN32)
    set(IMGUI_SOURCES
        cpp/third_party/imgui/imgui_impl_win32.cpp
        cpp/third_party/imgui/imgui_impl_d3d11.cpp
    )
endif()

set(CLIENT_SOURCES
    cpp/client/receiver.cpp
    cpp/client/decoder_hw.cpp
    cpp/client/input_capture.cpp
    cpp/client/jitter_buffer.cpp
    cpp/client/renderer_d3d11.cpp
    cpp/client/ui_system.cpp
    cpp/client/overlay.cpp
    cpp/client/launcher.cpp
    cpp/third_party/imgui/imgui.cpp
    cpp/third_party/imgui/imgui_draw.cpp
    cpp/third_party/imgui/imgui_tables.cpp
    cpp/third_party/imgui/imgui_widgets.cpp
    ${IMGUI_SOURCES}
)

# Shared Library (API)
add_library(ParsecLiteCore SHARED
    cpp/common/parsec_lite_api.cpp
    ${COMMON_SOURCES}
    ${HOST_SOURCES}
    ${CLIENT_SOURCES}
)

# Executable
add_executable(parsec-lite
    cpp/main.cpp
    ${COMMON_SOURCES}
    ${HOST_SOURCES}
    ${CLIENT_SOURCES}
)

# Set up include directories
target_include_directories(ParsecLiteCore PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cpp)
target_include_directories(parsec-lite PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cpp)

# --- Dependency Discovery ---

if(WIN32)
    # Search for FFmpeg
    find_path(AVCODEC_INCLUDE_DIR NAMES libavcodec/avcodec.h
        PATHS
        "C:/ffmpeg/include"
        "C:/ffmpeg-shared/include"
        "C:/vcpkg/installed/x64-windows/include"
        ENV FFMPEG_DIR
        PATH_SUFFIXES include
    )
    find_library(AVCODEC_LIBRARY NAMES avcodec
        PATHS
        "C:/ffmpeg/lib"
        "C:/ffmpeg-shared/lib"
        "C:/vcpkg/installed/x64-windows/lib"
        ENV FFMPEG_DIR
        PATH_SUFFIXES lib bin
    )
    find_library(AVUTIL_LIBRARY NAMES avutil
        PATHS
        "C:/ffmpeg/lib"
        "C:/ffmpeg-shared/lib"
        "C:/vcpkg/installed/x64-windows/lib"
        ENV FFMPEG_DIR
        PATH_SUFFIXES lib bin
    )

    if(AVCODEC_INCLUDE_DIR AND AVCODEC_LIBRARY)
        target_include_directories(ParsecLiteCore PRIVATE ${AVCODEC_INCLUDE_DIR})
        target_include_directories(parsec-lite PRIVATE ${AVCODEC_INCLUDE_DIR})
        target_link_libraries(ParsecLiteCore PRIVATE ${AVCODEC_LIBRARY} ${AVUTIL_LIBRARY})
        target_link_libraries(parsec-lite PRIVATE ${AVCODEC_LIBRARY} ${AVUTIL_LIBRARY})
        target_compile_definitions(ParsecLiteCore PRIVATE PARSEC_LITE_ENABLE_FFMPEG)
        target_compile_definitions(parsec-lite PRIVATE PARSEC_LITE_ENABLE_FFMPEG)
    else()
        message(WARNING "FFmpeg not found. Ensure it's in your PATH or CMAKE_PREFIX_PATH. Hardware encoding/decoding will be disabled.")
    endif()

    # Search for ViGEmClient
    find_path(VIGEM_INCLUDE_DIR NAMES ViGEm/Client.h
        PATHS
        "C:/ViGEmClient/include"
        "C:/vcpkg/installed/x64-windows/include"
        ENV VIGEM_DIR
        PATH_SUFFIXES include
    )
    find_library(VIGEM_LIBRARY NAMES ViGEmClient
        PATHS
        "C:/ViGEmClient/lib"
        "C:/vcpkg/installed/x64-windows/lib"
        ENV VIGEM_DIR
        PATH_SUFFIXES lib
    )

    if(VIGEM_INCLUDE_DIR AND VIGEM_LIBRARY)
        target_include_directories(ParsecLiteCore PRIVATE ${VIGEM_INCLUDE_DIR})
        target_include_directories(parsec-lite PRIVATE ${VIGEM_INCLUDE_DIR})
        target_link_libraries(ParsecLiteCore PRIVATE ${VIGEM_LIBRARY})
        target_link_libraries(parsec-lite PRIVATE ${VIGEM_LIBRARY})
        target_compile_definitions(ParsecLiteCore PRIVATE PARSEC_LITE_ENABLE_VIGEM)
        target_compile_definitions(parsec-lite PRIVATE PARSEC_LITE_ENABLE_VIGEM)
    else()
        message(WARNING "ViGEmClient not found. Controller support will be disabled.")
    endif()

    # System libraries
    target_link_libraries(ParsecLiteCore PRIVATE ws2_32 iphlpapi d3d11 dxgi xinput)
    target_link_libraries(parsec-lite PRIVATE ws2_32 iphlpapi d3d11 dxgi xinput)
else()
    target_link_libraries(ParsecLiteCore PRIVATE pthread)
    target_link_libraries(parsec-lite PRIVATE pthread)
endif()

# Include modern UI (if Qt is found)
find_package(Qt6 COMPONENTS Core QUIET)
if(Qt6_FOUND)
    add_subdirectory(NexusDash)
else()
    message(WARNING "Qt6 not found. Modern UI (NexusDash) will not be built.")
endif()
