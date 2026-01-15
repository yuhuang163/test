QT       += core gui   concurrent serialport printsupport network multimedia  qml quick widgets


QMAKE_PROJECT_DEPTH = 0

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

QMAKE_LFLAGS_RELEASE = /INCREMENTAL:NO /DEBUG /MAP

CONFIG += c++17
QMAKE_CXXFLAGS += /MP
# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0
#D:\qt\5.15.2\msvc2019_64\bin\windeployqt.exe new_production_20250304.exe
# E:\qt\MaintenanceTool.exe --urlreplace download.qt.io mirrors.tuna.tsinghua.edu.cn/qt
# E:\qt\MaintenanceTool.exe --urlreplace download.qt.io mirrors.ustc.edu.cn/qtproject

# E:\qt\MaintenanceTool.exe --urlreplace download.qt.io mirrors.aliyun.com/qt


# 用Windows开发的弊端在内码中显现的再明显不过，首先要明白几个内码的问题，源代码的编码，执行程序的编码，运行环境的编码，这几个不一致都可能出错。
# 首先最坑的是vc编译器，utf8的源文件不带bom它就当做本地编码，所以如果统一用utf8，必须保证源文件带bom。其次是qt用的utf8，要保证执行码也用utf8，vc必须cpp声明执行码为utf8，这些gcc全都不存在问题。
# 另外如果是控制台程序，windows默认本地编码，凡是qt字符串输出都要用tolocal8bit，否则控制台输出也基本是乱码。好好理解源码，执行码，运行环境的编码的关系才能不出错。

# force utf-8 msvc output 不勾选
# 默认编码utf-8
# 如果是utf-8添加bom
# text codec for tools用local
# 语言用中文



INCLUDEPATH += agreement
INCLUDEPATH += agreement/qmes
INCLUDEPATH += agreement/qadb
INCLUDEPATH += agreement/qshell
INCLUDEPATH += agreement/qat
INCLUDEPATH += agreement/qpb
INCLUDEPATH += agreement/qset
INCLUDEPATH += agreement/qpb/ble_protocol
INCLUDEPATH += agreement/qpb/factory_protocol
INCLUDEPATH += agreement/qusb
INCLUDEPATH += agreement/qjig
INCLUDEPATH += agreement/qbrush
INCLUDEPATH += agreement/adb
INCLUDEPATH += advance/imagewindow
INCLUDEPATH += advance/demo
INCLUDEPATH += my_set
INCLUDEPATH += lib/form
INCLUDEPATH += lib/imu
INCLUDEPATH += lib/nfc
INCLUDEPATH += lib/aes
INCLUDEPATH += lib/md5
INCLUDEPATH += lib/productlicense
INCLUDEPATH += lib/qcustomplot
INCLUDEPATH += lib/libusb-win32-bin-1.4.0.0/include


INCLUDEPATH += work_station/ageing
INCLUDEPATH += work_station/camera
INCLUDEPATH += work_station/imu
INCLUDEPATH += work_station/motor
INCLUDEPATH += work_station/quiescent_current
INCLUDEPATH += work_station/screen
INCLUDEPATH += work_station/wifi_ble
INCLUDEPATH += work_station/pcba
INCLUDEPATH += work_station/pressure
INCLUDEPATH += work_station/freework
INCLUDEPATH += work_station
INCLUDEPATH += qlog


# INCLUDEPATH += advance/xlsx
# DEPENDPATH  += advance/xlsx
# include(advance/xlsx/qtxlsx.pri)



