@echo off
REM Parsec-Lite Packaging Script
REM This script gathers all necessary files for distribution into the 'dist' folder.

set DIST_DIR=dist
set BUILD_DIR=build
set DEPS_DIR=deps

echo [1/4] Creating distribution folder: %DIST_DIR%
if not exist "%DIST_DIR%" mkdir "%DIST_DIR%"

echo [2/4] Copying executables...
set FOUND_EXE=0

if exist "%BUILD_DIR%\Release\parsec-lite.exe" (
    copy /Y "%BUILD_DIR%\Release\parsec-lite.exe" "%DIST_DIR%\"
    set FOUND_EXE=1
)

if exist "%BUILD_DIR%\NexusDash\Release\appNexusDash.exe" (
    copy /Y "%BUILD_DIR%\NexusDash\Release\appNexusDash.exe" "%DIST_DIR%\"
    set FOUND_EXE=1
)

if %FOUND_EXE%==0 (
    echo WARNING: No executables found in build directory.
    echo Ensure you have built the project in Release mode.
)

echo [3/4] Copying FFmpeg DLLs...
if exist "%DEPS_DIR%\ffmpeg\bin" (
    copy /Y "%DEPS_DIR%\ffmpeg\bin\*.dll" "%DIST_DIR%\"
) else (
    echo WARNING: FFmpeg DLLs not found in %DEPS_DIR%\ffmpeg\bin
)

echo [4/4] Copying ViGEmClient DLL...
if exist "%DEPS_DIR%\ViGEmClient\lib\x64\ViGEmClient.dll" (
    copy /Y "%DEPS_DIR%\ViGEmClient\lib\x64\ViGEmClient.dll" "%DIST_DIR%\"
) else (
    echo WARNING: ViGEmClient.dll not found in %DEPS_DIR%\ViGEmClient\lib\x64
)

echo.
echo Packaging complete!
echo Output directory: %cd%\%DIST_DIR%
echo.
echo IMPORTANT: If you are using the modern UI (NexusDash), you must also
echo ensure Qt6 DLLs are present in the dist folder.
echo You can run: windeployqt.exe dist\appNexusDash.exe
echo.
pause
