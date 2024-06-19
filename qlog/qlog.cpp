#include "qlog.h"
#if _MSC_VER >= 1600
    #pragma execution_character_set("utf-8")
#endif
Qlog::Qlog() {}

void Qlog::saveTestCsv(const QString &ver, const QString &sn, const QString &macAddress,
                       const QVector<TestItem> &testItems)
{
    // 构建 "测试结果" 文件夹的完整路径，这里选择保存到D盘
    QString folderPath = "D:/测试结果";

    // 如果 "测试结果" 文件夹不存在，则创建它
    if (!QDir(folderPath).exists())
    {
        QDir().mkpath(folderPath);
    }

    // 获取当前日期
    QDate currentDate = QDate::currentDate();

    // 构建完整的文件路径，加上日期
    QString fileName = currentDate.toString("yyyy-MM-dd") + QString("_%1报告.csv").arg(ver);
    QString filePath = QDir(folderPath).filePath(fileName);

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append))
    {
        QTextStream stream(&file);

        // 写入表头
        QStringList headers = {"sn",     "上位机版本", "mac地址", "时间戳",
                               "测试项", "测试数据",   "测试结果"};

        // 获取当前时间戳
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");

        if (file.size() == 0)
        {
            stream << headers.join(",") << "\n";
        }

        // 写入数据
        for (const TestItem &item : testItems)
        {
            stream << sn << "," << ver << "," << macAddress << "," << timestamp << ","
                   << item.testItem << "," << item.testData << "," << item.testResult << "\n";
        }
        file.close();
        qDebug() << "Data appended to" << filePath;
    }
    else
    {
        qDebug() << "Error appending to file";
    }
}



void Qlog::save_brush_log(QString macAddress, QString data)
{
    // 创建log目录
    QDir logDir(".");
    logDir.mkdir("log");
    // macAddress ="3c:84:27:29:50:32";
    QString fileNamemacAddress = macAddress;
    // 打开log文件
    QString fileName = "log/" + fileNamemacAddress.remove(":") + ".log";
    QFile logFile(fileName);
    // qDebug() << "macAddress:" << macAddress;

    if (logFile.open(QIODevice::Append | QIODevice::Text))
    {
        //qDebug() << "写入成功牙刷日志";
        // 写入数据
        QTextStream out(&logFile);
        out << data << endl;
        logFile.close();
    }
    else
    {
        qDebug() << "无法打开牙刷日志文件：" << fileName;
    }
}
