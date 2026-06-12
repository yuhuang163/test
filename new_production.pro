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


INCLUDEPATH += agreement
INCLUDEPATH += agreement/qmes
INCLUDEPATH += agreement/qadb
INCLUDEPATH += agreement/qshell
INCLUDEPATH += agreement/qat
INCLUDEPATH += agreement/qtransport
INCLUDEPATH += agreement/factory_protocol/access
INCLUDEPATH += agreement/factory_protocol/manager
INCLUDEPATH += agreement/factory_protocol/codec/fctp
INCLUDEPATH += agreement/factory_protocol/protocol/qpb
INCLUDEPATH += agreement/factory_protocol/protocol/qfctp
INCLUDEPATH += agreement/factory_protocol/protocol/qaiot
INCLUDEPATH += agreement/factory_protocol/protocol/qroot
INCLUDEPATH += business/ble_ota
INCLUDEPATH += business/tuple
INCLUDEPATH += business/plc_v3_fixture
INCLUDEPATH += platform/settings
INCLUDEPATH += platform/settings/test_flow
INCLUDEPATH += platform/settings/widgets
INCLUDEPATH += platform/test_case \
    platform/test_case/manifest
INCLUDEPATH += platform/test_record
INCLUDEPATH += platform/log_upload
INCLUDEPATH += agreement/factory_protocol/protocol/qpb/ble_protocol
INCLUDEPATH += agreement/factory_protocol/protocol/qpb/factory_protocol
INCLUDEPATH += agreement/qusb
INCLUDEPATH += agreement/scpi/access
INCLUDEPATH += agreement/scpi/manager
INCLUDEPATH += agreement/fixture/access
INCLUDEPATH += agreement/fixture/device
INCLUDEPATH += agreement/fixture/manager
INCLUDEPATH += agreement/fixture/codec
INCLUDEPATH += agreement/qbrush
INCLUDEPATH += agreement/product_serial/protocol
INCLUDEPATH += agreement/adb
INCLUDEPATH += agreement/modbus/access
INCLUDEPATH += agreement/modbus/manager
INCLUDEPATH += agreement/modbus/codec
INCLUDEPATH += agreement/modbus/device/inovance_h5u_tcp
INCLUDEPATH += agreement/modbus/device/hq_ammeter_rtu
INCLUDEPATH += agreement/modbus/device/lx_ammeter_rtu
INCLUDEPATH += agreement/scpi/codec
INCLUDEPATH += agreement/scpi/device/huiling_wfp60h_scpi
INCLUDEPATH += agreement/scpi/device/rs_cmw100_scpi
INCLUDEPATH += business/cmw_gprf
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
INCLUDEPATH += platform/driver/serial
INCLUDEPATH += platform/driver/visa


# INCLUDEPATH += advance/xlsx
# DEPENDPATH  += advance/xlsx
# include(advance/xlsx/qtxlsx.pri)



