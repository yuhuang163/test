#include "qpb.h"

#include <QDebug>
#include <iomanip>
#include <sstream>
#include <vector>
extern "C" {
#include "aes.h" // 引入 tiny-AES-c 的头文件
}
#include "pb.h"
#include "pb_decode.h"
#include "pb_encode.h"
#include "qcoreapplication.h"
#include "qdatetime.h"
#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

#define PACKET_NUM_OFFSET 0
#define APP_BLE_MAX_DATA_LEN 120
#define PACKET_NUM_LENGTH 1
#define PACKET_PAYLOAD_OFFSET 1
#define PACKET_CRC_LENGTH 1

#define MAX_PACKET_CNT 6
#define MAX_DATA_CNT (APP_BLE_MAX_DATA_LEN - PACKET_NUM_LENGTH) // first byte is packet number
#define MAX_DATA_CNT_FOR_PHONE (20 - PACKET_NUM_LENGTH)

Qpb::Qpb(QSerialPort* parent) : qProtocol(parent) {
    serialPort = parent;
    registerCommand();
}

void Qpb::set(DeviceCmd cmd, const QVariant& data) {
    switch (cmd) {
    case DeviceCmd::MotorCali:
        set_motor_cali(data.toInt());
        break;
    case DeviceCmd::WifiConnect: {
        if (data.canConvert<WifiConnectPayload>()) {
            const WifiConnectPayload payload = data.value<WifiConnectPayload>();
            set_connect_wifi(payload.name, payload.password);
            break;
        }
        if (data.canConvert<QVariantMap>()) {
            const QVariantMap wifiMap = data.toMap();
            const QByteArray name = wifiMap.value("name").toByteArray();
            const QByteArray password = wifiMap.value("password").toByteArray();
            set_connect_wifi(name, password);
            break;
        }
        const WifiInfo wifi = data.value<WifiInfo>();
        set_connect_wifi(wifi.wifi_name, wifi.wifi_password);
        break;
    }
    case DeviceCmd::CameraState:
        set_camera_state(data.toInt());
        break;
    case DeviceCmd::PressCollect:
        set_press_collect_param(static_cast<FacSwitch>(data.toInt()));
        break;
    case DeviceCmd::ImuCollect:
        set_imu_collect_param(static_cast<FacSwitch>(data.toInt()));
        break;
    case DeviceCmd::PressCaliResult:
        set_press_cali_result(data.value<press_calib_data_t>());
        break;
    case DeviceCmd::ImuCaliResult:
        set_imu_cali_result(data.value<ImuCalData>());
        break;
    case DeviceCmd::NewImuCaliResult:
        set_new_imu_cali_result(data.value<NewImuCalData>());
        break;
    case DeviceCmd::BaseInfo: {
        const QVariantList list = data.toList();
        if (list.size() >= 2) {
            set_base_info(static_cast<FacBasInfoType>(list.at(0).toInt()), list.at(1).value<FacGetDevBaseInfo>());
        }
        break;
    }
    case DeviceCmd::LocalOta: {
        if (data.canConvert<LocalOtaPayload>()) {
            const LocalOtaPayload payload = data.value<LocalOtaPayload>();
            set_local_ota(payload);
        } else {
            const QVariantList list = data.toList();
            if (list.size() >= 2) {
                set_local_ota(LocalOtaPayload{list.at(0).value<local_ota_data>(), list.at(1).value<local_ota_data>()});
            } else {
                qWarning() << "DeviceCmd::LocalOta expects LocalOtaPayload";
            }
        }
        break;
    }
    case DeviceCmd::StartOtaApp:
        set_start_ota_app(data.value<RotasFileStatusReq>());
        break;
    case DeviceCmd::StartMultiBleOtaApp: {
        if (data.canConvert<StartMultiBleOtaPayload>()) {
            const StartMultiBleOtaPayload payload = data.value<StartMultiBleOtaPayload>();
            set_start_multi_ble_ota_app(payload);
        } else {
            const QVariantList list = data.toList();
            if (list.size() >= 2) {
                set_start_multi_ble_ota_app(
                    StartMultiBleOtaPayload{list.at(0).value<RotasFileStatusReq>(), list.at(1).value<RotasFileStatusReq>()});
            } else {
                qWarning() << "DeviceCmd::StartMultiBleOtaApp expects StartMultiBleOtaPayload";
            }
        }
        break;
    }
    case DeviceCmd::ConfigNetworkApp:
        set_config_network_app(data.value<WifiInfo>());
        break;
    case DeviceCmd::MotorDampingState:
        set_motor_damping_state(data.toInt());
        break;
    case DeviceCmd::RgbColor:
        set_rgb_color(data.value<FacLedControl>());
        break;
    case DeviceCmd::LedColor: {
        const QVariantList list = data.toList();
        if (list.size() >= 2) {
            set_led_color(list.at(0).toInt(), list.at(1).toInt());
        }
        break;
    }
    case DeviceCmd::MotorTestState:
        set_motor_test_state(data.toInt());
        break;
    case DeviceCmd::MotorCaliState:
        set_motor_cali_state(data.toInt());
        break;
    case DeviceCmd::FacResult:
        set_fac_result(data.toInt());
        break;
    case DeviceCmd::ScreenColor:
        set_screen_color(data.toInt());
        break;
    case DeviceCmd::ShipMode:
        set_ship_mode(data.toInt());
        break;
    case DeviceCmd::MotorAdcSwitch:
        set_motor_adc_switch(data.toInt());
        break;
    case DeviceCmd::MotorState:
        set_motor_state(data.toInt());
        break;
    case DeviceCmd::MotorParam: {
        // data = QVariantList{fre, duty}
        const QVariantList list = data.toList();
        if (list.size() >= 2) {
            set_motor_param(list.at(0).toUInt(), list.at(1).toFloat());
        } else {
            qWarning() << "DeviceCmd::MotorParam expects QVariantList{fre, duty}";
        }
        break;
    }
    case DeviceCmd::MotorCaliResultParam:
        set_motor_cali_result_param(data.toUInt());
        break;
    case DeviceCmd::Music:
        set_music(data.toByteArray());
        break;
    case DeviceCmd::BurningMode: {
        if (data.canConvert<QVariantMap>()) {
            const QVariantMap map = data.toMap();
            const int mode = map.value("mode").toInt();
            if (mode <= 0) {
                qWarning() << "DeviceCmd::BurningMode map mode is invalid:" << mode;
                break;
            }
            const int switchValue = map.value("switch", map.value("enter", static_cast<int>(FacSwitch_OPEN))).toInt();
            const FacSwitch sw = static_cast<FacSwitch>(switchValue);
            set_burning_mode(mode, sw);
        } else {
            const QVariantList list = data.toList();
            if (list.size() >= 2) {
                set_burning_mode(list.at(0).toInt(), static_cast<FacSwitch>(list.at(1).toInt()));
            } else {
                qWarning() << "DeviceCmd::BurningMode expects QVariantMap{mode,switch?} or QVariantList{mode,switch}";
            }
        }
        break;
    }
    case DeviceCmd::BrushRecord:
        set_brush_record(data.value<FacSetBrushRecord>());
        break;
    case DeviceCmd::BrushTime:
        set_brush_time(data.toInt());
        break;
    case DeviceCmd::Sleep:
        set_sleeep(static_cast<FacSwitch>(data.toInt()));
        break;
    case DeviceCmd::ForbidSleep:
        set_forbid_sleep(static_cast<FacSwitch>(data.toInt()));
        break;
    case DeviceCmd::ScreenCameraState:
        set_screen_camera_state(data.toInt());
        break;
    case DeviceCmd::CameraLightState:
        set_camera_light_state(data.toInt());
        break;
    case DeviceCmd::CameraSupportState:
        set_camera_support_state(data.toInt());
        break;
    case DeviceCmd::CameraExposureTime:
        set_camera_exposure_time(data.toUInt());
        break;
    case DeviceCmd::DevReset:
        set_dev_reset();
        break;
    case DeviceCmd::BrushReset:
        set_brush_reset();
        break;
    case DeviceCmd::DeviceMode:
        set_device_mode(data.toInt());
        break;
    case DeviceCmd::BrushControl:
        set_brush_control(data.toInt());
        break;
    case DeviceCmd::FacMode:
        set_fac_mode(data.toInt());
        break;
    case DeviceCmd::CameraPictureState:
        set_camera_picture_state(data.toInt());
        break;
    case DeviceCmd::Sn: {
        if (data.canConvert<DeviceSnPayload>()) {
            const DeviceSnPayload payload = data.value<DeviceSnPayload>();
            set_sn(payload.which_sn, payload.sn);
        } else {
            const QVariantList list = data.toList();
            if (list.size() >= 2) {
                set_sn(static_cast<FacDevInfoType>(list.at(0).toInt()), list.at(1).toByteArray());
            }
        }
        break;
    }
    case DeviceCmd::IAmApp:
        set_i_am_app();
        break;
    case DeviceCmd::WifiDisconnect:
        set_wifi_disconnect();
        break;
    case DeviceCmd::ServoMotorInfo:
        set_servo_motor_info();
        break;
    case DeviceCmd::MicControl:
        set_mic_control(data.toInt());
        break;
    case DeviceCmd::UploadRecordData:
        set_upload_record_data(data.toInt());
        break;
    case DeviceCmd::SevorMotorParam: {
        if (data.canConvert<SevorMotorParamPayload>()) {
            const SevorMotorParamPayload payload = data.value<SevorMotorParamPayload>();
            set_sevor_motor_param(payload.sweeping_angle, payload.vibrate_angle, payload.sweeping_freq,
                                  payload.vibrate_freq);
        } else {
            const QVariantList list = data.toList();
            if (list.size() >= 4) {
                set_sevor_motor_param(list.at(0).toUInt(), list.at(1).toFloat(), list.at(2).toFloat(),
                                      list.at(3).toUInt());
            }
        }
        break;
    }
    case DeviceCmd::NewWifiConnect: {
        if (data.canConvert<NewWifiConnectPayload>()) {
            const NewWifiConnectPayload payload = data.value<NewWifiConnectPayload>();
            set_new_connect_wifi(payload.name, payload.password, payload.ip, payload.port);
        } else {
            const QVariantList list = data.toList();
            if (list.size() >= 4) {
                set_new_connect_wifi(list.at(0).toByteArray(), list.at(1).toByteArray(), list.at(2).toString(),
                                     list.at(3).toString());
            }
        }
        break;
    }
    default:
        qWarning() << "Unsupported set cmd:" << static_cast<int>(cmd);
        break;
    }
}

void Qpb::get(DeviceCmd cmd, const QVariant& param) {
    switch (cmd) {
    case DeviceCmd::GetBattery:
        get_battery();
        break;
    case DeviceCmd::BaseInfo:
        get_base_info();
        break;
    case DeviceCmd::ImuCaliResult:
    case DeviceCmd::GetImuCaliResult:
        get_imu_cali_result();
        break;
    case DeviceCmd::PressCaliResult:
    case DeviceCmd::GetPressCaliResult:
        get_press_cali_result();
        break;
    case DeviceCmd::DeviceInfo:
        get_device_info();
        break;
    case DeviceCmd::PeriphState:
        get_periph_state();
        break;
    case DeviceCmd::ConnectInfo:
        get_connect_info();
        break;
    case DeviceCmd::WifiInfo:
        get_wifi_info();
        break;
    case DeviceCmd::GetServoMotorInfo:
        get_servo_motor_info();
        break;
    case DeviceCmd::NowMusicInfo:
        get_now_music_info();
        break;
    case DeviceCmd::SdCardInfo:
        get_sd_card_info();
        break;
    case DeviceCmd::LightSensorInfo:
        get_light_sensor_info();
        break;
    case DeviceCmd::ButtonState:
        get_button_state(param.toInt());
        break;
    case DeviceCmd::Sn:
        get_sn(static_cast<FacDevInfoType>(param.toInt()));
        break;
    case DeviceCmd::BurshBacklog:
        get_bursh_backlog(param.toInt());
        break;
    default:
        qWarning() << "Unsupported get cmd:" << static_cast<int>(cmd);
        break;
    }
}
/*
 * 函数：aes256Decrypt
 * 说明：对输入数据使用 AES-256-CBC 模式进行解密，
 *       解密前要求数据长度为16字节的倍数，
 *       解密后会移除 PKCS#7 填充数据。
 *
 * 参数：
 *    encrypted - 待解密的加密数据（二进制）
 *
 * 返回值：
 *    解密后的原始数据；如果解密或填充校验失败，返回空 QByteArray。
 */
QByteArray Qpb::aes256Decrypt(const QByteArray& encrypted) {
    // qDebug() << "aes256Decrypt encrypted:" << encrypted.toHex().toUpper();

    static const uint8_t iv[16] = {0x51, 0x4C, 0xCA, 0x9B, 0xBC, 0x1A, 0x69, 0x16,
                                   0x24, 0x8E, 0x19, 0x59, 0xC5, 0xF9, 0xA6, 0x6F};
    uint8_t key[32] = {0x42, 0x93, 0x1A, 0x7D, 0xB6, 0xF9, 0x27, 0xCA, 0x4C, 0x13, 0xF2, 0x00, 0x19, 0x25, 0x79, 0x42,
                       0x6E, 0x1A, 0x84, 0xAE, 0xE6, 0x90, 0x4C, 0x4F, 0x9E, 0xE7, 0x40, 0xB0, 0xEB, 0xE5, 0xB6, 0xE3};

    // 加密数据长度必须为 16 字节的整数倍
    int encryptedLen = encrypted.size();
    if (encryptedLen % 16 != 0) {
        qWarning() << "加密数据长度不是16字节的倍数";
        return QByteArray();
    }

    // 拷贝一份数据用于就地解密
    QByteArray decrypted = encrypted;

    // 初始化 AES 上下文
    AES_ctx ctx;
    AES_init_ctx_iv(&ctx, key, iv);

    // 执行 CBC 模式解密（解密结果直接写入 decrypted）
    AES_CBC_decrypt_buffer(&ctx, reinterpret_cast<uint8_t*>(decrypted.data()), decrypted.size());

    // 获取最后一个字节作为填充字节数
    int padLen = static_cast<unsigned char>(decrypted.at(decrypted.size() - 1));
    // 校验填充是否合法：填充值应该在 1 到 16 之间
    if (padLen < 1 || padLen > 16) {
        qWarning() << "无效的填充值:" << padLen;
        return QByteArray();
    }
    // 校验最后 padLen 个字节是否均为 padLen
    for (int i = decrypted.size() - padLen; i < decrypted.size(); ++i) {
        if (static_cast<unsigned char>(decrypted.at(i)) != padLen) {
            qWarning() << "填充校验失败";
            return QByteArray();
        }
    }
    if (encrypted.size() == 240) {
        QByteArray lastBlock = decrypted.right(16);

        bool validPadding = std::all_of(lastBlock.begin(), lastBlock.end(), [](char c) { return c == 0x10; });
        qDebug() << "aes256Decrypt validPadding" << validPadding;
        if (!validPadding) {
            qDebug() << "解密后数据末尾填充错误，完整数据:" << lastBlock.toHex().toUpper();
        }
    }
    // 移除填充字节
    decrypted.chop(padLen);
    return decrypted;
}

/*
 * aes256Encrypt 函数与之前示例一致，用于加密数据（带 PKCS#7 补位）
 */
// QByteArray Qpb::aes256Encrypt(const QByteArray& input) {
//     static const uint8_t iv[16] = {0x51, 0x4C, 0xCA, 0x9B, 0xBC, 0x1A, 0x69, 0x16,
//                                    0x24, 0x8E, 0x19, 0x59, 0xC5, 0xF9, 0xA6, 0x6F};
//     uint8_t key[32] = {0x42, 0x93, 0x1A, 0x7D, 0xB6, 0xF9, 0x27, 0xCA, 0x4C, 0x13, 0xF2, 0x00, 0x19, 0x25, 0x79,
//     0x42,
//                        0x6E, 0x1A, 0x84, 0xAE, 0xE6, 0x90, 0x4C, 0x4F, 0x9E, 0xE7, 0x40, 0xB0, 0xEB, 0xE5, 0xB6,
//                        0xE3};

//     const int blockSize = 16;
//     int inputLen = input.size();
//     // 计算需要补位的字节数（PKCS#7 填充规则）
//     int padLen = blockSize - (inputLen % blockSize);
//     if (padLen == 0)
//         padLen = blockSize;  // 若刚好整数倍，则补一整块

//     // 对原始数据进行填充，每个补位字节的值为 padLen
//     QByteArray padded = input;
//     padded.append(QByteArray(padLen, char(padLen)));

//     // 输出加密结果（转换为大写十六进制字符串）
//     // qDebug() << "padded:" << padded.toHex().toUpper();

//     // 复制一份数据用于加密（加密函数就地操作）
//     QByteArray encrypted = padded;

//     // 初始化 AES 上下文
//     AES_ctx ctx;
//     AES_init_ctx_iv(&ctx, key, iv);

//     // 执行 CBC 模式加密
//     AES_CBC_encrypt_buffer(&ctx, reinterpret_cast<uint8_t*>(encrypted.data()), encrypted.size());

