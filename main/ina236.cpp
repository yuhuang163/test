#include "ina236.h"
#include "config.h"

#if 0
// INA236 寄存器 (I2C 16bit, MSB first)
#define INA236_REG_CONFIG   0x00  // 配置: 模式/转换时间/ADCRANGE
#define INA236_REG_SHUNT_V  0x01  // 分流电压 IN+-IN-, ADCRANGE=0 时 LSB=2.5µV
#define INA236_REG_BUS_V    0x02  // 总线电压, LSB=1.6mV
#define INA236_REG_CURRENT  0x04  // 电流(由 SHUNT×CAL 算出), 需配 CURRENT_LSB
#define INA236_REG_CALIB    0x05  // 校准: 0x00512/(LSB×Rshunt), 写入非读出电阻
#define INA236_REG_MANUF_ID 0x3E  // 厂商 ID, TI=0x5449
#define INA236_REG_DEVICE_ID 0x3F // 器件 ID, INA236=0x2360
#define INA236_MANUF_ID_EXPECT 0x5449  // TI
#define INA236_DEVICE_ID_EXPECT 0x2360

#define INA236_SHUNT_V_LSB_V 2.5e-6f  // INA236 ADCRANGE=0: ±81.92mV, 2.5µV/LSB
#define INA236_I2C_SDA HS_I2C_SDA
#define INA236_I2C_SCL HS_I2C_SCL

static float s_current_lsb = 0.0001f; // 100µA/bit
static bool s_ina236_ok = false;

static void soft_i2c_init()
{
    pinMode(INA236_I2C_SDA, OUTPUT);
    pinMode(INA236_I2C_SCL, OUTPUT);
    digitalWrite(INA236_I2C_SDA, HIGH);
    digitalWrite(INA236_I2C_SCL, HIGH);
}

static void soft_i2c_start()
{
    digitalWrite(INA236_I2C_SDA, HIGH);
    digitalWrite(INA236_I2C_SCL, HIGH);
    delayMicroseconds(5);
    digitalWrite(INA236_I2C_SDA, LOW);
    delayMicroseconds(5);
    digitalWrite(INA236_I2C_SCL, LOW);
}

static void soft_i2c_stop()
{
    digitalWrite(INA236_I2C_SDA, LOW);
    delayMicroseconds(5);
    digitalWrite(INA236_I2C_SCL, HIGH);
    delayMicroseconds(5);
    digitalWrite(INA236_I2C_SDA, HIGH);
    delayMicroseconds(5);
}

static bool soft_i2c_write_byte(uint8_t data)
{
    for (int i = 0; i < 8; i++) {
        digitalWrite(INA236_I2C_SDA, (data & 0x80) ? HIGH : LOW);
        delayMicroseconds(3);
        digitalWrite(INA236_I2C_SCL, HIGH);
        delayMicroseconds(5);
        digitalWrite(INA236_I2C_SCL, LOW);
        delayMicroseconds(3);
        data <<= 1;
    }

    pinMode(INA236_I2C_SDA, INPUT);
    delayMicroseconds(3);
    digitalWrite(INA236_I2C_SCL, HIGH);
    delayMicroseconds(5);
    bool ack = digitalRead(INA236_I2C_SDA) == LOW;
    digitalWrite(INA236_I2C_SCL, LOW);
    pinMode(INA236_I2C_SDA, OUTPUT);
    return ack;
}

static uint8_t soft_i2c_read_byte(bool ack)
{
    uint8_t val = 0;
    pinMode(INA236_I2C_SDA, INPUT);

    for (int i = 0; i < 8; i++) {
        digitalWrite(INA236_I2C_SCL, HIGH);
        delayMicroseconds(5);
        val <<= 1;
        val |= digitalRead(INA236_I2C_SDA);
        digitalWrite(INA236_I2C_SCL, LOW);
        delayMicroseconds(3);
    }

    pinMode(INA236_I2C_SDA, OUTPUT);
    digitalWrite(INA236_I2C_SDA, ack ? LOW : HIGH);
    delayMicroseconds(3);
    digitalWrite(INA236_I2C_SCL, HIGH);
    delayMicroseconds(5);
    digitalWrite(INA236_I2C_SCL, LOW);
    digitalWrite(INA236_I2C_SDA, HIGH);
    return val;
}

