#include "test_case_paths.h"

#include <QCoreApplication>
#include <QDir>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace TestCasePaths {

QString rootDir() {
    return QCoreApplication::applicationDirPath() + QStringLiteral("/test_case");
}

QString flowIniFileName() {
    return QStringLiteral("总的测试流程.ini");
}

QString flowIniPath() {
    return rootDir() + QLatin1Char('/') + flowIniFileName();
}

QString caseIniPath(const QString& caseName) {
    return rootDir() + QLatin1Char('/') + caseName.trimmed() + QStringLiteral(".ini");
}

QString stepsDir() {
    return rootDir() + QStringLiteral("/steps");
}

QString profilesDir() {
    return rootDir() + QStringLiteral("/profiles");
}

QString stepLibraryPath(const QString& stepId) {
    return stepsDir() + QLatin1Char('/') + stepId.trimmed() + QStringLiteral(".ini");
}

bool ensureRootDir() {
    QDir dir(rootDir());
    if (!dir.exists() && !dir.mkpath(QStringLiteral(".")))
        return false;
    QDir(stepsDir()).mkpath(QStringLiteral("."));
    QDir(profilesDir()).mkpath(QStringLiteral("."));
    return true;
}

bool isReservedCaseName(const QString& name) {
    const QString n = name.trimmed();
    if (n.isEmpty())
        return true;
    if (n.compare(flowIniFileName(), Qt::CaseInsensitive) == 0)
        return true;
    if (n.compare(QStringLiteral("总的测试流程"), Qt::CaseInsensitive) == 0)
        return true;
    return false;
}

bool isValidCaseFileName(const QString& name, QString* errorOut) {
    const QString n = name.trimmed();
    if (n.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("名称不能为空");
        return false;
    }
    if (isReservedCaseName(n)) {
        if (errorOut)
            *errorOut = QStringLiteral("名称与保留名冲突");
        return false;
    }
    static const QString forbidden = QStringLiteral("\\/:*?\"<>|");
    for (const QChar c : n) {
        if (forbidden.contains(c)) {
            if (errorOut)
                *errorOut = QStringLiteral("名称不能包含 \\ / : * ? \" < > |");
            return false;
        }
        if (c.category() == QChar::Other_Control) {
            if (errorOut)
                *errorOut = QStringLiteral("名称含有非法控制字符");
            return false;
        }
    }
    return true;
}

} // namespace TestCasePaths

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