SOURCES += \
    advance/demo/usmile_ring_buffer.cpp \
    advance/imagewindow/draggablecheckbox.cpp \
    agreement/qadb/qadb.cpp \
    agreement/qat/qat.cpp \
    agreement/qbrush/qproduct.cpp \
    agreement/qbulk/crc_md5.cpp \
    agreement/qbulk/qbulk.cpp \
    agreement/qjig/fixture_uart.cpp \
    agreement/qjig/qjig.cpp \
    agreement/qmes/hqmes.cpp \
    agreement/qmes/jjmes.cpp \
    agreement/qmes/lxmes.cpp \
    agreement/qmes/mesmanager.cpp \
    agreement/qmes/qmes.cpp \
    agreement/qmes/wksmes.cpp \
    agreement/qmes/xwdmes.cpp \
    agreement/qmes/ydmmes.cpp \
    agreement/qpb/ble_protocol/fx_ble_msg.pb.c \
    agreement/qpb/ble_protocol/data_collection.pb.c \
    agreement/qpb/factory_protocol/factory_msg.pb.c \
    agreement/qpb/pb_common.c \
    agreement/qpb/pb_decode.c \
    agreement/qpb/pb_encode.c \
    agreement/qpb/qpb.cpp \
    agreement/qset/qsetting.cpp \
    agreement/qshell/qshell.cpp \
    agreement/qusb/qusb.cpp \
    djitestfunction.cpp \
    factory_analyzer.cpp \
    lib/form/testmodel.cpp \
    lib/imu/imu_calibrate.cpp \
    lib/imu/sensor_hub.cpp \
    lib/aes/aes.c \
    lib/md5/md5.c \
    lib/imu/us_eigen_nonsymmsquare.cpp \
    lib/productlicense/productlicense.cpp \
    lib/qcustomplot/qcustomplot.cpp \
    main.cpp \
    mainlogic.cpp \
    mainwindow.cpp \
    qlog/qlog.cpp \
    work_station/box_base.cpp \
    work_station/ageing/ageing.cpp \
    work_station/ageing/ageingbox.cpp \
    work_station/camera/camerabox.cpp \
    work_station/camera/cameratest.cpp \
    work_station/common_class.cpp \
    work_station/freework/qfreework.cpp \
    work_station/freework/qfreeworkbox.cpp \
    work_station/freework/testFunction.cpp \
    work_station/imu/imubox.cpp \
    work_station/imu/imucali.cpp \
    work_station/motor/motor.cpp \
    work_station/motor/motorbox.cpp \
    work_station/pcba/pcbabox.cpp \
    work_station/pcba/pcbaform.cpp \
    work_station/quiescent_current/quiescent_current.cpp \
    work_station/quiescent_current/quiescent_current_box.cpp \
    work_station/screen/screenbox.cpp \
    work_station/screen/screentest.cpp \
    work_station/test_base.cpp \
    work_station/wifi_ble/wifibletest.cpp \
    work_station/wifi_ble/wifibox.cpp \
    work_station/pressure/pressuresensorform.cpp \
    work_station/pressure/PressCalibBox.cpp \
    work_station/pressure/ndt_sensor_cali.cpp \

