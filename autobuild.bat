@echo off
REM Parsec-Lite Auto-build Script for Windows
REM This script automates the CMake configuration and build process.

set BUILD_DIR=build

echo [1/3] Preparing build directory...
if not exist "%BUILD_DIR%" (
    mkdir "%BUILD_DIR%"
)

cd "%BUILD_DIR%"

echo [2/3] Configuring project with CMake...
REM If you have Qt installed in a custom path, you might need to add:
REM -DCMAKE_PREFIX_PATH=C:\Qt\6.x.x\msvc2022_64
cmake .. -DBUILD_NEXUSDASH=ON

if errorlevel 1 (
    echo.
    echo ERROR: CMake configuration failed.
    echo If Qt was not found, try running CMake with -DCMAKE_PREFIX_PATH
    echo Example: cmake .. -DBUILD_NEXUSDASH=ON -DCMAKE_PREFIX_PATH=C:\Qt\6.7.0\msvc2022_64
    pause
    exit /b 1
)

echo [3/3] Building project in Release mode...
cmake --build . --config Release

if errorlevel 1 (
    echo.
    echo ERROR: Build failed.
    pause
    exit /b 1
)

echo.
echo Build successful!
echo You can now run package.bat to gather all files for distribution.
echo.
pause
