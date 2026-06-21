#include "crash_handler.hpp"
#include "logger.hpp"
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <psapi.h>

namespace CrashDiagnostics {

void CrashHandler::Initialize() {
    SetUnhandledExceptionFilter(UnhandledExceptionFilter);
    AddVectoredExceptionHandler(1, VectoredExceptionHandler);
    LOG_INFO("CrashHandler", "Crash diagnostics initialized.");
}

LONG WINAPI CrashHandler::UnhandledExceptionFilter(struct _EXCEPTION_POINTERS* exceptionInfo) {
    LogException(exceptionInfo, false);
    // Returning EXCEPTION_EXECUTE_HANDLER will usually terminate the process after our logging
    return EXCEPTION_EXECUTE_HANDLER;
}

LONG WINAPI CrashHandler::VectoredExceptionHandler(struct _EXCEPTION_POINTERS* exceptionInfo) {
    DWORD code = exceptionInfo->ExceptionRecord->ExceptionCode;

    // We only care about fatal exceptions in the vectored handler to avoid noise
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
    PCONTEXT context = exceptionInfo->ContextRecord;

    std::stringstream ss;
    ss << "\n" << std::string(60, '=') << "\n";
    ss << "FATAL EXCEPTION DETECTED (" << (isVectored ? "Vectored" : "Unhandled") << ")\n";
    ss << "Exception Code: 0x" << std::hex << std::uppercase << record->ExceptionCode
       << " (" << ExceptionCodeToString(record->ExceptionCode) << ")\n";
    ss << "Fault Address:  0x" << std::setw(16) << std::setfill('0') << (uintptr_t)record->ExceptionAddress << "\n";

    std::string moduleName = GetModuleNameFromAddress(record->ExceptionAddress);
    ss << "Fault Module:   " << moduleName << "\n";

    if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2) {
        ss << "Access Type:    " << (record->ExceptionInformation[0] == 0 ? "READ" : (record->ExceptionInformation[0] == 1 ? "WRITE" : "EXECUTE")) << "\n";
        ss << "Access Address: 0x" << std::setw(16) << std::setfill('0') << record->ExceptionInformation[1] << "\n";
    }

    ss << "\nRegisters:\n";
#ifdef _M_X64
    ss << "RAX: 0x" << std::setw(16) << context->Rax << " RBX: 0x" << std::setw(16) << context->Rbx << "\n";
    ss << "RCX: 0x" << std::setw(16) << context->Rcx << " RDX: 0x" << std::setw(16) << context->Rdx << "\n";
    ss << "RSI: 0x" << std::setw(16) << context->Rsi << " RDI: 0x" << std::setw(16) << context->Rdi << "\n";
    ss << "RBP: 0x" << std::setw(16) << context->Rbp << " RSP: 0x" << std::setw(16) << context->Rsp << "\n";
    ss << "R8:  0x" << std::setw(16) << context->R8  << " R9:  0x" << std::setw(16) << context->R9  << "\n";
#else
    ss << "EAX: 0x" << std::setw(8) << context->Eax << " EBX: 0x" << std::setw(8) << context->Ebx << "\n";
    ss << "ECX: 0x" << std::setw(8) << context->Ecx << " EDX: 0x" << std::setw(8) << context->Edx << "\n";
    ss << "ESI: 0x" << std::setw(8) << context->Esi << " EDI: 0x" << std::setw(8) << context->Edi << "\n";
    ss << "EBP: 0x" << std::setw(8) << context->Ebp << " ESP: 0x" << std::setw(8) << context->Esp << "\n";
#endif
    ss << std::string(60, '=') << "\n";

    LOG_ERROR("CrashHandler", ss.str());
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
        case EXCEPTION_FLT_DENORMAL_OPERAND:     return "Floating-point Denormal Operand";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return "Floating-point Divide by Zero";
        case EXCEPTION_FLT_INEXACT_RESULT:       return "Floating-point Inexact Result";
        case EXCEPTION_FLT_INVALID_OPERATION:    return "Floating-point Invalid Operation";
        case EXCEPTION_FLT_OVERFLOW:             return "Floating-point Overflow";
        case EXCEPTION_FLT_STACK_CHECK:          return "Floating-point Stack Check";
        case EXCEPTION_FLT_UNDERFLOW:            return "Floating-point Underflow";
        case EXCEPTION_ILLEGAL_INSTRUCTION:      return "Illegal Instruction";
        case EXCEPTION_IN_PAGE_ERROR:            return "In-page Error";
        case EXCEPTION_INT_DIVIDE_BY_ZERO:       return "Integer Divide by Zero";
        case EXCEPTION_INT_OVERFLOW:             return "Integer Overflow";
        case EXCEPTION_INVALID_DISPOSITION:      return "Invalid Disposition";
        case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "Non-continuable Exception";
        case EXCEPTION_PRIV_INSTRUCTION:         return "Privileged Instruction";
        case EXCEPTION_SINGLE_STEP:              return "Single Step";
        case EXCEPTION_STACK_OVERFLOW:           return "Stack Overflow";
        default:                                 return "Unknown Exception";
    }
}

} // namespace CrashDiagnostics

#endif
