#pragma once

#ifdef _WIN32
#include <windows.h>
#include <string>

namespace CrashDiagnostics {

class CrashHandler {
public:
    static void Initialize();

private:
    static LONG WINAPI UnhandledExceptionFilter(struct _EXCEPTION_POINTERS* exceptionInfo);
    static LONG WINAPI VectoredExceptionHandler(struct _EXCEPTION_POINTERS* exceptionInfo);

    static void LogException(struct _EXCEPTION_POINTERS* exceptionInfo, bool isVectored);
    static std::string GetModuleNameFromAddress(PVOID address);
    static const char* ExceptionCodeToString(DWORD code);
};

} // namespace CrashDiagnostics
#endif
