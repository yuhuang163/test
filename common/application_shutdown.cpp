#include "application_shutdown.h"

#include "qlog.h"
#include "test_case_sync_service.h"

#include <QCoreApplication>
#include <QProcess>
#include <QThreadPool>
#include <QTimer>

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

void ApplicationShutdown::prepareForExit() {
    static bool done = false;
    if (done) {
        return;
    }
    done = true;

    TestCaseSyncService::stopDeviceAgent();
    Qlog::flushLogBuffers();
    QThreadPool::globalInstance()->waitForDone(3000);
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
