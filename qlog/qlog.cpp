#include "qlog.h"

#ifdef Q_OS_WIN
#include "qlog_win.h"
#endif

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QMessageLogContext>
#include <QRegularExpression>
#include <QSysInfo>
#include <QTextStream>

#include <cstdio>

#include "common_utils.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

/** 日志根目录（须 QStringLiteral，禁止 fromLatin1/裸 char*，否则目录名乱码） */
inline QString logRootDir() {
    return QStringLiteral("所有log");
}

bool appendTextLog(const QString& relativeDir, const QString& fileName, const QString& body,
                   bool prefixTimestampLine = false) {
    if (!CommonUtils::ensureLogDirectory(relativeDir)) {
        qDebug() << "无法创建目录:" << relativeDir;
        return false;
    }
    const QString filePath = CommonUtils::joinPath(relativeDir, fileName);
    QFile logFile(filePath);
    if (!logFile.open(QIODevice::Append | QIODevice::Text)) {
        qDebug() << "无法打开日志文件：" << fileName;
        return false;
    }
    QTextStream out(&logFile);
    out.setCodec("UTF-8");
    if (prefixTimestampLine) {
        out << CommonUtils::formatTimestampMs() << "\n";
    }
    out << body << "\n";
    return true;
}

void saveUartRawLogImpl(int txrx, const QByteArray& data, const QString& fileNamePrefix) {
    const QString folderName = logRootDir() + QStringLiteral("/治具log");
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

/** CSV 单元格：含逗号/引号/换行时用双引号包裹，避免 [0,1] 等被 Excel 拆列 */
QString csvEscapeCell(QString value) {
    value.replace(QLatin1String("\r\n"), QString());
    value.replace(QLatin1Char('\n'), QString());
    value.replace(QLatin1Char('\r'), QString());
    if (!value.contains(QLatin1Char(',')) && !value.contains(QLatin1Char('"')) && !value.contains(QLatin1Char('\n')))
        return value;
    value.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QLatin1Char('"') + value + QLatin1Char('"');
}

} // namespace

void qlogQtMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg) {
    Qlog::handleQtMessage(type, context, msg);
}

void Qlog::installQtMessageHandler() {
    qInstallMessageHandler(qlogQtMessageHandler);
}

void Qlog::showlog(const QString& msg, int machineIndex, QPlainTextEdit* msgEdit) {
    if (msg.isEmpty()) {
        return;
    }
    qDebug() << machineIndex << msg;
    if (msgEdit) {
        msgEdit->appendPlainText(msg);
    }
}

void Qlog::handleQtMessage(QtMsgType type, const QMessageLogContext& context, const QString& msg) {
    QString text;
    switch (type) {
    case QtInfoMsg:
        text = QStringLiteral("[Info]");
        break;
    case QtDebugMsg:
        text = QStringLiteral("[Debug]");
        break;
    case QtWarningMsg:
        text = QStringLiteral("[Warning]");
        break;
    case QtCriticalMsg:
        text = QStringLiteral("[Critical]");
        break;
    case QtFatalMsg:
        text = QStringLiteral("[Fatal]");
        break;
    }

    const QString currentDate = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz ddd"));
    const QString message =
        text + currentDate + QStringLiteral(" ") +
        QString(context.file).split(QLatin1Char('\\')).last() + QStringLiteral(" line:") +
        QString::number(context.line) + QStringLiteral(" 日志内容：") + msg;

    const QString folderName = logRootDir() + QStringLiteral("/上位机log");
    if (!CommonUtils::ensureLogDirectory(folderName)) {
        qDebug() << "无法创建目录:" << folderName;
        return;
    }

    QString fileNumber;
    const QRegularExpression re(QStringLiteral("^\\d+"));
    const QRegularExpressionMatch match = re.match(msg.trimmed());
    if (match.hasMatch()) {
        fileNumber = match.captured(0);
    } else {
        fileNumber = QStringLiteral("default");
    }

    const QString hostName = QSysInfo::machineHostName();
    const QString fileName = hostName + QStringLiteral("_上位机日志_") + fileNumber + QLatin1Char('_') +
        CommonUtils::formatDateIso() + QStringLiteral(".txt");
    const QString filePath = CommonUtils::joinPath(folderName, fileName);

    QFile file(filePath);
    const bool isNewLogFile = !file.exists() || file.size() == 0;
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }
    QTextStream textStream(&file);
    textStream.setCodec("UTF-8");
    textStream.setGenerateByteOrderMark(isNewLogFile);

    printf("%s", currentDate.toLocal8Bit().constData());
    printf("  %s\r\n", msg.toLocal8Bit().constData());
    fflush(stdout);
    textStream << message << "\r\n";
    file.close();
}

