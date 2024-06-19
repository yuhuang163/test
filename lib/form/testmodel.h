#ifndef TESTMODEL_H
#define TESTMODEL_H

#include <QStandardItemModel >

#if _MSC_VER >= 1600
    #pragma execution_character_set("utf-8")
#endif

class TestItems : public QObject
{
    Q_OBJECT
private:
    enum Policy
    {
        EQUAL,
        BETWEEN,
        LESS_THAN,
        GREATER_THAN,
        NO_TEST,
    } policy;

    int upperLimit, lowerLimit;
    QString equal;
    QString name;

public slots:
    void updateTestResult();
public:
    QStandardItem *testItemName;
    QStandardItem *setValue;
    QStandardItem *testVaule;
    QStandardItem *testResult;

    TestItems(QString name, QString displayName, QString value = "");
    bool hasTestItem(QStandardItem *item);

    QString getName() const;
};

class TestModel : public QStandardItemModel
{
    Q_OBJECT
public:
    TestModel();

    void addTestItem(TestItems *test);
    QStandardItem *getTestItemByName(QString name);
    void resetAllTestResult();
private:
    int itemsCounter = 0;
    QMap<QString, int> namePos;
    QStringList header = {"测试项", "测试要求", "测试值", "测试结果"};
};

#endif   // TESTMODEL_H