//     return encrypted;
// }
QByteArray Qpb::aes256Encrypt(const QByteArray& input) {
    static const uint8_t iv_init[16] = {0x51, 0x4C, 0xCA, 0x9B, 0xBC, 0x1A, 0x69, 0x16,
                                        0x24, 0x8E, 0x19, 0x59, 0xC5, 0xF9, 0xA6, 0x6F};
    uint8_t key[32] = {0x42, 0x93, 0x1A, 0x7D, 0xB6, 0xF9, 0x27, 0xCA, 0x4C, 0x13, 0xF2, 0x00, 0x19, 0x25, 0x79, 0x42,
                       0x6E, 0x1A, 0x84, 0xAE, 0xE6, 0x90, 0x4C, 0x4F, 0x9E, 0xE7, 0x40, 0xB0, 0xEB, 0xE5, 0xB6, 0xE3};

    const int blockSize = 16;
    int inputLen = input.size();
    // 计算需要补位的字节数（PKCS#7 填充规则）
    int padLen = blockSize - (inputLen % blockSize);

    if (padLen == 0)
        padLen = blockSize; // 若刚好整数倍，则补一整块

    if (padLen != 16)
        qDebug() << "padLen:" << padLen;

    // 对原始数据进行填充，每个补位字节的值为 padLen
    QByteArray padded = input;
    // padded.append(QByteArray(padLen, char(padLen)));
    padded.append(QByteArray(padLen, static_cast<char>(padLen))); // 确保补位字节正确
    // 输出加密结果（转换为大写十六进制字符串）
    // qDebug() << "padded:" << padded.toHex().toUpper();

    QByteArray encrypted(padded.size(), Qt::Uninitialized); // 避免QByteArray不连续
    memcpy(encrypted.data(), padded.constData(), padded.size());
    // 初始化 AES 上下文
    AES_ctx ctx;

    uint8_t iv[16];
    memcpy(iv, iv_init, 16); // 重新初始化 IV，防止被修改
    AES_init_ctx_iv(&ctx, key, iv);

    // 执行 CBC 模式加密
    AES_CBC_encrypt_buffer(&ctx, reinterpret_cast<uint8_t*>(encrypted.data()), encrypted.size());

    // 额外的解密验证逻辑
    if (input.size() == 224) {
        if (padLen != 16)
            qDebug() << "加密前的数据大小" << input.size() << "加密后数据大小" << encrypted.size();
        QByteArray lastBlock = padded.right(16);
        bool validPadding = std::all_of(lastBlock.begin(), lastBlock.end(), [](char c) { return c == 0x10; });
        qDebug() << "aes256Encrypt validPadding" << validPadding;
        if (!validPadding) {
            qDebug() << "加密前数据末尾填充错误，完整数据:" << lastBlock.toHex().toUpper();
        }
        aes256Decrypt(encrypted);
    }
    return encrypted;
}
uint16_t Qpb::calCrc16(const std::vector<uint8_t>& d) {
    unsigned short crc = 0xFFFF;

    for (uint32_t i = 0; i < d.size(); i++) {
        crc = (unsigned char)(crc >> 8) | (crc << 8);
        crc ^= d[i];
        crc ^= (unsigned char)(crc & 0xFF) >> 4;
        crc ^= (crc << 8) << 4;
        crc ^= ((crc & 0xFF) << 4) << 1;
    }

    return crc;
}

void Qpb::parseCmd(const QByteArray& byte) {
    std::vector<uint8_t> data(byte.begin(), byte.end());

    // const char* dataPtr = reinterpret_cast<const char*>(byte.data());
    // QString output;
    // for (int i = 0; i < byte.size(); ++i) {
    //     output += QString("%1 ").arg(static_cast<unsigned char>(dataPtr[i]), 2, 16, QLatin1Char('0'));
    // }
    // qDebug() << "收到的pb码为" << output.trimmed();

    for (auto x : data) {
        switch (state) {
        case STATE_IDLE:
            hitTimes = 0;
            if (x == 0xAA) {
                hitTimes = 0;
                ibuffer.clear();
                state = STATE_HEADER;
            }
            break;
        case STATE_HEADER:
            if (x == 0xAA) {
                hitTimes++;
            } else {
                state = STATE_IDLE;
            }
            if (hitTimes == 7) {
                state = STATE_CHANNEL;
            }
            break;

        case STATE_CHANNEL:

            pbChannel = (ext_ble_phy_channel_e)x;

            if (x > 3 || x == 0) {
                emitReport(QStringLiteral("ProtocolPbDate"), "通道出错，请更新dongle固件为1.3.3");

                state = STATE_IDLE;
                break;
            }
            state = STATE_LEN;

            break;

        case STATE_LEN:
            len = x - 1;

            state = STATE_PB_HEADER;
            break;

        case STATE_PB_HEADER: // 0就解包，非0就继续加东西
            state = x ? STATE_STAGE : STATE_UNPACK;
            break;

        case STATE_STAGE:
            ibuffer.push_back(x);
            if (ibuffer.size() == len) {
                ipack.insert(ipack.end(), ibuffer.begin(), ibuffer.end());
                state = STATE_IDLE;
            }
            break;

        case STATE_UNPACK:
            // qDebug() << "len" << len << ibuffer.size();
            if (ibuffer.size() == len - 1) {
                ipack.insert(ipack.end(), ibuffer.begin(), ibuffer.end());
                uint8_t crc16 = calCrc16(ipack);

                // qDebug() << "收到的crc16" << crc16 << x;

                if (crc16 == x) {
                    qDebug().noquote() << "PB RX:"
                                       << QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(ipack.data()),
                                                                         static_cast<int>(ipack.size()))
                                                                  .toHex(' ')
                                                                  .toUpper());
                    if (needAES) {
                        QByteArray byteArray(reinterpret_cast<const char*>(ipack.data()),
                                             static_cast<int>(ipack.size()));

                        // 调用解密函数，返回 QByteArray
                        QByteArray decrypted_data = aes256Decrypt(byteArray);

                        // 把解密后的 QByteArray 转换回 std::vector<uint8_t>
                        ipack.assign(decrypted_data.begin(), decrypted_data.end());
                    }

                    pb_istream_t istream = pb_istream_from_buffer(ipack.data(), ipack.size());
                    if (pbChannel == PHY_CHANNEL_FAC) {
                        if (pb_decode(&istream, FactoryDataPackage_fields, &recievePack)) {
                            // qDebug() << "command_id" << recievePack.cmd_id;

                            auto it = factoryCommandList.find(recievePack.cmd_id);
                            if (it != factoryCommandList.end()) {
                                it->second(recievePack);
                            } else {
                                qDebug() << "factory event not found , cmd id: " << recievePack.cmd_id;
                            }
                        } else {
                            qDebug() << "factory 解码失败原因：" << PB_GET_ERROR(&istream);
                        }
                    }

                    else if (pbChannel == PHY_CHANNEL_APP || pbChannel == PHY_CHANNEL_MAIN) {
                        if (pb_decode(&istream, DataPackage_fields, &blePack)) {
                            qDebug() << "blecommand_id" << blePack.command_id;

                            auto it = bleCommandList.find(blePack.command_id);
                            if (it != bleCommandList.end()) {
                                it->second(blePack);
                            } else {
                                qDebug() << "ble event not found , cmd id: " << blePack.command_id
                                         << blePack.which_command_data;
                            }
                        } else {
                            qDebug() << "ble 解码失败原因：" << PB_GET_ERROR(&istream);
                        }
                    } else {
                        emitReport(QStringLiteral("ProtocolPbDate"), "通道出错，请更新dongle固件为1.3.3");
                    }

                } else {
                    qDebug() << "pb协议的CRC校验失败";
                    qDebug() << "收到的crc16" << crc16 << x;
                }

                ipack.clear();
                state = STATE_IDLE;
            }
            ibuffer.push_back(x);
            break;

        default:
            break;
        }
    }
}

bool Qpb::sendCustomMessage(const QVariantMap& map) {
    Q_UNUSED(map);
    qWarning() << "Qpb 暂不支持 sendCustomMessage";
    return false;
}

void Qpb::sendMainPack(const DataPackage& pack) {
    std::vector<uint8_t> tx_buffer(1024);
    pb_ostream_t o_stream = pb_ostream_from_buffer(tx_buffer.data() + 1, tx_buffer.size() - 1);
    if (pb_encode(&o_stream, DataPackage_fields, &pack)) {
        size_t len = o_stream.bytes_written;
        if (len > tx_buffer.size() - 2) {
            qDebug() << "编码长度超出";
            return;
        }
        if (len == 0) {
            qDebug() << "编码长度等于0";
            return;
        }

        if (needAES) {
            // qDebug() << "笑容加发包前结果:"
            //          << QString(QByteArray::fromRawData(reinterpret_cast<const char*>(tx_buffer.data() + 1),
            //                                             static_cast<int>(len))
            //                         .toHex(' ')
            //                         .toUpper());
            // 只加密有效数据部分（去掉预留的第一个字节）
            QByteArray encrypted_data = aes256Encrypt(
                QByteArray::fromRawData(reinterpret_cast<const char*>(tx_buffer.data() + 1), static_cast<int>(len)));
            len = encrypted_data.size();
            // qDebug() << "笑容加加密结果:" << encrypted_data.toHex().toUpper();

            // 重新填充 tx_buffer，确保大小匹配（1 字节头 + 加密数据 + 1 字节 CRC）
            tx_buffer.resize(encrypted_data.size() + 2);

            // 保留预留字节
            tx_buffer[0] = 0;

            // 拷贝加密后的数据到 tx_buffer[1] 开始
            std::copy(encrypted_data.begin(), encrypted_data.end(), tx_buffer.begin() + 1);

            // 计算 CRC 并存放在最后一字节
            tx_buffer.back() = calCrc16(std::vector<uint8_t>(tx_buffer.begin() + 1, tx_buffer.end() - 1));
            // qDebug() << "笑容加封包结果:"
            //          << QString(QByteArray::fromRawData(reinterpret_cast<const char*>(tx_buffer.data()),
            //                                             static_cast<int>(tx_buffer.size()))
            //                         .toHex(' ')
            //                         .toUpper());

        } else {
            tx_buffer[0] = 0;
            tx_buffer[len + 1] = calCrc16(std::vector<uint8_t>(tx_buffer.begin() + 1, tx_buffer.begin() + len + 1));
        }

        std::vector<uint8_t> new_buffer;
        new_buffer.reserve(8 + 1 + 1 + len + 2);
        new_buffer.insert(new_buffer.begin(), 8, 0xcc);      //包头
        new_buffer.push_back(static_cast<uint8_t>(len + 2)); //长度
        new_buffer.push_back(PHY_CHANNEL_MAIN);              //通道
        new_buffer.insert(new_buffer.end(), tx_buffer.begin(), tx_buffer.begin() + len + 2);

        serialPort->write((char*)new_buffer.data(), new_buffer.size());
        serialPort->write((char*)new_buffer.data(), new_buffer.size());
        qDebug().noquote() << "PB TX:"
                           << QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(new_buffer.data()),
                                                             static_cast<int>(new_buffer.size()))
                                                      .toHex(' ')
                                                      .toUpper());

        const char* dataPtr = reinterpret_cast<const char*>(tx_buffer.data());
        QString output;
        for (int i = 0; i < len + 2; ++i) {
            output += QString("%1 ").arg(static_cast<unsigned char>(dataPtr[i]), 2, 16, QLatin1Char('0'));
        }
        qDebug() << "发出去的Mainpb码为" << output.trimmed();

        std::ostringstream oss;
        for (const auto& byte : new_buffer) {
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
        }

        // 使用 qDebug() 输出十六进制字符串
        QString hex_string = QString::fromStdString(oss.str());
        qDebug() << "发出去的Main新码为" << hex_string;

    } else {
        qDebug() << "短包编码失败原因：" << PB_GET_ERROR(&o_stream);
    }
}

void Qpb::sendShortPack(const DataPackage& pack) {
    std::vector<uint8_t> tx_buffer(1024);
    pb_ostream_t o_stream = pb_ostream_from_buffer(tx_buffer.data() + 1, tx_buffer.size() - 1);
    if (pb_encode(&o_stream, DataPackage_fields, &pack)) {
        size_t len = o_stream.bytes_written;
        if (len > tx_buffer.size() - 2) {
            qDebug() << "编码长度超出";
            return;
        }
        if (len == 0) {
            qDebug() << "编码长度等于0";
            return;
        }

        if (needAES) {
            // qDebug() << "笑容加发包前结果:"
            //          << QString(QByteArray::fromRawData(reinterpret_cast<const char*>(tx_buffer.data() + 1),
            //                                             static_cast<int>(len))
            //                         .toHex(' ')
            //                         .toUpper());
            // 只加密有效数据部分（去掉预留的第一个字节）
            QByteArray encrypted_data = aes256Encrypt(
                QByteArray::fromRawData(reinterpret_cast<const char*>(tx_buffer.data() + 1), static_cast<int>(len)));
            len = encrypted_data.size();
            // qDebug() << "笑容加加密结果:" << encrypted_data.toHex().toUpper();

            // 重新填充 tx_buffer，确保大小匹配（1 字节头 + 加密数据 + 1 字节 CRC）
            tx_buffer.resize(encrypted_data.size() + 2);

            // 保留预留字节
            tx_buffer[0] = 0;

            // 拷贝加密后的数据到 tx_buffer[1] 开始
            std::copy(encrypted_data.begin(), encrypted_data.end(), tx_buffer.begin() + 1);

            // 计算 CRC 并存放在最后一字节
            tx_buffer.back() = calCrc16(std::vector<uint8_t>(tx_buffer.begin() + 1, tx_buffer.end() - 1));
            // qDebug() << "笑容加封包结果:"
            //          << QString(QByteArray::fromRawData(reinterpret_cast<const char*>(tx_buffer.data()),
            //                                             static_cast<int>(tx_buffer.size()))
            //                         .toHex(' ')
            //                         .toUpper());

        } else {
            tx_buffer[0] = 0;
            tx_buffer[len + 1] = calCrc16(std::vector<uint8_t>(tx_buffer.begin() + 1, tx_buffer.begin() + len + 1));
        }

        std::vector<uint8_t> new_buffer;
        new_buffer.reserve(8 + 1 + 1 + len + 2);
        new_buffer.insert(new_buffer.begin(), 8, 0xcc);      //包头
        new_buffer.push_back(static_cast<uint8_t>(len + 2)); //长度
        new_buffer.push_back(PHY_CHANNEL_APP);               //通道
        new_buffer.insert(new_buffer.end(), tx_buffer.begin(), tx_buffer.begin() + len + 2);

        serialPort->write((char*)new_buffer.data(), new_buffer.size());
        qDebug().noquote() << "PB TX:"
                           << QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(new_buffer.data()),
                                                             static_cast<int>(new_buffer.size()))
                                                      .toHex(' ')
                                                      .toUpper());

        const char* dataPtr = reinterpret_cast<const char*>(tx_buffer.data());
        QString output;
        for (int i = 0; i < len + 2; ++i) {
            output += QString("%1 ").arg(static_cast<unsigned char>(dataPtr[i]), 2, 16, QLatin1Char('0'));
        }
        qDebug() << "发出去的笑容加pb码为" << output.trimmed();

        std::ostringstream oss;
        for (const auto& byte : new_buffer) {
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
        }

        // 使用 qDebug() 输出十六进制字符串
        QString hex_string = QString::fromStdString(oss.str());
        qDebug() << "发出去的笑容加新码为" << hex_string;

    } else {
        qDebug() << "短包编码失败原因：" << PB_GET_ERROR(&o_stream);
    }
}

void Qpb::sendShortPack(const FactoryDataPackage& pack) {
    // waitWork(20);
    std::vector<uint8_t> tx_buffer(1024);
    pb_ostream_t o_stream = pb_ostream_from_buffer(tx_buffer.data() + 1, tx_buffer.size() - 1);

    if (pb_encode(&o_stream, FactoryDataPackage_fields, &pack)) {
        size_t len = o_stream.bytes_written;
        qDebug() << "编码长度为" << len << "还需要加上2个字节";
        if (len > APP_BLE_MAX_DATA_LEN) {
            qDebug() << "编码长度超出" << APP_BLE_MAX_DATA_LEN << "目前为" << len;
            sendlongPack(pack);
            return;
        }
        if (len == 0) {
            qDebug() << "编码长度等于0";
            return;
        }
        tx_buffer[0] = 0;
        tx_buffer[len + 1] = calCrc16(std::vector<uint8_t>(tx_buffer.begin() + 1, tx_buffer.begin() + len + 1));

        std::vector<uint8_t> new_buffer;
        new_buffer.reserve(8 + 1 + 1 + len + 2);
        new_buffer.insert(new_buffer.begin(), 8, 0xcc);      //包头
        new_buffer.push_back(static_cast<uint8_t>(len + 2)); //长度
        new_buffer.push_back(PHY_CHANNEL_FAC);               //通道
        new_buffer.insert(new_buffer.end(), tx_buffer.begin(), tx_buffer.begin() + len + 2);

        serialPort->write((char*)new_buffer.data(), new_buffer.size());
        qDebug().noquote() << "PB TX:"
                           << QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(new_buffer.data()),
                                                             static_cast<int>(new_buffer.size()))
                                                      .toHex(' ')
                                                      .toUpper());

        /**************自我验证pb正常吗****************/
        /*   QByteArray dataToSend;
        // 在待发送的数据数组中添加8个0xAA
        for (int i = 0; i < 8; ++i) {
            dataToSend += 0xAA;
        }
        // 添加数据长度
        dataToSend += char(len + 2);//47
        // 添加tx_buffer中的数据
            for (int i=0;i<len + 2;i++)
        {
            dataToSend += tx_buffer[i];
        }
        qDebug() << "dataToSend.size" << dataToSend.size()<<tx_buffer[len + 1];
        parseCmd(dataToSend);

        */

        const char* dataPtr = reinterpret_cast<const char*>(tx_buffer.data());
        QString output;
        for (int i = 0; i < len + 2; ++i) {
            output += QString("%1 ").arg(static_cast<unsigned char>(dataPtr[i]), 2, 16, QLatin1Char('0'));
        }
        qDebug() << "发出去的工厂pb码为" << output.trimmed();

        std::ostringstream oss;
        for (const auto& byte : new_buffer) {
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
        }

        // 使用 qDebug() 输出十六进制字符串
        QString hex_string = QString::fromStdString(oss.str());
        qDebug() << "发出去的工厂新码为" << hex_string;

    } else {
        qDebug() << "短包工厂pb编码失败原因：" << PB_GET_ERROR(&o_stream);
    }
}

