// i2c.h
#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <vector>

// ======================== I2C 引脚配置 ========================
// I2C1 - 硬件I2C (Wire)
#define I2C1_SDA 38
#define I2C1_SCL 39
#define I2C1_FREQ 400000

// I2C2 - 硬件I2C (Wire1)
#define I2C2_SDA 5
#define I2C2_SCL 4
#define I2C2_FREQ 400000

// I2C3 - 软件I2C (Bit-Bang)
#define I2C3_SDA 16
#define I2C3_SCL 15
#define I2C3_FREQ 400000

// ======================== 传感器配置 ========================
#define SENSOR_I2C_ADDR 0x6C

// ======================== 数据结构 ========================
struct i2c_data_t {
    uint8_t i2c_addr;
    uint8_t reg_addr;
    uint8_t data[16];
    size_t data_len;
    uint32_t timestamp;
};

// ======================== I2C驱动类 ========================
class I2CDriver {
public:
    // 初始化所有I2C
    static void init_all();
    
    // 扫描指定总线
    static void scan(int bus_id);
    static void scan_all();
    
    // 寄存器读写
    static bool write_reg(int bus_id, uint8_t addr, uint8_t reg, uint8_t data);
    static uint8_t read_reg(int bus_id, uint8_t addr, uint8_t reg);
    static bool read_regs(int bus_id, uint8_t addr, uint8_t reg, uint8_t *data, size_t len);
    
    // 传感器专用函数
    static int32_t read_pressure(int bus_id, uint8_t addr = SENSOR_I2C_ADDR);
    static int16_t read_temperature(int bus_id, uint8_t addr = SENSOR_I2C_ADDR);
    static bool read_all_sensor_data(int bus_id, uint8_t addr, int32_t *pressure, int16_t *temperature);

    // 数据转换
    static float calc_pressure_pa(uint32_t raw, float p_min = -110.0f, float p_max = 110.0f);
    static float calc_temperature_celsius(int16_t raw, float t_min = -40.0f, float t_max = 125.0f);
    
    // 打印信息
    static void print_sensor_data(int bus_id, uint8_t addr = SENSOR_I2C_ADDR);
    
    // 读取并打印三路压力值（AT 格式）
    static void read_and_print_three_pressures();
    
private:
    // 软件I2C (Bit-Bang)
    static void soft_i2c_init();
    static bool soft_i2c_start();
    static void soft_i2c_stop();
    static bool soft_i2c_write_byte(uint8_t data);
    static uint8_t soft_i2c_read_byte(bool ack);
    static bool soft_i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t data);
    static uint8_t soft_i2c_read_reg(uint8_t addr, uint8_t reg);
    static bool soft_i2c_read_regs(uint8_t addr, uint8_t reg, uint8_t *data, size_t len);
};