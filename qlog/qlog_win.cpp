#include "qlog_win.h"

#include "qlog.h"

#ifdef Q_OS_WIN

#include <DbgHelp.h>
#include <Windows.h>

#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

namespace {

std::string g_crashExtraUtf8;

std::wstring crashTimestampWstr() {
    const time_t now = time(nullptr);
    const tm* ltm = localtime(&now);
    wchar_t buf[32] = {};
    swprintf_s(buf, L"%04d-%02d-%02d_%02d-%02d-%02d", 1900 + ltm->tm_year, 1 + ltm->tm_mon, ltm->tm_mday,
               ltm->tm_hour, ltm->tm_min, ltm->tm_sec);
    return buf;
}

std::string wideToUtf8(const std::wstring& ws) {
    if (ws.empty()) {
        return {};
    }
    const int bytes =
        WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()), nullptr, 0, nullptr, nullptr);
    if (bytes <= 0) {
        return {};
    }
    std::string out(static_cast<size_t>(bytes), '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()), out.data(), bytes, nullptr, nullptr);
    return out;
}

std::string formatHex64(DWORD64 v) {
    char buf[32] = {};
    sprintf_s(buf, "0x%016llX", static_cast<unsigned long long>(v));
    return buf;
}

/** UTF-8 闪退文本日志（带 BOM） */
class Utf8CrashLogFile {
public:
    bool open(const std::wstring& path) {
        handle_ = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle_ == INVALID_HANDLE_VALUE) {
            return false;
        }
        static const unsigned char kBom[] = {0xEF, 0xBB, 0xBF};
        DWORD written = 0;
        WriteFile(handle_, kBom, 3, &written, nullptr);
        return true;
    }

    void writeLine(const std::string& utf8Line) {
        if (handle_ == INVALID_HANDLE_VALUE || utf8Line.empty()) {
            return;
        }
        DWORD written = 0;
        WriteFile(handle_, utf8Line.data(), static_cast<DWORD>(utf8Line.size()), &written, nullptr);
        WriteFile(handle_, "\r\n", 2, &written, nullptr);
    }

    void close() {
        if (handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
        }
    }

private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
};

const char* exceptionCodeName(DWORD code) {
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:
            return "EXCEPTION_ACCESS_VIOLATION (0xC0000005)";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
            return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
        case EXCEPTION_BREAKPOINT:
            return "EXCEPTION_BREAKPOINT";
        case EXCEPTION_DATATYPE_MISALIGNMENT:
            return "EXCEPTION_DATATYPE_MISALIGNMENT";
        case EXCEPTION_FLT_DENORMAL_OPERAND:
            return "EXCEPTION_FLT_DENORMAL_OPERAND";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:
            return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
        case EXCEPTION_FLT_INEXACT_RESULT:
            return "EXCEPTION_FLT_INEXACT_RESULT";
        case EXCEPTION_FLT_INVALID_OPERATION:
            return "EXCEPTION_FLT_INVALID_OPERATION";
        case EXCEPTION_FLT_OVERFLOW:
            return "EXCEPTION_FLT_OVERFLOW";
        case EXCEPTION_FLT_STACK_CHECK:
            return "EXCEPTION_FLT_STACK_CHECK";
        case EXCEPTION_FLT_UNDERFLOW:
            return "EXCEPTION_FLT_UNDERFLOW";
        case EXCEPTION_ILLEGAL_INSTRUCTION:
            return "EXCEPTION_ILLEGAL_INSTRUCTION";
        case EXCEPTION_IN_PAGE_ERROR:
            return "EXCEPTION_IN_PAGE_ERROR";
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
            return "EXCEPTION_INT_DIVIDE_BY_ZERO";
        case EXCEPTION_INT_OVERFLOW:
            return "EXCEPTION_INT_OVERFLOW";
        case EXCEPTION_INVALID_DISPOSITION:
            return "EXCEPTION_INVALID_DISPOSITION";
        case EXCEPTION_NONCONTINUABLE_EXCEPTION:
            return "EXCEPTION_NONCONTINUABLE_EXCEPTION";
        case EXCEPTION_PRIV_INSTRUCTION:
            return "EXCEPTION_PRIV_INSTRUCTION";
        case EXCEPTION_SINGLE_STEP:
            return "EXCEPTION_SINGLE_STEP";
        case EXCEPTION_STACK_OVERFLOW:
            return "EXCEPTION_STACK_OVERFLOW";
        default:
            return "UNKNOWN_EXCEPTION";
    }
}

