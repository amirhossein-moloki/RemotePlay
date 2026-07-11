#include "crash_handler.hpp"
#include "logger.hpp"
#include "config.hpp"
#include "session_manager.hpp"
#include "network_manager.hpp"

#include <iomanip>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#else
#include <signal.h>
#include <execinfo.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#endif

#ifdef PARSEC_LITE_ENABLE_FFMPEG
extern "C" {
#include <libavcodec/avcodec.h>
}
#endif

namespace CrashDiagnostics {

void CrashHandler::Initialize() {
#ifdef _WIN32
    SetUnhandledExceptionFilter(UnhandledExceptionFilter);
    AddVectoredExceptionHandler(1, VectoredExceptionHandler);
#else
    struct sigaction sa;
    sa.sa_sigaction = [](int sig, siginfo_t* info, void* ctx) {
        POSIXSignalHandler(sig, info, ctx);
    };
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;

    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGFPE, &sa, nullptr);
    sigaction(SIGILL, &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
    sigaction(SIGBUS, &sa, nullptr);
#endif
    LOG_INFO("CrashHandler", "Crash diagnostics initialized.");
}

#ifdef _WIN32
LONG WINAPI CrashHandler::UnhandledExceptionFilter(struct _EXCEPTION_POINTERS* exceptionInfo) {
    LogException(exceptionInfo, false);
    return EXCEPTION_EXECUTE_HANDLER;
}

LONG WINAPI CrashHandler::VectoredExceptionHandler(struct _EXCEPTION_POINTERS* exceptionInfo) {
    DWORD code = exceptionInfo->ExceptionRecord->ExceptionCode;

    if (code == EXCEPTION_ACCESS_VIOLATION ||
        code == EXCEPTION_ARRAY_BOUNDS_EXCEEDED ||
        code == EXCEPTION_STACK_OVERFLOW ||
        code == EXCEPTION_ILLEGAL_INSTRUCTION) {
        LogException(exceptionInfo, true);
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

void CrashHandler::LogException(struct _EXCEPTION_POINTERS* exceptionInfo, bool isVectored) {
    PEXCEPTION_RECORD record = exceptionInfo->ExceptionRecord;
    std::stringstream ss;
    ss << "Fatal Windows Exception 0x" << std::hex << record->ExceptionCode
       << " (" << ExceptionCodeToString(record->ExceptionCode) << ")";

    GenerateCrashReport(ss.str(), exceptionInfo);
}

std::string CrashHandler::GetModuleNameFromAddress(PVOID address) {
    HMODULE hModule = NULL;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          (LPCSTR)address, &hModule)) {
        char path[MAX_PATH];
        if (GetModuleFileNameA(hModule, path, sizeof(path))) {
            std::string fullPath = path;
            size_t lastSlash = fullPath.find_last_of("\\/");
            if (lastSlash != std::string::npos) return fullPath.substr(lastSlash + 1);
            return fullPath;
        }
    }
    return "Unknown Module";
}

const char* CrashHandler::ExceptionCodeToString(DWORD code) {
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:         return "Access Violation (0xC0000005)";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return "Array Bounds Exceeded";
        case EXCEPTION_BREAKPOINT:               return "Breakpoint";
        case EXCEPTION_DATATYPE_MISALIGNMENT:    return "Datatype Misalignment";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return "Floating-point Divide by Zero";
        case EXCEPTION_ILLEGAL_INSTRUCTION:      return "Illegal Instruction";
        case EXCEPTION_STACK_OVERFLOW:           return "Stack Overflow";
        default:                                 return "Unknown Exception";
    }
}
#else
void CrashHandler::POSIXSignalHandler(int sig, void* info, void* context) {
    std::stringstream ss;
    ss << "Fatal POSIX Signal " << sig << " (" << SignalCodeToString(sig) << ")";
    GenerateCrashReport(ss.str(), nullptr, true);

    signal(sig, SIG_DFL);
    raise(sig);
}

const char* CrashHandler::SignalCodeToString(int sig) {
    switch (sig) {
        case SIGSEGV: return "Segmentation Fault (SIGSEGV)";
        case SIGFPE:  return "Floating-point Exception (SIGFPE)";
        case SIGILL:  return "Illegal Instruction (SIGILL)";
        case SIGABRT: return "Aborted (SIGABRT)";
        case SIGBUS:  return "Bus Error (SIGBUS)";
        default:      return "Unknown POSIX Signal";
    }
}
#endif

