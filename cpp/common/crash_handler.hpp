#pragma once

#include <string>

namespace CrashDiagnostics {

class CrashHandler {
public:
    static void Initialize();
    static void GenerateCrashReport(const std::string& reason, void* exceptionPointers = nullptr, bool isSignalContext = false);

private:
#ifdef _WIN32
    static long __stdcall UnhandledExceptionFilter(struct _EXCEPTION_POINTERS* exceptionInfo);
    static long __stdcall VectoredExceptionHandler(struct _EXCEPTION_POINTERS* exceptionInfo);
    static void LogException(struct _EXCEPTION_POINTERS* exceptionInfo, bool isVectored);
    static std::string GetModuleNameFromAddress(void* address);
    static const char* ExceptionCodeToString(unsigned long code);
#else
    static void POSIXSignalHandler(int sig, void* info, void* context);
    static const char* SignalCodeToString(int sig);
#endif
};

} // namespace CrashDiagnostics
