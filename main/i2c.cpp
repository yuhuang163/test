// i2c.cpp
#include "i2c.h"


int suction_data=0; // 1表示打印传感器数据日志
// ======================== 初始化 ========================

void I2CDriver::init_all() {
    Serial.println("========== 初始化所有I2C总线 ==========");
    
    // 初始化I2C1 (硬件 Wire)
    Wire.begin(I2C1_SDA, I2C1_SCL, I2C1_FREQ);
    Wire.setClock(I2C1_FREQ);
    Serial.printf("✅ I2C1 (硬件) 初始化成功 | SDA:%d SCL:%d 频率:%dHz\n", 
                  I2C1_SDA, I2C1_SCL, I2C1_FREQ);
    
    // 初始化I2C2 (硬件 Wire1)
    Wire1.begin(I2C2_SDA, I2C2_SCL, I2C2_FREQ);
    Wire1.setClock(I2C2_FREQ);
    Serial.printf("✅ I2C2 (硬件) 初始化成功 | SDA:%d SCL:%d 频率:%dHz\n", 
                  I2C2_SDA, I2C2_SCL, I2C2_FREQ);
    
    // 初始化软件I2C3
    soft_i2c_init();
    Serial.printf("✅ I2C3 (软件) 初始化成功 | SDA:%d SCL:%d 频率:%dHz\n", 
                  I2C3_SDA, I2C3_SCL, I2C3_FREQ);
    
    Serial.println("========================================");
}

// ======================== 扫描 ========================

void I2CDriver::scan(int bus_id) {
    Serial.printf("\n🔍 扫描 I2C%d 总线...\n", bus_id);
    
    int found_count = 0;
    
    if (bus_id == 1) {
        // 硬件I2C1 (Wire)
        for (int addr = 1; addr < 127; addr++) {
            Wire.beginTransmission(addr);
            if (Wire.endTransmission() == 0) {
                Serial.printf("  ✅ 找到设备: 0x%02X\n", addr);
                found_count++;
            }
            delay(1);
        }
    } else if (bus_id == 2) {
        // 硬件I2C2 (Wire1)
        for (int addr = 1; addr < 127; addr++) {
            Wire1.beginTransmission(addr);
            if (Wire1.endTransmission() == 0) {
                Serial.printf("  ✅ 找到设备: 0x%02X\n", addr);
                found_count++;
            }
            delay(1);
        }
    } else if (bus_id == 3) {
        // 软件I2C3
        soft_i2c_init();
        for (int addr = 1; addr < 127; addr++) {
            soft_i2c_start();
            bool found = soft_i2c_write_byte((addr << 1) | 0);
            soft_i2c_stop();
            if (found) {
                Serial.printf("  ✅ 找到设备: 0x%02X\n", addr);
                found_count++;
            }
            delay(1);
        }
    } else {
        Serial.printf("  ❌ 无效的总线ID: %d\n", bus_id);
        return;
    }
    
    if (found_count == 0) {
        Serial.printf("  ⚠️ I2C%d 上未找到设备\n", bus_id);
        Serial.println("  请检查:");
        Serial.println("    1. 上拉电阻是否连接 (4.7kΩ)");
        Serial.println("    2. SDA/SCL引脚是否正确");
        Serial.println("    3. 设备供电是否正常");
    } else {
        Serial.printf("  ✅ I2C%d 上找到 %d 个设备\n", bus_id, found_count);
    }
}

void I2CDriver::scan_all() {
    scan(1);
    scan(2);
    scan(3);
}

// ======================== 寄存器读写 ========================

bool I2CDriver::write_reg(int bus_id, uint8_t addr, uint8_t reg, uint8_t data) {
    if (bus_id == 3) {
        return soft_i2c_write_reg(addr, reg, data);
    }
    
    TwoWire *wire = (bus_id == 1) ? &Wire : &Wire1;
    if (wire == nullptr) return false;
    
    wire->beginTransmission(addr);
    wire->write(reg);
    wire->write(data);
    return wire->endTransmission() == 0;
}