void CrashHandler::GenerateCrashReport(const std::string& reason, void* exceptionPointers, bool isSignalContext) {
    // Generate timestamp
    auto now = std::chrono::system_clock::now();
    auto now_time = std::chrono::system_clock::to_time_t(now);
    struct tm timeinfo;
#ifdef _WIN32
    localtime_s(&timeinfo, &now_time);
#else
    localtime_r(&now_time, &timeinfo);
#endif

    std::stringstream fileTime;
    fileTime << std::put_time(&timeinfo, "%Y-%m-%d_%H-%M-%S");

    std::string reportDir = "CrashReports";
    std::filesystem::create_directories(reportDir);

    std::string reportFile = reportDir + "/crash_" + fileTime.str() + ".txt";
    std::string archiveFile = reportDir + "/crash_" + fileTime.str();

    std::ofstream file(reportFile);
    if (!file.is_open()) return;

    file << "================================================================================\n";
    file << "                           FATAL CRASH DIAGNOSTICS REPORT\n";
    file << "================================================================================\n";
    file << "Reason:               " << reason << "\n";
    file << "Timestamp:            " << fileTime.str() << "\n";
    file << "Thread ID:            " << Logger::getThreadSessionId() << "\n";
    file << "Process ID:           " << Logger::getInstance().levelToString(LogLevel::LL_FATAL) << "\n";

    file << "\n--------------------------------------------------------------------------------\n";
    file << "1. EXCEPTION DETAILS & CPU REGISTERS\n";
    file << "--------------------------------------------------------------------------------\n";

#ifdef _WIN32
    if (exceptionPointers) {
        struct _EXCEPTION_POINTERS* ep = (struct _EXCEPTION_POINTERS*)exceptionPointers;
        PEXCEPTION_RECORD record = ep->ExceptionRecord;
        PCONTEXT context = ep->ContextRecord;

        file << "Exception Code:    0x" << std::hex << record->ExceptionCode << "\n";
        file << "Fault Address:     0x" << std::hex << (uintptr_t)record->ExceptionAddress << "\n";
        file << "Fault Module:      " << GetModuleNameFromAddress(record->ExceptionAddress) << "\n";

        if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2) {
            file << "Access Type:       " << (record->ExceptionInformation[0] == 0 ? "READ" : (record->ExceptionInformation[0] == 1 ? "WRITE" : "EXECUTE")) << "\n";
            file << "Access Address:    0x" << std::hex << record->ExceptionInformation[1] << "\n";
        }

        file << "\nRegisters:\n";
#ifdef _M_X64
        file << "RAX: 0x" << std::setw(16) << std::setfill('0') << context->Rax << " RBX: 0x" << std::setw(16) << context->Rbx << "\n";
        file << "RCX: 0x" << std::setw(16) << context->Rcx << " RDX: 0x" << std::setw(16) << context->Rdx << "\n";
        file << "RSI: 0x" << std::setw(16) << context->Rsi << " RDI: 0x" << std::setw(16) << context->Rdi << "\n";
        file << "RBP: 0x" << std::setw(16) << context->Rbp << " RSP: 0x" << std::setw(16) << context->Rsp << "\n";
#else
        file << "EAX: 0x" << std::setw(8) << std::setfill('0') << context->Eax << " EBX: 0x" << std::setw(8) << context->Ebx << "\n";
        file << "ECX: 0x" << std::setw(8) << context->Ecx << " EDX: 0x" << std::setw(8) << context->Edx << "\n";
#endif
    } else {
        file << "Registers not available (no exception pointers provided).\n";
    }
#else
    file << "Registers are platform-dependent and not captured on POSIX fallback.\n";
#endif

    file << "\n--------------------------------------------------------------------------------\n";
    file << "2. STACK TRACE / BACKTRACE\n";
    file << "--------------------------------------------------------------------------------\n";

#ifdef _WIN32
    void* stack[32];
    unsigned short frames = CaptureStackBackTrace(0, 32, stack, NULL);
    for (unsigned short i = 0; i < frames; ++i) {
        file << "  [" << i << "] 0x" << std::hex << (uintptr_t)stack[i] << " in " << GetModuleNameFromAddress(stack[i]) << "\n";
    }
#else
    void* stack[32];
    int frames = backtrace(stack, 32);
    char** symbols = backtrace_symbols(stack, frames);
    if (symbols) {
        for (int i = 0; i < frames; ++i) {
            file << "  [" << i << "] " << symbols[i] << "\n";
        }
        free(symbols);
    } else {
        file << "  No symbols captured in backtrace.\n";
    }
#endif

    file << "\n--------------------------------------------------------------------------------\n";
    file << "3. LOADED MODULES\n";
    file << "--------------------------------------------------------------------------------\n";

#ifdef _WIN32
    HMODULE hMods[1024];
    DWORD cbNeeded;
    if (EnumProcessModules(GetCurrentProcess(), hMods, sizeof(hMods), &cbNeeded)) {
        for (unsigned int i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
            char szModName[MAX_PATH];
            if (GetModuleFileNameExA(GetCurrentProcess(), hMods[i], szModName, sizeof(szModName))) {
                file << "  0x" << std::hex << (uintptr_t)hMods[i] << " - " << szModName << "\n";
            }
        }
    }
#else
    std::ifstream maps("/proc/self/maps");
    if (maps.is_open()) {
        std::string line;
        while (std::getline(maps, line)) {
            file << "  " << line << "\n";
        }
    } else {
        file << "  Unable to read memory maps on Linux.\n";
    }
