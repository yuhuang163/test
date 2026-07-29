#include "ls_meter.h"
#include "config.h"
#include <math.h>

static constexpr int LS_PWR_EN_GPIO = 46;
static constexpr int LS_EN_GPIO = 40;
static constexpr int LS_UART_RX_GPIO = 21;
static constexpr int LS_UART_TX_GPIO = 47;
static constexpr uint8_t LS_MODBUS_ADDR = 0x01;
static constexpr uint32_t LS_MODBUS_BAUD = 9600;

static bool s_lowRangeReady = false;

static uint16_t modbusCrc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
        {
            if (crc & 0x0001)
            {
                crc >>= 1;
                crc ^= 0xA001;
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    return crc;
}

static bool modbusReadRegs(uint16_t startReg, uint16_t regCount, uint16_t *outRegs)
{
    if (outRegs == nullptr || regCount == 0)
    {
        return false;
    }

    uint8_t req[8];
    req[0] = LS_MODBUS_ADDR;
    req[1] = 0x03;
    req[2] = (uint8_t)(startReg >> 8);
    req[3] = (uint8_t)(startReg & 0xFF);
    req[4] = (uint8_t)(regCount >> 8);
    req[5] = (uint8_t)(regCount & 0xFF);
    uint16_t crc = modbusCrc16(req, 6);
    req[6] = (uint8_t)(crc & 0xFF);
    req[7] = (uint8_t)(crc >> 8);

    while (Serial2.available() > 0)
    {
        Serial2.read();
    }

    Serial2.write(req, sizeof(req));
    Serial2.flush();

    const size_t expected = 5 + regCount * 2;
    uint8_t resp[64] = {0};
    size_t got = Serial2.readBytes(resp, expected);
    if (got != expected)
    {
        return false;
    }
    if (resp[0] != LS_MODBUS_ADDR || resp[1] != 0x03 || resp[2] != regCount * 2)
    {
        return false;
    }

    uint16_t crcResp = (uint16_t)resp[expected - 2] | ((uint16_t)resp[expected - 1] << 8);
    uint16_t crcCalc = modbusCrc16(resp, expected - 2);
    if (crcResp != crcCalc)
    {
        return false;
    }

    for (uint16_t i = 0; i < regCount; i++)
    {
        outRegs[i] = ((uint16_t)resp[3 + i * 2] << 8) | resp[4 + i * 2];
    }
    return true;
}

static bool ensureLowRangeReady()
{
    if (s_lowRangeReady)
    {
        return true;
    }

    pinMode(LS_PWR_EN_GPIO, OUTPUT);
    pinMode(LS_EN_GPIO, OUTPUT);
    digitalWrite(LS_PWR_EN_GPIO, HIGH);
    delay(50);
    digitalWrite(LS_EN_GPIO, HIGH);

    Serial2.begin(LS_MODBUS_BAUD, SERIAL_8N1, LS_UART_RX_GPIO, LS_UART_TX_GPIO);
    Serial2.setTimeout(20);
    s_lowRangeReady = true;
    return true;
}

static void shutdownLowRange()
{
    if (!s_lowRangeReady)
    {
        return;
    }
    Serial2.end();
    digitalWrite(LS_EN_GPIO, LOW);
    digitalWrite(LS_PWR_EN_GPIO, LOW);
    s_lowRangeReady = false;
}

void LSMeter::read_and_print()
{
    if (!lsadc_data)
    {
        shutdownLowRange();
        return;
    }
    if (!ensureLowRangeReady())
    {
        return;
    }

    uint16_t regs[3] = {0};
    if (!modbusReadRegs(0x0000, 3, regs))
    {
        Serial.println("AT+LS_MODBUS_READ_FAIL");
        delay(50);
        return;
    }

    int32_t effective = ((int32_t)regs[0] << 16) | regs[1];
    uint16_t coeff = regs[2];
    if (coeff > 9)
    {
        coeff = 9;
    }
    float value = (float)effective / powf(10.0f, (float)coeff);

    // Serial.printf("AT+LS_MODBUS_EFFECTIVE=%ld\r\n", (long)effective);
    // Serial.printf("AT+LS_MODBUS_COEFF=%u\r\n", (unsigned)coeff);
    Serial.printf("AT+LS_MODBUS_VALUE=%.6f\r\n", value);
    delay(100);
}