void Qpb::waitWork(int ms) {
    QTime t;
    t.start();
    while (t.elapsed() < ms)
        QCoreApplication::processEvents();
}

uint32_t Qpb::sendlongPack(const FactoryDataPackage& pack) {
    std::vector<uint8_t> tx_buffer(4096);
    std::vector<uint8_t> small_buffer(244);

    uint32_t ret = 0;                // 初始化返回值为0
    int max_data_cnt = MAX_DATA_CNT; // 定义每一包数据的最大长度
    int pkt_payload_len;             // 每个数据包的负载长度

    // 创建一个输出流用于protobuf编码
    pb_ostream_t o_stream = pb_ostream_from_buffer(tx_buffer.data() + 1, tx_buffer.size() - 1);
    size_t pkt_cnt;

    // 使用protobuf对pack进行编码
    if (pb_encode(&o_stream, FactoryDataPackage_fields, &pack)) {
        size_t length = o_stream.bytes_written; // 获取编码后的数据长度

        // 计算数据包的数量
        pkt_cnt = length / max_data_cnt;

        // 将数据包的编号和CRC校验码放入tx_buffer
        tx_buffer[0] = pkt_cnt;
        tx_buffer[length + 1] = calCrc16(std::vector<uint8_t>(tx_buffer.begin() + 1, tx_buffer.begin() + length + 1));

        // 计算最后一个数据包的负载长度
        pkt_payload_len = ((length % max_data_cnt) == 0) ? max_data_cnt : length % max_data_cnt;
        qDebug() << "包数量" << pkt_cnt << QString::number(tx_buffer[length + 1], 16);

        // 遍历每个数据包并发送
        for (int i = pkt_cnt; i >= 0; i--) {
            small_buffer[0] = i;
            qDebug() << "发送包头" << small_buffer[0];
            // 如果这是最后一个数据包并且还有剩余数据
            if (i == 0 && length > 0) {
                // 发送剩余的数据
                memcpy(&small_buffer[1], tx_buffer.data() + 1 + (pkt_cnt - i) * max_data_cnt, pkt_payload_len + 1);
                serialPort->write((char*)small_buffer.data(), pkt_payload_len + 2);
                qDebug().noquote() << "PB TX:"
                                   << QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(small_buffer.data()),
                                                                     pkt_payload_len + 2)
                                                              .toHex(' ')
                                                              .toUpper());
                qDebug() << "发送剩余包" << pkt_payload_len + 2;

                QByteArray byteArray(reinterpret_cast<const char*>(small_buffer.data()), pkt_payload_len + 2);
                qDebug() << "Data in hex:" << byteArray.toHex();
            } else {
                memcpy(&small_buffer[1], tx_buffer.data() + 1 + (pkt_cnt - i) * max_data_cnt, max_data_cnt);
                serialPort->write((char*)small_buffer.data(), max_data_cnt + 1);
                qDebug().noquote() << "PB TX:"
                                   << QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(small_buffer.data()),
                                                                     max_data_cnt + 1)
                                                              .toHex(' ')
                                                              .toUpper());
                qDebug() << "发送数据包" << max_data_cnt + 1;

                QByteArray byteArray(reinterpret_cast<const char*>(small_buffer.data()), max_data_cnt + 1);
                qDebug() << "Data in hex:" << byteArray.toHex();
            }
            waitWork(20);

            // 计算剩余的待发送数据长度
            length -= max_data_cnt;
        }
    } else {
        qDebug() << "长包编码失败原因：" << PB_GET_ERROR(&o_stream);
    }

    return ret; // 如果成功则返回0
}

void Qpb::get_device_info() {
    FactroyCmd cmd = FactroyCmd_GET_DEVICE_INFO;
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_get_dev_info_tag;

    sendShortPack(pack);
}

void Qpb::get_button_state(int state) {
    FactroyCmd cmd = FactroyCmd_GET_BUTTON_STATE;
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_get_button_state_tag;
    pack.command_data.get_button_state.button_state_count = 2;
    if (state) {
        pack.command_data.get_button_state.button_state[0].type = FacButtonType_POWER_BUTTON;
        pack.command_data.get_button_state.button_state[0].command_data.power_button.state = FacSwitch_START;
        pack.command_data.get_button_state.button_state[0].which_command_data = FacButtonItem_power_button_tag;

        pack.command_data.get_button_state.button_state[1].type = FacButtonType_MODEL_BUTTON;
        pack.command_data.get_button_state.button_state[1].command_data.power_button.state = FacSwitch_START;
        pack.command_data.get_button_state.button_state[1].which_command_data = FacButtonItem_mode_button_tag;
    } else {
        pack.command_data.get_button_state.button_state[0].type = FacButtonType_POWER_BUTTON;
        pack.command_data.get_button_state.button_state[0].command_data.power_button.state = FacSwitch_STOP;
        pack.command_data.get_button_state.button_state[0].which_command_data = FacButtonItem_power_button_tag;

        pack.command_data.get_button_state.button_state[1].type = FacButtonType_MODEL_BUTTON;
        pack.command_data.get_button_state.button_state[1].command_data.power_button.state = FacSwitch_STOP;
        pack.command_data.get_button_state.button_state[1].which_command_data = FacButtonItem_mode_button_tag;
    }

    sendShortPack(pack);
    qDebug() << "已发送按键状态" << state;
}
void Qpb::get_periph_state() {
    FactroyCmd cmd = FactroyCmd_GET_PERIPH_STATE;
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_get_periph_state_tag;

    sendShortPack(pack);
}
void Qpb::get_base_info() {
    FactroyCmd cmd = FactroyCmd_GET_DEVICE_BASE_INFO;
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_get_dev_base_info_tag;

    sendShortPack(pack);
    qDebug() << "已经发送获取设备信息";
}

// void Qpb::getFacMotorCalibResult()
// {
//     FactroyCmd cmd = FactroyCmd_UPLOAD_MOTORCALI_DATA;
//     FactoryDataPackage pack;
//     memset(&pack,0,sizeof(pack));
//     pack.cmd_id = cmd;
//     pack.which_command_data = FactoryDataPackage_upload_motorcali_data_tag;
//     pack.command_data.upload_motorcali_data.which_value_item =
//     FactoryDataPackage_upload_motorcali_data_tag;

//     sendShortPack(pack);
// }

void Qpb::set_device_mode(int mode) // 进入亮白模式
{
    FactroyCmd cmd = FactroyCmd_SET_DEVICE_INFO;
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_set_dev_info_tag;
    pack.command_data.set_dev_info.dev_info_count = 1;

    pack.command_data.set_dev_info.dev_info[0].info_item = FacDevInfoType_BRUSH_MODE;
    pack.command_data.set_dev_info.dev_info[0].value_item.brush_mode = mode;
    pack.command_data.set_dev_info.dev_info[0].which_value_item = FacDevInfoValue_brush_mode_tag;

    sendShortPack(pack);
}

void Qpb::set_camera_state(int state) // 开启摄像头
{
    FactroyCmd cmd = FactroyCmd_CAMERA_CONTROL;
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_camera_control_tag;
    pack.command_data.camera_control.type = FacCameraControlType_camera_state;
    pack.command_data.camera_control.which_value_item = FacCameraControl_camera_switch_tag;

    if (state == 1)
        pack.command_data.camera_control.value_item.camera_switch = FacSwitch_OPEN;
    if (state == 2)
        pack.command_data.camera_control.value_item.camera_switch = FacSwitch_CLOSE;

    sendShortPack(pack);
}
void Qpb::set_screen_camera_state(int state) // 开启屏幕摄像头
{
    FactroyCmd cmd = FactroyCmd_CAMERA_CONTROL;
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_camera_control_tag;
    pack.command_data.camera_control.type = FacCameraControlType_camera_display_on_screen;
    pack.command_data.camera_control.which_value_item = FacCameraControl_camera_switch_tag;

    if (state == 1)
        pack.command_data.camera_control.value_item.display_on_screen = FacSwitch_OPEN;
    if (state == 2)
        pack.command_data.camera_control.value_item.display_on_screen = FacSwitch_CLOSE;

    sendShortPack(pack);
}

void Qpb::set_camera_support_state(int state) // 开启补光灯
{
    FactroyCmd cmd = FactroyCmd_CAMERA_CONTROL;
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_camera_control_tag;
    pack.command_data.camera_control.which_value_item = FacCameraControl_camera_support_tag;
    pack.command_data.camera_control.type = FacCameraControlType_camera_support_state;
    if (state == 1)
        pack.command_data.camera_control.value_item.camera_support = FacSwitch_OPEN;
    if (state == 2)
        pack.command_data.camera_control.value_item.camera_support = FacSwitch_CLOSE;

    sendShortPack(pack);
}
void Qpb::set_camera_picture_state(int state) {
    FactroyCmd cmd = FactroyCmd_CAMERA_CONTROL;
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_camera_control_tag;
    pack.command_data.camera_control.which_value_item = FacCameraControl_get_picture_tag;
    pack.command_data.camera_control.type = FacCameraControlType_camera_get_picture;
    if (state == 1)
        pack.command_data.camera_control.value_item.get_picture = FacSwitch_OPEN;
    if (state == 2)
        pack.command_data.camera_control.value_item.get_picture = FacSwitch_CLOSE;

    sendShortPack(pack);
    qDebug() << "已发送请求图片";
}
void Qpb::set_camera_light_state(int state) // 开启补光灯
{
    FactroyCmd cmd = FactroyCmd_CAMERA_CONTROL;
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_camera_control_tag;
    pack.command_data.camera_control.which_value_item = FacCameraControl_light_switch_tag;
    pack.command_data.camera_control.type = FacCameraControlType_camera_light;
    if (state == 1)
        pack.command_data.camera_control.value_item.light_switch = FacSwitch_OPEN;
    if (state == 2)
        pack.command_data.camera_control.value_item.light_switch = FacSwitch_CLOSE;

    sendShortPack(pack);
}
void Qpb::set_camera_exposure_time(uint32_t time) {
    FactroyCmd cmd = FactroyCmd_CAMERA_CONTROL;
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_camera_control_tag;
    pack.command_data.camera_control.which_value_item = FacCameraControl_exposure_time_tag;
    pack.command_data.camera_control.value_item.exposure_time = time;
    pack.command_data.camera_control.type = FacCameraControlType_camera_exposure_time;
    sendShortPack(pack);
}
void Qpb::set_motor_cali_result_param(uint32_t data) {
    FactroyCmd cmd = FactroyCmd_MOTO_CONTROL;
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_moto_control_tag;
    pack.command_data.moto_control.type = FacMotoControlType_motor_cali_data;

    pack.command_data.moto_control.which_value_item = FacMotoControl_motor_cali_result_tag;
    pack.command_data.moto_control.value_item.motor_cali_result = data;

    sendShortPack(pack);
}
void Qpb::set_sevor_motor_param(uint32_t sweeping_angle, float vibrate_angle, float sweeping_freq,
                                uint32_t vibrate_freq) {
    FactroyCmd cmd = FactroyCmd_MOTO_CONTROL;
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_moto_control_tag;
    pack.command_data.moto_control.type = FacMotoControlType_servo_param;

    pack.command_data.moto_control.which_value_item = FacMotoControl_servoparam_tag;
    pack.command_data.moto_control.value_item.servoparam.sweeping_angle = sweeping_angle;
    pack.command_data.moto_control.value_item.servoparam.sweeping_freq = sweeping_freq;
    pack.command_data.moto_control.value_item.servoparam.vibrate_angle = vibrate_angle;
    pack.command_data.moto_control.value_item.servoparam.vibrate_freq = vibrate_freq;
    qDebug() << "发送的sweeping_angle" << pack.command_data.moto_control.value_item.servoparam.sweeping_angle;
    qDebug() << "发送的sweeping_freq" << pack.command_data.moto_control.value_item.servoparam.sweeping_freq;

    qDebug() << "发送的vibrate_angle" << pack.command_data.moto_control.value_item.servoparam.vibrate_angle;
    qDebug() << "发送的vibrate_freq" << pack.command_data.moto_control.value_item.servoparam.vibrate_freq;

    sendShortPack(pack);
}

void Qpb::get_battery() // 获取电量信息
{
    FactroyCmd cmd = FactroyCmd_GET_DEVICE_INFO;
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_get_dev_info_tag;
    pack.command_data.get_dev_info.dev_info_count = 1;

    pack.command_data.get_dev_info.dev_info[0].info_item = FacDevInfoType_BATTERY_INFO;

    pack.command_data.get_dev_info.dev_info[0].which_value_item = FacDevInfoValue_battery_tag;

    sendShortPack(pack);
}
void Qpb::set_battery(FacBatteryType type) // 设置电量信息
{
    FactroyCmd cmd = FactroyCmd_SET_DEVICE_INFO;
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_set_dev_info_tag;
    pack.command_data.get_dev_info.dev_info_count = 1;
    pack.command_data.get_dev_info.dev_info[0].info_item = FacDevInfoType_BATTERY_INFO;
    pack.command_data.get_dev_info.dev_info[0].which_value_item = FacDevInfoValue_battery_tag;
    pack.command_data.get_dev_info.dev_info[0].value_item.battery.battery_type = type;

    sendShortPack(pack);
}
void Qpb::set_motor_cali(int state) {
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    FactroyCmd cmd = FactroyCmd_MOTO_CONTROL;
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_moto_control_tag;
    pack.command_data.moto_control.type = FacMotoControlType_motor_cali;
    pack.command_data.moto_control.which_value_item = FacMotoControl_switch_cali_tag;
    if (state == 1)
        pack.command_data.moto_control.value_item.switch_cali = FacMotoCali_HALL_CALIBRATION;
    if (state == 2)
        pack.command_data.moto_control.value_item.switch_cali = FacMotoCali_ZERO_CALIBRATION;

    QString hostName = QSysInfo::machineHostName();
    if (hostName.length() > 10) {
        hostName = hostName.right(10);
    }
    hostName = "," + hostName;
    QString currentDate = QDateTime::currentDateTime().toString("yyMMddhhmmss") + hostName;
    // 格式化字符串

    QString infoString = QString("%1,%2.").arg(currentDate).arg(appVersion);
    // 转换为 QByteArray 并存入 write_info
    QByteArray byteArray = infoString.toUtf8();

    qstrncpy(pack.command_data.moto_control.write_info, byteArray.data(),
             sizeof(pack.command_data.moto_control.write_info) - 1);
    pack.command_data.moto_control.write_info[sizeof(pack.command_data.moto_control.write_info) - 1] = '\0';

    sendShortPack(pack);
    qDebug() << "已发送电机校准类型" << state << pack.command_data.moto_control.write_info << byteArray << infoString;
}
void Qpb::set_motor_cali_state(int state) {
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    FactroyCmd cmd = FactroyCmd_MOTO_CONTROL;
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_moto_control_tag;
    pack.command_data.moto_control.type = FacMotoControlType_motor_cali;
    pack.command_data.moto_control.which_value_item = FacMotoControl_switch_cali_tag;
    if (state)
        pack.command_data.moto_control.value_item.switch_cali = FacMotoCali_MOTOR_CALI_START;
    else
        pack.command_data.moto_control.value_item.switch_cali = FacMotoCali_MOTOR_CALI_STOP;

    sendShortPack(pack);
    qDebug() << "已发送电机校准流程" << state;
}

void Qpb::set_motor_test_state(int state) {
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    FactroyCmd cmd = FactroyCmd_MOTO_CONTROL;
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_moto_control_tag;
    pack.command_data.moto_control.type = FacMotoControlType_motor_test;
    pack.command_data.moto_control.which_value_item = FacMotoControl_motor_testing_tag;
    if (state == 1)
        pack.command_data.moto_control.value_item.motor_testing = FacSwitch_OPEN;
    else
        pack.command_data.moto_control.value_item.motor_testing = FacSwitch_CLOSE;

    sendShortPack(pack);
    qDebug() << "已发送电机测试状态" << state;
}
void Qpb::set_motor_damping_state(int state) {
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    FactroyCmd cmd = FactroyCmd_MOTO_CONTROL;
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_moto_control_tag;
    pack.command_data.moto_control.type = FacMotoControlType_damping_state;
    pack.command_data.moto_control.which_value_item = FacMotoControl_damping_switch_tag;
    if (state == 1)
        pack.command_data.moto_control.value_item.damping_switch = FacSwitch_OPEN;
    else
        pack.command_data.moto_control.value_item.damping_switch = FacSwitch_CLOSE;

    sendShortPack(pack);
    qDebug() << "已发送电机阻尼状态" << state;
}

