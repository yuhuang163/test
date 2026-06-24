#ifndef QLOG_WIN_H
#define QLOG_WIN_H

#ifdef Q_OS_WIN

#include <Windows.h>

/** MyApplication __except 与 SetUnhandledExceptionFilter 共用（实现见 qlog_win.cpp） */
LONG WINAPI QlogApplicationCrashHandler(EXCEPTION_POINTERS* exceptionPointers);

/** 由 Qlog::setCrashReportExtraInfo 调用，崩溃时只读 */
void qlogWinSetCrashReportExtraInfoUtf8(const char* utf8);

#endif // Q_OS_WIN

#endif // QLOG_WIN_H
