QT       += core gui   concurrent serialport printsupport network multimedia  qml quick widgets quickwidgets


QMAKE_PROJECT_DEPTH = 0

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

QMAKE_LFLAGS_RELEASE = /INCREMENTAL:NO /DEBUG /MAP

CONFIG += c++17
QMAKE_CXXFLAGS += /MP
# MSVC: keep UTF-8 source files with Chinese literals parsed consistently.
QMAKE_CXXFLAGS += /utf-8
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

# Qt Creator编写代码时界面经常卡死解决办法
# 帮助->关于插件->C+±>取消勾选ClangCodeModel（重启QtCreator）


INCLUDEPATH += agreement
INCLUDEPATH += agreement/qmes
INCLUDEPATH += agreement/qadb
INCLUDEPATH += agreement/qshell
INCLUDEPATH += agreement/qat
INCLUDEPATH += agreement/qtransport
INCLUDEPATH += agreement/qProtocol
INCLUDEPATH += agreement/qProtocol/qpb
INCLUDEPATH += agreement/qProtocol/qfctp
INCLUDEPATH += agreement/qProtocol/qfctp/common_protocl
INCLUDEPATH += agreement/qProtocol/qaiot
INCLUDEPATH += agreement/qset
INCLUDEPATH += agreement/qProtocol/qpb/ble_protocol
INCLUDEPATH += agreement/qProtocol/qpb/factory_protocol
INCLUDEPATH += agreement/qusb
INCLUDEPATH += agreement/qjig
INCLUDEPATH += agreement/qbrush
INCLUDEPATH += agreement/qmomcozy
INCLUDEPATH += agreement/adb
INCLUDEPATH += agreement/qtuple
INCLUDEPATH += agreement/qplc
INCLUDEPATH += agreement/qvisa
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
INCLUDEPATH += work_station/key
INCLUDEPATH += work_station/suction
INCLUDEPATH += work_station
INCLUDEPATH += qlog


# INCLUDEPATH += advance/xlsx
# DEPENDPATH  += advance/xlsx
# include(advance/xlsx/qtxlsx.pri)



SOURCES += \
    advance/demo/usmile_ring_buffer.cpp \
    advance/imagewindow/draggablecheckbox.cpp \
    advance/imagewindow/myopenglwidget.cpp \
    agreement/qtransport/qchannel.cpp \
    agreement/qtransport/qmodbus_pdu.cpp \
    agreement/qtransport/qprocesschannel.cpp \
    agreement/qtransport/qserialportreader.cpp \
    agreement/qProtocol/qfctp/common_protocl/comm_protocol_builder.cpp \
    agreement/qProtocol/qfctp/common_protocl/comm_protocol_parser.cpp \
    agreement/qProtocol/qaiot/qaiot.cpp \
    agreement/qProtocol/qfctp/qfctp.cpp \
    agreement/qProtocol/qprotocol.cpp \
    agreement/qProtocol/qprotocolmanager.cpp \
    agreement/qadb/qadb.cpp \
    agreement/qat/qat.cpp \
    agreement/qmomcozy/qproduct.cpp \
    agreement/qbulk/crc_md5.cpp \
    agreement/qbulk/qbulk.cpp \
    agreement/qjig/fixture_uart.cpp \
    agreement/qjig/qjig.cpp \
    agreement/qmes/bydmes.cpp \
    agreement/qmes/hqmes.cpp \
    agreement/qmes/jjmes.cpp \
    agreement/qmes/lxmes.cpp \
    agreement/qmes/mesmanager.cpp \
    agreement/qmes/qmes.cpp \
    agreement/qmes/wksmes.cpp \
    agreement/qmes/xwdmes.cpp \
    agreement/qmes/ydmmes.cpp \
    agreement/qProtocol/qpb/ble_protocol/fx_ble_msg.pb.c \
    agreement/qProtocol/qpb/ble_protocol/data_collection.pb.c \
    agreement/qProtocol/qpb/factory_protocol/factory_msg.pb.c \
    agreement/qProtocol/qpb/pb_common.c \
    agreement/qProtocol/qpb/pb_decode.c \
    agreement/qProtocol/qpb/pb_encode.c \
    agreement/qProtocol/qpb/qpb.cpp \
    agreement/qset/qsetting.cpp \
    agreement/qshell/qshell.cpp \
    agreement/qtuple/qtupleservice.cpp \
    agreement/qvisa/qvisa.cpp \
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
    work_station/freework/qfreework_data.cpp \
    work_station/freework/qfreeworkbox.cpp \
    work_station/freework/testFunction.cpp \
    agreement/qplc/inovance_h5u_modbus_tcp.cpp \
    work_station/imu/imubox.cpp \
    work_station/key/key_test.cpp \
    work_station/key/key_test_box.cpp \
    work_station/suction/suction.cpp \
    work_station/suction/suction_box.cpp \
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
    advance/imagewindow/myopenglwidget.h \
    agreement/qtransport/qchannel.h \
    agreement/qtransport/qmodbus_pdu.h \
    agreement/qtransport/qprocesschannel.h \
    agreement/qtransport/qserialportreader.h \
    agreement/qProtocol/qfctp/common_protocl/comm_protocol.h \
    agreement/qProtocol/qfctp/common_protocl/comm_protocol_builder.h \
    agreement/qProtocol/qfctp/common_protocl/comm_protocol_defs.h \
    agreement/qProtocol/qfctp/common_protocl/comm_protocol_parser.h \
    agreement/qProtocol/qaiot/qaiot.h \
    agreement/qProtocol/qfctp/qfctp.h \
    agreement/qProtocol/qprotocol.h \
    agreement/qProtocol/qprotocol_types.h \
    agreement/qProtocol/qprotocolmanager.h \
    agreement/qadb/qadb.h \
    agreement/qat/qat.h \
    agreement/qmomcozy/qproduct.h \
    agreement/qbulk/qbulk.h \
    agreement/qjig/fixture_uart.h \
    agreement/qjig/qjig.h \
    agreement/qmes/bydmes.h \
    agreement/qmes/hqmes.h \
    agreement/qmes/jjmes.h \
    agreement/qmes/lxmes.h \
    agreement/qmes/mesmanager.h \
    agreement/qmes/qmes.h \
    agreement/qmes/wksmes.h \
    agreement/qmes/xwdmes.h \
    agreement/qmes/ydmmes.h \
    agreement/qProtocol/qpb/ble_protocol/fx_ble_msg.pb.h \
    agreement/qProtocol/qpb/ble_protocol/data_collection.pb.h \
    agreement/qProtocol/qpb/factory_protocol/factory_msg.pb.h \
    agreement/qProtocol/qpb/pb.h \
    agreement/qProtocol/qpb/pb_common.h \
    agreement/qProtocol/qpb/pb_decode.h \
    agreement/qProtocol/qpb/pb_encode.h \
    agreement/qProtocol/qpb/qpb.h \
    agreement/qset/qsetting.h \
    agreement/qshell/qshell.h \
    agreement/qtuple/qtupleservice.h \
    agreement/qvisa/qvisa.h \
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
    agreement/qplc/inovance_h5u_modbus_tcp.h \
    work_station/imu/imubox.h \
    work_station/imu/imucali.h \
    work_station/key/key_test.h \
    work_station/key/key_test_box.h \
    work_station/suction/suction.h \
    work_station/suction/suction_box.h \
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
    work_station/key/key_test.ui \
    work_station/key/key_test_box.ui \
    work_station/suction/suction.ui \
    work_station/suction/suction_box.ui \
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

