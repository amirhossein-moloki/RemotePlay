# Parsec-Lite Qt 6 Desktop Application

This is a modern desktop application template built with **Qt 6**, **QML**, and **C++17**. It features a clean architecture with a clear separation between the UI and backend logic.

## 🚀 Tech Stack
- **Qt 6.x** (MANDATORY)
- **Qt Quick / QML** (UI)
- **C++17** (Backend)
- **Qt Quick Controls 2** (Modern components)
- **CMake** (Build system)

## 📁 Project Structure
- `/src/main.cpp`: Application entry point.
- `/src/core/`: C++ backend logic (Bridge to QML).
- `/src/ui/`: QML interface files.
  - `/src/ui/components/`: Reusable UI components (Buttons, Cards, etc.).
  - `/src/ui/pages/`: Main application screens (Dashboard, Settings, About).
- `CMakeLists.txt`: Build configuration.

## 🛠 How to Build and Run

### Option 1: Qt Creator (Recommended)
1. Open **Qt Creator**.
2. Select **File > Open File or Project**.
3. Navigate to the `ParsecLiteQt` directory and select `CMakeLists.txt`.
4. Choose a **Qt 6 Kit** (e.g., Desktop Qt 6.x.x MSVC/GCC/Clang).
5. Click **Configure Project**.
6. Click the **Run** button (green play icon) or press `Ctrl+R`.

### Option 2: Command Line
Ensure you have Qt 6 and CMake in your PATH.

```bash
mkdir build
cd build
cmake ..
cmake --build .
./ParsecLiteQt
```

## ✨ Features
- **Modern Dashboard**: Clean layout with a collapsible-style sidebar.
- **Dark/Light Mode**: Dynamic theme switching integrated with C++ backend.
- **Clean Architecture**: Backend properties and methods exposed via a C++ `AppBridge` class.
- **Responsive Design**: Uses `RowLayout`, `ColumnLayout`, and `StackLayout` for flexibility.
- **Fluent Design Elements**: Rounded corners, subtle borders, and modern typography.

## 🧠 Backend ↔ Frontend Integration
The `AppBridge` class handles the core logic and is exposed to QML as a context property named `appBridge`.
- **Properties**: `userName`, `darkMode`, `recentSessions`.
- **Methods**: `connectToHost(ip)`, `startHosting()`, `toggleTheme()`.
