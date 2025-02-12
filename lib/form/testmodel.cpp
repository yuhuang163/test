#include "testmodel.h"

#include <QDebug>
#include <QRegularExpression>

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

TestModel::TestModel() { setHorizontalHeaderLabels(header); }

void TestModel::addTestItem(TestItems* test) {
    connect(this, &QStandardItemModel::itemChanged, this, [=](QStandardItem* item) {
        if (test->hasTestItem(item)) {
            test->updateTestResult();
        }
    });
    this->setItem(itemsCounter, 0, test->testItemName);
    this->setItem(itemsCounter, 1, test->setValue);
    this->setItem(itemsCounter, 2, test->testValue);
    this->setItem(itemsCounter, 3, test->testResult);
    namePos[test->getName()] = itemsCounter;
    itemsCounter++;
}

QStandardItem* TestModel::getTestItemByName(QString name) {
    if (namePos.contains(name)) {
        int row = namePos[name];
        return this->item(row, 2);
    } else {
        qDebug() << name << "not found";
        return NULL;
    }
}

void TestModel::resetAllTestResult() {
    for (auto n : namePos.keys()) {
        int row = namePos[n];
        auto item1 = this->item(row, 3);
        item1->setData("", Qt::DisplayRole);
        auto item2 = this->item(row, 2);
        item2->setData("", Qt::DisplayRole);
        QBrush brush(Qt::white);
        item1->setBackground(brush);
    }
}

QString TestItems::getName() const { return name; }

void TestItems::updateTestResult() {
    int data;
    enum { INIT, PASS, FAIL } result = INIT;

    struct {
        QString desc;
        QColor color;
    } resultDisPlay[3];

    resultDisPlay[INIT] = {"", Qt::white};
    resultDisPlay[PASS] = {"PASS", Qt::green};
    resultDisPlay[FAIL] = {"FAIL", Qt::red};

    switch (policy) {
        case Policy::EQUAL: result = (equal.contains(testValue->data(Qt::DisplayRole).toString())) ? PASS : FAIL; break;

        case Policy::BETWEEN:
            // TODO:待定义格式
            if ((testValue->data(Qt::DisplayRole).toInt() < upperLimit) &&
                (testValue->data(Qt::DisplayRole).toInt() > lowerLimit)) {
                result = PASS;
            } else {
                result = FAIL;
            }
            break;

        case Policy::GREATER_THAN:
            data = testValue->data(Qt::DisplayRole).toInt();
            result = (data > lowerLimit) ? PASS : FAIL;
            break;

        case Policy::LESS_THAN:
            data = testValue->data(Qt::DisplayRole).toInt();
            result = (data < upperLimit) ? PASS : FAIL;
            break;

        case Policy::NO_TEST: result = INIT; break;

        default: break;
    }

    testResult->setData(resultDisPlay[result].desc, Qt::DisplayRole);
    QBrush brush(resultDisPlay[result].color);
    testResult->setBackground(brush);
}

TestItems::TestItems(QString name, QString displayName, QString value) {
    this->name = name;

    QString x = value;
    x = x.remove(0, 1);

    testItemName = new QStandardItem(displayName);
    setValue = new QStandardItem(value);
    testValue = new QStandardItem("");
    testResult = new QStandardItem("");

    policy = Policy::EQUAL;

    if (value.isEmpty()) {
        policy = Policy::NO_TEST;
        return;
    }

    //"=x",> x" , "< x" , "(x0,x1)"
    char c = value[0].toLatin1();
    switch (c) {
        case '>':
            policy = Policy::GREATER_THAN;
            lowerLimit = x.toInt();
            break;

        case '<':
            policy = Policy::LESS_THAN;
            upperLimit = x.toInt();
            break;

        case '(':
            policy = Policy::BETWEEN;
            {
                QRegularExpression regex("\\((\\d+),(\\d+)\\)");
                QRegularExpressionMatch match = regex.match(value);
                if (match.hasMatch()) {
                    QString matchedText1 = match.captured(1);
                    QString matchedText2 = match.captured(2);
                    lowerLimit = matchedText1.toInt();
                    upperLimit = matchedText2.toInt();
                }
            }
            break;

        case '=':
            policy = Policy::EQUAL;
            equal = x;
            break;

        default: qDebug() << "设定值错误"; break;
    }
}

bool TestItems::hasTestItem(QStandardItem* item) {
    return item == testValue;
    // testValue
    //  return item.contains(testValue );
}