void writeExceptionSummary(Utf8CrashLogFile& log, EXCEPTION_POINTERS* ep) {
    if (!ep || !ep->ExceptionRecord) {
        log.writeLine("【异常】ExceptionPointers 无效");
        return;
    }

    EXCEPTION_RECORD* rec = ep->ExceptionRecord;
    char line[512] = {};
    sprintf_s(line, "【异常码】%s  raw=0x%08lX", exceptionCodeName(rec->ExceptionCode),
              static_cast<unsigned long>(rec->ExceptionCode));
    log.writeLine(line);
    sprintf_s(line, "【异常地址】%s", formatHex64(reinterpret_cast<DWORD64>(rec->ExceptionAddress)).c_str());
    log.writeLine(line);

    if (rec->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && rec->NumberParameters >= 2) {
        const ULONG_PTR op = rec->ExceptionInformation[0];
        const ULONG_PTR addr = rec->ExceptionInformation[1];
        const char* opText = (op == 0) ? "读" : (op == 1) ? "写" : (op == 8) ? "DEP执行" : "未知操作";
        sprintf_s(line, "【访问违例】%s 目标地址=%s", opText, formatHex64(addr).c_str());
        log.writeLine(line);
    }

    if (rec->ExceptionRecord) {
        log.writeLine("【嵌套异常】存在 chained ExceptionRecord");
    }
}

void initSymbolEngine(HANDLE hProcess) {
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS |
                  SYMOPT_INCLUDE_32BIT_MODULES);

    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring searchPath = exePath;
    const size_t slash = searchPath.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        searchPath.resize(slash);
    }
    searchPath += L";.;";

    SymInitialize(hProcess, nullptr, TRUE);
    SymSetSearchPathW(hProcess, searchPath.c_str());
}

bool captureStackFromException(EXCEPTION_POINTERS* ep, HANDLE hProcess, Utf8CrashLogFile& log) {
    if (!ep || !ep->ContextRecord) {
        log.writeLine("【堆栈】无 ContextRecord，无法解析崩溃线程堆栈");
        return false;
    }

    CONTEXT context = *ep->ContextRecord;
    STACKFRAME64 frame{};
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrStack.Mode = AddrModeFlat;
    frame.AddrFrame.Mode = AddrModeFlat;

    DWORD machineType = IMAGE_FILE_MACHINE_AMD64;
#ifdef _M_X64
    frame.AddrPC.Offset = context.Rip;
    frame.AddrStack.Offset = context.Rsp;
    frame.AddrFrame.Offset = context.Rbp;
#elif defined(_M_IX86)
    machineType = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset = context.Eip;
    frame.AddrStack.Offset = context.Esp;
    frame.AddrFrame.Offset = context.Ebp;
#else
    log.writeLine("【堆栈】不支持的 CPU 架构");
    return false;
#endif

    HANDLE hThread = GetCurrentThread();
    log.writeLine("【堆栈】自异常现场 ContextRecord 展开（含模块/偏移/源行，需 PDB）：");

    for (int depth = 0; depth < 64; ++depth) {
        if (!StackWalk64(machineType, hProcess, hThread, &frame, &context, nullptr, SymFunctionTableAccess64,
                         SymGetModuleBase64, nullptr)) {
            break;
        }

        const DWORD64 pc = frame.AddrPC.Offset;
        if (pc == 0) {
            break;
        }

        char symbolBuffer[sizeof(SYMBOL_INFO) + 1024] = {};
        SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = 1023;

        DWORD64 displacement = 0;
        std::string funcName = "<未知函数>";
        if (SymFromAddr(hProcess, pc, &displacement, symbol)) {
            funcName = symbol->Name;
        }

        std::string moduleName = "<未知模块>";
        IMAGEHLP_MODULE64 moduleInfo{};
        moduleInfo.SizeOfStruct = sizeof(moduleInfo);
        if (SymGetModuleInfo64(hProcess, pc, &moduleInfo)) {
            moduleName = moduleInfo.ModuleName;
        }

        std::string lineInfo;
        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof(line);
        DWORD lineDisplacement = 0;
        if (SymGetLineFromAddr64(hProcess, pc, &lineDisplacement, &line)) {
            char buf[512] = {};
            sprintf_s(buf, "  源文件: %s:%lu +0x%lX", line.FileName,
                      static_cast<unsigned long>(line.LineNumber), static_cast<unsigned long>(lineDisplacement));
            lineInfo = buf;
        }

        char frameLine[1024] = {};
        sprintf_s(frameLine, "#%02d %s  %s!%s +0x%llX", depth, formatHex64(pc).c_str(), moduleName.c_str(),
                  funcName.c_str(), static_cast<unsigned long long>(displacement));
        log.writeLine(frameLine);
        if (!lineInfo.empty()) {
            log.writeLine(lineInfo);
        }
    }
    return true;
}

