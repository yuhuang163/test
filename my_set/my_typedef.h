#ifndef MY_TYPEDEF_H
#define MY_TYPEDEF_H

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostInfo>
#include <QTextStream>
#include <QWidget>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMainWindow>
#include <QNetworkInterface>
#include <QObject>
#include <QSerialPort>
#include <QSysInfo>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include "qeventloop.h"
#include "qlibrary.h"

#define SETTING_NAME "上位机设置.ini"

#define SETTINGS SettingsManager::instance()
class SettingsManager {
public:
    //获取单例实例的方法
    static QSettings& instance() {
        static QSettings settings(SETTING_NAME, QSettings::IniFormat);
        settings.setIniCodec(QTextCodec::codecForName("UTF-8"));
        return settings;
    }

private:
    // 私有化构造函数
    SettingsManager() = default;
    ~SettingsManager() = default;

    // 删除拷贝构造函数和赋值操作符
    SettingsManager(const SettingsManager&) = delete;
    SettingsManager& operator=(const SettingsManager&) = delete;
};

typedef struct MesPacketData {
    QString factory;         //工厂
    QString userNo;          //登陆的用户名
    QString password;        //密码
    QString machineNo;       //工站
    QString Employee_ID;     //员工工号
    QString action;          //动作
    int mechines;            //上位机机号
    QString line;            //线体
    QString sn;              // sn
    QString result;          //结果
    QString itemvalue;       //测试项
    QString maxValue;        // 上限
    QString minValue;        // 下限
    QString standardValue;   // 标准值
    QString unit;            // 单位
    QString testTime;        // 开测/分项 TEST_TIME；空则 bydmes 用当前时间
    QString remark;          // 备注
    QString model;           //机种
    int is_hq_send_mac = 0;  //区分是否发送mac
    QString test_station;    //制程名称
    QString error;           //错误
    QString instruct_num;    //指令标识
    QString product;         //产品名字
    QString lotName;         //工单号码
    QString mac;             // mac地址
    int elapseTime = 1;      //测试耗时（默认 1，单位由 MES 接口定义）
    int testCount = 1;       //测试次数（默认 1）
    int iskeydata = 0;  // GetTestData 入口分流：0=常规取数，1=BYD AddSfcKey

} MesPacketData;

/** 统一 QSS 路径：仓库 stytle/qss，运行时优先 qrc，其次 exe 旁 stytle/qss。 */
inline QString resolveAppQssPath(const QString& styleFileName) {
    if (styleFileName.startsWith(QLatin1Char(':')))
        return styleFileName;
    const QString fileName =
        styleFileName.isEmpty() ? QStringLiteral("Ubuntu.qss") : QFileInfo(styleFileName).fileName();
    return QStringLiteral(":/stytle/qss/") + fileName;
}

/** 加载并应用界面 QSS；默认 Ubuntu.qss 对应 stytle/qss/Ubuntu.qss。 */
inline bool applyWidgetStyleSheet(QWidget* widget, const QString& style = QStringLiteral("Ubuntu.qss")) {
    if (!widget)
        return false;

    const QString fileName = style.isEmpty() ? QStringLiteral("Ubuntu.qss") : QFileInfo(style).fileName();
    const QStringList candidates = {
        resolveAppQssPath(style),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("stytle/qss/") + fileName),
        QStringLiteral("stytle/qss/") + fileName,
    };

    for (const QString& path : candidates) {
        QFile qss(path);
        if (!qss.open(QFile::ReadOnly))
            continue;
        QTextStream filetext(&qss);
        widget->setStyleSheet(filetext.readAll());
        qss.close();
        qDebug() << "qss load" << path;
        return true;
    }

    qDebug() << "qss not load" << candidates;
    return false;
}

#endif  // MY_TYPEDEF_H
