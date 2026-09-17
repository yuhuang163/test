#include "application_shutdown.h"

#include "qlog.h"
#include "test_case_sync_service.h"

#include <QCoreApplication>
#include <QProcess>
#include <QThreadPool>
#include <QTimer>

#include <atomic>

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

namespace {
std::atomic<bool> g_exiting{false};
} // namespace

bool ApplicationShutdown::isExiting() {
    return g_exiting.load();
}

void ApplicationShutdown::resetForNewSession() {
    g_exiting.store(false);
}

void ApplicationShutdown::prepareForExit() {
    static bool done = false;
    g_exiting.store(true);
    if (done) {
        return;
    }
    done = true;

    TestCaseSyncService::stopDeviceAgent(true);
    Qlog::flushLogBuffers();
    QThreadPool::globalInstance()->waitForDone(800);
}

void ApplicationShutdown::prepareForOtaReplace() {
    static bool done = false;
    if (done) {
        return;
    }
    done = true;

    TestCaseSyncService::stopDeviceAgent(true);
    Qlog::flushLogBuffers();
    QThreadPool::globalInstance()->waitForDone(800);
}

void ApplicationShutdown::scheduleForceExitForOta(int delayMs) {
    prepareForExit();
    QTimer::singleShot(delayMs, []() {
#ifdef Q_OS_WIN
        QProcess killer;
        killer.start(QStringLiteral("taskkill"),
                     {QStringLiteral("/F"), QStringLiteral("/T"), QStringLiteral("/PID"),
                      QString::number(QCoreApplication::applicationPid())});
        killer.waitForFinished(5000);
        TerminateProcess(GetCurrentProcess(), 0);
#endif
        if (QCoreApplication::instance()) {
            QCoreApplication::quit();
        }
    });
}
