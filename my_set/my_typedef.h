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
#include <QSettings>
#include <QSysInfo>
#include <QTextCodec>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include "qeventloop.h"
#include "qlibrary.h"

/** 团队默认配置（入库） */
#define SETTING_NAME "上位机设置.ini"
/** 本机常改配置（gitignore，见 .gitignore） */
#define SETTING_LOCAL_NAME "上位机设置.local.ini"

inline QString settingsIniPath(const char* fileName) {
    const QString path = QDir(QCoreApplication::applicationDirPath()).filePath(QString::fromUtf8(fileName));
    return QFile::exists(path) ? path : QString::fromUtf8(fileName);
}

/**
 * 仅下列项使用 上位机设置.local.ini，其余一律 上位机设置.ini：
 * 1. 串口与扫码框默认内容（[mechine]、键名含 comName；getMacDefault 仅手动写入 local.ini）
 * 2. 窗口大小（Window/SettingSize、Window/Size）
 * 3. 当前工站（SYSTEM/station、TestOrderMeta/SelectedStation*）
 * 4. WiFi 名称（WIFI/Name、WIFI/Name0…WIFI/Name9 等，各工位/路号）
 */
inline bool settingsUseLocalFile(const QString& fullKey) {
    if (fullKey.isEmpty())
        return false;

    const QString head = fullKey.section(QLatin1Char('/'), 0, 0);
    if (head.compare(QStringLiteral("mechine"), Qt::CaseInsensitive) == 0)
        return true;

    static const QStringList kLocalKeys = {
        QStringLiteral("Window/SettingSize"),
        QStringLiteral("Window/Size"),
        QStringLiteral("SYSTEM/station"),
        QStringLiteral("TestOrderMeta/SelectedStation"),
        QStringLiteral("TestOrderMeta/SelectedStationName"),
    };
    for (const QString& k : kLocalKeys) {
        if (fullKey.compare(k, Qt::CaseInsensitive) == 0)
            return true;
    }

    if (fullKey.contains(QStringLiteral("comName"), Qt::CaseInsensitive))
        return true;

    if (fullKey.startsWith(QStringLiteral("WIFI/Name"), Qt::CaseInsensitive))
        return true;

    return false;
}

inline bool settingsUseLocalGroup(const QString& groupPath) {
    return !groupPath.isEmpty() && settingsUseLocalFile(groupPath + QStringLiteral("/_"));
}

/**
 * SETTINGS：常改项 → local.ini；其余 → 上位机设置.ini（与原单文件语义一致）。
 */
class SettingsManager {
public:
    static SettingsManager& instance() {
        static SettingsManager self;
        return self;
    }

    QVariant value(const QString& key) const { return value(key, QVariant()); }

    QVariant value(const QString& key, const QVariant& defaultValue) const {
        const QString full = fullKey(key);
        if (!settingsUseLocalFile(full)) {
            QSettings& ini = baseIni();
            enter(ini);
            const QVariant v = ini.value(key, defaultValue);
            leave(ini);
            return v;
        }
        QSettings& loc = localIni();
        enter(loc);
        if (loc.contains(key)) {
            const QVariant v = loc.value(key);
            leave(loc);
            return v;
        }
        leave(loc);
        QSettings& ini = baseIni();
        enter(ini);
        const QVariant v = ini.value(key, defaultValue);
        leave(ini);
        return v;
    }

    void setValue(const QString& key, const QVariant& val) {
        QSettings& store = settingsUseLocalFile(fullKey(key)) ? localIni() : baseIni();
        enter(store);
        store.setValue(key, val);
        leave(store);
    }

    void sync() {
        baseIni().sync();
        localIni().sync();
    }

    void beginGroup(const QString& prefix) { groups_.append(prefix); }

    void endGroup() {
        if (!groups_.isEmpty())
            groups_.removeLast();
    }

    QStringList childGroups() const {
        QSettings& ini = baseIni();
        enter(ini);
        QStringList out = ini.childGroups();
        leave(ini);

        const QString here = groups_.join(QLatin1Char('/'));
        if (here.isEmpty() || settingsUseLocalGroup(here)) {
            QSettings& loc = localIni();
            enter(loc);
            for (const QString& g : loc.childGroups()) {
                if (!out.contains(g))
                    out.append(g);
            }
            leave(loc);
        }
        return out;
    }

    QStringList childKeys() const {
        const QString here = groups_.join(QLatin1Char('/'));
        QSettings& ini = baseIni();
        enter(ini);
        QStringList out = ini.childKeys();
        leave(ini);

        if (settingsUseLocalGroup(here)) {
            QSettings& loc = localIni();
            enter(loc);
            for (const QString& k : loc.childKeys()) {
                if (!out.contains(k))
                    out.append(k);
            }
            leave(loc);
        }
        return out;
    }

    void remove(const QString& key) {
        const bool toLocal = key.isEmpty() ? settingsUseLocalGroup(groups_.join(QLatin1Char('/')))
                                           : settingsUseLocalFile(fullKey(key));
        QSettings& store = toLocal ? localIni() : baseIni();
        enter(store);
        store.remove(key);
        leave(store);
    }

private:
    SettingsManager() = default;

    mutable QStringList groups_;

    QString fullKey(const QString& key) const {
        if (key.contains(QLatin1Char('/')))
            return key;
        QStringList parts = groups_;
        if (!key.isEmpty())
            parts.append(key);
        return parts.join(QLatin1Char('/'));
    }

    void enter(QSettings& s) const {
        for (const QString& g : groups_)
            s.beginGroup(g);
    }

    void leave(QSettings& s) const {
        for (int i = 0, n = groups_.size(); i < n; ++i)
            s.endGroup();
    }

    static QSettings& baseIni() {
        static QSettings s(settingsIniPath(SETTING_NAME), QSettings::IniFormat);
        s.setIniCodec(QTextCodec::codecForName("UTF-8"));
        return s;
    }

    static QSettings& localIni() {
        static QSettings s(settingsIniPath(SETTING_LOCAL_NAME), QSettings::IniFormat);
        s.setIniCodec(QTextCodec::codecForName("UTF-8"));
        return s;
    }

    SettingsManager(const SettingsManager&) = delete;
    SettingsManager& operator=(const SettingsManager&) = delete;
};

#define SETTINGS SettingsManager::instance()

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
