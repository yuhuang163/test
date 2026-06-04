#include "qlog_win.h"

#include "qlog.h"

#ifdef Q_OS_WIN

#include <DbgHelp.h>
#include <Windows.h>

#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

#include "common_utils.h"

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

namespace {

std::wstring crashTimestampWstr() {
    const time_t now = time(nullptr);
    const tm* ltm = localtime(&now);
    std::wstringstream ss;
    ss << std::setw(4) << std::setfill(L'0') << (1900 + ltm->tm_year) << L"-" << std::setw(2) << std::setfill(L'0')
       << (1 + ltm->tm_mon) << L"-" << std::setw(2) << std::setfill(L'0') << ltm->tm_mday << L"_" << std::setw(2)
       << std::setfill(L'0') << ltm->tm_hour << L"-" << std::setw(2) << std::setfill(L'0') << ltm->tm_min << L"-"
       << std::setw(2) << std::setfill(L'0') << ltm->tm_sec;
    return ss.str();
}

}  // namespace

LONG WINAPI QlogApplicationCrashHandler(EXCEPTION_POINTERS* pException) {
    const std::wstring timeStr = crashTimestampWstr();
    const std::wstring folderPath = L"所有log";

    CreateDirectoryW(folderPath.c_str(), nullptr);

    const std::wstring dumpFilePath = folderPath + L"\\" + timeStr + L"_闪退记录.dmp";
    HANDLE hDumpFile =
        CreateFileW(dumpFilePath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hDumpFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION dumpInfo{};
        dumpInfo.ExceptionPointers = pException;
        dumpInfo.ThreadId = GetCurrentThreadId();
        dumpInfo.ClientPointers = TRUE;
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hDumpFile, MiniDumpNormal, &dumpInfo, nullptr,
                          nullptr);
        CloseHandle(hDumpFile);
    }

    HANDLE hProcess = GetCurrentProcess();
    HANDLE hThread = GetCurrentThread();
    CONTEXT context{};
    RtlCaptureContext(&context);

    STACKFRAME64 stackFrame{};
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrStack.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Mode = AddrModeFlat;

#ifdef _M_IX86
    stackFrame.AddrPC.Offset = context.Eip;
    stackFrame.AddrStack.Offset = context.Esp;
    stackFrame.AddrFrame.Offset = context.Ebp;
#elif defined(_M_X64)
    stackFrame.AddrPC.Offset = context.Rip;
    stackFrame.AddrStack.Offset = context.Rsp;
    stackFrame.AddrFrame.Offset = context.Rbp;
#endif

    SymInitialize(hProcess, nullptr, TRUE);

    std::wstring stackTrace = L"堆栈跟踪:\n";
    while (StackWalk64(IMAGE_FILE_MACHINE_AMD64, hProcess, hThread, &stackFrame, &context, nullptr,
                       SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) {
        DWORD64 dwDisplacement = 0;
        char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_PATH];
        SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_PATH;
        if (SymFromAddr(hProcess, stackFrame.AddrPC.Offset, &dwDisplacement, symbol)) {
            stackTrace += std::wstring(symbol->Name, symbol->Name + strlen(symbol->Name)) + L"\n";
        }
    }

    const std::wstring logFilePath = folderPath + L"\\" + timeStr + L"_闪退堆栈.log";
    std::ofstream logFile(logFilePath, std::ios::out | std::ios::trunc);
    if (logFile.is_open()) {
        logFile << "崩溃发生时的堆栈跟踪:\n";
        logFile << std::string(stackTrace.begin(), stackTrace.end());
        logFile.close();
    }

    MessageBoxW(nullptr, L"上位机奔溃\r\n奔溃记录已保存", L"Error", MB_OK | MB_ICONERROR);
    return EXCEPTION_EXECUTE_HANDLER;
}

void Qlog::installWindowsCrashHandler() {
    SetUnhandledExceptionFilter(reinterpret_cast<LPTOP_LEVEL_EXCEPTION_FILTER>(QlogApplicationCrashHandler));
}

#if _MSC_VER >= 1600
#    pragma execution_character_set(pop)
#endif

#endif  // Q_OS_WIN
