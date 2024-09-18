QT       += core gui axcontainer concurrent serialport printsupport network multimedia webengine qml quick widgets webenginewidgets


greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# # 解决msvc构建套件下qtcreator控制台日志中文乱码问题
# msvc {
#     QMAKE_CFLAGS += /utf-8
#     QMAKE_CXXFLAGS += /utf-8
# }


CONFIG += c++17
QMAKE_CXXFLAGS += /MP
# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0
#D:\qt\5.15.2\msvc2019_64\bin\windeployqt.exe new_production.exe

INCLUDEPATH += agreement
INCLUDEPATH += agreement/qmes
INCLUDEPATH += agreement/qat
INCLUDEPATH += agreement/qpb
INCLUDEPATH += agreement/qpb/ble_protocol
INCLUDEPATH += agreement/qpb/factory_protocol
INCLUDEPATH += agreement/qusb
INCLUDEPATH += agreement/qjig
INCLUDEPATH += agreement/qbrush
INCLUDEPATH += advance/imagewindow
INCLUDEPATH += advance/demo
INCLUDEPATH += my_set
INCLUDEPATH += lib/form
INCLUDEPATH += lib/imu
INCLUDEPATH += lib/nfc
INCLUDEPATH += lib/productlicense
INCLUDEPATH += lib/quicklz
INCLUDEPATH += work_station/ageing
INCLUDEPATH += work_station/camera
INCLUDEPATH += work_station/imu
INCLUDEPATH += work_station/motor
INCLUDEPATH += work_station/quiescent_current
INCLUDEPATH += work_station/screen
INCLUDEPATH += work_station/wifi_ble
INCLUDEPATH += work_station/pcba
INCLUDEPATH += work_station/freework
INCLUDEPATH += work_station
INCLUDEPATH += qlog


# INCLUDEPATH += advance/xlsx
# DEPENDPATH  += advance/xlsx

include(advance/xlsx/qtxlsx.pri)



SOURCES += \
    advance/demo/usmile_ring_buffer.cpp \
    advance/imagewindow/draggablecheckbox.cpp \
    advance/imagewindow/imagewindow.cpp \
    agreement/qat/qat.cpp \
    agreement/qbrush/qproduct.cpp \
    agreement/qjig/fixture_uart.cpp \
    agreement/qjig/qjig.cpp \
    agreement/qmes/hqmes.cpp \
    agreement/qmes/jjmes.cpp \
    agreement/qmes/lxmes.cpp \
    agreement/qmes/mesmanager.cpp \
    agreement/qmes/qmes.cpp \
    agreement/qmes/xwdmes.cpp \
    agreement/qpb/ble_protocol/fx_ble_msg.pb.c \
    agreement/qpb/ble_protocol/data_collection.pb.c \
    agreement/qpb/factory_protocol/factory_msg.pb.c \
    agreement/qpb/pb_common.c \
    agreement/qpb/pb_decode.c \
    agreement/qpb/pb_encode.c \
    agreement/qpb/qpb.cpp \
    agreement/qusb/qusb.cpp \
    lib/form/testmodel.cpp \
    lib/imu/imu_calibrate.cpp \
    lib/imu/sensor_hub.cpp \
    lib/imu/us_eigen_nonsymmsquare.cpp \
    lib/productlicense/productlicense.cpp \
    lib/quicklz/quicklz.cpp \
    lib/quicklz/quicklz_dec.cpp \
    main.cpp \
    mainlogic.cpp \
    mainwindow.cpp \
    qlog/qlog.cpp \
    work_station/box_base.cpp \
    work_station/ageing/ageing.cpp \
    work_station/ageing/ageingbox.cpp \
    work_station/camera/camerabox.cpp \
    work_station/camera/cameratest.cpp \
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
    work_station/wifi_ble/wifibox.cpp

HEADERS += \
    advance/demo/usmile_ring_buffer.h \
    advance/imagewindow/draggablecheckbox.h \
    advance/imagewindow/imagewindow.h \
    agreement/qat/qat.h \
    agreement/qbrush/qproduct.h \
    agreement/qjig/fixture_uart.h \
    agreement/qjig/qjig.h \
    agreement/qmes/hqmes.h \
    agreement/qmes/jjmes.h \
    agreement/qmes/lxmes.h \
    agreement/qmes/mesmanager.h \
    agreement/qmes/qmes.h \
    agreement/qmes/xwdmes.h \
    agreement/qpb/ble_protocol/fx_ble_msg.pb.h \
    agreement/qpb/ble_protocol/data_collection.pb.h \
    agreement/qpb/factory_protocol/factory_msg.pb.h \
    agreement/qpb/pb.h \
    agreement/qpb/pb_common.h \
    agreement/qpb/pb_decode.h \
    agreement/qpb/pb_encode.h \
    agreement/qpb/qpb.h \
    agreement/qusb/qusb.h \
    lib/form/testmodel.h \
    lib/imu/imu_calibrate.h \
    lib/imu/sensor_hub.h \
    lib/imu/us_eigen_nonsymmsquare.h \
    lib/nfc/dcrf32.h \
    lib/productlicense/productlicense.h \
    lib/quicklz/quicklz.h \
    lib/quicklz/quicklz_dec.h \
    mainwindow.h \
    my_set/AbIni.h \
    my_set/my_typedef.h \
    qlog/qlog.h \
    work_station/box_base.h \
    work_station/ageing/ageing.h \
    work_station/ageing/ageingbox.h \
    work_station/camera/camerabox.h \
    work_station/camera/cameratest.h \
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
    work_station/wifi_ble/wifibox.h

FORMS += \
    advance/imagewindow/imagewindow.ui \
    agreement/qjig/fixture_uart.ui \
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
    work_station/wifi_ble/wifibox.ui

TRANSLATIONS += \
    new_production_zh_CN.ts
CONFIG += lrelease
CONFIG += embed_translations

#CONFIG += incremental

# 添加config配置
CONFIG += AbIni
# 指定要使用的预编译头文件
PRECOMPILED_HEADER += $$PWD/my_set/AbIni.h

RC_ICONS = ./stytle/picture/usmile.ico
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

DISTFILES += \
    agreement/qpb/ble_protocol/fx_ble_msg.proto \
    agreement/qpb/ble_protocol/data_collection.proto \
    agreement/qpb/ble_protocol/server_data.proto \
    agreement/qpb/factory_protocol/factory_msg.proto \
    lib/nfc/dcrf32.dll \
    lib/nfc/dcrf32.lib



