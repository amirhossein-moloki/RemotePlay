cmake_minimum_required(VERSION 3.16)
project(ParsecLite)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/dist/app)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/dist/app)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/dist/app)

if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-DNOMINMAX)
endif()

# Sources
set(COMMON_SOURCES
    cpp/common/network_manager.cpp
    cpp/common/logger.cpp
    cpp/common/config.cpp
    cpp/common/session_manager.cpp
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
target_compile_definitions(ParsecLiteCore PRIVATE PARSEC_LITE_EXPORT)

# Executable
add_executable(parsec-lite
    cpp/main.cpp
)
target_link_libraries(parsec-lite PRIVATE ParsecLiteCore)

# Set up include directories
target_include_directories(ParsecLiteCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/cpp)
target_include_directories(parsec-lite PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cpp)

# --- Dependency Discovery ---

if(WIN32)
    # Search for FFmpeg
    set(FFMPEG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/ffmpeg")
    find_path(AVCODEC_INCLUDE_DIR NAMES libavcodec/avcodec.h PATHS "${FFMPEG_DIR}/include")
    find_library(AVCODEC_LIBRARY NAMES avcodec PATHS "${FFMPEG_DIR}/lib" "${FFMPEG_DIR}/bin")
    find_library(AVUTIL_LIBRARY NAMES avutil PATHS "${FFMPEG_DIR}/lib" "${FFMPEG_DIR}/bin")
    find_library(SWSCALE_LIBRARY NAMES swscale PATHS "${FFMPEG_DIR}/lib" "${FFMPEG_DIR}/bin")

    if(AVCODEC_INCLUDE_DIR AND AVCODEC_LIBRARY)
        target_include_directories(ParsecLiteCore PRIVATE ${AVCODEC_INCLUDE_DIR})
        target_include_directories(parsec-lite PRIVATE ${AVCODEC_INCLUDE_DIR})
        target_link_libraries(ParsecLiteCore PRIVATE ${AVCODEC_LIBRARY} ${AVUTIL_LIBRARY} ${SWSCALE_LIBRARY})
        target_link_libraries(parsec-lite PRIVATE ${AVCODEC_LIBRARY} ${AVUTIL_LIBRARY} ${SWSCALE_LIBRARY})
        target_compile_definitions(ParsecLiteCore PRIVATE PARSEC_LITE_ENABLE_FFMPEG)
        target_compile_definitions(parsec-lite PRIVATE PARSEC_LITE_ENABLE_FFMPEG)

        # Copy FFmpeg DLLs to output
        file(GLOB FFMPEG_DLLS "${FFMPEG_DIR}/bin/*.dll")
        if(FFMPEG_DLLS)
            add_custom_command(TARGET ParsecLiteCore POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${FFMPEG_DLLS} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
                COMMENT "Copying FFmpeg DLLs to output"
            )
        endif()
    endif()

    # Search for ViGEmClient
    set(VIGEM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/ViGEmClient")
    find_path(VIGEM_INCLUDE_DIR NAMES ViGEm/Client.h PATHS "${VIGEM_DIR}/include")
    find_library(VIGEM_LIBRARY NAMES ViGEmClient PATHS "${VIGEM_DIR}/lib/x64")

    if(VIGEM_INCLUDE_DIR AND VIGEM_LIBRARY)
        target_include_directories(ParsecLiteCore PRIVATE ${VIGEM_INCLUDE_DIR})
        target_include_directories(parsec-lite PRIVATE ${VIGEM_INCLUDE_DIR})
        target_link_libraries(ParsecLiteCore PRIVATE ${VIGEM_LIBRARY})
        target_link_libraries(parsec-lite PRIVATE ${VIGEM_LIBRARY})
        target_compile_definitions(ParsecLiteCore PRIVATE PARSEC_LITE_ENABLE_VIGEM)
        target_compile_definitions(parsec-lite PRIVATE PARSEC_LITE_ENABLE_VIGEM)

        # Copy ViGEm DLL to output
        if(EXISTS "${VIGEM_DIR}/lib/x64/ViGEmClient.dll")
            add_custom_command(TARGET ParsecLiteCore POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${VIGEM_DIR}/lib/x64/ViGEmClient.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
                COMMENT "Copying ViGEmClient DLL to output"
            )
        endif()
    endif()

    target_link_libraries(ParsecLiteCore PRIVATE ws2_32 iphlpapi d3d11 dxgi xinput shcore)
    target_link_libraries(parsec-lite PRIVATE ws2_32 iphlpapi d3d11 dxgi xinput shcore)
else()
    target_link_libraries(ParsecLiteCore PRIVATE pthread)
    target_link_libraries(parsec-lite PRIVATE pthread)
endif()

# Include modern UI (NexusDash)
option(BUILD_NEXUSDASH "Build the modern Qt 6 UI" ON)

if(BUILD_NEXUSDASH)
    find_package(Qt6 COMPONENTS Core Gui Qml Quick QuickControls2 REQUIRED)
    add_subdirectory(NexusDash)

endif()
