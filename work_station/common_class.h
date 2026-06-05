#ifndef COMMON_CLASS_H
#define COMMON_CLASS_H

#include <QDebug>
#include <QMainWindow>
#include <QObject>
#include <QString>
#include <QTimer>
#include <functional>
#include <vector>

#include "agreement/qProtocol/qprotocolmanager.h"
#include "qpb.h"

class Qpb;

struct NamedFunction {
    QString name;
    std::function<void()> function;
};

class common_class {
public:
    common_class();
};

class TestFunctionExecutor : public QObject {
    Q_OBJECT

public:
    explicit TestFunctionExecutor(Qpb* pb);
    void executeFunctionByName(const QString& functionName);
    void createTestFunctions();
    int sendCommandWithRetry(std::function<void()> commandFunc);

public slots:
    void solveGetBrushResponse(int data);

private:
    Qpb* pb = nullptr;
    QProtocolManager protocolManager;
    bool getRespone = false;
    bool canGoNext = false;
    bool sendRetryOver = false;
    std::vector<NamedFunction> testFunctions;
};

#endif  // COMMON_CLASS_H
