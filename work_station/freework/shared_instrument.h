#ifndef SHARED_INSTRUMENT_H
#define SHARED_INSTRUMENT_H

#include <QString>
#include <QVariantMap>

/**
 * 自由工站「两工位共用一台外设」：全部由测试步骤 Param 配置（不读上位机设置）。
 *
 * 典型 Param（配置Visa程控电源.ini）：
 *   sharedPair=true
 *   stationsPerDevice=2
 *   visaAddress0=GPIB0::7::INSTR
 *   visaAddress1=GPIB0::8::INSTR
 *   scpiChannelSelectCmd=INST OUT%1   ; Agilent 双通道；会凌 SOURceN 可省略
 *
 * 温度记录仪：
 *   sharedPair=true
 *   stationsPerDevice=2
 *   tempComName0=COM10
 *   tempComName1=COM11
 *
 * 一拖四：工位1/2→设备0 通道1/2；工位3/4→设备1 通道1/2。
 */
namespace SharedInstrument {

struct Slot {
    int deviceIndex = 0; // 0-based
    int channel = 1;     // 1-based
};

/** 步骤写 sharedPair=true / shareInstrument=true，或已带 visaAddress0/tempComName0 即视为启用。 */
bool isEnabledInParam(const QVariantMap& paramMap);
int stationsPerDeviceFromParam(const QVariantMap& paramMap);
Slot slotForStation(int stationIndex1Based, int stationsPerDevice);

QString visaAddressFromParam(const QVariantMap& paramMap, int deviceIndex0Based);
QString tempComNameFromParam(const QVariantMap& paramMap, int deviceIndex0Based);

/**
 * 按工位改写 VISA Param：选地址表、填 powerChannel、改写/选通 SCPI。
 * 启用 sharedPair 时每次按工位重算通道（避免缓存里的 powerChannel 串工位）。
 */
bool applyVisaParamsForStation(int stationIndex1Based, QVariantMap* paramMap, QString* detailOut = nullptr);
bool applyTempLoggerParamsForStation(int stationIndex1Based, QVariantMap* paramMap, QString* detailOut = nullptr);

} // namespace SharedInstrument

#endif // SHARED_INSTRUMENT_H
