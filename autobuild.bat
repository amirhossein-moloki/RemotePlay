@echo off
setlocal enabledelayedexpansion

:: Set UTF-8 for Persian characters in console if supported
chcp 65001 >nul

echo ======================================================
echo    Parsec-lite: Automated Build ^& Setup (C++ Primary)
echo    سیستم خودکار بیلد و راه‌اندازی پارسک-لایت (نسخه اصلی C++)
echo ======================================================
echo.

:: 1. Check for CMake
echo [1/4] Checking for Build Tools (CMake)...
echo [1/4] در حال بررسی ابزارهای بیلد (CMake)...
cmake --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [!] CMake was not found. C++ version cannot be built automatically.
    echo [!] CMake پیدا نشد. نسخه C++ به صورت خودکار قابل بیلد نیست.
    echo [!] Please install CMake from https://cmake.org/
    echo [!] لطفا CMake را از سایت رسمی آن نصب کنید.
    goto :PYTHON_FALLBACK
)
echo [+] CMake found.
echo [+] CMake پیدا شد.
echo.

:: 2. Check for C++ Dependencies
echo [2/4] Checking for C++ Dependencies (FFmpeg ^& ViGEm)...
echo [2/4] در حال بررسی پیش‌نیازهای C++ (FFmpeg و ViGEm)...

:: Check FFmpeg
ffmpeg -version >nul 2>&1
if %errorlevel% neq 0 (
    echo [!] FFmpeg was not found in PATH.
    echo [!] FFmpeg در مسیر سیستم (PATH) پیدا نشد.
    echo [!] Note: FFmpeg is required for hardware encoding/decoding.
    echo [!] توجه: FFmpeg برای انکود و دیکود سخت‌افزاری مورد نیاز است.
) else (
    echo [+] FFmpeg found.
    echo [+] FFmpeg پیدا شد.
)

:: Note about ViGEm
echo [*] Note: Ensure ViGEmBus driver is installed for controller support.
echo [*] نکته: برای پشتیبانی از کنترلر، مطمئن شوید درایور ViGEmBus نصب است.
echo.

:: 3. Build C++ Version
echo [3/4] Building C++ Version (Production)...
echo [3/4] در حال بیلد نسخه C++ (نسخه اصلی)...

if not exist CMakeLists.txt (
    echo [!] CMakeLists.txt not found.
    echo [!] فایل CMakeLists.txt پیدا نشد.
    goto :PYTHON_FALLBACK
)

if not exist build (
    mkdir build
)

cd build

echo [*] Configuring project with CMake...
echo [*] در حال پیکربندی پروژه با CMake...
cmake ..
if %errorlevel% neq 0 (
    echo [!] CMake configuration failed. Ensure you have a C++17 compiler (MSVC or MinGW) installed.
    echo [!] پیکربندی CMake با خطا مواجه شد. مطمئن شوید کامپایلر C++17 (مانند MSVC یا MinGW) نصب است.
    cd ..
    goto :PYTHON_FALLBACK
)

echo [*] Compiling the project (Release mode)...
echo [*] در حال کامپایل پروژه (حالت Release)...
cmake --build . --config Release
if %errorlevel% neq 0 (
    echo [!] Build failed. Please check the error messages above.
    echo [!] بیلد با خطا مواجه شد. لطفا پیام‌های خطای بالا را بررسی کنید.
    cd ..
    goto :PYTHON_FALLBACK
)

echo.
echo ======================================================
echo [+] SUCCESS: C++ Version built and ready!
echo [+] موفقیت: نسخه C++ با موفقیت بیلد شد و آماده استفاده است!
echo.
if exist Release\parsec-lite.exe (
    echo Executable path: build\Release\parsec-lite.exe
    echo مسیر فایل اجرایی: build\Release\parsec-lite.exe
) else if exist parsec-lite.exe (
    echo Executable path: build\parsec-lite.exe
    echo مسیر فایل اجرایی: build\parsec-lite.exe
)
echo.
echo To start as host: build\Release\parsec-lite.exe --host
echo برای شروع به عنوان سرور: build\Release\parsec-lite.exe --host
echo ======================================================
cd ..
echo.
goto :FINISH

:PYTHON_FALLBACK
:: 4. Optional Python Prototype
echo [4/4] Setting up Python Prototype (Legacy/Optional)...
echo [4/4] در حال راه‌اندازی پروتوتایپ پایتون (نسخه قدیمی/اختیاری)...
python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [!] Python was not found, skipping python setup.
    echo [!] پایتون پیدا نشد، از راه‌اندازی پایتون صرف نظر شد.
) else (
    echo [+] Python found.
    echo [+] پایتون پیدا شد.
    if exist requirements.txt (
        echo [*] Installing dependencies from requirements.txt...
        echo [*] در حال نصب پیش‌نیازها از فایل requirements.txt...
        python -m pip install -r requirements.txt
    )
)

:FINISH
echo.
echo Setup process finished.
echo فرآیند راه‌اندازی به پایان رسید.
pause
