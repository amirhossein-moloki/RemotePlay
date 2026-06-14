@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

echo ======================================================
echo    Parsec-lite: Unified Modern Build and Setup
echo ======================================================
echo.

REM 1. Check for basic tools
echo [*] Checking for CMake...
cmake --version >nul 2>&1
if errorlevel 1 (
    echo [^!] ERROR: CMake not found. Please install CMake from https://cmake.org/
    goto :FAIL
)

echo [*] Checking for Qt 6...
where qmake >nul 2>&1
if errorlevel 1 (
    echo [^!] WARNING: Qt 6 (qmake) not found in PATH.
    echo [^!] The modern UI (NexusDash) requires Qt 6.7+.
    echo [^!] If you continue, the build might fail if Qt is not found by CMake.
    pause
)

REM 2. Automated Dependency Management
if not exist "deps" mkdir "deps"

REM --- FFmpeg ---
if not exist "deps\ffmpeg" (
    echo [*] FFmpeg not found. Downloading...
    powershell -Command "& { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; $url = 'https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl-shared.zip'; Invoke-WebRequest -Uri $url -OutFile 'deps\ffmpeg.zip' }"
    if errorlevel 1 (
        echo [^!] ERROR: Failed to download FFmpeg.
        goto :FAIL
    )
    echo [*] Extracting FFmpeg...
    powershell -Command "Expand-Archive -Path 'deps\ffmpeg.zip' -DestinationPath 'deps\ffmpeg_tmp' -Force"
    REM Move contents to deps\ffmpeg
    for /d %%i in ("deps\ffmpeg_tmp\ffmpeg-*") do (
        move "%%i" "deps\ffmpeg"
    )
    rmdir /s /q "deps\ffmpeg_tmp"
    del "deps\ffmpeg.zip"
    echo [+] FFmpeg setup complete.
) else (
    echo [+] FFmpeg already present in deps/.
)

REM --- ViGEmClient ---
if not exist "deps\ViGEmClient" (
    echo [*] ViGEmClient not found. Downloading...
    powershell -Command "& { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; $url = 'https://github.com/ViGEm/ViGEmClient/releases/download/v1.17.333/ViGEmClient_v1.17.333.zip'; Invoke-WebRequest -Uri $url -OutFile 'deps\ViGEmClient.zip' }"
    if errorlevel 1 (
        echo [^!] ERROR: Failed to download ViGEmClient.
        goto :FAIL
    )
    echo [*] Extracting ViGEmClient...
    powershell -Command "Expand-Archive -Path 'deps\ViGEmClient.zip' -DestinationPath 'deps\ViGEmClient' -Force"
    del "deps\ViGEmClient.zip"
    echo [+] ViGEmClient setup complete.
) else (
    echo [+] ViGEmClient already present in deps/.
)

REM 3. Build Process
echo.
echo [*] Starting Unified Build (Core + Modern UI)...
if not exist "build" mkdir "build"
cd build

echo [*] Configuring with CMake...
cmake .. -DBUILD_NEXUSDASH=ON
if errorlevel 1 (
    echo [^!] ERROR: CMake configuration failed.
    goto :FAIL_CD
)

echo [*] Compiling (Release mode)...
cmake --build . --config Release
if errorlevel 1 (
    echo [^!] ERROR: Build failed. Check the error messages above.
    goto :FAIL_CD
)

REM 4. Deployment (Copy DLLs to output directory)
echo.
echo [*] Deploying runtime dependencies...
set OUT_DIR=Release
if not exist "!OUT_DIR!" set OUT_DIR=.

REM FFmpeg DLLs
if exist "..\deps\ffmpeg\bin\*.dll" (
    echo [*] Copying FFmpeg DLLs...
    copy /y "..\deps\ffmpeg\bin\*.dll" "!OUT_DIR!\" >nul
)

REM ViGEmClient DLL
if exist "..\deps\ViGEmClient\lib\x64\ViGEmClient.dll" (
    echo [*] Copying ViGEmClient DLL...
    copy /y "..\deps\ViGEmClient\lib\x64\ViGEmClient.dll" "!OUT_DIR!\" >nul
)

cd ..
echo.
echo ======================================================
echo [+] SUCCESS: Parsec-Lite and NexusDash built!
echo Binary: build\!OUT_DIR!\parsec-lite.exe
echo ======================================================
pause
exit /b 0

:FAIL_CD
cd ..
:FAIL
echo.
echo [^!] Build process stopped due to errors.
pause
exit /b 1
