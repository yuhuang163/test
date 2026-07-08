#pragma once

#include <Arduino.h>

class ADCDriver {
public:
    static void init();
    static uint32_t read_raw();
    static int read_mv();
    static float read_pin_voltage_v();
    static float read_voltage_from_raw(uint32_t raw);
    static float read_voltage_design_v(uint32_t raw);
    static float read_current_a();
    static void read_and_print();
};
