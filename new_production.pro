QT       += core gui sql concurrent serialport printsupport network multimedia  qml quick widgets quickwidgets


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
# 总是删除with bom
# text codec for tools用local
# 语言用中文
# 项目编码规则  crlf  utf-8

# Qt Creator编写代码时界面经常卡死解决办法
# 帮助->关于插件->C+±>取消勾选ClangCodeModel（重启QtCreator）


# agreement 分层（工站/设备层仍用短名 #include，靠下列路径解析）
AGREEMENT_DIR = $$PWD/agreement
INCLUDEPATH += $$AGREEMENT_DIR
INCLUDEPATH += $$AGREEMENT_DIR/access
INCLUDEPATH += $$AGREEMENT_DIR/manager
INCLUDEPATH += $$AGREEMENT_DIR/transport
INCLUDEPATH += $$AGREEMENT_DIR/codec/fctp
INCLUDEPATH += $$AGREEMENT_DIR/codec/modbus
INCLUDEPATH += $$AGREEMENT_DIR/codec/fixture
INCLUDEPATH += $$AGREEMENT_DIR/service/mes
INCLUDEPATH += $$AGREEMENT_DIR/service/adb
INCLUDEPATH += $$AGREEMENT_DIR/service/shell
INCLUDEPATH += $$AGREEMENT_DIR/service/tuple
INCLUDEPATH += $$AGREEMENT_DIR/service/bulk
INCLUDEPATH += $$AGREEMENT_DIR/device/protocol/qfctp
INCLUDEPATH += $$AGREEMENT_DIR/device/protocol/qaiot
INCLUDEPATH += $$AGREEMENT_DIR/device/protocol/qpb
INCLUDEPATH += $$AGREEMENT_DIR/device/protocol/qpb/ble_protocol
INCLUDEPATH += $$AGREEMENT_DIR/device/protocol/qpb/factory_protocol
INCLUDEPATH += platform/settings
INCLUDEPATH += platform/settings/test_flow
INCLUDEPATH += platform/settings/widgets
INCLUDEPATH += platform/test_case \
    platform/test_case/manifest
INCLUDEPATH += platform/test_record
INCLUDEPATH += platform/log_upload
INCLUDEPATH += $$AGREEMENT_DIR/device/peripheral/qusb
INCLUDEPATH += $$AGREEMENT_DIR/device/peripheral/qjig
INCLUDEPATH += $$AGREEMENT_DIR/device/peripheral/qat
INCLUDEPATH += $$AGREEMENT_DIR/device/peripheral/fixture
INCLUDEPATH += $$AGREEMENT_DIR/device/modbus
INCLUDEPATH += $$AGREEMENT_DIR/device/product/qproduct
INCLUDEPATH += $$AGREEMENT_DIR/device/peripheral/qvisa
INCLUDEPATH += $$AGREEMENT_DIR/device/ble
INCLUDEPATH += advance/imagewindow
INCLUDEPATH += advance/demo
INCLUDEPATH += tools/factory_analyzer
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
INCLUDEPATH += common
# INCLUDEPATH += advance/xlsx
# DEPENDPATH  += advance/xlsx
# include(advance/xlsx/qtxlsx.pri)