#endif

    file << "\n--------------------------------------------------------------------------------\n";
    file << "4. ACTIVE THREADS LIST\n";
    file << "--------------------------------------------------------------------------------\n";

#ifdef _WIN32
    HANDLE hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hThreadSnap != INVALID_HANDLE_VALUE) {
        THREADENTRY32 te32;
        te32.dwSize = sizeof(THREADENTRY32);
        if (Thread32First(hThreadSnap, &te32)) {
            do {
                if (te32.th32OwnerProcessID == GetCurrentProcessId()) {
                    file << "  Thread ID: " << std::dec << te32.th32ThreadID << ", Priority: " << te32.tpBasePri << "\n";
                }
            } while (Thread32Next(hThreadSnap, &te32));
        }
        CloseHandle(hThreadSnap);
    }
#else
    try {
        for (const auto& entry : std::filesystem::directory_iterator("/proc/self/task")) {
            file << "  Thread (LWP): " << entry.path().filename().string() << "\n";
        }
    } catch (...) {
        file << "  Unable to query active thread list.\n";
    }
#endif

    file << "\n--------------------------------------------------------------------------------\n";
    file << "5. ACTIVE SESSIONS\n";
    file << "--------------------------------------------------------------------------------\n";
    file << "  Active Session running: " << (SessionManager::getInstance().isRunning() ? "Yes" : "No") << "\n";
    file << "  Session ID:             " << Logger::getThreadSessionId() << "\n";

    file << "\n--------------------------------------------------------------------------------\n";
    file << "6. GPU DETAILS\n";
    file << "--------------------------------------------------------------------------------\n";
#ifdef _WIN32
    file << "  GPU: DirectX 11 DXGI Adapter\n";
#else
    file << "  GPU details available in hardware diagnostics.\n";
#endif

    file << "\n--------------------------------------------------------------------------------\n";
    file << "7. FFMPEG STATE\n";
    file << "--------------------------------------------------------------------------------\n";
#ifdef PARSEC_LITE_ENABLE_FFMPEG
    file << "  FFmpeg Version: " << av_version_info() << "\n";
#else
    file << "  FFmpeg is disabled / mocked in this build.\n";
#endif

    file << "\n--------------------------------------------------------------------------------\n";
    file << "8. NETWORK STATE\n";
    file << "--------------------------------------------------------------------------------\n";
    try {
        auto interfaces = Network::NetworkManager::EnumerateInterfaces();
        for (const auto& iface : interfaces) {
            file << "  Interface: " << iface.name << " (" << iface.ip << ")\n";
        }
    } catch (...) {
        file << "  Failed to query network interfaces.\n";
    }

    file << "\n--------------------------------------------------------------------------------\n";
    file << "9. ENCODER / DECODER STATE\n";
    file << "--------------------------------------------------------------------------------\n";
    file << "  Use Hardware Encoding: " << (Config::getInstance().getBool("useHardwareEncoding", true) ? "Yes" : "No") << "\n";
    file << "  Encoder Preset:        " << Config::getInstance().getInt("encoderPreset", 0) << "\n";
    file << "  Resolution Scale:      " << Config::getInstance().getDouble("resolutionScale", 1.0) << "\n";

    file << "\n--------------------------------------------------------------------------------\n";
    file << "10. SYSTEM MEMORY USAGE\n";
    file << "--------------------------------------------------------------------------------\n";
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        file << "  Working Set Size:  " << std::dec << (pmc.WorkingSetSize / 1024 / 1024) << " MB\n";
        file << "  Pagefile Usage:    " << (pmc.PagefileUsage / 1024 / 1024) << " MB\n";
    }
#else
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        file << "  Total Memory:      " << std::dec << (si.totalram * si.mem_unit / 1024 / 1024) << " MB\n";
        file << "  Free Memory:       " << (si.freeram * si.mem_unit / 1024 / 1024) << " MB\n";
        file << "  Shared Memory:     " << (si.sharedram * si.mem_unit / 1024 / 1024) << " MB\n";
    }
#endif

    file << "================================================================================\n";
    file.close();

    // Log a warning about the crash
    LOG_WARN("CrashHandler", "Crash report generated at: " + reportFile);

    // Call standard system utility to compress the report ONLY if not in POSIX signal context
    if (!isSignalContext) {
#ifdef _WIN32
        std::string zipCmd = "powershell -NoProfile -Command \"Compress-Archive -Path '" + reportFile + "' -DestinationPath '" + archiveFile + ".zip' -Force\"";
        (void)system(zipCmd.c_str());
#else
        std::string tarCmd = "tar -czf " + archiveFile + ".tar.gz " + reportFile;
        (void)system(tarCmd.c_str());
#endif
    } else {
        LOG_WARN("CrashHandler", "Skipping file compression inside POSIX signal handler to guarantee async-signal safety.");
    }
}

} // namespace CrashDiagnostics
