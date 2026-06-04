#include "qlog.h"

#include <QDebug>
#include <QFile>
#include <QTextStream>

#include "common_utils.h"
#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif
Qlog::Qlog() {}

void Qlog::saveTestCsv(const QString& ver, const QString& sn, const QString& macAddress,
                       const QVector<TestItem>& testItems) {
    // 构建 "测试结果" 文件夹的完整路径，这里选择保存到D盘
    const QString folderPath = QStringLiteral("D:/测试结果");
    CommonUtils::ensureDirectory(folderPath);

    const QString fileName = CommonUtils::formatDateIso() + QStringLiteral("_%1报告.csv").arg(ver);
    const QString filePath = CommonUtils::joinPath(folderPath, fileName);

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
        QTextStream stream(&file);

        // 写入表头
        QStringList headers = {"sn", "上位机版本", "mac地址", "时间戳", "测试项", "测试数据", "测试结果", "测试要求"};

        // 获取当前时间戳
        const QString timestamp = CommonUtils::dateTimeStamp();

        if (file.size() == 0) {
            stream << headers.join(",") << "\n";
        }

        // 写入数据
        for (const TestItem& item : testItems) {
            QString testData = item.testData;  // 创建一个非 const 的副本
            testData.replace("\r\n", "");      // 移除所有的换行符
            testData.replace(",", "");         // 移除所有的换行符
            stream << sn << "," << ver << "," << macAddress << "," << timestamp << "," << item.testItem << ","
                   << testData << "," << item.testResult << "," << item.ask << "\n";
        }
        file.close();
        qDebug() << "Data appended to" << filePath;
    } else {
        qDebug() << "Error appending to file";
    }
}

void Qlog::save_brush_log(int m_index, QString macAddress, QString data) {
    const QString folderName = QStringLiteral("所有log/设备log");
    if (!CommonUtils::ensureLogDirectory(folderName)) {
        qDebug() << "无法创建目录:" << folderName;
        return;
    }

    const QString fileName = QString::number(m_index) + QStringLiteral(".log");
    const QString filePath = CommonUtils::joinPath(folderName, fileName);

    QFile logFile(filePath);

    // qDebug() << "macAddress:" << macAddress;

    if (logFile.open(QIODevice::Append | QIODevice::Text)) {
        // qDebug() << "写入成功设备日志";
        // 写入数据
        QTextStream out(&logFile);
        const QString detailedTimestamp = CommonUtils::formatTimestampMs();
        out << detailedTimestamp << "\n" << data << "\n";

        //   out << data << '\n';
        logFile.close();
    } else {
        qDebug() << "无法打开设备日志文件：" << fileName;
    }
}

namespace {

void saveJigUartLogImpl(int txrx, const QByteArray& data, const QString& fileNamePrefix) {
    const QString folderName = QStringLiteral("所有log/治具log");
    if (!CommonUtils::ensureLogDirectory(folderName)) {
        qDebug() << "无法创建目录:" << folderName;
        return;
    }

    const QString fileName = fileNamePrefix + CommonUtils::dateStampYmd() + QStringLiteral(".log");
    const QString filePath = CommonUtils::joinPath(folderName, fileName);

    QFile logFile(filePath);
    if (!logFile.open(QIODevice::Append | QIODevice::Text)) {
        qDebug() << "无法打开治具日志文件：" << fileName;
        return;
    }

    qDebug() << "写入成功治具日志";
    QTextStream out(&logFile);
    out.setCodec("UTF-8");
    const QString detailedTimestamp = CommonUtils::formatTimestampMs();
    const QString hexData = CommonUtils::toHexUpperSpaced(data);

    if (txrx) {
        out << detailedTimestamp << QStringLiteral("- tx发送的原始数据为：") << data << "\n";
        out << detailedTimestamp << QStringLiteral("- tx发送的16进制数据：") << hexData << "\n";
    } else {
        out << detailedTimestamp << QStringLiteral("- rx接收的原始数据为：") << data << "\n";
        out << detailedTimestamp << QStringLiteral("- rx接收的16进制数据：") << hexData << "\n";
    }
    logFile.close();
}

}  // namespace

void Qlog::save_fixture_uart_log(int txrx, const QByteArray& data) {
    saveJigUartLogImpl(txrx, data, QStringLiteral("治具日志"));
}

void Qlog::save_jig_uart_log(int txrx, const QByteArray& data) {
    saveJigUartLogImpl(txrx, data, QStringLiteral("Jig治具日志"));
}
