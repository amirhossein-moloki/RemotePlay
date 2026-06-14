@echo off
setlocal enabledelayedexpansion
chcp 65001 >nul

:MENU
cls
echo ======================================================
echo    Parsec-lite: Automated Build ^& Setup
echo    Parsec-lite: سیستم بیلد و نصب خودکار
echo ======================================================
echo.
echo [1] Build C++ Production Version (نسخه پروداکشن)
echo [2] Exit (خروج)
echo.
set /p choice="Select an option [1-2]: "

if "%choice%"=="1" goto :BUILD_CPP
if "%choice%"=="2" goto :FINISH
goto :MENU

:BUILD_CPP
cls
echo [1/3] Checking for Build Tools (CMake)...
echo [1/3] در حال بررسی ابزارهای بیلد (CMake)...
cmake --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [!] CMake was not found. Please install CMake from https://cmake.org/
    echo [!] CMake پیدا نشد. لطفا آن را نصب کنید.
    goto :PAUSE_FINISH
)
echo [+] CMake found. (CMake پیدا شد)
echo.

echo [2/3] Checking for C++ Dependencies (FFmpeg ^& ViGEm)...
echo [2/3] در حال بررسی وابستگی‌های سی‌پلاس‌پلاس...

ffmpeg -version >nul 2>&1
if %errorlevel% neq 0 (
    echo [!] FFmpeg was not found in PATH.
    echo [!] FFmpeg در مسیر سیستم پیدا نشد. برای انکود سخت‌افزاری الزامی است.
) else (
    echo [+] FFmpeg found. (FFmpeg پیدا شد)
)
echo [*] Note: Ensure ViGEmBus driver is installed for controller support.
echo [*] نکته: از نصب بودن درایور ViGEmBus برای پشتیبانی از دسته بازی اطمینان حاصل کنید.
echo.

echo [3/3] Building C++ Version (Production)...
echo [3/3] در حال بیلد نسخه سی‌پلاس‌پلاس...

if not exist CMakeLists.txt (
    echo [!] CMakeLists.txt not found. (فایل تنظیمات بیلد پیدا نشد)
    goto :PAUSE_FINISH
)

if not exist build (
    mkdir build
)

cd build

echo [*] Configuring project with CMake...
echo [*] در حال پیکربندی پروژه...
cmake ..
if %errorlevel% neq 0 (
    echo [!] CMake configuration failed. (پیکربندی شکست خورد)
    cd ..
    goto :PAUSE_FINISH
)

echo [*] Compiling the project (Release mode)...
echo [*] در حال کامپایل پروژه (حالت Release)...
cmake --build . --config Release
if %errorlevel% neq 0 (
    echo [!] Build failed. Please check the error messages above.
    echo [!] بیلد با خطا مواجه شد. پیام‌های بالا را بررسی کنید.
    cd ..
    goto :PAUSE_FINISH
)

echo.
echo ======================================================
echo [+] SUCCESS: Parsec-lite built and ready!
echo [+] موفقیت: بیلد به پایان رسید!
echo.

set EXE_PATH=
if exist Release\parsec-lite.exe (
    set EXE_PATH=build\Release\parsec-lite.exe
) else if exist parsec-lite.exe (
    set EXE_PATH=build\parsec-lite.exe
)

cd ..

if defined EXE_PATH (
    echo Executable path: %EXE_PATH%
    echo مسیر فایل اجرایی: %EXE_PATH%
    echo.
    echo To start as host: %EXE_PATH% --host
    echo To start as client: %EXE_PATH% --client ^<IP^>
) else (
    echo [!] ERROR: Executable file not found in build directory.
    echo [!] خطا: فایل اجرایی در پوشه بیلد پیدا نشد.
)
echo ======================================================
goto :PAUSE_FINISH

:PAUSE_FINISH
echo.
echo Press any key to return to menu or exit...
echo برای بازگشت به منو یا خروج کلیدی را فشار دهید...
pause >nul
goto :MENU

:FINISH
exit