SOURCES += \
    common/common_utils.cpp \
    agreement/transport/serial_channel.cpp \
    advance/demo/usmile_ring_buffer.cpp \
    advance/imagewindow/draggablecheckbox.cpp \
    advance/imagewindow/myopenglwidget.cpp \
    agreement/transport/qprocesschannel.cpp \
    agreement/codec/fctp/comm_protocol_builder.cpp \
    agreement/codec/fctp/comm_protocol_parser.cpp \
    agreement/codec/modbus/qmodbus_pdu.cpp \
    agreement/device/protocol/qaiot/qaiot.cpp \
    agreement/device/protocol/qfctp/qfctp.cpp \
    agreement/access/qprotocol.cpp \
    agreement/manager/qprotocolmanager.cpp \
    agreement/device/ble/root_ble_ota.cpp \
    agreement/service/adb/qadb.cpp \
    agreement/device/peripheral/qat/qat.cpp \
    agreement/device/product/qproduct/qproduct.cpp \
    agreement/service/bulk/crc_md5.cpp \
    agreement/service/bulk/qbulk.cpp \
    agreement/device/peripheral/fixture/fixture_uart.cpp \
    agreement/codec/fixture/fixture_camera_uart_protocol.cpp \
    agreement/codec/fixture/fixture_imu_uart_protocol.cpp \
    agreement/codec/fixture/fixture_pcba_uart_protocol.cpp \
    agreement/codec/fixture/fixture_press_uart_protocol.cpp \
    agreement/device/peripheral/qjig/qjig.cpp \
    agreement/service/mes/bydmes.cpp \
    agreement/service/mes/hqmes.cpp \
    agreement/service/mes/hzmes.cpp \
    agreement/service/mes/jjmes.cpp \
    agreement/service/mes/lxmes.cpp \
    agreement/service/mes/mesmanager.cpp \
    agreement/service/mes/qmes.cpp \
    agreement/service/mes/wksmes.cpp \
    agreement/service/mes/xwdmes.cpp \
    agreement/service/mes/ydmmes.cpp \
    agreement/device/protocol/qpb/ble_protocol/fx_ble_msg.pb.c \
    agreement/device/protocol/qpb/ble_protocol/data_collection.pb.c \
    agreement/device/protocol/qpb/factory_protocol/factory_msg.pb.c \
    agreement/device/protocol/qpb/pb_common.c \
    agreement/device/protocol/qpb/pb_decode.c \
    agreement/device/protocol/qpb/pb_encode.c \
    agreement/device/protocol/qpb/qpb.cpp \
    platform/settings/qsetting.cpp \
    platform/settings/qsetting_bindings.cpp \
    platform/settings/test_flow/test_flow_editor.cpp \
    platform/settings/widgets/test_case_edit_dialog.cpp \
    platform/test_case/manifest/device_cmd_manifest.cpp \
    platform/test_case/manifest/dongle_cmd_manifest.cpp \
    platform/test_case/manifest/fixture_pcba_cmd_manifest.cpp \
    platform/test_case/manifest/product_serial_cmd_manifest.cpp \
    platform/test_case/manifest/tuple_cmd_manifest.cpp \
    platform/test_case/test_case.cpp \
    platform/test_record/test_record_store.cpp \
    platform/log_upload/log_upload_service.cpp \
    agreement/service/shell/qshell.cpp \
    agreement/service/tuple/qtupleservice.cpp \
    agreement/device/peripheral/qvisa/qvisa.cpp \
    agreement/device/peripheral/qusb/qusb.cpp \
    tools/factory_analyzer/djitestfunction.cpp \
    tools/factory_analyzer/factory_analyzer.cpp \
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
    qlog/qlog_win.cpp \
    work_station/box_base.cpp \
    work_station/ageing/ageing.cpp \
    work_station/ageing/ageingbox.cpp \
    work_station/camera/camerabox.cpp \
    work_station/camera/cameratest.cpp \
    work_station/freework/qfreework.cpp \
    work_station/freework/qfreework_data.cpp \
    work_station/freework/qfreeworkbox.cpp \
    work_station/freework/qfreework_case_hooks.cpp \
    work_station/freework/qfreework_test_case.cpp \
    agreement/device/modbus/inovance_h5u_modbus_tcp.cpp \
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
    common/common_utils.h \
    agreement/transport/serial_channel.h \
    advance/demo/usmile_ring_buffer.h \
    advance/imagewindow/draggablecheckbox.h \
    advance/imagewindow/myopenglwidget.h \
    agreement/transport/qprocesschannel.h \
    agreement/codec/fctp/comm_protocol.h \
    agreement/codec/fctp/comm_protocol_builder.h \
    agreement/codec/fctp/comm_protocol_defs.h \
    agreement/codec/fctp/comm_protocol_parser.h \
    agreement/codec/modbus/qmodbus_pdu.h \
    agreement/device/protocol/qaiot/qaiot.h \
    agreement/device/protocol/qfctp/qfctp.h \
    agreement/access/qprotocol.h \
    agreement/access/qprotocol_types.h \
    agreement/manager/qprotocolmanager.h \
    agreement/device/ble/root_ble_ota.h \
    agreement/service/adb/qadb.h \
    agreement/device/peripheral/qat/qat.h \
    agreement/device/product/qproduct/qproduct.h \
    agreement/service/bulk/qbulk.h \
    agreement/device/peripheral/fixture/fixture_uart.h \
    agreement/codec/fixture/fixture_camera_uart_protocol.h \
    agreement/codec/fixture/fixture_imu_uart_protocol.h \
    agreement/codec/fixture/fixture_pcba_uart_protocol.h \
    agreement/codec/fixture/fixture_press_uart_protocol.h \
    agreement/codec/fixture/fixture_uart_types.h \
    agreement/device/peripheral/qjig/qjig.h \
    agreement/service/mes/bydmes.h \
    agreement/service/mes/hqmes.h \
    agreement/service/mes/hzmes.h \
    agreement/service/mes/jjmes.h \
    agreement/service/mes/lxmes.h \
    agreement/service/mes/mesmanager.h \
    agreement/service/mes/qmes.h \
    agreement/service/mes/wksmes.h \
    agreement/service/mes/xwdmes.h \
    agreement/service/mes/ydmmes.h \
    agreement/device/protocol/qpb/ble_protocol/fx_ble_msg.pb.h \
    agreement/device/protocol/qpb/ble_protocol/data_collection.pb.h \
    agreement/device/protocol/qpb/factory_protocol/factory_msg.pb.h \
    agreement/device/protocol/qpb/pb.h \
    agreement/device/protocol/qpb/pb_common.h \
    agreement/device/protocol/qpb/pb_decode.h \
    agreement/device/protocol/qpb/pb_encode.h \
    agreement/device/protocol/qpb/qpb.h \
    platform/settings/qsetting.h \
    platform/settings/qsetting_bindings.h \
    platform/settings/test_flow/test_flow_editor.h \
    platform/settings/widgets/test_case_edit_dialog.h \
    platform/test_case/manifest/cmd_manifest_common.h \
    platform/test_case/manifest/device_cmd_manifest.h \
    platform/test_case/manifest/dongle_cmd_manifest.h \
    platform/test_case/manifest/fixture_pcba_cmd_manifest.h \
    platform/test_case/manifest/product_serial_cmd_manifest.h \
    platform/test_case/manifest/tuple_cmd_manifest.h \
    platform/test_case/test_case.h \
    platform/test_record/test_record_store.h \
    platform/log_upload/log_upload_service.h \
    platform/test_case/test_case_types.h \
    agreement/service/shell/qshell.h \
    agreement/service/tuple/qtupleservice.h \
    agreement/device/peripheral/qvisa/qvisa.h \
    agreement/device/peripheral/qusb/qusb.h \
    tools/factory_analyzer/factory_analyzer.h \
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
    qlog/qlog_win.h \
    work_station/box_base.h \
    work_station/ageing/ageing.h \
    work_station/ageing/ageingbox.h \
    work_station/camera/camerabox.h \
    work_station/camera/cameratest.h \
    work_station/freework/qfreework.h \
    work_station/freework/qfreeworkbox.h \
    agreement/device/modbus/inovance_h5u_modbus_tcp.h \
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
    agreement/device/peripheral/fixture/fixture_uart.ui \
    platform/settings/qsetting.ui \
    platform/settings/widgets/test_case_edit_dialog.ui \
    tools/factory_analyzer/factory_analyzer.ui \
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
    # 启用/关闭后须重新 qmake 并全量构建，以重建预编译头（AbIni.h → qvisa.h 等）。
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
    agreement/device/protocol/qpb/ble_protocol/fx_ble_msg.proto \
    agreement/device/protocol/qpb/ble_protocol/data_collection.proto \
    agreement/device/protocol/qpb/ble_protocol/server_data.proto \
    agreement/device/protocol/qpb/factory_protocol/factory_msg.proto \
    lib/nfc/dcrf32.dll \
    lib/nfc/dcrf32.lib \
    new_production.qml

RESOURCES += \
    new_production.qrc