static bool soft_i2c_probe(uint8_t addr)
{
    soft_i2c_init();
    soft_i2c_start();
    bool ack = soft_i2c_write_byte((addr << 1) | 0);
    soft_i2c_stop();
    return ack;
}

static bool soft_i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t *data, size_t len)
{
    soft_i2c_init();
    soft_i2c_start();
    if (!soft_i2c_write_byte((addr << 1) | 0)) {
        soft_i2c_stop();
        return false;
    }
    if (!soft_i2c_write_byte(reg)) {
        soft_i2c_stop();
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        if (!soft_i2c_write_byte(data[i])) {
            soft_i2c_stop();
            return false;
        }
    }
    soft_i2c_stop();
    return true;
}

static bool soft_i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t *data, size_t len)
{
    soft_i2c_init();
    soft_i2c_start();
    if (!soft_i2c_write_byte((addr << 1) | 0)) {
        soft_i2c_stop();
        return false;
    }
    if (!soft_i2c_write_byte(reg)) {
        soft_i2c_stop();
        return false;
    }

    soft_i2c_start();
    if (!soft_i2c_write_byte((addr << 1) | 1)) {
        soft_i2c_stop();
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        data[i] = soft_i2c_read_byte(i < len - 1);
    }
    soft_i2c_stop();
    return true;
}

bool INA236Driver::write_reg16(uint8_t reg, uint16_t value)
{
    uint8_t data[2] = {
        (uint8_t)((value >> 8) & 0xFF),
        (uint8_t)(value & 0xFF),
    };
    return soft_i2c_write_reg(INA236_I2C_ADDR, reg, data, 2);
}

int16_t INA236Driver::read_reg16(uint8_t reg)
{
    uint8_t data[2] = {0};
    if (!soft_i2c_read_reg(INA236_I2C_ADDR, reg, data, 2)) {
        return 0;
    }
    return (int16_t)((data[0] << 8) | data[1]);
}

float INA236Driver::read_shunt_voltage_v()
{
    return read_reg16(INA236_REG_SHUNT_V) * INA236_SHUNT_V_LSB_V;
}

void INA236Driver::apply_shunt_calibration(float r_shunt, float max_current_a)
{
    s_current_lsb = max_current_a / 32768.0f;

    uint16_t shunt_cal = (uint16_t)(0.00512f / (s_current_lsb * r_shunt));
    write_reg16(INA236_REG_CALIB, shunt_cal);
    delay(5);
}

bool INA236Driver::is_high_shunt_range()
{
    return digitalRead(HS_SHUNT_SEL_PIN) == HIGH;
}

void INA236Driver::set_shunt_range(bool high_current)
{
    pinMode(HS_SHUNT_SEL_PIN, OUTPUT);
    digitalWrite(HS_SHUNT_SEL_PIN, high_current ? HIGH : LOW);
    delay(10);

    if (!s_ina236_ok) {
        return;
    }

    if (high_current) {
        apply_shunt_calibration(HS_SHUNT_HIGH_OHM, HS_CURRENT_MAX_A);
    } else {
        apply_shunt_calibration(HS_SHUNT_LOW_OHM + HS_SHUNT_HIGH_OHM, HS_CURRENT_LOW_MAX_A);
    }
}

float INA236Driver::active_shunt_ohm()
{
    return is_high_shunt_range()
        ? HS_SHUNT_HIGH_OHM
        : (HS_SHUNT_LOW_OHM + HS_SHUNT_HIGH_OHM);
}

float INA236Driver::read_current_reg_a()
{
    return read_reg16(INA236_REG_CURRENT) * s_current_lsb;
}

float INA236Driver::read_current_shunt_a()
{
    return read_shunt_voltage_v() / active_shunt_ohm();
}

// 切换判断用 10mΩ 等效电流：IN+/IN- 电压主要反映 10mΩ 压降，而非 10Ω 通路
static float read_switch_current_a()
{
    float i = INA236Driver::read_current_shunt_a();
    if (INA236Driver::is_high_shunt_range()) {
        return i;
    }
    return i * (HS_SHUNT_LOW_OHM + HS_SHUNT_HIGH_OHM) / HS_SHUNT_HIGH_OHM;
}

