#ifndef MY_TYPEDEF_H
#define MY_TYPEDEF_H

#include <QDebug>
#include <QDir>
#include <QHostInfo>
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

typedef struct MesPacketData {
    QString factory;         //工厂
    QString userNo;          //登陆的用户名
    QString password;        //密码
    QString machineNo;       //工站
    QString Employee_ID;     //员工工号
    QString action;          //动作
    int mechines;            //机号
    QString line;            //线体
    QString sn;              // sn
    QString result;          //结果
    QString itemvalue;       //测试项
    QString model;           //机种
    int is_hq_send_mac = 0;  //机号
    QString test_station;    //制程名称
    QString error;           //错误
    QString instruct_num;    //指令标识
    QString product;         //产品名字
    QString lotName;         //工单号码

} MesPacketData;

#endif  // MY_TYPEDEF_H
