@echo off
setlocal enabledelayedexpansion

echo ======================================================
echo    Parsec-lite: Automated Build ^& Setup
echo ======================================================
echo.

:: 1. Check for CMake
echo [1/3] Checking for Build Tools (CMake)...
cmake --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [!] CMake was not found.
    echo [!] Please install CMake from https://cmake.org/
    goto :FINISH
)
echo [+] CMake found.
echo.

:: 2. Check for C++ Dependencies
echo [2/3] Checking for C++ Dependencies (FFmpeg ^& ViGEm)...

:: Check FFmpeg
ffmpeg -version >nul 2>&1
if %errorlevel% neq 0 (
    echo [!] FFmpeg was not found in PATH.
    echo [!] Note: FFmpeg is required for hardware encoding/decoding.
) else (
    echo [+] FFmpeg found.
)

:: Note about ViGEm
echo [*] Note: Ensure ViGEmBus driver is installed for controller support.
echo.

:: 3. Build C++ Version
echo [3/3] Building C++ Version (Production)...

if not exist CMakeLists.txt (
    echo [!] CMakeLists.txt not found.
    goto :FINISH
)

if not exist build (
    mkdir build
)

cd build

echo [*] Configuring project with CMake...
cmake ..
if %errorlevel% neq 0 (
    echo [!] CMake configuration failed. Ensure you have a C++17 compiler (MSVC or MinGW) installed.
    cd ..
    goto :FINISH
)

echo [*] Compiling the project (Release mode)...
cmake --build . --config Release
if %errorlevel% neq 0 (
    echo [!] Build failed. Please check the error messages above.
    cd ..
    goto :FINISH
)

echo.
echo ======================================================
echo [+] SUCCESS: Parsec-lite built and ready!
echo.
if exist Release\parsec-lite.exe (
    echo Executable path: build\Release\parsec-lite.exe
) else if exist parsec-lite.exe (
    echo Executable path: build\parsec-lite.exe
)
echo.
echo To start as host: build\Release\parsec-lite.exe --host
echo ======================================================
cd ..
echo.

:FINISH
echo.
echo Setup process finished. Press Enter to exit.
pause >nul