void Qpb::set_wifi_disconnect() // 断开wifi
{
    FactroyCmd cmd = FactroyCmd_WIFI_DEMAND;
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_wifi_demand_tag;
    pack.command_data.wifi_demand.connect_command = 0;

    sendShortPack(pack);
}

void Qpb::set_new_connect_wifi(const QByteArray& name, const QByteArray& password, const QString& ip,
                               const QString& port) // 新的连接wifi带返回结果
{
    FactroyCmd cmd = FactroyCmd_WIFI_DEMAND;
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_wifi_demand_tag;
    pack.command_data.wifi_demand.connect_command = 1;
    pack.command_data.wifi_demand.has_wifi_info = 1;

    qstrncpy(pack.command_data.wifi_demand.wifi_info.wifi_name, name,
             sizeof(pack.command_data.wifi_demand.wifi_info.wifi_name));
    qstrncpy(pack.command_data.wifi_demand.wifi_info.wifi_password, password,
             sizeof(pack.command_data.wifi_demand.wifi_info.wifi_password));

    FacWifiInfo_ip_address_t ip_address;
    // 将QString类型的IP地址转换为FacWifiInfo_ip_address_t类型
    QStringList ipParts = ip.split(".");

    if (ipParts.size() == 4) {
        ip_address.size = 4;
        ip_address.bytes[0] = ipParts[0].toInt();
        ip_address.bytes[1] = ipParts[1].toInt();
        ip_address.bytes[2] = ipParts[2].toInt();
        ip_address.bytes[3] = ipParts[3].toInt();
    }

    pack.command_data.wifi_demand.wifi_info.ip_address = ip_address;
    pack.command_data.wifi_demand.wifi_info.port = port.toUInt();
    sendShortPack(pack);
    qDebug() << "已发送连接wifi";
}

void Qpb::get_servo_motor_info() {
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));

    pack.cmd_id = FactroyCmd_MOTO_CONTROL;
    pack.which_command_data = FactoryDataPackage_moto_control_tag;
    pack.command_data.moto_control.type = FacMotoControlType_get_servo_info_data;
    pack.command_data.moto_control.which_value_item = FacMotoControlType_get_servo_info_data;
    sendShortPack(pack);
    qDebug() << "开始获取电机内容";
}

void Qpb::get_now_music_info() {
    FactroyCmd cmd = FactroyCmd_GET_DEVICE_INFO;
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_get_dev_info_tag;
    pack.command_data.get_dev_info.dev_info_count = 1;

    pack.command_data.get_dev_info.dev_info[0].info_item = FacDevInfoType_MUSIC_STATE;

    pack.command_data.get_dev_info.dev_info[0].which_value_item = FacDevInfoValue_music_state_tag;

    sendShortPack(pack);
}
void Qpb::get_light_sensor_info() {
    FactroyCmd cmd = FactroyCmd_GET_DEVICE_INFO;
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_get_dev_info_tag;
    pack.command_data.get_dev_info.dev_info_count = 1;

    pack.command_data.get_dev_info.dev_info[0].info_item = FacDevInfoType_LIGHT_SENSOR;

    pack.command_data.get_dev_info.dev_info[0].which_value_item = FacDevInfoValue_light_sensor_tag;

    sendShortPack(pack);
}
void Qpb::get_sd_card_info() {
    FactroyCmd cmd = FactroyCmd_GET_DEVICE_INFO;
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_get_dev_info_tag;
    pack.command_data.get_dev_info.dev_info_count = 1;

    pack.command_data.get_dev_info.dev_info[0].info_item = FacDevInfoType_SD_CARD;

    pack.command_data.get_dev_info.dev_info[0].which_value_item = FacDevInfoValue_sdcard_tag;

    sendShortPack(pack);
}
//调试用，没啥用
void Qpb::set_servo_motor_info() {
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));

    pack.cmd_id = FactroyCmd_UPLOAD_MOTORCALI_DATA;
    pack.command_data.upload_motorcali_data.type = FacMotorUploadType_SERVO_INFO;
    pack.command_data.upload_motorcali_data.which_value_item = FacMotorCalibResult_servo_info_tag;
    pack.command_data.upload_motorcali_data.value_item.servo_info.has_opera_info = 1;
    pack.command_data.upload_motorcali_data.value_item.servo_info.motor_current = 255;
    pack.command_data.upload_motorcali_data.value_item.servo_info.motor_voltage = 255;

    strcpy(pack.command_data.upload_motorcali_data.value_item.servo_info.opera_info.hall_info,
           "呵呵呵呵呵呵呵呵呵呵呵呵呵呵呵");
    strcpy(pack.command_data.upload_motorcali_data.value_item.servo_info.opera_info.zero_info,
           "呵呵呵呵呵呵呵呵呵呵呵呵呵呵呵");
    pack.which_command_data = FactoryDataPackage_upload_motorcali_data_tag;
    sendShortPack(pack);
    qDebug() << "发送电机内容";
}

void Qpb::get_wifi_info() {
    FactroyCmd cmd = FactroyCmd_GET_DEVICE_INFO;
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_get_dev_info_tag;
    pack.command_data.get_dev_info.dev_info_count = 1;

    pack.command_data.get_dev_info.dev_info[0].info_item = FacDevInfoType_WIFI_INFO;

    pack.command_data.get_dev_info.dev_info[0].which_value_item = FacDevInfoValue_wifi_info_tag;

    sendShortPack(pack);
}
void Qpb::set_brush_control(int state) // 开始暂停使用
{
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    FactroyCmd cmd = FactroyCmd_BRUSH_CONTROL;

    if (state) {
        pack.cmd_id = cmd;
        pack.which_command_data = FactoryDataPackage_brush_control_tag;
        pack.command_data.brush_control.which_value_item = FacBrushControl_brush_start_tag;
        pack.command_data.brush_control.value_item.brush_start = FacSwitch_START;
        pack.command_data.brush_control.type = FacBrushControlType_BRUSH_START;
    }

    else {
        pack.cmd_id = cmd;
        pack.which_command_data = FactoryDataPackage_brush_control_tag;
        pack.command_data.brush_control.which_value_item = FacBrushControl_brush_start_tag;
        pack.command_data.brush_control.value_item.brush_start = FacSwitch_STOP;
        pack.command_data.brush_control.type = FacBrushControlType_BRUSH_START;
    }

    sendShortPack(pack);
}

void Qpb::set_fac_mode(int state) // 开始暂停工厂二维码
{
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    FactroyCmd cmd = FactroyCmd_SET_DEVICE_STATE;
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_set_dev_state_tag;

    if (state) {
        pack.command_data.set_dev_state.dev_state_type = DevStateType_FACTORY_QRCORD;
        pack.command_data.set_dev_state.state = FacSwitch_OPEN;
    }

    else {
        pack.command_data.set_dev_state.dev_state_type = DevStateType_FACTORY_QRCORD;
        pack.command_data.set_dev_state.state = FacSwitch_CLOSE;
    }

    sendShortPack(pack);
    qDebug() << "已发送工厂模式状态" << state;
}
void Qpb::set_fac_result(int state) {
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    FactroyCmd cmd = FactroyCmd_SET_DEVICE_INFO;
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_set_dev_info_tag;
    pack.command_data.set_dev_info.dev_info_count = 1;
    pack.command_data.set_dev_info.dev_info[0].info_item = FacDevInfoType_IF_QUALIFIED;
    pack.command_data.set_dev_info.dev_info[0].which_value_item = FacDevInfoValue_if_qualified_tag;

    if (state) {
        pack.command_data.set_dev_info.dev_info[0].value_item.if_qualified = FacSwitch_START;
    } else {
        pack.command_data.set_dev_info.dev_info[0].value_item.if_qualified = FacSwitch_STOP;
    }

    sendShortPack(pack);
    qDebug() << "已发送工厂测试状态" << state;
}

void Qpb::set_base_info(FacBasInfoType which_info, const FacGetDevBaseInfo& data) {
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));

    FactroyCmd cmd = FactroyCmd_SET_DEVICE_BASE_INFO;
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_set_dev_base_info_tag;
    if (which_info == FacBasInfoType_HW_VERSION) {
        pack.command_data.set_dev_base_info.basinfo_item = which_info;

        pack.command_data.set_dev_base_info.which_value_item = FacSetDevBaseInfo_hw_version_tag;
        // 假设 hw_version 是一个字符数组，并且 data.hw_version 是以 null 终止的字符串
        qstrncpy(pack.command_data.set_dev_base_info.value_item.hw_version, data.hw_version,
                 sizeof(pack.command_data.set_dev_base_info.value_item.hw_version) - 1);
        // 确保目标数组以 null 终止
        pack.command_data.set_dev_base_info.value_item
            .hw_version[sizeof(pack.command_data.set_dev_base_info.value_item.hw_version) - 1] = '\0';
    }
    sendShortPack(pack);
    qDebug() << "已发送固件版本号" << data.hw_version;
}

void Qpb::set_sn(FacDevInfoType which_sn, const QByteArray& sn) {
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    // 获取当前日期
    QString hostName = QSysInfo::machineHostName();
    if (hostName.length() > 10) {
        hostName = hostName.right(10);
    }
    hostName = "," + hostName;

    QString currentDate = QDateTime::currentDateTime().toString("yyMMddhhmmss") + hostName;
    // 格式化字符串

    QString infoString = QString("%1,%2").arg(currentDate).arg(appVersion);

    FactroyCmd cmd = FactroyCmd_SET_DEVICE_INFO;
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_set_dev_info_tag;
    pack.command_data.set_dev_info.dev_info_count = 1;

    if (which_sn == FacDevInfoType_BOARD_SN) {
        pack.command_data.set_dev_info.dev_info[0].info_item = which_sn;
        qstrncpy(pack.command_data.set_dev_info.dev_info[0].value_item.board_sn, sn,
                 sizeof(pack.command_data.set_dev_info.dev_info[0].value_item.board_sn) - 1);
        pack.command_data.set_dev_info.dev_info[0]
            .value_item.board_sn[sizeof(pack.command_data.set_dev_info.dev_info[0].value_item.board_sn) - 1] = '\0';
        pack.command_data.set_dev_info.dev_info[0].which_value_item = FacDevInfoValue_board_sn_tag;
    } else if (which_sn == FacDevInfoType_TAIL_SN) {
        pack.command_data.set_dev_info.dev_info[0].info_item = which_sn;
        qstrncpy(pack.command_data.set_dev_info.dev_info[0].value_item.product_sn, sn,
                 sizeof(pack.command_data.set_dev_info.dev_info[0].value_item.product_sn) - 1);
        pack.command_data.set_dev_info.dev_info[0]
            .value_item.product_sn[sizeof(pack.command_data.set_dev_info.dev_info[0].value_item.product_sn) - 1] = '\0';
        pack.command_data.set_dev_info.dev_info[0].which_value_item = FacDevInfoValue_tail_sn_tag;

    } else if (which_sn == FacDevInfoType_SUB_PID) {
        pack.command_data.set_dev_info.dev_info[0].info_item = which_sn;
        qstrncpy(pack.command_data.set_dev_info.dev_info[0].value_item.sub_pid, sn,
                 sizeof(pack.command_data.set_dev_info.dev_info[0].value_item.sub_pid) - 1);
        pack.command_data.set_dev_info.dev_info[0]
            .value_item.sub_pid[sizeof(pack.command_data.set_dev_info.dev_info[0].value_item.sub_pid) - 1] = '\0';
        pack.command_data.set_dev_info.dev_info[0].which_value_item = FacDevInfoValue_sub_pid_tag;
    } else if (which_sn == FacDevInfoType_SKUID) {
        pack.command_data.set_dev_info.dev_info[0].info_item = which_sn;
        qstrncpy(pack.command_data.set_dev_info.dev_info[0].value_item.sku_id, sn,
                 sizeof(pack.command_data.set_dev_info.dev_info[0].value_item.sku_id) - 1);
        pack.command_data.set_dev_info.dev_info[0]
            .value_item.sub_pid[sizeof(pack.command_data.set_dev_info.dev_info[0].value_item.sku_id) - 1] = '\0';
        pack.command_data.set_dev_info.dev_info[0].which_value_item = FacDevInfoValue_sku_id_tag;
    } else {
        // 处理未知的 which_sn 值，你可以抛出错误或者采取其他适当的措施
        qDebug() << "未知的 which_sn 值";
        return;
    }
    // 转换为 QByteArray 并存入 write_info
    QByteArray byteArray = infoString.toUtf8();
    // qstrncpy(pack.command_data.set_dev_info.write_info, byteArray.data(),
    //          sizeof(pack.command_data.set_dev_info.write_info) - 1);
    // pack.command_data.set_dev_info.write_info[sizeof(pack.command_data.set_dev_info.write_info) - 1] = '\0';

    qstrncpy(pack.command_data.set_dev_info.dev_info[0].write_info, byteArray.data(),
             sizeof(pack.command_data.set_dev_info.dev_info[0].write_info) - 1);
    pack.command_data.set_dev_info.dev_info[0]
        .write_info[sizeof(pack.command_data.set_dev_info.dev_info[0].write_info) - 1] = '\0';

    sendShortPack(pack);
    qDebug() << "已发送sn码" << pack.command_data.set_dev_info.dev_info[0].write_info << sn;
}

void Qpb::get_sn(FacDevInfoType which_sn) {
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));

    FactroyCmd cmd = FactroyCmd_GET_DEVICE_INFO;
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_get_dev_info_tag;
    pack.command_data.set_dev_info.dev_info_count = 1;
    if (which_sn == FacDevInfoType_BOARD_SN) {
        pack.command_data.set_dev_info.dev_info[0].info_item = which_sn;
        pack.command_data.set_dev_info.dev_info[0].which_value_item = FacDevInfoValue_board_sn_tag;
    } else if (which_sn == FacDevInfoType_TAIL_SN) {
        pack.command_data.set_dev_info.dev_info[0].info_item = which_sn;
        pack.command_data.set_dev_info.dev_info[0].which_value_item = FacDevInfoValue_tail_sn_tag;
    } else if (which_sn == FacDevInfoType_SUB_PID) {
        pack.command_data.set_dev_info.dev_info[0].info_item = which_sn;
        pack.command_data.set_dev_info.dev_info[0].which_value_item = FacDevInfoValue_sub_pid_tag;
    } else if (which_sn == FacDevInfoType_SKUID) {
        pack.command_data.set_dev_info.dev_info[0].info_item = which_sn;
        pack.command_data.set_dev_info.dev_info[0].which_value_item = FacDevInfoValue_sku_id_tag;
    } else {
        // 处理未知的 which_sn 值，你可以抛出错误或者采取其他适当的措施
        qDebug() << "未知的 which_sn 值";
        return;
    }
    qDebug() << "主动获取sn" << which_sn;
    sendShortPack(pack);
}

void Qpb::set_press_sensor_temp(int state) {
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    FactroyCmd cmd = FactroyCmd_SET_DEVICE_STATE;
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_set_dev_state_tag;
    pack.command_data.set_dev_state.dev_state_type = DevStateType_PRESS_SENSOR_TEMP;

    if (state) {
        pack.command_data.set_dev_state.state = FacSwitch_START;
    } else {
        pack.command_data.set_dev_state.state = FacSwitch_STOP;
    }

    sendShortPack(pack);
    qDebug() << "已发送压感温度补偿状态";
}

void Qpb::set_uart_receive(int state) {
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    FactroyCmd cmd = FactroyCmd_SET_DEVICE_STATE;
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_set_dev_state_tag;
    pack.command_data.set_dev_state.dev_state_type = DevStateType_UART_RECEIVE;

    if (state) {
        pack.command_data.set_dev_state.state = FacSwitch_START;
    } else {
        pack.command_data.set_dev_state.state = FacSwitch_STOP;
    }

    sendShortPack(pack);
    qDebug() << "已发送串口状态";
}

void Qpb::set_motor_adc_switch(int state) {
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    FactroyCmd cmd = FactroyCmd_SET_DEVICE_STATE;
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_set_dev_state_tag;
    pack.command_data.set_dev_state.dev_state_type = DevStateType_MOTOR_ADC_MOS;

    if (state) {
        pack.command_data.set_dev_state.state = FacSwitch_START;
    } else {
        pack.command_data.set_dev_state.state = FacSwitch_STOP;
    }

    sendShortPack(pack);
    qDebug() << "已发送船运";
}
void Qpb::get_bursh_backlog(int state) {
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    FactroyCmd cmd = FactroyCmd_FAC_LOG;
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_fac_log_tag;

    if (state) {
        pack.command_data.fac_log.switch_send_log = FacSwitch_START;
    } else {
        pack.command_data.fac_log.switch_send_log = FacSwitch_STOP;
    }

    sendShortPack(pack);
    qDebug() << "已发送获取设备黑盒日志";
}

