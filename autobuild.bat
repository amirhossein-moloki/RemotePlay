@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

:MENU
cls
echo ======================================================
echo    Parsec-lite: Automated Build ^& Setup
echo ======================================================
echo.
echo [1] Build C++ Production Version
echo [2] Build Modern Qt 6 Version
echo [3] Exit
echo.
set /p choice="Select an option [1-3]: "

if "%choice%"=="1" goto :BUILD_CPP
if "%choice%"=="2" goto :BUILD_QT
if "%choice%"=="3" goto :FINISH
goto :MENU

:BUILD_CPP
cls
echo [1/3] Checking for Build Tools (CMake)...
cmake --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [^!] CMake was not found. Please install CMake from https://cmake.org/
    goto :PAUSE_FINISH
)
echo [+] CMake found.
echo.

echo [2/3] Checking for C++ Dependencies (FFmpeg ^& ViGEm)...
ffmpeg -version >nul 2>&1
if %errorlevel% neq 0 (
    echo [^!] FFmpeg was not found in PATH. It is required for hardware encoding.
) else (
    echo [+] FFmpeg found.
)
echo [*] Note: Ensure ViGEmBus driver is installed for controller support.
echo.

echo [3/3] Building C++ Version (Production)...
if not exist CMakeLists.txt (
    echo [^!] ERROR: CMakeLists.txt not found in the current directory.
    goto :PAUSE_FINISH
)

if not exist build (
    mkdir build
)

cd build

echo [*] Configuring project with CMake...
cmake ..
if %errorlevel% neq 0 (
    echo [^!] ERROR: CMake configuration failed.
    cd ..
    goto :PAUSE_FINISH
)

echo [*] Compiling the project (Release mode)...
cmake --build . --config Release
if %errorlevel% neq 0 (
    echo [^!] ERROR: Build failed. Please check the compiler error messages above.
    cd ..
    goto :PAUSE_FINISH
)

echo.
echo ======================================================
echo [+] SUCCESS: Parsec-lite built and ready!
echo.

set FINAL_EXE_PATH=
if exist Release\parsec-lite.exe (
    set FINAL_EXE_PATH=build\Release\parsec-lite.exe
) else if exist parsec-lite.exe (
    set FINAL_EXE_PATH=build\parsec-lite.exe
)

cd ..

if defined FINAL_EXE_PATH (
    echo [BUILD OUTPUT]
    echo Binary Location: %FINAL_EXE_PATH%
    echo.
    echo Usage:
    echo   Host mode:   %FINAL_EXE_PATH% --host
    echo   Client mode: %FINAL_EXE_PATH% --client ^<HOST_IP^>
) else (
    echo [^!] ERROR: Executable file not found in build directory.
    echo Check build/Release/ or build/ for parsec-lite.exe
)
echo ======================================================
goto :PAUSE_FINISH

:PAUSE_FINISH
echo.
echo Press any key to return to menu or exit...
pause >nul
goto :MENU

:BUILD_QT
cls
echo [1/2] Checking for Qt 6 Environment...
echo [*] Ensure Qt 6.7+ is installed and in your PATH.
cmake --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [^!] CMake was not found.
    goto :PAUSE_FINISH
)

echo [2/2] Building Modern UI (NexusDash)...
if not exist build (
    mkdir build
)
cd build
cmake ..
if %errorlevel% neq 0 (
    echo [^!] ERROR: CMake configuration failed.
    cd ..
    goto :PAUSE_FINISH
)

echo [*] Compiling NexusDash...
cmake --build . --target appNexusDash --config Release
if %errorlevel% neq 0 (
    echo [^!] ERROR: Build failed.
    cd ..
    goto :PAUSE_FINISH
)

echo.
echo ======================================================
echo [+] SUCCESS: Modern UI built!
echo Binary Location: build\NexusDash\Release\appNexusDash.exe
echo ======================================================
cd ..
goto :PAUSE_FINISH

:FINISH
exit