static void update_shunt_range()
{
    if (!s_ina236_ok) {
        return;
    }

    float i = read_switch_current_a();
    if (INA236Driver::is_high_shunt_range()) {
        if (i < HS_CURRENT_SWITCH_A) {
            INA236Driver::set_shunt_range(false);
        }
    } else if (i > HS_CURRENT_SWITCH_A) {
        INA236Driver::set_shunt_range(true);
    }
}

bool INA236Driver::is_ready()
{
    return s_ina236_ok;
}

void INA236Driver::print_status()
{
    if (!s_ina236_ok) {
        Serial.println("AT+INA236_STAT=FAIL");
        return;
    }

    update_shunt_range();

    int16_t shunt_raw = read_reg16(INA236_REG_SHUNT_V);
    int16_t bus_raw = read_reg16(INA236_REG_BUS_V);
    int16_t cur_raw = read_reg16(INA236_REG_CURRENT);
    uint16_t cfg = (uint16_t)read_reg16(INA236_REG_CONFIG);
    uint16_t cal = (uint16_t)read_reg16(INA236_REG_CALIB);
    float bus_v = bus_raw * 1.6e-3f; // bus LSB = 1.6mV

    // Serial.println("AT+INA236_STAT=OK");
    Serial.printf("AT+INA236_GPIO=%d\r\n", is_high_shunt_range() ? 1 : 0);
    Serial.printf("AT+INA236_MODE=%s,R=%.4f\r\n",
                  is_high_shunt_range() ? "HIGH" : "LOW", active_shunt_ohm());
    Serial.printf("AT+INA236_REG=CFG:0x%04X,CAL:0x%04X,SHUNT:0x%04X,BUS:0x%04X,CUR:0x%04X\r\n",
                  cfg, cal, (uint16_t)shunt_raw, (uint16_t)bus_raw, (uint16_t)cur_raw);
    Serial.printf("AT+INA236_SHUNT_UV=%.1f\r\n", read_shunt_voltage_v() * 1e6f);
    Serial.printf("AT+INA236_BUS_MV=%.1f\r\n", bus_v * 1000.0f);
    Serial.printf("AT+HS_CURRENT_REG=%.9f\r\n", read_current_reg_a());
    Serial.printf("AT+HS_CURRENT_CALC=%.9f\r\n", read_current_shunt_a());
}

void INA236Driver::init()
{
    soft_i2c_init();

    Serial.printf("INA236 探测 I2C 0x%02X (SDA=%d SCL=%d)... ",
                  INA236_I2C_ADDR, INA236_I2C_SDA, INA236_I2C_SCL);
    if (!soft_i2c_probe(INA236_I2C_ADDR)) {
        Serial.println("无 ACK");
        return;
    }
    Serial.println("ACK OK");

    uint16_t manuf_id = (uint16_t)read_reg16(INA236_REG_MANUF_ID);
    uint16_t device_id = (uint16_t)read_reg16(INA236_REG_DEVICE_ID);
    Serial.printf("INA236 ID: MANUF=0x%04X DEV=0x%04X (期望 0x%04X/0x%04X)\r\n",
                  manuf_id, device_id,
                  INA236_MANUF_ID_EXPECT, INA236_DEVICE_ID_EXPECT);
    if (manuf_id != INA236_MANUF_ID_EXPECT || device_id != INA236_DEVICE_ID_EXPECT) {
        Serial.println("INA236 ID 不匹配，通信可能异常");
    }

    // 连续测量 shunt/bus，1.1ms 转换时间
    if (!write_reg16(INA236_REG_CONFIG, 0x4127)) {
        Serial.println("INA236 配置失败");
        return;
    }

    s_ina236_ok = true;
    set_shunt_range(true); // 默认大电流通路 10mΩ
    Serial.printf("INA236 初始化完成 GPIO%d=HIGH(10mΩ)\r\n", HS_SHUNT_SEL_PIN);
    print_status();
}

float INA236Driver::read_current_a()
{
    if (!s_ina236_ok) {
        return 0.0f;
    }

    update_shunt_range();
    return read_current_reg_a();
}
#endif