uint8_t I2CDriver::read_reg(int bus_id, uint8_t addr, uint8_t reg) {
    if (bus_id == 3) {
        return soft_i2c_read_reg(addr, reg);
    }
    
    TwoWire *wire = (bus_id == 1) ? &Wire : &Wire1;
    if (wire == nullptr) return 0;
    
    wire->beginTransmission(addr);
    wire->write(reg);
    if (wire->endTransmission(false) != 0) return 0;
    
    wire->requestFrom(addr, (uint8_t)1);
    if (wire->available()) {
        return wire->read();
    }
    return 0;
}

bool I2CDriver::read_regs(int bus_id, uint8_t addr, uint8_t reg, uint8_t *data, size_t len) {
    if (data == nullptr || len == 0) return false;
    
    if (bus_id == 3) {
        return soft_i2c_read_regs(addr, reg, data, len);
    }
    
    TwoWire *wire = (bus_id == 1) ? &Wire : &Wire1;
    if (wire == nullptr) return false;
    
    wire->beginTransmission(addr);
    wire->write(reg);
    if (wire->endTransmission(false) != 0) return false;
    
    wire->requestFrom(addr, (uint8_t)len);
    for (size_t i = 0; i < len; i++) {
        if (wire->available()) {
            data[i] = wire->read();
        } else {
            return false;
        }
    }
    return true;
}

// ======================== 传感器专用函数 ========================

int32_t I2CDriver::read_pressure(int bus_id, uint8_t addr)
{
    // 1. 配置为压力转换 (推荐0xE3)
    if (!write_reg(bus_id, addr, 0x5B, 0xE3)) {
        Serial.printf("[I2C%d] ❌ 压力配置失败\n", bus_id);
        return 0;
    }
    
    // 2. 等待转换完成 (7ms)
    delay(7);
    
    // 3. 读取压力数据 (3字节: 0x55高位, 0x56中位, 0x57低位)
    uint8_t data[3];
    if (!read_regs(bus_id, addr, 0x55, data, 3)) {
        Serial.printf("[I2C%d] ❌ 读取压力数据失败\n", bus_id);
        return 0;
    }
    
    uint32_t pressure = ((uint32_t)data[0] << 16) | 
                        ((uint32_t)data[1] << 8) | 
                        data[2];
    
    // Serial.printf("[I2C%d] 原始压力值: 0x%06lX (%lu)\n", bus_id, pressure, pressure);
    return pressure;
}

int16_t I2CDriver::read_temperature(int bus_id, uint8_t addr) {
    // 1. 配置为温度转换 (推荐0xA3)
    if (!write_reg(bus_id, addr, 0x5B, 0xA3)) {
        Serial.printf("[I2C%d] ❌ 温度配置失败\n", bus_id);
        return 0;
    }
    
    // 2. 等待转换完成 (3ms)
    delay(3);
    
    // 3. 读取温度数据 (2字节: 0x58高位, 0x59低位)
    uint8_t data[2];
    if (!read_regs(bus_id, addr, 0x58, data, 2)) {
        Serial.printf("[I2C%d] ❌ 读取温度数据失败\n", bus_id);
        return 0;
    }
    
    // 温度数据: bit14~8在0x58(bit7是符号位), bit7~0在0x59
    int16_t temp_raw = ((data[0] & 0x7F) << 8) | data[1];
    
    // 处理符号位 (0x58的bit7)
    if (data[0] & 0x80) {
        temp_raw = -temp_raw;
    }
    
    // Serial.printf("[I2C%d] 原始温度值: 0x%04X (%d)\n", bus_id, temp_raw, temp_raw);
    return temp_raw;
}

bool I2CDriver::read_all_sensor_data(int bus_id, uint8_t addr, int32_t *pressure, int16_t *temperature) {
    if (pressure == nullptr || temperature == nullptr) return false;
    
    // 读取压力
    *pressure = read_pressure(bus_id, addr);
    if (*pressure == 0) return false;
    
    // 读取温度
    *temperature = read_temperature(bus_id, addr);
    if (*temperature == 0) return false;
    
    return true;
}

