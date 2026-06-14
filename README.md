# Parsec-lite: High-Performance LAN Game Streaming

Parsec-lite is a low-latency, high-performance LAN game streaming system. It is a production-grade C++ implementation capable of sub-20ms end-to-end latency.

## ⚡ Quick Start (English)

```bash
mkdir build && cd build
cmake .. -DBUILD_NEXUSDASH=ON
cmake --build . --config Release
# Host
./parsec-lite.exe --host
# Client
./parsec-lite.exe --client <HOST_IP>
```

---

## 🇮🇷 راهنمای کامل نصب و بیلد (Persian Build Guide)

این بخش شامل مراحل کامل برای نصب پیش‌نیازها، آماده‌سازی وابستگی‌ها و بیلد کردن پروژه به صورت دستی است.

### ۱. پیش‌نیازهای سیستم
برای بیلد کردن پروژه، نرم‌افزارهای زیر باید روی سیستم شما نصب باشند:
- **Visual Studio 2019 یا 2022**: به همراه کامپوننت "Desktop development with C++".
- **CMake (نسخه 3.10 به بالا)**: برای مدیریت پروسه بیلد.
- **Qt 6.7 یا بالاتر**: (اختیاری - برای رابط کاربری مدرن NexusDash). مطمئن شوید که مسیر `bin` در Qt به `PATH` سیستم اضافه شده باشد.
- **ViGEmBus Driver**: برای پشتیبانی از کنترلر (گیم‌پد).

### ۲. آماده‌سازی وابستگی‌ها (Dependencies)
پروژه برای اجرا به FFmpeg و ViGEmClient نیاز دارد. مراحل زیر را برای آماده‌سازی پوشه `deps` دنبال کنید:

1. در ریشه پروژه، یک پوشه به نام `deps` بسازید.
2. **FFmpeg**:
   - نسخه مشترک (Shared) FFmpeg را برای ویندوز دانلود کنید (مثلاً از [BtbN/FFmpeg-Builds](https://github.com/BtbN/FFmpeg-Builds/releases)).
   - محتویات را در `deps/ffmpeg` استخراج کنید به طوری که پوشه‌های `bin` و `include` مستقیماً داخل `deps/ffmpeg` باشند.
3. **ViGEmClient**:
   - فایل SDK را از [ViGEm/ViGEmClient](https://github.com/ViGEm/ViGEmClient/releases) دانلود کنید.
   - محتویات را در `deps/ViGEmClient` استخراج کنید.

### ۳. پیکربندی و بیلد
یک ترمینال (مانند PowerShell یا CMD) در پوشه اصلی پروژه باز کرده و دستورات زیر را اجرا کنید:

```powershell
# ایجاد پوشه بیلد
mkdir build
cd build

# پیکربندی پروژه با CMake
# اگر Qt نصب ندارید، مقدار BUILD_NEXUSDASH را OFF بگذارید
cmake .. -DBUILD_NEXUSDASH=ON

# کامپایل پروژه در حالت Release
cmake --build . --config Release
```

### ۴. استقرار فایل‌های اجرایی (Deployment)
بعد از اتمام بیلد، فایل‌های اجرایی در پوشه `build/Release` (یا مشابه آن) قرار می‌گیرند. برای اجرای صحیح، باید فایل‌های DLL را در کنار فایل `.exe` کپی کنید:

1. تمام فایل‌های `.dll` موجود در `deps/ffmpeg/bin/` را به پوشه خروجی (`Release`) کپی کنید.
2. فایل `ViGEmClient.dll` را از مسیر `deps/ViGEmClient/lib/x64/` به پوشه خروجی کپی کنید.

حالا می‌توانید `parsec-lite.exe` را اجرا کنید!

---

## 🚀 Key Features
- **Ultra-Low Latency**: Target < 20ms E2E latency on standard Gigabit LAN.
- **High-Performance Capture**: GPU-direct frame access via Windows DXGI Desktop Duplication API.
- **Hardware Acceleration**: Full support for NVENC (NVIDIA), AMF (AMD), and QuickSync (Intel) encoding, with D3D11VA/DXVA2 hardware decoding.
- **Hardened Networking**: Custom UDP protocol with:
  - **FEC (Forward Error Correction)**: XOR-based parity for packet loss recovery.
  - **Adaptive Jitter Buffer**: Dynamically adjusts to network conditions to eliminate stutter.
  - **Lock-Free Queues**: SPSC (Single-Producer Single-Consumer) queues for zero-contention data flow.
- **Input Forwarding**:
  - **Keyboard/Mouse**: Low-latency Windows Raw Input capture and injection.
  - **Controllers**: Multi-client XInput support with ViGEmBus virtual controller injection.

## 🛠️ Requirements
- **OS**: Windows 10/11 (for DXGI and ViGEm)
- **Hardware**: NVIDIA/AMD/Intel GPU with hardware encoding support.
- **Dependencies**: FFmpeg (libavcodec), ViGEmBus driver.

## ⚖️ Performance Targets
| Component | Target Latency |
| :--- | :--- |
| Capture (DXGI) | 1-2ms |
| Encode (NVENC) | 3-5ms |
| Network (LAN) | 1-2ms |
| Decode & Render | 4-7ms |
| **Total E2E** | **~15-20ms** |

## 🛡️ License
MIT
