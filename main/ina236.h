#pragma once

#include <Arduino.h>

#if 0
// 高侧 INA236 (GPIO47/1 I2C)
#define HS_I2C_SDA 47
#define HS_I2C_SCL 1
#define INA236_I2C_ADDR 0x40       // A0 = GND
#define HS_SHUNT_HIGH_OHM 0.01f    // 大电流通路 10mΩ
#define HS_SHUNT_LOW_OHM 10.0f     // 小电流通路 10Ω
#define HS_SHUNT_SEL_PIN 40        // 高=大电流10mΩ, 低=小电流10Ω+10mΩ
#define HS_CURRENT_SWITCH_A 0.008f // >8mA 切换大电流通路
#define HS_CURRENT_MAX_A 5.0f      // 大电流通路采集上限 5A
#define HS_CURRENT_LOW_MAX_A 0.008f // 小电流通路(10Ω)约 8mA 上限

// 高侧电流采集 INA236 (I2C: GPIO47=SDA, GPIO1=SCL, A0=GND -> 0x40)
class INA236Driver {
public:
    static void init();
    static bool is_ready();
    static void set_shunt_range(bool high_current);
    static bool is_high_shunt_range();
    static void print_status();
    static float read_current_a();
    static float read_current_reg_a();
    static float read_current_shunt_a();

private:
    static float active_shunt_ohm();
    static void apply_shunt_calibration(float r_shunt, float max_current_a);
    static bool write_reg16(uint8_t reg, uint16_t value);
    static int16_t read_reg16(uint8_t reg);
    static float read_shunt_voltage_v();
};
#endif