void Qpb::set_mic_control(int state) {
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    FactroyCmd cmd = FactroyCmd_MIC_CONTROL;
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_mic_control_tag;

    if (state) {
        pack.command_data.mic_control.switch_record = FacSwitch_START;
    } else {
        pack.command_data.mic_control.switch_record = FacSwitch_STOP;
    }

    sendShortPack(pack);
    qDebug() << "已发送设置麦克风录制" << state;
}
void Qpb::set_upload_record_data(int state) {
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    FactroyCmd cmd = FactroyCmd_MIC_CONTROL;
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_mic_control_tag;

    if (state) {
        pack.command_data.mic_control.upload_record_data = FacSwitch_START;
    } else {
        pack.command_data.mic_control.upload_record_data = FacSwitch_STOP;
    }

    sendShortPack(pack);
    qDebug() << "已发送设置麦克风记录数据上报状态" << state;
}
void Qpb::set_ship_mode(int state) {
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    FactroyCmd cmd = FactroyCmd_SET_DEVICE_STATE;
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_set_dev_state_tag;
    pack.command_data.set_dev_state.dev_state_type = DevStateType_SHIP;

    if (state) {
        pack.command_data.set_dev_state.state = FacSwitch_START;
    } else {
        pack.command_data.set_dev_state.state = FacSwitch_STOP;
    }

    sendShortPack(pack);
    qDebug() << "已发送船运";
}
// 旧wifi接口
void Qpb::set_connect_wifi(const QByteArray& name, const QByteArray& password) {
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));

    FactroyCmd cmd = FactroyCmd_SET_DEVICE_INFO;
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_set_dev_info_tag;
    pack.command_data.set_dev_info.dev_info_count = 1;
    pack.command_data.set_dev_info.dev_info[0].info_item = FacDevInfoType_WIFI_INFO;
    qstrncpy(pack.command_data.set_dev_info.dev_info[0].value_item.wifi_info.wifi_name, name,
             sizeof(pack.command_data.set_dev_info.dev_info[0].value_item.wifi_info.wifi_name));
    qstrncpy(pack.command_data.set_dev_info.dev_info[0].value_item.wifi_info.wifi_password, password,
             sizeof(pack.command_data.set_dev_info.dev_info[0].value_item.wifi_info.wifi_password));
    pack.command_data.set_dev_info.dev_info[0].which_value_item = FacDevInfoValue_wifi_info_tag;
    sendShortPack(pack);
    qDebug() << "已发送连接wifi";
}

void Qpb::set_local_ota(const LocalOtaPayload& payload) {
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));

    FactroyCmd cmd = FactroyCmd_INTERNET_OTA;
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_internet_ota_tag;

    if (payload.file0.is_have_data && payload.file1.is_have_data)
        pack.command_data.internet_ota.file_info_count = 2;
    else
        pack.command_data.internet_ota.file_info_count = 1;

    pack.command_data.internet_ota.file_info[0].file_size = payload.file0.file_size;
    pack.command_data.internet_ota.file_info[0].version_code = payload.file0.version_code;
    pack.command_data.internet_ota.file_info[0].version_type = payload.file0.version_type;
    qstrncpy(pack.command_data.internet_ota.file_info[0].md5, payload.file0.md5,
             sizeof(pack.command_data.internet_ota.file_info[0].md5));
    qstrncpy(pack.command_data.internet_ota.file_info[0].url, payload.file0.url,
             sizeof(pack.command_data.internet_ota.file_info[0].url));

    pack.command_data.internet_ota.file_info[1].file_size = payload.file1.file_size;
    pack.command_data.internet_ota.file_info[1].version_code = payload.file1.version_code;
    pack.command_data.internet_ota.file_info[1].version_type = payload.file1.version_type;
    qstrncpy(pack.command_data.internet_ota.file_info[1].md5, payload.file1.md5,
             sizeof(pack.command_data.internet_ota.file_info[1].md5));
    qstrncpy(pack.command_data.internet_ota.file_info[1].url, payload.file1.url,
             sizeof(pack.command_data.internet_ota.file_info[1].url));

    sendShortPack(pack);
    qDebug() << "已发送本地ota";
}

void Qpb::set_music(const QByteArray& data) {
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    FactroyCmd cmd = FactroyCmd_MOTO_CONTROL;
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_moto_control_tag;
    pack.command_data.moto_control.type = FacMotoControlType_play_file;
    pack.command_data.moto_control.which_value_item = FacMotoControl_file_name_tag;

    // 使用 QByteArray 提供的安全方式进行字符串拷贝
    qstrncpy(pack.command_data.moto_control.value_item.file_name, data.constData(),
             sizeof(pack.command_data.moto_control.value_item.file_name));

    sendShortPack(pack);
    qDebug() << "已发送音乐" << data.constData();
}
void Qpb::set_motor_param(uint32_t fre, float duty) {
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));

    pack.cmd_id = FactroyCmd_MOTO_CONTROL;
    pack.which_command_data = FactoryDataPackage_moto_control_tag;
    pack.command_data.moto_control.type = FacMotoControlType_motor_param;
    pack.command_data.moto_control.which_value_item = FacMotoControl_param_tag;
    pack.command_data.moto_control.value_item.param.freq = fre;
    pack.command_data.moto_control.value_item.param.gain_or_duty = duty;

    sendShortPack(pack);
    qDebug() << "已发送电机参数";
}
void Qpb::set_motor_state(int state) {
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    FactroyCmd cmd = FactroyCmd_MOTO_CONTROL;
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_moto_control_tag;
    pack.command_data.moto_control.type = FacMotoControlType_motor_state;
    pack.command_data.moto_control.which_value_item = FacMotoControl_switch_state_tag;
    if (state)
        pack.command_data.moto_control.value_item.switch_state = FacSwitch_OPEN;
    else
        pack.command_data.moto_control.value_item.switch_state = FacSwitch_CLOSE;

    sendShortPack(pack);
    qDebug() << "已发送电机状态";
}

void Qpb::set_burning_mode(int state, FacSwitch switchs) {
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    FactroyCmd cmd = FactroyCmd_SET_AGEING_TEST;
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_ageing_tag;

    switch (state) {
    case 1:
        pack.command_data.ageing.type = FacAgeingTestType_AGEING_1;
        break;
    case 2:
        pack.command_data.ageing.type = FacAgeingTestType_AGEING_2;
        break;
    case 3:
        pack.command_data.ageing.type = FacAgeingTestType_AGEING_3;
        break;
    case 4:
        pack.command_data.ageing.type = FacAgeingTestType_AGEING_4;
        break;
    case 5:
        pack.command_data.ageing.type = FacAgeingTestType_AGEING_PRODUCTION_1;
        break;
    default:
        break;
    }
    pack.command_data.ageing.switch_state = switchs;

    sendShortPack(pack);
    qDebug() << "已发送老化" << state;
}
void Qpb::set_brush_record(const FacSetBrushRecord& record) {
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    FactroyCmd cmd = FactroyCmd_SET_BRUSH_RECORD;
    pack.cmd_id = cmd;
    pack.command_data.set_brush_record = record;
    pack.which_command_data = FactoryDataPackage_set_brush_record_tag;
    sendShortPack(pack);
}

void Qpb::set_brush_time(int timestamp) {
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    FactroyCmd cmd = FactroyCmd_SET_TIME;
    pack.cmd_id = cmd;
    pack.command_data.set_time.timestamp = timestamp;
    pack.command_data.set_time.timezone = 32;
    pack.which_command_data = FactoryDataPackage_set_time_tag;
    sendShortPack(pack);
}

// 开始灯颜色控制
void Qpb::set_led_color(int state, int lednumber) {
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    FactroyCmd cmd = FactroyCmd_LED_CONTROL;
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_led_control_tag;
    pack.command_data.led_control.which_led = lednumber;
    if (state) {
        pack.command_data.led_control.switch_state = FacSwitch_OPEN;
        qDebug() << "已发送led亮";
    } else {
        pack.command_data.led_control.switch_state = FacSwitch_CLOSE;
        qDebug() << "已发送led灭";
    }

    sendShortPack(pack);
}

// 开始灯颜色控制
void Qpb::set_rgb_color(FacLedControl data) {
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    FactroyCmd cmd = FactroyCmd_LED_CONTROL;
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_led_control_tag;
    pack.command_data.led_control.switch_state = FacSwitch_OPEN;
    pack.command_data.led_control.led_state_count = 4;

    pack.command_data.led_control.led_state[0].R = data.led_state[0].R;
    pack.command_data.led_control.led_state[0].G = data.led_state[0].G;
    pack.command_data.led_control.led_state[0].B = data.led_state[0].B;
    pack.command_data.led_control.led_state[0].index = data.led_state[0].index;

    pack.command_data.led_control.led_state[1].R = data.led_state[1].R;
    pack.command_data.led_control.led_state[1].G = data.led_state[1].G;
    pack.command_data.led_control.led_state[1].B = data.led_state[1].B;
    pack.command_data.led_control.led_state[1].index = data.led_state[1].index;

    pack.command_data.led_control.led_state[2].R = data.led_state[2].R;
    pack.command_data.led_control.led_state[2].G = data.led_state[2].G;
    pack.command_data.led_control.led_state[2].B = data.led_state[2].B;
    pack.command_data.led_control.led_state[2].index = data.led_state[2].index;

    pack.command_data.led_control.led_state[3].R = data.led_state[3].R;
    pack.command_data.led_control.led_state[3].G = data.led_state[3].G;
    pack.command_data.led_control.led_state[3].B = data.led_state[3].B;
    pack.command_data.led_control.led_state[3].index = data.led_state[3].index;

    sendShortPack(pack);

    qDebug() << "已发送灯光个数" << pack.command_data.led_control.led_state_count;
}

// 开始屏幕颜色
void Qpb::set_screen_color(int state) {
    FactoryDataPackage pack;
    int color[5] = {0xff, 0xff00, 0xff0000, 0xffffff, 0x000000};
    memset(&pack, 0, sizeof(pack));
    FactroyCmd cmd = FactroyCmd_LCD_CONTROL;
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_lcd_control_tag;
    pack.command_data.lcd_control.type = FacLcdControlType_color;
    pack.command_data.lcd_control.which_value_item = FacLcdControl_color_tag;
    pack.command_data.lcd_control.value_item.color = color[state];
    sendShortPack(pack);
    qDebug() << "已发送屏幕颜色" << state;
}
void Qpb::set_forbid_sleep(FacSwitch state) // 进入禁止休眠状态
{
    FactroyCmd cmd = FactroyCmd_SET_DEVICE_STATE;
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_set_dev_state_tag;
    pack.command_data.set_dev_state.dev_state_type = DevStateType_FORBID_SLEEP;
    pack.command_data.set_dev_state.state = state;
    qDebug() << "已发送静止休眠1";
    sendShortPack(pack);
    qDebug() << "已发送静止休眠3";
}
void Qpb::set_sleeep(FacSwitch state) // 进入休眠状态
{
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    FactroyCmd cmd = FactroyCmd_SET_DEVICE_STATE;
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_set_dev_state_tag;

    if (state) {
        pack.command_data.set_dev_state.dev_state_type = DevStateType_SLEEP;
        pack.command_data.set_dev_state.state = FacSwitch_OPEN;
    }

    else {
        pack.command_data.set_dev_state.dev_state_type = DevStateType_SLEEP;
        pack.command_data.set_dev_state.state = FacSwitch_CLOSE;
    }

    sendShortPack(pack);
}

void Qpb::set_dev_reset() // 复位设备
{
    FactroyCmd cmd = FactroyCmd_SET_DEVICE_STATE;
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_set_dev_state_tag;
    pack.command_data.set_dev_state.dev_state_type = DevStateType_REBOOT;
    pack.command_data.set_dev_state.state = FacSwitch_OPEN;

    sendShortPack(pack);
    qDebug() << "已复位设备";
}

void Qpb::set_brush_reset() // 复位设备
{
    FactroyCmd cmd = FactroyCmd_SET_DEVICE_STATE;
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_set_dev_state_tag;
    pack.command_data.set_dev_state.dev_state_type = DevStateType_RESET;
    pack.command_data.set_dev_state.state = FacSwitch_OPEN;

    sendShortPack(pack);
    qDebug() << "已重置设备";
}

void Qpb::set_press_collect_param(FacSwitch sta) // 设置压感采集开关
{
    FactroyCmd cmd = FactroyCmd_SET_COLLECT_PARAM;
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_set_collect_param_tag;
    pack.command_data.set_collect_param.param_count = 1;
    pack.command_data.set_collect_param.param[0].type = DataCollectType_PRESSURE_SENSOR;
    pack.command_data.set_collect_param.param[0].which_command_data = FacDataCollectItem_pressure_sensor_tag;
    pack.command_data.set_collect_param.param[0].command_data.pressure_sensor.state = sta;
    sendShortPack(pack);
    qDebug() << "设置压感采集" << sta;
}
void Qpb::set_imu_collect_param(FacSwitch sta) // 设置imu采集开关
{
    FactroyCmd cmd = FactroyCmd_SET_COLLECT_PARAM;
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_set_collect_param_tag;
    pack.command_data.set_collect_param.param_count = 1;
    pack.command_data.set_collect_param.param[0].type = DataCollectType_NINE_AXLE;
    pack.command_data.set_collect_param.param[0].which_command_data = FacDataCollectItem_nine_axle_tag;
    pack.command_data.set_collect_param.param[0].command_data.nine_axle.state = sta;
    pack.command_data.set_collect_param.param[0].command_data.nine_axle.sampling_rate = 25;

    sendShortPack(pack);
    qDebug() << "设置imu采集开关" << sta;
}
void Qpb::set_solve_imu_collect_param(FacSwitch sta) // 设置imu采集开关
{
    FactroyCmd cmd = FactroyCmd_SET_COLLECT_PARAM;
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_set_collect_param_tag;
    pack.command_data.set_collect_param.param_count = 1;
    pack.command_data.set_collect_param.param[0].type = DataCollectType_NINE_AXLE_SOLVE;
    pack.command_data.set_collect_param.param[0].which_command_data = FacDataCollectItem_nine_axle_solve_tag;
    pack.command_data.set_collect_param.param[0].command_data.nine_axle.state = sta;
    pack.command_data.set_collect_param.param[0].command_data.nine_axle.sampling_rate = 50;

    sendShortPack(pack);
    qDebug() << "设置处理后的imu采集开关" << sta;
}

void Qpb::set_camera_fault_data_packet(int count, const QVector<int>& data) // 发送校准结果
{
    qDebug() << "错误个数" << count;
    FactroyCmd cmd = FactroyCmd_UPLOAD_PICTURE_DATA;
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));

    if (count > sizeof(pack.command_data.upload_picture_data.fault_data_packet) / sizeof(uint32_t)) {
        emitReport(QStringLiteral("ProtocolPbDate"), "丢包过多" + QString::number(count));
        count = 50;
    }

    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_upload_picture_data_tag;
    pack.command_data.upload_picture_data.fault_data_packet_count = count;

    for (int i = 0; i < count && i < data.size(); i++) {
        pack.command_data.upload_picture_data.fault_data_packet[i] = data[i];
    }

    sendShortPack(pack);

    emitReport(QStringLiteral("ProtocolPbDate"), "成功发送摄像头错误数据包个数" + QString::number(count));
}

void Qpb::set_imu_cali_result(ImuCalData cali_ok) // 发送校准结果
{
    FactroyCmd cmd = FactroyCmd_SET_IMU_CALIB;
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_set_imu_calib_tag;

    pack.command_data.set_imu_calib.gyro_x = cali_ok.gyro_offset[0];
    pack.command_data.set_imu_calib.gyro_y = cali_ok.gyro_offset[1];
    pack.command_data.set_imu_calib.gyro_z = cali_ok.gyro_offset[2];

    sendShortPack(pack);
    qDebug() << "已发送六轴校准结果" << pack.command_data.set_imu_calib.gyro_x;
    qDebug() << "已发送六轴校准结果" << pack.command_data.set_imu_calib.gyro_y;
    qDebug() << "已发送六轴校准结果" << pack.command_data.set_imu_calib.gyro_z;
}
void Qpb::set_new_imu_cali_result(NewImuCalData cali_ok) {
    FactroyCmd cmd = FactroyCmd_SET_IMU_CALIB;
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_set_imu_calib_tag;
    pack.command_data.set_imu_calib.gyro_x = cali_ok.gyro_offset[0];
    pack.command_data.set_imu_calib.gyro_y = cali_ok.gyro_offset[1];
    pack.command_data.set_imu_calib.gyro_z = cali_ok.gyro_offset[2];

    pack.command_data.set_imu_calib.has_new_cali = 1;
    // Fill the calibration data into the package
    pack.command_data.set_imu_calib.new_cali.kx = cali_ok.kx;
    pack.command_data.set_imu_calib.new_cali.ky = cali_ok.ky;
    pack.command_data.set_imu_calib.new_cali.kz = cali_ok.kz;
    pack.command_data.set_imu_calib.new_cali.syx = cali_ok.syx;
    pack.command_data.set_imu_calib.new_cali.szx = cali_ok.szx;
    pack.command_data.set_imu_calib.new_cali.szy = cali_ok.szy;
    pack.command_data.set_imu_calib.new_cali.bx = cali_ok.bx;
    pack.command_data.set_imu_calib.new_cali.by = cali_ok.by;
    pack.command_data.set_imu_calib.new_cali.bz = cali_ok.bz;

    // Send the package
    sendShortPack(pack);

    // Debugging output
    qDebug() << "已发送六轴校准结果: "
             << " kx:" << pack.command_data.set_imu_calib.new_cali.kx
             << " ky:" << pack.command_data.set_imu_calib.new_cali.ky
             << " kz:" << pack.command_data.set_imu_calib.new_cali.kz
             << " syx:" << pack.command_data.set_imu_calib.new_cali.syx
             << " szx:" << pack.command_data.set_imu_calib.new_cali.szx
             << " szy:" << pack.command_data.set_imu_calib.new_cali.szy
             << " bx:" << pack.command_data.set_imu_calib.new_cali.bx
             << " by:" << pack.command_data.set_imu_calib.new_cali.by
             << " bz:" << pack.command_data.set_imu_calib.new_cali.bz;
}