void writeEnvironment(Utf8CrashLogFile& log) {
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    wchar_t cwd[MAX_PATH] = {};
    GetCurrentDirectoryW(MAX_PATH, cwd);

    wchar_t host[256] = {};
    DWORD hostLen = static_cast<DWORD>(sizeof(host) / sizeof(host[0]));
    GetComputerNameW(host, &hostLen);

    log.writeLine("【环境】");
    log.writeLine("  计算机名: " + wideToUtf8(host));
    log.writeLine("  可执行文件: " + wideToUtf8(exePath));
    log.writeLine("  工作目录: " + wideToUtf8(cwd));
    log.writeLine("  进程ID: " + std::to_string(GetCurrentProcessId()));
    log.writeLine("  线程ID: " + std::to_string(GetCurrentThreadId()));

    if (!g_crashExtraUtf8.empty()) {
        log.writeLine("  附加信息: " + g_crashExtraUtf8);
    }
}

}  // namespace

void qlogWinSetCrashReportExtraInfoUtf8(const char* utf8) {
    g_crashExtraUtf8 = (utf8 != nullptr) ? utf8 : "";
}

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
        const MINIDUMP_TYPE dumpType = static_cast<MINIDUMP_TYPE>(
            MiniDumpWithThreadInfo | MiniDumpWithProcessThreadData | MiniDumpWithDataSegs | MiniDumpWithHandleData |
            MiniDumpWithUnloadedModules);
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hDumpFile, dumpType, &dumpInfo, nullptr, nullptr);
        CloseHandle(hDumpFile);
    }

    const std::wstring logFilePath = folderPath + L"\\" + timeStr + L"_闪退堆栈.log";
    Utf8CrashLogFile log;
    if (log.open(logFilePath)) {
        log.writeLine("========== 上位机崩溃报告 ==========");
        log.writeLine("时间: " + wideToUtf8(timeStr));
        writeEnvironment(log);
        writeExceptionSummary(log, pException);

        HANDLE hProcess = GetCurrentProcess();
        initSymbolEngine(hProcess);
        captureStackFromException(pException, hProcess, log);
        SymCleanup(hProcess);

        log.writeLine("【分析建议】");
        log.writeLine("  1. 用 Visual Studio / WinDbg 打开同目录 .dmp，查看完整调用栈与局部变量");
        log.writeLine("  2. 将 .exe 同目录的 .pdb 放在符号路径，可显示源文件行号");
        log.writeLine("  3. 对照崩溃前 所有log/上位机log 中最后几条 qDebug 日志");
        log.writeLine("  dump: " + wideToUtf8(dumpFilePath));
        log.close();
    }

    const std::wstring msg = L"程序异常退出，详情已写入:\n" + logFilePath + L"\n\n可用 VS 打开同名的 .dmp 进一步分析";
    MessageBoxW(nullptr, msg.c_str(), L"上位机崩溃", MB_OK | MB_ICONERROR);
    return EXCEPTION_EXECUTE_HANDLER;
}

void Qlog::installWindowsCrashHandler() {
    SetUnhandledExceptionFilter(reinterpret_cast<LPTOP_LEVEL_EXCEPTION_FILTER>(QlogApplicationCrashHandler));
}

#if _MSC_VER >= 1600
#    pragma execution_character_set(pop)
#endif

#endif  // Q_OS_WIN