// ======================== 数据转换 ========================

float I2CDriver::calc_pressure_pa(uint32_t raw, float p_min, float p_max) {
    // 24位ADC范围: 0x000000 ~ 0xFFFFFF (0 ~ 16777215)
    // 正压范围: 0x800000 ~ 0xFFFFFF 对应 0 ~ 110kPa
    // 负压范围: 0x000000 ~ 0x7FFFFF 对应 -110 ~ 0kPa
    
    if (raw >= 0x800000) {
        // 正压: (raw - 0x800000) / 0x7FFFFF * 110
        return (float)(raw - 0x800000) / 8388607.0f * p_max;
    } else {
        // 负压: raw / 0x7FFFFF * -110
        return (float)raw / 8388607.0f * p_min;  // p_min = -110
    }
}

float I2CDriver::calc_temperature_celsius(int16_t raw, float t_min, float t_max) {
    // 15位范围: 0 ~ 32767 (正温度) 或 -32768 ~ -1 (负温度)
    float range = t_max - t_min;
    if (raw >= 0) {
        return t_min + (raw / 32767.0f) * range;
    } else {
        return t_min + (raw / 32768.0f) * range;
    }
}

// ======================== 打印传感器数据 ========================

void I2CDriver::print_sensor_data(int bus_id, uint8_t addr) {
    int32_t pressure_raw = read_pressure(bus_id, addr);
    int16_t temp_raw = read_temperature(bus_id, addr);
    
    if (pressure_raw == 0 || temp_raw == 0) {
        Serial.printf("[I2C%d] ❌ 读取传感器数据失败\n", bus_id);
        return;
    }
    
    float pressure_pa = calc_pressure_pa(pressure_raw);
    float temp_c = calc_temperature_celsius(temp_raw);
    
    Serial.printf("[I2C%d] 📊 传感器数据 (地址:0x%02X):\n", bus_id, addr);
    Serial.printf("  压力: %.2f Pa (原始: 0x%06lX)\n", pressure_pa, pressure_raw);
    Serial.printf("  温度: %.2f ℃ (原始: 0x%04X)\n", temp_c, temp_raw);
}

// ======================== 软件I2C (Bit-Bang) ========================

void I2CDriver::soft_i2c_init() {
    pinMode(I2C3_SDA, OUTPUT);
    pinMode(I2C3_SCL, OUTPUT);
    digitalWrite(I2C3_SDA, HIGH);
    digitalWrite(I2C3_SCL, HIGH);
}

bool I2CDriver::soft_i2c_start() {
    digitalWrite(I2C3_SDA, HIGH);
    digitalWrite(I2C3_SCL, HIGH);
    delayMicroseconds(5);
    digitalWrite(I2C3_SDA, LOW);
    delayMicroseconds(5);
    digitalWrite(I2C3_SCL, LOW);
    return true;
}

void I2CDriver::soft_i2c_stop() {
    digitalWrite(I2C3_SDA, LOW);
    delayMicroseconds(5);
    digitalWrite(I2C3_SCL, HIGH);
    delayMicroseconds(5);
    digitalWrite(I2C3_SDA, HIGH);
    delayMicroseconds(5);
}

bool I2CDriver::soft_i2c_write_byte(uint8_t data) {
    for (int i = 0; i < 8; i++) {
        digitalWrite(I2C3_SDA, (data & 0x80) ? HIGH : LOW);
        delayMicroseconds(3);
        digitalWrite(I2C3_SCL, HIGH);
        delayMicroseconds(5);
        digitalWrite(I2C3_SCL, LOW);
        delayMicroseconds(3);
        data <<= 1;
    }
    
    // 读取ACK
    pinMode(I2C3_SDA, INPUT);
    delayMicroseconds(3);
    digitalWrite(I2C3_SCL, HIGH);
    delayMicroseconds(5);
    bool ack = digitalRead(I2C3_SDA) == LOW;
    digitalWrite(I2C3_SCL, LOW);
    pinMode(I2C3_SDA, OUTPUT);
    
    return ack;
}

