# NexusDash - Modern Qt 6 Dashboard

NexusDash is a high-performance, modular desktop dashboard template built with Qt 6.7+, C++17, and QML. It features a modern UI/UX inspired by contemporary SaaS platforms, with full support for dark/light modes, collapsible navigation, and reusable UI components.

## 🚀 Features
- **Modern UI/UX**: Clean, professional design with smooth transitions.
- **Dark/Light Mode**: Real-time theme switching using a C++ service.
- **Modular Architecture**: Clear separation between C++ logic and QML interface.
- **Custom Components**: Includes reusable Buttons, Cards, Tables, Inputs, and Dialogs.
- **System Monitoring**: Live (simulated) CPU and Memory tracking via C++ backend.

## 🛠 Tech Stack
- **Framework**: Qt 6.x
- **UI**: Qt Quick (QML) & Qt Quick Controls 2
- **Language**: C++17
- **Build System**: CMake

## 📁 Project Structure
- `src/core/`: Application engine and initialization.
- `src/services/`: C++ logic layer (Theme management, System stats).
- `qml/theme/`: Global styling and design tokens.
- `qml/components/`: Reusable UI elements.
- `qml/pages/`: Individual application screens.

## 🏗 Build Instructions

### Prerequisites
- **Qt 6.x** installed (with Qt Quick, QML, and Quick Controls 2 modules).
- **CMake** 3.16 or newer.
- A C++17 compatible compiler (MSVC, GCC, or Clang).

### Option 1: Qt Creator (Recommended)
1. Open **Qt Creator**.
2. Go to **File > Open File or Project...**.
3. Select the `CMakeLists.txt` in the `NexusDash` folder.
4. Choose your Qt 6 Kit.
5. Click **Run** (or press `Ctrl+R`).

### Option 2: Command Line
```bash
mkdir build
cd build
cmake ..
cmake --build .
./appNexusDash  # (or appNexusDash.exe on Windows)
```

## 🧠 Architecture Notes
The application uses a **C++ to QML Integration** pattern:
- **`AppEngine`**: The central C++ entry point exposed as `backend` in QML.
- **`ThemeService`**: Manages the application's visual state.
- **`SystemService`**: Provides periodic data updates to the UI.
- **`Theme.qml`**: A singleton that maps C++ properties to UI colors and sizes.