void Qlog::saveDongleUartLog(int machineIndex, const QString& data) {
    const QString folderName = logRootDir() + QStringLiteral("/dongle的log");
    const QString fileName = QStringLiteral("dongle日志_") + QString::number(machineIndex) + QLatin1Char('_') +
        CommonUtils::dateStampYmd() + QStringLiteral(".log");
    appendTextLog(folderName, fileName, data, false);
}

void Qlog::saveDongleUartLogMain(const QString& data) {
    const QString folderName = logRootDir() + QStringLiteral("/dongle的log");
    const QString fileName = QStringLiteral("dongle日志") + CommonUtils::dateStampYmd() + QStringLiteral(".log");
    appendTextLog(folderName, fileName, data, false);
}

void Qlog::saveBlackboxLog(const QByteArray& data) {
    const QString folderName = logRootDir() + QStringLiteral("/设备黑盒的log");
    const QString fileName = QStringLiteral("黑盒日志") + CommonUtils::dateStampYmd() + QStringLiteral(".log");
    const QString body = CommonUtils::formatTimestampMs() + QStringLiteral("\n") + QString::fromUtf8(data);
    appendTextLog(folderName, fileName, body, false);
}

void Qlog::saveOtaStressLog(const QString& msg) {
    const QString folderName = logRootDir() + QStringLiteral("/ota升级压测/");
    appendTextLog(folderName, QStringLiteral("ota升级log.txt"), msg, false);
}

void Qlog::saveTestCsv(const QString& ver, const QString& sn, const QString& macAddress,
                       const QVector<TestItem>& testItems) {
    const QString folderPath = QStringLiteral("D:/测试结果");
    CommonUtils::ensureDirectory(folderPath);

    const QString fileName = CommonUtils::formatDateIso() + QStringLiteral("_%1报告.csv").arg(ver);
    const QString filePath = CommonUtils::joinPath(folderPath, fileName);

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
        QTextStream stream(&file);
        const QStringList headers = {QStringLiteral("sn"), QStringLiteral("上位机版本"),
                                     QStringLiteral("mac地址"), QStringLiteral("时间戳"),
                                     QStringLiteral("测试项"), QStringLiteral("测试数据"),
                                     QStringLiteral("测试结果"), QStringLiteral("测试要求")};
        const QString timestamp = CommonUtils::dateTimeStamp();

        if (file.size() == 0) {
            stream << headers.join(QStringLiteral(",")) << "\n";
        }

        for (const TestItem& item : testItems) {
            const QStringList row = {sn, ver, macAddress, timestamp, item.testItem, item.testData,
                                     item.testResult, item.ask};
            QStringList escaped;
            escaped.reserve(row.size());
            for (const QString& cell : row)
                escaped.append(csvEscapeCell(cell));
            stream << escaped.join(QStringLiteral(",")) << "\n";
        }
        file.close();
        qDebug() << "Data appended to" << filePath;
    } else {
        qDebug() << "Error appending to file";
    }
}

void Qlog::save_brush_log(int m_index, const QString& macAddress, const QString& data) {
    Q_UNUSED(macAddress);
    const QString folderName = logRootDir() + QStringLiteral("/设备log");
    const QString fileName = QString::number(m_index) + QStringLiteral(".log");
    const QString body = CommonUtils::formatTimestampMs() + QStringLiteral("\n") + data;
    appendTextLog(folderName, fileName, body, false);
}

void Qlog::save_fixture_uart_log(int txrx, const QByteArray& data) {
    saveUartRawLogImpl(txrx, data, QStringLiteral("治具日志"));
}

void Qlog::save_jig_uart_log(int txrx, const QByteArray& data) {
    saveUartRawLogImpl(txrx, data, QStringLiteral("Jig治具日志"));
}

void Qlog::writeRow(QTextStream& stream, const QStringList& rowData) {
    stream << rowData.join(QStringLiteral(",")) << "\n";
}

#ifdef Q_OS_WIN
void Qlog::setCrashReportExtraInfo(const QString& info) {
    qlogWinSetCrashReportExtraInfoUtf8(info.toUtf8().constData());
}
#endif

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