uint8_t I2CDriver::soft_i2c_read_byte(bool ack) {
    uint8_t val = 0;
    pinMode(I2C3_SDA, INPUT);
    
    for (int i = 0; i < 8; i++) {
        digitalWrite(I2C3_SCL, HIGH);
        delayMicroseconds(5);
        val <<= 1;
        val |= digitalRead(I2C3_SDA);
        digitalWrite(I2C3_SCL, LOW);
        delayMicroseconds(3);
    }
    
    // 发送ACK/NACK
    pinMode(I2C3_SDA, OUTPUT);
    digitalWrite(I2C3_SDA, ack ? LOW : HIGH);
    delayMicroseconds(3);
    digitalWrite(I2C3_SCL, HIGH);
    delayMicroseconds(5);
    digitalWrite(I2C3_SCL, LOW);
    digitalWrite(I2C3_SDA, HIGH);
    
    return val;
}

bool I2CDriver::soft_i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t data) {
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
    if (!soft_i2c_write_byte(data)) {
        soft_i2c_stop();
        return false;
    }
    soft_i2c_stop();
    return true;
}

uint8_t I2CDriver::soft_i2c_read_reg(uint8_t addr, uint8_t reg) {
    uint8_t data = 0;
    soft_i2c_init();
    
    // 写寄存器地址
    soft_i2c_start();
    if (!soft_i2c_write_byte((addr << 1) | 0)) {
        soft_i2c_stop();
        return 0;
    }
    if (!soft_i2c_write_byte(reg)) {
        soft_i2c_stop();
        return 0;
    }
    
    // 重新开始读取
    soft_i2c_start();
    if (!soft_i2c_write_byte((addr << 1) | 1)) {
        soft_i2c_stop();
        return 0;
    }
    
    data = soft_i2c_read_byte(false);
    soft_i2c_stop();
    return data;
}

bool I2CDriver::soft_i2c_read_regs(uint8_t addr, uint8_t reg, uint8_t *data, size_t len) {
    if (data == nullptr || len == 0) return false;
    
    soft_i2c_init();
    
    // 写寄存器地址
    soft_i2c_start();
    if (!soft_i2c_write_byte((addr << 1) | 0)) {
        soft_i2c_stop();
        return false;
    }
    if (!soft_i2c_write_byte(reg)) {
        soft_i2c_stop();
        return false;
    }
    
    // 重新开始读取
    soft_i2c_start();
    if (!soft_i2c_write_byte((addr << 1) | 1)) {
        soft_i2c_stop();
        return false;
    }
    
    // 读取数据
    for (size_t i = 0; i < len; i++) {
        data[i] = soft_i2c_read_byte(i < len - 1);
    }
    
    soft_i2c_stop();
    return true;
}

void I2CDriver::read_and_print_three_pressures() {
    if (suction_data)
    {
        int16_t raw_t1 = I2CDriver::read_temperature(1);
        int16_t raw_t2 = I2CDriver::read_temperature(2);
        int16_t raw_t3 = I2CDriver::read_temperature(3);

        float t1 = I2CDriver::calc_temperature_celsius(raw_t1);
        float t2 = I2CDriver::calc_temperature_celsius(raw_t2);
        float t3 = I2CDriver::calc_temperature_celsius(raw_t3);

        uint32_t raw_p1 = I2CDriver::read_pressure(1);
        uint32_t raw_p2 = I2CDriver::read_pressure(2);
        uint32_t raw_p3 = I2CDriver::read_pressure(3);

        float p1 = I2CDriver::calc_pressure_pa(raw_p1);
        float p2 = I2CDriver::calc_pressure_pa(raw_p2);
        float p3 = I2CDriver::calc_pressure_pa(raw_p3);

        Serial.printf("AT+SUCTION_DATA=%.2f,%.2f,%.2f\r\n", p1, p2, p3);
        Serial.printf("AT+TEMP_DATA=%.2f,%.2f,%.2f\r\n", t1, t2, t3);

        delay(20);
    }
    
    
}