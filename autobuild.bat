@echo off
setlocal enabledelayedexpansion

:: Set UTF-8 for Persian characters in console if supported
chcp 65001 >nul

echo ======================================================
echo    Parsec-lite: Automated Build ^& Setup
echo    سیستم خودکار بیلد و راه‌اندازی پارسک-لایت
echo ======================================================
echo.

:: 1. Check for Python and Install Requirements
echo [1/3] Checking for Python...
echo [1/3] در حال بررسی پایتون...
python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [!] Python was not found. Please install Python to use the prototype and tools.
    echo [!] پایتون پیدا نشد. برای استفاده از پروتوتایپ و ابزارها، لطفا پایتون را نصب کنید.
) else (
    echo [+] Python found.
    echo [+] پایتون پیدا شد.
    if exist requirements.txt (
        echo [*] Installing dependencies from requirements.txt...
        echo [*] در حال نصب پیش‌نیازها از فایل requirements.txt...
        python -m pip install -r requirements.txt
    ) else (
        echo [!] requirements.txt not found, skipping python dependency installation.
        echo [!] فایل requirements.txt پیدا نشد، از نصب پیش‌نیازهای پایتون صرف نظر شد.
    )
)

echo.

:: 2. Check for CMake
echo [2/3] Checking for Build Tools (CMake)...
echo [2/3] در حال بررسی ابزارهای بیلد (CMake)...
cmake --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [!] CMake was not found. C++ version cannot be built automatically.
    echo [!] CMake پیدا نشد. نسخه C++ به صورت خودکار قابل بیلد نیست.
    echo [!] You can still use the Python version.
    echo [!] همچنان می‌توانید از نسخه پایتون استفاده کنید.
    goto :FINISH
)

:: 3. Build C++ Version
echo [3/3] Building C++ Version (High Performance)...
echo [3/3] در حال بیلد نسخه C++ (نسخه با کارایی بالا)...

if not exist CMakeLists.txt (
    echo [!] CMakeLists.txt not found. Cannot build C++ version.
    echo [!] فایل CMakeLists.txt پیدا نشد. نسخه C++ قابل بیلد نیست.
    goto :FINISH
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
    goto :FINISH
)

echo [*] Compiling the project (Release mode)...
echo [*] در حال کامپایل پروژه (حالت Release)...
cmake --build . --config Release
if %errorlevel% neq 0 (
    echo [!] Build failed. Please check the error messages above.
    echo [!] بیلد با خطا مواجه شد. لطفا پیام‌های خطای بالا را بررسی کنید.
    cd ..
    goto :FINISH
)

echo.
echo ======================================================
echo [+] SUCCESS: Project built and ready!
echo [+] موفقیت: پروژه با موفقیت بیلد شد و آماده استفاده است!
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

:FINISH
echo.
echo Setup process finished.
echo فرآیند راه‌اندازی به پایان رسید.
pause