void Qpb::set_press_cali_result(press_calib_data_t cali_result) //发送校准结果
{
    FactroyCmd cmd = FactroyCmd_SET_PRESS_SENSOR_CALIB;
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_set_fsensor_calib_tag;

    pack.command_data.set_fsensor_calib.brush_head_adc = (uint32_t)cali_result.calib_factor[MODULE_BTH];
    pack.command_data.set_fsensor_calib.mode_button_adc = (uint32_t)cali_result.calib_factor[MODULE_MODE_BUTTON];
    pack.command_data.set_fsensor_calib.power_button_adc = (uint32_t)cali_result.calib_factor[MODULE_POWER_BUTTON];

    if (cali_result.temperature[MODULE_BTH] != 0) {
        pack.command_data.set_fsensor_calib.temperature = (uint32_t)cali_result.temperature[MODULE_BTH];
    } else if (cali_result.temperature[MODULE_MODE_BUTTON] != 0) {
        pack.command_data.set_fsensor_calib.temperature = (uint32_t)cali_result.temperature[MODULE_MODE_BUTTON];
    }
    pack.command_data.set_fsensor_calib.assistant_component =
        (uint32_t)cali_result.calib_factor[MODULE_ASSISTANT_COMPONENT];

    sendShortPack(pack);
    qDebug() << "已发送压感校准结果";
    qDebug() << "电机校准值 (brush_head_adc):" << pack.command_data.set_fsensor_calib.brush_head_adc;
    qDebug() << "模式按键校准值 (mode_button_adc):" << pack.command_data.set_fsensor_calib.mode_button_adc;
    qDebug() << "电源按键校准值 (power_button_adc):" << pack.command_data.set_fsensor_calib.power_button_adc;
    qDebug() << "电机温度校准值 (temperature):" << pack.command_data.set_fsensor_calib.temperature;
    qDebug() << "模式按键温度校准值 (temperature):" << pack.command_data.set_fsensor_calib.temperature;
    qDebug() << "辅助元件校准值 (assistant_component):" << pack.command_data.set_fsensor_calib.assistant_component;
}

void Qpb::get_press_cali_result() // 获取校准结果
{
    FactroyCmd cmd = FactroyCmd_GET_PRESS_SENSOR_CALIB;
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_get_fsensor_calib_tag;
    sendShortPack(pack);
    qDebug() << "已获取校准结果";
}
void Qpb::get_imu_cali_result() // 获取校准结果
{
    FactroyCmd cmd = FactroyCmd_GET_IMU_CALIB;
    FactoryDataPackage pack;
    memset(&pack, 0, sizeof(pack));
    pack.cmd_id = cmd;
    pack.which_command_data = FactoryDataPackage_get_imu_calib_tag;
    sendShortPack(pack);
    qDebug() << "已开始获取imu校准结果";
}

void Qpb::registerCommand() {
    factoryCommandList[FactroyCmd_SET_COLLECT_PARAM] =
        std::bind(&Qpb::process_FactroyCmd_SET_COLLECT_PARAM, this, std::placeholders::_1);
    factoryCommandList[FactroyCmd_UPLOAD_PRESS_SENSOR] =
        std::bind(&Qpb::process_FactroyCmd_UPLOAD_PRESS_SENSOR, this, std::placeholders::_1); // 获取压感数据
    factoryCommandList[FactroyCmd_UPLOAD_NINE_ALEX] =
        std::bind(&Qpb::process_FactroyCmd_UPLOAD_NINE_ALEX, this, std::placeholders::_1); // 获取imu数据

    factoryCommandList[FactroyCmd_FAC_LOG] =
        std::bind(&Qpb::process_FactroyCmd_FAC_LOG, this, std::placeholders::_1); // 获取imu数据

    factoryCommandList[FactroyCmd_SET_DEVICE_STATE] =
        std::bind(&Qpb::process_FactroyCmd_SET_DEVICE_STATE, this, std::placeholders::_1); // 禁止休眠
    factoryCommandList[FactroyCmd_SET_DEVICE_INFO] =
        std::bind(&Qpb::process_FactroyCmd_SET_DEVICE_INFO, this, std::placeholders::_1); // 进入亮白模式

    factoryCommandList[FactroyCmd_SET_PRESS_SENSOR_CALIB] =
        std::bind(&Qpb::process_FactroyCmd_SET_PRESS_SENSOR_CALIB, this, std::placeholders::_1); // 保存完成
    factoryCommandList[FactroyCmd_GET_PRESS_SENSOR_CALIB] =
        std::bind(&Qpb::process_FactroyCmd_GET_PRESS_SENSOR_CALIB, this, std::placeholders::_1); // 获取完成

    factoryCommandList[FactroyCmd_SET_IMU_CALIB] =
        std::bind(&Qpb::process_FactroyCmd_SET_IMU_CALIB, this, std::placeholders::_1); // 保存imu完成
    factoryCommandList[FactroyCmd_GET_DEVICE_BASE_INFO] =
        std::bind(&Qpb::process_FactroyCmd_GET_DEVICE_BASE_INFO, this, std::placeholders::_1); // 获取设备信息
    factoryCommandList[FactroyCmd_GET_PERIPH_STATE] =
        std::bind(&Qpb::process_FactroyCmd_GET_PERIPH_STATE, this, std::placeholders::_1); // 获取设备信息

    factoryCommandList[FactroyCmd_GET_DEVICE_INFO] =
        std::bind(&Qpb::process_FactroyCmd_GET_DEVICE_INFO, this, std::placeholders::_1); // 获取电量信息
    factoryCommandList[FactroyCmd_UPLOAD_BUTTON_STATE] =
        std::bind(&Qpb::process_FactroyCmd_UPLOAD_BUTTON_STATE, this, std::placeholders::_1); // 获取电量信息
    factoryCommandList[FactroyCmd_BRUSH_CONTROL] =
        std::bind(&Qpb::process_FactroyCmd_BRUSH_CONTROL, this, std::placeholders::_1); // 获取电量信息

    factoryCommandList[FactroyCmd_LED_CONTROL] =
        std::bind(&Qpb::process_FactroyCmd_LED_CONTROL, this, std::placeholders::_1); // 获取电量信息

    factoryCommandList[FactroyCmd_GET_BUTTON_STATE] =
        std::bind(&Qpb::process_FactroyCmd_GET_BUTTON_STATE, this, std::placeholders::_1); // 获取电量信息
    factoryCommandList[FactroyCmd_GET_IMU_CALIB] =
        std::bind(&Qpb::process_FactroyCmd_GET_IMU_CALIB, this, std::placeholders::_1); // 获取电量信息

    factoryCommandList[FactroyCmd_SET_AGEING_TEST] =
        std::bind(&Qpb::process_FactroyCmd_SET_AGEING_TEST, this, std::placeholders::_1); // 获取电量信息

    factoryCommandList[FactroyCmd_LCD_CONTROL] =
        std::bind(&Qpb::process_FactroyCmd_LCD_CONTROL, this, std::placeholders::_1); // 获取电量信息

    factoryCommandList[FactroyCmd_MOTO_CONTROL] =
        std::bind(&Qpb::process_FactroyCmd_MOTO_CONTROL, this, std::placeholders::_1); // 获取电量信息

    factoryCommandList[FactroyCmd_UPLOAD_MOTORCALI_DATA] =
        std::bind(&Qpb::process_FactroyCmd_UPLOAD_MOTORCALI_DATA, this,
                  std::placeholders::_1); // 获取电量信息

    factoryCommandList[FactroyCmd_CAMERA_CONTROL] =
        std::bind(&Qpb::process_FactroyCmd_CAMERA_CONTROL, this, std::placeholders::_1); // 获取电量信息

    factoryCommandList[FactroyCmd_INTERNET_OTA] =
        std::bind(&Qpb::process_FactroyCmd_INTERNET_OTA, this, std::placeholders::_1); // 获取电量信息

    bleCommandList[CommandId_CONNECT_PRO] = std::bind(&Qpb::process_CommandId_CONNECT_PRO, this, std::placeholders::_1);
    bleCommandList[CommandId_ROTAS_FILE_STATUS_RSP] =
        std::bind(&Qpb::process_CommandId_ROTAS, this, std::placeholders::_1);

    bleCommandList[CommandId_ROTAS_RESULT_RSP] =
        std::bind(&Qpb::process_CommandId_ROTAS_RESULT_RSP, this, std::placeholders::_1);

    bleCommandList[CommandId_ROTAS_DATA_RSP] = std::bind(&Qpb::process_CommandId_ROTAS, this, std::placeholders::_1);
    bleCommandList[CommandId_ROTAS_RESULT_REQ] = std::bind(&Qpb::process_CommandId_ROTAS, this, std::placeholders::_1);
    bleCommandList[CommandId_ROTAS_FILE_STATUS_REQ] =
        std::bind(&Qpb::process_CommandId_ROTAS_FILE_STATUS_REQ, this, std::placeholders::_1);

    bleCommandList[CommandId_GET_USER_INFO] =
        std::bind(&Qpb::process_CommandId_GET_USER_INFO, this, std::placeholders::_1);

    factoryCommandList[FactroyCmd_WIFI_DEMAND] =
        std::bind(&Qpb::process_FactroyCmd_WIFI_DEMAND, this, std::placeholders::_1); // 获取电量信息

    factoryCommandList[FactroyCmd_UPLOAD_PICTURE_DATA] =
        std::bind(&Qpb::process_FactroyCmd_UPLOAD_PICTURE_DATA, this, std::placeholders::_1); // 获取电量信息
}

void Qpb::process_FactroyCmd_GET_BUTTON_STATE(FactoryDataPackage& f) {
    Q_UNUSED(f);
    qDebug() << "收到按键上报状态指令开关回应";
}

void Qpb::process_FactroyCmd_UPLOAD_PICTURE_DATA(FactoryDataPackage& f) {
    FacPictureDataAck x;
    memcpy(&x, &f.command_data, sizeof(x));
    if (x.send_data_over == FacErrorCode_NO_ERROR) {
        ProtocolPictureSendOverData pictureAckData;
        pictureAckData.result = static_cast<int>(x.result);
        emitReport(QStringLiteral("ProtocolPictureSendOverData"), QVariant::fromValue(pictureAckData));
        emit sendGetProductResponse(1);
    }

    qDebug() << "收到发送数据包结束回应" << x.send_data_over;
}

void Qpb::process_FactroyCmd_WIFI_DEMAND(FactoryDataPackage& f) {
    FacWifiDemand x;
    memcpy(&x, &f.command_data, sizeof(x));

    ProtocolWifiDemandData wifiDemandData;
    wifiDemandData.result = static_cast<int>(x.result);
    emitReport(QStringLiteral("ProtocolWifiDemandData"), QVariant::fromValue(wifiDemandData));
    qDebug() << "收到wifi连接回应" << x.result;
    emit sendGetProductResponse(1);

    is_wif_set = 1;
}
void Qpb::process_FactroyCmd_INTERNET_OTA(FactoryDataPackage& f) {
    FacInternetOta x;
    memcpy(&x, &f.command_data, sizeof(x));
    ProtocolInternetOtaData internetOtaData;
    internetOtaData.result = static_cast<int>(x.result);
    emitReport(QStringLiteral("ProtocolInternetOtaData"), QVariant::fromValue(internetOtaData));
    qDebug() << "收到本地ota控制回应:" << x.result;
    emit sendGetProductResponse(1);
}
void Qpb::process_FactroyCmd_CAMERA_CONTROL(FactoryDataPackage& f) {
    FacCameraControl x;
    memcpy(&x, &f.command_data, sizeof(x));
    ProtocolCameraControlData cameraControlData;
    cameraControlData.type = static_cast<int>(x.type);
    emitReport(QStringLiteral("ProtocolCameraControlData"), QVariant::fromValue(cameraControlData));
    qDebug() << "收到摄像头控制回应:" << x.type;
    is_camera_control = true;
    emit sendGetProductResponse(1);
}
void Qpb::process_FactroyCmd_UPLOAD_MOTORCALI_DATA(FactoryDataPackage& f) {
    FacMotorCalibResult x;
    memcpy(&x, &f.command_data, sizeof(x));
    emitReport(QStringLiteral("ProtocolPbDate"), "收到设备上报信息");
    if (x.which_value_item == FacMotorCalibResult_hall_calibration_data_tag &&
        x.type == FacMotorUploadType_HALL_CALIBRATION_DATA) {
        emit sendGetProductResponse(1);
        emitReport(QStringLiteral("ProtocolPbDate"), "hall_calibration_data" + QString::number(x.value_item.hall_calibration_data));
        qDebug() << "获取到霍尔校准结果" << x.value_item.hall_calibration_data;
        if (x.value_item.hall_calibration_data == 0)
            is_hall_cali = 1;
        else
            is_hall_cali = 2;
    }
    if (x.which_value_item == FacMotorCalibResult_zero_calibration_data_tag &&
        x.type == FacMotorUploadType_ZERO_CALIBRATION_DATA) {
        emit sendGetProductResponse(1);
        emitReport(QStringLiteral("ProtocolPbDate"), "获取到0点校准结果为" + QString::number(x.value_item.zero_calibration_data));
        qDebug() << "获取到0点校准结果" << x.value_item.zero_calibration_data;
        if (x.value_item.zero_calibration_data == 0) {
            emitReport(QStringLiteral("ProtocolPbDate"), "0点校准成功");
            is_zero_cali = 1;
        } else {
            emitReport(QStringLiteral("ProtocolPbDate"), "0点校准失败");
            is_zero_cali = 2;
        }
    }

    if (x.which_value_item == FacMotorCalibResult_servo_info_tag && x.type == FacMotorUploadType_SERVO_INFO) {
        qDebug() << "收到电机信息"
                 << "new" << x.value_item.servo_info.opera_info.zero_info;
        ProtocolServoMotorInfoData servoMotorInfoData;
        servoMotorInfoData.uploadType = static_cast<int>(x.type);
        servoMotorInfoData.whichValue = static_cast<int>(x.which_value_item);
        servoMotorInfoData.motorCaliMark = static_cast<int>(x.value_item.servo_info.motor_cali_mark);
        servoMotorInfoData.motorCurrent = static_cast<int>(x.value_item.servo_info.motor_current);
        servoMotorInfoData.motorState = static_cast<int>(x.value_item.servo_info.motor_state);
        servoMotorInfoData.motorVoltage = static_cast<int>(x.value_item.servo_info.motor_voltage);
        servoMotorInfoData.faultCode = static_cast<int>(x.value_item.servo_info.FaultCode);
        servoMotorInfoData.hallInfo = QString::fromUtf8(x.value_item.servo_info.opera_info.hall_info);
        servoMotorInfoData.zeroInfo = QString::fromUtf8(x.value_item.servo_info.opera_info.zero_info);
        emitReport(QStringLiteral("ProtocolServoMotorInfoData"), QVariant::fromValue(servoMotorInfoData));
        emit sendGetProductResponse(1);
    }

    if (x.which_value_item == FacMotorCalibResult_motor_log_tag && x.type == FacMotorUploadType_MOTOR_LOG) {
        qDebug() << "收到电机日志" << x.value_item.motor_log;
        QString motorLogString = QString::fromLatin1(x.value_item.motor_log);
        emitReport(QStringLiteral("ProtocolMotorCaliMsg"), motorLogString);
        emit sendGetProductResponse(1);
    }

    qDebug() << "收到电机校准回应:" << x.type;
}

void Qpb::process_FactroyCmd_MOTO_CONTROL(FactoryDataPackage& f) {
    qDebug() << "收到电机控制回应:";
    FacMotoControl x;
    memcpy(&x, &f.command_data, sizeof(x));

    if (x.type == FacMotoControlType_motor_param) {
        is_motor_param_set = 1;
        qDebug() << "电机参数设置操作";
        emit sendGetProductResponse(1);
    }
    if (x.type == FacMotoControlType_motor_test) {
        qDebug() << "电机测试操作";
        emit sendGetProductResponse(1);
        is_motor_test_state = 1;
    }
    if (x.type == FacMotoControlType_damping_state) {
        qDebug() << "电机阻尼操作";
        emit sendGetProductResponse(1);
        is_damping_state = 1;
    }
    if (x.type == FacMotoControlType_servo_param) {
        qDebug() << "电机参数设置成功";
        emit sendGetProductResponse(1);
        is_motor_test_state = 1;
    }
    if (x.type == FacMotoControlType_motor_cali_data) {
        qDebug() << "电机校准参数设置完成";
        emit sendGetProductResponse(1);
        is_motor_cali_data_set = 1;
    }
    if (x.type == FacMotoControlType_get_servo_info_data) {
        qDebug() << "伺服电机信息获取成功";
        emit sendGetProductResponse(1);
    }

    if (x.type == FacMotoControlType_motor_cali && x.value_item.switch_cali == FacMotoCali_MOTOR_CALI_STOP) {
        qDebug() << "停止校准流程";
        emit sendGetProductResponse(1);
        is_stop_motor_cali = 1;
    }

    if (x.type == FacMotoControlType_motor_cali) {
        emit sendGetProductResponse(1);
        qDebug() << "电机校准流程操作（0是霍尔校准1是零点校准）" << x.value_item.switch_cali;
    }
}
void Qpb::process_FactroyCmd_LCD_CONTROL(FactoryDataPackage& f) {
    FacLcdControl x;
    memcpy(&x, &f.command_data, sizeof(x));
    ProtocolLcdControlData lcdControlData;
    lcdControlData.type = static_cast<int>(x.type);
    emitReport(QStringLiteral("ProtocolLcdControlData"), QVariant::fromValue(lcdControlData));
    qDebug() << "收到屏幕控制回应:" << x.type;
    emit sendGetProductResponse(1);
}
void Qpb::process_FactroyCmd_SET_AGEING_TEST(FactoryDataPackage& f) {
    FacAgeingTest x;
    memcpy(&x, &f.command_data, sizeof(x));

    if (x.switch_state == 1) {
        emit sendGetProductResponse(1);
    }
}