SOURCES += \
    common/common_utils.cpp \
    common/serial_port_controller.cpp \
    platform/driver/serial/serial_channel.cpp \
    advance/demo/usmile_ring_buffer.cpp \
    advance/imagewindow/draggablecheckbox.cpp \
    advance/imagewindow/myopenglwidget.cpp \
    agreement/scpi/codec/scpi_line_codec.cpp \
    agreement/scpi/device/huiling_wfp60h_scpi/huiling_wfp60h_profile.cpp \
    agreement/modbus/access/modbus_device_catalog.cpp \
    agreement/modbus/manager/qmodbusmanager.cpp \
    agreement/modbus/codec/qmodbus_pdu.cpp \
    agreement/modbus/codec/qmodbus_rtu_codec.cpp \
    agreement/modbus/codec/qmodbus_rtu_rx_buffer.cpp \
    agreement/modbus/device/hq_ammeter_rtu/hq_ammeter_rtu.cpp \
    agreement/modbus/device/lx_ammeter_rtu/lx_ammeter_rtu.cpp \
    agreement/qtransport/qprocesschannel.cpp \
    agreement/factory_protocol/codec/fctp/comm_protocol_builder.cpp \
    agreement/factory_protocol/codec/fctp/comm_protocol_parser.cpp \
    agreement/factory_protocol/protocol/qaiot/qaiot.cpp \
    agreement/factory_protocol/protocol/qfctp/qfctp.cpp \
    agreement/factory_protocol/protocol/qroot/qroot.cpp \
    agreement/factory_protocol/access/qprotocol.cpp \
    agreement/factory_protocol/manager/qprotocolmanager.cpp \
    business/ble_ota/root_ble_ota.cpp \
    agreement/qadb/qadb.cpp \
    agreement/qat/qat.cpp \
    agreement/product_serial/protocol/qproduct.cpp \
    agreement/qbulk/crc_md5.cpp \
    agreement/qbulk/qbulk.cpp \
    platform/settings/widgets/fixture_uart.cpp \
    agreement/fixture/manager/qfixturemanager.cpp \
    agreement/fixture/device/fixture_camera_device.cpp \
    agreement/fixture/device/fixture_imu_device.cpp \
    agreement/fixture/device/fixture_pcba_device.cpp \
    agreement/fixture/device/fixture_press_device.cpp \
    agreement/fixture/device/qjig.cpp \
    agreement/qmes/bydmes.cpp \
    agreement/qmes/hqmes.cpp \
    agreement/qmes/hzmes.cpp \
    agreement/qmes/jjmes.cpp \
    agreement/qmes/lxmes.cpp \
    agreement/qmes/mesmanager.cpp \
    agreement/qmes/qmes.cpp \
    agreement/qmes/wksmes.cpp \
    agreement/qmes/xwdmes.cpp \
    agreement/qmes/ydmmes.cpp \
    agreement/factory_protocol/protocol/qpb/ble_protocol/fx_ble_msg.pb.c \
    agreement/factory_protocol/protocol/qpb/ble_protocol/data_collection.pb.c \
    agreement/factory_protocol/protocol/qpb/factory_protocol/factory_msg.pb.c \
    agreement/factory_protocol/protocol/qpb/pb_common.c \
    agreement/factory_protocol/protocol/qpb/pb_decode.c \
    agreement/factory_protocol/protocol/qpb/pb_encode.c \
    agreement/factory_protocol/protocol/qpb/qpb.cpp \
    platform/settings/qsetting.cpp \
    platform/settings/qsetting_bindings.cpp \
    platform/settings/test_flow/test_flow_editor.cpp \
    platform/settings/widgets/test_case_edit_dialog.cpp \
    platform/test_case/manifest/device_cmd_manifest.cpp \
    platform/test_case/manifest/dongle_cmd_manifest.cpp \
    platform/test_case/manifest/fixture_pcba_cmd_manifest.cpp \
    platform/test_case/manifest/product_serial_cmd_manifest.cpp \
    platform/test_case/manifest/modbus_cmd_manifest.cpp \
    platform/test_case/manifest/scpi_cmd_manifest.cpp \
    platform/test_case/manifest/tuple_cmd_manifest.cpp \
    platform/test_case/test_case.cpp \
    platform/test_record/test_record_store.cpp \
    platform/log_upload/log_upload_service.cpp \
    agreement/qshell/qshell.cpp \
    business/tuple/qtupleservice.cpp \
    business/cmw_gprf/cmw_gprf_facade.cpp \
    platform/driver/visa/visa_channel.cpp \
    agreement/scpi/manager/qscpivisasession.cpp \
    agreement/scpi/manager/qscpimanager.cpp \
    agreement/scpi/manager/qscpiserialsession.cpp \
    agreement/scpi/device/huiling_wfp60h_scpi/huiling_wfp60h_scpi_device.cpp \
    agreement/scpi/device/rs_cmw100_scpi/rs_cmw100_scpi_device.cpp \
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
    agreement/modbus/device/inovance_h5u_tcp/inovance_h5u_tcp.cpp \
    business/plc_v3_fixture/plc_v3_touch.cpp \
    business/plc_v3_fixture/plc_v3_facade.cpp \
    business/plc_v3_fixture/plc_v3_fixture.cpp \
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
    common/serial_port_controller.h \
    platform/driver/serial/serial_channel.h \
    advance/demo/usmile_ring_buffer.h \
    advance/imagewindow/draggablecheckbox.h \
    advance/imagewindow/myopenglwidget.h \
    agreement/modbus/access/modbus_types.h \
    agreement/modbus/access/modbus_device_catalog.h \
    agreement/modbus/manager/qmodbusmanager.h \
    agreement/modbus/codec/qmodbus_pdu.h \
    agreement/modbus/codec/qmodbus_rtu_codec.h \
    agreement/modbus/codec/qmodbus_rtu_rx_buffer.h \
    agreement/modbus/device/hq_ammeter_rtu/hq_ammeter_rtu.h \
    agreement/modbus/device/lx_ammeter_rtu/lx_ammeter_rtu.h \
    agreement/qtransport/qprocesschannel.h \
    agreement/factory_protocol/codec/fctp/comm_protocol.h \
    agreement/factory_protocol/codec/fctp/comm_protocol_builder.h \
    agreement/factory_protocol/codec/fctp/comm_protocol_defs.h \
    agreement/factory_protocol/codec/fctp/comm_protocol_parser.h \
    agreement/factory_protocol/protocol/qaiot/qaiot.h \
    agreement/factory_protocol/protocol/qfctp/qfctp.h \
    agreement/factory_protocol/protocol/qroot/qroot.h \
    agreement/factory_protocol/access/qprotocol.h \
    agreement/factory_protocol/access/qprotocol_types.h \
    agreement/factory_protocol/manager/qprotocolmanager.h \
    business/ble_ota/root_ble_ota.h \
    agreement/qadb/qadb.h \
    agreement/qat/qat.h \
    agreement/product_serial/protocol/qproduct.h \
    agreement/qbulk/qbulk.h \
    platform/settings/widgets/fixture_uart.h \
    agreement/fixture/manager/qfixturemanager.h \
    agreement/fixture/device/fixture_camera_device.h \
    agreement/fixture/device/fixture_imu_device.h \
    agreement/fixture/device/fixture_pcba_device.h \
    agreement/fixture/device/fixture_press_device.h \
    agreement/fixture/access/fixture_uart_types.h \
    agreement/fixture/device/qjig.h \
    agreement/qmes/bydmes.h \
    agreement/qmes/hqmes.h \
    agreement/qmes/hzmes.h \
    agreement/qmes/jjmes.h \
    agreement/qmes/lxmes.h \
    agreement/qmes/mesmanager.h \
    agreement/qmes/qmes.h \
    agreement/qmes/wksmes.h \
    agreement/qmes/xwdmes.h \
    agreement/qmes/ydmmes.h \
    agreement/factory_protocol/protocol/qpb/ble_protocol/fx_ble_msg.pb.h \
    agreement/factory_protocol/protocol/qpb/ble_protocol/data_collection.pb.h \
    agreement/factory_protocol/protocol/qpb/factory_protocol/factory_msg.pb.h \
    agreement/factory_protocol/protocol/qpb/pb.h \
    agreement/factory_protocol/protocol/qpb/pb_common.h \
    agreement/factory_protocol/protocol/qpb/pb_decode.h \
    agreement/factory_protocol/protocol/qpb/pb_encode.h \
    agreement/factory_protocol/protocol/qpb/qpb.h \
    platform/settings/qsetting.h \
    platform/settings/qsetting_bindings.h \
    platform/settings/test_flow/test_flow_editor.h \
    platform/settings/widgets/test_case_edit_dialog.h \
    platform/test_case/manifest/cmd_manifest_common.h \
    platform/test_case/manifest/device_cmd_manifest.h \
    platform/test_case/manifest/dongle_cmd_manifest.h \
    platform/test_case/manifest/fixture_pcba_cmd_manifest.h \
    platform/test_case/manifest/modbus_cmd_manifest.h \
    platform/test_case/manifest/scpi_cmd_manifest.h \
    platform/test_case/manifest/product_serial_cmd_manifest.h \
    platform/test_case/manifest/tuple_cmd_manifest.h \
    platform/test_case/test_case.h \
    platform/test_record/test_record_store.h \
    platform/log_upload/log_upload_service.h \
    platform/test_case/test_case_types.h \
    agreement/qshell/qshell.h \
    business/tuple/qtupleservice.h \
    business/cmw_gprf/cmw_gprf_facade.h \
    agreement/scpi/access/scpi_types.h \
    agreement/scpi/access/scpi_transport.h \
    agreement/scpi/access/iscpi_device.h \
    platform/driver/visa/visa_channel.h \
    agreement/scpi/manager/qscpivisasession.h \
    agreement/scpi/manager/qscpimanager.h \
    agreement/scpi/manager/qscpiserialsession.h \
    agreement/scpi/device/huiling_wfp60h_scpi/huiling_wfp60h_scpi_device.h \
    agreement/scpi/device/rs_cmw100_scpi/rs_cmw100_scpi_device.h \
    agreement/qusb/qusb_types.h \
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
    agreement/modbus/access/imodbus_rtu_device.h \
    agreement/modbus/device/inovance_h5u_tcp/inovance_h5u_tcp.h \
    agreement/scpi/codec/scpi_line_codec.h \
    agreement/scpi/device/huiling_wfp60h_scpi/huiling_wfp60h_profile.h \
    business/plc_v3_fixture/plc_v3_touch.h \
    business/plc_v3_fixture/plc_v3_facade.h \
    business/plc_v3_fixture/plc_v3_fixture.h \
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
    platform/settings/widgets/fixture_uart.ui \
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
    # 启用/关闭后须重新 qmake 并全量构建，以重建预编译头。
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
    agreement/factory_protocol/protocol/qpb/ble_protocol/fx_ble_msg.proto \
    agreement/factory_protocol/protocol/qpb/ble_protocol/data_collection.proto \
    agreement/factory_protocol/protocol/qpb/ble_protocol/server_data.proto \
    agreement/factory_protocol/protocol/qpb/factory_protocol/factory_msg.proto \
    lib/nfc/dcrf32.dll \
    lib/nfc/dcrf32.lib \
    new_production.qml

RESOURCES += \
    new_production.qrc
