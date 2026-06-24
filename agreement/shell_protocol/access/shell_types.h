#ifndef SHELL_TYPES_H
#define SHELL_TYPES_H

#include <QString>
#include <QVariant>

enum class ShellCmd {
    ExecuteCommand,
    RunScript,
    TerminateProcess
};

struct ProtocolShellData {
    bool success;
    int exitCode;
    QString standardOutput;
    QString standardError;
};

#endif // SHELL_TYPES_H