void Qpb::process_FactroyCmd_GET_IMU_CALIB(FactoryDataPackage& f) {
    FacImuCalibResult x;
    memcpy(&x, &f.command_data, sizeof(x));

    qDebug() << "设备IMU校准值:";
    qDebug().nospace().noquote() << "gyro_x: " << x.gyro_x;
    qDebug().nospace().noquote() << "gyro_y: " << x.gyro_y;
    qDebug().nospace().noquote() << "gyro_z: " << x.gyro_z;
    qDebug().nospace().noquote() << "新校准值:";
    qDebug().nospace().noquote() << "bx: " << x.new_cali.bx;
    qDebug().nospace().noquote() << "by: " << x.new_cali.by;
    qDebug().nospace().noquote() << "bz: " << x.new_cali.bz;
    qDebug().nospace().noquote() << "kx: " << x.new_cali.kx;
    qDebug().nospace().noquote() << "ky: " << x.new_cali.ky;
    qDebug().nospace().noquote() << "kz: " << x.new_cali.kz;
    qDebug().nospace().noquote() << "syx: " << x.new_cali.syx;
    qDebug().nospace().noquote() << "szx: " << x.new_cali.szx;
    qDebug().nospace().noquote() << "szy: " << x.new_cali.szy;
    is_get_imu_cali_data = 1;
    ProtocolImuCalibResultData imuCalibData;
    imuCalibData.result = static_cast<int>(x.result);
    imuCalibData.gyroX = static_cast<int>(x.gyro_x);
    imuCalibData.gyroY = static_cast<int>(x.gyro_y);
    imuCalibData.gyroZ = static_cast<int>(x.gyro_z);
    imuCalibData.bx = static_cast<int>(x.new_cali.bx);
    imuCalibData.by = static_cast<int>(x.new_cali.by);
    imuCalibData.bz = static_cast<int>(x.new_cali.bz);
    imuCalibData.kx = static_cast<int>(x.new_cali.kx);
    imuCalibData.ky = static_cast<int>(x.new_cali.ky);
    imuCalibData.kz = static_cast<int>(x.new_cali.kz);
    imuCalibData.syx = static_cast<int>(x.new_cali.syx);
    imuCalibData.szx = static_cast<int>(x.new_cali.szx);
    imuCalibData.szy = static_cast<int>(x.new_cali.szy);
    emitReport(QStringLiteral("ProtocolImuCalibResultData"), QVariant::fromValue(imuCalibData));
    if (x.result) {
        emitReport(QStringLiteral("ProtocolPbDate"), "设备六轴校准数据有问题,可能是初始值");
    } else {
        emitReport(QStringLiteral("ProtocolPbDate"), "设备六轴校准数据是工厂值,是可靠的");
    }
    emit sendGetProductResponse(1);
}
void Qpb::process_FactroyCmd_LED_CONTROL(FactoryDataPackage& f) {
    FacLedControl x;
    memcpy(&x, &f.command_data, sizeof(x));
    ProtocolLedControlData ledControlData;
    ledControlData.switchState = static_cast<int>(x.switch_state);
    ledControlData.ledStateCount = static_cast<int>(x.led_state_count);
    emitReport(QStringLiteral("ProtocolLedControlData"), QVariant::fromValue(ledControlData));
    qDebug() << "收到灯光控制回应:" << x.switch_state;

    qDebug() << "收到灯光count:" << x.led_state_count;
    emit sendGetProductResponse(1);
}
void Qpb::process_FactroyCmd_BRUSH_CONTROL(FactoryDataPackage& f) {
    FacBrushControl x;
    memcpy(&x, &f.command_data, sizeof(x));
    ProtocolBrushControlData brushControlData;
    brushControlData.brushStart = static_cast<int>(x.value_item.brush_start);
    emitReport(QStringLiteral("ProtocolBrushControlData"), QVariant::fromValue(brushControlData));
    qDebug() << "收到设备控制回应:" << x.value_item.brush_start;
    emit sendGetProductResponse(1);
}
void Qpb::process_FactroyCmd_UPLOAD_BUTTON_STATE(FactoryDataPackage& f) {
    FacButtonState x;
    memcpy(&x, &f.command_data, sizeof(x));

    qDebug() << "获取到模式按键状态" << x.button_state[1].command_data.mode_button.button_state_now;
    qDebug() << "获取到开关按键状态" << x.button_state[0].command_data.power_button.button_state_now;

    ProtocolButtonStateData buttonStateData;
    buttonStateData.modeButtonState = static_cast<int>(x.button_state[1].command_data.mode_button.button_state_now);
    buttonStateData.powerButtonState = static_cast<int>(x.button_state[0].command_data.power_button.button_state_now);
    emitReport(QStringLiteral("ProtocolButtonStateData"), QVariant::fromValue(buttonStateData));
    emit sendGetProductResponse(1);
}

void Qpb::process_FactroyCmd_GET_DEVICE_INFO(FactoryDataPackage& f) {
    FacDevInfo x;
    memcpy(&x, &f.command_data, sizeof(x));

    if (x.dev_info[0].which_value_item == FacDevInfoValue_battery_tag) {
        ProtocolBatteryData batteryData;
        batteryData.chargeState = static_cast<int>(x.dev_info[0].value_item.battery.charge_state);
        batteryData.percent = static_cast<int>(x.dev_info[0].value_item.battery.percent);
        batteryData.voltageMv = static_cast<int>(x.dev_info[0].value_item.battery.voltage);
        emitReport(QStringLiteral("ProtocolBatteryData"), QVariant::fromValue(batteryData));
        qDebug() << "获取到电量信息";
        emit sendGetProductResponse(1);
        is_get_battery_data = 1;
    }
    if (x.dev_info[0].which_value_item == FacDevInfoValue_wifi_info_tag) {
        ProtocolWifiStateData wifiData;
        wifiData.wifiName = QString::fromUtf8(x.dev_info[0].value_item.wifi_info.wifi_name);
        wifiData.wifiPassword = QString::fromUtf8(x.dev_info[0].value_item.wifi_info.wifi_password);
        emitReport(QStringLiteral("ProtocolWifiStateData"), QVariant::fromValue(wifiData));
        qDebug() << "获取到wifi信息";
        emit sendGetProductResponse(1);
    }
    if (x.dev_info[0].which_value_item == FacDevInfoValue_board_sn_tag) {
        emitReport(QStringLiteral("ProtocolSnData"), QVariant::fromValue(ProtocolSnData{ProtocolSnType::BoardSn, QString::fromUtf8(x.dev_info[0].value_item.board_sn)}));
        qDebug() << "获取到板子的sn" << x.dev_info[0].value_item.board_sn;
        emit sendGetProductResponse(1);
    }
    if (x.dev_info[0].which_value_item == FacDevInfoValue_tail_sn_tag) {
        emitReport(QStringLiteral("ProtocolSnData"), QVariant::fromValue(ProtocolSnData{ProtocolSnType::TailSn, QString::fromUtf8(x.dev_info[0].value_item.product_sn)}));
        qDebug() << "获取到回复整机的sn" << x.dev_info[0].value_item.product_sn;
        emit sendGetProductResponse(1);
    }
    if (x.dev_info[0].which_value_item == FacDevInfoValue_sub_pid_tag) {
        emitReport(QStringLiteral("ProtocolSnData"), QVariant::fromValue(ProtocolSnData{ProtocolSnType::SubPid, QString::fromUtf8(x.dev_info[0].value_item.sub_pid)}));
        qDebug() << "获取到回应sub_pid" << x.dev_info[0].value_item.sub_pid;
        emit sendGetProductResponse(1);
    }
    if (x.dev_info[0].which_value_item == FacDevInfoValue_sku_id_tag) {
        emitReport(QStringLiteral("ProtocolSnData"), QVariant::fromValue(ProtocolSnData{ProtocolSnType::SkuId, QString::fromUtf8(x.dev_info[0].value_item.sku_id)}));
        qDebug() << "获取到回应sku_id" << x.dev_info[0].value_item.sku_id;
        emit sendGetProductResponse(1);
    }
    if (x.dev_info[0].which_value_item == FacDevInfoValue_music_state_tag) {
        ProtocolMusicStateData musicStateData;
        musicStateData.musicState = static_cast<int>(x.dev_info[0].value_item.music_state);
        emitReport(QStringLiteral("ProtocolMusicStateData"), QVariant::fromValue(musicStateData));
        qDebug() << "获取到音乐状态回应" << x.dev_info[0].value_item.music_state;
        emit sendGetProductResponse(1);
    }

    if (x.dev_info[0].which_value_item == FacDevInfoValue_sdcard_tag) {
        ProtocolSdInfoData sdInfoData;
        sdInfoData.cmd = static_cast<int>(x.dev_info[0].value_item.sdcard.cmd);
        sdInfoData.data = QString::fromUtf8(x.dev_info[0].value_item.sdcard.data);
        emitReport(QStringLiteral("ProtocolSdInfoData"), QVariant::fromValue(sdInfoData));
        qDebug() << "获取到sd卡命令" << x.dev_info[0].value_item.sdcard.cmd;
        qDebug() << "获取到sd卡信息" << x.dev_info[0].value_item.sdcard.data;
        emit sendGetProductResponse(1);
    }
    if (x.dev_info[0].which_value_item == FacDevInfoValue_light_sensor_tag) {
        ProtocolPhotosensitiveData photosensitiveData;
        photosensitiveData.lightSensor = static_cast<int>(x.dev_info[0].value_item.light_sensor);
        emitReport(QStringLiteral("ProtocolPhotosensitiveData"), QVariant::fromValue(photosensitiveData));
        qDebug() << "获取到光敏电阻信息" << x.dev_info[0].value_item.light_sensor;
        emit sendGetProductResponse(1);
    }

    emitReport(QStringLiteral("ProtocolPbDate"), "获取到写入的日期版本信息内容:" + QString(x.dev_info[0].write_info));
}

void Qpb::process_FactroyCmd_GET_PERIPH_STATE(FactoryDataPackage& f) {
    FacGetPeriphState x;
    memcpy(&x, &f.command_data, sizeof(x));
    ProtocolPeriphStateData periphData;
    periphData.imu_state = x.imu_state ? 1 : 0;
    periphData.flash_state = x.flash_state ? 1 : 0;
    periphData.magnet_state = x.magnet_state ? 1 : 0;
    periphData.press_state = x.press_state ? 1 : 0;
    periphData.audio_state = x.audio_state ? 1 : 0;
    periphData.result = static_cast<int>(x.result);
    emitReport(QStringLiteral("ProtocolPeriphStateData"), QVariant::fromValue(periphData));
    qDebug() << "获取到外设状态";
    emit sendGetProductResponse(1);
}
void Qpb::process_FactroyCmd_GET_DEVICE_BASE_INFO(FactoryDataPackage& f) {
    FacGetDevBaseInfo x;
    memcpy(&x, &f.command_data, sizeof(x));
    qDebug() << "获取到设备信息";
    ProtocolBaseInfoData baseInfoData;
    baseInfoData.product_name = QString::fromUtf8(x.product_name);
    baseInfoData.pb_phone_ver = static_cast<int>(x.pb_phone_ver);
    baseInfoData.pb_factory_ver = static_cast<int>(x.pb_factory_ver);
    baseInfoData.hw_version = QString::fromUtf8(x.hw_version);
    baseInfoData.soft_version = QString::fromUtf8(x.soft_version);
    baseInfoData.res_version = QString::fromUtf8(x.res_version);
    baseInfoData.algo_version = QString::fromUtf8(x.algo_version);
    baseInfoData.presure_version = QString::fromUtf8(x.presure_version);
    baseInfoData.motor_version = QString::fromUtf8(x.motor_version);
    baseInfoData.ble_version = QString::fromUtf8(x.ble_version);
    baseInfoData.camera_version = QString::fromUtf8(x.camera_version);
    baseInfoData.fsensor_version = QString::fromUtf8(x.fsensor_version);
    baseInfoData.ble_mac.size = static_cast<int>(x.ble_mac.size);
    baseInfoData.wifi_mac.size = static_cast<int>(x.wifi_mac.size);
    for (int i = 0; i < baseInfoData.ble_mac.size && i < 6; ++i) {
        baseInfoData.ble_mac.bytes[i] = x.ble_mac.bytes[i];
    }
    for (int i = 0; i < baseInfoData.wifi_mac.size && i < 6; ++i) {
        baseInfoData.wifi_mac.bytes[i] = x.wifi_mac.bytes[i];
    }
    baseInfoData.imu_id = static_cast<int>(x.imu_id);
    baseInfoData.result = static_cast<int>(x.result);
    baseInfoData.ageing_state = x.ageing_state ? 1 : 0;
    baseInfoData.ageingState = baseInfoData.ageing_state;
    emitReport(QStringLiteral("ProtocolBaseInfoData"), QVariant::fromValue(baseInfoData));
    emit sendGetProductResponse(1);
}
void Qpb::process_FactroyCmd_SET_IMU_CALIB(FactoryDataPackage& f) {
    qDebug() << "收到保存imu校准值回应" << f.command_data.set_imu_calib.result;
    emit sendGetProductResponse(1);
}
void Qpb::process_FactroyCmd_GET_PRESS_SENSOR_CALIB(FactoryDataPackage& f) {
    FacPreSensorCalibResult x;
    memcpy(&x, &f.command_data, sizeof(x));
    qDebug() << "获取设备校准的brush_head_adc=" << x.brush_head_adc;
    qDebug() << "获取设备校准的mode_button_adc=" << x.mode_button_adc;
    qDebug() << "获取设备校准的temperature=" << x.temperature;
    qDebug() << "获取设备校准的power_button_adc=" << x.power_button_adc;
    qDebug() << "获取模式辅助元器件校准的assistant_component=" << x.assistant_component;
    ProtocolPressCalibResultData pressCalibData;
    pressCalibData.brushHeadAdc = static_cast<int>(x.brush_head_adc);
    pressCalibData.modeButtonAdc = static_cast<int>(x.mode_button_adc);
    pressCalibData.powerButtonAdc = static_cast<int>(x.power_button_adc);
    pressCalibData.assistantComponent = static_cast<int>(x.assistant_component);
    pressCalibData.temperature = static_cast<int>(x.temperature);
    emitReport(QStringLiteral("ProtocolPressCalibResultData"), QVariant::fromValue(pressCalibData));
    is_save_press_cali_ok = 1;
    emit sendGetProductResponse(1);
}
void Qpb::process_FactroyCmd_SET_PRESS_SENSOR_CALIB(FactoryDataPackage& f) {
    FacDevInfo x;
    memcpy(&x, &f.command_data, sizeof(x));

    qDebug() << "保存压感校准值成功";
    get_press_cali_result();
}
void Qpb::process_FactroyCmd_SET_DEVICE_STATE(FactoryDataPackage& f) {
    FacDevState x;
    memcpy(&x, &f.command_data, sizeof(x));
    if (x.dev_state_type == DevStateType_FORBID_SLEEP && x.state == FacSwitch_OPEN &&
        x.result == FacErrorCode_NO_ERROR) {
        qDebug() << "进入禁止休眠成功";
        is_disable_sleep = 1;
        emit sendGetProductResponse(1);
    }
    if (x.dev_state_type == DevStateType_FORBID_SLEEP && x.state == FacSwitch_CLOSE &&
        x.result == FacErrorCode_NO_ERROR) {
        qDebug() << "取消禁止休眠成功";
        is_close_forbid_sleep = 1;
        emit sendGetProductResponse(1);
    }

    if (x.dev_state_type == DevStateType_SLEEP) {
        qDebug() << "设置休眠成功";
        emit sendGetProductResponse(1);
    }
    if (x.dev_state_type == DevStateType_SHIP) {
        qDebug() << "设置船运成功";
        emitReport(QStringLiteral("ProtocolPbDate"), "设置船运成功" + QString::number(shipCount));
        emit sendGetProductResponse(1);
        if (shipCount) {
            increaseShipCount();
        }
    }
    if (x.dev_state_type == DevStateType_FACTORY_QRCORD) {
        qDebug() << "设置工厂模式成功";
        emitReport(QStringLiteral("ProtocolPbDate"), "设置工厂模式成功");
        emit sendGetProductResponse(1);
    }
    if (x.dev_state_type == DevStateType_UART_RECEIVE) {
        qDebug() << "设置串口状态成功";
        emitReport(QStringLiteral("ProtocolPbDate"), "设置串口状态成功");
        emit sendGetProductResponse(1);
    }
}

