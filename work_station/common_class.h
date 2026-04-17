#ifndef COMMON_CLASS_H
#define COMMON_CLASS_H

class common_class {
public:
    common_class();
};
#include <QDebug>
#include <QMainWindow>  // 主窗口类
#include <QString>
#include <QTimer>  // 定时器
#include <functional>
#include <vector>

#include "agreement/qProtocol/qprotocolmanager.h"
#include "qpb.h"  // 引入 Qpb 头文件
class Qpb;  // 前向声明 Qpb 类型，避免头文件依赖

// 声明一个结构来存储函数的名称和对应的执行函数
struct NamedFunction {
    QString name;
    std::function<void()> function;
};

class TestFunctionExecutor : public QObject {
    Q_OBJECT
public:
    // 构造函数，传入需要操作的对象
    TestFunctionExecutor(Qpb* pb);

    // 执行特定名称的函数
    void executeFunctionByName(const QString& functionName);

    // 注册测试函数
    void createTestFunctions();
    int sendCommandWithRetry(std::function<void()> commandFunc);

private:
    Qpb* pb;  // 操作对象指针
    QProtocolManager protocolManager;

    bool getRespone = 0;
    bool canGoNext = false;
    bool sendRetryOver = false;
    std::vector<NamedFunction> testFunctions;  // 存储函数列表
public slots:
    void solveGetBrushResponse(int data);
};

#endif  // COMMON_CLASS_H