HEADERS += \
    advance/demo/usmile_ring_buffer.h \
    advance/imagewindow/draggablecheckbox.h \
    agreement/qadb/qadb.h \
    agreement/qat/qat.h \
    agreement/qbrush/qproduct.h \
    agreement/qbulk/qbulk.h \
    agreement/qjig/fixture_uart.h \
    agreement/qjig/qjig.h \
    agreement/qmes/hqmes.h \
    agreement/qmes/jjmes.h \
    agreement/qmes/lxmes.h \
    agreement/qmes/mesmanager.h \
    agreement/qmes/qmes.h \
    agreement/qmes/wksmes.h \
    agreement/qmes/xwdmes.h \
    agreement/qmes/ydmmes.h \
    agreement/qpb/ble_protocol/fx_ble_msg.pb.h \
    agreement/qpb/ble_protocol/data_collection.pb.h \
    agreement/qpb/factory_protocol/factory_msg.pb.h \
    agreement/qpb/pb.h \
    agreement/qpb/pb_common.h \
    agreement/qpb/pb_decode.h \
    agreement/qpb/pb_encode.h \
    agreement/qpb/qpb.h \
    agreement/qset/qsetting.h \
    agreement/qshell/qshell.h \
    agreement/qusb/qusb.h \
    factory_analyzer.h \
    lib/form/testmodel.h \
    lib/imu/imu_calibrate.h \
    lib/imu/sensor_hub.h \
    lib/imu/us_eigen_nonsymmsquare.h \
    lib/nfc/dcrf32.h \
    lib/aes/aes.h \
    lib/md5/md5.h \
    lib/productlicense/productlicense.h \
    lib/qcustomplot/qcustomplot.h \
    mainwindow.h \
    my_set/AbIni.h \
    my_set/my_typedef.h \
    qlog/qlog.h \
    work_station/box_base.h \
    work_station/ageing/ageing.h \
    work_station/ageing/ageingbox.h \
    work_station/camera/camerabox.h \
    work_station/camera/cameratest.h \
    work_station/common_class.h \
    work_station/freework/qfreework.h \
    work_station/freework/qfreeworkbox.h \
    work_station/imu/imubox.h \
    work_station/imu/imucali.h \
    work_station/motor/motor.h \
    work_station/motor/motorbox.h \
    work_station/pcba/pcbabox.h \
    work_station/pcba/pcbaform.h \
    work_station/quiescent_current/quiescent_current.h \
    work_station/quiescent_current/quiescent_current_box.h \
    work_station/screen/screenbox.h \
    work_station/screen/screentest.h \
    work_station/test_base.h \
    work_station/wifi_ble/wifibletest.h \
    work_station/wifi_ble/wifibox.h \
    work_station/pressure/pressuresensorform.h \
    work_station/pressure/PressCalibBox.h \
    work_station/pressure/ndt_sensor_cali.h \

FORMS += \
    agreement/qjig/fixture_uart.ui \
    agreement/qset/qsetting.ui \
    factory_analyzer.ui \
    mainwindow.ui \
    work_station/ageing/ageing.ui \
    work_station/ageing/ageingbox.ui \
    work_station/camera/camerabox.ui \
    work_station/camera/cameratest.ui \
    work_station/freework/qfreework.ui \
    work_station/freework/qfreeworkbox.ui \
    work_station/imu/imubox.ui \
    work_station/imu/imucali.ui \
    work_station/motor/motor.ui \
    work_station/motor/motorbox.ui \
    work_station/pcba/pcbabox.ui \
    work_station/pcba/pcbaform.ui \
    work_station/quiescent_current/quiescent_current.ui \
    work_station/quiescent_current/quiescent_current_box.ui \
    work_station/screen/screenbox.ui \
    work_station/screen/screentest.ui \
    work_station/wifi_ble/wifibletest.ui \
    work_station/wifi_ble/wifibox.ui \
    work_station/pressure/pressuresensorform.ui \
    work_station/pressure/PressCalibBox.ui \

TRANSLATIONS += \
    new_production_zh_CN.ts
CONFIG += lrelease
CONFIG += embed_translations

#CONFIG += incremental

# 添加config配置
CONFIG += AbIni
# 指定要使用的预编译头文件
PRECOMPILED_HEADER += $$PWD/my_set/AbIni.h

RC_ICONS = ./stytle/picture/dji1.ico
# 获取当前时间（Windows）
current_time = $$system(powershell -command "(Get-Date).ToString('yyyyMMdd')")

# 设置TARGET名称
TARGET = new_production_$$current_time

# 将可执行文件放在项目目录的bin文件夹中
DESTDIR = ./bin

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target


LIBS += -L$$PWD/lib/nfc/ -ldcrf32
LIBS += -lwinusb -lsetupapi
LIBS += -lDbgHelp
LIBS += -L$$PWD/lib/libusb-win32-bin-1.4.0.0/lib/msvc_x64 -llibusb
win32 {
    LIBS += -luser32
}





DISTFILES += \
    agreement/qpb/ble_protocol/fx_ble_msg.proto \
    agreement/qpb/ble_protocol/data_collection.proto \
    agreement/qpb/ble_protocol/server_data.proto \
    agreement/qpb/factory_protocol/factory_msg.proto \
    lib/nfc/dcrf32.dll \
    lib/nfc/dcrf32.lib