void Qpb::process_FactroyCmd_SET_DEVICE_INFO(FactoryDataPackage& f) {
    FacDevInfo x;
    memcpy(&x, &f.command_data, sizeof(x));
    //  qDebug() <<
    //  "进入亮白模式成功"<<x.dev_info_count<<"呵呵呵"<<x.dev_info[0].value_item.brush_mode<<"呵呵呵"<<
    //  x.dev_info[0].info_item<<"呵呵呵"<<x.dev_info[0].which_value_item;

    if (x.dev_info_count == 1 && x.dev_info[0].info_item == FacDevInfoType_BRUSH_MODE &&
        x.dev_info[0].which_value_item == FacDevInfoValue_brush_mode_tag) {
        qDebug() << "进入亮白模式成功";
        is_dev_into_white_mode = 1;

        emit sendGetProductResponse(1);
    }

    if (x.dev_info[0].info_item == FacDevInfoType_BOARD_SN) { //  qDebug() << "已经收到板子的sn";
        get_sn(FacDevInfoType_BOARD_SN);
        emit sendGetProductResponse(1);
    }
    if (x.dev_info[0].info_item == FacDevInfoType_SUB_PID) {
        qDebug() << "已经收到板子的subpid设置回应";
        emit sendGetProductResponse(1);
    }

    if (x.dev_info[0].info_item == FacDevInfoType_TAIL_SN) {
        is_banding_ok = 1;
        qDebug() << "已经收到写入整机的sn回应";
        emit sendGetProductResponse(1);
    }

    if (x.dev_info[0].info_item == FacDevInfoType_SKUID) {
        qDebug() << "已经收到skuid回应";
        emit sendGetProductResponse(1);
    }
    if (x.dev_info[0].info_item == FacDevInfoType_WIFI_INFO) {
        qDebug() << "wifi连接回应";
        emit sendGetProductResponse(1);
    }
}
void Qpb::process_CommandId_ROTAS_FILE_STATUS_REQ(DataPackage& f) {
    RotasFileStatusReq x;
    memcpy(&x, &f.command_data, sizeof(x));
    if (x.fileType == RotasUpdateFile_BLE_FIRMWARE) {
        emit sendGetProductResponse(1);
    }
}
void Qpb::process_CommandId_GET_USER_INFO(DataPackage& f) {
    DataPackage x;
    memcpy(&x, &f, sizeof(x));
    qDebug() << "x.which_command_data" << x.which_command_data;
    if (x.which_command_data == DataPackage_get_user_info_tag) //回应是0
    {
        emitReport(QStringLiteral("ProtocolPbDate"), "成功收到获取user_info指令回应");
        is_set_i_am_app = 1;
        emit sendGetProductResponse(1);
    }
}

void Qpb::process_CommandId_CONNECT_PRO(DataPackage& f) {
    emitReport(QStringLiteral("ProtocolPbInfo"), QString("firmware version : %1").arg(f.command_data.connect_pro.firmware_id));
    qDebug() << "获取到版本号" << f.command_data.connect_pro.firmware_id;
    emit sendGetProductResponse(1);
}

void Qpb::process_CommandId_ROTAS_RESULT_RSP(DataPackage& f) {
    RotasResultRsp x;
    memcpy(&x, &f.command_data, sizeof(x));
    if (x.rotaStatus == RotaFileStatus_ROTA_PAUSE) {
        emitReport(QStringLiteral("ProtocolPbDate"), "流控停止");
        emitReport(QStringLiteral("ProtocolOtaFlowControl"), QVariant::fromValue(0));
        emit sendGetProductResponse(1);
    }
    if (x.rotaStatus == RotaFileStatus_ROTA_START) {
        emitReport(QStringLiteral("ProtocolPbDate"), "流控开始");
        emitReport(QStringLiteral("ProtocolOtaFlowControl"), QVariant::fromValue(1));
        emit sendGetProductResponse(1);
    }
}
void Qpb::process_CommandId_ROTAS(DataPackage& f) {
    QStringList s;
    s << "手柄收到指令,开始OTA..."
      << "GENERAL"
      << "低电量"
      << "已经在OTA中"
      << "文件过大"
      << "MD5错误"
      << "文件不支持"
      << "FLASH错误"
      << "No Memory"
      << "TRANS_TIMEOUT"
      << "TRANS_OVER_RANGE"
      << " 下载成功"
      << "下载失败"
      << "没有配网"
      << "网络连接失败"
      << "获取文件URL失败，可能是长度超了"
      << "文件下载校验失败"
      << "文件下载安装失败"
      << "找不到网络"
      << "密码错误"
      << "正在下载文件中"
      << "WIFI已禁用"
      << "资源更新中"
      << "存储空间不足";

    switch (f.command_id) {
    case CommandId_ROTAS_FILE_STATUS_RSP:
        if (f.command_data.rota_file_status_rsp.result < s.size()) {
            emitReport(QStringLiteral("ProtocolPbInfo"), s[f.command_data.rota_file_status_rsp.result]);

            if (f.command_data.rota_file_status_rsp.result == 0) {
                emitReport(QStringLiteral("ProtocolPbDate"), "成功收到开始ota指令回应");
                is_ota_start = 1;
            }

        } else {
            emitReport(QStringLiteral("ProtocolPbDate"), QString("未知错误码: %1").arg(f.command_data.rota_file_status_rsp.result));
        }
        break;

    case CommandId_ROTAS_DATA_RSP:
        if (f.command_data.rota_data_rsp.progress) {
            emitReport(QStringLiteral("ProtocolOtaProgress"), QVariant::fromValue(f.command_data.rota_data_rsp.progress));
        } else {
            qDebug() << "是0的进度已经省略显示";
        }
        break;

    case CommandId_ROTAS_RESULT_REQ:
        if (f.command_data.rota_result_req.rotaResult < s.size()) {
            emitReport(QStringLiteral("ProtocolPbDate"), s[f.command_data.rota_result_req.rotaResult]);
            emitReport(QStringLiteral("ProtocolOtaResult"), QVariant::fromValue(static_cast<int>(f.command_data.rota_result_req.rotaResult)));
        } else {
            emitReport(QStringLiteral("ProtocolPbDate"), QString("未知结果码: %1").arg(f.command_data.rota_result_req.rotaResult));
        }
        break;

    default:
        break;
    }
}

void Qpb::get_connect_info() {
    CommandId cmd = CommandId_CONNECT_PRO;
    DataPackage pack;
    memset(&pack, 0, sizeof(pack));
    pack.command_id = cmd;
    pack.which_command_data = DataPackage_connect_pro_tag;
    sendShortPack(pack);
    pb_mode = CLIENT;
}

void Qpb::set_i_am_app() {
    CommandId cmd = CommandId_GET_USER_INFO;
    DataPackage pack;
    memset(&pack, 0, sizeof(pack));
    pack.command_id = cmd;
    pack.which_command_data = DataPackage_get_user_info_tag;

    sendMainPack(pack);
    pb_mode = CLIENT;
}

void Qpb::set_start_multi_ble_ota_app(const StartMultiBleOtaPayload& payload) {
    CommandId cmd = CommandId_MULTI_FILE_STATUS_REQ;
    DataPackage pack;
    memset(&pack, 0, sizeof(pack));
    pack.command_id = cmd;
    pack.which_command_data = DataPackage_multi_file_status_req_tag;
    pack.command_data.multi_file_status_req.file_status_list_count = 2;

    pack.command_data.multi_file_status_req.file_status_list[0].fileType = payload.req0.fileType;
    pack.command_data.multi_file_status_req.file_status_list[0].fileSize = payload.req0.fileSize;
    pack.command_data.multi_file_status_req.file_status_list[0].file_zip_md5 = payload.req0.file_zip_md5;

    pack.command_data.multi_file_status_req.file_status_list[1].fileType = payload.req1.fileType;
    pack.command_data.multi_file_status_req.file_status_list[1].fileSize = payload.req1.fileSize;
    pack.command_data.multi_file_status_req.file_status_list[1].file_zip_md5 = payload.req1.file_zip_md5;
    sendShortPack(pack);
}

void Qpb::set_start_ota_app(RotasFileStatusReq RotasFiledata) {
    CommandId cmd = CommandId_ROTAS_FILE_STATUS_REQ;
    DataPackage pack;
    memset(&pack, 0, sizeof(pack));
    pack.command_id = cmd;
    pack.which_command_data = DataPackage_rota_file_status_req_tag;

    switch (RotasFiledata.fileType) {
    case RotasUpdateFile_BLE_FIRMWARE:
        pack.command_data.rota_file_status_req.fileType = RotasFiledata.fileType;
        pack.command_data.rota_file_status_req.fileSize = RotasFiledata.fileSize;
        pack.command_data.rota_file_status_req.fileUnzipSize = RotasFiledata.fileSize;

        break;

    case RotasUpdateFile_WIFI_FIRMWARE:
        pack.command_data.rota_file_status_req.fileType = RotasFiledata.fileType;
        break;
    case RotasUpdateFile_UNKNOWN_TYPE:
    case RotasUpdateFile_UI_RESOURCE:
    case RotasUpdateFile_BP_RESOURCE:
    case RotasUpdateFile_UI_AND_FIRMWARE:
    case RotasUpdateFile_WIFI_VOICE_PACKET:
    case RotasUpdateFile_WIFI_THEME:
    case RotasUpdateFile_WIFI_MUSIC_ENJOY:
    case RotasUpdateFile_MOTOR_RESOURCE:
    case RotasUpdateFile_WIFI_LIGHT_CUSTOM:
    case RotasUpdateFile_BACKUP_CONFIG:
    case RotasUpdateFile_BACKUP_FILE:
    case RotasUpdateFile_RESTORE_CONFIG:
    case RotasUpdateFile_RESTORE_FILE:
    case RotasUpdateFile_BACKUP_ORAL_ARCHIVES:
    case RotasUpdateFile_RESTORE_ORAL_ARCHIVES:
        break;
    }

    sendShortPack(pack);
    pb_mode = CLIENT;
}

DataPackage Qpb::getBlePack() const {
    return blePack;
}

int Qpb::getState(PbStateType stateType) const {
    switch (stateType) {
    case PbStateType::DevIntoWhiteMode:
        return is_dev_into_white_mode;
    case PbStateType::OtaStart:
        return is_ota_start;
    case PbStateType::SetIamApp:
        return is_set_i_am_app;
    case PbStateType::HallCali:
        return is_hall_cali;
    case PbStateType::CameraControl:
        return is_camera_control;
    case PbStateType::DampingState:
        return is_damping_state;
    case PbStateType::WifiSet:
        return is_wif_set;
    case PbStateType::MotorTestState:
        return is_motor_test_state;
    case PbStateType::MotorCaliDataSet:
        return is_motor_cali_data_set;
    case PbStateType::GetBatteryData:
        return is_get_battery_data;
    case PbStateType::StopMotorCali:
        return is_stop_motor_cali;
    case PbStateType::ZeroCali:
        return is_zero_cali;
    case PbStateType::DisableSleep:
        return is_disable_sleep;
    case PbStateType::CollectParam:
        return is_set_press_collect_param;
    case PbStateType::ImuSetState:
        return is_imu_set_sta;
    case PbStateType::CloseForbidSleep:
        return is_close_forbid_sleep;
    case PbStateType::BandingOk:
        return is_banding_ok;
    case PbStateType::MotorParamSet:
        return is_motor_param_set;
    case PbStateType::GetImuCaliData:
        return is_get_imu_cali_data;
    case PbStateType::SetImuCollectParam:
        return is_setimu_collect_param;
    default:
        return 0;
    }
}

void Qpb::setState(PbStateType stateType, int value) {
    switch (stateType) {
    case PbStateType::DevIntoWhiteMode:
        is_dev_into_white_mode = value;
        break;
    case PbStateType::OtaStart:
        is_ota_start = value;
        break;
    case PbStateType::SetIamApp:
        is_set_i_am_app = value;
        break;
    case PbStateType::HallCali:
        is_hall_cali = value;
        break;
    case PbStateType::CameraControl:
        is_camera_control = value;
        break;
    case PbStateType::DampingState:
        is_damping_state = value;
        break;
    case PbStateType::WifiSet:
        is_wif_set = value;
        break;
    case PbStateType::MotorTestState:
        is_motor_test_state = value;
        break;
    case PbStateType::MotorCaliDataSet:
        is_motor_cali_data_set = value;
        break;
    case PbStateType::GetBatteryData:
        is_get_battery_data = value;
        break;
    case PbStateType::StopMotorCali:
        is_stop_motor_cali = value;
        break;
    case PbStateType::ZeroCali:
        is_zero_cali = value;
        break;
    case PbStateType::DisableSleep:
        is_disable_sleep = value;
        break;
    case PbStateType::CollectParam:
        is_set_press_collect_param = value;
        break;
    case PbStateType::ImuSetState:
        is_imu_set_sta = value;
        break;
    case PbStateType::CloseForbidSleep:
        is_close_forbid_sleep = value;
        break;
    case PbStateType::BandingOk:
        is_banding_ok = value;
        break;
    case PbStateType::MotorParamSet:
        is_motor_param_set = value;
        break;
    case PbStateType::GetImuCaliData:
        is_get_imu_cali_data = value;
        break;
    case PbStateType::SetImuCollectParam:
        is_setimu_collect_param = value;
        break;
    default:
        break;
    }
}

void Qpb::process_FactroyCmd_SET_COLLECT_PARAM(FactoryDataPackage& f) {
    FacCollectParam x;
    memcpy(&x, &f.command_data, sizeof(x));
    is_imu_set_sta = x.param[0].command_data.nine_axle.state;

    qDebug() << "接收到imu状态设置为" << x.param[0].command_data.nine_axle.state;
    qDebug() << "接收到压感状态设置为" << x.param[0].command_data.pressure_sensor.state;
    emit sendGetProductResponse(1);
}
void Qpb::process_FactroyCmd_UPLOAD_PRESS_SENSOR(FactoryDataPackage& f) {
    FacUploadPresSensor x;
    memcpy(&x, &f.command_data, sizeof(x));
    // qDebug () << "sensor data";
    ProtocolPressSampleData pressData;
    if (x.sensor_data_count > 0) {
        pressData.timeStamp = static_cast<int>(x.sensor_data[0].timestamp);
    }
    for (int i = 0; i < static_cast<int>(x.sensor_data_count) && i < 15; ++i) {
        if (x.sensor_data[i].has_brush_head) {
            pressData.adcValues.push_back(static_cast<int>(x.sensor_data[i].brush_head.adc));
            pressData.valueValues.push_back(static_cast<int>(x.sensor_data[i].brush_head.value));
        }
        if (x.sensor_data[i].has_mode_button) {
            pressData.adcValues.push_back(static_cast<int>(x.sensor_data[i].mode_button.adc));
            pressData.valueValues.push_back(static_cast<int>(x.sensor_data[i].mode_button.value));
        }
        if (x.sensor_data[i].has_power_button) {
            pressData.adcValues.push_back(static_cast<int>(x.sensor_data[i].power_button.adc));
            pressData.valueValues.push_back(static_cast<int>(x.sensor_data[i].power_button.value));
        }
        if (x.sensor_data[i].has_assistant_component) {
            pressData.adcValues.push_back(static_cast<int>(x.sensor_data[i].assistant_component.adc));
            pressData.valueValues.push_back(static_cast<int>(x.sensor_data[i].assistant_component.value));
        }
    }
    emitReport(QStringLiteral("ProtocolPressSampleData"), QVariant::fromValue(pressData));
    is_set_press_collect_param = 1;
    emit sendGetProductResponse(1);
}

void Qpb::process_FactroyCmd_FAC_LOG(FactoryDataPackage& f) {
    FacUploadNineAlex x;
    memcpy(&x, &f.command_data, sizeof(x));
    qDebug() << "收到设备黑盒日志回应";

    emit sendGetProductResponse(1);
}
void Qpb::process_FactroyCmd_UPLOAD_NINE_ALEX(FactoryDataPackage& f) {
    FacUploadNineAlex x;
    memcpy(&x, &f.command_data, sizeof(x));
    qDebug() << "收到imu数据包";
    ProtocolImuSampleData imuData;
    for (int i = 0; i < static_cast<int>(x.data_count) && i < 15; ++i) {
        imuData.timeStamp = static_cast<int>(x.data[i].timestamp);
        imuData.accelValues.push_back(static_cast<int>(x.data[i].acc_x));
        imuData.accelValues.push_back(static_cast<int>(x.data[i].acc_y));
        imuData.accelValues.push_back(static_cast<int>(x.data[i].acc_z));
        imuData.gyroValues.push_back(static_cast<int>(x.data[i].gyro_x));
        imuData.gyroValues.push_back(static_cast<int>(x.data[i].gyro_y));
        imuData.gyroValues.push_back(static_cast<int>(x.data[i].gyro_z));
    }
    emitReport(QStringLiteral("ProtocolImuSampleData"), QVariant::fromValue(imuData));
    is_setimu_collect_param = 1;
    emit sendGetProductResponse(1);
}

void Qpb::set_config_network_app(WifiInfo info) {
    CommandId cmd = CommandId_WIFI_INFO;
    DataPackage pack;
    memset(&pack, 0, sizeof(pack));
    pack.command_id = cmd;

    pack.which_command_data = DataPackage_wifi_info_tag;
    memcpy(&pack.command_data.wifi_info, &info, sizeof(info));
    sendShortPack(pack);
    pb_mode = CLIENT;
}