RC_ICONS = ./stytle/picture/lute.ico
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

win32 {
    # NI-VISA / IVI：依赖统一放在 lib/visa/，换电脑无需本机 IVI 安装路径。
    # 启用/关闭后须重新 qmake 并全量构建，以重建预编译头（AbIni.h → qusb.h → qvisa.h）。
    VISA_DIR = $$PWD/lib/visa
    exists($$VISA_DIR/visa.h):exists($$VISA_DIR/visatype.h):exists($$VISA_DIR/visa64.lib):exists($$VISA_DIR/visa64.dll):exists($$VISA_DIR/visaConfMgr.dll) {
        INCLUDEPATH += $$VISA_DIR
        LIBS += -L$$shell_path($$VISA_DIR) -lvisa64
        DEFINES += HAVE_NI_VISA
        VISA_DLLS = visa64.dll visaConfMgr.dll
        exists($$VISA_DIR/visaUtilities.dll) {
            VISA_DLLS += visaUtilities.dll
        }
        # 勿用 escape_expand(\\n\\t)：nmake 链接规则 @<< 后换行会破坏 Makefile
        VISA_POST_CMD =
        for(VISA_DLL, VISA_DLLS) {
            !isEmpty(VISA_POST_CMD): VISA_POST_CMD += " && "
            VISA_POST_CMD += copy /Y \"$$shell_path($$VISA_DIR/$$VISA_DLL)\" \"$$shell_path($$OUT_PWD/$$DESTDIR/$$VISA_DLL)\"
        }
        QMAKE_POST_LINK += $$quote(cmd /c $$VISA_POST_CMD)
        message("NI-VISA enabled from lib/visa.")
    } else {
        message("NI-VISA lib/visa incomplete — build without VISA. Required: visa.h, visatype.h, visa64.lib, visa64.dll, visaConfMgr.dll (optional: visaUtilities.dll)")
    }
}





DISTFILES += \
    agreement/qProtocol/qpb/ble_protocol/fx_ble_msg.proto \
    agreement/qProtocol/qpb/ble_protocol/data_collection.proto \
    agreement/qProtocol/qpb/ble_protocol/server_data.proto \
    agreement/qProtocol/qpb/factory_protocol/factory_msg.proto \
    lib/nfc/dcrf32.dll \
    lib/nfc/dcrf32.lib \
    new_production.qml

RESOURCES += \
    new_production.qrc



